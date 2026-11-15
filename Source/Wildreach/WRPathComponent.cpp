#include "WRPathComponent.h"
#include "WRWindPath.h"
#include "WRNecroPath.h"
#include "WRArtifacts.h"
#include "WRDeeds.h"
#include "WRSurvival.h"
#include "Wildreach.h"
#include "WRAbilities.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRGameplayTags.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "WRLifeSkills.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRNaturePath.h"
#include "Misc/CommandLine.h"
#include "Net/UnrealNetwork.h"

UWRPathComponent::UWRPathComponent()
{
	SetIsReplicatedByDefault(true);
	QuickSlots.SetNum(NumQuickSlots);
}

void UWRPathComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRPathComponent, MainPath);
	DOREPLIFETIME(UWRPathComponent, SubPath);
	DOREPLIFETIME_CONDITION(UWRPathComponent, Progress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRPathComponent, QuickSlots, COND_OwnerOnly);
	DOREPLIFETIME(UWRPathComponent, BloodSpilled);
	DOREPLIFETIME(UWRPathComponent, BloodWindowEnd);
	DOREPLIFETIME(UWRPathComponent, BloodStealSerial);
	DOREPLIFETIME(UWRPathComponent, BloodFightUntil);
}

UWRPathComponent* UWRPathComponent::FindFor(const AActor* Character)
{
	const APawn* Pawn = Cast<APawn>(Character);
	const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PS ? PS->FindComponentByClass<UWRPathComponent>() : nullptr;
}

namespace
{
	struct FWRNodeEffect
	{
		FName PathId;
		FName NodeId;
		FName Stat;
		float PerRank = 0.f;
	};

	const TArray<FWRNodeEffect>& NodeEffects()
	{
		static TArray<FWRNodeEffect> Effects = []()
		{
			TArray<FWRNodeEffect> Out;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("tuning.json"));
			const TSharedPtr<FJsonObject>* Table = nullptr;
			if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("nodeEffects"), Table))
			{
				return Out;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Table)->Values)
			{
				FString PathStr, NodeStr;
				const TSharedPtr<FJsonObject>* Stats = nullptr;
				if (!Entry.Key.Split(TEXT("."), &PathStr, &NodeStr) || !Entry.Value->TryGetObject(Stats))
				{
					continue;
				}
				for (const TPair<FString, TSharedPtr<FJsonValue>>& S : (*Stats)->Values)
				{
					Out.Add({ FName(*PathStr), FName(*NodeStr), FName(*S.Key), static_cast<float>(S.Value->AsNumber()) });
				}
			}
			return Out;
		}();
		return Effects;
	}
}

float UWRPathComponent::Stat(const AActor* Character, FName StatName)
{
	const UWRPathComponent* Paths = FindFor(Character);
	if (!Paths)
	{
		return 0.f;
	}
	float Total = 0.f;
	for (const FWRNodeEffect& Effect : NodeEffects())
	{
		if (Effect.Stat == StatName && Paths->IsActivePath(Effect.PathId))
		{
			if (const FWRPathProgress* P = Paths->GetProgress(Effect.PathId))
			{
				Total += Effect.PerRank * P->GetRank(Effect.NodeId);
			}
		}
	}
	return Total;
}

float UWRPathComponent::XPMultiplier()
{
	static float Mult = []()
	{
		float Value = WRTune(TEXT("server.xpMultiplier"), 1.f);
		FParse::Value(FCommandLine::Get(), TEXT("xpmult="), Value);
		return FMath::Clamp(Value, 0.5f, 10.f);
	}();
	return Mult;
}

bool UWRPathComponent::HasNode(const AActor* Character, FName PathId, FName NodeId)
{
	const UWRPathComponent* Paths = FindFor(Character);
	const FWRPathProgress* P = Paths && Paths->IsActivePath(PathId) ? Paths->GetProgress(PathId) : nullptr;
	return P && P->GetRank(NodeId) > 0;
}

const FWRPathProgress* UWRPathComponent::GetProgress(FName PathId) const
{
	return Progress.FindByPredicate([PathId](const FWRPathProgress& P) { return P.PathId == PathId; });
}

FWRPathProgress& UWRPathComponent::FindOrAddProgress(FName PathId)
{
	if (FWRPathProgress* Found = Progress.FindByPredicate([PathId](const FWRPathProgress& P) { return P.PathId == PathId; }))
	{
		return *Found;
	}
	FWRPathProgress& New = Progress.AddDefaulted_GetRef();
	New.PathId = PathId;
	return New;
}

