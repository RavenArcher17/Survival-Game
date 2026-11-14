// Taming beyond the tame (cooks/build-taming.md; FULL-GAME 6.5; numbers from survival-crafting 2.19-2.21, all in
// tuning.json "tame" and Data/tame_trees.json): creature levels and stats, bond levels from the tame's own work, the five
// role trees, saddles / pack saddles / barding, hunger, mating, eggs and pregnancy, babies and their care prompts,
// mutations, the incubator and the save. Server-authoritative; FWRTameRep carries what every machine shows.

#include "WRTaming.h"
#include "WRCreature.h"
#include "WRCharacter.h"
#include "WRBuilding.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRLifeSkills.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRSocial.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRAttributeSet.h"
#include "WRStatusComponent.h"
#include "WRGameplayTags.h"
#include "WRWater.h"
#include "WRSurvival.h"
#include "WRGameState.h"
#include "WRBoss.h"
#include "WRToolMesh.h"
#include "WRFarm.h"
#include "WRCharacterMovementComponent.h"
#include "WRWorldDressing.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRTameDetail
{
	float T(const TCHAR* Path, float Fallback) { return WRTune(*(FString(TEXT("tame.")) + Path), Fallback); }

	APlayerState* PlayerNamed(const UWorld* World, const FString& Name)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS || Name.IsEmpty()) return nullptr;
		for (APlayerState* PS : GS->PlayerArray) if (PS && PS->GetPlayerName() == Name) return PS;
		return nullptr;
	}

	APawn* PawnOf(const UWorld* World, const FString& Name)
	{
		const APlayerState* PS = PlayerNamed(World, Name);
		return PS ? PS->GetPawn() : nullptr;
	}

	/** The player's body even while they ride (the pawn is then the mount). */
	AActor* BodyOf(const UWorld* World, const FString& Name)
	{
		APawn* P = PawnOf(World, Name);
		if (const AWRCreature* Mount = Cast<AWRCreature>(P)) return Mount->GetRider() ? static_cast<AActor*>(Mount->GetRider()) : P;
		return P;
	}

	void Notify(const UWorld* World, const FString& Name, const FString& Message)
	{
		if (const APlayerState* PS = PlayerNamed(World, Name))
		{
			if (AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner())) PC->ClientNotify(Message);
		}
	}

	bool Allies(const FString& A, const FString& B) { return !A.IsEmpty() && (A == B || FWRClans::Get().SameClan(A, B)); }

	FString CreatureName(const AWRCreature* C)
	{
		const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
		return Def ? Def->Name : TEXT("tame");
	}

	/** The band a creature's wild level comes from. */
	FIntPoint Band(const FWRCreatureDef& Def)
	{
		const FString Tier = Def.Tier.ToString();
		if (Tier == TEXT("lowland")) return FIntPoint(int32(T(TEXT("levels.lowlandMin"), 1.f)), int32(T(TEXT("levels.lowlandMax"), 10.f)));
		if (Tier == TEXT("midland")) return FIntPoint(int32(T(TEXT("levels.midlandMin"), 11.f)), int32(T(TEXT("levels.midlandMax"), 20.f)));
		if (Tier == TEXT("highland")) return FIntPoint(int32(T(TEXT("levels.highlandMin"), 21.f)), int32(T(TEXT("levels.highlandMax"), 30.f)));
		return FIntPoint(0, 0);
	}

	/** A curated set of morph tints (pale ash, ember, frost, bone, obsidian, violet, gold, moss). */
	FColor MorphTint()
	{
		static const FColor Tints[] = { FColor(214, 206, 196), FColor(232, 104, 36), FColor(120, 206, 238), FColor(236, 226, 200),
			FColor(30, 26, 34), FColor(150, 92, 210), FColor(236, 186, 64), FColor(96, 150, 70) };
		return Tints[FMath::RandHelper(UE_ARRAY_COUNT(Tints))];
	}

	FString Join(const uint8* V, int32 N)
	{
		FString S;
		for (int32 i = 0; i < N; ++i) S += (i ? TEXT(",") : TEXT("")) + FString::FromInt(V[i]);
		return S;
	}

	void Split(const FString& S, uint8* Out, int32 N)
	{
		TArray<FString> Parts;
		S.ParseIntoArray(Parts, TEXT(","));
		for (int32 i = 0; i < N && i < Parts.Num(); ++i) Out[i] = uint8(FMath::Clamp(FCString::Atoi(*Parts[i]), 0, 255));
	}

	float BondCost(int32 NextLevel) { return T(TEXT("bondBase"), 100.f) + T(TEXT("bondPer"), 40.f) * NextLevel; }

	/** Kibble or anything it eats. */
	bool IsItsFood(const FWRCreatureDef& Def, FName Item) { return Item == TEXT("kibble") || Def.Food.Contains(Item); }
}
using namespace WRTameDetail;

// ---------------------------------------------------------------- names

const TCHAR* WRTameStat::Name(int32 Id)
{
	static const TCHAR* Names[] = { TEXT("health"), TEXT("stamina"), TEXT("damage"), TEXT("carry"), TEXT("speed"), TEXT("torpor") };
	return Id >= 0 && Id < Count ? Names[Id] : TEXT("");
}

const TCHAR* WRTameStat::Label(int32 Id)
{
	static const TCHAR* Names[] = { TEXT("Health"), TEXT("Stamina"), TEXT("Damage"), TEXT("Carry"), TEXT("Speed"), TEXT("Torpor") };
	return Id >= 0 && Id < Count ? Names[Id] : TEXT("");
}

const TCHAR* WRTameMorph::Name(uint8 Kind)
{
	switch (Kind)
	{
	case Pale: return TEXT("pale morph");
	case Dark: return TEXT("dark morph");
	case Bands: return TEXT("banded");
	case GlowLine: return TEXT("glow line");
	default: return TEXT("");
	}
}

// ---------------------------------------------------------------- genes

int32 FWRTameGenes::Level() const
{
	int32 N = 0;
	for (int32 i = 0; i < WRTameStat::Count; ++i) N += Points[i];
	return N;
}

FWRTameGenes FWRTameGenes::Wild(int32 Level)
{
	FWRTameGenes G;
	for (int32 i = 0; i < Level; ++i) ++G.Points[FMath::RandHelper(WRTameStat::Count)];
	G.Sex = FMath::RandBool() ? 1 : 2;
	G.bValid = true;
	return G;
}

void FWRTameGenes::Mutate(bool bFather)
{
	const int32 S = FMath::RandHelper(WRTameStat::Count);
	const uint8 Add = uint8(T(TEXT("breed.mutationPoints"), 2.f));
	Points[S] = uint8(FMath::Min(255, Points[S] + Add));
	MutPoints[S] = uint8(FMath::Min(255, MutPoints[S] + Add));
	uint8& Side = bFather ? MutFather : MutMother;
	Side = uint8(FMath::Min(255, Side + 1));
	const TPair<uint8, FColor> Morph(uint8(1 + FMath::RandHelper(4)), MorphTint());
	if (Morphs.Num() >= 4) Morphs[FMath::RandHelper(Morphs.Num())] = Morph;
	else Morphs.Add(Morph);
}

FWRTameGenes FWRTameGenes::Child(const FWRTameGenes& Father, const FWRTameGenes& Mother, float MutationOddsMult, int32& OutMutations)
{
	FWRTameGenes C;
	const float Higher = T(TEXT("breed.higherShare"), 0.55f);
	for (int32 s = 0; s < WRTameStat::Count; ++s)
	{
		const bool bFatherHigher = Father.Points[s] >= Mother.Points[s];
		const bool bTakeHigher = FMath::FRand() < Higher;
		const FWRTameGenes& From = (bFatherHigher == bTakeHigher) ? Father : Mother;
		C.Points[s] = From.Points[s];
		C.MutPoints[s] = From.MutPoints[s];
	}
	// Ark's lines: each side counts every mutation of that parent's own ancestry.
	C.MutFather = uint8(FMath::Min(255, Father.MutFather + Father.MutMother));
	C.MutMother = uint8(FMath::Min(255, Mother.MutFather + Mother.MutMother));
	for (const FWRTameGenes* P : { &Father, &Mother })
	{
		for (const TPair<uint8, FColor>& M : P->Morphs)
		{
			if (C.Morphs.Num() < 4 && FMath::RandBool()) C.Morphs.Add(M);
		}
	}
	OutMutations = 0;
	const int32 Rolls = FMath::RoundToInt(T(TEXT("breed.mutationRolls"), 3.f));
	const float Chance = T(TEXT("breed.mutationChance"), 0.025f) * FMath::Max(0.f, MutationOddsMult);
	const int32 Cap = FMath::RoundToInt(T(TEXT("breed.mutationCap"), 20.f));
	for (int32 r = 0; r < Rolls; ++r)
	{
		if (FMath::FRand() >= Chance) continue;
		const bool bFather = FMath::RandBool();
		if ((bFather ? C.MutFather : C.MutMother) >= Cap) continue;   // that line has had its share
		C.Mutate(bFather);
		++OutMutations;
	}
	C.Sex = FMath::RandBool() ? 1 : 2;
	C.bValid = true;
	C.bBred = true;
	return C;
}

void FWRTameGenes::ToSockets(TArray<FName>& Out, FName Species) const
{
	Out.Reset();
	Out.Add(FName(*(TEXT("species:") + Species.ToString())));
	Out.Add(FName(*FString::Printf(TEXT("sex:%d"), Sex)));
	Out.Add(FName(*(TEXT("pts:") + Join(Points, WRTameStat::Count))));
	Out.Add(FName(*(TEXT("mut:") + Join(MutPoints, WRTameStat::Count))));
	Out.Add(FName(*FString::Printf(TEXT("line:%d,%d"), MutFather, MutMother)));
	if (bBred) Out.Add(FName(TEXT("bred:1")));
	for (const TPair<uint8, FColor>& M : Morphs) Out.Add(FName(*FString::Printf(TEXT("morph:%d:%s"), M.Key, *M.Value.ToHex())));
}

FWRTameGenes FWRTameGenes::FromSockets(const TArray<FName>& In, FName* OutSpecies)
{
	FWRTameGenes G;
	for (const FName& N : In)
	{
		const FString S = N.ToString();
		FString Key, Value;
		if (!S.Split(TEXT(":"), &Key, &Value)) continue;
		if (Key == TEXT("species")) { if (OutSpecies) *OutSpecies = FName(*Value); }
		else if (Key == TEXT("sex")) G.Sex = uint8(FCString::Atoi(*Value));
		else if (Key == TEXT("pts")) { Split(Value, G.Points, WRTameStat::Count); G.bValid = true; }
		else if (Key == TEXT("mut")) Split(Value, G.MutPoints, WRTameStat::Count);
		else if (Key == TEXT("line")) { uint8 L[2] = { 0, 0 }; Split(Value, L, 2); G.MutFather = L[0]; G.MutMother = L[1]; }
		else if (Key == TEXT("bred")) G.bBred = true;
		else if (Key == TEXT("morph"))
		{
			FString Kind, Hex;
			if (Value.Split(TEXT(":"), &Kind, &Hex)) G.Morphs.Add({ uint8(FCString::Atoi(*Kind)), FColor::FromHex(Hex) });
		}
	}
	return G;
}

TSharedRef<FJsonObject> FWRTameGenes::ToJson() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	TArray<FName> S;
	ToSockets(S, NAME_None);
	TArray<TSharedPtr<FJsonValue>> A;
	for (const FName& N : S) A.Add(MakeShared<FJsonValueString>(N.ToString()));
	O->SetArrayField(TEXT("genes"), A);
	return O;
}

