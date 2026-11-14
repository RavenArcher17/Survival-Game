// The things in a fold (cooks/build-portals-artifacts.md 2.4, 2.5, 2.9): doors that shut behind a fight and slide up
// into their lintel when the room is won, the boss door that drops behind the first one through, the entry stone (the
// way out), personal chests, the four traps (pulse grates and the anvil blocks that plug them, slag chutes and their
// ropes, icicles, thin ice), the four puzzles (quench the blade, the bellows rhythm, the sun's braziers, the aurora
// lock), the rest spring and the boss arenas' props. Every trap gives at least 0.9 s of sound and sight before it
// bites, can kill, hurts creatures too and can be answered. Trap timings ride on the server's clock, replicated once,
// so every machine plays the tell at the same moment without a message each cycle.

#include "WRPortals.h"
#include "WRPortalsInternal.h"
#include "WRArtifacts.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

using namespace WRPortalsImpl;
using WRLookKit::FSurf;
using WRLookKit::FKit;

double WRFoldHazards::Now(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : World ? World->GetTimeSeconds() : 0.0;
}

namespace WRFoldThingImpl
{
	/** A blow from the fold itself (no one behind it): through ApplyHit like everything else. */
	void FoldHurt(AWRCharacterBase* Who, float Damage, FGameplayTag Element, float Burn, float Frost, bool bDot, const FVector& From)
	{
		if (!Who || !Who->IsAlive()) return;
		FWRHit Hit;
		Hit.Instigator = nullptr;
		Hit.Damage = Damage;
		Hit.PoiseDamage = bDot ? 0.f : Damage;
		Hit.Element = Element;
		Hit.BurnBuildup = Burn;
		Hit.FrostBuildup = Frost;
		Hit.bDamageOverTime = bDot;
		Hit.bUnblockable = true;
		Hit.Origin = From;
		Hit.Impact = bDot ? 1 : 3;
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}

	template <typename F> void FoldForBodies(UWorld* World, const FVector& Centre, float Radius, F&& Fn)
	{
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (It->IsAlive() && FVector::DistSquared(It->GetActorLocation(), Centre) < FMath::Square(Radius + 400.f)) Fn(*It);
		}
	}

	/** FoldFeet of a character (the bottom of its capsule). */
	FVector FoldFeet(const AWRCharacterBase* C) { return C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight()); }

	bool FoldAnyPlayerNear(UWorld* World, const FVector& At, float R)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && FVector::Dist(P->GetActorLocation(), At) < R) return true;
		}
		return false;
	}

	FString PcName(const AWRPlayerController* PC) { return PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(); }

	const TCHAR* FoldTroughName[3] = { TEXT("water"), TEXT("oil"), TEXT("brine") };
	/** The aurora's bands (never gold or crimson). */
	const FColor FoldAurora[3] = { FColor(80, 255, 150), FColor(170, 100, 255), FColor(255, 110, 170) };
}
using namespace WRFoldThingImpl;

// ================================================================================================ hazards

void WRFoldHazards::Step(AWRFold* F, float Dt)
{
	if (!F || !F->HasAuthority()) return;
	F->HazardClock += Dt;
	if (F->HazardClock < 0.5f) return;
	const float Step = F->HazardClock;
	F->HazardClock = 0.f;
	UWorld* World = F->GetWorld();
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || !F->Bounds.IsInsideOrOn(C->GetActorLocation())) continue;
		const FVector Ft = FoldFeet(C);
		// Lava runnels: 20 / s and burn to anything that stands in one (creatures too; fire-born ones shrug the burn).
		for (int32 i = 0; i < F->LavaA.Num(); ++i)
		{
			const FVector P = FMath::ClosestPointOnSegment(Ft, F->LavaA[i], F->LavaB[i]);
			if (FVector::Dist2D(P, Ft) < 45.f && FMath::Abs(Ft.Z - P.Z) < 60.f)
			{
				FoldHurt(C, 20.f * Step, WRTags::Element_Fire, 20.f * Step, 0.f, true, P);
				break;
			}
		}
		// Black water: frostbite and the cold 15 / s until you climb out.
		for (const FBox& Pit : F->Pits)
		{
			if (Pit.IsInsideOrOn(Ft + FVector(0, 0, 10.f)))
			{
				FoldHurt(C, 15.f * Step, WRTags::Element_Frost, 0.f, 25.f * Step, true, Ft);
				C->Status->ApplyStatus(WRTags::Status_Slowed, 1.f);
				break;
			}
		}
	}
}

// ================================================================================================ AWRFoldThing

AWRFoldThing::AWRFoldThing()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(12000.f));
}

void AWRFoldThing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRFoldThing, Kind);
	DOREPLIFETIME(AWRFoldThing, Biome);
	DOREPLIFETIME(AWRFoldThing, Fold);
	DOREPLIFETIME(AWRFoldThing, Room);
	DOREPLIFETIME(AWRFoldThing, Sub);
	DOREPLIFETIME(AWRFoldThing, State);
	DOREPLIFETIME(AWRFoldThing, TimeA);
	DOREPLIFETIME(AWRFoldThing, TimeB);
	DOREPLIFETIME(AWRFoldThing, bDone);
	DOREPLIFETIME(AWRFoldThing, Names);
	DOREPLIFETIME(AWRFoldThing, Tint);
	DOREPLIFETIME(AWRFoldThing, Size);
	DOREPLIFETIME(AWRFoldThing, Seq);
	DOREPLIFETIME(AWRFoldThing, Link);
}

void AWRFoldThing::BeginPlay()
{
	Super::BeginPlay();
	if (!Kind.IsNone()) BuildLook();
}

void AWRFoldThing::OnRep_Look() { BuildLook(); }
void AWRFoldThing::OnRep_State() { ShowState(); }

FString AWRFoldThing::Title() const
{
	if (Kind == TEXT("stone")) return TEXT("The entry stone");
	if (Kind == TEXT("chest")) return TEXT("A chest");
	if (Kind == TEXT("vault")) return TEXT("The vault coffer");
	if (Kind == TEXT("block")) return TEXT("A loose anvil block");
	if (Kind == TEXT("quench")) return TEXT("The blade blank");
	if (Kind == TEXT("trough")) return FString::Printf(TEXT("A trough of %s"), FoldTroughName[FMath::Min<int32>(Sub, 2)]);
	if (Kind == TEXT("bellows")) return FString::Printf(TEXT("The %s bellows"), Sub == 0 ? TEXT("left") : Sub == 1 ? TEXT("middle") : TEXT("right"));
	if (Kind == TEXT("brazier")) return TEXT("A frozen brazier");
	if (Kind == TEXT("flame")) return TEXT("The seal-oil lamp");
	if (Kind == TEXT("prism")) return TEXT("An ice prism");
	if (Kind == TEXT("spring")) return TEXT("The spring");
	if (Kind == TEXT("crucible")) return TEXT("The crucible's chain");
	if (Kind == TEXT("arenabrazier")) return TEXT("A brazier");
	return FString();
}

bool AWRFoldThing::CanInteract(const AWRPlayerController* PC) const
{
	static const TSet<FName> Kinds = { TEXT("stone"), TEXT("chest"), TEXT("vault"), TEXT("block"), TEXT("quench"), TEXT("trough"), TEXT("bellows"), TEXT("brazier"), TEXT("flame"), TEXT("prism"), TEXT("spring"), TEXT("crucible"), TEXT("arenabrazier") };
	return Kinds.Contains(Kind);
}

FString AWRFoldThing::Verb(const AWRPlayerController* PC) const
{
	const FString Me = PcName(PC);
	const AWRFoldThing* Ctl = Link ? Link.Get() : this;
	if (Kind == TEXT("stone")) return TEXT("Leave the fold");
	if (Kind == TEXT("chest") || Kind == TEXT("vault")) return Names.Contains(Me) ? FString() : TEXT("Open");
	if (Kind == TEXT("block")) return State == 0 ? TEXT("Slide it onto a grate") : FString();
	if (Kind == TEXT("quench")) return bDone ? FString() : State == 2 ? TEXT("Heating again...") : Names.Num() > 0 ? FString() : TEXT("Take the blank (hot)");
	if (Kind == TEXT("trough")) return Ctl && !Ctl->bDone && Ctl->Names.Contains(Me) ? TEXT("Quench the blade") : FString();
	if (Kind == TEXT("bellows")) return Ctl && !Ctl->bDone ? TEXT("Pump") : FString();
	if (Kind == TEXT("brazier")) return State == 0 && Ctl && !Ctl->bDone ? TEXT("Light it") : FString();
	if (Kind == TEXT("flame")) return TEXT("Take a flame");
	if (Kind == TEXT("prism")) return Ctl && !Ctl->bDone ? TEXT("Turn it") : FString();
	if (Kind == TEXT("spring")) return TEXT("Drink");
	if (Kind == TEXT("crucible")) return State == 0 ? (TimeB > 0.f ? TEXT("Hauling...") : TEXT("Haul the chain")) : FString();
	if (Kind == TEXT("arenabrazier")) return State == 0 ? (TimeB > 0.f ? TEXT("Lighting...") : TEXT("Relight it")) : FString();
	return FString();
}

void AWRFoldThing::SetOpen(bool bOpen)
{
	if (!HasAuthority() || (State == 1) == bOpen) return;
	State = bOpen ? 1 : 0;
	TimeA = float(WRFoldHazards::Now(this));
	if (Block) Block->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	WRSound::PlayAt(this, bOpen ? TEXT("portal.door_open") : TEXT("portal.door_shut"), GetActorLocation());
	ShowState();
}

// ------------------------------------------------------------------------------------------------ interact (server)

