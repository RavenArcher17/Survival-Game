// The Ancient forest on the map: the subsystem, its replicated state, the Canopy Lift, snare stakes, the site caches,
// caves and resource nodes, the offering stones, Rowan's day and night, and the forest's interactions (cooks/biome-
// forest.md 2.4-2.7, 2.11, 2.19; cooks/build-biome-forest.md 2.5, 2.12). The scenery itself is WRForestScenery.cpp.
// Everything is placed from Data/forest.json; every number is tuning (forest.*): this file decides how things behave,
// not where they go.

#include "WRForestWorld.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRFarm.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "WRWorldSystems.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace WRForestWorldImpl
{
	FName ForestBiome() { static const FName B(TEXT("forest")); return B; }

	double GameHours(const UObject* WorldContext)
	{
		const AWRGameState* GS = AWRGameState::Get(WorldContext);
		return GS ? GS->GetTotalHours() : 0.0;
	}

	/** Real seconds in one game day (the server's day length). */
	float DaySeconds()
	{
		return FMath::Max(60.f, WRTune(TEXT("server.dayMinutes"), WRTune(TEXT("clock.dayMinutes"), 60.f)) * 60.f);
	}

	AWRPlayerController* ControllerOf(const AActor* Pawn)
	{
		const APawn* P = Cast<APawn>(Pawn);
		return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr;
	}

	void Tell(const AActor* Pawn, const FString& Text)
	{
		if (AWRPlayerController* PC = ControllerOf(Pawn)) PC->ClientNotify(Text);
	}

	/** What's in the weapon hand (none for creatures). */
	const FWRItemStack* Held(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		return Inv ? &Inv->GetEquipped(EWREquipSlot::Weapon) : nullptr;
	}

	/** An axe in hand: a hatchet (any grade, not broken) or an axe-class weapon. */
	bool HoldsAxe(const AActor* Who)
	{
		const FWRItemStack* H = Held(Who);
		if (!H || H->IsEmpty()) return false;
		if (const FWRToolDef* Tool = FWRGathering::FindTool(H->ItemId)) return Tool->Kind == TEXT("hatchet") && H->Durability > 0.f;
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(H->ItemId);
		return Def && Def->WeaponClass == TEXT("axe");
	}

	/** A blade in hand (the knife rule for thorns and rhizomorph): a hatchet, a sword, a dagger or an axe. */
	bool HoldsBlade(const AActor* Who)
	{
		const FWRItemStack* H = Held(Who);
		if (!H || H->IsEmpty()) return false;
		if (FWRGathering::IsSharp(*H)) return true;
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(H->ItemId);
		return Def && (Def->WeaponClass == TEXT("dagger") || Def->WeaponClass == TEXT("axe") || Def->WeaponClass == TEXT("greatsword"));
	}

	/** A tamed creature of this kind belonging to Owner's player within Range of At. */
	bool TameNear(const UWorld* World, FName Kind, const AActor* Owner, const FVector& At, float Range)
	{
		const FString Me = WRWorld::PlayerOf(Owner);
		if (Me.IsEmpty()) return false;
		for (TActorIterator<AWRCreature> It(const_cast<UWorld*>(World)); It; ++It)
		{
			const FWRCreatureDef* Def = It->GetDef();
			if (!Def || Def->Id != Kind || !It->IsAlive() || !It->IsTamed() || It->GetTameOwner() != Me) continue;
			if (FVector::Dist(It->GetActorLocation(), At) <= Range) return true;
		}
		return false;
	}

	/** The ground under a point, or on the forest's own scenery there (a log's back, a platform): WRForestShape::GroundZ,
	 *  so the server's things sit on what every machine draws. */
	FVector Ground(const UWorld* World, const FVector& At, float Up = 3000.f, float Down = 6000.f)
	{
		return FVector(At.X, At.Y, WRForestShape::GroundZ(World, At, Up, Down, float(At.Z), true));
	}

	/** The bare ground under a point (no scenery: a person's standing spot on the earth). */
	FVector Earth(const UWorld* World, const FVector& At)
	{
		return FVector(At.X, At.Y, WRForestShape::GroundZ(World, At, 3000.f, 6000.f, float(At.Z), false));
	}

	/** Default contents of a hand site's cache (DC 2.4 "Pays") when forest.json gives the row no "loot". */
	TMap<FName, int32> SitePays(FName Kind, FString& OutLore, bool& bOnce)
	{
		bOnce = false;
		TMap<FName, int32> L;
		if (Kind == TEXT("wreck_meadow"))
		{
			L = { { TEXT("wood"), 12 }, { TEXT("rope"), 3 }, { TEXT("iron_ingot"), 1 }, { TEXT("lore_ship_lantern"), 1 } };
			OutLore = TEXT("Ship's nails rusted into the ribs, and under the hen-house floor a cracked ship's lantern still wrapped in sailcloth.");
		}
		else if (Kind == TEXT("farmstead"))
		{
			L = { { TEXT("meadow_oats_seeds"), 6 }, { TEXT("idunn_apple_seeds"), 2 }, { TEXT("fiber"), 6 } };
			OutLore = TEXT("A seed chest under the tool wall, its lid carved with an apple tree. Someone meant to come back.");
		}
		else if (Kind == TEXT("mill_race"))
		{
			L = { { TEXT("gears"), 2 }, { TEXT("wood"), 10 }, { TEXT("iron_ore"), 4 } };
			OutLore = TEXT("Two bronze pinions still on their oak arbors, green with age, at the bottom of the gear pit.");
		}
		else if (Kind == TEXT("woodcutters_camp"))
		{
			L = { { TEXT("iron_axe"), 1 }, { TEXT("wood"), 20 }, { TEXT("lore_tally_of_trees"), 1 } };
			OutLore = TEXT("A board nailed to the sledge, scored in fives. Hundreds of fives. The last line is half cut.");
		}
		else if (Kind == TEXT("antler_barrow"))
		{
			// His bow, and the way it was made: the bark scroll under his hand teaches the heartwood longbow (Workbench 3).
			L = { { TEXT("heartwood_longbow"), 1 }, { TEXT("bp_heartwood_longbow"), 1 }, { TEXT("lore_antler_crown"), 1 } };
			OutLore = TEXT("A Grovekin hunter laid out on a bier of antlers, a crown of tines on his brow, a bow of heartwood on his chest and a bark scroll of its making under his hand.");
			bOnce = true;
		}
		else if (Kind == TEXT("offering_stones"))
		{
			L = { { TEXT("copper_ingot"), 3 } };
			OutLore = TEXT("Coins and bone beads pressed into the moss round the bread stone.");
		}
		else if (Kind == TEXT("cradle_wood"))
		{
			L = { { TEXT("lore_bark_cradle"), 1 }, { TEXT("resin"), 2 } };
			OutLore = TEXT("A cradle of folded bark, empty, the root under it grown thick as a thigh.");
		}
		else if (Kind == TEXT("herb_terraces"))
		{
			L = { { TEXT("honey"), 2 }, { TEXT("hiveback_wax"), 2 }, { TEXT("fiber"), 8 } };
			OutLore = TEXT("Stoppered jars in the still's cold firebox: honey, wax, dried herbs.");
		}
		else if (Kind == TEXT("bread_stone_lane"))
		{
			L = { { TEXT("copper_ingot"), 2 }, { TEXT("lore_door_rune"), 1 } };
			OutLore = TEXT("A rune cut on a slate, prised from beside a door: the same rune is on every lintel in the lane.");
		}
		else if (Kind == TEXT("crystal_roots"))
		{
			L = { { TEXT("resonant_crystal"), 1 } };
			OutLore = TEXT("One shard, split off where the beech root broke the boulder. There won't be another.");
			bOnce = true;
		}
		else if (Kind == TEXT("rootgrave_fort"))
		{
			L = { { TEXT("hunting_bow"), 1 }, { TEXT("thorned_arrow"), 12 } };
			OutLore = TEXT("An archer's rack behind the west palisade, every arrow pointed at the pass.");
		}
		else if (Kind == TEXT("sky_larder"))
		{
			L = { { TEXT("hide"), 6 }, { TEXT("jerky"), 4 } };
			OutLore = TEXT("Hides still pegged to dry, and strips of meat gone hard as bark.");
		}
		else if (Kind == TEXT("bears_larder"))
		{
			L = { { TEXT("hide"), 3 }, { TEXT("bone"), 4 } };
			OutLore = TEXT("What the Mossmaul cached: half a Burlbeast under a quilt of moss.");
		}
		else if (Kind == TEXT("wasp_ravine"))
		{
			// DC 2.4 #19: a nest fallen from the overhang (the live ones are the hazards track's), grubs still in it.
			L = { { TEXT("egg_bramblewasp"), 2 }, { TEXT("wasp_paper"), 3 } };
			OutLore = TEXT("A nest torn down from the overhang, split like a gourd. Two fat white grubs still twitch in their cells.");
		}
		else if (Kind == TEXT("coil_hollow"))
		{
			L = { { TEXT("rootcoil_shed"), 1 } };
			OutLore = TEXT("A shed skin twelve metres long, hung in the roots like washing.");
		}
		else if (Kind == TEXT("frosted_ferns"))
		{
			L = { { TEXT("lichen"), 3 }, { TEXT("fiber"), 4 } };
			OutLore = TEXT("A Rime-folk offering at the inuksuk's foot: lichen, tied with forest grass.");
		}
		return L;
	}
}

// ================================================================================================ shapes (shared)

const FWRForestData::FTree* WRForestShape::GiantNear(const FVector& At, float Range, FName Kind)
{
	const FWRForestData& D = FWRForestData::Get();
	const FWRForestData::FTree* Best = nullptr;
	float BestD = Range;
	for (const FWRForestData::FTree& T : D.Giants)
	{
		if (!Kind.IsNone() && T.Kind != Kind) continue;
		const float Dd = FVector::Dist2D(T.Base, At);
		if (Dd < BestD) { BestD = Dd; Best = &T; }
	}
	return Best;
}

float WRForestShape::TrunkRadius(FName Species, float Height)
{
	const float Ref = WRTreeKit::ReferenceHeight(Species);
	return WRTreeKit::ReferenceRadius(Species) * (Ref > 1.f ? Height / Ref : 1.f);
}

void WRForestShape::HermitOak(const FWRForestData::FSite& Oak, float& OutHeight, float& OutRadius)
{
	// The row of "giants" the generator writes for the landmark, else tuning: a big field oak, 26 m.
	if (const FWRForestData::FTree* T = GiantNear(Oak.At, 1500.f, TEXT("hermit_oak")))
	{
		OutHeight = T->Height;
		OutRadius = T->Radius;
		return;
	}
	OutHeight = WRTune(TEXT("forestWorld.hermitOakHeight"), 2600.f);
	OutRadius = FMath::Max(120.f, TrunkRadius(TEXT("oak"), OutHeight));
}

