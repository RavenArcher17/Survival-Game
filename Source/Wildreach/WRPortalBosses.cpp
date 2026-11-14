// The portal bosses' arena mechanics (cooks/endgame.md 2.5.6; cooks/build-portals-artifacts.md 2.6). Their attacks are
// rows in Data/attacks.json and their numbers tuning bosses.<id>; this brain (server, on the boss) is the one counter
// each has, built from what its arena holds: the Alpha's mane and the lava runnels, the Broodqueen's clutches, the
// Mould-Born's crucibles and its cooling, Brannoch's bellows vents, Böðvildr's quench tanks, the Rimeclaw Alpha's ice
// plates, the Elder and the thick ice, the Thurs's shell, the Rime-Witch's braziers, Hrímgerðr's dawn window.
// An escaped boss carries the brain too (bEscaped): no arena, it roams within 400 m of its broken portal.

#include "WRPortals.h"
#include "WRPortalsInternal.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

using namespace WRPortalsImpl;

namespace WRBrainImpl
{
	float BrainTune(FName Boss, const TCHAR* Key, float Fallback) { return WRTune(*FString::Printf(TEXT("bosses.%s.%s"), *Boss.ToString(), Key), Fallback); }
	bool BrainIsElement(const FWRHit& Hit, const FGameplayTag& Tag) { return Hit.Element == Tag || (Tag == WRTags::Element_Fire && Hit.BurnBuildup > 0.f) || (Tag == WRTags::Element_Frost && Hit.FrostBuildup > 0.f); }
}
using namespace WRBrainImpl;

UWRPortalBossBrain::UWRPortalBossBrain()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
}

FName UWRPortalBossBrain::Id() const { const AWRCreature* C = Cast<AWRCreature>(GetOwner()); return C ? C->CreatureId : NAME_None; }
AWRBoss* UWRPortalBossBrain::Boss() const { return Cast<AWRBoss>(GetOwner()); }
double UWRPortalBossBrain::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0; }

void UWRPortalBossBrain::Stagger(float Seconds)
{
	if (AWRBoss* B = Boss()) UWRCombatLibrary::BreakStance(B, Seconds, nullptr);
}

void UWRPortalBossBrain::Summon(FName Kind, int32 Count, float Ring)
{
	AWRBoss* B = Boss();
	AWRFold* F = Fold.Get();
	if (!B) return;
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return;
	float H = 1.f, D = 1.f, S = 1.f;
	if (F) WRPortalsSys::PsScaleFor(F->Rank, Def->Tier, H, D, S);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector At = B->GetActorLocation() + FRotator(0.f, 360.f * i / FMath::Max(1, Count) + FMath::FRandRange(-20.f, 20.f), 0.f).Vector() * Ring - FVector(0, 0, B->GetSimpleCollisionHalfHeight());
		if (AWRCreature* C = WRPortalsSys::PsSpawnScaled(GetWorld(), Kind, At, B->GetActorRotation().Yaw, H, D, S, FColor(0, 0, 0, 0)))
		{
			C->SetHome(B->GetHome());
			if (AActor* T = B->GetAggroTarget()) C->SetAggroTarget(T);
			Adds.Add(C);
			if (F) F->RoomCreatures.FindOrAdd(Room).Add(C);
		}
	}
}

