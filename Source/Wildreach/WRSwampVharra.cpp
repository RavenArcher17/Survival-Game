// Vharra, Queen of the Mire, in the Mother's Throat (cooks/biome-swamp.md 2.13-2.14, 2.10, 2.15-2.16;
// cooks/build-biome-swamp.md 2.7, 2.10; cooks/combat.md 2.17). See the "Vharra" block of WRSwampBosses.h.
//
// The fight, server side: anchored to the west wall by her sucker disc in phase 1 (six moves and the Brood shed), the
// Sinking at 50% (the surge, the flood), the swimming queen in phase 2, the Water Turns at 25% with artifacts (blood
// water, clot crust). The lightning counter: the six Rodwarden rods (AWRSwampRod), the Throat's own water
// (AWRSwampFlood), strikes, Lightning spells and stormglass jars; a shock that finds her in the water convulses her.
// Every machine that draws builds her look: the MountainDragon front with its hind legs, tail, wings and head hidden,
// the three-plate hood that is her skull and her mouth, ten eyes, three blood sacs that fill, rust stripes, the 14
// ringed trunk segments to the 3 m anchor disc. Numbers: Data/tuning.json "swampVharra" (and bosses.vharra for the
// boss machine's own keys); moves: Data/attacks.json bosses.vharra.
#include "WRSwampBosses.h"
#include "WRSwampTypes.h"
#include "WRSwampSystems.h"
#include "WRStormTypes.h"
#include "WRLightningPath.h"
#include "WRBoss.h"
#include "WRAltar.h"
#include "WRAttributeSet.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"

// Unity builds can put WRSwampScenery.cpp (its palette names at file scope through `using namespace WRSwampLook`) ahead of
// this file; its own same-named surfaces are local on purpose, so the shadowing warning is off for this file only.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4459)
#endif

namespace WRSwampVharraLocal
{
	FName VhQueenId() { static const FName N(TEXT("vharra")); return N; }
	FName VhBroodId() { static const FName N(TEXT("vharra_brood")); return N; }

	/** swampVharra.<Key>. */
	float VhT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("swampVharra.%s"), Key), Fallback); }

	double VhNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }

	/** The server's clock as every machine sees it (the flood's rise, a rod's cooldown, the shock's window). */
	float VhServerTime(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : World ? World->GetTimeSeconds() : 0.f;
	}

	float VhDamageScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }

	FLinearColor VhHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }

	FVector VhFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }

	/** The land alone under a point (no rods, arena props, bodies or the flood): where plinths and seats stand. */
	FVector VhLand(const UWorld* World, const FVector& At, bool& bOk, float Up = 3000.f, float Down = 8000.f)
	{
		bOk = false;
		if (!World) return At;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRVharraLand), false);
		for (TActorIterator<AWRSwampRod> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AWRSwampBossProp> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < 4000.f) Q.AddIgnoredActor(*It);
		FHitResult Hit;
		bOk = World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q);
		return bOk ? FVector(Hit.ImpactPoint) : At;
	}

	float VhSegDist2D(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector2D AB(B - A), AP(P - A);
		const float T = FMath::Clamp(FVector2D::DotProduct(AP, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
		return FVector2D::Distance(FVector2D(A) + AB * T, FVector2D(P));
	}

	/** Bearing of P from Here relative to Yaw, degrees (-180..180, + = to the right). */
	float VhBearing(const FVector& Here, float Yaw, const FVector& P)
	{
		return FMath::FindDeltaAngleDegrees(Yaw, (P - Here).GetSafeNormal2D().Rotation().Yaw);
	}

	float VhHealthFrac(const AWRCharacterBase* Who)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
	}

	float VhMaxHealth(const AWRCharacterBase* Who)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		return A ? A->GetMaxHealth() : 1.f;
	}

	void VhHeal(AWRCharacterBase* Who, float Amount)
	{
		UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		if (ASC && A && Amount > 0.f && Who->IsAlive())
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
		}
	}

	void VhSetHealth(AWRCreature* C, float Health)
	{
		if (UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), Health);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Health);
		}
	}

	/** A status meter straight onto someone (the shocked water, the blood flood): the meter, no blow. */
	void VhBuildup(AWRCharacterBase* Who, int32 StatusId, float Amount, AActor* Source)
	{
		if (Who && Who->IsAlive() && Who->Status && Amount > 0.f)
		{
			Who->Status->AddStatusBuildup(StatusId, Amount, Cast<AWRCharacterBase>(Source));
		}
	}

	/** Grounded (the Lightning path's root passive or main path), a grounding charm or a grounding draught: the shocked
	 *  water doesn't touch them (cook 2.14 SPEC). */
	bool VhGrounded(const AWRCharacterBase* V)
	{
		if (!V) return false;
		if (WRLightning::IsGrounded(V) || WRStorm::HasBuff(V, TEXT("grounding"))) return true;
		const AWRPlayerState* PS = V->GetPlayerState<AWRPlayerState>();
		if (PS && PS->GetPaths() && PS->GetPaths()->GetMainPath() == TEXT("lightning")) return true;
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(V);
		return Inv && Inv->GetEquipped(EWREquipSlot::Talisman).ItemId == TEXT("grounding_charm");
	}

	/** Riding a Mirestrider (the bird stands tall): out of the water. */
	bool VhOnTallMount(const AActor* Who)
	{
		const AWRCreature* M = Who ? Cast<AWRCreature>(Who->GetAttachParentActor()) : nullptr;
		return M && M->CreatureId == TEXT("mirestrider");
	}

	/** A blow of hers outside the row machine (a grab's squeeze, a wave, a spray tick, a clot). Through ApplyHit. */
	void VhHit(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Bleed, bool bUnblockable, bool bLow, uint8 Impact, bool bOverTime, float Knock, const FVector& Origin)
	{
		if (!Boss || !Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * VhDamageScale() * (Boss->Phase >= 3 ? WRTune(TEXT("bosses.vharra.phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = bOverTime ? 0.f : Damage * 1.5f;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bLow = bLow;
		Hit.bHeavy = !bOverTime;
		Hit.bDamageOverTime = bOverTime;
		Hit.Tier = bOverTime ? EWRHitTier::Light : EWRHitTier::Heavy;
		Hit.Origin = Origin.IsZero() ? Boss->GetActorLocation() : Origin;
		Hit.Element = WRTags::Element_Physical;
		Hit.PhysType = 2;
		if (Bleed > 0.f) Hit.Buildups.Add(TEXT("bleed"), Bleed);
		const TWeakObjectPtr<AWRCharacterBase> WeakWho(Victim);
		const FVector From = Hit.Origin;
		UWRCombatLibrary::ApplyHit(Victim, Hit, [WeakWho, Knock, From](EWRHitResult Landed)
		{
			AWRCharacterBase* V = WeakWho.Get();
			if (V && V->IsAlive() && Knock > 0.f && (Landed == EWRHitResult::Hit || Landed == EWRHitResult::GuardBroken))
			{
				const FVector Push = (V->GetActorLocation() - From).GetSafeNormal2D() * Knock + FVector(0.f, 0.f, Knock * 0.45f);
				V->LaunchCharacter(Push, true, true);
			}
		});
	}

	/** The creatures.json row for an id (her palette, her lock points' places). */
	TSharedPtr<FJsonObject> VhRow(FName Id)
	{
		static TMap<FName, TSharedPtr<FJsonObject>> Rows;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
					FString S;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), S)) Rows.Add(FName(*S), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Found = Rows.Find(Id);
		return Found ? *Found : nullptr;
	}

	double VhNum(const TSharedPtr<FJsonObject>& O, const TCHAR* F, double Fallback)
	{
		double V = Fallback;
		if (O.IsValid()) O->TryGetNumberField(F, V);
		return V;
	}

	FLinearColor VhColor(const TSharedPtr<FJsonObject>& O, const TCHAR* F, const FLinearColor& Fallback)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Fallback;
	}

	/** A ring of the Throat (swamp.json arenas.vharra.rings.<name> = [inner, outer, floor z] metres) in cm. */
	bool VhRing(const TSharedPtr<FJsonObject>& Arena, const TCHAR* Name, float& OutInner, float& OutOuter, float& OutZ)
	{
		const TSharedPtr<FJsonObject>* Rings = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!Arena.IsValid() || !Arena->TryGetObjectField(TEXT("rings"), Rings) || !(*Rings)->TryGetArrayField(Name, A) || A->Num() < 3) return false;
		OutInner = float((*A)[0]->AsNumber()) * 100.f;
		OutOuter = float((*A)[1]->AsNumber()) * 100.f;
		OutZ = float((*A)[2]->AsNumber()) * 100.f;
		return true;
	}

	FVector VhArenaCentre(const TSharedPtr<FJsonObject>& O)
	{
		return O.IsValid() ? FVector(VhNum(O, TEXT("x"), 0.0), VhNum(O, TEXT("y"), 0.0), VhNum(O, TEXT("z"), 0.0)) * 100.0 : FVector::ZeroVector;
	}

	/** The Throat's flood near a point. */
	AWRSwampFlood* VhFlood(const UWorld* World, const FVector& Near, float Range = 12000.f)
	{
		if (!World) return nullptr;
		for (TActorIterator<AWRSwampFlood> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Near) < Range) return *It;
		}
		return nullptr;
	}

	/** Vharra in this world near a point (alive), or null. */
	AWRBoss* VhQueen(const UWorld* World, const FVector& Near, float Range = 15000.f)
	{
		if (!World) return nullptr;
		for (TActorIterator<AWRBoss> It(World); It; ++It)
		{
			if (It->CreatureId == VhQueenId() && It->IsAlive() && FVector::Dist2D(It->GetHome(), Near) < Range) return *It;
		}
		return nullptr;
	}

	/** Feet in the Throat's water (wading or swimming); a rider on a Mirestrider, or anyone carried, is not. */
	bool VhInWater(const AWRSwampFlood* Flood, const AActor* Who)
	{
		if (!Flood || !Who || VhOnTallMount(Who)) return false;
		if (Who->GetAttachParentActor() && !Cast<AWRBoss>(Who->GetAttachParentActor())) return false;
		return Flood->InWater(VhFeet(Who));
	}

	/** Every survivor and tame she may fight in the arena. */
	TArray<AWRCharacterBase*> VhFoes(const AWRBoss* Boss, float Range)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It) || It->GetAttachParentActor()) continue;
			if (FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) > Range) continue;
			if (const AWRCreature* C = Cast<AWRCreature>(*It); C && !C->IsTamed()) continue;
			Out.Add(*It);
		}
		return Out;
	}

	/** Everyone in or near a point whose feet the water reaches (survivors, tames, riders' mounts, her brood). */
	TArray<AWRCharacterBase*> VhPeople(const UWorld* World, const FVector& At, float Radius)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (!It->IsAlive() || It->IsA<AWRBoss>()) continue;
			if (FVector::Dist2D(It->GetActorLocation(), At) <= Radius + It->GetSimpleCollisionRadius()) Out.Add(*It);
		}
		return Out;
	}

	/** Pinned to her for a grab (attached) or let go again. */
	void VhHold(AWRCharacterBase* Victim, AActor* Holder, const FVector& RelativeAt, bool bHold)
	{
		if (!Victim) return;
		if (bHold)
		{
			Victim->SetActorEnableCollision(false);
			Victim->GetCharacterMovement()->DisableMovement();
			Victim->AttachToActor(Holder, FAttachmentTransformRules::KeepWorldTransform);
			Victim->SetActorRelativeLocation(RelativeAt);
		}
		else
		{
			Victim->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Victim->SetActorEnableCollision(true);
			Victim->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			if (Victim->Status) Victim->Status->ClearStatus(WRTags::Status_Staggered);
		}
	}

	/** Server: one of the Bosses track's arena pieces (mound bones, a bleedwater slick, a clot puddle, the clot crust). */
	AWRSwampBossProp* VhProp(UWorld* World, FName Kind, const FVector& At, float Size, uint8 Variant, float Rise, float Yaw)
	{
		if (!World) return nullptr;
		const FTransform Where(FRotator(0.f, Yaw, 0.f), At);
		AWRSwampBossProp* P = World->SpawnActorDeferred<AWRSwampBossProp>(AWRSwampBossProp::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (P)
		{
			P->Kind = Kind;
			P->Size = Size;
			P->Variant = Variant;
			P->Rise = Rise;
			P->FinishSpawning(Where);
		}
		return P;
	}

	/** Server: a puddle that hurts (bleed a second, slow) for Seconds where Vharra's blood lands. */
	AWRSwampBossProp* VhPuddle(UWorld* World, const FVector& At, float Radius, float Seconds, float BleedPerSecond, bool bSlow, uint8 Variant, AActor* Source)
	{
		AWRSwampBossProp* P = VhProp(World, TEXT("puddle"), At, Radius, Variant, 0.f, FMath::FRandRange(0.f, 360.f));
		if (P)
		{
			P->MakeHazard(BleedPerSecond, bSlow, Seconds, Source);
			P->SetLifeSpan(Seconds + 2.f);
		}
		return P;
	}

	/** The arena as the boss uses it (cook 2.14: the Throat), found once from Data/swamp.json arenas.vharra. */
	struct FWRVharraArena
	{
		bool bFound = false;
		FVector Centre = FVector::ZeroVector, Seat = FVector::ZeroVector, AnchorAt = FVector::ZeroVector, StairTop = FVector::ZeroVector;
		float SeatYaw = 0.f, PoolR = 2000.f, FloodR = 4500.f, LowerR = 3200.f, WaterZ = 60.f, PoolFloor = -800.f;
		TArray<FVector> Mounds, Plinths;
	};

	/** Server: one fight (and her life between fights). */
	struct FWRVharraFight
	{
		FWRVharraArena Arena;
		TArray<TWeakObjectPtr<AWRSwampRod>> Rods;
		TWeakObjectPtr<AWRSwampFlood> Flood;
		double NextLookFor = 0.0;
		// The sacs (part HP each), how full they show, which burst; her drain heal (each burst cuts a third).
		float SacHP[3] = { 1200.f, 1200.f, 1200.f };
		float Fill[3] = { 0.f, 0.f, 0.f };
		uint8 Burst = 0;
		float HealMult = 1.f;
		// The lightning counter.
		double ConvulseUntil = 0.0, ConvulseGapUntil = 0.0, NoHealUntil = 0.0, NextThrashAt = 0.0;
		bool bConvulsing = false;
		// The brood.
		TArray<TWeakObjectPtr<AWRCreature>> Brood;
		double NextBroodAt = 0.0, NextBroodLook = 0.0;
		// A grab: the three-jaw drain (lifted) or the drag (under).
		TWeakObjectPtr<AWRCharacterBase> Grabbed;
		FName GrabKind;
		double GrabUntil = 0.0, GrabTickAt = 0.0;
		float GrabDps = 0.f;
		// A move's own stages.
		int32 Stage = 0;
		double StageAt = 0.0, NextTickAt = 0.0, NextFxAt = 0.0;
		FVector Aim = FVector::ZeroVector, MoveFrom = FVector::ZeroVector, CoilAt = FVector::ZeroVector;
		float SweepFrom = 0.f, SweepTo = 0.f, LastAngle = 0.f;
		TArray<FVector> ClotSpots;
		int32 Puddles = 0;
		// A ring wave along the water (the heave's ring, the Sinking's surge).
		bool bWave = false, bWaveLow = true, bWaveWaterOnly = false, bWaveUnblockable = false;
		double WaveAt = 0.0;
		FVector WaveFrom = FVector::ZeroVector;
		float WaveRadius = 0.f, WaveDamage = 0.f, WaveHeight = 0.f, WaveKnock = 0.f;
		uint8 WaveImpact = 3;
		TSet<TWeakObjectPtr<AWRCharacterBase>> WaveHit;
		// The hidden phase.
		TArray<TWeakObjectPtr<AWRSwampBossProp>> Crusts;
		double NextBloodTick = 0.0, NextKindCheck = 0.0;
		// Phase 2's swim: where she heads between moves.
		FVector Heading = FVector::ZeroVector;
	};

	TMap<TWeakObjectPtr<const AWRBoss>, FWRVharraFight>& VhStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FWRVharraFight> Map;
		return Map;
	}
	FWRVharraFight& VhS(const AWRBoss* Boss) { return VhStates().FindOrAdd(Boss); }
	const FWRVharraFight* VhFind(const AWRBoss* Boss) { return VhStates().Find(Boss); }

	/** Lightning landing in the Throat's water: the last 2 s of it (the 60-in-2-s rule) and the people already shocked
	 *  in the current 1.5 s window, per flood. */
	struct FWRVharraWater
	{
		TArray<TPair<double, float>> Log;
		double NextShockAt = 0.0;
		float WindowUntil = 0.f;
		TSet<TWeakObjectPtr<AWRCharacterBase>> Shocked;
		TWeakObjectPtr<AActor> By;
	};
	TMap<TWeakObjectPtr<const AWRSwampFlood>, FWRVharraWater>& VhWaters()
	{
		static TMap<TWeakObjectPtr<const AWRSwampFlood>, FWRVharraWater> Map;
		return Map;
	}

	/** The arena from swamp.json (metres) in world cm: centre, rings, the anchor on the west wall, her seat east of it
	 *  on the lower ring, the mounds, the rods' plinths, the stair's top. */
	FWRVharraArena VhReadArena(const UWorld* World)
	{
		FWRVharraArena A;
		const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(VhQueenId());
		if (!Row.IsValid()) return A;
		A.Centre = VhArenaCentre(Row);
		float In = 0.f, Out = 0.f, Z = 0.f;
		if (VhRing(Row, TEXT("pool"), In, Out, Z)) { A.PoolR = Out; A.PoolFloor = Z; }
		if (VhRing(Row, TEXT("lower"), In, Out, Z)) A.LowerR = Out;
		if (VhRing(Row, TEXT("middle"), In, Out, Z)) A.FloodR = Out;
		A.WaterZ = FWRSwampData::Get().bLoaded ? FWRSwampData::Get().SeaZ : 60.f;
		A.AnchorAt = WRSwamp::Point(Row, TEXT("anchor"));
		if (A.AnchorAt.IsZero()) A.AnchorAt = A.Centre - FVector(A.FloodR + 1200.f, 0.f, 0.f);
		A.Mounds = WRSwamp::Points(Row, TEXT("mounds"));
		A.Plinths = WRSwamp::Points(Row, TEXT("rods"));
		const TArray<FVector> Stair = WRSwamp::Points(Row, TEXT("stair"));
		A.StairTop = Stair.Num() > 0 ? Stair[0] : A.Centre + FVector(7600.f, 0.f, 0.f);
		// She lies east of her disc, facing the open Throat (the fight is a half circle toward her: cook 2.14).
		const FVector In2D = (A.Centre - A.AnchorAt).GetSafeNormal2D();
		A.SeatYaw = In2D.Rotation().Yaw;
		const FVector SeatXY = A.AnchorAt + In2D * VhT(TEXT("seatFromAnchor"), 2700.f);
		bool bOk = false;
		const FVector G = VhLand(World, FVector(SeatXY.X, SeatXY.Y, A.Centre.Z), bOk, 4000.f, 8000.f);
		A.Seat = bOk ? G : FVector(SeatXY.X, SeatXY.Y, 0.f);
		// The disc presses the wall a little above the water (the cliff's foot).
		bool bWall = false;
		const FVector WallFoot = VhLand(World, FVector(A.AnchorAt.X, A.AnchorAt.Y, A.Centre.Z), bWall, 4000.f, 8000.f);
		A.AnchorAt.Z = FMath::Max(bWall ? WallFoot.Z : A.AnchorAt.Z, A.WaterZ) + VhT(TEXT("discAboveWater"), 120.f);
		A.bFound = true;
		return A;
	}

	/** High ground in the Throat: the four ossuary mounds and the six rod plinths (where the drowning coil goes). */
	TArray<FVector> VhHighGround(const FWRVharraArena& A)
	{
		TArray<FVector> Out = A.Mounds;
		Out.Append(A.Plinths);
		return Out;
	}
}
using namespace WRSwampVharraLocal;

namespace WRSwampVharraLocal
{
	UWRSwampBossBody* VhBody(AWRCreature* C)
	{
		UWRSwampBossBody* Body = UWRSwampBossBody::Get(C);
		if (!Body && C && C->HasAuthority()) Body = UWRSwampBossBody::Attach(C);
		return Body;
	}

