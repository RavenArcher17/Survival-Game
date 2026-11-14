// Modelled hand tools (see WRToolMesh.h). Reference: Neolithic hafted axes (a tapered ash haft swelling to a knob at
// the butt, a knapped flint or ground stone celt seated through it, rawhide bound in criss-cross wraps), stone and
// antler picks (a curved double point mortised onto the haft), and a plain hand rod with a wound spool and a carved
// float. Sizes in cm come from tuning gather.toolLook; the shapes are fixed here.
#include "WRToolMesh.h"
#include "Wildreach.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace WRToolMeshDetail
{
	/** Deterministic 0..1 from a few numbers (knapping jitter is the same on every machine and every frame). */
	static float Hash(float A, float B, float C)
	{
		const uint32 H = HashCombine(HashCombine(GetTypeHash(FMath::RoundToInt(A * 97.f)), GetTypeHash(FMath::RoundToInt(B * 89.f))), GetTypeHash(FMath::RoundToInt(C * 83.f)));
		return (H % 10007) / 10006.f;
	}

	/** Two axes across Axis (Y-ish and Z-ish), so a part laid along X keeps Y toward the striking side. */
	static void Frame(const FVector& Axis, FVector& OutY, FVector& OutZ)
	{
		const FVector Ref = FMath::Abs(Axis.Z) > 0.9f ? FVector::XAxisVector : FVector::ZAxisVector;
		OutY = FVector::CrossProduct(Ref, Axis).GetSafeNormal();
		OutZ = FVector::CrossProduct(Axis, OutY).GetSafeNormal();
		if (FMath::Abs(Axis.X) > 0.9f)
		{
			// Along the haft: Y must be the striking side and Z the flat.
			OutY = FVector::YAxisVector * FMath::Sign(Axis.X);
			OutZ = FVector::CrossProduct(Axis, OutY).GetSafeNormal();
		}
	}
}

FColor FWRToolMesh::Pack(const FLinearColor& Colour, float Rough)
{
	// Vertex colour reaches the material as stored (no sRGB step): linear values go in as they are.
	return FColor(FMath::Clamp(FMath::RoundToInt(Colour.R * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Colour.G * 255.f), 0, 255),
		FMath::Clamp(FMath::RoundToInt(Colour.B * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Rough * 255.f), 0, 255));
}

void FWRToolMesh::Tri(const FVector& A, const FVector& B, const FVector& C, const FColor& Col, const FVector2D& UA, const FVector2D& UB, const FVector2D& UC)
{
	// Flat-shaded: its own three vertices, the face normal. Callers pass A, B, C counter-clockwise seen from outside;
	// Unreal draws the side where (B - A) x (C - A) points away from the viewer, so the order is flipped here.
	const FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
	const int32 First = Verts.Num();
	Verts.Append({ A, C, B });
	Normals.Append({ N, N, N });
	UVs.Append({ UA, UC, UB });
	Colors.Append({ Col, Col, Col });
	Tris.Append({ First, First + 1, First + 2 });
}

