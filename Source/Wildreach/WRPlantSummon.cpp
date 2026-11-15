#include "WRPlantSummon.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRNaturePath.h"
#include "WRPathComponent.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRWorldDressing.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

// The Nature path's living summons (cooks/biome-forest.md 2.17, cooks/build-biome-forest.md 2.11). Every look is modelled
// in code (FWRToolMesh) or drawn from the Tree kit (WRTreeKit), at real size; every number is tuning (spells.<kind>.*,
// nature.plants.*, passives.pollen*, combos.json); every blow goes through UWRCombatLibrary::ApplyHit.

namespace WRPlantSummonImpl
{
	static const FName PathId(TEXT("nature"));

	// Colours (linear; the vertex colour M_HeldTool reads): young bark, moss, bramble, seed pod, thorn.
	static const FLinearColor Bark(0.16f, 0.13f, 0.10f);
	static const FLinearColor BarkLight(0.30f, 0.28f, 0.24f);
	static const FLinearColor Moss(0.11f, 0.20f, 0.04f);
	static const FLinearColor Bramble(0.10f, 0.16f, 0.05f);
	static const FLinearColor BrambleRed(0.20f, 0.07f, 0.05f);
	static const FLinearColor Pod(0.24f, 0.30f, 0.07f);
	static const FLinearColor PodLip(0.45f, 0.25f, 0.10f);
	static const FLinearColor ThornCol(0.08f, 0.05f, 0.03f);
	static const FLinearColor Earth(0.09f, 0.07f, 0.05f);
	static const FLinearColor Burl(0.21f, 0.15f, 0.10f);
	static const FLinearColor Leaf(0.13f, 0.27f, 0.05f);

	static float Ease(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Square(1.f - T);
	}

	/** A small overshoot at the end of a grow (a sprout that springs up and settles). */
	static float Spring(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		const float C = 1.70158f;
		return 1.f + (C + 1.f) * FMath::Pow(T - 1.f, 3.f) + C * FMath::Square(T - 1.f);
	}

	/** A cone of a thorn: from Base along Dir, Length long, BaseR thick, curving a little toward Curve. */
	static void Thorn(FWRToolMesh& M, const FVector& Base, const FVector& Dir, float Length, float BaseR, float Hook)
	{
		const TArray<FVector> Profile = { FVector(0.f, BaseR, BaseR), FVector(0.45f, BaseR * 0.55f, BaseR * 0.55f), FVector(1.f, 0.2f, 0.2f) };
		M.Lathe(Base, Dir.GetSafeNormal(), Length, Profile, 5, ThornCol, 0.55f, -Hook);
	}

	/** An arc of bramble cane from A to B bowing up by Rise, thorns along it every Every cm. */
	static void Cane(FWRToolMesh& M, const FVector& A, const FVector& B, float Rise, float R, float Every, const FLinearColor& Col)
	{
		TArray<FVector> Pts;
		const int32 N = 10;
		for (int32 i = 0; i <= N; ++i)
		{
			const float T = float(i) / N;
			Pts.Add(FMath::Lerp(A, B, T) + FVector(0.f, 0.f, Rise * FMath::Sin(PI * T)));
		}
		M.Tube(Pts, R, 6, Col, 0.6f);
		float Along = Every * 0.5f;
		const float Len = FVector::Dist(A, B) * 1.2f;
		int32 k = 0;
		while (Along < Len && k < 24)
		{
			const float T = FMath::Clamp(Along / Len, 0.f, 1.f);
			const int32 Seg = FMath::Clamp(FMath::FloorToInt(T * N), 0, N - 1);
			const FVector P = FMath::Lerp(Pts[Seg], Pts[Seg + 1], T * N - Seg);
			const FVector Tan = (Pts[Seg + 1] - Pts[Seg]).GetSafeNormal();
			FVector Side = FVector::CrossProduct(Tan, FVector::UpVector).GetSafeNormal();
			if (Side.IsNearlyZero()) Side = FVector::RightVector;
			const float Turn = (k % 3) * 2.1f;
			const FVector Out = (Side * FMath::Cos(Turn) + FVector::UpVector * FMath::Sin(Turn) - Tan * 0.35f).GetSafeNormal();
			Thorn(M, P + Out * R * 0.7f, Out, R * 2.4f, R * 0.45f, 1.5f);
			Along += Every;
			++k;
		}
	}

