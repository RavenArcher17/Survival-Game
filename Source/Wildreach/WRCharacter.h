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
/** How a survivor looks on every machine (cook hero-look-2 3.9): the face and build picked in the menu (?Look=), and the
 *  look of the armour worn (player_art.json "armorLooks"), its element (the trim palette) and tier-6 inlay. Server set. */
USTRUCT()
struct FWRHeroLook
{
	GENERATED_BODY()
	UPROPERTY() uint8 Face = 0;           // index into player_art.json hero.<sex>.faces
	UPROPERTY() uint8 Build = 1;          // 0 lean, 1 athletic, 2 strong (hero.<sex>.builds)
	UPROPERTY() FName Look;               // worn armour look id; None = the tier-0 Survivor Wraps
	UPROPERTY() FName Element;            // the worn armour's element (TrimColor palette); None = the look's own trim
	UPROPERTY() bool bAscendant = false;  // tier 6: starsteel inlay + halo-crest
	UPROPERTY() bool bHideHelmet = false; // the hide-helmet option (the Head slot shows the face; tier 0 always does)
	bool SameBody(const FWRHeroLook& O) const { return Face == O.Face && Build == O.Build; }
	bool operator==(const FWRHeroLook& O) const
	{
		return SameBody(O) && Look == O.Look && Element == O.Element && bAscendant == O.bAscendant && bHideHelmet == O.bHideHelmet;
	}
};

/** The path marks every machine draws (server computes from the PlayerState's path progress, which only the owner gets):
 *  main and sub path and their strengths 0..255 (hero.md 2.6: smoothstep(0.08, 1, points / max)). */
USTRUCT()
struct FWRPathMarks
{
	GENERATED_BODY()
	UPROPERTY() FName Main;
	UPROPERTY() FName Sub;
	UPROPERTY() uint8 MainS = 0;
	UPROPERTY() uint8 SubS = 0;
	bool operator==(const FWRPathMarks& O) const { return Main == O.Main && Sub == O.Sub && MainS == O.MainS && SubS == O.SubS; }
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
	 *  or the death screen took the buttons (WRInput::Transition). The input buffer empties too. */
	void DropHeldInputs();

	/** This machine: what a Light would crit right now, if the prompt for its kind hasn't done its job yet (the HUD's one
	 *  prompt: shown once per species, COMBAT-AND-LESSONS L6). */
	const AWRCharacterBase* CritPromptTarget() const;
	/** This machine: the crit prompt for this target's species has been used: saved, never shown again. */
	void NoteCritPromptSeen(const AWRCharacterBase* Target);

	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;

	/** Everyone: play a one-shot body clip from Data/player_art.json (e.g. "pickup" when looting). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastBodyOneShot(FName Key);
	/** Fishing (WRFishing): a rod is in the weapon hand. */
	bool IsHoldingRod() const;
	/** This machine: how long the cast has been wound up (0 when it isn't), for the HUD's charge ring. */
	double GetFishChargeSeconds() const;
	/** The body picked in the menu (None = the default body); the sound pass picks the voice by it. */
	FName GetBodyStyle() const { return BodyStyle; }
	/** Server: the armour now worn (WRInventory ApplyEquipment) picks the outfit look, its trim and its inlay. */
	void SetWornArmor(FName ItemId, FName Element);
	/** Server: the face and build from the menu (?Look=), clamped to what player_art.json has. */
	void SetHeroAppearance(uint8 Face, uint8 Build);
	/** Server: the hide-helmet option. */
	UFUNCTION(Server, Reliable) void ServerSetHideHelmet(bool bHide);
	/** A path ability was used (UWRGameplayAbility commit): the path marks flare (hero.md 2.6 combat pulse). */
	void OnPathAbilityUsed(FName PathId);
	const FWRHeroLook& GetHeroLook() const { return HeroLook; }

	// Held tools (WRHeldTool.cpp).
	/** Server (from the inventory): what's in the weapon hand, None for bare hands. A tool there shows in the hand. */
	void SetMeleeItem(FName ItemId);
	/** A weapon the hero holds as a modelled class weapon (dagger, spear, greatsword, great hammer, scythe). */
	static bool IsClassWeapon(FName ItemId);
	/** Two-handing (combat.md 2.4.0): the owner asks, the server sets bTwoHanded (one-handed weapons only). */
	UFUNCTION(Server, Reliable) void ServerSetTwoHanded(bool bOn);
	virtual void OnRep_TwoHanded() override;
	/** Players: a press while frozen breaks free sooner. */
	UFUNCTION(Server, Unreliable) void ServerMashFrozen();
	/** Giant framing and hiding the hero from a camera pressed against it (WRCharacterCombat.cpp). */
	void TickCombatCamera(float DeltaSeconds);
	float GiantBlend = 0.f;
	bool bHiddenByCamera = false;
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

