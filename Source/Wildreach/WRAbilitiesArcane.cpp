// The Arcane path's abilities (see WRAbilitiesArcane.h; cooks/biome-crystal.md 2.17, build-biome-crystal.md P2.9).
//
// Every blow goes through UWRCombatLibrary::ApplyHit on the server. Bolts fly straight (they curve after a locked
// creature by the shared lockHoming rule, never at a survivor); stars and runes are areas you can see before they bite:
// a star's landing point glints 1.2 s before it lands, a rune is a hexafoil on the ground anyone can see.

#include "WRAbilitiesArcane.h"
#include "WRRaiding.h"
#include "Wildreach.h"
#include "WRArcanePath.h"
#include "WRAttributeSet.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRCrystalTypes.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

namespace WRArcaneAim
{
	static const FLinearColor AaViolet = FLinearColor(FColor(0x7A, 0x3F, 0xD1));
	static const FLinearColor AaLilac = FLinearColor(FColor(0xD8, 0xD0, 0xFF));

	static AWRCharacterBase* AaLocked(const AWRCharacterBase* Caster)
	{
		const AWRPlayerController* PC = Caster ? Cast<AWRPlayerController>(Caster->GetController()) : nullptr;
		AWRCharacterBase* Target = PC ? PC->GetServerLock() : nullptr;
		return Target && Target->IsAlive() && !Target->IsHidden() && Caster->IsHostileTo(Target)
			&& FVector::Dist(Target->GetActorLocation(), Caster->GetActorLocation()) <= WRTune(TEXT("spells.lockRange"), 4000.f) ? Target : nullptr;
	}
	static FVector AaThroughCrosshair(const AWRCharacterBase* Caster, const FVector& From)
	{
		const FVector View = Caster->GetBaseAimRotation().Vector();
		const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
		FVector Point;
		if (!Survivor || !Survivor->GetController() || !Survivor->CrosshairPoint(20000.f, true, Point)) return View;
		const FVector Dir = (Point - From).GetSafeNormal();
		return Dir.IsNearlyZero() || FVector::DistSquared(Point, From) < FMath::Square(100.f) || FVector::DotProduct(Dir, View) < 0.3f ? View : Dir;
	}
	static FVector AaGround(const AActor* Caster, const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRArcaneGround), false, Caster);
		return Caster->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 2000), ECC_WorldStatic, Query) ? FVector(Hit.ImpactPoint) : At;
	}
	static bool AaClearLine(const AActor* From, const FVector& A, const AActor* Target)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRArcaneSight), false, From);
		Query.AddIgnoredActor(Target);
		return !From->GetWorld()->LineTraceSingleByChannel(Hit, A, Target->GetActorLocation(), ECC_WorldStatic, Query);
	}
	static FVector AaFeet(const AActor* Who) { return Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight()); }
	static bool AaCanPay(const FGameplayAbilityActorInfo* ActorInfo, float Cost)
	{
		const AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		if (!Who) return false;
		if (WRArcane::HasMana(Who, Cost)) return true;
		if (Who->IsLocallyControlled()) WRArcane::NoteRefused(Who);
		return false;
	}
	static void AaTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	static float AaServerNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
	}
	/** On the same side as the caster (a survivor of theirs, a tame): who a Barrier, a circle's buff may touch. */
	static bool AaAlly(const AWRCharacterBase* Caster, const AWRCharacterBase* Other)
	{
		return Other && Other->IsAlive() && (Other == Caster || (!Caster->IsHostileTo(Other) && (Other->GetPlayerState() || !Other->GetTameOwner().IsEmpty())));
	}
	static UMaterialInterface* AaWardMaterial()
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_ArcaneWard.M_ArcaneWard"));
		return M ? M : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_VoidLight.M_VoidLight"));
	}
	static UMaterialInstanceDynamic* AaWardMID(UObject* Outer, const FLinearColor& Col, float Glow)
	{
		UMaterialInterface* M = AaWardMaterial();
		if (!M) return nullptr;
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, Outer);
		D->SetVectorParameterValue(TEXT("Color"), Col);
		D->SetScalarParameterValue(TEXT("Glow"), Glow);
		return D;
	}
	static UProceduralMeshComponent* AaMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner, Name);
		P->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
		P->RegisterComponent();
		return P;
	}
	/** A circle of radius R in the ground plane, as a thin tube (Thick cm), lifted Z. */
	static void AaRing(FWRToolMesh& M, const FVector& C, float R, float Thick, const FLinearColor& Col, float Lines)
	{
		TArray<FVector> P;
		const int32 N = FMath::Clamp(int32(R / 12.f), 24, 96);
		for (int32 k = 0; k < N; ++k) { const float A = 2.f * PI * k / N; P.Add(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f)); }
		M.Tube(P, Thick, 4, Col, Lines, true);
	}
	/** The Wardcarvers' hexafoil flat on the ground: six circles on a seventh's rim (one compass setting). */
	static void AaHexafoil(FWRToolMesh& M, const FVector& C, float R, float Thick, const FLinearColor& Col, float Lines)
	{
		AaRing(M, C, R, Thick, Col, Lines);
		for (int32 k = 0; k < 6; ++k) { const float A = k * PI / 3.f; AaRing(M, C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R * 0.5f, R * 0.5f, Thick * 0.8f, Col, Lines); }
	}
	/** The eight-armed stave (Aegishjalmur, crystal 1.2): eight arms from a ring, each ending in a three-pronged fork. */
	static void AaStave(FWRToolMesh& M, const FVector& C, float R, float Thick, const FLinearColor& Col)
	{
		AaRing(M, C, R * 0.16f, Thick, Col, 1.f);
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = k * PI / 4.f;
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f), S(-D.Y, D.X, 0.f);
			const FVector Root = C + D * R * 0.16f, Tip = C + D * R;
			M.Tube({ Root, Tip }, Thick, 4, Col, 1.f);
			M.Tube({ Tip - D * R * 0.12f + S * R * 0.1f, Tip - D * R * 0.12f - S * R * 0.1f }, Thick, 4, Col, 1.f);
			for (const float Sd : { -1.f, 1.f }) M.Tube({ Tip - D * R * 0.12f + S * R * 0.1f * Sd, Tip - D * R * 0.02f + S * R * 0.1f * Sd }, Thick * 0.8f, 4, Col, 1.f);
			M.Tube({ C + D * R * 0.55f + S * R * 0.07f, C + D * R * 0.55f - S * R * 0.07f }, Thick * 0.8f, 4, Col, 1.f);
		}
	}
	/** A faceted dome (a geodesic-like shell of flat facets): R across, its facets' edges painted bright (vertex alpha). */
	static void AaDome(FWRToolMesh& M, float R, const FLinearColor& Col)
	{
		const int32 Rings = 6, Seg = 16;
		const FVector2D Z = FVector2D::ZeroVector;
		auto At = [R](int32 i, int32 j) { const float Th = (PI * 0.5f) * i / 6.f, Ph = 2.f * PI * (j + (i % 2) * 0.5f) / 16.f; return FVector(FMath::Cos(Ph) * FMath::Cos(Th) * R, FMath::Sin(Ph) * FMath::Cos(Th) * R, FMath::Sin(Th) * R); };
		const FColor Face = FWRToolMesh::Pack(Col, 0.15f), Edge = FWRToolMesh::Pack(Col, 1.f);
		for (int32 i = 0; i < Rings; ++i)
		{
			for (int32 j = 0; j < Seg; ++j)
			{
				const FVector A = At(i, j), B = At(i, j + 1), C = At(i + 1, j), D = At(i + 1, j + 1);
				M.Tri(A, C, B, Face, Z, Z, Z);
				if (i + 1 < Rings) M.Tri(B, C, D, Face, Z, Z, Z);
			}
		}
		// The facets' seams: bright lines along the rings and the meridians.
		for (int32 i = 0; i <= Rings; ++i)
		{
			TArray<FVector> P;
			for (int32 j = 0; j < Seg; ++j) P.Add(At(i, j));
			if (i < Rings) M.Tube(P, R * 0.004f + 1.f, 3, Col, 1.f, true);
		}
		for (int32 j = 0; j < Seg; j += 2)
		{
			TArray<FVector> P;
			for (int32 i = 0; i <= Rings; ++i) P.Add(At(i, j));
			M.Tube(P, R * 0.004f + 1.f, 3, Col, 1.f);
		}
		(void)Edge;
	}
}
using namespace WRArcaneAim;

