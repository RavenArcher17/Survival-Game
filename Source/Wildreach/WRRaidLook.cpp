// The raid tools and the raid bell, modelled in code (WRToolMesh parts, M_HeldTool: vertex colour + roughness).
// Real things measured (cooks/build-server-raids.md 1.4, 2.5): a petard, a tamped mining cartridge, an escalade ladder,
// a clay fire pot with a rag wick, a tuned bronze bell on its yoke in an iron bracket.
#include "WRRaidLook.h"
#include "WRToolMesh.h"

namespace WRRaidLookDetail
{
	const FLinearColor BellBronze(0.30f, 0.19f, 0.075f);      // cast bronze, aged (bell, petard)
	const FLinearColor BronzeDark(0.09f, 0.055f, 0.025f); // the inside of a bell
	const FLinearColor Iron(0.055f, 0.055f, 0.06f);
	const FLinearColor Oak(0.12f, 0.075f, 0.04f);         // oiled oak (plank, yoke)
	const FLinearColor Ash(0.40f, 0.30f, 0.18f);          // ash (ladder rails and rungs)
	const FLinearColor Terracotta(0.36f, 0.13f, 0.06f);
	const FLinearColor Cloth(0.42f, 0.36f, 0.26f);
	const FLinearColor CharBlack(0.03f, 0.025f, 0.02f);
	const FLinearColor Hide(0.25f, 0.16f, 0.09f);
	const FLinearColor ClayPlug(0.33f, 0.27f, 0.21f);
	const FLinearColor Match(0.36f, 0.31f, 0.21f);         // slow match: hemp cord soaked in saltpetre

