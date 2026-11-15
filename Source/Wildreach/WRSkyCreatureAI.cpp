// The Sky islands' creatures: their brains (FWRSkyAI) and the living world (UWRSkyLife). See WRSkyCreatures.h;
// cooks/biome-sky.md 2.9, build-biome-sky.md 2.11. Numbers: tuning skyLife.

#include "WRSkyCreatures.h"
#include "Wildreach.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRProjectile.h"
#include "WRSkyAir.h"
#include "WRSkyIsleTypes.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace WRSkyCrAI
{
	float SlTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("skyLife.%s"), Key), Fallback); }
	double SlNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	bool SlNight(const UObject* W) { return AWRGameState::IsNightIn(W); }
	bool SlFog(const UObject* W)
	{
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WRSkyIsles::Biome());
		return K && K->Id == TEXT("fog");
	}
	bool SlGale(const UObject* W)
	{
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WRSkyIsles::Biome());
		return K && K->Id == TEXT("gale");
	}
	const FWRAttackRow* SlRow(const AWRCreature* Self, FName Id)
	{
		const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
		return Rows ? Rows->Find(Id) : nullptr;
	}
	/** A survivor's eyes: is P on their screen (inside ~55 degrees of where they face)? */
	bool SlOnScreen(const AWRCharacterBase* Who, const FVector& P)
	{
		const AController* Ctl = Who ? Who->GetController() : nullptr;
		const FRotator View = Ctl ? Ctl->GetControlRotation() : Who ? Who->GetActorRotation() : FRotator::ZeroRotator;
		return Who && FVector::DotProduct(View.Vector(), (P - Who->GetActorLocation()).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(SlTn(TEXT("screenHalfAngle"), 55.f)));
	}
	/** Ground under P within Below cm (an island's top or the cape), else false. */
	bool SlGroundBelow(const FVector& P, float Below, float& OutZ)
	{
		float Z = 0.f;
		if (!WRSkyIsles::GroundUnder(P, Z)) return false;
		OutZ = Z;
		return P.Z - Z <= Below;
	}
	/** The island a body is standing over (NAME_None in open air). */
	FName SlIslandOf(const FVector& P)
	{
		const FWRSkyIsleData::FIsland* I = FWRSkyIsleData::Get().IslandAt(P, 0.f, 3000.f, 1500.f);
		return I ? I->Id : NAME_None;
	}
	/** The thorn trees and kite stakes a Gallowshrike pins its kills to (the Larder Tree, Kite Barrow's stakes). */
	bool SlNearestThorn(const FVector& From, float Reach, FVector& Out)
	{
		const FWRSkyIsleData& D = FWRSkyIsleData::Get();
		float Best = Reach;
		bool bAny = false;
		for (const FWRSkyIsleData::FSite& S : D.Sites)
		{
			if (S.Kind != TEXT("larder_tree") && S.Kind != TEXT("kite_graveyard") && S.Kind != TEXT("kite_field") && S.Kind != TEXT("shrine_barrow")) continue;
			// The tree's trunk and the stakes stand round the site's point; the nearest of a ring of 8 at 4 m.
			for (int32 k = 0; k < 9; ++k)
			{
				const FVector P = k == 0 ? S.At : S.At + FVector(FMath::Cos(k * PI / 4.f) * 400.f, FMath::Sin(k * PI / 4.f) * 400.f, 0.f);
				float Gz = P.Z;
				WRSkyIsles::GroundUnder(P + FVector(0, 0, 3000.f), Gz);
				const FVector G(P.X, P.Y, Gz);
				const float Dd = FVector::Dist(G, From);
				if (Dd < Best) { Best = Dd; Out = G; bAny = true; }
			}
		}
		return bAny;
	}
	void SlTell(AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
}
using namespace WRSkyCrAI;

// ================================================================================================ holds

AWRCreature* FWRSkyAI::HolderOf(const AActor* Victim)
{
	for (const TWeakObjectPtr<UWRSkyBody>& B : UWRSkyBody::Live)
	{
		if (B.IsValid() && B->Holding == Victim) return Cast<AWRCreature>(B->GetOwner());
	}
	return nullptr;
}

bool FWRSkyAI::Floats(const AWRCreature* Self)
{
	if (!Self) return false;
	const FWRCreatureDef* Def = Self->GetDef();
	const FName K = Self->CreatureId;
	return (Def && Def->Archetype == TEXT("flyer")) || K == TEXT("cloudback");
}

void FWRSkyAI::StartHold(AWRCreature* Self, UWRSkyBody* Body, AWRCharacterBase* Victim, FName Kind, float Seconds)
{
	if (!Self || !Body || !Victim || !Victim->IsAlive() || HolderOf(Victim)) return;
	Body->HeldBody = Victim;
	Body->Holding = Victim;
	Body->HoldKind = Kind;
	Body->HoldUntil = SlNow(Self) + Seconds;
	Body->HoldTickAt = SlNow(Self) + 1.0;
	Body->FreeStance = 0.f;
	Body->Set(WRSkyFlag::Holding, true);
	if (Victim->Status) Victim->Status->Stagger(Seconds + 0.1f);
	if (Kind == TEXT("impale"))
	{
		// Slammed onto the nearest thorn or stake within 6 m: bleed 60, pinned 1.5 s. None near: thrown 4 m instead.
		FVector Thorn;
		if (SlNearestThorn(Victim->GetActorLocation(), SlTn(TEXT("gallowshrike.thornReach"), 600.f), Thorn))
		{
			Body->PinAt = Thorn + FVector(0.f, 0.f, Victim->GetSimpleCollisionHalfHeight() + 40.f);
			FWRHit Hit;
			Hit.Instigator = Self;
			Hit.Damage = 0.f;
			Hit.bUnblockable = true;
			Hit.bDamageOverTime = true;
			Hit.Buildups.Add(TEXT("bleed"), SlTn(TEXT("gallowshrike.impaleBleed"), 60.f));
			Hit.Origin = Self->GetActorLocation();
			Hit.Element = WRTags::Element_Physical;
			UWRCombatLibrary::ApplyHit(Victim, Hit);
			Body->MulticastMoment(TEXT("hit.blood"), TEXT("head"), 1.4f, TEXT("creature.gallowshrike.signature"));
		}
		else
		{
			Body->HeldBody = nullptr;
			Body->Holding = nullptr;
			Body->Set(WRSkyFlag::Holding, false);
			const FVector Dir = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
			WRSkyAir::Push(Victim, Dir * SlTn(TEXT("gallowshrike.throwSpeed"), 1000.f) + FVector(0, 0, 300.f), 0.4f, TEXT("throw"), Self);
			return;
		}
	}
	else if (Kind == TEXT("smother"))
	{
		Body->Set(WRSkyFlag::Mantle, true);
		Body->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("creature.hushwing.signature"));
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY %s holds %s (%s, %.1f s)"), *Self->GetName(), *Victim->GetName(), *Kind.ToString(), Seconds);
}

