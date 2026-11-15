// The Swamp's scenery (cooks/biome-swamp.md 2.1-2.4, 2.20; cooks/build-biome-swamp.md 2.4): how each landmark, hand-made
// site, kit POI, camp and route edge is modelled in code from the build-look kit, pack rocks and tinted materials, at real
// scale. Placement is Data/swamp.json's; this file only decides how a thing is built. See WRSwampWorld.h.
// Colour law (cook 1.3 rule 7): the swamp's reds are garnet #7A0A14, oxblood #4A0A0F and rust #8A3B1E, never crimson.

#include "WRSwampWorld.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRFx.h"
#include "WRSwampSystems.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRSwampLook
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;

	FLinearColor C(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	// The palette (cook 2.2): bone, cypress grey and black, Spanish-moss grey-green, duckweed lime, plank grey-green,
	// bloodmoss garnet, oxblood, rust, reed gold, mud, salt, mangrove root red-brown and leaf green, verdigris, peat, clay.
	const FLinearColor Bone = C(214, 205, 184), BoneOld = C(176, 164, 136), CypressGrey = C(106, 100, 88), CypressBlack = C(38, 33, 29);
	const FLinearColor Moss = C(138, 144, 120), Duckweed = C(141, 176, 64), Plank = C(78, 82, 72), Garnet = C(122, 10, 20), Oxblood = C(74, 10, 15);
	const FLinearColor Rust = C(138, 59, 30), FallRust = C(154, 59, 34), Reed = C(180, 154, 90), Mud = C(90, 78, 64), SaltWhite = C(230, 225, 211);
	const FLinearColor RootBrown = C(92, 58, 42), Leaf = C(46, 74, 46), Verdigris = C(78, 154, 132), Peat = C(46, 33, 24), Clay = C(150, 112, 82);
	const FLinearColor Leather = C(74, 46, 28), Pink = C(232, 106, 160), Slate = C(62, 65, 72), Limestone = C(176, 170, 152);

	FSurf BoneS(float K = 1.f) { return FSurf::Stone(Bone * 0.62f * K, 0.78f); }
	FSurf OldBone() { return FSurf::Stone(BoneOld * 0.5f, 0.85f); }
	FSurf Bark(float K = 1.f) { return FSurf::Wood(CypressGrey * 0.42f * K); }
	FSurf Black(float K = 1.f) { return FSurf::Wood(CypressBlack * 0.6f * K); }
	FSurf MossS() { FSurf S = FSurf::Cloth(Moss * 0.5f); return S; }
	FSurf LeafS(float K = 1.f) { return FSurf::Cloth(Leaf * 0.55f * K); }
	FSurf RootS(float K = 1.f) { return FSurf::Wood(RootBrown * 0.48f * K); }
	FSurf Planks(float K = 1.f) { return FSurf::Wood(Plank * 0.5f * K); }
	FSurf IronS() { return FSurf::Steel(Rust * 0.16f, 0.78f); }
	FSurf RustS() { FSurf S = FSurf::Steel(Rust * 0.32f, 0.85f); S.Metal = 0.35f; S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf GreenCopper() { FSurf S = FSurf::Steel(Verdigris * 0.5f, 0.62f); S.Metal = 0.45f; S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf ReedS(float K = 1.f) { return FSurf::Cloth(Reed * 0.55f * K); }
	FSurf MudS(float K = 1.f) { return FSurf::Stone(Mud * 0.45f * K, 0.92f); }
	FSurf PeatS(float K = 1.f) { return FSurf::Stone(Peat * 0.6f * K, 0.96f); }
	FSurf GarnetMoss(float K = 1.f) { return FSurf::Cloth(Garnet * 0.6f * K); }
	FSurf SaltS() { return FSurf::Glaze(SaltWhite * 0.8f, 0.55f); }
	FSurf ClayS(float K = 1.f) { return FSurf::Stone(Clay * 0.5f * K, 0.8f); }
	FSurf LeatherS(float K = 1.f) { return FSurf::Stone(Leather * 0.5f * K, 0.6f); }
	FSurf Cord() { return FSurf::Cloth(C(120, 100, 70) * 0.4f); }
	FSurf Char() { return FSurf::Stone(FLinearColor(0.012f, 0.011f, 0.01f), 0.95f); }
	FSurf Glass(const FLinearColor& Tint) { return FSurf::Glass(Tint, 0.f); }
	FSurf Dark() { return FSurf::Stone(FLinearColor(0.008f, 0.008f, 0.008f), 0.95f); }

	UStaticMesh* RockMesh(int32 i)
	{
		static const TCHAR* Names[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
		const TCHAR* N = Names[((i % 6) + 6) % 6];
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
	}

	UMaterialInstanceDynamic* RockMat(UObject* Outer, const FLinearColor& Tint)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Tint"), Tint);
		return M;
	}
}
using namespace WRSwampLook;

/** One build in progress: the scenery actor, its block lists (the static one and each hung group), its ground. */
struct FWRSwampBuild
{
	struct FGroup
	{
		USceneComponent* Comp = nullptr;
		FTransform T;
		TArray<FWRLookBlock> Blocks;
		TUniquePtr<WRLookKit::FKit> Kit;
	};

	AWRSwampScenery* Act = nullptr;
	FTransform T;
	TArray<FWRLookBlock> Blocks;
	WRLookKit::FKit K;
	bool bServer = false;
	int32 Seed = 0;
	float WaterZ = 0.f;
	TArray<TUniquePtr<FGroup>> Groups;

	explicit FWRSwampBuild(AWRSwampScenery* InA)
		  : Act(InA), T(InA->GetActorTransform()), K(T, Blocks)
	{
		bServer = Act->GetNetMode() == NM_DedicatedServer;
		Seed = int32(GetTypeHash(Act->GetActorLocation().GridSnap(10.f).ToString()) % 9973u);
		WaterZ = FWRSwampData::Get().SeaZ - Act->GetActorLocation().Z;
	}

	float G(float X, float Y) const
	{
		const FVector W = T.TransformPosition(FVector(X, Y, 0.f));
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampGround), false, Act);
		if (Act->GetWorld()->LineTraceSingleByChannel(Hit, W + FVector(0, 0, 4000), W - FVector(0, 0, 8000), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			return T.InverseTransformPosition(Hit.ImpactPoint).Z;
		}
		return 0.f;
	}
	FVector At(float X, float Y, float Lift = 0.f) const { return FVector(X, Y, G(X, Y) + Lift); }
	/** On the ground or floating on the water, whichever is higher. */
	FVector Top(float X, float Y, float Lift = 0.f) const { return FVector(X, Y, FMath::Max(G(X, Y), WaterZ) + Lift); }
	float Rand(int32 Salt) const { return FMath::Frac(FMath::Sin(float(Seed * 12.9898 + Salt * 78.233)) * 43758.5453f); }
	FVector ToLocal(const FVector& World) const { return T.InverseTransformPosition(World); }
	float LocalYaw(float WorldYaw) const { return WorldYaw - T.Rotator().Yaw; }

	/** A hung group (a cage on its chain, a cluster of charms) that sways about Pivot (local). */
	WRLookKit::FKit& Swing(const FVector& Pivot)
	{
		TUniquePtr<FGroup> Gp = MakeUnique<FGroup>();
		Gp->Comp = NewObject<USceneComponent>(Act);
		Gp->Comp->SetMobility(EComponentMobility::Movable);
		Gp->Comp->SetupAttachment(Act->GetRootComponent());
		Gp->Comp->SetRelativeLocation(Pivot);
		Gp->Comp->RegisterComponent();
		Gp->T = FTransform(Pivot) * T;
		Gp->Kit = MakeUnique<WRLookKit::FKit>(Gp->T, Gp->Blocks);
		Groups.Add(MoveTemp(Gp));
		return *Groups.Last()->Kit;
	}

	UBoxComponent* Block(const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator) const
	{
		UBoxComponent* B = NewObject<UBoxComponent>(Act);
		B->SetupAttachment(Act->GetRootComponent());
		B->SetBoxExtent(Half);
		B->SetRelativeLocation(Centre);
		B->SetRelativeRotation(Rot);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		return B;
	}
	/** A collision bar from P0 to P1 (a trunk, a leg, a beam). */
	void Bar(const FVector& P0, const FVector& P1, float Radius) const
	{
		const FVector D = P1 - P0;
		const float L = D.Size();
		if (L < 1.f) return;
		Block((P0 + P1) * 0.5f, FVector(Radius, Radius, L * 0.5f), FRotationMatrix::MakeFromZ(D / L).Rotator());
	}
	UPointLightComponent* Light(const FVector& At, const FLinearColor& Colour, float Intensity, float Radius, bool bNight) const
	{
		if (bServer) return nullptr;
		UPointLightComponent* L = NewObject<UPointLightComponent>(Act);
		L->SetupAttachment(Act->GetRootComponent());
		L->SetRelativeLocation(At);
		L->SetLightColor(Colour);
		L->SetIntensity(bNight ? 0.f : Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		if (bNight) { Act->NightLights.Add(L); Act->NightBase.Add(Intensity); }
		return L;
	}
	USceneComponent* Fx(FName Effect, const FVector& At, float Scale = 1.f, const FRotator& Rot = FRotator::ZeroRotator, USceneComponent* Parent = nullptr) const
	{
		if (bServer) return nullptr;
		USceneComponent* F = WRFx::Attach(Parent ? Parent : Act->GetRootComponent(), Effect, NAME_None, Scale);
		if (F) { F->SetRelativeLocation(Parent ? At - Parent->GetRelativeLocation() : At); F->SetRelativeRotation(Rot); }
		return F;
	}
	UStaticMeshComponent* Rock(int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, float Sink = 10.f) const
	{
		if (bServer && !bCollide) return nullptr;
		UStaticMesh* Mesh = RockMesh(Which);
		if (!Mesh) return nullptr;
		UStaticMeshComponent* Cmp = NewObject<UStaticMeshComponent>(Act);
		Cmp->SetStaticMesh(Mesh);
		Cmp->SetupAttachment(Act->GetRootComponent());
		const FBox B = Mesh->GetBoundingBox();
		const FVector S = B.GetSize();
		const FVector Scale(Size.X / FMath::Max(1.f, S.X), Size.Y / FMath::Max(1.f, S.Y), Size.Z / FMath::Max(1.f, S.Z));
		Cmp->SetRelativeScale3D(Scale);
		Cmp->SetRelativeRotation(Rot);
		Cmp->SetRelativeLocation(Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Sink)));
		if (Mat) Cmp->SetMaterial(0, Mat);
		Cmp->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Cmp->SetCanEverAffectNavigation(false);
		Cmp->SetCullDistance(WRTune(TEXT("swampWorld.rockCull"), 40000.f));
		Cmp->RegisterComponent();
		return Cmp;
	}

	// ------------------------------------------------------------------------------------------------ the kit

	/** A chain of iron links from P0 to P1, each link turned a quarter to the last. */
	static void Chain(FKit& Kt, const FVector& P0, const FVector& P1, float Link = 14.f)
	{
		const FVector D = P1 - P0;
		const float L = D.Size();
		if (L < 1.f) return;
		const int32 N = FMath::Clamp(FMath::RoundToInt(L / Link), 1, 400);
		const FRotator R = FRotationMatrix::MakeFromX(D / L).Rotator();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector M = P0 + D * ((i + 0.5f) / N);
			Kt.Add(IronS(), FVector(L / N * 1.15f, 2.2f, 6.f), M, FRotator(R.Pitch, R.Yaw, i % 2 ? 90.f : 0.f));
		}
	}

	/** A bottle charm: a cord from Hook, a stoppered bottle (green, brown or clear glass) at its end, a bone bead above it. */
	static void Bottle(FKit& Kt, const FVector& Hook, float Len, int32 Var)
	{
		static const FLinearColor Tints[3] = { FLinearColor(0.05f, 0.12f, 0.06f), FLinearColor(0.16f, 0.08f, 0.03f), FLinearColor(0.35f, 0.38f, 0.36f) };
		const FVector End = Hook - FVector(0, 0, Len);
		Kt.Rod(Cord(), Hook, End, 0.8f);
		Kt.Add(BoneS(), FVector(3.f, 3.f, 4.f), Hook - FVector(0, 0, Len * 0.4f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		Kt.Add(Glass(Tints[((Var % 3) + 3) % 3]), FVector(7.f, 7.f, 14.f), End - FVector(0, 0, 9.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		Kt.Add(Glass(Tints[((Var % 3) + 3) % 3]), FVector(3.f, 3.f, 5.f), End - FVector(0, 0, 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		Kt.Add(FSurf::Wood(C(100, 80, 60) * 0.4f), FVector(3.4f, 3.4f, 2.f), End + FVector(0, 0, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}

	/** A bone chime: a cross-stick with five finger bones hung on cords at different lengths. */
	static void Chime(FKit& Kt, const FVector& Hook)
	{
		Kt.Rod(Cord(), Hook, Hook - FVector(0, 0, 30.f), 0.8f);
		Kt.Rod(BoneS(), Hook - FVector(0, 18.f, 30.f), Hook - FVector(0, -18.f, 30.f), 2.f);
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector Top = Hook - FVector(0, -16.f + 8.f * i, 30.f);
			const float L = 10.f + 6.f * ((i * 7) % 3);
			Kt.Rod(Cord(), Top, Top - FVector(0, 0, L), 0.5f);
			Kt.Rod(BoneS(1.05f), Top - FVector(0, 0, L), Top - FVector(0, 0, L + 14.f), 2.f);
		}
	}

	/** A crow skull: braincase, the long black beak, dark sockets; facing +X of Yaw. */
	static void CrowSkull(FKit& Kt, const FVector& At, float Yaw, float S = 1.f)
	{
		const FRotator R(0.f, Yaw, 0.f);
		Kt.Add(BoneS(1.08f), FVector(9.f, 7.5f, 7.f) * S, At, R, EWRLookMesh::Cylinder);
		Kt.Spike(FSurf::Stone(FLinearColor(0.02f, 0.02f, 0.022f), 0.35f), At + R.Vector() * 4.f * S, FRotator(-8.f, Yaw, 0.f).Vector(), 4.5f * S, 13.f * S);
		for (const float Sd : { -1.f, 1.f }) Kt.Add(Dark(), FVector(2.f, 2.6f, 2.6f) * S, At + R.RotateVector(FVector(1.5f, Sd * 3.4f, 1.f)) * S, R, EWRLookMesh::Cylinder);
	}

	/** An iron cage on its chain: Hook is the chain's top, the cage's top ring Drop below it; with a skeleton in it if
	 *  asked (slumped, legs through the bars). Built in a sway group's frame (Hook at its origin). */
	static void Cage(FKit& Kt, const FVector& Hook, float Drop, float H, float R, bool bSkeleton, bool bOpen = false)
	{
		const FVector Top = Hook - FVector(0, 0, Drop);
		Chain(Kt, Hook, Top + FVector(0, 0, 12.f));
		Kt.Spike(IronS(), Top - FVector(0, 0, 4.f), FVector::UpVector, R * 1.6f, 22.f);
		Kt.Disc(IronS(), Top + FVector(0, 0, 16.f), FVector::UpVector, 8.f, 6.f);
		const float Bottom = Top.Z - H;
		for (int32 b = 0; b < 10; ++b)
		{
			if (bOpen && (b == 0 || b == 1)) continue;
			const FVector O = FRotator(0.f, 36.f * b, 0.f).Vector() * R;
			Kt.Rod(RustS(), FVector(Top.X + O.X, Top.Y + O.Y, Top.Z - 2.f), FVector(Top.X + O.X, Top.Y + O.Y, Bottom), 2.6f);
		}
		for (const float Z : { float(Top.Z), float(Top.Z - H * 0.5f), float(Bottom + 3.f) }) Kt.Disc(IronS(), FVector(Top.X, Top.Y, Z), FVector::UpVector, R * 2.f + 3.f, 3.f);
		Kt.Disc(IronS(), FVector(Top.X, Top.Y, Bottom), FVector::UpVector, R * 2.f, 2.f);
		if (bOpen) Kt.Add(RustS(), FVector(3.f, R * 0.9f, H * 0.9f), FVector(Top.X + R + 20.f, Top.Y + 20.f, Top.Z - H * 0.5f), FRotator(0.f, 60.f, 0.f));
		if (bSkeleton)
		{
			// Slumped against the bars, knees up, the skull dropped to the chest, one arm through the bars.
			const FVector P(Top.X - R * 0.3f, Top.Y, Bottom + 12.f);
			Kt.Add(BoneS(), FVector(20.f, 26.f, 12.f), P);
			const FVector Neck = P + FVector(-R * 0.25f, 0, 52.f);
			Kt.Rod(BoneS(), P + FVector(0, 0, 6.f), Neck, 4.f);
			for (int32 r = 0; r < 5; ++r) for (const float Sd : { -1.f, 1.f }) Kt.Rod(BoneS(0.95f), FMath::Lerp(P, Neck, 0.35f + 0.12f * r), FMath::Lerp(P, Neck, 0.3f + 0.12f * r) + FVector(10.f, Sd * 13.f, -4.f), 1.8f);
			Kt.Add(BoneS(1.05f), FVector(18.f, 15.f, 19.f), Neck + FVector(10.f, 3.f, 4.f), FRotator(-35.f, 10.f, 0.f), EWRLookMesh::Cylinder);
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Hip = P + FVector(4.f, Sd * 9.f, 0.f), Knee = Hip + FVector(26.f, Sd * 6.f, 30.f), Foot = Knee + FVector(18.f, Sd * 4.f, -38.f);
				Kt.Rod(BoneS(), Hip, Knee, 3.2f);
				Kt.Rod(BoneS(), Knee, Foot, 2.6f);
				const FVector Sh = Neck + FVector(0, Sd * 15.f, -8.f);
				const FVector El = Sd > 0.f ? Sh + FVector(14.f, 8.f, -24.f) : Sh + FVector(22.f, -R * 0.6f, -6.f);
				Kt.Rod(BoneS(), Sh, El, 2.6f);
				Kt.Rod(BoneS(), El, Sd > 0.f ? El + FVector(18.f, 0, -14.f) : El + FVector(10.f, -R * 0.7f, -16.f), 2.f);
			}
		}
	}

	/** A dead or living bald cypress of height H at Base (local), leaning Lean degrees toward LeanYaw. Living ones carry
	 *  flat feathery tiers of foliage hung with Spanish moss; dead ones stand grey with broken limbs. Knees ring the foot.
	 *  Returns the tips of its main limbs (where cages and charms hang). */
	TArray<FVector> Cypress(FKit& Kt, const FVector& Base, float H, float Lean, float LeanYaw, bool bDead, int32 Sd, bool bCollide = true)
	{
		TArray<FVector> Tips;
		const FVector Axis = FRotator(90.f - Lean, LeanYaw, 0.f).Vector();
		const FSurf BarkS = bDead ? FSurf::Wood(C(120, 116, 108) * 0.42f) : Bark();
		// The flared buttress: three shrinking drums and six fins, then the trunk tapering to the crown.
		const float D0 = FMath::Max(80.f, H * 0.075f);
		for (int32 i = 0; i < 3; ++i) Kt.Rod(BarkS.Dim(0.95f), Base + Axis * (i * H * 0.03f - 40.f), Base + Axis * ((i + 1) * H * 0.03f), D0 * (1.f - 0.22f * i));
		for (int32 f = 0; f < 6; ++f)
		{
			const float A = 60.f * f + 13.f * Sd;
			const FVector Out = FRotator(0.f, A, 0.f).Vector();
			Kt.Add(BarkS.Dim(0.9f), FVector(D0 * 0.9f, 14.f, H * 0.07f), Base + Out * D0 * 0.45f + FVector(0, 0, H * 0.03f), FRotator(-24.f, A, 0.f));
		}
		const int32 Segs = 6;
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A0 = 0.09f + 0.91f * s / Segs, A1 = 0.09f + 0.91f * (s + 1) / Segs;
			Kt.Rod(BarkS, Base + Axis * H * A0, Base + Axis * H * A1, FMath::Lerp(D0 * 0.5f, D0 * 0.12f, A1));
		}
		if (bCollide) Bar(Base + Axis * 40.f, Base + Axis * H * 0.6f, D0 * 0.3f);
		// Knees: woody cones standing out of the mud round the foot.
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = 51.f * k + 20.f * Sd;
			const float R = D0 * (0.9f + 0.6f * ((k * 5 + Sd) % 4));
			const FVector P = FVector(Base.X, Base.Y, 0.f) + FRotator(0.f, A, 0.f).Vector() * R;
			Kt.Spike(BarkS.Dim(0.85f), FVector(P.X, P.Y, G(P.X, P.Y) - 15.f), FVector::UpVector, 22.f + 10.f * (k % 3), 40.f + 25.f * ((k + Sd) % 3));
		}
		// Limbs: living ones reach out level with a feathery flat tier at the end; dead ones stab upward, snapped.
		const int32 Limbs = bDead ? 7 : 9;
		for (int32 l = 0; l < Limbs; ++l)
		{
			const float Hh = FMath::Lerp(0.42f, 0.93f, float(l) / Limbs);
			const float A = l * 137.5f + Sd * 31.f;
			const FVector From = Base + Axis * H * Hh;
			const float Len = H * (bDead ? 0.12f : 0.17f) * (1.f - 0.4f * Hh) + 120.f;
			const FVector Dir = FRotator(bDead ? 35.f + 15.f * (l % 3) : 4.f + 6.f * (l % 3), A, 0.f).Vector();
			const FVector Mid = From + Dir * Len * 0.55f + FVector(0, 0, bDead ? 0.f : -Len * 0.05f);
			const FVector Tip = From + Dir * Len;
			Kt.Rod(BarkS.Dim(0.95f), From, Mid, FMath::Max(12.f, D0 * 0.12f));
			Kt.Rod(BarkS.Dim(0.95f), Mid, Tip, FMath::Max(8.f, D0 * 0.07f));
			if (bDead)
			{
				// A forked, broken end.
				Kt.Rod(BarkS, Mid, Mid + FRotator(55.f, A + 40.f, 0.f).Vector() * Len * 0.3f, 7.f);
			}
			else
			{
				for (int32 p = 0; p < 3; ++p)
				{
					const FVector Pad = FMath::Lerp(Mid, Tip, 0.4f + 0.3f * p) + FVector(0, 0, 25.f);
					Kt.Disc(LeafS(0.85f + 0.1f * p), Pad, FRotator(90.f - 6.f, A, 0.f).Vector(), Len * (0.55f - 0.1f * p), 45.f);
				}
				// Spanish moss: grey-green beards hanging a metre or three under the limb.
				for (int32 m = 0; m < 5; ++m)
				{
					const FVector Hang = FMath::Lerp(From, Tip, 0.25f + 0.17f * m) - FVector(0, 0, 8.f);
					const float Lm = 90.f + 70.f * ((m * 3 + l + Sd) % 4);
					Kt.Add(MossS(), FVector(5.f, 22.f + 8.f * (m % 2), Lm), Hang - FVector(0, 0, Lm * 0.5f), FRotator(0.f, A + 90.f, 4.f * (m % 3 - 1)));
				}
			}
			Tips.Add(Tip);
		}
		if (!bDead) Kt.Disc(LeafS(0.8f), Base + Axis * H * 0.99f, FVector::UpVector, H * 0.22f, 70.f);   // the flat crown
		return Tips;
	}

	/** A Rodwarden or Coven skeleton. Pose 0 lying on its back, 1 sitting slumped, 2 leaning back against something at
	 *  +X (standing), 3 face down. bPits: leech-scar pits in the bone; bHook: a Coven bone hook through the ribs. */
	void Skeleton(FKit& Kt, const FVector& Pelvis, float Yaw, int32 Pose, bool bPits, bool bHook) const
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Pelvis + R.RotateVector(FVector(X, Y, Z)); };
		FVector Neck, Head, Hip[2], Knee[2], Foot[2], Sh[2], El[2], Hand[2];
		if (Pose == 0 || Pose == 3)
		{
			Neck = P(-55.f, 0, 4.f); Head = P(-74.f, 0, 8.f);
			for (int32 s = 0; s < 2; ++s) { const float Sd = s ? 1.f : -1.f; Hip[s] = P(0, Sd * 10.f, 0); Knee[s] = P(44.f, Sd * 14.f, 2.f); Foot[s] = P(86.f, Sd * 18.f, 0); Sh[s] = P(-50.f, Sd * 18.f, 2.f); El[s] = P(-24.f, Sd * 26.f, 0); Hand[s] = P(2.f, Sd * 30.f, 0); }
		}
		else if (Pose == 1)
		{
			Neck = P(-14.f, 0, 52.f); Head = P(-2.f, 0, 58.f);
			for (int32 s = 0; s < 2; ++s) { const float Sd = s ? 1.f : -1.f; Hip[s] = P(0, Sd * 10.f, 0); Knee[s] = P(36.f, Sd * 16.f, 30.f); Foot[s] = P(56.f, Sd * 18.f, -4.f); Sh[s] = P(-12.f, Sd * 17.f, 45.f); El[s] = P(4.f, Sd * 22.f, 22.f); Hand[s] = P(26.f, Sd * 20.f, 12.f); }
		}
		else
		{
			Neck = P(18.f, 0, 56.f); Head = P(30.f, 0, 62.f);
			for (int32 s = 0; s < 2; ++s) { const float Sd = s ? 1.f : -1.f; Hip[s] = P(0, Sd * 10.f, 0); Knee[s] = P(-10.f, Sd * 12.f, -44.f); Foot[s] = P(-26.f, Sd * 14.f, -88.f); Sh[s] = P(16.f, Sd * 17.f, 48.f); El[s] = P(34.f, Sd * 24.f, 26.f); Hand[s] = P(36.f, Sd * 20.f, 4.f); }
		}
		const FSurf B = BoneS();
		Kt.Add(B, FVector(18.f, 28.f, 12.f), Pelvis, R);
		Kt.Rod(B, Pelvis, Neck, 4.f);
		for (int32 r = 0; r < 6; ++r)
		{
			const FVector S = FMath::Lerp(Pelvis, Neck, 0.4f + 0.1f * r);
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Out = R.RotateVector(FVector(0, Sd * 14.f, 0)) + (Pose == 0 ? FVector(0, 0, 12.f) : Pose == 3 ? FVector(0, 0, -6.f) : R.Vector() * 10.f);
				Kt.Rod(B.Dim(0.95f), S, S + Out, 1.8f);
				Kt.Rod(B.Dim(0.95f), S + Out, S + Out * 0.4f + (Pose == 0 ? FVector(0, 0, 10.f) : R.Vector() * 14.f), 1.6f);
			}
		}
		Kt.Add(BoneS(1.08f), FVector(19.f, 15.f, 20.f), Head, FRotator(Pose == 0 ? 80.f : Pose == 3 ? -80.f : 0.f, Yaw, 0.f), EWRLookMesh::Cylinder);
		Kt.Add(B, FVector(12.f, 12.f, 5.f), Head + R.RotateVector(FVector(6.f, 0, Pose == 0 ? 8.f : -9.f)), R);
		for (int32 s = 0; s < 2; ++s)
		{
			Kt.Rod(B, Hip[s], Knee[s], 3.2f); Kt.Rod(B, Knee[s], Foot[s], 2.6f);
			Kt.Rod(B, Sh[s], El[s], 2.6f); Kt.Rod(B, El[s], Hand[s], 2.f);
			Kt.Rod(B, Neck, Sh[s], 2.f);
		}
		if (bPits)
		{
			// Leech scars: round pits eaten into the long bones and the skull.
			for (int32 i = 0; i < 9; ++i)
			{
				const FVector On = i < 4 ? FMath::Lerp(Hip[i % 2], Knee[i % 2], 0.3f + 0.15f * i) : i < 7 ? FMath::Lerp(Sh[i % 2], El[i % 2], 0.5f) : Head + FVector(0, 0, 6.f);
				Kt.Add(FSurf::Stone(BoneOld * 0.18f, 0.9f), FVector(3.f, 3.f, 3.4f), On + FVector(0, 0, 2.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}
		if (bHook)
		{
			// A Coven bone hook through the ribs, its cord trailing.
			const FVector Hk = FMath::Lerp(Pelvis, Neck, 0.6f);
			Kt.Rod(OldBone(), Hk - R.Vector() * 16.f + FVector(0, 0, -10.f), Hk + R.Vector() * 12.f + FVector(0, 0, 14.f), 3.f);
			Kt.Spike(OldBone(), Hk + R.Vector() * 12.f + FVector(0, 0, 14.f), FRotator(-120.f, Yaw, 0.f).Vector(), 4.f, 16.f);
			Kt.Rod(Cord(), Hk - R.Vector() * 16.f + FVector(0, 0, -10.f), Hk - R.Vector() * 50.f + FVector(0, 30.f, -30.f), 1.f);
		}
	}

	/** A Coven fetish post: a pole of lashed long bones, a crossbar, the crow skull on top facing the Throat (FaceYaw,
	 *  local), a bottle charm (and for some, more skulls and a bone chime) hung from the crossbar. Var 0..2. */
	void FetishPost(const FVector& Base, float FaceYaw, int32 Var)
	{
		const float H = 260.f + 40.f * Var;
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector A0 = Base + FVector(0, 0, -50.f + s * (H + 50.f) / 3.f), A1 = Base + FVector(0, 0, -50.f + (s + 1) * (H + 50.f) / 3.f);
			K.Rod(BoneS(0.95f + 0.05f * s), A0, A1, 9.f - s);
			K.Add(BoneS(1.1f), FVector(14.f - s, 14.f - s, 8.f), A1, FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // the knuckled ends
			for (int32 w = 0; w < 4; ++w) K.Disc(Cord(), A1 - FVector(0, 0, 10.f + w * 3.f), FVector::UpVector, 13.f - s, 2.5f);
		}
		const FVector Bar = Base + FVector(0, 0, H * 0.82f);
		const FVector Side = FRotator(0.f, FaceYaw + 90.f, 0.f).Vector();
		K.Rod(BoneS(0.9f), Bar - Side * 50.f, Bar + Side * 50.f, 5.f);
		CrowSkull(K, Base + FVector(0, 0, H + 6.f), FaceYaw, 1.3f);
		WRLookKit::FKit& Hang = Swing(Bar + Side * 42.f);
		Bottle(Hang, FVector::ZeroVector, 40.f + 10.f * Var, Var);
		if (Var >= 1)
		{
			CrowSkull(K, Bar - Side * 30.f + FVector(0, 0, 8.f), FaceYaw, 1.f);
			CrowSkull(K, Bar + FVector(0, 0, 8.f), FaceYaw, 1.f);
			WRLookKit::FKit& Hang2 = Swing(Bar - Side * 44.f);
			Bottle(Hang2, FVector::ZeroVector, 55.f, Var + 1);
		}
		if (Var == 2)
		{
			WRLookKit::FKit& Ch = Swing(Bar);
			Chime(Ch, FVector::ZeroVector);
			for (int32 a = 0; a < 3; ++a) K.Rod(OldBone(), Base + FVector(0, 0, H * 0.95f), Base + FVector(0, 0, H * 0.95f) + FRotator(40.f, FaceYaw + 120.f * a, 0.f).Vector() * 50.f, 3.f);
		}
	}

	/** Reeds: N stems in a clump of radius R round Centre, a cattail head on some. */
	void Reeds(const FVector& Centre, float R, int32 N, float Hgt = 200.f, bool bOnWater = true)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float A = i * 2.39996f + Seed;
			const float D = R * FMath::Sqrt((i + 0.5f) / N);
			const FVector2D P(Centre.X + FMath::Cos(A) * D, Centre.Y + FMath::Sin(A) * D);
			const FVector Foot = bOnWater ? Top(P.X, P.Y, -20.f) : At(P.X, P.Y, -10.f);
			const float Hh = Hgt * (0.7f + 0.45f * Rand(i));
			const FVector Tip = Foot + FRotator(80.f + 10.f * Rand(i + 3), A * 57.f, 0.f).Vector() * Hh;
			K.Rod(ReedS(0.85f + 0.3f * Rand(i + 7)), Foot, Tip, 1.6f + 1.2f * Rand(i + 9));
			if (i % 4 == 0) K.Add(FSurf::Cloth(C(70, 50, 34) * 0.5f), FVector(4.f, 4.f, 20.f), Tip - (Tip - Foot).GetSafeNormal() * 18.f, FRotationMatrix::MakeFromZ((Tip - Foot).GetSafeNormal()).Rotator(), EWRLookMesh::Cylinder);
		}
	}

	/** A stilt hut: four black-cypress stilts sunk in the mud, a plank platform 2.2 m up, plank walls with a door at +X,
	 *  a steep reed-thatch roof, a ladder. Var 0 shut (hex mark from outside if bHex), 1 looted (door off, roof holed),
	 *  2 a Lanterntoad nest under it (a wet moss mound, glowing). */
	void StiltHut(const FVector& Base, float Yaw, int32 Var, bool bHex)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const float W = 360.f, D = 320.f, Lift = 220.f, Wall = 230.f;
		const float Floor = FMath::Max(G(Base.X, Base.Y), WaterZ) + Lift - Base.Z;
		for (const float X : { -D * 0.5f, D * 0.5f }) for (const float Y : { -W * 0.5f, W * 0.5f })
		{
			const FVector F = P(X, Y, 0.f);
			K.Rod(Black(), FVector(F.X, F.Y, G(F.X, F.Y) - 100.f), P(X, Y, Floor), 22.f);
			Bar(FVector(F.X, F.Y, G(F.X, F.Y)), P(X, Y, Floor), 12.f);
		}
		for (const float Y : { -W * 0.5f, W * 0.5f }) K.Rod(Black(0.9f), P(-D * 0.5f, Y, Floor * 0.45f), P(D * 0.5f, -Y, Floor * 0.45f), 8.f);   // cross braces
		for (int32 b = 0; b < 14; ++b) K.Add(Planks(0.9f + 0.2f * Rand(b)), FVector(D + 120.f, W / 14.f - 2.f, 5.f), P(60.f, -W * 0.5f + W * (b + 0.5f) / 14.f, Floor), R);
		Block(P(60.f, 0, Floor - 3.f), FVector((D + 120.f) * 0.5f, W * 0.5f, 6.f), R);
		// Walls: vertical planks, the door gap at +X.
		const FSurf Wd = Planks(0.75f);
		const auto WallRun = [&](const FVector& A0, const FVector& A1, int32 Gap0, int32 Gap1)
		{
			const int32 N = 12;
			for (int32 i = 0; i < N; ++i)
			{
				if (i >= Gap0 && i < Gap1) continue;
				K.Add(Wd.Dim(0.85f + 0.3f * Rand(i + 20)), FVector(4.f, (A1 - A0).Size() / N - 1.f, Wall), FMath::Lerp(A0, A1, (i + 0.5f) / N) + FVector(0, 0, Wall * 0.5f), FRotator(0.f, (A1 - A0).Rotation().Yaw + 90.f, 0.f));
			}
		};
		WallRun(P(-D * 0.5f, -W * 0.5f, Floor), P(-D * 0.5f, W * 0.5f, Floor), -1, -1);
		WallRun(P(-D * 0.5f, -W * 0.5f, Floor), P(D * 0.5f, -W * 0.5f, Floor), 4, Var == 1 ? 7 : 4);
		WallRun(P(-D * 0.5f, W * 0.5f, Floor), P(D * 0.5f, W * 0.5f, Floor), -1, -1);
		WallRun(P(D * 0.5f, -W * 0.5f, Floor), P(D * 0.5f, W * 0.5f, Floor), 5, 8);
		for (const FVector& Wc : { P(-D * 0.5f, 0, Floor + Wall * 0.5f), P(0, -W * 0.5f, Floor + Wall * 0.5f), P(0, W * 0.5f, Floor + Wall * 0.5f) })
		{
			Block(Wc, FVector(8.f, W * 0.5f, Wall * 0.5f), FRotator(0.f, Yaw + ((Wc - P(0, 0, Floor + Wall * 0.5f)).GetSafeNormal2D().Y != 0.f && FMath::Abs((Wc - P(0, 0, Floor + Wall * 0.5f)).GetSafeNormal2D().X) < 0.5f ? 90.f : 0.f), 0.f));
		}
		// The door: shut and barred from outside, hanging off, or gone.
		const FVector Door = P(D * 0.5f + 3.f, 0, Floor + 100.f);
		if (Var == 0)
		{
			K.Add(Wd.Dim(0.9f), FVector(5.f, 88.f, 196.f), Door, R);
			for (const float Y : { -56.f, 56.f }) K.Add(IronS(), FVector(8.f, 6.f, 16.f), P(D * 0.5f + 10.f, Y, Floor + 110.f), R);
			K.Add(Planks(1.1f), FVector(8.f, 140.f, 12.f), P(D * 0.5f + 16.f, 0, Floor + 112.f), R);   // the bar across, outside
			if (bHex)
			{
				// The hex mark daubed on the OUTSIDE face in dried blood: a ring crossed twice, three strokes under it.
				const FSurf Daub = FSurf::Stone(Oxblood * 0.55f, 0.7f);
				for (int32 r = 0; r < 10; ++r) K.Add(Daub, FVector(1.f, 7.f, 2.6f), Door + R.RotateVector(FVector(3.5f, FMath::Cos(r * 0.628f) * 24.f, 30.f + FMath::Sin(r * 0.628f) * 24.f)), FRotator(Yaw, 0.f, 0.f) + FRotator(0.f, 0.f, r * 36.f + 90.f));
				for (const float Rl : { 45.f, -45.f }) K.Add(Daub, FVector(1.f, 3.f, 64.f), Door + R.RotateVector(FVector(3.6f, 0, 30.f)), FRotator(0.f, Yaw, Rl));
				for (int32 s = 0; s < 3; ++s) K.Add(Daub, FVector(1.f, 3.f, 22.f), Door + R.RotateVector(FVector(3.6f, -14.f + 14.f * s, -22.f)), FRotator(0.f, Yaw, 0.f));
			}
		}
		else if (Var == 1)
		{
			K.Add(Wd.Dim(0.8f), FVector(5.f, 88.f, 196.f), P(D * 0.5f + 60.f, 70.f, Floor + 60.f), FRotator(70.f, Yaw + 30.f, 0.f));
		}
		// The roof: two steep slopes of reed thatch in courses over cypress rafters; a hole torn in a looted one.
		const float Ridge = Floor + Wall + 170.f;
		for (int32 r = 0; r < 5; ++r) for (const float Sd : { -1.f, 1.f })
		{
			const FVector Lo = P(-D * 0.5f - 40.f + r * (D + 80.f) / 4.f, Sd * (W * 0.5f + 50.f), Floor + Wall - 10.f), Hi = P(-D * 0.5f - 40.f + r * (D + 80.f) / 4.f, 0, Ridge);
			K.Rod(Black(), Lo, Hi, 9.f);
		}
		for (int32 c = 0; c < 6; ++c) for (const float Sd : { -1.f, 1.f })
		{
			if (Var == 1 && Sd > 0.f && (c == 2 || c == 3)) continue;
			const float T0 = c / 6.f;
			const FVector Lo = P(0, Sd * (W * 0.5f + 60.f) * (1.f - T0), Floor + Wall - 14.f + (Ridge - Floor - Wall + 14.f) * T0);
			K.Add(ReedS(0.85f + 0.08f * c), FVector(D + 110.f, 52.f, 18.f), Lo + FVector(0, 0, 12.f), FRotator(0.f, Yaw, Sd * -54.f));
		}
		K.Rod(ReedS(0.7f), P(-D * 0.5f - 55.f, 0, Ridge + 8.f), P(D * 0.5f + 55.f, 0, Ridge + 8.f), 22.f);
		Block(P(0, 0, (Floor + Wall + Ridge) * 0.5f), FVector(D * 0.5f + 40.f, W * 0.5f + 40.f, (Ridge - Floor - Wall) * 0.5f), R);
		// The ladder down from the platform's front edge.
		const FVector LTop = P(D * 0.5f + 115.f, 90.f, Floor), LFoot = P(D * 0.5f + 175.f, 90.f, 0.f);
		const FVector LF(LFoot.X, LFoot.Y, G(LFoot.X, LFoot.Y));
		for (const float Y : { -24.f, 24.f }) K.Rod(Black(1.1f), LF + R.RotateVector(FVector(0, Y, -20.f)), LTop + R.RotateVector(FVector(0, Y, 30.f)), 6.f);
		for (int32 g = 1; g < 8; ++g) K.Rod(Planks(1.2f), FMath::Lerp(LF, LTop, g / 8.f) + R.RotateVector(FVector(0, -24.f, 0)), FMath::Lerp(LF, LTop, g / 8.f) + R.RotateVector(FVector(0, 24.f, 0)), 4.f);
		if (Var == 2)
		{
			// A Lanterntoad nest: a wet moss mound in the dark under the floor, spawn strings glistening, the sacs glowing.
			const FVector N = At(Base.X, Base.Y, 0.f);
			K.Disc(MossS(), N + FVector(0, 0, 18.f), FVector::UpVector, 220.f, 36.f);
			K.Disc(FSurf::Glaze(C(40, 52, 30) * 0.5f, 0.2f), N + FVector(0, 0, 38.f), FVector::UpVector, 150.f, 8.f);
			for (int32 t = 0; t < 6; ++t)
			{
				const FVector Sac = N + FRotator(0.f, 60.f * t, 0.f).Vector() * 70.f + FVector(0, 0, 44.f);
				K.Add(FSurf::Lit(C(196, 210, 70) * 0.6f, 0.9f), FVector(9.f, 9.f, 7.f), Sac, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				if (!bServer && t % 2 == 0)
				{
					UPointLightComponent* L = Light(Sac + FVector(0, 0, 20.f), FLinearColor(0.75f, 0.85f, 0.25f), 0.f, 500.f, false);
					if (L) { Act->PulseLights.Add(L); Act->PulseBase.Add(WRTune(TEXT("swampWorld.toadLight"), 700.f)); }
				}
			}
		}
	}

	/** A bog body laid on its back: leathered skin, the cord braided three-strand round the neck and trailing, the face
	 *  calm (closed eyes, a still mouth). Var turns the arms and the cap. */
	void BogBody(const FVector& Base, float Yaw, int32 Var)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf Skin = LeatherS(0.9f + 0.1f * (Var % 3));
		K.Add(Skin, FVector(58.f, 34.f, 16.f), P(0, 0, 8.f), R);                                               // chest
		K.Add(Skin, FVector(36.f, 30.f, 14.f), P(40.f, 0, 7.f), R);                                            // belly, sunken
		K.Add(Skin.Dim(0.95f), FVector(22.f, 17.f, 19.f), P(-46.f, 0, 10.f), FRotator(80.f, Yaw, 0.f), EWRLookMesh::Cylinder);   // head
		for (const float Sd : { -1.f, 1.f }) K.Add(Dark(), FVector(1.f, 4.f, 0.8f), P(-48.f, Sd * 4.f, 19.f), R);  // closed eyes
		K.Add(Dark(), FVector(1.f, 5.f, 0.6f), P(-40.f, 0, 18.f), R);                                          // the still mouth
		if (Var % 2) K.Add(LeatherS(0.6f), FVector(22.f, 20.f, 6.f), P(-52.f, 0, 16.f), R);                    // a skin cap
		for (const float Sd : { -1.f, 1.f })
		{
			K.Rod(Skin, P(58.f, Sd * 9.f, 6.f), P(120.f, Sd * 12.f, 5.f), 10.f);
			K.Rod(Skin, P(120.f, Sd * 12.f, 5.f), P(168.f, Sd * 11.f, 6.f), 8.f);
			const FVector Sh = P(-18.f, Sd * 20.f, 9.f), El = Var % 2 ? P(10.f, Sd * 24.f, 7.f) : P(10.f, Sd * 12.f, 16.f);
			K.Rod(Skin, Sh, El, 7.f);
			K.Rod(Skin, El, Var % 2 ? P(40.f, Sd * 22.f, 6.f) : P(26.f, -Sd * 4.f, 18.f), 6.f);
		}
		// The braided cord: three strands twisted round the neck, tied, its end trailing over the shoulder.
		for (int32 s = 0; s < 3; ++s)
		{
			for (int32 i = 0; i < 10; ++i)
			{
				const float A0 = i * 36.f + s * 120.f, A1 = (i + 1) * 36.f + s * 120.f;
				const FVector N0 = P(-30.f, FMath::Cos(FMath::DegreesToRadians(A0)) * 9.f, 10.f + FMath::Sin(FMath::DegreesToRadians(A0)) * 9.f);
				const FVector N1 = P(-30.f, FMath::Cos(FMath::DegreesToRadians(A1)) * 9.f, 10.f + FMath::Sin(FMath::DegreesToRadians(A1)) * 9.f);
				K.Rod(Cord(), N0 + R.RotateVector(FVector(s * 1.2f - 1.2f, 0, 0)), N1 + R.RotateVector(FVector(s * 1.2f - 1.2f, 0, 0)), 1.4f);
			}
			K.Rod(Cord(), P(-30.f, 9.f, 10.f), P(-6.f + s * 4.f, 26.f + s * 2.f, 2.f), 1.3f);
		}
	}

	/** A flat-bottomed punt (5 m), Sink (cm) of its stern under, its pole lying across. */
	void Punt(const FVector& Base, float Yaw, float Tilt, float Sink, bool bLocker)
	{
		const FRotator R(Tilt, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)) - FVector(0, 0, Sink * (0.5f - X / 500.f)); };
		const FSurf Wd = Planks(0.7f);
		for (int32 b = 0; b < 6; ++b) K.Add(Wd.Dim(0.9f + 0.2f * Rand(b)), FVector(500.f, 20.f, 5.f), P(0, -50.f + b * 20.f, 0), R);
		for (const float Sd : { -1.f, 1.f }) K.Add(Wd, FVector(500.f, 5.f, 40.f), P(0, Sd * 62.f, 20.f), R);
		for (const float X : { -250.f, 250.f }) K.Add(Wd, FVector(5.f, 128.f, 34.f), P(X + (X > 0 ? 10.f : -10.f), 0, 22.f), FRotator(Tilt + (X > 0 ? -30.f : 30.f), Yaw, 0.f));
		for (int32 r = 0; r < 4; ++r) K.Add(Wd.Dim(0.8f), FVector(6.f, 124.f, 6.f), P(-180.f + r * 120.f, 0, 4.f), R);
		K.Add(Wd.Dim(1.1f), FVector(40.f, 124.f, 4.f), P(150.f, 0, 30.f), R);   // the thwart
		K.Rod(FSurf::Wood(Plank * 0.6f), P(-200.f, -40.f, 42.f), P(260.f, 30.f, 38.f), 5.f);
		if (bLocker)
		{
			K.Add(Wd.Dim(0.6f), FVector(70.f, 100.f, 36.f), P(-190.f, 0, 22.f), R);
			Act->CacheSpot = P(-130.f, 0, 30.f);
		}
	}

	/** A mound of Size R x Hgt (packed bone and peat), stepped discs with bones and skulls set in its face. */
	void Mound(const FVector& Base, float R, float Hgt, const FSurf& Body, int32 Bones)
	{
		const int32 Layers = FMath::Clamp(FMath::RoundToInt(Hgt / 40.f), 2, 14);
		for (int32 l = 0; l < Layers; ++l)
		{
			const float Z = Hgt * l / Layers;
			const float Rl = R * FMath::Sqrt(FMath::Max(0.05f, 1.f - FMath::Square(Z / Hgt)));
			K.Disc(Body.Dim(0.9f + 0.15f * Rand(l)), Base + FVector(0, 0, Z + Hgt / Layers * 0.5f - 10.f), FVector::UpVector, Rl * 2.f, Hgt / Layers + 6.f);
		}
		for (int32 b = 0; b < Bones; ++b)
		{
			const float A = b * 2.39996f + Seed, Hh = Rand(b + 40);
			const float Z = Hgt * Hh * 0.95f;
			const float Rl = R * FMath::Sqrt(FMath::Max(0.05f, 1.f - FMath::Square(Z / Hgt))) * 0.98f;
			const FVector On = Base + FVector(FMath::Cos(A) * Rl, FMath::Sin(A) * Rl, Z);
			if (b % 5 == 0) K.Add(BoneS(1.05f), FVector(20.f, 16.f, 18.f), On, FRotator(0.f, A * 57.f, 0.f), EWRLookMesh::Cylinder);
			else K.Rod(BoneS(0.9f + 0.15f * Rand(b)), On, On + FRotator(-10.f + 30.f * Rand(b + 3), A * 57.f + 90.f, 0.f).Vector() * (30.f + 25.f * Rand(b + 5)), 3.f + 1.5f * Rand(b + 7));
		}
		Block(Base + FVector(0, 0, Hgt * 0.4f), FVector(R * 0.65f, R * 0.65f, Hgt * 0.4f));
	}

	/** Bones scattered on the ground round Centre: long bones, ribs, now and then a skull. */
	void ScatterBones(const FVector& Centre, float R, int32 N, int32 Salt)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float A = i * 2.39996f + Salt;
			const float D = R * FMath::Sqrt((i + 0.5f) / N);
			const FVector P = At(Centre.X + FMath::Cos(A) * D, Centre.Y + FMath::Sin(A) * D, 2.f);
			if (i % 7 == 0) K.Add(BoneS(), FVector(19.f, 15.f, 16.f), P + FVector(0, 0, 6.f), FRotator(0.f, A * 50.f, 20.f), EWRLookMesh::Cylinder);
			else K.Rod(BoneS(0.85f + 0.2f * Rand(i + Salt)), P, P + FRotator(4.f, A * 70.f, 0.f).Vector() * (25.f + 30.f * Rand(i + 2 * Salt)), 2.5f + 2.f * Rand(i + 3));
		}
	}

	/** A Murkjaw-size beast's skull: long jaw, tusks or teeth (Mudhulk: tusks, heavy brow). */
	void BeastSkull(const FVector& Base, float Yaw, float S, bool bTusks)
	{
		const FRotator R(0.f, Yaw, 0.f);
		K.Add(BoneS(), FVector(120.f, 90.f, 70.f) * S, Base + FVector(0, 0, 30.f * S), R);
		K.Add(BoneS(0.95f), FVector(110.f, 70.f, 30.f) * S, Base + R.RotateVector(FVector(90.f, 0, 8.f)) * 1.f, FRotator(-8.f, Yaw, 0.f));
		for (const float Sd : { -1.f, 1.f })
		{
			K.Add(Dark(), FVector(20.f, 22.f, 22.f) * S, Base + R.RotateVector(FVector(30.f * S, Sd * 40.f * S, 46.f * S)), R, EWRLookMesh::Cylinder);
			if (bTusks) K.Spike(BoneS(1.1f), Base + R.RotateVector(FVector(110.f * S, Sd * 30.f * S, 10.f * S)), FRotator(40.f, Yaw + Sd * 25.f, 0.f).Vector(), 16.f * S, 80.f * S);
			else for (int32 t = 0; t < 6; ++t) K.Spike(BoneS(1.1f), Base + R.RotateVector(FVector((50.f + 18.f * t) * S, Sd * 26.f * S, 0.f)), FVector(0, 0, -1), 6.f * S, 16.f * S);
		}
	}

	/** Pitcher plants: a cluster of red-veined green tubes with hooded lids. */
	void Pitchers(const FVector& At0, int32 N)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const FVector P = At(At0.X + FMath::Cos(i * 2.1f) * 30.f * (i % 3), At0.Y + FMath::Sin(i * 2.1f) * 30.f * (i % 3), -4.f);
			const float H = 45.f + 25.f * Rand(i + 60);
			const FRotator Tilt(80.f - 15.f * Rand(i), i * 77.f, 0.f);
			K.Add(FSurf::Cloth(FMath::Lerp(C(96, 120, 50), Garnet, 0.35f + 0.3f * Rand(i)) * 0.6f), FVector(9.f, 9.f, H), P + Tilt.Vector() * H * 0.5f, FRotationMatrix::MakeFromZ(Tilt.Vector()).Rotator(), EWRLookMesh::Cylinder);
			K.Add(FSurf::Cloth(Garnet * 0.5f), FVector(14.f, 12.f, 2.f), P + Tilt.Vector() * (H + 4.f), FRotator(20.f, i * 77.f, 0.f), EWRLookMesh::Cylinder);
		}
	}

	/** A live red mangrove: trunk on a knot of prop roots arching out from it into the mud. */
	void Mangrove(const FVector& Base, float H, int32 Sd)
	{
		const FVector Knot = Base + FVector(0, 0, H * 0.25f);
		K.Rod(RootS(), Knot, Base + FVector(0, 0, H * 0.8f), FMath::Max(18.f, H * 0.04f));
		for (int32 r = 0; r < 8; ++r)
		{
			const float A = 45.f * r + Sd * 17.f;
			const FVector Foot = At(Base.X + FMath::Cos(FMath::DegreesToRadians(A)) * H * 0.35f, Base.Y + FMath::Sin(FMath::DegreesToRadians(A)) * H * 0.35f, -30.f);
			FVector Prev = Knot - FVector(0, 0, H * 0.05f * (r % 3));
			for (int32 s = 1; s <= 5; ++s)
			{
				const float T0 = s / 5.f;
				const FVector Pt = FMath::Lerp(Prev, Foot, 0.f) * 0.f + FMath::Lerp(Knot, Foot, T0) + FVector(0, 0, FMath::Sin(T0 * PI) * H * 0.12f);
				K.Rod(RootS(0.9f + 0.2f * Rand(r)), Prev, Pt, FMath::Lerp(14.f, 8.f, T0));
				Prev = Pt;
			}
		}
		for (int32 c = 0; c < 4; ++c) K.Disc(LeafS(0.9f), Base + FVector(FMath::Cos(c * 1.57f) * H * 0.18f, FMath::Sin(c * 1.57f) * H * 0.18f, H * (0.75f + 0.06f * c)), FVector::UpVector, H * 0.42f, H * 0.12f);
	}

	/** A turf stack: cut peat bricks laid in crossed courses to dry. */
	void TurfStack(const FVector& Base, float Yaw, int32 Courses)
	{
		for (int32 c = 0; c < Courses; ++c)
		{
			const float Y0 = (c % 2) ? 90.f : 0.f;
			for (int32 b = 0; b < 5 - c / 2; ++b)
			{
				const FRotator R(0.f, Yaw + Y0, 0.f);
				K.Add(PeatS(0.85f + 0.3f * Rand(b + c * 7)), FVector(36.f, 16.f, 12.f), Base + R.RotateVector(FVector(0, -40.f + b * 20.f + 10.f * (c / 2), 6.f + c * 12.f)), R);
			}
		}
	}

	/** A Rodwarden rod: a verdigris copper shaft, a cast collar, the tip; Bend folds the top half down (degrees). */
	void Rod(const FVector& Base, float H, float Lean, float LeanYaw, float Bend)
	{
		const FVector Up = FRotator(90.f - Lean, LeanYaw, 0.f).Vector();
		const FVector Mid = Base + Up * H * 0.5f;
		K.Rod(GreenCopper(), Base - Up * 120.f, Mid, 10.f);
		K.Disc(FSurf::Steel(C(140, 106, 58) * 0.5f, 0.5f), Base + Up * 14.f, Up, 26.f, 16.f);
		const FVector Up2 = Bend > 0.f ? FRotator(90.f - Lean - Bend, LeanYaw, 0.f).Vector() : Up;
		const FVector End = Mid + Up2 * H * 0.5f;
		K.Rod(GreenCopper(), Mid, End, 7.f);
		K.Spike(FSurf::Steel(C(184, 115, 51) * 0.5f, 0.45f), End, Up2, 7.f, 40.f);
		Bar(Base, Mid, 8.f);
	}
};