// ================================================================================================ the spell base

UWRGA_ArcaneSpell::UWRGA_ArcaneSpell()
{
	StaminaKey = TEXT("stamina.spell");
	StaminaFallback = 12.f;
	PathId = TEXT("arcane");
	ActivationOwnedTags.AddTag(WRTags::State_Attacking);
	FGameplayTagContainer Blocked = ActivationBlockedTags;
	Blocked.AddTag(WRTags::State_Attacking);
	Blocked.AddTag(WRTags::State_Dodging);
	ActivationBlockedTags = Blocked;
}

float UWRGA_ArcaneSpell::Spell(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

float UWRGA_ArcaneSpell::ManaCost() const
{
	return Spell(TEXT("manaCost"), 0.f);
}

float UWRGA_ArcaneSpell::Glyph(const AWRCharacterBase* Caster) const
{
	return 1.f + WRTune(TEXT("arcane.glyphPerRank"), 0.1f) * WRArcane::Rank(Caster, TEXT("glyph_power"));
}

bool UWRGA_ArcaneSpell::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && AaCanPay(ActorInfo, ManaCost());
}

void UWRGA_ArcaneSpell::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	AWRCharacterBase* Who = ActorInfo ? Cast<AWRCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (Who && HasAuthority(&ActivationInfo)) WRArcane::SpendMana(Who, ManaCost());
}

FWRHit UWRGA_ArcaneSpell::ArcaneHit(AWRCharacterBase* Caster, float Damage, float Mark) const
{
	FWRHit Hit;
	Hit.Instigator = Caster;
	Hit.Damage = Damage;
	Hit.Element = WRTags::Element_Arcane;
	Hit.PathId = TEXT("arcane");
	Hit.bSpell = true;
	if (Mark > 0.f) Hit.Buildups.Add(TEXT("mark"), Mark);
	Hit.Origin = Caster->GetActorLocation();
	return Hit;
}

void UWRGA_ArcaneSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Charge = 1.f;
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// A chargeable spell (Arcane blast) whose key is still held charges, x1.5 at 1.0 s (combat.md 2.13).
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (Spell(TEXT("chargeable"), 0.f) > 0.f && Spec && Spec->InputPressed)
	{
		bCharging = true;
		ChargeStarted = GetWorld()->GetTimeSeconds();
		Caster->SetCombatTag(WRTags::State_Charging, true);
		SetMoveRoot(true);
		Caster->PlaySwing(EWRSwingKind::Cast, 1.2f);
		if (IsServer()) WRFx::BroadcastOn(Caster, TEXT("arcane.cast"), TEXT("hand"), 1.4f, TEXT("arcane.charge"), WRTune(TEXT("spells.chargeMax"), 1.f) + 0.1f);
		if (UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true))
		{
			Wait->OnRelease.AddDynamic(this, &UWRGA_ArcaneSpell::OnChargeReleased);
			Wait->ReadyForActivation();
		}
		After(WRTune(TEXT("spells.chargeMax"), 1.f), [this]() { if (bCharging) OnChargeReleased(0.f); });
		return;
	}
	CastNow();
}

void UWRGA_ArcaneSpell::OnChargeReleased(float /*TimeHeld*/)
{
	if (!bCharging) return;
	bCharging = false;
	const float Held = float(GetWorld()->GetTimeSeconds() - ChargeStarted);
	const float Max = FMath::Max(0.1f, WRTune(TEXT("spells.chargeMax"), 1.f));
	Charge = FMath::Lerp(1.f, WRTune(TEXT("spells.chargeDamage"), 1.5f), FMath::Clamp(Held / Max, 0.f, 1.f));
	if (AWRCharacterBase* C = GetCharacter()) C->SetCombatTag(WRTags::State_Charging, false);
	CastNow();
}

void UWRGA_ArcaneSpell::CastNow()
{
	AWRCharacterBase* Caster = GetCharacter();
	if (!Caster)
	{
		EndSelf(true);
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const FString ShapeRow = TEXT("spells.shapes.") + ShapeName;
	// Quick cast / Quick mind shorten the wind-up, never under the 0.14 s coil (2.17).
	const float Windup = FMath::Max(WRTune(TEXT("arcane.minWindup"), 0.14f), Spell(TEXT("windup"), T.Num(ShapeRow + TEXT(".windup"), 0.3f)) * WRArcane::WindupScale(Caster));
	const float Recovery = Spell(TEXT("recovery"), T.Num(ShapeRow + TEXT(".recovery"), 0.35f));
	if (Spell(TEXT("roots"), T.Num(ShapeRow + TEXT(".roots"), 0.f)) > 0.f) SetMoveRoot(true);
	else SetMoveSlow(true, true);
	Caster->PlaySwing(EWRSwingKind::Cast, Windup + 0.1f);
	Caster->SetCombatTag(WRTags::State_Casting, true);
	if (IsServer())
	{
		// Violet light gathers in the casting hand, hexafoil sparks turning in it.
		WRFx::BroadcastOn(Caster, TEXT("arcane.cast"), TEXT("hand"), 1.f, TEXT("arcane.cast"), Windup + 0.1f);
	}
	After(Windup, [this]()
	{
		AWRCharacterBase* C = GetCharacter();
		if (!C) return;
		C->SetCombatTag(WRTags::State_Casting, false);
		if (!IsServer() || !C->IsAlive()) return;
		TakeAim(C);
		CastArcane(C, Charge, false);
		WRArcane::OnCast(C);
		// Echo: 20% the same spell again 0.6 s later, free, at the same aim (never a capstone or a shield).
		if (bEchoes && WRArcane::Rank(C, TEXT("echo")) > 0 && FMath::FRand() < WRTune(TEXT("arcane.echoChance"), 0.2f))
		{
			TWeakObjectPtr<UWRGA_ArcaneSpell> Self(this);
			TWeakObjectPtr<AWRCharacterBase> WeakC(C);
			const float Ch = Charge;
			FTimerHandle H;
			C->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self, WeakC, Ch]()
			{
				AWRCharacterBase* Who = WeakC.Get();
				if (!Self.IsValid() || !Who || !Who->IsAlive()) return;
				WRFx::BroadcastOn(Who, TEXT("arcane.echo"), TEXT("hand"), 1.f, TEXT("arcane.echo"), 0.3f);
				Self->CastArcane(Who, Ch, true);
			}), WRTune(TEXT("arcane.echoDelay"), 0.6f), false);
		}
	});
	After(Windup + Recovery, [this]() { EndSelf(); });
}

