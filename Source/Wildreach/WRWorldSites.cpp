// The world systems' places (cooks/build-world-systems.md 2.1, 2.3, 2.5, 2.6, 2.7, 2.10, 2.12): shrines, summoning
// stones, message stones, lava bombs and falling stars, cores / craters / caches, villages, the hidden tubes and the
// Sealed Vault's door. Each is modelled in code from the build kit (WRBuildLookKit.h) and the rock set the dressing
// uses, in its biome's stone: nothing is an engine shape left bare. Collision is fitted to what's drawn (dais tiers,
// steps, walls), and everything that glows reads at night.

#include "WRGraveTypes.h"
#include "WRCrystalTypes.h"
#include "WRFungalTypes.h"
#include "WRSkyWorld.h"
#include "WRWorldSystems.h"
#include "WRLand.h"
#include "WRStormTypes.h"
#include "WRSwampTypes.h"
#include "WRFarm.h"
#include "WRPathComponent.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace WRSiteLook
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;

	FLinearColor SiteLin(const FColor& C) { return FLinearColor(C); }

	FColor SiteColour(FName Biome, const TCHAR* Field, FColor Fallback) { return WRWorld::Rgb(WRWorld::Biome(Biome), Field, Fallback); }

	UStaticMesh* SiteRockMesh(int32 i)
	{
		static const TCHAR* Names[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
		const TCHAR* N = Names[((i % 6) + 6) % 6];
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
	}

	UMaterialInstanceDynamic* SiteRockMat(UObject* Outer, const FLinearColor& Tint)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Tint"), Tint);
		return M;
	}

	/** A rock of the set fitted to Size (cm) with its bottom at the given point, sunk Sink cm. */
	UStaticMeshComponent* SiteRock(AActor* Owner, USceneComponent* Parent, int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, float Sink = 10.f)
	{
		UStaticMesh* Mesh = SiteRockMesh(Which);
		if (!Mesh) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(Parent);
		const FBox B = Mesh->GetBoundingBox();
		const FVector S = B.GetSize();
		const FVector Scale(Size.X / FMath::Max(1.f, S.X), Size.Y / FMath::Max(1.f, S.Y), Size.Z / FMath::Max(1.f, S.Z));
		C->SetRelativeScale3D(Scale);
		C->SetRelativeRotation(Rot);
		C->SetRelativeLocation(Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Sink)));
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
		return C;
	}

	UBoxComponent* SiteBlock(AActor* Owner, USceneComponent* Parent, const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(Owner);
		B->SetupAttachment(Parent);
		B->SetBoxExtent(Half);
		B->SetRelativeLocation(Centre);
		B->SetRelativeRotation(Rot);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		return B;
	}

	UPointLightComponent* SiteLight(AActor* Owner, USceneComponent* Parent, const FVector& At, const FLinearColor& Colour, float Intensity, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Parent);
		L->SetRelativeLocation(At);
		L->SetLightColor(Colour);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	}

	/** The ground under a point of an actor's (local XY), as a local Z: rocks and posts sit on uneven ground. */
	float SiteGroundZ(const AActor* Owner, const FVector2D& Local, float Fallback = 0.f)
	{
		const FTransform T = Owner->GetActorTransform();
		const FVector W = T.TransformPosition(FVector(Local, 0.f));
		// On a runtime land (the Wasteland) the ground is its height function: its chunks may not have collision yet.
		const float LandZ = WRLand::GroundZAt(W, TNumericLimits<float>::Lowest());
		if (LandZ > TNumericLimits<float>::Lowest()) return T.InverseTransformPosition(FVector(W.X, W.Y, LandZ)).Z;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSiteGround), false, Owner);
		if (Owner->GetWorld()->LineTraceSingleByChannel(Hit, W + FVector(0, 0, 1500), W - FVector(0, 0, 3000), ECC_WorldStatic, Q) && Cast<APawn>(Hit.GetActor()) == nullptr)
		{
			return T.InverseTransformPosition(Hit.ImpactPoint).Z;
		}
		return Fallback;
	}

	/** An inuksuk (two legs, a lintel, a body, arms, a head) in stone, ~1.5 m. */
	void SiteInuksuk(FKit& K, const FSurf& S, const FVector& At, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](const FVector& L) { return At + R.RotateVector(L); };
		K.Add(S, FVector(28, 30, 55), P(FVector(0, -22, 27)), R);
		K.Add(S, FVector(26, 28, 52), P(FVector(0, 22, 26)), R);
		K.Add(S, FVector(36, 82, 20), P(FVector(0, 0, 64)), R);
		K.Add(S, FVector(34, 46, 40), P(FVector(0, 0, 94)), R);
		K.Add(S, FVector(22, 120, 14), P(FVector(0, 0, 120)), R);
		K.Add(S.Dim(0.92f), FVector(26, 26, 26), P(FVector(0, 0, 140)), R);
	}

	/** A slag cairn: three rough stones stacked, the top one glassy. */
	void SiteCairn(AActor* Owner, USceneComponent* Parent, const FVector& At, int32 Seed, UMaterialInterface* Mat, UMaterialInterface* Glass)
	{
		SiteRock(Owner, Parent, Seed, FVector(70, 60, 45), At, FRotator(0.f, Seed * 37.f, 0.f), Mat, true, 8.f);
		SiteRock(Owner, Parent, Seed + 1, FVector(52, 48, 36), At + FVector(0, 0, 36), FRotator(0.f, Seed * 71.f, 4.f), Mat, false, 0.f);
		SiteRock(Owner, Parent, Seed + 2, FVector(34, 30, 26), At + FVector(0, 0, 64), FRotator(0.f, Seed * 13.f, -5.f), Glass ? Glass : Mat, false, 0.f);
	}
}

using namespace WRSiteLook;

// The Swamp's five shrines (cooks/biome-swamp.md 2.11): the Coven's own fire-places, each its own thing on the common
// dais. Palette: bone, bronze gone brown, cypress black, garnet bloodmoss; the fire burns low and dark red.
namespace WRSwampShrine
{
	/** 0 Tithe Mound (bronze bowl), 1 Salt Cross, 2 Root altar, 3 Cage brazier, 4 Falls (bone brazier). */
	int32 Variant(FName Id)
	{
		const FString S = Id.ToString();
		return S.Contains(TEXT("tithe")) ? 0 : S.Contains(TEXT("salt")) ? 1 : S.Contains(TEXT("root")) ? 2 : S.Contains(TEXT("cage")) ? 3 : 4;
	}

	void Build(FKit& K, int32 V, float R, float BowlZ, const TFunctionRef<float(const FVector2D&)>& Ground)
	{
		const FSurf Bone = FSurf::Stone(FLinearColor(FColor(214, 205, 184)) * 0.62f, 0.8f);
		const FSurf BoneOld = FSurf::Stone(FLinearColor(FColor(180, 168, 140)) * 0.5f, 0.85f);
		const FSurf Bronze = FSurf::Steel(FLinearColor(FColor(110, 78, 40)) * 0.55f, 0.55f);
		const FSurf Iron = FSurf::Steel(FLinearColor(FColor(138, 59, 30)) * 0.18f, 0.75f);   // rust #8A3B1E gone dark
		const FSurf Cypress = FSurf::Wood(FLinearColor(0.03f, 0.026f, 0.022f));
		const FSurf Drift = FSurf::Wood(FLinearColor(FColor(170, 160, 140)) * 0.45f);
		const FSurf Salt = FSurf::Glaze(FLinearColor(FColor(230, 225, 211)) * 0.8f, 0.6f);
		const FSurf RootS = FSurf::Wood(FLinearColor(FColor(92, 58, 42)) * 0.45f);
		const FSurf Moss = FSurf::Cloth(FLinearColor(FColor(122, 10, 20)) * 0.5f);            // bloodmoss garnet #7A0A14
		const FSurf Cord = FSurf::Cloth(FLinearColor(0.12f, 0.1f, 0.07f));
		const auto At = [&](float X, float Y, float Lift = 0.f) { return FVector(X, Y, Ground(FVector2D(X, Y)) + Lift); };
		// Every swamp shrine: bone slabs laid round the dais foot and garnet bloodmoss creeping up its lowest tier.
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = 25.7f * i + 8.f;
			const float Ca = FMath::Cos(FMath::DegreesToRadians(A)), Sa = FMath::Sin(FMath::DegreesToRadians(A));
			if (FMath::Abs(Sa) < 0.35f && Ca > 0.f) continue;   // the steps
			const FVector P = At(Ca * (R + 70.f), Sa * (R + 70.f), 3.f);
			K.Add(BoneOld.Dim(0.9f + 0.2f * K.Rand(P, 2)), FVector(60.f, 34.f, 6.f), P, FRotator(0.f, A + 90.f, 2.f * K.Jit(P, 3)));
			K.Add(Moss, FVector(40.f + 30.f * K.Rand(P, 4), 30.f, 8.f), P + FVector(0, 0, 4.f) - FVector(Ca, Sa, 0.f) * 50.f, FRotator(0.f, A, 0.f), EWRLookMesh::Cylinder);
		}
		// Two bone lanterns flank the steps (lit with the shrine).
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector B = At(R + 150.f, Sd * 170.f);
			K.Rod(Bone, B - FVector(0, 0, 30.f), B + FVector(0, 0, 190.f), 9.f);
			K.Rod(Bone, B + FVector(0, 0, 180.f), B + FVector(28.f, 0, 196.f), 5.f);
			for (int32 r = 0; r < 6; ++r) K.Rod(Bone, B + FVector(28.f, 0, 150.f) + FRotator(0.f, 60.f * r, 0.f).Vector() * 11.f, B + FVector(28.f, 0, 186.f) + FRotator(0.f, 60.f * r, 0.f).Vector() * 6.f, 2.f);
			K.Add(FSurf::Lit(FLinearColor(0.9f, 0.32f, 0.12f), 0.5f), FVector(10.f, 10.f, 18.f), B + FVector(28.f, 0, 164.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		if (V == 0)
		{
			// The Tithe Mound's Great Shrine: the iron bowl lined with a broad bronze tithe bowl, worn green-brown, its
			// rim notched where the Coven's cups rested; a stone channel runs from it off the back of the dais and down
			// the mound toward the Throat (2.20: every basin drains toward her). A ring of fetish posts round the mound.
			K.Disc(Bronze, FVector(0, 0, BowlZ + 2.f), FVector::UpVector, 150.f, 10.f);
			for (int32 i = 0; i < 12; ++i) K.Add(Bronze.Dim(0.8f), FVector(10.f, 8.f, 6.f), FVector(0, 0, BowlZ + 9.f) + FRotator(0.f, 30.f * i, 0.f).Vector() * 72.f, FRotator(0.f, 30.f * i, 0.f));
			for (int32 c = 0; c < 8; ++c)
			{
				const FVector P0 = At(-R * 0.5f - c * 110.f, 0.f, 6.f);
				K.Add(FSurf::Stone(FLinearColor(0.08f, 0.075f, 0.07f), 0.9f), FVector(112.f, 40.f, 14.f), P0, FRotator(-3.f, 0.f, 0.f));
				K.Add(FSurf::Glaze(FLinearColor(FColor(74, 10, 15)) * 0.4f, 0.3f), FVector(108.f, 14.f, 2.f), P0 + FVector(0, 0, 7.f), FRotator(-3.f, 0.f, 0.f));
			}
			for (int32 i = 0; i < 8; ++i)
			{
				const float A = 45.f * i + 22.5f;
				const FVector P = At(FMath::Cos(FMath::DegreesToRadians(A)) * (R + 520.f), FMath::Sin(FMath::DegreesToRadians(A)) * (R + 520.f));
				K.Rod(Bone, P - FVector(0, 0, 40.f), P + FVector(0, 0, 230.f), 8.f);
				K.Add(Bone, FVector(22.f, 14.f, 16.f), P + FVector(0, 0, 240.f), FRotator(0.f, A + 180.f, 0.f));   // the crow skull
				K.Spike(BoneOld, P + FVector(0, 0, 240.f) + FRotator(0.f, A + 180.f, 0.f).Vector() * 10.f, FRotator(-10.f, A + 180.f, 0.f).Vector(), 7.f, 22.f);
			}
		}
		else if (V == 1)
		{
			// The Salt Cross: a driftwood cross behind the bowl, bleached and furred with salt where the spray dried on
			// it, lashed at the crossing; salt cakes heaped on the dais foot (the Bloat's counter, offered here).
			const FVector B(-R * 0.62f, 0.f, 70.f);
			K.Rod(Drift, B - FVector(0, 0, 40.f), B + FVector(0, 0, 420.f), 26.f);
			K.Rod(Drift, B + FVector(0, -150.f, 300.f), B + FVector(4.f, 150.f, 312.f), 20.f);
			for (int32 w = 0; w < 5; ++w) K.Rod(Cord, B + FVector(14.f, -20.f, 296.f + w * 5.f), B + FVector(14.f, 20.f, 310.f - w * 5.f), 2.f);
			for (int32 i = 0; i < 16; ++i)
			{
				const float T = K.Rand(FVector(i, 0, 0), 5);
				const bool bArm = i % 3 == 0;
				const FVector P = bArm ? B + FVector(14.f, FMath::Lerp(-140.f, 140.f, T), 306.f) : B + FVector(14.f, 0.f, FMath::Lerp(20.f, 400.f, T));
				K.Add(Salt, FVector(8.f, 22.f + 14.f * T, 14.f + 10.f * T), P, FRotator(0.f, 0.f, 30.f * K.Jit(P, 6)), EWRLookMesh::Cylinder);
			}
			for (int32 i = 0; i < 7; ++i) K.Add(Salt.Dim(0.95f), FVector(34.f, 26.f, 14.f), At(R + 40.f, -R * 0.6f + 28.f * i, 7.f + 14.f * (i % 2)), FRotator(0.f, 20.f * i, 0.f));
		}
		else if (V == 2)
		{
			// The Root altar: prop roots of the Cathedral's mangrove spring from the mud round the dais and arch over the
			// bowl, meeting in a knot 6 m up; the front stays open for the steps.
			const FVector Knot(0.f, 0.f, 620.f);
			for (int32 r = 0; r < 6; ++r)
			{
				const float A = 60.f * r + 30.f;
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, 0.f)) < 25.f) continue;
				const FVector Foot = At(FMath::Cos(FMath::DegreesToRadians(A)) * (R + 260.f), FMath::Sin(FMath::DegreesToRadians(A)) * (R + 260.f), -30.f);
				FVector Prev = Foot;
				for (int32 s = 1; s <= 8; ++s)
				{
					const float T = s / 8.f;
					const FVector P = FMath::Lerp(Foot, Knot, T) + FVector(0, 0, FMath::Sin(T * PI) * 260.f);
					K.Rod(RootS.Dim(0.9f + 0.15f * K.Rand(P, r)), Prev, P, FMath::Lerp(46.f, 26.f, T));
					Prev = P;
				}
			}
			K.Add(RootS, FVector(110.f, 110.f, 90.f), Knot, FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 i = 0; i < 9; ++i) K.Spike(RootS.Dim(0.8f), At(FMath::Cos(i * 0.7f) * (R + 120.f + 40.f * (i % 3)), FMath::Sin(i * 0.7f) * (R + 120.f + 40.f * (i % 3))), FVector::UpVector, 8.f, 30.f + 12.f * (i % 4));
		}
		else if (V == 3)
		{
			// The Cage brazier: the fire burns inside an iron gibbet cage hung from a gallows of two cypress posts and a
			// beam, its chain wound round the beam; the cage's foot rests in the bowl.
			const float CZ0 = BowlZ - 10.f, CZ1 = BowlZ + 210.f;
			for (int32 b = 0; b < 10; ++b)
			{
				const FVector O = FRotator(0.f, 36.f * b, 0.f).Vector() * 74.f;
				K.Rod(Iron, FVector(O.X, O.Y, CZ0), FVector(O.X * 0.9f, O.Y * 0.9f, CZ1), 4.f);
			}
			for (const float Z : { CZ0, CZ0 + 110.f, CZ1 }) K.Disc(Iron, FVector(0, 0, Z), FVector::UpVector, Z == CZ1 ? 136.f : 150.f, 4.f);
			K.Add(Iron, FVector(130.f, 130.f, 40.f), FVector(0, 0, CZ1 + 18.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			const float BeamZ = CZ1 + 260.f;
			for (const float Y : { -260.f, 260.f }) K.Rod(Cypress, At(0.f, Y, -60.f), FVector(0.f, Y, BeamZ + 20.f), 24.f);
			K.Rod(Cypress, FVector(0.f, -300.f, BeamZ), FVector(0.f, 300.f, BeamZ), 22.f);
			for (int32 l = 0; l < 8; ++l) K.Rod(Iron, FVector(0, 0, CZ1 + 40.f + l * 26.f), FVector(0, 0, CZ1 + 62.f + l * 26.f), 3.f);
			for (int32 w = 0; w < 4; ++w) K.Disc(Iron, FVector(0.f, -12.f + 8.f * w, BeamZ), FVector::RightVector, 30.f, 3.f);
		}
		else
		{
			// The Falls shrine: the bowl ringed by a ribcage of bones arching over it (a Mudhulk's, by the size), its skull
			// at the front looking out at the falls; the bones sweat in the mist.
			for (int32 r = 0; r < 7; ++r)
			{
				const float Y = -150.f + r * 50.f;
				for (const float Sd : { -1.f, 1.f })
				{
					FVector Prev(Sd * 120.f, Y, 80.f);
					for (int32 s = 1; s <= 5; ++s)
					{
						const float A = s / 5.f * 150.f;
						const FVector P(Sd * (120.f * FMath::Cos(FMath::DegreesToRadians(A * 0.5f))), Y, 80.f + 260.f * FMath::Sin(FMath::DegreesToRadians(A * 0.6f)));
						K.Rod(Bone, Prev, P, 11.f);
						Prev = P;
					}
				}
			}
			K.Rod(Bone, FVector(0.f, -200.f, 330.f), FVector(0.f, 200.f, 330.f), 18.f);   // the spine over the ribs
			const FVector Skull(R * 0.55f, 0.f, 110.f);
			K.Add(Bone, FVector(110.f, 80.f, 70.f), Skull, FRotator(-8.f, 0.f, 0.f), EWRLookMesh::Cylinder);
			K.Add(Bone, FVector(90.f, 60.f, 22.f), Skull + FVector(60.f, 0.f, -26.f), FRotator(-8.f, 0.f, 0.f));
			for (const float Sd : { -1.f, 1.f }) K.Disc(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f)), Skull + FVector(44.f, Sd * 22.f, 12.f), FVector::ForwardVector, 20.f, 4.f);
			for (const float Sd : { -1.f, 1.f }) K.Spike(BoneOld, Skull + FVector(-20.f, Sd * 40.f, 30.f), FVector(-0.6f, Sd * 0.5f, 0.6f), 16.f, 70.f);
		}
	}
}