// ================================================================================================ AWRSwampScenery

AWRSwampScenery::AWRSwampScenery()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampScenery::SetTideLook(bool bTide)
{
	if (bTideLook == bTide) return;
	bTideLook = bTide;
	if (FallsNormal) FallsNormal->SetVisibility(!bTide, true);
	if (FallsTide) FallsTide->SetVisibility(bTide, true);
	if (MistTide) MistTide->SetVisibility(bTide, true);
	if (MistGlow) MistGlow->SetIntensity(WRTune(TEXT("swampWorld.mistGlow"), 60000.f) * (bTide ? 2.f : 1.f));
}

namespace WRSwampSceneryDetail
{
	/** A number from the site row, or Fallback. */
	float RowNum(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field, float Fallback)
	{
		double V = Fallback;
		if (Row.IsValid()) Row->TryGetNumberField(Field, V);
		return float(V);
	}

	/** A radius out of an arena's rings entry: a number, or the largest of an array. */
	float RingRadius(const TSharedPtr<FJsonObject>& Rings, const TCHAR* Field, float Fallback)
	{
		if (!Rings.IsValid()) return Fallback;
		double V = 0.0;
		if (Rings->TryGetNumberField(Field, V)) return float(V) * 100.f;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (Rings->TryGetArrayField(Field, A) && A->Num() > 0)
		{
			double M = 0.0;
			for (const TSharedPtr<FJsonValue>& E : *A) M = FMath::Max(M, E->AsNumber());
			return float(M) * 100.f;
		}
		return Fallback;
	}

	/** An arena's centre (x, y, z fields in metres), or zero. */
	FVector ArenaCentre(const TSharedPtr<FJsonObject>& Ar)
	{
		if (!Ar.IsValid()) return FVector::ZeroVector;
		double X = 0, Y = 0, Z = 0;
		Ar->TryGetNumberField(TEXT("x"), X);
		Ar->TryGetNumberField(TEXT("y"), Y);
		Ar->TryGetNumberField(TEXT("z"), Z);
		return FVector(X, Y, Z) * 100.0;
	}

	bool IsLandmark(FName K)
	{
		static const FName L[] = { TEXT("weeping_wall"), TEXT("mothers_throat"), TEXT("bled_roc"), TEXT("walking_house"), TEXT("hanging_cypress"), TEXT("mangrove_cathedral"), TEXT("corpse_mere"), TEXT("falls") };
		for (const FName& N : L) if (N == K) return true;
		return false;
	}

	bool IsKit(FName K)
	{
		static const FName L[] = { TEXT("fetish_post"), TEXT("stilt_hut"), TEXT("bloodmoss_hummock"), TEXT("bog_iron_shallows"), TEXT("cage_gallows"), TEXT("egg_reeds"), TEXT("kill_scene"),
			TEXT("sinkhole"), TEXT("peat_stack"), TEXT("reedfowl_nest"), TEXT("broken_punt"), TEXT("toad_ring") };
		for (const FName& N : L) if (N == K) return true;
		return false;
	}
}

void AWRSwampScenery::Build()
{
	using namespace WRSwampSceneryDetail;
	FWRSwampBuild B(this);
	float Cull = WRTune(TEXT("swampWorld.siteCull"), 60000.f);
	bUnder = Kind == TEXT("coven_room") || Kind == TEXT("coven_lane");
	if (Kind == TEXT("coven_room")) { BuildCovenRoom(B); Cull = 0.f; }
	else if (Kind == TEXT("coven_lane")) { BuildCovenLane(B); Cull = 0.f; }
	else if (Kind == TEXT("candle_path")) { BuildCandlePath(B); Cull = WRTune(TEXT("swampWorld.kitCull"), 35000.f); }
	else if (Kind == TEXT("fall_ledge")) { BuildFallLedge(B); Cull = WRTune(TEXT("swampWorld.siteCull"), 60000.f); }
	else if (Kind == TEXT("falls")) { BuildFalls(B); Cull = 0.f; }
	else if (IsLandmark(Kind)) { BuildLandmark(B); Cull = 0.f; }
	else if (Kind.ToString().StartsWith(TEXT("route_"))) { BuildRouteEdge(B); Cull = WRTune(TEXT("swampWorld.routeCull"), 30000.f); }
	else if (IsKit(Kind)) { BuildKit(B); Cull = WRTune(TEXT("swampWorld.kitCull"), 35000.f); }
	else BuildSite(B);
	if (B.bServer) return;   // the server keeps the collision only
	FWRBuildLook::Spawn(this, RootComponent, B.Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
	for (TUniquePtr<FWRSwampBuild::FGroup>& Gp : B.Groups)
	{
		FWRBuildLook::Spawn(this, Gp->Comp, Gp->Blocks, FLinearColor(0.2f, 0.18f, 0.15f), false);
		Swingers.Add(Gp->Comp);
	}
	if (Cull > 0.f)
	{
		for (UActorComponent* Cmp : GetComponents())
		{
			if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Cmp)) I->SetCullDistances(0, (int32)Cull);
		}
	}
}

// ------------------------------------------------------------------------------------------------ the falls

void AWRSwampScenery::BuildFalls(FWRSwampBuild& B)
{
	// The Weeping Wall's five falls (cook 2.3): ribbons of rust-red water from the Crown's lip to the plunge pools,
	// hugging the cliff (found by tracing in from the east at each height), red-stained rock either side, spray and
	// pink mist at the foot. Two ribbon sets (rust and the Red Tide's oxblood); one shows. This actor stands at the
	// world origin's yaw, so local = world - its location.
	const FWRSwampData& D = FWRSwampData::Get();
	if (B.bServer) return;
	FallsNormal = NewObject<USceneComponent>(this);
	FallsNormal->SetupAttachment(RootComponent);
	FallsNormal->RegisterComponent();
	FallsTide = NewObject<USceneComponent>(this);
	FallsTide->SetupAttachment(RootComponent);
	FallsTide->RegisterComponent();
	TArray<FWRLookBlock> Normal, Tide, Stain;
	WRLookKit::FKit KN(B.T, Normal), KT(B.T, Tide), KS(B.T, Stain);
	FSurf Water = FSurf::Glaze(FallRust * 0.42f, 0.12f);
	Water.Glow = WRTune(TEXT("swampWorld.fallGlow"), 0.04f);
	FSurf TideWater = FSurf::Glaze(Oxblood * 0.5f, 0.1f);
	TideWater.Glow = Water.Glow;
	const FSurf Stained = FSurf::Stone(Rust * 0.22f, 0.7f);
	const float Step = WRTune(TEXT("swampWorld.fallStep"), 2000.f);
	int32 FallIdx = 0;
	// The third fall (the one in front of the Gullet: M-S4) draws into its own pair of sets, pivoted on its line, so the
	// world can narrow it at sunset.
	int32 ThinIdx = INDEX_NONE;
	if (const FWRSwampData::FSite* Gullet = D.FindSite(TEXT("gullet")))
	{
		float Best = 1e12f;
		for (int32 f = 0; f < D.Falls.Num(); ++f)
		{
			const float Dd = FVector::Dist2D(D.Falls[f].Top, Gullet->At);
			if (Dd < Best) { Best = Dd; ThinIdx = f; }
		}
	}
	for (const FWRSwampData::FFall& F : D.Falls)
	{
		const bool bThin = FallIdx == ThinIdx;
		TArray<FWRLookBlock> ThinN, ThinT;
		FTransform ThinT0 = B.T;
		FVector Pivot = FVector::ZeroVector;
		if (bThin) Pivot = B.ToLocal(FVector(F.Top.X, F.Top.Y, (F.Top.Z + F.Bottom) * 0.5f));
		ThinT0 = FTransform(Pivot) * B.T;
		WRLookKit::FKit KThinN(ThinT0, ThinN), KThinT(ThinT0, ThinT);
		WRLookKit::FKit& RibN = bThin ? KThinN : KN;
		WRLookKit::FKit& RibT = bThin ? KThinT : KT;
		const FVector Off = bThin ? Pivot : FVector::ZeroVector;
		const float Top = F.Top.Z, Bottom = F.Bottom;
		const int32 N = FMath::Clamp(FMath::CeilToInt((Top - Bottom) / Step), 2, 60);
		TArray<FVector> Line;
		float LastX = F.Top.X;
		for (int32 i = 0; i <= N; ++i)
		{
			const float Z = FMath::Lerp(Top, Bottom, float(i) / N);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampFall), false, this);
			const FVector From(F.Top.X + 60000.f, F.Top.Y, Z), To(F.Top.X - 30000.f, F.Top.Y, Z);
			if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q)) LastX = Hit.ImpactPoint.X + WRTune(TEXT("swampWorld.fallStandOff"), 180.f);
			Line.Add(B.ToLocal(FVector(LastX, F.Top.Y, Z)));
		}
		for (int32 i = 0; i < N; ++i)
		{
			const FVector P0 = Line[i], P1 = Line[i + 1];
			const FVector Mid = (P0 + P1) * 0.5f, Dir = (P1 - P0).GetSafeNormal();
			const FRotator R = FRotationMatrix::MakeFromZX(Dir, FVector::ForwardVector).Rotator();
			const float L = (P1 - P0).Size() + 40.f;
			// The ribbon narrows where it leaves the lip and spreads toward the foot; a second, thinner sheet in front.
			const float W = F.Width * FMath::Lerp(0.7f, 1.15f, float(i) / N);
			RibN.Add(Water.Dim(0.9f + 0.2f * KN.Rand(Mid, i)), FVector(30.f, W, L), Mid - Off, R);
			RibN.Add(Water.Dim(1.15f), FVector(10.f, W * 0.55f, L), Mid - Off + FVector(40.f, W * 0.1f * FMath::Sin(i * 1.3f), 0.f), R);
			RibT.Add(TideWater.Dim(0.9f + 0.2f * KT.Rand(Mid, i)), FVector(30.f, W, L), Mid - Off, R);
			RibT.Add(TideWater.Dim(1.1f), FVector(10.f, W * 0.55f, L), Mid - Off + FVector(40.f, W * 0.1f * FMath::Sin(i * 1.3f), 0.f), R);
			// The cliff either side stained rust where the spray dries.
			for (const float Sd : { -1.f, 1.f }) KS.Add(Stained.Dim(0.8f + 0.4f * KS.Rand(Mid, i + 7)), FVector(16.f, W * 0.8f, L), Mid + FVector(-30.f, Sd * W * 0.85f, 0.f), R);
			if (i % FMath::Max(1, N / 6) == 0)
			{
				B.Fx(TEXT("swamp.fall"), Mid, F.Width / 1800.f, FRotator::ZeroRotator, FallsNormal);
				B.Fx(TEXT("swamp.fall_tide"), Mid, F.Width / 1800.f, FRotator::ZeroRotator, FallsTide);
			}
		}
		// The plunge pool's foot: spray, pink mist rising, the faint glow the Falls Shrine's braziers give the mist.
		const FVector Foot = Line.Last();
		B.Fx(TEXT("swamp.plunge"), Foot + FVector(600.f, 0, 0), 1.2f);
		B.Light(Foot + FVector(800.f, 0, 600.f), FLinearColor(0.9f, 0.45f, 0.45f), WRTune(TEXT("swampWorld.mistNight"), 2500.f), 5000.f, true);
		if (bThin)
		{
			for (int32 k = 0; k < 2; ++k)
			{
				USceneComponent* Th = NewObject<USceneComponent>(this);
				Th->SetMobility(EComponentMobility::Movable);
				Th->SetupAttachment(k == 0 ? FallsNormal.Get() : FallsTide.Get());
				Th->SetRelativeLocation(Pivot);
				Th->RegisterComponent();
				FWRBuildLook::Spawn(this, Th, k == 0 ? ThinN : ThinT, FLinearColor::Black, false);
				FallThin.Add(Th);
			}
		}
		++FallIdx;
	}
	FWRBuildLook::Spawn(this, FallsNormal, Normal, FLinearColor::Black, false);
	FWRBuildLook::Spawn(this, FallsTide, Tide, FLinearColor::Black, false);
	FWRBuildLook::Spawn(this, RootComponent, Stain, FLinearColor::Black, false);
	FallsTide->SetVisibility(false, true);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP falls: %d drawn"), FallIdx);
}

// ------------------------------------------------------------------------------------------------ landmarks

