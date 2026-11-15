// The Ocean's creatures: their body component, their brains (FWROceanAI) and the living sea (UWROceanLife). See
// WROceanCreatures.h; cooks/biome-ocean.md 2.12, build-biome-ocean.md 2.12. Numbers: tuning oceanLife.

#include "WROceanCreatures.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRCharacter.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WROceanTypes.h"
#include "WROceanWorld.h"
#include "WRPlayerController.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTide.h"
#include "WRTidePath.h"
#include "WRTuning.h"
#include "WRUnderwater.h"
#include "WRSurvival.h"
#include "WRWorldSystems.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

TArray<TWeakObjectPtr<UWROceanBody>> UWROceanBody::Live;

namespace WROceanCrAI
{
	float OcTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("oceanLife.%s"), Key), Fallback); }
	double OcNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool OcNight(const UObject* W) { return AWRGameState::IsNightIn(W); }
	void OcTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	FString OcName(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		return P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	}
	AWRCharacter* OcPlayerNamed(const UWorld* W, const FString& Name)
	{
		if (!W || Name.IsEmpty()) return nullptr;
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && It->Get()->PlayerState && It->Get()->PlayerState->GetPlayerName() == Name) return Cast<AWRCharacter>(It->Get()->GetPawn());
		}
		return nullptr;
	}
	/** The water's surface over a point (the tide's level in the Ocean; the sea's elsewhere). */
	float OcSurface(const UObject* W, const FVector& P) { return WRTide::SurfaceAt(W, P); }
	float OcFloor(const FVector& P, float Fallback) { return WRLand::GroundZAt(P, Fallback); }
	bool OcInWater(const UObject* W, const FVector& P) { return W && WRUnderwater::WaterAt(W->GetWorld(), P) != nullptr; }
	/** A body in the water (its middle under the surface and a volume there). */
	bool OcBodyInWater(const AActor* A)
	{
		const ACharacter* C = Cast<ACharacter>(A);
		if (C && C->GetCharacterMovement() && C->GetCharacterMovement()->IsSwimming()) return true;
		return A && OcInWater(A, A->GetActorLocation());
	}
	/** Ocean creatures of one kind near a point, wild and alive. */
	template <typename F> void OcEachKind(const UWorld* W, FName Kind, const FVector& At, float Reach, F&& Fn)
	{
		for (const TWeakObjectPtr<UWROceanBody>& B : UWROceanBody::Live)
		{
			AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
			if (!C || C->GetWorld() != W || !C->IsAlive() || C->IsTamed() || (!Kind.IsNone() && C->CreatureId != Kind)) continue;
			if (FVector::Dist(C->GetActorLocation(), At) <= Reach) Fn(C, B.Get());
		}
	}
	void OcHit(AWRCreature* Self, AWRCharacterBase* V, float Damage, const FGameplayTag& Element, bool bUnblockable, bool bDot = false)
	{
		FWRHit Hit;
		Hit.Instigator = Self;
		Hit.Damage = Damage;
		Hit.bUnblockable = bUnblockable;
		Hit.bMelee = !bDot;
		Hit.bDamageOverTime = bDot;
		Hit.Origin = Self->GetActorLocation();
		Hit.Element = Element;
		UWRCombatLibrary::ApplyHit(V, Hit);
	}
	/** One food or fish stack in a bag (never gear). */
	bool OcFoodStack(const AWRCharacterBase* Who, FName& OutItem, int32& OutCount)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		if (!Inv) return false;
		for (const FWRItemStack& S : Inv->GetSlots())
		{
			if (S.ItemId.IsNone() || S.Count <= 0) continue;
			const FWRItemDef* D = FWRItemRegistry::Get().Find(S.ItemId);
			const FString Id = S.ItemId.ToString();
			if ((D && D->Category == TEXT("food")) || Id.Contains(TEXT("fish")) || Id == TEXT("raw_meat") || Id == TEXT("crab_meat"))
			{
				OutItem = S.ItemId;
				OutCount = FMath::Min(S.Count, FMath::RoundToInt(OcTn(TEXT("gullraider.snatchMax"), 5.f)));
				return true;
			}
		}
		return false;
	}
	FVector OcSite(FName Id, const FVector& Fallback)
	{
		const FWROceanData::FPlace* P = FWROceanData::Get().FindSite(Id);
		return P ? P->At : Fallback;
	}
}
using namespace WROceanCrAI;

// ================================================================================================ the body component

UWROceanBody::UWROceanBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWROceanBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWROceanBody, Flags);
	DOREPLIFETIME(UWROceanBody, Seed);
	DOREPLIFETIME(UWROceanBody, StolenItem);
	DOREPLIFETIME(UWROceanBody, PathTamer);
	DOREPLIFETIME(UWROceanBody, EggThief);
	DOREPLIFETIME(UWROceanBody, Holding);
}

UWROceanBody* UWROceanBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWROceanBody>() : nullptr;
}

bool UWROceanBody::IsOceanKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("ocean");
}

void UWROceanBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsOceanKind(Creature->CreatureId) || Get(Creature)) return;
	UWROceanBody* Body = NewObject<UWROceanBody>(Creature, TEXT("OceanBody"));
	Body->Seed = FMath::Rand();
	Body->Home = Creature->GetActorLocation();
	Body->RingPhase = FMath::FRandRange(0.f, 2.f * PI);
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
	// Swimmers hang at neutral buoyancy under the surface (they hold a depth and steer up and down in 3D); the Pearldiver,
	// the Kelpgrazer and the Driftback float.
	const FName K = Creature->CreatureId;
	if (UWRCharacterMovementComponent* M = Cast<UWRCharacterMovementComponent>(Creature->GetCharacterMovement()))
	{
		M->bHoldDepth = K == TEXT("reefjaw") || K == TEXT("tidecoil") || K == TEXT("wavecharger") || K == TEXT("glowray") || K == TEXT("ripplefin");
		if (M->bHoldDepth) M->MaxSwimSpeed = Creature->GetDef() ? Creature->GetDef()->SwimSpeed : M->MaxSwimSpeed;
	}
	Body->CruiseDepth = K == TEXT("reefjaw") ? FMath::FRandRange(300.f, 1500.f) : K == TEXT("glowray") ? FMath::FRandRange(300.f, 900.f)
		: K == TEXT("wavecharger") ? FMath::FRandRange(200.f, 800.f) : K == TEXT("ripplefin") ? FMath::FRandRange(250.f, 900.f) : K == TEXT("tidecoil") ? 100000.f : 0.f;
}

void UWROceanBody::Set(uint32 Flag, bool bOn)
{
	const uint32 Was = Flags;
	Flags = bOn ? (Flags | Flag) : (Flags & ~Flag);
	if (Was != Flags && GetNetMode() != NM_DedicatedServer) OnRep_Flags();
}

void UWROceanBody::OnRep_Flags()
{
}

void UWROceanBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWROceanBody>& B) { return !B.IsValid(); });
	Live.Add(this);
}

void UWROceanBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (AWRCreature* C = Cast<AWRCreature>(GetOwner()); C && HeldBody.IsValid()) FWROceanAI::Release(C, this);
	}
	Live.RemoveAll([this](const TWeakObjectPtr<UWROceanBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

void UWROceanBody::MulticastMoment_Implementation(FName Effect, float Scale, FName Sound)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FVector At = C->GetActorLocation() + C->GetActorForwardVector() * C->GetSimpleCollisionRadius();
	if (!Effect.IsNone()) WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
	if (!Sound.IsNone()) WRSound::PlayAt(C, Sound, At);
}

void UWROceanBody::BuildLook()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!Def || Def->ProcBody.IsNone() || Beast) return;
	// Old Barnacle and the Reef Tyrant reuse the bodies at their scale (creatures.json look beastScale).
	float Scale = 1.f;
	if (C->CreatureId == TEXT("reef_tyrant")) Scale = 2.3f;
	else if (C->CreatureId == TEXT("old_barnacle")) Scale = 2.5f;
	else if (C->CreatureId == TEXT("siren_eel")) Scale = 5.f;
	Beast = NewObject<UWROceanBeast>(C, TEXT("OceanBeast"));
	Beast->SetupAttachment(C->GetRootComponent());
	Beast->RegisterComponent();
	// The body's middle rides the capsule's middle; the crab and the birds stand on their feet.
	const float Down = Def->ProcBody == TEXT("crab") || Def->ProcBody == TEXT("gull") || Def->ProcBody == TEXT("puffin") ? C->GetSimpleCollisionHalfHeight() : 0.f;
	Beast->SetRelativeLocation(FVector(0.f, 0.f, -Down + (Def->ProcBody == TEXT("gull") || Def->ProcBody == TEXT("puffin") ? 26.f : 0.f)));
	Beast->Init(C, Def->ProcBody, Scale, Seed);
	// The capsule's placeholder shape and any rig go (the glow light, if any, stays).
	for (UActorComponent* Comp : C->GetComponents())
	{
		UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Comp);
		if (SM && SM != static_cast<UPrimitiveComponent*>(Beast)) SM->SetVisibility(false, false);
		if (USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(Comp)) Sk->SetVisibility(false, true);
	}
}

void UWROceanBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (C->HasAuthority()) FWROceanAI::TickBody(C, this, DeltaTime);
	if (GetNetMode() != NM_DedicatedServer && !Beast && LookTries < 60)
	{
		++LookTries;
		BuildLook();
	}
}