int32 UWRPathComponent::GetSpent(FName PathId) const
{
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
	const FWRPathProgress* P = GetProgress(PathId);
	return Def && P ? FWRPathRules::Spent(*Def, *P) : 0;
}

int32 UWRPathComponent::GetEarned(FName PathId) const
{
	const FWRPathProgress* P = GetProgress(PathId);
	return P ? FWRPathRules::PointsForXP(P->XP) : 0;
}

int32 UWRPathComponent::GetTotalSpentActive() const
{
	return GetSpent(MainPath) + GetSpent(SubPath);
}

int32 UWRPathComponent::GetCharacterLevel() const
{
	return FWRPathRules::CharacterLevel(GetTotalSpentActive());
}

FString UWRPathComponent::SetMainPath(FName PathId)
{
	if (!FWRPathRegistry::Get().Find(PathId))
	{
		return TEXT("unknown path");
	}
	if (!MainPath.IsNone())
	{
		return TEXT("main path is chosen at creation and can only change at an altar");
	}
	MainPath = PathId;
	if (SubPath == PathId)
	{
		SubPath = NAME_None;
	}
	FindOrAddProgress(PathId);
	Changed();
	return FString();
}

void UWRPathComponent::DevSetMainPath(FName PathId)
{
	if (!FWRPathRegistry::Get().Find(PathId))
	{
		return;
	}
	MainPath = PathId;
	if (SubPath == PathId)
	{
		SubPath = NAME_None;
	}
	FindOrAddProgress(PathId);
	Changed();
}

FString UWRPathComponent::SetSubPath(FName PathId)
{
	if (!PathId.IsNone() && !FWRPathRegistry::Get().Find(PathId))
	{
		return TEXT("unknown path");
	}
	if (PathId == MainPath)
	{
		return TEXT("subclass must come from a different biome");
	}
	if (!PathId.IsNone())
	{
		// Progress is kept per path; a path you've never used starts at zero.
		const int32 Existing = GetSpent(PathId);
		if (GetSpent(MainPath) + Existing > FMath::RoundToInt(WRTune(TEXT("paths.maxPointsTotal"), 150.f)))
		{
			return TEXT("saved progress would exceed the 150 cap");
		}
		FindOrAddProgress(PathId);
	}
	SubPath = PathId;
	Changed();
	return FString();
}

FString UWRPathComponent::BuyNode(FName PathId, FName NodeId)
{
	if (!IsActivePath(PathId))
	{
		return TEXT("path is not your main path or subclass");
	}
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
	if (!Def)
	{
		return TEXT("unknown path");
	}
	FWRPathProgress& P = FindOrAddProgress(PathId);
	const FName Other = PathId == MainPath ? SubPath : MainPath;
	const FString Why = FWRPathRules::CanBuy(*Def, P, GetSpent(Other), NodeId);
	if (!Why.IsEmpty())
	{
		return Why;
	}
	P.SetRank(NodeId, P.GetRank(NodeId) + 1);
	UE_LOG(LogWildreach, Log, TEXT("PATH bought %s.%s rank %d (spent %d/%d)"), *PathId.ToString(), *NodeId.ToString(),
		P.GetRank(NodeId), FWRPathRules::Spent(*Def, P), FWRPathRules::PointsForXP(P.XP));
	Changed();
	return FString();
}

int32 UWRPathComponent::AutoSpend(FName PathId, int32 Points)
{
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
	if (!Def || !IsActivePath(PathId))
	{
		return 0;
	}
	const int32 Before = GetSpent(PathId);
	bool bBought = true;
	while (bBought && GetSpent(PathId) - Before < Points)
	{
		bBought = false;
		for (const FWRPathNodeDef& Node : Def->Nodes)
		{
			if (GetSpent(PathId) - Before + Node.Cost > Points)
			{
				continue;
			}
			if (BuyNode(PathId, Node.Id).IsEmpty())
			{
				bBought = true;
				break;
			}
		}
	}
	return GetSpent(PathId) - Before;
}

