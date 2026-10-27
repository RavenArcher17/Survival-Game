#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include <atomic>
#include "WRFxSystem.generated.h"

class UMaterialInterface;
class UPointLightComponent;

/**
 * The effects runtime behind WRFx (see WRFx.h for the calls). A small CPU sprite particle system made for this game:
 * every effect is a recipe in Data/fx.json (layers of sprites from one atlas: how many, how fast, how they fall, fade
 * and glow), simulated on the game thread and drawn as camera-facing quads in two batches, additive glow (M_FxAdd)
 * and lit translucent smoke and debris (M_FxLit). Pools everything: a fixed particle budget per batch, a handful of
 * point lights handed out by importance, emitters reused. Client-side only; the server sends effect events through
 * AWRFxRelay. Chosen over Niagara because Niagara systems can't be authored headless in this pipeline, and these cook
 * as two materials and a texture.
 */

/** One sprite as the renderer draws it (world position; size and colour already for this frame). */
struct FWRFxSprite
{
	FVector Pos = FVector::ZeroVector;
	/** Velocity times the layer's stretch (cm): drawn as a streak along it, head at Pos. Zero = round. */
	FVector3f Stretch = FVector3f::ZeroVector;
	/** Flat sprites (ground rings) lie across this normal; zero = facing the camera. */
	FVector3f Normal = FVector3f::ZeroVector;
	float Size = 10.f;
	float Rot = 0.f;
	float Intensity = 1.f;
	/** Added to the distance when blended sprites are sorted (cm): a lava pool's crust draws under its molten patches. */
	float SortBias = 0.f;
	/** How softly it fades where it meets geometry (cm, depth fade): puffs ~25, sprites lying on the ground ~3. */
	float Soft = 25.f;
	FColor Color = FColor::White;
	uint8 Cell = 0;
};

/** Draws a batch of sprites (one material) as camera-facing quads built per view on the render thread. */
UCLASS(NotBlueprintable)
class WILDREACH_API UWRFxSpriteComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UWRFxSpriteComponent();

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Material;
	/** Back to front for alpha blending (additive doesn't care about order). */
	bool bSortByDistance = false;
	/** Filled by the subsystem each frame, handed to the render thread at the end of it. */
	TArray<FWRFxSprite> Sprites;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SendRenderDynamicData_Concurrent() override;
};

/** Client-local holder of the sprite batches and the light pool. */
UCLASS(NotPlaceable, Transient)
class WILDREACH_API AWRFxPool : public AActor
{
	GENERATED_BODY()

public:
	AWRFxPool();
	UPROPERTY() TObjectPtr<UWRFxSpriteComponent> Glow;
	UPROPERTY() TObjectPtr<UWRFxSpriteComponent> Lit;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
};

/** Server-spawned, always relevant: carries the server's effect events (WRFx::Broadcast) to every client. Reliable: several
 *  arrive in one frame (a fire wall's patches, a cast and its spell) and unreliable multicasts past two per update drop. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRFxRelay : public AActor
{
	GENERATED_BODY()

public:
	AWRFxRelay();

	UFUNCTION(NetMulticast, Reliable) void MulticastAt(FName Effect, FVector_NetQuantize Location, FRotator Rotation, float Scale, float Seconds, FName Sound);
	UFUNCTION(NetMulticast, Reliable) void MulticastOn(FName Effect, AActor* Target, FName Socket, float Scale, float Seconds, FName Sound);
};

/** Camera shake by "trauma": hits add to it, it decays, and the view wobbles by trauma squared (Perlin noise). */
UCLASS()
class WILDREACH_API UWRFxCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	void AddTrauma(float Amount);
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	float Trauma = 0.f;
	float Clock = 0.f;
};

