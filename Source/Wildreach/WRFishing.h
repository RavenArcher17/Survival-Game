#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRFishing.generated.h"

class AWRCharacter;
class AWRPlayerController;
class UProceduralMeshComponent;
class FSlateWindowElementList;
struct FGeometry;

/** One kind of fish (Data/fish.json). */
struct FWRFishDef
{
	FName Id;
	FString Name;
	FName Item;               // what lands in the bag
	FName Biome;              // volcanic / tundra / any
	FName Water;              // shallow / open / deep (by the depth where the float lies)
	FName Time;               // day / night / any
	FName Rarity;             // common / uncommon / rare / legendary
	float KgMin = 0.5f, KgMax = 2.f;
	float Fight = 0.4f;       // 0..1: how hard and often it surges
};

/** The fish table and the rules for which one bites (pure, so the numbers live in data). */
class WILDREACH_API FWRFishRegistry
{
public:
	static FWRFishRegistry& Get();
	const TArray<FWRFishDef>& All() const { return Fish; }
	const FWRFishDef* Find(FName Id) const;
	/** Which fish takes the bait here: weighted by rarity (tuning fishing.rarityWeight.*), x patient angler / a lure on
	 *  the rare ones; legendaries only with Legend catch, deep water only with Deep line. Null: nothing bites here. */
	const FWRFishDef* Pick(FName Biome, FName Water, bool bNight, float RareBoost, bool bLegend, float Random01) const;
	/** "Lost things" a Treasure hook brings up (fishing.treasure in fish.json): an item and how many. */
	TPair<FName, int32> Treasure(float Random01) const;

private:
	FWRFishRegistry();
	TArray<FWRFishDef> Fish;
	TArray<TTuple<FName, int32, int32, float>> TreasureTable;   // item, min, max, weight
};

UENUM()
enum class EWRFishState : uint8
{
	Flying,     // the cast's arc out to the water
	Waiting,    // on the water: nibbles, then a bite
	Bite,       // the float plunges: strike now
	Hooked,     // hold the attack to reel, ease off when it surges
	Done        // landed, lost or reeled in: the float is pulled back and goes
};

/**
 * A cast float and the fish on its line. The server runs it all (where it lands, when it bites, which fish, the
 * fight, the catch); everyone sees the float, its line from the rod tip, its dips, plunges and splashes; its owner
 * also gets the line's tension and length for the HUD. Spawned by AWRCharacter::ServerFishInput (a rod in hand,
 * Light held and released to cast); numbers in tuning "fishing", fish in Data/fish.json.
 */
UCLASS()
class WILDREACH_API AWRBobber : public AActor
{
	GENERATED_BODY()

public:
	AWRBobber();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: cast from the angler's rod; Charge 0..1 sets the distance. Null (and a notice) when it can't. */
	static AWRBobber* CastFrom(AWRCharacter* Angler, float Charge);
	/** Server: the attack pressed (reel in, strike, start reeling) / released (stop reeling). */
	void Press();
	void Release();
	/** Server: ends it (pulled back), telling the angler why if Why isn't empty. */
	void Finish(const FString& Why);

	AWRCharacter* GetAngler() const { return Angler; }
	EWRFishState GetState() const { return State; }
	float GetTension() const { return Tension / 255.f; }
	float GetLineMetres() const { return LineCm / 100.f; }
	bool IsSurging() const { return bSurge; }
	/** The float this player's pawn has out, on this machine (none: null). */
	static AWRBobber* FindFor(const AActor* Angler);

	/** The HUD's fishing layer: the cast's charge, the tension arc, what the button does now (WRFxHud draws it). */
	static int32 PaintHud(AWRPlayerController* PC, const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Size, int32 VX, int32 VY);

private:
	UPROPERTY(Replicated) TObjectPtr<AWRCharacter> Angler;
	UPROPERTY(Replicated) EWRFishState State = EWRFishState::Flying;
	/** Where the float is (server's truth; clients ease their float toward it). */
	UPROPERTY(Replicated) FVector_NetQuantize10 FloatAt;
	UPROPERTY(Replicated) FVector_NetQuantize10 CastFromAt;
	UPROPERTY(Replicated) float CastSeconds = 0.6f;
	UPROPERTY(ReplicatedUsing=OnRep_Nibble) uint8 NibbleSeq = 0;
	UPROPERTY(ReplicatedUsing=OnRep_Bite) uint8 BiteSeq = 0;
	UPROPERTY(Replicated) bool bSurge = false;
	UPROPERTY(Replicated) bool bReelingRep = false;   // the reel's ratchet is heard while it turns
	UPROPERTY(Replicated) uint8 Tension = 0;   // owner only: 0..255
	UPROPERTY(Replicated) float LineCm = 0.f;  // owner only
	UFUNCTION() void OnRep_Nibble();
	UFUNCTION() void OnRep_Bite();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> FloatMesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> LineMesh;

	void ServerTick(float Dt);
	void ClientTick(float Dt);
	void Land();
	void Snap();
	const FWRFishDef* ChooseFish() const;
	/** Takes the best bait in the angler's bag for this water (a lure for its biome first); false if none. */
	bool ConsumeBait();
	FName BiomeHere() const;
	FName WaterHere() const;
	void Notify(const FString& Text) const;

	// Server state
	double StartedAt = 0.0;
	double NextAt = 0.0;          // the next nibble, bite, bite's end or surge change
	TArray<double> Nibbles;
	FVector AnglerSpot = FVector::ZeroVector;
	FVector CastDir = FVector::ForwardVector;
	FVector LandAt = FVector::ZeroVector;
	bool bBaited = false;         // bait in the bag when cast (bites sooner)
	FName Lure;                   // a lure for this biome was in the bag when cast
	bool bReel = false;
	float StrainFor = 0.f;        // seconds at full tension
	float TensionF = 0.f;
	float Sway = 0.f;             // the fish's pull across the line
	double SurgeEnds = 0.0;
	const FWRFishDef* Fish = nullptr;
	float FishKg = 0.f;
	float FishFight = 0.4f;
	double DoneAt = 0.0;

	// Client state
	FVector Shown = FVector::ZeroVector;
	bool bShownSet = false;
	double DipAt = -10.0, PlungeAt = -10.0;
	double NextRatchet = 0.0;
};
