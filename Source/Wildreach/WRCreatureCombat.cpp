// Creatures in a fight (cooks/build-combat-full.md 2.15, 2.16, 2.19): attack rows from Data/attacks.json with real tells
// (a rising cue and a small glint on the attacking part, a final cue before a delayed strike), tracking that commits,
// punish windows, combos, packs that flank, surround and breathe with no attack token; lock points and breakable parts;
// fighting from the saddle and being thrown off it.
#include "WRCreature.h"
#include "WRAttackRows.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWeapons.h"
#include "WRPadInput.h"
#include "WRInventory.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------- data

namespace WRRowsDetail
{
	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* F, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(F, V) ? float(V) : Fallback;
	}

	FWRAttackRow Read(const FString& Id, const TSharedPtr<FJsonObject>& O)
	{
		FWRAttackRow R;
		R.Id = FName(*Id);
		FString S;
		if (O->TryGetStringField(TEXT("motion"), S)) R.Motion = FName(*S);
		const TArray<TSharedPtr<FJsonValue>>* Range = nullptr;
		if (O->TryGetArrayField(TEXT("range"), Range) && Range->Num() == 2)
		{
			R.MinRange = float((*Range)[0]->AsNumber());
			R.MaxRange = float((*Range)[1]->AsNumber());
			R.Range = R.MaxRange;
		}
		else
		{
			R.Range = Num(O, TEXT("range"), R.Range);
			R.MaxRange = R.Range;
		}
		R.Windup = Num(O, TEXT("windup"), R.Windup);
		R.TrackUntil = Num(O, TEXT("track"), R.TrackUntil);
		R.Hold = Num(O, TEXT("hold"), 0.f);
		R.Active = Num(O, TEXT("active"), R.Active);
		R.Punish = Num(O, TEXT("punish"), R.Punish);
		R.Dmg = Num(O, TEXT("dmg"), R.Dmg);
		R.Poise = Num(O, TEXT("poise"), R.Poise);
		R.Impact = uint8(FMath::Clamp(FMath::RoundToInt(Num(O, TEXT("impact"), 2.f)), 1, 5));
		R.StaminaDamage = Num(O, TEXT("staminaDamage"), -1.f);
		const TSharedPtr<FJsonObject>* Build = nullptr;
		if (O->TryGetObjectField(TEXT("buildup"), Build))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& B : (*Build)->Values) R.Buildups.Add(FName(*B.Key), float(B.Value->AsNumber()));
		}
		if (O->TryGetStringField(TEXT("type"), S)) R.PhysType = S == TEXT("slash") ? 1 : S == TEXT("strike") ? 2 : S == TEXT("pierce") ? 3 : 0;
		const TArray<TSharedPtr<FJsonValue>>* Flags = nullptr;
		if (O->TryGetArrayField(TEXT("flags"), Flags))
		{
			for (const TSharedPtr<FJsonValue>& F : *Flags)
			{
				const FString Flag = F->AsString();
				R.bParryable |= Flag == TEXT("parryable");
				R.bLow |= Flag == TEXT("low");
				R.bUnblockable |= Flag == TEXT("unblockable");
				R.bGrab |= Flag == TEXT("grab");
				R.bIgnoreHyperarmor |= Flag == TEXT("ignoreHyperarmor");
				R.bBehindOnly |= Flag == TEXT("behindOnly");
				R.bReadsAction |= Flag == TEXT("readsAction");
				R.bTamePunish |= Flag == TEXT("tamePunish");
				R.bTransition |= Flag == TEXT("transition");
				R.bThrown |= Flag == TEXT("thrown");
				if (Flag == TEXT("ranged")) R.bMelee = false;
			}
		}
		R.ProjectileSpeed = FMath::Min(Num(O, TEXT("projectileSpeed"), 1000.f), WRTune(TEXT("creatureAI.maxProjectileSpeed"), 1000.f));
		R.Knockback = Num(O, TEXT("knockback"), 0.f);
		R.Reach = Num(O, TEXT("reach"), R.Reach);
		R.Arc = Num(O, TEXT("arc"), R.Arc);
		R.Radius = Num(O, TEXT("radius"), 0.f);
		R.Speed = Num(O, TEXT("speed"), 0.f);
		R.WallStun = Num(O, TEXT("wallStun"), 0.f);
		const TSharedPtr<FJsonObject>* Next = nullptr;
		if (O->TryGetObjectField(TEXT("next"), Next))
		{
			R.Next = FName(*(*Next)->GetStringField(TEXT("id")));
			R.NextChance = Num(*Next, TEXT("chance"), 1.f);
		}
		R.Cooldown = Num(O, TEXT("cooldown"), R.Cooldown);
		const TSharedPtr<FJsonObject>* W = nullptr;
		if (O->TryGetObjectField(TEXT("weights"), W))
		{
			R.WClose = Num(*W, TEXT("close"), 1.f);
			R.WMid = Num(*W, TEXT("mid"), 1.f);
			R.WFar = Num(*W, TEXT("far"), 1.f);
		}
		const TSharedPtr<FJsonObject>* Tell = nullptr;
		if (O->TryGetObjectField(TEXT("tell"), Tell))
		{
			if ((*Tell)->TryGetStringField(TEXT("bone"), S)) R.TellBone = FName(*S);
			if ((*Tell)->TryGetStringField(TEXT("glint"), S)) R.Glint = S == TEXT("crimson") ? 2 : S == TEXT("none") ? 0 : 1;
		}
		if (R.bUnblockable || R.bGrab) R.Glint = 2;  // the colour law: crimson = dodge it
		if (O->TryGetStringField(TEXT("clip"), S)) R.Clip = FName(*S);
		// Bosses.
		if (O->TryGetStringField(TEXT("shape"), S)) R.Shape = FName(*S);
		R.Size = Num(O, TEXT("size"), R.Size);
		R.Repeats = FMath::Max(1, FMath::RoundToInt(Num(O, TEXT("repeats"), 1.f)));
		R.Gap = Num(O, TEXT("gap"), R.Gap);
		R.Phase = FMath::RoundToInt(Num(O, TEXT("phase"), 1.f));
		R.MaxPhase = FMath::RoundToInt(Num(O, TEXT("maxPhase"), 9.f));
		R.Height = uint8(FMath::RoundToInt(Num(O, TEXT("height"), 0.f)));
		R.DamageTaken = Num(O, TEXT("damageTaken"), 1.f);
		R.HoldOn = Num(O, TEXT("holdOn"), -1.f);
		if (O->HasField(TEXT("melee"))) R.bMelee = O->GetBoolField(TEXT("melee"));
		// The rules every row obeys (COMBAT-AND-LESSONS 1.13): a visible wind-up, a real punish.
		R.Windup = FMath::Max(R.Windup, WRTune(TEXT("creatureAI.minWindup"), 0.34f));
		return R;
	}
}

FWRAttackRows& FWRAttackRows::Get()
{
	static FWRAttackRows Instance;
	return Instance;
}

FWRAttackRows::FWRAttackRows()
{
	Reload();
}