void AWRSwampScenery::BuildLandmark(FWRSwampBuild& B)
{
	using namespace WRSwampSceneryDetail;
	FKit& K = B.K;
	const FName Kd = Kind;
	const FWRSwampData& D = FWRSwampData::Get();
	UMaterialInstanceDynamic* RustRock = B.bServer ? nullptr : RockMat(this, Rust * 0.9f);
	UMaterialInstanceDynamic* Lime = B.bServer ? nullptr : RockMat(this, Limestone * 0.9f);

	if (Kd == TEXT("weeping_wall"))
	{
		// The wall's foot (the falls themselves are their own actor): plunge-pool rims of boulders crusted rust-red with
		// bog iron, drowned driftwood, a Coven bone causeway post at the shallows' edge.
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = i * 2.39996f;
			const FVector P = B.At(FMath::Cos(A) * (900.f + 140.f * i), FMath::Sin(A) * (1500.f + 120.f * i));
			B.Rock(B.Seed + i, FVector(260.f + 90.f * (i % 3), 220.f + 60.f * (i % 2), 140.f + 60.f * (i % 4)), P, FRotator(4.f, A * 57.f, 3.f), RustRock, true, 50.f);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P = B.Top(-600.f + 300.f * i, 1200.f - 450.f * i, -10.f);
			K.Rod(FSurf::Wood(C(120, 110, 96) * 0.4f), P, P + FRotator(4.f, 40.f * i, 0.f).Vector() * (300.f + 80.f * (i % 3)), 18.f);
		}
		B.FetishPost(B.At(1800.f, 0.f), 180.f, 2);
		return;
	}
	if (Kd == TEXT("mothers_throat"))
	{
		// The super landmark (cook 2.3): 12 dead cypress 40 m tall on the upper ring, leaning in over the pool, their
		// trunks and knees sunk in the bone-and-peat bank; rusted cages on chains from nine of them hanging out over the
		// water; a red mist column off the water 150 m tall, lit from below. The rods and the flood are the boss's.
		const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("vharra"));
		const TSharedPtr<FJsonObject>* RingsObj = nullptr;
		TSharedPtr<FJsonObject> Rings;
		if (Ar.IsValid() && Ar->TryGetObjectField(TEXT("rings"), RingsObj)) Rings = *RingsObj;
		const FVector Centre = Ar.IsValid() ? B.ToLocal(ArenaCentre(Ar)) * FVector(1, 1, 0) : FVector::ZeroVector;
		const float Upper = RingRadius(Rings, TEXT("upper"), 5500.f);
		const float Pool = RingRadius(Rings, TEXT("pool"), 2000.f);
		const float TreeH = WRTune(TEXT("swampWorld.throatTreeHeight"), 4000.f);
		for (int32 t = 0; t < 12; ++t)
		{
			const float A = 30.f * t + 9.f;
			const FVector Foot = B.At(Centre.X + FMath::Cos(FMath::DegreesToRadians(A)) * (Upper - 300.f), Centre.Y + FMath::Sin(FMath::DegreesToRadians(A)) * (Upper - 300.f), -60.f);
			const float InYaw = A + 180.f;
			const TArray<FVector> Tips = B.Cypress(K, Foot, TreeH * (0.85f + 0.3f * B.Rand(t)), 12.f + 6.f * B.Rand(t + 20), InYaw, true, t);
			// The bank round the foot: bone and peat heaped against the trunk.
			for (int32 h = 0; h < 5; ++h) K.Disc(h % 2 ? PeatS() : OldBone(), Foot + FRotator(0.f, InYaw + 70.f * h, 0.f).Vector() * 160.f + FVector(0, 0, 20.f), FVector::UpVector, 260.f, 50.f);
			if (t % 4 != 3)
			{
				// A cage hung on its chain from the limb that reaches furthest over the pool.
				FVector Best = Tips.Num() ? Tips[0] : Foot + FVector(0, 0, TreeH * 0.7f);
				for (const FVector& Tp : Tips) if (FVector::Dist2D(Tp, Centre) < FVector::Dist2D(Best, Centre)) Best = Tp;
				FKit& Hang = B.Swing(Best);
				FWRSwampBuild::Cage(Hang, FVector::ZeroVector, 400.f + 500.f * B.Rand(t + 5), 190.f, 46.f, t % 3 == 0);
			}
		}
		// The mist column: smoke rising off the pool in tiers to 150 m, a red light under it on the water.
		const float Water = B.WaterZ;
		const int32 Tiers = (int32)WRTune(TEXT("swampWorld.mistTiers"), 6.f);
		const float ColH = WRTune(TEXT("swampWorld.mistHeight"), 15000.f);
		if (!B.bServer)
		{
			MistTide = NewObject<USceneComponent>(this);
			MistTide->SetupAttachment(RootComponent);
			MistTide->SetRelativeLocation(FVector(Centre.X, Centre.Y, Water));
			MistTide->RegisterComponent();
			for (int32 i = 0; i < Tiers; ++i)
			{
				const FVector P(Centre.X, Centre.Y, Water + ColH * i / Tiers);
				B.Fx(TEXT("swamp.mistcol"), P, Pool / 1600.f * (1.f + 0.15f * i));
				B.Fx(TEXT("swamp.mistcol"), P + FVector(0, 0, ColH * 0.5f / Tiers), Pool / 1600.f * (1.2f + 0.15f * i), FRotator(0.f, 40.f, 0.f), MistTide);
			}
			MistTide->SetVisibility(false, true);
			MistGlow = B.Light(FVector(Centre.X, Centre.Y, Water + 300.f), FLinearColor(0.8f, 0.12f, 0.08f), WRTune(TEXT("swampWorld.mistGlow"), 60000.f), Pool * 3.f, false);
			B.Light(FVector(Centre.X, Centre.Y, Water + ColH * 0.4f), FLinearColor(0.7f, 0.1f, 0.08f), WRTune(TEXT("swampWorld.mistGlowHigh"), 120000.f), ColH * 0.6f, true);
		}
		return;
	}
	if (Kd == TEXT("bled_roc"))
	{
		// The Rodwardens' champion (cook 2.3, 2.20 beat 8): a roc skeleton of 45 m span sprawled over the gut mouth,
		// spine and ribs arching 8 m, the wings fanned flat, hide still on the ribs in leech-scarred strips; Coven bone
		// hooks through the wing bones, their chains running to stakes driven in the bog.
		const float Span = WRTune(TEXT("swampWorld.rocSpan"), 4500.f);
		const float S = Span / 4500.f;
		FVector Prev = B.At(-900.f * S, 0, 120.f);
		for (int32 v = 0; v < 24; ++v)
		{
			const float X = -900.f * S + v * 75.f * S;
			const FVector P = B.At(X, 60.f * FMath::Sin(v * 0.4f), 120.f + 80.f * FMath::Sin(v * 0.25f));
			K.Add(BoneS(0.95f), FVector(70.f, 60.f, 55.f) * S, P, (P - Prev).Rotation());
			K.Spike(BoneS(), P + FVector(0, 0, 25.f * S), FVector::UpVector, 22.f * S, 55.f * S);
			if (v > 4 && v < 17)
			{
				const float Rr = (800.f - FMath::Abs(v - 10) * 50.f) * S;
				for (const float Sd : { -1.f, 1.f })
				{
					FVector Last = P;
					for (int32 s = 1; s <= 6; ++s)
					{
						const float A = s / 6.f * 170.f;
						const FVector Q = P + FVector(-20.f * s * S, Sd * Rr * 0.6f * FMath::Sin(FMath::DegreesToRadians(A)), Rr * (1.f - FMath::Cos(FMath::DegreesToRadians(A))) * 0.5f);
						K.Rod(BoneS(0.92f), Last, Q, 18.f * S);
						if (s >= 2 && s <= 4 && (v % 3) == 0) K.Add(LeatherS(0.55f), FVector(70.f * S, 6.f, 90.f * S), (Last + Q) * 0.5f + FVector(0, Sd * 10.f, -20.f), FRotator(0.f, 0.f, Sd * (30.f + 20.f * s)));
						Last = Q;
					}
				}
				if (v % 3 == 0) for (int32 p = 0; p < 4; ++p) K.Add(FSurf::Stone(BoneOld * 0.15f, 0.9f), FVector(9.f, 9.f, 10.f), P + FVector(0, (p - 1.5f) * 120.f * S, 380.f * S), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			Prev = P;
		}
		B.Bar(B.At(-900.f * S, 0, 120.f), B.At(900.f * S, 0, 120.f), 80.f * S);
		// The neck curving down to the skull, the great hooked beak open on the mud.
		const FVector NeckBase = B.At(900.f * S, 0, 150.f);
		FVector NPrev = NeckBase;
		for (int32 n = 1; n <= 8; ++n)
		{
			const FVector NP = NeckBase + FVector(n * 70.f * S, 120.f * S * FMath::Sin(n * 0.5f), 120.f * S * FMath::Sin(n * 0.35f) - n * 10.f);
			K.Add(BoneS(), FVector(60.f, 48.f, 44.f) * S, NP, (NP - NPrev).Rotation());
			NPrev = NP;
		}
		const FVector Skull = NPrev + FVector(160.f * S, 0, 0);
		K.Add(BoneS(1.05f), FVector(230.f, 150.f, 140.f) * S, Skull, FRotator(-10.f, 0.f, 15.f));
		K.Add(Dark(), FVector(40.f, 155.f, 50.f) * S, Skull + FVector(10.f, 0, 30.f) * S, FRotator(-10.f, 0.f, 15.f), EWRLookMesh::Cylinder);
		K.Spike(OldBone(), Skull + FVector(100.f, 0, 10.f) * S, FRotator(-25.f, 0.f, 0.f).Vector(), 120.f * S, 300.f * S);
		K.Spike(OldBone(), Skull + FVector(90.f, 0, -40.f) * S, FRotator(10.f, 0.f, 0.f).Vector(), 90.f * S, 240.f * S);
		// The wings: humerus, the forearm pair, the hand, and the long feather shafts fanned out across the bog.
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Sh = B.At(300.f * S, Sd * 300.f * S, 260.f * S);
			const FVector El = B.At(-100.f * S, Sd * 900.f * S, 120.f);
			const FVector Wr = B.At(250.f * S, Sd * 1600.f * S, 80.f);
			const FVector Hd = B.At(-50.f * S, Sd * Span * 0.5f, 60.f);
			K.Rod(BoneS(), Sh, El, 46.f * S);
			K.Rod(BoneS(0.95f), El, Wr, 30.f * S);
			K.Rod(BoneS(0.95f), El + FVector(0, 0, 30.f), Wr + FVector(0, 0, 30.f), 20.f * S);
			K.Rod(BoneS(), Wr, Hd, 24.f * S);
			for (int32 f = 0; f < 10; ++f)
			{
				const FVector RootP = FMath::Lerp(El, Hd, f / 10.f);
				const FVector Tip = B.At(RootP.X - (600.f + 120.f * f) * S, RootP.Y + Sd * 80.f * f * S, 20.f);
				K.Rod(OldBone(), RootP, Tip, 8.f * S);
			}
			// A Coven hook through the forearm, its chain running out to a stake driven in the bog.
			const FVector Hook = FMath::Lerp(El, Wr, 0.5f) + FVector(0, 0, 40.f);
			K.Rod(OldBone(), Hook - FVector(0, 0, 80.f * S), Hook + FVector(0, 0, 60.f * S), 18.f * S);
			K.Spike(OldBone(), Hook + FVector(0, 0, 60.f * S), FVector(0.6f, 0, -0.8f), 20.f * S, 70.f * S);
			const FVector Stake = B.At(Hook.X + 400.f, Hook.Y + Sd * 1200.f, 0.f);
			K.Rod(Black(), Stake - FVector(0, 0, 60.f), Stake + FVector(0, 0, 160.f), 26.f);
			FWRSwampBuild::Chain(K, Hook, Stake + FVector(0, 0, 140.f), 24.f);
			B.Bar(Sh, Wr, 30.f * S);
		}
		// Legs folded under, talons curled.
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Hip = B.At(-500.f * S, Sd * 150.f * S, 150.f);
			const FVector Kn = B.At(-300.f * S, Sd * 380.f * S, 90.f);
			const FVector Ft = B.At(-650.f * S, Sd * 420.f * S, 30.f);
			K.Rod(BoneS(), Hip, Kn, 36.f * S);
			K.Rod(BoneS(), Kn, Ft, 26.f * S);
			for (int32 c = 0; c < 4; ++c) K.Spike(OldBone(), Ft, FRotator(-20.f, (c < 3 ? 160.f + 25.f * (c - 1) : 0.f) + (Sd > 0 ? 0.f : 0.f), 0.f).Vector(), 18.f * S, 120.f * S);
		}
		// Its night read (cook 2.3): the ribs black against the lightning that flickers in Stormgut behind it.
		const TSharedPtr<FJsonObject>* Gut = nullptr;
		if (!B.bServer && D.Root.IsValid() && D.Root->TryGetObjectField(TEXT("gut"), Gut))
		{
			const FVector Mouth = B.ToLocal(WRSwamp::Point(*Gut, TEXT("mouth")));
			if (UPointLightComponent* L = B.Light(Mouth + FVector(0, 0, 2500.f), FLinearColor(0.72f, 0.68f, 1.f), 0.f, WRTune(TEXT("swampWorld.gutFlickerRadius"), 12000.f), false))
			{
				FlickerLights.Add(L);
				FlickerBase.Add(WRTune(TEXT("swampWorld.gutFlicker"), 60000.f));
			}
		}
		return;
	}
	if (Kd == TEXT("walking_house"))
	{
		// The Walking House (cook 2.3; Baba Yaga's hut on hen's legs): a log hut 15 m up on two bird legs carved from black
		// cypress, each a long shank down to a backward ankle, a scaled foot and four clawed toes sunk in the bog; a porch
		// at the door with the rope ladder hanging from it; smoke from the chimney; the swamp's one warm window; inside a
		// hearth with a cauldron, bloodmoss drying, a reliquary shelf, and the cellar trapdoor in the floor (M-S2).
		const float Floor = WRTune(TEXT("swampWorld.houseFloor"), 1500.f) + B.G(0, 0);
		const FSurf Leg = Black(1.1f);
		const FSurf Carve = Black(0.6f);
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Hip(-40.f, Sd * 210.f, Floor - 60.f);
			const FVector Ankle(-320.f, Sd * 250.f, B.G(-320.f, Sd * 250.f) + 720.f);
			const FVector Foot = B.At(-60.f, Sd * 270.f, 40.f);
			K.Add(Leg, FVector(130.f, 120.f, 160.f), Hip + FVector(0, 0, -20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);     // the thigh's bulk
			K.Rod(Leg, Hip, Ankle, 76.f);
			K.Add(Leg, FVector(96.f, 96.f, 96.f), Ankle, FRotator::ZeroRotator, EWRLookMesh::Cylinder);                            // the ankle joint
			K.Rod(Leg, Ankle, Foot, 56.f);
			B.Bar(Hip, Ankle, 38.f);
			B.Bar(Ankle, Foot, 28.f);
			// Carved bands and scale rings down the shank.
			for (int32 r = 1; r < 10; ++r) K.Disc(Carve, FMath::Lerp(Ankle, Foot, r / 10.f), (Foot - Ankle).GetSafeNormal(), 62.f, 6.f);
			for (int32 r = 1; r < 6; ++r) K.Disc(Carve, FMath::Lerp(Hip, Ankle, r / 6.f), (Ankle - Hip).GetSafeNormal(), 82.f, 8.f);
			// Four toes: three forward, one back, two joints each and a hooked claw, sunk to the knuckle in the mud.
			for (int32 t = 0; t < 4; ++t)
			{
				const float A = t < 3 ? -28.f + 28.f * t : 180.f;
				const FVector Dir = FRotator(0.f, A, 0.f).Vector();
				const FVector J0 = Foot - FVector(0, 0, 20.f), J1 = J0 + Dir * 160.f + FVector(0, 0, 20.f), J2 = J1 + Dir * 130.f - FVector(0, 0, 50.f);
				K.Rod(Leg, J0, J1, 34.f);
				K.Rod(Leg, J1, J2, 28.f);
				K.Add(Carve, FVector(40.f, 40.f, 40.f), J1, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				K.Spike(BoneS(0.7f), J2, (Dir - FVector(0, 0, 0.8f)).GetSafeNormal(), 22.f, 70.f);
			}
		}
		// The hut: walls of stacked logs crossing at the corners, 5 x 4 m, 2.6 m to the eaves.
		const float HW = 250.f, HD = 200.f, Eave = 260.f;
		K.Add(Black(1.2f), FVector(HD * 2.f + 60.f, HW * 2.f + 60.f, 30.f), FVector(0, 0, Floor - 15.f));
		B.Block(FVector(0, 0, Floor - 15.f), FVector(HD + 30.f, HW + 30.f, 15.f));
		for (int32 c = 0; c < 9; ++c)
		{
			const float Z = Floor + 15.f + c * 29.f;
			for (const float Sd : { -1.f, 1.f })
			{
				K.Rod(Black(0.9f + 0.15f * B.Rand(c)), FVector(-HD - 30.f, Sd * HW, Z), FVector(HD + 30.f, Sd * HW, Z), 30.f);
				if (Sd < 0.f || c < 2 || c > 7) K.Rod(Black(0.9f + 0.15f * B.Rand(c + 9)), FVector(-HD, -HW - 30.f, Z + 14.f), FVector(-HD, HW + 30.f, Z + 14.f), 30.f);
				// The front (+X): logs either side of the door, the window's gap on the left of it.
				if (c < 7 && !(c >= 3 && c <= 5 && Sd < 0.f)) K.Rod(Black(0.95f), FVector(HD, Sd * HW, Z + 14.f), FVector(HD, Sd * 60.f, Z + 14.f), 30.f);
			}
			if (c >= 7) K.Rod(Black(0.95f), FVector(HD, -HW - 30.f, Z + 14.f), FVector(HD, HW + 30.f, Z + 14.f), 30.f);
		}
		B.Block(FVector(-HD, 0, Floor + Eave * 0.5f), FVector(15.f, HW, Eave * 0.5f));
		for (const float Sd : { -1.f, 1.f }) B.Block(FVector(0, Sd * HW, Floor + Eave * 0.5f), FVector(HD, 15.f, Eave * 0.5f));
		for (const float Sd : { -1.f, 1.f }) B.Block(FVector(HD, Sd * (HW + 60.f) * 0.5f, Floor + Eave * 0.5f), FVector(15.f, (HW - 60.f) * 0.5f, Eave * 0.5f));
		// The window: the warm light behind a horn-pane, the swamp's only home light.
		FSurf Pane = FSurf::Glass(FLinearColor(0.9f, 0.55f, 0.22f) * 0.6f, 0.f);
		Pane.Glow = 1.2f;
		K.Add(Pane, FVector(6.f, 110.f, 80.f), FVector(HD + 2.f, -HW * 0.62f, Floor + 15.f + 4.f * 29.f));
		for (int32 m = 0; m < 3; ++m) K.Add(Black(0.5f), FVector(8.f, 4.f, 80.f), FVector(HD + 5.f, -HW * 0.62f - 36.f + 36.f * m, Floor + 131.f));
		B.Light(FVector(HD + 120.f, -HW * 0.62f, Floor + 130.f), FLinearColor(1.f, 0.58f, 0.26f), WRTune(TEXT("swampWorld.houseWindow"), 6000.f), 2200.f, true);
		B.Light(FVector(0, 0, Floor + 160.f), FLinearColor(1.f, 0.5f, 0.2f), WRTune(TEXT("swampWorld.houseHearth"), 3500.f), 700.f, false);
		// The door, open in on its pegs; the porch out front on the floor beams.
		K.Add(Black(1.3f), FVector(8.f, 110.f, 200.f), FVector(HD - 50.f, 70.f, Floor + 100.f), FRotator(0.f, 70.f, 0.f));
		for (int32 p = 0; p < 7; ++p) K.Add(Planks(0.8f), FVector(28.f, 260.f, 6.f), FVector(HD + 30.f + p * 30.f, 0.f, Floor - 4.f));
		B.Block(FVector(HD + 120.f, 0, Floor - 6.f), FVector(110.f, 130.f, 8.f));
		for (const float Sd : { -1.f, 1.f }) K.Rod(Black(), FVector(HD + 220.f, Sd * 125.f, Floor), FVector(HD + 220.f, Sd * 125.f, Floor + 100.f), 8.f);
		// The roof: steep shingles of split cypress over the ridge pole, moss on the north slope.
		const float Ridge = Floor + Eave + 230.f;
		K.Rod(Black(), FVector(-HD - 70.f, 0, Ridge), FVector(HD + 70.f, 0, Ridge), 26.f);
		for (int32 c = 0; c < 8; ++c) for (const float Sd : { -1.f, 1.f })
		{
			const float T0 = c / 8.f;
			K.Add((Sd > 0.f && c % 3 == 0) ? MossS() : Black(0.75f + 0.1f * (c % 3)), FVector(HD * 2.f + 140.f, 52.f, 7.f), FVector(0, Sd * (HW + 70.f) * (1.f - T0), Floor + Eave + (Ridge - Floor - Eave) * T0 + 10.f), FRotator(0.f, 0.f, Sd * -42.f));
		}
		B.Block(FVector(0, 0, (Floor + Eave + Ridge) * 0.5f), FVector(HD + 60.f, HW + 60.f, (Ridge - Floor - Eave) * 0.5f));
		// The chimney: clay-daubed stone stack at the back, its smoke.
		for (int32 c = 0; c < 12; ++c) K.Add(FSurf::Stone(C(96, 84, 70) * 0.45f, 0.9f), FVector(80.f, 80.f, 40.f), FVector(-HD + 30.f, HW * 0.5f, Floor + 20.f + c * 40.f), FRotator(0.f, c * 9.f, 0.f));
		B.Fx(TEXT("swamp.smoke"), FVector(-HD + 30.f, HW * 0.5f, Floor + 500.f), 1.2f);
		// Inside: the hearth and cauldron, bloodmoss drying on strings, the reliquary shelf of bowls, the trapdoor.
		for (int32 s = 0; s < 8; ++s) K.Add(FSurf::Stone(Slate * 0.6f), FVector(26.f, 18.f, 16.f), FVector(-HD + 70.f, HW * 0.5f, Floor + 8.f) + FRotator(0.f, 45.f * s, 0.f).Vector() * 45.f, FRotator(0.f, 45.f * s, 0.f));
		K.Add(IronS(), FVector(60.f, 60.f, 42.f), FVector(-HD + 70.f, HW * 0.5f, Floor + 44.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(FSurf::Lit(FLinearColor(0.6f, 0.15f, 0.04f), 0.8f), FVector(50.f, 50.f, 4.f), FVector(-HD + 70.f, HW * 0.5f, Floor + 12.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Rod(Cord(), FVector(-HD + 10.f, -HW + 20.f, Floor + 220.f), FVector(HD - 20.f, -HW + 20.f, Floor + 220.f), 1.f);
		for (int32 b = 0; b < 9; ++b) K.Add(GarnetMoss(0.8f), FVector(14.f, 14.f, 34.f), FVector(-HD + 40.f + b * 38.f, -HW + 20.f, Floor + 196.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Planks(0.7f), FVector(30.f, 200.f, 5.f), FVector(-HD + 30.f, -60.f, Floor + 150.f));
		for (int32 b = 0; b < 5; ++b) K.Add(FSurf::Steel(C(110, 78, 40) * 0.5f, 0.5f), FVector(22.f, 22.f, 10.f), FVector(-HD + 30.f, -150.f + 40.f * b, Floor + 158.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Black(1.4f), FVector(110.f, 110.f, 4.f), FVector(-40.f, -90.f, Floor + 2.f));
		K.Disc(IronS(), FVector(10.f, -90.f, Floor + 5.f), FVector::UpVector, 18.f, 3.f);
		for (int32 s = 0; s < 2; ++s) K.Add(IronS(), FVector(100.f, 8.f, 3.f), FVector(-40.f, -125.f + 70.f * s, Floor + 5.f));
		K.Add(IronS(), FVector(14.f, 10.f, 16.f), FVector(-40.f, -36.f, Floor + 10.f));   // the padlock
		// The rope ladder from the porch's edge to the bog (the climbable lane is the server's fixture beside it).
		const FVector LTop(HD + 236.f, 0.f, Floor), LFoot = B.At(HD + 290.f, 0.f);
		for (const float Y : { -26.f, 26.f }) K.Rod(FSurf::Cloth(C(150, 126, 90) * 0.45f), LTop + FVector(0, Y, 10.f), LFoot + FVector(0, Y, 0.f), 3.f);
		const int32 Rungs = FMath::Max(2, FMath::RoundToInt((LTop.Z - LFoot.Z) / 35.f));
		for (int32 g = 1; g < Rungs; ++g) K.Rod(Planks(1.2f), FMath::Lerp(LFoot, LTop, float(g) / Rungs) + FVector(0, -26.f, 0), FMath::Lerp(LFoot, LTop, float(g) / Rungs) + FVector(0, 26.f, 0), 4.f);
		return;
	}
	if (Kd == TEXT("hanging_cypress"))
	{
		// The oldest cypress (cook 2.3, 2.4 #10): 60 m, a 4 m buttress, knees all round; hundreds of bottle charms and bone
		// chimes on its limbs, swaying; nine iron cages on chains; the key cage 18 m up on the long east limb, a skeleton
		// in it gripping the Coven key. The way up: the knees, then a spiral of low limbs, each a step's jump above the
		// last, to the limb the key cage hangs from.
		const float H = WRTune(TEXT("swampWorld.hangingHeight"), 6000.f);
		const FVector Base = B.At(0, 0, -40.f);
		const TArray<FVector> Tips = B.Cypress(K, Base, H, 2.f, 0.f, false, 3, true);
		const float KeyZ = WRTune(TEXT("swampWorld.keyCageHeight"), 1800.f) + Base.Z;
		const float Trunk = FMath::Max(80.f, H * 0.075f) * 0.5f;
		// The climbing limbs: a spiral of stubs round the trunk, each flat on top and 90 cm above the last.
		const int32 Steps = FMath::CeilToInt((KeyZ - Base.Z - 60.f) / 90.f);
		for (int32 s = 0; s < Steps; ++s)
		{
			const float A = s * 47.f;
			const float Z = Base.Z + 90.f * (s + 1);
			const FVector Dir = FRotator(0.f, A, 0.f).Vector();
			const FVector P0 = FVector(Base.X, Base.Y, Z) + Dir * (Trunk * (1.f - 0.4f * float(s) / Steps) - 10.f);
			const FVector P1 = P0 + Dir * 150.f;
			K.Rod(Bark(0.9f), P0, P1 + FVector(0, 0, 8.f), 34.f);
			B.Block((P0 + P1) * 0.5f, FVector(75.f, 26.f, 14.f), FRotator(0.f, A, 0.f));
		}
		// The key limb: long, level, east, wide enough to walk; the key cage hangs off its end.
		const FVector KeyFrom = FVector(Base.X, Base.Y, KeyZ) + FVector(Trunk * 0.6f, 0, 0), KeyTo = KeyFrom + FVector(520.f, 0, 10.f);
		K.Rod(Bark(), KeyFrom, KeyTo, 60.f);
		B.Block((KeyFrom + KeyTo) * 0.5f + FVector(0, 0, 22.f), FVector(260.f, 30.f, 10.f));
		FKit& KeyHang = B.Swing(KeyTo + FVector(-60.f, 0, -20.f));
		FWRSwampBuild::Cage(KeyHang, FVector::ZeroVector, 160.f, 190.f, 46.f, true);
		// The key in the skeleton's raised hand (bronze, as long as a forearm).
		KeyHang.Rod(FSurf::Steel(C(140, 106, 58) * 0.6f, 0.4f), FVector(18.f, -20.f, -300.f), FVector(18.f, -20.f, -260.f), 4.f);
		KeyHang.Disc(FSurf::Steel(C(140, 106, 58) * 0.6f, 0.4f), FVector(18.f, -20.f, -255.f), FVector::ForwardVector, 14.f, 3.f);
		// Nine cages, charms by the hundred, bone chimes.
		int32 Cages = 0, Charms = 0;
		for (int32 t = 0; t < Tips.Num(); ++t)
		{
			const FVector Tip = Tips[t];
			if (Cages < 8 && t % 1 == 0)
			{
				FKit& Hang = B.Swing(Tip);
				FWRSwampBuild::Cage(Hang, FVector::ZeroVector, 300.f + 400.f * B.Rand(t), 180.f, 44.f, t % 3 == 1);
				++Cages;
			}
			const FVector From(Base.X, Base.Y, Tip.Z);
			for (int32 g = 0; g < 4; ++g)
			{
				FKit& Hang = B.Swing(FMath::Lerp(From, Tip, 0.3f + 0.17f * g) - FVector(0, 0, 10.f));
				for (int32 c = 0; c < 6; ++c)
				{
					const FVector Off(FMath::Cos(c * 1.1f) * 40.f, FMath::Sin(c * 1.1f) * 40.f, 0.f);
					if ((c + g) % 4 == 0) FWRSwampBuild::Chime(Hang, Off);
					else FWRSwampBuild::Bottle(Hang, Off, 30.f + 40.f * B.Rand(c + g * 7 + t), c + g + t);
					++Charms;
				}
			}
		}
		// Lanterntoads ring its knees at night.
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P = B.At(FMath::Cos(i * 1.05f) * (Trunk * 3.f), FMath::Sin(i * 1.05f) * (Trunk * 3.f), 30.f);
			if (USceneComponent* F = B.Fx(TEXT("swamp.toad"), P, 1.f)) NightFx.Add(F);
			if (UPointLightComponent* L = B.Light(P + FVector(0, 0, 30.f), FLinearColor(0.75f, 0.85f, 0.25f), 0.f, 600.f, false)) { PulseLights.Add(L); PulseBase.Add(WRTune(TEXT("swampWorld.toadLight"), 700.f)); }
		}
		UE_LOG(LogWildreach, Log, TEXT("SWAMP hanging cypress: %d cages + the key cage, %d charms, key limb at %s"), Cages, Charms, *GetActorTransform().TransformPosition(KeyTo).ToCompactString());
		return;
	}
	if (Kd == TEXT("mangrove_cathedral"))
	{
		// One giant red mangrove (cook 2.3): a trunk 3 m thick on a knot of roots, its prop roots arching 20 m high and
		// out 25 m over the channel crossroads like a vaulted nave, aerial roots dropping from the limbs; Lanterntoads in
		// the arches at night; a root altar at its foot (the Root Shrine stands here if the data puts one here).
		const float H = WRTune(TEXT("swampWorld.cathedralHeight"), 3200.f);
		const FVector Base = B.At(0, 0, -50.f);
		const FVector Knot = Base + FVector(0, 0, 700.f);
		K.Rod(RootS(), Base, Knot, 300.f);
		K.Rod(RootS(0.95f), Knot, Base + FVector(0, 0, H), 220.f);
		B.Bar(Base, Base + FVector(0, 0, H * 0.7f), 140.f);
		const int32 Roots = (int32)WRTune(TEXT("swampWorld.cathedralRoots"), 16.f);
		for (int32 r = 0; r < Roots; ++r)
		{
			const float A = 360.f * r / Roots + 7.f * B.Rand(r);
			const FVector Dir = FRotator(0.f, A, 0.f).Vector();
			const FVector From = Knot + FVector(0, 0, 300.f + 900.f * B.Rand(r + 30)) + Dir * 120.f;
			const float Reach = 1800.f + 900.f * B.Rand(r + 50);
			const FVector Foot = B.At(Dir.X * Reach, Dir.Y * Reach, -80.f);
			FVector Prev = From;
			for (int32 s = 1; s <= 10; ++s)
			{
				const float T0 = s / 10.f;
				const FVector Pt = FMath::Lerp(From, Foot, T0) + FVector(0, 0, FMath::Sin(T0 * PI * 0.9f) * (2000.f - From.Z + Base.Z) * 0.9f);
				K.Rod(RootS(0.9f + 0.2f * B.Rand(r + s)), Prev, Pt, FMath::Lerp(70.f, 34.f, T0));
				if (s == 10) for (int32 f = 0; f < 4; ++f) K.Rod(RootS(0.8f), Pt + FVector(0, 0, 60.f), B.At(Pt.X + FMath::Cos(f * 1.6f) * 120.f, Pt.Y + FMath::Sin(f * 1.6f) * 120.f, -40.f), 16.f);
				if (s == 5 && r % 3 == 0)
				{
					if (USceneComponent* F = B.Fx(TEXT("swamp.toad"), Pt - FVector(0, 0, 40.f), 1.f)) NightFx.Add(F);
					if (UPointLightComponent* L = B.Light(Pt - FVector(0, 0, 60.f), FLinearColor(0.75f, 0.85f, 0.25f), 0.f, 900.f, false)) { PulseLights.Add(L); PulseBase.Add(WRTune(TEXT("swampWorld.toadLight"), 700.f) * 1.3f); }
				}
				Prev = Pt;
			}
			B.Bar(FMath::Lerp(From, Foot, 0.85f) + FVector(0, 0, 200.f), Foot, 30.f);
		}
		// The canopy and the aerial roots.
		for (int32 c = 0; c < 10; ++c)
		{
			const FVector Cc = Base + FVector(FMath::Cos(c * 0.63f) * 900.f, FMath::Sin(c * 0.63f) * 900.f, H * (0.82f + 0.08f * (c % 3)));
			K.Rod(RootS(), Base + FVector(0, 0, H * 0.75f), Cc, 50.f);
			K.Disc(LeafS(0.85f + 0.1f * (c % 3)), Cc + FVector(0, 0, 60.f), FVector::UpVector, 1300.f, 260.f);
			for (int32 a = 0; a < 4; ++a)
			{
				const FVector Drop = Cc + FVector(FMath::Cos(a * 1.57f) * 300.f, FMath::Sin(a * 1.57f) * 300.f, 0.f);
				K.Rod(RootS(0.75f), Drop, FVector(Drop.X, Drop.Y, FMath::Max(B.G(Drop.X, Drop.Y), B.WaterZ) - 40.f), 6.f);
			}
		}
		bool bShrineHere = false;
		for (const FWRSwampData::FShrine& Sh : D.Shrines) bShrineHere |= FVector::Dist2D(Sh.At, GetActorLocation()) < 3000.f;
		if (!bShrineHere)
		{
			// The root altar: a table of grown roots at the trunk's foot, bowls of offerings, a garnet moss cloth.
			const FVector Alt = B.At(420.f, 0.f);
			for (int32 r = 0; r < 6; ++r) K.Rod(RootS(), Alt + FVector(FMath::Cos(r * 1.05f) * 90.f, FMath::Sin(r * 1.05f) * 60.f, -20.f), Alt + FVector(0, 0, 100.f), 22.f);
			K.Add(RootS(1.1f), FVector(180.f, 120.f, 20.f), Alt + FVector(0, 0, 105.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(GarnetMoss(), FVector(120.f, 70.f, 4.f), Alt + FVector(0, 0, 117.f));
			for (int32 b = 0; b < 3; ++b) K.Add(FSurf::Steel(C(110, 78, 40) * 0.5f, 0.5f), FVector(24.f, 24.f, 9.f), Alt + FVector(-40.f + 40.f * b, 10.f, 124.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Block(Alt + FVector(0, 0, 60.f), FVector(90.f, 60.f, 60.f));
		}
		return;
	}
	if (Kd == TEXT("corpse_mere"))
	{
		// The Mere's near shore and its far side (cook 2.3, 2.4 #18): a plank jetty on posts out into the water with a
		// punt half sunk against it; across the water the skeleton of a giant fish weir, its stakes leaning, its woven
		// panels rotted to tatters, the trap basket at its apex. The corpse candles are the world's (they drift).
		for (int32 p = 0; p < 9; ++p)
		{
			const FVector P0 = B.Top(p * 240.f, 0.f);
			for (const float Sd : { -1.f, 1.f }) K.Rod(Black(), FVector(P0.X, Sd * 90.f, B.G(P0.X, Sd * 90.f) - 80.f), FVector(P0.X, Sd * 90.f, B.WaterZ + 90.f), 18.f);
			if (p < 8) for (int32 b = 0; b < 9; ++b) K.Add(Planks(0.85f + 0.3f * B.Rand(b + p * 9)), FVector(24.f, 200.f, 5.f), FVector(p * 240.f + 13.f + b * 26.f, 0, B.WaterZ + 92.f));
		}
		B.Block(FVector(960.f, 0, B.WaterZ + 90.f), FVector(980.f, 100.f, 6.f));
		B.Punt(B.Top(900.f, 260.f, -30.f), 12.f, -9.f, 70.f, false);
		const FVector Weir = Row.IsValid() && Row->HasField(TEXT("weir")) ? B.ToLocal(WRSwamp::Point(Row, TEXT("weir"))) : FVector(WRTune(TEXT("swampWorld.mereWeirOffset"), 28000.f), 0.f, 0.f);
		const float WYaw = (FVector::ZeroVector - Weir).Rotation().Yaw;
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 s = 0; s < 26; ++s)
			{
				const FVector P = Weir + FRotator(0.f, WYaw + Sd * 35.f, 0.f).Vector() * (s * 140.f);
				const FVector F = B.Top(P.X, P.Y);
				const float Lean = 6.f + 18.f * B.Rand(s + (Sd > 0 ? 50 : 0));
				const FVector TopP = F + FRotator(90.f - Lean, WYaw + 180.f * B.Rand(s), 0.f).Vector() * (240.f + 120.f * B.Rand(s + 9));
				if ((s + (Sd > 0 ? 3 : 0)) % 7 != 0) K.Rod(Black(0.8f), F - FVector(0, 0, 120.f), TopP, 12.f);
				if (s % 3 && s < 24)
				{
					const FVector Q = Weir + FRotator(0.f, WYaw + Sd * 35.f, 0.f).Vector() * ((s + 1) * 140.f);
					const FVector QF = B.Top(Q.X, Q.Y);
					for (int32 w = 0; w < 4; ++w) K.Rod(ReedS(0.5f), F + FVector(0, 0, 20.f + w * 30.f), QF + FVector(0, 0, 26.f + w * 30.f), 3.f);
				}
			}
		}
		// The trap basket at the apex: hoops and staves, a cone of withies, burst.
		const FVector Bk = B.Top(Weir.X, Weir.Y);
		for (int32 h = 0; h < 4; ++h) K.Disc(ReedS(0.45f), Bk + FRotator(0.f, WYaw + 180.f, 0.f).Vector() * (h * 90.f) + FVector(0, 0, 70.f), FRotator(0.f, WYaw, 0.f).Vector(), 160.f - 30.f * h, 6.f);
		for (int32 s = 0; s < 10; ++s) if (s != 3) K.Rod(ReedS(0.4f), Bk + FVector(0, 0, 70.f) + FRotator(0.f, WYaw + 90.f, s * 36.f).RotateVector(FVector(0, 80.f, 0)), Bk + FRotator(0.f, WYaw + 180.f, 0.f).Vector() * 320.f + FVector(0, 0, 70.f), 2.f);
		return;
	}
}

// ------------------------------------------------------------------------------------------------ hand-made sites

/** The four landmark-foot sites (cook 2.4 #10, #13, #18, #23): defined at the end of this file. */
namespace WRSwampFootSites { bool Build(AWRSwampScenery& Act, FWRSwampBuild& B); }

void AWRSwampScenery::BuildSite(FWRSwampBuild& B)
{
	using namespace WRSwampSceneryDetail;
	FKit& K = B.K;
	const FName Kd = Kind;
	UMaterialInstanceDynamic* RustRock = B.bServer ? nullptr : RockMat(this, Rust * 0.9f);
	UMaterialInstanceDynamic* Lime = B.bServer ? nullptr : RockMat(this, Limestone * 0.9f);
	UMaterialInstanceDynamic* MudRock = B.bServer ? nullptr : RockMat(this, Mud * 0.6f);
	// Toward the Throat (the fetish posts and the bones face her: cook 2.20 beat 2).
	FVector ThroatWorld = FVector::ZeroVector;
	if (const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("vharra"))) ThroatWorld = ArenaCentre(Ar);
	else if (const FWRSwampData::FSite* S = FWRSwampData::Get().FindSite(TEXT("mothers_throat"))) ThroatWorld = S->At;
	const float ToThroat = B.LocalYaw((ThroatWorld - GetActorLocation()).Rotation().Yaw);
	const float West = B.LocalYaw(180.f);

	if (Kd == TEXT("rod_line"))
	{
		// The storm people's advance (cook 2.4 #1, 2.20 beat 7): nine verdigris rods in a line from Stormgut, a Rodwarden
		// skeleton leaning on each, leech pits in the bone, a Coven hook in the ribs; the ninth rod bent double.
		const float Gap = RowNum(Row, TEXT("spacing"), 75.f) * 100.f;
		FVector Dir = FVector::ForwardVector;
		if (Row.IsValid() && Row->HasField(TEXT("to"))) Dir = (B.ToLocal(WRSwamp::Point(Row, TEXT("to"))) * FVector(1, 1, 0)).GetSafeNormal(1.f, FVector::ForwardVector);
		for (int32 r = 0; r < 9; ++r)
		{
			const FVector P = Dir * (Gap * r) + FVector(0, 0, 0);
			const FVector Base = B.At(P.X, P.Y, -10.f);
			const float Lean = 4.f + 9.f * B.Rand(r);
			const float LYaw = 360.f * B.Rand(r + 11);
			B.Rod(Base, 700.f, Lean, LYaw, r == 8 ? 150.f : 0.f);
			const float SYaw = Dir.Rotation().Yaw + 180.f + 40.f * (B.Rand(r + 3) - 0.5f);
			const FVector Pelvis = Base + FRotator(0.f, SYaw, 0.f).Vector() * 70.f + FVector(0, 0, 95.f);
			B.Skeleton(K, Pelvis, SYaw + 180.f, 2, true, true);
			B.ScatterBones(P, 220.f, 5, r);
		}
		return;
	}
	if (Kd == TEXT("thorn_gate"))
	{
		// The Coven's pass fort (cook 2.4 #2): a palisade of driftwood and sharpened bone stakes round a yard, every
		// stake leaning out toward the gut (west); the gate on the west side burnt from outside, its timbers charred,
		// the ground before it glassed by lightning in a fern of scorch.
		const float R = RowNum(Row, TEXT("radius"), 15.f) * 100.f;
		const int32 N = 64;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 360.f * i / N;
			const FVector Out = FRotator(0.f, A, 0.f).Vector();
			const float Off = FMath::FindDeltaAngleDegrees(A, West);
			const bool bGate = FMath::Abs(Off) < 9.f;
			const FVector F = B.At(Out.X * R, Out.Y * R, -40.f);
			const FVector Lean = (FRotator(0.f, West, 0.f).Vector() * 0.55f + FVector(0, 0, 1.f)).GetSafeNormal();
			const bool bBone = i % 3 == 0;
			const FSurf S = bGate ? Char() : bBone ? BoneS(0.9f) : FSurf::Wood(C(140, 128, 110) * (FMath::Abs(Off) < 40.f ? 0.18f : 0.38f));
			if (bGate && i % 2) continue;
			K.Rod(S, F, F + Lean * (330.f + 60.f * B.Rand(i)), bBone ? 12.f : 20.f);
			K.Spike(S, F + Lean * (330.f + 60.f * B.Rand(i)), Lean, bBone ? 12.f : 20.f, 50.f);
			if (!bGate) B.Bar(F, F + Lean * 300.f, 14.f);
		}
		for (const float Rr : { 120.f, 230.f }) for (int32 i = 0; i < N; ++i)
		{
			const float A0 = 360.f * i / N, A1 = 360.f * (i + 1) / N;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(A0, West)) < 12.f) continue;
			const FVector P0 = FRotator(0.f, A0, 0.f).Vector() * R, P1 = FRotator(0.f, A1, 0.f).Vector() * R;
			K.Rod(FSurf::Wood(C(120, 110, 96) * 0.35f), B.At(P0.X, P0.Y, Rr), B.At(P1.X, P1.Y, Rr), 10.f);
		}
		// The burnt gate: two charred leaves fallen in, glassed sand fanning out west of it.
		const FVector GateAt = FRotator(0.f, West, 0.f).Vector() * R;
		for (const float Sd : { -1.f, 1.f })
		{
			K.Add(Char(), FVector(20.f, 160.f, 280.f), B.At(GateAt.X * 0.9f, GateAt.Y * 0.9f, 60.f) + FRotator(0.f, West + 90.f, 0.f).Vector() * Sd * 110.f, FRotator(70.f, West, Sd * 10.f));
		}
		for (int32 g = 0; g < 18; ++g)
		{
			const FVector P = GateAt + FRotator(0.f, West + (B.Rand(g) - 0.5f) * 70.f, 0.f).Vector() * (200.f + 90.f * g);
			FSurf Glz = FSurf::Glass(FLinearColor(0.06f, 0.06f, 0.07f), 0.f);
			K.Add(Glz, FVector(120.f + 40.f * B.Rand(g + 4), 14.f, 2.f), B.At(P.X, P.Y, 1.f), FRotator(0.f, West + 60.f * (B.Rand(g + 8) - 0.5f), 0.f));
		}
		// Inside: a lookout of lashed driftwood facing the gut, bone racks.
		const FVector Lk = B.At(-R * 0.4f * FMath::Cos(FMath::DegreesToRadians(West)), -R * 0.4f * FMath::Sin(FMath::DegreesToRadians(West)));
		for (int32 p = 0; p < 4; ++p) K.Rod(FSurf::Wood(C(140, 128, 110) * 0.35f), Lk + FRotator(0.f, 90.f * p + 45.f, 0.f).Vector() * 140.f, Lk + FRotator(0.f, 90.f * p + 45.f, 0.f).Vector() * 100.f + FVector(0, 0, 420.f), 14.f);
		K.Add(Planks(0.6f), FVector(240.f, 240.f, 8.f), Lk + FVector(0, 0, 400.f), FRotator(0.f, West, 0.f));
		B.Block(Lk + FVector(0, 0, 400.f), FVector(120.f, 120.f, 6.f), FRotator(0.f, West, 0.f));
		for (int32 r = 0; r < 3; ++r) for (int32 b = 0; b < 8; ++b) K.Rod(BoneS(0.9f), Lk + FVector(300.f, -200.f + 50.f * b, 60.f + 60.f * r), Lk + FVector(300.f, -180.f + 50.f * b, 110.f + 60.f * r), 3.f);
		return;
	}
	if (Kd == TEXT("ossuary"))
	{
		// The Hex-Warden's mound (cook 2.4 #3): the terrain carries its 25 m bulk; on it, packed bones and skulls set in
		// the peat over its slopes, crow skulls nailed in spirals, four totem sockets on its shoulders (the totems are the
		// boss's), and the stone basin of black dried blood on its flat top.
		const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("hex_warden"));
		const float Top = Ar.IsValid() && Ar->HasField(TEXT("top")) ? WRSwamp::Point(Ar, TEXT("top")).Z - GetActorLocation().Z : B.G(0, 0);
		for (int32 i = 0; i < 260; ++i)
		{
			const float A = i * 2.39996f;
			const float Dd = 4000.f * FMath::Sqrt((i + 0.5f) / 260.f);
			const FVector P = B.At(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 2.f);
			if (i % 6 == 0) K.Add(BoneS(), FVector(20.f, 16.f, 18.f), P + FVector(0, 0, 4.f), FRotator(-20.f, A * 57.f + ToThroat, 0.f), EWRLookMesh::Cylinder);
			else K.Rod(BoneS(0.85f + 0.2f * B.Rand(i)), P, P + FRotator(-6.f, A * 80.f, 0.f).Vector() * (40.f + 30.f * B.Rand(i + 1)), 3.f + 2.f * B.Rand(i + 2));
		}
		TArray<FVector> Sockets;
		if (Ar.IsValid()) for (const FVector& Tw : WRSwamp::Points(Ar, TEXT("totems"))) Sockets.Add(B.ToLocal(Tw));
		if (Sockets.Num() == 0) for (int32 t = 0; t < 4; ++t) Sockets.Add(B.At(FMath::Cos(t * PI * 0.5f + 0.4f) * 1400.f, FMath::Sin(t * PI * 0.5f + 0.4f) * 1400.f));
		for (int32 t = 0; t < Sockets.Num(); ++t)
		{
			const FVector S = B.At(Sockets[t].X, Sockets[t].Y);
			// A socket ring of set stones and long bones, and a spiral of crow skulls nailed into the mound round it.
			for (int32 s = 0; s < 10; ++s) K.Add(FSurf::Stone(Slate * 0.5f), FVector(40.f, 26.f, 30.f), S + FRotator(0.f, 36.f * s, 0.f).Vector() * 110.f + FVector(0, 0, 10.f), FRotator(0.f, 36.f * s, 0.f));
			for (int32 c = 0; c < 18; ++c)
			{
				const float A = c * 40.f + t * 23.f;
				const FVector Cp = B.At(S.X + FMath::Cos(FMath::DegreesToRadians(A)) * (160.f + 16.f * c), S.Y + FMath::Sin(FMath::DegreesToRadians(A)) * (160.f + 16.f * c), 8.f);
				FWRSwampBuild::CrowSkull(K, Cp, A + 180.f, 1.2f);
				K.Rod(IronS(), Cp - FVector(0, 0, 4.f), Cp + FVector(0, 0, 10.f), 1.5f);
			}
		}
		// The blood basin on the top: a stone bowl, black dried blood inside, runnels down its lip.
		const FVector Bs(0.f, 0.f, Top);
		K.Hex(FSurf::Stone(Slate * 0.55f), Bs.X, Bs.Y, Top - 10.f, Top + 70.f, 140.f);
		K.Disc(FSurf::Glaze(FLinearColor(0.02f, 0.006f, 0.006f), 0.35f), Bs + FVector(0, 0, 72.f), FVector::UpVector, 220.f, 4.f);
		for (int32 r = 0; r < 5; ++r) K.Add(FSurf::Glaze(FLinearColor(0.03f, 0.008f, 0.008f), 0.4f), FVector(4.f, 10.f, 60.f), Bs + FRotator(0.f, r * 72.f, 0.f).Vector() * 142.f + FVector(0, 0, 40.f), FRotator(0.f, r * 72.f, 0.f));
		B.Block(Bs + FVector(0, 0, 30.f), FVector(130.f, 130.f, 40.f));
		return;
	}
	if (Kd == TEXT("throat_stair"))
	{
		// The Throat Stair (cook 2.4 #4): bone paving down into the sinkhole, flanked by the Coven's tithe basins, each
		// with a stone channel cut down toward the water (every basin drains toward her: 2.20).
		TArray<FVector> Stair;
		if (const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("vharra"))) for (const FVector& P : WRSwamp::Points(Ar, TEXT("stair"))) Stair.Add(B.ToLocal(P));
		if (Stair.Num() < 2)
		{
			const FVector In = FRotator(0.f, ToThroat, 0.f).Vector();
			for (int32 i = 0; i < 6; ++i) Stair.Add(B.At(In.X * i * 600.f, In.Y * i * 600.f));
		}
		float Along = 0.f;
		int32 Basin = 0;
		for (int32 s = 0; s + 1 < Stair.Num(); ++s)
		{
			const FVector P0 = Stair[s], P1 = Stair[s + 1];
			const FVector Dir = (P1 - P0).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
			const float L = FVector::Dist2D(P0, P1);
			for (float d = 0.f; d < L; d += 60.f, Along += 60.f)
			{
				const FVector C0 = FMath::Lerp(P0, P1, d / L);
				for (int32 b = -2; b <= 2; ++b)
				{
					const FVector Q = C0 + Side * (b * 70.f) + Dir * (35.f * ((b + 2) % 2));
					K.Add(BoneS(0.8f + 0.25f * B.Rand(int32(Along) + b)), FVector(52.f, 64.f, 8.f), B.At(Q.X, Q.Y, 3.f), FRotator(0.f, Dir.Rotation().Yaw + 90.f, 0.f));
				}
				if (FMath::Fmod(Along, 600.f) < 60.f)
				{
					for (const float Sd : { -1.f, 1.f })
					{
						const FVector Bp = B.At(C0.X + Side.X * Sd * 300.f, C0.Y + Side.Y * Sd * 300.f);
						K.Hex(FSurf::Stone(Slate * 0.6f), Bp.X, Bp.Y, Bp.Z - 20.f, Bp.Z + 80.f, 70.f, 15.f);
						K.Disc(FSurf::Glaze(Oxblood * 0.25f, 0.3f), Bp + FVector(0, 0, 82.f), FVector::UpVector, 100.f, 3.f);
						// The channel: a stone trough stepping down beside the stair toward the water.
						for (int32 c = 1; c < 8; ++c)
						{
							const FVector Q = Bp + Dir * (c * 90.f) - Side * Sd * 60.f;
							const FVector QG = B.At(Q.X, Q.Y, 6.f);
							K.Add(FSurf::Stone(Slate * 0.45f), FVector(92.f, 30.f, 14.f), QG, FRotator(0.f, Dir.Rotation().Yaw, 0.f));
							K.Add(FSurf::Glaze(Oxblood * 0.2f, 0.3f), FVector(90.f, 10.f, 2.f), QG + FVector(0, 0, 7.f), FRotator(0.f, Dir.Rotation().Yaw, 0.f));
						}
						B.Block(Bp + FVector(0, 0, 30.f), FVector(60.f, 60.f, 50.f));
						++Basin;
					}
				}
			}
		}
		return;
	}
	if (Kd == TEXT("weeping_pools"))
	{
		// The Coven's smelting place under the falls (cook 2.4 #5): bog-iron crust on every stone, three cold clay
		// bloomeries cracked open, their tuyere holes, slag heaps gone glassy, a clay pit for the next charge.
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = i * 2.39996f;
			B.Rock(B.Seed + i, FVector(120.f + 60.f * (i % 3), 100.f + 40.f * (i % 2), 70.f + 30.f * (i % 4)), B.At(FMath::Cos(A) * (300.f + 70.f * i), FMath::Sin(A) * (300.f + 70.f * i)), FRotator(0.f, A * 57.f, 4.f), RustRock, true, 20.f);
		}
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector P = B.At(-300.f + 300.f * b, -500.f);
			for (int32 c = 0; c < 6; ++c) K.Disc(ClayS(0.8f + 0.06f * c), P + FVector(0, 0, 20.f + c * 30.f), FVector::UpVector, 150.f - 14.f * c, 32.f);
			K.Disc(Dark(), P + FVector(0, 0, 200.f), FVector::UpVector, 50.f, 6.f);
			K.Add(Dark(), FVector(30.f, 26.f, 30.f), P + FVector(70.f, 0, 40.f));                                        // the tuyere mouth
			K.Add(ClayS(0.6f), FVector(14.f, 70.f, 120.f), P + FVector(-60.f, 30.f, 80.f), FRotator(0.f, 30.f, 8.f));   // the crack
			for (int32 s = 0; s < 9; ++s) K.Add(FSurf::Glaze(C(40, 34, 30) * 0.4f, 0.25f), FVector(40.f, 32.f, 22.f), P + FVector(160.f + 30.f * (s % 3), -60.f + 40.f * (s / 3), 11.f + 10.f * (s % 2)), FRotator(10.f * s, 30.f * s, 0.f));
			B.Block(P + FVector(0, 0, 100.f), FVector(70.f, 70.f, 100.f));
		}
		K.Disc(ClayS(1.1f), B.At(400.f, -600.f, 2.f), FVector::UpVector, 320.f, 6.f);
		return;
	}
	if (Kd == TEXT("rookery"))
	{
		// Hexcrow Rookery (cook 2.4 #6): thirty dead cypress, the ground white with droppings and picked bones, a
		// Mudhulk's skull among them.
		for (int32 t = 0; t < 30; ++t)
		{
			const float A = t * 2.39996f;
			const float Dd = 3500.f * FMath::Sqrt((t + 0.5f) / 30.f);
			B.Cypress(K, B.At(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, -40.f), 1200.f + 900.f * B.Rand(t), 3.f + 8.f * B.Rand(t + 7), 360.f * B.Rand(t + 9), true, t, t % 3 == 0);
		}
		for (int32 w = 0; w < 70; ++w)
		{
			const float A = w * 2.39996f;
			const float Dd = 3200.f * FMath::Sqrt((w + 0.5f) / 70.f);
			K.Disc(FSurf::Stone(C(214, 210, 198) * 0.7f, 0.95f), B.At(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 1.f), FVector::UpVector, 260.f + 200.f * B.Rand(w), 2.f);
		}
		B.ScatterBones(FVector::ZeroVector, 2600.f, 80, 7);
		B.BeastSkull(B.At(300.f, 200.f, 0.f), 40.f, 1.5f, true);
		return;
	}
	if (Kd == TEXT("maw_nest"))
	{
		// Mother Maw's nest (cook 2.4 #7): a compost mound of rotting reeds and sticks (a real crocodile's nest is a
		// heap that cooks its eggs), broken eggshells, a newcomer's boot. The broken boardwalk square is the boss's.
		const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("mother_maw"));
		const FVector N = Ar.IsValid() && Ar->HasField(TEXT("nest")) ? B.ToLocal(WRSwamp::Point(Ar, TEXT("nest"))) : FVector::ZeroVector;
		const FVector Base = B.At(N.X, N.Y, -20.f);
		B.Mound(Base, 600.f, 220.f, FSurf::Cloth(C(52, 44, 30) * 0.6f), 0);
		for (int32 s = 0; s < 60; ++s)
		{
			const float A = s * 2.39996f, Hh = B.Rand(s + 3);
			const float Rl = 600.f * FMath::Sqrt(FMath::Max(0.05f, 1.f - Hh * Hh));
			const FVector On = Base + FVector(FMath::Cos(A) * Rl, FMath::Sin(A) * Rl, 220.f * Hh);
			if (s % 2) K.Rod(ReedS(0.5f), On, On + FRotator(20.f + 40.f * B.Rand(s), A * 57.f, 0.f).Vector() * 120.f, 3.f);
			else K.Rod(FSurf::Wood(C(60, 48, 36) * 0.5f), On, On + FRotator(10.f + 50.f * B.Rand(s), A * 57.f + 90.f, 0.f).Vector() * 90.f, 5.f);
		}
		for (int32 e = 0; e < 9; ++e)
		{
			const FVector P = B.At(N.X + FMath::Cos(e * 0.9f) * (650.f + 50.f * (e % 3)), N.Y + FMath::Sin(e * 0.9f) * (650.f + 50.f * (e % 3)), 4.f);
			K.Add(FSurf::Stone(C(226, 220, 204) * 0.7f, 0.6f), FVector(12.f, 9.f, 6.f), P, FRotator(30.f * e, 40.f * e, 60.f), EWRLookMesh::Cylinder);
		}
		// The boot: a leather upper and a sole, lying on its side at the waterline.
		const FVector Bt = B.Top(N.X + 760.f, N.Y + 120.f, 6.f);
		K.Add(LeatherS(0.8f), FVector(30.f, 12.f, 28.f), Bt + FVector(0, 0, 10.f), FRotator(0.f, 30.f, 80.f));
		K.Add(LeatherS(0.5f), FVector(30.f, 4.f, 12.f), Bt + FVector(10.f, -8.f, 4.f), FRotator(0.f, 30.f, 80.f));
		return;
	}
	if (Kd == TEXT("bloat_wallow"))
	{
		// The Bloat's Wallow (cook 2.4 #8): a Mudhulk drained to a husk, its skin collapsed onto its ribs.
		const FSurf Husk = LeatherS(0.7f);
		const FVector C0 = B.At(0, 0, 40.f);
		for (int32 r = 0; r < 9; ++r)
		{
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Sp = C0 + FVector(-200.f + 50.f * r, 0, 70.f);
				const FVector M = Sp + FVector(0, Sd * 130.f, -10.f), E = Sp + FVector(10.f, Sd * 170.f, -80.f);
				K.Rod(BoneS(0.9f), Sp, M, 9.f);
				K.Rod(BoneS(0.9f), M, E, 8.f);
				if (r % 2) K.Add(Husk, FVector(54.f, 8.f, 150.f), (Sp + E) * 0.5f + FVector(0, Sd * 6.f, 6.f), FRotator(0.f, 0.f, Sd * 55.f));
			}
		}
		K.Rod(BoneS(), C0 + FVector(-260.f, 0, 70.f), C0 + FVector(260.f, 0, 80.f), 16.f);
		K.Add(Husk, FVector(560.f, 300.f, 40.f), C0 + FVector(0, 0, 20.f), FRotator(0.f, 0.f, 4.f));
		B.BeastSkull(C0 + FVector(330.f, 0, -30.f), 0.f, 1.f, true);
		for (const float X : { -200.f, 160.f }) for (const float Sd : { -1.f, 1.f }) K.Rod(BoneS(0.9f), C0 + FVector(X, Sd * 120.f, 0.f), C0 + FVector(X + 60.f, Sd * 260.f, -30.f), 12.f);
		B.Block(C0 + FVector(0, 0, 40.f), FVector(280.f, 150.f, 60.f));
		return;
	}
	if (Kd == TEXT("deep_pool"))
	{
		// The Deep Pool (cook 2.4 #9, the Sunken Coven's first way in): a round black pool 40 m across roofed by a
		// floating peat mat with a gap at its middle; three cypress over it with iron cages whose chains run down into
		// the water.
		const FWRSwampData& D = FWRSwampData::Get();
		const FVector Cn = !D.DeepPool.IsZero() ? B.ToLocal(D.DeepPool) * FVector(1, 1, 0) : FVector::ZeroVector;
		const float R = RowNum(Row, TEXT("radius"), 20.f) * 100.f, Gap = WRTune(TEXT("swampWorld.deepPoolGap"), 300.f);
		const float Mz = B.WaterZ + 6.f;
		for (int32 ring = 0; ring < 6; ++ring)
		{
			const float R0 = FMath::Lerp(Gap, R * 0.9f, ring / 6.f), R1 = FMath::Lerp(Gap, R * 0.9f, (ring + 1) / 6.f);
			const int32 Segs = 10 + ring * 5;
			for (int32 s = 0; s < Segs; ++s)
			{
				const float A = 360.f * (s + 0.5f) / Segs;
				const FVector P = Cn + FRotator(0.f, A, 0.f).Vector() * ((R0 + R1) * 0.5f) + FVector(0, 0, Mz);
				const float W = 2.f * PI * (R0 + R1) * 0.5f / Segs + 8.f;
				K.Add(PeatS(0.8f + 0.4f * B.Rand(s + ring * 40)), FVector(R1 - R0 + 10.f, W, 14.f), P, FRotator(0.f, A, 0.f));
				if ((s + ring) % 3 == 0) K.Add(MossS(), FVector(60.f, 40.f, 10.f), P + FVector(0, 0, 9.f), FRotator(0.f, A * 3.f, 0.f), EWRLookMesh::Cylinder);
				if ((s + ring) % 5 == 0) K.Add(GarnetMoss(0.7f), FVector(50.f, 36.f, 8.f), P + FVector(20.f, 0, 9.f), FRotator(0.f, A * 5.f, 0.f), EWRLookMesh::Cylinder);
				B.Block(P, FVector((R1 - R0) * 0.5f, W * 0.5f, 7.f), FRotator(0.f, A, 0.f));
			}
		}
		for (int32 t = 0; t < 3; ++t)
		{
			const float A = 120.f * t + 30.f;
			const FVector Foot = B.At(Cn.X + FMath::Cos(FMath::DegreesToRadians(A)) * (R + 400.f), Cn.Y + FMath::Sin(FMath::DegreesToRadians(A)) * (R + 400.f), -30.f);
			B.Cypress(K, Foot, 2200.f + 400.f * t, 10.f, A + 180.f, t == 1, t + 40);
			// A limb out over the water, the cage hanging from it, its chain running on down through the mat.
			const FVector Limb0 = Foot + FVector(0, 0, 1400.f), Limb1 = FMath::Lerp(Limb0, FVector(Cn.X, Cn.Y, Limb0.Z), 0.55f);
			K.Rod(Bark(), Limb0, Limb1, 30.f);
			FKit& Hang = B.Swing(Limb1);
			FWRSwampBuild::Cage(Hang, FVector::ZeroVector, 700.f, 180.f, 44.f, t == 0);
			FWRSwampBuild::Chain(Hang, FVector(0, 0, -700.f - 180.f), FVector(0, 0, Mz - Limb1.Z - 300.f));
		}
		return;
	}
	if (Kd == TEXT("peat_cut"))
	{
		// The Peat Cut (cook 2.4 #15, 2.20 beat 3): a cutters' trench 60 m long in a raised-bog dome, its cut faces in
		// dark courses; a spade left standing in the turf; turf stacks; and three bog bodies laid side by side, a braided
		// cord at each neck, their faces calm. The fourth (M-S3) is under the dome's edge.
		const float L = RowNum(Row, TEXT("length"), 60.f) * 100.f;
		for (int32 s = 0; s < 30; ++s)
		{
			const float X = -L * 0.5f + L * (s + 0.5f) / 30.f;
			for (const float Sd : { -1.f, 1.f })
			{
				const FVector Face = B.At(X, Sd * 170.f, 0.f);
				for (int32 c = 0; c < 3; ++c) K.Add(PeatS(0.7f + 0.15f * c + 0.1f * B.Rand(s)), FVector(L / 30.f + 2.f, 12.f, 40.f), Face + FVector(0, -Sd * 4.f * c, -20.f - 40.f * c), FRotator::ZeroRotator);
			}
		}
		const FVector Sp = B.At(L * 0.3f, 120.f, 0.f);
		K.Rod(FSurf::Wood(C(120, 96, 70) * 0.45f), Sp + FVector(0, 0, 10.f), Sp + FVector(0, 0, 150.f), 4.f);
		K.Rod(FSurf::Wood(C(120, 96, 70) * 0.45f), Sp + FVector(0, -14.f, 148.f), Sp + FVector(0, 14.f, 148.f), 3.f);
		K.Add(IronS(), FVector(3.f, 22.f, 34.f), Sp + FVector(0, 0, -6.f), FRotator(0.f, 0.f, 0.f));
		for (int32 t = 0; t < 4; ++t) B.TurfStack(B.At(-L * 0.4f + 300.f * t, -420.f, 0.f), 20.f * t, 5);
		for (int32 b = 0; b < 3; ++b) B.BogBody(B.At(-200.f, -60.f + 70.f * b, 2.f) + FVector(0, 0, -B.G(-200.f, 0.f) + B.G(-200.f, 0.f)), 90.f, b);
		return;
	}
	if (Kd == TEXT("bloodmoss_dome"))
	{
		// Bloodmoss Dome (cook 2.4 #16): a raised bog 200 m across carpeted garnet, springy, ringed by pitcher plants.
		const float R = RowNum(Row, TEXT("radius"), 100.f) * 100.f;
		for (int32 i = 0; i < 180; ++i)
		{
			const float A = i * 2.39996f;
			const float Dd = R * 0.95f * FMath::Sqrt((i + 0.5f) / 180.f);
			K.Add(GarnetMoss(0.75f + 0.5f * B.Rand(i)), FVector(500.f + 300.f * B.Rand(i + 1), 380.f + 200.f * B.Rand(i + 2), 10.f), B.At(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 2.f), FRotator(0.f, A * 57.f, 0.f), EWRLookMesh::Cylinder);
		}
		for (int32 p = 0; p < 36; ++p)
		{
			const float A = p * 10.f + 4.f * B.Rand(p);
			B.Pitchers(FVector(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 0.f), 4 + p % 3);
		}
		return;
	}
	if (Kd == TEXT("drowned_causeway"))
	{
		// The Coven's main line toward the Throat (its planks are the boardwalk's): at its coast end two tall gateposts of
		// lashed bone with crow skulls facing in, and a bone lantern.
		for (const float Sd : { -1.f, 1.f }) B.FetishPost(B.At(0.f, Sd * 180.f), ToThroat, 1);
		return;
	}
	if (Kd == TEXT("stilt_village"))
	{
		// The Stilt Village (cook 2.4 #19, 2.20 beat 1): an ordinary life left behind; six stilt huts, every door barred
		// and hex-marked from OUTSIDE; rice paddies gone to reeds; a slime still, still dripping.
		const int32 Huts = (int32)RowNum(Row, TEXT("huts"), 6.f);
		for (int32 h = 0; h < Huts; ++h)
		{
			const float A = 360.f * h / Huts + 20.f * B.Rand(h);
			const FVector P = FRotator(0.f, A, 0.f).Vector() * (1500.f + 400.f * B.Rand(h + 3));
			B.StiltHut(FVector(P.X, P.Y, 0.f), A + 180.f, 0, true);
		}
		// Plank walks hut to hut.
		for (int32 h = 0; h < Huts; ++h)
		{
			const FVector P0 = FRotator(0.f, 360.f * h / Huts, 0.f).Vector() * 1200.f, P1 = FRotator(0.f, 360.f * (h + 1) / Huts, 0.f).Vector() * 1200.f;
			for (int32 s = 0; s < 8; ++s)
			{
				const FVector Q = FMath::Lerp(P0, P1, (s + 0.5f) / 8.f);
				K.Add(Planks(0.8f + 0.3f * B.Rand(s + h * 8)), FVector(FVector::Dist(P0, P1) / 8.f - 6.f, 60.f, 5.f), B.Top(Q.X, Q.Y, 40.f), FRotator(0.f, (P1 - P0).Rotation().Yaw, 0.f));
			}
		}
		// The paddies: low mud bunds in a grid, wild bog rice and reeds grown through them.
		for (int32 px = 0; px < 3; ++px) for (int32 py = 0; py < 2; ++py)
		{
			const FVector Pc(2400.f + px * 900.f, -500.f + py * 1000.f, 0.f);
			for (const float Sd : { -1.f, 1.f })
			{
				K.Add(MudS(), FVector(900.f, 50.f, 40.f), B.At(Pc.X, Pc.Y + Sd * 450.f, 0.f));
				K.Add(MudS(), FVector(50.f, 900.f, 40.f), B.At(Pc.X + Sd * 450.f, Pc.Y, 0.f));
			}
			for (int32 r = 0; r < 30; ++r)
			{
				const FVector Q = B.Top(Pc.X - 400.f + 800.f * B.Rand(r + px * 30), Pc.Y - 400.f + 800.f * B.Rand(r + py * 60 + 7), -15.f);
				K.Rod(FSurf::Cloth(C(110, 128, 60) * 0.55f), Q, Q + FRotator(75.f, 37.f * r, 0.f).Vector() * 100.f, 1.6f);
				K.Add(FSurf::Cloth(C(180, 160, 90) * 0.5f), FVector(3.f, 3.f, 22.f), Q + FRotator(75.f, 37.f * r, 0.f).Vector() * 100.f + FVector(4.f, 0, -8.f), FRotator(-40.f, 37.f * r, 0.f), EWRLookMesh::Cylinder);
			}
			B.Reeds(Pc + FVector(300.f, 300.f, 0.f), 200.f, 16, 220.f);
		}
		// The slime still: a vat on legs, a copper coil, the drip into a pot, pink slime crusted down its side.
		const FVector St = B.At(-400.f, 300.f);
		for (int32 l = 0; l < 4; ++l) K.Rod(Black(), St + FRotator(0.f, 90.f * l + 45.f, 0.f).Vector() * 70.f, St + FRotator(0.f, 90.f * l + 45.f, 0.f).Vector() * 60.f + FVector(0, 0, 120.f), 8.f);
		K.Add(Planks(0.7f), FVector(130.f, 130.f, 110.f), St + FVector(0, 0, 175.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 h = 0; h < 3; ++h) K.Disc(IronS(), St + FVector(0, 0, 135.f + 35.f * h), FVector::UpVector, 136.f, 5.f);
		for (int32 c = 0; c < 8; ++c) K.Rod(FSurf::Steel(C(184, 115, 51) * 0.5f, 0.4f), St + FVector(80.f + 10.f * c, 0, 200.f - 14.f * c) + FRotator(0.f, c * 90.f, 0.f).Vector() * 20.f, St + FVector(90.f + 10.f * c, 0, 186.f - 14.f * c) + FRotator(0.f, (c + 1) * 90.f, 0.f).Vector() * 20.f, 2.5f);
		FSurf Slime = FSurf::Glaze(Pink * 0.55f, 0.15f);
		Slime.Glow = 0.08f;
		K.Add(Slime, FVector(6.f, 6.f, 18.f), St + FVector(170.f, 0, 70.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(ClayS(0.9f), FVector(50.f, 50.f, 45.f), St + FVector(170.f, 0, 22.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Disc(Slime, St + FVector(170.f, 0, 46.f), FVector::UpVector, 40.f, 2.f);
		K.Add(Slime, FVector(8.f, 30.f, 70.f), St + FVector(62.f, 0, 170.f), FRotator(0.f, 0.f, 0.f));
		if (USceneComponent* Drip = B.Fx(TEXT("swamp.drip"), St + FVector(170.f, 0, 90.f), 0.05f)) { (void)Drip; }
		B.Block(St + FVector(0, 0, 120.f), FVector(70.f, 70.f, 120.f));
		return;
	}
	if (Kd == TEXT("clay_pit"))
	{
		// The Clay Pit (cook 2.4 #20): wet clay banks, a potter's kick wheel, a cold dome kiln, fired pots stacked.
		for (int32 i = 0; i < 8; ++i) B.Rock(B.Seed + i, FVector(260.f, 200.f, 80.f), B.At(FMath::Cos(i * 0.8f) * 700.f, FMath::Sin(i * 0.8f) * 500.f), FRotator(0.f, i * 45.f, 2.f), MudRock, false, 30.f);
		const FVector W = B.At(300.f, 200.f);
		K.Add(FSurf::Stone(Slate * 0.6f), FVector(100.f, 100.f, 14.f), W + FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Rod(Black(), W + FVector(0, 0, 16.f), W + FVector(0, 0, 80.f), 8.f);
		K.Add(ClayS(0.8f), FVector(60.f, 60.f, 8.f), W + FVector(0, 0, 82.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(ClayS(1.f), FVector(30.f, 30.f, 34.f), W + FVector(0, 0, 100.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Planks(0.6f), FVector(50.f, 120.f, 8.f), W + FVector(-90.f, 0, 40.f));
		const FVector Kn = B.At(-300.f, 400.f);
		for (int32 c = 0; c < 6; ++c) K.Disc(ClayS(0.7f), Kn + FVector(0, 0, 20.f + c * 32.f), FVector::UpVector, 280.f - 36.f * c, 34.f);
		K.Add(Dark(), FVector(20.f, 70.f, 80.f), Kn + FVector(130.f, 0, 50.f));
		B.Block(Kn + FVector(0, 0, 90.f), FVector(120.f, 120.f, 90.f));
		const FSurf Fired = FSurf::Stone(C(170, 90, 56) * 0.5f, 0.75f);
		for (int32 p = 0; p < 11; ++p)
		{
			const FVector P = B.At(-100.f + 50.f * (p % 4), -350.f + 60.f * (p / 4), 0.f) + FVector(0, 0, p >= 8 ? 50.f : 0.f);
			K.Add(Fired, FVector(36.f, 36.f, 44.f), P + FVector(0, 0, 22.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Disc(Fired.Dim(1.1f), P + FVector(0, 0, 46.f), FVector::UpVector, 30.f, 6.f);
		}
		return;
	}
	if (Kd == TEXT("salt_lick"))
	{
		// The Salt Lick (cook 2.4 #21): a brackish pan crusted white, cracked into plates, licked smooth in places; a
		// salt-rake broken at the handle.
		for (int32 i = 0; i < 70; ++i)
		{
			const float A = i * 2.39996f;
			const float Dd = 1500.f * FMath::Sqrt((i + 0.5f) / 70.f);
			const FVector P = B.At(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 1.f);
			K.Add(SaltS(), FVector(160.f + 90.f * B.Rand(i), 120.f + 60.f * B.Rand(i + 1), 3.f), P, FRotator(0.f, A * 57.f, 0.f));
			if (i % 2) K.Add(FSurf::Stone(Mud * 0.25f, 0.9f), FVector(150.f, 3.f, 3.5f), P + FVector(0, 0, 0.5f), FRotator(0.f, A * 57.f + 40.f, 0.f));
		}
		const FVector Rk = B.At(400.f, -250.f, 4.f);
		K.Rod(FSurf::Wood(C(140, 128, 110) * 0.4f), Rk, Rk + FVector(150.f, 30.f, 6.f), 5.f);
		K.Rod(FSurf::Wood(C(140, 128, 110) * 0.4f), Rk + FVector(170.f, 40.f, 2.f), Rk + FVector(260.f, 70.f, 2.f), 5.f);
		K.Add(FSurf::Wood(C(140, 128, 110) * 0.4f), FVector(10.f, 90.f, 8.f), Rk + FVector(-6.f, 0, 2.f));
		for (int32 t = 0; t < 7; ++t) if (t != 2 && t != 5) K.Add(FSurf::Wood(C(140, 128, 110) * 0.4f), FVector(18.f, 3.f, 3.f), Rk + FVector(-16.f, -40.f + 13.f * t, 1.f));
		return;
	}
	if (Kd == TEXT("estuary_wreck"))
	{
		// The Estuary Wreck (cook 2.4 #22): a newcomer ship sunk to its rails in the mangroves; its ribs and planking under
		// the water, the rails and the stern cabin's roof just above; the mast down across the mud; mangroves grown through
		// the deck; a Murkjaw's nest heaped under the stern. The salvage chest is in the cabin (swim in).
		const float Wz = B.WaterZ;
		const FSurf Hull = Planks(0.55f);
		for (int32 r = 0; r < 14; ++r)
		{
			const float X = -1000.f + r * 150.f;
			const float Wd = 380.f * FMath::Sin(PI * (r + 1) / 15.f);
			for (const float Sd : { -1.f, 1.f })
			{
				K.Rod(Hull, FVector(X, 0, Wz - 280.f), FVector(X, Sd * Wd, Wz + 30.f), 18.f);
				K.Add(Hull.Dim(0.9f), FVector(150.f, 8.f, 120.f), FVector(X, Sd * Wd * 0.95f, Wz - 40.f), FRotator(0.f, 0.f, Sd * 10.f));
			}
		}
		for (const float Sd : { -1.f, 1.f })
		{
			FVector Prev(-1000.f, 0, Wz + 30.f);
			for (int32 r = 0; r <= 14; ++r)
			{
				const FVector P(-1000.f + r * 150.f, Sd * 380.f * FMath::Sin(PI * (r + 1) / 15.f), Wz + 40.f);
				if (r > 0 && r != 6) K.Rod(Hull.Dim(1.1f), Prev, P, 14.f);
				Prev = P;
			}
		}
		// The stern cabin (aft, -X): walls up out of the water, a roof, the door gap facing forward.
		const FVector Cab(-700.f, 0, Wz);
		for (const float Sd : { -1.f, 1.f }) K.Add(Hull, FVector(320.f, 10.f, 240.f), Cab + FVector(0, Sd * 220.f, 40.f));
		K.Add(Hull, FVector(10.f, 440.f, 240.f), Cab + FVector(-160.f, 0, 40.f));
		for (const float Sd : { -1.f, 1.f }) K.Add(Hull, FVector(10.f, 140.f, 240.f), Cab + FVector(160.f, Sd * 150.f, 40.f));
		K.Add(Hull.Dim(0.8f), FVector(340.f, 460.f, 10.f), Cab + FVector(0, 0, 165.f), FRotator(0.f, 0.f, 4.f));
		B.Block(Cab + FVector(0, 0, 170.f), FVector(170.f, 230.f, 8.f));
		for (const float Sd : { -1.f, 1.f }) B.Block(Cab + FVector(0, Sd * 220.f, 40.f), FVector(160.f, 8.f, 120.f));
		B.Block(Cab + FVector(-160.f, 0, 40.f), FVector(8.f, 220.f, 120.f));
		CacheSpot = Cab + FVector(-60.f, 0, 10.f);
		// The mast across the mud, its yard snapped.
		K.Rod(FSurf::Wood(C(100, 84, 64) * 0.4f), FVector(100.f, 0, Wz + 60.f), B.At(1900.f, 900.f, 30.f), 34.f);
		K.Rod(FSurf::Wood(C(100, 84, 64) * 0.4f), B.At(1200.f, 300.f, 60.f), B.At(1300.f, 900.f, 20.f), 18.f);
		// Mangroves through the deck, and the Murkjaw nest under the stern.
		B.Mangrove(FVector(-200.f, 120.f, Wz), 900.f, 1);
		B.Mangrove(FVector(450.f, -150.f, Wz), 700.f, 2);
		B.Mangrove(FVector(1300.f, -700.f, B.G(1300.f, -700.f)), 1100.f, 3);
		B.Mound(B.At(-1250.f, -100.f, -20.f), 300.f, 120.f, FSurf::Cloth(C(60, 50, 34) * 0.55f), 6);
		return;
	}
	if (Kd == TEXT("gar_weir"))
	{
		// Gar Weir (cook 2.4 #24): a woven fish weir across the channel (bog eels in it), and on the bank a gar's skeleton
		// 2.5 m long: the long needle jaws, the armoured skull, the ribs and the fin rays.
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 s = 0; s < 14; ++s)
			{
				const FVector P = FRotator(0.f, Sd * 30.f, 0.f).Vector() * (s * 110.f);
				const FVector F = B.Top(P.X, P.Y);
				K.Rod(Black(0.9f), FVector(P.X, P.Y, B.G(P.X, P.Y) - 60.f), FVector(P.X, P.Y, F.Z + 140.f), 9.f);
				if (s < 13) for (int32 w = 0; w < 5; ++w)
				{
					const FVector Q = FRotator(0.f, Sd * 30.f, 0.f).Vector() * ((s + 1) * 110.f);
					K.Rod(ReedS(0.55f), FVector(P.X, P.Y, F.Z - 40.f + w * 30.f), FVector(Q.X, Q.Y, F.Z - 34.f + w * 30.f), 2.5f);
				}
			}
		}
		const FVector Gb = B.At(300.f, 900.f, 6.f);
		const float GarLen = WRTune(TEXT("swampWorld.garLength"), 250.f);
		K.Spike(BoneS(1.05f), Gb + FVector(GarLen * 0.3f, 0, 4.f), FVector::ForwardVector, 14.f, GarLen * 0.32f);
		K.Spike(BoneS(1.0f), Gb + FVector(GarLen * 0.3f, 0, -2.f), FVector(1.f, 0.f, -0.06f), 10.f, GarLen * 0.3f);
		for (int32 t = 0; t < 14; ++t) K.Spike(BoneS(1.1f), Gb + FVector(GarLen * 0.32f + t * GarLen * 0.02f, (t % 2 ? 4.f : -4.f), 0.f), FVector(0, 0, -1), 2.f, 6.f);
		K.Add(BoneS(), FVector(GarLen * 0.12f, 26.f, 22.f), Gb + FVector(GarLen * 0.22f, 0, 6.f));
		for (int32 v = 0; v < 30; ++v)
		{
			const FVector V = Gb + FVector(GarLen * 0.16f - v * GarLen * 0.022f, 0, 6.f);
			K.Add(BoneS(0.95f), FVector(5.f, 7.f, 7.f), V, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			if (v < 16) for (const float Sd : { -1.f, 1.f }) K.Rod(BoneS(0.9f), V, V + FVector(-4.f, Sd * 16.f, -6.f), 1.2f);
			if (v % 4 == 0) K.Rod(BoneS(0.9f), V, V + FVector(-6.f, 0, 14.f), 1.f);
		}
		for (int32 f = 0; f < 6; ++f) K.Rod(BoneS(0.9f), Gb + FVector(-GarLen * 0.45f, 0, 6.f), Gb + FVector(-GarLen * 0.55f, -20.f + 8.f * f, 6.f + (f % 2) * 6.f), 1.2f);
		return;
	}
	if (Kd == TEXT("leech_pond"))
	{
		// The Leech Pond (KEPT set-piece): a bleedwater pool ringed with reeds and a stake at its edge where a pair of
		// sealed waders hang (the waders are the pickup the world spawns on its peg); a deer drained white in the shallows.
		const FVector Post = B.At(250.f, 0.f);
		K.Rod(Black(), Post - FVector(0, 0, 60.f), Post + FVector(0, 0, 210.f), 14.f);
		K.Rod(Black(1.2f), Post + FVector(0, 0, 190.f), Post + FVector(40.f, 0, 196.f), 6.f);
		K.Add(IronS(), FVector(6.f, 6.f, 14.f), Post + FVector(40.f, 0, 190.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 c = 0; c < 6; ++c) B.Reeds(FVector(FMath::Cos(c * 1.05f) * 900.f, FMath::Sin(c * 1.05f) * 900.f, 0.f), 220.f, 18, 210.f);
		const FVector Deer = B.Top(-300.f, 200.f, -10.f);
		K.Add(FSurf::Stone(C(200, 190, 176) * 0.55f, 0.7f), FVector(110.f, 46.f, 40.f), Deer, FRotator(0.f, 30.f, 10.f), EWRLookMesh::Cylinder);
		K.Rod(FSurf::Stone(C(200, 190, 176) * 0.55f, 0.7f), Deer + FVector(50.f, 30.f, 10.f), Deer + FVector(100.f, 60.f, 40.f), 12.f);
		for (int32 l = 0; l < 4; ++l) K.Rod(BoneS(0.9f), Deer + FVector(-40.f + 25.f * l, (l % 2 ? 20.f : -20.f), -10.f), Deer + FVector(-60.f + 35.f * l, (l % 2 ? 70.f : -70.f), -30.f), 4.f);
		return;
	}
	if (Kd == TEXT("frozen_bog"))
	{
		// The Frost border (cook 2.4 #12): bubbles stopped in clear ice, a Reedfowl frozen mid-step, a Rime-folk inuksuk
		// on a Coven bone mound (the neighbours touched), cypress knees iced white.
		FSurf Ice = FSurf::Glass(FLinearColor(0.55f, 0.66f, 0.74f) * 0.6f, 0.f);
		const FSurf Bubble = FSurf::Stone(FLinearColor(0.85f, 0.9f, 0.95f) * 0.75f, 0.2f);
		for (int32 i = 0; i < 40; ++i)
		{
			const float A = i * 2.39996f;
			const float Dd = 1400.f * FMath::Sqrt((i + 0.5f) / 40.f);
			const FVector P = B.Top(FMath::Cos(A) * Dd, FMath::Sin(A) * Dd, 2.f);
			K.Add(Ice, FVector(320.f, 260.f, 4.f), P, FRotator(0.f, A * 57.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 b = 0; b < 4; ++b) K.Add(Bubble, FVector(10.f + 14.f * B.Rand(i * 4 + b), 10.f + 14.f * B.Rand(i * 4 + b), 3.f), P + FVector(-60.f + 40.f * b, 30.f * FMath::Sin(b * 2.f), -2.f - 8.f * (b % 3)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		// The Reedfowl in the ice: legs mid-stride, its neck forward, frosted white.
		const FVector Bd = B.Top(200.f, -150.f, 0.f);
		const FSurf Frost = FSurf::Stone(FLinearColor(0.8f, 0.84f, 0.88f) * 0.7f, 0.5f);
		K.Rod(Frost, Bd + FVector(0, 6.f, 0), Bd + FVector(8.f, 6.f, 70.f), 2.f);
		K.Rod(Frost, Bd + FVector(24.f, -6.f, 10.f), Bd + FVector(8.f, -6.f, 70.f), 2.f);
		K.Add(Frost, FVector(46.f, 24.f, 26.f), Bd + FVector(10.f, 0, 86.f), FRotator(-15.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		K.Rod(Frost, Bd + FVector(26.f, 0, 92.f), Bd + FVector(56.f, 0, 130.f), 5.f);
		K.Spike(Frost, Bd + FVector(56.f, 0, 130.f), FVector(1.f, 0, -0.2f), 4.f, 16.f);
		K.Add(Ice, FVector(110.f, 70.f, 150.f), Bd + FVector(20.f, 0, 70.f));
		// The inuksuk on its bone mound.
		const FVector Mn = B.At(-400.f, 300.f, -10.f);
		B.Mound(Mn, 260.f, 110.f, OldBone(), 14);
		const FVector Ik = Mn + FVector(0, 0, 110.f);
		UMaterialInstanceDynamic* Granite = B.bServer ? nullptr : RockMat(this, FLinearColor(0.42f, 0.43f, 0.45f));
		B.Rock(1, FVector(50.f, 40.f, 70.f), Ik + FVector(0, -40.f, 0), FRotator::ZeroRotator, Granite, true, 0.f);
		B.Rock(2, FVector(50.f, 40.f, 70.f), Ik + FVector(0, 40.f, 0), FRotator::ZeroRotator, Granite, true, 0.f);
		B.Rock(3, FVector(70.f, 150.f, 30.f), Ik + FVector(0, 0, 70.f), FRotator::ZeroRotator, Granite, false, 0.f);
		B.Rock(4, FVector(50.f, 60.f, 60.f), Ik + FVector(0, 0, 100.f), FRotator::ZeroRotator, Granite, false, 0.f);
		B.Rock(5, FVector(40.f, 180.f, 24.f), Ik + FVector(0, 0, 160.f), FRotator::ZeroRotator, Granite, false, 0.f);
		B.Rock(0, FVector(36.f, 36.f, 36.f), Ik + FVector(0, 0, 184.f), FRotator::ZeroRotator, Granite, false, 0.f);
		for (int32 k = 0; k < 10; ++k) K.Spike(Frost, B.At(FMath::Cos(k * 0.9f) * (500.f + 60.f * k), FMath::Sin(k * 0.9f) * (400.f + 50.f * k), -10.f), FVector::UpVector, 28.f, 50.f + 10.f * (k % 3));
		return;
	}
	if (Kd == TEXT("tithe_mound"))
	{
		// The Tithe Mound (cook 2.4 #14): a low bone mound with a stone tithe basin and a ring of fetish posts facing the
		// Throat. The Great Shrine is the world's (its bronze bowl brazier).
		B.Mound(B.At(0, 0, -30.f), 1300.f, 300.f, OldBone(), 90);
		const FVector Bs = B.At(-600.f, 0.f, 0.f);
		K.Hex(FSurf::Stone(Slate * 0.6f), Bs.X, Bs.Y, Bs.Z - 10.f, Bs.Z + 90.f, 110.f);
		K.Disc(FSurf::Glaze(Oxblood * 0.3f, 0.3f), Bs + FVector(0, 0, 92.f), FVector::UpVector, 170.f, 3.f);
		B.Block(Bs + FVector(0, 0, 40.f), FVector(100.f, 100.f, 50.f));
		for (int32 p = 0; p < 9; ++p)
		{
			const float A = 40.f * p;
			B.FetishPost(B.At(FMath::Cos(FMath::DegreesToRadians(A)) * 1700.f, FMath::Sin(FMath::DegreesToRadians(A)) * 1700.f), ToThroat, p % 3);
		}
		return;
	}
	if (Kd == TEXT("camp"))
	{
		// A newcomers' camp (world-map camps; R54's lights): a plank platform on posts over the wet, a stone fire pit with
		// its fire, two lantern poles, a tarp lean-to, crates and a drying rack for fish.
		const float Fz = FMath::Max(B.G(0, 0), B.WaterZ) + 60.f;
		for (int32 x = 0; x < 4; ++x) for (int32 y = 0; y < 4; ++y)
		{
			const FVector P(-450.f + 300.f * x, -450.f + 300.f * y, 0.f);
			K.Rod(Black(), FVector(P.X, P.Y, B.G(P.X, P.Y) - 80.f), FVector(P.X, P.Y, Fz), 16.f);
		}
		for (int32 b = 0; b < 40; ++b) K.Add(Planks(1.f + 0.25f * B.Rand(b)), FVector(24.f, 1000.f, 5.f), FVector(-490.f + 25.f * b, 0, Fz + 3.f));
		B.Block(FVector(0, 0, Fz), FVector(500.f, 500.f, 6.f));
		const FVector Fire(0, 0, Fz + 6.f);
		for (int32 i = 0; i < 10; ++i) K.Add(FSurf::Stone(Slate * 0.7f), FVector(26.f, 18.f, 16.f), Fire + FRotator(0.f, 36.f * i, 0.f).Vector() * 60.f + FVector(0, 0, 8.f), FRotator(0.f, 36.f * i, 0.f));
		K.Add(FSurf::Stone(Slate * 0.5f), FVector(140.f, 140.f, 4.f), Fire, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Fx(TEXT("held.torch"), Fire + FVector(0, 0, 14.f), 3.f);
		B.Light(Fire + FVector(0, 0, 80.f), FLinearColor(1.f, 0.55f, 0.25f), WRTune(TEXT("swampWorld.campFire"), 5000.f), 1600.f, true);
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Pole(Sd * 420.f, Sd * 420.f, Fz);
			K.Rod(Black(1.2f), Pole, Pole + FVector(0, 0, 260.f), 8.f);
			K.Rod(Black(1.2f), Pole + FVector(0, 0, 250.f), Pole + FVector(-Sd * 50.f, 0, 260.f), 5.f);
			K.Add(FSurf::Glass(FLinearColor(0.9f, 0.6f, 0.3f) * 0.6f, 0.9f), FVector(16.f, 16.f, 24.f), Pole + FVector(-Sd * 50.f, 0, 230.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Light(Pole + FVector(-Sd * 50.f, 0, 225.f), FLinearColor(1.f, 0.62f, 0.3f), WRTune(TEXT("swampWorld.campLantern"), 2200.f), 1100.f, true);
		}
		for (const float Y : { -200.f, 200.f }) K.Rod(Black(), FVector(-300.f, Y, Fz), FVector(-300.f, Y, Fz + 210.f), 9.f);
		K.Add(FSurf::Cloth(FLinearColor(0.3f, 0.26f, 0.18f)), FVector(300.f, 440.f, 4.f), FVector(-180.f, 0, Fz + 130.f), FRotator(-40.f, 0.f, 0.f));
		for (int32 c = 0; c < 3; ++c) K.Add(Planks(0.9f), FVector(80.f, 70.f, 60.f), FVector(-380.f, -300.f + 90.f * c, Fz + 30.f + (c == 1 ? 60.f : 0.f)), FRotator(0.f, 10.f * c, 0.f));
		K.Rod(Black(), FVector(300.f, -300.f, Fz), FVector(300.f, -300.f, Fz + 180.f), 6.f);
		K.Rod(Black(), FVector(300.f, 0.f, Fz), FVector(300.f, 0.f, Fz + 180.f), 6.f);
		K.Rod(Black(), FVector(300.f, -300.f, Fz + 170.f), FVector(300.f, 0.f, Fz + 170.f), 4.f);
		for (int32 f = 0; f < 6; ++f) K.Add(FSurf::Stone(C(150, 140, 120) * 0.45f, 0.5f), FVector(8.f, 14.f, 40.f), FVector(300.f, -270.f + 50.f * f, Fz + 145.f));
		return;
	}
	if (Kd == TEXT("cave") || Kd == TEXT("cave_mouth"))
	{
		// A karst cave's mouth (cook 2.6): wet limestone blocks round it, moss curtains dripping from the lintel, black
		// water lying in the entry. The cave itself is the server's AWRCave.
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector P = B.At(1700.f + 180.f * (i % 3), i < 3 ? -700.f - 100.f * i : 700.f + 100.f * (i - 3));
			B.Rock(B.Seed + i, FVector(420.f, 360.f, 340.f + 60.f * (i % 2)), P, FRotator(0.f, 40.f * i, 6.f), Lime, true, 60.f);
		}
		for (int32 m = 0; m < 12; ++m) K.Add(MossS(), FVector(6.f, 30.f, 80.f + 60.f * B.Rand(m)), FVector(2150.f, -260.f + 48.f * m, 860.f - 40.f - 40.f * B.Rand(m)), FRotator(0.f, 90.f, 0.f));
		if (!B.bServer)
		{
			if (UMaterialInterface* Lake = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("swampWorld.waterMaterial"), TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake"))))
			{
				UStaticMeshComponent* Wt = NewObject<UStaticMeshComponent>(this);
				Wt->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
				Wt->SetupAttachment(RootComponent);
				Wt->SetRelativeLocation(FVector(1800.f, 0.f, B.G(1800.f, 0.f) + WRTune(TEXT("swampWorld.caveEntryWater"), 45.f)));
				Wt->SetRelativeScale3D(FVector(9.f, 7.f, 1.f));
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Lake, this);
				M->SetVectorParameterValue(TEXT("Colour"), C(18, 20, 20));
				M->SetVectorParameterValue(TEXT("ColourDeep"), C(8, 9, 9));
				Wt->SetMaterial(0, M);
				Wt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Wt->SetCastShadow(false);
				Wt->RegisterComponent();
			}
			B.Fx(TEXT("swamp.drip"), FVector(2100.f, 0.f, 820.f), 0.6f);
		}
		return;
	}
	if (WRSwampFootSites::Build(*this, B)) return;
	UE_LOG(LogWildreach, Verbose, TEXT("SWAMP no look for site kind %s (%s)"), *Kd.ToString(), *Id.ToString());
}

// ------------------------------------------------------------------------------------------------ kit POIs

void AWRSwampScenery::BuildKit(FWRSwampBuild& B)
{
	using namespace WRSwampSceneryDetail;
	FKit& K = B.K;
	const FName Kd = Kind;
	const int32 V = Variant;
	FVector ThroatWorld = FVector::ZeroVector;
	if (const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("vharra"))) ThroatWorld = ArenaCentre(Ar);
	else if (const FWRSwampData::FSite* S = FWRSwampData::Get().FindSite(TEXT("mothers_throat"))) ThroatWorld = S->At;
	const float ToThroat = B.LocalYaw((ThroatWorld - GetActorLocation()).Rotation().Yaw);
	UMaterialInstanceDynamic* RustRock = B.bServer ? nullptr : RockMat(this, Rust * 0.9f);
	UMaterialInstanceDynamic* Lime = B.bServer ? nullptr : RockMat(this, Limestone * 0.9f);

	if (Kd == TEXT("fetish_post"))
	{
		B.FetishPost(B.At(0, 0), ToThroat, V % 3);
		B.ScatterBones(FVector::ZeroVector, 160.f, 6, V);
		return;
	}
	if (Kd == TEXT("stilt_hut"))
	{
		B.StiltHut(FVector(0, 0, 0), 0.f, V % 3, false);
		return;
	}
	if (Kd == TEXT("bloodmoss_hummock"))
	{
		// A hummock of sphagnum, garnet on top where it drinks blood-water, green-grey under (the nodes are the server's).
		B.Mound(B.At(0, 0, -20.f), 380.f + 60.f * V, 90.f, MossS(), 0);
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = i * 2.39996f;
			K.Add(GarnetMoss(0.8f + 0.4f * B.Rand(i)), FVector(90.f + 50.f * B.Rand(i + 1), 70.f, 10.f), B.At(FMath::Cos(A) * 200.f * B.Rand(i + 2), FMath::Sin(A) * 200.f * B.Rand(i + 2), 70.f * (1.f - B.Rand(i + 2) * 0.6f)), FRotator(0.f, A * 57.f, 0.f), EWRLookMesh::Cylinder);
		}
		B.Pitchers(FVector(320.f, 0.f, 0.f), 4);
		return;
	}
	if (Kd == TEXT("bog_iron_shallows"))
	{
		// Red shallows: stones crusted rust-red with bog iron in a hand's depth of water, the film on the surface.
		for (int32 i = 0; i < 9 + 3 * V; ++i)
		{
			const float A = i * 2.39996f;
			B.Rock(B.Seed + i, FVector(110.f + 50.f * (i % 3), 90.f + 40.f * (i % 2), 50.f + 20.f * (i % 3)), B.At(FMath::Cos(A) * (150.f + 60.f * i), FMath::Sin(A) * (140.f + 50.f * i)), FRotator(0.f, A * 57.f, 3.f), RustRock, false, 25.f);
		}
		FSurf Film = FSurf::Glaze(Rust * 0.35f, 0.12f);
		Film.Metal = 0.4f;
		for (int32 f = 0; f < 6; ++f) K.Add(Film, FVector(280.f + 90.f * B.Rand(f), 180.f, 1.f), B.Top(FMath::Cos(f * 1.1f) * 300.f, FMath::Sin(f * 1.1f) * 260.f, 1.f), FRotator(0.f, f * 50.f, 0.f), EWRLookMesh::Cylinder);
		return;
	}
	if (Kd == TEXT("cage_gallows"))
	{
		// A cypress (or a gallows of two posts and a beam) with one to three cages; one has fallen and burst on the mud,
		// what was in it still there (the cache).
		const int32 Cages = 1 + V % 3;
		TArray<FVector> Hooks;
		if (V == 1)
		{
			for (const float Y : { -260.f, 260.f }) K.Rod(Black(), B.At(0.f, Y, -80.f), FVector(0.f, Y, B.G(0, 0) + 650.f), 26.f);
			K.Rod(Black(), FVector(0.f, -320.f, B.G(0, 0) + 640.f), FVector(0.f, 320.f, B.G(0, 0) + 640.f), 24.f);
			B.Bar(B.At(0.f, -260.f), FVector(0.f, -260.f, B.G(0, 0) + 650.f), 14.f);
			B.Bar(B.At(0.f, 260.f), FVector(0.f, 260.f, B.G(0, 0) + 650.f), 14.f);
			for (int32 c = 0; c < Cages; ++c) Hooks.Add(FVector(0.f, -150.f + 150.f * c, B.G(0, 0) + 625.f));
		}
		else
		{
			const TArray<FVector> Tips = B.Cypress(K, B.At(0, 0, -40.f), 1600.f + 300.f * V, 4.f, 30.f * V, true, V + 70);
			for (int32 c = 0; c < Cages && c < Tips.Num(); ++c) Hooks.Add(Tips[c * 2 % Tips.Num()]);
		}
		for (int32 c = 0; c < Hooks.Num(); ++c)
		{
			FKit& Hang = B.Swing(Hooks[c]);
			FWRSwampBuild::Cage(Hang, FVector::ZeroVector, 200.f + 150.f * c, 180.f, 44.f, c == 0);
		}
		// The fallen cage, burst open on the mud: the cache lies in it.
		const FVector Fc = B.At(320.f, 160.f, 10.f);
		FWRSwampBuild::Cage(K, Fc + FVector(0, 0, 200.f), 0.f, 180.f, 44.f, false, true);
		CacheSpot = Fc + FVector(0, 0, 10.f);
		return;
	}
	if (Kd == TEXT("egg_reeds"))
	{
		// Slimeshell egg reeds: a reed stand at the water's edge with bright pink clutches stuck to the stems at the
		// waterline (#E86AA0, apple-snail pink).
		B.Reeds(FVector::ZeroVector, 260.f + 60.f * V, 30, 230.f);
		FSurf Eggs = FSurf::Glaze(Pink * 0.75f, 0.35f);
		for (int32 c = 0; c < 7 + 3 * V; ++c)
		{
			const float A = c * 2.39996f;
			const FVector P = B.Top(FMath::Cos(A) * 180.f * B.Rand(c), FMath::Sin(A) * 180.f * B.Rand(c), 18.f + 30.f * B.Rand(c + 4));
			for (int32 e = 0; e < 7; ++e) K.Add(Eggs, FVector(5.f, 5.f, 5.f), P + FVector(FMath::Cos(e * 0.9f) * 4.f, FMath::Sin(e * 0.9f) * 4.f, e * 3.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		return;
	}
	if (Kd == TEXT("kill_scene"))
	{
		// A kill: a Mudhulk drained by a leech pack, its hide sagging on its ribs (variant 0); or a carcass the Hexcrows
		// have picked to the bone, feathers on the mud (variant 1).
		const FVector C0 = B.At(0, 0, 30.f);
		for (int32 r = 0; r < 7; ++r) for (const float Sd : { -1.f, 1.f })
		{
			const FVector Sp = C0 + FVector(-150.f + 50.f * r, 0, 50.f);
			K.Rod(BoneS(0.9f), Sp, Sp + FVector(10.f, Sd * 110.f, -60.f), 7.f);
			if (V % 2 == 0 && r % 2) K.Add(LeatherS(0.6f), FVector(46.f, 8.f, 110.f), Sp + FVector(5.f, Sd * 60.f, -20.f), FRotator(0.f, 0.f, Sd * 55.f));
		}
		K.Rod(BoneS(), C0 + FVector(-190.f, 0, 50.f), C0 + FVector(190.f, 0, 55.f), 12.f);
		B.BeastSkull(C0 + FVector(250.f, 0, -20.f), 0.f, 0.7f, true);
		if (V % 2 == 0) for (int32 p = 0; p < 10; ++p) K.Add(FSurf::Stone(Oxblood * 0.3f, 0.3f), FVector(8.f, 8.f, 6.f), C0 + FVector(-140.f + 30.f * p, (p % 2 ? 60.f : -60.f), 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		else for (int32 f = 0; f < 16; ++f) K.Add(FSurf::Cloth(FLinearColor(0.012f, 0.012f, 0.015f)), FVector(30.f, 6.f, 1.f), B.At(FMath::Cos(f * 1.7f) * (60.f + 20.f * f), FMath::Sin(f * 1.7f) * (50.f + 18.f * f), 2.f), FRotator(0.f, f * 63.f, 0.f));
		return;
	}
	if (Kd == TEXT("sinkhole"))
	{
		// A karst sinkhole: a ring of slumped limestone round black water that goes down further than you can see, roots
		// hanging over its lip.
		const float R = 500.f + 200.f * V;
		for (int32 i = 0; i < 10; ++i)
		{
			const float A = 36.f * i + 10.f * B.Rand(i);
			B.Rock(B.Seed + i, FVector(200.f + 60.f * (i % 3), 160.f, 120.f), B.At(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R), FRotator(-15.f, A + 180.f, 0.f), Lime, true, 40.f);
		}
		K.Disc(FSurf::Glaze(FLinearColor(0.006f, 0.007f, 0.007f), 0.03f), FVector(0, 0, B.WaterZ - 30.f), FVector::UpVector, R * 1.7f, 4.f);
		for (int32 r = 0; r < 8; ++r)
		{
			const FVector Lip = B.At(FMath::Cos(r * 0.8f) * R * 0.95f, FMath::Sin(r * 0.8f) * R * 0.95f, 10.f);
			K.Rod(RootS(0.7f), Lip, Lip + FVector(-FMath::Cos(r * 0.8f) * 60.f, -FMath::Sin(r * 0.8f) * 60.f, -200.f - 100.f * B.Rand(r)), 5.f);
		}
		return;
	}
	if (Kd == TEXT("peat_stack"))
	{
		// A cutter's stack and lean-to: turf drying in crossed stacks, a lean-to of poles and reed thatch, a spade.
		for (int32 t = 0; t < 3 + V; ++t) B.TurfStack(B.At(-200.f + 160.f * t, -150.f, 0.f), 15.f * t, 4 + t % 2);
		for (int32 p = 0; p < 3; ++p) K.Rod(Black(), B.At(200.f, -120.f + 120.f * p, -30.f), B.At(260.f, -120.f + 120.f * p, 200.f), 8.f);
		for (int32 c = 0; c < 4; ++c) K.Add(ReedS(0.8f), FVector(260.f, 300.f, 14.f), B.At(330.f + 30.f * c, 0.f, 200.f - 40.f * c), FRotator(-50.f, 0.f, 0.f));
		K.Rod(FSurf::Wood(C(120, 96, 70) * 0.45f), B.At(150.f, 200.f, 0.f), B.At(160.f, 200.f, 140.f), 4.f);
		return;
	}
	if (Kd == TEXT("reedfowl_nest"))
	{
		// A reed island: a raft of trodden reeds, a woven nest ring on it, three or four pale speckled eggs.
		K.Add(ReedS(0.6f), FVector(500.f, 400.f, 30.f), B.Top(0, 0, 4.f), FRotator(0.f, 20.f * V, 0.f), EWRLookMesh::Cylinder);
		B.Reeds(FVector(-120.f, 80.f, 0.f), 160.f, 22, 200.f);
		const FVector N = B.Top(60.f, -40.f, 22.f);
		for (int32 i = 0; i < 12; ++i) K.Add(ReedS(0.75f), FVector(30.f, 10.f, 10.f), N + FRotator(0.f, 30.f * i, 0.f).Vector() * 36.f, FRotator(0.f, 30.f * i + 90.f, 0.f));
		for (int32 e = 0; e < 3 + V; ++e) K.Add(FSurf::Stone(C(214, 206, 180) * 0.65f, 0.4f), FVector(9.f, 7.f, 7.f), N + FVector(FMath::Cos(e * 2.f) * 12.f, FMath::Sin(e * 2.f) * 12.f, 2.f), FRotator(0.f, 40.f * e, 0.f), EWRLookMesh::Cylinder);
		return;
	}
	if (Kd == TEXT("broken_punt"))
	{
		B.Punt(B.Top(0, 0, -20.f), 0.f, V ? -6.f : 4.f, V ? 60.f : 20.f, true);
		B.Reeds(FVector(300.f, 200.f, 0.f), 200.f, 14, 200.f);
		return;
	}
	if (Kd == TEXT("toad_ring"))
	{
		// A Lanterntoad ring: cypress knees round a mossy hummock, the toads' throat sacs pulsing green-gold at night.
		K.Disc(MossS(), B.At(0, 0, 8.f), FVector::UpVector, 300.f, 24.f);
		const int32 N = 6 + 2 * V;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 360.f * i / N;
			const FVector P = B.At(FMath::Cos(FMath::DegreesToRadians(A)) * 190.f, FMath::Sin(FMath::DegreesToRadians(A)) * 190.f, 0.f);
			K.Spike(Bark(0.85f), P - FVector(0, 0, 10.f), FVector::UpVector, 30.f, 60.f + 20.f * (i % 3));
			const FVector Sac = P + FVector(0, 0, 70.f + 20.f * (i % 3));
			K.Add(FSurf::Lit(C(196, 210, 70) * 0.5f, 0.6f), FVector(8.f, 8.f, 6.f), Sac, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			if (USceneComponent* F = B.Fx(TEXT("swamp.toad"), Sac, 0.8f)) NightFx.Add(F);
			if (i % 2 == 0)
			{
				if (UPointLightComponent* L = B.Light(Sac + FVector(0, 0, 20.f), FLinearColor(0.75f, 0.85f, 0.25f), 0.f, 600.f, false)) { PulseLights.Add(L); PulseBase.Add(WRTune(TEXT("swampWorld.toadLight"), 700.f)); }
			}
		}
		return;
	}
}

// ------------------------------------------------------------------------------------------------ route edges

void AWRSwampScenery::BuildRouteEdge(FWRSwampBuild& B)
{
	// The dry routes' edges (build plan 2.4): reed bundles staked along a causeway's sides, bone paving slabs down a bone
	// causeway, arches of root over a root path. Points are world; this actor has no yaw.
	FKit& K = B.K;
	const FString RK = Kind.ToString().RightChop(6);
	TArray<FVector> L;
	for (const FVector& P : Points) L.Add(B.ToLocal(P));
	float Run = 0.f;
	const float HalfW = Width * 0.5f;
	const float Every = RK == TEXT("causeway") ? 300.f : RK == TEXT("bone") ? 110.f : 800.f;
	float Next = 0.f;
	int32 Idx = 0;
	for (int32 s = 0; s + 1 < L.Num(); ++s)
	{
		const FVector P0 = L[s], P1 = L[s + 1];
		const float SegL = FVector::Dist2D(P0, P1);
		if (SegL < 1.f) continue;
		const FVector Dir = (P1 - P0).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
		const float Yaw = Dir.Rotation().Yaw;
		while (Next <= Run + SegL)
		{
			const FVector C0 = FMath::Lerp(P0, P1, (Next - Run) / SegL);
			++Idx;
			if (RK == TEXT("causeway"))
			{
				for (const float Sd : { -1.f, 1.f })
				{
					const FVector E = C0 + Side * Sd * (HalfW + 30.f);
					const FVector G0 = B.At(E.X, E.Y, 18.f);
					K.Add(ReedS(0.8f + 0.25f * B.Rand(Idx)), FVector(290.f, 38.f, 38.f), G0, FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cube);
					for (const float Ax : { -110.f, 110.f })
					{
						const FVector St = G0 + Dir * Ax;
						K.Rod(Black(1.1f), St + Side * Sd * -28.f - FVector(0, 0, 50.f), St + Side * Sd * 28.f + FVector(0, 0, 50.f), 4.f);
						K.Rod(Black(1.1f), St + Side * Sd * 28.f - FVector(0, 0, 50.f), St + Side * Sd * -28.f + FVector(0, 0, 50.f), 4.f);
					}
					for (int32 w = 0; w < 3; ++w) K.Disc(Cord(), G0 + Dir * (-90.f + 90.f * w), Dir, 42.f, 3.f);
				}
			}
			else if (RK == TEXT("bone"))
			{
				const int32 N = FMath::Max(2, FMath::RoundToInt(Width / 70.f));
				for (int32 b = 0; b < N; ++b)
				{
					const FVector Q = C0 + Side * (-HalfW + Width * (b + 0.5f) / N) + Dir * (Idx % 2 ? 25.f : -25.f);
					K.Add(BoneS(0.78f + 0.25f * B.Rand(Idx * 5 + b)), FVector(64.f, Width / N - 6.f, 8.f), B.At(Q.X, Q.Y, 3.f), FRotator(0.f, Yaw + 4.f * (B.Rand(b + Idx) - 0.5f), 0.f));
				}
				if (Idx % 9 == 0) for (const float Sd : { -1.f, 1.f }) FWRSwampBuild::CrowSkull(K, B.At(C0.X + Side.X * Sd * (HalfW + 20.f), C0.Y + Side.Y * Sd * (HalfW + 20.f), 8.f), Yaw + 90.f * Sd, 1.2f);
			}
			else
			{
				// A root arch: two roots rising either side of the path, meeting overhead, their feet splayed in the mud.
				const FVector A0 = B.At(C0.X + Side.X * (HalfW + 60.f), C0.Y + Side.Y * (HalfW + 60.f), -30.f);
				const FVector A1 = B.At(C0.X - Side.X * (HalfW + 60.f), C0.Y - Side.Y * (HalfW + 60.f), -30.f);
				const float Hh = 260.f + 80.f * B.Rand(Idx);
				FVector Prev = A0;
				for (int32 k = 1; k <= 8; ++k)
				{
					const float T0 = k / 8.f;
					const FVector Pt = FMath::Lerp(A0, A1, T0) + FVector(0, 0, FMath::Sin(T0 * PI) * Hh);
					K.Rod(RootS(0.85f + 0.2f * B.Rand(Idx + k)), Prev, Pt, FMath::Lerp(26.f, 16.f, FMath::Sin(T0 * PI)));
					Prev = Pt;
				}
				for (const FVector& Ft : { A0, A1 }) for (int32 f = 0; f < 3; ++f) K.Rod(RootS(0.8f), Ft + FVector(0, 0, 60.f), Ft + FRotator(0.f, Yaw + 120.f * f, 0.f).Vector() * 70.f - FVector(0, 0, 20.f), 9.f);
				B.Bar(A0, A0 + FVector(0, 0, 150.f), 14.f);
				B.Bar(A1, A1 + FVector(0, 0, 150.f), 14.f);
			}
			Next += Every;
		}
		Run += SegL;
	}
}

// ================================================================================================ the Sunken Coven

namespace WRSwampCovenDetail
{
	/** A doorway left in a room's wall: wall 0 +X, 1 -X (both run along Y), 2 +Y, 3 -Y (along X); Along is the gap's
	 *  middle on the wall; Bottom / Top local heights. bPlug: a blind recess (dark stone a metre behind it). */
	struct FCvGap { int32 Wall = 0; float Along = 0.f; float Width = 360.f; float Bottom = 0.f; float Top = 320.f; bool bPlug = false; };

	FSurf CvStone(float K = 1.f) { return FSurf::Stone(C(70, 66, 58) * 0.42f * K, 0.9f); }
	FSurf CvWetStone(float K = 1.f) { return FSurf::Stone(C(64, 62, 56) * 0.36f * K, 0.45f); }
	FSurf CvBronze(float K = 1.f) { FSurf S = FSurf::Steel(C(110, 78, 40) * 0.5f * K, 0.5f); S.Metal = 0.7f; return S; }
	FSurf CvBloodFilm() { return FSurf::Glaze(Oxblood * 0.4f, 0.3f); }
	FSurf CvDried() { return FSurf::Stone(Oxblood * 0.5f, 0.75f); }

	float CvHash(int32 A, int32 Bv) { return FMath::Frac(FMath::Sin(float(A * 12.9898 + Bv * 78.233)) * 43758.5453f); }

	/** A Lanterntoad on a ledge or a post: the glowing throat sac, its light (always burning down here), the sprite. */
	void CvToad(FWRSwampBuild& B, const FVector& At, float Strength = 1.f)
	{
		B.K.Add(FSurf::Stone(C(70, 74, 40) * 0.5f, 0.4f), FVector(22.f, 18.f, 12.f), At, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(FSurf::Lit(C(196, 210, 70) * 0.6f, 0.9f), FVector(10.f, 10.f, 8.f), At + FVector(9.f, 0, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		if (USceneComponent* F = B.Fx(TEXT("swamp.toad"), At + FVector(9.f, 0, 8.f), 0.8f)) B.Act->NightFx.Add(F);
		if (UPointLightComponent* L = B.Light(At + FVector(0, 0, 40.f), FLinearColor(0.75f, 0.85f, 0.25f), 0.f, 900.f * Strength, false))
		{
			B.Act->PulseLights.Add(L);
			B.Act->PulseBase.Add(WRTune(TEXT("swampWorld.covenToadLight"), 900.f) * Strength);
		}
	}

	/** One straight run of wall from A to Bp (local), Z0..Z1: lower courses of fitted Coven stone to 1.4 m, cut peat
	 *  above with roots bursting out of it, a shoring post every 3.6 m, the wall plate; its collision. Out: outward. */
	void CvWallRun(FWRSwampBuild& B, const FVector& A, const FVector& Bp, float Z0, float Z1, const FVector& Out, bool bStoneOnly, int32 Salt)
	{
		FKit& K = B.K;
		const FVector D = Bp - A;
		const float L = D.Size2D();
		if (L < 20.f || Z1 - Z0 < 10.f) return;
		const FVector Dir = D.GetSafeNormal2D();
		const float Yaw = Dir.Rotation().Yaw;
		const float StoneTop = bStoneOnly ? Z1 : FMath::Min(Z1, FMath::Max(Z0, 140.f));
		// Courses of stone, staggered half a block each course.
		for (float Z = Z0; Z < StoneTop - 1.f; Z += 35.f)
		{
			const float Hc = FMath::Min(35.f, StoneTop - Z);
			const int32 Course = FMath::RoundToInt(Z / 35.f);
			float S = (Course % 2) ? -45.f : 0.f;
			int32 i = 0;
			while (S < L)
			{
				const float Len = 80.f + 30.f * CvHash(Salt + Course, i);
				const float S0 = FMath::Max(0.f, S), S1 = FMath::Min(L, S + Len);
				if (S1 - S0 > 4.f) K.Add(CvStone(0.75f + 0.45f * CvHash(Salt * 3 + i, Course)), FVector(S1 - S0 - 2.f, 50.f, Hc - 2.f), A + Dir * ((S0 + S1) * 0.5f) + Out * 25.f + FVector(0, 0, Z + Hc * 0.5f), FRotator(0.f, Yaw, 0.f));
				S += Len;
				++i;
			}
		}
		if (!bStoneOnly && Z1 > StoneTop)
		{
			// Cut peat in long turves, dark and wet, every face a little proud or sunk of the next.
			for (float Z = StoneTop; Z < Z1 - 1.f; Z += 60.f)
			{
				const float Hc = FMath::Min(60.f, Z1 - Z);
				int32 i = 0;
				for (float S = 0.f; S < L; S += 120.f, ++i)
				{
					const float S1 = FMath::Min(L, S + 120.f);
					const float Proud = 6.f * (CvHash(Salt + i, int32(Z)) - 0.5f);
					K.Add(PeatS(0.7f + 0.5f * CvHash(Salt * 7 + i, int32(Z) + 3)), FVector(S1 - S - 2.f, 60.f, Hc - 2.f), A + Dir * ((S + S1) * 0.5f) + Out * (30.f - Proud) + FVector(0, 0, Z + Hc * 0.5f), FRotator(0.f, Yaw, 0.f));
				}
			}
			// Roots breaking out of the peat and trailing down the face.
			const int32 NR = FMath::Max(1, int32(L / 180.f));
			for (int32 r = 0; r < NR; ++r)
			{
				const float S = L * (r + 0.5f) / NR + 40.f * (CvHash(Salt, r) - 0.5f);
				const FVector Top = A + Dir * S + Out * 2.f + FVector(0, 0, Z1 - 20.f - 60.f * CvHash(Salt + 5, r));
				const FVector Bot = Top - FVector(0, 0, 100.f + 140.f * CvHash(Salt + 9, r)) - Out * 10.f;
				K.Rod(RootS(0.7f + 0.3f * CvHash(r, Salt)), Top, Bot, 5.f + 4.f * CvHash(Salt + 2, r));
			}
		}
		// Shoring: a black cypress post against the wall every 3.6 m, the wall plate along the top.
		for (float S = 180.f; S < L - 60.f; S += 360.f)
		{
			K.Rod(Black(0.9f), A + Dir * S - Out * 12.f + FVector(0, 0, Z0), A + Dir * S - Out * 12.f + FVector(0, 0, Z1), 22.f, true);
		}
		K.Add(Black(0.8f), FVector(L, 24.f, 24.f), A + Dir * (L * 0.5f) - Out * 12.f + FVector(0, 0, Z1 - 12.f), FRotator(0.f, Yaw, 0.f));
		B.Block(A + Dir * (L * 0.5f) + Out * 30.f + FVector(0, 0, (Z0 + Z1) * 0.5f), FVector(L * 0.5f, 30.f, (Z1 - Z0) * 0.5f), FRotator(0.f, Yaw, 0.f));
	}

	/** A room's shell: the flagged (or silted) floor, four walls with their doorways, the peat-mat roof with roots hanging
	 *  through, every part with its collision. */
	void CvShell(FWRSwampBuild& B, float HX, float HY, float H, const TArray<FCvGap>& Gaps, bool bStoneOnly, bool bFlags, int32 Salt)
	{
		FKit& K = B.K;
		// The floor: worn flags in a grid, a hand's gap of silt between them.
		if (bFlags)
		{
			const int32 NX = FMath::Max(1, FMath::RoundToInt(HX * 2.f / 150.f)), NY = FMath::Max(1, FMath::RoundToInt(HY * 2.f / 150.f));
			const float SX = HX * 2.f / NX, SY = HY * 2.f / NY;
			for (int32 x = 0; x < NX; ++x) for (int32 y = 0; y < NY; ++y)
			{
				const float Tilt = 2.f * (CvHash(x + Salt, y) - 0.5f);
				K.Add(CvWetStone(0.75f + 0.4f * CvHash(x * 7 + Salt, y * 3)), FVector(SX - 6.f, SY - 6.f, 14.f), FVector(-HX + SX * (x + 0.5f), -HY + SY * (y + 0.5f), -7.f + 2.f * CvHash(x, y + Salt)), FRotator(Tilt, 0.f, -Tilt));
			}
		}
		K.Add(MudS(0.6f), FVector(HX * 2.f + 60.f, HY * 2.f + 60.f, 20.f), FVector(0, 0, -16.f));
		B.Block(FVector(0, 0, -30.f), FVector(HX + 60.f, HY + 60.f, 30.f));
		// The walls, each a run broken at its doorways.
		for (int32 w = 0; w < 4; ++w)
		{
			const bool bX = w < 2;
			const float Sgn = (w % 2 == 0) ? 1.f : -1.f;
			const float Half = bX ? HY : HX;
			const FVector Out = bX ? FVector(Sgn, 0, 0) : FVector(0, Sgn, 0);
			const auto P = [&](float Along) { return bX ? FVector(Sgn * HX, Along, 0.f) : FVector(Along, Sgn * HY, 0.f); };
			TArray<FCvGap> Mine;
			for (const FCvGap& Gp : Gaps) if (Gp.Wall == w) Mine.Add(Gp);
			Mine.Sort([](const FCvGap& A, const FCvGap& Bg) { return A.Along < Bg.Along; });
			float From = -Half - 60.f;
			for (const FCvGap& Gp : Mine)
			{
				const float G0 = FMath::Clamp(Gp.Along - Gp.Width * 0.5f, -Half, Half), G1 = FMath::Clamp(Gp.Along + Gp.Width * 0.5f, -Half, Half);
				if (G0 > From) CvWallRun(B, P(From), P(G0), 0.f, H, Out, bStoneOnly, Salt + w * 31 + int32(G0));
				// Under the doorway (a raised one: the chapel's lanes come in at its ledge), above it, its timber frame.
				if (Gp.Bottom > 5.f) CvWallRun(B, P(G0), P(G1), 0.f, Gp.Bottom, Out, true, Salt + 7);
				if (Gp.Top < H - 5.f) CvWallRun(B, P(G0), P(G1), Gp.Top, H, Out, bStoneOnly, Salt + 11);
				for (const float E : { G0, G1 }) K.Rod(Black(1.1f), P(E) + Out * 10.f + FVector(0, 0, Gp.Bottom - 20.f), P(E) + Out * 10.f + FVector(0, 0, Gp.Top + 20.f), 26.f, true);
				K.Rod(Black(1.05f), P(G0 - 30.f) + Out * 10.f + FVector(0, 0, Gp.Top + 14.f), P(G1 + 30.f) + Out * 10.f + FVector(0, 0, Gp.Top + 14.f), 28.f, true);
				if (Gp.bPlug)
				{
					// A blind doorway: dark stone a metre back, its reveals stone-lined.
					const FVector Mid = (P(G0) + P(G1)) * 0.5f + FVector(0, 0, (Gp.Bottom + Gp.Top) * 0.5f);
					K.Add(CvStone(0.45f), FVector(bX ? 30.f : G1 - G0, bX ? G1 - G0 : 30.f, Gp.Top - Gp.Bottom), Mid + Out * 120.f);
					B.Block(Mid + Out * 130.f, FVector(bX ? 20.f : (G1 - G0) * 0.5f, bX ? (G1 - G0) * 0.5f : 20.f, (Gp.Top - Gp.Bottom) * 0.5f));
					CvWallRun(B, P(G0), P(G0) + Out * 120.f, Gp.Bottom, Gp.Top, bX ? FVector(0, -1, 0) : FVector(-1, 0, 0), true, Salt + 13);
					CvWallRun(B, P(G1), P(G1) + Out * 120.f, Gp.Bottom, Gp.Top, bX ? FVector(0, 1, 0) : FVector(1, 0, 0), true, Salt + 17);
				}
				From = FMath::Max(From, G1);
			}
			if (Half + 60.f > From) CvWallRun(B, P(From), P(Half + 60.f), 0.f, H, Out, bStoneOnly, Salt + w * 31 + 999);
		}
		// The roof: the underside of the floating peat mat, slabs of matted root and peat, roots hanging through.
		const int32 RX = FMath::Max(1, FMath::RoundToInt(HX * 2.f / 240.f)), RY = FMath::Max(1, FMath::RoundToInt(HY * 2.f / 240.f));
		const float CX = HX * 2.f / RX, CY = HY * 2.f / RY;
		for (int32 x = 0; x < RX; ++x) for (int32 y = 0; y < RY; ++y)
		{
			const FVector Cn(-HX + CX * (x + 0.5f), -HY + CY * (y + 0.5f), H + 20.f - 18.f * CvHash(x + Salt, y * 5));
			K.Add(PeatS(0.55f + 0.35f * CvHash(x * 3, y + Salt)), FVector(CX + 4.f, CY + 4.f, 40.f), Cn);
			for (int32 r = 0; r < 2; ++r)
			{
				const FVector Top = Cn + FVector(CX * (CvHash(x + r, y + Salt + 1) - 0.5f) * 0.8f, CY * (CvHash(x, y + r + Salt) - 0.5f) * 0.8f, -20.f);
				const float Len = 40.f + 200.f * CvHash(x * 11 + r, y * 13 + Salt);
				K.Rod(RootS(0.6f + 0.4f * CvHash(r + x, Salt)), Top, Top - FVector(6.f * (r ? 1.f : -1.f), 4.f, Len), 3.f + 4.f * CvHash(y, r + x));
			}
		}
		B.Block(FVector(0, 0, H + 30.f), FVector(HX + 60.f, HY + 60.f, 30.f));
	}

	/** A Coven house of the drowned street: four stilts, a plank floor a metre up, plank walls, the door on the street
	 *  (+X of Yaw), its steep roof crushed against the peat mat; its collision. */
	void CvHouse(FWRSwampBuild& B, const FVector& Base, float Yaw, float Roof, int32 Var)
	{
		FKit& K = B.K;
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const float W = 300.f, D = 260.f, Floor = 100.f, Wall = 210.f;
		for (const float X : { -D * 0.5f, D * 0.5f }) for (const float Y : { -W * 0.5f, W * 0.5f })
		{
			K.Rod(Black(), P(X, Y, -30.f), P(X, Y, Floor + Wall), 20.f);
			B.Bar(P(X, Y, 0.f), P(X, Y, Floor + Wall), 11.f);
		}
		for (int32 b = 0; b < 12; ++b) K.Add(Planks(0.6f + 0.3f * B.Rand(b + Var * 20)), FVector(D + 10.f, W / 12.f - 2.f, 5.f), P(0.f, -W * 0.5f + W * (b + 0.5f) / 12.f, Floor), R);
		B.Block(P(0, 0, Floor - 3.f), FVector(D * 0.5f + 5.f, W * 0.5f, 6.f), R);
		const FSurf Wd = Planks(0.5f);
		for (int32 s = 0; s < 4; ++s)
		{
			const bool bFront = s == 0;
			const bool bAlongY = s < 2;
			const float X0 = bAlongY ? (s == 0 ? D * 0.5f : -D * 0.5f) : 0.f;
			const float Y0 = !bAlongY ? (s == 2 ? W * 0.5f : -W * 0.5f) : 0.f;
			const int32 N = 10;
			for (int32 i = 0; i < N; ++i)
			{
				if (bFront && (i == 4 || i == 5)) continue;   // the doorway onto the street
				if (Var % 3 == 1 && s == 2 && (i == 2 || i == 3 || i == 7)) continue;   // boards fallen out
				const float T = (i + 0.5f) / N - 0.5f;
				const FVector C0 = bAlongY ? P(X0, T * W, Floor + Wall * 0.5f) : P(T * D, Y0, Floor + Wall * 0.5f);
				K.Add(Wd.Dim(0.8f + 0.4f * B.Rand(i + s * 10 + Var)), FVector(4.f, (bAlongY ? W : D) / N - 1.f, Wall), C0, FRotator(0.f, Yaw + (bAlongY ? 0.f : 90.f), 0.f));
			}
			if (!bFront) B.Block(bAlongY ? P(X0, 0, Floor + Wall * 0.5f) : P(0, Y0, Floor + Wall * 0.5f), FVector(5.f, (bAlongY ? W : D) * 0.5f, Wall * 0.5f), FRotator(0.f, Yaw + (bAlongY ? 0.f : 90.f), 0.f));
		}
		// The roof, ridge along the house, its slopes broken against the mat above.
		const float Ridge = FMath::Min(Roof - 10.f, Floor + Wall + 120.f);
		for (int32 c = 0; c < 5; ++c) for (const float Sd : { -1.f, 1.f })
		{
			const float T0 = c / 5.f;
			K.Add(ReedS(0.35f + 0.08f * c), FVector(D + 60.f, 50.f, 14.f), P(0, Sd * (W * 0.5f + 30.f) * (1.f - T0), Floor + Wall + (Ridge - Floor - Wall) * T0), FRotator(0.f, Yaw, Sd * (-50.f + 15.f * (Var % 2))));
		}
		K.Rod(Black(0.8f), P(-D * 0.5f - 30.f, 0, Ridge), P(D * 0.5f + 30.f, 0, Ridge - 20.f * (Var % 2)), 12.f);
		// A hex mark on the doorpost and a bottle charm by the door.
		K.Add(CvDried(), FVector(2.f, 18.f, 18.f), P(D * 0.5f + 3.f, 70.f, Floor + 150.f), R, EWRLookMesh::Cylinder);
		FKit& Hang = B.Swing(P(D * 0.5f + 20.f, -90.f, Floor + Wall - 10.f));
		FWRSwampBuild::Bottle(Hang, FVector::ZeroVector, 40.f, Var);
	}

	/** A bog body seated upright in a niche: legs out, hands in the lap round a little bronze bowl, the braided cord at
	 *  the neck, head a little bowed, the face calm. Facing +X of Yaw. */
	void CvSeated(FKit& K, const FVector& Seat, float Yaw, int32 Var)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Seat + R.RotateVector(FVector(X, Y, Z)); };
		const FSurf Skin = LeatherS(0.75f + 0.12f * (Var % 4));
		K.Add(Skin, FVector(30.f, 34.f, 14.f), P(0, 0, 7.f), R);                                         // hips on the seat
		K.Add(Skin, FVector(24.f, 34.f, 48.f), P(-6.f, 0, 40.f), FRotator(-8.f, Yaw, 0.f));               // chest, leaning back
		K.Add(Skin.Dim(0.95f), FVector(18.f, 16.f, 20.f), P(2.f, 0, 76.f), FRotator(12.f + 4.f * (Var % 3), Yaw, 0.f), EWRLookMesh::Cylinder);   // head, bowed
		for (const float Sd : { -1.f, 1.f }) K.Add(Dark(), FVector(1.f, 4.f, 0.8f), P(10.f, Sd * 4.f, 78.f), R);   // closed eyes
		K.Add(Dark(), FVector(1.f, 5.f, 0.6f), P(11.f, 0, 70.f), R);                                                // the still mouth
		if (Var % 2) K.Add(LeatherS(0.55f), FVector(20.f, 18.f, 6.f), P(-2.f, 0, 86.f), R);                         // a skin cap
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector Hip = P(4.f, Sd * 9.f, 6.f), Knee = P(46.f, Sd * 11.f, 12.f), Foot = P(56.f, Sd * 12.f, -38.f);
			K.Rod(Skin, Hip, Knee, 10.f);
			K.Rod(Skin, Knee, Foot, 8.f);
			const FVector Sh = P(-6.f, Sd * 18.f, 60.f), El = P(10.f, Sd * 20.f, 34.f), Hand = P(28.f, Sd * 7.f, 22.f);
			K.Rod(Skin, Sh, El, 7.f);
			K.Rod(Skin, El, Hand, 6.f);
		}
		K.Add(CvBronze(0.9f + 0.1f * (Var % 3)), FVector(20.f, 20.f, 7.f), P(30.f, 0, 22.f), R, EWRLookMesh::Cylinder);
		K.Add(Dark(), FVector(16.f, 16.f, 1.f), P(30.f, 0, 26.f), R, EWRLookMesh::Cylinder);
		for (int32 s = 0; s < 3; ++s)
		{
			for (int32 i = 0; i < 10; ++i)
			{
				const float A0 = FMath::DegreesToRadians(i * 36.f + s * 120.f), A1 = FMath::DegreesToRadians((i + 1) * 36.f + s * 120.f);
				K.Rod(Cord(), P(FMath::Cos(A0) * 8.f, FMath::Sin(A0) * 8.f, 66.f + s * 1.2f), P(FMath::Cos(A1) * 8.f, FMath::Sin(A1) * 8.f, 66.f + s * 1.2f), 1.3f);
			}
			K.Rod(Cord(), P(8.f, 2.f, 66.f), P(14.f, 3.f + s, 40.f), 1.2f);
		}
	}
}

bool AWRSwampScenery::CovenPoint(FName Name, FVector& OutFloor, FVector& OutHalf)
{
	const FWRSwampData& D = FWRSwampData::Get();
	for (const FWRSwampData::FRoom& R : D.CovenRooms)
	{
		if (R.Id != Name) continue;
		OutFloor = AWRSwampCoven::ToCoven(R.At);
		OutHalf = R.Size * 0.5f;
		return true;
	}
	FVector P = FVector::ZeroVector;
	if (Name == TEXT("chimney")) P = D.Chimney;
	else if (Name == TEXT("cellar")) P = D.Cellar;
	else if (Name == TEXT("mouth"))
	{
		const TSharedPtr<FJsonObject>* Cv = nullptr;
		if (D.Root.IsValid() && D.Root->TryGetObjectField(TEXT("coven"), Cv)) P = WRSwamp::Point(*Cv, TEXT("mouth"));
	}
	if (P.IsZero()) return false;
	OutFloor = AWRSwampCoven::ToCoven(P);
	OutFloor.Z = CovenWalkFloor(Name);
	OutHalf = FVector::ZeroVector;
	return true;
}

void AWRSwampScenery::CovenLanes(TArray<TPair<FName, FName>>& Out)
{
	Out.Reset();
	const FWRSwampData& D = FWRSwampData::Get();
	const TSharedPtr<FJsonObject>* Cv = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Lanes = nullptr;
	if (!D.Root.IsValid() || !D.Root->TryGetObjectField(TEXT("coven"), Cv) || !(*Cv)->TryGetArrayField(TEXT("lanes"), Lanes)) return;
	for (const TSharedPtr<FJsonValue>& V : *Lanes)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (V.IsValid() && V->TryGetArray(Pair) && Pair->Num() >= 2) Out.Add(TPair<FName, FName>(FName(*(*Pair)[0]->AsString()), FName(*(*Pair)[1]->AsString())));
	}
}

float AWRSwampScenery::CovenWalkFloor(FName Name)
{
	const FWRSwampData& D = FWRSwampData::Get();
	for (const FWRSwampData::FRoom& R : D.CovenRooms)
	{
		if (R.Id != Name) continue;
		const float FloorZ = AWRSwampCoven::ToCoven(R.At).Z;
		return Name == TEXT("gar_pool") ? FloorZ + WRTune(TEXT("swampHazards.coven.garDepth"), 500.f) + 40.f : FloorZ;
	}
	// An entry: its landing is level with the room its lane leads to.
	TArray<TPair<FName, FName>> Lanes;
	CovenLanes(Lanes);
	for (const TPair<FName, FName>& L : Lanes)
	{
		const FName Other = L.Key == Name ? L.Value : L.Value == Name ? L.Key : FName();
		if (Other.IsNone()) continue;
		for (const FWRSwampData::FRoom& R : D.CovenRooms) if (R.Id == Other) return CovenWalkFloor(Other);
	}
	return WRSwamp::CovenLift();
}

FVector AWRSwampScenery::CovenLaneEnd(FName From, FName To)
{
	FVector F, H, T, Th;
	if (!CovenPoint(From, F, H) || !CovenPoint(To, T, Th)) return FVector::ZeroVector;
	const float Landing = WRTune(TEXT("swampWorld.covenLanding"), 300.f);
	if (H.IsNearlyZero()) H = FVector(Landing, Landing, 200.f);
	const FVector Dir = (T - F).GetSafeNormal2D();
	const float TX = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? H.X / FMath::Abs(Dir.X) : 1.0e9f;
	const float TY = FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER ? H.Y / FMath::Abs(Dir.Y) : 1.0e9f;
	FVector Out = F + Dir * FMath::Min(TX, TY);
	Out.Z = CovenWalkFloor(From);
	return Out;
}

void AWRSwampScenery::BuildCovenRoom(FWRSwampBuild& B)
{
	// A room of the Sunken Coven (cook 2.5): the drowned village under the peat mat, in its own space (WRSwampSystems).
	// This actor stands at the room's floor centre, unturned: local X / Y are the room's w / d, Z 0 its floor. The water,
	// the chimes, the bell and the things you use are the Systems' (NOTES.md).
	using namespace WRSwampCovenDetail;
	FKit& K = B.K;
	FVector FloorAt, Half;
	if (!CovenPoint(Id, FloorAt, Half)) return;
	const bool bEntry = Half.IsNearlyZero();
	const float Landing = WRTune(TEXT("swampWorld.covenLanding"), 300.f);
	const float HX = bEntry ? Landing : Half.X, HY = bEntry ? Landing : Half.Y, H = bEntry ? 420.f : Half.Z * 2.f;
	const float LaneW = WRTune(TEXT("swampWorld.covenLaneWidth"), 360.f), LaneH = WRTune(TEXT("swampWorld.covenLaneHeight"), 320.f);
	const int32 Salt = int32(GetTypeHash(Id) % 997u);
	TArray<FCvGap> Gaps;
	const auto AddGap = [&](const FVector& Local, float GapW, float GapH, bool bPlug)
	{
		FCvGap Gp;
		const bool bXWall = FMath::Abs(Local.X) / HX >= FMath::Abs(Local.Y) / HY;
		Gp.Wall = bXWall ? (Local.X >= 0.f ? 0 : 1) : (Local.Y >= 0.f ? 2 : 3);
		Gp.Along = bXWall ? Local.Y : Local.X;
		Gp.Width = GapW;
		Gp.Bottom = FMath::Max(0.f, float(Local.Z));
		Gp.Top = FMath::Min(H - 20.f, Gp.Bottom + GapH);
		Gp.bPlug = bPlug;
		Gaps.Add(Gp);
	};
	TArray<TPair<FName, FName>> Lanes;
	CovenLanes(Lanes);
	for (const TPair<FName, FName>& L : Lanes)
	{
		if (L.Key == Id) AddGap(B.ToLocal(CovenLaneEnd(Id, L.Value)), LaneW, LaneH, false);
		else if (L.Value == Id) AddGap(B.ToLocal(CovenLaneEnd(Id, L.Key)), LaneW, LaneH, false);
	}
	const FName Room = Id;
	if (Room == TEXT("weeping_gate"))
	{
		// The Weeping Gate's way out faces the Throat: its doorway is the gate's (the leaf and its frame are the Systems'
		// thing; the blind recess behind is where it opens).
		FVector Throat = FVector::ZeroVector;
		if (const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("vharra"))) Throat = WRSwampSceneryDetail::ArenaCentre(Ar);
		const FVector Dir = (Throat - FVector(FloorAt.X, FloorAt.Y, Throat.Z)).GetSafeNormal2D(1.e-4f, FVector(-1, 0, 0));
		const float TX = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? HX / FMath::Abs(Dir.X) : 1.0e9f;
		const float TY = FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER ? HY / FMath::Abs(Dir.Y) : 1.0e9f;
		FVector Edge = Dir * FMath::Min(TX, TY);
		Edge.X = FMath::Clamp(float(Edge.X), -HX, HX);
		Edge.Y = FMath::Clamp(float(Edge.Y), -HY, HY);
		AddGap(Edge, 300.f, 360.f, true);
	}
	const bool bStone = Room == TEXT("reliquary") || Room == TEXT("gar_pool") || Room == TEXT("weeping_gate");
	const bool bFlags = !(Room == TEXT("drowned_lane") || Room == TEXT("birthing_pools") || Room == TEXT("bell_pit") || bEntry);
	CvShell(B, HX, HY, H, Gaps, bStone, bFlags, Salt);

	if (bEntry)
	{
		// A landing: where the dive comes up (mouth), the dug chimney comes down, or the cellar's shaft ends. Wet stone
		// and roots; the hole, the flue and the ladder are the Systems' things.
		for (int32 r = 0; r < 8; ++r) K.Add(CvWetStone(0.7f + 0.4f * CvHash(r, Salt)), FVector(90.f, 70.f, 30.f), FVector(FMath::Cos(r * 0.785f) * 230.f, FMath::Sin(r * 0.785f) * 230.f, 10.f), FRotator(0.f, r * 45.f + 20.f, 4.f));
		CvToad(B, FVector(HX - 60.f, HY - 60.f, 20.f));
		return;
	}
	if (Room == TEXT("drowned_lane"))
	{
		// A street of stilt houses standing knee-deep in black water under the mat (the water sheet is the Systems'),
		// roots hanging through, Lanterntoads on the posts.
		const int32 N = FMath::Max(2, int32(HX * 2.f / 900.f));
		for (int32 h = 0; h < N; ++h)
		{
			for (const float Sd : { -1.f, 1.f })
			{
				if ((h + (Sd > 0.f ? 1 : 0)) % 4 == 3) continue;   // an empty lot where one fell in
				const FVector HouseAt(-HX + HX * 2.f * (h + 0.5f) / N + 120.f * Sd, Sd * (HY - 230.f), 0.f);
				CvHouse(B, HouseAt, Sd > 0.f ? -90.f : 90.f, H, h * 2 + (Sd > 0.f ? 1 : 0));
				if (h % 2 == 0) CvToad(B, HouseAt + FVector(160.f, -Sd * 170.f, 110.f));
			}
		}
		// The fallen ones: a roof slumped in the water, posts snapped off.
		for (int32 f = 0; f < 3; ++f)
		{
			const FVector Lot(-HX * 0.6f + HX * 0.6f * f, (f % 2 ? 1.f : -1.f) * (HY - 260.f), 0.f);
			for (int32 c = 0; c < 4; ++c) K.Add(ReedS(0.3f), FVector(320.f, 60.f, 14.f), Lot + FVector(0, 0, 30.f + c * 12.f), FRotator(0.f, 20.f * f + 6.f * c, 8.f * (c - 2)));
			for (int32 p = 0; p < 3; ++p) K.Rod(Black(), Lot + FVector(-100.f + 100.f * p, 40.f, -20.f), Lot + FVector(-100.f + 100.f * p, 40.f, 60.f + 40.f * p), 18.f);
		}
		// The longest roots: drawn down through the mat to the water.
		for (int32 r = 0; r < 24; ++r)
		{
			const FVector Top(-HX + HX * 2.f * CvHash(r, Salt), (CvHash(r + 7, Salt) - 0.5f) * HY * 0.8f, H);
			K.Rod(RootS(0.7f), Top, Top - FVector(10.f, -8.f, H - 30.f), 5.f + 5.f * CvHash(r, 3));
		}
		return;
	}
	if (Room == TEXT("tithe_hall"))
	{
		// The Coven's hall (cook 2.5, 2.20): long stone basins along the walls, each with a stone channel running down to
		// the one drain in the floor that leads toward the Throat; forty bronze bowls stacked; blood stains on every basin
		// lip and dried high-water lines of blood on the walls.
		FVector Throat = FVector::ZeroVector;
		if (const TSharedPtr<FJsonObject> Ar = WRSwamp::Arena(TEXT("vharra"))) Throat = WRSwampSceneryDetail::ArenaCentre(Ar);
		const FVector Drain = (Throat - FVector(FloorAt.X, FloorAt.Y, Throat.Z)).GetSafeNormal2D() * FMath::Min(HX, HY) * 0.4f;
		for (const float Sd : { -1.f, 1.f })
		{
			const int32 N = FMath::Max(2, int32(HX * 2.f / 550.f));
			for (int32 b = 0; b < N; ++b)
			{
				const FVector Bc(-HX + HX * 2.f * (b + 0.5f) / N, Sd * (HY - 90.f), 0.f);
				// The basin: a trough cut from one stone, its lip stained, dark blood dried in the bottom.
				K.Add(CvStone(0.9f), FVector(380.f, 100.f, 16.f), Bc + FVector(0, 0, 70.f));
				for (const float E : { -1.f, 1.f }) K.Add(CvStone(0.85f), FVector(380.f, 14.f, 40.f), Bc + FVector(0, E * 43.f, 98.f));
				for (const float E : { -1.f, 1.f }) K.Add(CvStone(0.85f), FVector(14.f, 100.f, 40.f), Bc + FVector(E * 183.f, 0, 98.f));
				K.Add(CvDried(), FVector(350.f, 72.f, 2.f), Bc + FVector(0, 0, 80.f));
				K.Add(CvBloodFilm(), FVector(380.f, 16.f, 2.f), Bc + FVector(0, -Sd * 43.f, 119.f));
				for (int32 d = 0; d < 4; ++d) K.Add(CvDried(), FVector(3.f, 10.f, 18.f + 14.f * CvHash(d, b)), Bc + FVector(-150.f + 100.f * d, -Sd * 51.f, 104.f));
				for (const float E : { -150.f, 150.f }) { K.Add(CvStone(0.8f), FVector(60.f, 80.f, 62.f), Bc + FVector(E, 0, 31.f)); B.Block(Bc + FVector(E, 0, 31.f), FVector(30.f, 40.f, 31.f)); }
				B.Block(Bc + FVector(0, 0, 98.f), FVector(190.f, 50.f, 20.f));
				// Its channel: a stone gutter from the basin's spout across the floor to the drain.
				const FVector Spout = Bc + FVector(0, -Sd * 60.f, 4.f);
				const FVector To = Drain + FVector(0, 0, 4.f);
				const FVector Dd = To - Spout;
				K.Add(CvStone(0.7f), FVector(Dd.Size(), 30.f, 10.f), (Spout + To) * 0.5f, Dd.Rotation());
				K.Add(CvDried(), FVector(Dd.Size(), 14.f, 2.f), (Spout + To) * 0.5f + FVector(0, 0, 5.f), Dd.Rotation());
				K.Rod(CvStone(0.8f), Bc + FVector(0, -Sd * 52.f, 72.f), Spout + FVector(0, 0, 4.f), 12.f, true);
			}
		}
		// The drain: a round iron grate over the dark, the blood-black stain fanned round it.
		K.Disc(CvDried(), Drain + FVector(0, 0, 2.f), FVector::UpVector, 260.f, 2.f);
		K.Disc(Dark(), Drain + FVector(0, 0, 3.f), FVector::UpVector, 90.f, 2.f);
		for (int32 g = 0; g < 7; ++g) K.Add(IronS(), FVector(96.f, 6.f, 4.f), Drain + FVector(0, -36.f + 12.f * g, 6.f));
		// Forty bronze bowls, stacked eight high in five stacks on a stone shelf at the far wall.
		const FVector Shelf(-HX + 80.f, 0.f, 0.f);
		K.Add(CvStone(0.8f), FVector(70.f, 420.f, 80.f), Shelf + FVector(0, 0, 40.f));
		B.Block(Shelf + FVector(0, 0, 40.f), FVector(35.f, 210.f, 40.f));
		for (int32 s = 0; s < 5; ++s) for (int32 b = 0; b < 8; ++b) K.Add(CvBronze(0.85f + 0.2f * CvHash(s, b)), FVector(24.f, 24.f, 6.f), Shelf + FVector(0, -160.f + 80.f * s, 84.f + b * 5.f), FRotator(0.f, 13.f * b, 0.f), EWRLookMesh::Cylinder);
		// High-water lines of dried blood on the walls: the hall filled, and fell, again and again.
		for (int32 l = 0; l < 4; ++l)
		{
			const float Z = 110.f + 45.f * l;
			for (const float Sd : { -1.f, 1.f }) K.Add(CvDried(), FVector(HX * 2.f - 40.f, 2.f, 3.f + l), FVector(0, Sd * (HY - 2.f), Z));
			for (const float Sd : { -1.f, 1.f }) K.Add(CvDried(), FVector(2.f, HY * 2.f - 40.f, 3.f + l), FVector(Sd * (HX - 2.f), 0, Z));
		}
		// Four roof posts down the hall's middle.
		for (int32 p = 0; p < 4; ++p)
		{
			const FVector Pp(-HX * 0.6f + HX * 0.4f * p, 0.f, 0.f);
			K.Rod(Black(1.1f), Pp, Pp + FVector(0, 0, H), 40.f);
			B.Bar(Pp, Pp + FVector(0, 0, H), 20.f);
			if (p % 2 == 0) CvToad(B, Pp + FVector(30.f, 0, 260.f));
		}
		return;
	}
	if (Room == TEXT("library"))
	{
		// The Witch's Library (cook 2.5): a dry loft of shelves, wax-sealed jars, the hex-ink stone; bark books gone to
		// pulp. (The lectern with the one readable leaf is the Systems' thing.)
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 s = 0; s < 3; ++s)
			{
				const FVector Sh(-HX + 150.f + (HX * 2.f - 300.f) * s / 2.f, Sd * (HY - 40.f), 0.f);
				for (const float E : { -80.f, 80.f }) K.Rod(Black(), Sh + FVector(E, 0, 0), Sh + FVector(E, 0, 300.f), 10.f, true);
				for (int32 l = 0; l < 4; ++l)
				{
					const float Z = 40.f + 70.f * l;
					K.Add(Planks(0.6f), FVector(170.f, 40.f, 5.f), Sh + FVector(0, 0, Z));
					for (int32 i = 0; i < 6; ++i)
					{
						const float X = -70.f + 28.f * i;
						const float Rr = CvHash(i + l * 6, s + Salt + (Sd > 0.f ? 50 : 0));
						if (Rr < 0.45f) K.Add(FSurf::Stone(C(80, 64, 44) * 0.35f, 0.95f), FVector(22.f, 26.f, 26.f + 10.f * Rr), Sh + FVector(X, 0, Z + 16.f), FRotator(4.f * Rr, 0.f, 9.f * (Rr - 0.2f)));   // a swollen book of bark leaves
						else if (Rr < 0.8f)
						{
							// A wax-sealed jar: green-brown glass, its garnet wax cap.
							K.Add(Glass(FLinearColor(0.06f, 0.1f, 0.05f)), FVector(14.f, 14.f, 22.f), Sh + FVector(X, 0, Z + 14.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
							K.Add(GarnetMoss(0.9f), FVector(15.f, 15.f, 4.f), Sh + FVector(X, 0, Z + 26.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
						}
					}
				}
				B.Block(Sh + FVector(0, 0, 150.f), FVector(85.f, 22.f, 150.f));
			}
		}
		// The hex-ink stone on a low table: a black slab with a hollow of dried ink, a bone stylus.
		const FVector Tb(HX * 0.4f, 0.f, 0.f);
		for (const float X : { -50.f, 50.f }) for (const float Y : { -30.f, 30.f }) K.Rod(Black(), Tb + FVector(X, Y, 0), Tb + FVector(X, Y, 70.f), 7.f);
		K.Add(Planks(0.7f), FVector(130.f, 80.f, 6.f), Tb + FVector(0, 0, 73.f));
		K.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.012f), 0.3f), FVector(46.f, 34.f, 8.f), Tb + FVector(-10.f, 0, 80.f));
		K.Add(CvDried(), FVector(20.f, 14.f, 1.f), Tb + FVector(-10.f, 0, 84.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Rod(BoneS(1.1f), Tb + FVector(20.f, 10.f, 78.f), Tb + FVector(40.f, 20.f, 79.f), 1.6f);
		B.Block(Tb + FVector(0, 0, 40.f), FVector(65.f, 40.f, 40.f));
		// Pulp heaped where the books fell; a toad in the dry corner.
		for (int32 p = 0; p < 7; ++p) K.Add(FSurf::Stone(C(80, 64, 44) * 0.28f, 0.95f), FVector(50.f + 30.f * CvHash(p, 1), 40.f, 10.f), FVector(-HX * 0.3f + 60.f * p, (CvHash(p, 2) - 0.5f) * HY, 5.f), FRotator(0.f, 40.f * p, 0.f), EWRLookMesh::Cylinder);
		CvToad(B, FVector(-HX + 80.f, -HY + 80.f, 10.f), 0.8f);
		return;
	}
	if (Room == TEXT("gar_pool"))
	{
		// The Old Gar's Pool (cook 2.5): a deep flooded chapel; a fishing ledge runs round its walls at the walk floor, the
		// pool below it (its water and the ledge's mooring rings are the Systems'); pillared niches; the sunken altar.
		const float Ledge = CovenWalkFloor(Room) - FloorAt.Z;
		const float LW = WRTune(TEXT("swampWorld.covenGarLedgeDepth"), 400.f);
		for (int32 w = 0; w < 4; ++w)
		{
			const bool bX = w < 2;
			const float Sg = (w % 2 == 0) ? 1.f : -1.f;
			const FVector Cn = bX ? FVector(Sg * (HX - LW * 0.5f), 0, Ledge - 20.f) : FVector(0, Sg * (HY - LW * 0.5f), Ledge - 20.f);
			const FVector Sz = bX ? FVector(LW, HY * 2.f, 40.f) : FVector(HX * 2.f - LW * 2.f, LW, 40.f);
			K.Add(CvStone(0.9f), Sz, Cn);
			B.Block(Cn, Sz * 0.5f);
			// Its inner face down to the pool bottom, coursed stone.
			const FVector Outward = bX ? FVector(Sg, 0, 0) : FVector(0, Sg, 0);
			const FVector A = bX ? FVector(Sg * (HX - LW), -HY, 0) : FVector(-HX + LW, Sg * (HY - LW), 0);
			const FVector Bp = bX ? FVector(Sg * (HX - LW), HY, 0) : FVector(HX - LW, Sg * (HY - LW), 0);
			CvWallRun(B, A - Outward * 50.f, Bp - Outward * 50.f, 0.f, Ledge - 40.f, Outward, true, Salt + w);
			// Edge stones along the lip.
			for (int32 e = 0; e < 12; ++e)
			{
				const float T = (e + 0.5f) / 12.f - 0.5f;
				const FVector Lp = bX ? FVector(Sg * (HX - LW), T * (HY * 2.f - LW * 2.f), Ledge + 4.f) : FVector(T * (HX * 2.f - LW * 2.f), Sg * (HY - LW), Ledge + 4.f);
				K.Add(CvStone(1.1f), FVector(bX ? 30.f : (HX * 2.f - LW * 2.f) / 12.f - 4.f, bX ? (HY * 2.f - LW * 2.f) / 12.f - 4.f : 30.f, 10.f), Lp + Outward * 15.f);
			}
		}
		// Pillared niches in the walls above the ledge, a toad in every other one.
		for (int32 n = 0; n < 8; ++n)
		{
			const int32 w = n % 4;
			const bool bX = w < 2;
			const float Sg = (w % 2 == 0) ? 1.f : -1.f;
			const float Al = (n < 4 ? -0.3f : 0.3f) * (bX ? HY : HX);
			const FVector Base = bX ? FVector(Sg * (HX - 8.f), Al, Ledge) : FVector(Al, Sg * (HY - 8.f), Ledge);
			const FVector Along = bX ? FVector(0, 1, 0) : FVector(1, 0, 0);
			const FVector Inward = bX ? FVector(-Sg, 0, 0) : FVector(0, -Sg, 0);
			for (const float E : { -70.f, 70.f }) K.Add(CvStone(1.05f), FVector(24.f, 24.f, 260.f), Base + Along * E + Inward * 12.f + FVector(0, 0, 130.f));
			K.Add(CvStone(1.1f), bX ? FVector(30.f, 190.f, 34.f) : FVector(190.f, 30.f, 34.f), Base + Inward * 12.f + FVector(0, 0, 277.f));
			K.Add(Dark(), bX ? FVector(4.f, 116.f, 240.f) : FVector(116.f, 4.f, 240.f), Base + FVector(0, 0, 125.f));
			if (n % 2 == 0) CvToad(B, Base + Inward * 30.f + FVector(0, 0, 10.f));
		}
		// The sunken altar on the pool's bottom and a bone pillar fallen across it; chains hanging into the water.
		K.Add(CvStone(0.7f), FVector(300.f, 160.f, 90.f), FVector(0, 0, 45.f));
		K.Rod(BoneS(0.7f), FVector(-260.f, -80.f, 20.f), FVector(240.f, 120.f, 130.f), 46.f);
		for (int32 c = 0; c < 4; ++c) FWRSwampBuild::Chain(K, FVector(-HX * 0.5f + HX * 0.33f * c, 0.f, H - 10.f), FVector(-HX * 0.5f + HX * 0.33f * c + 30.f, 40.f, Ledge - 200.f - 120.f * CvHash(c, Salt)), 18.f);
		return;
	}
	if (Room == TEXT("bog_gallery"))
	{
		// The Bog-Body Gallery (cook 2.5, 2.20 beat 6): twenty-two bog bodies seated in niches, cords at their necks, each
		// holding a small bronze bowl; one niche empty, a cord left lying in it.
		const int32 Per = 12;
		int32 Bodies = 0;
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 n = 0; n < Per; ++n)
			{
				if (Sd > 0.f && n == Per - 1) continue;   // 23 niches: 12 one side, 11 the other
				const FVector Back(-HX + 150.f + (HX * 2.f - 300.f) * n / (Per - 1), Sd * (HY - 10.f), 0.f);
				const float Face = Sd > 0.f ? -90.f : 90.f;
				const FRotator R(0.f, Face, 0.f);
				// The niche: two stone jambs, a lintel, a stone seat; dark behind.
				for (const float E : { -70.f, 70.f }) K.Add(CvStone(1.f), FVector(26.f, 30.f, 230.f), Back + R.RotateVector(FVector(30.f, E, 115.f)), R);
				K.Add(CvStone(1.1f), FVector(34.f, 170.f, 30.f), Back + R.RotateVector(FVector(30.f, 0, 245.f)), R);
				K.Add(CvStone(0.9f), FVector(60.f, 120.f, 46.f), Back + R.RotateVector(FVector(30.f, 0, 23.f)), R);
				K.Add(Dark(), FVector(6.f, 116.f, 200.f), Back + R.RotateVector(FVector(4.f, 0, 120.f)), R);
				B.Block(Back + R.RotateVector(FVector(30.f, 0, 23.f)), FVector(30.f, 60.f, 23.f), R);
				if (Sd > 0.f && n == 5)
				{
					// The empty niche (the Hex-Warden's own): only her cord, coiled on the seat.
					for (int32 c = 0; c < 9; ++c) K.Rod(Cord(), Back + R.RotateVector(FVector(30.f + FMath::Cos(c * 0.7f) * 14.f, FMath::Sin(c * 0.7f) * 14.f, 47.f)), Back + R.RotateVector(FVector(30.f + FMath::Cos(c * 0.7f + 0.7f) * 14.f, FMath::Sin(c * 0.7f + 0.7f) * 14.f, 47.f)), 1.6f);
					K.Rod(Cord(), Back + R.RotateVector(FVector(44.f, 0, 47.f)), Back + R.RotateVector(FVector(70.f, 30.f, 48.f)), 1.5f);
					continue;
				}
				CvSeated(K, Back + R.RotateVector(FVector(30.f, 0, 47.f)), Face, n + (Sd > 0.f ? 12 : 0));
				++Bodies;
			}
		}
		CvToad(B, FVector(0.f, 0.f, 6.f), 0.9f);
		CvToad(B, FVector(-HX * 0.7f, HY * 0.3f, 6.f), 0.7f);
		UE_LOG(LogWildreach, Verbose, TEXT("SWAMP coven gallery: %d bog bodies seated"), Bodies);
		return;
	}
	if (Room == TEXT("birthing_pools"))
	{
		// The Birthing Pools (cook 2.5): three warm pools (their bleedwater surfaces and boil are the Systems'), their stone
		// rims, the walls glazed with leech slime; steam off the water.
		for (int32 p = 0; p < 3; ++p)
		{
			const FVector Pc = FRotator(0.f, 120.f * p + 30.f, 0.f).Vector() * FMath::Min(HX, HY) * 0.45f;
			const float R = FMath::Min(HX, HY) * 0.32f;
			const int32 N = 18;
			for (int32 s = 0; s < N; ++s)
			{
				const float A = 360.f * (s + 0.5f) / N;
				const FVector Rp = Pc + FRotator(0.f, A, 0.f).Vector() * R;
				K.Add(CvWetStone(0.8f + 0.3f * CvHash(s, p)), FVector(40.f, 2.f * PI * R / N + 6.f, 36.f), Rp + FVector(0, 0, 10.f), FRotator(0.f, A, 0.f));
				B.Block(Rp + FVector(0, 0, 10.f), FVector(20.f, PI * R / N + 3.f, 18.f), FRotator(0.f, A, 0.f));
			}
			K.Disc(MudS(0.4f), Pc + FVector(0, 0, -30.f), FVector::UpVector, R * 2.f, 4.f);
			B.Fx(TEXT("swamp.smoke"), Pc + FVector(0, 0, 30.f), 1.4f);
		}
		for (int32 g = 0; g < 16; ++g)
		{
			const int32 w = g % 4;
			const bool bX = w < 2;
			const float Sg = (w % 2 == 0) ? 1.f : -1.f;
			const float Al = ((g / 4) - 1.5f) * (bX ? HY : HX) * 0.45f;
			const FVector Streak = bX ? FVector(Sg * (HX - 3.f), Al, 160.f + 80.f * CvHash(g, Salt)) : FVector(Al, Sg * (HY - 3.f), 160.f + 80.f * CvHash(g, Salt));
			K.Add(FSurf::Glaze(Oxblood * 0.3f, 0.15f), FVector(bX ? 2.f : 60.f, bX ? 60.f : 2.f, 200.f + 100.f * CvHash(g, 4)), Streak);
		}
		CvToad(B, FVector(0, 0, 4.f), 0.8f);
		return;
	}
	if (Room == TEXT("ringing_room"))
	{
		// The Ringing Room (cook 2.5): the beams the bone chimes hang from (the chimes are the Systems', they swing when
		// rung); a wall collapsed in at the -X side where two iron veins show (UWRSwampWorld spawns the nodes).
		const int32 Beams = 5;
		for (int32 b = 0; b < Beams; ++b)
		{
			const float X = -HX + HX * 2.f * (b + 0.5f) / Beams;
			K.Add(Black(1.1f), FVector(30.f, HY * 2.f + 40.f, 30.f), FVector(X, 0, H - 60.f));
			for (const float Sd : { -1.f, 1.f }) K.Rod(Black(), FVector(X, Sd * (HY - 20.f), 0.f), FVector(X, Sd * (HY - 20.f), H - 60.f), 26.f, true);
		}
		const FVector Fall(-HX + 140.f, 0.f, 0.f);
		for (int32 s = 0; s < 14; ++s)
		{
			const FVector P = Fall + FVector(160.f * CvHash(s, 1), (CvHash(s, 2) - 0.5f) * HY * 1.6f, 20.f + 50.f * CvHash(s, 3));
			K.Add(s % 3 ? CvStone(0.7f + 0.4f * CvHash(s, 4)) : PeatS(0.8f), FVector(90.f + 50.f * CvHash(s, 5), 70.f, 50.f), P, FRotator(20.f * CvHash(s, 6), 60.f * s, 15.f * CvHash(s, 7)));
		}
		K.Rod(Black(0.8f), Fall + FVector(60.f, -300.f, 0.f), Fall + FVector(-40.f, -60.f, 280.f), 26.f);
		K.Rod(Black(0.8f), Fall + FVector(80.f, 260.f, 0.f), Fall + FVector(-30.f, 120.f, 230.f), 22.f);
		B.Block(Fall + FVector(-60.f, 0, 40.f), FVector(60.f, HY * 0.7f, 40.f));
		CvToad(B, FVector(HX - 100.f, HY - 100.f, 6.f), 0.8f);
		return;
	}
	if (Room == TEXT("reliquary"))
	{
		// The Sealed Reliquary (cook 2.5): stone-lined, shelves of niches with little bone boxes, an altar slab at the
		// back. (The door and its cache are the Systems'.)
		for (const float Sd : { -1.f, 1.f })
		{
			for (int32 r = 0; r < 3; ++r) for (int32 c = 0; c < 4; ++c)
			{
				const FVector N(-HX + 150.f + (HX * 2.f - 300.f) * c / 3.f, Sd * (HY - 30.f), 60.f + 80.f * r);
				K.Add(CvStone(0.8f), FVector(80.f, 40.f, 8.f), N - FVector(0, 0, 30.f));
				K.Add(BoneS(0.85f + 0.2f * CvHash(r, c)), FVector(30.f, 22.f, 18.f), N - FVector(0, Sd * 6.f, 16.f), FRotator(0.f, 10.f * (c - 1.5f), 0.f));
				K.Add(CvBronze(0.7f), FVector(31.f, 23.f, 3.f), N - FVector(0, Sd * 6.f, 6.f), FRotator(0.f, 10.f * (c - 1.5f), 0.f));
			}
		}
		K.Add(CvStone(1.1f), FVector(80.f, HY, 90.f), FVector(-HX + 60.f, 0, 45.f));
		K.Add(GarnetMoss(0.6f), FVector(60.f, HY * 0.8f, 3.f), FVector(-HX + 60.f, 0, 92.f));
		B.Block(FVector(-HX + 60.f, 0, 45.f), FVector(40.f, HY * 0.5f, 45.f));
		CvToad(B, FVector(-HX + 60.f, HY * 0.3f, 96.f), 0.7f);
		return;
	}
	if (Room == TEXT("bell_pit"))
	{
		// The Bell Pit (cook 2.5): the floor steps down to a pit of silt in the middle (the bell and its yoke, half-sunk
		// in it, are the Systems').
		const float P = FMath::Min(HX, HY) * 0.55f, Down = 120.f;
		for (int32 w = 0; w < 4; ++w)
		{
			const bool bX = w < 2;
			const float Sg = (w % 2 == 0) ? 1.f : -1.f;
			const FVector Cn = bX ? FVector(Sg * (P + (HX - P) * 0.5f), 0, -7.f) : FVector(0, Sg * (P + (HY - P) * 0.5f), -7.f);
			const FVector Sz = bX ? FVector(HX - P, HY * 2.f, 14.f) : FVector(P * 2.f, HY - P, 14.f);
			K.Add(CvWetStone(0.85f), Sz, Cn);
			// Three steps down into the pit on every side.
			for (int32 s = 1; s <= 3; ++s)
			{
				const float In = P - 40.f * s;
				const FVector Sc = bX ? FVector(Sg * (In + 20.f), 0, -Down * s / 3.f + 7.f) : FVector(0, Sg * (In + 20.f), -Down * s / 3.f + 7.f);
				K.Add(CvWetStone(0.75f + 0.08f * s), bX ? FVector(40.f, P * 2.f, 14.f) : FVector(P * 2.f, 40.f, 14.f), Sc);
				B.Block(Sc, bX ? FVector(20.f, P, 7.f) : FVector(P, 20.f, 7.f));
			}
		}
		const FSurf Silt = FSurf::Glaze(Mud * 0.35f, 0.18f);
		K.Add(Silt, FVector(P * 2.f - 200.f, P * 2.f - 200.f, 10.f), FVector(0, 0, -Down - 5.f));
		B.Block(FVector(0, 0, -Down - 20.f), FVector(P, P, 15.f));
		for (int32 b = 0; b < 8; ++b) K.Add(Silt.Dim(1.2f), FVector(30.f + 20.f * CvHash(b, 1), 30.f, 6.f), FVector((CvHash(b, 2) - 0.5f) * P, (CvHash(b, 3) - 0.5f) * P, -Down + 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		CvToad(B, FVector(HX - 80.f, 0, 6.f), 0.8f);
		CvToad(B, FVector(-HX + 80.f, HY - 100.f, 6.f), 0.6f);
		return;
	}
	if (Room == TEXT("weeping_gate"))
	{
		// The Weeping Gate's vestibule (cook 2.5): stone-lined, iron-red streaks weeping down every wall from the Throat's
		// water seeping through.
		for (int32 s = 0; s < 20; ++s)
		{
			const int32 w = s % 4;
			const bool bX = w < 2;
			const float Sg = (w % 2 == 0) ? 1.f : -1.f;
			const float Al = (CvHash(s, Salt) - 0.5f) * (bX ? HY : HX) * 1.7f;
			const float L = 120.f + 200.f * CvHash(s, 2);
			const FVector Streak = bX ? FVector(Sg * (HX - 2.f), Al, H - 40.f - L * 0.5f) : FVector(Al, Sg * (HY - 2.f), H - 40.f - L * 0.5f);
			K.Add(FSurf::Glaze(Rust * 0.35f, 0.2f), FVector(bX ? 2.f : 14.f, bX ? 14.f : 2.f, L), Streak);
		}
		CvToad(B, FVector(0, 0, 4.f), 0.6f);
		return;
	}
}

void AWRSwampScenery::BuildCovenLane(FWRSwampBuild& B)
{
	// A lane of the Sunken Coven: a timber-shored passage cut through the peat from one room's doorway to the next, its
	// floor planked over the silt, ramping where the rooms differ in height. Points: its two ends (Coven space, at the
	// rooms' walk floors). This actor stands at the first end, unturned.
	using namespace WRSwampCovenDetail;
	FKit& K = B.K;
	if (Points.Num() < 2) return;
	const FVector A = B.ToLocal(Points[0]), Bp = B.ToLocal(Points[1]);
	const FVector D = Bp - A;
	const float L = D.Size2D();
	if (L < 30.f) return;
	const FVector Dir = D.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
	const float Yaw = Dir.Rotation().Yaw;
	const float HW = Width * 0.5f, H = WRTune(TEXT("swampWorld.covenLaneHeight"), 320.f);
	const int32 Salt = int32(GetTypeHash(Id) % 997u);
	const auto Along = [&](float S) { return FMath::Lerp(A, Bp, FMath::Clamp(S / L, 0.f, 1.f)); };
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(float(D.Z), L));
	// The floor: planks across on two sleepers; the collision as one ramp.
	for (float S = 0.f; S < L; S += 26.f)
	{
		K.Add(Planks(0.55f + 0.35f * CvHash(int32(S), Salt)), FVector(24.f, Width - 20.f, 5.f), Along(S + 13.f) + FVector(0, 0, -2.5f), FRotator(Pitch, Yaw, 0.f));
	}
	for (const float Sd : { -1.f, 1.f }) K.Rod(Black(0.7f), A + Side * Sd * (HW - 60.f) - FVector(0, 0, 14.f), Bp + Side * Sd * (HW - 60.f) - FVector(0, 0, 14.f), 16.f, true);
	B.Block((A + Bp) * 0.5f - FVector(0, 0, 20.f), FVector(D.Size() * 0.5f + 20.f, HW + 40.f, 20.f), FRotator(Pitch, Yaw, 0.f));
	// The walls: peat faces with roots; a shoring frame (two posts and a cap) every 2.4 m.
	for (const float Sd : { -1.f, 1.f })
	{
		const FVector Off = Side * Sd * HW;
		for (float S = 0.f; S < L; S += 120.f)
		{
			const float S1 = FMath::Min(L, S + 120.f);
			const FVector P0 = Along(S), P1 = Along(S1);
			for (int32 c = 0; c < FMath::CeilToInt(H / 60.f); ++c)
			{
				K.Add(PeatS(0.65f + 0.5f * CvHash(int32(S) + c, Salt + (Sd > 0.f ? 1 : 0))), FVector(S1 - S + 2.f, 50.f, 62.f), (P0 + P1) * 0.5f + Off + Side * Sd * (25.f + 5.f * CvHash(c, int32(S))) + FVector(0, 0, 31.f + 60.f * c), FRotator(0.f, Yaw, 0.f));
			}
			if (CvHash(int32(S), Salt + 3) < 0.4f)
			{
				const FVector Rt = (P0 + P1) * 0.5f + Off - Side * Sd * 4.f + FVector(0, 0, H - 30.f);
				K.Rod(RootS(0.7f), Rt, Rt - FVector(0, 0, 80.f + 120.f * CvHash(int32(S), 9)), 4.f);
			}
		}
		B.Block((A + Bp) * 0.5f + Off + Side * Sd * 30.f + FVector(0, 0, H * 0.5f), FVector(D.Size() * 0.5f, 30.f, H * 0.5f + 20.f), FRotator(Pitch, Yaw, 0.f));
	}
	for (float S = 120.f; S < L - 60.f; S += 240.f)
	{
		const FVector P = Along(S);
		for (const float Sd : { -1.f, 1.f }) K.Rod(Black(0.9f), P + Side * Sd * (HW - 14.f), P + Side * Sd * (HW - 14.f) + FVector(0, 0, H), 22.f, true);
		K.Rod(Black(0.95f), P + Side * (HW - 4.f) + FVector(0, 0, H - 12.f), P - Side * (HW - 4.f) + FVector(0, 0, H - 12.f), 24.f, true);
	}
	// The roof: peat mat with roots; collision.
	for (float S = 0.f; S < L; S += 240.f)
	{
		const float S1 = FMath::Min(L, S + 240.f);
		const FVector P = Along((S + S1) * 0.5f) + FVector(0, 0, H + 20.f);
		K.Add(PeatS(0.5f + 0.3f * CvHash(int32(S), Salt + 4)), FVector(S1 - S + 4.f, Width + 40.f, 40.f), P, FRotator(Pitch, Yaw, 0.f));
		for (int32 r = 0; r < 3; ++r)
		{
			const FVector Top = P + Dir * (240.f * (CvHash(r, int32(S)) - 0.5f)) + Side * (Width * 0.8f * (CvHash(r + 3, int32(S)) - 0.5f)) - FVector(0, 0, 20.f);
			K.Rod(RootS(0.6f + 0.3f * CvHash(r, Salt)), Top, Top - FVector(0, 0, 40.f + 150.f * CvHash(r + 5, int32(S) + Salt)), 3.f + 3.f * CvHash(r, 2));
		}
	}
	B.Block((A + Bp) * 0.5f + FVector(0, 0, H + 30.f), FVector(D.Size() * 0.5f, HW + 60.f, 30.f), FRotator(Pitch, Yaw, 0.f));
	// A Lanterntoad every 12 m or so, at a frame's foot.
	int32 T = 0;
	for (float S = 600.f; S < L - 300.f; S += 1200.f, ++T) CvToad(B, Along(S) + Side * (T % 2 ? 1.f : -1.f) * (HW - 40.f) + FVector(0, 0, 4.f), 0.8f);
}

// ------------------------------------------------------------------------------------------------ the secret ways

void AWRSwampScenery::BuildCandlePath(FWRSwampBuild& B)
{
	// M-S1 (cook 2.20): the Coven's hidden boardwalk 30 cm under the Corpse-Candle Mere's duckweed, from the near shore to
	// a reed island with a Coven reliquary. Planks 1.2 m wide on posts, black and slick, the duckweed (the water mats)
	// hiding them; its collision (AWRSwampWays adds none). Points are world; this actor stands at the first, unturned.
	FKit& K = B.K;
	const FWRSwampData& D = FWRSwampData::Get();
	const TArray<FVector>& Line = Points.Num() >= 2 ? Points : D.CandleLine;
	if (Line.Num() < 2) return;
	const float HW = WRTune(TEXT("swampWorld.candleWalkWidth"), 120.f) * 0.5f;
	for (int32 i = 0; i + 1 < Line.Num(); ++i)
	{
		const FVector P0 = B.ToLocal(Line[i]), P1 = B.ToLocal(Line[i + 1]);
		const FVector Dd = P1 - P0;
		const float L = Dd.Size2D();
		if (L < 1.f) continue;
		const FVector Dir = Dd.GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
		const float Yaw = Dir.Rotation().Yaw;
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(float(Dd.Z), L));
		for (float S = 0.f; S < L; S += 22.f)
		{
			// Black slimed boards; now and then one missing.
			if (B.Rand(i * 100 + int32(S)) < 0.06f) continue;
			const FVector P = FMath::Lerp(P0, P1, FMath::Min(1.f, (S + 11.f) / L));
			K.Add(FSurf::Wood(CypressBlack * 0.45f * (0.8f + 0.4f * B.Rand(i * 37 + int32(S)))), FVector(20.f, HW * 2.f, 5.f), P - FVector(0, 0, 2.5f), FRotator(Pitch, Yaw, 0.f));
		}
		for (const float Sd : { -1.f, 1.f })
		{
			const FVector F0 = P0 + Side * Sd * (HW - 14.f), F1 = P1 + Side * Sd * (HW - 14.f);
			K.Rod(Black(0.6f), F0 - FVector(0, 0, 12.f), F1 - FVector(0, 0, 12.f), 12.f, true);
			// A post at each joint, down into the mud.
			K.Rod(Black(0.55f), FVector(F0.X, F0.Y, FMath::Min(B.G(F0.X, F0.Y), float(F0.Z) - 60.f) - 100.f), F0 - FVector(0, 0, 8.f), 14.f);
			if (B.Rand(i * 3 + (Sd > 0.f ? 1 : 0)) < 0.3f) K.Add(FSurf::Cloth(Duckweed * 0.35f), FVector(30.f, 8.f, 2.f), F0 + Side * Sd * 14.f, FRotator(0.f, Yaw + 30.f, 0.f));
		}
		B.Block((P0 + P1) * 0.5f - FVector(0, 0, 3.f), FVector(Dd.Size() * 0.5f + 6.f, HW, 3.f), FRotator(Pitch, Yaw, 0.f));
	}
	// The reed island at its end: a raft of peat and trodden reeds a hand above the water, a ring of reeds round it, a
	// bone frame where the reliquary sits (the reliquary itself is the Systems' cache).
	if (!D.CandleIsland.IsZero())
	{
		const FVector Is = B.ToLocal(D.CandleIsland);
		const float R = WRTune(TEXT("swampWorld.candleIslandRadius"), 420.f);
		const float Topz = FMath::Max(float(Is.Z), B.WaterZ + 10.f);
		for (int32 l = 0; l < 4; ++l) K.Disc(l < 2 ? PeatS(0.8f + 0.1f * l) : ReedS(0.55f + 0.1f * l), FVector(Is.X, Is.Y, Topz - 60.f + 18.f * l), FVector::UpVector, R * 2.f * (1.f - 0.12f * l), 20.f);
		B.Block(FVector(Is.X, Is.Y, Topz - 30.f), FVector(R * 0.75f, R * 0.75f, 30.f));
		B.Reeds(FVector(Is.X, Is.Y, 0.f), R * 1.1f, 40, 240.f);
		for (const float Sd : { -1.f, 1.f }) K.Rod(BoneS(0.9f), FVector(Is.X + Sd * 60.f, Is.Y, Topz), FVector(Is.X + Sd * 60.f, Is.Y, Topz + 150.f), 8.f);
		K.Rod(BoneS(0.85f), FVector(Is.X - 75.f, Is.Y, Topz + 146.f), FVector(Is.X + 75.f, Is.Y, Topz + 146.f), 7.f);
		FWRSwampBuild::CrowSkull(K, FVector(Is.X, Is.Y, Topz + 160.f), B.LocalYaw(180.f), 1.2f);
		FKit& Hang = B.Swing(FVector(Is.X + 40.f, Is.Y, Topz + 140.f));
		FWRSwampBuild::Chime(Hang, FVector::ZeroVector);
	}
}

void AWRSwampScenery::BuildFallLedge(FWRSwampBuild& B)
{
	// M-S4 (cook 2.20): cut steps behind the Weeping Wall's third fall, hugging the cliff in flights that switch back
	// across the curtain, from the plunge pool's shallows up to the Gullet's mouth (the gullet site). Each step is found
	// by tracing the cliff at its height, so it sits on the face; its collision (AWRSwampWays adds none). This actor
	// stands unturned at the gullet site.
	FKit& K = B.K;
	const FWRSwampData& D = FWRSwampData::Get();
	const FWRSwampData::FSite* Gullet = D.FindSite(TEXT("gullet"));
	if (!Gullet || D.Falls.Num() == 0) return;
	const FWRSwampData::FFall* Third = &D.Falls[0];
	for (const FWRSwampData::FFall& F : D.Falls) if (FVector::Dist2D(F.Top, Gullet->At) < FVector::Dist2D(Third->Top, Gullet->At)) Third = &F;
	const float Rise = WRTune(TEXT("swampWorld.ledgeRise"), 26.f), Run = WRTune(TEXT("swampWorld.ledgeRun"), 34.f), StepW = WRTune(TEXT("swampWorld.ledgeWidth"), 140.f);
	const float Z0 = D.SeaZ + 20.f, Z1 = Gullet->At.Z;
	const float FlightRun = FMath::Min(Third->Width * 0.8f, WRTune(TEXT("swampWorld.ledgeFlight"), 1600.f));
	const float Yc = Third->Top.Y;
	const FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampLedge), false, this);
	const auto CliffX = [&](float Y, float Z, float Fallback)
	{
		FHitResult Hit;
		const FVector From(Third->Top.X + 30000.f, Y, Z + 60.f), To(Third->Top.X - 20000.f, Y, Z + 60.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor())) return float(Hit.ImpactPoint.X);
		return Fallback;
	};
	float Z = Z0, Y = Yc + FlightRun * 0.5f, Dir = -1.f, LastX = Gullet->At.X;
	int32 Steps = 0;
	while (Z < Z1 - 1.f && Steps < 400)
	{
		const float X = CliffX(Y, Z, LastX) + StepW * 0.5f - 10.f;
		LastX = X - StepW * 0.5f + 10.f;
		const FVector P = B.ToLocal(FVector(X, Y, Z + Rise * 0.5f));
		// A cut step: the tread, a rust-red seep along its nose.
		K.Add(FSurf::Stone(Slate * 0.55f * (0.8f + 0.3f * B.Rand(Steps)), 0.55f), FVector(StepW, Run + 4.f, Rise), P);
		K.Add(FSurf::Glaze(Rust * 0.3f, 0.25f), FVector(StepW - 20.f, 4.f, 3.f), P + FVector(0, -Dir * Run * 0.5f, Rise * 0.5f));
		B.Block(P, FVector(StepW * 0.5f, Run * 0.5f + 2.f, Rise * 0.5f));
		Z += Rise;
		Y += Dir * Run;
		++Steps;
		if (FMath::Abs(Y - Yc) > FlightRun * 0.5f && Z < Z1 - 1.f)
		{
			// A landing, then the next flight back the other way.
			const float LX = CliffX(Y, Z, LastX) + StepW * 0.5f - 10.f;
			const FVector Lp = B.ToLocal(FVector(LX, Y + Dir * StepW * 0.5f, Z));
			K.Add(FSurf::Stone(Slate * 0.5f, 0.6f), FVector(StepW, StepW + 20.f, 24.f), Lp - FVector(0, 0, 12.f));
			B.Block(Lp - FVector(0, 0, 12.f), FVector(StepW * 0.5f, StepW * 0.5f + 10.f, 12.f));
			// A Coven iron ring hammered into the rock at the turn, a rope rotted on it.
			K.Disc(IronS(), Lp + FVector(-StepW * 0.5f + 6.f, 0, 90.f), FVector::ForwardVector, 16.f, 3.f);
			K.Rod(Cord(), Lp + FVector(-StepW * 0.5f + 8.f, 0, 82.f), Lp + FVector(-StepW * 0.5f + 20.f, Dir * 60.f, 10.f), 2.f);
			Dir = -Dir;
			Y += Dir * StepW;
		}
	}
	// The top landing in the Gullet's mouth.
	const FVector TopAt = B.ToLocal(Gullet->At);
	K.Add(FSurf::Stone(Slate * 0.5f, 0.6f), FVector(260.f, 260.f, 30.f), TopAt - FVector(0, 0, 15.f));
	B.Block(TopAt - FVector(0, 0, 15.f), FVector(130.f, 130.f, 15.f));
	UE_LOG(LogWildreach, Log, TEXT("SWAMP fall ledge: %d steps up to the Gullet at %s"), Steps, *Gullet->At.ToCompactString());
}

// ------------------------------------------------------------------------------------------------ the landmark-foot sites

namespace WRSwampFootSites
{
	bool Build(AWRSwampScenery& Act, FWRSwampBuild& B)
	{
		FKit& K = B.K;
		const FName Kd = Act.Kind;
		if (Kd == TEXT("cage_yard"))
		{
			// The Cage-Smith's Yard (cook 2.4 #10, near the Hanging Cypress): where the Coven forged the cypress's nine cages
			// from bog iron. A cold clay bloomery, the slag heaped rust-red, a log anvil, cage hoops half-made on a rack,
			// coils of chain, an unfinished cage on its side; a gallows frame to test them on.
			UMaterialInstanceDynamic* RustRock = B.bServer ? nullptr : RockMat(&Act, Rust * 0.9f);
			const FVector F = B.At(0, 0);
			for (int32 r = 0; r < 6; ++r) K.Add(ClayS(0.85f + 0.06f * r), FVector(150.f - 14.f * r, 150.f - 14.f * r, 30.f), F + FVector(0, 0, 15.f + 30.f * r), FRotator(0.f, 12.f * r, 0.f), EWRLookMesh::Cylinder);
			K.Add(Dark(), FVector(40.f, 40.f, 50.f), F + FVector(70.f, 0, 40.f));
			B.Block(F + FVector(0, 0, 90.f), FVector(70.f, 70.f, 90.f));
			for (int32 s = 0; s < 9; ++s) B.Rock(B.Seed + s, FVector(90.f + 30.f * (s % 3), 70.f, 40.f + 15.f * (s % 2)), B.At(-260.f + 40.f * s, 260.f + 30.f * (s % 3)), FRotator(0.f, 40.f * s, 8.f), RustRock, false, 15.f);
			const FVector Anv = B.At(260.f, -60.f);
			K.Rod(Black(1.2f), Anv - FVector(0, 0, 20.f), Anv + FVector(0, 0, 70.f), 70.f);
			K.Add(IronS(), FVector(60.f, 26.f, 22.f), Anv + FVector(0, 0, 80.f));
			K.Spike(IronS(), Anv + FVector(30.f, 0, 82.f), FVector::ForwardVector, 16.f, 30.f);
			B.Block(Anv + FVector(0, 0, 40.f), FVector(40.f, 40.f, 50.f));
			// The hoop rack: two posts and a pole, hoops of every size hung on it.
			const FVector Rk = B.At(-80.f, -320.f);
			for (const float Y : { -160.f, 160.f }) K.Rod(Black(), Rk + FVector(0, Y, -40.f), Rk + FVector(0, Y, 200.f), 12.f);
			K.Rod(Black(), Rk + FVector(0, -180.f, 196.f), Rk + FVector(0, 180.f, 196.f), 10.f);
			for (int32 h = 0; h < 6; ++h) K.Disc(RustS(), Rk + FVector(0, -120.f + 48.f * h, 196.f - 30.f - 6.f * h), FVector::ForwardVector, 60.f + 16.f * h, 3.f);
			// Coils of chain on the ground and an unfinished cage on its side.
			for (int32 c = 0; c < 2; ++c) for (int32 l = 0; l < 4; ++l) K.Disc(IronS(), B.At(120.f + 160.f * c, 280.f, 6.f + 6.f * l), FVector::UpVector, 70.f - 8.f * l, 5.f);
			const FVector Cg = B.At(380.f, 220.f, 50.f);
			for (int32 b = 0; b < 7; ++b)
			{
				const FVector O = FRotator(0.f, 0.f, 51.f * b).RotateVector(FVector(0, 44.f, 0));
				K.Rod(RustS(), Cg + FVector(-90.f, O.Y, O.Z), Cg + FVector(90.f, O.Y, O.Z), 2.6f);
			}
			for (const float X : { -90.f, 0.f, 90.f }) K.Disc(IronS(), Cg + FVector(X, 0, 0), FVector::ForwardVector, 92.f, 3.f);
			// The test gallows: two black posts, a beam, one empty cage swinging.
			for (const float Y : { -200.f, 200.f }) { K.Rod(Black(), B.At(-420.f, Y, -80.f), B.At(-420.f, Y, 560.f), 24.f); B.Bar(B.At(-420.f, Y), B.At(-420.f, Y, 560.f), 12.f); }
			K.Rod(Black(), B.At(-420.f, -240.f, 550.f), B.At(-420.f, 240.f, 550.f), 22.f);
			FKit& Hang = B.Swing(B.At(-420.f, 0.f, 535.f));
			FWRSwampBuild::Cage(Hang, FVector::ZeroVector, 120.f, 180.f, 44.f, false);
			Act.CacheSpot = F + FVector(-150.f, 120.f, 10.f);
			B.ScatterBones(FVector(-300.f, 0.f, 0.f), 200.f, 6, 3);
			return true;
		}
		if (Kd == TEXT("house_yard"))
		{
			// The Drying Yard (cook 2.4 #13, below the Walking House): where the Mire-witch works when she's down: racks of
			// bloodmoss drying garnet, a cauldron on its tripod over a ring of stones, a fence of bones and stakes, a
			// grinding stone, bottles on a shelf plank, a hide stretched on a frame.
			for (int32 r = 0; r < 3; ++r)
			{
				const FVector Rc = B.At(-200.f + 200.f * r, -220.f);
				for (const float Y : { -110.f, 110.f }) K.Rod(Black(), Rc + FVector(0, Y, -40.f), Rc + FVector(0, Y, 170.f), 8.f);
				for (int32 l = 0; l < 3; ++l)
				{
					K.Rod(Cord(), Rc + FVector(0, -110.f, 90.f + 35.f * l), Rc + FVector(0, 110.f, 90.f + 35.f * l), 1.2f);
					for (int32 b = 0; b < 7; ++b) K.Add(GarnetMoss(0.7f + 0.3f * B.Rand(b + l * 7 + r * 21)), FVector(12.f, 12.f, 28.f), Rc + FVector(0, -90.f + 30.f * b, 74.f + 35.f * l), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				}
			}
			const FVector Fire = B.At(150.f, 120.f);
			for (int32 i = 0; i < 9; ++i) K.Add(FSurf::Stone(Slate * 0.6f), FVector(24.f, 18.f, 14.f), Fire + FRotator(0.f, 40.f * i, 0.f).Vector() * 55.f + FVector(0, 0, 7.f), FRotator(0.f, 40.f * i, 0.f));
			K.Add(Char(), FVector(80.f, 80.f, 4.f), Fire + FVector(0, 0, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 t = 0; t < 3; ++t) K.Rod(Black(), Fire + FRotator(0.f, 120.f * t, 0.f).Vector() * 90.f, Fire + FVector(0, 0, 170.f), 6.f);
			FWRSwampBuild::Chain(K, Fire + FVector(0, 0, 168.f), Fire + FVector(0, 0, 100.f), 10.f);
			K.Add(IronS(), FVector(64.f, 64.f, 46.f), Fire + FVector(0, 0, 74.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			K.Add(FSurf::Glaze(Oxblood * 0.4f, 0.2f), FVector(54.f, 54.f, 2.f), Fire + FVector(0, 0, 96.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Block(Fire + FVector(0, 0, 50.f), FVector(40.f, 40.f, 50.f));
			B.Light(Fire + FVector(0, 0, 40.f), FLinearColor(1.f, 0.45f, 0.2f), WRTune(TEXT("swampWorld.yardEmbers"), 900.f), 600.f, true);
			// The grinding stone and its pestle, garnet dust round it.
			const FVector Gs = B.At(320.f, -40.f);
			K.Add(FSurf::Stone(Limestone * 0.45f), FVector(90.f, 70.f, 40.f), Gs + FVector(0, 0, 20.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
			K.Add(GarnetMoss(1.1f), FVector(70.f, 50.f, 2.f), Gs + FVector(0, 0, 41.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
			K.Rod(FSurf::Stone(Limestone * 0.5f), Gs + FVector(10.f, 0, 42.f), Gs + FVector(30.f, 10.f, 70.f), 9.f);
			// A shelf plank on two stakes with bottles; a hide stretched on a frame.
			const FVector Sh = B.At(-60.f, 260.f);
			for (const float X : { -90.f, 90.f }) K.Rod(Black(), Sh + FVector(X, 0, -30.f), Sh + FVector(X, 0, 110.f), 7.f);
			K.Add(Planks(0.8f), FVector(200.f, 34.f, 5.f), Sh + FVector(0, 0, 100.f));
			for (int32 b = 0; b < 6; ++b) K.Add(Glass(b % 2 ? FLinearColor(0.06f, 0.1f, 0.05f) : FLinearColor(0.14f, 0.07f, 0.03f)), FVector(10.f, 10.f, 18.f + 6.f * (b % 3)), Sh + FVector(-75.f + 30.f * b, 0, 112.f + 3.f * (b % 3)), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			const FVector Hf = B.At(-320.f, 180.f);
			for (const float Y : { -90.f, 90.f }) K.Rod(Black(), Hf + FVector(0, Y, 0), Hf + FVector(0, Y, 210.f), 7.f);
			K.Rod(Black(), Hf + FVector(0, -100.f, 200.f), Hf + FVector(0, 100.f, 200.f), 6.f);
			K.Rod(Black(), Hf + FVector(0, -100.f, 40.f), Hf + FVector(0, 100.f, 40.f), 6.f);
			K.Add(LeatherS(0.8f), FVector(4.f, 160.f, 140.f), Hf + FVector(0, 0, 120.f));
			// The fence: stakes and long bones lashed, crow skulls looking out.
			for (int32 f = 0; f < 22; ++f)
			{
				const float A = 260.f + 10.f * f;
				const FVector P = B.At(FMath::Cos(FMath::DegreesToRadians(A)) * 560.f, FMath::Sin(FMath::DegreesToRadians(A)) * 560.f, -30.f);
				K.Rod(f % 3 ? Black(0.9f) : BoneS(0.9f), P, P + FVector(0, 0, 150.f + 30.f * B.Rand(f)), f % 3 ? 10.f : 7.f);
				if (f % 6 == 0) FWRSwampBuild::CrowSkull(K, P + FVector(0, 0, 186.f), A, 1.1f);
			}
			Act.CacheSpot = Sh + FVector(0, -60.f, 10.f);
			return true;
		}
		if (Kd == TEXT("mere_shore"))
		{
			// Candle Landing (cook 2.4 #18, the Mere's near shore): the Coven's boat landing: a half-built punt up on
			// trestles, eel traps stacked, fish drying on a rack gone to leather, a cold bone lantern on its post at the
			// water's edge, a slipway of logs down into the duckweed.
			const FVector Tr = B.At(0, 0);
			for (const float X : { -170.f, 170.f })
			{
				for (const float Sd : { -1.f, 1.f }) K.Rod(Black(), Tr + FVector(X, Sd * 70.f, -10.f), Tr + FVector(X + Sd * 6.f, 0, 90.f), 9.f);
				K.Rod(Black(), Tr + FVector(X, -80.f, 85.f), Tr + FVector(X, 80.f, 85.f), 10.f);
			}
			// The punt half-built: its bottom boards and one side, the ribs bare on the other.
			for (int32 b = 0; b < 6; ++b) K.Add(Planks(1.1f + 0.15f * B.Rand(b)), FVector(500.f, 20.f, 5.f), Tr + FVector(0, -50.f + b * 20.f, 98.f));
			K.Add(Planks(1.1f), FVector(500.f, 5.f, 40.f), Tr + FVector(0, -62.f, 118.f));
			for (int32 r = 0; r < 5; ++r) K.Rod(FSurf::Wood(C(150, 120, 85) * 0.45f), Tr + FVector(-200.f + 100.f * r, 62.f, 100.f), Tr + FVector(-200.f + 100.f * r, 62.f, 140.f), 6.f);
			B.Block(Tr + FVector(0, 0, 60.f), FVector(250.f, 70.f, 50.f));
			// Eel traps: long withy cones, stacked.
			for (int32 t = 0; t < 4; ++t)
			{
				const FVector Tp = B.At(-260.f + 30.f * (t % 2), 220.f, 30.f + 40.f * (t / 2));
				for (int32 h = 0; h < 4; ++h) K.Disc(ReedS(0.5f), Tp + FVector(h * 40.f, 0, 0), FVector::ForwardVector, 50.f - 9.f * h, 4.f);
				for (int32 s = 0; s < 8; ++s) K.Rod(ReedS(0.45f), Tp + FRotator(0.f, 0.f, 45.f * s).RotateVector(FVector(0, 25.f, 0)), Tp + FVector(150.f, 0, 0), 1.5f);
			}
			// The drying rack, its fish gone to leather.
			const FVector Dr = B.At(240.f, 240.f);
			for (const float Y : { -120.f, 120.f }) K.Rod(Black(), Dr + FVector(0, Y, -30.f), Dr + FVector(0, Y, 190.f), 8.f);
			K.Rod(Black(), Dr + FVector(0, -130.f, 180.f), Dr + FVector(0, 130.f, 180.f), 6.f);
			for (int32 f = 0; f < 8; ++f) K.Add(LeatherS(0.65f + 0.2f * B.Rand(f)), FVector(6.f, 14.f, 50.f), Dr + FVector(0, -100.f + 28.f * f, 150.f), FRotator(0.f, 0.f, 6.f * (f % 3 - 1)));
			// The slipway: logs laid down into the water toward the Mere.
			const FWRSwampData& D = FWRSwampData::Get();
			const FWRSwampData::FSite* Mere = D.FindSite(TEXT("corpse_mere"));
			const FVector Toward = Mere ? (B.ToLocal(Mere->At) * FVector(1, 1, 0)).GetSafeNormal(1.e-4f, FVector(-1, 0, 0)) : FVector(-1, 0, 0);
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Toward);
			for (int32 l = 0; l < 7; ++l)
			{
				const FVector Lp = Toward * (300.f + 70.f * l);
				const FVector G = B.Top(Lp.X, Lp.Y, -6.f - 6.f * l);
				K.Rod(Black(0.8f), G - Across * 110.f, G + Across * 110.f, 22.f);
			}
			// The bone lantern on its post at the water's edge, cold.
			const FVector Lp = B.Top(Toward.X * 280.f + Across.X * 160.f, Toward.Y * 280.f + Across.Y * 160.f, 0.f);
			K.Rod(BoneS(0.9f), Lp - FVector(0, 0, 80.f), Lp + FVector(0, 0, 220.f), 10.f);
			K.Rod(BoneS(0.85f), Lp + FVector(0, 0, 210.f), Lp + FVector(0, 0, 210.f) + Toward * 50.f, 5.f);
			FKit& Hang = B.Swing(Lp + FVector(0, 0, 205.f) + Toward * 46.f);
			Hang.Rod(Cord(), FVector::ZeroVector, FVector(0, 0, -14.f), 1.f);
			Hang.Add(BoneS(0.8f), FVector(22.f, 22.f, 30.f), FVector(0, 0, -30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			Hang.Add(Dark(), FVector(16.f, 16.f, 22.f), FVector(0, 0, -30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Reeds(Toward * 500.f + Across * 400.f, 300.f, 26, 220.f);
			Act.CacheSpot = Tr + FVector(-120.f, 0, 110.f);
			return true;
		}
		if (Kd == TEXT("cathedral_floor"))
		{
			// The Oyster Roots (cook 2.4 #23, the Mangrove Cathedral's channels): where the channels cross on their way to
			// the great tree: mangrove roots crusted white with oysters, crab holes in the mud bank, a Coven dugout sunk to
			// its gunwales, offering bowls set in the root crotches with old bones in them.
			for (int32 m = 0; m < 4; ++m)
			{
				const float A = m * 1.57f + 0.4f;
				B.Mangrove(FVector(FMath::Cos(A) * 700.f, FMath::Sin(A) * 700.f, FMath::Max(B.G(FMath::Cos(A) * 700.f, FMath::Sin(A) * 700.f), B.WaterZ)), 1000.f + 200.f * m, m + 5);
			}
			// Oyster crusts: clusters of grey-white shells on the roots at the waterline.
			for (int32 o = 0; o < 40; ++o)
			{
				const float A = o * 2.39996f;
				const FVector P = B.Top(FMath::Cos(A) * (300.f + 12.f * o), FMath::Sin(A) * (300.f + 12.f * o), 6.f);
				for (int32 s = 0; s < 4; ++s) K.Add(FSurf::Stone(C(200, 196, 182) * 0.55f, 0.6f), FVector(12.f, 8.f, 3.f), P + FVector(FMath::Cos(s * 1.6f) * 8.f, FMath::Sin(s * 1.6f) * 8.f, s * 3.f), FRotator(30.f * s, 70.f * s, 20.f));
			}
			// Crab holes in the bank.
			for (int32 c = 0; c < 18; ++c) K.Disc(Dark(), B.At(-200.f + 26.f * c, 380.f + 40.f * (c % 3), 1.f), FVector::UpVector, 10.f + 4.f * (c % 2), 2.f);
			// The dugout: one hollowed cypress trunk sunk to its gunwales, a paddle across it.
			const FVector Dg = B.Top(-260.f, -160.f, -30.f);
			for (const float Sd : { -1.f, 1.f }) K.Rod(Black(0.8f), Dg + FVector(-300.f, Sd * 34.f, 20.f), Dg + FVector(300.f, Sd * 34.f, 24.f), 14.f);
			K.Rod(Black(0.7f), Dg + FVector(-310.f, 0, 4.f), Dg + FVector(310.f, 0, 8.f), 60.f);
			K.Rod(FSurf::Wood(C(110, 90, 66) * 0.4f), Dg + FVector(-80.f, -60.f, 34.f), Dg + FVector(60.f, 70.f, 32.f), 5.f);
			// Offering bowls in the root crotches, bones in them.
			for (int32 b = 0; b < 5; ++b)
			{
				const FVector Bw = B.Top(FMath::Cos(b * 1.25f) * 560.f, FMath::Sin(b * 1.25f) * 560.f, 80.f + 30.f * (b % 2));
				K.Add(FSurf::Steel(C(110, 78, 40) * 0.45f, 0.5f), FVector(26.f, 26.f, 9.f), Bw, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				K.Rod(BoneS(0.85f), Bw + FVector(-8.f, 0, 6.f), Bw + FVector(8.f, 4.f, 9.f), 2.6f);
				K.Rod(RootS(0.9f), Bw - FVector(30.f, 0, 50.f), Bw + FVector(0, 0, -4.f), 14.f);
				K.Rod(RootS(0.9f), Bw - FVector(-30.f, 10.f, 50.f), Bw + FVector(0, 0, -4.f), 12.f);
			}
			// A fetish post at the crossing, facing the Throat (west).
			B.FetishPost(B.At(420.f, 0.f), B.LocalYaw(180.f), 0);
			Act.CacheSpot = Dg + FVector(120.f, 0, 40.f);
			return true;
		}
		return false;
	}
}
