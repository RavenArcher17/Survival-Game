// The Desert's creatures: their body component, their brains (FWRDesertAI) and the living desert (UWRDesertLife). See
// WRDesertCreatures.h; cooks/biome-desert.md 2.12, build-biome-desert.md 2.12. Numbers: tuning desertLife.

#include "WRDesertCreatures.h"
#include "WRDesertHazards.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCharacterMovementComponent.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRDesertTypes.h"
#include "WRDesertWorld.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSunPath.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

TArray<TWeakObjectPtr<UWRDesertBody>> UWRDesertBody::Live;

namespace WRDesertCrAI
{
	float DcTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("desertLife.%s"), Key), Fallback); }
	double DcNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool DcLongNoon(const UObject* W) { const AWRDesertState* S = AWRDesertState::Get(W); return S && S->bLongNoon; }
	bool DcNight(const UObject* W) { return AWRGameState::IsNightIn(W) && !DcLongNoon(W); }
	float DcHour(const UObject* W) { return DcLongNoon(W) ? 12.f : AWRGameState::HourIn(W); }
	FVector DcFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }
	uint8 DcSurf(const AActor* A) { return WRDesert::SurfaceAt(DcFeet(A) + FVector(0, 0, 20.f)); }
	bool DcSoftUnder(const AActor* A) { return WRDesert::IsSoftSand(DcSurf(A)); }
	float DcGround(const FVector& P, float Fallback) { return WRLand::GroundZAt(P, Fallback); }
	FString DcName(const AActor* Who) { return WRWorld::PlayerOf(Who); }
	void DcTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	template <typename F> void DcEachKind(const UWorld* W, FName Kind, const FVector& At, float Reach, F&& Fn)
	{
		for (const TWeakObjectPtr<UWRDesertBody>& B : UWRDesertBody::Live)
		{
			UWRDesertBody* Bd = B.Get();
			AWRCreature* C = Bd ? Cast<AWRCreature>(Bd->GetOwner()) : nullptr;
			if (!C || C->GetWorld() != W || !C->IsAlive() || (Kind != NAME_None && C->CreatureId != Kind)) continue;
			if (FVector::DistSquared(C->GetActorLocation(), At) > Reach * Reach) continue;
			Fn(C, Bd);
		}
	}
	void DcHit(AWRCreature* Self, AWRCharacterBase* V, float Damage, float Blind, bool bUnblockable, bool bDot = false)
	{
		FWRHit Hit;
		Hit.Instigator = Self;
		Hit.Damage = Damage;
		Hit.bUnblockable = bUnblockable;
		Hit.bMelee = !bDot;
		Hit.bDamageOverTime = bDot;
		Hit.Origin = Self->GetActorLocation();
		Hit.ImpactPoint = V->GetActorLocation();
		if (Blind > 0.f) Hit.Buildups.Add(TEXT("blind"), Blind);
		UWRCombatLibrary::ApplyHit(V, Hit);
	}
	bool DcSprinting(const AActor* A)
	{
		const ACharacter* C = Cast<ACharacter>(A);
		const UWRCharacterMovementComponent* M = C ? Cast<UWRCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr;
		return M && M->IsMovingOnGround() && M->IsSprinting() && C->GetVelocity().Size2D() > 300.f;
	}
	float DcBaseSpeed(const AWRCreature* C) { const FWRCreatureDef* D = C->GetDef(); return D ? D->Speed : 450.f; }
	bool DcPredator(const AWRCreature* C) { const FWRCreatureDef* D = C->GetDef(); return D && D->Behavior == TEXT("aggressive"); }
}
using namespace WRDesertCrAI;

// ================================================================================================ the body component

UWRDesertBody::UWRDesertBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRDesertBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRDesertBody, Flags);
	DOREPLIFETIME(UWRDesertBody, Seed);
	DOREPLIFETIME(UWRDesertBody, Holding);
}

UWRDesertBody* UWRDesertBody::Get(const AActor* Actor) { return Actor ? Actor->FindComponentByClass<UWRDesertBody>() : nullptr; }

bool UWRDesertBody::IsDesertKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("desert");
}

void UWRDesertBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsDesertKind(Creature->CreatureId) || Get(Creature)) return;
	UWRDesertBody* Body = NewObject<UWRDesertBody>(Creature, TEXT("DesertBody"));
	Body->Seed = FMath::Rand();
	Body->Home = Creature->GetActorLocation();
	Body->CircleAngle = FMath::FRandRange(0.f, 2.f * PI);
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

void UWRDesertBody::Set(uint32 Flag, bool bOn)
{
	const uint32 Was = Flags;
	Flags = bOn ? (Flags | Flag) : (Flags & ~Flag);
	if (Was != Flags && GetNetMode() != NM_DedicatedServer) OnRep_Flags();
}

void UWRDesertBody::OnRep_Flags() {}

void UWRDesertBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRDesertBody>& B) { return !B.IsValid(); });
	Live.Add(this);
}

void UWRDesertBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner() && GetOwner()->HasAuthority() && HeldBody.IsValid())
	{
		if (AWRCharacterBase* V = HeldBody.Get(); V && V->GetAbilitySystemComponent()) V->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::Status_Rooted);
	}
	Live.RemoveAll([this](const TWeakObjectPtr<UWRDesertBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

void UWRDesertBody::MulticastMoment_Implementation(FName Effect, float Scale, FName Sound)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FVector At = C->GetActorLocation() + C->GetActorForwardVector() * C->GetSimpleCollisionRadius();
	if (!Effect.IsNone()) WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
	if (!Sound.IsNone()) WRSound::PlayAt(C, Sound, At);
}

void UWRDesertBody::MulticastMirage_Implementation(FVector At, float Yaw, float Seconds)
{
	// The Shimmerbuck's mirage (cook 2.12.2): a still copy of it where it stood, no shadow (only the real one casts one),
	// a heat shimmer rising off it, gone in Seconds; then it bolts.
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!C || !Def || Def->ProcBody.IsNone() || GetNetMode() == NM_DedicatedServer) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ghost = C->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), At), P);
	if (!Ghost) return;
	USceneComponent* Root = NewObject<USceneComponent>(Ghost);
	Ghost->SetRootComponent(Root);
	Root->RegisterComponent();
	UWRDesertBeast* Copy = NewObject<UWRDesertBeast>(Ghost);
	Copy->SetupAttachment(Root);
	Copy->RegisterComponent();
	Copy->InitMirage(C, Def->ProcBody, C->CreatureId == TEXT("mirage_matriarch") ? 2.f : 1.f, Seed, Seconds);
	Ghost->SetLifeSpan(Seconds + 0.1f);
	WRFx::SpawnAt(C, TEXT("desert.mirage_shimmer"), At, FRotator::ZeroRotator, 1.f);
}

