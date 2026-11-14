// The war-halls (cooks/build-endgame.md 2.10; biome-void 2.23.2; cooks/endgame.md 2.9.2): the gate at the pass, the
// hall in hall space, its flags of basalt, lava and ice, the iron grate, the champions and their rivalry, the win, the
// rest. Server authority: the hall's state, its flags and the grate replicate; every blow, burn and burst goes through
// ApplyHit. Clients draw the gate, the hall and the flags (lava and ice are live sheets over the basalt, rebuilt as the
// flags change) and move the grate from the server's clock, so it slides, never snaps.
#include "WRWarHall.h"
#include "WRDeeds.h"
#include "WRDeedsInternal.h"
#include "Wildreach.h"
#include "WRArtifacts.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRRaiding.h"
#include "WRRecords.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRStore.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRPortalsInternal.h"
#include "WRBuilding.h"
#include "WRAttributeSet.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/DateTime.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace WRDeedsImpl;
using WRLookKit::FSurf;
using WRLookKit::FKit;

namespace WRHallImpl
{
	TSharedPtr<FJsonObject>& Data() { static TSharedPtr<FJsonObject> D; return D; }
	TSharedPtr<FJsonObject> Raids()
	{
		TSharedPtr<FJsonObject>& D = Data();
		if (!D.IsValid()) D = FWRTuning::LoadDataFile(TEXT("raids.json"));
		if (!D.IsValid()) D = MakeShared<FJsonObject>();
		return D;
	}
	float Frame(const TCHAR* Key, float Fallback) { return DNum(DChild(Raids(), TEXT("frame")), Key, Fallback); }
	TSharedPtr<FJsonObject> TilesRow(FName Hall) { return DChild(WRWarHall::Row(Hall), TEXT("tiles")); }
	float TileNum(FName Hall, const TCHAR* Key, float Fallback) { return DNum(TilesRow(Hall), Key, Fallback); }
	double Unix() { return double(FDateTime::UtcNow().ToUnixTimestamp()); }
	TSharedPtr<FJsonObject>& SavedDoc() { static TSharedPtr<FJsonObject> D; return D; }
	void SaveDoc(UWorld* World)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		for (TActorIterator<AWRWarHallGate> It(World); It; ++It)
		{
			TSharedRef<FJsonObject> H = MakeShared<FJsonObject>();
			H->SetNumberField(TEXT("restUntil"), It->State == EWRHallState::Resting || It->State == EWRHallState::Won ? It->RestUntil : 0.0);
			O->SetObjectField(It->HallId.ToString(), H);
		}
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(O, W);
		WRStore::WriteDoc(TEXT("warhalls"), Out);
	}
	AWRWarHall* HallOf(const AWRBoss* Boss)
	{
		const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
		return B ? B->Hall.Get() : nullptr;
	}
	AWRWarHallGate* GateNamed(UWorld* World, FName Id)
	{
		for (TActorIterator<AWRWarHallGate> It(World); It; ++It) if (It->HallId == Id) return *It;
		return nullptr;
	}
	AWRWarHall* HallNamed(UWorld* World, FName Id)
	{
		for (TActorIterator<AWRWarHall> It(World); It; ++It) if (It->HallId == Id) return *It;
		return nullptr;
	}
	/** Feet on the floor of a hall: the floor's height is the hall's own. */
	bool OnFloor(const AWRWarHall* H, const AWRCharacterBase* C)
	{
		const float FeetZ = C->GetActorLocation().Z - C->GetSimpleCollisionHalfHeight();
		return FMath::Abs(FeetZ - H->GetActorLocation().Z) < 60.f && !C->GetCharacterMovement()->IsFalling();
	}
	void Put(APawn* Pawn, const FVector& Floor, float Yaw)
	{
		if (!Pawn) return;
		const FRotator Face(0.f, Yaw, 0.f);
		// Onto the real ground there (a slope, the hall's flags), then a checked teleport so nobody lands inside rock.
		FVector At = Floor;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHallPut), false, Pawn);
		if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Floor + FVector(0, 0, 1500.f), Floor - FVector(0, 0, 3000.f), ECC_WorldStatic, Q)) At = Hit.ImpactPoint;
		const FVector Dest = At + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() + 30.f);
		if (!Pawn->TeleportTo(Dest, Face, false, false)) Pawn->TeleportTo(Dest + FVector(0, 0, 150.f), Face, false, true);
		if (AController* C = Pawn->GetController()) C->SetControlRotation(Face);
	}
	void TellHall(AWRWarHall* H, const FString& S)
	{
		if (!H) return;
		for (TActorIterator<AWRCharacter> It(H->GetWorld()); It; ++It)
		{
			if (!H->Contains(It->GetActorLocation(), 500.f)) continue;
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(S);
		}
	}
}
using namespace WRHallImpl;

// ================================================================================================ data

TSharedPtr<FJsonObject> WRWarHall::Row(FName Raid)
{
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(Raids(), TEXT("halls")))
		for (const TSharedPtr<FJsonValue>& V : *L) if (DStr(V->AsObject(), TEXT("id")) == Raid.ToString()) return V->AsObject();
	return nullptr;
}

bool WRWarHall::IsLive(FName Raid)
{
	const TSharedPtr<FJsonObject> R = Row(Raid);
	if (!DBool(R, TEXT("live"))) return false;
	// Its champions must be in this build too.
	if (const TArray<TSharedPtr<FJsonValue>>* C = DArr(R, TEXT("champions")))
		for (const TSharedPtr<FJsonValue>& V : *C) if (!FWRCreatureRegistry::Get().Find(FName(*V->AsString()))) return false;
	return true;
}

int32 WRWarHall::LiveCount()
{
	int32 N = 0;
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(Raids(), TEXT("halls")))
		for (const TSharedPtr<FJsonValue>& V : *L) N += IsLive(FName(*DStr(V->AsObject(), TEXT("id"))));
	return N;
}

FString WRWarHall::Name(FName Raid) { return DStr(Row(Raid), TEXT("name")); }

AWRWarHall* WRWarHall::HallAt(const UObject* WorldContext, const FVector& Point)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || Point.Z < DNum(DChild(Raids(), TEXT("hallSpace")), TEXT("z"), 200000.f) - 30000.f) return nullptr;
	for (TActorIterator<AWRWarHall> It(World); It; ++It) if (It->Contains(Point, 1500.f)) return *It;
	return nullptr;
}

FName WRWarHall::BiomeAt(const UObject* WorldContext, const FVector& Point)
{
	const AWRWarHall* H = HallAt(WorldContext, Point);
	if (!H) return NAME_None;
	const TArray<TSharedPtr<FJsonValue>>* Lands = DArr(Row(H->HallId), TEXT("lands"));
	if (!Lands || Lands->Num() < 2) return NAME_None;
	return FName(*(*Lands)[Point.X < H->GetActorLocation().X ? 0 : 1]->AsString());
}

bool WRWarHall::IceAt(const UObject* WorldContext, const FVector& Feet)
{
	const AWRWarHall* H = HallAt(WorldContext, Feet);
	if (!H) return false;
	const int32 i = H->TileAt(Feet);
	return i != INDEX_NONE && H->Tiles.IsValidIndex(i) && H->Tiles[i] == 2 && FMath::Abs(Feet.Z - H->GetActorLocation().Z) < 60.f;
}

// ================================================================================================ start

void WRWarHall::Start(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	FString Text;
	SavedDoc() = MakeShared<FJsonObject>();
	if (WRStore::ReadDoc(TEXT("warhalls"), Text))
	{
		TSharedPtr<FJsonObject> P;
		const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
		if (FJsonSerializer::Deserialize(R, P) && P.IsValid()) SavedDoc() = P;
	}
	const TSharedPtr<FJsonObject> Space = DChild(Raids(), TEXT("hallSpace"));
	int32 Index = 0;
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(Raids(), TEXT("halls")))
	{
		for (const TSharedPtr<FJsonValue>& V : *L)
		{
			const TSharedPtr<FJsonObject> H = V->AsObject();
			const FName Id(*DStr(H, TEXT("id")));
			if (!IsLive(Id)) continue;
			// Its pass on this island (none: not this world's).
			const FWRPoi* Pass = FWRPoi::All.FindByPredicate([&H](const FWRPoi& P) { return P.Kind == TEXT("pass") && P.Name == DStr(H, TEXT("pass")); });
			if (!Pass) { UE_LOG(LogWildreach, Warning, TEXT("WARHALL %s: no %s on this island"), *Id.ToString(), *DStr(H, TEXT("pass"))); continue; }
			const FVector Origin(DNum(Space, TEXT("x"), -1500000.f), DNum(Space, TEXT("y"), 0.f) + DNum(Space, TEXT("spacing"), 60000.f) * Index, DNum(Space, TEXT("z"), 200000.f));
			++Index;
			AWRWarHall* Hall = World->SpawnActorDeferred<AWRWarHall>(AWRWarHall::StaticClass(), FTransform(Origin), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Hall->HallId = Id;
			Hall->FinishSpawning(FTransform(Origin));
			// The gate: off the pass road toward the second land, facing the road, settled on the ground.
			const TSharedPtr<FJsonObject> G = DChild(H, TEXT("gate"));
			// The pass road crosses the ridge along X; the gate is cut into the rock beside it (+Y), its door looking back at
			// the road (-Y). Its footprint is traced at the centre and four corners and set on the lowest: the facade's
			// foot is buried in the slope (the heaped ridge rock hides it), never floating over it.
			FVector At = Pass->Location + FVector(0.f, DNum(G, TEXT("side"), 1800.f), 0.f);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHallGate), false);
			const FRotator Face(0.f, -90.f, 0.f);
			{
				float Low = TNumericLimits<float>::Max();
				const FVector Fw = Face.Vector(), Rt = FRotationMatrix(Face).GetUnitAxis(EAxis::Y);
				const FVector Corners[] = { FVector::ZeroVector, Fw * 450.f + Rt * 450.f, Fw * 450.f - Rt * 450.f, -Fw * 450.f + Rt * 450.f, -Fw * 450.f - Rt * 450.f };
				for (const FVector& O : Corners)
				{
					if (World->LineTraceSingleByChannel(Hit, At + O + FVector(0, 0, 5000.f), At + O - FVector(0, 0, 8000.f), ECC_WorldStatic, Q)) Low = FMath::Min(Low, Hit.ImpactPoint.Z);
				}
				if (Low < TNumericLimits<float>::Max()) At.Z = Low;
			}
			AWRWarHallGate* Gate = World->SpawnActorDeferred<AWRWarHallGate>(AWRWarHallGate::StaticClass(), FTransform(Face, At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Gate->HallId = Id;
			Gate->FinishSpawning(FTransform(Face, At));
			Hall->Gate = Gate;
			// The run-back stone 40 m back along the road (west, where the road comes up from the fire side); the record
			// tablet beside the gate's steps.
			FVector StoneAt = Gate->FrontPoint() + FVector(-DNum(G, TEXT("back"), 4000.f), 0.f, 0.f);
			if (World->LineTraceSingleByChannel(Hit, StoneAt + FVector(0, 0, 5000.f), StoneAt - FVector(0, 0, 8000.f), ECC_WorldStatic, Q)) StoneAt = Hit.ImpactPoint;
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (AWRRunbackStone* Stone = World->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), StoneAt, FRotator(0.f, 0.f, 0.f), SP))
			{
				Stone->BossId = Id;
				Hall->Runback = Stone;
			}
			FVector TabAt = At + Face.Vector() * 900.f + FRotationMatrix(Face).GetUnitAxis(EAxis::Y) * 700.f;
			if (World->LineTraceSingleByChannel(Hit, TabAt + FVector(0, 0, 5000.f), TabAt - FVector(0, 0, 8000.f), ECC_WorldStatic, Q)) TabAt = Hit.ImpactPoint;
			WRRecords::PlaceTablet(World, Id, TabAt, Face);
			// Resting from a win before the restart, or ready.
			const double Rest = DNum(DChild(SavedDoc(), Id.ToString()), TEXT("restUntil"), 0.f);
			if (Rest > Unix()) { Gate->RestUntil = Rest; Hall->SetState(EWRHallState::Resting); }
			else { Hall->SpawnChampions(); Hall->SetState(EWRHallState::Ready); }
			UE_LOG(LogWildreach, Log, TEXT("WARHALL %s: gate at %s, hall at %s (%s)"), *Id.ToString(), *At.ToCompactString(), *Origin.ToCompactString(), Rest > Unix() ? TEXT("resting") : TEXT("ready"));
		}
	}
}

