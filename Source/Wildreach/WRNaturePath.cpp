#include "WRNaturePath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBuilding.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRDeeds.h"
#include "WRForestTypes.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlantSummon.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WRToolMesh.h"                        // the temple buds (modelled like the held tools)
#include "ProceduralMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"

namespace WRNature
{
	UE_DEFINE_GAMEPLAY_TAG(Element_Nature, "Element.Nature");
	UE_DEFINE_GAMEPLAY_TAG(Passive_Rooted, "Passive.Rooted");
	UE_DEFINE_GAMEPLAY_TAG(Status_RootedHeal, "Status.Nature.RootedHeal");
	UE_DEFINE_GAMEPLAY_TAG(Ground_Lush, "State.Nature.Ground.Lush");
	UE_DEFINE_GAMEPLAY_TAG(Ground_Normal, "State.Nature.Ground.Normal");
	UE_DEFINE_GAMEPLAY_TAG(Ground_Barren, "State.Nature.Ground.Barren");
	UE_DEFINE_GAMEPLAY_TAG(Ability_BriarStrike, "Ability.Nature.BriarStrike");
	UE_DEFINE_GAMEPLAY_TAG(Ability_VineLash, "Ability.Nature.VineLash");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SmoulderRoots, "Combo.SmoulderRoots");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ForestFire, "Combo.ForestFire");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Evergreen, "Combo.Evergreen");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrozenGrove, "Combo.FrozenGrove");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Quickening, "Combo.Quickening");
	UE_DEFINE_GAMEPLAY_TAG(Combo_LivingConductor, "Combo.LivingConductor");
	UE_DEFINE_GAMEPLAY_TAG(Combo_RedSap, "Combo.RedSap");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ThornFeast, "Combo.ThornFeast");
}

namespace WRNatureImpl
{
	static const FName PathId(TEXT("nature"));

	/** A survivor's sampler and Rooted state (server). */
	struct FOwnerState
	{
		float SampleClock = 100.f;
		WRNature::EGround Ground = WRNature::EGround::Normal;
		float StillFor = 0.f;
		bool bHealing = false;
		double LastCreak = -100.0;
		double LastHairs = -100.0;
	};

	static TMap<TWeakObjectPtr<AWRCharacterBase>, FOwnerState>& Owners()
	{
		static TMap<TWeakObjectPtr<AWRCharacterBase>, FOwnerState> Map;
		return Map;
	}

	/** Entangled bodies (server): when the hold ends, and when its immunity ends. */
	struct FHold
	{
		double Until = 0.0;
		double ImmuneUntil = 0.0;
	};
	static TMap<TWeakObjectPtr<const AActor>, FHold>& Holds()
	{
		static TMap<TWeakObjectPtr<const AActor>, FHold> Map;
		return Map;
	}

	/** Server slows (an Entangle's boss fallback, Strangling vines on a major boss). */
	struct FSlow
	{
		double Until = 0.0;
		float Mult = 1.f;
	};
	static TMap<TWeakObjectPtr<const AActor>, FSlow>& Slows()
	{
		static TMap<TWeakObjectPtr<const AActor>, FSlow> Map;
		return Map;
	}

	/** The last Nature blow that carried bleed onto a body (Barbs, Thorn feast): who, and when (server time). */
	struct FBleedNote
	{
		TWeakObjectPtr<AWRCharacterBase> By;
		double At = -1.0;
	};
	static TMap<TWeakObjectPtr<const AActor>, FBleedNote>& BleedNotes()
	{
		static TMap<TWeakObjectPtr<const AActor>, FBleedNote> Map;
		return Map;
	}