FWRTameGenes FWRTameGenes::FromJson(const TSharedPtr<FJsonObject>& O)
{
	TArray<FName> S;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (O.IsValid() && O->TryGetArrayField(TEXT("genes"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A) S.Add(FName(*V->AsString()));
	}
	return FromSockets(S);
}

// ---------------------------------------------------------------- trees

FWRTameTrees& FWRTameTrees::Get()
{
	static FWRTameTrees Instance;
	return Instance;
}

FWRTameTrees::FWRTameTrees()
{
	Reload();
}

void FWRTameTrees::Reload()
{
	Trees.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("tame_trees.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("trees"), List)) return;
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FWRTameTree Tree;
		Tree.Id = FName(*O->GetStringField(TEXT("id")));
		Tree.Name = O->GetStringField(TEXT("name"));
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if (O->TryGetArrayField(TEXT("nodes"), Nodes))
		{
			for (const TSharedPtr<FJsonValue>& NV : *Nodes)
			{
				const TSharedPtr<FJsonObject> N = NV->AsObject();
				if (!N.IsValid()) continue;
				FWRTameNode Node;
				Node.Id = FName(*N->GetStringField(TEXT("id")));
				Node.Name = N->GetStringField(TEXT("name"));
				Node.Text = N->GetStringField(TEXT("text"));
				double Cost = 2.0;
				N->TryGetNumberField(TEXT("cost"), Cost);
				Node.Cost = FMath::Max(1, int32(Cost));
				Tree.Nodes.Add(Node);
			}
		}
		if (Tree.Nodes.Num() == 6) Trees.Add(Tree);
	}
}

const FWRTameTree* FWRTameTrees::Find(FName Id) const
{
	return Trees.FindByPredicate([Id](const FWRTameTree& T) { return T.Id == Id; });
}

const FWRTameTree* FWRTameTrees::For(const FWRCreatureDef& Def) const
{
	FString Role = Def.Role.TrimStartAndEnd();
	int32 Cut = INDEX_NONE;
	if (Role.FindChar(TEXT(','), Cut)) Role = Role.Left(Cut);
	if (Role.FindChar(TEXT(' '), Cut)) Role = Role.Left(Cut);
	return Find(FName(*Role.ToLower()));
}

// ---------------------------------------------------------------- rules

const FWRTackDef* FWRTaming::Tack(FName ItemId)
{
	static TMap<FName, FWRTackDef> Defs;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("items.json"));
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), Items))
		{
			for (const TSharedPtr<FJsonValue>& V : *Items)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const TSharedPtr<FJsonObject>* Tk = nullptr;
				if (!O.IsValid() || !O->HasField(TEXT("id")) || !O->TryGetObjectField(TEXT("tack"), Tk)) continue;
				FWRTackDef D;
				FString S;
				if ((*Tk)->TryGetStringField(TEXT("kind"), S)) D.Kind = FName(*S);
				double N = 0.0;
				if ((*Tk)->TryGetNumberField(TEXT("reduction"), N)) D.Reduction = N;
				if ((*Tk)->TryGetNumberField(TEXT("slots"), N)) D.Slots = int32(N);
				if ((*Tk)->TryGetNumberField(TEXT("speed"), N)) D.Speed = N;
				if ((*Tk)->TryGetStringField(TEXT("hazard"), S)) D.Hazard = FName(*S);
				Defs.Add(FName(*O->GetStringField(TEXT("id"))), D);
			}
		}
	}
	return Defs.Find(ItemId);
}

float FWRTaming::TackValue(const FWRItemStack& Stack, const AActor* Owner)
{
	const FWRTackDef* D = Stack.IsEmpty() ? nullptr : Tack(Stack.ItemId);
	if (!D) return 0.f;
	static const float QualityMult[] = { 0.9f, 1.f, 1.1f, 1.2f };
	const float Q = QualityMult[FMath::Clamp(int32(Stack.Quality), 0, 3)];
	const float Perk = D->Kind == TEXT("barding") ? UWRLifeSkillsComponent::StatFor(Owner, TEXT("bardingValue")) : UWRLifeSkillsComponent::StatFor(Owner, TEXT("saddleValue"));
	return FMath::Clamp(D->Reduction * Q * FMath::Max(0.5f, Stack.Roll) * (1.f + Perk), 0.f, T(TEXT("tack.maxReduction"), 0.75f));
}

bool FWRTaming::LaysEggs(const FWRCreatureDef& Def)
{
	return Def.Birth == TEXT("egg");
}

FName FWRTaming::MountKind(const FWRCreatureDef& Def)
{
	if (!Def.bMount) return NAME_None;
	if (!Def.MountKind.IsNone()) return Def.MountKind;
	return Def.Role.Contains(TEXT("hauler")) ? FName(TEXT("pack")) : FName(TEXT("ground"));
}

float FWRTaming::ByDifficulty(const FWRCreatureDef& Def, const TCHAR* Table, float Easy, float Medium, float Hard)
{
	const FString D = Def.Difficulty.IsNone() ? FString(TEXT("easy")) : Def.Difficulty.ToString();
	const float Fallback = D == TEXT("hard") ? Hard : D == TEXT("medium") ? Medium : Easy;
	return T(*FString::Printf(TEXT("%s.%s"), Table, *D), Fallback);
}

float FWRTaming::Maturation()
{
	return FMath::Max(0.05f, WRTune(TEXT("server.maturationMultiplier"), 1.f));
}

float FWRTaming::HungerSeconds(const FWRCreatureDef& Def)
{
	const float Hours = Def.Radius < 50.f ? T(TEXT("hunger.smallHours"), 3.f) : Def.Radius < 120.f ? T(TEXT("hunger.mediumHours"), 4.f) : T(TEXT("hunger.largeHours"), 6.f);
	return FMath::Max(60.f, Hours * 3600.f);
}

float FWRTaming::KillXP(const FWRCreatureDef& Def)
{
	if (Def.Archetype == TEXT("boss")) return T(TEXT("xp.killBoss"), 300.f);
	if (Def.Tier == TEXT("highland")) return T(TEXT("xp.killHighland"), 80.f);
	if (Def.Tier == TEXT("midland")) return T(TEXT("xp.killMidland"), 40.f);
	if (Def.Archetype == TEXT("dummy")) return 0.f;
	return T(TEXT("xp.killLowland"), 20.f);
}

TArray<FName> FWRTaming::FuelFor(FName Element)
{
	if (Element == TEXT("fire") || Element == TEXT("sun")) return { TEXT("charcoal"), TEXT("sulfur_crystal") };
	if (Element == TEXT("frost") || Element == TEXT("wind")) return { TEXT("ice_block") };
	if (Element == TEXT("tide") || Element == TEXT("blood")) return { TEXT("waterskin") };
	return { TEXT("wood") };
}

// ---------------------------------------------------------------- the creature's numbers

float AWRCreature::ExpectedPoints() const
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def) return 0.f;
	const FIntPoint B = Band(*Def);
	return (B.X + B.Y) * 0.5f / float(WRTameStat::Count);
}

void AWRCreature::InitGenes(const FWRTameGenes& In)
{
	Genes = In;
	Genes.bValid = true;
	TameRep.Sex = Genes.Sex;
	TameRep.WildLevel = uint8(FMath::Clamp(Genes.Level(), 0, 255));
	TameRep.Points.SetNum(WRTameStat::Count);
	TameRep.MutPoints.SetNum(WRTameStat::Count);
	for (int32 s = 0; s < WRTameStat::Count; ++s)
	{
		TameRep.Points[s] = Genes.Points[s];
		TameRep.MutPoints[s] = Genes.MutPoints[s];
	}
	TameRep.MutFather = Genes.MutFather;
	TameRep.MutMother = Genes.MutMother;
	TameRep.MorphKinds.Reset();
	TameRep.MorphColors.Reset();
	for (const TPair<uint8, FColor>& M : Genes.Morphs)
	{
		TameRep.MorphKinds.Add(M.Key);
		TameRep.MorphColors.Add(M.Value);
	}
	RecomputeStats();
	OnRep_TameRep();
}

void AWRCreature::RollWildGenes()
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def) return;
	const FIntPoint B = Band(*Def);
	if (B.Y <= 0 || Def->Archetype == TEXT("boss") || Def->Archetype == TEXT("dummy") || Def->Role == TEXT("minion"))
	{
		FWRTameGenes None;
		None.Sex = FMath::RandBool() ? 1 : 2;
		None.bValid = true;
		InitGenes(None);
		return;
	}
	FWRTameGenes G = FWRTameGenes::Wild(FMath::RandRange(B.X, B.Y));
	// A wild mutant now and then (twice as often near anyone with Rare finder).
	float Chance = T(TEXT("breed.wildMutantChance"), 0.01f);
	for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetPlayerState() && UWRLifeSkillsComponent::StatFor(*It, TEXT("rareFinder")) > 0.f
			&& FVector::Dist(It->GetActorLocation(), GetActorLocation()) < T(TEXT("breed.rareFinderRange"), 10000.f))
		{
			Chance *= 2.f;
			break;
		}
	}
	if (FMath::FRand() < Chance) G.Mutate(FMath::RandBool());
	InitGenes(G);
}

void AWRCreature::RecomputeStats()
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def || !GetAttributes()) return;
	const UWRAttributeSet* A = GetAttributes();
	const float Frac = A->GetMaxHealth() > 0.f ? FMath::Clamp(A->GetHealth() / A->GetMaxHealth(), 0.f, 1.f) : 1.f;
	const bool bTame = IsTamed();
	const float Per = T(TEXT("statPerPoint"), 0.02f);
	const float Clamp = T(TEXT("wildClamp"), 0.1f);
	const float Expected = ExpectedPoints();
	const float Bond = bTame ? 1.f + T(TEXT("bondStat"), 0.02f) * TameRep.Bond : 1.f;
	TArray<float> M;
	M.SetNum(WRTameStat::Count);
	for (int32 s = 0; s < WRTameStat::Count; ++s)
	{
		float Base = 1.f + Per * (Genes.Points[s] - Expected);
		Base = Genes.bBred ? FMath::Max(0.5f, Base) : FMath::Clamp(Base, 1.f - Clamp, 1.f + Clamp);
		if (Expected <= 0.f) Base = 1.f;   // bosses, minions, the dummy: no levels
		M[s] = Base * Bond;
	}
	// The role tree's stat nodes.
	if (bTame)
	{
		if (TameHasNode(TEXT("hardened"))) M[WRTameStat::Health] *= 1.15f;
		if (TameHasNode(TEXT("bite_force"))) M[WRTameStat::Damage] *= 1.15f;
		if (TameHasNode(TEXT("stride"))) M[WRTameStat::Speed] *= 1.08f;
		if (TameHasNode(TEXT("courser"))) M[WRTameStat::Speed] *= 1.15f;
		if (TameHasNode(TEXT("broad_back"))) M[WRTameStat::Carry] *= 1.25f;
		if (TameHasNode(TEXT("caravan"))) M[WRTameStat::Carry] *= 1.5f;
	}
	TameRep.Mults = M;
	// A baby is small and soft: health and bite grow with it.
	const float Grown = bTame && TameRep.IsBaby() ? FMath::Lerp(T(TEXT("breed.babyStatsAtBirth"), 0.25f), 1.f, TameRep.Growth) : 1.f;
	const float Difficulty = bTame ? 1.f + TameBonus : FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f));
	const FName Kind = FWRTaming::MountKind(*Def);
	const float MaxHealth = Def->Health * Difficulty * M[WRTameStat::Health] * Grown + (bTame && TameHasNode(TEXT("bulwark")) ? T(TEXT("tree.bulwarkHealth"), 200.f) : 0.f);
	const float MaxStamina = (Kind.IsNone() ? 100.f : Kind == TEXT("fly") ? T(TEXT("fly.stamina"), 300.f) : T(TEXT("mount.stamina"), 200.f)) * M[WRTameStat::Stamina];
	InitVitals(MaxHealth, MaxStamina, Def->Poise);
	SetHealthFraction(Frac);
	Weapon.FlatDamage = Def->Damage * Difficulty * M[WRTameStat::Damage] * Grown;
	MaxTorpor = Def->Health * WRTune(TEXT("taming.torporPerHealth"), 1.f) * M[WRTameStat::Torpor];
	// A tame keeps its own element: its bites carry it (FULL-GAME 6.5, DD How taming works).
	Weapon.Element = bTame && (Def->Element == TEXT("fire") || Def->Element == TEXT("frost")) ? Def->Element : NAME_None;
	if (!Rider.IsValid())
	{
		GetCharacterMovement()->MaxWalkSpeed = (Def->SwimSpeed > 0.f && Def->LandSpeed > 0.f ? Def->LandSpeed : Def->Speed) * M[WRTameStat::Speed];
		GetCharacterMovement()->MaxFlySpeed = Def->Speed * M[WRTameStat::Speed];
		if (Def->SwimSpeed > 0.f) GetCharacterMovement()->MaxSwimSpeed = Def->SwimSpeed * 0.5f * M[WRTameStat::Speed];
	}
	// Biome barding carries its land's pass to the mount (lava, slick ice).
	const FWRTackDef* Bard = FWRTaming::Tack(TameRep.Barding.ItemId);
	const bool bLava = Bard && Bard->Hazard == TEXT("lava"), bIce = Bard && Bard->Hazard == TEXT("ice");
	if (bLava != bBardingLava) { AbilitySystem->AddLooseGameplayTag(WRTags::Resist_Lava, bLava ? 1 : -1, EGameplayTagReplicationState::TagOnly); bBardingLava = bLava; }
	if (bIce != bBardingIce) { AbilitySystem->AddLooseGameplayTag(WRTags::Resist_Ice, bIce ? 1 : -1, EGameplayTagReplicationState::TagOnly); bBardingIce = bIce; }
}

float AWRCreature::StatMult(int32 Stat) const
{
	return TameRep.Mults.IsValidIndex(Stat) ? TameRep.Mults[Stat] : 1.f;
}