void UWRGA_ArcaneSpell::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AWRCharacterBase* C = GetCharacter())
	{
		C->SetCombatTag(WRTags::State_Charging, false);
		C->SetCombatTag(WRTags::State_Casting, false);
	}
	bCharging = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UWRGA_ArcaneSpell::AimedGround(AWRCharacterBase* Caster, float Range) const
{
	const FVector Feet = AaFeet(Caster);
	if (const AWRCharacterBase* Lock = AaLocked(Caster))
	{
		const FVector Off = Lock->GetActorLocation() - Feet;
		return AaGround(Caster, FVector(Feet.X, Feet.Y, Lock->GetActorLocation().Z) + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Caster);
	FVector Point;
	if (Survivor && Survivor->CrosshairPoint(Range, false, Point))
	{
		const FVector Off = Point - Feet;
		return FVector(Off.X, Off.Y, 0.f).Size() <= Range ? Point : AaGround(Caster, Feet + FVector(Off.X, Off.Y, 0.f).GetClampedToMaxSize(Range));
	}
	return AaGround(Caster, Feet + FRotator(0.f, Caster->GetBaseAimRotation().Yaw, 0.f).Vector() * Range * 0.6f);
}

// ================================================================================================ the spells

UWRGA_ArcaneMissile::UWRGA_ArcaneMissile()
{
	DisplayName = TEXT("Arcane missile"); NodeId = TEXT("arcane_missile");
	SpellKey = TEXT("spells.arcaneMissile"); CooldownKey = TEXT("spells.arcaneMissile.cooldown"); CooldownFallback = 0.8f;
	ShapeName = TEXT("bolt");
	bAllowedMounted = true;
}

void UWRGA_ArcaneMissile::TakeAim(AWRCharacterBase* Caster)
{
	const float Speed = FMath::Min(Spell(TEXT("speed"), 2800.f), WRTune(TEXT("spells.maxBoltSpeed"), 4000.f));
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	AWRCharacterBase* Lock = AaLocked(Caster);
	AimLock = Lock;
	if (Lock)
	{
		const FVector Velocity = WRTune(TEXT("spells.lockLead"), 1.f) > 0.f ? Lock->GetVelocity() : FVector::ZeroVector;
		AimDir = (UWRCombatLibrary::LeadPoint(Chest, Lock->GetActorLocation(), Velocity, Speed) - Chest).GetSafeNormal();
	}
	else
	{
		AimDir = AaThroughCrosshair(Caster, Chest);
	}
}

void UWRGA_ArcaneMissile::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	const float Speed = FMath::Min(Spell(TEXT("speed"), 2800.f), WRTune(TEXT("spells.maxBoltSpeed"), 4000.f));
	const FVector Chest = Caster->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	// Prism (Sun + Arcane 25, when the Sun path is built) splits it; for now one bolt along the aim.
	FWRHit Hit = ArcaneHit(Caster, Spell(TEXT("damage"), 14.f), Spell(TEXT("mark"), 25.f));
	Hit.bProjectile = true;
	Hit.Origin = Chest;
	Hit.PoiseDamage = Spell(TEXT("poise"), 8.f);
	const FVector SpawnAt = Chest + AimDir * 70.f;
	FHitResult Near;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRArcaneMuzzle), false, Caster);
	if (Caster->GetWorld()->SweepSingleByChannel(Near, Chest, SpawnAt, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(14.f), Query))
	{
		AWRCharacterBase* Blocker = Cast<AWRCharacterBase>(Near.GetActor());
		if (Blocker && Caster->IsHostileTo(Blocker)) { UWRCombatLibrary::ApplyHit(Blocker, Hit); return; }
	}
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Caster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRProjectile* Bolt = Caster->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), SpawnAt, AimDir.Rotation(), Params))
	{
		Bolt->Init(Hit, Speed, AaViolet);
		Bolt->SetTrail(TEXT("arcane.missile"));
		AWRCharacterBase* Lock = AimLock.Get();
		if (Lock && Lock->IsAlive() && Lock->IsCreature())
		{
			const float Homing = WRTune(TEXT("spells.lockHoming"), 6000.f);
			if (Homing > 0.f) Bolt->HomeOn(Lock, Homing);
		}
	}
}

UWRGA_Barrier::UWRGA_Barrier()
{
	DisplayName = TEXT("Barrier"); NodeId = TEXT("barrier");
	SpellKey = TEXT("spells.barrier"); CooldownKey = TEXT("spells.barrier.cooldown"); CooldownFallback = 12.f;
	ShapeName = TEXT("ally");
	bEchoes = false;
	bAllowedMounted = true;
}

void UWRGA_Barrier::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	AWRCharacterBase* On = Caster;
	// Ward allies: the ally (or tame) nearest your crosshair within 15 m that you can see; none, yourself.
	if (WRArcane::Rank(Caster, TEXT("ward_allies")) > 0)
	{
		const FVector Eye = Caster->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		const FVector View = Caster->GetBaseAimRotation().Vector();
		float Best = WRTune(TEXT("spells.barrier.allyCone"), 0.9f);
		for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (C == Caster || !AaAlly(Caster, C)) continue;
			const FVector To = C->GetActorLocation() - Eye;
			if (To.Size() > Spell(TEXT("allyRange"), 1500.f)) continue;
			const float Dot = float(FVector::DotProduct(To.GetSafeNormal(), View));
			if (Dot <= Best || !AaClearLine(Caster, Eye, C)) continue;
			Best = Dot;
			On = C;
		}
	}
	const float Amount = Spell(TEXT("shield"), 30.f) * WRArcane::ShieldScale(Caster);
	WRArcane::GiveShield(On, Amount, Spell(TEXT("seconds"), 8.f), Caster);
	WRFx::BroadcastOn(On, TEXT("arcane.barrier"), TEXT("spine"), 1.f, TEXT("arcane.barrier"), 0.6f);
	if (On != Caster) WRFx::Broadcast(Caster, TEXT("arcane.ally_thread"), Caster->GetActorLocation(), (On->GetActorLocation() - Caster->GetActorLocation()).Rotation(), FVector::Dist(On->GetActorLocation(), Caster->GetActorLocation()) / 100.f);
}