void FWRSkyAI::Release(AWRCreature* Self, UWRSkyBody* Body, bool bThrow)
{
	AWRCharacterBase* V = Body->HeldBody.Get();
	const FName Kind = Body->HoldKind;
	Body->HeldBody = nullptr;
	Body->Holding = nullptr;
	Body->Set(WRSkyFlag::Holding, false);
	Body->Set(WRSkyFlag::Mantle, false);
	if (!V) return;
	if (V->Status) V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive() && Self)
	{
		const FVector Dir = (V->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
		WRSkyAir::Push(V, Dir * 600.f + FVector(0, 0, 200.f), 0.3f, TEXT("throw"), Self);
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY %s lets go of %s (%s)"), Self ? *Self->GetName() : TEXT("?"), *V->GetName(), *Kind.ToString());
}

void FWRSkyAI::Mash(AWRCharacterBase* Victim)
{
	AWRCreature* Holder = HolderOf(Victim);
	UWRSkyBody* Body = UWRSkyBody::Get(Holder);
	if (!Body) return;
	Body->HoldUntil -= Body->HoldKind == TEXT("impale") ? SlTn(TEXT("gallowshrike.mash"), 0.15f) : SlTn(TEXT("hushwing.mash"), 0.15f);
}

// ================================================================================================ the body (server)

void FWRSkyAI::TickBody(AWRCreature* Self, UWRSkyBody* Body, float DeltaTime)
{
	const double T = SlNow(Self);
	if (!Self->IsAlive()) return;
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	const FName K = Self->CreatureId;

	// A hold: pinned to the thorn (impale) or wrapped in the wings (smother); its ticks through ApplyHit; freed by time,
	// mashing or an ally's stance damage on the holder.
	if (AWRCharacterBase* V = Body->HeldBody.Get())
	{
		const bool bSmother = Body->HoldKind == TEXT("smother");
		const float Free = bSmother ? SlTn(TEXT("hushwing.freeStance"), 60.f) : 1.0e9f;
		if (!V->IsAlive() || T >= Body->HoldUntil || Body->FreeStance >= Free) Release(Self, Body, false);
		else
		{
			if (Body->HoldKind == TEXT("impale")) V->SetActorLocation(FMath::VInterpTo(V->GetActorLocation(), Body->PinAt, DeltaTime, 12.f), false);
			if (bSmother) V->SetActorLocation(FMath::VInterpTo(V->GetActorLocation(), Self->GetActorLocation() + Self->GetActorForwardVector() * 80.f, DeltaTime, 10.f), true);
			if (bSmother && T >= Body->HoldTickAt)
			{
				Body->HoldTickAt = T + 1.0;
				FWRHit Hit;
				Hit.Instigator = Self;
				Hit.Damage = SlTn(TEXT("hushwing.smotherDps"), 8.f);
				Hit.bUnblockable = true;
				Hit.bMelee = true;
				Hit.Origin = Self->GetActorLocation();
				Hit.Element = WRTags::Element_Physical;
				UWRCombatLibrary::ApplyHit(V, Hit);
				if (V->Status) V->Status->Stagger(1.1f);
			}
		}
	}
	// The wind-ups that move the body: the stoop's climb to its fold point (up and back of the target), the belly-flop's
	// 15 m rise over where its shadow lies.
	if (AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController()); AI && (AI->RowPhase == 1 || AI->RowPhase == 2))
	{
		const FName Rid = AI->RowId;
		if (Rid == TEXT("stoop") || Rid == TEXT("meat_theft") || Rid == TEXT("silent_swoop") || Rid == TEXT("silent_swoop_read"))
		{
			if (!Body->StoopFrom.IsZero() && !Body->Has(WRSkyFlag::Stoop))
			{
				const FVector D = Body->StoopFrom - Self->GetActorLocation();
				const double Left = FMath::Max(0.15, AI->RowStarted + (SlRow(Self, Rid) ? SlRow(Self, Rid)->Windup : 0.8f) - T);
				Move->SetMovementMode(MOVE_Flying);
				Move->Velocity = D.GetSafeNormal() * FMath::Min(float(D.Size() / Left), Self->GetDef() ? Self->GetDef()->Speed * 1.4f : 2000.f);
				if (AWRCharacterBase* Tg = AI->RowTarget.Get()) Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), (Tg->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D().Rotation(), DeltaTime, 6.f));
			}
		}
		else if (Rid == TEXT("belly_flop"))
		{
			Move->SetMovementMode(MOVE_Flying);
			Move->Velocity = FVector(0.f, 0.f, SlTn(TEXT("skyray.flopRise"), 1500.f));
		}
	}
	// Down after a stoop or a swoop: back up when the window closes.
	if (Body->Has(WRSkyFlag::Grounded) && T >= Body->GroundedUntil)
	{
		Body->Set(WRSkyFlag::Grounded, false);
		Body->Set(WRSkyFlag::Flap, false);
		if (Floats(Self)) Move->SetMovementMode(MOVE_Flying);
	}
	// The stoop: down the locked line, never homing (the line was fixed at the fold).
	if (Body->Has(WRSkyFlag::Stoop))
	{
		const FVector Line = Body->StoopTo - Body->StoopFrom;
		const float Speed = FMath::Min(SlTn(TEXT("galeharrier.stoopSpeed"), 3600.f), Line.Size() / FMath::Max(0.1f, SlTn(TEXT("stoopSeconds"), 0.32f)));
		Move->SetMovementMode(MOVE_Flying);
		Move->Velocity = Line.GetSafeNormal() * Speed;
		if (FVector::DotProduct(Self->GetActorLocation() - Body->StoopTo, Line) >= 0.f || T - Body->StoopStart > 1.2)
		{
			// The line is spent: down on whatever's under it (the bring-down window), or hanging on its wings in open air.
			Body->Set(WRSkyFlag::Stoop, false);
			Body->Set(WRSkyFlag::Crest, false);
			Move->Velocity *= 0.1f;
			float Gz = 0.f;
			const bool bGround = SlGroundBelow(Self->GetActorLocation(), SlTn(TEXT("groundBelow"), 600.f), Gz);
			Body->Set(WRSkyFlag::Grounded, true);
			Body->GroundedUntil = T + (K == TEXT("hushwing") ? SlTn(TEXT("hushwing.grounded"), 1.4f) : SlTn(TEXT("galeharrier.grounded"), 1.5f));
			if (bGround) Move->SetMovementMode(MOVE_Falling);
			else Body->Set(WRSkyFlag::Flap, true);
		}
	}
	// A Puffwing off an edge: wings spread, it parachutes (slow fall), then walks home after 120 s (it never dies of it).
	if (K == TEXT("puffwing"))
	{
		float Gz = 0.f;
		const bool bAir = Move->IsFalling() && !SlGroundBelow(Self->GetActorLocation(), SlTn(TEXT("puffwing.parachuteAbove"), 300.f), Gz);
		if (bAir && !Body->Has(WRSkyFlag::Parachute))
		{
			Body->Set(WRSkyFlag::Parachute, true);
			Body->ParachuteSince = T;
			Move->GravityScale = SlTn(TEXT("puffwing.gravity"), 0.06f);
			Move->Velocity.Z = FMath::Max(Move->Velocity.Z, -SlTn(TEXT("puffwing.sink"), 250.f));
			Body->MulticastMoment(TEXT("sky.feathers"), TEXT("spine"), 1.f, TEXT("creature.puffwing.signature"));
		}
		if (Body->Has(WRSkyFlag::Parachute))
		{
			Move->Velocity.Z = FMath::Max(Move->Velocity.Z, -SlTn(TEXT("puffwing.sink"), 250.f));
			if (Move->IsMovingOnGround()) { Body->Set(WRSkyFlag::Parachute, false); Move->GravityScale = 1.f; }
		}
		if (Body->ParachuteSince > 0.0 && T - Body->ParachuteSince > SlTn(TEXT("puffwing.homeAfter"), 120.f) && SlIslandOf(Self->GetActorLocation()) != Body->HomeIsland && !Body->Has(WRSkyFlag::Parachute))
		{
			// Home again where nobody watches (its burrow on its own island).
			bool bSeen = false;
			for (FConstPlayerControllerIterator It = Self->GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
				if (P && (FVector::Dist(P->GetActorLocation(), Self->GetActorLocation()) < 6000.f || FVector::Dist(P->GetActorLocation(), Body->HomeNode) < 6000.f)) bSeen = true;
			}
			if (!bSeen)
			{
				Self->SetActorLocation(Body->HomeNode + FVector(0, 0, Self->GetSimpleCollisionHalfHeight() + 20.f), false, nullptr, ETeleportType::TeleportPhysics);
				Self->SetAggroTarget(nullptr);
				Body->ParachuteSince = 0.0;
			}
		}
	}
	// A Cloudback floats 2 m over what it's above (the lagoon, the garden), always.
	if (K == TEXT("cloudback"))
	{
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		float Gz = 0.f;
		const FVector P = Self->GetActorLocation();
		const float Want = (WRSkyIsles::GroundUnder(P, Gz) ? Gz : Body->HomeNode.Z) + SlTn(TEXT("cloudback.float"), 200.f) + Self->GetSimpleCollisionHalfHeight();
		Move->Velocity.Z = FMath::Clamp((Want - P.Z) * 1.5f, -300.f, 300.f);
	}
	// A Skyray's breach: out of the cloud 20 m and a belly-flop back (2.4 s, a pure arc).
	if (Body->Has(WRSkyFlag::Breach))
	{
		const float S = float(T - Body->BreachStart) / SlTn(TEXT("skyray.breachSeconds"), 2.4f);
		if (S >= 1.f)
		{
			Body->Set(WRSkyFlag::Breach, false);
			Self->SetActorLocation(Body->BreachFrom, true);
			Body->MulticastMoment(TEXT("sky.cloud_splash"), NAME_None, 2.f, TEXT("creature.skyray.splash"));
		}
		else
		{
			const float H = SlTn(TEXT("skyray.breachHeight"), 2000.f) * 4.f * S * (1.f - S);
			Self->SetActorLocation(Body->BreachFrom + Self->GetActorForwardVector() * 600.f * S + FVector(0, 0, H), false);
		}
	}
	// A Windmane up on its takeoff gust: 4 s in the air, then down again with a charge at whoever it was.
	if (K == TEXT("windmane") && Body->AirborneUntil > 0.0)
	{
		if (T < Body->AirborneUntil)
		{
			Body->Set(WRSkyFlag::Flying, true);
			Move->SetMovementMode(MOVE_Flying);
			Move->Velocity.Z = FMath::Max(Move->Velocity.Z, T - (Body->AirborneUntil - 4.0) < 1.5 ? 500.f : 0.f);
		}
		else
		{
			Body->AirborneUntil = 0.0;
			Body->Set(WRSkyFlag::Flying, false);
			Move->SetMovementMode(MOVE_Falling);
			if (AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController()))
			{
				if (AWRCharacterBase* Tg = Cast<AWRCharacterBase>(Self->GetAggroTarget())) AI->StartRow(Self, Tg, TEXT("antler_charge"), true);
			}
		}
	}
	// The Sailbell's curtain: anyone passing under it (12 m down, 2.5 m round) is stung, once a second at most.
	if (K == TEXT("sailbell") && T >= Body->NextStingAt)
	{
		Body->NextStingAt = T + 0.25;
		const float R = SlTn(TEXT("sailbell.curtainRadius"), 250.f), Down = SlTn(TEXT("sailbell.curtainLength"), 1200.f);
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive() || !Self->IsHostileTo(V) || (V->IsA<AWRCreature>() && UWRSkyBody::Get(V))) continue;
			const FVector D = V->GetActorLocation() - Self->GetActorLocation();
			if (D.Size2D() > R + V->GetSimpleCollisionRadius() || D.Z > 100.f || D.Z < -Down) continue;
			double& Last = Body->StungAt.FindOrAdd(V);
			if (T - Last < 1.0) continue;
			Last = T;
			FWRHit Hit;
			Hit.Instigator = Self;
			Hit.Damage = Self->Weapon.FlatDamage * SlTn(TEXT("sailbell.touch"), 0.5f);
			Hit.bUnblockable = true;
			Hit.bMelee = true;
			Hit.Origin = V->GetActorLocation() + FVector(0, 0, 100.f);
			Hit.Element = WRTags::Element_Physical;
			UWRCombatLibrary::ApplyHit(V, Hit);
			if (V->Status) V->Status->ApplyStatus(WRTags::Status_Slowed, SlTn(TEXT("sailbell.slowSeconds"), 3.f));
			WRSound::PlayAt(Self, TEXT("creature.sailbell.sting"), V->GetActorLocation());
		}
	}
}