void FWRToolMesh::Lathe(const FVector& Start, const FVector& Axis, float Length, const TArray<FVector>& Profile, int32 Sides, const FLinearColor& Colour, float Rough, float Bend, float Seed)
{
	using namespace WRToolMeshDetail;
	FVector AY, AZ;
	Frame(Axis, AY, AZ);
	const FColor Col = Pack(Colour, Rough);
	const int32 Base = Verts.Num();
	for (int32 r = 0; r < Profile.Num(); ++r)
	{
		const float T = Profile[r].X;
		// Bend > 0: a gentle bow across Y (a haft's grain); Bend < 0: a droop toward -Z growing to the tip (a rod).
		const FVector Bow = Bend >= 0.f ? AY * Bend * FMath::Sin(PI * T) : AZ * Bend * T * T;
		const FVector Centre = Start + Axis * (T * Length) + Bow;
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A = 2.f * PI * s / Sides;
			// A hand-carved part is never quite round: a little wobble per vertex, the same every time.
			const float Wobble = Seed > 0.f ? 1.f + 0.035f * (Hash(Seed, r, s) - 0.5f) : 1.f;
			const float RY = Profile[r].Y * Wobble, RZ = Profile[r].Z * Wobble;
			Verts.Add(Centre + AY * FMath::Cos(A) * RY + AZ * FMath::Sin(A) * RZ);
			Normals.Add((AY * FMath::Cos(A) / FMath::Max(RY, 0.01f) + AZ * FMath::Sin(A) / FMath::Max(RZ, 0.01f)).GetSafeNormal());
			UVs.Add(FVector2D(T * Length / 12.f, s / static_cast<float>(Sides)));
			Colors.Add(Col);
		}
	}
	for (int32 r = 0; r + 1 < Profile.Num(); ++r)
	{
		for (int32 s = 0; s < Sides; ++s)
		{
			const int32 A = Base + r * Sides + s, B = Base + r * Sides + (s + 1) % Sides;
			const int32 C = A + Sides, D = B + Sides;
			// (A, B, C) runs counter-clockwise seen from outside; Unreal wants the reverse (see Tri).
			Tris.Append({ A, C, B, B, C, D });
		}
	}
	// Caps at both ends (a fan from the middle), flat.
	for (const int32 End : { 0, Profile.Num() - 1 })
	{
		if (Profile[End].Y <= 0.01f) continue;
		const FVector Out = End == 0 ? -Axis : Axis;
		const float T = Profile[End].X;
		const FVector Bow = Bend >= 0.f ? AY * Bend * FMath::Sin(PI * T) : AZ * Bend * T * T;
		const FVector Mid = Start + Axis * (T * Length) + Bow;
		for (int32 s = 0; s < Sides; ++s)
		{
			const FVector P0 = Verts[Base + End * Sides + s], P1 = Verts[Base + End * Sides + (s + 1) % Sides];
			FVector A = Mid, B = P0, C = P1;
			if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Out) < 0.f) Swap(B, C);
			Tri(A, B, C, Col, FVector2D(0.5f, 0.5f), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f));
		}
	}
}

void FWRToolMesh::Tube(const TArray<FVector>& Points, float R, int32 Sides, const FLinearColor& Colour, float Rough, bool bClosed)
{
	if (Points.Num() < 2) return;
	const FColor Col = Pack(Colour, Rough);
	const int32 Base = Verts.Num();
	const int32 N = Points.Num();
	// Parallel-transported frame, so the tube doesn't twist.
	FVector T0 = (Points[1] - Points[0]).GetSafeNormal();
	FVector U = FVector::CrossProduct(T0, FMath::Abs(T0.Z) > 0.9f ? FVector::XAxisVector : FVector::ZAxisVector).GetSafeNormal();
	float Along = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		const FVector Prev = Points[bClosed ? (i - 1 + N) % N : FMath::Max(0, i - 1)];
		const FVector Next = Points[bClosed ? (i + 1) % N : FMath::Min(N - 1, i + 1)];
		const FVector T = (Next - Prev).GetSafeNormal();
		U = (U - T * FVector::DotProduct(U, T)).GetSafeNormal();
		const FVector V = FVector::CrossProduct(T, U);
		if (i > 0) Along += FVector::Dist(Points[i], Points[i - 1]);
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A = 2.f * PI * s / Sides;
			const FVector Dir = U * FMath::Cos(A) + V * FMath::Sin(A);
			Verts.Add(Points[i] + Dir * R);
			Normals.Add(Dir);
			UVs.Add(FVector2D(Along / 4.f, s / static_cast<float>(Sides)));
			Colors.Add(Col);
		}
	}
	const int32 Segs = bClosed ? N : N - 1;
	for (int32 i = 0; i < Segs; ++i)
	{
		for (int32 s = 0; s < Sides; ++s)
		{
			const int32 A = Base + i * Sides + s, B = Base + i * Sides + (s + 1) % Sides;
			const int32 C = Base + ((i + 1) % N) * Sides + s, D = Base + ((i + 1) % N) * Sides + (s + 1) % Sides;
			// U, V, T is right-handed: B is counter-clockwise from A seen down the tube, so this winding faces out.
			Tris.Append({ A, C, B, B, C, D });
		}
	}
}

