// The strongholds' garrisons: see WRGarrison.h (Aaron 11-16, q-stronghold-garrison; cooks/build-wild-strongholds.md 2.4-2.5).

#include "WRGarrison.h"
#include "WRStronghold.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRAbilities.h"
#include "Components/CapsuleComponent.h"
#include "WRBoss.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRWeapons.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace WRGarrisonDetail
{
	float Tn(const TCHAR* Key, float Def) { return WRTune(*(FString(TEXT("strongholds.garrison.")) + Key), Def); }

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Def)
	{
		double V = Def;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Def;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(Key, V);
		return V;
	}

	bool Flag(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		bool V = false;
		return O.IsValid() && O->TryGetBoolField(Key, V) && V;
	}

	TArray<TPair<FName, int32>> Loot(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<TPair<FName, int32>> Out;
		const TSharedPtr<FJsonObject>* L = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Key, L))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*L)->Values) Out.Add({ FName(*E.Key), int32(E.Value->AsNumber()) });
		}
		return Out;
	}

	FWRHoldMove ReadMove(const TSharedPtr<FJsonObject>& O)
	{
		FWRHoldMove M;
		M.Id = FName(*Str(O, TEXT("id")));
		const FString Shape = Str(O, TEXT("shape"));
		if (!Shape.IsEmpty()) M.Shape = FName(*Shape);
		M.Range = Num(O, TEXT("range"), M.Range);
		M.Size = Num(O, TEXT("size"), M.Size);
		M.Windup = Num(O, TEXT("windup"), M.Windup);
		M.Hold = Num(O, TEXT("hold"), 0.f);
		M.Active = Num(O, TEXT("active"), M.Active);
		M.Punish = Num(O, TEXT("punish"), M.Punish);
		M.Damage = Num(O, TEXT("dmg"), M.Damage);
		M.Poise = Num(O, TEXT("poise"), M.Poise);
		M.Impact = uint8(FMath::Clamp(int32(Num(O, TEXT("impact"), 3.f)), 1, 5));
		const FString El = Str(O, TEXT("element"));
		if (!El.IsEmpty()) M.Element = FName(*El);
		M.Buildup = Num(O, TEXT("buildup"), 0.f);
		M.bUnblockable = Flag(O, TEXT("unblockable"));
		M.bParryable = Flag(O, TEXT("parryable"));
		M.bLow = Flag(O, TEXT("low"));
		M.bGrab = Flag(O, TEXT("grab"));
		M.bGuardBreak = Flag(O, TEXT("guardBreak"));
		M.bTransition = Flag(O, TEXT("transition"));
		M.bTamePunish = Flag(O, TEXT("tamePunish"));
		M.bLunge = Flag(O, TEXT("lunge"));
		M.Phase = int32(Num(O, TEXT("phase"), 1.f));
		M.MaxPhase = int32(Num(O, TEXT("maxPhase"), 9.f));
		M.Cooldown = Num(O, TEXT("cooldown"), M.Cooldown);
		const TSharedPtr<FJsonObject>* W = nullptr;
		if (O->TryGetObjectField(TEXT("weights"), W))
		{
			M.WClose = Num(*W, TEXT("close"), 0.f);
			M.WMid = Num(*W, TEXT("mid"), 0.f);
			M.WFar = Num(*W, TEXT("far"), 0.f);
		}
		const FString Clip = Str(O, TEXT("clip"));
		if (!Clip.IsEmpty()) M.Clip = FName(*Clip);
		M.ProjectileSpeed = Num(O, TEXT("speed"), M.ProjectileSpeed);
		M.Repeats = FMath::Max(1, int32(Num(O, TEXT("repeats"), 1.f)));
		M.Gap = Num(O, TEXT("gap"), M.Gap);
		M.HoldOn = int32(Num(O, TEXT("holdOn"), 1.f));
		M.HazardSeconds = Num(O, TEXT("hazard"), 0.f);
		M.HazardRadius = Num(O, TEXT("hazardRadius"), M.HazardRadius);
		const FString Sound = Str(O, TEXT("sound"));
		if (!Sound.IsEmpty()) M.Sound = FName(*Sound);
		const FString Fx = Str(O, TEXT("fx"));
		if (!Fx.IsEmpty()) M.Fx = FName(*Fx);
		M.Mesh = Str(O, TEXT("mesh"));
		M.MeshAlong = Str(O, TEXT("meshAlong"));
		return M;
	}

	FGameplayTag ElementTag(FName Element)
	{
		return Element == TEXT("fire") ? WRTags::Element_Fire : Element == TEXT("frost") ? WRTags::Element_Frost : WRTags::Element_Physical;
	}

	/** The survivor who gets the credit for a blow or a kill: the pawn's player, or a tame's owner. */
	AWRPlayerState* CreditFor(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerState* PS = P ? P->GetPlayerState<AWRPlayerState>() : nullptr) return PS;
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		const FString Owner = C ? C->GetTameOwner() : FString();
		if (Owner.IsEmpty() || !Who->GetWorld() || !Who->GetWorld()->GetGameState()) return nullptr;
		for (APlayerState* S : Who->GetWorld()->GetGameState()->PlayerArray)
		{
			if (S && S->GetPlayerName() == Owner) return Cast<AWRPlayerState>(S);
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------- the ranks and warlords

const FWRGarrisonData& FWRGarrisonData::Get()
{
	static const FWRGarrisonData Data = []()
	{
		using namespace WRGarrisonDetail;
		FWRGarrisonData D;
		const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("strongholds.json"));
		const TSharedPtr<FJsonObject>* Ranks = nullptr;
		if (J.IsValid() && J->TryGetObjectField(TEXT("ranks"), Ranks))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Ranks)->Values)
			{
				const TSharedPtr<FJsonObject> O = E.Value->AsObject();
				FWRHoldRank R;
				R.Id = FName(*E.Key);
				R.Title = Str(O, TEXT("title"));
				R.Tier = FName(*Str(O, TEXT("tier")));
				R.Health = Num(O, TEXT("health"), R.Health);
				R.Stamina = Num(O, TEXT("stamina"), R.Stamina);
				R.Poise = Num(O, TEXT("poise"), R.Poise);
				R.Armor = Num(O, TEXT("armor"), R.Armor);
				R.Sight = Num(O, TEXT("sight"), R.Sight);
				R.Cone = Num(O, TEXT("cone"), R.Cone);
				R.Hear = Num(O, TEXT("hear"), R.Hear);
				R.Leash = Num(O, TEXT("leash"), R.Leash);
				R.WalkSpeed = Num(O, TEXT("walk"), R.WalkSpeed);
				R.RunSpeed = Num(O, TEXT("run"), R.RunSpeed);
				R.Guard = Num(O, TEXT("guard"), R.Guard);
				R.GuardShield = Num(O, TEXT("guardShield"), R.Guard);
				R.Dodge = Num(O, TEXT("dodge"), R.Dodge);
				R.Heavy = Num(O, TEXT("heavy"), R.Heavy);
				R.Charged = Num(O, TEXT("charged"), R.Charged);
				R.ChainMax = FMath::Max(1, int32(Num(O, TEXT("chain"), 3.f)));
				R.KickAfter = Num(O, TEXT("kickAfter"), 0.f);
				R.ShotDamage = Num(O, TEXT("shot"), 0.f);
				R.DrawSeconds = FMath::Max(0.9f, Num(O, TEXT("draw"), R.DrawSeconds));   // CAL 1.13: >= 0.9 s of warning
				R.Reload = Num(O, TEXT("reload"), R.Reload);
				R.KeepMin = FMath::Max(400.f, Num(O, TEXT("keepMin"), R.KeepMin));      // nothing loosed inside 4 m
				R.KeepMax = Num(O, TEXT("keepMax"), R.KeepMax);
				R.HornSeconds = Num(O, TEXT("horn"), 0.f);
				R.Loot = Loot(O, TEXT("loot"));
				R.RareChance = Num(O, TEXT("rare"), 0.f);
				const FString Rare = Str(O, TEXT("rareItem"));
				if (!Rare.IsEmpty()) R.RareItem = FName(*Rare);
				D.Ranks.Add(R.Id, R);
			}
		}
		const TSharedPtr<FJsonObject>* Lords = nullptr;
		if (J.IsValid() && J->TryGetObjectField(TEXT("warlords"), Lords))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& E : (*Lords)->Values)
			{
				const TSharedPtr<FJsonObject> O = E.Value->AsObject();
				FWRWarlordDef W;
				W.Id = FName(*E.Key);
				W.Name = Str(O, TEXT("name"));
				W.Element = FName(*Str(O, TEXT("element")));
				W.Health = Num(O, TEXT("health"), W.Health);
				W.Poise = Num(O, TEXT("poise"), W.Poise);
				W.Armor = Num(O, TEXT("armor"), W.Armor);
				W.PerFighter = Num(O, TEXT("perFighter"), W.PerFighter);
				W.Phase2At = Num(O, TEXT("phase2At"), W.Phase2At);
				W.Phase2Speed = Num(O, TEXT("phase2Speed"), W.Phase2Speed);
				W.ResetSeconds = Num(O, TEXT("resetSeconds"), W.ResetSeconds);
				W.FirstKill = FName(*Str(O, TEXT("firstKill")));
				W.Loot = Loot(O, TEXT("loot"));
				const TArray<TSharedPtr<FJsonValue>>* Moves = nullptr;
				if (O->TryGetArrayField(TEXT("moves"), Moves))
				{
					for (const TSharedPtr<FJsonValue>& MV : *Moves) if (MV->AsObject().IsValid()) W.Moves.Add(ReadMove(MV->AsObject()));
				}
				D.Warlords.Add(W.Id, W);
			}
		}
		UE_LOG(LogWildreach, Log, TEXT("HOLD garrison data: %d ranks, %d warlords"), D.Ranks.Num(), D.Warlords.Num());
		return D;
	}();
	return Data;
}