// ================================================================================================ holds

AWRCreature* FWROceanAI::HolderOf(const AActor* Victim)
{
	if (!Victim) return nullptr;
	for (const TWeakObjectPtr<UWROceanBody>& B : UWROceanBody::Live)
	{
		if (B.IsValid() && B->Holding == Victim) return Cast<AWRCreature>(B->GetOwner());
	}
	return nullptr;
}

namespace WROceanCrAI
{
	void OcStartHold(AWRCreature* Self, UWROceanBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds)
	{
		if (!Self || !Body || !Victim || !Victim->IsAlive() || FWROceanAI::HolderOf(Victim)) return;
		Body->HeldBody = Victim;
		Body->Holding = Victim;
		Body->HoldKind = Kind;
		Body->HoldUntil = OcNow(Self) + Seconds;
		Body->HoldTickAt = OcNow(Self) + 1.0;
		Body->FreeDamage = 0.f;
		Body->DragFrom = Victim->GetActorLocation();
		Body->Set(WROceanFlag::Gape, true);
		if (Victim->Status) Victim->Status->Stagger(Seconds + 0.1f);
		// No breath back while held (the drag down, the second jaw).
		if (UWRDiveComponent* Dive = UWRDiveComponent::Of(Victim)) Dive->HoldBreath(Seconds + 0.5f);
		Body->MulticastMoment(TEXT("hit.blood"), 1.3f, Kind == TEXT("drag") ? FName(TEXT("creature.reefjaw.signature")) : FName(TEXT("creature.tidecoil.signature")));
		UE_LOG(LogWildreach, Log, TEXT("OCEAN %s holds %s (%s, %.1f s)"), *Self->GetName(), *Victim->GetName(), *Kind.ToString(), Seconds);
	}
}

