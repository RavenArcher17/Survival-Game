// Artifacts (cooks/build-portals-artifacts.md 2.12-2.14; cooks/endgame.md 2.6): the component on every PlayerState
// (hooks, pouch, geis records, the band), the shared numbers, and the thirty weird rules. A rule is a query
// (WRArtifacts::Has) at the one place in the game it bends: the roll, the guard, the parry window, the drink, the regen,
// the light, the HUD, the boss's move list, a tame's will to fight, the fall; the few that act on their own (lightning
// that finds you, the fall from a height, circling sunwise, the Hag's cup) run in the component's tick.

#include "WRArtifacts.h"
#include "WRPortals.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace WRArtImpl
{
	TSharedPtr<FJsonObject>& ArtCached() { static TSharedPtr<FJsonObject> D; return D; }
	float ANum(const TSharedPtr<FJsonObject>& O, const FString& Key, float Fallback) { double V = 0.0; return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback; }
	FString AStr(const TSharedPtr<FJsonObject>& O, const FString& Key) { FString S; if (O.IsValid()) O->TryGetStringField(Key, S); return S; }
	TSharedPtr<FJsonObject> AChild(const TSharedPtr<FJsonObject>& O, const FString& Key) { const TSharedPtr<FJsonObject>* C = nullptr; return O.IsValid() && O->TryGetObjectField(Key, C) ? *C : nullptr; }
	const TArray<TSharedPtr<FJsonValue>>* AArr(const TSharedPtr<FJsonObject>& O, const FString& Key) { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; return O.IsValid() && O->TryGetArrayField(Key, A) ? A : nullptr; }

	AWRPlayerState* ArtStateOf(const AActor* Who)
	{
		if (const AWRPlayerState* PS = Cast<AWRPlayerState>(Who)) return const_cast<AWRPlayerState*>(PS);
		if (const APawn* P = Cast<APawn>(Who)) return Cast<AWRPlayerState>(P->GetPlayerState());
		if (const AController* C = Cast<AController>(Who)) return Cast<AWRPlayerState>(C->PlayerState);
		return nullptr;
	}
	AWRPlayerState* ArtStateNamed(UWorld* World, const FString& Name)
	{
		if (!World || !World->GetGameState()) return nullptr;
		for (APlayerState* P : World->GetGameState()->PlayerArray) if (P && P->GetPlayerName() == Name) return Cast<AWRPlayerState>(P);
		return nullptr;
	}
	/** The server's clock on any machine (PausedUntil and HookingUntil are on it). */
	float ArtNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : W ? float(W->GetTimeSeconds()) : 0.f;
	}

	/** Bands live on the server: band id -> members; when a member's been gone bandIdleMinutes they drop out. */
	struct FArtBands
	{
		TArray<TArray<FString>> List;
		TMap<FString, double> GoneSince;
		int32 Find(const FString& N) const { for (int32 i = 0; i < List.Num(); ++i) if (List[i].Contains(N)) return i; return INDEX_NONE; }
	};
	FArtBands& ArtBands() { static FArtBands B; return B; }

	void ArtSyncBands(UWorld* World)
	{
		FArtBands& B = ArtBands();
		B.List.RemoveAll([](const TArray<FString>& L) { return L.Num() < 2; });
		if (!World || !World->GetGameState()) return;
		for (APlayerState* P : World->GetGameState()->PlayerArray)
		{
			UWRArtifactComponent* A = UWRArtifactComponent::For(P);
			if (!A) continue;
			const int32 i = B.Find(P->GetPlayerName());
			TArray<FString> Others = i != INDEX_NONE ? B.List[i] : TArray<FString>();
			Others.Remove(P->GetPlayerName());
			if (Others != A->Band) A->Band = Others;
		}
	}

	/** Something hunts this player close by, or they took or dealt a blow in a fight lately. */
	bool ArtInFight(const AActor* Who)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		if (!C) return false;
		if (C->HasTag(WRTags::State_CombatTagged)) return true;
		for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && !It->IsTamed() && It->GetAggroTarget() == Who && FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) < 4000.f) return true;
		}
		return false;
	}
	AWRBoss* ArtEngagedBossNear(const AActor* Who, float Range)
	{
		if (!Who) return nullptr;
		for (TActorIterator<AWRBoss> It(Who->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && It->bEngaged && FVector::Dist2D(It->GetHome(), Who->GetActorLocation()) < Range) return *It;
		}
		return nullptr;
	}
}
using namespace WRArtImpl;

// ================================================================================================ data

TSharedPtr<FJsonObject> WRArtifacts::Data()
{
	TSharedPtr<FJsonObject>& D = ArtCached();
	if (!D.IsValid()) D = FWRTuning::LoadDataFile(TEXT("artifacts.json"));
	if (!D.IsValid()) D = MakeShared<FJsonObject>();
	return D;
}

TSharedPtr<FJsonObject> WRArtifacts::Def(FName Id)
{
	if (const TArray<TSharedPtr<FJsonValue>>* L = AArr(Data(), TEXT("artifacts")))
		for (const TSharedPtr<FJsonValue>& V : *L) if (AStr(V->AsObject(), TEXT("id")) == Id.ToString()) return V->AsObject();
	return nullptr;
}

float WRArtifacts::Num(const FString& Path, float Fallback)
{
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));
	TSharedPtr<FJsonObject> O = Data();
	for (int32 i = 0; i + 1 < Parts.Num(); ++i) { O = AChild(O, Parts[i]); if (!O.IsValid()) return Fallback; }
	return Parts.Num() ? ANum(O, Parts.Last(), Fallback) : Fallback;
}

FString WRArtifacts::Name(FName Id) { const FString N = AStr(Def(Id), TEXT("name")); return N.IsEmpty() ? Id.ToString() : N; }

FString WRArtifacts::MetalName(int32 Weight)
{
	const TArray<TSharedPtr<FJsonValue>>* W = AArr(Data(), TEXT("weights"));
	return W && W->IsValidIndex(Weight - 1) ? AStr((*W)[Weight - 1]->AsObject(), TEXT("metal")) : TEXT("Bone");
}

FColor WRArtifacts::MetalColour(int32 Weight)
{
	const TArray<TSharedPtr<FJsonValue>>* W = AArr(Data(), TEXT("weights"));
	const TArray<TSharedPtr<FJsonValue>>* C = W && W->IsValidIndex(Weight - 1) ? AArr((*W)[Weight - 1]->AsObject(), TEXT("colour")) : nullptr;
	return C && C->Num() >= 3 ? FColor(uint8((*C)[0]->AsNumber()), uint8((*C)[1]->AsNumber()), uint8((*C)[2]->AsNumber())) : FColor(222, 214, 190);
}

