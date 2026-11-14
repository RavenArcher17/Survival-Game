// Fishing (see WRFishing.h): a rod in the weapon hand, hold the attack to wind up a cast and let go; the float lands,
// nibbles one to three times, then plunges: strike within the window and the fight starts. Hold to reel; when the fish
// surges, reeling strains the line, so ease off and let it run (the line pays out) until it tires. Land it close in.
// Researched from Animal Crossing (fakes then a deep plunge), Minecraft (a clear dip and splash), Valheim (reeling
// spends stamina against a fish that pulls), Red Dead Redemption 2 (tension snaps a line) and Sea of Thieves (surges
// you counter, no number meters). Server-authoritative; clients draw the float, the line and the HUD.
#include "WRFishing.h"
#include "WRFarm.h"
#include "WRSurvival.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPadInput.h"
#include "WRPlayerController.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "WRWater.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

// ---------------------------------------------------------------- the fish table

FWRFishRegistry& FWRFishRegistry::Get()
{
	static FWRFishRegistry Instance;
	return Instance;
}

FWRFishRegistry::FWRFishRegistry()
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("fish.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("fish"), List))
	{
		UE_LOG(LogWildreach, Error, TEXT("fish.json missing or broken: nothing bites"));
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FWRFishDef D;
		D.Id = FName(*O->GetStringField(TEXT("id")));
		D.Name = O->GetStringField(TEXT("name"));
		FString S;
		D.Item = O->TryGetStringField(TEXT("item"), S) ? FName(*S) : D.Id;
		D.Biome = O->TryGetStringField(TEXT("biome"), S) ? FName(*S) : FName(TEXT("any"));
		D.Water = O->TryGetStringField(TEXT("water"), S) ? FName(*S) : FName(TEXT("open"));
		D.Time = O->TryGetStringField(TEXT("time"), S) ? FName(*S) : FName(TEXT("any"));
		D.Rarity = O->TryGetStringField(TEXT("rarity"), S) ? FName(*S) : FName(TEXT("common"));
		double N = 0.0;
		if (O->TryGetNumberField(TEXT("kgMin"), N)) D.KgMin = N;
		if (O->TryGetNumberField(TEXT("kgMax"), N)) D.KgMax = N;
		if (O->TryGetNumberField(TEXT("fight"), N)) D.Fight = FMath::Clamp(static_cast<float>(N), 0.f, 1.f);
		Fish.Add(D);
	}
	const TArray<TSharedPtr<FJsonValue>>* Treasure = nullptr;
	if (Root->TryGetArrayField(TEXT("treasure"), Treasure))
	{
		for (const TSharedPtr<FJsonValue>& V : *Treasure)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			TreasureTable.Add(MakeTuple(FName(*O->GetStringField(TEXT("item"))), static_cast<int32>(O->GetNumberField(TEXT("min"))),
				static_cast<int32>(O->GetNumberField(TEXT("max"))), static_cast<float>(O->GetNumberField(TEXT("weight")))));
		}
	}
	// Biome baits (Bait maker) and Fishing XP by rarity (cooks/build-farming.md 2.7).
	const TSharedPtr<FJsonObject>* LureObj = nullptr;
	if (Root->TryGetObjectField(TEXT("lures"), LureObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*LureObj)->Values) Lures.Add(FName(*L.Key), FName(*L.Value->AsString()));
	}
	const TSharedPtr<FJsonObject>* XpObj = nullptr;
	if (Root->TryGetObjectField(TEXT("xp"), XpObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& X : (*XpObj)->Values) Xp.Add(FName(*X.Key), float(X.Value->AsNumber()));
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d fish, %d treasures, %d biome baits"), Fish.Num(), TreasureTable.Num(), Lures.Num());
}

const FWRFishDef* FWRFishRegistry::Find(FName Id) const
{
	return Fish.FindByPredicate([Id](const FWRFishDef& D) { return D.Id == Id; });
}

const FWRFishDef* FWRFishRegistry::Pick(FName Biome, FName Water, bool bNight, float RareBoost, bool bLegend, float Random01) const
{
	const FWRTuning& T = FWRTuning::Get();
	TArray<TPair<const FWRFishDef*, float>> Pool;
	float Total = 0.f;
	for (const FWRFishDef& D : Fish)
	{
		const bool bLegendary = D.Rarity == TEXT("legendary");
		if ((D.Biome != TEXT("any") && D.Biome != Biome) || D.Water != Water || (bLegendary && !bLegend)
			|| (D.Time == TEXT("night") && !bNight) || (D.Time == TEXT("day") && bNight))
		{
			continue;
		}
		float W = T.Num(*FString::Printf(TEXT("fishing.rarityWeight.%s"), *D.Rarity.ToString()), D.Rarity == TEXT("common") ? 60.f : D.Rarity == TEXT("uncommon") ? 25.f : D.Rarity == TEXT("rare") ? 8.f : 1.5f);
		if (D.Rarity == TEXT("rare") || bLegendary) W *= 1.f + RareBoost;
		else if (D.Rarity == TEXT("uncommon")) W *= 1.f + RareBoost * 0.5f;
		Pool.Add({ &D, W });
		Total += W;
	}
	float Roll = Random01 * Total;
	for (const TPair<const FWRFishDef*, float>& P : Pool)
	{
		Roll -= P.Value;
		if (Roll <= 0.f) return P.Key;
	}
	return Pool.Num() > 0 ? Pool.Last().Key : nullptr;
}

