// The Ancient forest's scenery (cooks/biome-forest.md 2.3, 2.4, 2.20; cooks/build-biome-forest.md 2.5): every landmark
// the world track owns (L2 the Eldest, L3 the Great Falls, L4 the Rotted Treant, L8 the Hermit's Oak, L9 the Strangler
// Arch, L10 the Antler Barrow, L11 the Bell-Tree, L12 the Ringfort), every non-arena hand site, every kit kind, the
// canopy walkways, the waterfalls and what the five shrines stand on. Modelled in code at real scale: trees from the
// tree kit (WRTreeKit), built things from the build look kit (FKit blocks) and code meshes (FWRToolMesh: roots, ropes,
// bones, turned wood). Each piece says why it is there (its story beat, its route, what it pays) and how it meets the
// ground, the water and the player. Built on every machine (no network cost); a dedicated server keeps the collision.
//
// This file: the actor and the dispatch from a site's kind to its builder. The palette, FBuild and the shared pieces are
// in WRForestSceneryKit.h; the builders are split by family:
//   WRForestSceneryLandmarks.cpp  the landmarks and the waterfalls,
//   WRForestScenerySites.cpp      the hand sites of the meadows, the streams and the old growth,
//   WRForestSceneryVillages.cpp   the Grovekin villages, the Elder Canopy's sites, the camps, the village, cave mouths,
//   WRForestSceneryKit.cpp        the kit POIs, fallen logs, walkways and the five shrines' footings.
// The four arena landmarks (four_hearts, clear_cut, paper_hollow, bower) are the boss track's (WRForestBosses.cpp).

#include "WRForestSceneryKit.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace WRForestSceneryImpl
{
	/** The skyline: read from 1 km, never culled. */
	bool IsLandmark(FName K)
	{
		static const FName L[] = { TEXT("eldest"), TEXT("great_falls"), TEXT("rotted_treant"), TEXT("hermits_oak"), TEXT("strangler_arch"), TEXT("antler_barrow"),
			TEXT("bell_tree"), TEXT("ringfort"), TEXT("fall") };
		for (const FName& N : L) if (N == K) return true;
		return false;
	}

	/** The kit POIs (and the logs and walkways that are as common): culled nearer than the hand sites. */
	bool IsKitKind(FName K)
	{
		static const FName L[] = { TEXT("trail_tree"), TEXT("fallen_giant"), TEXT("hedgerow_gap"), TEXT("beehive_tree"), TEXT("house_tree"), TEXT("kill_scene"),
			TEXT("ford"), TEXT("foxfire_bank"), TEXT("field_ruin"), TEXT("canopy_platform"), TEXT("fallen"), TEXT("nurse_trunk") };
		for (const FName& N : L) if (N == K) return true;
		return false;
	}
}

AWRForestScenery::AWRForestScenery()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestScenery::EndPlay(const EEndPlayReason::Type Reason)
{
	for (USceneComponent* Fx : Loops) if (Fx) WRFx::Stop(Fx);
	Loops.Reset();
	if (RainbowFx) { WRFx::Stop(RainbowFx); RainbowFx = nullptr; }
	Super::EndPlay(Reason);
}