	/** Finds (once, then again if one goes) the Throat's rods and flood round her arena. */
	void VhEnsureArena(AWRBoss* Boss, FWRVharraFight& St)
	{
		if (!St.Arena.bFound) St.Arena = VhReadArena(Boss->GetWorld());
		const double T = VhNow(Boss);
		bool bMissing = !St.Flood.IsValid() || St.Rods.Num() == 0;
		for (const TWeakObjectPtr<AWRSwampRod>& R : St.Rods) bMissing |= !R.IsValid();
		if (!bMissing || T < St.NextLookFor) return;
		St.NextLookFor = T + 2.0;
		const FVector Centre = St.Arena.bFound ? St.Arena.Centre : Boss->GetHome();
		St.Rods.Reset();
		for (TActorIterator<AWRSwampRod> It(Boss->GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Centre) < 8000.f) St.Rods.Add(*It);
		}
		St.Flood = VhFlood(Boss->GetWorld(), Centre, 8000.f);
	}

	/** Her own body in the Throat's water (the shock finds her there: phase 1 on the lower ring, phase 2 nearly always). */
	bool VhQueenInWater(const AWRBoss* Boss, const FWRVharraFight& St)
	{
		const AWRSwampFlood* Flood = St.Flood.Get();
		if (!Flood) return false;
		// Her body is long: the water at her feet, or under her trunk's first rings behind her.
		const FVector Feet = VhFeet(Boss);
		return Flood->InWater(Feet) || Flood->InWater(Feet - Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() * 1.5f);
	}

	/** Where her anchor disc is on the server (no mesh here): on the wall in phase 1, behind her at the trunk's end after. */
	FVector VhDiscPoint(const AWRBoss* Boss, const FWRVharraFight& St)
	{
		if (Boss->Phase < 2 && St.Arena.bFound) return St.Arena.AnchorAt;
		return Boss->GetActorLocation() - Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() * 0.6f + VhT(TEXT("trunkLength"), 1800.f))
			- FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.6f);
	}

	/** Her mouth (the hood's tip) on the server. */
	FVector VhMouth(const AWRBoss* Boss)
	{
		return Boss->GetActorLocation() + Boss->GetActorForwardVector() * Boss->GetSimpleCollisionRadius() * 1.25f + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.45f);
	}

	/** The water's surface in her Throat now (cm). */
	float VhSurface(const FWRVharraFight& St)
	{
		return St.Flood.IsValid() ? St.Flood->SurfaceZ() : St.Arena.WaterZ;
	}

	/** The body's moment cues back to rest after a move (the hood, the trunk, the rear, a coil). */
	void VhRestLook(AWRBoss* Boss, FWRVharraFight& St)
	{
		UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
		if (!Body) return;
		if (!St.Grabbed.IsValid()) Body->Set(WRSwampBossFlag::JawOpen, false);
		Body->Set(WRSwampBossFlag::Rising, false);
		Body->Lash = 0;
		if (Boss->Phase >= 2) Body->Anchor = FVector::ZeroVector;
	}

	/** The sacs show what she drank (they ease back down; a burst one stays empty). */
	void VhFillSacs(FWRVharraFight& St, float Amount)
	{
		int32 Open = 0;
		for (int32 i = 0; i < 3; ++i) Open += (St.Burst & (1 << i)) ? 0 : 1;
		if (Open == 0) return;
		for (int32 i = 0; i < 3; ++i)
		{
			if (!(St.Burst & (1 << i))) St.Fill[i] = FMath::Clamp(St.Fill[i] + Amount / Open, 0.f, 1.f);
		}
	}

	/** Server: a grab's end (time up, a convulse, a stance break, her death): the body drops back into the water. */
	void VhReleaseGrab(AWRBoss* Boss, FWRVharraFight& St)
	{
		AWRCharacterBase* V = St.Grabbed.Get();
		St.Grabbed = nullptr;
		St.GrabKind = NAME_None;
		if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss))
		{
			Body->Set(WRSwampBossFlag::JawOpen, false);
		}
		if (!V) return;
		VhHold(V, Boss, FVector::ZeroVector, false);
		if (V->IsAlive())
		{
			const FVector Away = (V->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
			V->LaunchCharacter(Away * 350.f + FVector(0.f, 0.f, 200.f), true, true);
		}
		UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra lets %s go"), *V->GetName());
	}

	/** Server: the catch of the three-jaw drain (lifted in the open hood) or the drag (pulled under). */
	void VhStartGrab(AWRBoss* Boss, FWRVharraFight& St, AWRCharacterBase* Victim, FName Kind, float Seconds, float Dps)
	{
		if (!Victim || !Victim->IsAlive() || St.Grabbed.IsValid()) return;
		const double T = VhNow(Boss);
		St.Grabbed = Victim;
		St.GrabKind = Kind;
		St.GrabUntil = T + Seconds;
		St.GrabTickAt = T + VhT(TEXT("grabTick"), 0.25f);
		St.GrabDps = Dps;
		const float R = Boss->GetSimpleCollisionRadius(), HH = Boss->GetSimpleCollisionHalfHeight();
		const bool bDrain = Kind == TEXT("drain");
		// The drain lifts you in the hood's three jaws; the drag holds you low at her jaw as she goes under.
		const FVector Rel = bDrain ? FVector(R * 1.15f, 0.f, HH * 0.55f + VhT(TEXT("drainLift"), 250.f)) : FVector(R * 1.05f, 0.f, -HH * 0.35f);
		VhHold(Victim, Boss, Rel, true);
		if (Victim->Status) Victim->Status->Stagger(Seconds + 0.4f);
		if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss))
		{
			Body->Set(WRSwampBossFlag::JawOpen, true);
			if (!bDrain) Body->Set(WRSwampBossFlag::Swimming, true);
			Body->MulticastMoment(bDrain ? FName(TEXT("vharra.sacburst")) : FName(TEXT("mire.splash")), Victim->GetActorLocation(), bDrain ? 1.f : 2.f,
				bDrain ? FName(TEXT("creature.vharra.signature")) : FName(TEXT("water.splash")));
		}
		UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra %s %s (%.1f s at %.0f/s)"), bDrain ? TEXT("drains") : TEXT("drags under"), *Victim->GetName(), Seconds, Dps);
	}

	/** Server, while a grab holds: the squeeze ticks through ApplyHit; the drain heals her 3x and fills the sacs. */
	bool VhTickGrab(AWRBoss* Boss, FWRVharraFight& St)
	{
		AWRCharacterBase* V = St.Grabbed.Get();
		const double T = VhNow(Boss);
		if (!V || !V->IsAlive() || T >= St.GrabUntil || Boss->HasTag(WRTags::Status_Staggered) || Boss->HasTag(WRTags::Status_StanceBroken))
		{
			VhReleaseGrab(Boss, St);
			return false;
		}
		if (T >= St.GrabTickAt)
		{
			const float Every = VhT(TEXT("grabTick"), 0.25f);
			St.GrabTickAt = T + Every;
			const float Before = V->GetAttributes() ? V->GetAttributes()->GetHealth() : 0.f;
			VhHit(Boss, V, St.GrabDps * Every, 0.f, true, false, 1, true, 0.f, Boss->GetActorLocation());
			const float Taken = FMath::Max(0.f, Before - (V->GetAttributes() ? V->GetAttributes()->GetHealth() : Before));
			if (St.GrabKind == TEXT("drain"))
			{
				VhHeal(Boss, Taken * VhT(TEXT("drainHeal"), 3.f) * St.HealMult);
				VhFillSacs(St, Taken * VhT(TEXT("sacFillPerDamage"), 0.012f));
			}
		}
		return true;
	}

	/** Server: a ring wave rolling out along the water (the heave's ring, the Sinking's surge). */
	void VhStartWave(AWRBoss* Boss, FWRVharraFight& St, const FVector& From, float Radius, float Damage, float Height, bool bLow, bool bWaterOnly, bool bUnblockable, uint8 Impact, float Knock)
	{
		St.bWave = true;
		St.WaveAt = VhNow(Boss);
		St.WaveFrom = From;
		St.WaveRadius = Radius;
		St.WaveDamage = Damage;
		St.WaveHeight = Height;
		St.bWaveLow = bLow;
		St.bWaveWaterOnly = bWaterOnly;
		St.bWaveUnblockable = bUnblockable;
		St.WaveImpact = Impact;
		St.WaveKnock = Knock;
		St.WaveHit.Reset();
		// The front shows as spray running along the water at the wave's own speed (each recipe's life is its radius over
		// the wave speed: vharra.heavewave 2 s, vharra.surge 3.9 s).
		const float Surface = VhSurface(St);
		const bool bSurge = Height > 100.f;
		WRFx::Broadcast(Boss, bSurge ? FName(TEXT("vharra.surge")) : FName(TEXT("vharra.heavewave")), FVector(From.X, From.Y, Surface), FRotator::ZeroRotator, Radius / 100.f,
			bSurge ? FName(TEXT("swamp.vharra.flood")) : FName(TEXT("water.splash")));
	}

	void VhTickWave(AWRBoss* Boss, FWRVharraFight& St)
	{
		if (!St.bWave) return;
		const double T = VhNow(Boss);
		const float Speed = VhT(TEXT("waveSpeed"), 1200.f);
		const float Front = float(T - St.WaveAt) * Speed;
		if (Front > St.WaveRadius + 300.f)
		{
			St.bWave = false;
			return;
		}
		const float Depth = VhT(TEXT("waveDepth"), 220.f);
		const float Surface = VhSurface(St);
		const AWRSwampFlood* Flood = St.Flood.Get();
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Boss || !V->IsAlive() || !Boss->IsHostileTo(V) || V->GetAttachParentActor() || St.WaveHit.Contains(V)) continue;
			if (const AWRCreature* C = Cast<AWRCreature>(V); C && !C->IsTamed()) continue;
			const FVector Feet = VhFeet(V);
			const float D = FVector::Dist2D(Feet, St.WaveFrom) - V->GetSimpleCollisionRadius();
			if (D > Front || D < Front - Depth || D > St.WaveRadius) continue;
			// It runs along the water: the heave's ring only reaches feet in the water; the surge's 1.4 m wall reaches
			// anyone standing lower than its crest (the plinths and the mounds stand above it).
			if (St.bWaveWaterOnly && !(Flood && Flood->InWater(Feet))) continue;
			if (!St.bWaveWaterOnly && Feet.Z > Surface + St.WaveHeight) continue;
			St.WaveHit.Add(V);
			VhHit(Boss, V, St.WaveDamage, 0.f, St.bWaveUnblockable, St.bWaveLow, St.WaveImpact, false, St.WaveKnock, St.WaveFrom);
		}
	}

	/** Server: the Brood shed's young into the water beside her; each goes for the furthest tame (then the furthest
	 *  survivor). */
	void VhShedBrood(AWRBoss* Boss, FWRVharraFight& St)
	{
		UWorld* World = Boss->GetWorld();
		const int32 N = FMath::RoundToInt(VhT(TEXT("broodCount"), 4.f));
		TArray<AWRCharacterBase*> Foes = VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f));
		const FVector Here = Boss->GetActorLocation();
		Foes.Sort([Here](const AWRCharacterBase& A, const AWRCharacterBase& B)
		{
			const bool bTameA = A.IsCreature(), bTameB = B.IsCreature();
			if (bTameA != bTameB) return bTameA;   // tames first
			return FVector::DistSquared2D(A.GetActorLocation(), Here) > FVector::DistSquared2D(B.GetActorLocation(), Here);
		});
		const FVector Pool = St.Arena.bFound ? St.Arena.Centre : Here;
		const float Surface = VhSurface(St);
		int32 Made = 0;
		for (int32 i = 0; i < N; ++i)
		{
			// Out of her split sacs and into the water in front of her, then away to their prey.
			const FVector Toward = (Pool - Here).GetSafeNormal2D();
			const FVector Spot2D = Here + Toward * (Boss->GetSimpleCollisionRadius() + 300.f) + FRotator(0.f, 90.f * i - 135.f, 0.f).Vector() * 250.f;
			const FVector Spot(Spot2D.X, Spot2D.Y, Surface + 40.f);
			AWRCreature* Young = AWRCreature::SpawnCreature(World, VhBroodId(), Spot, FRotator(0.f, Toward.Rotation().Yaw, 0.f));
			if (!Young) continue;
			VhSetHealth(Young, VhT(TEXT("broodHealth"), 25.f));
			Young->SetHome(Spot);
			if (Foes.Num() > 0) Young->SetAggroTarget(Foes[Made % FMath::Min(Foes.Num(), 2)]);
			Young->SetLifeSpan(VhT(TEXT("broodLinger"), 120.f));
			St.Brood.Add(Young);
			++Made;
		}
		WRFx::Broadcast(Boss, TEXT("vharra.brood"), FVector(Here.X, Here.Y, Surface), FRotator::ZeroRotator, 2.f, TEXT("creature.gorgeleech.aggro"));
		UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra sheds %d brood (%s first)"), Made, Foes.Num() > 0 ? *Foes[0]->GetName() : TEXT("nobody"));
	}

	/** Brood upkeep: the dead go, the living keep after the furthest tame (a new one when theirs falls). */
	void VhTickBrood(AWRBoss* Boss, FWRVharraFight& St)
	{
		St.Brood.RemoveAll([](const TWeakObjectPtr<AWRCreature>& B) { return !B.IsValid() || !B->IsAlive(); });
		const double T = VhNow(Boss);
		if (St.Brood.Num() == 0 || T < St.NextBroodLook) return;
		St.NextBroodLook = T + 1.0;
		AWRCharacterBase* Far = nullptr;
		float FarD = -1.f;
		bool bFarIsTame = false;
		for (AWRCharacterBase* F : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			const bool bTame = F->IsCreature();
			const float D = FVector::Dist2D(F->GetActorLocation(), Boss->GetActorLocation());
			if ((bTame && !bFarIsTame) || (bTame == bFarIsTame && D > FarD)) { FarD = D; Far = F; bFarIsTame = bTame; }
		}
		for (const TWeakObjectPtr<AWRCreature>& B : St.Brood)
		{
			const AWRCharacterBase* Prey = Cast<AWRCharacterBase>(B->GetAggroTarget());
			if ((!Prey || !Prey->IsAlive()) && Far) B->SetAggroTarget(Far);
		}
	}

	/** Server: the water's 1.5 s shock reaches everyone standing or swimming in it (once a window): shock 60, unless
	 *  Grounded, up on a Mirestrider, on a plinth or a mound (dry), or on the clot crust. */
	void VhShockPeople(AWRSwampFlood* Flood, AActor* By)
	{
		if (!Flood) return;
		FWRVharraWater& W = VhWaters().FindOrAdd(Flood);
		const float Amount = VhT(TEXT("shockBuildup"), 60.f);
		for (AWRCharacterBase* V : VhPeople(Flood->GetWorld(), Flood->GetActorLocation(), Flood->FloodRadius + 200.f))
		{
			if (W.Shocked.Contains(V) || !VhInWater(Flood, V) || VhGrounded(V)) continue;
			W.Shocked.Add(V);
			VhBuildup(V, WRStatus::Shock, Amount, By);
			WRFx::Broadcast(Flood, TEXT("status.shock"), V->GetActorLocation(), FRotator::ZeroRotator, 1.f);
		}
	}

	/** Server: the Throat's water electrified for its seconds (arcs from the rod across the surface, everyone in it
	 *  shocked). */
	void VhShockWater(UWorld* World, AWRSwampFlood* Flood, AWRSwampRod* Rod, const FVector& At, AActor* By, AWRBoss* Queen)
	{
		if (!World || !Flood) return;
		const float Seconds = VhT(TEXT("shockSeconds"), 1.5f);
		Flood->Electrify(Seconds);
		FWRVharraWater& W = VhWaters().FindOrAdd(Flood);
		W.Shocked.Reset();
		W.WindowUntil = VhServerTime(World) + Seconds;
		W.By = By;
		const float Surface = Flood->SurfaceZ();
		const FVector Mid(Flood->GetActorLocation().X, Flood->GetActorLocation().Y, Surface);
		if (Rod)
		{
			// Blue-white arcs race from the rod's foot across the surface: to her, to whoever's in it, out over the water.
			TArray<FVector_NetQuantize> To;
			if (Queen && Queen->IsAlive()) To.Add(FVector_NetQuantize(FVector(Queen->GetActorLocation().X, Queen->GetActorLocation().Y, Surface)));
			for (AWRCharacterBase* V : VhPeople(World, Mid, Flood->FloodRadius))
			{
				if (To.Num() >= 8) break;
				if (VhInWater(Flood, V)) To.Add(FVector_NetQuantize(FVector(V->GetActorLocation().X, V->GetActorLocation().Y, Surface)));
			}
			for (int32 i = To.Num(); i < 8; ++i)
			{
				const FVector P = Mid + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(0.2f, 0.95f) * Flood->FloodRadius;
				To.Add(FVector_NetQuantize(FVector(P.X, P.Y, Surface)));
			}
			Rod->MulticastArcs(To);
			WRFx::Broadcast(World, TEXT("storm.rodflash"), Rod->Tip(), FRotator::ZeroRotator, 1.5f, TEXT("swamp.rod.shock"));
		}
		WRFx::Broadcast(World, TEXT("vharra.watershock"), Mid, FRotator::ZeroRotator, Flood->FloodRadius / 1000.f, TEXT("swamp.water_shock"), Seconds);
		VhShockPeople(Flood, By);
		UE_LOG(LogWildreach, Log, TEXT("BOSS Throat water shocked at %s (%s)"), *At.ToCompactString(), Rod ? *Rod->GetName() : TEXT("into the water"));
	}

	/** Does a rod take lightning that landed here (on its mast, at its tip, at its foot)? */
	bool VhRodCatches(const AWRSwampRod* Rod, const FVector& At, float Reach)
	{
		if (!Rod) return false;
		const FVector Foot = Rod->GetActorLocation();
		return FVector::Dist2D(Foot, At) <= Reach && At.Z >= Foot.Z - 250.f && At.Z <= Rod->Tip().Z + 250.f;
	}

	/** Someone she may fight standing up on a mound or a plinth (dry high ground): where the drowning coil goes. */
	bool VhHighGroundFoe(const AWRBoss* Boss, const FWRVharraFight& St, FVector& OutSpot)
	{
		const float Surface = VhSurface(St);
		const float Reach = VhT(TEXT("coilRadius"), 500.f);
		float Best = TNumericLimits<float>::Max();
		bool bFound = false;
		for (AWRCharacterBase* F : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			const FVector Feet = VhFeet(F);
			if (Feet.Z < Surface + VhT(TEXT("highGroundAbove"), 30.f)) continue;
			for (const FVector& Spot : VhHighGround(St.Arena))
			{
				if (FVector::Dist2D(Feet, Spot) > Reach) continue;
				const float D = FVector::Dist2D(Spot, Boss->GetActorLocation());
				if (D < Best) { Best = D; OutSpot = Spot; bFound = true; }
			}
		}
		return bFound;
	}

	/** A row by id on her (its index), or INDEX_NONE. */
	int32 VhRowIndex(const TArray<FWRBossAttack>& Attacks, FName Id)
	{
		return Attacks.IndexOfByPredicate([Id](const FWRBossAttack& A) { return A.Id == Id; });
	}
}

namespace WRSwampVharraLocal
{
	/** The Blood path's altar: at the top of the Throat Stair, a little to its side (swamp.json arenas.vharra.stair),
	 *  facing out over the sinkhole. Every machine works it out the same from the data. */
	bool VhAltarSpot(const TSharedPtr<FJsonObject>& Row, FVector& Out, float& OutYaw)
	{
		if (!Row.IsValid()) return false;
		const TArray<FVector> Stair = WRSwamp::Points(Row, TEXT("stair"));
		const FVector Centre = VhArenaCentre(Row);
		if (Stair.Num() == 0) return false;
		const FVector Top = Stair[0];
		const FVector Down = Stair.Num() > 1 ? (Stair[1] - Stair[0]).GetSafeNormal2D() : (Centre - Top).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Down).GetSafeNormal();
		Out = Top + Side * VhT(TEXT("altarAside"), 650.f) - Down * VhT(TEXT("altarBack"), 150.f);
		OutYaw = (Centre - Out).GetSafeNormal2D().Rotation().Yaw;
		return true;
	}
}

// ---------------------------------------------------------------- who she is, her life between fights

bool FWRSwampBoss::IsVharra(const AActor* Actor)
{
	const AWRCreature* C = Cast<AWRCreature>(Actor);
	return C && (C->CreatureId == VhQueenId() || C->CreatureId == VhBroodId());
}

void FWRSwampBoss::VharraBegin(AWRBoss* Boss)
{
	if (!Boss || Boss->CreatureId != VhQueenId() || !Boss->HasAuthority())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	VhEnsureArena(Boss, St);
	for (int32 i = 0; i < 3; ++i) St.SacHP[i] = VhT(TEXT("sacHealth"), 1200.f);
	if (UWRSwampBossBody* Body = VhBody(Boss))
	{
		Body->Flags = 0;
		Body->Set(WRSwampBossFlag::InWater, true);
		Body->Anchor = St.Arena.bFound ? St.Arena.AnchorAt : FVector::ZeroVector;
		Body->Burst = 0;
		for (int32 i = 0; i < 3; ++i) Body->SetSac(i, 0.f);
	}
	if (St.Arena.bFound)
	{
		Boss->SetActorRotation(FRotator(0.f, St.Arena.SeatYaw, 0.f));
	}
	Boss->SpeedBoost = 0.f;   // on her anchor: she never walks in phase 1
	UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra on her anchor at %s (disc %s, %d rods, flood %s)"), *Boss->GetActorLocation().ToCompactString(),
		*St.Arena.AnchorAt.ToCompactString(), St.Rods.Num(), St.Flood.IsValid() ? TEXT("found") : TEXT("not yet"));
}

void FWRSwampBoss::VharraReset(AWRBoss* Boss)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	VhReleaseGrab(Boss, St);
	for (const TWeakObjectPtr<AWRCreature>& B : St.Brood) if (B.IsValid()) B->Destroy();
	St.Brood.Reset();
	for (const TWeakObjectPtr<AWRSwampBossProp>& C : St.Crusts) if (C.IsValid()) C->Destroy();
	St.Crusts.Reset();
	for (int32 i = 0; i < 3; ++i)
	{
		St.SacHP[i] = VhT(TEXT("sacHealth"), 1200.f);
		St.Fill[i] = 0.f;
	}
	St.Burst = 0;
	St.HealMult = 1.f;
	St.ConvulseUntil = St.ConvulseGapUntil = St.NoHealUntil = St.NextBroodAt = 0.0;
	St.bConvulsing = false;
	St.bWave = false;
	St.Stage = 0;
	// The rods stand armed again and the flood drains (cook 2.10 edge cases: the flood resets on a wipe).
	VhEnsureArena(Boss, St);
	for (const TWeakObjectPtr<AWRSwampRod>& R : St.Rods) if (R.IsValid()) R->Rearm();
	if (AWRSwampFlood* Flood = St.Flood.Get())
	{
		Flood->Drain();
		Flood->SetKind(uint8(WRSwamp::IsTide(Boss) ? 1 : 0));
	}
	if (UWRSwampBossBody* Body = VhBody(Boss))
	{
		Body->Flags = 0;
		Body->Set(WRSwampBossFlag::InWater, true);
		Body->Anchor = St.Arena.bFound ? St.Arena.AnchorAt : FVector::ZeroVector;
		Body->Burst = 0;
		Body->Lash = 0;
		for (int32 i = 0; i < 3; ++i) Body->SetSac(i, 0.f);
	}
	// Back onto her seat at once (in phase 2 she swam off it; she never walks the Throat's floor).
	Boss->bReturning = false;
	Boss->TeleportTo(Boss->GetHome(), FRotator(0.f, St.Arena.bFound ? St.Arena.SeatYaw : Boss->GetActorRotation().Yaw, 0.f));
	Boss->SpeedBoost = 0.f;
	UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra reset: rods armed, the flood drains, back on her anchor"));
}

void FWRSwampBoss::VharraEndPlay(AWRBoss* Boss)
{
	if (!Boss)
	{
		return;
	}
	if (FWRVharraFight* St = VhStates().Find(Boss))
	{
		if (AWRCharacterBase* V = St->Grabbed.Get()) VhHold(V, Boss, FVector::ZeroVector, false);
		for (const TWeakObjectPtr<AWRCreature>& B : St->Brood) if (B.IsValid()) B->Destroy();
		for (const TWeakObjectPtr<AWRSwampBossProp>& C : St->Crusts) if (C.IsValid()) C->Destroy();
	}
	VhStates().Remove(Boss);
}

void FWRSwampBoss::VharraDied(AWRBoss* Boss)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	VhReleaseGrab(Boss, St);
	// Her brood scatter and die off in the water; the crust breaks up; the rods stand ready for the next one.
	for (const TWeakObjectPtr<AWRCreature>& B : St.Brood) if (B.IsValid()) B->SetLifeSpan(10.f);
	St.Brood.Reset();
	for (const TWeakObjectPtr<AWRSwampBossProp>& C : St.Crusts) if (C.IsValid()) C->Destroy();
	St.Crusts.Reset();
	for (const TWeakObjectPtr<AWRSwampRod>& R : St.Rods) if (R.IsValid()) R->Rearm();
	// The flood drains a while after she sinks (the blood clears with it).
	if (AWRSwampFlood* Flood = St.Flood.Get())
	{
		const TWeakObjectPtr<AWRSwampFlood> WeakFlood(Flood);
		const bool bTide = WRSwamp::IsTide(Boss);
		FTimerHandle Handle;
		Boss->GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Flood, [WeakFlood, bTide]()
		{
			if (AWRSwampFlood* F = WeakFlood.Get())
			{
				F->Drain();
				F->SetKind(uint8(bTide ? 1 : 0));
			}
		}), FMath::Max(0.1f, VhT(TEXT("floodAfterDeath"), 20.f)), false);
	}
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss))
	{
		Body->Set(WRSwampBossFlag::Convulsing, false);
		Body->Set(WRSwampBossFlag::JawOpen, false);
		Body->Set(WRSwampBossFlag::Rising, false);
		Body->Set(WRSwampBossFlag::Swimming, false);
		Body->Lash = 0;
	}
	UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra dies in the Throat"));
}

bool FWRSwampBoss::VharraTickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	bSkipWake = false;
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return false;
	}
	FWRVharraFight& St = VhS(Boss);
	VhEnsureArena(Boss, St);
	const double T = VhNow(Boss);
	// On her anchor between fights: no loop to walk. After a reset she's put back on her seat (never walked there).
	if (Boss->bReturning || FVector::Dist2D(Boss->GetActorLocation(), Boss->GetHome()) > 400.f)
	{
		Boss->bReturning = false;
		Boss->TeleportTo(Boss->GetHome(), FRotator(0.f, St.Arena.bFound ? St.Arena.SeatYaw : Boss->GetActorRotation().Yaw, 0.f));
	}
	Boss->SpeedBoost = 0.f;
	if (St.Arena.bFound)
	{
		const FRotator Want(0.f, St.Arena.SeatYaw, 0.f);
		Boss->SetActorRotation(FMath::RInterpConstantTo(Boss->GetActorRotation(), Want, DeltaSeconds, 30.f));
	}
	for (int32 i = 0; i < 3; ++i) St.Fill[i] = FMath::Max(0.f, St.Fill[i] - VhT(TEXT("sacEmptyPerSecond"), 0.03f) * DeltaSeconds);
	if (UWRSwampBossBody* Body = VhBody(Boss))
	{
		Body->Set(WRSwampBossFlag::InWater, VhQueenInWater(Boss, St));
		Body->Set(WRSwampBossFlag::Free, false);
		Body->Set(WRSwampBossFlag::Swimming, false);
		if (St.Arena.bFound) Body->Anchor = St.Arena.AnchorAt;
		for (int32 i = 0; i < 3; ++i) Body->SetSac(i, (St.Burst & (1 << i)) ? 0.f : St.Fill[i]);
	}
	// The Red Tide turns her water too (R24): its look follows the tide while nobody fights her.
	if (AWRSwampFlood* Flood = St.Flood.Get(); Flood && T >= St.NextKindCheck)
	{
		St.NextKindCheck = T + 2.0;
		Flood->SetKind(uint8(WRSwamp::IsTide(Boss) ? 1 : 0));
	}
	return true;
}

