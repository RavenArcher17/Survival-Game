// The dropped bags, modelled (cooks/cookpass-multiplayer.md 3.5; real-world: a traveller's pack-roll, the hide satchel
// with a flap and bone toggle found at Hedeby, a nailed board crate that bursts along its nails). Measurements in cm.
#include "WRLootLook.h"

namespace WRLootLookKit
{
	// Linear colours (M_HeldTool), then roughness.
	const FLinearColor Fur(0.11f, 0.07f, 0.04f);
	const FLinearColor FurLight(0.26f, 0.2f, 0.13f);
	const FLinearColor Wool(0.42f, 0.36f, 0.27f);
	const FLinearColor Hide(0.2f, 0.11f, 0.05f);
	const FLinearColor HideDark(0.12f, 0.065f, 0.03f);
	const FLinearColor Strap(0.07f, 0.04f, 0.02f);
	const FLinearColor Bronze(0.45f, 0.28f, 0.1f);
	const FLinearColor Bone(0.62f, 0.56f, 0.42f);
	const FLinearColor Board(0.3f, 0.19f, 0.09f);
	const FLinearColor BoardGrey(0.27f, 0.23f, 0.17f);
	const FLinearColor Nail(0.05f, 0.05f, 0.055f);
	const FLinearColor Burlap(0.36f, 0.27f, 0.14f);
	const FLinearColor Cord(0.3f, 0.23f, 0.13f);
	const FLinearColor Cork(0.33f, 0.21f, 0.1f);

	/** A flat-faced box: centre, half sizes, turned by Rot. */
	void Box(FWRToolMesh& M, const FVector& Centre, const FVector& Half, const FRotator& Rot, const FLinearColor& Colour, float Rough)
	{
		const FQuat Q = Rot.Quaternion();
		TArray<TArray<FVector>> Rings;
		for (const float X : { -Half.X, Half.X })
		{
			TArray<FVector>& R = Rings.AddDefaulted_GetRef();
			R.Add(Centre + Q.RotateVector(FVector(X, -Half.Y, -Half.Z)));
			R.Add(Centre + Q.RotateVector(FVector(X, Half.Y, -Half.Z)));
			R.Add(Centre + Q.RotateVector(FVector(X, Half.Y, Half.Z)));
			R.Add(Centre + Q.RotateVector(FVector(X, -Half.Y, Half.Z)));
		}
		M.Loft(Rings, Colour, Rough, true);
	}

	/** A rounded-cornered block (a stuffed satchel): its cross-section a rounded rectangle in YZ, lofted along X with the
	 *  ends drawn in a little (a sewn bag is never square at its ends). */
	void SoftBlock(FWRToolMesh& M, const FVector& Centre, const FVector& Half, float Corner, float Yaw, float Pitch, const FLinearColor& Colour, float Rough)
	{
		const FQuat Q = FRotator(Pitch, Yaw, 0.f).Quaternion();
		TArray<TArray<FVector>> Rings;
		const float Xs[] = { -1.f, -0.9f, 0.9f, 1.f };
		const float Shrink[] = { 0.86f, 1.f, 1.f, 0.86f };
		for (int32 i = 0; i < 4; ++i)
		{
			TArray<FVector>& R = Rings.AddDefaulted_GetRef();
			const float HY = Half.Y * Shrink[i], HZ = Half.Z * Shrink[i];
			const float C = FMath::Min(Corner, FMath::Min(HY, HZ) * 0.9f);
			// Four corners, each a quarter circle in 3 steps.
			const FVector2D Mids[] = { FVector2D(HY - C, HZ - C), FVector2D(-(HY - C), HZ - C), FVector2D(-(HY - C), -(HZ - C)), FVector2D(HY - C, -(HZ - C)) };
			for (int32 k = 0; k < 4; ++k)
			{
				for (int32 j = 0; j <= 3; ++j)
				{
					const float A = (k * 90.f + j * 30.f) * PI / 180.f;
					const FVector2D P = Mids[k] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * C;
					// The top sags in the middle (a bag, not a box).
					const float Sag = P.Y > 0.f ? -1.2f * (1.f - FMath::Abs(Xs[i])) : 0.f;
					R.Add(Centre + Q.RotateVector(FVector(Xs[i] * Half.X, P.X, P.Y + Sag)));
				}
			}
		}
		M.Loft(Rings, Colour, Rough, true);
	}