// ================================================================================================ the gate

AWRWarHallGate::AWRWarHallGate()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(60000.f));
}

void AWRWarHallGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWarHallGate, HallId);
	DOREPLIFETIME(AWRWarHallGate, State);
	DOREPLIFETIME(AWRWarHallGate, StateSince);
	DOREPLIFETIME(AWRWarHallGate, RestUntil);
	DOREPLIFETIME(AWRWarHallGate, Group);
	DOREPLIFETIME(AWRWarHallGate, Leader);
	DOREPLIFETIME(AWRWarHallGate, Inside);
}

void AWRWarHallGate::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AWRWarHallGate::OnRep_Look() { BuildLook(); }
void AWRWarHallGate::OnRep_State() {}

FVector AWRWarHallGate::FrontPoint() const
{
	// On the ground in front of the steps.
	return GetActorLocation() + GetActorForwardVector() * 420.f;
}

void AWRWarHallGate::BuildLook()
{
	if (bBuilt || HallId.IsNone()) return;
	bBuilt = true;
	// Collision, every machine: the facade and its cheek walls, the steps (the door itself is a way in through the
	// screen, not open air: it stays shut until someone goes through).
	const auto Block = [this](const FVector& Min, const FVector& Max)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetupAttachment(RootComponent);
		B->SetBoxExtent((Max - Min) * 0.5f);
		B->SetRelativeLocation((Min + Max) * 0.5f);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
	};
	Block(FVector(-400.f, -420.f, -200.f), FVector(-60.f, 420.f, 620.f));       // the facade and the rock behind
	Block(FVector(-60.f, -420.f, -200.f), FVector(240.f, -300.f, 260.f));       // left cheek wall
	Block(FVector(-60.f, 300.f, -200.f), FVector(240.f, 420.f, 260.f));         // right cheek wall
	for (int32 s = 0; s < 3; ++s) Block(FVector(240.f - 80.f * (s + 1), -300.f, -60.f), FVector(240.f - 80.f * s, 300.f, 20.f * (s + 1)));
	if (GetNetMode() == NM_DedicatedServer) return;

	// The undercroft door the forts will stand over: a basalt facade of coursed ashlar (west) meeting glacier ice
	// blocks (east), set in heaped ridge rock; three steps up between cheek walls; a round-headed doorway with a heavy
	// oak double door on iron straps; the lintel carved with the two champions' sigils, the Smith's left and the
	// Wyrm's right; a fire brazier on the left cheek, a cold blue lamp on the right.
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Basalt = FSurf::Stone(FLinearColor(0.035f, 0.032f, 0.03f), 0.9f);
	const FSurf BasaltAlt = FSurf::Stone(FLinearColor(0.055f, 0.05f, 0.047f), 0.92f);
	const FSurf Ice = FSurf::Glass(FLinearColor(0.42f, 0.62f, 0.82f), 0.05f);
	const FSurf IceDeep = FSurf::Glass(FLinearColor(0.12f, 0.24f, 0.42f), 0.03f);
	const FSurf Oak = FSurf::Wood(FLinearColor(0.11f, 0.07f, 0.045f));
	const FSurf Iron = FSurf::Steel(FLinearColor(0.035f, 0.033f, 0.03f), 0.55f);
	const FSurf Ember = FSurf::Lit(FLinearColor(1.f, 0.4f, 0.08f), 1.2f);
	const FSurf Cold = FSurf::Lit(FLinearColor(0.45f, 0.75f, 1.f), 1.f);
	// Facade courses: 60 cm ashlar, staggered; the left half basalt, the right half ice.
	for (int32 Row = 0; Row < 10; ++Row)
	{
		const float Z = -40.f + Row * 60.f;
		const float Off = (Row % 2) * 45.f;
		for (float Y = -420.f - Off; Y < 420.f; Y += 90.f)
		{
			const float Y0 = FMath::Max(Y, -420.f), Y1 = FMath::Min(Y + 90.f, 420.f);
			if (Y1 - Y0 < 10.f) continue;
			// The doorway: 280 wide, 400 tall to the spring of its round head.
			const float Mid = (Y0 + Y1) * 0.5f;
			if (FMath::Abs(Mid) < 150.f && Z < 470.f) continue;
			const FVector C(-80.f, Mid, Z + 30.f);
			const bool bIce = Mid > 0.f;
			const FSurf S = bIce ? (Row % 3 == 0 ? IceDeep : Ice) : ((Row + int32(Y)) % 3 == 0 ? BasaltAlt : Basalt);
			K.Add(S.Dim(0.9f + 0.18f * K.Rand(C, 3)), FVector(36.f + 6.f * K.Rand(C, 5), Y1 - Y0 - 3.f, 57.f), C, FRotator(0.f, 0.6f * K.Jit(C, 7), 0.f));
		}
	}
	// The round head over the door: voussoirs in a half ring.
	for (int32 v = 0; v < 9; ++v)
	{
		const float A = PI * (v + 0.5f) / 9.f;
		const FVector C(-62.f, FMath::Cos(A) * 158.f, 400.f + FMath::Sin(A) * 158.f);
		K.Add(v < 4 ? Basalt : (v > 4 ? Ice : BasaltAlt), FVector(44.f, 34.f, 56.f), C, FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) - 90.f));
	}
	// The lintel band with the two sigils cut in it.
	K.Box(BasaltAlt.Dim(1.1f), FVector(-58.f, -260.f, 572.f), FVector(-50.f, 260.f, 610.f));
	const TArray<FString> Smith = WRDeeds::Sigil(TEXT("vulkarn"));
	const TArray<FString> Wyrm = WRDeeds::Sigil(TEXT("skaldrith"));
	const auto Cut = [&K](const TArray<FString>& Rows, float CY, const FSurf& S)
	{
		for (int32 r = 0; r < Rows.Num(); ++r)
			for (int32 c = 0; c < Rows[r].Len(); ++c)
				if (Rows[r][c] == TCHAR('#')) K.Add(S, FVector(1.5f, 3.6f, 3.6f), FVector(-49.f, CY + (c - 4) * 4.f, 591.f - (r - 4) * 4.f));
	};
	Cut(Smith, -180.f, Ember.Dim(0.5f));
	Cut(Wyrm, 180.f, Cold.Dim(0.5f));
	// Cheek walls and the three steps.
	for (const float S : { -1.f, 1.f })
	{
		const FSurf W = S < 0.f ? Basalt : Ice;
		K.Box(W, FVector(-60.f, S < 0.f ? -420.f : 300.f, -60.f), FVector(240.f, S < 0.f ? -300.f : 420.f, 240.f));
		K.Box(W.Dim(1.15f), FVector(-60.f, S < 0.f ? -428.f : 292.f, 240.f), FVector(250.f, S < 0.f ? -292.f : 428.f, 262.f));   // coping
	}
	for (int32 s = 0; s < 3; ++s)
	{
		K.Box(BasaltAlt.Dim(0.85f + 0.08f * s), FVector(240.f - 80.f * (s + 1), -298.f, -60.f), FVector(240.f - 80.f * s, 298.f, 20.f * (s + 1)));
	}
	// The doors: oak planks on iron straps, ring pulls.
	for (const float S : { -1.f, 1.f })
	{
		// Each leaf hangs on its hinge at the jamb (the leaf's origin) and runs inward 140 cm to the meeting edge.
		USceneComponent* Leaf = NewObject<USceneComponent>(this);
		Leaf->SetupAttachment(RootComponent);
		Leaf->SetRelativeLocation(FVector(-70.f, S * 140.f, 60.f));
		Leaf->RegisterComponent();
		Leaves.Add(Leaf);
		const float In = -S;
		TArray<FWRLookBlock> LB;
		FKit LK(GetActorTransform(), LB);
		for (int32 p = 0; p < 5; ++p)
		{
			const float A = In * 28.f * p, B = In * (28.f * p + 27.f);
			LK.Box(Oak.Dim(0.9f + 0.1f * (p % 2)), FVector(-6.f, FMath::Min(A, B), 0.f), FVector(6.f, FMath::Max(A, B), 400.f));
		}
		for (const float Z : { 60.f, 200.f, 340.f }) LK.Box(Iron, FVector(6.f, FMath::Min(0.f, In * 136.f), Z), FVector(9.f, FMath::Max(0.f, In * 136.f), Z + 14.f));
		for (const float Z : { 60.f, 340.f }) LK.Add(Iron, FVector(12.f, 12.f, 20.f), FVector(0.f, 0.f, Z + 7.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // hinge knuckles
		LK.Add(Iron, FVector(16.f, 16.f, 3.f), FVector(10.f, In * 115.f, 200.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		FWRBuildLook::Spawn(this, Leaf, LB, FLinearColor(0.3f, 0.25f, 0.2f), false);
	}
	// The brazier (fire, left) and the cold lamp (right) on the cheek walls.
	K.Add(Iron, FVector(56.f, 56.f, 20.f), FVector(150.f, -360.f, 272.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Ember, FVector(44.f, 44.f, 6.f), FVector(150.f, -360.f, 284.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Iron, FVector(30.f, 30.f, 46.f), FVector(150.f, 360.f, 285.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Cold, FVector(22.f, 22.f, 30.f), FVector(150.f, 360.f, 292.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	// Ridge rock heaped round and behind it (the hill it's cut into).
	UMaterialInstanceDynamic* Rock = WRFoldLook::RockMat(this, FLinearColor(0.55f, 0.55f, 0.57f));
	const FVector RockAt[] = { FVector(-420.f, -520.f, 0.f), FVector(-420.f, 520.f, 0.f), FVector(-600.f, 0.f, 0.f), FVector(-250.f, -640.f, 0.f), FVector(-250.f, 640.f, 0.f), FVector(-520.f, -260.f, 300.f), FVector(-520.f, 260.f, 300.f) };
	const FVector RockSize[] = { FVector(700.f, 520.f, 760.f), FVector(700.f, 520.f, 720.f), FVector(800.f, 1100.f, 900.f), FVector(420.f, 360.f, 380.f), FVector(420.f, 360.f, 400.f), FVector(500.f, 500.f, 420.f), FVector(500.f, 500.f, 440.f) };
	for (int32 i = 0; i < UE_ARRAY_COUNT(RockAt); ++i) WRFoldLook::Rock(this, RootComponent, i, RockSize[i], RockAt[i], FRotator(0.f, 37.f * i, 0.f), Rock, false, 120.f);
	WRFoldLook::Light(this, RootComponent, FVector(150.f, -360.f, 330.f), FLinearColor(1.f, 0.5f, 0.15f), 9000.f, 1400.f);
	WRFoldLook::Light(this, RootComponent, FVector(150.f, 360.f, 330.f), FLinearColor(0.5f, 0.75f, 1.f), 6000.f, 1200.f);
	if (USceneComponent* Fire = WRFx::Attach(RootComponent, TEXT("brazier.fire"), NAME_None, 0.6f)) Fire->SetRelativeLocation(FVector(150.f, -360.f, 288.f));
}

void AWRWarHallGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || Leaves.Num() < 2) return;
	// The doors stand ajar while a group is going in (open, or the loot time after a win), shut otherwise.
	const float Want = State == EWRHallState::Open || State == EWRHallState::Won ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(LeafOpen, Want, 0.001f)) return;
	LeafOpen = FMath::FInterpConstantTo(LeafOpen, Want, DeltaSeconds, 0.6f);
	const float Ease = FMath::SmoothStep(0.f, 1.f, LeafOpen);
	// They swing in, away from you (the left leaf turns its free edge toward -X: +yaw; the right one -yaw).
	Leaves[0]->SetRelativeRotation(FRotator(0.f, 65.f * Ease, 0.f));
	Leaves[1]->SetRelativeRotation(FRotator(0.f, -65.f * Ease, 0.f));
}

// ================================================================================================ the hall

AWRWarHall::AWRWarHall()
{
	bReplicates = true;
	bAlwaysRelevant = false;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(30000.f));
}

void AWRWarHall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWarHall, HallId);
	DOREPLIFETIME(AWRWarHall, Tiles);
	DOREPLIFETIME(AWRWarHall, GrateAt);
	DOREPLIFETIME(AWRWarHall, bGrateDown);
}

void AWRWarHall::BeginPlay()
{
	Super::BeginPlay();
	const TSharedPtr<FJsonObject> R = WRWarHall::Row(HallId);
	const TArray<TSharedPtr<FJsonValue>>* Size = DArr(R, TEXT("size"));
	Tile = TileNum(HallId, TEXT("size"), 400.f);
	if (Size && Size->Num() >= 2)
	{
		NX = FMath::Max(4, FMath::RoundToInt(float((*Size)[0]->AsNumber()) / Tile));
		NY = FMath::Max(4, FMath::RoundToInt(float((*Size)[1]->AsNumber()) / Tile));
	}
	HalfX = NX * Tile * 0.5f;
	HalfY = NY * Tile * 0.5f;
	if (HasAuthority())
	{
		Tiles.SetNumZeroed(NX * NY);
		FlippedAt.SetNumZeroed(NX * NY);
		RevertAt.SetNumZeroed(NX * NY);
	}
	BuildLook();
}

void AWRWarHall::OnRep_Look() { BuildLook(); }

bool AWRWarHall::Contains(const FVector& W, float Grow) const
{
	const FVector L = W - GetActorLocation();
	return L.X > -HalfX - Grow && L.X < HalfX + Grow && L.Y > -HalfY - 1400.f - Grow && L.Y < HalfY + Grow && L.Z > -800.f && L.Z < 4600.f;
}

bool AWRWarHall::InVestibule(const FVector& W) const
{
	const FVector L = W - GetActorLocation();
	return L.Y < -HalfY && L.Y > -HalfY - 1400.f && FMath::Abs(L.X) < 800.f;
}

int32 AWRWarHall::TileAt(const FVector& W) const
{
	const FVector L = W - GetActorLocation();
	const int32 X = FMath::FloorToInt((L.X + HalfX) / Tile), Y = FMath::FloorToInt((L.Y + HalfY) / Tile);
	return X >= 0 && X < NX && Y >= 0 && Y < NY ? Y * NX + X : INDEX_NONE;
}

FVector AWRWarHall::TileCentre(int32 Index) const
{
	return Local(-HalfX + Tile * (Index % NX + 0.5f), -HalfY + Tile * (Index / NX + 0.5f), 0.f);
}

void AWRWarHall::BuildLook()
{
	if (bBuilt || HallId.IsNone() || NX <= 0) return;
	bBuilt = true;
	const float WallH = 1400.f, Vault = DNum(WRWarHall::Row(HallId), TEXT("vault"), 4000.f);
	const float VY = -HalfY, VD = 1400.f;   // the vestibule runs south from the hall's south wall
	// Collision, every machine: the floor in three strips (two lava gutters run the hall's length between them), the
	// gutters' beds, the walls, the vault, the vestibule; the grate's own box moves with it.
	const auto Box = [this](const FVector& Min, const FVector& Max)
	{
		WRFoldLook::Block(this, RootComponent, (Min + Max) * 0.5f, (Max - Min) * 0.5f);
	};
	const float G = 1200.f, GW = 40.f;   // gutter lines at y = +-1200, 80 cm wide
	Box(FVector(-HalfX, -HalfY, -60.f), FVector(HalfX, -G - GW, 0.f));
	Box(FVector(-HalfX, -G + GW, -60.f), FVector(HalfX, G - GW, 0.f));
	Box(FVector(-HalfX, G + GW, -60.f), FVector(HalfX, HalfY, 0.f));
	for (const float GY : { -G, G }) Box(FVector(-HalfX, GY - GW, -140.f), FVector(HalfX, GY + GW, -80.f));
	Box(FVector(-HalfX - 200.f, -HalfY, -60.f), FVector(-HalfX, HalfY, Vault));
	Box(FVector(HalfX, -HalfY, -60.f), FVector(HalfX + 200.f, HalfY, Vault));
	Box(FVector(-HalfX, HalfY, -60.f), FVector(HalfX, HalfY + 200.f, Vault));
	Box(FVector(-HalfX, VY - 200.f, -60.f), FVector(-500.f, VY, Vault));
	Box(FVector(500.f, VY - 200.f, -60.f), FVector(HalfX, VY, Vault));
	Box(FVector(-500.f, VY - 200.f, 900.f), FVector(500.f, VY, Vault));   // (walls rise to the vault: a flier never slips out under it)
	Box(FVector(-HalfX - 200.f, -HalfY - 200.f, Vault), FVector(HalfX + 200.f, HalfY + 200.f, Vault + 200.f));
	// The vestibule: floor, its walls, its ceiling, the exit arch's wall.
	Box(FVector(-700.f, VY - VD, -60.f), FVector(700.f, VY, 0.f));
	Box(FVector(-900.f, VY - VD, -60.f), FVector(-700.f, VY, 700.f));
	Box(FVector(700.f, VY - VD, -60.f), FVector(900.f, VY, 700.f));
	Box(FVector(-900.f, VY - VD, 700.f), FVector(900.f, VY, 800.f));
	Box(FVector(-900.f, VY - VD - 200.f, -60.f), FVector(900.f, VY - VD, 800.f));
	// The grate's own block rides with it (down: the arch is shut; up: it's in the wall over the arch).
	GrateRoot = NewObject<USceneComponent>(this);
	GrateRoot->SetupAttachment(RootComponent);
	GrateRoot->RegisterComponent();
	WRFoldLook::Block(this, GrateRoot, FVector(0.f, VY - 80.f, 450.f), FVector(500.f, 20.f, 450.f));
	if (GetNetMode() == NM_DedicatedServer) { TickGrate(); return; }

	const TArray<TSharedPtr<FJsonValue>>* Lands = DArr(WRWarHall::Row(HallId), TEXT("lands"));
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Flag = FSurf::Stone(FLinearColor(0.03f, 0.028f, 0.027f), 0.88f);
	const FSurf FlagAlt = FSurf::Stone(FLinearColor(0.045f, 0.041f, 0.038f), 0.9f);
	const FSurf Basalt = FSurf::Stone(FLinearColor(0.04f, 0.036f, 0.034f), 0.9f);
	const FSurf BasaltAlt = FSurf::Stone(FLinearColor(0.06f, 0.054f, 0.05f), 0.92f);
	const FSurf Glacier = FSurf::Glass(FLinearColor(0.3f, 0.5f, 0.72f), 0.04f);
	const FSurf GlacierDeep = FSurf::Glass(FLinearColor(0.08f, 0.18f, 0.36f), 0.03f);
	const FSurf Rib = FSurf::Stone(FLinearColor(0.075f, 0.068f, 0.062f), 0.85f);
	const FSurf Bronze = FSurf::Steel(FLinearColor(0.42f, 0.24f, 0.11f), 0.4f);
	const FSurf Iron = FSurf::Steel(FLinearColor(0.035f, 0.033f, 0.03f), 0.55f);
	const FSurf Molten = FSurf::Lit(FLinearColor(1.f, 0.33f, 0.05f), 1.1f);
	const FSurf Crust = FSurf::Stone(FLinearColor(0.02f, 0.015f, 0.012f), 0.95f);
	// The floor: flags on the 4 m grid, a little proud of one another, joints dark; trimmed where the gutters run.
	for (int32 y = 0; y < NY; ++y)
	{
		for (int32 x = 0; x < NX; ++x)
		{
			const float CX = -HalfX + Tile * (x + 0.5f), CY = -HalfY + Tile * (y + 0.5f);
			float Y0 = CY - Tile * 0.5f + 2.f, Y1 = CY + Tile * 0.5f - 2.f;
			for (const float GY : { -G, G })
			{
				if (Y0 < GY + GW && Y1 > GY - GW) { if (CY < GY) Y1 = GY - GW - 2.f; else Y0 = GY + GW + 2.f; }
			}
			const FVector C(CX, (Y0 + Y1) * 0.5f, -14.f);
			K.Add(((x + y) % 2 ? FlagAlt : Flag).Dim(0.86f + 0.2f * K.Rand(C, 41)), FVector(Tile - 4.f, Y1 - Y0, 28.f + 3.f * K.Rand(C, 43)), C, FRotator(0.f, 0.35f * K.Jit(C, 45), 0.f));
		}
	}
	// The gutters: cut 1.4 m down, their sides dressed basalt, the sump's lava at the bottom with a cooling crust in plates.
	for (const float GY : { -G, G })
	{
		for (float X = -HalfX; X < HalfX; X += 400.f)
		{
			const FVector C(X + 200.f, GY, -110.f);
			K.Add(Molten.Dim(0.75f + 0.35f * K.Rand(C, 51)), FVector(400.f, GW * 2.f, 8.f), C);
			if (K.Rand(C, 53) > 0.45f) K.Add(Crust, FVector(80.f + 120.f * K.Rand(C, 55), GW * 1.6f, 6.f), C + FVector(K.Jit(C, 57) * 120.f, 0.f, 5.f), FRotator(0.f, 6.f * K.Jit(C, 59), 0.f));
			for (const float S : { -1.f, 1.f }) K.Add(BasaltAlt.Dim(0.7f), FVector(400.f, 10.f, 110.f), FVector(X + 200.f, GY + S * (GW + 5.f), -55.f));
		}
		for (float X = -HalfX + 700.f; X < HalfX; X += 1400.f) WRFoldLook::Light(this, RootComponent, FVector(X, GY, -40.f), FLinearColor(1.f, 0.38f, 0.08f), 2600.f, 900.f);
	}
	// The walls: the west half coursed basalt ashlar (the Smith's land), the east half glacier ice (the Wyrm's); 14 m to
	// the spring of the vault.
	const auto Wall = [&](const FVector& A, const FVector& B, float Thick, bool bSouth)
	{
		const FVector Dir = (B - A).GetSafeNormal();
		const float Len = (B - A).Size();
		const FRotator Yaw = Dir.Rotation();
		for (int32 Row = 0; Row < int32(WallH / 100.f); ++Row)
		{
			const float Z = Row * 100.f;
			for (float D = (Row % 2) * -60.f; D < Len; D += 120.f)
			{
				const float D0 = FMath::Max(D, 0.f), D1 = FMath::Min(D + 120.f, Len);
				if (D1 - D0 < 20.f) continue;
				const FVector C = A + Dir * ((D0 + D1) * 0.5f) + FVector(0.f, 0.f, Z + 50.f);
				// The vestibule's archway in the south wall.
				if (bSouth && FMath::Abs(C.X) < 500.f && Z < 900.f) continue;
				const bool bIce = C.X > 0.f;
				const FSurf S = bIce ? (Row % 4 == 0 ? GlacierDeep : Glacier) : ((Row + int32(D / 120.f)) % 3 == 0 ? BasaltAlt : Basalt);
				K.Add(S.Dim(0.88f + 0.2f * K.Rand(C, 61)), FVector(D1 - D0 - 4.f, Thick + 10.f * K.Rand(C, 63), 96.f), C, FRotator(0.f, Yaw.Yaw, 0.f));
			}
		}
	};
	Wall(FVector(-HalfX - 80.f, -HalfY, 0.f), FVector(-HalfX - 80.f, HalfY, 0.f), 120.f, false);
	Wall(FVector(HalfX + 80.f, -HalfY, 0.f), FVector(HalfX + 80.f, HalfY, 0.f), 120.f, false);
	Wall(FVector(-HalfX, HalfY + 80.f, 0.f), FVector(HalfX, HalfY + 80.f, 0.f), 120.f, false);
	Wall(FVector(-HalfX, -HalfY - 80.f, 0.f), FVector(HalfX, -HalfY - 80.f, 0.f), 120.f, true);
	// The vault: eight pairs of ribs across the hall's width, a barrel between them in long stones.
	const float Rise = Vault - WallH, Span = HalfY;
	const int32 Seg = 18;
	for (int32 r = 0; r <= 8; ++r)
	{
		const float X = -HalfX + (2.f * HalfX) * r / 8.f;
		for (int32 s = 0; s < Seg; ++s)
		{
			const float A0 = PI * s / Seg, A1 = PI * (s + 1) / Seg;
			const FVector P0(X, -FMath::Cos(A0) * Span, WallH + FMath::Sin(A0) * Rise), P1(X, -FMath::Cos(A1) * Span, WallH + FMath::Sin(A1) * Rise);
			K.Rod(Rib, P0, P1, 120.f, true);
		}
		if (r == 8) continue;
		for (int32 s = 0; s < Seg; ++s)
		{
			const float A0 = PI * s / Seg, A1 = PI * (s + 1) / Seg;
			const FVector P0(X + HalfX / 8.f, -FMath::Cos(A0) * Span, WallH + FMath::Sin(A0) * Rise + 40.f), P1(X + HalfX / 8.f, -FMath::Cos(A1) * Span, WallH + FMath::Sin(A1) * Rise + 40.f);
			const FVector Mid = (P0 + P1) * 0.5f;
			const FVector Along = (P1 - P0).GetSafeNormal();
			const FRotator Rot = FRotationMatrix::MakeFromYZ(Along, FVector::CrossProduct(FVector::XAxisVector, Along)).Rotator();
			K.Add((Mid.X > 0.f ? GlacierDeep : Basalt).Dim(0.85f + 0.15f * K.Rand(Mid, 65)), FVector(2.f * HalfX / 8.f - 120.f, (P1 - P0).Size() + 8.f, 60.f), Mid, Rot);
		}
	}
	// Braziers on the west wall (the Smith's side), cold lamps behind the ice on the east.
	for (int32 i = 0; i < 6; ++i)
	{
		const float Y = -HalfY + 600.f + i * (2.f * HalfY - 1200.f) / 5.f;
		const FVector W(-HalfX + 120.f, Y, 0.f);
		K.Add(Iron, FVector(18.f, 18.f, 180.f), W + FVector(0, 0, 90.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Iron, FVector(90.f, 90.f, 30.f), W + FVector(0, 0, 195.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Molten, FVector(76.f, 76.f, 8.f), W + FVector(0, 0, 212.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		WRFoldLook::Light(this, RootComponent, W + FVector(40.f, 0.f, 300.f), FLinearColor(1.f, 0.5f, 0.16f), 14000.f, 2600.f);
		if (USceneComponent* Fire = WRFx::Attach(RootComponent, TEXT("brazier.fire"), NAME_None, 1.f)) Fire->SetRelativeLocation(W + FVector(0, 0, 214.f));
		const FVector E(HalfX - 40.f, Y, 700.f);
		K.Add(FSurf::Lit(FLinearColor(0.45f, 0.72f, 1.f), 0.6f), FVector(30.f, 180.f, 260.f), E);
		WRFoldLook::Light(this, RootComponent, E + FVector(-160.f, 0.f, 0.f), FLinearColor(0.5f, 0.75f, 1.f), 9000.f, 2800.f);
	}
	// The inner threshold: a bronze strip across the arch (the line the seal waits for), and the vestibule: flags,
	// walls, a low ceiling, the exit arch with its bronze-bound frame.
	K.Box(Bronze, FVector(-500.f, -HalfY - 6.f, -2.f), FVector(500.f, -HalfY + 6.f, 1.f));
	for (float X = -700.f; X < 700.f; X += 200.f)
		for (float Y = VY - VD; Y < VY; Y += 200.f)
			K.Add(Flag.Dim(0.85f + 0.2f * K.Rand(FVector(X, Y, 0), 71)), FVector(196.f, 196.f, 24.f), FVector(X + 100.f, Y + 100.f, -12.f));
	for (const float S : { -1.f, 1.f }) K.Box(S < 0 ? Basalt : Glacier, FVector(S < 0 ? -900.f : 700.f, VY - VD, 0.f), FVector(S < 0 ? -700.f : 900.f, VY, 700.f));
	K.Box(BasaltAlt.Dim(0.7f), FVector(-900.f, VY - VD, 700.f), FVector(900.f, VY, 800.f));
	K.Box(Basalt, FVector(-900.f, VY - VD - 200.f, 0.f), FVector(900.f, VY - VD, 800.f));
	for (const float S : { -1.f, 1.f }) K.Box(Bronze, FVector(S * 220.f - 20.f, VY - VD + 2.f, 0.f), FVector(S * 220.f + 20.f, VY - VD + 12.f, 420.f));
	K.Box(Bronze, FVector(-240.f, VY - VD + 2.f, 420.f), FVector(240.f, VY - VD + 12.f, 450.f));
	K.Box(FSurf::Lit(FLinearColor(0.9f, 0.85f, 0.75f), 0.35f), FVector(-200.f, VY - VD + 1.f, 0.f), FVector(200.f, VY - VD + 3.f, 420.f));   // daylight up the stair
	WRFoldLook::Light(this, RootComponent, FVector(0.f, VY - VD + 300.f, 450.f), FLinearColor(1.f, 0.9f, 0.75f), 7000.f, 1600.f);
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);

	// The grate: a portcullis of iron bars in the arch, running in grooves cut into the jambs (it drops on its weight,
	// is winched up on two chains over a drum in the wall: what you see moving is what holds it).
	{
		TArray<FWRLookBlock> GB;
		FKit GK(GetActorTransform(), GB);
		// Eleven square bars 92 cm apart across the 10 m arch, three flat cross-bands riveted on, a spike at each foot. It
		// runs in a groove in the middle of the wall's thickness, and rises into the wall and the stone chamber over it.
		const float GY = VY - 80.f;
		for (int32 b = 0; b < 11; ++b)
		{
			const float X = -460.f + b * 92.f;
			GK.Box(Iron, FVector(X - 5.f, GY - 8.f, 26.f), FVector(X + 5.f, GY + 8.f, 900.f));
			GK.Spike(Iron, FVector(X, GY, 28.f), FVector(0.f, 0.f, -1.f), 12.f, 26.f);
		}
		for (const float Z : { 120.f, 420.f, 720.f }) GK.Box(Iron, FVector(-480.f, GY - 10.f, Z), FVector(480.f, GY + 10.f, Z + 16.f));
		for (int32 b = 0; b < 11; ++b) for (const float Z : { 128.f, 428.f, 728.f }) GK.Add(Iron, FVector(5.f, 5.f, 4.f), FVector(-460.f + b * 92.f, GY + 12.f, Z), FRotator(90.f, 0.f, 0.f) + FRotator(0.f, 90.f, 0.f), EWRLookMesh::Cylinder);   // rivets
		FWRBuildLook::Spawn(this, GrateRoot, GB, FLinearColor(0.3f, 0.25f, 0.2f), false);
	}
	// The grate chamber over the arch: coursed stone that houses the raised grate and its winch (above the wall's top).
	{
		TArray<FWRLookBlock> HB;
		FKit HK(GetActorTransform(), HB);
		for (int32 Row = 0; Row < 6; ++Row)
			for (int32 c = 0; c < 9; ++c)
			{
				const FVector C(-540.f + c * 135.f + (Row % 2) * 30.f, VY - 80.f, WallH + 50.f + Row * 100.f);
				HK.Add((C.X > 0.f ? GlacierDeep : BasaltAlt).Dim(0.85f + 0.2f * HK.Rand(C, 81)), FVector(132.f, 200.f, 96.f), C);
			}
		HK.Box(Rib, FVector(-620.f, VY - 200.f, WallH + 600.f), FVector(620.f, VY + 40.f, WallH + 660.f));
		FWRBuildLook::Spawn(this, RootComponent, HB, FLinearColor(0.3f, 0.25f, 0.2f), false);
	}
	// The flags' live sheets: lava over the basalt where it's lava, ice where it's ice.
	LavaMesh = NewObject<UProceduralMeshComponent>(this);
	LavaMesh->SetupAttachment(RootComponent);
	LavaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LavaMesh->SetCastShadow(false);
	LavaMesh->RegisterComponent();
	IceMesh = NewObject<UProceduralMeshComponent>(this);
	IceMesh->SetupAttachment(RootComponent);
	IceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IceMesh->SetCastShadow(false);
	IceMesh->RegisterComponent();
	if (UMaterialInterface* Lava = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Lava.M_Lava")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Lava, this);
		M->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("lava.glow"), 1.f));
		M->SetScalarParameterValue(TEXT("HotLevel"), WRTune(TEXT("lava.hotLevel"), 6.f));
		M->SetScalarParameterValue(TEXT("MoltenCore"), WRTune(TEXT("lava.moltenCore"), 0.9f));
		M->SetScalarParameterValue(TEXT("Steady"), WRTune(TEXT("lava.steady"), 0.85f));
		LavaMesh->SetMaterial(0, M);
	}
	if (UMaterialInterface* IceMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Ice.M_Ice"))) IceMesh->SetMaterial(0, IceMat);
	DrawTiles();
	TickGrate();
}

void AWRWarHall::OnRep_Tiles()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// Every flag that changed hisses or cracks where it is (lava pours, ice creaks).
	for (int32 i = 0; i < Tiles.Num() && i < Drawn.Num(); ++i)
	{
		if (Tiles[i] == Drawn[i]) continue;
		const FVector C = TileCentre(i);
		if (Tiles[i] == 1) WRFx::SpawnAt(this, TEXT("lava.embers"), C, FRotator::ZeroRotator, 1.4f);
		else if (Tiles[i] == 2) WRFx::SpawnAt(this, TEXT("hit.ice"), C, FRotator::ZeroRotator, 2.f);
	}
	DrawTiles();
}

void AWRWarHall::DrawTiles()
{
	if (!LavaMesh || !IceMesh || Tiles.Num() != NX * NY) return;
	int32 LavaN = 0, IceN = 0;
	for (int32 i = 0; i < Tiles.Num(); ++i) { LavaN += Tiles[i] == 1; IceN += Tiles[i] == 2; }
	bool bChanged = Drawn != Tiles;
	if (!bChanged) return;
	if (Drawn.Num() == Tiles.Num())
	{
		// A sound for the flags that turned (one per kind per change, at the first one's place).
		for (int32 i = 0; i < Tiles.Num(); ++i)
		{
			if (Tiles[i] == Drawn[i]) continue;
			WRSound::PlayAt(this, Tiles[i] == 1 ? FName(TEXT("warhall.lava")) : Tiles[i] == 2 ? FName(TEXT("warhall.ice")) : FName(TEXT("warhall.thaw")), TileCentre(i));
			break;
		}
	}
	Drawn = Tiles;
	// Each flag a 3 x 3 grid of points: lava molten in the middle (R = heat) and crusted at the joints; ice a glossy slab.
	const auto Section = [this](UProceduralMeshComponent* Mesh, uint8 Kind, float Lift)
	{
		TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Col;
		for (int32 i = 0; i < Tiles.Num(); ++i)
		{
			if (Tiles[i] != Kind) continue;
			const FVector C = TileCentre(i) - GetActorLocation();
			const int32 Base = V.Num();
			const float H = Tile * 0.5f - 6.f;
			for (int32 yy = 0; yy < 3; ++yy)
			{
				for (int32 xx = 0; xx < 3; ++xx)
				{
					const FVector P = C + FVector((xx - 1) * H, (yy - 1) * H, Lift + (xx == 1 && yy == 1 ? 2.f : 0.f));
					V.Add(P);
					N.Add(FVector::UpVector);
					UV.Add(FVector2D(P.X, P.Y) / 400.f);
					const bool bMid = xx == 1 && yy == 1;
					Col.Add(Kind == 1 ? FLinearColor(bMid ? 1.f : 0.4f, 1.f, 0.f, 1.f) : FLinearColor::White);
				}
			}
			for (int32 yy = 0; yy < 2; ++yy)
			{
				for (int32 xx = 0; xx < 2; ++xx)
				{
					const int32 A = Base + yy * 3 + xx;
					T.Append({ A, A + 3, A + 1, A + 1, A + 3, A + 4 });
				}
			}
		}
		if (V.Num() == 0) { Mesh->ClearAllMeshSections(); Mesh->SetVisibility(false); return; }
		Mesh->CreateMeshSection_LinearColor(0, V, T, N, UV, Col, TArray<FProcMeshTangent>(), false);
		Mesh->SetVisibility(true);
	};
	Section(LavaMesh, 1, 2.f);
	Section(IceMesh, 2, 4.f);
}

void AWRWarHall::TickGrate()
{
	if (!GrateRoot) return;
	// 3 s down on its weight, 6 s up on the winch; from the server's time on every machine.
	const float Since = float(DNow(this)) - GrateAt;
	const float Down = bGrateDown ? FMath::Clamp(Since / 3.f, 0.f, 1.f) : 1.f - FMath::Clamp(Since / 6.f, 0.f, 1.f);
	const float Ease = bGrateDown ? Down * Down : FMath::SmoothStep(0.f, 1.f, Down);
	GrateRoot->SetRelativeLocation(FVector(0.f, 0.f, 900.f * (1.f - Ease)));
}

void AWRWarHall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickGrate();
	if (!HasAuthority()) return;
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	AWRWarHallGate* G = Gate.Get();
	// Flags go back to basalt as they cool or melt away.
	bool bTilesChanged = false;
	for (int32 i = 0; i < Tiles.Num(); ++i)
	{
		if (Tiles[i] != 0 && RevertAt[i] > 0.0 && Now >= RevertAt[i]) { Tiles[i] = 0; RevertAt[i] = 0.0; bTilesChanged = true; }
	}
	if (bTilesChanged && GetNetMode() != NM_DedicatedServer) OnRep_Tiles();
	// Lava burns what stands on it (and anyone down in a gutter): through ApplyHit, a slow tick.
	HazardClock += DeltaSeconds;
	int32 InsideNow = 0;
	TArray<FString> Here;
	if (HazardClock >= 0.25f)
	{
		const float Dt = HazardClock;
		HazardClock = 0.f;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive() || C->GetAttachParentActor() || !Contains(C->GetActorLocation())) continue;
			const FVector Feet = C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight());
			const FVector L = Feet - GetActorLocation();
			const bool bGutter = L.Z < -60.f && (FMath::Abs(L.Y - 1200.f) < 60.f || FMath::Abs(L.Y + 1200.f) < 60.f);
			const int32 T = TileAt(Feet);
			const bool bLava = bGutter || (T != INDEX_NONE && Tiles[T] == 1 && OnFloor(this, C));
			if (!bLava || FWRBiomeHazards::Resists(C, TEXT("lava"))) continue;
			FWRHit Hit;
			Hit.Instigator = this;
			Hit.Damage = TileNum(HallId, TEXT("lavaDps"), 30.f) * Dt * (bGutter ? 2.f : 1.f);
			Hit.Element = WRTags::Element_Fire;
			Hit.BurnBuildup = TileNum(HallId, TEXT("lavaBurn"), 20.f) * Dt;
			Hit.bDamageOverTime = true;
			Hit.Origin = Feet;
			UWRCombatLibrary::ApplyHit(C, Hit);
		}
	}
	for (TActorIterator<AWRCharacter> It(World); It; ++It)
	{
		// Only the living count: a wiped group on its death screens doesn't hold the hall.
		if (!It->GetPlayerState() || !It->IsAlive() || !Contains(It->GetActorLocation(), 200.f)) continue;
		++InsideNow;
		Here.Add(It->GetPlayerState()->GetPlayerName());
		// The seal: the first of the group to cross the bronze line.
		if (G && G->State == EWRHallState::Open && It->IsAlive() && It->GetActorLocation().Y - GetActorLocation().Y > -HalfY + 150.f)
		{
			// Everyone inside now is the group (the vestibule too).
			TArray<FString> Members;
			for (TActorIterator<AWRCharacter> J(World); J; ++J) if (J->GetPlayerState() && J->IsAlive() && Contains(J->GetActorLocation(), 200.f)) Members.AddUnique(J->GetPlayerState()->GetPlayerName());
			G->Group = Members;
			SetState(EWRHallState::Sealed);
			TellHall(this, FString::Printf(TEXT("The grate falls behind you: %s is sealed"), *WRWarHall::Name(HallId)));
		}
	}
	if (G) G->Inside = InsideNow;
	// The fight's clock: from the first blow on either champion.
	if (FightStart <= 0.0)
	{
		for (const TWeakObjectPtr<AWRBoss>& B : Champions) if (B.IsValid() && B->FirstHitAt > 0.0) FightStart = FightStart <= 0.0 ? B->FirstHitAt : FMath::Min(FightStart, B->FirstHitAt);
	}
	if (!G) return;
	switch (G->State)
	{
	case EWRHallState::Open:
	case EWRHallState::Sealed:
	{
		// Nobody of the group inside for emptySeconds: the champions reset (they do, on their own), the grate rises.
		// A group that goes in but never crosses the bronze line doesn't hold the door: after openSeconds they're walked
		// back out and the hall is free for the next.
		if (G->State == EWRHallState::Open && float(DNow(this)) - G->StateSince > Frame(TEXT("openSeconds"), 120.f))
		{
			TellHall(this, TEXT("The door won't wait: come back when you're ready to cross the line"));
			Clear(true);
			G->Leader.Reset();
			ResetHall();
			break;
		}
		const bool bAny = G->State == EWRHallState::Open ? InsideNow > 0 : Here.ContainsByPredicate([G](const FString& N) { return G->Group.Contains(N); });
		if (bAny) { EmptySince = -1.0; break; }
		if (EmptySince < 0.0) EmptySince = Now;
		if (Now - EmptySince >= Frame(TEXT("emptySeconds"), 8.f)) ResetHall();
		break;
	}
	case EWRHallState::Won:
		if (Now >= LootUntil) { Clear(true); SetState(EWRHallState::Resting); }
		break;
	case EWRHallState::Resting:
		if (Unix() >= G->RestUntil) { SpawnChampions(); SetState(EWRHallState::Ready); }
		break;
	default:
		break;
	}
}

