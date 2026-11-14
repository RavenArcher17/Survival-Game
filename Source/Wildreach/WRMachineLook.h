#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRMachine.h"
#include "WRMachineLook.generated.h"

class AWRBuildPiece;
class UAudioComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FWRPieceDef;
struct FWRMachineDef;

/**
 * How machines look and move (cooks/build-machinery.md 2.6), drawn from the building kit's blocks (planks, coursed
 * stone, iron, bronze and cloth) like the stations: a frame that stands still and assemblies that turn about their real
 * axles. Every moving part is driven by one angle per machine that advances at its replicated speed, eased up and down
 * like a heavy wheel, so gears stay meshed (each turns at its radius ratio), sails sweep, saws stroke on their cranks,
 * stamps lift on their cams and drop, cleats run round their drums and items glide along the belt. Nothing snaps.
 * The ports (where shafts join) are part of the model: LocalPorts is the one place they're defined.
 */
struct WILDREACH_API FWRMachineLook
{
	/** The pieces drawn here: every machine and the lava forge. */
	static bool Handles(const FWRPieceDef& Def);
	/** Its ports in its own space (Hub: a wheel's axle height over its piece). */
	static TArray<FWRPort> LocalPorts(const FWRPieceDef& Def, float Hub);
	/** A conveyor's head (true) or tail end, on the belt's surface, in its own space. */
	static FVector BeltEnd(const FWRMachineDef& M, bool bHead);
	/** Where the pump's trough spills (an irrigation channel joins there), in its own space. */
	static FVector PumpOutlet() { return FVector(205.f, 0.f, 30.f); }
	/** Collision boxes (size, centre) in its own space: what you bump into is what you see. */
	static TArray<TPair<FVector, FVector>> CollisionBoxes(const FWRPieceDef& Def, float Hub);
	/** Builds the look on a piece (a ghost too, at rest); registers it to move on machines that draw. */
	static void Build(AWRBuildPiece* Piece, const FWRPieceDef& Def);
	/** Lifts: the platform's world place at a server time (every machine; the server moves its collision too). */
	static bool LiftPlatform(const AWRBuildPiece* Piece, float ServerTime, FVector& OutWorld);
	/** Of two linked lifts, the one whose platform it is (the other only calls it). */
	static bool OwnsPlatform(const AWRBuildPiece* Piece);
};

/** One moving assembly of a machine (client), turned / slid / swung from the machine's drive angle each frame. */
enum class EWRMotion : uint8 { Spin, Slide, Swing, Cam, Rod, CrankSlide, Beam, BeamTip, Cleat, Cap, Fan, Chute, Mirror, Platform, Coil, Free, TurretYaw, TurretPitch };

struct FWRMovePart
{
	TWeakObjectPtr<USceneComponent> Comp;
	EWRMotion Motion = EWRMotion::Spin;
	FVector Axis = FVector::UpVector;   // in its parent's space
	FVector Base = FVector::ZeroVector;
	FQuat BaseRot = FQuat::Identity;
	float Ratio = 1.f;                  // turns per drive turn
	float Amp = 0.f;                    // slide cm / swing degrees / cam lift cm
	float Phase = 0.f;                  // radians
	// Crank and rod: the crank's centre and axis, its throw, the rod's length, the slider's line and where it rests.
	FVector C = FVector::ZeroVector, S = FVector::UpVector, U = FVector::ForwardVector, V = FVector::RightVector;
	float CrankR = 0.f, RodL = 0.f;
	// Cleats round a belt: the loop's half length, drum radius, top height, the cleat's share of the loop.
	float HalfLen = 0.f, DrumR = 0.f, TopZ = 0.f, LoopAt = 0.f;
	// Mirrors: the point they throw the sun on (piece space).
	FVector Aim = FVector::ZeroVector;
};

/** A machine as one machine draws it: its parts, its eased speed and angle, light, sound and what rides its belt. */
struct FWRMachineVisual
{
	TWeakObjectPtr<AWRBuildPiece> Piece;
	TArray<FWRMovePart> Parts;
	float Rpm = 30.f;
	float Speed = 0.f;
	float Angle = 0.f;
	float CapYaw = 0.f;
	float ChuteYaw = 0.f;
	float FanSpin = 0.f;
	float CoilSpin = 0.f;
	TWeakObjectPtr<UPointLightComponent> Light;
	float LightIntensity = 0.f;
	TWeakObjectPtr<UAudioComponent> Loop;
	FName LoopEvent;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Riders;
	TArray<FName> RiderItems;
	float BeltLen = 200.f;
	float BeltTop = 72.f;
	float CamWas = 0.f;
	float FxClock = 0.f;
	FVector LastTarget = FVector::ZeroVector;
	float LastTrip = 0.f;
	bool bWasTurning = false;
	TWeakObjectPtr<UStaticMeshComponent> Cable;
	TWeakObjectPtr<USceneComponent> Platform;
	/** Shafts and gearboxes: which ports carry a drive pulley into a station (its belt down into the station's side). */
	TArray<int32> Pulleys;
	float StationLook = 0.f;
	/** Turrets (WRTurret): the head's eased turn and slope, the last tell / shot seen, the aim line drawn in the tell. */
	bool bTurret = false;
	float TurretYaw = 0.f;
	float TurretPitch = 0.f;
	float LastTell = 0.f;
	float LastShot = 0.f;
	TWeakObjectPtr<USceneComponent> TurretHead;
	TWeakObjectPtr<UStaticMeshComponent> AimLine;
};

/**
 * Turns every machine's parts, eases their speed, lights them, plays their loops and moves what rides their belts
 * (every machine but a dedicated server). On a server it moves the updraft lifts' platform collision too, so riders
 * stand on it where everyone sees it.
 */
UCLASS()
class WILDREACH_API UWRMachineLookSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWRMachineLookSubsystem* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate() && Visuals.Num() > 0; }
	void Add(const FWRMachineVisual& Visual) { Visuals.Add(Visual); }

private:
	TArray<FWRMachineVisual> Visuals;
	void TickOne(FWRMachineVisual& V, float Dt, const FVector& Eye);
	/** A turret's head after the server's aim, its tell's red line and sound, its shot's puff (WRTurret). */
	void TickTurret(FWRMachineVisual& V, float Dt, float Now);
};
