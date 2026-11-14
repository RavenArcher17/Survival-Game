#pragma once

#include "CoreMinimal.h"
#include "WRMachineTypes.generated.h"

/** What a machine is doing, one word for the prompt, the screen and the ghost (cooks/build-machinery.md 2.7). */
UENUM()
enum class EWRMachineState : uint8
{
	Idle,        // nothing to do (an empty smelter, a lamp by day, a source nothing draws on)
	Running,
	Slow,        // running, the network short of power (speed under 100%)
	NoPower,     // switched on with work but its network gives nothing
	OutputFull,
	Off,         // switched off on its screen
	Worn,        // running at half speed until repaired
	Asleep,      // no one within the active range: it holds where it was
	NoWater,     // a water wheel / pump / still / tide mill without its water
	NoWind,      // a windmill blocked or becalmed
	NoHeat,      // a turbine with no lava by it
	NoFuel,      // a soul engine with no soulstone
	NeedsDark,   // a mycelium vat in daylight
	ChainLong,   // a belt that won't feed the 5th machine of its chain
	NoLink,      // an updraft lift with no partner
	NoStorm,     // a storm coil between storms with a flat battery
	NoSun,       // a sun mirror array at night with a flat battery
	NoAmmo,      // a turret with nothing to shoot (build-building)
};

namespace WRMachineFlag
{
	constexpr uint8 On = 1 << 0;
	constexpr uint8 Overclock = 1 << 1;
	constexpr uint8 Lit = 1 << 2;          // a lamp shining, the smelter's fire, the turbine's steam
	constexpr uint8 Working = 1 << 3;      // doing its work this step (looks: the saw strokes, the stamp falls)
	constexpr uint8 Worn = 1 << 4;
	constexpr uint8 Tundra = 1 << 5;       // a cold store on tundra ground (needs no power)
}

/** A machine's replicated state: what every machine draws and the screen and prompt read. */
USTRUCT()
struct FWRMachineRep
{
	GENERATED_BODY()

	/** How fast its drive turns now, 0..2 in 1/100ths (network speed x overclock x wear; a source: 1 while it runs). */
	UPROPERTY() uint8 Speed100 = 0;
	UPROPERTY() EWRMachineState State = EWRMachineState::Idle;
	UPROPERTY() uint8 Flags = 0;
	/** Its network's supply and load, in tenths of P; Own: this piece's own output (sources) or draw (loads) now. */
	UPROPERTY() uint16 Supply10 = 0;
	UPROPERTY() uint16 Load10 = 0;
	UPROPERTY() uint16 Own10 = 0;
	/** Pieces on its network: sources and machines (the screen's line). */
	UPROPERTY() uint8 NetSources = 0;
	UPROPERTY() uint8 NetMachines = 0;
	/** The one it's making, 0..255; a battery's charge, 0..255; the fuel left of the burning soulstone. */
	UPROPERTY() uint8 Progress = 0;
	UPROPERTY() uint8 Charge = 0;
	/** What it's working on (a processor's recipe, a drill's ore, a sorting chest's last send). */
	UPROPERTY() FName Making;
	/** Sources: why it gives what it does, as a factor (wind 0.8, a storm x2...) in 1/100ths. */
	UPROPERTY() uint16 Factor100 = 100;
	/** A sorting chest's chute: the place it last sent to (it turns there). A lift: its partner. */
	UPROPERTY() FVector_NetQuantize Target = FVector::ZeroVector;
	/** A lift: the server time its current trip began (0: resting) and where the platform rests / heads (0 here, 1 at
	 *  the partner). Every machine works out where the platform is from these, so it glides the same everywhere. */
	UPROPERTY() float TripStart = 0.f;
	UPROPERTY() uint8 TripTo = 0;
	/** A conveyor: how far its belt has run (cm) as of server time BeltT; its items' places are worked out from it,
	 *  carried on at Speed100 between updates, so they glide (FWRBeltItem::Enter is the belt's run when one came on). */
	UPROPERTY() float BeltD = 0.f;
	UPROPERTY() float BeltT = 0.f;
};

/** One stack riding a conveyor: what it is and how far the belt had run when it came on (its place is worked out from it). */
USTRUCT()
struct FWRBeltItem
{
	GENERATED_BODY()

	UPROPERTY() FName Item;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() float Enter = 0.f;
};

/**
 * A turret's replicated aim (WRTurret, cooks/build-building.md 2.6): where its head points (degrees, in the piece's own
 * space), the aim line's start (server time) and the point it runs to, its last shot. Every machine eases the head
 * toward Yaw / Pitch and draws the line from TellAt for the tell's length, so everyone sees the same warning.
 */
USTRUCT()
struct FWRTurretRep
{
	GENERATED_BODY()
	UPROPERTY() float Yaw = 0.f;
	UPROPERTY() float Pitch = 0.f;
	UPROPERTY() float TellAt = 0.f;
	UPROPERTY() FVector_NetQuantize AimAt = FVector::ZeroVector;
	UPROPERTY() float ShotAt = 0.f;
	/** What it shoots now: bolt, fire, frost, or another path's status (the loaded material's row). */
	UPROPERTY() FName Element;
	/** Shots left of the burning path material (element turrets), and a target's in sight (the head tracks). */
	UPROPERTY() uint8 ShotsLeft = 0;
	UPROPERTY() bool bTracking = false;
};
