// The Ocean's landmarks, shrines' settings, camps, caves, the Drowned Hall and the decoration (cooks/biome-ocean.md
// 2.3-2.5, cooks/build-biome-ocean.md 2.2-2.5), modelled in code at real scale on the forest's FBuild.
#include "WROceanSceneryParts.h"
#include "Wildreach.h"
#include "WRFx.h"
#include "WRTide.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"

namespace WROceanParts
{
	float FOcCtx::WRTideMeanZ() { return WRTide::MeanZ(); }

	// ------------------------------------------------------------------------------------------------ helpers

	/** A rock dome seen from inside (a cave room's walls and roof): rings of points round Centre up to a cap, jittered
	 *  like water-cut limestone, every triangle facing the middle. Floor not included. */
	void OcDomeInward(FBuild& B, const FVector& Centre, float R, float H, int32 Sides, const FLinearColor& Col, float Jitter)
	{
		const int32 Rings = 6;
		TArray<TArray<FVector>> P;
		for (int32 r = 0; r <= Rings; ++r)
		{
			const float A = float(r) / Rings * 0.5f * PI;       // 0 at the floor ring, pi/2 at the cap
			TArray<FVector>& Ring = P.AddDefaulted_GetRef();
			for (int32 s = 0; s < Sides; ++s)
			{
				const float T = float(s) / Sides * 2.f * PI;
				const float J = 1.f + Jitter * (B.Rand(r * 97 + s * 13) - 0.5f);
				const float Rr = R * FMath::Cos(A) * J;
				Ring.Add(Centre + FVector(FMath::Cos(T) * Rr, FMath::Sin(T) * Rr, H * FMath::Sin(A) * (r == 0 ? 0.f : J)));
			}
		}
		const FColor C = FWRToolMesh::Pack(Col, 0.9f);
		const auto Tri = [&](const FVector& X, const FVector& Y, const FVector& Z)
		{
			const FVector N = FVector::CrossProduct(Y - X, Z - X);
			// Faces the middle: the winding's cross product points away from the centre (Unreal draws it facing back).
			const bool bOut = FVector::DotProduct(N, (X + Y + Z) / 3.f - Centre) > 0.f;
			if (bOut) B.M.Tri(X, Y, Z, C, FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1));
			else B.M.Tri(X, Z, Y, C, FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 0));
		};
		for (int32 r = 0; r < Rings; ++r)
		{
			for (int32 s = 0; s < Sides; ++s)
			{
				const int32 s1 = (s + 1) % Sides;
				Tri(P[r][s], P[r][s1], P[r + 1][s1]);
				Tri(P[r][s], P[r + 1][s1], P[r + 1][s]);
			}
		}
	}

	/** Collision for a cave room: a ring of wall boxes and a roof slab (the shell is solid rock round the room). */
	void OcRoomCollision(FBuild& B, const FVector& Centre, float R, float H)
	{
		const int32 N = 14;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = (i + 0.5f) / N * 2.f * PI;
			const FVector P = Centre + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (R + 60.f) + FVector(0, 0, H * 0.5f);
			B.Box(P, FVector(60.f, PI * R / N + 40.f, H * 0.5f + 60.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
		}
		B.Box(Centre + FVector(0, 0, H + 60.f), FVector(R * 0.95f, R * 0.95f, 60.f));
		B.Box(Centre + FVector(0, 0, H * 0.8f + 60.f), FVector(R * 0.75f, R * 0.75f, 60.f), FRotator(0.f, 45.f, 0.f));
	}

	/** A rock mound seen from outside (the Pearl Cave's reef outcrop, Siren's Hollow's headland): a lumpy dome. */
	void OcMoundOutward(FBuild& B, const FVector& Base, float R, float H, const FLinearColor& Col, int32 Seed)
	{
		TArray<FVector> Prof;
		for (int32 i = 0; i <= 8; ++i)
		{
			const float T = i / 8.f;
			const float Rr = R * FMath::Sqrt(FMath::Max(0.f, 1.f - T * T)) * (0.92f + 0.16f * B.Rand(i + Seed));
			Prof.Add(FVector(T, FMath::Max(20.f, Rr), FMath::Max(20.f, Rr * (0.85f + 0.2f * B.Rand(i * 3 + Seed)))));
		}
		B.M.Lathe(Base - FVector(0, 0, 200.f), FVector::UpVector, H + 200.f, Prof, 18, Col, 0.92f, 0.f, float(Seed % 50 + 1));
	}

	// ------------------------------------------------------------------------------------------------ the Pound

	void Pound(FOcCtx& C)
	{
		FBuild& B = C.B;
		const float R = C.D.PoundRadius();                 // the wall's inner face
		const float Thick = 500.f, H = C.D.ArenaNum(TEXT("thalassor"), TEXT("wallTop"), 30.f) * 100.f;
		const float Flat = C.D.PoundAt().Z - float(C.A->GetActorLocation().Z);   // the flat's level (the actor stands on the pool floor)
		const int32 Segs = 96;
		const FLinearColor Lime = OcSRGB(0xC9, 0xA4, 0x6A) * 0.55f;
		// The wall's faces as one turned mesh each (inner and outer), its courses picked out as 60 cm bands that step in
		// a hand's breadth, a battered plinth at the foot.
		{
			TArray<FVector> In, Out;
			for (int32 c = 0; c <= 50; ++c)
			{
				const float T = float(c) / 50.f;
				const float Batter = (1.f - FMath::Min(1.f, T * 6.f)) * 60.f;            // the plinth's batter
				const float Step = (c % 2) ? 3.f : 0.f;
				In.Add(FVector(T, R - Batter - Step, R - Batter - Step));
				Out.Add(FVector(T, R + Thick + Batter + Step, R + Thick + Batter + Step));
			}
			// The inner face is seen from inside: built as an outward lathe of the outer radius set, then its inner by
			// the dome's trick (a flipped lathe): the turned inner face is a ring of quads facing the middle.
			const FColor Col = FWRToolMesh::Pack(Lime, 0.9f), Dark = FWRToolMesh::Pack(Lime * 0.82f, 0.9f);
			for (int32 c = 0; c < 50; ++c)
			{
				const float Z0 = Flat + H * c / 50.f - 60.f, Z1 = Flat + H * (c + 1) / 50.f - 60.f;
				for (int32 s = 0; s < Segs; ++s)
				{
					const float A0 = float(s) / Segs * 2.f * PI, A1 = float(s + 1) / Segs * 2.f * PI;
					const float R0 = In[c].Y, R1 = In[c + 1].Y;
					const FVector P00(FMath::Cos(A0) * R0, FMath::Sin(A0) * R0, Z0), P10(FMath::Cos(A1) * R0, FMath::Sin(A1) * R0, Z0);
					const FVector P01(FMath::Cos(A0) * R1, FMath::Sin(A0) * R1, Z1), P11(FMath::Cos(A1) * R1, FMath::Sin(A1) * R1, Z1);
					// Joints: every other course the block ends move half a block (two segments a block).
					const FColor Cc = ((s + (c % 2)) % 2 == 0 && B.Rand(c * 131 + s) < 0.25f) ? Dark : Col;
					B.M.Tri(P00, P11, P10, Cc, FVector2D(0, 0), FVector2D(1, 1), FVector2D(1, 0));
					B.M.Tri(P00, P01, P11, Cc, FVector2D(0, 0), FVector2D(0, 1), FVector2D(1, 1));
				}
			}
			B.M.Lathe(FVector(0, 0, Flat - 60.f), FVector::UpVector, H + 60.f, Out, Segs, Lime * 0.95f, 0.9f);
		}
		// The walkway's paving (4 m) and its parapets, a little lower on the lagoon side (you look in over it).
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A = (s + 0.5f) / Segs * 360.f;
			const FRotator Ry(0.f, A, 0.f);
			const float Chord = 2.f * PI * (R + Thick * 0.5f) / Segs + 6.f;
			B.K.Add(OcLime(0.95f + 0.1f * B.Rand(s)), FVector(Thick, Chord, 30.f), Ry.RotateVector(FVector(R + Thick * 0.5f, 0, Flat + H - 15.f)), Ry + FRotator(0, 90.f, 0));
			B.K.Add(OcLime(0.9f), FVector(50.f, Chord * 0.96f, 80.f), Ry.RotateVector(FVector(R + 25.f, 0, Flat + H + 40.f)), Ry + FRotator(0, 90.f, 0));
			B.K.Add(OcLime(0.9f), FVector(60.f, Chord * 0.96f, 110.f), Ry.RotateVector(FVector(R + Thick - 30.f, 0, Flat + H + 55.f)), Ry + FRotator(0, 90.f, 0));
			// Collision: the wall body, the walkway on top, the parapets.
			B.Box(Ry.RotateVector(FVector(R + Thick * 0.5f, 0, Flat + H * 0.5f - 30.f)), FVector(Thick * 0.5f, Chord * 0.5f, H * 0.5f + 30.f), Ry);
			B.Box(Ry.RotateVector(FVector(R + 25.f, 0, Flat + H + 40.f)), FVector(25.f, Chord * 0.5f, 40.f), Ry);
			B.Box(Ry.RotateVector(FVector(R + Thick - 30.f, 0, Flat + H + 55.f)), FVector(30.f, Chord * 0.5f, 55.f), Ry);
			// Buttresses outside every 30 degrees (the wall holds the terraces back).
			if (s % 8 == 0)
			{
				B.K.Add(OcLime(0.85f), FVector(260.f, 300.f, H - 300.f), Ry.RotateVector(FVector(R + Thick + 120.f, 0, Flat + (H - 300.f) * 0.5f)), Ry);
				B.Box(Ry.RotateVector(FVector(R + Thick + 120.f, 0, Flat + (H - 300.f) * 0.5f)), FVector(130.f, 150.f, (H - 300.f) * 0.5f), Ry);
			}
		}
		// The sluice gates (east, cook 2.14): four openings 6 m wide and 8 m tall through the wall's foot, a bronze sill
		// beam at +1.4 m, two leaves each on hinge drums; the north-east one burst outward long ago (its leaves bent like
		// foil), the other three shut until Thalassor's transition swings them out (AWROceanScenery::Leaves).
		const float GateAngles[4] = { -24.f, -8.f, 8.f, 24.f };
		const int32 Burst = 0;
		for (int32 g = 0; g < 4; ++g)
		{
			const FRotator Ry(0.f, GateAngles[g], 0.f);
			const FVector Mid = Ry.RotateVector(FVector(R + Thick * 0.5f, 0, Flat));
			// The gate's frame: two bronze-capped piers and a lintel of three voussoir blocks.
			for (int32 s = -1; s <= 1; s += 2)
			{
				B.K.Add(OcLime(1.05f), FVector(Thick + 40.f, 90.f, 900.f), Mid + Ry.RotateVector(FVector(0, s * 345.f, 450.f)), Ry);
				B.K.Add(OcVerdigris(), FVector(Thick + 50.f, 30.f, 40.f), Mid + Ry.RotateVector(FVector(0, s * 300.f, 880.f)), Ry);
			}
			B.K.Add(OcLime(1.1f), FVector(Thick + 30.f, 780.f, 120.f), Mid + Ry.RotateVector(FVector(0, 0, 960.f)), Ry);
			B.K.Add(OcVerdigris(), FVector(Thick, 600.f, 30.f), Mid + Ry.RotateVector(FVector(0, 0, 140.f)), Ry);   // the sill beam
			B.Box(Mid + Ry.RotateVector(FVector(0, 0, 70.f)), FVector(Thick * 0.5f, 300.f, 70.f), Ry);
			for (int32 s = -1; s <= 1; s += 2)
			{
				// The hinge drum on the pier's lagoon face, its leaf hung from it (the leaf's 3 m swings about the drum).
				const FVector Hinge = Mid + Ry.RotateVector(FVector(-Thick * 0.5f + 20.f, s * 300.f, 140.f));
				B.K.Add(OcBronze(), FVector(40.f, 40.f, 760.f), Hinge + FVector(0, 0, 380.f), Ry, EWRLookMesh::Cylinder);
				if (g == Burst)
				{
					// Burst: the leaves bent out past the wall, twisted and torn at their free edge.
					const FRotator Rl = Ry + FRotator(6.f * s, s * 74.f, s * 9.f);
					B.K.Add(OcVerdigris(), FVector(30.f, 300.f, 700.f), Hinge + Rl.RotateVector(FVector(0, -s * 150.f, 360.f)), Rl);
					B.K.Add(OcBright(), FVector(6.f, 40.f, 600.f), Hinge + Rl.RotateVector(FVector(14.f, -s * 296.f, 360.f)), Rl);   // fresh-bright tear
					continue;
				}
				if (C.A->GetNetMode() == NM_DedicatedServer) { B.Box(Hinge + Ry.RotateVector(FVector(0, -s * 150.f, 380.f)), FVector(15.f, 150.f, 380.f), Ry); continue; }
				// A shut leaf: its own pivot so it can swing (every machine), with its bronze ribs.
				USceneComponent* Pivot = NewObject<USceneComponent>(C.A);
				Pivot->SetupAttachment(C.A->GetRootComponent());
				Pivot->SetRelativeLocation(Hinge);
				Pivot->SetRelativeRotation(Ry);
				Pivot->RegisterComponent();
				TArray<FWRLookBlock> Leaf;
				FKit LK(FTransform::Identity, Leaf);
				LK.Add(OcVerdigris(), FVector(30.f, 300.f, 740.f), FVector(0, -s * 150.f, 370.f));
				for (int32 rb = 0; rb < 5; ++rb) LK.Add(OcBronze(), FVector(40.f, 290.f, 24.f), FVector(-10.f, -s * 150.f, 60.f + rb * 160.f));
				LK.Add(OcBronze(), FVector(44.f, 24.f, 740.f), FVector(-10.f, -s * 290.f, 370.f));
				FWRBuildLook::Spawn(C.A, Pivot, Leaf, FLinearColor::Black, false);
				C.A->Leaves.Add(Pivot);
				C.A->LeafSign.Add(-float(s));
				B.Box(Hinge + Ry.RotateVector(FVector(0, -s * 150.f, 380.f)), FVector(15.f, 150.f, 380.f), Ry);
			}
		}
		// The stair towers (N, W, S): an outer flight from the terraces up to the walkway, an inner flight down the
		// lagoon face to the flat; 2 m wide, 30 cm risers, the treads of dressed slabs.
		const float StairAt[3] = { -90.f, 180.f, 90.f };
		for (int32 st = 0; st < 3; ++st)
		{
			const FRotator Ry(0.f, StairAt[st], 0.f);
			const FVector Radial = Ry.Vector(), Tangent = FRotator(0.f, StairAt[st] + 90.f, 0.f).Vector();
			// Outer flight: from the ground outside (B.G there) up to the walkway, along the outer face.
			const FVector OutFoot = Radial * (R + Thick + 220.f) - Tangent * 600.f;
			const float G0 = B.G(OutFoot.X, OutFoot.Y, Flat + 1500.f);
			const float Rise = Flat + H - G0;
			const int32 Steps = FMath::Max(4, int32(Rise / 30.f));
			for (int32 k = 0; k < Steps; ++k)
			{
				const FVector P = OutFoot + Tangent * (k * (1200.f / Steps)) + FVector(0, 0, G0 + k * Rise / Steps);
				B.K.Add(OcLime(0.95f + 0.08f * B.Rand(k)), FVector(1200.f / Steps + 4.f, 200.f, 30.f + 8.f), P + FVector(0, 0, 15.f), FRotator(0.f, StairAt[st] + 90.f, 0.f));
			}
			B.SegBox(OutFoot + FVector(0, 0, G0 - 20.f), OutFoot + Tangent * 1200.f + FVector(0, 0, Flat + H - 20.f), 100.f, 25.f);
			B.K.Add(OcLime(0.88f), FVector(1260.f, 60.f, 140.f), OutFoot + Tangent * 600.f + Radial * 120.f + FVector(0, 0, G0 + Rise * 0.5f + 70.f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(Rise, 1200.f)), StairAt[st] + 90.f, 0.f));
			// Inner flight down to the flat (it starts at the parapet's gap).
			const FVector InTop = Radial * (R - 110.f) + Tangent * 300.f;
			const int32 InSteps = int32(H / 30.f);
			const float Run = H * 1.4f;
			for (int32 k = 0; k < InSteps; ++k)
			{
				const FVector P = InTop - Tangent * (k * Run / InSteps) + FVector(0, 0, Flat + H - k * H / InSteps);
				B.K.Add(OcLime(0.92f + 0.08f * B.Rand(k + 50)), FVector(Run / InSteps + 4.f, 200.f, 38.f), P - FVector(0, 0, 19.f), FRotator(0.f, StairAt[st] + 90.f, 0.f));
			}
			B.SegBox(InTop + FVector(0, 0, Flat + H - 20.f), InTop - Tangent * Run + FVector(0, 0, Flat - 20.f), 100.f, 25.f);
			// The stair's corbelled landing on the walkway: where the run-back stone's path comes in (north).
			B.K.Add(OcLime(1.05f), FVector(300.f, 300.f, 40.f), Radial * (R + Thick + 150.f) + Tangent * 600.f + FVector(0, 0, Flat + H - 20.f), Ry);
			B.Box(Radial * (R + Thick + 150.f) + Tangent * 600.f + FVector(0, 0, Flat + H - 20.f), FVector(150.f, 150.f, 20.f), Ry);
			// The mirror plinth: a stone drum on the walkway with a bronze trunnion yoke (the dish is the mirror's own).
			const FVector Pl = Radial * (R + Thick * 0.5f) - Tangent * 500.f + FVector(0, 0, Flat + H);
			B.K.Add(OcLime(1.0f), FVector(160.f, 160.f, 90.f), Pl + FVector(0, 0, 45.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.K.Add(OcBronze(), FVector(30.f, 220.f, 30.f), Pl + FVector(0, 0, 110.f), Ry + FRotator(0, 90.f, 0));
			B.Box(Pl + FVector(0, 0, 45.f), FVector(80.f, 80.f, 45.f));
		}
		// The flat's eight stilt-tower stumps are AWROceanBossProp towers (WROceanBosses: Thalassor's breach breaks them);
		// its four bronze bollards are scenery.
		for (const FVector& Bo : C.D.ArenaPoints(TEXT("thalassor"), TEXT("bollards")))
		{
			const FVector Lp = C.L(FVector(Bo.X, Bo.Y, C.A->GetActorLocation().Z));
			OcBollard(B, FVector(Lp.X, Lp.Y, B.G(Lp.X, Lp.Y, Flat)), 30.f, 0.8f);
		}
		// Harpoons rusting in the flat's mud, here and there (the hunters' leavings).
		for (int32 h = 0; h < 22; ++h)
		{
			const float A = B.Rand(h * 3) * 2.f * PI, D = 4500.f + 6000.f * B.Rand(h * 7);
			const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
			OcHarpoon(B, FVector(P.X, P.Y, B.G(P.X, P.Y, Flat) - 60.f), FVector(B.Rand(h) - 0.5f, B.Rand(h + 9) - 0.5f, 1.6f), 240.f, true);
		}
		B.Loop(TEXT("ocean.pound_spray"), FVector(R + Thick, 0, Flat + H), FRotator::ZeroRotator, 2.f);
	}

	// ------------------------------------------------------------------------------------------------ the Colossus

	void Colossus(FOcCtx& C)
	{
		// The Sunwatch's champion face-down in the Gate Inlet (biome-desert's fix): 60 m of sandstone, its chest buried
		// 2 m so the water line cuts the shoulders; the back eaten through by coral and barnacles; a Saltkin whaling chain
		// wrapped thrice round the neck to a bronze stake; one arm reaching west to the pass; the gold-leaf eye (turned
		// up on the cheek that shows) catches the moon. +X is toward the pass (the head).
		FBuild& B = C.B;
		const float Z0 = B.G(0.f, 0.f, 0.f) - 200.f;
		const FLinearColor Sand = OcSRGB(0xC8, 0xA0, 0x70) * 0.55f, SandDark = OcSRGB(0x8E, 0x70, 0x50) * 0.5f;
		// The torso and hips (a long body lying prone, its back up): lofted ovals.
		B.M.Lathe(FVector(-2600.f, 0, Z0 + 250.f), FVector(1, 0, 0), 2600.f, { FVector(0.f, 380.f, 230.f), FVector(0.3f, 560.f, 330.f), FVector(0.7f, 620.f, 380.f), FVector(1.f, 520.f, 330.f) }, 16, Sand, 0.9f);
		B.M.Lathe(FVector(-5200.f, 0, Z0 + 180.f), FVector(1, 0, 0), 2700.f, { FVector(0.f, 260.f, 160.f), FVector(0.5f, 400.f, 220.f), FVector(1.f, 380.f, 230.f) }, 14, Sand * 0.95f, 0.9f);
		// The legs trailing east (toward the sea), one a little bent.
		for (int32 s = -1; s <= 1; s += 2)
		{
			B.Limb(FVector(-5000.f, s * 220.f, Z0 + 150.f), FVector(-7400.f, s * 300.f, Z0 + 90.f), 200.f, 150.f, Sand * 0.92f, 12);
			B.Limb(FVector(-7400.f, s * 300.f, Z0 + 90.f), FVector(-9000.f, s * (330.f + 120.f * (s > 0)), Z0 + 60.f), 150.f, 110.f, Sand * 0.9f, 12);
		}
		// The head: a striped royal headcloth (the Sunwatch's), turned so the right cheek shows, the gold-leaf eye.
		const FVector Head(900.f, 0.f, Z0 + 380.f);
		B.Ball(Head, 420.f, Sand, 0.88f, 0.9f);
		for (int32 s = 0; s < 7; ++s) B.Limb(Head + FVector(-300.f, -380.f + s * 120.f, 260.f), Head + FVector(-700.f, -440.f + s * 140.f, 40.f), 40.f, 40.f, (s % 2) ? Sand : SandDark, 6);
		B.K.Add(OcGold(), FVector(80.f, 40.f, 30.f), Head + FVector(200.f, 330.f, 140.f), FRotator(0.f, 20.f, 0.f));
		B.N.Add(FSurf::Lit(OcSRGB(0xFF, 0xD8, 0x80), 2.f), FVector(60.f, 30.f, 20.f), Head + FVector(210.f, 345.f, 140.f), FRotator(0.f, 20.f, 0.f));
		// The arms: the left pinned under the body, the right reaching west past the head toward Saltgate.
		B.Limb(FVector(-300.f, 500.f, Z0 + 300.f), FVector(1700.f, 700.f, Z0 + 160.f), 170.f, 130.f, Sand * 0.95f, 12);
		B.Limb(FVector(1700.f, 700.f, Z0 + 160.f), FVector(3300.f, 600.f, Z0 + 120.f), 130.f, 110.f, Sand * 0.93f, 12);
		for (int32 f = 0; f < 4; ++f) B.Limb(FVector(3300.f, 600.f, Z0 + 120.f), FVector(3700.f, 480.f + f * 80.f, Z0 + 60.f), 32.f, 24.f, Sand * 0.9f, 6);
		B.Limb(FVector(-300.f, -520.f, Z0 + 260.f), FVector(-1200.f, -900.f, Z0 + 60.f), 160.f, 130.f, Sand * 0.9f, 12);
		// The back eaten through: coral heads and barnacle crusts in its hollows.
		for (int32 k = 0; k < 26; ++k)
		{
			const FVector P(-4800.f + 5400.f * B.Rand(k * 3), (B.Rand(k * 5) - 0.5f) * 700.f, Z0 + 560.f);
			OcCoralHead(B, P, 0.7f + 0.9f * B.Rand(k), k % 6, k + B.Seed);
			B.K.Add(OcBarnacle(), FVector(120.f, 90.f, 30.f), P + FVector(150.f, 80.f, -30.f), FRotator(0.f, k * 37.f, 0.f));
		}
		// The whaling chain thrice round the neck, then away west to its bronze stake in the inlet floor.
		const FVector Neck(500.f, 0.f, Z0 + 330.f);
		for (int32 w = 0; w < 3; ++w)
		{
			for (int32 s = 0; s < 12; ++s)
			{
				const float A0 = s / 12.f * 2.f * PI, A1 = (s + 1) / 12.f * 2.f * PI;
				OcChain(B, Neck + FVector(w * 60.f, FMath::Cos(A0) * 430.f, FMath::Sin(A0) * 330.f), Neck + FVector(w * 60.f + 5.f, FMath::Cos(A1) * 430.f, FMath::Sin(A1) * 330.f), 70.f, 0.f, OcRust());
			}
		}
		const FVector Stake(2400.f, -1500.f, B.G(2400.f, -1500.f, Z0));
		B.K.Add(OcVerdigris(), FVector(90.f, 90.f, 400.f), Stake + FVector(0, 0, 100.f), FRotator(12.f, 30.f, 0.f), EWRLookMesh::Cylinder);
		OcChain(B, Neck + FVector(0, -430.f, 0.f), Stake + FVector(0, 0, 260.f), 70.f, 200.f, OcRust());
		// Saltkin harpoons still in the shoulders.
		for (int32 h = 0; h < 5; ++h) OcHarpoon(B, FVector(-400.f + h * 260.f, -400.f + (h % 2) * 800.f, Z0 + 500.f), FVector(0.2f, (h % 2) ? 0.3f : -0.3f, 1.f), 220.f, true);
		// Collision: the body's bulk (you can climb on the back at low tide).
		B.Box(FVector(-1400.f, 0, Z0 + 300.f), FVector(1500.f, 560.f, 330.f));
		B.Box(FVector(-3900.f, 0, Z0 + 220.f), FVector(1300.f, 400.f, 230.f));
		B.Box(Head, FVector(400.f, 400.f, 380.f));
		B.SegBox(FVector(-300.f, 500.f, Z0 + 300.f), FVector(3300.f, 600.f, Z0 + 120.f), 140.f, 140.f);
		B.Loop(TEXT("ocean.surf_wash"), FVector(-2000.f, 700.f, C.SeaZ()), FRotator::ZeroRotator, 3.f);
	}

	// ------------------------------------------------------------------------------------------------ the Beacon Stack

	void BeaconStack(FOcCtx& C)
	{
		// A 70 m sea stack of eroded limestone drums 120 m off the shelf (its foot on the sea floor), a half-fallen Saltkin
		// causeway of block piers back to the shelf, a keeper's stair round the stack on corbels, the driftwood beacon (a
		// 6 m cone of lashed driftwood) under a cage of shell chimes on an iron hoop. Lit by a clan, its fire burns.
		FBuild& B = C.B;
		const float Foot = 0.f, Top = 7000.f - float(C.A->GetActorLocation().Z);   // the summit at 70 m over the sea
		const FLinearColor Lime = OcSRGB(0xC9, 0xA4, 0x6A) * 0.5f;
		TArray<FVector> Prof;
		for (int32 d = 0; d <= 14; ++d)
		{
			const float T = d / 14.f;
			const float R = FMath::Lerp(1500.f, 900.f, T) * (0.9f + 0.18f * B.Rand(d)) - ((d % 3 == 1) ? 120.f : 0.f);   // drums with notches
			Prof.Add(FVector(T, R, R * 0.92f));
		}
		B.M.Lathe(FVector(0, 0, Foot - 400.f), FVector::UpVector, Top + 400.f, Prof, 20, Lime, 0.9f, 0.f, 7.f);
		B.M.Lathe(FVector(0, 0, Foot + Top - 10.f), FVector::UpVector, 60.f, { FVector(0.f, 880.f, 820.f), FVector(1.f, 840.f, 780.f) }, 20, Lime * 1.1f, 0.9f);
		for (int32 b = 0; b < 8; ++b) B.Box(FVector(0, 0, Foot + Top * 0.5f), FVector(1150.f, 400.f, Top * 0.5f + 300.f), FRotator(0.f, b * 22.5f, 0.f));
		// The barnacle and weed bands at the tide's reach.
		const float Sea = C.SeaZ();
		B.M.Lathe(FVector(0, 0, Sea - 150.f), FVector::UpVector, 270.f, { FVector(0.f, 1520.f, 1400.f), FVector(1.f, 1500.f, 1380.f) }, 20, OcSRGB(0x2A, 0x2A, 0x28) * 0.5f, 0.35f);
		// The keeper's stair: corbelled steps spiralling up the stack (two and a half turns).
		const int32 Steps = 240;
		for (int32 k = 0; k < Steps; ++k)
		{
			const float T = float(k) / Steps;
			const float A = T * 2.5f * 2.f * PI + PI;
			const float Z = Sea + 200.f + T * (Foot + Top - Sea - 200.f);
			const float R = FMath::Lerp(1500.f, 900.f, (Z - Foot) / Top) + 110.f;
			const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
			const FRotator Rt(0.f, FMath::RadiansToDegrees(A), 0.f);
			B.K.Add(OcLime(0.95f + 0.1f * B.Rand(k)), FVector(170.f, 70.f, 24.f), P, Rt);
			if (k % 3 == 0) B.K.Add(OcLime(0.82f), FVector(60.f, 50.f, 60.f), P - FVector(0, 0, 40.f) - Rt.RotateVector(FVector(60.f, 0, 0)), Rt);
			B.Box(P, FVector(85.f, 35.f, 12.f), Rt);
		}
		// The causeway: piers of blocks every 12 m west to the shelf, the middle two fallen (stubs in the surf).
		for (int32 p = 1; p <= 9; ++p)
		{
			const FVector At(-1500.f - p * 1200.f, 0.f, 0.f);
			const float G = B.G(At.X, At.Y, Foot - 300.f);
			const bool bFallen = p == 4 || p == 5;
			const float H = bFallen ? FMath::Max(80.f, Sea - G - 60.f) : Sea - G + 260.f;
			B.K.Add(OcLime(0.9f), FVector(300.f, 380.f, H), FVector(At.X, At.Y, G + H * 0.5f), FRotator(0.f, B.Rand(p) * 6.f, 0.f));
			B.Box(FVector(At.X, At.Y, G + H * 0.5f), FVector(150.f, 190.f, H * 0.5f));
			if (!bFallen && p != 3 && p != 6) B.K.Add(OcLime(1.0f), FVector(1240.f, 300.f, 40.f), FVector(At.X + 600.f, 0, Sea + 280.f));
			if (!bFallen && p != 3 && p != 6) B.Box(FVector(At.X + 600.f, 0, Sea + 280.f), FVector(620.f, 150.f, 20.f));
		}
		// The beacon: a cone of lashed driftwood on the summit, the shell-chime cage over it on an iron hoop.
		const FVector Sum(0, 0, Foot + Top + 50.f);
		for (int32 l = 0; l < 22; ++l)
		{
			const float A = l / 22.f * 2.f * PI;
			B.Limb(Sum + FVector(FMath::Cos(A) * 260.f, FMath::Sin(A) * 260.f, 0.f), Sum + FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, 600.f), 14.f, 8.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 6);
		}
		for (int32 r = 0; r < 4; ++r) B.Line(Sum + FVector(-200.f + r * 30.f, -200.f, 120.f + r * 120.f), Sum + FVector(200.f - r * 30.f, 200.f, 120.f + r * 120.f), 3.f, 10.f, OcSRGB(0x8C, 0x7A, 0x5A) * 0.45f);
		for (int32 u = 0; u < 4; ++u) B.K.Rod(OcIron(), Sum + FRotator(0.f, u * 90.f, 0.f).RotateVector(FVector(380.f, 0, 0)), Sum + FVector(0, 0, 820.f), 8.f);
		B.K.Disc(OcIron(), Sum + FVector(0, 0, 720.f), FVector::UpVector, 520.f, 8.f);
		for (int32 s = 0; s < 18; ++s)
		{
			const FVector H = Sum + FRotator(0.f, s * 20.f, 0.f).RotateVector(FVector(255.f, 0, 715.f));
			B.K.Rod(OcNet(), H, H - FVector(0, 0, 60.f + 30.f * B.Rand(s)), 1.5f);
			B.K.Add(s % 2 ? OcShellPink() : OcShell(), FVector(14.f, 10.f, 5.f), H - FVector(0, 0, 70.f + 30.f * B.Rand(s)), FRotator(70.f, s * 20.f, 0.f), EWRLookMesh::Cone);
		}
		B.Box(Sum + FVector(0, 0, 300.f), FVector(220.f, 220.f, 300.f));
		B.Loop(TEXT("ocean.chime_wind"), Sum + FVector(0, 0, 700.f));
		// The fire (shows for the clans that lit it): its flame, its embers, a light that reaches 300 m over the shelf.
		if (C.A->GetNetMode() != NM_DedicatedServer)
		{
			USceneComponent* Fire = NewObject<USceneComponent>(C.A);
			Fire->SetupAttachment(C.A->GetRootComponent());
			Fire->SetRelativeLocation(Sum + FVector(0, 0, 250.f));
			Fire->RegisterComponent();
			if (USceneComponent* Fx = WRFx::Attach(Fire, TEXT("ocean.beacon_fire"), NAME_None, 3.f)) Fx->SetRelativeLocation(FVector::ZeroVector);
			UPointLightComponent* Lt = NewObject<UPointLightComponent>(C.A);
			Lt->SetupAttachment(Fire);
			Lt->SetLightColor(OcSRGB(0xFF, 0xA8, 0x58));
			Lt->SetIntensity(WRTune(TEXT("oceanWorld.beaconLight"), 200000.f));
			Lt->SetAttenuationRadius(WRTune(TEXT("oceanWorld.beaconRadius"), 30000.f));
			Lt->SetCastShadows(false);
			Lt->RegisterComponent();
			Fire->SetVisibility(false, true);
			C.A->Fire = Fire;
		}
	}

	// ------------------------------------------------------------------------------------------------ the Spouting Horn

	void SpoutingHorn(FOcCtx& C)
	{
		// A 3 m vent in the shelf down into the Blowhole Grotto (its surge drives the column: rising and high water only),
		// a Saltkin arch round it (two leaning pillars, a capstone carved with a wave), the worn footholds round the rim
		// that show where to stand to ride it (O-S3), and a 22 m rock stack beside it with the hidden ledge at 18 m.
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		for (int32 r = 0; r < 18; ++r)
		{
			const float A = r / 18.f * 360.f;
			const FRotator Ra(0.f, A, 0.f);
			B.K.Add(OcLime(0.8f + 0.15f * B.Rand(r)), FVector(90.f, 110.f, 70.f), Ra.RotateVector(FVector(200.f, 0, G - 10.f)), Ra);
			B.Box(Ra.RotateVector(FVector(200.f, 0, G - 10.f)), FVector(45.f, 55.f, 35.f), Ra);
			if (r % 3 == 0) B.K.Disc(FSurf::Stone(OcSRGB(0x5A, 0x50, 0x40) * 0.5f, 0.6f), Ra.RotateVector(FVector(290.f, 0, G + 2.f)), FVector::UpVector, 50.f, 4.f);   // a foothold
		}
		B.M.Lathe(FVector(0, 0, G - 900.f), FVector::UpVector, 900.f, { FVector(0.f, 160.f, 160.f), FVector(1.f, 150.f, 150.f) }, 14, OcSRGB(0x2A, 0x2A, 0x28) * 0.5f, 0.3f);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector Foot(0.f, s * 420.f, G);
			for (int32 c = 0; c < 9; ++c) B.K.Add(OcLime(0.95f), FVector(110.f, 100.f, 60.f), Foot + FVector(0, -s * c * 14.f, 30.f + c * 60.f), FRotator(0.f, 0.f, s * 8.f));
			B.Box(Foot + FVector(0, -s * 60.f, 270.f), FVector(55.f, 60.f, 270.f), FRotator(0.f, 0.f, s * 8.f));
		}
		B.K.Add(OcLime(1.05f), FVector(140.f, 760.f, 80.f), FVector(0, 0, G + 580.f));
		for (int32 w = 0; w < 3; ++w) B.K.Ring(OcLimeDark(), FVector(70.f, -120.f + w * 120.f, G + 570.f), 28.f, 6.f, 6.f, 6, 0.f, 180.f, 1.f);
		// The stack and its hidden ledge (O-S3: the Horn's launch throws you up to it).
		const FVector St(-1400.f, 600.f, G);
		B.M.Lathe(St - FVector(0, 0, 300.f), FVector::UpVector, 2500.f, { FVector(0.f, 600.f, 560.f), FVector(0.5f, 480.f, 440.f), FVector(0.8f, 420.f, 400.f), FVector(1.f, 300.f, 280.f) }, 14, OcSRGB(0xC9, 0xA4, 0x6A) * 0.5f, 0.9f, 0.f, 3.f);
		for (int32 b = 0; b < 4; ++b) B.Box(St + FVector(0, 0, 1100.f), FVector(500.f, 260.f, 1400.f), FRotator(0.f, b * 45.f, 0.f));
		const FVector Ledge = St + FVector(450.f, -150.f, 1800.f);
		B.K.Add(OcLime(0.95f), FVector(500.f, 360.f, 60.f), Ledge);
		B.Box(Ledge, FVector(250.f, 180.f, 30.f));
		// The Saltkin watch hut on the ledge: three low walls, a driftwood roof, a sleeping mat.
		OcCoursedWall(B, Ledge + FVector(-200.f, -150.f, 30.f), Ledge + FVector(-200.f, 150.f, 30.f), 160.f, 40.f, 80.f, 40.f);
		OcCoursedWall(B, Ledge + FVector(-200.f, 150.f, 30.f), Ledge + FVector(80.f, 150.f, 30.f), 160.f, 40.f, 80.f, 40.f);
		for (int32 r = 0; r < 6; ++r) B.K.Rod(OcDrift(), Ledge + FVector(-220.f, -160.f + r * 60.f, 200.f), Ledge + FVector(100.f, -160.f + r * 60.f, 180.f), 9.f);
		B.K.Add(OcNet(), FVector(180.f, 80.f, 6.f), Ledge + FVector(-80.f, -60.f, 34.f));
	}

	// ------------------------------------------------------------------------------------------------ the Dyke

	void DykeHead(FOcCtx& C)
	{
		// The landward tower of the Long Dyke: 8 m square, 25 m tall in big coursed blocks, a gate stair up the land side
		// to a door at 3 m, and on the seaward face the door the dyke's wall top leaves by; over it the empty iron
		// key-hook the size of a man (the key the king held, gone: story beat 5), and under the hook the iron-bound vault
		// door (O-S2: the Sluice key opens it). A phosphor lamp niche high on the sea face.
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		const float S = 800.f, H = 2500.f;
		for (int32 sd = 0; sd < 4; ++sd)
		{
			const FRotator R(0.f, sd * 90.f, 0.f);
			const FVector A = R.RotateVector(FVector(S * 0.5f, -S * 0.5f, G)), Bt = R.RotateVector(FVector(S * 0.5f, S * 0.5f, G));
			OcCoursedWall(B, A, Bt, H, 90.f, 160.f, 80.f, 1.f, true);
		}
		B.K.Add(OcLime(1.05f), FVector(S + 60.f, S + 60.f, 50.f), FVector(0, 0, G + H + 25.f));
		for (int32 m = 0; m < 12; ++m) B.K.Add(OcLime(0.95f), FVector(90.f, 90.f, 70.f), FRotator(0.f, m * 30.f, 0.f).RotateVector(FVector(S * 0.5f, 0, 0)) * FVector(1, 1, 0) + FVector(0, 0, G + H + 85.f));
		B.Box(FVector(0, 0, G + H + 25.f), FVector(S * 0.5f + 30.f, S * 0.5f + 30.f, 25.f));
		// The gate stair (land side, -X): 10 steps up to the door's sill at 3 m.
		for (int32 k = 0; k < 10; ++k)
		{
			B.K.Add(OcLime(0.95f), FVector(40.f, 260.f, 300.f - k * 30.f), FVector(-S * 0.5f - 60.f - (9 - k) * 40.f, 0.f, G + (300.f - k * 30.f) * 0.5f));
			B.Box(FVector(-S * 0.5f - 60.f - (9 - k) * 40.f, 0.f, G + (300.f - k * 30.f) * 0.5f), FVector(20.f, 130.f, (300.f - k * 30.f) * 0.5f));
		}
		B.K.Add(OcTimber(0.6f), FVector(14.f, 160.f, 240.f), FVector(-S * 0.5f - 6.f, 0, G + 420.f));
		// The seaward face (+X): the dyke's door at the wall top's level, the key-hook over it, the vault door under.
		const float Wt = WRTide::MeanZ() + C.D.DykeWallTop - C.A->GetActorLocation().Z;
		B.K.Add(OcIron(), FVector(10.f, 180.f, 230.f), FVector(S * 0.5f + 8.f, 0, Wt + 140.f));
		B.K.Ring(OcLime(1.1f), FVector(S * 0.5f + 20.f, 0, Wt + 250.f), 100.f, 40.f, 60.f, 7, 0.f, 180.f, 2.f);
		const FVector Hook(S * 0.5f + 30.f, 0, Wt + 520.f);
		B.K.Add(OcRust(), FVector(40.f, 40.f, 60.f), Hook);                                                // the hook's boss
		B.K.Rod(OcRust(), Hook + FVector(10.f, 0, -20.f), Hook + FVector(70.f, 0, -20.f), 18.f);
		B.K.Rod(OcRust(), Hook + FVector(70.f, 0, -20.f), Hook + FVector(70.f, 0, -200.f), 18.f);
		B.K.Rod(OcRust(), Hook + FVector(70.f, 0, -200.f), Hook + FVector(30.f, 0, -170.f), 16.f);         // empty
		// The vault door (land floor level on the sea face's foot): planks, three iron straps, a keyhole plate.
		B.K.Add(OcTimber(0.5f), FVector(16.f, 150.f, 220.f), FVector(S * 0.5f + 10.f, 260.f, G + 110.f));
		for (int32 st = 0; st < 3; ++st) B.K.Add(OcIron(), FVector(20.f, 156.f, 14.f), FVector(S * 0.5f + 16.f, 260.f, G + 30.f + st * 80.f));
		B.K.Add(OcIron(), FVector(22.f, 26.f, 36.f), FVector(S * 0.5f + 18.f, 300.f, G + 110.f));
		B.N.Add(OcPhosphor(), FVector(10.f, 60.f, 60.f), FVector(S * 0.5f + 6.f, -250.f, G + H - 300.f));
		B.Lamp(FVector(S * 0.5f + 80.f, -250.f, G + H - 300.f), OcSRGB(0x5A, 0xE0, 0xC8), 1800.f, 1400.f);
	}

	void LongDyke(FOcCtx& C)
	{
		// Eleven tower stumps 200 m apart from the Dyke Head out to the reef and curving north, joined by the sunken wall
		// top (3 m wide at -1.0 m: dry at low tide, 2.2 m under at high). Each stump 8 m square, standing 2-6 m over mean
		// water, a phosphor lamp niche on its sea face. Stump 7 holds the Drowned Hall's one-way Sluice Door.
		FBuild& B = C.B;
		const float Mean = WRTide::MeanZ();
		const float WallTop = Mean + C.D.DykeWallTop - C.A->GetActorLocation().Z;
		const TArray<FVector>& T = C.D.DykeTowers;
		for (int32 i = 0; i < T.Num(); ++i)
		{
			const FVector Lp = C.L(FVector(T[i].X, T[i].Y, C.A->GetActorLocation().Z));
			const float G = B.G(Lp.X, Lp.Y, WallTop - 300.f);
			const float Top = WallTop + 100.f + 200.f + 400.f * B.Rand(i * 11);
			if (i > 0)
			{
				for (int32 sd = 0; sd < 4; ++sd)
				{
					const FRotator R(0.f, sd * 90.f + 8.f * B.Rand(i), 0.f);
					OcCoursedWall(B, Lp + R.RotateVector(FVector(400.f, -400.f, 0.f)) + FVector(0, 0, G), Lp + R.RotateVector(FVector(400.f, 400.f, 0.f)) + FVector(0, 0, G), Top - G, 80.f, 160.f, 80.f, 0.9f, true);
				}
				B.K.Add(OcLime(0.85f), FVector(780.f, 780.f, 40.f), Lp + FVector(0, 0, Top + 10.f));
				B.Box(Lp + FVector(0, 0, Top + 10.f), FVector(390.f, 390.f, 20.f));
				B.N.Add(OcPhosphor(), FVector(10.f, 50.f, 50.f), Lp + FVector(405.f, 0, Top - 120.f));
				B.Lamp(Lp + FVector(470.f, 0, Top - 120.f), OcSRGB(0x5A, 0xE0, 0xC8), 900.f, 1000.f);
				// Barnacles and weed at the tide's band round the stump.
				for (int32 sd = 0; sd < 4; ++sd) B.K.Add(OcBarnacle(), FVector(20.f, 800.f, 160.f), Lp + FRotator(0.f, sd * 90.f, 0.f).RotateVector(FVector(410.f, 0, 0)) + FVector(0, 0, Mean - C.A->GetActorLocation().Z), FRotator(0.f, sd * 90.f, 0.f));
				if (i == 6)
				{
					// The Sluice Door: a bronze-bound hatch in the stump's landward face at the wall top, barred inside.
					B.K.Add(OcVerdigris(), FVector(16.f, 160.f, 200.f), Lp + FVector(-408.f, 0, WallTop + 110.f));
					B.K.Add(OcBronze(), FVector(20.f, 170.f, 18.f), Lp + FVector(-412.f, 0, WallTop + 150.f));
				}
			}
			// The wall top to the next stump.
			if (i + 1 < T.Num())
			{
				const FVector Ln = C.L(FVector(T[i + 1].X, T[i + 1].Y, C.A->GetActorLocation().Z));
				const FVector A(Lp.X, Lp.Y, 0.f), Bn(Ln.X, Ln.Y, 0.f);
				const FVector D = (Bn - A).GetSafeNormal2D();
				const FVector From = A + D * 420.f, To = Bn - D * 420.f;
				const float L = FVector::Dist2D(From, To);
				const FRotator R(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f);
				for (float s = 0.f; s < L; s += 180.f)
				{
					// Its blocks: the wall's top course (worn) and its sides down to the sea floor (two courses showing).
					const FVector P = From + D * (s + 90.f);
					const float Gp = B.G(P.X, P.Y, WallTop - 300.f);
					B.K.Add(OcLime(0.78f + 0.15f * B.Rand(int32(s) + i * 7)), FVector(176.f, 300.f, 60.f), FVector(P.X, P.Y, WallTop - 30.f), R + FRotator(0.f, 0.f, 2.f * (B.Rand(int32(s)) - 0.5f)));
					if (WallTop - 60.f > Gp) B.K.Add(OcLime(0.7f), FVector(180.f, 320.f, WallTop - 60.f - Gp), FVector(P.X, P.Y, (WallTop - 60.f + Gp) * 0.5f), R);
				}
				B.SegBox(From + FVector(0, 0, WallTop - 30.f), To + FVector(0, 0, WallTop - 30.f), 150.f, 30.f);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------ the reef towers

	void ReefLighthouse(FOcCtx& C)
	{
		// A Saltkin lighthouse on the reef crest: a round tower 6 m across, broken at 20 m and leaning 8 degrees seaward
		// (east), a stair inside up to the open lamp room where the shrine is; bronze ladles with their bottoms knocked out
		// hang from every sill (the funayurei charm), clinking.
		FBuild& B = C.B;
		const float G = B.G(0, 0, -100.f);
		const FRotator Lean(-8.f, 0.f, 0.f);             // pitch down toward +X (seaward)
		const auto P = [&](float X, float Y, float Z) { return FVector(0, 0, G) + Lean.RotateVector(FVector(X, Y, Z)); };
		const float H = 2000.f;
		for (int32 c = 0; c < int32(H / 50.f); ++c)
		{
			const int32 N = 22;
			for (int32 s = 0; s < N; ++s)
			{
				const float A = (s + (c % 2) * 0.5f) / N * 360.f;
				// Broken top: courses above 17 m only on the landward half, jagged.
				if (c * 50.f > 1700.f && (FMath::Cos(FMath::DegreesToRadians(A)) > -0.1f || B.Rand(c * 31 + s) < 0.4f)) continue;
				// Windows every 4 m (one a course band): a gap.
				if (c % 80 > 0 && (c % 16) >= 12 && (c % 16) <= 14 && (s == 0 || s == 11)) continue;
				const FRotator Ra = Lean + FRotator(0.f, A, 0.f);
				B.K.Add(OcLime(0.88f + 0.18f * B.Rand(c * 19 + s)), FVector(60.f, 2.f * PI * 300.f / N + 2.f, 48.f), P(0, 0, 25.f + c * 50.f) + Ra.RotateVector(FVector(300.f, 0, 0)), Ra);
			}
		}
		for (int32 b = 0; b < 6; ++b) B.Box(P(0, 0, H * 0.5f), FVector(320.f, 140.f, H * 0.5f), Lean + FRotator(0.f, b * 30.f, 0.f));
		// The stair inside (blocks winding up the inner wall; collision per step).
		for (int32 k = 0; k < 64; ++k)
		{
			const float A = k / 64.f * 4.f * PI;
			const FVector Sp = P(FMath::Cos(A) * 200.f, FMath::Sin(A) * 200.f, 30.f + k * 28.f);
			B.K.Add(OcLime(0.92f), FVector(150.f, 60.f, 26.f), Sp, Lean + FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
			B.Box(Sp, FVector(75.f, 30.f, 13.f), Lean + FRotator(0.f, FMath::RadiansToDegrees(A) + 90.f, 0.f));
		}
		B.K.Add(OcLime(0.95f), FVector(560.f, 560.f, 30.f), P(0, 0, 1820.f), Lean, EWRLookMesh::Cylinder);
		B.Box(P(0, 0, 1820.f), FVector(270.f, 270.f, 15.f), Lean);
		// The ladles: on cords from the window sills and the broken rim, bottoms knocked out.
		for (int32 l = 0; l < 24; ++l)
		{
			const float A = l / 24.f * 2.f * PI;
			const FVector Hang = P(FMath::Cos(A) * 340.f, FMath::Sin(A) * 340.f, 700.f + (l % 3) * 400.f);
			B.K.Rod(OcNet(), Hang, Hang - FVector(0, 0, 50.f), 1.5f);
			B.K.Add(OcVerdigris(), FVector(18.f, 18.f, 10.f), Hang - FVector(0, 0, 60.f), FRotator(0.f, l * 15.f, 0.f), EWRLookMesh::Cylinder);
			B.K.Rod(OcVerdigris(), Hang - FVector(0, 0, 55.f), Hang - FVector(0, 0, 10.f), 2.5f);
		}
		B.Loop(TEXT("ocean.ladles_clink"), P(0, 0, 900.f));
		B.Loop(TEXT("ocean.surf_crest"), FVector(400.f, 0, C.SeaZ()), FRotator::ZeroRotator, 2.f);
	}

	void BellTower(FOcCtx& C)
	{
		// The top of the Drowned Hall's bell tower in the crest's lee: 4 m square, its louvred openings and the phosphor
		// finial, its stairwell running down 18 m into the Bell Chamber (way in 1). At midnight high tide the shutter over
		// its east window stands open (O-S1).
		FBuild& B = C.B;
		const float Mean = WRTide::MeanZ() - C.A->GetActorLocation().Z;
		const float Base = Mean - 1800.f, Top = Mean + 540.f;
		for (int32 c = 0; c < int32((Top - Base) / 50.f); ++c)
		{
			const float Z = Base + 25.f + c * 50.f;
			for (int32 sd = 0; sd < 4; ++sd)
			{
				const FRotator R(0.f, sd * 90.f, 0.f);
				// The louvred openings near the top (the bell's voice comes out), the window low on the east face.
				const bool bLouvre = Z > Mean + 100.f && Z < Mean + 400.f;
				const bool bWindow = sd == 0 && Z > Mean - 320.f && Z < Mean - 120.f;
				if (bLouvre || bWindow)
				{
					B.K.Add(OcLime(0.9f), FVector(50.f, 70.f, 48.f), R.RotateVector(FVector(200.f, -165.f, Z)), R);
					B.K.Add(OcLime(0.9f), FVector(50.f, 70.f, 48.f), R.RotateVector(FVector(200.f, 165.f, Z)), R);
					if (bLouvre && c % 2 == 0) B.K.Add(OcTimber(0.5f), FVector(16.f, 260.f, 10.f), R.RotateVector(FVector(196.f, 0, Z)), R + FRotator(30.f, 0.f, 0.f));
					continue;
				}
				B.K.Add(OcLime(0.82f + 0.15f * B.Rand(c * 9 + sd)), FVector(50.f, 400.f, 48.f), R.RotateVector(FVector(200.f, 0, Z)), R);
			}
		}
		for (int32 sd = 0; sd < 4; ++sd) B.Box(FRotator(0.f, sd * 90.f, 0.f).RotateVector(FVector(200.f, 0, (Base + Top) * 0.5f)), FVector(25.f, 200.f, (Top - Base) * 0.5f), FRotator(0.f, sd * 90.f, 0.f));
		// The cap and the finial (a bronze fish on a rod, phosphor-painted).
		B.K.Spike(OcLime(1.0f), FVector(0, 0, Top), FVector::UpVector, 460.f, 260.f);
		B.K.Rod(OcVerdigris(), FVector(0, 0, Top + 240.f), FVector(0, 0, Top + 420.f), 8.f);
		B.K.Add(OcVerdigris(), FVector(70.f, 14.f, 26.f), FVector(0, 0, Top + 430.f));
		B.N.Add(OcPhosphor(), FVector(74.f, 16.f, 28.f), FVector(0, 0, Top + 430.f));
		B.Lamp(FVector(0, 0, Top + 440.f), OcSRGB(0x5A, 0xE0, 0xC8), 700.f, 900.f);
		// The stair down the well: steps round the inner walls to the Bell Chamber.
		for (int32 k = 0; k < 60; ++k)
		{
			const int32 Side = (k / 15) % 4;
			const float T = (k % 15) / 15.f;
			const FRotator R(0.f, Side * 90.f, 0.f);
			const FVector Sp = R.RotateVector(FVector(130.f, -150.f + T * 300.f, Mean + 200.f - k * 33.f));
			B.K.Add(OcLime(0.85f), FVector(90.f, 30.f, 24.f), Sp, R);
			B.Box(Sp, FVector(45.f, 15.f, 12.f), R);
		}
		// The O-S1 shutter: a weed-hung bronze shutter over the east window (the secret's spot opens it).
		B.K.Add(OcVerdigris(), FVector(10.f, 300.f, 190.f), FVector(228.f, 0, Mean - 220.f));
		B.K.Add(OcWeed(), FVector(6.f, 280.f, 120.f), FVector(234.f, 0, Mean - 250.f));
	}

	// ------------------------------------------------------------------------------------------------ shrines' settings

	void Shrine(FOcCtx& C)
	{
		// The settings round each shrine (the brazier itself is the shrine's own: WRWorldSites' Ocean look).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		if (C.A->Kind == TEXT("shrine_mirror_pool"))
		{
			// A round Saltkin tide pool 12 m across carved into a basin: a dressed rim of curved blocks, three steps down,
			// the water standing in it (the sea's: it fills on every tide), a stone post in the middle for the bowl.
			for (int32 s = 0; s < 32; ++s)
			{
				const FRotator R(0.f, s * 11.25f, 0.f);
				B.K.Add(OcLime(0.98f + 0.08f * B.Rand(s)), FVector(80.f, 125.f, 50.f), R.RotateVector(FVector(640.f, 0, G + 10.f)), R);
				B.K.Add(OcLime(0.85f), FVector(70.f, 110.f, 40.f), R.RotateVector(FVector(560.f, 0, G - 40.f)), R);
				B.Box(R.RotateVector(FVector(640.f, 0, G + 10.f)), FVector(40.f, 62.f, 25.f), R);
			}
			B.K.Add(FSurf::Glaze(OcSRGB(0x3E, 0x8C, 0x7A) * 0.45f, 0.05f), FVector(1100.f, 1100.f, 8.f), FVector(0, 0, G - 60.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			return;
		}
		if (C.A->Kind == TEXT("shrine_gate"))
		{
			// A ring of seven bronze bollards on the salt flat round where the brazier stands.
			for (int32 b = 0; b < 7; ++b) OcBollard(B, FRotator(0.f, b * 360.f / 7.f, 0.f).RotateVector(FVector(420.f, 0, 0)) + FVector(0, 0, B.G(0, 0, 0.f)), b * 51.f, 0.45f);
			return;
		}
		if (C.A->Kind == TEXT("shrine_driftwood"))
		{
			// Shell chimes on driftwood posts round the brazier, a ring of white stones.
			for (int32 p = 0; p < 5; ++p) OcChimeBeacon(B, FRotator(0.f, p * 72.f + 20.f, 0.f).RotateVector(FVector(380.f, 0, 0)) + FVector(0, 0, B.G(0, 0, 0.f)), p * 72.f, 3.f, p % 3);
			for (int32 s = 0; s < 16; ++s) B.K.Add(OcShell(), FVector(30.f, 24.f, 14.f), FRotator(0.f, s * 22.5f, 0.f).RotateVector(FVector(240.f, 0, G + 6.f)), FRotator(0.f, s * 22.5f, 0.f));
			return;
		}
		if (C.A->Kind == TEXT("shrine_kelp_stack"))
		{
			// A 14 m sea stack you climb out of the water to: ledges like steps up its weather side, a cleft at the top
			// for the brazier, gulls' white streaks down the faces.
			const FWROceanData::FPlace* Site = C.D.FindSite(TEXT("shrine_kelp_stack"));
			const float H = Site && Site->Stack > 0.f ? Site->Stack : 1400.f;
			const float Sea = C.SeaZ();
			const float Top = Sea + H;
			B.M.Lathe(FVector(0, 0, G - 100.f), FVector::UpVector, Top - G + 100.f, { FVector(0.f, 700.f, 640.f), FVector(0.6f, 520.f, 480.f), FVector(1.f, 380.f, 360.f) }, 14, OcSRGB(0xC9, 0xA4, 0x6A) * 0.5f, 0.9f, 0.f, 5.f);
			for (int32 b = 0; b < 4; ++b) B.Box(FVector(0, 0, (G + Top) * 0.5f), FVector(520.f, 260.f, (Top - G) * 0.5f), FRotator(0.f, b * 45.f, 0.f));
			for (int32 k = 0; k < 9; ++k)
			{
				const FVector Lp = FRotator(0.f, -40.f + k * 22.f, 0.f).RotateVector(FVector(620.f - k * 25.f, 0, Sea + 60.f + k * (H - 100.f) / 9.f));
				B.K.Add(OcLime(0.9f), FVector(150.f, 130.f, 40.f), Lp, FRotator(0.f, -40.f + k * 22.f, 0.f));
				B.Box(Lp, FVector(75.f, 65.f, 20.f), FRotator(0.f, -40.f + k * 22.f, 0.f));
			}
			B.K.Add(OcLime(0.95f), FVector(700.f, 700.f, 40.f), FVector(0, 0, Top), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			B.Box(FVector(0, 0, Top), FVector(350.f, 350.f, 20.f));
			for (int32 g = 0; g < 6; ++g) B.K.Add(FSurf::Stone(OcSRGB(0xF2, 0xEF, 0xE6) * 0.75f, 0.9f), FVector(10.f, 30.f, 300.f), FRotator(0.f, g * 60.f, 0.f).RotateVector(FVector(470.f, 0, Top - 300.f)), FRotator(0.f, g * 60.f, 0.f));
			B.Loop(TEXT("ocean.gull_colony"), FVector(0, 0, Top + 200.f));
			return;
		}
		// The lighthouse's shrine: its brazier stands in the broken lamp room (the lighthouse builds the room).
	}

	// ------------------------------------------------------------------------------------------------ camps

	void Camp(FOcCtx& C)
	{
		// A newcomers' camp (world-map: everyone came the same way): a curved windbreak of driftwood posts and sailcloth on
		// the seaward side, a fire ring of beach stones, the broken skiff they came in turned over for a shelter.
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		for (int32 p = 0; p < 9; ++p)
		{
			const float A = FMath::DegreesToRadians(-60.f + p * 15.f);
			const FVector Pp(FMath::Cos(A) * 700.f, FMath::Sin(A) * 700.f, 0.f);
			B.Limb(FVector(Pp.X, Pp.Y, B.G(Pp.X, Pp.Y, G) - 30.f), FVector(Pp.X, Pp.Y, B.G(Pp.X, Pp.Y, G) + 230.f), 9.f, 6.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 6);
			if (p < 8)
			{
				const float A2 = FMath::DegreesToRadians(-60.f + (p + 0.5f) * 15.f);
				B.K.Add(OcSail(), FVector(8.f, 190.f, 170.f), FVector(FMath::Cos(A2) * 700.f, FMath::Sin(A2) * 700.f, B.G(Pp.X, Pp.Y, G) + 130.f), FRotator(0.f, FMath::RadiansToDegrees(A2), 0.f));
			}
		}
		for (int32 s = 0; s < 10; ++s) B.K.Add(OcLime(0.7f), FVector(36.f, 30.f, 24.f), FRotator(0.f, s * 36.f, 0.f).RotateVector(FVector(110.f, 0, G + 8.f)), FRotator(0.f, s * 36.f + 10.f, 0.f));
		B.K.Add(OcEmber(), FVector(120.f, 120.f, 8.f), FVector(0, 0, G + 4.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Loop(TEXT("brazier.fire"), FVector(0, 0, G + 10.f));
		B.Lamp(FVector(0, 0, G + 120.f), OcSRGB(0xFF, 0xA8, 0x60), 2000.f, 1500.f);
		OcHull(B, FVector(-400.f, 500.f, B.G(-400.f, 500.f, G)), 30.f, 620.f, 175.f, 0.3f, OcTimber(0.7f));
	}

	void Runback(FOcCtx& C) {}

	void CaveMouth(FOcCtx& C)
	{
		// Siren's Hollow's shore mouth at the cliff foot: a sea-worn arch of limestone, weed to the tide's band, the drip of
		// the cave inside (the cave itself is built with the cave).
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		B.K.Ring(OcLime(0.85f), FVector(0, 0, G), 520.f, 180.f, 400.f, 11, 0.f, 180.f, 6.f);
		B.K.Add(OcWeed(), FVector(380.f, 1000.f, 120.f), FVector(-20.f, 0, C.SeaZ() + 20.f));
		B.Loop(TEXT("ocean.cave_drip"), FVector(-300.f, 0, G + 300.f));
	}

	// ------------------------------------------------------------------------------------------------ the caves

	void Cave(FOcCtx& C)
	{
		FBuild& B = C.B;
		const FWROceanData::FCave* Cv = C.D.Caves.IsValidIndex(C.A->Index) ? &C.D.Caves[C.A->Index] : nullptr;
		if (!Cv) return;
		// The actor stands at the cave's first point (floor level), not turned (the rooms' offsets are world-aligned).
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const FLinearColor Rock = OcSRGB(0xC9, 0xA4, 0x6A) * 0.36f, RockWet = OcSRGB(0x5A, 0x50, 0x40) * 0.4f;
		const bool bMound = Cv->Id == TEXT("pearl_cave") || Cv->Id == TEXT("sirens_hollow");
		for (int32 i = 0; i < Cv->Rooms.Num(); ++i)
		{
			const FVector R = Cv->Rooms[i];
			const FVector Centre(R.X, R.Y, 0.f);
			const float Rad = R.Z, H = Cv->Roof;
			OcDomeInward(B, Centre, Rad, H, 22, (Cv->bDry ? Rock : RockWet), 0.18f);
			OcRoomCollision(B, Centre, Rad, H);
			// The floor: a rough disc of sand and broken limestone.
			B.M.Lathe(Centre - FVector(0, 0, 40.f), FVector::UpVector, 40.f, { FVector(0.f, Rad * 1.02f, Rad * 1.02f), FVector(1.f, Rad, Rad) }, 22, Cv->bDry ? Rock * 1.1f : OcSRGB(0xD8, 0xCD, 0xB0) * 0.4f, 0.95f);
			B.Box(Centre - FVector(0, 0, 40.f), FVector(Rad, Rad, 40.f));
			// Stalactites over the room (dripping), fallen blocks on its floor.
			for (int32 s = 0; s < int32(Rad / 150.f); ++s)
			{
				const float A = B.Rand(i * 50 + s) * 2.f * PI, D = Rad * 0.8f * FMath::Sqrt(B.Rand(i * 50 + s + 7));
				const FVector Up = Centre + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, H * FMath::Cos(D / Rad * 0.5f * PI) - 20.f);
				B.K.Spike(FSurf::Stone(Rock * 1.2f, 0.4f), Up, FVector(0, 0, -1), 40.f + 30.f * B.Rand(s), 120.f + 200.f * B.Rand(s + 3));
			}
			for (int32 f = 0; f < 4; ++f) B.Rock(f, FVector(160.f, 140.f, 100.f) * (0.6f + B.Rand(f + i * 9)), Centre + FVector((B.Rand(f * 3 + i) - 0.5f) * Rad, (B.Rand(f * 5 + i) - 0.5f) * Rad, 0.f), FRotator(0.f, f * 70.f, 0.f), OcRockTint() * 0.6f, true);
			// Light: phosphor in the water (sea caves) or nothing at all (the mine: bring a lamp).
			if (!Cv->bDry) B.Lamp(Centre + FVector(0, 0, H * 0.4f), OcSRGB(0x5A, 0xC8, 0xD0), 600.f, Rad * 1.5f);
			// The passage to the next room: a short tube (inward) and its collision.
			if (i + 1 < Cv->Rooms.Num())
			{
				const FVector N(Cv->Rooms[i + 1].X, Cv->Rooms[i + 1].Y, 0.f);
				const FVector D = (N - Centre).GetSafeNormal2D();
				const FVector From = Centre + D * (Rad * 0.8f), To = N - D * (Cv->Rooms[i + 1].Z * 0.8f);
				const float W = 260.f;
				const FVector Side(-D.Y, D.X, 0.f);
				for (int32 sd = -1; sd <= 1; sd += 2) B.SegBox(From + Side * sd * (W + 60.f) + FVector(0, 0, 200.f), To + Side * sd * (W + 60.f) + FVector(0, 0, 200.f), 60.f, 260.f);
				B.SegBox(From + FVector(0, 0, 480.f), To + FVector(0, 0, 480.f), W + 100.f, 50.f);
				B.SegBox(From - FVector(0, 0, 40.f), To - FVector(0, 0, 40.f), W + 40.f, 40.f);
				for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(FSurf::Stone(Rock, 0.9f), FVector(FVector::Dist(From, To), 80.f, 520.f), (From + To) * 0.5f + Side * sd * (W + 20.f) + FVector(0, 0, 200.f), D.Rotation());
				B.K.Add(FSurf::Stone(Rock * 0.9f, 0.9f), FVector(FVector::Dist(From, To), W * 2.f + 120.f, 60.f), (From + To) * 0.5f + FVector(0, 0, 460.f), D.Rotation());
			}
		}
		// The way in.
		const FVector Mouth = Cv->MouthOffset;
		const FRotator My(0.f, Cv->MouthYaw, 0.f);
		if (Cv->Id == TEXT("saltkin_mine"))
		{
			// An adit into the Spine's face: timber sets (posts and caps) every 2 m down a 30 m drift to the first room,
			// cart rails on sleepers, a tide-tablet on the wall at the first room (story: the Saltkin mined here).
			const FVector Dir(-1.f, 0.f, 0.f);
			for (int32 s = 0; s < 15; ++s)
			{
				const FVector P = Mouth + Dir * (s * 200.f);
				for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(OcTimber(0.7f), FVector(24.f, 24.f, 300.f), P + FVector(0, sd * 230.f, 150.f));
				B.K.Add(OcTimber(0.65f), FVector(30.f, 500.f, 30.f), P + FVector(0, 0, 310.f));
				B.K.Add(OcTimber(0.6f), FVector(30.f, 160.f, 12.f), P + FVector(0, 0, 6.f));
			}
			for (int32 sd = -1; sd <= 1; sd += 2)
			{
				B.K.Rod(OcRust(), Mouth + FVector(300.f, sd * 50.f, 14.f), Mouth + Dir * 3200.f + FVector(0, sd * 50.f, 14.f), 5.f, true);
				B.SegBox(Mouth + FVector(100.f, sd * 300.f, 160.f), Mouth + Dir * 3100.f + FVector(0, sd * 300.f, 160.f), 60.f, 220.f);
			}
			B.SegBox(Mouth + FVector(100.f, 0, 380.f), Mouth + Dir * 3100.f + FVector(0, 0, 380.f), 360.f, 50.f);
			B.SegBox(Mouth + FVector(300.f, 0, -30.f), Mouth + Dir * 3100.f + FVector(0, 0, -30.f), 300.f, 30.f);
			for (int32 sd = -1; sd <= 1; sd += 2) B.K.Add(FSurf::Stone(Rock, 0.9f), FVector(3200.f, 60.f, 420.f), Mouth + Dir * 1500.f + FVector(0, sd * 280.f, 190.f));
			B.K.Add(FSurf::Stone(Rock * 0.9f, 0.9f), FVector(3200.f, 640.f, 60.f), Mouth + Dir * 1500.f + FVector(0, 0, 400.f));
			// The portal's dressed stone collar (covers the ground's hole round the adit).
			B.K.Ring(OcLime(0.9f), Mouth + FVector(60.f, 0, 0), 300.f, 120.f, 200.f, 9, 0.f, 180.f, 4.f);
			B.K.Add(OcClay(), FVector(10.f, 80.f, 110.f), Mouth + Dir * 3000.f + FVector(0, -270.f, 180.f));   // the tide-tablet
			return;
		}
		if (bMound)
		{
			// The rock outcrop round the rooms (the Pearl Cave's reef mound, Siren's Hollow's headland), its mouth an arch.
			const FVector R0(Cv->Rooms[0].X, Cv->Rooms[0].Y, 0.f);
			OcMoundOutward(B, R0 - FVector(0, 0, 300.f), Cv->Rooms[0].Z + 600.f, Cv->Roof + 900.f, OcSRGB(0xC9, 0xA4, 0x6A) * 0.45f, C.A->Index * 17);
			for (int32 k = 1; k < Cv->Rooms.Num(); ++k) OcMoundOutward(B, FVector(Cv->Rooms[k].X, Cv->Rooms[k].Y, -300.f), Cv->Rooms[k].Z + 500.f, Cv->Roof + 600.f, OcSRGB(0xC9, 0xA4, 0x6A) * 0.45f, C.A->Index * 17 + k);
			B.K.Ring(OcLime(0.8f), Mouth + FVector(0, 0, 0), Cv->MouthW * 0.5f, 160.f, 300.f, 9, 0.f, 180.f, 6.f);
			// Coral and weed on the outcrop's flanks (it's in the sea).
			for (int32 k = 0; k < 18; ++k)
			{
				const float A = B.Rand(k * 3 + 1) * 2.f * PI;
				OcCoralHead(B, R0 + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (Cv->Rooms[0].Z + 650.f) + FVector(0, 0, 100.f + 600.f * B.Rand(k)), 0.6f + B.Rand(k + 2), k % 6, k + C.A->Index);
			}
			if (Cv->Id == TEXT("sirens_hollow"))
			{
				// The arena: five rock islands in the lake, a ring ledge round its edge 4 m up, the stalactites she brings
				// down (the boss's), phosphor in the water.
				for (const FVector& I : Cv->Islands)
				{
					const FVector P(I.X * 100.f, I.Y * 100.f, 0.f);
					B.M.Lathe(P - FVector(0, 0, 200.f), FVector::UpVector, -Cv->At.Z + C.SeaZ() + 350.f, { FVector(0.f, I.Z * 120.f, I.Z * 110.f), FVector(0.85f, I.Z * 100.f, I.Z * 90.f), FVector(1.f, I.Z * 60.f, I.Z * 55.f) }, 12, OcSRGB(0x5A, 0x50, 0x40) * 0.45f, 0.5f);
					B.Box(P + FVector(0, 0, (-Cv->At.Z + C.SeaZ() + 150.f) * 0.5f), FVector(I.Z * 80.f, I.Z * 80.f, (-Cv->At.Z + C.SeaZ() + 150.f) * 0.5f));
				}
				const float LedgeZ = C.SeaZ() + 400.f;
				for (int32 s = 0; s < 40; ++s)
				{
					const FRotator R(0.f, s * 9.f, 0.f);
					B.K.Add(OcLime(0.7f), FVector(300.f, 480.f, 60.f), R.RotateVector(FVector(Cv->Rooms[0].Z - 260.f, 0, LedgeZ)), R);
					B.Box(R.RotateVector(FVector(Cv->Rooms[0].Z - 260.f, 0, LedgeZ)), FVector(150.f, 240.f, 30.f), R);
				}
				B.Lamp(FVector(0, 0, C.SeaZ() + 100.f), OcSRGB(0x5A, 0xE0, 0xC8), 2400.f, 4000.f);
				B.Loop(TEXT("ocean.cave_drip"), FVector(0, 0, LedgeZ + 600.f), FRotator::ZeroRotator, 2.f);
			}
			return;
		}
		// The grotto's sea mouth (an arch in the shelf edge under the water's reach) and Lazarus Deep's in the Blue Wall.
		B.K.Ring(OcLime(0.7f), Mouth, Cv->MouthW * 0.5f, 140.f, 260.f, 9, 0.f, 180.f, 6.f);
		for (int32 sd = -1; sd <= 1; sd += 2) B.SegBox(Mouth + FVector(0, sd * (Cv->MouthW * 0.5f + 40.f), 200.f), FVector(Cv->Rooms[0].X * 100.f, Cv->Rooms[0].Y * 100.f, 0.f) + FVector(0, sd * (Cv->MouthW * 0.5f + 40.f), 200.f), 50.f, 300.f);
		if (Cv->Id == TEXT("lazarus_deep"))
		{
			// The black pool at the back (the Lazarus Fish), and the side pocket at 40 m a Pearldiver finds (O-S5).
			const FVector Back(Cv->Rooms.Last().X, Cv->Rooms.Last().Y, 0.f);
			B.K.Add(FSurf::Glaze(OcSRGB(0x06, 0x0A, 0x10), 0.02f), FVector(700.f, 700.f, 10.f), Back + FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			const FVector Side(Cv->Side.X, Cv->Side.Y, Cv->Side.Z - Cv->At.Z);
			OcDomeInward(B, Side, 300.f, 260.f, 12, RockWet, 0.2f);
			B.K.Add(OcRust(), FVector(60.f, 50.f, 70.f), Side + FVector(0, 0, 35.f));     // the diver's last cache's crate
		}
	}

	// ------------------------------------------------------------------------------------------------ the Drowned Hall

	void Hall(FOcCtx& C)
	{
		// The Saltkin city's great hall under the reef crest (cook 2.5): its rooms in rock and coral cement, the Air Dome a
		// 40 m dome holding a lake of trapped air over waist-deep water, the dry rooms (the Bell Chamber, the King's
		// Tower, the Archive), the flooded ones (the Sluice Works' gears, the Pearl Beds, the Warren), the Low Gate Hall's
		// ramp. Long tables, fallen shell chandeliers, the dais and its empty throne; the king on his bed; the woman at the
		// lever with the key still in the lock and a Sunwatch sun-disc on her wrist.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const FVector O = C.A->GetActorLocation();
		const FLinearColor Coral = OcSRGB(0xEA, 0xDF, 0xC8) * 0.4f, Lime = OcSRGB(0xC9, 0xA4, 0x6A) * 0.42f;
		for (const FWROceanData::FHallRoom& R : C.D.HallRooms)
		{
			const FVector P = R.At - O;
			const float H = R.bDome ? (R.AirTop - R.At.Z) + 200.f : 600.f;
			OcDomeInward(B, P, R.R, H, R.bDome ? 32 : 18, R.bDome ? Coral : Lime, R.bDome ? 0.1f : 0.16f);
			OcRoomCollision(B, P, R.R, H);
			B.M.Lathe(P - FVector(0, 0, 40.f), FVector::UpVector, 40.f, { FVector(0.f, R.R, R.R), FVector(1.f, R.R, R.R) }, R.bDome ? 32 : 18, OcSRGB(0xB8, 0xA8, 0x88) * 0.4f, 0.8f);
			B.Box(P - FVector(0, 0, 40.f), FVector(R.R, R.R, 40.f));
			// The flagged floor's joints (a grid of darker lines).
			for (int32 g = -int32(R.R / 200.f); g <= int32(R.R / 200.f); ++g) B.K.Add(FSurf::Stone(Lime * 0.7f, 0.9f), FVector(4.f, R.R * 1.6f, 2.f), P + FVector(g * 200.f, 0, 2.f));
			if (R.Id == TEXT("air_dome"))
			{
				// Long tables in two rows, benches, fallen shell chandeliers, the raised dais and the empty throne; glowray
				// light through gaps in the coral (shafts) and on the trapped air's lake.
				for (int32 t = -1; t <= 1; t += 2)
				{
					B.K.Add(OcTimber(0.45f), FVector(1600.f, 140.f, 16.f), P + FVector(-200.f, t * 500.f, 80.f));
					for (int32 l = 0; l < 6; ++l) B.K.Add(OcTimber(0.4f), FVector(14.f, 14.f, 80.f), P + FVector(-900.f + l * 300.f, t * 500.f + 50.f, 40.f));
					for (int32 s = -1; s <= 1; s += 2) B.K.Add(OcTimber(0.42f), FVector(1600.f, 40.f, 8.f), P + FVector(-200.f, t * 500.f + s * 120.f, 46.f));
				}
				for (int32 ch = 0; ch < 3; ++ch)
				{
					const FVector Cp = P + FVector(-700.f + ch * 600.f, (ch - 1) * 200.f, 20.f);
					B.K.Disc(OcVerdigris(), Cp, FVector(0.2f, 0.1f, 1.f), 260.f, 12.f);
					for (int32 s = 0; s < 14; ++s) B.K.Add(s % 2 ? OcShell() : OcShellPink(), FVector(18.f, 14.f, 6.f), Cp + FRotator(0.f, s * 25.7f, 0.f).RotateVector(FVector(110.f + 30.f * B.Rand(s), 0, 6.f)), FRotator(0.f, s * 25.7f, 0.f));
				}
				B.K.Add(OcLime(1.0f), FVector(600.f, 900.f, 90.f), P + FVector(1300.f, 0, 45.f));
				B.Box(P + FVector(1300.f, 0, 45.f), FVector(300.f, 450.f, 45.f));
				for (int32 st = 0; st < 3; ++st) B.K.Add(OcLime(0.95f), FVector(60.f, 900.f, 30.f * (st + 1)), P + FVector(970.f + st * 30.f, 0, 15.f * (st + 1)));
				B.K.Add(OcLime(1.1f), FVector(120.f, 160.f, 90.f), P + FVector(1440.f, 0, 135.f));
				B.K.Add(OcLime(1.1f), FVector(40.f, 160.f, 260.f), P + FVector(1500.f, 0, 220.f));
				B.K.Add(OcVerdigris(), FVector(30.f, 30.f, 120.f), P + FVector(1440.f, -80.f, 160.f));
				B.K.Add(OcVerdigris(), FVector(30.f, 30.f, 120.f), P + FVector(1440.f, 80.f, 160.f));
				// Glowray light: soft blue shafts from gaps in the dome's coral, a light over the air's lake.
				for (int32 g = 0; g < 5; ++g)
				{
					const FVector Gp = P + FRotator(0.f, g * 72.f, 0.f).RotateVector(FVector(R.R * 0.5f, 0, H * 0.9f));
					B.K.Add(FSurf::Lit(OcSRGB(0x5A, 0xC8, 0xE0), 1.2f), FVector(80.f, 80.f, 6.f), Gp, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					B.Lamp(Gp - FVector(0, 0, 300.f), OcSRGB(0x5A, 0xC8, 0xE0), 900.f, 1600.f);
				}
				// The waist-deep water's surface inside the dome (the air above it is the pocket's).
				B.K.Add(FSurf::Glaze(OcSRGB(0x1E, 0x6E, 0x80) * 0.4f, 0.05f), FVector(R.R * 1.9f, R.R * 1.9f, 4.f), FVector(P.X, P.Y, R.WaterTop - O.Z), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
			}
			else if (R.Id == TEXT("bell_chamber"))
			{
				// The bronze bell (3 m) in its headstock, its clapper bound in rope; it swings on every machine when rung.
				if (C.A->GetNetMode() != NM_DedicatedServer)
				{
					USceneComponent* Head = NewObject<USceneComponent>(C.A);
					Head->SetupAttachment(C.A->GetRootComponent());
					Head->SetRelativeLocation(P + FVector(0, 0, 520.f));
					Head->RegisterComponent();
					TArray<FWRLookBlock> Bell;
					FKit BK(FTransform::Identity, Bell);
					BK.Add(OcTimber(0.5f), FVector(80.f, 400.f, 60.f), FVector(0, 0, 30.f));
					BK.Add(OcVerdigris(), FVector(260.f, 260.f, 300.f), FVector(0, 0, -150.f), FRotator::ZeroRotator, EWRLookMesh::Cone);
					BK.Add(OcVerdigris(), FVector(300.f, 300.f, 40.f), FVector(0, 0, -300.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					BK.Add(OcRust(), FVector(30.f, 30.f, 220.f), FVector(0, 0, -200.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					BK.Add(OcNet(), FVector(46.f, 46.f, 60.f), FVector(0, 0, -300.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					FWRBuildLook::Spawn(C.A, Head, Bell, FLinearColor::Black, false);
					C.A->Bell = Head;
				}
				for (int32 s = -1; s <= 1; s += 2) B.K.Add(OcTimber(0.5f), FVector(40.f, 40.f, 560.f), P + FVector(0, s * 220.f, 280.f));
				B.K.Rod(OcNet(), P + FVector(0, 0, 220.f), P + FVector(80.f, 40.f, 40.f), 3.f);
			}
			else if (R.Id == TEXT("kings_tower"))
			{
				// A bed, a skeleton on it in a gold circlet, one hand at its own throat where a neck-chain is broken.
				B.K.Add(OcTimber(0.5f), FVector(220.f, 140.f, 50.f), P + FVector(0, 0, 25.f));
				B.K.Add(OcSail(), FVector(200.f, 130.f, 10.f), P + FVector(0, 0, 55.f));
				for (int32 r = 0; r < 6; ++r) B.K.Add(OcBone(), FVector(8.f, 50.f, 6.f), P + FVector(-40.f + r * 14.f, 0, 64.f));
				B.Ball(P + FVector(80.f, 0, 70.f), 13.f, OcSRGB(0xE8, 0xE0, 0xCC) * 0.7f, 0.7f);
				B.K.Disc(OcGold(), P + FVector(80.f, 0, 80.f), FVector::UpVector, 30.f, 4.f);
				B.K.Rod(OcBone(), P + FVector(20.f, 30.f, 62.f), P + FVector(55.f, 6.f, 68.f), 4.f);
				B.K.Rod(OcGold(), P + FVector(58.f, -4.f, 66.f), P + FVector(48.f, -30.f, 60.f), 1.5f);   // the broken chain
				// The empty key-hook beside the bed (its twin is over the dyke door).
				B.K.Rod(OcRust(), P + FVector(-R.R + 60.f, 0, 260.f), P + FVector(-R.R + 110.f, 0, 260.f), 6.f);
				B.K.Rod(OcRust(), P + FVector(-R.R + 110.f, 0, 260.f), P + FVector(-R.R + 110.f, 0, 200.f), 6.f);
			}
			else if (R.Id == TEXT("sluice_works"))
			{
				// A flooded machine hall: bronze gear wheels meshed on their shafts (a pinion on the lever's shaft driving
				// the great wheel, whose shaft runs to the sluice chain drum), the chains leading up into the dark, the great
				// lever standing open, the key in its lock, the woman at the lever.
				const FVector Wheel = P + FVector(-200.f, 300.f, 300.f);
				B.K.Disc(OcVerdigris(), Wheel, FVector(1, 0, 0), 520.f, 40.f);
				for (int32 t = 0; t < 24; ++t) B.K.Add(OcVerdigris(), FVector(40.f, 30.f, 50.f), Wheel + FRotator(t * 15.f, 0.f, 0.f).RotateVector(FVector(0, 0, 275.f)), FRotator(t * 15.f, 0.f, 0.f));
				const FVector Pinion = Wheel + FVector(0, -360.f, -100.f);
				B.K.Disc(OcVerdigris(), Pinion, FVector(1, 0, 0), 180.f, 40.f);
				for (int32 t = 0; t < 10; ++t) B.K.Add(OcVerdigris(), FVector(40.f, 26.f, 40.f), Pinion + FRotator(t * 36.f, 0.f, 0.f).RotateVector(FVector(0, 0, 100.f)), FRotator(t * 36.f, 0.f, 0.f));
				B.K.Rod(OcBronze(), Wheel + FVector(-300.f, 0, 0), Wheel + FVector(300.f, 0, 0), 40.f);
				B.K.Rod(OcBronze(), Pinion + FVector(-300.f, 0, 0), Pinion + FVector(300.f, 0, 0), 30.f);
				const FVector Drum = Wheel + FVector(300.f, 0, 0);
				B.K.Disc(OcRust(), Drum, FVector(1, 0, 0), 160.f, 120.f);
				OcChain(B, Drum + FVector(0, 0, 80.f), Drum + FVector(0, 200.f, 600.f), 40.f, 0.f, OcRust());
				// The lever: from the pinion's shaft out and up, standing open; the lock plate with the key in it.
				const FVector Lever = Pinion + FVector(300.f, 0, 0);
				B.K.Rod(OcBronze(), Lever, Lever + FVector(0, 200.f, 380.f), 22.f);
				B.K.Add(OcIron(), FVector(30.f, 60.f, 60.f), Lever + FVector(0, 60.f, 120.f));
				B.K.Rod(OcRust(), Lever + FVector(-20.f, 60.f, 120.f), Lever + FVector(-70.f, 60.f, 120.f), 5.f);   // the key
				// The woman at the lever: a seated skeleton in Saltkin pearls, a gold sun-disc bracelet.
				const FVector Wm = Lever + FVector(0, -100.f, 0.f);
				B.Ball(Wm + FVector(0, 0, 110.f), 13.f, OcSRGB(0xE8, 0xE0, 0xCC) * 0.7f, 0.7f);
				for (int32 r = 0; r < 5; ++r) B.K.Add(OcBone(), FVector(40.f, 8.f, 6.f), Wm + FVector(0, 0, 50.f + r * 10.f));
				for (int32 pl = 0; pl < 9; ++pl) B.Ball(Wm + FVector(FMath::Cos(pl * 0.7f) * 12.f, FMath::Sin(pl * 0.7f) * 12.f, 92.f), 2.5f, OcSRGB(0xF4, 0xF0, 0xE6) * 0.8f, 0.2f);
				B.K.Rod(OcBone(), Wm + FVector(0, 0, 80.f), Lever + FVector(-10.f, 40.f, 100.f), 4.f);
				B.K.Disc(OcGold(), Lever + FVector(-10.f, 30.f, 96.f), FVector(1, 0, 0), 12.f, 3.f);
				B.Box(Wheel, FVector(30.f, 260.f, 260.f));
			}
			else if (R.Id == TEXT("pearl_beds"))
			{
				for (int32 k = 0; k < 8; ++k) OcClam(B, P + FRotator(0.f, k * 45.f, 0.f).RotateVector(FVector(700.f, 0, 0)), k * 45.f + 90.f, 1.6f);
			}
			else if (R.Id == TEXT("tidecoil_warren"))
			{
				// Collapsed houses full of eel holes: wall stubs, fallen roof slabs, dark holes in every course.
				for (int32 h = 0; h < 4; ++h) OcSaltkinHouse(B, P + FRotator(0.f, h * 90.f, 0.f).RotateVector(FVector(600.f, 0, 0)), h * 90.f, false, h);
				for (int32 hole = 0; hole < 14; ++hole) B.K.Disc(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.012f), 0.9f), P + FRotator(0.f, hole * 25.7f, 0.f).RotateVector(FVector(R.R - 40.f, 0, 60.f + 90.f * (hole % 3))), FRotator(0.f, hole * 25.7f, 0.f).Vector(), 60.f, 6.f);
			}
			else if (R.Id == TEXT("archive"))
			{
				// Shelves of clay tide-tablets, harpoon heads in rows with date-notch tags.
				for (int32 sh = 0; sh < 3; ++sh)
				{
					B.K.Add(OcTimber(0.45f), FVector(30.f, 400.f, 220.f), P + FVector(-R.R + 60.f + sh * 120.f, 0, 110.f));
					for (int32 t = 0; t < 12; ++t) B.K.Add(OcClay(), FVector(26.f, 24.f, 30.f), P + FVector(-R.R + 80.f + sh * 120.f, -170.f + t * 30.f, 60.f + (t % 3) * 60.f));
				}
				for (int32 h = 0; h < 10; ++h) B.K.Spike(OcRust(), P + FVector(200.f, -200.f + h * 40.f, 120.f), FVector(1, 0, 0), 12.f, 34.f);
			}
			else if (R.Id == TEXT("low_gate_hall"))
			{
				// A paved ramp up to the Low Gate; the sand that seals it (slumps in the King Tide's Low: Sand).
				for (int32 k = 0; k < 12; ++k) B.K.Add(OcLime(0.9f), FVector(100.f, 500.f, 40.f), P + FVector(-k * 100.f, 0, k * 30.f));
				B.SegBox(P, P + FVector(-1200.f, 0, 360.f), 250.f, 20.f);
				if (C.A->GetNetMode() != NM_DedicatedServer)
				{
					USceneComponent* Sand = NewObject<USceneComponent>(C.A);
					Sand->SetupAttachment(C.A->GetRootComponent());
					Sand->SetRelativeLocation(P + FVector(-1300.f, 0, 360.f));
					Sand->RegisterComponent();
					TArray<FWRLookBlock> SandBlocks;
					FKit SK(FTransform::Identity, SandBlocks);
					SK.Add(FSurf::Stone(OcSRGB(0xD8, 0xCD, 0xB0) * 0.55f, 0.95f), FVector(300.f, 600.f, 500.f), FVector(0, 0, 250.f), FRotator(0.f, 0.f, 0.f));
					FWRBuildLook::Spawn(C.A, Sand, SandBlocks, FLinearColor::Black, false);
					C.A->Sand = Sand;
				}
			}
		}
		// The coral mound over the dome on the reef flat (its top 2 m under at low tide) and the gaps glowray light comes
		// through; collision over it (it's the reef there).
		const FWROceanData::FHallRoom* Dome = C.D.FindHallRoom(TEXT("air_dome"));
		if (Dome)
		{
			const FVector P = Dome->At - O;
			OcMoundOutward(B, FVector(P.X, P.Y, Dome->AirTop - O.Z - 300.f), Dome->R + 300.f, 600.f, OcSRGB(0xEA, 0xDF, 0xC8) * 0.5f, 77);
			for (int32 k = 0; k < 24; ++k) OcCoralHead(B, P + FRotator(0.f, k * 15.f, 0.f).RotateVector(FVector((Dome->R + 200.f) * B.Rand(k), 0, Dome->AirTop - O.Z + 250.f)), 0.8f + B.Rand(k + 1), k % 6, k);
		}
	}

	void TsunamiStone(FOcCtx& C)
	{
		OcTsunamiStone(C.B, FVector(0, 0, C.B.G(0, 0, 0.f)), 0.f, false);
	}

	// ------------------------------------------------------------------------------------------------ decoration cells

	void Decor(FOcCtx& C)
	{
		// One 250 m cell of the decoration rule (build plan 2.2): its sea-grass, driftwood, tamarisk, walls, rocks,
		// barnacle bands, kelp, coral, clams, urchins and mangroves. The actor isn't turned.
		FBuild& B = C.B;
		C.A->SetActorRotation(FRotator::ZeroRotator);
		const float Cell = C.A->Size;
		const FVector O = C.A->GetActorLocation();
		const auto In = [&](const FVector& W) { return FMath::Abs(W.X - O.X) <= Cell * 0.5f && FMath::Abs(W.Y - O.Y) <= Cell * 0.5f; };
		const auto Lp = [&](const FVector& W) { return W - O; };
		int32 n = 0;
		for (const auto& D : C.D.Grass)
		{
			if (!In(D.At)) continue;
			// A tuft of marram: 6-9 blades leaning with the wind off the sea.
			const FVector P = Lp(D.At);
			for (int32 b = 0; b < 7; ++b) B.K.Spike(FSurf::Cloth(OcSRGB(0x8F, 0xA6, 0x5A) * (0.42f + 0.1f * B.Rand(n * 7 + b))), P + FVector((b - 3) * 4.f, (b % 2) * 4.f, -2.f), FVector(-0.25f + 0.1f * (b - 3), 0.08f * (b % 3), 1.f), 3.f, 50.f * D.Scale + 12.f * B.Rand(b + n));
			++n;
		}
		for (const auto& D : C.D.Driftwood)
		{
			if (!In(D.At)) continue;
			// A silver log at the drift line (where the high tide leaves it), roots at one end.
			const FVector P = Lp(D.At);
			const FVector Dir = FRotator(0.f, D.Yaw, 0.f).Vector();
			B.Limb(P - Dir * 180.f * D.Scale + FVector(0, 0, 12.f), P + Dir * 180.f * D.Scale + FVector(0, 0, 10.f), 18.f * D.Scale, 12.f * D.Scale, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 7);
			for (int32 r = 0; r < 4; ++r) B.Limb(P - Dir * 180.f * D.Scale + FVector(0, 0, 14.f), P - Dir * (220.f + 40.f * B.Rand(r)) * D.Scale + FRotator(r * 40.f - 60.f, D.Yaw + 90.f, 0.f).Vector() * 60.f, 8.f, 3.f, OcSRGB(0x98, 0x93, 0x8A) * 0.5f, 5);
			B.SegBox(P - Dir * 170.f * D.Scale + FVector(0, 0, 14.f), P + Dir * 170.f * D.Scale + FVector(0, 0, 12.f), 16.f * D.Scale, 14.f * D.Scale);
		}
		for (const auto& D : C.D.Tamarisk)
		{
			if (!In(D.At)) continue;
			// A tamarisk: a few grey-brown stems from a knot, feathery grey-green sprays (the salt-tolerant scrub).
			const FVector P = Lp(D.At);
			for (int32 s = 0; s < 4; ++s)
			{
				const FVector Tip = P + FRotator(70.f - 12.f * s, D.Yaw + s * 90.f, 0.f).Vector() * 160.f * D.Scale + FVector(0, 0, 60.f * D.Scale);
				B.Limb(P, Tip, 5.f * D.Scale, 2.f * D.Scale, OcSRGB(0x6E, 0x5E, 0x4E) * 0.5f, 5);
				B.Ball(Tip, 55.f * D.Scale, OcSRGB(0x8C, 0x9A, 0x7A) * 0.45f, 0.95f, 0.7f);
			}
		}
		for (const auto& W : C.D.Walls)
		{
			if (!In(W.A)) continue;
			// A dry-stone terrace wall along a bench edge (knee high, stones laid without mortar).
			OcCoursedWall(B, Lp(W.A), Lp(W.B), 70.f, 60.f, 50.f, 22.f, 0.85f, true);
		}
		int32 r = 0;
		for (const auto& D : C.D.Rocks)
		{
			if (!In(D.At)) continue;
			B.Rock(r++, FVector(160.f, 140.f, 110.f) * D.Scale, Lp(D.At), FRotator(0.f, D.Yaw, 0.f), OcSRGB(0x8E, 0x8A, 0x82), D.Scale > 1.6f);
		}
		for (const auto& D : C.D.Barnacles)
		{
			if (!In(D.At)) continue;
			// A barnacle-and-weed crust on the intertidal rock: a low cream patch with a weed fringe.
			const FVector P = Lp(D.At);
			B.K.Add(OcBarnacle(), FVector(140.f, 100.f, 12.f) * D.Scale, P + FVector(0, 0, 4.f), FRotator(0.f, D.Yaw, 0.f), EWRLookMesh::Cylinder);
			B.K.Add(OcWeed(), FVector(100.f, 70.f, 8.f) * D.Scale, P + FVector(40.f, 20.f, 2.f), FRotator(0.f, D.Yaw + 40.f, 0.f), EWRLookMesh::Cylinder);
		}
		for (const auto& D : C.D.Kelp) if (In(D.At)) OcKelpStipe(B, Lp(D.At), D.Scale * 100.f, D.Yaw, n++);
		for (const auto& D : C.D.Coral) if (In(D.At)) OcCoralHead(B, Lp(D.At), D.Scale, D.Variant, n++);
		for (const auto& D : C.D.Clams) if (In(D.At)) OcClam(B, Lp(D.At), D.Yaw, D.Scale);
		for (const auto& D : C.D.Urchins)
		{
			if (!In(D.At)) continue;
			// A cluster of purple urchins (the barren's grazers): spined balls on the rock.
			const FVector P = Lp(D.At);
			for (int32 u = 0; u < 5; ++u)
			{
				const FVector Up = P + FVector((B.Rand(u + n) - 0.5f) * 90.f, (B.Rand(u * 3 + n) - 0.5f) * 90.f, 6.f);
				B.Ball(Up, 7.f, OcUrchin(), 0.6f);
				for (int32 s = 0; s < 6; ++s) B.K.Spike(FSurf::Stone(OcUrchin() * 1.2f, 0.5f), Up, FRotator(30.f + s * 10.f, s * 60.f, 0.f).Vector(), 2.f, 12.f);
			}
			++n;
		}
		for (const auto& D : C.D.Mangroves)
		{
			if (!In(D.At)) continue;
			// A red mangrove where the Swamp's estuary creeps in: arching prop roots into the mud, a dense crown.
			const FVector P = Lp(D.At);
			const float S = D.Scale;
			for (int32 pr = 0; pr < 7; ++pr)
			{
				const float A = pr / 7.f * 2.f * PI + D.Yaw;
				const FVector Foot = P + FVector(FMath::Cos(A) * 220.f * S, FMath::Sin(A) * 220.f * S, -20.f);
				B.Root({ Foot, Foot + FVector(-FMath::Cos(A) * 80.f, -FMath::Sin(A) * 80.f, 160.f) * S, P + FVector(0, 0, 240.f * S) }, 7.f * S, 9.f * S, OcSRGB(0x5E, 0x4A, 0x3A) * 0.5f, 6);
			}
			B.Limb(P + FVector(0, 0, 230.f * S), P + FVector(0, 0, 420.f * S), 14.f * S, 9.f * S, OcSRGB(0x5E, 0x4A, 0x3A) * 0.5f, 7);
			B.Ball(P + FVector(0, 0, 520.f * S), 220.f * S, OcSRGB(0x3E, 0x5E, 0x2E) * 0.45f, 0.9f, 0.6f);
			B.Box(P + FVector(0, 0, 250.f * S), FVector(25.f, 25.f, 250.f) * S);
		}
	}

	// ------------------------------------------------------------------------------------------------ Hae-won's house

	void HaewonHouse(FOcCtx& C)
	{
		// A stilt house over the Mirror Pool's edge (cook 2.11): a plank floor 2 m up on driftwood stilts braced in X, a
		// porch on the pool side with her lamp, a sailcloth-and-shell-tile roof, nets and lines drying on a rail, a ladder.
		FBuild& B = C.B;
		const float G = B.G(0, 0, 0.f);
		const float Floor = G + 220.f;
		for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sy = -1; sy <= 1; sy += 2)
		{
			const FVector Foot(sx * 260.f, sy * 220.f, B.G(sx * 260.f, sy * 220.f, G) - 40.f);
			B.Limb(Foot, FVector(Foot.X, Foot.Y, Floor), 12.f, 10.f, OcSRGB(0xA8, 0xA3, 0x9A) * 0.5f, 7);
			B.Box(FVector(Foot.X, Foot.Y, (Foot.Z + Floor) * 0.5f), FVector(12.f, 12.f, (Floor - Foot.Z) * 0.5f));
		}
		for (int32 s = -1; s <= 1; s += 2) { B.K.Rod(OcDrift(0.9f), FVector(-260.f, s * 220.f, G + 30.f), FVector(260.f, s * 220.f, Floor - 20.f), 6.f); B.K.Rod(OcDrift(0.9f), FVector(260.f, s * 220.f, G + 30.f), FVector(-260.f, s * 220.f, Floor - 20.f), 6.f); }
		for (int32 p = 0; p < 12; ++p) B.K.Add(OcTimber(0.75f + 0.1f * B.Rand(p)), FVector(780.f, 40.f, 8.f), FVector(80.f, -240.f + p * 42.f, Floor));
		B.Box(FVector(80.f, 0, Floor), FVector(390.f, 250.f, 6.f));
		// The room (-X): driftwood walls with a door to the porch (+X), the roof.
		OcCoursedWall(B, FVector(-260.f, -230.f, Floor), FVector(-260.f, 230.f, Floor), 230.f, 20.f, 40.f, 230.f, 0.5f, true);
		for (int32 s = -1; s <= 1; s += 2) for (int32 k = 0; k < 12; ++k) B.K.Add(OcDrift(0.8f + 0.2f * B.Rand(k + s)), FVector(30.f, 8.f, 230.f), FVector(-250.f + k * 30.f, s * 230.f, Floor + 115.f));
		for (int32 s = -1; s <= 1; s += 2) B.K.Add(OcShellTile(), FVector(560.f, 280.f, 10.f), FVector(-90.f, s * 120.f, Floor + 280.f), FRotator(0.f, 0.f, s * 16.f));
		// The porch over the pool: her lamp on a hook, a stool, nets and fishing lines on a rail.
		B.K.Rod(OcDrift(), FVector(460.f, -240.f, Floor + 100.f), FVector(460.f, 240.f, Floor + 100.f), 6.f);
		B.K.Add(OcNet(), FVector(10.f, 300.f, 120.f), FVector(466.f, 0, Floor + 40.f));
		B.K.Add(OcTimber(0.6f), FVector(40.f, 40.f, 50.f), FVector(380.f, 120.f, Floor + 25.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.K.Add(OcBronze(), FVector(26.f, 26.f, 36.f), FVector(420.f, -180.f, Floor + 160.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		B.Lamp(FVector(420.f, -180.f, Floor + 150.f), OcSRGB(0xFF, 0xC8, 0x80), 1600.f, 1200.f);
		for (int32 k = 0; k < 8; ++k) B.K.Add(OcTimber(0.6f), FVector(8.f, 60.f, 6.f), FVector(-290.f, 0, G + 30.f + k * 26.f));
		for (int32 s = -1; s <= 1; s += 2) B.K.Rod(OcTimber(0.6f), FVector(-290.f, s * 30.f, G), FVector(-290.f, s * 30.f, Floor + 20.f), 4.f);
		// Her harpoons on pegs by the door.
		for (int32 h = 0; h < 2; ++h) OcHarpoon(B, FVector(-240.f, -210.f + h * 40.f, Floor + 20.f), FVector(0.05f, 0.f, 1.f), 260.f);
	}

	// ------------------------------------------------------------------------------------------------ dispatch

	void Build(AWROceanScenery* A)
	{
		const FName K = A->Kind;
		const bool bDecor = K == TEXT("decor");
		const float Cull = bDecor ? WRTune(TEXT("oceanWorld.decorCull"), 20000.f) : K == TEXT("pound") || K == TEXT("beacon_stack") || K == TEXT("reef_lighthouse") || K == TEXT("colossus") || K == TEXT("dyke_head") || K == TEXT("long_dyke")
			? WRTune(TEXT("oceanWorld.landmarkCull"), 300000.f) : WRTune(TEXT("oceanWorld.siteCull"), 40000.f);
		FOcCtx C(A, Cull);
		if (K == TEXT("pound")) Pound(C);
		else if (K == TEXT("colossus")) Colossus(C);
		else if (K == TEXT("beacon_stack")) BeaconStack(C);
		else if (K == TEXT("spouting_horn")) SpoutingHorn(C);
		else if (K == TEXT("dyke_head")) DykeHead(C);
		else if (K == TEXT("long_dyke")) LongDyke(C);
		else if (K == TEXT("reef_lighthouse")) ReefLighthouse(C);
		else if (K == TEXT("bell_tower")) BellTower(C);
		else if (K.ToString().StartsWith(TEXT("shrine_"))) Shrine(C);
		else if (K == TEXT("camp")) Camp(C);
		else if (K == TEXT("runback")) Runback(C);
		else if (K == TEXT("cave_mouth")) CaveMouth(C);
		else if (K == TEXT("cave")) Cave(C);
		else if (K == TEXT("hall")) Hall(C);
		else if (K == TEXT("tsunami_stone")) TsunamiStone(C);
		else if (bDecor) Decor(C);
		else if (K == TEXT("haewon_house")) HaewonHouse(C);
		else if (!Site(C) && !Kit(C))
		{
			UE_LOG(LogWildreach, Warning, TEXT("OCEAN scenery: no builder for %s (%s)"), *K.ToString(), *A->SiteId.ToString());
		}
		C.B.Finish();
	}
}
