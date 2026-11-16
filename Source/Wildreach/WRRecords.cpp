// Kill-time boards and their stone tablets (cooks/build-endgame.md 2.5; biome-void 2.23.5). The world document
// "records": { seasons: { "<n>": { boards: { <id>: { kills, first {names, at}, bands { "<band>": [best 10],
// "<band>.al": [best 10] } } } } } }, each entry { names, size, secs, al, at }. A tablet is a cut granite stele with a
// rounded head, the board's sigil carved in its head, its name and the three fastest cut into a sunk panel; a new
// record regrinds the letters with a stone grind.
#include "WRRecords.h"
#include "WRGod.h"
#include "WRDeeds.h"
#include "WRDeedsInternal.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCreature.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStore.h"
#include "WRTuning.h"
#include "WRWarHall.h"
#include "WRWorldSystems.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Misc/DateTime.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

using namespace WRDeedsImpl;
using WRLookKit::FSurf;
using WRLookKit::FKit;

namespace WRRecordsImpl
{
	TSharedPtr<FJsonObject>& Doc() { static TSharedPtr<FJsonObject> D; return D; }
	TMap<FName, TSharedPtr<FJsonObject>>& ClientCache() { static TMap<FName, TSharedPtr<FJsonObject>> C; return C; }
	FTimerHandle TabletTimer;