// ================================================================================================ AWRShrine

AWRShrine::AWRShrine()
{
	bReplicates = true;
	bAlwaysRelevant = true;   // the map lists every one, and its fire is a landmark from across the land
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRShrine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRShrine, ShrineId);
	DOREPLIFETIME(AWRShrine, Biome);
	DOREPLIFETIME(AWRShrine, DisplayName);
	DOREPLIFETIME(AWRShrine, bGreat);
	DOREPLIFETIME(AWRShrine, bLit);
	DOREPLIFETIME(AWRShrine, bHeld);
	DOREPLIFETIME(AWRShrine, bGuarded);
	DOREPLIFETIME(AWRShrine, KindleDoneAt);
	DOREPLIFETIME(AWRShrine, Kindler);
}

FVector AWRShrine::BowlLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, bGreat ? 235.f : 215.f);
}

void AWRShrine::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	OnRep_Fire();
}

void AWRShrine::BuildLook()
{
	if (bLookBuilt) return;
	bLookBuilt = true;
	const float Sc = bGreat ? 1.3f : 1.f;
	const float R = 300.f * Sc;
	const FColor RockC = SiteColour(Biome, TEXT("rock"), FColor(30, 27, 25));
	const FColor AltC = SiteColour(Biome, TEXT("rockAlt"), FColor(52, 44, 38));
	const bool bFrost = Biome == TEXT("tundra");
	// Collision, every machine: the three dais tiers (an octagon of two crossed boxes each), the steps, the pillar.
	const float Tiers[3][3] = { { R, -60.f, 20.f }, { R * 0.78f, 20.f, 45.f }, { R * 0.56f, 45.f, 70.f } };
	for (const auto& T : Tiers)
	{
		const float Half = T[0] * 0.92f;
		for (const float Yaw : { 0.f, 45.f }) SiteBlock(this, RootComponent, FVector(0, 0, (T[1] + T[2]) * 0.5f), FVector(Half * 0.92f, Half * 0.92f, (T[2] - T[1]) * 0.5f), FRotator(0.f, Yaw, 0.f));
	}
	for (int32 s = 0; s < 3; ++s)
	{
		const float Top = 20.f + 25.f * s;
		SiteBlock(this, RootComponent, FVector(R - 30.f + 45.f * (2 - s) * 0.5f + 40.f, 0, Top * 0.5f - 30.f), FVector(45.f + 22.f * (2 - s), 90.f * Sc, (Top + 60.f) * 0.5f));
	}
	SiteBlock(this, RootComponent, FVector(0, 0, 130.f), FVector(32.f, 32.f, 62.f));

	if (GetNetMode() == NM_DedicatedServer) return;

	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Stone = FSurf::Stone(SiteLin(AltC), 0.9f);
	const FSurf Dark = FSurf::Stone(SiteLin(RockC), 0.93f);
	const FSurf Iron = FSurf::Steel(FLinearColor(0.035f, 0.032f, 0.03f), 0.62f);
	const FSurf Rust = FSurf::Steel(FLinearColor(0.09f, 0.045f, 0.025f), 0.7f);
	// The dais: three hexagonal tiers of fitted stone, the lowest sunk into the ground.
	K.Hex(Dark, 0, 0, -60.f, 20.f, R, 15.f);
	K.Hex(Stone, 0, 0, 20.f, 45.f, R * 0.78f, 0.f);
	// The top tier laid as ten wedge flags round a round centre stone, a finger's gap between them.
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = 36.f * i + 18.f;
		const FVector C = FRotator(0.f, A, 0.f).Vector() * (R * 0.36f);
		K.Add(Stone.Dim(0.92f + 0.12f * K.Rand(C, 3)), FVector(R * 0.36f, 2.f * R * 0.36f * FMath::Sin(PI / 10.f) - 3.f, 24.f), C + FVector(0, 0, 57.f), FRotator(0.f, A, 0.f));
	}
	K.Disc(Stone.Dim(1.08f), FVector(0, 0, 58.f), FVector::UpVector, R * 0.3f, 25.f);
	// The steps up from the front (+X), each a slab with its nose worn.
	for (int32 s = 0; s < 3; ++s)
	{
		const float Top = 20.f + 25.f * s;
		const float X = R - 20.f + 45.f * (2 - s) * 0.5f + 40.f;
		K.Add(Stone.Dim(0.95f + 0.05f * s), FVector(90.f + 44.f * (2 - s), 180.f * Sc, Top + 60.f), FVector(X, 0.f, Top * 0.5f - 30.f));
	}
	// The pillar: a hexagonal column on a plinth, a capital it carries the bowl on.
	K.Hex(Dark, 0, 0, 70.f, 90.f, 44.f);
	K.Hex(Stone, 0, 0, 90.f, 180.f, 30.f, 30.f);
	K.Disc(Dark, FVector(0, 0, 186.f), FVector::UpVector, 74.f, 12.f);
	// The fire-bowl: an iron dish (bottom, sixteen tilted rim plates, a rolled lip) on three straps to the capital.
	const float BowlZ = (bGreat ? 235.f : 215.f) - 20.f;
	K.Disc(Iron, FVector(0, 0, BowlZ - 14.f), FVector::UpVector, 60.f, 8.f);
	for (int32 i = 0; i < 16; ++i)
	{
		const float A = 22.5f * i;
		const FRotator Rot(0.f, A, 0.f);
		K.Add(i % 2 ? Iron : Rust, FVector(8.f, 27.f, 34.f), FVector(0, 0, BowlZ) + Rot.Vector() * 46.f, FRotator(-28.f, A, 0.f));
		K.Add(Iron, FVector(10.f, 10.f, 10.f), FVector(0, 0, BowlZ + 18.f) + Rot.Vector() * 57.f, Rot, EWRLookMesh::Cylinder);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		const FRotator Rot(0.f, 120.f * i + 30.f, 0.f);
		K.Rod(Iron, FVector(0, 0, 190.f) + Rot.Vector() * 64.f, FVector(0, 0, BowlZ - 10.f) + Rot.Vector() * 42.f, 7.f, true);
		K.Add(Iron, FVector(6.f, 6.f, 4.f), FVector(0, 0, BowlZ - 6.f) + Rot.Vector() * 44.f, Rot, EWRLookMesh::Cylinder);
	}
	// Coals and ash in the bowl (they glow under the flames once it's lit).
	K.Disc(FSurf::Lit(FLinearColor(0.35f, 0.08f, 0.02f), 0.6f), FVector(0, 0, BowlZ - 6.f), FVector::UpVector, 84.f, 6.f);
	// Runes: a pale line down each standing stone's inner face (the shrine's people), kept faint: it reads at night.
	const int32 Stones = bGreat ? 9 : 7;
	for (int32 i = 0; i < Stones; ++i)
	{
		const float A = 40.f + (280.f / (Stones - 1)) * i;   // the front arc stays open for the steps
		const FVector Out = FRotator(0.f, A, 0.f).Vector();
		const float H = 140.f + 100.f * K.Rand(Out * 100.f, 9) * Sc;
		const FVector At = Out * (R + 110.f * Sc);
		K.Add(FSurf::Lit(bFrost ? FLinearColor(0.45f, 0.65f, 1.f) : FLinearColor(0.55f, 0.75f, 1.f), 0.35f), FVector(3.f, 6.f, H * 0.55f), At - Out * 32.f + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) + H * 0.55f), FRotator(0.f, A, 0.f));
	}
	// Storm peaks: a copper rod rises out of the bowl, guyed to the pillar; its tip is the brazier (the spark runs up
	// it when the shrine is kindled: shrine.storm), and the standing stones carry thunder marks.
	if (Biome == TEXT("storm_peaks"))
	{
		const FSurf Verd = FSurf::Steel(FLinearColor(FColor(78, 154, 132)) * 0.55f, 0.62f);
		const FSurf Cop = FSurf::Steel(FLinearColor(FColor(184, 115, 51)) * 0.75f, 0.38f);
		K.Rod(Verd, FVector(0, 0, BowlZ - 10.f), FVector(0, 0, BowlZ + 420.f), 14.f);
		K.Rod(Verd, FVector(0, 0, BowlZ + 420.f), FVector(0, 0, BowlZ + 620.f), 9.f);
		K.Spike(Cop, FVector(0, 0, BowlZ + 620.f), FVector::UpVector, 10.f, 60.f);
		for (int32 i = 0; i < 3; ++i)
		{
			const FRotator Rot(0.f, 120.f * i, 0.f);
			K.Rod(Verd, FVector(0, 0, BowlZ + 300.f), FVector(0, 0, BowlZ) + Rot.Vector() * 58.f, 2.5f);
		}
	}
	// The Swamp's five: each its own brazier and dressing (cook 2.11).
	if (Biome == TEXT("swamp"))
	{
		WRSwampShrine::Build(K, WRSwampShrine::Variant(ShrineId), R, BowlZ, [this](const FVector2D& L) { return SiteGroundZ(this, L); });
	}
	// Biome dressing at the dais foot.
	if (Biome == TEXT("swamp"))
	{
		// Cypress knees standing out of the mud round the dais (the swamp's own cairns).
		for (int32 i = 0; i < 7; ++i)
		{
			const float A = 51.f * i + 70.f;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (R + 260.f + 60.f * K.Rand(FVector(A, 0, 0), 7));
			K.Spike(FSurf::Wood(FLinearColor(0.07f, 0.06f, 0.05f)), At + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) - 10.f), FVector::UpVector, 22.f + 10.f * (i % 3), 50.f + 20.f * (i % 4));
		}
	}
	else if (Biome == TEXT("wasteland"))
	{
		// The Wasteland's dais foot (biome-void 2.11): grey ash drifted against it, a ring of thrown stones left by the
		// newcomers who came to count rifts, and black voidglass splinters fused where an old tear once touched.
		const FSurf Glass = FSurf::Glaze(FLinearColor(0.004f, 0.0035f, 0.006f), 0.12f);
		const FSurf Ash = FSurf::Stone(FLinearColor(FColor(154, 149, 140)), 0.97f);
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = 360.f / 14.f * i + 9.f * K.Jit(FVector(i, 3, 0), 1);
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (R + 70.f + 40.f * K.Rand(FVector(A, 1, 0), 2));
			K.Add(Ash, FVector(16.f, 13.f, 10.f), At + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) + 4.f), FRotator(0.f, A * 3.f, 0.f));
		}
		for (int32 i = 0; i < 7; ++i)
		{
			const float A = 51.f * i + 20.f;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (R + 30.f + 50.f * K.Rand(FVector(A, 0, 0), 4));
			K.Spike(Glass, At + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) - 4.f), FVector(0.25f * FMath::Cos(A), 0.25f * FMath::Sin(A), 1.f).GetSafeNormal(), 6.f + 3.f * (i % 3), 26.f + 14.f * (i % 4));
			K.Add(Ash, FVector(90.f, 46.f, 5.f), At + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) + 1.f), FRotator(0.f, A, 0.f));
		}
	}
	else if (Biome == TEXT("storm_peaks"))
	{
		// Thunder marks (six-petal rosettes) cut into flat stones laid at the dais foot.
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = 60.f * i + 30.f;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (R + 40.f);
			const FVector Base = At + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) + 6.f);
			K.Add(Stone.Dim(1.1f), FVector(70.f, 70.f, 10.f), Base, FRotator(0.f, A, 0.f));
			for (int32 p = 0; p < 6; ++p) K.Add(Dark, FVector(26.f, 8.f, 1.5f), Base + FVector(0, 0, 6.f) + FRotator(0.f, A + 60.f * p, 0.f).Vector() * 13.f, FRotator(0.f, A + 60.f * p, 0.f));
		}
	}
	else if (bFrost)
	{
		const FSurf Granite = FSurf::Stone(SiteLin(RockC), 0.85f);
		for (const float Side : { -1.f, 1.f })
		{
			const FVector At(R + 160.f, Side * 190.f * Sc, 0.f);
			SiteInuksuk(K, Granite, At + FVector(0, 0, SiteGroundZ(this, FVector2D(At))), 0.f);
		}
		// Prayer strips on a cord between the two stones beside the steps: red, blue, yellow, the band's colours.
		const FVector A0 = FRotator(0.f, 40.f, 0.f).Vector() * (R + 110.f) + FVector(0, 0, 150.f + SiteGroundZ(this, FVector2D(FRotator(0.f, 40.f, 0.f).Vector() * (R + 110.f))));
		const FVector A1 = FRotator(0.f, 320.f, 0.f).Vector() * (R + 110.f) + FVector(0, 0, 150.f + SiteGroundZ(this, FVector2D(FRotator(0.f, 320.f, 0.f).Vector() * (R + 110.f))));
		K.Rod(FSurf::Cloth(FLinearColor(0.25f, 0.2f, 0.14f)), A0, A1, 1.6f);
		static const FLinearColor Band[3] = { FLinearColor(0.6f, 0.04f, 0.03f), FLinearColor(0.03f, 0.1f, 0.45f), FLinearColor(0.75f, 0.55f, 0.04f) };
		for (int32 i = 1; i < 10; ++i)
		{
			const FVector P = FMath::Lerp(A0, A1, i / 10.f) - FVector(0, 0, 8.f * FMath::Sin(PI * i / 10.f));
			K.Add(FSurf::Cloth(Band[i % 3]), FVector(1.f, 10.f, 34.f), P - FVector(0, 0, 18.f), FRotator(0.f, (A1 - A0).Rotation().Yaw, 4.f * K.Jit(P, 2)));
		}
	}
	else
	{
		// Sulfur crust round the dais foot: yellow scabs where the vents' breath settled.
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = 47.f * i + 11.f;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (R + 20.f + 60.f * K.Rand(FVector(A, 0, 0), 4));
			K.Add(FSurf::Glaze(SiteLin(SiteColour(Biome, TEXT("accent"), FColor(217, 194, 74))), 0.55f), FVector(70.f + 50.f * K.Rand(At, 5), 50.f, 4.f), At + FVector(0, 0, SiteGroundZ(this, FVector2D(At)) + 1.f), FRotator(0.f, A * 2.f, 0.f));
		}
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);

	// The standing stones themselves: rocks of the set, stood up and leaning a little, in the biome's rock.
	UMaterialInstanceDynamic* RockMat = SiteRockMat(this, SiteLin(RockC) * 4.f);
	UMaterialInstanceDynamic* GlassMat = SiteRockMat(this, FLinearColor(0.02f, 0.02f, 0.025f));
	for (int32 i = 0; i < Stones; ++i)
	{
		const float A = 40.f + (280.f / (Stones - 1)) * i;
		const FVector Out = FRotator(0.f, A, 0.f).Vector();
		const FVector At = Out * (R + 110.f * Sc);
		const float H = 140.f + 100.f * FWRBuildLook::Hash01(GetActorTransform().TransformPosition(Out * 100.f), 9) * Sc;
		SiteRock(this, RootComponent, i + 3, FVector(70.f * Sc, 46.f * Sc, H), At + FVector(0, 0, SiteGroundZ(this, FVector2D(At))), FRotator(4.f * FMath::Sin(i * 1.7f), A, 3.f * FMath::Cos(i * 2.3f)), RockMat, true, 18.f);
		Runes.Add(SiteLight(this, RootComponent, At - Out * 60.f + FVector(0, 0, H * 0.55f), FLinearColor(0.6f, 0.75f, 1.f), 450.f, 260.f));
	}
	if (Biome == TEXT("swamp"))
	{
		// Bone lanterns by the steps: dark until the shrine is lit (cook 2.1 night lights).
		for (const float Sd : { -1.f, 1.f })
		{
			UPointLightComponent* L = SiteLight(this, RootComponent, FVector(R + 178.f, Sd * 170.f, SiteGroundZ(this, FVector2D(R + 178.f, Sd * 170.f)) + 164.f), FLinearColor(1.f, 0.42f, 0.16f), 0.f, 900.f);
			if (L) L->ComponentTags.Add(TEXT("swamp_lantern"));
		}
	}
	else if (!bFrost)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = 40.f + 70.f * i + 35.f;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * (R + 220.f * Sc);
			SiteCairn(this, RootComponent, At + FVector(0, 0, SiteGroundZ(this, FVector2D(At))), i * 3, RockMat, GlassMat);
		}
	}
}