void UWRPathComponent::AwardXP(FName PathId, int32 Amount)
{
	if (!IsActivePath(PathId) || Amount <= 0)
	{
		return;
	}
	FWRPathProgress& P = FindOrAddProgress(PathId);
	const int32 PointsBefore = FWRPathRules::PointsForXP(P.XP);
	// The party's artifacts: +4% an AL (to +60% at AL 15).
	P.XP += FMath::RoundToInt(Amount * XPMultiplier() * WRArtifacts::XPScale(GetOwner()));
	bDirty = true;
	if (FWRPathRules::PointsForXP(P.XP) != PointsBefore)
	{
		UE_LOG(LogWildreach, Log, TEXT("PATH %s earned a point (%d earned, xp %d)"), *PathId.ToString(), FWRPathRules::PointsForXP(P.XP), P.XP);
	}
	// XP alone changes no grants; skip a full re-apply.
}

TArray<FWRNodeRank> UWRPathComponent::ReturnPath(FName PathId)
{
	FWRPathProgress& P = FindOrAddProgress(PathId);
	TArray<FWRNodeRank> Layout;
	for (const FWRNodeRank& R : P.Ranks) if (R.Rank > 0) Layout.Add(R);
	P.Ranks.Reset();
	P.XP = 0;
	UE_LOG(LogWildreach, Log, TEXT("PATH %s returned: %d ranks kept as its layout"), *PathId.ToString(), Layout.Num());
	Changed();   // the starter comes back free; level, health and grants follow
	return Layout;
}

bool UWRPathComponent::UnlearnPath(FName PathId)
{
	if (!IsActivePath(PathId)) return false;
	FWRPathProgress& P = FindOrAddProgress(PathId);
	if (P.Ranks.Num() == 0) return false;
	P.Ranks.Reset();
	UE_LOG(LogWildreach, Log, TEXT("PATH %s unlearned (xp %d kept)"), *PathId.ToString(), P.XP);
	Changed();
	return true;
}

int32 UWRPathComponent::RestoreLayout(FName PathId, const TArray<FWRNodeRank>& Layout)
{
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
	if (!Def || !IsActivePath(PathId)) return 0;
	// Tree order (trunk first, gates respected): keep buying the layout's nodes up to their saved ranks while anything sells.
	int32 Bought = 0;
	bool bAny = true;
	while (bAny)
	{
		bAny = false;
		for (const FWRPathNodeDef& Node : Def->Nodes)
		{
			const FWRNodeRank* Want = Layout.FindByPredicate([&Node](const FWRNodeRank& R) { return R.NodeId == Node.Id; });
			const FWRPathProgress* P = GetProgress(PathId);
			if (!Want || !P || P->GetRank(Node.Id) >= Want->Rank) continue;
			if (BuyNode(PathId, Node.Id).IsEmpty()) { ++Bought; bAny = true; }
		}
	}
	return Bought;
}

void UWRPathComponent::GivePoints(FName PathId, int32 Points)
{
	if (!IsActivePath(PathId))
	{
		return;
	}
	FWRPathProgress& P = FindOrAddProgress(PathId);
	const int32 Have = FWRPathRules::PointsForXP(P.XP);
	P.XP = FMath::Max(P.XP, FWRPathRules::XPForPoints(Have + Points));
	bDirty = true;
}

void UWRPathComponent::RefreshQuickSlots()
{
	// Keep slots that still point at an owned ability; fill gaps in tree order (main path first).
	TArray<FName> Owned;
	for (const FName PathId : { MainPath, SubPath })
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
		const FWRPathProgress* P = GetProgress(PathId);
		if (!Def || !P)
		{
			continue;
		}
		for (const FWRPathNodeDef& Node : Def->Nodes)
		{
			if (Node.Kind != EWRNodeKind::Stat && P->GetRank(Node.Id) > 0 && UWRAbilityRegistry::ClassForNode(PathId, Node.Id))
			{
				Owned.Add(FName(*FString::Printf(TEXT("%s.%s"), *PathId.ToString(), *Node.Id.ToString())));
			}
		}
	}
	// An awakened boss weapon in the hand gives its Soul art a quick slot (WRDeeds): first in the owned list.
	const FName SoulArt = WRDeeds::SoulArtFor(GetOwner());
	if (!SoulArt.IsNone())
	{
		Owned.Insert(SoulArt, 0);
	}
	for (FName& Slot : QuickSlots)
	{
		if (!Owned.Contains(Slot))
		{
			Slot = NAME_None;
		}
	}
	if (!SoulArt.IsNone() && !QuickSlots.Contains(SoulArt) && QuickSlots.IndexOfByKey(NAME_None) == INDEX_NONE && QuickSlots.Num() > 0)
	{
		// Every slot taken: it goes in the last one, and that slot's ability is remembered to come back to it when the
		// weapon leaves the hand.
		SoulDisplaced = QuickSlots.Last();
		QuickSlots.Last() = SoulArt;
	}
	if (SoulArt.IsNone() && !SoulDisplaced.IsNone())
	{
		if (QuickSlots.Num() > 0 && QuickSlots.Last().IsNone() && Owned.Contains(SoulDisplaced) && !QuickSlots.Contains(SoulDisplaced)) QuickSlots.Last() = SoulDisplaced;
		SoulDisplaced = NAME_None;
	}
	for (const FName& Ability : Owned)
	{
		if (QuickSlots.Contains(Ability))
		{
			continue;
		}
		const int32 Free = QuickSlots.IndexOfByKey(NAME_None);
		if (Free == INDEX_NONE)
		{
			break;
		}
		QuickSlots[Free] = Ability;
	}
}