void FWROceanAI::Release(AWRCreature* Self, UWROceanBody* Body)
{
	AWRCharacterBase* V = Body->HeldBody.Get();
	Body->HeldBody = nullptr;
	Body->Holding = nullptr;
	Body->Set(WROceanFlag::Gape, false);
	if (V && V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
	if (V) UE_LOG(LogWildreach, Log, TEXT("OCEAN %s lets go of %s"), Self ? *Self->GetName() : TEXT("?"), *V->GetName());
}

void FWROceanAI::Mash(AWRCharacterBase* Victim)
{
	UWROceanBody* Body = UWROceanBody::Get(HolderOf(Victim));
	if (Body) Body->HoldUntil -= OcTn(TEXT("mash"), 0.15f);
}

bool FWROceanAI::WaterOnly(const AWRCreature* Self)
{
	if (!Self) return false;
	const FName K = Self->CreatureId;
	return K == TEXT("reefjaw") || K == TEXT("glowray") || K == TEXT("ripplefin") || K == TEXT("kelpgrazer") || (K == TEXT("wavecharger") && !Self->IsTamed());
}

// ================================================================================================ the body (server)

void FWROceanAI::TickBody(AWRCreature* Self, UWROceanBody* Body, float DeltaTime)
{
	const double T = OcNow(Self);
	const FName K = Self->CreatureId;
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	if (!Self->IsAlive())
	{
		if (Body->HeldBody.IsValid()) Release(Self, Body);
		return;
	}
	const FVector P = Self->GetActorLocation();

	// A hold: the drag 6 m down (the Reefjaw) or to its hole (the Tidecoil); ticks through ApplyHit; freed by time,
	// mashing, an ally's blows on the holder (the Tidecoil: any ally's hit, fire or a shock).
	if (AWRCharacterBase* V = Body->HeldBody.Get())
	{
		const bool bDrag = Body->HoldKind == TEXT("drag");
		const float Free = bDrag ? OcTn(TEXT("reefjaw.freeDamage"), 100.f) : 1.f;
		if (!V->IsAlive() || T >= Body->HoldUntil || Body->FreeDamage >= Free) Release(Self, Body);
		else
		{
			const float Total = FMath::Max(0.5f, bDrag ? OcTn(TEXT("reefjaw.dragSeconds"), 2.f) : OcTn(TEXT("tidecoil.holdSeconds"), 2.f));
			const float S = FMath::Clamp(1.f - float(Body->HoldUntil - T) / Total, 0.f, 1.f);
			FVector Want = bDrag ? Body->DragFrom - FVector(0, 0, OcTn(TEXT("reefjaw.dragDepth"), 600.f) * S) : FMath::Lerp(Body->DragFrom, Body->Home, S);
			// Never through the floor.
			Want.Z = FMath::Max(Want.Z, OcFloor(Want, Want.Z - 1.f) + V->GetSimpleCollisionHalfHeight() + 20.f);
			V->SetActorLocation(FMath::VInterpTo(V->GetActorLocation(), Want, DeltaTime, 8.f), true);
			Self->SetActorLocation(FMath::VInterpTo(P, Want + (P - V->GetActorLocation()).GetSafeNormal() * (Self->GetSimpleCollisionRadius() + 60.f), DeltaTime, 6.f), true);
			if (T >= Body->HoldTickAt)
			{
				Body->HoldTickAt = T + 1.0;
				OcHit(Self, V, bDrag ? OcTn(TEXT("reefjaw.dragDps"), 15.f) : OcTn(TEXT("tidecoil.holdDps"), 12.f), WRTags::Element_Physical, true, true);
				if (V->Status) V->Status->Stagger(1.1f);
				if (UWRDiveComponent* Dive = UWRDiveComponent::Of(V)) Dive->HoldBreath(1.5f);
			}
		}
	}

	// Swimmers: in the water they steer in 3D inside their depth band (the default chase drives the flat; this adds the
	// up and down); out of it, the water-only ones head back to the sea.
	const FWRCreatureDef* Def = Self->GetDef();
	const bool bSwimmer = Def && Def->SwimSpeed > 0.f;
	if (bSwimmer && !Self->GetRider() && !Body->HeldBody.IsValid() && !Body->Has(WROceanFlag::Stranded))
	{
		const float Surface = OcSurface(Self, P);
		const float Floor = OcFloor(P, Surface - 10000.f);
		if (Move->IsSwimming())
		{
			float WantZ;
			const AActor* Tg = Self->GetAggroTarget();
			const float Half = Self->GetSimpleCollisionHalfHeight();
			if (K == TEXT("pearldiver") || K == TEXT("kelpgrazer") || K == TEXT("driftback")) WantZ = Surface - Half * 0.4f;   // floats
			else if (Tg && OcBodyInWater(Tg)) WantZ = Tg->GetActorLocation().Z;
			else if (Tg) WantZ = Surface - Half - 60.f;   // up under someone on the surface or the shore
			else WantZ = Surface - Body->CruiseDepth;
			WantZ = FMath::Clamp(WantZ, Floor + Half + 40.f, Surface - (K == TEXT("pearldiver") || K == TEXT("kelpgrazer") || K == TEXT("driftback") ? 0.f : Half * 0.6f));
			const float Dz = WantZ - P.Z;
			if (FMath::Abs(Dz) > 30.f) Self->AddMovementInput(FVector(0.f, 0.f, FMath::Sign(Dz)), FMath::Clamp(FMath::Abs(Dz) / 300.f, 0.15f, 1.f));
			// Never into the shallows on its own (the water-only ones): a wall of sand ahead turns it back.
			if (WaterOnly(Self) && !Body->Has(WROceanFlag::Jaw))
			{
				const FVector Ahead = P + Move->Velocity.GetSafeNormal2D() * (Self->GetSimpleCollisionRadius() + 300.f);
				if (OcFloor(Ahead, Surface - 10000.f) > Surface - Half * 1.2f)
				{
					Move->Velocity.X *= 0.3f;
					Move->Velocity.Y *= 0.3f;
					Self->AddMovementInput((Body->Home - P).GetSafeNormal2D(), 1.f);
				}
			}
		}
		else if (WaterOnly(Self) && (Move->IsMovingOnGround() || Move->IsFalling()))
		{
			// Beached (not stranded by a lunge): it thrashes back toward the deeper water.
			Self->AddMovementInput((Body->Home - P).GetSafeNormal2D(), 1.f);
		}
	}

	// The surf lunge's stranding: belly up on the wet sand 1.5 s (a lock point, x1.3 taken), then it thrashes back in.
	if (Body->Has(WROceanFlag::Stranded))
	{
		if (T >= Body->StrandedUntil)
		{
			Body->Set(WROceanFlag::Stranded, false);
			Self->LaunchCharacter((Body->Home - P).GetSafeNormal2D() * 900.f + FVector(0, 0, 250.f), true, true);
		}
		else Move->StopMovementImmediately();
	}
	// A King Tide's stranded grazers lie flat (WRTide's Low): they don't move.
	if (Body->bStrandedFlat) Move->StopMovementImmediately();

	// A Spinecrab's guard and its flip.
	if (Body->Has(WROceanFlag::Guard) && T >= Body->GuardUntil) { Body->Set(WROceanFlag::Guard, false); Body->GuardStance = 0.f; }
	if (Body->Has(WROceanFlag::Flipped))
	{
		Move->StopMovementImmediately();
		if (T >= Body->FlippedUntil) Body->Set(WROceanFlag::Flipped, false);
	}

	// A Tidecoil in its hole: only its head shows; out of it at low tide it slithers to another pool.
	if (K == TEXT("tidecoil") && !Body->HeldBody.IsValid())
	{
		const bool bLow = WRTide::Fullness(Self) < OcTn(TEXT("tidecoil.lowTide"), 0.3f);
		const bool bOut = !Self->GetAggroTarget() && bLow;
		Body->Set(WROceanFlag::InHole, !bOut && FVector::Dist(P, Body->Home) < 250.f);
		if (bOut && T >= Body->NextSlitherAt)
		{
			Body->NextSlitherAt = T + FMath::FRandRange(40.f, 90.f);
			const FVector2D R = FMath::RandPointInCircle(OcTn(TEXT("tidecoil.slither"), 1500.f));
			Body->Home = FVector(Body->Home.X + R.X, Body->Home.Y + R.Y, OcFloor(Body->Home + FVector(R, 0.f), Body->Home.Z) + 50.f);
			Self->SetHome(Body->Home);
		}
	}

	// A Gullraider with what it stole: off to the Gullery; held there 60 s, then it drops it in the colony (a bag anyone
	// can take, where you can see it).
	if (K == TEXT("gullraider") && Body->Has(WROceanFlag::Carrying))
	{
		const FVector Gullery = OcSite(TEXT("gullery"), Body->Home) + FVector(0, 0, 600.f);
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		Self->AddMovementInput((Gullery - P).GetSafeNormal(), 1.f);
		if (T - Body->StolenAt > OcTn(TEXT("gullraider.keepSeconds"), 60.f) && FVector::Dist(P, Gullery) < 2500.f)
		{
			TArray<FWRItemStack> Items;
			FWRItemStack S;
			S.ItemId = Body->StolenItem;
			S.Count = Body->StolenCount;
			Items.Add(S);
			AWRLootBag::Spill(Self->GetWorld(), FVector(Gullery.X, Gullery.Y, OcFloor(Gullery, Gullery.Z - 600.f) + 30.f), FString(), Items);
			Body->StolenItem = NAME_None;
			Body->StolenCount = 0;
			Body->Set(WROceanFlag::Carrying, false);
			Body->Set(WROceanFlag::Pouch, false);
		}
	}

	// A Ripplefin school balls up while a Reefjaw is near (the silver sphere that says a shark is about).
	if (K == TEXT("ripplefin"))
	{
		bool bShark = false;
		OcEachKind(Self->GetWorld(), TEXT("reefjaw"), P, OcTn(TEXT("ripplefin.sharkNear"), 2500.f), [&](AWRCreature*, UWROceanBody*) { bShark = true; });
		if (bShark) Body->BaitUntil = T + 6.0;
		Body->Set(WROceanFlag::BaitBall, T < Body->BaitUntil);
		if (Body->Has(WROceanFlag::BaitBall)) Move->Velocity *= 0.9f;
	}

	// A Reefjaw's path-magic calm under way: it hangs still under the Tide walker's hand; 4 s finishes it.
	if (!Body->PathTamer.IsEmpty())
	{
		AWRCharacter* Tamer = OcPlayerNamed(Self->GetWorld(), Body->PathTamer);
		FString Broke;
		const float Reach = Self->GetSimpleCollisionRadius() + OcTn(TEXT("reefjaw.pathMagicReach"), 400.f) + 100.f;
		if (!Tamer || Self->IsTamed()) Broke = TEXT("The calm breaks");
		else if (FVector::Dist(Tamer->GetActorLocation(), P) > Reach || FVector::Dist(Tamer->GetActorLocation(), Body->HandAt) > 200.f) Broke = TEXT("You moved: the calm breaks");
		else if (Tamer->IsSwinging() || Tamer->HasTag(WRTags::State_Dodging)) Broke = TEXT("The calm breaks");
		if (!Broke.IsEmpty())
		{
			Body->PathTamer.Empty();
			OcTell(Tamer, Broke);
		}
		else
		{
			Self->SetAggroTarget(nullptr);
			Move->StopMovementImmediately();
			if (T - Body->PathTameStart >= OcTn(TEXT("reefjaw.pathMagicSeconds"), 4.f))
			{
				Body->PathTamer.Empty();
				if (!WRTidePath::SpendFlow(Tamer, OcTn(TEXT("reefjaw.pathMagicFlow"), 30.f)))
				{
					OcTell(Tamer, FString::Printf(TEXT("You haven't the Flow (%.0f) to finish the calm"), OcTn(TEXT("reefjaw.pathMagicFlow"), 30.f)));
				}
				else
				{
					// The water goes still round it: under, and kept under; the tame goes on as knockout and feed.
					Self->AddTorpor(Self->MaxTorpor * 2.f);
					Self->Torpor = Self->MaxTorpor;
					WRFx::Broadcast(Self, TEXT("tide.cast"), P, FRotator::ZeroRotator, 2.f, TEXT("tide.cleanse"));
					OcTell(Tamer, TEXT("The water stills round the Reefjaw and it sinks, calm: feed it to finish the tame"));
					UE_LOG(LogWildreach, Log, TEXT("OCEAN path-magic calm done on %s by %s"), *Self->GetName(), *Tamer->GetName());
				}
			}
		}
	}

	// An egg eased out of a sleeping male's pouch (3 s): moving or the herd waking breaks it.
	if (!Body->EggThief.IsEmpty())
	{
		AWRCharacter* Thief = OcPlayerNamed(Self->GetWorld(), Body->EggThief);
		if (!Thief || !Thief->IsAlive() || FVector::Dist(Thief->GetActorLocation(), Body->HandAt) > 150.f || !Body->Has(WROceanFlag::Asleep) || Self->GetAggroTarget())
		{
			Body->EggThief.Empty();
			OcTell(Thief, TEXT("You let go of the egg"));
		}
		else if (T - Body->EggStart >= OcTn(TEXT("wavecharger.eggSeconds"), 3.f))
		{
			Body->EggThief.Empty();
			Body->Set(WROceanFlag::Pouch, false);
			if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Thief)) Inv->AddItem(TEXT("egg_wavecharger"), 1);
			OcTell(Thief, TEXT("You have a Wavecharger egg: hatch it warm and it's yours"));
			// Heard? Anyone not sneaking or swimming gives it away: the herd wakes and turns on the thief.
			const bool bQuiet = Thief->bIsCrouched || (Thief->GetCharacterMovement() && Thief->GetCharacterMovement()->IsSwimming());
			if (!bQuiet || FMath::FRand() < OcTn(TEXT("wavecharger.wakeChance"), 0.25f))
			{
				OcEachKind(Self->GetWorld(), TEXT("wavecharger"), P, OcTn(TEXT("wavecharger.herdReach"), 3000.f), [&](AWRCreature* C, UWROceanBody* B)
				{
					B->Set(WROceanFlag::Asleep, false);
					C->bGuardAggressive = true;
					C->SetAggroTarget(Thief);
				});
				Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.wavecharger.aggro"));
			}
			UE_LOG(LogWildreach, Log, TEXT("OCEAN %s stole a Wavecharger egg"), *Thief->GetName());
		}
	}

	// A tame Glowray: its light (the generic glow) and the hostile ones' bubble lines shown within 30 m; a tame Pearldiver
	// finds pearls in the clams near it and points out sunken caches.
	if (Self->IsTamed())
	{
		if (K == TEXT("glowray") && T >= Body->NextTrailAt)
		{
			Body->NextTrailAt = T + OcTn(TEXT("glowray.trailEvery"), 1.0f);
			for (const TWeakObjectPtr<UWROceanBody>& W : UWROceanBody::Live)
			{
				AWRCreature* C = W.IsValid() ? Cast<AWRCreature>(W->GetOwner()) : nullptr;
				if (!C || C == Self || !C->IsAlive() || C->IsTamed() || !C->GetDef() || C->GetDef()->Behavior != TEXT("aggressive")) continue;
				if (FVector::Dist(C->GetActorLocation(), P) > OcTn(TEXT("glowray.trailReach"), 3000.f) || !OcBodyInWater(C)) continue;
				WRFx::Broadcast(C, TEXT("ocean.bubble_trail"), C->GetActorLocation(), C->GetActorRotation(), 1.f, NAME_None, 1.2f);
			}
		}
		if (K == TEXT("pearldiver") && T >= Body->NextFindAt)
		{
			Body->NextFindAt = T + OcTn(TEXT("pearldiver.findEvery"), 90.f);
			AWRCharacter* Owner = OcPlayerNamed(Self->GetWorld(), Self->GetTameOwner());
			if (Owner && FVector::Dist(Owner->GetActorLocation(), P) < 4000.f)
			{
				bool bClam = false;
				for (const FWROceanData::FNode& N : FWROceanData::Get().Nodes)
				{
					if ((N.Item == TEXT("giant_clam") || N.Item == TEXT("oyster")) && FVector::Dist(N.At, P) < OcTn(TEXT("pearldiver.clamReach"), 2000.f)) { bClam = true; break; }
				}
				if (bClam && FMath::FRand() < OcTn(TEXT("pearldiver.pearlChance"), 0.5f))
				{
					if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Owner)) Inv->AddItem(TEXT("pearl"), 1);
					OcTell(Owner, TEXT("Your Pearldiver surfaces with a pearl from the clams"));
					Body->MulticastMoment(TEXT("ocean.splash"), 0.6f, TEXT("creature.pearldiver.signature"));
				}
				for (TActorIterator<AWRWorldCache> It(Self->GetWorld()); It; ++It)
				{
					if (FVector::Dist(It->GetActorLocation(), P) < OcTn(TEXT("pearldiver.cacheReach"), 4000.f) && WROcean::Contains(It->GetActorLocation()))
					{
						WRFx::Broadcast(Self, TEXT("ocean.bubble_column"), It->GetActorLocation(), FRotator::ZeroRotator, 0.8f, TEXT("creature.pearldiver.call"), 4.f);
						OcTell(Owner, TEXT("Your Pearldiver dives and circles something sunken: bubbles mark it"));
						break;
					}
				}
			}
		}
	}
}