FString AWRFoldThing::Interact(AWRPlayerController* PC)
{
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!Me || !PS || !Me->IsAlive()) return TEXT("Not now");
	const FString Name = PS->GetPlayerName();
	const float Now = float(WRFoldHazards::Now(this));
	UWRPortalSystem* Sys = UWRPortalSystem::Get(this);
	AWRFoldThing* Ctl = Link ? Link.Get() : this;
	if (Kind == TEXT("stone")) return Sys ? Sys->Leave(PC) : FString();
	if (Kind == TEXT("spring"))
	{
		if (UAbilitySystemComponent* ASC = Me->GetAbilitySystemComponent()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), 100.f);
		WRSound::PlayOn(Me, TEXT("drink"));
		return TEXT("Cold spring water");
	}
	if (Kind == TEXT("chest") || Kind == TEXT("vault"))
	{
		if (Names.Contains(Name)) return TEXT("Already yours");
		AWRFold* F = Fold.Get();
		if (!F) return FString();
		// Each player opens their own copy: no racing for a group's chest.
		Names.Add(Name);
		const TArray<TSharedPtr<FJsonValue>>* Rewards = PArr(WRPortals::Data(), TEXT("rewards"));
		const TSharedPtr<FJsonObject> Row = Rewards && Rewards->IsValidIndex(F->Rank - 1) ? (*Rewards)[F->Rank - 1]->AsObject() : nullptr;
		const float PerMod = WRPortals::Num(TEXT("lootPerModifier"), 0.25f);
		const float Q = (1.f + PerMod * F->Modifiers.Num()) * WRArtifacts::LootScale(PS);
		TMap<FName, int32> Items;
		TArray<FName> Special;
		const bool bTreasure = Sub == 1;
		WRPortalsSys::PsResolve(PChild(Row, bTreasure ? TEXT("treasure") : TEXT("chest")), F->Biome, Q, Items);
		if (bTreasure) WRPortalsSys::PsChances(PChild(Row, TEXT("treasureChance")), F->Biome, Items, Special);
		if (Kind == TEXT("vault"))
		{
			WRPortalsSys::PsResolve(PChild(Row, TEXT("vault")), F->Biome, Q, Items);
			WRPortalsSys::PsChances(PChild(Row, TEXT("vaultChance")), F->Biome, Items, Special);
		}
		TArray<FWRItemStack> Gear;
		for (const FName& S : Special)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pool = PArr(WRPortals::Obj(TEXT("fineGear")), S == TEXT("@masterwork") ? TEXT("5") : TEXT("2"));
			const EWRQuality Grade = S == TEXT("@masterwork") || WRArtifacts::RollQualityStep(PS) ? EWRQuality::Masterwork : EWRQuality::Fine;   // artifacts: a step up now and then
			if (Pool && Pool->Num()) Gear.Add(WRPortalsSys::PsGearAt(FName((*Pool)[FMath::RandHelper(Pool->Num())]->AsString()), Grade));
		}
		WRSound::PlayAt(this, TEXT("portal.chest"), GetActorLocation());
		const FString Got = WRPortalsSys::PsGive(PS, Items, Gear);
		return Got.IsEmpty() ? TEXT("Empty") : FString::Printf(TEXT("You take %s"), *Got);
	}
	if (Kind == TEXT("block"))
	{
		if (State != 0) return FString();
		// It scrapes along the floor onto the nearest open grate within 6 m.
		AWRFoldThing* Best = nullptr;
		float BestD = 600.f;
		for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It)
		{
			if (It->Fold != Fold || It->Kind != TEXT("grate") || It->bDone) continue;
			const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		if (!Best) return TEXT("No grate near enough to push it onto");
		State = 1;
		Link = Best;
		Goal = Best->GetActorLocation() + FVector(0, 0, 2.f);
		Period = FVector::Dist(GetActorLocation(), Goal);
		TimeA = Now;
		TimeB = Now + 1.5f;
		WRSound::PlayAt(this, TEXT("portal.block_scrape"), GetActorLocation());
		return TEXT("You heave the block across");
	}
	if (Kind == TEXT("quench"))
	{
		if (bDone || State == 2 || Names.Num() > 0) return FString();
		Names = { Name };
		TimeB = Now + 30.f;
		WRSound::PlayAt(this, TEXT("portal.tongs"), GetActorLocation());
		return FString::Printf(TEXT("The blank glows %s in your tongs: quench it before it cools"), *FString(TEXT("bright")));
	}
	if (Kind == TEXT("trough"))
	{
		if (!Ctl || Ctl->bDone || !Ctl->Names.Contains(Name)) return FString();
		Ctl->Names.Reset();
		WRFx::Broadcast(this, TEXT("reaction.steam"), GetActorLocation() + FVector(0, 0, 90.f), FRotator::ZeroRotator, 1.2f, TEXT("portal.quench"));
		if (Ctl->Seq.IsValidIndex(0) && Ctl->Seq[0] == Sub)
		{
			Ctl->bDone = true;
			Ctl->ShowState();
			if (Sys) Sys->RoomCleared(Fold.Get(), Room);
			return TEXT("The blade takes its temper: the bar lifts");
		}
		Ctl->State = 2;
		Ctl->TimeA = Now + 20.f;
		WRSound::PlayAt(this, TEXT("portal.crack"), GetActorLocation());
		return TEXT("The blade cracks: a new blank heats on the anvil");
	}
	if (Kind == TEXT("bellows"))
	{
		if (!Ctl || Ctl->bDone) return FString();
		State = State + 1;   // the pump's own counter (its animation)
		TimeA = Now;
		WRSound::PlayAt(this, TEXT("portal.bellows"), GetActorLocation());
		// The forge listens: the right bellows in the hammer's order.
		if (Ctl->Seq.IsValidIndex(Ctl->State) && Ctl->Seq[Ctl->State] == Sub)
		{
			Ctl->State++;
			if (Ctl->State >= Ctl->Seq.Num())
			{
				Ctl->bDone = true;
				Ctl->ShowState();
				WRFx::Broadcast(Ctl, TEXT("breath.fire"), Ctl->GetActorLocation() + FVector(0, 0, 160.f), FRotator(90.f, 0.f, 0.f), 1.2f, TEXT("portal.forge_roar"));
				if (Sys) Sys->RoomCleared(Fold.Get(), Room);
				return TEXT("The forge roars: the bar lifts");
			}
			return FString();
		}
		Ctl->State = 0;
		Ctl->Period += 1.f;   // misses
		WRFx::Broadcast(Ctl, TEXT("dust.cloud"), Ctl->GetActorLocation() + FVector(0, 0, 200.f), FRotator::ZeroRotator, 1.f, TEXT("portal.forge_cough"));
		if (Ctl->Period >= 3.f)
		{
			// Three misses: the automaton strikes a new rhythm.
			Ctl->Period = 0.f;
			const int32 N = FMath::RandRange(3, 5);
			Ctl->Seq.Reset();
			for (int32 i = 0; i < N; ++i) Ctl->Seq.Add(uint8(FMath::RandRange(0, 2)));
			Ctl->Names = { FString::JoinBy(Ctl->Seq, TEXT(""), [](uint8 B) { return FString::FromInt(B); }) };
		}
		Ctl->TimeA = Now + 1.5f;   // the hammer plays it again
		return TEXT("The forge coughs smoke: the hammer strikes the rhythm again");
	}
	if (Kind == TEXT("flame"))
	{
		Names = { Name };
		TimeA = Now + 20.f;
		WRSound::PlayOn(Me, TEXT("world.kindle"));
		return TEXT("You cup a flame from the lamp: 20 s to carry it");
	}
	if (Kind == TEXT("brazier"))
	{
		if (State != 0 || !Ctl || Ctl->bDone) return FString();
		// Fire from your torch, a fire path, or a flame carried from the seal-oil lamp.
		bool bFire = false;
		if (const UWRSurvivalComponent* S = UWRSurvivalComponent::For(Me)) bFire |= S->IsLightOn();
		if (const UWRPathComponent* Paths = PS->GetPaths()) bFire |= Paths->IsActivePath(TEXT("fire"));
		for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It)
		{
			if (It->Fold == Fold && It->Room == Room && It->Kind == TEXT("flame") && It->Names.Contains(Name) && Now < It->TimeA) { bFire = true; It->Names.Reset(); }
		}
		if (!bFire) return TEXT("You need fire: a lit torch, or a flame from the lamp");
		// The sun's order: dawn first (east), then on west. Out of order, the snow on the vent puts them all out.
		int32 Lit = 0;
		TArray<AWRFoldThing*> All;
		for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It) if (It->Fold == Fold && It->Room == Room && It->Kind == TEXT("brazier")) { All.Add(*It); Lit += It->State; }
		if (Sub != Lit)
		{
			for (AWRFoldThing* B : All) { B->State = 0; B->ShowState(); }
			WRFx::Broadcast(this, TEXT("snow"), GetActorLocation() + FVector(0, 0, 300.f), FRotator::ZeroRotator, 1.f, TEXT("portal.douse"));
			return TEXT("Snow pours from the vent: every brazier hisses out");
		}
		State = 1;
		ShowState();
		WRSound::PlayAt(this, TEXT("world.kindle"), GetActorLocation());
		if (Lit + 1 >= All.Num())
		{
			Ctl->bDone = true;
			if (Sys) Sys->RoomCleared(Fold.Get(), Room);
			return TEXT("The sun's road is lit: the ice door melts");
		}
		return FString();
	}
	if (Kind == TEXT("prism"))
	{
		if (!Ctl || Ctl->bDone) return FString();
		State = (State + 1) % 6;
		TimeA = Now;
		WRSound::PlayAt(this, TEXT("portal.prism"), GetActorLocation());
		// Solved when every prism's colour meets the band over its socket.
		bool bAll = true;
		for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It)
		{
			if (It->Fold != Fold || It->Room != Room || It->Kind != TEXT("prism")) continue;
			const int32 Want = Ctl->Seq.IsValidIndex(It->Sub) ? Ctl->Seq[It->Sub] : 0;
			bAll &= (It->State % 3) == Want;
		}
		if (bAll)
		{
			Ctl->bDone = true;
			Ctl->ShowState();
			if (Sys) Sys->RoomCleared(Fold.Get(), Room);
			return TEXT("The sockets blaze with the aurora: the ice door melts");
		}
		return FString();
	}
	if (Kind == TEXT("crucible") || Kind == TEXT("arenabrazier"))
	{
		if (State != 0 || TimeB > 0.f) return FString();
		// A held job: stand by it (a blow or a step away stops it).
		Names = { Name };
		TimeB = Now + (Kind == TEXT("crucible") ? WRTune(TEXT("bosses.mould_born.chainSeconds"), 1.5f) : WRTune(TEXT("bosses.rime_witch.relightSeconds"), 2.f));
		Goal = Me->GetActorLocation();
		WRSound::PlayAt(this, Kind == TEXT("crucible") ? TEXT("portal.chain") : TEXT("world.kindle"), GetActorLocation());
		return FString();
	}
	return FString();
}

void AWRFoldThing::TakeHit(AWRCharacterBase* By, float Damage, bool bFire)
{
	if (!HasAuthority() || bDone) return;
	const float Now = float(WRFoldHazards::Now(this));
	if (Kind == TEXT("icicle"))
	{
		// Anything drops it early: a thrown stone, an arrow, a spell.
		if (TimeA <= 0.f) TimeA = Now + 0.35f;
		return;
	}
	if (Kind == TEXT("chute"))
	{
		// Its rope (100): cut it and the slag pours now (onto whatever's under it).
		Health -= Damage;
		if (Health <= 0.f && TimeA - Now > 1.2f)
		{
			TimeA = Now + 1.0f;
			Health = 100.f;
		}
		return;
	}
	if (Kind == TEXT("clutch") || Kind == TEXT("bolt"))
	{
		Health -= Damage;
		WRFx::SpawnAt(this, Kind == TEXT("bolt") ? TEXT("hit.ice") : TEXT("hit.blood"), GetActorLocation() + FVector(0, 0, 60.f));
		if (Health <= 0.f)
		{
			bDone = true;
			if (Block) Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WRFx::Broadcast(this, Kind == TEXT("bolt") ? TEXT("hit.ice") : TEXT("death.poof"), GetActorLocation() + FVector(0, 0, 60.f), FRotator::ZeroRotator, 2.f, Kind == TEXT("bolt") ? TEXT("portal.ice_break") : TEXT("portal.clutch_break"));
			ShowState();
		}
	}
	(void)By; (void)bFire;
}

// ------------------------------------------------------------------------------------------------ server tick

