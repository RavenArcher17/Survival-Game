#pragma once

// The swamp bosses' shared server state and helpers (WRSwampBosses.cpp: Mother Maw, the Hex-Warden, the Bloat;
// WRSwampVharra.cpp: Vharra). Private to those two files: everything here is inline, one state map for the module.

#include "CoreMinimal.h"
#include "WRSwampBosses.h"
#include "WRSwampTypes.h"
#include "WRBoss.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

/** Server: one swamp boss's fight (and its life between fights). */
struct FWRSwampBossState
{
	// Edges of what lands on it (a burn proc, frostbite, a bleed proc, a stance break).
	bool bBurnSeen = false, bFrostSeen = false, bStanceSeen = false;
	bool bArenaFound = false;
	// A grab: the death roll, the blood harvest, the latch, the three-jaw drain, the drag.
	TWeakObjectPtr<AWRCharacterBase> Grabbed;
	FName GrabKind;
	double GrabUntil = 0.0, GrabTickAt = 0.0;
	float GrabDps = 0.f, GrabFree = 0.f;
	bool bGrabAttached = false, bGrabWasDodging = false;
	// A move's own stages.
	int32 Stage = 0;
	double StageAt = 0.0, NextTickAt = 0.0;
	FVector Aim = FVector::ZeroVector, Aim2 = FVector::ZeroVector;
	float SweepFrom = 0.f, SweepTo = 0.f;
	TSet<TWeakObjectPtr<AWRCharacterBase>> Struck;
	// A ring wave rolling out along the ground or the water (the Sinking's surge, the heave's ring).
	bool bWave = false, bWaveLow = false;
	double WaveAt = 0.0;
	FVector WaveFrom = FVector::ZeroVector;
	float WaveRadius = 0.f, WaveDamage = 0.f, WaveHeight = 0.f, WaveKnock = 0.f;
	TSet<TWeakObjectPtr<AWRCharacterBase>> WaveHit;

	// Mother Maw.
	TArray<TWeakObjectPtr<AWRSwampBossProp>> Sections;
	TWeakObjectPtr<AWRSwampBossProp> Nest;
	bool bYoung60 = false, bYoung30 = false;
	TArray<TWeakObjectPtr<AWRCreature>> Young;
	double DefendUntil = 0.0, NextNestCheck = 0.0;
	bool bRearing = false, bRearPunish = false;
	/** Her belly's open (a rear, a roll) until; the rear's own extra punish was added. */
	double BellyUntil = 0.0;
	FVector NestAt = FVector::ZeroVector, RestAt = FVector::ZeroVector;
	float PoolRadius = 3000.f, NestRadius = 650.f;
	bool bNestYoungRoar = false;

	// The Hex-Warden.
	TWeakObjectPtr<AWRSwampBossProp> TotemProps[4];
	TWeakObjectPtr<AWRCreature> Totems[4];
	uint8 TotemsDown = 0;
	int32 HitCount = 0;
	bool bScattered = false;
	double ReformAt = 0.0;
	int32 ReformTotem = INDEX_NONE;
	struct FMimic { TWeakObjectPtr<AWRCreature> Tame; double Until = 0.0; EWRTameOrder Order = EWRTameOrder::Follow; bool bAggressive = true; FVector Side = FVector::ZeroVector; };
	TArray<FMimic> Mimicked;
	TSet<TWeakObjectPtr<AWRCharacterBase>> BleedingNear;
	double NextBloodLook = 0.0;
	int32 WalkIndex = 0, WalkDir = 1;
	bool bWalking = false;
	double NextHexAt = 0.0, HexUntil = 0.0;
	bool bMaskBroken = false;
	FVector OssuaryAt = FVector::ZeroVector;