// ================================================================================================ the component

UWRArtifactComponent::UWRArtifactComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	Hooks.SetNum(3);
}

void UWRArtifactComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRArtifactComponent, Hooks);
	DOREPLIFETIME(UWRArtifactComponent, PausedUntil);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, Pouch, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, Records, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, Titles, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, Band, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, BandInvites, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, HookingUntil, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRArtifactComponent, HookingSlot, COND_OwnerOnly);
}

UWRArtifactComponent* UWRArtifactComponent::For(const AActor* Who)
{
	const AWRPlayerState* PS = ArtStateOf(Who);
	return PS ? PS->FindComponentByClass<UWRArtifactComponent>() : nullptr;
}

int32 UWRArtifactComponent::RawAL() const
{
	// The server's artifact settings: slots (0-3) and the level cap (off / 5 / 10 / 15).
	const FString Cap = FWRSocialRules::Setting(TEXT("artifactCap"), TEXT("15")).ToLower();
	if (Cap == TEXT("off") || Cap == TEXT("0")) return 0;
	const int32 Slots = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.artifactSlots"), 3.f)), 0, 3);
	int32 Sum = 0;
	for (int32 i = 0; i < Hooks.Num() && i < Slots; ++i) if (!Hooks[i].IsEmpty()) Sum += Hooks[i].Weight;
	return FMath::Min(Sum, FMath::Clamp(FCString::Atoi(*Cap), 0, 15));
}

bool UWRArtifactComponent::IsPaused() const
{
	if (ArtNow(this) < PausedUntil) return true;
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	const APawn* P = PS ? PS->GetPawn() : nullptr;
	return P && FWRSocialRules::IsInSafeZone(P->GetWorld(), P->GetActorLocation());
}

bool UWRArtifactComponent::Wears(FName Rule) const
{
	if (Rule.IsNone()) return false;
	// Each artifact's rule, read once (this is asked on hot paths: every blow, every movement step of a wearer).
	static TMap<FName, FName> RuleOf;
	if (RuleOf.Num() == 0)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* L = AArr(WRArtifacts::Data(), TEXT("artifacts")))
			for (const TSharedPtr<FJsonValue>& V : *L) RuleOf.Add(FName(AStr(V->AsObject(), TEXT("id"))), FName(AStr(V->AsObject(), TEXT("rule"))));
	}
	const int32 Slots = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.artifactSlots"), 3.f)), 0, 3);
	for (int32 i = 0; i < Hooks.Num() && i < Slots; ++i)
	{
		if (Hooks[i].IsEmpty()) continue;
		const FName* R = RuleOf.Find(Hooks[i].Id);
		if (R && *R == Rule) return true;
	}
	return false;
}

void UWRArtifactComponent::Give(FName Id, uint8 Weight, const FString& Why)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !WRArtifacts::Def(Id).IsValid()) return;
	FWRArtifactRep A;
	A.Id = Id;
	A.Weight = uint8(FMath::Clamp<int32>(Weight, 1, 5));
	Pouch.Add(A);
	if (AWRPlayerController* PC = Cast<AWRPlayerController>(GetOwner()->GetOwner()))
	{
		PC->ClientNotify(FString::Printf(TEXT("An artifact: %s (%s, Weight %d)%s. Wear it, and the land fights harder, and pays more."), *WRArtifacts::Name(Id), *WRArtifacts::MetalName(A.Weight), A.Weight, Why.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", %s"), *Why)));
		WRSound::PlayUI(PC, TEXT("artifact.found"));
	}
	UE_LOG(LogWildreach, Log, TEXT("ARTIFACT %s gets %s W%d (%s)"), *GetOwner()->GetName(), *Id.ToString(), A.Weight, *Why);
}

FString UWRArtifactComponent::Hook(int32 PouchIndex, int32 Slot)
{
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	const APawn* Me = PS ? PS->GetPawn() : nullptr;
	const int32 Slots = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.artifactSlots"), 3.f)), 0, 3);
	if (!Me || !Pouch.IsValidIndex(PouchIndex)) return TEXT("Not now");
	if (Slot < 0 || Slot >= Slots) return Slots == 0 ? TEXT("This world allows no artifacts") : TEXT("That hook isn't open on this world");
	if (WRPortals::FoldOf(Me)) return TEXT("Not inside a fold: your charms were set as you came in");
	if (ArtInFight(Me)) return TEXT("Not in a fight");
	if (ArtEngagedBossNear(Me, WRArtifacts::Num(TEXT("hookBossRange"), 9000.f))) return TEXT("Not this close to a boss fight");
	if (HookingUntil > 0.f) return TEXT("You're already tying one on");
	HookingUntil = ArtNow(this) + WRArtifacts::Num(TEXT("hookSeconds"), 2.f);
	HookingSlot = int8(Slot);
	HookingFrom = PouchIndex;
	HookingAt = Me->GetActorLocation();
	return FString::Printf(TEXT("Tying on %s..."), *WRArtifacts::Name(Pouch[PouchIndex].Id));
}

FString UWRArtifactComponent::Unhook(int32 Slot)
{
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	const APawn* Me = PS ? PS->GetPawn() : nullptr;
	if (!Me || !Hooks.IsValidIndex(Slot) || Hooks[Slot].IsEmpty()) return TEXT("Nothing on that hook");
	if (WRPortals::FoldOf(Me)) return TEXT("Not inside a fold");
	if (ArtInFight(Me)) return TEXT("Not in a fight");
	if (ArtEngagedBossNear(Me, WRArtifacts::Num(TEXT("hookBossRange"), 9000.f))) return TEXT("Not this close to a boss fight");
	if (HookingUntil > 0.f) return TEXT("You're already at your belt");
	HookingUntil = ArtNow(this) + WRArtifacts::Num(TEXT("hookSeconds"), 2.f);
	HookingSlot = int8(Slot);
	HookingFrom = -1;
	HookingAt = Me->GetActorLocation();
	return FString::Printf(TEXT("Untying %s..."), *WRArtifacts::Name(Hooks[Slot].Id));
}