void FWRToolMesh::Loft(const TArray<TArray<FVector>>& Rings, const FLinearColor& Colour, float Rough, bool bCapEnds)
{
	if (Rings.Num() < 2) return;
	const FColor Col = Pack(Colour, Rough);
	FVector Centroid = FVector::ZeroVector;
	int32 Count = 0;
	for (const TArray<FVector>& R : Rings) for (const FVector& P : R) { Centroid += P; ++Count; }
	Centroid /= FMath::Max(1, Count);
	const auto Face = [this, &Col, &Centroid](const FVector& A, const FVector& B, const FVector& C, const FVector2D& UA, const FVector2D& UB, const FVector2D& UC)
	{
		const FVector Mid = (A + B + C) / 3.f;
		if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Mid - Centroid) >= 0.f) Tri(A, B, C, Col, UA, UB, UC);
		else Tri(A, C, B, Col, UA, UC, UB);
	};
	for (int32 r = 0; r + 1 < Rings.Num(); ++r)
	{
		const TArray<FVector>& R0 = Rings[r];
		const TArray<FVector>& R1 = Rings[r + 1];
		const int32 N = FMath::Min(R0.Num(), R1.Num());
		for (int32 s = 0; s < N; ++s)
		{
			const int32 s1 = (s + 1) % N;
			const float U0 = r / 4.f, U1 = (r + 1) / 4.f, V0 = s / static_cast<float>(N), V1 = (s + 1) / static_cast<float>(N);
			Face(R0[s], R0[s1], R1[s], FVector2D(U0, V0), FVector2D(U0, V1), FVector2D(U1, V0));
			Face(R0[s1], R1[s1], R1[s], FVector2D(U0, V1), FVector2D(U1, V1), FVector2D(U1, V0));
		}
	}
	if (!bCapEnds) return;
	for (const TArray<FVector>* R : { &Rings[0], &Rings.Last() })
	{
		FVector Mid = FVector::ZeroVector;
		for (const FVector& P : *R) Mid += P;
		Mid /= FMath::Max(1, R->Num());
		for (int32 s = 0; s < R->Num(); ++s)
		{
			Face(Mid, (*R)[s], (*R)[(s + 1) % R->Num()], FVector2D(0.5f, 0.5f), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f));
		}
	}
}

void FWRToolMesh::HatchetHead(const FVector& At, float S, bool bCrude, const FLinearColor& Head)
{
	using namespace WRToolMeshDetail;
	// Cross-sections from the poll (-Y) through the haft to the bit (+Y): half-height along the haft (X) and
	// half-thickness (Z). The bit flares and thins to an edge; a crude flint head is narrower and rougher.
	static const float Ys[] = { -4.f, -2.6f, 0.f, 3.f, 6.f, 9.f, 11.6f, 13.f };
	static const float Hx[] = { 2.5f, 3.3f, 3.9f, 4.3f, 4.8f, 5.4f, 5.9f, 6.1f };
	static const float Tz[] = { 1.4f, 2.0f, 2.25f, 1.95f, 1.45f, 0.95f, 0.5f, 0.16f };
	const int32 Around = 8;
	const float Jitter = bCrude ? 0.16f : 0.06f;
	const float Narrow = bCrude ? 0.86f : 1.f;
	TArray<TArray<FVector>> Body, Bit;
	for (int32 r = 0; r < 8; ++r)
	{
		TArray<FVector>& Ring = (r >= 6 ? Bit : Body).AddDefaulted_GetRef();
		for (int32 s = 0; s < Around; ++s)
		{
			const float A = 2.f * PI * (s + 0.5f) / Around;
			// Knapping: each facet corner sits in or out a little, the same every time.
			const float J = 1.f + Jitter * (Hash(r, s, bCrude ? 3.f : 7.f) - 0.5f) * 2.f;
			Ring.Add(At + FVector(FMath::Cos(A) * Hx[r] * Narrow * J, Ys[r], FMath::Sin(A) * Tz[r] * J) * S);
		}
		if (r == 6) Body.Add(Ring);   // the bit starts where the body ends (shared ring, no gap)
	}
	// Body: the stone; the bit: freshly flaked (crude) or ground smooth (stone), lighter and glossier.
	Loft(Body, Head, bCrude ? 0.85f : 0.72f, true);
	Loft(Bit, Head * (bCrude ? 1.75f : 1.45f), bCrude ? 0.55f : 0.32f, false);
	// Close the bit's edge with a sliver cap (the last ring).
	TArray<TArray<FVector>> Edge = { Bit.Last(), Bit.Last() };
	FVector Mid = FVector::ZeroVector;
	for (const FVector& P : Bit.Last()) Mid += P;
	Mid /= Bit.Last().Num();
	for (FVector& P : Edge[1]) P = FMath::Lerp(P, Mid, 0.98f) + FVector(0.f, 0.05f * S, 0.f);
	Loft(Edge, Head * (bCrude ? 1.9f : 1.6f), 0.3f, true);
}

