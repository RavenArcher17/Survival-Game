#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRSiege.generated.h"

class AWRCharacter;
class AWRCharacterBase;
class AWRCreature;
class AWRPlayerController;
class AWRPlayerState;
class AWRProjectile;
class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
enum class EWRRaidSource : uint8;

/**
 * What every machine needs to draw a siege engine the same way (cookpass-building 3.6): where it stands, how it's
 * turned, and the state its mechanism is in with the server time that state began, so each machine works the motion
 * out for itself (a pendulum, a falling counterweight, a windlass) and nothing snaps between net updates.
 */
USTRUCT()
struct FWRSiegeRep
{
	GENERATED_BODY()

	/** Where it stands and its heading (the ram moves; each machine eases its own copy to these). */
	UPROPERTY() FVector_NetQuantize10 Loc = FVector::ZeroVector;
	UPROPERTY() float Heading = 0.f;
	/** The ram's body slope over its wheels (degrees, + nose up). */
	UPROPERTY() float Slope = 0.f;
	/** The trebuchet's turntable / the ballista's stock (world yaw) and the ballista's pitch. */
	UPROPERTY() float Aim = 0.f;
	UPROPERTY() float AimPitch = 0.f;
	/** Ram: 0 rest, 1 drawing back, 2 swinging. Trebuchet: 0 up, 1 winding, 2 cocked, 3 thrown. Ballista: 0 idle,
	 *  1 drawing, 2 drawn, 3 the aim line before it looses. */
	UPROPERTY() uint8 State = 0;
	UPROPERTY() float StateAt = 0.f;
	/** Ram: how far it was drawn when let go (0..1). Trebuchet: the winding (0 arm up .. 1 cocked). */
	UPROPERTY() float Wind = 0.f;
	/** Trebuchet: the release pin (0, 1, 2 = 20 / 40 / 60 m). */
	UPROPERTY() uint8 Range = 1;
	/** A stone in the trebuchet's pouch / a bolt in the ballista's groove. */
	UPROPERTY() bool bLoaded = false;
	/** When the ballista last loosed (its string snaps forward), the trebuchet last threw. */
	UPROPERTY() float ShotAt = -100.f;
};

/**
 * A siege engine (cookpass-building 3.6; biome-crystal 2.21.5): a battering ram, a counterweight trebuchet or a
 * ballista, placed from an item, crewed at its posts, broken by blows. The server decides everything (crew, motion,
 * hits); every machine draws it from Rep. Its parts are modelled in code (WRSiegeLook.cpp).
 */
UCLASS()
class WILDREACH_API AWRSiegeEngine : public AActor
{
	GENERATED_BODY()

public:
	AWRSiegeEngine();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** battering_ram / trebuchet / ballista (items.json, tuning siege.<kind>). */
	UPROPERTY(ReplicatedUsing=OnRep_Kind) FName Kind;
	UPROPERTY(Replicated) FString OwnerName;
	UPROPERTY(Replicated) float Health = 1000.f;
	UPROPERTY(Replicated) float MaxHealth = 1000.f;
	/** The ram's wet-hide roof (it takes blows from above first). */
	UPROPERTY(Replicated) float RoofHealth = 0.f;
	UPROPERTY(Replicated) FWRSiegeRep Rep;
	/** Who is at each post (ram: two handles; trebuchet: the winch, the aiming lever; ballista: the gunner). */
	UPROPERTY(Replicated) TArray<TObjectPtr<AWRCharacter>> Crew;
	/** A hauler in the ram's harness. */
	UPROPERTY(Replicated) TObjectPtr<AWRCreature> Hauler;
	/** Server: its builder's Siege engineer bonus when it was built (x damage; saved). */
	float BuiltMult = 1.f;

	bool IsRam() const { return Kind == TEXT("battering_ram"); }
	bool IsTrebuchet() const { return Kind == TEXT("trebuchet"); }
	bool IsBallista() const { return Kind == TEXT("ballista"); }
	/** How many posts it has and what each is called ("the left handle", "the winch"...). */
	int32 PostCount() const;
	FString PostName(int32 Post) const;
	/** 0 push, 1 crank, 2 aim: the body's pose at a post (WRHeroAnimInstance). */
	int32 PostPose(int32 Post) const;
	/** Where a post's crew stands, in the engine's own space (the base, not the turntable). */
	FVector PostLocal(int32 Post) const;
	/** The post Who is at, or INDEX_NONE. */
	int32 PostOf(const AActor* Who) const;
	/** The nearest free post to a point, or INDEX_NONE. */
	int32 FreePostNear(const FVector& At) const;
	/** The crank's angle now (for a crank post's hands): the winch drum, the ballista's windlass. */
	float CrankAngle() const;

	/** Server: a blow on it. Source scales it (siege.<kind>.take.<source>); Raider must be hostile to the owner. */
	void TakeHit(float Amount, AActor* By, EWRRaidSource Source, const FString& Raider, bool bFromAbove = false);
	/** May Raider's side hurt it (not the owner's clan; on PvE worlds no player may)? */
	bool MayHurt(const FString& Raider) const;