float FWRFishRegistry::XpFor(FName Rarity, bool bFirstLegend) const
{
	if (bFirstLegend) return Xp.FindRef(TEXT("legendaryFirst")) > 0.f ? Xp.FindRef(TEXT("legendaryFirst")) : 500.f;
	const float* X = Xp.Find(Rarity);
	return X ? *X : 15.f;
}

TPair<FName, int32> FWRFishRegistry::Treasure(float Random01) const
{
	float Total = 0.f;
	for (const auto& Row : TreasureTable) Total += Row.Get<3>();
	float Roll = Random01 * Total;
	for (const auto& Row : TreasureTable)
	{
		Roll -= Row.Get<3>();
		if (Roll <= 0.f) return { Row.Get<0>(), FMath::RandRange(Row.Get<1>(), FMath::Max(Row.Get<1>(), Row.Get<2>())) };
	}
	return { NAME_None, 0 };
}

// ---------------------------------------------------------------- the float

AWRBobber::AWRBobber()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);   // FloatAt carries it; clients ease toward it
	SetNetUpdateFrequency(30.f);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AWRBobber::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRBobber, Angler);
	DOREPLIFETIME(AWRBobber, State);
	DOREPLIFETIME(AWRBobber, FloatAt);
	DOREPLIFETIME(AWRBobber, CastFromAt);
	DOREPLIFETIME(AWRBobber, CastSeconds);
	DOREPLIFETIME(AWRBobber, NibbleSeq);
	DOREPLIFETIME(AWRBobber, BiteSeq);
	DOREPLIFETIME(AWRBobber, bSurge);
	DOREPLIFETIME(AWRBobber, bReelingRep);
	DOREPLIFETIME_CONDITION(AWRBobber, Tension, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AWRBobber, LineCm, COND_OwnerOnly);
}

AWRBobber* AWRBobber::FindFor(const AActor* Who)
{
	if (!Who || !Who->GetWorld()) return nullptr;
	for (TActorIterator<AWRBobber> It(Who->GetWorld()); It; ++It)
	{
		if (It->Angler == Who && It->State != EWRFishState::Done) return *It;
	}
	return nullptr;
}

void AWRBobber::Notify(const FString& Text) const
{
	if (AWRPlayerController* PC = Angler ? Cast<AWRPlayerController>(Angler->GetController()) : nullptr)
	{
		PC->ClientNotify(Text);
	}
}

AWRBobber* AWRBobber::CastFrom(AWRCharacter* Who, float Charge)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	UWRWaterSubsystem* Water = World ? UWRWaterSubsystem::Get(World) : nullptr;
	AWRPlayerController* PC = Who ? Cast<AWRPlayerController>(Who->GetController()) : nullptr;
	if (!World || !Who->HasAuthority() || !Who->IsAlive())
	{
		return nullptr;
	}
	const FWRTuning& T = FWRTuning::Get();
	const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
	const FWRItemStack* Rod = Inv ? &Inv->GetEquipped(EWREquipSlot::Weapon) : nullptr;
	if (Rod && !Rod->IsEmpty() && Rod->Durability <= 0.f)
	{
		if (PC) PC->ClientNotify(TEXT("Your rod is broken: repair it at a workbench"));
		return nullptr;
	}
	if (Who->GetCharacterMovement()->IsSwimming() || Who->GetCharacterMovement()->IsFalling())
	{
		if (PC) PC->ClientNotify(TEXT("Stand on the shore or in the shallows to cast"));
		return nullptr;
	}
	// Out along where you face, Charge of the way from the shortest cast to the longest.
	const FVector Dir = FRotator(0.f, Who->GetBaseAimRotation().Yaw, 0.f).Vector();
	const float Dist = FMath::Lerp(T.Num(TEXT("fishing.castMin"), 400.f), T.Num(TEXT("fishing.castMax"), 1800.f), FMath::Clamp(Charge, 0.f, 1.f));
	FVector Land = Who->GetActorLocation() + Dir * Dist;
	if (!Water || !Water->HasWater() || Water->DepthAt(Land) < T.Num(TEXT("fishing.minDepth"), 60.f))
	{
		if (PC) PC->ClientNotify(TEXT("No water there: cast into the sea, a river or a lake"));
		WRSound::Broadcast(Who, TEXT("swing.light"));
		return nullptr;
	}
	Land.Z = Water->GetSurfaceZ();
	FActorSpawnParameters Params;
	Params.Owner = Who;   // the angler's connection gets the tension and the line's length
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRBobber* B = World->SpawnActor<AWRBobber>(AWRBobber::StaticClass(), FTransform(Land), Params);
	if (!B) return nullptr;
	B->Angler = Who;
	B->State = EWRFishState::Flying;
	B->CastFromAt = Who->RodTipLocation();
	B->FloatAt = Land;
	B->LandAt = Land;
	B->CastDir = Dir;
	B->AnglerSpot = Who->GetActorLocation();
	B->StartedAt = World->GetTimeSeconds();
	B->CastSeconds = FMath::Lerp(0.45f, 0.9f, Charge);
	// Bait in the bag makes bites come sooner; a lure for this biome draws the rare ones (it's used up at the bite).
	bool bAnyBait = Inv && Inv->CountOf(TEXT("cut_bait")) > 0;
	for (const FWRItemStack& S : Inv ? Inv->GetSlots() : TArray<FWRItemStack>()) bAnyBait |= !S.IsEmpty() && FWRFishRegistry::Get().IsLure(S.ItemId);
	B->bBaited = bAnyBait;
	const FName LureHere = FWRFishRegistry::Get().LureFor(B->BiomeHere());
	B->Lure = Inv && Inv->CountOf(LureHere) > 0 ? LureHere : NAME_None;
	WRSound::Broadcast(Who, TEXT("fish.cast"));
	Who->MulticastBodyOneShot(TEXT("throw"));
	UE_LOG(LogWildreach, Log, TEXT("FISH %s cast %.0f cm into %s %s water (%.0f cm deep)%s"), *Who->GetName(), Dist, *B->BiomeHere().ToString(), *B->WaterHere().ToString(),
		Water->DepthAt(Land), B->bBaited ? TEXT(", baited") : TEXT(""));
	return B;
}