// ================================================================================================ the wild brain

bool FWROceanAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return false;
	const double T = OcNow(Self);
	const FName K = Self->CreatureId;
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	const FVector P = Self->GetActorLocation();
	// No row under way: the rows' own looks are over.
	if (AI->RowPhase == 0)
	{
		Body->Set(WROceanFlag::Jaw, false);
		if (!Body->HeldBody.IsValid()) Body->Set(WROceanFlag::Gape, false);
		Body->Set(WROceanFlag::Cocked, false);
		Body->Set(WROceanFlag::Bands, false);
		Body->Set(WROceanFlag::Flare, false);
	}
	if (Body->HeldBody.IsValid() || Body->Has(WROceanFlag::Stranded) || Body->Has(WROceanFlag::Flipped) || !Body->PathTamer.IsEmpty() || Body->bStrandedFlat) return true;
	if (K == TEXT("gullraider") && Body->Has(WROceanFlag::Carrying)) return true;   // off home with what it took
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	const bool bNight = OcNight(Self);

	// A Spinecrab with nothing to do buries itself in the sand: only its eyestalks show; it bursts out at what comes within 5 m.
	if (K == TEXT("spinecrab"))
	{
		if (!Target && Move->IsMovingOnGround() && !OcBodyInWater(Self))
		{
			if (!Body->Has(WROceanFlag::Buried)) Body->MulticastMoment(TEXT("land.dust"), 1.f, TEXT("creature.spinecrab.bury"));
			Body->Set(WROceanFlag::Buried, true);
			Move->StopMovementImmediately();
			for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), P) < OcTn(TEXT("spinecrab.burstReach"), 500.f))
				{
					Self->SetAggroTarget(*It);
					AI->StartRow(Self, *It, TEXT("sand_burst"), false);
					break;
				}
			}
			return true;
		}
		if (Body->Has(WROceanFlag::Buried) && AI->RowId != TEXT("sand_burst")) Body->Set(WROceanFlag::Buried, false);
	}

	// A Tidecoil waits in its hole: it strikes what comes within 4 m of it and never chases far from it.
	if (K == TEXT("tidecoil") && Body->Has(WROceanFlag::InHole))
	{
		Move->StopMovementImmediately();
		if (Target && FVector::Dist(Target->GetActorLocation(), Body->Home) > OcTn(TEXT("tidecoil.holeReach"), 500.f) + 300.f)
		{
			Self->SetAggroTarget(nullptr);
			Target = nullptr;
		}
		if (Target)
		{
			Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), (Target->GetActorLocation() - P).Rotation(), DeltaSeconds, 5.f));
			return false;   // the row fight picks hole_strike / second_jaw / shock_pulse (RowAllowed keeps it to those)
		}
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && !UWROceanBody::Get(*It) && FVector::Dist(It->GetActorLocation(), Body->Home) < OcTn(TEXT("tidecoil.holeReach"), 500.f))
			{
				Self->SetAggroTarget(*It);
				break;
			}
		}
		return true;
	}

	// The water-only ones never come out after someone on the land: they circle the shallows' edge facing them, and a
	// Reefjaw takes the surf lunge (RowAllowed) when they stand on the wet sand.
	if (Target && WaterOnly(Self) && !OcBodyInWater(Target))
	{
		const float Surface = OcSurface(Self, Target->GetActorLocation());
		const bool bWetSand = Target->GetActorLocation().Z - Target->GetSimpleCollisionHalfHeight() < Surface + OcTn(TEXT("reefjaw.wetSandAbove"), 150.f);
		if (K == TEXT("reefjaw") && bWetSand && FVector::Dist2D(Target->GetActorLocation(), P) < OcTn(TEXT("reefjaw.surfReach"), 900.f))
		{
			if (AI->RowReadyAt.FindRef(TEXT("surf_lunge")) <= T && AI->RowPhase == 0) AI->StartRow(Self, Target, TEXT("surf_lunge"), false);
			return AI->RowPhase != 0 ? false : true;
		}
		if (FVector::Dist2D(Target->GetActorLocation(), P) > OcTn(TEXT("loseOnLand"), 4000.f)) { Self->SetAggroTarget(nullptr); return false; }
		Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), (Target->GetActorLocation() - P).GetSafeNormal2D().Rotation(), DeltaSeconds, 3.f));
		Self->AddMovementInput((Target->GetActorLocation() - P).GetSafeNormal2D(), 0.3f);
		return true;
	}
	if (Target)
	{
		// A 120 m leash from home.
		if (FVector::Dist(P, Body->Home) > OcTn(TEXT("leash"), 12000.f)) { Self->SetAggroTarget(nullptr); return false; }
		Body->Set(WROceanFlag::Asleep, false);
		return false;   // the row fight (TickRowFight) takes it from here
	}

	// Nothing to fight: each its own way.
	if (K == TEXT("wavecharger") || K == TEXT("pearldiver"))
	{
		// Night: the herd sleeps at the Kelp Cathedral with tails wrapped on the stipes (a male's pouch full of eggs); the
		// otters wrap themselves in kelp and sleep on the surface.
		if (bNight)
		{
			const FVector Bed = K == TEXT("wavecharger") ? OcSite(TEXT("kelp_cathedral"), Body->Home) : Body->Home;
			const FVector D = Bed - P;
			if (K == TEXT("wavecharger") && D.Size2D() > 1200.f && !Body->Has(WROceanFlag::Asleep))
			{
				Self->AddMovementInput(D.GetSafeNormal2D(), 0.6f);
				return true;
			}
			if (!Body->Has(WROceanFlag::Asleep))
			{
				Body->Set(WROceanFlag::Asleep, true);
				if (K == TEXT("wavecharger") && (Body->Seed & 1)) Body->Set(WROceanFlag::Pouch, true);
			}
			Move->StopMovementImmediately();
			return true;
		}
		if (Body->Has(WROceanFlag::Asleep)) { Body->Set(WROceanFlag::Asleep, false); Body->Set(WROceanFlag::Pouch, false); }
	}
	if (K == TEXT("driftback") && Body->Has(WROceanFlag::Hauled))
	{
		// Hauled out on the Turtle Beach to dig: it stays till dawn, then heads back to the sea.
		if (!bNight) { Body->Set(WROceanFlag::Hauled, false); Self->SetHome(Body->Home); return false; }
		Move->StopMovementImmediately();
		return true;
	}
	if (K == TEXT("glowray") && Body->Has(WROceanFlag::Ring))
	{
		// A ring over the reef at night, its lights pulsing in sync with the others (the look reads the shared clock).
		if (!bNight) { Body->Set(WROceanFlag::Ring, false); return false; }
		Body->RingPhase += DeltaSeconds * OcTn(TEXT("glowray.ringSpeed"), 0.25f);
		const float R = OcTn(TEXT("glowray.ringRadius"), 900.f);
		const FVector Want = Body->Home + FVector(FMath::Cos(Body->RingPhase) * R, FMath::Sin(Body->RingPhase) * R, 0.f);
		const FVector D = Want - P;
		Self->AddMovementInput(D.GetSafeNormal(), 0.5f);
		if (D.Size2D() > 100.f) Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), D.GetSafeNormal2D().Rotation(), DeltaSeconds, 2.f));
		return true;
	}
	if (K == TEXT("ripplefin"))
	{
		if (Body->Has(WROceanFlag::BaitBall)) { Move->StopMovementImmediately(); return true; }
		// The school wanders a slow loop through the kelp round where it was put.
		Body->RingPhase += DeltaSeconds * 0.08f;
		const FVector Want = Body->Home + FVector(FMath::Cos(Body->RingPhase) * 1500.f, FMath::Sin(Body->RingPhase * 1.3f) * 1500.f, 0.f);
		Self->AddMovementInput((Want - P).GetSafeNormal2D(), 0.6f);
		return true;
	}
	// Swimmers with nothing to do cruise round home; crabs and birds keep the default life.
	if (WaterOnly(Self) && Move->IsSwimming())
	{
		Body->RingPhase += DeltaSeconds * (K == TEXT("reefjaw") ? 0.05f : 0.08f);
		const float R = K == TEXT("reefjaw") ? 3000.f : 1500.f;
		const FVector Want = Body->Home + FVector(FMath::Cos(Body->RingPhase) * R, FMath::Sin(Body->RingPhase) * R, 0.f);
		Self->AddMovementInput((Want - P).GetSafeNormal2D(), K == TEXT("kelpgrazer") ? 0.3f : 0.5f);
		return false;   // the default still looks for prey and targets
	}
	return false;
}

