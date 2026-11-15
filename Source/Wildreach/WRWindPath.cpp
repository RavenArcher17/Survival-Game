// The Wind path's rules (cooks/biome-sky.md 2.18-2.19): Momentum, Featherfall, knockback, the passives, the combos, the
// body. See WRWindPath.h. The abilities are in WRAbilitiesWind.cpp.

#include "WRWindPath.h"
#include "WRInventory.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRLightningPath.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlantSummon.h"
#include "WRProjectile.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"

namespace WRWind
{
	UE_DEFINE_GAMEPLAY_TAG(Element_Wind, "Element.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Passive_Featherfall, "Passive.Featherfall");
	UE_DEFINE_GAMEPLAY_TAG(State_Spinning, "State.Wind.Spinning");
	UE_DEFINE_GAMEPLAY_TAG(State_SkyLord, "State.Wind.SkyLord");
	UE_DEFINE_GAMEPLAY_TAG(State_HighMomentum, "State.Wind.HighMomentum");
	UE_DEFINE_GAMEPLAY_TAG(State_AirDash, "State.Wind.AirDash");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Firestorm, "Combo.Firestorm");
	UE_DEFINE_GAMEPLAY_TAG(Combo_EmberRain, "Combo.EmberRain");
	UE_DEFINE_GAMEPLAY_TAG(Combo_IceArrows, "Combo.IceArrows");
	UE_DEFINE_GAMEPLAY_TAG(Combo_AvalanchePlunge, "Combo.AvalanchePlunge");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Tailcharge, "Combo.Tailcharge");
	UE_DEFINE_GAMEPLAY_TAG(Combo_JetStream, "Combo.JetStream");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BleedingArrows, "Combo.BleedingArrows");
	UE_DEFINE_GAMEPLAY_TAG(Combo_HexStorm, "Combo.HexStorm");
	UE_DEFINE_GAMEPLAY_TAG(Combo_LeafGlide, "Combo.LeafGlide");
	UE_DEFINE_GAMEPLAY_TAG(Combo_WindborneSeeds, "Combo.WindborneSeeds");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SoulWind, "Combo.SoulWind");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ReapersCyclone, "Combo.ReapersCyclone");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SandBlast, "Combo.SandBlast");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Sunshot, "Combo.Sunshot");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SeaSpray, "Combo.SeaSpray");
	UE_DEFINE_GAMEPLAY_TAG(Combo_HurricaneEye, "Combo.HurricaneEye");
}

namespace WRWindImpl
{
	static const FName WiWindId(TEXT("wind"));

	/** A survivor's Momentum state (server). */
	struct FWiOwnerState
	{
		float SinceGain = 100.f;
		double LastFightAt = -100.0;
		int32 ArrowsLanded = 0;
	};
	static TMap<TWeakObjectPtr<const AActor>, FWiOwnerState>& WiOwners()
	{
		static TMap<TWeakObjectPtr<const AActor>, FWiOwnerState> Map;
		return Map;
	}

	/** Every machine: the per-airtime state of a body (Air dash once per airtime, Sky lord's fall cover, Lift's hop). */
	struct FWiMoveState
	{
		bool bAirDashUsed = false;
		bool bFallCovered = false;
		bool bWasBoltDash = false;
		bool bSetJumpCount = false;
		double BoltDashEndAt = -100.0;
		float BaseJumpZ = -1.f;
	};
	static TMap<TWeakObjectPtr<const AActor>, FWiMoveState>& WiMoves()
	{
		static TMap<TWeakObjectPtr<const AActor>, FWiMoveState> Map;
		return Map;
	}

	/** Server: the tornadoes standing (Eye of the storm, Hurricane eye). */
	struct FWiTornado
	{
		TWeakObjectPtr<AActor> Actor;
		float Radius = 250.f;
	};
	static TMap<TWeakObjectPtr<const AActor>, FWiTornado>& WiTornadoes()
	{
		static TMap<TWeakObjectPtr<const AActor>, FWiTornado> Map;
		return Map;
	}