FName AWRBobber::BiomeHere() const
{
	return WRBiome::At(this, GetActorLocation());
}

FName AWRBobber::WaterHere() const
{
	const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(this);
	const float Depth = Water ? Water->DepthAt(GetActorLocation()) : 0.f;
	if (Depth >= WRTune(TEXT("fishing.deepDepth"), 700.f) && UWRLifeSkillsComponent::StatFor(Angler, TEXT("deepLine")) > 0.f) return TEXT("deep");
	return Depth >= WRTune(TEXT("fishing.openDepth"), 250.f) ? FName(TEXT("open")) : FName(TEXT("shallow"));
}

const FWRFishDef* AWRBobber::ChooseFish() const
{
	const float Rare = UWRLifeSkillsComponent::StatFor(Angler, TEXT("rareFish")) + (Lure.IsNone() ? 0.f : WRTune(TEXT("fishing.lureRareBoost"), 2.f));
	// A legend bites only with Legend catch and this biome's bait on, once a real day each (cooks/build-farming.md 2.7).
	const bool bLegend = UWRLifeSkillsComponent::StatFor(Angler, TEXT("legendCatch")) > 0.f && !Lure.IsNone();
	const FWRFishDef* Got = FWRFishRegistry::Get().Pick(BiomeHere(), WaterHere(), AWRGameState::IsNightIn(this), Rare, bLegend, FMath::FRand());
	if (Got && Got->Rarity == TEXT("legendary") && !FWRFarm::LegendAllowed(Angler ? Angler->GetPlayerState() : nullptr, Got->Id))
	{
		Got = FWRFishRegistry::Get().Pick(BiomeHere(), WaterHere(), AWRGameState::IsNightIn(this), Rare, false, FMath::FRand());
	}
	return Got;
}

bool AWRBobber::ConsumeBait()
{
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Angler);
	if (!Inv) return false;
	for (const FName Bait : { Lure, FName(TEXT("cut_bait")) })
	{
		if (!Bait.IsNone() && Inv->CountOf(Bait) > 0)
		{
			Inv->RemoveItem(Bait, 1);
			return true;
		}
	}
	// Any other biome's lure still works as plain bait.
	for (const FWRItemStack& S : Inv->GetSlots())
	{
		if (!S.IsEmpty() && FWRFishRegistry::Get().IsLure(S.ItemId))
		{
			Inv->RemoveItem(S.ItemId, 1);
			return true;
		}
	}
	return false;
}

void AWRBobber::Press()
{
	if (!HasAuthority()) return;
	switch (State)
	{
	case EWRFishState::Flying:
	case EWRFishState::Waiting:
		Finish(TEXT("Reeled in"));
		break;
	case EWRFishState::Bite:
	{
		// Struck in time: the fight is on.
		State = EWRFishState::Hooked;
		LineCm = FVector::Dist2D(Angler->GetActorLocation(), FloatAt);
		TensionF = 0.25f;
		bReel = true;
		bSurge = false;
		NextAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.5f, 1.2f);
		FishKg = FMath::Lerp(Fish->KgMin, Fish->KgMax, FMath::Square(FMath::FRand()));   // big ones are rarer
		const float Size = Fish->KgMax > Fish->KgMin ? (FishKg - Fish->KgMin) / (Fish->KgMax - Fish->KgMin) : 0.5f;
		FishFight = FMath::Clamp(Fish->Fight * (0.75f + 0.5f * Size), 0.05f, 1.f);
		WRFx::Broadcast(this, TEXT("fish.splash"), FloatAt, FRotator::ZeroRotator, 0.8f, TEXT("fish.hook"));
		UE_LOG(LogWildreach, Log, TEXT("FISH %s hooked a %s (%.1f kg, fight %.2f)"), *Angler->GetName(), *Fish->Name, FishKg, FishFight);
		break;
	}
	case EWRFishState::Hooked:
		bReel = true;
		break;
	default:
		break;
	}
}

