#pragma once

// The Desert's hazards and its event (cooks/biome-desert.md 2.8-2.10; cooks/build-biome-desert.md 2.8-2.10). Numbers:
// tuning hazards.desert.* and desertEvents.longNoon.*. Everything here does nothing while the Desert isn't live.
//
//  - The belt (D5 and the strip north of it) and Sandstorm weather over the whole desert: run x0.7 (Dune oil, Dunewrap 4
//    or a sand veil ease it to x0.85; Sandwalker passes it), sprint stamina x1.3, vision 25 m (60 m in a veil or
//    Dunewrap 4: the sky's sand fog, WRSky), the compass ribbon and waypoints hidden unless you carry a Sunwatch lens or
//    your tamed Scarab digger is within 15 m (CompassHidden, the HUD asks).
//  - Soft sand (softsand / slipface / blown / shellgrit / grass surfaces): run x0.85, sprint stamina x1.3; Sandwalker,
//    a Dromad or Shimmerbuck under you pass it; the veil / Dune oil / Dunewrap halve it.
//  - Quicksand (the pockets): a wet suck 0.9 s, then you sink over 4 s (run x0.3, the body lowers into the sand on every
//    machine: SinkDepth); each dodge lifts you 25% (Dune oil: the first frees you); at full you're held (no actions) and
//    drown 5 s later. An ally pulls you out (E on you, 2 s beside you). Sandwalker doesn't sink.
//  - Brine crust (D1's patches): creaks 1.0 s, then breaks: you drop 1.5 m into the brine (run x0.4, no roll, 4 a second)
//    until you wade out of the patch and climb 2 s at its edge. The patch stays broken 2 game hours.
//  - Booming slipfaces: 1.2 s on a slipface starts the boom; then the avalanche carries you 12 m down the fall line and
//    buries you 2 s (stamina -40, 20 damage) and pops any Dunejaw within 20 m.
//  - Dust devils on the White Pan 11:00-16:00, 2-4 of them, their paths functions of the server clock (DevilAt): a
//    touch lifts you 1.2 s and throws you 6 m (20, blind 30).
//  - Flash floods down the two flood wadis after rain or a storm in the Storm peaks: the roar 6 s ahead, a trickle 3 s
//    ahead, a 2 m brown wall at 6 m/s (its front a function of the replicated start time): 50, carried 15 m, 3-5 debris
//    hits of 10. The ledges stand above it.
//  - The Long Noon (world.json events.long_noon): the sign, the held noon (AWRDesertState::bLongNoon: the sky, Radiance
//    and the creatures read it), sunstone x3 and the Pan's bloom, the tilted stelae's caches, the Seventh Vault, the
//    Fata Morgana, the Gildbacks' calm, Glasstails and Dunejaws dug in, kites x2, the 2-minute dusk after.
//  - Thrown things: the Oasis water jar (wet sand 6 m for 30 s: no Dunejaw dives there; a water hit) and the flash pot
//    (0.6 s fuse, blind 80 within 4 m to everyone facing it).

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NativeGameplayTags.h"
#include "WRDesertHazards.generated.h"

class AWRCharacterBase;
class AWRPlayerController;
class AWRCreature;

namespace WRDesertTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SoftSand);   // on soft sand, not passed: run x0.85
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Belt);       // in the belt or a sandstorm, not passed: run x0.7
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eased);      // a veil / Dune oil / Dunewrap 4: the slows above halved
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sinking);    // in quicksand: run x0.3
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held);       // sunk to the shoulders: no moving, no actions
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pit);        // in a broken brine crust: run x0.4, no roll
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Buried);     // under a slipface's avalanche: no moving
}

