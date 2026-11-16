#pragma once
// NPC strongholds (Aaron 11-16, r-siege-pve: "Allow it to practice siegeing on NPC strongholds"; and q-stronghold-garrison:
// "make the NPC strongholds designed like elden ring areas, with small, medium, and boss enemies"). Designs: showcase-notes
// cooks/wild-answers-1115.md 2.7 and cooks/build-wild-strongholds.md.
//
// Two forts on the Old Island (Data/strongholds.json, laid out and sited off the heightmap by Tools/terrain/strongholds.mjs),
// layered like Elden Ring's forts: an outpost on the approach (an archer and the horn-blower), a gate front between two
// towers with bolt turrets, archers on the corner towers and the wall walk (ladders up), a breach on the far side (the side
// path, the hounds' kennel), the bailey yard (campfire, barracks, the land's work), a two-storey keep of the land's own
// building set (its ground door barred: the siege door; an outside stair to the first floor where the keep warden guards
// the war chest; the beacon on its roof), and the inner ward behind it: the warlord's arena, with a postern in its back
// wall that opens only from inside, back to the run-back stone (the shortcut).
//
// Every piece is one of the game's build pieces, owned by "@hold:<id>", so every raid rule, charge, ram, trebuchet stone and
// spell works on it unchanged, on every world (FWRRaiding::MayRaid). The turrets need no power or bolts and shoot at
// survivors and their tames (WRTurret). The garrison is people (AWRGarrison, WRGarrison.h) and the land's beasts as hounds.
// The keep's door or a keep wall falling opens the war chest, and so does the warlord's fall, which takes the fort: its
// beacon goes dark and its banners come down. While it stands, a dead soldier comes back after respawnHours with nobody
// near; once taken (or its keep breached) it stands again whole after rebuildHours with nobody near. What is broken,
// unbarred and dead is saved with the world (the "strongholds" document), so a restart doesn't mend it.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRStronghold.generated.h"

class AWRBuildPiece;
class AWRCreature;
class AWRCharacterBase;
class AWRGarrison;
class AWRSiegeLadder;
class AWRRunbackStone;
class APawn;

/** One piece of a fort's layout (game cm, its ground or floor height). */
struct FWRHoldPiece
{
	FName Piece;
	FName Role;      // gate, palisade, tower, outpost, keep_floor, keep_wall, keep_door, war_chest, ward_wall, ward_floor,
	                 // postern, beacon, banner, hall, deck, decor (anything else: a plain piece of the fort)
	FVector At = FVector::ZeroVector;
	float Yaw = 0.f;
	/** Ground pieces stand a little into the ground (no daylight under a post on a slope). */
	bool bEmbed = false;
	/** Barred doors that open from inside (gate, postern): the way in, toward the fort (world, unit). */
	FVector Inward = FVector::ZeroVector;
	/** A bolt turret rides its deck (the gate towers). */
	bool bTurret = false;
};

/** A fixed ladder (an AWRSiegeLadder owned by the fort): its foot, facing out from what it leans on. */
struct FWRHoldLadder
{
	FVector Foot = FVector::ZeroVector;
	float Yaw = 0.f;
	float Length = 560.f;
	float FootOut = 70.f;
	int32 Against = INDEX_NONE;   // the piece it leans on (it falls with it)
};

/** One soldier of a fort (strongholds.json forts[].garrison[]). */
struct FWRHoldSoldier
{
	FName Id;
	FName Rank;
	FString Name;               // a proper name for elites and the warlord (empty: the rank's title)
	FVector At = FVector::ZeroVector;
	float Yaw = 0.f;
	TArray<FVector> Route;      // patrol marks (world); empty: it keeps its post
	FName Weapon;
	FName Metal = TEXT("iron");
	bool bShield = false;
	FName Bow;
	FName Armor[4];             // head, chest, hands, legs
	FName Sex = TEXT("m");
	int32 Face = 0;
	int32 Build = 0;
	FName Warlord;              // the warlord's row (strongholds.json warlords)
};

/** Hounds of the land's kind, kennelled at a spot. */
struct FWRHoldHounds
{
	FName Creature;
	int32 Count = 2;
	FVector At = FVector::ZeroVector;
};