	/** A closed ring round an axis (a strap round the roll). */
	TArray<FVector> Ring(const FVector& Centre, const FVector& Axis, float RY, float RZ, int32 N)
	{
		FVector AY = FVector::CrossProduct(Axis, FVector::UpVector);
		if (AY.IsNearlyZero()) AY = FVector::RightVector;
		AY.Normalize();
		const FVector AZ = FVector::CrossProduct(AY, Axis).GetSafeNormal();
		TArray<FVector> P;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 2.f * PI * i / N;
			P.Add(Centre + AY * FMath::Cos(A) * RY + AZ * FMath::Sin(A) * RZ);
		}
		return P;
	}

	/** A round thing (a lump of ore, a turnip) as a little lathe: R across, H tall, on the ground at At. */
	void Lump(FWRToolMesh& M, const FVector& At, float R, float H, const FLinearColor& Colour, float Rough, float Seed)
	{
		M.Lathe(At, FVector::UpVector, H, { FVector(0.f, R * 0.3f, R * 0.3f), FVector(0.2f, R * 0.85f, R * 0.8f), FVector(0.55f, R, R * 0.95f), FVector(0.85f, R * 0.7f, R * 0.75f), FVector(1.f, 0.f, 0.f) },
			7, Colour, Rough, 0.f, Seed);
	}
}