// ---------------------------------------------------------------- the soldier

AWRGarrison::AWRGarrison(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Its own ability system (a survivor's lives on the PlayerState), as the world's people have (AWRWorldNpc).
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("GarrisonAbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UWRAttributeSet>(TEXT("GarrisonAttributes"));
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetNetCullDistanceSquared(FMath::Square(25000.f));
}

void AWRGarrison::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRGarrison, HoldId);
	DOREPLIFETIME(AWRGarrison, RankId);
	DOREPLIFETIME(AWRGarrison, SoldierName);
	DOREPLIFETIME(AWRGarrison, WarlordId);
	DOREPLIFETIME(AWRGarrison, Phase);
	DOREPLIFETIME(AWRGarrison, bEngaged);
	DOREPLIFETIME(AWRGarrison, Participants);
}

const FWRHoldRank* AWRGarrison::GetRank() const { return FWRGarrisonData::Get().Rank(RankId); }
const FWRWarlordDef* AWRGarrison::GetWarlord() const { return WarlordId.IsNone() ? nullptr : FWRGarrisonData::Get().Warlord(WarlordId); }
double AWRGarrison::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0; }

void AWRGarrison::Setup(FName InHold, const FWRHoldSoldier& Row, const FString& Faction)
{
	HoldId = InHold;
	SoldierId = Row.Id;
	RankId = Row.Rank;
	WarlordId = Row.Warlord;
	Post = Row.At;
	PostYaw = Row.Yaw;
	Route = Row.Route;
	MeleeWeapon = Row.Weapon;
	BowItem = Row.Bow;
	ChestPiece = Row.Armor[1];
	bShield = Row.bShield;
	const FWRHoldRank* R = GetRank();
	const FWRWarlordDef* W = GetWarlord();
	SoldierName = W ? W->Name : !Row.Name.IsEmpty() ? Row.Name : FString::Printf(TEXT("%s %s"), *Faction, R ? *R->Title : TEXT("soldier"));
	// The look: the body, face and build, the armour piece by piece (an empty slot shows the wraps), the weapon in hand
	// and the shield on the arm for one-handers.
	BodyStyle = Row.Sex == TEXT("f") ? FName(TEXT("f")) : FName(TEXT("m"));
	SetHeroAppearance(uint8(FMath::Clamp(Row.Face, 0, 255)), uint8(FMath::Clamp(Row.Build, 0, 255)));
	const FName NoElements[4] = { NAME_None, NAME_None, NAME_None, NAME_None };
	SetWornArmor(Row.Armor, NoElements);
	Weapon.Metal = Row.Metal.IsNone() ? FName(TEXT("iron")) : Row.Metal;
	Weapon.Quality = TEXT("common");
	SetMeleeItem(MeleeWeapon);
	bShieldShown = bShield;
	if (!BowItem.IsNone()) SetRangedItem(BowItem);
}

void AWRGarrison::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);   // no PlayerState: the survivor's set-up is skipped
	if (!AbilitySystem) return;
	AbilitySystem->InitAbilityActorInfo(this, this);
	// The survivor's own four combat abilities and the critical: the real move sets of what it holds.
	if (!AbilitySystem->FindAbilitySpecFromClass(UWRGA_LightAttack::StaticClass()))
	{
		GrantAbility(UWRGA_LightAttack::StaticClass(), EWRAbilityInput::Light);
		GrantAbility(UWRGA_HeavyAttack::StaticClass(), EWRAbilityInput::Heavy);
		GrantAbility(UWRGA_Dodge::StaticClass(), EWRAbilityInput::Dodge);
		GrantAbility(UWRGA_Block::StaticClass(), EWRAbilityInput::Block);
		GrantAbility(UWRGA_Critical::StaticClass(), EWRAbilityInput::None);
	}
	const FWRHoldRank* R = GetRank();
	const FWRWarlordDef* W = GetWarlord();
	const float Health = W ? W->Health : R ? R->Health : 150.f;
	SoloHealth = Health;
	InitVitals(Health, R ? R->Stamina : 100.f, W ? W->Poise : R ? R->Poise : 30.f);
	ArmorValue = W ? W->Armor : R ? R->Armor : 0.f;
}

void AWRGarrison::BeginPlay()
{
	Super::BeginPlay();
	const FWRHoldRank* R = GetRank();
	GetCharacterMovement()->MaxWalkSpeed = R ? R->WalkSpeed : 160.f;
	if (HasAuthority())
	{
		// Archers have the bow out at their post.
		if (!BowItem.IsNone()) SetRangedMode(true);
		StateSince = Now();
		LastPos = GetActorLocation();
		NextThink = Now() + FMath::FRandRange(0.f, 0.3f);
		// A patrol starts walking from where it stands (its first mark is its post).
		RouteIndex = Route.Num() > 1 ? 1 : 0;
	}
}

FString AWRGarrison::GetDisplayLabel() const
{
	return SoldierName.IsEmpty() ? GetName() : SoldierName;
}

bool AWRGarrison::IsHostileTo(const AWRCharacterBase* Other) const
{
	if (!Other || Other == this) return false;
	// Its own fort's people and hounds are its side; every survivor, tame and beast is not.
	if (Cast<AWRGarrison>(Other)) return false;
	const FName Theirs = FWRStrongholds::HoldOf(Other);
	return Theirs.IsNone() || Theirs != HoldId;
}

bool AWRGarrison::IsValidMark(const AActor* Who) const
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C || !C->IsAlive() || !IsHostileTo(C) || C->HasTag(WRTags::State_Invulnerable)) return false;
	// Survivors and their tames (beasts that wander in are fought only if they start it: NotifyAttackedBy).
	if (C->GetPlayerState()) return true;
	return !C->GetTameOwner().IsEmpty();
}

bool AWRGarrison::CanSee(const AActor* Who, float Range, float ConeDeg) const
{
	if (!Who) return false;
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector To = Who->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	// A crouching survivor is seen at half the range.
	const ACharacter* Ch = Cast<ACharacter>(Who);
	if (Ch && Ch->GetCharacterMovement() && Ch->GetCharacterMovement()->IsCrouching()) Range *= WRGarrisonDetail::Tn(TEXT("crouchSight"), 0.5f);
	if (FVector::Dist(Eye, To) > Range) return false;
	if (ConeDeg < 359.f)
	{
		const FVector Dir = (To - Eye).GetSafeNormal2D();
		const float Cos = FMath::Cos(FMath::DegreesToRadians(ConeDeg * 0.5f));
		if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), Dir) < Cos && FVector::Dist2D(Eye, To) > 250.f) return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGarrisonSight), false, this);
	Q.AddIgnoredActor(Who);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, To, ECC_Visibility, Q);
}

