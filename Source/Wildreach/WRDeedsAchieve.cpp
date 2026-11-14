// The achievement engine and the endgame's hooks (cooks/build-endgame.md 2.3, 2.6, 2.7): which of the 150 rows are live
// in this build, the checks every player gets every 2 s (landmarks stood at, passes crossed, hidden roads found, paths
// and skills to the top, days alive, artifact level, prestige marks, geis records), the census every 30 s (claims,
// Hearths, building sets, monuments, workshops, tames, machine chains, turret lines), the events that come once (a
// boss solo or deathless, a portal cleared, a raid won), and the boss's drops (souls, trophy heads, the Heron's draught).
#include "WRDeeds.h"
#include "WRDeedsInternal.h"
#include "Wildreach.h"
#include "WRArtifacts.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRBuildSets.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRMachine.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRRecords.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRSocial.h"
#include "WRAttributeSet.h"
#include "WRTuning.h"
#include "WRWarHall.h"
#include "WRWorldDressing.h"
#include "WRWorldSystems.h"
#include "EngineUtils.h"
#include "TimerManager.h"

using namespace WRDeedsImpl;

namespace WRDeedsAch
{
	struct FLandmark { FName Id; FVector At; FName Biome; };
	TArray<FLandmark>& Landmarks() { static TArray<FLandmark> L; return L; }
	TWeakObjectPtr<UWorld>& LandmarkWorld() { static TWeakObjectPtr<UWorld> W; return W; }
	/** The census: player -> stat -> value (server). */
	TMap<FString, TMap<FName, int32>>& Census() { static TMap<FString, TMap<FName, int32>> C; return C; }
	TMap<FName, bool>& LiveCache() { static TMap<FName, bool> C; return C; }
	FTimerHandle CensusTimer;
	FDelegateHandle RestHandle;

