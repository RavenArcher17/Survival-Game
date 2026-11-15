// The Fungal caverns' live half, part 1 (cooks/biome-fungal.md 2.4, 2.7, 2.20; cooks/build-biome-fungal.md 2.5, 2.7): the
// server's resource nodes (deepcap only on the dead, a blade doubles it; glow-moss, amadou, rhizomorph, silk, saltpetre,
// the galleries' ores, the wild oyster caps and lion's mane with their seeds), the hand sites' caches and the lore
// keepsakes where their stories are, the secrets' pay; and every machine's node looks (WRFungal::NodeLook), modelled in
// code: a deepcap on the bones it grew from, a glow-moss cushion, a hoof fungus on deadwood, a rhizomorph knot, a silk
// bundle, a puffball, a log of oyster caps, a lion's mane on its stump.

#include "WRFungalWorld.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRResourceNode.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "ProceduralMeshComponent.h"

namespace WRFungalThingsImpl
{
	float FtTn(const FString& Key, float Def) { return WRTune(*Key, Def); }
	/** Game days in real seconds (clock.dayMinutes). */
	float FtDays(float Days) { return Days * WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f; }
}

void UWRFungalWorld::SpawnServerActors()
{
	using namespace WRFungalThingsImpl;
	UWorld* World = GetWorld();
	const FWRFungalData& D = FWRFungalData::Get();

	// ---- the gathering (fungal 2.7; numbers in tuning fungalWorld.nodes.<kind>) ----
	struct FKindRow { const TCHAR* Key; const TCHAR* Item; float Hits; float PerHit; float RespawnDays; };
	static const FKindRow Kinds[] = {
		{ TEXT("deepcap"), TEXT("deepcap"), 1, 1, 3.f }, { TEXT("puffball_sac"), TEXT("spore_sac"), 2, 1, 0.5f }, { TEXT("glow_moss"), TEXT("glow_moss"), 1, 2, 0.5f },
		{ TEXT("amadou"), TEXT("amadou"), 2, 1, 1.f }, { TEXT("bootlace_rhizomorph"), TEXT("bootlace_rhizomorph"), 4, 2, 1.f }, { TEXT("webspinner_silk"), TEXT("webspinner_silk"), 2, 2, 1.f },
		{ TEXT("saltpetre"), TEXT("saltpetre"), 4, 3, 0.75f }, { TEXT("copper_ore"), TEXT("copper_ore"), 6, 1, 0.5f }, { TEXT("tin_ore"), TEXT("tin_ore"), 6, 1, 0.5f },
		{ TEXT("iron_ore"), TEXT("iron_ore"), 6, 1, 0.5f }, { TEXT("gem"), TEXT("gem_ruby"), 3, 1, 2.f }, { TEXT("oyster_cap"), TEXT("oyster_cap"), 1, 3, 0.5f },
		{ TEXT("lions_mane"), TEXT("lions_mane"), 1, 2, 1.f }, { TEXT("clay"), TEXT("clay"), 3, 2, 0.5f } };
	int32 Made = 0;
	for (const FKindRow& K : Kinds)
	{
		const TArray<FWRFungalData::FNode>* List = D.Resources.Find(FName(K.Key));
		if (!List) continue;
		const FString T = FString::Printf(TEXT("fungalWorld.nodes.%s."), K.Key);
		for (const FWRFungalData::FNode& N : *List)
		{
			FName Item(K.Item);
			if (FName(K.Key) == TEXT("gem")) { static const TCHAR* G[] = { TEXT("gem_ruby"), TEXT("gem_topaz"), TEXT("gem_onyx") }; Item = G[FMath::Abs(int32(N.At.X / 100.f) + int32(N.At.Y / 100.f)) % 3]; }
			const FTransform Tr(FRotator(0.f, float((Made * 137) % 360), 0.f), N.At);
			AWRResourceNode* Node = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Node) continue;
			Node->ItemId = Item;
			Node->Hits = FMath::Max(1, int32(FtTn(T + TEXT("hits"), K.Hits)));
			Node->PerHit = FMath::Max(1, FMath::RoundToInt(FtTn(T + TEXT("perHit"), K.PerHit)));
			Node->RespawnSeconds = FtDays(FtTn(T + TEXT("respawnDays"), K.RespawnDays));
			Node->GemChance = FtTn(T + TEXT("gemChance"), 0.04f);
			Node->FinishSpawning(Tr);
			++Made;
			if (Item == TEXT("deepcap"))
			{
				// A blade doubles it (fungal 2.7: cut at the stipe, the cap comes whole); the Sporefall pays x3.
				TWeakObjectPtr<AWRResourceNode> Weak(Node);
				Node->YieldScale = [Weak](AWRCharacterBase* Who)
				{
					float Mult = 1.f;
					if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) if (FWRGathering::IsSharp(Inv->GetEquipped(EWREquipSlot::Weapon))) Mult *= WRTune(TEXT("fungalWorld.deepcapBlade"), 2.f);
					if (const AWRResourceNode* Nd = Weak.Get()) if (const UWRFungalWorld* FW = UWRFungalWorld::Get(Nd)) Mult *= FW->SignatureScale();
					return Mult;
				};
			}
			else if (Item == TEXT("oyster_cap") || Item == TEXT("lions_mane"))
			{
				// The wild crops (fungal 2.7, survival-crafting): their spawn comes from gathering them.
				const FName Seeds(*(Item.ToString() + TEXT("_seeds")));
				Node->OnHarvested = [Seeds](AWRCharacterBase* Who)
				{
					if (FMath::FRand() < WRTune(TEXT("fungalWorld.seedChance"), 0.4f)) if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who)) Inv->AddItem(Seeds, 1);
				};
			}
			else if (Item == TEXT("spore_sac"))
			{
				// A wild puffball's sac: the Sporefall doubles them (fungal 2.10 pays).
				TWeakObjectPtr<AWRResourceNode> Weak(Node);
				Node->YieldScale = [Weak](AWRCharacterBase* Who)
				{
					const AWRResourceNode* Nd = Weak.Get();
					const UWRFungalWorld* FW = Nd ? UWRFungalWorld::Get(Nd) : nullptr;
					return FW && FW->bSporefall ? WRTune(TEXT("fungalEvents.sporefall.sacMult"), 2.f) : 1.f;
				};
			}
			Nodes.Add(Node);
		}
	}

	// ---- the hand sites' caches and the lore keepsakes (fungal 2.4, 2.20): what each place pays, where its story is.
	const auto Cache = [&](const FVector& At, const FString& Title, const TMap<FName, int32>& Items, const FString& Lore, float RefillHours) -> AWRWorldCache*
	{
		const FTransform Tr(FRotator::ZeroRotator, At);
		AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) return nullptr;
		C->Kind = TEXT("chest");
		C->Biome = WRFungal::Biome();
		C->Title = Title;
		C->Items = Items;
		C->Lore = Lore;
		C->RefillHours = RefillHours;
		C->FinishSpawning(Tr);
		Caches.Add(C);
		return C;
	};
	const float Refill = FtTn(TEXT("fungalWorld.cacheRefillHours"), 48.f);
	const float LoreRefill = FtTn(TEXT("fungalWorld.loreRefillHours"), 24.f);
	// Where a site's cache stands: its site's floor (a cave) or the ground (the surface), offset along its yaw.
	const auto SiteSpot = [&](FName Site, const FVector& Off) -> FVector
	{
		const FWRFungalData::FSite* S = D.FindSite(Site);
		if (!S) return FVector::ZeroVector;
		FVector At = S->At + FRotator(0.f, S->Yaw, 0.f).RotateVector(Off) + FVector(0.f, 0.f, 40.f);
		if (!S->bCave) { float Z = 0.f; if (WRLand::GroundUnder(At + FVector(0.f, 0.f, 3000.f), Z)) At.Z = Z + 40.f; }
		return At;
	};
	struct FCacheRow { const TCHAR* Site; FVector Off; const TCHAR* Title; TMap<FName, int32> Items; const TCHAR* Lore; bool bLore; };
	const TArray<FCacheRow> Rows = {
		{ TEXT("S1"), FVector(250.f, 450.f, 0.f), TEXT("The terraces' tool chest"), { { TEXT("oyster_cap_seeds"), 4 }, { TEXT("rope"), 2 } }, TEXT("Everything in its place, oiled against the damp. Whoever kept it meant to come back up."), false },
		{ TEXT("S2"), FVector(0.f, 80.f, 0.f), TEXT("Clay at the cairn's foot"), { { TEXT("clay"), 6 } }, TEXT("Fresh clay, pressed into a ball and left at the cairn. Someone was still making idols."), false },
		{ TEXT("S3"), FVector(0.f, -300.f, 0.f), TEXT("A basket of dried caps"), { { TEXT("oyster_cap"), 6 }, { TEXT("amadou"), 3 } }, TEXT("Dried to grey paper on the racks, a winter's food. They didn't need it."), false },
		{ TEXT("S4"), FVector(0.f, 300.f, 0.f), TEXT("Under the granaries"), { { TEXT("rope"), 4 }, { TEXT("bronze_ingot"), 1 } }, TEXT("A bronze measure and the ladders' lashings, laid out neatly beside the towers."), false },
		{ TEXT("S6"), FVector(-120.f, 0.f, 0.f), TEXT("The flyer's pack"), { { TEXT("linen"), 3 }, { TEXT("rope"), 2 } }, TEXT("Still strapped to the bones: a Kitewright's kit for mending sail in the air."), false },
		{ TEXT("C3"), FVector(0.f, 0.f, 0.f), TEXT("A Deepfarmer's cache on the ledge"), { { TEXT("bronze_ingot"), 2 }, { TEXT("glow_gland"), 1 } }, TEXT("Forty metres up the grove's wall, where only the caps could put you."), false },
		{ TEXT("C5"), FVector(380.f, 0.f, 0.f), TEXT("A newcomer's satchel"), { { TEXT("rope"), 2 }, { TEXT("berries"), 4 }, { TEXT("hand_torch"), 1 } }, TEXT("Cut down from the lines. Its owner wasn't with it."), false },
		{ TEXT("C6"), FVector(1600.f, 1200.f, 0.f), TEXT("The caravan's pots"), { { TEXT("sunstone"), 2 }, { TEXT("clay"), 2 } }, TEXT("A Desert caravan that fell through the Sieves: pots of sunstone, a Dromad's bones, a Sunwatch gnomon still true."), false },
		{ TEXT("C10"), FVector(0.f, 0.f, 0.f), TEXT("The larder's leavings"), { { TEXT("webspinner_silk"), 10 }, { TEXT("linen"), 2 } }, TEXT("Silk off the cocoons, and the torn sail of a glider that never landed."), false },
		{ TEXT("C12"), FVector(0.f, 400.f, 0.f), TEXT("The nursery's stores"), { { TEXT("clay"), 6 }, { TEXT("bronze_ingot"), 2 } }, TEXT("Clay for the moulds and bronze for the biers. They meant to make more."), false },
		{ TEXT("C15"), FVector(200.f, 0.f, 0.f), TEXT("Inside the diving bell"), { { TEXT("bronze_ingot"), 6 }, { TEXT("gem_topaz"), 1 } }, TEXT("Under the bell, with the diver's bones: what he went down for, and didn't bring up."), false },
		{ TEXT("C17"), FVector(0.f, 0.f, 0.f), TEXT("A cache on the last ledge"), { { TEXT("iron_ingot"), 2 }, { TEXT("toad_skin"), 2 } }, TEXT("On the far pillar, where only a toad's leap reaches."), false },
		// The lore keepsakes (fungal 2.20): short and wordless; whoever comes later finds another (refills).
		{ TEXT("S1"), FVector(-300.f, -200.f, 0.f), TEXT("Terrace tally"), { { TEXT("lore_terrace_tally"), 1 } }, TEXT(""), true },
		{ TEXT("S2"), FVector(0.f, -60.f, 0.f), TEXT("A knocker idol"), { { TEXT("knocker_idol"), 1 } }, TEXT(""), true },
		{ TEXT("C8"), FVector(0.f, 0.f, 0.f), TEXT("Spore print jar"), { { TEXT("lore_spore_print_jar"), 1 } }, TEXT(""), true },
		{ TEXT("C12"), FVector(1200.f, 0.f, 0.f), TEXT("Titan mould sherd"), { { TEXT("lore_titan_mould_sherd"), 1 } }, TEXT(""), true },
		{ TEXT("C15"), FVector(260.f, 60.f, 0.f), TEXT("Diver's bell"), { { TEXT("lore_divers_bell"), 1 } }, TEXT(""), true },
		{ TEXT("S6"), FVector(-60.f, 40.f, 0.f), TEXT("Kite harness token"), { { TEXT("lore_kite_harness_token"), 1 } }, TEXT(""), true },
		{ TEXT("C18"), FVector(150.f, 280.f, 0.f), TEXT("Champion's kite-token"), { { TEXT("lore_champions_kite_token"), 1 } }, TEXT(""), true },
		{ TEXT("HG4"), FVector(0.f, 0.f, 0.f), TEXT("Elder's bowl"), { { TEXT("lore_elders_bowl"), 1 } }, TEXT(""), true },
	};
	int32 Caches0 = 0;
	for (const FCacheRow& R : Rows)
	{
		FVector At = SiteSpot(FName(R.Site), R.Off);
		if (At.IsZero()) continue;
		// The Elders' bowl is at the tower's top (its stair of shelves).
		if (FName(R.Site) == TEXT("HG4")) { if (const FWRFungalData::FHall* H = D.FindHall(TEXT("HG4"))) At = H->At + FVector(0.f, 0.f, FMath::Min(4000.f, H->H - 600.f) + 70.f); }
		if (Cache(At, R.Title, R.Items, R.Lore, R.bLore ? LoreRefill : Refill)) ++Caches0;
	}
	// The summited niches' caches: whatever they carried up, at the foot of their root.
	for (const FWRFungalData::FKit& K : D.Kit)
	{
		if (K.Kind != TEXT("summited_niche")) continue;
		if (Cache(K.At + FVector(120.f, 0.f, 40.f), TEXT("Beneath the summited"), { { TEXT("rope"), 1 }, { TEXT("deepcap"), 1 }, { TEXT("bronze_ingot"), 1 } }, TEXT("Left at the foot of the climb, set down carefully, as if they'd be back for it."), Refill)) ++Caches0;
	}
	// The secrets' pay (fungal 2.20): the reliquary under the ring's altar, the fifth tunnel's gallery, the dark star's
	// ledge, the trading post behind the Sand Falls. Each is reachable only by its secret's way (WRFungalHazards opens them).
	for (const FWRFungalData::FSecret& S : D.Secrets)
	{
		FVector At = S.At + FVector(0.f, 0.f, 40.f);
		if (S.Id == TEXT("F-S1")) { if (const FWRFungalData::FHall* H = D.FindHall(TEXT("RQ"))) Cache(H->At + FVector(0, 0, 40.f), TEXT("The Deepfarmer reliquary"), { { TEXT("deepfarmer_ring"), 1 }, { TEXT("gem_onyx"), 1 }, { TEXT("bronze_ingot"), 3 } }, TEXT("Under the altar, sealed: a ring in a clay box, the box pressed with a toad's foot."), 0.f); }
		else if (S.Id == TEXT("F-S2")) { if (const FWRFungalData::FHall* H = D.FindHall(TEXT("H7"))) Cache(H->At + FVector(0, 0, 40.f), TEXT("The fifth tunnel's end"), { { TEXT("iron_ore"), 3 }, { TEXT("knocker_idol"), 1 } }, TEXT("A gallery dug and then walled up from the inside. A knocker idol faces the wall."), Refill); }
		else if (S.Id == TEXT("F-S3")) Cache(At, TEXT("A climber's cache under the dark star"), { { TEXT("rope"), 3 }, { TEXT("gem_ruby"), 1 }, { TEXT("star_thread"), 1 } }, TEXT("A dry ledge where no thread hangs: a Webspinner's egg, cold, and a climber's bag."), Refill);
		else if (S.Id == TEXT("F-S4")) Cache(At, TEXT("The trading post behind the falls"), { { TEXT("sunstone"), 2 }, { TEXT("deepcap"), 2 }, { TEXT("lore_trade_token"), 1 } }, TEXT("Both peoples' goods side by side: the Sunwatch's and the Deepfarmers'. They met here."), Refill);
	}
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL server: %d nodes, %d caches"), Made, Caches.Num());
}