void AWRShrine::OnRep_Fire()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const bool bBurning = bLit && !bHeld;
	if (bBurning && !Fire)
	{
		const bool bStorm = Biome == TEXT("storm_peaks");
		// The Swamp's fires burn low and dark red (the Coven's braziers: cook 2.11).
		const bool bSwamp = Biome == TEXT("swamp");
		// The Wasteland's braziers burn small and pale under the rifts' glow (biome-void 2.11: a black iron bowl).
		const bool bVoid = Biome == TEXT("wasteland");
		Fire = WRFx::Attach(RootComponent, bStorm ? FName(TEXT("shrine.storm")) : FName(TEXT("shrine.fire")), NAME_None, (bGreat ? 2.4f : 1.9f) * (bSwamp ? WRTune(TEXT("swampWorld.shrineFireScale"), 0.65f) : 1.f));
		if (Fire) Fire->SetRelativeLocation(BowlLocation() - GetActorLocation() + (bStorm ? FVector(0, 0, 680.f) : FVector::ZeroVector));
		FireLight = SiteLight(this, RootComponent, BowlLocation() - GetActorLocation() + FVector(0, 0, 80.f), bSwamp ? FLinearColor(0.85f, 0.16f, 0.07f) : bVoid ? FLinearColor(1.f, 0.72f, 0.5f) : FLinearColor(1.f, 0.55f, 0.2f),
			WRTune(TEXT("world.shrineLight"), 9000.f) * (bSwamp ? 0.6f : bVoid ? 0.75f : 1.f), bGreat ? 2600.f : 2000.f);
	}
	else if (!bBurning && Fire)
	{
		WRFx::Stop(Fire);
		Fire = nullptr;
		if (FireLight) { FireLight->DestroyComponent(); FireLight = nullptr; }
	}
	// The runes glow brighter on a dark shrine: the cold silhouette you find at night.
	for (UPointLightComponent* L : Runes) if (L) L->SetIntensity(bBurning ? 220.f : 650.f);
	for (UActorComponent* C : GetComponents())
	{
		UPointLightComponent* L = Cast<UPointLightComponent>(C);
		if (L && L->ComponentHasTag(TEXT("swamp_lantern"))) L->SetIntensity(bBurning ? WRTune(TEXT("swampWorld.boneLantern"), 1800.f) : 0.f);
	}
}

FString AWRShrine::StartKindle(AWRPlayerController* PC)
{
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!Me || !Me->IsAlive()) return FString();
	if (bHeld) return TEXT("Invaders hold this shrine: drive them off");
	if (bLit) return FString();
	if (bGuarded) return TEXT("Its guards are still here");
	const float Now = UWRWeatherSubsystem::Now(this);
	if (KindleDoneAt > Now) return KindlerPawn.Get() == Me ? FString() : TEXT("Someone is kindling it");
	if (Me->HasTag(WRTags::State_CombatTagged)) return TEXT("In combat: not now");
	KindleDoneAt = Now + WRWorld::Num(TEXT("shrines.kindleSeconds"), 3.f);
	Kindler = PC->PlayerState->GetPlayerName();
	KindlerPawn = Me;
	KindleFrom = Me->GetActorLocation();
	Me->SetActorRotation(FRotator(0.f, (GetActorLocation() - Me->GetActorLocation()).Rotation().Yaw, 0.f));
	Me->MulticastBodyOneShot(TEXT("pickup"));
	WRFx::Broadcast(this, TEXT("shrine.kindle"), BowlLocation(), FRotator::ZeroRotator, bGreat ? 1.6f : 1.3f, TEXT("world.kindle"), WRWorld::Num(TEXT("shrines.kindleSeconds"), 3.f));
	ForceNetUpdate();
	return TEXT("Kindling: hold still");
}

