// The Fire and Frost micro-hazards (cooks/cookpass-world.md 3.5), numbers in tuning hazards.ff.*. Every timed hazard
// (fumaroles, geysers, serac falls, katabatic gusts) runs on the server's clock with its own phase, so every machine
// draws its tell and its burst at the same moment with nothing replicated; only the server deals the blow (ApplyHit).
// The two that react to people (thin ice, snow bridges) are the server's state machines, replicated through
// AWRIsleState: their collision and looks follow the state on every machine.
#include "WRIslandPlaces.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRBuildLook.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LandscapeProxy.h"

namespace WRIsleHz
{
	float HzT(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("hazards.ff.")) + Key), Fallback); }

	/** A clocked hazard's period (its own, from its interval range) and where it is in its cycle now. */
	void HzCycle(FWRIsleHazard& H, double Now, const TCHAR* Kind, float MinP, float MaxP, int64& OutCycle, float& OutInto)
	{
		if (H.Period <= 0.f)
		{
			const float Lo = HzT(*FString::Printf(TEXT("%s.intervalMin"), Kind), MinP), Hi = HzT(*FString::Printf(TEXT("%s.intervalMax"), Kind), MaxP);
			H.Period = FMath::Lerp(Lo, Hi, FWRBuildLook::Hash01(H.At, 53)) + 0.001f;
		}
		const double t = Now + H.Phase;
		OutCycle = int64(FMath::FloorToDouble(t / H.Period));
		OutInto = float(t - double(OutCycle) * H.Period);
	}

	/** Feet of a character (bottom of its capsule). */
	FVector HzFeet(const AWRCharacterBase* C) { return C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight()); }

	/** How heavy a body is on thin ice (a survivor 100; a creature by its size; a rider counts as nothing, the mount
	 *  carries them). */
	float HzWeight(const AWRCharacterBase* C)
	{
		if (C->GetAttachParentActor()) return 0.f;
		if (Cast<AWRCreature>(C))
		{
			const float R = C->GetSimpleCollisionRadius();
			return FMath::Clamp(HzT(TEXT("thinIce.player"), 100.f) * FMath::Square(R / 42.f), 30.f, 1500.f);
		}
		return HzT(TEXT("thinIce.player"), 100.f);
	}

	bool HzLandZ(UWorld* World, const FVector2D& P, float& OutZ)
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRIsleGust), false);
		World->LineTraceMultiByObjectType(Hits, FVector(P, 200000.f), FVector(P, -30000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
		for (const FHitResult& H : Hits) if (H.GetActor() && H.GetActor()->IsA<ALandscapeProxy>()) { OutZ = float(H.ImpactPoint.Z); return true; }
		return false;
	}

	/** Downhill at a point (unit 2D), from the landscape 4 m either side; outward from Centre where it's flat. */
	FVector2D HzDownhill(UWorld* World, const FVector2D& P, const FVector2D& Centre)
	{
		float Zx0, Zx1, Zy0, Zy1;
		const float D = 400.f;
		if (HzLandZ(World, P - FVector2D(D, 0), Zx0) && HzLandZ(World, P + FVector2D(D, 0), Zx1) && HzLandZ(World, P - FVector2D(0, D), Zy0) && HzLandZ(World, P + FVector2D(0, D), Zy1))
		{
			const FVector2D G((Zx1 - Zx0) / (2.f * D), (Zy1 - Zy0) / (2.f * D));
			if (G.Size() > 0.03f) return -G.GetSafeNormal();
		}
		return (P - Centre).GetSafeNormal();
	}

	void HzBlow(AWRCharacterBase* Who, const FVector& Origin, float Damage, uint8 Impact, bool bUnblockable)
	{
		FWRHit Hit;
		Hit.Damage = Damage;
		Hit.Origin = Origin;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.Tier = Damage >= 50.f ? EWRHitTier::Heavy : EWRHitTier::Light;
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}

	/** The gusts: one every interval on average, each start nudged by its own hash (never closer than the minimum).
	 *  Gust n starts at n x Period + Jit(n). */
	float HzGustPeriod() { return (HzT(TEXT("katabatic.intervalMin"), 10.f) + HzT(TEXT("katabatic.intervalMax"), 18.f)) * 0.5f; }
	double HzGustStart(int64 n)
	{
		const float Spread = (HzT(TEXT("katabatic.intervalMax"), 18.f) - HzT(TEXT("katabatic.intervalMin"), 10.f)) * 0.25f;
		return double(n) * HzGustPeriod() + Spread * (FWRBuildLook::Hash01(FVector(double(n % 100000), 7.0, 3.0), 61) * 2.f - 1.f);
	}
	/** The gust whose push window holds Now, or the next one to start (its index and start time). */
	int64 HzNextGust(double Now, double& OutStart)
	{
		int64 n = int64(FMath::FloorToDouble(Now / HzGustPeriod())) - 1;
		const float Live = HzT(TEXT("katabatic.seconds"), 0.6f);
		while (HzGustStart(n) + Live < Now) ++n;
		OutStart = HzGustStart(n);
		return n;
	}
}
using namespace WRIsleHz;

