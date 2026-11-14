#pragma once

#include "CoreMinimal.h"
#include "WRCharacterBase.h"
#include "WRCombatTypes.h"
#include "WRCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UGroomComponent;
class ULODSyncComponent;

/** Acceleration for other machines' copies of a survivor (Lyra's pattern): motion matching sees a start or a stop the
 *  moment the stick moves, not a round trip later when the velocity shows it. 3 bytes, plus the gait / stance bits. */
USTRUCT()
struct FWRReplicatedAccel
{
	GENERATED_BODY()
	UPROPERTY() uint8 Angle = 0;      // direction on the ground, 0..255 = 0..360 degrees
	UPROPERTY() uint8 Magnitude = 0;  // 0..255 = 0..the movement's max acceleration
	UPROPERTY() int8 Z = 0;           // -127..127 = -max..max
	UPROPERTY() uint8 Bits = 0;       // gait (2 bits), stance (3 bits), locked on (1 bit)
};
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Player survivor. One pawn, two views (first/third person, V). Input is built in code
 * (no input assets) and routed to GAS by ability input id.
 */
UCLASS()
class WILDREACH_API AWRCharacter : public AWRCharacterBase
{
	GENERATED_BODY()

public:
	AWRCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Camera") bool IsFirstPerson() const { return bFirstPerson; }
	UFUNCTION(BlueprintCallable, Category="Camera") void SetFirstPerson(bool bNewFirstPerson);

	virtual void Tick(float DeltaSeconds) override;
	/** Standing on slick ice without ice resistance (set on the server and the owning client). */
	bool IsOnIce() const { return bOnIce; }

	/** Press/release an ability input (used by real input and by the test bot). */
	void PressAbilityInput(EWRAbilityInput Input);
	void ReleaseAbilityInput(EWRAbilityInput Input);
	/** Let go of every held gameplay input (block, a charging heavy, sprint, jump): a menu, wheel, build placing, a mount
	 *  or the death screen took the buttons (WRInput::Transition). */
	void DropHeldInputs();

	/** Everyone: play a one-shot body clip from Data/player_art.json (e.g. "pickup" when looting). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastBodyOneShot(FName Key);
	/** Fishing (WRFishing): a rod is in the weapon hand. */
	bool IsHoldingRod() const;
	/** This machine: how long the cast has been wound up (0 when it isn't), for the HUD's charge ring. */
	double GetFishChargeSeconds() const;
	/** The body picked in the menu (None = the default body); the sound pass picks the voice by it. */
	FName GetBodyStyle() const { return BodyStyle; }

	// Held tools (WRHeldTool.cpp).
	/** Server (from the inventory): what's in the weapon hand, None for bare hands. A tool there shows in the hand. */
	void SetMeleeItem(FName ItemId);
	FName GetMeleeItem() const { return MeleeItem; }

	// Ranged weapons (WRRanged.cpp).
	/** Server (from the inventory): the weapon in the ranged slot, None if empty. */
	void SetRangedItem(FName ItemId);
	/** Owner: swap between sword-and-shield and the ranged weapon (G). */
	void ToggleWeaponSet();
	void SetRangedMode(bool bOn);
	/** Owner: aim the ranged weapon (right mouse while it's out). */
	void SetAiming(bool bOn);
	/** Owner: fire or throw at the crosshair (left mouse while it's out). */
	void FireRanged();
	/** Where the crosshair looks from: the survivor's camera and control rotation. */
	void GetAimView(FVector& OutLoc, FRotator& OutRot) const;
	/** The control rotation that puts the crosshair on a world point (WRAimAt). */
	FRotator AimRotationAt(const FVector& Target) const;
	/**
	 * Where the crosshair's ray lands, out to Range past the survivor (the camera's own place behind them doesn't count, so
	 * a rock at their back can't stop it). Works on the server too: the camera's place comes from the control rotation.
	 * bPawns: creatures and players count, not just the ground and what's built. False: it met nothing that far.
	 */
	bool CrosshairPoint(float Range, bool bPawns, FVector& OutPoint) const;
	/** Owner: next / previous ammo type (mouse wheel). */
	void CycleAmmo(int32 Dir);
	bool IsRangedOut() const { return bRangedMode && !RangedItem.IsNone(); }
	bool IsAimingRanged() const { return bAiming && IsRangedOut(); }
	FName GetRangedItem() const { return RangedItem; }
	FName GetSelectedAmmo() const { return SelectedAmmo; }

protected:
	virtual void BeginPlay() override;
	virtual void OnDeath(AActor* Killer) override;
	virtual void OnSwingVisual(EWRSwingKind Kind, float Duration) override;
	virtual void OnDeathVisual() override;
	virtual void OnHitVisual(EWRHitResult Result) override;