void AWRShrine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	// A kindle under way: it fails if you step off, fall, or take a blow.
	if (KindleDoneAt > 0.f)
	{
		AWRCharacter* K = KindlerPawn.Get();
		const float Now = UWRWeatherSubsystem::Now(this);
		const bool bBroken = !K || !K->IsAlive() || FVector::Dist(K->GetActorLocation(), KindleFrom) > WRWorld::Num(TEXT("shrines.kindleMove"), 150.f) || K->HasTag(WRTags::Status_Flinch) || K->HasTag(WRTags::Status_Staggered) || bGuarded || bHeld;
		if (bBroken)
		{
			if (AWRPlayerController* PC = K ? Cast<AWRPlayerController>(K->GetController()) : nullptr) PC->ClientNotify(TEXT("The kindling went out"));
			KindleDoneAt = 0.f;
			Kindler.Empty();
			KindlerPawn = nullptr;
		}
		else if (Now >= KindleDoneAt)
		{
			FinishKindle();
		}
	}
	GuardClock -= DeltaSeconds;
	if (GuardClock <= 0.f)
	{
		GuardClock = 1.f;
		StepGuards();
	}
}

void AWRShrine::StepGuards()
{
	if (bGreat) return;
	const double Hours = AWRGameState::Get(this) ? AWRGameState::Get(this)->GetTotalHours() : 0.0;
	// Re-guarding: four game days after it was lit, the fire's gone and the land's hunters take the place back.
	if (bLit && ReguardAtHours >= 0.0 && Hours >= ReguardAtHours)
	{
		bLit = false;
		OnRep_Fire();
		ForceNetUpdate();
	}
	Guards.RemoveAll([](const TWeakObjectPtr<AWRCreature>& G) { return !G.IsValid() || !G->IsAlive() || G->IsTamed(); });
	const bool bWasGuarded = bGuarded;
	if (!bLit && Guards.Num() == 0 && bGuarded)
	{
		bGuarded = false;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->GetPawn() && FVector::Dist(PC->GetPawn()->GetActorLocation(), GetActorLocation()) < 6000.f) PC->ClientNotify(FString::Printf(TEXT("The %s is clear: kindle it"), *DisplayName));
		}
	}
	if (!bLit && !bGuarded && Guards.Num() == 0 && !bWasGuarded)
	{
		// Guards come back the next time someone comes near a dark shrine that isn't freshly cleared.
		static TMap<TWeakObjectPtr<AWRShrine>, double> ClearedAt;
		const double Now = GetWorld()->GetTimeSeconds();
		bool bNear = false;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It && !bNear; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			bNear = P && FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < WRWorld::Num(TEXT("shrines.guardRange"), 12000.f);
		}
		double& Since = ClearedAt.FindOrAdd(this, -1.0);
		if (bNear && Since < 0.0)
		{
			Since = Now;
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			const TSharedPtr<FJsonObject> Root = WRWorld::Obj(TEXT("shrines"));
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("list"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> Row = V->AsObject();
					FString Id;
					Row->TryGetStringField(TEXT("id"), Id);
					const TSharedPtr<FJsonObject>* GuardsObj = nullptr;
					if (FName(*Id) != ShrineId || !Row->TryGetObjectField(TEXT("guards"), GuardsObj)) continue;
					int32 n = 0;
					// stormGuards: more that only hold it while a storm is on (the Glass Scar's Raiju, DC 2.13).
					TArray<TPair<FString, TSharedPtr<FJsonValue>>> All;
					for (const auto& KV : (*GuardsObj)->Values) All.Add(KV);
					const TSharedPtr<FJsonObject>* StormObj = nullptr;
					if (Row->TryGetObjectField(TEXT("stormGuards"), StormObj) && WRStorm::InStorm(this, GetActorLocation()))
					{
						for (const auto& KV : (*StormObj)->Values) All.Add(KV);
					}
					for (const auto& KV : All)
					{
						const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*KV.Key));
						for (int32 i = 0; Def && i < int32(KV.Value->AsNumber()); ++i, ++n)
						{
							// In a loose ring round the dais, facing out: the place they hold.
							const float A = 360.f * n / 7.f + FMath::FRandRange(-15.f, 15.f);
							// A shrine under the ground (the Graveyard's Watchers' Vault) keeps its guards in its room.
							const bool bUnder = WRGrave::InCatacombs(GetActorLocation() + FVector(0, 0, 100.f)) || WRCrystal::InCave(GetActorLocation() + FVector(0, 0, 100.f)) || WRFungal::InCave(GetActorLocation() + FVector(0, 0, 100.f));
							const float Dist = bUnder ? FMath::FRandRange(250.f, 480.f) : FMath::FRandRange(WRWorld::Num(TEXT("shrines.guardRingMin"), 1500.f), WRWorld::Num(TEXT("shrines.guardRingMax"), 2500.f));
							const FVector P = GetActorLocation() + FRotator(0.f, A, 0.f).Vector() * Dist;
							FHitResult G;
							FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGuard), false, this);
							if (!GetWorld()->LineTraceSingleByChannel(G, P + FVector(0, 0, bUnder ? 200.f : 5000.f), P - FVector(0, 0, bUnder ? 600.f : 8000.f), ECC_WorldStatic, Q)) continue;
							if (AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), Def->Id, G.ImpactPoint + FVector(0.f, 0.f, Def->HalfHeight + 30.f), FRotator(0.f, A, 0.f)))
							{
								C->SetHome(GetActorLocation());
								// Gone aggressive while the shrine is dark (the Toll Shrine's Windmane herd).
								const TArray<TSharedPtr<FJsonValue>>* Angry = nullptr;
								if (Row->TryGetArrayField(TEXT("aggressive"), Angry)) for (const auto& Aid : *Angry) if (FName(*Aid->AsString()) == Def->Id) C->bGuardAggressive = true;
								Guards.Add(C);
							}
						}
					}
				}
			}
			// The Swamp's shrines: their guards are named in Data/swamp.json (shrines[].guard) when world.json has none.
			if (Guards.Num() == 0 && Biome == TEXT("swamp"))
			{
				const FWRSwampData& SD = FWRSwampData::Get();
				const FWRSwampData::FShrine* Mine = nullptr;
				for (const FWRSwampData::FShrine& Sh : SD.Shrines)
				{
					if (Sh.Id == ShrineId || (!Mine && FVector::Dist2D(Sh.At, GetActorLocation()) < WRTune(TEXT("swampWorld.shrineMatch"), 6000.f))) Mine = &Sh;
				}
				for (int32 n = 0; Mine && n < Mine->Guard.Num(); ++n)
				{
					const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Mine->Guard[n]);
					if (!Def) continue;
					const float A = 360.f * n / FMath::Max(1, Mine->Guard.Num()) + FMath::FRandRange(-15.f, 15.f);
					const float Dist = FMath::FRandRange(WRWorld::Num(TEXT("shrines.guardRingMin"), 1500.f), WRWorld::Num(TEXT("shrines.guardRingMax"), 2500.f));
					const FVector P = GetActorLocation() + FRotator(0.f, A, 0.f).Vector() * Dist;
					FHitResult G;
					FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampGuard), false, this);
					if (!GetWorld()->LineTraceSingleByChannel(G, P + FVector(0, 0, 5000), P - FVector(0, 0, 8000), ECC_WorldStatic, Q)) continue;
					if (AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), Def->Id, G.ImpactPoint + FVector(0.f, 0.f, Def->HalfHeight + 30.f), FRotator(0.f, A, 0.f)))
					{
						C->SetHome(GetActorLocation());
						Guards.Add(C);
					}
				}
			}
			bGuarded = Guards.Num() > 0;
			if (bGuarded) UE_LOG(LogWildreach, Log, TEXT("SHRINE %s guarded by %d"), *ShrineId.ToString(), Guards.Num());
		}
		else if (!bNear && Since >= 0.0 && Now - Since > 120.0)
		{
			Since = -1.0;   // nobody stayed: they come again for the next visitor
		}
	}
}

void AWRShrine::FinishKindle()
{
	AWRCharacter* K = KindlerPawn.Get();
	AWRPlayerState* PS = K ? K->GetPlayerState<AWRPlayerState>() : nullptr;
	AWRPlayerController* PC = K ? Cast<AWRPlayerController>(K->GetController()) : nullptr;
	KindleDoneAt = 0.f;
	KindlerPawn = nullptr;
	if (!PS) return;
	const bool bFirst = !bEverLit;
	bLit = true;
	bEverLit = true;
	ReguardAtHours = AWRGameState::Get(this) ? AWRGameState::Get(this)->GetTotalHours() + WRWorld::Num(TEXT("shrines.reguardDays"), 4.f) * 24.0 : -1.0;
	OnRep_Fire();
	ForceNetUpdate();
	WRFx::Broadcast(this, TEXT("discover"), BowlLocation(), FRotator::ZeroRotator, 3.f, TEXT("world.shrine_lit"));
	UWRWorldSystems* Sys = UWRWorldSystems::Get(this);
	const TSharedPtr<FJsonObject> Row = WRWorld::Biome(Biome);
	FString PathStr, FactionStr;
	if (Row.IsValid()) { Row->TryGetStringField(TEXT("path"), PathStr); Row->TryGetStringField(TEXT("faction"), FactionStr); }
	// Found for the kindler, their clan and everyone standing with them.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* Each = Cast<AWRPlayerController>(It->Get());
		AWRPlayerState* EPS = Each ? Each->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!EPS || !Each->GetPawn()) continue;
		const bool bHere = FVector::Dist(Each->GetPawn()->GetActorLocation(), GetActorLocation()) < WRWorld::Num(TEXT("shrines.shareRadius"), 3000.f);
		if (EPS != PS && !bHere) continue;
		if (Sys) Sys->NoteFound(ShrineId, EPS->GetPlayerName());
		// Path XP: the full sum the first time, a fifth for lighting it again after it was retaken.
		const float Scale = bFirst ? 1.f : WRWorld::Num(TEXT("shrines.rekindleFraction"), 0.2f);
		const int32 Own = FMath::RoundToInt(WRWorld::Num(bGreat ? TEXT("shrines.xpGreatOwn") : TEXT("shrines.xpLesserOwn"), 1000.f) * Scale);
		const int32 EachXP = FMath::RoundToInt(WRWorld::Num(bGreat ? TEXT("shrines.xpGreatEach") : TEXT("shrines.xpLesserEach"), 500.f) * Scale);
		UWRPathComponent* Paths = EPS->GetPaths();
		FString Got;
		const FName Path(*PathStr);
		if (Paths && (Paths->IsActivePath(Path) || (Paths->GetMainPath().IsNone() && Paths->GetSubPath().IsNone()))) { Paths->AwardXP(Path, Own); Got = FString::Printf(TEXT("+%d %s XP"), Own, *PathStr); }
		else if (Paths)
		{
			for (const FName P : { Paths->GetMainPath(), Paths->GetSubPath() }) if (!P.IsNone()) { Paths->AwardXP(P, EachXP); Got += FString::Printf(TEXT("%s+%d %s XP"), Got.IsEmpty() ? TEXT("") : TEXT(", "), EachXP, *P.ToString()); }
		}
		if (UWRQuestLog* Log = UWRQuestLog::For(EPS))
		{
			if (EPS == PS) Log->LitByMe.AddUnique(ShrineId);
			Log->AddStanding(FName(*FactionStr), int32(WRWorld::Num(TEXT("shrines.standing"), 60.f)));
		}
		if (Sys && EPS == PS) Sys->QuestEvent(EPS, TEXT("shrine"), ShrineId);
		Each->ClientNotify(FString::Printf(TEXT("%s %s: %s"), *DisplayName, bFirst ? TEXT("kindled") : TEXT("rekindled"), *Got));
	}
	UE_LOG(LogWildreach, Log, TEXT("SHRINE %s kindled by %s (%s)"), *ShrineId.ToString(), *PS->GetPlayerName(), bFirst ? TEXT("first") : TEXT("again"));
	// The Toll Shrine lit: lanterns on the Fallen Span's hanging deck for good (cooks/biome-sky.md L3).
	if (ShrineId == TEXT("sky_toll")) if (UWRSkyWorld* SW = UWRSkyWorld::Get(this)) SW->SetTollLit();
	if (PC) WRSound::PlayUI(PC, TEXT("discover"));
}

// ================================================================================================ AWRSummoningStone

AWRSummoningStone::AWRSummoningStone()
{
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSummoningStone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSummoningStone, BossId);
	DOREPLIFETIME(AWRSummoningStone, Biome);
	DOREPLIFETIME(AWRSummoningStone, FlareAt);
}

