// The siege engines modelled in code (cookpass-building 3.6, 1.3): a battering ram (a heartwood log hung by two chains
// from the ridge of a wheeled shed under wet hides), a counterweight trebuchet (an arm on an axle between two A-frame
// towers, a hinged box of stones, a sling, a winch with its ratchet, the whole upper frame on a turntable), a ballista (a
// steel bow on a stock on a swivelling yoke over a tripod, drawn by a windlass). Measures from real engines; every part
// rests on what carries it: wheels on axles in bearing blocks, posts on soles, rafters on the ridge and the plates, the
// arm's axle in the towers' apex blocks, the drum's spindle in the frame. Vertex colour carries colour and roughness
// (M_HeldTool adds grain along U).

#include "WRSiegeLook.h"
#include "WRToolMesh.h"
#include "ProceduralMeshComponent.h"

namespace WRSiegeLookDetail
{
	const FLinearColor Oak(0.17f, 0.105f, 0.055f);        // oiled oak (frames)
	const FLinearColor OakDark(0.09f, 0.055f, 0.03f);     // end grain, cleats
	const FLinearColor Heart(0.36f, 0.19f, 0.07f);        // heartwood: amber under the bark
	const FLinearColor Bark(0.08f, 0.055f, 0.035f);
	const FLinearColor Iron(0.05f, 0.05f, 0.055f);
	const FLinearColor Steel(0.32f, 0.33f, 0.35f);
	const FLinearColor Hide(0.22f, 0.13f, 0.07f);
	const FLinearColor HideWet(0.13f, 0.075f, 0.04f);
	const FLinearColor Leather(0.2f, 0.11f, 0.05f);
	const FLinearColor RopeC(0.42f, 0.35f, 0.22f);
	const FLinearColor StoneC(0.36f, 0.34f, 0.31f);
	const FLinearColor StoneDark(0.2f, 0.19f, 0.18f);

	/** A squared timber or strap from A to B, Width across Side and Thick across the other way. */
	void Bar(FWRToolMesh& M, const FVector& A, const FVector& B, float Width, float Thick, const FLinearColor& Col, float Rough, FVector Side = FVector::ZeroVector)
	{
		const FVector Along = (B - A).GetSafeNormal();
		if (Along.IsNearlyZero()) return;
		if (Side.IsNearlyZero()) Side = FMath::Abs(Along.Z) > 0.9f ? FVector::YAxisVector : FVector::CrossProduct(FVector::ZAxisVector, Along).GetSafeNormal();
		const FVector S = (Side - Along * FVector::DotProduct(Side, Along)).GetSafeNormal() * Width * 0.5f;
		const FVector T = FVector::CrossProduct(Along, S.GetSafeNormal()) * Thick * 0.5f;
		const TArray<FVector> R0 = { A - S - T, A + S - T, A + S + T, A - S + T };
		const TArray<FVector> R1 = { B - S - T, B + S - T, B + S + T, B - S + T };
		M.Loft({ R0, R1 }, Col, Rough, true);
	}
	/** An axis-aligned block. */
	void Block(FWRToolMesh& M, const FVector& Min, const FVector& Max, const FLinearColor& Col, float Rough)
	{
		const FVector C = (Min + Max) * 0.5f;
		Bar(M, FVector(Min.X, C.Y, C.Z), FVector(Max.X, C.Y, C.Z), Max.Y - Min.Y, Max.Z - Min.Z, Col, Rough, FVector::YAxisVector);
	}
	/** A round bar (an axle, a pin, a spoke) from A to B. */
	void Cyl(FWRToolMesh& M, const FVector& A, const FVector& B, float R, const FLinearColor& Col, float Rough, int32 Sides = 12, float Seed = 0.f)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 0.1f) return;
		M.Lathe(A, D / L, L, { FVector(0.f, R, R), FVector(1.f, R, R) }, Sides, Col, Rough, 0.f, Seed);
	}
	/** A ring (a tyre, a band, a chain link) round Axis. */
	void Ring(FWRToolMesh& M, const FVector& Centre, const FVector& Axis, float Radius, float Thick, const FLinearColor& Col, float Rough, int32 N = 20, float Squash = 1.f)
	{
		const FVector A = Axis.GetSafeNormal();
		const FVector U = FVector::CrossProduct(A, FMath::Abs(A.Z) > 0.9f ? FVector::XAxisVector : FVector::ZAxisVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(A, U);
		TArray<FVector> P;
		for (int32 i = 0; i < N; ++i)
		{
			const float T = 2.f * PI * i / N;
			P.Add(Centre + U * FMath::Cos(T) * Radius + V * FMath::Sin(T) * Radius * Squash);
		}
		M.Tube(P, Thick, 6, Col, Rough, true);
	}
	/** A stave lashed with three turns of rope at a joint. */
	void Lashing(FWRToolMesh& M, const FVector& At, const FVector& Axis, float R)
	{
		for (int32 t = -1; t <= 1; ++t) Ring(M, At + Axis.GetSafeNormal() * t * 2.4f, Axis, R + 1.2f, 1.2f, RopeC, 0.9f, 14);
	}
}