void FWRAttackRows::Reload()
{
	Creatures.Reset();
	Bosses.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("attacks.json"));
	if (!Root.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("ATTACKS Data/attacks.json missing: creatures fight the old way"));
		return;
	}
	const auto ReadSet = [](const TSharedPtr<FJsonObject>& Set, TFunction<void(FName, const FWRAttackRow&)> Add)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Who : Set->Values)
		{
			const TSharedPtr<FJsonObject> Rows = Who.Value->AsObject();
			if (!Rows.IsValid() || Who.Key.StartsWith(TEXT("_"))) continue;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Row : Rows->Values)
			{
				const TSharedPtr<FJsonObject> R = Row.Value->AsObject();
				if (R.IsValid() && !Row.Key.StartsWith(TEXT("_")))
				{
					Add(FName(*Who.Key), WRRowsDetail::Read(Row.Key, R));
				}
			}
		}
	};
	const TSharedPtr<FJsonObject>* Set = nullptr;
	if (Root->TryGetObjectField(TEXT("creatures"), Set))
	{
		ReadSet(*Set, [this](FName Who, const FWRAttackRow& R) { Creatures.FindOrAdd(Who).Add(R.Id, R); });
	}
	if (Root->TryGetObjectField(TEXT("bosses"), Set))
	{
		ReadSet(*Set, [this](FName Who, const FWRAttackRow& R) { Bosses.FindOrAdd(Who).Add(R); });
	}
	UE_LOG(LogWildreach, Log, TEXT("ATTACKS rows for %d creatures, %d bosses"), Creatures.Num(), Bosses.Num());
}

static FAutoConsoleCommand GReloadAttacksCmd(TEXT("WR.ReloadAttacks"), TEXT("Re-read Data/attacks.json"),
	FConsoleCommandDelegate::CreateLambda([]() { FWRAttackRows::Get().Reload(); }));

void FWRCreatureDef::ReadCombat(const TSharedPtr<FJsonObject>& Row)
{
	using WRRowsDetail::Num;
	const TSharedPtr<FJsonObject>* Resist = nullptr;
	if (Row->TryGetObjectField(TEXT("physResist"), Resist))
	{
		PhysResist[1] = Num(*Resist, TEXT("slash"), 1.f);
		PhysResist[2] = Num(*Resist, TEXT("strike"), 1.f);
		PhysResist[3] = Num(*Resist, TEXT("pierce"), 1.f);
	}
	FString S;
	if (Row->TryGetStringField(TEXT("physMaterial"), S)) PhysMaterial = FName(*S);
	if (Row->TryGetStringField(TEXT("firstKill"), S)) FirstKillItem = FName(*S);
	const auto ReadPoints = [&Row](const TCHAR* Field, TArray<FBodyPoint>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!Row->TryGetArrayField(Field, List)) return;
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FBodyPoint P;
			FString Str;
			P.Id = O->TryGetStringField(TEXT("id"), Str) ? FName(*Str) : NAME_None;
			O->TryGetStringField(TEXT("name"), P.Name);
			if (O->TryGetStringField(TEXT("bone"), Str)) P.Bone = FName(*Str);
			const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
			if (O->TryGetArrayField(TEXT("at"), At) && At->Num() == 3)
			{
				P.At = FVector((*At)[0]->AsNumber(), (*At)[1]->AsNumber(), (*At)[2]->AsNumber());
			}
			P.Stance = WRRowsDetail::Num(O, TEXT("stance"), 0.f);
			if (O->TryGetStringField(TEXT("onBreak"), Str)) P.OnBreak = FName(*Str);
			if (O->TryGetStringField(TEXT("drop"), Str)) P.Drop = FName(*Str);
			if (P.Id.IsNone()) P.Id = FName(*P.Name.ToLower());
			Out.Add(P);
		}
	};
	ReadPoints(TEXT("lockPoints"), LockPoints);
	ReadPoints(TEXT("parts"), Parts);
	const TArray<TSharedPtr<FJsonValue>>* RoamList = nullptr;
	if (Row->TryGetArrayField(TEXT("roam"), RoamList))
	{
		for (const TSharedPtr<FJsonValue>& V : *RoamList)
		{
			const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
			if (P.Num() >= 2) Roam.Add(FVector(P[0]->AsNumber(), P[1]->AsNumber(), P.Num() > 2 ? P[2]->AsNumber() : 0.0));
		}
	}
}

// ---------------------------------------------------------------- lock points and parts

int32 AWRCreature::NumLockPoints() const
{
	const FWRCreatureDef* Def = GetDef();
	return Def ? Def->LockPoints.Num() : 0;
}

FName AWRCreature::FindBone(FName Hint) const
{
	const USkinnedAsset* Asset = Skel && Skel->IsVisible() ? Skel->GetSkinnedAsset() : nullptr;
	if (!Asset || Hint.IsNone())
	{
		return NAME_None;
	}
	if (Hint == TEXT("head"))
	{
		return HeadBone();
	}
	const FString Want = Hint.ToString().ToLower();
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		const FString Name = Ref.GetBoneName(i).ToString().ToLower();
		if (Name.Contains(Want) && !Name.Contains(TEXT("end")) && !Name.Contains(TEXT("nub")))
		{
			return Ref.GetBoneName(i);
		}
	}
	return NAME_None;
}

FVector AWRCreature::BodyPointLocation(const FWRCreatureDef::FBodyPoint& Point) const
{
	// Where this machine has the body, the bone itself; the server (no mesh) uses the point in body units.
	const FName Bone = GetNetMode() == NM_DedicatedServer ? NAME_None : FindBone(Point.Bone);
	if (!Bone.IsNone())
	{
		return Skel->GetBoneLocation(Bone);
	}
	const float R = GetSimpleCollisionRadius(), HH = GetSimpleCollisionHalfHeight();
	return GetActorLocation() + GetActorForwardVector() * Point.At.X * R + GetActorRightVector() * Point.At.Y * R + FVector(0.f, 0.f, Point.At.Z * HH);
}

FVector AWRCreature::LockPointLocation(int32 Index) const
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def || !Def->LockPoints.IsValidIndex(Index))
	{
		return GetActorLocation();
	}
	return BodyPointLocation(Def->LockPoints[Index]);
}

bool AWRCreature::IsNearLockPoint(const FVector& At, FName Name, float Radius) const
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def || At.IsZero())
	{
		return false;
	}
	for (const FWRCreatureDef::FBodyPoint& P : Def->LockPoints)
	{
		if ((P.Id == Name || P.Bone == Name) && FVector::Dist(BodyPointLocation(P), At) <= Radius)
		{
			return true;
		}
	}
	return false;
}

bool AWRCreature::IsPartBroken(FName PartId) const
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def)
	{
		return false;
	}
	for (int32 i = 0; i < Def->Parts.Num() && i < 8; ++i)
	{
		if (Def->Parts[i].Id == PartId)
		{
			return (PartsBroken & (1 << i)) != 0;
		}
	}
	return false;
}

void AWRCreature::ResetParts()
{
	PartsBroken = 0;
	PartStance.Reset();
}

void AWRCreature::DamagePart(const FWRHit& Hit, float Stance, AActor* By)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def || Def->Parts.Num() == 0 || Stance <= 0.f)
	{
		return;
	}
	if (PartStance.Num() != Def->Parts.Num())
	{
		PartStance.SetNum(Def->Parts.Num());
		for (int32 i = 0; i < Def->Parts.Num(); ++i) PartStance[i] = Def->Parts[i].Stance;
	}
	// The part the blow landed on: nearest within partRadius of where it met the body (a wing hit by an arrow in flight
	// counts too: a ranged blow lands where it struck).
	const FVector At = Hit.ImpactPoint.IsZero() ? Hit.Origin : Hit.ImpactPoint;
	const float Reach = WRTune(TEXT("stance.partRadius"), 120.f) + GetSimpleCollisionRadius() * 0.25f;
	int32 Best = INDEX_NONE;
	float BestDist = Reach;
	for (int32 i = 0; i < Def->Parts.Num() && i < 8; ++i)
	{
		if (PartsBroken & (1 << i)) continue;
		const float D = FVector::Dist(BodyPointLocation(Def->Parts[i]), At);
		if (D < BestDist)
		{
			BestDist = D;
			Best = i;
		}
	}
	if (Best == INDEX_NONE)
	{
		return;
	}
	PartStance[Best] -= Stance;
	if (PartStance[Best] > 0.f)
	{
		return;
	}
	// Broken (combat.md 2.10): a part stagger, a share of the body's stance, its drop once, and what it changes.
	const FWRCreatureDef::FBodyPoint& Part = Def->Parts[Best];
	PartsBroken |= uint8(1 << Best);
	OnRep_PartsBroken();
	Status->Stagger(WRTune(TEXT("stance.partStaggerSeconds"), 1.2f));
	if (UWRAttributeSet* A = GetAttributes())
	{
		AbilitySystem->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), A->GetStance() - A->GetMaxStance() * WRTune(TEXT("stance.partBreakBodyStance"), 0.3f));
		Status->NotifyStanceDamaged();
		if (A->GetStance() <= 0.f && !HasTag(WRTags::Status_StanceBroken))
		{
			UWRCombatLibrary::BreakStance(this, UWRCombatLibrary::StanceBreakSeconds(this), By);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("PART %s's %s broken by %s"), *GetName(), *Part.Id.ToString(), *GetNameSafe(By));
	OnPartBroken(Part, By);
}