bool FWRSwampBoss::TickVharra(AWRBoss* Boss, FWRSwampBossState& St, UWRSwampBossBody* Body, float DeltaSeconds)
{
	// Vharra keeps her own fight state (WRSwampVharra.cpp); this is the shared machine's entry, kept for its callers.
	(void)St;
	(void)Body;
	return VharraTick(Boss, DeltaSeconds);
}

bool FWRSwampBoss::VharraTick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return false;
	}
	FWRVharraFight& St = VhS(Boss);
	VhEnsureArena(Boss, St);
	UWRSwampBossBody* Body = VhBody(Boss);
	const double T = VhNow(Boss);
	AWRSwampFlood* Flood = St.Flood.Get();
	const bool bInWater = VhQueenInWater(Boss, St);

	// What every machine shows of her: in the water, free of the wall, the sacs' fill and bursts.
	for (int32 i = 0; i < 3; ++i) St.Fill[i] = FMath::Max(0.f, St.Fill[i] - VhT(TEXT("sacEmptyPerSecond"), 0.03f) * DeltaSeconds);
	if (Body)
	{
		Body->Set(WRSwampBossFlag::InWater, bInWater);
		Body->Set(WRSwampBossFlag::Free, Boss->Phase >= 2);
		for (int32 i = 0; i < 3; ++i) Body->SetSac(i, (St.Burst & (1 << i)) ? 0.f : St.Fill[i]);
		Body->Burst = St.Burst;
		if (Boss->Phase < 2 && St.Arena.bFound) Body->Anchor = St.Arena.AnchorAt;
	}
	// Her water's kind: the Red Tide's bleedwater, the hidden phase's blood.
	if (Flood && T >= St.NextKindCheck)
	{
		St.NextKindCheck = T + 1.0;
		Flood->SetKind(uint8(Boss->Phase >= 3 ? 2 : WRSwamp::IsTide(Boss) ? 1 : 0));
	}
	VhTickWave(Boss, St);
	VhTickBrood(Boss, St);

	// The Water Turns (hidden phase 3): the flood is blood (bleed 15 a second in it); she heals 1% a second while she's
	// in it, unless a rod's shock burnt the water in the last 10 s.
	if (Boss->Phase >= 3 && Flood && T >= St.NextBloodTick)
	{
		const float Every = 0.5f;
		St.NextBloodTick = T + Every;
		for (AWRCharacterBase* V : VhPeople(Boss->GetWorld(), Flood->GetActorLocation(), Flood->FloodRadius + 200.f))
		{
			const AWRCreature* C = Cast<AWRCreature>(V);
			if ((C && !C->IsTamed()) || !VhInWater(Flood, V)) continue;   // her brood and the swamp's own are blood-born
			VhBuildup(V, WRStatus::Bleed, VhT(TEXT("p3Bleed"), 15.f) * Every, Boss);
		}
		if (bInWater && T >= St.NoHealUntil)
		{
			VhHeal(Boss, VhMaxHealth(Boss) * VhT(TEXT("p3HealPerSecond"), 0.01f) * Every);
		}
	}

	// A grab holds her (she's busy drinking you or dragging you under); then the punish.
	if (St.Grabbed.IsValid())
	{
		const FName Kind = St.GrabKind;
		if (VhTickGrab(Boss, St))
		{
			return true;
		}
		Boss->BusyUntil = T + (Kind == TEXT("drag") ? VhT(TEXT("dragPunish"), 1.0f) : VhT(TEXT("drainPunish"), 1.2f));
	}

	// Convulsing in the shocked water: an eel's thrash on the surface for 4 s, open (stance x2; lightning x1.5).
	if (T < St.ConvulseUntil)
	{
		St.bConvulsing = true;
		if (Body)
		{
			Body->Set(WRSwampBossFlag::Convulsing, true);
			Body->Set(WRSwampBossFlag::Swimming, false);
		}
		Boss->SpeedBoost = 0.f;
		if (T >= St.NextThrashAt)
		{
			St.NextThrashAt = T + VhT(TEXT("thrashEvery"), 0.45f);
			const FVector At(Boss->GetActorLocation().X, Boss->GetActorLocation().Y, VhSurface(St));
			WRFx::Broadcast(Boss, TEXT("vharra.convulse"), At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Boss->GetSimpleCollisionRadius() / 100.f, TEXT("water.splash"));
		}
		return true;
	}
	if (St.bConvulsing)
	{
		St.bConvulsing = false;
		if (Body) Body->Set(WRSwampBossFlag::Convulsing, false);
		Boss->BusyUntil = FMath::Max(Boss->BusyUntil, T + 0.4);
	}

	if (Boss->Phase < 2)
	{
		// Phase 1: on her anchor. She never walks; after a lunge she settles back onto her seat.
		Boss->SpeedBoost = 0.f;
		const FVector Here = Boss->GetActorLocation();
		const FVector Seat = Boss->GetHome();
		if (Boss->Current == INDEX_NONE && FVector::Dist2D(Here, Seat) > 30.f)
		{
			const FVector To = FMath::VInterpConstantTo(FVector(Here.X, Here.Y, 0.f), FVector(Seat.X, Seat.Y, 0.f), DeltaSeconds, VhT(TEXT("settleSpeed"), 500.f));
			Boss->SetActorLocation(FVector(To.X, To.Y, Here.Z), false);
		}
		// She faces the open Throat: the fight is a half circle toward her (cook 2.14).
		if (St.Arena.bFound)
		{
			const float Clamp = VhT(TEXT("faceClamp"), 110.f);
			const float Off = FMath::FindDeltaAngleDegrees(St.Arena.SeatYaw, Boss->GetActorRotation().Yaw);
			if (FMath::Abs(Off) > Clamp)
			{
				const FRotator Held(0.f, St.Arena.SeatYaw + FMath::Sign(Off) * Clamp, 0.f);
				Boss->SetActorRotation(Held);
				if (Boss->Current != INDEX_NONE) Boss->AttackFacing = Held;
			}
		}
	}
	else
	{
		// Phase 2 on: free in the flood, never up onto the dry upper ring.
		Boss->SpeedBoost = 1.f;
		if (Flood)
		{
			const FVector C = Flood->GetActorLocation();
			const FVector Here = Boss->GetActorLocation();
			const float Edge = FMath::Max(500.f, Flood->FloodRadius - Boss->GetSimpleCollisionRadius() * 0.8f);
			if (FVector::Dist2D(Here, C) > Edge)
			{
				const FVector In = FVector(C.X, C.Y, 0.f) + (FVector(Here.X, Here.Y, 0.f) - FVector(C.X, C.Y, 0.f)).GetSafeNormal() * Edge;
				Boss->SetActorLocation(FVector(In.X, In.Y, Here.Z), false);
			}
			// Under the flood between moves where it's deep enough (the pool and the drowned terraces).
			if (Body && Boss->Current == INDEX_NONE)
			{
				const float Depth = Flood->SurfaceZ() - VhFeet(Boss).Z;
				Body->Set(WRSwampBossFlag::Swimming, Depth > VhT(TEXT("swimDepth"), 100.f));
			}
		}
	}
	return false;
}

AWRCharacterBase* FWRSwampBoss::VharraPickTarget(AWRBoss* Boss, AWRCharacterBase* Nearest)
{
	(void)Boss;
	return Nearest;
}

// ---------------------------------------------------------------- the attack machine

bool FWRSwampBoss::VharraAllowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	const FWRVharraFight* St = VhFind(Boss);
	if (!Boss || !St)
	{
		return true;
	}
	const double T = VhNow(Boss);
	if (St->Grabbed.IsValid() || T < St->ConvulseUntil)
	{
		return false;
	}
	const FName Id = Attack.Id;
	const float Dist = Target ? FVector::Dist2D(Target->GetActorLocation(), Boss->GetActorLocation()) : 0.f;
	if (Id == TEXT("brood_shed"))
	{
		// Once per 45 s from 75%, and not while her last brood still swims.
		int32 Alive = 0;
		for (const TWeakObjectPtr<AWRCreature>& B : St->Brood) Alive += B.IsValid() && B->IsAlive();
		return VhHealthFrac(Boss) <= VhT(TEXT("broodFrom"), 0.75f) && T >= St->NextBroodAt && Alive == 0;
	}
	if (Id == TEXT("clot_spit"))
	{
		// At those over 12 m away.
		for (AWRCharacterBase* F : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			const float D = FVector::Dist2D(F->GetActorLocation(), Boss->GetActorLocation());
			if (D > VhT(TEXT("clotMinRange"), 1200.f) && D <= Attack.Range) return true;
		}
		return false;
	}
	if (Id == TEXT("ambush_rise"))
	{
		// Only under someone in the water.
		return Target && St->Flood.IsValid() && VhInWater(St->Flood.Get(), Target);
	}
	if (Id == TEXT("drowning_coil"))
	{
		FVector Spot;
		return VhHighGroundFoe(Boss, *St, Spot);
	}
	if (Id == TEXT("three_jaw_drain") || Id == TEXT("drag"))
	{
		return Target && !Target->GetAttachParentActor() && Dist <= Attack.Range + Boss->GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius();
	}
	if (Id == TEXT("tail_sweep") || Id == TEXT("anchor_heave"))
	{
		// From the water (her trunk lies in it; the heave's ring runs along it).
		return VhQueenInWater(Boss, *St);
	}
	return true;
}

void FWRSwampBoss::VharraAttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	UWRSwampBossBody* Body = VhBody(Boss);
	const double T = VhNow(Boss);
	const FName Id = Attack.Id;
	St.Stage = 0;
	St.StageAt = T;
	St.NextTickAt = T;
	St.NextFxAt = T;
	St.MoveFrom = Boss->GetActorLocation();
	St.Aim = Target ? VhFeet(Target) : Boss->AttackPoint;
	St.Puddles = 0;
	const float Surface = VhSurface(St);
	if (Id == TEXT("three_jaw_drain") || Id == TEXT("drag"))
	{
		// The head splits open into three with a wet crack (the crimson glint in the throat is the row's tell).
		if (Body) Body->Set(WRSwampBossFlag::JawOpen, true);
		WRSound::Broadcast(Boss, TEXT("creature.vharra.signature"));
	}
	else if (Id == TEXT("tail_sweep") || Id == TEXT("tail_backhand"))
	{
		// The ringed trunk lifts from the pool, water streaming off it: the sweep (low: jump) from her left, the backhand
		// (roll) back from her right.
		const float Arc = VhT(TEXT("tailSweepArc"), 100.f);
		St.SweepFrom = Id == TEXT("tail_sweep") ? -Arc : Arc;
		St.SweepTo = -St.SweepFrom;
		if (Body) Body->Lash = int8(FMath::Clamp(FMath::RoundToInt(St.SweepFrom), -127, 127));
		const FVector Side = Boss->GetActorLocation() + FRotator(0.f, Boss->GetActorRotation().Yaw + St.SweepFrom, 0.f).Vector() * Boss->GetSimpleCollisionRadius() * 1.5f;
		WRFx::Broadcast(Boss, TEXT("vharra.drip"), FVector(Side.X, Side.Y, Surface + 200.f), FRotator::ZeroRotator, 2.f, TEXT("water.splash"), Attack.Windup);
	}
	else if (Id == TEXT("tail_overhead"))
	{
		// Up over her, straight: it comes down along a line (sidestep it).
		if (Body) Body->Lash = 1;
		WRFx::Broadcast(Boss, TEXT("vharra.drip"), FVector(Boss->GetActorLocation().X, Boss->GetActorLocation().Y, Surface + 600.f), FRotator::ZeroRotator, 2.5f, TEXT("water.splash"), Attack.Windup + 0.4f);
	}
	else if (Id == TEXT("blood_spray") || Id == TEXT("blood_spray_held"))
	{
		// The sacs pulse in sequence with a rising gurgle.
		if (Body) Body->MulticastMoment(TEXT("vharra.sacpulse"), Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight() * 0.2f), 2.f, TEXT("creature.vharra.tell"));
	}
	else if (Id == TEXT("clot_spit"))
	{
		// Three clots, each at a survivor over 12 m off (never homing: where they stood now); a shadow grows at each
		// landing spot for the whole flight.
		TArray<AWRCharacterBase*> Far;
		for (AWRCharacterBase* F : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			const float D = FVector::Dist2D(F->GetActorLocation(), Boss->GetActorLocation());
			if (D > VhT(TEXT("clotMinRange"), 1200.f) && D <= Attack.Range) Far.Add(F);
		}
		for (int32 i = Far.Num() - 1; i > 0; --i) Far.Swap(i, FMath::RandRange(0, i));
		St.ClotSpots.Reset();
		const int32 N = FMath::RoundToInt(VhT(TEXT("clotCount"), 3.f));
		const FVector Mouth = VhMouth(Boss);
		for (int32 i = 0; i < N && Far.Num() > 0; ++i)
		{
			FVector Spot = VhFeet(Far[i % Far.Num()]);
			if (i >= Far.Num()) Spot += FVector(FMath::RandPointInCircle(VhT(TEXT("clotSpread"), 300.f)), 0.f);
			bool bOk = false;
			const FVector G = VhLand(Boss->GetWorld(), Spot, bOk, 600.f, 1500.f);
			Spot = bOk ? FVector(G.X, G.Y, FMath::Max(G.Z, Surface)) : Spot;
			St.ClotSpots.Add(Spot);
			const float Flight = FVector::Dist2D(Mouth, Spot) / FMath::Max(100.f, VhT(TEXT("clotSpeed"), 1000.f));
			const float Size = VhT(TEXT("clotRadius"), 200.f);
			AWRGroundMarker::Spawn(Boss->GetWorld(), Spot + FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector2D(Size, Size), false,
				VhHex(TEXT("#1A0306")), Attack.Windup + Flight + 0.2f);
		}
		if (Body) Body->MulticastMoment(TEXT("vharra.sacpulse"), VhMouth(Boss), 1.2f, TEXT("creature.vharra.tell"));
	}
	else if (Id == TEXT("anchor_heave"))
	{
		// The disc pulls taut (a chain-like creak at the wall), then the body coils.
		const FVector Disc = VhDiscPoint(Boss, St);
		WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(Disc.X, Disc.Y, Surface), FRotator::ZeroRotator, 1.5f, TEXT("swamp.vharra.creak"));
		WRSound::Broadcast(Boss, TEXT("swamp.vharra.creak"));
	}
	else if (Id == TEXT("brood_shed"))
	{
		// The sacs contract and split at their seams.
		if (Body) Body->MulticastMoment(TEXT("vharra.sacpulse"), Boss->GetActorLocation(), 2.5f, TEXT("creature.vharra.tell"));
	}
	else if (Id == TEXT("the_sinking"))
	{
		// The Sinking's tell: she rears to her full height, all three sacs flare, a crimson glint runs along her teeth
		// (the row's crimson glint), the leopard-seal trill echoes round the sinkhole.
		if (Body) Body->Set(WRSwampBossFlag::Rising, true);
		for (int32 i = 0; i < 3; ++i) if (!(St.Burst & (1 << i))) St.Fill[i] = 1.f;
		WRFx::Broadcast(Boss, TEXT("vharra.drip"), Boss->GetActorLocation() + FVector(0.f, 0.f, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 3.f, TEXT("swamp.vharra.rise"), Attack.Windup);
	}
	else if (Id == TEXT("ambush_rise"))
	{
		// Under, and a wake circles toward you.
		if (Body) Body->Set(WRSwampBossFlag::Swimming, true);
	}
	else if (Id == TEXT("drowning_coil"))
	{
		// Her trunk circles the mound or plinth under the water (the wake shows), then rises round it.
		FVector Spot;
		St.CoilAt = VhHighGroundFoe(Boss, St, Spot) ? Spot : St.Aim;
		if (Body)
		{
			Body->Set(WRSwampBossFlag::Swimming, true);
			Body->Anchor = St.CoilAt;
		}
	}
}

bool FWRSwampBoss::VharraTickWindup(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return false;
	}
	FWRVharraFight& St = VhS(Boss);
	const double T = VhNow(Boss);
	const FName Id = Attack.Id;
	const float Surface = VhSurface(St);
	const FVector Here = Boss->GetActorLocation();
	if (Id == TEXT("ambush_rise"))
	{
		// She runs under the water to beneath the target's spot; then a ring of bubbles forms there.
		const FVector Goal(St.Aim.X, St.Aim.Y, Here.Z);
		Boss->SetActorLocation(FMath::VInterpConstantTo(Here, Goal, DeltaSeconds, VhT(TEXT("ambushSwimSpeed"), 1800.f)), false);
		if (T >= St.NextFxAt && St.Stage == 0)
		{
			St.NextFxAt = T + 0.3;
			WRFx::Broadcast(Boss, TEXT("mire.wake"), FVector(Here.X, Here.Y, Surface), Boss->GetActorRotation(), 4.f);
		}
		if (St.Stage == 0 && (FVector::Dist2D(Here, Goal) < 200.f || T - Boss->StrikeStarted >= Attack.Windup * 0.6f))
		{
			St.Stage = 1;
			const float Left = FMath::Max(0.2f, float(Boss->ResolveAt - T));
			WRFx::Broadcast(Boss, TEXT("mire.bubbles"), FVector(St.Aim.X, St.Aim.Y, Surface), FRotator::ZeroRotator, VhT(TEXT("ambushRadius"), 250.f) / 60.f, TEXT("swamp.vharra.rise"), Left);
		}
	}
	else if (Id == TEXT("drowning_coil"))
	{
		// Round to the mound's side under the water, the wake showing her trunk circling it.
		const FVector Out = (FVector(Here.X, Here.Y, 0.f) - FVector(St.CoilAt.X, St.CoilAt.Y, 0.f)).GetSafeNormal();
		const FVector Goal = FVector(St.CoilAt.X, St.CoilAt.Y, Here.Z) + Out * (VhT(TEXT("coilRadius"), 500.f) + Boss->GetSimpleCollisionRadius());
		Boss->SetActorLocation(FMath::VInterpConstantTo(Here, Goal, DeltaSeconds, VhT(TEXT("coilSwimSpeed"), 1500.f)), false);
		Boss->SetActorRotation(FRotator(0.f, (St.CoilAt - Here).GetSafeNormal2D().Rotation().Yaw, 0.f));
		if (T >= St.NextFxAt)
		{
			St.NextFxAt = T + 0.3;
			const float A = float(T - Boss->StrikeStarted) * 4.f;
			const FVector W = St.CoilAt + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * VhT(TEXT("coilRadius"), 500.f);
			WRFx::Broadcast(Boss, TEXT("mire.wake"), FVector(W.X, W.Y, Surface), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f), 4.f);
		}
	}
	return false;
}

bool FWRSwampBoss::VharraBeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return false;
	}
	FWRVharraFight& St = VhS(Boss);
	UWRSwampBossBody* Body = VhBody(Boss);
	UWorld* World = Boss->GetWorld();
	const double T = VhNow(Boss);
	const FName Id = Attack.Id;
	const float Active = Attack.bRow ? FMath::Max(0.1f, Attack.Row.Active) : 0.25f;
	const float Surface = VhSurface(St);
	const FVector Here = Boss->GetActorLocation();
	const FVector Fwd = Boss->AttackFacing.Vector().GetSafeNormal2D();
	St.Stage = 0;
	St.StageAt = T;
	St.NextTickAt = T;
	if (Id == TEXT("three_jaw_drain") || Id == TEXT("drag"))
	{
		St.MoveFrom = Here;
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("tail_sweep") || Id == TEXT("tail_backhand"))
	{
		St.LastAngle = St.SweepFrom;
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("tail_overhead"))
	{
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("blood_spray") || Id == TEXT("blood_spray_held"))
	{
		WRSound::Broadcast(Boss, TEXT("creature.vharra.attack"));
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("clot_spit"))
	{
		// Lobbed on a high arc at 1,000 cm/s to where each target stood: every machine flies it the same way (never
		// homing); it bursts where it really comes down and leaves a 2 m slowing clot puddle.
		const FVector Mouth = VhMouth(Boss);
		const float Speed = FMath::Max(100.f, VhT(TEXT("clotSpeed"), 1000.f));
		const float G = 980.f * VhT(TEXT("clotGravity"), 1.f);
		const TWeakObjectPtr<AWRBoss> WeakBoss(Boss);
		for (const FVector& Spot : St.ClotSpots)
		{
			const FVector Flat = (Spot - Mouth) * FVector(1.f, 1.f, 0.f);
			const float Flight = FMath::Max(0.25f, float(Flat.Size()) / Speed);
			const FVector Velocity = Flat / Flight + FVector(0.f, 0.f, (Spot.Z - Mouth.Z) / Flight + 0.5f * G * Flight);
			const FTransform Where(Velocity.Rotation(), Mouth);
			AWRSwampShot* Shot = World->SpawnActorDeferred<AWRSwampShot>(AWRSwampShot::StaticClass(), Where, Boss, Boss, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Shot) continue;
			Shot->Kind = TEXT("clot");
			Shot->Start = Mouth;
			Shot->Velocity = Velocity;
			Shot->Gravity = G;
			Shot->LaunchAt = VhServerTime(Boss);
			FWRHit Hit;
			Hit.Instigator = Boss;
			Hit.Damage = Attack.Damage * VhDamageScale() * (Boss->Phase >= 3 ? WRTune(TEXT("bosses.vharra.phase3DamageScale"), 1.4f) : 1.f);
			Hit.PoiseDamage = Attack.Poise;
			Hit.Impact = Attack.Impact;
			Hit.bProjectile = true;
			Hit.bHeavy = true;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.Element = WRTags::Element_Physical;
			Hit.PhysType = 2;
			Hit.Origin = Mouth;
			for (const TPair<FName, float>& B : Attack.Row.Buildups) Hit.Buildups.Add(B.Key, B.Value);
			Shot->Hit = Hit;
			Shot->MaxRange = Attack.Range + 800.f;
			Shot->Radius = VhT(TEXT("clotRadius"), 200.f);
			Shot->OnLanded = [WeakBoss](const FVector& At)
			{
				AWRBoss* B = WeakBoss.Get();
				if (!B) return;
				VhPuddle(B->GetWorld(), At, VhT(TEXT("clotRadius"), 200.f), VhT(TEXT("clotPuddleSeconds"), 8.f), 0.f, true, 1, B);
				WRFx::Broadcast(B, TEXT("vharra.clotsplat"), At, FRotator::ZeroRotator, 1.f, TEXT("water.splash"));
			};
			Shot->FinishSpawning(Where);
		}
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("anchor_heave"))
	{
		// The whole forebody slams across a terrace along 12 m (the line it lands on takes it), then a ring wave runs
		// out along the water at 1,200 cm/s (low: jump it).
		St.MoveFrom = Here;
		const float Length = VhT(TEXT("heaveLength"), 1200.f);
		const float Half = VhT(TEXT("heaveWidth"), 300.f) * 0.5f;
		const FVector End = Here + Fwd * Length;
		for (AWRCharacterBase* V : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			if (VhSegDist2D(V->GetActorLocation(), Here, End) <= Half + V->GetSimpleCollisionRadius()) Boss->StrikeHit(V);
		}
		for (int32 i = 1; i <= 4; ++i)
		{
			const FVector P = FMath::Lerp(Here, End, i / 4.f);
			WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(P.X, P.Y, Surface), FRotator::ZeroRotator, 2.5f, i == 4 ? FName(TEXT("stomp")) : NAME_None);
		}
		const FVector WaveAt = Here + Fwd * Length * 0.75f;
		VhStartWave(Boss, St, FVector(WaveAt.X, WaveAt.Y, Surface), VhT(TEXT("heaveWaveRadius"), 2400.f), VhT(TEXT("heaveWaveDamage"), 30.f), 50.f, true, true, false, 3, VhT(TEXT("heaveWaveKnock"), 300.f));
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("brood_shed"))
	{
		VhShedBrood(Boss, St);
		St.NextBroodAt = T + VhT(TEXT("broodEvery"), 45.f);
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("the_sinking"))
	{
		// She dives and the sinkhole floods: a surge 1.4 m high rolls out at 1,200 cm/s (60 and a knockdown; jump it or
		// stand on a plinth or a mound), the water rises 1.2 m over 6 s, she lets go of the wall.
		if (Body)
		{
			Body->Set(WRSwampBossFlag::Rising, false);
			Body->Set(WRSwampBossFlag::Swimming, true);
			Body->Set(WRSwampBossFlag::Free, true);
			Body->Anchor = FVector::ZeroVector;
		}
		const float Radius = (St.Flood.IsValid() ? St.Flood->FloodRadius : St.Arena.FloodR) + 300.f;
		VhStartWave(Boss, St, FVector(Here.X, Here.Y, Surface), Radius, VhT(TEXT("sinkWaveDamage"), 60.f), VhT(TEXT("sinkWaveHeight"), 140.f), true, false, true, 5, VhT(TEXT("sinkWaveKnock"), 450.f));
		if (AWRSwampFlood* Flood = St.Flood.Get()) Flood->StartFlood();
		WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(Here.X, Here.Y, Surface), FRotator::ZeroRotator, 6.f, TEXT("swamp.vharra.rise"));
		Boss->StrikeEnds = T + FMath::Max(Active, Radius / FMath::Max(100.f, VhT(TEXT("waveSpeed"), 1200.f)));
		UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra: the Sinking (the Throat floods)"));
		return true;
	}
	if (Id == TEXT("ambush_rise"))
	{
		// Up out of the water under the target's spot: everyone in the 2.5 m circle takes it and is thrown 4 m up.
		if (Body)
		{
			Body->Set(WRSwampBossFlag::Swimming, false);
			Body->Set(WRSwampBossFlag::Rising, true);
		}
		Boss->SetActorLocation(FVector(St.Aim.X, St.Aim.Y, Here.Z), false);
		const float Radius = VhT(TEXT("ambushRadius"), 250.f);
		for (AWRCharacterBase* V : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			if (FVector::Dist2D(V->GetActorLocation(), St.Aim) <= Radius + V->GetSimpleCollisionRadius()) Boss->StrikeHit(V);
		}
		WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(St.Aim.X, St.Aim.Y, Surface), FRotator::ZeroRotator, 4.f, TEXT("creature.vharra.attack"));
		Boss->StrikeEnds = T + Active;
		return true;
	}
	if (Id == TEXT("drowning_coil"))
	{
		// The squeeze: everyone on that mound or plinth (5 m ring) takes it and is thrown off into the water.
		const float Radius = VhT(TEXT("coilRadius"), 500.f);
		for (AWRCharacterBase* V : VhFoes(Boss, WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f)))
		{
			if (FVector::Dist2D(V->GetActorLocation(), St.CoilAt) <= Radius + V->GetSimpleCollisionRadius()) Boss->StrikeHit(V);
		}
		WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(St.CoilAt.X, St.CoilAt.Y, Surface), FRotator::ZeroRotator, 4.f, TEXT("stomp"));
		WRFx::Broadcast(Boss, TEXT("part.break"), St.CoilAt + FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, 2.f);
		Boss->StrikeEnds = T + Active;
		return true;
	}
	return false;
}