FVector WRForestShape::HermitHouseFloor(const FWRForestData::FSite& Oak)
{
	float H = 0.f, R = 0.f;
	HermitOak(Oak, H, R);
	const FWRForestData::FTree* T = GiantNear(Oak.At, 1500.f, TEXT("hermit_oak"));
	const FVector Base = T ? T->Base : Oak.At;
	// The floor is a ring round the trunk in the fork: Rowan stands between the trunk and the hearth, door side (+X).
	const FRotator Yaw(0.f, Oak.Yaw, 0.f);
	return Base + FVector(0, 0, WRTune(TEXT("forestWorld.hermitHouseZ"), 1200.f) + 22.f) + Yaw.RotateVector(FVector(R + 170.f, -120.f, 0.f));
}

float WRForestShape::CanopyDeckZ(const FWRForestData::FSite& Kit)
{
	// The rows carry their deck height (the Canopy Shrine's deck 30 m up, each canopy platform's: forest.json "deck").
	if (const float Deck = RowDeck(Kit.Id); Deck > 0.f) return float(Kit.At.Z) + Deck;
	// Else the walkway that ends nearest sets the deck's height (the generator's catenary ends on the deck's edge, up to
	// a deck's half-diagonal plus a step away), else 20-40 m by variant.
	const FWRForestData& D = FWRForestData::Get();
	float Best = WRTune(TEXT("forestWorld.deckWalkwayReach"), 2500.f);
	float Z = float(Kit.At.Z) + 2000.f + 1000.f * (Kit.Variant % 3);
	for (const FWRForestData::FWalkway& W : D.Walkways)
	{
		if (!W.Arena.IsNone()) continue;
		const float Da = float(FVector::Dist2D(W.A, Kit.At)), Db = float(FVector::Dist2D(W.B, Kit.At));
		if (Da < Best) { Best = Da; Z = float(W.A.Z); }
		if (Db < Best) { Best = Db; Z = float(W.B.Z); }
	}
	return Z;
}

float WRForestShape::RowDeck(FName RowId)
{
	// forest.json's shrine, kit and site rows may say how far above their ground point they stand ("deck", metres): the
	// generator set it where the walkways meet them. Read once (FWRForestData doesn't carry it).
	static TMap<FName, float> Decks;
	static bool bRead = false;
	if (!bRead)
	{
		bRead = true;
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("forest.json"));
		for (const TCHAR* ListName : { TEXT("shrines"), TEXT("kit"), TEXT("sites") })
		{
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (!Root.IsValid() || !Root->TryGetArrayField(ListName, List)) continue;
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
				double DeckM = 0.0;
				FString Id;
				if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id) && O->TryGetNumberField(TEXT("deck"), DeckM)) Decks.Add(FName(*Id), float(DeckM) * 100.f);
			}
		}
	}
	const float* Found = RowId.IsNone() ? nullptr : Decks.Find(RowId);
	return Found ? *Found : 0.f;
}

float WRForestShape::LiftTopZ(const FVector& Trunk, float TrunkR, float FootZ)
{
	// The walkways the generator ran from the lift's giant start beside its bark at the top landing's height.
	const FWRForestData& D = FWRForestData::Get();
	const float Reach = TrunkR + WRTune(TEXT("forestWorld.lift.walkwayReach"), 800.f);
	float Top = -1.f;
	for (const FWRForestData::FWalkway& W : D.Walkways)
	{
		if (!W.Arena.IsNone()) continue;
		if (FVector::Dist2D(W.A, Trunk) < Reach) Top = FMath::Max(Top, float(W.A.Z));
		if (FVector::Dist2D(W.B, Trunk) < Reach) Top = FMath::Max(Top, float(W.B.Z));
	}
	const float Rise = Top > FootZ ? Top - FootZ : WRTune(TEXT("forestWorld.lift.rise"), 3000.f);
	return FootZ + FMath::Clamp(Rise, WRTune(TEXT("forestWorld.lift.minRise"), 1500.f), WRTune(TEXT("forestWorld.lift.maxRise"), 5000.f));
}

AWRLand* WRForestShape::Land(const UWorld* World)
{
	// The forest's land file is the one whose biome is the forest (Tools/terrain/forest.mjs -> Data/lands/).
	if (!World) return nullptr;
	for (const FWRLandDef& Def : WRLand::All())
	{
		if (Def.Biome == WRForestWorldImpl::ForestBiome()) return UWRLandSubsystem::FindLand(World, Def.Id);
	}
	return nullptr;
}

bool WRForestShape::GroundReady(const UWorld* World)
{
	bool bHasLandFile = false;
	for (const FWRLandDef& Def : WRLand::All()) bHasLandFile |= Def.Biome == WRForestWorldImpl::ForestBiome();
	if (!bHasLandFile) return true;            // a landscape map: its ground is there from the start
	const AWRLand* L = Land(World);
	return L && L->IsGroundReady();
}

float WRForestShape::GroundZ(const UWorld* World, const FVector& At, float Up, float Down, float Fallback, bool bOnScenery, const AActor* Ignore)
{
	if (!World) return Fallback;
	const AWRLand* L = Land(World);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestGround), false, Ignore);
	// Look through anything that isn't the ground (or, asked for, the forest's own built pieces): a few tries.
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, Up), At - FVector(0, 0, Down), ECC_WorldStatic, Q)) break;
		const AActor* A = Hit.GetActor();
		bool bTake;
		if (!A) bTake = true;                                                                 // world geometry
		else if (A->IsA<AWRLand>()) bTake = true;                                             // the land's chunks
		else if (A->IsA<AWRForestScenery>()) bTake = bOnScenery;                              // a deck, a log, a platform
		else bTake = !L && !A->GetIsReplicated() && !A->IsA<APawn>() && !A->IsA<AWRDressing>();  // a landscape map's ground
		if (bTake) return float(Hit.ImpactPoint.Z);
		Q.AddIgnoredActor(A);
	}
	// No hit (the chunk isn't built yet): the height function of whichever land's ground is there (the Graveyard's
	// pieces stand on the Graveyard's samples, not the forest's).
	if (const FWRLandDef* D = WRLand::At(At)) return float(D->ToWorld(D->ToMap(At), D->Height(D->ToMap(At))).Z);
	return L ? L->GroundZ(At) : Fallback;
}

bool WRForestShape::ShrineSpot(const UWorld* World, FName ShrineId, FVector& OutAt, float& OutYaw)
{
	const FWRForestData& D = FWRForestData::Get();
	const FWRForestData::FSite* Row = nullptr;
	for (const FWRForestData::FSite& S : D.Shrines) if (S.Id == ShrineId) Row = &S;
	if (!Row) return false;
	OutYaw = Row->Yaw;
	if (ShrineId == TEXT("falls"))
	{
		// On the Grovekin fishing platform in the Great Falls' pool (DC 2.11): the pool site's deck, which stands its lift
		// over the plunge pool's surface (WRForestScenerySites FallsPool: the same numbers).
		if (const FWRForestData::FSite* Pool = D.FindSite(TEXT("falls_pool")); Pool && FVector::Dist2D(Pool->At, Row->At) < WRTune(TEXT("forestWorld.fallsShrineReach"), 10000.f))
		{
			float Surface = float(Pool->At.Z) + 40.f;
			float Best = 8000.f;
			for (const FWRForestData::FFall& F : D.Falls)
			{
				const float Dd = float(FVector::Dist2D(F.Bottom, Pool->At));
				if (Dd < Best) { Best = Dd; Surface = float(F.Bottom.Z); }
			}
			OutAt = FVector(Pool->At.X, Pool->At.Y, Surface + WRTune(TEXT("forestWorld.fallsDeckLift"), 70.f));
			return true;
		}
	}
	if (ShrineId == TEXT("canopy"))
	{
		OutAt = FVector(Row->At.X, Row->At.Y, CanopyDeckZ(*Row));
		return true;
	}
	// On the ground: the land's height there (the row's own z when there's no land function).
	const AWRLand* L = Land(World);
	OutAt = FVector(Row->At.X, Row->At.Y, L ? L->GroundZ(Row->At) : float(Row->At.Z));
	return true;
}

// ================================================================================================ AWRForestState

AWRForestState::AWRForestState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestState, Glows);
}

AWRForestState* AWRForestState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWRForestState> It(const_cast<UWorld*>(World)); It; ++It) return *It;
	return nullptr;
}

void AWRForestState::AddGlow(const FVector& At, float UntilHours)
{
	FWRForestGlow G;
	G.At = At;
	G.UntilHours = UntilHours;
	Glows.Add(G);
	ForceNetUpdate();
	OnRep_Glows();   // listen server / standalone draws it too
}

void AWRForestState::PruneGlows(double NowHours)
{
	const int32 Before = Glows.Num();
	Glows.RemoveAll([NowHours](const FWRForestGlow& G) { return NowHours >= G.UntilHours; });
	if (Glows.Num() != Before) { ForceNetUpdate(); OnRep_Glows(); }
}

void AWRForestState::OnRep_Glows()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// One soft green light per cut end (the rhizomorph's luminous tips: Armillaria glows where it's freshly broken).
	for (UPointLightComponent* L : GlowLights) if (L) L->DestroyComponent();
	GlowLights.Reset();
	for (const FWRForestGlow& G : Glows)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetUsingAbsoluteLocation(true);
		L->SetWorldLocation(FVector(G.At) + FVector(0, 0, 40.f));
		L->SetLightColor(FLinearColor(FColor(127, 224, 122)));
		L->SetIntensity(WRTune(TEXT("forestWorld.rhizoGlowLight"), 900.f));
		L->SetAttenuationRadius(WRTune(TEXT("forestWorld.rhizoGlowRadius"), 450.f));
		L->SetCastShadows(false);
		L->RegisterComponent();
		GlowLights.Add(L);
	}
}

// ================================================================================================ AWRCanopyLift

AWRCanopyLift::AWRCanopyLift()
{
	bReplicates = true;
	bAlwaysRelevant = false;
	SetNetCullDistanceSquared(FMath::Square(60000.f));
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRCanopyLift::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRCanopyLift, Rise);
	DOREPLIFETIME(AWRCanopyLift, TrunkAt);
	DOREPLIFETIME(AWRCanopyLift, TrunkRadius);
	DOREPLIFETIME(AWRCanopyLift, bGoingUp);
	DOREPLIFETIME(AWRCanopyLift, TripStart);
	DOREPLIFETIME(AWRCanopyLift, TripFrom);
}

float AWRCanopyLift::ServerNow() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	return GS ? float(GS->GetServerWorldTimeSeconds()) : (W ? W->GetTimeSeconds() : 0.f);
}