void AWRWarHall::SetState(EWRHallState S)
{
	AWRWarHallGate* G = Gate.Get();
	if (!G) return;
	G->State = S;
	G->StateSince = float(DNow(this));
	const bool bDown = S == EWRHallState::Sealed;
	if (bDown != bGrateDown)
	{
		bGrateDown = bDown;
		GrateAt = float(DNow(this));
		WRSound::PlayAt(this, bDown ? FName(TEXT("warhall.seal")) : FName(TEXT("warhall.door")), Local(0.f, -HalfY, 300.f));
		WRFx::Broadcast(this, TEXT("dust.cloud"), Local(0.f, -HalfY, 20.f), FRotator::ZeroRotator, 3.f);
	}
	if (S == EWRHallState::Ready) { G->Group.Reset(); G->Leader.Reset(); EmptySince = -1.0; bTriumphSeen = false; FightStart = 0.0; }
	if (G->GetNetMode() != NM_DedicatedServer) G->ForceNetUpdate();
	SaveDoc(GetWorld());
	UE_LOG(LogWildreach, Log, TEXT("WARHALL %s: %s"), *HallId.ToString(), *UEnum::GetValueAsString(S));
}

void AWRWarHall::SpawnChampions()
{
	RaidLedger.Reset();
	RaidSize = 0;
	FightStart = 0.0;
	bTriumphSeen = false;
	for (const TWeakObjectPtr<AWRBoss>& B : Champions) if (B.IsValid()) B->Destroy();
	Champions.Reset();
	const TSharedPtr<FJsonObject> R = WRWarHall::Row(HallId);
	const TSharedPtr<FJsonObject> Spawns = DChild(R, TEXT("spawns"));
	if (const TArray<TSharedPtr<FJsonValue>>* C = DArr(R, TEXT("champions")))
	{
		for (const TSharedPtr<FJsonValue>& V : *C)
		{
			const FName Id(*V->AsString());
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Id);
			const TSharedPtr<FJsonValue> At = Spawns.IsValid() ? Spawns->TryGetField(Id.ToString()) : TSharedPtr<FJsonValue>();
			if (!Def) continue;
			const FVector Spot = Local(DAt(At, 0, 0.f), DAt(At, 1, 0.f), 0.f);
			// Each faces the other down the hall's length.
			const float Yaw = DAt(At, 0, 0.f) < 0.f ? 0.f : 180.f;
			const FTransform T(FRotator(0.f, Yaw, 0.f), Spot + FVector(0.f, 0.f, FMath::Max(Def->HalfHeight, Def->Radius) + 10.f));
			AWRBoss* B = GetWorld()->SpawnActorDeferred<AWRBoss>(AWRBoss::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
			if (!B) continue;
			B->CreatureId = Id;
			B->FinishSpawning(T);
			UWRChampionBrain* Brain = NewObject<UWRChampionBrain>(B);
			Brain->Hall = this;
			Brain->RegisterComponent();
			Champions.Add(B);
		}
	}
	// Rivals: each the other's.
	if (Champions.Num() == 2)
	{
		for (int32 i = 0; i < 2; ++i)
			if (UWRChampionBrain* Br = Champions[i]->FindComponentByClass<UWRChampionBrain>()) Br->Rival = Champions[1 - i];
	}
	// The floor goes back to basalt.
	for (int32 i = 0; i < Tiles.Num(); ++i) { Tiles[i] = 0; RevertAt[i] = 0.0; FlippedAt[i] = 0.0; }
	if (GetNetMode() != NM_DedicatedServer) OnRep_Tiles();
	FightStart = 0.0;
	bTriumphSeen = false;
	UE_LOG(LogWildreach, Log, TEXT("WARHALL %s: %d champions rise"), *HallId.ToString(), Champions.Num());
}