void AWRBobber::Release()
{
	if (HasAuthority() && State == EWRFishState::Hooked) bReel = false;
}

void AWRBobber::Finish(const FString& Why)
{
	if (!HasAuthority() || State == EWRFishState::Done) return;
	State = EWRFishState::Done;
	bSurge = false;
	bReelingRep = false;
	DoneAt = GetWorld()->GetTimeSeconds();
	if (!Why.IsEmpty()) Notify(Why);
}

void AWRBobber::Land()
{
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Angler);
	if (!Inv || !Fish)
	{
		Finish(FString());
		return;
	}
	TArray<FWRItemStack> Got;
	FString Card;
	// Treasure hook: now and then the line brings up something lost instead.
	if (FMath::FRand() < UWRLifeSkillsComponent::StatFor(Angler, TEXT("treasureHook")))
	{
		const TPair<FName, int32> Find = FWRFishRegistry::Get().Treasure(FMath::FRand());
		const FWRItemDef* Def = Find.Key.IsNone() ? nullptr : FWRItemRegistry::Get().Find(Find.Key);
		if (Def)
		{
			FWRItemStack& S = Got.AddDefaulted_GetRef();
			S.ItemId = Find.Key;
			S.Count = Find.Value;
			Card = FString::Printf(TEXT("Hooked up something lost: %d %s"), Find.Value, *Def->Name);
		}
	}
	if (Got.Num() == 0)
	{
		// Net caster: sometimes two come up together.
		const int32 Count = FMath::FRand() < UWRLifeSkillsComponent::StatFor(Angler, TEXT("doubleCatch")) ? 2 : 1;
		FWRItemStack& S = Got.AddDefaulted_GetRef();
		S.ItemId = Fish->Item;
		S.Count = Count;
		const bool bLegend = Fish->Rarity == TEXT("legendary");
		Card = bLegend ? FString::Printf(TEXT("Caught %s, a legend of these waters · %.1f kg"), *Fish->Name, FishKg)
			: FString::Printf(TEXT("Caught %s %s · %.1f kg%s"), Fish->Rarity == TEXT("rare") ? TEXT("a rare") : TEXT("a"), *Fish->Name, FishKg, Count > 1 ? TEXT(" (and another!)") : TEXT(""));
	}
	TArray<FWRItemStack> Over;
	for (const FWRItemStack& S : Got)
	{
		const int32 Left = Inv->AddItem(S.ItemId, S.Count);
		if (Left > 0) { FWRItemStack& O = Over.AddDefaulted_GetRef(); O.ItemId = S.ItemId; O.Count = Left; }
	}
	if (Over.Num() > 0)
	{
		const APlayerState* PS = Angler->GetPlayerState();
		AWRLootBag::Spill(GetWorld(), Angler->GetActorLocation() + Angler->GetActorForwardVector() * 60.f, PS ? PS->GetPlayerName() : FString(), Over);
		Card += TEXT(" (your bag is full: it's on the ground)");
	}
	// Fishing XP by rarity (fish.json xp: common 20, rare 60, a legend 500 the first time, 100 after). A legend is
	// once a real day each, and its first catch gives its trophy (mount it at home: the fish trophy piece).
	bool bFirstLegend = false;
	if (Fish->Rarity == TEXT("legendary") && Got.Num() > 0 && Got[0].ItemId == Fish->Item)
	{
		FWRFarm::LegendCaught(Angler->GetPlayerState(), Fish->Id, bFirstLegend);
		if (bFirstLegend)
		{
			const FName Trophy = FWRFishRegistry::TrophyItem(Fish->Id);
			if (FWRItemRegistry::Get().Find(Trophy) && Inv->AddItem(Trophy, 1) == 0) Card += TEXT(". Its trophy is yours: mount it at home");
		}
		WRSound::Broadcast(Angler, TEXT("fish.legend"));
	}
	const float Xp = FWRFishRegistry::Get().XpFor(Fish->Rarity, bFirstLegend);
	UWRLifeSkillsComponent::Award(Angler, TEXT("fishing"), TEXT("fishCatch"), Xp);   // (no tuning row of that name: the fish.json number)
	Inv->WearEquipped(EWREquipSlot::Weapon, WRTune(TEXT("fishing.wearPerCatch"), 1.f));
	WRFx::Broadcast(this, TEXT("fish.splash"), FloatAt, FRotator::ZeroRotator, 1.2f, TEXT("fish.land"));
	if (Fish->Rarity == TEXT("legendary")) WRFx::Broadcast(Angler, TEXT("discover"), Angler->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("discover"));
	Notify(Card);
	UE_LOG(LogWildreach, Log, TEXT("FISH %s landed: %s"), *Angler->GetName(), *Card);
	Finish(FString());
}

void AWRBobber::Snap()
{
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Angler);
	if (Inv) Inv->WearEquipped(EWREquipSlot::Weapon, WRTune(TEXT("fishing.wearPerSnap"), 3.f));
	WRSound::Broadcast(Angler, TEXT("fish.snap"));
	UE_LOG(LogWildreach, Log, TEXT("FISH %s's line snapped on a %s"), *Angler->GetName(), Fish ? *Fish->Name : TEXT("fish"));
	Finish(FString::Printf(TEXT("The line snapped: the %s got away. Ease off when it runs"), Fish ? *Fish->Name.ToLower() : TEXT("fish")));
}