	/** Every landmark of the island (its POIs, shrines and villages), each with its land. */
	const TArray<FLandmark>& LandmarksOf(UWorld* World)
	{
		TArray<FLandmark>& L = Landmarks();
		if (LandmarkWorld().Get() == World && L.Num() > 0) return L;
		L.Reset();
		LandmarkWorld() = World;
		for (const FWRPoi& P : FWRPoi::All)
		{
			if (P.Kind == TEXT("boss") || P.Kind == TEXT("mini-boss") || P.Kind == TEXT("camp") || P.Kind == TEXT("cave"))
				L.Add({ FName(*P.Name), P.Location, WRBiome::At(World, P.Location) });
		}
		for (TActorIterator<AWRShrine> It(World); It; ++It) L.Add({ FName(*FString::Printf(TEXT("shrine.%s"), *It->ShrineId.ToString())), It->GetActorLocation(), It->Biome });
		for (TActorIterator<AWRVillage> It(World); It; ++It) L.Add({ FName(*FString::Printf(TEXT("village.%s"), *It->VillageId.ToString())), It->GetActorLocation(), It->Biome });
		return L;
	}
	int32 LandmarkCount(UWorld* World, FName Biome)
	{
		int32 N = 0;
		for (const FLandmark& L : LandmarksOf(World)) N += L.Biome == Biome;
		return N;
	}
	/** The wild species of a land (lowland, midland, highland; not the portals' own). */
	TArray<FName> SpeciesOf(FName Biome, bool bTameable)
	{
		TArray<FName> Out;
		for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All())
		{
			if (C.Biome != Biome || C.bPortalOnly || C.PortalRank > 0) continue;
			if (C.Tier != TEXT("lowland") && C.Tier != TEXT("midland") && C.Tier != TEXT("highland")) continue;
			if (bTameable && C.TamingMethods.Num() == 0) continue;
			Out.Add(C.Id);
		}
		return Out;
	}
	TSharedPtr<FJsonObject> Params(const TSharedPtr<FJsonObject>& Row) { return DChild(Row, TEXT("params")); }
	FName PName(const TSharedPtr<FJsonObject>& Row, const TCHAR* Key) { return FName(*DStr(Params(Row), Key)); }
	int32 PInt(const TSharedPtr<FJsonObject>& Row, const TCHAR* Key, int32 Fallback = 0) { return FMath::RoundToInt(DNum(Params(Row), Key, float(Fallback))); }
	int32 Today(const UObject* Ctx) { const AWRGameState* GS = Ctx && Ctx->GetWorld() ? Ctx->GetWorld()->GetGameState<AWRGameState>() : nullptr; return GS ? GS->GetDay() : 1; }

	void SetProgress(UWRDeedsComponent* D, FName Id, int32 Have, int32 Need)
	{
		const int32 i = D->ProgressIds.IndexOfByKey(Id);
		const FIntPoint P(FMath::Min(Have, Need), Need);
		if (i == INDEX_NONE) { D->ProgressIds.Add(Id); D->Progress.Add(P); }
		else if (D->Progress[i] != P) D->Progress[i] = P;
	}

	void Unlock(AWRPlayerState* PS, const TSharedPtr<FJsonObject>& Row)
	{
		UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
		const FName Id(*DStr(Row, TEXT("id")));
		if (!D || Id.IsNone() || D->Achieved.Contains(Id)) return;
		D->Achieved.Add(Id);
		D->bDirty = true;
		const FString Title = DStr(Row, TEXT("title"));
		if (!Title.IsEmpty()) WRDeeds::GiveTitle(PS, Title, DStr(Row, TEXT("name")));
		const FString Cos = DStr(Row, TEXT("cosmetic"));
		if (!Cos.IsEmpty()) WRDeeds::GiveCosmetic(PS, FName(*Cos), DStr(Row, TEXT("name")));
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientDeed(Id);
		UE_LOG(LogWildreach, Log, TEXT("DEEDS %s achieved %s (%s)"), *PS->GetPlayerName(), *Id.ToString(), *DStr(Row, TEXT("name")));
	}

	/** The rows that read state (checked every 2 s per player): true once met. Sets the progress shown. */
	bool Met(AWRPlayerState* PS, UWRDeedsComponent* D, const TSharedPtr<FJsonObject>& Row)
	{
		UWorld* World = PS->GetWorld();
		const FName Id(*DStr(Row, TEXT("id")));
		const FString Ev = DStr(Row, TEXT("event"));
		const UWRPathComponent* Paths = PS->GetPaths();
		if (Ev == TEXT("landmarks"))
		{
			const FName B = PName(Row, TEXT("biome"));
			int32 Have = 0;
			for (const FLandmark& L : LandmarksOf(World)) Have += L.Biome == B && D->Landmarks.Contains(L.Id);
			const int32 Need = LandmarkCount(World, B);
			SetProgress(D, Id, Have, Need);
			return Need > 0 && Have >= Need;
		}
		if (Ev == TEXT("hidden")) return D->HiddenFound.Contains(PName(Row, TEXT("biome")));
		if (Ev == TEXT("pass")) return D->PassesCrossed.Contains(PName(Row, TEXT("pass")));
		if (Ev == TEXT("huntall") || Ev == TEXT("tameall"))
		{
			const bool bTame = Ev == TEXT("tameall");
			const TArray<FName> All = SpeciesOf(PName(Row, TEXT("biome")), bTame);
			int32 Have = 0;
			for (const FName& S : All) Have += (bTame ? D->Tamed : D->Species).Contains(S);
			SetProgress(D, Id, Have, All.Num());
			return All.Num() > 0 && Have >= All.Num();
		}
		if (Ev == TEXT("belt"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Minis = DArr(Params(Row), TEXT("minis"));
			if (!Minis || Minis->Num() == 0) return false;
			int32 Have = 0;
			for (const TSharedPtr<FJsonValue>& V : *Minis) Have += D->Bosses.Contains(FName(*V->AsString()));
			SetProgress(D, Id, Have, Minis->Num());
			return Have >= Minis->Num();
		}
		if (Ev == TEXT("path100"))
		{
			const int32 Need = FMath::RoundToInt(WRTune(TEXT("prestige.atPoints"), 100.f));
			const int32 Have = Paths ? Paths->GetEarned(PName(Row, TEXT("path"))) : 0;
			SetProgress(D, Id, Have, Need);
			// A returned path was at 100 once: its marks say so.
			return Have >= Need || D->MarksOn(PName(Row, TEXT("path"))) != nullptr;
		}
		if (Ev == TEXT("combos"))
		{
			// Pairs of paths both at 25 or more (saved progress counts, DR #18).
			TArray<FName> At25;
			for (const FWRPathDef& P : FWRPathRegistry::Get().All()) if (Paths && Paths->GetSpent(P.Id) >= 25) At25.Add(P.Id);
			const int32 Pairs = At25.Num() * (At25.Num() - 1) / 2;
			SetProgress(D, Id, Pairs, PInt(Row, TEXT("count"), 10));
			return Pairs >= PInt(Row, TEXT("count"), 10);
		}
		if (Ev == TEXT("skill100"))
		{
			const UWRLifeSkillsComponent* S = PS->GetLifeSkills();
			const int32 Have = S ? S->GetLevel(PName(Row, TEXT("skill"))) : 0;
			SetProgress(D, Id, Have, 100);
			return Have >= 100;
		}
		if (Ev == TEXT("census"))
		{
			const TMap<FName, int32>* Mine = Census().Find(PS->GetPlayerName());
			const int32 Have = Mine ? Mine->FindRef(PName(Row, TEXT("stat"))) : 0;
			SetProgress(D, Id, Have, PInt(Row, TEXT("count"), 1));
			return Have >= PInt(Row, TEXT("count"), 1);
		}
		if (Ev == TEXT("days"))
		{
			const int32 Have = D->DeathDay >= 0 ? Today(PS) - D->DeathDay : 0;
			SetProgress(D, Id, Have, PInt(Row, TEXT("count"), 100));
			return Have >= PInt(Row, TEXT("count"), 100);
		}
		if (Ev == TEXT("artifactal")) return WRArtifacts::AL(PS) >= PInt(Row, TEXT("count"), 15);
		if (Ev == TEXT("geis"))
		{
			const UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
			int32 Best = 0;
			if (A) for (const FWRGeisRecord& R : A->Records) Best = FMath::Max(Best, int32(R.Best));
			return Best >= PInt(Row, TEXT("count"), 5);
		}
		if (Ev == TEXT("prestige") || Ev == TEXT("prestige3") || Ev == TEXT("prestigepaths") || Ev == TEXT("prestigemarks"))
		{
			int32 Sum = 0, Paths3 = 0, PathsAny = 0;
			for (const FWRPrestigeRep& R : D->Marks) { Sum += R.Total(); Paths3 += R.Total() >= 3; PathsAny += R.Total() > 0; }
			if (Ev == TEXT("prestige")) return Sum >= PInt(Row, TEXT("count"), 1);
			if (Ev == TEXT("prestige3")) return Paths3 > 0;
			if (Ev == TEXT("prestigepaths")) { SetProgress(D, Id, PathsAny, PInt(Row, TEXT("count"), 12)); return PathsAny >= PInt(Row, TEXT("count"), 12); }
			SetProgress(D, Id, Sum, PInt(Row, TEXT("count"), 36));
			return Sum >= PInt(Row, TEXT("count"), 36);
		}
		if (Ev == TEXT("raid")) return D->Raids.Contains(PName(Row, TEXT("raid")));
		return false;
	}

	/** Every live row this player hasn't got yet. */
	void Evaluate(AWRPlayerState* PS)
	{
		UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
		const TArray<TSharedPtr<FJsonValue>>* Rows = DArr(WRDeeds::Achievements(), TEXT("achievements"));
		if (!D || !Rows) return;
		for (const TSharedPtr<FJsonValue>& V : *Rows)
		{
			const TSharedPtr<FJsonObject> Row = V->AsObject();
			if (D->Achieved.Contains(FName(*DStr(Row, TEXT("id")))) || !WRDeeds::IsLive(Row)) continue;
			if (Met(PS, D, Row)) Unlock(PS, Row);
		}
	}

	/** Server, every censusSeconds: what every player has built and keeps. */
	void TakeCensus(UWorld* World)
	{
		TMap<FString, TMap<FName, int32>>& C = Census();
		TMap<FString, TMap<FName, int32>> Next;
		// Comfort rested in is a best-ever (OnRested), kept across censuses.
		for (const auto& P : C) if (const int32* Comfort = P.Value.Find(TEXT("comfort"))) Next.FindOrAdd(P.Key).Add(TEXT("comfort"), *Comfort);
		TMap<FString, TMap<FName, int32>> SetCounts;
		TMap<FString, TArray<FVector>> Turrets;
		TMap<FString, TSet<FName>> MonumentKinds;
		TMap<FString, TArray<TPair<FVector, FName>>> Stations;
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* Def = It->GetDef();
			if (!Def || It->OwnerName.IsEmpty()) continue;
			TMap<FName, int32>& Mine = Next.FindOrAdd(It->OwnerName);
			if (!Def->Tier.IsNone())
			{
				const FWRTierDef* T = FWRBuildSets::Get().Tier(Def->Tier);
				if (T && T->IsBiome()) SetCounts.FindOrAdd(It->OwnerName).FindOrAdd(Def->Tier) += 1;
			}
			if (!Def->Monument.IsNone()) { Mine.FindOrAdd(TEXT("monuments")) += 1; MonumentKinds.FindOrAdd(It->OwnerName).Add(Def->Monument); }
			if (Def->Id.ToString().Contains(TEXT("turret"))) Turrets.FindOrAdd(It->OwnerName).Add(It->GetActorLocation());
			if (!Def->Station.IsNone()) Stations.FindOrAdd(It->OwnerName).Add({ It->GetActorLocation(), Def->Station });
			if (FWRMachines::IsMachine(*Def) && It->Machine.State == EWRMachineState::Running)
			{
				int32& Links = Mine.FindOrAdd(TEXT("machineLinks"));
				Links = FMath::Max(Links, int32(It->Machine.NetMachines));
			}
		}
		for (const auto& P : SetCounts) { int32 Best = 0; for (const auto& T : P.Value) Best = FMath::Max(Best, T.Value); Next.FindOrAdd(P.Key).Add(TEXT("setPieces"), Best); }
		for (const auto& P : MonumentKinds) Next.FindOrAdd(P.Key).Add(TEXT("monumentKinds"), P.Value.Num());
		for (const auto& P : Turrets)
		{
			int32 Best = 0;
			for (const FVector& A : P.Value) { int32 N = 0; for (const FVector& B : P.Value) N += FVector::Dist(A, B) <= 3000.f; Best = FMath::Max(Best, N); }
			Next.FindOrAdd(P.Key).Add(TEXT("turretLine"), Best);
		}
		for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
		{
			if (It->OwnerName.IsEmpty()) continue;
			Next.FindOrAdd(It->OwnerName).FindOrAdd(TEXT("claims")) += 1;
			// The full workshop: the four stations inside this claim with its owner's Hearth at 5.
			if (FWRMachines::OwnedHearthLevel(World, It->OwnerName) < 5) continue;
			TSet<FName> Here;
			if (const TArray<TPair<FVector, FName>>* S = Stations.Find(It->OwnerName))
				for (const TPair<FVector, FName>& X : *S) if (FVector::Dist2D(X.Key, It->GetActorLocation()) <= It->Radius) Here.Add(X.Value);
			if (Here.Contains(TEXT("workbench")) && Here.Contains(TEXT("forge")) && Here.Contains(TEXT("alchemy_table")) && Here.Contains(TEXT("armorer")))
				Next.FindOrAdd(It->OwnerName).Add(TEXT("stations5"), 1);
		}
		for (TActorIterator<AWRCreature> It(World); It; ++It) if (It->IsTamed() && It->IsAlive()) Next.FindOrAdd(It->TamedBy).FindOrAdd(TEXT("tames")) += 1;
		if (World->GetGameState())
		{
			for (APlayerState* P : World->GetGameState()->PlayerArray)
			{
				if (!P) continue;
				Next.FindOrAdd(P->GetPlayerName()).Add(TEXT("hearth"), FWRMachines::OwnedHearthLevel(World, P->GetPlayerName()));
			}
		}
		C = MoveTemp(Next);
	}

	void GiveOrBagImpl(AWRPlayerState* PS, FName Item, int32 N, const FVector& At)
	{
		const int32 Left = PS->GetInventory()->AddItem(Item, N);
		if (Left <= 0) return;
		// No room: it waits in a bag on the ground, only theirs (as the boss's blueprints do).
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRLootBag* Bag = PS->GetWorld()->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), At, FRotator::ZeroRotator, P))
		{
			Bag->OwnerName = PS->GetPlayerName();
			Bag->bOwnerOnly = true;
			FWRItemStack S = FWRInventoryRules::MakeInstance(Item);
			S.Count = Left;
			Bag->Items.Add(S);
			Bag->SetLifeSpan(WRTune(TEXT("death.bagMinutes"), 30.f) * 60.f);
		}
	}
}
using namespace WRDeedsAch;

