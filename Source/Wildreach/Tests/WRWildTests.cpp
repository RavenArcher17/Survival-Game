// Automation tests for the ranged weapons, taming, creature AI, clock, island wildlife and version rules (no world
// needed). Run headless:
//   Tools\test.ps1 Ranged / Taming / Creatures / World   (or all of Wildreach)

#include "Misc/AutomationTest.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRMainMenu.h"
#include "WRFarm.h"
#include "WRTuning.h"
#include "WRWildlife.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRRangedItemsTest, "Wildreach.Ranged.Items",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRRangedItemsTest::RunTest(const FString& Parameters)
{
	const FWRItemRegistry& Items = FWRItemRegistry::Get();
	const FWRItemDef* Bow = Items.Find(TEXT("hunting_bow"));
	const FWRItemDef* Crossbow = Items.Find(TEXT("crossbow"));
	const FWRItemDef* Spear = Items.Find(TEXT("throwing_spear"));
	const FWRItemDef* Tranq = Items.Find(TEXT("tranq_arrow"));
	if (!TestTrue(TEXT("the ranged items load"), Bow && Crossbow && Spear && Tranq))
	{
		return false;
	}
	// Launchers fire an ammo kind into the ranged slot; throwables are their own ammo and stack.
	TestTrue(TEXT("the bow fires arrows from the ranged slot"), Bow->IsRanged() && Bow->AmmoKind == TEXT("arrow") && Bow->Slot == EWREquipSlot::Ranged);
	TestFalse(TEXT("a bow isn't thrown"), Bow->IsThrowable());
	TestTrue(TEXT("spears are thrown and stack"), Spear->IsThrowable() && Spear->StackSize > 1);
	TestTrue(TEXT("tranq arrows are arrows that add torpor"), Tranq->AmmoOf == TEXT("arrow") && Tranq->AmmoTorpor > 0.f);
	TestTrue(TEXT("the crossbow hits harder and reloads slower than the bow"), Crossbow->RangedDamage > Bow->RangedDamage && Crossbow->Reload > Bow->Reload);
	TestTrue(TEXT("the crossbow is held across the hands in the rifle pose"), Crossbow->HeldAlong == TEXT("hands") && Crossbow->HeldPose == TEXT("rifle_aim"));
	TestTrue(TEXT("every ranged item has a model"), !Bow->HeldMesh.IsEmpty() && !Crossbow->HeldMesh.IsEmpty() && !Spear->HeldMesh.IsEmpty());

	// Ammo: the preferred kind while it lasts, then whatever matches; the wheel cycles through what's carried.
	TArray<FWRItemStack> Slots;
	Slots.SetNum(6);
	FWRInventoryRules::Add(Slots, TEXT("stone_arrow"), 5);
	FWRInventoryRules::Add(Slots, TEXT("tranq_arrow"), 2);
	FWRInventoryRules::Add(Slots, TEXT("crossbow_bolt"), 3);
	TestEqual(TEXT("no preference: the first arrows in the bag"), FWRInventoryRules::FindAmmo(Slots, TEXT("arrow"), NAME_None), FName(TEXT("stone_arrow")));
	TestEqual(TEXT("tranq arrows when chosen"), FWRInventoryRules::FindAmmo(Slots, TEXT("arrow"), TEXT("tranq_arrow")), FName(TEXT("tranq_arrow")));
	TestEqual(TEXT("bolts are not arrows"), FWRInventoryRules::FindAmmo(Slots, TEXT("bolt"), TEXT("tranq_arrow")), FName(TEXT("crossbow_bolt")));
	TestEqual(TEXT("no darts carried"), FWRInventoryRules::FindAmmo(Slots, TEXT("dart"), NAME_None), FName());
	TestEqual(TEXT("the wheel: stone -> tranq"), FWRInventoryRules::NextAmmo(Slots, TEXT("arrow"), TEXT("stone_arrow"), 1), FName(TEXT("tranq_arrow")));
	TestEqual(TEXT("the wheel wraps: tranq -> stone"), FWRInventoryRules::NextAmmo(Slots, TEXT("arrow"), TEXT("tranq_arrow"), 1), FName(TEXT("stone_arrow")));

	// Stackable gear stacks (one slot for three spears); two bows are two separate items.
	TArray<FWRItemStack> Bag;
	Bag.SetNum(4);
	TestEqual(TEXT("three spears fit"), FWRInventoryRules::Add(Bag, TEXT("throwing_spear"), 3), 0);
	TestEqual(TEXT("in one stack"), Bag[0].Count, 3);
	TestEqual(TEXT("two bows fit"), FWRInventoryRules::Add(Bag, TEXT("hunting_bow"), 2), 0);
	TestTrue(TEXT("as two items"), Bag[1].Count == 1 && Bag[2].Count == 1);

	// Crafted launchers roll quality like swords: better quality, more damage.
	const FWRTuning& T = FWRTuning::Get();
	TestTrue(TEXT("quality scales ranged damage"), T.Num(TEXT("weapon.quality.masterwork"), 1.f) > T.Num(TEXT("weapon.quality.common"), 1.f)
		&& T.Num(TEXT("weapon.quality.common"), 1.f) > T.Num(TEXT("weapon.quality.crude"), 1.f));
	const FWRRecipe* BowRecipe = FWRRecipeRegistry::Get().ForOutput(TEXT("hunting_bow"));
	const FWRRecipe* SpearRecipe = FWRRecipeRegistry::Get().ForOutput(TEXT("throwing_spear"));
	TestTrue(TEXT("bows are crafted with a quality roll"), BowRecipe && BowRecipe->bQuality);
	TestTrue(TEXT("spears are made by hand"), SpearRecipe && SpearRecipe->Station == TEXT("hand"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRTamingRulesTest, "Wildreach.Taming.Food",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRTamingRulesTest::RunTest(const FString& Parameters)
{
	const FWRCreatureRegistry& Creatures = FWRCreatureRegistry::Get();
	const FWRCreatureDef* Cinderfang = Creatures.Find(TEXT("cinderfang"));
	const FWRCreatureDef* Ashstrider = Creatures.Find(TEXT("ashstrider"));
	if (!TestTrue(TEXT("the creatures load"), Cinderfang && Ashstrider))
	{
		return false;
	}
	TestEqual(TEXT("a Cinderfang eats raw meat"), Cinderfang->FoodValue(TEXT("raw_meat")), 1.f);
	TestEqual(TEXT("but not berries"), Cinderfang->FoodValue(TEXT("berries")), 0.f);
	TestEqual(TEXT("an Ashstrider eats berries"), Ashstrider->FoodValue(TEXT("berries")), 1.f);
	TestTrue(TEXT("kibble beats plain food for both"), Cinderfang->FoodValue(TEXT("kibble")) > 1.f && Ashstrider->FoodValue(TEXT("kibble")) > 1.f);
	TestTrue(TEXT("harder creatures take longer"), Creatures.Find(TEXT("rimeclaw")) && Creatures.Find(TEXT("rimeclaw"))->TameSeconds() > Cinderfang->TameSeconds()
		&& Cinderfang->TameSeconds() > Ashstrider->TameSeconds());
	// Every tameable creature can be fed something, and kibble and narcotic can be made.
	for (const FWRCreatureDef& Def : Creatures.All())
	{
		if (Def.CanTameBy(TEXT("knockout_feed")) || Def.CanTameBy(TEXT("offer_food")))
		{
			TestTrue(FString::Printf(TEXT("%s eats something"), *Def.Id.ToString()), Def.Food.Num() > 0 && Def.FoodValue(Def.Food[0]) > 0.f);
		}
	}
	TestNotNull(TEXT("kibble has a recipe"), FWRRecipeRegistry::Get().ForOutput(TEXT("kibble")));
	TestNotNull(TEXT("narcotic has a recipe"), FWRRecipeRegistry::Get().ForOutput(TEXT("narcotic")));
	const FWRTuning& T = FWRTuning::Get();
	TestTrue(TEXT("a perfect tame gets a bonus, effectiveness is lost per hit"), T.Num(TEXT("taming.bonusAtFull"), 0.f) > 0.f && T.Num(TEXT("taming.effectivenessPerHit"), 0.f) > 0.f);
	TestTrue(TEXT("a narcotic adds torpor and knocked-out creatures drain slower than awake ones"), T.Num(TEXT("taming.narcoticTorpor"), 0.f) > 0.f
		&& T.Num(TEXT("taming.unconsciousTorporDecay"), 1.f) < T.Num(TEXT("taming.torporDecay"), 0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRCreatureAIRulesTest, "Wildreach.Creatures.Behaviour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRCreatureAIRulesTest::RunTest(const FString& Parameters)
{
	const FWRCreatureRegistry& Creatures = FWRCreatureRegistry::Get();
	const auto Def = [&](const TCHAR* Id) { return Creatures.Find(FName(Id)); };
	if (!TestTrue(TEXT("the creatures load"), Def(TEXT("cinderfang")) && Def(TEXT("pumicehog")) && Def(TEXT("basaltback")) && Def(TEXT("rimeclaw"))))
	{
		return false;
	}
	// Diets come from what they eat when not given.
	TestEqual(TEXT("Cinderfang is a carnivore"), Def(TEXT("cinderfang"))->Diet, FName(TEXT("carnivore")));
	TestEqual(TEXT("Ashstrider is a herbivore"), Def(TEXT("ashstrider"))->Diet, FName(TEXT("herbivore")));
	TestEqual(TEXT("Kilnmite eats ore"), Def(TEXT("kilnmite"))->Diet, FName(TEXT("lithovore")));

	// The food chain: small prey yes, the herd giants and other hunters no; packs take bigger prey.
	TestTrue(TEXT("a Cinderfang hunts a Pumicehog"), FWRCreatureDef::CanHunt(*Def(TEXT("cinderfang")), *Def(TEXT("pumicehog"))));
	TestTrue(TEXT("a Cinderfang pack takes an Emberhorn"), FWRCreatureDef::CanHunt(*Def(TEXT("cinderfang")), *Def(TEXT("emberhorn"))));
	TestFalse(TEXT("nothing hunts a Basaltback"), FWRCreatureDef::CanHunt(*Def(TEXT("rimeclaw")), *Def(TEXT("basaltback"))) || FWRCreatureDef::CanHunt(*Def(TEXT("cinderfang")), *Def(TEXT("basaltback"))));
	TestFalse(TEXT("hunters don't hunt hunters"), FWRCreatureDef::CanHunt(*Def(TEXT("rimeclaw")), *Def(TEXT("cinderfang"))));
	TestFalse(TEXT("herbivores don't hunt"), FWRCreatureDef::CanHunt(*Def(TEXT("emberhorn")), *Def(TEXT("pumicehog"))));
	TestFalse(TEXT("bosses aren't prey"), Def(TEXT("vulkarn")) && FWRCreatureDef::CanHunt(*Def(TEXT("rimeclaw")), *Def(TEXT("vulkarn"))));
	// Every carnivore that roams has something to hunt somewhere.
	for (const FWRCreatureDef& Hunter : Creatures.All())
	{
		if (Hunter.Diet == TEXT("carnivore") && Hunter.Archetype != TEXT("boss"))
		{
			const bool bHasPrey = Creatures.All().ContainsByPredicate([&](const FWRCreatureDef& Prey) { return FWRCreatureDef::CanHunt(Hunter, Prey); });
			TestTrue(FString::Printf(TEXT("%s has prey"), *Hunter.Id.ToString()), bHasPrey);
		}
	}

	// Attacks and activity are known kinds with numbers behind them.
	const FWRTuning& T = FWRTuning::Get();
	for (const FWRCreatureDef& C : Creatures.All())
	{
		for (const FName& Attack : C.Attacks)
		{
			TestTrue(FString::Printf(TEXT("%s: %s is a known attack"), *C.Id.ToString(), *Attack.ToString()),
				(Attack == TEXT("lunge") || Attack == TEXT("charge") || Attack == TEXT("spit") || Attack == TEXT("stomp"))
				&& T.Num(FString::Printf(TEXT("creatureAI.%s.windup"), *Attack.ToString()), -1.f) > 0.f);
		}
		// "storm": out only in a thunderstorm (the Raiju, Storm peaks).
		TestTrue(FString::Printf(TEXT("%s: activity %s"), *C.Id.ToString(), *C.Activity.ToString()),
			C.Activity == TEXT("any") || C.Activity == TEXT("diurnal") || C.Activity == TEXT("nocturnal") || C.Activity == TEXT("storm"));
	}
	TestTrue(TEXT("Gloomhowls hunt at night"), Def(TEXT("gloomhowl")) && Def(TEXT("gloomhowl"))->Activity == TEXT("nocturnal"));
	TestTrue(TEXT("Emberhorns charge"), Def(TEXT("emberhorn"))->Attacks.Contains(TEXT("charge")));
	TestTrue(TEXT("a creature runs before it's dead, sooner when the pack breaks"), T.Num(TEXT("creatureAI.fleeHealth"), 0.f) > 0.f
		&& T.Num(TEXT("creatureAI.packFleeHealth"), 0.f) > T.Num(TEXT("creatureAI.fleeHealth"), 0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRClockRulesTest, "Wildreach.World.Clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRClockRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("noon is day"), AWRGameState::IsNightAt(12.f));
	TestTrue(TEXT("midnight is night"), AWRGameState::IsNightAt(0.f));
	TestTrue(TEXT("23:00 is night"), AWRGameState::IsNightAt(23.f));
	TestFalse(TEXT("dawn starts the day"), AWRGameState::IsNightAt(FWRTuning::Get().Num(TEXT("clock.dawnHour"), 6.f)));
	TestEqual(TEXT("full daylight at noon"), AWRGameState::DaylightAt(12.f), 1.f);
	TestEqual(TEXT("none at midnight"), AWRGameState::DaylightAt(0.f), 0.f);
	const float Dawn = FWRTuning::Get().Num(TEXT("clock.dawnHour"), 6.f);
	const float Dusk = FWRTuning::Get().Num(TEXT("clock.duskHour"), 20.f);
	const float Morning = AWRGameState::DaylightAt(Dawn + 0.5f);
	const float Evening = AWRGameState::DaylightAt(Dusk - 0.5f);
	TestTrue(TEXT("it brightens through dawn"), Morning > 0.f && Morning < 1.f && AWRGameState::DaylightAt(Dawn + 0.25f) < Morning);
	TestTrue(TEXT("and dims through dusk"), Evening > 0.f && Evening < 1.f && AWRGameState::DaylightAt(Dusk - 0.25f) < Evening);
	TestTrue(TEXT("a day lasts some real minutes"), FWRTuning::Get().Num(TEXT("clock.dayMinutes"), 0.f) >= 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRSpawnRegionsTest, "Wildreach.World.SpawnRegions",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext)

bool FWRSpawnRegionsTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("spawn_regions.json"));
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (!TestTrue(TEXT("spawn_regions.json loads, with its regions"), Root.IsValid() && Root->TryGetArrayField(TEXT("regions"), Regions) && Regions->Num() > 0))
	{
		return false;
	}
	TSet<FName> Placed;
	TSet<FName> Sides;
	double Total = 0.0;
	for (const TSharedPtr<FJsonValue>& V : *Regions)
	{
		const TSharedPtr<FJsonObject> R = V->AsObject();
		const FString Id = R->GetStringField(TEXT("id")), Side = R->GetStringField(TEXT("side")), Band = R->GetStringField(TEXT("band"));
		// A region is on a live land (the Old Island's two halves, Storm peaks, the Swamp, the Forest, the Graveyard...) in
		// a band; the new lands' regions follow their sub-zones, so a band is a difficulty, not the creature's tier.
		TestTrue(*FString::Printf(TEXT("%s is on a live land (%s) and in a band"), *Id, *Side), WRBiome::IsLive(FName(*Side))
			&& (Band == TEXT("lowland") || Band == TEXT("midland") || Band == TEXT("highland")));
		Sides.Add(FName(*Side));
		TestTrue(*FString::Printf(TEXT("%s has a population target"), *Id), R->GetNumberField(TEXT("target")) >= 1.0);
		Total += R->GetNumberField(TEXT("target"));
		const TSharedPtr<FJsonObject>* Kinds = nullptr;
		if (!TestTrue(*FString::Printf(TEXT("%s has its creatures"), *Id), R->TryGetObjectField(TEXT("creatures"), Kinds) && (*Kinds)->Values.Num() > 0))
		{
			continue;
		}
		// Day, night, storm and raider tables all name creatures of the region's own land.
		for (const TCHAR* Table : { TEXT("creatures"), TEXT("night"), TEXT("storm"), TEXT("raiders") })
		{
			const TSharedPtr<FJsonObject>* Rows = nullptr;
			if (!R->TryGetObjectField(Table, Rows)) continue;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Rows)->Values)
			{
				const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*KV.Key));
				if (!TestNotNull(*FString::Printf(TEXT("%s in %s (%s) is a creature"), *KV.Key, *Id, Table), Def))
				{
					continue;
				}
				TestTrue(*FString::Printf(TEXT("%s has a weight in %s (%s)"), *KV.Key, *Id, Table), KV.Value->AsNumber() > 0.0);
				TestTrue(*FString::Printf(TEXT("%s lives on %s's land (%s)"), *KV.Key, *Id, *Def->Biome.ToString()), Def->Biome.ToString() == Side);
				Placed.Add(Def->Id);
			}
		}
	}
	// Every wild species of a land the regions fill has a region (not bosses, minis, their minions, summons or portal
	// creatures: those come from their arenas, events or code). The other lands spawn from their own tables for now.
	// Spawned only by their boss, a raise or an event (audit 11-18 data.md 6.4): Vharra's brood and her leeches, the
	// Graveyard's risen dead and the Crypt Mother's young.
	static const TSet<FName> ByCode = { TEXT("vharra_brood"), TEXT("latch_leech"), TEXT("hex_totem"), TEXT("risen_mourner"), TEXT("risen_hound"),
		TEXT("risen_bearer"), TEXT("crypt_young"), TEXT("galvara_core"), TEXT("wisp"), TEXT("training_dummy") };
	for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All())
	{
		const FString Role = Def.Role;
		const bool bWild = Def.Archetype != TEXT("boss") && Def.Tier != TEXT("boss") && !Def.Biome.IsNone() && Sides.Contains(Def.Biome)
			&& !Def.bPortalOnly && Def.PortalRank == 0 && !Role.Contains(TEXT("boss")) && !Role.Contains(TEXT("minion"))
			&& !Role.Contains(TEXT("summon")) && !Role.Contains(TEXT("champion")) && Role != TEXT("copy") && Role != TEXT("double")
			&& !ByCode.Contains(Def.Id);
		if (bWild) TestTrue(*FString::Printf(TEXT("%s has a region to live in"), *Def.Id.ToString()), Placed.Contains(Def.Id));
	}
	TestTrue(*FString::Printf(TEXT("the island is busy (%.0f animals)"), Total), Total >= 200.0);
	TestTrue(TEXT("new packs start out of a player's sight"), FWRTuning::Get().Num(TEXT("wildlife.spawnAwayFromPlayers"), 0.f) > FWRTuning::Get().Num(TEXT("wildlife.activationRadius"), 0.f)
		&& FWRTuning::Get().Num(TEXT("wildlife.spawnAwayFromBuildings"), 0.f) > 0.f && FWRTuning::Get().Num(TEXT("wildlife.spawnCheckSeconds"), 0.f) > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRWalkMapTest, "Wildreach.World.WalkMap",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext)

bool FWRWalkMapTest::RunTest(const FString& Parameters)
{
	// (island_spawns.json is gone since the 0.6.2 region spawner; the packs are checked from the creature rows.)
	for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All())
	{
		if (Def.Archetype == TEXT("boss")) continue;
		TestTrue(*FString::Printf(TEXT("%s has a pack size (%d to %d)"), *Def.Id.ToString(), Def.PackMin, Def.PackMax), Def.PackMin >= 1 && Def.PackMax >= Def.PackMin);
	}
	// Packs are small: 3 or 4 for the pack animals (Raven: big herds didn't feel right; 4 at most everywhere, 11-18),
	// lone hunters alone or in pairs.
	for (const TCHAR* Pack : { TEXT("ashstrider"), TEXT("emberhorn"), TEXT("frostantler"), TEXT("cinderfang"), TEXT("gloomhowl") })
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Pack);
		TestTrue(*FString::Printf(TEXT("%s goes about in threes and fours"), Pack), Def && Def->PackMin == 3 && Def->PackMax == 4);
	}
	for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All())
	{
		if (Def.Archetype != TEXT("boss")) TestTrue(*FString::Printf(TEXT("no pack of %s is over 4"), *Def.Id.ToString()), Def.PackMax <= 4);
	}
	TestTrue(TEXT("hunters can be alone"), FWRCreatureRegistry::Get().Find(TEXT("magmaw")) && FWRCreatureRegistry::Get().Find(TEXT("magmaw"))->PackMin == 1);
	// The walk map keeps creatures on their own ground.
	TestFalse(TEXT("nobody stands in the deep sea"), WRWildlife::CanStand(TEXT("ashstrider"), FVector2D(-199000.0, -199000.0)));
	TestFalse(TEXT("nor on the other side of the island"), WRWildlife::CanStand(TEXT("ashstrider"), FVector2D(130000.0, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRVersionRulesTest, "Wildreach.World.Version",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRVersionRulesTest::RunTest(const FString& Parameters)
{
	// The menu's update offer (the updater, Tools/installer/WildreachUpdater.cs, compares the same way).
	TestTrue(TEXT("v0.2.1 is newer than 0.2.0"), SWRMainMenu::IsNewerVersion(TEXT("v0.2.1"), TEXT("0.2.0")));
	TestTrue(TEXT("v0.10.0 is newer than 0.9.9 (numbers, not text)"), SWRMainMenu::IsNewerVersion(TEXT("v0.10.0"), TEXT("0.9.9")));
	TestTrue(TEXT("v1.0 is newer than 0.9.9"), SWRMainMenu::IsNewerVersion(TEXT("v1.0"), TEXT("0.9.9")));
	TestFalse(TEXT("the same version isn't"), SWRMainMenu::IsNewerVersion(TEXT("v0.2.0"), TEXT("0.2.0")));
	TestFalse(TEXT("0.2 is 0.2.0"), SWRMainMenu::IsNewerVersion(TEXT("0.2"), TEXT("0.2.0")));
	TestFalse(TEXT("an older one isn't"), SWRMainMenu::IsNewerVersion(TEXT("v0.1.9"), TEXT("0.2.0")));
	TestFalse(TEXT("the first playtest's tag isn't a version"), SWRMainMenu::IsNewerVersion(TEXT("playtest-2026-10-25"), TEXT("0.2.0")));
	TestFalse(TEXT("nor a pre-release suffix"), SWRMainMenu::IsNewerVersion(TEXT("v0.3.0-beta"), TEXT("0.2.0")));
	TestFalse(TEXT("no tag (offline) is no update"), SWRMainMenu::IsNewerVersion(TEXT(""), TEXT("0.2.0")));
	const FString Mine = SWRMainMenu::GameVersion();
	TestTrue(FString::Printf(TEXT("this build has a version (%s)"), *Mine), SWRMainMenu::IsNewerVersion(TEXT("v") + Mine + TEXT(".1"), Mine) && !SWRMainMenu::IsNewerVersion(Mine, Mine));
	return true;
}

#endif