void AWRWarHall::Clear(bool bToGate)
{
	AWRWarHallGate* G = Gate.Get();
	if (!G) return;
	// Everyone still inside is walked out to the gate; bags left inside go there too.
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		if (!Contains(It->GetActorLocation(), 300.f)) continue;
		if (bToGate) Put(*It, G->FrontPoint(), G->GetActorRotation().Yaw);
	}
	for (TActorIterator<AWRLootBag> It(GetWorld()); It; ++It)
	{
		if (!Contains(It->GetActorLocation(), 300.f)) continue;
		// The move replicates (bags don't usually move), and the bag sits on the ground by the gate's steps.
		It->SetReplicateMovement(true);
		FVector To = G->FrontPoint() + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHallBag), false, *It);
		if (GetWorld()->LineTraceSingleByChannel(Hit, To + FVector(0, 0, 1500.f), To - FVector(0, 0, 3000.f), ECC_WorldStatic, Q)) To = Hit.ImpactPoint;
		It->SetActorLocation(To + FVector(0, 0, 40.f));
		It->ForceNetUpdate();
	}
}

void AWRWarHall::ResetHall()
{
	RaidLedger.Reset();
	RaidSize = 0;
	UE_LOG(LogWildreach, Log, TEXT("WARHALL %s: empty, reset"), *HallId.ToString());
	Clear(false);
	for (const TWeakObjectPtr<AWRBoss>& B : Champions) if (B.IsValid()) if (UWRChampionBrain* Br = B->FindComponentByClass<UWRChampionBrain>()) Br->Reset();
	for (int32 i = 0; i < Tiles.Num(); ++i) { Tiles[i] = 0; RevertAt[i] = 0.0; }
	if (GetNetMode() != NM_DedicatedServer) OnRep_Tiles();
	// A champion that died while the other lived rises again with it (they're fought as one).
	const bool bAllAlive = Champions.Num() == 2 && Champions[0].IsValid() && Champions[1].IsValid() && Champions[0]->IsAlive() && Champions[1]->IsAlive();
	if (!bAllAlive) SpawnChampions();
	SetState(EWRHallState::Ready);
}