void UWRDesertBody::BuildLook()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!Def || Def->ProcBody.IsNone() || Beast) return;
	// The minis reuse the bodies at their scale (the Glass Empress x4, the Matriarch x2, Ssarrok x3.7).
	float Scale = 1.f;
	if (C->CreatureId == TEXT("glass_empress")) Scale = 4.f;
	else if (C->CreatureId == TEXT("mirage_matriarch")) Scale = 2.f;
	else if (C->CreatureId == TEXT("ssarrok")) Scale = 3.7f;
	else if (C->CreatureId == TEXT("matriarch_mirage")) Scale = 2.f;
	Beast = NewObject<UWRDesertBeast>(C, TEXT("DesertBeast"));
	Beast->SetupAttachment(C->GetRootComponent());
	Beast->RegisterComponent();
	// The body stands on its feet at the capsule's bottom.
	Beast->SetRelativeLocation(FVector(0.f, 0.f, -C->GetSimpleCollisionHalfHeight()));
	Beast->Init(C, Def->ProcBody, Scale, Seed);
	// The Matriarch's mirages cast no shadow: only the real one does (cook 2.13.2).
	if (C->CreatureId == TEXT("matriarch_mirage")) Beast->SetCastShadow(false);
	for (UActorComponent* Comp : C->GetComponents())
	{
		UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Comp);
		if (SM && SM != static_cast<UPrimitiveComponent*>(Beast)) SM->SetVisibility(false, false);
		if (USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(Comp)) Sk->SetVisibility(false, true);
	}
}

void UWRDesertBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C) return;
	if (C->HasAuthority()) FWRDesertAI::TickBody(C, this, DeltaTime);
	if (GetNetMode() != NM_DedicatedServer && !Beast && LookTries < 60)
	{
		++LookTries;
		BuildLook();
	}
}

// ================================================================================================ the brains

bool FWRDesertAI::CanDive(const AActor* Who)
{
	// Soft sand only (the cook's rule made real): never wet sand, hardpan, rock, the slot floors or the plaza.
	return Who && DcSoftUnder(Who) && !WRDesertHz::WetSandAt(Who, Who->GetActorLocation());
}

AWRCreature* FWRDesertAI::HolderOf(const AActor* Victim)
{
	if (!Victim) return nullptr;
	for (const TWeakObjectPtr<UWRDesertBody>& W : UWRDesertBody::Live)
	{
		UWRDesertBody* B = W.Get();
		if (B && B->Holding == Victim) return Cast<AWRCreature>(B->GetOwner());
	}
	return nullptr;
}

void FWRDesertAI::TickBody(AWRCreature* Self, UWRDesertBody* Body, float DeltaTime)
{
	const double T = DcNow(Self);
	// The grip (the Glasstail's pincer grab): held 1.5 s at the pincers, then the sting (x2, blind 40), unless an ally's
	// blows on it freed you first.
	// (A boss's holds are its own: WRDesertBosses.)
	if (AWRCharacterBase* V = Self->GetDef() && Self->GetDef()->Tier == TEXT("boss") ? nullptr : Body->HeldBody.Get())
	{
		if (!Self->IsAlive() || !V->IsAlive() || Body->FreeDamage >= DcTn(TEXT("glasstail.gripFree"), 60.f))
		{
			Body->HeldBody = nullptr;
			Body->Holding = nullptr;
			Body->Set(WRDesertFlag::Grip, false);
			if (V->GetAbilitySystemComponent()) V->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::Status_Rooted);
			if (Body->FreeDamage >= DcTn(TEXT("glasstail.gripFree"), 60.f)) { Body->MulticastMoment(TEXT("hit.dust"), 1.f, TEXT("creature.glasstail.release")); if (Self->Status) Self->Status->Stagger(0.6f); }
		}
		else
		{
			const FVector Claw = Self->GetActorLocation() + Self->GetActorForwardVector() * (Self->GetSimpleCollisionRadius() + 70.f);
			V->SetActorLocation(FVector(Claw.X, Claw.Y, V->GetActorLocation().Z), true);
			if (T >= Body->HoldUntil)
			{
				// The sting: the sunglass bulb flashes white 0.2 s before (the fx), then the strike through the hold.
				Body->MulticastMoment(TEXT("sun.blind_flash"), 1.f, TEXT("creature.glasstail.sting"));
				DcHit(Self, V, Self->Weapon.FlatDamage * DcTn(TEXT("glasstail.gripSting"), 2.f), DcTn(TEXT("glasstail.gripBlind"), 40.f), true);
				Body->HeldBody = nullptr;
				Body->Holding = nullptr;
				Body->Set(WRDesertFlag::Grip, false);
				Body->Set(WRDesertFlag::Raised, false);
				if (V->GetAbilitySystemComponent()) V->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::Status_Rooted);
			}
		}
	}
	// Beached: a Dunejaw that burst onto firm ground lies out, then rights itself.
	if (Body->Has(WRDesertFlag::Beached) && T >= Body->BeachedUntil) Body->Set(WRDesertFlag::Beached, false);
	// The Gildback's roll ends; its ducked head comes back up.
	if (Body->Has(WRDesertFlag::Rolling) && T >= Body->RollUntil) Body->Set(WRDesertFlag::Rolling, false);
	if (Body->Has(WRDesertFlag::Ducked) && T >= Body->DuckUntil) Body->Set(WRDesertFlag::Ducked, false);
	// The stored light: Gildback spines and the Glasstail's moonlit cyan show at night.
	if (Self->CreatureId == TEXT("gildback") || Self->CreatureId == TEXT("glasstail") || Self->CreatureId == TEXT("glass_empress"))
	{
		const bool bGlow = DcNight(Self);
		if (Body->Has(WRDesertFlag::Glowing) != bGlow) Body->Set(WRDesertFlag::Glowing, bGlow);
	}
}