TSharedRef<FJsonObject> UWRPathComponent::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("main"), MainPath.ToString());
	Root->SetStringField(TEXT("sub"), SubPath.IsNone() ? FString() : SubPath.ToString());

	TArray<TSharedPtr<FJsonValue>> PathArray;
	for (const FWRPathProgress& P : Progress)
	{
		TSharedRef<FJsonObject> PathObj = MakeShared<FJsonObject>();
		PathObj->SetStringField(TEXT("id"), P.PathId.ToString());
		PathObj->SetNumberField(TEXT("xp"), P.XP);
		TSharedRef<FJsonObject> Ranks = MakeShared<FJsonObject>();
		for (const FWRNodeRank& R : P.Ranks)
		{
			if (R.Rank > 0)
			{
				Ranks->SetNumberField(R.NodeId.ToString(), R.Rank);
			}
		}
		PathObj->SetObjectField(TEXT("ranks"), Ranks);
		PathArray.Add(MakeShared<FJsonValueObject>(PathObj));
	}
	Root->SetArrayField(TEXT("paths"), PathArray);

	TArray<TSharedPtr<FJsonValue>> SlotArray;
	for (const FName& Slot : QuickSlots)
	{
		SlotArray.Add(MakeShared<FJsonValueString>(Slot.IsNone() ? FString() : Slot.ToString()));
	}
	Root->SetArrayField(TEXT("quickSlots"), SlotArray);
	return Root;
}

void UWRPathComponent::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid())
	{
		return;
	}
	Progress.Reset();
	const TArray<TSharedPtr<FJsonValue>>* PathArray = nullptr;
	if (Json->TryGetArrayField(TEXT("paths"), PathArray))
	{
		for (const TSharedPtr<FJsonValue>& V : *PathArray)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid() || !FWRPathRegistry::Get().Find(FName(*O->GetStringField(TEXT("id")))))
			{
				continue;
			}
			FWRPathProgress& P = Progress.AddDefaulted_GetRef();
			P.PathId = FName(*O->GetStringField(TEXT("id")));
			P.XP = O->GetIntegerField(TEXT("xp"));
			const TSharedPtr<FJsonObject>* Ranks = nullptr;
			if (O->TryGetObjectField(TEXT("ranks"), Ranks))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& R : (*Ranks)->Values)
				{
					P.SetRank(FName(*R.Key), static_cast<int32>(R.Value->AsNumber()));
				}
			}
		}
	}
	const FString Main = Json->GetStringField(TEXT("main"));
	const FString Sub = Json->GetStringField(TEXT("sub"));
	MainPath = FWRPathRegistry::Get().Find(FName(*Main)) ? FName(*Main) : MainPath;
	SubPath = !Sub.IsEmpty() && FWRPathRegistry::Get().Find(FName(*Sub)) ? FName(*Sub) : NAME_None;

	const TArray<TSharedPtr<FJsonValue>>* SlotArray = nullptr;
	if (Json->TryGetArrayField(TEXT("quickSlots"), SlotArray))
	{
		for (int32 i = 0; i < QuickSlots.Num() && i < SlotArray->Num(); ++i)
		{
			const FString S = (*SlotArray)[i]->AsString();
			QuickSlots[i] = S.IsEmpty() ? NAME_None : FName(*S);
		}
	}
	// A save from before starter spells gets them now (and is written back with them).
	const bool bGranted = GrantStarters();
	Changed();
	bDirty = bGranted;
}