FString UWRArtifactComponent::Fuse(int32 A, int32 B)
{
	AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	if (!PS || !Pouch.IsValidIndex(A) || !Pouch.IsValidIndex(B) || A == B) return TEXT("Pick two");
	if (!WRArtifacts::AtAlchemyTable(PS->GetPawn())) return TEXT("Fuse at an alchemy table");
	const FWRArtifactRep X = Pouch[A], Y = Pouch[B];
	if (X.Id != Y.Id || X.Weight != Y.Weight) return TEXT("Two of the same artifact at the same Weight");
	if (X.Weight >= 5) return TEXT("Starsteel is the top");
	// Its land's signature resource x 5 a Weight (a raid's: both lands', x 3).
	const FString Land = AStr(WRArtifacts::Def(X.Id), TEXT("biome"));
	const FString Sig = AStr(AChild(WRPortals::Obj(TEXT("biomes")), Land), TEXT("signature"));
	const int32 Need = FMath::RoundToInt(WRArtifacts::Num(TEXT("fuse.signaturePerWeight"), 5.f)) * X.Weight;
	if (!Sig.IsEmpty() && PS->GetInventory()->CountOf(FName(*Sig)) < Need) return FString::Printf(TEXT("You need %d %s"), Need, *Sig);
	if (!Sig.IsEmpty()) PS->GetInventory()->RemoveItem(FName(*Sig), Need);
	Pouch.RemoveAt(FMath::Max(A, B));
	Pouch.RemoveAt(FMath::Min(A, B));
	FWRArtifactRep F = X;
	F.Weight = X.Weight + 1;
	Pouch.Add(F);
	WRSound::PlayAt(PS, TEXT("artifact.fuse"), PS->GetPawn() ? PS->GetPawn()->GetActorLocation() : FVector::ZeroVector);
	return FString::Printf(TEXT("Fused: %s, now %s"), *WRArtifacts::Name(F.Id), *WRArtifacts::MetalName(F.Weight));
}

FString UWRArtifactComponent::Salvage(int32 Index)
{
	AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	if (!PS || !Pouch.IsValidIndex(Index)) return TEXT("Pick one");
	if (!WRArtifacts::AtAlchemyTable(PS->GetPawn())) return TEXT("Salvage at an alchemy table");
	const FWRArtifactRep X = Pouch[Index];
	const FString Land = AStr(WRArtifacts::Def(X.Id), TEXT("biome"));
	FString Essence = AStr(AChild(WRPortals::Obj(TEXT("biomes")), Land), TEXT("essence"));
	if (Essence.IsEmpty()) Essence = TEXT("ember_essence");
	const int32 N = FMath::RoundToInt(WRArtifacts::Num(TEXT("fuse.salvageEssencePerWeight"), 2.f)) * X.Weight;
	Pouch.RemoveAt(Index);
	PS->GetInventory()->AddItem(FName(*Essence), N);
	return FString::Printf(TEXT("Salvaged %s: %s x%d"), *WRArtifacts::Name(X.Id), *Essence, N);
}

void UWRArtifactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	APawn* Me = PS ? PS->GetPawn() : nullptr;
	// Tying a charm on or off: 2 s standing (a step away, a blow or a fight stops it).
	if (HookingUntil > 0.f)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Me);
		if (!C || !C->IsAlive() || FVector::Dist2D(C->GetActorLocation(), HookingAt) > 80.f || C->HasTag(WRTags::Status_Staggered))
		{
			HookingUntil = 0.f;
			HookingSlot = -1;
		}
		else if (ArtNow(this) >= HookingUntil)
		{
			if (Hooks.Num() < 3) Hooks.SetNum(3);
			if (HookingFrom >= 0 && Pouch.IsValidIndex(HookingFrom) && Hooks.IsValidIndex(HookingSlot))
			{
				const FWRArtifactRep Was = Hooks[HookingSlot];
				Hooks[HookingSlot] = Pouch[HookingFrom];
				Pouch.RemoveAt(HookingFrom);
				if (!Was.IsEmpty()) Pouch.Add(Was);
			}
			else if (HookingFrom < 0 && Hooks.IsValidIndex(HookingSlot) && !Hooks[HookingSlot].IsEmpty())
			{
				Pouch.Add(Hooks[HookingSlot]);
				Hooks[HookingSlot] = FWRArtifactRep();
			}
			WRSound::PlayOn(Me, TEXT("equip"));
			HookingUntil = 0.f;
			HookingSlot = -1;
		}
	}
	StepRules(DeltaTime);
}

