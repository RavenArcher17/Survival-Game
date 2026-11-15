// The world systems' server side (cooks/build-world-systems.md): UWRWorldSystems (placing, the save, Starfall, the
// biome events, invasions, caravans, fast travel), AWRWorldState, the quest log and the hooks the rest of the game calls.
// The actors' looks and their own behaviour are in WRWorldSites.cpp, the people in WRWorldNpc.cpp, the screens in
// WRUIWorld.cpp.

#include "WRWorldSystems.h"
#include "WRLand.h"
#include "WRForestTypes.h"
#include "WRStormWorld.h"
#include "WRSwampSystems.h"
#include "WRForestHazards.h"
#include "WRForestWorld.h"
#include "WRGraveWorld.h"
#include "WRSkyWorld.h"
#include "WRGraveTypes.h"
#include "WRCrystalWorld.h"
#include "WRSwampBosses.h"
#include "WRDeeds.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameMode.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWildlife.h"
#include "WRFarm.h"
#include "WRResourceNode.h"
#include "WRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "WRWorldDressing.h"
#include "WRWorlds.h"
#include "WRStore.h"
#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyBosses.h"

/** A row placed on a Sky island ("island": id, "at" in map metres): the island's top there (cooks/build-biome-sky.md 2.15). */
static bool WSkyIslandSpot(const TSharedPtr<FJsonObject>& Row, double X, double Y, FVector& Out)
{
	FString Island;
	if (!Row.IsValid() || !Row->TryGetStringField(TEXT("island"), Island) || Island.IsEmpty()) return false;
	const FWRSkyIsleData::FIsland* I = FWRSkyIsleData::Get().FindIsland(FName(*Island));
	if (!I) return false;
	Out = FVector(X * 100.0, Y * 100.0, 0.0);
	Out.Z = I->TopZ(Out);
	return true;
}

namespace WRWorldImpl
{
	TSharedPtr<FJsonObject>& WCached() { static TSharedPtr<FJsonObject> D; return D; }

	TSharedPtr<FJsonObject> WChild(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		const TSharedPtr<FJsonObject>* C = nullptr;
		return O.IsValid() && O->TryGetObjectField(Key, C) ? *C : nullptr;
	}
	const TArray<TSharedPtr<FJsonValue>>* WArr(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(Key, A) ? A : nullptr;
	}
	float WNumOf(const TSharedPtr<FJsonObject>& O, const FString& Key, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback;
	}
	FString WStrOf(const TSharedPtr<FJsonObject>& O, const FString& Key, const FString& Fallback = FString())
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(Key, S) ? S : Fallback;
	}
	FVector2D WRangeOf(const TSharedPtr<FJsonObject>& O, const FString& Key, FVector2D Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = WArr(O, Key);
		return A && A->Num() >= 2 ? FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) : Fallback;
	}
	TMap<FName, int32> WCountsOf(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FName, int32> Out;
		if (O.IsValid()) for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : O->Values) Out.Add(FName(*KV.Key), FMath::RoundToInt(KV.Value->AsNumber()));
		return Out;
	}
	/** An item -> [min, max] table rolled. */
	TMap<FName, int32> WRoll(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FName, int32> Out;
		if (!O.IsValid()) return Out;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : O->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (KV.Value->TryGetArray(A) && A->Num() >= 2) Out.Add(FName(*KV.Key), FMath::RandRange(int32((*A)[0]->AsNumber()), int32((*A)[1]->AsNumber())));
			else Out.Add(FName(*KV.Key), FMath::RoundToInt(KV.Value->AsNumber()));
		}
		return Out;
	}
	/** item -> chance: one each that comes up. */
	void WBonus(const TSharedPtr<FJsonObject>& O, TMap<FName, int32>& Into)
	{
		if (!O.IsValid()) return;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : O->Values) if (FMath::FRand() < KV.Value->AsNumber()) Into.FindOrAdd(FName(*KV.Key)) += 1;
	}

	double WGameHours(const UObject* Ctx) { const AWRGameState* GS = AWRGameState::Get(Ctx); return GS ? GS->GetTotalHours() : 0.0; }
	float WServerNow(const UObject* Ctx) { return UWRWeatherSubsystem::Now(Ctx); }

	/** A point on land: traced down; in the sea it steps toward the island's middle until it isn't. */
	bool WSettle(UWorld* World, FVector2D Cm, FVector& Out)
	{
		const float Sea = WRTune(TEXT("sea.level"), -20.f) + 150.f;
		for (int32 Step = 0; Step < 80; ++Step)
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRWorldSettle), false);
			if (!World->LineTraceSingleByChannel(Hit, FVector(Cm, 200000.f), FVector(Cm, -20000.f), ECC_WorldStatic, Q))
			{
				return false;   // the ground isn't here yet (streaming)
			}
			if (Hit.ImpactPoint.Z > Sea && !Cast<APawn>(Hit.GetActor()))
			{
				Out = Hit.ImpactPoint;
				return true;
			}
			const FVector2D In = -Cm.GetSafeNormal();
			Cm += In * 2000.f;
		}
		return false;
	}

	FVector2D WMetres(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return V.IsValid() && V->TryGetArray(A) && A->Num() >= 2 ? FVector2D((*A)[0]->AsNumber() * 100.0, (*A)[1]->AsNumber() * 100.0) : FVector2D::ZeroVector;
	}

	const FWRPoi* WPoi(const FString& Name)
	{
		for (const FWRPoi& P : FWRPoi::All) if (P.Name == Name) return &P;
		return nullptr;
	}

	AWRPlayerState* WStateOf(const FString& Name, UWorld* World)
	{
		if (!World || !World->GetGameState()) return nullptr;
		for (APlayerState* P : World->GetGameState()->PlayerArray) if (P && P->GetPlayerName() == Name) return Cast<AWRPlayerState>(P);
		return nullptr;
	}

	AWRPlayerController* WControllerOf(AWRPlayerState* PS) { return PS ? Cast<AWRPlayerController>(PS->GetOwner()) : nullptr; }

	/** Path XP the shrine way: Own to the land's path if it's one of yours, else Each to each of your paths. */
	FString WPayPathXP(AWRPlayerState* PS, FName Path, int32 Own, int32 Each)
	{
		UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
		if (!Paths) return FString();
		if (Paths->IsActivePath(Path) || (Paths->GetMainPath().IsNone() && Paths->GetSubPath().IsNone()))
		{
			Paths->AwardXP(Path, Own);
			return FString::Printf(TEXT("+%d %s XP"), Own, *Path.ToString());
		}
		TArray<FString> Said;
		for (const FName P : { Paths->GetMainPath(), Paths->GetSubPath() })
		{
			if (P.IsNone()) continue;
			Paths->AwardXP(P, Each);
			Said.Add(FString::Printf(TEXT("+%d %s XP"), Each, *P.ToString()));
		}
		return FString::Join(Said, TEXT(", "));
	}

	FString WCompass(const FVector& From, const FVector& To)
	{
		// The island's x is east, y south (as the map draws it).
		const FVector D = To - From;
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y));
		static const TCHAR* Names[] = { TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"), TEXT("south"), TEXT("south-west"), TEXT("west"), TEXT("north-west") };
		return Names[(FMath::RoundToInt((Deg < 0.f ? Deg + 360.f : Deg) / 45.f)) % 8];
	}

	/** Something wild is on this one right now (a creature has it as its target, close by). */
	bool WInFight(const AActor* Who)
	{
		if (!Who) return false;
		for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && !It->IsTamed() && It->GetAggroTarget() == Who && FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) < 4000.f) return true;
		}
		return false;
	}

	void WHealBy(AWRCharacterBase* Body, float Fraction)
	{
		UAbilitySystemComponent* ASC = Body ? Body->GetAbilitySystemComponent() : nullptr;
		const UWRAttributeSet* A = Body ? Body->GetAttributes() : nullptr;
		if (!ASC || !A) return;
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + A->GetMaxHealth() * Fraction));
	}

	FString WSettingOf(const TCHAR* Key) { return FWRSocialRules::Setting(Key, TEXT("normal")).ToLower(); }
	float WEveryScale(const FString& Setting)
	{
		return Setting == TEXT("rare") ? WRWorld::Num(TEXT("events.rareScale"), 2.f) : Setting == TEXT("frequent") ? WRWorld::Num(TEXT("events.frequentScale"), 0.5f) : 1.f;
	}

	bool WAnyPlayerIn(UWorld* World, FName Biome)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && (Biome.IsNone() || WRBiome::At(World, P->GetActorLocation()) == Biome)) return true;
		}
		return false;
	}

	void WTellBiome(UWorld* World, FName Biome, const FString& Message, FName Sound)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			const APawn* P = PC ? PC->GetPawn() : nullptr;
			if (!PC || (!Biome.IsNone() && (!P || WRBiome::At(World, P->GetActorLocation()) != Biome))) continue;
			PC->ClientNotify(Message);
			if (!Sound.IsNone() && P) WRSound::PlayOn(const_cast<APawn*>(P), Sound);
		}
	}

	/** The midland / highland creature kinds of a biome (for Starfall's draw and the invasions' night). */
	FName WKindOf(FName Biome, FName Tier, bool bAggressive)
	{
		TArray<FName> Ids;
		for (const FWRCreatureDef& D : FWRCreatureRegistry::Get().All())
		{
			if (D.Biome == Biome && D.Tier == Tier && D.Archetype != TEXT("boss") && (!bAggressive || D.Behavior == TEXT("aggressive"))) Ids.Add(D.Id);
		}
		return Ids.Num() ? Ids[FMath::RandHelper(Ids.Num())] : NAME_None;
	}
}

using namespace WRWorldImpl;

// ================================================================================================ WRWorld

TSharedPtr<FJsonObject> WRWorld::Data()
{
	TSharedPtr<FJsonObject>& D = WCached();
	if (!D.IsValid()) D = FWRTuning::LoadDataFile(TEXT("world.json"));
	if (!D.IsValid()) D = MakeShared<FJsonObject>();
	return D;
}

TSharedPtr<FJsonObject> WRWorld::Obj(const FString& Path)
{
	TSharedPtr<FJsonObject> O = Data();
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));
	for (const FString& P : Parts)
	{
		O = WChild(O, P);
		if (!O.IsValid()) return nullptr;
	}
	return O;
}

float WRWorld::Num(const FString& Path, float Fallback)
{
	FString Head, Key;
	if (!Path.Split(TEXT("."), &Head, &Key, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) return WNumOf(Data(), Path, Fallback);
	return WNumOf(Obj(Head), Key, Fallback);
}

FColor WRWorld::Rgb(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FColor Fallback)
{
	const TArray<TSharedPtr<FJsonValue>>* A = WArr(O, Field);
	return A && A->Num() >= 3 ? FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber())) : Fallback;
}

TSharedPtr<FJsonObject> WRWorld::Biome(FName Id) { return WChild(Obj(TEXT("biomes")), Id.ToString()); }

FString WRWorld::ItemName(FName Id)
{
	const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
	return D ? D->Name : Id.ToString();
}

FString WRWorld::PlayerOf(const AActor* Actor)
{
	if (const APawn* P = Cast<APawn>(Actor))
	{
		if (P->GetPlayerState()) return P->GetPlayerState()->GetPlayerName();
	}
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(Actor))
	{
		return C->GetTameOwner();
	}
	return FString();
}

bool WRWorld::BlocksHit(const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target) return false;
	const FString Hitter = PlayerOf(Hit.Instigator);
	if (const AWRWorldNpc* Npc = Cast<AWRWorldNpc>(Target))
	{
		// Masters can't be hurt at all; merchants only by the wild.
		if (!Npc->HasVitals() || !Hitter.IsEmpty() || Cast<AWRWorldNpc>(Hit.Instigator)) return true;
		return false;
	}
	UWRWorldSystems* Sys = UWRWorldSystems::Get(Target);
	if (!Sys || !Sys->IsServerRunning()) return false;
	if (const AWRCreature* C = Cast<AWRCreature>(Target))
	{
		if (const TArray<FString>* Party = Sys->Sealed.Find(TWeakObjectPtr<AWRCreature>(const_cast<AWRCreature*>(C))))
		{
			// A summoned boss is the summoner's party's fight (endgame.md 2.8): anyone else's blows do nothing.
			bool bIn = Hitter.IsEmpty() && !Hit.Instigator;   // the world's own (a hazard)
			for (const FString& P : *Party) bIn |= !Hitter.IsEmpty() && FWRSocialRules::CanShare(P, Hitter);
			if (!bIn) return true;
		}
		if (!Hitter.IsEmpty() && Sys->Invaders.Contains(TWeakObjectPtr<AWRCreature>(const_cast<AWRCreature*>(C))))
		{
			Sys->InvasionLedger.FindOrAdd(Hitter) += FMath::Max(0.f, Hit.Damage);
		}
	}
	return false;
}

void WRWorld::OnKilled(AWRCharacterBase* Victim, AActor* Killer)
{
	UWRWorldSystems* Sys = UWRWorldSystems::Get(Victim);
	const AWRCreature* C = Cast<AWRCreature>(Victim);
	if (!Sys || !Sys->IsServerRunning() || !C) return;
	const FString Who = PlayerOf(Killer);
	AWRPlayerState* PS = Who.IsEmpty() ? nullptr : WStateOf(Who, Victim->GetWorld());
	if (PS)
	{
		const bool bBurning = Victim->HasTag(WRTags::Status_Burn);
		const bool bSlowed = Victim->HasTag(WRTags::Status_Slowed) || Victim->HasTag(WRTags::Status_Frozen) || Victim->HasTag(WRTags::Status_Rimed);
		Sys->QuestEvent(PS, TEXT("kill"), C->CreatureId);
		if (bBurning) Sys->QuestEvent(PS, TEXT("kill.burning"), C->CreatureId);
		if (bSlowed) Sys->QuestEvent(PS, TEXT("kill.slowed"), C->CreatureId);
	}
	// A boss is beaten by everyone who fought it (the loot rule's share: Saga Venn's "Two of one"), not only the blow.
	if (const AWRBoss* Boss = Cast<AWRBoss>(C))
	{
		const float Needed = Boss->GetAttributes() ? Boss->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f) : 0.f;
		for (const TPair<FString, float>& L : Boss->Ledger)
		{
			if (L.Key == Who || L.Value < Needed) continue;
			if (AWRPlayerState* Other = WStateOf(L.Key, Victim->GetWorld())) Sys->QuestEvent(Other, TEXT("kill"), C->CreatureId);
		}
	}
	// The Swamp's quest deeds (Ruslana: a Murkjaw dead on land, a cursed kill, the Bloat beaten with salt): WRSwampSystems.
	WRSwamp::OnKilled(Victim, Killer, PS);
}

void WRWorld::OnTamed(AWRCreature* Creature, const FString& Player)
{
	UWRWorldSystems* Sys = Creature ? UWRWorldSystems::Get(Creature) : nullptr;
	if (!Sys || !Sys->IsServerRunning()) return;
	if (AWRPlayerState* PS = WStateOf(Player, Creature->GetWorld())) Sys->QuestEvent(PS, TEXT("tame"), Creature->CreatureId);
}

void WRWorld::OnBossCredit(AWRPlayerState* PS, FName BossId)
{
	FString Effigy;
	if (!PS || !Obj(TEXT("summoning.effigies")).IsValid() || !Obj(TEXT("summoning.effigies"))->TryGetStringField(BossId.ToString(), Effigy)) return;
	if (!PS->KnownBlueprints.Contains(FName(*Effigy)))
	{
		PS->KnownBlueprints.Add(FName(*Effigy));
		if (AWRPlayerController* PC = WControllerOf(PS)) PC->ClientNotify(FString::Printf(TEXT("You know how to make the %s now (alchemy table)"), *ItemName(FName(*Effigy))));
	}
}

float WRWorld::GatherScale(const UObject* WorldContext, FName ItemId, const FVector& At)
{
	const AWRWorldState* State = AWRWorldState::Get(WorldContext);
	if (!State) return 1.f;
	const FName Biome = WRBiome::At(WorldContext, At);
	const TSharedPtr<FJsonObject> Row = WRWorld::Biome(Biome);
	if (!Row.IsValid() || WStrOf(Row, TEXT("signature")) != ItemId.ToString()) return 1.f;
	for (const FWRWorldEventRep& E : State->Events)
	{
		if (E.Biome == Biome && E.Stage >= 1) return WNumOf(Obj(TEXT("events.") + E.Id.ToString()), TEXT("signatureScale"), 3.f);
	}
	// The aurora night after a whiteout: lichen at its best.
	if (State->AuroraBiomes.Contains(Biome) && AWRGameState::IsNightIn(WorldContext)) return 3.f;
	return 1.f;
}

bool WRWorld::IsNightTable(const UObject* WorldContext, FName Biome)
{
	if (AWRGameState::IsNightIn(WorldContext)) return true;
	const AWRWorldState* State = AWRWorldState::Get(WorldContext);
	if (!State) return false;
	for (const FWRWorldEventRep& E : State->Events)
	{
		if (E.Stage == 1 && (Biome.IsNone() || E.Biome == Biome) && WNumOf(Obj(TEXT("events.") + E.Id.ToString()), TEXT("nightTable"), 0.f) > 0.f) return true;
	}
	return false;
}

// ================================================================================================ AWRWorldState