	// The Bloat.
	float Gorge = 0.f;
	float VeinHP[3] = { 300.f, 300.f, 300.f };
	double NoGainUntil = 0.0, RestUntil = 0.0, LastRest = -10000.0, LatchUntil = 0.0;
	int32 RouteIndex = 0, CausewayIndex = 0, CausewayDir = 1;
	TArray<FVector> Route, Causeway;
	TWeakObjectPtr<AWRCreature> Prey;
	double PreyUntil = 0.0, NextPreyLook = 0.0, ClashUntil = 0.0, NextClashAt = 0.0;
	bool bInvading = false;
	FVector InvadeTo = FVector::ZeroVector;
	/** The way it walks to an invasion (Stormgut's foot, its mouth, the target) and how far along. */
	TArray<FVector> InvadeWay;
	int32 InvadeIndex = 0;
	bool bBurstQueued = false, bSleeping = false, bWasTide = false;
	double BurstStaggerUntil = 0.0, NextWadeTick = 0.0;
	TWeakObjectPtr<AWRBoss> ClashWith;

	// Vharra.
	TArray<TWeakObjectPtr<AWRSwampRod>> Rods;
	TWeakObjectPtr<AWRSwampFlood> Flood;
	float SacHP[3] = { 1200.f, 1200.f, 1200.f };
	float Fill[3] = { 0.f, 0.f, 0.f };
	double ConvulseUntil = 0.0, ConvulseGapUntil = 0.0, NoHealUntil = 0.0, NextBroodAt = 0.0, LashStart = 0.0;
	TArray<TWeakObjectPtr<AWRCreature>> Brood;
	FVector Seat = FVector::ZeroVector, AnchorAt = FVector::ZeroVector, Centre = FVector::ZeroVector, CoilAt = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AWRSwampBossProp>> Crusts;
	TArray<TPair<double, float>> WaterLog;
	TArray<TWeakObjectPtr<AWRCharacterBase>> ClotTargets;
};

namespace WRSwampBossDetail
{
	inline TMap<TWeakObjectPtr<const AWRBoss>, FWRSwampBossState>& SwbStates()
	{
		static TMap<TWeakObjectPtr<const AWRBoss>, FWRSwampBossState> Map;
		return Map;
	}

	inline FWRSwampBossState& SwbS(const AWRBoss* Boss) { return SwbStates().FindOrAdd(Boss); }
	inline const FWRSwampBossState* SwbFind(const AWRBoss* Boss) { return SwbStates().Find(Boss); }

	/** The Hex-Warden's hexed beasts and until when (world seconds). */
	inline TMap<TWeakObjectPtr<const AActor>, double>& SwbHexed()
	{
		static TMap<TWeakObjectPtr<const AActor>, double> Map;
		return Map;
	}