	/** The Sprout turret's bramble mound (its feet in the ground: a low earth heap with canes arching out of it). */
	static const FWRToolMesh& TurretMound()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			// The heap the sprout broke out of: churned earth with moss on top, sunk 6 cm into the ground.
			M.Lathe(FVector(0.f, 0.f, -6.f), FVector::UpVector, 26.f,
				{ FVector(0.f, 62.f, 58.f), FVector(0.35f, 52.f, 48.f), FVector(0.75f, 30.f, 28.f), FVector(1.f, 4.f, 4.f) }, 12, Earth, 0.95f, 3.f, 1.f);
			M.Lathe(FVector(0.f, 0.f, 12.f), FVector::UpVector, 12.f,
				{ FVector(0.f, 40.f, 36.f), FVector(0.6f, 26.f, 22.f), FVector(1.f, 3.f, 3.f) }, 10, Moss, 0.9f, 2.f, 2.f);
			// Seven bramble canes arch out of the heap and back into the ground round it.
			for (int32 i = 0; i < 7; ++i)
			{
				const float Yaw = i * (2.f * PI / 7.f) + 0.3f * (i % 2);
				const FVector In(FMath::Cos(Yaw) * 12.f, FMath::Sin(Yaw) * 12.f, 14.f);
				const float Reach = 58.f + 14.f * ((i * 37) % 5) / 4.f;
				const FVector Out(FMath::Cos(Yaw + 0.5f) * Reach, FMath::Sin(Yaw + 0.5f) * Reach, -4.f);
				Cane(M, In, Out, 26.f + 6.f * (i % 3), 2.4f, 11.f, (i % 3 == 0) ? BrambleRed : Bramble);
			}
			// The stalk the head rides on (a thick green stem, a little bowed), its root collar of sepals.
			M.Lathe(FVector(0.f, 0.f, 18.f), FVector::UpVector, 74.f,
				{ FVector(0.f, 8.f, 8.f), FVector(0.4f, 6.f, 6.f), FVector(0.85f, 5.f, 5.f), FVector(1.f, 6.5f, 6.5f) }, 8, Bramble, 0.6f, 4.f, 3.f);
			for (int32 i = 0; i < 5; ++i)
			{
				const float Yaw = i * (2.f * PI / 5.f);
				const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.6f);
				Thorn(M, FVector(0.f, 0.f, 30.f + 9.f * i) + FVector(Dir.X, Dir.Y, 0.f) * 6.f, Dir, 9.f, 1.8f, 2.f);
			}
			return M;
		}();
		return Mesh;
	}

	/** The Sprout turret's head: a seed pod 46 cm long pointing along +X, its mouth open at the front (MouthLocal),
	 *  five sepals clasping its back, thorns down its spine. Pivot at the stalk's top. */
	static const FVector TurretMouthLocal(34.f, 0.f, 6.f);
	static const FWRToolMesh& TurretHead()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			M.Lathe(FVector(-14.f, 0.f, 6.f), FVector::ForwardVector, 46.f,
				{ FVector(0.f, 3.f, 3.f), FVector(0.2f, 13.f, 12.f), FVector(0.55f, 15.f, 14.f), FVector(0.85f, 10.f, 9.f), FVector(1.f, 7.f, 6.f) }, 12, Pod, 0.5f, 0.f, 4.f);
			// The mouth: a dark lipped ring the thorns leave by.
			TArray<FVector> Lip;
			for (int32 i = 0; i < 12; ++i)
			{
				const float A = i * 2.f * PI / 12.f;
				Lip.Add(FVector(31.f, FMath::Cos(A) * 6.5f, 6.f + FMath::Sin(A) * 5.5f));
			}
			M.Tube(Lip, 1.8f, 5, PodLip, 0.6f, true);
			// Sepals: five leafy claws from the stalk over the pod's back half.
			for (int32 i = 0; i < 5; ++i)
			{
				const float A = i * 2.f * PI / 5.f;
				const FVector Side(0.f, FMath::Cos(A), FMath::Sin(A));
				TArray<FVector> Pts = { FVector(-12.f, 0.f, 4.f) + Side * 3.f, FVector(-6.f, 0.f, 6.f) + Side * 13.f, FVector(4.f, 0.f, 6.f) + Side * 16.f, FVector(12.f, 0.f, 6.f) + Side * 13.f };
				M.Tube(Pts, 2.2f, 5, Leaf, 0.7f);
			}
			// Thorns down its spine and flanks.
			for (int32 i = 0; i < 6; ++i)
			{
				const float X = -6.f + i * 6.5f;
				Thorn(M, FVector(X, 0.f, 18.f), FVector(-0.3f, 0.f, 1.f), 7.f, 1.6f, 1.f);
				Thorn(M, FVector(X, 12.f, 8.f), FVector(-0.3f, 1.f, 0.4f), 5.f, 1.3f, 1.f);
				Thorn(M, FVector(X, -12.f, 8.f), FVector(-0.3f, -1.f, 0.4f), 5.f, 1.3f, 1.f);
			}
			return M;
		}();
		return Mesh;
	}

	/** The World tree's root flare: six roots running out from its foot into the ground (the Tree kit draws the tree). */
	static const FWRToolMesh& WorldTreeRoots()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			for (int32 i = 0; i < 6; ++i)
			{
				const float Yaw = i * (2.f * PI / 6.f) + 0.2f * (i % 2);
				const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
				const float Len = 170.f + 40.f * (i % 3);
				TArray<FVector> Pts;
				for (int32 s = 0; s <= 6; ++s)
				{
					const float T = s / 6.f;
					Pts.Add(Dir * (20.f + Len * T) + FVector(0.f, 0.f, 30.f * (1.f - T) * (1.f - T) - 10.f * T) + FVector(-Dir.Y, Dir.X, 0.f) * 18.f * FMath::Sin(T * 5.f + i));
				}
				M.Tube(Pts, 14.f - 3.f * (i % 2), 7, BarkLight, 0.8f);
			}
			// Moss over the root crown.
			M.Lathe(FVector(0.f, 0.f, -8.f), FVector::UpVector, 20.f, { FVector(0.f, 90.f, 86.f), FVector(0.6f, 60.f, 58.f), FVector(1.f, 8.f, 8.f) }, 12, Moss, 0.95f, 3.f, 5.f);
			return M;
		}();
		return Mesh;
	}

	/** The Ancient guardian's torso: a walking tree's bole from the hips (0) up to the shoulders (210), knotted, a mossy
	 *  hump, the head-burl with two hollow eye-knots at the front (+X). */
	static const FWRToolMesh& GuardianTorso()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			M.Lathe(FVector(0.f, 0.f, 0.f), FVector::UpVector, 230.f,
				{ FVector(0.f, 46.f, 40.f), FVector(0.2f, 40.f, 34.f), FVector(0.5f, 36.f, 30.f), FVector(0.8f, 44.f, 36.f), FVector(0.92f, 34.f, 30.f), FVector(1.f, 18.f, 16.f) }, 12, Bark, 0.85f, 6.f, 7.f);
			// The head-burl, sunk between the shoulders and leaning forward.
			M.Lathe(FVector(10.f, 0.f, 205.f), FVector(0.25f, 0.f, 1.f).GetSafeNormal(), 70.f,
				{ FVector(0.f, 22.f, 22.f), FVector(0.35f, 30.f, 28.f), FVector(0.75f, 24.f, 22.f), FVector(1.f, 6.f, 6.f) }, 10, Burl, 0.8f, 2.f, 8.f);
			// Eye-knots: dark hollows ringed by bark.
			for (const float Side : { -1.f, 1.f })
			{
				TArray<FVector> Ring;
				for (int32 i = 0; i < 10; ++i)
				{
					const float A = i * 2.f * PI / 10.f;
					Ring.Add(FVector(36.f, Side * 11.f + FMath::Cos(A) * 5.f, 240.f + FMath::Sin(A) * 4.f));
				}
				M.Tube(Ring, 2.6f, 5, BarkLight, 0.8f, true);
				M.Lathe(FVector(30.f, Side * 11.f, 240.f), FVector::ForwardVector, 6.f, { FVector(0.f, 4.f, 3.5f), FVector(1.f, 2.f, 1.5f) }, 6, FLinearColor(0.02f, 0.015f, 0.01f), 1.f);
			}
			// Bark ridges spiralling up the bole and a moss hump over the shoulders.
			for (int32 r = 0; r < 5; ++r)
			{
				TArray<FVector> Pts;
				for (int32 s = 0; s <= 8; ++s)
				{
					const float T = s / 8.f;
					const float A = r * 2.f * PI / 5.f + T * 0.9f;
					const float Rad = FMath::Lerp(44.f, 38.f, T);
					Pts.Add(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad * 0.85f, 10.f + T * 180.f));
				}
				M.Tube(Pts, 5.f, 5, BarkLight, 0.85f);
			}
			M.Lathe(FVector(-8.f, 0.f, 168.f), FVector::UpVector, 40.f, { FVector(0.f, 40.f, 44.f), FVector(0.6f, 32.f, 36.f), FVector(1.f, 8.f, 8.f) }, 10, Moss, 0.95f, 4.f, 9.f);
			return M;
		}();
		return Mesh;
	}

	/** A guardian arm hanging from the shoulder pivot (0) down -Z: a branch 170 cm long ending in a knotted burl fist,
	 *  twigs off the forearm. */
	static const FWRToolMesh& GuardianArm()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			const TArray<FVector> Pts = { FVector(0.f, 0.f, 0.f), FVector(6.f, 4.f, -50.f), FVector(16.f, 2.f, -100.f), FVector(22.f, -2.f, -140.f), FVector(24.f, 0.f, -160.f) };
			M.Tube(Pts, 15.f, 8, Bark, 0.85f);
			M.Lathe(FVector(24.f, 0.f, -150.f), FVector(0.1f, 0.f, -1.f).GetSafeNormal(), 52.f,
				{ FVector(0.f, 20.f, 20.f), FVector(0.4f, 30.f, 27.f), FVector(0.8f, 24.f, 22.f), FVector(1.f, 6.f, 6.f) }, 10, Burl, 0.8f, 3.f, 10.f);
			// Twigs and their leaves off the forearm.
			for (int32 i = 0; i < 3; ++i)
			{
				const float Z = -60.f - i * 28.f;
				const FVector Out(FMath::Cos(i * 2.2f), FMath::Sin(i * 2.2f), 0.4f);
				M.Tube({ FVector(10.f + i * 4.f, 0.f, Z), FVector(10.f + i * 4.f, 0.f, Z) + Out * 30.f, FVector(10.f + i * 4.f, 0.f, Z) + Out * 48.f + FVector(0.f, 0.f, 10.f) }, 2.6f, 5, BarkLight, 0.8f);
				M.Lathe(FVector(10.f + i * 4.f, 0.f, Z) + Out * 46.f, Out, 18.f, { FVector(0.f, 2.f, 1.f), FVector(0.4f, 9.f, 2.f), FVector(1.f, 1.f, 0.5f) }, 6, Leaf, 0.7f);
			}
			return M;
		}();
		return Mesh;
	}

	/** A guardian leg from the hip pivot (0) down to the ground at -90: a root column with root toes splayed out. */
	static const FWRToolMesh& GuardianLeg()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			M.Lathe(FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, -1.f), 84.f,
				{ FVector(0.f, 26.f, 24.f), FVector(0.5f, 20.f, 19.f), FVector(1.f, 28.f, 26.f) }, 10, Bark, 0.85f, 3.f, 11.f);
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = -0.9f + i * 0.6f;
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
				M.Tube({ FVector(0.f, 0.f, -70.f) + Dir * 14.f, FVector(0.f, 0.f, -84.f) + Dir * 40.f, FVector(0.f, 0.f, -92.f) + Dir * 62.f }, 7.f - i % 2, 6, BarkLight, 0.85f);
			}
			return M;
		}();
		return Mesh;
	}

	/** A thorn in flight: a curved dark thorn 22 cm long along +X (tip at 0, where the collision is), a green sheath at
	 *  its base. */
	static const FWRToolMesh& ThornModel()
	{
		static const FWRToolMesh Mesh = []()
		{
			FWRToolMesh M;
			M.Lathe(FVector(-22.f, 0.f, 0.f), FVector::ForwardVector, 22.f,
				{ FVector(0.f, 2.6f, 2.6f), FVector(0.35f, 2.2f, 2.2f), FVector(0.8f, 0.9f, 0.9f), FVector(1.f, 0.15f, 0.15f) }, 6, ThornCol, 0.5f, -1.2f);
			M.Lathe(FVector(-24.f, 0.f, 0.f), FVector::ForwardVector, 7.f, { FVector(0.f, 1.f, 1.f), FVector(0.5f, 3.2f, 3.2f), FVector(1.f, 2.4f, 2.4f) }, 6, Bramble, 0.7f);
			return M;
		}();
		return Mesh;
	}

	/** Kind data: tuning key, display name, capsule, grow seconds. */
	struct FKind
	{
		FName Id;
		const TCHAR* Key;
		const TCHAR* Name;
		float Radius, HalfHeight, Grow, Health, Seconds, Poise;
	};
	static const FKind* KindOf(const UClass* Class)
	{
		static const FKind Turret{ TEXT("sprout_turret"), TEXT("spells.sproutTurret"), TEXT("Sprout turret"), 40.f, 60.f, 0.6f, 60.f, 20.f, 999.f };
		static const FKind Tree{ TEXT("world_tree"), TEXT("spells.worldTree"), TEXT("World tree"), 55.f, 400.f, 0.8f, 300.f, 15.f, 999.f };
		static const FKind Guardian{ TEXT("ancient_guardian"), TEXT("spells.ancientGuardian"), TEXT("Ancient guardian"), 70.f, 200.f, 1.4f, 400.f, 30.f, 120.f };
		if (!Class) return nullptr;
		if (Class->IsChildOf(AWRSproutTurret::StaticClass())) return &Turret;
		if (Class->IsChildOf(AWRWorldTree::StaticClass())) return &Tree;
		if (Class->IsChildOf(AWRAncientGuardian::StaticClass())) return &Guardian;
		return nullptr;
	}

	/** Ground a plant can root in at At (within a short drop), flat enough, not water: the point on it. */
	static bool FindRootGround(UWorld* World, const AActor* Ignore, const FVector& At, FVector& Out)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRPlantGround), false, Ignore);
		if (!World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 250.f), At - FVector(0.f, 0.f, 600.f), ECC_WorldStatic, Query))
		{
			return false;
		}
		if (Hit.ImpactNormal.Z < WRTune(TEXT("nature.plants.minGroundNormal"), 0.6f))
		{
			return false;   // a cliff face or a wall
		}
		if (Cast<AWRCharacterBase>(Hit.GetActor()))
		{
			return false;
		}
		Out = Hit.ImpactPoint;
		return true;
	}
}