AWRWorldState::AWRWorldState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRWorldState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWorldState, Events);
	DOREPLIFETIME(AWRWorldState, Invasion);
	DOREPLIFETIME(AWRWorldState, AuroraBiomes);
}

AWRWorldState* AWRWorldState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRWorldState> It(World); It; ++It) return *It;
	return nullptr;
}

const FWRWorldEventRep* AWRWorldState::Event(FName Id) const
{
	return Events.FindByPredicate([Id](const FWRWorldEventRep& E) { return E.Id == Id; });
}

bool AWRWorldState::IsOn(FName Id, FName Biome) const
{
	const FWRWorldEventRep* E = Event(Id);
	return E && E->Stage == 1 && (Biome.IsNone() || E->Biome == Biome);
}

void AWRWorldState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	// Every machine that draws: Starfall's streaks across the sky round the viewer, the eruption's tremors.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return;
	if (const FWRWorldEventRep* S = Event(TEXT("starfall")); S && S->Stage == 0)
	{
		StreakClock -= DeltaSeconds;
		if (StreakClock <= 0.f)
		{
			const FVector2D Every = WRangeOf(WRWorld::Obj(TEXT("events.starfall")), TEXT("streakEvery"), FVector2D(1.5, 3.0));
			StreakClock = FMath::FRandRange(Every.X, Every.Y);
			const float Yaw = FMath::FRandRange(0.f, 360.f);
			const FVector At = Me->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(30000.f, 80000.f) + FVector(0.f, 0.f, FMath::FRandRange(60000.f, 90000.f));
			WRFx::SpawnAt(this, TEXT("meteor.streak"), At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), 6.f);
		}
	}
	if (const FWRWorldEventRep* E = Event(TEXT("eruption")); E && E->Stage <= 1 && WRBiome::At(this, Me->GetActorLocation()) == E->Biome)
	{
		TremorClock -= DeltaSeconds;
		if (TremorClock <= 0.f)
		{
			TremorClock = WNumOf(WRWorld::Obj(TEXT("events.eruption")), TEXT("tremorEvery"), 30.f);
			WRFx::Shake(this, WNumOf(WRWorld::Obj(TEXT("events.eruption")), TEXT("tremorShake"), 0.05f) * 20.f);
			WRSound::PlayUI(this, TEXT("world.tremor"));
		}
	}
}

// ================================================================================================ UWRQuestLog

UWRQuestLog::UWRQuestLog()
{
	SetIsReplicatedByDefault(true);
}

void UWRQuestLog::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWRQuestLog, Quests, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRQuestLog, FoundShrines, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRQuestLog, LitByMe, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRQuestLog, Standing, COND_OwnerOnly);
}

UWRQuestLog* UWRQuestLog::For(const AActor* PlayerOrState)
{
	if (!PlayerOrState) return nullptr;
	const APlayerState* PS = Cast<APlayerState>(PlayerOrState);
	if (!PS)
	{
		if (const APawn* P = Cast<APawn>(PlayerOrState)) PS = P->GetPlayerState();
		else if (const AController* C = Cast<AController>(PlayerOrState)) PS = C->PlayerState;
	}
	return PS ? PS->FindComponentByClass<UWRQuestLog>() : nullptr;
}

const FWRQuestRep* UWRQuestLog::QuestWith(FName Master) const
{
	return Quests.FindByPredicate([Master](const FWRQuestRep& Q) { return Q.Master == Master; });
}

FWRQuestRep& UWRQuestLog::QuestMutable(FName Master)
{
	if (FWRQuestRep* Q = Quests.FindByPredicate([Master](const FWRQuestRep& R) { return R.Master == Master; })) return *Q;
	FWRQuestRep& N = Quests.AddDefaulted_GetRef();
	N.Master = Master;
	return N;
}

int32 UWRQuestLog::StandingOf(FName Faction) const
{
	const FWRStanding* S = Standing.FindByPredicate([Faction](const FWRStanding& R) { return R.Faction == Faction; });
	return S ? S->Value : 0;
}

int32 UWRQuestLog::TierOf(FName Faction) const
{
	const int32 V = StandingOf(Faction);
	int32 Tier = 0;
	if (const TArray<TSharedPtr<FJsonValue>>* Tiers = WArr(WRWorld::Obj(TEXT("factions")), TEXT("tiers")))
	{
		for (int32 i = 0; i < Tiers->Num(); ++i) if (V >= WNumOf((*Tiers)[i]->AsObject(), TEXT("at"), 0.f)) Tier = i;
	}
	return Tier;
}

void UWRQuestLog::AddStanding(FName Faction, int32 Amount)
{
	if (Faction.IsNone() || Amount == 0) return;
	const int32 Before = TierOf(Faction);
	FWRStanding* S = Standing.FindByPredicate([Faction](const FWRStanding& R) { return R.Faction == Faction; });
	if (!S) { S = &Standing.AddDefaulted_GetRef(); S->Faction = Faction; }
	S->Value = FMath::Max(0, S->Value + Amount);
	const int32 After = TierOf(Faction);
	if (After > Before)
	{
		const TArray<TSharedPtr<FJsonValue>>* Tiers = WArr(WRWorld::Obj(TEXT("factions")), TEXT("tiers"));
		const FString TierName = Tiers && Tiers->IsValidIndex(After) ? WStrOf((*Tiers)[After]->AsObject(), TEXT("name")) : FString();
		const FString FactionName = WStrOf(WChild(WRWorld::Obj(TEXT("factions")), Faction.ToString()), TEXT("name"), Faction.ToString());
		if (AWRPlayerController* PC = WControllerOf(Cast<AWRPlayerState>(GetOwner()))) PC->ClientNotify(FString::Printf(TEXT("%s count you %s now"), *FactionName, *TierName));
	}
}

TSharedRef<FJsonObject> UWRQuestLog::ToJson() const
{
	const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Q;
	for (const FWRQuestRep& R : Quests)
	{
		const TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("master"), R.Master.ToString());
		J->SetNumberField(TEXT("index"), R.Index);
		J->SetBoolField(TEXT("taken"), R.bTaken);
		J->SetNumberField(TEXT("progress"), R.Progress);
		J->SetNumberField(TEXT("stage"), R.Stage);
		Q.Add(MakeShared<FJsonValueObject>(J));
	}
	O->SetArrayField(TEXT("quests"), Q);
	TArray<TSharedPtr<FJsonValue>> Lit;
	for (const FName& N : LitByMe) Lit.Add(MakeShared<FJsonValueString>(N.ToString()));
	O->SetArrayField(TEXT("lit"), Lit);
	const TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	for (const FWRStanding& R : Standing) S->SetNumberField(R.Faction.ToString(), R.Value);
	O->SetObjectField(TEXT("standing"), S);
	O->SetNumberField(TEXT("appraisedAway"), AppraisedAway);
	return O;
}

void UWRQuestLog::FromJson(const TSharedPtr<FJsonObject>& O)
{
	if (!O.IsValid()) return;
	Quests.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* Q = WArr(O, TEXT("quests")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Q)
		{
			const TSharedPtr<FJsonObject> J = V->AsObject();
			FWRQuestRep& R = Quests.AddDefaulted_GetRef();
			R.Master = FName(*WStrOf(J, TEXT("master")));
			R.Index = int32(WNumOf(J, TEXT("index"), 0.f));
			J->TryGetBoolField(TEXT("taken"), R.bTaken);
			R.Progress = int32(WNumOf(J, TEXT("progress"), 0.f));
			R.Stage = uint8(WNumOf(J, TEXT("stage"), 0.f));
		}
	}
	LitByMe.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* L = WArr(O, TEXT("lit"))) for (const TSharedPtr<FJsonValue>& V : *L) LitByMe.AddUnique(FName(*V->AsString()));
	Standing.Reset();
	if (const TSharedPtr<FJsonObject> S = WChild(O, TEXT("standing")))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : S->Values) { FWRStanding& R = Standing.AddDefaulted_GetRef(); R.Faction = FName(*KV.Key); R.Value = int32(KV.Value->AsNumber()); }
	}
	AppraisedAway = int32(WNumOf(O, TEXT("appraisedAway"), 0.f));
}

// ================================================================================================ UWRWorldSystems

bool UWRWorldSystems::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UWRWorldSystems* UWRWorldSystems::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRWorldSystems>() : nullptr;
}

TStatId UWRWorldSystems::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRWorldSystems, STATGROUP_Tickables); }

void UWRWorldSystems::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	WCached().Reset();   // world.json read fresh each game (it's data: edits take without a rebuild)
	// The server of a real world (the menu's background island runs none and saves nothing, as the wildlife).
	bServer = InWorld.GetNetMode() != NM_Client && InWorld.GetAuthGameMode<AWRGameMode>() != nullptr && !WRWorlds::Current().IsEmpty();
	if (!bServer) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<AWRWorldState>(AWRWorldState::StaticClass(), FTransform::Identity, P);
	Load();
	UE_LOG(LogWildreach, Log, TEXT("WORLDSYS started: clock %.0f s, %d messages to place"), Clock, Loaded.IsValid() && WArr(Loaded, TEXT("messages")) ? WArr(Loaded, TEXT("messages"))->Num() : 0);
}

void UWRWorldSystems::Deinitialize()
{
	if (bServer && bPlaced) Save();
	Super::Deinitialize();
}

void UWRWorldSystems::Load()
{
	FString Text;
	if (!WRStore::ReadDoc(TEXT("world_systems"), Text)) return;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;
	Loaded = Root;
	Clock = WNumOf(Root, TEXT("clock"), 0.f);
	VaultOpenUntilHours = WNumOf(Root, TEXT("vaultUntil"), -1.f);
	if (const TSharedPtr<FJsonObject> N = WChild(Root, TEXT("nextEvent"))) for (const auto& KV : N->Values) NextEventAt.Add(FName(*KV.Key), KV.Value->AsNumber());
	if (const TSharedPtr<FJsonObject> N = WChild(Root, TEXT("nextInvasion"))) for (const auto& KV : N->Values) NextInvasionAt.Add(FString(KV.Key), KV.Value->AsNumber());
	if (const TSharedPtr<FJsonObject> S = WChild(Root, TEXT("shrines")))
	{
		for (const auto& KV : S->Values)
		{
			TSet<FString>& Set = FoundBy.FindOrAdd(FName(*KV.Key));
			if (const TArray<TSharedPtr<FJsonValue>>* F = WArr(KV.Value->AsObject(), TEXT("found"))) for (const auto& V : *F) Set.Add(V->AsString());
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* Aur = WArr(Root, TEXT("aurora")))
	{
		// (placed on the state once it exists, in TryPlace)
	}
}

void UWRWorldSystems::WriteJson(TSharedRef<FJsonObject> Root) const
{
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("clock"), Clock);
	Root->SetNumberField(TEXT("vaultUntil"), VaultOpenUntilHours);
	const TSharedRef<FJsonObject> Next = MakeShared<FJsonObject>();
	for (const auto& KV : NextEventAt) Next->SetNumberField(KV.Key.ToString(), KV.Value);
	Root->SetObjectField(TEXT("nextEvent"), Next);
	const TSharedRef<FJsonObject> NextInv = MakeShared<FJsonObject>();
	for (const auto& KV : NextInvasionAt) NextInv->SetNumberField(KV.Key, KV.Value);
	Root->SetObjectField(TEXT("nextInvasion"), NextInv);
	const TSharedRef<FJsonObject> Sh = MakeShared<FJsonObject>();
	TSet<FName> Ids;
	for (const auto& KV : Shrines) Ids.Add(KV.Key);
	for (const auto& KV : FoundBy) Ids.Add(KV.Key);
	for (const FName& Id : Ids)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		if (const TWeakObjectPtr<AWRShrine>* S = Shrines.Find(Id); S && S->IsValid())
		{
			O->SetBoolField(TEXT("lit"), (*S)->bLit);
			O->SetBoolField(TEXT("everLit"), (*S)->bEverLit);
			O->SetNumberField(TEXT("reguardAt"), (*S)->ReguardAtHours);
		}
		else if (const TSharedPtr<FJsonObject> Was = WChild(WChild(Loaded, TEXT("shrines")), Id.ToString()))
		{
			for (const auto& F : Was->Values) O->SetField(F.Key, F.Value);
		}
		TArray<TSharedPtr<FJsonValue>> Found;
		if (const TSet<FString>* F = FoundBy.Find(Id)) for (const FString& N : *F) Found.Add(MakeShared<FJsonValueString>(N));
		O->SetArrayField(TEXT("found"), Found);
		Sh->SetObjectField(Id.ToString(), O);
	}
	Root->SetObjectField(TEXT("shrines"), Sh);
	TArray<TSharedPtr<FJsonValue>> Msgs;
	for (const TWeakObjectPtr<AWRMessageStone>& W : Messages)
	{
		const AWRMessageStone* M = W.Get();
		if (!M || M->IsActorBeingDestroyed()) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		const FVector L = M->GetActorLocation();
		O->SetNumberField(TEXT("x"), L.X); O->SetNumberField(TEXT("y"), L.Y); O->SetNumberField(TEXT("z"), L.Z);
		O->SetNumberField(TEXT("yaw"), M->GetActorRotation().Yaw);
		O->SetStringField(TEXT("text"), M->Text);
		O->SetStringField(TEXT("author"), M->Author);
		O->SetNumberField(TEXT("good"), M->Good);
		O->SetNumberField(TEXT("poor"), M->Poor);
		O->SetStringField(TEXT("biome"), M->Biome.ToString());
		O->SetNumberField(TEXT("at"), M->PlacedAt);
		TArray<TSharedPtr<FJsonValue>> Rated;
		for (const FString& R : M->RatedBy) Rated.Add(MakeShared<FJsonValueString>(R));
		O->SetArrayField(TEXT("rated"), Rated);
		Msgs.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("messages"), Msgs);
	const TSharedRef<FJsonObject> Car = MakeShared<FJsonObject>();
	for (const FCaravan& C : Caravans) Car->SetNumberField(C.Id.ToString(), C.DeadUntilHours);
	Root->SetObjectField(TEXT("caravans"), Car);
	const TSharedRef<FJsonObject> Caches = MakeShared<FJsonObject>();
	for (TActorIterator<AWRWorldCache> It(GetWorld()); It; ++It)
	{
		if (It->RefillHours > 0.f) Caches->SetNumberField(It->Title, It->bTaken ? It->RefillAtHours : -1.0);
	}
	Root->SetObjectField(TEXT("caches"), Caches);
	if (const AWRWorldState* State = AWRWorldState::Get(GetWorld()))
	{
		TArray<TSharedPtr<FJsonValue>> Aur;
		for (const FName& B : State->AuroraBiomes) Aur.Add(MakeShared<FJsonValueString>(B.ToString()));
		Root->SetArrayField(TEXT("aurora"), Aur);
	}
}

void UWRWorldSystems::Save()
{
	if (!bServer || WRWorlds::CurrentDir().IsEmpty()) return;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	WriteJson(Root);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	WRStore::WriteDoc(TEXT("world_systems"), Text);
}

void UWRWorldSystems::Tick(float DeltaTime)
{
	if (!bServer) return;
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) return;
	if (!bPlaced)
	{
		PlaceClock -= DeltaTime;
		if (PlaceClock <= 0.f)
		{
			PlaceClock = 2.f;
			bPlaced = TryPlace();
		}
		return;
	}
	const bool bAnyone = WAnyPlayerIn(World, NAME_None);
	if (bAnyone) Clock += DeltaTime;
	StepShrinesAndKindles(DeltaTime);
	StepMasters(DeltaTime);
	StepEvents(DeltaTime);
	StepInvasion(DeltaTime);
	StepCaravans(DeltaTime);
	SaveClock += DeltaTime;
	if (SaveClock >= 60.f)
	{
		SaveClock = 0.f;
		Save();
	}
}

