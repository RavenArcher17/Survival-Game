// Automation tests for the pure rules (no world needed). Run headless:
//   Tools\test.ps1   (UnrealEditor-Cmd ... -ExecCmds="Automation RunTests Wildreach;Quit")

#include "Misc/AutomationTest.h"
#include "WRCombatLibrary.h"
#include "WRInventory.h"
#include "WRPathTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRPathXPTest, "Wildreach.Paths.XPCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRPathXPTest::RunTest(const FString& Parameters)
{
	// Starting numbers: point n costs 100 + 10n, about 60,500 XP for all 100.
	TestEqual(TEXT("point 1 costs 110"), FWRPathRules::PointCost(1), 110);
	TestEqual(TEXT("100 points = 60,500 XP"), FWRPathRules::XPForPoints(100), 60500);
	TestEqual(TEXT("60,500 XP = 100 points"), FWRPathRules::PointsForXP(60500), 100);
	TestEqual(TEXT("one XP short = 99 points"), FWRPathRules::PointsForXP(60499), 99);
	TestEqual(TEXT("XP past the cap stays 100"), FWRPathRules::PointsForXP(1000000), 100);
	TestEqual(TEXT("no XP = 0 points"), FWRPathRules::PointsForXP(0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRCharacterLevelTest, "Wildreach.Paths.CharacterLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRCharacterLevelTest::RunTest(const FString& Parameters)
{
	// Design review #35/#36: level = points / 3; level 50 = exactly 250 health / 150 stamina.
	TestEqual(TEXT("0 points = level 1"), FWRPathRules::CharacterLevel(0), 1);
	TestEqual(TEXT("30 points = level 10"), FWRPathRules::CharacterLevel(30), 10);
	TestEqual(TEXT("150 points = level 50"), FWRPathRules::CharacterLevel(150), 50);
	TestEqual(TEXT("level 1 health"), FWRPathRules::LevelStat(100.f, 250.f, 1), 100.f);
	TestEqual(TEXT("level 50 health"), FWRPathRules::LevelStat(100.f, 250.f, 50), 250.f);
	TestEqual(TEXT("level 50 stamina"), FWRPathRules::LevelStat(100.f, 150.f, 50), 150.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRTreeShapeTest, "Wildreach.Paths.TreeShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRTreeShapeTest::RunTest(const FString& Parameters)
{
	// Every tree: 10-point trunk + three 30-point branches = 100.
	for (const FName PathId : { FName(TEXT("fire")), FName(TEXT("frost")) })
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
		if (!TestNotNull(FString::Printf(TEXT("%s loaded"), *PathId.ToString()), Def))
		{
			continue;
		}
		TMap<FName, int32> Sums;
		for (const FWRPathNodeDef& Node : Def->Nodes)
		{
			Sums.FindOrAdd(Node.Branch) += Node.Cost * Node.MaxRank;
		}
		TestEqual(FString::Printf(TEXT("%s trunk = 10"), *PathId.ToString()), Sums.FindRef(NAME_None), 10);
		TestEqual(FString::Printf(TEXT("%s has trunk + 3 branches"), *PathId.ToString()), Sums.Num(), 4);
		int32 Total = 0;
		for (const TPair<FName, int32>& Pair : Sums)
		{
			Total += Pair.Value;
			if (!Pair.Key.IsNone())
			{
				TestEqual(FString::Printf(TEXT("%s.%s = 30"), *PathId.ToString(), *Pair.Key.ToString()), Pair.Value, 30);
			}
		}
		TestEqual(FString::Printf(TEXT("%s total = 100"), *PathId.ToString()), Total, 100);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRBuyRulesTest, "Wildreach.Paths.BuyRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRBuyRulesTest::RunTest(const FString& Parameters)
{
	const FWRPathDef* Fire = FWRPathRegistry::Get().Find(TEXT("fire"));
	if (!TestNotNull(TEXT("fire loaded"), Fire))
	{
		return false;
	}

	FWRPathProgress P;
	P.PathId = TEXT("fire");
	TestEqual(TEXT("no points, can't buy"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("fire_bolt")), FString(TEXT("not enough points")));

	P.XP = FWRPathRules::XPForPoints(100);
	TestTrue(TEXT("trunk node buyable"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("fire_bolt")).IsEmpty());
	TestEqual(TEXT("branch before trunk"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("spreading_flames")), FString(TEXT("finish the trunk first")));
	TestEqual(TEXT("unknown node"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("nope")), FString(TEXT("no such node")));

	// Finish the trunk (fire_bolt 3, heat_capacity 3 ranks, ignite_chance 2 ranks, flame_slash 2).
	P.SetRank(TEXT("fire_bolt"), 1);
	P.SetRank(TEXT("heat_capacity"), 3);
	P.SetRank(TEXT("ignite_chance"), 2);
	P.SetRank(TEXT("flame_slash"), 1);
	TestEqual(TEXT("trunk spent = 10"), FWRPathRules::Spent(*Fire, P), 10);
	TestTrue(TEXT("trunk complete"), FWRPathRules::TrunkComplete(*Fire, P));
	TestEqual(TEXT("maxed node"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("fire_bolt")), FString(TEXT("already maxed")));
	TestTrue(TEXT("first branch node now buyable"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("spreading_flames")).IsEmpty());
	TestFalse(TEXT("capstone gated"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("wildfire")).IsEmpty());

	// 150 total cap: with 140 in the other path, a 3-point node on 10 spent here would make 153.
	TestEqual(TEXT("total cap"), FWRPathRules::CanBuy(*Fire, P, 140, TEXT("spreading_flames")), FString(TEXT("total cap (150)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRDamageMathTest, "Wildreach.Combat.DamageMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRDamageMathTest::RunTest(const FString& Parameters)
{
	// Starting numbers: copper sword 12 light / 22 heavy; metal x quality x (1 + 5% per upgrade).
	FWRWeaponStats Copper;
	TestEqual(TEXT("copper light"), UWRCombatLibrary::WeaponDamage(Copper, false), 12.f);
	TestEqual(TEXT("copper heavy"), UWRCombatLibrary::WeaponDamage(Copper, true), 22.f);

	FWRWeaponStats Iron;
	Iron.Metal = TEXT("iron");
	Iron.Quality = TEXT("fine");
	Iron.Upgrade = 6;
	TestTrue(TEXT("fine iron +6 light = 29.17"), FMath::IsNearlyEqual(UWRCombatLibrary::WeaponDamage(Iron, false), 12.f * 1.7f * 1.1f * 1.3f, 0.01f));

	// Lowland creature (70 HP) takes about 6 copper light hits.
	TestEqual(TEXT("lowland hits"), FMath::CeilToInt(70.f / UWRCombatLibrary::WeaponDamage(Copper, false)), 6);

	TestEqual(TEXT("block cost floor"), UWRCombatLibrary::BlockStaminaCost(0.f), 10.f);
	TestEqual(TEXT("block cost ceiling"), UWRCombatLibrary::BlockStaminaCost(100.f), 25.f);
	TestTrue(TEXT("full heat bonus"), FMath::IsNearlyEqual(UWRCombatLibrary::HeatBonus(100.f, 100.f), 0.3f));
	TestEqual(TEXT("no heat bonus"), UWRCombatLibrary::HeatBonus(0.f, 100.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRInventoryRulesTest, "Wildreach.Inventory.Stacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRInventoryRulesTest::RunTest(const FString& Parameters)
{
	TArray<FWRItemStack> Slots;
	Slots.SetNum(3);
	// Potions stack to 5: 12 potions fill three slots (5, 5, 2) with nothing left over.
	TestEqual(TEXT("12 potions fit in 3 slots"), FWRInventoryRules::Add(Slots, TEXT("minor_healing_potion"), 12), 0);
	TestEqual(TEXT("count 12"), FWRInventoryRules::CountOf(Slots, TEXT("minor_healing_potion")), 12);
	TestEqual(TEXT("4 more: 3 fit, 1 left over"), FWRInventoryRules::Add(Slots, TEXT("minor_healing_potion"), 4), 1);
	TestEqual(TEXT("unknown items never fit"), FWRInventoryRules::Add(Slots, TEXT("not_an_item"), 2), 2);
	TestEqual(TEXT("remove 7"), FWRInventoryRules::Remove(Slots, TEXT("minor_healing_potion"), 7), 7);
	TestEqual(TEXT("8 left"), FWRInventoryRules::CountOf(Slots, TEXT("minor_healing_potion")), 8);
	TestEqual(TEXT("remove more than you have"), FWRInventoryRules::Remove(Slots, TEXT("minor_healing_potion"), 50), 8);
	TestTrue(TEXT("all slots empty again"), Slots[0].IsEmpty() && Slots[1].IsEmpty() && Slots[2].IsEmpty());
	FWRInventoryRules::Add(Slots, TEXT("wood"), 10);
	TestTrue(TEXT("weight = 10 x 0.2"), FMath::IsNearlyEqual(FWRInventoryRules::TotalWeight(Slots), 2.f));
	return true;
}

#endif
