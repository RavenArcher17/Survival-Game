#pragma once

// The Ancient forest's scenery kit (shared by WRForestScenery*.cpp): the forest palette and FBuild, one site's build in
// the actor's local space (day and night blocks, one code mesh, collision boxes, tree kit meshes, night lights) with the
// shared pieces (tōrō, antler, hulder post, cottage, Grovekin round house, canopy deck, walkway). Private to the
// scenery files; every free function is inline (unity builds join the files).

#include "CoreMinimal.h"
#include "WRForestWorld.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRFx.h"
#include "WRToolMesh.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/Package.h"

namespace WRForestLook
{
	using FSurf = WRLookKit::FSurf;
	using FKit = WRLookKit::FKit;

	inline FLinearColor Rgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	// Surfaces (linear colour, dimmed as the build kit's tinted blocks want it).
	inline FSurf Bark(float K = 1.f) { return FSurf::Wood(Rgb(74, 58, 42) * 0.55f * K); }
	inline FSurf BeechBark(float K = 1.f) { return FSurf::Wood(Rgb(168, 164, 154) * 0.5f * K); }
	inline FSurf Timber(float K = 1.f) { return FSurf::Wood(Rgb(90, 70, 54) * 0.45f * K); }
	inline FSurf Plank(float K = 1.f) { return FSurf::Wood(Rgb(140, 112, 80) * 0.42f * K); }
	inline FSurf Carved(float K = 1.f) { return FSurf::Wood(Rgb(176, 140, 96) * 0.5f * K); }
	inline FSurf Rope() { return FSurf::Cloth(Rgb(156, 132, 96) * 0.5f); }
	inline FSurf Straw() { return FSurf::Cloth(Rgb(201, 168, 90) * 0.5f); }
	inline FSurf Iron() { return FSurf::Steel(FLinearColor(0.035f, 0.033f, 0.032f), 0.65f); }
	inline FSurf Bronze() { return FSurf::Steel(Rgb(140, 106, 58) * 0.6f, 0.5f); }
	inline FSurf OldStone(float K = 1.f) { return FSurf::Stone(Rgb(138, 132, 120) * 0.55f * K, 0.92f); }
	inline FSurf Basalt(float K = 1.f) { return FSurf::Stone(Rgb(52, 54, 58) * 0.7f * K, 0.88f); }
	inline FSurf Moss(float K = 1.f) { return FSurf::Cloth(Rgb(94, 140, 49) * 0.42f * K); }
	inline FSurf DeepMoss(float K = 1.f) { return FSurf::Cloth(Rgb(58, 82, 34) * 0.5f * K); }
	inline FSurf Turf() { return FSurf::Cloth(Rgb(78, 104, 46) * 0.45f); }
	inline FSurf Earth(float K = 1.f) { return FSurf::Cloth(Rgb(74, 56, 40) * 0.42f * K); }
	inline FSurf Sand() { return FSurf::Stone(Rgb(196, 178, 140) * 0.6f, 0.95f); }
	inline FSurf Thatch() { return FSurf::Cloth(Rgb(176, 146, 80) * 0.42f); }
	inline FSurf Daub() { return FSurf::Stone(Rgb(214, 204, 182) * 0.62f, 0.95f); }
	inline FSurf Clay() { return FSurf::Stone(Rgb(150, 96, 62) * 0.5f, 0.9f); }
	inline FSurf BoneS() { return FSurf::Stone(Rgb(216, 210, 196) * 0.7f, 0.8f); }
	inline FSurf Black(float K = 1.f) { return FSurf::Wood(Rgb(30, 26, 22) * 0.6f * K); }
	inline FSurf Canvas(float K = 1.f) { return FSurf::Cloth(Rgb(190, 178, 150) * 0.45f * K); }
	inline FSurf Hide(float K = 1.f) { return FSurf::Cloth(Rgb(120, 86, 56) * 0.45f * K); }
	inline FSurf Blood() { return FSurf::Glaze(Rgb(70, 10, 8) * 0.5f, 0.4f); }
	inline FSurf Water() { return FSurf::Glaze(Rgb(60, 92, 104) * 0.45f, 0.05f); }
	inline FSurf Salt() { return FSurf::Stone(Rgb(232, 228, 220) * 0.8f, 0.7f); }
	inline FSurf Leaf(float K = 1.f) { return FSurf::Cloth(Rgb(78, 122, 58) * 0.45f * K); }
	inline FSurf Amber(float Glow) { return FSurf::Lit(Rgb(208, 138, 30) * 0.6f, Glow); }
	inline FSurf Foxfire(float Glow) { return FSurf::Lit(Rgb(127, 224, 122) * 0.55f, Glow); }
	inline FSurf Window(float Glow) { return FSurf::Lit(FLinearColor(1.f, 0.62f, 0.28f), Glow); }
	inline FSurf Violet(float Glow) { return FSurf::Lit(Rgb(138, 107, 176) * 0.7f, Glow); }
	inline FSurf Crystal() { FSurf S = FSurf::Glass(Rgb(150, 120, 200) * 0.5f, 0.15f); return S; }

	// Code-mesh colours (vertex colour on M_HeldTool).
	inline FLinearColor BarkC() { return Rgb(74, 58, 42) * 0.55f; }
	inline FLinearColor RootC() { return Rgb(90, 74, 58) * 0.5f; }
	inline FLinearColor BlackC() { return Rgb(30, 26, 22) * 0.6f; }
	inline FLinearColor MossC() { return Rgb(94, 140, 49) * 0.42f; }
	inline FLinearColor BoneC() { return Rgb(216, 210, 196) * 0.7f; }
	inline FLinearColor RopeC() { return Rgb(156, 132, 96) * 0.5f; }
	inline FLinearColor PaleWoodC() { return Rgb(176, 140, 96) * 0.5f; }
	inline FLinearColor BeechC() { return Rgb(168, 164, 154) * 0.5f; }