void UWRIslandPlaces::OnBreak(const FWRIsleBreak& B)
{
	for (FWRIsleHazard& H : Hazards)
	{
		if (H.Key != B.Key) continue;
		if (!bServer)
		{
			H.State = B.State;
			H.StateAt = B.At;
		}
		if (H.ShownState != B.State)
		{
			if (B.State == 1) H.CracksPlayed = 0;
			ApplyShown(H, B.State);
		}
		return;
	}
}

void UWRIslandPlaces::ApplyShown(FWRIsleHazard& H, uint8 NewState)
{
	const uint8 Was = H.ShownState;
	H.ShownState = NewState;
	const bool bOpen = NewState == 2;
	for (const TWeakObjectPtr<USceneComponent>& C : H.Whole) if (C.IsValid()) C->SetVisibility(!bOpen, true);
	for (const TWeakObjectPtr<USceneComponent>& C : H.Broken) if (C.IsValid()) C->SetVisibility(bOpen, true);
	for (const TWeakObjectPtr<UBoxComponent>& C : H.Solid) if (C.IsValid()) C->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	for (const TWeakObjectPtr<UBoxComponent>& C : H.RimSolid) if (C.IsValid()) C->SetCollisionEnabled(bOpen ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (!bDraw) return;
	UWorld* World = GetWorld();
	if (bOpen && Was != 2)
	{
		const bool bIce = H.Kind == FWRIsleHazard::EKind::ThinIce;
		WRFx::SpawnAt(World, bIce ? TEXT("ff.ice_break") : TEXT("ff.bridge_fall"), H.At, H.Rot, bIce ? H.Radius / 600.f : 1.f);
		WRSound::PlayAt(World, bIce ? TEXT("ff.ice_break") : TEXT("ff.bridge_fall"), H.At);
	}
	else if (!bOpen && Was == 2)
	{
		// Refrozen / drifted shut: a soft settle.
		WRSound::PlayAt(World, H.Kind == FWRIsleHazard::EKind::ThinIce ? TEXT("ff.ice_refreeze") : TEXT("ff.bridge_reform"), H.At);
	}
}

void UWRIslandPlaces::StepServer(float Dt)
{
	ServerAccum += Dt;
	const float Step = ServerAccum;
	if (Step < 0.1f) return;
	ServerAccum = 0.f;
	UWorld* World = GetWorld();
	const double Now = WRIsle::Now(World);
	TArray<AWRCharacterBase*> Bodies;
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		if (It->IsAlive() && !Cast<AWRBoss>(*It)) Bodies.Add(*It);
	}
	if (Bodies.Num() == 0) return;
	AWRIsleState* St = State.Get();
	for (FWRIsleHazard& H : Hazards)
	{
		// Nobody near: nothing to judge (the clocked ones keep their clock regardless).
		bool bNear = false;
		for (const AWRCharacterBase* C : Bodies) if (FVector::DistSquared2D(C->GetActorLocation(), H.At) < FMath::Square(H.Radius + 2500.f)) { bNear = true; break; }
		if (!bNear && H.State == 0) continue;
		switch (H.Kind)
		{
		case FWRIsleHazard::EKind::Fumarole:
		case FWRIsleHazard::EKind::Geyser:
		case FWRIsleHazard::EKind::Serac:
		{
			const bool bFum = H.Kind == FWRIsleHazard::EKind::Fumarole, bGey = H.Kind == FWRIsleHazard::EKind::Geyser;
			const TCHAR* K = bFum ? TEXT("fumarole") : bGey ? TEXT("geyser") : TEXT("serac");
			int64 Cyc; float Into;
			HzCycle(H, Now, K, bFum ? 30.f : bGey ? 40.f : 35.f, bFum ? 50.f : bGey ? 40.f : 70.f, Cyc, Into);
			const float Live = HzT(*FString::Printf(TEXT("%s.seconds"), K), bFum ? 1.2f : bGey ? 1.6f : 0.4f);
			if (Into >= Live) break;
			if (Cyc != H.LastCycle) { H.LastCycle = Cyc; H.HitThisCycle.Reset(); }
			const float Rad = HzT(*FString::Printf(TEXT("%s.radius"), K), H.Radius);
			for (AWRCharacterBase* C : Bodies)
			{
				if (H.HitThisCycle.Contains(C)) continue;
				const FVector F = HzFeet(C);
				const float Up = bFum ? 600.f : bGey ? 400.f : 300.f;
				if (FVector::DistSquared2D(F, H.At) > Rad * Rad || F.Z < H.At.Z - 150.f || F.Z > H.At.Z + Up) continue;
				H.HitThisCycle.Add(C);
				if (bFum)
				{
					HzBlow(C, H.At, HzT(TEXT("fumarole.damage"), 35.f), 3, true);
					if (C->IsAlive()) C->LaunchCharacter(FVector(0.f, 0.f, HzT(TEXT("fumarole.launch"), 600.f)), false, true);
				}
				else if (bGey)
				{
					HzBlow(C, H.At, HzT(TEXT("geyser.damage"), 10.f), 1, true);
					// Straight up, and a little toward where the spout leans (the Glasswall ledge's geyser).
					const FVector Lean = H.Rot.Vector() * H.Half.X;
					if (C->IsAlive()) C->LaunchCharacter(FVector(Lean.X, Lean.Y, HzT(TEXT("geyser.launch"), 1600.f)), true, true);
				}
				else
				{
					HzBlow(C, H.At, HzT(TEXT("serac.damage"), 60.f), 3, true);
				}
			}
			break;
		}
		case FWRIsleHazard::EKind::Acid:
		{
			for (AWRCharacterBase* C : Bodies)
			{
				const FVector F = HzFeet(C);
				if (FVector::DistSquared2D(F, H.At) > H.Radius * H.Radius || F.Z > H.At.Z + 90.f || F.Z < H.At.Z - 250.f) continue;
				// A mount wades it for you (its own hide takes it).
				if (C->GetAttachParentActor()) continue;
				FWRHit Hit;
				Hit.Damage = HzT(TEXT("acid.dps"), 6.f) * Step;
				Hit.bDamageOverTime = true;
				Hit.Origin = F;
				UWRCombatLibrary::ApplyHit(C, Hit);
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(C)) Inv->WearArmour(HzT(TEXT("acid.wear"), 2.f) * Step);
			}
			break;
		}
		case FWRIsleHazard::EKind::ThinIce:
		{
			const float Top = H.At.Z + H.Half.Z;
			float Weight = 0.f;
			bool bSlam = false;
			TArray<AWRCharacterBase*> InHole;
			for (AWRCharacterBase* C : Bodies)
			{
				const FVector F = HzFeet(C);
				if (FVector::DistSquared2D(F, H.At) > FMath::Square(H.Radius * 0.97f) || F.Z > Top + 150.f || F.Z < H.At.Z - 200.f) continue;
				if (H.State == 2) { InHole.Add(C); continue; }
				Weight += HzWeight(C);
				// A heavy landing breaks it at once (falling faster than a 4 m drop lands).
				if (C->GetCharacterMovement() && C->GetCharacterMovement()->Velocity.Z < -HzT(TEXT("thinIce.slamSpeed"), 880.f) && F.Z < Top + 60.f) bSlam = true;
			}
			const float Limit = HzT(TEXT("thinIce.weight"), 300.f);
			const float BreakAfter = HzT(TEXT("thinIce.breakAfter"), 1.2f);
			if (H.State == 0 && (Weight >= Limit || bSlam))
			{
				H.State = bSlam ? 2 : 1;
				H.StateAt = Now;
				if (St) St->Set(H.Key, H.State, float(Now));
			}
			else if (H.State == 1)
			{
				if (Weight < Limit) { H.State = 0; H.StateAt = Now; if (St) St->Set(H.Key, 0, float(Now)); }
				else if (Now - H.StateAt >= BreakAfter || bSlam) { H.State = 2; H.StateAt = Now; if (St) St->Set(H.Key, 2, float(Now)); }
			}
			else if (H.State == 2)
			{
				// Icy water: frost builds while you're in it (freezing is the danger); it refreezes once nobody's in it.
				for (AWRCharacterBase* C : InHole)
				{
					FWRHit Hit;
					Hit.FrostBuildup = HzT(TEXT("thinIce.frost"), 25.f) * Step;
					Hit.Element = WRTags::Element_Frost;
					Hit.bDamageOverTime = true;
					Hit.Origin = HzFeet(C);
					UWRCombatLibrary::ApplyHit(C, Hit);
				}
				if (InHole.Num() == 0 && Now - H.StateAt >= HzT(TEXT("thinIce.refreeze"), 90.f))
				{
					H.State = 0;
					H.StateAt = Now;
					if (St) St->Set(H.Key, 0, float(Now));
				}
			}
			break;
		}
		case FWRIsleHazard::EKind::SnowBridge:
		{
			const FTransform BT(H.Rot, H.At);
			bool bOn = false, bInside = false;
			for (AWRCharacterBase* C : Bodies)
			{
				const FVector L = BT.InverseTransformPosition(HzFeet(C));
				if (FMath::Abs(L.X) > H.Half.X + 20.f || FMath::Abs(L.Y) > H.Half.Y + 20.f) continue;
				if (L.Z > H.Half.Z - 40.f && L.Z < H.Half.Z + 120.f) bOn = true;
				if (L.Z > -H.Half.Z - 600.f && L.Z < H.Half.Z + 120.f) bInside = true;
			}
			if (H.State == 0 && bOn)
			{
				H.State = 1;
				H.StateAt = Now;
				if (St) St->Set(H.Key, 1, float(Now));
			}
			else if (H.State == 1 && Now - H.StateAt >= HzT(TEXT("snowBridge.collapse"), 0.9f))
			{
				H.State = 2;
				H.StateAt = Now;
				if (St) St->Set(H.Key, 2, float(Now));
			}
			else if (H.State == 2 && !bInside && Now - H.StateAt >= HzT(TEXT("snowBridge.reform"), 30.f))
			{
				H.State = 0;
				H.StateAt = Now;
				if (St) St->Set(H.Key, 0, float(Now));
			}
			break;
		}
		}
	}
	// The katabatic gusts: pouring downhill off the glacier in the ring round the bowl.
	if (GustOuter > 0.f)
	{
		double Start;
		const int64 n = HzNextGust(Now, Start);
		if (Now >= Start && n != GustDone)
		{
			GustDone = n;
			for (AWRCharacterBase* C : Bodies)
			{
				const FVector2D P(C->GetActorLocation());
				const float D = FVector2D::Distance(P, GustCentre);
				if (D < GustInner || D > GustOuter) continue;
				const FVector2D Dir = HzDownhill(World, P, GustCentre);
				float K = HzT(TEXT("katabatic.push"), 700.f);
				if (FWRBiomeHazards::Resists(C, TEXT("ice"))) K *= HzT(TEXT("katabatic.iceResistScale"), 0.5f);
				WRSkyAir::Push(C, FVector(Dir * K, 0.f), HzT(TEXT("katabatic.seconds"), 0.6f), TEXT("katabatic"), nullptr);
			}
		}
	}
}