void FWRToolMesh::PickHead(const FVector& At, float S, bool bCrude, const FLinearColor& Head)
{
	using namespace WRToolMeshDetail;
	// A bar across the haft, curving back toward the hand at both ends and drawn to points: rings along the arc.
	const int32 Steps = 13, Around = 6;
	const float Reach = 15.f, Sweep = 4.5f;
	TArray<TArray<FVector>> Rings;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float U = -1.f + 2.f * i / (Steps - 1);
		const float Thick = FMath::Max(0.12f, 2.5f * (1.f - FMath::Pow(FMath::Abs(U), 1.7f))) * (bCrude ? 0.9f : 1.f);
		const FVector C = At + FVector(-Sweep * U * U, Reach * U, 0.f) * S;
		// The ring lies across the arc's direction (dX/du, dY/du).
		const FVector Tan = FVector(-2.f * Sweep * U, Reach, 0.f).GetSafeNormal();
		const FVector Across = FVector::CrossProduct(Tan, FVector::ZAxisVector).GetSafeNormal();
		TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
		for (int32 s = 0; s < Around; ++s)
		{
			const float A = 2.f * PI * (s + 0.5f) / Around;
			const float J = 1.f + (bCrude ? 0.14f : 0.05f) * (Hash(i, s, 11.f) - 0.5f) * 2.f;
			Ring.Add(C + (Across * FMath::Cos(A) * Thick * 1.15f + FVector::ZAxisVector * FMath::Sin(A) * Thick) * S * J);
		}
	}
	Loft(Rings, Head, bCrude ? 0.82f : 0.7f, true);
	// The striking point (+Y) is ground bright.
	TArray<TArray<FVector>> Tip;
	for (int32 i = Steps - 3; i < Steps; ++i) Tip.Add(Rings[i]);
	for (TArray<FVector>& R : Tip) for (FVector& P : R) P += FVector(0.f, 0.02f, 0.f) * S;
	Loft(Tip, Head * 1.6f, 0.35f, true);
}

void FWRToolMesh::Lashing(const FVector& At, float HaftR, float Span, bool bCrude, bool bCross)
{
	// Rawhide (stone tools) or twisted plant fibre (crude): a tight coil on each side of the head, and strands that
	// cross over the head's flats from one coil to the other, lying on its faces rather than through them.
	const FLinearColor Cord = bCrude ? FLinearColor(0.42f, 0.36f, 0.2f) : FLinearColor(0.36f, 0.24f, 0.13f);
	const float R = bCrude ? 0.32f : 0.28f;
	const int32 Turns = bCrude ? 3 : 4;
	for (const float Side : { -1.f, 1.f })
	{
		TArray<FVector> Coil;
		const float X0 = At.X + Side * (Span * 0.5f + 0.6f);
		for (int32 k = 0; k <= Turns * 10; ++k)
		{
			const float A = 2.f * PI * k / 10.f;
			Coil.Add(FVector(X0 + Side * k * (R * 2.1f) / 10.f, At.Y + FMath::Cos(A) * (HaftR + R), At.Z + FMath::Sin(A) * (HaftR + R)));
		}
		Tube(Coil, R, 5, Cord, 0.92f);
	}
	if (!bCross) return;
	// Two strands a side (an X over each flat of the head).
	for (const float Face : { -1.f, 1.f })
	{
		for (const float Diag : { -1.f, 1.f })
		{
			TArray<FVector> Strand;
			for (int32 k = 0; k <= 8; ++k)
			{
				const float T = k / 8.f;
				const float X = At.X + (T - 0.5f) * (Span + 1.2f);
				const float Lift = FMath::Sin(PI * T);
				const float Z = At.Z + Face * (HaftR + R + Lift * 0.9f);
				Strand.Add(FVector(X, At.Y + Diag * (T - 0.5f) * HaftR * 1.6f, Z));
			}
			Tube(Strand, R * 0.9f, 4, Cord * 0.92f, 0.92f);
		}
	}
}

