#pragma once
// NPC strongholds (Aaron 11-16, r-siege-pve: "Allow it to practice siegeing on NPC strongholds"; showcase-notes
// cooks/wild-answers-1115.md 2.7, 3 part D).
//
// Two forts on the Old Island (Data/strongholds.json, placed off the heightmap by Tools/terrain/strongholds.mjs): a
// palisade ring with a gate, two gate towers with a bolt turret on each deck, a stone keep with a reinforced door and a
// war chest inside, and the land's beasts in the yard. They are built of the game's own build pieces, owned by
// "@hold:<id>", so every raid rule, charge, ram, trebuchet stone and spell works on them unchanged, on every world:
// players may always break a stronghold, PvE worlds included (FWRRaiding::MayRaid). The turrets need no power or bolts
// and shoot at survivors and their tames (WRTurret), which is what makes the engines' reach and the ram's roof matter.
// The keep's door (or any keep wall) falling opens the war chest. Once a fort has fallen and nobody has been near it for
// rebuildHours, it stands again whole, its beasts back and its chest refilled. What is broken is saved with the world
// (the "strongholds" document), so a restart doesn't mend it.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRStronghold.generated.h"

class AWRBuildPiece;
class AWRCreature;
class AWRCharacterBase;

/** One piece of a fort's layout (game cm, its ground or floor height). */
struct FWRHoldPiece
{
	FName Piece;
	FName Role;      // gate, palisade, tower, keep_floor, keep_wall, keep_door, war_chest
	FVector At = FVector::ZeroVector;
	float Yaw = 0.f;
};

struct FWRHoldDef
{
	FName Id;
	FString Name;
	FString Lore;
	FName Biome;
	FVector Centre = FVector::ZeroVector;
	float Yaw = 0.f;
	float RingRadius = 1100.f;
	float ClearRadius = 1700.f;
	TArray<FWRHoldPiece> Pieces;
	TArray<TPair<FName, int32>> Defenders;
	TArray<TPair<FName, int32>> Chest;
};

/** The forts' data and the rules every machine can answer. */
struct WILDREACH_API FWRStrongholds
{
	static const TArray<FWRHoldDef>& All();
	static const FWRHoldDef* Find(FName Id);
	/** "@hold:<id>": the owner name of every piece of a fort. */
	static FString OwnerFor(FName Id) { return FString(TEXT("@hold:")) + Id.ToString(); }
	static bool IsHoldOwner(const FString& Owner) { return Owner.StartsWith(TEXT("@hold:")); }
	static bool IsHoldPiece(const AWRBuildPiece* Piece);
	/** "Emberwatch" for a hold owner name (else the name as it is). */
	static FString DisplayName(const FString& Owner);
	/** The fort whose ground this is (its clear radius + Extra cm), or null. */
	static const FWRHoldDef* At(const FVector& Where, float Extra = 0.f);
	/** Building there: why not ("A stronghold stands here"), or empty. */
	static FString BuildBlock(const FVector& Where);
	/** The war chest is barred until its keep is breached (its piece's bLocked, replicated). */
	static bool IsBarredChest(const AWRBuildPiece* Piece);
};

UCLASS()
class WILDREACH_API UWRStrongholdSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	static UWRStrongholdSubsystem* Get(const UObject* WorldContext);

	/** Server, with the world's save (the "strongholds" document). */
	static void Save(UWorld* World);
	static void Load(UWorld* World);

private:
	struct FFort
	{
		bool bBuilt = false;
		/** Layout index -> the piece standing there (stale = broken). */
		TArray<TWeakObjectPtr<AWRBuildPiece>> Pieces;
		/** Tower index -> the turret on its deck. */
		TMap<int32, TWeakObjectPtr<AWRBuildPiece>> Turrets;
		TSet<int32> Broken;
		TArray<TWeakObjectPtr<AWRCreature>> Defenders;
		/** Game hours it fell (its keep breached); -1 standing. */
		double FallenAt = -1.0;
		/** The war chest's contents from the save (applied when it's built), or empty for a full chest. */
		TArray<TSharedPtr<class FJsonValue>> SavedChest;
		bool bHasSavedChest = false;
		/** Survivors already told its name and story (once each, this session). */
		TSet<FString> Told;
	};
	TMap<FName, FFort> Forts;
	bool bDressingCleared = false;
	float Clock = 0.f;

	void ClearDressing();
	void Build(const FWRHoldDef& D, FFort& F, bool bFresh);
	void Step(const FWRHoldDef& D, FFort& F);
	void Rebuild(const FWRHoldDef& D, FFort& F);
	void TopUpDefenders(const FWRHoldDef& D, FFort& F, bool bAll);
	bool PlayerWithin(const FVector& At, float Range) const;
	double Hours() const;
};