float AWRCanopyLift::WheelRadius() const { return WRTune(TEXT("forestWorld.lift.wheelRadius"), 110.f); }

FVector AWRCanopyLift::WheelCentre() const
{
	// Local frame: the basket's rope hangs at the origin's XY, the trunk is along +X. The wheel's outer tangent is over the
	// basket, its inner one over the stone (2 R nearer the trunk). The fork is WheelHeadroom over the top landing.
	return FVector(WheelRadius(), 0.f, Rise + WRTune(TEXT("forestWorld.lift.wheelHeadroom"), 450.f));
}

float AWRCanopyLift::PositionAt(float ServerSeconds) const
{
	const float Speed = WRTune(TEXT("forestWorld.lift.speed"), 150.f);
	const float Run = FMath::Max(0.f, ServerSeconds - TripStart) * Speed / FMath::Max(100.f, Rise);
	return FMath::Clamp(TripFrom + (bGoingUp ? Run : -Run), 0.f, 1.f);
}

bool AWRCanopyLift::IsMoving() const
{
	const float T = PositionAt(ServerNow());
	return bGoingUp ? T < 1.f : T > 0.f;
}

FVector AWRCanopyLift::BasketAt(float T) const
{
	return GetActorTransform().TransformPosition(FVector(0.f, 0.f, Rise * T));
}

bool AWRCanopyLift::InReach(const FVector& At) const
{
	const float Reach = WRTune(TEXT("forestWorld.lift.reach"), 300.f);
	const float T = PositionAt(ServerNow());
	// In the basket, or standing at either landing (the bottom sill, the top deck beside the basket's door, +Y).
	if (FVector::Dist(At, BasketAt(T) + FVector(0, 0, 90.f)) < Reach) return true;
	const FVector Side = GetActorRightVector() * 200.f;
	return FVector::Dist(At, BasketAt(0.f) + Side + FVector(0, 0, 90.f)) < Reach || FVector::Dist(At, BasketAt(1.f) + Side + FVector(0, 0, 90.f)) < Reach;
}

FString AWRCanopyLift::Pull(AWRPlayerController* PC)
{
	if (!HasAuthority()) return FString();
	const float Now = ServerNow();
	if (IsMoving()) return TEXT("The lift is already moving");
	const float T = PositionAt(Now);
	// Pulling the stone down raises the basket; letting it up lowers it: whichever end the basket isn't at.
	TripFrom = T;
	bGoingUp = T < 0.5f;
	TripStart = Now;
	ForceNetUpdate();
	OnRep_Trip();
	UE_LOG(LogWildreach, Log, TEXT("FOREST lift %s by %s"), bGoingUp ? TEXT("up") : TEXT("down"), PC && PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("?"));
	return bGoingUp ? TEXT("You haul the stone down: the basket rises") : TEXT("You let the stone up: the basket sinks");
}

void AWRCanopyLift::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	Place(PositionAt(ServerNow()));
}

void AWRCanopyLift::OnRep_Trip()
{
	if (!bLookBuilt) BuildLook();
	if (GetNetMode() == NM_DedicatedServer) return;
	// The creak runs while the rope runs (the axle in its wooden bearings, the rope over the wheel's groove).
	if (IsMoving() && !Creak) Creak = WRSound::StartLoop(this, TEXT("forest.lift_creak"));
}

void AWRCanopyLift::BuildLook()
{
	if (bLookBuilt || Rise <= 0.f) return;
	bLookBuilt = true;
	using WRLookKit::FSurf;
	using WRLookKit::FKit;
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const float R = WheelRadius();
	const FVector W = WheelCentre();
	const FLinearColor Oak = FLinearColor(FColor(90, 70, 54)) * 0.45f;
	const FSurf Wood = FSurf::Wood(Oak);
	const FSurf Pale = FSurf::Wood(FLinearColor(FColor(140, 112, 80)) * 0.45f);
	const FSurf Rope = FSurf::Cloth(FLinearColor(FColor(156, 132, 96)) * 0.5f);
	const FSurf Iron = FSurf::Steel(FLinearColor(0.035f, 0.033f, 0.032f), 0.65f);
	const FSurf StoneS = FSurf::Stone(FLinearColor(FColor(138, 132, 120)) * 0.6f, 0.92f);
	const FSurf Moss = FSurf::Cloth(FLinearColor(FColor(78, 107, 42)) * 0.55f);
	const auto MakePart = [this](const FVector& Local)
	{
		USceneComponent* C = NewObject<USceneComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetRelativeLocation(Local);
		C->RegisterComponent();
		return C;
	};
	const auto Box = [this](USceneComponent* Parent, const FVector& Centre, const FVector& Half)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetupAttachment(Parent);
		B->SetMobility(EComponentMobility::Movable);
		B->SetBoxExtent(Half);
		B->SetRelativeLocation(Centre);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		return B;
	};

	// The fixed parts (local to the actor): the beam socketed into the trunk over the fork, the yoke the wheel axle runs
	// in, a knee brace back to the trunk, the bottom sill the basket lands on, and the top landing deck on the +Y side
	// (the stone runs between the basket and the trunk at Y = 0, so the deck stays clear of it).
	{
		TArray<FWRLookBlock> Blocks;
		FKit K(GetActorTransform(), Blocks);
		const FVector Local = GetActorTransform().InverseTransformPosition(FVector(TrunkAt.X, TrunkAt.Y, GetActorLocation().Z));
		const float TrunkX = Local.X;                       // the trunk's axis, along +X
		const float BeamZ = W.Z + R + 60.f;
		const float BeamOut = -90.f;                        // the beam's free end, just past the basket rope
		const FVector Socket(TrunkX - TrunkRadius * 0.85f, 0.f, BeamZ);
		K.Add(Wood, FVector(Socket.X - BeamOut, 40.f, 46.f), FVector((Socket.X + BeamOut) * 0.5f, 0.f, BeamZ));
		for (const float X : { Socket.X - 20.f, Socket.X - 70.f }) K.Add(Iron, FVector(8.f, 48.f, 54.f), FVector(X, 0.f, BeamZ));   // bands where it's let in
		K.Add(Wood.Dim(0.7f), FVector(60.f, 70.f, 70.f), Socket + FVector(30.f, 0, 0));                                             // the notch's lip of bark
		// The yoke: two iron straps down either side of the wheel to its axle.
		for (const float Y : { -34.f, 34.f })
		{
			K.Rod(Iron, FVector(W.X, Y, BeamZ - 23.f), FVector(W.X, Y, W.Z), 6.f, true);
			K.Add(Iron, FVector(16.f, 6.f, 16.f), FVector(W.X, Y, W.Z));
		}
		K.Rod(Iron, FVector(W.X, -40.f, W.Z), FVector(W.X, 40.f, W.Z), 7.f);                                                        // the axle
		K.Rod(Wood, FVector(TrunkX - TrunkRadius * 0.9f, 0.f, BeamZ - 420.f), FVector(BeamOut + 90.f, 0.f, BeamZ - 20.f), 22.f);    // knee brace
		K.Add(Iron, FVector(14.f, 30.f, 30.f), FVector(TrunkX - TrunkRadius * 0.92f, 0.f, BeamZ - 420.f));
		// The bottom sill: two squared logs the basket settles on, mossed.
		for (const float X : { -70.f, 70.f })
		{
			K.Add(Wood.Dim(0.85f), FVector(26.f, 220.f, 24.f), FVector(X, 0.f, 6.f));
			K.Add(Moss, FVector(20.f, 180.f, 3.f), FVector(X, 0.f, 19.f));
		}
		// The top landing: planks on two joists let into the trunk, a post with an iron ring the walkway ropes tie to.
		const float DeckZ = Rise;
		const float DeckX0 = -140.f, DeckX1 = TrunkX - TrunkRadius * 0.8f;
		for (const float Y : { 170.f, 330.f }) K.Add(Wood, FVector(DeckX1 - DeckX0, 20.f, 24.f), FVector((DeckX0 + DeckX1) * 0.5f, Y, DeckZ - 18.f));
		for (float X = DeckX0 + 15.f; X < DeckX1; X += 30.f) K.Add(Pale.Dim(0.9f + 0.2f * K.Rand(FVector(X, 0, 0), 4)), FVector(28.f, 240.f, 5.f), FVector(X, 250.f, DeckZ - 2.f));
		K.Rod(Wood, FVector(DeckX0 + 10.f, 360.f, DeckZ), FVector(DeckX0 + 10.f, 360.f, DeckZ + 120.f), 14.f);
		K.Disc(Iron, FVector(DeckX0 + 10.f, 368.f, DeckZ + 100.f), FVector(0, 1, 0), 18.f, 3.f);
		K.Rod(Rope, FVector(DeckX0 + 10.f, 360.f, DeckZ + 100.f), FVector(DeckX1, 360.f, DeckZ + 100.f), 4.f);
		if (bDraw) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
		Box(RootComponent, FVector((DeckX0 + DeckX1) * 0.5f, 250.f, DeckZ - 6.f), FVector((DeckX1 - DeckX0) * 0.5f, 120.f, 6.f));
		Box(RootComponent, FVector(0.f, 0.f, 8.f), FVector(90.f, 110.f, 8.f));
	}

	// The wheel: an oak rim with a rope groove, eight spokes into a hub on the axle. It turns about Y (the axle).
	Wheel = MakePart(W);
	{
		TArray<FWRLookBlock> Blocks;
		FKit K(Wheel->GetComponentTransform(), Blocks);
		K.Ring(Wood, FVector::ZeroVector, R - 14.f, 16.f, 22.f, 20);
		K.Ring(Wood.Dim(0.75f), FVector(0, -13.f, 0), R - 4.f, 8.f, 6.f, 20);   // the groove's two lips
		K.Ring(Wood.Dim(0.75f), FVector(0, 13.f, 0), R - 4.f, 8.f, 6.f, 20);
		for (int32 s = 0; s < 8; ++s)
		{
			const float A = FMath::DegreesToRadians(45.f * s);
			K.Rod(Wood, FVector::ZeroVector, FVector(FMath::Cos(A), 0.f, FMath::Sin(A)) * (R - 22.f), 9.f);
		}
		K.Disc(Wood.Dim(0.8f), FVector::ZeroVector, FVector(0, 1, 0), 40.f, 30.f);
		K.Disc(Iron, FVector::ZeroVector, FVector(0, 1, 0), 18.f, 34.f);
		if (bDraw) FWRBuildLook::Spawn(this, Wheel, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
	}

	// The basket: a plank floor in an oak frame, four corner posts, rails at hip height on three sides (the door side +Y
	// open), four slings from the corners to an iron ring the main rope is spliced to.
	Basket = MakePart(FVector::ZeroVector);
	{
		TArray<FWRLookBlock> Blocks;
		FKit K(Basket->GetComponentTransform(), Blocks);
		for (const float X : { -86.f, 86.f }) K.Add(Wood, FVector(12.f, 184.f, 14.f), FVector(X, 0, 20.f));
		for (int32 i = 0; i < 6; ++i) K.Add(Pale.Dim(0.9f + 0.2f * K.Rand(FVector(i, 0, 0), 2)), FVector(170.f, 28.f, 5.f), FVector(0, -75.f + 30.f * i, 29.f));
		const FVector Corners[4] = { FVector(-86, -86, 0), FVector(86, -86, 0), FVector(86, 86, 0), FVector(-86, 86, 0) };
		for (const FVector& C : Corners) K.Rod(Wood, C + FVector(0, 0, 14.f), C + FVector(0, 0, 130.f), 9.f);
		K.Rod(Wood, Corners[0] + FVector(0, 0, 105.f), Corners[1] + FVector(0, 0, 105.f), 6.f);   // back rail (-Y)
		K.Rod(Wood, Corners[1] + FVector(0, 0, 105.f), Corners[2] + FVector(0, 0, 105.f), 6.f);   // trunk side (+X)
		K.Rod(Wood, Corners[3] + FVector(0, 0, 105.f), Corners[0] + FVector(0, 0, 105.f), 6.f);   // outer side (-X)
		// A woven rope rail at knee height under the wooden one (a child can't slip under it).
		K.Rod(Rope, Corners[0] + FVector(0, 0, 60.f), Corners[1] + FVector(0, 0, 60.f), 2.5f);
		K.Rod(Rope, Corners[1] + FVector(0, 0, 60.f), Corners[2] + FVector(0, 0, 60.f), 2.5f);
		K.Rod(Rope, Corners[3] + FVector(0, 0, 60.f), Corners[0] + FVector(0, 0, 60.f), 2.5f);
		const FVector SlingRing(0, 0, 230.f);
		for (const FVector& C : Corners) K.Rod(Rope, C + FVector(0, 0, 128.f), SlingRing, 3.f);
		K.Ring(Iron, SlingRing, 9.f, 3.f, 3.f, 10);
		if (bDraw) FWRBuildLook::Spawn(this, Basket, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
		Box(Basket, FVector(0, 0, 22.f), FVector(92.f, 92.f, 10.f));                  // the floor the riders stand on
		Box(Basket, FVector(0, -88.f, 80.f), FVector(92.f, 4.f, 48.f));               // rails: back, trunk side, outer side
		Box(Basket, FVector(88.f, 0, 80.f), FVector(4.f, 92.f, 48.f));
		Box(Basket, FVector(-88.f, 0, 80.f), FVector(4.f, 92.f, 48.f));
	}

	// The counterweight: a dressed stone in a cradle of crossed ropes, hanging from the inner fall of the rope.
	Stone = MakePart(FVector(2.f * R, 0.f, 0.f));
	{
		TArray<FWRLookBlock> Blocks;
		FKit K(Stone->GetComponentTransform(), Blocks);
		K.Add(StoneS, FVector(96.f, 96.f, 120.f), FVector(0, 0, -150.f));
		K.Add(Moss, FVector(70.f, 70.f, 4.f), FVector(0, 0, -88.f));
		for (const float Y : { -30.f, 30.f })
		{
			K.Rod(Rope, FVector(-50.f, Y, -212.f), FVector(-50.f, Y, -88.f), 3.f);
			K.Rod(Rope, FVector(50.f, Y, -212.f), FVector(50.f, Y, -88.f), 3.f);
			K.Rod(Rope, FVector(-50.f, Y, -212.f), FVector(50.f, Y, -212.f), 3.f);
		}
		for (const float X : { -50.f, 50.f }) K.Rod(Rope, FVector(X, -30.f, -88.f), FVector(0, 0, 0), 3.f);
		for (const float X : { -50.f, 50.f }) K.Rod(Rope, FVector(X, 30.f, -88.f), FVector(0, 0, 0), 3.f);
		K.Ring(Iron, FVector::ZeroVector, 8.f, 3.f, 3.f, 10);
		if (bDraw) FWRBuildLook::Spawn(this, Stone, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
	}

	// The two falls of rope: a unit length each, stretched from the wheel's tangent down to what it carries.
	const auto MakeRope = [&](float X)
	{
		USceneComponent* C = MakePart(FVector(X, 0.f, W.Z));
		TArray<FWRLookBlock> Blocks;
		FKit K(C->GetComponentTransform(), Blocks);
		K.Rod(Rope, FVector::ZeroVector, FVector(0, 0, -100.f), 4.f);
		if (bDraw) FWRBuildLook::Spawn(this, C, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
		return C;
	};
	RopeBasket = MakeRope(0.f);
	RopeStone = MakeRope(2.f * R);
}

void AWRCanopyLift::Place(float T)
{
	if (!Basket || !Stone || !Wheel) return;
	if (FMath::IsNearlyEqual(T, LastT, 0.00001f)) return;
	LastT = T;
	const float R = WheelRadius();
	const FVector W = WheelCentre();
	// The basket's floor at T of the rise; the stone the same distance the other way (one rope over one wheel).
	const float BasketZ = Rise * T;
	const float StoneTop = W.Z - WRTune(TEXT("forestWorld.lift.stoneHang"), 250.f) - Rise * (1.f - T);
	Basket->SetRelativeLocation(FVector(0.f, 0.f, BasketZ));
	Stone->SetRelativeLocation(FVector(2.f * R, 0.f, StoneTop));
	// The rope's travel turns the wheel: angle = distance / radius (it rolls without slipping in its groove).
	Wheel->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(Rise * T / R), 0.f, 0.f));
	RopeBasket->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(0.05f, (W.Z - (BasketZ + 230.f)) / 100.f)));
	RopeStone->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(0.05f, (W.Z - StoneTop) / 100.f)));
}