bool AWRCreature::TameHasNode(FName NodeId) const
{
	const FWRCreatureDef* Def = GetDef();
	const FWRTameTree* Tree = Def ? FWRTameTrees::Get().For(*Def) : nullptr;
	if (!Tree || !IsTamed()) return false;
	for (int32 i = 0; i < Tree->Nodes.Num(); ++i)
	{
		if (Tree->Nodes[i].Id == NodeId) return TameRep.HasNode(i);
	}
	return false;
}

float AWRCreature::ImprintScale() const
{
	if (TameRep.Imprint <= 0.f || TameRep.Imprinter.IsEmpty()) return 1.f;
	const AWRCharacter* R = Rider.Get();
	bool bBeside = R && R->GetPlayerState() && R->GetPlayerState()->GetPlayerName() == TameRep.Imprinter;
	if (!bBeside)
	{
		const AActor* Body = BodyOf(GetWorld(), TameRep.Imprinter);
		bBeside = Body && FVector::Dist(Body->GetActorLocation(), GetActorLocation()) < T(TEXT("imprint.besideRange"), 1500.f);
	}
	return bBeside ? 1.f + T(TEXT("imprint.bonus"), 0.2f) * TameRep.Imprint : 1.f;
}

void AWRCreature::GainBondXP(float Amount, const TCHAR* Why)
{
	const FWRCreatureDef* Def = GetDef();
	const int32 Max = FMath::RoundToInt(T(TEXT("bondMax"), 20.f));
	if (!HasAuthority() || !IsTamed() || !IsAlive() || !Def || Amount <= 0.f || TameRep.Bond >= Max) return;
	TameRep.BondXP += Amount * FMath::Max(0.f, WRTune(TEXT("server.xpMultiplier"), 1.f));
	bool bUp = false;
	while (TameRep.Bond < Max && TameRep.BondXP >= BondCost(TameRep.Bond + 1))
	{
		TameRep.BondXP -= BondCost(TameRep.Bond + 1);
		++TameRep.Bond;
		bUp = true;
	}
	if (TameRep.Bond >= Max) TameRep.BondXP = 0.f;
	if (bUp)
	{
		RecomputeStats();
		WRFx::Broadcast(this, TEXT("tame.levelup"), GetActorLocation(), FRotator::ZeroRotator, FMath::Max(0.6f, GetSimpleCollisionRadius() / 80.f), TEXT("tame.levelup"));
		MulticastSound(TEXT("voice.idle"));
		UWRLifeSkillsComponent::Award(PlayerNamed(GetWorld(), TamedBy), TEXT("taming"), TEXT("tameBond"), T(TEXT("skillXp.bond"), 20.f));
		Notify(GetWorld(), TamedBy, FString::Printf(TEXT("%s reached bond %d: a skill point to spend (Tend)"), *Def->Name, TameRep.Bond));
		UE_LOG(LogWildreach, Log, TEXT("TAME %s bond %d (%s)"), *GetName(), TameRep.Bond, Why);
	}
}

FName AWRCreature::GetMountKind() const
{
	const FWRCreatureDef* Def = GetDef();
	return Def ? FWRTaming::MountKind(*Def) : NAME_None;
}

FVector AWRCreature::SeatLocation() const
{
	const FWRCreatureDef* Def = GetDef();
	const float R = GetCapsuleComponent()->GetScaledCapsuleRadius(), H = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return GetActorLocation() + GetActorForwardVector() * (Def ? Def->SeatFwd : 0.f) * R + GetActorUpVector() * (Def ? Def->SeatUp : 1.f) * H;
}

int32 AWRCreature::CargoSlots() const
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def || Def->Carry <= 0.f) return 0;
	const FWRTackDef* Pack = FWRTaming::Tack(TameRep.Saddle.ItemId);
	return FMath::Max(1, FMath::FloorToInt(Def->Carry / FMath::Max(1.f, T(TEXT("mount.slotKg"), 20.f))))
		+ (Pack && Pack->Kind == TEXT("pack") ? Pack->Slots : 0) + (TameHasNode(TEXT("packing")) ? 8 : 0);
}

float AWRCreature::CargoCapacityKg() const
{
	const FWRCreatureDef* Def = GetDef();
	return Def ? Def->Carry * StatMult(WRTameStat::Carry) * ImprintScale() : 0.f;
}

float AWRCreature::CargoKg() const
{
	return FWRInventoryRules::TotalWeight(Cargo);
}

int32 AWRCreature::AddCargo(FName ItemId, int32 Count)
{
	const FWRItemDef* Item = FWRItemRegistry::Get().Find(ItemId);
	if (!HasAuthority() || !Item || Count <= 0 || CargoSlots() <= 0) return Count;
	if (Cargo.Num() < CargoSlots()) Cargo.SetNum(CargoSlots());
	const float Room = CargoCapacityKg() - CargoKg();
	const int32 ByWeight = Item->Weight > 0.f ? FMath::FloorToInt(Room / Item->Weight) : Count;
	const int32 Want = FMath::Clamp(ByWeight, 0, Count);
	const int32 Left = FWRInventoryRules::Add(Cargo, ItemId, Want);
	TameRep.CargoKg = CargoKg();
	return Count - Want + Left;
}

void AWRCreature::OnRep_TameRep()
{
	if (GetNetMode() != NM_DedicatedServer) RefreshTameLook();
}

bool AWRCreature::MountStaminaRates(float& OutDrain, float& OutRegen) const
{
	if (!Rider.IsValid()) return false;
	OutDrain = T(TEXT("mount.sprintStamina"), 20.f) * (TameHasNode(TEXT("sure_footed")) ? 0.85f : 1.f);
	OutRegen = T(TEXT("mount.regen"), 25.f) * (TameHasNode(TEXT("courser")) ? 1.5f : 1.f);
	const FWRCreatureDef* Def = GetDef();
	if (Def && Def->Id == TEXT("palestrider")) OutDrain = 0.f;   // never tires (FULL-GAME 3.2 graveyard)
	return true;
}

// ---------------------------------------------------------------- riding speeds and flight

void AWRCreature::ApplyRideSpeeds()
{
	const FWRCreatureDef* Def = GetDef();
	AWRCharacter* R = Rider.Get();
	if (!Def || !R) return;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FName Kind = FWRTaming::MountKind(*Def);
	const FWRTackDef* Bard = FWRTaming::Tack(TameRep.Barding.ItemId);
	const bool bRiderImprinter = !TameRep.Imprinter.IsEmpty() && R->GetPlayerState() && R->GetPlayerState()->GetPlayerName() == TameRep.Imprinter;
	float Speed = Def->RideSpeed * StatMult(WRTameStat::Speed) * (1.f + UWRLifeSkillsComponent::StatFor(R, TEXT("rideSpeed")))
		* (1.f + (Bard ? Bard->Speed : 0.f)) * (bRiderImprinter ? 1.f + T(TEXT("imprint.bonus"), 0.2f) * TameRep.Imprint : 1.f);
	// A hauler loaded past its share slows down (Steady: it doesn't).
	if (Kind == TEXT("pack") && TameRep.CargoKg > CargoCapacityKg() * T(TEXT("mount.overloadShare"), 0.75f) && !TameHasNode(TEXT("steady")))
	{
		Speed *= 1.f - T(TEXT("mount.overloadSlow"), 0.3f);
	}
	if (UWRCharacterMovementComponent* WRMove = Cast<UWRCharacterMovementComponent>(Move))
	{
		WRMove->SprintSpeed = Kind == TEXT("pack") ? Speed : Speed * T(TEXT("mount.sprintMult"), 1.35f);
	}
	if (Kind == TEXT("swim"))
	{
		Move->MaxWalkSpeed = Def->LandSpeed > 0.f ? Def->LandSpeed : T(TEXT("swim.landSpeed"), 250.f);
		Move->MaxSwimSpeed = Speed;
	}
	else
	{
		Move->MaxWalkSpeed = Speed;
	}
	if (Kind == TEXT("fly"))
	{
		Move->MaxFlySpeed = (Def->FlySpeed > 0.f ? Def->FlySpeed : T(TEXT("fly.speed"), 1600.f)) * StatMult(WRTameStat::Speed) * (bDiveHeld ? T(TEXT("fly.diveMult"), 1.3f) : 1.f);
		Move->BrakingDecelerationFlying = 900.f;
		// Out of the air with no stamina it glides down slowly (a falling mount, its wings spread).
		Move->GravityScale = T(TEXT("fly.glideGravity"), 0.25f);
		Move->AirControl = 0.8f;
	}
}

void AWRCreature::ServerFly_Implementation(bool bFly)
{
	if (GetMountKind() != TEXT("fly") || !Rider.IsValid()) return;
	if (bFly && (TameRep.FlyStamina <= 0.f || TameRep.bNoFly)) return;
	GetCharacterMovement()->SetMovementMode(bFly ? MOVE_Flying : MOVE_Falling);
	if (bFly) WRFx::Broadcast(this, TEXT("land.dust"), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 2.f, TEXT("mount.wings"));
}

void AWRCreature::TickMount(float DeltaSeconds)
{
	const FWRCreatureDef* Def = GetDef();
	if (!Def || !Rider.IsValid()) return;
	ApplyRideSpeeds();
	const FName Kind = FWRTaming::MountKind(*Def);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Kind == TEXT("fly"))
	{
		const bool bFlying = Move->MovementMode == MOVE_Flying;
		// The one moving it: hold Jump to climb, Dodge to dive; nothing held, it holds its height.
		if (IsLocallyControlled() && bFlying)
		{
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(WRFlyCeiling), false, this);
			Params.AddIgnoredActor(Rider.Get());
			const float Ceiling = T(TEXT("fly.ceiling"), 40000.f);
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Ground, GetActorLocation(), GetActorLocation() - FVector(0.f, 0.f, Ceiling + 5000.f), ECC_Visibility, Params);
			const float Height = bHit ? GetActorLocation().Z - Ground.ImpactPoint.Z : Ceiling + 1.f;
			if (bClimbHeld && TameRep.FlyStamina > 0.f && Height < Ceiling) AddMovementInput(FVector::UpVector, T(TEXT("fly.climbRate"), 0.6f));
			else if (bDiveHeld) AddMovementInput(FVector::DownVector, 1.f);
			if (TameRep.FlyStamina <= 0.f || TameRep.bNoFly)
			{
				Move->SetMovementMode(MOVE_Falling);   // the server does the same: it glides down
			}
		}
		if (HasAuthority())
		{
			// Stamina: climbing costs most, holding level a little, a glide or a dive nothing; on the ground it comes back.
			float& S = TameRep.FlyStamina;
			const float Max = T(TEXT("fly.stamina"), 300.f) * StatMult(WRTameStat::Stamina);
			if (bFlying)
			{
				const float Vz = GetVelocity().Z;
				S -= (Vz > 50.f ? T(TEXT("fly.climb"), 15.f) : Vz < -50.f ? 0.f : T(TEXT("fly.level"), 4.f)) * DeltaSeconds;
				if (S <= 0.f) { S = 0.f; Move->SetMovementMode(MOVE_Falling); }
			}
			else if (Move->IsMovingOnGround())
			{
				S = FMath::Min(Max, S + T(TEXT("fly.landedRegen"), 30.f) * DeltaSeconds);
			}
			// No flying near a boss arena: bosses are fought on foot or on ground mounts (it lands and won't rise).
			bool bNear = false;
			for (TActorIterator<AWRBoss> It(GetWorld()); It; ++It)
			{
				if (FVector::Dist2D(It->GetHome(), GetActorLocation()) < T(TEXT("fly.bossNoFly"), 15000.f)) { bNear = true; break; }
			}
			TameRep.bNoFly = bNear;
			if (bNear && bFlying)
			{
				Move->SetMovementMode(MOVE_Falling);
				Notify(GetWorld(), Rider->GetPlayerState() ? Rider->GetPlayerState()->GetPlayerName() : FString(), TEXT("A boss's ground: it won't fly here"));
			}
		}
	}
	// Charge (mount tree): sprinting into an enemy lowers its head (0.4 s tell), then knocks it back.
	if (HasAuthority() && TameHasNode(TEXT("charge")))
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const UWRCharacterMovementComponent* WRMove = Cast<UWRCharacterMovementComponent>(Move);
		if (ChargeStrikeAt < 0.0 && Now >= ChargeReadyAt && WRMove && WRMove->IsSprinting())
		{
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				if (*It == this || *It == Rider.Get() || !It->IsAlive() || !Rider->IsHostileTo(*It)) continue;
				const FVector To = It->GetActorLocation() - GetActorLocation();
				if (To.Size2D() - It->GetSimpleCollisionRadius() - GetSimpleCollisionRadius() < T(TEXT("mount.charge.reach"), 150.f)
					&& FVector::DotProduct(To.GetSafeNormal2D(), GetActorForwardVector()) > 0.6f)
				{
					ChargeTarget = *It;
					ChargeStrikeAt = Now + T(TEXT("mount.charge.tell"), 0.4f);
					PlaySwing(EWRSwingKind::Strike, T(TEXT("mount.charge.tell"), 0.4f));   // the lowered head, its glow and call
					break;
				}
			}
		}
		if (ChargeStrikeAt >= 0.0 && Now >= ChargeStrikeAt)
		{
			ChargeStrikeAt = -1.0;
			ChargeReadyAt = Now + T(TEXT("mount.charge.cooldown"), 4.f);
			AWRCharacterBase* Victim = ChargeTarget.Get();
			if (Victim && Victim->IsAlive() && FVector::Dist2D(Victim->GetActorLocation(), GetActorLocation()) - Victim->GetSimpleCollisionRadius() - GetSimpleCollisionRadius() < T(TEXT("mount.charge.reach"), 150.f) * 1.5f)
			{
				FWRHit Hit;
				Hit.Instigator = this;
				Hit.Damage = T(TEXT("mount.charge.damage"), 40.f);
				Hit.PoiseDamage = 60.f;
				Hit.Impact = 4;
				Hit.Tier = EWRHitTier::Heavy;
				Hit.PhysType = 2;
				Hit.bMelee = true;
				Hit.bHeavy = true;
				Hit.Element = WRTags::Element_Physical;
				Hit.Origin = GetActorLocation();
				UWRCombatLibrary::ApplyHit(Victim, Hit);
				if (Victim->IsAlive()) Victim->LaunchCharacter(GetActorForwardVector() * T(TEXT("mount.charge.knock"), 900.f) + FVector(0.f, 0.f, 250.f), true, true);
			}
		}
	}
}