bool UWRWorldSystems::TryPlace()
{
	// Everything waits for its ground: a shrine's spot must trace to land before anything goes down.
	UWorld* World = GetWorld();
	if (FWRPoi::All.Num() == 0) return false;
	FVector Probe;
	const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Obj(TEXT("shrines")), TEXT("list"));
	if (List)
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> Row = V->AsObject();
			if (Row->HasField(TEXT("at")) && !WSettle(World, WMetres(Row->TryGetField(TEXT("at"))), Probe)) return false;
		}
	}
	// Villages first (the Great Shrines and masters stand in them).
	if (const TArray<TSharedPtr<FJsonValue>>* Vs = WArr(WRWorld::Data(), TEXT("villages")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Vs)
		{
			const TSharedPtr<FJsonObject> Row = V->AsObject();
			const FName Id(*WStrOf(Row, TEXT("id")));
			const FName Biome(*WStrOf(Row, TEXT("biome")));
			if (!WRBiome::IsLive(Biome)) continue;
			const FWRPoi* Camp = WPoi(WStrOf(Row, TEXT("camp")));
			const TSharedPtr<FJsonObject> B = WRWorld::Biome(Biome);
			const FVector2D Centre = WMetres(B.IsValid() ? B->TryGetField(TEXT("centre")) : nullptr);
			if (!Camp) continue;
			const FVector2D In = (Centre - FVector2D(Camp->Location)).GetSafeNormal();
			FVector Ground;
			if (!WSettle(World, FVector2D(Camp->Location) + In * WNumOf(Row, TEXT("inland"), 250.f) * 100.f, Ground)) return false;
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FRotator Face(0.f, FVector(-In, 0.f).Rotation().Yaw, 0.f);   // the shrine's steps toward the camp and the sea
			AWRVillage* Village = World->SpawnActorDeferred<AWRVillage>(AWRVillage::StaticClass(), FTransform(Face, Ground), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Village) continue;
			Village->VillageId = Id;
			Village->Biome = Biome;
			Village->Style = FName(*WStrOf(Row, TEXT("style"), TEXT("basalt")));
			Village->Huts = int32(WNumOf(Row, TEXT("huts"), 5.f));
			Village->Ring = WNumOf(Row, TEXT("ring"), 1600.f);
			Village->DisplayName = WStrOf(Row, TEXT("name"), Id.ToString());
			Village->FinishSpawning(FTransform(Face, Ground));
			Villages.Add(Id, Village);
			if (AWRSafeZone* Zone = World->SpawnActor<AWRSafeZone>(AWRSafeZone::StaticClass(), FTransform(Ground), P))
			{
				Zone->Radius = WNumOf(Row, TEXT("radius"), 4500.f);
			}
		}
	}
	PlaceShrines();
	PlaceMasters();
	PlaceTubes();
	// Messages from the save.
	if (const TArray<TSharedPtr<FJsonValue>>* Msgs = WArr(Loaded, TEXT("messages")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Msgs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector L(WNumOf(O, TEXT("x"), 0.f), WNumOf(O, TEXT("y"), 0.f), WNumOf(O, TEXT("z"), 0.f));
			AWRMessageStone* M = World->SpawnActorDeferred<AWRMessageStone>(AWRMessageStone::StaticClass(), FTransform(FRotator(0.f, WNumOf(O, TEXT("yaw"), 0.f), 0.f), L), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!M) continue;
			M->Text = WStrOf(O, TEXT("text"));
			M->Author = WStrOf(O, TEXT("author"));
			M->Good = int32(WNumOf(O, TEXT("good"), 0.f));
			M->Poor = int32(WNumOf(O, TEXT("poor"), 0.f));
			M->Biome = FName(*WStrOf(O, TEXT("biome")));
			M->PlacedAt = WNumOf(O, TEXT("at"), 0.f);
			if (const TArray<TSharedPtr<FJsonValue>>* R = WArr(O, TEXT("rated"))) for (const auto& X : *R) M->RatedBy.Add(X->AsString());
			M->FinishSpawning(FTransform(FRotator(0.f, WNumOf(O, TEXT("yaw"), 0.f), 0.f), L));
			Messages.Add(M);
		}
	}
	// Caravans.
	if (const TArray<TSharedPtr<FJsonValue>>* Cs = WArr(WRWorld::Data(), TEXT("caravans")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Cs)
		{
			const TSharedPtr<FJsonObject> Row = V->AsObject();
			if (!WRBiome::IsLive(FName(*WStrOf(Row, TEXT("biome"))))) continue;
			FCaravan C;
			C.Id = FName(*WStrOf(Row, TEXT("id")));
			C.DeadUntilHours = WNumOf(WChild(Loaded, TEXT("caravans")), C.Id.ToString(), -1.f);
			const TArray<TSharedPtr<FJsonValue>>* Stops = WArr(Row, TEXT("stops"));
			for (int32 i = 0; Stops && i < Stops->Num(); ++i)
			{
				FString Kind, Name;
				(*Stops)[i]->AsString().Split(TEXT(":"), &Kind, &Name);
				FVector At = FVector::ZeroVector;
				bool bFound = false;
				if (Kind == TEXT("village"))
				{
					if (const TWeakObjectPtr<AWRVillage>* Vg = Villages.Find(FName(*Name)); Vg && Vg->IsValid()) { At = (*Vg)->StallAt(); bFound = true; }
				}
				else if (const FWRPoi* P = WPoi(Name))
				{
					FVector2D Spot(P->Location);
					if (Kind == TEXT("pass"))
					{
						// The caravan stops at its own side's mouth of the pass.
						const TSharedPtr<FJsonObject> B = WRWorld::Biome(FName(*WStrOf(Row, TEXT("biome"))));
						const FVector2D Centre = WMetres(B.IsValid() ? B->TryGetField(TEXT("centre")) : nullptr);
						Spot += FVector2D(FMath::Sign(Centre.X) * WRWorld::Num(TEXT("caravan.passInset"), 120.f) * 100.f, 0.f);
					}
					else
					{
						Spot += (FVector2D::ZeroVector - Spot).GetSafeNormal() * 1500.f;   // beside the camp, not in its middle
					}
					bFound = WSettle(World, Spot, At);
				}
				if (bFound) { C.Stops.Add(At); C.StopNames.Add(Name); }
			}
			if (C.Stops.Num() >= 2) Caravans.Add(C);
		}
	}
	if (AWRWorldState* State = AWRWorldState::Get(World))
	{
		if (const TArray<TSharedPtr<FJsonValue>>* Aur = WArr(Loaded, TEXT("aurora"))) for (const auto& V : *Aur) State->AuroraBiomes.AddUnique(FName(*V->AsString()));
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AWRPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AWRPlayerState>() : nullptr) RefreshFound(PS);
	}
	UE_LOG(LogWildreach, Log, TEXT("WORLDSYS placed: %d shrines, %d villages, %d masters, %d messages, %d caravans"), Shrines.Num(), Villages.Num(), Masters.Num(), Messages.Num(), Caravans.Num());
	return true;
}

void UWRWorldSystems::PlaceShrines()
{
	UWorld* World = GetWorld();
	const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Obj(TEXT("shrines")), TEXT("list"));
	if (!List) return;
	const double Hours = WGameHours(World);
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> Row = V->AsObject();
		const FName Id(*WStrOf(Row, TEXT("id")));
		const FName Biome(*WStrOf(Row, TEXT("biome")));
		if (!WRBiome::IsLive(Biome) || Shrines.Contains(Id)) continue;
		bool bGreat = false;
		Row->TryGetBoolField(TEXT("great"), bGreat);
		FVector Ground;
		FRotator Face = FRotator::ZeroRotator;
		bool bSwampRow = false;
		Row->TryGetBoolField(TEXT("swamp"), bSwampRow);
		FVector2D SwampAt = FVector2D::ZeroVector;
		float SwampYaw = 0.f;
		bool bForestRow = false;
		Row->TryGetBoolField(TEXT("forest"), bForestRow);
		float ForestYaw = 0.f;
		const FName OnLand(*WStrOf(Row, TEXT("land")));
		if (!OnLand.IsNone())
		{
			// A runtime land's shrine (the Wasteland's five): "at" is in the land's own map metres, the ground its height
			// function (the land's chunks may still be building, so never a trace).
			const TArray<TSharedPtr<FJsonValue>>* AtArr = WArr(Row, TEXT("at"));
			if (!AtArr || AtArr->Num() < 2) continue;
			// On a Sky island (the land's map metres, the island's top: cooks/build-biome-sky.md 2.15).
			if (!WSkyIslandSpot(Row, (*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber(), Ground)
				&& !WRLand::PlaceOn(OnLand, FVector2D((*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber()), Ground)) continue;
			Face = FRotator(0.f, WNumOf(Row, TEXT("yaw"), 0.f), 0.f);
			// Under the ground with a floor height (map metres; the Crystal caves' halls): it stands on that floor, not the
			// plateau above it.
			if (Row->HasField(TEXT("floor")))
			{
				const FWRLandDef* LD = WRLand::Find(OnLand);
				if (!LD) continue;
				Ground = LD->ToWorld(FVector2D((*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber()), WNumOf(Row, TEXT("floor"), 0.f));
			}
			// Under the ground: a shrine in a catacomb room stands on the room's floor (the Graveyard's Watchers' Vault).
			const FString Room = WStrOf(Row, TEXT("room"));
			if (!Room.IsEmpty())
			{
				const FWRGraveData::FRoom* R = FWRGraveData::Get().FindRoom(FName(*Room));
				if (!R) continue;
				Ground = R->At + FVector(0.f, 0.f, 5.f);
			}
		}
		else if (bSwampRow)
		{
			// The Swamp's five take their places from Data/swamp.json shrines (WRSwampSystems), steps toward the nearest camp.
			if (!WRSwamp::ShrinePlace(Id, SwampAt, SwampYaw) || !WSettle(World, SwampAt, Ground)) continue;
			Face = FRotator(0.f, SwampYaw, 0.f);
		}
		else if (bForestRow && WRForestShape::ShrineSpot(World, Id, Ground, ForestYaw))
		{
			// The Ancient forest's five stand where Data/forest.json puts them, on what they stand on (WRForestWorld): the
			// Falls Shrine on the fishing platform, the Canopy Shrine on its deck 30 m up, the rest on the ground.
			Face = FRotator(0.f, ForestYaw, 0.f);
		}
		else if (bGreat && !Row->HasField(TEXT("at")))
		{
			const TWeakObjectPtr<AWRVillage>* Vg = Villages.Find(FName(*WStrOf(Row, TEXT("village"))));
			if (!Vg || !Vg->IsValid()) continue;
			Ground = (*Vg)->GetActorLocation();
			Face = (*Vg)->GetActorRotation();
		}
		else
		{
			if (!WSettle(World, WMetres(Row->TryGetField(TEXT("at"))), Ground)) continue;
			// The steps face the way most people come: toward the island's coast road (out from the middle), or the
			// row's own yaw (a Great Shrine with no village, Storm peaks' shrines on their paths).
			Face = FRotator(0.f, Row->HasField(TEXT("yaw")) ? WNumOf(Row, TEXT("yaw"), 0.f) : FVector(Ground.X, Ground.Y, 0.f).Rotation().Yaw, 0.f);
		}
		AWRShrine* S = World->SpawnActorDeferred<AWRShrine>(AWRShrine::StaticClass(), FTransform(Face, Ground), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!S) continue;
		S->ShrineId = Id;
		S->Biome = Biome;
		S->bGreat = bGreat;
		S->DisplayName = WStrOf(Row, TEXT("name"), Id.ToString());
		const TSharedPtr<FJsonObject> Saved = WChild(WChild(Loaded, TEXT("shrines")), Id.ToString());
		bool bLit = bGreat, bEver = bGreat;
		if (Saved.IsValid()) { Saved->TryGetBoolField(TEXT("lit"), bLit); Saved->TryGetBoolField(TEXT("everLit"), bEver); }
		S->bLit = bGreat || bLit;
		S->bEverLit = bEver;
		S->ReguardAtHours = WNumOf(Saved, TEXT("reguardAt"), -1.f);
		if (!bGreat && S->bLit && S->ReguardAtHours >= 0.0 && Hours >= S->ReguardAtHours) S->bLit = false;
		S->FinishSpawning(FTransform(Face, Ground));
		Shrines.Add(Id, S);
	}
}

void UWRWorldSystems::PlaceMasters()
{
	UWorld* World = GetWorld();
	const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Data(), TEXT("masters"));
	if (!List) return;
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> Row = V->AsObject();
		// A master lives in their village beside its Great Shrine, or (no village: Vesna Groza at her climbers' camp in
		// Strikerock's split) at the row's own spot ("at", metres) and facing ("yaw").
		const FName OwnBiome(*WStrOf(Row, TEXT("biome")));
		if (!OwnBiome.IsNone() && !WRBiome::IsLive(OwnBiome)) continue;
		const TWeakObjectPtr<AWRVillage>* Vg = Villages.Find(FName(*WStrOf(Row, TEXT("village"))));
		const AWRVillage* Village = Vg && Vg->IsValid() ? Vg->Get() : nullptr;
		FVector SwampSpot = FVector::ZeroVector;
		float SwampYaw = 0.f;
		// Ruslana (swampDay / swampNight): her day place at the Tithe Mound; WRSwampSystems moves her at dusk and dawn.
		const bool bSwampPlace = WRSwamp::MasterPlace(Row, SwampSpot, SwampYaw);
		if (!Village && !Row->HasField(TEXT("at")) && !bSwampPlace) continue;
		FVector Spot;
		FRotator Face;
		const FName OnLand(*WStrOf(Row, TEXT("land")));
		bool bLandPlace = false;
		if (!OnLand.IsNone())
		{
			// On a runtime land (Maren Vey at the Shrine of the Edge): the land's map metres on its height function.
			const TArray<TSharedPtr<FJsonValue>>* AtArr = WArr(Row, TEXT("at"));
			if (!AtArr || AtArr->Num() < 2) continue;
			if (!WSkyIslandSpot(Row, (*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber(), Spot)
				&& !WRLand::PlaceOn(OnLand, FVector2D((*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber()), Spot)) continue;
			// Under the ground (Saga Venn on the Great Hexafoil's floor): the row's floor height, map metres.
			if (Row->HasField(TEXT("floor")))
			{
				if (const FWRLandDef* LD = WRLand::Find(OnLand)) Spot = LD->ToWorld(FVector2D((*AtArr)[0]->AsNumber(), (*AtArr)[1]->AsNumber()), WNumOf(Row, TEXT("floor"), 0.f));
			}
			Face = FRotator(0.f, WNumOf(Row, TEXT("yaw"), 0.f), 0.f);
			bLandPlace = true;
		}
		else if (bSwampPlace)
		{
			Spot = SwampSpot;
			Face = FRotator(0.f, SwampYaw, 0.f);
		}
		else if (Village)
		{
			const FVector2D Off = WMetres(Row->TryGetField(TEXT("offset"))) / 100.f;   // the row is in cm (beside the shrine), not metres
			Spot = Village->GetActorTransform().TransformPosition(FVector(Off.X, Off.Y, 0.f));
			Face = FRotator(0.f, Village->GetActorRotation().Yaw, 0.f);   // facing the work at their hands (the anvil, the hide frame), the shrine at their side
		}
		else
		{
			Spot = FVector(WMetres(Row->TryGetField(TEXT("at"))), 0.f);
			Face = FRotator(0.f, WNumOf(Row, TEXT("yaw"), 0.f), 0.f);
		}
		FVector Ground;
		if (!bLandPlace && WSettle(World, FVector2D(Spot), Ground)) Spot = Ground;
		AWRWorldNpc* N = World->SpawnActorDeferred<AWRWorldNpc>(AWRWorldNpc::StaticClass(), FTransform(Face, Spot + FVector(0.f, 0.f, 100.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!N) continue;
		N->NpcRole = TEXT("master");
		N->NpcId = FName(*WStrOf(Row, TEXT("id")));
		N->NpcName = WStrOf(Row, TEXT("name"));
		N->Biome = Village ? Village->Biome : OwnBiome;
		FName Armor[4], Elements[4];
		const TArray<TSharedPtr<FJsonValue>>* A = WArr(Row, TEXT("armor"));
		const TArray<TSharedPtr<FJsonValue>>* E = WArr(Row, TEXT("elements"));
		for (int32 i = 0; i < 4; ++i)
		{
			const FString Ai = A && A->IsValidIndex(i) ? (*A)[i]->AsString() : FString();
			const FString Ei = E && E->IsValidIndex(i) ? (*E)[i]->AsString() : FString();
			Armor[i] = Ai.IsEmpty() ? NAME_None : FName(*Ai);
			Elements[i] = Ei.IsEmpty() ? NAME_None : FName(*Ei);
		}
		N->SetLook(FName(*WStrOf(Row, TEXT("sex"), TEXT("m"))), int32(WNumOf(Row, TEXT("face"), 0.f)), int32(WNumOf(Row, TEXT("build"), 0.f)), Armor, Elements);
		N->WorkAt = Spot;
		N->WorkFacing = Face;
		N->WorkClip = FName(*WStrOf(Row, TEXT("work"), TEXT("chop")));
		N->WorkEvery = WNumOf(Row, TEXT("workEvery"), 3.f);
		N->WorkSound = FName(*WStrOf(Row, TEXT("workSound")));
		N->WorkFx = FName(*WStrOf(Row, TEXT("workFx")));
		N->FinishSpawning(FTransform(Face, Spot + FVector(0.f, 0.f, 100.f)));
		Masters.Add(N);
	}
}

void UWRWorldSystems::PlaceStones()
{
	// Summoning stones (endgame.md 2.8): beside each major's shrine-stone, at each mini's lair. A major's shrine-stone
	// comes with its boss, so this runs again until every boss has its stone.
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Eff = WRWorld::Obj(TEXT("summoning.effigies"));
	if (!Eff.IsValid()) return;
	TSet<FName> Have;
	for (TActorIterator<AWRSummoningStone> It(World); It; ++It) Have.Add(It->BossId);
	for (const auto& KV : Eff->Values)
	{
		const FName Boss(*KV.Key);
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Boss);
		if (Have.Contains(Boss) || !Def || !WRBiome::IsLive(Def->Biome)) continue;
		FVector Spot = FVector::ZeroVector;
		FRotator Face = FRotator::ZeroRotator;
		for (TActorIterator<AWRRunbackStone> It(World); It; ++It)
		{
			if (It->BossId != Boss) continue;
			// To the stone's right as you face the arena, so the two read as a pair at the approach.
			Spot = It->GetActorLocation() + It->GetActorRightVector() * WRWorld::Num(TEXT("summoning.sideOffset"), 600.f);
			Face = It->GetActorRotation();
		}
		if (Spot.IsZero())
		{
			FVector Lair = FVector::ZeroVector;
			if (Def->Roam.Num() > 0) Lair = Def->Roam[0];
			for (TActorIterator<AWRCreatureSpawner> It(World); It && Lair.IsZero(); ++It) if (It->CreatureId == Boss) Lair = It->GetActorLocation();
			if (Lair.IsZero()) continue;
			// A lair's stone sits toward the nearest camp: where you'd come from.
			FVector Camp = FVector::ZeroVector;
			float Best = TNumericLimits<float>::Max();
			for (const FWRPoi& P : FWRPoi::All) if (P.Kind == TEXT("camp") && FVector::Dist2D(P.Location, Lair) < Best) { Best = FVector::Dist2D(P.Location, Lair); Camp = P.Location; }
			const FVector Dir = (Camp - Lair).GetSafeNormal2D();
			FVector Ground;
			if (!WSettle(World, FVector2D(Lair + Dir * WRWorld::Num(TEXT("summoning.lairOffset"), 1200.f) * (Def->Roam.Num() > 0 ? 1.f : 2.f)), Ground)) continue;
			Spot = Ground;
			Face = FRotator(0.f, (-Dir).Rotation().Yaw, 0.f);
		}
		else
		{
			FVector Ground;
			if (WSettle(World, FVector2D(Spot), Ground)) Spot = Ground;
		}
		AWRSummoningStone* S = World->SpawnActorDeferred<AWRSummoningStone>(AWRSummoningStone::StaticClass(), FTransform(Face, Spot), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!S) continue;
		S->BossId = Boss;
		S->Biome = Def->Biome;
		S->FinishSpawning(FTransform(Face, Spot));
		UE_LOG(LogWildreach, Log, TEXT("WORLDSYS summoning stone for %s at %s"), *Boss.ToString(), *Spot.ToCompactString());
	}
}

void UWRWorldSystems::PlaceTubes()
{
	UWorld* World = GetWorld();
	const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Data(), TEXT("hidden"));
	if (!List) return;
	const TSharedPtr<FJsonObject> SavedCaches = WChild(Loaded, TEXT("caches"));
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> Row = V->AsObject();
		const FName Biome(*WStrOf(Row, TEXT("biome")));
		if (!WRBiome::IsLive(Biome)) continue;
		TArray<FVector> Route;
		if (const TArray<TSharedPtr<FJsonValue>>* R = WArr(Row, TEXT("route")))
		{
			for (const TSharedPtr<FJsonValue>& P : *R)
			{
				FVector G;
				if (WSettle(World, WMetres(P), G)) Route.Add(G);
			}
		}
		if (Route.Num() < 2) continue;
		AWRHiddenTube* Tube = World->SpawnActorDeferred<AWRHiddenTube>(AWRHiddenTube::StaticClass(), FTransform(Route[0]), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Tube->TubeId = FName(*WStrOf(Row, TEXT("id")));
		Tube->Route = Route;
		Tube->FinishSpawning(FTransform(Route[0]));
		// The rooms' contents at their route points, set into the floor beside the stream.
		const TArray<TSharedPtr<FJsonValue>>* Rooms = WArr(Row, TEXT("rooms"));
		for (int32 r = 0; Rooms && r < Rooms->Num(); ++r)
		{
			const TSharedPtr<FJsonObject> Room = (*Rooms)[r]->AsObject();
			const int32 At = FMath::Clamp(int32(WNumOf(Room, TEXT("at"), 0.f)), 0, Route.Num() - 1);
			const FVector C = Route[At];
			const FVector Along = (Route[FMath::Min(At + 1, Route.Num() - 1)] - Route[FMath::Max(At - 1, 0)]).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
			const FString RoomName = WStrOf(Room, TEXT("name"));
			for (const auto& KV : WCountsOf(WChild(Room, TEXT("creatures"))))
			{
				AWRCreatureSpawner* Sp = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), FTransform(C + Along * 300.f), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Sp) continue;
				Sp->CreatureId = KV.Key;
				Sp->Count = KV.Value;
				Sp->Radius = 250.f;
				Sp->RespawnSeconds = 1800.f;
				Sp->ActivationRadius = 9000.f;
				Sp->bTraceGround = true;
				Sp->FinishSpawning(FTransform(C + Along * 300.f));
			}
			int32 n = 0;
			for (const auto& KV : WCountsOf(WChild(Room, TEXT("ore"))))
			{
				for (int32 i = 0; i < KV.Value; ++i, ++n)
				{
					const FVector P = C + Side * (220.f * (n % 2 ? 1.f : -1.f)) + Along * (150.f * (n / 2) - 150.f);
					FHitResult G;
					FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTubeOre), false, Tube);
					const FVector Where = World->LineTraceSingleByChannel(G, P + FVector(0, 0, 400), P - FVector(0, 0, 2000), ECC_WorldStatic, Q) ? G.ImpactPoint : P;
					AWRResourceNode* Node = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (!Node) continue;
					Node->ItemId = KV.Key;
					Node->Hits = 6;
					Node->PerHit = 2;
					Node->RespawnSeconds = 2400.f;
					Node->FinishSpawning(FTransform(Node->GetActorRotation(), Where));
				}
			}
			const TSharedPtr<FJsonObject> Cache = WChild(Room, TEXT("cache"));
			if (Cache.IsValid() || Room->HasField(TEXT("lore")))
			{
				const FVector P = C - Side * 260.f + Along * 120.f;
				FHitResult G;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTubeCache), false, Tube);
				const FVector Where = World->LineTraceSingleByChannel(G, P + FVector(0, 0, 400), P - FVector(0, 0, 2000), ECC_WorldStatic, Q) ? G.ImpactPoint : P;
				AWRWorldCache* Box = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), FTransform(FRotator(0.f, Side.Rotation().Yaw, 0.f), Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Box)
				{
					bool bVault = false;
					Room->TryGetBoolField(TEXT("vault"), bVault);
					Box->Kind = bVault ? FName(TEXT("vault")) : FName(TEXT("chest"));
					Box->Biome = Biome;
					Box->Title = FString::Printf(TEXT("%s: %s"), *WStrOf(Row, TEXT("name")), *RoomName);
					Box->Items = WCountsOf(Cache);
					Box->Lore = WStrOf(Room, TEXT("lore"));
					Box->RefillHours = 96.f;
					const float Refill = WNumOf(SavedCaches, Box->Title, -1.f);
					if (Refill >= 0.f && WGameHours(World) < Refill) { Box->bTaken = true; Box->RefillAtHours = Refill; }
					Box->FinishSpawning(FTransform(FRotator(0.f, Side.Rotation().Yaw, 0.f), Where));
				}
				bool bVaultRoom = false;
				Room->TryGetBoolField(TEXT("vault"), bVaultRoom);
				if (bVaultRoom)
				{
					// The vault's slab between the corridor and its cache.
					const FVector D = (C - Side * 120.f);
					AWRVaultDoor* Door = World->SpawnActorDeferred<AWRVaultDoor>(AWRVaultDoor::StaticClass(), FTransform(FRotator(0.f, Side.Rotation().Yaw, 0.f), D), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (Door) { Door->Biome = Biome; Door->FinishSpawning(FTransform(FRotator(0.f, Side.Rotation().Yaw, 0.f), D)); }
				}
			}
		}
	}
}