void AWRBobber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) ServerTick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer) ClientTick(DeltaSeconds);
}

void AWRBobber::ServerTick(float Dt)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const FWRTuning& T = FWRTuning::Get();
	if (State == EWRFishState::Done)
	{
		if (Now - DoneAt > 0.4) Destroy();
		return;
	}
	// The rod put away, the angler gone, in the water or walked off: the line comes in.
	const FWRToolDef* Held = Angler ? FWRGathering::FindTool(Angler->GetMeleeItem()) : nullptr;
	if (!Angler || !Angler->IsAlive())
	{
		Finish(FString());
		return;
	}
	if (!Held || Held->Kind != TEXT("rod")) { Finish(TEXT("You put the rod away: the line's reeled in")); return; }
	if (Angler->GetCharacterMovement()->IsSwimming()) { Finish(TEXT("You went in: the line's reeled in")); return; }
	if (FVector::Dist2D(Angler->GetActorLocation(), AnglerSpot) > T.Num(TEXT("fishing.leashCm"), 350.f)) { Finish(TEXT("You walked off: the line's reeled in")); return; }

	switch (State)
	{
	case EWRFishState::Flying:
		if (Now - StartedAt >= CastSeconds)
		{
			State = EWRFishState::Waiting;
			WRFx::Broadcast(this, TEXT("fish.ripple"), FloatAt, FRotator::ZeroRotator, 1.f, TEXT("fish.plop"));
			// When it bites: sooner with bait and with the fishing skill; one to three nibbles before.
			const float Delay = FMath::FRandRange(T.Num(TEXT("fishing.biteMin"), 6.f), T.Num(TEXT("fishing.biteMax"), 20.f))
				/ (1.f + UWRLifeSkillsComponent::StatFor(Angler, TEXT("biteSpeed"))) * (bBaited ? T.Num(TEXT("fishing.baitBiteFactor"), 0.65f) : 1.f);
			NextAt = Now + Delay;
			Nibbles.Reset();
			for (int32 k = FMath::RandRange(1, 3); k > 0; --k) Nibbles.Add(Now + Delay * FMath::FRandRange(0.25f, 0.85f));
			Nibbles.Sort();
		}
		break;
	case EWRFishState::Waiting:
		while (Nibbles.Num() > 0 && Now >= Nibbles[0])
		{
			Nibbles.RemoveAt(0);
			++NibbleSeq;
			if (GetNetMode() != NM_DedicatedServer) OnRep_Nibble();
		}
		if (Now >= NextAt)
		{
			Fish = ChooseFish();
			if (!Fish)
			{
				Finish(TEXT("Nothing bites here: try other water"));
				break;
			}
			if (bBaited) bBaited = ConsumeBait();   // the fish takes the bait whether you strike in time or not
			State = EWRFishState::Bite;
			NextAt = Now + T.Num(TEXT("fishing.hookWindow"), 0.9f);
			++BiteSeq;
			if (GetNetMode() != NM_DedicatedServer) OnRep_Bite();
			WRFx::Broadcast(this, TEXT("fish.splash"), FloatAt, FRotator::ZeroRotator, 0.6f, TEXT("fish.bite"));
		}
		break;
	case EWRFishState::Bite:
		if (Now >= NextAt)
		{
			// Too late: it spat the hook. Wait on for another.
			Notify(TEXT("Too slow: it got away. Strike the moment the float goes under"));
			Fish = nullptr;
			State = EWRFishState::Waiting;
			const float Delay = FMath::FRandRange(T.Num(TEXT("fishing.biteMin"), 6.f), T.Num(TEXT("fishing.biteMax"), 20.f)) / (1.f + UWRLifeSkillsComponent::StatFor(Angler, TEXT("biteSpeed")));
			NextAt = Now + Delay;
			Nibbles = { Now + Delay * FMath::FRandRange(0.4f, 0.8f) };
		}
		break;
	case EWRFishState::Hooked:
	{
		// Surges come and go; the stronger the fish, the longer and more often.
		if (!bSurge && Now >= NextAt)
		{
			bSurge = true;
			SurgeEnds = Now + FMath::FRandRange(0.6f, 1.4f) * (0.6f + FishFight);
			Sway = FMath::RandBool() ? 1.f : -1.f;
		}
		else if (bSurge && Now >= SurgeEnds)
		{
			bSurge = false;
			NextAt = Now + FMath::FRandRange(1.5f, 3.f) * (1.3f - FishFight * 0.6f);
		}
		// Reeling spends stamina; with none left you can only let it run.
		UAbilitySystemComponent* ASC = Angler->GetAbilitySystemComponent();
		const UWRAttributeSet* Attr = Angler->GetAttributes();
		bool bReeling = bReel;
		if (bReeling && ASC && Attr)
		{
			if (Attr->GetStamina() < 1.f) bReeling = false;
			else ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), Attr->GetStamina() - T.Num(TEXT("fishing.reelStamina"), 6.f) * Dt);
			if (UWRStatusComponent* St = Angler->FindComponentByClass<UWRStatusComponent>()) St->NotifyStaminaUsed();
		}
		if (bReeling)
		{
			LineCm -= T.Num(TEXT("fishing.reelSpeed"), 260.f) * (bSurge ? 0.2f : 1.f) * Dt;
			TensionF += (bSurge ? T.Num(TEXT("fishing.tensionSurge"), 1.25f) : T.Num(TEXT("fishing.tensionReel"), 0.12f)) * (0.55f + FishFight) * Dt;
		}
		else
		{
			TensionF -= T.Num(TEXT("fishing.tensionRelax"), 0.7f) * Dt;
			if (bSurge) LineCm += T.Num(TEXT("fishing.runSpeed"), 160.f) * (0.5f + FishFight) * Dt;
		}
		bReelingRep = bReeling;
		TensionF = FMath::Clamp(TensionF, 0.f, 1.f);
		Tension = static_cast<uint8>(FMath::RoundToInt(TensionF * 255.f));
		StrainFor = TensionF >= 0.999f ? StrainFor + Dt : 0.f;
		if (StrainFor >= T.Num(TEXT("fishing.snapSeconds"), 0.45f)) { Snap(); break; }
		if (LineCm > T.Num(TEXT("fishing.castMax"), 1800.f) + T.Num(TEXT("fishing.extraLine"), 700.f)) { Finish(FString::Printf(TEXT("The line ran out: the %s got away"), *Fish->Name.ToLower())); break; }
		// Where the float is: along the cast from you, pulled across by the fish.
		const FVector Across = FVector::CrossProduct(CastDir, FVector::UpVector);
		const float SwayCm = (bSurge ? T.Num(TEXT("fishing.surgeSway"), 160.f) : 40.f) * Sway * FMath::Sin(static_cast<float>(Now) * (bSurge ? 3.1f : 1.3f));
		const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(this);
		FVector At = Angler->GetActorLocation() + CastDir * FMath::Max(0.f, LineCm) + Across * SwayCm;
		At.Z = Water ? Water->GetSurfaceZ() : At.Z;
		FloatAt = At;
		SetActorLocation(At);
		// Close in, or dragged into the shallows at your feet: landed.
		if (LineCm <= T.Num(TEXT("fishing.landCm"), 170.f) || (Water && Water->DepthAt(At) < 25.f && LineCm < 600.f))
		{
			Land();
		}
		break;
	}
	default:
		break;
	}
}

