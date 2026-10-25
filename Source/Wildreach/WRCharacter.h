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

	// Body animation: one AnimSequence at a time, looping by movement state; one-shots (swings, hits, dodge) hold
	// the body until they finish.
	struct FBodyAnimSet { TArray<TObjectPtr<class UAnimSequenceBase>> Clips; int32 Next = 0; };
	TMap<FName, FBodyAnimSet> BodyAnims;
	UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequenceBase>> BodyAnimRefs;  // keeps the clips loaded
	FName BodyAnim;
	double BodyOneShotUntil = 0.0;
	float WalkClipSpeed = 150.f;
	float RunClipSpeed = 420.f;
	FName LoadedBodyStyle;
	bool bBodyLoaded = false;
	/** Load (or swap to) the body for BodyStyle; does nothing if that body is already on. */
	void LoadBodyArt();
	UFUNCTION() void OnRep_BodyStyle();
	void TickBodyAnim();
	/** Play a clip from a set; bLoop for locomotion. Duration > 0 stretches a one-shot to fit it. Returns the clip length. */
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
	UPROPERTY(Transient) TMap<EWRAbilityInput, TObjectPtr<UInputAction>> AbilityActions;
};
