// Automation tests for the world's look (polish pass): sky blending, the sea disc, build-piece detail blocks.
//   Tools\test.ps1 WorldLook

#include "Misc/AutomationTest.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRSea.h"
#include "WRSky.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRSkyAirTest, "Wildreach.WorldLook.SkyAir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRSkyAirTest::RunTest(const FString& Parameters)
{
	// The biome blend: volcanic (west) 0, tundra (east) 1, half way on the ridge line, smooth across the band.
	TestEqual(TEXT("far west is volcanic"), FWRSkyRules::BiomeBlend(-150000.f, 0.f, 60000.f), 0.f);
	TestEqual(TEXT("far east is tundra"), FWRSkyRules::BiomeBlend(150000.f, 0.f, 60000.f), 1.f);
	TestTrue(TEXT("the ridge line is half and half"), FMath::IsNearlyEqual(FWRSkyRules::BiomeBlend(0.f, 0.f, 60000.f), 0.5f, 0.01f));
	TestTrue(TEXT("rises across the band"), FWRSkyRules::BiomeBlend(-10000.f, 0.f, 60000.f) < FWRSkyRules::BiomeBlend(10000.f, 0.f, 60000.f));
	// Golden hour: strong just over the horizon, gone at noon and with the sun well down.
	TestTrue(TEXT("a low sun counts"), FWRSkyRules::LowSun(2.f, 18.f) > 0.9f);
	TestEqual(TEXT("not at noon"), FWRSkyRules::LowSun(60.f, 18.f), 0.f);
	TestEqual(TEXT("not deep in the night"), FWRSkyRules::LowSun(-20.f, 18.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRSeaMeshTest, "Wildreach.WorldLook.SeaMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRSeaMeshTest::RunTest(const FString& Parameters)
{
	// A 64-cell grid with an island in the middle (cells 24..39), as Data/sea_mask.json's rows would say it.
	FWRSeaMask Mask;
	Mask.Width = Mask.Height = 64;
	Mask.Sea.Init(1, 64 * 64);
	for (int32 Y = 24; Y < 40; ++Y) for (int32 X = 24; X < 40; ++X) Mask.Sea[Y * 64 + X] = 0;
	Mask.ComputeDistance();
	TestEqual(TEXT("land is no distance from land"), Mask.DistanceAt(30, 30), 0.f);
	TestEqual(TEXT("one cell off the shore"), Mask.DistanceAt(23, 30), 1.f);
	TestTrue(TEXT("off the grid it keeps growing"), Mask.DistanceAt(-10, 30) > Mask.DistanceAt(0, 30) + 9.f);

	const int32 Root = 256;
	const TArray<FWRSeaLeaf> Leaves = FWRSeaRules::BuildLeaves(Mask, Root, 2.f, 64);
	TestTrue(TEXT("some squares"), Leaves.Num() > 50);
	// Every sea cell of the grid in exactly one square, no square over dry land, the shore at the finest size.
	TArray<int32> Covered;
	Covered.Init(0, 64 * 64);
	bool bOverLand = false, bShoreFine = true;
	for (const FWRSeaLeaf& L : Leaves)
	{
		for (int32 Y = FMath::Max(0, L.Y); Y < FMath::Min(64, L.Y + L.Size); ++Y)
		{
			for (int32 X = FMath::Max(0, L.X); X < FMath::Min(64, L.X + L.Size); ++X)
			{
				++Covered[Y * 64 + X];
				bOverLand |= !Mask.IsSea(X, Y);
				bShoreFine &= Mask.DistanceAt(X, Y) > 1.5f || L.Size == 1;
			}
		}
	}
	bool bOnce = true;
	for (int32 i = 0; i < 64 * 64; ++i) bOnce &= Covered[i] == (Mask.Sea[i] ? 1 : 0);
	TestTrue(TEXT("each sea cell drawn once, land never"), bOnce);
	TestFalse(TEXT("no square over land"), bOverLand);
	TestTrue(TEXT("finest at the shore"), bShoreFine);
	// Neighbours at most one size apart (so the mesh has no cracks), and the whole root is covered but the island.
	const TSet<FWRSeaLeaf> Set(Leaves);
	const int32 Start = FWRSeaRules::RootStart(Mask, Root);
	int64 Area = 0;
	bool bBalanced = true;
	for (const FWRSeaLeaf& L : Leaves)
	{
		Area += int64(L.Size) * L.Size;
		for (int32 S = 1; S <= 64; S *= 2)
		{
			const FWRSeaLeaf Right{ Start + FMath::FloorToInt((L.X + L.Size - Start) / float(S)) * S, Start + FMath::FloorToInt((L.Y - Start) / float(S)) * S, S };
			if (const FWRSeaLeaf* N = Set.Find(Right)) bBalanced &= N->Size * 2 >= L.Size && L.Size * 2 >= N->Size;
		}
	}
	TestTrue(TEXT("neighbours one size apart at most"), bBalanced);
	TestEqual(TEXT("everything but the island"), Area, int64(Root) * Root - 16 * 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRBuildLookTest, "Wildreach.WorldLook.BuildPieces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRBuildLookTest::RunTest(const FString& Parameters)
{
	const FTransform At(FRotator(0.f, 90.f, 0.f), FVector(-132000.f, -56400.f, 4834.f));
	int32 Structures = 0;
	for (const FWRPieceDef& Def : FWRBuildRegistry::Get().All())
	{
		const TArray<FWRLookBlock> Blocks = FWRBuildLook::Blocks(Def, At);
		if (Def.Snaps())
		{
			++Structures;
			TestTrue(FString::Printf(TEXT("%s has detail"), *Def.Id.ToString()), Blocks.Num() > 0);
			// Sensible sizes, near the piece (the detail dresses the collision blocks; it doesn't wander off).
			bool bNear = true, bSized = true;
			for (const FWRLookBlock& B : Blocks)
			{
				bNear &= FMath::Abs(B.Centre.X) < 260.f && FMath::Abs(B.Centre.Y) < 260.f && B.Centre.Z > -160.f && B.Centre.Z < 560.f;
				bSized &= B.Size.GetMin() > 0.f && B.Size.GetMax() < 520.f && B.Tone >= 0.f && B.Tone <= 1.f;
			}
			TestTrue(FString::Printf(TEXT("%s blocks stay on the piece"), *Def.Id.ToString()), bNear);
			TestTrue(FString::Printf(TEXT("%s blocks sized"), *Def.Id.ToString()), bSized);
			// Kept light: a 50-piece base must stay cheap to draw.
			TestTrue(FString::Printf(TEXT("%s under 400 blocks (%d)"), *Def.Id.ToString(), Blocks.Num()), Blocks.Num() < 400);
		}
	}
	TestTrue(TEXT("every structure piece checked"), Structures >= 20);

	// Two walls meeting at a corner draw one post: same place, same shade, so it can't flicker.
	const FWRPieceDef* Wall = FWRBuildRegistry::Get().Find(TEXT("wood_wall"));
	if (TestNotNull(TEXT("wood wall"), Wall))
	{
		const FTransform A(FRotator::ZeroRotator, FVector(0.f, -200.f, 100.f));
		const FTransform B(FRotator(0.f, 90.f, 0.f), FVector(200.f, 0.f, 100.f));
		const auto PostAt = [](const TArray<FWRLookBlock>& Blocks, const FTransform& T, const FVector& Where, float& OutTone)
		{
			for (const FWRLookBlock& Bl : Blocks)
			{
				if (Bl.Size.Z > 250.f && FVector::Dist2D(T.TransformPosition(Bl.Centre), Where) < 1.f)
				{
					OutTone = Bl.Tone;
					return true;
				}
			}
			return false;
		};
		float ToneA = -1.f, ToneB = -2.f;
		TestTrue(TEXT("first wall has the corner post"), PostAt(FWRBuildLook::Blocks(*Wall, A), A, FVector(200.f, -200.f, 0.f), ToneA));
		TestTrue(TEXT("second wall has the corner post"), PostAt(FWRBuildLook::Blocks(*Wall, B), B, FVector(200.f, -200.f, 0.f), ToneB));
		TestEqual(TEXT("one shade for the shared post"), ToneA, ToneB);
	}
	TestEqual(TEXT("shade is the same for the same spot"), FWRBuildLook::Hash01(FVector(10.f, 20.f, 30.f), 3), FWRBuildLook::Hash01(FVector(10.4f, 19.6f, 30.2f), 3));
	return true;
}

#endif
