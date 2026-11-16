// The air: gliding's parameters, gusts as pure functions, the one push entry, fall damage, edge kills, the glider you
// see. See WRSkyAir.h (cooks/build-biome-sky.md 2.6-2.7; world-map 2.5; combat SPEC fall damage).

#include "WRSkyAir.h"
#include "Wildreach.h"
#include "WRSkyIsleTypes.h"
#include "WRSkyWorld.h"
#include "WRWorldSystems.h"
#include "WRPlayerState.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCharacterMovementComponent.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRCombatLibrary.h"
#include "WRAttributeSet.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRStormTypes.h"
#include "WRSurvival.h"
#include "WRArtifacts.h"
#include "WRSound.h"
#include "WRFx.h"
#include "WRToolMesh.h"
#include "WRWindPath.h"
#include "WRTuning.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "ProceduralMeshComponent.h"

namespace WRSkyAirImpl
{
	struct FCredit { TWeakObjectPtr<AActor> By; double At = -100.0; };
	TMap<TWeakObjectPtr<const AActor>, FCredit>& Credits() { static TMap<TWeakObjectPtr<const AActor>, FCredit> M; return M; }

	float H01(int32 A, int32 B = 0) { return FMath::Frac(FMath::Sin(float(A) * 12.9898f + float(B) * 78.233f) * 43758.5453f); }

	const FName NCrude(TEXT("crude_glider")), NLevi(TEXT("levistone_glider")), NWind(TEXT("wind"));

	/** The glider row from tuning glide.<id> (world-map 2.5's table as the fallbacks). */
	FWRGlideParams Row(FName Kind)
	{
		FWRGlideParams P;
		P.Kind = Kind;
		const bool bLevi = Kind == NLevi;
		const bool bPath = Kind == TEXT("path");
		const bool bSilk = Kind == TEXT("skysilk");
		const FString K = FString::Printf(TEXT("glide.%s."), *Kind.ToString());
		P.Forward = WRTune(*(K + TEXT("forward")), bLevi ? 1100.f : bPath ? 950.f : bSilk ? 900.f : 800.f);
		P.Sink = WRTune(*(K + TEXT("sink")), bLevi ? 200.f : bPath ? 230.f : bSilk ? 300.f : 260.f);
		P.StaminaPerSecond = WRTune(*(K + TEXT("stamina")), bLevi ? 3.f : bPath ? 3.f : bSilk ? 0.f : 5.f);
		P.Drift = WRTune(*(K + TEXT("drift")), bLevi ? 0.1f : 0.15f);
		P.TurnRate = WRTune(*(K + TEXT("turnRate")), bLevi ? 140.f : 110.f);
		return P;
	}
	/** The first stack of this glider in the bag that still holds. */
	const FWRItemStack* GliderStack(const UWRInventoryComponent* Inv, FName Kind)
	{
		if (!Inv) return nullptr;
		for (const FWRItemStack& S : Inv->GetSlots())
		{
			if (S.ItemId == Kind && S.Count > 0 && S.Durability > 0.f) return &S;
		}
		return nullptr;
	}
}

// ================================================================================================ gliding

bool WRSkyAir::HasFeatherfall(const AActor* Who)
{
	// Featherfall is the Wind path's root (a tag on the main path), and Sky lord's hover, the fall after it and Plunge's
	// own dive take no fall damage either (WRWindPath).
	return Who && WRWind::NoFallDamage(Who);
}

bool WRSkyAir::GlideParams(const ACharacter* Who, FWRGlideParams& Out)
{
	using namespace WRSkyAirImpl;
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Who);
	if (!Survivor || !Survivor->IsAlive()) return false;
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Survivor);
	if (GliderStack(Inv, NLevi)) Out = Row(NLevi);
	else if (GliderStack(Inv, NCrude)) Out = Row(NCrude);
	else if (WRWind::CanPathGlide(Survivor))
	{
		// Path Glide: the path's own speeds; its Momentum, then stamina, are the path's to spend (PathGlideTick).
		Out = Row(TEXT("path"));
		WRWind::PathGlideStats(Survivor, Out.Forward, Out.Sink);
		Out.StaminaPerSecond = 0.f;
	}
	else if (Inv && Inv->GearStat(TEXT("glideSeconds")) > 0.f)
	{
		Out = Row(TEXT("skysilk"));
		Out.MaxSeconds = Inv->GearStat(TEXT("glideSeconds"));
	}
	else return false;
	Out.bFeatherfall = HasFeatherfall(Survivor);
	// Featherfall halves a glider's stamina; Leaf glide sinks it slower (WRWindPath).
	Out.StaminaPerSecond *= WRWind::GliderStaminaScale(Survivor);
	Out.Sink *= WRWind::GlideSinkScale(Survivor);
	// The Kitewright's last kite (the Bonebell's echo): glider stamina -20%.
	// The Kitewright's last kite and Gulper's pearl: -20% each.
	if (Inv) Out.StaminaPerSecond *= FMath::Max(0.f, 1.f + Inv->GearStat(TEXT("glideStamina")));
	// The Wind Mask of Aeolith: no stamina at all in a wind over 10 m/s (cooks/biome-sky.md 2.11; a Gale).
	if (Inv && Inv->GearStat(TEXT("glideStaminaWindy")) > 0.f && WRSkyIsles::Contains(Survivor->GetActorLocation())
		&& WRTune(TEXT("glide.windSpeed"), 800.f) * WRSkyIsles::WindStrength(Survivor) > WRTune(TEXT("glide.windyAbove"), 1000.f))
		Out.StaminaPerSecond = 0.f;
	return true;
}