// ================================================================================================ the wild brain

bool FWRSkyAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return false;
	const double T = SlNow(Self);
	const FName K = Self->CreatureId;
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();
	// No row under way (the row machine runs first): a row's own looks are over (the lobes rolled, the lash, the mantle...).
	if (AI->RowPhase == 0)
	{
		Body->Set(WRSkyFlag::Undertow, false);
		Body->Set(WRSkyFlag::Lobes, false);
		Body->Set(WRSkyFlag::Lash, false);
		Body->Set(WRSkyFlag::Mantle, Body->HeldBody.IsValid() && Body->HoldKind == TEXT("smother"));
		Body->Set(WRSkyFlag::Spout, false);
		Body->Set(WRSkyFlag::Hood, false);
		Body->Set(WRSkyFlag::Gape, false);
		if (!Body->Has(WRSkyFlag::Stoop)) Body->Set(WRSkyFlag::Crest, false);
		if (!Body->Has(WRSkyFlag::Grounded) && K != TEXT("gallowshrike")) Body->Set(WRSkyFlag::Flap, false);
	}
	if (Body->HeldBody.IsValid() || Body->Has(WRSkyFlag::Stoop) || Body->Has(WRSkyFlag::Breach) || Body->Has(WRSkyFlag::Parachute)) return true;
	if (Body->Has(WRSkyFlag::Grounded))
	{
		Move->StopMovementImmediately();
		// A Hushwing down after its swoop snaps from where it sits (the gape), at what's in reach.
		AWRCharacterBase* Tg = Cast<AWRCharacterBase>(Self->GetAggroTarget());
		if (K == TEXT("hushwing") && Tg && FVector::Dist(Tg->GetActorLocation(), Self->GetActorLocation()) < 260.f && AI->RowReadyAt.FindRef(TEXT("gape_snap")) <= T)
		{
			AI->StartRow(Self, Tg, TEXT("gape_snap"), false);
		}
		return true;
	}
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	const FWRCreatureDef* Def = Self->GetDef();

	// The Sailbell never chases: it drifts at 45 degrees to the wind (its hand) at its height, lashes what comes under.
	if (K == TEXT("sailbell"))
	{
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		const FVector2D Wind = WRSkyIsles::WindDir(Self);
		const float Hand = Body->Has(WRSkyFlag::LeftHand) ? 1.f : -1.f;
		const FVector2D Drift = Wind.GetRotated(Hand * SlTn(TEXT("sailbell.driftAngle"), 45.f));
		Self->SpeedBoost = 1.f;
		Self->AddMovementInput(FVector(Drift, 0.f), SlTn(TEXT("sailbell.driftInput"), 0.8f));
		Move->Velocity.Z = FMath::Clamp((Body->HomeNode.Z - Self->GetActorLocation().Z) * 0.5f, -80.f, 80.f);
		Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Drift.Y, Drift.X)) + Hand * 45.f, 0.f), DeltaSeconds, 0.5f));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive() || !Self->IsHostileTo(V) || (V->IsA<AWRCreature>() && UWRSkyBody::Get(V))) continue;
			const FVector D = V->GetActorLocation() - Self->GetActorLocation();
			if (D.Size2D() < SlTn(TEXT("sailbell.lashReach"), 800.f) && D.Z < 200.f && D.Z > -1300.f && AI->RowReadyAt.FindRef(TEXT("lash")) <= T)
			{
				Self->SetAggroTarget(V);
				AI->StartRow(Self, V, TEXT("lash"), false);
				break;
			}
		}
		return true;
	}

	// Mistlarks: a flock circling its column (a free lift); they burst away from anything close and fast.
	if (K == TEXT("mistlark"))
	{
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		const bool bScatter = Body->Has(WRSkyFlag::Chased) && T < Body->GroundedUntil;
		if (!bScatter) Body->Set(WRSkyFlag::Chased, false);
		for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It && !bScatter; ++It)
		{
			if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < SlTn(TEXT("mistlark.startle"), 800.f) && It->GetVelocity().Size() > 300.f)
			{
				Body->Set(WRSkyFlag::Chased, true);
				Body->GroundedUntil = T + 4.0;
				Body->MulticastMoment(NAME_None, NAME_None, 1.f, TEXT("creature.mistlark.signature"));
			}
		}
		if (Body->Has(WRSkyFlag::Chased))
		{
			Self->AddMovementInput((Self->GetActorLocation() - Body->HomeNode).GetSafeNormal2D() + FVector(0, 0, 0.6f), 1.f);
			return true;
		}
		// By night they roost in the cedars on the column's island.
		if (SlNight(Self) && !Body->HomeIsland.IsNone())
		{
			const FVector Roost = Body->HomeNode - FVector(0, 0, Body->RingHeight);
			if (FVector::Dist(Self->GetActorLocation(), Roost) > 300.f) { Self->AddMovementInput((Roost - Self->GetActorLocation()).GetSafeNormal(), 0.7f); return true; }
			Move->StopMovementImmediately();
			return true;
		}
	}

	// The flyer rule: no target, it rides a ring over its home node; flyers pick a gliding survivor first; a 150 m leash.
	const bool bFlyer = Def && Def->Archetype == TEXT("flyer");
	if (bFlyer && (K == TEXT("galeharrier") || K == TEXT("hushwing")))
	{
		// Hushwings: the night (or Cloud fog by day) above 700 m; by day they sleep in their roost.
		const bool bHushAwake = K != TEXT("hushwing") || SlNight(Self) || SlFog(Self);
		if (K == TEXT("hushwing"))
		{
			if (!bHushAwake && !Target)
			{
				// Dawn: home to its roost first (the shelf, the cave), then asleep there till dusk.
				const FVector D = Body->HomeNode - Self->GetActorLocation();
				if (D.Size() > 250.f)
				{
					Body->Set(WRSkyFlag::Asleep, false);
					if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
					Self->AddMovementInput(D.GetSafeNormal(), 0.8f);
					return true;
				}
				Body->Set(WRSkyFlag::Asleep, true);
				Move->StopMovementImmediately();
				return true;
			}
			Body->Set(WRSkyFlag::Asleep, false);
		}
		for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacter* S = *It;
			if (!S->IsAlive() || !Self->IsHostileTo(S) || !WRSkyAir::IsGliding(S)) continue;
			if (FVector::Dist(S->GetActorLocation(), Self->GetActorLocation()) < Def->AggroRadius && Target != S)
			{
				Self->SetAggroTarget(S);
				Target = S;
				break;
			}
		}
		// Meat in the bag of anyone near: the Galeharrier comes for it (the robbery).
		if (K == TEXT("galeharrier") && !Target && T - Body->LastStolenAt > SlTn(TEXT("galeharrier.robEvery"), 60.f))
		{
			for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
			{
				const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(*It);
				if (It->IsAlive() && Inv && Inv->CountOf(TEXT("raw_meat")) > 0 && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < SlTn(TEXT("galeharrier.smellMeat"), 3000.f))
				{
					Self->SetAggroTarget(*It);
					Target = *It;
					break;
				}
			}
		}
	}
	if (Target && FVector::Dist(Self->GetActorLocation(), Body->HomeNode) > (bFlyer ? SlTn(TEXT("flyerLeash"), 15000.f) : SlTn(TEXT("groundLeash"), 4000.f)))
	{
		Self->SetAggroTarget(nullptr);
		Target = nullptr;
	}
	// A fighting flyer keeps the default chase's home with it (its own 150 m leash is the one above).
	if (Target && bFlyer) Self->SetHome(Self->GetActorLocation());

	// Ground creatures never path off their island: a target on another island (or in the air) is watched from the lip.
	if (Target && !bFlyer && K != TEXT("cloudback") && K != TEXT("windmane") && K != TEXT("gallowshrike"))
	{
		if (SlIslandOf(Target->GetActorLocation()) != SlIslandOf(Self->GetActorLocation()))
		{
			Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D().Rotation(), DeltaSeconds, 4.f));
			return true;
		}
	}
	// A Gallowshrike only crosses to another island by its hop-glide (40 m at most, level or down).
	if (Target && K == TEXT("gallowshrike") && SlIslandOf(Target->GetActorLocation()) != SlIslandOf(Self->GetActorLocation()))
	{
		const FVector D = Target->GetActorLocation() - Self->GetActorLocation();
		if (D.Size2D() < SlTn(TEXT("gallowshrike.hopReach"), 4000.f) && D.Z < 300.f && AI->RowReadyAt.FindRef(TEXT("hop_pounce")) <= T) AI->StartRow(Self, Target, TEXT("hop_pounce"), false);
		return true;
	}
	// A Windmane in the air after someone on another island: it flies (all four wings), steering up or down to them.
	if (Target && K == TEXT("windmane"))
	{
		const bool bAway = SlIslandOf(Target->GetActorLocation()) != SlIslandOf(Self->GetActorLocation());
		if (bAway && Move->MovementMode != MOVE_Flying) { Move->SetMovementMode(MOVE_Flying); Body->Set(WRSkyFlag::Flying, true); }
		if (Move->MovementMode == MOVE_Flying)
		{
			const FVector D = Target->GetActorLocation() - Self->GetActorLocation();
			Self->AddMovementInput(FVector(0.f, 0.f, FMath::Clamp(D.Z / 400.f, -1.f, 1.f)), 1.f);
			if (!bAway && D.Size2D() < 600.f) { Move->SetMovementMode(MOVE_Falling); Body->Set(WRSkyFlag::Flying, false); }
		}
	}
	if (Target) return false;   // the row fight (TickRowFight) takes it from here

	// No target: home. Flyers ride their ring; a Skyray drifts its figure-eight off the lip; ground ones graze on their island.
	if (bFlyer || K == TEXT("cloudback"))
	{
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		Body->RingPhase += DeltaSeconds * Body->RingDir * (SlTn(TEXT("ringSpeed"), 650.f) / FMath::Max(800.f, Body->RingRadius));
		const bool bEight = K == TEXT("skyray");
		const float A = Body->RingPhase;
		const FVector2D Off = bEight ? FVector2D(FMath::Sin(A), FMath::Sin(A) * FMath::Cos(A)) * Body->RingRadius : FVector2D(FMath::Cos(A), FMath::Sin(A)) * Body->RingRadius;
		const float Bob = (K == TEXT("galeharrier") ? 300.f : 120.f) * FMath::Sin(A * 0.5f);
		const FVector Want = Body->HomeNode + FVector(Off, (K == TEXT("cloudback") ? 0.f : Bob) + Body->RingLift);
		const FVector D = Want - Self->GetActorLocation();
		const float Pace = K == TEXT("skyray") ? 0.5f : K == TEXT("cloudback") ? 0.35f : 0.8f;
		Self->SpeedBoost = 1.f;
		Self->AddMovementInput(D.GetSafeNormal(), Pace);
		if (D.Size() > 200.f) Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), D.GetSafeNormal2D().Rotation(), DeltaSeconds, 2.f));
		Self->SetHome(Body->HomeNode);
		return true;
	}
	// A Windmane away from its home (the dawn grazing, or back to the ledges): it flies there and lands.
	if (K == TEXT("windmane"))
	{
		const FVector D = Body->HomeNode - Self->GetActorLocation();
		if (D.Size2D() > SlTn(TEXT("windmane.landNear"), 2000.f) || FMath::Abs(D.Z) > 800.f)
		{
			if (Move->MovementMode != MOVE_Flying) { Move->SetMovementMode(MOVE_Flying); Body->Set(WRSkyFlag::Flying, true); }
			Self->AddMovementInput((D + FVector(0, 0, D.Size2D() > 3000.f ? 1500.f : 0.f)).GetSafeNormal(), 1.f);
			Self->SetActorRotation(FMath::RInterpTo(Self->GetActorRotation(), D.GetSafeNormal2D().Rotation(), DeltaSeconds, 2.f));
			return true;
		}
		if (Move->MovementMode == MOVE_Flying) { Move->SetMovementMode(MOVE_Falling); Body->Set(WRSkyFlag::Flying, false); }
		Self->SetHome(Body->HomeNode);
	}
	return false;   // grazing round home (the default wander, kept near the island's middle)
}