	static float WiMom(const TCHAR* Field, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("resources.momentum.")) + Field, Fallback); }
	static float WiPass(const TCHAR* Field, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("passives.wind.")) + Field, Fallback); }
	static float WiCombo(const TCHAR* Combo, const TCHAR* Key, float Fallback) { return FWRComboRegistry::Get().Num(FName(Combo), FName(Key), Fallback); }

	static double WiNow(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? World->GetTimeSeconds() : 0.0;
	}

	static const UWRAttributeSet* WiAttrOf(const AActor* Who)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C ? C->GetAttributes() : nullptr;
	}

	static bool WiHasTag(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}

	static int32 WiRankOf(const AActor* Who, FName Node)
	{
		const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
		const FWRPathProgress* Prog = P && P->IsActivePath(WiWindId) ? P->GetProgress(WiWindId) : nullptr;
		return Prog ? Prog->GetRank(Node) : 0;
	}

	static void WiSetTag(AWRCharacterBase* Who, const FGameplayTag& Tag, bool bOn)
	{
		UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return;
		}
		const int32 Want = bOn ? 1 : 0;
		if (ASC->GetTagCount(Tag) != Want)
		{
			ASC->SetLooseGameplayTagCount(Tag, Want, EGameplayTagReplicationState::TagOnly);
		}
	}

	/** In the air: jumping, falling, gliding, hovering, carried up a column (the movement's own word, both machines). */
	static bool WiAirborne(const ACharacter* Who)
	{
		const UCharacterMovementComponent* Move = Who ? Who->GetCharacterMovement() : nullptr;
		if (!Move)
		{
			return false;
		}
		return Move->IsFalling() || Move->IsFlying() || WRSkyAir::IsGliding(Who);
	}

	/** The ammo row's own "pierce" (items.json ammo: the stormglass and palp-spine arrows). */
	static int32 WiAmmoPierce(FName AmmoId)
	{
		static TMap<FName, int32> Cache;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("items.json"));
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), Items))
			{
				for (const TSharedPtr<FJsonValue>& V : *Items)
				{
					const TSharedPtr<FJsonObject> I = V->AsObject();
					const TSharedPtr<FJsonObject>* Ammo = nullptr;
					double Pierce = 0.0;
					if (I.IsValid() && I->TryGetObjectField(TEXT("ammo"), Ammo) && (*Ammo)->TryGetNumberField(TEXT("pierce"), Pierce) && Pierce > 0.0)
					{
						Cache.Add(FName(*I->GetStringField(TEXT("id"))), FMath::RoundToInt(Pierce));
					}
				}
			}
		}
		const int32* P = Cache.Find(AmmoId);
		return P ? *P : 0;
	}

	/** The player_art.json paths.wind row's skin number (the drift and the glow the look starts from). */
	static TSharedPtr<FJsonObject> WiWindRow()
	{
		const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
		const TSharedPtr<FJsonObject>* Paths = nullptr;
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (Art.IsValid() && Art->TryGetObjectField(TEXT("paths"), Paths) && (*Paths)->TryGetObjectField(TEXT("wind"), Row))
		{
			return *Row;
		}
		return nullptr;
	}
}

using namespace WRWindImpl;

FName WRWind::PathId()
{
	return WiWindId;
}

bool WRWind::Walks(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(WiWindId);
}

// ---------------------------------------------------------------- Featherfall and the air

bool WRWind::NoFallDamage(const AActor* Who)
{
	if (!Who)
	{
		return false;
	}
	if (WiHasTag(Who, Passive_Featherfall) || WiHasTag(Who, State_SkyLord))
	{
		return true;
	}
	const FWiMoveState* S = WiMoves().Find(Who);
	return S && S->bFallCovered;
}

float WRWind::GliderStaminaScale(const AActor* Who)
{
	return WiHasTag(Who, Passive_Featherfall) ? WiPass(TEXT("featherfallGliderStamina"), 0.5f) : 1.f;
}

bool WRWind::CanPathGlide(const AActor* Who)
{
	return Who && UWRPathComponent::HasNode(Who, WiWindId, TEXT("wind_glide"));
}

void WRWind::PathGlideStats(const AActor* Who, float& OutForward, float& OutSink)
{
	const FWRTuning& Tn = FWRTuning::Get();
	OutForward = Tn.Num(TEXT("spells.windGlide.forward"), 950.f);
	OutSink = Tn.Num(TEXT("spells.windGlide.sink"), 230.f) * GlideSinkScale(Who);
}

float WRWind::GlideSinkScale(const AActor* Who)
{
	if (!WiHasTag(Who, Combo_LeafGlide))
	{
		return 1.f;
	}
	const float Leaf = WiCombo(TEXT("leaf_glide"), TEXT("sinkScale"), 0.6f);
	const FVector At = Who->GetActorLocation();
	if (WRBiome::At(Who, At) == TEXT("forest"))
	{
		return Leaf;
	}
	// Among trees: five distinct trees within 20 m (the dressing's nearest tree from the middle and four points 10 m out).
	const AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(Who->GetWorld());
	if (!Dressing)
	{
		return 1.f;
	}
	const float Reach = WiCombo(TEXT("leaf_glide"), TEXT("treeRadius"), 2000.f);
	const int32 Want = FMath::RoundToInt(WiCombo(TEXT("leaf_glide"), TEXT("trees"), 5.f));
	TArray<FVector> Found;
	const FVector Probes[5] = { FVector::ZeroVector, FVector(Reach * 0.5f, 0, 0), FVector(-Reach * 0.5f, 0, 0), FVector(0, Reach * 0.5f, 0), FVector(0, -Reach * 0.5f, 0) };
	for (const FVector& Off : Probes)
	{
		FVector Tree;
		if (Dressing->FindNearest(At + Off, TEXT("tree"), Reach * 0.5f, Tree) && FVector::Dist2D(Tree, At) <= Reach
			&& !Found.ContainsByPredicate([&Tree](const FVector& F) { return FVector::DistSquared2D(F, Tree) < 100.f; }))
		{
			Found.Add(Tree);
		}
	}
	return Found.Num() >= Want ? Leaf : 1.f;
}

