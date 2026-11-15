// The people of the world (cooks/build-world-systems.md 2.2, 2.4): path masters at their villages' Great Shrines and
// the traveling merchants with their outriders, on the hero's body (MetaHuman rig, motion-matched clips). Also the
// client's side of the world systems: what E does near a world thing, the prompt, and opening their screens.

#include "WRWorldSystems.h"
#include "WRFungalWorld.h"
#include "WROceanWorld.h"
#include "WRStormWorld.h"
#include "WRSwampSystems.h"
#include "WRForestHazards.h"
#include "WRForestWorld.h"
#include "WRGraveWorld.h"
#include "WRGraveBosses.h"
#include "WRSkyWorld.h"
#include "WRSkyBosses.h"
#include "WRCrystalWorld.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace WRNpcDetail
{
	TSharedPtr<FJsonObject> MasterRow(FName Id)
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		const TSharedPtr<FJsonObject> D = WRWorld::Data();
		if (D.IsValid() && D->TryGetArrayField(TEXT("masters"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List) { FString S; V->AsObject()->TryGetStringField(TEXT("id"), S); if (FName(*S) == Id) return V->AsObject(); }
		}
		return nullptr;
	}

	FString Line(FName Master, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject> Row = MasterRow(Master);
		const TSharedPtr<FJsonObject>* Lines = nullptr;
		FString S;
		if (Row.IsValid() && Row->TryGetObjectField(TEXT("lines"), Lines)) (*Lines)->TryGetStringField(Key, S);
		return S;
	}

	/** What the master says to this player now: first meeting, a task to offer, one in hand, one done, all done. */
	FString GreetingFor(FName Master, const UWRQuestLog* Log)
	{
		const FWRQuestRep* Q = Log ? Log->QuestWith(Master) : nullptr;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		const TSharedPtr<FJsonObject> Quests = WRWorld::Obj(TEXT("quests"));
		const int32 Count = Quests.IsValid() && Quests->TryGetArrayField(Master.ToString(), List) ? List->Num() : 0;
		if (!Q) return Line(Master, TEXT("first"));
		if (Q->Index >= Count) return Line(Master, TEXT("all"));
		if (!Q->bTaken) return Line(Master, TEXT("offer"));
		double Need = 1.0;
		(*List)[Q->Index]->AsObject()->TryGetNumberField(TEXT("count"), Need);
		return Q->Progress >= int32(Need) ? Line(Master, TEXT("done")) : Line(Master, TEXT("waiting"));
	}
}

// ================================================================================================ AWRWorldNpc

AWRWorldNpc::AWRWorldNpc(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("NpcAbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UWRAttributeSet>(TEXT("NpcAttributes"));
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetNetCullDistanceSquared(FMath::Square(20000.f));
}

void AWRWorldNpc::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWorldNpc, NpcRole);
	DOREPLIFETIME(AWRWorldNpc, NpcId);
	DOREPLIFETIME(AWRWorldNpc, NpcName);
	DOREPLIFETIME(AWRWorldNpc, Biome);
	DOREPLIFETIME(AWRWorldNpc, bTrading);
}

void AWRWorldNpc::SetLook(FName Sex, int32 InFace, int32 InBuild, const FName (&Armor)[4], const FName (&Elements)[4])
{
	BodyStyle = Sex == TEXT("f") ? FName(TEXT("f")) : FName(TEXT("m"));
	SetHeroAppearance(uint8(FMath::Clamp(InFace, 0, 255)), uint8(FMath::Clamp(InBuild, 0, 255)));
	SetWornArmor(Armor, Elements);
	// A master wears their path at 75 points (fire-frost 3.3.10 / 3.4.10: the body change as a living preview); a
	// merchant the faint marks of a life on their land.
	bNpcMarks = true;
	const TSharedPtr<FJsonObject> B = WRWorld::Biome(Biome);
	FString Path;
	if (B.IsValid()) B->TryGetStringField(TEXT("path"), Path);
	NpcMarks.Main = FName(*Path);
	NpcMarks.MainS = uint8(NpcRole == TEXT("master") ? 190 : 50);
}