AActor* AWRGarrison::LookForTarget(bool& bOutClear) const
{
	bOutClear = false;
	const FWRHoldRank* R = GetRank();
	const float Range = R ? R->Sight : 1800.f, Cone = R ? R->Cone : 120.f;
	AActor* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!IsValidMark(*It)) continue;
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D > Range || D >= BestD) continue;
		// The warlord fights only who is in his ward.
		if (IsWarlord() && !InWard(It->GetActorLocation(), 150.f)) continue;
		if (!IsWarlord() && !CanSee(*It, Range, Cone)) continue;
		Best = *It;
		BestD = D;
	}
	bOutClear = Best != nullptr;
	return Best;
}

void AWRGarrison::SetState(EWRGarrisonState S)
{
	if (State == S) return;
	State = S;
	StateSince = Now();
	AAIController* AI = Cast<AAIController>(GetController());
	if (S == EWRGarrisonState::Fight)
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
		if (AI && Target.IsValid()) AI->SetFocus(Target.Get());
	}
	else
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		if (AI) AI->ClearFocus(EAIFocusPriority::Gameplay);
		// Let go of anything held.
		Release(EWRAbilityInput::Block);
		if (IsRangedOut()) SetAiming(false);
		DrawStarted = -1.0;
	}
}

void AWRGarrison::Engage(AActor* NewTarget)
{
	if (!HasAuthority() || !IsAlive() || !IsValidMark(NewTarget)) return;
	if (IsWarlord() && !InWard(NewTarget->GetActorLocation(), 300.f)) return;
	Target = NewTarget;
	LastSeenAt = NewTarget->GetActorLocation();
	LastSeenTime = Now();
	if (State != EWRGarrisonState::Fight)
	{
		WRSound::Broadcast(this, TEXT("creature.humanoid.aggro"));
		SetState(EWRGarrisonState::Fight);
	}
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->SetFocus(NewTarget);
}

void AWRGarrison::Alarm(const FVector& Where, AActor* Seen)
{
	if (!IsAlive()) return;
	// The horn: hold-fast posts (archers, the warden, the warlord) stay where they are and fight what they see; everyone
	// else comes to where the survivor was seen.
	if (Seen && IsValidMark(Seen) && !IsWarlord() && RankId != TEXT("warden") && BowItem.IsNone())
	{
		Engage(Seen);
	}
	else if (!IsWarlord() && RankId != TEXT("warden") && BowItem.IsNone() && State != EWRGarrisonState::Fight)
	{
		LastSeenAt = Where;
		LastSeenTime = Now();
		SetState(EWRGarrisonState::Search);
	}
	bHornBlown = true;   // no second horn once the fort is roused
}

void AWRGarrison::NotifyAttackedBy(AActor* Attacker)
{
	Super::NotifyAttackedBy(Attacker);
	if (!HasAuthority() || !Attacker) return;
	// Hit from anywhere: it turns on its attacker (a beast that bit it too), and the fellows who can hear it come.
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Attacker);
	if (C && C->IsAlive() && IsHostileTo(C) && (!IsWarlord() || InWard(Attacker->GetActorLocation(), 300.f)))
	{
		Target = Attacker;
		LastSeenAt = Attacker->GetActorLocation();
		LastSeenTime = Now();
		SetState(EWRGarrisonState::Fight);
	}
	if (UWRStrongholdSubsystem* S = UWRStrongholdSubsystem::Get(this))
	{
		const FWRHoldRank* R = GetRank();
		S->Shout(HoldId, GetActorLocation(), Attacker, R ? R->Hear : 1500.f);
	}
}

void AWRGarrison::OnDamageTaken(const FWRHit& Hit, float Damage)
{
	Super::OnDamageTaken(Hit, Damage);
	if (!HasAuthority() || !IsWarlord() || Damage <= 0.f) return;
	// The warlord's pay: who dealt what (a tame's blows count for its owner).
	if (const AWRPlayerState* PS = WRGarrisonDetail::CreditFor(Hit.Instigator)) Ledger.FindOrAdd(PS->GetPlayerName()) += Damage;
	WRSound::Broadcast(this, TEXT("creature.humanoid.hurt"));
}

void AWRGarrison::OnRep_Phase()
{
	// Everyone: the phase change's roar is in its transition move; the bar reads Phase.
}

// ---------------------------------------------------------------- the brain (server)

void AWRGarrison::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !IsAlive()) return;
	const double T = Now();
	if (T >= NextThink)
	{
		NextThink = T + WRGarrisonDetail::Tn(TEXT("thinkSeconds"), 0.1f);
		Think();
	}
	// A blow that staggers it, a crit it's held in, or a freeze: nothing until it's over.
	if (HasTag(WRTags::Status_Staggered) || HasTag(WRTags::State_BeingCrit) || HasTag(WRTags::Status_Frozen) || HasTag(WRTags::Status_StanceBroken))
	{
		if (HornStarted >= 0.0 && !bHornBlown) HornStarted = -1.0;   // the horn is knocked from its lips
		if (DrawStarted >= 0.0) { SetAiming(false); DrawStarted = -1.0; }
		return;
	}
	if (IsWarlord() && CurrentMove != INDEX_NONE)
	{
		TickMove(DeltaSeconds);
		return;
	}
	switch (State)
	{
	case EWRGarrisonState::Post:
		TickPost(DeltaSeconds);
		break;
	case EWRGarrisonState::Look:
		if (Target.IsValid()) FaceToward(Target->GetActorLocation(), DeltaSeconds, 6.f);
		break;
	case EWRGarrisonState::Fight:
		TickFight(DeltaSeconds);
		break;
	case EWRGarrisonState::Search:
		MoveToward(LastSeenAt, GetRank() ? GetRank()->RunSpeed : 400.f, 120.f);
		break;
	case EWRGarrisonState::Return:
		MoveToward(HomePoint(), GetRank() ? GetRank()->WalkSpeed : 160.f, 60.f);
		break;
	}
}

void AWRGarrison::Think()
{
	const double T = Now();
	const FWRHoldRank* R = GetRank();
	if (IsWarlord())
	{
		if (T >= NextParticipantCheck)
		{
			NextParticipantCheck = T + 1.0;
			UpdateParticipants();
		}
		CheckPhase();
		// Phase 2: his weapon carries his element; it shows on the head (renewed while it lasts, so a reset puts it out).
		if (Phase >= 2 && T >= NextEmberAt)
		{
			const FWRWarlordDef* W = GetWarlord();
			NextEmberAt = T + 10.0;
			WRFx::BroadcastOn(this, W && W->Element == TEXT("frost") ? FName(TEXT("status.frost")) : FName(TEXT("lava.embers")), TEXT("weapon"), 1.f, NAME_None, 10.5f);
		}
	}
	if (Target.IsValid() && !IsValidMark(Target.Get())) Target = nullptr;
	bool bSeen = false;
	AActor* Seen = LookForTarget(bSeen);
	switch (State)
	{
	case EWRGarrisonState::Post:
	case EWRGarrisonState::Return:
	case EWRGarrisonState::Search:
		if (Seen)
		{
			Target = Seen;
			LastSeenAt = Seen->GetActorLocation();
			LastSeenTime = T;
			// Something seen at the edge: it turns to look before it commits (Elden Ring's pause). Close, or already
			// searching, it goes straight in.
			const bool bClose = FVector::Dist(Seen->GetActorLocation(), GetActorLocation()) < 700.f;
			if (State == EWRGarrisonState::Search || bClose || IsWarlord()) Engage(Seen);
			else SetState(EWRGarrisonState::Look);
		}
		else if (State == EWRGarrisonState::Search && (T - StateSince > WRGarrisonDetail::Tn(TEXT("searchSeconds"), 8.f) || FVector::Dist2D(GetActorLocation(), LastSeenAt) < 150.f && T - StateSince > 3.0))
		{
			SetState(EWRGarrisonState::Return);
		}
		else if (State == EWRGarrisonState::Return && FVector::Dist2D(GetActorLocation(), HomePoint()) < 90.f)
		{
			// Back on its round at the mark it came back to, or at its post.
			if (Route.Num() > 1) RouteIndex = (NearestMark() + 1) % Route.Num();
			SetState(EWRGarrisonState::Post);
		}
		break;
	case EWRGarrisonState::Look:
		if (!Target.IsValid()) { SetState(EWRGarrisonState::Post); break; }
		if (T - StateSince >= WRGarrisonDetail::Tn(TEXT("lookSeconds"), 0.8f)) Engage(Target.Get());
		break;
	case EWRGarrisonState::Fight:
	{
		if (Seen && (!Target.IsValid() || FVector::Dist(Seen->GetActorLocation(), GetActorLocation()) + 300.f < FVector::Dist(Target->GetActorLocation(), GetActorLocation())))
		{
			Target = Seen;   // the nearest it can see
			if (AAIController* AI = Cast<AAIController>(GetController())) AI->SetFocus(Seen);
		}
		if (Target.IsValid())
		{
			const bool bSight = IsWarlord() ? InWard(Target->GetActorLocation(), 300.f) : CanSee(Target.Get(), (R ? R->Sight : 1800.f) * 1.3f, 360.f);
			if (bSight) { LastSeenAt = Target->GetActorLocation(); LastSeenTime = T; }
		}
		const float Leash = R ? R->Leash : 3000.f;
		const bool bLost = !Target.IsValid() || T - LastSeenTime > WRGarrisonDetail::Tn(TEXT("loseSeconds"), 5.f);
		const bool bLeashed = !IsWarlord() && FVector::Dist2D(GetActorLocation(), HomePoint()) > Leash;
		if (IsWarlord() && (!Target.IsValid() || !InWard(Target->GetActorLocation(), 300.f)))
		{
			Target = nullptr;
			SetState(EWRGarrisonState::Return);
		}
		else if (bLeashed)
		{
			Target = nullptr;
			SetState(EWRGarrisonState::Return);
		}
		else if (bLost)
		{
			Target = nullptr;
			SetState(EWRGarrisonState::Search);
		}
		break;
	}
	}
	// Stuck far from its post with nothing to fight for a long time: put back where nobody sees it happen.
	if (State == EWRGarrisonState::Return)
	{
		if (FVector::Dist(GetActorLocation(), LastPos) > 60.f) { LastPos = GetActorLocation(); LostPostSince = T; }
		if (LostPostSince < 0.0) LostPostSince = T;
		if (T - LostPostSince > WRGarrisonDetail::Tn(TEXT("stuckSeconds"), 30.f))
		{
			bool bWatched = false;
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
				bWatched |= P && (FVector::Dist(P->GetActorLocation(), GetActorLocation()) < 6000.f || FVector::Dist(P->GetActorLocation(), HomePoint()) < 6000.f);
			}
			if (!bWatched)
			{
				SetActorLocation(HomePoint() + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				SetActorRotation(FRotator(0.f, PostYaw, 0.f));
				SetState(EWRGarrisonState::Post);
			}
			LostPostSince = T;
		}
	}
	else
	{
		LostPostSince = -1.0;
	}
}