	inline double SwbNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }

	/** The server's clock as every machine sees it (replicated timings: the flood's rise, a shot's flight). */
	inline float SwbServerTime(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : World ? World->GetTimeSeconds() : 0.f;
	}

	inline float SwbT(const TCHAR* Key, float Fallback) { return WRTune(*FString::Printf(TEXT("swampBosses.%s"), Key), Fallback); }
	inline float SwbST(const AWRCreature* Boss, const TCHAR* Key, float Fallback)
	{
		return FWRTuning::Get().Num(FString::Printf(TEXT("swampBosses.%s.%s"), Boss ? *Boss->CreatureId.ToString() : TEXT("_"), Key), Fallback);
	}
	/** swampBosses.<id>.<key> by a boss id (a prop or a totem asking for its boss's number). */
	inline float SwbIT(FName Id, const TCHAR* Key, float Fallback)
	{
		return FWRTuning::Get().Num(FString::Printf(TEXT("swampBosses.%s.%s"), *Id.ToString(), Key), Fallback);
	}

	inline float SwbScale() { return WRTune(TEXT("bosses.damageScale"), 1.f) * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }

	inline FName SwbKind(const AWRCreature* Boss)
	{
		static const FName Ids[] = { TEXT("mother_maw"), TEXT("hex_warden"), TEXT("bloat"), TEXT("vharra") };
		if (Boss) for (const FName& Id : Ids) if (Boss->CreatureId == Id) return Id;
		return NAME_None;
	}

	inline bool SwbIsHexKind(FName Id) { return Id == TEXT("bogstalker") || Id == TEXT("murkjaw"); }

	inline FVector SwbGround(const UWorld* World, const FVector& At, bool& bOk, float Up = 3000.f, float Down = 8000.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampBossGround), false);
		bOk = World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q);
		return bOk ? FVector(Hit.ImpactPoint) : At;
	}

	/** The landscape alone under a point (no props, no bodies): where posts and plinths stand. */
	inline FVector SwbLand(const UWorld* World, const FVector& At, bool& bOk, float Up = 3000.f, float Down = 8000.f)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampBossLand), false);
		bOk = false;
		if (!World) return At;
		for (TActorIterator<AWRSwampBossProp> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AWRSwampRod> It(World); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < 3000.f) Q.AddIgnoredActor(*It);
		FHitResult Hit;
		bOk = World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, Up), At - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q);
		return bOk ? FVector(Hit.ImpactPoint) : At;
	}

	inline FVector SwbFeet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }

	inline double SwbNum(const TSharedPtr<FJsonObject>& O, const TCHAR* F, double Fallback)
	{
		double V = Fallback;
		if (O.IsValid()) O->TryGetNumberField(F, V);
		return V;
	}

	/** An arena's centre ({x, y, z} metres) in world cm. */
	inline FVector SwbCentre(const TSharedPtr<FJsonObject>& O)
	{
		return O.IsValid() ? FVector(SwbNum(O, TEXT("x"), 0.0), SwbNum(O, TEXT("y"), 0.0), SwbNum(O, TEXT("z"), 0.0)) * 100.0 : FVector::ZeroVector;
	}

	inline bool SwbHasPoint(const TSharedPtr<FJsonObject>& O, const TCHAR* F)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(F, A) && A->Num() >= 2;
	}

	/** A ring of the Throat ("rings": {"middle": [inner, outer, floor z]}, metres): inner and outer radius, floor (cm). */
	inline bool SwbRing(const TSharedPtr<FJsonObject>& Arena, const TCHAR* Name, float& OutInner, float& OutOuter, float& OutZ)
	{
		const TSharedPtr<FJsonObject>* Rings = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!Arena.IsValid() || !Arena->TryGetObjectField(TEXT("rings"), Rings) || !(*Rings)->TryGetArrayField(Name, A) || A->Num() < 3) return false;
		OutInner = float((*A)[0]->AsNumber()) * 100.f;
		OutOuter = float((*A)[1]->AsNumber()) * 100.f;
		OutZ = float((*A)[2]->AsNumber()) * 100.f;
		return true;
	}

	/** Every survivor and tame it may fight inside its chase range. */
	inline TArray<AWRCharacterBase*> SwbFoes(const AWRBoss* Boss, float Range, bool bPlayersOnly = false)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
		{
			if (*It == Boss || !It->IsAlive() || !Boss->IsHostileTo(*It) || It->GetAttachParentActor()) continue;
			if (bPlayersOnly && !It->GetPlayerState()) continue;
			if (FVector::Dist2D(It->GetActorLocation(), Boss->GetHome()) > Range) continue;
			if (const AWRCreature* C = Cast<AWRCreature>(*It); C && !C->IsTamed()) continue;   // wild things aren't its fight
			Out.Add(*It);
		}
		return Out;
	}

	/** Everyone (survivors and their tames, mounted ones too) near a point: what water, waves and spills reach. */
	inline TArray<AWRCharacterBase*> SwbNear(const UWorld* World, const FVector& At, float Radius, const AActor* Except = nullptr)
	{
		TArray<AWRCharacterBase*> Out;
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It == Except || !It->IsAlive() || It->IsA<AWRBoss>()) continue;
			const AWRCreature* C = Cast<AWRCreature>(*It);
			if (C && !C->IsTamed()) continue;
			if (FVector::Dist2D(It->GetActorLocation(), At) <= Radius + It->GetSimpleCollisionRadius()) Out.Add(*It);
		}
		return Out;
	}

	/** A blow from one of its moves outside the row machine (a grab's squeeze, a wave, a spray tick). */
	inline void SwbHit(AWRBoss* Boss, AWRCharacterBase* Victim, float Damage, float Bleed, float Curse, bool bUnblockable, bool bLow, uint8 Impact, bool bOverTime = false, float Knock = 0.f)
	{
		if (!Boss || !Victim || !Victim->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage * SwbScale() * (Boss->Phase >= 3 ? SwbST(Boss, TEXT("phase3DamageScale"), 1.4f) : 1.f);
		Hit.PoiseDamage = bOverTime ? 0.f : Damage * 1.2f;
		Hit.Impact = Impact;
		Hit.bUnblockable = bUnblockable;
		Hit.bLow = bLow;
		Hit.bHeavy = !bOverTime;
		Hit.bDamageOverTime = bOverTime;
		Hit.Tier = bOverTime ? EWRHitTier::Light : EWRHitTier::Heavy;
		Hit.Origin = Boss->GetActorLocation();
		Hit.Element = WRTags::Element_Physical;
		if (Bleed > 0.f) Hit.Buildups.Add(TEXT("bleed"), Bleed);
		if (Curse > 0.f) Hit.Buildups.Add(TEXT("curse"), Curse);
		const TWeakObjectPtr<AWRBoss> WeakBoss(Boss);
		const TWeakObjectPtr<AWRCharacterBase> WeakWho(Victim);
		UWRCombatLibrary::ApplyHit(Victim, Hit, [WeakBoss, WeakWho, Knock](EWRHitResult Landed)
		{
			AWRBoss* B = WeakBoss.Get();
			AWRCharacterBase* V = WeakWho.Get();
			if (B && V && V->IsAlive() && Knock > 0.f && (Landed == EWRHitResult::Hit || Landed == EWRHitResult::GuardBroken))
			{
				const FVector Push = (V->GetActorLocation() - B->GetActorLocation()).GetSafeNormal2D() * Knock + FVector(0.f, 0.f, Knock * 0.45f);
				V->LaunchCharacter(Push, true, true);
			}
		});
	}

	/** Bleed buildup straight onto someone (a bleedwater slick, the blood flood): the meter, no blow. */
	inline void SwbBleed(AWRCharacterBase* Who, float Amount, AActor* Source)
	{
		if (Who && Who->IsAlive() && Who->Status && Amount > 0.f)
		{
			Who->Status->AddStatusBuildup(WRStatus::Bleed, Amount, Cast<AWRCharacterBase>(Source));
		}
	}

	/** Heal a boss by an amount (capped at its max). */
	inline void SwbHeal(AWRCharacterBase* Who, float Amount)
	{
		UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr;
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		if (ASC && A && Amount > 0.f && Who->IsAlive())
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
		}
	}

	inline float SwbMaxHealth(const AWRCharacterBase* Who)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		return A ? A->GetMaxHealth() : 1.f;
	}

	inline float SwbHealthFrac(const AWRCharacterBase* Who)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		return A ? A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()) : 1.f;
	}

	/** Shortest distance on the ground from P to the segment A-B. */
	inline float SwbSegDist2D(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector2D AB(B - A), AP(P - A);
		const float T = FMath::Clamp(FVector2D::DotProduct(AP, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
		return FVector2D::Distance(FVector2D(A) + AB * T, FVector2D(P));
	}

	/** The creatures.json row for an id (the look's palette). */
	inline TSharedPtr<FJsonObject> SwbRow(FName Id)
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
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FString S;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), S)) Rows.Add(FName(*S), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Found = Rows.Find(Id);
		return Found ? *Found : nullptr;
	}

	/** Its row's "look" object (palette, flatten, length), or null. */
	inline TSharedPtr<FJsonObject> SwbLook(FName Id)
	{
		const TSharedPtr<FJsonObject> Row = SwbRow(Id);
		const TSharedPtr<FJsonObject>* Look = nullptr;
		return Row.IsValid() && Row->TryGetObjectField(TEXT("look"), Look) ? *Look : nullptr;
	}

	inline FLinearColor SwbHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }
	inline FLinearColor SwbColor(const TSharedPtr<FJsonObject>& O, const TCHAR* F, const FLinearColor& Fallback)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Fallback;
	}

	/** The Throat's flood near a point (Vharra's). */
	inline AWRSwampFlood* SwbFlood(const UWorld* World, const FVector& Near, float Range = 12000.f)
	{
		if (!World) return nullptr;
		for (TActorIterator<AWRSwampFlood> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Near) < Range) return *It;
		}
		return nullptr;
	}

	inline AWRBoss* SwbFindBoss(const UWorld* World, FName Id)
	{
		if (!World) return nullptr;
		for (TActorIterator<AWRBoss> It(World); It; ++It)
		{
			if (It->CreatureId == Id && It->IsAlive()) return *It;
		}
		return nullptr;
	}

	/** A spawned creature's health set (a young Murkjaw at 120, a brood leech at 25). */
	inline void SwbSetHealth(AWRCreature* C, float Health)
	{
		if (UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), Health);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Health);
		}
	}

	/** Standing on one of Mother Maw's whole boardwalk sections. */
	inline AWRSwampBossProp* SwbOnSection(const AActor* Who)
	{
		if (!Who) return nullptr;
		const FVector Feet = SwbFeet(Who);
		for (TActorIterator<AWRSwampBossProp> It(Who->GetWorld()); It; ++It)
		{
			if (It->Kind == TEXT("walk") && It->State != 1 && It->OnDeck(Feet)) return *It;
		}
		return nullptr;
	}

	/** A player controller by player name (a tame's owner). */
	inline AWRPlayerController* SwbOwnerPC(const UWorld* World, const FString& Name)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Name) return PC;
		}
		return nullptr;
	}

	/** A sound event if the table has it, else a stand-in that does (the swamp's own lines come with its audio). */
	inline FName SwbSound(const TCHAR* Event, const TCHAR* Fallback)
	{
		const FName E(Event);
		return WRSound::Has(E) ? E : FName(Fallback);
	}

	/** On a Mirestrider (the bird stands tall): out of the shocked water. */
	inline bool SwbOnTallMount(const AActor* Who)
	{
		const AWRCreature* M = Who ? Cast<AWRCreature>(Who->GetAttachParentActor()) : nullptr;
		return M && M->CreatureId == TEXT("mirestrider");
	}

	/** Pinned in place for a grab (attached to the boss) or let go again. */
	inline void SwbHold(AWRCharacterBase* Victim, AActor* Holder, const FVector& RelativeAt, bool bHold)
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

	/** The point along a polyline nearest P (its index). */
	inline int32 SwbNearestIndex(const TArray<FVector>& Pts, const FVector& P)
	{
		int32 Best = 0;
		float BestD = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const float D = FVector::DistSquared2D(Pts[i], P);
			if (D < BestD) { BestD = D; Best = i; }
		}
		return Best;
	}

	/** Walk a body toward a point on the ground (AI input, a share of its speed); true once there. */
	inline bool SwbWalkTo(AWRBoss* Boss, const FVector& Goal, float Speed, float Arrive = 250.f)
	{
		const FVector To = (Goal - Boss->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (To.Size2D() < Arrive) return true;
		const FWRCreatureDef* Def = Boss->GetDef();
		const float Scale = FMath::Clamp(Speed / FMath::Max(50.f, Def ? Def->Speed : 400.f), 0.1f, 1.f);
		Boss->AddMovementInput(To.GetSafeNormal2D(), Scale);
		return false;
	}

	/** A survivor's tame (or the survivor) for the hooks that care who's behind a blow. */
	inline bool SwbIsSurvivorSide(const AActor* A)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(A);
		if (!C) return false;
		if (C->GetPlayerState()) return true;
		const AWRCreature* Cr = Cast<AWRCreature>(C);
		return Cr && Cr->IsTamed();
	}
}