	/** Healing zones (Sanctuary pools, World tree crowns). */
	struct FZone
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<AWRCharacterBase> Owner;
		float Radius = 0.f;
		bool bShield = false;
	};
	static TArray<FZone>& Zones()
	{
		static TArray<FZone> List;
		return List;
	}

	/** This machine: the last refused press for Growth, per body. */
	static TMap<TWeakObjectPtr<const AActor>, double>& Refusals()
	{
		static TMap<TWeakObjectPtr<const AActor>, double> Map;
		return Map;
	}

	static const UWRAttributeSet* AttrOf(const AActor* Who)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C ? C->GetAttributes() : nullptr;
	}

	static FVector FeetOf(const AWRCharacterBase* Who)
	{
		return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight());
	}

	static double Now(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? World->GetTimeSeconds() : 0.0;
	}

	static void SetTag(AWRCharacterBase* Who, const FGameplayTag& Tag, bool bOn)
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

	/** A boss (mini or major) by the stance rules: roots never hold one (cooks/build-biome-forest.md 2.15). */
	static bool IsBoss(const AWRCharacterBase* Who)
	{
		return Who && UWRCombatLibrary::StanceKind(Who) != TEXT("normal");
	}

	/** Any Plant-tagged actor or component (a plant summon, a crop, a hedge, a tagged forest tree) within Radius of
	 *  Feet, or a standing tree or shrub of the island's / the forest's decoration. */
	static bool PlantsNear(const AWRCharacterBase* Who, const FVector& Feet, float Radius)
	{
		UWorld* World = Who->GetWorld();
		for (TActorIterator<AWRPlantSummon> It(World); It; ++It)
		{
			if (It->IsStanding() && FVector::DistSquared(It->GetActorLocation(), Feet) < FMath::Square(Radius + It->GetSimpleCollisionHalfHeight()))
			{
				return true;
			}
		}
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WRNatureGround), false, Who);
		if (World->OverlapMultiByObjectType(Overlaps, Feet, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeSphere(Radius), Params))
		{
			const FName Plant = WRNature::PlantTag();
			for (const FOverlapResult& O : Overlaps)
			{
				const AActor* A = O.GetActor();
				const UPrimitiveComponent* C = O.GetComponent();
				if ((A && A != Who && A->ActorHasTag(Plant)) || (C && C->ComponentHasTag(Plant)))
				{
					return true;
				}
			}
		}
		if (const AWRDressing* Dressing = UWRDressingSubsystem::FindDressing(World))
		{
			FVector At;
			if (Dressing->FindNearest(Feet, TEXT("tree"), Radius, At) || Dressing->FindNearest(Feet, TEXT("shrub"), Radius, At))
			{
				return true;
			}
		}
		return false;
	}

	/** Who is standing on a built floor (its material), else None. */
	static FName FloorUnder(const AWRCharacterBase* Who, const FVector& Feet)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRNatureFloor), false, Who);
		if (!Who->GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 40.f), Feet - FVector(0.f, 0.f, 120.f), ECC_Visibility, Query))
		{
			return NAME_None;
		}
		const AActor* Ground = Hit.GetActor();
		const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Ground);
		if (!Piece && Ground)
		{
			Piece = Cast<AWRBuildPiece>(Ground->GetOwner());
		}
		if (!Piece)
		{
			return NAME_None;
		}
		const FWRPieceDef* Def = Piece->GetDef();
		return Def && !Def->Material.IsNone() ? Def->Material : FName(TEXT("stone"));
	}

	static float RateFor(WRNature::EGround G)
	{
		switch (G)
		{
		case WRNature::EGround::Lush: return WRTune(TEXT("resources.growth.lush"), 6.f);
		case WRNature::EGround::Normal: return WRTune(TEXT("resources.growth.normal"), 3.f);
		default: return WRTune(TEXT("resources.growth.barren"), 1.f);
		}
	}

	static void AddGrowth(AWRCharacterBase* Who, float Amount)
	{
		UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
		UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
		if (!ASC || !Attr || Amount <= 0.f)
		{
			return;
		}
		// Marked by corruption fills path resources slower; worn growthGain gear, prestige (Deeper marks) and talismans
		// scale it as Heat, Chill and Charge are.
		if (const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(Who))
		{
			Amount *= Survival->ResourceGainMult();
		}
		Amount *= FMath::Max(0.f, 1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("growthGain")));
		Amount *= WRDeeds::PrestigeResource(Who, PathId) * WRDeeds::ResourceGainScale(Who);
		if (Attr->GetGrowth() < Attr->GetMaxGrowth())
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetGrowthAttribute(), FMath::Min(Attr->GetMaxGrowth(), Attr->GetGrowth() + Amount));
		}
	}

	/** Prunes the server maps of bodies that are gone. */
	static void Prune()
	{
		for (auto It = Holds().CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
		for (auto It = Slows().CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
		for (auto It = BleedNotes().CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
		for (auto It = Owners().CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
		Zones().RemoveAll([](const FZone& Z) { return !Z.Actor.IsValid(); });
	}
}

// ---------------------------------------------------------------- Growth

FName WRNature::PlantTag()
{
	static const FName Tag(TEXT("Plant"));
	return Tag;
}

bool WRNature::Walks(const AActor* Who)
{
	const UWRPathComponent* Paths = UWRPathComponent::FindFor(Who);
	return Paths && Paths->IsActivePath(WRNatureImpl::PathId);
}

float WRNature::GrowthValue(const AActor* Who)
{
	const UWRAttributeSet* A = WRNatureImpl::AttrOf(Who);
	return A ? A->GetGrowth() : 0.f;
}

float WRNature::GrowthFraction(const AActor* Who)
{
	const UWRAttributeSet* A = WRNatureImpl::AttrOf(Who);
	return A && A->GetMaxGrowth() > 0.f ? FMath::Clamp(A->GetGrowth() / A->GetMaxGrowth(), 0.f, 1.f) : 0.f;
}

bool WRNature::HasGrowth(const AActor* Who, float Amount)
{
	return Amount <= 0.f || GrowthValue(Who) + 0.01f >= Amount;
}

bool WRNature::SpendGrowth(AWRCharacterBase* Who, float Amount)
{
	UAbilitySystemComponent* ASC = Who && Who->HasAuthority() ? Who->GetAbilitySystemComponent() : nullptr;
	UWRAttributeSet* Attr = Who ? Who->GetAttributes() : nullptr;
	if (!ASC || !Attr)
	{
		return false;
	}
	if (Amount <= 0.f)
	{
		return true;
	}
	if (Attr->GetGrowth() + 0.01f < Amount)
	{
		return false;
	}
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetGrowthAttribute(), FMath::Max(0.f, Attr->GetGrowth() - Amount));
	return true;
}

void WRNature::NoteRefused(const AActor* Who)
{
	if (Who)
	{
		WRNatureImpl::Refusals().Add(Who, WRNatureImpl::Now(Who));
	}
}

float WRNature::RefusedFlash(const AActor* Who)
{
	const double* At = Who ? WRNatureImpl::Refusals().Find(Who) : nullptr;
	if (!At)
	{
		return 0.f;
	}
	const float Len = WRTune(TEXT("passives.natureRefuseFlash"), 0.35f);
	const float Age = float(WRNatureImpl::Now(Who) - *At);
	return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
}

WRNature::EGround WRNature::GroundOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C)
	{
		return EGround::Normal;
	}
	if (C->HasAuthority())
	{
		if (const WRNatureImpl::FOwnerState* S = WRNatureImpl::Owners().Find(const_cast<AWRCharacterBase*>(C)))
		{
			return S->Ground;
		}
	}
	return C->HasTag(Ground_Lush) ? EGround::Lush : C->HasTag(Ground_Barren) ? EGround::Barren : EGround::Normal;
}

WRNature::EGround WRNature::ClassifyGround(const AActor* Who, const FVector& Feet)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C || !C->GetWorld())
	{
		return EGround::Normal;
	}
	const float Radius = WRTune(TEXT("resources.growth.sampleRadius"), 800.f);
	const bool bEvergreen = C->HasTag(Combo_Evergreen);
	// Lava and ice under your feet: barren (Evergreen: ice counts as Normal, cooks/biome-forest.md 2.17 combos).
	const FName Hazard = FWRBiomeHazards::KindAt(Feet);
	if (Hazard == TEXT("lava"))
	{
		return EGround::Barren;
	}
	if (Hazard == TEXT("ice"))
	{
		return bEvergreen ? EGround::Normal : EGround::Barren;
	}
	const bool bPlants = WRNatureImpl::PlantsNear(C, Feet, Radius);
	// A cave or a built floor: barren, unless plants grow there (potted plants on a floor, moss in a cave): Normal.
	if (WRBiome::IsUnderground(C, Feet) || !WRNatureImpl::FloorUnder(C, Feet).IsNone())
	{
		return bPlants ? EGround::Normal : EGround::Barren;
	}
	// Plants within 8 m (your plants, crops, hedges, the trees and shrubs around you) make any open ground Lush.
	if (bPlants)
	{
		return EGround::Lush;
	}
	const FName Biome = WRBiome::At(C, Feet);
	if (Biome == TEXT("forest"))
	{
		// The Ancient forest's sub-zones are all grass, moss, crops and forest floor; the Spine's bare face is not.
		const TCHAR Zone = WRForest::ZoneAt(C, Feet);
		if (Zone >= TEXT('1') && Zone <= TEXT('5'))
		{
			return EGround::Lush;
		}
		return Zone == TEXT('s') ? EGround::Barren : EGround::Normal;
	}
	if (Biome == TEXT("swamp"))
	{
		return EGround::Lush;   // reed beds and moss
	}
	if (Biome == TEXT("tundra"))
	{
		return bEvergreen ? EGround::Normal : EGround::Barren;   // snow
	}
	if (Biome == TEXT("volcanic") || Biome == TEXT("wasteland"))
	{
		return EGround::Barren;   // ash and bare lava rock, void ground (their shrubs and trees count as plants above)
	}
	// Elsewhere (Storm peaks' granite and turf, anything new): by the ground's own surface.
	const FName Surface = WRSound::SurfaceUnder(C);
	if (Surface == TEXT("grass"))
	{
		return EGround::Lush;
	}
	if (Surface == TEXT("sand") || (Surface == TEXT("snow") && !bEvergreen))
	{
		return EGround::Barren;
	}
	return EGround::Normal;
}