void AWRCanopyLift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLookBuilt) BuildLook();
	Place(PositionAt(ServerNow()));
	if (Creak && !IsMoving())
	{
		Creak->Stop();
		Creak = nullptr;
		if (GetNetMode() != NM_DedicatedServer) WRSound::PlayAt(this, TEXT("forest.lift_creak"), BasketAt(PositionAt(ServerNow())), 0.5f);   // the thump as it settles
	}
}

void AWRCanopyLift::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Creak) { Creak->Stop(); Creak = nullptr; }
	Super::EndPlay(Reason);
}

// ================================================================================================ AWRForestSnare

AWRForestSnare::AWRForestSnare()
{
	bReplicates = true;
	SetNetCullDistanceSquared(FMath::Square(8000.f));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestSnare::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestSnare, bSprung);
}

void AWRForestSnare::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AWRForestSnare::BuildLook()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	using WRLookKit::FSurf;
	using WRLookKit::FKit;
	// A heartwood stake driven into the ground, a bent sapling-spring tied to it, and a noose of rope laid open on the
	// ground in a ring of pegs: the hunter's spring snare.
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Heart = FSurf::Wood(FLinearColor(FColor(150, 92, 40)) * 0.45f);
	const FSurf Rope = FSurf::Cloth(FLinearColor(FColor(156, 132, 96)) * 0.5f);
	K.Rod(Heart, FVector(-40.f, 0, -20.f), FVector(-40.f, 0, 45.f), 6.f);
	K.Spike(Heart, FVector(-40.f, 0, 45.f), FVector::UpVector, 6.f, 6.f);
	K.Rod(FSurf::Wood(FLinearColor(FColor(110, 92, 60)) * 0.4f), FVector(-40.f, 0, 40.f), FVector(-20.f, 0, 110.f), 3.f);
	for (int32 i = 0; i < 6; ++i) K.Rod(Heart, FRotator(0.f, 60.f * i, 0.f).Vector() * 30.f + FVector(0, 0, -8.f), FRotator(0.f, 60.f * i, 0.f).Vector() * 30.f + FVector(0, 0, 8.f), 2.5f);
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
	Noose = NewObject<USceneComponent>(this);
	Noose->SetupAttachment(RootComponent);
	Noose->RegisterComponent();
	TArray<FWRLookBlock> NB;
	FKit N(Noose->GetComponentTransform(), NB);
	N.Ring(Rope, FVector(0, 0, 2.f), 28.f, 2.5f, 2.5f, 14);
	N.Rod(Rope, FVector(-28.f, 0, 2.f), FVector(-20.f, 0, 108.f), 2.f);
	FWRBuildLook::Spawn(this, Noose, NB, FLinearColor(0.25f, 0.2f, 0.15f), false);
	// (FKit::Ring stands in the XZ plane: the noose is laid flat by turning its component.)
	Noose->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	OnRep_Sprung();
}

void AWRForestSnare::OnRep_Sprung()
{
	if (!Noose) return;
	// Sprung: the noose jerks up off the ground, closed.
	Noose->SetRelativeLocation(bSprung ? FVector(0, 0, 40.f) : FVector::ZeroVector);
	Noose->SetRelativeScale3D(bSprung ? FVector(0.35f, 0.35f, 1.f) : FVector(1.f));
}

void AWRForestSnare::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	if (WRForestWorldImpl::GameHours(this) >= GoneAtHours) { Destroy(); return; }
	if (bSprung) return;
	const float Trigger = WRTune(TEXT("forestWorld.snare.trigger"), 90.f);
	const float MaxRadius = WRTune(TEXT("forestWorld.snare.maxRadius"), 130.f);
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		AWRCreature* C = *It;
		if (!C->IsAlive() || FVector::Dist2D(C->GetActorLocation(), GetActorLocation()) > Trigger + C->GetSimpleCollisionRadius() * 0.5f) continue;
		if (FMath::Abs(C->GetActorLocation().Z - C->GetSimpleCollisionHalfHeight() - GetActorLocation().Z) > 120.f) continue;
		if (C->GetSimpleCollisionRadius() > MaxRadius) continue;                          // up to mount size
		if (C->IsTamed() && C->GetTameOwner() == Placer) continue;                       // your own tames step round it
		bSprung = true;
		OnRep_Sprung();
		ForceNetUpdate();
		WRForest::Entangle(C, WRTune(TEXT("forestWorld.snare.seconds"), 3.f), PlacerBody.Get(), true);
		WRFx::Broadcast(this, TEXT("forest.snare_snap"), GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("nature.entangle"));
		UE_LOG(LogWildreach, Log, TEXT("FOREST snare of %s caught %s"), *Placer, *C->GetName());
		SetLifeSpan(30.f);
		return;
	}
}

// ================================================================================================ UWRForestWorld

UWRForestWorld* UWRForestWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRForestWorld>() : nullptr;
}