bool WRWind::PathGlideTick(AWRCharacterBase* Who, float DeltaTime, bool bInColumn)
{
	if (!Who || !Who->HasAuthority())
	{
		return true;
	}
	if (bInColumn)
	{
		return true;   // free in an updraft (cooks/biome-sky.md 2.18.3)
	}
	const FWRTuning& Tn = FWRTuning::Get();
	const float MomCost = Tn.Num(TEXT("spells.windGlide.momentumPerSecond"), 4.f) * DeltaTime;
	if (MomentumValue(Who) >= MomCost)
	{
		SpendMomentum(Who, MomCost);
		return true;
	}
	UWRAttributeSet* A = Who->GetAttributes();
	if (!A || A->GetStamina() <= 0.f)
	{
		return false;
	}
	Who->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(),
		A->GetStamina() - Tn.Num(TEXT("spells.windGlide.staminaPerSecond"), 3.f) * DeltaTime * GliderStaminaScale(Who));
	Who->Status->NotifyStaminaUsed();
	return true;
}

void WRWind::OnEnteredUpdraft(const AActor* Who)
{
	if (FWiMoveState* S = WiMoves().Find(Who))
	{
		S->bAirDashUsed = false;
	}
}

bool WRWind::HasAirHop(const AActor* Who)
{
	return WiRankOf(Who, TEXT("wind_lift")) >= FMath::RoundToInt(WiPass(TEXT("hopRank"), 3.f));
}

// ---------------------------------------------------------------- Momentum

float WRWind::MomentumValue(const AActor* Who)
{
	const UWRAttributeSet* A = WiAttrOf(Who);
	return A ? A->GetMomentum() : 0.f;
}

float WRWind::MomentumFraction(const AActor* Who)
{
	const UWRAttributeSet* A = WiAttrOf(Who);
	return A && A->GetMaxMomentum() > 0.f ? FMath::Clamp(A->GetMomentum() / A->GetMaxMomentum(), 0.f, 1.f) : 0.f;
}

bool WRWind::HighMomentum(const AActor* Who)
{
	return Walks(Who) && MomentumValue(Who) >= WiMom(TEXT("highAt"), 60.f);
}

void WRWind::GainMomentum(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority() || Amount <= 0.f || !Walks(Who) || !Cast<AWRCharacter>(Who))
	{
		return;
	}
	UWRAttributeSet* A = Who->GetAttributes();
	if (!A)
	{
		return;
	}
	// The Wind Mask of Aeolith: Momentum +20% (cooks/biome-sky.md 2.11).
	Amount *= 1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("momentumGain"));
	Who->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetMomentumAttribute(), FMath::Min(A->GetMaxMomentum(), A->GetMomentum() + Amount));
	WiOwners().FindOrAdd(Who).SinceGain = 0.f;
}

float WRWind::SpendMomentum(AWRCharacterBase* Who, float Amount)
{
	UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || !Who->HasAuthority() || Amount <= 0.f)
	{
		return 0.f;
	}
	const float Taken = FMath::Min(Amount, A->GetMomentum());
	Who->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetMomentumAttribute(), A->GetMomentum() - Taken);
	return Taken;
}

float WRWind::PayMomentum(AWRCharacterBase* Who, float Cost, bool bFreeInSkyLord)
{
	if (!Who || Cost <= 0.f)
	{
		return 1.f;
	}
	if (bFreeInSkyLord && Who->HasTag(State_SkyLord))
	{
		return 1.f;
	}
	if (MomentumValue(Who) >= Cost)
	{
		if (Who->HasAuthority())
		{
			SpendMomentum(Who, Cost);
		}
		return 1.f;
	}
	// No dead button: short of Momentum it still goes off, at half effect, for its stamina alone.
	UE_LOG(LogWildreach, Log, TEXT("WIND %s short of Momentum (%.0f of %.0f): half effect"), *Who->GetName(), MomentumValue(Who), Cost);
	return WiMom(TEXT("halfEffect"), 0.5f);
}