void WRNature::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	using namespace WRNatureImpl;
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState())
	{
		return;
	}
	if (!Walks(Who) || !Who->IsAlive())
	{
		if (Owners().Remove(Who) > 0)
		{
			SetTag(Who, Ground_Lush, false);
			SetTag(Who, Ground_Normal, false);
			SetTag(Who, Ground_Barren, false);
			SetTag(Who, Status_RootedHeal, false);
		}
		return;
	}
	FOwnerState& S = Owners().FindOrAdd(Who);
	const double Time = Now(Who);

	// The sampler: every growthSampleSeconds the ground within 8 m is read (cooks/biome-forest.md 2.17), and Growth
	// regrows by it the whole time between (in and out of combat), x Sap.
	S.SampleClock += DeltaTime;
	if (S.SampleClock >= WRTune(TEXT("resources.growth.sampleSeconds"), 0.5f))
	{
		S.SampleClock = 0.f;
		S.Ground = ClassifyGround(Who, FeetOf(Who));
		SetTag(Who, Ground_Lush, S.Ground == EGround::Lush);
		SetTag(Who, Ground_Normal, S.Ground == EGround::Normal);
		SetTag(Who, Ground_Barren, S.Ground == EGround::Barren);
		if (FMath::FRand() < 0.02f)
		{
			Prune();
		}
	}
	AddGrowth(Who, RateFor(S.Ground) * (1.f + UWRPathComponent::Stat(Who, TEXT("growthRegen")) + UWRInventoryComponent::GearStatFor(Who, TEXT("growthRegen"))) * DeltaTime);

	// Rooted (the root passive, main path only): 1.5 s with no move input and no roll, then 1% of max health a second
	// (half on barren ground). Attacking, casting and guarding in place are fine; any damage starts the wait again.
	if (!Who->HasTag(Passive_Rooted))
	{
		S.StillFor = 0.f;
		if (S.bHealing)
		{
			S.bHealing = false;
			SetTag(Who, Status_RootedHeal, false);
		}
		return;
	}
	const UCharacterMovementComponent* Move = Who->GetCharacterMovement();
	const bool bMoving = !Move || Move->GetCurrentAcceleration().SizeSquared2D() > 1.f || Move->IsFalling() || Move->IsSwimming()
		|| Who->GetVelocity().Size2D() > WRTune(TEXT("passives.rootedStillSpeed"), 30.f) || Who->HasTag(WRTags::State_Dodging) || Who->HasTag(WRTags::State_Mounted);
	S.StillFor = bMoving ? 0.f : S.StillFor + DeltaTime;
	const UWRAttributeSet* Attr = Who->GetAttributes();
	const bool bHeal = S.StillFor >= FMath::Max(0.f, WRTune(TEXT("passives.rootedDelay"), 1.5f) - UWRInventoryComponent::GearStatFor(Who, TEXT("rootedSooner"))) && Attr && Attr->GetHealth() < Attr->GetMaxHealth();
	if (bHeal)
	{
		const float Mult = S.Ground == EGround::Barren ? WRTune(TEXT("passives.rootedBarrenMult"), 0.5f) : 1.f;
		Heal(Who, Who, Attr->GetMaxHealth() * WRTune(TEXT("passives.rootedHealPct"), 0.01f) * Mult * DeltaTime, false, false);
		// Thin root-hairs grow from the soles into the ground (kept up a second at a time), a soft creak now and then.
		if (Time - S.LastHairs >= 0.9)
		{
			S.LastHairs = Time;
			const bool bCreak = Time - S.LastCreak >= WRTune(TEXT("passives.rootedCreakEvery"), 4.f);
			if (bCreak) S.LastCreak = Time;
			WRFx::Broadcast(Who, TEXT("nature.rooted"), FeetOf(Who), Who->GetActorRotation(), 1.f, bCreak ? FName(TEXT("nature.rooted")) : NAME_None, 1.05f);
		}
	}
	if (bHeal != S.bHealing)
	{
		S.bHealing = bHeal;
		SetTag(Who, Status_RootedHeal, bHeal);   // the bark patches tint green on every machine while it heals (2.18)
	}
}

// ---------------------------------------------------------------- healing

AWRCharacterBase* WRNature::Caster(AActor* By)
{
	if (AWRPlantSummon* Plant = Cast<AWRPlantSummon>(By))
	{
		return Plant->OwnerPawn.Get();
	}
	return Cast<AWRCharacterBase>(By);
}

bool WRNature::IsAlly(const AWRCharacterBase* Healer, const AWRCharacterBase* Who)
{
	if (!Healer || !Who || !Who->IsAlive())
	{
		return false;
	}
	if (Who == Healer)
	{
		return true;
	}
	// Survivors you can't fight, your clan's tames and plants; never a wild creature.
	if (Who->IsCreature() && Who->GetTameOwner().IsEmpty())
	{
		return false;
	}
	return !Healer->IsHostileTo(Who) && !Who->IsHostileTo(Healer);
}