bool UWRForestWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRForestWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Only the island's world holds the forest (test arenas and stages don't), and only once the forest is live.
	FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	Map.RemoveFromStart(InWorld.StreamingLevelsPrefix);
	if (!Map.EndsWith(FWRTuning::Get().Str(TEXT("forestWorld.map"), TEXT("L_Island"))) || !WRBiome::IsLive(WRForestWorldImpl::ForestBiome())) return;
	const FWRForestData& D = FWRForestData::Get();
	if (!D.bLoaded)
	{
		UE_LOG(LogWildreach, Warning, TEXT("FOREST is live but Data/forest.json didn't load: nothing of the forest is built"));
		return;
	}
	// Live: Tick waits for the land's ground (WRLand builds its chunks a few a frame on every machine), then queues the
	// scenery, builds it a few places a frame, and (the server) spawns the things with state on what was built.
	bActive = true;
	UE_LOG(LogWildreach, Log, TEXT("FOREST live on %s: waiting for the ground"), *Map);
}

void UWRForestWorld::StepBuild()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bQueued)
	{
		WaitClock += World->GetDeltaSeconds();
		if (!WRForestShape::GroundReady(World))
		{
			// A land that never comes (its file missing or broken) would leave the forest empty: say so once a minute.
			if (WaitClock > 60.f) { WaitClock = 0.f; UE_LOG(LogWildreach, Warning, TEXT("FOREST still waiting for the forest land's ground (Data/lands, biome forest)")); }
			return;
		}
		BuildScenery();
		bQueued = true;
	}
	if (!bSceneryDone)
	{
		const int32 PerFrame = FMath::Max(1, int32(WRTune(TEXT("forestWorld.buildPerFrame"), 6.f)));
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 n = 0; n < PerFrame && PiecesBuilt < Pieces.Num(); ++n, ++PiecesBuilt)
		{
			const FPiece& Pc = Pieces[PiecesBuilt];
			AWRForestScenery* A = World->SpawnActor<AWRForestScenery>(AWRForestScenery::StaticClass(), FTransform(FRotator(0.f, Pc.Yaw, 0.f), Pc.At), P);
			if (!A) continue;
			A->Kind = Pc.Kind;
			A->SiteId = Pc.Id;
			A->Variant = Pc.Variant;
			A->Extra = Pc.E1;
			A->Extra2 = Pc.E2;
			A->Size = Pc.Size;
			A->Theme = Pc.Theme;
			A->Build();
			Scenery.Add(A);
		}
		if (PiecesBuilt < Pieces.Num()) return;
		bSceneryDone = true;
		Pieces.Empty();
		LightClock = 0.f;
		UE_LOG(LogWildreach, Log, TEXT("FOREST scenery built: %d places"), Scenery.Num());
	}
	// The server's things sit on the scenery (heartwood on a log's broken end, the lift beside its giant): after it.
	if (!bServerDone)
	{
		bServerDone = true;
		if (World->GetNetMode() != NM_Client) SpawnServerActors();
	}
}

void UWRForestWorld::Deinitialize()
{
	OnBlightLanded().RemoveAll(this);
	Super::Deinitialize();
}

void UWRForestWorld::BuildScenery()
{
	// Queues every place of the forest in a fixed order (the same on every machine, so each builds the same collision);
	// StepBuild spawns them a few a frame. Order matters: sites first (the falls pool's platform before the Falls Shrine's
	// footing that stands on it), then the kit, the logs, the walkways, the falls, the shrines' footings.
	UWorld* World = GetWorld();
	const FWRForestData& D = FWRForestData::Get();
	Pieces.Reset();
	PiecesBuilt = 0;
	const auto Put = [this](FName Kind, FName Id, const FVector& At, float Yaw, int32 Variant, const FVector& E1, const FVector& E2, float Size, FName Theme)
	{
		FPiece& Pc = Pieces.AddDefaulted_GetRef();
		Pc.Kind = Kind;
		Pc.Id = Id;
		Pc.At = At;
		Pc.Yaw = Yaw;
		Pc.Variant = Variant;
		Pc.E1 = E1;
		Pc.E2 = E2;
		Pc.Size = Size;
		Pc.Theme = Theme;
	};
	// The arena landmarks and the hazards track's pieces are drawn by their own code (WRForestBosses / WRForestArenas,
	// WRForestHazards): the four arenas, the wasp nests, the thornbrake caches, the Lost Path, the eel trap.
	static const FName Skip[] = { TEXT("four_hearts"), TEXT("clear_cut"), TEXT("paper_hollow"), TEXT("bower"), TEXT("wasp_nest"), TEXT("thornbrake_cache"),
		TEXT("lost_path"), TEXT("eel_trap"), TEXT("shrine") };
	const auto Skipped = [](FName Kind) { for (const FName& Sk : Skip) if (Sk == Kind) return true; return false; };
	for (const FWRForestData::FSite& S : D.Sites)
	{
		if (Skipped(S.Kind)) continue;
		Put(S.Kind, S.Id, S.At, S.Yaw, S.Variant, FVector::ZeroVector, FVector::ZeroVector, 0.f, S.Cave);
		if (S.Kind == TEXT("offering_stones"))
		{
			// The flat stone with the carved loaf sits 2.4 m in front of the bent trail tree (WRForestScenerySites
			// OfferingStones): the server answers "offer" there, every machine prompts there.
			FOffering O;
			O.At = S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(240.f, 0.f, 50.f));
			Offerings.Add(O);
		}
	}
	for (const FWRForestData::FSite& S : D.Kit)
	{
		if (Skipped(S.Kind)) continue;
		// A kit fallen giant is drawn from the "fallen" rows (they carry its ends and girth); one with no row of its own
		// gets a log of its own along its yaw.
		if (S.Kind == TEXT("fallen_giant"))
		{
			bool bHas = false;
			for (const FWRForestData::FLog& L : D.Fallen) bHas |= FVector::Dist2D((L.A + L.B) * 0.5f, S.At) < 4000.f || FVector::Dist2D(L.B, S.At) < 2500.f;
			if (bHas) continue;
		}
		Put(S.Kind, S.Id, S.At, S.Yaw, S.Variant, FVector::ZeroVector, FVector::ZeroVector, 0.f, NAME_None);
	}
	for (const FWRForestData::FLog& L : D.Fallen)
	{
		Put(L.bNurse ? FName(TEXT("nurse_trunk")) : FName(TEXT("fallen")), NAME_None, L.A, float((L.B - L.A).Rotation().Yaw), L.Nodes, L.A, L.B, L.Radius, NAME_None);
	}
	for (const FWRForestData::FWalkway& W : D.Walkways)
	{
		if (!W.Arena.IsNone()) continue;   // the Bower's walkways fall: AWRForestWalkway (boss track)
		Put(TEXT("walkway"), W.Id, (W.A + W.B) * 0.5f, float((W.B - W.A).Rotation().Yaw), 0, W.A, W.B, 0.f, NAME_None);
	}
	for (const FWRForestData::FFall& F : D.Falls)
	{
		Put(TEXT("fall"), F.Id, F.Bottom, float((F.Bottom - F.Top).Rotation().Yaw), 0, F.Top, F.Bottom, F.Width, NAME_None);
	}
	for (const FWRForestData::FSite& S : D.Shrines)
	{
		// What each shrine stands on or in (the shrine itself is AWRShrine from world.json, placed at WRForestShape::
		// ShrineSpot): the fishing platform under the falls (the footing goes where the shrine goes: on the pool's deck),
		// the Canopy Shrine's deck 30 m up, the Hedge Shrine's niche, the Nurse Shrine's hollow, the Grove Circle's nine
		// limes (inside the Long-hall).
		FVector FootAt = S.At;
		if (S.Id == TEXT("falls"))
		{
			FVector Spot;
			float SpotYaw = 0.f;
			if (WRForestShape::ShrineSpot(World, S.Id, Spot, SpotYaw)) FootAt = Spot;
		}
		Put(FName(*(TEXT("shrine_") + S.Id.ToString())), S.Id, FootAt, S.Yaw, S.Variant, FVector::ZeroVector, FVector::ZeroVector, 0.f, NAME_None);
	}
	UE_LOG(LogWildreach, Log, TEXT("FOREST scenery queued: %d places (%d sites, %d kit, %d logs, %d walkways, %d falls, %d shrines)"), Pieces.Num(), D.Sites.Num(), D.Kit.Num(), D.Fallen.Num(), D.Walkways.Num(), D.Falls.Num(), D.Shrines.Num());
}

void UWRForestWorld::SpawnCache(const FVector& At, float Yaw, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours)
{
	UWorld* World = GetWorld();
	const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
	AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!C) return;
	C->Kind = TEXT("chest");
	C->Biome = WRForestWorldImpl::ForestBiome();
	C->Title = Title;
	C->Items = Items;
	C->Lore = Lore;
	C->RefillHours = RefillHours;
	C->FinishSpawning(Tr);
	Caches.Add(C);
}