struct FWRHoldDef
{
	FName Id;
	FString Name;
	FString Lore;
	FString Faction;            // "Reaver": the word in front of its soldiers' ranks
	FName Biome;
	FVector Centre = FVector::ZeroVector;
	float Yaw = 0.f;
	float ClearRadius = 1700.f;
	TArray<FWRHoldPiece> Pieces;
	TArray<FWRHoldLadder> Ladders;
	TArray<FWRHoldSoldier> Garrison;
	TArray<FWRHoldHounds> Hounds;
	TArray<TPair<FName, int32>> Chest;
	/** The inner ward (the warlord's arena): its middle and half extents along the fort's X and Y (cm). */
	FVector WardCentre = FVector::ZeroVector;
	FVector2D WardHalf = FVector2D(1000.f, 1000.f);
	/** The run-back stone outside the postern, facing the ward. */
	FVector RunbackAt = FVector::ZeroVector;
	float RunbackYaw = 0.f;
	FVector FortX() const { return FRotator(0.f, Yaw, 0.f).Vector(); }
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
	/** The war chest is barred until its keep is breached or its warlord falls (its piece's bLocked, replicated). */
	static bool IsBarredChest(const AWRBuildPiece* Piece);
	/** The fort a garrison soldier or a kennelled hound belongs to (None: neither). */
	static FName HoldOf(const AActor* Actor);
	/** The layout row a fort piece stands on (every machine: from where it stands), or null. */
	static const FWRHoldPiece* RowOf(const AWRBuildPiece* Piece, const FWRHoldDef** OutFort = nullptr);
	/** A barred door that opens from inside (the gate, the postern): is By on its inside? */
	static bool IsInside(const AWRBuildPiece* Door, const APawn* By);
	/** The prompt's verb and line for a fort door (every machine): "Lift the bar" or why not. */
	static FString DoorPrompt(const AWRBuildPiece* Door, const APawn* By);
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

	/** Server: E on a fort door (AWRBuildPiece::Interact): lift its bar from inside, or say why not. */
	FString UseDoor(AWRBuildPiece* Door, APawn* By);
	/** Server: a soldier saw a survivor and the horn sounded (or a gate / keep door broke): the whole fort turns out. */
	void SoundAlarm(FName Hold, const FVector& Where, AActor* Seen, bool bHorn);
	/** Server: a soldier fell. */
	void SoldierDied(AWRGarrison* Soldier, AActor* Killer);
	/** Server: the warlord fell: the fort is taken (pays the fighters, unbars the chest, the beacon dark, the banners down). */
	void WarlordDied(AWRGarrison* Warlord, AActor* Killer);
	/** Server: a soldier is hit / sees a fight: its fellows within hearing join. */
	void Shout(FName Hold, const FVector& Where, AActor* Foe, float Range);

private:
	struct FFort
	{
		bool bBuilt = false;
		/** Layout index -> the piece standing there (stale = broken). */
		TArray<TWeakObjectPtr<AWRBuildPiece>> Pieces;
		/** Tower index -> the turret on its deck. */
		TMap<int32, TWeakObjectPtr<AWRBuildPiece>> Turrets;
		TArray<TWeakObjectPtr<AWRSiegeLadder>> Ladders;
		TSet<int32> Broken;
		/** Doors whose bar is lifted (gate, postern): they open and shut for anyone until the fort rebuilds. */
		TSet<int32> Unbarred;
		TArray<TWeakObjectPtr<AWRCreature>> Hounds;
		/** Soldier id -> the body standing for it. */
		TMap<FName, TWeakObjectPtr<AWRGarrison>> Soldiers;
		/** Soldier id -> game hours it may come back (dead while the fort stands). */
		TMap<FName, double> DeadUntil;
		TWeakObjectPtr<AWRRunbackStone> Runback;
		/** Game hours it fell (its keep breached or its warlord dead); -1 standing. */
		double FallenAt = -1.0;
		bool bWarlordDown = false;
		/** Game hours a survivor was last within quietRange (the rebuild waits rebuildHours after it). */
		double LastNear = 0.0;
		double AlarmUntil = 0.0;
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
	void TopUpHounds(const FWRHoldDef& D, FFort& F);
	void TopUpGarrison(const FWRHoldDef& D, FFort& F, bool bAll);
	AWRGarrison* SpawnSoldier(const FWRHoldDef& D, const FWRHoldSoldier& Row);
	void Fall(const FWRHoldDef& D, FFort& F, const FString& Why);
	void Unbar(const FWRHoldDef& D, FFort& F, int32 Index, APawn* By);
	bool PlayerWithin(const FVector& At, float Range) const;
	double Hours() const;
	FFort* FortOf(FName Hold, const FWRHoldDef** OutDef);
};