int32 AWRGarrison::NearestMark() const
{
	int32 Best = 0;
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Route.Num(); ++i)
	{
		const float D = FVector::Dist2D(Route[i], GetActorLocation());
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

FVector AWRGarrison::HomePoint() const
{
	// A patrol's home is its round (the nearest mark of it); everyone else's its post.
	return Route.Num() > 1 ? Route[NearestMark()] : Post;
}

void AWRGarrison::MoveToward(const FVector& At, float Speed, float StopAt)
{
	FVector To = At - GetActorLocation();
	To.Z = 0.f;
	if (To.Size() <= StopAt) return;
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	AddMovementInput(To.GetSafeNormal(), 1.f);
	// Bumping into a wall or a post: step round it (the side picked once, until it's clear).
	const double T = Now();
	if (GetVelocity().Size2D() < 30.f && !Busy())
	{
		if (StuckSince < 0.0) StuckSince = T;
		if (T - StuckSince > 0.6) AddMovementInput(FVector::CrossProduct(To.GetSafeNormal(), FVector::UpVector) * StrafeSign, 1.f);
		if (T - StuckSince > 2.5) { StrafeSign = -StrafeSign; StuckSince = T; }
	}
	else
	{
		StuckSince = -1.0;
	}
}

void AWRGarrison::FaceToward(const FVector& At, float DeltaSeconds, float Rate)
{
	const FVector To = (At - GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero()) return;
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, Rate));
}

void AWRGarrison::TickPost(float DeltaSeconds)
{
	const FWRHoldRank* R = GetRank();
	const double T = Now();
	if (Route.Num() > 1)
	{
		// The round: walk to each mark, look about there for a moment, go on.
		if (T < RoutePauseUntil) return;
		const FVector Mark = Route[RouteIndex % Route.Num()];
		if (FVector::Dist2D(GetActorLocation(), Mark) < 90.f)
		{
			RouteIndex = (RouteIndex + 1) % Route.Num();
			RoutePauseUntil = T + WRGarrisonDetail::Tn(TEXT("patrolPause"), 3.f);
			return;
		}
		MoveToward(Mark, R ? R->WalkSpeed : 160.f, 60.f);
		return;
	}
	if (FVector::Dist2D(GetActorLocation(), Post) > 70.f)
	{
		MoveToward(Post, R ? R->WalkSpeed : 160.f, 50.f);
		return;
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, PostYaw, 0.f), DeltaSeconds, 3.f));
}

void AWRGarrison::Press(EWRAbilityInput Input) { PressAbilityInput(Input); }
void AWRGarrison::Release(EWRAbilityInput Input) { ReleaseAbilityInput(Input); }

bool AWRGarrison::Busy() const
{
	return HasTag(WRTags::State_Attacking) || HasTag(WRTags::State_Dodging) || HasTag(WRTags::State_BlockStun) || HasTag(WRTags::State_ParryRecovery)
		|| HasTag(WRTags::State_Critting) || HasTag(WRTags::State_Charging);
}

float AWRGarrison::ClassReach() const
{
	return FWRWeapons::Get().ClassFor(this).Reach;
}

void AWRGarrison::SetBowOut(bool bBow)
{
	if (BowItem.IsNone()) return;
	if (bBow == IsRangedOut()) return;
	if (!bBow) SetAiming(false);
	SetRangedMode(bBow);
	bMeleeOut = !bBow;
}

void AWRGarrison::TickFight(float DeltaSeconds)
{
	AActor* Mark = Target.Get();
	if (!Mark) return;
	const float Dist = FVector::Dist2D(Mark->GetActorLocation(), GetActorLocation());
	const FWRHoldRank* R = GetRank();
	// The horn-blower's first job is the horn.
	if (R && R->HornSeconds > 0.f && !bHornBlown)
	{
		TickHorn(Mark, DeltaSeconds);
		return;
	}
	if (IsWarlord() && TickWarlord(Mark, Dist, DeltaSeconds)) return;
	if (!BowItem.IsNone() && Dist >= (R ? R->KeepMin : 400.f) * (IsRangedOut() ? 1.f : 1.6f))
	{
		TickArcher(Mark, Dist, DeltaSeconds);
		return;
	}
	if (!BowItem.IsNone()) SetBowOut(false);   // too close to loose: the dagger
	TickMelee(Mark, Dist, DeltaSeconds);
}

void AWRGarrison::TickHorn(AActor* Mark, float DeltaSeconds)
{
	const FWRHoldRank* R = GetRank();
	const double T = Now();
	FaceToward(Mark->GetActorLocation(), DeltaSeconds, 6.f);
	if (HornStarted < 0.0)
	{
		// The horn to the lips: the arm comes up (the cast clip), a shout. 2 s to stop it.
		HornStarted = T;
		MulticastBodyOneShot(TEXT("cast"));
		WRSound::Broadcast(this, TEXT("hold.horn_raise"));
		return;
	}
	if (T - HornStarted >= (R ? R->HornSeconds : 2.f))
	{
		bHornBlown = true;
		WRSound::Broadcast(this, TEXT("hold.horn"));
		UE_LOG(LogWildreach, Log, TEXT("HOLD %s: %s sounds the horn"), *HoldId.ToString(), *GetDisplayLabel());
		if (UWRStrongholdSubsystem* S = UWRStrongholdSubsystem::Get(this)) S->SoundAlarm(HoldId, Mark->GetActorLocation(), Mark, true);
	}
}