namespace WRSiegeLook
{
using namespace WRSiegeLookDetail;

// ================================================================================================ the battering ram

float RamChainX() { return 110.f; }
float RamHookZ() { return 246.f; }
float RamChainLength() { return 100.f; }   // the log's axis rides at 108 cm, clear over the cross ties
float RamEyeZ() { return 38.f; }           // the eye bolts' rings over the log's axis
float RamWheelRadius() { return 45.f; }

FWRToolMesh RamFrame()
{
	FWRToolMesh M;
	const float SoleY = 68.f, SoleZ0 = 55.f, SoleZ1 = 75.f;
	// Two sole beams carry everything; three cross ties hold them apart.
	for (const float Y : { -SoleY, SoleY }) Block(M, FVector(-235.f, Y - 10.f, SoleZ0), FVector(235.f, Y + 10.f, SoleZ1), Oak, 0.75f);
	for (const float X : { -225.f, 0.f, 225.f }) Block(M, FVector(X - 9.f, -SoleY - 10.f, SoleZ0 + 2.f), FVector(X + 9.f, SoleY + 10.f, SoleZ1 - 2.f), Oak, 0.75f);
	// Bearing blocks bolted under the soles at each wheel, the iron axles turning in them.
	for (const float X : { -150.f, 150.f })
	{
		for (const float Y : { -SoleY, SoleY })
		{
			Block(M, FVector(X - 14.f, Y - 9.f, 38.f), FVector(X + 14.f, Y + 9.f, SoleZ0), OakDark, 0.8f);
			for (const float BX : { X - 10.f, X + 10.f }) Cyl(M, FVector(BX, Y, 36.f), FVector(BX, Y, SoleZ1 + 2.f), 1.6f, Iron, 0.5f, 6);   // bolts through
		}
		Cyl(M, FVector(X, -98.f, RamWheelRadius()), FVector(X, 98.f, RamWheelRadius()), 5.f, Iron, 0.45f);
		for (const float Y : { -98.f, 98.f }) Ring(M, FVector(X, Y, RamWheelRadius()), FVector::YAxisVector, 6.5f, 1.5f, Iron, 0.5f, 10);   // linch rings
	}
	// Corner posts on the soles up to the eave plates.
	const float PlateZ = 170.f;
	for (const float X : { -215.f, 215.f }) for (const float Y : { -SoleY, SoleY }) Block(M, FVector(X - 8.f, Y - 8.f, SoleZ1), FVector(X + 8.f, Y + 8.f, PlateZ), Oak, 0.75f);
	for (const float Y : { -SoleY, SoleY }) Block(M, FVector(-235.f, Y - 8.f, PlateZ), FVector(235.f, Y + 8.f, PlateZ + 12.f), Oak, 0.75f);
	// Two A-frames inside carry the ridge where the chains hang: rafters from the soles up to it, a collar across.
	const float RidgeZ = 252.f;
	for (const float X : { -RamChainX() - 20.f, RamChainX() + 20.f })
	{
		for (const float Y : { -SoleY, SoleY }) Bar(M, FVector(X, Y, SoleZ1), FVector(X, 0.f, RidgeZ - 6.f), 14.f, 14.f, Oak, 0.75f, FVector::XAxisVector);
		Block(M, FVector(X - 7.f, -SoleY * 0.45f, 196.f), FVector(X + 7.f, SoleY * 0.45f, 208.f), Oak, 0.75f);
	}
	Block(M, FVector(-240.f, -9.f, RidgeZ - 8.f), FVector(240.f, 9.f, RidgeZ + 8.f), Oak, 0.75f);
	// The chains' hooks: iron eyes through the ridge.
	for (const float X : { -RamChainX(), RamChainX() })
	{
		Ring(M, FVector(X, 0.f, RamHookZ() - 2.f), FVector::YAxisVector, 4.f, 1.4f, Iron, 0.5f, 10);
		Cyl(M, FVector(X, 0.f, RamHookZ() + 2.f), FVector(X, 0.f, RidgeZ + 10.f), 1.8f, Iron, 0.5f, 6);
	}
	// Rafters every 50 cm from the ridge down past the plates to the eaves (1.3 m), and the wet hides lapped over them.
	const float EaveY = 112.f, EaveZ = 130.f;
	for (float X = -225.f; X <= 226.f; X += 50.f)
	{
		for (const float S : { -1.f, 1.f }) Bar(M, FVector(X, 0.f, RidgeZ + 4.f), FVector(X, S * EaveY, EaveZ), 7.f, 9.f, Oak, 0.75f, FVector::XAxisVector);
	}
	for (int32 h = 0; h < 4; ++h)
	{
		const float X = -180.f + h * 120.f;
		for (const float S : { -1.f, 1.f })
		{
			// Each hide drapes a little lower in the middle of its span and overlaps the next by a hand.
			const FLinearColor C = (h % 2) ? Hide : HideWet;
			Bar(M, FVector(X, S * 2.f, RidgeZ + 11.f), FVector(X, S * (EaveY + 6.f), EaveZ + 3.f), 132.f, 2.5f, C, 0.55f, FVector::XAxisVector);
			// Lacing at the eave: thongs round the rafter feet.
			for (const float LX : { X - 45.f, X, X + 45.f }) Ring(M, FVector(LX, S * (EaveY - 4.f), EaveZ + 6.f), FVector(0.f, -S * 0.75f, 0.66f), 2.2f, 0.7f, Leather, 0.7f, 8);
		}
	}
	// A ridge cap hide over the top.
	Bar(M, FVector(-240.f, 0.f, RidgeZ + 13.f), FVector(240.f, 0.f, RidgeZ + 13.f), 30.f, 3.f, HideWet, 0.5f, FVector::YAxisVector);
	// The push handles: two grips out the back off the rear posts, a bar across for the hands.
	for (const float Y : { -SoleY, SoleY })
	{
		Bar(M, FVector(-215.f, Y, 100.f), FVector(-268.f, Y, 98.f), 8.f, 8.f, Oak, 0.7f);
		Cyl(M, FVector(-268.f, Y - 14.f, 98.f), FVector(-268.f, Y + 14.f, 98.f), 3.4f, OakDark, 0.6f, 10);   // the worn grip
	}
	// The harness pole: back from the middle tie to a crossbar with two rope traces for a hauler's collar.
	Bar(M, FVector(-225.f, 0.f, 66.f), FVector(-430.f, 0.f, 62.f), 10.f, 10.f, Oak, 0.75f);
	Bar(M, FVector(-430.f, -38.f, 62.f), FVector(-430.f, 38.f, 62.f), 9.f, 9.f, Oak, 0.75f);
	for (const float Y : { -34.f, 34.f })
	{
		Lashing(M, FVector(-430.f, Y, 62.f), FVector::YAxisVector, 4.5f);
		M.Tube({ FVector(-430.f, Y, 62.f), FVector(-470.f, Y * 0.9f, 74.f), FVector(-505.f, Y * 0.7f, 92.f) }, 1.6f, 6, RopeC, 0.9f);
	}
	return M;
}

FWRToolMesh RamWheel()
{
	// A solid wheel of three oak planks (90 cm across), two cleats nailed across them, an iron tyre, a hub on the axle.
	FWRToolMesh M;
	const float R = RamWheelRadius();
	const float W = 12.f;
	for (int32 p = -1; p <= 1; ++p)
	{
		// Each plank is the slice of the disc it covers (a lathe with a squashed profile reads as the round face).
		const float Z0 = p * (2.f * R / 3.f) - R / 3.f, Z1 = Z0 + 2.f * R / 3.f - 0.8f;
		const float HalfW = FMath::Sqrt(FMath::Max(1.f, R * R - FMath::Square(FMath::Max(FMath::Abs(Z0), FMath::Abs(Z1))))) ;
		Block(M, FVector(-HalfW, -W * 0.5f, Z0), FVector(HalfW, W * 0.5f, Z1), p == 0 ? Oak : OakDark * 1.6f, 0.75f);
	}
	// The round of the wheel: the disc's rim between the planks' corners, then the tyre over it.
	M.Lathe(FVector(0.f, -W * 0.5f + 1.f, 0.f), FVector::YAxisVector, W - 2.f, { FVector(0.f, R - 2.f, R - 2.f), FVector(1.f, R - 2.f, R - 2.f) }, 28, Oak, 0.75f, 0.f, 3.f);
	Ring(M, FVector(0.f, 0.f, 0.f), FVector::YAxisVector, R, 3.2f, Iron, 0.45f, 32);
	for (const float Y : { -W * 0.5f - 1.5f, W * 0.5f + 1.5f })
	{
		Block(M, FVector(-R * 0.82f, Y - 1.5f, -6.f), FVector(R * 0.82f, Y + 1.5f, 6.f), OakDark, 0.8f);
		for (float X = -R * 0.7f; X <= R * 0.71f; X += R * 0.35f) Cyl(M, FVector(X, Y - 2.f, 0.f), FVector(X, Y + 2.f, 0.f), 1.4f, Iron, 0.5f, 6);
	}
	Cyl(M, FVector(0.f, -W * 0.5f - 6.f, 0.f), FVector(0.f, W * 0.5f + 6.f, 0.f), 9.f, OakDark, 0.7f, 14);
	return M;
}

FWRToolMesh RamChain(float Length)
{
	// Links 9 cm long, each turned a quarter from the last, hung straight down from the hook.
	FWRToolMesh M;
	const float Step = 7.f;
	for (int32 i = 0; i * Step < Length - 3.f; ++i)
	{
		const FVector C(0.f, 0.f, -4.f - i * Step);
		const FVector Axis = (i % 2) ? FVector::XAxisVector : FVector::YAxisVector;
		Ring(M, C, Axis, 3.6f, 0.9f, Iron, 0.5f, 10, 1.45f);
	}
	return M;
}

FWRToolMesh RamLog()
{
	// A 4.5 m heartwood log, 40 cm through, a little uneven (a felled trunk dressed with an adze), the amber heart showing
	// at the back end; a steel cap shrunk on its head; iron bands where the eye bolts go through.
	FWRToolMesh M;
	const float Half = 225.f, R = 20.f;
	M.Lathe(FVector(-Half, 0.f, 0.f), FVector::XAxisVector, 2.f * Half, { FVector(0.f, R * 0.92f, R * 0.92f), FVector(0.3f, R, R), FVector(0.7f, R * 0.97f, R * 0.97f), FVector(1.f, R * 0.95f, R * 0.95f) }, 16, Heart, 0.7f, 0.f, 7.f);
	Ring(M, FVector(-Half + 0.5f, 0.f, 0.f), FVector::XAxisVector, R * 0.55f, 1.2f, Bark, 0.9f, 14);   // growth rings at the butt
	Ring(M, FVector(-Half + 0.5f, 0.f, 0.f), FVector::XAxisVector, R * 0.3f, 1.f, OakDark, 0.9f, 12);
	// The steel cap: a sleeve over the head and a blunt boss.
	M.Lathe(FVector(Half - 22.f, 0.f, 0.f), FVector::XAxisVector, 46.f, { FVector(0.f, R + 2.f, R + 2.f), FVector(0.45f, R + 2.f, R + 2.f), FVector(0.75f, R - 2.f, R - 2.f), FVector(1.f, R * 0.55f, R * 0.55f) }, 16, Steel, 0.32f);
	for (const float X : { -RamChainX(), RamChainX() })
	{
		Ring(M, FVector(X, 0.f, 0.f), FVector::XAxisVector, R + 0.8f, 1.6f, Iron, 0.45f, 18);
		Cyl(M, FVector(X, 0.f, R - 2.f), FVector(X, 0.f, R + 14.f), 1.8f, Iron, 0.5f, 6);
		Ring(M, FVector(X, 0.f, R + 18.f), FVector::XAxisVector, 4.f, 1.4f, Iron, 0.5f, 10);   // the eye the chain's last link takes
	}
	return M;
}

// ================================================================================================ the trebuchet

float TrebAxleZ() { return 420.f; }
float TrebLong() { return 600.f; }
float TrebShort() { return 150.f; }
float TrebSlingLength() { return 240.f; }
FVector TrebDrumAt() { return FVector(-150.f, 0.f, 112.f); }
FVector TrebPawlAt() { return FVector(-104.f, -76.f, 152.f); }

FWRToolMesh TrebBase()
{
	// Four squared timbers on the ground and the turntable's lower ring with its iron track.
	FWRToolMesh M;
	for (const float Y : { -150.f, 150.f }) Block(M, FVector(-330.f, Y - 14.f, 0.f), FVector(330.f, Y + 14.f, 28.f), Oak, 0.8f);
	for (const float X : { -260.f, 260.f }) Block(M, FVector(X - 14.f, -175.f, 2.f), FVector(X + 14.f, 175.f, 26.f), Oak, 0.8f);
	// Diagonal braces from the corners to the ring's seat.
	for (const float X : { -1.f, 1.f }) for (const float Y : { -1.f, 1.f }) Bar(M, FVector(X * 250.f, Y * 140.f, 14.f), FVector(X * 95.f, Y * 95.f, 14.f), 14.f, 22.f, OakDark * 1.5f, 0.8f, FVector::ZAxisVector);
	M.Lathe(FVector(0.f, 0.f, 26.f), FVector::ZAxisVector, 12.f, { FVector(0.f, 160.f, 160.f), FVector(1.f, 160.f, 160.f) }, 40, Oak, 0.75f);
	Ring(M, FVector(0.f, 0.f, 38.5f), FVector::ZAxisVector, 150.f, 2.2f, Iron, 0.4f, 48);
	Cyl(M, FVector(0.f, 0.f, 20.f), FVector(0.f, 0.f, 70.f), 9.f, Iron, 0.4f, 14);   // the king pin
	return M;
}

FWRToolMesh TrebUpper()
{
	FWRToolMesh M;
	const float Z0 = 50.f, Z1 = 74.f;
	// The upper frame on its rollers over the track (the rollers' axles in the sills).
	for (const float Y : { -120.f, 120.f }) Block(M, FVector(-300.f, Y - 12.f, Z0), FVector(300.f, Y + 12.f, Z1), Oak, 0.75f);
	for (const float X : { -220.f, 220.f }) Block(M, FVector(X - 12.f, -132.f, Z0 + 2.f), FVector(X + 12.f, 132.f, Z1 - 2.f), Oak, 0.75f);
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = 2.f * PI * (i + 0.5f) / 8.f;
		const FVector C(FMath::Cos(A) * 150.f, FMath::Sin(A) * 150.f, 45.f);   // on the iron track (40.7), under the sills (50)
		const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.f);
		Cyl(M, C - Radial * 9.f, C + Radial * 9.f, 4.f, Iron, 0.45f, 10);
	}
	// Two towers: an A of two legs from the sills to the apex block where the axle turns, a brace across, a stay.
	const float Apex = TrebAxleZ();
	for (const float Y : { -120.f, 120.f })
	{
		Bar(M, FVector(-200.f, Y, Z1), FVector(-6.f, Y, Apex - 10.f), 22.f, 22.f, Oak, 0.75f, FVector::YAxisVector);
		Bar(M, FVector(200.f, Y, Z1), FVector(6.f, Y, Apex - 10.f), 22.f, 22.f, Oak, 0.75f, FVector::YAxisVector);
		Block(M, FVector(-110.f, Y - 10.f, 210.f), FVector(110.f, Y + 10.f, 226.f), Oak, 0.75f);
		Block(M, FVector(-22.f, Y - 16.f, Apex - 22.f), FVector(22.f, Y + 16.f, Apex - 8.f), OakDark, 0.8f);   // the apex bearing block
		Ring(M, FVector(0.f, Y + (Y > 0.f ? 17.f : -17.f), Apex), FVector::YAxisVector, 10.f, 2.f, Iron, 0.4f, 16);   // the axle's iron cap
	}
	// The axle across the apexes (the arm's hub turns on it).
	Cyl(M, FVector(0.f, -140.f, Apex), FVector(0.f, 140.f, Apex), 8.f, Iron, 0.4f, 16);
	// The trough between the sills the sling lies in when cocked: planks on two bearers, running back past the frame.
	for (const float Y : { -26.f, 26.f }) Block(M, FVector(-560.f, Y - 5.f, Z0 + 4.f), FVector(-60.f, Y + 5.f, Z1 + 10.f), Oak, 0.8f);
	Block(M, FVector(-560.f, -26.f, Z0 + 2.f), FVector(-60.f, 26.f, Z0 + 8.f), OakDark * 1.4f, 0.85f);
	for (const float X : { -540.f, -420.f })
	{
		Block(M, FVector(X - 8.f, -40.f, 0.f), FVector(X + 8.f, 40.f, Z0 + 2.f), Oak, 0.8f);   // a trestle under the trough's far end
	}
	// The winch's spindle seats on two posts, and the pawl's post.
	for (const float Y : { -100.f, 100.f }) Block(M, FVector(TrebDrumAt().X - 10.f, Y - 8.f, Z1), FVector(TrebDrumAt().X + 10.f, Y + 8.f, TrebDrumAt().Z + 10.f), Oak, 0.75f);
	Block(M, FVector(TrebPawlAt().X - 6.f, TrebPawlAt().Y - 6.f, Z1), FVector(TrebPawlAt().X + 6.f, TrebPawlAt().Y + 6.f, TrebPawlAt().Z + 4.f), Oak, 0.75f);
	// The aiming lever: a pole out from the rear corner of the upper frame for the aimer to walk it round.
	Bar(M, FVector(-300.f, -120.f, 60.f), FVector(-380.f, -190.f, 100.f), 10.f, 10.f, Oak, 0.75f);
	Cyl(M, FVector(-380.f, -190.f, 100.f), FVector(-392.f, -201.f, 104.f), 4.f, OakDark, 0.6f, 10);
	return M;
}