bool FWRSwampBoss::VharraTickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return false;
	}
	FWRVharraFight& St = VhS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	const double T = VhNow(Boss);
	const FName Id = Attack.Id;
	const float Span = FMath::Max(0.05f, float(Boss->StrikeEnds - Boss->StrikeStarted));
	const float A = FMath::Clamp(float(T - Boss->StrikeStarted) / Span, 0.f, 1.f);
	const float R = Boss->GetSimpleCollisionRadius();
	const float Surface = VhSurface(St);
	const FVector Fwd = Boss->AttackFacing.Vector().GetSafeNormal2D();
	const float Chase = WRTune(TEXT("bosses.vharra.chaseRadius"), 9000.f);
	(void)DeltaSeconds;
	if (Id == TEXT("three_jaw_drain") || Id == TEXT("drag"))
	{
		// A 12 m line lunge: her neck and forebody shoot along the line (the body itself moves up to its reach from where
		// it lay) and the open jaws take the first body they meet.
		const bool bDrain = Id == TEXT("three_jaw_drain");
		const float BodyReach = bDrain ? VhT(TEXT("lungeReach"), 600.f) : VhT(TEXT("dragLunge"), 250.f);
		const FVector BodyAt = St.MoveFrom + Fwd * BodyReach * FMath::Sin(A * PI * 0.5f);
		Boss->SetActorLocation(FVector(BodyAt.X, BodyAt.Y, Boss->GetActorLocation().Z), false);
		const float Reach = FMath::Lerp(R, Attack.Range, A);
		for (AWRCharacterBase* V : VhFoes(Boss, Chase))
		{
			const FVector To = (V->GetActorLocation() - St.MoveFrom) * FVector(1.f, 1.f, 0.f);
			const float Along = FVector::DotProduct(To, Fwd);
			const float Side = FMath::Abs(FVector::CrossProduct(Fwd, To).Z);
			if (Along > 0.f && Along <= Reach + V->GetSimpleCollisionRadius() && Side <= Attack.Size + V->GetSimpleCollisionRadius())
			{
				Boss->StrikeHit(V);
				break;   // the jaws take one
			}
		}
		if (St.Grabbed.IsValid()) Boss->StrikeEnds = T;
		return true;
	}
	if (Id == TEXT("tail_sweep") || Id == TEXT("tail_backhand"))
	{
		// The trunk sweeps across from one side to the other, 14 m out: whoever the sweep passes takes it.
		const float Angle = FMath::Lerp(St.SweepFrom, St.SweepTo, A);
		const float Lo = FMath::Min(St.LastAngle, Angle) - 6.f, Hi = FMath::Max(St.LastAngle, Angle) + 6.f;
		const float Reach = VhT(TEXT("tailReach"), 1400.f), Inner = VhT(TEXT("tailInner"), 200.f);
		const FVector Pivot = Boss->GetActorLocation();
		for (AWRCharacterBase* V : VhFoes(Boss, Chase))
		{
			const float D = FVector::Dist2D(V->GetActorLocation(), Pivot) - V->GetSimpleCollisionRadius();
			const float B = VhBearing(Pivot, Boss->AttackFacing.Yaw, V->GetActorLocation());
			if (D <= Reach && D >= Inner - V->GetSimpleCollisionRadius() && B >= Lo && B <= Hi) Boss->StrikeHit(V);
		}
		St.LastAngle = Angle;
		if (Body) Body->Lash = int8(FMath::Clamp(FMath::RoundToInt(Angle), -127, 127));
		if (T >= St.NextFxAt)
		{
			St.NextFxAt = T + 0.12;
			const FVector Tip = Pivot + FRotator(0.f, Boss->AttackFacing.Yaw + Angle, 0.f).Vector() * Reach * 0.8f;
			WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(Tip.X, Tip.Y, Surface), FRotator::ZeroRotator, 2.f, NAME_None);
		}
		return true;
	}
	if (Id == TEXT("tail_overhead"))
	{
		// Over and down along a line toward the target at the contact frame (sidestep it).
		if (St.Stage == 0 && A >= 0.55f)
		{
			St.Stage = 1;
			const FVector From = Boss->GetActorLocation();
			const FVector End = From + Fwd * VhT(TEXT("tailReach"), 1400.f);
			const float Half = VhT(TEXT("overheadWidth"), 300.f) * 0.5f;
			for (AWRCharacterBase* V : VhFoes(Boss, Chase))
			{
				if (VhSegDist2D(V->GetActorLocation(), From, End) <= Half + V->GetSimpleCollisionRadius() && FVector::Dist2D(V->GetActorLocation(), From) > R * 0.5f) Boss->StrikeHit(V);
			}
			for (int32 i = 1; i <= 3; ++i)
			{
				const FVector P = FMath::Lerp(From, End, 0.4f + 0.2f * i);
				WRFx::Broadcast(Boss, TEXT("mire.splash"), FVector(P.X, P.Y, Surface), FRotator::ZeroRotator, 3.f, i == 3 ? FName(TEXT("stomp")) : NAME_None);
			}
			if (Body) Body->Lash = 0;
		}
		return true;
	}
	if (Id == TEXT("blood_spray") || Id == TEXT("blood_spray_held"))
	{
		// A 12 m x 50 degree cone from her mouth sweeping 60 degrees (120 held): 15 a tick and bleed; bleedwater puddles
		// left on the terraces it crosses.
		const bool bHeld = Id == TEXT("blood_spray_held");
		const float Sweep = bHeld ? VhT(TEXT("sprayHeldSweep"), 120.f) : VhT(TEXT("spraySweep"), 60.f);
		const float Centre = FMath::Lerp(-Sweep * 0.5f, Sweep * 0.5f, A);
		const float Yaw = Boss->AttackFacing.Yaw + Centre;
		const float Range = VhT(TEXT("sprayRange"), 1200.f);
		const float Half = VhT(TEXT("sprayHalfAngle"), 25.f);
		const FVector Mouth = VhMouth(Boss);
		if (T >= St.NextTickAt)
		{
			const float Every = VhT(TEXT("sprayTickEvery"), 0.3f);
			St.NextTickAt = T + Every;
			WRFx::Broadcast(Boss, TEXT("vharra.spray"), Mouth, FRotator(-8.f, Yaw, 0.f), Range / 100.f, NAME_None);
			for (AWRCharacterBase* V : VhFoes(Boss, Chase))
			{
				const float D = FVector::Dist2D(V->GetActorLocation(), Mouth) - V->GetSimpleCollisionRadius();
				const float B = VhBearing(Mouth, Yaw, V->GetActorLocation());
				if (D <= Range && FMath::Abs(B) <= Half + 5.f)
				{
					VhHit(Boss, V, VhT(TEXT("sprayTickDamage"), 15.f), VhT(TEXT("sprayTickBleed"), 15.f), false, false, 2, false, 0.f, Mouth);
				}
			}
		}
		const int32 Want = FMath::RoundToInt(VhT(TEXT("sprayPuddles"), 3.f));
		if (St.Puddles < Want && A >= (St.Puddles + 0.5f) / Want)
		{
			++St.Puddles;
			const FVector P = Mouth + FRotator(0.f, Yaw, 0.f).Vector() * Range * 0.65f;
			bool bOk = false;
			const FVector G = VhLand(Boss->GetWorld(), P, bOk, 1500.f, 3000.f);
			// On the terraces (not the deep pool, where it just clouds the water).
			if (bOk && G.Z > St.Arena.PoolFloor + 300.f)
			{
				VhPuddle(Boss->GetWorld(), G, VhT(TEXT("sprayPuddleRadius"), 220.f), VhT(TEXT("sprayPuddleSeconds"), 6.f), VhT(TEXT("sprayPuddleBleed"), 10.f), false, 0, Boss);
			}
		}
		return true;
	}
	if (Id == TEXT("anchor_heave"))
	{
		// The forebody's slam: out along its line and back as the disc hauls it home.
		const float Lunge = VhT(TEXT("heaveLunge"), 500.f) * FMath::Sin(A * PI);
		const FVector At = St.MoveFrom + Fwd * Lunge;
		Boss->SetActorLocation(FVector(At.X, At.Y, Boss->GetActorLocation().Z), false);
		return true;
	}
	if (Id == TEXT("clot_spit") || Id == TEXT("brood_shed") || Id == TEXT("the_sinking") || Id == TEXT("ambush_rise") || Id == TEXT("drowning_coil"))
	{
		return true;   // their shots, young, waves and squeezes run on their own
	}
	return false;
}

void FWRSwampBoss::VharraEndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss);
	const FName Id = Attack.Id;
	if (!Body)
	{
		return;
	}
	if (Id == TEXT("three_jaw_drain") || Id == TEXT("drag"))
	{
		if (!St.Grabbed.IsValid()) Body->Set(WRSwampBossFlag::JawOpen, false);
	}
	else if (Id.ToString().StartsWith(TEXT("tail_")))
	{
		if (Id == TEXT("tail_overhead") || !Attack.bRow || Attack.Row.Next.IsNone()) Body->Lash = 0;
	}
	else if (Id == TEXT("ambush_rise"))
	{
		Body->Set(WRSwampBossFlag::Rising, false);
	}
	else if (Id == TEXT("drowning_coil"))
	{
		Body->Anchor = FVector::ZeroVector;
	}
	else if (Id == TEXT("the_sinking"))
	{
		Body->Set(WRSwampBossFlag::Rising, false);
		Body->Set(WRSwampBossFlag::Free, true);
	}
}

void FWRSwampBoss::VharraModifyHit(const AWRBoss* Boss, const FWRBossAttack& Attack, FWRHit& Hit)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	// Her blows are teeth, trunk and blood: physical, with the rows' bleed riding along. The drain's and the drag's catch
	// hold as a grab (unblockable, through hyperarmor).
	Hit.Element = WRTags::Element_Physical;
	if (Attack.Id == TEXT("three_jaw_drain") || Attack.Id == TEXT("drag"))
	{
		Hit.bGrab = true;
		Hit.bUnblockable = true;
	}
}

void FWRSwampBoss::VharraHitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!Boss || !Victim || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (AttackId == TEXT("three_jaw_drain") && Result == EWRHitResult::Hit && Victim->IsAlive())
	{
		VhStartGrab(Boss, St, Victim, TEXT("drain"), VhT(TEXT("drainSeconds"), 3.f), VhT(TEXT("drainDps"), 18.f));
	}
	else if (AttackId == TEXT("drag") && Result == EWRHitResult::Hit && Victim->IsAlive())
	{
		VhStartGrab(Boss, St, Victim, TEXT("drag"), VhT(TEXT("dragSeconds"), 2.f), VhT(TEXT("dragDps"), 25.f));
	}
	else if (AttackId == TEXT("ambush_rise") && bLanded && Victim->IsAlive())
	{
		// Thrown 4 m up out of the water.
		const float Up = FMath::Sqrt(2.f * 980.f * VhT(TEXT("ambushLaunch"), 400.f));
		const FVector Out = (Victim->GetActorLocation() - St.Aim).GetSafeNormal2D();
		Victim->LaunchCharacter(Out * 250.f + FVector(0.f, 0.f, Up), true, true);
	}
	else if (AttackId == TEXT("drowning_coil") && bLanded && Victim->IsAlive())
	{
		// Squeezed off the high ground into the water (where the lightning is).
		const FVector Out = (Victim->GetActorLocation() - St.CoilAt).GetSafeNormal2D();
		const float K = VhT(TEXT("coilKnock"), 700.f);
		Victim->LaunchCharacter(Out * K + FVector(0.f, 0.f, K * 0.35f), true, true);
	}
}

float FWRSwampBoss::VharraIncoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return Damage;
	}
	// Weak to Lightning (SPEC 1.5x), convulsing or not.
	if (Hit.Element == WRTags::Element_Lightning)
	{
		Damage *= WRTune(TEXT("bosses.weaknessMultiplier"), 1.5f);
	}
	// The anchor disc, once she's let go of the wall: a lock point that takes more.
	if (const FWRVharraFight* St = VhFind(Boss); St && Boss->Phase >= 2 && !Hit.ImpactPoint.IsZero()
		&& FVector::Dist(Hit.ImpactPoint, VhDiscPoint(Boss, *St)) <= VhT(TEXT("discRadius"), 350.f))
	{
		Damage *= VhT(TEXT("discDamageTaken"), 1.3f);
	}
	return Damage;
}

bool FWRSwampBoss::VharraOwnWeakness(const AWRBoss* Boss)
{
	// Her weakness is Lightning (VharraIncoming), not the opposite of fire or frost.
	return Boss && Boss->CreatureId == VhQueenId();
}

void FWRSwampBoss::VharraDamaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!Boss || Boss->CreatureId != VhQueenId() || Hit.bDamageOverTime)
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	// A Lightning Wildbrand's blow or a lightning bolt on her while she's in the water: lightning on water she's touching
	// (cook 2.14). (Chains and zaps count through WRLightning::Zap, bolts elsewhere through their impact.)
	if (Hit.Element == WRTags::Element_Lightning && (Hit.bMelee || Hit.bProjectile) && VhQueenInWater(Boss, St))
	{
		LightningAt(Boss->GetWorld(), VhFeet(Boss), Damage, Hit.Instigator, false);
	}
	// The three blood sacs: their own health (1,200 each); the blows that land on them.
	const FWRCreatureDef* Def = Boss->GetDef();
	if (!Def || Hit.ImpactPoint.IsZero())
	{
		return;
	}
	for (int32 i = 0; i < Def->Parts.Num() && i < 8; ++i)
	{
		const FWRCreatureDef::FBodyPoint& P = Def->Parts[i];
		if (P.OnBreak != TEXT("sac") || Boss->IsPartBroken(P.Id)) continue;
		const FString S = P.Id.ToString();
		const int32 Sac = FMath::Clamp(S.IsEmpty() ? 0 : int32(S[S.Len() - 1] - TEXT('0')), 0, 2);
		if (FVector::Dist(Boss->BodyPointLocation(P), Hit.ImpactPoint) > WRTune(TEXT("stance.partRadius"), 120.f) + Boss->GetSimpleCollisionRadius() * 0.25f) continue;
		St.SacHP[Sac] -= Damage;
		if (St.SacHP[Sac] <= 0.f)
		{
			Boss->PartsBroken |= uint8(1 << i);
			Boss->OnRep_PartsBroken();
			UE_LOG(LogWildreach, Log, TEXT("PART Vharra's %s ruptured"), *P.Id.ToString());
			Boss->OnPartBroken(P, Hit.Instigator);
		}
	}
}

void FWRSwampBoss::VharraPartBroken(AWRBoss* Boss, const FWRCreatureDef::FBodyPoint& Part, AActor* By)
{
	if (!Boss || Boss->CreatureId != VhQueenId() || Part.OnBreak != TEXT("sac"))
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	const FString S = Part.Id.ToString();
	const int32 Sac = FMath::Clamp(S.IsEmpty() ? 0 : int32(S[S.Len() - 1] - TEXT('0')), 0, 2);
	if (St.Burst & (1 << Sac))
	{
		return;
	}
	// It spills a 10 m bleedwater slick for 30 s and cuts her drain heal by a third.
	St.Burst |= uint8(1 << Sac);
	St.Fill[Sac] = 0.f;
	St.HealMult = FMath::Max(0.f, St.HealMult - VhT(TEXT("sacHealCut"), 0.3334f));
	const FVector At = Boss->BodyPointLocation(Part);
	bool bOk = false;
	const FVector G = VhLand(Boss->GetWorld(), At, bOk, 500.f, 3000.f);
	VhPuddle(Boss->GetWorld(), bOk ? FVector(G.X, G.Y, FMath::Max(G.Z, VhSurface(St))) : At, VhT(TEXT("sacSlickRadius"), 1000.f), VhT(TEXT("sacSlickSeconds"), 30.f),
		VhT(TEXT("sacSlickBleed"), 10.f), false, 0, Boss);
	if (UWRSwampBossBody* Body = UWRSwampBossBody::Get(Boss))
	{
		Body->Burst = St.Burst;
		Body->SetSac(Sac, 0.f);
		Body->MulticastMoment(TEXT("vharra.sacburst"), At, 2.f, TEXT("creature.vharra.hurt"));
	}
	Boss->Status->Stagger(VhT(TEXT("sacBurstStagger"), 1.0f));
	UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra's sac %d bursts (drain heal x%.2f) by %s"), Sac, St.HealMult, *GetNameSafe(By));
}

void FWRSwampBoss::VharraPhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Boss);
	VhEnsureArena(Boss, St);
	UWRSwampBossBody* Body = VhBody(Boss);
	// Snap, snap, drag becomes snap, snap, snap, drag when the water turns.
	const int32 SnapB = VhRowIndex(Boss->Attacks, TEXT("snap_b"));
	if (Boss->Attacks.IsValidIndex(SnapB) && Boss->Attacks[SnapB].bRow)
	{
		Boss->Attacks[SnapB].Row.Next = NewPhase >= 3 && VhRowIndex(Boss->Attacks, TEXT("snap_c")) != INDEX_NONE ? FName(TEXT("snap_c")) : FName(TEXT("drag"));
	}
	if (NewPhase <= 1)
	{
		if (Body)
		{
			Body->Set(WRSwampBossFlag::Free, false);
			Body->Set(WRSwampBossFlag::Swimming, false);
			if (St.Arena.bFound) Body->Anchor = St.Arena.AnchorAt;
		}
		return;
	}
	if (NewPhase == 2)
	{
		// The swimming queen: off the wall, the flood up (if the Sinking was cut short, it floods now).
		if (Body)
		{
			Body->Set(WRSwampBossFlag::Free, true);
			Body->Anchor = FVector::ZeroVector;
		}
		if (AWRSwampFlood* Flood = St.Flood.Get(); Flood && !Flood->IsFlooded()) Flood->StartFlood();
		Boss->SpeedBoost = 1.f;
		UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra lets go of the wall: phase 2"));
		return;
	}
	// The Water Turns (hidden phase 3, artifacts only): the flood to blood.
	if (AWRSwampFlood* Flood = St.Flood.Get())
	{
		if (!Flood->IsFlooded()) Flood->StartFlood();
		Flood->SetKind(2);
		WRFx::Broadcast(Boss, TEXT("blood.tide"), FVector(Flood->GetActorLocation().X, Flood->GetActorLocation().Y, Flood->SurfaceZ()), FRotator::ZeroRotator, Flood->FloodRadius / 100.f, TEXT("swamp.vharra.flood"));
	}
	if (Body) Body->Set(WRSwampBossFlag::Free, true);
	St.NextBloodTick = VhNow(Boss) + 1.0;
	UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra: the Water Turns (phase 3)"));
}

bool FWRSwampBoss::VharraPlaceRunback(AWRBoss* Boss)
{
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return false;
	}
	// The run-back stone 50 m out from the Throat Stair (swamp.json arenas.vharra.runback).
	const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(VhQueenId());
	const FVector At = Row.IsValid() ? WRSwamp::Point(Row, TEXT("runback")) : FVector::ZeroVector;
	if (At.IsZero())
	{
		return false;
	}
	for (TActorIterator<AWRRunbackStone> It(Boss->GetWorld()); It; ++It)
	{
		if (It->BossId == Boss->CreatureId) { Boss->Runback = *It; return true; }
	}
	bool bOk = false;
	const FVector G = VhLand(Boss->GetWorld(), At, bOk, 3000.f, 6000.f);
	if (!bOk)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector Centre = VhArenaCentre(Row);
	const FRotator Face(0.f, (Centre - G).Rotation().Yaw, 0.f);
	if (AWRRunbackStone* Stone = Boss->GetWorld()->SpawnActor<AWRRunbackStone>(AWRRunbackStone::StaticClass(), G, Face, Params))
	{
		Stone->BossId = Boss->CreatureId;
		Boss->Runback = Stone;
		UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra's shrine-stone at %s"), *G.ToCompactString());
	}
	return true;
}

FName FWRSwampBoss::VharraBossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	if (Boss && Boss->CreatureId == VhQueenId())
	{
		// The Mire heart (kills on bleeding enemies heal you); the Scarred one when the items table has it.
		static const FName Scarred(TEXT("mire_heart_scarred"));
		return bScarred && FWRItemRegistry::Get().Find(Scarred) ? Scarred : FName(TEXT("mire_heart"));
	}
	return Fallback;
}