void AWRFoldThing::StepServer(float Dt)
{
	UWorld* World = GetWorld();
	const float Now = float(WRFoldHazards::Now(this));
	const float Wake = WRPortals::Num(TEXT("entry.trapWake"), 4000.f);
	if (Kind == TEXT("grate"))
	{
		if (bDone || !FoldAnyPlayerNear(World, GetActorLocation(), Wake)) return;
		// tell 1.2 s (glow and hiss), 2 s of lava, then quiet: on the shared clock (TimeA = this grate's offset).
		const float Phase = FMath::Fmod(Now - TimeA + 1000.f * Period, Period);
		if (Phase >= 1.2f && Phase < 3.2f)
		{
			Clock += Dt;
			if (Clock >= 0.25f)
			{
				const float Step = Clock;
				Clock = 0.f;
				FoldForBodies(World, GetActorLocation(), 120.f, [&](AWRCharacterBase* C)
				{
					const FVector L = GetActorTransform().InverseTransformPosition(FoldFeet(C));
					if (FMath::Abs(L.X) < 85.f && FMath::Abs(L.Y) < 85.f && L.Z > -30.f && L.Z < 120.f) FoldHurt(C, 30.f * Step, WRTags::Element_Fire, 20.f * Step, 0.f, true, GetActorLocation());
				});
			}
		}
		return;
	}
	if (Kind == TEXT("block") && State == 1 && TimeB > 0.f)
	{
		const float T = FMath::Clamp((Now - TimeA) / FMath::Max(0.1f, TimeB - TimeA), 0.f, 1.f);
		// Scraped across the floor at an even pace (a heavy thing, never a jump).
		const FVector Dir = (Goal - GetActorLocation()).GetSafeNormal2D();
		const float Left = FVector::Dist2D(Goal, GetActorLocation());
		SetActorLocation(GetActorLocation() + Dir * FMath::Min(Left, Period / 1.5f * Dt));
		if (T >= 1.f)
		{
			SetActorLocation(Goal);
			TimeB = 0.f;
			State = 2;
			if (AWRFoldThing* G = Link.Get()) { G->bDone = true; G->ShowState(); }
			WRSound::PlayAt(this, TEXT("portal.block_set"), GetActorLocation());
		}
		return;
	}
	if (Kind == TEXT("chute"))
	{
		if (!FoldAnyPlayerNear(World, GetActorLocation(), Wake)) { if (TimeA < Now) TimeA = Now + Period; return; }
		if (TimeA <= 0.f) TimeA = Now + Period;
		if (!bFired && Now >= TimeA)
		{
			bFired = true;
			// The pour: a 3 x 6 m strip under the chute, 80 and burn 40, then 3 s of glowing slag.
			FoldForBodies(World, GetActorLocation(), 400.f, [&](AWRCharacterBase* C)
			{
				const FVector L = GetActorTransform().InverseTransformPosition(FoldFeet(C));
				if (FMath::Abs(L.X) < 150.f && FMath::Abs(L.Y) < 300.f && L.Z > -600.f && L.Z < 60.f) FoldHurt(C, 80.f, WRTags::Element_Fire, 40.f, 0.f, false, GetActorLocation() + FVector(0, 0, 500.f));
			});
			WRFx::Broadcast(this, TEXT("lavapool"), GetActorLocation(), FRotator::ZeroRotator, 1.3f, TEXT("portal.pour"));
			TimeB = Now + 3.f;
		}
		if (bFired && Now < TimeB)
		{
			Clock += Dt;
			if (Clock >= 0.25f)
			{
				const float Step = Clock;
				Clock = 0.f;
				FoldForBodies(World, GetActorLocation(), 400.f, [&](AWRCharacterBase* C)
				{
					const FVector L = GetActorTransform().InverseTransformPosition(FoldFeet(C));
					if (FMath::Abs(L.X) < 150.f && FMath::Abs(L.Y) < 300.f && L.Z > -30.f && L.Z < 60.f) FoldHurt(C, 20.f * Step, WRTags::Element_Fire, 10.f * Step, 0.f, true, GetActorLocation());
				});
			}
		}
		if (bFired && Now >= TimeB)
		{
			bFired = false;
			TimeA = Now + Period;   // it re-arms (the crucible above refills)
		}
		return;
	}
	if (Kind == TEXT("icicle"))
	{
		if (bDone) return;
		if (TimeA <= 0.f)
		{
			// Someone walks under: it cracks from its root.
			FoldForBodies(World, GetActorLocation(), 300.f, [&](AWRCharacterBase* C)
			{
				if (TimeA <= 0.f && C->GetPlayerState() && FVector::Dist2D(C->GetActorLocation(), GetActorLocation()) < 260.f) TimeA = Now + 1.0f;
			});
			return;
		}
		if (Now >= TimeA)
		{
			bDone = true;
			const FVector Floor = GetActorLocation() - FVector(0, 0, Size.Z);
			FoldForBodies(World, Floor, 150.f, [&](AWRCharacterBase* C)
			{
				if (FVector::Dist2D(C->GetActorLocation(), Floor) < 150.f && FMath::Abs(FoldFeet(C).Z - Floor.Z) < 200.f) FoldHurt(C, 60.f, WRTags::Element_Frost, 0.f, 30.f, false, GetActorLocation());
			});
			ShowState();
		}
		return;
	}
	if (Kind == TEXT("pane"))
	{
		if (State == 0)
		{
			bool bOn = false;
			FoldForBodies(World, GetActorLocation(), FMath::Max(Size.X, Size.Y), [&](AWRCharacterBase* C)
			{
				const FVector L = GetActorTransform().InverseTransformPosition(FoldFeet(C));
				bOn |= FMath::Abs(L.X) < Size.X * 0.5f && FMath::Abs(L.Y) < Size.Y * 0.5f && FMath::Abs(L.Z) < 60.f;
			});
			if (bOn)
			{
				State = 1;
				TimeA = Now + 1.0f;
				ShowState();
			}
		}
		else if (State == 1 && Now >= TimeA)
		{
			State = 2;
			TimeB = Now + 20.f;
			if (Block) Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WRSound::PlayAt(this, TEXT("portal.ice_break"), GetActorLocation());
			ShowState();
		}
		else if (State == 2 && Now >= TimeB)
		{
			// It refreezes, unless someone's still in the water under it.
			bool bUnder = false;
			FoldForBodies(World, GetActorLocation(), FMath::Max(Size.X, Size.Y), [&](AWRCharacterBase* C)
			{
				const FVector L = GetActorTransform().InverseTransformPosition(C->GetActorLocation());
				bUnder |= FMath::Abs(L.X) < Size.X * 0.5f + 50.f && FMath::Abs(L.Y) < Size.Y * 0.5f + 50.f && L.Z < 150.f;
			});
			if (!bUnder)
			{
				State = 0;
				if (Block) Block->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				ShowState();
			}
		}
		return;
	}
	if (Kind == TEXT("quench"))
	{
		if (State == 2 && Now >= TimeA) { State = 0; ShowState(); }
		// A blank carried too long cools: back on the anvil to heat again.
		if (Names.Num() > 0 && Now >= TimeB) { Names.Reset(); State = 2; TimeA = Now + 5.f; }
		// The carrier walks slower (the tongs and the heat).
		for (const FString& N : Names)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AWRCharacterBase* C = It->Get() ? Cast<AWRCharacterBase>(It->Get()->GetPawn()) : nullptr;
				if (C && It->Get()->PlayerState && It->Get()->PlayerState->GetPlayerName() == N)
				{
					C->Status->ApplyStatus(WRTags::Status_Slowed, 0.6f);
					if (!C->IsAlive()) Names.Reset();
				}
			}
		}
		return;
	}
	if (Kind == TEXT("forge"))
	{
		// The hammer plays its rhythm again whenever no one has pumped for a while.
		if (!bDone && State == 0 && Now > TimeA + Seq.Num() * 0.8f + 4.f) TimeA = Now + 0.5f;
		return;
	}
	if (Kind == TEXT("crucible") || Kind == TEXT("arenabrazier"))
	{
		if (TimeB > 0.f)
		{
			// The held job: the one hauling must stay by it and on their feet.
			AWRCharacterBase* Who = nullptr;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				if (It->Get() && It->Get()->PlayerState && Names.Contains(It->Get()->PlayerState->GetPlayerName())) Who = Cast<AWRCharacterBase>(It->Get()->GetPawn());
			if (!Who || !Who->IsAlive() || FVector::Dist2D(Who->GetActorLocation(), Goal) > 150.f || Who->HasTag(WRTags::Status_Staggered))
			{
				TimeB = 0.f;
				Names.Reset();
			}
			else if (Now >= TimeB)
			{
				TimeB = 0.f;
				State = 1;
				TimeA = Now;
				ShowState();
				if (Kind == TEXT("arenabrazier")) WRSound::PlayAt(this, TEXT("world.kindle"), GetActorLocation());
				if (Kind == TEXT("crucible"))
				{
					// It tips: molten iron in a 4 m circle under it. The Mould-Born under it takes 600 and staggers.
					const FVector Under = GetActorLocation();
					WRFx::Broadcast(this, TEXT("lavapool"), Under, FRotator::ZeroRotator, 1.6f, TEXT("portal.pour"));
					FoldForBodies(World, Under, 400.f, [&](AWRCharacterBase* C)
					{
						if (FVector::Dist2D(C->GetActorLocation(), Under) > WRTune(TEXT("bosses.mould_born.crucibleRadius"), 350.f)) return;
						if (AWRBoss* B = Cast<AWRBoss>(C))
						{
							FoldHurt(B, WRTune(TEXT("bosses.mould_born.crucibleDamage"), 600.f), WRTags::Element_Fire, 0.f, 0.f, false, Under + FVector(0, 0, 300.f));
							B->Status->Stagger(WRTune(TEXT("bosses.mould_born.crucibleStagger"), 4.f));
						}
						else FoldHurt(C, 60.f, WRTags::Element_Fire, 40.f, 0.f, false, Under + FVector(0, 0, 300.f));
					});
				}
			}
		}
		return;
	}
	if (Kind == TEXT("tank") && State == 1 && Now >= TimeA)
	{
		State = 0;   // refilled from the cistern pipe
		ShowState();
		WRSound::PlayAt(this, TEXT("portal.refill"), GetActorLocation());
	}
}

// ------------------------------------------------------------------------------------------------ look