// ---------------------------------------------------------------- the tame's life (server)

void AWRCreature::TickTameLife(float DeltaSeconds)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def || !IsTamed() || !IsAlive()) return;
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();

	// Every frame: distances for ride, haul and walk XP / care.
	const FVector Here = GetActorLocation();
	const float Moved = bHasLastTameAt ? FVector::Dist2D(Here, LastTameAt) : 0.f;
	LastTameAt = Here;
	bHasLastTameAt = true;
	if (Moved > 0.f && Moved < 2000.f)
	{
		if (Rider.IsValid())
		{
			RideMetres += Moved / 100.f;
			const float Per = T(TEXT("xp.rideMetres"), 50.f);
			if (RideMetres >= Per) { GainBondXP(FMath::FloorToFloat(RideMetres / Per), TEXT("ride")); RideMetres = FMath::Fmod(RideMetres, Per); }
		}
		if (CargoCapacityKg() > 0.f && TameRep.CargoKg >= CargoCapacityKg() * T(TEXT("xp.haulShare"), 0.25f))
		{
			HaulMetres += Moved / 100.f;
			const float Per = T(TEXT("xp.haulMetres"), 100.f);
			if (HaulMetres >= Per) { GainBondXP(FMath::FloorToFloat(HaulMetres / Per), TEXT("haul")); HaulMetres = FMath::Fmod(HaulMetres, Per); }
		}
		if (TameRep.Care == 2)
		{
			// Walk it: it has to come along beside whoever's imprinting it (anyone of its owner's, the first time).
			const FString Who = TameRep.Imprinter.IsEmpty() ? TamedBy : TameRep.Imprinter;
			const AActor* Body = BodyOf(World, Who);
			if (Body && FVector::Dist(Body->GetActorLocation(), Here) < T(TEXT("imprint.walkBeside"), 1000.f))
			{
				TameRep.CareWalked += Moved / 100.f;
				if (TameRep.CareWalked >= T(TEXT("imprint.walkMetres"), 50.f)) AnswerCare(Who);
			}
		}
	}
	// A cuddle that's run its 2 s.
	if (CuddleUntil > 0.0 && Now >= CuddleUntil)
	{
		CuddleUntil = -1.0;
		const AActor* Body = BodyOf(World, CuddleBy);
		if (TameRep.Care == 3 && Body && FVector::Dist(Body->GetActorLocation(), Here) < T(TEXT("imprint.cuddleRange"), 350.f)) AnswerCare(CuddleBy);
		else Notify(World, CuddleBy, TEXT("You moved away: stay beside it for the cuddle"));
	}

	TameClock += DeltaSeconds;
	if (TameClock < 1.f) return;
	const float Dt = TameClock;
	TameClock = 0.f;

	// Nothing ticks with nobody near (the world's own rule): a tame left at home doesn't starve or age while you're away.
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It; ++It)
	{
		if (It->GetPlayerState() && FVector::Dist(It->GetActorLocation(), Here) < T(TEXT("hunger.activeRange"), 25000.f)) { bNear = true; break; }
	}
	if (!bNear) return;
	const float Mat = FWRTaming::Maturation();

	// Hunger: it eats its food from its packs, then a pen's trough within 20 m; empty, it stops recovering stamina,
	// then wastes away (through ApplyHit, a starving tame can die).
	float& Hunger = TameRep.Hunger;
	Hunger = FMath::Max(0.f, Hunger - Dt * 100.f / FWRTaming::HungerSeconds(*Def) * (TameHasNode(TEXT("content")) ? 0.75f : 1.f));
	if (Hunger < T(TEXT("hunger.eatBelow"), 60.f))
	{
		bool bAte = false;
		for (FWRItemStack& S : Cargo)
		{
			if (!S.IsEmpty() && IsItsFood(*Def, S.ItemId)) { --S.Count; if (S.Count <= 0) S = FWRItemStack(); bAte = true; break; }
		}
		if (!bAte)
		{
			for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
			{
				const FWRPieceDef* PDef = It->GetDef();
				if (!It->bGhost && PDef && PDef->Kind == TEXT("pen") && It->Feed > 0 && FVector::Dist(It->GetActorLocation(), Here) < T(TEXT("hunger.troughRange"), 2000.f))
				{
					--It->Feed;
					It->OnRep_Farm();
					bAte = true;
					break;
				}
			}
		}
		if (bAte)
		{
			Hunger = FMath::Min(100.f, Hunger + T(TEXT("hunger.perFood"), 25.f));
			TameRep.CargoKg = CargoKg();
			StarvingSince = -1.0;
		}
	}
	if (Hunger <= 0.f)
	{
		if (StarvingSince < 0.0)
		{
			StarvingSince = Now;
			Notify(World, TamedBy, FString::Printf(TEXT("Your %s is starving: put its food in its packs or a pen's trough nearby"), *Def->Name));
		}
		if (Now - StarvingSince > T(TEXT("hunger.starveMinutes"), 30.f) * 60.f && GetAttributes())
		{
			FWRHit Starve;
			Starve.Damage = GetAttributes()->GetMaxHealth() * T(TEXT("hunger.starveHpPerMinute"), 0.01f) * Dt / 60.f;
			Starve.bDamageOverTime = true;
			Starve.Element = WRTags::Element_Physical;
			Starve.Origin = Here;
			UWRCombatLibrary::ApplyHit(this, Starve);
		}
	}
	if (!IsAlive()) return;

	// Growing up: 10% to full size over its time; a care prompt every eighth.
	if (TameRep.IsBaby())
	{
		const float Start = T(TEXT("breed.babyStart"), 0.1f);
		const float Seconds = FWRTaming::ByDifficulty(*Def, TEXT("breed.growth"), 7200.f, 28800.f, 86400.f);
		const float Was = TameRep.Growth;
		TameRep.Growth = FMath::Min(1.f, TameRep.Growth + Dt * Mat * (1.f - Start) / FMath::Max(1.f, Seconds));
		SetActorScale3D(FVector(TameRep.Growth));
		if (FMath::FloorToInt(Was * 8.f) != FMath::FloorToInt(TameRep.Growth * 8.f) || TameRep.Growth >= 1.f) RecomputeStats();
		const int32 Slice = FMath::Min(7, FMath::FloorToInt((TameRep.Growth - Start) / FMath::Max(0.01f, 1.f - Start) * 8.f));
		if (Slice > CareSlice)
		{
			if (TameRep.Care != 0 && CareSlice >= 0) Notify(World, TamedBy, FString::Printf(TEXT("Your baby %s's last care went unanswered"), *Def->Name));
			CareSlice = Slice;
			TameRep.Care = uint8(1 + FMath::RandHelper(3));
			TameRep.CareWalked = 0.f;
			TameRep.CareFood = Def->Food.Num() > 0 ? Def->Food[FMath::RandHelper(Def->Food.Num())] : FName(TEXT("kibble"));
			static const TCHAR* Asks[] = { TEXT(""), TEXT("is hungry for "), TEXT("wants a walk (50 m beside you)"), TEXT("wants a cuddle") };
			const FWRItemDef* Food = FWRItemRegistry::Get().Find(TameRep.CareFood);
			const FString Ask = TameRep.Care == 1 ? FString(Asks[1]) + (Food ? Food->Name : TameRep.CareFood.ToString()) : FString(Asks[TameRep.Care]);
			Notify(World, TameRep.Imprinter.IsEmpty() ? TamedBy : TameRep.Imprinter, FString::Printf(TEXT("Your baby %s %s"), *Def->Name, *Ask));
			MulticastSound(TEXT("voice.idle"));
		}
		if (TameRep.Growth >= 1.f)
		{
			SetActorScale3D(FVector(1.f));
			Notify(World, TamedBy, FString::Printf(TEXT("Your %s is grown (imprint %.0f%%)"), *Def->Name, TameRep.Imprint * 100.f));
		}
	}

	// Breeding clocks: her rest after mating, the young she carries.
	if (TameRep.Cooldown > 0.f) TameRep.Cooldown = FMath::Max(0.f, TameRep.Cooldown - Dt * Mat * (TameHasNode(TEXT("fertile")) ? 1.5f : 1.f));
	if (TameRep.Pregnant > 0.f)
	{
		TameRep.Pregnant = FMath::Max(0.f, TameRep.Pregnant - Dt * Mat);
		if (TameRep.Pregnant <= 0.f)
		{
			const APlayerState* PS = PlayerNamed(World, TamedBy);
			const int32 Young = bPregnantTwins ? 2 : 1;
			if (PS && CountTames(World, TamedBy) + Young > TameLimitFor(PS))
			{
				TameRep.Pregnant = 1.f;   // she waits for room
				if (!bBirthHeldNotified) Notify(World, TamedBy, FString::Printf(TEXT("Your %s is ready to give birth: make room under your tame limit"), *Def->Name));
				bBirthHeldNotified = true;
			}
			else
			{
				bBirthHeldNotified = false;
				for (int32 i = 0; i < Young; ++i)
				{
					FWRTameGenes G = PregnantGenes;
					if (i > 0) G.Sex = FMath::RandBool() ? 1 : 2;
					const FVector At = Here - GetActorForwardVector() * (GetSimpleCollisionRadius() + 80.f) + GetActorRightVector() * (i * 120.f - 60.f * (Young - 1));
					FWRTaming::Hatch(World, Def->Id, G, TamedBy, At, T(TEXT("breed.babyStart"), 0.1f));
				}
				TameRep.PregnantTotal = 0.f;
				bPregnantTwins = false;
				WRFx::Broadcast(this, TEXT("tame.hatch"), Here, FRotator::ZeroRotator, 1.f, TEXT("tame.birth"));
				UWRLifeSkillsComponent::Award(PS, TEXT("taming"), TEXT("tameBirth"), T(TEXT("skillXp.birth"), 40.f));
				Notify(World, TamedBy, FString::Printf(TEXT("Your %s gave birth"), *Def->Name));
			}
		}
	}

	// Utility work beyond the built ore digging: a light tame lighting its owner's dark (bond XP every 5 min).
	if (Def->Utility == TEXT("light"))
	{
		const AActor* Body = BodyOf(World, TamedBy);
		const bool bDark = Body && (WRBiome::IsUnderground(this, Body->GetActorLocation()) || AWRGameState::IsNightIn(this));
		if (bDark && FVector::Dist(Body->GetActorLocation(), Here) < WRTune(TEXT("lights.creature.radius"), 1200.f) * (TameHasNode(TEXT("bright")) ? 1.5f : 1.f))
		{
			LightClock += Dt;
			if (LightClock >= T(TEXT("xp.lightMinutes"), 5.f) * 60.f) { LightClock = 0.f; GainBondXP(1.f, TEXT("light")); }
		}
	}
	TameRep.CargoKg = CargoKg();
}

void AWRCreature::AnswerCare(const FString& Player)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || TameRep.Care == 0 || !Def) return;
	if (!TameRep.Imprinter.IsEmpty() && TameRep.Imprinter != Player) return;
	TameRep.Imprinter = Player;
	TameRep.Imprint = FMath::Min(1.f, TameRep.Imprint + 1.f / FMath::Max(1.f, T(TEXT("imprint.prompts"), 8.f)));
	TameRep.Care = 0;
	TameRep.CareWalked = 0.f;
	WRFx::Broadcast(this, TEXT("tame.heart"), GetActorLocation() + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("tame.care"));
	MulticastSound(TEXT("voice.idle"));
	UWRLifeSkillsComponent::Award(PlayerNamed(GetWorld(), Player), TEXT("taming"), TEXT("tameImprint"), T(TEXT("skillXp.imprint"), 10.f));
	Notify(GetWorld(), Player, FString::Printf(TEXT("%s imprint %.0f%%"), *Def->Name, TameRep.Imprint * 100.f));
}