bool WRSkyAir::IsGliding(const ACharacter* Who)
{
	const UWRCharacterMovementComponent* M = Who ? Cast<UWRCharacterMovementComponent>(Who->GetCharacterMovement()) : nullptr;
	return M && M->IsGliding();
}

float WRSkyAir::GlideDrain(const ACharacter* Who)
{
	if (!IsGliding(Who)) return -1.f;
	FWRGlideParams P;
	if (!GlideParams(Who, P)) return 0.f;
	// None while an updraft lifts you (world-map 2.5).
	if (WRSkyIsles::LiftAt(Who, Who->GetActorLocation()) > 50.f) return 0.f;
	return P.StaminaPerSecond;
}

// ================================================================================================ pushes

float WRSkyAir::PushScale(const ACharacter* Who, FName Source)
{
	if (!Who) return 0.f;
	const AWRCharacterBase* Ch = Cast<AWRCharacterBase>(Who);
	// A roll's i-frames let it pass (the dodge's invulnerable window).
	if (Ch && (Ch->HasTag(WRTags::State_Invulnerable))) return 0.f;
	if (const AWRCreature* C = Cast<AWRCreature>(Who))
	{
		const FWRCreatureDef* D = C->GetDef();
		// Fliers and the Sky-born resist their biome's push (CR SPEC); a tame with gustImmune too; bosses hold.
		if (!D || D->Archetype == TEXT("flyer") || D->Resists.Contains(TEXT("gust")) || Cast<AWRBoss>(Who)) return 0.f;
		if (C->IsTamed() && C->TameHasNode(TEXT("gustImmune"))) return 0.f;
		return 1.f;
	}
	// Anchor boots and Skysilk's four pieces: gusts never push you (gear stat passGusts).
	if (UWRInventoryComponent::GearStatFor(Who, TEXT("passGusts")) > 0.f) return 0.f;
	// Riding: the mount takes it.
	if (Who->GetAttachParentActor()) return 0.f;
	// Brace: crouching, or holding a guard, and not swinging.
	const UWRCharacterMovementComponent* M = Cast<UWRCharacterMovementComponent>(Who->GetCharacterMovement());
	const bool bBrace = M && (M->bWantsToCrouch || M->bBlockSlow) && !(Ch && Ch->IsSwinging());
	return bBrace ? WRTune(TEXT("hazards.sky.gust.braceScale"), 0.5f) : 1.f;
}