void AWRWorldNpc::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);   // no PlayerState: the survivor's set-up is skipped
	if (!AbilitySystem) return;
	AbilitySystem->InitAbilityActorInfo(this, this);
	if (NpcRole == TEXT("master"))
	{
		// Masters stand outside the fight: no health to take (BlocksHit too), nothing hunts them.
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), 0.f);
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), 0.f);
		bVitals = false;
		return;
	}
	InitVitals(WRWorld::Num(TEXT("caravan.health"), 400.f), 100.f, 60.f);
	bVitals = true;
}

void AWRWorldNpc::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = NpcRole == TEXT("master") ? 160.f : WRWorld::Num(TEXT("caravan.speed"), 300.f);
	LastPos = GetActorLocation();
	WorkClock = FMath::FRandRange(0.f, WorkEvery);
}

FString AWRWorldNpc::GetDisplayLabel() const
{
	if (NpcRole == TEXT("master"))
	{
		const TSharedPtr<FJsonObject> B = WRWorld::Biome(Biome);
		FString Path;
		if (B.IsValid()) B->TryGetStringField(TEXT("path"), Path);
		return FString::Printf(TEXT("%s · %s path master"), *NpcName, *Path.Left(1).ToUpper().Append(Path.RightChop(1)));
	}
	return NpcRole == TEXT("merchant") ? FString::Printf(TEXT("%s%s"), *NpcName, bTrading ? TEXT(" · trading") : TEXT(" · on the road")) : NpcName;
}

void AWRWorldNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || (bVitals && !IsAlive())) return;
	// Whoever's nearest within talking range: the master turns to them and stops work; a merchant at a stop faces them.
	APawn* Nearest = nullptr;
	float Best = WRWorld::Num(TEXT("quests.talkRange"), 400.f) * 1.4f;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		const float D = P ? FVector::Dist(P->GetActorLocation(), GetActorLocation()) : TNumericLimits<float>::Max();
		if (D < Best) { Best = D; Nearest = P; }
	}
	if (NpcRole == TEXT("master"))
	{
		// Keep to the work spot (a shove from a player walks them back).
		const FVector Off = WorkAt - GetActorLocation();
		if (Off.Size2D() > 60.f) AddMovementInput(Off.GetSafeNormal2D(), 0.6f);
		const FRotator Want = Nearest ? FRotator(0.f, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f) : WorkFacing;
		if (Off.Size2D() <= 60.f) SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 3.f));
		WorkClock -= DeltaSeconds;
		if (!Nearest && WorkClock <= 0.f && !WorkClip.IsNone())
		{
			WorkClock = WorkEvery;
			MulticastBodyOneShot(WorkClip);
			// The blow lands a beat into the swing: the anvil ring and its sparks.
			const TWeakObjectPtr<AWRWorldNpc> Self = this;
			FTimerHandle T;
			GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([Self]()
			{
				AWRWorldNpc* N = Self.Get();
				if (!N) return;
				const FVector Hands = N->GetActorLocation() + N->GetActorForwardVector() * 70.f + FVector(0.f, 0.f, 0.f);
				if (!N->WorkSound.IsNone()) WRSound::Broadcast(N, N->WorkSound);
				if (!N->WorkFx.IsNone()) WRFx::Broadcast(N, N->WorkFx, Hands, N->GetActorRotation(), 1.f);
			}), 0.45f, false);
		}
		// Greetings: a short line to each player who comes near (once a while each), no camera grab.
		GreetClock -= DeltaSeconds;
		if (GreetClock <= 0.f)
		{
			GreetClock = 0.5f;
			const double Now = GetWorld()->GetTimeSeconds();
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
				APawn* P = PC ? PC->GetPawn() : nullptr;
				if (!P || !PC->PlayerState || FVector::Dist(P->GetActorLocation(), GetActorLocation()) > WRWorld::Num(TEXT("quests.greetRange"), 600.f)) continue;
				double& At = GreetedAt.FindOrAdd(PC->PlayerState->GetPlayerName(), -1000.0);
				if (Now - At < WRWorld::Num(TEXT("quests.greetEvery"), 90.f)) continue;
				At = Now;
				const FString Said = WRNpcDetail::GreetingFor(NpcId, UWRQuestLog::For(PC->PlayerState));
				if (Said.IsEmpty()) continue;
				PC->ClientNotify(FString::Printf(TEXT("%s: \"%s\""), *NpcName, *Said));
				const TSharedPtr<FJsonObject> Row = WRNpcDetail::MasterRow(NpcId);
				FString Sound;
				if (Row.IsValid() && Row->TryGetStringField(TEXT("lineSound"), Sound) && !Sound.IsEmpty()) WRSound::PlayOn(this, FName(*Sound));
			}
		}
		return;
	}
	// Merchants and outriders walk the road (the subsystem says where), at a steady 3 m/s.
	if (bWalking)
	{
		const FVector To = WalkTo - GetActorLocation();
		if (To.Size2D() > 150.f) AddMovementInput(To.GetSafeNormal2D(), 1.f);
		// Stuck on a rock: a step past it along the way (never a slide through it).
		StuckClock += DeltaSeconds;
		if (StuckClock >= 4.f)
		{
			if (FVector::Dist2D(GetActorLocation(), LastPos) < 100.f && To.Size2D() > 300.f)
			{
				const FVector Step = GetActorLocation() + To.GetSafeNormal2D() * 400.f;
				FHitResult G;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(WRNpcStep), false, this);
				if (GetWorld()->LineTraceSingleByChannel(G, Step + FVector(0, 0, 800), Step - FVector(0, 0, 2000), ECC_WorldStatic, Q)) TeleportTo(G.ImpactPoint + FVector(0, 0, GetSimpleCollisionHalfHeight() + 20.f), GetActorRotation());
			}
			StuckClock = 0.f;
			LastPos = GetActorLocation();
		}
	}
	else if (Nearest)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f), DeltaSeconds, 3.f));
	}
}

