#pragma once

// Storm peaks (cooks/biome-lightning.md, cooks/build-biome-lightning.md): the shared calls between the land
// (WRStormWorld.cpp: sites, rods, bridges, hazards), the weather's lightning (WRSurvival.cpp: UWRWeatherSubsystem),
// the creatures and bosses (WRCreature / WRBoss) and the Lightning path (WRAbilities). One place, so a strike from any
// source (the storm, a boss, Tempest) looks, sounds and resolves the same.

#include "CoreMinimal.h"

class AActor;
class AWRCharacterBase;

/** Who called a strike down (decides who it may hurt and what counts it). */
enum class EWRStrikeSource : uint8
{
	Weather,      // the storm picked a spot (above 600 m in a Thunderstorm; everywhere in the Great Storm)
	Creature,     // a creature or boss called it (Galvara's rain, Kharvox's called strikes, a Raiju's arrival)
	Player        // a player's ability (Tempest, Thunder god, Blizzard storm): hurts only what that player may hurt
};

/** One strike request. The strike always lands on the SPOT (never follows anyone). */
struct FWRStrikeCall
{
	FVector Spot = FVector::ZeroVector;
	/** The 3 s body pre-tell on the picked pawn (hair lift, St Elmo's corona, the buzz) before the ground glow; 0 = none. */
	float PreTell = 0.f;
	/** The ground glow and crackle before it lands (>= 1.0 s for anything that hits players). */
	float Tell = 1.f;
	float Damage = 60.f;
	float Shock = 0.f;          // shock buildup on everyone hit
	float Stun = 0.8f;          // seconds (Grounded: none)
	float Radius = 300.f;       // cm
	EWRStrikeSource Source = EWRStrikeSource::Weather;
	TWeakObjectPtr<AActor> Instigator;
	/** The pawn the storm picked (shows the body pre-tell); the strike still lands on Spot. */
	TWeakObjectPtr<AWRCharacterBase> Picked;
	/** Rods within RodShadow of the spot take the strike instead (Kharvox's rod shadow, the Rodfield). */
	bool bRodsTake = true;
	/** Leave a fulgurite node (35% on sand or granite) and push a ground current on wet ground. */
	bool bGroundEffects = true;
};

namespace WRStorm
{
	/** Call a strike down. Server only; replicated to everyone as FX. Returns false if refused (a claim, a camp). */
	WILDREACH_API bool CallStrike(UObject* WorldContext, const FWRStrikeCall& Call);

	/** Fires on the server when any strike lands (after rods took their share): creatures use it (a Raiju arrives in
	 *  one, Rodcrests glow after one, Kharvox charges +10 when it called it). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStrikeLanded, UWorld* /*World*/, const FWRStrikeCall& /*Call*/);
	WILDREACH_API FOnStrikeLanded& OnStrikeLanded();

	/** The Storm peaks sub-zone at a point: '1' Wind Passes, '2' Cliff Paths, '3' Rope Bridges, '4' Struck Crags,
	 *  '5' Cliff Shrines, '6' the Spire, 'e' the Crown's east face, '~' sea, 0 = not Storm peaks land. */
	WILDREACH_API TCHAR ZoneAt(const UObject* WorldContext, const FVector& Pos);

	/** A Thunderstorm (or the Great Storm) is on over this point. */
	WILDREACH_API bool InStorm(const UObject* WorldContext, const FVector& Pos);

	/** The Great Storm is running (cooks/biome-lightning.md 2.15). */
	WILDREACH_API bool GreatStorm(const UObject* WorldContext);

	/** The nearest standing rod's tip within Range of Pos (rods take strikes: the Rodfield, waymarks, Kharvox's rim). */
	WILDREACH_API bool NearestRod(const UObject* WorldContext, const FVector& Pos, float Range, FVector& OutTip);

	/** Rods anything places (Kharvox's 12 rim rods, the Rodfield, waymarks, fort towers) register their tips here so
	 *  strikes find them. Server side. Returns a handle for RemoveRod (a rod that melts or falls). */
	WILDREACH_API int32 AddRod(UWorld* World, AActor* Owner, const FVector& Tip);
	WILDREACH_API void RemoveRod(UWorld* World, int32 Handle);
	/** How many strikes a registered rod has taken (Kharvox's rods melt after 6). */
	WILDREACH_API int32 RodStrikes(UWorld* World, int32 Handle);

	/** Server: a drink's timed protection (items.json use.buff): grounding, shock_ward. */
	WILDREACH_API void GrantBuff(AActor* Who, FName Id, float Seconds);
	WILDREACH_API bool HasBuff(const AActor* Who, FName Id);

	/** Lightning resistance tag for creatures of this land ("resists": ["strike", "shock"]). */
	WILDREACH_API bool ResistsStrikes(const AActor* Actor);
}