UWRGA_ArcaneBlast::UWRGA_ArcaneBlast()
{
	DisplayName = TEXT("Arcane blast"); NodeId = TEXT("arcane_blast");
	SpellKey = TEXT("spells.arcaneBlast"); CooldownKey = TEXT("spells.arcaneBlast.cooldown"); CooldownFallback = 8.f;
	ShapeName = TEXT("nova");
}

void UWRGA_ArcaneBlast::TakeAim(AWRCharacterBase* Caster)
{
	AimPoint = AimedGround(Caster, Spell(TEXT("range"), 1200.f));
}

void UWRGA_ArcaneBlast::CastArcane(AWRCharacterBase* Caster, float Ch, bool /*bEcho*/)
{
	const float Radius = Spell(TEXT("radius"), 300.f);
	WRFx::Broadcast(Caster, TEXT("arcane.blast"), AimPoint, FRotator::ZeroRotator, Radius / 150.f, TEXT("arcane.blast"));
	WRCrystal::Emit(Caster, AimPoint, WRCrystal::NoiseRadius(TEXT("cast")) * 1.5f, Caster, TEXT("cast"));
	// Path raiding (cookpass-building 3.7): the blast hits the buildings in it too (arcane x1.25 on stone).
	FWRRaiding::SplashPieces(Caster->GetWorld(), AimPoint, Radius, Spell(TEXT("damage"), 32.f) * Ch, EWRRaidSource::Spell, Caster, FWRRaiding::Responsible(Caster), PathId);
	for (TActorIterator<AWRCharacterBase> It(Caster->GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || !Caster->IsHostileTo(Who) || FVector::Dist(Who->GetActorLocation(), AimPoint) > Radius + Who->GetSimpleCollisionRadius()) continue;
		FWRHit Hit = ArcaneHit(Caster, Spell(TEXT("damage"), 32.f) * Ch, Spell(TEXT("mark"), 20.f));
		Hit.Origin = AimPoint;
		Hit.PoiseDamage = Spell(TEXT("poise"), 40.f) * Ch;
		Hit.StanceDamage = Hit.Damage * Spell(TEXT("stance"), 2.f);
		Hit.Impact = 3;
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

UWRGA_RuneTrap::UWRGA_RuneTrap()
{
	DisplayName = TEXT("Rune trap"); NodeId = TEXT("rune_trap");
	SpellKey = TEXT("spells.runeTrap"); CooldownKey = TEXT("spells.runeTrap.cooldown"); CooldownFallback = 10.f;
	ShapeName = TEXT("pool");
}

void UWRGA_RuneTrap::TakeAim(AWRCharacterBase* Caster)
{
	AimPoint = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
}

void UWRGA_RuneTrap::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	// Never inside another clan's claim (BL "no traps" spirit): the rune won't take there.
	FString Why;
	const APlayerState* PS = Caster->GetPlayerState();
	if (PS && !AWRClaimTotem::CanBuild(Caster->GetWorld(), AimPoint, PS->GetPlayerName(), Why))
	{
		WRArcane::GainMana(Caster, ManaCost());
		AaTell(Caster, TEXT("The rune won't take inside someone else's claim"));
		WRFx::Broadcast(Caster, TEXT("crystal.fizzle"), AimPoint, FRotator::ZeroRotator, 1.f, TEXT("crystal.fizzle"));
		return;
	}
	// At most three of yours: the oldest fades as the fourth is drawn.
	TArray<AWRArcaneField*> Mine;
	for (const TWeakObjectPtr<AWRArcaneField>& W : AWRArcaneField::All())
	{
		if (AWRArcaneField* F = W.Get(); F && F->Kind == TEXT("rune") && F->Caster == Caster && F->Until > F->ServerNow()) Mine.Add(F);
	}
	Mine.Sort([](const AWRArcaneField& A, const AWRArcaneField& B) { return A.Born < B.Born; });
	for (int32 i = 0; i + int32(Spell(TEXT("max"), 3.f)) <= Mine.Num(); ++i) Mine[i]->Finish(false);
	const float Seconds = Spell(TEXT("seconds"), 30.f) * (1.f + WRTune(TEXT("arcane.lastingPerRank"), 0.25f) * WRArcane::Rank(Caster, TEXT("lasting_runes")));
	const FTransform Tr(FRotator(0.f, Caster->GetActorRotation().Yaw, 0.f), AimPoint + FVector(0.f, 0.f, 3.f));
	if (AWRArcaneField* F = Caster->GetWorld()->SpawnActorDeferred<AWRArcaneField>(AWRArcaneField::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		F->Kind = TEXT("rune");
		F->Radius = Spell(TEXT("radius"), 300.f);
		F->Born = AaServerNow(Caster);
		F->Until = F->Born + Seconds;
		F->Caster = Caster;
		F->Damage = Spell(TEXT("damage"), 40.f) * Glyph(Caster);
		// Frozen rune (Arcane + Frost 50): the burst freezes.
		F->bLinked = WRArcane::Rank(Caster, TEXT("rune_link")) > 0;
		F->Frost = Caster->HasTag(WRArcane::Combo_FrozenRune) ? FWRComboRegistry::Get().Num(TEXT("frozen_rune"), TEXT("frost"), 100.f) : 0.f;
		F->FinishSpawning(Tr);
	}
	WRSound::PlayAt(Caster, TEXT("arcane.rune_drawn"), AimPoint);
}

UWRGA_Inscribe::UWRGA_Inscribe()
{
	DisplayName = TEXT("Inscribe"); NodeId = TEXT("inscribe");
	SpellKey = TEXT("spells.inscribe"); CooldownKey = TEXT("spells.inscribe.cooldown"); CooldownFallback = 30.f;
	ShapeName = TEXT("buff");
	bEchoes = false;
}

void UWRGA_Inscribe::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	// A rune drawn down the blade with a fingertip: violet lines along the weapon for 30 s (+Glyph power on its damage).
	const float Seconds = Spell(TEXT("seconds"), 30.f);
	UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
	if (!ASC) return;
	if (!ASC->HasMatchingGameplayTag(WRArcane::Status_Inscribed)) ASC->AddLooseGameplayTag(WRArcane::Status_Inscribed, 1, EGameplayTagReplicationState::TagOnly);
	WRFx::BroadcastOn(Caster, TEXT("arcane.inscribe"), TEXT("weapon"), 1.f, TEXT("arcane.inscribe"), Seconds);
	TWeakObjectPtr<AWRCharacterBase> Weak(Caster);
	FTimerHandle H;
	Caster->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak]()
	{
		AWRCharacterBase* C = Weak.Get();
		if (UAbilitySystemComponent* A = C ? C->GetAbilitySystemComponent() : nullptr; A && A->HasMatchingGameplayTag(WRArcane::Status_Inscribed))
		{
			A->RemoveLooseGameplayTag(WRArcane::Status_Inscribed, 1, EGameplayTagReplicationState::TagOnly);
		}
	}), Seconds, false);
}