void AWRWorldNpc::OnDeath(AActor* Killer)
{
	// No respawn, no death penalty: a merchant killed on the road drops a tenth of the stock on it.
	if (NpcRole == TEXT("merchant"))
	{
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		FString Table;
		const TSharedPtr<FJsonObject> D = WRWorld::Data();
		if (D.IsValid() && D->TryGetArrayField(TEXT("caravans"), Rows))
		{
			for (const auto& V : *Rows) { FString Id; V->AsObject()->TryGetStringField(TEXT("id"), Id); if (FName(*Id) == NpcId) V->AsObject()->TryGetStringField(TEXT("trade"), Table); }
		}
		const TArray<TSharedPtr<FJsonValue>>* Offers = nullptr;
		TArray<FWRItemStack> Drop;
		const TSharedPtr<FJsonObject> Trade = WRWorld::Obj(TEXT("trade"));
		if (Trade.IsValid() && Trade->TryGetArrayField(Table, Offers))
		{
			// Its stock is ten of each offer's goods; a tenth falls.
			const float Fraction = WRWorld::Num(TEXT("caravan.dropFraction"), 0.1f);
			for (const auto& O : *Offers)
			{
				const TSharedPtr<FJsonObject>* Get = nullptr;
				if (!O->AsObject()->TryGetObjectField(TEXT("get"), Get)) continue;
				for (const auto& KV : (*Get)->Values)
				{
					const int32 N = FMath::Max(1, FMath::RoundToInt(KV.Value->AsNumber() * 10.f * Fraction));
					FWRItemStack S = FWRInventoryRules::MakeInstance(FName(*KV.Key));
					S.Count = N;
					Drop.Add(S);
				}
			}
		}
		if (Drop.Num() > 0) AWRLootBag::Spill(GetWorld(), GetActorLocation(), FString(), Drop);
		UE_LOG(LogWildreach, Log, TEXT("CARAVAN %s's merchant killed by %s: %d goods on the road"), *NpcId.ToString(), *GetNameSafe(Killer), Drop.Num());
	}
	DetachFromControllerPendingDestroy();
	SetLifeSpan(20.f);
}

// ================================================================================================ the client's side

void AWRPlayerController::ClientOpenWorldScreen_Implementation(FName Screen, AActor* Target)
{
	WorldScreen = Screen;
	WorldTarget = Target;
	Menu = EWRMenu::World;
	MenuIndex = 0;
	WRSound::PlayUI(this, TEXT("ui.open"));
}

namespace WRNpcDetail
{
	struct FNear { AActor* Actor = nullptr; float Dist = TNumericLimits<float>::Max(); };