	int32 Season() { return FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("server.recordsSeason"), 1.f))); }
	TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		TSharedPtr<FJsonObject> C = DChild(O, Key);
		if (!C.IsValid() && O.IsValid()) { C = MakeShared<FJsonObject>(); O->SetObjectField(Key, C); }
		return C;
	}
	TSharedPtr<FJsonObject> SeasonObj(int32 S) { return Child(Child(Doc(), TEXT("seasons")), FString::FromInt(S)); }
	TSharedPtr<FJsonObject> BoardObj(int32 S, FName Board) { return Child(Child(SeasonObj(S), TEXT("boards")), Board.ToString()); }
	FString Write(const TSharedPtr<FJsonObject>& O)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(O.ToSharedRef(), W);
		return Out;
	}
	void Save() { if (Doc().IsValid()) WRStore::WriteDoc(TEXT("records"), Write(Doc())); }
	TArray<FString> NamesOf(const TSharedPtr<FJsonObject>& Entry)
	{
		TArray<FString> Out;
		if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(Entry, TEXT("names"))) for (const TSharedPtr<FJsonValue>& V : *A) Out.Add(V->AsString());
		return Out;
	}
	/** Every entry of a board, all bands and both columns. */
	TArray<TSharedPtr<FJsonObject>> AllEntries(const TSharedPtr<FJsonObject>& Board)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		const TSharedPtr<FJsonObject> Bands = DChild(Board, TEXT("bands"));
		if (Bands.IsValid()) for (const auto& B : Bands->Values) { const TArray<TSharedPtr<FJsonValue>>* L = nullptr; if (B.Value->TryGetArray(L)) for (const TSharedPtr<FJsonValue>& V : *L) Out.Add(V->AsObject()); }
		return Out;
	}
	FString LineOf(const TSharedPtr<FJsonObject>& E)
	{
		const int32 AL = FMath::RoundToInt(DNum(E, TEXT("al"), 0.f));
		const int32 Size = FMath::RoundToInt(DNum(E, TEXT("size"), 1.f));
		return FString::Printf(TEXT("%s  %s%s%s"), *WRDeeds::Clock(DNum(E, TEXT("secs"), 0.f)), *FString::Join(NamesOf(E), TEXT(", ")),
			Size > 1 ? *FString::Printf(TEXT("  (%d)"), Size) : TEXT(""), AL > 0 ? *FString::Printf(TEXT("  AL %d"), AL) : TEXT(""));
	}
	FString TabletLines(FName Board)
	{
		TArray<TSharedPtr<FJsonObject>> All = AllEntries(DChild(DChild(SeasonObj(Season()), TEXT("boards")), Board.ToString()));
		All.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B) { return DNum(A, TEXT("secs"), 1e9f) < DNum(B, TEXT("secs"), 1e9f); });
		TArray<FString> Lines;
		for (int32 i = 0; i < All.Num() && i < 3; ++i) Lines.Add(FString::Printf(TEXT("%d   %s"), i + 1, *LineOf(All[i])));
		return Lines.Num() > 0 ? FString::Join(Lines, TEXT("\n")) : FString(TEXT("No kills yet"));
	}
	void RefreshTablets(UWorld* World, FName Board)
	{
		for (TActorIterator<AWRRecordTablet> It(World); It; ++It)
		{
			if (It->BoardId != Board) continue;
			It->Title = WRRecords::BoardName(Board);
			It->Lines = TabletLines(Board);
			if (World->GetNetMode() != NM_DedicatedServer) It->ForceNetUpdate();
		}
	}
	void RefreshMonuments(UWorld* World, FName Boss)
	{
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* Def = It->GetDef();
			if (!Def || Def->Monument != Boss || It->IsGhost()) continue;
			const FString Line = WRRecords::PlaqueText(World, Boss, It->OwnerName);
			if (Line != It->CustomName) { It->CustomName = Line; It->OnRep_CustomName(); }
		}
	}
	bool Ground(UWorld* World, const FVector& Near, FVector& Out)
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTablet), false);
		if (!World->LineTraceSingleByChannel(H, Near + FVector(0, 0, 2000.f), Near - FVector(0, 0, 4000.f), ECC_WorldStatic, Q)) return false;
		Out = H.ImpactPoint;
		return true;
	}
	void PlaceTablets(UWorld* World)
	{
		TSet<FName> Have;
		for (TActorIterator<AWRRecordTablet> It(World); It; ++It) Have.Add(It->BoardId);
		// A great boss's beside its shrine-stone (to its left, the summoning stone on its right).
		for (TActorIterator<AWRRunbackStone> It(World); It; ++It)
		{
			if (It->BossId.IsNone() || Have.Contains(It->BossId)) continue;
			FVector At;
			if (!Ground(World, It->GetActorLocation() - It->GetActorRightVector() * WRTune(TEXT("records.sideCm"), 600.f), At)) continue;
			WRRecords::PlaceTablet(World, It->BossId, At, It->GetActorRotation());
			Have.Add(It->BossId);
		}
		// A roaming mini's beside its summoning stone at its lair.
		for (TActorIterator<AWRSummoningStone> It(World); It; ++It)
		{
			if (It->BossId.IsNone() || Have.Contains(It->BossId)) continue;
			FVector At;
			if (!Ground(World, It->GetActorLocation() + It->GetActorRightVector() * WRTune(TEXT("records.miniSideCm"), 350.f), At)) continue;
			WRRecords::PlaceTablet(World, It->BossId, At, It->GetActorRotation());
			Have.Add(It->BossId);
		}
	}
}
using namespace WRRecordsImpl;

// ================================================================================================ the boards

TArray<FString> WRRecords::Bands()
{
	TArray<FString> Out;
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(FWRTuning::Get().Object(TEXT("records")), TEXT("bands")))
		for (const TSharedPtr<FJsonValue>& V : *L) Out.Add(DStr(V->AsObject(), TEXT("id")));
	if (Out.Num() == 0) Out = { TEXT("solo"), TEXT("2"), TEXT("3-4"), TEXT("5-7"), TEXT("8-10"), TEXT("11+") };
	return Out;
}

FString WRRecords::BandOf(int32 Size)
{
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(FWRTuning::Get().Object(TEXT("records")), TEXT("bands")))
		for (const TSharedPtr<FJsonValue>& V : *L) if (Size <= FMath::RoundToInt(DNum(V->AsObject(), TEXT("max"), 999.f))) return DStr(V->AsObject(), TEXT("id"));
	return Size <= 1 ? TEXT("solo") : Size == 2 ? TEXT("2") : Size <= 4 ? TEXT("3-4") : Size <= 7 ? TEXT("5-7") : Size <= 10 ? TEXT("8-10") : TEXT("11+");
}