FWRToolMesh TrebDrum()
{
	// The drum (the rope winds on it), two spoked capstan wheels at its ends, the ratchet wheel inside the left one.
	FWRToolMesh M;
	Cyl(M, FVector(0.f, -92.f, 0.f), FVector(0.f, 92.f, 0.f), 4.5f, Iron, 0.45f, 12);   // spindle
	M.Lathe(FVector(0.f, -70.f, 0.f), FVector::YAxisVector, 140.f, { FVector(0.f, 24.f, 24.f), FVector(0.05f, 21.f, 21.f), FVector(0.95f, 21.f, 21.f), FVector(1.f, 24.f, 24.f) }, 18, Oak, 0.75f, 0.f, 4.f);
	for (int32 k = 0; k < 9; ++k) Ring(M, FVector(0.f, -60.f + k * 15.f, 0.f), FVector::YAxisVector, 22.f, 1.4f, RopeC, 0.9f, 18);   // wound rope
	for (const float Y : { -86.f, 86.f })
	{
		Ring(M, FVector(0.f, Y, 0.f), FVector::YAxisVector, 55.f, 3.f, Oak, 0.75f, 28);
		for (int32 s = 0; s < 6; ++s)
		{
			const float A = PI * s / 3.f;
			const FVector Dir(FMath::Cos(A), 0.f, FMath::Sin(A));
			Bar(M, FVector(0.f, Y, 0.f) + Dir * 8.f, FVector(0.f, Y, 0.f) + Dir * 66.f, 6.f, 6.f, Oak, 0.75f, FVector::YAxisVector);   // spokes, ends proud as handles
		}
	}
	// The ratchet wheel: 24 teeth (one every 15 deg, the pawl's click), next to the left capstan.
	const float RY = -76.f;
	M.Lathe(FVector(0.f, RY - 3.f, 0.f), FVector::YAxisVector, 6.f, { FVector(0.f, 30.f, 30.f), FVector(1.f, 30.f, 30.f) }, 24, Iron, 0.45f);
	for (int32 t = 0; t < 24; ++t)
	{
		const float A = 2.f * PI * t / 24.f;
		const FVector Dir(FMath::Cos(A), 0.f, FMath::Sin(A));
		const FVector Tan(-FMath::Sin(A), 0.f, FMath::Cos(A));
		Bar(M, FVector(0.f, RY, 0.f) + Dir * 29.f, FVector(0.f, RY, 0.f) + Dir * 35.f + Tan * 3.f, 5.f, 4.f, Iron, 0.45f, FVector::YAxisVector);
	}
	return M;
}