	/** The nearest world thing in its reach. */
	FNear Nearest(const AWRPlayerController* PC)
	{
		FNear N;
		const APawn* Me = PC ? PC->GetPawn() : nullptr;
		if (!Me) return N;
		UWorld* World = Me->GetWorld();
		const FVector L = Me->GetActorLocation();
		const auto Consider = [&N, &L](AActor* A, const FVector& At, float Reach)
		{
			const float D = FVector::Dist(L, At);
			if (D < Reach && D < N.Dist) { N.Actor = A; N.Dist = D; }
		};
		const float ShrineReach = WRWorld::Num(TEXT("shrines.interactRange"), 450.f);
		for (TActorIterator<AWRShrine> It(World); It; ++It) Consider(*It, It->GetActorLocation() + FVector(0.f, 0.f, 120.f), ShrineReach * (It->bGreat ? 1.3f : 1.f));
		for (TActorIterator<AWRWorldNpc> It(World); It; ++It)
		{
			if (It->NpcRole == TEXT("master") || (It->NpcRole == TEXT("merchant") && It->bTrading)) Consider(*It, It->GetActorLocation(), WRWorld::Num(TEXT("quests.talkRange"), 400.f));
		}
		for (TActorIterator<AWRSummoningStone> It(World); It; ++It) Consider(*It, It->GetActorLocation(), 320.f);
		for (TActorIterator<AWRMessageStone> It(World); It; ++It) Consider(*It, It->GetActorLocation(), WRWorld::Num(TEXT("messages.readRange"), 300.f));
		for (TActorIterator<AWRWorldCache> It(World); It; ++It) if (!It->bTaken) Consider(*It, It->GetActorLocation(), It->Kind == TEXT("crater") ? 450.f : 300.f);
		return N;
	}
}