bool FWROceanAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return false;
	Body->Set(WROceanFlag::Asleep, false);
	Body->Set(WROceanFlag::Ring, false);
	Body->Set(WROceanFlag::Hauled, false);
	if (Body->HeldBody.IsValid() || Body->Has(WROceanFlag::Stranded) || Body->Has(WROceanFlag::Flipped)) return true;
	return false;
}

// ================================================================================================ rows

bool FWROceanAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return true;
	const FName Id = Row.Id;
	const FName K = Self->CreatureId;
	if (Body->Has(WROceanFlag::Flipped) || Body->Has(WROceanFlag::Stranded)) return false;
	const bool bTgWater = Target && OcBodyInWater(Target);
	// The Reefjaw: the breach only at someone at the surface; the surf lunge only on the wet sand; nothing else at someone
	// out of the water.
	if (K == TEXT("reefjaw"))
	{
		if (Id == TEXT("surf_lunge")) return Target && !bTgWater;
		if (Id == TEXT("breach_strike"))
		{
			if (!Target || !bTgWater) return false;
			const float Surface = OcSurface(Self, Target->GetActorLocation());
			return Target->GetActorLocation().Z > Surface - OcTn(TEXT("reefjaw.breachNearSurface"), 200.f) && Self->GetActorLocation().Z < Surface - 300.f;
		}
		return bTgWater;
	}
	// The Tidecoil in its hole: the hole's strikes, the jaw, the pulse; out of it all but the hole strike.
	if (K == TEXT("tidecoil"))
	{
		if (Id == TEXT("hole_strike")) return Body->Has(WROceanFlag::InHole);
		if (Id == TEXT("coil_whip")) return !Body->Has(WROceanFlag::InHole);
		return true;
	}
	if (K == TEXT("spinecrab"))
	{
		if (Id == TEXT("sand_burst")) return Body->Has(WROceanFlag::Buried);
		if (Body->Has(WROceanFlag::Buried)) return false;
		// The guard: when you wind up a heavy (or swing) at it from the front.
		if (Id == TEXT("shell_guard")) return Target && Target->IsSwinging() && FVector::DotProduct(Self->GetActorForwardVector(), (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D()) > 0.3f;
		return true;
	}
	if (K == TEXT("gullraider"))
	{
		if (Id == TEXT("snatch")) { FName Item; int32 N = 0; return Target && Target->IsA<AWRCharacter>() && OcFoodStack(Target, Item, N) && !Body->Has(WROceanFlag::Carrying); }
		if (Id == TEXT("mob_call")) return !Body->bMobCalled;
		return true;
	}
	// The Glowray's flare, the herd click: as the rows say.
	if (K == TEXT("wavecharger") && Id == TEXT("herd_click")) return false;   // it plays from Damaged (the herd turning)
	return true;
}

void FWROceanAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return;
	const FName Id = Row.Id;
	if (Id == TEXT("slingshot_bite") || Id == TEXT("slingshot_read"))
	{
		// Gill slits flare, the jaw drops and unhinges (the gape), the teeth glint.
		Body->Set(WROceanFlag::Gape, true);
	}
	else if (Id == TEXT("thrash_drag") || Id == TEXT("second_jaw")) Body->Set(WROceanFlag::Gape, true);
	else if (Id == TEXT("ram")) Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.reefjaw.thrum"));
	else if (Id == TEXT("breach_strike") && Target)
	{
		// A bubble column rises under the target and a low thrum builds (1 s): swim off it.
		Body->ColumnSpot = Target->GetActorLocation();
		Body->ColumnAt = OcNow(Self);
		WRFx::Broadcast(Self, TEXT("ocean.bubble_column"), Body->ColumnSpot, FRotator::ZeroRotator, 1.4f, TEXT("creature.reefjaw.thrum"), Row.Windup + Row.Hold);
		// It drops away below to come up through it.
		const float Floor = OcFloor(Body->ColumnSpot, Body->ColumnSpot.Z - 1200.f);
		const FVector Below(Body->ColumnSpot.X, Body->ColumnSpot.Y, FMath::Max(Floor + Self->GetSimpleCollisionHalfHeight() + 50.f, Body->ColumnSpot.Z - 900.f));
		Self->SetActorLocation(FMath::Lerp(Self->GetActorLocation(), Below, 0.5f), true);
	}
	else if (Id == TEXT("surf_lunge"))
	{
		// A wave lifts it and its fin cuts the surf toward the shore.
		Body->MulticastMoment(TEXT("ocean.splash"), 1.6f, TEXT("creature.reefjaw.surf"));
	}
	else if (Id == TEXT("shock_pulse"))
	{
		// Blue-white bands race down its body, a rising crackle.
		Body->Set(WROceanFlag::Bands, true);
		Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.tidecoil.crackle"));
	}
	else if (Id == TEXT("club_snap")) { Body->Set(WROceanFlag::Cocked, true); Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.spinecrab.ratchet")); }
	else if (Id == TEXT("shell_guard"))
	{
		Body->Set(WROceanFlag::Guard, true);
		Body->GuardUntil = OcNow(Self) + OcTn(TEXT("spinecrab.guardSeconds"), 3.f);
		Body->GuardStance = 0.f;
	}
	else if (Id == TEXT("sand_burst")) Body->MulticastMoment(TEXT("land.dust"), 1.4f, TEXT("creature.spinecrab.burst_tell"));
	else if (Id == TEXT("side_charge")) Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.spinecrab.drum"));
	else if (Id == TEXT("snatch") || Id == TEXT("mob_call")) Body->Set(WROceanFlag::Pouch, true);
	else if (Id == TEXT("snout_snap")) Body->Set(WROceanFlag::Jaw, true);
	else if (Id == TEXT("light_flare")) Body->Set(WROceanFlag::Flare, true);
	else if (Id == TEXT("barb_sting")) Body->Set(WROceanFlag::Cocked, true);
}

bool FWROceanAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return false;
	const FName Id = Row.Id;
	if (Id == TEXT("slingshot_bite") || Id == TEXT("slingshot_read") || Id == TEXT("thrash_drag"))
	{
		// The whole jaw shoots out 120 cm ahead of the head (the bite's reach is in the row).
		Body->Set(WROceanFlag::Jaw, true);
		return false;
	}
	if (Id == TEXT("breach_strike"))
	{
		// Up through the column: everything within its reach of the column is struck, and it rises clear of the water.
		const FVector Spot = Body->ColumnSpot;
		Self->SetActorLocation(FVector(Spot.X, Spot.Y, Self->GetActorLocation().Z), true);
		Self->LaunchCharacter(FVector(0.f, 0.f, OcTn(TEXT("reefjaw.breachSpeed"), 1600.f)), true, true);
		WRFx::Broadcast(Self, TEXT("ocean.splash"), FVector(Spot.X, Spot.Y, OcSurface(Self, Spot)), FRotator::ZeroRotator, 2.5f, TEXT("creature.reefjaw.breach"));
		const float R = Row.Reach > 0.f ? Row.Reach : 260.f;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Spot) < R + It->GetSimpleCollisionRadius() && FMath::Abs(It->GetActorLocation().Z - Spot.Z) < 500.f) AI->RowHit(Self, *It);
		}
		return true;
	}
	if (Id == TEXT("surf_lunge") && Target)
	{
		// Out of the surf at them: it lands on the wet sand and lies stranded (the lock point) for 1.5 s.
		const FVector To = Target->GetActorLocation();
		Body->Set(WROceanFlag::Jaw, true);
		Self->LaunchCharacter((To - Self->GetActorLocation()).GetSafeNormal2D() * OcTn(TEXT("reefjaw.surfSpeed"), 1300.f) + FVector(0, 0, 350.f), true, true);
		AI->RowDir = (To - Self->GetActorLocation()).GetSafeNormal2D();
		Body->Set(WROceanFlag::Stranded, true);
		Body->StrandedUntil = OcNow(Self) + 0.6 + OcTn(TEXT("reefjaw.strandedSeconds"), 1.5f);
		return false;   // the lunge's hit is the row's own
	}
	if (Id == TEXT("shock_pulse"))
	{
		// A pulse through the water (4 m; 2 m out of it) that hits everything in that water, its own kind too. Grounded:
		// immune (out of the water and on the ground when it's in the water).
		const bool bWet = OcBodyInWater(Self);
		const float R = bWet ? (Row.Radius > 0.f ? Row.Radius : 400.f) : OcTn(TEXT("tidecoil.pulseOnLand"), 200.f);
		WRFx::Broadcast(Self, TEXT("ocean.shock_ring"), Self->GetActorLocation(), FRotator::ZeroRotator, R / 100.f, TEXT("creature.tidecoil.pulse"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive() || FVector::Dist(V->GetActorLocation(), Self->GetActorLocation()) > R + V->GetSimpleCollisionRadius()) continue;
			if (OcBodyInWater(V) != bWet) continue;
			if (bWet && !OcBodyInWater(V) && V->GetCharacterMovement() && V->GetCharacterMovement()->IsMovingOnGround()) continue;
			AI->RowHit(Self, V);
		}
		Body->Set(WROceanFlag::Bands, false);
		return true;
	}
	if (Id == TEXT("sand_burst"))
	{
		Body->Set(WROceanFlag::Buried, false);
		WRFx::Broadcast(Self, TEXT("land.dust"), Self->GetActorLocation(), FRotator::ZeroRotator, 2.5f, TEXT("creature.spinecrab.burst"));
		const float R = Row.Reach > 0.f ? Row.Reach : 300.f;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < R + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		return true;
	}
	if (Id == TEXT("mob_call"))
	{
		// Every Gullraider within 50 m comes (once a fight).
		Body->bMobCalled = true;
		Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.gullraider.mob"));
		// Wax in your ears (beeswax plugs): the call doesn't reach you, so nothing comes for you.
		if (Target && UWRInventoryComponent::GearStatFor(Target, TEXT("passMobCall")) > 0.f) { Body->Set(WROceanFlag::Pouch, false); return true; }
		OcEachKind(Self->GetWorld(), TEXT("gullraider"), Self->GetActorLocation(), OcTn(TEXT("gullraider.mobReach"), 5000.f), [&](AWRCreature* C, UWROceanBody*)
		{
			if (C != Self && Target) C->SetAggroTarget(Target);
		});
		Body->Set(WROceanFlag::Pouch, false);
		return true;
	}
	if (Id == TEXT("light_flare"))
	{
		const float R = Row.Radius > 0.f ? Row.Radius : 400.f;
		WRFx::Broadcast(Self, TEXT("ocean.glow_flare"), Self->GetActorLocation(), FRotator::ZeroRotator, R / 100.f, TEXT("creature.glowray.flare"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < R + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		// Then it glides away.
		if (Target) Self->StartFleeing(Target, TEXT("flare"));
		return true;
	}
	return false;
}

void FWROceanAI::TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds)
{
}

void FWROceanAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	const double T = OcNow(Self);
	if (RowId == TEXT("club_snap"))
	{
		// The cavitation pop 0.06 s behind the strike: a block takes both; under water it hits all within 1.5 m.
		Body->Set(WROceanFlag::Cocked, false);
		const FVector At = Self->GetActorLocation() + Self->GetActorForwardVector() * (Self->GetSimpleCollisionRadius() + 120.f);
		WRFx::Broadcast(Self, TEXT("ocean.cavitation"), At, FRotator::ZeroRotator, 1.f, TEXT("creature.spinecrab.pop"));
		const float Pop = Self->Weapon.FlatDamage * OcTn(TEXT("spinecrab.popDmg"), 0.4f);
		if (Result != EWRHitResult::Dodged && Result != EWRHitResult::Parried) OcHit(Self, Victim, Pop, WRTags::Element_Physical, false);
		if (OcBodyInWater(Self))
		{
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It != Self && *It != Victim && It->IsAlive() && FVector::Dist(It->GetActorLocation(), At) < OcTn(TEXT("spinecrab.popReach"), 150.f) + It->GetSimpleCollisionRadius()) OcHit(Self, *It, Pop, WRTags::Element_Physical, false);
			}
		}
		return;
	}
	if (!bLanded) return;
	if (RowId == TEXT("thrash_drag")) OcStartHold(Self, Body, Victim, TEXT("drag"), OcTn(TEXT("reefjaw.dragSeconds"), 2.f));
	else if (RowId == TEXT("second_jaw")) OcStartHold(Self, Body, Victim, TEXT("jaw"), OcTn(TEXT("tidecoil.holdSeconds"), 2.f));
	else if (RowId == TEXT("snatch"))
	{
		// One stack of food or fish (5 at most), off to the Gullery.
		FName Item;
		int32 N = 0;
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Victim);
		if (Inv && OcFoodStack(Victim, Item, N))
		{
			N = Inv->RemoveItem(Item, N);
			Body->StolenItem = Item;
			Body->StolenCount = N;
			Body->StolenFrom = OcName(Victim);
			Body->StolenAt = T;
			Body->Set(WROceanFlag::Carrying, true);
			Self->SetAggroTarget(nullptr);
			const FWRItemDef* D = FWRItemRegistry::Get().Find(Item);
			OcTell(Victim, FString::Printf(TEXT("A Gullraider took your %s (%d): bring it down within a minute, or find it at the Gullery"), D ? *D->Name : *Item.ToString(), N));
			Body->MulticastMoment(TEXT("sky.feathers"), 1.f, TEXT("creature.gullraider.signature"));
		}
	}
	else if (RowId == TEXT("barb_sting")) Body->Set(WROceanFlag::Cocked, false);
	else if (RowId == TEXT("stone_smash") || RowId == TEXT("panic_bump") || RowId == TEXT("peck"))
	{
		// Then it dives and flees.
		Self->StartFleeing(Victim, TEXT("cornered"));
	}
}

float FWROceanAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return Damage;
	const FVector From = Hit.Origin.IsZero() && Hit.Instigator ? Hit.Instigator->GetActorLocation() : Hit.Origin;
	const bool bFront = FVector::DotProduct(Self->GetActorForwardVector(), (From - Self->GetActorLocation()).GetSafeNormal2D()) > 0.3f;
	if (Body->Has(WROceanFlag::Guard) && bFront) Damage *= OcTn(TEXT("spinecrab.guardTaken"), 0.3f);
	if (Body->Has(WROceanFlag::Flipped)) Damage *= OcTn(TEXT("spinecrab.flippedTaken"), 1.3f);
	if (Body->Has(WROceanFlag::Stranded)) Damage *= OcTn(TEXT("reefjaw.strandedTaken"), 1.3f);
	if (Body->bPoolBound) Damage *= OcTn(TEXT("poolBoundTaken"), 1.3f);
	return Damage;
}

void FWROceanAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return;
	const FName K = Self->CreatureId;
	const float Stance = Hit.StanceDamage >= 0.f ? Hit.StanceDamage : Hit.PoiseDamage;
	// A blow on a holder from anyone but the held frees them (the Reefjaw: 100 on it; the Tidecoil: any hit, fire, a shock).
	if (Body->HeldBody.IsValid() && Hit.Instigator != Body->HeldBody.Get()) Body->FreeDamage += Damage + (Hit.Element == WRTags::Element_Fire || Hit.Element == WRTags::Element_Lightning ? 1000.f : 0.f);
	// A guarded Spinecrab broken by 60 stance: on its back 2 s.
	if (Body->Has(WROceanFlag::Guard))
	{
		Body->GuardStance += Stance;
		if (Body->GuardStance >= OcTn(TEXT("spinecrab.guardBreak"), 60.f))
		{
			Body->Set(WROceanFlag::Guard, false);
			Body->Set(WROceanFlag::Flipped, true);
			Body->FlippedUntil = OcNow(Self) + OcTn(TEXT("spinecrab.flippedSeconds"), 2.f);
			Body->MulticastMoment(TEXT("hit.dust"), 1.2f, TEXT("creature.spinecrab.flip"));
			if (AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController())) AI->EndRow(Self, true);
		}
	}
	if (Body->Has(WROceanFlag::Buried)) Body->Set(WROceanFlag::Buried, false);
	// Woken: a sleeping herd, a hauled Driftback, a ring: the neutral ones defend each other.
	Body->Set(WROceanFlag::Asleep, false);
	if (Hit.Instigator && (K == TEXT("wavecharger") || K == TEXT("driftback") || K == TEXT("glowray")))
	{
		const float Reach = K == TEXT("wavecharger") ? OcTn(TEXT("wavecharger.herdReach"), 3000.f) : 2000.f;
		bool bClicked = false;
		OcEachKind(Self->GetWorld(), K, Self->GetActorLocation(), Reach, [&](AWRCreature* C, UWROceanBody* B)
		{
			B->Set(WROceanFlag::Asleep, false);
			B->Set(WROceanFlag::Ring, false);
			if (K != TEXT("glowray")) { C->bGuardAggressive = true; C->SetAggroTarget(Hit.Instigator); }
			bClicked = true;
		});
		if (K == TEXT("wavecharger") && bClicked && OcNow(Self) >= Body->NextVoiceAt)
		{
			Body->NextVoiceAt = OcNow(Self) + 6.0;
			Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.wavecharger.click"));
		}
		if (K == TEXT("glowray") && Hit.Instigator) Self->StartFleeing(Hit.Instigator, TEXT("hurt"));
	}
	if (!Body->EggThief.IsEmpty()) Body->EggThief.Empty();
	if (!Body->PathTamer.IsEmpty() && Hit.Instigator && OcName(Hit.Instigator) != Body->PathTamer) Body->PathTamer.Empty();
}

void FWROceanAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWROceanBody* Body = UWROceanBody::Get(Self);
	if (!Body) return;
	if (Body->HeldBody.IsValid()) Release(Self, Body);
	// A Gullraider brought down within 60 s drops what it stole where it falls.
	if (Body->StolenCount > 0 && !Body->StolenItem.IsNone())
	{
		TArray<FWRItemStack> Items;
		FWRItemStack S;
		S.ItemId = Body->StolenItem;
		S.Count = Body->StolenCount;
		Items.Add(S);
		float Z = Self->GetActorLocation().Z;
		Z = FMath::Min(Z, OcFloor(Self->GetActorLocation(), Z) + 30.f);
		AWRLootBag::Spill(Self->GetWorld(), FVector(Self->GetActorLocation().X, Self->GetActorLocation().Y, Z), Body->StolenFrom, Items);
		Body->StolenItem = NAME_None;
		Body->StolenCount = 0;
	}
	Body->Flags &= ~(WROceanFlag::Jaw | WROceanFlag::Gape | WROceanFlag::Bands | WROceanFlag::Flare | WROceanFlag::Carrying | WROceanFlag::Buried | WROceanFlag::Guard);
	if (UWROceanLife* L = UWROceanLife::Get(Self)) L->NoteDeath(Self, Killer);
}