UWRGA_Aegis::UWRGA_Aegis()
{
	DisplayName = TEXT("Aegis"); NodeId = TEXT("aegis");
	SpellKey = TEXT("spells.aegis"); CooldownKey = TEXT("spells.aegis.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("nova");
	bEchoes = false;
}

void UWRGA_Aegis::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	const FVector Feet = AaFeet(Caster);
	const FTransform Tr(FRotator::ZeroRotator, Feet);
	if (AWRArcaneField* F = Caster->GetWorld()->SpawnActorDeferred<AWRArcaneField>(AWRArcaneField::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		F->Kind = TEXT("dome");
		F->Radius = Spell(TEXT("radius"), 500.f);
		F->Born = AaServerNow(Caster);
		// The Tyrant plate talisman: Barrier and Aegis +20% (the dome holds that much longer).
		F->Until = F->Born + Spell(TEXT("seconds"), 8.f) * (1.f + UWRInventoryComponent::GearStatFor(Caster, TEXT("shieldBonus")));
		F->Caster = Caster;
		F->bReflect = WRArcane::Rank(Caster, TEXT("reflect")) > 0;
		F->FinishSpawning(Tr);
	}
	WRSound::PlayAt(Caster, TEXT("arcane.aegis"), Feet);
}

UWRGA_Starfall::UWRGA_Starfall()
{
	DisplayName = TEXT("Starfall"); NodeId = TEXT("starfall");
	SpellKey = TEXT("spells.starfall"); CooldownKey = TEXT("spells.starfall.cooldown"); CooldownFallback = 50.f;
	ShapeName = TEXT("strike");
	bEchoes = false;
}

void UWRGA_Starfall::TakeAim(AWRCharacterBase* Caster)
{
	AimPoint = AimedGround(Caster, Spell(TEXT("range"), 2500.f));
}

void UWRGA_Starfall::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	// Twelve stars over r 700, falling one after another; each landing point glints 1.2 s before the star arrives (the
	// strike delay), and a star never lands on a point it didn't mark.
	const int32 N = int32(Spell(TEXT("stars"), 12.f));
	const float R = Spell(TEXT("radius"), 700.f);
	const float Glint = Spell(TEXT("glint"), 1.2f);
	const float Spread = Spell(TEXT("spread"), 1.8f);
	const float Burn = Caster->HasTag(WRArcane::Combo_MeteorStorm) ? FWRComboRegistry::Get().Num(TEXT("meteor_storm"), TEXT("burn"), 30.f) : 0.f;
	const float Bleed = Caster->HasTag(WRArcane::Combo_BloodStar) ? FWRComboRegistry::Get().Num(TEXT("blood_star"), TEXT("bleed"), 25.f) : 0.f;
	TWeakObjectPtr<AWRCharacterBase> Weak(Caster);
	FRandomStream Rng(FMath::Rand());
	for (int32 i = 0; i < N; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI), D = R * FMath::Sqrt(Rng.FRand());
		const FVector At = AaGround(Caster, AimPoint + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f));
		const float Start = Spread * i / FMath::Max(1, N - 1);
		const float StarDamage = Spell(TEXT("damage"), 18.f);
		const float StarRadius = Spell(TEXT("starRadius"), 160.f);
		const float Mark = Spell(TEXT("mark"), 15.f);
		FTimerHandle G;
		Caster->GetWorldTimerManager().SetTimer(G, FTimerDelegate::CreateLambda([Weak, At, Glint]()
		{
			if (AWRCharacterBase* C = Weak.Get()) WRFx::Broadcast(C, TEXT("arcane.star_glint"), At + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, 1.f, TEXT("arcane.star_glint"), Glint);
		}), FMath::Max(0.01f, Start), false);
		FTimerHandle L;
		Caster->GetWorldTimerManager().SetTimer(L, FTimerDelegate::CreateLambda([Weak, At, StarDamage, StarRadius, Mark, Burn, Bleed]()
		{
			AWRCharacterBase* C = Weak.Get();
			if (!C) return;
			WRFx::Broadcast(C, Burn > 0.f ? TEXT("arcane.meteor") : TEXT("arcane.star"), At, FRotator::ZeroRotator, StarRadius / 120.f, TEXT("arcane.star_land"));
			FWRRaiding::SplashPieces(C->GetWorld(), At, StarRadius, StarDamage, EWRRaidSource::Spell, C, FWRRaiding::Responsible(C), TEXT("arcane"));
			for (TActorIterator<AWRCharacterBase> It(C->GetWorld()); It; ++It)
			{
				AWRCharacterBase* Who = *It;
				if (!Who->IsAlive() || !C->IsHostileTo(Who) || FVector::Dist(Who->GetActorLocation(), At) > StarRadius + Who->GetSimpleCollisionRadius()) continue;
				FWRHit Hit;
				Hit.Instigator = C;
				Hit.Damage = StarDamage;
				Hit.Element = WRTags::Element_Arcane;
				Hit.PathId = TEXT("arcane");
				Hit.bSpell = true;
				Hit.Origin = At + FVector(0.f, 0.f, 400.f);
				Hit.Impact = 2;
				Hit.Buildups.Add(TEXT("mark"), Mark);
				if (Burn > 0.f) Hit.Buildups.Add(TEXT("burn"), Burn);
				if (Bleed > 0.f) Hit.Buildups.Add(TEXT("bleed"), Bleed);
				UWRCombatLibrary::ApplyHit(Who, Hit);
			}
		}), Start + Glint, false);
	}
}

UWRGA_RuneCircle::UWRGA_RuneCircle()
{
	DisplayName = TEXT("Rune circle"); NodeId = TEXT("rune_circle");
	SpellKey = TEXT("spells.runeCircle"); CooldownKey = TEXT("spells.runeCircle.cooldown"); CooldownFallback = 60.f;
	ShapeName = TEXT("summon");
	bEchoes = false;
}

void UWRGA_RuneCircle::TakeAim(AWRCharacterBase* Caster)
{
	AimPoint = AimedGround(Caster, Spell(TEXT("range"), 1500.f));
}

void UWRGA_RuneCircle::CastArcane(AWRCharacterBase* Caster, float /*Charge*/, bool /*bEcho*/)
{
	const FTransform Tr(FRotator::ZeroRotator, AimPoint + FVector(0.f, 0.f, 3.f));
	if (AWRArcaneField* F = Caster->GetWorld()->SpawnActorDeferred<AWRArcaneField>(AWRArcaneField::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		F->Kind = TEXT("circle");
		F->Radius = Spell(TEXT("radius"), 600.f);
		F->Born = AaServerNow(Caster);
		F->Until = F->Born + Spell(TEXT("seconds"), 15.f) * Glyph(Caster);
		F->Caster = Caster;
		F->bFaerie = Caster->HasTag(WRArcane::Combo_FaerieRing);
		F->FinishSpawning(Tr);
	}
	WRSound::PlayAt(Caster, TEXT("arcane.rune_circle"), AimPoint);
}

UWRGA_RunicStrike::UWRGA_RunicStrike()
{
	DisplayName = TEXT("Runic strike"); PathId = TEXT("arcane"); NodeId = TEXT("runic_strike");
	StaminaKey = TEXT("stamina.weaponArt"); StaminaFallback = 18.f;
	CooldownKey = TEXT("spells.runicStrike.cooldown"); CooldownFallback = 2.5f;
	TimingKey = TEXT("weapon.lightTiming"); SwingKind = EWRSwingKind::WeaponArt;
	Element = WRTags::Element_Arcane; ArtKey = TEXT("spells.runicStrike");
	StanceMultiplier = 1.5f;
}

bool UWRGA_RunicStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && AaCanPay(ActorInfo, WRTune(TEXT("spells.runicStrike.manaCost"), 15.f));
}