void AWRCreature::OnPartBroken(const FWRCreatureDef::FBodyPoint& Part, AActor* By)
{
	// Its drop, once per fight: a boss's goes to everyone on its ledger (AWRBoss), anything else to who broke it.
	if (Part.Drop.IsNone())
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(By);
	const AWRPlayerState* PS = Pawn ? Pawn->GetPlayerState<AWRPlayerState>() : nullptr;
	if (PS && PS->GetInventory())
	{
		PS->GetInventory()->AddItem(Part.Drop, 1);
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(Pawn->GetController()))
		{
			PC->ClientNotify(FString::Printf(TEXT("+1 %s"), *Part.Name));
		}
	}
}

void AWRCreature::OnRep_PartsBroken()
{
	// Every machine: a burst of debris where the part gave way (stone, horn, ice by the body).
	const FWRCreatureDef* Def = GetDef();
	if (!Def || GetNetMode() == NM_DedicatedServer)
	{
		PartsShown = PartsBroken;
		return;
	}
	for (int32 i = 0; i < Def->Parts.Num() && i < 8; ++i)
	{
		const uint8 Bit = uint8(1 << i);
		if ((PartsBroken & Bit) && !(PartsShown & Bit))
		{
			WRFx::SpawnAt(this, TEXT("part.break"), BodyPointLocation(Def->Parts[i]), FRotator(90.f, 0.f, 0.f), FMath::Max(0.8f, GetSimpleCollisionRadius() / 120.f));
			WRSound::PlayAt(this, TEXT("part.break"), BodyPointLocation(Def->Parts[i]));
		}
	}
	PartsShown = PartsBroken;
}

void AWRCreature::MulticastTell_Implementation(uint8 Glint, FName BoneHint, FName Sound)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FName Bone = FindBone(BoneHint);
	if (Glint > 0)
	{
		// A small glint on the attacking part: white = it can be blocked, crimson = dodge it (the colour law).
		const FVector At = Bone.IsNone() ? MouthLocation() : Skel->GetBoneLocation(Bone);
		WRFx::SpawnAt(this, Glint == 2 ? TEXT("glint.crimson") : TEXT("glint.white"), At, FRotator::ZeroRotator, FMath::Clamp(GetSimpleCollisionRadius() / 90.f, 0.7f, 2.5f));
	}
	if (!Sound.IsNone())
	{
		if (Sound == TEXT("voice"))
		{
			WRSound::Voice(this, TEXT("attack"));
		}
		else
		{
			WRSound::PlayOn(this, Sound);
		}
	}
}

float AWRCreature::ModifyIncomingDamage(const FWRHit& Hit, float Damage)
{
	// Shelled up: blows glance off the shell, blunt ones less (creatureAI.withdraw).
	if (HasAuthority() && GetWorld()->GetTimeSeconds() < WithdrawnUntil && !Hit.bDamageOverTime)
	{
		Damage *= WRTune(TEXT("creatureAI.withdraw.damageTaken"), 0.2f) * (Hit.PhysType == 2 ? WRTune(TEXT("creatureAI.withdraw.strikeMult"), 1.5f) : 1.f);
	}
	return Damage;
}

// ---------------------------------------------------------------- the attack rows

namespace WRRowFight
{
	/** The off-screen rule (COMBAT-AND-LESSONS 1.13): a wind-up starts only where its target can see it coming (on
	 *  its screen), or hear it close by. Tames and other creatures don't need warning. */
	bool CanSee(const AWRCreature* Self, const AWRCharacterBase* Target)
	{
		const APlayerController* PC = Target ? Cast<APlayerController>(Target->GetController()) : nullptr;
		if (!PC)
		{
			return true;
		}
		const FVector To = Self->GetActorLocation() - Target->GetActorLocation();
		if (To.Size() < WRTune(TEXT("creatureAI.offscreenAudibleRadius"), 800.f))
		{
			return true;
		}
		const FVector View = PC->GetControlRotation().Vector();
		return FVector::DotProduct(View.GetSafeNormal(), To.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(WRTune(TEXT("creatureAI.offscreenHalfAngle"), 55.f)));
	}

	EWRSwingKind SwingFor(FName Motion)
	{
		if (Motion == TEXT("lunge") || Motion == TEXT("pounce")) return EWRSwingKind::Lunge;
		if (Motion == TEXT("charge") || Motion == TEXT("roll") || Motion == TEXT("slam")) return EWRSwingKind::Charge;
		if (Motion == TEXT("spit")) return EWRSwingKind::Spit;
		if (Motion == TEXT("stomp")) return EWRSwingKind::Stomp;
		return EWRSwingKind::Strike;
	}

	/** The pack it fights with: the same spawner's living members on the same target, biggest first (the leader). */
	TArray<AWRCreature*> Pack(const AWRCreature* Self, const AWRCharacterBase* Target)
	{
		TArray<AWRCreature*> Out;
		if (!Self->Spawner.IsValid())
		{
			return Out;
		}
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (It->Spawner == Self->Spawner && It->IsAlive() && !It->IsTamed() && It->GetAggroTarget() == Target)
			{
				Out.Add(*It);
			}
		}
		Out.Sort([](const AWRCreature& A, const AWRCreature& B)
		{
			return A.GetSimpleCollisionRadius() != B.GetSimpleCollisionRadius() ? A.GetSimpleCollisionRadius() > B.GetSimpleCollisionRadius() : A.GetName() < B.GetName();
		});
		return Out;
	}
}

FVector AWRCreatureAI::GroupSpot(AWRCreature* Self, AWRCharacterBase* Target, bool& bInPlace) const
{
	// Roles by position, not permission (combat.md 2.18.4): the biggest presses from the front, the others circle to
	// their angle off the target's facing (90, -90, 150, -150), and with four or more one keeps the far side.
	bInPlace = true;
	const FWRCreatureDef* Def = Self->GetDef();
	if (!Def || (Def->Archetype != TEXT("pack_hunter") && Def->PackMax < 3))
	{
		return Target->GetActorLocation();
	}
	const TArray<AWRCreature*> Pack = WRRowFight::Pack(Self, Target);
	const int32 Index = Pack.IndexOfByKey(Self);
	if (Pack.Num() < 2 || Index <= 0)
	{
		return Target->GetActorLocation();  // the leader (or alone): straight in
	}
	const FWRTuning& T = FWRTuning::Get();
	static const float Angles[] = { 90.f, -90.f, 150.f, -150.f };
	float Angle = Angles[(Index - 1) % 4];
	if (Pack.Num() >= 4 && Index == Pack.Num() - 1)
	{
		Angle = 180.f;  // the far side
	}
	const FRotator Facing = Target->GetController() ? FRotator(0.f, Target->GetControlRotation().Yaw, 0.f) : FRotator(0.f, Target->GetActorRotation().Yaw, 0.f);
	const float Dist = FMath::Lerp(T.Num(TEXT("creatureAI.flank.inner"), 400.f), T.Num(TEXT("creatureAI.flank.outer"), 700.f), 0.5f);
	const FVector Spot = Target->GetActorLocation() + Facing.RotateVector(FVector::ForwardVector).RotateAngleAxis(Angle, FVector::UpVector) * Dist;
	bInPlace = FVector::Dist2D(Spot, Self->GetActorLocation()) < T.Num(TEXT("creatureAI.flank.tolerance"), 160.f);
	return Spot;
}