void AWRWarHall::Won()
{
	AWRWarHallGate* G = Gate.Get();
	if (!G) return;
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	LootUntil = Now + Frame(TEXT("lootSeconds"), 120.f);
	G->RestUntil = Unix() + 3600.0 * FMath::Clamp(WRTune(TEXT("server.raidRespawnHours"), 24.f), 1.f, 168.f);
	SetState(EWRHallState::Won);
	WRSound::PlayAt(this, TEXT("warhall.win"), Local(0.f, 0.f, 600.f));
	TellHall(this, FString::Printf(TEXT("%s is won. The grate rises; the hall empties in %d minutes."), *WRWarHall::Name(HallId), FMath::RoundToInt(Frame(TEXT("lootSeconds"), 120.f) / 60.f)));
	UE_LOG(LogWildreach, Log, TEXT("WARHALL %s won (%.0f s)"), *HallId.ToString(), FightStart > 0.0 ? Now - FightStart : 0.0);
}

// --------------------------------------------------------------------------------------------- the flags

void AWRWarHall::Flip(const FVector& Centre, float Radius, uint8 NewState, AActor* By, float ConeHalfAngle, const FVector& Dir)
{
	const FVector D2 = Dir.GetSafeNormal2D();
	// One blow's Thaws hurt each body once, however many of its flags burst (WRWarHall::Thaw).
	ThawHit.Reset();
	bInFlip = true;
	for (int32 i = 0; i < Tiles.Num(); ++i)
	{
		const FVector C = TileCentre(i);
		const FVector To = C - Centre;
		const float Dist = To.Size2D();
		if (Dist > Radius + Tile * 0.35f) continue;
		if (ConeHalfAngle > 0.f && Dist > 50.f && FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(To.GetSafeNormal2D(), D2))) > ConeHalfAngle) continue;
		FlipTile(i, NewState, By);
	}
	bInFlip = false;
	if (GetNetMode() != NM_DedicatedServer) OnRep_Tiles();
}