void UWRPortalBossBrain::Spawned()
{
	AWRBoss* B = Boss();
	AWRFold* F = Fold.Get();
	if (!B || !F || Room == INDEX_NONE) return;
	const FName Me = Id();
	const TSharedPtr<FJsonObject> Arena = PChild(F->RoomRow(Room), TEXT("arena"));
	const FWRFoldRoomRep& R = F->Rooms[Room];
	const float Yaw = F->RoomFrame(Room).Rotator().Yaw;
	const auto Make = [&](FName Kind, const FVector& At, TFunctionRef<void(AWRFoldThing*)> Set) -> AWRFoldThing*
	{
		const FTransform T(FRotator(0.f, Yaw, 0.f), At);
		AWRFoldThing* Th = GetWorld()->SpawnActorDeferred<AWRFoldThing>(AWRFoldThing::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Th) return nullptr;
		Th->Kind = Kind;
		Th->Biome = F->Biome;
		Th->Fold = F;
		Th->Room = uint8(Room);
		Set(Th);
		Th->FinishSpawning(T);
		Props.Add(Th);
		return Th;
	};
	const auto Each = [&](const TCHAR* Field, TFunctionRef<void(const TSharedPtr<FJsonValue>&)> Fn)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* L = PArr(Arena, Field)) for (const TSharedPtr<FJsonValue>& V : *L) Fn(V);
	};
	if (Me == TEXT("kilnmite_broodqueen"))
	{
		Each(TEXT("clutches"), [&](const TSharedPtr<FJsonValue>& V) { Make(TEXT("clutch"), F->RoomPoint(Room, PAt(V, 0), PAt(V, 1)), [&](AWRFoldThing* T) { T->Health = BrainTune(Me, TEXT("clutchHealth"), 200.f); }); });
		NextLay = Now() + BrainTune(Me, TEXT("layEvery"), 20.f);
	}
	else if (Me == TEXT("mould_born"))
	{
		Each(TEXT("crucibles"), [&](const TSharedPtr<FJsonValue>& V) { Make(TEXT("crucible"), F->RoomPoint(Room, PAt(V, 0), PAt(V, 1)), [&](AWRFoldThing* T) { T->Size = FVector(0, 0, R.H - 380.f); }); });
	}
	else if (Me == TEXT("bodvildr"))
	{
		const TSharedPtr<FJsonValue> Cistern = Arena.IsValid() ? Arena->Values.FindRef(TEXT("cistern")) : nullptr;
		const FVector CisternAt = F->RoomPoint(Room, PAt(Cistern, 0, R.W / 100.f), PAt(Cistern, 1, 0.f));
		Each(TEXT("tanks"), [&](const TSharedPtr<FJsonValue>& V) { Make(TEXT("tank"), F->RoomPoint(Room, PAt(V, 0), PAt(V, 1)), [&](AWRFoldThing* T) { T->Goal = CisternAt; }); });
	}
	else if (Me == TEXT("rime_witch"))
	{
		Each(TEXT("braziers"), [&](const TSharedPtr<FJsonValue>& V) { Make(TEXT("arenabrazier"), F->RoomPoint(Room, PAt(V, 0), PAt(V, 1)), [&](AWRFoldThing* T) { T->State = 1; }); });
		NextSnuff = Now() + BrainTune(Me, TEXT("snuffEvery"), 25.f);
	}
	else if (Me == TEXT("hrimgerdr"))
	{
		const TSharedPtr<FJsonValue> Win = Arena.IsValid() ? Arena->Values.FindRef(TEXT("window")) : nullptr;
		Make(TEXT("window"), F->RoomPoint(Room, PAt(Win, 0, R.W / 100.f) - 0.6f, PAt(Win, 1, 0.f)), [&](AWRFoldThing* T) { T->Size = FVector(60.f, R.D - 200.f, R.H - 60.f); T->State = 0; });
		int32 b = 0;
		Each(TEXT("bolts"), [&](const TSharedPtr<FJsonValue>& V) { Make(TEXT("bolt"), F->RoomPoint(Room, PAt(V, 0), PAt(V, 1), PAt(V, 2) * 100.f), [&](AWRFoldThing* T) { T->Health = BrainTune(Me, TEXT("boltHealth"), 600.f); T->Sub = uint8(b++); }); });
	}
	else if (Me == TEXT("frostbound_thurs"))
	{
		bShell = true;
		ShellAt = Now();
	}
	UE_LOG(LogWildreach, Log, TEXT("PORTAL boss %s arena: %d props"), *Me.ToString(), Props.Num());
}