bool FWRDesertAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return false;
	const double T = DcNow(Self);
	const FName K = Self->CreatureId;
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	const FVector P = Self->GetActorLocation();
	if (AI->RowPhase == 0)
	{
		Body->Set(WRDesertFlag::Jaw, false);
		if (!Body->HeldBody.IsValid()) Body->Set(WRDesertFlag::Raised, false);
		Body->Set(WRDesertFlag::Flaring, false);
		Body->Set(WRDesertFlag::Dulla, false);
		Body->Set(WRDesertFlag::Spray, false);
	}
	if (Body->HeldBody.IsValid() || Body->Has(WRDesertFlag::Beached) || Body->Has(WRDesertFlag::Rolling)) return true;
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	const bool bNight = DcNight(Self);

	// ---- Dunejaw: the fin over the soft sand; a sprint's vibration draws it; the burst from below.
	if (K == TEXT("dunejaw"))
	{
		const bool bSoft = CanDive(Self);
		if (!Target)
		{
			Body->Set(WRDesertFlag::Diving, bSoft);
			Move->MaxWalkSpeed = bSoft ? DcTn(TEXT("dunejaw.cruise"), 350.f) : DcBaseSpeed(Self);
			// Vibration sense: anyone sprinting on soft sand within 40 m (walking doesn't).
			for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !Self->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), P) > DcTn(TEXT("dunejaw.vibration"), 4000.f)) continue;
				if (DcSprinting(*It) && DcSoftUnder(*It) && !WRSun::Sandwalker(*It)) { Self->SetAggroTarget(*It); break; }
			}
			return false;
		}
		const float D = FVector::Dist2D(Target->GetActorLocation(), P);
		if (bSoft && D > DcTn(TEXT("dunejaw.burstReach"), 500.f) && AI->RowPhase == 0)
		{
			// Swim at it under the sand at 9 m/s, only the fin showing, the hiss rising as it comes.
			Body->Set(WRDesertFlag::Diving, true);
			Move->MaxWalkSpeed = DcTn(TEXT("dunejaw.swim"), 900.f);
			const FVector Dir = (Target->GetActorLocation() - P).GetSafeNormal2D();
			Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), Dir.Rotation(), DeltaSeconds, 6.f));
			Self->AddMovementInput(Dir, 1.f);
			if (T >= Body->NextHissAt) { Body->NextHissAt = T + 0.6; Body->MulticastMoment(TEXT("desert.fin_wake"), 1.f, TEXT("creature.dunejaw.hiss")); }
			return true;
		}
		if (bSoft && Body->Has(WRDesertFlag::Diving) && AI->RowPhase == 0)
		{
			if (AI->RowReadyAt.FindRef(TEXT("burst_lunge")) <= T) { AI->StartRow(Self, Target, TEXT("burst_lunge"), false); return false; }
		}
		if (!bSoft) Body->Set(WRDesertFlag::Diving, false);
		Move->MaxWalkSpeed = DcBaseSpeed(Self);
		return false;
	}

	// ---- Glasstail: by night it waits under the sand on the paths and bursts at what comes within 3 m.
	if (K == TEXT("glasstail"))
	{
		if (!Target && bNight && Move->IsMovingOnGround() && DcSurf(Self) != EWRDesertSurf::RedRock && DcSurf(Self) != EWRDesertSurf::Glass)
		{
			if (!Body->Has(WRDesertFlag::Buried)) Body->MulticastMoment(TEXT("land.dust"), 1.f, TEXT("creature.glasstail.bury"));
			Body->Set(WRDesertFlag::Buried, true);
			Move->StopMovementImmediately();
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It) || UWRDesertBody::Get(*It)) continue;
				if (FVector::Dist(It->GetActorLocation(), P) < DcTn(TEXT("glasstail.ambushReach"), 300.f))
				{
					Self->SetAggroTarget(*It);
					AI->StartRow(Self, *It, TEXT("sand_burst"), false);
					break;
				}
			}
			return true;
		}
		if (Body->Has(WRDesertFlag::Buried) && AI->RowId != TEXT("sand_burst")) Body->Set(WRDesertFlag::Buried, false);
		return false;
	}

	// ---- Carrion kite: circles a wounded survivor 20 s, then screams the predators onto them.
	if (K == TEXT("carrion_kite"))
	{
		AActor* Over = Body->CircleOver.Get();
		if (!Over && !Target)
		{
			for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
			{
				const UWRAttributeSet* A = It->GetAttributes();
				if (!It->IsAlive() || !A || A->GetHealth() > A->GetMaxHealth() * DcTn(TEXT("kite.circleBelow"), 0.5f)) continue;
				if (FVector::Dist(It->GetActorLocation(), P) > DcTn(TEXT("kite.circleReach"), 12000.f)) continue;
				Body->CircleOver = *It;
				Body->CircleStart = T;
				Body->CircleUntil = T + DcTn(TEXT("kite.circleSeconds"), 20.f);
				Over = *It;
				break;
			}
		}
		if (Over)
		{
			Body->Set(WRDesertFlag::Circling, true);
			Body->CircleAngle += DeltaSeconds * DcTn(TEXT("kite.circleTurn"), 0.35f);
			const float R = DcTn(TEXT("kite.circleRadius"), 1200.f);
			const FVector Want = Over->GetActorLocation() + FVector(FMath::Cos(Body->CircleAngle) * R, FMath::Sin(Body->CircleAngle) * R, DcTn(TEXT("kite.circleHeight"), 2500.f));
			const FVector Dir = (Want - P).GetSafeNormal();
			if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
			Self->AddMovementInput(Dir, 1.f);
			Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), Dir.Rotation(), DeltaSeconds, 3.f));
			if (T >= Body->CircleUntil)
			{
				// The scream: predators within 120 m turn toward the circled body (kill or ground it before it calls).
				Body->CircleOver = nullptr;
				Body->Set(WRDesertFlag::Circling, false);
				if (const AWRCharacter* Ch = Cast<AWRCharacter>(Over))
				{
					Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.carrion_kite.scream"));
					DcEachKind(Self->GetWorld(), NAME_None, Over->GetActorLocation(), DcTn(TEXT("kite.callReach"), 12000.f), [&](AWRCreature* C, UWRDesertBody*)
					{
						if (C != Self && DcPredator(C) && !C->IsTamed() && !C->GetAggroTarget()) C->SetAggroTarget(const_cast<AWRCharacter*>(Ch));
					});
					Self->SetAggroTarget(const_cast<AWRCharacter*>(Ch));
				}
			}
			return true;
		}
		Body->Set(WRDesertFlag::Circling, false);
		return false;
	}

	// ---- Gildback: basks on the Pan at noon (calm: the tame window), its spines' glow at night.
	if (K == TEXT("gildback") && !Target)
	{
		const float H = DcHour(Self);
		const bool bBask = (H >= DcTn(TEXT("gildback.baskFrom"), 11.f) && H < DcTn(TEXT("gildback.baskTo"), 15.f)) && WRDesert::InPan(P);
		Body->Set(WRDesertFlag::Basking, bBask);
		if (bBask) { Move->StopMovementImmediately(); return true; }
		return false;
	}

	// ---- The herds' night: Dromads couch, Shimmerbucks sleep in rings; at Ahrumet's dawn song every Dromad turns east.
	if ((K == TEXT("dromad") || K == TEXT("shimmerbuck")) && !Target)
	{
		if (T < Body->FaceEastUntil)
		{
			Move->StopMovementImmediately();
			Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), FRotator(0.f, 0.f, 0.f), DeltaSeconds, 2.f));
			return true;
		}
		Body->Set(WRDesertFlag::Asleep, bNight && !Self->IsFleeing());
		if (bNight && !Self->IsFleeing()) { Move->StopMovementImmediately(); return true; }
		return false;
	}

	// ---- Scarab digger: always behind its ball.
	if (K == TEXT("scarab_digger")) { Body->Set(WRDesertFlag::Ball, true); return false; }

	// ---- Wellfrog: in its clay by day (squeezable), out at night and in rain.
	if (K == TEXT("wellfrog"))
	{
		const FWRWeatherKind* Wx = UWRWeatherSubsystem::ActiveKind(Self, WRDesert::Biome());
		const bool bOut = bNight || (Wx && (Wx->Id == TEXT("rain") || Wx->Id == TEXT("storm")));
		Body->Set(WRDesertFlag::Buried, !bOut);
		if (!bOut) { Move->StopMovementImmediately(); return true; }
		return false;
	}

	// ---- Duskjackal: the pack's chorus at the start of a hunt (RowAllowed picks it once).
	return false;
}