float WRNature::Heal(AWRCharacterBase* Healer, AWRCharacterBase* Target, float Amount, bool bPurify, bool bFirstTick)
{
	UAbilitySystemComponent* ASC = Target && Target->HasAuthority() ? Target->GetAbilitySystemComponent() : nullptr;
	UWRAttributeSet* Attr = Target ? Target->GetAttributes() : nullptr;
	if (!ASC || !Attr || !Target->IsAlive() || Amount <= 0.f)
	{
		return 0.f;
	}
	const FWRTuning& T = FWRTuning::Get();
	// Lifebloom: +8% to all your Nature healing a rank; rot and decay halve what the body takes.
	Amount *= 1.f + UWRPathComponent::Stat(Healer, TEXT("natureHealing"));
	Amount *= Target->Status ? Target->Status->HealingTakenScale() : 1.f;
	const float Healed = FMath::Clamp(Amount, 0.f, FMath::Max(0.f, Attr->GetMaxHealth() - Attr->GetHealth()));
	if (Healed > 0.f)
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Attr->GetHealth() + Healed);
	}
	// Purify (Grove node): each heal tick from Regrowth, Sanctuary or a World tree draws poison and rot out; the first
	// tick of each ends an active poison or rot. Grey-green motes lift out of the body, a clean chime.
	if (bPurify && Healer && Target->Status && UWRPathComponent::HasNode(Healer, WRNatureImpl::PathId, TEXT("purify")))
	{
		const int32 Poison = WRStatus::Poison, Rot = WRStatus::Rot;
		const bool bHad = Target->HasTag(WRTags::Status_Poison) || Target->HasTag(WRTags::Status_Rot)
			|| (Target->BuildupPct.IsValidIndex(Poison) && Target->BuildupPct[Poison] > 0) || (Target->BuildupPct.IsValidIndex(Rot) && Target->BuildupPct[Rot] > 0);
		Target->Status->ReduceBuildup(Poison, T.Num(TEXT("passives.purifyPoison"), 20.f));
		Target->Status->ReduceBuildup(Rot, T.Num(TEXT("passives.purifyRot"), 20.f));
		if (bFirstTick)
		{
			Target->Status->ClearStatus(WRTags::Status_Poison);
			Target->Status->ClearStatus(WRTags::Status_Rot);
		}
		if (bHad)
		{
			WRFx::BroadcastOn(Target, TEXT("nature.purify"), NAME_None, 1.f, TEXT("nature.purify"));
		}
	}
	// Red sap (Nature + Blood 25): healing others heals you for 30% of it.
	if (Healer && Healer != Target && Healed > 0.f && Healer->HasTag(Combo_RedSap) && Healer->IsAlive())
	{
		UWRAttributeSet* HA = Healer->GetAttributes();
		UAbilitySystemComponent* HASC = Healer->GetAbilitySystemComponent();
		if (HA && HASC)
		{
			const float Back = Healed * FWRComboRegistry::Get().Num(TEXT("red_sap"), TEXT("share"), 0.3f) * (Healer->Status ? Healer->Status->HealingTakenScale() : 1.f);
			HASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(HA->GetMaxHealth(), HA->GetHealth() + Back));
		}
	}
	return Healed;
}

void WRNature::AddHealZone(AActor* Zone, AWRCharacterBase* Owner, float Radius, bool bShield)
{
	if (!Zone || !Owner)
	{
		return;
	}
	WRNatureImpl::FZone& Z = WRNatureImpl::Zones().AddDefaulted_GetRef();
	Z.Actor = Zone;
	Z.Owner = Owner;
	Z.Radius = Radius;
	Z.bShield = bShield;
}

// ---------------------------------------------------------------- passives the combat code asks

float WRNature::DamageTakenScale(const AWRCharacterBase* Target, const FWRHit& Hit)
{
	// World tree: allies under its crown take 20% less (combat.md "shields 20%").
	for (const WRNatureImpl::FZone& Z : WRNatureImpl::Zones())
	{
		const AActor* A = Z.Actor.Get();
		const AWRCharacterBase* Owner = Z.Owner.Get();
		if (Z.bShield && A && Owner && FVector::Dist2D(A->GetActorLocation(), Target->GetActorLocation()) < Z.Radius && IsAlly(Owner, Target))
		{
			const AWRPlantSummon* Plant = Cast<AWRPlantSummon>(A);
			if (!Plant || Plant->IsStanding())
			{
				return FMath::Max(0.f, 1.f + WRTune(TEXT("spells.worldTree.damageTaken"), -0.2f));
			}
		}
	}
	return 1.f;
}

namespace WRNatureImpl
{
	/** Inside a living Sanctuary or World tree zone of an ally who owns Frozen grove. */
	static bool InFrozenGrove(const AWRCharacterBase* Who)
	{
		for (const FZone& Z : Zones())
		{
			const AActor* A = Z.Actor.Get();
			const AWRCharacterBase* Owner = Z.Owner.Get();
			if (A && Owner && Owner->HasTag(WRNature::Combo_FrozenGrove) && FVector::Dist2D(A->GetActorLocation(), Who->GetActorLocation()) < Z.Radius && WRNature::IsAlly(Owner, Who))
			{
				const AWRPlantSummon* Plant = Cast<AWRPlantSummon>(A);
				if (!Plant || Plant->IsStanding()) return true;
			}
		}
		return false;
	}
}

float WRNature::BonusPoise(const AWRCharacterBase* Target)
{
	return Target && WRNatureImpl::InFrozenGrove(Target) ? FWRComboRegistry::Get().Num(TEXT("frozen_grove"), TEXT("poise"), 15.f) : 0.f;
}

float WRNature::BuildupTakenScale(const AWRCharacterBase* Who, int32 Id)
{
	if (Id == WRStatus::Frost && Who && WRNatureImpl::InFrozenGrove(Who))
	{
		return FWRComboRegistry::Get().Num(TEXT("frozen_grove"), TEXT("frostbiteTaken"), 0.5f);
	}
	return 1.f;
}

float WRNature::MoveSpeedScale(const AWRCharacterBase* Who)
{
	if (!Who || Who->IsA<AWRPlantSummon>())
	{
		return 1.f;
	}
	float Scale = 1.f;
	// An Entangle's boss fallback, Strangling vines on a major boss (the server moves those bodies).
	if (Who->HasAuthority())
	{
		if (const WRNatureImpl::FSlow* Slow = WRNatureImpl::Slows().Find(Who))
		{
			if (Slow->Until > WRNatureImpl::Now(Who)) Scale *= Slow->Mult;
		}
	}
	// Pollen (Bloom node): within 400 cm of an enemy's plant that has it, x0.8 (bosses x0.9).
	const float Radius = WRTune(TEXT("passives.pollenRadius"), 400.f);
	for (TActorIterator<AWRPlantSummon> It(Who->GetWorld()); It; ++It)
	{
		if (It->bPollen && It->IsStanding() && FVector::DistSquared2D(It->GetActorLocation(), Who->GetActorLocation()) < FMath::Square(Radius) && It->IsHostileTo(Who))
		{
			Scale *= WRNatureImpl::IsBoss(Who) ? WRTune(TEXT("passives.pollenBossSlow"), 0.9f) : WRTune(TEXT("passives.pollenSlow"), 0.8f);
			break;
		}
	}
	return Scale;
}

float WRNature::RootRadiusScale(const AActor* Who)
{
	return 1.f + UWRPathComponent::Stat(Caster(const_cast<AActor*>(Who)), TEXT("rootRadius"));
}

void WRNature::SlowBody(AWRCharacterBase* Who, float Mult, float Seconds)
{
	if (!Who || !Who->HasAuthority() || Seconds <= 0.f)
	{
		return;
	}
	WRNatureImpl::FSlow& S = WRNatureImpl::Slows().FindOrAdd(Who);
	const double Until = WRNatureImpl::Now(Who) + Seconds;
	// The stronger slow wins while both run.
	if (S.Until < WRNatureImpl::Now(Who) || Mult < S.Mult)
	{
		S.Mult = Mult;
	}
	S.Until = FMath::Max(S.Until, Until);
}

// ---------------------------------------------------------------- Entangled (the WRForest contract)