void WRDeeds::GiveOrBag(AWRPlayerState* PS, FName Item, int32 Count, const FVector& At) { if (PS && Count > 0) GiveOrBagImpl(PS, Item, Count, At); }

// ================================================================================================ live rows

bool WRDeeds::IsLive(const TSharedPtr<FJsonObject>& Row)
{
	const FName Id(*DStr(Row, TEXT("id")));
	if (const bool* Cached = LiveCache().Find(Id)) return *Cached;
	bool bLive = true;
	if (const TArray<TSharedPtr<FJsonValue>>* Needs = DArr(Row, TEXT("needs")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Needs)
		{
			FString K, Val;
			const FString N = V->AsString();
			if (!N.Split(TEXT(":"), &K, &Val)) { K = N; }
			const int32 Count = FCString::Atoi(*Val);
			bool bOk = false;
			if (K == TEXT("boss")) bOk = FWRCreatureRegistry::Get().Find(FName(*Val)) != nullptr;
			else if (K == TEXT("biome")) { for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All()) bOk |= C.Biome == FName(*Val) && !C.bPortalOnly; }
			else if (K == TEXT("path")) bOk = FWRPathRegistry::Get().Find(FName(*Val)) != nullptr;
			else if (K == TEXT("paths")) bOk = FWRPathRegistry::Get().All().Num() >= Count;
			else if (K == TEXT("skill"))
			{
				if (const TArray<TSharedPtr<FJsonValue>>* Skills = DArr(FWRTuning::LoadDataFile(TEXT("lifeskills.json")), TEXT("skills")))
					for (const TSharedPtr<FJsonValue>& S : *Skills) bOk |= DStr(S->AsObject(), TEXT("id")) == Val;
			}
			else if (K == TEXT("raid")) bOk = WRWarHall::IsLive(FName(*Val));
			else if (K == TEXT("raids")) bOk = WRWarHall::LiveCount() >= Count;
			else if (K == TEXT("majors")) { int32 M = 0; for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All()) M += C.Role.Contains(TEXT("major")); bOk = M >= Count; }
			else if (K == TEXT("pass")) { for (const FWRPoi& P : FWRPoi::All) bOk |= P.Kind == TEXT("pass") && P.Name == Val; }
			else if (K == TEXT("hiddenin"))
			{
				if (const TArray<TSharedPtr<FJsonValue>>* H = DArr(WRWorld::Data(), TEXT("hidden")))
					for (const TSharedPtr<FJsonValue>& T : *H) bOk |= DStr(T->AsObject(), TEXT("biome")) == Val;
			}
			else if (K == TEXT("mythic")) bOk = DBool(Mythic(), TEXT("live"));
			if (!bOk) { bLive = false; break; }
		}
	}
	// The island's POIs load with the map: don't cache a "pass" answer before they're there.
	if (FWRPoi::All.Num() > 0) LiveCache().Add(Id, bLive);
	return bLive;
}