float UWRPortalBossBrain::Incoming(const FWRHit& Hit, float Damage)
{
	const FName Me = Id();
	const double T = Now();
	if (Me == TEXT("cinderfang_alpha"))
	{
		// Frost or a soaking puts the mane out for a while.
		if (bManeLit && (BrainIsElement(Hit, WRTags::Element_Frost) || Hit.Buildups.Contains(TEXT("soak"))))
		{
			bManeLit = false;
			ManeOutUntil = T + BrainTune(Me, TEXT("douseSeconds"), 15.f);
			if (AWRCreature* C = Cast<AWRCreature>(GetOwner())) C->PortalMark.A = 50;
			WRFx::Broadcast(GetOwner(), TEXT("reaction.steam"), GetOwner()->GetActorLocation() + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.5f, TEXT("portal.douse"));
		}
	}
	else if (Me == TEXT("mould_born"))
	{
		// Still cooling from its mould: frost hardens it, fire feeds it.
		if (BrainIsElement(Hit, WRTags::Element_Frost)) HardenedUntil = T + BrainTune(Me, TEXT("frostSeconds"), 8.f);
		if (BrainIsElement(Hit, WRTags::Element_Fire) && !Hit.bDamageOverTime)
		{
			if (AWRBoss* B = Boss(); B && B->GetAttributes())
			{
				const UWRAttributeSet* A = B->GetAttributes();
				B->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + A->GetMaxHealth() * BrainTune(Me, TEXT("fireHeal"), 0.01f)));
			}
			return 0.f;
		}
		if (T < HardenedUntil) Damage *= BrainTune(Me, TEXT("frostTaken"), 0.8f);
	}
	else if (Me == TEXT("bodvildr"))
	{
		if (T < CooledUntil) Damage *= BrainTune(Me, TEXT("cooledTaken"), 1.3f);
	}
	else if (Me == TEXT("rimeclaw_alpha"))
	{
		if (BrainIsElement(Hit, WRTags::Element_Fire) && T >= PlatesMeltedUntil)
		{
			PlatesMeltedUntil = T + BrainTune(Me, TEXT("meltSeconds"), 20.f);
			WRFx::Broadcast(GetOwner(), TEXT("reaction.steam"), GetOwner()->GetActorLocation() + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.5f, TEXT("portal.douse"));
		}
	}
	else if (Me == TEXT("frostbound_thurs"))
	{
		if (bShell) Damage *= BrainTune(Me, TEXT("shellTaken"), 0.4f);
	}
	else if (Me == TEXT("hrimgerdr"))
	{
		if (StunnedUntil > T) Damage *= BrainTune(Me, TEXT("dawnTaken"), 1.5f);   // in the dawn: her skin turning to stone
	}
	return Damage;
}

float UWRPortalBossBrain::Outgoing() const
{
	const FName Me = Id();
	if (Me == TEXT("cinderfang_alpha") && bManeLit) return BrainTune(Me, TEXT("maneDamage"), 1.2f);
	if (Me == TEXT("brannoch") && VentsBroken == 1 && CloneUntil > Now()) return BrainTune(Me, TEXT("oneVentCone"), 0.5f);
	return 1.f;
}

void UWRPortalBossBrain::OnAttack(FName AttackId)
{
	AWRBoss* B = Boss();
	if (!B) return;
	const FName Me = Id();
	const double T = Now();
	// Brannoch's bellows breath: halved through one broken vent (CloneUntil marks the cone's breath).
	if (Me == TEXT("brannoch")) CloneUntil = AttackId == TEXT("bellows_cone") ? T + 3.0 : 0.0;
	if (Me == TEXT("cinderfang_alpha") && AttackId == TEXT("howl")) Summon(TEXT("cinderfang"), FMath::RoundToInt(BrainTune(Me, TEXT("howlCalls"), 2.f)), 500.f);
	if (Me == TEXT("kilnmite_broodqueen") && AttackId == TEXT("egg_lay")) NextLay = T;
	// Moves that land where the target stood as the wind-up began: their arena answer is judged at the landing.
	AActor* Target = B->GetAggroTarget();
	const FVector Aim = Target ? Target->GetActorLocation() : B->GetActorLocation() + B->GetActorForwardVector() * 600.f;
	const TWeakObjectPtr<UWRPortalBossBrain> Weak = this;
	if (Me == TEXT("bodvildr") && AttackId == TEXT("overhead_slam"))
	{
		FTimerHandle H;
		GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak, Aim]()
		{
			UWRPortalBossBrain* Br = Weak.Get();
			if (!Br) return;
			const FName Id = Br->Id();
			for (const TWeakObjectPtr<AWRFoldThing>& W : Br->Props)
			{
				AWRFoldThing* Tank = W.Get();
				if (!Tank || Tank->Kind != TEXT("tank") || Tank->State != 0 || FVector::Dist2D(Tank->GetActorLocation(), Aim) > BrainTune(Id, TEXT("tankReach"), 450.f)) continue;
				// Her hammer on the rim: the tank tips, the steam takes her off her feet and cools her armour.
				Tank->State = 1;
				Tank->TimeA = float(WRFoldHazards::Now(Tank)) + BrainTune(Id, TEXT("tankRefill"), 60.f);
				WRFx::Broadcast(Tank, TEXT("reaction.steam"), Tank->GetActorLocation() + FVector(0, 0, 150.f), FRotator::ZeroRotator, 4.f, TEXT("portal.quench"));
				Br->Stagger(BrainTune(Id, TEXT("tankStagger"), 8.f));
				Br->CooledUntil = Br->Now() + BrainTune(Id, TEXT("cooledSeconds"), 12.f);
				break;
			}
		}), 1.15f, false);
	}
	if (Me == TEXT("glacimaw_elder") && AttackId == TEXT("burrow_erupt"))
	{
		FTimerHandle H;
		GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak, Aim]()
		{
			UWRPortalBossBrain* Br = Weak.Get();
			AWRFold* F = Br ? Br->Fold.Get() : nullptr;
			if (!F) return;
			// Erupting under someone on the dark, thick ice: it breaks its skull on it.
			for (const FBox& Ice : F->ThickIce)
			{
				if (Ice.ExpandBy(FVector(60.f, 60.f, 400.f)).IsInsideOrOn(Aim))
				{
					Br->Stagger(BrainTune(Br->Id(), TEXT("thickStun"), 3.f));
					WRFx::Broadcast(Br->GetOwner(), TEXT("hit.ice"), Aim, FRotator::ZeroRotator, 3.f, TEXT("portal.ice_break"));
					break;
				}
			}
		}), 1.05f, false);
	}
}