void WRWind::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	// Survivors only (a ridden mount shares its rider's player state, not their Momentum).
	if (!Who || !Who->HasAuthority() || !Cast<AWRCharacter>(Who))
	{
		return;
	}
	UWRAttributeSet* A = Who->GetAttributes();
	UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent();
	if (!A || !ASC)
	{
		return;
	}
	if (!Walks(Who))
	{
		if (A->GetMomentum() > 0.f)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMomentumAttribute(), 0.f);
		}
		WiSetTag(Who, State_HighMomentum, false);
		return;
	}
	FWiOwnerState& S = WiOwners().FindOrAdd(Who);
	const UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(Who->GetCharacterMovement());
	const bool bAir = Who->IsAlive() && WiAirborne(Who) && !(Move && Move->IsSwimming());
	const bool bSprint = Who->IsAlive() && Move && Move->IsSprinting() && Who->GetVelocity().Size2D() > WiMom(TEXT("sprintSpeed"), 650.f);
	float Gain = 0.f;
	if (bAir) Gain += WiMom(TEXT("airborne"), 14.f) * DeltaTime;
	else if (bSprint) Gain += WiMom(TEXT("sprint"), 8.f) * DeltaTime;
	if (Gain > 0.f)
	{
		GainMomentum(Who, Gain);
	}
	else
	{
		S.SinceGain += DeltaTime;
		if (A->GetMomentum() > 0.f && S.SinceGain >= WiMom(TEXT("decayDelay"), 2.5f))
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMomentumAttribute(), FMath::Max(0.f, A->GetMomentum() - WiMom(TEXT("decayPerSecond"), 10.f) * DeltaTime));
		}
	}
	// The look's fight state: high Momentum while you've hit or been hit lately (every machine reads the tag).
	const bool bFight = WiNow(Who) - S.LastFightAt <= WiMom(TEXT("fightSeconds"), 8.f);
	WiSetTag(Who, State_HighMomentum, bFight && A->GetMomentum() >= WiMom(TEXT("highAt"), 60.f));
	// Tailcharge (Lightning + Wind 25): sprinting and gliding build Charge.
	if (Who->HasTag(Combo_Tailcharge) && Who->IsAlive() && (bSprint || WRSkyAir::IsGliding(Who)))
	{
		WRLightning::GainCharge(Who, WiCombo(TEXT("tailcharge"), TEXT("chargePerSecond"), 6.f) * DeltaTime);
	}
}

// ---------------------------------------------------------------- every machine, every frame

void WRWind::TickMovement(ACharacter* Who, float DeltaTime)
{
	UCharacterMovementComponent* Move = Who ? Who->GetCharacterMovement() : nullptr;
	// Survivors only: a ridden mount has its rider's player state, but its own legs.
	if (!Move || !Who->GetPlayerState() || !Cast<AWRCharacter>(Who))
	{
		return;
	}
	FWiMoveState& S = WiMoves().FindOrAdd(Who);
	if (S.BaseJumpZ < 0.f)
	{
		S.BaseJumpZ = Move->JumpZVelocity;
	}
	const bool bGrounded = Move->IsMovingOnGround() || Move->IsSwimming();
	if (bGrounded)
	{
		S.bAirDashUsed = false;
		S.bFallCovered = false;
	}
	// Lift: +12% jump height a rank (the speed goes as the root); rank 3 adds a second, small hop once per airtime. The
	// jump count and the speed are set from replicated path ranks and the CMC's own jump count, so the owner's prediction
	// and the server agree.
	const int32 Lift = WiRankOf(Who, TEXT("wind_lift"));
	const float Height = 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Who, TEXT("jumpHeight")));
	const bool bHop = Lift >= FMath::RoundToInt(WiPass(TEXT("hopRank"), 3.f));
	if (bHop)
	{
		Who->JumpMaxCount = FMath::Max(Who->JumpMaxCount, 2);
		S.bSetJumpCount = true;
		const float G = 980.f * FMath::Max(0.1f, Move->GravityScale);
		Move->JumpZVelocity = Who->JumpCurrentCount >= 1 && !bGrounded ? FMath::Sqrt(2.f * G * WiPass(TEXT("hopHeight"), 200.f)) : S.BaseJumpZ * FMath::Sqrt(Height);
	}
	else
	{
		if (S.bSetJumpCount)
		{
			Who->JumpMaxCount = 1;
			S.bSetJumpCount = false;
		}
		Move->JumpZVelocity = S.BaseJumpZ * FMath::Sqrt(Height);
	}
	// Jet stream's clock: when this body's last Bolt dash ended (this machine's view).
	const bool bBolt = WiHasTag(Who, WRLightning::State_BoltDash);
	if (S.bWasBoltDash && !bBolt)
	{
		S.BoltDashEndAt = WiNow(Who);
	}
	S.bWasBoltDash = bBolt;
}