void AWRSummoningStone::BeginPlay()
{
	Super::BeginPlay();
	SiteBlock(this, RootComponent, FVector(0, 0, 40.f), FVector(80.f, 55.f, 50.f));
	if (GetNetMode() == NM_DedicatedServer) return;
	const bool bFrost = Biome == TEXT("tundra");
	const FColor RockC = SiteColour(Biome, TEXT("rock"), FColor(30, 27, 25));
	const FColor AltC = SiteColour(Biome, TEXT("rockAlt"), FColor(52, 44, 38));
	const FLinearColor Elem = bFrost ? FLinearColor(0.35f, 0.7f, 1.f) : FLinearColor(1.f, 0.38f, 0.08f);
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Stone = FSurf::Stone(SiteLin(AltC), 0.9f);
	// The altar stone: a squat block on a wider footing, its top dished into a bowl, a lip round it.
	K.Add(FSurf::Stone(SiteLin(RockC), 0.94f), FVector(190.f, 140.f, 30.f), FVector(0, 0, -5.f));
	K.Add(Stone, FVector(160.f, 110.f, 70.f), FVector(0, 0, 45.f));
	K.Disc(Stone.Dim(0.7f), FVector(0, 0, 80.f), FVector::UpVector, 70.f, 6.f);
	K.Disc(FSurf::Stone(SiteLin(RockC) * 0.6f, 0.97f), FVector(0, 0, 79.f), FVector::UpVector, 56.f, 6.f);
	// The boss's sigil cut in the front face: a mark of five strokes, laid out from the boss's own name, glowing in its element.
	const uint32 Seed = GetTypeHash(BossId);
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = float((Seed >> (i * 5)) % 180);
		const float L = 22.f + float((Seed >> (i * 3)) % 26);
		const FVector C(81.f, -30.f + 15.f * i, 30.f + float((Seed >> (i * 4)) % 30));
		K.Add(FSurf::Lit(Elem, 0.9f), FVector(2.f, 4.f, L), C, FRotator(0.f, 0.f, A - 90.f));
	}
	K.Add(FSurf::Lit(Elem, 0.9f), FVector(2.f, 60.f, 3.f), FVector(81.f, 0.f, 18.f));
	// The ring of the land's waymarks round it.
	if (bFrost)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = 45.f + 90.f * i;
			const FVector At = FRotator(0.f, A, 0.f).Vector() * 320.f;
			SiteInuksuk(K, FSurf::Stone(SiteLin(RockC), 0.85f), At + FVector(0, 0, SiteGroundZ(this, FVector2D(At))), A + 180.f);
		}
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	if (!bFrost)
	{
		UMaterialInstanceDynamic* RockMat = SiteRockMat(this, SiteLin(RockC) * 4.f);
		UMaterialInstanceDynamic* GlassMat = SiteRockMat(this, FLinearColor(0.02f, 0.02f, 0.025f));
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector At = FRotator(0.f, 30.f + 60.f * i, 0.f).Vector() * 300.f;
			SiteCairn(this, RootComponent, At + FVector(0, 0, SiteGroundZ(this, FVector2D(At))), i * 2 + 1, RockMat, GlassMat);
		}
	}
	Glyph = SiteLight(this, RootComponent, FVector(130.f, 0.f, 50.f), Elem, 500.f, 300.f);
}

void AWRSummoningStone::OnRep_Flare()
{
	if (Glyph) Glyph->SetIntensity(6000.f);
	FTimerHandle T;
	const TWeakObjectPtr<UPointLightComponent> L = Glyph;
	GetWorldTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([L]() { if (L.IsValid()) L->SetIntensity(500.f); }), 4.f, false);
}

FString AWRSummoningStone::Offer(AWRPlayerController* PC)
{
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(BossId);
	if (!Me || !PS || !Def) return FString();
	FString EffigyStr;
	const TSharedPtr<FJsonObject> Eff = WRWorld::Obj(TEXT("summoning.effigies"));
	if (Eff.IsValid()) Eff->TryGetStringField(BossId.ToString(), EffigyStr);
	const FName Effigy(*EffigyStr);
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		if (It->CreatureId == BossId && It->IsAlive()) return FString::Printf(TEXT("%s is already awake"), *Def->Name);
	}
	if (PS->GetInventory()->CountOf(Effigy) <= 0) return FString::Printf(TEXT("Needs the %s (alchemy table)"), *WRWorld::ItemName(Effigy));
	if (Me->HasTag(WRTags::State_CombatTagged)) return TEXT("In combat: not now");
	// Bring it back now: its spawner's wait ends (a placed boss), or its loop's (a roaming one).
	bool bDone = false;
	for (TActorIterator<AWRCreatureSpawner> It(GetWorld()); It && !bDone; ++It) if (It->CreatureId == BossId) bDone = It->SummonNow();
	if (!bDone && Def->Roam.Num() > 0)
	{
		if (AWRCreature* C = AWRCreature::SpawnCreature(GetWorld(), BossId, Def->Roam[0] + FVector(0.f, 0.f, Def->HalfHeight + 200.f), FRotator::ZeroRotator)) bDone = C != nullptr;
	}
	if (!bDone) return FString::Printf(TEXT("%s can't be raised here"), *Def->Name);
	PS->GetInventory()->RemoveItem(Effigy, 1);
	// Whose fight: the summoner's clan and whoever stands with them at the stone.
	UWRWorldSystems::FSummon S;
	S.Boss = BossId;
	S.At = GetWorld()->GetTimeSeconds();
	S.Stone = GetActorLocation();
	S.Party.Add(PS->GetPlayerName());
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && P != Me && P->GetPlayerState() && FVector::Dist(P->GetActorLocation(), GetActorLocation()) < WRWorld::Num(TEXT("summoning.partyRadius"), 3000.f)) S.Party.Add(P->GetPlayerState()->GetPlayerName());
	}
	if (UWRWorldSystems* Sys = UWRWorldSystems::Get(this)) Sys->PendingSummons.Add(S);
	Me->MulticastBodyOneShot(TEXT("pickup"));
	FlareAt = UWRWeatherSubsystem::Now(this);
	OnRep_Flare();
	WRFx::Broadcast(this, TEXT("summon.flare"), GetActorLocation() + FVector(0, 0, 90.f), FRotator::ZeroRotator, 2.f, TEXT("world.summon"));
	UE_LOG(LogWildreach, Log, TEXT("SUMMON %s offered %s"), *PS->GetPlayerName(), *Effigy.ToString());
	return FString::Printf(TEXT("%s rises. The fight is your party's"), *Def->Name);
}

// ================================================================================================ AWRMessageStone

AWRMessageStone::AWRMessageStone()
{
	bReplicates = true;
	SetNetCullDistanceSquared(FMath::Square(9000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRMessageStone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRMessageStone, Text);
	DOREPLIFETIME(AWRMessageStone, Author);
	DOREPLIFETIME(AWRMessageStone, Good);
	DOREPLIFETIME(AWRMessageStone, Poor);
	DOREPLIFETIME(AWRMessageStone, Biome);
}

void AWRMessageStone::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	// A flat stone stood leaning on a footing stone, its face toward whoever comes, the words cut in, a strip of cloth
	// tied round its neck so it reads as placed, not fallen.
	const FColor RockC = SiteColour(Biome, TEXT("rockAlt"), FColor(52, 44, 38));
	UMaterialInstanceDynamic* Mat = SiteRockMat(this, SiteLin(RockC) * 3.f);
	SiteRock(this, RootComponent, 4, FVector(46.f, 40.f, 22.f), FVector(-12.f, 0.f, 0.f), FRotator::ZeroRotator, Mat, false, 6.f);
	SiteRock(this, RootComponent, 1, FVector(18.f, 62.f, 78.f), FVector(-6.f, 0.f, 4.f), FRotator(-14.f, 0.f, 0.f), Mat, false, 4.f);
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FRotator Lean(-14.f, 0.f, 0.f);
	const FSurf Cut = FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.98f);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector At = Lean.RotateVector(FVector(4.f, 0.f, 50.f - 9.f * i));
		K.Add(Cut, FVector(1.5f, 34.f - 6.f * (i % 3), 2.2f), At, Lean);
	}
	const FSurf Rag = FSurf::Cloth(Biome == TEXT("tundra") ? FLinearColor(0.45f, 0.04f, 0.03f) : FLinearColor(0.32f, 0.22f, 0.08f));
	K.Add(Rag, FVector(22.f, 66.f, 7.f), Lean.RotateVector(FVector(-6.f, 0.f, 70.f)), Lean);
	K.Add(Rag, FVector(2.f, 7.f, 30.f), Lean.RotateVector(FVector(6.f, 22.f, 54.f)), FRotator(-6.f, 0.f, 8.f));
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	SiteLight(this, RootComponent, FVector(50.f, 0.f, 60.f), FLinearColor(1.f, 0.85f, 0.6f), 120.f, 200.f);
}

/** The crater list lives on the subsystem (private): the bomb hands its crater over through here. */
struct FWRWorldSystemsAccess
{
	static void AddCrater(UWRWorldSystems* Sys, AWRWorldCache* C)
	{
		UWRWorldSystems::FCrater Cr;
		Cr.Cache = C;
		Cr.LandedHours = AWRGameState::Get(C) ? AWRGameState::Get(C)->GetTotalHours() : 0.0;
		Cr.Biome = C->Biome;
		Sys->Craters.Add(Cr);
	}
};

// ================================================================================================ AWRLavaBomb

AWRLavaBomb::AWRLavaBomb()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRLavaBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRLavaBomb, From);
	DOREPLIFETIME(AWRLavaBomb, To);
	DOREPLIFETIME(AWRLavaBomb, LaunchAt);
	DOREPLIFETIME(AWRLavaBomb, Travel);
	DOREPLIFETIME(AWRLavaBomb, bMeteor);
}

void AWRLavaBomb::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Travel + 3.f);
	if (HasAuthority())
	{
		// The boom at the vent's side, heard far off: 1.2 s before it lands.
		WRSound::PlayAt(this, bMeteor ? TEXT("world.meteor") : TEXT("world.bomb_launch"), bMeteor ? To : From);
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	// A glowing rock (the lava's own material on a rock of the set), its trail of fire and smoke.
	UMaterialInterface* Lava = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Lava.M_Lava"));
	const float Size = bMeteor ? 180.f : 80.f;
	SiteRock(this, RootComponent, bMeteor ? 2 : 5, FVector(Size, Size * 0.85f, Size * 0.8f), FVector(0, 0, -Size * 0.4f), FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Lava, false, 0.f);
	Trail = WRFx::Attach(RootComponent, bMeteor ? TEXT("meteor.trail") : TEXT("bomb.trail"), NAME_None, bMeteor ? 2.5f : 1.2f);
}

void AWRLavaBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = UWRWeatherSubsystem::Now(this);
	const float A = FMath::Clamp((Now - LaunchAt) / FMath::Max(0.05f, Travel), 0.f, 1.f);
	// A ballistic arc: straight in plan, a parabola in height (it never steers: nothing a creature or the world throws
	// homes, COMBAT-AND-LESSONS).
	const float Peak = bMeteor ? 0.f : 1800.f;
	const FVector P = FMath::Lerp(From, To, A) + FVector(0.f, 0.f, 4.f * Peak * A * (1.f - A));
	SetActorLocation(P);
	if (!bGlowShown && GetNetMode() != NM_DedicatedServer && Now >= LaunchAt + Travel - WRWorld::Num(TEXT("events.eruption.bombGlow"), 0.9f))
	{
		bGlowShown = true;
		WRFx::SpawnAt(this, TEXT("bomb.glow"), To + FVector(0, 0, 5.f), FRotator::ZeroRotator, bMeteor ? 3.f : 1.f);
	}
	if (A >= 1.f && !bLanded)
	{
		bLanded = true;
		if (Trail) { WRFx::Stop(Trail); Trail = nullptr; }
		if (HasAuthority()) Land();
		SetActorHiddenInGame(true);
	}
}

