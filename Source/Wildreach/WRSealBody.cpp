#include "WRSealBody.h"
#include "WRCreature.h"
#include "WRToolMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace WRSealDetail
{
	constexpr float TailX = -150.f, NoseX = 168.f, RMax = 38.f;
	constexpr int32 Rings = 30, Around = 20;

	FColor Pack(const FLinearColor& C, float Rough)
	{
		return FColor(FMath::Clamp(FMath::RoundToInt(C.R * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(C.G * 255.f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(C.B * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Rough * 255.f), 0, 255));
	}

	/** A cheap repeatable 0..1 noise for the coat's dapples. */
	float Hash(float A, float B)
	{
		const float S = FMath::Sin(A * 127.1f + B * 311.7f) * 43758.5453f;
		return S - FMath::FloorToFloat(S);
	}

	/** The coat at a point of the body: dark back, silver flanks dappled darker, pale belly, the harp over the shoulders
	 *  and hips, a dark muzzle. */
	FLinearColor Coat(float T, float Angle)
	{
		const float Up = FMath::Cos(Angle);
		const FLinearColor Silver(0.4f, 0.42f, 0.44f), Back(0.22f, 0.23f, 0.25f), Belly(0.74f, 0.73f, 0.68f), Harp(0.06f, 0.06f, 0.07f);
		FLinearColor C = Up > 0.f ? FMath::Lerp(Silver, Back, FMath::Pow(Up, 2.f)) : FMath::Lerp(Silver, Belly, FMath::SmoothStep(0.15f, 0.7f, -Up));
		// The harp: two dark bands from the shoulders' sides curving back to meet over the hips.
		if (T > 0.3f && T < 0.74f && Up > -0.2f)
		{
			const float Centre = FMath::Lerp(0.3f, 1.25f, (T - 0.3f) / 0.44f);
			const float Off = FMath::Abs(FMath::Abs(FMath::UnwindRadians(Angle)) - Centre);
			C = FMath::Lerp(C, Harp, 1.f - FMath::SmoothStep(0.12f, 0.26f, Off));
		}
		// A dark face and muzzle.
		if (T > 0.86f && Up > -0.35f) C = FMath::Lerp(C, Harp * 1.6f, FMath::SmoothStep(0.86f, 0.93f, T) * 0.85f);
		// Dapples on the flanks and back.
		if (Up > -0.3f && Hash(T * 9.f, Angle * 2.3f) > 0.84f) C *= 0.62f;
		return C;
	}
}
using namespace WRSealDetail;

UWRSealBody::UWRSealBody(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bUseAsyncCooking = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);
}

void UWRSealBody::Init(AWRCreature* InOwner)
{
	Creature = InOwner;
	Build();
}

void UWRSealBody::CarrySaddle(USceneComponent* Tack)
{
	if (Tack) Carried.AddUnique(Tack);
}

void UWRSealBody::Flinch()
{
	FlinchLeft = 0.35f;
}

void UWRSealBody::Die()
{
	bDead = true;
}

float UWRSealBody::Radius(float T) const
{
	// The profile from the hind flippers' root to the nose (share of RMax): hips, chest, neck, head, muzzle.
	static const FVector2D P[] = { {0.f, 0.08f}, {0.08f, 0.24f}, {0.2f, 0.5f}, {0.36f, 0.82f}, {0.55f, 1.f}, {0.7f, 0.95f}, {0.79f, 0.74f},
		{0.85f, 0.66f}, {0.91f, 0.6f}, {0.965f, 0.38f}, {1.f, 0.1f} };
	for (int32 i = 0; i + 1 < UE_ARRAY_COUNT(P); ++i)
	{
		if (T <= P[i + 1].X)
		{
			const float A = (T - P[i].X) / FMath::Max(0.001f, P[i + 1].X - P[i].X);
			return RMax * FMath::Lerp(P[i].Y, P[i + 1].Y, FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(A, 0.f, 1.f), 2.f));
		}
	}
	return RMax * 0.1f;
}