bool FWRDesertAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return false;
	const FName K = Self->CreatureId;
	const double T = DcNow(Self);
	// The Scarab digger's nose (cook 2.12.1): buried things within 40 m show for its owner (the dig mounds), and it chirps
	// at a closed Hourhold door.
	if (K == TEXT("scarab_digger") && T >= Body->BaskCheckAt)
	{
		Body->BaskCheckAt = T + 2.0;
		const float R = DcTn(TEXT("scarab.senseReach"), 4000.f);
		bool bChirp = false;
		for (TActorIterator<AWRDesertSpot> It(Self->GetWorld()); It; ++It)
		{
			if (It->Kind != TEXT("dig_mound") || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > R) continue;
			if (It->bConcealed) { It->bConcealed = false; bChirp = true; }
		}
		const FWRDesertData& D = FWRDesertData::Get();
		const AWRDesertState* S = AWRDesertState::Get(Self);
		for (const FWRDesertData::FWay& W : D.Ways)
		{
			const bool bClosed = (W.Kind == TEXT("lee_door") && (!S || S->LeeOpenUntil <= 0.f)) || (W.Kind == TEXT("dig") && (!S || !S->bDigDone));
			if (bClosed && FVector::Dist(W.From, Self->GetActorLocation()) < R) bChirp = true;
		}
		if (bChirp) Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.scarab_digger.chirp"));
	}
	return false;
}

bool FWRDesertAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return true;
	const FName Id = Row.Id;
	const FName K = Self->CreatureId;
	if (Body->Has(WRDesertFlag::Beached)) return false;
	if (K == TEXT("dunejaw"))
	{
		if (Id == TEXT("burst_lunge")) return Body->Has(WRDesertFlag::Diving);
		if (Id == TEXT("re_dive")) return !Body->Has(WRDesertFlag::Diving) && CanDive(Self);
		return !Body->Has(WRDesertFlag::Diving);
	}
	if (K == TEXT("glasstail"))
	{
		if (Id == TEXT("sand_burst")) return Body->Has(WRDesertFlag::Buried);
		if (Body->Has(WRDesertFlag::Buried)) return false;
		if (Id == TEXT("glare")) return !DcNight(Self);          // daylight only: it raises the bulb to the sun
		return true;
	}
	if (K == TEXT("duskjackal"))
	{
		if (Id == TEXT("chorus")) return !Body->bChorused;
		// Hamstring: only from the flank or behind (90-150 degrees off where you face).
		if (Id == TEXT("hamstring") && Target)
		{
			const FVector Face = Target->GetActorForwardVector().GetSafeNormal2D();
			const FVector From = (Self->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			return FVector::DotProduct(Face, From) < FMath::Cos(FMath::DegreesToRadians(DcTn(TEXT("duskjackal.flankFrom"), 90.f)));
		}
		return true;
	}
	if (K == TEXT("carrion_kite"))
	{
		if (Id == TEXT("bone_drop")) return Target && Self->GetActorLocation().Z > Target->GetActorLocation().Z + DcTn(TEXT("kite.dropAbove"), 1200.f);
		if (Id == TEXT("talon_dive")) return Target && Self->GetCharacterMovement()->MovementMode == MOVE_Flying && Self->GetActorLocation().Z > Target->GetActorLocation().Z + 600.f;
		if (Id == TEXT("wing_buffet")) return Self->GetCharacterMovement()->IsMovingOnGround();
		return true;
	}
	return true;
}

void FWRDesertAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return;
	const FName Id = Row.Id;
	if (Id == TEXT("burst_lunge"))
	{
		// The tell: a sand bulge rising under the target, a rising hiss and the fin dropping (0.9 s).
		if (Target) WRFx::Broadcast(Self, TEXT("desert.sand_bulge"), DcFeet(Target), FRotator::ZeroRotator, 1.2f, TEXT("creature.dunejaw.rise"), Row.Windup);
	}
	else if (Id == TEXT("bite") || Id == TEXT("double_bite") || Id == TEXT("pincer_snap") || Id == TEXT("horn_thrust") || Id == TEXT("headbutt")) Body->Set(WRDesertFlag::Jaw, true);
	else if (Id == TEXT("sand_spray")) Body->Set(WRDesertFlag::Spray, true);
	else if (Id == TEXT("glare")) { Body->Set(WRDesertFlag::Raised, true); Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.glasstail.glass_ring")); }
	else if (Id == TEXT("sting") || Id == TEXT("pincer_grab")) Body->Set(WRDesertFlag::Raised, true);
	else if (Id == TEXT("sand_burst")) Body->MulticastMoment(TEXT("desert.sand_bulge"), 1.f, TEXT("creature.glasstail.rattle"));
	else if (Id == TEXT("spine_flare")) { Body->Set(WRDesertFlag::Flaring, true); Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.gildback.hum")); }
	else if (Id == TEXT("spine_roll")) { Body->Set(WRDesertFlag::Rolling, true); Body->RollUntil = DcNow(Self) + Row.Windup + Row.Active + 0.2; }
	else if (Id == TEXT("dulla_spit") || Id == TEXT("body_crash")) { Body->Set(WRDesertFlag::Dulla, true); Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.dromad.gurgle")); }
	else if (Id == TEXT("chorus")) Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.duskjackal.chorus"));
	else if (Id == TEXT("talon_dive")) Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.carrion_kite.screech"));
	else if (Id == TEXT("ball_roll")) Body->Set(WRDesertFlag::Ball, true);
}

bool FWRDesertAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return false;
	const FName Id = Row.Id;
	const FVector P = Self->GetActorLocation();
	if (Id == TEXT("burst_lunge") && Target)
	{
		// Up out of the sand at them (the lunge's hit is the row's own); where it lands decides: soft sand, it re-dives
		// later; firm ground (hardpan, rock, wet sand), it lies out beached 0.8 s.
		Body->Set(WRDesertFlag::Diving, false);
		Body->Set(WRDesertFlag::Jaw, true);
		const FVector To = Target->GetActorLocation();
		Self->LaunchCharacter((To - P).GetSafeNormal2D() * DcTn(TEXT("dunejaw.burstSpeed"), 1200.f) + FVector(0, 0, DcTn(TEXT("dunejaw.burstLift"), 500.f)), true, true);
		AI->RowDir = (To - P).GetSafeNormal2D();
		WRFx::Broadcast(Self, TEXT("desert.sand_burst"), DcFeet(Self), FRotator::ZeroRotator, 1.6f, TEXT("creature.dunejaw.burst"));
		if (!WRDesert::IsSoftSand(WRDesert::SurfaceAt(DcFeet(Target) + FVector(0, 0, 20.f))))
		{
			Body->Set(WRDesertFlag::Beached, true);
			Body->BeachedUntil = DcNow(Self) + Row.Active + DcTn(TEXT("dunejaw.beachSeconds"), 0.8f) + 0.4;
		}
		return false;
	}
	if (Id == TEXT("re_dive"))
	{
		Body->Set(WRDesertFlag::Diving, true);
		Body->MulticastMoment(TEXT("desert.sand_burst"), 1.f, TEXT("creature.dunejaw.dive"));
		return true;
	}
	if (Id == TEXT("sand_spray") || Id == TEXT("glare") || Id == TEXT("spine_flare") || Id == TEXT("sand_burst"))
	{
		// Cones and rings of grit or light: the row's buildups (blind) land on everyone in them; the glare only on those
		// facing it (your camera within 60 degrees of it): turn away and it's nothing.
		const bool bCone = Id == TEXT("sand_spray") || Id == TEXT("glare");
		const float R = Row.Radius > 0.f ? Row.Radius : Id == TEXT("glare") ? 600.f : Id == TEXT("spine_flare") ? 400.f : 300.f;
		const float Half = FMath::DegreesToRadians(Row.Arc > 0.f ? Row.Arc : 30.f);
		const FName Fx = Id == TEXT("glare") ? FName(TEXT("sun.flare")) : Id == TEXT("spine_flare") ? FName(TEXT("sun.flare")) : FName(TEXT("land.dust"));
		WRFx::Broadcast(Self, Fx, P + Self->GetActorForwardVector() * (bCone ? 100.f : 0.f), Self->GetActorRotation(), R / 600.f, NAME_None);
		if (Id == TEXT("sand_burst")) Body->Set(WRDesertFlag::Buried, false);
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive() || !Self->IsHostileTo(V)) continue;
			const FVector To = V->GetActorLocation() - P;
			if (To.Size() > R + V->GetSimpleCollisionRadius()) continue;
			if (bCone && FMath::Acos(FMath::Clamp(FVector::DotProduct(To.GetSafeNormal2D(), Self->GetActorForwardVector().GetSafeNormal2D()), -1.f, 1.f)) > Half) continue;
			if (Id == TEXT("glare"))
			{
				const APawn* Pw = Cast<APawn>(V);
				const FVector View = Pw && Pw->GetController() ? Pw->GetControlRotation().Vector() : V->GetActorForwardVector();
				if (FVector::DotProduct(View.GetSafeNormal(), (-To).GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(DcTn(TEXT("glasstail.glareFacing"), 60.f)))) continue;
			}
			AI->RowHit(Self, V);
		}
		return true;
	}
	if (Id == TEXT("chorus"))
	{
		// The yip-howl calls the packmates within 60 m (once a fight; a blow during the call cancels it: the row's).
		Body->bChorused = true;
		Body->ChorusAt = DcNow(Self);
		DcEachKind(Self->GetWorld(), TEXT("duskjackal"), P, DcTn(TEXT("duskjackal.chorusReach"), 6000.f), [&](AWRCreature* C, UWRDesertBody* B)
		{
			if (C != Self && Target && !C->IsTamed()) { C->SetAggroTarget(Target); B->bChorused = true; }
		});
		return true;
	}
	if (Id == TEXT("bone_drop") && Target)
	{
		// It lets go of a bone or a rock from 15-40 m: the shadow grows on the ground under it for the last 0.9 s; it never
		// homes (where you were when it let go).
		const FVector From = P - FVector(0, 0, Self->GetSimpleCollisionHalfHeight());
		const FVector To = DcFeet(Target);
		FWRHit Hit;
		Hit.Instigator = Self;
		Hit.Damage = Self->Weapon.FlatDamage * Row.Dmg;
		Hit.bProjectile = true;
		Hit.PhysType = 2;
		Hit.Origin = From;
		FActorSpawnParameters Sp;
		Sp.Owner = Self;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Bone = Self->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, FRotator(-90.f, 0.f, 0.f), Sp))
		{
			const float Horizontal = FMath::Max(100.f, FVector::Dist2D(From, To) / FMath::Max(0.6f, DcTn(TEXT("kite.dropFall"), 1.0f)));
			Bone->InitLob(Hit, AWRProjectile::LobVelocity(From, To, Horizontal, 1.f), 1.f, DcTn(TEXT("kite.dropSplash"), 160.f), TEXT("code:kite_bone"));
		}
		WRFx::Broadcast(Self, TEXT("desert.drop_shadow"), To + FVector(0, 0, 4.f), FRotator::ZeroRotator, 1.f, NAME_None, 1.0f);
		return true;
	}
	if (Id == TEXT("spine_roll") && Target)
	{
		// Curled into a spiked ball, it rolls 8 m at you (the roll's contact is the row's).
		AI->RowDir = (Target->GetActorLocation() - P).GetSafeNormal2D();
		Self->LaunchCharacter(AI->RowDir * DcTn(TEXT("gildback.rollSpeed"), 900.f), true, false);
		return false;
	}
	return false;
}