bool WRWind::AirDashReady(const AActor* Who)
{
	const FWiMoveState* S = WiMoves().Find(Who);
	return !S || !S->bAirDashUsed;
}

void WRWind::NoteAirDash(const AActor* Who)
{
	if (Who) WiMoves().FindOrAdd(Who).bAirDashUsed = true;
}

bool WRWind::BoltDashJustEnded(const AActor* Who)
{
	const FWiMoveState* S = WiMoves().Find(Who);
	return S && (WiHasTag(Who, WRLightning::State_BoltDash) || WiNow(Who) - S->BoltDashEndAt <= WiCombo(TEXT("jet_stream"), TEXT("window"), 0.5f));
}

void WRWind::CoverFall(const AActor* Who)
{
	if (Who) WiMoves().FindOrAdd(Who).bFallCovered = true;
}

bool WRWind::InSkyLord(const AActor* Who)
{
	return WiHasTag(Who, State_SkyLord);
}

// ---------------------------------------------------------------- knockback

bool WRWind::IsBoss(const AWRCharacterBase* Who)
{
	return Who && UWRCombatLibrary::StanceKind(Who) != TEXT("normal");
}

FName WRWind::TierOf(const AWRCharacterBase* Who)
{
	const AWRCreature* C = Cast<AWRCreature>(Who);
	return C && C->GetDef() ? C->GetDef()->Tier : FName(TEXT("highland"));
}

void WRWind::Knock(AWRCharacterBase* By, AWRCharacterBase* Target, const FVector& Dir, float Distance, float Seconds, FName Source)
{
	if (!Target || !Target->HasAuthority() || !Target->IsAlive() || IsBoss(Target) || Distance <= 1.f)
	{
		return;
	}
	const FWRTuning& Tn = FWRTuning::Get();
	// Survivors (PvP) take the full push and their brace / boots / roll through WRSkyAir; creatures by their tier.
	float Share = 1.f;
	if (!Target->GetPlayerState())
	{
		const FName Tier = TierOf(Target);
		Share = Tier == TEXT("lowland") ? Tn.Num(TEXT("knockback.lowland"), 1.f) : Tier == TEXT("midland") ? Tn.Num(TEXT("knockback.midland"), 0.75f) : Tn.Num(TEXT("knockback.highland"), 0.5f);
	}
	if (By && HighMomentum(By))
	{
		Share *= WiMom(TEXT("knockbackAtHigh"), 1.25f);
	}
	const float D = Distance * Share;
	if (D <= 1.f)
	{
		return;
	}
	FVector Flat = Dir.GetSafeNormal2D();
	if (Flat.IsNearlyZero() && By)
	{
		Flat = (Target->GetActorLocation() - By->GetActorLocation()).GetSafeNormal2D();
	}
	if (Flat.IsNearlyZero())
	{
		Flat = Target->GetActorForwardVector() * -1.f;
	}
	const float Secs = FMath::Max(0.08f, Seconds);
	const float Speed = D / Secs;
	// A little lift off the ground, so the push carries instead of dying in the walk's friction (combat.md 2.11).
	const FVector Velocity = Flat * Speed + FVector(0.f, 0.f, Tn.Num(TEXT("knockback.lift"), 0.18f) * Speed);
	WRSkyAir::Push(Target, Velocity, Secs, Source, By);

	UWorld* World = Target->GetWorld();
	// A wall within the push: the body slams into it when it gets there (+20 stance, + Off balance).
	{
		FHitResult Wall;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRWindSlam), false, Target);
		const float R = Target->GetSimpleCollisionRadius() * 0.9f;
		const float HH = Target->GetSimpleCollisionHalfHeight() * 0.7f;
		const FVector From = Target->GetActorLocation() + FVector(0.f, 0.f, HH * 0.3f);
		if (World->SweepSingleByChannel(Wall, From, From + Flat * D, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(R, HH), Query)
			&& Wall.bBlockingHit && Wall.ImpactNormal.Z < 0.6f)
		{
			const float At = FMath::Max(0.05f, Wall.Distance / Speed);
			const FVector SlamAt = Wall.ImpactPoint;
			TWeakObjectPtr<AWRCharacterBase> WeakT(Target), WeakBy(By);
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Target, [WeakT, WeakBy, SlamAt]()
			{
				AWRCharacterBase* V = WeakT.Get();
				if (!V || !V->IsAlive())
				{
					return;
				}
				AWRCharacterBase* B = WeakBy.Get();
				FWRHit Slam;
				Slam.Instigator = B;
				Slam.Damage = WRTune(TEXT("knockback.wallSlamDamage"), 4.f);
				Slam.StanceDamage = WRTune(TEXT("knockback.wallSlamStance"), 20.f) * (1.f + (B ? UWRPathComponent::Stat(B, TEXT("windStance")) : 0.f));
				Slam.PoiseDamage = WRTune(TEXT("knockback.wallSlamPoise"), 15.f);
				Slam.Element = WRTags::Element_Physical;
				Slam.PathId = WiWindId;
				Slam.bUnblockable = true;
				Slam.Impact = 2;
				Slam.Tier = EWRHitTier::Heavy;
				Slam.PhysType = 2;
				Slam.Origin = SlamAt;
				Slam.ImpactPoint = SlamAt;
				UWRCombatLibrary::ApplyHit(V, Slam);
				WRFx::Broadcast(V, TEXT("wind.slam"), SlamAt, FRotator::ZeroRotator, 1.f, TEXT("wind.slam"));
			}), At, false);
		}
	}
	// Firestorm (Fire + Wind 25): a burning body thrown by your wind spreads its burn where it stops.
	if (By && By->HasTag(Combo_Firestorm) && Target->HasTag(WRTags::Status_Burn))
	{
		TWeakObjectPtr<AWRCharacterBase> WeakT(Target), WeakBy(By);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Target, [WeakT, WeakBy]()
		{
			AWRCharacterBase* V = WeakT.Get();
			AWRCharacterBase* B = WeakBy.Get();
			if (!V || !B)
			{
				return;
			}
			const float Radius = WiCombo(TEXT("firestorm"), TEXT("spreadRadius"), 300.f);
			const float Burn = WiCombo(TEXT("firestorm"), TEXT("spreadBurn"), 25.f);
			for (TActorIterator<AWRCharacterBase> It(V->GetWorld()); It; ++It)
			{
				if (*It != V && *It != B && It->IsAlive() && B->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), V->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
				{
					It->Status->AddBuildup(WRTags::Element_Fire, Burn, B);
				}
			}
			WRFx::Broadcast(V, TEXT("fire.impact"), V->GetActorLocation(), FRotator(90.f, 0.f, 0.f), Radius / 200.f, TEXT("fire.impact"));
		}), Secs, false);
	}
}