bool WRNature::EntangleEx(AActor* TargetActor, float Seconds, AActor* Source, bool bIgnoreBossRule, float BossSlow, float BossSlowSeconds)
{
	using namespace WRNatureImpl;
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(TargetActor);
	if (!Target || !Target->HasAuthority() || !Target->IsAlive() || !Target->Status || Target->IsA<AWRPlantSummon>())
	{
		return false;
	}
	const FWRTuning& T = FWRTuning::Get();
	const double Time = Now(Target);
	AWRCharacterBase* By = Caster(Source);
	// Mini and major bosses are never held: a slow instead (x0.8 for 1 s), unless the caller says this one is (the
	// Hornwarden's snares, cooks/biome-forest.md 2.13.4; Strangling vines on a mini).
	if (IsBoss(Target) && !bIgnoreBossRule)
	{
		SlowBody(Target, BossSlow >= 0.f ? BossSlow : T.Num(TEXT("status.entangled.bossSlow"), 0.8f),
			BossSlowSeconds >= 0.f ? BossSlowSeconds : T.Num(TEXT("status.entangled.bossSlowSeconds"), 1.f));
		WRFx::Broadcast(Target, TEXT("nature.entangle_snap"), FeetOf(Target), Target->GetActorRotation(), FMath::Max(1.f, Target->GetSimpleCollisionRadius() / 60.f), TEXT("nature.entangle"));
		return false;
	}
	FHold& H = Holds().FindOrAdd(Target);
	if (Time < H.ImmuneUntil || Time < H.Until)
	{
		return false;   // held already, or still immune from the last one (no chain-locks)
	}
	// The tier's time when the caller has none (lowland 1.5, midland 1.0, highland 0.6); a player is held 0.6 s at most
	// (the PvP block), whatever held them.
	if (Target->GetPlayerState())
	{
		const float Cap = T.Num(TEXT("status.entangled.player"), 0.6f);
		Seconds = Seconds < 0.f ? Cap : FMath::Min(Seconds, Cap);
	}
	else if (Seconds < 0.f)
	{
		const AWRCreature* Creature = Cast<AWRCreature>(Target);
		const FWRCreatureDef* Def = Creature ? Creature->GetDef() : nullptr;
		const FString Tier = Def && !Def->Tier.IsNone() ? Def->Tier.ToString() : FString(TEXT("lowland"));
		Seconds = T.Num(TEXT("status.entangled.") + (IsBoss(Target) ? FString(TEXT("highland")) : Tier), T.Num(TEXT("status.entangled.lowland"), 1.5f));
	}
	if (Seconds <= 0.f)
	{
		return false;
	}
	// Roots hold the feet (Status.Rooted: no move, no roll, no jump; turning, attacks and casts still work).
	Target->Status->ApplyStatus(WRTags::Status_Rooted, Seconds);
	if (UCharacterMovementComponent* Move = Target->GetCharacterMovement(); Move && !Move->IsFalling())
	{
		Move->StopMovementImmediately();
	}
	H.Until = Time + Seconds;
	H.ImmuneUntil = H.Until + T.Num(TEXT("status.entangled.immunity"), 8.f);
	// Roots burst from the ground and wrap the legs (kept up for the hold), a wood snap and a rustle.
	WRFx::Broadcast(Target, TEXT("nature.entangle"), FeetOf(Target), Target->GetActorRotation(), FMath::Max(0.7f, Target->GetSimpleCollisionRadius() / 45.f), TEXT("nature.entangle"), Seconds);
	// Smoulder roots (Fire + Nature 25): the roots wrap smouldering: 40 burn buildup at once.
	if (By && By->HasTag(Combo_SmoulderRoots))
	{
		Target->Status->AddBuildup(WRTags::Element_Fire, FWRComboRegistry::Get().Num(TEXT("smoulder_roots"), TEXT("burn"), 40.f), By);
	}
	UE_LOG(LogWildreach, Log, TEXT("NATURE %s entangled for %.1f s by %s"), *Target->GetName(), Seconds, *GetNameSafe(Source));
	return true;
}

bool WRForest::Entangle(AActor* Target, float Seconds, AActor* Source, bool bIgnoreBossRule)
{
	return WRNature::EntangleEx(Target, Seconds, Source, bIgnoreBossRule);
}

bool WRForest::IsEntangled(const AActor* Target)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Target);
	if (!C || !C->HasTag(WRTags::Status_Rooted))
	{
		return false;
	}
	// Server: a Nature hold (not a bola's tangle); a client only has the tag.
	if (!C->HasAuthority())
	{
		return true;
	}
	const WRNatureImpl::FHold* H = WRNatureImpl::Holds().Find(Target);
	return H && H->Until > WRNatureImpl::Now(Target);
}

void WRForest::GrantGrowth(AActor* Who, float Amount)
{
	AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (C && C->HasAuthority() && WRNature::Walks(C))
	{
		WRNatureImpl::AddGrowth(C, Amount);
	}
}

int32 WRForest::NaturePoints(const AActor* Who)
{
	const UWRPathComponent* Paths = UWRPathComponent::FindFor(Who);
	return Paths && Paths->IsActivePath(WRNatureImpl::PathId) ? Paths->GetSpent(WRNatureImpl::PathId) : 0;
}

// ---------------------------------------------------------------- shared hits and procs

void WRNature::NatureHit(AWRCharacterBase* By, AWRCharacterBase* Target, float Damage, float Bleed, const FVector& Origin, bool bSpell, bool bReaction,
	float StanceDamage, float Poise, uint8 PhysType)
{
	if (!Target || !Target->IsAlive() || !Target->HasAuthority())
	{
		return;
	}
	FWRHit Hit;
	Hit.Instigator = By;
	Hit.Damage = Damage;
	Hit.PoiseDamage = Poise;
	Hit.Element = Element_Nature;
	if (Bleed > 0.f) Hit.Buildups.Add(TEXT("bleed"), Bleed);
	Hit.PathId = WRNatureImpl::PathId;
	Hit.bSpell = bSpell;
	Hit.bReaction = bReaction;
	Hit.StanceDamage = StanceDamage;
	Hit.PhysType = PhysType;
	Hit.Impact = 1;
	Hit.Origin = Origin;
	Hit.ImpactPoint = Target->GetActorLocation();
	UWRCombatLibrary::ApplyHit(Target, Hit);
}

void WRNature::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Caster(Hit.Instigator);
	if (!Target || !Attacker || Attacker == Target || Hit.bDamageOverTime || Hit.bReaction || !Target->HasAuthority() || !Target->IsAlive())
	{
		return;
	}
	const bool bNature = Hit.PathId == WRNatureImpl::PathId;
	// Briar strike: roots hold a lowland creature 0.6 s (wildbrands.briar_strike.entangleLowland).
	if (Hit.bWeaponArt && Hit.bMelee && Attacker->HasTag(Ability_BriarStrike))
	{
		const AWRCreature* Creature = Cast<AWRCreature>(Target);
		const FWRCreatureDef* Def = Creature ? Creature->GetDef() : nullptr;
		if (Def && Def->Tier == TEXT("lowland") && !WRNatureImpl::IsBoss(Target))
		{
			EntangleEx(Target, WRTune(TEXT("wildbrands.briar_strike.entangleLowland"), 0.6f), Attacker, false);
			if (!Target->IsAlive()) return;
		}
	}
	// Entangle chance (trunk): each weapon hit and each Nature spell hit may root the target (5% a rank).
	if ((Hit.bMelee && Hit.Instigator == Attacker) || (bNature && Hit.bSpell))
	{
		const float Chance = UWRPathComponent::Stat(Attacker, TEXT("entangleChance"));
		if (Chance > 0.f && FMath::FRand() < Chance)
		{
			EntangleEx(Target, -1.f, Attacker, false);
		}
	}
}