void FWRDesertAI::TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds) {}

void FWRDesertAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (!bLanded) return;
	if (RowId == TEXT("pincer_grab") && !Body->HeldBody.IsValid())
	{
		// Held 1.5 s in the pincers, then the sting (TickBody); an ally's blows on it free you.
		Body->HeldBody = Victim;
		Body->Holding = Victim;
		Body->HoldUntil = DcNow(Self) + DcTn(TEXT("glasstail.gripSeconds"), 1.5f);
		Body->FreeDamage = 0.f;
		Body->Set(WRDesertFlag::Grip, true);
		if (Victim->GetAbilitySystemComponent()) Victim->GetAbilitySystemComponent()->AddLooseGameplayTag(WRTags::Status_Rooted);
		DcTell(Victim, TEXT("Caught in the pincers: an ally's blows on it will free you"));
	}
	else if (RowId == TEXT("hamstring") && Victim->Status) Victim->Status->ApplyStatus(WRTags::Status_Slowed, DcTn(TEXT("duskjackal.hamstringSeconds"), 2.f));
}

float FWRDesertAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return Damage;
	const FName K = Self->CreatureId;
	// Under the sand a Dunejaw is out of reach (a heavy blunt blow on the fin's sand shakes it out: Damaged).
	if (K == TEXT("dunejaw") && Body->Has(WRDesertFlag::Diving)) return Hit.bHeavy && Hit.bMelee ? Damage * 0.25f : 0.f;
	// Beached, or fighting on firm ground: it's out of its element (x1.3).
	if (K == TEXT("dunejaw") && (Body->Has(WRDesertFlag::Beached) || !CanDive(Self))) return Damage * DcTn(TEXT("dunejaw.firmTaken"), 1.3f);
	// The Gildback's false head: blows from the front land on the hump while its real head is tucked (x0.5).
	if (K == TEXT("gildback") && Body->Has(WRDesertFlag::Ducked))
	{
		const FVector From = (Hit.Origin - Self->GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(From, Self->GetActorForwardVector().GetSafeNormal2D()) > 0.f) return Damage * DcTn(TEXT("gildback.humpTaken"), 0.5f);
	}
	return Damage;
}

void FWRDesertAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return;
	const FName K = Self->CreatureId;
	const double T = DcNow(Self);
	if (Body->HeldBody.IsValid() && Hit.Instigator != Body->HeldBody.Get()) Body->FreeDamage += Damage;
	// A heavy blow on a diving Dunejaw's fin: it's shaken out of the sand, staggered.
	if (K == TEXT("dunejaw") && Body->Has(WRDesertFlag::Diving) && Hit.bHeavy && Hit.bMelee)
	{
		Body->Set(WRDesertFlag::Diving, false);
		Body->MulticastMoment(TEXT("desert.sand_burst"), 1.2f, TEXT("creature.dunejaw.pain"));
		if (Self->Status) Self->Status->Stagger(DcTn(TEXT("dunejaw.shakenSeconds"), 1.2f));
	}
	// The Gildback tucks its real head when threatened (until its next attack).
	if (K == TEXT("gildback")) { Body->Set(WRDesertFlag::Ducked, true); Body->DuckUntil = T + DcTn(TEXT("gildback.duckSeconds"), 3.f); Body->Set(WRDesertFlag::Basking, false); }
	// The Shimmerbuck leaves a mirage of itself (3 s, no shadow) and bolts.
	if ((K == TEXT("shimmerbuck")) && T >= Body->MirageReadyAt && Self->IsAlive())
	{
		Body->MirageReadyAt = T + DcTn(TEXT("shimmerbuck.mirageEvery"), 10.f);
		Body->MulticastMirage(Self->GetActorLocation(), Self->GetActorRotation().Yaw, DcTn(TEXT("shimmerbuck.mirageSeconds"), 3.f));
		Body->MulticastMoment(NAME_None, 1.f, TEXT("creature.shimmerbuck.whistle"));
		if (AActor* By = Hit.Instigator) Self->StartFleeing(By, TEXT("mirage"));
	}
	Body->Set(WRDesertFlag::Asleep, false);
}

void FWRDesertAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body) return;
	if (AWRCharacterBase* V = Body->HeldBody.Get(); V && V->GetAbilitySystemComponent()) V->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WRTags::Status_Rooted);
	Body->HeldBody = nullptr;
	Body->Holding = nullptr;
	if (UWRDesertLife* L = UWRDesertLife::Get(Self)) L->NoteDeath(Self, Killer);
}

bool FWRDesertAI::SqueezePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb)
{
	const UWRDesertBody* Body = UWRDesertBody::Get(Self);
	if (!Body || Self->CreatureId != TEXT("wellfrog") || Self->IsTamed() || !Self->IsAlive() || !Body->Has(WRDesertFlag::Buried)) return false;
	if (!Who || FVector::Dist(Who->GetActorLocation(), Self->GetActorLocation()) > 300.f) return false;
	OutVerb = TEXT("Squeeze it for a drink");
	return true;
}

bool FWRDesertAI::TrySqueeze(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult)
{
	UWRDesertBody* Body = UWRDesertBody::Get(Wild);
	AWRCharacterBase* Me = Player ? Cast<AWRCharacterBase>(Player->GetPawn()) : nullptr;
	FString Verb;
	if (!Body || !Me || !SqueezePrompt(Wild, Me, Verb)) return false;
	const double Hours = AWRGameState::Get(Wild) ? AWRGameState::Get(Wild)->GetTotalHours() : 0.0;
	if (Hours < Body->FaceEastUntil)                  // (its squeeze clock rides the same field: game hours until it gives again)
	{
		OutResult = TEXT("It's gone back into the clay. Leave it be a while");
		return true;
	}
	// One full drink (thirst +30) and it lives: back into the clay for 2 game hours (cook 2.12.1).
	if (UAbilitySystemComponent* ASC = Me->GetAbilitySystemComponent())
	{
		if (const UWRAttributeSet* A = Me->GetAttributes()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), FMath::Min(100.f, A->GetThirst() + DcTn(TEXT("wellfrog.drink"), 30.f)));
	}
	Body->FaceEastUntil = Hours + DcTn(TEXT("wellfrog.hours"), 2.f);
	Body->MulticastMoment(NAME_None, 1.f, TEXT("desert.wellfrog_squeeze"));
	OutResult = TEXT("Cool water, a little earthy. The frog shrinks back into the clay, none the worse.");
	return true;
}

