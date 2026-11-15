// The fill-in map: see WRMapReveal.h.

#include "WRMapReveal.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Base64.h"
#include "Misc/Compression.h"

namespace WRRevealDetail
{
	constexpr int32 TileCells = 64;
	constexpr int32 TileBytes = TileCells * TileCells / 8;   // 512
}

UWRMapRevealComponent::UWRMapRevealComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
	// Each player's grid (the class default needs none: it would only hold a copy of an empty map).
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) Size();
}

UWRMapRevealComponent* UWRMapRevealComponent::For(const AActor* PlayerOrState)
{
	if (!PlayerOrState) return nullptr;
	const APlayerState* PS = Cast<APlayerState>(PlayerOrState);
	if (!PS)
	{
		if (const APawn* P = Cast<APawn>(PlayerOrState)) PS = P->GetPlayerState();
		else if (const AController* C = Cast<AController>(PlayerOrState)) PS = C->PlayerState;
	}
	return PS ? PS->FindComponentByClass<UWRMapRevealComponent>() : nullptr;
}

void UWRMapRevealComponent::Size()
{
	// The grid covers the map picture (ui.mapMin / ui.mapSize: it grows with the continent) in cellCm cells.
	CellCm = FMath::Max(100.f, WRTune(TEXT("ui.reveal.cellCm"), 800.f));
	MinCm = WRTune(TEXT("ui.mapMin"), -201800.f);
	SpanCm = FMath::Max(1000.f, WRTune(TEXT("ui.mapSize"), 403600.f));
	N = FMath::Max(1, FMath::CeilToInt(SpanCm / CellCm));
	T = FMath::DivideAndRoundUp(N, WRRevealDetail::TileCells);
	Bits.SetNumZeroed(T * T * WRRevealDetail::TileBytes);
}

void UWRMapRevealComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UWRMapRevealComponent::Cell(int32 X, int32 Y) const
{
	if (X < 0 || Y < 0 || X >= N || Y >= N) return true;
	const int32 Bit = (Y % WRRevealDetail::TileCells) * WRRevealDetail::TileCells + (X % WRRevealDetail::TileCells);
	const int32 Byte = TileOf(X, Y) * WRRevealDetail::TileBytes + Bit / 8;
	return Bits.IsValidIndex(Byte) && (Bits[Byte] & (1 << (Bit % 8))) != 0;
}

bool UWRMapRevealComponent::IsRevealed(const FVector2D& World) const
{
	return Cell(FMath::FloorToInt((World.X - MinCm) / CellCm), FMath::FloorToInt((World.Y - MinCm) / CellCm));
}

void UWRMapRevealComponent::RevealCircle(const FVector2D& Centre, float RadiusCm)
{
	const float CX = (Centre.X - MinCm) / CellCm, CY = (Centre.Y - MinCm) / CellCm, R = RadiusCm / CellCm;
	const int32 X0 = FMath::Max(0, FMath::FloorToInt(CX - R)), X1 = FMath::Min(N - 1, FMath::CeilToInt(CX + R));
	const int32 Y0 = FMath::Max(0, FMath::FloorToInt(CY - R)), Y1 = FMath::Min(N - 1, FMath::CeilToInt(CY + R));
	bool bAny = false;
	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			if (FMath::Square(X + 0.5f - CX) + FMath::Square(Y + 0.5f - CY) > R * R) continue;
			const int32 Bit = (Y % WRRevealDetail::TileCells) * WRRevealDetail::TileCells + (X % WRRevealDetail::TileCells);
			const int32 Tile = TileOf(X, Y);
			uint8& B = Bits[Tile * WRRevealDetail::TileBytes + Bit / 8];
			const uint8 Mask = uint8(1 << (Bit % 8));
			if (B & Mask) continue;
			B |= Mask;
			Dirty.Add(Tile);
			bAny = true;
		}
	}
	if (bAny) ++Version;
}

void UWRMapRevealComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (GetOwnerRole() != ROLE_Authority)
	{
		// A client's own copy starts empty: once it's ours (its controller is this machine's), ask the server for every
		// tile it has (a returning player's map). Other players' copies on this client stay empty and quiet.
		const APlayerController* Mine = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
		if (!bAsked && Mine && Mine->IsLocalController())
		{
			bAsked = true;
			ServerSendAll();
		}
		if (bAsked) SetComponentTickEnabled(false);   // nothing more to do on this machine (tiles arrive by RPC)
		return;
	}
	Accum += DeltaTime;
	if (Accum >= WRTune(TEXT("ui.reveal.tickSeconds"), 1.f))
	{
		Accum = 0.f;
		// Where the body is (walking, riding, flying, swimming): everything around it in reach is seen.
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (const AController* C = PS ? Cast<AController>(PS->GetOwner()) : nullptr) Pawn = C->GetPawn();
		if (Pawn) RevealCircle(FVector2D(Pawn->GetActorLocation()), WRTune(TEXT("ui.reveal.radiusCm"), 15000.f));
		// A shrine found inks its surroundings once (its sub-zone, as a map fragment does).
		if (const UWRQuestLog* Log = UWRQuestLog::For(PS))
		{
			for (const FName& Shrine : Log->FoundShrines)
			{
				if (Stamped.Contains(Shrine)) continue;
				Stamped.Add(Shrine);
				for (TActorIterator<AWRShrine> It(GetWorld()); It; ++It)
				{
					if (It->ShrineId == Shrine) RevealCircle(FVector2D(It->GetActorLocation()), WRTune(TEXT("ui.reveal.shrineCm"), 60000.f));
				}
			}
		}
	}
	// The owner's client gets the changed tiles, a few a frame (its own listen-server or solo copy is this one).
	const APlayerController* Owner = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
	const bool bRemote = Owner && !Owner->IsLocalController();
	if (!bRemote)
	{
		Dirty.Reset();
		SendQueue.Reset();
		return;
	}
	for (const int32 D : Dirty) SendQueue.AddUnique(D);
	Dirty.Reset();
	const int32 PerTick = FMath::Max(1, static_cast<int32>(WRTune(TEXT("ui.reveal.tilesPerTick"), 4.f)));
	for (int32 k = 0; k < PerTick && SendQueue.Num() > 0; ++k)
	{
		const int32 Tile = SendQueue[0];
		SendQueue.RemoveAt(0);
		TArray<uint8> Bytes;
		Bytes.Append(&Bits[Tile * WRRevealDetail::TileBytes], WRRevealDetail::TileBytes);
		ClientTile(Tile, Bytes);
	}
}