FName AWRCreatureAI::PickRow(AWRCreature* Self, AWRCharacterBase* Target, bool bReading) const
{
	const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
	if (!Rows || !Target)
	{
		return NAME_None;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector To = Target->GetActorLocation() - Self->GetActorLocation();
	const float Gap = To.Size2D() - Target->GetSimpleCollisionRadius() - Self->GetSimpleCollisionRadius();
	const float Dist = To.Size2D();
	const bool bBehind = FVector::DotProduct(Self->GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal2D()) < -0.3f;
	const float Close = WRTune(TEXT("bosses.bands.close"), 400.f), Mid = WRTune(TEXT("bosses.bands.mid"), 1200.f);
	float Total = 0.f;
	TArray<TPair<FName, float>> Options;
	for (const TPair<FName, FWRAttackRow>& Pair : *Rows)
	{
		const FWRAttackRow& R = Pair.Value;
		if (R.Motion == TEXT("withdraw") || (bReading && !R.bReadsAction))
		{
			continue;
		}
		if (!bReading && Now < RowReadyAt.FindRef(R.Id))
		{
			continue;
		}
		// Range: the gap to the body for melee rows, the distance for thrown ones; a kick only at what's behind it.
		const float Measure = R.Motion == TEXT("spit") || R.Motion == TEXT("burst") || R.Motion == TEXT("howl") ? Dist : Gap;
		if (Measure < R.MinRange || Measure > R.MaxRange || R.bBehindOnly != bBehind)
		{
			continue;
		}
		if ((R.Motion == TEXT("lunge") || R.Motion == TEXT("charge") || R.Motion == TEXT("pounce")) && Self->GetSpeedMultiplier() <= 0.f)
		{
			continue;  // rooted / frozen: no leaping or charging
		}
		if (R.Motion == TEXT("howl") && WRRowFight::Pack(Self, Target).Num() >= 4)
		{
			continue;  // a full pack doesn't call for more
		}
		const float W = Dist < Close ? R.WClose : Dist < Mid ? R.WMid : R.WFar;
		if (W > 0.f)
		{
			Options.Add({ R.Id, W });
			Total += W;
		}
	}
	float Roll = FMath::FRand() * Total;
	for (const TPair<FName, float>& O : Options)
	{
		Roll -= O.Value;
		if (Roll <= 0.f) return O.Key;
	}
	return Options.Num() > 0 ? Options.Last().Key : NAME_None;
}

bool AWRCreatureAI::StartRow(AWRCreature* Self, AWRCharacterBase* Target, FName Id, bool bCombo)
{
	const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
	const FWRAttackRow* R = Rows ? Rows->Find(Id) : nullptr;
	if (!R || !Target || !Self->IsAlive())
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	RowId = Id;
	RowPhase = 1;
	RowStarted = Now;
	RowPhaseEnd = Now + R->Windup;
	RowTarget = Target;
	RowHits.Reset();
	bRowFinalCue = false;
	RowTravelled = 0.f;
	RowCombo = bCombo ? RowCombo + 1 : 1;
	RowDir = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
	// Thrown and burst rows land where the target stood when the wind-up began (never homing, CAL 1.9).
	RowAim = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
	RowReadyAt.Add(Id, Now + R->Windup + R->Hold + R->Active + R->Punish + R->Cooldown);
	// Winding up (no backstab then), with hyperarmor on a big body from the first tell to the last strike (CAL 1.5).
	Self->SetCombatTag(WRTags::State_Windup, true);
	const FWRCreatureDef* Def = Self->GetDef();
	if (Def && Def->bHyperarmor)
	{
		Self->SetCombatTag(WRTags::State_Hyperarmor, true);
	}
	Self->SetCombatTag(WRTags::State_Attacking, true);
	// Its real clip, warped so the strike lands at wind-up + hold; the rear-back pose and the rising cue come with it.
	Self->PlaySwing(WRRowFight::SwingFor(R->Motion), R->Windup + R->Hold);
	// The tell: its voice and a small glint on the attacking part (white blockable, crimson unblockable).
	Self->MulticastTell(R->Glint, R->TellBone, TEXT("voice"));
	if (R->Motion == TEXT("burst"))
	{
		// The ground gives it away: snow, ash or dirt heaving where it will come up.
		WRFx::Broadcast(Self, TEXT("burst.tell"), RowAim, FRotator::ZeroRotator, FMath::Max(1.f, R->Radius / 150.f), TEXT("burst.tell"), R->Windup);
	}
	UE_LOG(LogWildreach, Log, TEXT("AI %s winds up %s at %s%s"), *Self->GetName(), *Id.ToString(), *Target->GetName(), bCombo ? TEXT(" (combo)") : TEXT(""));
	return true;
}

void AWRCreatureAI::EndRow(AWRCreature* Self, bool bInterrupted)
{
	const FWRCreatureDef* Def = Self->GetDef();
	Self->SpeedBoost = 1.f;
	Self->SetCombatTag(WRTags::State_Windup, false);
	Self->SetCombatTag(WRTags::State_Hyperarmor, false);
	Self->SetCombatTag(WRTags::State_Attacking, false);
	if (Self->IsHidden() && Def && Def->Archetype == TEXT("burrower"))
	{
		Self->SetActorHiddenInGame(false);
		Self->SetActorEnableCollision(true);
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	RowPhase = 0;
	RowId = NAME_None;
	if (!bInterrupted && Def && (Def->Archetype == TEXT("pack_hunter") || Def->Archetype == TEXT("ground_predator")))
	{
		// Breathing (combat.md 2.18.4): after its string and punish it backs off to circle a moment, then comes again.
		const FWRTuning& T = FWRTuning::Get();
		BreatheUntil = GetWorld()->GetTimeSeconds() + FMath::FRandRange(T.Num(TEXT("creatureAI.breathe.min"), 1.f), T.Num(TEXT("creatureAI.breathe.max"), 2.5f));
		BreatheSide = FMath::RandBool() ? 1.f : -1.f;
	}
}

void AWRCreatureAI::RowHit(AWRCreature* Self, AWRCharacterBase* Victim)
{
	const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
	const FWRAttackRow* R = Rows ? Rows->Find(RowId) : nullptr;
	if (!R || !Victim || RowHits.Contains(Victim) || !Victim->IsAlive() || !Self->IsHostileTo(Victim))
	{
		return;
	}
	RowHits.Add(Victim);
	FWRHit Hit;
	Hit.Instigator = Self;
	Hit.Damage = Self->Weapon.FlatDamage * R->Dmg * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f));
	Hit.PoiseDamage = R->Poise;
	Hit.StaminaDamage = R->StaminaDamage;
	Hit.Impact = R->Impact;
	Hit.bParryable = R->bParryable;
	Hit.bLow = R->bLow;
	Hit.bUnblockable = R->bUnblockable;
	Hit.bGrab = R->bGrab;
	Hit.bIgnoreHyperarmor = R->bIgnoreHyperarmor;
	Hit.bMelee = R->bMelee;
	Hit.bHeavy = R->Impact >= 3;
	Hit.Tier = R->Impact >= 3 ? EWRHitTier::Heavy : EWRHitTier::Light;
	Hit.PhysType = R->PhysType;
	Hit.Origin = Self->GetActorLocation();
	Hit.Element = WRTags::Element_Physical;
	for (const TPair<FName, float>& B : R->Buildups)
	{
		if (B.Key == TEXT("burn")) { Hit.BurnBuildup += B.Value; Hit.Element = WRTags::Element_Fire; }
		else if (B.Key == TEXT("frost")) { Hit.FrostBuildup += B.Value; Hit.Element = WRTags::Element_Frost; }
		else Hit.Buildups.Add(B.Key, B.Value);
	}
	UE_LOG(LogWildreach, Log, TEXT("AI %s %s hits %s"), *Self->GetName(), *RowId.ToString(), *Victim->GetName());
	const float Knockback = R->Knockback;
	TWeakObjectPtr<AWRCreature> WeakSelf(Self);
	TWeakObjectPtr<AWRCharacterBase> WeakVictim(Victim);
	UWRCombatLibrary::ApplyHit(Victim, Hit, [WeakSelf, WeakVictim, Knockback](EWRHitResult Result)
	{
		AWRCreature* S = WeakSelf.Get();
		AWRCharacterBase* V = WeakVictim.Get();
		if (S && V && V->IsAlive() && Knockback > 0.f && (Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken))
		{
			const FVector Push = (V->GetActorLocation() - S->GetActorLocation()).GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, Knockback * 0.35f);
			V->LaunchCharacter(Push, true, true);
		}
	});
}