void UWRArtifactComponent::StepRules(float Dt)
{
	AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	AWRCharacter* Me = PS ? Cast<AWRCharacter>(PS->GetPawn()) : nullptr;
	if (!Me || !Me->IsAlive())
	{
		bFalling = false;
		return;
	}
	const bool bOn = !IsPaused();
	const float T = ArtNow(this);
	// Whether a fight is on (looked at once a second: it walks the creatures).
	FightCheckClock -= Dt;
	if (FightCheckClock <= 0.f)
	{
		FightCheckClock = 1.f;
		if (ArtInFight(Me)) LastFightAt = T;
		else if (T - LastFightAt > 20.f) bHagSpent = false;   // a new fight, a new cup
	}
	// Kite String: falls hurt from 3 m and kill from 10 m (there's no other fall damage in the game).
	if (const UCharacterMovementComponent* Move = Me->GetCharacterMovement())
	{
		if (Move->IsFalling())
		{
			if (!bFalling) { bFalling = true; FallTopZ = Me->GetActorLocation().Z; }
			FallTopZ = FMath::Max(FallTopZ, Me->GetActorLocation().Z);
		}
		else if (bFalling)
		{
			bFalling = false;
			const float Drop = FallTopZ - Me->GetActorLocation().Z;
			if (bOn && Wears(TEXT("brittle_falls")))
			{
				const float From = WRArtifacts::RuleNum(Me, TEXT("brittle_falls"), TEXT("hurtFrom"), 300.f);
				const float Kill = WRArtifacts::RuleNum(Me, TEXT("brittle_falls"), TEXT("killFrom"), 1000.f);
				if (Drop > From)
				{
					FWRHit Hit;
					Hit.Damage = Drop >= Kill ? 99999.f : Me->GetAttributes()->GetMaxHealth() * (Drop - From) / FMath::Max(1.f, Kill - From);
					Hit.bUnblockable = true;
					Hit.bDamageOverTime = true;
					Hit.Origin = Me->GetActorLocation() + FVector(0, 0, 100.f);
					UWRCombatLibrary::ApplyHit(Me, Hit);
				}
			}
		}
	}
	if (!bOn) return;
	// Rod-crown: outdoors in rain or storm, lightning finds you every 40 to 60 s (a 1.0 s glow under your feet first).
	if (Wears(TEXT("lightning_finds")))
	{
		const FName Biome = WRBiome::At(Me, Me->GetActorLocation());
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(Me, Biome);
		const UWRSurvivalComponent* Surv = UWRSurvivalComponent::For(Me);
		const bool bStorm = K && (K->bStrikes || K->Id == TEXT("rain") || K->Id == TEXT("thunderstorm") || K->Id == TEXT("squall") || K->Id == TEXT("sleet"));
		if (bStorm && !(Surv && Surv->IsSheltered()))
		{
			if (NextBoltAt <= 0.0) NextBoltAt = T + FMath::FRandRange(40.f, 60.f);
			if (BoltAt <= 0.0 && T >= NextBoltAt - 1.0)
			{
				BoltAt = NextBoltAt;
				BoltSpot = Me->GetActorLocation() - FVector(0, 0, Me->GetSimpleCollisionHalfHeight());
				WRFx::Broadcast(Me, TEXT("weather.strikeWarn"), BoltSpot, FRotator::ZeroRotator, 1.f);
			}
			if (BoltAt > 0.0 && T >= BoltAt)
			{
				WRFx::Broadcast(Me, TEXT("weather.strike"), BoltSpot, FRotator::ZeroRotator, 1.f, TEXT("weather.thunder"));
				if (FVector::Dist2D(Me->GetActorLocation(), BoltSpot) < 250.f)
				{
					FWRHit Hit;
					Hit.Damage = WRArtifacts::RuleNum(Me, TEXT("lightning_finds"), TEXT("damage"), 60.f);
					Hit.bUnblockable = true;
					Hit.Element = WRTags::Element_Lightning;
					Hit.Origin = BoltSpot + FVector(0, 0, 2000.f);
					Hit.Impact = 3;
					UWRCombatLibrary::ApplyHit(Me, Hit);
					Me->Status->Stagger(WRArtifacts::RuleNum(Me, TEXT("lightning_finds"), TEXT("stun"), 0.8f));
				}
				BoltAt = 0.0;
				NextBoltAt = T + FMath::FRandRange(40.f, 60.f);
			}
		}
		else { NextBoltAt = 0.0; BoltAt = 0.0; }
	}
	// Conaire's geis: circling your target sunwise (clockwise seen from above) costs stamina.
	if (Wears(TEXT("never_sunwise")) && Me->CombatTarget.IsValid())
	{
		const FVector To = (Me->CombatTarget->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
		const FVector V = Me->GetVelocity().GetSafeNormal2D();
		// Clockwise round the target: moving to the target's right as you face it (UE: +Y is right of +X).
		if (Me->GetVelocity().Size2D() > 150.f && FVector::CrossProduct(To, V).Z > 0.5f)
		{
			if (UAbilitySystemComponent* ASC = Me->GetAbilitySystemComponent())
				ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, Me->GetAttributes()->GetStamina() - WRArtifacts::RuleNum(Me, TEXT("never_sunwise"), TEXT("cost"), 6.f) * Dt));
		}
	}
	// The Hag's Offer: the first time each fight you fall under half health, you drink, like it or not.
	if (Wears(TEXT("hags_offer")) && !bHagSpent && Me->GetAttributes()->GetHealth() < Me->GetAttributes()->GetMaxHealth() * WRArtifacts::RuleNum(Me, TEXT("hags_offer"), TEXT("below"), 0.5f))
	{
		bHagSpent = true;
		UWRInventoryComponent* Inv = PS->GetInventory();
		for (const FName Potion : { FName(TEXT("greater_healing_potion")), FName(TEXT("healing_potion")), FName(TEXT("minor_healing_potion")) })
		{
			const int32 Slot = Inv->GetSlotsMutable().IndexOfByPredicate([Potion](const FWRItemStack& S) { return S.ItemId == Potion && S.Count > 0; });
			if (Slot != INDEX_NONE) { Inv->UseSlot(Slot); break; }
		}
	}
}

TSharedRef<FJsonObject> UWRArtifactComponent::ToJson() const
{
	const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	const auto List = [](const TArray<FWRArtifactRep>& L)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		for (const FWRArtifactRep& R : L)
		{
			const TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
			X->SetStringField(TEXT("id"), R.Id.ToString());
			X->SetNumberField(TEXT("w"), R.Weight);
			A.Add(MakeShared<FJsonValueObject>(X));
		}
		return A;
	};
	O->SetArrayField(TEXT("hooks"), List(Hooks));
	O->SetArrayField(TEXT("pouch"), List(Pouch));
	TArray<TSharedPtr<FJsonValue>> Rec, Tit;
	for (const FWRGeisRecord& R : Records)
	{
		const TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("boss"), R.Boss.ToString());
		X->SetNumberField(TEXT("best"), R.Best);
		Rec.Add(MakeShared<FJsonValueObject>(X));
	}
	for (const FString& S : Titles) Tit.Add(MakeShared<FJsonValueString>(S));
	O->SetArrayField(TEXT("records"), Rec);
	O->SetArrayField(TEXT("titles"), Tit);
	O->SetNumberField(TEXT("hollow"), HollowLost);
	return O;
}

void UWRArtifactComponent::FromJson(const TSharedPtr<FJsonObject>& O)
{
	if (!O.IsValid()) return;
	const auto List = [&O](const TCHAR* Key, TArray<FWRArtifactRep>& Out, bool bFixed)
	{
		Out.Reset();
		if (const TArray<TSharedPtr<FJsonValue>>* A = AArr(O, Key))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				FWRArtifactRep R;
				const FString Id = AStr(V->AsObject(), TEXT("id"));
				R.Id = Id.IsEmpty() || Id == TEXT("None") ? NAME_None : FName(*Id);
				R.Weight = uint8(FMath::Clamp(FMath::RoundToInt(ANum(V->AsObject(), TEXT("w"), 1.f)), 1, 5));
				if (bFixed || !R.IsEmpty()) Out.Add(R);
			}
		}
		if (bFixed) Out.SetNum(3);
	};
	List(TEXT("hooks"), Hooks, true);
	List(TEXT("pouch"), Pouch, false);
	Records.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* A = AArr(O, TEXT("records")))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			FWRGeisRecord R;
			R.Boss = FName(AStr(V->AsObject(), TEXT("boss")));
			R.Best = uint8(ANum(V->AsObject(), TEXT("best"), 0.f));
			Records.Add(R);
		}
	}
	Titles.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* A = AArr(O, TEXT("titles"))) for (const TSharedPtr<FJsonValue>& V : *A) Titles.Add(V->AsString());
	HollowLost = ANum(O, TEXT("hollow"), 0.f);
}

// ================================================================================================ the shared numbers

int32 WRArtifacts::AL(const AActor* Who)
{
	const UWRArtifactComponent* A = UWRArtifactComponent::For(Who);
	return A && !A->IsPaused() ? A->RawAL() : 0;
}

bool WRArtifacts::Has(const AActor* Who, FName Rule)
{
	const UWRArtifactComponent* A = UWRArtifactComponent::For(Who);
	return A && !A->IsPaused() && A->Wears(Rule);
}

float WRArtifacts::RuleNum(const AActor* Who, FName Rule, const TCHAR* Field, float Fallback)
{
	if (const TArray<TSharedPtr<FJsonValue>>* L = AArr(Data(), TEXT("artifacts")))
		for (const TSharedPtr<FJsonValue>& V : *L) if (AStr(V->AsObject(), TEXT("rule")) == Rule.ToString()) return ANum(V->AsObject(), Field, Fallback);
	return Fallback;
}