AWRShrine* UWRWorldSystems::FindShrine(FName Id) const
{
	const TWeakObjectPtr<AWRShrine>* S = Shrines.Find(Id);
	return S ? S->Get() : nullptr;
}

void UWRWorldSystems::NoteFound(FName Shrine, const FString& Player)
{
	TSet<FString>& Set = FoundBy.FindOrAdd(Shrine);
	if (Set.Contains(Player)) return;
	Set.Add(Player);
	// Everyone this player shares with sees it too.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AWRPlayerState>() : nullptr;
		if (PS && FWRSocialRules::CanShare(Player, PS->GetPlayerName())) RefreshFound(PS);
	}
}

void UWRWorldSystems::RefreshFound(AWRPlayerState* PS)
{
	UWRQuestLog* Log = UWRQuestLog::For(PS);
	if (!Log) return;
	const FString Me = PS->GetPlayerName();
	TArray<FName> Found;
	for (const auto& KV : FoundBy)
	{
		for (const FString& Who : KV.Value)
		{
			if (Who == Me || FWRSocialRules::CanShare(Who, Me)) { Found.AddUnique(KV.Key); break; }
		}
	}
	// The Great Shrines are where everyone's first steps go: known from the start.
	for (const auto& KV : Shrines) if (KV.Value.IsValid() && KV.Value->bGreat) Found.AddUnique(KV.Key);
	Log->FoundShrines = Found;
}

void UWRWorldSystems::PlayerJoined(AWRPlayerState* PS)
{
	if (!bServer || !PS) return;
	RefreshFound(PS);
	UWRQuestLog* Log = UWRQuestLog::For(PS);
	if (Log && Log->AppraisedAway > 0)
	{
		const int32 N = Log->AppraisedAway;
		Log->AppraisedAway = 0;
		FTimerHandle T;
		const TWeakObjectPtr<AWRPlayerState> Weak = PS;
		GetWorld()->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([Weak, N]()
		{
			if (AWRPlayerController* PC = Weak.IsValid() ? WControllerOf(Weak.Get()) : nullptr) PC->ClientNotify(FString::Printf(TEXT("While you were away your messages were appraised %d %s"), N, N == 1 ? TEXT("time") : TEXT("times")));
		}), 8.f, false);
	}
}

void UWRWorldSystems::StepShrinesAndKindles(float Dt)
{
	StepClock += Dt;
	if (StepClock < 5.f) return;
	StepClock = 0.f;
	// Summoning stones whose bosses came later, sealed summons that rose, merchants' and craters' timers.
	PlaceStones();
	// A Great Shrine's first visit: its fire is always lit, so walking up its steps is the kindling (2,000 path XP once).
	for (const auto& KV : Shrines)
	{
		AWRShrine* S = KV.Value.Get();
		if (!S || !S->bGreat) continue;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
			UWRQuestLog* Log = UWRQuestLog::For(PS);
			if (!PS || !Log || !PC->GetPawn() || Log->LitByMe.Contains(S->ShrineId) || FVector::Dist(PC->GetPawn()->GetActorLocation(), S->GetActorLocation()) > 900.f) continue;
			Log->LitByMe.AddUnique(S->ShrineId);
			NoteFound(S->ShrineId, PS->GetPlayerName());
			const FName Path(*WStrOf(WRWorld::Biome(S->Biome), TEXT("path")));
			const FString Got = WPayPathXP(PS, Path, int32(WRWorld::Num(TEXT("shrines.xpGreatOwn"), 2000.f)), int32(WRWorld::Num(TEXT("shrines.xpGreatEach"), 1000.f)));
			PC->ClientNotify(FString::Printf(TEXT("%s: %s"), *S->DisplayName, *Got));
			WRSound::PlayUI(PC, TEXT("discover"));
			UE_LOG(LogWildreach, Log, TEXT("SHRINE %s first visit by %s"), *S->ShrineId.ToString(), *PS->GetPlayerName());
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = PendingSummons.Num() - 1; i >= 0; --i)
	{
		FSummon& S = PendingSummons[i];
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (It->CreatureId == S.Boss && It->IsAlive() && !Sealed.Contains(TWeakObjectPtr<AWRCreature>(*It)) && It->GetWorld()->GetTimeSeconds() - S.At < 120.0)
			{
				Sealed.Add(*It, S.Party);
				UE_LOG(LogWildreach, Log, TEXT("SUMMON %s rose for %s"), *S.Boss.ToString(), *FString::Join(S.Party, TEXT(", ")));
				S.At = -1.0;
				break;
			}
		}
		if (S.At < 0.0 || Now - S.At > 120.0) PendingSummons.RemoveAt(i);
	}
	for (auto It = Sealed.CreateIterator(); It; ++It) if (!It.Key().IsValid() || !It.Key()->IsAlive()) It.RemoveCurrent();
	// Caches that fill again.
	const double Hours = WGameHours(GetWorld());
	for (TActorIterator<AWRWorldCache> It(GetWorld()); It; ++It)
	{
		if (It->bTaken && It->RefillHours > 0.f && It->RefillAtHours >= 0.0 && Hours >= It->RefillAtHours) It->Refill();
	}
	// The vault.
	const bool bVault = VaultOpenUntilHours >= 0.0 && Hours < VaultOpenUntilHours;
	for (TActorIterator<AWRVaultDoor> It(GetWorld()); It; ++It)
	{
		if (It->bOpen != bVault) { It->bOpen = bVault; WRSound::Broadcast(*It, TEXT("world.vault")); }
	}
	// Messages: poor ones vanish.
	const int32 HideAt = int32(WRWorld::Num(TEXT("messages.hideAt"), -3.f));
	for (const TWeakObjectPtr<AWRMessageStone>& M : Messages) if (M.IsValid() && M->Good - M->Poor <= HideAt) M->Destroy();
	Messages.RemoveAll([](const TWeakObjectPtr<AWRMessageStone>& M) { return !M.IsValid() || M->IsActorBeingDestroyed(); });
	// Craters cool, and their glow draws the nearest hunters.
	for (int32 i = Craters.Num() - 1; i >= 0; --i)
	{
		FCrater& C = Craters[i];
		AWRWorldCache* Cache = C.Cache.Get();
		if (!Cache || Hours - C.LandedHours > WRWorld::Num(TEXT("events.starfall.craterHours"), 24.f))
		{
			if (Cache) Cache->Destroy();
			Craters.RemoveAt(i);
			continue;
		}
		if (!C.bDrew && Hours - C.LandedHours >= WRWorld::Num(TEXT("events.starfall.drawAfterHours"), 2.f) && !Cache->bTaken)
		{
			C.bDrew = true;
			const FName Kind = WKindOf(C.Biome, TEXT("midland"), true);
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
			for (int32 n = 0; Def && n < FMath::Clamp(Def->PackMax, 1, 3); ++n)
			{
				const FVector P = Cache->GetActorLocation() + FVector(FMath::RandPointInCircle(1800.f), 0.f);
				FVector G;
				if (WSettle(GetWorld(), FVector2D(P), G)) AWRCreature::SpawnCreature(GetWorld(), Kind, G + FVector(0.f, 0.f, Def->HalfHeight + 30.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
			}
		}
	}
}

void UWRWorldSystems::StepMasters(float Dt)
{
	Masters.RemoveAll([](const TWeakObjectPtr<AWRWorldNpc>& M) { return !M.IsValid(); });
	// Quench and sneak quests are checked here (the rest come from the game's own moments).
	static float Clock2 = 0.f;
	Clock2 += Dt;
	if (Clock2 < 0.5f) return;
	Clock2 = 0.f;
	UWorld* World = GetWorld();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AWRPlayerState>() : nullptr;
		AWRCharacter* Pawn = PS ? Cast<AWRCharacter>(PS->GetPawn()) : nullptr;
		UWRQuestLog* Log = UWRQuestLog::For(PS);
		if (!Pawn || !Log || !Pawn->IsAlive()) continue;
		for (FWRQuestRep& Q : Log->Quests)
		{
			if (!Q.bTaken) continue;
			const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Obj(TEXT("quests")), Q.Master.ToString());
			if (!List || !List->IsValidIndex(Q.Index)) continue;
			const TSharedPtr<FJsonObject> Row = (*List)[Q.Index]->AsObject();
			const FString Type = WStrOf(Row, TEXT("type"));
			const int32 Need = int32(WNumOf(Row, TEXT("count"), 1.f));
			if (Q.Progress >= Need) continue;
			if (Type == TEXT("quench"))
			{
				const UWRInventoryComponent* Bag = PS->GetInventory();
				const FWRItemStack& W = Bag->GetEquipped(EWREquipSlot::Weapon);
				if (!W.IsEmpty() && W.Element == FName(*WStrOf(Row, TEXT("element"))) && Pawn->GetCharacterMovement()->IsSwimming())
				{
					Q.Progress = Need;
					WRFx::Broadcast(Pawn, TEXT("reaction.steam"), Pawn->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("reaction.steam"));
					if (AWRPlayerController* PC = WControllerOf(PS)) PC->ClientNotify(TEXT("The blade hisses in the water. Take it to your master"));
				}
			}
			else if (Type == TEXT("sneak"))
			{
				const FName Kind(*WStrOf(Row, TEXT("creature")));
				const float Near = WRWorld::Num(TEXT("quests.sneakNear"), 500.f), Away = WRWorld::Num(TEXT("quests.sneakAway"), 3000.f);
				bool bClose = false, bAnyAwakeNear = false, bAsleepNear = false;
				for (TActorIterator<AWRCreature> C(World); C; ++C)
				{
					if (C->CreatureId != Kind || !C->IsAlive() || C->IsTamed()) continue;
					const float D = FVector::Dist(C->GetActorLocation(), Pawn->GetActorLocation());
					if (D < Near && C->IsAsleep()) bClose = true;
					if (D < Away) { if (C->IsAsleep()) bAsleepNear = true; else bAnyAwakeNear |= C->GetAggroTarget() == Pawn; }
				}
				if (Q.Stage == 0 && bClose) { Q.Stage = 1; if (AWRPlayerController* PC = WControllerOf(PS)) PC->ClientNotify(TEXT("Close enough to hear them breathe. Now back away")); }
				else if (Q.Stage == 1 && bAnyAwakeNear) { Q.Stage = 0; if (AWRPlayerController* PC = WControllerOf(PS)) PC->ClientNotify(TEXT("They woke. Try again")); }
				else if (Q.Stage == 1 && !bAsleepNear && !bAnyAwakeNear) { Q.Progress = Need; if (AWRPlayerController* PC = WControllerOf(PS)) PC->ClientNotify(TEXT("Out, and they never knew. Go back to Áilu")); }
			}
		}
	}
}