FWRToolMesh TrebArm()
{
	// Tapering oak (30 cm square at the hub, 16 at the tip), bound with iron every metre; the hub's iron boss on the axle;
	// the counterweight's pin through the short end; a hook under the long end the winch rope takes.
	FWRToolMesh M;
	const float L = TrebLong(), S = TrebShort();
	TArray<FVector> R0, R1, R2;
	const auto Sq = [](float X, float H) { return TArray<FVector>{ FVector(X, -H, -H), FVector(X, H, -H), FVector(X, H, H), FVector(X, -H, H) }; };
	M.Loft({ Sq(-L, 8.f), Sq(-L * 0.5f, 12.f), Sq(0.f, 15.f), Sq(S, 15.f) }, Oak, 0.75f, true);
	for (float X = -L + 60.f; X < S; X += 100.f)
	{
		const float H = FMath::Lerp(8.f, 15.f, FMath::Clamp((X + L) / L, 0.f, 1.f)) + 0.8f;
		Bar(M, FVector(X - 3.f, 0.f, 0.f), FVector(X + 3.f, 0.f, 0.f), 2.f * H, 2.f * H, Iron, 0.45f, FVector::YAxisVector);
	}
	Cyl(M, FVector(0.f, -24.f, 0.f), FVector(0.f, 24.f, 0.f), 14.f, Iron, 0.4f, 16);   // the hub
	Cyl(M, FVector(S - 10.f, -22.f, 0.f), FVector(S - 10.f, 22.f, 0.f), 4.f, Iron, 0.4f, 10);   // the weight's pin
	Ring(M, FVector(-L + 40.f, 0.f, -11.f), FVector::YAxisVector, 4.f, 1.4f, Iron, 0.45f, 10);   // the winch hook's eye
	return M;
}