void UWRGA_RunicStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Mana is spent as the swing begins; a hexafoil flares along the blade.
	if (IsServer() && IsActive())
	{
		WRArcane::SpendMana(GetCharacter(), WRTune(TEXT("spells.runicStrike.manaCost"), 15.f));
		WRFx::BroadcastOn(GetCharacter(), TEXT("arcane.weapon"), TEXT("weapon"), 1.f, TEXT("arcane.cast"), 0.5f);
		WRArcane::OnCast(GetCharacter());
	}
}

// ================================================================================================ AWRArcaneField

AWRArcaneField::AWRArcaneField()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(4.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRArcaneField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRArcaneField, Kind);
	DOREPLIFETIME(AWRArcaneField, Radius);
	DOREPLIFETIME(AWRArcaneField, Born);
	DOREPLIFETIME(AWRArcaneField, Until);
	DOREPLIFETIME(AWRArcaneField, Caster);
	DOREPLIFETIME(AWRArcaneField, bLinked);
}

TArray<TWeakObjectPtr<AWRArcaneField>>& AWRArcaneField::All()
{
	static TArray<TWeakObjectPtr<AWRArcaneField>> List;
	List.RemoveAll([](const TWeakObjectPtr<AWRArcaneField>& W) { return !W.IsValid(); });
	return List;
}

float AWRArcaneField::ServerNow() const
{
	return AaServerNow(this);
}

float AWRArcaneField::Fade(float Now) const
{
	// Rises over 0.3 s, holds, fades over 0.5 s at its end (never pops).
	const float In = FMath::Clamp((Now - Born) / 0.3f, 0.f, 1.f);
	const float Out = FMath::Clamp((Until - Now) / 0.5f, 0.f, 1.f);
	return FMath::Min(In, Out);
}

void AWRArcaneField::BeginPlay()
{
	Super::BeginPlay();
	All().AddUnique(this);
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

void AWRArcaneField::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
	{
		for (const TWeakObjectPtr<AWRCharacterBase>& W : InCircle)
		{
			if (UAbilitySystemComponent* ASC = W.IsValid() ? W->GetAbilitySystemComponent() : nullptr; ASC && ASC->HasMatchingGameplayTag(WRArcane::Status_InCircle))
			{
				ASC->RemoveLooseGameplayTag(WRArcane::Status_InCircle, 1, EGameplayTagReplicationState::TagOnly);
			}
		}
		InCircle.Reset();
	}
	All().Remove(this);
	Super::EndPlay(Reason);
}

void AWRArcaneField::Finish(bool bShattered)
{
	if (!HasAuthority() || bDone) return;
	bDone = true;
	const float Now = ServerNow();
	Until = FMath::Min(Until, Now + 0.5f);
	ForceNetUpdate();
	if (bShattered) WRFx::Broadcast(this, TEXT("arcane.dome_break"), GetActorLocation() + FVector(0.f, 0.f, Radius * 0.5f), FRotator::ZeroRotator, Radius / 300.f, TEXT("arcane.dome_break"));
	SetLifeSpan(0.6f);
}

void AWRArcaneField::BuildLook()
{
	FWRToolMesh M;
	const FLinearColor Col = Kind == TEXT("circle") ? AaLilac : AaViolet;
	if (Kind == TEXT("rune"))
	{
		// A hexafoil a little smaller than its reach, its compass-point dots at the centres, faint on the ground.
		AaHexafoil(M, FVector(0.f, 0.f, 1.f), Radius * 0.55f, 2.2f, Col, 1.f);
		AaRing(M, FVector(0.f, 0.f, 1.f), Radius * 0.98f, 1.2f, Col, 0.4f);
	}
	else if (Kind == TEXT("dome"))
	{
		AaDome(M, Radius, Col);
		AaStave(M, FVector(0.f, 0.f, 2.f), Radius * 0.85f, 3.f, Col);
	}
	else
	{
		// The great circle: a double ring, eight hexafoils round it, compass lines crossing it like a surveyor's drawing.
		AaRing(M, FVector(0.f, 0.f, 1.f), Radius, 3.f, Col, 1.f);
		AaRing(M, FVector(0.f, 0.f, 1.f), Radius * 0.93f, 1.6f, Col, 0.6f);
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = k * PI / 4.f;
			AaHexafoil(M, FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius * 0.965f + FVector(0.f, 0.f, 1.5f), Radius * 0.06f, 1.2f, Col, 1.f);
			const FVector P0 = FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius * 0.93f, P1 = FVector(FMath::Cos(A + PI * 0.75f), FMath::Sin(A + PI * 0.75f), 0.f) * Radius * 0.93f;
			M.Tube({ P0 + FVector(0, 0, 1.f), P1 + FVector(0, 0, 1.f) }, 0.9f, 3, Col, 0.5f);
		}
		AaHexafoil(M, FVector(0.f, 0.f, 1.5f), Radius * 0.22f, 2.f, Col, 1.f);
	}
	Look = AaMesh(this, nullptr, TEXT("ArcaneLook"));
	M.ApplyTo(Look);
	Glow = AaWardMID(this, Col, 0.f);
	if (Glow) Look->SetMaterial(0, Glow);
	Lamp = NewObject<UPointLightComponent>(this, TEXT("ArcaneLamp"));
	Lamp->SetupAttachment(RootComponent);
	Lamp->SetRelativeLocation(FVector(0.f, 0.f, Kind == TEXT("dome") ? Radius * 0.4f : 60.f));
	Lamp->SetLightColor(Col);
	Lamp->SetIntensity(0.f);
	Lamp->SetAttenuationRadius(Radius * 1.6f);
	Lamp->SetCastShadows(false);
	Lamp->RegisterComponent();
}

void AWRArcaneField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	if (Look)
	{
		const float F = Fade(Now);
		const bool bDome = Kind == TEXT("dome");
		if (Glow) Glow->SetScalarParameterValue(TEXT("Glow"), F * WRTune(bDome ? TEXT("arcane.look.domeGlow") : Kind == TEXT("rune") ? TEXT("arcane.look.runeGlow") : TEXT("arcane.look.circleGlow"), bDome ? 1.4f : 0.9f));
		if (bDome) Look->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(0.02f, F)));
		if (Lamp) Lamp->SetIntensity(F * WRTune(bDome ? TEXT("arcane.look.domeLight") : TEXT("arcane.look.fieldLight"), bDome ? 3000.f : 600.f));
		if (Kind == TEXT("circle")) Look->SetRelativeRotation(FRotator(0.f, Now * 6.f, 0.f));
	}
	if (!HasAuthority()) return;
	if (Now >= Until) { if (!bDone) { bDone = true; SetLifeSpan(0.1f); } return; }
	if (bDone) return;
	if (Kind == TEXT("rune")) StepRune(DeltaSeconds);
	else if (Kind == TEXT("dome")) StepDome(DeltaSeconds);
	else StepCircle(DeltaSeconds);
}