void UWRWorldSystems::QuestEvent(AWRPlayerState* PS, FName Type, FName What, int32 Count)
{
	UWRQuestLog* Log = UWRQuestLog::For(PS);
	if (!Log) return;
	for (FWRQuestRep& Q : Log->Quests)
	{
		if (!Q.bTaken) continue;
		const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Obj(TEXT("quests")), Q.Master.ToString());
		if (!List || !List->IsValidIndex(Q.Index)) continue;
		const TSharedPtr<FJsonObject> Row = (*List)[Q.Index]->AsObject();
		const FString QType = WStrOf(Row, TEXT("type"));
		const FString Status = WStrOf(Row, TEXT("status"));
		const int32 Need = int32(WNumOf(Row, TEXT("count"), 1.f));
		bool bCounts = false;
		if (QType == TEXT("kill"))
		{
			const FString Want = Status.IsEmpty() ? TEXT("kill") : TEXT("kill.") + Status;
			bCounts = Type.ToString() == Want && What == FName(*WStrOf(Row, TEXT("creature")));
		}
		else if (QType == TEXT("tame")) bCounts = Type == TEXT("tame") && What == FName(*WStrOf(Row, TEXT("creature")));
		// event: a land's own deed counted by its system (Storm peaks: rodroad = a rod touched on the rod-road, pretell =
		// a strike's warning lived through above 600 m, lantern = a winch hut's lantern lit; WRStormWorld.cpp).
		else if (QType == TEXT("event")) bCounts = Type == FName(*WStrOf(Row, TEXT("event")));
		else if (QType == TEXT("shrines") && Type == TEXT("shrine"))
		{
			// Counted from the shrines you've kindled yourself.
			int32 Have = 0;
			if (const TArray<TSharedPtr<FJsonValue>>* Want = WArr(Row, TEXT("shrines"))) for (const auto& W : *Want) Have += Log->LitByMe.Contains(FName(*W->AsString())) ? 1 : 0;
			if (Have != Q.Progress) { Q.Progress = Have; if (AWRPlayerController* PC = WControllerOf(PS)) PC->ClientNotify(FString::Printf(TEXT("%s: %d / %d"), *WStrOf(Row, TEXT("title")), FMath::Min(Have, Need), Need)); }
			continue;
		}
		if (!bCounts || Q.Progress >= Need) continue;
		Q.Progress = FMath::Min(Need, Q.Progress + Count);
		if (AWRPlayerController* PC = WControllerOf(PS))
		{
			PC->ClientNotify(Q.Progress >= Need ? FString::Printf(TEXT("%s: done. Go back to your master"), *WStrOf(Row, TEXT("title")))
				: FString::Printf(TEXT("%s: %d / %d"), *WStrOf(Row, TEXT("title")), Q.Progress, Need));
		}
	}
}

// ------------------------------------------------------------------------------------------------ fast travel

FString UWRWorldSystems::TravelBlock(const AWRPlayerController* PC, const AWRShrine* To)
{
	const AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	const UWRQuestLog* Log = UWRQuestLog::For(PC);
	if (!Me || !Log || !To) return TEXT("Not now");
	if (!Log->HasFound(To->ShrineId)) return TEXT("Not found yet");
	if (To->bHeld) return TEXT("Held by invaders");
	// From a lit shrine only (Dark Souls' bonfires, Valheim's portals: the road is part of the game).
	const AWRShrine* From = nullptr;
	const float Range = WRWorld::Num(TEXT("shrines.interactRange"), 450.f) * 2.f;
	for (TActorIterator<AWRShrine> It(Me->GetWorld()); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < Range) From = *It;
	if (!From) return TEXT("Travel from a lit shrine");
	if (From == To) return TEXT("You're here");
	if (!From->bLit || From->bHeld) return TEXT("This shrine is dark");
	if (Me->HasTag(WRTags::State_CombatTagged) || WInFight(Me)) return TEXT("In combat");
	if (Me->HasTag(WRTags::State_Mounted)) return TEXT("Get down from your mount");
	return FString();
}

FString UWRWorldSystems::Travel(AWRPlayerController* PC, FName To)
{
	AWRShrine* Dest = FindShrine(To);
	const FString Why = TravelBlock(PC, Dest);
	if (!Why.IsEmpty()) return Why;
	AWRCharacter* Me = Cast<AWRCharacter>(PC->GetPawn());
	const FString Name = PC->PlayerState->GetPlayerName();
	TArray<TWeakObjectPtr<AWRCreature>> Tames;
	const float R = WRWorld::Num(TEXT("shrines.travelTameRadius"), 3000.f);
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && It->TamedBy == Name && !It->Rider.IsValid() && It->TameOrder == EWRTameOrder::Follow && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < R) Tames.Add(*It);
	}
	const float Fade = WRWorld::Num(TEXT("shrines.travelFade"), 1.2f);
	PC->ClientSetCameraFade(true, FColor::Black, FVector2D(0.f, 1.f), Fade, true, true);
	WRSound::PlayUI(PC, TEXT("world.travel"));
	WRFx::Broadcast(Me, TEXT("discover"), Me->GetActorLocation(), FRotator::ZeroRotator, 1.5f);
	const TWeakObjectPtr<AWRPlayerController> WeakPC = PC;
	const TWeakObjectPtr<AWRShrine> WeakDest = Dest;
	FTimerHandle T;
	GetWorld()->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WeakPC, WeakDest, Tames, Fade]()
	{
		AWRPlayerController* P = WeakPC.Get();
		AWRShrine* S = WeakDest.Get();
		APawn* Pawn = P ? P->GetPawn() : nullptr;
		if (!P || !S || !Pawn) return;
		// Arrive on the steps facing the bowl.
		const FVector Front = S->GetActorLocation() + S->GetActorForwardVector() * (S->bGreat ? 700.f : 560.f);
		const FRotator Face(0.f, (S->GetActorLocation() - Front).Rotation().Yaw, 0.f);
		Pawn->TeleportTo(Front + FVector(0.f, 0.f, 120.f), Face);
		P->SetControlRotation(Face);
		int32 n = 0;
		for (const TWeakObjectPtr<AWRCreature>& W : Tames)
		{
			AWRCreature* C = W.Get();
			if (!C) continue;
			const FVector Spot = Front + FRotator(0.f, Face.Yaw + 120.f + 40.f * n++, 0.f).Vector() * 450.f;
			C->TeleportTo(Spot + FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() + 60.f), Face);
		}
		P->ClientSetCameraFade(true, FColor::Black, FVector2D(1.f, 0.f), Fade, true, false);
		UE_LOG(LogWildreach, Log, TEXT("TRAVEL %s -> %s with %d tames"), *P->PlayerState->GetPlayerName(), *S->DisplayName, n);
	}), Fade + 0.15f, false);
	return FString::Printf(TEXT("Travelling to %s"), *Dest->DisplayName);
}

// ------------------------------------------------------------------------------------------------ messages

FString UWRWorldSystems::PlaceMessage(AWRPlayerController* PC, const TArray<int32>& Picks)
{
	// Picks: template, word category, word, conjunction (-1 none), template 2, category 2, word 2.
	AWRCharacter* Me = Cast<AWRCharacter>(PC->GetPawn());
	AWRPlayerState* PS = PC->GetPlayerState<AWRPlayerState>();
	if (!Me || !PS || !Me->IsAlive()) return TEXT("Not now");
	if (PS->GetInventory()->CountOf(TEXT("scratch_stone")) <= 0) return TEXT("You need a Scratch-stone");
	if (FWRSocialRules::IsInSafeZone(GetWorld(), Me->GetActorLocation()) && WRBiome::IsUnderground(GetWorld(), Me->GetActorLocation())) return TEXT("Not here");
	const TSharedPtr<FJsonObject> M = WRWorld::Obj(TEXT("messages"));
	const TArray<TSharedPtr<FJsonValue>>* Templates = WArr(M, TEXT("templates"));
	const TArray<TSharedPtr<FJsonValue>>* Conj = WArr(M, TEXT("conjunctions"));
	const TSharedPtr<FJsonObject> Words = WChild(M, TEXT("words"));
	if (!Templates || !Words.IsValid() || Picks.Num() < 3) return TEXT("That message can't be cut");
	TArray<FString> Cats;
	for (const auto& KV : Words->Values) Cats.Add(FString(KV.Key));
	const auto Clause = [&](int32 T, int32 C, int32 W, FString& Out) -> bool
	{
		if (!Templates->IsValidIndex(T) || !Cats.IsValidIndex(C)) return false;
		const TArray<TSharedPtr<FJsonValue>>* List = WArr(Words, Cats[C]);
		if (!List || !List->IsValidIndex(W)) return false;
		Out = (*Templates)[T]->AsString().Replace(TEXT("****"), *(*List)[W]->AsString());
		return true;
	};
	FString Text, Second;
	if (!Clause(Picks[0], Picks[1], Picks[2], Text)) return TEXT("That message can't be cut");
	if (Picks.Num() >= 7 && Picks[3] >= 0 && Conj && Conj->IsValidIndex(Picks[3]) && Clause(Picks[4], Picks[5], Picks[6], Second))
	{
		const FString C = (*Conj)[Picks[3]]->AsString();
		Text = C == TEXT(",") ? Text + TEXT(", ") + Second : Text + TEXT(" ") + C + TEXT(" ") + Second;
	}
	// The oldest of yours crumbles past the limit.
	const FString Name = PS->GetPlayerName();
	TArray<AWRMessageStone*> Mine;
	for (const TWeakObjectPtr<AWRMessageStone>& W : Messages) if (W.IsValid() && W->Author == Name) Mine.Add(W.Get());
	Mine.Sort([](const AWRMessageStone& A, const AWRMessageStone& B) { return A.PlacedAt < B.PlacedAt; });
	const int32 Max = int32(WRWorld::Num(TEXT("messages.maxPerPlayer"), 10.f));
	for (int32 i = 0; i <= Mine.Num() - Max; ++i)
	{
		WRFx::Broadcast(Mine[i], TEXT("hit.dust"), Mine[i]->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("world.carve"));
		Mine[i]->Destroy();
	}
	// At your feet, a step ahead, flat on the ground.
	const FVector Ahead = Me->GetActorLocation() + Me->GetActorForwardVector().GetSafeNormal2D() * 110.f;
	FHitResult G;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRMessage), false, Me);
	if (!GetWorld()->LineTraceSingleByChannel(G, Ahead + FVector(0, 0, 100), Ahead - FVector(0, 0, 400), ECC_WorldStatic, Q)) return TEXT("No ground here for a stone");
	const FRotator Face(0.f, Me->GetActorRotation().Yaw + 180.f, 0.f);   // its words face you as you cut them, and whoever comes the same way
	AWRMessageStone* Stone = GetWorld()->SpawnActorDeferred<AWRMessageStone>(AWRMessageStone::StaticClass(), FTransform(Face, G.ImpactPoint), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Stone->Text = Text;
	Stone->Author = Name;
	Stone->Biome = WRBiome::At(GetWorld(), G.ImpactPoint);
	Stone->PlacedAt = Clock;
	Stone->FinishSpawning(FTransform(Face, G.ImpactPoint));
	Messages.Add(Stone);
	Me->MulticastBodyOneShot(TEXT("gather"));
	WRFx::Broadcast(Stone, TEXT("hit.dust"), G.ImpactPoint, FRotator::ZeroRotator, 1.f, TEXT("world.carve"));
	UE_LOG(LogWildreach, Log, TEXT("MESSAGE %s: \"%s\" at %s"), *Name, *Text, *G.ImpactPoint.ToCompactString());
	return TEXT("Your message is cut in the stone");
}

// ------------------------------------------------------------------------------------------------ events

bool UWRWorldSystems::StartEvent(FName Id, bool bNow)
{
	const TSharedPtr<FJsonObject> Row = WRWorld::Obj(TEXT("events.") + Id.ToString());
	if (!Row.IsValid() || Runs.ContainsByPredicate([Id](const FEventRun& R) { return R.Id == Id; })) return false;
	const FName Biome(*WStrOf(Row, TEXT("biome")));
	if (!Biome.IsNone() && !WRBiome::IsLive(Biome)) return false;
	FEventRun R;
	R.Id = Id;
	R.Biome = Biome;
	const double Now = WServerNow(GetWorld());
	R.WarnAt = Now;
	R.StartAt = Now + (bNow ? 5.0 : WRWorld::Num(TEXT("events.warnSeconds"), 300.f));
	R.EndAt = R.StartAt + WNumOf(Row, TEXT("end"), 3600.f);
	R.bForced = bNow;
	if (const TSharedPtr<FJsonObject> B = WRWorld::Biome(Biome))
	{
		const FVector2D C = WMetres(B->TryGetField(TEXT("centre")));
		FVector G;
		R.Centre = WSettle(GetWorld(), C, G) ? G : FVector(C, 0.f);
	}
	if (Id == TEXT("starfall"))
	{
		R.StartAt = Now + WRWorld::Num(TEXT("events.starfall.streakSeconds"), 300.f) * (bNow ? 0.1f : 1.f);
		R.EndAt = R.StartAt + 60.0;
		const FVector2D M = WRangeOf(Row, TEXT("meteors"), FVector2D(3, 5));
		R.Meteors = FMath::RandRange(int32(M.X), int32(M.Y));
		WTellBiome(GetWorld(), NAME_None, WStrOf(Row, TEXT("warn"), TEXT("Stars are falling")), TEXT("world.event"));
	}
	else
	{
		WTellBiome(GetWorld(), Biome, WStrOf(Row, TEXT("warn")), TEXT("world.event"));
	}
	Runs.Add(R);
	UE_LOG(LogWildreach, Log, TEXT("EVENT %s warning (%s): starts in %.0f s"), *Id.ToString(), *Biome.ToString(), R.StartAt - Now);
	SyncEventRep();
	return true;
}

void UWRWorldSystems::StepEvents(float Dt)
{
	UWorld* World = GetWorld();
	const double Now = WServerNow(World);
	const TSharedPtr<FJsonObject> Ev = WRWorld::Obj(TEXT("events"));
	// The schedule, on played time.
	if (Ev.IsValid())
	{
		for (const auto& KV : Ev->Values)
		{
			const TSharedPtr<FJsonObject> Row = KV.Value->AsObject();
			if (!Row.IsValid() || !Row->HasField(TEXT("everyHours"))) continue;
			const FName Id(*KV.Key);
			const bool bWorld = !Row->HasField(TEXT("biome"));
			const FString Setting = WSettingOf(bWorld ? TEXT("worldEvents") : TEXT("biomeEvents"));
			if (Setting == TEXT("off")) continue;
			const double Every = WNumOf(Row, TEXT("everyHours"), 72.f) * 3600.0 * WEveryScale(Setting);
			double& Next = NextEventAt.FindOrAdd(Id, -1.0);
			if (Next < 0.0) Next = Clock + Every * FMath::FRandRange(0.6f, 1.4f);
			if (Id == TEXT("starfall"))
			{
				// Waits for dusk once it's due.
				const bool bNight = AWRGameState::IsNightIn(World);
				if (Clock >= Next && bNight && !bWasNight && WAnyPlayerIn(World, NAME_None))
				{
					int32 WorldOn = 0;
					for (const FEventRun& R : Runs) WorldOn += R.Biome.IsNone() ? 1 : 0;
					if (WorldOn < int32(WRWorld::Num(TEXT("events.maxWorld"), 1.f)) && StartEvent(Id, false)) Next = Clock + Every * FMath::FRandRange(0.7f, 1.3f);
				}
				bWasNight = bNight;
				continue;
			}
			if (Clock < Next) continue;
			const FName Biome(*WStrOf(Row, TEXT("biome")));
			int32 BiomeOn = 0;
			for (const FEventRun& R : Runs) BiomeOn += R.Biome.IsNone() ? 0 : 1;
			if (BiomeOn >= int32(WRWorld::Num(TEXT("events.maxBiome"), 2.f)) || !WAnyPlayerIn(World, Biome)) { Next = Clock + 600.0; continue; }
			if (StartEvent(Id, false)) Next = Clock + Every * FMath::FRandRange(0.7f, 1.3f);
		}
	}
	for (int32 i = Runs.Num() - 1; i >= 0; --i)
	{
		FEventRun& R = Runs[i];
		if (Now >= R.StartAt && R.NextTremor == 0.0)
		{
			// It starts.
			R.NextTremor = Now;
			const TSharedPtr<FJsonObject> Row = WRWorld::Obj(TEXT("events.") + R.Id.ToString());
			const FString Weather = WStrOf(Row, TEXT("weather"));
			if (!Weather.IsEmpty())
			{
				if (UWRWeatherSubsystem* W = World->GetSubsystem<UWRWeatherSubsystem>()) W->Force(R.Biome, FName(*Weather), float(R.EndAt - R.StartAt) / 60.f);
			}
			if (R.Id != TEXT("starfall")) WTellBiome(World, R.Biome, WStrOf(Row, TEXT("startText")), TEXT("world.event"));
			if (R.Id == TEXT("eruption"))
			{
				// The plume stands over the crater for the whole event: seen island-wide.
				WRFx::Broadcast(World, TEXT("eruption.plume"), R.Centre + FVector(0.f, 0.f, 3000.f), FRotator::ZeroRotator, 1.f, TEXT("world.bomb_launch"), float(R.EndAt - R.StartAt));
				VaultOpenUntilHours = WGameHours(World) + WNumOf(Row, TEXT("vaultHours"), 24.f);
			}
			UE_LOG(LogWildreach, Log, TEXT("EVENT %s on"), *R.Id.ToString());
			SyncEventRep();
		}
		if (Now >= R.StartAt)
		{
			if (R.Id == TEXT("eruption")) StepEruption(R, Now);
			else if (R.Id == TEXT("starfall")) StepStarfall(R, Now);
		}
		if (Now >= R.EndAt)
		{
			EndEvent(R);
			Runs.RemoveAt(i);
			SyncEventRep();
		}
	}
	// The aurora night ends at dawn.
	if (AWRWorldState* State = AWRWorldState::Get(World))
	{
		static bool bWasNightAur = false;
		const bool bNight = AWRGameState::IsNightIn(World);
		if (bWasNightAur && !bNight && State->AuroraBiomes.Num() > 0) State->AuroraBiomes.Reset();
		bWasNightAur = bNight;
	}
}

