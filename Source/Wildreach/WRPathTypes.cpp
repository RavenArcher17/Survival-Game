#include "WRPathTypes.h"
#include "Wildreach.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FWRPathNodeDef* FWRPathDef::FindNode(FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FWRPathNodeDef& N) { return N.Id == NodeId; });
}

FWRPathRegistry& FWRPathRegistry::Get()
{
	static FWRPathRegistry Instance;
	return Instance;
}

FWRPathRegistry::FWRPathRegistry()
{
	Reload();
}

const FWRPathDef* FWRPathRegistry::Find(FName PathId) const
{
	return Paths.FindByPredicate([PathId](const FWRPathDef& P) { return P.Id == PathId; });
}

static void ParseNodes(const TArray<TSharedPtr<FJsonValue>>& Array, FName Branch, TArray<FWRPathNodeDef>& Out)
{
	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}
		FWRPathNodeDef Node;
		Node.Id = FName(*Obj->GetStringField(TEXT("id")));
		Node.Name = Obj->GetStringField(TEXT("name"));
		Obj->TryGetStringField(TEXT("text"), Node.Text);
		const FString Kind = Obj->GetStringField(TEXT("kind"));
		Node.Kind = Kind == TEXT("stat") ? EWRNodeKind::Stat : Kind == TEXT("capstone") ? EWRNodeKind::Capstone : EWRNodeKind::Ability;
		if (Node.Kind == EWRNodeKind::Stat)
		{
			Node.Cost = 1;
			Node.MaxRank = Obj->GetIntegerField(TEXT("ranks"));
		}
		else
		{
			Node.Cost = Obj->GetIntegerField(TEXT("cost"));
			Node.MaxRank = 1;
		}
		int32 Gate = 0;
		Obj->TryGetNumberField(TEXT("gate"), Gate);
		Node.Gate = Gate;
		Node.Branch = Branch;
		Out.Add(Node);
	}
}