	// ---- the hero's look (WRHeroLook.cpp, cook hero-look-2) ----
	UPROPERTY(ReplicatedUsing=OnRep_HeroLook) FWRHeroLook HeroLook;
	FWRHeroLook LoadedLook;               // what the drawn body was built for
	FString LoadedFaceKey;                // "<sex><face>-<build>": the body/face/grooms on now
	FName LoadedOutfit;                   // the look whose pieces are on now ("wraps" for tier 0)
	UFUNCTION() void OnRep_HeroLook();
	/** Put on the worn look's 4 slot pieces, their trims and the body-hide masks (or the Wraps). */
	void ApplyOutfit(const TSharedPtr<class FJsonObject>& Art);
	/** Skin, eye and hair materials made dynamic once per body so path marks and hide masks can be set. */
	void BuildLookMaterials();
	/** Path marks on the skin, eyes and hair from the main and sub paths (hero.md 2.6), refreshed when they change. */
	void ApplyPathMarks(bool bForce);
	void TickPathMarks(float DeltaSeconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastPathPulse(FName PathId);
	UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> SkinMIDs;   // [0] = body, then the head's
	UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> EyeMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> HairMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> OutfitMIDs;
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<UStaticMeshComponent> Canines;  // Blood path (hero.md 2.6)
	UPROPERTY(VisibleAnywhere, Category="Mesh") TObjectPtr<USkeletalMeshComponent> HaloCrest; // Ascendant
	TMap<FName, float> FaceHairParams;    // the face's hair colour (hairMelanin, hairRedness) on the shared plugin grooms
	TMap<FName, float> EyeDefaults;       // the eye/hair material's own values, so marks blend from them
	TMap<FName, FLinearColor> EyeDefaultColors;
	UPROPERTY(Replicated) FWRPathMarks PathMarks;
	void RefreshLODSync();
	uint32 PathMarkSig = 0;
	float PathMarkCheck = 0.f;
	FName PulsePath;
	double PulseUntil = -1.0;
	float PulseSeconds = 1.5f;
	bool bHairHiddenByHelm = false;
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
	FName TorchSocket;             // the off hand's torch grip (sockets.torch) and the left hip (sockets.hipLeft): the lantern
	FName HipSocket;
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
	// --- Survival on the body (WRHeldLight.cpp): the lit torch in the off hand or the lantern on the hip, its light,
	// and lying on a bed while asleep. Clients draw them from the player state's UWRSurvivalComponent.
	UPROPERTY(Transient) TObjectPtr<class UProceduralMeshComponent> LightMesh;
	UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> HeldLight;
	FName LightMeshFor;
	bool bSleepPose = false;
	double WakeAskedAt = -10.0;
	double LightLookCheckedAt = -10.0;
	void UpdateHeldLightLook();
	void TickSurvivalLook(float DeltaSeconds);
	/** Owner: a move or jump while asleep gets you up (the server checks it too). */
	UFUNCTION(Server, Reliable) void ServerWakeUp();
public:
	/** Re-dress the hands (the shield comes off for a torch; the torch / lantern shows): WRSurvival's light changes. */
	void RefreshHeldLook() { UpdateWeaponLook(); }
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
	// The input buffer (COMBAT-AND-LESSONS 1.16): a press that can't start yet waits combat.bufferSeconds (a roll pressed
	// mid-swing up to combat.dodgeBufferMax) and fires the moment it can; the last press wins.
	EWRAbilityInput BufferedInput = EWRAbilityInput::None;
	double BufferedAt = 0.0;
	bool CanStartNow(EWRAbilityInput Input) const;
	void FireAbilityInput(EWRAbilityInput Input);
	void TickInputBuffer();
	/** Light on a parried, guard-broken or stance-broken target in front (or an unaware one from behind): the critical. */
	bool TryCritical();
	// The guard's faint gold edge on the shield (COMBAT-AND-LESSONS 1.2, colour law: gold is block / parry).
	void TickGuardEdge();
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> GuardEdgeMaterial;
	bool bGuardEdgeOn = false;
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