// ================================================================================================ AWRPlantSummon

AWRPlantSummon::AWRPlantSummon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A plant owns its ability system like a creature (health, statuses), but no AI brain of its own: the turret and the
	// tree stand, the guardian walks by a plain AI controller (AddMovementInput, no pathfinding needed for its 15 m).
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UWRAttributeSet>(TEXT("Attributes"));
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;
	SetReplicates(true);

	// The placeholder body is never shown: the plant's look is built in code (BuildLook).
	BodyMesh->SetVisibility(false);
	FaceMarker->SetVisibility(false);
	WeaponMesh->SetVisibility(false);

	LookRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LookRoot"));
	LookRoot->SetupAttachment(RootComponent);

	// Every plant counts as Lush ground for a Nature walker within the sampler's 8 m (WRNaturePath.cpp).
	Tags.Add(WRNature::PlantTag());
	PrimaryActorTick.bCanEverTick = true;
}

void AWRPlantSummon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRPlantSummon, OwnerPawn);
	DOREPLIFETIME(AWRPlantSummon, OwnerName);
	DOREPLIFETIME(AWRPlantSummon, CreatureId);
	DOREPLIFETIME(AWRPlantSummon, BornAt);
	DOREPLIFETIME(AWRPlantSummon, bPollen);
	DOREPLIFETIME(AWRPlantSummon, WitherStart);
}

float AWRPlantSummon::Num(const TCHAR* Field, float Fallback) const
{
	return FWRTuning::Get().Num(SpellKey + TEXT(".") + Field, Fallback);
}

float AWRPlantSummon::ServerNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const AGameStateBase* GS = World->GetGameState();
	return GS ? float(GS->GetServerWorldTimeSeconds()) : float(World->GetTimeSeconds());
}

bool AWRPlantSummon::IsStanding() const
{
	return WitherStart < 0.f && IsAlive();
}

AWRPlantSummon* AWRPlantSummon::Plant(UClass* Class, AWRCharacterBase* Planter, const FVector& Ground, float Yaw)
{
	using namespace WRPlantSummonImpl;
	const FKind* Kind = KindOf(Class);
	UWorld* World = Planter ? Planter->GetWorld() : nullptr;
	if (!Kind || !World || !Planter->HasAuthority() || !Planter->IsAlive())
	{
		return nullptr;
	}
	FVector Root;
	if (!FindRootGround(World, Planter, Ground, Root))
	{
		return nullptr;
	}
	// In the sea (deeper than a wade) nothing takes root.
	if (const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(World); Water && Water->DepthAt(Root) > WRTune(TEXT("nature.plants.maxWaterDepth"), 40.f))
	{
		return nullptr;
	}
	// Room for the body: the capsule standing on the ground mustn't overlap a wall or a building.
	const FVector Centre = Root + FVector(0.f, 0.f, Kind->HalfHeight + 2.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRPlantRoom), false, Planter);
	if (World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(Kind->Radius * 0.8f, Kind->HalfHeight * 0.9f), Query))
	{
		return nullptr;
	}

	// The planter's limit for the kind: one (turrets two with Overgrowth). The oldest withers to make room.
	int32 Limit = 1;
	if (Kind->Id == TEXT("sprout_turret") && UWRPathComponent::HasNode(Planter, PathId, TEXT("overgrowth")))
	{
		Limit = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("passives.overgrowthTurrets"), 2.f)));
	}
	TArray<AWRPlantSummon*> Mine = PlantsOf(Planter, Kind->Id);
	Mine.Sort([](const AWRPlantSummon& A, const AWRPlantSummon& B) { return A.BornAt < B.BornAt; });
	for (int32 i = 0; i <= Mine.Num() - Limit; ++i)
	{
		Mine[i]->Wither();
	}

	const FTransform At(FRotator(0.f, Yaw, 0.f), Centre);
	AWRPlantSummon* P = World->SpawnActorDeferred<AWRPlantSummon>(Class, At, Planter, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!P)
	{
		return nullptr;
	}
	P->OwnerPawn = Planter;
	P->OwnerName = Planter->GetPlayerState() ? Planter->GetPlayerState()->GetPlayerName() : Planter->GetTameOwner();
	P->CreatureId = Kind->Id;
	P->KindId = Kind->Id;
	P->SpellKey = Kind->Key;
	P->KindName = Kind->Name;
	P->GrowSeconds = P->Num(TEXT("growSeconds"), Kind->Grow);
	// Deep roots: +15% health a rank; Fertile: +15% time a rank.
	P->PlantHealth = P->Num(TEXT("health"), Kind->Health) * (1.f + UWRPathComponent::Stat(Planter, TEXT("plantHealth")));
	P->PlantSeconds = P->Num(TEXT("seconds"), Kind->Seconds) * (1.f + UWRPathComponent::Stat(Planter, TEXT("plantDuration")));
	// Snow and ice wither a plant sooner (Evergreen: they don't; cooks/biome-forest.md 2.17 combos).
	if (!Planter->HasTag(WRNature::Combo_Evergreen) && WRNature::ClassifyGround(Planter, Root) == WRNature::EGround::Barren
		&& (WRBiome::At(Planter, Root) == TEXT("tundra") || FWRBiomeHazards::KindAt(Root) == TEXT("ice")))
	{
		P->PlantSeconds *= WRTune(TEXT("nature.plants.frostLifetime"), 0.6f);
	}
	P->bPollen = UWRPathComponent::HasNode(Planter, PathId, TEXT("pollen"));
	P->BornAt = P->ServerNow();
	P->FinishSpawning(At);
	if (!P || !IsValid(P))
	{
		return nullptr;
	}
	UE_LOG(LogWildreach, Log, TEXT("NATURE %s planted %s (%.0f hp, %.0f s)"), *GetNameSafe(Planter), *Kind->Id.ToString(), P->PlantHealth, P->PlantSeconds);
	return P;
}