FString AWRCreature::StartCuddle(AWRPlayerController* Player)
{
	const APlayerState* PS = Player ? Player->GetPlayerState<APlayerState>() : nullptr;
	if (!PS || TameRep.Care != 3) return TEXT("It isn't asking for a cuddle");
	if (!TameRep.Imprinter.IsEmpty() && TameRep.Imprinter != PS->GetPlayerName()) return FString::Printf(TEXT("It's imprinting on %s"), *TameRep.Imprinter);
	const AActor* Body = BodyOf(GetWorld(), PS->GetPlayerName());
	if (!Body || FVector::Dist(Body->GetActorLocation(), GetActorLocation()) > T(TEXT("imprint.cuddleRange"), 350.f)) return TEXT("Go right up to it to cuddle it");
	CuddleBy = PS->GetPlayerName();
	CuddleUntil = GetWorld()->GetTimeSeconds() + T(TEXT("imprint.cuddleSeconds"), 2.f);
	if (AWRCharacter* Hero = Cast<AWRCharacter>(const_cast<AActor*>(Body))) Hero->MulticastBodyOneShot(TEXT("pickup"));
	MulticastSound(TEXT("voice.idle"));
	return TEXT("Cuddling... stay beside it");
}

void AWRCreature::DropTack()
{
	if (!HasAuthority()) return;
	TArray<FWRItemStack> Items;
	if (!TameRep.Saddle.IsEmpty()) Items.Add(TameRep.Saddle);
	if (!TameRep.Barding.IsEmpty()) Items.Add(TameRep.Barding);
	for (const FWRItemStack& S : Cargo) if (!S.IsEmpty()) Items.Add(S);
	TameRep.Saddle = FWRItemStack();
	TameRep.Barding = FWRItemStack();
	Cargo.Reset();
	TameRep.CargoKg = 0.f;
	if (Items.Num() > 0)
	{
		AWRLootBag::Spill(GetWorld(), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), TamedBy, Items);
	}
	RecomputeStats();
	OnRep_TameRep();
}

// ---------------------------------------------------------------- combat hooks

float FWRTaming::AttackerScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target)
{
	float Scale = 1.f;
	if (const AWRCreature* Tame = Cast<AWRCreature>(Attacker); Tame && Tame->IsTamed())
	{
		const UWRAttributeSet* A = Tame->GetAttributes();
		if (A && A->GetMaxHealth() > 0.f && A->GetHealth() / A->GetMaxHealth() < T(TEXT("tree.berserkBelow"), 0.3f) && Tame->TameHasNode(TEXT("berserk"))) Scale *= 1.4f;
		if (Tame->TameHasNode(TEXT("pack_tactics")))
		{
			const UWRTamingSubsystem* Sub = UWRTamingSubsystem::Get(Tame);
			const int32 Others = Sub ? FMath::Min(Sub->OthersOn(Attacker, Target, T(TEXT("tree.packWindow"), 3.f)), int32(T(TEXT("tree.packMax"), 3.f))) : 0;
			Scale *= 1.f + T(TEXT("tree.packPer"), 0.1f) * Others;
		}
		Scale *= Tame->ImprintScale();
	}
	// The rider's blow from a Warhorse's saddle.
	if (const AWRCharacter* Hero = Cast<AWRCharacter>(Attacker); Hero && Hero->GetAttachParentActor())
	{
		if (const AWRCreature* Mount = Cast<AWRCreature>(Hero->GetAttachParentActor()); Mount && Mount->GetRider() == Hero && Mount->TameHasNode(TEXT("warhorse"))) Scale *= 1.2f;
	}
	return Scale;
}

float FWRTaming::PlayerTakes(AWRCharacterBase* Player, const FWRHit& Hit, float Damage)
{
	if (!Player || !Player->GetPlayerState() || Hit.bDamageOverTime || Damage <= 0.f) return Damage;
	const FString Name = Player->GetPlayerState()->GetPlayerName();
	UWorld* World = Player->GetWorld();
	// Bulwark: a hauler of yours or your clan's within 6 m.
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (It->IsAlive() && Allies(It->TamedBy, Name) && It->TameHasNode(TEXT("bulwark"))
			&& FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) < T(TEXT("tree.bulwarkRange"), 600.f))
		{
			Damage *= 1.f - T(TEXT("tree.bulwarkCut"), 0.1f);
			break;
		}
	}
	// Guardian: a fighter of yours within 4 m takes its share of the blow (through ApplyHit, so it can fall for you).
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		AWRCreature* G = *It;
		if (G->IsAlive() && G->TamedBy == Name && !G->GetRider() && G != Hit.Instigator && G->TameHasNode(TEXT("guardian"))
			&& FVector::Dist(G->GetActorLocation(), Player->GetActorLocation()) < T(TEXT("tree.guardianRange"), 400.f))
		{
			const float Share = Damage * T(TEXT("tree.guardianShare"), 0.3f);
			FWRHit Taken;
			Taken.Instigator = Hit.Instigator;
			Taken.Damage = Share;
			Taken.Element = Hit.Element;
			Taken.bDamageOverTime = true;   // a share passed on, not a blow of its own (no reaction, no second share)
			Taken.Origin = Hit.Origin;
			UWRCombatLibrary::ApplyHit(G, Taken);
			return Damage - Share;
		}
	}
	return Damage;
}

void FWRTaming::AfterHit(AWRCharacterBase* Attacker, AWRCharacterBase* Target, float Damage, bool bDies)
{
	if (!Attacker || !Target || Attacker == Target) return;
	UWRTamingSubsystem* Sub = UWRTamingSubsystem::Get(Target);
	if (Sub) Sub->NoteHit(Attacker, Target);
	if (AWRCreature* Tame = Cast<AWRCreature>(Attacker); Tame && Tame->IsTamed())
	{
		Tame->GainBondXP(Damage * T(TEXT("xp.perDamage"), 0.1f), TEXT("fight"));
	}
	else if (AWRCharacter* Hero = Cast<AWRCharacter>(Attacker))
	{
		// From the saddle, the mount shares the fight.
		if (AWRCreature* Mount = Cast<AWRCreature>(Hero->GetAttachParentActor()); Mount && Mount->GetRider() == Hero)
		{
			Mount->GainBondXP(Damage * T(TEXT("xp.perDamage"), 0.1f) * 0.5f, TEXT("fight (ridden)"));
		}
	}
	if (bDies && Sub)
	{
		const AWRCreature* Body = Cast<AWRCreature>(Target);
		const FWRCreatureDef* Def = Body ? Body->GetDef() : nullptr;
		if (Def && !Body->IsTamed())
		{
			for (AWRCreature* Helper : Sub->TamesOn(Target, T(TEXT("xp.killWindow"), 10.f))) Helper->GainBondXP(KillXP(*Def), TEXT("kill"));
		}
	}
}

float FWRTaming::StanceTaken(const AWRCharacterBase* Target, float Stance)
{
	const AWRCreature* Tame = Cast<AWRCreature>(Target);
	return Tame && Tame->TameHasNode(TEXT("stalwart")) ? Stance * 0.6f : Stance;
}

bool FWRTaming::KeepsSeat(const AWRCreature* Mount, const FWRHit& Hit, float Damage)
{
	if (!Mount || !Mount->TameHasNode(TEXT("steady_seat"))) return false;
	const float Stance = Hit.StanceDamage >= 0.f ? Hit.StanceDamage : Damage * WRTune(TEXT("stance.creaturePerDamage"), 0.3f);
	return Stance < T(TEXT("tree.steadyStance"), 80.f);
}

int32 FWRTaming::Forage(const AActor* Owner, FName ItemId, int32 Count)
{
	const APlayerState* PS = Cast<APlayerState>(Owner);
	const AActor* Body = PS ? BodyOf(PS->GetWorld(), PS->GetPlayerName()) : nullptr;
	if (!Body || Count <= 0) return Count;
	if (AWRCreature* Hauler = TameWithNode(Body->GetWorld(), PS->GetPlayerName(), TEXT("forager"), Body->GetActorLocation(), T(TEXT("tree.foragerRange"), 800.f)))
	{
		return Hauler->AddCargo(ItemId, Count);
	}
	return Count;
}

float FWRTaming::GatherScale(const AActor* Player)
{
	const AWRCharacterBase* P = Cast<AWRCharacterBase>(Player);
	const FString Name = P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	if (Name.IsEmpty()) return 1.f;
	for (TActorIterator<AWRCreature> It(P->GetWorld()); It; ++It)
	{
		if (It->IsAlive() && Allies(It->TamedBy, Name) && It->TameHasNode(TEXT("lodestar"))
			&& FVector::Dist(It->GetActorLocation(), P->GetActorLocation()) < T(TEXT("tree.lodestarRange"), 1000.f))
		{
			return 1.15f;
		}
	}
	return 1.f;
}

void FWRTaming::Sense(AWRCreature* Tame)
{
	const FWRCreatureDef* Def = Tame ? Tame->GetDef() : nullptr;
	const AActor* Body = Def ? BodyOf(Tame->GetWorld(), Tame->TamedBy) : nullptr;
	if (!Body || FVector::Dist(Body->GetActorLocation(), Tame->GetActorLocation()) > 3000.f) return;
	static TMap<TPair<FString, TWeakObjectPtr<AActor>>, double> Told;
	const double Now = Tame->GetWorld()->GetTimeSeconds();
	const float Range = T(TEXT("sense.range"), 8000.f) * (Tame->TameHasNode(TEXT("keen_sense")) ? 1.5f : 1.f);
	AActor* Best = nullptr;
	float BestD = Range;
	FString What;
	const auto Consider = [&](AActor* A, const TCHAR* Kind)
	{
		const float D = FVector::Dist2D(A->GetActorLocation(), Body->GetActorLocation());
		const double* When = Told.Find({ Tame->TamedBy, A });
		if (D < BestD && (!When || Now - *When > T(TEXT("sense.repeatSeconds"), 600.f))) { BestD = D; Best = A; What = Kind; }
	};
	for (TActorIterator<AWRCave> It(Tame->GetWorld()); It; ++It) Consider(*It, TEXT("a cave"));
	for (TActorIterator<AWRNest> It(Tame->GetWorld()); It; ++It) if (It->bHasEgg) Consider(*It, TEXT("an egg nest"));
	if (!Best) return;
	Told.Add({ Tame->TamedBy, Best }, Now);
	const FVector D = Best->GetActorLocation() - Body->GetActorLocation();
	static const TCHAR* Dirs[] = { TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"), TEXT("south"), TEXT("south-west"), TEXT("west"), TEXT("north-west") };
	// The map's north is -Y (Unreal's X east, Y south).
	const float Bearing = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)) + 360.f + 22.5f, 360.f);
	const FString Line = FString::Printf(TEXT("%s senses %s %.0f m %s"), *Def->Name, *What, D.Size2D() / 100.f, Dirs[FMath::Clamp(int32(Bearing / 45.f), 0, 7)]);
	Notify(Tame->GetWorld(), Tame->TamedBy, TEXT("Your ") + Line);
	if (Tame->TameHasNode(TEXT("seeker")))
	{
		const FString Clan = FWRClans::Get().ClanOf(Tame->TamedBy);
		for (const FString& M : Clan.IsEmpty() ? TArray<FString>() : FWRClans::Get().Members(Clan)) if (M != Tame->TamedBy) Notify(Tame->GetWorld(), M, FString::Printf(TEXT("%s's %s"), *Tame->TamedBy, *Line));
	}
	Tame->GainBondXP(T(TEXT("xp.dig"), 3.f), TEXT("sense"));
}

AWRCreature* FWRTaming::TameWithNode(const UWorld* World, const FString& Owner, FName Node, const FVector& Near, float Range)
{
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (It->IsAlive() && It->TamedBy == Owner && It->TameHasNode(Node) && FVector::Dist(It->GetActorLocation(), Near) < Range) return *It;
	}
	return nullptr;
}

// ---------------------------------------------------------------- hatching, the save