void AWRWarHall::FlipTile(int32 i, uint8 NewState, AActor* By)
{
	if (!Tiles.IsValidIndex(i)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const uint8 Old = Tiles[i];
	// The Thaw: lava to ice or ice to lava inside the window bursts in steam.
	if ((Old == 1 && NewState == 2) || (Old == 2 && NewState == 1))
	{
		if (Now - FlippedAt[i] <= TileNum(HallId, TEXT("thawWindow"), 3.f)) { Thaw(i); return; }
	}
	Tiles[i] = NewState;
	FlippedAt[i] = Now;
	RevertAt[i] = NewState == 0 ? 0.0 : Now + TileNum(HallId, TEXT("revertSeconds"), 25.f);
	// A champion's slam melting the ice under its rival: the rival slips (stance x1.5 for a while).
	if (NewState != 2 && Old == 2)
	{
		for (const TWeakObjectPtr<AWRBoss>& B : Champions)
		{
			if (!B.IsValid() || B.Get() == By || !B->IsAlive()) continue;
			if (FVector::Dist2D(B->GetActorLocation(), TileCentre(i)) < B->GetSimpleCollisionRadius() + Tile * 0.5f)
			{
				if (UWRChampionBrain* Br = B->FindComponentByClass<UWRChampionBrain>())
				{
					if (Br->SlipUntil < Now) WRFx::Broadcast(B.Get(), TEXT("hit.ice"), B->GetActorLocation() - FVector(0, 0, B->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 3.f, TEXT("warhall.ice"));
					Br->SlipUntil = Now + TileNum(HallId, TEXT("slipSeconds"), 4.f);
				}
			}
		}
	}
}

void AWRWarHall::Thaw(int32 i)
{
	const FVector C = TileCentre(i);
	Tiles[i] = 0;
	RevertAt[i] = 0.0;
	FlippedAt[i] = 0.0;
	WRFx::Broadcast(this, TEXT("reaction.steam"), C + FVector(0, 0, 40.f), FRotator::ZeroRotator, 3.f, TEXT("warhall.thaw"));
	WRFx::Shake(this, 0.5f, C, 2500.f);
	// Everything on it or touching it: survivors, tames and champions alike.
	const float R = TileNum(HallId, TEXT("thawRadius"), 300.f);
	if (!bInFlip) ThawHit.Reset();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || FVector::Dist2D(Who->GetActorLocation(), C) > R + Who->GetSimpleCollisionRadius() || FMath::Abs(Who->GetActorLocation().Z - Who->GetSimpleCollisionHalfHeight() - C.Z) > 200.f) continue;
		if (ThawHit.Contains(Who)) continue;
		ThawHit.Add(Who);
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = TileNum(HallId, TEXT("thawDamage"), 150.f);
		Hit.PoiseDamage = 60.f;
		Hit.Impact = 3;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.bHeavy = true;
		Hit.bUnblockable = true;
		Hit.Element = WRTags::Element_Physical;
		Hit.Buildups.Add(TEXT("blind"), TileNum(HallId, TEXT("thawBlind"), 20.f));
		Hit.Origin = C;
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
	UE_LOG(LogWildreach, Log, TEXT("WARHALL %s: a Thaw bursts at flag %d"), *HallId.ToString(), i);
}

// ================================================================================================ the champion's brain

UWRChampionBrain::UWRChampionBrain()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
}

void UWRChampionBrain::Reset()
{
	bTriumph = false;
	SlipUntil = HardenUntil = NextRivalAt = 0.0;
	if (AWRBoss* B = Cast<AWRBoss>(GetOwner()); B && BaseSpeed > 0.f) B->GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
}