// ================================================================================================ the component's checks

void UWRDeedsComponent::StepChecks()
{
	AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!PS || !World) return;
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(Pawn); C && C->IsAlive())
	{
		const FVector At = Pawn->GetActorLocation();
		// Landmarks: stood within reach of it.
		const float Reach = WRTune(TEXT("deeds.landmarkCm"), 6000.f);
		for (const FLandmark& L : LandmarksOf(World))
		{
			if (!Landmarks.Contains(L.Id) && FVector::Dist2D(L.At, At) < Reach) { Landmarks.Add(L.Id); bDirty = true; }
		}
		// Passes: crossing the ridge near one (on foot or mounted).
		const int32 Side = At.X < WRTune(TEXT("sky.biomeCentre"), 0.f) ? -1 : 1;
		FName Near;
		float Best = WRTune(TEXT("deeds.passCm"), 30000.f);
		for (const FWRPoi& P : FWRPoi::All)
		{
			if (P.Kind == TEXT("pass") && FVector::Dist2D(P.Location, At) < Best) { Best = FVector::Dist2D(P.Location, At); Near = FName(*P.Name); }
		}
		if (LastSide != 0 && Side != LastSide && !Near.IsNone() && !PassesCrossed.Contains(Near)) { PassesCrossed.Add(Near); bDirty = true; }
		LastSide = Side;
		// Hidden roads: inside one's tube.
		for (TActorIterator<AWRHiddenTube> It(World); It; ++It)
		{
			const TArray<FVector>& R = It->Route;
			for (int32 i = 0; i + 1 < R.Num(); ++i)
			{
				const FVector P = FMath::ClosestPointOnSegment(At, R[i], R[i + 1]);
				if (FVector::Dist2D(P, At) < 350.f && FMath::Abs(P.Z - At.Z) < 600.f)
				{
					FName Biome = WRBiome::At(World, P);
					if (const TArray<TSharedPtr<FJsonValue>>* H = DArr(WRWorld::Data(), TEXT("hidden")))
						for (const TSharedPtr<FJsonValue>& T : *H) if (DStr(T->AsObject(), TEXT("id")) == It->TubeId.ToString()) Biome = FName(*DStr(T->AsObject(), TEXT("biome")));
					if (!HiddenFound.Contains(Biome)) { HiddenFound.Add(Biome); bDirty = true; }
					break;
				}
			}
		}
	}
	// The soul art follows the held weapon (an awakened one gives it a quick slot).
	if (UWRPathComponent* Paths = PS->GetPaths())
	{
		const FName Art = WRDeeds::SoulArtFor(PS);
		const bool bHas = !Art.IsNone() && Paths->GetQuickSlots().Contains(Art);
		const bool bStale = Art.IsNone() && Paths->GetQuickSlots().ContainsByPredicate([](const FName& S) { return S.ToString().StartsWith(TEXT("soul.")); });
		if ((!Art.IsNone() && !bHas) || bStale) Paths->RefreshSoulArt();
	}
	Evaluate(PS);
}