void WRWind::WindHit(AWRCharacterBase* By, AWRCharacterBase* Target, float Damage, float Poise, float StanceMult, const FVector& Origin, bool bSpell, bool bGustLike, bool bDamageOverTime)
{
	if (!Target || !Target->IsAlive())
	{
		return;
	}
	FWRHit Hit;
	Hit.Instigator = By;
	Hit.Damage = Damage;
	Hit.PoiseDamage = Poise;
	Hit.Element = Element_Wind;
	Hit.PathId = WiWindId;
	Hit.bSpell = bSpell;
	Hit.bDamageOverTime = bDamageOverTime;
	Hit.Origin = Origin;
	Hit.ImpactPoint = Target->GetActorLocation();
	Hit.Impact = bDamageOverTime ? 1 : 2;
	Hit.Tier = EWRHitTier::Light;
	if (!bDamageOverTime)
	{
		Hit.StanceDamage = Damage * WRTune(TEXT("stance.spellPerDamage"), 0.3f) * StanceMult;
	}
	if (bGustLike && By)
	{
		// Firestorm burns, Sand blast blinds, Sea spray soaks what your gusts and vortices throw.
		if (By->HasTag(Combo_Firestorm)) Hit.BurnBuildup += WiCombo(TEXT("firestorm"), TEXT("burn"), 40.f);
		if (By->HasTag(Combo_SandBlast)) Hit.Buildups.FindOrAdd(TEXT("blind")) += WiCombo(TEXT("sand_blast"), TEXT("blind"), 40.f);
		if (By->HasTag(Combo_SeaSpray)) Hit.Buildups.FindOrAdd(TEXT("soak")) += WiCombo(TEXT("sea_spray"), TEXT("soak"), 40.f);
	}
	UWRCombatLibrary::ApplyHit(Target, Hit);
}

// ---------------------------------------------------------------- passives