FString WRRecords::BandName(const FString& Band)
{
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(FWRTuning::Get().Object(TEXT("records")), TEXT("bands")))
		for (const TSharedPtr<FJsonValue>& V : *L) if (DStr(V->AsObject(), TEXT("id")) == Band) return DStr(V->AsObject(), TEXT("name"), Band);
	return Band;
}

FString WRRecords::BoardName(FName Board)
{
	if (const FWRCreatureDef* C = FWRCreatureRegistry::Get().Find(Board))
	{
		FString Short = C->Name;
		Short.Split(TEXT(","), &Short, nullptr);
		return Short;
	}
	const FString Raid = WRWarHall::Name(Board);
	return Raid.IsEmpty() ? Board.ToString() : Raid;
}

void WRRecords::Start(UWorld* World)
{
	FString Text;
	Doc() = MakeShared<FJsonObject>();
	if (WRStore::ReadDoc(TEXT("records"), Text))
	{
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
		if (FJsonSerializer::Deserialize(R, Parsed) && Parsed.IsValid()) Doc() = Parsed;
		else UE_LOG(LogWildreach, Error, TEXT("RECORDS: the records document didn't parse; starting empty boards (the old text stays in the store's backups)"));
	}
	UE_LOG(LogWildreach, Log, TEXT("RECORDS season %d"), Season());
	World->GetTimerManager().SetTimer(TabletTimer, FTimerDelegate::CreateWeakLambda(World, [World]() { PlaceTablets(World); }), 5.f, true, 3.f);
}

void WRRecords::Note(UWorld* World, FName Board, const TArray<FString>& Names, int32 Size, float Seconds, int32 AL)
{
	if (!World || Board.IsNone() || Names.Num() == 0 || Seconds <= 0.f) return;
	// A kill a test character took part in (the testers' god mode, WRGod) goes on no board.
	for (const FString& N : Names)
	{
		if (WRGod::IsTestedName(World, N))
		{
			UE_LOG(LogWildreach, Log, TEXT("RECORDS %s kill not noted: %s is a test character"), *Board.ToString(), *N);
			return;
		}
	}
	if (!Doc().IsValid()) Doc() = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> B = BoardObj(Season(), Board);
	B->SetNumberField(TEXT("kills"), DNum(B, TEXT("kills"), 0.f) + 1.f);
	const int64 Unix = FDateTime::UtcNow().ToUnixTimestamp();
	TArray<TSharedPtr<FJsonValue>> NameVals;
	for (const FString& N : Names) NameVals.Add(MakeShared<FJsonValueString>(N));
	if (!B->HasField(TEXT("first")))
	{
		// Who first killed it on this world: shown on its tablet's board only, never announced (DD).
		TSharedRef<FJsonObject> F = MakeShared<FJsonObject>();
		F->SetArrayField(TEXT("names"), NameVals);
		F->SetNumberField(TEXT("at"), double(Unix));
		B->SetObjectField(TEXT("first"), F);
	}
	TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
	E->SetArrayField(TEXT("names"), NameVals);
	E->SetNumberField(TEXT("size"), Size);
	E->SetNumberField(TEXT("secs"), FMath::RoundToFloat(Seconds * 10.f) / 10.f);
	E->SetNumberField(TEXT("al"), AL);
	E->SetNumberField(TEXT("at"), double(Unix));
	const FString Key = BandOf(Size) + (AL > 0 ? TEXT(".al") : TEXT(""));
	const TSharedPtr<FJsonObject> Bands = Child(B, TEXT("bands"));
	TArray<TSharedPtr<FJsonValue>> List;
	if (const TArray<TSharedPtr<FJsonValue>>* Old = DArr(Bands, Key)) List = *Old;
	List.Add(MakeShared<FJsonValueObject>(E));
	List.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& Bx) { return DNum(A->AsObject(), TEXT("secs"), 1e9f) < DNum(Bx->AsObject(), TEXT("secs"), 1e9f); });
	const int32 Keep = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("records.keep"), 10.f)));
	const bool bOnBoard = List.IndexOfByPredicate([&E](const TSharedPtr<FJsonValue>& V) { return V->AsObject() == E; }) < Keep;
	if (List.Num() > Keep) List.SetNum(Keep);
	Bands->SetArrayField(Key, List);
	Save();
	UE_LOG(LogWildreach, Log, TEXT("RECORDS %s: %s in %s by %s (size %d, AL %d)%s"), *Board.ToString(), *WRDeeds::Clock(Seconds), *Key, *FString::Join(Names, TEXT(", ")), Size, AL, bOnBoard ? TEXT(" - on the board") : TEXT(""));
	RefreshTablets(World, Board);
	RefreshMonuments(World, Board);
	if (bOnBoard)
	{
		for (const FString& N : Names)
		{
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(DStateNamed(World, N) ? DStateNamed(World, N)->GetOwner() : nullptr))
				PC->ClientNotify(FString::Printf(TEXT("%s: %s, on the board (%s)"), *BoardName(Board), *WRDeeds::Clock(Seconds), *BandName(BandOf(Size))));
		}
	}
}