AWRCreature* FWRTaming::Hatch(UWorld* World, FName Species, const FWRTameGenes& Genes, const FString& Owner, const FVector& At, float Growth)
{
	AWRCreature* C = AWRCreature::SpawnCreature(World, Species, At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	if (!C) return nullptr;
	C->TameRep.Growth = FMath::Clamp(Growth, 0.05f, 1.f);
	C->CompleteTame(Owner, false);
	if (Genes.bValid) C->InitGenes(Genes);
	else C->RollWildGenes();
	C->SetActorScale3D(FVector(C->TameRep.Growth));
	C->TameRep.Hunger = 100.f;
	C->RecomputeStats();
	C->SetHealthFraction(1.f);
	C->OnRep_TameRep();
	UE_LOG(LogWildreach, Log, TEXT("TAME %s born to %s (level %d, %s)"), *C->GetName(), *Owner, C->Genes.Level(), C->Genes.Sex == 2 ? TEXT("female") : TEXT("male"));
	return C;
}

TSharedRef<FJsonObject> FWRTaming::SaveTame(const AWRCreature* Tame)
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	const FWRTameRep& R = Tame->TameRep;
	O->SetObjectField(TEXT("genes"), Tame->Genes.ToJson());
	O->SetNumberField(TEXT("bond"), R.Bond);
	O->SetNumberField(TEXT("bondXP"), R.BondXP);
	O->SetNumberField(TEXT("nodes"), R.Nodes);
	if (!R.Saddle.IsEmpty()) O->SetObjectField(TEXT("saddle"), FWRInventoryRules::StackToJson(R.Saddle));
	if (!R.Barding.IsEmpty()) O->SetObjectField(TEXT("barding"), FWRInventoryRules::StackToJson(R.Barding));
	O->SetNumberField(TEXT("imprint"), R.Imprint);
	O->SetStringField(TEXT("imprinter"), R.Imprinter);
	O->SetNumberField(TEXT("growth"), R.Growth);
	O->SetNumberField(TEXT("care"), R.Care);
	O->SetStringField(TEXT("careFood"), R.CareFood.ToString());
	O->SetNumberField(TEXT("careWalked"), R.CareWalked);
	O->SetNumberField(TEXT("careSlice"), Tame->CareSlice);
	O->SetBoolField(TEXT("mating"), R.bMating);
	O->SetNumberField(TEXT("cooldown"), R.Cooldown);
	O->SetNumberField(TEXT("pregnant"), R.Pregnant);
	O->SetNumberField(TEXT("pregnantTotal"), R.PregnantTotal);
	if (R.Pregnant > 0.f)
	{
		O->SetObjectField(TEXT("pregnantGenes"), Tame->PregnantGenes.ToJson());
		O->SetBoolField(TEXT("twins"), Tame->bPregnantTwins);
	}
	O->SetNumberField(TEXT("hunger"), R.Hunger);
	O->SetNumberField(TEXT("flyStamina"), R.FlyStamina);
	O->SetNumberField(TEXT("health"), Tame->GetHealthFraction());
	TArray<TSharedPtr<FJsonValue>> Cargo;
	for (const FWRItemStack& S : Tame->Cargo) Cargo.Add(MakeShared<FJsonValueObject>(FWRInventoryRules::StackToJson(S)));
	O->SetArrayField(TEXT("cargo"), Cargo);
	return O;
}

void FWRTaming::LoadTame(AWRCreature* Tame, const TSharedPtr<FJsonObject>& O)
{
	if (!Tame || !O.IsValid()) return;
	FWRTameRep& R = Tame->TameRep;
	const TSharedPtr<FJsonObject>* GenesObj = nullptr;
	double N = 0.0;
	FString S;
	if (O->TryGetNumberField(TEXT("bond"), N)) R.Bond = uint8(N);
	if (O->TryGetNumberField(TEXT("bondXP"), N)) R.BondXP = N;
	if (O->TryGetNumberField(TEXT("nodes"), N)) R.Nodes = uint8(N);
	const TSharedPtr<FJsonObject>* Stack = nullptr;
	if (O->TryGetObjectField(TEXT("saddle"), Stack)) R.Saddle = FWRInventoryRules::StackFromJson(*Stack);
	if (O->TryGetObjectField(TEXT("barding"), Stack)) R.Barding = FWRInventoryRules::StackFromJson(*Stack);
	if (O->TryGetNumberField(TEXT("imprint"), N)) R.Imprint = N;
	O->TryGetStringField(TEXT("imprinter"), R.Imprinter);
	if (O->TryGetNumberField(TEXT("growth"), N)) R.Growth = FMath::Clamp(float(N), 0.05f, 1.f);
	if (O->TryGetNumberField(TEXT("care"), N)) R.Care = uint8(N);
	if (O->TryGetStringField(TEXT("careFood"), S)) R.CareFood = FName(*S);
	if (O->TryGetNumberField(TEXT("careWalked"), N)) R.CareWalked = N;
	if (O->TryGetNumberField(TEXT("careSlice"), N)) Tame->CareSlice = int32(N);
	O->TryGetBoolField(TEXT("mating"), R.bMating);
	if (O->TryGetNumberField(TEXT("cooldown"), N)) R.Cooldown = N;
	if (O->TryGetNumberField(TEXT("pregnant"), N)) R.Pregnant = N;
	if (O->TryGetNumberField(TEXT("pregnantTotal"), N)) R.PregnantTotal = N;
	if (O->TryGetObjectField(TEXT("pregnantGenes"), GenesObj)) Tame->PregnantGenes = FWRTameGenes::FromJson(*GenesObj);
	O->TryGetBoolField(TEXT("twins"), Tame->bPregnantTwins);
	if (O->TryGetNumberField(TEXT("hunger"), N)) R.Hunger = N;
	if (O->TryGetNumberField(TEXT("flyStamina"), N)) R.FlyStamina = N;
	const TArray<TSharedPtr<FJsonValue>>* Cargo = nullptr;
	Tame->Cargo.Reset();
	if (O->TryGetArrayField(TEXT("cargo"), Cargo))
	{
		for (const TSharedPtr<FJsonValue>& V : *Cargo) Tame->Cargo.Add(FWRInventoryRules::StackFromJson(V->AsObject()));
	}
	// Genes last: they set the stats with the bond, tree and tack above in place. Older saves had none: wild ones.
	if (O->TryGetObjectField(TEXT("genes"), GenesObj)) Tame->InitGenes(FWRTameGenes::FromJson(*GenesObj));
	if (!Tame->Genes.bValid) Tame->RollWildGenes();
	Tame->SetActorScale3D(FVector(R.Growth));
	Tame->RecomputeStats();
	if (O->TryGetNumberField(TEXT("health"), N) && N > 0.0) Tame->SetHealthFraction(N);
	R.CargoKg = Tame->CargoKg();
	Tame->OnRep_TameRep();
}

// ---------------------------------------------------------------- the card's commands

FString FWRTaming::CardCommand(AWRPlayerController* Player, AWRCreature* Tame, const TArray<FString>& Args)
{
	const APlayerState* PS = Player ? Player->GetPlayerState<APlayerState>() : nullptr;
	const FWRCreatureDef* Def = Tame ? Tame->GetDef() : nullptr;
	if (!PS || !Def || !Tame->IsTamed() || !Tame->IsAlive()) return TEXT("no tame of yours nearby");
	const FString Me = PS->GetPlayerName();
	const FString Verb = Args.IsValidIndex(0) ? Args[0] : FString();
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? FCString::Atoi(*Args[i]) : -1; };
	UWRInventoryComponent* Bag = UWRInventoryComponent::FindFor(PS);
	FWRTameRep& R = Tame->TameRep;
	// Care is anyone's of its owner's clan; everything else is its owner's.
	if (Verb == TEXT("care"))
	{
		if (!Allies(Tame->TamedBy, Me)) return TEXT("not your clan's tame");
		if (R.Care == 1)
		{
			if (!R.Imprinter.IsEmpty() && R.Imprinter != Me) return FString::Printf(TEXT("It's imprinting on %s"), *R.Imprinter);
			if (!Bag || Bag->CountOf(R.CareFood) < 1)
			{
				const FWRItemDef* Food = FWRItemRegistry::Get().Find(R.CareFood);
				return FString::Printf(TEXT("It wants %s: you have none"), Food ? *Food->Name : *R.CareFood.ToString());
			}
			Bag->RemoveItem(R.CareFood, 1);
			Tame->MulticastSound(TEXT("eat"));
			Tame->AnswerCare(Me);
			return TEXT("Fed");
		}
		if (R.Care == 2) return FString::Printf(TEXT("Walk with it: %.0f of %.0f m"), R.CareWalked, T(TEXT("imprint.walkMetres"), 50.f));
		if (R.Care == 3) return Tame->StartCuddle(Player);
		return TEXT("It doesn't need anything now");
	}
	if (Tame->TamedBy != Me) return TEXT("not your tame");
	const bool bOwnPS = true;
	(void)bOwnPS;
	if (Verb == TEXT("learn"))
	{
		const FWRTameTree* Tree = FWRTameTrees::Get().For(*Def);
		const int32 i = Arg(1);
		if (!Tree || !Tree->Nodes.IsValidIndex(i)) return TEXT("no such skill");
		if (R.HasNode(i)) return TEXT("already learned");
		if (i >= 1 && i <= 3 && !R.HasNode(i - 1)) return FString::Printf(TEXT("Learn %s first"), *Tree->Nodes[i - 1].Name);
		if (i >= 4)
		{
			for (int32 k = 0; k < 4; ++k) if (!R.HasNode(k)) return TEXT("A capstone needs the four skills before it");
			if (R.HasNode(i == 4 ? 5 : 4)) return TEXT("It has the other capstone: only one");
		}
		int32 Spent = 0;
		for (int32 k = 0; k < Tree->Nodes.Num(); ++k) if (R.HasNode(k)) Spent += Tree->Nodes[k].Cost;
		if (R.Bond - Spent < Tree->Nodes[i].Cost) return FString::Printf(TEXT("Needs %d skill points (%d free): bond levels give them"), Tree->Nodes[i].Cost, FMath::Max(0, R.Bond - Spent));
		R.Nodes |= uint8(1 << i);
		Tame->RecomputeStats();
		WRFx::Broadcast(Tame, TEXT("tame.levelup"), Tame->GetActorLocation(), FRotator::ZeroRotator, 0.7f, TEXT("tame.levelup"));
		return FString::Printf(TEXT("%s learned %s"), *Def->Name, *Tree->Nodes[i].Name);
	}
	if (Verb == TEXT("saddle") || Verb == TEXT("barding"))
	{
		const int32 Slot = Arg(1);
		if (!Bag || !Bag->GetSlots().IsValidIndex(Slot) || Bag->GetSlots()[Slot].IsEmpty()) return TEXT("nothing in that slot");
		const FWRItemStack Item = Bag->GetSlots()[Slot];
		const FWRTackDef* Tk = Tack(Item.ItemId);
		const FName Kind = MountKind(*Def);
		if (!Tk) return TEXT("that isn't tack");
		if (Tame->IsBaby()) return TEXT("Not on a baby: wait till it's grown");
		if (Kind.IsNone()) return FString::Printf(TEXT("A %s can't be ridden or packed"), *Def->Name);
		const bool bBarding = Verb == TEXT("barding");
		if (bBarding != (Tk->Kind == TEXT("barding"))) return bBarding ? TEXT("That's a saddle") : TEXT("That's barding");
		if (!bBarding && (Tk->Kind == TEXT("pack")) != (Kind == TEXT("pack"))) return Kind == TEXT("pack") ? TEXT("Haulers wear a pack saddle") : TEXT("A pack saddle is for haulers");
		FWRItemStack& Worn = bBarding ? R.Barding : R.Saddle;
		const FWRItemStack Old = Worn;
		Bag->GetSlotsMutable()[Slot] = FWRItemStack();
		Worn = Item;
		Worn.Count = 1;
		if (Item.Count > 1) { FWRItemStack Rest = Item; Rest.Count = Item.Count - 1; Bag->GetSlotsMutable()[Slot] = Rest; }
		if (!Old.IsEmpty() && !Bag->AddInstance(Old)) AWRLootBag::Spill(Tame->GetWorld(), Tame->GetActorLocation(), Me, { Old });
		Tame->RecomputeStats();
		Tame->OnRep_TameRep();
		Tame->MulticastSound(TEXT("tame.saddle"));
		const FWRItemDef* ItemDef = FWRItemRegistry::Get().Find(Item.ItemId);
		return FString::Printf(TEXT("%s fitted (%.0f%% less damage to it)"), ItemDef ? *ItemDef->Name : TEXT("Tack"), TackValue(Worn, Player->GetPlayerState<APlayerState>()) * 100.f);
	}
	if (Verb == TEXT("unsaddle") || Verb == TEXT("unbarding"))
	{
		FWRItemStack& Worn = Verb == TEXT("unbarding") ? R.Barding : R.Saddle;
		if (Worn.IsEmpty()) return TEXT("nothing fitted");
		if (Verb == TEXT("unsaddle"))
		{
			const FWRTackDef* Tk = Tack(Worn.ItemId);
			if (Tk && Tk->Kind == TEXT("pack"))
			{
				int32 Used = 0;
				for (const FWRItemStack& S : Tame->Cargo) Used += S.IsEmpty() ? 0 : 1;
				if (Used > Tame->CargoSlots() - Tk->Slots) return TEXT("Empty its packs first");
			}
			if (Tame->GetRider()) return TEXT("Get off first");
		}
		if (!Bag || !Bag->AddInstance(Worn)) return TEXT("No room in your bag");
		Worn = FWRItemStack();
		Tame->Cargo.SetNum(FMath::Min(Tame->Cargo.Num(), FMath::Max(Tame->CargoSlots(), 0)));
		Tame->RecomputeStats();
		Tame->OnRep_TameRep();
		Tame->MulticastSound(TEXT("tame.saddle"));
		return TEXT("Taken off");
	}
	if (Verb == TEXT("mate"))
	{
		if (Tame->IsBaby()) return TEXT("Babies don't mate");
		R.bMating = Args.IsValidIndex(1) ? Args[1] == TEXT("on") : !R.bMating;
		if (!R.bMating) R.MateBar = 0.f;
		return R.bMating ? FString::Printf(TEXT("%s will mate with a %s of the other sex within 10 m (both fed)"), *Def->Name, *Def->Name) : TEXT("Mating off");
	}
	if (Verb == TEXT("feed"))
	{
		TArray<FName> Foods = { FName(TEXT("kibble")) };
		Foods.Append(Def->Food);
		for (const FName& F : Foods)
		{
			if (Bag && Bag->CountOf(F) > 0)
			{
				Bag->RemoveItem(F, 1);
				R.Hunger = FMath::Min(100.f, R.Hunger + T(TEXT("hunger.perFood"), 25.f));
				Tame->MulticastSound(TEXT("eat"));
				return FString::Printf(TEXT("Fed: hunger %.0f%%"), R.Hunger);
			}
		}
		const FWRItemDef* Food = Def->Food.Num() > 0 ? FWRItemRegistry::Get().Find(Def->Food[0]) : nullptr;
		return FString::Printf(TEXT("It eats %s or kibble"), Food ? *Food->Name : TEXT("its own food"));
	}
	if (Verb == TEXT("put") || Verb == TEXT("take"))
	{
		if (Tame->CargoSlots() <= 0) return TEXT("It has no packs");
		if (Tame->Cargo.Num() < Tame->CargoSlots()) Tame->Cargo.SetNum(Tame->CargoSlots());
		const int32 Slot = Arg(1);
		const int32 Want = FMath::Max(1, Arg(2));
		if (Verb == TEXT("put"))
		{
			if (!Bag || !Bag->GetSlots().IsValidIndex(Slot) || Bag->GetSlots()[Slot].IsEmpty()) return TEXT("nothing in that slot");
			FWRItemStack& From = Bag->GetSlotsMutable()[Slot];
			const FWRItemDef* Item = FWRItemRegistry::Get().Find(From.ItemId);
			const float Room = Tame->CargoCapacityKg() - Tame->CargoKg();
			const int32 ByWeight = Item && Item->Weight > 0.f ? FMath::FloorToInt(Room / Item->Weight) : Want;
			const int32 N = FMath::Min3(Want, From.Count, ByWeight);
			if (N <= 0) return TEXT("Its packs are full (weight)");
			const int32 Moved = FWRInventoryRules::MoveFrom(From, Tame->Cargo, N);
			if (From.Count <= 0) From = FWRItemStack();
			R.CargoKg = Tame->CargoKg();
			return Moved > 0 ? FString::Printf(TEXT("Packed %d"), Moved) : TEXT("Its packs are full");
		}
		if (!Tame->Cargo.IsValidIndex(Slot) || Tame->Cargo[Slot].IsEmpty()) return TEXT("nothing in that pack");
		FWRItemStack& From = Tame->Cargo[Slot];
		const int32 Moved = Bag ? FWRInventoryRules::MoveFrom(From, Bag->GetSlotsMutable(), FMath::Min(Want, From.Count)) : 0;
		if (From.Count <= 0) From = FWRItemStack();
		R.CargoKg = Tame->CargoKg();
		return Moved > 0 ? FString::Printf(TEXT("Took %d"), Moved) : TEXT("No room in your bag");
	}
	return TEXT("unknown");
}