void UWRChampionBrain::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRBoss* Me = Cast<AWRBoss>(GetOwner());
	AWRWarHall* H = Hall.Get();
	if (!Me || !H || !Me->HasAuthority() || !Me->IsAlive()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	UCharacterMovementComponent* Move = Me->GetCharacterMovement();
	if (BaseSpeed <= 0.f) BaseSpeed = Move->MaxWalkSpeed;
	if (!Me->bEngaged && bTriumph) Reset();
	// Triumph: its rival under triumphAt while it's above it (keep them even).
	const AWRBoss* R = Rival.Get();
	const auto Frac = [](const AWRBoss* B) { return B && B->GetAttributes() ? B->GetAttributes()->GetHealth() / FMath::Max(1.f, B->GetAttributes()->GetMaxHealth()) : 0.f; };
	const float At = Frame(TEXT("triumphAt"), 0.3f);
	if (!bTriumph && Me->bEngaged && (!R || !R->IsAlive() || Frac(R) < At) && Frac(Me) >= At)
	{
		bTriumph = true;
		H->bTriumphSeen = true;
		WRFx::Broadcast(Me, TEXT("roar.blast"), Me->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("warhall.triumph"));
		WRFx::BroadcastOn(Me, Me->CreatureId == TEXT("fallen_smith") ? FName(TEXT("lava.embers")) : FName(TEXT("portal.snow")), NAME_None, 2.f, NAME_None, 600.f);
		WRSound::Voice(Me, TEXT("aggro"));
		TellHall(H, FString::Printf(TEXT("%s triumphs: it hits harder and moves faster"), *Me->GetDisplayLabel()));
		UE_LOG(LogWildreach, Log, TEXT("WARHALL %s triumphs"), *Me->GetDisplayLabel());
	}
	// Speed: Triumph faster (hardened seams slow the attacks, not the walk: WRWarHall::RecoveryScale).
	const float Speed = BaseSpeed * WRWarHall::SpeedScale(Me);
	if (!FMath::IsNearlyEqual(Move->MaxWalkSpeed, Speed)) Move->MaxWalkSpeed = Speed;
	// The Smith's Quenching (hidden third phase): the ice he steps on hisses to stone, the steam blinding who's near.
	if (Me->Phase >= 3 && Me->CreatureId == TEXT("fallen_smith"))
	{
		const int32 T = H->TileAt(Me->GetActorLocation());
		if (T != INDEX_NONE && H->Tiles[T] == 2)
		{
			H->Tiles[T] = 0;
			H->RevertAt[T] = 0.0;
			const FVector C = H->TileCentre(T);
			WRFx::Broadcast(Me, TEXT("reaction.steam"), C + FVector(0, 0, 30.f), FRotator::ZeroRotator, 2.f, TEXT("warhall.thaw"));
			for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), C) > TileNum(H->HallId, TEXT("quenchRadius"), 300.f)) continue;
				FWRHit Hit;
				Hit.Instigator = Me;
				Hit.Damage = 1.f;
				Hit.bDamageOverTime = true;
				Hit.Buildups.Add(TEXT("blind"), TileNum(H->HallId, TEXT("quenchBlind"), 20.f));
				UWRCombatLibrary::ApplyHit(*It, Hit);
			}
			H->RedrawTiles();
			H->ForceNetUpdate();
		}
	}
}

// ================================================================================================ the boss's hooks

bool WRWarHall::IsRival(const AWRBoss* Boss, const AWRCharacterBase* Other)
{
	const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	return B && Other && B->Rival.Get() == Other;
}

bool WRWarHall::MayTargetRival(const AWRBoss* Boss, float Dist)
{
	const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	return B && Dist <= Frame(TEXT("rivalReach"), 700.f) && Boss->GetWorld()->GetTimeSeconds() >= B->NextRivalAt;
}

void WRWarHall::OnAttackStart(AWRBoss* Boss, const AWRCharacterBase* Target)
{
	UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	if (B && Target && B->Rival.Get() == Target) B->NextRivalAt = Boss->GetWorld()->GetTimeSeconds() + Frame(TEXT("rivalEvery"), 12.f);
}

float WRWarHall::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	if (!B) return Damage;
	// A rival's blow lands at half (and counts for no one: it has no player behind it).
	if (Hit.Instigator && Hit.Instigator == B->Rival.Get()) return Damage * Frame(TEXT("crossDamage"), 0.5f);
	return Damage;
}

float WRWarHall::SpeedScale(const AWRBoss* Boss)
{
	const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	return B && B->bTriumph ? Frame(TEXT("triumphSpeed"), 1.15f) : 1.f;
}

float WRWarHall::RecoveryScale(const AWRBoss* Boss)
{
	// Hardened seams: attack speed x hardenSpeed, i.e. the pause between strings / hardenSpeed.
	const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	const AWRWarHall* H = B ? B->Hall.Get() : nullptr;
	if (!H || Boss->GetWorld()->GetTimeSeconds() >= B->HardenUntil) return 1.f;
	return 1.f / FMath::Max(0.3f, TileNum(H->HallId, TEXT("hardenSpeed"), 0.85f));
}

FVector WRWarHall::ClampInside(const AWRBoss* Boss, const FVector& Want)
{
	const AWRWarHall* H = Boss ? HallAt(Boss, Boss->GetActorLocation()) : nullptr;
	if (!H) return Want;
	const float R = Boss->GetSimpleCollisionRadius() + 200.f;
	const FVector O = H->GetActorLocation();
	const float Top = DNum(Row(H->HallId), TEXT("vault"), 4000.f) - Boss->GetSimpleCollisionHalfHeight() - 300.f;
	return FVector(FMath::Clamp(Want.X, O.X - H->HalfX + R, O.X + H->HalfX - R), FMath::Clamp(Want.Y, O.Y - H->HalfY + R, O.Y + H->HalfY - R),
		FMath::Clamp(Want.Z, O.Z + Boss->GetSimpleCollisionHalfHeight(), O.Z + FMath::Max(Top, 600.f)));
}

float WRWarHall::Outgoing(const AWRBoss* Boss)
{
	const UWRChampionBrain* B = Boss ? Boss->FindComponentByClass<UWRChampionBrain>() : nullptr;
	if (!B) return 1.f;
	// Raid tuned for (server): champions hit as hard as a group of that size can take, around the design's 8.
	const float Tuned = FMath::Clamp(float(FWRRaiding::RaidGroupTuned()) / 8.f, Frame(TEXT("tunedMin"), 0.5f), Frame(TEXT("tunedMax"), 1.5f));
	return Tuned * (B->bTriumph ? 1.f + Frame(TEXT("triumphDamage"), 0.2f) : 1.f);
}

float WRWarHall::StanceScale(const AWRCharacterBase* Target)
{
	const UWRChampionBrain* B = Target ? Target->FindComponentByClass<UWRChampionBrain>() : nullptr;
	if (!B || !B->Hall.IsValid()) return 1.f;
	return Target->GetWorld()->GetTimeSeconds() < B->SlipUntil ? TileNum(B->Hall->HallId, TEXT("slipStance"), 1.5f) : 1.f;
}

bool WRWarHall::Aftermath(AWRBoss* Boss, FName AttackId, const FVector& At, float Size, const FRotator& Facing)
{
	AWRWarHall* H = HallOf(Boss);
	if (!H) return false;
	const FName Hall = H->HallId;
	const FString Id = AttackId.ToString();
	const FVector Floor = Boss->GetActorLocation() - FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight());
	if (Boss->CreatureId == TEXT("fallen_smith"))
	{
		// Fire melts: his slams and the slag's landing turn flags to lava; the bellows' cone too.
		if (Id == TEXT("overhead_slam") || Id == TEXT("quench_feint")) { H->Flip(At, TileNum(Hall, TEXT("slam"), 600.f), 1, Boss); return true; }
		if (Id == TEXT("slag_throw")) { H->Flip(At, TileNum(Hall, TEXT("slag"), 200.f), 1, Boss); return true; }
		if (Id == TEXT("bellows_breath")) { H->Flip(Floor, TileNum(Hall, TEXT("breath"), 1200.f) * 0.7f, 1, Boss, TileNum(Hall, TEXT("breathCone"), 30.f), Facing.Vector()); return true; }
		if (Id == TEXT("rekindle"))
		{
			// Rekindle: every ice flag within 12 m runs to lava.
			for (int32 i = 0; i < H->Tiles.Num(); ++i) if (H->Tiles[i] == 2 && FVector::Dist2D(H->TileCentre(i), Floor) < TileNum(Hall, TEXT("transition"), 1200.f)) H->FlipTile(i, 1, Boss);
			if (H->GetNetMode() != NM_DedicatedServer) H->ForceNetUpdate();
			return true;
		}
		return Id == TEXT("dual_hammer") || Id == TEXT("late_hammer") || Id == TEXT("hammer_combo");
	}
	if (Boss->CreatureId == TEXT("burnt_wyrm"))
	{
		// Frost freezes: the sweep's ring, the breath's cone, the dive's landing, the air breath's strip.
		if (Id == TEXT("tail_sweep") || Id == TEXT("late_tail")) { H->Flip(Floor, TileNum(Hall, TEXT("sweep"), 650.f), 2, Boss); return true; }
		if (Id == TEXT("frost_breath") || Id == TEXT("freezing_breath"))
		{
			H->Flip(Floor, TileNum(Hall, TEXT("breath"), 1200.f), 2, Boss, TileNum(Hall, TEXT("breathCone"), 30.f), Facing.Vector());
			// The breath hardens the Smith's seams if it caught him.
			for (const TWeakObjectPtr<AWRBoss>& B : H->Champions)
			{
				if (!B.IsValid() || B.Get() == Boss || !B->IsAlive()) continue;
				const FVector To = B->GetActorLocation() - Boss->GetActorLocation();
				if (To.Size2D() < TileNum(Hall, TEXT("breath"), 1200.f) && FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(To.GetSafeNormal2D(), Facing.Vector().GetSafeNormal2D()))) < 35.f)
					if (UWRChampionBrain* Br = B->FindComponentByClass<UWRChampionBrain>()) Br->HardenUntil = Boss->GetWorld()->GetTimeSeconds() + TileNum(Hall, TEXT("hardenSeconds"), 6.f);
			}
			return true;
		}
		if (Id == TEXT("air_breath")) { H->Flip(At, TileNum(Hall, TEXT("airBreath"), 450.f), 2, Boss); return true; }
		if (Id == TEXT("dive")) { H->Flip(At, Size, 2, Boss); return false; }   // the dive still lands (the boss grounds itself)
		if (Id == TEXT("refreeze"))
		{
			for (int32 i = 0; i < H->Tiles.Num(); ++i) if (H->Tiles[i] == 1 && FVector::Dist2D(H->TileCentre(i), Floor) < TileNum(Hall, TEXT("transition"), 1200.f)) H->FlipTile(i, 2, Boss);
			if (H->GetNetMode() != NM_DedicatedServer) H->ForceNetUpdate();
			return true;
		}
		return false;
	}
	return false;
}