TArray<AWRPlantSummon*> AWRPlantSummon::PlantsOf(const AWRCharacterBase* Planter, FName Kind)
{
	TArray<AWRPlantSummon*> Out;
	UWorld* World = Planter ? Planter->GetWorld() : nullptr;
	if (!World)
	{
		return Out;
	}
	for (TActorIterator<AWRPlantSummon> It(World); It; ++It)
	{
		if (It->OwnerPawn == Planter && It->IsStanding() && (Kind.IsNone() || It->CreatureId == Kind))
		{
			Out.Add(*It);
		}
	}
	return Out;
}

void AWRPlantSummon::BeginPlay()
{
	using namespace WRPlantSummonImpl;
	if (const FKind* Kind = KindOf(GetClass()))
	{
		GetCapsuleComponent()->SetCapsuleSize(Kind->Radius, Kind->HalfHeight);
		if (KindId.IsNone())
		{
			KindId = Kind->Id;
			SpellKey = Kind->Key;
			KindName = Kind->Name;
		}
	}
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);
	LookRoot->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	if (HasAuthority())
	{
		const FKind* Kind = KindOf(GetClass());
		InitVitals(PlantHealth, 100.f, Kind ? Kind->Poise : 999.f);
		DieAt = BornAt + PlantSeconds;
		// A plant is never a tame: it never counts for a boss's head count or a tame slot (it isn't an AWRCreature).
		// It answers to its planter: when they go, it withers (Tick). Only the guardian walks (its plain AI controller).
		if (AutoPossessAI != EAutoPossessAI::Disabled && !Controller)
		{
			SpawnDefaultController();
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		BuildLook();
		bLookBuilt = true;
		OnRep_Pollen();
	}
}

void AWRPlantSummon::EndPlay(const EEndPlayReason::Type Reason)
{
	if (PollenFx)
	{
		WRFx::Stop(PollenFx);
		PollenFx = nullptr;
	}
	if (PollenHum)
	{
		PollenHum->Stop();
		PollenHum = nullptr;
	}
	Super::EndPlay(Reason);
}

bool AWRPlantSummon::IsHostileTo(const AWRCharacterBase* Other) const
{
	if (!Other || Other == this || Other == OwnerPawn)
	{
		return false;
	}
	// Another plant: friends' plants never fight; anyone else's only where their planters may fight.
	if (const AWRPlantSummon* Plant = Cast<AWRPlantSummon>(Other))
	{
		if (Plant->OwnerName == OwnerName || FWRSocialRules::CanShare(OwnerName, Plant->OwnerName))
		{
			return false;
		}
		return OwnerPawn && Plant->OwnerPawn ? FWRSocialRules::PlayersCanFight(OwnerPawn, Plant->OwnerPawn) : false;
	}
	const FString OtherOwner = Other->GetTameOwner();
	if (!OtherOwner.IsEmpty() && FWRSocialRules::CanShare(OtherOwner, OwnerName))
	{
		return false;   // the planter's or a clanmate's tame
	}
	if (const APlayerState* PS = Other->GetPlayerState())
	{
		if (FWRSocialRules::CanShare(OwnerName, PS->GetPlayerName()))
		{
			return false;
		}
		// Other survivors: only where the planter may fight them (PvP rules, safe zones).
		return OwnerPawn ? FWRSocialRules::PlayersCanFight(OwnerPawn, Other) : false;
	}
	if (!OtherOwner.IsEmpty())
	{
		// Someone else's tame: as its owner stands with the planter.
		return OwnerPawn ? Other->IsHostileTo(OwnerPawn) && OwnerPawn->IsHostileTo(Other) : false;
	}
	return true;   // wild creatures and bosses
}

FString AWRPlantSummon::GetDisplayLabel() const
{
	return OwnerName.IsEmpty() ? KindName : FString::Printf(TEXT("%s's %s"), *OwnerName, *KindName);
}

float AWRPlantSummon::ModifyIncomingDamage(const FWRHit& Hit, float Damage)
{
	// Its planter's own blows and their friends' don't hurt it (an area spell cast over your own turret).
	const AWRCharacterBase* By = WRNature::Caster(Hit.Instigator);
	if (By && (By == OwnerPawn || (!By->IsCreature() && !IsHostileTo(By))))
	{
		return 0.f;
	}
	return Super::ModifyIncomingDamage(Hit, Damage);
}

void AWRPlantSummon::HealShare(float Share)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const UWRAttributeSet* A = GetAttributes();
	if (!HasAuthority() || !IsStanding() || !ASC || !A || Share <= 0.f || A->GetHealth() >= A->GetMaxHealth())
	{
		return;
	}
	ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + A->GetMaxHealth() * Share));
	// Sap wells up green out of its bark.
	WRFx::Broadcast(this, TEXT("nature.heal"), GetActorLocation(), FRotator::ZeroRotator, 0.7f, NAME_None);
}

void AWRPlantSummon::Wither()
{
	if (!HasAuthority() || WitherStart >= 0.f)
	{
		return;
	}
	WitherStart = ServerNow();
	// It folds back into the ground: no kill, no loot, no Forest fire (only a plant that's killed bursts).
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	WRFx::Broadcast(this, TEXT("nature.wither"), Feet, FRotator::ZeroRotator, FMath::Max(1.f, GetSimpleCollisionRadius() / 40.f), TEXT("forest.canopy_rustle"));
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GetCharacterMovement()) GetCharacterMovement()->DisableMovement();
	OnRep_Withering();
}

void AWRPlantSummon::OnDeath(AActor* Killer)
{
	Super::OnDeath(Killer);
	if (WitherStart < 0.f)
	{
		WitherStart = ServerNow();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WRFx::Broadcast(this, TEXT("nature.wither"), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator,
			FMath::Max(1.f, GetSimpleCollisionRadius() / 40.f), TEXT("forest.bark_strip"));
		OnRep_Withering();
	}
	// Forest fire (Fire + Nature 50): a plant that's killed bursts into flame (r 300, 30 damage, burn 50).
	Explode();
}