void WRSkyAir::NoteHit(AActor* Who, AActor* By)
{
	if (!Who || !By || Who == By || !Who->HasAuthority()) return;
	// A tame's or a projectile's blow belongs to its owner.
	AActor* Owner = By;
	if (APawn* P = Cast<APawn>(By); !P && By->GetInstigator()) Owner = By->GetInstigator();
	WRSkyAirImpl::FCredit& C = WRSkyAirImpl::Credits().FindOrAdd(Who);
	C.By = Owner;
	C.At = WRSkyIsles::ServerTime(Who);
	// Any hit taken closes a glider (world-map 2.5).
	if (ACharacter* Ch = Cast<ACharacter>(Who); Ch && IsGliding(Ch))
	{
		Ch->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
}

AActor* WRSkyAir::FallCredit(const AActor* Victim)
{
	const WRSkyAirImpl::FCredit* C = Victim ? WRSkyAirImpl::Credits().Find(Victim) : nullptr;
	if (!C || !C->By.IsValid()) return nullptr;
	return WRSkyIsles::ServerTime(Victim) - C->At <= WRTune(TEXT("skyWorld.fall.creditSeconds"), 6.f) ? C->By.Get() : nullptr;
}

void WRSkyAir::Push(ACharacter* Who, const FVector& Vel, float Seconds, FName Source, AActor* Pusher)
{
	if (!Who || !Who->HasAuthority()) return;
	if (Pusher) NoteHit(Who, Pusher);
	const float K = PushScale(Who, Source);
	if (K <= 0.f) return;
	UCharacterMovementComponent* M = Who->GetCharacterMovement();
	if (!M) return;
	// A sustained push of Vel for Seconds covers Vel x Seconds; a launch carries that far on its own (friction and
	// air control take it from there), so the launch keeps the distance the push would have had.
	const float Carry = FMath::Clamp(Seconds / FMath::Max(0.1f, WRTune(TEXT("skyWorld.push.launchSeconds"), 0.5f)), 0.6f, 2.f);
	FVector V = Vel * K * Carry;
	if (IsGliding(Who))
	{
		// A glider is shoved, not closed: PhysGlide eases it back to its own speed.
		M->Velocity += FVector(V.X, V.Y, FMath::Min(0.f, V.Z));
		return;
	}
	V.Z = FMath::Max(V.Z, WRTune(TEXT("skyWorld.push.lift"), 160.f));
	Who->LaunchCharacter(V, false, false);
}

WRSkyAir::FGust WRSkyAir::GustAt(const UObject* WorldContext, const FVector& Pos, bool bOnFoot)
{
	using namespace WRSkyAirImpl;
	FGust G;
	if (!WRSkyIsles::Contains(Pos)) return G;
	const FWRSkyIsleData& D = FWRSkyIsleData::Get();
	const double T = WRSkyIsles::ServerTime(WorldContext);
	const float Tell = WRTune(TEXT("hazards.sky.gust.tell"), 1.5f);
	const float Live = WRTune(TEXT("hazards.sky.gust.seconds"), 0.8f);
	const FVector2D Wind = WRSkyIsles::WindDir(WorldContext);
	// Gust lanes (island lips, bridges, the cliff ridge): each on its own steady beat of 8-15 s, so a lane can be read.
	for (int32 i = 0; i < D.Lanes.Num(); ++i)
	{
		const FWRSkyIsleData::FLane& L = D.Lanes[i];
		if (L.Points.Num() < 2) continue;
		FVector Off = FVector::ZeroVector;
		if (const FWRSkyIsleData::FIsland* I = D.FindIsland(L.Island)) Off = WRSkyIsles::DriftOf(I->Index);
		float Best = TNumericLimits<float>::Max();
		float BestZ = 0.f;
		for (int32 k = 0; k + 1 < L.Points.Num(); ++k)
		{
			const FVector A = L.Points[k] + Off, B = L.Points[k + 1] + Off;
			const FVector C = FMath::ClosestPointOnSegment(Pos, A, B);
			const float Dd = FVector2D::Distance(FVector2D(Pos), FVector2D(C));
			if (Dd < Best) { Best = Dd; BestZ = C.Z; }
		}
		if (Best > L.Width || FMath::Abs(Pos.Z - BestZ) > 900.f) continue;
		const float Period = FMath::Lerp(WRTune(TEXT("hazards.sky.gust.everyMin"), 8.f), WRTune(TEXT("hazards.sky.gust.everyMax"), 15.f), H01(i, 7));
		const float Ph = float(FMath::Fmod(T + double(i) * 3.1, double(Period))) - Tell;
		if (Ph < -Tell || Ph > Live) continue;
		// Toward the lane's marked drop side, bent toward the wind (the wind blows it; the lane says where it falls).
		FVector2D Dir = (L.Drop * 0.75f + Wind * 0.25f).GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = Wind;
		G.bOn = true;
		G.Phase = Ph;
		G.Seconds = Live;
		G.Dir = Dir;
		G.Lane = i;
		G.From = FVector(FVector2D(Pos) - Dir * 4000.f, Pos.Z + 200.f);
		return G;
	}
	// A Gale: every exposed point above 300 m, every 4-8 s by the 40 m cell you stand in, along the wind +-20 degrees.
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(WorldContext, WRSkyIsles::Biome());
	if (K && K->Id == TEXT("gale") && Pos.Z >= WRTune(TEXT("hazards.sky.gale.minAltitude"), 30000.f))
	{
		const int32 Cx = FMath::FloorToInt(Pos.X / 4000.f), Cy = FMath::FloorToInt(Pos.Y / 4000.f);
		const float Period = FMath::Lerp(WRTune(TEXT("hazards.sky.gale.everyMin"), 4.f), WRTune(TEXT("hazards.sky.gale.everyMax"), 8.f), H01(Cx, Cy));
		const float Ph = float(FMath::Fmod(T + double(H01(Cy, Cx)) * 13.0, double(Period))) - Tell;
		if (Ph >= -Tell && Ph <= Live)
		{
			const float Bend = FMath::DegreesToRadians((H01(Cx + 3, Cy - 5) * 2.f - 1.f) * WRTune(TEXT("hazards.sky.gale.spread"), 20.f));
			G.bOn = true;
			G.Phase = Ph;
			G.Seconds = Live;
			G.Dir = FVector2D(Wind.X * FMath::Cos(Bend) - Wind.Y * FMath::Sin(Bend), Wind.X * FMath::Sin(Bend) + Wind.Y * FMath::Cos(Bend));
			G.From = FVector(FVector2D(Pos) - G.Dir * 4000.f, Pos.Z + 200.f);
			return G;
		}
	}
	// A lee rotor's down-gust at a lee lip, on foot (W1): toward the edge, 0.5 s, on the rotor's own beat.
	if (bOnFoot)
	{
		const float Period = WRTune(TEXT("hazards.sky.rotor.period"), 4.f);
		const float RTell = WRTune(TEXT("hazards.sky.rotor.tell"), 1.f);
		for (int32 i = 0; i < D.Rotors.Num(); ++i)
		{
			const FWRSkyIsleData::FRotor& R = D.Rotors[i];
			if (FVector2D::Distance(FVector2D(Pos), FVector2D(R.At)) > R.Radius || FMath::Abs(Pos.Z - R.At.Z) > R.Height * 0.5f) continue;
			const FWRSkyIsleData::FIsland* I = D.FindIsland(R.Island);
			if (!I) continue;
			const FVector2D Out = (FVector2D(Pos) - FVector2D(I->Now())).GetSafeNormal();
			if (FVector2D::DotProduct(Wind, Out) < 0.3f) continue;   // the wind must put this lip in the lee
			const float Ph = float(FMath::Fmod(T + i * 1.37, double(Period))) - RTell;
			const float RLive = WRTune(TEXT("hazards.sky.rotor.footSeconds"), 0.5f);
			if (Ph < -RTell || Ph > RLive) continue;
			G.bOn = true;
			G.bRotor = true;
			G.Phase = Ph;
			G.Seconds = RLive;
			G.Dir = Out;
			G.From = R.At;
			return G;
		}
	}
	return G;
}

FVector WRSkyAir::WindPushAt(const ACharacter* Who, const FVector& Pos, bool bOnFoot)
{
	if (!Who || !WRSkyIsles::Contains(Pos)) return FVector::ZeroVector;
	const FGust G = GustAt(Who, Pos, bOnFoot);
	if (!G.bOn || G.Phase < 0.f || G.Phase > G.Seconds) return FVector::ZeroVector;
	const float K = PushScale(Who, TEXT("gust"));
	if (K <= 0.f) return FVector::ZeroVector;
	const float Speed = G.bRotor ? WRTune(TEXT("hazards.sky.rotor.footPush"), 400.f) : WRTune(TEXT("hazards.sky.gust.push"), 1200.f);
	// A rotor's down-gust presses down as well as out.
	return FVector(G.Dir * Speed * K, G.bRotor ? -200.f * K : 0.f);
}

// ================================================================================================ the glider you see

UWRGliderLook::UWRGliderLook()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

namespace WRSkyAirImpl
{
	FLinearColor Lin(uint8 R, uint8 G, uint8 B, float K = 1.f) { return FLinearColor(FColor(R, G, B)) * K; }

	/** A thin panel (a sheet of hide or silk) through Pts (a closed outline in a plane), Thick cm. */
	void Panel(FWRToolMesh& M, const TArray<FVector>& Edge, float Thick, const FLinearColor& C, float Rough)
	{
		// Lofted as two rings (the panel's two faces) so it reads from both sides.
		TArray<TArray<FVector>> Rings;
		TArray<FVector> Up, Dn;
		for (const FVector& P : Edge) { Up.Add(P + FVector(0, 0, Thick * 0.5f)); Dn.Add(P - FVector(0, 0, Thick * 0.5f)); }
		Rings.Add(Dn);
		Rings.Add(Up);
		M.Loft(Rings, C, Rough, true);
	}
	void Spar(FWRToolMesh& M, const TArray<FVector>& Pts, float R, const FLinearColor& C, float Rough = 0.7f)
	{
		M.Tube(Pts, R, 6, C, Rough);
	}

	/** The open wing over the hang point (local: X forward, Y right, Z up; the hang point at the origin, the survivor's
	 *  shoulders 50 cm under it). Span 560 cm (crude) / 620 cm (levistone); dihedral tips 35 cm up. */
	FWRToolMesh OpenWing(uint8 Kind)
	{
		FWRToolMesh M;
		const bool bLevi = Kind == 2;
		const float Half = bLevi ? 310.f : 280.f;
		const FLinearColor SparC = bLevi ? Lin(107, 84, 66, 0.55f) : Lin(139, 107, 74, 0.5f);
		const FLinearColor SkinA = bLevi ? Lin(232, 238, 242, 0.75f) : Lin(160, 120, 80, 0.5f);
		const FLinearColor SkinB = bLevi ? Lin(159, 214, 207, 0.7f) : Lin(128, 92, 60, 0.5f);
		const FLinearColor Lash = Lin(201, 168, 90, 0.5f);
		const FLinearColor Fit = bLevi ? Lin(150, 150, 150, 0.6f) : Lin(184, 115, 51, 0.6f);
		// The leading edge: one bent spar, tips raised (dihedral) and swept back.
		auto Lead = [&](float t) { const float Y = t * Half; return FVector(70.f - 30.f * FMath::Abs(t) * FMath::Abs(t), Y, 35.f * t * t); };
		auto Trail = [&](float t) { const float Y = t * Half * 0.96f; return FVector(-70.f + 20.f * FMath::Abs(t), Y, 30.f * t * t - 6.f); };
		TArray<FVector> LeadPts;
		for (int32 i = -8; i <= 8; ++i) LeadPts.Add(Lead(i / 8.f));
		Spar(M, LeadPts, bLevi ? 3.2f : 3.8f, SparC);
		// The keel down the middle and the hand bar under it on two down-tubes.
		Spar(M, { FVector(78.f, 0, 2.f), FVector(-95.f, 0, -4.f) }, 3.5f, SparC);
		Spar(M, { FVector(20.f, -38.f, -48.f), FVector(20.f, 38.f, -48.f) }, 2.4f, Fit, 0.4f);
		Spar(M, { FVector(40.f, 0.f, 0.f), FVector(20.f, -36.f, -46.f) }, 2.f, SparC);
		Spar(M, { FVector(40.f, 0.f, 0.f), FVector(20.f, 36.f, -46.f) }, 2.f, SparC);
		// Harness lines from the keel to the shoulders.
		Spar(M, { FVector(-10.f, 0.f, -2.f), FVector(-18.f, -16.f, -50.f) }, 0.7f, Lash, 0.95f);
		Spar(M, { FVector(-10.f, 0.f, -2.f), FVector(-18.f, 16.f, -50.f) }, 0.7f, Lash, 0.95f);
		// Ribs (battens) and the skin between them, alternating panels (the Kitewrights' two-tone cloth / the crude
		// wing's stitched hides).
		const int32 Ribs = 6;
		for (int32 s = -1; s <= 1; s += 2)
		{
			for (int32 r = 0; r < Ribs; ++r)
			{
				const float T0 = s * float(r) / Ribs, T1 = s * float(r + 1) / Ribs;
				const FVector A0 = Lead(T0), A1 = Lead(T1), B0 = Trail(T0), B1 = Trail(T1);
				Spar(M, { A1, FMath::Lerp(A1, B1, 0.5f) + FVector(0, 0, 4.f), B1 }, 1.4f, SparC);
				Panel(M, { A0, A1, FMath::Lerp(A1, B1, 0.5f) + FVector(0, 0, 4.f), B1, B0, FMath::Lerp(A0, B0, 0.5f) + FVector(0, 0, 4.f) }, 0.8f, (r % 2) ? SkinA : SkinB, 0.95f);
				// Lashings at each rib's root.
				M.Lathe(A1 - FVector(2.f, 0, 0), FVector(1, 0, 0), 4.f, { FVector(0, 4.6f, 4.6f), FVector(1, 4.6f, 4.6f) }, 6, Lash, 0.95f);
			}
		}
		// The levistone wing's stones float in the spar joints (one at each rib root and the keel's nose).
		if (bLevi)
		{
			const FLinearColor Stone = Lin(169, 196, 217, 0.85f);
			for (int32 r = -Ribs; r <= Ribs; r += 2)
			{
				const FVector P = Lead(float(r) / Ribs) + FVector(-6.f, 0, 9.f);
				M.Lathe(P - FVector(0, 0, 4.f), FVector(0, 0, 1), 8.f, { FVector(0, 0.5f, 0.5f), FVector(0.4f, 3.6f, 3.2f), FVector(1, 0.5f, 0.5f) }, 6, Stone, 0.15f);
			}
		}
		return M;
	}
	/** Folded: the wing rolled round its spars, slung diagonally across the back with two straps. */
	FWRToolMesh Bundle(uint8 Kind)
	{
		FWRToolMesh M;
		const bool bLevi = Kind == 2;
		const FLinearColor Skin = bLevi ? Lin(232, 238, 242, 0.75f) : Lin(160, 120, 80, 0.5f);
		const FLinearColor SparC = bLevi ? Lin(107, 84, 66, 0.55f) : Lin(139, 107, 74, 0.5f);
		const FVector Axis = FVector(0.f, 1.f, 0.55f).GetSafeNormal();
		const FVector Start = -Axis * 62.f;
		M.Lathe(Start, Axis, 124.f, { FVector(0, 5, 5), FVector(0.06f, 9, 8), FVector(0.5f, 10, 9), FVector(0.94f, 9, 8), FVector(1, 5, 5) }, 10, Skin, 0.95f);
		M.Tube({ Start - Axis * 10.f, Start + Axis * 134.f }, 2.6f, 6, SparC, 0.7f);
		const FLinearColor Strap = Lin(74, 56, 40, 0.5f);
		for (const float T : { 0.3f, 0.7f })
		{
			const FVector C = Start + Axis * 124.f * T;
			M.Lathe(C - Axis * 2.f, Axis, 4.f, { FVector(0, 10.6f, 9.6f), FVector(1, 10.6f, 9.6f) }, 10, Strap, 0.9f);
		}
		return M;
	}
}

void UWRGliderLook::Build(uint8 Kind)
{
	BuiltKind = Kind;
	for (USceneComponent* C : { static_cast<USceneComponent*>(Folded), static_cast<USceneComponent*>(Open), Trail.Get() })
	{
		if (C) C->DestroyComponent();
	}
	Folded = nullptr;
	Open = nullptr;
	Trail = nullptr;
	if (Kind == 0) return;
	AActor* Owner = GetOwner();
	if (Kind == 1 || Kind == 2)
	{
		Folded = NewObject<UProceduralMeshComponent>(Owner);
		Folded->SetupAttachment(this);
		Folded->SetRelativeLocation(FVector(-26.f, 0.f, 38.f));
		Folded->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Folded->SetCastShadow(true);
		Folded->RegisterComponent();
		WRSkyAirImpl::Bundle(Kind).ApplyTo(Folded);
		Open = NewObject<UProceduralMeshComponent>(Owner);
		Open->SetupAttachment(this);
		Open->SetRelativeLocation(FVector(-6.f, 0.f, 140.f));
		Open->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Open->SetVisibility(false);
		Open->RegisterComponent();
		WRSkyAirImpl::OpenWing(Kind).ApplyTo(Open);
	}
	else
	{
		// The Path Glide and Skysilk's glide: the wind itself, streaming off the shoulders while it carries you.
		Trail = WRFx::Attach(this, TEXT("sky.windWing"), NAME_None, 1.f);
		if (Trail) Trail->SetVisibility(false, true);
	}
}

void UWRGliderLook::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ACharacter* Ch = Cast<ACharacter>(GetOwner());
	const bool bGlide = WRSkyAir::IsGliding(Ch);
	// Eased open over 0.25 s and shut over 0.2 s: the spars swing out from the keel, never snap.
	Spread = FMath::FInterpConstantTo(Spread, bGlide ? 1.f : 0.f, DeltaTime, bGlide ? 4.f : 5.f);
	const float E = Spread * Spread * (3.f - 2.f * Spread);
	if (Open)
	{
		Open->SetVisibility(E > 0.02f);
		Open->SetRelativeScale3D(FVector(FMath::Lerp(0.4f, 1.f, E), FMath::Max(0.05f, E), FMath::Lerp(0.4f, 1.f, E)));
		// Bank into turns: roll with the sideways acceleration, pitch with the sink.
		if (Ch && E > 0.02f)
		{
			const FVector V = Ch->GetVelocity();
			const FVector Right = Ch->GetActorRightVector();
			const float Roll = FMath::Clamp(FVector::DotProduct(Ch->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D(), Right) * 14.f, -14.f, 14.f);
			const float Pitch = FMath::Clamp(V.Z / 60.f, -10.f, 8.f);
			Open->SetRelativeRotation(FMath::RInterpTo(Open->GetRelativeRotation(), FRotator(Pitch, 0.f, Roll), DeltaTime, 4.f));
		}
	}
	if (Folded) Folded->SetVisibility(E < 0.98f);
	if (Trail) Trail->SetVisibility(bGlide, true);
	// The cloth's flap: louder with speed, only while it carries you.
	if (bGlide && !Flap && Ch)
	{
		Flap = WRSound::StartLoop(const_cast<ACharacter*>(Ch), BuiltKind <= 2 ? FName(TEXT("sky.glider_flap")) : FName(TEXT("sky.wind_glide")));
	}
	if (Flap)
	{
		if (!bGlide) { Flap->FadeOut(0.3f, 0.f); Flap = nullptr; }
		else if (Ch) Flap->SetVolumeMultiplier(FMath::GetMappedRangeValueClamped(FVector2D(300.f, 1400.f), FVector2D(0.35f, 1.f), Ch->GetVelocity().Size()));
	}
}