void UWRPortalBossBrain::OnPart(FName PartId, FName OnBreak)
{
	if (Id() == TEXT("brannoch") && OnBreak == TEXT("vent"))
	{
		++VentsBroken;
		if (VentsBroken >= 2)
		{
			Stagger(BrainTune(Id(), TEXT("ventStagger"), 6.f));
			WRFx::Broadcast(GetOwner(), TEXT("stance.break"), GetOwner()->GetActorLocation() + FVector(0, 0, 200.f), FRotator::ZeroRotator, 2.f, TEXT("portal.bellows"));
		}
	}
	(void)PartId;
}

void UWRPortalBossBrain::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWRBoss* B = Boss();
	if (!B || !B->HasAuthority() || !B->IsAlive()) return;
	const FName Me = Id();
	const double T = Now();
	Adds.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
	if (bEscaped)
	{
		// It never rests: when nothing holds it, it walks the ground within 400 m of where its portal broke.
		if (!B->bEngaged && !B->IsReturning() && (T > ShellAt || FVector::Dist2D(B->GetActorLocation(), B->GetHome()) < 300.f))
		{
			const float R = WRPortals::Num(TEXT("escalation.escapedRoam"), 40000.f);
			const float A = FMath::FRand() * 2.f * PI, D = FMath::Sqrt(FMath::FRand()) * R;
			B->SetHome(Home + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f));
			ShellAt = T + FMath::FRandRange(25.0, 45.0);
		}
		return;
	}
	AWRFold* F = Fold.Get();
	if (Me == TEXT("cinderfang_alpha") && !bManeLit && T >= ManeOutUntil && F)
	{
		// It relights its mane at the lava runnels.
		const FVector Feet = B->GetActorLocation() - FVector(0, 0, B->GetSimpleCollisionHalfHeight());
		for (int32 i = 0; i < F->LavaA.Num(); ++i)
		{
			if (FVector::Dist2D(FMath::ClosestPointOnSegment(Feet, F->LavaA[i], F->LavaB[i]), Feet) < BrainTune(Me, TEXT("maneRelight"), 300.f))
			{
				bManeLit = true;
				B->PortalMark.A = 255;
				WRFx::Broadcast(B, TEXT("fire.impact"), B->GetActorLocation() + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.5f, TEXT("world.kindle"));
				break;
			}
		}
	}
	else if (Me == TEXT("kilnmite_broodqueen") && T >= NextLay)
	{
		// While a clutch stands she lays: four mites from a living clutch.
		TArray<AWRFoldThing*> Living;
		for (const TWeakObjectPtr<AWRFoldThing>& W : Props) if (W.IsValid() && W->Kind == TEXT("clutch") && !W->bDone) Living.Add(W.Get());
		if (Living.Num() > 0 && B->bEngaged)
		{
			AWRFoldThing* C = Living[FMath::RandHelper(Living.Num())];
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(TEXT("anvil_mite"));
			float H = 1.f, D = 1.f, S = 1.f;
			if (Def && F) WRPortalsSys::PsScaleFor(F->Rank, Def->Tier, H, D, S);
			for (int32 i = 0; i < FMath::RoundToInt(BrainTune(Me, TEXT("layCount"), 4.f)); ++i)
			{
				if (AWRCreature* M = WRPortalsSys::PsSpawnScaled(GetWorld(), TEXT("anvil_mite"), C->GetActorLocation() + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), 40.f), 0.f, H, D, S, FColor(0, 0, 0, 0)))
				{
					M->SetHome(B->GetHome());
					if (AActor* Tg = B->GetAggroTarget()) M->SetAggroTarget(Tg);
					Adds.Add(M);
				}
			}
			WRFx::Broadcast(C, TEXT("hit.blood"), C->GetActorLocation() + FVector(0, 0, 70.f), FRotator::ZeroRotator, 1.5f, TEXT("portal.hatch"));
		}
		NextLay = T + BrainTune(Me, TEXT("layEvery"), 20.f);
	}
	else if (Me == TEXT("frostbound_thurs"))
	{
		const bool bHot = B->HasTag(WRTags::Status_Burn) || B->HasTag(WRTags::Status_Bleeding);
		if (bShell && bHot)
		{
			bShell = false;
			ShellAt = T + BrainTune(Me, TEXT("shellEvery"), 25.f);
			WRFx::Broadcast(B, TEXT("hit.ice"), B->GetActorLocation() + FVector(0, 0, 150.f), FRotator::ZeroRotator, 3.f, TEXT("portal.ice_break"));
		}
		else if (!bShell && !bHot && T >= ShellAt)
		{
			bShell = true;
			WRFx::Broadcast(B, TEXT("status.frozen"), B->GetActorLocation() + FVector(0, 0, 150.f), FRotator::ZeroRotator, 2.f, TEXT("icelance"));
		}
	}
	else if (Me == TEXT("rime_witch") && T >= NextSnuff && B->bEngaged)
	{
		// She heaps snow on a lit brazier.
		TArray<AWRFoldThing*> Lit;
		for (const TWeakObjectPtr<AWRFoldThing>& W : Props) if (W.IsValid() && W->Kind == TEXT("arenabrazier") && W->State == 1) Lit.Add(W.Get());
		if (Lit.Num() > 0)
		{
			AWRFoldThing* Br = Lit[FMath::RandHelper(Lit.Num())];
			Br->State = 0;
			WRFx::Broadcast(Br, TEXT("snow"), Br->GetActorLocation() + FVector(0, 0, 200.f), FRotator::ZeroRotator, 1.5f, TEXT("portal.douse"));
		}
		NextSnuff = T + BrainTune(Me, TEXT("snuffEvery"), 25.f);
	}
	else if (Me == TEXT("hrimgerdr") && F && Room != INDEX_NONE)
	{
		// The dawn through the broken bolts: in a shaft she slows and turns toward stone.
		int32 Bits = 0;
		AWRFoldThing* Window = nullptr;
		for (const TWeakObjectPtr<AWRFoldThing>& W : Props)
		{
			if (!W.IsValid()) continue;
			if (W->Kind == TEXT("bolt") && W->bDone) Bits |= 1 << W->Sub;
			if (W->Kind == TEXT("window")) Window = W.Get();
		}
		if (Window && Window->State != Bits) { Window->State = Bits; }
		const FVector L = F->RoomFrame(Room).InverseTransformPosition(B->GetActorLocation());
		const float Wd = F->Rooms[Room].W;
		bool bInDawn = false;
		for (const TWeakObjectPtr<AWRFoldThing>& W : Props)
		{
			if (!W.IsValid() || W->Kind != TEXT("bolt") || !W->bDone) continue;
			const FVector BL = F->RoomFrame(Room).InverseTransformPosition(W->GetActorLocation());
			if (FMath::Abs(L.Y - BL.Y) < BrainTune(Me, TEXT("shaftWidth"), 450.f) * 0.5f && L.X > Wd - 2600.f) bInDawn = true;
		}
		if (bInDawn)
		{
			StunnedUntil = T + 0.5;
			B->Status->ApplyStatus(WRTags::Status_Slowed, 0.6f);
		}
	}
}