FWRToolMesh WRLootLook::Satchel()
{
	using namespace WRLootLookKit;
	FWRToolMesh M;
	// The bedroll: 70 cm of rolled fur along X, 13 cm across, lying on the ground.
	const float RollR = 12.5f;
	const FVector RollStart(-35.f, -6.f, RollR);
	M.Lathe(RollStart, FVector::ForwardVector, 70.f,
		{ FVector(0.f, RollR * 0.72f, RollR * 0.7f), FVector(0.025f, RollR * 0.95f, RollR * 0.93f), FVector(0.08f, RollR, RollR * 0.98f),
		  FVector(0.5f, RollR * 1.04f, RollR * 1.0f), FVector(0.92f, RollR, RollR * 0.98f), FVector(0.975f, RollR * 0.95f, RollR * 0.93f), FVector(1.f, RollR * 0.72f, RollR * 0.7f) },
		16, Fur, 0.95f, 0.f, 3.f);
	// The roll's layers at both ends: a spiral of the wool lining between the fur turns.
	for (const float EndX : { -35.4f, 35.4f })
	{
		TArray<FVector> Spiral;
		for (int32 i = 0; i <= 40; ++i)
		{
			const float T = i / 40.f;
			const float A = T * 4.f * PI;
			const float R = 1.5f + T * (RollR * 0.62f);
			Spiral.Add(FVector(EndX, RollStart.Y + FMath::Cos(A) * R, RollStart.Z + FMath::Sin(A) * R));
		}
		M.Tube(Spiral, 0.75f, 5, Wool, 0.9f);
	}
	// Two straps round it, each with a bronze buckle on top.
	for (const float SX : { -20.f, 19.f })
	{
		M.Tube(Ring(FVector(SX, RollStart.Y, RollStart.Z), FVector::ForwardVector, RollR + 0.9f, RollR + 0.6f, 20), 1.1f, 4, Strap, 0.7f, true);
		Box(M, FVector(SX, RollStart.Y - 2.f, RollStart.Z + RollR + 1.3f), FVector(1.6f, 2.2f, 0.5f), FRotator::ZeroRotator, Bronze, 0.35f);
		Box(M, FVector(SX, RollStart.Y - 2.f, RollStart.Z + RollR + 1.9f), FVector(0.4f, 1.6f, 0.25f), FRotator::ZeroRotator, Bronze, 0.3f);
	}
	// The strap that came loose: from the buckle down the roll's far side and out across the ground.
	M.Tube({ FVector(19.f, RollStart.Y - 4.f, RollStart.Z + RollR + 0.8f), FVector(20.f, RollStart.Y - RollR - 1.f, RollStart.Z + 4.f),
		FVector(22.f, RollStart.Y - RollR - 6.f, 1.4f), FVector(30.f, RollStart.Y - RollR - 14.f, 1.1f), FVector(41.f, RollStart.Y - RollR - 17.f, 1.1f) }, 1.1f, 4, Strap, 0.7f);
	// The satchel slumped against the roll's near side: 34 x 24 x 13, its back leaning on the fur.
	const FVector Bag(3.f, 17.5f, 7.f);
	SoftBlock(M, Bag, FVector(17.f, 11.5f, 7.f), 4.f, 0.f, 0.f, Hide, 0.8f);
	// Its flap: over the top and down the front, a little proud of the body, and the bone toggle that holds it.
	Box(M, Bag + FVector(0.f, -1.f, 7.4f), FVector(16.f, 10.8f, 0.5f), FRotator(0.f, 0.f, -3.f), HideDark, 0.75f);
	Box(M, Bag + FVector(0.f, 11.8f, 3.2f), FVector(15.f, 0.5f, 4.6f), FRotator(0.f, 0.f, 4.f), HideDark, 0.75f);
	M.Lathe(Bag + FVector(-3.f, 12.6f, 0.5f), FVector::ForwardVector, 6.f, { FVector(0.f, 0.5f, 0.5f), FVector(0.2f, 0.9f, 0.9f), FVector(0.8f, 0.9f, 0.9f), FVector(1.f, 0.5f, 0.5f) }, 6, Bone, 0.5f);
	M.Tube({ Bag + FVector(0.f, 12.4f, 0.6f), Bag + FVector(0.f, 12.1f, 3.f), Bag + FVector(0.f, 12.3f, 5.2f) }, 0.35f, 4, Strap, 0.7f);
	// Its carrying strap trailing over the top of the roll.
	M.Tube({ Bag + FVector(-14.f, -9.f, 6.f), FVector(-14.f, RollStart.Y + 2.f, RollStart.Z + RollR + 1.2f), FVector(-13.f, RollStart.Y - RollR + 2.f, RollStart.Z + 5.f),
		FVector(-12.f, RollStart.Y - RollR - 4.f, 1.2f) }, 1.f, 4, Strap, 0.7f);
	// Spilled beside it: a cord-tied pouch, and the waterskin with its stopper.
	{
		const FVector At(-23.f, 33.f, 0.f);
		M.Lathe(At, FVector::UpVector, 11.f, { FVector(0.f, 3.f, 3.f), FVector(0.15f, 5.4f, 5.f), FVector(0.45f, 6.f, 5.6f), FVector(0.72f, 4.6f, 4.3f), FVector(0.82f, 2.2f, 2.2f), FVector(0.9f, 2.8f, 2.8f), FVector(1.f, 3.2f, 3.2f) },
			10, Hide, 0.85f, 0.f, 5.f);
		M.Tube(Ring(At + FVector(0.f, 0.f, 9.f), FVector::UpVector, 2.5f, 2.5f, 10), 0.4f, 4, Cord, 0.9f, true);
		M.Tube({ At + FVector(2.4f, 0.f, 9.f), At + FVector(5.f, 1.f, 6.f), At + FVector(8.f, 2.f, 0.6f) }, 0.35f, 4, Cord, 0.9f);
	}
	{
		const FVector Axis = FVector(0.85f, -0.4f, 0.f).GetSafeNormal();
		const FVector Start(-30.f, -26.f, 5.f);
		M.Lathe(Start, Axis, 25.f, { FVector(0.f, 4.f, 2.6f), FVector(0.12f, 7.f, 4.6f), FVector(0.5f, 7.6f, 5.f), FVector(0.8f, 5.5f, 3.6f), FVector(0.92f, 2.f, 1.8f), FVector(1.f, 2.f, 1.8f) },
			12, HideDark, 0.7f, 0.f, 7.f);
		M.Lathe(Start + Axis * 25.f, Axis, 3.5f, { FVector(0.f, 1.6f, 1.6f), FVector(0.5f, 1.9f, 1.9f), FVector(1.f, 1.7f, 1.7f) }, 8, Cork, 0.9f);
	}
	return M;
}