float FWRSwampBoss::VharraStanceTaken(AWRCharacterBase* Target, const FWRHit& Hit, float Stance)
{
	(void)Stance;
	AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!Boss || Boss->CreatureId != VhQueenId())
	{
		return 1.f;
	}
	const FWRVharraFight* St = VhFind(Boss);
	if (!St)
	{
		return 1.f;
	}
	float Mult = 1.f;
	// Convulsing: x2 stance taken.
	if (VhNow(Boss) < St->ConvulseUntil) Mult *= VhT(TEXT("convulseStance"), 2.f);
	// The anchor disc (phase 2 on).
	if (Boss->Phase >= 2 && !Hit.ImpactPoint.IsZero() && FVector::Dist(Hit.ImpactPoint, VhDiscPoint(Boss, *St)) <= VhT(TEXT("discRadius"), 350.f))
	{
		Mult *= VhT(TEXT("discStance"), 1.5f);
	}
	return Mult;
}

float FWRSwampBoss::VharraBuildupThresholdScale(const AWRCharacterBase* Who, int32 StatusId)
{
	// Bleed builds on her at x0.5 (she's blood): her threshold x2.
	const AWRCreature* C = Cast<AWRCreature>(Who);
	if (C && C->CreatureId == VhQueenId() && StatusId == WRStatus::Bleed)
	{
		return VhT(TEXT("bleedThresholdScale"), 2.f);
	}
	return 1.f;
}

bool FWRSwampBoss::VharraPrompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	// Nothing of hers to use by hand: the rods take lightning, the altar has its own prompt.
	(void)PC;
	(void)OutTarget;
	(void)OutVerb;
	return false;
}

bool FWRSwampBoss::VharraClientInteract(AWRPlayerController* PC)
{
	(void)PC;
	return false;
}

// ---------------------------------------------------------------- the lightning counter

void FWRSwampBoss::Shock(AWRBoss* Vharra, AWRSwampRod* From, const FVector& At)
{
	UWorld* World = From ? From->GetWorld() : Vharra ? Vharra->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	AWRSwampFlood* Flood = VhFlood(World, From ? From->GetActorLocation() : At, 9000.f);
	if (!Flood)
	{
		return;
	}
	AActor* By = VhWaters().FindOrAdd(Flood).By.Get();
	VhShockWater(World, Flood, From, At, By, Vharra);
	if (!Vharra || !Vharra->IsAlive() || Vharra->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraFight& St = VhS(Vharra);
	VhEnsureArena(Vharra, St);
	const double T = VhNow(World);
	if (Vharra->Phase >= 3)
	{
		// Lightning burns the blood: no heal for 10 s, and the water round the rod clots into a dark crust you can stand
		// on for 10 s (new dry ground).
		St.NoHealUntil = T + VhT(TEXT("p3NoHeal"), 10.f);
		if (From)
		{
			const FVector Foot = From->GetActorLocation();
			const FVector In = (Flood->GetActorLocation() - Foot).GetSafeNormal2D();
			const FVector Spot = FVector(Foot.X, Foot.Y, 0.f) + In * VhT(TEXT("crustOffset"), 500.f);
			const float Seconds = VhT(TEXT("crustSeconds"), 10.f);
			if (AWRSwampBossProp* Crust = VhProp(World, TEXT("crust"), FVector(Spot.X, Spot.Y, Flood->SurfaceZ()), VhT(TEXT("crustRadius"), 600.f), 0, 0.f, In.Rotation().Yaw))
			{
				Crust->MakeHazard(0.f, false, Seconds, Vharra);
				Crust->SetLifeSpan(Seconds + 1.f);
				St.Crusts.Add(Crust);
				WRFx::Broadcast(World, TEXT("vharra.crust"), FVector(Spot.X, Spot.Y, Flood->SurfaceZ()), FRotator::ZeroRotator, VhT(TEXT("crustRadius"), 600.f) / 100.f, TEXT("water.splash"));
			}
		}
		St.Crusts.RemoveAll([](const TWeakObjectPtr<AWRSwampBossProp>& C) { return !C.IsValid(); });
	}
	// Her body in that water: she convulses 4 s (an electric eel's thrash on the surface), x2 stance, lightning x1.5.
	if (!VhQueenInWater(Vharra, St) || T < St.ConvulseGapUntil)
	{
		return;
	}
	St.ConvulseUntil = T + VhT(TEXT("convulseSeconds"), 4.f);
	St.ConvulseGapUntil = St.ConvulseUntil + VhT(TEXT("convulseGap"), 6.f);
	St.NextThrashAt = T;
	VhReleaseGrab(Vharra, St);
	// The shock breaks whatever she was doing (a cut-short Sinking still floods: VharraPhaseChanged).
	if (Vharra->Current != INDEX_NONE)
	{
		VhRestLook(Vharra, St);
		Vharra->Current = INDEX_NONE;
		Vharra->StrikeState = AWRBoss::EStrike::None;
	}
	Vharra->SpeedBoost = 0.f;
	if (UWRSwampBossBody* Body = VhBody(Vharra))
	{
		Body->Set(WRSwampBossFlag::Convulsing, true);
		Body->Set(WRSwampBossFlag::Swimming, false);
	}
	WRSound::Broadcast(Vharra, TEXT("creature.vharra.hurt"));
	WRFx::BroadcastOn(Vharra, TEXT("status.shock"), NAME_None, 3.f, NAME_None, VhT(TEXT("convulseSeconds"), 4.f));
	UE_LOG(LogWildreach, Log, TEXT("BOSS Vharra convulses in the shocked water (%.1f s)"), VhT(TEXT("convulseSeconds"), 4.f));
}

float FWRSwampBoss::WaterZ(const AWRBoss* Boss)
{
	if (const FWRVharraFight* St = VhFind(Boss); St && St->Flood.IsValid())
	{
		return St->Flood->SurfaceZ();
	}
	if (const AWRSwampFlood* Flood = Boss ? VhFlood(Boss->GetWorld(), Boss->GetActorLocation()) : nullptr)
	{
		return Flood->SurfaceZ();
	}
	return FWRSwampData::Get().bLoaded ? FWRSwampData::Get().SeaZ : 60.f;
}

void FWRSwampBoss::LightningAt(UWorld* World, const FVector& At, float Amount, AActor* By, bool bForce)
{
	if (!World || World->GetNetMode() == NM_Client || Amount <= 0.f)
	{
		return;
	}
	AWRSwampFlood* Flood = VhFlood(World, At, 9000.f);
	if (!Flood)
	{
		return;   // not the Throat
	}
	AWRBoss* Queen = VhQueen(World, Flood->GetActorLocation());
	FWRVharraWater& W = VhWaters().FindOrAdd(Flood);
	W.By = By;
	// A rod takes what lands on it: 60 lightning damage within 2 s on it (or a strike) fires it.
	const float Catch = VhT(TEXT("rodCatch"), 250.f);
	for (TActorIterator<AWRSwampRod> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Flood->GetActorLocation()) > Flood->FloodRadius + 3000.f || !VhRodCatches(*It, At, Catch)) continue;
		if (It->TakeLightning(Amount, By, bForce))
		{
			Shock(Queen, *It, At);
		}
		return;
	}
	// Into the water: a spell into the pool, a bolt at someone wading, a strike on the surface (60 in 2 s, or at once).
	const float Surface = Flood->SurfaceZ();
	const bool bWater = FVector::Dist2D(At, Flood->GetActorLocation()) <= Flood->FloodRadius && At.Z <= Surface + 120.f && At.Z >= Flood->FloorZ - 200.f;
	if (!bWater)
	{
		return;
	}
	const double T = VhNow(World);
	const float Window = VhT(TEXT("waterWindow"), 2.f);
	W.Log.RemoveAll([T, Window](const TPair<double, float>& E) { return T - E.Key > Window; });
	W.Log.Add({ T, Amount });
	float Sum = 0.f;
	for (const TPair<double, float>& E : W.Log) Sum += E.Value;
	if ((bForce || Sum >= VhT(TEXT("waterThreshold"), 60.f)) && T >= W.NextShockAt)
	{
		W.Log.Reset();
		W.NextShockAt = T + VhT(TEXT("waterShockCooldown"), 3.f);
		if (Queen) Shock(Queen, nullptr, At);
		else VhShockWater(World, Flood, nullptr, At, By, nullptr);
	}
}

void FWRSwampBoss::StrikeLanded(UWorld* World, const FWRStrikeCall& Call)
{
	// A strike (the storm, a Tempest, a Thunder god bolt) on a Throat rod or into its water shocks at once.
	LightningAt(World, Call.Spot, FMath::Max(1.f, Call.Damage), Call.Instigator.Get(), true);
}

void FWRSwampBoss::VharraWaterShocked(UWorld* World, const FVector& Center, float Radius, float Seconds, AActor* Instigator)
{
	(void)Seconds;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	AWRSwampFlood* Flood = VhFlood(World, Center, 9000.f);
	if (!Flood)
	{
		return;
	}
	// A stormglass jar: near a rod it fires the rod (as lightning on it would); in the water it shocks the water itself.
	AWRBoss* Queen = VhQueen(World, Flood->GetActorLocation());
	VhWaters().FindOrAdd(Flood).By = Instigator;
	for (TActorIterator<AWRSwampRod> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Flood->GetActorLocation()) > Flood->FloodRadius + 3000.f || !VhRodCatches(*It, Center, Radius + VhT(TEXT("rodCatch"), 250.f))) continue;
		if (It->TakeLightning(VhT(TEXT("rodThreshold"), 60.f), Instigator, true))
		{
			Shock(Queen, *It, Center);
			return;
		}
	}
	const float Surface = Flood->SurfaceZ();
	if (FVector::Dist2D(Center, Flood->GetActorLocation()) > Flood->FloodRadius + Radius * 0.5f || Center.Z > Surface + 150.f)
	{
		return;
	}
	if (Queen) Shock(Queen, nullptr, Center);
	else VhShockWater(World, Flood, nullptr, Center, Instigator, nullptr);
}

bool FWRSwampBoss::WeepingGatePoint(FVector& Out, FRotator& Facing)
{
	// The one-way door from the Sunken Coven opens on the Throat's rim (swamp.json arenas.vharra.gate): you step out in
	// front of it, facing down into the sinkhole.
	const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(VhQueenId());
	if (!Row.IsValid())
	{
		return false;
	}
	const FVector Gate = WRSwamp::Point(Row, TEXT("gate"));
	if (Gate.IsZero())
	{
		return false;
	}
	const FVector Centre = VhArenaCentre(Row);
	const FVector In = (Centre - Gate).GetSafeNormal2D();
	Out = Gate + In * VhT(TEXT("gateStepOut"), 250.f) + FVector(0.f, 0.f, VhT(TEXT("gateStandZ"), 110.f));
	Facing = FRotator(0.f, In.Rotation().Yaw, 0.f);
	return true;
}

// ---------------------------------------------------------------- the arena

bool FWRSwampBoss::VharraPlaceArena(UWRSwampArenas* Arenas, const TSharedPtr<FJsonObject>& Row)
{
	UWorld* World = Arenas ? Arenas->GetWorld() : nullptr;
	if (!World || !Row.IsValid() || World->GetNetMode() == NM_Client)
	{
		return false;
	}
	const FVector Centre = VhArenaCentre(Row);
	// Once its ground is in (a survivor near enough to stream it), once a world.
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = FVector::Dist2D(It->GetActorLocation(), Centre) < VhT(TEXT("placeRadius"), 30000.f);
	bool bOk = false;
	VhLand(World, Centre, bOk, 6000.f, 12000.f);
	if (!bNear || !bOk)
	{
		return false;
	}
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It)
	{
		if (It->CreatureId == VhQueenId()) return true;   // already here
	}
	const FWRVharraArena A = VhReadArena(World);
	if (!A.bFound)
	{
		return false;
	}

	// Her spawner on her seat east of the anchor (it brings her back server.bossRespawnHours after she falls).
	const FTransform SeatAt(FRotator(0.f, A.SeatYaw, 0.f), A.Seat + FVector(0.f, 0.f, 50.f));
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), SeatAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = VhQueenId();
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = true;
		Spawner->FinishSpawning(SeatAt);
	}

	// The Throat's own water (the flood rises from it; it's what the rods electrify).
	{
		const FTransform WaterAt(FRotator::ZeroRotator, FVector(Centre.X, Centre.Y, A.WaterZ));
		if (AWRSwampFlood* Flood = World->SpawnActorDeferred<AWRSwampFlood>(AWRSwampFlood::StaticClass(), WaterAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Flood->BaseZ = A.WaterZ;
			Flood->FloorZ = A.PoolFloor;
			Flood->PoolRadius = A.PoolR;
			Flood->FloodRadius = A.FloodR;
			Flood->Rise = VhT(TEXT("floodRise"), 120.f);
			Flood->RiseSeconds = VhT(TEXT("floodSeconds"), 6.f);
			Flood->WaterKind = uint8(WRSwamp::IsTide(World) ? 1 : 0);
			Flood->FinishSpawning(WaterAt);
		}
	}

	// The six Rodwarden rods on their plinths between the terraces, each facing the pool (its copper strap runs down
	// into the water on that side).
	for (const FVector& P : A.Plinths)
	{
		bool bGround = false;
		const FVector G = VhLand(World, P, bGround, 3000.f, 6000.f);
		const FTransform RodAt(FRotator(0.f, (Centre - G).GetSafeNormal2D().Rotation().Yaw, 0.f), bGround ? G : P);
		if (AWRSwampRod* Rod = World->SpawnActorDeferred<AWRSwampRod>(AWRSwampRod::StaticClass(), RodAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Rod->Height = VhT(TEXT("rodHeight"), 650.f);
			Rod->FinishSpawning(RodAt);
		}
	}

	// The four ossuary mounds' bones (the terrain carries their 3 m bulk).
	for (const FVector& P : A.Mounds)
	{
		bool bGround = false;
		const FVector G = VhLand(World, P, bGround, 3000.f, 6000.f);
		VhProp(World, TEXT("mound"), bGround ? G : P, VhT(TEXT("moundRadius"), 520.f), 0, VhT(TEXT("moundHeight"), 320.f), (Centre - G).GetSafeNormal2D().Rotation().Yaw);
	}

	// The Blood path's altar at the top of the Throat Stair, beside the Coven's tithe basins (its basin is the flood's
	// fixtures: AWRSwampFlood draws it on every machine).
	{
		FVector AltarAt;
		float AltarYaw = 0.f;
		if (VhAltarSpot(Row, AltarAt, AltarYaw))
		{
			bool bGround = false;
			const FVector G = VhLand(World, AltarAt, bGround, 3000.f, 6000.f);
			const FTransform T(FRotator(0.f, AltarYaw, 0.f), (bGround ? G : AltarAt) + FVector(0.f, 0.f, 50.f));
			if (AWRBossAltar* Altar = World->SpawnActorDeferred<AWRBossAltar>(AWRBossAltar::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Altar->PathId = TEXT("blood");
				Altar->FinishSpawning(T);
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP the Mother's Throat placed at %s: seat %s, disc %s, %d rods, %d mounds"), *Centre.ToCompactString(), *A.Seat.ToCompactString(),
		*A.AnchorAt.ToCompactString(), A.Plinths.Num(), A.Mounds.Num());
	return true;
}

// ---------------------------------------------------------------- the server command

FString FWRSwampBoss::VharraCommand(AWRPlayerController* PC, const TArray<FString>& Args)
{
	if (Args.Num() == 0 || !Args[0].Equals(TEXT("vharra"), ESearchCase::IgnoreCase))
	{
		return FString();
	}
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return TEXT("vharra: server only");
	}
	const APawn* Pawn = PC->GetPawn();
	const FVector Near = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	AWRSwampFlood* Flood = nullptr;
	for (TActorIterator<AWRSwampFlood> It(World); It; ++It) { Flood = *It; break; }
	AWRBoss* Queen = Flood ? VhQueen(World, Flood->GetActorLocation(), 20000.f) : VhQueen(World, Near, 1e9f);
	const FString Sub = Args.Num() > 1 ? Args[1].ToLower() : FString(TEXT("status"));
	if (Sub == TEXT("rods"))
	{
		FString Out;
		int32 i = 0;
		for (TActorIterator<AWRSwampRod> It(World); It; ++It)
		{
			Out += FString::Printf(TEXT("rod %d: %s  "), i++, It->State == 0 ? TEXT("armed") : *FString::Printf(TEXT("cooling %.0f s"), FMath::Max(0.f, It->CoolEndsAt - VhServerTime(World))));
		}
		return Out.IsEmpty() ? FString(TEXT("vharra: no rods placed")) : Out;
	}
	if (Sub == TEXT("shock"))
	{
		if (!Flood) return TEXT("vharra: no Throat flood placed");
		VhWaters().FindOrAdd(Flood).By = PC->GetPawn();
		AWRSwampRod* First = nullptr;
		for (TActorIterator<AWRSwampRod> It(World); It && !First; ++It) if (It->TakeLightning(1000.f, PC->GetPawn(), true)) First = *It;
		Shock(Queen, First, First ? First->GetActorLocation() : Flood->GetActorLocation());
		return First ? TEXT("vharra: a rod fires into the water") : TEXT("vharra: the water is shocked");
	}
	if (Sub == TEXT("flood"))
	{
		if (!Flood) return TEXT("vharra: no Throat flood placed");
		Flood->StartFlood();
		return TEXT("vharra: the Throat floods");
	}
	if (Sub == TEXT("drain"))
	{
		if (!Flood) return TEXT("vharra: no Throat flood placed");
		Flood->Drain();
		return TEXT("vharra: the flood drains");
	}
	if (Sub == TEXT("blood") || Sub == TEXT("murk") || Sub == TEXT("bleedwater"))
	{
		if (!Flood) return TEXT("vharra: no Throat flood placed");
		Flood->SetKind(uint8(Sub == TEXT("blood") ? 2 : Sub == TEXT("bleedwater") ? 1 : 0));
		return FString::Printf(TEXT("vharra: the Throat's water is %s"), *Sub);
	}
	if (Sub == TEXT("phase") && Args.Num() > 2)
	{
		if (!Queen) return TEXT("vharra: she isn't out");
		const int32 P = FMath::Clamp(FCString::Atoi(*Args[2]), 1, 3);
		Queen->SetPhase(P);
		return FString::Printf(TEXT("vharra: phase %d"), P);
	}
	if (!Queen) return TEXT("vharra: she isn't out");
	const FWRVharraFight* St = VhFind(Queen);
	return FString::Printf(TEXT("vharra: phase %d, %.0f%% health, sacs %d%%/%d%%/%d%% (burst %d), heal x%.2f, %s, flood %s"), Queen->Phase, VhHealthFrac(Queen) * 100.f,
		St ? FMath::RoundToInt(St->Fill[0] * 100.f) : 0, St ? FMath::RoundToInt(St->Fill[1] * 100.f) : 0, St ? FMath::RoundToInt(St->Fill[2] * 100.f) : 0,
		St ? int32(St->Burst) : 0, St ? St->HealMult : 1.f, St && VhNow(Queen) < St->ConvulseUntil ? TEXT("convulsing") : TEXT("not convulsing"),
		Flood ? (Flood->IsFlooded() ? TEXT("up") : TEXT("down")) : TEXT("not placed"));
}

// ---------------------------------------------------------------- the Rodwarden rods

AWRSwampRod::AWRSwampRod()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetCullDistanceSquared(FMath::Square(40000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Shaft = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(RootComponent);
	// Solid: it stops bodies and bolts (a Spark into the mast is lightning on the rod).
	Shaft->SetCollisionProfileName(TEXT("BlockAll"));
	Shaft->SetCanEverAffectNavigation(false);
	Shaft->SetCapsuleSize(24.f, 300.f);
	Shaft->SetRelativeLocation(FVector(0.f, 0.f, 320.f));
}

void AWRSwampRod::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampRod, State);
	DOREPLIFETIME(AWRSwampRod, CoolEndsAt);
	DOREPLIFETIME(AWRSwampRod, Height);
}

void AWRSwampRod::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Look();
	if (HasAuthority() && RodHandle == INDEX_NONE)
	{
		// The storm's bolts find its tip as they find the Rodfield's (WRStorm rods).
		RodHandle = WRStorm::AddRod(GetWorld(), this, Tip());
	}
}

void AWRSwampRod::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && RodHandle != INDEX_NONE)
	{
		WRStorm::RemoveRod(GetWorld(), RodHandle);
		RodHandle = INDEX_NONE;
	}
	if (CoolFx)
	{
		WRFx::Stop(CoolFx);
		CoolFx = nullptr;
	}
	Super::EndPlay(Reason);
}

FVector AWRSwampRod::Tip() const
{
	return GetActorLocation() + FVector(0.f, 0.f, Height);
}

bool AWRSwampRod::TakeLightning(float Amount, AActor* By, bool bForce)
{
	if (!HasAuthority() || State == 1)
	{
		return false;   // cooling (glowing blue): it can't fire again yet
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Window = VhT(TEXT("rodWindow"), 2.f);
	Taken.RemoveAll([Now, Window](const TPair<double, float>& E) { return Now - E.Key > Window; });
	Taken.Add({ Now, Amount });
	float Sum = 0.f;
	for (const TPair<double, float>& E : Taken) Sum += E.Value;
	if (!bForce && Sum < VhT(TEXT("rodThreshold"), 60.f))
	{
		return false;
	}
	Taken.Reset();
	State = 1;
	CoolEndsAt = VhServerTime(this) + VhT(TEXT("rodCooldown"), 20.f);
	OnRep_Look();
	ForceNetUpdate();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP rod %s fires (%.0f lightning, by %s)"), *GetName(), Sum, *GetNameSafe(By));
	return true;
}

void AWRSwampRod::Rearm()
{
	if (!HasAuthority())
	{
		return;
	}
	State = 0;
	CoolEndsAt = 0.f;
	Taken.Reset();
	OnRep_Look();
	ForceNetUpdate();
}

void AWRSwampRod::MulticastArcs_Implementation(const TArray<FVector_NetQuantize>& To)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	ArcTo.Reset();
	for (const FVector_NetQuantize& P : To) ArcTo.Add(FVector(P));
	ArcUntil = GetWorld()->GetTimeSeconds() + VhT(TEXT("shockSeconds"), 1.5f);
	ArcClock = 0.f;
}

void AWRSwampRod::OnRep_Look()
{
	// The mast's collision stands its full height over the plinth.
	const float Tall = FMath::Max(100.f, Height - 20.f);
	Shaft->SetCapsuleSize(24.f, Tall * 0.5f);
	Shaft->SetRelativeLocation(FVector(0.f, 0.f, 20.f + Tall * 0.5f));
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (BuiltState == 255 || LookComps.Num() == 0)
	{
		Build();
	}
	// Fired: blue sparks crawl its crown through the cooldown. Armed again: the hum, and they're gone.
	if (State == 1 && !CoolFx)
	{
		CoolFx = WRFx::Attach(RootComponent, TEXT("vharra.rodglow"), NAME_None, 1.f);
		if (CoolFx) CoolFx->SetRelativeLocation(FVector(0.f, 0.f, Height - 20.f));
	}
	else if (State == 0 && CoolFx)
	{
		WRFx::Stop(CoolFx);
		CoolFx = nullptr;
	}
	if (BuiltState == 1 && State == 0)
	{
		WRSound::PlayOn(this, TEXT("storm.rod_hum"));
	}
	BuiltState = State;
}

