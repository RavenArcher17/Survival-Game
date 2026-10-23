#include "WRPathComponent.h"
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
		float Value = 1.f;
		FParse::Value(FCommandLine::Get(), TEXT("xpmult="), Value);
		return FMath::Clamp(Value, 0.5f, 10.f);
	}();
	return Mult;
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
	P.XP += FMath::RoundToInt(Amount * XPMultiplier());
	bDirty = true;
	if (FWRPathRules::PointsForXP(P.XP) != PointsBefore)
	{
		UE_LOG(LogWildreach, Log, TEXT("PATH %s earned a point (%d earned, xp %d)"), *PathId.ToString(), FWRPathRules::PointsForXP(P.XP), P.XP);
	}
	// XP alone changes no grants; skip a full re-apply.
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
	for (FName& Slot : QuickSlots)
	{
		if (!Owned.Contains(Slot))
		{
			Slot = NAME_None;
		}
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
	Changed();
	bDirty = false;
}

void UWRPathComponent::Changed()
{
	bDirty = true;
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
	}
	const bool bFireFrost = (MainPath == TEXT("fire") && SubPath == TEXT("frost")) || (MainPath == TEXT("frost") && SubPath == TEXT("fire"));
	const int32 Low = FMath::Min(GetSpent(MainPath), GetSpent(SubPath));
	if (bFireFrost && Low >= 25) GrantedTags.AddTag(WRTags::Combo_Thaw);
	if (bFireFrost && Low >= 50) GrantedTags.AddTag(WRTags::Combo_Balance);
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
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHeatAttribute(),
		WRTune(TEXT("resources.heatMax"), 100.f) + WRTune(TEXT("resources.heatCapacityPerRank"), 15.f) * RankIn(TEXT("fire"), TEXT("heat_capacity")));
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxChillAttribute(),
		WRTune(TEXT("resources.chillMax"), 100.f) + WRTune(TEXT("resources.chillCapacityPerRank"), 15.f) * RankIn(TEXT("frost"), TEXT("chill_capacity")));

	// Tempered: more poise.
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxPoiseAttribute(), WRTune(TEXT("player.poise"), 40.f) + Stat(Character, TEXT("maxPoise")));

	const int32 Level = GetCharacterLevel();
	const float HealthRatio = Attr->GetMaxHealth() > 0.f ? Attr->GetHealth() / Attr->GetMaxHealth() : 1.f;
	const float MaxHealth = FWRPathRules::LevelStat(WRTune(TEXT("player.health"), 100.f), WRTune(TEXT("player.healthAtLevel50"), 250.f), Level);
	const float MaxStamina = FWRPathRules::LevelStat(WRTune(TEXT("player.stamina"), 100.f), WRTune(TEXT("player.staminaAtLevel50"), 150.f), Level);
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