FWRToolMesh WRLootLook::Crate()
{
	using namespace WRLootLookKit;
	FWRToolMesh M;
	// 70 x 50 x 44, of boards 2 cm thick nailed to corner posts; it lies where the chest stood.
	const float L = 35.f, W = 25.f, H = 44.f, T = 1.f;
	// Corner posts.
	for (const float X : { -L + 2.f, L - 2.f }) for (const float Y : { -W + 2.f, W - 2.f })
	{
		Box(M, FVector(X, Y, H * 0.5f), FVector(2.f, 2.f, H * 0.5f), FRotator::ZeroRotator, BoardGrey, 0.85f);
	}
	// The floor: five boards along X, each a shade apart.
	for (int32 i = 0; i < 5; ++i)
	{
		const float Y = -W + 5.f + i * 10.f;
		Box(M, FVector(0.f, Y, T), FVector(L, 4.7f, T), FRotator(0.f, 0.f, 0.f), i % 2 ? Board : BoardGrey, 0.85f);
	}
	// The back wall and the two ends: four boards each, the nail heads at the posts.
	for (int32 i = 0; i < 4; ++i)
	{
		const float Z = 6.f + i * 11.f;
		Box(M, FVector(0.f, -W, Z), FVector(L, T, 5.2f), FRotator::ZeroRotator, i % 2 ? BoardGrey : Board, 0.85f);
		for (const float X : { -L, L })
		{
			Box(M, FVector(X, 0.f, Z), FVector(T, W, 5.2f), FRotator::ZeroRotator, i % 2 ? Board : BoardGrey, 0.85f);
			for (const float Y : { -W + 2.f, W - 2.f }) Box(M, FVector(X + FMath::Sign(X) * 1.1f, Y, Z), FVector(0.25f, 0.6f, 0.6f), FRotator::ZeroRotator, Nail, 0.4f);
		}
		for (const float X : { -L + 2.f, L - 2.f }) Box(M, FVector(X, -W - 1.1f, Z), FVector(0.6f, 0.25f, 0.6f), FRotator::ZeroRotator, Nail, 0.4f);
	}
	// The front: the top board still on, one hanging from a nail, two out on the ground (one split short).
	Box(M, FVector(0.f, W, 39.f), FVector(L, T, 5.2f), FRotator::ZeroRotator, Board, 0.85f);
	Box(M, FVector(-14.f, W + 3.f, 22.f), FVector(L * 0.62f, T, 5.2f), FRotator(-24.f, 0.f, -18.f), BoardGrey, 0.85f);
	Box(M, FVector(4.f, W + 22.f, 1.2f), FVector(L, 5.2f, T), FRotator(0.f, 14.f, 0.f), Board, 0.85f);
	Box(M, FVector(-20.f, W + 33.f, 1.2f), FVector(L * 0.45f, 5.2f, T), FRotator(0.f, -31.f, 0.f), BoardGrey, 0.85f);
	// The lid, knocked off: leaning on the back wall's outside, its cleats showing.
	Box(M, FVector(4.f, -W - 12.f, 20.f), FVector(L + 1.f, 1.2f, W), FRotator(0.f, 0.f, -26.f), BoardGrey, 0.85f);
	// A burlap sack slumped against the near end, tied at the neck, and what rolled out in front.
	{
		const FVector At(L + 19.f, 6.f, 0.f);
		M.Lathe(At, FVector(-0.2f, 0.f, 1.f).GetSafeNormal(), 34.f,
			{ FVector(0.f, 10.f, 9.f), FVector(0.1f, 14.f, 12.5f), FVector(0.45f, 15.f, 13.f), FVector(0.72f, 11.f, 10.f), FVector(0.84f, 3.f, 3.f), FVector(0.92f, 4.2f, 4.f), FVector(1.f, 3.6f, 3.4f) },
			12, Burlap, 0.95f, 2.f, 11.f);
		M.Tube(Ring(At + FVector(-0.2f, 0.f, 1.f).GetSafeNormal() * 28.5f, FVector(-0.2f, 0.f, 1.f).GetSafeNormal(), 3.4f, 3.4f, 10), 0.5f, 4, Cord, 0.9f, true);
	}
	Lump(M, FVector(8.f, W + 10.f, 0.f), 3.5f, 5.f, FLinearColor(0.18f, 0.17f, 0.16f), 0.9f, 13.f);
	Lump(M, FVector(-6.f, W + 15.f, 0.f), 2.8f, 4.f, FLinearColor(0.32f, 0.2f, 0.09f), 0.8f, 17.f);
	Lump(M, FVector(18.f, W + 6.f, 0.f), 2.4f, 3.5f, FLinearColor(0.22f, 0.24f, 0.2f), 0.9f, 19.f);
	return M;
}