void UWRWorldSystems::StepEruption(FEventRun& R, double Now)
{
	const TSharedPtr<FJsonObject> Row = WRWorld::Obj(TEXT("events.eruption"));
	const double T = Now - R.StartAt;
	const FVector2D Fire = WRangeOf(Row, TEXT("fire"), FVector2D(600, 3000));
	if (T < Fire.X || T > Fire.Y) return;
	UWorld* World = GetWorld();
	const float Range = WNumOf(Row, TEXT("bombRange"), 120000.f);
	const FVector2D Interval = WRangeOf(Row, TEXT("bombInterval"), FVector2D(8, 14));
	const FVector2D Offset = WRangeOf(Row, TEXT("bombOffset"), FVector2D(300, 1200));
	const float Safe = WNumOf(Row, TEXT("safeRadius"), 5000.f);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(P);
		if (!C || !C->IsAlive() || FVector::Dist2D(P->GetActorLocation(), R.Centre) > Range) continue;
		double& Next = R.NextBomb.FindOrAdd(P, Now + FMath::FRandRange(1.f, Interval.Y));
		if (Now < Next) continue;
		Next = Now + FMath::FRandRange(Interval.X, Interval.Y);
		if (WRBiome::IsUnderground(World, P->GetActorLocation())) continue;
		// Never in a camp or village (or near one).
		bool bSafe = false;
		for (TActorIterator<AWRSafeZone> Z(World); Z && !bSafe; ++Z) bSafe = FVector::Dist2D(Z->GetActorLocation(), P->GetActorLocation()) < Z->Radius + Safe;
		if (bSafe) continue;
		const FVector Aim = P->GetActorLocation() + FVector(FMath::RandPointInCircle(1.f).GetSafeNormal() * FMath::FRandRange(Offset.X, Offset.Y), 0.f);
		FHitResult G;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBombAim), false, P);
		if (!World->LineTraceSingleByChannel(G, Aim + FVector(0, 0, 3000), Aim - FVector(0, 0, 5000), ECC_WorldStatic, Q)) continue;
		// From the crater's side, high: the arc comes over the land from the volcano.
		const FVector Back = (R.Centre - G.ImpactPoint).GetSafeNormal2D();
		const FVector From = G.ImpactPoint + Back * FMath::Min(30000.f, FVector::Dist2D(R.Centre, G.ImpactPoint) * 0.4f) + FVector(0.f, 0.f, 6000.f);
		AWRLavaBomb* Bomb = World->SpawnActorDeferred<AWRLavaBomb>(AWRLavaBomb::StaticClass(), FTransform(From), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Bomb) continue;
		Bomb->From = From;
		Bomb->To = G.ImpactPoint;
		Bomb->LaunchAt = float(Now);
		Bomb->Travel = WNumOf(Row, TEXT("bombTravel"), 1.2f);
		Bomb->FinishSpawning(FTransform(From));
	}
}

void UWRWorldSystems::StepStarfall(FEventRun& R, double Now)
{
	const double Each = 6.0;
	while (R.Meteors > 0 && Now >= R.StartAt + (double(FMath::Max(0, 5 - R.Meteors)) * Each))
	{
		--R.Meteors;
		// A biome with someone in it, else any live one.
		TArray<FName> Biomes;
		if (const TSharedPtr<FJsonObject> B = WRWorld::Obj(TEXT("biomes"))) for (const auto& KV : B->Values) if (WRBiome::IsLive(FName(*KV.Key))) Biomes.Add(FName(*KV.Key));
		if (Biomes.Num() == 0) return;
		DropMeteor(Biomes[FMath::RandHelper(Biomes.Num())]);
	}
}

void UWRWorldSystems::DropMeteor(FName Biome)
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Row = WRWorld::Obj(TEXT("events.starfall"));
	const float Clear = WNumOf(Row, TEXT("clearOfRadius"), 40000.f);
	const FName Mid = WKindOf(Biome, TEXT("midland"), false);
	const FName High = WKindOf(Biome, TEXT("highland"), false);
	for (int32 Try = 0; Try < 60; ++Try)
	{
		// Anywhere on a land in the game: the island, or (once it's live) Storm peaks' frame to the north.
		const bool bNorth = WRBiome::IsLive(TEXT("storm_peaks")) && FMath::FRand() < WRWorld::Num(TEXT("events.starfall.northShare"), 0.35f);
		FVector2D At = bNorth ? FVector2D(FMath::FRandRange(-260000.f, 60000.f), FMath::FRandRange(-560000.f, -230000.f))
			: FVector2D(FMath::FRandRange(-180000.f, 180000.f), FMath::FRandRange(-180000.f, 180000.f));
		// The Ancient forest's star falls on its own frame south of the island (Data/forest.json; its ground below).
		if (Biome == TEXT("forest"))
		{
			const FWRForestData& FD = FWRForestData::Get();
			if (!FD.bLoaded) return;
			At = FVector2D(FMath::FRandRange(FD.FrameMin.X, FD.FrameMax.X), FMath::FRandRange(WRTune(TEXT("world.islandSouthEdge"), 201600.f), FD.FrameMax.Y));
		}
		if (!(Mid.IsNone() ? false : WRWildlife::CanStand(Mid, At)) && !(High.IsNone() ? false : WRWildlife::CanStand(High, At))) continue;
		bool bNear = false;
		for (const FWRPoi& P : FWRPoi::All) bNear |= FVector::Dist2D(P.Location, FVector(At, 0.f)) < Clear && P.Kind != TEXT("cave");
		for (TActorIterator<AWRClaimTotem> It(World); It && !bNear; ++It) bNear = FVector::Dist2D(It->GetActorLocation(), FVector(At, 0.f)) < Clear;
		for (TActorIterator<AWRShrine> It(World); It && !bNear; ++It) bNear = FVector::Dist2D(It->GetActorLocation(), FVector(At, 0.f)) < Clear;
		for (TActorIterator<AWRVillage> It(World); It && !bNear; ++It) bNear = FVector::Dist2D(It->GetActorLocation(), FVector(At, 0.f)) < Clear;
		if (bNear) continue;
		FVector G;
		if (!WSettle(World, At, G) || WRBiome::At(World, G) != Biome) continue;
		const FVector From = G + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 60000.f + FVector(0.f, 0.f, 90000.f);
		AWRLavaBomb* M = World->SpawnActorDeferred<AWRLavaBomb>(AWRLavaBomb::StaticClass(), FTransform(From), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!M) return;
		M->From = From;
		M->To = G;
		M->LaunchAt = WServerNow(World);
		M->Travel = WNumOf(Row, TEXT("fallSeconds"), 2.4f);
		M->bMeteor = true;
		M->FinishSpawning(FTransform(From));
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->GetPawn()) PC->ClientNotify(WStrOf(Row, TEXT("landText"), TEXT("A star fell to the {dir}")).Replace(TEXT("{dir}"), *WCompass(PC->GetPawn()->GetActorLocation(), G)));
		}
		UE_LOG(LogWildreach, Log, TEXT("STARFALL a star falls at %s (%s)"), *G.ToCompactString(), *Biome.ToString());
		return;
	}
}

void UWRWorldSystems::EndEvent(FEventRun& R)
{
	const TSharedPtr<FJsonObject> Row = WRWorld::Obj(TEXT("events.") + R.Id.ToString());
	if (R.Id != TEXT("starfall")) WTellBiome(GetWorld(), R.Biome, WStrOf(Row, TEXT("endText")), TEXT("world.event"));
	if (R.Id == TEXT("whiteout"))
	{
		bool bAurora = false;
		if (Row.IsValid()) Row->TryGetBoolField(TEXT("auroraNight"), bAurora);
		if (bAurora) if (AWRWorldState* State = AWRWorldState::Get(GetWorld())) State->AuroraBiomes.AddUnique(R.Biome);
	}
	UE_LOG(LogWildreach, Log, TEXT("EVENT %s over"), *R.Id.ToString());
}

void UWRWorldSystems::SyncEventRep()
{
	AWRWorldState* State = AWRWorldState::Get(GetWorld());
	if (!State) return;
	const double Now = WServerNow(GetWorld());
	State->Events.Reset();
	for (const FEventRun& R : Runs)
	{
		FWRWorldEventRep& E = State->Events.AddDefaulted_GetRef();
		E.Id = R.Id;
		E.Biome = R.Biome;
		E.Stage = Now >= R.StartAt ? 1 : 0;
		E.StartAt = float(R.StartAt);
		E.EndAt = float(R.EndAt);
		E.Centre = R.Centre;
	}
	State->ForceNetUpdate();
}

// ------------------------------------------------------------------------------------------------ invasions

bool UWRWorldSystems::StartInvasion(int32 PairIndex, bool bNow)
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Root = WRWorld::Obj(TEXT("invasions"));
	const TArray<TSharedPtr<FJsonValue>>* Pairs = WArr(Root, TEXT("pairs"));
	if (Inv.bOn || !Pairs || !Pairs->IsValidIndex(PairIndex)) return false;
	const TSharedPtr<FJsonObject> Pair = (*Pairs)[PairIndex]->AsObject();
	const FName From(*WStrOf(Pair, TEXT("from"))), To(*WStrOf(Pair, TEXT("to")));
	if (!WRBiome::IsLive(From) || !WRBiome::IsLive(To)) return false;
	TArray<const FWRPoi*> Passes;
	for (const FWRPoi& P : FWRPoi::All) if (P.Kind == TEXT("pass")) Passes.Add(&P);
	if (Passes.Num() == 0) return false;
	const FWRPoi* Pass = Passes[FMath::RandHelper(Passes.Num())];
	const FVector2D FromC = WMetres(WRWorld::Biome(From)->TryGetField(TEXT("centre")));
	const FVector2D ToC = WMetres(WRWorld::Biome(To)->TryGetField(TEXT("centre")));
	const float Inset = WNumOf(Root, TEXT("passInset"), 150.f) * 100.f;
	FVector A, D;
	// A pair that names its own pass (Stormgut between Storm peaks and the Swamp): its mouths, metres, each side's own.
	FString PassName = Pass->Name;
	if (const TSharedPtr<FJsonObject> Mouths = WChild(Pair, TEXT("mouths")))
	{
		const FVector2D MA = WMetres(Mouths->TryGetField(From.ToString())), MD = WMetres(Mouths->TryGetField(To.ToString()));
		if (!MA.IsZero() && !MD.IsZero())
		{
			if (!WSettle(World, MA, A) || !WSettle(World, MD, D)) return false;
			PassName = WStrOf(Pair, TEXT("passName"), PassName);
		}
		else if (!WSettle(World, FVector2D(Pass->Location) + FVector2D(FMath::Sign(FromC.X), 0.f) * Inset, A) || !WSettle(World, FVector2D(Pass->Location) + FVector2D(FMath::Sign(ToC.X), 0.f) * Inset, D)) return false;
	}
	else if (!WSettle(World, FVector2D(Pass->Location) + FVector2D(FMath::Sign(FromC.X), 0.f) * Inset, A) || !WSettle(World, FVector2D(Pass->Location) + FVector2D(FMath::Sign(ToC.X), 0.f) * Inset, D)) return false;
	FInvasion N;
	N.bOn = true;
	N.From = From;
	N.To = To;
	N.Pass = PassName;
	N.AttackMouth = A;
	N.DefendMouth = D;
	// What they march on: the nearest claim or lesser shrine within reach of the pass's far mouth.
	float Best = WNumOf(Root, TEXT("targetRange"), 150000.f);
	N.Target = D + (FVector(ToC, D.Z) - D).GetSafeNormal2D() * 30000.f;
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
	{
		const float Dist = FVector::Dist2D(It->GetActorLocation(), D);
		if (Dist < Best && WRBiome::At(World, It->GetActorLocation()) == To) { Best = Dist; N.Target = It->GetActorLocation(); N.TargetShrine = NAME_None; }
	}
	for (const auto& KV : Shrines)
	{
		const AWRShrine* S = KV.Value.Get();
		if (!S || S->bGreat || S->Biome != To) continue;
		const float Dist = FVector::Dist2D(S->GetActorLocation(), D);
		if (Dist < Best) { Best = Dist; N.Target = S->GetActorLocation(); N.TargetShrine = KV.Key; }
	}
	// Who comes: by the hour, rolled to 8-14 by weight.
	const bool bNight = AWRGameState::IsNightIn(World);
	const TMap<FName, int32> Weights = WCountsOf(WChild(Pair, bNight ? TEXT("night") : TEXT("day")));
	const FVector2D Range = WRangeOf(Root, TEXT("count"), FVector2D(8, 14));
	int32 Want = FMath::RandRange(int32(Range.X), int32(Range.Y));
	int32 Total = 0;
	for (const auto& KV : Weights) Total += KV.Value;
	TMap<FName, int32> Picked;
	for (int32 i = 0; i < Want && Total > 0; ++i)
	{
		int32 Roll2 = FMath::RandRange(1, Total);
		for (const auto& KV : Weights) { if ((Roll2 -= KV.Value) <= 0) { Picked.FindOrAdd(KV.Key) += 1; break; } }
	}
	for (const auto& KV : Picked) N.Kinds.Add({ KV.Key, KV.Value });
	N.Leader = FName(*WStrOf(Pair, TEXT("leader")));
	N.Stage = 0;
	N.StageEnd = WServerNow(World) + (bNow ? 10.0 : WNumOf(Root, TEXT("warnSeconds"), 300.f));
	Inv = N;
	InvasionLedger.Reset();
	Invaders.Reset();
	const FString FromName = WStrOf(WRWorld::Biome(From), TEXT("name"), From.ToString());
	const FString Msg = FString::Printf(TEXT("Invasion: %s is coming through the %s"), *FromName, *PassName.ToLower());
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
		if (!PC || !PC->GetPawn() || WRBiome::At(World, PC->GetPawn()->GetActorLocation()) != To) continue;
		PC->ClientRaid(Inv.Kinds.Num() ? Inv.Kinds[0].Key : NAME_None, FString::Printf(TEXT("%s invades"), *FromName), Msg, 0);
		WRSound::PlayUI(PC, TEXT("world.invasion"));
	}
	UE_LOG(LogWildreach, Log, TEXT("INVASION %s -> %s through %s, %d kinds, target %s"), *From.ToString(), *To.ToString(), *PassName, Inv.Kinds.Num(), *Inv.Target.ToCompactString());
	return true;
}