// ================================================================================================ hooks

void WRDeeds::Start(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	LiveCache().Reset();
	Landmarks().Reset();
	Census().Reset();
	World->GetTimerManager().SetTimer(CensusTimer, FTimerDelegate::CreateWeakLambda(World, [World]() { TakeCensus(World); }), WRTune(TEXT("deeds.censusSeconds"), 30.f), true, 5.f);
	if (!RestHandle.IsValid()) RestHandle = UWRSurvivalComponent::OnRest.AddLambda([](AWRPlayerState* PS) { WRDeeds::OnRested(PS); });
	WRRecords::Start(World);
	WRWarHall::Start(World);
}

void WRDeeds::OnRested(AWRPlayerState* PS)
{
	const UWRSurvivalComponent* S = PS ? PS->GetSurvival() : nullptr;
	if (!S) return;
	int32& Comfort = Census().FindOrAdd(PS->GetPlayerName()).FindOrAdd(TEXT("comfort"));
	Comfort = FMath::Max(Comfort, S->GetComfortHere());
	Evaluate(PS);
}

void WRDeeds::Notify(AWRPlayerState* PS, FName Event, FName Key, int32 Value)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	const TArray<TSharedPtr<FJsonValue>>* Rows = DArr(Achievements(), TEXT("achievements"));
	if (!D || !Rows) return;
	for (const TSharedPtr<FJsonValue>& V : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = V->AsObject();
		const FName Id(*DStr(Row, TEXT("id")));
		if (D->Achieved.Contains(Id) || !IsLive(Row)) continue;
		const FString Ev = DStr(Row, TEXT("event"));
		bool bHit = false;
		if (Event == TEXT("dev")) bHit = Id == Key;
		else if (Ev != Event.ToString()) continue;
		else if (Ev == TEXT("bosssolo") || Ev == TEXT("bossdeathless")) bHit = PName(Row, TEXT("boss")) == Key;
		else if (Ev == TEXT("portal")) bHit = Value / 100 >= PInt(Row, TEXT("rank"), 5) && Value % 100 >= PInt(Row, TEXT("mods"), 3);
		else if (Ev == TEXT("raidal")) bHit = Value >= PInt(Row, TEXT("count"), 10);
		else if (Ev == TEXT("mythic")) bHit = PName(Row, TEXT("kind")) == Key;
		else bHit = true;   // escaped, raidheld, unmade, triumph
		if (bHit) Unlock(PS, Row);
	}
	Evaluate(PS);
}

