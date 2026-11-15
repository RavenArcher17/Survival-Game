// A runtime land's places (cooks/build-biome-void.md 2.4-2.5; design in cooks/biome-void.md 2.3-2.5, 2.20). Every place
// is composed here from the look kit (stone, glass, wood, cloth, lit surfaces) and the packs' rocks and dead trees, at
// the design's scale, each with its night light. The land file names each place's kind, position and facing (world-
// map metres); this file holds what each kind looks like. Collision is boxes where you walk or are stopped; small
// things are look only. Everything that floats is a land mover (a function of the server's clock).
//
// The Choir's look (biome-void 2.1, 2.20): bone-grey dolomite with chant-grooves, voidglass inlay in doorways and
// keels, horn shapes aimed at rifts, point-down cairns; their dead are shrouded and seated facing the tears. The
// newcomers': greywood, rope, sailcloth, salvaged hulls. Violet only at the void's edges.
#include "WRLandSites.h"
#include "WRLand.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRFx.h"
#include "WRPortalsInternal.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

using WRLookKit::FSurf;
using WRLookKit::FKit;

namespace WRLandSitesImpl
{
	FLinearColor Lin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	/** The land's surfaces (palette law: grey land, black glass, violet only at the void's edges). */
	struct FPal
	{
		FSurf Dolo = FSurf::Stone(Lin(201, 195, 184), 0.88f);
		FSurf DoloDark = FSurf::Stone(Lin(150, 145, 137), 0.9f);
		FSurf Bone = FSurf::Stone(Lin(217, 207, 184), 0.62f);
		FSurf Glass = FSurf::Glass(Lin(11, 10, 16), 0.f);
		FSurf GlassEdge = FSurf::Lit(Lin(123, 79, 214), 0.55f);
		FSurf Void = FSurf::Lit(Lin(43, 24, 64), 0.25f);
		FSurf Greywood = FSurf::Wood(Lin(142, 138, 132));
		FSurf GreywoodDark = FSurf::Wood(Lin(96, 92, 88));
		FSurf Iron = FSurf::Steel(Lin(32, 30, 30), 0.6f);
		FSurf Bronze = FSurf::Steel(Lin(110, 72, 40), 0.45f);
		FSurf Ash = FSurf::Stone(Lin(110, 106, 102), 0.95f);
		FSurf Shroud = FSurf::Cloth(Lin(163, 158, 149));
		FSurf ShroudDark = FSurf::Cloth(Lin(120, 116, 110));
		FSurf Sail = FSurf::Cloth(Lin(176, 170, 156));
		FSurf Rope = FSurf::Cloth(Lin(120, 104, 80));
		FSurf Salt = FSurf::Stone(Lin(236, 234, 228), 0.7f);
		FSurf Char = FSurf::Stone(Lin(30, 28, 27), 0.95f);
		FSurf Crystal = FSurf::Glass(Lin(214, 206, 236), 0.12f);
		FSurf Fire = FSurf::Lit(Lin(255, 150, 60), 1.2f);
		FSurf Ember = FSurf::Lit(Lin(255, 96, 30), 0.8f);
		FSurf Flesh = FSurf::Stone(Lin(180, 172, 164), 0.7f);
		FSurf Leather = FSurf::Stone(Lin(92, 80, 68), 0.75f);
	};