bool UWRPathComponent::GrantStarters()
{
	// Every path you take up comes with its starter spell (Data/paths "starter": Fire bolt, Frost shard), free, so it
	// has something to earn path XP with from level 1. The quick slots then put it first.
	bool bGranted = false;
	for (const FName PathId : { MainPath, SubPath })
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(PathId);
		if (Def && FWRPathRules::GrantStarter(*Def, FindOrAddProgress(PathId)))
		{
			bGranted = true;
			const APlayerState* PS = Cast<APlayerState>(GetOwner());
			UE_LOG(LogWildreach, Log, TEXT("PATH starter %s.%s for %s"), *PathId.ToString(), *Def->Starter.ToString(), PS ? *PS->GetPlayerName() : TEXT("?"));
		}
	}
	return bGranted;
}

void UWRPathComponent::Changed()
{
	bDirty = true;
	GrantStarters();
	RefreshQuickSlots();
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (AWRCharacter* Character = PS ? Cast<AWRCharacter>(PS->GetPawn()) : nullptr)
	{
		ApplyToCharacter(Character);
	}
}

void UWRPathComponent::ApplyToCharacter(AWRCharacter* Character)
{
	if (!Character || !Character->HasAuthority())
	{
		return;
	}
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	UWRAttributeSet* Attr = Character->GetAttributes();
	if (!ASC || !Attr)
	{
		return;
	}

	// Abilities: one per quick slot, bound to that slot's input.
	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilities)
	{
		ASC->ClearAbility(Handle);
	}
	GrantedAbilities.Reset();
	for (int32 i = 0; i < QuickSlots.Num(); ++i)
	{
		FString PathStr, NodeStr;
		if (QuickSlots[i].IsNone() || !QuickSlots[i].ToString().Split(TEXT("."), &PathStr, &NodeStr))
		{
			continue;
		}
		if (TSubclassOf<UGameplayAbility> Class = UWRAbilityRegistry::ClassForNode(FName(*PathStr), FName(*NodeStr)))
		{
			const EWRAbilityInput Input = static_cast<EWRAbilityInput>(static_cast<int32>(EWRAbilityInput::Slot1) + i);
			GrantedAbilities.Add(Character->GrantAbility(Class, Input));
		}
	}

	// Tags: root passive of the main path, combos for the main/sub pair.
	for (const FGameplayTag& Tag : GrantedTags)
	{
		ASC->SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::TagOnly);
	}
	GrantedTags.Reset();
	if (MainPath == TEXT("fire"))
	{
		GrantedTags.AddTag(WRTags::Passive_BurningBlood);
		GrantedTags.AddTag(WRTags::Resist_Lava);
	}
	else if (MainPath == TEXT("frost"))
	{
		GrantedTags.AddTag(WRTags::Passive_Permafrost);
		GrantedTags.AddTag(WRTags::Resist_Ice);
	}
	else if (MainPath == TEXT("lightning"))
	{
		// Grounded (cooks/biome-lightning.md 2.18.3): no stun or stagger from lightning; storm strikes deal half.
		GrantedTags.AddTag(WRTags::Passive_Grounded);
	}
	else if (MainPath == TEXT("nature"))
	{
		// Rooted (cooks/biome-forest.md 2.17): standing still slowly heals (WRNaturePath.cpp).
		GrantedTags.AddTag(WRNature::Passive_Rooted);
	}
	else if (MainPath == TEXT("wind"))
	{
		// Featherfall (cooks/biome-sky.md 2.18.3): no fall damage from any height; gliders at half the stamina.
		GrantedTags.AddTag(WRWind::Passive_Featherfall);
	}
	else if (MainPath == TEXT("necromancy"))
	{
		// Deathless (cooks/biome-graveyard.md 2.17): once per Rest a killing blow leaves you at 1 HP (WRNecroPath.cpp).
		GrantedTags.AddTag(WRNecro::Passive_Deathless);
	}
	// Path combos for the main / sub pair (Data/combos.json: Thaw, Balance, Plasma, Meltdown, Brittle shock, Blizzard storm).
	for (const FGameplayTag& Tag : FWRComboRegistry::Get().TagsFor(*this))
	{
		GrantedTags.AddTag(Tag);
	}
	for (const FGameplayTag& Tag : GrantedTags)
	{
		ASC->SetLooseGameplayTagCount(Tag, 1, EGameplayTagReplicationState::TagOnly);
	}

	// Stats: resource capacity nodes, and health/stamina from character level.
	const auto RankIn = [this](FName PathId, FName NodeId)
	{
		const FWRPathProgress* P = IsActivePath(PathId) ? GetProgress(PathId) : nullptr;
		return P ? P->GetRank(NodeId) : 0;
	};
	// Prestige (Deeper marks): that path's resource holds more (WRDeeds).
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHeatAttribute(),
		(WRTune(TEXT("resources.heatMax"), 100.f) + WRTune(TEXT("resources.heatCapacityPerRank"), 15.f) * RankIn(TEXT("fire"), TEXT("heat_capacity"))) * WRDeeds::PrestigeResource(GetOwner(), TEXT("fire")));
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxChillAttribute(),
		(WRTune(TEXT("resources.chillMax"), 100.f) + WRTune(TEXT("resources.chillCapacityPerRank"), 15.f) * RankIn(TEXT("frost"), TEXT("chill_capacity"))) * WRDeeds::PrestigeResource(GetOwner(), TEXT("frost")));
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxChargeAttribute(),
		(WRTune(TEXT("resources.chargeMax"), 100.f) + WRTune(TEXT("resources.chargeCapacityPerRank"), 15.f) * RankIn(TEXT("lightning"), TEXT("charge_capacity"))) * WRDeeds::PrestigeResource(GetOwner(), TEXT("lightning")));
	// Growth (the Nature path): 100 + 10 a rank of Growth capacity (nodeEffects nature.growth_capacity maxGrowth); a
	// bigger bar keeps how full it was.
	{
		const float OldMax = FMath::Max(1.f, Attr->GetMaxGrowth());
		const float Share = FMath::Clamp(Attr->GetGrowth() / OldMax, 0.f, 1.f);
		const float NewMax = (WRTune(TEXT("resources.growth.max"), 100.f) + (IsActivePath(TEXT("nature")) ? FMath::Max(0.f, Stat(Character, TEXT("maxGrowth"))) : 0.f))
			* WRDeeds::PrestigeResource(GetOwner(), TEXT("nature"));
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxGrowthAttribute(), NewMax);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetGrowthAttribute(), NewMax * Share);
	}
	// Momentum (the Wind path): 100 + 15 a rank of Momentum capacity (nodeEffects wind.momentum_capacity maxMomentum).
	{
		const float NewMax = (WRTune(TEXT("resources.momentum.max"), 100.f) + (IsActivePath(TEXT("wind")) ? FMath::Max(0.f, Stat(Character, TEXT("maxMomentum"))) : 0.f))
			* WRDeeds::PrestigeResource(GetOwner(), TEXT("wind"));
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxMomentumAttribute(), NewMax);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetMomentumAttribute(), FMath::Min(Attr->GetMomentum(), NewMax));
	}

	// Tempered: more poise.
	// Reinforced (armorcraft perk) adds to the path nodes' poise.
	// With the worn armour's poise (COMBAT-AND-LESSONS 1.5): one sum, UWRCombatLibrary::RefreshPoise.
	UWRCombatLibrary::RefreshPoise(Character);

	const int32 Level = GetCharacterLevel();
	const float HealthRatio = Attr->GetMaxHealth() > 0.f ? Attr->GetHealth() / Attr->GetMaxHealth() : 1.f;
	// Blood reserve (Blood path): +8 max HP a rank (nodeEffects blood.blood_reserve maxHealth).
	const float MaxHealth = FWRPathRules::LevelStat(WRTune(TEXT("player.health"), 100.f), WRTune(TEXT("player.healthAtLevel50"), 250.f), Level)
		+ FMath::Max(0.f, Stat(Character, TEXT("maxHealth")));
	// Corruption at 75 takes some max stamina while it lasts (WRSurvival).
	const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(GetOwner());
	const float MaxStamina = FWRPathRules::LevelStat(WRTune(TEXT("player.stamina"), 100.f), WRTune(TEXT("player.staminaAtLevel50"), 150.f), Level)
		- (Survival ? Survival->MaxStaminaPenalty() : 0.f);
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
	if (Character->IsAlive())
	{
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), MaxHealth * HealthRatio);
	}

	UE_LOG(LogWildreach, Log, TEXT("PATH apply %s: main=%s(%d) sub=%s(%d) level=%d abilities=%d tags=%s"),
		*Character->GetName(), *MainPath.ToString(), GetSpent(MainPath), *SubPath.ToString(), GetSpent(SubPath), Level,
		GrantedAbilities.Num(), *GrantedTags.ToStringSimple());
}