void AWRCreatureAI::StrikeRow(AWRCreature* Self)
{
	const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
	const FWRAttackRow* R = Rows ? Rows->Find(RowId) : nullptr;
	if (!R)
	{
		return;
	}
	const FName M = R->Motion;
	AWRCharacterBase* Target = RowTarget.Get();
	RowOrigin = Self->GetActorLocation();
	RowStrikeStarted = GetWorld()->GetTimeSeconds();
	UE_LOG(LogWildreach, Log, TEXT("AI %s uses %s"), *Self->GetName(), *RowId.ToString());
	if (M == TEXT("lunge") || M == TEXT("pounce"))
	{
		// A leap at where the target will be when it lands.
		const float Flight = 0.45f;
		const FVector Aim = Target ? Target->GetActorLocation() + Target->GetVelocity() * FVector(1.f, 1.f, 0.f) * Flight : Self->GetActorLocation() + RowDir * 400.f;
		const FVector Leap = (Aim - Self->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		const float Speed = R->Speed > 0.f ? R->Speed : WRTune(TEXT("creatureAI.lunge.speed"), 1100.f);
		RowDir = Leap.GetSafeNormal();
		Self->SetActorRotation(RowDir.Rotation());
		Self->LaunchCharacter(RowDir * FMath::Min(Speed, Leap.Size() / Flight) + FVector(0.f, 0.f, WRTune(TEXT("creatureAI.lunge.lift"), 380.f)), true, true);
	}
	else if (M == TEXT("spit"))
	{
		// A glob of its element, at where the target stood when it wound up; it never homes (CAL 1.9).
		const FWRCreatureDef* Def = Self->GetDef();
		const bool bFrost = Def && Def->Element == TEXT("frost");
		const FVector From = Self->MouthLocation();
		const FVector AimAt = RowAim + FVector(0.f, 0.f, 60.f);
		FActorSpawnParameters Params;
		Params.Owner = Self;
		Params.Instigator = Self;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Glob = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, (AimAt - From).Rotation(), Params))
		{
			FWRHit Hit;
			Hit.Instigator = Self;
			Hit.Damage = Self->Weapon.FlatDamage * R->Dmg;
			Hit.PoiseDamage = R->Poise;
			Hit.Impact = R->Impact;
			Hit.bProjectile = true;
			Hit.Element = bFrost ? WRTags::Element_Frost : WRTags::Element_Fire;
			for (const TPair<FName, float>& B : R->Buildups)
			{
				if (B.Key == TEXT("burn")) Hit.BurnBuildup += B.Value;
				else if (B.Key == TEXT("frost")) Hit.FrostBuildup += B.Value;
				else Hit.Buildups.Add(B.Key, B.Value);
			}
			Glob->Init(Hit, R->ProjectileSpeed, bFrost ? FLinearColor(0.5f, 0.85f, 1.f) : FLinearColor(1.f, 0.45f, 0.05f));
		}
	}
	else if (M == TEXT("stomp"))
	{
		// The ground wave: dust and grit roll out from its feet at stomp.waveSpeed; the damage rides the front, low enough
		// to jump (combat.md 2.17.1b). TickRow checks the front.
		const float Radius = R->Radius > 0.f ? R->Radius : 320.f * FMath::Max(1.f, Self->GetSimpleCollisionRadius() / 90.f);
		WRFx::Broadcast(Self, TEXT("shockwave"), Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, Radius / 100.f, TEXT("stomp"));
	}
	else if (M == TEXT("burst"))
	{
		// Up out of the ground where the target stood.
		const float Radius = FMath::Max(150.f, R->Radius);
		WRFx::Broadcast(Self, TEXT("burst"), RowAim, FRotator(90.f, 0.f, 0.f), Radius / 120.f, TEXT("stomp"));
		if (Self->IsHidden())
		{
			Self->SetActorHiddenInGame(false);
			Self->SetActorEnableCollision(true);
			Self->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != Self && FVector::Dist2D(It->GetActorLocation(), RowAim) < Radius + It->GetSimpleCollisionRadius())
			{
				RowHit(Self, *It);
			}
		}
	}
	else if (M == TEXT("howl"))
	{
		// A call: packmates and its kind within callRadius come for the same target.
		const float Call = WRTune(TEXT("creatureAI.howl.callRadius"), 4000.f);
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (*It != Self && It->CreatureId == Self->CreatureId && It->IsAlive() && !It->IsTamed() && !It->GetAggroTarget()
				&& FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Call && Target)
			{
				It->SetAggroTarget(Target);
			}
		}
		Self->MulticastTell(0, NAME_None, TEXT("voice"));
	}
	else if (M == TEXT("dust"))
	{
		// A cloud of scales or ash in the face (blind), then it runs.
		const float Radius = FMath::Max(150.f, R->Radius);
		WRFx::Broadcast(Self, TEXT("dust.cloud"), Self->GetActorLocation(), FRotator::ZeroRotator, Radius / 120.f, TEXT("hit.dust"));
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It != Self && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
			{
				RowHit(Self, *It);
			}
		}
	}
	else if (M == TEXT("charge") || M == TEXT("roll") || M == TEXT("slam"))
	{
		const FWRCreatureDef* Def = Self->GetDef();
		Self->SpeedBoost = R->Speed > 0.f && Def ? R->Speed / FMath::Max(1.f, Def->Speed) : WRTune(TEXT("creatureAI.charge.speedMult"), 2.f);
	}
}