// ================================================================================================ the subsystem

void UWRSkyAirSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	if (World->GetNetMode() != NM_Client) StepFalls(DeltaTime);
	LookClock -= DeltaTime;
	if (LookClock <= 0.f && World->GetNetMode() != NM_DedicatedServer)
	{
		LookClock = 0.25f;
		StepLooks();
	}
}

void UWRSkyAirSubsystem::StepLooks()
{
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		AWRCharacter* C = *It;
		UWRGliderLook* L = C->FindComponentByClass<UWRGliderLook>();
		const uint8 Want = C->GliderKind;
		if (!L && Want == 0) continue;
		if (!L)
		{
			L = NewObject<UWRGliderLook>(C);
			L->SetupAttachment(C->GetRootComponent());
			L->RegisterComponent();
		}
		if (L->BuiltKind != Want) L->Build(Want);
	}
}

void UWRSkyAirSubsystem::StepFalls(float Dt)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	WearClock += Dt;
	const bool bWear = WearClock >= 1.f;
	if (bWear) WearClock -= 1.f;
	// Survivors everywhere (fall damage is game-wide); creatures only where edges are (the Sky).
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* C = *It;
		AWRCharacterBase* Base = Cast<AWRCharacterBase>(C);
		if (!Base || !Base->IsAlive())
		{
			Falls.Remove(C);
			continue;
		}
		AWRCharacter* Survivor = Cast<AWRCharacter>(C);
		const bool bSky = WRSkyIsles::Contains(C->GetActorLocation());
		if (!Survivor && !bSky)
		{
			Falls.Remove(C);
			continue;
		}
		if (Survivor)
		{
			// Which glider shows on the back (every machine draws it from this): 1 crude, 2 levistone, 3 the wind.
			FWRGlideParams P;
			const uint8 Kind = !WRSkyAir::GlideParams(C, P) ? 0 : P.Kind == WRSkyAirImpl::NLevi ? 2 : P.Kind == WRSkyAirImpl::NCrude ? 1 : 3;
			if (Survivor->GliderKind != Kind) Survivor->GliderKind = Kind;
			// A glider wears a second a second while it carries you (300 s crude, 900 s levistone).
			if (bWear && WRSkyAir::IsGliding(C) && (Kind == 1 || Kind == 2))
			{
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(C))
				{
					for (FWRItemStack& S : Inv->GetSlotsMutable())
					{
						if (S.ItemId != P.Kind || S.Durability <= 0.f) continue;
						S.Durability = FMath::Max(0.f, S.Durability - 1.f);
						if (S.Durability <= 0.f)
						{
							if (AWRPlayerController* PC = Cast<AWRPlayerController>(C->GetController())) PC->ClientNotify(TEXT("Your glider tears: it needs mending at a workbench"));
							WRSound::Broadcast(C, TEXT("sky.glider_tear"));
						}
						break;
					}
				}
			}
		}
		FFall& F = Falls.FindOrAdd(C);
		const UCharacterMovementComponent* M = C->GetCharacterMovement();
		const float Z = C->GetActorLocation().Z;
		const bool bGlide = WRSkyAir::IsGliding(C);
		if (F.bWasGliding && !bGlide) { F.GlideClosedAt = Now; F.GlideClosedZ = Z; }
		if (bGlide && Survivor)
		{
			// The Wind path's Path Glide spends Momentum, then stamina (WRWindPath); an updraft re-arms its air dash.
			const bool bLift = WRSkyIsles::LiftAt(C, C->GetActorLocation()) > 50.f;
			if (bLift && !F.bInLift) WRWind::OnEnteredUpdraft(C);
			F.bInLift = bLift;
			FWRGlideParams GP;
			if (WRSkyAir::GlideParams(C, GP) && GP.Kind == TEXT("path") && !WRWind::PathGlideTick(Survivor, Dt, bLift))
			{
				C->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		else F.bInLift = false;
		F.bWasGliding = bGlide;
		const bool bHeld = !M || bGlide || M->IsSwimming() || M->IsFlying() || M->MovementMode == MOVE_Custom || C->GetAttachParentActor() != nullptr;
		if (Survivor && bSky) StepDeeds(Survivor, M && M->IsMovingOnGround() && !bHeld, bGlide, C->GetAttachParentActor() != nullptr || (M && M->MovementMode == MOVE_Custom && !bGlide));
		if (bHeld)
		{
			F.TopZ = Z;
			F.bAir = false;
			continue;
		}
		if (M->IsFalling())
		{
			if (!F.bAir) { F.bAir = true; F.TopZ = Z; F.AirSince = Now; }
			F.TopZ = FMath::Max(F.TopZ, Z);
			// A creature gone over an edge: it won't come back from this far down; the kill is whoever sent it.
			if (!Survivor && F.TopZ - Z > WRTune(TEXT("skyWorld.fall.creatureGoneDrop"), 6000.f))
			{
				Landed(C, F, F.TopZ - Z);
				F.bAir = false;
			}
			continue;
		}
		if (F.bAir)
		{
			F.bAir = false;
			Landed(C, F, F.TopZ - Z);
		}
		F.TopZ = Z;
	}
}