void AWRSwampRod::Build()
{
	using namespace WRLookKit;
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	GlowRoot = nullptr;
	// An old Rodwarden rod (cook 2.20 beat 9: the storm people got this far and their rods still work): a copper mast,
	// green with age, socketed in a dressed granite plinth sunk into the terrace, braced by three iron stays, a crown of
	// three prongs round its point, and its copper earth-strap running down the plinth's face into the water: that's
	// how a strike on it gets into the Throat.
	const FSurf Copper = FSurf::Steel(VhHex(TEXT("#B87333")), 0.38f);
	FSurf Verd = FSurf::Steel(VhHex(TEXT("#4E9A84")), 0.62f);
	Verd.Metal = 0.35f;
	const FSurf Granite = FSurf::Stone(VhHex(TEXT("#5E5A54")), 0.9f);
	const FSurf WetFoot = FSurf::Stone(VhHex(TEXT("#34302B")), 0.55f);
	const FSurf Iron = FSurf::Steel(VhHex(TEXT("#3A3430")), 0.72f);
	const FSurf Rust = FSurf::Stone(VhHex(TEXT("#8A3B1E")), 0.85f);
	const FSurf Blue = FSurf::Lit(VhHex(TEXT("#9FD8FF")), 0.f);
	TArray<FWRLookBlock> Blocks, Glow;
	FKit Kit(GetActorTransform(), Blocks);
	FKit GlowKit(GetActorTransform(), Glow);
	const float H = FMath::Max(300.f, Height);

	// The plinth: three courses of dressed granite down into the terrace (wet and dark at its foot), an iron band,
	// the cap slab the mast is leaded into.
	Kit.Hex(WetFoot, 0.f, 0.f, -260.f, -150.f, 132.f, 0.f);
	Kit.Hex(Granite.Dim(0.85f), 0.f, 0.f, -150.f, -62.f, 120.f, 20.f);
	Kit.Hex(Granite, 0.f, 0.f, -62.f, 0.f, 108.f, 40.f);
	Kit.Hex(Iron, 0.f, 0.f, -40.f, -26.f, 112.f, 40.f);
	for (int32 i = 0; i < 6; ++i)
	{
		// Rust run-off streaking down from the band's rivets.
		const float A = FMath::DegreesToRadians(40.f + 60.f * i);
		const FVector P(FMath::Cos(A) * 96.f, FMath::Sin(A) * 96.f, -70.f);
		Kit.Add(Rust.Dim(0.8f + 0.2f * Kit.Rand(P, 1)), FVector(10.f, 4.f, 60.f + 30.f * Kit.Rand(P, 2)), P, FRotator(0.f, 40.f + 60.f * i, 0.f));
	}
	Kit.Disc(Granite.Dim(0.92f), FVector(0.f, 0.f, 8.f), FVector::UpVector, 205.f, 18.f);
	Kit.Disc(FSurf::Stone(VhHex(TEXT("#6B6E70")), 0.4f), FVector(0.f, 0.f, 18.f), FVector::UpVector, 56.f, 6.f);   // the lead socket

	// The mast: six lengths of copper, each joint a collar, the old ones gone verdigris.
	const int32 Lengths = 6;
	for (int32 s = 0; s < Lengths; ++s)
	{
		const float Z0 = 20.f + (H - 60.f) * s / Lengths, Z1 = 20.f + (H - 60.f) * (s + 1) / Lengths;
		const float D = FMath::Lerp(26.f, 11.f, float(s) / Lengths);
		Kit.Rod(s % 3 == 1 ? Verd : Copper.Dim(0.9f + 0.1f * (s % 2)), FVector(0.f, 0.f, Z0), FVector(0.f, 0.f, Z1), D);
		Kit.Disc(Verd.Dim(0.8f), FVector(0.f, 0.f, Z1), FVector::UpVector, D + 10.f, 8.f);
		// Its glow ring (blue-white while it cools).
		GlowKit.Disc(Blue, FVector(0.f, 0.f, Z1 + 5.f), FVector::UpVector, D + 4.f, 3.f);
	}
	// The crown: a point and three prongs swept out, a ring binding them.
	const FVector Crown(0.f, 0.f, H - 40.f);
	Kit.Spike(Copper, Crown, FVector::UpVector, 12.f, 60.f);
	for (int32 p = 0; p < 3; ++p)
	{
		const FVector Out = FRotator(0.f, 120.f * p, 0.f).Vector();
		Kit.Rod(Copper.Dim(0.95f), Crown, Crown + Out * 28.f + FVector(0.f, 0.f, 20.f), 6.f);
		Kit.Spike(Copper, Crown + Out * 28.f + FVector(0.f, 0.f, 20.f), (Out * 0.3f + FVector::UpVector).GetSafeNormal(), 6.f, 26.f);
	}
	Kit.Disc(Verd, Crown + FVector(0.f, 0.f, 6.f), FVector::UpVector, 40.f, 6.f);
	GlowKit.Disc(Blue, Crown + FVector(0.f, 0.f, 64.f), FVector::UpVector, 16.f, 14.f);

	// Three iron stays from a third of the way up to eyes leaded into the cap's edge, each with a turnbuckle.
	const float StayTop = 20.f + (H - 60.f) * 0.38f;
	for (int32 k = 0; k < 3; ++k)
	{
		const FVector Dir = FRotator(0.f, 60.f + 120.f * k, 0.f).Vector();
		const FVector Top(0.f, 0.f, StayTop);
		const FVector Foot = Dir * 92.f + FVector(0.f, 0.f, 18.f);
		Kit.Rod(Iron, Top + Dir * 12.f, Foot, 5.f);
		Kit.Add(Iron.Dim(0.85f), FVector(7.f, 7.f, 22.f), FMath::Lerp(Top + Dir * 12.f, Foot, 0.55f), FRotationMatrix::MakeFromZ(Foot - Top).Rotator());
		Kit.Disc(Iron, Foot, FVector::UpVector, 14.f, 6.f);
		Kit.Disc(Iron, Top + Dir * 10.f, Dir, 16.f, 5.f);
	}

	// The earth-strap: braided copper from the mast's foot over the cap's lip, down the plinth's pool side into the
	// water, ending on a copper plate in the water.
	bool bOk = false;
	const FVector Land = VhLand(GetWorld(), GetActorLocation() + GetActorForwardVector() * 320.f, bOk, 600.f, 1500.f);
	const float WaterLocal = (FWRSwampData::Get().bLoaded ? FWRSwampData::Get().SeaZ : 60.f) - GetActorLocation().Z;
	const float FloorLocal = bOk ? Land.Z - GetActorLocation().Z : WaterLocal - 40.f;
	const FVector Strap[] = { FVector(14.f, 0.f, 24.f), FVector(70.f, 0.f, 20.f), FVector(104.f, 0.f, 10.f), FVector(116.f, 0.f, -40.f), FVector(126.f, 0.f, -150.f),
		FVector(190.f, 4.f, FMath::Max(FloorLocal + 6.f, -250.f)), FVector(300.f, -6.f, FMath::Max(FloorLocal + 4.f, -260.f)) };
	for (int32 i = 0; i + 1 < int32(UE_ARRAY_COUNT(Strap)); ++i)
	{
		Kit.Rod(i % 2 ? Copper.Dim(0.85f) : Verd.Dim(0.9f), Strap[i], Strap[i + 1], 7.f);
		if (i >= 2 && i <= 4) Kit.Add(Iron, FVector(10.f, 16.f, 6.f), Strap[i + 1] + FVector(-4.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f));   // staples into the stone
	}
	Kit.Disc(Copper.Dim(0.7f), Strap[UE_ARRAY_COUNT(Strap) - 1] + FVector(10.f, 0.f, 0.f), FVector::UpVector, 80.f, 4.f);
	(void)WaterLocal;

	const auto Spawn = [this](const TArray<FWRLookBlock>& B, bool bGlow)
	{
		if (B.Num() == 0) return;
		USceneComponent* Frame = NewObject<USceneComponent>(this);
		Frame->SetupAttachment(RootComponent);
		Frame->RegisterComponent();
		LookComps.Add(Frame);
		FWRBuildLook::Spawn(this, Frame, B, FLinearColor::White, false);
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(false, Kids);
		for (USceneComponent* C : Kids) LookComps.Add(C);
		if (bGlow) GlowRoot = Frame;
	};
	Spawn(Blocks, false);
	Spawn(Glow, true);
}

void AWRSwampRod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = VhServerTime(this);
	if (HasAuthority() && State == 1 && Now >= CoolEndsAt)
	{
		// Re-armed: it can take a charge again.
		State = 0;
		CoolEndsAt = 0.f;
		OnRep_Look();
		ForceNetUpdate();
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The blue glow of a fired rod, fading over its cooldown (a flicker on it); dark when armed.
	if (GlowRoot)
	{
		const float Cool = FMath::Max(1.f, VhT(TEXT("rodCooldown"), 20.f));
		const float Left = State == 1 ? FMath::Clamp((CoolEndsAt - Now) / Cool, 0.f, 1.f) : 0.f;
		const float Glow = State == 1 ? (1.2f + 6.f * Left) * (0.85f + 0.15f * FMath::Sin(Now * 9.f)) : 0.f;
		TArray<USceneComponent*> Kids;
		GlowRoot->GetChildrenComponents(false, Kids);
		for (USceneComponent* C : Kids)
		{
			UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(C);
			if (!ISM || ISM->NumCustomDataFloats < 10 || !ISM->PerInstanceSMCustomData.IsValidIndex(8)) continue;
			if (FMath::IsNearlyEqual(ISM->PerInstanceSMCustomData[8], Glow, 0.05f)) continue;
			for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, Glow, i == ISM->GetInstanceCount() - 1);
		}
	}
	// The shock: arcs down the mast and out from the strap's plate across the water to each point, jagged, flickering.
	const double T = GetWorld()->GetTimeSeconds();
	if (T < ArcUntil)
	{
		ArcClock += DeltaSeconds;
		if (ArcClock >= 0.05f)
		{
			ArcClock = 0.f;
			const FVector Foot = GetActorLocation() + GetActorForwardVector() * 300.f;
			const FVector A0 = Tip(), A1 = GetActorLocation() + FVector(0.f, 0.f, 30.f);
			WRFx::SpawnAt(this, TEXT("vharra.waterarc"), A0, (A1 - A0).Rotation(), (A1 - A0).Size() / 100.f);
			for (const FVector& To : ArcTo)
			{
				FVector From(Foot.X, Foot.Y, To.Z + 4.f);
				for (int32 s = 1; s <= 3; ++s)
				{
					FVector Next = FMath::Lerp(FVector(Foot.X, Foot.Y, To.Z + 4.f), To + FVector(0.f, 0.f, 4.f), s / 3.f);
					if (s < 3)
					{
						const FVector Side = FVector::CrossProduct((To - Foot).GetSafeNormal2D(), FVector::UpVector);
						Next += Side * FMath::FRandRange(-160.f, 160.f);
					}
					const FVector D = Next - From;
					if (D.SizeSquared() > 1.f) WRFx::SpawnAt(this, TEXT("vharra.waterarc"), From, D.Rotation(), D.Size() / 100.f);
					From = Next;
				}
			}
		}
	}
}

// ---------------------------------------------------------------- the Throat's own water

AWRSwampFlood::AWRSwampFlood(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(false);   // every machine raises it from the replicated start time
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	SetNetCullDistanceSquared(FMath::Square(50000.f));
	SetHidden(false);
	bWaterVolume = true;
	Priority = 3;   // over the sea's own volume where they meet
	// Pawns only (CharacterMovement finds its physics volume by overlap; the swim code's water-line trace too);
	// traces, the camera, building and projectiles never see it.
	UBrushComponent* Shape = GetBrushComponent();
	Shape->SetMobility(EComponentMobility::Movable);
	Shape->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetCollisionObjectType(ECC_WorldDynamic);
	Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Shape->SetGenerateOverlapEvents(true);
	Shape->SetCanEverAffectNavigation(false);
}

void AWRSwampFlood::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampFlood, BaseZ);
	DOREPLIFETIME(AWRSwampFlood, FloorZ);
	DOREPLIFETIME(AWRSwampFlood, PoolRadius);
	DOREPLIFETIME(AWRSwampFlood, FloodRadius);
	DOREPLIFETIME(AWRSwampFlood, Rise);
	DOREPLIFETIME(AWRSwampFlood, RiseStartedAt);
	DOREPLIFETIME(AWRSwampFlood, DrainStartedAt);
	DOREPLIFETIME(AWRSwampFlood, RiseSeconds);
	DOREPLIFETIME(AWRSwampFlood, WaterKind);
	DOREPLIFETIME(AWRSwampFlood, ShockUntil);
}

void AWRSwampFlood::BeginPlay()
{
	Super::BeginPlay();
	FluidFriction = WRTune(TEXT("water.friction"), 2.f);
	OnRep_Shape();
	OnRep_Look();
}

float AWRSwampFlood::SurfaceZ() const
{
	if (RiseStartedAt <= 0.f)
	{
		return BaseZ;
	}
	const float Now = VhServerTime(this);
	float Up = Rise * FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Now - RiseStartedAt) / FMath::Max(0.1f, RiseSeconds), 0.f, 1.f));
	if (DrainStartedAt > 0.f)
	{
		Up *= 1.f - FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Now - DrainStartedAt) / FMath::Max(0.1f, VhT(TEXT("floodDrainSeconds"), 12.f)), 0.f, 1.f));
	}
	return BaseZ + Up;
}

bool AWRSwampFlood::InWater(const FVector& Feet) const
{
	return FVector::Dist2D(Feet, GetActorLocation()) <= FloodRadius && Feet.Z <= SurfaceZ() && Feet.Z >= FloorZ - 150.f;
}

bool AWRSwampFlood::InRisen(const FVector& Feet) const
{
	// Wading the drowned terraces (not swimming the pool): the risen water over them.
	return SurfaceZ() > BaseZ + 10.f && InWater(Feet) && Feet.Z > BaseZ - 150.f;
}

void AWRSwampFlood::StartFlood()
{
	if (!HasAuthority())
	{
		return;
	}
	RiseStartedAt = VhServerTime(this);
	DrainStartedAt = 0.f;
	ForceNetUpdate();
	WRFx::Broadcast(this, TEXT("vharra.floodrise"), FVector(GetActorLocation().X, GetActorLocation().Y, BaseZ), FRotator::ZeroRotator, FloodRadius / 1000.f, TEXT("swamp.vharra.flood"), RiseSeconds);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP the Throat floods (+%.0f cm over %.1f s)"), Rise, RiseSeconds);
}

void AWRSwampFlood::Drain()
{
	if (!HasAuthority() || !IsFlooded() || DrainStartedAt > 0.f)
	{
		return;
	}
	DrainStartedAt = VhServerTime(this);
	ForceNetUpdate();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP the Throat's flood drains"));
}

void AWRSwampFlood::SetKind(uint8 Kind)
{
	if (!HasAuthority() || WaterKind == Kind)
	{
		return;
	}
	WaterKind = Kind;
	OnRep_Look();
	ForceNetUpdate();
}

void AWRSwampFlood::Electrify(float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	ShockUntil = VhServerTime(this) + Seconds;
	ForceNetUpdate();
}

void AWRSwampFlood::OnRep_Shape()
{
	BuildBody();
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildSurface();
	}
	PlacedZ = -1.f;
	PlaceSurface();
}

void AWRSwampFlood::OnRep_Look()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildSurface();
	}
}

void AWRSwampFlood::BuildBody()
{
	// No brush model (that's editor geometry): the volume is a body of three boxes turned 0 / 60 / 120 degrees (a
	// hexagon round the pool), from under the pool's floor up to the surface (the actor's Z, which rises with it).
	UBrushComponent* Shape = GetBrushComponent();
	if (!Shape || PoolRadius <= 0.f)
	{
		return;
	}
	UBodySetup* Setup = NewObject<UBodySetup>(Shape);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	const float Top = 0.f;
	const float Bottom = FloorZ - 60.f - BaseZ - Rise;
	const float Tall = FMath::Max(100.f, Top - Bottom);
	const float Corner = PoolRadius / 0.8660254f;
	for (int32 k = 0; k < 3; ++k)
	{
		FKBoxElem Box(Corner, Corner * 1.7320508f, Tall);
		Box.Center = FVector(0.f, 0.f, (Top + Bottom) * 0.5f);
		Box.Rotation = FRotator(0.f, 60.f * k, 0.f);
		Setup->AggGeom.BoxElems.Add(Box);
	}
	Setup->CreatePhysicsMeshes();
	Shape->BrushBodySetup = Setup;
	Shape->RecreatePhysicsState();
	Shape->UpdateBounds();
	BuiltRadius = PoolRadius;
	bBodyOn = true;
}

void AWRSwampFlood::BuildSurface()
{
	using namespace WRLookKit;
	if (GetNetMode() == NM_DedicatedServer || FloodRadius <= 0.f)
	{
		return;
	}
	// The fixtures that stand still whatever the water does: the Coven's tithe basin round the Blood altar's foot at the
	// top of the Throat Stair, its channel running down toward the water (cook 2.4 #4, 2.20: the channels all run to her).
	bool bFixtures = false;
	for (const USceneComponent* C : SurfaceComps) bFixtures |= C && C->ComponentHasTag(TEXT("VhFixtures"));
	if (!bFixtures)
	{
		FVector AltarAt;
		float AltarYaw = 0.f;
		const TSharedPtr<FJsonObject> Row = WRSwamp::Arena(VhQueenId());
		if (VhAltarSpot(Row, AltarAt, AltarYaw))
		{
			bool bOk = false;
			const FVector G = VhLand(GetWorld(), AltarAt, bOk, 3000.f, 6000.f);
			if (bOk)
			{
				const FTransform AltarT(FRotator(0.f, AltarYaw, 0.f), G);
				TArray<FWRLookBlock> B;
				FKit K(AltarT, B);
				const FSurf Peat = FSurf::Stone(VhHex(TEXT("#2E241A")), 0.95f);
				const FSurf BoneS = FSurf::Stone(VhHex(TEXT("#CFC6B0")), 0.7f);
				const FSurf Stone = FSurf::Stone(VhHex(TEXT("#5A554D")), 0.88f);
				const FSurf Bronze = FSurf::Steel(VhHex(TEXT("#8C6A3A")), 0.5f);
				const FSurf Stain = FSurf::Glaze(VhHex(TEXT("#4A0A0F")), 0.35f);
				// A ring of packed peat and long bones round the pedestal's foot, knee high, its lip stained oxblood.
				for (int32 i = 0; i < 14; ++i)
				{
					const float A = 360.f * i / 14.f;
					const FVector P = FRotator(0.f, A, 0.f).Vector() * 125.f;
					K.Add(i % 2 ? Peat : Peat.Dim(0.85f), FVector(62.f, 34.f, 46.f), P + FVector(0.f, 0.f, 18.f), FRotator(0.f, A + 90.f, 0.f));
					K.Rod(BoneS.Dim(0.85f + 0.15f * K.Rand(P, 3)), P + FRotator(0.f, A + 90.f, 0.f).Vector() * 28.f + FVector(0.f, 0.f, 40.f), P - FRotator(0.f, A + 90.f, 0.f).Vector() * 28.f + FVector(0.f, 0.f, 44.f), 8.f);
					K.Add(Stain, FVector(40.f, 30.f, 3.f), P + FVector(0.f, 0.f, 44.f), FRotator(0.f, A + 90.f, 0.f));
				}
				// The tithe bowl beside it on three bone legs.
				const FVector Bowl(-40.f, 190.f, 0.f);
				for (int32 l = 0; l < 3; ++l)
				{
					const FVector Leg = Bowl + FRotator(0.f, 120.f * l, 0.f).Vector() * 30.f;
					K.Rod(BoneS, Leg, Bowl + FVector(0.f, 0.f, 70.f), 7.f);
				}
				K.Disc(Bronze, Bowl + FVector(0.f, 0.f, 78.f), FVector::UpVector, 76.f, 16.f);
				K.Disc(Stain, Bowl + FVector(0.f, 0.f, 86.f), FVector::UpVector, 60.f, 2.f);
				// The stone channel from the basin down toward the Throat (it falls away over the rim).
				for (int32 s = 0; s < 8; ++s)
				{
					const FVector P(160.f + 70.f * s, 0.f, 4.f - 14.f * s);
					K.Add(Stone.Dim(0.9f + 0.1f * (s % 2)), FVector(72.f, 40.f, 16.f), P, FRotator(-11.f, 0.f, 0.f));
					K.Add(Stain, FVector(70.f, 16.f, 3.f), P + FVector(0.f, 0.f, 8.f), FRotator(-11.f, 0.f, 0.f));
				}
				USceneComponent* Frame = NewObject<USceneComponent>(this);
				Frame->SetupAttachment(RootComponent);
				Frame->SetUsingAbsoluteLocation(true);
				Frame->SetUsingAbsoluteRotation(true);
				Frame->SetUsingAbsoluteScale(true);
				Frame->RegisterComponent();
				Frame->SetWorldLocationAndRotation(G, FRotator(0.f, AltarYaw, 0.f));
				Frame->ComponentTags.Add(TEXT("VhFixtures"));
				SurfaceComps.Add(Frame);
				FWRBuildLook::Spawn(this, Frame, B, FLinearColor::White, false);
				TArray<USceneComponent*> Kids;
				Frame->GetChildrenComponents(false, Kids);
				for (USceneComponent* C : Kids) SurfaceComps.Add(C);
			}
		}
	}
	if (BuiltKind == WaterKind && FMath::IsNearlyEqual(BuiltRadius, FloodRadius, 1.f) && SurfaceRoot)
	{
		return;
	}
	// The risen surface over the whole flooded floor (the resting water is the swamp's own): murk with duckweed scum and
	// floating bone, bleedwater on the Red Tide, blood in the hidden phase with clots and froth.
	if (SurfaceRoot)
	{
		TArray<USceneComponent*> Kids;
		SurfaceRoot->GetChildrenComponents(true, Kids);
		for (USceneComponent* C : Kids) if (C) { SurfaceComps.Remove(C); C->DestroyComponent(); }
		SurfaceComps.Remove(SurfaceRoot);
		SurfaceRoot->DestroyComponent();
		SurfaceRoot = nullptr;
	}
	BuiltKind = WaterKind;
	BuiltRadius = FloodRadius;
	const FLinearColor WaterC = WaterKind == 2 ? VhHex(TEXT("#7A0A14")) : WaterKind == 1 ? VhHex(TEXT("#4A0A0F")) : VhHex(TEXT("#12150E"));
	const FLinearColor ScumC = WaterKind == 2 ? VhHex(TEXT("#3A0508")) : WaterKind == 1 ? VhHex(TEXT("#6A1A18")) : VhHex(TEXT("#2B2E1C"));
	const FSurf Water = FSurf::Glaze(WaterC, WaterKind == 2 ? 0.12f : 0.06f);
	const FSurf Scum = FSurf::Stone(ScumC, 0.8f);
	const FSurf BoneS = FSurf::Stone(VhHex(TEXT("#BDB39C")), 0.75f);
	const FSurf Reed = FSurf::Wood(VhHex(TEXT("#5A4A2C")));
	const FSurf Clot = FSurf::Glaze(VhHex(TEXT("#2A0306")), 0.3f);
	TArray<FWRLookBlock> B;
	const FTransform SurfaceAt(GetActorLocation());
	FKit K(SurfaceAt, B);
	K.Disc(Water, FVector(0.f, 0.f, -3.f), FVector::UpVector, FloodRadius * 2.f, 6.f);
	const int32 Ring = 64;
	for (int32 i = 0; i < Ring; ++i)
	{
		const float A = 360.f * i / Ring;
		const FVector P = FRotator(0.f, A, 0.f).Vector() * (FloodRadius - 90.f);
		K.Add(Scum.Dim(0.85f + 0.15f * K.Rand(P, 1)), FVector(2.f * PI * FloodRadius / Ring * 1.15f, 140.f + 60.f * K.Rand(P, 2), 3.f), P, FRotator(0.f, A + 90.f, 0.f));
	}
	FRandomStream Rand(int32(FMath::RoundToInt(GetActorLocation().X * 0.01f) * 7919 + FMath::RoundToInt(GetActorLocation().Y * 0.01f)));
	for (int32 i = 0; i < 48; ++i)
	{
		const FVector P = FVector(Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f), 0.f).GetClampedToMaxSize(1.f) * (FloodRadius - 200.f);
		const float Yaw = Rand.FRandRange(0.f, 360.f);
		if (i % 3 == 0)
		{
			K.Rod(BoneS.Dim(0.8f + 0.2f * Rand.FRand()), P + FRotator(0.f, Yaw, 0.f).Vector() * 30.f, P - FRotator(0.f, Yaw, 0.f).Vector() * 30.f, 9.f);   // a long bone afloat
		}
		else if (i % 3 == 1)
		{
			K.Add(Reed, FVector(160.f + 120.f * Rand.FRand(), 5.f, 4.f), P, FRotator(0.f, Yaw, 0.f));   // a broken reed
		}
		else
		{
			K.Disc(WaterKind == 2 ? Clot : Scum, P + FVector(0.f, 0.f, 0.5f), FVector::UpVector, 60.f + 120.f * Rand.FRand(), 3.f);   // scum (clots in blood)
		}
	}
	SurfaceRoot = NewObject<USceneComponent>(this);
	SurfaceRoot->SetupAttachment(RootComponent);
	SurfaceRoot->RegisterComponent();
	SurfaceComps.Add(SurfaceRoot);
	FWRBuildLook::Spawn(this, SurfaceRoot, B, FLinearColor::White, false);
	TArray<USceneComponent*> Kids;
	SurfaceRoot->GetChildrenComponents(false, Kids);
	for (USceneComponent* C : Kids) SurfaceComps.Add(C);
	PlacedZ = -1.f;
	PlaceSurface();
}