// ---------------------------------------------------------------- the incubator

void FWRTaming::PieceToJson(const AWRBuildPiece* Piece, const TSharedRef<FJsonObject>& O)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def || Def->Kind != TEXT("incubator")) return;
	O->SetNumberField(TEXT("incubation"), Piece->Growth);
	O->SetNumberField(TEXT("heat"), Piece->FuelClock);
	O->SetStringField(TEXT("incubatedBy"), Piece->Planter);
}

void FWRTaming::PieceFromJson(AWRBuildPiece* Piece, const TSharedPtr<FJsonObject>& O)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def || Def->Kind != TEXT("incubator") || !O.IsValid()) return;
	double N = 0.0;
	if (O->TryGetNumberField(TEXT("incubation"), N)) Piece->Growth = N;
	if (O->TryGetNumberField(TEXT("heat"), N)) Piece->FuelClock = N;
	O->TryGetStringField(TEXT("incubatedBy"), Piece->Planter);
	if (Piece->Storage.Num() < 2) Piece->Storage.SetNum(2);
	Piece->Growth8 = uint8(FMath::Clamp(Piece->Growth, 0.f, 1.f) * 255.f);
}

namespace WRTameDetail
{
	/** The species an egg stack hatches (a fertilised egg's genes say; a stolen egg's id: egg_<species>). */
	FName EggSpecies(const FWRItemStack& Egg)
	{
		if (Egg.IsEmpty()) return NAME_None;
		FName Species;
		FWRTameGenes::FromSockets(Egg.Sockets, &Species);
		if (!Species.IsNone()) return Species;
		const FString Id = Egg.ItemId.ToString();
		if (Id.StartsWith(TEXT("fertile_"))) return FName(*Id.RightChop(8));
		if (Id.StartsWith(TEXT("egg_"))) return FName(*Id.RightChop(4));
		return NAME_None;
	}
}

FString FWRTaming::IncubatorCommand(AWRPlayerController* Player, AWRBuildPiece* Piece, const TArray<FString>& Args)
{
	const APlayerState* PS = Player ? Player->GetPlayerState<APlayerState>() : nullptr;
	const FWRPieceDef* PDef = Piece ? Piece->GetDef() : nullptr;
	if (!PS || !PDef || PDef->Kind != TEXT("incubator")) return TEXT("no incubator nearby");
	if (!Piece->CanAccess(PS->GetPlayerName())) return TEXT("not yours");
	if (Piece->Storage.Num() < 2) Piece->Storage.SetNum(2);
	UWRInventoryComponent* Bag = UWRInventoryComponent::FindFor(PS);
	const FString Verb = Args.IsValidIndex(0) ? Args[0] : FString();
	const int32 Slot = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : -1;
	FWRItemStack& Egg = Piece->Storage[0];
	FWRItemStack& Fuel = Piece->Storage[1];
	FString Out;
	if (Verb == TEXT("in"))
	{
		if (!Egg.IsEmpty()) return TEXT("It already holds an egg");
		if (!Bag || !Bag->GetSlots().IsValidIndex(Slot) || Bag->GetSlots()[Slot].IsEmpty()) return TEXT("nothing in that slot");
		FWRItemStack& From = Bag->GetSlotsMutable()[Slot];
		const FName Species = EggSpecies(From);
		if (Species.IsNone() || !FWRCreatureRegistry::Get().Find(Species)) return TEXT("That isn't an egg it can hatch");
		Egg = From;
		Egg.Count = 1;
		if (--From.Count <= 0) From = FWRItemStack();
		Piece->Growth = 0.f;
		Piece->Planter = PS->GetPlayerName();
		Out = TEXT("Egg in: keep it fuelled");
	}
	else if (Verb == TEXT("fuel"))
	{
		if (!Bag || !Bag->GetSlots().IsValidIndex(Slot) || Bag->GetSlots()[Slot].IsEmpty()) return TEXT("nothing in that slot");
		FWRItemStack& From = Bag->GetSlotsMutable()[Slot];
		const FWRCreatureDef* Sp = FWRCreatureRegistry::Get().Find(EggSpecies(Egg));
		const TArray<FName> Ok = Sp ? FuelFor(Sp->Element) : TArray<FName>{ TEXT("charcoal"), TEXT("sulfur_crystal"), TEXT("ice_block"), TEXT("waterskin"), TEXT("wood") };
		if (!Ok.Contains(From.ItemId)) return Sp ? FString::Printf(TEXT("A %s egg needs %s"), *Sp->Name, *FString::JoinBy(Ok, TEXT(" or "), [](const FName& F) { const FWRItemDef* D = FWRItemRegistry::Get().Find(F); return D ? D->Name : F.ToString(); })) : TEXT("That isn't fuel");
		if (!Fuel.IsEmpty() && Fuel.ItemId != From.ItemId) return TEXT("Take the other fuel out first");
		const FWRItemDef* FD = FWRItemRegistry::Get().Find(From.ItemId);
		const int32 Room = (FD ? FD->StackSize : 20) - (Fuel.IsEmpty() ? 0 : Fuel.Count);
		const int32 N = FMath::Min(Room, From.Count);
		if (N <= 0) return TEXT("Its fuel box is full");
		if (Fuel.IsEmpty()) { Fuel = From; Fuel.Count = 0; }
		Fuel.Count += N;
		From.Count -= N;
		if (From.Count <= 0) From = FWRItemStack();
		Out = FString::Printf(TEXT("Added %d fuel"), N);
		WRSound::Broadcast(Piece, TEXT("incubator.fuel"));
	}
	else if (Verb == TEXT("out"))
	{
		if (Egg.IsEmpty()) return TEXT("no egg in it");
		if (!Bag || !Bag->AddInstance(Egg)) return TEXT("No room in your bag");
		Egg = FWRItemStack();
		Piece->Growth = 0.f;
		Out = TEXT("Took the egg out (it starts over in an incubator)");
	}
	else if (Verb == TEXT("fuelout"))
	{
		if (Fuel.IsEmpty()) return TEXT("no fuel in it");
		const int32 Left = Bag ? Bag->AddItem(Fuel.ItemId, Fuel.Count) : Fuel.Count;
		if (Left >= Fuel.Count) return TEXT("No room in your bag");
		Fuel.Count = Left;
		if (Fuel.Count <= 0) Fuel = FWRItemStack();
		Out = Left > 0 ? TEXT("Took what fits") : TEXT("Took the fuel out");
	}
	else return TEXT("unknown");
	Piece->Growth8 = uint8(FMath::Clamp(Piece->Growth, 0.f, 1.f) * 255.f);
	Piece->OnRep_Farm();
	return Out;
}

// ---------------------------------------------------------------- the subsystem

UWRTamingSubsystem* UWRTamingSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRTamingSubsystem>() : nullptr;
}

TStatId UWRTamingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRTamingSubsystem, STATGROUP_Tickables);
}

void UWRTamingSubsystem::NoteHit(AWRCharacterBase* Attacker, AWRCharacterBase* Target)
{
	TArray<FHitBy>& List = Hits.FindOrAdd(Target);
	const double Now = GetWorld()->GetTimeSeconds();
	List.RemoveAll([Attacker, Now](const FHitBy& H) { return !H.Who.IsValid() || H.Who.Get() == Attacker || Now - H.At > 15.0; });
	List.Add({ Attacker, Now });
}

int32 UWRTamingSubsystem::OthersOn(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, float Window) const
{
	const TArray<FHitBy>* List = Hits.Find(const_cast<AWRCharacterBase*>(Target));
	if (!List) return 0;
	const double Now = GetWorld()->GetTimeSeconds();
	const FString Owner = Attacker ? Attacker->GetTameOwner() : FString();
	int32 N = 0;
	for (const FHitBy& H : *List)
	{
		const AWRCharacterBase* Who = H.Who.Get();
		if (!Who || Who == Attacker || Now - H.At > Window) continue;
		// Allies: its owner and their clan, and their tames.
		const FString WhoName = Who->GetPlayerState() ? Who->GetPlayerState()->GetPlayerName() : Who->GetTameOwner();
		N += Allies(Owner, WhoName) ? 1 : 0;
	}
	return N;
}

TArray<AWRCreature*> UWRTamingSubsystem::TamesOn(const AWRCharacterBase* Target, float Window) const
{
	TArray<AWRCreature*> Out;
	const TArray<FHitBy>* List = Hits.Find(const_cast<AWRCharacterBase*>(Target));
	if (!List) return Out;
	const double Now = GetWorld()->GetTimeSeconds();
	for (const FHitBy& H : *List)
	{
		AWRCreature* C = Cast<AWRCreature>(H.Who.Get());
		if (C && C->IsTamed() && C->IsAlive() && Now - H.At <= Window) Out.AddUnique(C);
	}
	return Out;
}

void UWRTamingSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !World->HasBegunPlay()) return;
	Clock += DeltaTime;
	if (Clock < 1.f) return;
	const float Dt = Clock;
	Clock = 0.f;
	TickPairs(Dt);
	TickIncubators(Dt);
	for (auto It = Hits.CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
}