void AWRPlantSummon::Explode()
{
	AWRCharacterBase* By = OwnerPawn.Get();
	if (bExploded || !By || !By->HasTag(WRNature::Combo_ForestFire))
	{
		return;
	}
	bExploded = true;
	const FWRComboRegistry& C = FWRComboRegistry::Get();
	const float Radius = C.Num(TEXT("forest_fire"), TEXT("radius"), 300.f);
	const FVector At = GetActorLocation();
	WRFx::Broadcast(this, TEXT("nature.forest_fire"), At - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() * 0.5f), FRotator::ZeroRotator, Radius / 100.f, TEXT("fire.impact"));
	TArray<AWRCharacterBase*> Caught;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->IsAlive() && By->IsHostileTo(*It) && IsHostileTo(*It)
			&& FVector::Dist(It->GetActorLocation(), At) < Radius + It->GetSimpleCollisionRadius())
		{
			Caught.Add(*It);
		}
	}
	for (AWRCharacterBase* Who : Caught)
	{
		if (!Who->IsAlive()) continue;
		FWRHit Hit;
		Hit.Instigator = By;
		Hit.Damage = C.Num(TEXT("forest_fire"), TEXT("damage"), 30.f);
		Hit.PoiseDamage = 20.f;
		Hit.Element = WRTags::Element_Fire;
		Hit.BurnBuildup = C.Num(TEXT("forest_fire"), TEXT("burn"), 50.f);
		Hit.bReaction = true;
		Hit.Origin = At;
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

void AWRPlantSummon::OnRep_Pollen()
{
	// Pollen (Bloom node): a pale-yellow haze over the area it slows, a faint bee hum (every machine with a picture).
	if (GetNetMode() == NM_DedicatedServer || !bLookBuilt)
	{
		return;
	}
	const bool bWant = bPollen && WitherStart < 0.f;
	if (bWant && !PollenFx)
	{
		PollenFx = WRFx::Attach(LookRoot, TEXT("nature.pollen"), NAME_None, WRTune(TEXT("passives.pollenRadius"), 400.f) / 100.f);
		PollenHum = WRSound::StartLoop(this, TEXT("nature.pollen"));
	}
	else if (!bWant)
	{
		if (PollenFx) { WRFx::Stop(PollenFx); PollenFx = nullptr; }
		if (PollenHum) { PollenHum->Stop(); PollenHum = nullptr; }
	}
}

void AWRPlantSummon::OnRep_Withering()
{
	OnRep_Pollen();
}

void AWRPlantSummon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	const float WitherSeconds = WRTune(TEXT("nature.plants.witherSeconds"), 1.2f);
	if (HasAuthority())
	{
		if (WitherStart >= 0.f)
		{
			if (Now - WitherStart >= WitherSeconds)
			{
				Destroy();
				return;
			}
		}
		else if (IsAlive())
		{
			// Its time is up, or its planter is gone (logged out, dead): it withers.
			const AWRCharacterBase* Planter = OwnerPawn.Get();
			if (!Planter || !Planter->IsAlive() || Now >= DieAt)
			{
				Wither();
			}
			else if (Now - BornAt >= GrowSeconds)
			{
				if (!bGrownDone)
				{
					bGrownDone = true;
					OnGrown();
				}
				ServerTick(DeltaSeconds);
			}
		}
	}
	if (bLookBuilt)
	{
		const float Grow = GrowSeconds > 0.f ? FMath::Clamp((Now - BornAt) / GrowSeconds, 0.f, 1.f) : 1.f;
		const float WitherShare = WitherStart >= 0.f ? FMath::Clamp((Now - WitherStart) / FMath::Max(0.1f, WitherSeconds), 0.f, 1.f) : 0.f;
		TickLook(DeltaSeconds, Grow, WitherShare);
	}
}

UStaticMeshComponent* AWRPlantSummon::AddTree(USceneComponent* Parent, FName Species, int32 Variant, float Height, const FVector& At, const FRotator& Rot)
{
	UStaticMesh* TreeMesh = GetNetMode() == NM_DedicatedServer ? nullptr : WRTreeKit::Mesh(this, Species, Variant);
	if (!TreeMesh || !Parent)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetStaticMesh(TreeMesh);
	C->SetRelativeLocation(At);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(FVector(Height / FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species))));
	C->RegisterComponent();
	return C;
}

UProceduralMeshComponent* AWRPlantSummon::AddModel(USceneComponent* Parent, const FWRToolMesh& Part, const FVector& At, const FRotator& Rot)
{
	if (!Parent || GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bUseAsyncCooking = true;
	C->SetRelativeLocation(At);
	C->SetRelativeRotation(Rot);
	C->RegisterComponent();
	Part.ApplyTo(C);
	return C;
}

// ================================================================================================ Sprout turret

AWRSproutTurret::AWRSproutTurret(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(40.f, 60.f);
}

void AWRSproutTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSproutTurret, AimAt);
	DOREPLIFETIME(AWRSproutTurret, ShotSerial);
}

AWRCharacterBase* AWRSproutTurret::PickTarget() const
{
	// The nearest enemy within 1,500 cm it can see; its planter's lock comes first when that's in range and in sight.
	const float Range = Num(TEXT("targetRange"), 1500.f);
	const FVector Eye = MouthWorld();
	const auto InSight = [this, &Eye](const AWRCharacterBase* Who)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRTurretSight), false, this);
		Query.AddIgnoredActor(Who);
		return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Who->GetActorLocation(), ECC_WorldStatic, Query);
	};
	const auto Fits = [this, Range](const AWRCharacterBase* Who)
	{
		return Who && Who->IsAlive() && !Who->IsHidden() && IsHostileTo(Who) && FVector::Dist(Who->GetActorLocation(), GetActorLocation()) <= Range + Who->GetSimpleCollisionRadius()
			&& !FWRSocialRules::IsInSafeZone(GetWorld(), Who->GetActorLocation());
	};
	if (const AWRCharacterBase* Planter = OwnerPawn.Get())
	{
		AWRCharacterBase* Theirs = Planter->CombatTarget.Get();
		if (Fits(Theirs) && InSight(Theirs))
		{
			return Theirs;
		}
	}
	AWRCharacterBase* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* Who = *It;
		if (!Fits(Who))
		{
			continue;
		}
		const float D = FVector::DistSquared(Who->GetActorLocation(), GetActorLocation());
		if (D < BestD && InSight(Who))
		{
			BestD = D;
			Best = Who;
		}
	}
	return Best;
}

FVector AWRSproutTurret::MouthWorld() const
{
	// Where the stalk's head is (the server has no look: the same numbers as the model).
	const FVector Top = GetActorLocation() + FVector(0.f, 0.f, -GetSimpleCollisionHalfHeight() + 18.f + 74.f);
	const float Yaw = AimAt ? (AimAt->GetActorLocation() - Top).Rotation().Yaw : GetActorRotation().Yaw;
	return Top + FRotator(0.f, Yaw, 0.f).RotateVector(WRPlantSummonImpl::TurretMouthLocal);
}

void AWRSproutTurret::ServerTick(float DeltaSeconds)
{
	// 3 thorns a second (Quickening: 30% faster), at the target it picks; idle with nothing in reach.
	Clock += DeltaSeconds;
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(AimAt.Get());
	if (!Target || !Target->IsAlive() || Clock >= 0.5f)
	{
		AWRCharacterBase* Picked = PickTarget();
		if (Picked != Target)
		{
			AimAt = Picked;
			Target = Picked;
		}
	}
	float Rate = Num(TEXT("shotsPerSecond"), 3.f);
	if (ServerNow() < QuickenedUntil)
	{
		Rate *= 1.f + FWRComboRegistry::Get().Num(TEXT("quickening"), TEXT("fireRate"), 0.3f);
	}
	const float Every = 1.f / FMath::Max(0.1f, Rate);
	if (!Target)
	{
		Clock = FMath::Min(Clock, Every);
		return;
	}
	if (Clock >= Every)
	{
		Clock = 0.f;
		Shoot(Target);
	}
}

void AWRSproutTurret::Shoot(AWRCharacterBase* Target)
{
	const float Speed = Num(TEXT("thornSpeed"), 2400.f);
	const FVector Mouth = MouthWorld();
	// Aimed where the target is going; the thorn itself flies straight (never homing: COMBAT-AND-LESSONS 1.9).
	const FVector Aim = UWRCombatLibrary::LeadPoint(Mouth, Target->GetActorLocation(), Target->GetVelocity(), Speed);
	const FVector Dir = (Aim - Mouth).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return;
	}
	FWRHit Hit;
	Hit.Instigator = this;
	Hit.Damage = Num(TEXT("thornDamage"), 6.f);
	Hit.PoiseDamage = Num(TEXT("thornPoise"), 3.f);
	Hit.Element = WRNature::Element_Nature;
	Hit.Buildups.Add(TEXT("bleed"), Num(TEXT("thornBleed"), 3.f));
	Hit.PathId = WRPlantSummonImpl::PathId;
	Hit.PhysType = 3;
	Hit.Impact = 1;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWRThornShot* Thorn = GetWorld()->SpawnActor<AWRThornShot>(AWRThornShot::StaticClass(), Mouth, Dir.Rotation(), Params))
	{
		Thorn->Launch(Hit, Speed, this);
	}
	++ShotSerial;
	OnRep_Shot();
}

void AWRSproutTurret::OnRep_Shot()
{
	// Every machine: the pod jerks back, a puff at its mouth, the spit.
	Recoil = 1.f;
	if (GetNetMode() != NM_DedicatedServer)
	{
		WRFx::SpawnAt(this, TEXT("nature.turret_muzzle"), MouthWorld(), Head ? Head->GetComponentRotation() : GetActorRotation(), 1.f);
		WRSound::PlayOn(this, TEXT("nature.turret_shot"));
	}
}

