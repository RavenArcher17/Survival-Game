// Deterministic combat-rule checks in a real (headless) game world: two creatures, hits applied
// directly through the damage pipeline, results checked exactly. Complements the bot soak test
// (Tools/mptest.ps1), which proves the same systems run over the network but isn't deterministic.

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WRAttributeSet.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRGameplayTags.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FWRTestArena
	{
		UWorld* World = nullptr;
		AWRCreature* Attacker = nullptr;
		AWRCreature* Target = nullptr;

		FWRTestArena()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WRCombatTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();

			// Attacker faces +X toward the target 150 cm away; target faces back at it.
			Attacker = AWRCreature::SpawnCreature(World, TEXT("cinderfang"), FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator);
			Target = AWRCreature::SpawnCreature(World, TEXT("rimeclaw"), FVector(150.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f));

			// A bare test world has no game mode, so BeginPlay isn't dispatched for us.
			for (AActor* Actor : { static_cast<AActor*>(Attacker), static_cast<AActor*>(Target) })
			{
				if (Actor && !Actor->HasActorBegunPlay())
				{
					Actor->DispatchBeginPlay();
				}
			}
		}

		~FWRTestArena()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		FWRHit Melee(float Damage, bool bHeavy = false, FGameplayTag Element = WRTags::Element_Physical) const
		{
			FWRHit Hit;
			Hit.Instigator = Attacker;
			Hit.Damage = Damage;
			Hit.PoiseDamage = 0.f;
			Hit.Element = Element;
			Hit.bMelee = true;
			Hit.bHeavy = bHeavy;
			Hit.Origin = Attacker->GetActorLocation();
			return Hit;
		}

		float Health() const { return Target->GetAttributes()->GetHealth(); }
		void Heal() const { Target->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), Target->GetAttributes()->GetMaxHealth()); }
		void Tag(AWRCreature* Who, const FGameplayTag& T, bool bOn) const { Who->GetAbilitySystemComponent()->SetLooseGameplayTagCount(T, bOn ? 1 : 0); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRCombatScenarioTest, "Wildreach.Combat.Scenarios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRCombatScenarioTest::RunTest(const FString& Parameters)
{
	FWRTestArena A;
	if (!TestNotNull(TEXT("attacker spawned"), A.Attacker) || !TestNotNull(TEXT("target spawned"), A.Target))
	{
		return false;
	}
	TestTrue(TEXT("target alive with midland health"), A.Target->IsAlive() && FMath::IsNearlyEqual(A.Health(), WRTune(TEXT("enemies.midland.health"), 120.f)));

	// 1. Plain hit.
	TestEqual(TEXT("plain hit result"), UWRCombatLibrary::ApplyHit(A.Target, A.Melee(10.f)), EWRHitResult::Hit);
	TestEqual(TEXT("plain hit damage"), A.Health(), 110.f);
	A.Heal();

	// 2. Block from the front: no damage, stamina spent.
	A.Tag(A.Target, WRTags::State_Blocking, true);
	const float StaminaBefore = A.Target->GetAttributes()->GetStamina();
	TestEqual(TEXT("block result"), UWRCombatLibrary::ApplyHit(A.Target, A.Melee(20.f)), EWRHitResult::Blocked);
	TestEqual(TEXT("block takes no damage"), A.Health(), 120.f);
	TestTrue(TEXT("block costs stamina"), A.Target->GetAttributes()->GetStamina() < StaminaBefore);

	// 3. Block from behind doesn't count.
	FWRHit FromBehind = A.Melee(20.f);
	FromBehind.Origin = FVector(400.f, 0.f, 0.f);
	TestEqual(TEXT("block from behind is a hit"), UWRCombatLibrary::ApplyHit(A.Target, FromBehind), EWRHitResult::Hit);
	A.Heal();

	// 4. Parry staggers the attacker.
	A.Tag(A.Target, WRTags::State_Parrying, true);
	TestEqual(TEXT("parry result"), UWRCombatLibrary::ApplyHit(A.Target, A.Melee(20.f)), EWRHitResult::Parried);
	TestTrue(TEXT("parried attacker staggered"), A.Attacker->HasTag(WRTags::Status_Staggered));
	A.Attacker->Status->ClearStatus(WRTags::Status_Staggered);
	A.Tag(A.Target, WRTags::State_Parrying, false);
	A.Tag(A.Target, WRTags::State_Blocking, false);

	// 5. Dodge i-frames.
	A.Tag(A.Target, WRTags::State_Invulnerable, true);
	TestEqual(TEXT("i-frames dodge"), UWRCombatLibrary::ApplyHit(A.Target, A.Melee(20.f)), EWRHitResult::Dodged);
	TestEqual(TEXT("dodge takes no damage"), A.Health(), 120.f);
	A.Tag(A.Target, WRTags::State_Invulnerable, false);

	// 6. Poise break -> stagger -> riposte for x3.
	FWRHit PoiseBreaker = A.Melee(1.f);
	PoiseBreaker.PoiseDamage = 1000.f;
	UWRCombatLibrary::ApplyHit(A.Target, PoiseBreaker);
	TestTrue(TEXT("poise break staggers"), A.Target->HasTag(WRTags::Status_Staggered));
	A.Heal();
	UWRCombatLibrary::ApplyHit(A.Target, A.Melee(10.f));
	TestEqual(TEXT("riposte triples damage"), A.Health(), 120.f - 10.f * WRTune(TEXT("weapon.riposteMultiplier"), 3.f));
	TestFalse(TEXT("riposte consumes the stagger"), A.Target->HasTag(WRTags::Status_Staggered));
	A.Heal();

	// 7. Frost buildup: first full meter = frostbite + slowed, second = frozen.
	FWRHit Frost = A.Melee(0.f, false, WRTags::Element_Frost);
	Frost.bMelee = false;
	Frost.FrostBuildup = 100.f;
	UWRCombatLibrary::ApplyHit(A.Target, Frost);
	TestTrue(TEXT("frostbite"), A.Target->HasTag(WRTags::Status_Frostbite));
	TestTrue(TEXT("slowed"), A.Target->HasTag(WRTags::Status_Slowed));
	UWRCombatLibrary::ApplyHit(A.Target, Frost);
	TestTrue(TEXT("frozen when frostbitten again"), A.Target->HasTag(WRTags::Status_Frozen));

	// 8. Thaw: fire on a frozen target with the 25/25 combo adds the thaw burst and ends Frozen.
	A.Heal();
	A.Tag(A.Attacker, WRTags::Combo_Thaw, true);
	FWRHit Fire = A.Melee(10.f, false, WRTags::Element_Fire);
	Fire.bMelee = false;
	UWRCombatLibrary::ApplyHit(A.Target, Fire);
	const float FrostbiteMult = 1.f + WRTune(TEXT("status.frostbiteDamageTakenBonus"), 0.15f);
	TestTrue(TEXT("thaw adds its burst"), FMath::IsNearlyEqual(A.Health(), 120.f - (10.f * FrostbiteMult + WRTune(TEXT("status.thawDamage"), 40.f)), 0.01f));
	TestFalse(TEXT("thaw ends frozen"), A.Target->HasTag(WRTags::Status_Frozen));
	A.Tag(A.Attacker, WRTags::Combo_Thaw, false);

	// 9. Shatter: a heavy hit on a frozen target spends all the attacker's Chill for a bigger burst.
	A.Heal();
	A.Target->Status->ApplyStatus(WRTags::Status_Frozen, 2.5f);
	A.Attacker->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), A.Attacker->GetAttributes()->GetMaxChill());
	UWRCombatLibrary::ApplyHit(A.Target, A.Melee(10.f, true));
	const float Shatter = WRTune(TEXT("status.shatterDamage"), 30.f) * (1.f + WRTune(TEXT("resources.chillShatterBonusAtFull"), 1.f));
	TestTrue(TEXT("full-chill shatter"), FMath::IsNearlyEqual(A.Health(), 120.f - (10.f * FrostbiteMult + Shatter), 0.01f));
	TestEqual(TEXT("shatter spends all chill"), A.Attacker->GetAttributes()->GetChill(), 0.f);
	TestFalse(TEXT("shatter ends frozen"), A.Target->HasTag(WRTags::Status_Frozen));

	// 10. Balance: gaining Heat also gives half as much Chill.
	A.Tag(A.Attacker, WRTags::Combo_Balance, true);
	A.Attacker->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHeatAttribute(), 0.f);
	A.Attacker->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetChillAttribute(), 0.f);
	UWRCombatLibrary::GainResource(A.Attacker, WRTags::Element_Fire, 20.f);
	TestEqual(TEXT("balance heat"), A.Attacker->GetAttributes()->GetHeat(), 20.f);
	TestEqual(TEXT("balance chill share"), A.Attacker->GetAttributes()->GetChill(), 10.f);

	// 11. Heat stops at full and never hurts you (design review #2).
	const float AttackerHealth = A.Attacker->GetAttributes()->GetHealth();
	UWRCombatLibrary::GainResource(A.Attacker, WRTags::Element_Fire, 10000.f);
	TestEqual(TEXT("heat capped at max"), A.Attacker->GetAttributes()->GetHeat(), A.Attacker->GetAttributes()->GetMaxHeat());
	TestEqual(TEXT("full heat costs no health"), A.Attacker->GetAttributes()->GetHealth(), AttackerHealth);

	// 12. Death at zero health.
	UWRCombatLibrary::ApplyHit(A.Target, A.Melee(10000.f));
	TestFalse(TEXT("target dead"), A.Target->IsAlive());
	TestEqual(TEXT("dead targets ignore hits"), UWRCombatLibrary::ApplyHit(A.Target, A.Melee(5.f)), EWRHitResult::Ignored);
	return true;
}

#endif