	inline UStaticMesh* RockMesh(int32 i)
	{
		static const TCHAR* Names[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
		const TCHAR* Nm = Names[((i % 6) + 6) % 6];
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), Nm, Nm));
	}

	/** A catenary-ish sag between two points (cosh is close enough to a parabola at a rope's sag). */
	inline FVector Sag(const FVector& A, const FVector& B, float T, float Depth)
	{
		return FMath::Lerp(A, B, T) - FVector(0, 0, 4.f * Depth * T * (1.f - T));
	}

	/** A parabolic arch from A to B rising Height over the chord. */
	inline FVector Arch(const FVector& A, const FVector& B, float T, float Height)
	{
		return FMath::Lerp(A, B, T) + FVector(0, 0, 4.f * Height * T * (1.f - T));
	}

	/**
	 * One site's build: day blocks (K), night-only blocks (N: they show at night only), one code mesh (M) for the
	 * organic parts, collision boxes, tree kit meshes, night lights. Everything in the actor's local space.
	 */
	struct FBuild
	{
		AWRForestScenery* Act;
		const FTransform T;
		TArray<FWRLookBlock> Day;
		TArray<FWRLookBlock> Night;
		FKit K;
		FKit N;
		FWRToolMesh M;
		bool bServer;
		int32 Seed;
		float Cull;

		explicit FBuild(AWRForestScenery* InA, float InCull)
			: Act(InA), T(InA->GetActorTransform()), K(T, Day), N(T, Night), bServer(InA->GetNetMode() == NM_DedicatedServer),
			  Seed(int32(GetTypeHash(InA->GetActorLocation().GridSnap(10.f).ToString()) % 997u)), Cull(InCull) {}

		/** The ground under a local XY, as a local Z (pieces sit on uneven ground, or on a platform or log the forest built
		 *  before them). Only the land and the forest's own scenery count (WRForestShape::GroundZ), so a server that already
		 *  has a shrine or a cave there builds the same collision as a client that hasn't. */
		float G(float X, float Y, float Fallback = 0.f) const
		{
			const FVector W = T.TransformPosition(FVector(X, Y, 0.f));
			constexpr float NoGround = -1.0e9f;
			const float Zw = WRForestShape::GroundZ(Act->GetWorld(), W, 4000.f, 8000.f, NoGround, true, Act);
			if (Zw <= NoGround * 0.5f) return Fallback;
			return float(T.InverseTransformPosition(FVector(W.X, W.Y, Zw)).Z);
		}
		FVector At(float X, float Y, float Lift = 0.f) const { return FVector(X, Y, G(X, Y) + Lift); }
		/** A world point in local space. */
		FVector Local(const FVector& World) const { return T.InverseTransformPosition(World); }
		float Rand(int32 Salt) const { return FMath::Frac(FMath::Sin(float(Seed * 131 + Salt * 977)) * 43758.5453f); }

		UBoxComponent* Box(const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator)
		{
			UBoxComponent* B = NewObject<UBoxComponent>(Act);
			B->SetupAttachment(Act->GetRootComponent());
			B->SetBoxExtent(Half.ComponentMax(FVector(2.f)));
			B->SetRelativeLocation(Centre);
			B->SetRelativeRotation(Rot);
			B->SetCollisionProfileName(TEXT("BlockAll"));
			B->SetCanEverAffectNavigation(false);
			B->RegisterComponent();
			return B;
		}
		/** A box along a segment (a beam, a deck span, a root you can walk on): Half = (half width, half height). */
		UBoxComponent* SegBox(const FVector& P0, const FVector& P1, float HalfW, float HalfH)
		{
			const FVector D = P1 - P0;
			const float L = D.Size();
			if (L < 1.f) return nullptr;
			return Box((P0 + P1) * 0.5f, FVector(L * 0.5f, HalfW, HalfH), D.Rotation());
		}

		/** A tree kit mesh grown to Height, its base at Base, turned by Rot. CollideH > 0: a trunk box that tall. */
		UStaticMeshComponent* Tree(FName Species, int32 Var, const FVector& Base, float Height, const FRotator& Rot = FRotator::ZeroRotator, float CollideH = -1.f, float ForcedR = 0.f)
		{
			const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species));
			const float Sc = Height / Ref;
			const float R = ForcedR > 0.f ? ForcedR : WRTreeKit::ReferenceRadius(Species) * Sc;
			const float H = CollideH < 0.f ? FMath::Min(Height * 0.45f, 2500.f) : CollideH;
			if (H > 0.f && R > 4.f) Box(Base + Rot.RotateVector(FVector(0, 0, H * 0.5f)), FVector(R * 0.78f, R * 0.78f, H * 0.5f), Rot);
			if (bServer) return nullptr;
			UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), Species, Var);
			if (!Mesh) return nullptr;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Act);
			C->SetStaticMesh(Mesh);
			C->SetupAttachment(Act->GetRootComponent());
			C->SetRelativeLocation(Base);
			C->SetRelativeRotation(Rot);
			C->SetRelativeScale3D(FVector(Sc));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			C->SetCullDistance(Cull);
			C->RegisterComponent();
			return C;
		}
		/** A log of the kit lying from P0 (root plate) to P1 (broken end), girth Radius. */
		UStaticMeshComponent* Log(FName Species, int32 Var, const FVector& P0, const FVector& P1, float Radius, bool bCollide = true)
		{
			const FVector D = P1 - P0;
			const float L = D.Size();
			if (bCollide) SegBox(P0, P1, Radius * 0.85f, Radius * 0.85f);
			if (bServer || L < 10.f) return nullptr;
			UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), Species, Var);
			if (!Mesh) return nullptr;
			const float Ref = FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species));
			const float RefR = FMath::Max(1.f, WRTreeKit::ReferenceRadius(Species));
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Act);
			C->SetStaticMesh(Mesh);
			C->SetupAttachment(Act->GetRootComponent());
			C->SetRelativeLocation(P0);
			C->SetRelativeRotation(D.Rotation());
			C->SetRelativeScale3D(FVector(L / Ref, Radius / RefR, Radius / RefR));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			C->SetCullDistance(Cull);
			C->RegisterComponent();
			return C;
		}
		/** A boulder of the rock set fitted to Size (cm), its bottom at Base, sunk Sink cm, tinted. */
		UStaticMeshComponent* Rock(int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, const FLinearColor& Tint, bool bCollide, float Sink = 15.f)
		{
			UStaticMesh* Mesh = RockMesh(Which + Seed);
			const FRotator R = Rot;
			if (!Mesh)
			{
				if (bCollide) Box(Base + FVector(0, 0, Size.Z * 0.5f - Sink), Size * 0.45f, R);
				return nullptr;
			}
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Act);
			C->SetStaticMesh(Mesh);
			C->SetupAttachment(Act->GetRootComponent());
			const FBox B = Mesh->GetBoundingBox();
			const FVector S = B.GetSize();
			const FVector Scale(Size.X / FMath::Max(1.f, S.X), Size.Y / FMath::Max(1.f, S.Y), Size.Z / FMath::Max(1.f, S.Z));
			C->SetRelativeScale3D(Scale);
			C->SetRelativeRotation(R);
			C->SetRelativeLocation(Base + R.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Sink)));
			if (!bServer)
			{
				if (UMaterialInterface* Base0 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock")))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base0, Act);
					MID->SetVectorParameterValue(TEXT("Tint"), Tint);
					C->SetMaterial(0, MID);
				}
			}
			C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
			C->SetCanEverAffectNavigation(false);
			C->SetCullDistance(Cull);
			C->RegisterComponent();
			return C;
		}
		/** A light that burns at night only (StepLights sets it), Power when lit. */
		void Lamp(const FVector& At, const FLinearColor& Colour, float Power, float Radius)
		{
			if (bServer) return;
			UPointLightComponent* L = NewObject<UPointLightComponent>(Act);
			L->SetupAttachment(Act->GetRootComponent());
			L->SetRelativeLocation(At);
			L->SetLightColor(Colour);
			L->SetIntensity(0.f);
			L->SetAttenuationRadius(Radius);
			L->SetCastShadows(false);
			L->RegisterComponent();
			Act->NightLights.Add(L);
			Act->NightPower.Add(Power);
		}
		/** A looping effect that runs all the time here (spray, smoke, bees). */
		void Loop(FName Effect, const FVector& At, const FRotator& Rot = FRotator::ZeroRotator, float Scale = 1.f)
		{
			if (bServer) return;
			if (USceneComponent* Fx = WRFx::Attach(Act->GetRootComponent(), Effect, NAME_None, Scale))
			{
				Fx->SetRelativeLocation(At);
				Fx->SetRelativeRotation(Rot);
				Act->Loops.Add(Fx);
			}
		}

		// ---- code-mesh parts ----

		/** A tapered limb (a root, a branch, a rib) from P0 (radius R0) to P1 (R1). */
		void Limb(const FVector& P0, const FVector& P1, float R0, float R1, const FLinearColor& C, int32 Sides = 8, float Rough = 0.88f)
		{
			const FVector D = P1 - P0;
			const float L = D.Size();
			if (L < 1.f) return;
			const FVector Dir = D / L;
			// Each piece starts a little back inside the one before, so a bent root has no crack at its knee.
			const float Back = FMath::Min(R0 * 0.6f, L * 0.3f);
			M.Lathe(P0 - Dir * Back, Dir, L + Back, { FVector(0.f, R0, R0), FVector(0.55f, FMath::Lerp(R0, R1, 0.55f) * 1.04f, FMath::Lerp(R0, R1, 0.55f)), FVector(1.f, R1, R1) }, Sides, C, Rough, 0.f, float(Seed % 50 + 1));
		}
		/** A root along points, tapering R0 -> R1. */
		void Root(const TArray<FVector>& Pts, float R0, float R1, const FLinearColor& C, int32 Sides = 8)
		{
			for (int32 i = 0; i + 1 < Pts.Num(); ++i)
			{
				const float A0 = float(i) / (Pts.Num() - 1), A1 = float(i + 1) / (Pts.Num() - 1);
				Limb(Pts[i], Pts[i + 1], FMath::Lerp(R0, R1, A0), FMath::Lerp(R0, R1, A1), C, Sides);
			}
		}
		/** A rounded lump (an apple, a skull dome, a knot): a lathe sphere. */
		void Ball(const FVector& Centre, float R, const FLinearColor& C, float Rough = 0.8f, float Squash = 1.f)
		{
			TArray<FVector> P;
			for (int32 i = 0; i <= 6; ++i)
			{
				const float A = PI * i / 6.f;
				P.Add(FVector(i / 6.f, FMath::Max(0.5f, R * FMath::Sin(A)), FMath::Max(0.5f, R * FMath::Sin(A))));
			}
			M.Lathe(Centre - FVector(0, 0, R * Squash), FVector::UpVector, 2.f * R * Squash, P, 10, C, Rough);
		}
		/** A rope or chain (as rope) between two points with a little sag. */
		void Line(const FVector& P0, const FVector& P1, float R, float SagCm, const FLinearColor& C, int32 Steps = 8)
		{
			TArray<FVector> Pts;
			for (int32 i = 0; i <= Steps; ++i) Pts.Add(Sag(P0, P1, float(i) / Steps, SagCm));
			M.Tube(Pts, R, 6, C, 0.95f);
		}

		void Finish(const FLinearColor& Cloth = FLinearColor(0.25f, 0.2f, 0.15f))
		{
			if (bServer) return;
			FWRBuildLook::Spawn(Act, Act->GetRootComponent(), Day, Cloth, false);
			if (Night.Num() > 0)
			{
				Act->NightRoot = NewObject<USceneComponent>(Act);
				Act->NightRoot->SetupAttachment(Act->GetRootComponent());
				Act->NightRoot->RegisterComponent();
				FWRBuildLook::Spawn(Act, Act->NightRoot, Night, Cloth, false);
				Act->NightRoot->SetVisibility(false, true);
			}
			if (M.Verts.Num() > 0)
			{
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Act);
				P->SetupAttachment(Act->GetRootComponent());
				P->RegisterComponent();
				M.ApplyTo(P);
				P->SetCullDistance(Cull);
			}
		}

		// ---- shared pieces ----

		/** A tōrō: a Japanese stone lantern (the Lantern Mourners' kind: world-map), base, stem, light box, roof with lifted
		 *  corners, finial. Tilt and Split for the ones the roots took. Unlit (they light only before an invasion). */
		void Toro(const FVector& Base, float Yaw, float Tilt, float Split)
		{
			const FRotator R(Tilt, Yaw, Tilt * 0.4f);
			const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
			const FSurf S = OldStone(0.85f);
			K.Hex(S, Base.X, Base.Y, Base.Z - 20.f, Base.Z + 25.f, 42.f, Yaw);
			K.Add(S, FVector(26.f, 26.f, 70.f), P(0, 0, 60.f), R, EWRLookMesh::Cylinder);
			K.Add(S, FVector(64.f, 64.f, 16.f), P(0, 0, 103.f), R);
			// The light box: four posts and a dark window each side; the halves stand apart where a root split it.
			for (int32 s = 0; s < 4; ++s)
			{
				const float Off = (s < 2 ? -1.f : 1.f) * Split * 0.5f;
				const FVector C0 = P((s % 2 ? 22.f : -22.f) + Off, s < 2 ? -22.f : 22.f, 140.f);
				K.Add(S, FVector(12.f, 12.f, 56.f), C0, R);
			}
			K.Add(OldStone(0.3f), FVector(30.f, 30.f, 40.f), P(0, 0, 140.f), R);
			// The kasa: a broad roof slab, its four corners turned up.
			K.Add(S, FVector(96.f, 96.f, 14.f), P(0, 0, 176.f), R);
			K.Add(S, FVector(70.f, 70.f, 16.f), P(0, 0, 190.f), R);
			for (int32 c = 0; c < 4; ++c) K.Add(S, FVector(18.f, 18.f, 12.f), P(c % 2 ? 46.f : -46.f, c < 2 ? -46.f : 46.f, 186.f), FRotator(R.Pitch, R.Yaw + 45.f, R.Roll));
			K.Spike(S, P(0, 0, 198.f), R.RotateVector(FVector::UpVector), 22.f, 30.f);
		}

		/** A palmate antler (elk / Elderelk): a beam from the pedicle out and up into a flat palm with tines along its rim. */
		void Antler(const FVector& Root, const FVector& Out, float Span, bool bMirror, const FLinearColor& C)
		{
			const FVector Up = FVector::UpVector;
			const FVector Side = Out.GetSafeNormal();
			const FVector Beam0 = Root, Beam1 = Root + Side * Span * 0.32f + Up * Span * 0.08f;
			Limb(Beam0, Beam1, Span * 0.035f, Span * 0.028f, C, 8);
			const FVector Brow = Root + Side * Span * 0.1f;
			Limb(Brow, Brow + (Side * 0.3f + FVector(bMirror ? -0.2f : 0.2f, 0, 0) + Up * 0.1f).GetSafeNormal() * Span * 0.14f + FVector(Span * 0.12f, 0, 0), Span * 0.018f, Span * 0.006f, C, 6);
			// The palm: a fan of overlapping flat blades.
			for (int32 i = 0; i < 5; ++i)
			{
				const float A = -30.f + 15.f * i;
				const FVector Dir = (Side * FMath::Cos(FMath::DegreesToRadians(A)) + Up * FMath::Sin(FMath::DegreesToRadians(A + 40.f))).GetSafeNormal();
				const FVector P0 = Beam1, P1 = Beam1 + Dir * Span * 0.2f;
				K.Add(FSurf::Stone(C, 0.75f), FVector((P1 - P0).Size(), Span * 0.07f, Span * 0.012f), (P0 + P1) * 0.5f, (P1 - P0).Rotation());
				Limb(P1, P1 + Dir * Span * 0.07f + Up * Span * 0.03f, Span * 0.012f, Span * 0.003f, C, 6);
			}
		}

		/** Appends a mesh built at the origin, moved by Xf (a lathe can't turn, so turned parts are built facing +X). */
		void Append(const FWRToolMesh& Src, const FTransform& Xf)
		{
			const int32 Base = M.Verts.Num();
			for (int32 i = 0; i < Src.Verts.Num(); ++i)
			{
				M.Verts.Add(Xf.TransformPosition(Src.Verts[i]));
				M.Normals.Add(Src.Normals.IsValidIndex(i) ? Xf.TransformVectorNoScale(Src.Normals[i]) : FVector::UpVector);
				M.UVs.Add(Src.UVs.IsValidIndex(i) ? Src.UVs[i] : FVector2D::ZeroVector);
				M.Colors.Add(Src.Colors.IsValidIndex(i) ? Src.Colors[i] : FColor::White);
			}
			for (const int32 Tri : Src.Tris) M.Tris.Add(Base + Tri);
		}

		/** A hulder post: a carved woman, life size and lovely, long hair down her back, a cow's tail under her hem, and
		 *  her back hollow bark: the Grovekin carved them at every gate (DC 2.20). Faces +X of Yaw. */
		void Hulder(const FVector& Base, float Yaw, float Scale = 1.f)
		{
			const FRotator R(0.f, Yaw, 0.f);
			// Built at the origin facing +X (an upright lathe's first radius runs across Y, its second along X), then moved.
			FWRToolMesh Saved = MoveTemp(M);
			M = FWRToolMesh();
			const FVector Fwd = FVector::ForwardVector, Up = FVector::UpVector, O = FVector::ZeroVector;
			const FLinearColor Wood = PaleWoodC();
			// The body turned up the post: plinth, the skirt's fall, hips, waist, bust, shoulders, neck, head. Y radius
			// across her, X radius front to back, so the figure has a woman's shape from the front and the side.
			const TArray<FVector> Body = {
				FVector(0.00f, 20.f, 18.f), FVector(0.03f, 22.f, 19.f), FVector(0.06f, 17.f, 14.f), FVector(0.30f, 15.f, 12.f),
				FVector(0.46f, 19.f, 13.f), FVector(0.52f, 20.f, 14.f), FVector(0.60f, 12.5f, 10.f), FVector(0.68f, 15.f, 14.5f),
				FVector(0.72f, 16.f, 15.f), FVector(0.77f, 19.f, 9.f), FVector(0.80f, 17.f, 8.f), FVector(0.83f, 5.f, 5.f),
				FVector(0.86f, 5.5f, 5.5f), FVector(0.91f, 8.5f, 9.f), FVector(0.96f, 8.f, 8.5f), FVector(1.00f, 2.f, 2.f) };
			M.Lathe(O, Up, 190.f, Body, 12, Wood, 0.7f, 0.f, 3.f);
			// The arms down her sides, hands resting at her hips.
			for (const float S : { -1.f, 1.f })
			{
				const FVector Side = FVector(0, S, 0);
				Limb(Up * 148.f + Side * 18.f, Up * 118.f + Side * 21.f + Fwd * 2.f, 4.5f, 3.8f, Wood, 6);
				Limb(Up * 118.f + Side * 21.f + Fwd * 2.f, Up * 96.f + Side * 17.f + Fwd * 6.f, 3.8f, 3.f, Wood, 6);
			}
			// Her hair: long locks from the crown over the shoulders and down her back to the waist.
			for (int32 i = 0; i < 7; ++i)
			{
				const FVector Dir = FRotator(0.f, 180.f - 60.f + 20.f * i, 0.f).Vector();
				Root({ Up * 182.f + Dir * 6.f, Up * 150.f + Dir * 12.f, Up * (108.f + 6.f * (i % 3)) + Dir * 11.f }, 3.2f, 1.6f, Rgb(150, 112, 70) * 0.45f, 6);
			}
			// The tail, peeping from under the hem: a cow's tail with its tuft.
			Root({ Up * 30.f - Fwd * 16.f, Up * 12.f - Fwd * 26.f, Up * 4.f - Fwd * 22.f }, 2.2f, 1.2f, Wood, 6);
			Ball(Up * 5.f - Fwd * 20.f, 4.f, Rgb(150, 112, 70) * 0.45f);
			FWRToolMesh Built = MoveTemp(M);
			M = MoveTemp(Saved);
			Append(Built, FTransform(R, Base, FVector(Scale)));
			// The back is bark, rough and hollow-looking: a shell of bark slabs over her back from shoulder to heel.
			for (int32 i = 0; i < 6; ++i)
			{
				K.Add(Bark(0.9f + 0.05f * i), FVector(6.f, 30.f * Scale, 26.f * Scale), Base + R.RotateVector(FVector(-14.f, 0, 16.f + 25.f * i) * Scale), R);
			}
			Box(Base + FVector(0, 0, 95.f * Scale), FVector(22.f, 22.f, 95.f) * Scale, R);
		}

		/** A newcomer cottage (the meadows' and the village's people): oak-framed, wattle-and-daub walls, a thatched roof, a
		 *  field-stone chimney, a door to +X, shuttered windows; lit at night. Ruined: roofless, a wall down, no light. */
		void Cottage(const FVector& Base, float Yaw, int32 Var, bool bRuined)
		{
			const FRotator R(0.f, Yaw, 0.f);
			const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
			const float L = 700.f + 100.f * (Var % 3), W = 480.f, H = 260.f;
			// Stone footing, then a box frame of oak: sill, posts, wall plate, braces; daub between.
			K.Add(OldStone(0.9f), FVector(L + 30.f, W + 30.f, 50.f), P(0, 0, -5.f), R);
			const float Xs[4] = { -L * 0.5f, -L * 0.17f, L * 0.17f, L * 0.5f };
			for (const float Y : { -W * 0.5f, W * 0.5f })
			{
				for (const float X : Xs) K.Add(Timber(), FVector(20.f, 20.f, H), P(X, Y, 20.f + H * 0.5f), R);
				K.Add(Timber(), FVector(L + 20.f, 22.f, 20.f), P(0, Y, 20.f + H), R);
				for (int32 b = 0; b < 3; ++b)
				{
					if (bRuined && Y > 0.f && b == 1) continue;
					K.Add(Daub(), FVector(L / 3.f - 22.f, 12.f, H - 20.f), P((Xs[b] + Xs[b + 1]) * 0.5f, Y, 20.f + H * 0.5f), R);
					K.Add(Timber(0.9f), FVector(14.f, 14.f, (L / 3.f) * 1.1f), P((Xs[b] + Xs[b + 1]) * 0.5f, Y + (Y > 0 ? 5.f : -5.f), 20.f + H * 0.5f), FRotator(b % 2 ? 50.f : -50.f, Yaw, 0.f), EWRLookMesh::Cube);   // the braces, in the wall's plane
				}
			}
			// Gable ends: the front with the door (+X), the back with the chimney.
			for (const float X : { -L * 0.5f, L * 0.5f })
			{
				K.Add(Timber(), FVector(20.f, W, 20.f), P(X, 0, 20.f + H), R);
				if (X > 0.f)
				{
					K.Add(Daub(), FVector(12.f, W * 0.5f - 70.f, H - 20.f), P(X, -W * 0.25f - 35.f, 20.f + H * 0.5f), R);
					K.Add(Daub(), FVector(12.f, W * 0.5f - 70.f, H - 20.f), P(X, W * 0.25f + 35.f, 20.f + H * 0.5f), R);
					K.Add(Timber(0.8f), FVector(8.f, 100.f, 200.f), P(X + 30.f, -40.f, 120.f), FRotator(0.f, Yaw + (bRuined ? 70.f : 20.f), 0.f));   // the door, ajar
					K.Add(Timber(), FVector(22.f, 140.f, 20.f), P(X, 0, 225.f), R);
				}
				else K.Add(Daub(), FVector(12.f, W - 20.f, H - 20.f), P(X, 0, 20.f + H * 0.5f), R);
				// The gable triangle in daub, stepped.
				for (int32 s = 0; s < 4; ++s) K.Add(Daub(), FVector(12.f, W * (1.f - s * 0.25f) - 20.f, 50.f), P(X, 0, 20.f + H + 25.f + 50.f * s), R);
			}
			// Windows: shutters on the long sides; lit panes at night (the village's warm windows: DC 2.1 night read).
			for (const float Y : { -W * 0.5f - 8.f, W * 0.5f + 8.f })
			{
				K.Add(Timber(0.8f), FVector(70.f, 6.f, 60.f), P(-L * 0.25f, Y, 150.f), R);
				K.Add(Timber(0.75f), FVector(36.f, 4.f, 60.f), P(-L * 0.25f - 54.f, Y, 150.f), FRotator(0.f, Yaw + (Y > 0 ? 25.f : -25.f), 0.f));
				if (!bRuined)
				{
					N.Add(Window(0.9f), FVector(56.f, 4.f, 46.f), P(-L * 0.25f, Y * 0.99f, 150.f), R);
				}
			}
			if (!bRuined) Lamp(P(-L * 0.25f, 0, 160.f), FLinearColor(1.f, 0.6f, 0.28f), 2400.f, 900.f);
			// The thatch: thick sloped slabs from the wall plates to the ridge, an overhang, the ridge capped.
			if (!bRuined || Var % 2 == 0)
			{
				const float Pitch = 45.f;
				const float Slope = (W * 0.5f + 60.f) / FMath::Cos(FMath::DegreesToRadians(Pitch));
				for (const float S : { -1.f, 1.f })
				{
					const float Len = bRuined ? L * 0.45f : L + 80.f;
					const float Cx = bRuined ? -L * 0.25f : 0.f;
					K.Add(Thatch(), FVector(Len, Slope, 30.f), P(Cx, S * (W * 0.25f + 30.f), 20.f + H + (W * 0.25f + 30.f) * FMath::Tan(FMath::DegreesToRadians(Pitch)) * 0.95f), FRotator(0.f, Yaw, S * -Pitch));
				}
				K.Add(Thatch(), FVector(bRuined ? L * 0.45f : L + 80.f, 40.f, 40.f), P(bRuined ? -L * 0.25f : 0.f, 0, 20.f + H + W * 0.5f + 40.f), R);
			}
			else
			{
				// Fallen in: rafters sticking up, the thatch rotted to a heap inside.
				for (int32 i = 0; i < 4; ++i) K.Rod(Timber(0.8f), P(-L * 0.4f + i * L * 0.25f, -W * 0.5f, 20.f + H), P(-L * 0.35f + i * L * 0.25f, 40.f, 20.f + H + 140.f - 60.f * (i % 2)), 12.f);
				K.Add(Thatch(), FVector(L * 0.6f, W * 0.6f, 50.f), P(0, 0, 40.f), FRotator(0.f, Yaw, 4.f));
			}
			// The chimney up the back gable: field stone in courses, a little smoke if lived in.
			for (int32 c = 0; c < 9; ++c) K.Add(OldStone(0.8f + 0.06f * (c % 3)), FVector(90.f, 110.f, 50.f), P(-L * 0.5f - 40.f, 0, 25.f + 50.f * c), R);
			if (!bRuined) Loop(TEXT("forest.chimney"), P(-L * 0.5f - 40.f, 0, 25.f + 450.f));
			Box(P(0, 0, 20.f + H * 0.5f), FVector(L * 0.5f, W * 0.5f, H * 0.5f + 20.f), R);
		}

		/** A Grovekin round house round a living trunk (house-trees, the Hermit's Oak): a ring floor on joists socketed into
		 *  the trunk, a woven-branch wall, a moss roof sloping up to the trunk, a door to +X. FloorZ local. */
		void GroveHouse(const FVector& Trunk, float TrunkR, float FloorZ, float Yaw, float Out, bool bLit, bool bBroken, float NotchHalf = 0.f)
		{
			const float Ro = TrunkR + Out;
			const float WallH = 230.f;
			const float A0 = NotchHalf, A1 = 360.f - NotchHalf;
			// Floor: radial boards between the trunk and the wall (the gap at the stair notch).
			const int32 Boards = 36;
			for (int32 i = 0; i < Boards; ++i)
			{
				const float A = A0 + (A1 - A0) * (i + 0.5f) / Boards;
				const FVector Dir = FRotator(0.f, Yaw + A, 0.f).Vector();
				const float Chord = 2.f * Ro * FMath::Sin(FMath::DegreesToRadians((A1 - A0) / Boards * 0.5f));
				K.Add(Plank(0.9f + 0.15f * FMath::Frac(A * 0.37f)), FVector(Out, Chord * 0.95f, 6.f), Trunk + Dir * (TrunkR + Out * 0.5f) + FVector(0, 0, FloorZ - 3.f), FRotator(0.f, Yaw + A, 0.f));
				if (i % 4 == 0)
				{
					// A joist let into the trunk, a knee brace under it back to the bark.
					K.Add(Timber(), FVector(Out + 40.f, 18.f, 22.f), Trunk + Dir * (TrunkR + Out * 0.5f - 20.f) + FVector(0, 0, FloorZ - 17.f), FRotator(0.f, Yaw + A, 0.f));
					K.Rod(Timber(0.9f), Trunk + Dir * (TrunkR - 10.f) + FVector(0, 0, FloorZ - 220.f), Trunk + Dir * (TrunkR + Out * 0.7f) + FVector(0, 0, FloorZ - 26.f), 12.f);
				}
			}
			for (int32 i = 0; i < 8; ++i)
			{
				const float A = A0 + (A1 - A0) * (i + 0.5f) / 8.f;
				const FVector Dir = FRotator(0.f, Yaw + A, 0.f).Vector();
				const float Chord = 2.f * Ro * FMath::Sin(FMath::DegreesToRadians((A1 - A0) / 8.f * 0.5f));
				Box(Trunk + Dir * (TrunkR + Out * 0.5f) + FVector(0, 0, FloorZ - 6.f), FVector(Out * 0.5f, Chord * 0.5f, 6.f), FRotator(0.f, Yaw + A, 0.f));
			}
			// The wall: stakes and woven withies (living branches bent round), a door gap at +X (or at the notch's edge).
			const float DoorA = NotchHalf > 0.f ? NotchHalf + 12.f : 0.f;
			const int32 Stakes = 28;
			for (int32 i = 0; i < Stakes; ++i)
			{
				const float A = A0 + (A1 - A0) * i / (Stakes - 1);
				if (FMath::Abs(FRotator::NormalizeAxis(A - DoorA)) < 9.f) continue;
				const FVector P0 = Trunk + FRotator(0.f, Yaw + A, 0.f).Vector() * Ro + FVector(0, 0, FloorZ);
				M.Lathe(P0, FVector::UpVector, WallH + 10.f, { FVector(0, 5.f, 5.f), FVector(1, 3.5f, 3.5f) }, 6, BarkC(), 0.9f, 0.f, float(i + 1));
			}
			for (int32 c = 0; c < 9; ++c)
			{
				TArray<FVector> Ring;
				const float Z = FloorZ + 20.f + c * 24.f;
				for (int32 s = 0; s <= 40; ++s)
				{
					const float A = A0 + (A1 - A0) * s / 40.f;
					if (FMath::Abs(FRotator::NormalizeAxis(A - DoorA)) < 9.f && c < 8) { if (Ring.Num() > 1) M.Tube(Ring, 2.6f, 5, RootC(), 0.9f); Ring.Reset(); continue; }
					Ring.Add(Trunk + FRotator(0.f, Yaw + A, 0.f).Vector() * (Ro + ((s + c) % 2 ? 4.f : -4.f)) + FVector(0, 0, Z));
				}
				if (Ring.Num() > 1) M.Tube(Ring, 2.6f, 5, RootC(), 0.9f);
			}
			for (int32 i = 0; i < 16; ++i)
			{
				const float A = A0 + (A1 - A0) * (i + 0.5f) / 16.f;
				if (FMath::Abs(FRotator::NormalizeAxis(A - DoorA)) < 12.f) continue;
				const float Chord = 2.f * Ro * FMath::Sin(FMath::DegreesToRadians((A1 - A0) / 16.f * 0.5f));
				Box(Trunk + FRotator(0.f, Yaw + A, 0.f).Vector() * Ro + FVector(0, 0, FloorZ + WallH * 0.5f), FVector(8.f, Chord * 0.5f, WallH * 0.5f), FRotator(0.f, Yaw + A, 0.f));
			}
			// The door: a frame of two hulder-carved posts and a woven door leaf, open (or torn down).
			const FVector DoorDir = FRotator(0.f, Yaw + DoorA, 0.f).Vector();
			const FVector DoorSide = FRotator(0.f, Yaw + DoorA + 90.f, 0.f).Vector();
			const FVector DoorAt = Trunk + DoorDir * Ro + FVector(0, 0, FloorZ);
			for (const float S : { -1.f, 1.f }) K.Rod(Carved(), DoorAt + DoorSide * S * 62.f, DoorAt + DoorSide * S * 62.f + FVector(0, 0, 205.f), 14.f, true);
			K.Add(Carved(0.9f), FVector(16.f, 150.f, 18.f), DoorAt + FVector(0, 0, 210.f), FRotator(0.f, Yaw + DoorA + 90.f - 90.f, 0.f));
			if (!bBroken) K.Add(Bark(0.9f), FVector(6.f, 110.f, 190.f), DoorAt + DoorSide * 90.f + DoorDir * 40.f + FVector(0, 0, 95.f), FRotator(0.f, Yaw + DoorA + 60.f, 0.f));
			else K.Add(Bark(0.8f), FVector(110.f, 190.f, 6.f), DoorAt + DoorDir * 120.f + FVector(0, 0, 4.f), FRotator(0.f, Yaw + DoorA + 20.f, 3.f));
			// The moss roof: a cone of turf slabs from the wall top up to the trunk, eaves out past the wall.
			const int32 Seg = 24;
			const float RoofIn = TrunkR + 10.f, RoofOut = Ro + 70.f;
			const float ZIn = FloorZ + WallH + 170.f, ZOut = FloorZ + WallH - 20.f;
			const float SlopeLen = FVector2D(RoofOut - RoofIn, ZIn - ZOut).Size();
			const float RoofPitch = FMath::RadiansToDegrees(FMath::Atan2(ZIn - ZOut, RoofOut - RoofIn));
			for (int32 i = 0; i < Seg; ++i)
			{
				const float A = 360.f * (i + 0.5f) / Seg;
				const FRotator Face(0.f, Yaw + A, 0.f);
				const float Chord = 2.f * RoofOut * FMath::Sin(FMath::DegreesToRadians(180.f / Seg)) * 1.08f;
				const FVector Mid = Trunk + Face.Vector() * ((RoofIn + RoofOut) * 0.5f) + FVector(0, 0, (ZIn + ZOut) * 0.5f);
				K.Add(DeepMoss(1.f + 0.15f * FMath::Frac(A * 0.21f)), FVector(SlopeLen, Chord, 22.f), Mid, FRotator(RoofPitch, Yaw + A, 0.f));
				K.Add(Bark(0.8f), FVector(SlopeLen, 10.f, 10.f), Mid - FVector(0, 0, 18.f), FRotator(RoofPitch, Yaw + A, 0.f));   // the rafter under
			}
			if (bLit) Lamp(Trunk + DoorDir * (TrunkR + Out * 0.5f) + FVector(0, 0, FloorZ + 160.f), FLinearColor(1.f, 0.6f, 0.28f), 2000.f, 800.f);
		}

		/** A plank deck at DeckZ round (or beside) a trunk, carried on joists let into it, with rope rails and iron rings
		 *  for the walkways (the canopy layer's platforms: DC 2.4 kit F5, plan 2.5). Centre and HalfSize local. */
		void Deck(const FVector& Centre, const FVector2D& Half, float DeckZ, float Yaw, const TArray<FVector>& Trunks, const TArray<float>& Radii, bool bRails)
		{
			const FRotator R(0.f, Yaw, 0.f);
			const auto P = [&](float X, float Y, float Z) { return FVector(Centre.X, Centre.Y, DeckZ) + R.RotateVector(FVector(X, Y, Z)); };
			for (float X = float(-Half.X) + 15.f; X < float(Half.X); X += 30.f) K.Add(Plank(0.88f + 0.24f * FMath::Frac(X * 0.013f)), FVector(28.f, float(Half.Y) * 2.f, 6.f), P(X, 0, -3.f), R);
			for (const float Y : { float(-Half.Y) + 20.f, 0.f, float(Half.Y) - 20.f }) K.Add(Timber(), FVector(Half.X * 2.f, 20.f, 26.f), P(0, Y, -19.f), R);
			Box(P(0, 0, -6.f), FVector(Half.X, Half.Y, 6.f), R);
			// Beams from the deck to each trunk it hangs on, let in with iron bands; a knee brace below.
			for (int32 t = 0; t < Trunks.Num(); ++t)
			{
				const FVector Tk(Trunks[t].X, Trunks[t].Y, DeckZ - 20.f);
				const FVector From = FVector(Centre.X, Centre.Y, DeckZ - 20.f);
				const FVector Dir = (Tk - From).GetSafeNormal2D();
				const FVector Socket = Tk - Dir * Radii[t] * 0.85f;
				for (const float S : { -1.f, 1.f })
				{
					const FVector Off = FVector::CrossProduct(Dir, FVector::UpVector) * S * 70.f;
					K.Rod(Timber(), From + Off, Socket + Off, 24.f, true);
					K.Add(Iron(), FVector(10.f, 30.f, 30.f), Socket + Off - Dir * 25.f, Dir.Rotation());
				}
				K.Rod(Timber(0.9f), Socket - FVector(0, 0, 320.f), FMath::Lerp(From, Socket, 0.4f), 18.f);
			}
			if (!bRails) return;
			// Rails: posts at the corners and mid-sides, a rope at hand height; one iron ring per side for walkway ropes.
			const FVector2D Corners[4] = { FVector2D(-Half.X, -Half.Y), FVector2D(Half.X, -Half.Y), FVector2D(Half.X, Half.Y), FVector2D(-Half.X, Half.Y) };
			for (int32 c = 0; c < 4; ++c)
			{
				const FVector2D C0 = Corners[c], C1 = Corners[(c + 1) % 4];
				K.Rod(Timber(), P(float(C0.X), float(C0.Y), 0.f), P(float(C0.X), float(C0.Y), 115.f), 12.f);
				K.Disc(Iron(), P(float(C0.X + C1.X) * 0.5f, float(C0.Y + C1.Y) * 0.5f, 100.f), FVector::UpVector, 16.f, 3.f);
				Line(P(float(C0.X), float(C0.Y), 105.f), P(float(C1.X), float(C1.Y), 105.f), 2.5f, 8.f, RopeC());
			}
		}

		/** A walkway deck from A to B (local, deck heights): planks on two stringer ropes sagging in a catenary, rope hand
		 *  rails at 110 cm tied down to the deck edge every 1.5 m, each end tied to an iron ring. Collision follows the sag. */
		void Walkway(const FVector& P0, const FVector& P1)
		{
			const float Len = FVector::Dist(P0, P1);
			const float SagD = Len * WRTune(TEXT("forestWorld.walkwaySag"), 0.04f);
			const FVector Dir = (P1 - P0).GetSafeNormal();
			const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
			const int32 Planks = FMath::Max(4, FMath::RoundToInt(Len / 32.f));
			for (int32 i = 0; i < Planks; ++i)
			{
				const float Ti = (i + 0.5f) / Planks;
				const FVector C = Sag(P0, P1, Ti, SagD);
				const FVector Ahead = Sag(P0, P1, FMath::Min(1.f, Ti + 0.02f), SagD) - Sag(P0, P1, FMath::Max(0.f, Ti - 0.02f), SagD);
				K.Add(Plank(0.85f + 0.25f * FMath::Frac(i * 0.61f)), FVector(26.f, 120.f, 5.f), C - FVector(0, 0, 2.5f), FRotator(Ahead.Rotation().Pitch, Ahead.Rotation().Yaw, 0.f));
			}
			for (const float S : { -1.f, 1.f })
			{
				Line(P0 + Side * S * 58.f - FVector(0, 0, 6.f), P1 + Side * S * 58.f - FVector(0, 0, 6.f), 3.f, SagD, RopeC(), 16);
				Line(P0 + Side * S * 62.f + FVector(0, 0, 110.f), P1 + Side * S * 62.f + FVector(0, 0, 110.f), 2.5f, SagD * 0.8f, RopeC(), 16);
				const int32 Ties = FMath::Max(2, FMath::RoundToInt(Len / 150.f));
				for (int32 i = 1; i < Ties; ++i)
				{
					const float Ti = float(i) / Ties;
					const FVector Low = Sag(P0, P1, Ti, SagD) + Side * S * 58.f;
					const FVector High = Sag(P0 + FVector(0, 0, 110.f), P1 + FVector(0, 0, 110.f), Ti, SagD * 0.8f) + Side * S * 62.f;
					M.Tube({ Low, High }, 1.4f, 4, RopeC(), 0.95f);
				}
			}
			const int32 Segs = FMath::Max(2, FMath::RoundToInt(Len / 250.f));
			for (int32 i = 0; i < Segs; ++i)
			{
				const FVector A0 = Sag(P0, P1, float(i) / Segs, SagD), A1 = Sag(P0, P1, float(i + 1) / Segs, SagD);
				SegBox(A0 - FVector(0, 0, 6.f), A1 - FVector(0, 0, 6.f), 62.f, 6.f);
				for (const float S : { -1.f, 1.f }) SegBox(A0 + Side * S * 64.f + FVector(0, 0, 60.f), A1 + Side * S * 64.f + FVector(0, 0, 60.f), 3.f, 55.f);
			}
		}

		// ---- the kinds (WRForestScenery.cpp below) ----
		void Eldest(); void GreatFalls(); void Fall(); void RottedTreant(); void HermitsOak(); void StranglerArch(); void AntlerBarrow();
		void BellTree(); void Ringfort(); void WreckMeadow(); void Farmstead(); void MillRace(); void TruffleGlade(); void WoodcuttersCamp();
		void HangingOak(); void HernesFord(); void FallsPool(); void RootBridge(); void OtterSteps(); void FrostedFerns(); void NurseLog();
		void BearsLarder(); void WaspRavine(); void CoilHollow(); void OfferingStones(); void LongHall(); void CradleWood(); void HerbTerraces();
		void BreadStoneLane(); void RootMine(); void SunkenOrchard(); void CrystalRoots(); void RootgraveFort(); void LanternLine(); void FernStair();
		void CanopyLiftFoot(); void SkyLarder(); void Sinkroot(); void Camp(); void Village(); void CaveMouth(); void TrailTree(); void FallenGiant();
		void FallenLog(bool bNurse); void HedgerowGap(); void BeehiveTree(); void HouseTree(); void KillScene(); void Ford(); void FoxfireBank();
		void FieldRuin(); void CanopyPlatform(); void WalkwayKind(); void ShrineGroveCircle(); void ShrineHedge(); void ShrineFalls();
		void ShrineNurse(); void ShrineCanopy();
	};
}