void WRRecords::SendBoards(AWRPlayerController* PC, FName Board, int32 InSeason)
{
	if (!PC) return;
	const int32 S = InSeason > 0 ? InSeason : Season();
	const FString Me = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetNumberField(TEXT("season"), S);
	Out->SetNumberField(TEXT("current"), Season());
	const TSharedPtr<FJsonObject> Boards = DChild(DChild(DChild(Doc(), TEXT("seasons")), FString::FromInt(S)), TEXT("boards"));
	if (Board.IsNone())
	{
		// The summary: every board with kills, each band's best, and your own best.
		TArray<TSharedPtr<FJsonValue>> List;
		if (Boards.IsValid())
		{
			for (const auto& P : Boards->Values)
			{
				const TSharedPtr<FJsonObject> B = P.Value->AsObject();
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetStringField(TEXT("id"), P.Key);
				Row->SetStringField(TEXT("name"), BoardName(FName(*P.Key)));
				Row->SetNumberField(TEXT("kills"), DNum(B, TEXT("kills"), 0.f));
				if (const TSharedPtr<FJsonObject> F = DChild(B, TEXT("first"))) Row->SetObjectField(TEXT("first"), F);
				TArray<TSharedPtr<FJsonObject>> All = AllEntries(B);
				All.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& Bx) { return DNum(A, TEXT("secs"), 1e9f) < DNum(Bx, TEXT("secs"), 1e9f); });
				if (All.Num() > 0) Row->SetObjectField(TEXT("best"), All[0]);
				for (const TSharedPtr<FJsonObject>& E : All) if (NamesOf(E).Contains(Me)) { Row->SetObjectField(TEXT("mine"), E); break; }
				List.Add(MakeShared<FJsonValueObject>(Row));
			}
		}
		Out->SetArrayField(TEXT("boards"), List);
	}
	else
	{
		const TSharedPtr<FJsonObject> B = DChild(Boards, Board.ToString());
		Out->SetStringField(TEXT("id"), Board.ToString());
		Out->SetStringField(TEXT("name"), BoardName(Board));
		if (B.IsValid()) Out->SetObjectField(TEXT("board"), B);
	}
	PC->ClientRecords(Board, Write(Out));
}

TSharedPtr<FJsonObject> WRRecords::ClientBoards(FName Board) { return ClientCache().FindRef(Board); }

void WRRecords::ClientReceive(FName Board, const FString& Json)
{
	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(R, Parsed) && Parsed.IsValid()) ClientCache().Add(Board, Parsed);
}

