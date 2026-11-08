// Creature raids (WRRaids): every region lists raiders that exist and hunt; a raid's plan spends its points on them as far
// as they go (never over, never a kind from elsewhere) and grows with the Hearth level; a spot on either side of the
// island finds its side's region. (Tools/raidtest.ps1 plays a whole raid.)

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "WRCreature.h"
#include "WRRaids.h"
#include "WRTuning.h"
#include "WRWildlife.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRRaidPlanTest, "Wildreach.Raids.Plan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRRaidPlanTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("spawn_regions.json"));
	if (!TestTrue(TEXT("Data/spawn_regions.json loads"), Root.IsValid())) return false;
	const float Base = WRTune(TEXT("raid.points"), 3.f), PerLevel = WRTune(TEXT("raid.pointsPerHearthLevel"), 2.f);
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("regions")))
	{
		const TSharedPtr<FJsonObject> R = V->AsObject();
		const FString Id = R->GetStringField(TEXT("id"));
		const TSharedPtr<FJsonObject>* Raiders = nullptr;
		if (!TestTrue(FString::Printf(TEXT("%s lists its raiders"), *Id), R->TryGetObjectField(TEXT("raiders"), Raiders))) continue;
		TArray<TPair<FName, float>> Kinds;
		int32 Cheapest = MAX_int32;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Raiders)->Values)
		{
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*KV.Key));
			TestTrue(FString::Printf(TEXT("%s's raider %s is a creature that hunts"), *Id, *KV.Key), Def && Def->Behavior == TEXT("aggressive") && Def->Tier != TEXT("boss"));
			Kinds.Add({ FName(*KV.Key), static_cast<float>(KV.Value->AsNumber()) });
			Cheapest = FMath::Min(Cheapest, WRRaids::CostOf(FName(*KV.Key)));
		}
		int32 LastCost = 0;
		for (int32 Level = 0; Level <= 3; ++Level)
		{
			const int32 Points = FMath::RoundToInt(Base + PerLevel * Level);
			int32 Least = MAX_int32;
			for (int32 Seed = 0; Seed < 40; ++Seed)
			{
				FRandomStream Rand(Seed * 31 + Level);
				const TArray<TPair<FName, int32>> Plan = WRRaids::Plan(Kinds, Points, Rand);
				int32 Cost = 0;
				bool bOwn = true;
				for (const TPair<FName, int32>& P : Plan)
				{
					Cost += WRRaids::CostOf(P.Key) * P.Value;
					bOwn &= Kinds.ContainsByPredicate([&P](const TPair<FName, float>& K) { return K.Key == P.Key; });
				}
				if (Cost > Points || Points - Cost >= Cheapest || !bOwn || Plan.Num() == 0)
				{
					AddError(FString::Printf(TEXT("%s level %d (%d points), seed %d: cost %d, own kinds %d, kinds %d"), *Id, Level, Points, Seed, Cost, bOwn ? 1 : 0, Plan.Num()));
					break;
				}
				Least = FMath::Min(Least, Cost);
			}
			TestTrue(FString::Printf(TEXT("%s: a level %d Hearth brings at least as much as level %d"), *Id, Level, Level - 1), Least >= LastCost);
			LastCost = Least;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRRaidRegionTest, "Wildreach.Raids.Regions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRRaidRegionTest::RunTest(const FString& Parameters)
{
	// Either side of the divide (X = 0), at a few distances across: always a region of that side, with raiders of it.
	for (const float Across : { 20000.f, 60000.f, 110000.f, 160000.f })
	{
		for (const float Sign : { -1.f, 1.f })
		{
			TArray<TPair<FName, float>> Kinds;
			const FName Region = WRWildlife::RaidersAt(FVector2D(Sign * Across, 0.f), Kinds);
			const FString Side = Sign < 0.f ? TEXT("volcanic") : TEXT("tundra");
			TestTrue(FString::Printf(TEXT("%s side at %.0f m: a %s region (%s)"), *Side, Across / 100.f, *Side, *Region.ToString()), Region.ToString().StartsWith(Side));
			bool bSide = Kinds.Num() > 0;
			for (const TPair<FName, float>& K : Kinds)
			{
				const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(K.Key);
				bSide &= Def && Def->Biome.ToString() == Side;
			}
			TestTrue(FString::Printf(TEXT("%s side at %.0f m: its raiders live on that side"), *Side, Across / 100.f), bSide);
		}
	}
	return true;
}

#endif