void AWRGarrison::TickArcher(AActor* Mark, float Dist, float DeltaSeconds)
{
	const FWRHoldRank* R = GetRank();
	const double T = Now();
	SetBowOut(true);
	FaceToward(Mark->GetActorLocation(), DeltaSeconds, 8.f);
	const bool bSee = CanSee(Mark, (R ? R->Sight : 3500.f) * 1.3f, 360.f);
	// An archer keeps its post (a deck, the wall walk): it only steps off the ground to keep its distance.
	const bool bHigh = Post.Z - GetActorLocation().Z < 120.f && Post.Z > 0.f && FVector::Dist2D(GetActorLocation(), Post) < 150.f && Route.Num() == 0;
	if (DrawStarted < 0.0)
	{
		if (!bHigh && Dist > (R ? R->KeepMax : 3000.f)) MoveToward(Mark->GetActorLocation(), R ? R->RunSpeed : 400.f, (R ? R->KeepMax : 3000.f) * 0.8f);
		if (T >= NextShot && bSee && Dist <= (R ? R->KeepMax : 3000.f) * 1.15f)
		{
			// The draw: the bow comes up (the aim pose and the creak, every machine): the warning before the shot.
			SetAiming(true);
			if (GetNetMode() != NM_Standalone) WRSound::Broadcast(this, TEXT("bow.draw"));
			DrawStarted = T;
		}
		return;
	}
	if (!bSee)
	{
		SetAiming(false);
		DrawStarted = -1.0;
		NextShot = T + 0.5;
		return;
	}
	if (T - DrawStarted >= (R ? R->DrawSeconds : 1.1f))
	{
		Shoot(Mark);
		SetAiming(false);
		DrawStarted = -1.0;
		NextShot = T + (R ? R->Reload : 1.4f);
	}
}

void AWRGarrison::Shoot(AActor* Mark)
{
	const FWRItemDef* Bow = FWRItemRegistry::Get().Find(BowItem);
	const FWRItemDef* Arrow = FWRItemRegistry::Get().Find(TEXT("stone_arrow"));
	const FWRHoldRank* R = GetRank();
	if (!Bow || !Mark) return;
	// Loosed at where the mark stands as it leaves the string: no lead, and it never homes after (CAL 1.13).
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 55.f);
	const FVector Aim = Mark->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	const float Speed = FMath::Max(1000.f, Bow->ProjectileSpeed);
	const float G = 980.f * Bow->ProjectileGravity;
	const FVector Flat = FVector(Aim.X - Chest.X, Aim.Y - Chest.Y, 0.f);
	const float X = Flat.Size(), Y = Aim.Z - Chest.Z;
	float Angle = FMath::Atan2(Y, FMath::Max(1.f, X));
	if (G > 1.f)
	{
		const float V2 = Speed * Speed;
		const float Root = V2 * V2 - G * (G * X * X + 2.f * Y * V2);
		if (Root >= 0.f) Angle = FMath::Atan2(V2 - FMath::Sqrt(Root), G * FMath::Max(1.f, X));
	}
	FVector Dir = Flat.GetSafeNormal() * FMath::Cos(Angle) + FVector::UpVector * FMath::Sin(Angle);
	// A little scatter at long range (people miss).
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(WRGarrisonDetail::Tn(TEXT("arrowSpread"), 1.2f)));
	FWRHit Hit;
	Hit.Instigator = this;
	Hit.Damage = (R && R->ShotDamage > 0.f ? R->ShotDamage : Bow->RangedDamage) + (Arrow ? Arrow->AmmoDamage : 0.f);
	Hit.PoiseDamage = Hit.Damage * WRTune(TEXT("ranged.poisePerDamage"), 0.4f);
	Hit.Element = WRTags::Element_Physical;
	Hit.PhysType = 3;
	Hit.Impact = 2;
	Hit.Tier = EWRHitTier::Light;
	Hit.bReflectable = true;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* P = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Chest + Dir * 45.f, Dir.Rotation(), Params))
	{
		P->InitPhysical(Hit, Dir * Speed, Bow->ProjectileGravity, Arrow && !Arrow->ProjectileMesh.IsEmpty() ? Arrow->ProjectileMesh : Bow->HeldMesh,
			Arrow && !Arrow->ProjectileMesh.IsEmpty() ? Arrow->MeshAlong : Bow->MeshAlong, Bow->RangedKind, NAME_None);
	}
	PlayAbilityShot(R ? R->Reload : 1.4f);
	WRSound::Broadcast(this, TEXT("bow.release"));
}

void AWRGarrison::TickMelee(AActor* Mark, float Dist, float DeltaSeconds)
{
	const FWRHoldRank* R = GetRank();
	const double T = Now();
	const AWRCharacterBase* Foe = Cast<AWRCharacterBase>(Mark);
	const float Reach = ClassReach() + GetSimpleCollisionRadius() + Mark->GetSimpleCollisionRadius();
	// Held things first: a raised guard, a charging heavy, a chain to continue.
	if (BlockUntil > 0.0 && T >= BlockUntil)
	{
		Release(EWRAbilityInput::Block);
		BlockUntil = 0.0;
		NextAct = T + 0.15;
	}
	if (ChargeReleaseAt > 0.0 && T >= ChargeReleaseAt)
	{
		Release(EWRAbilityInput::Heavy);
		ChargeReleaseAt = 0.0;
	}
	if (ChainLeft > 0 && HasTag(WRTags::State_ComboWindow) && T >= ChainPressAt)
	{
		--ChainLeft;
		Press(EWRAbilityInput::Light);
		Release(EWRAbilityInput::Light);
		ChainPressAt = T + 0.2;
	}
	if (!Busy() && BlockUntil <= 0.0) FaceToward(Mark->GetActorLocation(), DeltaSeconds, 7.f);
	// Its mark guarding: how long (the captain's kick waits for a held guard).
	if (Foe && Foe->HasTag(WRTags::State_Blocking)) { if (TargetGuardSince < 0.0) TargetGuardSince = T; }
	else TargetGuardSince = -1.0;
	if (Busy() || BlockUntil > 0.0) return;
	const float Stamina = GetAttributes() ? GetAttributes()->GetStamina() : 100.f;
	// Winded: back off and circle until it has its breath.
	if (Stamina < WRGarrisonDetail::Tn(TEXT("backOffStamina"), 25.f) && BackOffUntil < T) BackOffUntil = T + WRGarrisonDetail::Tn(TEXT("backOffSeconds"), 1.5f);
	if (T < BackOffUntil)
	{
		GetCharacterMovement()->MaxWalkSpeed = R ? R->WalkSpeed * 1.4f : 220.f;
		AddMovementInput(-(Mark->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 0.7f);
		AddMovementInput(FVector::CrossProduct((Mark->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), FVector::UpVector) * StrafeSign, 0.5f);
		return;
	}
	if (T < NextAct)
	{
		// Between blows: hold the gap and circle (the side flips now and then).
		if (T >= NextStrafeFlip) { StrafeSign = FMath::RandBool() ? 1.f : -1.f; NextStrafeFlip = T + FMath::FRandRange(1.2f, 2.6f); }
		if (Dist > Reach * 0.95f) MoveToward(Mark->GetActorLocation(), R ? R->RunSpeed : 420.f, Reach * 0.8f);
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = R ? R->WalkSpeed : 160.f;
			AddMovementInput(FVector::CrossProduct((Mark->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), FVector::UpVector) * StrafeSign, 0.6f);
		}
		return;
	}
	// A blow coming at it from in front: guard (a shield more often) or roll aside.
	const bool bIncoming = Foe && Dist < Reach + 250.f && (Foe->HasTag(WRTags::State_Windup) || Foe->HasTag(WRTags::State_Charging) || Foe->HasTag(WRTags::State_Casting));
	if (bIncoming)
	{
		const float Guard = bShield ? (R ? R->GuardShield : 0.5f) : (R ? R->Guard : 0.2f);
		const float Roll = FMath::FRand();
		if (Roll < Guard)
		{
			Press(EWRAbilityInput::Block);
			BlockUntil = T + FMath::FRandRange(0.6f, 1.2f);
			NextAct = T + 0.3;
			return;
		}
		if (Roll < Guard + (R ? R->Dodge : 0.15f) && Stamina > 30.f)
		{
			const FVector Side = FVector::CrossProduct((Mark->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
			AddMovementInput(Side, 1.f);
			GetCharacterMovement()->Velocity = Side * 200.f + FVector(0.f, 0.f, GetCharacterMovement()->Velocity.Z);
			Press(EWRAbilityInput::Dodge);
			Release(EWRAbilityInput::Dodge);
			NextAct = T + 0.6;
			return;
		}
	}
	if (Dist > Reach)
	{
		MoveToward(Mark->GetActorLocation(), R ? R->RunSpeed : 420.f, Reach * 0.8f);
		return;
	}
	// In reach: the captain kicks a guard held too long; else heavy, a held charged heavy, or a chain of lights.
	if (R && R->KickAfter > 0.f && TargetGuardSince >= 0.0 && T - TargetGuardSince >= R->KickAfter)
	{
		Kick(Mark);
		NextAct = T + 1.3;
		TargetGuardSince = -1.0;
		return;
	}
	const float Pick = FMath::FRand();
	if (Pick < (R ? R->Charged : 0.1f))
	{
		Press(EWRAbilityInput::Heavy);
		ChargeReleaseAt = T + FMath::FRandRange(0.55f, 0.85f);
	}
	else if (Pick < (R ? R->Charged + R->Heavy : 0.35f))
	{
		Press(EWRAbilityInput::Heavy);
		Release(EWRAbilityInput::Heavy);
	}
	else
	{
		Press(EWRAbilityInput::Light);
		Release(EWRAbilityInput::Light);
		ChainLeft = FMath::RandRange(0, (R ? R->ChainMax : 3) - 1);
		ChainPressAt = T + 0.15;
	}
	NextAct = T + FMath::FRandRange(WRGarrisonDetail::Tn(TEXT("actMin"), 0.45f), WRGarrisonDetail::Tn(TEXT("actMax"), 1.1f));
}

void AWRGarrison::Kick(AActor* Mark)
{
	// The captain's kick (the hero's kick clip): a short wind-up, then a foot into the guard. It breaks a held guard
	// (its stamina cost empties a shield) and does little else: the opening is what it's for.
	MulticastBodyOneShot(TEXT("kick"));
	WRSound::Broadcast(this, TEXT("creature.humanoid.attack"));
	SetCombatTag(WRTags::State_Windup, true);
	TWeakObjectPtr<AWRGarrison> Weak(this);
	TWeakObjectPtr<AActor> WeakMark(Mark);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Weak, WeakMark]()
	{
		AWRGarrison* Me = Weak.Get();
		if (!Me || !Me->IsAlive()) return;
		Me->SetCombatTag(WRTags::State_Windup, false);
		AWRCharacterBase* Foe = Cast<AWRCharacterBase>(WeakMark.Get());
		if (!Foe || !Foe->IsAlive() || Me->HasTag(WRTags::Status_Staggered)) return;
		const FVector To = Foe->GetActorLocation() - Me->GetActorLocation();
		if (To.Size2D() > 220.f + Foe->GetSimpleCollisionRadius() || FVector::DotProduct(To.GetSafeNormal2D(), Me->GetActorForwardVector()) < 0.6f) return;
		FWRHit Hit;
		Hit.Instigator = Me;
		Hit.Damage = WRGarrisonDetail::Tn(TEXT("kickDamage"), 12.f);
		Hit.PoiseDamage = WRGarrisonDetail::Tn(TEXT("kickPoise"), 60.f);
		Hit.StaminaDamage = WRGarrisonDetail::Tn(TEXT("kickStamina"), 120.f);
		Hit.Element = WRTags::Element_Physical;
		Hit.PhysType = 2;
		Hit.Impact = 3;
		Hit.bMelee = true;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = Me->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Foe, Hit);
	}), WRGarrisonDetail::Tn(TEXT("kickWindup"), 0.45f), false);
}

