#pragma once

// Taming beyond the tame itself (cooks/build-taming.md, FULL-GAME 6.5): creature levels and the six stats, bond levels
// from the tame's own work, the five role skill trees (Data/tame_trees.json), saddles / pack saddles / barding (items
// with "tack"), mount kinds (ground, pack, swim, fly), hunger, breeding (mating, eggs, pregnancy), babies growing and
// their care prompts (imprinting), mutations and their visible morphs, the incubator, and how all of it saves.
// Server-authoritative: the creature's FWRTameRep replicates for every machine's card, HUD and look.

#include "CoreMinimal.h"
#include "WRInventory.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRTaming.generated.h"

class AWRCreature;
class AWRCharacter;
class AWRCharacterBase;
class AWRBuildPiece;
class AWRPlayerController;
class FJsonObject;
struct FWRCreatureDef;
struct FWRHit;

/** The six stats a creature's points go into (Ark's per-stat levels). */
namespace WRTameStat
{
	enum EId : int32 { Health, Stamina, Damage, Carry, Speed, Torpor, Count };
	WILDREACH_API const TCHAR* Name(int32 Id);
	WILDREACH_API const TCHAR* Label(int32 Id);
}

/** What a mutation shows on the body (an overlay over its own texture). */
namespace WRTameMorph
{
	enum EKind : uint8 { None, Pale, Dark, Bands, GlowLine };
	WILDREACH_API const TCHAR* Name(uint8 Kind);
}

/** A creature's inheritance: points per stat, mutations on each parent's side, its visible morphs, its sex. */
struct WILDREACH_API FWRTameGenes
{
	uint8 Points[WRTameStat::Count] = { 0, 0, 0, 0, 0, 0 };
	/** Points that came from mutations (shown on the card as the mutated share). */
	uint8 MutPoints[WRTameStat::Count] = { 0, 0, 0, 0, 0, 0 };
	/** Mutations counted on the father's line and the mother's (each capped at tame.breed.mutationCap for new ones). */
	uint8 MutFather = 0, MutMother = 0;
	/** Up to 4 morphs: kind (WRTameMorph) and its tint (packed sRGB). */
	TArray<TPair<uint8, FColor>> Morphs;
	/** 1 male, 2 female. */
	uint8 Sex = 0;
	bool bValid = false;
	/** Bred or hatched from a bred egg: its stats aren't held to the wild +-10% (mutations push past it). */
	bool bBred = false;

	int32 Level() const;
	/** Random wild genes for a level. */
	static FWRTameGenes Wild(int32 Level);
	/** A child of two parents (55/45 per stat, mutations by the Breeder's odds). */
	static FWRTameGenes Child(const FWRTameGenes& Father, const FWRTameGenes& Mother, float MutationOddsMult, int32& OutMutations);
	/** Adds one mutation (+points to a random stat, a new morph). bFather: which side's counter. */
	void Mutate(bool bFather);
	/** Into / out of an item stack's socket list (a fertilised egg carries its child's genes). */
	void ToSockets(TArray<FName>& Out, FName Species) const;
	static FWRTameGenes FromSockets(const TArray<FName>& In, FName* OutSpecies = nullptr);
	TSharedRef<FJsonObject> ToJson() const;
	static FWRTameGenes FromJson(const TSharedPtr<FJsonObject>& O);
};

/** Everything a tame's card, HUD and look need on every machine. */
USTRUCT()
struct FWRTameRep
{
	GENERATED_BODY()