	/** Animated survivor body (Data/player_art.json); the placeholder shapes are hidden once it loads. */
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<class USkeletalMeshComponent> Body;
	/** The MetaHuman head (its own face rig follows the body's pose), the hair grooms bound to it, the outfit pieces
	 *  (Head, Chest, Hands, Legs) following the body's bones, and one LOD for them all. */
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<class USkeletalMeshComponent> Face;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TArray<TObjectPtr<UGroomComponent>> Grooms;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TArray<TObjectPtr<class USkeletalMeshComponent>> Outfit;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<ULODSyncComponent> LODSync;
	/** Shield strapped to the body's left forearm (player_art.json "shieldMesh"). */
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> ShieldMesh;
	/** The ranged weapon in hand while it's out. */
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> RangedMesh;
	bool bShieldShown = false;
	/** Body picked in the menu (?Body= on the join URL, kept on the PlayerState): a variant under "bodies" in
	 *  Data/player_art.json. None = the default body. */
	UPROPERTY(ReplicatedUsing=OnRep_BodyStyle) FName BodyStyle;

	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<USpringArmComponent> ThirdPersonBoom;
	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> ThirdPersonCamera;
	UPROPERTY(VisibleAnywhere, Category="Camera") TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(ReplicatedUsing=OnRep_FirstPerson) bool bFirstPerson = false;

private:
	void InitAbilityActorInfo();
	void ClearStaleTags();

	bool bOnIce = false;

	// Body animation (UWRAnimInstance): locomotion blended by speed and direction, block / swings / casts / shots on
	// the upper body, dodge / falls / swimming / gathering over the whole body. One-shots keep the gameplay's timing:
	// BodyOneShotUntil is when the current one ends (nothing new starts before then, as before).
	struct FBodyAnimSet { TArray<TObjectPtr<class UAnimSequenceBase>> Clips; int32 Next = 0; };
	TMap<FName, FBodyAnimSet> BodyAnims;
	UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequenceBase>> BodyAnimRefs;  // keeps the clips loaded
	TSet<FName> FullBodyKeys;  // one-shots over the whole body (player_art.json "fullBody"); the rest are upper body
	FName BodyAnim;            // the one-shot playing (None once it's done)
	double BodyOneShotUntil = 0.0;
	class UWRHeroAnimInstance* BodyAnimInstance() const;
	/** Locomotion sets, turn clips and the upper body for the animation instance, from the loaded clips. */
	void SetupBodyAnim(const TSharedPtr<class FJsonObject>& Art, const TSharedPtr<class FJsonObject>& Variant);
	class UAnimSequenceBase* BodyClip(FName Key) const;
	FName LoadedBodyStyle;
	bool bBodyLoaded = false;
	/** Load (or swap to) the body for BodyStyle; does nothing if that body is already on. */
	void LoadBodyArt();
	UFUNCTION() void OnRep_BodyStyle();
	FName QueuedOneShot;          // played when the current one-shot ends (a bow's nock, a crossbow's reload)
	float QueuedOneShotSeconds = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_RangedLook) FName RangedItem;
	UPROPERTY(ReplicatedUsing=OnRep_RangedLook) bool bRangedMode = false;
	UPROPERTY(Replicated) bool bAiming = false;
	UPROPERTY(Replicated) FName SelectedAmmo;
	double NextShotAt = 0.0;       // server: the weapon is ready again
	double LocalNextShotAt = 0.0;  // owner: don't flood the server
	float AimBlend = 0.f;          // owner camera, 0 = normal, 1 = aiming
	float BodyYaw = 0.f;           // the body art's meshYaw
	float RangedAimYaw = 0.f;      // extra upper-body turn while aiming, so a weapon held across the body points ahead
	float BodyAimTurn = 0.f;       // that turn, eased in and out (a spine twist, so walking legs keep their line)
	float BodyBaseZ = 0.f;         // the body art's height on the capsule (feet on its bottom)
	float SwimLift = 0.f;          // how much higher the body rides while swimming (WRWater), eased
	float SwumCm = 0.f;            // server: distance swum since the last swimming XP
	FVector LastSwimAt = FVector::ZeroVector;
	float SwimCheck = 0.f;
	FName WeaponSocket;            // player_art.json sockets: where the sword, shield and held gear go
	FName ShieldSocket;
	float AimPitchBlend = 0.f;     // the spine's share of the aim pitch (first person, aiming), eased
	// First person: the eye on the head bone (hero.md 2.7).
	float EyeZ = 0.f, EyeZSpeed = 0.f;
	bool bEyeInit = false;
	FVector DodgeEye = FVector::ZeroVector;  // the eye's place on the capsule when a roll began (the view only drops)
	double DodgeStartedAt = -1.0;
	void TickFirstPersonCamera(float DeltaSeconds);
	UPROPERTY(Replicated) FWRReplicatedAccel ReplicatedAccel;
	void UpdateReplicatedAccel();
	FTimerHandle ThrowTimer;
	UFUNCTION() void OnRep_RangedLook();
	void UpdateWeaponLook();
	void AttachHeld(UStaticMeshComponent* Comp, class UStaticMesh* Model, const struct FWRItemDef& Def);
	void ApplyAimRotation();
	void TickAimCamera(float DeltaSeconds);
	FName RangedAnimGroup() const;  // bow / rifle (crossbow, blowgun) / throw
	void LaunchProjectile(const struct FWRItemDef& Launcher, FName AmmoId, const FVector& Dir, float LauncherMult = 1.f);
	UFUNCTION(Server, Reliable) void ServerSetRangedMode(bool bOn);
	UFUNCTION(Server, Reliable) void ServerSetAiming(bool bOn);
	UFUNCTION(Server, Reliable) void ServerFireRanged(FVector_NetQuantize AimPoint, FVector_NetQuantizeNormal ViewDir);
	UFUNCTION(Server, Reliable) void ServerCycleAmmo(int32 Dir);
	/** Fishing: the attack pressed / released with a rod in hand (cast, reel in, strike, reel), and Block (let it go). */
	UFUNCTION(Server, Reliable) void ServerFishInput(bool bDown);
	UFUNCTION(Server, Reliable) void ServerFishCancel();
	double FishDownAt = -1.0;        // server: when the cast started winding up (-1: it isn't)
	double FishChargeLocal = -1.0;   // owner: the same, for the HUD
	UFUNCTION(NetMulticast, Unreliable) void MulticastRangedShot(FName FireKey, FName ReloadKey, float Reload);
	/** The weapon hand's item (WRHeldTool.cpp): a tool there is built from engine shapes in place of the sword. */
	UPROPERTY(ReplicatedUsing=OnRep_RangedLook) FName MeleeItem;
	/** The held tool's modelled mesh (WRToolMesh), built on machines that draw. */
	UPROPERTY(Transient) TObjectPtr<class UProceduralMeshComponent> ToolMesh;
	/** The weapon socket's frame for a held blade or tool, from the body's reference pose: X runs out of the fist on the
	 *  index finger's side, Y toward the fingers (WRHeldTool.cpp, the sword). */
	FTransform FistFrame() const;
	FName ToolMeshFor;             // the tool ToolMesh was built for
	void UpdateToolLook(bool bShow);