TArray<FString> WRArtifacts::BandOf(const AWRPlayerState* PS)
{
	const UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
	return A ? A->Band : TArray<FString>();
}

TArray<FString> WRArtifacts::BandOfName(UWorld* World, const FString& Player)
{
	const int32 i = ArtBands().Find(Player);
	TArray<FString> Out = i != INDEX_NONE ? ArtBands().List[i] : TArray<FString>();
	Out.Remove(Player);
	return Out;
}

bool WRArtifacts::InParty(UWorld* World, const FString& A, const FString& B)
{
	if (A == B) return true;
	if (FWRClans::Get().SameClan(A, B)) return true;
	const int32 i = ArtBands().Find(A);
	return i != INDEX_NONE && ArtBands().List[i].Contains(B);
}

int32 WRArtifacts::PartyAL(const AActor* Who)
{
	const AWRPlayerState* Me = ArtStateOf(Who);
	const APawn* Pawn = Me ? Me->GetPawn() : nullptr;
	if (!Me || !Pawn) return AL(Who);
	UWorld* World = Pawn->GetWorld();
	const FString Name = Me->GetPlayerName();
	int32 Best = AL(Me);
	const AWRFold* Fold = WRPortals::FoldOf(Pawn);
	const AWRBoss* Boss = Fold ? nullptr : ArtEngagedBossNear(Pawn, 9000.f);
	const float Near = Num(TEXT("partyRange"), 6000.f);
	for (APlayerState* P : World->GetGameState()->PlayerArray)
	{
		const APawn* Other = P ? P->GetPawn() : nullptr;
		if (!Other || P == Me) continue;
		bool bCounts = false;
		if (Fold) bCounts = WRPortals::FoldOf(Other) == Fold;   // in a fold: everyone inside the same one
		else if (Boss) bCounts = InParty(World, Name, P->GetPlayerName()) && FVector::Dist2D(Other->GetActorLocation(), Boss->GetHome()) < Boss->GetChaseRadius();
		else bCounts = InParty(World, Name, P->GetPlayerName()) && FVector::Dist(Other->GetActorLocation(), Pawn->GetActorLocation()) < Near;
		if (bCounts) Best = FMath::Max(Best, AL(P));
	}
	return Best;
}

float WRArtifacts::DamageTakenScale(const AWRCharacterBase* Target, const AWRCharacterBase* Attacker, const FWRHit& Hit)
{
	// PvE only: players' blows (and their tames') are never touched.
	if (!Target || !Target->GetPlayerState()) return 1.f;
	if (Attacker && (Attacker->GetPlayerState() || !Attacker->GetTameOwner().IsEmpty())) return 1.f;
	float S = 1.f + Num(TEXT("perAL.damageTaken"), 0.07f) * PartyAL(Target);
	// The Feather: a blow within 2 s of your kill weighs double.
	if (Has(Target, TEXT("weighed_heart")))
	{
		const UWRArtifactComponent* A = UWRArtifactComponent::For(Target);
		if (A && ArtNow(Target) - A->LastKillAt < RuleNum(Target, TEXT("weighed_heart"), TEXT("window"), 2.f)) S *= RuleNum(Target, TEXT("weighed_heart"), TEXT("scale"), 2.f);
	}
	return S;
}

float WRArtifacts::LootScale(const AActor* Who) { return 1.f + Num(TEXT("perAL.loot"), 0.05f) * PartyAL(Who); }
float WRArtifacts::XPScale(const AActor* Who) { return 1.f + Num(TEXT("perAL.pathXP"), 0.04f) * PartyAL(Who); }
float WRArtifacts::PartChanceBonus(const AActor* Who) { return Num(TEXT("perAL.partChance"), 0.01f) * PartyAL(Who); }
bool WRArtifacts::RollScarred(const AActor* Who) { return PartyAL(Who) >= int32(Num(TEXT("scarredFromAL"), 5.f)) && FMath::FRand() < Num(TEXT("scarredChance"), 0.25f); }
bool WRArtifacts::RollQualityStep(const AActor* Who) { return FMath::FRand() < Num(TEXT("perAL.qualityStep"), 0.02f) * PartyAL(Who); }

bool WRArtifacts::PhaseThreeOpen(const AWRBoss* Boss)
{
	if (!Boss) return false;
	UWorld* World = Boss->GetWorld();
	// The ledger in parties (clan or band); the party with the most of it decides.
	TArray<TPair<TArray<FString>, float>> Parties;
	for (const TPair<FString, float>& L : Boss->Ledger)
	{
		int32 i = Parties.IndexOfByPredicate([&](const TPair<TArray<FString>, float>& P) { return P.Key.ContainsByPredicate([&](const FString& N) { return InParty(World, N, L.Key); }); });
		if (i == INDEX_NONE) i = Parties.Add({ TArray<FString>(), 0.f });
		Parties[i].Key.Add(L.Key);
		Parties[i].Value += L.Value;
	}
	const TPair<TArray<FString>, float>* Top = nullptr;
	for (const auto& P : Parties) if (!Top || P.Value > Top->Value) Top = &P;
	if (!Top) return false;
	int32 Best = 0;
	for (const FString& N : Top->Key) Best = FMath::Max(Best, AL(ArtStateNamed(World, N)));
	return Best >= int32(Num(TEXT("phase3FromAL"), 1.f));
}

// ================================================================================================ hooks

void WRArtifacts::NotePvP(AActor* A, AActor* B)
{
	const float Until = ArtNow(A) + Num(TEXT("pvpPauseSeconds"), 30.f);
	for (AActor* Who : { A, B })
	{
		if (UWRArtifactComponent* C = UWRArtifactComponent::For(Who)) C->PausedUntil = FMath::Max(C->PausedUntil, Until);
	}
}

bool WRArtifacts::BlocksHit(const AWRCharacterBase* Target, const FWRHit& Hit)
{
	// Grave Count: a wearer who died in this boss's fight can't hurt it again until the fight resets.
	const AWRBoss* Boss = Cast<AWRBoss>(Target);
	if (!Boss || !Hit.Instigator) return false;
	const AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator);
	const AActor* Player = By && By->GetPlayerState() ? By : nullptr;
	if (!Player && By && !By->GetTameOwner().IsEmpty()) return false;
	UWRArtifactComponent* A = UWRArtifactComponent::For(Player);
	if (!A) return false;
	if (!Boss->bEngaged) { A->GraveBosses.Remove(const_cast<AWRBoss*>(Boss)); return false; }
	return A->GraveBosses.Contains(const_cast<AWRBoss*>(Boss));
}