void AWRSproutTurret::BuildLook()
{
	using namespace WRPlantSummonImpl;
	AddModel(LookRoot, TurretMound(), FVector::ZeroVector, FRotator(0.f, WRTreeKit::VariantAt(GetActorLocation()) * 47.f, 0.f));
	Head = NewObject<USceneComponent>(this, TEXT("TurretHead"));
	Head->SetupAttachment(LookRoot);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 18.f + 74.f));
	Head->RegisterComponent();
	AddModel(Head, TurretHead(), FVector::ZeroVector, FRotator::ZeroRotator);
	HeadYaw = GetActorRotation().Yaw;
	// It bursts out of the ground: soil and leaves thrown up, a creeping rustle.
	WRFx::SpawnAt(this, TEXT("nature.sprout"), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f);
	WRSound::PlayOn(this, TEXT("forest.vine_creep"));
}

void AWRSproutTurret::TickLook(float DeltaSeconds, float Grow, float WitherShare)
{
	using namespace WRPlantSummonImpl;
	// Grow: it springs up out of the heap in GrowSeconds; wither: it shrivels and sinks back.
	const float S = FMath::Max(0.02f, Spring(Grow) * (1.f - 0.7f * WitherShare));
	LookRoot->SetRelativeScale3D(FVector(FMath::Lerp(0.6f, 1.f, Grow), FMath::Lerp(0.6f, 1.f, Grow), S));
	LookRoot->SetRelativeLocation(FVector(0.f, 0.f, -GetSimpleCollisionHalfHeight() - 30.f * WitherShare));
	if (!Head)
	{
		return;
	}
	// The head turns smoothly to what it aims at and dips to it; idle, it sways and sniffs about.
	const double Now = FPlatformTime::Seconds();
	float WantYaw = HeadYaw, WantPitch = -5.f;
	if (const AActor* Target = AimAt.Get(); Target && WitherShare <= 0.f)
	{
		const FVector D = Target->GetActorLocation() - Head->GetComponentLocation();
		WantYaw = D.Rotation().Yaw;
		WantPitch = FMath::Clamp(D.Rotation().Pitch, -40.f, 35.f);
	}
	else
	{
		WantYaw = GetActorRotation().Yaw + 35.f * FMath::Sin(float(Now) * 0.6f + float(GetUniqueID() % 97));
		WantPitch = -8.f + 6.f * FMath::Sin(float(Now) * 1.3f);
	}
	HeadYaw = FMath::FixedTurn(HeadYaw, WantYaw, 360.f * DeltaSeconds);
	Recoil = FMath::Max(0.f, Recoil - DeltaSeconds * 8.f);
	const float Droop = 60.f * WitherShare;
	Head->SetWorldRotation(FRotator(WantPitch - Droop + 12.f * Recoil, HeadYaw, 0.f));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 18.f + 74.f) - Head->GetRelativeRotation().Vector() * 7.f * Recoil);
	Head->SetRelativeScale3D(FVector(1.f + 0.12f * Recoil, 1.f - 0.06f * Recoil, 1.f - 0.06f * Recoil));
}

// ================================================================================================ World tree

AWRWorldTree::AWRWorldTree(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 400.f);
}

void AWRWorldTree::OnGrown()
{
	// Its crown is a healing, shielding zone (damage taken -20%: WRNature::DamageTakenScale; Frozen grove reads it).
	WRNature::AddHealZone(this, OwnerPawn.Get(), Num(TEXT("radius"), 600.f), true);
}

void AWRWorldTree::ServerTick(float DeltaSeconds)
{
	// Every second, every ally within 600 cm is healed 6 (Lifebloom, Purify; the first tick on each ends a poison or rot).
	Clock += DeltaSeconds;
	if (Clock < 1.f)
	{
		return;
	}
	Clock = 0.f;
	AWRCharacterBase* By = OwnerPawn.Get();
	if (!By)
	{
		return;
	}
	const float Radius = Num(TEXT("radius"), 600.f);
	const FVector Foot = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	TArray<AWRCharacterBase*> Inside;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsA<AWRPlantSummon>() && FVector::Dist2D(It->GetActorLocation(), Foot) < Radius + It->GetSimpleCollisionRadius()
			&& FMath::Abs(It->GetActorLocation().Z - Foot.Z) < 500.f && WRNature::IsAlly(By, *It))
		{
			Inside.Add(*It);
		}
	}
	for (AWRCharacterBase* Who : Inside)
	{
		const bool bFirst = !Ticked.Contains(Who);
		Ticked.Add(Who);
		if (WRNature::Heal(By, Who, Num(TEXT("healPerSecond"), 6.f), true, bFirst) > 0.f)
		{
			WRFx::BroadcastOn(Who, TEXT("nature.heal"), NAME_None, 0.7f, NAME_None);
		}
	}
}

void AWRWorldTree::BuildLook()
{
	using namespace WRPlantSummonImpl;
	// An 8 m lime from the Tree kit (gold-green crown), its root flare and moss modelled over the ground.
	const float Height = Num(TEXT("height"), 800.f);
	const int32 Variant = WRTreeKit::VariantAt(GetActorLocation());
	AddTree(LookRoot, TEXT("lime"), Variant, Height, FVector::ZeroVector, FRotator(0.f, Variant * 83.f, 0.f));
	AddModel(LookRoot, WorldTreeRoots(), FVector::ZeroVector, FRotator(0.f, Variant * 31.f, 0.f));
	CrownLight = NewObject<UPointLightComponent>(this, TEXT("CrownLight"));
	CrownLight->SetupAttachment(LookRoot);
	CrownLight->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.7f));
	CrownLight->SetLightColor(FLinearColor(0.75f, 0.9f, 0.35f));
	CrownLight->SetAttenuationRadius(Num(TEXT("radius"), 600.f) * 1.3f);
	CrownLight->SetIntensity(0.f);
	CrownLight->SetCastShadows(false);
	CrownLight->RegisterComponent();
	CrownFx = WRFx::Attach(LookRoot, TEXT("nature.world_tree_crown"), NAME_None, Num(TEXT("radius"), 600.f) / 100.f);
	// A tree 8 m tall grows out of the ground in 0.8 s: the earth heaves, wood creaks, birdsong (cooks/biome-forest.md 2.17).
	WRFx::SpawnAt(this, TEXT("nature.world_tree"), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f);
	WRSound::PlayOn(this, TEXT("nature.world_tree"));
}

void AWRWorldTree::TickLook(float DeltaSeconds, float Grow, float WitherShare)
{
	using namespace WRPlantSummonImpl;
	// Rises out of the ground (height first, then the crown spreads), and shrinks back and goes grey as it withers.
	const float Up = Ease(Grow);
	const float Spread = Spring(FMath::Clamp(Grow * 1.25f - 0.25f, 0.f, 1.f));
	const float Gone = 1.f - WitherShare;
	LookRoot->SetRelativeScale3D(FVector(FMath::Max(0.05f, Spread * Gone), FMath::Max(0.05f, Spread * Gone), FMath::Max(0.02f, Up * FMath::Lerp(1.f, 0.4f, WitherShare))));
	LookRoot->SetRelativeLocation(FVector(0.f, 0.f, -GetSimpleCollisionHalfHeight() - 120.f * WitherShare));
	if (CrownLight)
	{
		const float Pulse = 0.85f + 0.15f * FMath::Sin(float(FPlatformTime::Seconds()) * 1.7f);
		CrownLight->SetIntensity(WRTune(TEXT("nature.plants.worldTreeLight"), 2500.f) * Up * Gone * Pulse);
	}
	if (CrownFx && WitherShare > 0.f)
	{
		WRFx::Stop(CrownFx);
		CrownFx = nullptr;
	}
}

// ================================================================================================ Ancient guardian

AWRAncientGuardian::AWRAncientGuardian(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(70.f, 200.f);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 160.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 260.f;
	AutoPossessAI = EAutoPossessAI::Spawned;
}

void AWRAncientGuardian::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRAncientGuardian, SlamStart);
}

void AWRAncientGuardian::OnGrown()
{
	GetCharacterMovement()->MaxWalkSpeed = Num(TEXT("walkSpeed"), 260.f);
	// It draws every creature within 10 m to it as it stands (cooks/biome-forest.md 2.17).
	const float Radius = Num(TEXT("aggroRadius"), 1000.f);
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && IsHostileTo(*It) && It->IsHostileTo(this) && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < Radius)
		{
			It->CombatTarget = this;
			It->NotifyAttackedBy(this);
		}
	}
	WRFx::Broadcast(this, TEXT("nature.guardian_slam"), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.4f, TEXT("nature.guardian_slam"));
}