float WRWind::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	float Scale = 1.f;
	// Aerial mastery: +6% a rank on anything you deal while in the air (weapons, bows, spells, Plunge).
	if (Attacker && !Hit.bDamageOverTime && Cast<AWRCharacter>(Attacker) && WiAirborne(Attacker))
	{
		Scale *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("airDamage"));
	}
	// Eye of the storm: -20% taken while you spin (Whirlwind, a Vortex cast) or stand in your own Tornado's eye.
	if (Target && (Target->HasTag(State_Spinning) || InOwnTornado(Target)) && UWRPathComponent::HasNode(Target, WiWindId, TEXT("eye_of_the_storm")))
	{
		Scale *= WiPass(TEXT("eyeOfTheStorm"), 0.8f);
	}
	return Scale;
}

float WRWind::StanceScale(const AWRCharacterBase* Attacker, const FWRHit& Hit)
{
	if (!Attacker || (Hit.Element != Element_Wind && Hit.PathId != WiWindId))
	{
		return 1.f;
	}
	return 1.f + UWRPathComponent::Stat(Attacker, TEXT("windStance"));
}

float WRWind::StaminaRegenScale(const AActor* Who)
{
	return HighMomentum(Who) ? 1.f + WiMom(TEXT("staminaRegenAtHigh"), 0.15f) : 1.f;
}

void WRWind::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	const double TimeNow = WiNow(Target);
	if (Target && Walks(Target)) WiOwners().FindOrAdd(Target).LastFightAt = TimeNow;
	if (!Attacker || Hit.bDamageOverTime || !Walks(Attacker))
	{
		return;
	}
	WiOwners().FindOrAdd(Attacker).LastFightAt = TimeNow;
	// Fighting in the air feeds the wind: +4 a blow landed while you're airborne.
	if (WiAirborne(Attacker))
	{
		GainMomentum(Attacker, WiMom(TEXT("airHit"), 4.f));
	}
}

// ---------------------------------------------------------------- ranged

float WRWind::DrawTimeScale(const AActor* Who)
{
	return FMath::Max(WiPass(TEXT("drawFloor"), 0.5f), 1.f - UWRPathComponent::Stat(Who, TEXT("drawTime")));
}

float WRWind::ProjectileSpeedScale(const AActor* Who)
{
	return 1.f + UWRPathComponent::Stat(Who, TEXT("projectileSpeed"));
}

float WRWind::ProjectileDamageScale(const AActor* Who)
{
	return 1.f + UWRPathComponent::Stat(Who, TEXT("projectileDamage"));
}

int32 WRWind::ShotPierce(const AActor* Who, FName AmmoId)
{
	return WiAmmoPierce(AmmoId) + (Who && UWRPathComponent::HasNode(Who, WiWindId, TEXT("piercing_gale")) ? 1 : 0);
}

void WRWind::OnShotFired(AWRCharacterBase* Who, AWRProjectile* Shot, bool bAimed)
{
	if (!Who || !Who->HasAuthority() || !bAimed || !UWRPathComponent::HasNode(Who, WiWindId, TEXT("tailwind_shot")))
	{
		return;
	}
	// Tailwind shot: a fully drawn shot pushes you back (further in the air, without your air dash); hold Sprint to stand.
	const UWRCharacterMovementComponent* Move = Cast<UWRCharacterMovementComponent>(Who->GetCharacterMovement());
	if (Move && Move->bWantsToSprint)
	{
		return;
	}
	const bool bAir = WiAirborne(Who);
	const float Dist = bAir ? WiPass(TEXT("tailwindAirDistance"), 400.f) : WiPass(TEXT("tailwindDistance"), 250.f);
	const float Secs = FMath::Max(0.05f, WiPass(TEXT("tailwindSeconds"), 0.15f));
	const FVector Back = -FRotator(0.f, Who->GetControlRotation().Yaw, 0.f).Vector();
	Who->LaunchCharacter(Back * (Dist / Secs), true, false);
	WRFx::Broadcast(Who, TEXT("wind.dash"), Who->GetActorLocation(), Back.Rotation(), 0.6f, TEXT("wind.dash"));
}

void WRWind::OnArrowLanded(AWRCharacterBase* Who, const FVector& At)
{
	if (!Who || !Who->HasAuthority() || !Who->HasTag(Combo_WindborneSeeds))
	{
		return;
	}
	// Windborne seeds (Nature + Wind 50): every 4th arrow that comes down plants a Sprout turret there (half health, 10 s).
	FWiOwnerState& S = WiOwners().FindOrAdd(Who);
	if (++S.ArrowsLanded % FMath::Max(1, FMath::RoundToInt(WiCombo(TEXT("windborne_seeds"), TEXT("every"), 4.f))) != 0)
	{
		return;
	}
	if (AWRPlantSummon* Turret = AWRPlantSummon::Plant(AWRSproutTurret::StaticClass(), Who, At, FMath::FRandRange(0.f, 360.f)))
	{
		Turret->ScaleLife(WiCombo(TEXT("windborne_seeds"), TEXT("healthShare"), 0.5f), WiCombo(TEXT("windborne_seeds"), TEXT("seconds"), 10.f));
		WRFx::Broadcast(Who, TEXT("nature.sprout"), At, FRotator::ZeroRotator, 1.f, TEXT("nature.cast"));
	}
}