void WRArtifacts::OnHitLanded(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	if (!Target || Damage <= 0.f) return;
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	// Player against player pauses both sides' artifacts.
	if (Attacker && Attacker != Target && Target->GetPlayerState() && (Attacker->GetPlayerState() || !Attacker->GetTameOwner().IsEmpty())) NotePvP(Attacker, Target);
	// Forge-brand: your path's power burns you too (8% of what it dealt), once per blow, never a chain.
	if (Attacker && Attacker->GetPlayerState() && !Hit.PathId.IsNone() && !Hit.bReaction && !Hit.bDamageOverTime && Has(Attacker, TEXT("path_backlash")))
	{
		FWRHit Back;
		Back.Damage = Damage * RuleNum(Attacker, TEXT("path_backlash"), TEXT("share"), 0.08f);
		Back.bDamageOverTime = true;
		Back.bReaction = true;
		Back.bUnblockable = true;
		Back.Origin = Attacker->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Attacker, Back);
	}
	if (!Target->GetPlayerState()) return;
	UWRArtifactComponent* A = UWRArtifactComponent::For(Target);
	if (!A || A->IsPaused()) return;
	const bool bPvE = !Attacker || (!Attacker->GetPlayerState() && Attacker->GetTameOwner().IsEmpty());
	if (!bPvE) return;
	// Gale Feather: every heavy blow knocks you down.
	if (A->Wears(TEXT("double_knockback")) && !Hit.bDamageOverTime && Hit.Impact >= 4)
	{
		Target->Status->Stagger(RuleNum(Target, TEXT("double_knockback"), TEXT("down"), 1.2f));
	}
	// Rime Ring: struck while winded, you freeze.
	if (A->Wears(TEXT("tired_freeze")) && Target->GetAttributes()->GetStamina() < RuleNum(Target, TEXT("tired_freeze"), TEXT("below"), 30.f))
	{
		Target->Status->ApplyStatus(WRTags::Status_Frozen, RuleNum(Target, TEXT("tired_freeze"), TEXT("seconds"), 0.8f));
	}
	// Hollow Coin: each blow takes 1 from your greatest health until you rest.
	if (A->Wears(TEXT("hollowing")) && !Hit.bDamageOverTime)
	{
		const float Per = RuleNum(Target, TEXT("hollowing"), TEXT("per"), 1.f);
		UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
		const UWRAttributeSet* At = Target->GetAttributes();
		if (ASC && At && At->GetMaxHealth() > 50.f)
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), At->GetMaxHealth() - Per);
			A->HollowLost += Per;
		}
	}
}

void WRArtifacts::OnKilled(AWRCharacterBase* Victim, AActor* Killer)
{
	const AWRCharacterBase* By = Cast<AWRCharacterBase>(Killer);
	if (By && !By->GetPlayerState() && !By->GetTameOwner().IsEmpty())
	{
		// A tame's kill counts for its owner.
		if (UWorld* World = By->GetWorld()) if (AWRPlayerState* PS = ArtStateNamed(World, By->GetTameOwner())) By = Cast<AWRCharacterBase>(PS->GetPawn());
	}
	// Grave Count: dying in a boss's fight bars you from it until the fight resets.
	if (Victim && Victim->GetPlayerState() && Has(Victim, TEXT("no_runback")))
	{
		if (AWRBoss* B = ArtEngagedBossNear(Victim, 9000.f)) if (UWRArtifactComponent* A = UWRArtifactComponent::For(Victim)) A->GraveBosses.Add(B);
	}
	if (!By || !By->GetPlayerState() || !Victim || Victim->GetPlayerState()) return;
	UWRArtifactComponent* A = UWRArtifactComponent::For(By);
	if (!A || A->IsPaused()) return;
	A->LastKillAt = ArtNow(By);
	// Hive Seed: what you kill bursts, a breath later, on you alone.
	if (A->Wears(TEXT("kill_burst")))
	{
		const FVector At = Victim->GetActorLocation();
		const TWeakObjectPtr<AWRCharacterBase> WeakBy = const_cast<AWRCharacterBase*>(By);
		WRFx::Broadcast(Victim, TEXT("status.poison"), At, FRotator::ZeroRotator, 1.4f, TEXT("artifact.swell"));
		FTimerHandle H;
		By->GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakBy, At]()
		{
			AWRCharacterBase* P = WeakBy.Get();
			if (!P || !P->IsAlive()) return;
			WRFx::Broadcast(P, TEXT("burst"), At, FRotator::ZeroRotator, 1.2f, TEXT("artifact.burst"));
			if (FVector::Dist(P->GetActorLocation(), At) < RuleNum(P, TEXT("kill_burst"), TEXT("radius"), 300.f))
			{
				FWRHit Hit;
				Hit.Damage = 40.f;
				Hit.Buildups.Add(TEXT("poison"), 30.f);
				Hit.bUnblockable = true;
				Hit.Origin = At;
				UWRCombatLibrary::ApplyHit(P, Hit);
			}
		}), RuleNum(By, TEXT("kill_burst"), TEXT("delay"), 1.f), false);
	}
}

namespace WRArtImpl
{
	/** Weights by a table [I..V] percentages. */
	uint8 ArtRollWeight(const TArray<TSharedPtr<FJsonValue>>* W)
	{
		if (!W || W->Num() < 5) return 1;
		float Total = 0.f;
		for (const auto& V : *W) Total += V->AsNumber();
		float R = FMath::FRand() * Total;
		for (int32 i = 0; i < 5; ++i) { R -= (*W)[i]->AsNumber(); if (R <= 0.f) return uint8(i + 1); }
		return 1;
	}
	FName ArtifactOf(const FString& Source)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* L = AArr(WRArtifacts::Data(), TEXT("artifacts")))
			for (const TSharedPtr<FJsonValue>& V : *L) if (AStr(V->AsObject(), TEXT("source")) == Source) return FName(AStr(V->AsObject(), TEXT("id")));
		return NAME_None;
	}
}