AWRCharacterBase* AWRAncientGuardian::PickTarget() const
{
	// What its planter fights first, else the nearest enemy within its reach of the planter.
	const AWRCharacterBase* Planter = OwnerPawn.Get();
	const float Leash = Num(TEXT("leash"), 1800.f);
	const auto Fits = [this, Planter, Leash](const AWRCharacterBase* Who)
	{
		return Who && Who->IsAlive() && !Who->IsHidden() && IsHostileTo(Who) && Planter
			&& FVector::Dist(Who->GetActorLocation(), Planter->GetActorLocation()) < Leash
			&& !FWRSocialRules::IsInSafeZone(GetWorld(), Who->GetActorLocation());
	};
	if (Planter && Fits(Planter->CombatTarget.Get()))
	{
		return Planter->CombatTarget.Get();
	}
	AWRCharacterBase* Best = nullptr;
	float BestD = FMath::Square(Num(TEXT("seekRange"), 1500.f));
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		const float D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (D < BestD && Fits(*It))
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

void AWRAncientGuardian::ServerTick(float DeltaSeconds)
{
	const float Now = ServerNow();
	const float Windup = Num(TEXT("slamWindup"), 0.8f);
	// A slam in progress: it stands its ground until the arms come down (the blow lands then, where they land).
	if (Now - SlamStart < Windup + Num(TEXT("slamRecovery"), 0.7f))
	{
		if (!bStruck && Now - SlamStart >= Windup)
		{
			bStruck = true;
			Strike();
		}
		return;
	}
	AWRCharacterBase* Target = Foe.Get();
	if (!Target || !Target->IsAlive() || !IsHostileTo(Target) || FMath::FRand() < DeltaSeconds)
	{
		Target = PickTarget();
		Foe = Target;
	}
	const AWRCharacterBase* Planter = OwnerPawn.Get();
	FVector Goal;
	float Stop;
	if (Target)
	{
		Goal = Target->GetActorLocation();
		Stop = GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius() + Num(TEXT("slamReach"), 150.f);
	}
	else if (Planter)
	{
		// Nothing to fight: it keeps by its planter.
		Goal = Planter->GetActorLocation();
		Stop = Num(TEXT("followDistance"), 450.f);
	}
	else
	{
		return;
	}
	const FVector To = (Goal - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	const float Dist = To.Size();
	if (Dist > Stop)
	{
		AddMovementInput(To.GetSafeNormal(), 1.f);
		return;
	}
	if (!Target)
	{
		return;
	}
	// In reach: face it and slam (one every slamEvery s).
	SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	if (Now >= NextSlamAt)
	{
		SlamStart = Now;
		NextSlamAt = Now + Num(TEXT("slamEvery"), 2.2f);
		bStruck = false;
		// The wind-up you can read: both arms rise over the head, a deep creak.
		WRSound::Broadcast(this, TEXT("forest.bark_strip"));
	}
}

void AWRAncientGuardian::Strike()
{
	// The fists land a slamReach in front of it: everyone hostile in that circle takes the blow (contact where the arms
	// come down, not a sphere round the body).
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	const FVector Contact = Feet + Fwd * (GetSimpleCollisionRadius() + Num(TEXT("slamReach"), 150.f) * 0.7f);
	const float Radius = Num(TEXT("slamRadius"), 170.f);
	WRFx::Broadcast(this, TEXT("nature.guardian_slam"), Contact, GetActorRotation(), Radius / 100.f, TEXT("nature.guardian_slam"));
	TArray<AWRCharacterBase*> Caught;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		if (*It == this || !It->IsAlive() || !IsHostileTo(*It))
		{
			continue;
		}
		const FVector P = It->GetActorLocation();
		if (FVector::Dist2D(P, Contact) < Radius + It->GetSimpleCollisionRadius() && P.Z - It->GetSimpleCollisionHalfHeight() < Contact.Z + 160.f && P.Z + It->GetSimpleCollisionHalfHeight() > Contact.Z - 60.f)
		{
			Caught.Add(*It);
		}
	}
	for (AWRCharacterBase* Who : Caught)
	{
		if (!Who->IsAlive()) continue;
		FWRHit Hit;
		Hit.Instigator = this;
		Hit.Damage = Num(TEXT("slamDamage"), 25.f);
		Hit.PoiseDamage = Num(TEXT("slamPoise"), 45.f);
		Hit.StanceDamage = Num(TEXT("slamStance"), 12.f);
		Hit.Element = WRTags::Element_Physical;
		Hit.PathId = WRPlantSummonImpl::PathId;
		Hit.bMelee = true;
		Hit.bLow = false;
		Hit.PhysType = 2;
		Hit.Impact = 3;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = GetActorLocation();
		Hit.ImpactPoint = Who->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Who, Hit);
	}
}

void AWRAncientGuardian::BuildLook()
{
	using namespace WRPlantSummonImpl;
	Body = NewObject<USceneComponent>(this, TEXT("GuardianBody"));
	Body->SetupAttachment(LookRoot);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 92.f));
	Body->RegisterComponent();
	AddModel(Body, GuardianTorso(), FVector::ZeroVector, FRotator::ZeroRotator);
	// Its crown: a hazel's leaves over the head-burl (the Tree kit), 1.6 m.
	AddTree(Body, TEXT("hazel"), WRTreeKit::VariantAt(GetActorLocation()), Num(TEXT("crownHeight"), 160.f), FVector(-6.f, 0.f, 228.f), FRotator(0.f, 0.f, 0.f));
	const auto Limb = [this](const TCHAR* PivotName, USceneComponent* Parent, const FVector& At, const FWRToolMesh& Part) -> USceneComponent*
	{
		USceneComponent* Pivot = NewObject<USceneComponent>(this, PivotName);
		Pivot->SetupAttachment(Parent);
		Pivot->SetRelativeLocation(At);
		Pivot->RegisterComponent();
		AddModel(Pivot, Part, FVector::ZeroVector, FRotator::ZeroRotator);
		return Pivot;
	};
	ArmL = Limb(TEXT("ArmL"), Body, FVector(0.f, -58.f, 196.f), GuardianArm());
	ArmR = Limb(TEXT("ArmR"), Body, FVector(0.f, 58.f, 196.f), GuardianArm());
	LegL = Limb(TEXT("LegL"), LookRoot, FVector(0.f, -30.f, 92.f), GuardianLeg());
	LegR = Limb(TEXT("LegR"), LookRoot, FVector(0.f, 30.f, 92.f), GuardianLeg());
	// Amber eyes in the eye-knots.
	USceneComponent* Eyes = NewObject<USceneComponent>(this, TEXT("GuardianEyes"));
	Eyes->SetupAttachment(Body);
	Eyes->SetRelativeLocation(FVector(34.f, 0.f, 240.f));
	Eyes->RegisterComponent();
	WRFx::Attach(Eyes, TEXT("nature.guardian_eyes"), NAME_None, 1.f);
	// It pulls itself up out of the ground: the earth tears, a deep groan, footfalls.
	WRFx::SpawnAt(this, TEXT("nature.guardian_rise"), GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), GetActorRotation(), 1.f);
	WRSound::PlayOn(this, TEXT("nature.guardian_rise"));
}