FVector FWRToolMesh::RodTip(float Length, float Back)
{
	// The wand droops 6 cm over its length (Lathe's negative bend): the tip ring sits there.
	return FVector(Length - Back, 0.f, -6.f + 1.0f);
}

FWRToolMesh FWRToolMesh::Tool(FName Kind, bool bCrude, const FLinearColor& Head, float Length, float Back, float Thick, float HeadScale)
{
	FWRToolMesh M;
	const FLinearColor Wood = bCrude ? FLinearColor(0.27f, 0.17f, 0.08f) : FLinearColor(0.21f, 0.12f, 0.055f);
	const float R = Thick * 0.5f;
	if (Kind == TEXT("rod"))
	{
		// A long wand of springy wood tapering to the tip, a hide-wrapped grip around the fist, a spool of wound line
		// just ahead of it, three guide rings the line runs through, and the tip ring it leaves from.
		const float L = Length;
		M.Lathe(FVector(-Back, 0.f, 0.f), FVector::XAxisVector, L,
			{ FVector(0.f, R * 0.95f, R * 0.95f), FVector(0.02f, R * 1.05f, R * 1.05f), FVector(0.3f, R * 0.8f, R * 0.8f), FVector(0.6f, R * 0.52f, R * 0.52f), FVector(0.9f, R * 0.3f, R * 0.3f), FVector(1.f, R * 0.22f, R * 0.22f) },
			10, Wood * 1.15f, 0.55f, -6.f, 5.f);
		// Grip: ridged wraps over the butt.
		TArray<FVector> Grip;
		for (int32 k = 0; k <= 14; ++k) Grip.Add(FVector(k / 14.f, (k % 2 ? 1.13f : 1.2f) * R, (k % 2 ? 1.13f : 1.2f) * R));
		M.Lathe(FVector(-Back + 2.f, 0.f, 0.f), FVector::XAxisVector, Back + 8.f, Grip, 10, FLinearColor(0.3f, 0.2f, 0.11f), 0.9f);
		// Spool: two wooden flanges with pale line wound between.
		const float SpoolX = 10.f;
		M.Lathe(FVector(SpoolX, 0.f, 0.f), FVector::XAxisVector, 0.8f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 3.f, 3.f) }, 14, Wood, 0.6f);
		M.Lathe(FVector(SpoolX + 4.8f, 0.f, 0.f), FVector::XAxisVector, 0.8f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 3.f, 3.f) }, 14, Wood, 0.6f);
		M.Lathe(FVector(SpoolX + 0.8f, 0.f, 0.f), FVector::XAxisVector, 4.f, { FVector(0.f, 2.3f, 2.3f), FVector(0.5f, 2.45f, 2.45f), FVector(1.f, 2.3f, 2.3f) }, 14, FLinearColor(0.62f, 0.56f, 0.42f), 0.85f);
		// Guide rings standing on top of the wand, and the tip ring.
		for (const float T : { 0.38f, 0.62f, 0.84f, 1.f })
		{
			const float X = -Back + T * L;
			const float Droop = -6.f * T * T;
			const float WandR = FMath::Lerp(R * 0.8f, R * 0.22f, FMath::Clamp((T - 0.3f) / 0.7f, 0.f, 1.f));
			const float RingR = T >= 1.f ? 0.55f : 0.8f;
			TArray<FVector> Ring;
			for (int32 k = 0; k < 10; ++k)
			{
				const float A = 2.f * PI * k / 10.f;
				Ring.Add(FVector(X, FMath::Cos(A) * RingR, Droop + WandR + RingR + FMath::Sin(A) * RingR));
			}
			M.Tube(Ring, 0.12f, 4, FLinearColor(0.2f, 0.2f, 0.21f), 0.4f, true);
		}
		return M;
	}
	const bool bPick = Kind == TEXT("pick");
	const float HeadX = Length - Back - 6.f * HeadScale;   // the head sits just short of the haft's end
	// The haft: a knob at the butt, a slimmer grip, swelling again toward the head; oval, deeper toward the strike.
	M.Lathe(FVector(-Back, 0.f, 0.f), FVector::XAxisVector, Length,
		{ FVector(0.f, R * 0.9f, R * 0.8f), FVector(0.025f, R * 1.35f, R * 1.15f), FVector(0.07f, R * 1.1f, R * 0.95f), FVector(0.15f, R * 1.0f, R * 0.85f),
		  FVector(0.5f, R * 1.0f, R * 0.84f), FVector(0.82f, R * 1.08f, R * 0.9f), FVector(0.96f, R * 1.12f, R * 0.95f), FVector(1.f, R * 0.85f, R * 0.75f) },
		12, Wood, bCrude ? 0.75f : 0.6f, 0.8f, bCrude ? 3.f : 9.f);
	const FVector HeadAt(HeadX, 0.f, 0.f);
	if (bPick)
	{
		M.PickHead(HeadAt, HeadScale, bCrude, Head);
		M.Lashing(HeadAt, R * 1.08f, 5.6f * HeadScale, bCrude, true);
	}
	else
	{
		M.HatchetHead(HeadAt, HeadScale, bCrude, Head);
		M.Lashing(HeadAt, R * 1.1f, 8.2f * HeadScale, bCrude, true);
	}
	return M;
}