void UWRSealBody::Build()
{
	Rest.Reset();
	Tris.Reset();
	UVs.Reset();
	Colors.Reset();
	const auto Add = [this](float T, const FVector& Off, const FVector& N, const FColor& Col, const FVector2D& UV, uint8 Part = 0, const FVector& Pivot = FVector::ZeroVector)
	{
		FVert V;
		V.T = T;
		V.Off = Off;
		V.Normal = N;
		V.Part = Part;
		V.Pivot = Pivot;
		Rest.Add(V);
		Colors.Add(Col);
		UVs.Add(UV);
		return Rest.Num() - 1;
	};
	// Winding as M_HeldTool's meshes: the engine draws the side the cross product points away from.
	const auto Tri = [this](int32 A, int32 B, int32 C, const FVector& Out)
	{
		const FVector N = FVector::CrossProduct(Rest[B].Off - Rest[A].Off + FVector((Rest[B].T - Rest[A].T) * (NoseX - TailX), 0.f, 0.f),
			Rest[C].Off - Rest[A].Off + FVector((Rest[C].T - Rest[A].T) * (NoseX - TailX), 0.f, 0.f));
		if (FVector::DotProduct(N, Out) > 0.f) Tris.Append({ A, C, B });
		else Tris.Append({ A, B, C });
	};

	// --- The body: rings round the spine, smooth, its coat in vertex colour (fur grain along U = along the body).
	const auto Section = [this](float T, float A, float& OutY, float& OutZ, FVector& OutN)
	{
		const float R = Radius(T);
		const float W = T > 0.84f ? 1.0f : 1.12f, H = T > 0.84f ? 0.96f : 0.9f;
		const float S = FMath::Sin(A), C = FMath::Cos(A);
		// A flatter belly (it lies on it), a rounder back.
		const float Zc = C >= 0.f ? C : -FMath::Pow(-C, 0.7f) * 0.82f;
		OutY = R * W * S;
		OutZ = R * H * Zc;
		OutN = FVector(0.f, S / W, Zc / H).GetSafeNormal();
	};
	int32 Base = Rest.Num();
	for (int32 r = 0; r < Rings; ++r)
	{
		const float T = r / float(Rings - 1);
		for (int32 a = 0; a <= Around; ++a)
		{
			const float A = 2.f * PI * a / Around;
			float Y, Z;
			FVector N;
			Section(T, A, Y, Z, N);
			Add(T, FVector(0.f, Y, Z), N, Pack(Coat(T, A), 0.62f), FVector2D(T * 6.f, a / float(Around)));
		}
	}
	for (int32 r = 0; r + 1 < Rings; ++r)
	{
		for (int32 a = 0; a < Around; ++a)
		{
			const int32 I0 = Base + r * (Around + 1) + a, I1 = I0 + 1, J0 = I0 + Around + 1, J1 = J0 + 1;
			const FVector Out = Rest[I0].Normal + Rest[J1].Normal;
			Tri(I0, J0, J1, Out);
			Tri(I0, J1, I1, Out);
		}
	}
	// Caps: the nose's tip and the tail root.
	for (const int32 r : { 0, Rings - 1 })
	{
		const float T = r / float(Rings - 1);
		const int32 Tip = Add(T, FVector(r == 0 ? -6.f : 4.f, 0.f, 0.f), FVector(r == 0 ? -1.f : 1.f, 0.f, 0.f), Pack(Coat(T, 0.f) * 0.7f, 0.5f), FVector2D(T * 6.f, 0.5f));
		for (int32 a = 0; a < Around; ++a)
		{
			const int32 I0 = Base + r * (Around + 1) + a;
			Tri(Tip, I0, I0 + 1, FVector(r == 0 ? -1.f : 1.f, 0.f, 0.f));
		}
	}

	// --- Parts: built with the tool kit's builders in the spine's frame at their T, then stored against it.
	const auto Absorb = [this, &Add](const FWRToolMesh& M, float T, uint8 Part, const FVector& Pivot)
	{
		const int32 First = Rest.Num();
		for (int32 i = 0; i < M.Verts.Num(); ++i) Add(T, M.Verts[i], M.Normals[i], M.Colors[i], M.UVs[i], Part, Pivot);
		// The tool kit's triangles are already wound for the engine.
		for (int32 i = 0; i < M.Tris.Num(); ++i) Tris.Add(First + M.Tris[i]);
	};

	// The floe: seven thick slabs of sea ice grown along the back from the hips to the shoulders, glassy, rimed white at
	// their edges where the frost has built up. A rider sits on the middle ones.
	for (int32 k = 0; k < 7; ++k)
	{
		const float T = FMath::Lerp(0.36f, 0.73f, k / 6.f);
		const float R = Radius(T) * 0.9f;
		const float Size = FMath::Lerp(15.f, 21.f, FMath::Sin(k / 6.f * PI)) * (0.9f + 0.2f * Hash(k, 3.f));
		const float Thick = 6.f + 3.f * Hash(k, 7.f);
		const float Side = (k % 2 ? 1.f : -1.f) * 4.f * Hash(k, 9.f);
		const float Tilt = (Hash(k, 11.f) - 0.5f) * 0.25f;
		TArray<TArray<FVector>> Slab;
		for (const float Z : { 0.f, Thick * 0.8f, Thick })
		{
			TArray<FVector> Ring;
			const float Shrink = Z >= Thick ? 0.82f : 1.f;   // a bevelled top edge
			for (int32 s = 0; s < 6; ++s)
			{
				const float A = s * PI / 3.f + Hash(k, s) * 0.3f;
				const float Rr = Size * Shrink * (0.85f + 0.3f * Hash(k * 7.f, s * 3.f));
				Ring.Add(FVector(FMath::Cos(A) * Rr * 1.15f, Side + FMath::Sin(A) * Rr, R * 0.92f + Z + FMath::Cos(A) * Rr * Tilt));
			}
			Slab.Add(Ring);
		}
		// Its glassy top: the rim's ring drawn in toward the middle a little above it.
		TArray<FVector> Inner = Slab[2];
		FVector Mid = FVector::ZeroVector;
		for (const FVector& P : Inner) Mid += P / 6.f;
		for (FVector& P : Inner) P = Mid + (P - Mid) * 0.55f + FVector(0.f, 0.f, 0.8f);
		FWRToolMesh Ice;
		Ice.Loft({ Slab[0], Slab[1] }, FLinearColor(0.5f, 0.68f, 0.82f), 0.12f, true);    // the clear sides
		Ice.Loft({ Slab[1], Slab[2] }, FLinearColor(0.88f, 0.93f, 0.98f), 0.6f, false);   // the rimed bevel
		Ice.Loft({ Slab[2], Inner }, FLinearColor(0.66f, 0.82f, 0.94f), 0.1f, true);      // the glassy top
		Absorb(Ice, T, 0, FVector::ZeroVector);
	}

	// Fore flippers: from low on the chest's sides, paddles angled back and down, five dark claws at the end.
	for (const float Side : { -1.f, 1.f })
	{
		const float T = 0.63f;
		const float R = Radius(T);
		const FVector Root(0.f, Side * R * 0.95f, -R * 0.42f);
		const FVector Dir = FVector(-0.4f, Side * 0.3f, -0.86f).GetSafeNormal();
		FWRToolMesh F;
		F.Lathe(Root, Dir, 40.f, { FVector(0.f, 9.f, 7.f), FVector(0.35f, 8.5f, 4.5f), FVector(0.75f, 8.f, 3.f), FVector(0.95f, 6.f, 2.2f), FVector(1.f, 2.f, 1.f) }, 10, FLinearColor(0.3f, 0.31f, 0.33f), 0.6f);
		for (int32 c = 0; c < 5; ++c)
		{
			const FVector Tip = Root + Dir * 37.f + FVector(0.f, 0.f, 0.f) + FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal() * (c - 2) * 2.6f;
			F.Tube({ Tip, Tip + Dir * 3.5f + FVector(0.f, 0.f, -1.f) }, 0.55f, 4, FLinearColor(0.03f, 0.03f, 0.03f), 0.35f);
		}
		Absorb(F, T, Side < 0.f ? 1 : 2, Root);
	}

	// Hind flippers: two broad fans behind the hips, webbed between five dark-lined digits.
	for (const float Side : { -1.f, 1.f })
	{
		const FVector Root(-2.f, Side * 5.f, 0.f);
		const FVector Dir = FVector(-1.f, Side * 0.32f, -0.05f).GetSafeNormal();
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
		TArray<TArray<FVector>> Grid;
		for (int32 r = 0; r < 6; ++r)
		{
			const float U = r / 5.f;
			const float Half = FMath::Lerp(5.f, 14.f, FMath::Pow(U, 0.8f));
			TArray<FVector> Row;
			for (int32 c = 0; c < 7; ++c)
			{
				const float V = c / 6.f * 2.f - 1.f;
				// The trailing edge scalloped between the digits.
				const float Len = 46.f * U - (U > 0.95f ? 3.f * FMath::Abs(FMath::Sin(V * PI * 2.5f)) : 0.f);
				Row.Add(Root + Dir * Len + Across * V * Half + FVector(0.f, 0.f, (1.f - V * V) * 1.5f));
			}
			Grid.Add(Row);
		}
		FWRToolMesh H;
		TArray<TArray<FVector>> Under = Grid;
		for (TArray<FVector>& Row : Under) for (FVector& P : Row) P.Z -= 2.5f;
		for (int32 r = 0; r + 1 < Grid.Num(); ++r) H.Loft({ Grid[r], Grid[r + 1], Under[r + 1], Under[r] }, FLinearColor(0.27f, 0.28f, 0.3f), 0.6f, false);
		for (int32 d = 0; d < 5; ++d)
		{
			const float V = d / 4.f * 2.f - 1.f;
			H.Tube({ Root + Dir * 6.f + Across * V * 5.f + FVector(0.f, 0.f, 1.8f), Root + Dir * 44.f + Across * V * 13.f + FVector(0.f, 0.f, 1.8f) }, 0.7f, 4, FLinearColor(0.1f, 0.1f, 0.11f), 0.55f);
		}
		Absorb(H, 0.02f, Side < 0.f ? 3 : 4, Root);
	}

	// The head: big dark eyes set in, nostrils on top of the muzzle, the mouth's line, and its whiskers (each part
	// stored at its own place along the spine, so the neck's bend carries them).
	{
		FWRToolMesh Eyes, Nose, Mouth;
		const float TE = 0.9f, RE = Radius(TE);
		for (const float Side : { -1.f, 1.f })
		{
			const FVector N = FVector(0.15f, Side * FMath::Sin(0.8f), FMath::Cos(0.8f)).GetSafeNormal();
			const FVector At = FVector(0.f, Side * RE * FMath::Sin(0.8f), RE * 0.96f * FMath::Cos(0.8f)) - N * 1.5f;
			TArray<FVector> Eye;
			for (int32 i = 0; i <= 8; ++i) { const float A = PI * i / 8.f; Eye.Add(FVector(i / 8.f, FMath::Sin(A) * 4.4f, FMath::Sin(A) * 4.0f)); }
			Eyes.Lathe(At - N * 2.f, N, 6.f, Eye, 12, FLinearColor(0.012f, 0.012f, 0.016f), 0.04f);
		}
		Absorb(Eyes, TE, 0, FVector::ZeroVector);
		const float TN = 0.985f, RN = Radius(TN);
		for (const float Side : { -1.f, 1.f })
		{
			Nose.Tube({ FVector(1.f, Side * RN * 0.35f, RN * 0.55f), FVector(3.f, Side * RN * 0.3f, RN * 0.7f) }, 0.8f, 5, FLinearColor(0.02f, 0.02f, 0.02f), 0.3f);
		}
		Absorb(Nose, TN, 0, FVector::ZeroVector);
		const float TM = 0.95f, RM = Radius(TM);
		for (const float Side : { -1.f, 1.f })
		{
			Mouth.Tube({ FVector(-10.f, Side * RM * 0.8f, -RM * 0.45f), FVector(-2.f, Side * RM * 0.85f, -RM * 0.4f), FVector(8.f, Side * RM * 0.4f, -RM * 0.5f) }, 0.6f, 4, FLinearColor(0.03f, 0.03f, 0.03f), 0.4f);
			// Whiskers: stiff, pale, fanning out and back from the muzzle's pads.
			for (int32 w = 0; w < 8; ++w)
			{
				const float Row = (w % 4) / 3.f, Col = (w / 4) * 1.f;
				const FVector Pad(4.f - Col * 4.f, Side * RM * 0.88f, RM * (0.1f - Row * 0.35f));
				const FVector Out = FVector(0.25f - Row * 0.2f, Side * 1.f, 0.05f - Row * 0.25f).GetSafeNormal();
				const float L = 13.f + 4.f * Hash(w, Side);
				Mouth.Tube({ Pad, Pad + Out * L * 0.5f + FVector(-1.5f, 0.f, -0.6f), Pad + Out * L + FVector(-5.f, 0.f, -2.f) }, 0.22f, 3, FLinearColor(0.82f, 0.8f, 0.74f), 0.5f);
			}
		}
		Absorb(Mouth, TM, 0, FVector::ZeroVector);
	}

	Verts.SetNum(Rest.Num());
	Normals.SetNum(Rest.Num());
	Pose(0.f);
	CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
	SetMaterial(0, FWRToolMesh::Material());
	bBuilt = true;
}