void AWRLavaBomb::Land()
{
	const TSharedPtr<FJsonObject> Row = WRWorld::Obj(bMeteor ? TEXT("events.starfall") : TEXT("events.eruption"));
	UWorld* World = GetWorld();
	WRFx::Broadcast(this, TEXT("bomb.blast"), To, FRotator::ZeroRotator, bMeteor ? 4.f : 2.f, bMeteor ? TEXT("world.meteor") : TEXT("world.bomb_land"));
	if (bMeteor)
	{
		AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), To), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!C) return;
		C->Kind = TEXT("crater");
		C->Biome = WRBiome::At(World, To);
		C->Title = TEXT("Fallen star");
		const TSharedPtr<FJsonObject>* Ore = nullptr;
		if (Row.IsValid() && Row->TryGetObjectField(TEXT("ore"), Ore))
		{
			for (const auto& KV : (*Ore)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
				C->Items.Add(FName(*KV.Key), KV.Value->TryGetArray(R) && R->Num() >= 2 ? FMath::RandRange(int32((*R)[0]->AsNumber()), int32((*R)[1]->AsNumber())) : int32(KV.Value->AsNumber()));
			}
		}
		const TSharedPtr<FJsonObject>* Bonus = nullptr;
		if (Row.IsValid() && Row->TryGetObjectField(TEXT("oreBonus"), Bonus)) for (const auto& KV : (*Bonus)->Values) if (FMath::FRand() < KV.Value->AsNumber()) C->Items.FindOrAdd(FName(*KV.Key)) += 1;
		C->FinishSpawning(FTransform(C->GetActorRotation(), To));
		WRFx::Shake(this, 0.6f, To, 15000.f);
		if (UWRWorldSystems* Sys = UWRWorldSystems::Get(this))
		{
			FWRWorldSystemsAccess::AddCrater(Sys, C);
		}
		return;
	}
	// A roof takes it: the first thing over the landing point is a built piece.
	FHitResult Over;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBombRoof), false, this);
	if (World->LineTraceSingleByChannel(Over, To + FVector(0, 0, 3000), To + FVector(0, 0, 50), ECC_WorldStatic, Q))
	{
		if (AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Over.GetActor()))
		{
			Piece->TakeBuildDamage(WRWorld::Num(TEXT("events.eruption.roofDamage"), 60.f), this);
			return;
		}
	}
	// The blast: unblockable fire on everyone in it, through ApplyHit like every blow.
	const float Radius = WRWorld::Num(TEXT("events.eruption.bombRadius"), 350.f);
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || FVector::Dist(C->GetActorLocation(), To) > Radius + C->GetSimpleCollisionRadius()) continue;
		FWRHit Hit;
		Hit.Instigator = nullptr;
		Hit.Damage = WRWorld::Num(TEXT("events.eruption.bombDamage"), 70.f);
		Hit.Element = WRTags::Element_Fire;
		Hit.BurnBuildup = WRWorld::Num(TEXT("events.eruption.bombBurn"), 50.f);
		Hit.bUnblockable = true;
		Hit.Impact = 3;
		Hit.Tier = EWRHitTier::Heavy;
		Hit.Origin = To;
		UWRCombatLibrary::ApplyHit(C, Hit);
	}
	// What it leaves: a core cooling on the ground.
	AWRWorldCache* Core = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), To), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Core) return;
	Core->Kind = TEXT("core");
	Core->Biome = WRBiome::At(World, To);
	Core->Title = TEXT("Bomb core");
	const TSharedPtr<FJsonObject>* Yield = nullptr;
	if (Row.IsValid() && Row->TryGetObjectField(TEXT("core"), Yield))
	{
		for (const auto& KV : (*Yield)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
			Core->Items.Add(FName(*KV.Key), KV.Value->TryGetArray(R) && R->Num() >= 2 ? FMath::RandRange(int32((*R)[0]->AsNumber()), int32((*R)[1]->AsNumber())) : int32(KV.Value->AsNumber()));
		}
	}
	const TSharedPtr<FJsonObject>* Bonus = nullptr;
	if (Row.IsValid() && Row->TryGetObjectField(TEXT("coreBonus"), Bonus)) for (const auto& KV : (*Bonus)->Values) if (FMath::FRand() < KV.Value->AsNumber()) Core->Items.FindOrAdd(FName(*KV.Key)) += 1;
	Core->FinishSpawning(FTransform(Core->GetActorRotation(), To));
	Core->SetLifeSpan(WRWorld::Num(TEXT("events.eruption.coreSeconds"), 600.f));
}



// ================================================================================================ AWRWorldCache

AWRWorldCache::AWRWorldCache()
{
	bReplicates = true;
	SetNetCullDistanceSquared(FMath::Square(60000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRWorldCache::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWorldCache, Kind);
	DOREPLIFETIME(AWRWorldCache, Biome);
	DOREPLIFETIME(AWRWorldCache, Title);
	DOREPLIFETIME(AWRWorldCache, bTaken);
}

void AWRWorldCache::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
	OnRep_Taken();
}

void AWRWorldCache::BuildLook()
{
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	if (Kind == TEXT("core"))
	{
		SiteBlock(this, RootComponent, FVector(0, 0, 20.f), FVector(35.f, 35.f, 25.f));
		if (!bDraw) return;
		UMaterialInterface* Lava = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Lava.M_Lava"));
		UMaterialInstanceDynamic* Crust = SiteRockMat(this, FLinearColor(0.08f, 0.07f, 0.065f));
		// A cracked crust over a glowing heart, half sunk in the scorched hole it made.
		Glowing.Add(SiteRock(this, RootComponent, 5, FVector(70.f, 60.f, 50.f), FVector(0, 0, 0), FRotator(0.f, 0.f, 12.f), Lava, false, 22.f));
		SiteRock(this, RootComponent, 2, FVector(78.f, 64.f, 30.f), FVector(6.f, 0, 8.f), FRotator(0.f, 40.f, -8.f), Crust, false, 0.f);
		Smoke = WRFx::Attach(RootComponent, TEXT("core.smoke"), NAME_None, 1.f);
		return;
	}
	if (Kind == TEXT("crater"))
	{
		SiteBlock(this, RootComponent, FVector(0, 0, 30.f), FVector(70.f, 70.f, 40.f));
		if (!bDraw) return;
		// A 20 m crater: the scorched ground in a ring of thrown rock, the star half buried at its heart.
		UMaterialInstanceDynamic* Scorch = SiteRockMat(this, FLinearColor(0.035f, 0.032f, 0.03f));
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = 22.5f * i + FMath::FRandRange(-8.f, 8.f);
			const float D = FMath::FRandRange(800.f, 1100.f);
			const FVector2D At = FVector2D(FRotator(0.f, A, 0.f).Vector()) * D;
			const float S = FMath::FRandRange(90.f, 190.f);
			SiteRock(this, RootComponent, i, FVector(S, S * 0.8f, S * 0.6f), FVector(At, SiteGroundZ(this, At)), FRotator(FMath::FRandRange(-20.f, 20.f), A, 0.f), Scorch, true, S * 0.2f);
		}
		TArray<FWRLookBlock> Blocks;
		FKit K(GetActorTransform(), Blocks);
		for (int32 i = 0; i < 24; ++i)
		{
			// Scorch on the ground: dark plates following it, thicker toward the middle.
			const float A = 15.f * i;
			for (const float D : { 250.f, 520.f })
			{
				const FVector2D At = FVector2D(FRotator(0.f, A + D * 0.05f, 0.f).Vector()) * D;
				K.Add(FSurf::Stone(FLinearColor(0.012f, 0.011f, 0.01f), 0.99f), FVector(260.f, 200.f, 4.f), FVector(At, SiteGroundZ(this, At) + 1.f), FRotator(0.f, A, 0.f));
			}
		}
		FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
		// The star: a dark iron-grey body with a cold blue light in its cracks.
		UMaterialInstanceDynamic* Star = SiteRockMat(this, FLinearColor(0.05f, 0.06f, 0.08f));
		Glowing.Add(SiteRock(this, RootComponent, 2, FVector(170.f, 150.f, 120.f), FVector(0, 0, 0), FRotator(18.f, 0.f, 8.f), Star, false, 50.f));
		Smoke = WRFx::Attach(RootComponent, TEXT("crater.smoke"), NAME_None, 1.5f);
		Light = WRSiteLook::SiteLight(this, RootComponent, FVector(0, 0, 180.f), FLinearColor(0.6f, 0.75f, 1.f), 7000.f, 4000.f);
		return;
	}
	// A cache in a hidden room: a stone coffer with iron bands and a carved lid, a lore tablet leaning on it.
	SiteBlock(this, RootComponent, FVector(0, 0, 35.f), FVector(55.f, 35.f, 35.f));
	if (!bDraw) return;
	const bool bFrost = Biome == TEXT("tundra");
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Stone = FSurf::Stone(bFrost ? FLinearColor(0.42f, 0.47f, 0.52f) : FLinearColor(0.07f, 0.065f, 0.06f), 0.88f);
	const FSurf Iron = FSurf::Steel(FLinearColor(0.04f, 0.035f, 0.03f), 0.6f);
	K.Add(Stone, FVector(110.f, 70.f, 56.f), FVector(0, 0, 28.f));
	K.Add(Stone.Dim(1.1f), FVector(116.f, 76.f, 14.f), FVector(0, 0, 63.f));
	for (const float X : { -38.f, 0.f, 38.f }) K.Add(Iron, FVector(8.f, 78.f, 72.f), FVector(X, 0, 36.f));
	K.Add(FSurf::Stone(Stone.Color * 1.2f, 0.9f), FVector(6.f, 46.f, 60.f), FVector(-62.f, 0.f, 28.f), FRotator(-12.f, 0.f, 0.f));
	for (int32 i = 0; i < 4; ++i) K.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.01f), 0.98f), FVector(1.f, 30.f - 4.f * i, 2.f), FVector(-65.f, 0.f, 46.f - 8.f * i), FRotator(-12.f, 0.f, 0.f));
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	Light = WRSiteLook::SiteLight(this, RootComponent, FVector(0, 0, 110.f), bFrost ? FLinearColor(0.55f, 0.75f, 1.f) : FLinearColor(1.f, 0.55f, 0.25f), 900.f, 500.f);
}

void AWRWorldCache::OnRep_Taken()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (Kind == TEXT("crater"))
	{
		// Taken: the star's gone from the hole; the crater stays until it weathers.
		for (USceneComponent* G : Glowing) if (G) G->SetVisibility(!bTaken);
		if (Smoke && bTaken) { WRFx::Stop(Smoke); Smoke = nullptr; }
		if (Light) Light->SetIntensity(bTaken ? 0.f : 7000.f);
	}
	else if (Kind == TEXT("core"))
	{
		for (USceneComponent* G : Glowing) if (G) G->SetVisibility(!bTaken);
	}
	else if (Light)
	{
		Light->SetIntensity(bTaken ? 0.f : 900.f);
	}
}

FString AWRWorldCache::Take(AWRPlayerController* PC)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || bTaken) return TEXT("Nothing left");
	if (Kind == TEXT("vault"))
	{
		bool bOpen = false;
		for (TActorIterator<AWRVaultDoor> It(GetWorld()); It; ++It) bOpen |= It->bOpen && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 1500.f;
		if (!bOpen) return TEXT("The vault is sealed. Something would have to shake it open");
	}
	TArray<FString> Got;
	for (const auto& KV : Items)
	{
		const int32 Left = PS->GetInventory()->AddItem(KV.Key, KV.Value);
		if (Left > 0)
		{
			TArray<FWRItemStack> Spill;
			Spill.Add(FWRInventoryRules::MakeInstance(KV.Key));
			Spill.Last().Count = Left;
			AWRLootBag::Spill(GetWorld(), GetActorLocation() + FVector(0, 0, 50.f), PS->GetPlayerName(), Spill);
		}
		Got.Add(FString::Printf(TEXT("%s x%d"), *WRWorld::ItemName(KV.Key), KV.Value));
	}
	bTaken = true;
	OnRep_Taken();
	if (PC->GetPawn()) Cast<AWRCharacter>(PC->GetPawn())->MulticastBodyOneShot(TEXT("pickup"));
	WRSound::Broadcast(this, TEXT("pickup"));
	if (RefillHours > 0.f)
	{
		RefillAtHours = (AWRGameState::Get(this) ? AWRGameState::Get(this)->GetTotalHours() : 0.0) + RefillHours;
	}
	else if (Kind == TEXT("core"))
	{
		SetLifeSpan(2.f);
	}
	if (!Lore.IsEmpty()) PC->ClientNotify(Lore);
	return Got.Num() ? FString::Printf(TEXT("%s: %s"), *Title, *FString::Join(Got, TEXT(", "))) : Title;
}

void AWRWorldCache::Refill()
{
	bTaken = false;
	RefillAtHours = -1.0;
	OnRep_Taken();
}

// ================================================================================================ AWRVillage

AWRVillage::AWRVillage()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRVillage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVillage, VillageId);
	DOREPLIFETIME(AWRVillage, Biome);
	DOREPLIFETIME(AWRVillage, Style);
	DOREPLIFETIME(AWRVillage, Huts);
	DOREPLIFETIME(AWRVillage, Ring);
	DOREPLIFETIME(AWRVillage, DisplayName);
}

FVector AWRVillage::StallAt() const
{
	// Beside the path in from the steps, to the left as you walk up: the merchant faces the path.
	return GetActorTransform().TransformPosition(FVector(Ring * 0.55f, -Ring * 0.45f, 0.f));
}

