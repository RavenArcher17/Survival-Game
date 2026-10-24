#include "WRLifeSkills.h"
#include "Wildreach.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------- registry

FWRLifeSkillRegistry& FWRLifeSkillRegistry::Get()
{
	static FWRLifeSkillRegistry Instance;
	return Instance;
}

const FWRLifeSkillDef* FWRLifeSkillRegistry::Find(FName Id) const
{
	return Defs.FindByPredicate([Id](const FWRLifeSkillDef& D) { return D.Id == Id; });
}

FWRLifeSkillRegistry::FWRLifeSkillRegistry()
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("lifeskills.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("skills"), List))
	{
		UE_LOG(LogWildreach, Error, TEXT("lifeskills.json missing or broken"));
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FWRLifeSkillDef D;
		D.Id = FName(*O->GetStringField(TEXT("id")));
		D.Name = O->GetStringField(TEXT("name"));
		O->TryGetStringField(TEXT("group"), D.Group);
		O->TryGetStringField(TEXT("levelsBy"), D.LevelsBy);
		const TSharedPtr<FJsonObject>* Every = nullptr;
		if (O->TryGetObjectField(TEXT("everyLevel"), Every))
		{
			FString Stat;
			(*Every)->TryGetStringField(TEXT("text"), D.EveryLevelText);
			if ((*Every)->TryGetStringField(TEXT("stat"), Stat)) D.EveryLevelStat = FName(*Stat);
			double Per = 0.0;
			if ((*Every)->TryGetNumberField(TEXT("perLevel"), Per)) D.PerLevel = Per;
		}
		const TSharedPtr<FJsonObject>* Perks = nullptr;
		O->TryGetObjectField(TEXT("perks"), Perks);
		for (const int32 Tier : TierLevels)
		{
			TArray<FWRPerkDef>& Options = D.Tiers.AddDefaulted_GetRef();
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Perks && (*Perks)->TryGetArrayField(FString::FromInt(Tier), Arr))
			{
				for (const TSharedPtr<FJsonValue>& PV : *Arr)
				{
					const TSharedPtr<FJsonObject> P = PV->AsObject();
					FWRPerkDef Perk;
					Perk.Id = FName(*P->GetStringField(TEXT("id")));
					Perk.Name = P->GetStringField(TEXT("name"));
					P->TryGetStringField(TEXT("text"), Perk.Text);
					FString Stat;
					if (P->TryGetStringField(TEXT("stat"), Stat)) Perk.Stat = FName(*Stat);
					double Value = 0.0;
					if (P->TryGetNumberField(TEXT("value"), Value)) Perk.Value = Value;
					Options.Add(Perk);
				}
			}
		}
		Defs.Add(D);
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d life skills"), Defs.Num());
}

int32 FWRLifeSkillRules::XPToNext(int32 Level)
{
	return FMath::RoundToInt(WRTune(TEXT("lifeSkills.levelXpBase"), 50.f) + WRTune(TEXT("lifeSkills.levelXpPerLevel"), 5.f) * Level);
}

// ---------------------------------------------------------------- component

UWRLifeSkillsComponent::UWRLifeSkillsComponent()
{
	SetIsReplicatedByDefault(true);
}

void UWRLifeSkillsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWRLifeSkillsComponent, Skills, COND_OwnerOnly);
}

UWRLifeSkillsComponent* UWRLifeSkillsComponent::FindFor(const AActor* Actor)
{
	const APlayerState* PS = Cast<APlayerState>(Actor);
	if (!PS)
	{
		if (const APawn* Pawn = Cast<APawn>(Actor)) PS = Pawn->GetPlayerState();
		else if (const AController* C = Cast<AController>(Actor)) PS = C->PlayerState;
	}
	return PS ? PS->FindComponentByClass<UWRLifeSkillsComponent>() : nullptr;
}

float UWRLifeSkillsComponent::StatFor(const AActor* Actor, FName StatName)
{
	const UWRLifeSkillsComponent* C = FindFor(Actor);
	return C ? C->Stat(StatName) : 0.f;
}

void UWRLifeSkillsComponent::Award(const AActor* Actor, FName Skill, const TCHAR* XPKey, float Fallback, float Times)
{
	if (UWRLifeSkillsComponent* C = FindFor(Actor))
	{
		C->AwardXP(Skill, WRTune(*FString::Printf(TEXT("lifeSkills.xp.%s"), XPKey), Fallback) * Times);
	}
}

FWRSkillProgress& UWRLifeSkillsComponent::Progress(FName Skill)
{
	if (FWRSkillProgress* P = Skills.FindByPredicate([Skill](const FWRSkillProgress& S) { return S.Skill == Skill; }))
	{
		return *P;
	}
	FWRSkillProgress& P = Skills.AddDefaulted_GetRef();
	P.Skill = Skill;
	P.Perks.Init(-1, 4);
	return P;
}

const FWRSkillProgress* UWRLifeSkillsComponent::FindProgress(FName Skill) const
{
	return Skills.FindByPredicate([Skill](const FWRSkillProgress& S) { return S.Skill == Skill; });
}

int32 UWRLifeSkillsComponent::GetLevel(FName Skill) const
{
	const FWRSkillProgress* P = FindProgress(Skill);
	return P ? P->Level : 1;
}