void WRDeeds::OnBossKilled(AWRBoss* Boss)
{
	if (!Boss || !Boss->HasAuthority()) return;
	UWorld* World = Boss->GetWorld();
	const FWRCreatureDef* Def = Boss->GetDef();
	if (!Def) return;
	const double Now = World->GetTimeSeconds();
	const float Seconds = Boss->FirstHitAt > 0.0 ? float(Now - Boss->FirstHitAt) : 0.f;
	const float Need = Boss->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	// The party's AL: that of whoever dealt the most.
	FString Top;
	float TopDmg = -1.f;
	TArray<FString> Names;
	for (const TPair<FString, float>& L : Boss->Ledger)
	{
		if (L.Value <= 0.f) continue;
		Names.Add(L.Key);
		if (L.Value > TopDmg) { TopDmg = L.Value; Top = L.Key; }
	}
	AWRPlayerState* TopPS = DStateNamed(World, Top);
	const int32 PartyAL = TopPS ? WRArtifacts::PartyAL(TopPS) : 0;
	// Kill-time boards (a raid's champions are timed as one fight by their hall).
	if (!Boss->IsChampion() && Names.Num() > 0 && Seconds > 0.f) WRRecords::Note(World, Boss->CreatureId, Names, Boss->Participants, Seconds, PartyAL);
	const bool bSolo = Names.Num() == 1 && Boss->Participants <= 1;
	const TSharedPtr<FJsonObject> Soul = SoulRow(Boss->CreatureId);
	const FVector At = Boss->GetActorLocation();
	for (const TPair<FString, float>& L : Boss->Ledger)
	{
		AWRPlayerState* PS = DStateNamed(World, L.Key);
		UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
		if (!PS || !D || L.Value < Need) continue;
		D->Bosses.AddUnique(Boss->CreatureId);
		D->bDirty = true;
		const bool bFirst = !PS->BossKills.Contains(Boss->CreatureId);
		const int32 AL = WRArtifacts::PartyAL(PS);
		TArray<FString> Got;
		if (Boss->IsMajor())
		{
			// The soul (cooks/endgame.md 2.7): always on your first kill and on your first kill at AL 10+.
			if (Soul.IsValid())
			{
				const TSharedPtr<FJsonObject> Drop = DChild(Souls(), TEXT("drop"));
				const UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
				const FWRGeisRecord* Rec = A ? A->Records.FindByPredicate([Boss](const FWRGeisRecord& R) { return R.Boss == Boss->CreatureId; }) : nullptr;
				const int32 GeisAt = FMath::RoundToInt(DNum(Drop, TEXT("geisAt"), 10.f));
				const bool bGeisFirst = AL >= GeisAt && (!Rec || Rec->Best < GeisAt);
				float Chance = bFirst || bGeisFirst ? DNum(Drop, TEXT("first"), 1.f) : DNum(Drop, TEXT("repeat"), 0.1f) + DNum(Drop, TEXT("perAL"), 0.01f) * AL;
				if (Boss->Phase >= 3) Chance *= DNum(Drop, TEXT("phase3Mult"), 2.f);
				if (FMath::FRand() < Chance)
				{
					GiveOrBagImpl(PS, FName(*DStr(Soul, TEXT("soul"))), 1, At);
					Got.Add(TEXT("its soul"));
				}
			}
			// Its head, for a monument at home.
			const FName Head(*FString::Printf(TEXT("trophy_head_%s"), *Boss->CreatureId.ToString()));
			if (FWRItemRegistry::Get().Find(Head) && FMath::FRand() < (bFirst ? WRTune(TEXT("deeds.headFirst"), 1.f) : WRTune(TEXT("deeds.headRepeat"), 0.25f)))
			{
				GiveOrBagImpl(PS, Head, 1, At);
				Got.Add(TEXT("its head"));
			}
			// The Heron's draught: a rare repeat-kill drop (the respec, DR #9).
			if (!bFirst && FMath::FRand() < WRTune(TEXT("deeds.draught.majorRepeat"), 0.03f) + WRTune(TEXT("deeds.draught.majorPerAL"), 0.005f) * AL)
			{
				GiveOrBagImpl(PS, TEXT("heron_draught"), 1, At);
				Got.Add(TEXT("a Heron's draught"));
			}
		}
		if (Got.Num() > 0)
		{
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(FString::Printf(TEXT("From %s: %s"), *Boss->GetDisplayLabel(), *FString::Join(Got, TEXT(", "))));
		}
		if (bSolo) Notify(PS, TEXT("bosssolo"), Boss->CreatureId);
		if (!Boss->bSomeoneDied) Notify(PS, TEXT("bossdeathless"), Boss->CreatureId);
		Evaluate(PS);
	}
}