void UWRTamingSubsystem::TickPairs(float Dt)
{
	UWorld* World = GetWorld();
	const float Range = T(TEXT("breed.range"), 1000.f);
	// Who's ready: adult, mating on, fed, not ridden; she's not resting or carrying.
	TArray<AWRCreature*> Males, Females;
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		AWRCreature* C = *It;
		const FWRTameRep& R = C->TameRep;
		if (!C->IsTamed() || !C->IsAlive() || !R.bMating || R.IsBaby() || R.Hunger <= 0.f || C->GetRider()) { C->TameRep.MateBar = 0.f; C->MatePartner = nullptr; continue; }
		if (R.Sex == 1) Males.Add(C);
		else if (R.Sex == 2 && R.Cooldown <= 0.f && R.Pregnant <= 0.f) Females.Add(C);
	}
	for (AWRCreature* F : Females)
	{
		AWRCreature* Best = nullptr;
		float BestD = Range;
		for (AWRCreature* M : Males)
		{
			const float D = FVector::Dist(M->GetActorLocation(), F->GetActorLocation());
			if (M->CreatureId == F->CreatureId && Allies(M->TamedBy, F->TamedBy) && D < BestD) { Best = M; BestD = D; }
		}
		if (!Best) { F->TameRep.MateBar = 0.f; F->MatePartner = nullptr; continue; }
		F->MatePartner = Best;
		Best->MatePartner = F;
		const float Was = F->TameRep.MateBar;
		F->TameRep.MateBar = FMath::Min(1.f, Was + Dt / FMath::Max(1.f, T(TEXT("breed.mateSeconds"), 120.f)));
		Best->TameRep.MateBar = F->TameRep.MateBar;
		if (FMath::FloorToInt(Was * 30.f) != FMath::FloorToInt(F->TameRep.MateBar * 30.f))
		{
			const FVector Mid = (F->GetActorLocation() + Best->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, F->GetSimpleCollisionHalfHeight());
			WRFx::Broadcast(F, TEXT("tame.heart"), Mid, FRotator::ZeroRotator, 0.8f, TEXT("tame.mate"));
		}
		if (F->TameRep.MateBar < 1.f) continue;
		// Conceived: an egg at her feet, or young she carries.
		const FWRCreatureDef* Def = F->GetDef();
		const APlayerState* Owner = PlayerNamed(World, F->TamedBy);
		const float Odds = 1.f + UWRLifeSkillsComponent::StatFor(Owner, TEXT("mutationOdds"));
		int32 Mutations = 0;
		const FWRTameGenes Child = FWRTameGenes::Child(Best->Genes, F->Genes, Odds, Mutations);
		const bool bTwins = FMath::FRand() < (F->TameHasNode(TEXT("prolific")) ? T(TEXT("breed.twinChance"), 0.25f) : 0.f);
		F->TameRep.MateBar = 0.f;
		Best->TameRep.MateBar = 0.f;
		F->MatePartner = nullptr;
		Best->MatePartner = nullptr;
		F->TameRep.Cooldown = T(TEXT("breed.cooldownHours"), 4.f) * 3600.f;
		if (Def && FWRTaming::LaysEggs(*Def))
		{
			for (int32 i = 0; i < (bTwins ? 2 : 1); ++i)
			{
				FWRItemStack Egg;
				Egg.ItemId = FWRTaming::FertileEgg(Def->Id);
				Egg.Count = 1;
				FWRTameGenes G = Child;
				if (i > 0) G.Sex = FMath::RandBool() ? 1 : 2;
				G.ToSockets(Egg.Sockets, Def->Id);
				AWRLaidEgg::Lay(World, F->GetActorLocation() - FVector(0.f, 0.f, F->GetSimpleCollisionHalfHeight()) + F->GetActorRightVector() * (i * 40.f), Egg, F->TamedBy, Def->Color);
			}
			Notify(World, F->TamedBy, FString::Printf(TEXT("Your %s laid %s%s"), *Def->Name, bTwins ? TEXT("two eggs") : TEXT("an egg"), Mutations > 0 ? TEXT(" (something's different about it)") : TEXT("")));
		}
		else if (Def)
		{
			F->PregnantGenes = Child;
			F->bPregnantTwins = bTwins;
			F->TameRep.PregnantTotal = FWRTaming::ByDifficulty(*Def, TEXT("breed.pregnancy"), 1800.f, 7200.f, 21600.f);
			F->TameRep.Pregnant = F->TameRep.PregnantTotal;
			Notify(World, F->TamedBy, FString::Printf(TEXT("Your %s is carrying young"), *Def->Name));
		}
		WRFx::Broadcast(F, TEXT("tame.heart"), F->GetActorLocation() + FVector(0.f, 0.f, F->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.5f, TEXT("tame.birth"));
		UE_LOG(LogWildreach, Log, TEXT("TAME %s x %s bred (%d mutations)"), *Best->GetName(), *F->GetName(), Mutations);
	}
}

void UWRTamingSubsystem::TickIncubators(float Dt)
{
	UWorld* World = GetWorld();
	const float Mat = FWRTaming::Maturation();
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		AWRBuildPiece* P = *It;
		const FWRPieceDef* PDef = P->bGhost ? nullptr : P->GetDef();
		if (!PDef || PDef->Kind != TEXT("incubator")) continue;
		if (P->Storage.Num() < 2) P->Storage.SetNum(2);
		FWRItemStack& Egg = P->Storage[0];
		FWRItemStack& Fuel = P->Storage[1];
		const uint8 WasFlags = P->FarmFlags, WasStop = P->FarmStop, WasGrowth = P->Growth8;
		const FName Species = EggSpecies(Egg);
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Species);
		bool bWarm = false;
		uint8 Stop = 0;
		if (Def)
		{
			if (P->FuelClock <= 0.f && !Fuel.IsEmpty() && FWRTaming::FuelFor(Def->Element).Contains(Fuel.ItemId))
			{
				if (--Fuel.Count <= 0) Fuel = FWRItemStack();
				P->FuelClock = T(TEXT("incubator.fuelSeconds"), 900.f);
			}
			if (P->FuelClock > 0.f && P->Growth < 1.f)
			{
				bWarm = true;
				P->FuelClock = FMath::Max(0.f, P->FuelClock - Dt * Mat);
				P->Growth = FMath::Min(1.f, P->Growth + Dt * Mat / FMath::Max(1.f, FWRTaming::ByDifficulty(*Def, TEXT("breed.pregnancy"), 1800.f, 7200.f, 21600.f)));
			}
			else if (P->Growth < 1.f) Stop = 1;   // no fuel: the egg waits (it never dies)
			if (P->Growth >= 1.f)
			{
				const APlayerState* PS = PlayerNamed(World, P->Planter);
				if (PS && AWRCreature::CountTames(World, P->Planter) < AWRCreature::TameLimitFor(PS))
				{
					const FWRTameGenes G = FWRTameGenes::FromSockets(Egg.Sockets);
					const FVector At = P->GetActorLocation() + P->GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 20.f);
					if (FWRTaming::Hatch(World, Species, G, P->Planter, At, T(TEXT("breed.babyStart"), 0.1f)))
					{
						WRFx::Broadcast(P, TEXT("tame.hatch"), P->GetActorLocation() + FVector(0.f, 0.f, 90.f), FRotator::ZeroRotator, 1.f, TEXT("tame.birth"));
						UWRLifeSkillsComponent::Award(PS, TEXT("taming"), TEXT("tameBirth"), T(TEXT("skillXp.birth"), 40.f));
						Notify(World, P->Planter, FString::Printf(TEXT("A %s hatched: it's yours (answer its care to imprint it)"), *Def->Name));
						Egg = FWRItemStack();
						P->Growth = 0.f;
					}
				}
				else Stop = 2;   // ready, waiting for room under the tame limit
			}
		}
		P->FarmFlags = bWarm ? (P->FarmFlags | WRFarmFlag::Working) : (P->FarmFlags & ~WRFarmFlag::Working);
		P->FarmStop = Stop;
		P->Growth8 = uint8(FMath::Clamp(P->Growth, 0.f, 1.f) * 255.f);
		if (P->FarmFlags != WasFlags || P->FarmStop != WasStop || P->Growth8 != WasGrowth) P->OnRep_Farm();
	}
}

// ---------------------------------------------------------------- a laid egg

AWRLaidEgg::AWRLaidEgg()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;
}

void AWRLaidEgg::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRLaidEgg, Egg);
	DOREPLIFETIME(AWRLaidEgg, OwnerName);
	DOREPLIFETIME(AWRLaidEgg, Shell);
}

AWRLaidEgg* AWRLaidEgg::Lay(UWorld* World, const FVector& Feet, const FWRItemStack& InEgg, const FString& Owner, const FLinearColor& InShell)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector At = Feet;
	FHitResult Ground;
	if (World->LineTraceSingleByChannel(Ground, Feet + FVector(0.f, 0.f, 100.f), Feet - FVector(0.f, 0.f, 300.f), ECC_WorldStatic)) At = Ground.ImpactPoint;
	AWRLaidEgg* E = World->SpawnActorDeferred<AWRLaidEgg>(AWRLaidEgg::StaticClass(), FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E) return nullptr;
	E->Egg = InEgg;
	E->OwnerName = Owner;
	E->Shell = InShell;
	E->LaidAt = World->GetTimeSeconds();
	E->FinishSpawning(FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), At));
	return E;
}

FString AWRLaidEgg::Take(AWRPlayerController* Player)
{
	const APlayerState* PS = Player ? Player->GetPlayerState<APlayerState>() : nullptr;
	if (!PS || Egg.IsEmpty()) return FString();
	if (!Allies(OwnerName, PS->GetPlayerName()) && GetWorld()->GetTimeSeconds() - LaidAt < T(TEXT("breed.eggClaimSeconds"), 600.f))
	{
		return FString::Printf(TEXT("%s's egg"), *OwnerName);
	}
	UWRInventoryComponent* Bag = UWRInventoryComponent::FindFor(PS);
	if (!Bag || !Bag->AddInstance(Egg)) return TEXT("Inventory full");
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(Egg.ItemId);
	Egg = FWRItemStack();
	Destroy();
	return FString::Printf(TEXT("Took the %s: hatch it in an incubator"), Def ? *Def->Name : TEXT("egg"));
}

void AWRLaidEgg::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	// A scrape in the ground lined with straw and shed down, the egg lying in it on its side (WRToolMesh's builders).
	FWRToolMesh M;
	const FLinearColor Straw(0.42f, 0.33f, 0.16f), Down(0.62f, 0.6f, 0.56f);
	TArray<FVector> Ring;
	for (int32 i = 0; i < 26; ++i)
	{
		const float A = i * 2.f * PI / 26.f;
		Ring.Add(FVector(FMath::Cos(A) * 26.f, FMath::Sin(A) * 26.f, 3.f + FMath::Sin(A * 3.f) * 1.2f));
	}
	M.Tube(Ring, 4.5f, 6, Straw, 0.9f, true);
	for (int32 s = 0; s < 14; ++s)
	{
		const float A = s * 2.4f;
		const FVector C(FMath::Cos(A) * 14.f, FMath::Sin(A) * 14.f, 1.5f);
		M.Tube({ C - FVector(FMath::Cos(A + 1.3f), FMath::Sin(A + 1.3f), 0.f) * 11.f, C + FVector(FMath::Cos(A + 1.3f), FMath::Sin(A + 1.3f), 0.2f) * 11.f }, 0.7f, 4, s % 3 == 0 ? Down : Straw, 0.95f);
	}
	// The egg: an ovoid lathed along X, broad end back, its shell in her colour, lightened and speckled darker.
	const FLinearColor ShellCol = FMath::Lerp(Shell, FLinearColor(0.85f, 0.8f, 0.7f), 0.55f);
	TArray<FVector> Profile;
	for (int32 i = 0; i <= 12; ++i)
	{
		const float t = i / 12.f;
		const float R = FMath::Sqrt(FMath::Max(0.f, FMath::Sin(t * PI))) * (9.5f - 2.2f * t);
		Profile.Add(FVector(t, R, R));
	}
	M.Lathe(FVector(-12.f, 0.f, 9.f), FVector(1.f, 0.f, 0.f), 24.f, Profile, 16, ShellCol, 0.35f);
	for (int32 k = 0; k < 9; ++k)
	{
		const float t = 0.15f + 0.08f * k, A = k * 2.1f;
		const float R = FMath::Sqrt(FMath::Max(0.f, FMath::Sin(t * PI))) * (9.5f - 2.2f * t);
		const FVector P(-12.f + 24.f * t, FMath::Cos(A) * R, 9.f + FMath::Sin(A) * R);
		M.Tube({ P, P + FVector(0.6f, 0.f, 0.f) }, 1.1f, 5, ShellCol * 0.55f, 0.5f);
	}
	M.ApplyTo(Mesh);
}