void AWRBobber::OnRep_Nibble()
{
	DipAt = GetWorld()->GetTimeSeconds();
	WRSound::PlayAt(this, TEXT("fish.nibble"), GetActorLocation());
}

void AWRBobber::OnRep_Bite()
{
	PlungeAt = GetWorld()->GetTimeSeconds();
	// The angler feels it (the sound and splash come with the server's broadcast).
	if (Angler && Angler->IsLocallyControlled())
	{
		WRInput::Rumble(GetWorld(), WRTune(TEXT("fishing.biteRumble"), 0.7f), 0.3f);
	}
}

void AWRBobber::ClientTick(float Dt)
{
	if (!FloatMesh)
	{
		FloatMesh = NewObject<UProceduralMeshComponent>(this, TEXT("Float"));
		FloatMesh->SetupAttachment(Root);
		FloatMesh->SetUsingAbsoluteLocation(true);
		FloatMesh->SetUsingAbsoluteRotation(true);
		FloatMesh->RegisterComponent();
		FWRToolMesh::Float().ApplyTo(FloatMesh);
		LineMesh = NewObject<UProceduralMeshComponent>(this, TEXT("Line"));
		LineMesh->SetupAttachment(Root);
		LineMesh->SetUsingAbsoluteLocation(true);
		LineMesh->SetUsingAbsoluteRotation(true);
		LineMesh->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		LineMesh->RegisterComponent();
		LineMesh->SetCastShadow(false);
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Tip = Angler ? Angler->RodTipLocation() : FVector(CastFromAt);
	if (!bShownSet)
	{
		Shown = CastFromAt;
		bShownSet = true;
		StartedAt = Now;   // this machine's start of the arc
	}
	// Where the float shows: the cast's arc, then eased toward the server's spot, bobbing on the swell; dips on a
	// nibble, goes right under on the bite, skates half under while hooked; back to the rod tip when it's over.
	FVector Goal = FloatAt;
	if (State == EWRFishState::Flying)
	{
		const float A = FMath::Clamp(static_cast<float>((Now - StartedAt) / FMath::Max(0.1f, CastSeconds)), 0.f, 1.f);
		const FVector From = CastFromAt;
		Goal = FMath::Lerp(From, FVector(FloatAt), A) + FVector(0.f, 0.f, FMath::Sin(PI * A) * FVector::Dist(From, FloatAt) * 0.28f);
		Shown = Goal;
	}
	else if (State == EWRFishState::Done)
	{
		Shown = FMath::VInterpTo(Shown, Tip, Dt, 9.f);
	}
	else
	{
		Shown = FMath::VInterpTo(Shown, Goal, Dt, 10.f);
	}
	float Bob = 0.f;
	if (State == EWRFishState::Waiting || State == EWRFishState::Bite)
	{
		Bob = 0.7f * FMath::Sin(static_cast<float>(Now) * 2.1f) + 0.3f * FMath::Sin(static_cast<float>(Now) * 3.7f + 1.f);
		const float Dip = static_cast<float>(Now - DipAt);
		if (Dip < 0.35f) Bob -= 2.6f * FMath::Sin(PI * Dip / 0.35f);
		const float Plunge = static_cast<float>(Now - PlungeAt);
		if (State == EWRFishState::Bite || Plunge < 0.25f) Bob -= 8.f * FMath::Clamp(Plunge / 0.12f, 0.f, 1.f);
	}
	else if (State == EWRFishState::Hooked)
	{
		Bob = bSurge ? -7.f : -4.f + 0.8f * FMath::Sin(static_cast<float>(Now) * 6.f);
	}
	FloatMesh->SetWorldLocation(Shown + FVector(0.f, 0.f, Bob));
	// The reel's ratchet while it turns (heard near the angler).
	if (State == EWRFishState::Hooked && bReelingRep && Angler && Now >= NextRatchet)
	{
		WRSound::PlayAt(this, TEXT("fish.reel"), Angler->GetActorLocation());
		NextRatchet = Now + WRTune(TEXT("fishing.ratchetSeconds"), 0.55f);
	}
	// Leans toward the rod when the line is tight.
	const FVector ToRod = (Tip - Shown).GetSafeNormal2D();
	const float Lean = State == EWRFishState::Hooked ? (bSurge ? 35.f : 20.f) : 4.f * FMath::Sin(static_cast<float>(Now) * 1.7f);
	FloatMesh->SetWorldRotation(FRotator(0.f, ToRod.Rotation().Yaw, 0.f).Quaternion() * FRotator(-Lean, 0.f, 0.f).Quaternion());

	// The line: from the rod tip to the float's quill, sagging when slack, straight when strained.
	const FVector End = Shown + FVector(0.f, 0.f, Bob + 5.f);
	const float Len = FVector::Dist(Tip, End);
	const float Strain = State == EWRFishState::Hooked ? (Angler && Angler->IsLocallyControlled() ? GetTension() : (bSurge ? 0.8f : 0.4f)) : 0.f;
	const float Sag = Len * (State == EWRFishState::Flying ? 0.02f : 0.09f) * (1.f - Strain);
	TArray<FVector> Points;
	for (int32 k = 0; k <= 14; ++k)
	{
		const float A = k / 14.f;
		Points.Add(FMath::Lerp(Tip, End, A) - FVector(0.f, 0.f, Sag * 4.f * A * (1.f - A)));
	}
	FWRToolMesh Line;
	Line.Tube(Points, 0.09f, 3, FLinearColor(0.32f, 0.27f, 0.18f), 0.6f);
	Line.ApplyTo(LineMesh);
	LineMesh->SetVisibility(Len > 5.f);
}

// ---------------------------------------------------------------- HUD

int32 AWRBobber::PaintHud(AWRPlayerController* PC, const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Size, int32 VX, int32 VY)
{
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	const FWRToolDef* Held = Me ? FWRGathering::FindTool(Me->GetMeleeItem()) : nullptr;
	if (!Me || !Held || Held->Kind != TEXT("rod"))
	{
		return Layer;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const auto Text = [&](const FString& S, FSlateFontInfo Font, const FVector2D& CentreTop, const FLinearColor& Colour)
	{
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.015f, 0.01f, 0.9f * Colour.A);
		const FVector2D TS = Measure->Measure(S, Font);
		FSlateDrawElement::MakeText(Out, Layer + 3, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(FVector2D(CentreTop.X - TS.X * 0.5f, CentreTop.Y))), S, Font, ESlateDrawEffect::None, Colour);
	};
	const auto Arc = [&](const FVector2D& C, float R, float From, float To, const FLinearColor& Colour, float Thick)
	{
		TArray<FVector2D> Pts;
		const int32 Steps = FMath::Max(2, FMath::RoundToInt(FMath::Abs(To - From) / 6.f));
		for (int32 k = 0; k <= Steps; ++k)
		{
			const float A = FMath::DegreesToRadians(FMath::Lerp(From, To, k / static_cast<float>(Steps)));
			Pts.Add(C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R);
		}
		FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Colour, true, Thick);
	};
	const FVector2D C = Size * 0.5f;
	const FString Light = WRInput::ActionKey(TEXT("Light")).ToString();
	const FString Block = WRInput::ActionKey(TEXT("Block")).ToString();
	const AWRBobber* B = FindFor(Me);
	const double Now = PC->GetWorld()->GetTimeSeconds();
	if (!B)
	{
		// Winding up a cast: a ring filling round the crosshair, how far it will go.
		const double Since = Me->GetFishChargeSeconds();
		if (Since > 0.0)
		{
			const float Charge = FMath::Clamp(static_cast<float>((Since - 0.1) / WRTune(TEXT("fishing.chargeSeconds"), 1.f)), 0.f, 1.f);
			const float Dist = FMath::Lerp(WRTune(TEXT("fishing.castMin"), 400.f), WRTune(TEXT("fishing.castMax"), 1800.f), Charge) / 100.f;
			Arc(C, 34.f, 0.f, 360.f, FLinearColor(0.f, 0.f, 0.f, 0.45f), 5.f);
			Arc(C, 34.f, 0.f, FMath::Max(2.f, 360.f * Charge), WRUI::Alpha(WRUI::Brass, 0.95f), 3.f);
			Text(FString::Printf(TEXT("Let go to cast · %d m"), FMath::RoundToInt(Dist)), WRUI::Body(13, 700), C + FVector2D(0.f, 44.f), WRUI::Ink);
		}
		else
		{
			Text(FString::Printf(TEXT("Hold %s to cast"), *Light), WRUI::Body(12, 600), C + FVector2D(0.f, 40.f), WRUI::Alpha(WRUI::Ink2, 0.85f));
		}
		return Layer + 3;
	}
	switch (B->GetState())
	{
	case EWRFishState::Flying:
	case EWRFishState::Waiting:
		Text(FString::Printf(TEXT("Wait for the float to go under · %s reels in"), *Light), WRUI::Body(12, 600), C + FVector2D(0.f, 40.f), WRUI::Alpha(WRUI::Ink2, 0.85f));
		break;
	case EWRFishState::Bite:
	{
		const float Pulse = 0.75f + 0.25f * FMath::Sin(static_cast<float>(Now) * 18.f);
		Text(FString::Printf(TEXT("Strike!  %s"), *Light), WRUI::Title(18), C + FVector2D(0.f, 38.f), WRUI::Alpha(WRUI::Warn, Pulse));
		break;
	}
	case EWRFishState::Hooked:
	{
		// The tension arc over the crosshair: pale when slack, ember, then red; it shakes near snapping.
		const float Tn = B->GetTension();
		const FLinearColor Col = Tn < 0.55f ? FMath::Lerp(WRUI::Good, WRUI::Warn, Tn / 0.55f) : FMath::Lerp(WRUI::Warn, WRUI::Bad, (Tn - 0.55f) / 0.45f);
		const FVector2D Shake = Tn > 0.85f ? FVector2D(FMath::Sin(static_cast<float>(Now) * 60.f), FMath::Cos(static_cast<float>(Now) * 53.f)) * 2.f * (Tn - 0.85f) / 0.15f : FVector2D::ZeroVector;
		Arc(C + Shake, 46.f, -70.f, 70.f, FLinearColor(0.f, 0.f, 0.f, 0.5f), 7.f);
		Arc(C + Shake, 46.f, -70.f, -70.f + 140.f * FMath::Max(0.02f, Tn), Col, 4.f);
		Text(FString::Printf(TEXT("%d m"), FMath::RoundToInt(B->GetLineMetres())), WRUI::Caps(12), C + FVector2D(0.f, -72.f), WRUI::Ink);
		Text(B->IsSurging() ? FString(TEXT("It's running: ease off!")) : FString::Printf(TEXT("Hold %s to reel"), *Light), WRUI::Body(13, 800), C + FVector2D(0.f, 40.f),
			B->IsSurging() ? WRUI::Bad : WRUI::Ink);
		Text(FString::Printf(TEXT("%s: let it go"), *Block), WRUI::Body(11, 600), C + FVector2D(0.f, 60.f), WRUI::Alpha(WRUI::Ink3, 0.9f));
		break;
	}
	default:
		break;
	}
	return Layer + 3;
}