void WRNature::AfterDamage(AWRCharacterBase* Target, const FWRHit& Hit, float Damage, bool bLanded)
{
	if (!Target || !Target->HasAuthority())
	{
		return;
	}
	// Any damage starts Rooted's 1.5 s wait again.
	if (Damage > 0.f)
	{
		if (WRNatureImpl::FOwnerState* S = WRNatureImpl::Owners().Find(Target))
		{
			S->StillFor = 0.f;
		}
	}
	// Thorned skin (Thorn node): every melee blow that lands on you returns 8 damage + bleed 10 to the attacker on the
	// same contact frame (x0.6 between players). Its own blow is a reaction, so it never bounces back again.
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!bLanded || !Hit.bMelee || Hit.bReaction || Hit.bDamageOverTime || !Attacker || Attacker == Target || !Attacker->IsAlive()
		|| !Target->IsAlive() || !UWRPathComponent::HasNode(Target, WRNatureImpl::PathId, TEXT("thorned_skin")))
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const bool bPvP = Attacker->GetPlayerState() || !Attacker->GetTameOwner().IsEmpty();
	const float Pvp = bPvP ? T.Num(TEXT("passives.thornedSkinPvp"), 0.6f) : 1.f;
	const FVector At = Hit.ImpactPoint.IsNearlyZero() ? Target->GetActorLocation() : Hit.ImpactPoint;
	// Thorns flick out of your armour at the hit point; a scratching hiss.
	WRFx::Broadcast(Target, TEXT("nature.thorned_skin"), At, (Attacker->GetActorLocation() - At).Rotation(), 1.f, TEXT("nature.thorned_skin"));
	FWRHit Thorns;
	Thorns.Instigator = Target;
	Thorns.Damage = T.Num(TEXT("passives.thornedSkinDamage"), 8.f) * Pvp;
	Thorns.Element = Element_Nature;
	Thorns.Buildups.Add(TEXT("bleed"), T.Num(TEXT("passives.thornedSkinBleed"), 10.f) * Pvp);
	Thorns.PathId = WRNatureImpl::PathId;
	Thorns.bDamageOverTime = true;   // no guard, parry or hit-stop: the thorns simply bite
	Thorns.bReaction = true;         // and never bounce again
	Thorns.Origin = At;
	Thorns.ImpactPoint = Attacker->GetActorLocation();
	UWRCombatLibrary::ApplyHit(Attacker, Thorns);
}

void WRNature::NoteBuildups(AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Target || Hit.PathId != WRNatureImpl::PathId || Hit.Buildups.FindRef(TEXT("bleed")) <= 0.f)
	{
		return;
	}
	WRNatureImpl::FBleedNote& N = WRNatureImpl::BleedNotes().FindOrAdd(Target);
	N.By = Caster(Hit.Instigator);
	N.At = WRNatureImpl::Now(Target);
}

void WRNature::OnBleedBurst(AWRCharacterBase* Owner, AWRCharacterBase* Source, float Burst)
{
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	const WRNatureImpl::FBleedNote* N = WRNatureImpl::BleedNotes().Find(Owner);
	AWRCharacterBase* By = Caster(Source);
	// Only a bleed that a Nature blow set off this very moment counts (the note is from the same blow).
	if (!N || !By || N->By.Get() != By || FMath::Abs(N->At - WRNatureImpl::Now(Owner)) > 0.001)
	{
		return;
	}
	// Barbs: +10% bleed burst a rank on bleeds your Nature hits cause.
	const float Extra = Burst * UWRPathComponent::Stat(By, TEXT("natureBleedBurst"));
	if (Extra > 0.f && Owner->IsAlive())
	{
		FWRHit Barb;
		Barb.Instigator = By;
		Barb.Damage = Extra;
		Barb.Element = WRTags::Element_Physical;
		Barb.bDamageOverTime = true;
		Barb.bBurst = true;
		Barb.bReaction = true;
		Barb.Origin = Owner->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Owner, Barb);
	}
	// Thorn feast (Nature + Blood 50): each bleed proc from your Nature hits heals your plants 20% of their health.
	if (By->HasTag(Combo_ThornFeast))
	{
		for (AWRPlantSummon* Plant : AWRPlantSummon::PlantsOf(By))
		{
			Plant->HealShare(FWRComboRegistry::Get().Num(TEXT("thorn_feast"), TEXT("heal"), 0.2f));
		}
	}
}

void WRNature::OnShockProc(AWRCharacterBase* Owner, AWRCharacterBase* Source)
{
	// Quickening (Lightning + Nature 25): a shock proc you set off within 10 m of your plants makes them fire 30% faster
	// for 5 s.
	AWRCharacterBase* By = Caster(Source);
	if (!Owner || !By || !By->HasTag(Combo_Quickening))
	{
		return;
	}
	const FWRComboRegistry& C = FWRComboRegistry::Get();
	const float Radius = C.Num(TEXT("quickening"), TEXT("radius"), 1000.f);
	for (AWRPlantSummon* Plant : AWRPlantSummon::PlantsOf(By))
	{
		if (FVector::Dist(Plant->GetActorLocation(), Owner->GetActorLocation()) < Radius)
		{
			Plant->Quicken(Plant->ServerNow() + C.Num(TEXT("quickening"), TEXT("seconds"), 5.f));
		}
	}
}

// ---------------------------------------------------------------- the body

namespace WRNatureLook
{
	/** Path strength (hero.md 2.6: smoothstep(0.08, 1, points / 100)) at a number of points. */
	static float StrengthAt(float Points)
	{
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		return FMath::SmoothStep(0.08f, 1.f, FMath::Clamp(Points / Max, 0.f, 1.f));
	}

	/** 0..1: how far into the 2.18 stage that starts at From (share of 100 points) the strength S is (5 points to come in). */
	static float Stage(float S, float From)
	{
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		const float Hi = StrengthAt(From * Max), Lo = StrengthAt(From * Max - 5.f);
		return Hi <= Lo ? (S >= Hi ? 1.f : 0.f) : FMath::Clamp((S - Lo) / (Hi - Lo), 0.f, 1.f);
	}