// ================================================================================================ path magic, the egg

bool FWROceanAI::PathTamePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb)
{
	const UWROceanBody* B = UWROceanBody::Get(Self);
	const FWRCreatureDef* Def = Self ? Self->GetDef() : nullptr;
	if (!B || !Def || !Who || Self->CreatureId != TEXT("reefjaw") || Self->IsTamed() || !Self->IsAlive() || !Def->CanTameBy(TEXT("path_magic"))) return false;
	const FString Me = OcName(Who);
	if (!B->PathTamer.IsEmpty())
	{
		if (B->PathTamer != Me) return false;
		OutVerb = TEXT("Hold still: the water is going calm round it");
		return true;
	}
	// Knocked out, or soaked under 30%; you in the water with it and 10 Tide points; the Flow to finish.
	const UWRStatusComponent* St = Self->Status;
	const bool bSoaked = St && St->HasStatus(WRTags::Status_Soak);
	if (!Self->IsUnconscious() && !(bSoaked && Self->GetHealthFraction() < OcTn(TEXT("reefjaw.pathMagicBelow"), 0.3f))) return false;
	if (WRTidePath::PointsIn(Who) < FMath::RoundToInt(OcTn(TEXT("reefjaw.pathMagicPoints"), 10.f)) || !OcBodyInWater(Who)) return false;
	if (!WRTidePath::HasFlow(Who, OcTn(TEXT("reefjaw.pathMagicFlow"), 30.f))) return false;
	OutVerb = FString::Printf(TEXT("Hold: calm the water round it (%.0f s, %.0f Flow)"), OcTn(TEXT("reefjaw.pathMagicSeconds"), 4.f), OcTn(TEXT("reefjaw.pathMagicFlow"), 30.f));
	return true;
}

bool FWROceanAI::TryPathTame(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult)
{
	UWROceanBody* B = UWROceanBody::Get(Wild);
	AWRCharacter* Who = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	FString Verb;
	if (!B || !Who || !Wild->HasAuthority() || !PathTamePrompt(Wild, Who, Verb)) return false;
	const FString Me = OcName(Who);
	if (B->PathTamer == Me) { OutResult = Verb; return true; }
	if (FVector::Dist(Who->GetActorLocation(), Wild->GetActorLocation()) > Wild->GetSimpleCollisionRadius() + OcTn(TEXT("reefjaw.pathMagicReach"), 400.f))
	{
		OutResult = TEXT("Get your hand on its snout");
		return true;
	}
	B->PathTamer = Me;
	B->PathTameStart = OcNow(Wild);
	B->HandAt = Who->GetActorLocation();
	Wild->SetAggroTarget(nullptr);
	WRFx::BroadcastOn(Who, TEXT("tide.cast"), TEXT("hand"), 1.2f, TEXT("tide.cast"), OcTn(TEXT("reefjaw.pathMagicSeconds"), 4.f));
	OutResult = FString::Printf(TEXT("Hold E: calm the water round it (%.0f s)"), OcTn(TEXT("reefjaw.pathMagicSeconds"), 4.f));
	return true;
}

void FWROceanAI::StopPathTame(AWRPlayerController* Player)
{
	const FString Me = Player && Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (Me.IsEmpty() || !Player->GetWorld()) return;
	for (const TWeakObjectPtr<UWROceanBody>& W : UWROceanBody::Live)
	{
		UWROceanBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C) continue;
		if (B->PathTamer == Me)
		{
			const double Held = OcNow(C) - B->PathTameStart;
			B->PathTamer.Empty();
			Player->ClientNotify(Held < 0.35 ? C->Feed(Player) : FString(TEXT("You let go: the calm fades")));
			return;
		}
		if (B->EggThief == Me)
		{
			B->EggThief.Empty();
			Player->ClientNotify(TEXT("You let go of the egg"));
			return;
		}
	}
}

bool FWROceanAI::PathTameNear(const APlayerController* Player)
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const FString Me = Player && Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (Me.IsEmpty() || !Pawn) return false;
	for (const TWeakObjectPtr<UWROceanBody>& W : UWROceanBody::Live)
	{
		const UWROceanBody* B = W.Get();
		const AActor* C = B ? B->GetOwner() : nullptr;
		if (C && (B->PathTamer == Me || B->EggThief == Me) && FVector::Dist(C->GetActorLocation(), Pawn->GetActorLocation()) < 1500.f) return true;
	}
	return false;
}

bool FWROceanAI::EggPrompt(const AWRCreature* Male, const AActor* Who, FString& OutVerb)
{
	const UWROceanBody* B = UWROceanBody::Get(Male);
	if (!B || !Who || Male->CreatureId != TEXT("wavecharger") || Male->IsTamed() || !Male->IsAlive() || !B->Has(WROceanFlag::Asleep) || !B->Has(WROceanFlag::Pouch)) return false;
	const FString Me = OcName(Who);
	if (!B->EggThief.IsEmpty())
	{
		if (B->EggThief != Me) return false;
		OutVerb = TEXT("Hold still: easing an egg out of its pouch");
		return true;
	}
	OutVerb = FString::Printf(TEXT("Hold: steal an egg from its pouch (%.0f s; sneak or swim, or the herd hears)"), OcTn(TEXT("wavecharger.eggSeconds"), 3.f));
	return true;
}

bool FWROceanAI::TryStealEgg(AWRCreature* Male, AWRPlayerController* Player, FString& OutResult)
{
	UWROceanBody* B = UWROceanBody::Get(Male);
	AWRCharacter* Who = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	FString Verb;
	if (!B || !Who || !Male->HasAuthority() || !EggPrompt(Male, Who, Verb)) return false;
	const FString Me = OcName(Who);
	if (B->EggThief == Me) { OutResult = Verb; return true; }
	if (FVector::Dist(Who->GetActorLocation(), Male->GetActorLocation()) > Male->GetSimpleCollisionRadius() + 300.f)
	{
		OutResult = TEXT("Get close to its pouch");
		return true;
	}
	B->EggThief = Me;
	B->EggStart = OcNow(Male);
	B->HandAt = Who->GetActorLocation();
	OutResult = FString::Printf(TEXT("Hold E: easing an egg out (%.0f s)"), OcTn(TEXT("wavecharger.eggSeconds"), 3.f));
	return true;
}

// ================================================================================================ the living sea

UWROceanLife* UWROceanLife::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWROceanLife>() : nullptr;
}

bool UWROceanLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

bool UWROceanLife::IsTickable() const
{
	const UWorld* W = GetWorld();
	return W && W->GetNetMode() != NM_Client && WROcean::Live();
}

TStatId UWROceanLife::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWROceanLife, STATGROUP_Tickables);
}

void UWROceanLife::Tick(float DeltaTime)
{
	Clock += DeltaTime;
	BeatClock += DeltaTime;
	if (Clock >= OcTn(TEXT("stepSeconds"), 5.f))
	{
		Clock = 0.f;
		StepPopulations();
	}
	if (BeatClock >= 3.f)
	{
		BeatClock = 0.f;
		StepBeats();
	}
}

AWRCreature* UWROceanLife::SpawnOne(FName Kind, const FVector& At, FName Zone)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return nullptr;
	AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), Kind, At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	if (!C) return nullptr;
	C->SetHome(At);
	if (UWROceanBody* B = UWROceanBody::Get(C))
	{
		B->Home = At;
		const bool bNight = OcNight(this);
		if (Kind == TEXT("tidecoil")) B->Set(WROceanFlag::InHole, true);
		if (Kind == TEXT("spinecrab") && !OcInWater(this, At)) B->Set(WROceanFlag::Buried, true);
		if (Kind == TEXT("glowray") && bNight) B->Set(WROceanFlag::Ring, true);
		if (Kind == TEXT("driftback") && bNight && !OcInWater(this, At)) B->Set(WROceanFlag::Hauled, true);
	}
	if (Def->Archetype == TEXT("flyer")) C->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	else if (OcInWater(this, At) && Def->SwimSpeed > 0.f) C->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
	Spawned.Add(C);
	return C;
}