FWRToolMesh TrebPin(int32 Pin)
{
	// An iron prong out of the arm's tip; the sling's loop slips off it at the angle it's bent to (Pin 0 low .. 2 high).
	FWRToolMesh M;
	const float Bend = FMath::DegreesToRadians(25.f + 20.f * FMath::Clamp(Pin, 0, 2));
	const FVector Dir(-FMath::Cos(Bend), 0.f, FMath::Sin(Bend));
	Cyl(M, FVector::ZeroVector, Dir * 26.f, 1.8f, Iron, 0.4f, 8);
	Ring(M, FVector::ZeroVector, FVector::XAxisVector, 9.f, 1.6f, Iron, 0.45f, 12);   // its collar on the tip
	return M;
}

FWRToolMesh TrebWeight()
{
	// Two iron hangers from the pin to a box of planks bound in iron, heaped with stones (1.4 m on a side).
	FWRToolMesh M;
	const float Hang = 34.f, Side = 70.f, Deep = 140.f;
	for (const float Y : { -Side - 6.f, Side + 6.f })
	{
		Bar(M, FVector(0.f, Y, 0.f), FVector(0.f, Y, -Hang - 20.f), 8.f, 3.f, Iron, 0.45f, FVector::XAxisVector);
		Ring(M, FVector(0.f, Y, 0.f), FVector::YAxisVector, 5.f, 1.6f, Iron, 0.45f, 10);
	}
	const float Top = -Hang;
	for (int32 p = 0; p < 7; ++p)
	{
		const float Z = Top - p * (Deep / 7.f);
		for (const float X : { -Side, Side }) Block(M, FVector(X - 3.f, -Side, Z - Deep / 7.f + 0.8f), FVector(X + 3.f, Side, Z), p % 2 ? Oak : OakDark * 1.5f, 0.8f);
		for (const float Y : { -Side, Side }) Block(M, FVector(-Side, Y - 3.f, Z - Deep / 7.f + 0.8f), FVector(Side, Y + 3.f, Z), p % 2 ? OakDark * 1.5f : Oak, 0.8f);
	}
	Block(M, FVector(-Side, -Side, Top - Deep - 4.f), FVector(Side, Side, Top - Deep), OakDark, 0.85f);
	for (const float Z : { Top - 10.f, Top - Deep * 0.5f, Top - Deep + 10.f })
	{
		for (const float X : { -Side - 3.5f, Side + 3.5f }) Block(M, FVector(X - 0.8f, -Side - 4.f, Z - 4.f), FVector(X + 0.8f, Side + 4.f, Z + 4.f), Iron, 0.45f);
		for (const float Y : { -Side - 3.5f, Side + 3.5f }) Block(M, FVector(-Side - 4.f, Y - 0.8f, Z - 4.f), FVector(Side + 4.f, Y + 0.8f, Z + 4.f), Iron, 0.45f);
	}
	// The stones heaped at the top of the box.
	for (int32 s = 0; s < 14; ++s)
	{
		const float X = -50.f + (s % 4) * 33.f + ((s * 37) % 11) - 5.f, Y = -50.f + (s / 4) * 33.f + ((s * 53) % 13) - 6.f;
		const float R = 13.f + (s * 7) % 6;
		M.Lathe(FVector(X, Y, Top - 8.f - R), FVector::ZAxisVector, 2.f * R, { FVector(0.f, R * 0.4f, R * 0.5f), FVector(0.5f, R, R * 0.9f), FVector(1.f, R * 0.35f, R * 0.4f) }, 9, s % 3 ? StoneC : StoneDark, 0.9f, 0.f, 11.f + s);
	}
	return M;
}