bool FWRSkyAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return false;
	Body->Set(WRSkyFlag::Asleep, false);
	// A tame Mistlark flies to the nearest updraft column within 300 m and circles it (your eyes, no HUD); a tame
	// Sailbell tethered at home stings anything hostile within 10 m (its curtain does the stinging in TickBody).
	if (Self->CreatureId == TEXT("mistlark") && !Self->GetRider())
	{
		const FWRSkyIsleData& D = FWRSkyIsleData::Get();
		const FVector P = Self->GetActorLocation();
		const FWRSkyIsleData::FUpdraft* Best = nullptr;
		float BestD = SlTn(TEXT("mistlark.tameReach"), 30000.f);
		for (const FWRSkyIsleData::FUpdraft& U : D.Updrafts)
		{
			const float Dd = FVector2D::Distance(FVector2D(P), U.At);
			if (Dd < BestD) { BestD = Dd; Best = &U; }
		}
		if (Best)
		{
			UCharacterMovementComponent* Move = Self->GetCharacterMovement();
			if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
			Body->RingPhase += DeltaSeconds * 0.8f;
			const FVector Want(Best->At + FVector2D(FMath::Cos(Body->RingPhase), FMath::Sin(Body->RingPhase)) * Best->Radius * 0.5f, FMath::Min(Best->Top - 1500.f, P.Z + 300.f));
			Self->AddMovementInput((Want - P).GetSafeNormal(), 0.9f);
			return true;
		}
	}
	return false;
}

// ================================================================================================ rows

bool FWRSkyAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return true;
	const FName Id = Row.Id;
	if (Body->Has(WRSkyFlag::Grounded) && Id != TEXT("gape_snap")) return false;
	// The meat theft: only on a survivor with raw meat on them.
	if (Id == TEXT("meat_theft"))
	{
		const UWRInventoryComponent* Inv = Target ? UWRInventoryComponent::FindFor(Target) : nullptr;
		return Inv && Inv->CountOf(TEXT("raw_meat")) > 0;
	}
	// The Hushwing's swoop starts only on the survivor's screen or within 8 m (its churr gives the side).
	if (Id == TEXT("silent_swoop") || Id == TEXT("silent_swoop_read")) return Target && (SlOnScreen(Target, Self->GetActorLocation()) || FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < SlTn(TEXT("hushwing.churrRange"), 800.f));
	if (Id == TEXT("gape_snap")) return Body->Has(WRSkyFlag::Grounded) || (Target && Self->GetCharacterMovement()->IsMovingOnGround());
	// The hop-glide pounce: from 3 m above or across open air.
	if (Id == TEXT("hop_pounce") && Target)
	{
		const bool bAbove = Self->GetActorLocation().Z - Target->GetActorLocation().Z > SlTn(TEXT("gallowshrike.pounceAbove"), 300.f);
		return bAbove || SlIslandOf(Target->GetActorLocation()) != SlIslandOf(Self->GetActorLocation());
	}
	if (Id == TEXT("rattle")) return !Body->bRattled;
	// The Windmane's hind-wing kick: what stands behind it (behindOnly); the gust once it's on the ground.
	if (Id == TEXT("takeoff_gust")) return Body->AirborneUntil <= 0.0;
	// The Cloudback spouts only with someone on or over its back.
	if (Id == TEXT("spout") && Target) return Target->GetActorLocation().Z > Self->GetActorLocation().Z;
	return true;
}

void FWRSkyAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return;
	const FName Id = Row.Id;
	if (Id == TEXT("stoop") || Id == TEXT("meat_theft"))
	{
		// The climb: up and back of the target as the crest flares (the wind-up), a rising scream.
		Body->Set(WRSkyFlag::Crest, true);
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		if (Target)
		{
			const FVector Back = (Self->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			Body->StoopFrom = Target->GetActorLocation() + Back * SlTn(TEXT("galeharrier.foldBack"), 900.f) + FVector(0, 0, SlTn(TEXT("galeharrier.foldUp"), 900.f));
		}
	}
	else if (Id == TEXT("silent_swoop") || Id == TEXT("silent_swoop_read"))
	{
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	else if (Id == TEXT("undertow") || Id == TEXT("undertow_read"))
	{
		// It hovers off the lip, the horn-lobes unroll, a rising whoosh; dust and grass pulled toward it.
		Body->Set(WRSkyFlag::Lobes, true);
		Body->Set(WRSkyFlag::Flap, true);
		Self->GetCharacterMovement()->StopMovementImmediately();
		Body->MulticastMoment(TEXT("sky.undertow_tell"), TEXT("head"), 1.f, TEXT("creature.skyray.tell"));
	}
	else if (Id == TEXT("belly_flop") && Target)
	{
		// It climbs 15 m; its shadow grows where it will come down.
		Body->Set(WRSkyFlag::Flap, true);
		WRFx::Broadcast(Self, TEXT("sky.shadow"), AI->RowAim, FRotator::ZeroRotator, 3.f, NAME_None, Row.Windup + Row.Hold);
	}
	else if (Id == TEXT("takeoff_gust")) Body->Set(WRSkyFlag::Flying, true);
	else if (Id == TEXT("hind_wing_kick")) Body->Set(WRSkyFlag::Hood, true);
	else if (Id == TEXT("spout")) { Body->Set(WRSkyFlag::Spout, true); Body->MulticastMoment(NAME_None, NAME_None, 1.f, TEXT("creature.cloudback.whistle")); }
	else if (Id == TEXT("wing_smother")) Body->Set(WRSkyFlag::Mantle, true);
	else if (Id == TEXT("lash")) Body->Set(WRSkyFlag::Lash, true);
	else if (Id == TEXT("hop_pounce")) Body->Set(WRSkyFlag::Flap, true);
	else if (Id == TEXT("gape_snap")) Body->Set(WRSkyFlag::Gape, true);
}

bool FWRSkyAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return false;
	const FName Id = Row.Id;
	const double T = SlNow(Self);
	if (Id == TEXT("stoop") || Id == TEXT("meat_theft") || Id == TEXT("silent_swoop") || Id == TEXT("silent_swoop_read"))
	{
		// The line locks at the fold: from here to where the target is now (a little low: through them), never homing.
		Body->StoopFrom = Self->GetActorLocation();
		Body->StoopTo = Target ? Target->GetActorLocation() + (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal() * 200.f : Self->GetActorLocation() + Self->GetActorForwardVector() * 1000.f;
		Body->StoopStart = T;
		Body->Set(WRSkyFlag::Stoop, true);
		Self->SetActorRotation((Body->StoopTo - Body->StoopFrom).GetSafeNormal2D().Rotation());
		Body->MulticastMoment(Id.ToString().StartsWith(TEXT("silent")) ? FName(TEXT("sky.hush_flash")) : FName(TEXT("sky.stoop_streak")), TEXT("head"), 1.f, Id.ToString().StartsWith(TEXT("silent")) ? NAME_None : FName(TEXT("creature.galeharrier.signature")));
		return true;
	}
	if (Id == TEXT("undertow") || Id == TEXT("undertow_read"))
	{
		Body->Set(WRSkyFlag::Undertow, true);
		return true;   // the pull runs in TickStrike
	}
	if (Id == TEXT("belly_flop"))
	{
		// Down at where its shadow lay.
		Body->Set(WRSkyFlag::Flap, false);
		const FVector To = AI->RowAim + FVector(0, 0, Self->GetSimpleCollisionHalfHeight());
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Self->GetCharacterMovement()->Velocity = (To - Self->GetActorLocation()).GetSafeNormal() * SlTn(TEXT("skyray.flopSpeed"), 2800.f);
		Body->StoopTo = To;
		Body->StoopStart = T;
		return true;
	}
	if (Id == TEXT("takeoff_gust"))
	{
		// A ring of air from all four wings beating down: everyone in 6 m pushed out (the knockback rides RowHit).
		const float R = Row.Radius > 0.f ? Row.Radius : 600.f;
		WRFx::Broadcast(Self, TEXT("sky.wing_gust"), Self->GetActorLocation(), FRotator::ZeroRotator, R / 300.f, TEXT("creature.windmane.wings"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < R + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		Body->AirborneUntil = T + SlTn(TEXT("windmane.airborne"), 4.f);
		return true;
	}
	if (Id == TEXT("spout"))
	{
		// Anyone on or over its back goes 6 m up.
		Body->Set(WRSkyFlag::Spout, false);
		WRFx::Broadcast(Self, TEXT("sky.spout"), Self->GetActorLocation() + FVector(0, 0, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.5f, TEXT("creature.cloudback.spout"));
		const float R = Row.Radius > 0.f ? Row.Radius : 380.f;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive()) continue;
			const FVector D = V->GetActorLocation() - Self->GetActorLocation();
			if (D.Size2D() < R && D.Z > 0.f && D.Z < 900.f) WRSkyAir::Push(V, FVector(0, 0, SlTn(TEXT("cloudback.spoutLift"), 1100.f)), 0.25f, TEXT("spout"), Self);
		}
		return true;
	}
	if (Id == TEXT("lash"))
	{
		// The curtain sweeps out 8 m below and round it.
		Body->Set(WRSkyFlag::Lash, true);
		const float R = Row.Radius > 0.f ? Row.Radius : 800.f;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive() || (V->IsA<AWRCreature>() && UWRSkyBody::Get(V))) continue;
			const FVector D = V->GetActorLocation() - Self->GetActorLocation();
			if (D.Size2D() < R + V->GetSimpleCollisionRadius() && D.Z < 200.f && D.Z > -SlTn(TEXT("sailbell.curtainLength"), 1200.f) - 100.f) AI->RowHit(Self, V);
		}
		return true;
	}
	if (Id == TEXT("rattle"))
	{
		// Packmates within 50 m come for the same one, once a fight.
		Body->bRattled = true;
		Body->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("creature.gallowshrike.rattle"));
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->CreatureId == Self->CreatureId && It->IsAlive() && !It->IsTamed() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < SlTn(TEXT("gallowshrike.rattleReach"), 5000.f) && Target)
				It->SetAggroTarget(Target);
		}
		return true;
	}
	if (Id == TEXT("hop_pounce") && Target)
	{
		// The hop-glide: wings spread, a long flat arc at where the target will be (gravity light while it glides).
		const FVector From = Self->GetActorLocation();
		const FVector To = Target->GetActorLocation() + Target->GetVelocity() * FVector(1, 1, 0) * 0.4f;
		const float G = SlTn(TEXT("gallowshrike.glideGravity"), 0.35f);
		Self->GetCharacterMovement()->GravityScale = G;
		Self->SetActorRotation((To - From).GetSafeNormal2D().Rotation());
		Self->LaunchCharacter(AWRProjectile::LobVelocity(From, To, Row.Speed > 0.f ? Row.Speed : 1400.f, G), true, true);
		AI->RowDir = (To - From).GetSafeNormal2D();
		return true;
	}
	return false;
}