bool UWROceanLife::FindSpot(FName Kind, FName Zone, const FVector& Near, const FVector& Facing, FVector& Out) const
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return false;
	const bool bNight = OcNight(this);
	const float Half = Def->HalfHeight;
	// A night's special places: the herd at the Kelp Cathedral, the Driftbacks on the Turtle Beach, the rings over the reef.
	if (bNight && Kind == TEXT("wavecharger")) { const FVector S = OcSite(TEXT("kelp_cathedral"), FVector::ZeroVector); if (!S.IsZero() && FVector::Dist2D(S, Near) < 25000.f) { Out = S + FVector(FMath::RandPointInCircle(1000.f), 0.f); Out.Z = OcSurface(this, Out) - 500.f; return true; } }
	if (bNight && Kind == TEXT("driftback") && Zone == TEXT("O1")) { const FVector S = OcSite(TEXT("turtle_beach"), FVector::ZeroVector); if (!S.IsZero() && FVector::Dist2D(S, Near) < 25000.f) { Out = S + FVector(FMath::RandPointInCircle(1500.f), 0.f); Out.Z = OcFloor(Out, S.Z) + Half + 30.f; return true; } }
	const float MinR = OcTn(TEXT("spawnMin"), 6000.f), MaxR = OcTn(TEXT("spawnMax"), 11000.f);
	for (int32 Try = 0; Try < 16; ++Try)
	{
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		// Out of sight: not in front of where the nearest survivor faces.
		if (FVector2D::DotProduct(Dir, FVector2D(Facing).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(OcTn(TEXT("unseenHalfAngle"), 60.f)))) continue;
		FVector P(FVector2D(Near) + Dir * FMath::FRandRange(MinR, MaxR), Near.Z);
		if (WROcean::ZoneAt(P) != Zone && !(Zone == TEXT("O5") && WROcean::InSea(P))) continue;
		if (WROcean::InCave(P) || WROcean::InHall(P)) continue;
		const float Surface = OcSurface(this, P);
		const float Floor = OcFloor(P, Surface - 10000.f);
		const float Depth = Surface - Floor;
		const bool bSwim = Def->SwimSpeed > 0.f && Kind != TEXT("driftback");
		if (Kind == TEXT("gullraider"))
		{
			if (Floor < Surface - 200.f) continue;            // over the land or the cliffs
			P.Z = Floor + FMath::FRandRange(1200.f, 2500.f);
		}
		else if (Kind == TEXT("spinecrab") || Kind == TEXT("tidefowl"))
		{
			if (Floor < Surface - 60.f) continue;             // the sand, the shelf, the cliff tops
			P.Z = Floor + Half + 30.f;
		}
		else if (Kind == TEXT("tidecoil"))
		{
			if (Depth < 120.f) continue;                      // a hole in the rock under the water (a rock pool at low tide)
			P.Z = Floor + Half + 30.f;
		}
		else if (bSwim || Kind == TEXT("driftback"))
		{
			const float Min = Kind == TEXT("reefjaw") ? 500.f : Kind == TEXT("ripplefin") || Kind == TEXT("glowray") ? 400.f : 180.f;
			if (Depth < Min) continue;
			const float Want = Kind == TEXT("pearldiver") || Kind == TEXT("kelpgrazer") || Kind == TEXT("driftback") ? Surface - Half * 0.4f : Surface - FMath::Min(Depth * 0.5f, 900.f);
			P.Z = FMath::Max(Want, Floor + Half + 60.f);
		}
		else continue;
		Out = P;
		return true;
	}
	return false;
}

void UWROceanLife::StepPopulations()
{
	// Per survivor in the Ocean: the sub-zone's day or night table (tuning oceanLife.tables) keeps about perPlayer
	// creatures within reach of them, filled out of sight; far from everyone they're taken in.
	const TSharedPtr<FJsonObject> Tables = FWRTuning::Get().Object(TEXT("oceanLife.tables"));
	if (!Tables.IsValid()) return;
	Spawned.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive() || C->IsTamed(); });
	struct FWho { FVector At; FVector Facing; };
	TArray<FWho> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && WROcean::Contains(P->GetActorLocation())) Players.Add({ P->GetActorLocation(), It->Get()->GetControlRotation().Vector() });
	}
	// Taken in: nobody within despawnReach and not fighting.
	const float Gone = OcTn(TEXT("despawnReach"), 22000.f);
	for (const TWeakObjectPtr<AWRCreature>& W : Spawned)
	{
		AWRCreature* C = W.Get();
		if (!C || C->GetAggroTarget()) continue;
		bool bNear = false;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && FVector::Dist(P->GetActorLocation(), C->GetActorLocation()) < Gone) bNear = true;
		}
		if (!bNear) C->Destroy();
	}
	Spawned.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid(); });
	const bool bNight = OcNight(this);
	const float Mult = FMath::Max(0.1f, FWRTuning::Get().Num(TEXT("server.creatureNumbers"), 1.f));
	const FWRWeatherKind* Wx = UWRWeatherSubsystem::ActiveKind(this, WROcean::Biome());
	const bool bSquall = Wx && Wx->Id == TEXT("squall");
	for (const FWho& Pl : Players)
	{
		const FName Zone = WROcean::ZoneAt(Pl.At);
		if (Zone.IsNone()) continue;
		const TSharedPtr<FJsonObject>* ZoneT = nullptr;
		if (!Tables->TryGetObjectField(Zone.ToString(), ZoneT)) continue;
		const TSharedPtr<FJsonObject>* Table = nullptr;
		if (!(*ZoneT)->TryGetObjectField(bNight ? TEXT("night") : TEXT("day"), Table)) continue;
		const float Reach = OcTn(TEXT("countReach"), 14000.f);
		int32 Have = 0;
		for (const TWeakObjectPtr<AWRCreature>& C : Spawned) if (C.IsValid() && FVector::Dist(C->GetActorLocation(), Pl.At) < Reach) ++Have;
		// The open sea's Reefjaws double in a Squall and at night (the tables carry the night).
		const int32 Want = FMath::RoundToInt(OcTn(TEXT("perPlayer"), 12.f) * Mult * (bSquall && Zone == TEXT("O5") ? OcTn(TEXT("squallSeaMult"), 2.f) : 1.f));
		if (Have >= Want) continue;
		// A weighted pick from the table.
		float Sum = 0.f;
		for (const auto& Pair : (*Table)->Values) Sum += float(Pair.Value->AsNumber());
		if (Sum <= 0.f) continue;
		float Roll = FMath::FRandRange(0.f, Sum);
		FName Kind;
		for (const auto& Pair : (*Table)->Values) { Roll -= float(Pair.Value->AsNumber()); if (Roll <= 0.f) { Kind = FName(*Pair.Key); break; } }
		if (Kind.IsNone()) continue;
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
		FVector Spot;
		if (!Def || !FindSpot(Kind, Zone, Pl.At, Pl.Facing, Spot)) continue;
		// Never in front of anyone else either.
		bool bSeen = false;
		for (const FWho& O : Players) if (FVector::Dist(O.At, Spot) < OcTn(TEXT("spawnUnseen"), 5000.f)) bSeen = true;
		if (bSeen) continue;
		const int32 Pack = FMath::Clamp(FMath::RandRange(Def->PackMin, Def->PackMax), 1, FMath::Max(1, Want - Have));
		for (int32 n = 0; n < Pack; ++n)
		{
			FVector At = Spot + FVector(FMath::RandPointInCircle(OcTn(TEXT("packSpread"), 500.f)), 0.f);
			if (Kind == TEXT("spinecrab") || Kind == TEXT("tidefowl") || Kind == TEXT("tidecoil")) At.Z = OcFloor(At, Spot.Z) + Def->HalfHeight + 30.f;
			SpawnOne(Kind, At, Zone);
		}
	}
}

void UWROceanLife::StepBeats()
{
	// Reefjaws take Kelpgrazers at the kelp edge (now and then a hunting one goes for one near it).
	for (const TWeakObjectPtr<UWROceanBody>& W : UWROceanBody::Live)
	{
		UWROceanBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || C->CreatureId != TEXT("reefjaw") || !C->IsAlive() || C->IsTamed() || C->GetAggroTarget()) continue;
		if (FMath::FRand() > OcTn(TEXT("beats.huntChance"), 0.05f)) continue;
		AWRCreature* Prey = nullptr;
		OcEachKind(GetWorld(), TEXT("kelpgrazer"), C->GetActorLocation(), OcTn(TEXT("beats.huntReach"), 4000.f), [&](AWRCreature* G, UWROceanBody*) { if (!Prey) Prey = G; });
		if (!Prey) OcEachKind(GetWorld(), TEXT("ripplefin"), C->GetActorLocation(), OcTn(TEXT("beats.huntReach"), 4000.f), [&](AWRCreature* G, UWROceanBody*) { if (!Prey) Prey = G; });
		if (Prey) C->SetAggroTarget(Prey);
	}
}

void UWROceanLife::NoteDeath(AWRCreature* Dead, AActor* Killer)
{
	if (!Dead) return;
	const AWRCreature* By = Cast<AWRCreature>(Killer);
	// A kill in the water: a blood cloud you can see from 300 m, and the Gullraiders come diving on it.
	if (OcBodyInWater(Dead) && (By || Dead->CreatureId == TEXT("kelpgrazer") || Dead->CreatureId == TEXT("reefjaw")))
	{
		const FVector At(Dead->GetActorLocation().X, Dead->GetActorLocation().Y, OcSurface(this, Dead->GetActorLocation()));
		WRFx::Broadcast(Dead, TEXT("ocean.blood_cloud"), Dead->GetActorLocation(), FRotator::ZeroRotator, 3.f, NAME_None, 30.f);
		OcEachKind(GetWorld(), TEXT("gullraider"), At, OcTn(TEXT("beats.gullsReach"), 30000.f), [&](AWRCreature* G, UWROceanBody* B)
		{
			if (!G->GetAggroTarget() && !B->Has(WROceanFlag::Carrying)) { G->SetHome(At + FVector(0, 0, 1200.f)); B->Home = At + FVector(0, 0, 1200.f); }
		});
	}
}