FWRToolMesh WRLootLook::Sack()
{
	using namespace WRLootLookKit;
	FWRToolMesh M;
	// A hide sack, 40 cm tall, slouched (it leans toward +X), tied at the neck, its top flopped over.
	const FVector Axis = FVector(0.18f, 0.06f, 1.f).GetSafeNormal();
	M.Lathe(FVector::ZeroVector, Axis, 40.f,
		{ FVector(0.f, 11.f, 10.f), FVector(0.08f, 15.f, 14.f), FVector(0.4f, 16.f, 15.f), FVector(0.68f, 12.f, 11.f), FVector(0.8f, 3.4f, 3.4f), FVector(0.87f, 4.8f, 4.6f), FVector(1.f, 5.5f, 3.2f) },
		14, Hide, 0.85f, 3.f, 23.f);
	M.Tube(Ring(Axis * 32.5f, Axis, 3.8f, 3.8f, 10), 0.55f, 4, Cord, 0.9f, true);
	M.Tube({ Axis * 32.5f + FVector(3.6f, 0.f, 0.f), FVector(8.f, 2.f, 26.f), FVector(11.f, 3.f, 18.f) }, 0.45f, 4, Cord, 0.9f);
	// A rolled fur bundle beside it, tied once.
	M.Lathe(FVector(-6.f, 20.f, 7.f), FVector(1.f, 0.25f, 0.f).GetSafeNormal(), 30.f,
		{ FVector(0.f, 5.f, 5.f), FVector(0.06f, 7.f, 6.8f), FVector(0.5f, 7.4f, 7.f), FVector(0.94f, 7.f, 6.8f), FVector(1.f, 5.f, 5.f) }, 12, Fur, 0.95f, 0.f, 29.f);
	M.Tube(Ring(FVector(-6.f, 20.f, 7.f) + FVector(1.f, 0.25f, 0.f).GetSafeNormal() * 15.f, FVector(1.f, 0.25f, 0.f).GetSafeNormal(), 7.8f, 7.6f, 14), 0.7f, 4, Strap, 0.7f, true);
	return M;
}

FWRToolMesh WRLootLook::Beacon(float Height)
{
	FWRToolMesh M;
	// Wider at the foot, a thread at the top; the glow material fades it.
	M.Lathe(FVector::ZeroVector, FVector::UpVector, Height,
		{ FVector(0.f, 22.f, 22.f), FVector(0.08f, 14.f, 14.f), FVector(0.4f, 9.f, 9.f), FVector(0.8f, 5.f, 5.f), FVector(1.f, 0.5f, 0.5f) }, 16, FLinearColor::White, 0.f);
	return M;
}