// ---------------------------------------------------------------- the warlord

bool AWRGarrison::InWard(const FVector& At, float Margin) const
{
	const FWRHoldDef* D = FWRStrongholds::Find(HoldId);
	if (!D) return true;
	const FVector Local = FRotator(0.f, D->Yaw, 0.f).UnrotateVector(At - D->WardCentre);
	return FMath::Abs(Local.X) <= D->WardHalf.X + Margin && FMath::Abs(Local.Y) <= D->WardHalf.Y + Margin && FMath::Abs(Local.Z) < 600.f;
}

void AWRGarrison::UpdateParticipants()
{
	const FWRWarlordDef* W = GetWarlord();
	if (!W) return;
	// Who is in the fight: survivors and their tames in the ward (each tame counts, as at every boss).
	int32 N = 0;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (*It == this || !It->IsAlive() || !IsValidMark(*It) || !InWard(It->GetActorLocation(), 200.f)) continue;
		++N;
	}
	const double T = Now();
	if (N == 0)
	{
		if (bEngaged)
		{
			if (EmptySince < 0.0) EmptySince = T;
			if (T - EmptySince >= W->ResetSeconds) ResetWarlord();
		}
		return;
	}
	EmptySince = -1.0;
	bEngaged = true;
	if (N != Participants && GetAttributes())
	{
		// Health scales with the fighters, the share already lost kept (CAL 1.14: +80% each).
		const float Fraction = GetAttributes()->GetHealth() / FMath::Max(1.f, GetAttributes()->GetMaxHealth());
		const float Max = SoloHealth * (1.f + W->PerFighter * float(N - 1));
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), Max);
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Max * Fraction);
		Participants = N;
	}
}

void AWRGarrison::ResetWarlord()
{
	// Nobody left in the ward: he walks back to his place whole, phase 1 (the fight starts over for whoever comes next).
	const FWRWarlordDef* W = GetWarlord();
	bEngaged = false;
	EmptySince = -1.0;
	Participants = 1;
	Phase = 1;
	bPhaseTwoPending = false;
	Ledger.Reset();
	MoveReadyAt.Reset();
	Weapon.Element = NAME_None;
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), SoloHealth);
	AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), SoloHealth);
	const FWRHoldRank* R = GetRank();
	GetCharacterMovement()->MaxWalkSpeed = R ? R->WalkSpeed : 170.f;
	Target = nullptr;
	SetState(EWRGarrisonState::Return);
	UE_LOG(LogWildreach, Log, TEXT("HOLD %s: %s resets (nobody in the ward)"), *HoldId.ToString(), W ? *W->Name : TEXT("warlord"));
}

void AWRGarrison::CheckPhase()
{
	const FWRWarlordDef* W = GetWarlord();
	if (!W || Phase >= 2 || bPhaseTwoPending || !GetAttributes()) return;
	if (GetAttributes()->GetHealth() > GetAttributes()->GetMaxHealth() * W->Phase2At) return;
	// Never during a crit or a stance break: the crit finishes, then the phase turns (CAL 1.4).
	if (HasTag(WRTags::State_BeingCrit) || HasTag(WRTags::Status_StanceBroken)) return;
	bPhaseTwoPending = true;
}

int32 AWRGarrison::PickMove(AActor* Mark, float Dist, AActor*& OutMark) const
{
	const FWRWarlordDef* W = GetWarlord();
	OutMark = Mark;
	if (!W) return INDEX_NONE;
	const double T = Now();
	if (bPhaseTwoPending)
	{
		for (int32 i = 0; i < W->Moves.Num(); ++i) if (W->Moves[i].bTransition) return i;
	}
	float Total = 0.f;
	TArray<TPair<int32, float>> Pool;
	for (int32 i = 0; i < W->Moves.Num(); ++i)
	{
		const FWRHoldMove& M = W->Moves[i];
		if (M.bTransition || Phase < M.Phase || Phase > M.MaxPhase) continue;
		if (const double* Ready = MoveReadyAt.Find(M.Id); Ready && T < *Ready) continue;
		if (M.Id == LastMove && SameInARow >= 2) continue;
		if (M.Shape == TEXT("throw") && Dist < 400.f) continue;   // nothing thrown inside 4 m
		const float Wt = Dist < 400.f ? M.WClose : Dist < 1200.f ? M.WMid : M.WFar;
		if (Wt <= 0.f) continue;
		Pool.Add({ i, Wt });
		Total += Wt;
	}
	if (Total <= 0.f) return INDEX_NONE;
	float Roll = FMath::FRand() * Total;
	int32 Pick = Pool.Last().Key;
	for (const TPair<int32, float>& P : Pool) { if ((Roll -= P.Value) <= 0.f) { Pick = P.Key; break; } }
	// A tame-punisher goes for the furthest tame in the ward first (CAL R1).
	if (W->Moves[Pick].bTamePunish)
	{
		float Far = -1.f;
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !It->IsTamed() || !InWard(It->GetActorLocation(), 100.f)) continue;
			const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (D > Far && D <= W->Moves[Pick].Range) { Far = D; OutMark = *It; }
		}
	}
	return Pick;
}