void AWRVillage::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AWRVillage::BuildLook()
{
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const bool bLavvu = Style == TEXT("lavvu");
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FColor RockC = SiteColour(Biome, TEXT("rock"), FColor(30, 27, 25));
	const FColor AltC = SiteColour(Biome, TEXT("rockAlt"), FColor(52, 44, 38));
	const FSurf Wall = FSurf::Stone(SiteLin(RockC) * (bLavvu ? 1.f : 1.15f), 0.92f);
	const FSurf Turf = FSurf::Cloth(FLinearColor(0.07f, 0.06f, 0.045f));
	const FSurf Hide = FSurf::Cloth(FLinearColor(0.36f, 0.27f, 0.18f));
	const FSurf Pole = FSurf::Wood(FLinearColor(0.16f, 0.11f, 0.07f));
	const FSurf Lamp = FSurf::Lit(FLinearColor(1.f, 0.6f, 0.25f), 0.95f);
	// The huts: in a ring round the shrine, doors toward it, the front (+X) kept open for the path and the stall.
	for (int32 h = 0; h < Huts; ++h)
	{
		const float A = 70.f + (220.f / FMath::Max(1, Huts - 1)) * h;
		const FVector2D C2 = FVector2D(FRotator(0.f, A, 0.f).Vector()) * Ring;
		const float GZ = SiteGroundZ(this, C2);
		const FVector C(C2, GZ);
		const float DoorYaw = A + 180.f;   // facing the shrine
		const FRotator Face(0.f, DoorYaw, 0.f);
		if (!bLavvu)
		{
			// A round house of dry-laid basalt: twelve wall runs in three courses, the doorway gap toward the shrine,
			// a turf-and-ash cone roof with a glowing smoke hole.
			const float R = 260.f;
			for (int32 s = 0; s < 12; ++s)
			{
				const float SA = 30.f * s + 15.f;
				if (FMath::Abs(FRotator::NormalizeAxis(SA - 0.f)) < 20.f) continue;   // the doorway (local +X of the hut = toward the shrine)
				const FRotator SR(0.f, DoorYaw + SA, 0.f);
				const FVector P = C + SR.Vector() * R;
				for (int32 c = 0; c < 3; ++c)
				{
					K.Add(Wall.Dim(0.9f + 0.18f * K.Rand(P + FVector(0, 0, c), 4)), FVector(70.f, 2.f * R * FMath::Sin(PI / 12.f) - 4.f + 8.f * (c % 2), 62.f), P + FVector(0, 0, 31.f + 62.f * c - 10.f), FRotator(0.f, DoorYaw + SA, 0.f));
				}
				SiteBlock(this, RootComponent, P + FVector(0, 0, 90.f), FVector(35.f, R * FMath::Sin(PI / 12.f), 100.f), FRotator(0.f, DoorYaw + SA, 0.f));
			}
			// Door posts and lintel.
			const FVector Door = C + Face.Vector() * R;
			for (const float Side : { -1.f, 1.f }) K.Add(Pole, FVector(22.f, 22.f, 190.f), Door + Face.RotateVector(FVector(0, Side * 75.f, 95.f)), Face);
			K.Add(Pole, FVector(30.f, 190.f, 22.f), Door + FVector(0, 0, 196.f), Face);
			K.Add(Turf, FVector(560.f, 560.f, 260.f), C + FVector(0, 0, 186.f + 130.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			K.Add(Turf.Dim(0.8f), FVector(600.f, 600.f, 20.f), C + FVector(0, 0, 190.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Disc(FSurf::Lit(FLinearColor(1.f, 0.42f, 0.12f), 0.7f), C + FVector(0, 0, 440.f), FVector::UpVector, 40.f, 6.f);
			// The lantern by the door.
			K.Add(Pole, FVector(10.f, 10.f, 170.f), Door + Face.RotateVector(FVector(60.f, 110.f, 85.f)), Face);
			K.Add(Lamp, FVector(18.f, 18.f, 24.f), Door + Face.RotateVector(FVector(60.f, 110.f, 160.f)), Face);
			if (bDraw)
			{
				SiteLight(this, RootComponent, Door + Face.RotateVector(FVector(70.f, 110.f, 160.f)), FLinearColor(1.f, 0.6f, 0.28f), 1600.f, 900.f);
				if (USceneComponent* Smoke = WRFx::Attach(RootComponent, TEXT("hearth.fire"), NAME_None, 0.6f)) Smoke->SetRelativeLocation(C + FVector(0, 0, 445.f));
			}
		}
		else
		{
			// A lavvu: twelve poles from a 2.2 m ring crossing above a 4.5 m apex, hide wrapped round them, the door
			// flap toward the shrine, the smoke flap open at the top.
			const float R = 220.f;
			for (int32 p = 0; p < 12; ++p)
			{
				const FRotator PR(0.f, DoorYaw + 30.f * p + 15.f, 0.f);
				const FVector Foot = C + PR.Vector() * R;
				const FVector Top = C + PR.Vector() * -38.f + FVector(0, 0, 520.f);
				K.Rod(Pole, Foot, Top, 9.f);
			}
			K.Add(Hide, FVector(2.f * R + 10.f, 2.f * R + 10.f, 430.f), C + FVector(0, 0, 215.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			const FVector Door = C + Face.Vector() * (R - 40.f);
			K.Add(FSurf::Cloth(FLinearColor(0.05f, 0.04f, 0.03f)), FVector(6.f, 90.f, 150.f), Door + FVector(0, 0, 75.f), FRotator(-14.f, DoorYaw, 0.f));
			K.Add(FSurf::Cloth(FLinearColor(0.5f, 0.05f, 0.04f)), FVector(4.f, 92.f, 12.f), Door + FVector(6.f, 0, 152.f), FRotator(-14.f, DoorYaw, 0.f));
			SiteBlock(this, RootComponent, C + FVector(0, 0, 150.f), FVector(R * 0.7f, R * 0.7f, 150.f), FRotator(0.f, 45.f, 0.f));
			// A drying rack with fish beside it.
			const FVector Rack = C + FRotator(0.f, DoorYaw + 70.f, 0.f).Vector() * (R + 160.f);
			const FRotator RR(0.f, DoorYaw + 70.f, 0.f);
			for (const float S : { -1.f, 1.f })
			{
				K.Rod(Pole, Rack + RR.RotateVector(FVector(-30.f, S * 120.f, 0.f)), Rack + RR.RotateVector(FVector(0.f, S * 120.f, 190.f)), 8.f);
				K.Rod(Pole, Rack + RR.RotateVector(FVector(30.f, S * 120.f, 0.f)), Rack + RR.RotateVector(FVector(0.f, S * 120.f, 190.f)), 8.f);
			}
			K.Rod(Pole, Rack + RR.RotateVector(FVector(0.f, -130.f, 186.f)), Rack + RR.RotateVector(FVector(0.f, 130.f, 186.f)), 7.f);
			for (int32 f = 0; f < 6; ++f) K.Add(FSurf::Glaze(FLinearColor(0.4f, 0.42f, 0.45f), 0.4f), FVector(8.f, 14.f, 48.f), Rack + RR.RotateVector(FVector(0.f, -95.f + 38.f * f, 155.f)), RR);
			K.Add(Pole, FVector(10.f, 10.f, 170.f), Door + Face.RotateVector(FVector(80.f, 100.f, 85.f)), Face);
			K.Add(Lamp, FVector(18.f, 18.f, 24.f), Door + Face.RotateVector(FVector(80.f, 100.f, 160.f)), Face);
			if (bDraw)
			{
				SiteLight(this, RootComponent, Door + Face.RotateVector(FVector(90.f, 100.f, 160.f)), FLinearColor(1.f, 0.6f, 0.28f), 1600.f, 900.f);
				if (USceneComponent* Smoke = WRFx::Attach(RootComponent, TEXT("hearth.fire"), NAME_None, 0.45f)) Smoke->SetRelativeLocation(C + FVector(0, 0, 520.f));
			}
		}
	}
	// The master's work place beside the shrine (world.json masters offset): a smith's anvil on its stump with a
	// quench trough (Emberwick), or a hide stretched on its frame (Rimehold). They work facing it.
	{
		FVector2D Spot(250.f, bLavvu ? 620.f : -620.f);
		const TSharedPtr<FJsonObject> Data = WRWorld::Data();
		const TArray<TSharedPtr<FJsonValue>>* MasterRows = nullptr;
		if (Data.IsValid() && Data->TryGetArrayField(TEXT("masters"), MasterRows))
		{
			for (const auto& MV : *MasterRows)
			{
				FString V;
				const TArray<TSharedPtr<FJsonValue>>* Off = nullptr;
				if (MV->AsObject()->TryGetStringField(TEXT("village"), V) && FName(*V) == VillageId && MV->AsObject()->TryGetArrayField(TEXT("offset"), Off) && Off->Num() >= 2) Spot = FVector2D((*Off)[0]->AsNumber(), (*Off)[1]->AsNumber());
			}
		}
		const FVector2D At = Spot + FVector2D(85.f, 0.f);
		const FVector W(At, SiteGroundZ(this, At));
		if (!bLavvu)
		{
			const FSurf Iron = FSurf::Steel(FLinearColor(0.045f, 0.042f, 0.04f), 0.5f);
			K.Add(Pole.Dim(0.8f), FVector(60.f, 60.f, 62.f), W + FVector(0, 0, 31.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(Iron, FVector(30.f, 22.f, 18.f), W + FVector(0, 0, 71.f));
			K.Add(Iron, FVector(56.f, 26.f, 14.f), W + FVector(0, 0, 87.f));
			K.Spike(Iron, W + FVector(28.f, 0, 87.f), FVector::ForwardVector, 14.f, 30.f);
			K.Add(Iron.Dim(1.4f), FVector(10.f, 4.f, 16.f), W + FVector(-24.f, 15.f, 96.f));
			// The quench trough beside it, its water dark, steam-stained rim.
			const FVector T = W + FVector(0.f, 110.f, 0.f);
			K.Add(Wall, FVector(120.f, 50.f, 44.f), T + FVector(0, 0, 22.f));
			K.Add(FSurf::Glass(FLinearColor(0.02f, 0.03f, 0.035f)), FVector(108.f, 38.f, 2.f), T + FVector(0, 0, 41.f));
			SiteBlock(this, RootComponent, W + FVector(0, 0, 47.f), FVector(32.f, 32.f, 47.f));
			SiteBlock(this, RootComponent, T + FVector(0, 0, 22.f), FVector(60.f, 25.f, 22.f));
		}
		else
		{
			// A hide on a frame of two poles and two crossbars, lashed at the corners.
			for (const float Y : { -80.f, 80.f }) K.Rod(Pole, W + FVector(0.f, Y, 0.f), W + FVector(-20.f, Y, 190.f), 8.f);
			for (const float Z : { 40.f, 170.f }) K.Rod(Pole, W + FVector(-Z * 0.1f, -90.f, Z), W + FVector(-Z * 0.1f, 90.f, Z), 7.f);
			K.Add(Hide.Dim(1.15f), FVector(3.f, 150.f, 120.f), W + FVector(-10.f, 0.f, 105.f), FRotator(-6.f, 0.f, 0.f));
		}
	}
	// The path in from the steps: flagstones following the ground for 12 m.
	for (int32 i = 0; i < 14; ++i)
	{
		const FVector2D At(520.f + 90.f * i, 30.f * FMath::Sin(i * 0.9f));
		K.Add(FSurf::Stone(SiteLin(AltC) * 1.1f, 0.9f).Dim(0.9f + 0.2f * K.Rand(FVector(At, 0), 6)), FVector(80.f, 120.f + 30.f * K.Rand(FVector(At, 1), 7), 8.f), FVector(At, SiteGroundZ(this, At) + 2.f), FRotator(0.f, 8.f * K.Jit(FVector(At, 2), 8), 0.f));
	}
	// The merchant's stall: two posts, an awning, a counter of planks, crates and sacks of what they trade.
	const FVector S = GetActorTransform().InverseTransformPosition(StallAt());
	const float SZ = SiteGroundZ(this, FVector2D(S));
	const FRotator SF(0.f, 90.f, 0.f);   // facing the path
	const FVector SC(S.X, S.Y, SZ);
	for (const float Side : { -1.f, 1.f })
	{
		K.Add(Pole, FVector(14.f, 14.f, 260.f), SC + SF.RotateVector(FVector(-90.f, Side * 140.f, 130.f)), SF);
		K.Add(Pole, FVector(12.f, 12.f, 210.f), SC + SF.RotateVector(FVector(90.f, Side * 140.f, 105.f)), SF);
	}
	K.Add(FSurf::Cloth(bLavvu ? FLinearColor(0.35f, 0.05f, 0.04f) : FLinearColor(0.32f, 0.18f, 0.06f)), FVector(230.f, 320.f, 5.f), SC + FVector(0, 0, 238.f), FRotator(-12.f, SF.Yaw, 0.f));
	K.Add(FSurf::Wood(FLinearColor(0.2f, 0.13f, 0.08f)), FVector(70.f, 280.f, 90.f), SC + SF.RotateVector(FVector(70.f, 0.f, 45.f)), SF);
	K.Add(FSurf::Wood(FLinearColor(0.15f, 0.1f, 0.06f)), FVector(56.f, 56.f, 50.f), SC + SF.RotateVector(FVector(-60.f, -90.f, 25.f)), SF);
	K.Add(FSurf::Cloth(FLinearColor(0.42f, 0.36f, 0.24f)), FVector(46.f, 40.f, 54.f), SC + SF.RotateVector(FVector(-50.f, 80.f, 27.f)), SF, EWRLookMesh::Cylinder);
	SiteBlock(this, RootComponent, SC + SF.RotateVector(FVector(70.f, 0.f, 45.f)), FVector(35.f, 140.f, 45.f), SF);
	if (bDraw) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
}

// ================================================================================================ AWRHiddenTube

AWRHiddenTube::AWRHiddenTube()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRHiddenTube::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRHiddenTube, TubeId);
	DOREPLIFETIME(AWRHiddenTube, Route);
}

void AWRHiddenTube::BeginPlay()
{
	Super::BeginPlay();
	BuildLook();
}

void AWRHiddenTube::OnRep_Route() { if (HasActorBegunPlay()) BuildLook(); }

void AWRHiddenTube::BuildLook()
{
	if (bBuilt || Route.Num() < 2) return;
	bBuilt = true;
	TSharedPtr<FJsonObject> Row;
	const TSharedPtr<FJsonObject> Root = WRWorld::Data();
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("hidden"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List) { FString Id; V->AsObject()->TryGetStringField(TEXT("id"), Id); if (FName(*Id) == TubeId) Row = V->AsObject(); }
	}
	if (!Row.IsValid()) return;
	FString BiomeStr, Look;
	Row->TryGetStringField(TEXT("biome"), BiomeStr);
	Row->TryGetStringField(TEXT("look"), Look);
	const bool bIce = Look == TEXT("ice");
	double W = 700.0, H = 550.0;
	Row->TryGetNumberField(TEXT("width"), W);
	Row->TryGetNumberField(TEXT("height"), H);
	TSet<int32> Skylights;
	const TArray<TSharedPtr<FJsonValue>>* Sky = nullptr;
	if (Row->TryGetArrayField(TEXT("skylights"), Sky)) for (const auto& V : *Sky) Skylights.Add(int32(V->AsNumber()));
	const FColor RockC = SiteColour(FName(*BiomeStr), TEXT("rock"), FColor(30, 27, 25));
	UMaterialInstanceDynamic* Mat = SiteRockMat(this, SiteLin(RockC) * 3.f);
	if (bIce)
	{
		// The Blue Below's walls are the ice itself (the rock texture tinted pale still read as dark stone on the snow):
		// the seracs' blue-white (WRWorldDressing serac).
		if (UMaterialInterface* IceBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			Mat = UMaterialInstanceDynamic::Create(IceBase, this);
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.68f, 0.84f));
		}
	}
	// On a map with lands of its own the tube runs under a ridge the ground generator raised over it (round_ground.mjs
	// tube ridges): its walls are deeper there, so the ridge buries their outsides and only the inside shows them.
	const bool bRidge = WRLand::OwnLandMap();
	const float WallDepth = bRidge ? 760.f : 420.f, WallOut = W * 0.5f + (bRidge ? 330.f : 160.f);
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	int32 Seed = 0;
	float SinceLight = 0.f;
	UMaterialInstanceDynamic* HeapMat = nullptr;
	// Where the rooms open off the corridor: the wall on their side is left out there (the cave's mouth).
	TArray<FVector> Mouths;
	const TArray<TSharedPtr<FJsonValue>>* RoomList = nullptr;
	if (Row->TryGetArrayField(TEXT("rooms"), RoomList))
	{
		for (const auto& RV : *RoomList)
		{
			double RA = 0.0;
			RV->AsObject()->TryGetNumberField(TEXT("at"), RA);
			const int32 RI = FMath::Clamp(int32(RA), 0, Route.Num() - 1);
			const FVector RAlong = (Route[FMath::Min(RI + 1, Route.Num() - 1)] - Route[FMath::Max(RI - 1, 0)]).GetSafeNormal2D();
			Mouths.Add(Route[RI] - GetActorLocation() - FVector::CrossProduct(FVector::UpVector, RAlong) * WallOut);
		}
	}
	for (int32 s = 0; s + 1 < Route.Num(); ++s)
	{
		const FVector A = Route[s] - GetActorLocation(), B = Route[s + 1] - GetActorLocation();
		const FVector Along = (B - A).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
		const float Len = FVector::Dist2D(A, B);
		const int32 Steps = FMath::Max(1, FMath::RoundToInt(Len / 300.f));
		for (int32 i = 0; i <= Steps; ++i, ++Seed)
		{
			const float T = float(i) / Steps;
			const FVector2D Mid2 = FVector2D(FMath::Lerp(A, B, T));
			const float Floor = SiteGroundZ(this, Mid2, FMath::Lerp(A.Z, B.Z, T));
			const FVector Mid(Mid2, Floor);
			const FRotator Yaw(0.f, Along.Rotation().Yaw, 0.f);
			// The walls: boulders shoulder to shoulder each side, sunk 3 m so the ground never shows a gap.
			for (const float S : { -1.f, 1.f })
			{
				const FVector2D WP = FVector2D(Mid + Side * S * WallOut);
				if (S < 0.f && Mouths.ContainsByPredicate([&WP](const FVector& M) { return FVector2D::Distance(FVector2D(M), WP) < 450.f; })) continue;
				const float WZ = FMath::Min(SiteGroundZ(this, WP, Floor), Floor);
				SiteRock(this, RootComponent, Seed + (S > 0 ? 3 : 0), FVector(WallDepth, 360.f, H + 380.f), FVector(WP, WZ), FRotator(0.f, Yaw.Yaw + 90.f * S + 10.f * FMath::Sin(Seed * 1.3f), 0.f), Mat, true, 300.f);
			}
			// The roof: slabs resting across the walls; a skylight leaves the first 9 m of its run open.
			const bool bOpen = Skylights.Contains(s) && T * Len < 900.f;
			if (!bOpen)
			{
				SiteRock(this, RootComponent, Seed + 1, FVector(380.f, W + 520.f, 240.f), Mid + FVector(0, 0, H), FRotator(4.f * FMath::Sin(Seed * 0.7f), Yaw.Yaw, 0.f), Mat, true, 0.f);
				if (bIce && bDraw && Seed % 2 == 0)
				{
					// Icicles from the roof.
					for (int32 c = 0; c < 3; ++c) K.Spike(FSurf::Glass(FLinearColor(0.7f, 0.85f, 1.f), 0.15f), Mid + Side * (-200.f + 200.f * c) + FVector(0, 0, H - 10.f), -FVector::UpVector, 18.f, 60.f + 50.f * K.Rand(Mid + FVector(c, 0, 0), 3));
				}
			}
			else if (T * Len < 300.f)
			{
				// Under a skylight: the drift of ash or snow that blew in breaks the 7 m drop (a squashed rock of the walls'
				// set, so it lies as a lumpy drift rather than a drum).
				if (!HeapMat)
				{
					HeapMat = SiteRockMat(this, FLinearColor(0.05f, 0.048f, 0.046f));
					if (bIce) if (UMaterialInterface* SnowBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
					{
						HeapMat = UMaterialInstanceDynamic::Create(SnowBase, this);
						HeapMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.82f, 0.86f, 0.9f));
					}
				}
				SiteRock(this, RootComponent, Seed + 11, FVector(560.f, W * 0.85f, 150.f), Mid, Yaw, HeapMat, false, 60.f);   // no collision: it fills the corridor (snow and ash to wade through)
			}
			// The floor: a seam of glowing crust (cooling lava under a black skin) or a sheet of meltwater ice.
			if (bDraw)
			{
				if (bIce) K.Add(FSurf::Glass(FLinearColor(0.55f, 0.75f, 0.9f), 0.05f), FVector(310.f, W * 0.55f, 3.f), Mid + FVector(0, 0, 2.f), Yaw);
				else
				{
					K.Add(FSurf::Stone(FLinearColor(0.012f, 0.011f, 0.01f), 0.97f), FVector(310.f, W * 0.45f, 4.f), Mid + FVector(0, 0, 2.f), Yaw);
					K.Add(FSurf::Lit(FLinearColor(1.f, 0.35f, 0.06f), 0.85f), FVector(300.f, 10.f, 5.f), Mid + Side * (40.f * FMath::Sin(Seed * 0.8f)) + FVector(0, 0, 3.f), FRotator(0.f, Yaw.Yaw + 14.f * FMath::Sin(Seed * 1.9f), 0.f));
				}
				SinceLight += 300.f;
				if (SinceLight >= 2500.f)
				{
					SinceLight = 0.f;
					SiteLight(this, RootComponent, Mid + FVector(0, 0, 80.f), bIce ? FLinearColor(0.4f, 0.65f, 1.f) : FLinearColor(1.f, 0.4f, 0.1f), 1800.f, 1400.f);
					if (!bIce) if (USceneComponent* E = WRFx::Attach(RootComponent, TEXT("lava.embers"), NAME_None, 1.f)) E->SetRelativeLocation(Mid);
				}
			}
		}
	}
	// The rooms: the sculpted cave shells at the route's corners (their mouths on the corridor).
	const TArray<TSharedPtr<FJsonValue>>* Rooms = nullptr;
	if (Row->TryGetArrayField(TEXT("rooms"), Rooms))
	{
		static const TCHAR* Variants[] = { TEXT("/Game/Props/Caves/CaveA/cave_a.cave_a"), TEXT("/Game/Props/Caves/CaveB/cave_b.cave_b"), TEXT("/Game/Props/Caves/CaveC/cave_c.cave_c") };
		for (int32 r = 0; r < Rooms->Num(); ++r)
		{
			double At = 0.0;
			(*Rooms)[r]->AsObject()->TryGetNumberField(TEXT("at"), At);
			const int32 i = FMath::Clamp(int32(At), 0, Route.Num() - 1);
			const FVector C = Route[i] - GetActorLocation();
			const FVector Along = (Route[FMath::Min(i + 1, Route.Num() - 1)] - Route[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Along);
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Variants[r % 3]);
			if (!Mesh) continue;
			// The cave's tunnel runs out along its +X: point it at the corridor from beside it.
			const FVector Room = C - Side * (W * 0.5f + 2200.f);
			UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(this);
			Shell->SetStaticMesh(Mesh);
			Shell->SetupAttachment(RootComponent);
			Shell->SetRelativeLocation(FVector(FVector2D(Room), SiteGroundZ(this, FVector2D(Room), C.Z)));
			Shell->SetRelativeRotation(FRotator(0.f, Side.Rotation().Yaw, 0.f));
			Shell->SetCollisionProfileName(TEXT("BlockAll"));
			Shell->SetMaterial(0, Mat);
			Shell->RegisterComponent();
			if (bDraw) SiteLight(this, RootComponent, Room + FVector(0, 0, SiteGroundZ(this, FVector2D(Room), C.Z) + 300.f), bIce ? FLinearColor(0.4f, 0.65f, 1.f) : FLinearColor(1.f, 0.45f, 0.15f), 3500.f, 1800.f);
		}
	}
	if (bDraw) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
}

// ================================================================================================ AWRVaultDoor

AWRVaultDoor::AWRVaultDoor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRVaultDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVaultDoor, bOpen);
	DOREPLIFETIME(AWRVaultDoor, Biome);
}