void UWRForestWorld::SpawnNode(FName Item, const FVector& At, FName Kind, int32 Hits, float PerHit, float RespawnSeconds)
{
	using namespace WRForestWorldImpl;
	UWorld* World = GetWorld();
	const FVector Spot = Ground(World, At, 400.f, 1200.f);
	const FTransform Tr(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Spot);
	AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!N) return;
	N->ItemId = Item;
	N->Hits = FMath::Max(1, Hits);
	N->PerHit = FMath::Max(1, FMath::RoundToInt(PerHit));
	N->RespawnSeconds = RespawnSeconds;
	TWeakObjectPtr<UWRForestWorld> Self(this);
	TWeakObjectPtr<AWRResourceNode> WeakNode(N);
	const FName K = Kind;
	if (K == TEXT("heartwood"))
	{
		// DC 2.7: an axe on the amber core, 8 a hit; a tamed Burlbeast within 15 m +25% (its tusks lever the wood); the
		// Bloom x3; the Gnarl heart talisman's woodcutting +15% counts (heartwood is woodcutting).
		N->CanHarvest = [](AWRCharacterBase* Who)
		{
			if (HoldsAxe(Who)) return true;
			Tell(Who, TEXT("Heartwood needs an axe: the amber core turns anything else"));
			return false;
		};
		N->YieldScale = [WeakNode](AWRCharacterBase* Who)
		{
			float Scale = 1.f + UWRLifeSkillsComponent::StatFor(Who, TEXT("woodYield"));
			const AWRResourceNode* Core = WeakNode.Get();
			if (Core && TameNear(Core->GetWorld(), TEXT("burlbeast"), Who, Core->GetActorLocation(), WRTune(TEXT("forestWorld.burlbeastRange"), 1500.f))) Scale *= WRTune(TEXT("forestWorld.burlbeastBonus"), 1.25f);
			if (Core && WRForest::Bloom(Core)) Scale *= WRTune(TEXT("forestWorld.bloomHeartwood"), 3.f);
			return Scale;
		};
	}
	else if (K == TEXT("thorns") || K == TEXT("rhizomorph"))
	{
		// The knife rule: a blade cuts them clean (no bleed while you cut with one held: the hazards track reads the same).
		N->CanHarvest = [K](AWRCharacterBase* Who)
		{
			if (HoldsBlade(Who)) return true;
			Tell(Who, K == TEXT("thorns") ? TEXT("Cut thorns with a blade, not your hands") : TEXT("The laces are tough as rope: cut them with a blade"));
			return false;
		};
		if (K == TEXT("rhizomorph"))
		{
			N->OnHarvested = [Self, WeakNode](AWRCharacterBase* Who)
			{
				// A cut lace glows a game day (DC 2.7); your first one teaches the Bootlace blight (DC 2.19).
				if (Self.IsValid() && WeakNode.IsValid() && Self->State) Self->State->AddGlow(WeakNode->GetActorLocation(), float(GameHours(Self.Get()) + WRTune(TEXT("forestWorld.rhizoGlowHours"), 24.f)));
				AWRPlayerState* PS = Who ? Who->GetPlayerState<AWRPlayerState>() : nullptr;
				static const FName Blight(TEXT("quest_bootlace_blight"));
				if (PS && !PS->KnownBlueprints.Contains(Blight))
				{
					PS->KnownBlueprints.AddUnique(Blight);
					Tell(Who, TEXT("The cut ends glow green and smell of rot. You could put that in a jar: Bootlace blight learned"));
				}
			};
		}
	}
	else if (K == TEXT("honey"))
	{
		// Smoke first (a torch held at the hive 3 s or a smoke pot: WRForest::InSmoke), or the hive's Hivebacks come for you.
		// Honey taken in smoke with no sting is Rowan's "Smoke and honey". Wax comes away with the comb; the Bloom doubles it.
		N->YieldScale = [WeakNode](AWRCharacterBase*) { return WeakNode.IsValid() && WRForest::Bloom(WeakNode.Get()) ? WRTune(TEXT("forestWorld.bloomHoney"), 2.f) : 1.f; };
		N->OnHarvested = [WeakNode](AWRCharacterBase* Who)
		{
			AWRResourceNode* Hive = WeakNode.Get();
			if (!Hive || !Who) return;
			if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) Inv->AddItem(TEXT("hiveback_wax"), 1);
			if (WRForest::InSmoke(Hive, Hive->GetActorLocation()))
			{
				AWRPlayerState* PS = Who->GetPlayerState<AWRPlayerState>();
				UWRWorldSystems* WS = UWRWorldSystems::Get(Hive);
				if (PS && WS) WS->QuestEvent(PS, TEXT("forest_smoke_honey"), NAME_None);
				return;
			}
			int32 Woke = 0;
			for (TActorIterator<AWRCreature> It(Hive->GetWorld()); It; ++It)
			{
				const FWRCreatureDef* Def = It->GetDef();
				if (!Def || Def->Id != TEXT("hiveback") || It->IsTamed() || !It->IsAlive()) continue;
				if (FVector::Dist(It->GetActorLocation(), Hive->GetActorLocation()) > WRTune(TEXT("forestWorld.hiveDefendRadius"), 2500.f)) continue;
				It->SetAggroTarget(Who);
				++Woke;
			}
			Tell(Who, Woke > 0 ? TEXT("The hive boils out at you: smoke it first") : TEXT("The comb tears loose. Smoke calms a hive before you rob it"));
		};
	}
	else if (K == TEXT("oats") || K == TEXT("apple"))
	{
		// Wild crops (survival-crafting 2.x): a stalk or apple, and now and then its seed to plant at home; the Bloom's
		// rare seed chance (10%) on top.
		const FName Seed = K == TEXT("oats") ? FName(TEXT("meadow_oats_seeds")) : FName(TEXT("idunn_apple_seeds"));
		N->OnHarvested = [Seed, WeakNode](AWRCharacterBase* Who)
		{
			UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
			if (!Inv) return;
			float Chance = WRTune(TEXT("forestWorld.wildSeedChance"), 0.35f);
			if (WeakNode.IsValid() && WRForest::Bloom(WeakNode.Get())) Chance += WRTune(TEXT("forestWorld.bloomSeedChance"), 0.1f);
			if (FMath::FRand() < Chance) Inv->AddItem(Seed, 1);
		};
	}
	N->FinishSpawning(Tr);
	Nodes.Add({ N, Kind });
}