// ================================================================================================ the living desert

UWRDesertLife* UWRDesertLife::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRDesertLife>() : nullptr;
}

bool UWRDesertLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UWRDesertLife::IsTickable() const
{
	const UWorld* W = GetWorld();
	return W && W->GetNetMode() != NM_Client && WRDesert::Live() && UWRDesertWorld::Get(W) && UWRDesertWorld::Get(W)->IsBuilt();
}

TStatId UWRDesertLife::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRDesertLife, STATGROUP_Tickables);
}

void UWRDesertLife::Tick(float DeltaTime)
{
	Clock += DeltaTime;
	BeatClock += DeltaTime;
	if (Clock >= DcTn(TEXT("stepSeconds"), 5.f)) { Clock = 0.f; StepPopulations(); }
	if (BeatClock >= 3.f) { BeatClock = 0.f; StepBeats(); }
}

AWRCreature* UWRDesertLife::SpawnOne(FName Kind, const FVector& At, FName Zone)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return nullptr;
	AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), Kind, At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	if (!C) return nullptr;
	C->SetHome(At);
	if (UWRDesertBody* B = UWRDesertBody::Get(C))
	{
		B->Home = At;
		if (Kind == TEXT("dunejaw") && FWRDesertAI::CanDive(C)) B->Set(WRDesertFlag::Diving, true);
		if (Kind == TEXT("wellfrog") && !DcNight(this)) B->Set(WRDesertFlag::Buried, true);
		if (Kind == TEXT("scarab_digger")) B->Set(WRDesertFlag::Ball, true);
	}
	if (Def->Archetype == TEXT("flyer")) C->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Spawned.Add(C);
	return C;
}

bool UWRDesertLife::FindSpot(FName Kind, FName Zone, const FVector& Near, const FVector& Facing, FVector& Out) const
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return false;
	const float Half = Def->HalfHeight;
	const float MinR = DcTn(TEXT("spawnMin"), 6000.f), MaxR = DcTn(TEXT("spawnMax"), 11000.f);
	for (int32 Try = 0; Try < 16; ++Try)
	{
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		if (FVector2D::DotProduct(Dir, FVector2D(Facing).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(DcTn(TEXT("unseenHalfAngle"), 60.f)))) continue;
		FVector P(FVector2D(Near) + Dir * FMath::FRandRange(MinR, MaxR), Near.Z);
		if (WRDesert::ZoneAt(P) != Zone || WRDesert::InCave(P)) continue;
		float Ground = 0.f;
		if (!WRLand::GroundUnder(P, Ground)) continue;
		if (Ground < 80.f) continue;                                   // never in the sea
		P.Z = Ground;
		const uint8 S = WRDesert::SurfaceAt(P + FVector(0, 0, 20.f));
		if (S == EWRDesertSurf::Quicksand || S == EWRDesertSurf::Brine || S == EWRDesertSurf::Sea || S == EWRDesertSurf::None) continue;
		// Each in its own place: the Dunejaw on soft sand only; the Wellfrog in the oasis clay and the wadi; the Sandhen
		// on the grass dunes and oases; the Scarab on the dunes; the kites up in the air.
		if (Kind == TEXT("dunejaw") && !WRDesert::IsSoftSand(S)) continue;
		if (Kind == TEXT("wellfrog") && !(S == EWRDesertSurf::Oasis || S == EWRDesertSurf::Gravel || S == EWRDesertSurf::Salt)) continue;
		if (Kind == TEXT("sandhen") && !(S == EWRDesertSurf::Grass || S == EWRDesertSurf::Oasis || S == EWRDesertSurf::Sand || S == EWRDesertSurf::Gravel)) continue;
		if (Kind == TEXT("scarab_digger") && !(WRDesert::IsSoftSand(S) || S == EWRDesertSurf::Oasis)) continue;
		if (S == EWRDesertSurf::Slipface && Kind != TEXT("dunejaw")) continue;
		P.Z += Def->Archetype == TEXT("flyer") ? FMath::FRandRange(DcTn(TEXT("kite.spawnAbove"), 1800.f), DcTn(TEXT("kite.spawnAboveMax"), 3500.f)) : Half + 30.f;
		Out = P;
		return true;
	}
	return false;
}