void WRArtifacts::OnBossKilled(AWRBoss* Boss)
{
	if (!Boss || !Boss->IsMajor()) return;
	UWorld* World = Boss->GetWorld();
	const FName Art = ArtifactOf(Boss->CreatureId.ToString());
	const float Need = Boss->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	const bool bPhase3 = Boss->Phase >= 3;
	for (const TPair<FString, float>& L : Boss->Ledger)
	{
		if (L.Value < Need) continue;
		AWRPlayerState* PS = ArtStateNamed(World, L.Key);
		UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
		if (!PS || !A) continue;
		const int32 Party = PartyAL(PS);
		// The first artifact: the first kill of your 4th distinct major (BossKills already counts this one).
		int32 Majors = 0;
		for (const FName& B : PS->BossKills) { const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(B); Majors += D && D->Role.Contains(TEXT("major")); }
		const bool bFirstOfFour = Majors == int32(Num(TEXT("drops.firstArtifactAtMajors"), 4.f)) && A->Pouch.Num() == 0 && A->Hooks.ContainsByPredicate([](const FWRArtifactRep& R) { return R.IsEmpty(); }) && !A->Hooks.ContainsByPredicate([](const FWRArtifactRep& R) { return !R.IsEmpty(); });
		if (!Art.IsNone())
		{
			if (bFirstOfFour) A->Give(Art, 1, TEXT("your fourth great kill"));
			else
			{
				float Chance = Num(TEXT("drops.majorBase"), 0.08f) + Num(TEXT("perAL.dropChance"), 0.01f) * Party;
				if (bPhase3) Chance *= Num(TEXT("drops.phase3Scale"), 1.5f);
				if (FMath::FRand() < Chance)
				{
					const TArray<TSharedPtr<FJsonValue>>* Tables = AArr(AChild(Data(), TEXT("drops")), TEXT("majorWeights"));
					const TArray<TSharedPtr<FJsonValue>>* W = nullptr;
					for (int32 t = 0; Tables && t < Tables->Num(); ++t) if (Party >= ANum((*Tables)[t]->AsObject(), TEXT("fromAL"), 0.f)) W = AArr((*Tables)[t]->AsObject(), TEXT("weights"));
					A->Give(Art, ArtRollWeight(W), Boss->GetDisplayLabel());
				}
			}
		}
		// Geis records: a new highest AL on this boss pays once.
		const int32 MyAL = Party;
		FWRGeisRecord* R = A->Records.FindByPredicate([&](const FWRGeisRecord& X) { return X.Boss == Boss->CreatureId; });
		if (!R) { A->Records.Add({ Boss->CreatureId, 0 }); R = &A->Records.Last(); }
		if (MyAL > R->Best)
		{
			const int32 Was = R->Best;
			R->Best = uint8(MyAL);
			TArray<FString> Said;
			if (UWRPathComponent* Paths = PS->GetPaths(); Paths && !Paths->GetMainPath().IsNone())
			{
				const int32 XP = FMath::RoundToInt(Num(TEXT("records.xpPerAL"), 300.f)) * MyAL;
				Paths->AwardXP(Paths->GetMainPath(), XP);
				Said.Add(FString::Printf(TEXT("+%d path XP"), XP));
			}
			if (Was < int32(Num(TEXT("records.scarredAt"), 5.f)) && MyAL >= int32(Num(TEXT("records.scarredAt"), 5.f)))
			{
				const FName Part = Boss->CreatureId == TEXT("vulkarn") ? FName(TEXT("part_giants_ember_scarred")) : FName(TEXT("part_wyrm_scale_scarred"));
				PS->GetInventory()->AddItem(Part, 1);
				Said.Add(TEXT("a Scarred part"));
			}
			if (MyAL >= int32(Num(TEXT("records.baneAt"), 15.f)))
			{
				const FWRCreatureDef* D = FWRCreatureRegistry::Get().Find(Boss->CreatureId);
				FString Short = D ? D->Name : Boss->CreatureId.ToString();
				Short.Split(TEXT(","), &Short, nullptr);
				A->Titles.AddUnique(FString::Printf(TEXT("%s's Bane"), *Short));
				Said.Add(FString::Printf(TEXT("the title %s's Bane"), *Short));
			}
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(FString::Printf(TEXT("A new geis record on %s at AL %d: %s"), *Boss->GetDisplayLabel(), MyAL, *FString::Join(Said, TEXT(", "))));
		}
	}
}

FString WRArtifacts::PortalDrop(AWRPlayerState* PS, FName Biome, int32 Rank, float BaseChance)
{
	UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
	if (!A || BaseChance <= 0.f) return FString();
	const float Chance = BaseChance + Num(TEXT("perAL.dropChance"), 0.01f) * PartyAL(PS);
	if (FMath::FRand() >= Chance) return FString();
	const FName Art(AStr(AChild(WRPortals::Obj(TEXT("biomes")), Biome.ToString()), TEXT("artifact")));
	if (Art.IsNone() || !Def(Art).IsValid()) return FString();
	uint8 W = ArtRollWeight(AArr(AChild(AChild(Data(), TEXT("drops")), TEXT("portalWeights")), FString::FromInt(Rank)));
	// AL 10+: artifacts come at III to V.
	if (PartyAL(PS) >= int32(Num(TEXT("highWeightsFromAL"), 10.f))) W = FMath::Max<uint8>(W, 3);
	A->Give(Art, W, TEXT("from the fold"));
	return FString::Printf(TEXT("an artifact: %s (%s)"), *Name(Art), *MetalName(W));
}

float WRArtifacts::KnockScale(const AActor* Victim) { return Has(Victim, TEXT("double_knockback")) ? RuleNum(Victim, TEXT("double_knockback"), TEXT("scale"), 2.f) : 1.f; }

float WRArtifacts::HealFilter(const AWRCharacterBase* Who, float Heal, bool bPotion)
{
	if (!Who || Heal <= 0.f) return Heal;
	if (WRPortals::HealBlocked(Who)) return 0.f;
	if (bPotion && Has(Who, TEXT("half_potions"))) Heal *= RuleNum(Who, TEXT("half_potions"), TEXT("scale"), 0.5f);
	if (Has(Who, TEXT("heal_cap")) && Who->GetAttributes())
	{
		const float Cap = Who->GetAttributes()->GetMaxHealth() * RuleNum(Who, TEXT("heal_cap"), TEXT("cap"), 0.7f);
		Heal = FMath::Clamp(Cap - Who->GetAttributes()->GetHealth(), 0.f, Heal);
	}
	return Heal;
}

void WRArtifacts::OnAbilityUsed(AWRCharacterBase* Who, bool bSpell)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	// Blood-price: every Wildbrand and spell also costs 5 health.
	if (Has(Who, TEXT("blood_price")))
	{
		FWRHit Hit;
		Hit.Damage = RuleNum(Who, TEXT("blood_price"), TEXT("cost"), 5.f);
		Hit.bDamageOverTime = true;
		Hit.bReaction = true;
		Hit.bUnblockable = true;
		Hit.Origin = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
	// Wardcarver's Chime: every spell rings out, and what's within 40 m comes.
	if (bSpell && Has(Who, TEXT("loud_spells")))
	{
		const float R = RuleNum(Who, TEXT("loud_spells"), TEXT("radius"), 4000.f);
		WRSound::PlayAt(Who, TEXT("world.chime"), Who->GetActorLocation(), 2.f);
		for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && !It->IsTamed() && !It->GetAggroTarget() && FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) < R) It->SetAggroTarget(Who);
		}
	}
}