	/** The nature row of Data/player_art.json (paths.nature). */
	static TSharedPtr<FJsonObject> Row()
	{
		const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
		const TSharedPtr<FJsonObject>* Paths = nullptr;
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Art.IsValid() && Art->TryGetObjectField(TEXT("paths"), Paths) && (*Paths)->TryGetObjectField(TEXT("nature"), Out) ? *Out : TSharedPtr<FJsonObject>();
	}

	static float Num(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Fallback)
	{
		double V = Fallback;
		if (Obj.IsValid()) Obj->TryGetNumberField(Field, V);
		return float(V);
	}

	static FLinearColor Vec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FLinearColor& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Arr) || Arr->Num() < 3) return Fallback;
		return FLinearColor(float((*Arr)[0]->AsNumber()), float((*Arr)[1]->AsNumber()), float((*Arr)[2]->AsNumber()), Arr->Num() > 3 ? float((*Arr)[3]->AsNumber()) : 1.f);
	}

	static TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& In, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return In.IsValid() && In->TryGetObjectField(Field, Out) ? *Out : TSharedPtr<FJsonObject>();
	}
}

uint32 WRNature::LookSig(const AActor* Hero)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Hero);
	return C && C->HasTag(Status_RootedHeal) ? 0x2007u : 0u;
}

void WRNature::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Eyes,
	const TMap<FName, FLinearColor>& EyeDefaultColors, FName Main, float MainS, FName Sub, float SubS)
{
	const FName Nature = WRNatureImpl::PathId;
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Hero);
	if (!C || (Main != Nature && Sub != Nature))
	{
		return;
	}
	const TSharedPtr<FJsonObject> R = WRNatureLook::Row();
	const TSharedPtr<FJsonObject> SkinRow = WRNatureLook::Obj(R, TEXT("skin"));
	const TSharedPtr<FJsonObject> Look = WRNatureLook::Obj(R, TEXT("look"));
	const FWRTuning& T = FWRTuning::Get();
	// The bark: fine young-beech bark, silver-grey (the row's tint); the G mask reveals it by strength: knuckles at 0.3,
	// elbows and knees at 0.6, shoulders and collarbones at 1.0 (the mask's grading is the stage's reveal).
	const FLinearColor Bark = WRNatureLook::Vec(SkinRow, TEXT("AlbedoTint"), FLinearColor(0.33f, 0.31f, 0.27f));
	// Rooted's heal (main path only): the bark patches shift green for as long as it heals (an albedo shift, no glow).
	const bool bRooted = Main == Nature && C->HasTag(Status_RootedHeal);
	const FLinearColor Green(T.Num(TEXT("passives.rootedTint.r"), 0.16f), T.Num(TEXT("passives.rootedTint.g"), 0.3f), T.Num(TEXT("passives.rootedTint.b"), 0.09f));
	const FName OtherPath = Main == Nature ? Sub : Main;
	for (const TPair<FString, float> Side : { TPair<FString, float>(TEXT("Main"), Main == Nature ? MainS : 0.f), TPair<FString, float>(TEXT("Sub"), Sub == Nature ? SubS : 0.f) })
	{
		const float S = Side.Value;
		if (S <= 0.f)
		{
			continue;
		}
		const FString& Prefix = Side.Key;
		const FString OtherPrefix = Prefix == TEXT("Main") ? FString(TEXT("Sub")) : FString(TEXT("Main"));
		// At 0.6 the skin takes a warm, sun-kissed tone (+5% warmth: a little more colour and light). The material scales
		// these by Strength, so they're divided back by it to land at the stage's number.
		const float Warm = WRNatureLook::Stage(S, WRNatureLook::Num(Look, TEXT("warmFrom"), 0.6f));
		const float Desat = (WRNatureLook::Num(SkinRow, TEXT("SkinDesat"), 0.f) - WRNatureLook::Num(Look, TEXT("warmth"), 0.05f) * Warm) / FMath::Max(S, 0.05f);
		const float Light = (WRNatureLook::Num(SkinRow, TEXT("SkinLight"), 0.f) + WRNatureLook::Num(Look, TEXT("warmLight"), 0.02f) * Warm) / FMath::Max(S, 0.05f);
		const FLinearColor Want = bRooted && Prefix == TEXT("Main") ? FMath::Lerp(Bark, Green, T.Num(TEXT("passives.rootedTint.share"), 0.6f)) : Bark;
		// Nature with another path (2.18): with Fire, ember veins run in the bark's cracks (fire's lines read the bark mask
		// too); with Frost, rime forms along the bark's edges; with Blood, garnet threads under the bark.
		float OtherBark = 0.f;
		if (OtherPath == TEXT("fire")) OtherBark = WRNatureLook::Num(Look, TEXT("mixFireBark"), 0.6f);
		else if (OtherPath == TEXT("frost")) OtherBark = WRNatureLook::Num(Look, TEXT("mixFrostBark"), 0.5f);
		else if (OtherPath == TEXT("blood")) OtherBark = WRNatureLook::Num(Look, TEXT("mixBloodBark"), 0.35f);
		for (UMaterialInstanceDynamic* MID : Skin)
		{
			if (!MID) continue;
			MID->SetVectorParameterValue(FName(*(Prefix + TEXT("AlbedoTint"))), Want);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("SkinDesat"))), Desat);
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("SkinLight"))), Light);
			if (OtherBark > 0.f)
			{
				FLinearColor Ch(0.f, 0.f, 0.f, 0.f);
				MID->GetVectorParameterValue(FMaterialParameterInfo(FName(*(OtherPrefix + TEXT("Channels")))), Ch);
				Ch.G = FMath::Max(Ch.G, OtherBark * FMath::Min(1.f, S * 2.f));
				MID->SetVectorParameterValue(FName(*(OtherPrefix + TEXT("Channels"))), Ch);
			}
		}
	}
	// Eyes (main path only): a leaf-green rim at 0.3, half the iris at 0.6, all of it at 1.0.
	const float MS = Main == Nature ? MainS : 0.f;
	const float Rim = WRNatureLook::Stage(MS, WRNatureLook::Num(Look, TEXT("irisRimFrom"), 0.3f));
	const float Iris = 0.5f * WRNatureLook::Stage(MS, WRNatureLook::Num(Look, TEXT("irisHalfFrom"), 0.6f)) + 0.5f * WRNatureLook::Stage(MS, WRNatureLook::Num(Look, TEXT("irisFullFrom"), 1.f));
	const FLinearColor RimColor = WRNatureLook::Vec(Look, TEXT("rim"), FLinearColor(0.5f, 1.1f, 0.3f));
	const FLinearColor IrisColor = WRNatureLook::Vec(Look, TEXT("iris"), FLinearColor(0.7f, 1.4f, 0.45f));
	const FLinearColor* RimOwn = EyeDefaultColors.Find(TEXT("eye:Melanosis Ring Color"));
	const FLinearColor* IrisOwn = EyeDefaultColors.Find(TEXT("eye:Iris Color Multiply"));
	if (Main == Nature)
	{
		for (UMaterialInstanceDynamic* MID : Eyes)
		{
			if (!MID) continue;
			if (RimOwn) MID->SetVectorParameterValue(TEXT("Melanosis Ring Color"), FMath::Lerp(*RimOwn, RimColor, Rim));
			if (IrisOwn) MID->SetVectorParameterValue(TEXT("Iris Color Multiply"), FMath::Lerp(*IrisOwn, IrisColor, Iris));
		}
	}
}

