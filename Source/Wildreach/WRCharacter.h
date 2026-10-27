#pragma once

#include "CoreMinimal.h"
#include "WRCharacterBase.h"
#include "WRCombatTypes.h"
#include "WRCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
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

	/** Everyone: play a one-shot body clip from Data/player_art.json (e.g. "pickup" when looting). */
	UFUNCTION(NetMulticast, Unreliable) void MulticastBodyOneShot(FName Key);
	/** The body picked in the menu (None = the default body); the sound pass picks the voice by it. */
	FName GetBodyStyle() const { return BodyStyle; }

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
	class UWRAnimInstance* BodyAnimInstance() const;
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
	UFUNCTION(NetMulticast, Unreliable) void MulticastRangedShot(FName FireKey, FName ReloadKey, float Reload);
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
	void StartSprint();
	void StopSprint();
	void ToggleView();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
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