void AWRAncientGuardian::TickLook(float DeltaSeconds, float Grow, float WitherShare)
{
	using namespace WRPlantSummonImpl;
	const float Half = GetSimpleCollisionHalfHeight();
	// Rising: it climbs out of the ground leaning forward, an arm hauling on the earth; withering, it sinks and slumps.
	const float Rise = Ease(Grow);
	const float Sunk = (1.f - Rise) * 380.f + WitherShare * 300.f;
	LookRoot->SetRelativeLocation(FVector(0.f, 0.f, -Half - Sunk));
	LookRoot->SetRelativeRotation(FRotator(-25.f * (1.f - Rise) - 20.f * WitherShare, 0.f, 0.f));
	if (!Body || !ArmL || !ArmR || !LegL || !LegR)
	{
		return;
	}
	// The walk: legs swing from the hips by its speed, arms against them, the body rolls side to side.
	const float Speed = GetVelocity().Size2D();
	const float StepLen = 150.f;
	const float PrevPhase = WalkPhase;
	WalkPhase += DeltaSeconds * Speed / StepLen * PI;
	const float Stride = FMath::Clamp(Speed / 200.f, 0.f, 1.f);
	const float S = FMath::Sin(WalkPhase);
	LegL->SetRelativeRotation(FRotator(28.f * S * Stride, 0.f, 0.f));
	LegR->SetRelativeRotation(FRotator(-28.f * S * Stride, 0.f, 0.f));
	Body->SetRelativeRotation(FRotator(-6.f * Stride, 0.f, 4.f * S * Stride));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 92.f + 6.f * FMath::Abs(FMath::Cos(WalkPhase)) * Stride));
	// A footfall each time a foot comes down (a heavy step on what it stands on).
	if (Stride > 0.2f && FMath::FloorToInt(PrevPhase / PI) != FMath::FloorToInt(WalkPhase / PI))
	{
		WRSound::Footstep(this, 1.6f);
	}
	// The slam (the same server clock everywhere): arms rise over the head through the 0.8 s wind-up, come down hard in
	// 0.12 s onto the ground in front, stay a beat, then swing back.
	const float Windup = Num(TEXT("slamWindup"), 0.8f);
	const float T = ServerNow() - SlamStart;
	float ArmPitch = -20.f * S * Stride;   // walking swing
	float Lean = 0.f;
	if (T >= 0.f && T < Windup + 0.7f)
	{
		if (T < Windup)
		{
			ArmPitch = FMath::Lerp(0.f, 165.f, Ease(T / Windup));   // up and back over the head
			Lean = -8.f * (T / Windup);
		}
		else if (T < Windup + 0.12f)
		{
			ArmPitch = FMath::Lerp(165.f, -70.f, (T - Windup) / 0.12f);   // down to the ground in front
			Lean = FMath::Lerp(-8.f, 22.f, (T - Windup) / 0.12f);
		}
		else
		{
			const float R = FMath::Clamp((T - Windup - 0.12f) / 0.58f, 0.f, 1.f);
			ArmPitch = FMath::Lerp(-70.f, 0.f, Ease(R));
			Lean = FMath::Lerp(22.f, 0.f, Ease(R));
		}
		Body->SetRelativeRotation(FRotator(-Lean, 0.f, 0.f));
	}
	// The arms hang down -Z from the shoulder; a positive pitch here swings them forward and up.
	ArmL->SetRelativeRotation(FRotator(ArmPitch, 0.f, -6.f));
	ArmR->SetRelativeRotation(FRotator(T >= 0.f && T < Windup + 0.7f ? ArmPitch : -ArmPitch, 0.f, 6.f));
	if (WitherShare > 0.f)
	{
		ArmL->SetRelativeRotation(FRotator(-30.f * WitherShare, 0.f, -20.f * WitherShare));
		ArmR->SetRelativeRotation(FRotator(-30.f * WitherShare, 0.f, 20.f * WitherShare));
	}
}

// ================================================================================================ the thorn in flight

AWRThornShot::AWRThornShot()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 2.f;
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(5.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	RootComponent = Collision;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bIsHomingProjectile = false;   // never homing (COMBAT-AND-LESSONS 1.9)
	Movement->InitialSpeed = 2400.f;
	Movement->MaxSpeed = 4000.f;
}

void AWRThornShot::BeginPlay()
{
	Super::BeginPlay();
	if (GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	}
	Collision->OnComponentHit.AddDynamic(this, &AWRThornShot::OnHit);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Model = NewObject<UProceduralMeshComponent>(this, TEXT("ThornModel"));
		Model->SetupAttachment(Collision);
		Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Model->RegisterComponent();
		WRPlantSummonImpl::ThornModel().ApplyTo(Model);
		Trail = WRFx::Attach(Collision, TEXT("nature.thorn_trail"));
	}
}

void AWRThornShot::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Trail)
	{
		WRFx::Stop(Trail);
		Trail = nullptr;
	}
	Super::EndPlay(Reason);
}

void AWRThornShot::Launch(const FWRHit& InHit, float Speed, AActor* From)
{
	Hit = InHit;
	Spitter = From;
	Movement->MaxSpeed = FMath::Max(Movement->MaxSpeed, Speed);
	Movement->Velocity = GetActorForwardVector() * Speed;
	// The spitter and its planter never stop their own thorn.
	if (From)
	{
		Collision->IgnoreActorWhenMoving(From, true);
		if (const AWRPlantSummon* Plant = Cast<AWRPlantSummon>(From); Plant && Plant->OwnerPawn)
		{
			Collision->IgnoreActorWhenMoving(Plant->OwnerPawn, true);
		}
	}
	const float Range = Cast<AWRPlantSummon>(From) ? WRTune(TEXT("spells.sproutTurret.targetRange"), 1500.f) * 1.3f : WRTune(TEXT("spells.thornShot.range"), 3000.f);
	SetLifeSpan(Range / FMath::Max(100.f, Speed) + 0.1f);
}

void AWRThornShot::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult)
{
	if (!HasAuthority() || bSpent)
	{
		return;
	}
	bSpent = true;
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(OtherActor);
	const AWRCharacterBase* By = Cast<AWRCharacterBase>(Hit.Instigator);
	const FVector At = HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : GetActorLocation();
	const FVector Dir = Movement->Velocity.GetSafeNormal();
	// The thorn bites with a dry crack and a spray of splinters.
	WRFx::Broadcast(this, TEXT("nature.thorn_hit"), At, (-Dir).Rotation(), 1.f, TEXT("nature.thorn"));
	if (Target && By && Target->IsAlive() && By->IsHostileTo(Target))
	{
		FWRHit Bite = Hit;
		Bite.Origin = GetActorLocation() - Dir * 100.f;
		Bite.ImpactPoint = At;
		Bite.bProjectile = true;
		UWRCombatLibrary::ApplyHit(Target, Bite);
		// Living conductor (Lightning + Nature 50): a turret's thorn may chain 10 damage and shock 15 to a second enemy
		// within 4 m.
		const AWRPlantSummon* Plant = Cast<AWRPlantSummon>(Spitter.Get());
		AWRCharacterBase* Planter = Plant ? Plant->OwnerPawn.Get() : nullptr;
		const FWRComboRegistry& C = FWRComboRegistry::Get();
		if (Planter && Planter->HasTag(WRNature::Combo_LivingConductor) && FMath::FRand() < C.Num(TEXT("living_conductor"), TEXT("chance"), 0.2f))
		{
			AWRCharacterBase* Next = nullptr;
			float BestD = FMath::Square(C.Num(TEXT("living_conductor"), TEXT("radius"), 400.f));
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				const float D = FVector::DistSquared(It->GetActorLocation(), Target->GetActorLocation());
				if (*It != Target && It->IsAlive() && D < BestD && Plant->IsHostileTo(*It))
				{
					BestD = D;
					Next = *It;
				}
			}
			if (Next)
			{
				const FVector A = Target->GetActorLocation(), B = Next->GetActorLocation();
				WRFx::Broadcast(this, TEXT("lightning.arc"), A, (B - A).Rotation(), FVector::Dist(A, B) / 100.f, TEXT("storm.arc"));
				FWRHit Arc;
				Arc.Instigator = Planter;
				Arc.Damage = C.Num(TEXT("living_conductor"), TEXT("damage"), 10.f);
				Arc.Element = WRTags::Element_Lightning;
				Arc.Buildups.Add(TEXT("shock"), C.Num(TEXT("living_conductor"), TEXT("shock"), 15.f));
				Arc.bReaction = true;
				Arc.bProjectile = true;
				Arc.Origin = A;
				Arc.ImpactPoint = B;
				UWRCombatLibrary::ApplyHit(Next, Arc);
			}
		}
		Destroy();
		return;
	}
	// It sticks where it struck (a tree, a wall, the ground), quivering a moment, then is gone.
	Movement->StopMovementImmediately();
	Movement->SetUpdatedComponent(nullptr);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorLocation(At);
	SetLifeSpan(WRTune(TEXT("nature.plants.thornStickSeconds"), 4.f));
}