FWRToolMesh TrebPawl()
{
	// A forged pawl on its pin: its nose drops onto each tooth of the ratchet wheel as the drum turns (the click).
	FWRToolMesh M;
	Cyl(M, FVector(0.f, -6.f, 0.f), FVector(0.f, 6.f, 0.f), 3.f, Iron, 0.45f, 10);
	Bar(M, FVector(0.f, 0.f, 0.f), FVector(24.f, 0.f, -12.f), 5.f, 6.f, Iron, 0.45f, FVector::YAxisVector);
	return M;
}

// ================================================================================================ the ballista

float BalSwivelZ() { return 122.f; }
float BalTrunnionZ() { return 36.f; }
FVector BalLeafRoot(bool bLeft) { return FVector(118.f, bLeft ? 10.f : -10.f, 6.f); }
float BalLeafLength() { return 82.f; }
float BalSliderRest() { return 100.f; }
float BalSliderDrawn() { return -58.f; }
FVector BalWindlassAt() { return FVector(-104.f, 0.f, -2.f); }

FWRToolMesh BalTripod()
{
	// Three oak legs from a capped head down to iron-shod feet, braced by a ring of rails halfway down.
	FWRToolMesh M;
	const FVector Head(0.f, 0.f, BalSwivelZ() - 8.f);
	TArray<FVector> Mid;
	for (int32 i = 0; i < 3; ++i)
	{
		const float A = PI + 2.f * PI * i / 3.f;   // one leg straight back, under the gunner
		const FVector Foot(FMath::Cos(A) * 115.f, FMath::Sin(A) * 115.f, 0.f);
		Bar(M, Head, Foot, 12.f, 12.f, Oak, 0.75f);
		Block(M, Foot - FVector(9.f, 9.f, 0.f), Foot + FVector(9.f, 9.f, 6.f), Iron, 0.5f);
		Mid.Add(FMath::Lerp(Head, Foot, 0.58f));
	}
	for (int32 i = 0; i < 3; ++i) Bar(M, Mid[i], Mid[(i + 1) % 3], 7.f, 7.f, Oak, 0.75f, FVector::ZAxisVector);
	M.Lathe(FVector(0.f, 0.f, BalSwivelZ() - 20.f), FVector::ZAxisVector, 20.f, { FVector(0.f, 16.f, 16.f), FVector(1.f, 18.f, 18.f) }, 16, OakDark, 0.8f);
	Cyl(M, FVector(0.f, 0.f, BalSwivelZ() - 2.f), FVector(0.f, 0.f, BalSwivelZ() + 10.f), 4.f, Iron, 0.4f, 10);   // the swivel pin
	return M;
}