void FWRSkyAI::TickStrike(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, float DeltaSeconds)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return;
	const FName Id = Row.Id;
	if (Body->Has(WRSkyFlag::Undertow))
	{
		// The pull: 500 cm/s toward its mouth in a 15 m, 40 degree cone; off the lip is the fall. Brace halves it, anchor
		// boots stop it (WRSkyAir::Push's rules); nothing is pulled into the last 2 m.
		const FVector Mouth = Self->GetActorLocation() + Self->GetActorForwardVector() * Self->GetSimpleCollisionRadius() * 1.4f;
		const FVector Fwd = Self->GetActorForwardVector();
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Self || !V->IsAlive() || (V->IsA<AWRCreature>() && UWRSkyBody::Get(V))) continue;
			const FVector D = V->GetActorLocation() - Mouth;
			if (D.Size() > SlTn(TEXT("skyray.undertowRange"), 1500.f) || D.Size() < 200.f) continue;
			if (FVector::DotProduct(D.GetSafeNormal(), Fwd) < FMath::Cos(FMath::DegreesToRadians(SlTn(TEXT("skyray.undertowHalfAngle"), 20.f)))) continue;
			WRSkyAir::Push(V, -D.GetSafeNormal() * SlTn(TEXT("skyray.undertowPull"), 500.f), 0.15f, TEXT("undertow"), Self);
		}
	}
	if (Id == TEXT("belly_flop") && !Body->StoopTo.IsZero())
	{
		float Gz = 0.f;
		const bool bLanded = SlGroundBelow(Self->GetActorLocation(), Self->GetSimpleCollisionHalfHeight() + 80.f, Gz) || FVector::Dist(Self->GetActorLocation(), Body->StoopTo) < 150.f;
		if (bLanded || SlNow(Self) - Body->StoopStart > 1.0)
		{
			// The impact ring: 6 m at 12 m/s, low (jump it).
			const float R = Row.Radius > 0.f ? Row.Radius : 600.f;
			WRFx::Broadcast(Self, TEXT("shockwave"), Self->GetActorLocation() - FVector(0, 0, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, R / 100.f, TEXT("creature.skyray.flop"));
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It != Self && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < R + It->GetSimpleCollisionRadius()
					&& FMath::Abs(It->GetActorLocation().Z - Self->GetActorLocation().Z) < 400.f && !It->GetCharacterMovement()->IsFalling()) AI->RowHit(Self, *It);
			}
			Body->StoopTo = FVector::ZeroVector;
			Self->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	// A Gallowshrike's glide ends when it lands (gravity back).
	if (Id == TEXT("hop_pounce") && !Self->GetCharacterMovement()->IsFalling()) { Self->GetCharacterMovement()->GravityScale = 1.f; Body->Set(WRSkyFlag::Flap, false); }
}

void FWRSkyAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body || !Victim) return;
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	Body->Set(WRSkyFlag::Hood, false);
	Body->Set(WRSkyFlag::Gape, false);
	if (RowId == TEXT("lash")) Body->Set(WRSkyFlag::Lash, false);
	if (!bLanded) return;
	if (RowId == TEXT("impale")) StartHold(Self, Body, Victim, TEXT("impale"), SlTn(TEXT("gallowshrike.pinSeconds"), 1.5f));
	else if (RowId == TEXT("wing_smother")) StartHold(Self, Body, Victim, TEXT("smother"), SlTn(TEXT("hushwing.smotherSeconds"), 3.f));
	else if (RowId == TEXT("meat_theft"))
	{
		// It takes a piece of raw meat and is off 60 m with it.
		if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Victim); Inv && Inv->CountOf(TEXT("raw_meat")) > 0)
		{
			Inv->RemoveItem(TEXT("raw_meat"), 1);
			SlTell(Victim, TEXT("The Galeharrier took your meat"));
			Body->MulticastMoment(TEXT("sky.feathers"), TEXT("head"), 1.f, TEXT("creature.galeharrier.signature"));
		}
		Body->LastStolenAt = SlNow(Self);
		Self->SetAggroTarget(nullptr);
		const FVector Away = (Self->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D();
		Body->HomeNode = Body->HomeNode + Away * SlTn(TEXT("galeharrier.fleeWithMeat"), 6000.f) * 0.2f;
	}
	else if (RowId == TEXT("barb_strike") || RowId == TEXT("lash"))
	{
		if (RowId == TEXT("lash") && Victim->Status) Victim->Status->ApplyStatus(WRTags::Status_Slowed, SlTn(TEXT("sailbell.slowSeconds"), 3.f));
	}
	else if (RowId == TEXT("hiss_peck"))
	{
		// Then it runs for the nearest edge and parachutes off.
		Self->StartFleeing(Victim, TEXT("cornered"));
	}
	// A stoop that struck a glider closes it (any hit does: WRSkyAir::NoteHit through ApplyHit).
}

float FWRSkyAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return Damage;
	// Asleep in its roost (a Hushwing by day) it takes the first blow in full (it's helpless): no change. Grounded after a
	// stoop it's open: x1.2.
	if (Body->Has(WRSkyFlag::Grounded)) Damage *= SlTn(TEXT("groundedTaken"), 1.2f);
	return Damage;
}

void FWRSkyAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return;
	const float Stance = Hit.StanceDamage >= 0.f ? Hit.StanceDamage : Hit.PoiseDamage;
	// A blow on a grab's holder frees the held (the smother: 60 stance from allies).
	if (Body->HeldBody.IsValid() && Hit.Instigator != Body->HeldBody.Get()) Body->FreeStance += Stance + (Hit.Element == WRTags::Element_Fire ? 1000.f : 0.f);
	// The Undertow broken by a blow to its mouth with 30 stance or more.
	if (Body->Has(WRSkyFlag::Undertow) && Stance >= SlTn(TEXT("skyray.undertowBreak"), 30.f))
	{
		const FVector Mouth = Self->GetActorLocation() + Self->GetActorForwardVector() * Self->GetSimpleCollisionRadius();
		const FVector At = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
		if (FVector::DotProduct((At - Self->GetActorLocation()).GetSafeNormal2D(), Self->GetActorForwardVector()) > 0.3f || FVector::Dist(At, Mouth) < 400.f)
		{
			Body->Set(WRSkyFlag::Undertow, false);
			Body->Set(WRSkyFlag::Lobes, false);
			Body->Set(WRSkyFlag::Flap, false);
			if (AWRCreatureAI* AI = Cast<AWRCreatureAI>(Self->GetController())) AI->EndRow(Self, true);
			Body->MulticastMoment(TEXT("hit.dust"), TEXT("head"), 1.2f, TEXT("creature.skyray.hurt"));
		}
	}
	// Woken in its roost: the Hushwing goes for whoever did it.
	if (Body->Has(WRSkyFlag::Asleep)) { Body->Set(WRSkyFlag::Asleep, false); if (Hit.Instigator) Self->SetAggroTarget(Hit.Instigator); }
	// Hurt, a Mistlark flock bursts; a Windmane's herd defends it together.
	if (Self->CreatureId == TEXT("windmane") && Hit.Instigator)
	{
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (It->CreatureId == TEXT("windmane") && !It->IsTamed() && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < SlTn(TEXT("windmane.herdReach"), 6000.f))
			{
				It->bGuardAggressive = true;
				It->SetAggroTarget(Hit.Instigator);
			}
		}
	}
	if (Self->CreatureId == TEXT("cloudback") && Hit.Instigator)
	{
		Self->bGuardAggressive = true;
		Self->SetAggroTarget(Hit.Instigator);
	}
	// Fire: a Sailbell burns (and bursts if it dies of it).
	if (Self->CreatureId == TEXT("sailbell")) Body->bSwallowed = Hit.Element == WRTags::Element_Fire;
}

void FWRSkyAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRSkyBody* Body = UWRSkyBody::Get(Self);
	if (!Body) return;
	if (Body->HeldBody.IsValid()) Release(Self, Body, false);
	Body->Flags &= ~(WRSkyFlag::Stoop | WRSkyFlag::Undertow | WRSkyFlag::Lash | WRSkyFlag::Flap);
	if (Self->CreatureId == TEXT("sailbell"))
	{
		if (Body->bSwallowed)
		{
			// Fire: the gas pops (push 3 m, a puff of violet), nothing left to sting.
			WRFx::Broadcast(Self, TEXT("sky.bell_pop"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.2f, TEXT("creature.sailbell.pop"));
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				AWRCharacterBase* V = *It;
				const FVector D = V->GetActorLocation() - Self->GetActorLocation();
				if (V != Self && V->IsAlive() && D.Size() < SlTn(TEXT("sailbell.popRadius"), 300.f) + V->GetSimpleCollisionRadius())
				{
					FWRHit Hit;
					Hit.Instigator = Self;
					Hit.Damage = Self->Weapon.FlatDamage * SlTn(TEXT("sailbell.popDmg"), 0.5f);
					Hit.bUnblockable = true;
					Hit.Origin = Self->GetActorLocation();
					Hit.Element = WRTags::Element_Fire;
					UWRCombatLibrary::ApplyHit(V, Hit);
					WRSkyAir::Push(V, D.GetSafeNormal() * SlTn(TEXT("sailbell.popPush"), 900.f), 0.33f, TEXT("pop"), Self);
				}
			}
		}
		else
		{
			// Any other death: it deflates and its curtain falls, stinging 60 s more where it lies (UWRSkyLife).
			Body->Set(WRSkyFlag::Deflated, true);
		}
	}
	if (UWRSkyLife* L = UWRSkyLife::Get(Self)) L->NoteDeath(Self, Killer);
}

