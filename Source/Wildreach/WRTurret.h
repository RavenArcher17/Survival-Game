#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRTurret.generated.h"

class AWRBuildPiece;
class AWRCharacterBase;
struct FWRPieceDef;
struct FWRMachineDef;

/**
 * Powered turrets (cooks/build-building.md 2.6; FULL-GAME 6.4 RAVEN-PLANNED "powered turrets"; survival-crafting 2.18).
 * A turret is a machine (machines.json role "turret"): it draws its power while switched on with something to shoot,
 * and short of power it spans slower. It never moves off its plinth, never chases, and hurts nothing that only stands
 * near it: it picks a hostile wild creature (or, on a PvP server, a player not of its builder's clan inside the claim
 * it stands in), turns its head onto it, shows a red aim line for the tell, then looses a straight shot that never
 * homes. All damage through UWRCombatLibrary::ApplyHit.
 */
struct WILDREACH_API FWRTurretGeom
{
	/** In the piece's space: the turntable's centre height, the bow's trunnion, the muzzle's reach along the stock, the port. */
	static constexpr float SlewZ = 66.f;
	static constexpr float TrunnionZ = 140.f;
	static constexpr float MuzzleFwd = 62.f;
	static constexpr float MuzzleUp = 24.f;
	static FVector PortLocal() { return FVector(-96.f, 0.f, 82.f); }
	/** The muzzle in world space for a head turned Yaw / Pitch (degrees, the piece's own frame). */
	static FVector Muzzle(const AWRBuildPiece* Piece, float Yaw, float Pitch);
};

/** One row of what a turret can shoot (Data/machines.json "turrets.ammo"). */
struct FWRTurretAmmo
{
	FName Item;
	FName Turret;      // bolt_turret / element_turret
	FName Element;     // bolt, fire, frost, or a path's status
	FName Shape;       // bolt (a straight shot) or cone (a spray in front of it)
	float Damage = 45.f;
	float Speed = 4000.f;
	float Burn = 0.f, Frost = 0.f;
	FName Status;
	float Buildup = 0.f;
	float Range = 3000.f;
	float Cone = 600.f, ConeAngle = 30.f;
	int32 Shots = 1;   // shots one item gives (an essence lasts 50)
	FLinearColor Color = FLinearColor(1.f, 0.4f, 0.1f);
};

UCLASS()
class WILDREACH_API UWRTurretSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** What this turret takes (and its row): the machine screen's "put in" and the server's Takes. */
	static const FWRTurretAmmo* AmmoFor(FName Turret, FName Item);
	static bool IsTurret(const FWRPieceDef& Def);
	/** May this turret's shot hurt this one (hostile wild creatures; PvP: other clans' players in its claim)? */
	static bool MayHit(const AWRBuildPiece* Turret, const AWRCharacterBase* Target);
	/** How long the red aim line shows before a shot (machines.json turrets.tellSeconds). */
	static float TellSeconds();
	/** Something to shoot (an item in its box, or shots left of a burning essence). */
	static bool IsArmed(const AWRBuildPiece* Piece);
	/** The screen's line: what it's loaded with and how many shots are left. */
	static FString AmmoText(const AWRBuildPiece* Piece);

private:
	struct FBrain
	{
		TWeakObjectPtr<AWRCharacterBase> Target;
		float LookClock = 0.f;
		float Reload = 0.f;     // seconds until it's spanned again
		float TellLeft = -1.f;  // < 0: not telling
		FVector AimDir = FVector::ForwardVector;
	};
	TMap<TWeakObjectPtr<AWRBuildPiece>, FBrain> Brains;
	float ScanClock = 0.f;
	TArray<TWeakObjectPtr<AWRBuildPiece>> Turrets;

	AWRCharacterBase* PickTarget(AWRBuildPiece* Piece, float Range) const;
	bool CanSee(AWRBuildPiece* Piece, const FVector& From, const AWRCharacterBase* Target) const;
	void Fire(AWRBuildPiece* Piece, FBrain& B, const FWRTurretAmmo& Ammo);
	/** The row it fires now, taking an item into the chamber when its last one is spent; null: nothing to shoot. */
	const FWRTurretAmmo* Loaded(AWRBuildPiece* Piece, bool bConsume);
};