void UWRForestWorld::SpawnServerActors()
{
	using namespace WRForestWorldImpl;
	UWorld* World = GetWorld();
	const FWRForestData& D = FWRForestData::Get();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	State = World->SpawnActor<AWRForestState>(AWRForestState::StaticClass(), FTransform::Identity, P);
	const float Refill = WRTune(TEXT("forestWorld.siteRefillHours"), 72.f);
	const float Day = DaySeconds();

	// Site caches: what each hand-made site pays (DC 2.4 "Pays": forest.json "loot" if the row has it, else the cook's
	// list). They fill again after forest.siteRefillHours; the one-of-a-kind ones (the barrow's bow, the crystal) don't.
	for (const FWRForestData::FSite& S : D.Sites)
	{
		FString Lore;
		bool bOnce = false;
		TMap<FName, int32> Items = SitePays(S.Kind, Lore, bOnce);
		if (S.Loot.Num() > 0) Items = S.Loot;
		if (Items.Num() == 0) continue;
		// Where the cache lies in each site's own dressing (WRForestScenery.cpp puts the chest, the bier, the jar there):
		// local XY, the lift over what it rests on, and how far above to start the trace down (under a roof: low).
		FVector Off(150.f, 80.f, 40.f);
		float TraceUp = 300.f;
		if (S.Kind == TEXT("antler_barrow")) { Off = FVector(-1500.f, 0.f, 95.f); TraceUp = 60.f; }   // on the bier in the chamber
		else if (S.Kind == TEXT("mill_race")) Off = FVector(-150.f, 560.f, 40.f);                    // in the walled gear pit
		else if (S.Kind == TEXT("crystal_roots")) Off = FVector(180.f, 0.f, 30.f);
		else if (S.Kind == TEXT("offering_stones")) Off = FVector(240.f, 60.f, 50.f);
		else if (S.Kind == TEXT("woodcutters_camp")) { Off = FVector(-300.f, 420.f, 20.f); TraceUp = 400.f; }   // on the sledge's load
		const FVector At = S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(Off.X, Off.Y, 0.f));
		const FVector Spot = S.Kind == TEXT("sky_larder")
			? S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(0.f, 220.f, 0.f)) + FVector(0, 0, WRTune(TEXT("forestWorld.skyLarderZ"), 3500.f) + 40.f)
			: Ground(World, At, TraceUp, 900.f) + FVector(0, 0, Off.Z);
		SpawnCache(Spot, S.Yaw, S.Name.IsEmpty() ? S.Kind.ToString() : S.Name, Items, Lore, bOnce ? 0.f : Refill);
	}
	// Kit POIs that pay: a canopy platform's hunter's cache, an unlooted house-tree's shelf.
	for (const FWRForestData::FSite& S : D.Kit)
	{
		if (S.Kind == TEXT("canopy_platform"))
		{
			const float Z = WRForestShape::CanopyDeckZ(S);
			SpawnCache(FVector(S.At.X, S.At.Y, Z + 40.f) + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(160.f, 0, 0)), S.Yaw, TEXT("Canopy cache"),
				{ { TEXT("rope"), 2 }, { TEXT("jerky"), 2 }, { TEXT("resin"), 2 } }, TEXT("A Grovekin hunter's box lashed to the deck: rope, dried meat, a lump of amber sap."), Refill);
		}
		else if (S.Kind == TEXT("house_tree") && S.Variant % 3 == 0)
		{
			SpawnCache(Ground(World, S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(-120.f, 0, 0))) + FVector(0, 0, 40.f), S.Yaw, TEXT("House-tree shelf"),
				{ { TEXT("fiber"), 4 }, { TEXT("honey"), 1 }, { TEXT("copper_ingot"), 1 } }, TEXT("A shelf grown out of the living wall, its jars still sealed with wax."), Refill);
		}
	}

	// Caves (DC 2.6): AWRCave with the forest's look and its theme's ore and creatures (WRWorldDressing.cpp).
	for (const FWRForestData::FSite& S : D.Sites)
	{
		if (S.Cave.IsNone()) continue;
		const FTransform Tr(FRotator(0.f, S.Yaw, 0.f), S.At);
		AWRCave* Cave = World->SpawnActorDeferred<AWRCave>(AWRCave::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Cave) continue;
		Cave->Biome = ForestBiome();
		Cave->Theme = S.Cave;
		Cave->FinishSpawning(Tr);
		// The Foxfire Cellar's ceiling is a mat of foxfire with bootlace rhizomorph hanging from it (x2).
		if (S.Cave == TEXT("foxfire_cellar"))
		{
			for (int32 i = 0; i < 2; ++i) SpawnNode(TEXT("bootlace_rhizomorph"), Tr.TransformPosition(FVector(-400.f + 300.f * i, i ? 450.f : -450.f, 0.f)), TEXT("rhizomorph"), 3, 2.f, Day * WRTune(TEXT("forestWorld.rhizoRespawnDays"), 2.f));
		}
	}

	// Heartwood at every fallen giant's broken end (the amber core: DC 2.7), a resin weep halfway along (the wound where
	// it split), nodes = hits; the Nurse Log's is the richest (12). They come back in 3 game days.
	const float HeartwoodRespawn = Day * WRTune(TEXT("forestWorld.heartwoodRespawnDays"), 3.f);
	for (const FWRForestData::FLog& L : D.Fallen)
	{
		const FVector Dir = (L.B - L.A).GetSafeNormal2D();
		SpawnNode(TEXT("heartwood"), L.B - Dir * 60.f + FVector(0, 0, L.Radius * 0.6f), TEXT("heartwood"), L.bNurse ? FMath::Max(L.Nodes, 12) : FMath::Clamp(L.Nodes, 4, 8), WRTune(TEXT("forestWorld.heartwoodPerHit"), 8.f), HeartwoodRespawn);
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
		SpawnNode(TEXT("resin"), FMath::Lerp(L.A, L.B, 0.55f) + Side * (L.Radius + 30.f), TEXT("resin"), 3, 2.f, Day * WRTune(TEXT("forestWorld.resinRespawnDays"), 1.f));
	}
	// A kit fallen giant with no "fallen" row of its own still pays at its broken end (drawn along its yaw, 40 m).
	for (const FWRForestData::FSite& S : D.Kit)
	{
		if (S.Kind != TEXT("fallen_giant")) continue;
		bool bHas = false;
		for (const FWRForestData::FLog& L : D.Fallen) bHas |= FVector::Dist2D((L.A + L.B) * 0.5f, S.At) < 4000.f || FVector::Dist2D(L.B, S.At) < 2500.f;
		if (bHas) continue;
		const FVector Fwd = FRotator(0.f, S.Yaw, 0.f).Vector();
		SpawnNode(TEXT("heartwood"), S.At + Fwd * (WRTune(TEXT("forestWorld.kitLogLength"), 4000.f) - 80.f), TEXT("heartwood"), 4 + S.Variant % 5, WRTune(TEXT("forestWorld.heartwoodPerHit"), 8.f), HeartwoodRespawn);
	}
	// The kit's and the hand sites' gatherables.
	for (const FWRForestData::FSite& S : D.Kit)
	{
		const FRotator Yaw(0.f, S.Yaw, 0.f);
		if (S.Kind == TEXT("beehive_tree")) SpawnNode(TEXT("honey"), S.At + Yaw.RotateVector(FVector(110.f, 0, 0)), TEXT("honey"), 3, 2.f, Day * WRTune(TEXT("forestWorld.honeyRespawnDays"), 1.f));
		else if (S.Kind == TEXT("foxfire_bank")) SpawnNode(TEXT("foxfire_wood"), S.At + Yaw.RotateVector(FVector(0, 160.f, 0)), TEXT("foxfire"), 3, 2.f, Day * WRTune(TEXT("forestWorld.foxfireRespawnDays"), 2.f));
	}
	for (const FWRForestData::FSite& S : D.Sites)
	{
		const FRotator Yaw(0.f, S.Yaw, 0.f);
		if (S.Kind == TEXT("farmstead"))
		{
			// The oat field gone wild (south of the house), the beehive tree's hive, the orchard's one Idunn apple tree.
			for (int32 i = 0; i < 4; ++i) SpawnNode(TEXT("meadow_oats"), S.At + Yaw.RotateVector(FVector(-1400.f + 500.f * (i % 2), 1100.f + 700.f * (i / 2), 0)), TEXT("oats"), 3, 2.f, Day * WRTune(TEXT("forestWorld.cropRespawnDays"), 2.f));
			SpawnNode(TEXT("honey"), S.At + Yaw.RotateVector(FVector(900.f, -1300.f, 0)), TEXT("honey"), 3, 2.f, Day * WRTune(TEXT("forestWorld.honeyRespawnDays"), 1.f));
			SpawnNode(TEXT("idunn_apple"), S.At + Yaw.RotateVector(FVector(1600.f, 900.f, 0)), TEXT("apple"), 3, 2.f, Day * WRTune(TEXT("forestWorld.cropRespawnDays"), 2.f));
		}
		else if (S.Kind == TEXT("sunken_orchard"))
		{
			SpawnNode(TEXT("idunn_apple"), S.At + Yaw.RotateVector(FVector(400.f, 300.f, 0)), TEXT("apple"), 4, 2.f, Day * WRTune(TEXT("forestWorld.cropRespawnDays"), 2.f));
		}
		else if (S.Kind == TEXT("herb_terraces"))
		{
			SpawnNode(TEXT("honey"), S.At + Yaw.RotateVector(FVector(-700.f, -600.f, 0)), TEXT("honey"), 3, 2.f, Day * WRTune(TEXT("forestWorld.honeyRespawnDays"), 1.f));
		}
		else if (S.Kind == TEXT("hernes_ford"))
		{
			// An antler shed in the shallows (Herne's Ford: elk trails cross here) — a new one most days.
			SpawnNode(TEXT("elk_antler"), S.At + Yaw.RotateVector(FVector(300.f, 200.f, 0)), TEXT("antler"), 1, 1.f, Day * WRTune(TEXT("forestWorld.antlerRespawnDays"), 1.f));
		}
		else if (S.Kind == TEXT("frosted_ferns") || S.Kind == TEXT("meadow"))
		{
			SpawnNode(TEXT("meadow_oats"), S.At + Yaw.RotateVector(FVector(600.f, 0, 0)), TEXT("oats"), 2, 2.f, Day * WRTune(TEXT("forestWorld.cropRespawnDays"), 2.f));
		}
	}
	// Thorns at a thornbrake's outer edge (on the side the gap is not), cut with a blade: every wall (the ones across ways,
	// where people stand and look for a way round) and every forestWorld.thornEvery-th giant's ring (600 rings would be
	// 600 replicated nodes; one in eight is still a patch every few hundred metres of old growth).
	const int32 ThornEvery = FMath::Max(1, int32(WRTune(TEXT("forestWorld.thornEvery"), 8.f)));
	for (int32 b = 0; b < D.Brakes.Num(); ++b)
	{
		const FWRForestData::FBrake& B = D.Brakes[b];
		if (B.bRing && B.Guards.IsNone() && b % ThornEvery != 0) continue;
		const FVector Dir = FRotator(0.f, B.Yaw + 180.f, 0.f).Vector();
		const FVector At = B.bRing ? B.Centre + Dir * (B.Radius + B.Depth + 60.f) : B.Centre + FRotator(0.f, B.Yaw, 0.f).Vector() * (B.Length * 0.5f + 80.f);
		SpawnNode(TEXT("thorns"), At, TEXT("thorns"), 3, 2.f, Day * WRTune(TEXT("forestWorld.thornRespawnDays"), 1.f));
	}

	// The Canopy Lift: on the giant nearest its site, 30 m up to the walkways.
	for (const FWRForestData::FSite& S : D.Sites)
	{
		if (S.Kind != TEXT("canopy_lift")) continue;
		const FWRForestData::FTree* Giant = WRForestShape::GiantNear(S.At, 3000.f);
		const FVector Trunk = Giant ? Giant->Base : S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(WRTune(TEXT("forestWorld.lift.ownGiantOffset"), 900.f), 0, 0));
		const float TrunkR = Giant ? Giant->Radius : WRForestShape::TrunkRadius(TEXT("giant"), 9000.f);
		const float R = WRTune(TEXT("forestWorld.lift.wheelRadius"), 110.f);
		FVector Away = (S.At - Trunk).GetSafeNormal2D();
		if (Away.IsNearlyZero()) Away = FRotator(0.f, S.Yaw + 180.f, 0.f).Vector();
		// The basket hangs 2 R + a stone's clearance out from the bark; its local +X looks at the trunk.
		const FVector Foot = Ground(World, Trunk + Away * (TrunkR + 2.f * R + WRTune(TEXT("forestWorld.lift.stoneClear"), 160.f)));
		const FTransform Tr(FRotator(0.f, (-Away).Rotation().Yaw, 0.f), Foot);
		AWRCanopyLift* Lift = World->SpawnActorDeferred<AWRCanopyLift>(AWRCanopyLift::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Lift) continue;
		// Up to the gallery the canopy walkways start from (WRForestShape::LiftTopZ; the scenery builds the gallery there).
		Lift->Rise = WRForestShape::LiftTopZ(Trunk, TrunkR, float(Foot.Z)) - float(Foot.Z);
		Lift->TrunkAt = Trunk;
		Lift->TrunkRadius = TrunkR;
		Lift->FinishSpawning(Tr);
		Lifts.Add(Lift);
	}

	// The newcomer village's safe zone (world-map: the largest settlement; no fighting in its lanes).
	if (!D.Village.IsZero())
	{
		if (AWRSafeZone* Zone = World->SpawnActor<AWRSafeZone>(AWRSafeZone::StaticClass(), FTransform(D.Village), P)) Zone->Radius = D.VillageRadius;
	}
	// Offering stones are known to the server too (BuildScenery made the list on every machine).
	UE_LOG(LogWildreach, Log, TEXT("FOREST server: %d caches, %d nodes, %d lifts, %d offering stones"), Caches.Num(), Nodes.Num(), Lifts.Num(), Offerings.Num());
}

void UWRForestWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World) return;
	if (!bSceneryDone || !bServerDone)
	{
		StepBuild();
		if (!bSceneryDone) return;
	}
	if (World->GetNetMode() != NM_DedicatedServer) StepLights(DeltaTime);
	if (World->GetNetMode() == NM_Client) return;
	StepServer(DeltaTime);
	StepRowan(DeltaTime);
}

void UWRForestWorld::StepLights(float Dt)
{
	// Night lights and night-only glow (lit windows, lanterns, foxfire, glowworms, the crystal roots), the noon rainbow
	// in the Great Falls' spray (11:00-13:00 on a clear day: DC 2.3 L3).
	LightClock -= Dt;
	if (LightClock > 0.f) return;
	LightClock = 1.f;
	UWorld* World = GetWorld();
	const bool bNight = AWRGameState::IsNightIn(World);
	const float Hour = AWRGameState::HourIn(World);
	const FWRWeatherKind* Weather = UWRWeatherSubsystem::ActiveKind(World, WRForestWorldImpl::ForestBiome());
	const bool bClear = Weather == nullptr;
	const bool bNoon = bClear && Hour >= WRTune(TEXT("forestWorld.rainbowFrom"), 11.f) && Hour < WRTune(TEXT("forestWorld.rainbowTo"), 13.f);
	// Overcast closes the Elder Canopy to near-dark by day (DC 2.9; WRSky draws the dark): its foxfire, glowworms and
	// lit windows show as they do at dusk. Only F5's places (by where each stands); the rest of the forest keeps its day.
	const bool bDeepDusk = Weather && Weather->Id == TEXT("overcast");
	const float DuskPower = WRTune(TEXT("forestWorld.overcastLightPower"), 0.8f);
	for (const TWeakObjectPtr<AWRForestScenery>& W : Scenery)
	{
		AWRForestScenery* S = W.Get();
		if (!S) continue;
		const bool bDusk = !bNight && bDeepDusk && WRForest::ZoneAt(World, S->GetActorLocation()) == TCHAR('5');
		const bool bLit = bNight || bDusk;
		for (int32 i = 0; i < S->NightLights.Num(); ++i)
		{
			if (UPointLightComponent* L = S->NightLights[i]) L->SetIntensity(bLit ? (S->NightPower.IsValidIndex(i) ? S->NightPower[i] : 2000.f) * (bDusk ? DuskPower : 1.f) : 0.f);
		}
		if (S->NightRoot && S->NightRoot->IsVisible() != bLit) S->NightRoot->SetVisibility(bLit, true);
		if (S->bRainbow)
		{
			if (bNoon && !S->RainbowFx)
			{
				S->RainbowFx = WRFx::Attach(S->GetRootComponent(), TEXT("forest.rainbow"));
				if (S->RainbowFx) S->RainbowFx->SetRelativeTransform(S->RainbowAt);
			}
			else if (!bNoon && S->RainbowFx)
			{
				WRFx::Stop(S->RainbowFx);
				S->RainbowFx = nullptr;
			}
		}
	}
}