public:
	/** Where the held fishing rod's tip is in the world (the line leaves from there); the hand when the rod isn't drawn. */
	FVector RodTipLocation() const;
private:
	void TickBodyAnim();
	/** Play a one-shot clip from a set (its next variant), on the upper body or, for "fullBody" keys, the whole body.
	 *  Duration > 0 stretches it to fit. Returns its play time. bLoop is unused: loops follow the body's state. */
	float PlayBodyAnim(FName Key, bool bLoop, float PlayRate = 1.f, float Duration = 0.f);
	float DefaultFriction = 8.f;
	float DefaultBraking = 2048.f;
	float DefaultAcceleration = 2048.f;

	UFUNCTION() void OnRep_FirstPerson();
	UFUNCTION(Server, Reliable) void ServerSetFirstPerson(bool bNewFirstPerson);
	void ApplyViewMode();

	void BuildInput();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	/** The pad's right stick (turn rates, curve and sensitivity in WRInput::PadLook); flicks switch the lock-on target. */
	void PadLook(const FInputActionValue& Value);
	/** Button presses from the input mapping, through the held-input gate (WRInput::AllowPress). */
	void InputPressed(EWRAbilityInput Input);
	void InputReleased(EWRAbilityInput Input);
	void InputJump();
	void InputSprint();
	void InputView();
	TSet<EWRAbilityInput> HeldInputs;
	bool bPadFlickReady = true;
	void StartSprint();
	void StopSprint();
	void ToggleView();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PadLookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToggleViewAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SwapWeaponAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AmmoNextAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AmmoPrevAction;
	void AmmoNext() { WheelStep(1); }
	void AmmoPrev() { WheelStep(-1); }
	/** The mouse wheel: ammo while aiming, the hotbar otherwise. */
	void WheelStep(int32 Dir);
	/** (Re)map every key from FWRKeyBindings. */
	void MapKeys();
	FDelegateHandle KeysChangedHandle;
	UPROPERTY(Transient) TMap<EWRAbilityInput, TObjectPtr<UInputAction>> AbilityActions;
};