bool AWRGarrison::TickWarlord(AActor* Mark, float Dist, float DeltaSeconds)
{
	const double T = Now();
	if (Busy()) return false;   // its weapon's own swing is playing out
	// Its own moves share the time with its weapon's set: once a second it weighs one of them (about half the time it
	// takes it); the phase change can't wait.
	if (!bPhaseTwoPending)
	{
		if (T < NextAct || T < NextSpecialRoll) return false;
		NextSpecialRoll = T + WRGarrisonDetail::Tn(TEXT("warlordRollSeconds"), 1.f);
		if (FMath::FRand() > WRGarrisonDetail::Tn(TEXT("warlordMoveShare"), 0.5f)) return false;   // the class move set (TickMelee) this time
	}
	AActor* Aim = Mark;
	const int32 Pick = PickMove(Mark, Dist, Aim);
	if (Pick == INDEX_NONE) return false;
	StartMove(Pick, Aim);
	return true;
}

void AWRGarrison::StartMove(int32 Index, AActor* Mark)
{
	const FWRWarlordDef* W = GetWarlord();
	if (!W || !W->Moves.IsValidIndex(Index) || !Mark) return;
	const FWRHoldMove& M = W->Moves[Index];
	const double T = Now();
	CurrentMove = Index;
	MoveStarted = T;
	MoveRepeat = 0;
	bMoveStruck = false;
	bFinalCue = false;
	MoveAim = Mark->GetActorLocation();
	LeapFrom = GetActorLocation();
	LungeHits.Reset();
	Target = Mark;
	const float Hold = M.HoldOn <= 1 ? M.Hold : 0.f;
	MoveStrikeAt = T + M.Windup + Hold;
	MoveEndsAt = MoveStrikeAt + M.Active + M.Punish;
	MoveReadyAt.Add(M.Id, T + M.Cooldown);
	SameInARow = M.Id == LastMove ? SameInARow + 1 : 1;
	LastMove = M.Id;
	// Committed: hyperarmoured from the first tell to the last strike; the wind-up reads (counter-hits land double).
	SetCombatTag(WRTags::State_Hyperarmor, true);
	SetCombatTag(WRTags::State_Windup, true);
	// The tell: the clip (its swing warped to this row's timing, or a one-shot), a shout, and on the big ones the ground
	// shows where it lands (crimson only for the unblockable).
	if (M.Clip == TEXT("heavy") || M.Clip == TEXT("light"))
	{
		PlaySwing(M.Clip == TEXT("heavy") ? EWRSwingKind::Heavy : EWRSwingKind::Light, M.Windup + Hold + M.Active, 0, M.Windup + Hold, M.Active, M.Punish);
	}
	else
	{
		MulticastBodyOneShot(M.Clip);
	}
	WRSound::Broadcast(this, M.bTransition ? FName(TEXT("creature.humanoid.aggro")) : FName(TEXT("creature.humanoid.attack")));
	if (M.Shape == TEXT("self") || M.Shape == TEXT("target") || M.bUnblockable)
	{
		const FLinearColor Col = M.bUnblockable ? FLinearColor(0.75f, 0.05f, 0.05f, 0.55f) : FLinearColor(0.85f, 0.85f, 0.8f, 0.35f);
		const FVector At = M.Shape == TEXT("self") ? GetActorLocation() : M.Shape == TEXT("target") ? MoveAim : GetActorLocation() + GetActorForwardVector() * M.Range * 0.5f;
		const FVector Ground = At - FVector(0.f, 0.f, M.Shape == TEXT("target") ? 90.f : GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 2.f);
		const bool bBox = M.Shape == TEXT("line");
		MoveMarker = AWRGroundMarker::Spawn(GetWorld(), Ground, FRotator(0.f, GetActorRotation().Yaw, 0.f),
			bBox ? FVector2D(M.Range, M.Size * 2.f) : FVector2D(M.Size, M.Size), bBox, Col, M.Windup + Hold + M.Active);
	}
	UE_LOG(LogWildreach, Verbose, TEXT("HOLD %s: %s starts %s"), *HoldId.ToString(), *GetDisplayLabel(), *M.Id.ToString());
}

bool AWRGarrison::InMoveShape(const FWRHoldMove& M, const FVector& Point, float Radius) const
{
	const FVector Me = GetActorLocation();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	if (FMath::Abs(Point.Z - Me.Z) > 260.f) return false;
	if (M.Shape == TEXT("self")) return FVector::Dist2D(Point, Me) <= M.Size + Radius;
	if (M.Shape == TEXT("target")) return FVector::Dist2D(Point, MoveAim) <= M.Size + Radius;
	if (M.Shape == TEXT("line"))
	{
		const FVector Rel = Point - LeapFrom;
		const FVector Dir = (MoveAim - LeapFrom).GetSafeNormal2D();
		const float Along = FVector::DotProduct(Rel, Dir);
		const float Across = FVector::CrossProduct(Dir, Rel).Z;
		return Along >= -Radius && Along <= M.Range + Radius && FMath::Abs(Across) <= M.Size + Radius;
	}
	// cone
	const FVector To = Point - Me;
	if (To.Size2D() > M.Range + Radius) return false;
	if (To.Size2D() < Radius + 40.f) return true;
	return FVector::DotProduct(To.GetSafeNormal2D(), Fwd) >= FMath::Cos(FMath::DegreesToRadians(M.Size));
}

void AWRGarrison::TickMove(float DeltaSeconds)
{
	const FWRWarlordDef* W = GetWarlord();
	if (!W || !W->Moves.IsValidIndex(CurrentMove)) { CurrentMove = INDEX_NONE; return; }
	const FWRHoldMove& M = W->Moves[CurrentMove];
	const double T = Now();
	AActor* Mark = Target.Get();
	// Tracking: it turns toward the mark through the first half of the wind-up, then commits (CAL 1.13 track 0.5).
	if (!bMoveStruck && Mark && T - MoveStarted < M.Windup * 0.5)
	{
		FaceToward(Mark->GetActorLocation(), DeltaSeconds, 5.f);
		if (M.Shape == TEXT("target") || M.Shape == TEXT("line")) MoveAim = Mark->GetActorLocation();
		if (MoveMarker.IsValid() && M.Shape == TEXT("target"))
		{
			MoveMarker->SetActorLocation(MoveAim - FVector(0.f, 0.f, 90.f));
		}
	}
	// The delayed ones: the final cue (a glint on the weapon and a sting) 0.2 s before contact, so the delay reads.
	if (!bFinalCue && M.Hold > 0.f && T >= MoveStrikeAt - 0.2)
	{
		bFinalCue = true;
		WRFx::BroadcastOn(this, TEXT("charge.full"), TEXT("weapon"), 1.f, TEXT("charge.full"), 0.3f);
	}
	// A leap: off the ground so it comes down on the mark at the strike (its own arc, the same on every machine).
	const float Air = WRGarrisonDetail::Tn(TEXT("leapSeconds"), 0.55f);
	if (M.Shape == TEXT("target") && M.Range > 500.f && MoveRepeat == 0 && !bMoveStruck && T >= MoveStrikeAt - Air && FVector::Dist2D(GetActorLocation(), MoveAim) > 150.f && GetCharacterMovement()->IsMovingOnGround())
	{
		const FVector From = GetActorLocation();
		FVector To = MoveAim - (MoveAim - From).GetSafeNormal2D() * 90.f;
		const float G = -GetCharacterMovement()->GetGravityZ();
		const FVector Flat = FVector(To.X - From.X, To.Y - From.Y, 0.f) / Air;
		const float Vz = ((To.Z - From.Z) + 0.5f * G * Air * Air) / Air;
		LaunchCharacter(Flat + FVector(0.f, 0.f, Vz), true, true);
	}
	// A lunge along the line through its active time.
	if (M.Shape == TEXT("line") && M.bLunge && T >= MoveStrikeAt && T < MoveStrikeAt + M.Active)
	{
		const FVector Dir = (MoveAim - LeapFrom).GetSafeNormal2D();
		GetCharacterMovement()->Velocity = Dir * (M.Range / FMath::Max(0.1f, M.Active)) + FVector(0.f, 0.f, GetCharacterMovement()->Velocity.Z);
		// Hits along the way (each body once).
		StrikeMove();
		bMoveStruck = true;
	}
	else if (!bMoveStruck && T >= MoveStrikeAt)
	{
		StrikeMove();
		bMoveStruck = true;
	}
	if (bMoveStruck && MoveRepeat + 1 < M.Repeats && T >= MoveStrikeAt + M.Gap)
	{
		// The next blow of the combo (the hold on the one it's written on).
		++MoveRepeat;
		bMoveStruck = false;
		bFinalCue = false;
		const float Hold = M.HoldOn == MoveRepeat + 1 ? M.Hold : 0.f;
		MoveStrikeAt = T + M.Windup * 0.6f + Hold;
		MoveEndsAt = MoveStrikeAt + M.Active + M.Punish;
		if (Mark) MoveAim = Mark->GetActorLocation();
		if (M.Clip == TEXT("heavy") || M.Clip == TEXT("light")) PlaySwing(MoveRepeat + 1 == M.Repeats ? EWRSwingKind::Heavy : EWRSwingKind::Light, M.Windup * 0.6f + Hold + M.Active, uint8(MoveRepeat % 3), M.Windup * 0.6f + Hold, M.Active, M.Punish);
		else MulticastBodyOneShot(M.Clip);
	}
	if (T >= MoveStrikeAt + M.Active) SetCombatTag(WRTags::State_Hyperarmor, false);
	if (T >= MoveEndsAt && (bMoveStruck || M.bLunge) && MoveRepeat + 1 >= M.Repeats)
	{
		CurrentMove = INDEX_NONE;
		SetCombatTag(WRTags::State_Windup, false);
		SetCombatTag(WRTags::State_Hyperarmor, false);
		NextAct = T + FMath::FRandRange(0.3f, 0.7f);
	}
}