void UWRSkyAirSubsystem::StepDeeds(AWRCharacter* Who, bool bStanding, bool bGlide, bool bHeld)
{
	// Yara Kesh's quests counted where they happen (cooks/biome-sky.md 2.13; world.json quests.yara: first_flight,
	// updraft_top, broken_way). A mount, a lift's cage or a line spoils a clean flight or crossing.
	UWRWorldSystems* Sys = Who->GetWorld()->GetSubsystem<UWRWorldSystems>();
	AWRPlayerState* PS = Who->GetPlayerState<AWRPlayerState>();
	if (!Sys || !PS) return;
	FDeeds& D = Deeds.FindOrAdd(Who);
	const FVector P = Who->GetActorLocation();
	const FWRSkyIsleData& Data = FWRSkyIsleData::Get();
	if (bHeld) { D.bClean = false; D.bCrossing = false; }
	if (bGlide)
	{
		// Read the wind: a column ridden to its top (the top 30 m of it, inside its radius).
		for (int32 i = 0; i < Data.Updrafts.Num(); ++i)
		{
			const FWRSkyIsleData::FUpdraft& U = Data.Updrafts[i];
			if (D.Tops.Contains(i) || FVector2D::Distance(FVector2D(P), U.At) > U.Radius || P.Z < U.Top - WRTune(TEXT("skyWorld.quests.topBand"), 3000.f)) continue;
			D.Tops.Add(i);
			Sys->QuestEvent(PS, TEXT("updraft_top"), NAME_None);
		}
	}
	if (!bStanding) return;
	const FWRSkyIsleData::FIsland* I = Data.IslandAt(P, 0.f, 400.f, 400.f);
	const FName Here = I ? I->Id : FName(TEXT("ground"));
	if (Here == D.Stood) { D.bClean = true; return; }
	// First flight: off the First Isle and onto Terrace Isle without touching ground between.
	if (D.Stood == TEXT("first_isle") && Here == TEXT("terrace_isle") && D.bClean) Sys->QuestEvent(PS, TEXT("first_flight"), NAME_None);
	// The Broken Way: Tollgate (with the Toll Shrine lit) to Brink on your own feet, wings or rope.
	const AWRSkyState* St = AWRSkyState::Get(Who);
	if (Here == TEXT("tollgate")) D.bCrossing = St && St->bTollLit;
	else if (Here == TEXT("brink") && D.bCrossing) { D.bCrossing = false; Sys->QuestEvent(PS, TEXT("broken_way"), NAME_None); }
	else if (Here == TEXT("ground") || (I && I->Zone != '3')) D.bCrossing = false;
	D.Stood = Here;
	D.bClean = true;
}

