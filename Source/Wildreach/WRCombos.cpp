#include "WRCombos.h"
#include "Wildreach.h"
#include "WRPathComponent.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"

FWRComboRegistry& FWRComboRegistry::Get()
{
	static FWRComboRegistry Instance;
	return Instance;
}

FWRComboRegistry::FWRComboRegistry()
{
	Reload();
}

void FWRComboRegistry::Reload()
{
	Pairs.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("combos.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("pairs"), List))
	{
		UE_LOG(LogWildreach, Warning, TEXT("COMBOS: Data/combos.json missing or has no pairs"));
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Paths = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(TEXT("paths"), Paths) || Paths->Num() != 2)
		{
			continue;
		}
		FWRComboPair& Pair = Pairs.AddDefaulted_GetRef();
		Pair.A = FName(*(*Paths)[0]->AsString());
		Pair.B = FName(*(*Paths)[1]->AsString());
		for (const TPair<const TCHAR*, int32> L : { TPair<const TCHAR*, int32>(TEXT("at25"), 25), TPair<const TCHAR*, int32>(TEXT("at50"), 50) })
		{
			const TSharedPtr<FJsonObject>* LevelObj = nullptr;
			if (!O->TryGetObjectField(L.Key, LevelObj))
			{
				continue;
			}
			FWRComboLevel& Level = Pair.Levels.AddDefaulted_GetRef();
			Level.At = L.Value;
			Level.Id = FName(*(*LevelObj)->GetStringField(TEXT("id")));
			(*LevelObj)->TryGetStringField(TEXT("name"), Level.Name);
			(*LevelObj)->TryGetStringField(TEXT("text"), Level.Text);
			int32 At = Level.At;
			if ((*LevelObj)->TryGetNumberField(TEXT("at"), At)) Level.At = At;
			const TArray<TSharedPtr<FJsonValue>>* Grants = nullptr;
			if ((*LevelObj)->TryGetArrayField(TEXT("grants"), Grants))
			{
				for (const TSharedPtr<FJsonValue>& G : *Grants)
				{
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*G->AsString()), false);
					if (Tag.IsValid()) Level.Grants.Add(Tag);
					else UE_LOG(LogWildreach, Warning, TEXT("COMBOS: %s grants unknown tag %s"), *Level.Id.ToString(), *G->AsString());
				}
			}
			FString Requires;
			FString ReqPath, ReqNode;
			if ((*LevelObj)->TryGetStringField(TEXT("requires"), Requires) && Requires.Split(TEXT("."), &ReqPath, &ReqNode))
			{
				Level.RequiresPath = FName(*ReqPath);
				Level.RequiresNode = FName(*ReqNode);
			}
			const TSharedPtr<FJsonObject>* Numbers = nullptr;
			if ((*LevelObj)->TryGetObjectField(TEXT("numbers"), Numbers))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& N : (*Numbers)->Values)
				{
					double D = 0.0;
					if (N.Value.IsValid() && N.Value->TryGetNumber(D)) Level.Numbers.Add(FName(*N.Key), float(D));
				}
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("COMBOS: %d path pairs loaded"), Pairs.Num());
}

const FWRComboPair* FWRComboRegistry::Find(FName PathA, FName PathB) const
{
	return Pairs.FindByPredicate([PathA, PathB](const FWRComboPair& P) { return (P.A == PathA && P.B == PathB) || (P.A == PathB && P.B == PathA); });
}

const FWRComboLevel* FWRComboRegistry::Level(FName ComboId) const
{
	for (const FWRComboPair& P : Pairs)
	{
		for (const FWRComboLevel& L : P.Levels)
		{
			if (L.Id == ComboId) return &L;
		}
	}
	return nullptr;
}

float FWRComboRegistry::Num(FName ComboId, FName Key, float Fallback) const
{
	const FWRComboLevel* L = Level(ComboId);
	const float* V = L ? L->Numbers.Find(Key) : nullptr;
	return V ? *V : Fallback;
}

TArray<FGameplayTag> FWRComboRegistry::TagsFor(const UWRPathComponent& Paths) const
{
	TArray<FGameplayTag> Out;
	const FName Main = Paths.GetMainPath(), Sub = Paths.GetSubPath();
	const FWRComboPair* Pair = Main.IsNone() || Sub.IsNone() ? nullptr : Find(Main, Sub);
	if (!Pair)
	{
		return Out;
	}
	// Points in both paths (the lower of the two) against each level's threshold.
	const int32 Low = FMath::Min(Paths.GetSpent(Main), Paths.GetSpent(Sub));
	for (const FWRComboLevel& L : Pair->Levels)
	{
		if (Low < L.At)
		{
			continue;
		}
		if (!L.RequiresNode.IsNone())
		{
			const FWRPathProgress* P = Paths.IsActivePath(L.RequiresPath) ? Paths.GetProgress(L.RequiresPath) : nullptr;
			if (!P || P->GetRank(L.RequiresNode) <= 0)
			{
				continue;
			}
		}
		Out.Append(L.Grants);
	}
	return Out;
}