	/** One place being built: a scene component at its ground point facing its "face" point, the blocks it collects,
	 *  the land and its definition. Local X is the facing, Z up; Gz(x, y) is the ground's height there in local cm. */
	struct FSite
	{
		AWRLand* Land = nullptr;
		const FWRLandDef* D = nullptr;
		USceneComponent* Root = nullptr;
		TArray<FWRLookBlock> Blocks;
		FTransform World;
		FPal P;
		bool bDraw = true;
		FKit K;
		FSite(AWRLand* InLand, const FTransform& T, bool bInDraw) : Land(InLand), D(InLand->Def()), World(T), bDraw(bInDraw), K(World, Blocks)
		{
			Root = NewObject<USceneComponent>(Land);
			Root->SetupAttachment(Land->GetRootComponent());
			Root->SetWorldTransform(T);
			Root->RegisterComponent();
		}
		float Gz(float X, float Y) const
		{
			const FVector W = World.TransformPosition(FVector(X, Y, 0.f));
			return Land->GroundZ(W) - World.GetLocation().Z;
		}
		FVector G(float X, float Y, float Up = 0.f) const { return FVector(X, Y, Gz(X, Y) + Up); }
		void Box(const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator)
		{
			WRFoldLook::Block(Land, Root, Centre, Half, Rot);
		}
		void Light(const FVector& At, const FLinearColor& C, float Intensity, float Radius)
		{
			if (bDraw) WRFoldLook::Light(Land, Root, At, C, Intensity, Radius);
		}
		void Fire(const FVector& At, float Scale = 1.f)
		{
			if (!bDraw) return;
			if (USceneComponent* F = WRFx::Attach(Root, TEXT("brazier.fire"), NAME_None, Scale)) F->SetRelativeLocation(At);
			WRFoldLook::Light(Land, Root, At + FVector(0, 0, 60.f), Lin(255, 150, 70), 5200.f * Scale, 1800.f * Scale);
		}
		/** Finish: the blocks become instanced shapes (culled at Cull cm). */
		void Done(float Cull = 40000.f)
		{
			if (!bDraw || Blocks.Num() == 0) return;
			const int32 Before = Root->GetNumChildrenComponents();
			FWRBuildLook::Spawn(Land, Root, Blocks, Lin(163, 158, 149), false);
			for (int32 i = Before; i < Root->GetNumChildrenComponents(); ++i)
			{
				if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Root->GetChildComponent(i)))
				{
					I->SetCullDistances(0, int32(Cull));
					I->SetCachedMaxDrawDistance(Cull);
				}
			}
			Blocks.Reset();
		}
	};

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback;
	}
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString S;
		if (O.IsValid()) O->TryGetStringField(Key, S);
		return S;
	}

	// ============================================================================================ the vocabulary

	/** A Choir waymark: a cairn balanced point-down (the broad stone on top, the cone's tip in the earth). */
	void Cairn(FSite& S, const FVector& At, float H, float Turn, bool bRisen = false)
	{
		const float Base = H * 0.38f;
		// A flat footing slab, the inverted cone stone standing on its tip in a little socket, then two flat stones.
		S.K.Add(S.P.DoloDark, FVector(Base * 1.3f, Base * 1.1f, 14.f), At + FVector(0, 0, 5.f), FRotator(0.f, Turn, 0.f));
		S.K.Spike(S.P.Dolo, At + FVector(0, 0, H * 0.62f), FVector(0, 0, -1.f), Base, H * 0.6f);
		const float Lift = bRisen ? 60.f + H * 0.4f : 0.f;   // the floating cairn field: their top stones risen off them
		S.K.Add(S.P.Dolo, FVector(Base * 1.25f, Base * 1.0f, H * 0.14f), At + FVector(0, 0, H * 0.7f + Lift), FRotator(4.f, Turn + 25.f, 0.f));
		S.K.Add(S.P.DoloDark, FVector(Base * 0.8f, Base * 0.7f, H * 0.12f), At + FVector(0, 0, H * 0.84f + Lift * 1.4f), FRotator(-3.f, Turn - 30.f, 2.f));
		if (bRisen)
		{
			// The keel that lifted them: a black glass shard grown into the stone's underside.
			S.K.Spike(S.P.Glass, At + FVector(0, 0, H * 0.64f + Lift), FVector(0, 0, -1.f), Base * 0.35f, Lift * 0.6f + 12.f);
		}
	}

	/** One of the Choir's dead: seated, shrouded in grey cloth, hood forward, hands open in the lap. Drawn = 0..1 how far
	 *  a tear has pulled it (its front half leaning into the dark, the cloth streaming toward it). */
	void Seated(FSite& S, const FVector& At, float Yaw, float Scale = 1.f, float Drawn = 0.f, bool bMouthOpen = false)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto L = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z) * Scale); };
		const float Lean = 8.f + 40.f * Drawn;
		// The seat: a squared dolomite block.
		S.K.Add(S.P.DoloDark, FVector(52.f, 56.f, 42.f) * Scale, L(-6.f, 0.f, 21.f), R);
		// Thighs forward, shins down, under one shroud.
		S.K.Add(S.P.Shroud, FVector(46.f, 44.f, 18.f) * Scale, L(16.f, 0.f, 50.f), R);
		S.K.Add(S.P.ShroudDark, FVector(16.f, 40.f, 44.f) * Scale, L(40.f, 0.f, 22.f), R + FRotator(-8.f, 0.f, 0.f));
		// The torso: a tapering cloth cone, leaning forward.
		S.K.Add(S.P.Shroud, FVector(44.f, 48.f, 60.f) * Scale, L(-6.f + Lean * 0.4f, 0.f, 84.f), R + FRotator(-Lean, 0.f, 0.f), EWRLookMesh::Cone);
		// Hood and the face's shadow inside it.
		const FVector Head = L(4.f + Lean * 0.9f, 0.f, 120.f - Lean * 0.2f);
		S.K.Add(S.P.ShroudDark, FVector(24.f, 26.f, 30.f) * Scale, Head, R + FRotator(-Lean - 10.f, 0.f, 0.f), EWRLookMesh::Cone);
		S.K.Add(S.P.Char, FVector(8.f, 14.f, bMouthOpen ? 16.f : 10.f) * Scale, Head + R.RotateVector(FVector(10.f, 0.f, -4.f) * Scale), R);
		// Hands open, palms up, in the lap.
		for (const float Side : { -1.f, 1.f })
		{
			S.K.Add(S.P.Flesh.Dim(0.75f), FVector(12.f, 8.f, 3.f) * Scale, L(22.f, Side * 8.f, 62.f), R + FRotator(0.f, Side * 12.f, 0.f));
		}
		if (Drawn > 0.3f)
		{
			// The shroud's hem drawn out toward the tear in strips.
			for (int32 i = 0; i < 3; ++i)
			{
				S.K.Add(S.P.ShroudDark, FVector(70.f * Drawn, 10.f, 2.f) * Scale, L(60.f + 30.f * Drawn, -14.f + 14.f * i, 70.f + 6.f * i), R + FRotator(10.f, 4.f * i, 0.f));
			}
		}
	}

	/** A standing dead pine (the Red Forest rule: they never fell, never rotted): the pack's bare tree, silver-grey. */
	struct FForest
	{
		TArray<UStaticMesh*> Trees;
		UMaterialInstanceDynamic* Bark = nullptr;
		UMaterialInstanceDynamic* Burnt = nullptr;
		TArray<UInstancedStaticMeshComponent*> Parts;
		TArray<UInstancedStaticMeshComponent*> BurntParts;
		void Init(AWRLand* Land)
		{
			for (const TCHAR* N : { TEXT("CharTreeA/char_tree_a_textured"), TEXT("CharTreeB/char_tree_b_textured"), TEXT("CharTreeC/char_tree_c_textured"), TEXT("CharTreeD/char_tree_d_textured") })
			{
				FString Folder, Name;
				FString(N).Split(TEXT("/"), &Folder, &Name);
				if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Props/%s/%s/StaticMeshes/%s.%s"), *Folder, *Name, *Name, *Name))) Trees.Add(M);
			}
			Bark = WRFoldLook::RockMat(Land, Lin(142, 138, 132) * 0.55f);
			Burnt = WRFoldLook::RockMat(Land, Lin(30, 28, 27) * 0.6f);
		}
		UInstancedStaticMeshComponent* Part(AWRLand* Land, int32 i, bool bBurnt)
		{
			TArray<UInstancedStaticMeshComponent*>& Set = bBurnt ? BurntParts : Parts;
			if (!Trees.Num()) return nullptr;
			while (Set.Num() <= i)
			{
				UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(Land);
				H->SetStaticMesh(Trees[Set.Num() % Trees.Num()]);
				H->SetupAttachment(Land->GetRootComponent());
				H->SetMobility(EComponentMobility::Static);
				H->SetCollisionProfileName(TEXT("BlockAll"));
				H->SetCanEverAffectNavigation(false);
				if (UMaterialInstanceDynamic* M = bBurnt ? Burnt : Bark) for (int32 m = 0; m < H->GetNumMaterials(); ++m) H->SetMaterial(m, M);
				H->SetCullDistances(0, 32000);
				H->RegisterComponent();
				Set.Add(H);
			}
			return Set[i];
		}
		/** A tree at a world point: Height cm tall, leaning Lean degrees toward LeanYaw. */
		void Add(AWRLand* Land, const FVector& WorldFoot, float Height, float Yaw, float Lean, float LeanYaw, int32 Which, bool bBurnt = false)
		{
			UInstancedStaticMeshComponent* C = Part(Land, Which % FMath::Max(1, Trees.Num()), bBurnt);
			if (!C) return;
			const FBox B = Trees[Which % Trees.Num()]->GetBoundingBox();
			const float K = Height / FMath::Max(1.f, B.GetSize().Z);
			const FQuat Q = FQuat(FRotator(0.f, LeanYaw, 0.f)) * FQuat(FRotator(-Lean, 0.f, 0.f)) * FQuat(FRotator(0.f, Yaw - LeanYaw, 0.f));
			C->AddInstance(FTransform(Q, WorldFoot - FVector(0, 0, 30.f), FVector(K)), true);
		}
	};

	/** A boulder of the SimpleRocks set, grey, at a local point (its own component; collides). */
	UStaticMeshComponent* Boulder(FSite& S, int32 Which, const FVector& Size, const FVector& Base, float Yaw, UMaterialInterface* Mat, bool bCollide = true)
	{
		return WRFoldLook::Rock(S.Land, S.Root, Which, Size, Base, FRotator(0.f, Yaw, 0.f), Mat, bCollide, Size.Z * 0.12f);
	}

	/** A stone held up by a voidglass keel grown into its underside (the floating rule): a mover, bobbing. */
	USceneComponent* KeeledStone(FSite& S, int32 Which, const FVector& Size, const FVector& At, float Yaw, UMaterialInterface* Mat, float Bob, float Period, float Phase, bool bCollide, float Spin = 0.f)
	{
		USceneComponent* Holder = NewObject<USceneComponent>(S.Land);
		Holder->SetupAttachment(S.Root);
		Holder->SetRelativeLocation(At);
		Holder->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		Holder->RegisterComponent();
		WRFoldLook::Rock(S.Land, Holder, Which, Size, FVector::ZeroVector, FRotator::ZeroRotator, Mat, bCollide, Size.Z * 0.1f);
		if (S.bDraw)
		{
			TArray<FWRLookBlock> KB;
			FKit KK(Holder->GetComponentTransform(), KB);
			const float Keel = FMath::Max(Size.X, Size.Y) * 0.55f;
			KK.Spike(S.P.Glass, FVector(0, 0, 4.f), FVector(0.f, 0.f, -1.f), Keel * 0.35f, Keel);
			KK.Spike(S.P.Glass, FVector(Keel * 0.18f, Keel * 0.1f, 2.f), FVector(0.25f, 0.1f, -1.f), Keel * 0.18f, Keel * 0.55f);
			KK.Add(S.P.GlassEdge, FVector(Keel * 0.36f, Keel * 0.36f, 3.f), FVector(0, 0, -Keel * 0.15f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			FWRBuildLook::Spawn(S.Land, Holder, KB, FLinearColor::Gray, false);
		}
		FWRLandMover M;
		M.Comp = Holder;
		M.Base = At;
		M.Rot = FRotator(0.f, Yaw, 0.f);
		M.Bob = Bob;
		M.BobPeriod = Period;
		M.Phase = Phase;
		M.Spin = Spin;
		S.Land->AddMover(M);
		return Holder;
	}

	/** A coursed dolomite wall from A to B (local), H high, Thick deep; Ruin 0..1 knocks the top down in steps. */
	void Wall(FSite& S, const FVector2D& A, const FVector2D& B, float H, float Thick, float Ruin, bool bCollide = true, const FSurf* Surf = nullptr)
	{
		const FVector2D Dir = (B - A).GetSafeNormal();
		const float Len = FVector2D::Distance(A, B);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		const FSurf& Su = Surf ? *Surf : S.P.Dolo;
		const int32 Rows = FMath::Max(1, int32(H / 60.f));
		float MaxTop = 0.f;
		for (int32 r = 0; r < Rows; ++r)
		{
			for (float D = (r % 2) * -45.f; D < Len; D += 90.f)
			{
				const float D0 = FMath::Max(D, 0.f), D1 = FMath::Min(D + 90.f, Len);
				if (D1 - D0 < 15.f) continue;
				const FVector2D C2 = A + Dir * ((D0 + D1) * 0.5f);
				// The ruin line: a slow wave along the wall, lower where it's broken.
				const float Keep = H * (1.f - Ruin * (0.5f + 0.5f * S.D->Noise(C2 / 220.f + FVector2D(r * 0.01f, 3.f))));
				if (r * 60.f > Keep) continue;
				MaxTop = FMath::Max(MaxTop, r * 60.f + 60.f);
				const float Z = S.Gz(C2.X, C2.Y) + r * 60.f + 30.f;
				S.K.Add(Su.Dim(0.86f + 0.2f * S.K.Rand(FVector(C2, Z), 31)), FVector(D1 - D0 - 3.f, Thick + 8.f * S.K.Rand(FVector(C2, Z), 33), 57.f), FVector(C2.X, C2.Y, Z), FRotator(0.f, Yaw, 0.f));
			}
		}
		if (bCollide && MaxTop > 0.f)
		{
			const FVector2D M = (A + B) * 0.5f;
			S.Box(FVector(M.X, M.Y, S.Gz(M.X, M.Y) + MaxTop * 0.5f - 40.f), FVector(Len * 0.5f, Thick * 0.5f, MaxTop * 0.5f + 40.f), FRotator(0.f, Yaw, 0.f));
		}
	}

	/** A hollow fluted column: drums with shallow flutes, H high, open at the top (it moans in the Grey wind). */
	void Column(FSite& S, const FVector& Base, float H, float Diam, bool bBroken, bool bCollide = true)
	{
		const float Top = bBroken ? H * (0.35f + 0.5f * S.K.Rand(Base, 41)) : H;
		S.K.Add(S.P.DoloDark, FVector(Diam * 1.35f, Diam * 1.35f, 30.f), Base + FVector(0, 0, 15.f));
		for (float Z = 30.f; Z < Top; Z += 90.f)
		{
			const float DZ = FMath::Min(90.f, Top - Z);
			S.K.Add(S.P.Dolo.Dim(0.9f + 0.15f * S.K.Rand(Base + FVector(0, 0, Z), 43)), FVector(Diam, Diam, DZ - 2.f), Base + FVector(0, 0, Z + DZ * 0.5f), FRotator(0.f, 15.f * (int32(Z) % 3), 0.f), EWRLookMesh::Cylinder);
			for (int32 f = 0; f < 8; ++f)
			{
				const float A = 2.f * PI * f / 8.f;
				S.K.Add(S.P.DoloDark, FVector(4.f, Diam * 0.12f, DZ - 6.f), Base + FVector(FMath::Cos(A) * Diam * 0.49f, FMath::Sin(A) * Diam * 0.49f, Z + DZ * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
		}
		if (!bBroken)
		{
			S.K.Add(S.P.Dolo, FVector(Diam * 1.4f, Diam * 1.4f, 26.f), Base + FVector(0, 0, Top + 13.f));
			S.K.Add(S.P.Char, FVector(Diam * 0.5f, Diam * 0.5f, 4.f), Base + FVector(0, 0, Top + 27.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // the hollow mouth
		}
		if (bCollide) S.Box(Base + FVector(0, 0, Top * 0.5f), FVector(Diam * 0.45f, Diam * 0.45f, Top * 0.5f));
	}

	/** A horn-tower: rings of dolomite blocks tapering from R0 at the foot to R1 at H, its mouth tilted toward Aim. */
	void Horn(FSite& S, const FVector& Base, float R0, float R1, float H, float RingH, int32 Segs, bool bCollide, float TopBreak = 1.f)
	{
		const float Top = H * TopBreak;
		for (float Z = 0.f; Z < Top; Z += RingH)
		{
			const float T = Z / H;
			const float R = FMath::Lerp(R0, R1, FMath::Pow(T, 0.8f));
			const float Thick = FMath::Lerp(R0 * 0.18f, R1 * 0.35f, T);
			const int32 N = FMath::Max(8, int32(Segs * R / R0));
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 2.f * PI * (i + (int32(Z / RingH) % 2) * 0.5f) / N;
				const float Chord = 2.f * R * FMath::Sin(PI / N) - 4.f;
				const FVector C = Base + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z + RingH * 0.5f);
				// A chant-groove band every fourth ring: a darker, slightly inset course.
				const bool bGroove = int32(Z / RingH) % 4 == 3;
				S.K.Add((bGroove ? S.P.DoloDark : S.P.Dolo).Dim(0.86f + 0.18f * S.K.Rand(C, 51)), FVector(Thick, Chord, RingH - 3.f), C - FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (bGroove ? 6.f : 0.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
		}
		if (bCollide)
		{
			// The tower's mass: a stack of boxes (each ring band's square inside its circle), so you can't walk through it.
			for (float Z = 0.f; Z < Top; Z += RingH * 4.f)
			{
				const float R = FMath::Lerp(R0, R1, FMath::Pow(Z / H, 0.8f));
				S.Box(Base + FVector(0, 0, Z + RingH * 2.f), FVector(R * 0.72f, R * 0.72f, RingH * 2.f));
			}
		}
	}

	/** A newcomer ship's hull on its side: keel, ribs, planking, a mast, Len long, Heel degrees over. */
	void Hull(FSite& S, const FVector& At, float Yaw, float Len, float Beam, float Heel, bool bBroken)
	{
		const FRotator R(0.f, Yaw, Heel);
		const auto L = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z)); };
		S.K.Add(S.P.GreywoodDark, FVector(Len, 30.f, 34.f), L(0.f, 0.f, 16.f), R);   // keel
		const int32 Ribs = int32(Len / 120.f);
		for (int32 i = 0; i <= Ribs; ++i)
		{
			const float X = -Len * 0.5f + Len * i / Ribs;
			if (bBroken && FMath::Abs(X) < Len * 0.08f) continue;
			const float Taper = 1.f - FMath::Pow(FMath::Abs(X) / (Len * 0.5f), 2.f) * 0.75f;
			const float W = Beam * Taper * 0.5f;
			for (int32 k = 0; k < 6; ++k)
			{
				const float A0 = PI * k / 6.f, A1 = PI * (k + 1) / 6.f;
				const FVector P0 = L(X, -FMath::Cos(A0) * W, FMath::Sin(A0) * W * 0.95f + 20.f), P1 = L(X, -FMath::Cos(A1) * W, FMath::Sin(A1) * W * 0.95f + 20.f);
				S.K.Rod(S.P.GreywoodDark, P0, P1, 16.f, true);
			}
		}
		// Planks over the ribs on the lower side, half of the upper side's gone (the weather took them).
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = PI * (k + 0.5f) / 6.f;
			const bool bUpper = k >= 3;
			for (float X = -Len * 0.42f; X < Len * 0.42f; X += 140.f)
			{
				if (bUpper && S.K.Rand(FVector(X, k, 0), 61) < 0.55f) continue;
				if (bBroken && FMath::Abs(X) < Len * 0.1f) continue;
				const float Taper = 1.f - FMath::Pow(FMath::Abs(X) / (Len * 0.5f), 2.f) * 0.75f;
				const float W = Beam * Taper * 0.5f;
				S.K.Add(S.P.Greywood.Dim(0.85f + 0.2f * S.K.Rand(FVector(X, k, 1), 63)), FVector(138.f, 8.f, W * PI / 6.f + 4.f), L(X + 70.f, -FMath::Cos(A) * (W + 6.f), FMath::Sin(A) * W * 0.95f + 20.f), R + FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) - 90.f));
			}
		}
		// The mast, snapped, lying along the sand.
		S.K.Rod(S.P.Greywood, L(Len * 0.1f, 0.f, 40.f), L(Len * 0.1f + 30.f, Beam * 1.4f, 80.f), 26.f);
		S.Box(L(0.f, 0.f, Beam * 0.3f), FVector(Len * 0.5f, Beam * 0.5f, Beam * 0.3f), R);
	}

	/** Offering bowls: a stack of N licked-clean stone bowls. */
	void Bowls(FSite& S, const FVector& At, int32 N, float Turn)
	{
		for (int32 i = 0; i < N; ++i)
		{
			S.K.Add(S.P.Dolo.Dim(0.9f + 0.1f * S.K.Rand(At, i)), FVector(34.f, 34.f, 10.f), At + FVector(0, 0, 5.f + i * 8.f), FRotator(0.f, Turn + i * 13.f, 0.f), EWRLookMesh::Cylinder);
			S.K.Add(S.P.DoloDark, FVector(26.f, 26.f, 2.f), At + FVector(0, 0, 10.5f + i * 8.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
	}

	/** A Choir brazier: a black iron bowl on a tripod (the Great Shrine's kind), lit. */
	void Brazier(FSite& S, const FVector& At, bool bLit = true)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = 2.f * PI * i / 3.f;
			S.K.Rod(S.P.Iron, At + FVector(FMath::Cos(A) * 40.f, FMath::Sin(A) * 40.f, 0.f), At + FVector(0, 0, 110.f), 6.f);
		}
		S.K.Add(S.P.Iron, FVector(70.f, 70.f, 22.f), At + FVector(0, 0, 118.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		if (bLit)
		{
			S.K.Add(S.P.Ember, FVector(56.f, 56.f, 4.f), At + FVector(0, 0, 130.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.Fire(At + FVector(0, 0, 132.f), 0.8f);
		}
	}

	/** A greywood lean-to with a bedroll and a cold fire ring. */
	void LeanTo(FSite& S, const FVector& At, float Yaw)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto L = [&](float X, float Y, float Z) { return At + R.RotateVector(FVector(X, Y, Z)); };
		for (const float Y : { -160.f, 160.f })
		{
			S.K.Rod(S.P.Greywood, L(-100.f, Y, 0.f), L(-100.f, Y, 210.f), 14.f);
			S.K.Rod(S.P.Greywood, L(-100.f, Y, 210.f), L(120.f, Y, 0.f), 12.f);
		}
		S.K.Rod(S.P.Greywood, L(-100.f, -180.f, 205.f), L(-100.f, 180.f, 205.f), 12.f);
		for (float T = 0.1f; T < 1.f; T += 0.12f)
		{
			S.K.Rod(S.P.GreywoodDark, L(FMath::Lerp(-100.f, 120.f, T), -170.f, FMath::Lerp(210.f, 0.f, T)), L(FMath::Lerp(-100.f, 120.f, T), 170.f, FMath::Lerp(210.f, 0.f, T)), 9.f);
		}
		S.K.Add(S.P.Sail.Dim(0.8f), FVector(190.f, 80.f, 6.f), L(-40.f, 0.f, 4.f), R);   // the bedroll
		S.K.Add(S.P.Rope, FVector(30.f, 80.f, 18.f), L(-120.f, 0.f, 9.f), R, EWRLookMesh::Cylinder);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2.f * PI * i / 8.f;
			S.K.Add(S.P.DoloDark, FVector(22.f, 18.f, 14.f), L(230.f + FMath::Cos(A) * 50.f, FMath::Sin(A) * 50.f, 6.f), R + FRotator(0.f, 40.f * i, 0.f));
		}
		S.K.Add(S.P.Char, FVector(70.f, 70.f, 4.f), L(230.f, 0.f, 2.f), R, EWRLookMesh::Cylinder);
		S.Box(L(10.f, 0.f, 100.f), FVector(120.f, 170.f, 100.f), R + FRotator(0.f, 0.f, 0.f));
	}

	/** A seated row facing a point: N seats in an arc of Radius, centred on the facing. */
	void SeatedArc(FSite& S, const FVector2D& Focus, float Radius, float ArcDeg, int32 N, float Drawn, bool bMouths = false)
	{
		const float Face = 0.f;   // local X points at the focus already
		for (int32 i = 0; i < N; ++i)
		{
			const float A = FMath::DegreesToRadians(180.f + Face + (N > 1 ? -ArcDeg * 0.5f + ArcDeg * i / (N - 1) : 0.f));
			const FVector2D P = Focus + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Focus.Y - P.Y, Focus.X - P.X));
			Seated(S, S.G(P.X, P.Y), Yaw, 1.f + 0.06f * S.K.Jit(FVector(P, 0), i), i < 3 ? Drawn : 0.f, bMouths);
		}
	}

	/** A satchel cache (a lootable bag sits on it at runtime through the world systems' caches). */
	void Satchel(FSite& S, const FVector& At, float Yaw)
	{
		S.K.Add(S.P.Leather, FVector(40.f, 26.f, 30.f), At + FVector(0, 0, 15.f), FRotator(0.f, Yaw, 8.f));
		S.K.Add(S.P.Leather.Dim(0.7f), FVector(42.f, 4.f, 20.f), At + FVector(0, 14.f, 26.f), FRotator(0.f, Yaw, 30.f));
		S.K.Rod(S.P.Rope, At + FVector(-10.f, 0.f, 32.f), At + FVector(10.f, -30.f, 80.f), 3.f);
	}
}

// ================================================================================================ the places

namespace WRLandSitesImpl
{
	FTransform SiteFrame(AWRLand* Land, const TSharedPtr<FJsonObject>& P)
	{
		const FWRLandDef& D = *Land->Def();
		const FVector2D At = WRLand::Vec2(P->TryGetField(TEXT("at")));
		float Yaw = Num(P, TEXT("yaw"), 0.f);
		if (P->HasField(TEXT("face")))
		{
			const FVector2D F = WRLand::Vec2(P->TryGetField(TEXT("face")));
			if (!F.Equals(At, 0.5f)) Yaw = FMath::RadiansToDegrees(FMath::Atan2(F.Y - At.Y, F.X - At.X));
		}
		return FTransform(FRotator(0.f, Yaw, 0.f), D.GroundPoint(At));
	}

	// ---------------------------------------------------------------- L2 the Throat
	void Throat(FSite& S)
	{
		// 90 m of tapering dolomite rings (r 11 m at the foot to 3.2 m at the mouth), the last 12 m bending toward the Great
		// Rift (local +X). A spiral stair winds inside to the singing gallery at 70 m (a ledge ring round the inside).
		Horn(S, FVector::ZeroVector, 1100.f, 380.f, 7800.f, 120.f, 28, true);
		for (int32 b = 0; b < 10; ++b)
		{
			// The bend: rings offset toward the rift, tilting.
			const float Z = 7800.f + b * 120.f;
			const float Off = FMath::Square(b / 10.f) * 900.f;
			const float R = FMath::Lerp(380.f, 520.f, b / 10.f);
			const int32 N = 14;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 2.f * PI * (i + (b % 2) * 0.5f) / N;
				S.K.Add(S.P.Dolo.Dim(0.88f + 0.15f * S.K.Rand(FVector(b, i, 0), 71)), FVector(110.f, 2.f * R * FMath::Sin(PI / N) - 4.f, 117.f),
					FVector(Off + FMath::Cos(A) * R, FMath::Sin(A) * R, Z + 60.f), FRotator(-b * 3.f, FMath::RadiansToDegrees(A), 0.f));
			}
		}
		// The doorway at the foot, voidglass-inlaid, facing away from the rift (the village side).
		S.K.Add(S.P.Glass, FVector(30.f, 260.f, 420.f), FVector(-1060.f, 0.f, 210.f));
		S.K.Add(S.P.GlassEdge, FVector(10.f, 270.f, 6.f), FVector(-1078.f, 0.f, 424.f));
		// The inner stair: a stone tread every 35 cm round the inside wall, up to the gallery.
		for (int32 i = 0; i < 200; ++i)
		{
			const float Z = i * 35.f;
			const float A = i * 0.16f;
			const float R = FMath::Lerp(1100.f, 380.f, FMath::Pow(Z / 7800.f, 0.8f)) * 0.78f;
			const FVector C(FMath::Cos(A) * R, FMath::Sin(A) * R, Z + 18.f);
			S.K.Add(S.P.DoloDark, FVector(180.f, 90.f, 30.f), C, FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
			S.Box(C, FVector(90.f, 45.f, 15.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
		// The singing gallery at 70 m: a ledge with stone music stands (S-16, the last rehearsal).
		const float GZ = 7000.f;
		const float GR = FMath::Lerp(1100.f, 380.f, FMath::Pow(GZ / 7800.f, 0.8f)) * 0.78f;
		S.Box(FVector(0, 0, GZ - 15.f), FVector(GR * 0.7f, GR * 0.7f, 15.f));
		S.K.Add(S.P.DoloDark, FVector(GR * 1.4f, GR * 1.4f, 30.f), FVector(0, 0, GZ - 15.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = PI * 0.5f + PI * i / 5.f;
			const FVector St(FMath::Cos(A) * GR * 0.5f, FMath::Sin(A) * GR * 0.5f, GZ);
			S.K.Add(S.P.Dolo, FVector(16.f, 16.f, 110.f), St + FVector(0, 0, 55.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Add(S.P.Ash, FVector(4.f, 50.f, 40.f), St + FVector(0, 0, 125.f), FRotator(20.f, FMath::RadiansToDegrees(A) + 180.f, 0.f));
		}
		S.Light(FVector(0, 0, GZ + 200.f), Lin(200, 190, 230), 2400.f, 1400.f);
		// Night: a faint violet in the mouth (rift light caught in the horn).
		S.Light(FVector(900.f, 0, 8900.f), Lin(123, 79, 214), 9000.f, 3000.f);
	}

	// ---------------------------------------------------------------- L4 the Hollow Colossus
	void Colossus(FSite& S)
	{
		// A 45 m singer of stacked dolomite drums: robe from a broad plinth, the chest carved into a walk-in hollow (an arch
		// 6 m high, a stair up inside to a door at the back onto the Great Shrine's terrace), arms down and open, head thrown
		// back mid-note, hair falling behind. She faces the Quiet Rift (local +X).
		const FSurf Stone = S.P.Dolo;
		S.K.Add(S.P.DoloDark, FVector(2400.f, 2400.f, 300.f), FVector(0, 0, 150.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Box(FVector(0, 0, 150.f), FVector(1100.f, 1100.f, 150.f));
		// The robe: drums narrowing to the waist, folds as vertical ribs.
		for (int32 i = 0; i < 14; ++i)
		{
			const float Z = 300.f + i * 120.f;
			const float D = FMath::Lerp(1900.f, 1150.f, i / 13.f);
			S.K.Add(Stone.Dim(0.9f + 0.1f * S.K.Rand(FVector(0, 0, Z), 81)), FVector(D, D * 0.85f, 118.f), FVector(0, 0, Z + 60.f), FRotator(0.f, i * 3.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 f = 0; f < 12; ++f)
			{
				const float A = 2.f * PI * f / 12.f;
				S.K.Add(S.P.DoloDark, FVector(40.f, 60.f, 118.f), FVector(FMath::Cos(A) * D * 0.5f, FMath::Sin(A) * D * 0.425f, Z + 60.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
		}
		S.Box(FVector(0, 0, 300.f + 840.f), FVector(600.f, 550.f, 840.f));
		// The torso: hollow. Walls of blocks round a 6 m-wide chamber, open at the front in a tall arch.
		const float T0 = 1980.f, T1 = 3200.f;
		for (float Z = T0; Z < T1; Z += 100.f)
		{
			const float R = FMath::Lerp(600.f, 520.f, (Z - T0) / (T1 - T0));
			for (int32 i = 0; i < 22; ++i)
			{
				const float A = 2.f * PI * i / 22.f;
				const bool bArch = FMath::Abs(FMath::Sin(A)) < 0.45f && FMath::Cos(A) > 0.f && Z < T0 + 800.f;
				if (bArch) continue;
				S.K.Add(Stone.Dim(0.85f + 0.15f * S.K.Rand(FVector(i, Z, 0), 83)), FVector(110.f, 2.f * R * FMath::Sin(PI / 22.f), 98.f), FVector(FMath::Cos(A) * R, FMath::Sin(A) * R * 0.85f, Z + 50.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
		}
		// The chamber's floor and the inner stair up to the back door (the shrine terrace is behind and below: a ramp out).
		S.K.Add(S.P.DoloDark, FVector(1000.f, 850.f, 40.f), FVector(0, 0, T0 - 20.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Box(FVector(0, 0, T0 - 20.f), FVector(480.f, 400.f, 20.f));
		for (int32 i = 0; i < 48; ++i)
		{
			// The front steps up from the plinth into the arch.
			const float X = 1150.f - i * 22.f;
			const float Z = 300.f + i * 35.f;
			if (Z > T0) break;
			S.K.Add(S.P.Dolo, FVector(26.f, 400.f, 36.f), FVector(X, 0, Z + 18.f));
			S.Box(FVector(X, 0, Z + 18.f), FVector(13.f, 200.f, 18.f));
		}
		for (const float Side : { -1.f, 1.f })
		{
			// Walls of the climb, so the stair runs in a cut through the robe.
			S.Box(FVector(900.f, Side * 230.f, 1100.f), FVector(400.f, 30.f, 900.f));
		}
		// Shoulders, one robe slipping off the left; the arms hang open, palms forward.
		S.K.Add(Stone, FVector(1400.f, 500.f, 260.f), FVector(0, 0, T1 + 130.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		for (const float Side : { -1.f, 1.f })
		{
			S.K.Rod(Stone, FVector(0.f, Side * 650.f, T1 + 120.f), FVector(150.f, Side * 820.f, T1 - 1300.f), 300.f);
			S.K.Rod(Stone, FVector(150.f, Side * 820.f, T1 - 1300.f), FVector(380.f, Side * 760.f, T1 - 2200.f), 240.f);
			S.K.Add(S.P.Dolo, FVector(120.f, 260.f, 340.f), FVector(420.f, Side * 760.f, T1 - 2400.f), FRotator(10.f, 0.f, 0.f));
		}
		S.K.Add(S.P.Dolo, FVector(500.f, 30.f, 400.f), FVector(-120.f, -560.f, T1 - 100.f), FRotator(0.f, 0.f, 18.f));   // the slipped robe's edge
		// The neck and the head thrown back, mouth open to the sky; hair falling down her back.
		S.K.Rod(Stone, FVector(0, 0, T1 + 240.f), FVector(-120.f, 0, T1 + 700.f), 360.f);
		const FVector Head(-260.f, 0.f, T1 + 1000.f);
		S.K.Add(Stone, FVector(560.f, 480.f, 640.f), Head, FRotator(-35.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		S.K.Add(S.P.Char, FVector(110.f, 160.f, 70.f), Head + FVector(190.f, 0.f, 220.f), FRotator(-35.f, 0.f, 0.f));
		for (int32 h = 0; h < 7; ++h)
		{
			S.K.Rod(S.P.DoloDark, Head + FVector(-180.f, -180.f + h * 60.f, 100.f), FVector(-700.f, -220.f + h * 70.f, T1 - 300.f), 70.f);
		}
		// Inside her chest, a lamp of the Choir's (the hollow reads from the Quire at night).
		S.Light(FVector(0, 0, T0 + 300.f), Lin(210, 196, 255), 6000.f, 1500.f);
		S.Light(FVector(1200.f, 0, 400.f), Lin(255, 160, 80), 2600.f, 1200.f);
	}

	// ---------------------------------------------------------------- L3 the Hanging Wood
	void HangingWood(FSite& S, FForest& Forest)
	{
		// Forty dead pines floating 20-40 m up over the rift, roots trailing, each root plate keeled with voidglass; some drift
		// a few metres on a 120 s cycle. A fallen trunk leans from the ground up to the lowest tree (the cache route).
		for (int32 i = 0; i < 40; ++i)
		{
			const float A = 2.f * PI * S.K.Rand(FVector(i, 0, 0), 91);
			const float Rr = 400.f + 3400.f * FMath::Sqrt(S.K.Rand(FVector(i, 1, 0), 93));
			const float Up = 2000.f + 2000.f * S.K.Rand(FVector(i, 2, 0), 95) * (1.f - Rr / 4200.f * 0.4f);
			USceneComponent* Holder = NewObject<USceneComponent>(S.Land);
			Holder->SetupAttachment(S.Root);
			const FVector Base(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Up);
			Holder->SetRelativeLocation(Base);
			Holder->RegisterComponent();
			if (S.bDraw)
			{
				TArray<FWRLookBlock> B;
				FKit K(Holder->GetComponentTransform(), B);
				// The root plate: a torn disc of earth and stone, roots trailing below, a black keel at its heart.
				K.Add(S.P.Ash, FVector(420.f, 380.f, 90.f), FVector(0, 0, -20.f), FRotator(0.f, 30.f * i, 0.f), EWRLookMesh::Cylinder);
				for (int32 r = 0; r < 7; ++r)
				{
					const float RA = 2.f * PI * r / 7.f;
					K.Rod(S.P.GreywoodDark, FVector(FMath::Cos(RA) * 150.f, FMath::Sin(RA) * 150.f, -40.f), FVector(FMath::Cos(RA) * 230.f, FMath::Sin(RA) * 230.f, -320.f - 80.f * K.Rand(FVector(r, i, 0), 97)), 14.f);
				}
				K.Spike(S.P.Glass, FVector(0, 0, -50.f), FVector(0, 0, -1.f), 120.f, 380.f);
				K.Add(S.P.GlassEdge, FVector(80.f, 80.f, 3.f), FVector(0, 0, -150.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				FWRBuildLook::Spawn(S.Land, Holder, B, FLinearColor::Gray, false);
			}
			// The tree on its plate (the pack's dead tree, as a plain component so it rides the mover).
			if (Forest.Trees.Num())
			{
				UStaticMeshComponent* T = NewObject<UStaticMeshComponent>(S.Land);
				UStaticMesh* M = Forest.Trees[i % Forest.Trees.Num()];
				T->SetStaticMesh(M);
				T->SetupAttachment(Holder);
				const float H = 1400.f + 700.f * S.K.Rand(FVector(i, 3, 0), 99);
				T->SetRelativeScale3D(FVector(H / FMath::Max(1.f, M->GetBoundingBox().GetSize().Z)));
				T->SetRelativeLocation(FVector(0, 0, 10.f));
				T->SetRelativeRotation(FRotator(6.f * S.K.Jit(FVector(i, 4, 0), 3), 360.f * S.K.Rand(FVector(i, 5, 0), 5), 0.f));
				if (Forest.Bark) for (int32 m = 0; m < T->GetNumMaterials(); ++m) T->SetMaterial(m, Forest.Bark);
				T->SetCollisionProfileName(i < 6 ? TEXT("BlockAll") : TEXT("NoCollision"));
				T->RegisterComponent();
			}
			if (i < 6) WRFoldLook::Block(S.Land, Holder, FVector(0, 0, -20.f), FVector(200.f, 200.f, 45.f));
			FWRLandMover Mv;
			Mv.Comp = Holder;
			Mv.Base = Base;
			Mv.Bob = 30.f + 20.f * S.K.Rand(FVector(i, 6, 0), 7);
			Mv.BobPeriod = 14.f + 6.f * S.K.Rand(FVector(i, 7, 0), 9);
			Mv.Phase = S.K.Rand(FVector(i, 8, 0), 11);
			// A few drift a few metres on the slow cycle.
			if (i % 5 == 0) { Mv.Sway = FVector(FMath::Cos(A + 1.f), FMath::Sin(A + 1.f), 0.f) * 350.f; Mv.SwayPeriod = 120.f; }
			S.Land->AddMover(Mv);
		}
		// The fallen trunk ramp, from the ground to the lowest plate's height (the lowest tree is i = 0's ring).
		S.K.Rod(S.P.GreywoodDark, S.G(-2600.f, -900.f, 40.f), FVector(-700.f, -300.f, 1900.f), 90.f);
		const FVector A = S.G(-2600.f, -900.f, 40.f), B(-700.f, -300.f, 1900.f);
		const FVector Mid = (A + B) * 0.5f;
		S.Box(Mid, FVector((B - A).Size() * 0.5f, 45.f, 40.f), (B - A).Rotation());
		S.Light(FVector(0, 0, 400.f), Lin(123, 79, 214), 5000.f, 3000.f);
	}

	// ---------------------------------------------------------------- L5 the Hollow Fort
	void HollowFort(FSite& S)
	{
		// Two curved horn-walls opening east into the gorge, arrow slits aimed into it, a gate between them toward the west.
		for (const float Side : { -1.f, 1.f })
		{
			for (int32 i = 0; i < 9; ++i)
			{
				const float A0 = FMath::DegreesToRadians(Side * (20.f + i * 12.f)), A1 = FMath::DegreesToRadians(Side * (20.f + (i + 1) * 12.f));
				const FVector2D P0(FMath::Cos(A0) * 2200.f - 600.f, FMath::Sin(A0) * 1400.f), P1(FMath::Cos(A1) * 2200.f - 600.f, FMath::Sin(A1) * 1400.f);
				Wall(S, P0, P1, 900.f - i * 40.f, 220.f, 0.12f);
				// Slits: dark recesses every segment, aimed east.
				const FVector2D M = (P0 + P1) * 0.5f;
				S.K.Add(S.P.Char, FVector(14.f, 30.f, 130.f), S.G(M.X + 120.f, M.Y, 420.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(P1.Y - P0.Y, P1.X - P0.X)), 0.f));
			}
			// The horn's flared lip at the gorge end.
			Horn(S, S.G(1500.f, Side * 700.f), 360.f, 520.f, 1300.f, 110.f, 12, true);
		}
		// The gate's two towers on the west.
		for (const float Side : { -1.f, 1.f }) Column(S, S.G(-2200.f, Side * 420.f), 1100.f, 380.f, false);
		S.K.Add(S.P.Dolo, FVector(300.f, 1220.f, 140.f), S.G(-2200.f, 0.f, 1160.f));
		S.Box(S.G(-2200.f, 0.f, 1160.f), FVector(150.f, 610.f, 70.f));
		Brazier(S, S.G(-1900.f, 260.f));
		S.Light(S.G(-1600.f, 0.f, 600.f), Lin(255, 160, 80), 4000.f, 2200.f);
	}

	// ---------------------------------------------------------------- L6 the Unravelling Viaduct
	void Viaduct(FSite& S, UMaterialInterface* RockMat)
	{
		// Five spans of 22 m across the ravine (local Y runs along the ravine; the viaduct runs along local X, west to
		// east). The middle two are broken: their stones hang 2-8 m below the gap, turning slowly on keels. Their gaps
		// (1.5-2.5 m) are the jump route across.
		const float Span = 2200.f, Deck = 0.f;
		const float DeckZ = FMath::Max(S.Gz(-5500.f, 0.f), S.Gz(5500.f, 0.f)) + 200.f;
		for (int32 p = 0; p <= 5; ++p)
		{
			const float X = -5500.f + p * Span;
			const float Foot = S.Gz(X, 0.f) - 200.f;
			// Piers: battered dolomite, from the ravine floor to the deck.
			for (float Z = Foot; Z < DeckZ - 120.f; Z += 100.f)
			{
				S.K.Add(S.P.Dolo.Dim(0.85f + 0.15f * S.K.Rand(FVector(p, Z, 0), 101)), FVector(420.f + (DeckZ - Z) * 0.04f, 560.f, 98.f), FVector(X, 0.f, Z + 50.f));
			}
			S.Box(FVector(X, 0, (Foot + DeckZ) * 0.5f), FVector(220.f, 280.f, (DeckZ - Foot) * 0.5f));
		}
		for (int32 sp = 0; sp < 5; ++sp)
		{
			const float X0 = -5500.f + sp * Span, X1 = X0 + Span;
			const bool bBroken = sp == 2 || sp == 3;
			// The arch: voussoirs, then the deck.
			if (!bBroken)
			{
				for (int32 v = 0; v < 14; ++v)
				{
					const float A = PI * (v + 0.5f) / 14.f;
					const FVector C((X0 + X1) * 0.5f - FMath::Cos(A) * Span * 0.45f, 0.f, DeckZ - 120.f - Span * 0.3f + FMath::Sin(A) * Span * 0.3f);
					S.K.Add(S.P.Dolo, FVector(170.f, 500.f, 80.f), C, FRotator(FMath::RadiansToDegrees(A) - 90.f, 0.f, 0.f));
				}
				S.K.Add(S.P.DoloDark, FVector(Span, 520.f, 60.f), FVector((X0 + X1) * 0.5f, 0.f, DeckZ - 30.f));
				S.Box(FVector((X0 + X1) * 0.5f, 0.f, DeckZ - 30.f), FVector(Span * 0.5f, 260.f, 30.f));
				for (const float Side : { -1.f, 1.f }) S.K.Add(S.P.Dolo, FVector(Span, 40.f, 90.f), FVector((X0 + X1) * 0.5f, Side * 240.f, DeckZ + 45.f));
				continue;
			}
			// Broken: stones frozen mid-fall in a stepping line across the gap, each on its keel, slowly turning.
			for (int32 k = 0; k < 6; ++k)
			{
				const float X = X0 + 260.f + k * 340.f;
				const float Drop = 200.f + 600.f * FMath::Abs(FMath::Sin(k * 1.3f + sp));
				KeeledStone(S, k + sp, FVector(260.f, 240.f, 110.f), FVector(X, 30.f * S.K.Jit(FVector(k, sp, 0), 3), DeckZ - Drop), 20.f * k, RockMat, 18.f, 11.f + k, 0.13f * k, true, k % 2 ? 1.2f : -0.9f);
			}
		}
	}

	// ---------------------------------------------------------------- L7 the Ash-cast cage
	void AshCage(FSite& S)
	{
		// Eight crystal bars round a grey ash cast of the Cantor: tall, long-armed, mouth open.
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2.f * PI * i / 8.f;
			S.K.Hex(S.P.Crystal, FMath::Cos(A) * 420.f, FMath::Sin(A) * 420.f, 0.f, 900.f, 36.f, 30.f * i);
			// Hexafoil carving on each bar: a pale lit ring.
			S.K.Add(S.P.Crystal.Dim(1.4f), FVector(90.f, 90.f, 8.f), FVector(FMath::Cos(A) * 420.f, FMath::Sin(A) * 420.f, 460.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.Box(FVector(FMath::Cos(A) * 420.f, FMath::Sin(A) * 420.f, 450.f), FVector(36.f, 36.f, 450.f));
		}
		S.K.Add(S.P.Crystal, FVector(980.f, 980.f, 40.f), FVector(0, 0, 920.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		const FSurf AshS = S.P.Ash;
		S.K.Rod(AshS, FVector(0, 0, 0.f), FVector(0, 0, 380.f), 110.f);         // legs as one robed column
		S.K.Add(AshS, FVector(120.f, 150.f, 180.f), FVector(0, 0, 470.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		S.K.Add(AshS, FVector(70.f, 60.f, 90.f), FVector(10.f, 0, 600.f), FRotator(-25.f, 0.f, 0.f), EWRLookMesh::Cylinder);
		S.K.Add(S.P.Char, FVector(30.f, 34.f, 40.f), FVector(42.f, 0, 610.f), FRotator(-25.f, 0.f, 0.f));   // the open mouth
		for (const float Side : { -1.f, 1.f })
		{
			S.K.Rod(AshS, FVector(0, Side * 80.f, 540.f), FVector(60.f, Side * 160.f, 250.f), 34.f);
			S.K.Rod(AshS, FVector(60.f, Side * 160.f, 250.f), FVector(90.f, Side * 150.f, 40.f), 28.f);   // the long arms, to the knees
		}
		S.Box(FVector(0, 0, 340.f), FVector(80.f, 120.f, 340.f));
		S.Light(FVector(0, 0, 980.f), Lin(214, 206, 236), 3000.f, 1400.f);
	}

	// ---------------------------------------------------------------- L8 the Broken Knee
	void BrokenKnee(FSite& S)
	{
		// A 60 m ridge of white giant bone: the femur's end, condyles to the west, the shaft running east and sheared flat
		// at its east face (local +X) with the voidglass sheen of a rift ring. A dark crack in the cut face (S-17).
		for (int32 i = 0; i < 12; ++i)
		{
			const float X = -2400.f + i * 300.f;
			const float D = FMath::Lerp(2200.f, 1300.f, FMath::Clamp((X + 2400.f) / 1800.f, 0.f, 1.f));
			S.K.Add(S.P.Bone.Dim(0.9f + 0.12f * S.K.Rand(FVector(i, 0, 0), 111)), FVector(310.f, D, D * 0.8f), FVector(X, 0.f, S.Gz(X, 0.f) + D * 0.22f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
		}
		// The condyles: two great knuckles at the west end.
		for (const float Side : { -1.f, 1.f })
		{
			S.K.Add(S.P.Bone, FVector(1500.f, 1300.f, 1300.f), FVector(-2700.f, Side * 700.f, S.Gz(-2700.f, Side * 700.f) + 350.f), FRotator(0.f, 0.f, 90.f), EWRLookMesh::Cylinder);
		}
		// The cut face: flat black glass with a violet edge.
		S.K.Add(S.P.Glass, FVector(30.f, 1300.f, 1050.f), FVector(1240.f, 0.f, S.Gz(1240.f, 0.f) + 300.f));
		S.K.Add(S.P.GlassEdge, FVector(10.f, 1310.f, 8.f), FVector(1258.f, 0.f, S.Gz(1240.f, 0.f) + 826.f));
		S.K.Add(S.P.Char, FVector(40.f, 110.f, 380.f), FVector(1250.f, 180.f, S.Gz(1240.f, 0.f) + 190.f));   // the marrow crack
		S.Box(FVector(-600.f, 0, S.Gz(-600.f, 0.f) + 250.f), FVector(1900.f, 600.f, 500.f));
		S.Box(FVector(-2700.f, 0, S.Gz(-2700.f, 0.f) + 300.f), FVector(700.f, 1300.f, 600.f));
	}

	// ---------------------------------------------------------------- L9 the Glass Sea
	void GlassSea(FSite& S)
	{
		// The ground is already black glass (the land's colour); on it: fused cracks catching violet, and footprints fused
		// in the glass leading toward the collapsed rift's centre and none leading back (S-20).
		for (int32 i = 0; i < 26; ++i)
		{
			const float A = 2.f * PI * i / 26.f + 0.2f * S.K.Jit(FVector(i, 0, 0), 3);
			const float R0 = 1500.f + 4000.f * S.K.Rand(FVector(i, 1, 0), 5), R1 = R0 + 1800.f + 3000.f * S.K.Rand(FVector(i, 2, 0), 7);
			S.K.Add(S.P.GlassEdge.Dim(0.5f), FVector(R1 - R0, 8.f, 2.f), S.G(FMath::Cos(A) * (R0 + R1) * 0.5f, FMath::Sin(A) * (R0 + R1) * 0.5f, 1.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
		}
		for (int32 i = 0; i < 30; ++i)
		{
			const float X = -9000.f + i * 280.f;
			const float Y = (i % 2 ? 18.f : -18.f) + 400.f * FMath::Sin(i * 0.15f);
			S.K.Add(S.P.Char, FVector(30.f, 12.f, 1.f), S.G(X, Y, 0.6f), FRotator(0.f, 4.f * FMath::Cos(i * 0.15f), 0.f));
		}
		S.Light(S.G(0.f, 0.f, 300.f), Lin(123, 79, 214), 4000.f, 6000.f);
	}

	// ---------------------------------------------------------------- L10 the Black Beacon
	void BlackBeacon(FSite& S, UMaterialInterface* LightMat)
	{
		// A Choir watch-tower on the cape crowned by a 6 m polished voidglass lens that gathers the rifts' glow and throws a
		// violet cone 300 m out to sea at night. The keeper's room is its ground floor (S-10).
		Horn(S, FVector::ZeroVector, 520.f, 380.f, 2200.f, 100.f, 18, true);
		S.K.Add(S.P.DoloDark, FVector(900.f, 900.f, 80.f), FVector(0, 0, 2240.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = 2.f * PI * i / 6.f;
			S.K.Rod(S.P.Iron, FVector(FMath::Cos(A) * 380.f, FMath::Sin(A) * 380.f, 2280.f), FVector(FMath::Cos(A) * 300.f, FMath::Sin(A) * 300.f, 2600.f), 18.f);
		}
		// The lens: a thick disc of black glass on its iron cradle, tipped toward the sea (local +X).
		S.K.Disc(S.P.Glass, FVector(40.f, 0, 2900.f), FVector(1.f, 0.f, 0.15f), 600.f, 90.f);
		S.K.Disc(S.P.GlassEdge, FVector(86.f, 0, 2907.f), FVector(1.f, 0.f, 0.15f), 610.f, 6.f);
		// The keeper's room at the foot: a cot, a lens shard on its stand, ship marks scratched on the wall.
		S.K.Add(S.P.Greywood, FVector(190.f, 80.f, 40.f), FVector(-200.f, 200.f, 20.f));
		S.K.Add(S.P.Sail, FVector(180.f, 70.f, 8.f), FVector(-200.f, 200.f, 44.f));
		S.K.Add(S.P.Iron, FVector(10.f, 10.f, 110.f), FVector(150.f, -150.f, 55.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.K.Disc(S.P.Glass, FVector(150.f, -150.f, 130.f), FVector(1.f, 0.f, 0.f), 60.f, 10.f);
		for (int32 m = 0; m < 17; ++m) S.K.Add(S.P.Char, FVector(3.f, 2.f, m == 16 ? 14.f : 30.f), FVector(-440.f, -150.f + m * 14.f, 160.f), FRotator(0.f, 0.f, m % 5 == 4 ? 60.f : 0.f));
		S.Light(FVector(0, 0, 120.f), Lin(255, 180, 110), 1800.f, 700.f);
		if (S.bDraw && LightMat)
		{
			// The cone out to sea (additive light), and the lens's own glow.
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(LightMat, S.Land);
			M->SetVectorParameterValue(TEXT("Color"), Lin(123, 79, 214));
			M->SetScalarParameterValue(TEXT("Glow"), 0.35f);
			M->SetScalarParameterValue(TEXT("Edge"), 1.4f);
			WRFoldLook::Shape(S.Land, S.Root, TEXT("Cone"), FVector(6000.f, 6000.f, 30000.f), FVector(15000.f, 0.f, 2900.f), FRotator(-90.f, 0.f, 0.f), M);
			USpotLightComponent* Spot = NewObject<USpotLightComponent>(S.Land);
			Spot->SetupAttachment(S.Root);
			Spot->SetRelativeLocation(FVector(120.f, 0, 2900.f));
			Spot->SetRelativeRotation(FRotator(-2.f, 0.f, 0.f));
			Spot->SetLightColor(Lin(123, 79, 214));
			Spot->SetIntensity(400000.f);
			Spot->SetAttenuationRadius(30000.f);
			Spot->SetOuterConeAngle(9.f);
			Spot->SetInnerConeAngle(4.f);
			Spot->SetCastShadows(false);
			Spot->RegisterComponent();
		}
	}

	// ---------------------------------------------------------------- L1 the Great Rift (its skyline: the column)
	void GreatRiftColumn(FSite& S, UMaterialInterface* LightMat)
	{
		if (!S.bDraw || !LightMat) return;
		// A 400 m pillar of bent light over the hole: a soft column and three slowly turning lensing rings round it (the
		// Einstein ring). Additive, so it reads by day as the sky swimming and by night as a violet-white shaft.
		const FWRLandFeature* Hole = S.D->Feature(TEXT("great_rift"));
		const float R = Hole ? Hole->R * 100.f : 16000.f;
		UMaterialInstanceDynamic* Col = UMaterialInstanceDynamic::Create(LightMat, S.Land);
		Col->SetVectorParameterValue(TEXT("Color"), Lin(200, 190, 255));
		Col->SetScalarParameterValue(TEXT("Glow"), 0.22f);
		Col->SetScalarParameterValue(TEXT("Edge"), 3.f);
		WRFoldLook::Shape(S.Land, S.Root, TEXT("Cylinder"), FVector(R * 0.7f, R * 0.7f, 40000.f), FVector(0, 0, 20000.f), FRotator::ZeroRotator, Col);
		for (int32 i = 0; i < 3; ++i)
		{
			UMaterialInstanceDynamic* Ring = UMaterialInstanceDynamic::Create(LightMat, S.Land);
			Ring->SetVectorParameterValue(TEXT("Color"), Lin(150, 110, 240));
			Ring->SetScalarParameterValue(TEXT("Glow"), 0.5f);
			Ring->SetScalarParameterValue(TEXT("Edge"), 1.2f);
			USceneComponent* Holder = NewObject<USceneComponent>(S.Land);
			Holder->SetupAttachment(S.Root);
			Holder->RegisterComponent();
			WRFoldLook::Shape(S.Land, Holder, TEXT("Cylinder"), FVector(R * (1.1f + 0.25f * i), R * (1.1f + 0.25f * i), 120.f), FVector::ZeroVector, FRotator::ZeroRotator, Ring);
			FWRLandMover M;
			M.Comp = Holder;
			M.Base = FVector(0, 0, 9000.f + i * 11000.f);
			M.Rot = FRotator(6.f + 4.f * i, 0.f, 0.f);
			M.Spin = 2.f + i;
			M.Bob = 600.f;
			M.BobPeriod = 40.f + 10.f * i;
			S.Land->AddMover(M);
		}
		// The rim's glass ring catches it: a violet light low over the lip on four sides.
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = PI * 0.5f * i + PI * 0.25f;
			S.Light(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 300.f), Lin(123, 79, 214), 9000.f, 5000.f);
		}
	}

	// ---------------------------------------------------------------- camps
	void Camp(FSite& S, const FString& Kind)
	{
		if (Kind == TEXT("camp_wreck"))
		{
			// Driftwrack: the Grey Gull, a two-master on her side, her hold a shelter; a salt pan and a drying rack.
			Hull(S, FVector(-900.f, 0.f, 0.f), 90.f, 2600.f, 700.f, -58.f, false);
			S.K.Rod(S.P.Greywood, FVector(-200.f, -400.f, 0.f), FVector(-200.f, -400.f, 220.f), 10.f);
			S.K.Rod(S.P.Greywood, FVector(-200.f, 400.f, 0.f), FVector(-200.f, 400.f, 220.f), 10.f);
			S.K.Rod(S.P.Greywood, FVector(-200.f, -400.f, 210.f), FVector(-200.f, 400.f, 210.f), 8.f);
			for (int32 i = 0; i < 5; ++i) S.K.Add(S.P.Leather, FVector(6.f, 60.f, 90.f), FVector(-200.f, -300.f + i * 150.f, 160.f), FRotator(0.f, 0.f, 3.f * i));
		}
		else if (Kind == TEXT("camp_hull"))
		{
			// Saltglass: a hull broken in two on the black sand; a hearth built of Choir blocks.
			Hull(S, FVector(-900.f, -500.f, 0.f), 70.f, 2200.f, 640.f, -30.f, true);
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = 2.f * PI * i / 10.f;
				S.K.Add(S.P.Dolo, FVector(60.f, 40.f, 40.f), FVector(200.f + FMath::Cos(A) * 80.f, FMath::Sin(A) * 80.f, 20.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
		}
		else
		{
			// Lantern Cove: a beached longboat under the Beacon's light; fish racks. (The Grey Crossing's landing.)
			Hull(S, FVector(400.f, 0.f, 0.f), 0.f, 900.f, 240.f, -8.f, false);
			for (int32 r = 0; r < 2; ++r)
			{
				const float Y = -500.f - r * 260.f;
				S.K.Rod(S.P.Greywood, FVector(-300.f, Y, 0.f), FVector(-300.f, Y, 200.f), 9.f);
				S.K.Rod(S.P.Greywood, FVector(100.f, Y, 0.f), FVector(100.f, Y, 200.f), 9.f);
				S.K.Rod(S.P.Greywood, FVector(-300.f, Y, 190.f), FVector(100.f, Y, 190.f), 7.f);
				for (int32 f = 0; f < 5; ++f) S.K.Add(S.P.Ash.Dim(1.3f), FVector(8.f, 18.f, 60.f), FVector(-250.f + f * 80.f, Y, 150.f));
			}
		}
		// Every camp: a lit fire ring (a light to sleep by), a bedroll and a crate.
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2.f * PI * i / 8.f;
			S.K.Add(S.P.DoloDark, FVector(24.f, 20.f, 16.f), FVector(300.f + FMath::Cos(A) * 60.f, 300.f + FMath::Sin(A) * 60.f, 8.f), FRotator(0.f, 45.f * i, 0.f));
		}
		S.K.Add(S.P.Char, FVector(90.f, 90.f, 4.f), FVector(300.f, 300.f, 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		S.Fire(FVector(300.f, 300.f, 10.f), 1.f);
		S.K.Add(S.P.Sail.Dim(0.8f), FVector(190.f, 80.f, 6.f), FVector(80.f, 450.f, 3.f), FRotator(0.f, 20.f, 0.f));
		S.K.Add(S.P.Greywood, FVector(80.f, 60.f, 60.f), FVector(500.f, 520.f, 30.f), FRotator(0.f, 12.f, 0.f));
	}

	// ---------------------------------------------------------------- story sites
	void Story(FSite& S, const FString& Kind, const TSharedPtr<FJsonObject>& Row, FForest& Forest, UMaterialInterface* RockMat)
	{
		if (Kind == TEXT("graves_row"))
		{
			// A Lantern Mourner row (the Graveyard's grey stones, a stone lantern) that stops: the last three open and empty, the
			// soil streaked south (local +X) toward the void.
			for (int32 i = 0; i < 12; ++i)
			{
				const FVector At = S.G(-300.f, -1100.f + i * 200.f);
				const bool bOpen = i >= 9;
				if (!bOpen) S.K.Add(S.P.DoloDark, FVector(26.f, 80.f, 110.f), At + FVector(-60.f, 0.f, 55.f), FRotator(0.f, 0.f, 3.f * S.K.Jit(At, 3)));
				S.K.Add(bOpen ? S.P.Char : S.P.Ash, FVector(180.f, 80.f, bOpen ? 6.f : 18.f), At + FVector(40.f, 0.f, bOpen ? -2.f : 9.f));
				if (bOpen) for (int32 k = 0; k < 4; ++k) S.K.Add(S.P.Ash, FVector(160.f, 10.f, 4.f), At + FVector(220.f + k * 30.f, -30.f + k * 20.f, 1.f), FRotator(0.f, 5.f * k, 0.f));
			}
			// The tōrō lantern at the row's head, its flame leaning south.
			const FVector L = S.G(-300.f, -1350.f);
			S.K.Add(S.P.DoloDark, FVector(50.f, 50.f, 90.f), L + FVector(0, 0, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.K.Add(S.P.Dolo, FVector(70.f, 70.f, 50.f), L + FVector(0, 0, 115.f));
			S.K.Add(S.P.Dolo, FVector(110.f, 110.f, 30.f), L + FVector(0, 0, 155.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			S.K.Add(S.P.Fire, FVector(10.f, 10.f, 22.f), L + FVector(5.f, 0, 120.f), FRotator(-14.f, 0.f, 0.f), EWRLookMesh::Cone);
			S.Light(L + FVector(0, 0, 120.f), Lin(255, 190, 120), 1500.f, 700.f);
		}
		else if (Kind == TEXT("first_hut"))
		{
			// Maren's first hut: a lean-to, a cold fire, a bedroll, a ring of thrown stones round a small rift.
			LeanTo(S, S.G(-400.f, -300.f), 30.f);
			for (int32 i = 0; i < 14; ++i)
			{
				const float A = 2.f * PI * i / 14.f;
				const FVector At(FMath::Cos(A) * 900.f + 1000.f, FMath::Sin(A) * 900.f, 0.f);
				S.K.Add(S.P.DoloDark, FVector(18.f, 16.f, 12.f), S.G(At.X, At.Y, 14.f), FRotator(0.f, 30.f * i, 10.f));   // floating a hand's width up
			}
		}
		else if (Kind == TEXT("burned_stand"))
		{
			// Pines burned to black spikes (the land's ground is char here); a melted bronze lantern at its upwind edge.
			const float R = Num(Row, TEXT("r"), 100.f) * 100.f;
			for (int32 i = 0; i < 60; ++i)
			{
				const float A = 2.f * PI * S.K.Rand(FVector(i, 0, 0), 121), Rr = R * FMath::Sqrt(S.K.Rand(FVector(i, 1, 0), 123));
				const FVector2D P(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr);
				Forest.Add(S.Land, S.Root->GetComponentTransform().TransformPosition(S.G(P.X, P.Y)), 900.f + 500.f * S.K.Rand(FVector(i, 2, 0), 125), 360.f * S.K.Rand(FVector(i, 3, 0), 127), 4.f, 0.f, i, true);
			}
			S.K.Add(S.P.Bronze.Dim(0.6f), FVector(40.f, 50.f, 26.f), S.G(-R - 300.f, 0.f, 12.f), FRotator(10.f, 30.f, 70.f), EWRLookMesh::Cylinder);
		}
		else if (Kind == TEXT("goat_shelter"))
		{
			// A Choir shepherd's hut beside a pen of Withergoat bones, the gate open; the taming tip carved on the gatepost.
			Wall(S, FVector2D(-400.f, -300.f), FVector2D(-400.f, 300.f), 240.f, 60.f, 0.1f);
			Wall(S, FVector2D(-400.f, 300.f), FVector2D(0.f, 300.f), 240.f, 60.f, 0.1f);
			Wall(S, FVector2D(-400.f, -300.f), FVector2D(0.f, -300.f), 240.f, 60.f, 0.1f);
			for (int32 i = 0; i < 16; ++i)
			{
				const float A = 2.f * PI * i / 16.f;
				if (i == 0 || i == 1) continue;   // the gate, open
				S.K.Rod(S.P.Greywood, S.G(700.f + FMath::Cos(A) * 500.f, FMath::Sin(A) * 500.f), S.G(700.f + FMath::Cos(A) * 500.f, FMath::Sin(A) * 500.f, 130.f), 10.f);
			}
			S.K.Rod(S.P.Greywood, S.G(1200.f, 0.f), S.G(1200.f, 0.f, 160.f), 16.f);   // the gatepost
			S.K.Add(S.P.Char, FVector(4.f, 30.f, 30.f), S.G(1208.f, 0.f, 120.f));     // the carved goat eating grain
			for (int32 b = 0; b < 9; ++b) S.K.Add(S.P.Bone, FVector(50.f, 12.f, 10.f), S.G(700.f + 200.f * S.K.Jit(FVector(b, 0, 0), 1), 200.f * S.K.Jit(FVector(b, 1, 0), 2), 5.f), FRotator(0.f, 40.f * b, 0.f));
		}
		else if (Kind == TEXT("cairn_field"))
		{
			// Thirty point-down cairns over a small rift, their top stones risen and hanging above them.
			for (int32 i = 0; i < 30; ++i)
			{
				const float A = 2.f * PI * S.K.Rand(FVector(i, 0, 0), 131), Rr = 600.f + 2400.f * FMath::Sqrt(S.K.Rand(FVector(i, 1, 0), 133));
				Cairn(S, S.G(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr), 110.f + 50.f * S.K.Rand(FVector(i, 2, 0), 135), 20.f * i, true);
			}
		}
		else if (Kind == TEXT("salt_rest"))
		{
			// Choir salt pans: rakes leaning on a cone of grey salt, three seated dead facing the sea (not the rifts).
			for (int32 p = 0; p < 3; ++p)
			{
				const FVector At = S.G(-600.f + p * 500.f, -700.f);
				S.K.Add(S.P.DoloDark, FVector(420.f, 300.f, 16.f), At + FVector(0, 0, 8.f));
				S.K.Add(S.P.Salt, FVector(380.f, 260.f, 4.f), At + FVector(0, 0, 17.f));
			}
			const FVector Cone = S.G(-300.f, 200.f);
			S.K.Add(S.P.Salt.Dim(0.85f), FVector(260.f, 260.f, 180.f), Cone + FVector(0, 0, 90.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
			for (int32 r = 0; r < 3; ++r) S.K.Rod(S.P.Greywood, Cone + FVector(120.f, -60.f + r * 60.f, 0.f), Cone + FVector(20.f, -40.f + r * 40.f, 170.f), 6.f);
			for (int32 d = 0; d < 3; ++d) Seated(S, S.G(400.f, -200.f + d * 160.f), 0.f, 1.f, 0.f);
		}
		else if (Kind == TEXT("hanging_cache"))
		{
			// A satchel caught in the highest floating pine's crown: hung on its strap from a branch over the rift.
			Satchel(S, FVector(200.f, 100.f, 4100.f), 30.f);
		}
		else if (Kind == TEXT("drowned_procession"))
		{
			// A row of cairns marching straight into the sea and on under the surf.
			for (int32 i = 0; i < 16; ++i) Cairn(S, S.G(-1600.f + i * 300.f, 0.f), 120.f, 15.f * i);
			Satchel(S, S.G(3200.f, 0.f), 0.f);
		}
		else if (Kind == TEXT("seated_listeners"))
		{
			// Twelve seated in a curve facing the Quiet Rift, hands open in their laps: the Great Shrine's approach.
			SeatedArc(S, FVector2D(0.f, 0.f), 1300.f, 110.f, 12, 0.f);
		}
		else if (Kind == TEXT("alms_court"))
		{
			// Hundreds of bowls stacked against a wall, every one licked clean; great claw marks above.
			Wall(S, FVector2D(600.f, -1400.f), FVector2D(600.f, 1400.f), 600.f, 120.f, 0.05f);
			for (int32 c = 0; c < 26; ++c) Bowls(S, S.G(440.f, -1300.f + c * 100.f), 4 + (c * 7) % 6, 10.f * c);
			for (int32 m = 0; m < 4; ++m) S.K.Add(S.P.Char, FVector(4.f, 18.f, 260.f), S.G(538.f, -300.f + m * 70.f, 380.f), FRotator(0.f, 0.f, 12.f));
			Wall(S, FVector2D(-1000.f, -1400.f), FVector2D(600.f, -1400.f), 400.f, 100.f, 0.4f);
			Wall(S, FVector2D(-1000.f, 1400.f), FVector2D(600.f, 1400.f), 400.f, 100.f, 0.4f);
		}
		else if (Kind == TEXT("tuning_hall"))
		{
			// A hall of hung bronze bars of graded length; some bent inward toward one point on the floor (X-1: strike low to
			// high and the door in the back wall opens: WRLandSecrets).
			Wall(S, FVector2D(-1200.f, -700.f), FVector2D(1200.f, -700.f), 700.f, 100.f, 0.15f);
			Wall(S, FVector2D(-1200.f, 700.f), FVector2D(1200.f, 700.f), 700.f, 100.f, 0.15f);
			Wall(S, FVector2D(1200.f, -700.f), FVector2D(1200.f, 700.f), 700.f, 100.f, 0.f);
			S.K.Rod(S.P.Iron, FVector(-900.f, -600.f, 600.f), FVector(900.f, -600.f, 600.f), 12.f);
			S.K.Rod(S.P.Iron, FVector(-900.f, 600.f, 600.f), FVector(900.f, 600.f, 600.f), 12.f);
			for (int32 b = 0; b < 12; ++b)
			{
				const float X = -800.f + b * 145.f;
				const float L = 120.f + b * 32.f;
				const bool bBent = b % 3 == 1;
				const FVector Top(X, 0.f, 600.f);
				S.K.Rod(S.P.Iron, FVector(X, -600.f, 600.f), FVector(X, 600.f, 600.f), 6.f);
				S.K.Add(S.P.Bronze, FVector(16.f, 16.f, L), Top - FVector(0, 0, L * 0.5f), FRotator(0.f, 0.f, bBent ? 14.f : 0.f), EWRLookMesh::Cylinder);
			}
			S.K.Add(S.P.Glass, FVector(30.f, 30.f, 2.f), FVector(0.f, 160.f, 1.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // the point they lean toward
		}
		else if (Kind == TEXT("choir_benches"))
		{
			// Small seats in an amphitheatre and a dropped wooden whistle.
			for (int32 r = 0; r < 5; ++r)
			{
				const float R = 600.f + r * 160.f;
				for (int32 i = 0; i < 9 + r * 2; ++i)
				{
					const float A = FMath::DegreesToRadians(180.f - 60.f + 120.f * i / (8 + r * 2));
					const FVector At = S.G(FMath::Cos(A) * R, FMath::Sin(A) * R, r * 40.f);
					S.K.Add(S.P.Dolo, FVector(46.f, 70.f, 40.f), At + FVector(0, 0, 20.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
				}
			}
			S.K.Add(S.P.Greywood, FVector(18.f, 3.f, 3.f), S.G(-200.f, 60.f, 2.f), FRotator(0.f, 30.f, 0.f), EWRLookMesh::Cylinder);
		}
		else if (Kind == TEXT("marrow_cache"))
		{
			// The crack in the Knee's cut face leads into the hollow marrow channel: a satchel at its end.
			Satchel(S, S.G(-300.f, 150.f), 90.f);
		}
		else if (Kind == TEXT("offering_row"))
		{
			// Seated dead in a row at the rift's lip, the front three half drawn into the tear.
			SeatedArc(S, FVector2D(0.f, 0.f), 1100.f, 70.f, 9, 0.85f);
		}
		else if (Kind == TEXT("surveyors"))
		{
			// A newcomer survey camp: tripod, string lines, chalk marks; three bodies in the line of a rift's pull, dragged
			// until they stopped against a cairn.
			const FVector T = S.G(-800.f, -400.f);
			for (int32 l = 0; l < 3; ++l)
			{
				const float A = 2.f * PI * l / 3.f;
				S.K.Rod(S.P.Greywood, T + FVector(FMath::Cos(A) * 50.f, FMath::Sin(A) * 50.f, 0.f), T + FVector(0, 0, 150.f), 5.f);
			}
			S.K.Add(S.P.Bronze, FVector(20.f, 30.f, 16.f), T + FVector(0, 0, 160.f));
			for (int32 s = 0; s < 4; ++s) S.K.Rod(S.P.Rope, S.G(-800.f, -400.f + s * 200.f, 30.f), S.G(400.f, -500.f + s * 220.f, 30.f), 1.5f);
			Cairn(S, S.G(200.f, 0.f), 150.f, 0.f);
			for (int32 b = 0; b < 3; ++b)
			{
				// Bodies in sailcloth coats, face down, toward the tear (local +X), against the cairn.
				const FVector B = S.G(60.f - b * 40.f, -120.f + b * 120.f, 20.f);
				S.K.Add(S.P.Sail.Dim(0.7f), FVector(160.f, 50.f, 30.f), B, FRotator(0.f, 6.f * b, 0.f));
				S.K.Add(S.P.Flesh.Dim(0.8f), FVector(24.f, 22.f, 22.f), B + FVector(90.f, 0.f, 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				for (int32 d = 0; d < 3; ++d) S.K.Add(S.P.Ash, FVector(300.f, 14.f, 3.f), B - FVector(250.f + d * 10.f, -20.f + d * 20.f, 18.f));   // drag marks
			}
		}
		else if (Kind == TEXT("glass_footprints"))
		{
			// Drawn by GlassSea; nothing more.
		}
		else if (Kind == TEXT("riftmaw_slough"))
		{
			// A rift ringed with gnawed bones, a sloughed Riftmaw skin hung over a cairn.
			for (int32 b = 0; b < 40; ++b)
			{
				const float A = 2.f * PI * b / 40.f;
				S.K.Add(S.P.Bone, FVector(60.f, 12.f, 10.f), S.G(FMath::Cos(A) * 1100.f, FMath::Sin(A) * 1100.f, 4.f), FRotator(0.f, 40.f * b, 8.f));
			}
			Cairn(S, S.G(1400.f, 400.f), 160.f, 10.f);
			S.K.Add(S.P.Ash.Dim(0.7f), FVector(260.f, 20.f, 200.f), S.G(1400.f, 400.f, 120.f), FRotator(0.f, 30.f, 70.f));
		}
		else if (Kind == TEXT("nullwing_roost"))
		{
			// A broken horn-tower stub, its top crusted with droppings and voidglass grit, bones under it.
			Horn(S, FVector::ZeroVector, 520.f, 380.f, 2200.f, 100.f, 16, true, 0.55f);
			S.K.Add(S.P.Salt.Dim(0.9f), FVector(820.f, 820.f, 30.f), FVector(0, 0, 1220.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			for (int32 b = 0; b < 20; ++b) S.K.Add(S.P.Bone, FVector(50.f, 10.f, 10.f), S.G(700.f * S.K.Jit(FVector(b, 0, 0), 1), 700.f * S.K.Jit(FVector(b, 1, 0), 2), 5.f), FRotator(0.f, 37.f * b, 0.f));
		}
		else if (Kind == TEXT("last_pilgrim"))
		{
			// A pilgrim's pack on the last standing span, and a staff hanging in the air below the break.
			Satchel(S, FVector(-2500.f, 100.f, S.Gz(-5500.f, 0.f) + 200.f + 60.f), 0.f);
			FSurf Staff = S.P.Greywood;
			USceneComponent* Holder = NewObject<USceneComponent>(S.Land);
			Holder->SetupAttachment(S.Root);
			Holder->RegisterComponent();
			if (S.bDraw)
			{
				TArray<FWRLookBlock> B;
				FKit K(Holder->GetComponentTransform(), B);
				K.Rod(Staff, FVector(-80.f, 0.f, 0.f), FVector(80.f, 0.f, 20.f), 7.f);
				FWRBuildLook::Spawn(S.Land, Holder, B, FLinearColor::Gray, false);
			}
			FWRLandMover M;
			M.Comp = Holder;
			M.Base = FVector(-600.f, 0.f, FMath::Max(S.Gz(-5500.f, 0.f), S.Gz(5500.f, 0.f)) - 300.f);
			M.Bob = 25.f;
			M.BobPeriod = 9.f;
			M.Spin = 3.f;
			S.Land->AddMover(M);
		}
		else if (Kind == TEXT("last_choir"))
		{
			// On the bridge's first slabs: thirty dead in rows facing the island, mouths open, two standing at the front with
			// tuning-staves raised.
			for (int32 r = 0; r < 5; ++r) for (int32 c = 0; c < 6; ++c) Seated(S, FVector(-900.f - r * 170.f, -425.f + c * 170.f, S.Gz(-900.f - r * 170.f, -425.f + c * 170.f)), 0.f, 1.f, 0.f, true);
			for (const float Side : { -1.f, 1.f })
			{
				const FVector F = S.G(-500.f, Side * 140.f);
				S.K.Add(S.P.Shroud, FVector(50.f, 54.f, 160.f), F + FVector(0, 0, 80.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
				S.K.Add(S.P.ShroudDark, FVector(26.f, 28.f, 34.f), F + FVector(0, 0, 175.f), FRotator(-20.f, 0.f, 0.f), EWRLookMesh::Cone);
				S.K.Rod(S.P.Iron, F + FVector(20.f, Side * 30.f, 120.f), F + FVector(60.f, Side * 40.f, 330.f), 6.f);
				S.K.Add(S.P.Iron, FVector(4.f, 40.f, 40.f), F + FVector(62.f, Side * 40.f, 350.f));
			}
		}
		else if (Kind == TEXT("war_rack"))
		{
			// Iron tuning-staves racked facing the gorge; crystal arrowheads embedded in the fort's east wall.
			for (int32 r = 0; r < 3; ++r)
			{
				const FVector Base = S.G(0.f, -400.f + r * 400.f);
				S.K.Rod(S.P.GreywoodDark, Base + FVector(0, -150.f, 160.f), Base + FVector(0, 150.f, 160.f), 10.f);
				for (int32 s = 0; s < 6; ++s) S.K.Rod(S.P.Iron, Base + FVector(-30.f, -125.f + s * 50.f, 0.f), Base + FVector(10.f, -125.f + s * 50.f, 280.f), 5.f);
			}
			for (int32 a = 0; a < 14; ++a) S.K.Spike(S.P.Crystal, S.G(700.f, -500.f + a * 75.f, 200.f + 300.f * S.K.Rand(FVector(a, 0, 0), 3)), FVector(-1.f, 0.f, 0.f), 6.f, 30.f);
		}
		else if (Kind == TEXT("cage_watchers"))
		{
			// Two Choir statues outside the cage reaching through the bars, their hands bleached white on the crystal.
			for (const float Side : { -1.f, 1.f })
			{
				const FVector F = S.G(-180.f, Side * 300.f);
				S.K.Add(S.P.Dolo, FVector(70.f, 80.f, 210.f), F + FVector(0, 0, 105.f), FRotator(-12.f, 0.f, 0.f), EWRLookMesh::Cone);
				S.K.Add(S.P.DoloDark, FVector(34.f, 34.f, 42.f), F + FVector(30.f, 0, 230.f), FRotator(-30.f, 0.f, 0.f), EWRLookMesh::Cylinder);
				S.K.Rod(S.P.Dolo, F + FVector(20.f, Side * -10.f, 180.f), F + FVector(240.f, Side * -60.f, 170.f), 16.f);
				S.K.Add(S.P.Salt, FVector(30.f, 14.f, 10.f), F + FVector(250.f, Side * -62.f, 170.f));
			}
		}
		else if (Kind == TEXT("kill_scene"))
		{
			// Bones of the last kill, a dark stain: the living hunt is the wildlife's (S-5: a pack spawns here at dusk).
			S.K.Add(S.P.Char, FVector(300.f, 220.f, 2.f), S.G(0.f, 0.f, 1.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
			for (int32 b = 0; b < 8; ++b) S.K.Add(S.P.Bone, FVector(60.f, 12.f, 10.f), S.G(140.f * S.K.Jit(FVector(b, 0, 0), 1), 140.f * S.K.Jit(FVector(b, 1, 0), 2), 5.f), FRotator(0.f, 45.f * b, 0.f));
			S.K.Add(S.P.Bone.Dim(0.9f), FVector(40.f, 30.f, 26.f), S.G(60.f, 20.f, 13.f), FRotator(0.f, 30.f, 0.f), EWRLookMesh::Cone);   // a horned skull
		}
		else if (Kind == TEXT("beacon_room") || Kind == TEXT("hanging_cache_none"))
		{
			// Built with the Black Beacon.
		}
	}

	// ---------------------------------------------------------------- the Gauntlet's stations (arenas)
	void Station(FSite& S, int32 N, UMaterialInterface* RockMat)
	{
		if (N == 1)
		{
			// The Cantor's Stair: twelve shallow steps up to a lectern platform 30 m across; empty robes on stone hooks.
			for (int32 st = 0; st < 12; ++st)
			{
				const float X = -1600.f + st * 110.f;
				S.K.Add(S.P.Dolo.Dim(0.9f + 0.05f * (st % 2)), FVector(110.f, 2400.f, 18.f + st * 18.f), FVector(X, 0.f, (st * 18.f + 18.f) * 0.5f));
				S.Box(FVector(X, 0.f, (st * 18.f + 18.f) * 0.5f), FVector(55.f, 1200.f, (st * 18.f + 18.f) * 0.5f));
			}
			S.K.Add(S.P.DoloDark, FVector(3000.f, 3000.f, 40.f), FVector(1100.f, 0.f, 216.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			S.Box(FVector(1100.f, 0.f, 200.f), FVector(1060.f, 1060.f, 40.f));
			S.K.Add(S.P.Dolo, FVector(60.f, 110.f, 130.f), FVector(2300.f, 0.f, 300.f));   // the lectern
			S.K.Add(S.P.Dolo, FVector(70.f, 130.f, 12.f), FVector(2300.f, 0.f, 370.f), FRotator(-20.f, 0.f, 0.f));
			for (const float Side : { -1.f, 1.f })
			{
				Wall(S, FVector2D(-1700.f, Side * 1500.f), FVector2D(2600.f, Side * 1500.f), 700.f, 140.f, 0.2f);
				for (int32 h = 0; h < 10; ++h)
				{
					const FVector Hook(-1300.f + h * 380.f, Side * 1410.f, S.Gz(-1300.f + h * 380.f, Side * 1410.f) + 520.f);
					S.K.Add(S.P.DoloDark, FVector(30.f, 40.f, 20.f), Hook);
					S.K.Add(S.P.Shroud.Dim(0.8f), FVector(70.f, 30.f, 260.f), Hook - FVector(0, Side * 20.f, 140.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
				}
			}
			Brazier(S, FVector(2400.f, -600.f, 236.f));
			Brazier(S, FVector(2400.f, 600.f, 236.f));
		}
		else if (N == 2)
		{
			// The Rift Pens: six stone pens round a court 40 m across, each round a small tear, gates hanging off; the lair
			// rift at the north end (the Mother's).
			for (int32 p = 0; p < 6; ++p)
			{
				const float A = PI * 0.25f + PI * 1.5f * p / 5.f;
				const FVector2D C(FMath::Cos(A) * 1700.f, FMath::Sin(A) * 1700.f);
				for (int32 w = 0; w < 4; ++w)
				{
					if (w == 0) continue;   // open toward the court
					const float WA = A + PI + PI * 0.5f * w;
					const FVector2D D(FMath::Cos(WA), FMath::Sin(WA));
					const FVector2D T(-D.Y, D.X);
					Wall(S, C + D * 300.f - T * 300.f, C + D * 300.f + T * 300.f, 260.f, 50.f, 0.2f);
				}
				const FVector2D GateAt = C - FVector2D(FMath::Cos(A), FMath::Sin(A)) * 300.f;
				S.K.Add(S.P.Iron, FVector(10.f, 280.f, 220.f), S.G(GateAt.X, GateAt.Y, 90.f), FRotator(0.f, FMath::RadiansToDegrees(A) + 40.f, 28.f));
			}
			Wall(S, FVector2D(-2400.f, -2400.f), FVector2D(-2400.f, 2400.f), 400.f, 120.f, 0.35f);
		}
		else if (N == 3)
		{
			// The Alms House: a long hall (60 x 20 m) of bowl racks, the back wall fallen open onto the road.
			for (const float Side : { -1.f, 1.f })
			{
				Wall(S, FVector2D(-3000.f, Side * 1000.f), FVector2D(3000.f, Side * 1000.f), 900.f, 140.f, 0.15f);
				for (int32 r = 0; r < 12; ++r)
				{
					const float X = -2700.f + r * 480.f;
					for (int32 sh = 0; sh < 4; ++sh)
					{
						S.K.Add(S.P.GreywoodDark, FVector(400.f, 60.f, 8.f), S.G(X, Side * 900.f, 60.f + sh * 120.f));
						for (int32 b = 0; b < 6; ++b) Bowls(S, S.G(X - 160.f + b * 64.f, Side * 900.f, 64.f + sh * 120.f), 2, 20.f * b);
					}
				}
			}
			Wall(S, FVector2D(3000.f, -1000.f), FVector2D(3000.f, 1000.f), 900.f, 140.f, 0.f);
			Wall(S, FVector2D(-3000.f, -1000.f), FVector2D(-3000.f, 1000.f), 900.f, 140.f, 0.85f);   // the fallen back wall
			for (int32 i = 0; i < 4; ++i) S.Light(S.G(-2000.f + i * 1400.f, 0.f, 600.f), Lin(255, 170, 100), 2200.f, 1600.f);
		}
		else
		{
			// The Null Gate's court (35 m) before the gatehouse; the gate machine is its own actor (WRVoidMachines).
			for (const float Side : { -1.f, 1.f }) Wall(S, FVector2D(-1800.f, Side * 1750.f), FVector2D(1200.f, Side * 1750.f), 800.f, 160.f, 0.1f);
			for (const float Side : { -1.f, 1.f }) Horn(S, S.G(1500.f, Side * 700.f), 420.f, 300.f, 1600.f, 100.f, 14, true);
		}
	}

	// ---------------------------------------------------------------- kit POIs (the seeded pass)
	void Kit(FSite& S, const FString& Kind, FForest& Forest, UMaterialInterface* RockMat, int32 Index)
	{
		if (Kind == TEXT("ruin_hut") || Kind == TEXT("ruin_house"))
		{
			const float W = Kind == TEXT("ruin_house") ? 700.f : 450.f;
			Wall(S, FVector2D(-W, -W), FVector2D(W, -W), 380.f, 60.f, 0.5f);
			Wall(S, FVector2D(W, -W), FVector2D(W, W), 380.f, 60.f, 0.5f);
			Wall(S, FVector2D(-W, W), FVector2D(-W * 0.2f, W), 380.f, 60.f, 0.6f);
			Wall(S, FVector2D(-W, -W), FVector2D(-W, W * 0.3f), 380.f, 60.f, 0.7f);
			for (int32 r = 0; r < 8; ++r) S.K.Add(S.P.Dolo, FVector(60.f, 45.f, 40.f), S.G(W * 1.3f * S.K.Jit(FVector(r, 0, Index), 1), W * 1.3f * S.K.Jit(FVector(r, 1, Index), 2), 18.f), FRotator(10.f, 33.f * r, 6.f));
		}
		else if (Kind == TEXT("cairn_circle") || Kind == TEXT("cairn_stack"))
		{
			const int32 N = Kind == TEXT("cairn_stack") ? 1 : 9;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 2.f * PI * i / N;
				Cairn(S, S.G(N > 1 ? FMath::Cos(A) * 700.f : 0.f, N > 1 ? FMath::Sin(A) * 700.f : 0.f), N > 1 ? 130.f : 220.f, 40.f * i);
			}
		}
		else if (Kind == TEXT("warren"))
		{
			for (int32 h = 0; h < 6; ++h)
			{
				const FVector At = S.G(500.f * S.K.Jit(FVector(h, 0, Index), 1), 500.f * S.K.Jit(FVector(h, 1, Index), 2));
				S.K.Add(S.P.Ash.Dim(0.8f), FVector(160.f, 160.f, 50.f), At + FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
				S.K.Add(S.P.Char, FVector(50.f, 50.f, 8.f), At + FVector(0, 0, 32.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
		}
		else if (Kind == TEXT("greywood_stand"))
		{
			for (int32 t = 0; t < 9; ++t)
			{
				const FVector2D P(600.f * S.K.Jit(FVector(t, 0, Index), 3), 600.f * S.K.Jit(FVector(t, 1, Index), 4));
				Forest.Add(S.Land, S.Root->GetComponentTransform().TransformPosition(S.G(P.X, P.Y)), 1600.f + 500.f * S.K.Rand(FVector(t, 2, Index), 5), 360.f * S.K.Rand(FVector(t, 3, Index), 6), 3.f, 0.f, t + Index);
			}
		}
		else if (Kind == TEXT("cache") || Kind == TEXT("offering_niche"))
		{
			if (Kind == TEXT("offering_niche"))
			{
				Wall(S, FVector2D(-200.f, -250.f), FVector2D(-200.f, 250.f), 320.f, 80.f, 0.f);
				S.K.Add(S.P.Char, FVector(20.f, 160.f, 160.f), S.G(-150.f, 0.f, 120.f));
				Bowls(S, S.G(-120.f, 0.f, 60.f), 2, 0.f);
			}
			Satchel(S, S.G(0.f, 0.f), 30.f * Index);
			Cairn(S, S.G(200.f, 150.f), 100.f, 10.f);
		}
		else if (Kind == TEXT("litter_scar"))
		{
			for (int32 t = 0; t < 7; ++t)
			{
				const FVector2D P(500.f * S.K.Jit(FVector(t, 0, Index), 7), 500.f * S.K.Jit(FVector(t, 1, Index), 8));
				Forest.Add(S.Land, S.Root->GetComponentTransform().TransformPosition(S.G(P.X, P.Y)), 800.f + 300.f * S.K.Rand(FVector(t, 2, Index), 9), 360.f * S.K.Rand(FVector(t, 3, Index), 10), 5.f, 0.f, t, true);
			}
			S.K.Add(S.P.Char, FVector(1200.f, 1000.f, 2.f), S.G(0.f, 0.f, 1.f), FRotator(0.f, 30.f, 0.f), EWRLookMesh::Cylinder);
		}
		else if (Kind == TEXT("fire_tower") || Kind == TEXT("watch_post"))
		{
			// A greywood lookout on four legs, a ladder, a platform: a vista and a night fire.
			const float H = Kind == TEXT("fire_tower") ? 1200.f : 600.f;
			for (int32 l = 0; l < 4; ++l)
			{
				const FVector2D C(l % 2 ? 180.f : -180.f, l / 2 ? 180.f : -180.f);
				S.K.Rod(S.P.Greywood, S.G(C.X * 1.4f, C.Y * 1.4f), FVector(C.X, C.Y, S.Gz(0, 0) + H), 18.f);
			}
			S.K.Add(S.P.GreywoodDark, FVector(460.f, 460.f, 20.f), FVector(0, 0, S.Gz(0, 0) + H), FRotator::ZeroRotator);
			S.Box(FVector(0, 0, S.Gz(0, 0) + H), FVector(230.f, 230.f, 10.f));
			for (float Z = 40.f; Z < H; Z += 35.f)
			{
				S.K.Rod(S.P.Greywood, FVector(-260.f, -40.f, S.Gz(0, 0) + Z), FVector(-260.f, 40.f, S.Gz(0, 0) + Z), 5.f);
				S.Box(FVector(-260.f, 0.f, S.Gz(0, 0) + Z), FVector(10.f, 40.f, 4.f));
			}
			Brazier(S, FVector(0, 0, S.Gz(0, 0) + H + 10.f));
		}
		else if (Kind == TEXT("wreck"))
		{
			Hull(S, FVector::ZeroVector, 30.f * Index, 1400.f + 300.f * (Index % 3), 420.f, -40.f + 20.f * (Index % 3), Index % 2 == 0);
		}
		else if (Kind == TEXT("salt_pan") || Kind == TEXT("black_sand"))
		{
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector At = S.G(-500.f + (p % 2) * 500.f, -300.f + (p / 2) * 400.f);
				S.K.Add(S.P.DoloDark, FVector(420.f, 330.f, 14.f), At + FVector(0, 0, 7.f));
				S.K.Add(S.P.Salt, FVector(390.f, 300.f, 4.f), At + FVector(0, 0, 15.f));
			}
			S.K.Add(S.P.Salt.Dim(0.85f), FVector(180.f, 180.f, 130.f), S.G(500.f, 300.f, 65.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
		}
		else if (Kind == TEXT("small_theatre") || Kind == TEXT("vista_steps"))
		{
			for (int32 r = 0; r < 6; ++r)
			{
				const float R = 700.f + r * 180.f;
				for (int32 i = 0; i < 12 + r * 2; ++i)
				{
					const float A = FMath::DegreesToRadians(180.f - 70.f + 140.f * i / (11 + r * 2));
					S.K.Add(S.P.Dolo.Dim(0.9f + 0.08f * (r % 2)), FVector(175.f, 2.f * R * FMath::Sin(FMath::DegreesToRadians(70.f / (11 + r * 2))) + 6.f, 50.f + r * 40.f), S.G(FMath::Cos(A) * R, FMath::Sin(A) * R, (50.f + r * 40.f) * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
				}
			}
		}
		else if (Kind == TEXT("horn_stub") || Kind == TEXT("roost_tower"))
		{
			Horn(S, FVector::ZeroVector, 380.f, 260.f, Kind == TEXT("roost_tower") ? 1800.f : 900.f, 90.f, 14, true, Kind == TEXT("roost_tower") ? 0.75f : 0.6f);
		}
		else if (Kind == TEXT("quarry"))
		{
			for (int32 b = 0; b < 6; ++b) S.K.Add(S.P.Dolo, FVector(180.f, 120.f, 100.f), S.G(-300.f + b * 120.f, 300.f * S.K.Jit(FVector(b, 0, Index), 1), 50.f), FRotator(0.f, 7.f * b, 0.f));
			for (int32 r = 0; r < 4; ++r) Boulder(S, r + Index, FVector(400.f, 360.f, 300.f), S.G(-500.f + r * 300.f, -600.f), 40.f * r, RockMat);
		}
		else if (Kind == TEXT("rubble_teacher") || Kind == TEXT("crust_teacher"))
		{
			// A cairn with a ring of thrown stones: the camps' advice made into a place (the hazard lives here).
			Cairn(S, S.G(0.f, 0.f), 180.f, 0.f);
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = 2.f * PI * i / 10.f;
				S.K.Add(S.P.DoloDark, FVector(18.f, 16.f, 12.f), S.G(FMath::Cos(A) * 500.f, FMath::Sin(A) * 500.f, 6.f), FRotator(0.f, 30.f * i, 0.f));
			}
		}
		else if (Kind == TEXT("offering_rows") || Kind == TEXT("cairn_field_small"))
		{
			if (Kind == TEXT("offering_rows")) for (int32 r = 0; r < 2; ++r) for (int32 c = 0; c < 5; ++c) Seated(S, S.G(-r * 170.f, -340.f + c * 170.f), 0.f);
			else for (int32 i = 0; i < 12; ++i) Cairn(S, S.G(800.f * S.K.Jit(FVector(i, 0, Index), 1), 800.f * S.K.Jit(FVector(i, 1, Index), 2)), 120.f, 30.f * i, i % 3 == 0);
		}
		else if (Kind == TEXT("vista_bone"))
		{
			// A giant's rib arching out of the dust.
			for (int32 s = 0; s < 10; ++s)
			{
				const float A0 = PI * s / 10.f, A1 = PI * (s + 1) / 10.f;
				S.K.Rod(S.P.Bone, S.G(0.f, 0.f) + FVector(-FMath::Cos(A0) * 1500.f, 0.f, FMath::Sin(A0) * 1200.f - 100.f), S.G(0.f, 0.f) + FVector(-FMath::Cos(A1) * 1500.f, 0.f, FMath::Sin(A1) * 1200.f - 100.f), 160.f - s * 6.f);
			}
		}
		else if (Kind == TEXT("kill_scene"))
		{
			Story(S, Kind, nullptr, Forest, RockMat);
		}
		// small_rift / rift_ring / rift_pool: the rift itself is the place (WRRift places them from kitPois).
	}

	// ---------------------------------------------------------------- decoration between the places
	void Decorate(AWRLand* Land, FForest& Forest, UMaterialInterface* RockMat)
	{
		const FWRLandDef& D = *Land->Def();
		const bool bDraw = Land->GetNetMode() != NM_DedicatedServer;
		// Pines and rocks on a jittered 22 m grid by each zone's density; cairns every 150 m along the routes (30 m on the
		// Gauntlet Road). Kept off the places (their own pass already dressed them) and off the Great Rift's lip.
		TArray<FVector2D> Pois;
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (D.Json->TryGetArrayField(TEXT("pois"), L)) for (const TSharedPtr<FJsonValue>& V : *L) Pois.Add(WRLand::Vec2(V->AsObject()->TryGetField(TEXT("at"))));
		TArray<UStaticMesh*> Rocks;
		for (const TCHAR* N : { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("12A") })
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N))) Rocks.Add(M);
		}
		TArray<UHierarchicalInstancedStaticMeshComponent*> RockParts;
		for (UStaticMesh* M : Rocks)
		{
			UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(Land);
			H->SetStaticMesh(M);
			H->SetupAttachment(Land->GetRootComponent());
			H->SetCollisionProfileName(TEXT("BlockAll"));
			H->SetCanEverAffectNavigation(false);
			if (RockMat) H->SetMaterial(0, RockMat);
			H->SetCullDistances(0, 26000);
			H->RegisterComponent();
			RockParts.Add(H);
		}
		TArray<FWRLookBlock> Tufts;
		const float Step = 22.f;
		for (float Y = D.MapBox.Min.Y; Y < D.MapBox.Max.Y; Y += Step)
		{
			for (float X = D.MapBox.Min.X; X < D.MapBox.Max.X; X += Step)
			{
				const FVector2D Cell(X, Y);
				const float J1 = D.Noise(Cell * 0.731f + FVector2D(11.f, 3.f)), J2 = D.Noise(Cell * 0.917f + FVector2D(5.f, 17.f));
				const FVector2D M = Cell + FVector2D(J1, J2) * Step * 0.45f;
				if (D.CoastDistance(M) < 4.f || D.HoleAt(M, 25.f)) continue;
				bool bNear = false;
				for (const FVector2D& P : Pois) if (FVector2D::DistSquared(P, M) < 45.f * 45.f) { bNear = true; break; }
				if (bNear) continue;
				const FWRLandZone* Z = D.ZoneAt(M);
				if (!Z || !Z->Decor.IsValid()) continue;
				const float R = (D.Noise(M * 1.37f + FVector2D(2.f, 9.f)) + 1.f) * 0.5f;
				const float Pines = Num(Z->Decor, TEXT("pines"), 0.f);
				const float RockD = Num(Z->Decor, TEXT("rocks"), 0.f);
				const float Grass = Num(Z->Decor, TEXT("greyGrass"), 0.f);
				const FVector W = D.GroundPoint(M);
				const float Slope = FMath::Abs(D.Height(M + FVector2D(2.f, 0.f)) - D.Height(M - FVector2D(2.f, 0.f))) + FMath::Abs(D.Height(M + FVector2D(0.f, 2.f)) - D.Height(M - FVector2D(0.f, 2.f)));
				// The Withered Forest: dense standing dead pines in stands (noise-clustered), 12-22 m.
				const float Stand = (D.Noise(M / 90.f + FVector2D(1.f, 1.f)) + 1.f) * 0.5f;
				if (Pines > 0.f && Slope < 4.f && R < Pines * 0.55f * Stand)
				{
					Forest.Add(Land, W, 1200.f + 1000.f * R, 360.f * J1, 2.f + 3.f * FMath::Abs(J2), 360.f * J2, int32(FMath::Abs(J1) * 97.f));
					continue;
				}
				if (RockD > 0.f && R > 1.f - RockD * 0.12f && RockParts.Num())
				{
					const float S = 120.f + 380.f * (R - (1.f - RockD * 0.12f)) / (RockD * 0.12f + 0.001f);
					UHierarchicalInstancedStaticMeshComponent* H = RockParts[int32(FMath::Abs(J2) * 31.f) % RockParts.Num()];
					const FBox B = H->GetStaticMesh()->GetBoundingBox();
					const float K = S / FMath::Max(1.f, B.GetSize().GetMax());
					H->AddInstance(FTransform(FRotator(6.f * J1, 360.f * J2, 6.f * J2), W - FVector(0, 0, S * 0.2f), FVector(K, K, K * 0.8f)), true);
					continue;
				}
				if (bDraw && Grass > 0.f && R < Grass * 0.5f)
				{
					// Dead grass tufts (thin grey blades), standing straight up near the rifts.
					FKit K(FTransform::Identity, Tufts);
					for (int32 b = 0; b < 5; ++b)
					{
						const FVector O(30.f * FMath::Cos(b * 1.3f + J1 * 6.f), 30.f * FMath::Sin(b * 1.3f + J1 * 6.f), 0.f);
						K.Spike(FSurf::Stone(Lin(170, 165, 150), 0.95f), W - Land->GetActorLocation() + O, FVector(0.08f * J1, 0.08f * J2, 1.f), 5.f, 45.f + 25.f * R);
					}
				}
			}
		}
		if (bDraw && Tufts.Num())
		{
			const int32 Before = Land->GetRootComponent()->GetNumChildrenComponents();
			FWRBuildLook::Spawn(Land, Land->GetRootComponent(), Tufts, Lin(163, 158, 149), false);
			for (int32 i = Before; i < Land->GetRootComponent()->GetNumChildrenComponents(); ++i)
			{
				if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(Land->GetRootComponent()->GetChildComponent(i))) I->SetCullDistances(0, 9000);
			}
		}
		// Cairns along the routes: every 150 m on the roads between camps and the Quire, every 30 m on the Gauntlet.
		if (const FWRLandFeature* Road = D.Feature(TEXT("gauntlet_road")))
		{
			FSite S(Land, FTransform(D.Origin), bDraw);
			for (int32 i = 0; i + 1 < Road->Points.Num(); ++i)
			{
				const float Len = FVector2D::Distance(Road->Points[i], Road->Points[i + 1]);
				const FVector2D Dir = (Road->Points[i + 1] - Road->Points[i]) / Len;
				const FVector2D Side(-Dir.Y, Dir.X);
				for (float T = 15.f; T < Len; T += 30.f)
				{
					const FVector2D P = Road->Points[i] + Dir * T + Side * (Road->Width * 0.5f + 2.f) * ((int32(T / 30.f) % 2) ? 1.f : -1.f);
					Cairn(S, D.GroundPoint(P) - D.Origin, 140.f, T);
				}
			}
			S.Done(26000.f);
		}
	}
}

// ================================================================================================ the entry

void WRLandSites::Build(AWRLand* Land)
{
	using namespace WRLandSitesImpl;
	if (!Land || !Land->Def()) return;
	const FWRLandDef& D = *Land->Def();
	const bool bDraw = Land->GetNetMode() != NM_DedicatedServer;
	const double T0 = FPlatformTime::Seconds();
	FForest Forest;
	Forest.Init(Land);
	UMaterialInterface* RockMat = WRFoldLook::RockMat(Land, Lin(150, 146, 140) * 0.32f);
	int32 Count = 0;
	const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr;
	if (D.Json->TryGetArrayField(TEXT("pois"), Pois))
	{
		for (const TSharedPtr<FJsonValue>& V : *Pois)
		{
			const TSharedPtr<FJsonObject> P = V->AsObject();
			const FString Kind = Str(P, TEXT("kind"));
			const FTransform T = SiteFrame(Land, P);
			if (Kind == TEXT("cave"))
			{
				// The caves are the island's sculpted mounds (they replicate: the server spawns them).
				if (Land->HasAuthority())
				{
					const FTransform CT(FRotator(0.f, Num(P, TEXT("yaw"), 0.f), 0.f), T.GetLocation(), FVector(1.2f));
					if (AWRCave* Cave = Land->GetWorld()->SpawnActorDeferred<AWRCave>(AWRCave::StaticClass(), CT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
					{
						Cave->Biome = D.Biome;
						Cave->Variant = int32(Num(P, TEXT("variant"), 0.f));
						Cave->FinishSpawning(CT);
					}
				}
				FWRPoi& Poi = FWRPoi::All.AddDefaulted_GetRef();
				Poi.Name = Str(P, TEXT("name"));
				Poi.Kind = TEXT("cave");
				Poi.Location = T.GetLocation();
				++Count;
				continue;
			}
			FSite S(Land, T, bDraw);
			if (Kind == TEXT("throat")) Throat(S);
			else if (Kind == TEXT("colossus")) Colossus(S);
			else if (Kind == TEXT("hanging_wood")) HangingWood(S, Forest);
			else if (Kind == TEXT("hollow_fort")) HollowFort(S);
			else if (Kind == TEXT("viaduct")) Viaduct(S, RockMat);
			else if (Kind == TEXT("ash_cage")) AshCage(S);
			else if (Kind == TEXT("broken_knee")) BrokenKnee(S);
			else if (Kind == TEXT("glass_sea")) GlassSea(S);
			else if (Kind == TEXT("black_beacon")) BlackBeacon(S, Land->LightMaterial());
			else if (Kind == TEXT("great_rift")) GreatRiftColumn(S, Land->LightMaterial());
			else if (Kind.StartsWith(TEXT("camp_")))
			{
				Camp(S, Kind);
				FWRPoi& Poi = FWRPoi::All.AddDefaulted_GetRef();
				Poi.Name = Str(P, TEXT("name"));
				Poi.Kind = TEXT("camp");
				Poi.Location = T.GetLocation();
			}
			else if (Kind.StartsWith(TEXT("station_"))) Station(S, int32(Num(P, TEXT("station"), 1.f)), RockMat);
			else Story(S, Kind, P, Forest, RockMat);
			S.Done(Num(P, TEXT("landmark"), 0.f) > 0.f || P->HasField(TEXT("landmark")) ? 500000.f : 45000.f);
			++Count;
		}
	}
	// The kit POIs: the seeded pass over each zone (150 m from any place; fills route gaps > 125 m).
	if (const TSharedPtr<FJsonObject> KitDef = D.Section(TEXT("kitPois")))
	{
		const float MinGap = Num(KitDef, TEXT("minGap"), 150.f);
		TArray<FVector2D> Taken;
		if (Pois) for (const TSharedPtr<FJsonValue>& V : *Pois) Taken.Add(WRLand::Vec2(V->AsObject()->TryGetField(TEXT("at"))));
		const TSharedPtr<FJsonObject>* Zones = nullptr;
		if (KitDef->TryGetObjectField(TEXT("zones"), Zones))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& ZV : (*Zones)->Values)
			{
				const TSharedPtr<FJsonObject> Z = ZV.Value->AsObject();
				const int32 Want = int32(Num(Z, TEXT("count"), 0.f));
				const TSharedPtr<FJsonObject>* KindsObj = nullptr;
				if (!Z->TryGetObjectField(TEXT("kinds"), KindsObj)) continue;
				// The kinds as a deck: each kind as many times as its weight, dealt in order.
				TArray<FString> Deck;
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*KindsObj)->Values) for (int32 n = 0; n < int32(KV.Value->AsNumber()); ++n) Deck.Add(KV.Key);
				const FWRLandZone* Zone = D.Zones.FindByPredicate([&](const FWRLandZone& Q) { return Q.Id == FName(*ZV.Key); });
				if (!Zone || Deck.Num() == 0) continue;
				int32 Placed = 0;
				for (int32 Try = 0; Try < 4000 && Placed < Want; ++Try)
				{
					const FVector2D M(FMath::Lerp(D.MapBox.Min.X, D.MapBox.Max.X, (D.Noise(FVector2D(Try * 0.61f, 3.3f)) + 1.f) * 0.5f),
						FMath::Lerp(D.MapBox.Min.Y, D.MapBox.Max.Y, (D.Noise(FVector2D(7.7f, Try * 0.53f)) + 1.f) * 0.5f));
					if (!Zone->Contains(M) || D.ZoneAt(M) != Zone || D.CoastDistance(M) < 25.f || D.HoleAt(M, 40.f)) continue;
					bool bClose = false;
					for (const FVector2D& Q : Taken) if (FVector2D::DistSquared(Q, M) < MinGap * MinGap) { bClose = true; break; }
					if (bClose) continue;
					Taken.Add(M);
					const FString Kind = Deck[Placed % Deck.Num()];
					FSite S(Land, FTransform(FRotator(0.f, 360.f * (D.Noise(M * 0.3f) * 0.5f + 0.5f), 0.f), D.GroundPoint(M)), bDraw);
					Kit(S, Kind, Forest, RockMat, Placed);
					S.Done(40000.f);
					WRLandSites::KitPois().Add({ D.Id, FName(*Kind), M });
					++Placed;
					++Count;
				}
				UE_LOG(LogWildreach, Log, TEXT("LAND %s: %s %d of %d kit POIs"), *D.Id.ToString(), *ZV.Key, Placed, Want);
			}
		}
	}
	Decorate(Land, Forest, RockMat);
	UE_LOG(LogWildreach, Log, TEXT("LAND %s: %d places built in %.2f s"), *D.Id.ToString(), Count, FPlatformTime::Seconds() - T0);
}

TArray<WRLandSites::FKitPoi>& WRLandSites::KitPois()
{
	static TArray<FKitPoi> List;
	return List;
}
