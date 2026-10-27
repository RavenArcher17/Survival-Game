// Automation tests for the performance rules (no world needed). Run headless:
//   Tools\test.ps1 Perf

#include "Misc/AutomationTest.h"
#include "WRSignificance.h"
#include "WRTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRSignificanceRulesTest, "Wildreach.Perf.Significance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRSignificanceRulesTest::RunTest(const FString& Parameters)
{
	const float FarRadius = WRTune(TEXT("significance.farRadius"), 20000.f);
	const float Margin = WRTune(TEXT("significance.margin"), 2000.f);
	const float NearRadius = FarRadius - Margin;
	// Creatures replicate to clients within 150 m: anything a client can see must stay at full rate, with room to spare
	// for how far a creature runs between two checks.
	TestTrue(TEXT("far starts well beyond the 150 m clients are sent creatures within"), NearRadius >= 15000.f + 1000.f);
	TestTrue(TEXT("a host on this machine gets at least the remote radius"), WRTune(TEXT("significance.localFarRadius"), 35000.f) >= FarRadius);
	TestTrue(TEXT("a far creature still ticks several times a second"), WRTune(TEXT("significance.farTickInterval"), 0.25f) > 0.f && WRTune(TEXT("significance.farTickInterval"), 0.25f) <= 0.5f);
	// Movement over a far tick: at most 8 steps of farMaxStep before the engine drops time.
	TestTrue(TEXT("a far tick fits in the movement's 8 steps"), WRTune(TEXT("significance.farTickInterval"), 0.25f) <= 8.f * WRTune(TEXT("significance.farMaxStep"), 0.1f) * 0.5f);

	TestFalse(TEXT("near a player: full rate"), FWRSignificanceRules::IsFar(5000.f, false, FarRadius, NearRadius));
	TestTrue(TEXT("beyond the far radius: far"), FWRSignificanceRules::IsFar(FarRadius + 1.f, false, FarRadius, NearRadius));
	TestTrue(TEXT("a far one walking back stays far until inside the near radius"), FWRSignificanceRules::IsFar(FarRadius - 100.f, true, FarRadius, NearRadius));
	TestFalse(TEXT("...and is near again inside it"), FWRSignificanceRules::IsFar(NearRadius - 1.f, true, FarRadius, NearRadius));
	TestFalse(TEXT("a near one on the edge stays near"), FWRSignificanceRules::IsFar(FarRadius - 100.f, false, FarRadius, NearRadius));
	return true;
}

#endif