float WRArtifacts::LightScale(const AActor* Who) { return (Has(Who, TEXT("half_light")) ? RuleNum(Who, TEXT("half_light"), TEXT("scale"), 0.5f) : 1.f) * WRPortals::LightScale(Who); }

float WRArtifacts::StaminaRegenScale(const AWRCharacterBase* Who)
{
	if (!Who || !Who->GetPlayerState()) return 1.f;
	float S = 1.f;
	if (Has(Who, TEXT("slow_regen"))) S *= RuleNum(Who, TEXT("slow_regen"), TEXT("scale"), 0.7f);
	if (Has(Who, TEXT("moving_regen")) && Who->GetVelocity().Size2D() < 60.f) S = 0.f;
	return S;
}

float WRArtifacts::AggroScale(const AActor* Who) { return Has(Who, TEXT("every_challenge")) ? 10.f : 1.f; }

bool WRArtifacts::AtAlchemyTable(const AActor* Who)
{
	if (!Who) return false;
	for (TActorIterator<AWRStation> It(Who->GetWorld()); It; ++It)
	{
		if (It->StationType == TEXT("alchemy_table") && FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) < 500.f) return true;
	}
	return false;
}

// ================================================================================================ commands

FString WRArtifacts::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
	if (!PS || !A || Args.Num() < 2) return FString();
	UWorld* World = PC->GetWorld();
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString Me = PS->GetPlayerName();
	if (Args[0] == TEXT("art"))
	{
		const FString V = Args[1];
		if (V == TEXT("hook")) return A->Hook(FCString::Atoi(*Arg(2)), FCString::Atoi(*Arg(3)));
		if (V == TEXT("unhook")) return A->Unhook(FCString::Atoi(*Arg(2)));
		if (V == TEXT("fuse")) return A->Fuse(FCString::Atoi(*Arg(2)), FCString::Atoi(*Arg(3)));
		if (V == TEXT("salvage")) return A->Salvage(FCString::Atoi(*Arg(2)));
		if (V == TEXT("dev") && Arg(2) == TEXT("give"))
		{
			A->Give(FName(*Arg(3)), uint8(FMath::Clamp(FCString::Atoi(*Arg(4)), 1, 5)), TEXT("dev"));
			return TEXT("Given");
		}
		return FString();
	}
	if (Args[0] == TEXT("band"))
	{
		FArtBands& B = ArtBands();
		const FString V = Args[1];
		const FString Who = Arg(2);
		if (V == TEXT("invite"))
		{
			AWRPlayerState* Other = ArtStateNamed(World, Who);
			UWRArtifactComponent* OA = UWRArtifactComponent::For(Other);
			if (!Other || !OA || Who == Me) return TEXT("They aren't here");
			if (InParty(World, Me, Who) && !FWRClans::Get().SameClan(Me, Who)) return TEXT("Already in your band");
			OA->BandInvites.AddUnique(Me);
			if (AWRPlayerController* OPC = Cast<AWRPlayerController>(Other->GetOwner())) OPC->ClientNotify(FString::Printf(TEXT("%s asks you into their band (Clan tab)"), *Me));
			return FString::Printf(TEXT("Asked %s into your band"), *Who);
		}
		if (V == TEXT("accept"))
		{
			if (!A->BandInvites.Contains(Who)) return TEXT("No such invite");
			A->BandInvites.Remove(Who);
			// Leave any band you're in, then join theirs (or start one with them).
			const int32 Mine = B.Find(Me);
			if (Mine != INDEX_NONE) B.List[Mine].Remove(Me);
			const int32 Theirs = B.Find(Who);
			if (Theirs != INDEX_NONE) B.List[Theirs].AddUnique(Me);
			else B.List.Add({ Who, Me });
			ArtSyncBands(World);
			for (const FString& N : BandOfName(World, Me)) if (AWRPlayerState* O = ArtStateNamed(World, N)) if (AWRPlayerController* OPC = Cast<AWRPlayerController>(O->GetOwner())) OPC->ClientNotify(FString::Printf(TEXT("%s joins your band"), *Me));
			return FString::Printf(TEXT("You join %s's band"), *Who);
		}
		if (V == TEXT("decline")) { A->BandInvites.Remove(Who); return TEXT("Declined"); }
		if (V == TEXT("leave"))
		{
			const int32 Mine = B.Find(Me);
			if (Mine == INDEX_NONE) return TEXT("You're not in a band");
			B.List[Mine].Remove(Me);
			ArtSyncBands(World);
			A->Band.Reset();
			return TEXT("You leave the band");
		}
	}
	return FString();
}

// The bands' upkeep: whoever's been gone bandIdleMinutes leaves; everyone's list follows (the server's game mode ticks it
// through the portal system's second, see WRPortals.cpp StepInvites).
namespace WRArtImpl
{
	void TickBands(UWorld* World)
	{
		FArtBands& B = ArtBands();
		const double ArtNow = FPlatformTime::Seconds();
		TSet<FString> Online;
		if (World && World->GetGameState()) for (APlayerState* P : World->GetGameState()->PlayerArray) if (P) Online.Add(P->GetPlayerName());
		for (TArray<FString>& L : B.List)
		{
			for (int32 i = L.Num() - 1; i >= 0; --i)
			{
				if (Online.Contains(L[i])) { B.GoneSince.Remove(L[i]); continue; }
				double& Since = B.GoneSince.FindOrAdd(L[i], ArtNow);
				if (ArtNow - Since > WRArtifacts::Num(TEXT("bandIdleMinutes"), 10.f) * 60.0) L.RemoveAt(i);
			}
		}
		ArtSyncBands(World);
	}
}
void WRArtifacts::TickBands(UWorld* World) { WRArtImpl::TickBands(World); }

void UWRArtifactComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority()) RestHandle = UWRSurvivalComponent::OnRest.AddUObject(this, &UWRArtifactComponent::OnRested);
}

void UWRArtifactComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UWRSurvivalComponent::OnRest.Remove(RestHandle);
	Super::EndPlay(Reason);
}

void UWRArtifactComponent::OnRested(AWRPlayerState* PS)
{
	// Hollow Coin: what it took from your greatest health comes back when you sleep.
	if (PS != GetOwner() || HollowLost <= 0.f) return;
	if (AWRCharacterBase* C = Cast<AWRCharacterBase>(PS->GetPawn()); C && C->GetAbilitySystemComponent() && C->GetAttributes())
	{
		C->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), C->GetAttributes()->GetMaxHealth() + HollowLost);
	}
	HollowLost = 0.f;
}