// ---------------------------------------------------------------- the angler's side (AWRCharacter)

bool AWRCharacter::IsHoldingRod() const
{
	const FWRToolDef* Tool = FWRGathering::FindTool(MeleeItem);
	return Tool && Tool->Kind == TEXT("rod");
}

double AWRCharacter::GetFishChargeSeconds() const
{
	return FishChargeLocal >= 0.0 && GetWorld() ? GetWorld()->GetTimeSeconds() - FishChargeLocal : 0.0;
}

void AWRCharacter::ServerFishInput_Implementation(bool bDown)
{
	if (!IsAlive())
	{
		return;
	}
	AWRBobber* B = AWRBobber::FindFor(this);
	if (bDown)
	{
		if (B) B->Press();
		else if (IsHoldingRod()) FishDownAt = GetWorld()->GetTimeSeconds();   // winding up a cast
		return;
	}
	if (B)
	{
		B->Release();
	}
	else if (FishDownAt >= 0.0 && IsHoldingRod())
	{
		// Let go: the cast flies as far as it was wound up (timed here, on the server).
		const float Held = static_cast<float>(GetWorld()->GetTimeSeconds() - FishDownAt);
		AWRBobber::CastFrom(this, FMath::Clamp((Held - 0.1f) / WRTune(TEXT("fishing.chargeSeconds"), 1.f), 0.f, 1.f));
	}
	FishDownAt = -1.0;
}

void AWRCharacter::ServerFishCancel_Implementation()
{
	FishDownAt = -1.0;
	if (AWRBobber* B = AWRBobber::FindFor(this))
	{
		B->Finish(B->GetState() == EWRFishState::Hooked ? TEXT("You let it go") : TEXT("Reeled in"));
	}
}