// ================================================================================================ the living world

UWRSkyLife* UWRSkyLife::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRSkyLife>() : nullptr;
}

bool UWRSkyLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

bool UWRSkyLife::IsTickable() const
{
	const UWorld* W = GetWorld();
	return W && W->GetNetMode() != NM_Client && WRSkyIsles::Live();
}

namespace WRSkyLifeImpl
{
	/** The dead Sailbells' fallen curtains: where, until when. */
	struct FCurtain { FVector At; double Until; TWeakObjectPtr<AActor> Bell; TMap<TWeakObjectPtr<AActor>, double> Stung; };
	TArray<FCurtain>& Curtains() { static TArray<FCurtain> C; return C; }
}

void UWRSkyLife::NoteDeath(AWRCreature* Dead, AActor* Killer)
{
	if (!Dead) return;
	if (UWRSkyBody* B = UWRSkyBody::Get(Dead); B && B->Has(WRSkyFlag::Deflated))
	{
		WRSkyLifeImpl::FCurtain C;
		float Gz = Dead->GetActorLocation().Z;
		WRSkyIsles::GroundUnder(Dead->GetActorLocation(), Gz);
		C.At = FVector(Dead->GetActorLocation().X, Dead->GetActorLocation().Y, Gz);
		C.Until = SlNow(this) + SlTn(TEXT("sailbell.deadSting"), 60.f);
		C.Bell = Dead;
		WRSkyLifeImpl::Curtains().Add(C);
	}
}

void UWRSkyLife::EggTaken(const FVector& Nest, AActor* Thief)
{
	// The herd defends its eggs together: every Windmane within 60 m comes for the thief.
	int32 N = 0;
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->CreatureId == TEXT("windmane") && !It->IsTamed() && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Nest) < SlTn(TEXT("windmane.nestReach"), 6000.f))
		{
			It->bGuardAggressive = true;
			It->SetAggroTarget(Thief);
			if (UWRSkyBody* B = UWRSkyBody::Get(*It)) B->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("creature.windmane.aggro"));
			++N;
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SKY a Windmane egg taken: %d of the herd come"), N);
}

AWRCreature* UWRSkyLife::SpawnOn(FName Kind, FName IslandId, const FVector& At)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return nullptr;
	const FVector Spawn = At + FVector(0, 0, Def->HalfHeight + 40.f);
	AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), Kind, Spawn, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	if (!C) return nullptr;
	if (UWRSkyBody* B = UWRSkyBody::Get(C))
	{
		B->HomeIsland = IslandId;
		B->HomeNode = Spawn;
		B->RingRadius = Kind == TEXT("galeharrier") ? FMath::FRandRange(1250.f, 2000.f) : Kind == TEXT("hushwing") ? FMath::FRandRange(1500.f, 2500.f)
			: Kind == TEXT("mistlark") ? FMath::FRandRange(500.f, 1200.f) : Kind == TEXT("skyray") ? FMath::FRandRange(1500.f, 2500.f) : 900.f;
		B->RingPhase = FMath::FRandRange(0.f, 6.28f);
		B->RingDir = FMath::RandBool() ? 1.f : -1.f;
	}
	C->SetHome(Spawn);
	if (FWRSkyAI::Floats(C)) C->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	return C;
}

void UWRSkyLife::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Clock += DeltaTime;
	BeatClock += DeltaTime;
	if (Clock >= SlTn(TEXT("stepSeconds"), 5.f))
	{
		Clock = 0.f;
		StepPopulations();
	}
	// The fallen curtains sting where they lie.
	{
		const double T = SlNow(this);
		TArray<WRSkyLifeImpl::FCurtain>& Cs = WRSkyLifeImpl::Curtains();
		for (int32 i = Cs.Num() - 1; i >= 0; --i)
		{
			WRSkyLifeImpl::FCurtain& C = Cs[i];
			if (T > C.Until) { Cs.RemoveAt(i); continue; }
			for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), C.At) > SlTn(TEXT("sailbell.deadReach"), 500.f)) continue;
				double& Last = C.Stung.FindOrAdd(*It);
				if (T - Last < 1.0) continue;
				Last = T;
				FWRHit Hit;
				Hit.Instigator = C.Bell.Get();
				Hit.Damage = SlTn(TEXT("sailbell.deadDmg"), 6.f);
				Hit.bUnblockable = true;
				Hit.bDamageOverTime = true;
				Hit.Origin = C.At;
				Hit.Element = WRTags::Element_Physical;
				UWRCombatLibrary::ApplyHit(*It, Hit);
				if (It->Status) It->Status->ApplyStatus(WRTags::Status_Slowed, SlTn(TEXT("sailbell.slowSeconds"), 3.f));
			}
		}
	}
	if (BeatClock >= 3.f)
	{
		BeatClock = 0.f;
		BeatMistlarks();
		BeatBreach();
		BeatGraze();
		BeatBlooms();
	}
}