void UWRDesertLife::StepPopulations()
{
	// Per survivor in the desert: the sub-zone's day or night table (tuning desertLife.tables) keeps about perPlayer
	// creatures within reach, filled out of sight; far from everyone they're taken in.
	const TSharedPtr<FJsonObject> Tables = FWRTuning::Get().Object(TEXT("desertLife.tables"));
	if (!Tables.IsValid()) return;
	Spawned.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive() || C->IsTamed(); });
	struct FWho { FVector At; FVector Facing; };
	TArray<FWho> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && WRDesert::Contains(P->GetActorLocation()) && !WRDesert::InCave(P->GetActorLocation())) Players.Add({ P->GetActorLocation(), It->Get()->GetControlRotation().Vector() });
	}
	const float Gone = DcTn(TEXT("despawnReach"), 22000.f);
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
	const bool bNight = DcNight(this);
	const float Mult = FMath::Max(0.1f, FWRTuning::Get().Num(TEXT("server.creatureNumbers"), 1.f));
	for (const FWho& Pl : Players)
	{
		const FName Zone = WRDesert::ZoneAt(Pl.At);
		if (Zone.IsNone()) continue;
		const TSharedPtr<FJsonObject>* ZoneT = nullptr;
		if (!Tables->TryGetObjectField(Zone.ToString(), ZoneT)) continue;
		const TSharedPtr<FJsonObject>* Table = nullptr;
		// The belt has no day or night (its one table).
		if (!(*ZoneT)->TryGetObjectField(bNight ? TEXT("night") : TEXT("day"), Table) && !(*ZoneT)->TryGetObjectField(TEXT("day"), Table)) continue;
		const float Reach = DcTn(TEXT("countReach"), 14000.f);
		int32 Have = 0;
		for (const TWeakObjectPtr<AWRCreature>& C : Spawned) if (C.IsValid() && FVector::Dist(C->GetActorLocation(), Pl.At) < Reach) ++Have;
		const int32 Want = FMath::RoundToInt(DcTn(TEXT("perPlayer"), 10.f) * Mult);
		if (Have >= Want) continue;
		float Sum = 0.f;
		TArray<TPair<FName, float>> Rows;
		for (const auto& Pair : (*Table)->Values)
		{
			const FName Kind(*Pair.Key);
			const float* Ks = KindScale.Find(Kind);
			const float W = float(Pair.Value->AsNumber()) * (Ks ? *Ks : 1.f);
			if (W <= 0.f) continue;
			Rows.Add(TPair<FName, float>(Kind, W));
			Sum += W;
		}
		if (Sum <= 0.f) continue;
		float Roll = FMath::FRandRange(0.f, Sum);
		FName Kind;
		for (const auto& R : Rows) { Roll -= R.Value; if (Roll <= 0.f) { Kind = R.Key; break; } }
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
		FVector Spot;
		if (!Def || !FindSpot(Kind, Zone, Pl.At, Pl.Facing, Spot)) continue;
		bool bSeen = false;
		for (const FWho& O : Players) if (FVector::Dist(O.At, Spot) < DcTn(TEXT("spawnUnseen"), 5000.f)) bSeen = true;
		if (bSeen) continue;
		// The belt's packs are bigger (the jackals hunt by scent in the storm: 4).
		const int32 PackMax = Zone == TEXT("D5") && Kind == TEXT("duskjackal") ? FMath::Max(Def->PackMax, 4) : Def->PackMax;
		const int32 Pack = FMath::Clamp(FMath::RandRange(Def->PackMin, PackMax), 1, FMath::Max(1, Want - Have));
		for (int32 n = 0; n < Pack; ++n)
		{
			FVector At = Spot + FVector(FMath::RandPointInCircle(DcTn(TEXT("packSpread"), 500.f)), 0.f);
			float G = 0.f;
			if (Def->Archetype != TEXT("flyer") && WRLand::GroundUnder(At, G)) At.Z = G + Def->HalfHeight + 30.f;
			SpawnOne(Kind, At, Zone);
		}
	}
}

void UWRDesertLife::StepBeats()
{
	const double T = DcNow(this);
	const bool bNight = DcNight(this);
	const float H = DcHour(this);
	for (const TWeakObjectPtr<UWRDesertBody>& W : UWRDesertBody::Live)
	{
		UWRDesertBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || !C->IsAlive() || C->IsTamed() || C->GetAggroTarget()) continue;
		// Dunejaws hunt Shimmerbucks at the dune troughs (the fin, then the burst).
		if (C->CreatureId == TEXT("dunejaw") && FMath::FRand() < DcTn(TEXT("beats.huntChance"), 0.05f))
		{
			AWRCreature* Prey = nullptr;
			DcEachKind(GetWorld(), TEXT("shimmerbuck"), C->GetActorLocation(), DcTn(TEXT("beats.huntReach"), 4000.f), [&](AWRCreature* S, UWRDesertBody*) { if (!Prey) Prey = S; });
			if (Prey) C->SetAggroTarget(Prey);
		}
		// Duskjackals harry Dromad stragglers at night.
		if (C->CreatureId == TEXT("duskjackal") && bNight && FMath::FRand() < DcTn(TEXT("beats.harryChance"), 0.05f))
		{
			AWRCreature* Prey = nullptr;
			DcEachKind(GetWorld(), TEXT("dromad"), C->GetActorLocation(), DcTn(TEXT("beats.huntReach"), 4000.f), [&](AWRCreature* S, UWRDesertBody*) { if (!Prey) Prey = S; });
			if (Prey) C->SetAggroTarget(Prey);
		}
		// The Sandhens fly to water at dawn (and the soaked males back to the chicks after).
		if (C->CreatureId == TEXT("sandhen") && H >= 6.f && H < 7.f && FMath::FRand() < 0.2f)
		{
			const FWRDesertData& D = FWRDesertData::Get();
			FVector Best = B->Home;
			float BestD = 1.0e12f;
			for (const FWRDesertData::FPlace& K : D.Kit) if (K.Kind == TEXT("seep")) { const float Dd = FVector::DistSquared(K.At, C->GetActorLocation()); if (Dd < BestD) { BestD = Dd; Best = K.At; } }
			if (BestD < FMath::Square(DcTn(TEXT("beats.waterReach"), 30000.f))) C->SetHome(Best + FVector(FMath::RandPointInCircle(400.f), 0.f));
		}
	}
	// At every dawn Ahrumet's chest sings (the Memnon note, heard 2 km across the desert) and every Dromad within 2 km turns
	// its head east.
	const AWRGameState* GS = AWRGameState::Get(this);
	const int32 Day = GS ? int32(GS->GetTotalHours() / 24.0) : -1;
	if (GS && H >= 6.f && H < 6.5f && Day != LastDawnDay)
	{
		LastDawnDay = Day;
		const FWRDesertData::FPlace* Gn = FWRDesertData::Get().FindLandmark(TEXT("gnomon"));
		if (Gn)
		{
			if (AWRDesertState* S = AWRDesertState::Get(this)) WRSound::PlayAt(S, TEXT("desert.dawn_song"), Gn->At + FVector(0, 0, 1500.f));
			DcEachKind(GetWorld(), TEXT("dromad"), Gn->At, DcTn(TEXT("beats.songReach"), 200000.f), [&](AWRCreature*, UWRDesertBody* B) { B->FaceEastUntil = T + DcTn(TEXT("beats.faceEastSeconds"), 6.f); });
		}
	}
}

void UWRDesertLife::NoteDeath(AWRCreature* Dead, AActor* Killer)
{
	if (!Dead) return;
	// Kites circle every kill (cook 2.12.3): those within 300 m come and circle the body for a minute.
	DcEachKind(GetWorld(), TEXT("carrion_kite"), Dead->GetActorLocation(), DcTn(TEXT("beats.kiteReach"), 30000.f), [&](AWRCreature* K, UWRDesertBody* B)
	{
		if (K->GetAggroTarget() || B->CircleOver.IsValid()) return;
		B->CircleOver = Dead;
		B->CircleStart = DcNow(Dead);
		B->CircleUntil = DcNow(Dead) + DcTn(TEXT("beats.killCircleSeconds"), 60.f);
	});
}