// ---------------------------------------------------------------- combos read elsewhere

float WRWind::SoulPickupScale(const AActor* Who)
{
	return WiHasTag(Who, Combo_SoulWind) ? WiCombo(TEXT("soul_wind"), TEXT("radiusScale"), 2.5f) : 1.f;
}

void WRWind::SetTornado(const AWRCharacterBase* Caster, AActor* Tornado, float Radius)
{
	if (!Caster)
	{
		return;
	}
	if (!Tornado)
	{
		WiTornadoes().Remove(Caster);
		return;
	}
	FWiTornado& T = WiTornadoes().FindOrAdd(Caster);
	T.Actor = Tornado;
	T.Radius = Radius;
}

bool WRWind::InOwnTornado(const AWRCharacterBase* Who)
{
	const FWiTornado* T = Who ? WiTornadoes().Find(Who) : nullptr;
	const AActor* A = T ? T->Actor.Get() : nullptr;
	return A && FVector::Dist2D(A->GetActorLocation(), Who->GetActorLocation()) <= T->Radius && FMath::Abs(A->GetActorLocation().Z - Who->GetActorLocation().Z) < 600.f;
}

// ---------------------------------------------------------------- the body

uint32 WRWind::LookSig(const AActor* Hero)
{
	return WiHasTag(Hero, State_HighMomentum) ? 0x51D0u : 0u;
}

void WRWind::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Hair,
	FName Main, float MainS, FName Sub, float SubS)
{
	if (!Hero || (Main != WiWindId && Sub != WiWindId) || !WiHasTag(Hero, State_HighMomentum))
	{
		return;   // the row's own drift, glow and stir stand (cooks/biome-sky.md 2.18.5 stages ride the strength)
	}
	const TSharedPtr<FJsonObject> Row = WiWindRow();
	const TSharedPtr<FJsonObject>* SkinRow = nullptr;
	double Nits = 0.15;
	FLinearColor Pan(0.02f, 0.f, 0.f, 0.f);
	if (Row.IsValid() && Row->TryGetObjectField(TEXT("skin"), SkinRow))
	{
		(*SkinRow)->TryGetNumberField(TEXT("EmissiveNits"), Nits);
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if ((*SkinRow)->TryGetArrayField(TEXT("Pan"), P) && P->Num() >= 2)
		{
			Pan = FLinearColor(float((*P)[0]->AsNumber()), float((*P)[1]->AsNumber()), P->Num() > 2 ? float((*P)[2]->AsNumber()) : 0.f, P->Num() > 3 ? float((*P)[3]->AsNumber()) : 0.f);
		}
	}
	const FWRTuning& Tn = FWRTuning::Get();
	// In a fight at >= 60 Momentum: the spirals' drift x4 and +1 nit (on whichever side the Wind path draws).
	const float Drift = Tn.Num(TEXT("passives.wind.lookDrift"), 4.f);
	const float Glow = Tn.Num(TEXT("passives.wind.lookNits"), 1.f);
	for (const TPair<FString, float>& Side : { TPair<FString, float>(TEXT("Main"), Main == WiWindId ? MainS : 0.f), TPair<FString, float>(TEXT("Sub"), Sub == WiWindId ? SubS : 0.f) })
	{
		if (Side.Value <= 0.f)
		{
			continue;
		}
		for (UMaterialInstanceDynamic* MID : Skin)
		{
			if (!MID) continue;
			MID->SetVectorParameterValue(FName(*(Side.Key + TEXT("Pan"))), Pan * Drift);
			MID->SetScalarParameterValue(FName(*(Side.Key + TEXT("EmissiveNits"))), float(Nits) + Glow);
		}
	}
	// The hair streams as if in a 10 m/s wind (main path drives the hair).
	if (Main == WiWindId)
	{
		for (UMaterialInstanceDynamic* MID : Hair)
		{
			if (!MID) continue;
			MID->SetScalarParameterValue(TEXT("PathStir"), Tn.Num(TEXT("passives.wind.lookHairStir"), 3.f) * MainS);
			MID->SetScalarParameterValue(TEXT("StirHz"), Tn.Num(TEXT("passives.wind.lookHairHz"), 1.6f));
			MID->SetScalarParameterValue(TEXT("PathLift"), Tn.Num(TEXT("passives.wind.lookHairLift"), 0.8f) * MainS);
		}
	}
}