void UWRForestWorld::StepServer(float Dt)
{
	ServerClock -= Dt;
	if (ServerClock > 0.f) return;
	ServerClock = 2.f;
	const double Hours = WRForestWorldImpl::GameHours(this);
	if (!State) State = AWRForestState::Get(this);
	if (State) State->PruneGlows(Hours);
	for (const TWeakObjectPtr<AWRWorldCache>& C : Caches)
	{
		if (C.IsValid() && C->bTaken && C->RefillAtHours > 0.0 && Hours >= C->RefillAtHours) C->Refill();
	}
}

void UWRForestWorld::StepRowan(float Dt)
{
	// Rowan Ashby works at the Grove Circle by day and sleeps in the Hermit's Oak at night (DC 2.11). He goes when no one
	// is watching (no player within forest.rowanUnseen), so nobody sees him blink across the forest; otherwise he waits.
	RowanClock -= Dt;
	if (RowanClock > 0.f) return;
	RowanClock = 5.f;
	UWorld* World = GetWorld();
	const int32 Night = AWRGameState::IsNightIn(World) ? 1 : 0;
	if (Night == RowanNight) return;
	AWRWorldNpc* Rowan = nullptr;
	for (TActorIterator<AWRWorldNpc> It(World); It; ++It) if (It->NpcRole == TEXT("master") && It->NpcId == TEXT("rowan")) Rowan = *It;
	if (!Rowan) return;
	if (RowanNight < 0 && !Night) { RowanNight = 0; return; }   // placed at his day spot already (world.json "at")
	const float Unseen = WRTune(TEXT("forestWorld.rowanUnseen"), 6000.f);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && FVector::Dist(P->GetActorLocation(), Rowan->GetActorLocation()) < Unseen) return;
	}
	const FWRForestData& D = FWRForestData::Get();
	FVector Spot = FVector::ZeroVector;
	FRotator Face = Rowan->WorkFacing;
	if (Night)
	{
		for (const FWRForestData::FSite& S : D.Sites) if (S.Kind == TEXT("hermits_oak")) { Spot = WRForestShape::HermitHouseFloor(S); Face = FRotator(0.f, S.Yaw + 180.f, 0.f); }
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && !Spot.IsZero() && FVector::Dist(P->GetActorLocation(), Spot) < Unseen) return;
		}
	}
	else
	{
		for (const FWRForestData::FSite& S : D.Shrines) if (S.Id == TEXT("grove_circle")) { Spot = WRForestWorldImpl::Earth(World, S.At + FRotator(0.f, S.Yaw, 0.f).RotateVector(FVector(250.f, 520.f, 0))); Face = FRotator(0.f, S.Yaw - 90.f, 0.f); }
	}
	if (Spot.IsZero()) return;
	RowanNight = Night;
	Rowan->SetActorLocation(Spot + FVector(0, 0, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	Rowan->SetActorRotation(Face);
	Rowan->WorkAt = Spot;
	Rowan->WorkFacing = Face;
	UE_LOG(LogWildreach, Log, TEXT("FOREST Rowan goes to %s"), Night ? TEXT("the Hermit's Oak") : TEXT("the Grove Circle"));
}

// ---------------------------------------------------------------- interactions

const UWRForestWorld::FOffering* UWRForestWorld::OfferingNear(const FVector& At, float Reach) const
{
	for (const FOffering& O : Offerings) if (FVector::Dist(O.At, At) < Reach) return &O;
	return nullptr;
}

AWRCanopyLift* UWRForestWorld::LiftNear(const FVector& At) const
{
	for (TActorIterator<AWRCanopyLift> It(GetWorld()); It; ++It) if (It->InReach(At)) return *It;
	return nullptr;
}

bool UWRForestWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const UWRForestWorld* F = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!F || !F->bActive || !Me) return false;
	if (const AWRCanopyLift* Lift = F->LiftNear(Me->GetActorLocation()))
	{
		OutTarget = TEXT("Canopy Lift");
		OutVerb = Lift->IsMoving() ? TEXT("Moving...") : Lift->CurrentPosition() < 0.5f ? TEXT("Haul the stone down (go up)") : TEXT("Let the stone up (go down)");
		return true;
	}
	if (F->OfferingNear(Me->GetActorLocation(), WRTune(TEXT("forestWorld.offeringReach"), 260.f)))
	{
		OutTarget = TEXT("Offering stone");
		OutVerb = TEXT("Leave food");
		return true;
	}
	return false;
}

bool UWRForestWorld::ClientInteract(AWRPlayerController* PC)
{
	const UWRForestWorld* F = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!F || !F->bActive || !Me) return false;
	if (F->LiftNear(Me->GetActorLocation()))
	{
		PC->SendCommand(TEXT("world forest lift"));
		return true;
	}
	if (F->OfferingNear(Me->GetActorLocation(), WRTune(TEXT("forestWorld.offeringReach"), 260.f)))
	{
		PC->SendCommand(TEXT("world forest offer"));
		return true;
	}
	return false;
}

FString UWRForestWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRForestWorld* F = Get(PC);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!F || !F->bActive || !Me) return TEXT("Not here");
	const FString Verb = Args.IsValidIndex(2) ? Args[2] : FString();
	if (Verb == TEXT("lift"))
	{
		// Server reach check with a little slack (the basket moves under you while the command travels).
		AWRCanopyLift* Lift = nullptr;
		for (TActorIterator<AWRCanopyLift> It(PC->GetWorld()); It; ++It) if (It->InReach(Me->GetActorLocation()) || FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < 450.f) Lift = *It;
		return Lift ? Lift->Pull(PC) : TEXT("No lift here");
	}
	if (Verb == TEXT("offer")) return F->Offer(PC);
	return TEXT("?");
}

FString UWRForestWorld::Offer(AWRPlayerController* PC)
{
	// The Offering Stones (DC 2.4 #21): leave food on the carved loaf, once a game day per stone, for +50 Growth.
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Me || !Inv) return TEXT("Not now");
	FOffering* O = const_cast<FOffering*>(OfferingNear(Me->GetActorLocation(), WRTune(TEXT("forestWorld.offeringReach"), 260.f) + 150.f));
	if (!O) return TEXT("No offering stone here");
	const int32 Today = FMath::FloorToInt(WRForestWorldImpl::GameHours(this) / 24.0);
	int32& Last = O->DayBy.FindOrAdd(PS->GetPlayerName(), -1);
	if (Last == Today) return TEXT("You left something here today already");
	// Bread first (the Grovekin left bread), else any food that's in the bag.
	FName Food = NAME_None;
	for (const FName Pref : { FName(TEXT("bread")), FName(TEXT("honey_cake")), FName(TEXT("idunn_pie")), FName(TEXT("berries")) }) if (Inv->CountOf(Pref) > 0) { Food = Pref; break; }
	if (Food.IsNone())
	{
		for (const FWRItemStack& S : Inv->GetSlots())
		{
			const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
			if (Def && Def->Category == TEXT("food")) { Food = S.ItemId; break; }
		}
	}
	if (Food.IsNone()) return TEXT("The stone wants food. You have none");
	Inv->RemoveItem(Food, 1);
	Last = Today;
	WRForest::GrantGrowth(Me, WRTune(TEXT("resources.growth.offeringStone"), 50.f));
	WRFx::Broadcast(Me, TEXT("forest.offering"), O->At, FRotator::ZeroRotator, 1.f, TEXT("forest.offering"));
	return FString::Printf(TEXT("You leave %s on the stone. The forest takes it"), *WRWorld::ItemName(Food));
}

FString UWRForestWorld::UseItem(AWRPlayerState* PS, FName ItemId)
{
	if (ItemId != TEXT("snare_stake")) return FString();
	AWRCharacterBase* Who = PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
	UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (!Who || !Who->IsAlive() || !Bag) return TEXT("Not now");
	UWorld* World = Who->GetWorld();
	// A hunting snare is for the wild: not inside anyone's claim (no traps in bases).
	const FVector Front = Who->GetActorLocation() + Who->GetActorForwardVector() * 120.f;
	if (AWRClaimTotem::FindAt(World, Front)) return TEXT("Not inside a claim: snares are for the wild");
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestSnare), false, Who);
	if (!World->LineTraceSingleByChannel(Hit, Front + FVector(0, 0, 100.f), Front - FVector(0, 0, 300.f), ECC_WorldStatic, Q)) return TEXT("Set it on firm ground");
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRForestSnare* S = World->SpawnActor<AWRForestSnare>(AWRForestSnare::StaticClass(), FTransform(FRotator(0.f, Who->GetActorRotation().Yaw, 0.f), Hit.ImpactPoint), P);
	if (!S) return TEXT("Couldn't set it");
	S->Placer = PS->GetPlayerName();
	S->PlacerBody = Who;
	S->GoneAtHours = WRForestWorldImpl::GameHours(World) + WRTune(TEXT("forestWorld.snare.hours"), 1.f);
	Bag->RemoveItem(ItemId, 1);
	WRSound::Broadcast(Who, TEXT("pickup"));
	return TEXT("Snare set: the first beast to step in it is held");
}

FWROnBlightLanded& UWRForestWorld::OnBlightLanded()
{
	static FWROnBlightLanded D;
	return D;
}

void UWRForestWorld::BlightLanded(UWorld* World, const FVector& At, AActor* Thrower)
{
	if (!World) return;
	// The jar breaks: black spores and green-glowing threads (the boss track decides what it does to what's there).
	WRFx::Broadcast(World, TEXT("forest.blight_splash"), At, FRotator::ZeroRotator, 1.f, TEXT("raid.pot_break"));
	UE_LOG(LogWildreach, Log, TEXT("FOREST blight jar broke at %s"), *At.ToCompactString());
	OnBlightLanded().Broadcast(World, At, Thrower);
}

void UWRForestWorld::SmokePotLanded(UWorld* World, const FVector& At, AActor* Thrower)
{
	if (!World) return;
	// DC 2.19: a 6 m smoke cloud for 10 s (wasps and bees in it go passive; the scout mark comes off: WRForestHazards).
	WRFx::Broadcast(World, TEXT("forest.pot_break"), At, FRotator::ZeroRotator, 1.f, TEXT("raid.pot_break"));
	WRForest::AddSmoke(World, At, WRTune(TEXT("forestWorld.smokePot.radius"), 600.f), WRTune(TEXT("forestWorld.smokePot.seconds"), 10.f));
}