FWRToolMesh FWRToolMesh::Float()
{
	// A carved float: a keel stick under, a pale spindle body, a red cap over the waterline with a dark band, a quill.
	FWRToolMesh M;
	M.Lathe(FVector(0.f, 0.f, -6.5f), FVector::ZAxisVector, 3.f, { FVector(0.f, 0.18f, 0.18f), FVector(1.f, 0.22f, 0.22f) }, 6, FLinearColor(0.22f, 0.14f, 0.07f), 0.6f);
	M.Lathe(FVector(0.f, 0.f, -3.6f), FVector::ZAxisVector, 3.6f,
		{ FVector(0.f, 0.25f, 0.25f), FVector(0.35f, 1.0f, 1.0f), FVector(0.75f, 1.5f, 1.5f), FVector(1.f, 1.6f, 1.6f) }, 12, FLinearColor(0.78f, 0.7f, 0.52f), 0.45f);
	M.Lathe(FVector(0.f, 0.f, 0.f), FVector::ZAxisVector, 0.45f, { FVector(0.f, 1.62f, 1.62f), FVector(1.f, 1.6f, 1.6f) }, 12, FLinearColor(0.03f, 0.025f, 0.02f), 0.5f);
	M.Lathe(FVector(0.f, 0.f, 0.45f), FVector::ZAxisVector, 3.2f,
		{ FVector(0.f, 1.6f, 1.6f), FVector(0.3f, 1.45f, 1.45f), FVector(0.7f, 0.9f, 0.9f), FVector(1.f, 0.22f, 0.22f) }, 12, FLinearColor(0.62f, 0.04f, 0.02f), 0.35f);
	M.Lathe(FVector(0.f, 0.f, 3.6f), FVector::ZAxisVector, 2.2f, { FVector(0.f, 0.16f, 0.16f), FVector(1.f, 0.08f, 0.08f) }, 5, FLinearColor(0.85f, 0.82f, 0.75f), 0.4f);
	return M;
}

UMaterialInterface* FWRToolMesh::Material()
{
	// Built by Tools/art/make_held_tool_material.py (engine content only). Rooted: tools and floats come and go.
	static UMaterialInterface* Mat = nullptr;
	if (!Mat)
	{
		Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Wildreach/Materials/M_HeldTool.M_HeldTool"));
		if (!Mat)
		{
			UE_LOG(LogWildreach, Warning, TEXT("TOOLS M_HeldTool is missing (Tools/art/make_held_tool_material.py): tools draw in the plain shape material"));
			Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (Mat) Mat->AddToRoot();
	}
	return Mat;
}

void FWRToolMesh::ApplyTo(UProceduralMeshComponent* Comp) const
{
	if (!Comp) return;
	Comp->ClearAllMeshSections();
	Comp->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
	Comp->SetMaterial(0, Material());
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(true);
}