void WRWarHall::OnChampionDied(AWRBoss* Boss)
{
	AWRWarHall* H = HallOf(Boss);
	if (!H) return;
	UWorld* World = Boss->GetWorld();
	const FName Raid = H->HallId;
	// The raid's extra loot per looter of this champion (the 10% rule): a raid cosmetic now and then, the Heron's draught.
	const float Need = Boss->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	const TSharedPtr<FJsonObject> Loot = DChild(Raids(), TEXT("loot"));
	TArray<FName> Pool;
	for (const TCHAR* List : { TEXT("dyes"), TEXT("marks") })
	{
		if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(WRDeeds::Cosmetics(), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const TSharedPtr<FJsonObject> C = V->AsObject();
				if (DStr(C, TEXT("raid")) != Raid.ToString() || DNum(C, TEXT("chance"), 0.f) <= 0.f) continue;
				const FString Champ = DStr(C, TEXT("champion"));
				if (!Champ.IsEmpty() && Champ != Boss->CreatureId.ToString()) continue;
				Pool.Add(FName(*DStr(C, TEXT("id"))));
			}
		}
	}
	for (const TPair<FString, float>& L : Boss->Ledger)
	{
		AWRPlayerState* PS = DStateNamed(World, L.Key);
		if (!PS || L.Value < Need) continue;
		if (Pool.Num() > 0 && FMath::FRand() < DNum(Loot, TEXT("cosmeticChance"), 0.03f)) WRDeeds::GiveCosmetic(PS, Pool[FMath::RandRange(0, Pool.Num() - 1)], WRWarHall::Name(Raid));
		if (FMath::FRand() < DNum(Loot, TEXT("draughtChance"), 0.03f))
		{
			WRDeeds::GiveOrBag(PS, TEXT("heron_draught"), 1, Boss->GetActorLocation());   // a full bag: it waits on the floor, theirs only
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(TEXT("Among the champion's things: a Heron's draught"));
		}
	}
	// Its fighters go into the fight's ledger now (a fallen champion's body is cleared away before its rival falls).
	for (const TPair<FString, float>& L : Boss->Ledger) H->RaidLedger.FindOrAdd(L.Key) += L.Value;
	H->RaidSize = FMath::Max(H->RaidSize, Boss->Participants);
	// Both down: the raid is won. Its board times the whole fight; the group's raid deeds.
	bool bBothDead = true;
	for (const TWeakObjectPtr<AWRBoss>& B : H->Champions) bBothDead &= !B.IsValid() || B.Get() == Boss || !B->IsAlive();
	if (!bBothDead) return;
	const double Now = World->GetTimeSeconds();
	const TMap<FString, float> Damage = H->RaidLedger;
	const int32 Size = FMath::Max(1, H->RaidSize);
	TArray<FString> Names;
	FString Top;
	float TopDmg = -1.f;
	for (const TPair<FString, float>& D : Damage)
	{
		if (D.Value <= 0.f) continue;
		Names.Add(D.Key);
		if (D.Value > TopDmg) { TopDmg = D.Value; Top = D.Key; }
	}
	AWRPlayerState* TopPS = DStateNamed(World, Top);
	const int32 AL = TopPS ? WRArtifacts::PartyAL(TopPS) : 0;
	if (H->FightStart > 0.0 && Names.Num() > 0) WRRecords::Note(World, Raid, Names, Size, float(Now - H->FightStart), AL);
	const AWRWarHallGate* G = H->Gate.Get();
	TArray<FString> Group = G ? G->Group : Names;
	for (const FString& N : Names) Group.AddUnique(N);
	for (const FString& N : Group) if (AWRPlayerState* PS = DStateNamed(World, N)) WRDeeds::OnRaidWon(PS, Raid, !H->bTriumphSeen, AL);
	H->Won();
}

// ================================================================================================ players

FString WRWarHall::StateLine(const AWRWarHallGate* Gate)
{
	if (!Gate) return FString();
	const float Since = float(DNow(Gate)) - Gate->StateSince;
	switch (Gate->State)
	{
	case EWRHallState::Ready: return TEXT("The champions wait. The door is unbarred.");
	case EWRHallState::Open: return FString::Printf(TEXT("%s's group is going in (%d inside). Their party may follow until the grate falls."), *Gate->Leader, Gate->Inside);
	case EWRHallState::Sealed: return FString::Printf(TEXT("Sealed: a group is inside  ·  %s"), *WRDeeds::Clock(Since));
	case EWRHallState::Won: return TEXT("Won. The hall empties soon.");
	case EWRHallState::Resting:
	{
		const double Left = FMath::Max(0.0, Gate->RestUntil - double(FDateTime::UtcNow().ToUnixTimestamp()));
		const int32 Hrs = int32(Left / 3600.0), Min = int32(FMath::Fmod(Left, 3600.0) / 60.0);
		return FString::Printf(TEXT("The champions lie still. They rise again in %d h %d min."), Hrs, Min);
	}
	}
	return FString();
}

FString WRWarHall::EnterBlock(const AWRPlayerController* PC, const AWRWarHallGate* Gate)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !Gate || !PC->PlayerState) return TEXT("Not now");
	const FString Name = PC->PlayerState->GetPlayerName();
	if (FVector::Dist(Me->GetActorLocation(), Gate->GetActorLocation()) > Frame(TEXT("gateRange"), 450.f) + 400.f) return TEXT("Too far");
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(Me))
	{
		if (!C->IsAlive()) return TEXT("Not now");
		if (C->HasTag(WRTags::State_Mounted)) return TEXT("Get down from your mount");
	}
	switch (Gate->State)
	{
	case EWRHallState::Resting: return TEXT("The champions are resting");
	case EWRHallState::Won: return TEXT("The hall is emptying");
	case EWRHallState::Sealed: if (!Gate->Group.Contains(Name)) return TEXT("Sealed: a group is inside"); break;
	case EWRHallState::Open: if (!Gate->Leader.IsEmpty() && Gate->Leader != Name && !WRArtifacts::InParty(Gate->GetWorld(), Gate->Leader, Name)) return FString::Printf(TEXT("%s's group is going in: only their party"), *Gate->Leader); break;
	default: break;
	}
	const int32 Cap = FWRRaiding::RaidGroupMax();
	if (Cap > 0 && Gate->State != EWRHallState::Sealed && Gate->Inside >= Cap) return FString::Printf(TEXT("The hall takes %d: it's full"), Cap);
	return FString();
}

FString WRWarHall::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PS || !Pawn || Args.Num() < 2) return FString();
	UWorld* World = PC->GetWorld();
	if (Args[1] == TEXT("enter"))
	{
		AWRWarHallGate* Gate = GateNamed(World, Args.IsValidIndex(2) ? FName(*Args[2]) : NAME_None);
		AWRWarHall* Hall = Gate ? HallNamed(World, Gate->HallId) : nullptr;
		if (!Hall) return TEXT("Not now");
		const FString Why = EnterBlock(PC, Gate);
		if (!Why.IsEmpty()) return Why;
		const FString Who = PS->GetPlayerName();
		if (Gate->State == EWRHallState::Ready) { Gate->Leader = Who; Hall->SetState(EWRHallState::Open); }
		// Your following tames come in with you (they count for the champions' health, as for any boss).
		const FVector From = Pawn->GetActorLocation();
		const bool bSealed = Gate->State == EWRHallState::Sealed;
		const FVector In = bSealed ? Hall->ReentryPoint() : Hall->EntryPoint();
		Put(Pawn, In, 90.f);
		int32 n = 0;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (It->IsAlive() && It->TamedBy == Who && !It->Rider.IsValid() && It->TameOrder == EWRTameOrder::Follow && FVector::Dist(It->GetActorLocation(), From) < 1500.f)
				Put(*It, In + FVector(-300.f + 200.f * (n++ % 4), -250.f, 0.f), 90.f);
		}
		// You wake at the gate's shrine-stone if you fall.
		if (AWRRunbackStone* Stone = Hall->Runback.Get()) { PS->BoundStones.AddUnique(Stone); PS->RespawnPoint = Stone; }
		WRSound::Broadcast(Pawn, TEXT("warhall.door"));
		return bSealed ? TEXT("Back into the hall") : FString::Printf(TEXT("%s: cross the bronze line and the grate falls"), *Name(Gate->HallId));
	}
	if (Args[1] == TEXT("leave"))
	{
		AWRWarHall* Hall = HallAt(PC, Pawn->GetActorLocation());
		AWRWarHallGate* Gate = Hall ? Hall->Gate.Get() : nullptr;
		if (!Hall || !Gate) return TEXT("Not in a war-hall");
		if (!Hall->InVestibule(Pawn->GetActorLocation()) && Gate->State != EWRHallState::Won) return TEXT("The grate is down: the way out is behind it");
		Put(Pawn, Gate->FrontPoint(), Gate->GetActorRotation().Yaw);
		return TEXT("Out under the sky");
	}
	return FString();
}

namespace WRHallImpl
{
	/** A record tablet or a monument within reach (the endgame's other things you read). */
	AWRRecordTablet* TabletNear(const UWorld* World, const FVector& At)
	{
		for (TActorIterator<AWRRecordTablet> It(World); It; ++It) if (FVector::Dist(It->GetActorLocation(), At) < WRTune(TEXT("records.tabletReach"), 320.f)) return *It;
		return nullptr;
	}
	AWRBuildPiece* MonumentNear(const UWorld* World, const FVector& At)
	{
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* D = It->GetDef();
			if (D && !D->Monument.IsNone() && !It->IsGhost() && FVector::Dist(It->GetActorLocation(), At) < WRTune(TEXT("records.tabletReach"), 320.f)) return *It;
		}
		return nullptr;
	}
}

bool WRWarHall::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	for (TActorIterator<AWRWarHallGate> It(PC->GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) > Frame(TEXT("gateRange"), 450.f) + 250.f) continue;
		OutTarget = Name(It->HallId);
		OutVerb = TEXT("Look at the door");
		return true;
	}
	if (const AWRWarHall* H = HallAt(PC, Me->GetActorLocation()))
	{
		if (FVector::Dist2D(H->ExitArch(), Me->GetActorLocation()) < 450.f)
		{
			OutTarget = Name(H->HallId);
			OutVerb = TEXT("Leave by the stair");
			return true;
		}
	}
	if (const AWRRecordTablet* T = WRHallImpl::TabletNear(PC->GetWorld(), Me->GetActorLocation()))
	{
		OutTarget = FString::Printf(TEXT("%s: kill times"), *WRRecords::BoardName(T->BoardId));
		OutVerb = TEXT("Read the board");
		return true;
	}
	if (const AWRBuildPiece* M = WRHallImpl::MonumentNear(PC->GetWorld(), Me->GetActorLocation()))
	{
		OutTarget = M->GetDef()->Name;
		OutVerb = TEXT("Read the plaque");
		return true;
	}
	return false;
}

bool WRWarHall::ClientInteract(AWRPlayerController* PC)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !PC->GetUI()) return false;
	for (TActorIterator<AWRWarHallGate> It(PC->GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) > Frame(TEXT("gateRange"), 450.f) + 250.f) continue;
		PC->ClientOpenWorldScreen_Implementation(TEXT("warhall"), *It);
		return true;
	}
	if (const AWRWarHall* H = HallAt(PC, Me->GetActorLocation()))
	{
		if (FVector::Dist2D(H->ExitArch(), Me->GetActorLocation()) < 450.f)
		{
			PC->SendCommand(TEXT("warhall leave"));
			return true;
		}
	}
	if (AWRRecordTablet* T = WRHallImpl::TabletNear(PC->GetWorld(), Me->GetActorLocation()))
	{
		// The board in full: the server sends it, the screen shows it as it comes.
		PC->SendCommand(FString::Printf(TEXT("deeds records %s"), *T->BoardId.ToString()));
		PC->ClientOpenWorldScreen_Implementation(TEXT("records"), T);
		return true;
	}
	if (AWRBuildPiece* M = WRHallImpl::MonumentNear(PC->GetWorld(), Me->GetActorLocation()))
	{
		PC->ClientOpenWorldScreen_Implementation(TEXT("plaque"), M);
		return true;
	}
	return false;
}