void WRDeeds::OnKilled(AWRCharacterBase* Victim, AActor* Killer)
{
	if (!Victim || !Victim->HasAuthority()) return;
	UWorld* World = Victim->GetWorld();
	if (const AWRPlayerState* VictimPS = Cast<AWRPlayerState>(Victim->GetPlayerState()))
	{
		// A survivor's death: their days alive start over, and any boss fight they were in isn't deathless.
		if (UWRDeedsComponent* D = UWRDeedsComponent::For(VictimPS)) { D->DeathDay = Today(Victim); D->bDirty = true; }
		const float Reach = WRTune(TEXT("deeds.deathlessCm"), 9000.f);
		for (TActorIterator<AWRBoss> It(World); It; ++It)
		{
			if (It->IsAlive() && It->bEngaged && FVector::Dist2D(It->GetHome(), Victim->GetActorLocation()) < FMath::Max(Reach, It->GetChaseRadius())) It->bSomeoneDied = true;
		}
		return;
	}
	const AWRCreature* C = Cast<AWRCreature>(Victim);
	if (!C || C->IsTamed()) return;
	const FString Who = WRWorld::PlayerOf(Killer);
	AWRPlayerState* PS = Who.IsEmpty() ? nullptr : DStateNamed(World, Who);
	if (UWRDeedsComponent* D = UWRDeedsComponent::For(PS))
	{
		if (!D->Species.Contains(C->CreatureId)) { D->Species.Add(C->CreatureId); D->bDirty = true; }
	}
}