	FColor Pack(const FLinearColor& C, float Rough)
	{
		return FColor(FMath::Clamp(FMath::RoundToInt(C.R * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(C.G * 255.f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(C.B * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Rough * 255.f), 0, 255));
	}

	/** An open turned shell (a bell, a pot's mouth): outer surface facing out, inner facing in, a rounded lip joining them
	 *  at the end, closed at the start. Profiles: (t along Length, radius); Inner the same count as Outer. */
	void Shell(FWRToolMesh& M, const FVector& Start, const FVector& Axis, float Length, const TArray<FVector2D>& Outer, const TArray<FVector2D>& Inner,
		int32 Sides, const FLinearColor& OutCol, const FLinearColor& InCol, float Rough)
	{
		const FVector Ref = FMath::Abs(Axis.Z) > 0.9f ? FVector::XAxisVector : FVector::ZAxisVector;
		const FVector AY = FVector::CrossProduct(Ref, Axis).GetSafeNormal();
		const FVector AZ = FVector::CrossProduct(Axis, AY).GetSafeNormal();
		const auto Surface = [&](const TArray<FVector2D>& Prof, const FLinearColor& Col, bool bIn)
		{
			const FColor C = Pack(Col, Rough);
			const int32 Base = M.Verts.Num();
			for (int32 r = 0; r < Prof.Num(); ++r)
			{
				// The slope of the profile tilts the normal along the axis (a smooth bell, not a stack of cylinders).
				const int32 r0 = FMath::Max(0, r - 1), r1 = FMath::Min(Prof.Num() - 1, r + 1);
				const float dR = Prof[r1].Y - Prof[r0].Y, dT = (Prof[r1].X - Prof[r0].X) * Length;
				for (int32 s = 0; s < Sides; ++s)
				{
					const float A = 2.f * PI * s / Sides;
					const FVector Radial = AY * FMath::Cos(A) + AZ * FMath::Sin(A);
					M.Verts.Add(Start + Axis * (Prof[r].X * Length) + Radial * Prof[r].Y);
					FVector N = (Radial * dT - Axis * dR).GetSafeNormal();
					M.Normals.Add(bIn ? -N : N);
					M.UVs.Add(FVector2D(Prof[r].X * Length / 12.f, s / float(Sides)));
					M.Colors.Add(C);
				}
			}
			for (int32 r = 0; r + 1 < Prof.Num(); ++r)
			{
				for (int32 s = 0; s < Sides; ++s)
				{
					const int32 A = Base + r * Sides + s, B = Base + r * Sides + (s + 1) % Sides, Cc = A + Sides, D = B + Sides;
					if (bIn) M.Tris.Append({ A, B, Cc, B, D, Cc });
					else M.Tris.Append({ A, Cc, B, B, Cc, D });
				}
			}
		};
		Surface(Outer, OutCol, false);
		Surface(Inner, InCol, true);
		// The lip: a short band from the outer rim to the inner one (faces along the axis, out of the mouth).
		TArray<FVector2D> Lip = { FVector2D(Outer.Last().X, Outer.Last().Y), FVector2D(Inner.Last().X, Inner.Last().Y) };
		const FColor C = Pack(OutCol, Rough);
		const int32 Base = M.Verts.Num();
		for (int32 r = 0; r < 2; ++r)
		{
			for (int32 s = 0; s < Sides; ++s)
			{
				const float A = 2.f * PI * s / Sides;
				const FVector Radial = AY * FMath::Cos(A) + AZ * FMath::Sin(A);
				M.Verts.Add(Start + Axis * (Lip[r].X * Length) + Radial * Lip[r].Y);
				M.Normals.Add(Axis);
				M.UVs.Add(FVector2D(r, s / float(Sides)));
				M.Colors.Add(C);
			}
		}
		for (int32 s = 0; s < Sides; ++s)
		{
			const int32 A = Base + s, B = Base + (s + 1) % Sides, Cc = A + Sides, D = B + Sides;
			M.Tris.Append({ A, B, Cc, B, D, Cc });
		}
	}

	/** A squared timber or strap from A to B, Width across (Side) and Thick across the other way. */
	void Beam(FWRToolMesh& M, const FVector& A, const FVector& B, const FVector& Side, float Width, float Thick, const FLinearColor& Col, float Rough)
	{
		const FVector Along = (B - A).GetSafeNormal();
		const FVector S = (Side - Along * FVector::DotProduct(Side, Along)).GetSafeNormal() * Width * 0.5f;
		const FVector T = FVector::CrossProduct(Along, S.GetSafeNormal()) * Thick * 0.5f;
		const TArray<FVector> R0 = { A - S - T, A + S - T, A + S + T, A - S + T };
		const TArray<FVector> R1 = { B - S - T, B + S - T, B + S + T, B - S + T };
		M.Loft({ R0, R1 }, Col, Rough, true);
	}
}

namespace WRRaidLook
{
using namespace WRRaidLookDetail;

FWRToolMesh Bell()
{
	// A tuned bell's profile, 28 cm across the lip, 24 cm from crown to lip, hung below its pin at the origin (-Z).
	// Crown, shoulder, waist, the thickening sound bow where the clapper strikes, and the lip.
	FWRToolMesh M;
	const float Top = -7.f;   // the crown sits under the yoke
	const float L = 24.f;
	const TArray<FVector2D> Outer = {
		{ 0.00f, 5.2f }, { 0.05f, 6.6f }, { 0.12f, 7.5f }, { 0.25f, 7.7f }, { 0.42f, 7.6f }, { 0.58f, 8.3f },
		{ 0.72f, 9.8f }, { 0.84f, 11.8f }, { 0.93f, 13.4f }, { 1.00f, 14.0f } };
	// The inside follows it at the wall's thickness: thin at the waist (1/15 of the lip), thick at the sound bow (1/13).
	const TArray<FVector2D> Inner = {
		{ 0.04f, 3.6f }, { 0.08f, 5.3f }, { 0.15f, 6.4f }, { 0.27f, 6.7f }, { 0.43f, 6.6f }, { 0.59f, 7.2f },
		{ 0.73f, 8.5f }, { 0.85f, 10.2f }, { 0.93f, 11.6f }, { 1.00f, 12.6f } };
	Shell(M, FVector(0.f, 0.f, Top), -FVector::ZAxisVector, L, Outer, Inner, 28, BellBronze, BronzeDark, 0.35f);
	// Two cast mouldings (wires) round the shoulder and above the sound bow.
	TArray<FVector> Ring1, Ring2;
	for (int32 i = 0; i < 28; ++i)
	{
		const float A = 2.f * PI * i / 28.f;
		Ring1.Add(FVector(FMath::Cos(A) * 7.65f, FMath::Sin(A) * 7.65f, Top - 0.12f * L));
		Ring2.Add(FVector(FMath::Cos(A) * 11.2f, FMath::Sin(A) * 11.2f, Top - 0.80f * L));
	}
	M.Tube(Ring1, 0.35f, 6, BellBronze, 0.3f, true);
	M.Tube(Ring2, 0.35f, 6, BellBronze, 0.3f, true);
	// The crown's cast loop (the canon) up into the yoke.
	TArray<FVector> Canon;
	for (int32 i = 0; i <= 10; ++i)
	{
		const float A = PI * i / 10.f;
		Canon.Add(FVector(FMath::Cos(A) * 2.6f, 0.f, Top + FMath::Sin(A) * 3.2f));
	}
	M.Tube(Canon, 0.9f, 8, BellBronze, 0.35f);
	// The oak yoke the canon is bolted into, and the iron pin through it (the pivot, at the origin, along Y).
	Beam(M, FVector(0.f, -9.f, -1.5f), FVector(0.f, 9.f, -1.5f), FVector::XAxisVector, 7.f, 7.f, Oak, 0.7f);
	M.Lathe(FVector(0.f, -13.f, 0.f), FVector::YAxisVector, 26.f, { FVector(0.f, 1.f, 1.f), FVector(1.f, 1.f, 1.f) }, 10, Iron, 0.5f);
	// Iron straps from the yoke down round the crown.
	for (const float Y : { -3.f, 3.f })
	{
		M.Tube({ FVector(-3.6f, Y, -1.5f), FVector(-4.2f, Y, Top + 1.f), FVector(-5.4f, Y, Top - 1.6f) }, 0.5f, 5, Iron, 0.55f);
		M.Tube({ FVector(3.6f, Y, -1.5f), FVector(4.2f, Y, Top + 1.f), FVector(5.4f, Y, Top - 1.6f) }, 0.5f, 5, Iron, 0.55f);
	}
	return M;
}

FWRToolMesh Clapper()
{
	// Hung from a staple inside the crown (its pivot at the origin): a wrought rod and a ball that strikes the sound bow.
	FWRToolMesh M;
	M.Tube({ FVector(0.f, -1.2f, 0.f), FVector(0.f, 0.f, 1.f), FVector(0.f, 1.2f, 0.f) }, 0.45f, 6, Iron, 0.55f);
	M.Lathe(FVector(0.f, 0.f, -0.5f), -FVector::ZAxisVector, 17.f, { FVector(0.f, 0.55f, 0.55f), FVector(0.85f, 0.7f, 0.7f), FVector(1.f, 0.9f, 0.9f) }, 8, Iron, 0.55f);
	M.Lathe(FVector(0.f, 0.f, -16.5f), -FVector::ZAxisVector, 4.6f, { FVector(0.f, 0.9f, 0.9f), FVector(0.2f, 1.9f, 1.9f), FVector(0.5f, 2.3f, 2.3f), FVector(0.8f, 1.9f, 1.9f), FVector(1.f, 0.6f, 0.6f) }, 12, Iron, 0.5f);
	M.Lathe(FVector(0.f, 0.f, -21.f), -FVector::ZAxisVector, 2.f, { FVector(0.f, 0.6f, 0.6f), FVector(1.f, 0.3f, 0.3f) }, 8, Iron, 0.5f);   // the flight below the ball
	return M;
}

FWRToolMesh Bracket(float PostRadius, float Reach)
{
	// Strapped round the post at the origin (the post's axis is Z), an arm out along +X to a fork whose cheeks hold the
	// bell's pin at (Reach, 0, -6). A diagonal brace under the arm takes the bell's weight back to the strap.
	FWRToolMesh M;
	for (const float Z : { 4.f, -4.f })
	{
		TArray<FVector> Strap;
		for (int32 i = 0; i < 24; ++i)
		{
			const float A = 2.f * PI * i / 24.f;
			Strap.Add(FVector(FMath::Cos(A) * (PostRadius + 0.6f), FMath::Sin(A) * (PostRadius + 0.6f), Z));
		}
		M.Tube(Strap, 0.8f, 6, Iron, 0.6f, true);
	}
	// The strap's clinch plate and rivets on the bell's side.
	Beam(M, FVector(PostRadius + 0.6f, 0.f, 6.f), FVector(PostRadius + 0.6f, 0.f, -6.f), FVector::YAxisVector, 7.f, 1.6f, Iron, 0.6f);
	// The arm and the brace.
	Beam(M, FVector(PostRadius, 0.f, 3.f), FVector(Reach + 3.f, 0.f, 3.f), FVector::YAxisVector, 3.2f, 2.f, Iron, 0.55f);
	Beam(M, FVector(PostRadius, 0.f, -10.f), FVector(Reach * 0.7f, 0.f, 2.f), FVector::YAxisVector, 2.4f, 1.6f, Iron, 0.6f);
	// The fork: two cheeks hanging from the arm's end, either side of the yoke, drilled for the pin.
	for (const float Y : { -10.5f, 10.5f })
	{
		Beam(M, FVector(Reach, Y, 3.f), FVector(Reach, Y, -8.f), FVector::XAxisVector, 4.f, 1.4f, Iron, 0.55f);
		M.Lathe(FVector(Reach, Y - 1.2f * FMath::Sign(Y), -6.f), FVector::YAxisVector * FMath::Sign(Y), 2.6f, { FVector(0.f, 1.5f, 1.5f), FVector(1.f, 1.5f, 1.5f) }, 8, Iron, 0.5f);  // the pin's head
	}
	Beam(M, FVector(Reach, -11.f, 3.f), FVector(Reach, 11.f, 3.f), FVector::XAxisVector, 3.2f, 2.f, Iron, 0.55f);
	return M;
}

FWRToolMesh Petard()
{
	// Wall face at X = 0, out toward +X. The oak plank (the madrier) against the wall, two iron straps across it, the
	// bronze petard bolted mouth-first to its middle, a coil of slow match hung on a nail with its end in the touch hole.
	FWRToolMesh M;
	Beam(M, FVector(3.f, 0.f, -20.f), FVector(3.f, 0.f, 20.f), FVector::YAxisVector, 60.f, 6.f, Oak, 0.75f);
	for (const float Z : { -12.f, 12.f })
	{
		Beam(M, FVector(6.3f, -30.f, Z), FVector(6.3f, 30.f, Z), FVector::ZAxisVector, 4.f, 0.6f, Iron, 0.6f);
		for (const float Y : { -26.f, -8.f, 8.f, 26.f })
		{
			M.Lathe(FVector(6.5f, Y, Z), FVector::XAxisVector, 0.8f, { FVector(0.f, 0.9f, 0.9f), FVector(1.f, 0.5f, 0.5f) }, 6, Iron, 0.5f);   // nail heads
		}
	}
	// The petard: a squat bell, 28 cm across the mouth, 30 cm long, a cast ring round its middle, a domed base.
	M.Lathe(FVector(6.f, 0.f, 0.f), FVector::XAxisVector, 30.f,
		{ FVector(0.f, 14.f, 14.f), FVector(0.06f, 15.f, 15.f), FVector(0.1f, 13.6f, 13.6f), FVector(0.4f, 12.2f, 12.2f), FVector(0.45f, 13.f, 13.f),
		  FVector(0.5f, 12.f, 12.f), FVector(0.8f, 10.8f, 10.8f), FVector(0.92f, 8.f, 8.f), FVector(1.f, 3.f, 3.f) }, 24, BellBronze, 0.4f);
	// Its flange bolts through the plank.
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = 2.f * PI * (i + 0.5f) / 6.f;
		M.Lathe(FVector(7.2f, FMath::Cos(A) * 16.f, FMath::Sin(A) * 16.f), FVector::XAxisVector, 1.2f, { FVector(0.f, 1.2f, 1.2f), FVector(1.f, 0.9f, 0.9f) }, 6, Iron, 0.5f);
	}
	// The touch-hole boss on top, near the base.
	M.Lathe(FVector(30.f, 0.f, 10.5f), FVector::ZAxisVector, 2.6f, { FVector(0.f, 2.2f, 2.2f), FVector(1.f, 1.6f, 1.6f) }, 10, BellBronze, 0.4f);
	// The slow match: a coil on a nail in the plank's corner, the cord running to the touch hole.
	TArray<FVector> Coil;
	for (int32 i = 0; i <= 36; ++i)
	{
		const float A = 2.f * PI * i / 12.f;
		Coil.Add(FVector(8.5f + i * 0.04f, -22.f + FMath::Cos(A) * 4.f, 13.f + FMath::Sin(A) * 4.f));
	}
	M.Tube(Coil, 0.5f, 5, Match, 0.95f);
	M.Tube(MatchCord(), 0.5f, 5, Match, 0.95f);
	return M;
}

TArray<FVector> MatchCord()
{
	// From the coil to the touch hole: over the plank's edge, along the petard's flank, up into the boss.
	return { FVector(9.9f, -22.f, 17.f), FVector(11.f, -17.f, 19.f), FVector(14.f, -10.f, 17.f), FVector(20.f, -5.f, 15.f),
		FVector(26.f, -2.f, 14.f), FVector(29.5f, -0.5f, 13.6f), FVector(30.f, 0.f, 13.2f) };
}

FWRToolMesh MiningCartridge()
{
	// Rock face at X = 0. A hide cartridge tamped into a drilled hole (half of it in the rock), a clay plug, the fuse.
	FWRToolMesh M;
	M.Lathe(FVector(-12.f, 0.f, 0.f), FVector::XAxisVector, 18.f, { FVector(0.f, 2.6f, 2.6f), FVector(0.9f, 3.f, 3.f), FVector(1.f, 2.4f, 2.4f) }, 10, Hide, 0.9f);
	TArray<FVector> Tie;
	for (int32 i = 0; i < 10; ++i) { const float A = 2.f * PI * i / 10.f; Tie.Add(FVector(3.f, FMath::Cos(A) * 3.1f, FMath::Sin(A) * 3.1f)); }
	M.Tube(Tie, 0.35f, 5, Match, 0.9f, true);
	M.Lathe(FVector(5.5f, 0.f, 0.f), FVector::XAxisVector, 2.6f, { FVector(0.f, 3.4f, 3.4f), FVector(1.f, 2.4f, 2.4f) }, 10, ClayPlug, 0.95f);
	M.Tube({ FVector(8.f, 0.f, 0.f), FVector(12.f, 1.f, 3.f), FVector(15.f, 3.f, 1.f), FVector(17.f, 4.f, -3.f) }, 0.45f, 5, Match, 0.95f);
	return M;
}

FWRToolMesh Ladder(float Length, float FootOut)
{
	// The ladder's own frame: the foot at the origin, the wall toward -X, Z up. The rails lean from the foot to the top
	// against the wall's edge; rungs every 30 cm pegged through both rails; two iron hooks bent over the wall's top.
	FWRToolMesh M;
	const float Rise = FMath::Sqrt(FMath::Max(1.f, Length * Length - FootOut * FootOut));
	const FVector Top(-FootOut, 0.f, Rise);
	const FVector Up = Top.GetSafeNormal();
	const FVector Out = FVector::CrossProduct(FVector::YAxisVector, Up).GetSafeNormal();   // away from the wall (and a little up)
	for (const float Y : { -22.f, 22.f })
	{
		const FVector Off(0.f, Y, 0.f);
		Beam(M, Off, Top + Off, Out, 5.f, 8.f, Ash, 0.75f);
	}
	for (float D = 30.f; D < Length - 10.f; D += 30.f)
	{
		const FVector At = Up * D;
		M.Lathe(At + FVector(0.f, -25.f, 0.f), FVector::YAxisVector, 50.f, { FVector(0.f, 1.5f, 1.5f), FVector(0.1f, 1.8f, 1.8f), FVector(0.5f, 1.9f, 1.9f), FVector(0.9f, 1.8f, 1.8f), FVector(1.f, 1.5f, 1.5f) }, 8, Ash, 0.7f, 0.f, D);
		for (const float Y : { -22.f, 22.f })
		{
			M.Lathe(At + Out * 4.6f + FVector(0.f, Y, 0.f), Out, 1.2f, { FVector(0.f, 1.1f, 1.1f), FVector(1.f, 0.8f, 0.8f) }, 6, Oak, 0.8f);   // the peg through the rail
		}
	}
	// The hooks: up past the rail's end, over the wall's top and down its far side.
	for (const float Y : { -22.f, 22.f })
	{
		const FVector Base = Top + FVector(0.f, Y, 0.f);
		M.Tube({ Base - Up * 14.f, Base, Base + Up * 10.f + FVector(-6.f, 0.f, 4.f), Base + FVector(-18.f, 0.f, 12.f), Base + FVector(-28.f, 0.f, 4.f), Base + FVector(-30.f, 0.f, -8.f) },
			1.1f, 6, Iron, 0.55f);
		// Iron bands where the hook is bolted to the rail.
		M.Lathe(Base - Up * 12.f, Up, 3.f, { FVector(0.f, 4.f, 3.4f), FVector(1.f, 4.f, 3.4f) }, 8, Iron, 0.6f);
	}
	// Shod feet: iron spikes where the rails meet the ground.
	for (const float Y : { -22.f, 22.f })
	{
		M.Lathe(FVector(0.f, Y, 4.f), -Up, 9.f, { FVector(0.f, 3.f, 2.4f), FVector(0.5f, 2.f, 1.6f), FVector(1.f, 0.2f, 0.2f) }, 6, Iron, 0.6f);
	}
	return M;
}

FWRToolMesh FirePot()
{
	// A fired clay jar along +X (base at 0), 15 cm long: round belly, short neck, rolled lip, a cloth bung tied on, and a
	// rag wick standing out of it, its end charred (it's lit as it's thrown).
	FWRToolMesh M;
	M.Lathe(FVector(-6.f, 0.f, 0.f), FVector::XAxisVector, 12.f,
		{ FVector(0.f, 3.5f, 3.5f), FVector(0.08f, 5.2f, 5.2f), FVector(0.3f, 6.8f, 6.8f), FVector(0.55f, 6.6f, 6.6f), FVector(0.78f, 4.6f, 4.6f),
		  FVector(0.9f, 2.6f, 2.6f), FVector(0.96f, 2.5f, 2.5f), FVector(1.f, 3.1f, 3.1f) }, 16, Terracotta, 0.85f, 0.f, 3.f);
	M.Lathe(FVector(5.6f, 0.f, 0.f), FVector::XAxisVector, 2.4f, { FVector(0.f, 3.3f, 3.3f), FVector(0.6f, 3.1f, 3.1f), FVector(1.f, 2.2f, 2.2f) }, 10, Cloth, 0.95f);
	TArray<FVector> Tie;
	for (int32 i = 0; i < 10; ++i) { const float A = 2.f * PI * i / 10.f; Tie.Add(FVector(5.2f, FMath::Cos(A) * 2.75f, FMath::Sin(A) * 2.75f)); }
	M.Tube(Tie, 0.3f, 5, Match, 0.9f, true);
	M.Lathe(FVector(7.8f, 0.f, 0.f), FVector::XAxisVector, 4.5f, { FVector(0.f, 0.8f, 0.6f), FVector(0.7f, 0.7f, 0.5f), FVector(1.f, 0.4f, 0.3f) }, 6, Cloth, 0.95f, 0.6f, 5.f);
	M.Lathe(FVector(11.4f, 0.f, 0.f), FVector::XAxisVector, 1.f, { FVector(0.f, 0.55f, 0.45f), FVector(1.f, 0.2f, 0.2f) }, 6, CharBlack, 1.f);
	return M;
}
}
