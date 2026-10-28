// Automation tests for the water you swim in (feedback pass): the sea mask's cells merged into the volume's boxes.
//   Tools\test.ps1 Water

#include "Misc/AutomationTest.h"
#include "WRSea.h"
#include "WRWater.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRWaterBoxesTest, "Wildreach.Water.Boxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRWaterBoxesTest::RunTest(const FString& Parameters)
{
	// A 64-cell grid: an island in the middle (cells 20..43) with a bay cut into it from the west (rows 30..33 up to
	// cell 35) and a hollow inside it the sea doesn't reach (cells 38..40), which must stay dry.
	FWRSeaMask Mask;
	Mask.Width = Mask.Height = 64;
	Mask.Sea.Init(1, 64 * 64);
	for (int32 Y = 20; Y < 44; ++Y) for (int32 X = 20; X < 44; ++X) Mask.Sea[Y * 64 + X] = 0;
	for (int32 Y = 30; Y < 34; ++Y) for (int32 X = 20; X < 36; ++X) Mask.Sea[Y * 64 + X] = 1;
	const TArray<FIntRect> Boxes = FWRWaterRules::MergeCells(Mask);
	TArray<int32> Covered;
	Covered.Init(0, 64 * 64);
	for (const FIntRect& R : Boxes)
	{
		for (int32 Y = R.Min.Y; Y < R.Max.Y; ++Y) for (int32 X = R.Min.X; X < R.Max.X; ++X) ++Covered[Y * 64 + X];
	}
	bool bOnce = true;
	for (int32 i = 0; i < 64 * 64; ++i) bOnce &= Covered[i] == (Mask.Sea[i] ? 1 : 0);
	TestTrue(TEXT("every sea cell in exactly one box, no land in any"), bOnce);
	TestEqual(TEXT("the hollow stays dry"), Covered[39 * 64 + 39], 0);
	TestEqual(TEXT("the bay is water"), Covered[31 * 64 + 34], 1);
	// Rows merge into few boxes: the sea around the island and the bay take a handful, not one per cell or row.
	TestTrue(TEXT("few boxes"), Boxes.Num() > 0 && Boxes.Num() <= 8);
	FWRSeaMask Empty;
	TestEqual(TEXT("an empty mask has none"), FWRWaterRules::MergeCells(Empty).Num(), 0);
	return true;
}

#endif