void AWRVaultDoor::BeginPlay()
{
	Super::BeginPlay();
	Leaf = NewObject<USceneComponent>(this, TEXT("Leaf"));
	Leaf->SetupAttachment(RootComponent);
	Leaf->RegisterComponent();
	Block = WRSiteLook::SiteBlock(this, Leaf, FVector(0, 0, 160.f), FVector(30.f, 160.f, 170.f));
	if (GetNetMode() == NM_DedicatedServer) return;
	// A basalt slab in a frame, three iron bands, a ring of runes that wake when the ground shakes it loose.
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Basalt = FSurf::Stone(FLinearColor(0.03f, 0.028f, 0.026f), 0.9f);
	K.Add(Basalt, FVector(60.f, 320.f, 340.f), FVector(0, 0, 160.f));
	for (const float Z : { 60.f, 160.f, 260.f }) K.Add(FSurf::Steel(FLinearColor(0.05f, 0.04f, 0.035f), 0.6f), FVector(64.f, 324.f, 14.f), FVector(0, 0, Z));
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = 36.f * i;
		K.Add(FSurf::Lit(FLinearColor(1.f, 0.4f, 0.1f), 0.5f), FVector(4.f, 8.f, 22.f), FVector(32.f, 0, 160.f) + FVector(0.f, FMath::Cos(FMath::DegreesToRadians(A)) * 80.f, FMath::Sin(FMath::DegreesToRadians(A)) * 80.f), FRotator(0.f, 0.f, A));
	}
	FWRBuildLook::Spawn(this, Leaf, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	// The frame stays (it doesn't sink with the slab).
	TArray<FWRLookBlock> Frame;
	FKit F(GetActorTransform(), Frame);
	for (const float S : { -1.f, 1.f }) F.Add(Basalt.Dim(1.3f), FVector(90.f, 50.f, 380.f), FVector(0, S * 185.f, 180.f));
	F.Add(Basalt.Dim(1.3f), FVector(90.f, 420.f, 60.f), FVector(0, 0, 370.f));
	FWRBuildLook::Spawn(this, RootComponent, Frame, FLinearColor(0.3f, 0.25f, 0.2f), false);
}

void AWRVaultDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// It sinks into its slot over 4 s (grinding stone), and rises the same way: never a snap.
	const float Want = bOpen ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(Sink, Want)) return;
	Sink = FMath::FInterpConstantTo(Sink, Want, DeltaSeconds, 0.25f);
	if (Leaf) Leaf->SetRelativeLocation(FVector(0.f, 0.f, -335.f * FMath::SmoothStep(0.f, 1.f, Sink)));
}