void UWRLifeSkillsComponent::AwardXP(FName Skill, float Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0.f || !FWRLifeSkillRegistry::Get().Find(Skill))
	{
		return;
	}
	// The server's XP multiplier applies to life skills too; fractions carry over.
	float& Rest = Carry.FindOrAdd(Skill);
	Rest += Amount * FMath::Clamp(WRTune(TEXT("server.xpMultiplier"), 1.f), 0.5f, 10.f);
	const int32 Whole = FMath::FloorToInt(Rest);
	Rest -= Whole;
	if (Whole <= 0)
	{
		return;
	}
	FWRSkillProgress& P = Progress(Skill);
	if (P.Level >= FWRLifeSkillRules::MaxLevel)
	{
		return;
	}
	P.XP += Whole;
	while (P.Level < FWRLifeSkillRules::MaxLevel && P.XP >= FWRLifeSkillRules::XPToNext(P.Level))
	{
		P.XP -= FWRLifeSkillRules::XPToNext(P.Level);
		++P.Level;
		UE_LOG(LogWildreach, Log, TEXT("SKILL %s %s reached level %d"), *GetNameSafe(GetOwner()), *Skill.ToString(), P.Level);
	}
	if (P.Level >= FWRLifeSkillRules::MaxLevel) P.XP = 0;
}

void UWRLifeSkillsComponent::DevSetLevel(FName Skill, int32 Level)
{
	FWRSkillProgress& P = Progress(Skill);
	P.Level = FMath::Clamp(Level, 1, FWRLifeSkillRules::MaxLevel);
	P.XP = 0;
}

float UWRLifeSkillsComponent::Stat(FName StatName) const
{
	float Total = 0.f;
	for (const FWRSkillProgress& P : Skills)
	{
		const FWRLifeSkillDef* Def = FWRLifeSkillRegistry::Get().Find(P.Skill);
		if (!Def)
		{
			continue;
		}
		if (Def->EveryLevelStat == StatName)
		{
			Total += Def->PerLevel * (P.Level - 1);
		}
		for (int32 T = 0; T < Def->Tiers.Num() && T < P.Perks.Num(); ++T)
		{
			if (P.Perks[T] >= 0 && Def->Tiers[T].IsValidIndex(P.Perks[T]) && Def->Tiers[T][P.Perks[T]].Stat == StatName)
			{
				Total += Def->Tiers[T][P.Perks[T]].Value;
			}
		}
	}
	return Total;
}

FString UWRLifeSkillsComponent::ChoosePerk(FName Skill, int32 TierLevel, int32 Option)
{
	const FWRLifeSkillDef* Def = FWRLifeSkillRegistry::Get().Find(Skill);
	const int32 T = TierLevel / 25 - 1;
	if (!Def || T < 0 || T > 3 || !Def->Tiers[T].IsValidIndex(Option))
	{
		return TEXT("no such perk");
	}
	FWRSkillProgress& P = Progress(Skill);
	if (P.Level < TierLevel)
	{
		return FString::Printf(TEXT("%s needs level %d"), *Def->Name, TierLevel);
	}
	if (P.Perks[T] >= 0)
	{
		return TEXT("already chose that perk");  // choices are permanent
	}
	P.Perks[T] = Option;
	const FWRPerkDef& Perk = Def->Tiers[T][Option];
	UE_LOG(LogWildreach, Log, TEXT("SKILL %s chose %s perk %s"), *GetNameSafe(GetOwner()), *Skill.ToString(), *Perk.Id.ToString());
	return FString::Printf(TEXT("%s perk: %s%s"), *Def->Name, *Perk.Name, Perk.Stat.IsNone() ? TEXT(" (takes effect once its system is in)") : TEXT(""));
}

TSharedRef<FJsonObject> UWRLifeSkillsComponent::ToJson() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	for (const FWRSkillProgress& P : Skills)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("level"), P.Level);
		O->SetNumberField(TEXT("xp"), P.XP);
		TArray<TSharedPtr<FJsonValue>> Perks;
		for (const int8 C : P.Perks) Perks.Add(MakeShared<FJsonValueNumber>(C));
		O->SetArrayField(TEXT("perks"), Perks);
		Root->SetObjectField(P.Skill.ToString(), O);
	}
	return Root;
}

void UWRLifeSkillsComponent::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	Skills.Reset();
	if (!Json.IsValid())
	{
		return;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Json->Values)
	{
		const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
		if (!O.IsValid()) continue;
		FWRSkillProgress& P = Progress(FName(*KV.Key));
		P.Level = FMath::Clamp(static_cast<int32>(O->GetNumberField(TEXT("level"))), 1, FWRLifeSkillRules::MaxLevel);
		P.XP = static_cast<int32>(O->GetNumberField(TEXT("xp")));
		const TArray<TSharedPtr<FJsonValue>>* Perks = nullptr;
		if (O->TryGetArrayField(TEXT("perks"), Perks))
		{
			for (int32 i = 0; i < 4 && i < Perks->Num(); ++i) P.Perks[i] = static_cast<int8>((*Perks)[i]->AsNumber());
		}
	}
}