void WRDeeds::OnTamed(AWRCreature* Creature, const FString& Player)
{
	if (!Creature) return;
	AWRPlayerState* PS = DStateNamed(Creature->GetWorld(), Player);
	if (UWRDeedsComponent* D = UWRDeedsComponent::For(PS))
	{
		if (!D->Tamed.Contains(Creature->CreatureId)) { D->Tamed.Add(Creature->CreatureId); D->bDirty = true; }
	}
}

void WRDeeds::OnQuestStep(AWRPlayerState* PS, const TSharedPtr<FJsonObject>& Step)
{
	const FString Title = DStr(Step, TEXT("title_reward"));
	if (PS && !Title.IsEmpty()) GiveTitle(PS, Title, DStr(Step, TEXT("title")));
}

void WRDeeds::OnPortalCleared(AWRPlayerState* PS, int32 Rank, int32 Modifiers, float)
{
	if (!PS) return;
	// The Heron's draught: rank 3 to 5 guardians now and then.
	const TArray<TSharedPtr<FJsonValue>>* Odds = DArr(DChild(FWRTuning::Get().Object(TEXT("deeds")), TEXT("draught")), TEXT("portal"));
	const float Chance = Odds && Odds->IsValidIndex(Rank - 1) ? float((*Odds)[Rank - 1]->AsNumber()) : 0.f;
	if (Chance > 0.f && FMath::FRand() < Chance)
	{
		GiveOrBagImpl(PS, TEXT("heron_draught"), 1, PS->GetPawn() ? PS->GetPawn()->GetActorLocation() : FVector::ZeroVector);
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(TEXT("Among the guardian's things: a Heron's draught"));
	}
	Notify(PS, TEXT("portal"), NAME_None, Rank * 100 + Modifiers);
}

void WRDeeds::OnEscapedSlain(AWRPlayerState* PS) { if (PS) Notify(PS, TEXT("escaped")); }
void WRDeeds::OnRaidHeld(AWRPlayerState* PS) { if (PS) Notify(PS, TEXT("raidheld")); }

void WRDeeds::OnRaidWon(AWRPlayerState* PS, FName Raid, bool bNoTriumph, int32 PartyAL)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	if (!D) return;
	D->Raids.Add(Raid);
	D->bDirty = true;
	if (bNoTriumph) Notify(PS, TEXT("triumph"));
	Notify(PS, TEXT("raidal"), NAME_None, PartyAL);
	Evaluate(PS);
}