void UWRWorldSystems::StepInvasion(float Dt)
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Root = WRWorld::Obj(TEXT("invasions"));
	const FString Setting = WSettingOf(TEXT("invasions"));
	const double Now = WServerNow(World);
	if (!Inv.bOn)
	{
		if (Setting == TEXT("off")) return;
		const float PerWeek = WNumOf(WChild(Root, TEXT("perWeek")), Setting, 2.f);
		if (PerWeek <= 0.f) return;
		const TArray<TSharedPtr<FJsonValue>>* Pairs = WArr(Root, TEXT("pairs"));
		for (int32 i = 0; Pairs && i < Pairs->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> Pair = (*Pairs)[i]->AsObject();
			// Both lands must be in the game (the Storm <-> Swamp pair waits for the Swamp).
			if (!WRBiome::IsLive(FName(*WStrOf(Pair, TEXT("from")))) || !WRBiome::IsLive(FName(*WStrOf(Pair, TEXT("to"))))) continue;
			const FString Key = WStrOf(Pair, TEXT("from")) + TEXT(">") + WStrOf(Pair, TEXT("to"));
			const double Every = 7.0 * 24.0 * 3600.0 / PerWeek;
			double& Next = NextInvasionAt.FindOrAdd(Key, -1.0);
			if (Next < 0.0) Next = Clock + Every * FMath::FRandRange(0.5f, 1.2f);
			if (Clock < Next) continue;
			if (!WAnyPlayerIn(World, FName(*WStrOf(Pair, TEXT("to"))))) { Next = Clock + 600.0; continue; }
			if (StartInvasion(i, false)) { Next = Clock + Every * FMath::FRandRange(0.8f, 1.2f); break; }
		}
		return;
	}
	AWRWorldState* State = AWRWorldState::Get(World);
	if (Inv.Stage == 0 && Now >= Inv.StageEnd)
	{
		// They come out at the attacking mouth and march through to the far one first (the pass is the only way over).
		const FRotator Facing(0.f, (Inv.DefendMouth - Inv.AttackMouth).Rotation().Yaw, 0.f);
		const auto Spawn = [&](FName Kind)
		{
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
			if (!Def) return;
			FVector G;
			if (!WSettle(World, FVector2D(Inv.AttackMouth) + FMath::RandPointInCircle(900.f), G)) G = Inv.AttackMouth;
			AWRCreature* C = AWRCreature::SpawnCreature(World, Kind, G + FVector(0.f, 0.f, Def->HalfHeight + 40.f), Facing);
			if (!C) return;
			C->bRaider = true;
			C->RaidBase = Inv.DefendMouth;
			C->RaidRadius = 2500.f;
			C->RaidOwner = TEXT("~invasion");
			C->SetHome(Inv.DefendMouth);
			Invaders.Add(C);
		};
		for (const TPair<FName, int32>& K : Inv.Kinds) for (int32 i = 0; i < K.Value; ++i) Spawn(K.Key);
		// The roaming mini leaves its loop to lead them (its own body if it's out; otherwise it rises at the head).
		AWRCreature* Leader = nullptr;
		for (TActorIterator<AWRCreature> It(World); It; ++It) if (It->CreatureId == Inv.Leader && It->IsAlive()) Leader = *It;
		// The Bloat leaves its loop by its own road (WRSwampBosses: it walks out through Stormgut to the target, the pack
		// behind it); it counts as an invader for the ledger and the end.
		if (Inv.Leader == TEXT("bloat") && FWRSwampBoss::LeadInvasion(World, Inv.Target))
		{
			if (AWRBoss* Bloat = FWRSwampBoss::Bloat(World)) Invaders.Add(Bloat);
		}
		else if (Leader)
		{
			Leader->TeleportTo(Inv.AttackMouth + FVector(0.f, 0.f, Leader->GetSimpleCollisionHalfHeight() + 60.f), Facing);
			Leader->bRaider = true;
			Leader->RaidBase = Inv.DefendMouth;
			Leader->RaidRadius = 2500.f;
			Leader->RaidOwner = TEXT("~invasion");
			Leader->SetHome(Inv.DefendMouth);
			Invaders.Add(Leader);
		}
		else
		{
			Spawn(Inv.Leader);
		}
		Inv.Stage = 1;
		Inv.StageEnd = Now + WNumOf(Root, TEXT("holdSeconds"), 1800.f) + 600.0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->GetPawn() && WRBiome::At(World, PC->GetPawn()->GetActorLocation()) == Inv.To) PC->ClientRaid(NAME_None, FString(), FString::Printf(TEXT("They're through the %s"), *Inv.Pass.ToLower()), 1);
		}
	}
	if (Inv.Stage >= 1)
	{
		int32 Left = 0;
		bool bAtTarget = false;
		for (const TWeakObjectPtr<AWRCreature>& W : Invaders)
		{
			AWRCreature* C = W.Get();
			if (!C || !C->IsAlive()) continue;
			++Left;
			// Through the pass: on to the target.
			if (C->RaidBase.Equals(Inv.DefendMouth, 1.f) && FVector::Dist2D(C->GetActorLocation(), Inv.DefendMouth) < 3000.f)
			{
				C->RaidBase = Inv.Target;
				C->SetHome(Inv.Target);
				C->RaidRadius = Inv.TargetShrine.IsNone() ? 3000.f : 1200.f;
				for (TActorIterator<AWRClaimTotem> T(World); T; ++T) if (FVector::Dist2D(T->GetActorLocation(), Inv.Target) < 100.f) { C->RaidOwner = T->OwnerName; C->RaidRadius = T->Radius; }
			}
			bAtTarget |= FVector::Dist2D(C->GetActorLocation(), Inv.Target) < 2500.f;
		}
		if (Inv.Stage == 1 && bAtTarget)
		{
			Inv.Stage = 2;
			Inv.StageEnd = Now + WNumOf(Root, TEXT("holdSeconds"), 1800.f);
			if (AWRShrine* S = FindShrine(Inv.TargetShrine)) { S->bHeld = true; S->ForceNetUpdate(); }
			WTellBiome(World, Inv.To, Inv.TargetShrine.IsNone() ? FString(TEXT("The invaders have reached their target")) : FString::Printf(TEXT("The invaders hold the %s: travel there is shut"), *FindShrine(Inv.TargetShrine)->DisplayName), NAME_None);
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->GetPawn() && WRBiome::At(World, PC->GetPawn()->GetActorLocation()) == Inv.To)
			{
				PC->ClientRaidTrack(uint8(FMath::Min(Inv.Stage, 2)), uint8(FMath::Min(Left, 255)), uint8(FMath::Min(Invaders.Num(), 255)), float(Inv.StageEnd - Now), Inv.Pass);
			}
		}
		if (Left == 0) EndInvasion(true);
		else if (Now >= Inv.StageEnd) EndInvasion(false);
	}
	if (State)
	{
		FWRInvasionRep Rep;
		Rep.bOn = Inv.bOn;
		Rep.From = Inv.From; Rep.To = Inv.To; Rep.Pass = Inv.Pass; Rep.Stage = uint8(Inv.Stage);
		Rep.Until = float(Inv.StageEnd); Rep.HeldShrine = Inv.Stage == 2 ? Inv.TargetShrine : NAME_None; Rep.Target = Inv.Target;
		int32 Left = 0;
		for (const auto& W : Invaders) Left += W.IsValid() && W->IsAlive() ? 1 : 0;
		Rep.Left = uint8(FMath::Min(Left, 255)); Rep.Total = uint8(FMath::Min(Invaders.Num(), 255));
		State->Invasion = Rep;
	}
}

void UWRWorldSystems::EndInvasion(bool bWon)
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Root = WRWorld::Obj(TEXT("invasions"));
	if (AWRShrine* S = FindShrine(Inv.TargetShrine)) { S->bHeld = false; S->ForceNetUpdate(); }
	if (bWon)
	{
		// Everyone who dealt 10% of their total health is paid.
		float TotalHealth = 0.f;
		for (const TPair<FName, int32>& K : Inv.Kinds) if (const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(K.Key)) TotalHealth += D->Health * K.Value;
		if (const FWRCreatureDef* L = FWRCreatureRegistry::Get().Find(Inv.Leader)) TotalHealth += L->Health;
		const float Need = TotalHealth * WNumOf(Root, TEXT("lootFraction"), 0.1f);
		const FName ToPath(*WStrOf(WRWorld::Biome(Inv.To), TEXT("path")));
		const FName Faction(*WStrOf(WRWorld::Biome(Inv.To), TEXT("faction")));
		// A pair may name its own pay (the Severance: both sides' essences, crystal 2.22); else the shared list.
		TMap<FName, int32> Pays = WCountsOf(WChild(Root, TEXT("pays")));
		if (const TArray<TSharedPtr<FJsonValue>>* Pairs = WArr(Root, TEXT("pairs")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Pairs)
			{
				const TSharedPtr<FJsonObject> Pair = V->AsObject();
				if (Pair.IsValid() && FName(*WStrOf(Pair, TEXT("from"))) == Inv.From && FName(*WStrOf(Pair, TEXT("to"))) == Inv.To && WChild(Pair, TEXT("pays")).IsValid()) Pays = WCountsOf(WChild(Pair, TEXT("pays")));
			}
		}
		const int32 XP = int32(WNumOf(Root, TEXT("xp"), 2000.f));
		for (const auto& KV : InvasionLedger)
		{
			AWRPlayerState* PS = WStateOf(KV.Key, World);
			AWRPlayerController* PC = WControllerOf(PS);
			if (!PS) continue;
			if (KV.Value < Need) { if (PC) PC->ClientNotify(TEXT("The invasion is beaten: you didn't do enough of it for a share")); continue; }
			FString Got = WPayPathXP(PS, ToPath, XP, XP / 2);
			for (const auto& P : Pays) { PS->GetInventory()->AddItem(P.Key, P.Value); Got += FString::Printf(TEXT(", %s x%d"), *WRWorld::ItemName(P.Key), P.Value); }
			if (UWRQuestLog* Log = UWRQuestLog::For(PS)) Log->AddStanding(Faction, int32(WRWorld::Num(TEXT("factions.perDefence"), 200.f)));
			if (PC) PC->ClientRaid(NAME_None, FString(), FString::Printf(TEXT("Invasion beaten: %s"), *Got), 2);
		}
	}
	else
	{
		WTellBiome(World, Inv.To, TEXT("The invaders go back through the pass"), NAME_None);
	}
	// The Bloat goes home along its own road (WRSwampBosses).
	if (Inv.Leader == TEXT("bloat")) FWRSwampBoss::EndInvasion(World);
	for (const TWeakObjectPtr<AWRCreature>& W : Invaders)
	{
		AWRCreature* C = W.Get();
		if (!C || !C->IsAlive()) continue;
		if (C->CreatureId == Inv.Leader)
		{
			C->bRaider = false;   // back to its loop
			continue;
		}
		C->bRaidLeaving = true;
		C->RaidBase = Inv.AttackMouth;
		C->SetLifeSpan(120.f);
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get())) PC->ClientRaidTrack(2, 0, 0, 0.f, Inv.Pass);
	}
	UE_LOG(LogWildreach, Log, TEXT("INVASION over: %s"), bWon ? TEXT("beaten") : TEXT("they went back"));
	Inv = FInvasion();
	Invaders.Reset();
	if (AWRWorldState* State = AWRWorldState::Get(World)) State->Invasion = FWRInvasionRep();
}

// ------------------------------------------------------------------------------------------------ caravans

FVector UWRWorldSystems::CaravanAt(const FCaravan& C, double Hours, bool& bStopped, int32& OutStop, FVector& OutHeading) const
{
	// The loop's length in game hours: each leg at the caravan's speed, then its stop.
	const float Speed = WRWorld::Num(TEXT("caravan.speed"), 300.f);   // cm/s
	const AWRGameState* GS = AWRGameState::Get(GetWorld());
	const float DayMinutes = FMath::Max(1.f, WRTune(TEXT("server.dayMinutes"), WRTune(TEXT("clock.dayMinutes"), 60.f)));
	const double SecondsPerHour = DayMinutes * 60.0 / 24.0;
	const double Stop = WRWorld::Num(TEXT("caravan.stopHours"), 1.f);
	TArray<double> Legs;
	double Loop = 0.0;
	for (int32 i = 0; i < C.Stops.Num(); ++i)
	{
		const double LegHours = FVector::Dist(C.Stops[i], C.Stops[(i + 1) % C.Stops.Num()]) / Speed / SecondsPerHour;
		Legs.Add(LegHours);
		Loop += Stop + LegHours;
	}
	double T = FMath::Fmod(FMath::Max(0.0, Hours - C.StartHours), FMath::Max(Loop, 0.01));
	for (int32 i = 0; i < C.Stops.Num(); ++i)
	{
		const FVector A = C.Stops[i], B = C.Stops[(i + 1) % C.Stops.Num()];
		if (T < Stop) { bStopped = true; OutStop = i; OutHeading = (B - A).GetSafeNormal2D(); return A; }
		T -= Stop;
		if (T < Legs[i])
		{
			bStopped = false;
			OutStop = i;
			OutHeading = (B - A).GetSafeNormal2D();
			return FMath::Lerp(A, B, float(T / FMath::Max(Legs[i], 0.0001)));
		}
		T -= Legs[i];
	}
	bStopped = true;
	OutStop = 0;
	OutHeading = FVector::ForwardVector;
	return C.Stops[0];
}