void AWRCreatureAI::TickRow(AWRCreature* Self, float DeltaSeconds)
{
	const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Self->CreatureId);
	const FWRAttackRow* R = Rows ? Rows->Find(RowId) : nullptr;
	AWRCharacterBase* Target = RowTarget.Get();
	if (!R)
	{
		EndRow(Self, true);
		return;
	}
	// A blow that broke its poise (it had no hyperarmor), a freeze, a stance break or a crit cuts it short.
	if (Self->HasTag(WRTags::Status_Staggered) || Self->HasTag(WRTags::Status_Frozen) || Self->HasTag(WRTags::State_BeingCrit))
	{
		UE_LOG(LogWildreach, Log, TEXT("AI %s's %s interrupted"), *Self->GetName(), *RowId.ToString());
		EndRow(Self, true);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FName M = R->Motion;
	if (RowPhase == 1 || RowPhase == 2)
	{
		// Wind-up: feet planted, turning toward the target only through its first part (180 deg/s), so a late step aside
		// still makes it miss; then committed. A charge commits to its line at once.
		const float Track = M == TEXT("charge") || M == TEXT("roll") ? 0.15f : R->TrackUntil;
		if (RowPhase == 1 && Target && Now - RowStarted < R->Windup * Track)
		{
			RowDir = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
			Self->SetActorRotation(FMath::RInterpConstantTo(Self->GetActorRotation(), RowDir.Rotation(), DeltaSeconds, WRTune(TEXT("creature.biteTrackRate"), 180.f)));
		}
		else if (!RowDir.IsNearlyZero())
		{
			RowDir = Self->GetActorForwardVector().GetSafeNormal2D();
		}
		// A burrower travels under the ground to where it will come up.
		if (M == TEXT("burst") && Self->IsHidden())
		{
			Self->SetActorLocation(FMath::VInterpConstantTo(Self->GetActorLocation(), RowAim + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), DeltaSeconds, 1200.f), false);
		}
		// The final cue on a delayed strike: a second glint and a sting 0.2 s before contact (never a cheat).
		const double Contact = RowStarted + R->Windup + R->Hold;
		if (R->Hold > 0.f && !bRowFinalCue && Now >= Contact - WRTune(TEXT("creatureAI.finalCueSeconds"), 0.2f))
		{
			bRowFinalCue = true;
			Self->MulticastTell(R->Glint, R->TellBone, TEXT("tell.final"));
		}
		if (RowPhase == 1 && Now >= RowPhaseEnd)
		{
			RowPhase = 2;
			RowPhaseEnd = Contact;
		}
		if (RowPhase == 2 && Now >= RowPhaseEnd)
		{
			RowPhase = 3;
			RowPhaseEnd = Now + R->Active;
			Self->SetCombatTag(WRTags::State_Windup, false);
			StrikeRow(Self);
		}
		return;
	}
	if (RowPhase == 3)
	{
		const float BodyR = Self->GetSimpleCollisionRadius();
		if (M == TEXT("bite") || M == TEXT("claw") || M == TEXT("sweep") || M == TEXT("kick") || M == TEXT("buffet") || M == TEXT("snap"))
		{
			// Damage only while the jaw, claw or hoof really travels: bodies inside its reach and arc (a kick: behind it).
			const FVector Fwd = M == TEXT("kick") ? -Self->GetActorForwardVector().GetSafeNormal2D() : Self->GetActorForwardVector().GetSafeNormal2D();
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It) || (It->HasTag(WRTags::Status_Unconscious) && *It != Target)) continue;
				const FVector To = It->GetActorLocation() - Self->GetActorLocation();
				if (To.Size2D() - It->GetSimpleCollisionRadius() - BodyR <= R->Reach
					&& FVector::DotProduct(Fwd, To.GetSafeNormal2D()) >= FMath::Cos(FMath::DegreesToRadians(R->Arc)) && FMath::Abs(To.Z) < Self->GetSimpleCollisionHalfHeight() + It->GetSimpleCollisionHalfHeight())
				{
					RowHit(Self, *It);
				}
			}
		}
		else if (M == TEXT("lunge") || M == TEXT("pounce") || M == TEXT("charge") || M == TEXT("roll") || M == TEXT("slam"))
		{
			if (M != TEXT("lunge") && M != TEXT("pounce"))
			{
				Self->AddMovementInput(RowDir, 1.f);
				RowTravelled += Self->GetVelocity().Size2D() * DeltaSeconds;
				// Into a wall or a rock at speed: stopped dead and stunned (MH's wall stun; Emberhorn, Cinderhorn).
				FHitResult Wall;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(WRChargeWall), false, Self);
				if (R->WallStun > 0.f && RowTravelled > 200.f && GetWorld()->SweepSingleByChannel(Wall, Self->GetActorLocation(), Self->GetActorLocation() + RowDir * (BodyR + 40.f),
					FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(BodyR * 0.6f), Q))
				{
					UE_LOG(LogWildreach, Log, TEXT("AI %s charged into a wall"), *Self->GetName());
					WRFx::Broadcast(Self, TEXT("stomp"), Wall.ImpactPoint, FRotator::ZeroRotator, 1.2f, TEXT("stomp"));
					EndRow(Self, true);
					Self->Status->Stagger(R->WallStun);
					return;
				}
			}
			// The first hostile body it reaches takes it (passing over knocked-out ones).
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && (!It->HasTag(WRTags::Status_Unconscious) || *It == Target)
					&& FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < BodyR + It->GetSimpleCollisionRadius() + R->Reach)
				{
					RowHit(Self, *It);
					if (M == TEXT("charge")) RowPhaseEnd = FMath::Min(RowPhaseEnd, Now + 0.05);  // a charge stops in what it hits
					break;
				}
			}
			if ((M == TEXT("lunge") || M == TEXT("pounce")) && Now - RowStrikeStarted > 0.3 && !Self->GetCharacterMovement()->IsFalling())
			{
				RowPhaseEnd = FMath::Min(RowPhaseEnd, Now);  // landed
			}
		}
		else if (M == TEXT("stomp"))
		{
			// The wave's front: bodies it passes take it once; it keeps rolling past the active window to its radius.
			const float Radius = R->Radius > 0.f ? R->Radius : 320.f * FMath::Max(1.f, BodyR / 90.f);
			const float Front = float(Now - RowStrikeStarted) * WRTune(TEXT("creatureAI.stomp.waveSpeed"), 1200.f);
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It == Self || !It->IsAlive() || !Self->IsHostileTo(*It)) continue;
				const float D = FVector::Dist2D(It->GetActorLocation(), RowOrigin) - It->GetSimpleCollisionRadius();
				if (D <= FMath::Min(Front, Radius) && D >= Front - WRTune(TEXT("creatureAI.stomp.waveDepth"), 150.f) - 60.f)
				{
					RowHit(Self, *It);
				}
			}
			RowPhaseEnd = FMath::Max(RowPhaseEnd, RowStrikeStarted + Radius / WRTune(TEXT("creatureAI.stomp.waveSpeed"), 1200.f));
		}
		if (Now >= RowPhaseEnd)
		{
			Self->SpeedBoost = 1.f;
			RowPhase = 4;
			RowPhaseEnd = Now + R->Punish;
			Self->SetCombatTag(WRTags::State_Hyperarmor, false);
		}
		return;
	}
	if (RowPhase == 4)
	{
		// The punish window: it stands, open, facing where it struck.
		if (Now < RowPhaseEnd)
		{
			return;
		}
		// A combo's next hit (at most 4 in a string, CAL 1.13), else breathe.
		if (!R->Next.IsNone() && RowCombo < 4 && FMath::FRand() < R->NextChance && Target && Target->IsAlive())
		{
			const FName Next = R->Next;
			RowPhase = 0;
			if (StartRow(Self, Target, Next, true))
			{
				return;
			}
		}
		EndRow(Self, false);
	}
}