void AWRForestScenery::Build()
{
	using WRForestLook::FBuild;
	// Landmarks stay drawn at any range (the forest's skyline); walkways are canopy-layer pieces seen from the floor 40 m
	// below, so they keep the hand sites' range; the kit goes nearer.
	const bool bLandmark = WRForestSceneryImpl::IsLandmark(Kind);
	const float Cull = bLandmark ? 0.f
		: WRForestSceneryImpl::IsKitKind(Kind) ? WRTune(TEXT("forestWorld.kitCull"), 35000.f)
		: WRTune(TEXT("forestWorld.siteCull"), 60000.f);
	FBuild B(this, Cull);
	const FName K = Kind;
	// Landmarks (WRForestSceneryLandmarks.cpp).
	if (K == TEXT("eldest")) B.Eldest();
	else if (K == TEXT("great_falls")) B.GreatFalls();
	else if (K == TEXT("fall")) B.Fall();
	else if (K == TEXT("rotted_treant")) B.RottedTreant();
	else if (K == TEXT("hermits_oak")) B.HermitsOak();
	else if (K == TEXT("strangler_arch")) B.StranglerArch();
	else if (K == TEXT("antler_barrow")) B.AntlerBarrow();
	else if (K == TEXT("bell_tree")) B.BellTree();
	else if (K == TEXT("ringfort")) B.Ringfort();
	// Hand sites: meadows, streams, old growth (WRForestScenerySites.cpp).
	else if (K == TEXT("wreck_meadow")) B.WreckMeadow();
	else if (K == TEXT("farmstead")) B.Farmstead();
	else if (K == TEXT("mill_race")) B.MillRace();
	else if (K == TEXT("truffle_glade")) B.TruffleGlade();
	else if (K == TEXT("woodcutters_camp")) B.WoodcuttersCamp();
	else if (K == TEXT("hanging_oak")) B.HangingOak();
	else if (K == TEXT("hernes_ford")) B.HernesFord();
	else if (K == TEXT("falls_pool")) B.FallsPool();
	else if (K == TEXT("root_bridge")) B.RootBridge();
	else if (K == TEXT("otter_steps")) B.OtterSteps();
	else if (K == TEXT("frosted_ferns")) B.FrostedFerns();
	else if (K == TEXT("nurse_log")) B.NurseLog();
	else if (K == TEXT("bears_larder")) B.BearsLarder();
	else if (K == TEXT("wasp_ravine")) B.WaspRavine();
	else if (K == TEXT("coil_hollow")) B.CoilHollow();
	else if (K == TEXT("offering_stones")) B.OfferingStones();
	// Grovekin villages, the Elder Canopy, camps, the village, caves (WRForestSceneryVillages.cpp).
	else if (K == TEXT("long_hall")) B.LongHall();
	else if (K == TEXT("cradle_wood")) B.CradleWood();
	else if (K == TEXT("herb_terraces")) B.HerbTerraces();
	else if (K == TEXT("bread_stone_lane")) B.BreadStoneLane();
	else if (K == TEXT("root_mine")) B.RootMine();
	else if (K == TEXT("sunken_orchard")) B.SunkenOrchard();
	else if (K == TEXT("crystal_roots")) B.CrystalRoots();
	else if (K == TEXT("rootgrave_fort")) B.RootgraveFort();
	else if (K == TEXT("lantern_line")) B.LanternLine();
	else if (K == TEXT("fern_stair")) B.FernStair();
	else if (K == TEXT("canopy_lift")) B.CanopyLiftFoot();
	else if (K == TEXT("sky_larder")) B.SkyLarder();
	else if (K == TEXT("sinkroot")) B.Sinkroot();
	else if (K == TEXT("camp")) B.Camp();
	else if (K == TEXT("village")) B.Village();
	else if (K == TEXT("cave") || K == TEXT("cave_mouth")) B.CaveMouth();   // forest.json writes the four outer caves as "cave_mouth"
	// The kit, logs, walkways, the shrines' footings (WRForestSceneryKit.cpp).
	else if (K == TEXT("trail_tree")) B.TrailTree();
	else if (K == TEXT("fallen_giant")) B.FallenGiant();
	else if (K == TEXT("fallen")) B.FallenLog(false);
	else if (K == TEXT("nurse_trunk")) B.FallenLog(true);
	else if (K == TEXT("hedgerow_gap")) B.HedgerowGap();
	else if (K == TEXT("beehive_tree")) B.BeehiveTree();
	else if (K == TEXT("house_tree")) B.HouseTree();
	else if (K == TEXT("kill_scene")) B.KillScene();
	else if (K == TEXT("ford")) B.Ford();
	else if (K == TEXT("foxfire_bank")) B.FoxfireBank();
	else if (K == TEXT("field_ruin")) B.FieldRuin();
	else if (K == TEXT("canopy_platform")) B.CanopyPlatform();
	else if (K == TEXT("walkway")) B.WalkwayKind();
	else if (K == TEXT("shrine_grove_circle")) B.ShrineGroveCircle();
	else if (K == TEXT("shrine_hedge")) B.ShrineHedge();
	else if (K == TEXT("shrine_falls")) B.ShrineFalls();
	else if (K == TEXT("shrine_nurse")) B.ShrineNurse();
	else if (K == TEXT("shrine_canopy")) B.ShrineCanopy();
	else
	{
		// A kind the generator wrote that nothing draws: say so (Data/forest.json and this list must agree).
		UE_LOG(LogWildreach, Warning, TEXT("FOREST scenery: no builder for kind %s (%s)"), *K.ToString(), *SiteId.ToString());
		return;
	}
	B.Finish();
	// The build kit's blocks come as instanced meshes: they get the same range as the rest of the place.
	if (Cull > 0.f && GetNetMode() != NM_DedicatedServer)
	{
		for (UActorComponent* Cmp : GetComponents())
		{
			if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Cmp)) I->SetCullDistances(0, int32(Cull));
		}
	}
}