	// --- Server: crew input (from the "siege" commands)
	void ServerTake(AWRCharacter* Who, int32 Post);
	void ServerLeave(AWRCharacter* Who, bool bPushedOff = false);
	void ServerMove(AWRCharacter* Who, int32 Forward, int32 Right);
	void ServerAct(AWRCharacter* Who, bool bDown);
	void ServerAim(AWRCharacter* Who, float Yaw, float Pitch);
	/** Packing up is a held Interact (Aaron 11-16, r-packup): the hold's start, then its end once packHoldSeconds have
	 *  passed (server-timed); refused with a reason. */
	FString ServerPackBegin(AWRCharacter* Who);
	FString ServerPack(AWRCharacter* Who);
	void ServerPackCancel(AWRCharacter* Who);
	/** Why Who may not pack it up now (empty: they may). Every machine (the prompt), from replicated state. */
	FString PackBlock(const AActor* Who) const;
	/** Server time of the last blow it took (a hit since the hold began stops it). */
	UPROPERTY(Replicated) float LastHitAt = -100.f;
	FString ServerHarness(AWRCharacter* Who);

	/** The trebuchet's stone path for its current aim and pin: where the sling lets go, its velocity, gravity (cm/s2). */
	bool ThrowPath(FVector& OutFrom, FVector& OutVelocity, float& OutGravity) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void OnRep_Kind();
	void Build();
	void ServerStep(float Dt);
	void StepRam(float Dt);
	void StepTrebuchet(float Dt);
	void StepBallista(float Dt);
	void Break(AActor* By);
	void ClientStep(float Dt);
	void DrawArc();
	void DrawAimLine(bool bShow, const FVector& From, const FVector& To);
	float ServerNow() const;
	float Num(const TCHAR* Key, float Fallback) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Blocker;
	/** The parts that move on their own (each one procedural mesh hung from its pivot). */
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Frame;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Body;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Parts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> AimLine;
	bool bBuilt = false;

	// --- Server
	int32 MoveForward[2] = { 0, 0 };
	int32 MoveRight[2] = { 0, 0 };
	bool bActHeld[2] = { false, false };
	float AimYawWant = 0.f, AimPitchWant = 0.f;
	float StrikeAt = -1.f;
	float ReleaseAt = -1.f;
	float LastCrewed = 0.f;
	float WarnClock = 0.f;
	TWeakObjectPtr<AWRCharacter> PackBy;
	float PackStartAt = -100.f;
	TSet<TWeakObjectPtr<AActor>> StruckThisSwing;

	// --- Every machine: the eased copy of where it stands, what the wheels and cranks have turned
	FVector ShownLoc = FVector::ZeroVector;
	float ShownHeading = 0.f, ShownSlope = 0.f, ShownAim = 0.f, ShownAimPitch = 0.f, ShownWind = 0.f;
	float WheelTurn = 0.f;
	float LastDrumClick = 0.f;
	bool bShownInit = false;
	float WheelSoundClock = 0.f;
	float ArcClock = 0.f;
	uint8 LastState = 0;
	uint8 ShownPin = 255;
};

/** The siege engines' rules, item use, prompts and commands (WRSiege.cpp). */
struct WILDREACH_API FWRSiege
{
	/** Server: an engine item used from the bag or belt (items.json category "siege"): stand it in front of you. */
	static FString UseItem(AWRPlayerState* PS, FName ItemId);
	/** The engine a survivor is crewing (attached to), or null. */
	static AWRSiegeEngine* CrewingOf(const AActor* Who);
	/** The nearest engine within reach of a survivor (a post's prompt), or null. */
	static AWRSiegeEngine* NearOf(const AActor* Who, float Reach = 300.f);
	/** HUD: the prompt near an engine (or on a post). */
	static bool Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb);
	/** E near an engine: take a post / leave it (sends "siege ..."). */
	static bool ClientInteract(AWRPlayerController* PC);
	/** Server: "siege take|leave|move|act|aim|pack|harness ..."; returns a line for the player or empty. */
	static FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Server: blasts, pots, stones and spells reach engines too (FWRRaiding::SplashPieces calls it). */
	static void Splash(UWorld* World, const FVector& At, float Radius, float Amount, EWRRaidSource Source, AActor* Causer, const FString& Raider);
	/** Server: a trebuchet stone came down (AWRProjectile kind "trebuchet"). */
	static void StoneLanded(AWRProjectile* Stone, const FVector& At, AActor* HitActor, AWRCharacterBase* Thrower);
	/** Server: a harnessed hauler walks behind its ram (AWRCreatureAI). */
	static void TickHauler(AWRCreature* Hauler, AWRSiegeEngine* Ram, float DeltaSeconds);
	/** The engines in the world store (the "siege" document): saved with the world, restored at load. */
	static void Save(UWorld* World);
	static void Load(UWorld* World);
	/** This machine's survivor on a post: forward their move / look input to the engine (rate-limited commands). */
	static void TickLocalCrew(AWRCharacter* Who, float DeltaSeconds);
	/** A survivor's attack / dodge on a post: the engine's act; true when it was used. */
	static bool CrewInput(AWRCharacter* Who, int32 Input, bool bDown);
};