namespace WRDesertHz
{
	/** Both machines (the replicated tags): the Desert's run multiplier on Who. */
	WILDREACH_API float MoveScale(const AWRCharacterBase* Who);
	/** Both machines: sprint stamina x (1.3 on soft sand and in the belt). */
	WILDREACH_API float SprintStaminaMult(const AActor* Who);
	/** No roll here (a broken brine crust; held in quicksand). */
	WILDREACH_API bool NoRoll(const AActor* Who);
	/** Held fast (sunk to the shoulders, buried): no actions. */
	WILDREACH_API bool IsHeld(const AActor* Who);
	/** How far the body shows sunk into the ground (cm), eased on every machine from the replicated state. */
	WILDREACH_API float SinkDepth(const AActor* Who);
	/** The local survivor's view: the belt's sand fog is on (0..1, the sandstorm counts) and how far you see (cm). */
	WILDREACH_API float BeltAmount(const APawn* Me, float& OutVisionCm);
	/** The compass ribbon and waypoint markers hide (in the belt or a sandstorm, no lens carried, no Scarab near). */
	WILDREACH_API bool CompassHidden(const APawn* Me);
	/** Server, a dodge began: quicksand lifts you (Dune oil: frees you). */
	WILDREACH_API void OnDodge(AWRCharacterBase* Who);
	/** Wet sand here now (an Oasis water jar's splash): no Dunejaw dives, Ssarrok beaches. */
	WILDREACH_API bool WetSandAt(const UObject* WorldContext, const FVector& P);
	/** Offer-food taming x (the Long Noon's calm Gildbacks x2). */
	WILDREACH_API float TameSpeedMult(const AWRCreature* Creature);
	/** The thrown things of this file ("oasis_jar", "flash_pot"). */
	WILDREACH_API bool IsThrownKind(FName Kind);
	/** Server: one of them broke at At. */
	WILDREACH_API void ThrownLanded(AActor* Projectile, FName Kind, const FVector& At, AActor* HitActor, AWRCharacterBase* By);
	/** Where dust devil Index stands at server time T (cm, its foot on the Pan), every machine the same; false while it's
	 *  down (outside 11:00-16:00 or beyond today's count). */
	WILDREACH_API bool DevilAt(const UObject* WorldContext, int32 Index, double T, FVector& Out);
	/** Prompts: a sinking ally in front of you to pull out. */
	WILDREACH_API bool PullPrompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** Server: Puller starts pulling the sinking survivor nearest in front ("world desert pull"). */
	WILDREACH_API FString StartPull(AWRPlayerController* PC);
	/** The sky's hour where the local view is (the Long Noon holds the Desert at noon, then a short dusk eases it back);
	 *  InOutDaylight follows. Every machine, from the replicated stage. */
	WILDREACH_API float SkyHour(const UObject* WorldContext, float Hour, float& InOutDaylight);
	/** The Long Noon now (its day; not the sign). */
	WILDREACH_API bool LongNoon(const UObject* WorldContext);
}

UCLASS()
class WILDREACH_API UWRDesertHazards : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	static UWRDesertHazards* Get(const UObject* WorldContext);

	/** Server: wet sand for Seconds round At (Radius cm). */
	void Wet(const FVector& At, float Radius, float Seconds);

	struct FTrack
	{
		bool bSoft = false, bBelt = false, bEased = false;
		float SlipSeconds = 0.f;
		double SlipReadyAt = 0.0, BuriedUntil = 0.0;
		int32 Quick = INDEX_NONE;
		uint8 QuickPhase = 0;          // 0 none, 1 the suck (tell), 2 sinking, 3 held
		double QuickSince = 0.0, HeldSince = 0.0;
		float Sink = 0.f;              // 0..1
		bool bOilUsed = false;
		int32 Brine = INDEX_NONE;
		uint8 BrinePhase = 0;          // 0 none, 1 creaking, 2 in the pit, 3 climbing out
		double BrineSince = 0.0, NextBrineHurt = 0.0;
		double DevilHitAt = -100.0;
		TWeakObjectPtr<AWRCharacterBase> PulledBy;
		double PullSince = 0.0;
		int32 FloodHit = INDEX_NONE;
		int32 DebrisLeft = 0;
		double NextDebrisAt = 0.0;
	};
	TMap<TWeakObjectPtr<AWRCharacterBase>, FTrack> Tracks;
	/** Client: the eased sink each body shows (cm). */
	TMap<TWeakObjectPtr<const AActor>, float> ShownSink;
	struct FWet { FVector At; float R; double Until; };
	TArray<FWet> WetSand;

private:
	void StepServer(float Dt);
	void StepBody(AWRCharacterBase* C, FTrack& W, double T, float Dt);
	void StepDevilsServer(double T);
	void StepFloods(double T);
	void StepLongNoon();
	void StepClient(float Dt);
	void SetSink(AWRCharacterBase* C, float DepthCm);
	double ServerNow() const;

	float ServerClock = 0.f;
	TArray<FName> WasStormWeather;
	FName LastStormWeather;
	uint8 NoonStage = 0;
	TArray<TWeakObjectPtr<AActor>> BloomNodes;
	TArray<TWeakObjectPtr<AActor>> DevilLooks, FloodLooks, MorganaLooks;
	TArray<float> DevilSpin;
	bool bClientSetup = false;
};