void UWRSealBody::SpineAt(float T, FVector& OutCentre, FQuat& OutFrame) const
{
	const float Lat = 26.f, Vert = 10.f;
	const auto Centre = [&](float U)
	{
		const float X = FMath::Lerp(TailX, NoseX, U);
		// In the water: a side-to-side wave from the chest back, strongest at the hind flippers (the stroke); still,
		// a slow scull. On land: the gallumph, a wave up and down the body. A look to the side bends the neck.
		const float Swim = SwimBlend * (Pace * Lat + (1.f - Pace) * 4.f) * FMath::Pow(1.f - U, 1.6f) * FMath::Sin(2.f * PI * (1.1f * U - Clock));
		const float Gallop = (1.f - SwimBlend) * Pace * Vert * (0.4f + 0.6f * U) * FMath::Sin(2.f * PI * (0.9f * U - Clock));
		const float Breathe = (1.f - Pace) * 0.6f * FMath::Sin(Clock * 2.f * PI * 0.35f) * FMath::SmoothStep(0.3f, 0.7f, U);
		const float Look = LookYaw * 30.f * FMath::SmoothStep(0.72f, 1.f, U);
		const float Jerk = FlinchLeft * 22.f * FMath::Sin(U * PI);
		const float Land = FMath::Lerp(-40.f, -14.f, SwimBlend);
		// The head lifts on land (it looks about over the ice), lies low in the water.
		const float HeadUp = (1.f - SwimBlend) * 10.f * FMath::SmoothStep(0.75f, 1.f, U) * (1.f - DeadBlend);
		return FVector(X, Swim + Look + Jerk, Land + Gallop + Breathe + HeadUp);
	};
	const FVector C = Centre(T);
	const FVector Along = (Centre(FMath::Min(1.f, T + 0.02f)) - Centre(FMath::Max(0.f, T - 0.02f))).GetSafeNormal();
	// Dead: it rolls onto its side.
	const FVector Up = FQuat(FVector::XAxisVector, DeadBlend * FMath::DegreesToRadians(80.f)).RotateVector(FVector::UpVector);
	OutCentre = C - FVector(0.f, 0.f, DeadBlend * 12.f);
	OutFrame = FRotationMatrix::MakeFromXZ(Along.IsNearlyZero() ? FVector::XAxisVector : Along, Up).ToQuat();
}