float UWRFungalWorld::SignatureScale() const
{
	// The Sporefall's x3 on deepcap (fungal 2.10 pays, signatureScale in world.json events.sporefall).
	return bSporefall ? WRTune(TEXT("fungalEvents.sporefall.deepcapMult"), 3.f) : 1.f;
}

// ================================================================================================ the node looks

bool WRFungal::NodeLook(AActor* Node, FName Item, uint32 Seed)
{
	using namespace WRFungalLook;
	if (!Node || Node->GetNetMode() == NM_DedicatedServer) return false;
	static const TSet<FName> Mine = { TEXT("deepcap"), TEXT("spore_sac"), TEXT("glow_moss"), TEXT("amadou"), TEXT("bootlace_rhizomorph"), TEXT("webspinner_silk"), TEXT("oyster_cap"), TEXT("lions_mane") };
	if (!Mine.Contains(Item)) return false;
	// Only in the Fungal caverns (the forest's rhizomorph keeps its own look).
	if (!WRFungal::Contains(Node->GetActorLocation()) && !WRFungal::InCave(Node->GetActorLocation() + FVector(0, 0, 100.f))) return false;
	FWRToolMesh M, G, V;
	FRandomStream R(int32(Seed & 0x7fffffff));
	const auto Cap = [&](FWRToolMesh& Into, const FVector& Base, float H, float CapR, const FLinearColor& Stipe, const FLinearColor& Top, float Glow)
	{
		M.Lathe(Base - FVector(0, 0, 4.f), FVector::UpVector, H + 4.f, { FVector(0.f, CapR * 0.28f, CapR * 0.28f), FVector(0.5f, CapR * 0.18f, CapR * 0.18f), FVector(1.f, CapR * 0.22f, CapR * 0.22f) }, 10, Stipe, 0.8f);
		Into.Lathe(Base + FVector(0, 0, H - CapR * 0.1f), FVector::UpVector, CapR * 0.5f, { FVector(0.f, CapR, CapR), FVector(0.5f, CapR * 0.75f, CapR * 0.75f), FVector(1.f, 2.f, 2.f) }, 14, Top, Glow);
	};
	if (Item == TEXT("deepcap"))
	{
		// On the dead (fungal 2.7): a skull and a few ribs furred white, one to three deepcaps rising from them, their
		// translucent blue-white caps lit cyan from within.
		const FLinearColor Bone = FMath::Lerp(BoneC(), MycelC(), 0.55f);
		M.Lathe(FVector(30.f, 0.f, 0.f), FVector::UpVector, 22.f, { FVector(0.f, 14.f, 12.f), FVector(0.6f, 15.f, 13.f), FVector(1.f, 6.f, 6.f) }, 10, Bone, 0.85f);
		for (int32 r = 0; r < 4; ++r) M.Tube({ FVector(-20.f + r * 10.f, -16.f, 2.f), FVector(-18.f + r * 10.f, -4.f, 16.f), FVector(-16.f + r * 10.f, 10.f, 4.f) }, 2.4f, 5, Bone, 0.85f);
		M.Lathe(FVector(-10.f, 0.f, -2.f), FVector::UpVector, 8.f, { FVector(0.f, 34.f, 24.f), FVector(1.f, 20.f, 14.f) }, 10, MycelC(), 0.95f);
		const int32 N = 1 + int32(R.FRand() * 2.9f);
		for (int32 c = 0; c < N; ++c)
		{
			const FVector B(-8.f + c * 12.f, R.FRandRange(-8.f, 8.f), 6.f);
			Cap(G, B, 22.f + 10.f * R.FRand(), 9.f + 4.f * R.FRand(), Rgb(236, 244, 248) * 0.6f, Rgb(200, 236, 255), 0.95f);
		}
	}
	else if (Item == TEXT("glow_moss"))
	{
		for (int32 c = 0; c < 5; ++c) G.Lathe(FVector(R.FRandRange(-60.f, 60.f), R.FRandRange(-60.f, 60.f), -6.f), FVector::UpVector, 18.f, { FVector(0.f, 50.f, 40.f), FVector(0.5f, 40.f, 32.f), FVector(1.f, 8.f, 6.f) }, 10, CyanC(), 0.75f);
	}
	else if (Item == TEXT("amadou"))
	{
		// A hoof fungus on a broken stump (the tinder fungus Ötzi carried: grey-brown, banded).
		M.Lathe(FVector(0.f, 0.f, -10.f), FVector::UpVector, 110.f, { FVector(0.f, 34.f, 34.f), FVector(0.9f, 26.f, 26.f), FVector(1.f, 22.f, 22.f) }, 10, Rgb(110, 92, 72) * 0.5f, 0.85f);
		for (int32 h = 0; h < 3; ++h)
		{
			const float A = h * 2.1f;
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
			M.Lathe(D * 24.f + FVector(0.f, 0.f, 40.f + h * 22.f), D, 26.f, { FVector(0.f, 20.f, 14.f), FVector(0.6f, 18.f, 12.f), FVector(1.f, 6.f, 4.f) }, 10, Rgb(150, 130, 104) * 0.55f, 0.9f);
		}
	}
	else if (Item == TEXT("bootlace_rhizomorph"))
	{
		// A knot of black bootlaces coming out of the rock and back into it.
		for (int32 l = 0; l < 4; ++l)
		{
			const float A = l * 1.6f;
			M.Tube({ FVector(FMath::Cos(A) * 60.f, FMath::Sin(A) * 60.f, -10.f), FVector(FMath::Cos(A) * 20.f, FMath::Sin(A) * 20.f, 60.f + l * 10.f), FVector(-FMath::Cos(A) * 40.f, -FMath::Sin(A) * 40.f, 20.f), FVector(-FMath::Cos(A) * 70.f, -FMath::Sin(A) * 70.f, -10.f) }, 7.f, 6, RhizoC(), 0.3f);
		}
	}
	else if (Item == TEXT("webspinner_silk"))
	{
		for (int32 b = 0; b < 3; ++b) V.Lathe(FVector(-30.f + b * 30.f, 0.f, 0.f), FVector::UpVector, 60.f + 20.f * b, { FVector(0.f, 22.f, 22.f), FVector(0.5f, 30.f, 28.f), FVector(1.f, 6.f, 6.f) }, 10, Rgb(226, 226, 218), 0.15f);
	}
	else if (Item == TEXT("spore_sac"))
	{
		// A grounded puffball, tight and creamy, its skin crazed.
		M.Lathe(FVector(0.f, 0.f, -6.f), FVector::UpVector, 70.f, { FVector(0.f, 30.f, 30.f), FVector(0.3f, 46.f, 46.f), FVector(0.7f, 44.f, 44.f), FVector(1.f, 8.f, 8.f) }, 14, Rgb(237, 230, 214) * 0.62f, 0.7f);
		G.Lathe(FVector(0.f, 0.f, 58.f), FVector::UpVector, 4.f, { FVector(0.f, 10.f, 10.f), FVector(1.f, 4.f, 4.f) }, 6, CyanC(), 0.5f);
	}
	else if (Item == TEXT("oyster_cap"))
	{
		M.Lathe(FVector(-50.f, 0.f, 18.f), FVector::ForwardVector, 100.f, { FVector(0.f, 18.f, 18.f), FVector(1.f, 16.f, 16.f) }, 10, TimberC(), 0.9f);
		for (int32 c = 0; c < 5; ++c) M.Lathe(FVector(-35.f + c * 18.f, 17.f, 22.f), FVector(0.f, 1.f, 0.15f), 18.f, { FVector(0.f, 4.f, 4.f), FVector(1.f, 16.f, 12.f) }, 10, OysterC(), 0.75f);
	}
	else if (Item == TEXT("lions_mane"))
	{
		M.Lathe(FVector(0.f, 0.f, -6.f), FVector::UpVector, 60.f, { FVector(0.f, 30.f, 30.f), FVector(1.f, 24.f, 24.f) }, 10, TimberC(), 0.9f);
		for (int32 s = 0; s < 18; ++s) M.Lathe(FVector(R.FRandRange(-18.f, 18.f), 26.f, 40.f + R.FRandRange(-8.f, 8.f)), FVector(0.f, 0.f, -1.f), 18.f + 8.f * R.FRand(), { FVector(0.f, 3.f, 3.f), FVector(1.f, 0.5f, 0.5f) }, 4, Rgb(245, 240, 230) * 0.62f, 0.85f);
	}
	const auto Make = [Node](FWRToolMesh& Mesh, UMaterialInterface* Mat)
	{
		if (Mesh.Verts.Num() == 0) return;
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Node);
		P->SetupAttachment(Node->GetRootComponent());
		P->SetUsingAbsoluteScale(true);
		P->RegisterComponent();
		Mesh.ApplyTo(P);
		if (Mat) P->SetMaterial(0, Mat);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	};
	Make(M, nullptr);
	Make(G, WRFungalLook::GlowMaterial(Node));
	Make(V, WRFungalLook::VeilMaterial(Node));
	return true;
}
