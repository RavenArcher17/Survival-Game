// Automation tests for the pure rules (no world needed). Run headless:
//   Tools\test.ps1   (UnrealEditor-Cmd ... -ExecCmds="Automation RunTests Wildreach;Quit")

#include "Misc/AutomationTest.h"
#include "WRBuilding.h"
#include "WRCombatLibrary.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRAbilities.h"
#include "WRPathTypes.h"
#include "WRResourceNode.h"

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
	TestEqual(TEXT("no points, can't buy"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("heat_capacity")), FString(TEXT("not enough points")));
	TestTrue(TEXT("the starter needs no points"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("fire_bolt")).IsEmpty());

	P.XP = FWRPathRules::XPForPoints(100);
	TestTrue(TEXT("trunk node buyable"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("fire_bolt")).IsEmpty());
	TestEqual(TEXT("branch before trunk"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("spreading_flames")), FString(TEXT("finish the trunk first")));
	TestEqual(TEXT("unknown node"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("nope")), FString(TEXT("no such node")));

	// Finish the trunk (fire_bolt 3 but the starter is free, heat_capacity 3 ranks, ignite_chance 2 ranks, flame_slash 2).
	P.SetRank(TEXT("fire_bolt"), 1);
	P.SetRank(TEXT("heat_capacity"), 3);
	P.SetRank(TEXT("ignite_chance"), 2);
	P.SetRank(TEXT("flame_slash"), 1);
	TestEqual(TEXT("trunk spent = 7 (the starter came free)"), FWRPathRules::Spent(*Fire, P), 7);
	TestTrue(TEXT("trunk complete"), FWRPathRules::TrunkComplete(*Fire, P));
	TestEqual(TEXT("maxed node"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("fire_bolt")), FString(TEXT("already maxed")));
	TestTrue(TEXT("first branch node now buyable"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("spreading_flames")).IsEmpty());
	TestFalse(TEXT("capstone gated"), FWRPathRules::CanBuy(*Fire, P, 0, TEXT("wildfire")).IsEmpty());

	// 150 total cap: with 141 in the other path, a 3-point node on 7 spent here would make 151.
	TestEqual(TEXT("total cap"), FWRPathRules::CanBuy(*Fire, P, 141, TEXT("spreading_flames")), FString(TEXT("total cap (150)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRStarterSpellTest, "Wildreach.Paths.StarterSpell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRStarterSpellTest::RunTest(const FString& Parameters)
{
	// Every path comes with its first spell at level 1, free, so it has something to earn path XP with.
	const TPair<const TCHAR*, const TCHAR*> Expected[] = { { TEXT("fire"), TEXT("fire_bolt") }, { TEXT("frost"), TEXT("frost_shard") } };
	for (const TPair<const TCHAR*, const TCHAR*>& E : Expected)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(E.Key);
		if (!TestNotNull(FString::Printf(TEXT("%s loaded"), E.Key), Def))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s starter"), E.Key), Def->Starter, FName(E.Value));
		TestNotNull(FString::Printf(TEXT("%s starter is a spell"), E.Key), UWRAbilityRegistry::ClassForNode(Def->Id, Def->Starter).Get());
		FWRPathProgress P;
		P.PathId = Def->Id;
		TestTrue(FString::Printf(TEXT("%s starter given"), E.Key), FWRPathRules::GrantStarter(*Def, P));
		TestFalse(FString::Printf(TEXT("%s starter given once"), E.Key), FWRPathRules::GrantStarter(*Def, P));
		TestEqual(FString::Printf(TEXT("%s starter owned"), E.Key), P.GetRank(Def->Starter), 1);
		TestEqual(FString::Printf(TEXT("%s starter costs nothing"), E.Key), FWRPathRules::Spent(*Def, P), 0);
		TestEqual(FString::Printf(TEXT("%s still level 1"), E.Key), FWRPathRules::CharacterLevel(FWRPathRules::Spent(*Def, P)), 1);
		TestEqual(FString::Printf(TEXT("%s starter maxed"), E.Key), FWRPathRules::CanBuy(*Def, P, 0, Def->Starter), FString(TEXT("already maxed")));
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRCraftingRulesTest, "Wildreach.Crafting.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRCraftingRulesTest::RunTest(const FString& Parameters)
{
	// Hearth tech levels need 0/1/2/4/6 different major boss cores (Crafting tab).
	TestEqual(TEXT("0 cores = level 1"), FWRCraftingRules::TechLevelForCores(0), 1);
	TestEqual(TEXT("1 core = level 2"), FWRCraftingRules::TechLevelForCores(1), 2);
	TestEqual(TEXT("2 cores = level 3"), FWRCraftingRules::TechLevelForCores(2), 3);
	TestEqual(TEXT("3 cores still level 3"), FWRCraftingRules::TechLevelForCores(3), 3);
	TestEqual(TEXT("4 cores = level 4"), FWRCraftingRules::TechLevelForCores(4), 4);
	TestEqual(TEXT("6 cores = level 5"), FWRCraftingRules::TechLevelForCores(6), 5);
	// Upgrade cap +2 per Hearth level; each step costs one more shard.
	TestEqual(TEXT("cap at level 1"), FWRCraftingRules::UpgradeCap(1), 2);
	TestEqual(TEXT("cap at level 3"), FWRCraftingRules::UpgradeCap(3), 6);
	TestEqual(TEXT("+0 -> +1 costs 1"), FWRCraftingRules::UpgradeCost(0), 1);
	TestEqual(TEXT("+5 -> +6 costs 6"), FWRCraftingRules::UpgradeCost(5), 6);
	// Quality odds: low rolls are crude, high rolls masterwork, and better Hearths shift odds up.
	TestEqual(TEXT("roll 0 = crude"), FWRCraftingRules::RollQuality(0.f, 1, 0), EWRQuality::Crude);
	TestEqual(TEXT("roll 0.5 = common"), FWRCraftingRules::RollQuality(0.5f, 1, 0), EWRQuality::Common);
	TestEqual(TEXT("roll 0.999 = masterwork"), FWRCraftingRules::RollQuality(0.999f, 1, 0), EWRQuality::Masterwork);
	TestTrue(TEXT("a better Hearth makes crude rarer"),
		FWRCraftingRules::RollQuality(0.2f, 1, 0) == EWRQuality::Crude && FWRCraftingRules::RollQuality(0.2f, 5, 100) != EWRQuality::Crude);
	// Recipes load and every input/output is a real item.
	TestNotNull(TEXT("copper sword recipe"), FWRRecipeRegistry::Get().Find(TEXT("copper_sword")));
	for (const FName Station : { FName(TEXT("forge")), FName(TEXT("armorer")), FName(TEXT("alchemy_table")), FName(TEXT("workbench")), FName(TEXT("hand")) })
	{
		for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(Station))
		{
			TestNotNull(FString::Printf(TEXT("%s output exists"), *R->Id.ToString()), FWRItemRegistry::Get().Find(R->Output));
			for (const TPair<FName, int32>& In : R->Inputs)
			{
				TestNotNull(FString::Printf(TEXT("%s input %s exists"), *R->Id.ToString(), *In.Key.ToString()), FWRItemRegistry::Get().Find(In.Key));
			}
		}
	}
	// Equipment never stacks and starts at full durability.
	TArray<FWRItemStack> Slots;
	Slots.SetNum(3);
	TestEqual(TEXT("two swords take two slots"), FWRInventoryRules::Add(Slots, TEXT("copper_sword"), 2), 0);
	TestTrue(TEXT("separate instances"), Slots[0].Count == 1 && Slots[1].Count == 1);
	TestEqual(TEXT("full durability"), Slots[0].Durability, 200.f);
	// A fresh survivor can make the crude tools by hand from what the island gives bare-handed (wood, stone); everything
	// else made by hand takes things gathered bare-handed (shrubs give fiber and berries) or made by another recipe.
	for (const FName Tool : { FName(TEXT("crude_hatchet")), FName(TEXT("crude_pick")) })
	{
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(Tool);
		TestTrue(FString::Printf(TEXT("%s made by hand"), *Tool.ToString()), R && R->Station == TEXT("hand"));
		for (const TPair<FName, int32>& In : R ? R->Inputs : TArray<TPair<FName, int32>>())
		{
			TestTrue(FString::Printf(TEXT("%s needs only wood and stone"), *Tool.ToString()), In.Key == TEXT("wood") || In.Key == TEXT("stone"));
		}
	}
	for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(TEXT("hand")))
	{
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			const bool bBareHanded = In.Key == TEXT("wood") || In.Key == TEXT("stone") || In.Key == TEXT("fiber") || In.Key == TEXT("berries");
			TestTrue(FString::Printf(TEXT("%s input %s can be had"), *R->Id.ToString(), *In.Key.ToString()), bBareHanded || FWRRecipeRegistry::Get().ForOutput(In.Key) != nullptr);
		}
	}
	// Every ranged weapon and its ammo can be made.
	for (const FName Id : { FName(TEXT("hunting_bow")), FName(TEXT("crossbow")), FName(TEXT("blowgun")), FName(TEXT("throwing_spear")), FName(TEXT("bola")),
		FName(TEXT("stone_arrow")), FName(TEXT("tranq_arrow")), FName(TEXT("crossbow_bolt")), FName(TEXT("tranq_bolt")), FName(TEXT("torpor_dart")) })
	{
		TestNotNull(FString::Printf(TEXT("%s has a recipe"), *Id.ToString()), FWRRecipeRegistry::Get().ForOutput(Id));
	}
	// Tools work from the bag: the best of the right kind counts, nothing for other materials.
	TestEqual(TEXT("wood wants a hatchet"), FWRGathering::ToolKindFor(TEXT("wood")), FName(TEXT("hatchet")));
	TestEqual(TEXT("ore wants a pick"), FWRGathering::ToolKindFor(TEXT("copper_ore")), FName(TEXT("pick")));
	TestEqual(TEXT("sulfur wants a pick"), FWRGathering::ToolKindFor(TEXT("sulfur_crystal")), FName(TEXT("pick")));
	TestEqual(TEXT("fiber has no tool"), FWRGathering::ToolKindFor(TEXT("fiber")), FName());
	TestNull(TEXT("bare hands"), FWRGathering::BestTool(TEXT("hatchet"), { TEXT("copper_sword"), TEXT("wood") }));
	const FWRToolDef* Crude = FWRGathering::BestTool(TEXT("hatchet"), { TEXT("crude_hatchet"), TEXT("crude_pick") });
	TestTrue(TEXT("crude hatchet beats bare hands"), Crude && Crude->ItemId == TEXT("crude_hatchet") && Crude->Yield > 1.f);
	const FWRToolDef* Best = FWRGathering::BestTool(TEXT("hatchet"), { TEXT("crude_hatchet"), TEXT("stone_hatchet") });
	TestTrue(TEXT("the stone hatchet is the better one"), Best && Crude && Best->ItemId == TEXT("stone_hatchet") && Best->Yield > Crude->Yield);
	TestNull(TEXT("a pick doesn't chop"), FWRGathering::BestTool(TEXT("hatchet"), { TEXT("stone_pick") }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRBuildRulesTest, "Wildreach.Building.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRBuildRulesTest::RunTest(const FString& Parameters)
{
	const float Grid = 400.f;
	// Edge pieces snap to the nearest cell edge; a gable keeps its flip, stairs and roofs the way they rise.
	float Yaw = 33.f;
	TestEqual(TEXT("wall on the east edge"), FWRBuildRules::SnapToGrid(TEXT("wall"), FVector(170, 20, 0), 50.f, Grid, Yaw), FVector(200, 0, 50));
	TestEqual(TEXT("...running along Y"), Yaw, 90.f);
	Yaw = 180.f;
	FWRBuildRules::SnapToGrid(TEXT("gable"), FVector(20, -150, 0), 0.f, Grid, Yaw);
	TestEqual(TEXT("a gable turned round stays turned"), Yaw, 180.f);
	Yaw = 268.f;
	FWRBuildRules::SnapToGrid(TEXT("roof"), FVector(10, 10, 0), 0.f, Grid, Yaw);
	TestEqual(TEXT("a roof rises the way R turned it"), Yaw, 270.f);
	Yaw = 45.f;
	FWRBuildRules::SnapToGrid(TEXT("floor"), FVector(10, 10, 0), 0.f, Grid, Yaw);
	TestEqual(TEXT("floors sit square"), Yaw, 0.f);
	// One piece per slot.
	TestEqual(TEXT("a door takes a wall's slot"), FWRBuildRules::SlotOf(TEXT("door")), FWRBuildRules::SlotOf(TEXT("wall")));
	TestEqual(TEXT("a doorway too"), FWRBuildRules::SlotOf(TEXT("doorway")), FName(TEXT("edge")));
	TestEqual(TEXT("a roof takes a floor's slot"), FWRBuildRules::SlotOf(TEXT("roof")), FWRBuildRules::SlotOf(TEXT("floor")));
	TestNotEqual(TEXT("pillars have their own"), FWRBuildRules::SlotOf(TEXT("pillar")), FWRBuildRules::SlotOf(TEXT("wall")));
	// Support: straight on top of a wall costs nothing, beside it one step, out of reach not at all.
	TestEqual(TEXT("wall on a wall"), FWRBuildRules::SupportCost(FVector(200, 0, 100), TEXT("wall"), FVector(200, 0, 400), Grid), 0);
	TestEqual(TEXT("floor on walls"), FWRBuildRules::SupportCost(FVector(200, 0, 100), TEXT("wall"), FVector(0, 0, 400), Grid), 1);
	TestEqual(TEXT("floor beside a floor"), FWRBuildRules::SupportCost(FVector(0, 0, 400), TEXT("floor"), FVector(400, 0, 400), Grid), 1);
	TestEqual(TEXT("a floor doesn't carry what's on it for free"), FWRBuildRules::SupportCost(FVector(0, 0, 400), TEXT("floor"), FVector(0, 0, 700), Grid), 1);
	TestEqual(TEXT("two storeys apart don't touch"), FWRBuildRules::SupportCost(FVector(0, 0, 100), TEXT("wall"), FVector(0, 0, 700), Grid), INDEX_NONE);
	TestEqual(TEXT("two cells apart don't touch"), FWRBuildRules::SupportCost(FVector(0, 0, 100), TEXT("foundation"), FVector(800, 0, 100), Grid), INDEX_NONE);
	// The storey you aim into: ground floor (from the foundation top up a wall), the first floor's surface, a ceiling above.
	TestEqual(TEXT("wall aimed at the ground outside"), FWRBuildRules::LevelFor(TEXT("wall"), 940.f, 1000.f), 0);
	TestEqual(TEXT("wall aimed high on a ground-floor wall"), FWRBuildRules::LevelFor(TEXT("wall"), 1290.f, 1000.f), 0);
	TestEqual(TEXT("wall aimed at the first floor"), FWRBuildRules::LevelFor(TEXT("wall"), 1320.f, 1000.f), 1);
	TestEqual(TEXT("ceiling aimed at the ground floor"), FWRBuildRules::LevelFor(TEXT("floor"), 1000.f, 1000.f), 1);
	TestEqual(TEXT("roof aimed at the first floor"), FWRBuildRules::LevelFor(TEXT("roof"), 1320.f, 1000.f), 2);
	// Claim upkeep: an hour at 6 wood/h costs 6 wood (it once took the hour's share rounded up every minute: 60).
	{
		TArray<FWRItemStack> Stock;
		Stock.SetNum(6);
		FWRInventoryRules::Add(Stock, TEXT("wood"), 100);
		FWRInventoryRules::Add(Stock, TEXT("stone"), 2);
		TMap<FName, float> Owed;
		const TMap<FName, float> Rate = { { FName(TEXT("wood")), 6.f }, { FName(TEXT("stone")), 0.5f } };
		bool bAllPaid = true;
		for (int32 Minute = 0; Minute < 60; ++Minute) bAllPaid &= FWRBuildRules::PayUpkeepMinute(Stock, Owed, Rate);
		TestTrue(TEXT("a stocked claim pays every minute of an hour"), bAllPaid);
		TestEqual(TEXT("an hour at 6 wood/h costs 6 wood"), FWRInventoryRules::CountOf(Stock, TEXT("wood")), 94);
		TestEqual(TEXT("half a stone an hour: none taken in the first hour"), FWRInventoryRules::CountOf(Stock, TEXT("stone")), 2);
		for (int32 Minute = 0; Minute < 60; ++Minute) FWRBuildRules::PayUpkeepMinute(Stock, Owed, Rate);
		TestEqual(TEXT("...and one stone by the end of the second"), FWRInventoryRules::CountOf(Stock, TEXT("stone")), 1);
		TArray<FWRItemStack> Empty;
		Empty.SetNum(6);
		TMap<FName, float> Owed2;
		TestFalse(TEXT("an empty claim is unpaid straight away"), FWRBuildRules::PayUpkeepMinute(Empty, Owed2, Rate));
		for (int32 Minute = 0; Minute < 30; ++Minute) FWRBuildRules::PayUpkeepMinute(Empty, Owed2, Rate);
		FWRInventoryRules::Add(Empty, TEXT("wood"), 10);
		FWRInventoryRules::Add(Empty, TEXT("stone"), 10);
		FWRBuildRules::PayUpkeepMinute(Empty, Owed2, Rate);
		TestTrue(TEXT("restocking doesn't charge back pay for the unpaid half hour"), FWRInventoryRules::CountOf(Empty, TEXT("wood")) >= 9);
	}
	// Every piece's costs are real items, and every structure has a known shape.
	for (const FWRPieceDef& Def : FWRBuildRegistry::Get().All())
	{
		for (const TPair<FName, int32>& C : Def.Cost)
		{
			TestNotNull(FString::Printf(TEXT("%s cost %s exists"), *Def.Id.ToString(), *C.Key.ToString()), FWRItemRegistry::Get().Find(C.Key));
		}
		TestTrue(FString::Printf(TEXT("%s has a slot"), *Def.Id.ToString()), !Def.Snaps() || !FWRBuildRules::SlotOf(Def.Shape).IsNone());
	}
	return true;
}

#endif