FString WRRecords::PlaqueText(UWorld* World, FName Boss, const FString& Owner, int32* OutBestAL)
{
	// The owner's (or a clanmate's) fastest kill of this boss in any season, and the highest AL they've killed it at.
	TSharedPtr<FJsonObject> Best;
	int32 BestAL = 0;
	const TSharedPtr<FJsonObject> Seasons = DChild(Doc(), TEXT("seasons"));
	if (Seasons.IsValid())
	{
		for (const auto& S : Seasons->Values)
		{
			const TSharedPtr<FJsonObject> B = DChild(DChild(S.Value->AsObject(), TEXT("boards")), Boss.ToString());
			for (const TSharedPtr<FJsonObject>& E : AllEntries(B))
			{
				bool bOurs = false;
				for (const FString& N : NamesOf(E)) bOurs |= N == Owner || FWRSocialRules::CanShare(Owner, N);
				if (!bOurs) continue;
				BestAL = FMath::Max(BestAL, FMath::RoundToInt(DNum(E, TEXT("al"), 0.f)));
				if (!Best.IsValid() || DNum(E, TEXT("secs"), 1e9f) < DNum(Best, TEXT("secs"), 1e9f)) Best = E;
			}
		}
	}
	if (OutBestAL) *OutBestAL = BestAL;
	if (!Best.IsValid()) return FString();
	return FString::Printf(TEXT("%s|%s|%d"), *WRDeeds::Clock(DNum(Best, TEXT("secs"), 0.f)), *FString::Join(NamesOf(Best), TEXT(", ")), BestAL);
}

void WRRecords::RefreshBanners(UWorld* World, const FString& Owner)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(DStateNamed(World, Owner));
	if (!D) return;
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		if (It->PieceId != TEXT("banner") || It->OwnerName != Owner || It->IsGhost() || It->Emblem == D->BannerSigil) continue;
		It->Emblem = D->BannerSigil;
		if (World->GetNetMode() != NM_DedicatedServer) It->OnRep_Emblem();
	}
}

AWRRecordTablet* WRRecords::PlaceTablet(UWorld* World, FName Board, const FVector& At, const FRotator& Facing)
{
	if (!World) return nullptr;
	AWRRecordTablet* T = World->SpawnActorDeferred<AWRRecordTablet>(AWRRecordTablet::StaticClass(), FTransform(FRotator(0.f, Facing.Yaw, 0.f), At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!T) return nullptr;
	T->BoardId = Board;
	T->Title = BoardName(Board);
	T->Lines = TabletLines(Board);
	T->FinishSpawning(FTransform(FRotator(0.f, Facing.Yaw, 0.f), At));
	UE_LOG(LogWildreach, Log, TEXT("RECORDS tablet for %s at %s"), *Board.ToString(), *At.ToCompactString());
	return T;
}

// ================================================================================================ the tablet

AWRRecordTablet::AWRRecordTablet()
{
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(30000.f));
}

void AWRRecordTablet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRRecordTablet, BoardId);
	DOREPLIFETIME(AWRRecordTablet, Title);
	DOREPLIFETIME(AWRRecordTablet, Lines);
}

void AWRRecordTablet::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	OnRep_Lines();
}