namespace WRNatureLook
{
	/** One temple bud, modelled along +X from its base (cm at scale 1): a closed young-leaf bud on a silver-bark collar,
	 *  two leaflets just parting from it (hero.md / cooks/biome-forest.md 2.18: "small leaf-bud decals at the temples"). */
	static const FWRToolMesh& BudMesh()
	{
		static FWRToolMesh M;
		if (M.Verts.Num() > 0)
		{
			return M;
		}
		const FLinearColor Bark(0.33f, 0.31f, 0.27f), Bud(0.16f, 0.30f, 0.06f), Leaf(0.26f, 0.44f, 0.09f);
		// The collar where the bud breaks the skin: a low ring of the same silver bark as the patches.
		M.Lathe(FVector::ZeroVector, FVector::ForwardVector, 0.3f, { FVector(0.f, 0.42f, 0.38f), FVector(0.6f, 0.4f, 0.36f), FVector(1.f, 0.3f, 0.27f) }, 10, Bark, 0.7f);
		// The bud: a slim teardrop closing to a point.
		M.Lathe(FVector(0.2f, 0.f, 0.f), FVector::ForwardVector, 1.3f,
			{ FVector(0.f, 0.28f, 0.25f), FVector(0.18f, 0.4f, 0.35f), FVector(0.45f, 0.44f, 0.4f), FVector(0.75f, 0.3f, 0.27f), FVector(0.94f, 0.08f, 0.07f), FVector(1.f, 0.f, 0.f) },
			10, Bud, 0.5f);
		// Two leaflets parting from the bud's waist, each a thin blade (a flattened lathe) leaning out about 35 degrees.
		for (const float Side : { -1.f, 1.f })
		{
			const FVector Axis = (FVector::ForwardVector + FVector(0.f, 0.f, Side * 0.7f)).GetSafeNormal();
			M.Lathe(FVector(0.45f, 0.f, Side * 0.12f), Axis, 1.05f,
				{ FVector(0.f, 0.05f, 0.03f), FVector(0.3f, 0.36f, 0.06f), FVector(0.62f, 0.33f, 0.05f), FVector(0.9f, 0.1f, 0.03f), FVector(1.f, 0.f, 0.f) },
				8, Leaf, 0.45f);
		}
		return M;
	}
}

void WRNature::ApplyTempleBuds(AActor* Hero, USkeletalMeshComponent* Body, bool bShow, FName Main, float MainS)
{
	if (!Hero || !Body)
	{
		return;
	}
	static const FName BudTag(TEXT("NatureTempleBud"));
	TArray<UProceduralMeshComponent*> All;
	Hero->GetComponents<UProceduralMeshComponent>(All);
	TArray<UProceduralMeshComponent*> Buds;
	for (UProceduralMeshComponent* C : All)
	{
		if (C && C->ComponentHasTag(BudTag)) Buds.Add(C);
	}
	const TSharedPtr<FJsonObject> Tune = WRNatureLook::Obj(WRNatureLook::Obj(WRNatureLook::Row(), TEXT("look")), TEXT("templeBud"));
	FString BoneName(TEXT("head"));
	if (Tune.IsValid()) Tune->TryGetStringField(TEXT("bone"), BoneName);
	const FName HeadBone(*BoneName);
	// They come in over the last 5 points before 1.0 (main path only), as the iris finishes going green.
	const float Grow = Main == WRNatureImpl::PathId ? WRNatureLook::Stage(MainS, WRNatureLook::Num(Tune, TEXT("from"), 1.f)) : 0.f;
	const USkinnedAsset* Asset = Body->GetSkinnedAsset();
	const int32 Bone = Asset ? Asset->GetRefSkeleton().FindBoneIndex(HeadBone) : INDEX_NONE;
	if (!bShow || Grow <= 0.01f || Bone == INDEX_NONE)
	{
		for (UProceduralMeshComponent* C : Buds) C->SetVisibility(false);
		return;
	}
	while (Buds.Num() < 2)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(Hero);
		C->ComponentTags.Add(BudTag);
		C->SetupAttachment(Body, HeadBone);
		C->RegisterComponent();
		WRNatureLook::BudMesh().ApplyTo(C);
		C->SetCastShadow(false);   // a centimetre of leaf: a shadow would only flicker
		Buds.Add(C);
	}
	// Placed from the head bone's reference pose, so the offsets are in the body's own front / side / up (cm) whatever
	// way the rig's bone axes point: up and forward of the bone, at each temple, leaning up and back.
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	FTransform HeadCS = FTransform::Identity;
	for (int32 b = Bone; b != INDEX_NONE; b = Ref.GetParentIndex(b))
	{
		HeadCS = HeadCS * Ref.GetRefBonePose()[b];
	}
	const FTransform CompInActor = Body->GetComponentTransform().GetRelativeTransform(Hero->GetActorTransform());
	const float Fwd = WRNatureLook::Num(Tune, TEXT("forward"), 7.5f), SideOff = WRNatureLook::Num(Tune, TEXT("side"), 6.6f), Up = WRNatureLook::Num(Tune, TEXT("up"), 9.5f);
	const float Size = WRNatureLook::Num(Tune, TEXT("size"), 1.f) * Grow / FMath::Max(0.01f, float(Body->GetComponentScale().X));
	for (int32 i = 0; i < 2; ++i)
	{
		const float S = i == 0 ? 1.f : -1.f;
		const FVector OffsetActor(Fwd, S * SideOff, Up);
		const FVector AxisActor = FVector(-0.35f, S * 0.45f, 0.82f).GetSafeNormal();
		const FVector PosCS = HeadCS.GetLocation() + CompInActor.InverseTransformVector(OffsetActor);
		const FVector AxisCS = CompInActor.InverseTransformVectorNoScale(AxisActor);
		const FQuat RotCS = FRotationMatrix::MakeFromXZ(AxisCS, CompInActor.InverseTransformVectorNoScale(FVector(1.f, 0.f, 0.f))).ToQuat();
		UProceduralMeshComponent* C = Buds[i];
		C->SetRelativeLocationAndRotation(HeadCS.InverseTransformPosition(PosCS), HeadCS.InverseTransformRotation(RotCS));
		C->SetRelativeScale3D(FVector(Size));
		C->SetVisibility(true);
	}
}