	UPROPERTY() uint8 Sex = 0;
	UPROPERTY() uint8 WildLevel = 1;
	UPROPERTY() uint8 Bond = 0;
	UPROPERTY() float BondXP = 0.f;
	UPROPERTY() TArray<uint8> Points;
	UPROPERTY() TArray<uint8> MutPoints;
	UPROPERTY() uint8 MutFather = 0;
	UPROPERTY() uint8 MutMother = 0;
	/** Morph kind per entry and its colour (parallel arrays). */
	UPROPERTY() TArray<uint8> MorphKinds;
	UPROPERTY() TArray<FColor> MorphColors;
	/** Learned skill-tree nodes (bit per node: 0-3 the line, 4 capstone A, 5 capstone B). */
	UPROPERTY() uint8 Nodes = 0;
	UPROPERTY() FWRItemStack Saddle;
	UPROPERTY() FWRItemStack Barding;
	UPROPERTY() float Imprint = 0.f;
	UPROPERTY() FString Imprinter;
	/** 0.1..1: a baby grows to 1 (adult). */
	UPROPERTY() float Growth = 1.f;
	/** The care it asks for now (0 none, 1 feed, 2 walk, 3 cuddle), the food it wants, how far a walk has come (m). */
	UPROPERTY() uint8 Care = 0;
	UPROPERTY() FName CareFood;
	UPROPERTY() float CareWalked = 0.f;
	UPROPERTY() bool bMating = false;
	/** 0..1 while a mating pair is together. */
	UPROPERTY() float MateBar = 0.f;
	/** Seconds until she can mate again; seconds left / total of a pregnancy (0 = not pregnant). */
	UPROPERTY() float Cooldown = 0.f;
	UPROPERTY() float Pregnant = 0.f;
	UPROPERTY() float PregnantTotal = 0.f;
	UPROPERTY() float Hunger = 100.f;
	/** Flying stamina (fly mounts) for the HUD; carried kg for haulers. */
	UPROPERTY() float FlyStamina = 0.f;
	UPROPERTY() float CargoKg = 0.f;
	/** A fly mount near a boss arena: it won't take off (and lands). */
	UPROPERTY() bool bNoFly = false;
	/** Its stat multipliers as they stand (for the card): one per stat. */
	UPROPERTY() TArray<float> Mults;
	bool IsBaby() const { return Growth < 0.999f; }
	bool HasNode(int32 Index) const { return (Nodes & (1 << Index)) != 0; }
};

/** One node of a role tree. */
struct FWRTameNode
{
	FName Id;
	FString Name;
	FString Text;
	int32 Cost = 2;
};

/** A role tree: four nodes in a line, then one of two capstones. */
struct FWRTameTree
{
	FName Id;
	FString Name;
	TArray<FWRTameNode> Nodes;   // 0-3 the line, 4 / 5 the capstones
};

class WILDREACH_API FWRTameTrees
{
public:
	static FWRTameTrees& Get();
	const FWRTameTree* Find(FName Id) const;
	/** The tree a creature uses (its role's first word: "fighter, scout" is Fighter; "farm animal" is Farm). */
	const FWRTameTree* For(const FWRCreatureDef& Def) const;
	void Reload();

private:
	FWRTameTrees();
	TArray<FWRTameTree> Trees;
};

/** Saddles, pack saddles and barding (items.json "tack"). */
struct FWRTackDef
{
	FName Kind;            // saddle / pack / barding
	float Reduction = 0.f; // damage the mount takes, less this share
	int32 Slots = 0;       // pack saddle: extra pack slots
	float Speed = 0.f;     // barding: its weight on the mount
	FName Hazard;          // biome barding: lava / ice / ...
};

/** Static taming rules shared by the creature, the combat code, the screens and the save. */
struct WILDREACH_API FWRTaming
{
	static const FWRTackDef* Tack(FName ItemId);
	/** A tack piece's value: its reduction x quality x roll x the owner's perk. */
	static float TackValue(const FWRItemStack& Stack, const AActor* Owner);
	/** "egg" (lays) or "live" (gives birth). */
	static bool LaysEggs(const FWRCreatureDef& Def);
	/** ground / pack / swim / fly ("" not a mount). */
	static FName MountKind(const FWRCreatureDef& Def);
	/** Seconds by difficulty (easy / medium / hard) from a tame.breed table. */
	static float ByDifficulty(const FWRCreatureDef& Def, const TCHAR* Table, float Easy, float Medium, float Hard);
	/** The server's maturation speed (server.maturationMultiplier). */
	static float Maturation();
	/** Its hunger empties in this many seconds (3 / 4 / 6 h by size). */
	static float HungerSeconds(const FWRCreatureDef& Def);
	/** Kill XP a tame gets for helping kill this. */
	static float KillXP(const FWRCreatureDef& Def);
	/** The incubator fuel its element burns (charcoal / sulfur, ice block, water, wood). */
	static TArray<FName> FuelFor(FName Element);
	/** A fertilised egg's item id for a species. */
	static FName FertileEgg(FName Species) { return FName(*(FString(TEXT("fertile_")) + Species.ToString())); }