namespace WRFxDetail
{
	/** One layer of an effect recipe: a kind of sprite and how it spawns, moves, fades and glows. */
	struct FLayer
	{
		uint8 Cell = 0;
		bool bAdditive = true;
		float Count = 0.f;            // burst at the start
		float Rate = 0.f;             // per second while the effect runs
		FVector2f Life = FVector2f(0.5f, 1.f);
		FVector2f Size = FVector2f(10.f, 20.f);
		float Grow = 1.f;             // size at the end of life, x the start
		FVector2f Speed = FVector2f(0.f, 0.f);
		FVector3f Dir = FVector3f(0.f, 0.f, 1.f);   // in the effect's frame (X = its rotation's forward)
		float Spread = 180.f;         // half-angle around Dir, degrees
		float Gravity = 0.f;          // x 980 cm/s2 down (negative rises)
		float Drag = 0.f;             // 1/s
		FVector2f Spin = FVector2f(0.f, 0.f);
		float Stretch = 0.f;          // streak length = speed x this (s)
		TArray<FLinearColor> Colors;  // over life, evenly spaced
		float Intensity = 1.f;        // additive: HDR brightness; lit: self-glow
		float Radius = 0.f;           // spawn inside this sphere (or disc)
		bool bDisc = false;           // spawn on the effect's XY plane only
		bool bRing = false;           // spawn on the disc's rim
		float Line = 0.f;             // spawn along the effect's X, 0..Line (cm)
		float LineDelay = 0.f;        // s per metre along the line (ice lance runs out from the caster)
		float Delay = 0.f;            // burst particles start up to this late (s)
		bool bFlat = false;           // lies across the effect's Z (rings on the ground)
		bool bFollow = false;         // moves with the effect (a bolt's core, a hand's glow)
		bool bScaleSpeed = false;     // the effect's scale scales speed too (a wider ring reaches its edge)
		bool bScaleSize = true;       // the effect's scale scales sprite size
		float CountExp = 0.f;         // count and rate x scale^this (1: along a line, 2: over an area)
		bool bRadial = false;         // flies out from the effect's centre (Dir.Z still lifts it)
		FVector3f Box = FVector3f::ZeroVector;   // spawn inside this box too (half extents, effect frame)
		FVector2f Rot = FVector2f(0.f, 360.f);   // starting roll, degrees
		float Turbulence = 0.f;       // cm/s2 of random wander
		float Bounce = -1.f;          // >= 0: lands on the ground under the effect and bounces this much
		float Inherit = 0.f;          // takes this much of the effect's own velocity
		float Lift = 0.f;             // spawn this high above the effect (cm, scaled)
		float SortBias = 0.f;         // lit sprites: drawn as if this much further away (layering flat sprites)
		float Soft = 25.f;            // depth-fade distance into geometry (cm); flat sprites default to 3
	};

	/** An effect recipe. */
	struct FDef
	{
		TArray<int32> Layers;
		bool bLoop = false;
		FLinearColor LightColor = FLinearColor::Black;
		float LightIntensity = 0.f;
		float LightRadius = 0.f;
		float LightSeconds = 0.3f;    // one-shots: how long the flash takes to fade
		float LightFlicker = 0.f;
		float LightLift = 0.f;
		FName Shake;                  // tuning fx.shake.<name>
		float Cull = 0.f;             // skip beyond this from the camera (cm); 0 = the global fx.cullDistance
		float Seconds = 2.f;          // a looping effect started without a duration (SpawnAt) runs this long
		float Linger = 0.f;           // one-shots: how long the emitter outlives its burst (light, following sprites)
	};

	struct FParticle
	{
		FVector Pos;                  // world, or offset from the emitter when the layer follows it
		FVector3f Vel;
		float Age;                    // < 0: waiting to appear
		float Life;
		float Size0;
		float Size1;
		float Rot;
		float Spin;
		float FloorZ;
		FVector3f Normal;
		int32 Layer;
		int32 Emitter;                // following layers only
		FColor Tint;
	};

	struct FEmitter
	{
		FName Name;
		const FDef* Def = nullptr;
		TWeakObjectPtr<USceneComponent> Anchor;
		FName Socket;
		bool bAnchored = false;
		FTransform Xf;
		FVector Velocity = FVector::ZeroVector;
		float Scale = 1.f;
		float RateScale = 1.f;
		float Age = 0.f;
		float Seconds = 0.f;          // > 0: a looping effect stops after this
		float FloorZ = -1e9f;
		FColor Tint = FColor::White;
		TArray<float> Acc;            // spawn accumulators per layer
		int32 Light = INDEX_NONE;
		float StopAge = -1.f;         // when it stopped (light fades out)
		uint32 Serial = 0;
		bool bLive = false;
		bool bStopped = false;
	};

	struct FLightSlot
	{
		int32 Emitter = INDEX_NONE;
		uint32 Serial = 0;
		float Priority = 0.f;
	};
}