bool WRWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	// A gap in a Storm peaks rope bridge at your feet: mend it (2 wood).
	if (const APawn* Me = PC ? PC->GetPawn() : nullptr)
	{
		int32 Plank = INDEX_NONE;
		if (UWRStormWorld* SW = UWRStormWorld::Get(PC); SW && SW->BridgeGapNear(Me->GetActorLocation(), Plank))
		{
			OutTarget = TEXT("A gap in the bridge");
			OutVerb = FString::Printf(TEXT("Lay a plank (%d wood)"), (int32)WRTune(TEXT("storm.plankWood"), 2.f));
			return true;
		}
	}
	// The Swamp's things (WRSwampSystems): boardwalk gaps, the bosses' broken square, caches, the key cage, the trapdoor,
	// the Coven's ladders, flue, bell, eggs and gate, wild plants, the Murkjaw blood rite.
	if (WRSwamp::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Ancient forest's things (WRForestHazards, WRForestHollow): a wasp nest to rob, the Hollow Root's doors and roots,
	// the secrets (the bell, the barred door, the eel trap, the seed jars).
	if (WRForestHazards::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Ancient forest's world (WRForestWorld): the Canopy Lift, the offering stones.
	if (UWRForestWorld::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Graveyard's spots (WRGraveWorld): keepsakes, digs, the Stair's last lantern, the seventh name, the drowned bell.
	if (UWRGraveWorld::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Graveyard's boss fights (WRGraveBosses): dig a buried friend out, pull a tame up, drive a stake into a tomb.
	if (UWRGraveArenas::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Sky's things (WRSkyWorld): a floating shard, a Kitewright lift, a line to climb, a drift, a bell, a vista.
	if (UWRSkyWorld::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Last Circle (WRSkyBosses): grab Aeolith's back on a low pass; run the crest to the next vent.
	if (UWRSkyArenas::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Crystal caves' spots (WRCrystalThings): the halls' keystone chimes, the keepsakes, the found things.
	if (UWRCrystalWorld::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Ocean's spots (WROceanWorld): keepsakes, clams, the tsunami stones, the drowned bell, the Sluice key, the beacon.
	if (UWROceanWorld::Prompt(PC, OutTarget, OutVerb)) return true;
	// The Fungal caverns' doors and secrets (WRFungalSpots): the Sealed Gate's web, the mole wall, the fifth tunnel, the
	// ring's altar, the Heart Gate, the Seed Vault.
	if (UWRFungalWorld::Prompt(PC, OutTarget, OutVerb)) return true;
	const WRNpcDetail::FNear N = WRNpcDetail::Nearest(PC);
	if (!N.Actor) return false;
	if (const AWRShrine* S = Cast<AWRShrine>(N.Actor))
	{
		OutTarget = S->DisplayName;
		OutVerb = S->bHeld ? TEXT("Held by invaders") : S->bLit ? TEXT("Shrine: travel") : S->bGuarded ? TEXT("Guarded: clear it first") : S->KindleDoneAt > 0.f ? TEXT("Kindling...") : TEXT("Kindle (stand still)");
		return true;
	}
	if (const AWRWorldNpc* Npc = Cast<AWRWorldNpc>(N.Actor))
	{
		OutTarget = Npc->GetDisplayLabel();
		OutVerb = Npc->NpcRole == TEXT("master") ? TEXT("Talk") : TEXT("Trade");
		return true;
	}
	if (const AWRSummoningStone* St = Cast<AWRSummoningStone>(N.Actor))
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(St->BossId);
		OutTarget = FString::Printf(TEXT("Summoning stone · %s"), Def ? *Def->Name : *St->BossId.ToString());
		OutVerb = TEXT("Offer effigy");
		return true;
	}
	if (const AWRMessageStone* M = Cast<AWRMessageStone>(N.Actor))
	{
		OutTarget = FString::Printf(TEXT("\"%s\""), *M->Text);
		OutVerb = TEXT("Read message");
		return true;
	}
	if (const AWRWorldCache* C = Cast<AWRWorldCache>(N.Actor))
	{
		OutTarget = C->Title;
		OutVerb = C->Kind == TEXT("core") ? TEXT("Take the core") : C->Kind == TEXT("crater") ? TEXT("Take the star iron") : TEXT("Open");
		return true;
	}
	return false;
}

bool WRWorld::ClientInteract(AWRPlayerController* PC)
{
	if (const APawn* Me = PC ? PC->GetPawn() : nullptr)
	{
		int32 Plank = INDEX_NONE;
		if (UWRStormWorld* SW = UWRStormWorld::Get(PC); SW && SW->BridgeGapNear(Me->GetActorLocation(), Plank))
		{
			PC->SendCommand(TEXT("world plank"));
			return true;
		}
	}
	if (WRSwamp::ClientInteract(PC)) return true;
	if (WRForestHazards::ClientInteract(PC)) return true;
	if (UWRForestWorld::ClientInteract(PC)) return true;
	if (UWRGraveArenas::ClientInteract(PC)) return true;
	if (UWRGraveWorld::ClientInteract(PC)) return true;
	if (UWRSkyArenas::ClientInteract(PC)) return true;
	if (UWRSkyWorld::ClientInteract(PC)) return true;
	if (UWRCrystalWorld::ClientInteract(PC)) return true;
	if (UWROceanWorld::ClientInteract(PC)) return true;
	if (UWRFungalWorld::ClientInteract(PC)) return true;
	const WRNpcDetail::FNear N = WRNpcDetail::Nearest(PC);
	if (!N.Actor) return false;
	if (AWRShrine* S = Cast<AWRShrine>(N.Actor))
	{
		if (S->bLit && !S->bHeld) { PC->ClientOpenWorldScreen_Implementation(TEXT("shrine"), S); return true; }
		if (S->bHeld) { PC->ShowLocalNotice(TEXT("Invaders hold this shrine: drive them off")); return true; }
		if (S->bGuarded) { PC->ShowLocalNotice(TEXT("Its guards are still near: clear them first")); return true; }
		PC->SendCommand(FString::Printf(TEXT("world kindle %s"), *S->ShrineId.ToString()));
		return true;
	}
	if (AWRWorldNpc* Npc = Cast<AWRWorldNpc>(N.Actor))
	{
		PC->ClientOpenWorldScreen_Implementation(Npc->NpcRole == TEXT("master") ? FName(TEXT("talk")) : FName(TEXT("trade")), Npc);
		return true;
	}
	if (AWRSummoningStone* St = Cast<AWRSummoningStone>(N.Actor))
	{
		PC->SendCommand(FString::Printf(TEXT("world summon %s"), *St->BossId.ToString()));
		return true;
	}
	if (AWRMessageStone* M = Cast<AWRMessageStone>(N.Actor))
	{
		PC->ClientOpenWorldScreen_Implementation(TEXT("msgread"), M);
		return true;
	}
	if (AWRWorldCache* C = Cast<AWRWorldCache>(N.Actor))
	{
		const FVector L = C->GetActorLocation();
		PC->SendCommand(FString::Printf(TEXT("world open %.1f %.1f %.1f"), L.X, L.Y, L.Z));
		return true;
	}
	return false;
}