void AWRArcaneField::StepRune(float Dt)
{
	AWRCharacterBase* By = Caster;
	if (!By) { Finish(false); return; }
	const FVector C = GetActorLocation();
	// An enemy steps on it: it bursts.
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || !By->IsHostileTo(Who)) continue;
		if (FVector::Dist2D(Who->GetActorLocation(), C) > Radius + Who->GetSimpleCollisionRadius() * 0.5f || FMath::Abs(Who->GetActorLocation().Z - Who->GetSimpleCollisionHalfHeight() - C.Z) > 150.f) continue;
		WRFx::Broadcast(this, TEXT("arcane.rune_burst"), C, FRotator::ZeroRotator, Radius / 150.f, TEXT("arcane.rune_burst"));
		WRCrystal::Emit(this, C, WRCrystal::NoiseRadius(TEXT("cast")), By, TEXT("cast"));
		for (TActorIterator<AWRCharacterBase> Jt(GetWorld()); Jt; ++Jt)
		{
			AWRCharacterBase* V = *Jt;
			if (!V->IsAlive() || !By->IsHostileTo(V) || FVector::Dist(V->GetActorLocation(), C) > Radius + V->GetSimpleCollisionRadius() + 60.f) continue;
			FWRHit Hit;
			Hit.Instigator = By;
			Hit.Damage = Damage;
			Hit.Element = WRTags::Element_Arcane;
			Hit.PathId = TEXT("arcane");
			Hit.bSpell = true;
			Hit.Origin = C;
			Hit.Impact = 3;
			Hit.Buildups.Add(TEXT("mark"), WRTune(TEXT("spells.runeTrap.mark"), 25.f));
			if (Frost > 0.f) Hit.Buildups.Add(TEXT("frost"), Frost);
			UWRCombatLibrary::ApplyHit(V, Hit);
		}
		Finish(false);
		return;
	}
	// Rune link: a violet thread to each of the caster's runes within 8 m; an enemy crossing one takes 12 and mark 15
	// (once every 0.5 s). Each pair is worked by the older rune.
	if (!bLinked) return;
	const double T = GetWorld()->GetTimeSeconds();
	for (const TWeakObjectPtr<AWRArcaneField>& W : All())
	{
		AWRArcaneField* O = W.Get();
		if (!O || O == this || O->Kind != TEXT("rune") || O->Caster != By || !O->bLinked || O->Born < Born || O->bDone) continue;
		const FVector A = C + FVector(0.f, 0.f, 20.f), B = O->GetActorLocation() + FVector(0.f, 0.f, 20.f);
		if (FVector::Dist(A, B) > WRTune(TEXT("arcane.runeLink"), 800.f)) continue;
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			if (!Who->IsAlive() || !By->IsHostileTo(Who)) continue;
			const FVector Feet = AaFeet(Who) + FVector(0.f, 0.f, 20.f);
			const FVector P = FMath::ClosestPointOnSegment(Feet, A, B);
			if (FVector::Dist(P, Feet) > Who->GetSimpleCollisionRadius() + 20.f) continue;
			double& Last = ThreadHitAt.FindOrAdd(Who);
			if (T - Last < WRTune(TEXT("arcane.runeLinkEvery"), 0.5f)) continue;
			Last = T;
			FWRHit Hit;
			Hit.Instigator = By;
			Hit.Damage = WRTune(TEXT("arcane.runeLinkDamage"), 12.f);
			Hit.Element = WRTags::Element_Arcane;
			Hit.PathId = TEXT("arcane");
			Hit.bSpell = true;
			Hit.Origin = P;
			Hit.Impact = 1;
			Hit.Buildups.Add(TEXT("mark"), WRTune(TEXT("arcane.runeLinkMark"), 15.f));
			UWRCombatLibrary::ApplyHit(Who, Hit);
			WRFx::Broadcast(this, TEXT("arcane.thread_snap"), P, FRotator::ZeroRotator, 1.f, TEXT("arcane.thread_snap"));
		}
	}
	(void)Dt;
}

void AWRArcaneField::StepDome(float Dt)
{
	// A projectile that crosses the shell from outside stops on it (or bounces back with Reflect); a catapult stone
	// breaks the dome (and stops). Beams and melee pass: they aren't projectiles.
	const FVector C = GetActorLocation();
	AWRCharacterBase* By = Caster;
	for (TActorIterator<AWRProjectile> It(GetWorld()); It; ++It)
	{
		AWRProjectile* P = *It;
		if (!IsValid(P) || P->IsActorBeingDestroyed()) continue;
		const float D = float(FVector::Dist(P->GetActorLocation(), C));
		const float* Was = LastProjectileDist.Find(P);
		LastProjectileDist.Add(P, D);
		if (!Was || *Was <= Radius || D > Radius) continue;
		const AWRCharacterBase* From = Cast<AWRCharacterBase>(P->GetHit().Instigator.Get());
		if (By && From && !By->IsHostileTo(From)) continue;   // your own side's shots pass out and in
		const FName K = P->GetKind();
		const FVector At = C + (P->GetActorLocation() - C).GetSafeNormal() * Radius;
		if (K == TEXT("catapult") || K == TEXT("trebuchet"))
		{
			P->Destroy();
			Finish(true);
			return;
		}
		if (bReflect && By && WRArcane::IsReflectable(P->GetHit())) WRArcane::Reflect(By, P->GetHit(), P->GetHit().Damage, WRTune(TEXT("arcane.reflectScale"), 0.8f), At);
		else WRFx::Broadcast(this, TEXT("arcane.dome_stop"), At, (At - C).Rotation(), 1.f, TEXT("arcane.dome_stop"));
		P->Destroy();
	}
	for (auto It = LastProjectileDist.CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
	(void)Dt;
}

void AWRArcaneField::StepCircle(float Dt)
{
	AWRCharacterBase* By = Caster;
	if (!By) { Finish(false); return; }
	StepClock += Dt;
	const bool bPulse = StepClock >= 0.5f;
	if (bPulse) StepClock = 0.f;
	const FVector C = GetActorLocation();
	TSet<TWeakObjectPtr<AWRCharacterBase>> Now;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Who->IsAlive() || FVector::Dist2D(Who->GetActorLocation(), C) > Radius || FMath::Abs(Who->GetActorLocation().Z - C.Z) > 400.f) continue;
		if (AaAlly(By, Who))
		{
			Now.Add(Who);
			if (UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent(); ASC && !ASC->HasMatchingGameplayTag(WRArcane::Status_InCircle))
			{
				ASC->AddLooseGameplayTag(WRArcane::Status_InCircle, 1, EGameplayTagReplicationState::TagOnly);
			}
			// Faerie ring (Nature + Arcane 50): the plants heal allies 4 a second.
			if (bFaerie && bPulse)
			{
				const UWRAttributeSet* A = Who->GetAttributes();
				if (UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent(); A && ASC)
				{
					ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + FWRComboRegistry::Get().Num(TEXT("faerie_ring"), TEXT("heal"), 4.f) * 0.5f));
				}
			}
		}
		else if (By->IsHostileTo(Who) && bPulse && Who->Status)
		{
			// Enemies inside: mark 10 a second.
			Who->Status->AddStatusBuildup(WRStatus::FromName(TEXT("mark")), WRTune(TEXT("spells.runeCircle.markPerSecond"), 10.f) * 0.5f, By);
		}
	}
	for (const TWeakObjectPtr<AWRCharacterBase>& W : InCircle)
	{
		if (Now.Contains(W)) continue;
		if (UAbilitySystemComponent* ASC = W.IsValid() ? W->GetAbilitySystemComponent() : nullptr; ASC && ASC->HasMatchingGameplayTag(WRArcane::Status_InCircle))
		{
			ASC->RemoveLooseGameplayTag(WRArcane::Status_InCircle, 1, EGameplayTagReplicationState::TagOnly);
		}
	}
	InCircle = MoveTemp(Now);
}