void AWRGarrison::StrikeMove()
{
	const FWRWarlordDef* W = GetWarlord();
	if (!W || !W->Moves.IsValidIndex(CurrentMove)) return;
	const FWRHoldMove& M = W->Moves[CurrentMove];
	SetCombatTag(WRTags::State_Windup, false);
	const bool bFirstContact = !bMoveStruck;
	if (M.Shape == TEXT("throw"))
	{
		if (!bFirstContact) return;
		// The harpoon: thrown at where the mark stands as it leaves the hand, straight, never homing; its speed keeps the
		// wind-up plus the flight over 0.9 s at any range it's thrown from (CAL 1.13).
		AActor* Mark = Target.Get();
		const FVector From = GetActorLocation() + FVector(0.f, 0.f, 70.f) + GetActorForwardVector() * 60.f;
		const FVector To = (Mark ? Mark->GetActorLocation() : MoveAim) + FVector(0.f, 0.f, 20.f);
		const FVector Dir = (To - From).GetSafeNormal();
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = M.Damage;
		Hit.PoiseDamage = M.Poise;
		Hit.Element = WRGarrisonDetail::ElementTag(M.Element);
		if (M.Element == TEXT("fire")) Hit.BurnBuildup = M.Buildup; else if (M.Element == TEXT("frost")) Hit.FrostBuildup = M.Buildup;
		Hit.Impact = M.Impact;
		Hit.PhysType = 3;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.bReflectable = true;
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const float Speed = FMath::Min(M.ProjectileSpeed, WRTune(TEXT("creatureAI.maxProjectileSpeed"), 1000.f));
		if (AWRProjectile* P = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Dir.Rotation(), Params))
		{
			P->InitPhysical(Hit, Dir * Speed, 0.f, M.Mesh, M.MeshAlong, TEXT("spear"), NAME_None);
		}
		WRSound::Broadcast(this, TEXT("throw"));
		return;
	}
	FWRHit Hit;
	Hit.Instigator = this;
	Hit.Damage = M.Damage;
	Hit.PoiseDamage = M.Poise;
	Hit.Element = WRGarrisonDetail::ElementTag(M.Element);
	if (M.Element == TEXT("fire")) Hit.BurnBuildup = M.Buildup; else if (M.Element == TEXT("frost")) Hit.FrostBuildup = M.Buildup;
	Hit.Impact = M.Impact;
	Hit.PhysType = 2;
	Hit.bMelee = M.Shape == TEXT("cone");
	Hit.bHeavy = true;
	Hit.Tier = EWRHitTier::Heavy;
	Hit.bParryable = M.bParryable;
	Hit.bLow = M.bLow;
	Hit.bUnblockable = M.bUnblockable;
	Hit.bGrab = M.bGrab;
	Hit.bIgnoreHyperarmor = M.bGrab;
	if (M.bGuardBreak) Hit.StaminaDamage = WRGarrisonDetail::Tn(TEXT("kickStamina"), 120.f);
	Hit.Origin = GetActorLocation();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Victim = *It;
		if (Victim == this || !Victim->IsAlive() || !IsHostileTo(Victim)) continue;
		if (!InMoveShape(M, Victim->GetActorLocation(), Victim->GetSimpleCollisionRadius())) continue;
		if (M.bLunge && LungeHits.Contains(Victim)) continue;
		if (M.bLunge) LungeHits.Add(Victim);
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}
	if (!bFirstContact) return;
	const FVector At = M.Shape == TEXT("target") ? MoveAim : M.Shape == TEXT("self") ? GetActorLocation() : GetActorLocation() + GetActorForwardVector() * M.Range * 0.6f;
	const FVector Ground = At - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (!M.Fx.IsNone() || !M.Sound.IsNone()) WRFx::Broadcast(this, M.Fx.IsNone() ? FName(TEXT("stomp")) : M.Fx, Ground, FRotator::ZeroRotator, FMath::Max(1.f, M.Size / 250.f), M.Sound);
	// Burning ground / ice where it struck (it hurts what stands in it for a few seconds).
	if (M.HazardSeconds > 0.f)
	{
		AWRHazard::Spawn(GetWorld(), this, Ground, M.HazardRadius, M.HazardSeconds, WRGarrisonDetail::Tn(TEXT("hazardDps"), 10.f), M.Element == TEXT("frost") ? FName(TEXT("ice")) : FName(TEXT("lava")));
	}
	// The transition lands: phase 2 (his blows carry his element now, and he's quicker).
	if (M.bTransition && Phase < 2)
	{
		Phase = 2;
		bPhaseTwoPending = false;
		Weapon.Element = W->Element;
		const FWRHoldRank* R = GetRank();
		GetCharacterMovement()->MaxWalkSpeed = (R ? R->RunSpeed : 460.f) * W->Phase2Speed;
		NextEmberAt = 0.0;   // the weapon's embers / rime from now (Think renews them)
		UE_LOG(LogWildreach, Log, TEXT("HOLD %s: %s enters phase 2"), *HoldId.ToString(), *W->Name);
	}
}

// ---------------------------------------------------------------- death

void AWRGarrison::OnDeath(AActor* Killer)
{
	// A person falls: no respawn here (the fort decides), no player death flow. Its loot to whoever killed it.
	WRSound::Broadcast(this, TEXT("creature.humanoid.death"));
	SetAiming(false);
	if (MoveMarker.IsValid()) MoveMarker->Destroy();
	const FWRHoldRank* R = GetRank();
	AWRPlayerState* PS = WRGarrisonDetail::CreditFor(Killer);
	if (!IsWarlord() && R && PS && PS->GetInventory())
	{
		const float Mult = WRTune(TEXT("server.gatherMultiplier"), 1.f);
		for (const TPair<FName, int32>& L : R->Loot)
		{
			if (FWRItemRegistry::Get().Find(L.Key)) PS->GetInventory()->AddItem(L.Key, FMath::Max(1, FMath::RoundToInt(L.Value * Mult)));
		}
		if (R->RareChance > 0.f && FMath::FRand() < R->RareChance)
		{
			const FName Item = R->RareItem == TEXT("weapon") ? MeleeWeapon : R->RareItem == TEXT("chest") ? ChestPiece : R->RareItem;
			if (!Item.IsNone() && FWRItemRegistry::Get().Find(Item))
			{
				PS->GetInventory()->AddItem(Item, 1);
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(FString::Printf(TEXT("%s's %s is yours"), *GetDisplayLabel(), *FWRItemRegistry::Get().Find(Item)->Name));
			}
		}
	}
	if (UWRStrongholdSubsystem* S = UWRStrongholdSubsystem::Get(this))
	{
		if (IsWarlord()) S->WarlordDied(this, Killer);
		else S->SoldierDied(this, Killer);
	}
	DetachFromControllerPendingDestroy();
	SetLifeSpan(WRGarrisonDetail::Tn(TEXT("bodySeconds"), 20.f));
}