void UWRMapRevealComponent::ClientTile_Implementation(int32 Tile, const TArray<uint8>& Bytes)
{
	if (Tile < 0 || Tile >= T * T || Bytes.Num() != WRRevealDetail::TileBytes) return;
	FMemory::Memcpy(&Bits[Tile * WRRevealDetail::TileBytes], Bytes.GetData(), WRRevealDetail::TileBytes);
	++Version;
}

void UWRMapRevealComponent::ServerSendAll_Implementation()
{
	for (int32 Tile = 0; Tile < T * T; ++Tile)
	{
		const uint8* P = &Bits[Tile * WRRevealDetail::TileBytes];
		for (int32 i = 0; i < WRRevealDetail::TileBytes; ++i)
		{
			if (P[i]) { SendQueue.AddUnique(Tile); break; }
		}
	}
}

TSharedPtr<FJsonObject> UWRMapRevealComponent::ToJson() const
{
	// Only the tiles with anything seen, joined and zlib-packed (a few KB for hours of exploring).
	TArray<TSharedPtr<FJsonValue>> Tiles;
	TArray<uint8> Raw;
	for (int32 Tile = 0; Tile < T * T; ++Tile)
	{
		const uint8* P = &Bits[Tile * WRRevealDetail::TileBytes];
		bool bAny = false;
		for (int32 i = 0; i < WRRevealDetail::TileBytes && !bAny; ++i) bAny = P[i] != 0;
		if (!bAny) continue;
		Tiles.Add(MakeShared<FJsonValueNumber>(Tile));
		Raw.Append(P, WRRevealDetail::TileBytes);
	}
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("cellCm"), CellCm);
	J->SetNumberField(TEXT("minCm"), MinCm);
	J->SetNumberField(TEXT("cells"), N);
	J->SetArrayField(TEXT("tiles"), Tiles);
	J->SetNumberField(TEXT("raw"), Raw.Num());
	if (Raw.Num() > 0)
	{
		int32 Packed = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
		TArray<uint8> Out;
		Out.SetNumUninitialized(Packed);
		if (FCompression::CompressMemory(NAME_Zlib, Out.GetData(), Packed, Raw.GetData(), Raw.Num()))
		{
			Out.SetNum(Packed);
			J->SetStringField(TEXT("data"), FBase64::Encode(Out));
		}
	}
	return J;
}

void UWRMapRevealComponent::FromJson(const TSharedPtr<FJsonObject>& J)
{
	if (!J.IsValid()) return;
	// A grid saved for another map size or cell (the continent grew): it no longer lines up, so it starts again.
	if (!FMath::IsNearlyEqual(J->GetNumberField(TEXT("cellCm")), CellCm) || !FMath::IsNearlyEqual(J->GetNumberField(TEXT("minCm")), MinCm) || static_cast<int32>(J->GetNumberField(TEXT("cells"))) != N)
	{
		UE_LOG(LogWildreach, Log, TEXT("REVEAL saved map grid is for another map size: starting a fresh map"));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Tiles = nullptr;
	FString Data;
	if (!J->TryGetArrayField(TEXT("tiles"), Tiles) || !J->TryGetStringField(TEXT("data"), Data)) return;
	const int32 RawSize = static_cast<int32>(J->GetNumberField(TEXT("raw")));
	TArray<uint8> Packed, Raw;
	if (RawSize != Tiles->Num() * WRRevealDetail::TileBytes || !FBase64::Decode(Data, Packed)) return;
	Raw.SetNumUninitialized(RawSize);
	if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawSize, Packed.GetData(), Packed.Num())) return;
	for (int32 i = 0; i < Tiles->Num(); ++i)
	{
		const int32 Tile = static_cast<int32>((*Tiles)[i]->AsNumber());
		if (Tile < 0 || Tile >= T * T) continue;
		FMemory::Memcpy(&Bits[Tile * WRRevealDetail::TileBytes], &Raw[i * WRRevealDetail::TileBytes], WRRevealDetail::TileBytes);
	}
	++Version;
}