	// --- combat hooks (WRCombatLibrary::ApplyHit; server) ---
	/** Damage x for a tame's own blow (stats, bond, tree, imprint) and a rider's blow from its saddle (Warhorse). */
	static float AttackerScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target);
	/** A player's blow taken: Bulwark haulers near; Guardian fighters take their share through ApplyHit. Returns the rest. */
	static float PlayerTakes(AWRCharacterBase* Player, const FWRHit& Hit, float Damage);
	/** After a blow landed: fight XP, the tames that hit it (kill XP later), Pack tactics' record. */
	static void AfterHit(AWRCharacterBase* Attacker, AWRCharacterBase* Target, float Damage, bool bDies);
	/** Stance taken by a tame (Stalwart). */
	static float StanceTaken(const AWRCharacterBase* Target, float Stance);
	/** Steady seat: does this blow fail to throw the rider? */
	static bool KeepsSeat(const AWRCreature* Mount, const FWRHit& Hit, float Damage);

	// --- other systems ---
	/** Forager: what didn't fit in a player's bag goes into their forager hauler's packs within reach. Returns what's left. */
	static int32 Forage(const AActor* Owner, FName ItemId, int32 Count);
	/** Lodestar: gathering x for a player near a Lodestar tame. */
	static float GatherScale(const AActor* Player);
	/** Server: a sensing tame (sense_secrets) feels out the nearest cave or egg nest near its owner and tells them where
	 *  (Keen sense: further; Seeker: their clan too). */
	static void Sense(AWRCreature* Tame);
	/** A tame of this owner within Range with the node (by id), or null. */
	static AWRCreature* TameWithNode(const UWorld* World, const FString& Owner, FName Node, const FVector& Near, float Range);
	/** A tame's save line and its load. */
	static TSharedRef<FJsonObject> SaveTame(const AWRCreature* Tame);
	static void LoadTame(AWRCreature* Tame, const TSharedPtr<FJsonObject>& O);
	/** An incubator's state in the piece's save. */
	static void PieceToJson(const AWRBuildPiece* Piece, const TSharedRef<FJsonObject>& O);
	static void PieceFromJson(AWRBuildPiece* Piece, const TSharedPtr<FJsonObject>& O);
	/** Server: a player's incubator command ("in <bagSlot>", "fuel <bagSlot>", "out"). */
	static FString IncubatorCommand(AWRPlayerController* Player, AWRBuildPiece* Piece, const TArray<FString>& Args);
	/** Server: a player's tame-card command on their tame ("learn <i>", "saddle <bagSlot>", "barding <bagSlot>",
	 *  "unsaddle", "unbarding", "mate on|off", "care", "feed", "put <bagSlot> <n>", "take <slot> <n>"). */
	static FString CardCommand(AWRPlayerController* Player, AWRCreature* Tame, const TArray<FString>& Args);
	/** Hatch a creature from genes for an owner at a place (tamed, baby). */
	static AWRCreature* Hatch(UWorld* World, FName Species, const FWRTameGenes& Genes, const FString& Owner, const FVector& At, float Growth);
};

/**
 * A fertilised egg a tame laid, lying in the scrape she made at her feet (straw and down): E takes it into your bag
 * (her owner's clan only for its first 10 minutes). Eggs don't spoil or die; it waits for an incubator.
 */
UCLASS()
class WILDREACH_API AWRLaidEgg : public AActor
{
	GENERATED_BODY()

public:
	AWRLaidEgg();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Server: lay one here. */
	static AWRLaidEgg* Lay(UWorld* World, const FVector& Feet, const FWRItemStack& Egg, const FString& Owner, const FLinearColor& Shell);
	/** Server: E on it. */
	FString Take(AWRPlayerController* Player);

	UPROPERTY(Replicated) FWRItemStack Egg;
	UPROPERTY(Replicated) FString OwnerName;
	UPROPERTY(Replicated) FLinearColor Shell = FLinearColor(0.8f, 0.75f, 0.65f);
	double LaidAt = 0.0;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UProceduralMeshComponent> Mesh;
};

/** The looks (WRTameLook.cpp): an incubator's egg and warmth (the creature's tack and morphs: AWRCreature::RefreshTameLook). */
struct WILDREACH_API FWRTameLook
{
	static void RefreshPiece(AWRBuildPiece* Piece);
};

/** Server: mating pairs, incubators, the tames' slow clocks (once a second). */
UCLASS()
class WILDREACH_API UWRTamingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRTamingSubsystem* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** Tames that hit a target lately (kill XP; Pack tactics counts allies on the same target). */
	void NoteHit(AWRCharacterBase* Attacker, AWRCharacterBase* Target);
	int32 OthersOn(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, float Window) const;
	TArray<AWRCreature*> TamesOn(const AWRCharacterBase* Target, float Window) const;

private:
	void TickPairs(float Dt);
	void TickIncubators(float Dt);
	float Clock = 0.f;
	float PairClock = 0.f;
	struct FHitBy { TWeakObjectPtr<AWRCharacterBase> Who; double At = 0.0; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, TArray<FHitBy>> Hits;
	TMap<TPair<TWeakObjectPtr<AWRCreature>, TWeakObjectPtr<AWRCreature>>, float> Pairs;
};