void AWRCreatureAI::TickRowFight(AWRCreature* Self, AWRCharacterBase* Target, float DeltaSeconds)
{
	const FWRTuning& T = FWRTuning::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	if (RowPhase != 0)
	{
		TickRow(Self, DeltaSeconds);
		return;
	}
	const FWRCreatureDef* Def = Self->GetDef();
	const FVector To = Target->GetActorLocation() - Self->GetActorLocation();
	const float Dist = To.Size2D();

	// Shelled up after taking a beating (Basaltback, Rimeshell): a quarter of its stance gone in two seconds.
	if (UWRAttributeSet* A = Self->GetAttributes(); A && FWRAttackRows::Get().Creature(Self->CreatureId) && FWRAttackRows::Get().Creature(Self->CreatureId)->Contains(TEXT("withdraw")))
	{
		if (LastStance < 0.f) LastStance = A->GetStance();
		if (Now - StanceWindowStart > T.Num(TEXT("creatureAI.withdraw.window"), 2.f))
		{
			StanceWindowStart = Now;
			StanceWindow = 0.f;
		}
		StanceWindow += FMath::Max(0.f, LastStance - A->GetStance());
		LastStance = A->GetStance();
		if (StanceWindow >= A->GetMaxStance() * T.Num(TEXT("creatureAI.withdraw.share"), 0.25f) && Now >= Self->WithdrawnUntil)
		{
			Self->WithdrawnUntil = Now + T.Num(TEXT("creatureAI.withdraw.seconds"), 3.f);
			StanceWindow = 0.f;
			Self->MulticastTell(0, NAME_None, TEXT("voice"));
			UE_LOG(LogWildreach, Log, TEXT("AI %s withdraws into its shell"), *Self->GetName());
		}
		if (Now < Self->WithdrawnUntil)
		{
			return;  // tucked in: no moving, no attacking
		}
	}

	// readsAction (highland creatures, CAL 1.13): a drink, a cast or a charge started in reach is answered at once.
	const bool bReadable = Target->HasTag(WRTags::State_Drinking) || Target->HasTag(WRTags::State_Casting) || Target->HasTag(WRTags::State_Charging);
	if (bReadable && Now - ReadUsedAt > T.Num(TEXT("creatureAI.readsActionCooldown"), 6.f) && WRRowFight::CanSee(Self, Target))
	{
		const FName Read = PickRow(Self, Target, true);
		if (!Read.IsNone() && StartRow(Self, Target, Read, false))
		{
			ReadUsedAt = Now;
			UE_LOG(LogWildreach, Log, TEXT("AI %s reads %s's action"), *Self->GetName(), *Target->GetName());
			return;
		}
	}

	// Breathing: back off to 5-8 m and circle, then come again (a gap a skilled player uses, not a turn order).
	if (Now < BreatheUntil)
	{
		const float Want = FMath::Lerp(T.Num(TEXT("creatureAI.breathe.distance.min"), 500.f), T.Num(TEXT("creatureAI.breathe.distance.max"), 800.f), 0.5f);
		const FVector Radial = -To.GetSafeNormal2D();
		const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Radial) * BreatheSide;
		const FVector Dir = (Tangent + Radial * FMath::Clamp((Want - Dist) / 200.f, -1.f, 1.f)).GetSafeNormal2D();
		Self->AddMovementInput(Dir, 0.6f);
		SetControlRotation(To.Rotation());
		return;
	}

	// Position: the leader goes straight in; flankers take their angle first, and strike from there (or when the target
	// turns its back).
	bool bInPlace = true;
	const FVector Spot = GroupSpot(Self, Target, bInPlace);
	const bool bFacingAway = FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), -To.GetSafeNormal2D()) < 0.f;
	const FName Pick = (bInPlace || bFacingAway) && WRRowFight::CanSee(Self, Target) ? PickRow(Self, Target, false) : NAME_None;
	if (!Pick.IsNone())
	{
		SetControlRotation(To.Rotation());
		StartRow(Self, Target, Pick, false);
		return;
	}
	// Move: to its spot, or in on the target; flyers circle high and come down to strike.
	const FVector Goal = bInPlace ? Target->GetActorLocation() : Spot;
	FVector Dir = (Goal - Self->GetActorLocation()).GetSafeNormal2D();
	const float Stop = Self->GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius() + (Def ? FMath::Min(Def->AttackRange * 0.5f, 120.f) : 80.f);
	if (Def && Def->Archetype == TEXT("flyer") && !Self->HasTag(WRTags::Status_Staggered))
	{
		const float WantZ = Target->GetActorLocation().Z + (Dist > 700.f ? 450.f : 0.f);
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Dir.Z = FMath::Clamp((WantZ - Self->GetActorLocation().Z) / 300.f, -1.f, 1.f);
	}
	if (!bInPlace || Dist > Stop)
	{
		Self->AddMovementInput(Dir, 1.f);
	}
	else
	{
		SetControlRotation(To.Rotation());
	}
}

// ---------------------------------------------------------------- the saddle

void AWRCreature::RideLightPressed()
{
	if (WRInput::AllowPress(Cast<APlayerController>(GetController())))
	{
		ServerRideAttack(false, false);
	}
}

void AWRCreature::RideHeavyPressed()
{
	if (WRInput::AllowPress(Cast<APlayerController>(GetController())))
	{
		ServerRideAttack(true, true);
	}
}

void AWRCreature::RideHeavyReleased()
{
	ServerRideAttackRelease();
}

void AWRCreature::ServerRideAttack_Implementation(bool bHeavy, bool bHeld)
{
	AWRCharacter* Hero = Rider.Get();
	const FWRTuning& T = FWRTuning::Get();
	if (!Hero || !IsAlive() || Hero->HasTag(WRTags::State_Attacking) || HasTag(WRTags::Status_Staggered))
	{
		return;
	}
	const FWRWeaponClass& Class = FWRWeapons::Get().ClassFor(Hero);
	// The couched spear (combat.md 2.19): holding Heavy with a spear at a gallop lowers it; the first body in its line
	// takes the mount's speed.
	if (bHeavy && Class.Id == TEXT("spear") && GetVelocity().Size2D() >= T.Num(TEXT("mounted.couchSpeed"), 900.f))
	{
		CouchUntil = GetWorld()->GetTimeSeconds() + T.Num(TEXT("mounted.couchSeconds"), 1.5f);
		bCouchHit = false;
		Hero->PlaySwing(EWRSwingKind::Heavy, 1.5f, 0, 0.2f, 1.3f, 0.3f);
		Hero->SetCombatTag(WRTags::State_Attacking, true);
		UE_LOG(LogWildreach, Log, TEXT("RIDE %s couches the spear"), *Hero->GetName());
		return;
	}
	// A swing to the side the camera faces (left or right of the mount), or the mount rears and the rider strikes
	// down over its head (heavy). They count as running attacks (stance x1.2 / x2.8).
	const FWRMove* Row = Class.Move(bHeavy ? TEXT("runHeavy") : TEXT("runLight"));
	if (!Row)
	{
		return;
	}
	const FRotator View = GetController() ? GetController()->GetControlRotation() : GetActorRotation();
	const float Side = FVector::DotProduct(View.Vector().GetSafeNormal2D(), GetActorRightVector()) >= 0.f ? 1.f : -1.f;
	Hero->PlaySwing(bHeavy ? EWRSwingKind::Heavy : EWRSwingKind::Light, Row->Windup + Row->Active, uint8(Row->Clip), Row->Windup, Row->Active, Row->Recovery);
	Hero->SetCombatTag(WRTags::State_Attacking, true);
	if (bHeavy)
	{
		PlaySwing(EWRSwingKind::Strike, Row->Windup);  // the mount rears with it
	}
	RideHits.Reset();
	TWeakObjectPtr<AWRCreature> Weak(this);
	const FName RowName = bHeavy ? FName(TEXT("runHeavy")) : FName(TEXT("runLight"));
	FTimerHandle Strike;
	GetWorldTimerManager().SetTimer(Strike, FTimerDelegate::CreateWeakLambda(this, [Weak, Side, bHeavy, RowName]()
	{
		AWRCreature* Mount = Weak.Get();
		AWRCharacter* H = Mount ? Mount->Rider.Get() : nullptr;
		if (!Mount || !H)
		{
			return;
		}
		const FWRWeaponClass& C = FWRWeapons::Get().ClassFor(H);
		const FWRMove* M = C.Move(RowName);
		const FVector Out = bHeavy ? Mount->GetActorForwardVector() : Mount->GetActorRightVector() * Side;
		const float Reach = C.Reach + Mount->GetSimpleCollisionRadius() * (bHeavy ? 1.f : 0.5f);
		for (TActorIterator<AWRCharacterBase> It(Mount->GetWorld()); It; ++It)
		{
			if (*It == Mount || *It == H || !It->IsAlive() || !H->IsHostileTo(*It)) continue;
			const FVector To = It->GetActorLocation() - Mount->GetActorLocation();
			if (To.Size2D() - It->GetSimpleCollisionRadius() > Reach || FVector::DotProduct(To.GetSafeNormal2D(), Out) < 0.35f) continue;
			FWRHit Hit;
			Hit.Instigator = H;
			Hit.Damage = UWRCombatLibrary::WeaponDamage(H->Weapon, bHeavy) * (M ? M->Damage : 1.f);
			Hit.PoiseDamage = M ? M->Poise : 30.f;
			Hit.StanceDamage = M ? M->Stance : 6.f;
			Hit.StaminaDamage = M ? M->GuardStamina : 30.f;
			Hit.Impact = M ? M->Impact : 2;
			Hit.Tier = bHeavy ? EWRHitTier::Heavy : EWRHitTier::Light;
			Hit.PhysType = M ? M->PhysType : 1;
			Hit.bMelee = true;
			Hit.bHeavy = bHeavy;
			Hit.bParryable = true;
			Hit.Element = WRTags::Element_Physical;
			Hit.Origin = Mount->GetActorLocation();
			UWRCombatLibrary::ApplyHit(*It, Hit);
		}
	}), Row->Windup + Row->Active * 0.5f, false);
	FTimerHandle Done;
	GetWorldTimerManager().SetTimer(Done, FTimerDelegate::CreateWeakLambda(this, [Weak]()
	{
		if (AWRCreature* Mount = Weak.Get(); Mount && Mount->Rider.IsValid())
		{
			Mount->Rider->SetCombatTag(WRTags::State_Attacking, false);
		}
	}), Row->Windup + Row->Active + Row->Recovery, false);
}