/** Effect recipes (Data/fx.json), simulation, pools, and the ambient effects (Hearths, torches, lava, weather). */
UCLASS()
class WILDREACH_API UWRFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	static UWRFxSubsystem* Get(const UObject* WorldContext);
	/** This machine draws effects (a rendering client, listen server or standalone). */
	bool IsActive() const { return bActive; }

	/** Start an effect: at Xf, or following Anchor (a component; Socket on it). Returns a handle (or INDEX_NONE).
	 *  Looping effects: Seconds > 0 ends it then; 0 = until stopped when anchored, else the recipe's "seconds"; < 0 = until stopped. */
	int32 Start(FName Effect, const FTransform& Xf, float Scale, float Seconds, USceneComponent* Anchor = nullptr, FName Socket = NAME_None, FColor Tint = FColor::White);
	void StopEmitter(int32 Id);
	/** Emitter that follows this anchor (from WRFx::Attach). */
	int32 FindByAnchor(const USceneComponent* Anchor) const;
	/** Scale an emitter's spawn rate (weather density); 0 pauses it. */
	void SetRateScale(int32 Id, float RateScale);
	void MoveEmitter(int32 Id, const FTransform& Xf);
	bool HasEffect(FName Effect) const { return Defs.Contains(Effect); }
	/** Camera shake strength for a named group in tuning.json fx.shake.<name> (strength, radius). */
	void ShakeNamed(FName Group, const FVector& At, float Scale = 1.f);

	FVector GetViewLocation() const { return ViewLocation; }
	/** Re-read Data/fx.json (console: WR.FxReload). */
	void LoadDefs();
	AWRFxRelay* GetRelay() const { return Relay.Get(); }
	/** The impact effect for a hit on this character: by creature id (fx.json "impacts"), survivors bleed. */
	FName ImpactFor(const AActor* Target) const;
	/** Sockets a cast's hand glow tries, in order (fx.json "handSockets"). */
	const TArray<FName>& GetHandSockets() const { return HandSockets; }

	/** What the HUD layer draws (WRFxHud.cpp): recent hits on this machine, the hit marker, the parry flash. */
	struct FHudHit
	{
		TWeakObjectPtr<AActor> Target;
		FVector World = FVector::ZeroVector;
		float Damage = 0.f;
		EWRHitResult Result = EWRHitResult::Hit;
		uint16 Flags = 0;
		double Time = 0.0;
		bool bMine = false;
		bool bOnMe = false;
		float Drift = 0.f;
	};
	TArray<FHudHit> HudHits;
	double MarkerTime = -100.0;
	uint16 MarkerFlags = 0;
	EWRHitResult MarkerResult = EWRHitResult::Hit;
	double ParryFlashTime = -100.0;
	void AddHudHit(AActor* Target, const FVector& Point, float Damage, EWRHitResult Result, uint16 Flags, bool bMine, bool bOnMe);

	/** Stats for `WR.FxStats` and the benchmark. */
	int32 NumParticles() const;
	int32 NumEmitters() const;
	/** Game-thread cost of the effects tick, averaged since the last WR.FxStats (ms per frame). */
	double TickSeconds = 0.0;
	int32 TickFrames = 0;
	/** Effects started by name since the last WR.FxStats (shows the server's events arrive). */
	TMap<FName, int32> StartCounts;
	/** Render-thread cost of building the sprite quads (microseconds, summed by the proxies). */
	static std::atomic<int64> RenderMicros;
	static std::atomic<int32> RenderFrames;

private:
	using FLayer = WRFxDetail::FLayer;
	using FDef = WRFxDetail::FDef;
	using FParticle = WRFxDetail::FParticle;
	using FEmitter = WRFxDetail::FEmitter;
	using FLightSlot = WRFxDetail::FLightSlot;

	/** Smear: how far the effect moved this frame; a moving effect's sprites spread back along it (a bolt's trail stays continuous). */
	void SpawnLayer(FEmitter& E, int32 EmitterIndex, int32 LayerIndex, int32 Count, const FVector& Smear = FVector::ZeroVector);
	void Simulate(float DeltaTime);
	void UpdateEmitters(float DeltaTime);
	void UpdateLights(float DeltaTime);
	void BuildSprites();
	int32 GrabLight(float Priority);
	void TickAmbient(float DeltaTime);
	void TickWeather(float DeltaTime);
	void TickHitStops(float DeltaTime);
	FVector CameraLocation() const;

	const FDef* FindDef(FName Effect) const;
	void ReleaseEmitter(int32 Index);
	int32 Resolve(int32 Handle) const;

	TMap<FName, FDef> Defs;
	TArray<FLayer> Layers;
	TMap<FName, FName> Impacts;
	TArray<FName> HandSockets;
	TWeakObjectPtr<AWRFxRelay> Relay;
	TArray<FParticle> Particles[2];
	TArray<FEmitter> Emitters;
	TArray<int32> FreeEmitters;
	TArray<FLightSlot> LightSlots;
	TWeakObjectPtr<AWRFxPool> Pool;
	TSet<FName> Warned;
	FVector ViewLocation = FVector::ZeroVector;
	bool bActive = false;
	uint32 NextSerial = 1;
	float Clock = 0.f;

	// Ambient: fires on Hearths and light pieces, embers over lava, statuses on characters, weather at the camera.
	struct FAmbient { TWeakObjectPtr<AActor> Actor; TWeakObjectPtr<UPointLightComponent> Light; float BaseIntensity = 0.f; int32 Emitter = INDEX_NONE; FName Effect; float Seed = 0.f; };
	TArray<FAmbient> Ambient;
	struct FStatusFx { TWeakObjectPtr<AActor> Actor; int32 Burn = INDEX_NONE; int32 Frozen = INDEX_NONE; };
	TArray<FStatusFx> StatusFx;
	TArray<FVector> LavaNear;
	float AmbientClock = 0.f;
	float LavaSpawnAcc = 0.f;
	int32 SnowEmitter = INDEX_NONE;
	int32 AshEmitter = INDEX_NONE;
	float Sheltered = 0.f;
	float ShelterTarget = 0.f;

public:
	/** Hit-stop bookkeeping (WRFx::HitStop): meshes this machine paused, and until when. */
	struct FHitStop { TWeakObjectPtr<class USkeletalMeshComponent> Mesh; double Until = 0.0; };
	TArray<FHitStop> HitStops;
};
