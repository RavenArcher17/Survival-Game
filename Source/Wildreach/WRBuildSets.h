#pragma once

#include "CoreMinimal.h"

struct FWRPieceDef;
class AWRBuildPiece;
class AWRPlayerController;
class AActor;
class UObject;

/** How a tier's pieces look (Data/building_sets.json "look"): the WRBuildLookSets style and its colours (linear). */
struct FWRTierLook
{
	FName Style;
	FLinearColor A = FLinearColor(0.3f, 0.3f, 0.3f);   // main surface
	FLinearColor B = FLinearColor(0.2f, 0.2f, 0.2f);   // second surface (frames, bands, lintels)
	FLinearColor C = FLinearColor(0.1f, 0.1f, 0.1f);   // accent (copper, coral, snow, crystal)
	FLinearColor Glow = FLinearColor::Black;           // what glows (ember glass, sunstone, crystal)
	FLinearColor Glass = FLinearColor(0.1f, 0.12f, 0.14f);
	FLinearColor Trim = FLinearColor(0.1f, 0.08f, 0.06f);
	float Metal = 0.f;
	float Rough = 0.85f;
	float GlassGlow = 0.f;
};

/** A building tier past wood and stone, or wood / stone for their beams (building_sets.json "tiers"). */
struct FWRTierDef
{
	FName Id;
	FString Name;
	FName Biome;          // the land it belongs to (shown while that land is live)
	FName Block;          // its biome block (what it costs and its upkeep)
	FName Boss;           // the major boss whose blueprint teaches its decor
	FName Signature;      // the land's signature resource
	int32 Hearth = 0;
	int32 Span = 0;
	float WallHp = 400.f;
	float FoundationHp = 0.f;
	float DoorHp = 0.f;
	TArray<TPair<FName, int32>> Wall;
	FName UpkeepItem;
	float Upkeep = 0.f;
	bool bFreeformOnly = false;
	FWRTierLook Look;
	bool IsBiome() const { return !Biome.IsNone(); }
};

/**
 * Metal, the 12 biome building sets, freeform beams and posts, and the sets' decor (cooks/build-building.md). The tiers
 * live in Data/building_sets.json and are expanded into ordinary pieces (FWRBuildRegistry), so placing, saving, support,
 * upkeep and the build bar treat them as they treat wood and stone.
 */
class WILDREACH_API FWRBuildSets
{
public:
	static const FWRBuildSets& Get();
	const FWRTierDef* Tier(FName Id) const { return Tiers.FindByPredicate([Id](const FWRTierDef& T) { return T.Id == Id; }); }
	const TArray<FWRTierDef>& All() const { return Tiers; }
	/** What share of a weapon's or tool's hit a piece of this material takes (wood 0.1, stone 0.02, metal / biome 0). */
	float WeaponShare(FName Material) const;

	/** FWRBuildRegistry's constructor: the generated pieces, their spans and upkeep. */
	static void Expand(TArray<FWRPieceDef>& Pieces, TMap<FName, int32>& Span, TMap<FName, float>& Upkeep, TMap<FName, FName>& UpkeepItem);

	/**
	 * The rules every piece meets wherever it goes (client ghost and server alike; empty = fine): its blueprint is
	 * learned, it's not a base piece in a cave (no underground bases), a hot spring bath stands on a vent.
	 */
	static FString PlaceCheck(const UObject* WorldContext, const FWRPieceDef& Def, const FVector& Location, const AActor* Builder);
	/** Metal and biome set pieces (and their beams and decor): Master builder makes them cheaper. */
	static bool IsHighTier(const FWRPieceDef& Def);
	/** Building XP for placing it: metal and biome pieces 12, the rest 8. */
	static float PlaceXP(const FWRPieceDef& Def);
	/** The blueprint's teacher, for the card ("Vulkarn drops its blueprint"). */
	static FString BlueprintSource(FName Blueprint);

	/** The Hearth offering (survival-crafting 2.2): E on your Hearth in its first game day with a cooked meal in your bag
	 *  leaves the meal on it. Empty: nothing happened (E goes on to the Hearth). */
	static FString TryOffer(AWRBuildPiece* HearthPiece, AWRPlayerController* Player);
	/** Comfort a fed Hearth adds to a bed in its claim. */
	static int32 OfferingComfort(const UObject* WorldContext, const FVector& BedAt);

private:
	FWRBuildSets();
	TArray<FWRTierDef> Tiers;
	TMap<FName, float> Damage;
};