bool FWRPathRegistry::ParsePath(const FString& JsonText, FWRPathDef& Out)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	Out = FWRPathDef();
	Out.Id = FName(*Root->GetStringField(TEXT("id")));
	Out.Name = Root->GetStringField(TEXT("name"));
	Out.Biome = FName(*Root->GetStringField(TEXT("biome")));
	const TSharedPtr<FJsonObject>* Resource = nullptr;
	if (Root->TryGetObjectField(TEXT("resource"), Resource))
	{
		Out.ResourceId = FName(*(*Resource)->GetStringField(TEXT("id")));
	}
	const TSharedPtr<FJsonObject>* Passive = nullptr;
	if (Root->TryGetObjectField(TEXT("rootPassive"), Passive))
	{
		Out.RootPassiveName = (*Passive)->GetStringField(TEXT("name"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Trunk = nullptr;
	if (Root->TryGetArrayField(TEXT("trunk"), Trunk))
	{
		ParseNodes(*Trunk, NAME_None, Out.Nodes);
	}
	const TArray<TSharedPtr<FJsonValue>>* Branches = nullptr;
	if (Root->TryGetArrayField(TEXT("branches"), Branches))
	{
		for (const TSharedPtr<FJsonValue>& BranchValue : *Branches)
		{
			const TSharedPtr<FJsonObject> Branch = BranchValue->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
			if (Branch.IsValid() && Branch->TryGetArrayField(TEXT("nodes"), Nodes))
			{
				ParseNodes(*Nodes, FName(*Branch->GetStringField(TEXT("id"))), Out.Nodes);
			}
		}
	}
	// The starter spell: named in the JSON, else the trunk's first ability.
	FString StarterStr;
	if (Root->TryGetStringField(TEXT("starter"), StarterStr) && !Out.FindNode(FName(*StarterStr)))
	{
		UE_LOG(LogWildreach, Warning, TEXT("Path %s: starter %s is not a node, using the trunk's first ability"), *Out.Id.ToString(), *StarterStr);
		StarterStr.Reset();
	}
	if (StarterStr.IsEmpty())
	{
		const FWRPathNodeDef* First = Out.Nodes.FindByPredicate([](const FWRPathNodeDef& N) { return N.Branch.IsNone() && N.Kind == EWRNodeKind::Ability; });
		StarterStr = First ? First->Id.ToString() : FString();
	}
	Out.Starter = StarterStr.IsEmpty() ? NAME_None : FName(*StarterStr);
	for (FWRPathNodeDef& Node : Out.Nodes)
	{
		Node.bStarter = Node.Id == Out.Starter;
	}
	return Out.IsValid();
}

void FWRPathRegistry::Reload()
{
	Paths.Reset();
	const FString Dir = FPaths::Combine(FWRTuning::DataDir(), TEXT("paths"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		FString Text;
		FWRPathDef Def;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, File)) && ParsePath(Text, Def))
		{
			Paths.Add(MoveTemp(Def));
		}
		else
		{
			UE_LOG(LogWildreach, Error, TEXT("Could not load path %s"), *File);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d paths from %s"), Paths.Num(), *Dir);
}

int32 FWRPathProgress::GetRank(FName NodeId) const
{
	const FWRNodeRank* Found = Ranks.FindByPredicate([NodeId](const FWRNodeRank& R) { return R.NodeId == NodeId; });
	return Found ? Found->Rank : 0;
}

void FWRPathProgress::SetRank(FName NodeId, int32 Rank)
{
	if (FWRNodeRank* Found = Ranks.FindByPredicate([NodeId](const FWRNodeRank& R) { return R.NodeId == NodeId; }))
	{
		Found->Rank = Rank;
	}
	else
	{
		Ranks.Add({ NodeId, Rank });
	}
}

int32 FWRPathRules::PointCost(int32 N)
{
	return FMath::RoundToInt(WRTune(TEXT("paths.pointXpBase"), 100.f) + WRTune(TEXT("paths.pointXpPerPoint"), 10.f) * N);
}

int32 FWRPathRules::PointsForXP(int32 XP)
{
	const int32 Max = FMath::RoundToInt(WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
	int32 Points = 0;
	int32 Needed = 0;
	while (Points < Max)
	{
		Needed += PointCost(Points + 1);
		if (Needed > XP)
		{
			break;
		}
		++Points;
	}
	return Points;
}

int32 FWRPathRules::XPForPoints(int32 Points)
{
	int32 Total = 0;
	for (int32 N = 1; N <= Points; ++N)
	{
		Total += PointCost(N);
	}
	return Total;
}

int32 FWRPathRules::Spent(const FWRPathDef& Def, const FWRPathProgress& Progress)
{
	int32 Total = 0;
	for (const FWRPathNodeDef& Node : Def.Nodes)
	{
		// The starter came free.
		Total += FMath::Max(0, Progress.GetRank(Node.Id) - (Node.bStarter ? 1 : 0)) * Node.Cost;
	}
	return Total;
}

int32 FWRPathRules::SpentInBranch(const FWRPathDef& Def, const FWRPathProgress& Progress, FName Branch)
{
	int32 Total = 0;
	for (const FWRPathNodeDef& Node : Def.Nodes)
	{
		if (Node.Branch == Branch)
		{
			Total += FMath::Max(0, Progress.GetRank(Node.Id) - (Node.bStarter ? 1 : 0)) * Node.Cost;
		}
	}
	return Total;
}

bool FWRPathRules::TrunkComplete(const FWRPathDef& Def, const FWRPathProgress& Progress)
{
	for (const FWRPathNodeDef& Node : Def.Nodes)
	{
		if (Node.Branch.IsNone() && Progress.GetRank(Node.Id) < Node.MaxRank)
		{
			return false;
		}
	}
	return true;
}

FString FWRPathRules::CanBuy(const FWRPathDef& Def, const FWRPathProgress& Progress, int32 OtherPathSpent, FName NodeId)
{
	const FWRPathNodeDef* Node = Def.FindNode(NodeId);
	if (!Node)
	{
		return TEXT("no such node");
	}
	if (Progress.GetRank(NodeId) >= Node->MaxRank)
	{
		return TEXT("already maxed");
	}
	const int32 Spent = FWRPathRules::Spent(Def, Progress);
	const int32 Cost = NextRankCost(*Node, Progress);
	if (PointsForXP(Progress.XP) - Spent < Cost)
	{
		return TEXT("not enough points");
	}
	if (Spent + Cost > FMath::RoundToInt(WRTune(TEXT("paths.maxPointsPerPath"), 100.f)))
	{
		return TEXT("path cap (100)");
	}
	if (Spent + OtherPathSpent + Cost > FMath::RoundToInt(WRTune(TEXT("paths.maxPointsTotal"), 150.f)))
	{
		return TEXT("total cap (150)");
	}
	if (!Node->Branch.IsNone())
	{
		if (!TrunkComplete(Def, Progress))
		{
			return TEXT("finish the trunk first");
		}
		if (SpentInBranch(Def, Progress, Node->Branch) < Node->Gate)
		{
			return FString::Printf(TEXT("needs %d points in the branch"), Node->Gate);
		}
	}
	return FString();
}

int32 FWRPathRules::NextRankCost(const FWRPathNodeDef& Node, const FWRPathProgress& Progress)
{
	return Node.bStarter && Progress.GetRank(Node.Id) == 0 ? 0 : Node.Cost;
}

bool FWRPathRules::GrantStarter(const FWRPathDef& Def, FWRPathProgress& Progress)
{
	if (Def.Starter.IsNone() || Progress.GetRank(Def.Starter) > 0)
	{
		return false;
	}
	Progress.SetRank(Def.Starter, 1);
	return true;
}

int32 FWRPathRules::CharacterLevel(int32 TotalSpent)
{
	return FMath::Clamp(TotalSpent / 3, 1, 50);
}

float FWRPathRules::LevelStat(float AtLevel1, float AtLevel50, int32 Level)
{
	const float T = (FMath::Clamp(Level, 1, 50) - 1) / 49.f;
	return FMath::RoundToFloat(FMath::Lerp(AtLevel1, AtLevel50, T));
}