void UWRWorldSystems::StepCaravans(float Dt)
{
	UWorld* World = GetWorld();
	const double Hours = WGameHours(World);
	const float SpawnRange = WRWorld::Num(TEXT("caravan.spawnRange"), 15000.f);
	const float Despawn = WRWorld::Num(TEXT("caravan.despawnRange"), 25000.f);
	const TArray<TSharedPtr<FJsonValue>>* Rows = WArr(WRWorld::Data(), TEXT("caravans"));
	for (FCaravan& C : Caravans)
	{
		const TSharedPtr<FJsonObject>* RowPtr = nullptr;
		TSharedPtr<FJsonObject> Row;
		for (int32 i = 0; Rows && i < Rows->Num(); ++i) if (WStrOf((*Rows)[i]->AsObject(), TEXT("id")) == C.Id.ToString()) Row = (*Rows)[i]->AsObject();
		if (!Row.IsValid()) continue;
		(void)RowPtr;
		AWRWorldNpc* M = C.Merchant.Get();
		if (M && !M->IsAlive() && M->HasVitals())
		{
			// Killed on the road: it's gone until the next game day, then sets out from home again.
			C.DeadUntilHours = FMath::FloorToDouble(Hours / 24.0) * 24.0 + 24.0;
			C.StartHours = C.DeadUntilHours;
			C.Merchant = nullptr;
			if (AWRWorldNpc* O = C.Outrider.Get()) O->Destroy();
			continue;
		}
		if (C.DeadUntilHours >= 0.0 && Hours < C.DeadUntilHours) continue;
		bool bStopped = false;
		int32 Stop = 0;
		FVector Heading;
		const FVector Where = CaravanAt(C, Hours, bStopped, Stop, Heading);
		bool bNear = false, bFar = true;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (!P) continue;
			const float D = FVector::Dist2D(P->GetActorLocation(), M ? M->GetActorLocation() : Where);
			bNear |= D < SpawnRange;
			bFar &= D > Despawn;
		}
		if (!M && bNear)
		{
			FVector G;
			if (!WSettle(World, FVector2D(Where), G)) continue;
			const auto Make = [&](const TSharedPtr<FJsonObject>& Look, FName NewRole, const FVector& At) -> AWRWorldNpc*
			{
				const FRotator Face(0.f, Heading.Rotation().Yaw, 0.f);
				AWRWorldNpc* N = World->SpawnActorDeferred<AWRWorldNpc>(AWRWorldNpc::StaticClass(), FTransform(Face, At + FVector(0.f, 0.f, 100.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
				if (!N) return nullptr;
				N->NpcRole = NewRole;
				N->NpcId = C.Id;
				N->NpcName = NewRole == TEXT("merchant") ? WStrOf(Row, TEXT("name")) : FString::Printf(TEXT("%s's outrider"), *WStrOf(Row, TEXT("name")).Left(WStrOf(Row, TEXT("name")).Find(TEXT(" "))));
				N->Biome = FName(*WStrOf(Row, TEXT("biome")));
				const FName Elem = N->Biome == TEXT("volcanic") ? FName(TEXT("fire")) : FName(TEXT("frost"));
				const FName Armor[4] = { NAME_None, TEXT("hide_armor"), TEXT("hide_hands"), TEXT("hide_legs") };
				const FName Elements[4] = { NAME_None, Elem, NAME_None, Elem };
				N->SetLook(FName(*WStrOf(Look, TEXT("sex"), TEXT("m"))), int32(WNumOf(Look, TEXT("face"), 0.f)), int32(WNumOf(Look, TEXT("build"), 0.f)), Armor, Elements);
				N->FinishSpawning(FTransform(Face, At + FVector(0.f, 0.f, 100.f)));
				return N;
			};
			C.Merchant = Make(Row, TEXT("merchant"), G);
			FVector G2;
			const FVector Beside = G - Heading * 350.f + FVector::CrossProduct(FVector::UpVector, Heading) * 180.f;
			C.Outrider = Make(WChild(Row, TEXT("outrider")), TEXT("outrider"), WSettle(World, FVector2D(Beside), G2) ? G2 : G);
			M = C.Merchant.Get();
			UE_LOG(LogWildreach, Log, TEXT("CARAVAN %s on the road at %s (%s, stop %d %s)"), *C.Id.ToString(), *G.ToCompactString(), bStopped ? TEXT("stopped") : TEXT("walking"), Stop, C.StopNames.IsValidIndex(Stop) ? *C.StopNames[Stop] : TEXT(""));
		}
		else if (M && bFar)
		{
			M->Destroy();
			if (AWRWorldNpc* O = C.Outrider.Get()) O->Destroy();
			C.Merchant = nullptr;
			C.Outrider = nullptr;
			continue;
		}
		if (!M) continue;
		// Walk where the loop says it should be (a fight or a hold-up lets it fall behind; it catches up).
		const bool bFighting = WInFight(M);
		M->bTrading = bStopped && !bFighting;
		M->bWalking = !bStopped && !bFighting;
		M->WalkTo = bStopped ? C.Stops[Stop] : Where + Heading * 600.f;
		if (FVector::Dist2D(M->GetActorLocation(), Where) > 6000.f && !bNear)
		{
			M->TeleportTo(Where + FVector(0.f, 0.f, 120.f), Heading.Rotation());
		}
		if (AWRWorldNpc* O = C.Outrider.Get())
		{
			O->bWalking = M->bWalking || FVector::Dist2D(O->GetActorLocation(), M->GetActorLocation()) > 600.f;
			O->WalkTo = M->GetActorLocation() - M->GetActorForwardVector() * 300.f + M->GetActorRightVector() * 180.f;
		}
	}
}

// ------------------------------------------------------------------------------------------------ commands

FString WRWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRWorldSystems* Sys = UWRWorldSystems::Get(PC);
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!Sys || !Sys->IsServerRunning() || !PS) return TEXT("Not in this world");
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString Verb = Arg(1);
	UWRQuestLog* Log = UWRQuestLog::For(PS);
	const auto Near = [Me](const AActor* A, float R) { return Me && A && FVector::Dist(Me->GetActorLocation(), A->GetActorLocation()) < R; };

	if (Verb == TEXT("kindle"))
	{
		AWRShrine* S = Sys->FindShrine(FName(*Arg(2)));
		if (!S || !Near(S, WRWorld::Num(TEXT("shrines.interactRange"), 450.f) * 1.5f)) return TEXT("No shrine here");
		return S->StartKindle(PC);
	}
	if (Verb == TEXT("travel"))
	{
		return Sys->Travel(PC, FName(*Arg(2)));
	}
	if (Verb == TEXT("take") || Verb == TEXT("hand"))
	{
		// Quests with a master you stand by.
		const FName Master(*Arg(2));
		AWRWorldNpc* N = nullptr;
		for (TActorIterator<AWRWorldNpc> It(PC->GetWorld()); It; ++It) if (It->NpcId == Master && It->NpcRole == TEXT("master")) N = *It;
		if (!N || !Near(N, WRWorld::Num(TEXT("quests.talkRange"), 400.f) * 1.5f) || !Log) return TEXT("They're not here");
		const TArray<TSharedPtr<FJsonValue>>* List = WArr(WRWorld::Obj(TEXT("quests")), Master.ToString());
		FWRQuestRep& Q = Log->QuestMutable(Master);
		if (!List || !List->IsValidIndex(Q.Index)) return TEXT("Nothing more to learn here");
		const TSharedPtr<FJsonObject> Row = (*List)[Q.Index]->AsObject();
		const FString Title = WStrOf(Row, TEXT("title"));
		if (Verb == TEXT("take"))
		{
			if (Q.bTaken) return TEXT("You have it already");
			Q.bTaken = true;
			Q.Progress = 0;
			Q.Stage = 0;
			if (WStrOf(Row, TEXT("type")) == TEXT("shrines")) Sys->QuestEvent(PS, TEXT("shrine"), NAME_None);   // shrines already lit count
			WRSound::PlayUI(PC, TEXT("recipe.unlock"));
			return FString::Printf(TEXT("New task: %s"), *Title);
		}
		if (!Q.bTaken) return TEXT("Take the task first");
		const FString Type = WStrOf(Row, TEXT("type"));
		const int32 Need = int32(WNumOf(Row, TEXT("count"), 1.f));
		if ((Type == TEXT("bring") || Type == TEXT("show")) && Row->HasField(TEXT("need")))
		{
			// Several things at once ("need": item -> count; "a|b" takes either: a trophy or the talisman made of it).
			// Yara Kesh's first and last (cooks/biome-sky.md 2.13).
			TArray<TPair<FName, int32>> Take;
			for (const auto& KV : Row->GetObjectField(TEXT("need"))->Values)
			{
				const int32 Want = int32(KV.Value->AsNumber());
				TArray<FString> Alts;
				FString(*KV.Key).ParseIntoArray(Alts, TEXT("|"));
				if (Alts.Num() == 0) continue;
				FName Have;
				for (const FString& A : Alts) if (PS->GetInventory()->CountOf(FName(*A)) >= Want) { Have = FName(*A); break; }
				if (Have.IsNone()) return FString::Printf(TEXT("Bring %s x%d (you have %d)"), *WRWorld::ItemName(FName(*Alts[0])), Want, PS->GetInventory()->CountOf(FName(*Alts[0])));
				Take.Emplace(Have, Want);
			}
			if (Type == TEXT("bring")) for (const auto& T : Take) PS->GetInventory()->RemoveItem(T.Key, T.Value);
			Q.Progress = Need;
		}
		else if (Type == TEXT("bring") || Type == TEXT("show"))
		{
			const FName Item(*WStrOf(Row, TEXT("item")));
			if (PS->GetInventory()->CountOf(Item) < Need) return FString::Printf(TEXT("Bring %s x%d (you have %d)"), *WRWorld::ItemName(Item), Need, PS->GetInventory()->CountOf(Item));
			if (Type == TEXT("bring")) PS->GetInventory()->RemoveItem(Item, Need);
			Q.Progress = Need;
		}
		if (Q.Progress < Need) return FString::Printf(TEXT("Not done yet: %d / %d"), Q.Progress, Need);
		// Paid.
		const FName Path(*([&]() { FString P; for (const auto& V : *WArr(WRWorld::Data(), TEXT("masters"))) if (WStrOf(V->AsObject(), TEXT("id")) == Master.ToString()) P = WStrOf(V->AsObject(), TEXT("path")); return P; })());
		const int32 XP = int32(WNumOf(Row, TEXT("xp"), 0.f));
		TArray<FString> Got;
		// A master's quests pay their own path (Saga Venn's into the Arcane path); a survivor who doesn't walk it is paid
		// the shrine way: a share into each path they do walk (world.json questOtherPathShare), never lost.
		if (XP > 0 && !Path.IsNone())
		{
			const FString Paid = WPayPathXP(PS, Path, XP, FMath::Max(1, FMath::RoundToInt(XP * WNumOf(WRWorld::Data(), TEXT("questOtherPathShare"), 0.5f))));
			if (!Paid.IsEmpty()) Got.Add(Paid);
		}
		for (const auto& KV : WCountsOf(WChild(Row, TEXT("items")))) { PS->GetInventory()->AddItem(KV.Key, KV.Value); Got.Add(FString::Printf(TEXT("%s x%d"), *WRWorld::ItemName(KV.Key), KV.Value)); }
		if (const TArray<TSharedPtr<FJsonValue>>* Rec = WArr(Row, TEXT("recipes")))
		{
			for (const auto& R : *Rec)
			{
				const FName Bp(*(TEXT("quest_") + R->AsString()));
				PS->KnownBlueprints.AddUnique(Bp);
				Got.Add(FString::Printf(TEXT("recipe: %s"), *WRWorld::ItemName(FName(*R->AsString()))));
			}
			WRSound::PlayUI(PC, TEXT("blueprint.learn"));
		}
		const FName Faction(*WStrOf(WRWorld::Biome(N->Biome), TEXT("faction")));
		if (Log) Log->AddStanding(Faction, int32(WRWorld::Num(TEXT("quests.standing"), 150.f)));
		Q.Index += 1;
		Q.bTaken = false;
		Q.Progress = 0;
		Q.Stage = 0;
		N->MulticastSound(TEXT("levelup"));
		WRDeeds::OnQuestStep(PS, Row);   // a step's title (the masters' last steps), the deeds
		UE_LOG(LogWildreach, Log, TEXT("QUEST %s handed in %s to %s: %s"), *PS->GetPlayerName(), *Title, *Master.ToString(), *FString::Join(Got, TEXT(", ")));
		return FString::Printf(TEXT("%s done: %s"), *Title, *FString::Join(Got, TEXT(", ")));
	}
	if (Verb == TEXT("trade"))
	{
		// world trade <caravan> <offer index>
		AWRWorldNpc* N = nullptr;
		for (TActorIterator<AWRWorldNpc> It(PC->GetWorld()); It; ++It) if (It->NpcId == FName(*Arg(2)) && It->NpcRole == TEXT("merchant")) N = *It;
		if (!N || !Near(N, 600.f) || !N->bTrading) return TEXT("The merchant isn't trading here");
		const TArray<TSharedPtr<FJsonValue>>* Rows = WArr(WRWorld::Data(), TEXT("caravans"));
		FString Table;
		for (int32 i = 0; Rows && i < Rows->Num(); ++i) if (WStrOf((*Rows)[i]->AsObject(), TEXT("id")) == Arg(2)) Table = WStrOf((*Rows)[i]->AsObject(), TEXT("trade"));
		const TArray<TSharedPtr<FJsonValue>>* Offers = WArr(WRWorld::Obj(TEXT("trade")), Table);
		const int32 Index = FCString::Atoi(*Arg(3));
		if (!Offers || !Offers->IsValidIndex(Index)) return TEXT("No such offer");
		const TSharedPtr<FJsonObject> Offer = (*Offers)[Index]->AsObject();
		const FName Faction(*WStrOf(WRWorld::Biome(N->Biome), TEXT("faction")));
		const int32 Tier = Log ? Log->TierOf(Faction) : 0;
		if (Tier < int32(WNumOf(Offer, TEXT("tier"), 0.f))) return TEXT("They don't trust you with that yet");
		const TArray<TSharedPtr<FJsonValue>>* Tiers = WArr(WRWorld::Obj(TEXT("factions")), TEXT("tiers"));
		const float Discount = Tiers && Tiers->IsValidIndex(Tier) ? WNumOf((*Tiers)[Tier]->AsObject(), TEXT("discount"), 0.f) : 0.f;
		TMap<FName, int32> Give = WCountsOf(WChild(Offer, TEXT("give")));
		for (auto& KV : Give) KV.Value = FMath::Max(1, FMath::CeilToInt(KV.Value * (1.f - Discount)));
		for (const auto& KV : Give) if (PS->GetInventory()->CountOf(KV.Key) < KV.Value) return FString::Printf(TEXT("You need %s x%d"), *WRWorld::ItemName(KV.Key), KV.Value);
		for (const auto& KV : Give) PS->GetInventory()->RemoveItem(KV.Key, KV.Value);
		TArray<FString> Got;
		for (const auto& KV : WCountsOf(WChild(Offer, TEXT("get"))))
		{
			const int32 Left = PS->GetInventory()->AddItem(KV.Key, KV.Value);
			if (Left > 0)
			{
				TArray<FWRItemStack> Spill;
				Spill.Add(FWRInventoryRules::MakeInstance(KV.Key));
				Spill.Last().Count = Left;
				AWRLootBag::Spill(PC->GetWorld(), Me->GetActorLocation(), PS->GetPlayerName(), Spill);
			}
			Got.Add(FString::Printf(TEXT("%s x%d"), *WRWorld::ItemName(KV.Key), KV.Value));
		}
		if (Log) Log->AddStanding(Faction, int32(WRWorld::Num(TEXT("factions.perTrade"), 5.f)));
		WRSound::Broadcast(N, TEXT("world.trade"));
		return FString::Printf(TEXT("Traded for %s"), *FString::Join(Got, TEXT(", ")));
	}
	if (Verb == TEXT("msg"))
	{
		TArray<int32> Picks;
		for (int32 i = 2; i < Args.Num(); ++i) Picks.Add(FCString::Atoi(*Args[i]));
		return Sys->PlaceMessage(PC, Picks);
	}
	if (Verb == TEXT("rate"))
	{
		// world rate x y z good|poor
		const FVector At(FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)), FCString::Atof(*Arg(4)));
		AWRMessageStone* Stone = nullptr;
		for (const auto& W : Sys->Messages) if (W.IsValid() && FVector::Dist(W->GetActorLocation(), At) < 50.f) Stone = W.Get();
		if (!Stone || !Near(Stone, 600.f)) return TEXT("No message here");
		const FString Name = PS->GetPlayerName();
		if (Stone->Author == Name) return TEXT("You can't rate your own message");
		if (Stone->RatedBy.Contains(Name)) return TEXT("You've rated it already");
		Stone->RatedBy.Add(Name);
		if (Arg(5) == TEXT("good"))
		{
			++Stone->Good;
			// The author, if they're here and alive, feels it.
			AWRPlayerState* Author = WStateOf(Stone->Author, PC->GetWorld());
			AWRCharacterBase* Body = Author ? Cast<AWRCharacterBase>(Author->GetPawn()) : nullptr;
			if (Body && Body->IsAlive())
			{
				WHealBy(Body, WRWorld::Num(TEXT("messages.healFraction"), 0.2f));
				if (AWRPlayerController* APC = WControllerOf(Author)) { APC->ClientNotify(TEXT("Your message was appraised")); WRSound::PlayUI(APC, TEXT("world.appraise")); }
				WRFx::Broadcast(Body, TEXT("potion.heal"), Body->GetActorLocation(), FRotator::ZeroRotator, 1.f);
			}
			else if (UWRQuestLog* AL = Author ? UWRQuestLog::For(Author) : nullptr) AL->AppraisedAway += 1;
			return TEXT("You appraised it");
		}
		++Stone->Poor;
		return TEXT("You rated it poor");
	}
	if (Verb == TEXT("summon"))
	{
		AWRSummoningStone* S = nullptr;
		for (TActorIterator<AWRSummoningStone> It(PC->GetWorld()); It; ++It) if (It->BossId == FName(*Arg(2))) S = *It;
		if (!S || !Near(S, 500.f)) return TEXT("No summoning stone here");
		return S->Offer(PC);
	}
	if (Verb == TEXT("swamp"))
	{
		// The Swamp's things (WRSwampSystems): world swamp use / plank / rite / boss ...
		return WRSwamp::Command(PC, Args);
	}
	if (Verb == TEXT("fhaz"))
	{
		// The Ancient forest's hazards and secrets (WRForestHazards, WRForestHollow): world fhaz use / secret x y z.
		return WRForestHazards::Command(PC, Args);
	}
	if (Verb == TEXT("sky"))
	{
		// The Sky's world (WRSkyWorld): world sky shard | lift | line | spot; the Last Circle: world sky boss grab | shift.
		if (Args.Num() > 2 && Args[2] == TEXT("boss"))
		{
			TArray<FString> Sub(Args);
			Sub.RemoveAt(0);
			if (UWRSkyArenas* A = UWRSkyArenas::Get(PC)) return A->Command(PC, Sub);
			return FString();
		}
		return UWRSkyWorld::Command(PC, Args);
	}
	if (Verb == TEXT("crystal"))
	{
		// The Crystal caves (WRCrystalThings): world crystal spot.
		return UWRCrystalWorld::Command(PC, Args);
	}
	if (Verb == TEXT("grave"))
	{
		// The Graveyard's world (WRGraveWorld): world grave spot <name>.
		return UWRGraveWorld::Command(PC, Args);
	}
	if (Verb == TEXT("forest"))
	{
		// The Ancient forest's world (WRForestWorld): world forest lift / offer.
		return UWRForestWorld::Command(PC, Args);
	}
	if (Verb == TEXT("plank"))
	{
		// Storm peaks' rope bridges: lay a new plank in the gap you stand at (cooks/biome-lightning.md 2.5).
		const APawn* Walker = PC->GetPawn();
		int32 Plank = INDEX_NONE;
		AWRStormBridge* B = Walker && UWRStormWorld::Get(PC) ? UWRStormWorld::Get(PC)->BridgeGapNear(Walker->GetActorLocation(), Plank) : nullptr;
		return B ? B->Mend(PC, Plank) : TEXT("No gap to mend here");
	}
	if (Verb == TEXT("open"))
	{
		const FVector At(FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)), FCString::Atof(*Arg(4)));
		for (TActorIterator<AWRWorldCache> It(PC->GetWorld()); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), At) < 50.f && Near(*It, 500.f)) return It->Take(PC);
		}
		return TEXT("Nothing here");
	}
#if !UE_BUILD_SHIPPING
	if (Verb == TEXT("dev")) return Sys->Dev(PC, Args);
#endif
	return TEXT("?");
}

FString UWRWorldSystems::Dev(AWRPlayerController* PC, const TArray<FString>& Args)
{
	// world dev event <eruption|whiteout|starfall> | invasion [0|1] | shrine <id> lit|dark | found all | quest <master> <index> | caravan here | meteor
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString What = Arg(2);
	if (What == TEXT("event")) return StartEvent(FName(*Arg(3)), true) ? TEXT("event started") : TEXT("can't start that");
	if (What == TEXT("invasion")) return StartInvasion(FCString::Atoi(*Arg(3)), true) ? TEXT("invasion coming") : TEXT("can't");
	if (What == TEXT("meteor")) { DropMeteor(WRBiome::At(PC, PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector)); return TEXT("a star falls"); }
	if (What == TEXT("found"))
	{
		for (const auto& KV : Shrines) NoteFound(KV.Key, PC->PlayerState->GetPlayerName());
		return TEXT("every shrine found");
	}
	if (What == TEXT("shrine"))
	{
		AWRShrine* S = FindShrine(FName(*Arg(3)));
		if (!S) return TEXT("no such shrine");
		S->bLit = Arg(4) != TEXT("dark");
		S->ReguardAtHours = S->bLit ? WGameHours(PC) + 96.0 : -1.0;
		return S->bLit ? TEXT("lit") : TEXT("dark");
	}
	if (What == TEXT("quest"))
	{
		if (UWRQuestLog* Log = UWRQuestLog::For(PC)) { FWRQuestRep& Q = Log->QuestMutable(FName(*Arg(3))); Q.Index = FCString::Atoi(*Arg(4)); Q.bTaken = false; Q.Progress = 0; }
		return TEXT("quest set");
	}
	if (What == TEXT("caravan"))
	{
		for (FCaravan& C : Caravans) { C.DeadUntilHours = -1.0; C.StartHours = WGameHours(PC) - 0.01; }
		return TEXT("caravans reset to their homes");
	}
	return TEXT("world dev event <id> | invasion <pair> | meteor | found | shrine <id> lit|dark | quest <master> <n> | caravan");
}

FString WRWorld::UseScratchStone(AWRPlayerState* PS)
{
	AWRPlayerController* PC = WControllerOf(PS);
	if (!PC) return FString();
	PC->ClientOpenWorldScreen(TEXT("msgwrite"), nullptr);
	return TEXT("Choose what to cut into the stone");
}