void UWRSkyLife::StepPopulations()
{
	// Populations (tuning skyLife.populations): each keeps its count while a survivor is within reach of its islands,
	// refilling what dies on its timer away from eyes; left alone long enough, it's taken in (the islands are big).
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const TArray<TSharedPtr<FJsonValue>>* Pops = nullptr;
	const TSharedPtr<FJsonObject> Root = FWRTuning::Get().Object(TEXT("skyLife"));
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("populations"), Pops)) return;
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr) if (WRSkyIsles::Contains(P->GetActorLocation())) Players.Add(P->GetActorLocation());
	}
	const double T = SlNow(this);
	const bool bNight = SlNight(this);
	for (const TSharedPtr<FJsonValue>& V : *Pops)
	{
		const TSharedPtr<FJsonObject> P = V->AsObject();
		if (!P.IsValid()) continue;
		const FName Id(*P->GetStringField(TEXT("id")));
		const FName Kind(*P->GetStringField(TEXT("kind")));
		FString Mode = TEXT("top");
		P->TryGetStringField(TEXT("mode"), Mode);
		bool bNightOnly = false;
		P->TryGetBoolField(TEXT("nightOnly"), bNightOnly);
		TArray<const FWRSkyIsleData::FIsland*> Isles;
		const TArray<TSharedPtr<FJsonValue>>* IsleList = nullptr;
		if (P->TryGetArrayField(TEXT("islands"), IsleList)) for (const auto& I : *IsleList) if (const FWRSkyIsleData::FIsland* Is = D.FindIsland(FName(*I->AsString()))) Isles.Add(Is);
		const TArray<TSharedPtr<FJsonValue>>* Zones = nullptr;
		if (P->TryGetArrayField(TEXT("zones"), Zones)) for (const auto& Z : *Zones) for (const FWRSkyIsleData::FIsland& Is : D.Islands) if (Is.Zone == Z->AsString()[0]) Isles.Add(&Is);
		if (Isles.Num() == 0) continue;
		TArray<TWeakObjectPtr<AWRCreature>>& Mine = Kept.FindOrAdd(Id);
		Mine.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive() || C->IsTamed(); });
		// Near enough to matter?
		const float Reach = SlTn(TEXT("activeReach"), 90000.f);
		bool bNear = false;
		for (const FVector& Pl : Players) for (const FWRSkyIsleData::FIsland* Is : Isles) if (FVector::Dist2D(Pl, Is->Centre) < Reach + Is->R()) { bNear = true; break; }
		if (!bNear || (bNightOnly && !bNight))
		{
			// Unwatched (or the night hunters' night is over): take them in once nobody's near any of them.
			if (!bNear || bNightOnly)
			{
				for (const TWeakObjectPtr<AWRCreature>& C : Mine)
				{
					if (!C.IsValid() || C->GetAggroTarget()) continue;
					bool bSeen = false;
					for (const FVector& Pl : Players) if (FVector::Dist(Pl, C->GetActorLocation()) < SlTn(TEXT("despawnUnseen"), 15000.f)) bSeen = true;
					if (!bSeen) C->Destroy();
				}
				Mine.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid(); });
			}
			continue;
		}
		const int32 Want = FMath::RoundToInt(float(P->GetNumberField(TEXT("count"))) * FMath::Max(0.1f, FWRTuning::Get().Num(TEXT("server.creatureNumbers"), 1.f)));
		if (Mine.Num() >= Want) continue;
		if (bSeeded && T < NextRefillAt.FindRef(Id)) continue;
		NextRefillAt.Add(Id, T + SlTn(TEXT("refillSeconds"), 180.f));
		// A pack at a time (the creature's pack size) on one of its islands, away from survivors' eyes after the first fill.
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
		if (!Def) continue;
		int32 Guard = 0;
		while (Mine.Num() < Want && Guard++ < 12)
		{
			const FWRSkyIsleData::FIsland* I = Isles[FMath::RandRange(0, Isles.Num() - 1)];
			FVector Base;
			if (Mode == TEXT("column"))
			{
				// Mistlarks: a column on that island (else its middle), near the column's top.
				const FWRSkyIsleData::FUpdraft* Col = nullptr;
				for (const FWRSkyIsleData::FUpdraft& U : D.Updrafts) if (U.Island == I->Id) { Col = &U; break; }
				Base = Col ? FVector(Col->At, Col->Top - SlTn(TEXT("mistlark.belowTop"), 1500.f)) : I->Centre + FVector(0, 0, I->TopZ(I->Centre) - I->Centre.Z + 2500.f);
			}
			else
			{
				const FVector Top = D.OnIsland(*I, FMath::FRandRange(-0.45f, 0.45f), FMath::FRandRange(-0.45f, 0.45f));
				Base = Top;
				if (Mode == TEXT("soar")) Base = Top + FVector(0, 0, FMath::FRandRange(SlTn(TEXT("soarMin"), 1500.f), SlTn(TEXT("soarMax"), 4000.f)));
				else if (Mode == TEXT("float")) Base = Top + FVector(0, 0, SlTn(TEXT("cloudback.float"), 200.f));
				else if (Mode == TEXT("lip") || Mode == TEXT("drift"))
				{
					// Off the island's edge: 10-30 m out and under its lip (the Skyray), or across a gap at deck height (a bloom).
					const float A = FMath::FRandRange(0.f, 2.f * PI);
					const FVector2D L(FMath::Cos(A), FMath::Sin(A));
					const float Out = I->EdgeRadius(A) + FMath::FRandRange(1000.f, 3000.f);
					Base = FVector(I->World2D(L * Out), I->TopZ(I->Centre) - (Mode == TEXT("lip") ? FMath::FRandRange(1000.f, 3000.f) : FMath::FRandRange(-500.f, 1500.f)));
				}
			}
			// Not in front of anyone (after the first fill): a pack never pops up in view.
			bool bSeen = false;
			if (bSeeded) for (const FVector& Pl : Players) if (FVector::Dist(Pl, Base) < SlTn(TEXT("spawnUnseen"), 6000.f)) bSeen = true;
			if (bSeen) continue;
			const int32 Pack = FMath::Clamp(FMath::RandRange(Def->PackMin, Def->PackMax), 1, Want - Mine.Num());
			for (int32 n = 0; n < Pack; ++n)
			{
				const FVector Jit = Mode == TEXT("top") || Mode == TEXT("float") ? FVector(FMath::RandPointInCircle(SlTn(TEXT("packSpread"), 600.f)), 0.f) : FVector(FMath::RandPointInCircle(400.f), FMath::FRandRange(-200.f, 200.f));
				FVector At = Base + Jit;
				if (Mode == TEXT("top") || Mode == TEXT("roost")) At.Z = I->TopZ(At);
				if (AWRCreature* C = SpawnOn(Kind, I->Id, At))
				{
					if (UWRSkyBody* B = UWRSkyBody::Get(C))
					{
						if (Mode == TEXT("top") || Mode == TEXT("roost")) B->HomeNode = I->Centre + FVector(0, 0, I->TopZ(I->Centre) - I->Centre.Z + Def->HalfHeight);
						if (Mode == TEXT("soar") || Mode == TEXT("column")) B->RingHeight = At.Z - I->TopZ(I->Centre);
						if (Mode == TEXT("roost")) { B->Set(WRSkyFlag::Asleep, true); B->RingLift = SlTn(TEXT("hushwing.ringLift"), 2500.f); }
					}
					Mine.Add(C);
				}
			}
		}
	}
	bSeeded = true;
	// The Windmanes' ledge nests and the Hushwings: nothing more here (the nests are the world's spots: WRSkyWorld).
}

void UWRSkyLife::BeatMistlarks()
{
	// A gust's tell (the whistle 1.5 s before): Mistlarks within 30 m of its lane scatter downwind.
	for (const TWeakObjectPtr<UWRSkyBody>& W : UWRSkyBody::Live)
	{
		UWRSkyBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || C->CreatureId != TEXT("mistlark") || !C->IsAlive() || C->IsTamed()) continue;
		const WRSkyAir::FGust G = WRSkyAir::GustAt(C, C->GetActorLocation(), false);
		if (G.Phase < 0.f && G.Phase > -1.5f && !B->Has(WRSkyFlag::Chased))
		{
			B->Set(WRSkyFlag::Chased, true);
			B->GroundedUntil = SlNow(C) + 3.0;
			C->GetCharacterMovement()->Velocity += FVector(G.Dir * 700.f, 300.f);
		}
	}
}

void UWRSkyLife::BeatBreach()
{
	// Dusk in a Gale: two to five Skyrays leap 20 m out of the cloud and belly-flop back (mobula).
	const float H = AWRGameState::HourIn(this);
	if (!SlGale(this) || H < SlTn(TEXT("skyray.breachFrom"), 18.f) || H > SlTn(TEXT("skyray.breachTo"), 20.f)) return;
	const double T = SlNow(this);
	if (T - LastBreachAt < SlTn(TEXT("skyray.breachEvery"), 45.f)) return;
	LastBreachAt = T;
	int32 N = FMath::RandRange(2, 5);
	for (const TWeakObjectPtr<UWRSkyBody>& W : UWRSkyBody::Live)
	{
		UWRSkyBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || C->CreatureId != TEXT("skyray") || !C->IsAlive() || C->IsTamed() || C->GetAggroTarget() || N <= 0) continue;
		B->Set(WRSkyFlag::Breach, true);
		B->BreachStart = T + FMath::FRandRange(0.f, 1.5f);
		B->BreachFrom = C->GetActorLocation();
		B->MulticastMoment(TEXT("sky.cloud_splash"), NAME_None, 1.5f, TEXT("creature.skyray.breach"));
		--N;
	}
}

void UWRSkyLife::BeatGraze()
{
	// Dawn: the Windmane herds fly from the Mane Ledges to graze a garden island, and back to the ledges mid-morning.
	const float H = AWRGameState::HourIn(this);
	const bool bGraze = H >= SlTn(TEXT("windmane.grazeFrom"), 5.f) && H < SlTn(TEXT("windmane.grazeTo"), 9.f);
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const FWRSkyIsleData::FIsland* Ledges = D.FindIsland(TEXT("mane_ledges"));
	const FWRSkyIsleData::FIsland* Garden = D.FindIsland(FName(*FWRTuning::Get().Str(TEXT("skyLife.windmane.grazeIsland"), TEXT("flax_isle"))));
	if (!Ledges || !Garden) return;
	for (const TWeakObjectPtr<UWRSkyBody>& W : UWRSkyBody::Live)
	{
		UWRSkyBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || C->CreatureId != TEXT("windmane") || !C->IsAlive() || C->IsTamed() || C->GetAggroTarget()) continue;
		const FWRSkyIsleData::FIsland* Want = bGraze ? Garden : Ledges;
		if (B->HomeIsland == Want->Id) continue;
		B->HomeIsland = Want->Id;
		const FVector Top = D.OnIsland(*Want, FMath::FRandRange(-0.3f, 0.3f), FMath::FRandRange(-0.3f, 0.3f));
		B->HomeNode = FVector(Top.X, Top.Y, Want->TopZ(Top) + C->GetSimpleCollisionHalfHeight());
		B->MulticastMoment(NAME_None, TEXT("head"), 1.f, TEXT("creature.windmane.bugle"));
	}
}

void UWRSkyLife::BeatBlooms()
{
	// A bloom that has drifted out of the Sky (or 800 m from where it came in) is taken in; the population brings it back
	// in again upwind (the next bloom crosses the same bridge the same way).
	for (const TWeakObjectPtr<UWRSkyBody>& W : UWRSkyBody::Live)
	{
		UWRSkyBody* B = W.Get();
		AWRCreature* C = B ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || C->CreatureId != TEXT("sailbell") || !C->IsAlive() || C->IsTamed() || C->GetAggroTarget()) continue;
		if (!WRSkyIsles::Contains(C->GetActorLocation()) || FVector::Dist2D(C->GetActorLocation(), B->HomeNode) > SlTn(TEXT("sailbell.driftMax"), 80000.f)) C->Destroy();
	}
}