void UWRSealBody::Pose(float Dt)
{
	// Flipper swings: tucked in the water, planting and swinging on land, the hind fans spreading on each stroke.
	const float ForeSwing = (1.f - SwimBlend) * Pace * 28.f * FMath::Sin(2.f * PI * (0.9f * 0.63f - Clock) + 0.6f);
	const float ForeTuck = SwimBlend * -35.f;
	const float Fan = SwimBlend * Pace * 14.f * FMath::Sin(2.f * PI * (-Clock) + 1.2f);
	for (int32 i = 0; i < Rest.Num(); ++i)
	{
		const FVert& V = Rest[i];
		FVector Off = V.Off;
		FVector N = V.Normal;
		if (V.Part >= 1)
		{
			FQuat Q = FQuat::Identity;
			if (V.Part <= 2) Q = FQuat(FVector::YAxisVector, FMath::DegreesToRadians(ForeSwing + ForeTuck));
			else Q = FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(V.Part == 3 ? -Fan : Fan) + (DeadBlend * 0.3f));
			Off = V.Pivot + Q.RotateVector(Off - V.Pivot);
			N = Q.RotateVector(N);
		}
		FVector C;
		FQuat F;
		SpineAt(V.T, C, F);
		Verts[i] = C + F.RotateVector(Off);
		Normals[i] = F.RotateVector(N);
	}
	// Tack rides the seat on the ice crust.
	FVector SeatC;
	FQuat SeatF;
	SpineAt(SeatT, SeatC, SeatF);
	const FVector Seat = SeatC + SeatF.RotateVector(FVector(0.f, 0.f, Radius(SeatT) * 0.92f + 8.f));
	for (const TWeakObjectPtr<USceneComponent>& T : Carried)
	{
		if (T.IsValid()) T->SetRelativeLocationAndRotation(Seat, SeatF);
	}
}

void UWRSealBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AWRCreature* C = Creature.Get();
	if (!bBuilt || !C) return;
	// Off screen: the pose is left as it is (tack still follows when it comes back).
	if (!WasRecentlyRendered(0.5f) && Carried.Num() == 0) return;
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const float Speed = C->GetVelocity().Size2D();
	const bool bWater = Move && Move->IsSwimming();
	const float Top = FMath::Max(100.f, bWater ? Move->MaxSwimSpeed : Move->MaxWalkSpeed);
	SwimBlend = FMath::FInterpTo(SwimBlend, bWater ? 1.f : 0.f, DeltaTime, 3.f);
	Pace = FMath::FInterpTo(Pace, FMath::Clamp(Speed / Top, 0.f, 1.f), DeltaTime, 4.f);
	Clock += DeltaTime * (FMath::Lerp(0.35f, bWater ? 1.4f : 1.6f, Pace));
	// It looks about now and then while still.
	LookClock -= DeltaTime;
	if (LookClock <= 0.f)
	{
		LookClock = FMath::FRandRange(2.f, 5.f);
		LookTarget = Pace < 0.2f ? FMath::FRandRange(-0.8f, 0.8f) : 0.f;
	}
	LookYaw = FMath::FInterpTo(LookYaw, LookTarget, DeltaTime, 2.f);
	FlinchLeft = FMath::Max(0.f, FlinchLeft - DeltaTime);
	DeadBlend = FMath::FInterpTo(DeadBlend, bDead || !C->IsAlive() ? 1.f : 0.f, DeltaTime, 2.5f);
	if (bDead || !C->IsAlive()) Pace = 0.f;
	Pose(DeltaTime);
	UpdateMeshSection(0, Verts, Normals, UVs, Colors, TArray<FProcMeshTangent>());
}