void UWRIslandPlaces::StepClient(float Dt)
{
	UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Me = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
	if (!Me) return;
	const FVector MeAt = Me->GetActorLocation();
	const double Now = WRIsle::Now(World);
	const float Reach = HzT(TEXT("drawReach"), 15000.f);
	int32 IceNow = INDEX_NONE;
	for (int32 i = 0; i < Hazards.Num(); ++i)
	{
		FWRIsleHazard& H = Hazards[i];
		if (FVector::DistSquared2D(MeAt, H.At) > Reach * Reach) continue;
		switch (H.Kind)
		{
		case FWRIsleHazard::EKind::Fumarole:
		case FWRIsleHazard::EKind::Geyser:
		case FWRIsleHazard::EKind::Serac:
		{
			const bool bFum = H.Kind == FWRIsleHazard::EKind::Fumarole, bGey = H.Kind == FWRIsleHazard::EKind::Geyser;
			const TCHAR* K = bFum ? TEXT("fumarole") : bGey ? TEXT("geyser") : TEXT("serac");
			int64 Cyc; float Into;
			HzCycle(H, Now, K, bFum ? 30.f : bGey ? 40.f : 35.f, bFum ? 50.f : bGey ? 40.f : 70.f, Cyc, Into);
			const float Tell = HzT(*FString::Printf(TEXT("%s.tell"), K), bFum ? 1.f : bGey ? 1.f : 1.2f);
			// The tell for the next cycle's burst: the last Tell seconds of this one.
			if (Into >= H.Period - Tell && H.DrawnTell != Cyc + 1)
			{
				H.DrawnTell = Cyc + 1;
				const FName Fx = bFum ? TEXT("ff.fumarole.tell") : bGey ? TEXT("ff.geyser.tell") : TEXT("ff.serac.dust");
				const FVector At = bGey || bFum ? H.At : H.At + FVector(0.f, 0.f, H.Half.Z);
				WRFx::SpawnAt(World, Fx, At);
				WRSound::PlayAt(World, bFum ? TEXT("ff.fumarole_tell") : bGey ? TEXT("ff.geyser_tell") : TEXT("ff.serac_tell"), At);
			}
			if (Into < 0.3f && H.DrawnBurst != Cyc)
			{
				H.DrawnBurst = Cyc;
				WRFx::SpawnAt(World, bFum ? TEXT("ff.fumarole.burst") : bGey ? TEXT("ff.geyser.burst") : TEXT("ff.serac.impact"), H.At, H.Rot);
				WRSound::PlayAt(World, bFum ? TEXT("ff.fumarole") : bGey ? TEXT("ff.geyser") : TEXT("ff.serac_fall"), H.At);
				if (!bFum && !bGey) WRFx::Shake(World, HzT(TEXT("serac.shake"), 0.25f), H.At, 2500.f);
			}
			break;
		}
		case FWRIsleHazard::EKind::ThinIce:
		{
			const FVector F = MeAt - FVector(0.f, 0.f, Me->GetSimpleCollisionHalfHeight());
			if (FVector::DistSquared2D(F, H.At) < FMath::Square(H.Radius) && F.Z < H.At.Z + H.Half.Z + 100.f && F.Z > H.At.Z - 200.f) IceNow = i;
			// The three cracks while it bears too much (louder each time, white lines spreading).
			if (H.ShownState == 1)
			{
				const float Since = float(Now - H.StateAt);
				const float Stages[3] = { 0.f, HzT(TEXT("thinIce.stage2"), 0.6f), HzT(TEXT("thinIce.stage3"), 1.2f) - 0.05f };
				while (H.CracksPlayed < 3 && Since >= Stages[H.CracksPlayed])
				{
					const float Loud = 0.6f + 0.25f * H.CracksPlayed;
					WRSound::PlayAt(World, TEXT("ff.ice_crack"), H.At, Loud);
					WRFx::SpawnAt(World, TEXT("ff.ice_crack"), H.At + FVector(0.f, 0.f, H.Half.Z + 2.f), FRotator(0.f, 120.f * H.CracksPlayed, 0.f), (H.Radius / 600.f) * (0.5f + 0.25f * H.CracksPlayed));
					++H.CracksPlayed;
				}
			}
			break;
		}
		case FWRIsleHazard::EKind::SnowBridge:
			// The creak and the snow trickling from its underside as it starts to go.
			if (H.ShownState == 1 && H.CracksPlayed == 0)
			{
				H.CracksPlayed = 1;
				WRSound::PlayAt(World, TEXT("ff.bridge_creak"), H.At);
				WRFx::SpawnAt(World, TEXT("ff.snow_trickle"), H.At - FVector(0.f, 0.f, H.Half.Z), H.Rot);
			}
			else if (H.ShownState != 1) H.CracksPlayed = 0;
			break;
		default: break;
		}
	}
	// Stepping onto a thin patch: one faint creak (it's thin: read the colour), every time you step on.
	if (IceNow != IceUnderMe)
	{
		if (IceNow != INDEX_NONE && Hazards[IceNow].ShownState == 0) WRSound::PlayAt(World, TEXT("ff.ice_creak"), Hazards[IceNow].At, 0.7f);
		IceUnderMe = IceNow;
	}
	// The katabatic tell: a rising howl and snow streaming downhill round you a second before it shoves.
	if (GustOuter > 0.f)
	{
		const float D = FVector2D::Distance(FVector2D(MeAt), GustCentre);
		if (D >= GustInner - 2000.f && D <= GustOuter + 2000.f)
		{
			double Start;
			const int64 Next = HzNextGust(Now, Start);
			const float Tell = HzT(TEXT("katabatic.tell"), 1.f);
			if (Now >= Start - Tell && Now < Start && GustDrawn != Next)
			{
				GustDrawn = Next;
				const FVector2D Dir = HzDownhill(World, FVector2D(MeAt), GustCentre);
				WRSound::PlayAt(World, TEXT("ff.gust_howl"), MeAt - FVector(Dir * 1500.f, 0.f));
				WRFx::SpawnAt(World, TEXT("ff.gust"), MeAt - FVector(Dir * 600.f, 0.f) - FVector(0.f, 0.f, 60.f), FVector(Dir, 0.f).Rotation());
			}
		}
	}
}