void AWRRecordTablet::BuildLook()
{
	if (bBuilt) return;
	bBuilt = true;
	// Collision on every machine: the stele and its footing (you can't walk through it).
	UBoxComponent* Box = NewObject<UBoxComponent>(this);
	Box->SetupAttachment(RootComponent);
	Box->SetBoxExtent(FVector(20.f, 52.f, 105.f));
	Box->SetRelativeLocation(FVector(0.f, 0.f, 105.f));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	if (GetNetMode() == NM_DedicatedServer) return;

	// A cut grey granite stele (runestones like Jelling's, dressed flat on the face): a two-step footing sunk into the
	// ground, the slab with a round head, a sunk panel for the names, a raised border, and the board's sigil cut in the
	// head. Its face (+X) looks down the approach.
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Granite = FSurf::Stone(FLinearColor(0.16f, 0.16f, 0.17f), 0.82f);
	const FSurf Cut = FSurf::Stone(FLinearColor(0.1f, 0.1f, 0.11f), 0.9f);
	const FSurf Moss = FSurf::Stone(FLinearColor(0.05f, 0.08f, 0.03f), 0.97f);
	K.Box(Granite.Dim(0.85f), FVector(-34.f, -66.f, -60.f), FVector(34.f, 66.f, 10.f));          // footing, sunk 60 cm
	K.Box(Granite.Dim(0.92f), FVector(-26.f, -58.f, 10.f), FVector(26.f, 58.f, 22.f));           // second step
	K.Box(Granite, FVector(-17.f, -48.f, 22.f), FVector(17.f, 48.f, 168.f));                     // the slab
	K.Add(Granite, FVector(96.f, 96.f, 34.f), FVector(0.f, 0.f, 168.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);   // the round head
	K.Box(Cut, FVector(15.f, -40.f, 40.f), FVector(17.6f, 40.f, 140.f));                         // the sunk panel
	K.Box(Granite.Dim(1.08f), FVector(16.f, -44.f, 140.f), FVector(18.6f, 44.f, 144.f));          // border over it
	K.Box(Granite.Dim(1.08f), FVector(16.f, -44.f, 36.f), FVector(18.6f, 44.f, 40.f));            // border under it
	for (const float Y : { -44.f, 40.f }) K.Box(Granite.Dim(1.08f), FVector(16.f, Y, 36.f), FVector(18.6f, Y + 4.f, 144.f));
	// Moss at the foot on the shaded side, and lichen spots.
	K.Add(Moss, FVector(30.f, 70.f, 5.f), FVector(-20.f, 0.f, 22.f), FRotator(0.f, 0.f, 0.f));
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector C(-17.5f, -40.f + 15.f * i + 6.f * K.Jit(FVector(i, 0, 0), 3), 50.f + 90.f * K.Rand(FVector(i, 1, 0), 5));
		K.Add(FSurf::Stone(FLinearColor(0.35f, 0.37f, 0.26f), 0.98f), FVector(1.f, 6.f + 5.f * K.Rand(C, 7), 5.f + 4.f * K.Rand(C, 9)), C);
	}
	// The sigil cut into the head: each cell a shallow sunk square (dark), 5 cm on the 9 x 9 grid.
	const TArray<FString> Sigil = WRDeeds::Sigil(BoardId);
	const float Cell = 5.f;
	for (int32 r = 0; r < Sigil.Num(); ++r)
	{
		for (int32 c = 0; c < Sigil[r].Len(); ++c)
		{
			if (Sigil[r][c] != TCHAR('#')) continue;
			K.Add(Cut.Dim(0.7f), FVector(1.2f, Cell - 0.6f, Cell - 0.6f), FVector(17.4f, (c - 4) * Cell, 186.f - (r - 4) * Cell));
		}
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	// The letters: cut and chalked so they read (pale on the dark panel).
	const FColor Chalk(228, 222, 206);
	const auto Line = [this, Chalk](float Z, float Size)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(RootComponent);
		T->SetRelativeLocation(FVector(17.9f, 0.f, Z));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextTop);
		T->SetWorldSize(Size);
		T->SetTextRenderColor(Chalk);
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->SetCastShadow(false);
		T->RegisterComponent();
		Text.Add(T);
	};
	Line(136.f, 9.f);    // the board's name
	Line(118.f, 4.2f);   // the three best
}

void AWRRecordTablet::OnRep_Lines()
{
	if (GetNetMode() == NM_DedicatedServer || !bBuilt || Text.Num() < 2) return;
	Text[0]->SetText(FText::FromString(Title.ToUpper()));
	Text[1]->SetText(FText::FromString(Lines.Replace(TEXT("\n"), TEXT("<br>"))));
	// A record falling regrinds the letters (not the first time it's drawn).
	if (bShownOnce) WRSound::PlayOn(this, TEXT("records.grind"));
	bShownOnce = true;
}