void AWRSwampFlood::PlaceSurface()
{
	// The volume and its surface ride the water's height (every machine works it out from the replicated times).
	const float Z = SurfaceZ();
	if (FMath::Abs(Z - PlacedZ) > 0.5f)
	{
		PlacedZ = Z;
		const FVector Here = GetActorLocation();
		SetActorLocation(FVector(Here.X, Here.Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (SurfaceRoot)
	{
		const bool bShow = Z > BaseZ + 3.f;
		if (SurfaceRoot->IsVisible() != bShow) SurfaceRoot->SetVisibility(bShow, true);
	}
}

void AWRSwampFlood::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PlaceSurface();
	const float Now = VhServerTime(this);
	if (HasAuthority())
	{
		if (DrainStartedAt > 0.f && Now - DrainStartedAt >= VhT(TEXT("floodDrainSeconds"), 12.f))
		{
			RiseStartedAt = 0.f;
			DrainStartedAt = 0.f;
			ForceNetUpdate();
		}
		// Whoever wades into it while it's still live is shocked too (once a window).
		if (Now < ShockUntil)
		{
			const FWRVharraWater* W = VhWaters().Find(this);
			VhShockPeople(this, W ? W->By.Get() : nullptr);
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Surface = SurfaceZ();
	const FVector Mid(GetActorLocation().X, GetActorLocation().Y, Surface + 3.f);
	// Electrified: blue-white arcs race across the surface in jagged runs.
	if (Now < ShockUntil)
	{
		ArcClock += DeltaSeconds;
		if (ArcClock >= 0.05f)
		{
			ArcClock = 0.f;
			for (int32 k = 0; k < 4; ++k)
			{
				FVector From = Mid + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(0.f, FloodRadius * 0.95f);
				const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
				for (int32 s = 0; s < 3; ++s)
				{
					FVector Next = From + Dir.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector) * FMath::FRandRange(250.f, 500.f);
					if (FVector::Dist2D(Next, Mid) > FloodRadius) break;
					Next.Z = Mid.Z;
					const FVector D = Next - From;
					WRFx::SpawnAt(this, TEXT("vharra.waterarc"), From, D.Rotation(), D.Size() / 100.f);
					From = Next;
				}
			}
		}
	}
	// Risen: rings spread where something moves under it.
	if (Surface > BaseZ + 5.f)
	{
		RippleClock += DeltaSeconds;
		if (RippleClock >= 0.45f)
		{
			RippleClock = 0.f;
			const FVector P = Mid + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(0.f, FloodRadius * 0.9f);
			WRFx::SpawnAt(this, TEXT("swamp.ripple"), P, FRotator::ZeroRotator, 6.f);
		}
	}
}

// ---------------------------------------------------------------- her look (every machine that draws)

namespace WRSwampVharraLocal
{
	/** One Vharra's built look on this machine: the hood plates and how they open, the sacs, the trunk's rings. */
	struct FWRVharraLook
	{
		TWeakObjectPtr<USceneComponent> Plates[3];
		FQuat PlateBase[3] = { FQuat::Identity, FQuat::Identity, FQuat::Identity };
		FVector PlateAxis[3] = { FVector::YAxisVector, FVector::YAxisVector, FVector::YAxisVector };
		TWeakObjectPtr<USceneComponent> Sacs[3], Torn[3];
		FVector SacScale[3] = { FVector::OneVector, FVector::OneVector, FVector::OneVector };
		TArray<float> SegDiam;
		float SegLen = 130.f, Scale = 1.f;
		FVector MeshBaseLoc = FVector::ZeroVector;
		FRotator MeshBaseRot = FRotator::ZeroRotator;
		bool bBase = false, bPosed = false;
		TArray<FVector> Trail;
		TArray<float> GroundZ;
		double NextGroundAt = 0.0, NextFxAt = 0.0, NextWakeAt = 0.0;
		float Jaw = 0.f, Rise = 0.f, Swim = 0.f, Conv = 0.f, LashDeg = 0.f, LashLift = 0.f;
		float SacGlow[3] = { -1.f, -1.f, -1.f };
	};

	TMap<TWeakObjectPtr<const UWRSwampBossBody>, FWRVharraLook>& VhLooks()
	{
		static TMap<TWeakObjectPtr<const UWRSwampBossBody>, FWRVharraLook> Map;
		return Map;
	}

	/** Every instanced block under a frame: its glow (custom data 8, as the build kit's tinted blocks read it). */
	void VhSetGlow(USceneComponent* Frame, float Glow)
	{
		if (!Frame) return;
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(true, Kids);
		for (USceneComponent* K : Kids)
		{
			UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(K);
			if (!ISM || ISM->NumCustomDataFloats < 10) continue;
			for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, Glow, i == ISM->GetInstanceCount() - 1);
		}
	}

	/** The body's surface water here on this machine (the Throat's flood if it's near, else the swamp's own water). */
	float VhLocalWater(const AActor* A)
	{
		if (const AWRSwampFlood* Flood = A ? VhFlood(A->GetWorld(), A->GetActorLocation(), 9000.f) : nullptr) return Flood->SurfaceZ();
		return FWRSwampData::Get().bLoaded ? FWRSwampData::Get().SeaZ : 60.f;
	}

	/** A frame of blocks on her: attached to Bone (it follows the animation), placed in the world now; its parts go into
	 *  the body's look list (cleared with the rest). AbsScale: its size never follows the bone's. */
	USceneComponent* VhFrame(UWRSwampBossBody* Body, AWRCreature* C, USkeletalMeshComponent* Skel, FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks)
	{
		if (!Body || !C || Blocks.Num() == 0) return nullptr;
		USceneComponent* Frame = NewObject<USceneComponent>(C);
		Frame->RegisterComponent();
		if (Skel)
		{
			Frame->AttachToComponent(Skel, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
		}
		else
		{
			Frame->AttachToComponent(C->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
		Frame->SetUsingAbsoluteScale(true);
		Frame->SetWorldScale3D(FVector(1.f));
		Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
		Body->LookComps.Add(Frame);
		FWRBuildLook::Spawn(C, Frame, Blocks, FLinearColor::White, false);
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(false, Kids);
		for (USceneComponent* K : Kids) Body->LookComps.Add(K);
		return Frame;
	}

	/** Bones whose names hold Hint, the top of each such chain only (its parent doesn't hold it). */
	TArray<FName> VhChainTops(const USkeletalMeshComponent* Skel, const TCHAR* Hint, const TCHAR* Not = nullptr)
	{
		TArray<FName> Out;
		const USkeletalMesh* Mesh = Skel ? Skel->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh) return Out;
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (!N.Contains(Hint) || (Not && N.Contains(Not))) continue;
			const int32 Parent = Ref.GetParentIndex(i);
			if (Parent != INDEX_NONE && Ref.GetBoneName(Parent).ToString().ToLower().Contains(Hint)) continue;
			Out.Add(Ref.GetBoneName(i));
		}
		return Out;
	}
}

void FWRSwampBoss::VharraBuildLook(UWRSwampBossBody* Body)
{
	using namespace WRLookKit;
	AWRCreature* C = Body ? Cast<AWRCreature>(Body->GetOwner()) : nullptr;
	if (!C)
	{
		return;
	}
	if (C->GetNetMode() == NM_DedicatedServer || C->CreatureId != VhQueenId())
	{
		Body->LookFor = C->CreatureId;   // her brood wear the swamp creatures' leech look; a server draws nothing
		return;
	}
	USkeletalMeshComponent* Skel = C->GetCreatureSkel();
	const FWRCreatureDef* Def = C->GetDef();
	if (!Skel || !Skel->GetSkeletalMeshAsset() || !Def || !Skel->IsVisible())
	{
		++Body->LookTries;
		return;
	}
	// From scratch (a rebuild after the rig changed).
	for (USceneComponent* Comp : Body->LookComps) if (Comp) Comp->DestroyComponent();
	Body->LookComps.Reset();
	for (USceneComponent* Comp : Body->Trunk) if (Comp) Comp->DestroyComponent();
	Body->Trunk.Reset();
	Body->TrunkPts.Reset();
	Body->TintMats.Reset();
	VhLooks().Remove(Body);
	FWRVharraLook& L = VhLooks().Add(Body);
	Body->LookFor = C->CreatureId;

	// Her palette over the rig's own texture (M_CreatureTint): glossy black-olive, the light areas rust (the leech's
	// stripes), no glow.
	const TSharedPtr<FJsonObject> Row = VhRow(C->CreatureId);
	const TSharedPtr<FJsonObject>* LookObj = nullptr;
	const TSharedPtr<FJsonObject> Look = Row.IsValid() && Row->TryGetObjectField(TEXT("look"), LookObj) ? *LookObj : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (Look.IsValid() && Master && Look->TryGetArrayField(TEXT("tint"), Tints) && Tints->Num() > 0)
	{
		const TSharedPtr<FJsonObject> Pick = (*Tints)[0]->AsObject();
		for (int32 Slot = 0; Pick.IsValid() && Slot < Skel->GetNumMaterials(); ++Slot)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
			FString Path;
			const auto Tex = [&](const TCHAR* Field, const TCHAR* Param, const TCHAR* Fallback)
			{
				const FString P = Pick->TryGetStringField(Field, Path) ? Path : FString(Fallback);
				if (UTexture* Found = P.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *P)) M->SetTextureParameterValue(Param, Found);
			};
			Tex(TEXT("tex"), TEXT("BaseColorTex"), TEXT(""));
			Tex(TEXT("normal"), TEXT("NormalTex"), TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
			Tex(TEXT("glowTex"), TEXT("GlowTex"), TEXT("/Engine/EngineResources/Black.Black"));
			M->SetScalarParameterValue(TEXT("Desaturation"), float(VhNum(Pick, TEXT("desat"), 1.0)));
			M->SetScalarParameterValue(TEXT("Brightness"), float(VhNum(Pick, TEXT("bright"), 1.0)));
			M->SetScalarParameterValue(TEXT("TwoTone"), float(VhNum(Pick, TEXT("twoTone"), 0.0)));
			M->SetScalarParameterValue(TEXT("LightFrom"), float(VhNum(Pick, TEXT("lightFrom"), 0.0)));
			M->SetScalarParameterValue(TEXT("Roughness"), float(VhNum(Pick, TEXT("rough"), 0.75)));
			M->SetScalarParameterValue(TEXT("GlowStrength"), float(VhNum(Pick, TEXT("glowStrength"), 0.0)));
			M->SetVectorParameterValue(TEXT("Tint"), VhColor(Pick, TEXT("tint"), FLinearColor::White));
			M->SetVectorParameterValue(TEXT("TintLight"), VhColor(Pick, TEXT("light"), VhColor(Pick, TEXT("tint"), FLinearColor::White)));
			M->SetVectorParameterValue(TEXT("GlowColor"), VhColor(Pick, TEXT("glow"), VhHex(TEXT("#7A0A14"))));
			Skel->SetMaterial(Slot, M);
			Body->TintMats.Add(M);
		}
	}

	// The drake front only: no hind legs, no tail, no wings (the body becomes the trunk), and the rig's own head gives way
	// to the three-plate hood that is her skull.
	USkeletalMeshComponent* SkelMut = Skel;
	FName HeadBone = C->FindBone(TEXT("Head"));
	const USkeletalMesh* Mesh = Skel->GetSkeletalMeshAsset();
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 HeadIndex = HeadBone.IsNone() ? INDEX_NONE : Ref.FindBoneIndex(HeadBone);
	const FName NeckEnd = HeadIndex != INDEX_NONE && Ref.GetParentIndex(HeadIndex) != INDEX_NONE ? Ref.GetBoneName(Ref.GetParentIndex(HeadIndex)) : C->FindBone(TEXT("Neck"));
	for (const TCHAR* Hint : { TEXT("thigh"), TEXT("tail"), TEXT("wing") })
	{
		for (const FName B : VhChainTops(Skel, Hint)) SkelMut->HideBoneByName(B, EPhysBodyOp::PBO_None);
	}
	if (!HeadBone.IsNone() && !NeckEnd.IsNone()) SkelMut->HideBoneByName(HeadBone, EPhysBodyOp::PBO_None);

	L.MeshBaseLoc = Skel->GetRelativeLocation();
	L.MeshBaseRot = Skel->GetRelativeRotation();
	L.bBase = true;

	const float R = Def->Radius, HH = Def->HalfHeight;
	const float S = R / 450.f;
	L.Scale = S;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Up = FVector::UpVector;
	const FTransform W = FTransform::Identity;

	const FSurf Skin = FSurf::Glaze(VhHex(TEXT("#1C2014")), 0.22f);
	const FSurf SkinDark = FSurf::Glaze(VhHex(TEXT("#12150D")), 0.25f);
	const FSurf Belly = FSurf::Glaze(VhHex(TEXT("#2C2C1E")), 0.3f);
	const FSurf Stripe = FSurf::Glaze(VhHex(TEXT("#8A3B1E")), 0.3f);
	const FSurf Oxblood = FSurf::Glaze(VhHex(TEXT("#4A0A0F")), 0.25f);
	const FSurf Garnet = FSurf::Glaze(VhHex(TEXT("#7A0A14")), 0.18f);
	const FSurf Tooth = FSurf::Stone(VhHex(TEXT("#D8D0BC")), 0.4f);
	const FSurf Horn = FSurf::Stone(VhHex(TEXT("#1A1714")), 0.5f);
	const FSurf Eye = FSurf::Lit(VhHex(TEXT("#7A0A14")), 1.6f);

	// ---- The hood: three plates round the head's axis (top, lower left, lower right), a drake's skull closed and a
	// radial three-jawed mouth open, each plate lined with rows of leech teeth along its seams; ten eyes in five pairs
	// on the top plate's ridge; the gullet and its ring of hooked teeth behind them.
	FVector HeadPos = HeadBone.IsNone() ? C->GetActorLocation() + Fwd * R * 1.2f + Up * HH * 0.5f : Skel->GetBoneLocation(HeadBone);
	const FVector NeckPos = NeckEnd.IsNone() ? HeadPos - Fwd * R * 0.4f : Skel->GetBoneLocation(NeckEnd);
	FVector Axis = (HeadPos - NeckPos).GetSafeNormal();
	if (Axis.IsNearlyZero() || FVector::DotProduct(Axis, Fwd) < 0.2f) Axis = Fwd;
	Axis = (Axis + Up * -0.15f).GetSafeNormal();   // the skull hangs a little nose-down from the neck
	const FRotator HoodRot = FRotationMatrix::MakeFromXZ(Axis, Up).Rotator();
	const float Lh = 330.f * S, Rb = 78.f * S, Rf = 24.f * S;
	const FVector Hinge = HeadPos - Axis * Lh * 0.12f;
	const FName HoodBone = NeckEnd.IsNone() ? HeadBone : NeckEnd;
	const float OpenAngles[3] = { 90.f, 210.f, 330.f };
	for (int32 k = 0; k < 3; ++k)
	{
		const float Ak = OpenAngles[k];
		const FVector Rk(0.f, FMath::Cos(FMath::DegreesToRadians(Ak)), FMath::Sin(FMath::DegreesToRadians(Ak)));
		const FVector HingeLocal = Rk * Rb * 0.9f;
		TArray<FWRLookBlock> B;
		FKit K(W, B);
		const int32 Stations = 6;
		for (int32 s = 0; s < Stations; ++s)
		{
			const float X0 = Lh * s / Stations, X1 = Lh * (s + 1) / Stations;
			const float Xm = (X0 + X1) * 0.5f;
			const float Rad = FMath::Lerp(Rb, Rf, Xm / Lh) * (s == 0 ? 1.08f : 1.f);
			for (const float Da : { -40.f, 0.f, 40.f })
			{
				const float A = FMath::DegreesToRadians(Ak + Da);
				const FVector Radial(0.f, FMath::Cos(A), FMath::Sin(A));
				const FVector At = FVector(Xm, 0.f, 0.f) + Radial * Rad - HingeLocal;
				const float Chord = 2.f * Rad * FMath::Sin(FMath::DegreesToRadians(20.f)) * 1.18f;
				const FRotator Rot = FRotationMatrix::MakeFromXZ(FVector::XAxisVector, Radial).Rotator();
				K.Add(s % 2 ? Skin : SkinDark, FVector((X1 - X0) * 1.12f, Chord, Rb * 0.14f), At, Rot);
				// The lining inside: wet oxblood flesh (you see it when she opens).
				K.Add(Oxblood, FVector((X1 - X0) * 1.1f, Chord * 0.95f, Rb * 0.05f), At - Radial * Rb * 0.1f, Rot);
			}
			// A rust stripe pair down the middle of each plate (the leech's pattern carries onto the hood).
			const float A0 = FMath::DegreesToRadians(Ak);
			const FVector Mid = FVector(Xm, 0.f, 0.f) + FVector(0.f, FMath::Cos(A0), FMath::Sin(A0)) * (Rad + Rb * 0.07f) - HingeLocal;
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Side = FVector(0.f, -FMath::Sin(A0), FMath::Cos(A0)) * Sd * Rad * 0.22f;
				K.Add(Stripe, FVector((X1 - X0) * 1.05f, Rb * 0.07f, Rb * 0.03f), Mid + Side, FRotationMatrix::MakeFromXZ(FVector::XAxisVector, FVector(0.f, FMath::Cos(A0), FMath::Sin(A0))).Rotator());
			}
			// Leech teeth along both seams, pointing in at the axis (rows of them: three jaws' worth when it opens).
			if (s >= 1)
			{
				for (const float Seam : { -55.f, 55.f })
				{
					const float As = FMath::DegreesToRadians(Ak + Seam);
					const FVector Radial(0.f, FMath::Cos(As), FMath::Sin(As));
					for (int32 t = 0; t < 2; ++t)
					{
						const float Xt = FMath::Lerp(X0, X1, 0.25f + 0.5f * t);
						const float Rt = FMath::Lerp(Rb, Rf, Xt / Lh) * 0.9f;
						K.Spike(Tooth, FVector(Xt, 0.f, 0.f) + Radial * Rt - HingeLocal, (-Radial + FVector(0.25f, 0.f, 0.f)).GetSafeNormal(), Rb * 0.12f, Rt * 0.42f);
					}
				}
			}
		}
		if (k == 0)
		{
			// The top plate: the ridge, ten small dull-red eyes in five pairs along it, two horns swept back.
			for (int32 e = 0; e < 5; ++e)
			{
				const float Xe = Lh * (0.22f + 0.09f * e);
				const float Re = FMath::Lerp(Rb, Rf, Xe / Lh);
				for (const float Sd : { -1.f, 1.f })
				{
					const FVector Dir = FVector(0.f, Sd * 0.42f, 1.f).GetSafeNormal();
					K.Disc(Eye, FVector(Xe, 0.f, 0.f) + Dir * (Re * 1.04f + Rb * 0.04f) - HingeLocal, Dir, Rb * 0.13f, Rb * 0.05f);
				}
			}
			for (int32 r = 0; r < Stations; ++r)
			{
				const float Xr = Lh * (r + 0.5f) / Stations;
				K.Spike(Horn, FVector(Xr, 0.f, FMath::Lerp(Rb, Rf, Xr / Lh) * 1.06f) - HingeLocal, FVector(-0.4f, 0.f, 1.f).GetSafeNormal(), Rb * 0.16f, Rb * (0.3f - 0.03f * r));
			}
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Base = FVector(Lh * 0.05f, Sd * Rb * 0.55f, Rb * 0.8f) - HingeLocal;
				K.Spike(Horn, Base, FVector(-1.f, Sd * 0.35f, 0.45f).GetSafeNormal(), Rb * 0.3f, Lh * 0.42f);
			}
		}
		const FVector WorldHinge = Hinge + HoodRot.RotateVector(HingeLocal);
		if (USceneComponent* Plate = VhFrame(Body, C, Skel, HoodBone, WorldHinge, HoodRot, B))
		{
			L.Plates[k] = Plate;
			L.PlateBase[k] = Plate->GetRelativeRotation().Quaternion();
			// Opening rotates the plate's tip out along its own radial (the axis X x radial).
			L.PlateAxis[k] = FVector(0.f, -Rk.Z, Rk.Y).GetSafeNormal();
		}
	}
	{
		// The gullet: a dark oxblood throat behind the jaws with a ring of hooked teeth at its mouth.
		TArray<FWRLookBlock> B;
		FKit K(W, B);
		K.Rod(Oxblood.Dim(0.6f), FVector(-Lh * 0.15f, 0.f, 0.f), FVector(Lh * 0.5f, 0.f, 0.f), Rb * 1.3f);
		K.Disc(FSurf::Glaze(VhHex(TEXT("#160204")), 0.4f), FVector(Lh * 0.5f, 0.f, 0.f), FVector::XAxisVector, Rb * 1.1f, Rb * 0.05f);
		for (int32 t = 0; t < 12; ++t)
		{
			const float A = 2.f * PI * t / 12.f;
			const FVector Radial(0.f, FMath::Cos(A), FMath::Sin(A));
			K.Spike(Tooth, FVector(Lh * 0.48f, 0.f, 0.f) + Radial * Rb * 0.62f, (-Radial + FVector(0.6f, 0.f, 0.f)).GetSafeNormal(), Rb * 0.1f, Rb * 0.3f);
		}
		VhFrame(Body, C, Skel, HoodBone, Hinge, HoodRot, B);
	}

	// ---- Rust stripes down the neck (paired, the medicinal leech's), riding the neck bones.
	{
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FName N = Ref.GetBoneName(i);
			if (!N.ToString().ToLower().Contains(TEXT("neck")) || N == HeadBone) continue;
			const FVector P = Skel->GetBoneLocation(N);
			FVector Next = HeadPos;
			for (int32 j = 0; j < Ref.GetNum(); ++j) if (Ref.GetParentIndex(j) == i) { Next = Skel->GetBoneLocation(Ref.GetBoneName(j)); break; }
			const FVector D = Next - P;
			const float Len = FMath::Max(30.f, float(D.Size()));
			const FRotator Rot = FRotationMatrix::MakeFromXZ(D.GetSafeNormal(), Up).Rotator();
			TArray<FWRLookBlock> B;
			FKit K(W, B);
			for (const float Sd : { -1.f, 1.f })
			{
				K.Add(Stripe, FVector(Len * 1.05f, R * 0.05f, R * 0.025f), FVector(Len * 0.5f, Sd * R * 0.11f, R * 0.3f));
			}
			// A ridge of low spines down the nape.
			K.Spike(Horn, FVector(Len * 0.5f, 0.f, R * 0.31f), FVector(-0.5f, 0.f, 1.f).GetSafeNormal(), R * 0.06f, R * 0.12f);
			VhFrame(Body, C, Skel, N, P, Rot, B);
		}
	}

	// ---- Hooked claws on the webbed forefeet.
	for (const TCHAR* Hand : { TEXT("L-Hand"), TEXT("R-Hand") })
	{
		const FName HB = C->FindBone(Hand);
		if (HB.IsNone()) continue;
		const FVector P = Skel->GetBoneLocation(HB);
		TArray<FWRLookBlock> B;
		FKit K(W, B);
		for (int32 f = 0; f < 3; ++f)
		{
			const FVector Toe(R * 0.12f, (f - 1) * R * 0.07f, -R * 0.05f);
			K.Rod(Horn, Toe, Toe + FVector(R * 0.1f, (f - 1) * R * 0.02f, -R * 0.02f), R * 0.035f);
			K.Spike(Horn, Toe + FVector(R * 0.1f, (f - 1) * R * 0.02f, -R * 0.02f), FVector(0.4f, 0.f, -1.f).GetSafeNormal(), R * 0.03f, R * 0.09f);
		}
		// The web between the toes.
		K.Add(SkinDark, FVector(R * 0.14f, R * 0.16f, R * 0.01f), FVector(R * 0.16f, 0.f, -R * 0.055f));
		VhFrame(Body, C, Skel, HB, P, Yaw, B);
	}

	// ---- The three blood sacs (crop chambers showing through the skin of her flanks), where her lock points are; a torn
	// version of each for after it bursts.
	const FName SpineBone = C->FindBone(TEXT("Spine"));
	for (int32 i = 0; i < 3; ++i)
	{
		FVector At = i == 0 ? FVector(0.15f, -0.9f, 0.05f) : i == 1 ? FVector(0.15f, 0.9f, 0.05f) : FVector(-0.55f, 0.f, 0.6f);
		const FName Want(*FString::Printf(TEXT("sac_%d"), i));
		for (const FWRCreatureDef::FBodyPoint& P : Def->LockPoints) if (P.Id == Want) At = P.At;
		const FVector World = C->GetActorLocation() + Fwd * At.X * R + Right * At.Y * R + Up * At.Z * HH;
		const FVector Out = (At.Z > 0.4f ? Up : (Right * FMath::Sign(At.Y))).GetSafeNormal();
		const FRotator Rot = FRotationMatrix::MakeFromXZ(Fwd, Out).Rotator();
		const float Len = 150.f * S, Wide = 105.f * S;
		TArray<FWRLookBlock> B;
		FKit K(W, B);
		// An egg of stacked rounds, the skin over it thin and garnet with the blood in it.
		for (int32 d = 0; d < 7; ++d)
		{
			const float T = (d + 0.5f) / 7.f;
			const float Dm = Wide * FMath::Sin(T * PI) * (0.65f + 0.35f * FMath::Sin(T * PI));
			K.Disc(Garnet, FVector((T - 0.5f) * Len, 0.f, Wide * 0.12f), FVector::XAxisVector, FMath::Max(Wide * 0.2f, Dm), Len / 7.f * 1.25f);
		}
		// Veins over it, and the oxblood membrane where it meets the flank.
		for (int32 v = 0; v < 4; ++v)
		{
			const float A = PI * (0.2f + 0.2f * v);
			K.Rod(Stripe.Dim(0.7f), FVector(-Len * 0.42f, FMath::Cos(A) * Wide * 0.35f, Wide * 0.12f + FMath::Sin(A) * Wide * 0.35f), FVector(Len * 0.4f, FMath::Cos(A + 0.3f) * Wide * 0.3f, Wide * 0.12f + FMath::Sin(A + 0.3f) * Wide * 0.32f), Wide * 0.03f);
		}
		K.Disc(Oxblood, FVector(0.f, 0.f, -Wide * 0.02f), FVector::UpVector, Len * 1.05f, Wide * 0.06f);
		L.Sacs[i] = VhFrame(Body, C, Skel, SpineBone, World, Rot, B);
		// Torn: the skin split in flaps round a dark wound.
		TArray<FWRLookBlock> TB;
		FKit TK(W, TB);
		TK.Disc(FSurf::Glaze(VhHex(TEXT("#160204")), 0.35f), FVector(0.f, 0.f, Wide * 0.02f), FVector::UpVector, Len * 0.7f, Wide * 0.05f);
		for (int32 f = 0; f < 6; ++f)
		{
			const float A = 2.f * PI * f / 6.f;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			TK.Add(f % 2 ? Oxblood : Garnet.Dim(0.6f), FVector(Len * 0.28f, Wide * 0.22f, Wide * 0.04f), Dir * Len * 0.36f + FVector(0.f, 0.f, Wide * 0.12f), FRotator(35.f, FMath::RadiansToDegrees(A), 0.f));
		}
		L.Torn[i] = VhFrame(Body, C, Skel, SpineBone, World, Rot, TB);
		if (L.Torn[i].IsValid()) L.Torn[i]->SetVisibility(false, true);
	}

	// ---- The trunk: 14 ringed segments tapering 18 m from her hips to the 3 m sucker disc (placed every frame along the
	// curve to the wall, or trailing her when free: UWRSwampBossBody::PlaceTrunk).
	const int32 Segs = FMath::Clamp(FMath::RoundToInt(VhT(TEXT("trunkSegments"), 14.f)), 4, 24);
	L.SegLen = VhT(TEXT("trunkLength"), 1800.f) * S / Segs;
	L.SegDiam.Reset();
	for (int32 i = 0; i < Segs; ++i)
	{
		const float D = FMath::Lerp(VhT(TEXT("trunkHipDiam"), 300.f), VhT(TEXT("trunkTipDiam"), 110.f), float(i) / FMath::Max(1, Segs - 1)) * S;
		L.SegDiam.Add(D);
		TArray<FWRLookBlock> B;
		FKit K(W, B);
		const float Len = L.SegLen;
		K.Rod(i % 2 ? Skin : SkinDark, FVector(0.f, 0.f, 0.f), FVector(Len * 1.1f, 0.f, 0.f), D);
		// The annuli: rings round each segment.
		K.Disc(SkinDark, FVector(Len * 0.08f, 0.f, 0.f), FVector::XAxisVector, D * 1.06f, Len * 0.1f);
		K.Disc(Skin, FVector(Len * 0.55f, 0.f, 0.f), FVector::XAxisVector, D * 1.07f, Len * 0.16f);
		// The paired rust stripes on the back, the paler belly.
		for (const float Sd : { -1.f, 1.f })
		{
			K.Add(Stripe, FVector(Len * 1.02f, D * 0.08f, D * 0.05f), FVector(Len * 0.55f, Sd * D * 0.17f, D * 0.48f));
		}
		K.Add(Belly, FVector(Len * 1.02f, D * 0.5f, D * 0.06f), FVector(Len * 0.55f, 0.f, -D * 0.47f));
		if (i == 0)
		{
			// The join at her hips: a thick collar where the drake's body becomes the leech.
			K.Disc(SkinDark, FVector(-Len * 0.1f, 0.f, 0.f), FVector::XAxisVector, D * 1.25f, Len * 0.35f);
		}
		USceneComponent* Frame = NewObject<USceneComponent>(C);
		Frame->SetupAttachment(C->GetRootComponent());
		Frame->SetUsingAbsoluteLocation(true);
		Frame->SetUsingAbsoluteRotation(true);
		Frame->SetUsingAbsoluteScale(true);
		Frame->RegisterComponent();
		Frame->SetWorldLocationAndRotation(C->GetActorLocation() - Fwd * (R * 0.6f + Len * i), Yaw + FRotator(0.f, 180.f, 0.f));
		Body->Trunk.Add(Frame);
		FWRBuildLook::Spawn(C, Frame, B, FLinearColor::White, false);
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(false, Kids);
		for (USceneComponent* Kd : Kids) Body->LookComps.Add(Kd);
	}
	{
		// The posterior sucker disc, 3 m across: its face (local +X) pressed to the wall, a ring of hooked teeth round its
		// rim, radial muscle under the skin, the dark pit at its centre.
		const float Dd = VhT(TEXT("discDiam"), 300.f) * S;
		TArray<FWRLookBlock> B;
		FKit K(W, B);
		K.Disc(SkinDark, FVector(-Dd * 0.12f, 0.f, 0.f), FVector::XAxisVector, Dd * 0.75f, Dd * 0.22f);
		K.Disc(Skin, FVector(0.f, 0.f, 0.f), FVector::XAxisVector, Dd, Dd * 0.12f);
		K.Disc(Oxblood, FVector(Dd * 0.065f, 0.f, 0.f), FVector::XAxisVector, Dd * 0.82f, Dd * 0.02f);
		K.Disc(FSurf::Glaze(VhHex(TEXT("#160204")), 0.4f), FVector(Dd * 0.08f, 0.f, 0.f), FVector::XAxisVector, Dd * 0.22f, Dd * 0.02f);
		for (int32 m = 0; m < 10; ++m)
		{
			const float A = 2.f * PI * m / 10.f;
			const FVector Radial(0.f, FMath::Cos(A), FMath::Sin(A));
			K.Add(SkinDark, FVector(Dd * 0.03f, Dd * 0.05f, Dd * 0.3f), FVector(Dd * 0.07f, 0.f, 0.f) + Radial * Dd * 0.27f, FRotationMatrix::MakeFromXZ(FVector::XAxisVector, Radial).Rotator());
		}
		for (int32 t = 0; t < 18; ++t)
		{
			const float A = 2.f * PI * t / 18.f;
			const FVector Radial(0.f, FMath::Cos(A), FMath::Sin(A));
			K.Spike(Tooth, FVector(Dd * 0.06f, 0.f, 0.f) + Radial * Dd * 0.45f, (FVector(1.f, 0.f, 0.f) - Radial * 0.6f).GetSafeNormal(), Dd * 0.035f, Dd * 0.09f);
		}
		USceneComponent* Frame = NewObject<USceneComponent>(C);
		Frame->SetupAttachment(C->GetRootComponent());
		Frame->SetUsingAbsoluteLocation(true);
		Frame->SetUsingAbsoluteRotation(true);
		Frame->SetUsingAbsoluteScale(true);
		Frame->RegisterComponent();
		Frame->SetWorldLocationAndRotation(Body->Anchor.IsZero() ? C->GetActorLocation() - Fwd * 2200.f * S : FVector(Body->Anchor), Yaw + FRotator(0.f, 180.f, 0.f));
		Body->Trunk.Add(Frame);
		FWRBuildLook::Spawn(C, Frame, B, FLinearColor::White, false);
		TArray<USceneComponent*> Kids;
		Frame->GetChildrenComponents(false, Kids);
		for (USceneComponent* Kd : Kids) Body->LookComps.Add(Kd);
	}
	L.Trail.Reset();
	L.GroundZ.Reset();
	Body->PlaceTrunk(0.f);
	UE_LOG(LogWildreach, Log, TEXT("LOOK Vharra built: hood, %d trunk rings, disc, sacs"), Segs);
}