FWRToolMesh BalYoke()
{
	// A round iron plate on the pin and two oak cheeks up to the trunnion's bearings.
	FWRToolMesh M;
	M.Lathe(FVector(0.f, 0.f, 0.f), FVector::ZAxisVector, 5.f, { FVector(0.f, 22.f, 22.f), FVector(1.f, 22.f, 22.f) }, 18, Iron, 0.45f);
	for (const float Y : { -16.f, 16.f })
	{
		Block(M, FVector(-14.f, Y - 4.f, 5.f), FVector(14.f, Y + 4.f, BalTrunnionZ() + 10.f), Oak, 0.75f);
		Ring(M, FVector(0.f, Y + (Y > 0.f ? 4.5f : -4.5f), BalTrunnionZ()), FVector::YAxisVector, 5.f, 1.4f, Iron, 0.45f, 12);
	}
	return M;
}

FWRToolMesh BalStock()
{
	// The stock (2.3 m of oak, a groove along its top for the bolt), the trunnion pin across it, the bow's iron mount at
	// the front, the windlass's two cheeks at the back and two grips under it for the gunner.
	FWRToolMesh M;
	Block(M, FVector(-120.f, -7.f, -6.f), FVector(122.f, 7.f, 4.f), Oak, 0.75f);
	for (const float Y : { -6.f, 6.f }) Block(M, FVector(-60.f, Y - 1.f, 4.f), FVector(118.f, Y + 1.f, 7.f), OakDark * 1.4f, 0.8f);   // the groove's lips
	Cyl(M, FVector(0.f, -14.f, 0.f), FVector(0.f, 14.f, 0.f), 3.5f, Iron, 0.4f, 10);
	Block(M, FVector(110.f, -18.f, -8.f), FVector(126.f, 18.f, 12.f), Iron, 0.45f);   // the bow's mount
	for (const float Y : { -15.f, 15.f }) Block(M, FVector(-118.f, Y - 3.f, -14.f), FVector(-90.f, Y + 3.f, 8.f), Oak, 0.75f);
	for (const float Y : { -9.f, 9.f })
	{
		Bar(M, FVector(-84.f, Y, -6.f), FVector(-92.f, Y, -30.f), 5.f, 5.f, OakDark, 0.6f, FVector::YAxisVector);
	}
	// The trigger lever under the claw's rest.
	Bar(M, FVector(-40.f, 0.f, -6.f), FVector(-58.f, 0.f, -20.f), 3.f, 3.f, Iron, 0.45f, FVector::YAxisVector);
	return M;
}