void UWRSkyAirSubsystem::Landed(ACharacter* Who, FFall& F, float Drop)
{
	const float Meters = Drop / 100.f;
	const float From = WRTune(TEXT("fall.hurtFrom"), 6.f), Kill = WRTune(TEXT("fall.killFrom"), 18.f);
	if (Meters < From) return;
	AWRCharacterBase* Base = Cast<AWRCharacterBase>(Who);
	if (!Base || !Base->GetAttributes()) return;
	if (AWRCreature* Cr = Cast<AWRCreature>(Who))
	{
		// Bosses never die of a fall (WRSkyBosses lifts them back onto an island); the Sky-born parachute (resist
		// "fall": WRSkyCreatures); the rest die from 18 m.
		const FWRCreatureDef* Def = Cr->GetDef();
		if (Cast<AWRBoss>(Cr) || Meters < Kill || (Def && Def->Resists.Contains(TEXT("fall")))) return;
		FWRHit Hit;
		Hit.Damage = 1.0e7f;
		Hit.bUnblockable = true;
		Hit.bDamageOverTime = true;
		Hit.Instigator = WRSkyAir::FallCredit(Cr);
		Hit.Origin = Cr->GetActorLocation() + FVector(0, 0, 100.f);
		UWRCombatLibrary::ApplyHit(Cr, Hit);
		return;
	}
	// Survivors: Featherfall (the Wind root) takes no fall damage; the Kite String's own rule replaces this one.
	if (WRSkyAir::HasFeatherfall(Who)) return;
	if (WRArtifacts::Has(Who, TEXT("brittle_falls"))) return;
	float Mult = FMath::Max(0.f, 1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("fallDamage")));
	if (WRStorm::HasBuff(Who, TEXT("featherdown"))) Mult *= WRTune(TEXT("fall.featherdownMult"), 0.5f);
	if (Mult <= 0.f) return;
	const float MaxHp = Base->GetAttributes()->GetMaxHealth();
	const float Share = (Meters - From) / FMath::Max(1.f, Kill - From);
	FWRHit Hit;
	Hit.Damage = MaxHp * Share * Mult;
	if (Share >= 1.f && Mult >= 1.f) Hit.Damage = 1.0e7f;
	Hit.bUnblockable = true;
	Hit.bDamageOverTime = true;
	Hit.bBurst = true;   // the ground meeting you: a blow's whole stack
	Hit.Origin = Who->GetActorLocation() + FVector(0, 0, 100.f);
	UWRCombatLibrary::ApplyHit(Base, Hit);
	WRSound::Broadcast(Who, Meters >= Kill * 0.75f ? TEXT("sky.fall_heavy") : TEXT("sky.fall_land"));
}