void FWRSwampBoss::VharraTickLook(UWRSwampBossBody* Body, float DeltaSeconds)
{
	AWRCreature* C = Body ? Cast<AWRCreature>(Body->GetOwner()) : nullptr;
	if (!C || C->GetNetMode() == NM_DedicatedServer || C->CreatureId != VhQueenId())
	{
		return;
	}
	FWRVharraLook* L = VhLooks().Find(Body);
	if (!L)
	{
		// Not built yet (the rig wasn't in when the body asked): try again every half second until it is.
		Body->FxClock += DeltaSeconds;
		if (Body->FxClock >= 0.5f)
		{
			Body->FxClock = 0.f;
			VharraBuildLook(Body);
		}
		return;
	}
	Body->Clock += DeltaSeconds;
	const float Clk = Body->Clock;
	const double Now = C->GetWorld()->GetTimeSeconds();
	L->Jaw = FMath::FInterpTo(L->Jaw, Body->Has(WRSwampBossFlag::JawOpen) ? 1.f : 0.f, DeltaSeconds, 9.f);
	L->Rise = FMath::FInterpTo(L->Rise, Body->Has(WRSwampBossFlag::Rising) ? 1.f : 0.f, DeltaSeconds, 3.5f);
	L->Swim = FMath::FInterpTo(L->Swim, Body->Has(WRSwampBossFlag::Swimming) ? 1.f : 0.f, DeltaSeconds, 2.5f);
	L->Conv = FMath::FInterpTo(L->Conv, Body->Has(WRSwampBossFlag::Convulsing) ? 1.f : 0.f, DeltaSeconds, 8.f);
	Body->JawAlpha = L->Jaw;
	Body->RearAlpha = L->Rise;

	// The hood: three plates part along their seams (the three-part wet crack you learn to fear).
	const float Open = VhT(TEXT("jawOpen"), 38.f) * L->Jaw + 4.f * L->Conv * FMath::Sin(Clk * 37.f);
	for (int32 k = 0; k < 3; ++k)
	{
		if (USceneComponent* Plate = L->Plates[k].Get())
		{
			Plate->SetRelativeRotation(L->PlateBase[k] * FQuat(L->PlateAxis[k], FMath::DegreesToRadians(Open)));
		}
	}

	// The body: reared to full height (the Sinking's tell, the ambush rise), sunk under the flood when she swims, an
	// eel's thrash when the water's live under her.
	USkeletalMeshComponent* Skel = C->GetCreatureSkel();
	if (Skel && L->bBase)
	{
		if (C->IsAlive())
		{
			const float R = C->GetSimpleCollisionRadius(), HH = C->GetSimpleCollisionHalfHeight();
			const float Pitch = VhT(TEXT("risePitch"), 38.f) * L->Rise;   // nose up, pivoting on her hips
			const float Thrash = VhT(TEXT("convulseShake"), 9.f) * L->Conv;
			const FQuat Q(FRotator(Pitch + Thrash * 0.4f * FMath::Sin(Clk * 29.f), Thrash * FMath::Sin(Clk * 23.f), Thrash * FMath::Sin(Clk * 41.f)));
			const FVector Pivot(-R * 0.6f, 0.f, -HH);
			const FVector Lift(0.f, 0.f, VhT(TEXT("riseLift"), 300.f) * L->Scale * L->Rise - VhT(TEXT("swimSink"), 1.1f) * HH * L->Swim);
			Skel->SetRelativeLocationAndRotation(Pivot + Q.RotateVector(L->MeshBaseLoc - Pivot) + Lift, Q * FQuat(L->MeshBaseRot));
			L->bPosed = true;
		}
		else if (L->bPosed)
		{
			Skel->SetRelativeLocationAndRotation(L->MeshBaseLoc, L->MeshBaseRot);
			L->bPosed = false;
		}
	}

	// The sacs fill visibly garnet as she drains (brighter, swollen); they flare in the Sinking's tell; a burst one is
	// torn open.
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bBurst = (Body->Burst & (1 << i)) != 0;
		USceneComponent* Sac = L->Sacs[i].Get();
		USceneComponent* Torn = L->Torn[i].Get();
		if (Sac && Sac->IsVisible() == bBurst) Sac->SetVisibility(!bBurst, true);
		if (Torn && Torn->IsVisible() != bBurst) Torn->SetVisibility(bBurst, true);
		if (!Sac || bBurst) continue;
		const float Fill = Body->SacFill(i);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Clk * 3.f + i * 2.1f);
		const float Glow = FMath::Max(0.25f + Fill * 3.f + L->Rise * 3.f, 0.f) * (0.9f + 0.1f * Pulse);
		if (FMath::Abs(Glow - L->SacGlow[i]) > 0.05f)
		{
			L->SacGlow[i] = Glow;
			VhSetGlow(Sac, Glow);
		}
		const float Swell = 0.82f + 0.35f * Fill + 0.15f * L->Rise + 0.03f * Pulse;
		Sac->SetRelativeScale3D(L->SacScale[i] * Swell);
	}

	// Moments in the water round her: the thrash's sparks, the streaming as she rears, the wake when she swims.
	const float Surf = VhLocalWater(C);
	const FVector Here = C->GetActorLocation();
	if (L->Conv > 0.5f && Now >= L->NextFxAt)
	{
		L->NextFxAt = Now + 0.08;
		const float R = C->GetSimpleCollisionRadius();
		const FVector A = Here + FVector(FMath::RandPointInCircle(R), FMath::FRandRange(-0.3f, 0.6f) * C->GetSimpleCollisionHalfHeight());
		const FVector B = Here + FVector(FMath::RandPointInCircle(R * 1.4f), FMath::FRandRange(-0.3f, 0.6f) * C->GetSimpleCollisionHalfHeight());
		WRFx::SpawnAt(C, TEXT("vharra.waterarc"), A, (B - A).Rotation(), (B - A).Size() / 100.f);
	}
	else if (L->Rise > 0.3f && Now >= L->NextFxAt)
	{
		L->NextFxAt = Now + 0.3;
		WRFx::SpawnAt(C, TEXT("vharra.drip"), Here + FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() * 0.5f), FRotator::ZeroRotator, 1.5f);
	}
	if (L->Swim > 0.5f && C->GetVelocity().Size2D() > 100.f && Now >= L->NextWakeAt)
	{
		L->NextWakeAt = Now + 0.25;
		const FVector Ahead = Here + C->GetVelocity().GetSafeNormal2D() * C->GetSimpleCollisionRadius();
		WRFx::SpawnAt(C, TEXT("mire.wake"), FVector(Ahead.X, Ahead.Y, Surf + 2.f), C->GetVelocity().Rotation(), 5.f);
	}
	Body->PlaceTrunk(DeltaSeconds);
}

void UWRSwampBossBody::PlaceTrunk(float DeltaTime)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	FWRVharraLook* L = VhLooks().Find(this);
	if (!C || !L || Trunk.Num() < 3 || L->SegDiam.Num() < 2)
	{
		return;
	}
	const int32 Segs = Trunk.Num() - 1;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	const float R = C->GetSimpleCollisionRadius(), HalfH = C->GetSimpleCollisionHalfHeight();
	const float Surf = VhLocalWater(C);
	const double Now = C->GetWorld()->GetTimeSeconds();
	const float Clk = Clock;
	const bool bFree = Has(WRSwampBossFlag::Free);
	const FVector AnchorAt = FVector(Anchor);
	// Her hips, behind the forebody at the waterline (under it when she swims).
	FVector Hip = C->GetActorLocation() - Fwd * R * 0.62f;
	Hip.Z = C->GetActorLocation().Z - HalfH * 0.45f - HalfH * L->Swim;
	TArray<FVector> P;
	P.SetNum(Segs + 1);
	const bool bWall = !bFree && !AnchorAt.IsZero();
	const bool bCoil = bFree && !AnchorAt.IsZero();
	if (bWall)
	{
		// Phase 1: a curve from her hips out behind her to the disc on the west wall.
		const FVector Out = (Hip - AnchorAt).GetSafeNormal2D();
		const float Span = float(FVector::Dist(Hip, AnchorAt));
		const FVector C1 = Hip - Fwd * Span * 0.35f, C2 = AnchorAt + Out * Span * 0.3f;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float T = float(i) / Segs, U = 1.f - T;
			P[i] = Hip * (U * U * U) + C1 * (3.f * U * U * T) + C2 * (3.f * U * T * T) + AnchorAt * (T * T * T);
		}
	}
	else if (bCoil)
	{
		// The drowning coil: out from her hips into a ring round the mound, a turn and a bit.
		const float Rr = VhT(TEXT("coilRadius"), 500.f) * 0.9f;
		const float A0 = FMath::Atan2(Hip.Y - AnchorAt.Y, Hip.X - AnchorAt.X);
		const int32 Lead = 3;
		const FVector Start = AnchorAt + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.f) * Rr;
		for (int32 i = 0; i <= Segs; ++i)
		{
			if (i <= Lead)
			{
				P[i] = FMath::Lerp(Hip, FVector(Start.X, Start.Y, Hip.Z), float(i) / Lead);
				continue;
			}
			const float A = A0 + float(i - Lead) / FMath::Max(1, Segs - Lead) * 2.f * PI * 1.15f;
			P[i] = AnchorAt + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rr;
		}
	}
	else
	{
		// Free: the trunk trails her through the water along the way her hips came.
		if (L->Trail.Num() == 0 || FVector::Dist(Hip, L->Trail.Last()) > L->SegLen * 0.5f) L->Trail.Add(Hip);
		const int32 Keep = (Segs + 6) * 2;
		if (L->Trail.Num() > Keep) L->Trail.RemoveAt(0, L->Trail.Num() - Keep);
		P[0] = Hip;
		FVector Cur = Hip;
		int32 Idx = L->Trail.Num() - 2;
		for (int32 i = 1; i <= Segs; ++i)
		{
			float Left = L->SegLen;
			while (Left > 0.f)
			{
				if (Idx >= 0)
				{
					const FVector NextPt = L->Trail[Idx];
					const float D = float(FVector::Dist(Cur, NextPt));
					if (D >= Left && D > KINDA_SMALL_NUMBER)
					{
						Cur += (NextPt - Cur) / D * Left;
						Left = 0.f;
					}
					else
					{
						Left -= D;
						Cur = NextPt;
						--Idx;
					}
				}
				else
				{
					Cur += -Fwd * Left;
					Left = 0.f;
				}
			}
			P[i] = Cur;
		}
	}
	// The lash: the trunk's free part lifted out of the water and swung to a side (1: straight up for the overhead).
	L->LashDeg = FMath::FInterpTo(L->LashDeg, float(Lash), DeltaTime, 10.f);
	L->LashLift = FMath::FInterpTo(L->LashLift, Lash != 0 ? 1.f : 0.f, DeltaTime, 6.f);
	const float Lift = 500.f * L->Scale * (FMath::Abs(L->LashDeg) < 5.f ? 1.8f : 1.f);
	// Float at the waterline, never through the terraces (the ground under each ring, looked up a few times a second).
	if (Now >= L->NextGroundAt || L->GroundZ.Num() != P.Num())
	{
		L->NextGroundAt = Now + 0.2;
		L->GroundZ.SetNum(P.Num());
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRVharraTrunk), false, C);
		for (int32 i = 0; i < P.Num(); ++i)
		{
			FHitResult Hit;
			L->GroundZ[i] = C->GetWorld()->LineTraceSingleByChannel(Hit, P[i] + FVector(0.f, 0.f, 1500.f), P[i] - FVector(0.f, 0.f, 2500.f), ECC_WorldStatic, Q) ? float(Hit.ImpactPoint.Z) : -1e7f;
		}
	}
	for (int32 i = 0; i <= Segs; ++i)
	{
		const float T = float(i) / Segs;
		const float D = L->SegDiam[FMath::Min(i, L->SegDiam.Num() - 1)];
		const float W = bWall || bCoil ? FMath::Sin(T * PI) : T;
		if (L->LashLift > 0.01f && !bCoil)
		{
			P[i] += Right * FMath::Sin(FMath::DegreesToRadians(L->LashDeg)) * VhT(TEXT("tailReach"), 1400.f) * 0.45f * W * L->LashLift;
		}
		float Z = Surf - D * (0.25f + 0.5f * L->Swim);
		if (bWall) Z = FMath::Lerp(Z, float(P[i].Z), FMath::SmoothStep(0.72f, 1.f, T));
		if (bCoil) Z = Surf - D * 0.2f + (Has(WRSwampBossFlag::Swimming) ? -D * 0.4f : D * 0.3f);
		Z += Lift * W * L->LashLift;
		Z += FMath::Sin(Clk * 40.f + i) * D * 0.12f * L->Conv;
		P[i] += Right * FMath::Sin(Clk * 33.f + i * 1.7f) * D * 0.15f * L->Conv;
		if (L->GroundZ.IsValidIndex(i)) Z = FMath::Max(Z, L->GroundZ[i] + D * 0.45f);
		P[i].Z = Z;
	}
	if (bWall) P[Segs] = AnchorAt;
	for (int32 i = 0; i < Segs; ++i)
	{
		USceneComponent* Frame = Trunk[i];
		if (!Frame) continue;
		const FVector Dir = P[i + 1] - P[i];
		const float Len = float(Dir.Size());
		Frame->SetWorldLocationAndRotation(P[i], FRotationMatrix::MakeFromX(Len > 1.f ? Dir / Len : -Fwd).Rotator());
		Frame->SetWorldScale3D(FVector(FMath::Clamp(Len / FMath::Max(1.f, L->SegLen), 0.4f, 3.f), 1.f, 1.f));
	}
	if (USceneComponent* Disc = Trunk[Segs])
	{
		// Pressed to the wall (its face toward it), or trailing at the end when she's let go.
		const FVector Face = bWall ? (AnchorAt - P[Segs - 1]).GetSafeNormal() : (P[Segs] - P[Segs - 1]).GetSafeNormal();
		Disc->SetWorldLocationAndRotation(P[Segs], FRotationMatrix::MakeFromX(Face.IsNearlyZero() ? -Fwd : Face).Rotator());
	}
	TrunkPts = P;
	LashNow = L->LashDeg;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