FWRToolMesh BalLeaf(bool bLeft)
{
	// A steel leaf, thick at the root and thinning to a nocked tip, a gentle sweep forward.
	FWRToolMesh M;
	const float S = bLeft ? 1.f : -1.f;
	const float L = BalLeafLength();
	const auto Section = [S](float Y, float X, float W, float T) { return TArray<FVector>{ FVector(X - W, S * Y, -T), FVector(X + W, S * Y, -T), FVector(X + W, S * Y, T), FVector(X - W, S * Y, T) }; };
	M.Loft({ Section(0.f, 0.f, 5.f, 2.2f), Section(L * 0.5f, 4.f, 4.f, 1.8f), Section(L, 10.f, 2.6f, 1.3f) }, Steel, 0.3f, true);
	Ring(M, FVector(10.f, S * (L - 2.f), 0.f), FVector::ZAxisVector, 2.5f, 1.f, Iron, 0.45f, 8);   // the nock
	return M;
}

FWRToolMesh BalWindlass()
{
	FWRToolMesh M;
	Cyl(M, FVector(0.f, -14.f, 0.f), FVector(0.f, 14.f, 0.f), 6.f, Oak, 0.75f, 12);
	for (int32 k = 0; k < 4; ++k) Ring(M, FVector(0.f, -7.f + k * 4.5f, 0.f), FVector::YAxisVector, 6.6f, 0.9f, RopeC, 0.9f, 12);
	for (const float Y : { -20.f, 20.f })
	{
		const float Side = Y > 0.f ? 1.f : -1.f;
		Bar(M, FVector(0.f, Y, 0.f), FVector(0.f, Y, (Y > 0.f ? 1.f : -1.f) * 24.f), 4.f, 3.f, Iron, 0.45f, FVector::XAxisVector);
		Cyl(M, FVector(0.f, Y, Side * 24.f), FVector(0.f, Y + Side * 10.f, Side * 24.f), 2.2f, OakDark, 0.6f, 8);   // handles, half a turn apart
	}
	return M;
}

FWRToolMesh BalSlider()
{
	// The slider in the groove and the claw that holds the string.
	FWRToolMesh M;
	Block(M, FVector(-12.f, -5.f, 3.f), FVector(12.f, 5.f, 9.f), OakDark, 0.7f);
	Bar(M, FVector(10.f, 0.f, 9.f), FVector(16.f, 0.f, 14.f), 4.f, 3.f, Iron, 0.45f, FVector::YAxisVector);
	M.Tube({ FVector(-12.f, 0.f, 6.f), FVector(-30.f, 0.f, 4.f) }, 1.2f, 6, RopeC, 0.9f);   // the windlass cord's end
	return M;
}

// ================================================================================================ shared

FWRToolMesh Rope(float Radius)
{
	FWRToolMesh M;
	M.Lathe(FVector::ZeroVector, FVector::XAxisVector, 1.f, { FVector(0.f, Radius, Radius), FVector(1.f, Radius, Radius) }, 6, RopeC, 0.9f);
	return M;
}

FWRToolMesh Pouch()
{
	FWRToolMesh M;
	// A leather pouch hung in a cradle of two rope ends.
	M.Lathe(FVector(0.f, 0.f, -10.f), FVector::XAxisVector, 60.f, { FVector(0.f, 4.f, 2.f), FVector(0.25f, 26.f, 14.f), FVector(0.75f, 26.f, 14.f), FVector(1.f, 4.f, 2.f) }, 12, Leather, 0.7f);
	return M;
}

FWRToolMesh Stone()
{
	FWRToolMesh M;
	const float R = 35.f;
	M.Lathe(FVector(0.f, 0.f, -R), FVector::ZAxisVector, 2.f * R, { FVector(0.f, 4.f, 4.f), FVector(0.12f, R * 0.66f, R * 0.62f), FVector(0.5f, R, R * 0.95f), FVector(0.88f, R * 0.66f, R * 0.7f), FVector(1.f, 4.f, 4.f) }, 14, StoneC, 0.9f, 0.f, 21.f);
	return M;
}

FWRToolMesh Bolt()
{
	FWRToolMesh M;
	Cyl(M, FVector(-60.f, 0.f, 0.f), FVector(52.f, 0.f, 0.f), 2.2f, Oak, 0.7f, 8);
	M.Lathe(FVector(52.f, 0.f, 0.f), FVector::XAxisVector, 18.f, { FVector(0.f, 3.f, 3.f), FVector(0.3f, 4.2f, 4.2f), FVector(1.f, 0.2f, 0.2f) }, 8, Steel, 0.3f);
	for (int32 v = 0; v < 3; ++v)
	{
		const float A = 2.f * PI * v / 3.f;
		const FVector Out(0.f, FMath::Cos(A), FMath::Sin(A));
		Bar(M, FVector(-58.f, 0.f, 0.f) + Out * 2.f, FVector(-38.f, 0.f, 0.f) + Out * 2.f, 0.6f, 9.f, Leather, 0.7f, Out);   // leather vanes
	}
	return M;
}

bool BuildThrownModel(const FString& Path, UProceduralMeshComponent* Comp)
{
	if (Path == TEXT("code:siegestone")) { Stone().ApplyTo(Comp); return true; }
	if (Path == TEXT("code:ballistabolt")) { Bolt().ApplyTo(Comp); return true; }
	return false;
}
}