// ================================================================================================ UWRArcaneWorld

bool UWRArcaneWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UWRArcaneWorld::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (World->GetNetMode() != NM_Client) WRArcane::TickShields(World, DeltaTime);
	if (World->GetNetMode() == NM_DedicatedServer) return;
	StepFilms(DeltaTime);
	ScanClock -= DeltaTime;
	if (ScanClock <= 0.f)
	{
		ScanClock = 0.25f;
		StepThreads();
	}
}

void UWRArcaneWorld::StepFilms(float Dt)
{
	// The faceted violet film round anyone an Arcane shield holds (2.17): it grows in over 0.25 s and thins out the same.
	UWorld* World = GetWorld();
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->HasTag(WRArcane::Status_Shielded) || Films.Contains(C)) continue;
		FFilm F;
		FWRToolMesh M;
		const float R = C->GetSimpleCollisionRadius() * 1.35f, H = C->GetSimpleCollisionHalfHeight() * 1.12f;
		// A faceted capsule of light: eight sides, five bands, the seams painted bright.
		const FVector2D Z = FVector2D::ZeroVector;
		const FColor Face = FWRToolMesh::Pack(AaViolet, 0.12f);
		auto P = [R, H](int32 i, int32 j) { const float V = -1.f + 2.f * i / 5.f, A = 2.f * PI * (j + (i % 2) * 0.5f) / 8.f; const float W = FMath::Sqrt(FMath::Max(0.05f, 1.f - V * V * 0.55f)); return FVector(FMath::Cos(A) * R * W, FMath::Sin(A) * R * W, V * H); };
		for (int32 i = 0; i < 5; ++i) for (int32 j = 0; j < 8; ++j)
		{
			M.Tri(P(i, j), P(i, j + 1), P(i + 1, j), Face, Z, Z, Z);
			M.Tri(P(i, j + 1), P(i + 1, j + 1), P(i + 1, j), Face, Z, Z, Z);
		}
		for (int32 i = 0; i <= 5; ++i) { TArray<FVector> L; for (int32 j = 0; j < 8; ++j) L.Add(P(i, j)); M.Tube(L, 0.8f, 3, AaViolet, 1.f, true); }
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(C, NAME_None);
		Mesh->SetupAttachment(C->GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		M.ApplyTo(Mesh);
		UMaterialInstanceDynamic* G = AaWardMID(Mesh, AaViolet, 0.f);
		if (G) Mesh->SetMaterial(0, G);
		F.Mesh = Mesh;
		F.Glow = G;
		Films.Add(C, F);
	}
	for (auto It = Films.CreateIterator(); It; ++It)
	{
		AWRCharacterBase* C = It->Key.Get();
		FFilm& F = It->Value;
		const bool bOn = C && C->IsAlive() && C->HasTag(WRArcane::Status_Shielded);
		F.Shown = FMath::FInterpConstantTo(F.Shown, bOn ? 1.f : 0.f, Dt, 4.f);
		if (UMaterialInstanceDynamic* G = F.Glow.Get()) G->SetScalarParameterValue(TEXT("Glow"), F.Shown * WRTune(TEXT("arcane.look.filmGlow"), 0.8f));
		if (UProceduralMeshComponent* M = F.Mesh.Get()) M->SetRelativeRotation(FRotator(0.f, GetWorld()->GetTimeSeconds() * 20.f, 0.f));
		if (!bOn && F.Shown <= 0.f)
		{
			if (UProceduralMeshComponent* M = F.Mesh.Get()) M->DestroyComponent();
			It.RemoveCurrent();
		}
	}
}

void UWRArcaneWorld::StepThreads()
{
	// Rune link's threads: drawn between a caster's runes within 8 m while they have the node (the server works them;
	// every machine sees the same pairs because the runes and their casters replicate).
	for (FThread& T : Threads) if (UProceduralMeshComponent* M = T.Mesh.Get()) M->DestroyComponent();
	Threads.Reset();
	TArray<AWRArcaneField*> Runes;
	for (const TWeakObjectPtr<AWRArcaneField>& W : AWRArcaneField::All()) if (AWRArcaneField* F = W.Get(); F && F->Kind == TEXT("rune") && F->Caster && F->bLinked && F->Until > F->ServerNow()) Runes.Add(F);
	for (int32 i = 0; i < Runes.Num(); ++i)
	{
		for (int32 j = i + 1; j < Runes.Num(); ++j)
		{
			AWRArcaneField* A = Runes[i];
			AWRArcaneField* B = Runes[j];
			if (A->Caster != B->Caster || FVector::Dist(A->GetActorLocation(), B->GetActorLocation()) > WRTune(TEXT("arcane.runeLink"), 800.f)) continue;
			// Both drawn by a caster who had Rune link (bLinked replicates with the rune).
			FWRToolMesh M;
			const FVector P0(0.f, 0.f, 20.f), P1 = B->GetActorLocation() - A->GetActorLocation() + FVector(0.f, 0.f, 20.f);
			TArray<FVector> Pts;
			for (int32 k = 0; k <= 8; ++k) { const float Tk = k / 8.f; Pts.Add(FMath::Lerp(P0, P1, Tk) + FVector(0.f, 0.f, -FMath::Sin(Tk * PI) * 6.f)); }
			M.Tube(Pts, 1.2f, 4, AaViolet, 1.f);
			UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(A, NAME_None);
			Mesh->SetupAttachment(A->GetRootComponent());
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->RegisterComponent();
			M.ApplyTo(Mesh);
			if (UMaterialInstanceDynamic* G = AaWardMID(Mesh, AaViolet, 0.7f)) Mesh->SetMaterial(0, G);
			FThread T;
			T.Mesh = Mesh;
			T.A = A;
			T.B = B;
			Threads.Add(T);
		}
	}
}