void AWRCreature::ServerRideAttackRelease_Implementation()
{
	if (CouchUntil > 0.0)
	{
		CouchUntil = 0.0;
		if (Rider.IsValid()) Rider->SetCombatTag(WRTags::State_Attacking, false);
	}
}

void AWRCreature::TickRider(float DeltaSeconds)
{
	AWRCharacter* Hero = Rider.Get();
	if (!Hero)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	// The couched spear: the first hostile body in its line while the mount keeps its speed.
	if (CouchUntil > 0.0)
	{
		if (Now > CouchUntil || bCouchHit || GetVelocity().Size2D() < T.Num(TEXT("mounted.couchSpeed"), 900.f) * 0.8f)
		{
			CouchUntil = 0.0;
			Hero->SetCombatTag(WRTags::State_Attacking, false);
		}
		else
		{
			const FVector Tip = GetActorLocation() + GetActorForwardVector() * (GetSimpleCollisionRadius() + T.Num(TEXT("mounted.couchReach"), 260.f));
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It == this || *It == Hero || !It->IsAlive() || !Hero->IsHostileTo(*It)) continue;
				FVector OnLine, OnBody;
				FMath::SegmentDistToSegmentSafe(GetActorLocation(), Tip, It->GetActorLocation(), It->GetActorLocation(), OnLine, OnBody);
				if (FVector::Dist(OnLine, OnBody) < It->GetSimpleCollisionRadius() + T.Num(TEXT("mounted.couchRadius"), 60.f))
				{
					const FWRMove* Heavy = FWRWeapons::Get().ClassFor(Hero).Move(TEXT("heavy"));
					FWRHit Hit;
					Hit.Instigator = Hero;
					Hit.Damage = UWRCombatLibrary::WeaponDamage(Hero->Weapon, true) * T.Num(TEXT("mounted.couchDamage"), 2.f);
					Hit.PoiseDamage = (Heavy ? Heavy->Poise : 48.f) * 2.f;
					Hit.StanceDamage = (Heavy ? Heavy->Stance : 10.f) * T.Num(TEXT("mounted.couchStance"), 4.f) * 0.5f;
					Hit.Impact = 5;
					Hit.Tier = EWRHitTier::Counter;
					Hit.PhysType = 3;
					Hit.bMelee = true;
					Hit.bHeavy = true;
					Hit.Origin = GetActorLocation();
					UWRCombatLibrary::ApplyHit(*It, Hit);
					bCouchHit = true;
					break;
				}
			}
		}
	}
	// The mount fights too: an aggressive tame bites its rider's lock target when it's in reach.
	const AWRPlayerController* PC = Cast<AWRPlayerController>(GetController());
	AWRCharacterBase* Lock = PC ? PC->GetServerLock() : nullptr;
	const FWRCreatureDef* Def = GetDef();
	if (Lock && Lock->IsAlive() && bTameAggressive && Def && Now >= NextRiderBite && IsHostileTo(Lock)
		&& FVector::Dist2D(Lock->GetActorLocation(), GetActorLocation()) - Lock->GetSimpleCollisionRadius() - GetSimpleCollisionRadius() < Def->AttackRange * 0.6f)
	{
		NextRiderBite = Now + T.Num(TEXT("mounted.mountBiteSeconds"), 2.f);
		PlaySwing(EWRSwingKind::Strike, 0.45f);
		TWeakObjectPtr<AWRCreature> Weak(this);
		TWeakObjectPtr<AWRCharacterBase> WeakLock(Lock);
		FTimerHandle Bite;
		GetWorldTimerManager().SetTimer(Bite, FTimerDelegate::CreateWeakLambda(this, [Weak, WeakLock]()
		{
			AWRCreature* Mount = Weak.Get();
			AWRCharacterBase* Victim = WeakLock.Get();
			if (!Mount || !Victim || !Victim->IsAlive()) return;
			FWRHit Hit;
			Hit.Instigator = Mount;
			Hit.Damage = Mount->Weapon.FlatDamage;
			Hit.PoiseDamage = 25.f;
			Hit.Impact = 2;
			Hit.bMelee = true;
			Hit.bParryable = true;
			Hit.Origin = Mount->GetActorLocation();
			UWRCombatLibrary::ApplyHit(Victim, Hit);
		}), 0.45f, false);
	}
}

void AWRCreature::Unhorse(AActor* By)
{
	AWRCharacter* Hero = Rider.Get();
	if (!HasAuthority() || !Hero)
	{
		return;
	}
	UE_LOG(LogWildreach, Log, TEXT("RIDE %s thrown from %s by %s"), *Hero->GetName(), *GetName(), *GetNameSafe(By));
	Dismount();
	// Thrown to the ground (combat.md 2.19): knocked down a moment (the wake-roll comes with the knockdown clips).
	if (Hero->IsAlive())
	{
		const FVector Away = (Hero->GetActorLocation() - (By ? By->GetActorLocation() : GetActorLocation())).GetSafeNormal2D();
		Hero->LaunchCharacter(Away * 350.f + FVector(0.f, 0.f, 250.f), true, true);
		Hero->Status->Stagger(WRTune(TEXT("mounted.unhorseKnockdown"), 1.2f));
		WRFx::Broadcast(Hero, TEXT("land.dust"), Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.4f, TEXT("land"));
	}
}