void AWRFoldThing::BuildLook()
{
	if (bLookBuilt || Kind.IsNone()) return;
	bLookBuilt = true;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const bool bFrost = Biome == TEXT("tundra");
	const FLinearColor RankC = (Kind == TEXT("stone") || Kind == TEXT("bossdoor") || Kind == TEXT("vault")) ? FLinearColor(Tint) : Fold ? FLinearColor(WRPortals::RankColour(Fold->Rank)) : FLinearColor(0.9f, 0.88f, 0.8f);
	const FLinearColor Stone = WRFoldLook::KitColour(Biome, TEXT("stone"), FColor(24, 21, 20));
	const FLinearColor StoneAlt = WRFoldLook::KitColour(Biome, TEXT("stoneAlt"), FColor(44, 38, 34));
	const FLinearColor TrimC = WRFoldLook::KitColour(Biome, TEXT("trim"), FColor(140, 92, 46));
	const FSurf Iron = FSurf::Steel(FLinearColor(0.03f, 0.028f, 0.026f), 0.6f);
	const FSurf Wood = FSurf::Wood(FLinearColor(0.15f, 0.095f, 0.055f));
	const FSurf Bone = FSurf::Stone(FLinearColor(0.62f, 0.58f, 0.5f), 0.7f);
	const FSurf Rock = FSurf::Stone(Stone, 0.9f);
	const FSurf RockAlt = FSurf::Stone(StoneAlt, 0.9f);
	const FSurf Trim = bFrost ? Bone : FSurf::Steel(TrimC, 0.5f);
	const FSurf Ice = FSurf::Glass(FLinearColor(0.45f, 0.62f, 0.82f), 0.05f);
	TArray<FWRLookBlock> Blocks;
	TArray<FWRLookBlock> MoveBlocks;
	FKit K(GetActorTransform(), Blocks);
	Moving = NewObject<USceneComponent>(this);
	Moving->SetupAttachment(RootComponent);
	Moving->RegisterComponent();
	FKit KM(GetActorTransform(), MoveBlocks);

	if (Kind == TEXT("door") || Kind == TEXT("bossdoor"))
	{
		// The leaf fills the 4 x 6 m socket; it slides up into the slot in the lintel (or, ice, slumps and melts).
		const bool bIceDoor = Sub == 1;
		Block = WRFoldLook::Block(this, Moving, FVector(0, 0, 300.f), FVector(bFrost ? 50.f : 30.f, 200.f, 300.f));
		if (State == 1) Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (bDraw)
		{
			if (bIceDoor)
			{
				KM.Add(Ice.Dim(0.9f), FVector(70.f, 400.f, 600.f), FVector(0, 0, 300.f));
				for (int32 r = 0; r < 4; ++r) KM.Add(Ice.Dim(1.2f), FVector(80.f, 380.f, 10.f), FVector(0, 0, 80.f + 140.f * r));
			}
			else if (bFrost)
			{
				// A grate of whale-bone staves lashed to driftwood rails.
				for (int32 b = 0; b < 9; ++b) KM.Add(Bone.Dim(0.85f + 0.2f * KM.Rand(FVector(b, 0, 0), 3)), FVector(18.f, 22.f, 590.f), FVector(0, -180.f + 45.f * b, 295.f));
				for (const float Z : { 90.f, 300.f, 520.f }) KM.Add(Wood.Dim(0.7f), FVector(26.f, 400.f, 22.f), FVector(-14.f, 0, Z));
			}
			else
			{
				// A basalt slab with two bronze bands and a drop bar across it.
				KM.Add(Rock.Dim(1.2f), FVector(50.f, 400.f, 600.f), FVector(0, 0, 300.f));
				for (const float Z : { 140.f, 460.f }) KM.Add(Trim, FVector(56.f, 404.f, 22.f), FVector(0, 0, Z));
				for (int32 r = 0; r < 6; ++r) KM.Add(Trim.Dim(1.2f), FVector(60.f, 10.f, 10.f), FVector(0, -175.f + 70.f * r, 140.f));
			}
			if (Kind == TEXT("bossdoor"))
			{
				// The boss door's rune band in the rank colour.
				KM.Add(FSurf::Lit(RankC, 0.8f), FVector(60.f, 300.f, 8.f), FVector(0, 0, 300.f));
				KM.Add(FSurf::Lit(RankC, 0.8f), FVector(60.f, 8.f, 300.f), FVector(0, 0, 300.f));
			}
		}
	}
	else if (Kind == TEXT("stone"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 120.f), FVector(45.f, 60.f, 120.f));
		if (bDraw)
		{
			K.Add(RockAlt.Dim(0.9f), FVector(150.f, 150.f, 30.f), FVector(0, 0, 15.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(Rock.Dim(1.3f), FVector(80.f, 110.f, 230.f), FVector(0, 0, 145.f), FRotator(0.f, 0.f, 2.f));
			K.Add(Rock.Dim(1.4f), FVector(86.f, 116.f, 20.f), FVector(0, 0, 268.f));
			// The run-back rune: a carved line glowing in the rank colour, on both faces.
			for (const float S : { -1.f, 1.f })
			{
				K.Add(FSurf::Lit(RankC, 0.7f), FVector(4.f, 10.f, 150.f), FVector(S * 41.f, 0, 150.f));
				K.Add(FSurf::Lit(RankC, 0.7f), FVector(4.f, 60.f, 8.f), FVector(S * 41.f, 0, 190.f), FRotator(0.f, 0.f, 30.f));
				K.Add(FSurf::Lit(RankC, 0.7f), FVector(4.f, 60.f, 8.f), FVector(S * 41.f, 0, 130.f), FRotator(0.f, 0.f, -30.f));
			}
			Light = WRFoldLook::Light(this, RootComponent, FVector(80.f, 0, 160.f), RankC, 2200.f, 700.f);
			Fx = WRFx::Attach(RootComponent, TEXT("runback.rune"), NAME_None, 1.f);
		}
	}
	else if (Kind == TEXT("chest") || Kind == TEXT("vault"))
	{
		const bool bVault = Kind == TEXT("vault");
		const FVector Half = bVault ? FVector(70.f, 110.f, 55.f) : FVector(45.f, 70.f, 40.f);
		Block = WRFoldLook::Block(this, RootComponent, FVector(0, 0, Half.Z), Half);
		if (bDraw)
		{
			const FSurf Body = bVault ? RockAlt.Dim(1.2f) : (bFrost ? Wood.Dim(1.3f) : Wood);
			K.Add(Body, Half * 2.f - FVector(0, 0, 20.f), FVector(0, 0, Half.Z - 10.f));
			for (const float Y : { -Half.Y * 0.6f, Half.Y * 0.6f }) K.Add(bFrost ? Bone : Iron, FVector(Half.X * 2.f + 4.f, 10.f, Half.Z * 2.f - 16.f), FVector(0, Y, Half.Z - 10.f));
			K.Add(Trim, FVector(10.f, 18.f, 22.f), FVector(Half.X + 3.f, 0, Half.Z * 1.4f));
			// The lid: hinged at the back; it shows open for whoever has taken theirs.
			KM.Add(Body.Dim(1.08f), FVector(Half.X * 2.f, Half.Y * 2.f, 20.f), FVector(Half.X, 0, 10.f));
			KM.Add(bFrost ? Bone : Iron, FVector(Half.X * 2.f + 4.f, 10.f, 24.f), FVector(Half.X, -Half.Y * 0.6f, 10.f));
			KM.Add(bFrost ? Bone : Iron, FVector(Half.X * 2.f + 4.f, 10.f, 24.f), FVector(Half.X, Half.Y * 0.6f, 10.f));
			Moving->SetRelativeLocation(FVector(-Half.X, 0, Half.Z * 2.f - 20.f));
			if (bVault) Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, 140.f), RankC, 1200.f, 500.f);
		}
	}
	else if (Kind == TEXT("grate"))
	{
		if (bDraw)
		{
			// The lava channel under the floor, the grate over it: six bars in an iron frame.
			K.Add(FSurf::Stone(FLinearColor(0.01f, 0.008f, 0.007f), 0.98f), FVector(170.f, 170.f, 4.f), FVector(0, 0, -6.f));
			K.Add(FSurf::Lit(FLinearColor(0.7f, 0.12f, 0.02f), 0.35f), FVector(150.f, 150.f, 4.f), FVector(0, 0, -30.f));
			for (const float S : { -1.f, 1.f })
			{
				K.Add(Iron, FVector(170.f, 12.f, 8.f), FVector(0, S * 80.f, 2.f));
				K.Add(Iron, FVector(12.f, 170.f, 8.f), FVector(S * 80.f, 0, 2.f));
			}
			for (int32 b = 0; b < 6; ++b) KM.Add(Iron.Dim(1.1f), FVector(150.f, 9.f, 8.f), FVector(0, -62.f + 25.f * b, 2.f));
			GlowMat = WRFoldLook::Glow(this, FLinearColor(1.f, 0.25f, 0.03f), 0.f);
			if (UStaticMeshComponent* G = WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(150.f, 150.f, 4.f), FVector(0, 0, -2.f), FRotator::ZeroRotator, GlowMat)) Glows.Add(G);
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, 60.f), FLinearColor(1.f, 0.35f, 0.08f), 0.f, 600.f);
		}
	}
	else if (Kind == TEXT("block"))
	{
		SetReplicateMovement(true);
		Block = WRFoldLook::Block(this, RootComponent, FVector(0, 0, 40.f), FVector(70.f, 70.f, 40.f));
		if (bDraw)
		{
			K.Add(Iron.Dim(1.4f), FVector(140.f, 140.f, 70.f), FVector(0, 0, 35.f));
			K.Add(Iron.Dim(1.7f), FVector(146.f, 146.f, 10.f), FVector(0, 0, 74.f));
			K.Add(Iron, FVector(20.f, 60.f, 16.f), FVector(78.f, 0, 50.f));
		}
	}
	else if (Kind == TEXT("chute"))
	{
		if (bDraw)
		{
			// In the ceiling: a slot with a stone lip; above it the crucible's glow; the lip's rope runs over a pulley to
			// a cleat on the wall (Goal), the bell hangs off the chute's chain.
			const float Up = Size.Z;
			K.Add(Rock.Dim(0.6f), FVector(320.f, 140.f, 40.f), FVector(0, 0, Up - 20.f));
			K.Add(FSurf::Lit(FLinearColor(1.f, 0.4f, 0.06f), 0.6f), FVector(280.f, 100.f, 6.f), FVector(0, 0, Up + 12.f));
			KM.Add(Trim, FVector(300.f, 40.f, 14.f), FVector(0, 0, 0.f));
			Moving->SetRelativeLocation(FVector(0, 60.f, Up - 45.f));
			const FVector Cleat = GetActorTransform().InverseTransformPosition(Goal);
			K.Rod(FSurf::Cloth(FLinearColor(0.35f, 0.28f, 0.18f)), FVector(0, 60.f, Up - 40.f), FVector(Cleat.X, Cleat.Y, Up - 40.f), 4.f);
			K.Rod(FSurf::Cloth(FLinearColor(0.35f, 0.28f, 0.18f)), FVector(Cleat.X, Cleat.Y, Up - 40.f), Cleat + FVector(0, 0, 150.f), 4.f);
			K.Add(Iron, FVector(30.f, 12.f, 40.f), Cleat + FVector(0, 0, 140.f));
			K.Add(Trim, FVector(36.f, 36.f, 40.f), FVector(0, -110.f, Up - 120.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			K.Rod(Iron, FVector(0, -110.f, Up - 100.f), FVector(0, -110.f, Up - 10.f), 3.f);
			// The strip below: scorched floor that glows while the slag lies on it.
			GlowMat = WRFoldLook::Glow(this, FLinearColor(1.f, 0.3f, 0.04f), 0.f);
			if (UStaticMeshComponent* G = WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(300.f, 600.f, 3.f), FVector(0, 0, 2.f), FRotator::ZeroRotator, GlowMat)) Glows.Add(G);
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, 120.f), FLinearColor(1.f, 0.35f, 0.08f), 0.f, 700.f);
		}
	}
	else if (Kind == TEXT("icicle"))
	{
		if (bDraw)
		{
			// The icicle hangs from the ceiling (Size.Z up); its root is a crust of rime.
			K.Add(Ice.Dim(1.3f), FVector(90.f, 90.f, 20.f), FVector(0, 0, -10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			KM.Spike(FSurf::Glass(FLinearColor(0.7f, 0.86f, 1.f), 0.15f), FVector(0, 0, -10.f), -FVector::UpVector, 46.f, 170.f);
			KM.Spike(FSurf::Glass(FLinearColor(0.6f, 0.8f, 1.f), 0.1f), FVector(25.f, 10.f, -10.f), -FVector::UpVector, 24.f, 90.f);
			KM.Spike(FSurf::Glass(FLinearColor(0.6f, 0.8f, 1.f), 0.1f), FVector(-20.f, -15.f, -10.f), -FVector::UpVector, 20.f, 70.f);
			// Its shadow on the floor grows through the tell.
			GlowMat = WRFoldLook::Glow(this, FLinearColor(0.f, 0.f, 0.f), 0.f);
			if (UStaticMeshComponent* G = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(300.f, 300.f, 1.f), FVector(0, 0, -Size.Z + 3.f), FRotator::ZeroRotator, GlowMat)) { G->SetVisibility(false); Glows.Add(G); }
		}
	}
	else if (Kind == TEXT("pane"))
	{
		Block = WRFoldLook::Block(this, RootComponent, FVector(0, 0, -15.f), FVector(Size.X * 0.5f, Size.Y * 0.5f, 15.f));
		if (bDraw)
		{
			// Thin ice: grey-white, full of air, lighter than the safe dark-blue; cracks show while it creaks.
			KM.Add(FSurf::Glaze(FLinearColor(0.62f, 0.66f, 0.7f), 0.3f), FVector(Size.X - 4.f, Size.Y - 4.f, 12.f), FVector(0, 0, -6.f));
			GlowMat = WRFoldLook::Glow(this, FLinearColor(0.9f, 0.95f, 1.f), 0.f);
			for (int32 c = 0; c < 5; ++c)
			{
				if (UStaticMeshComponent* G = WRFoldLook::Shape(this, Moving, TEXT("Cube"), FVector(Size.X * 0.6f, 3.f, 2.f), FVector(0, 0, 1.f), FRotator(0.f, 36.f * c + 10.f, 0.f), GlowMat)) { G->SetVisibility(false); Glows.Add(G); }
			}
		}
	}
	else if (Kind == TEXT("quench"))
	{
		// The anvil and the small forge beside it (the blank heats there); the lintel band at Goal shows the temper.
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 45.f), FVector(60.f, 60.f, 45.f));
		if (bDraw)
		{
			K.Add(Wood.Dim(0.8f), FVector(90.f, 90.f, 55.f), FVector(0, 0, 27.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(Iron, FVector(80.f, 34.f, 34.f), FVector(0, 0, 72.f));
			GlowMat = WRFoldLook::Glow(this, FLinearColor(1.f, 0.45f, 0.08f), 4.f);
			if (UStaticMeshComponent* G = WRFoldLook::Shape(this, Moving, TEXT("Cube"), FVector(70.f, 9.f, 4.f), FVector::ZeroVector, FRotator::ZeroRotator, GlowMat)) Glows.Add(G);
			Moving->SetRelativeLocation(FVector(0, 0, 92.f));
			const FVector Band = GetActorTransform().InverseTransformPosition(Goal);
			K.Add(FSurf::Glaze(FLinearColor(Tint), 0.3f), FVector(20.f, 300.f, 30.f), Band);
			K.Add(Trim, FVector(22.f, 310.f, 6.f), Band + FVector(0, 0, 18.f));
			Light = WRFoldLook::Light(this, Moving, FVector(0, 0, 20.f), FLinearColor(1.f, 0.5f, 0.15f), 2500.f, 500.f);
		}
	}
	else if (Kind == TEXT("trough"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 45.f), FVector(110.f, 60.f, 45.f));
		if (bDraw)
		{
			for (const float S : { -1.f, 1.f })
			{
				K.Add(RockAlt, FVector(220.f, 18.f, 90.f), FVector(0, S * 51.f, 45.f));
				K.Add(RockAlt, FVector(18.f, 120.f, 90.f), FVector(S * 101.f, 0, 45.f));
			}
			K.Add(RockAlt.Dim(0.7f), FVector(220.f, 120.f, 20.f), FVector(0, 0, 10.f));
			// Water clear-dark, oil amber-black and glossy, brine pale with a salt crust on the rim.
			const FSurf Liquid = Sub == 0 ? FSurf::Glass(FLinearColor(0.02f, 0.04f, 0.06f), 0.f) : Sub == 1 ? FSurf::Glaze(FLinearColor(0.08f, 0.04f, 0.01f), 0.05f) : FSurf::Glass(FLinearColor(0.25f, 0.3f, 0.28f), 0.f);
			K.Add(Liquid, FVector(184.f, 84.f, 4.f), FVector(0, 0, 78.f));
			if (Sub == 2) K.Add(FSurf::Stone(FLinearColor(0.85f, 0.85f, 0.8f), 0.95f), FVector(224.f, 124.f, 6.f), FVector(0, 0, 91.f));
			if (Sub == 1) K.Add(Wood.Dim(0.6f), FVector(30.f, 30.f, 50.f), FVector(-140.f, 0, 25.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
	}
	else if (Kind == TEXT("forge"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 70.f), FVector(160.f, 110.f, 70.f));
		if (bDraw)
		{
			// The great forge: a stone hearth with coals, a hood up into the ceiling, and the hammer automaton's cam
			// shaft running along the wall from the wheel slot to its arm.
			K.Add(RockAlt.Dim(1.1f), FVector(320.f, 220.f, 120.f), FVector(0, 0, 60.f));
			K.Add(FSurf::Lit(FLinearColor(1.f, 0.38f, 0.06f), 0.8f), FVector(260.f, 160.f, 10.f), FVector(0, 0, 124.f));
			K.Add(Rock.Dim(0.9f), FVector(300.f, 200.f, 30.f), FVector(0, 0, 360.f));
			for (const float S : { -1.f, 1.f }) K.Add(Rock, FVector(40.f, 40.f, 230.f), FVector(S * 130.f, -80.f, 245.f));
			K.Add(Rock.Dim(0.8f), FVector(160.f, 120.f, Size.Z), FVector(0, -40.f, 375.f + Size.Z * 0.5f));
			K.Rod(Iron, FVector(-400.f, -150.f, 260.f), FVector(160.f, -150.f, 260.f), 22.f);
			for (int32 c = 0; c < 3; ++c) K.Add(Iron.Dim(1.3f), FVector(30.f, 50.f, 60.f), FVector(-200.f + 140.f * c, -150.f, 260.f), FRotator(0.f, 0.f, 40.f * c));
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 120.f, 200.f), FLinearColor(1.f, 0.45f, 0.12f), 6000.f, 1500.f);
			Fx = WRFx::Attach(RootComponent, TEXT("hearth.fire"), NAME_None, 1.2f);
			if (Fx) Fx->SetRelativeLocation(FVector(0, 0, 130.f));
		}
	}
	else if (Kind == TEXT("hammer"))
	{
		if (bDraw)
		{
			// The automaton: an iron arm on the cam shaft, a hammer head at its end, the striking anvil under it.
			K.Add(Iron.Dim(1.2f), FVector(60.f, 60.f, 70.f), FVector(0, 0, 35.f));
			K.Add(Iron, FVector(440.f, 50.f, 14.f), FVector(0, 120.f, 72.f));
			// Three striking spots, worn bright, one under each bellows.
			for (int32 s = 0; s < 3; ++s) K.Add(FSurf::Steel(FLinearColor(0.2f, 0.19f, 0.18f), 0.25f), FVector(40.f, 40.f, 3.f), FVector(-140.f + 140.f * s, 120.f, 80.f));
			KM.Rod(Iron, FVector::ZeroVector, FVector(0, 0, -150.f), 20.f, true);
			KM.Add(Iron.Dim(1.3f), FVector(70.f, 50.f, 50.f), FVector(0, 0, -170.f));
			Moving->SetRelativeLocation(FVector(0, 120.f, 280.f));
		}
	}
	else if (Kind == TEXT("bellows"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 60.f), FVector(80.f, 60.f, 60.f));
		if (bDraw)
		{
			// A double bellows of oak boards and leather on a stand, its nozzle into the forge; the lever on top.
			K.Add(Wood.Dim(0.8f), FVector(150.f, 110.f, 40.f), FVector(0, 0, 20.f));
			KM.Add(FSurf::Cloth(FLinearColor(0.28f, 0.17f, 0.09f)), FVector(140.f, 100.f, 50.f), FVector(0, 0, 25.f));
			KM.Add(Wood, FVector(150.f, 110.f, 10.f), FVector(0, 0, 55.f));
			KM.Rod(Wood.Dim(0.7f), FVector(-60.f, 0, 60.f), FVector(-160.f, 0, 120.f), 10.f);
			K.Spike(Iron, FVector(75.f, 0, 30.f), FVector(1.f, 0.f, 0.f), 26.f, 60.f);
			Moving->SetRelativeLocation(FVector(0, 0, 40.f));
		}
	}
	else if (Kind == TEXT("brazier") || Kind == TEXT("arenabrazier"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 60.f), FVector(45.f, 45.f, 60.f));
		if (bDraw)
		{
			for (int32 l = 0; l < 3; ++l) K.Rod(Iron, FRotator(0.f, 120.f * l, 0.f).Vector() * 42.f, FVector(0, 0, 100.f), 6.f);
			K.Add(Iron.Dim(1.2f), FVector(86.f, 86.f, 28.f), FVector(0, 0, 104.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			// Frozen over: a cap of rime until it's lit.
			KM.Add(FSurf::Glass(FLinearColor(0.75f, 0.85f, 0.95f), 0.05f), FVector(84.f, 84.f, 16.f), FVector(0, 0, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Moving->SetRelativeLocation(FVector(0, 0, 124.f));
			if (Kind == TEXT("brazier"))
			{
				// The carving over it: a stone plaque with the sun at its height on the day's arc (east to west).
				const int32 N = FMath::Max(1, int32(Size.X));
				const float Tday = (Sub + 0.5f) / N;
				const FVector Plaque(0, 0, 260.f);
				K.Add(RockAlt.Dim(1.2f), FVector(16.f, 120.f, 90.f), Plaque);
				for (int32 a = 0; a < 7; ++a) K.Add(RockAlt.Dim(0.6f), FVector(18.f, 10.f, 4.f), Plaque + FVector(0, -50.f + 100.f * a / 6.f, -30.f + 55.f * FMath::Sin(PI * a / 6.f)));
				K.Add(FSurf::Lit(FLinearColor(1.f, 0.75f, 0.4f), 0.3f), FVector(20.f, 20.f, 20.f), Plaque + FVector(0, -50.f + 100.f * Tday, -30.f + 55.f * FMath::Sin(PI * Tday)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, 170.f), FLinearColor(1.f, 0.55f, 0.2f), 0.f, Kind == TEXT("arenabrazier") ? WRTune(TEXT("bosses.rime_witch.lightRadius"), 500.f) * 2.f : 1200.f);
		}
	}
	else if (Kind == TEXT("flame"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 50.f), FVector(40.f, 40.f, 50.f));
		if (bDraw)
		{
			K.Add(RockAlt, FVector(70.f, 70.f, 90.f), FVector(0, 0, 45.f));
			K.Add(FSurf::Stone(FLinearColor(0.2f, 0.21f, 0.19f), 0.6f), FVector(50.f, 50.f, 12.f), FVector(0, 0, 96.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(FSurf::Lit(FLinearColor(1.f, 0.6f, 0.25f), 0.8f), FVector(6.f, 36.f, 8.f), FVector(18.f, 0, 104.f));
			Light = WRFoldLook::Light(this, RootComponent, FVector(30.f, 0, 130.f), FLinearColor(1.f, 0.6f, 0.3f), 2400.f, 900.f);
			GlowMat = WRFoldLook::Glow(this, FLinearColor(1.f, 0.55f, 0.2f), 5.f);
			if (UStaticMeshComponent* G = WRFoldLook::Shape(this, Moving, TEXT("Sphere"), FVector(12.f), FVector::ZeroVector, FRotator::ZeroRotator, GlowMat)) { G->SetVisibility(false); Glows.Add(G); }
		}
	}
	else if (Kind == TEXT("prism"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 60.f), FVector(40.f, 40.f, 60.f));
		if (bDraw)
		{
			K.Add(RockAlt, FVector(70.f, 70.f, 90.f), FVector(0, 0, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			// The prism turns on its stand; its beam runs to its socket on the door.
			KM.Add(FSurf::Glass(FLinearColor(0.8f, 0.9f, 1.f), 0.15f), FVector(50.f, 50.f, 70.f), FVector(0, 0, 35.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			Moving->SetRelativeLocation(FVector(0, 0, 90.f));
			GlowMat = WRFoldLook::Glow(this, FLinearColor(FoldAurora[0]), 3.f);
			const FVector To = GetActorTransform().InverseTransformPosition(Goal) - FVector(0, 0, 120.f);
			if (UStaticMeshComponent* G = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(6.f, 6.f, To.Size()), To * 0.5f + FVector(0, 0, 120.f), FRotationMatrix::MakeFromZ(To.GetSafeNormal()).Rotator(), GlowMat)) Glows.Add(G);
		}
	}
	else if (Kind == TEXT("skylight"))
	{
		if (bDraw)
		{
			// A shaft up through the ceiling with the aurora moving in it: three bands of colour, slowly waving.
			for (int32 b = 0; b < 3; ++b)
			{
				const FColor C = FoldAurora[Seq.IsValidIndex(b) ? Seq[b] : b];
				UMaterialInstanceDynamic* M = WRFoldLook::Glow(this, FLinearColor(C), 2.5f);
				if (UStaticMeshComponent* G = WRFoldLook::Shape(this, Moving, TEXT("Cube"), FVector(240.f, 140.f, 6.f), FVector(0, -150.f + 150.f * b, 0.f), FRotator(0.f, 0.f, 8.f), M)) Glows.Add(G);
			}
			Moving->SetRelativeLocation(FVector(0, 0, Size.Z - 30.f));
			// The three sockets over the door (Goal), each a crystal that blazes when its beam is right.
			const FVector Door = GetActorTransform().InverseTransformPosition(Goal);
			for (int32 b = 0; b < 3; ++b) K.Add(FSurf::Glass(FLinearColor(FoldAurora[Seq.IsValidIndex(b) ? Seq[b] : b]) * 0.3f, 0.1f), FVector(30.f, 30.f, 30.f), Door + FVector(0, -150.f + 150.f * b, 0.f));
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, Size.Z - 200.f), FLinearColor(0.4f, 0.9f, 0.7f), 3000.f, 1500.f);
		}
	}
	else if (Kind == TEXT("spring"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 40.f), FVector(90.f, 90.f, 40.f));
		if (bDraw)
		{
			// A stone basin fed from a crack in the wall: real water you can drink, the only quiet place in a fold.
			K.Add(RockAlt.Dim(1.1f), FVector(200.f, 200.f, 80.f), FVector(0, 0, 40.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(FSurf::Glass(FLinearColor(0.03f, 0.06f, 0.08f), 0.f), FVector(170.f, 170.f, 4.f), FVector(0, 0, 78.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(FSurf::Glass(FLinearColor(0.3f, 0.45f, 0.55f), 0.05f), FVector(8.f, 30.f, 160.f), FVector(-95.f, 0, 160.f));
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, 120.f), FLinearColor(0.6f, 0.75f, 0.9f), 900.f, 500.f);
		}
	}
	else if (Kind == TEXT("clutch"))
	{
		Block = WRFoldLook::Block(this, RootComponent, FVector(0, 0, 50.f), FVector(90.f, 90.f, 50.f));
		Block->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		if (bDraw)
		{
			// A clutch of leathery eggs on a hot rock, veined with ore-light.
			K.Add(FSurf::Stone(FLinearColor(0.03f, 0.025f, 0.02f), 0.95f), FVector(200.f, 180.f, 40.f), FVector(0, 0, 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 e = 0; e < 7; ++e)
			{
				const FVector C = FRotator(0.f, 51.f * e, 0.f).Vector() * (e == 0 ? 0.f : 50.f) + FVector(0, 0, 65.f);
				KM.Add(FSurf::Glaze(FLinearColor(0.22f, 0.12f, 0.05f), 0.4f), FVector(50.f, 50.f, 66.f), C, FRotator(10.f * e, 0.f, 0.f), EWRLookMesh::Cylinder);
				KM.Add(FSurf::Lit(FLinearColor(1.f, 0.45f, 0.1f), 0.4f), FVector(6.f, 52.f, 4.f), C + FVector(0, 0, 10.f), FRotator(10.f * e, 0.f, 0.f));
			}
			Light = WRFoldLook::Light(this, RootComponent, FVector(0, 0, 120.f), FLinearColor(1.f, 0.45f, 0.1f), 1500.f, 600.f);
		}
	}
	else if (Kind == TEXT("crucible"))
	{
		if (bDraw)
		{
			// A great crucible on a chain from a ceiling beam, a haul chain down to a floor ring (where you stand).
			const float Up = Size.Z;
			KM.Add(Iron, FVector(160.f, 160.f, 130.f), FVector(0, 0, 0.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			KM.Add(FSurf::Lit(FLinearColor(1.f, 0.4f, 0.05f), 0.95f), FVector(140.f, 140.f, 6.f), FVector(0, 0, 64.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			KM.Spike(Iron, FVector(80.f, 0, 50.f), FVector(1.f, 0.f, 0.2f), 30.f, 50.f);
			Moving->SetRelativeLocation(FVector(0, 0, Up));
			K.Rod(Iron, FVector(0, 0, Up + 60.f), FVector(0, 0, Up + 600.f), 8.f);
			// The haul chain runs over a pulley on the beam and down to a ring bolted in the floor, clear of the pour.
			K.Rod(Iron, FVector(-70.f, 0, Up + 40.f), FVector(-450.f, 0, Up + 300.f), 6.f);
			K.Add(Iron.Dim(1.3f), FVector(40.f, 14.f, 40.f), FVector(-450.f, 0, Up + 300.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
			K.Rod(Iron, FVector(-450.f, 0, Up + 300.f), FVector(-450.f, 0, 12.f), 6.f);
			K.Add(Iron.Dim(1.4f), FVector(40.f, 40.f, 10.f), FVector(-450.f, 0, 5.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Light = WRFoldLook::Light(this, Moving, FVector(0, 0, -80.f), FLinearColor(1.f, 0.4f, 0.08f), 4000.f, 1000.f);
		}
	}
	else if (Kind == TEXT("tank"))
	{
		WRFoldLook::Block(this, RootComponent, FVector(0, 0, 70.f), FVector(160.f, 110.f, 70.f));
		if (bDraw)
		{
			// A quench tank of riveted plate on a stone footing, fed by a pipe from the cistern behind its grille.
			for (const float S : { -1.f, 1.f })
			{
				K.Add(Iron, FVector(320.f, 16.f, 140.f), FVector(0, S * 104.f, 70.f));
				K.Add(Iron, FVector(16.f, 220.f, 140.f), FVector(S * 152.f, 0, 70.f));
			}
			for (int32 r = 0; r < 8; ++r) K.Add(Iron.Dim(1.6f), FVector(10.f, 10.f, 10.f), FVector(-140.f + 40.f * r, -112.f, 120.f));
			KM.Add(FSurf::Glass(FLinearColor(0.02f, 0.04f, 0.06f), 0.f), FVector(290.f, 190.f, 4.f), FVector::ZeroVector);
			Moving->SetRelativeLocation(FVector(0, 0, 125.f));
			const FVector Pipe = GetActorTransform().InverseTransformPosition(Goal);
			K.Rod(Iron.Dim(1.2f), FVector(0, 0, 150.f), FVector(Pipe.X, Pipe.Y, 150.f), 24.f);
			K.Rod(Iron.Dim(1.2f), FVector(Pipe.X, Pipe.Y, 150.f), FVector(Pipe.X, Pipe.Y, 20.f), 24.f);
		}
	}
	else if (Kind == TEXT("bolt"))
	{
		Block = WRFoldLook::Block(this, RootComponent, FVector::ZeroVector, FVector(50.f, 60.f, 60.f));
		Block->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		if (bDraw)
		{
			// A frost bolt driven through the ice window's frame: a column of blue-black ice bound with iron bands.
			KM.Add(FSurf::Glass(FLinearColor(0.15f, 0.3f, 0.55f), 0.1f), FVector(70.f, 70.f, 160.f), FVector::ZeroVector, FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			for (const float X : { -40.f, 40.f }) KM.Add(Iron, FVector(16.f, 80.f, 80.f), FVector(X, 0, 0), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		}
	}
	else if (Kind == TEXT("window"))
	{
		if (bDraw)
		{
			// The east wall's ice window: thick and dark until a bolt breaks, then a shaft of dawn through each crack.
			K.Add(FSurf::Glass(FLinearColor(0.1f, 0.2f, 0.4f), 0.08f), FVector(60.f, Size.Y, Size.Z), FVector(0, 0, Size.Z * 0.5f));
			for (int32 s = 0; s < 4; ++s)
			{
				UMaterialInstanceDynamic* M = WRFoldLook::Glow(this, FLinearColor(1.f, 0.78f, 0.5f), 1.5f);
				if (UStaticMeshComponent* G = WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(2400.f, 450.f, 300.f), FVector(-1200.f, (s % 2 ? 1.f : -1.f) * 800.f, s < 2 ? 400.f : 1100.f), FRotator(-14.f, 0.f, 0.f), M)) { G->SetVisibility(false); Glows.Add(G); }
			}
			Light = WRFoldLook::Light(this, RootComponent, FVector(-300.f, 0, 600.f), FLinearColor(1.f, 0.8f, 0.55f), 0.f, 3000.f);
		}
	}
	if (bDraw)
	{
		if (Blocks.Num()) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
		if (MoveBlocks.Num())
		{
			// The moving part's blocks are in its own frame (its relative location is set above).
			FWRBuildLook::Spawn(this, Moving, MoveBlocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
		}
	}
	ShowState();
}

void AWRFoldThing::ShowState()
{
	// Collision first, on every machine: a client's own movement runs against it.
	if (Block && bLookBuilt)
	{
		bool bSolid = true;
		if (Kind == TEXT("door") || Kind == TEXT("bossdoor")) bSolid = State == 0;
		else if (Kind == TEXT("pane")) bSolid = State != 2;
		else if (Kind == TEXT("clutch") || Kind == TEXT("bolt")) bSolid = !bDone;
		Block->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (GetNetMode() == NM_DedicatedServer || !bLookBuilt) return;
	const UWorld* World = GetWorld();
	const APlayerController* Local = World ? World->GetFirstPlayerController() : nullptr;
	const FString Me = Local && Local->PlayerState ? Local->PlayerState->GetPlayerName() : FString();
	if (Kind == TEXT("chest") || Kind == TEXT("vault"))
	{
		if (Moving) Moving->SetRelativeRotation(Names.Contains(Me) ? FRotator(-100.f, 0.f, 0.f) : FRotator::ZeroRotator);
	}
	else if (Kind == TEXT("grate"))
	{
		if (Moving) Moving->SetVisibility(true, true);
	}
	else if (Kind == TEXT("brazier") || Kind == TEXT("arenabrazier"))
	{
		const bool bLit = State == 1;
		if (Moving) Moving->SetVisibility(!bLit, true);
		if (Light) Light->SetIntensity(bLit ? 5000.f : 0.f);
		if (bLit && !Fx) { Fx = WRFx::Attach(RootComponent, TEXT("brazier.fire"), NAME_None, 0.9f); if (Fx) Fx->SetRelativeLocation(FVector(0, 0, 125.f)); }
		if (!bLit && Fx) { WRFx::Stop(Fx); Fx = nullptr; }
	}
	else if (Kind == TEXT("clutch") || Kind == TEXT("bolt"))
	{
		if (Moving) Moving->SetVisibility(!bDone, true);
		if (Light) Light->SetVisibility(!bDone);
	}
	else if (Kind == TEXT("icicle"))
	{
		if (bDone && Moving) Moving->SetVisibility(false, true);
	}
	else if (Kind == TEXT("pane"))
	{
		if (Moving) Moving->SetVisibility(State != 2, true);
		for (UStaticMeshComponent* G : Glows) if (G) G->SetVisibility(State == 1);
		if (GlowMat) GlowMat->SetScalarParameterValue(TEXT("Glow"), State == 1 ? 2.f : 0.f);
		if (State == 1) WRSound::PlayAt(this, TEXT("portal.ice_creak"), GetActorLocation());
	}
	else if (Kind == TEXT("crucible"))
	{
		if (Moving) Moving->SetRelativeRotation(State == 1 ? FRotator(0.f, 0.f, 70.f) : FRotator::ZeroRotator);
	}
	else if (Kind == TEXT("tank"))
	{
		if (Moving) Moving->SetVisibility(State == 0, true);
	}
	else if (Kind == TEXT("skylight") && bDone)
	{
		if (Light) Light->SetIntensity(9000.f);
	}
	else if (Kind == TEXT("window"))
	{
		// One shaft of dawn per broken bolt (State: a bit per bolt).
		int32 Lit = 0;
		for (int32 i = 0; i < Glows.Num(); ++i) { const bool bOn = (State & (1 << i)) != 0; if (Glows[i]) Glows[i]->SetVisibility(bOn); Lit += bOn; }
		if (Light) Light->SetIntensity(Lit * 4000.f);
	}
}

void AWRFoldThing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) StepServer(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || !bLookBuilt) return;
	const UWorld* World = GetWorld();
	const float Now = float(WRFoldHazards::Now(this));
	if ((Kind == TEXT("door") || Kind == TEXT("bossdoor")) && Moving)
	{
		// Slides up (2 s) into its lintel, or drops (0.5 s) shut; an ice door slumps and melts away (3 s).
		const bool bIce = Sub == 1;
		const float Since = Now - TimeA;
		const float Open = State == 1 ? FMath::Clamp(Since / (bIce ? 3.f : 2.f), 0.f, 1.f) : 1.f - FMath::Clamp(Since / 0.5f, 0.f, 1.f);
		const float Ease = Open * Open * (3.f - 2.f * Open);
		if (bIce) { Moving->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(0.01f, 1.f - Ease))); Moving->SetVisibility(Ease < 0.99f, true); }
		else Moving->SetRelativeLocation(FVector(0, 0, Ease * 590.f));
		if (State != ShownState)
		{
			if (ShownState != -100 && bIce && State == 1) WRFx::SpawnAt(this, TEXT("reaction.steam"), GetActorLocation() + FVector(0, 0, 200.f), FRotator::ZeroRotator, 2.f);
			ShownState = State;
		}
		return;
	}
	if (Kind == TEXT("grate"))
	{
		// The tell and the well-up, on the shared clock: dull red rising 1.2 s with a hiss, 2 s of lava, then dark.
		const float Phase = bDone ? 99.f : FMath::Fmod(Now - TimeA + 1000.f * Period, Period);
		const float Heat = Phase < 1.2f ? Phase / 1.2f * 0.6f : Phase < 3.2f ? 1.f : FMath::Max(0.f, 1.f - (Phase - 3.2f) * 2.f);
		if (GlowMat) GlowMat->SetScalarParameterValue(TEXT("Glow"), Heat * 5.f);
		if (Light) Light->SetIntensity(Heat * 6000.f);
		if (Phase < LastShown && !bDone && FVector::DistSquared(GetActorLocation(), World->GetFirstPlayerController() && World->GetFirstPlayerController()->GetPawn() ? World->GetFirstPlayerController()->GetPawn()->GetActorLocation() : FVector(1e9f)) < FMath::Square(2500.f))
		{
			WRSound::PlayAt(this, TEXT("portal.grate_hiss"), GetActorLocation());
		}
		if (LastShown < 1.2f && Phase >= 1.2f && Phase < 1.5f && !bDone) WRFx::SpawnAt(this, TEXT("lava.embers"), GetActorLocation() + FVector(0, 0, 20.f));
		LastShown = Phase;
		if (Moving) Moving->SetVisibility(true, true);
		return;
	}
	if (Kind == TEXT("chute"))
	{
		// The bell rings once 1.0 s before the pour; the lip swings open; slag glows on the floor for 3 s.
		const float To = TimeA - Now;
		if (To <= 1.0f && To > 0.f && !bTold) { bTold = true; WRSound::PlayAt(this, TEXT("portal.bell"), GetActorLocation() + FVector(0, 0, Size.Z - 120.f)); }
		if (To > 1.0f) bTold = false;
		const float Lip = To <= 0.f && Now < TimeA + 3.f ? 1.f : To < 1.0f ? FMath::Clamp(1.f - To, 0.f, 0.3f) : 0.f;
		if (Moving) Moving->SetRelativeRotation(FRotator(0.f, 0.f, -70.f * Lip));
		const float Slag = Now >= TimeA && Now < TimeA + 3.f ? 1.f - (Now - TimeA) / 3.f : To < 1.f && To > 0.f ? 0.15f : 0.f;
		if (GlowMat) GlowMat->SetScalarParameterValue(TEXT("Glow"), Slag * 4.f);
		if (Light) Light->SetIntensity(Slag * 7000.f);
		return;
	}
	if (Kind == TEXT("icicle"))
	{
		if (TimeA > 0.f && !bDone)
		{
			const float To = TimeA - Now;
			if (!bTold) { bTold = true; WRSound::PlayAt(this, TEXT("portal.icicle_crack"), GetActorLocation()); WRFx::SpawnAt(this, TEXT("snow"), GetActorLocation(), FRotator::ZeroRotator, 0.6f); }
			if (Glows.Num() && Glows[0])
			{
				Glows[0]->SetVisibility(true);
				const float Grow = FMath::Clamp(1.f - To, 0.1f, 1.f);
				Glows[0]->SetRelativeScale3D(FVector(3.f * Grow, 3.f * Grow, 0.01f));
			}
			if (To < 0.f && Moving) Moving->SetRelativeLocation(FVector(0, 0, -FMath::Min(Size.Z, 0.5f * 980.f * To * To * 4.f)));
		}
		if (bDone)
		{
			if (Glows.Num() && Glows[0]) Glows[0]->SetVisibility(false);
			if (!bFired) { bFired = true; WRFx::SpawnAt(this, TEXT("hit.ice"), GetActorLocation() - FVector(0, 0, Size.Z), FRotator::ZeroRotator, 2.5f); WRSound::PlayAt(this, TEXT("portal.ice_break"), GetActorLocation() - FVector(0, 0, Size.Z)); }
		}
		return;
	}
	if (Kind == TEXT("quench") && Moving)
	{
		// The blank: on the anvil (glowing), cracked (dark, reheating), or in the carrier's tongs.
		APawn* Carrier = nullptr;
		if (Names.Num() > 0)
		{
			for (TActorIterator<APawn> It(World); It; ++It) if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Names[0]) Carrier = *It;
		}
		if (Carrier) Moving->SetWorldLocation(Carrier->GetActorLocation() + Carrier->GetActorForwardVector() * 60.f + FVector(0, 0, 10.f));
		else Moving->SetRelativeLocation(FVector(0, 0, 92.f));
		if (GlowMat) GlowMat->SetScalarParameterValue(TEXT("Glow"), bDone ? 0.3f : State == 2 ? 0.5f + 3.5f * FMath::Clamp(1.f - (TimeA - Now) / 20.f, 0.f, 1.f) : 4.f);
		return;
	}
	if (Kind == TEXT("hammer") && Moving && Link)
	{
		// The automaton plays the forge's rhythm: each beat it swings to one of the three spots and comes down.
		const AWRFoldThing* F = Link.Get();
		TArray<uint8> Seq2;
		if (F->Names.Num() > 0) for (const TCHAR C : F->Names[0]) Seq2.Add(uint8(C - TEXT('0')));
		const float T = Now - F->TimeA;
		const int32 Beat = FMath::FloorToInt(T / 0.8f);
		float Yaw = 0.f, Down = 0.f;
		if (!F->bDone && T >= 0.f && Seq2.IsValidIndex(Beat))
		{
			const float In = FMath::Fmod(T, 0.8f) / 0.8f;
			Yaw = (int32(Seq2[Beat]) - 1) * -35.f;
			Down = In < 0.5f ? In * 2.f : 2.f - In * 2.f;
			if (Beat != ShownState) { ShownState = Beat; WRSound::PlayAt(this, TEXT("portal.hammer_beat"), GetActorLocation() + FVector(0, 0, 80.f)); }
		}
		else if (T < 0.f) ShownState = -1;
		Moving->SetRelativeRotation(FRotator(-Down * 35.f, Yaw, 0.f));
		return;
	}
	if (Kind == TEXT("bellows") && Moving)
	{
		const float Since = Now - TimeA;
		const float Squash = Since < 0.6f ? FMath::Sin(Since / 0.6f * PI) : 0.f;
		Moving->SetRelativeScale3D(FVector(1.f, 1.f, 1.f - 0.45f * Squash));
		return;
	}
	if (Kind == TEXT("prism") && Moving)
	{
		const float Want = State * 60.f;
		const float Was = (State - 1) * 60.f;
		const float T = FMath::Clamp((Now - TimeA) / 0.6f, 0.f, 1.f);
		Moving->SetRelativeRotation(FRotator(0.f, FMath::Lerp(Was, Want, T * T * (3.f - 2.f * T)), 0.f));
		if (GlowMat) GlowMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(FoldAurora[State % 3]));
		return;
	}
	if (Kind == TEXT("skylight") && Moving)
	{
		for (int32 i = 0; i < Glows.Num(); ++i) if (Glows[i]) Glows[i]->SetRelativeRotation(FRotator(0.f, 0.f, 8.f * FMath::Sin(Now * 0.4f + i * 1.3f)));
		return;
	}
	if (Kind == TEXT("flame") && Glows.Num() && Glows[0])
	{
		APawn* Carrier = nullptr;
		if (Names.Num() > 0 && Now < TimeA)
		{
			for (TActorIterator<APawn> It(World); It; ++It) if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Names[0]) Carrier = *It;
		}
		Glows[0]->SetVisibility(Carrier != nullptr);
		if (Carrier) Moving->SetWorldLocation(Carrier->GetActorLocation() + Carrier->GetActorForwardVector() * 45.f + FVector(0, 0, 30.f));
		return;
	}
	if (Kind == TEXT("crucible") && Moving && State == 0)
	{
		Moving->SetRelativeRotation(FRotator(0.f, 0.f, 3.f * FMath::Sin(Now * 0.7f)));
	}
}

// ================================================================================================ populating a fold

void UWRPortalSystem::PopulateThings(AWRFold* F)
{
	UWorld* World = GetWorld();
	const auto Make = [&](FName Kind, const FVector& At, float Yaw, int32 Room, TFunctionRef<void(AWRFoldThing*)> Set) -> AWRFoldThing*
	{
		const FTransform T(FRotator(0.f, Yaw, 0.f), At);
		AWRFoldThing* Th = World->SpawnActorDeferred<AWRFoldThing>(AWRFoldThing::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Th) return nullptr;
		Th->Kind = Kind;
		Th->Biome = F->Biome;
		Th->Fold = F;
		Th->Room = uint8(Room);
		Set(Th);
		Th->FinishSpawning(T);
		return Th;
	};
	const auto None = [](AWRFoldThing*) {};
	const bool bFrost = F->Biome == TEXT("tundra");
	// The main line's chests: the rank's count over the rooms with a chest spot, spread toward the end.
	const TArray<TSharedPtr<FJsonValue>>* Rewards = PArr(WRPortals::Data(), TEXT("rewards"));
	const TSharedPtr<FJsonObject> RewardRow = Rewards && Rewards->IsValidIndex(F->Rank - 1) ? (*Rewards)[F->Rank - 1]->AsObject() : nullptr;
	const int32 Chests = int32(PNum(RewardRow, TEXT("chests"), 1.f));
	TArray<int32> ChestRooms;
	for (int32 i = 0; i < F->Rooms.Num(); ++i)
	{
		const FWRFoldRoomRep& R = F->Rooms[i];
		if (R.Side == 0 && R.Type != TEXT("entry") && R.Type != TEXT("boss") && R.Type != TEXT("rest") && F->RoomRow(i).IsValid() && F->RoomRow(i)->HasField(TEXT("chest"))) ChestRooms.Add(i);
	}
	TSet<int32> WithChest;
	for (int32 c = 0; c < Chests && ChestRooms.Num() > 0; ++c)
	{
		const int32 Pick = ChestRooms[FMath::Clamp(FMath::RoundToInt((ChestRooms.Num() - 1) * (c + 1.f) / Chests), 0, ChestRooms.Num() - 1)];
		WithChest.Add(Pick);
	}

	for (int32 i = 0; i < F->Rooms.Num(); ++i)
	{
		const FWRFoldRoomRep& R = F->Rooms[i];
		const TSharedPtr<FJsonObject> Row = F->RoomRow(i);
		if (!Row.IsValid()) continue;
		const FTransform RF = F->RoomFrame(i);
		const float Yaw = RF.Rotator().Yaw;
		const auto P = [&](const TSharedPtr<FJsonValue>& V, float Z = 0.f) { return F->RoomPoint(i, PAt(V, 0), PAt(V, 1), Z); };
		const FVector EastDoor = F->RoomPoint(i, R.W / 100.f, 0.f);
		// The entry stone.
		const FColor RankColour = WRPortals::RankColour(F->Rank);
		if (R.Type == TEXT("entry")) Make(TEXT("stone"), P(Row->Values.FindRef(TEXT("stone"))), Yaw, i, [&RankColour](AWRFoldThing* T) { T->Tint = RankColour; });
		// Doors: a fight or a puzzle shuts its way on; the boss room's door stands open until someone crosses it.
		const bool bNextIsMain = F->Rooms.IsValidIndex(i + 1) && F->Rooms[i + 1].Side == 0;
		AWRFoldThing* ExitDoor = nullptr;
		if (R.Side == 0 && bNextIsMain && (R.Type == TEXT("combat") || R.Type == TEXT("puzzle")))
		{
			const bool bIce = bFrost && R.Type == TEXT("puzzle");
			ExitDoor = Make(TEXT("door"), EastDoor + RF.GetRotation().RotateVector(FVector(bFrost ? 50.f : 30.f, 0, 0)), Yaw, i, [bIce](AWRFoldThing* T) { T->Sub = bIce ? 1 : 0; T->State = 0; });
		}
		if (R.Type == TEXT("boss")) Make(TEXT("bossdoor"), F->RoomPoint(i, 0.f, 0.f) - RF.GetRotation().RotateVector(FVector(40.f, 0, 0)), Yaw, i, [&RankColour](AWRFoldThing* T) { T->State = 1; T->Tint = RankColour; });
		// Chests.
		if (Row->HasField(TEXT("chest")) && (WithChest.Contains(i) || R.Type == TEXT("treasure") || R.Type == TEXT("rest")))
		{
			const bool bVault = R.Type == TEXT("treasure") && F->Rank >= 5;
			Make(bVault ? TEXT("vault") : TEXT("chest"), P(Row->Values.FindRef(TEXT("chest"))), Yaw + 180.f, i, [&R, &RankColour](AWRFoldThing* T) { T->Sub = R.Type == TEXT("treasure") ? 1 : 0; T->Tint = RankColour; });
		}
		// Rest springs.
		if (const TArray<TSharedPtr<FJsonValue>>* Feats = PArr(Row, TEXT("features")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Feats)
			{
				if (PAtStr(V, 0) != TEXT("spring")) continue;
				const TSharedRef<FJsonValueArray> XY = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueNumber>(PAt(V, 1)), MakeShared<FJsonValueNumber>(PAt(V, 2)) });
				Make(TEXT("spring"), P(XY), Yaw, i, None);
			}
		}
		// Traps.
		if (const TSharedPtr<FJsonObject> Trap = PChild(Row, TEXT("trap")))
		{
			int32 n = 0;
			if (const TArray<TSharedPtr<FJsonValue>>* G = PArr(Trap, TEXT("at")))
			{
				const FString K = PStr(Trap, TEXT("kind"));
				for (const TSharedPtr<FJsonValue>& V : *G)
				{
					if (K == TEXT("grates") || K == TEXT("chutes"))
						Make(TEXT("grate"), P(V), Yaw, i, [&n](AWRFoldThing* T) { T->Period = 4.5f; T->TimeA = 0.6f * n; });
					else if (K == TEXT("icicles"))
						Make(TEXT("icicle"), P(V, R.H - 10.f), Yaw, i, [&R](AWRFoldThing* T) { T->Size = FVector(150.f, 150.f, R.H - 10.f); });
					++n;
				}
			}
			if (const TArray<TSharedPtr<FJsonValue>>* G = PArr(Trap, TEXT("grates")))
				for (const TSharedPtr<FJsonValue>& V : *G) Make(TEXT("grate"), P(V), Yaw, i, [&n](AWRFoldThing* T) { T->Period = 4.5f; T->TimeA = 0.6f * n++; });
			if (const TArray<TSharedPtr<FJsonValue>>* G = PArr(Trap, TEXT("icicles")))
				for (const TSharedPtr<FJsonValue>& V : *G) Make(TEXT("icicle"), P(V, R.H - 10.f), Yaw, i, [&R](AWRFoldThing* T) { T->Size = FVector(150.f, 150.f, R.H - 10.f); });
			if (const TArray<TSharedPtr<FJsonValue>>* G = PArr(Trap, TEXT("blocks")))
				for (const TSharedPtr<FJsonValue>& V : *G) Make(TEXT("block"), P(V), Yaw, i, None);
			if (const TArray<TSharedPtr<FJsonValue>>* G = PArr(Trap, TEXT("chutes")))
			{
				int32 c = 0;
				for (const TSharedPtr<FJsonValue>& V : *G)
				{
					// The rope's cleat is on the nearer long wall.
					const float Y = PAt(V, 1);
					const FVector Cleat = F->RoomPoint(i, PAt(V, 0), (Y <= 0.f ? -1.f : 1.f) * (R.D / 200.f - 0.4f));
					Make(TEXT("chute"), P(V), Yaw + PAt(V, 2), i, [&](AWRFoldThing* T) { T->Size = FVector(300.f, 600.f, R.H); T->Period = 12.f; T->Health = 100.f; T->Goal = Cleat; T->TimeA = 0.f; ++c; });
				}
			}
			if (const TArray<TSharedPtr<FJsonValue>>* G = PArr(Trap, TEXT("panes")))
			{
				for (const TSharedPtr<FJsonValue>& V : *G)
				{
					const FVector At = F->RoomPoint(i, PAt(V, 0), PAt(V, 1));
					Make(TEXT("pane"), At, Yaw, i, [&V](AWRFoldThing* T) { T->Size = FVector(PAt(V, 2) * 100.f, PAt(V, 3) * 100.f, 30.f); });
				}
			}
		}
		// Puzzles.
		if (const TSharedPtr<FJsonObject> Pz = PChild(Row, TEXT("puzzle")))
		{
			const FString K = PStr(Pz, TEXT("kind"));
			const FVector Lintel = EastDoor + FVector(0, 0, FMath::Min(600.f, R.H - 60.f) + 30.f) - RF.GetRotation().RotateVector(FVector(20.f, 0, 0));
			if (K == TEXT("quench"))
			{
				// Straw -> oil, purple -> water, blue -> brine (WRFoldThingImpl::Temper is indexed by trough).
				const int32 Right = FMath::RandRange(0, 2);
				static const FColor Band[3] = { FColor(112, 70, 128), FColor(226, 196, 112), FColor(60, 96, 178) };
				AWRFoldThing* Ctl = Make(TEXT("quench"), P(Pz->Values.FindRef(TEXT("anvil"))), Yaw, i, [&](AWRFoldThing* T) { T->Seq = { uint8(Right) }; T->Tint = Band[Right]; T->Goal = Lintel; });
				if (const TArray<TSharedPtr<FJsonValue>>* Tr = PArr(Pz, TEXT("troughs")))
				{
					for (int32 t = 0; t < Tr->Num() && t < 3; ++t) Make(TEXT("trough"), P((*Tr)[t]), Yaw + 90.f, i, [&](AWRFoldThing* T) { T->Sub = uint8(t); T->Link = Ctl; });
				}
				if (const TSharedPtr<FJsonValue> Fg = Pz->Values.FindRef(TEXT("forge"))) Make(TEXT("forge"), P(Fg), Yaw, i, [&R](AWRFoldThing* T) { T->Size = FVector(0, 0, FMath::Max(0.f, R.H - 420.f)); T->bDone = true; });
			}
			else if (K == TEXT("bellows"))
			{
				const int32 N = FMath::RandRange(3, 5);
				TArray<uint8> Seq;
				for (int32 b = 0; b < N; ++b) Seq.Add(uint8(FMath::RandRange(0, 2)));
				AWRFoldThing* Ctl = Make(TEXT("forge"), P(Pz->Values.FindRef(TEXT("forge"))), Yaw, i, [&](AWRFoldThing* T)
				{
					T->Seq = Seq;
					T->Names = { FString::JoinBy(Seq, TEXT(""), [](uint8 B) { return FString::FromInt(B); }) };
					T->TimeA = 0.f;
					T->Period = 0.f;
					T->Size = FVector(0, 0, FMath::Max(0.f, R.H - 420.f));
				});
				Make(TEXT("hammer"), P(Pz->Values.FindRef(TEXT("hammer"))), Yaw, i, [&](AWRFoldThing* T) { T->Link = Ctl; });
				if (const TArray<TSharedPtr<FJsonValue>>* Bl = PArr(Pz, TEXT("bellows")))
					for (int32 b = 0; b < Bl->Num() && b < 3; ++b) Make(TEXT("bellows"), P((*Bl)[b]), Yaw + 90.f, i, [&](AWRFoldThing* T) { T->Sub = uint8(b); T->Link = Ctl; });
			}
			else if (K == TEXT("braziers"))
			{
				const TArray<TSharedPtr<FJsonValue>>* At = PArr(Pz, TEXT("at"));
				AWRFoldThing* Ctl = nullptr;
				for (int32 b = 0; At && b < At->Num(); ++b)
				{
					AWRFoldThing* T = Make(TEXT("brazier"), P((*At)[b]), Yaw + 90.f, i, [&](AWRFoldThing* Th) { Th->Sub = uint8(b); Th->Size = FVector(At->Num(), 0, 0); Th->Link = Ctl; });
					if (!Ctl) Ctl = T;
				}
				if (const TSharedPtr<FJsonValue> Fl = Pz->Values.FindRef(TEXT("flame"))) Make(TEXT("flame"), P(Fl), Yaw, i, None);
			}
			else if (K == TEXT("aurora"))
			{
				TArray<uint8> Bands = { 0, 1, 2 };
				for (int32 b = 2; b > 0; --b) Bands.Swap(b, FMath::RandHelper(b + 1));
				AWRFoldThing* Ctl = Make(TEXT("skylight"), P(Pz->Values.FindRef(TEXT("skylight"))), Yaw, i, [&](AWRFoldThing* T) { T->Seq = Bands; T->Size = FVector(0, 0, R.H); T->Goal = Lintel; });
				if (const TArray<TSharedPtr<FJsonValue>>* Pr = PArr(Pz, TEXT("prisms")))
				{
					for (int32 p = 0; p < Pr->Num() && p < 3; ++p)
					{
						const FVector Socket = Lintel + RF.GetRotation().RotateVector(FVector(0, -150.f + 150.f * p, 0));
						// Each starts on a wrong colour.
						Make(TEXT("prism"), P((*Pr)[p]), Yaw, i, [&](AWRFoldThing* T) { T->Sub = uint8(p); T->Link = Ctl; T->Goal = Socket; T->State = (Bands[p] + 1 + FMath::RandRange(0, 1)) % 3; });
					}
				}
			}
			(void)ExitDoor;
		}
	}
}
