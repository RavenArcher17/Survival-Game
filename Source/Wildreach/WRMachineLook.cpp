// How machines look and move (WRMachineLook.h; cooks/build-machinery.md 2.6). Every model is in the piece's own space:
// X forward (the way it was placed facing), Z up, the ground at Z = 0. Sizes in cm. Iron is the building kit's stone
// darkened to 0.2 (it reads as black iron); bronze, copper, leather, cloth, glass and glows are flat colours.

#include "WRMachineLook.h"
#include "WRMachine.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRCrafting.h"
#include "EngineUtils.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "Wildreach.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRMachineLookDetail
{
	// Colours (linear).
	const FLinearColor Bronze(0.42f, 0.25f, 0.08f);
	const FLinearColor Copper(0.55f, 0.21f, 0.08f);
	const FLinearColor Leather(0.16f, 0.08f, 0.035f);
	const FLinearColor Sail(0.62f, 0.55f, 0.42f);
	const FLinearColor Glass(0.42f, 0.55f, 0.5f);
	const FLinearColor LampGlow(1.f, 0.72f, 0.35f);
	const FLinearColor Lava(1.f, 0.32f, 0.04f);
	const FLinearColor Soul(0.35f, 1.f, 0.6f);
	const FLinearColor Crystal(0.6f, 0.5f, 1.f);
	const FLinearColor Void(0.03f, 0.015f, 0.05f);
	const FLinearColor VoidSwirl(0.45f, 0.08f, 0.75f);
	const FLinearColor Levi(0.72f, 0.82f, 0.95f);
	const FLinearColor StormGlass(0.35f, 0.55f, 0.9f);
	const FLinearColor SunStone(1.f, 0.62f, 0.14f);
	const FLinearColor Coral(0.8f, 0.42f, 0.33f);
	const FLinearColor Ice(0.72f, 0.88f, 1.f);
	const FLinearColor Blood(0.36f, 0.025f, 0.03f);
	const FLinearColor Compost(0.06f, 0.04f, 0.02f);
	const FLinearColor Cap(0.72f, 0.64f, 0.5f);
	const FLinearColor Water(0.75f, 0.85f, 0.9f);
	const FLinearColor Rope(0.45f, 0.36f, 0.22f);
	const FLinearColor Ore(0.2f, 0.18f, 0.16f);
	constexpr float Iron = 0.2f;

	enum class EM : uint8 { Wood, Stone, Iron, Cloth };

	/** Blocks in one space, grouped by flat colour (cloth blocks of each colour are one Spawn). */
	struct FMB
	{
		FTransform World;    // this space -> world (for the per-block shade)
		TArray<TPair<FLinearColor, FWRLookBlock>> Blocks;

		FWRLookBlock& Add(EM Mat, const FVector& Size, const FVector& Centre, const FRotator& Rot = FRotator::ZeroRotator, float Dim = 1.f,
			EWRLookMesh Mesh = EWRLookMesh::Cube, const FLinearColor& Col = FLinearColor::White)
		{
			FWRLookBlock B;
			B.Size = Size.ComponentMax(FVector(0.5f));
			B.Centre = Centre;
			B.Rot = Rot;
			B.Mesh = Mesh;
			B.Mat = Mat == EM::Wood ? EWRLookMat::Wood : Mat == EM::Cloth ? EWRLookMat::Cloth : EWRLookMat::Stone;
			B.Dim = Mat == EM::Iron ? Iron * Dim : Dim;
			B.Tone = FWRBuildLook::Hash01(World.TransformPosition(Centre), static_cast<int32>(Mat) * 7 + 1);
			const FVector Long = Mesh != EWRLookMesh::Cube ? FVector::ZAxisVector
				: Size.X >= Size.Y && Size.X >= Size.Z ? FVector::XAxisVector : Size.Y >= Size.Z ? FVector::YAxisVector : FVector::ZAxisVector;
			const FVector Dir = World.TransformVectorNoScale(Rot.RotateVector(Long)).GetAbs();
			B.Axis = Dir.Z > 0.7f ? 2.f : Dir.X >= Dir.Y ? 0.f : 1.f;
			return Blocks.Add_GetRef({ Mat == EM::Cloth ? Col : FLinearColor::White, B }).Value;
		}
		void Box(EM Mat, const FVector& Min, const FVector& Max, float Dim = 1.f, const FLinearColor& Col = FLinearColor::White)
		{
			Add(Mat, Max - Min, (Min + Max) * 0.5f, FRotator::ZeroRotator, Dim, EWRLookMesh::Cube, Col);
		}
		/** A round (or square) bar from A to B. */
		void Rod(EM Mat, const FVector& A, const FVector& B, float Diam, float Dim = 1.f, bool bSquare = false, const FLinearColor& Col = FLinearColor::White)
		{
			const FVector D = B - A;
			const float Len = D.Size();
			if (Len < 0.5f) return;
			Add(Mat, FVector(Diam, Diam, Len), (A + B) * 0.5f, FRotationMatrix::MakeFromZ(D / Len).Rotator(), Dim, bSquare ? EWRLookMesh::Cube : EWRLookMesh::Cylinder, Col);
		}
		/** A disc (a cylinder along Axis). */
		void Disc(EM Mat, const FVector& Centre, const FVector& Axis, float Diam, float Thick, float Dim = 1.f, const FLinearColor& Col = FLinearColor::White)
		{
			Add(Mat, FVector(Diam, Diam, Thick), Centre, FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator(), Dim, EWRLookMesh::Cylinder, Col);
		}
		/** Two unit vectors square to Axis (the plane a wheel turns in). */
		static void Basis(const FVector& Axis, FVector& U, FVector& V)
		{
			const FVector A = Axis.GetSafeNormal();
			U = FVector::CrossProduct(A, FMath::Abs(A.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
			V = FVector::CrossProduct(A, U).GetSafeNormal();
		}
		/** A spur gear: a web, a hub, cogs round the rim (Phase turns the first cog). */
		void Gear(EM Mat, const FVector& Centre, const FVector& Axis, float Radius, float Thick, int32 Teeth, float Tooth, float Dim = 1.f, float Phase = 0.f,
			const FLinearColor& Col = FLinearColor::White)
		{
			FVector U, V;
			Basis(Axis, U, V);
			Disc(Mat, Centre, Axis, Radius * 2.f, Thick, Dim, Col);
			Disc(EM::Iron, Centre, Axis, FMath::Max(8.f, Radius * 0.35f), Thick + 4.f, 1.f);
			for (int32 k = 0; k < Teeth; ++k)
			{
				const float A = Phase + k * 2.f * PI / Teeth;
				const FVector Dir = U * FMath::Cos(A) + V * FMath::Sin(A);
				Add(Mat, FVector(Tooth, FMath::Max(1.5f, PI * Radius / Teeth * 0.9f), Thick), Centre + Dir * (Radius + Tooth * 0.45f),
					FRotationMatrix::MakeFromXZ(Dir, Axis).Rotator(), Dim * 0.95f, EWRLookMesh::Cube, Col);
			}
		}
		/** A crown (face) wheel: its cogs stand out of one face toward +Axis, round the rim (it meshes a pinion square to it). */
		void Crown(EM Mat, const FVector& Centre, const FVector& Axis, float Radius, float Thick, int32 Teeth, float Tooth, float Dim = 1.f, float Phase = 0.f)
		{
			FVector U, V;
			Basis(Axis, U, V);
			const FVector A = Axis.GetSafeNormal();
			Disc(Mat, Centre, A, Radius * 2.f + 8.f, Thick, Dim);
			Disc(EM::Iron, Centre, A, FMath::Max(8.f, Radius * 0.3f), Thick + 6.f, 1.f);
			for (int32 k = 0; k < Teeth; ++k)
			{
				const float Ang = Phase + k * 2.f * PI / Teeth;
				const FVector Dir = U * FMath::Cos(Ang) + V * FMath::Sin(Ang);
				Add(Mat, FVector(8.f, FMath::Max(1.5f, PI * Radius / Teeth * 0.8f), Tooth), Centre + Dir * Radius + A * (Thick * 0.5f + Tooth * 0.5f),
					FRotationMatrix::MakeFromXZ(Dir, A).Rotator(), Dim * 0.9f);
			}
		}
		/** A lantern pinion: two discs joined by round staves (the cogs of a crown or spur wheel run between them). */
		void Lantern(const FVector& Centre, const FVector& Axis, float Radius, float Length, int32 Staves, float Phase = 0.f)
		{
			FVector U, V;
			Basis(Axis, U, V);
			const FVector A = Axis.GetSafeNormal();
			Disc(EM::Wood, Centre - A * Length * 0.5f, A, Radius * 2.f + 10.f, 5.f, 0.9f);
			Disc(EM::Wood, Centre + A * Length * 0.5f, A, Radius * 2.f + 10.f, 5.f, 0.9f);
			for (int32 k = 0; k < Staves; ++k)
			{
				const float Ang = Phase + k * 2.f * PI / Staves;
				const FVector P = Centre + (U * FMath::Cos(Ang) + V * FMath::Sin(Ang)) * Radius;
				Rod(EM::Wood, P - A * Length * 0.5f, P + A * Length * 0.5f, 3.f, 1.1f);
			}
		}
		/** A wheel rim of N straight felloes. */
		void Rim(EM Mat, const FVector& Centre, const FVector& Axis, float Radius, float Depth, float Width, int32 N, float Dim = 1.f, float Phase = 0.f)
		{
			FVector U, V;
			Basis(Axis, U, V);
			for (int32 k = 0; k < N; ++k)
			{
				const float A0 = Phase + k * 2.f * PI / N, A1 = Phase + (k + 1) * 2.f * PI / N;
				const FVector P0 = Centre + (U * FMath::Cos(A0) + V * FMath::Sin(A0)) * Radius;
				const FVector P1 = Centre + (U * FMath::Cos(A1) + V * FMath::Sin(A1)) * Radius;
				const FVector Mid = (P0 + P1) * 0.5f;
				const FVector T = (P1 - P0).GetSafeNormal();
				Add(Mat, FVector((P1 - P0).Size() + 2.f, Depth, Width), Mid, FRotationMatrix::MakeFromXZ(T, Axis).Rotator(), Dim);
			}
		}
		/** A trestle: an A-frame from the ground to a bearing at Top, across Across (unit, level). Legs sink below Z = Ground. */
		void Trestle(const FVector& Top, const FVector& Across, float Ground, float Spread)
		{
			const FVector A = Across.GetSafeNormal();
			const FVector FootA = FVector(Top.X, Top.Y, Ground - 60.f) + A * Spread;
			const FVector FootB = FVector(Top.X, Top.Y, Ground - 60.f) - A * Spread;
			const FVector Head = Top - FVector(0.f, 0.f, 14.f);
			Rod(EM::Wood, FootA, Head + A * 6.f, 11.f, 0.95f, true);
			Rod(EM::Wood, FootB, Head - A * 6.f, 11.f, 0.95f, true);
			// The cross tie a third of the way up, and the bearing block that holds the shaft.
			const float TieZ = FMath::Lerp(Ground, Head.Z, 0.4f);
			const float Half = Spread * (1.f - (TieZ - (Ground - 60.f)) / FMath::Max(1.f, Head.Z - (Ground - 60.f)));
			Rod(EM::Wood, FVector(Top.X, Top.Y, TieZ) + A * Half, FVector(Top.X, Top.Y, TieZ) - A * Half, 7.f, 0.85f, true);
			Add(EM::Wood, FVector(18.f, 34.f, 10.f), Head - FVector(0.f, 0.f, 4.f), FRotationMatrix::MakeFromYZ(A, FVector::UpVector).Rotator(), 0.8f);
			Add(EM::Cloth, FVector(14.f, 26.f, 20.f), Top, FRotationMatrix::MakeFromYZ(A, FVector::UpVector).Rotator(), 1.f, EWRLookMesh::Cube, Bronze);   // plummer block
			Rod(EM::Iron, Top + FVector(0.f, 0.f, 10.f) + A * 10.f, Top + FVector(0.f, 0.f, 10.f) - A * 10.f, 3.f);
		}
		/** A coupling flange at a shaft's end. */
		void Flange(const FVector& At, const FVector& Axis)
		{
			Disc(EM::Iron, At, Axis, 30.f, 6.f);
			FVector U, V;
			Basis(Axis, U, V);
			for (int32 k = 0; k < 4; ++k)
			{
				const float A = k * HALF_PI + PI / 4.f;
				Disc(EM::Iron, At + (U * FMath::Cos(A) + V * FMath::Sin(A)) * 11.f, Axis, 4.f, 9.f, 1.3f);
			}
		}
		/** An octagonal oak shaft with iron bands from A to B. */
		void Shaft(const FVector& A, const FVector& B, float Diam = 16.f)
		{
			const FVector D = (B - A).GetSafeNormal();
			Add(EM::Wood, FVector(Diam, Diam, (B - A).Size()), (A + B) * 0.5f, FRotationMatrix::MakeFromZ(D).Rotator(), 1.05f);
			Add(EM::Wood, FVector(Diam, Diam, (B - A).Size() - 2.f), (A + B) * 0.5f, (FRotationMatrix::MakeFromZ(D).ToQuat() * FQuat(FVector::UpVector, PI / 4.f)).Rotator(), 1.05f);
			const int32 Bands = FMath::Max(1, FMath::RoundToInt((B - A).Size() / 100.f));
			for (int32 k = 0; k <= Bands; ++k)
			{
				Disc(EM::Iron, FMath::Lerp(A, B, (k + 0.5f) / (Bands + 1.f)), D, Diam + 4.f, 4.f);
			}
		}
	};

	/** One moving assembly: its pivot (in its parent's space), its blocks (in its own), how it moves. */
	struct FPartModel
	{
		FVector Pivot = FVector::ZeroVector;
		FQuat Rot = FQuat::Identity;
		FMB Blocks;
		FWRMovePart Move;
		int32 Parent = INDEX_NONE;
		bool bCollide = false;
		FVector CollideSize = FVector::ZeroVector;
	};

	struct FModel
	{
		FMB Frame;
		TArray<FPartModel> Parts;
		bool bLight = false;
		FVector LightAt = FVector::ZeroVector;
		FLinearColor LightColour = FLinearColor(1.f, 0.6f, 0.3f);
		float LightRadius = 1000.f;
		float LightIntensity = 5000.f;
		FName Loop;
		float BeltLen = 0.f;

		FPartModel& Part(const FVector& Pivot, EWRMotion Motion, const FVector& Axis, float Ratio = 1.f, int32 Parent = INDEX_NONE)
		{
			FPartModel& P = Parts.AddDefaulted_GetRef();
			P.Pivot = Pivot;
			P.Move.Motion = Motion;
			P.Move.Axis = Axis.GetSafeNormal();
			P.Move.Ratio = Ratio;
			P.Parent = Parent;
			P.Blocks.World = Frame.World;
			P.Blocks.World.SetLocation(Frame.World.TransformPosition(Pivot));
			return P;
		}
	};

	float ConvHalf(float Len) { return Len * 0.5f - 6.f; }
	constexpr float BeltTop = 72.f;
	constexpr float DrumR = 11.f;

	// ---------------------------------------------------------------- the models

	void DriveShaft(FModel& M, float Len)
	{
		const float H = 100.f;
		const int32 Trestles = Len > 450.f ? 3 : 2;
		for (int32 t = 0; t < Trestles; ++t)
		{
			const float X = Trestles == 2 ? (t == 0 ? -Len * 0.33f : Len * 0.33f) : (t - 1) * Len * 0.4f;
			M.Frame.Trestle(FVector(X, 0.f, H), FVector::RightVector, 0.f, 55.f);
		}
		FPartModel& S = M.Part(FVector(0.f, 0.f, H), EWRMotion::Spin, FVector::ForwardVector);
		S.Blocks.Shaft(FVector(-Len * 0.5f + 4.f, 0.f, 0.f), FVector(Len * 0.5f - 4.f, 0.f, 0.f));
		S.Blocks.Flange(FVector(-Len * 0.5f + 3.f, 0.f, 0.f), FVector::ForwardVector);
		S.Blocks.Flange(FVector(Len * 0.5f - 3.f, 0.f, 0.f), FVector::ForwardVector);
		M.Loop = NAME_None;
	}

	void Gearbox(FModel& M)
	{
		const float H = 100.f;
		// A stone pad, an oak cage with corner posts and a lid; four stub shafts out of its sides.
		M.Frame.Box(EM::Stone, FVector(-62.f, -62.f, -30.f), FVector(62.f, 62.f, 12.f), 1.05f);
		for (const float X : { -46.f, 46.f }) for (const float Y : { -46.f, 46.f }) M.Frame.Box(EM::Wood, FVector(X - 7.f, Y - 7.f, 12.f), FVector(X + 7.f, Y + 7.f, 168.f), 0.9f);
		for (const float Z : { 40.f, 162.f })
		{
			M.Frame.Box(EM::Wood, FVector(-53.f, -53.f, Z - 5.f), FVector(53.f, -39.f, Z + 5.f), 0.85f);
			M.Frame.Box(EM::Wood, FVector(-53.f, 39.f, Z - 5.f), FVector(53.f, 53.f, Z + 5.f), 0.85f);
			M.Frame.Box(EM::Wood, FVector(-53.f, -39.f, Z - 5.f), FVector(-39.f, 39.f, Z + 5.f), 0.85f);
			M.Frame.Box(EM::Wood, FVector(39.f, -39.f, Z - 5.f), FVector(53.f, 39.f, Z + 5.f), 0.85f);
		}
		M.Frame.Box(EM::Wood, FVector(-55.f, -55.f, 166.f), FVector(55.f, 55.f, 174.f), 0.8f);
		// Bearings in the cage's sides where the stubs come out.
		for (int32 s = 0; s < 4; ++s)
		{
			const FVector D = FRotator(0.f, s * 90.f, 0.f).Vector();
			M.Frame.Disc(EM::Cloth, D * 46.f + FVector(0.f, 0.f, H), D, 24.f, 14.f, 1.f, Bronze);
		}
		// The vertical arbor with the crown wheel under the stubs; its cogs face up into the four bevel pinions.
		// (The crown reaches 26 cm to the lanterns' 10: it turns that much slower than the stubs it meshes.)
		FPartModel& Arbor = M.Part(FVector(0.f, 0.f, H), EWRMotion::Spin, FVector::UpVector, 10.f / 26.f);
		Arbor.Blocks.Rod(EM::Iron, FVector(0.f, 0.f, -86.f), FVector(0.f, 0.f, 66.f), 8.f);
		Arbor.Blocks.Crown(EM::Wood, FVector(0.f, 0.f, -22.f), FVector::UpVector, 26.f, 7.f, 16, 9.f, 0.95f);
		// X stubs (one shaft right through) and Y stubs: their lanterns ride on the crown's cogs, all at the line's speed.
		for (const int32 Ax : { 0, 1 })
		{
			const FVector D = Ax == 0 ? FVector::ForwardVector : FVector::RightVector;
			FPartModel& Stub = M.Part(FVector(0.f, 0.f, H), EWRMotion::Spin, D, 1.f);
			Stub.Blocks.Rod(EM::Iron, -D * 72.f, D * 72.f, 8.f);
			Stub.Blocks.Lantern(D * 26.f, D, 10.f, 10.f, 8);
			Stub.Blocks.Flange(D * 70.f, D);
			Stub.Blocks.Flange(-D * 70.f, D);
		}
	}

	void WaterWheel(FModel& M, float Hub, bool bTide)
	{
		const float Out = 300.f;
		const float R = bTide ? 260.f : 200.f;
		const float W = bTide ? 140.f : 110.f;
		// The bank: two stone piers carrying a timber headstock with the inner bearing; a stone sill under it.
		const float PierTop = Hub - 14.f;
		M.Frame.Box(EM::Stone, FVector(-70.f, -75.f, -60.f), FVector(30.f, 75.f, FMath::Max(10.f, PierTop - 40.f)), 1.1f);
		for (const float Y : { -55.f, 55.f }) M.Frame.Box(EM::Stone, FVector(-40.f, Y - 20.f, -60.f), FVector(10.f, Y + 20.f, PierTop), 1.f);
		M.Frame.Box(EM::Wood, FVector(-45.f, -78.f, PierTop), FVector(15.f, 78.f, PierTop + 14.f), 0.85f);
		M.Frame.Add(EM::Cloth, FVector(30.f, 34.f, 24.f), FVector(-15.f, 0.f, Hub), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		// The outer bearing: a post standing in the water, its foot sunk in the bed, a brace back to the bank.
		const float OuterX = Out + W * 0.5f + 40.f;
		M.Frame.Box(EM::Wood, FVector(OuterX - 12.f, -12.f, Hub - 520.f), FVector(OuterX + 12.f, 12.f, Hub - 12.f), 0.9f);
		M.Frame.Add(EM::Cloth, FVector(30.f, 30.f, 22.f), FVector(OuterX, 0.f, Hub), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		M.Frame.Rod(EM::Wood, FVector(OuterX, 0.f, Hub - 40.f), FVector(15.f, 0.f, PierTop - 30.f), 12.f, 0.85f, true);
		// The wheel on its axle (turning about X: the axle runs from the bank out over the water).
		FPartModel& Wheel = M.Part(FVector(0.f, 0.f, Hub), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Wheel.Blocks.Shaft(FVector(-58.f, 0.f, 0.f), FVector(OuterX + 16.f, 0.f, 0.f), 22.f);
		Wheel.Blocks.Flange(FVector(-56.f, 0.f, 0.f), FVector::ForwardVector);
		for (const float Side : { -1.f, 1.f })
		{
			const FVector C(Out + Side * W * 0.5f, 0.f, 0.f);
			Wheel.Blocks.Rim(EM::Wood, C, FVector::ForwardVector, R - 8.f, 16.f, 10.f, 16, 0.9f);
			Wheel.Blocks.Disc(EM::Iron, C, FVector::ForwardVector, 46.f, 26.f);
			for (int32 k = 0; k < 8; ++k)
			{
				const float A = k * PI / 4.f + (Side > 0.f ? 0.f : PI / 8.f) * 0.f;
				const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
				Wheel.Blocks.Rod(EM::Wood, C + Dir * 20.f, C + Dir * (R - 12.f), 10.f, 0.95f, true);
			}
		}
		// Floats: 16 boards across the two rims, their outer edge clear of the rim by 30 cm (they dip into the water).
		for (int32 k = 0; k < 16; ++k)
		{
			const float A = k * PI / 8.f;
			const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
			Wheel.Blocks.Add(EM::Wood, FVector(W + 8.f, 4.f, 48.f), FVector(Out, 0.f, 0.f) + Dir * (R - 14.f), FRotationMatrix::MakeFromXZ(FVector::ForwardVector, Dir).Rotator(), 0.8f + 0.04f * (k % 3));
		}
		M.Loop = TEXT("machine.waterwheel");
	}

	void Windmill(FModel& M)
	{
		// A stone plinth, an octagonal weatherboarded smock body tapering 200 -> 130 cm across 60..660, a gear house at
		// its foot (-X) where the power comes out. The cap turns on the curb; sails and fantail ride on it.
		M.Frame.Add(EM::Stone, FVector(430.f, 430.f, 70.f), FVector(0.f, 0.f, 25.f), FRotator(0.f, 22.5f, 0.f), 1.05f, EWRLookMesh::Cylinder);
		for (int32 f = 0; f < 8; ++f)
		{
			const float A = f * 45.f + 22.5f;
			const float Rb = 195.f, Rt = 128.f;
			const FVector In = FRotator(0.f, A, 0.f).Vector();
			// One face: boards lapped bottom to top, the corner post at each edge.
			const float FaceW0 = 2.f * Rb * FMath::Tan(PI / 8.f), FaceW1 = 2.f * Rt * FMath::Tan(PI / 8.f);
			const float Lean = FMath::RadiansToDegrees(FMath::Atan2(Rb - Rt, 600.f));
			for (int32 b = 0; b < 20; ++b)
			{
				const float t = (b + 0.5f) / 20.f;
				const float Rr = FMath::Lerp(Rb, Rt, t) * FMath::Cos(PI / 8.f);
				const FVector C = In * Rr + FVector(0.f, 0.f, 60.f + 600.f * t);
				M.Frame.Add(EM::Wood, FVector(5.f, FMath::Lerp(FaceW0, FaceW1, t) + 6.f, 31.f), C, FRotator(-Lean, A, 0.f), 0.78f + 0.08f * (b % 2));
			}
			const FVector Corner0 = FRotator(0.f, A + 22.5f, 0.f).Vector() * Rb + FVector(0.f, 0.f, 60.f);
			const FVector Corner1 = FRotator(0.f, A + 22.5f, 0.f).Vector() * Rt + FVector(0.f, 0.f, 660.f);
			M.Frame.Rod(EM::Wood, Corner0, Corner1, 14.f, 0.7f, true);
		}
		// A door on the +X face and a window above; the gear house on -X with the output stub's bearing.
		M.Frame.Box(EM::Wood, FVector(183.f, -40.f, 60.f), FVector(193.f, 40.f, 250.f), 0.55f);
		M.Frame.Box(EM::Stone, FVector(145.f, -22.f, 380.f), FVector(176.f, 22.f, 430.f), 0.08f);
		M.Frame.Box(EM::Wood, FVector(-262.f, -60.f, 0.f), FVector(-180.f, 60.f, 150.f), 0.8f);
		M.Frame.Box(EM::Wood, FVector(-270.f, -68.f, 150.f), FVector(-172.f, 68.f, 160.f), 0.7f);
		M.Frame.Disc(EM::Cloth, FVector(-264.f, 0.f, 90.f), FVector::ForwardVector, 26.f, 10.f, 1.f, Bronze);
		// The curb ring the cap turns on.
		M.Frame.Disc(EM::Wood, FVector(0.f, 0.f, 664.f), FVector::UpVector, 280.f, 10.f, 0.75f);
		// Output stub (turns with the sails, geared down inside): a short shaft with its flange.
		FPartModel& Stub = M.Part(FVector(-264.f, 0.f, 90.f), EWRMotion::Spin, FVector::ForwardVector, 2.f);
		Stub.Blocks.Rod(EM::Iron, FVector(-20.f, 0.f, 0.f), FVector(40.f, 0.f, 0.f), 9.f);
		Stub.Blocks.Flange(FVector(-24.f, 0.f, 0.f), FVector::ForwardVector);
		// The boat cap (turns to face the wind about Z): a ribbed hood, the windshaft out of its front, the fantail at the back.
		const int32 CapIx = M.Parts.Num();
		FPartModel& CapPart = M.Part(FVector(0.f, 0.f, 670.f), EWRMotion::Cap, FVector::UpVector, 0.f);
		for (int32 r = 0; r < 9; ++r)
		{
			const float A = -PI / 2.f + r * PI / 8.f;
			CapPart.Blocks.Add(EM::Wood, FVector(300.f, 28.f, 8.f), FVector(0.f, FMath::Sin(A) * 118.f, FMath::Cos(A) * 92.f), FRotator(0.f, 0.f, FMath::RadiansToDegrees(-A)), 0.72f);
		}
		CapPart.Blocks.Box(EM::Wood, FVector(-150.f, -120.f, -6.f), FVector(150.f, 120.f, 4.f), 0.65f);
		CapPart.Blocks.Add(EM::Cloth, FVector(40.f, 40.f, 40.f), FVector(150.f, 0.f, 40.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);   // neck bearing
		// Fantail staging at the back.
		CapPart.Blocks.Rod(EM::Wood, FVector(-140.f, -70.f, 20.f), FVector(-250.f, 0.f, 100.f), 9.f, 0.85f, true);
		CapPart.Blocks.Rod(EM::Wood, FVector(-140.f, 70.f, 20.f), FVector(-250.f, 0.f, 100.f), 9.f, 0.85f, true);
		FPartModel& Fan = M.Part(FVector(-255.f, 0.f, 105.f), EWRMotion::Fan, FVector::RightVector, 1.f, CapIx);
		Fan.Blocks.Disc(EM::Iron, FVector::ZeroVector, FVector::RightVector, 14.f, 16.f);
		for (int32 v = 0; v < 6; ++v)
		{
			const float A = v * PI / 3.f;
			const FVector Dir(FMath::Cos(A), 0.f, FMath::Sin(A));
			Fan.Blocks.Add(EM::Wood, FVector(80.f, 3.f, 20.f), Dir * 46.f, FRotationMatrix::MakeFromXY(Dir, FVector(0.f, 0.94f, 0.34f)).Rotator(), 0.9f);
		}
		// Sails: two crossed stocks through the windshaft's poll end; each sail a lattice of bars with cloth spread on it.
		FPartModel& Sails = M.Part(FVector(222.f, 0.f, 40.f), EWRMotion::Spin, FVector::ForwardVector, 1.f, CapIx);
		Sails.Blocks.Shaft(FVector(-90.f, 0.f, 0.f), FVector(10.f, 0.f, 0.f), 30.f);
		Sails.Blocks.Add(EM::Iron, FVector(36.f, 36.f, 36.f), FVector(16.f, 0.f, 0.f), FRotator::ZeroRotator, 1.f);  // the poll end
		for (int32 s = 0; s < 4; ++s)
		{
			const float A = s * HALF_PI;
			const FVector Dir(0.f, FMath::Cos(A), FMath::Sin(A));
			const FVector Side(0.f, -FMath::Sin(A), FMath::Cos(A));
			Sails.Blocks.Rod(EM::Wood, FVector(16.f, 0.f, 0.f), FVector(16.f, 0.f, 0.f) + Dir * 450.f, 13.f, 0.85f, true);   // whip
			// Bars across, and the two hemlath rails.
			for (int32 b = 0; b < 10; ++b)
			{
				const float d = 110.f + b * 36.f;
				Sails.Blocks.Rod(EM::Wood, FVector(18.f, 0.f, 0.f) + Dir * d, FVector(18.f, 0.f, 0.f) + Dir * d + Side * 95.f, 4.f, 1.f, true);
			}
			Sails.Blocks.Rod(EM::Wood, FVector(18.f, 0.f, 0.f) + Dir * 110.f + Side * 95.f, FVector(18.f, 0.f, 0.f) + Dir * 440.f + Side * 95.f, 5.f, 0.9f, true);
			// The cloth spread on the lattice (just behind the bars), twisted a little as common sails are weathered.
			Sails.Blocks.Add(EM::Cloth, FVector(2.f, 92.f, 320.f), FVector(22.f, 0.f, 0.f) + Dir * 275.f + Side * 48.f,
				FRotationMatrix::MakeFromYZ(Side, Dir).Rotator() + FRotator(0.f, 0.f, 0.f), 1.f, EWRLookMesh::Cube, Sail);
		}
		M.Loop = TEXT("machine.windmill");
	}

	void Turbine(FModel& M)
	{
		// The bore head (+X), a steam pipe up and over into a riveted drum on stone cradles, the spindle out to a flywheel
		// and the output stub (-X); an exhaust stack on the drum.
		M.Frame.Add(EM::Stone, FVector(120.f, 120.f, 46.f), FVector(130.f, 0.f, 23.f), FRotator::ZeroRotator, 1.05f, EWRLookMesh::Cylinder);
		M.Frame.Add(EM::Stone, FVector(70.f, 70.f, 10.f), FVector(130.f, 0.f, 50.f), FRotator::ZeroRotator, 0.1f, EWRLookMesh::Cylinder);
		M.Frame.Rod(EM::Iron, FVector(130.f, 0.f, 46.f), FVector(130.f, 0.f, 210.f), 26.f);
		M.Frame.Rod(EM::Iron, FVector(130.f, 0.f, 210.f), FVector(70.f, 0.f, 210.f), 26.f);
		M.Frame.Rod(EM::Iron, FVector(70.f, 0.f, 210.f), FVector(70.f, 0.f, 150.f), 26.f);
		for (const float X : { -40.f, 40.f }) M.Frame.Box(EM::Stone, FVector(X - 22.f, -60.f, 0.f), FVector(X + 22.f, 60.f, 90.f), 1.f);
		M.Frame.Disc(EM::Iron, FVector(0.f, 0.f, 150.f), FVector::ForwardVector, 150.f, 140.f, 1.1f);
		for (int32 r = 0; r < 4; ++r) M.Frame.Disc(EM::Iron, FVector(-60.f + r * 40.f, 0.f, 150.f), FVector::ForwardVector, 158.f, 5.f, 1.6f);
		M.Frame.Rod(EM::Iron, FVector(-20.f, 0.f, 220.f), FVector(-20.f, 0.f, 330.f), 30.f, 0.9f);
		M.Frame.Disc(EM::Iron, FVector(-20.f, 0.f, 332.f), FVector::UpVector, 40.f, 6.f, 1.2f);
		M.Frame.Box(EM::Stone, FVector(-140.f, -30.f, 0.f), FVector(-112.f, 30.f, 132.f), 1.f);
		M.Frame.Add(EM::Cloth, FVector(26.f, 30.f, 26.f), FVector(-126.f, 0.f, 150.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		FPartModel& Fly = M.Part(FVector(0.f, 0.f, 150.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Fly.Blocks.Rod(EM::Iron, FVector(-200.f, 0.f, 0.f), FVector(-70.f, 0.f, 0.f), 10.f);
		Fly.Blocks.Rim(EM::Iron, FVector(-95.f, 0.f, 0.f), FVector::ForwardVector, 62.f, 14.f, 16.f, 16, 1.f);
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = k * PI / 3.f;
			Fly.Blocks.Rod(EM::Iron, FVector(-95.f, 0.f, 0.f), FVector(-95.f, FMath::Cos(A) * 56.f, FMath::Sin(A) * 56.f), 7.f);
		}
		Fly.Blocks.Flange(FVector(-198.f, 0.f, 0.f), FVector::ForwardVector);
		M.Loop = TEXT("machine.turbine");
	}

	void SoulEngine(FModel& M)
	{
		// A bone-and-stone plinth, a black iron frame round a glass burn chamber (the soulstone's pale green flame), the
		// fuel hopper on top, a flywheel and stub at -X.
		M.Frame.Box(EM::Stone, FVector(-90.f, -80.f, 0.f), FVector(90.f, 80.f, 40.f), 0.9f);
		for (int32 b = 0; b < 10; ++b)
		{
			const float A = b * 36.f;
			M.Frame.Add(EM::Cloth, FVector(60.f, 6.f, 6.f), FRotator(0.f, A, 0.f).Vector() * 86.f + FVector(0.f, 0.f, 20.f), FRotator(0.f, A + 90.f, 20.f), 1.f,
				EWRLookMesh::Cylinder, FLinearColor(0.75f, 0.7f, 0.6f));   // bones bedded in the plinth
		}
		for (const float X : { -50.f, 50.f }) for (const float Y : { -50.f, 50.f }) M.Frame.Box(EM::Iron, FVector(X - 6.f, Y - 6.f, 40.f), FVector(X + 6.f, Y + 6.f, 220.f), 1.f);
		M.Frame.Box(EM::Iron, FVector(-56.f, -56.f, 214.f), FVector(56.f, 56.f, 226.f), 1.f);
		M.Frame.Add(EM::Cloth, FVector(84.f, 84.f, 150.f), FVector(0.f, 0.f, 130.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Soul * 0.6f);
		M.Frame.Add(EM::Cloth, FVector(94.f, 94.f, 150.f), FVector(0.f, 0.f, 130.f), FRotator(0.f, 22.5f, 0.f), 1.f, EWRLookMesh::Cylinder, Glass * 0.5f);
		M.Frame.Add(EM::Iron, FVector(70.f, 70.f, 50.f), FVector(0.f, 0.f, 250.f), FRotator(180.f, 0.f, 0.f), 1.f, EWRLookMesh::Cone);   // hopper
		M.Frame.Add(EM::Cloth, FVector(30.f, 30.f, 20.f), FVector(-96.f, 0.f, 110.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		FPartModel& Fly = M.Part(FVector(-80.f, 0.f, 110.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Fly.Blocks.Rod(EM::Iron, FVector(-60.f, 0.f, 0.f), FVector(20.f, 0.f, 0.f), 9.f);
		Fly.Blocks.Rim(EM::Iron, FVector(-30.f, 0.f, 0.f), FVector::ForwardVector, 50.f, 12.f, 12.f, 12, 1.f);
		for (int32 k = 0; k < 4; ++k) Fly.Blocks.Rod(EM::Iron, FVector(-30.f, 0.f, 0.f), FVector(-30.f, FMath::Cos(k * HALF_PI) * 46.f, FMath::Sin(k * HALF_PI) * 46.f), 6.f);
		Fly.Blocks.Flange(FVector(-58.f, 0.f, 0.f), FVector::ForwardVector);
		M.bLight = true;
		M.LightAt = FVector(0.f, 0.f, 140.f);
		M.LightColour = FLinearColor(0.4f, 1.f, 0.65f);
		M.LightRadius = 900.f;
		M.LightIntensity = 3500.f;
		M.Loop = TEXT("machine.coil");
	}

	/** A small horizontal line shaft from a machine's port at (PortX, 0, Z) into it to X1, on a bearing. */
	void InputShaft(FModel& M, float PortX, float X1, float Z, int32& OutIx, float Ratio = 1.f)
	{
		M.Frame.Box(EM::Wood, FVector(PortX + 18.f, -16.f, 0.f), FVector(PortX + 40.f, 16.f, Z - 10.f), 0.85f);
		M.Frame.Add(EM::Cloth, FVector(24.f, 30.f, 22.f), FVector(PortX + 29.f, 0.f, Z), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		OutIx = M.Parts.Num();
		FPartModel& S = M.Part(FVector(0.f, 0.f, Z), EWRMotion::Spin, FVector::ForwardVector, Ratio);
		S.Blocks.Shaft(FVector(PortX + 4.f, 0.f, 0.f), FVector(X1, 0.f, 0.f), 12.f);
		S.Blocks.Flange(FVector(PortX + 3.f, 0.f, 0.f), FVector::ForwardVector);
	}

	void Sawmill(FModel& M)
	{
		// A 3 m timber frame on sills; the input shaft runs in low to a crank disc; the pitman rod drives the sash (a
		// frame 2 m tall with the blade) up and down between guide posts; the log lies on bolsters on the carriage rails
		// through the sash (its bottom bar always passes under the log, its top bar over it); sawn planks off the end.
		for (const float Y : { -60.f, 60.f })
		{
			M.Frame.Box(EM::Wood, FVector(-150.f, Y - 9.f, 0.f), FVector(160.f, Y + 9.f, 20.f), 0.8f);       // sills
			M.Frame.Box(EM::Wood, FVector(-150.f, Y - 7.f, 120.f), FVector(160.f, Y + 7.f, 134.f), 0.85f);  // carriage rails
			for (const float X : { -140.f, 150.f }) M.Frame.Box(EM::Wood, FVector(X - 8.f, Y - 8.f, 20.f), FVector(X + 8.f, Y + 8.f, 120.f), 0.9f);
		}
		for (const float X : { -70.f, 90.f }) M.Frame.Box(EM::Wood, FVector(X - 8.f, -60.f, 134.f), FVector(X + 8.f, 60.f, 148.f), 0.8f);   // bolsters
		for (const float Y : { -46.f, 46.f }) M.Frame.Box(EM::Wood, FVector(-10.f, Y - 7.f, 20.f), FVector(10.f, Y + 7.f, 366.f), 0.95f);  // guide posts
		M.Frame.Box(EM::Wood, FVector(-14.f, -60.f, 350.f), FVector(14.f, 60.f, 366.f), 0.85f);
		M.Frame.Add(EM::Wood, FVector(44.f, 44.f, 250.f), FVector(10.f, 0.f, 170.f), FRotator(90.f, 0.f, 0.f), 1.15f, EWRLookMesh::Cylinder);
		for (int32 p = 0; p < 6; ++p) M.Frame.Box(EM::Wood, FVector(175.f, -40.f + (p % 2) * 4.f, p * 6.f), FVector(275.f, 40.f + (p % 2) * 4.f, p * 6.f + 5.f), 1.1f);
		int32 Shaft = 0;
		InputShaft(M, -190.f, -24.f, 30.f, Shaft, 1.f);
		FPartModel& Crank = M.Part(FVector(-20.f, 0.f, 30.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Crank.Blocks.Disc(EM::Iron, FVector::ZeroVector, FVector::ForwardVector, 66.f, 8.f);
		Crank.Blocks.Rod(EM::Iron, FVector(0.f, 0.f, 26.f), FVector(10.f, 0.f, 26.f), 6.f);
		// Crank pin (26 cm throw, turning in the plane across X) to the sash's foot on the vertical: the rod is 72 cm.
		FWRMovePart Kin;
		Kin.C = FVector(-12.f, 0.f, 30.f); Kin.U = FVector::UpVector; Kin.V = FVector::RightVector; Kin.CrankR = 26.f; Kin.RodL = 72.f; Kin.S = FVector::UpVector;
		FPartModel& Rod = M.Part(FVector::ZeroVector, EWRMotion::Rod, FVector::UpVector, 1.f);
		{ const EWRMotion K = Rod.Move.Motion; Rod.Move = Kin; Rod.Move.Motion = K; }
		Rod.Blocks.Rod(EM::Wood, FVector(0.f, 0.f, -36.f), FVector(0.f, 0.f, 36.f), 9.f, 0.8f, true);
		FPartModel& Sash = M.Part(FVector::ZeroVector, EWRMotion::CrankSlide, FVector::UpVector, 1.f);
		{ const EWRMotion K = Sash.Move.Motion; Sash.Move = Kin; Sash.Move.Motion = K; }
		Sash.Blocks.Box(EM::Wood, FVector(-5.f, -40.f, 0.f), FVector(5.f, -32.f, 200.f), 0.75f);
		Sash.Blocks.Box(EM::Wood, FVector(-5.f, 32.f, 0.f), FVector(5.f, 40.f, 200.f), 0.75f);
		Sash.Blocks.Box(EM::Wood, FVector(-6.f, -40.f, 0.f), FVector(6.f, 40.f, 10.f), 0.75f);
		Sash.Blocks.Box(EM::Wood, FVector(-6.f, -40.f, 190.f), FVector(6.f, 40.f, 200.f), 0.75f);
		Sash.Blocks.Box(EM::Iron, FVector(-1.f, -1.f, 10.f), FVector(1.f, 1.f, 190.f), 1.5f);   // the blade, in its kerf
		M.Loop = TEXT("machine.sawmill");
	}

	void Grinder(FModel& M)
	{
		// The hurst frame: four posts carrying a deck; the round wooden tun with the runner stone over the bed stone;
		// the hopper on its horse above, the shoe and damsel feeding the eye; the stone spindle down to the stone nut,
		// driven by the input shaft's crown wheel.
		for (const float X : { -80.f, 80.f }) for (const float Y : { -80.f, 80.f }) M.Frame.Box(EM::Wood, FVector(X - 10.f, Y - 10.f, 0.f), FVector(X + 10.f, Y + 10.f, 110.f), 0.9f);
		M.Frame.Box(EM::Wood, FVector(-92.f, -92.f, 104.f), FVector(92.f, 92.f, 114.f), 0.8f);
		M.Frame.Add(EM::Stone, FVector(140.f, 140.f, 20.f), FVector(0.f, 0.f, 124.f), FRotator::ZeroRotator, 0.9f, EWRLookMesh::Cylinder);     // bed stone
		for (int32 s = 0; s < 20; ++s)
		{
			const float A = s * 18.f;
			M.Frame.Add(EM::Wood, FVector(6.f, 25.f, 40.f), FRotator(0.f, A, 0.f).Vector() * 76.f + FVector(0.f, 0.f, 134.f), FRotator(0.f, A, 0.f), 0.85f);  // tun staves
		}
		M.Frame.Disc(EM::Iron, FVector(0.f, 0.f, 150.f), FVector::UpVector, 160.f, 3.f);
		// Horse and hopper.
		for (const float Y : { -60.f, 60.f }) M.Frame.Box(EM::Wood, FVector(-70.f, Y - 5.f, 154.f), FVector(70.f, Y + 5.f, 162.f), 0.8f);
		for (const float X : { -40.f, 40.f }) M.Frame.Box(EM::Wood, FVector(X - 4.f, -60.f, 162.f), FVector(X + 4.f, 60.f, 210.f), 0.8f);
		M.Frame.Add(EM::Wood, FVector(90.f, 90.f, 60.f), FVector(0.f, 0.f, 236.f), FRotator(180.f, 45.f, 0.f), 0.9f, EWRLookMesh::Cone);
		int32 Shaft = 0;
		InputShaft(M, -150.f, -26.f, 50.f, Shaft, 1.f);
		// The input shaft's end carries a crown wheel whose cogs stand toward +X; at its top they run between the staves
		// of the stone nut (a lantern on the stone spindle), 4 x faster: the stones turn as a mill's do, fast off a slow wheel.
		FPartModel& Crown = M.Part(FVector(-22.f, 0.f, 50.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Crown.Blocks.Crown(EM::Wood, FVector::ZeroVector, FVector::ForwardVector, 40.f, 8.f, 24, 9.f, 0.95f);
		FPartModel& Spindle = M.Part(FVector(0.f, 0.f, 90.f), EWRMotion::Spin, FVector::UpVector, 4.f);
		Spindle.Blocks.Rod(EM::Iron, FVector(0.f, 0.f, -30.f), FVector(0.f, 0.f, 58.f), 8.f);
		Spindle.Blocks.Lantern(FVector::ZeroVector, FVector::UpVector, 10.f, 12.f, 6);
		// The runner stone rides the spindle's top through the bed stone's eye.
		FPartModel& Runner = M.Part(FVector(0.f, 0.f, 142.f), EWRMotion::Spin, FVector::UpVector, 4.f);
		Runner.Blocks.Add(EM::Stone, FVector(132.f, 132.f, 18.f), FVector::ZeroVector, FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);
		Runner.Blocks.Add(EM::Stone, FVector(22.f, 22.f, 20.f), FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator, 0.1f, EWRLookMesh::Cylinder);   // the eye
		Runner.Blocks.Rod(EM::Iron, FVector(-14.f, 0.f, 10.f), FVector(14.f, 0.f, 10.f), 4.f, 1.f, true);      // the rynd
		for (int32 f = 0; f < 8; ++f) Runner.Blocks.Add(EM::Stone, FVector(50.f, 3.f, 1.f), FRotator(0.f, f * 45.f + 12.f, 0.f).RotateVector(FVector(38.f, 0.f, 9.5f)), FRotator(0.f, f * 45.f + 12.f, 0.f), 0.6f);
		// The shoe under the hopper, knocked by the damsel: a little shake.
		FPartModel& Shoe = M.Part(FVector(0.f, 0.f, 172.f), EWRMotion::Slide, FVector::RightVector, 4.f);
		Shoe.Move.Amp = 1.5f;
		Shoe.Blocks.Box(EM::Wood, FVector(-14.f, -8.f, -4.f), FVector(14.f, 8.f, 4.f), 0.85f);
		M.Loop = TEXT("machine.grinder");
	}

	void Loom(FModel& M)
	{
		// A floor loom: four posts, the warp beam at the back and the cloth beam at the front with linen rolled on it,
		// the warp threads between, two heddle frames lifting in turn, the batten swinging on its pivots; a cam shaft
		// under it all from the input stub.
		for (const float X : { -90.f, 90.f }) for (const float Y : { -75.f, 75.f }) M.Frame.Box(EM::Wood, FVector(X - 7.f, Y - 7.f, 0.f), FVector(X + 7.f, Y + 7.f, 170.f), 0.9f);
		for (const float Y : { -75.f, 75.f }) M.Frame.Box(EM::Wood, FVector(-97.f, Y - 6.f, 160.f), FVector(97.f, Y + 6.f, 172.f), 0.85f);
		M.Frame.Rod(EM::Wood, FVector(-90.f, -70.f, 90.f), FVector(-90.f, 70.f, 90.f), 16.f, 1.f);
		M.Frame.Rod(EM::Wood, FVector(90.f, -70.f, 80.f), FVector(90.f, 70.f, 80.f), 14.f, 1.f);
		M.Frame.Add(EM::Cloth, FVector(28.f, 28.f, 120.f), FVector(90.f, 0.f, 80.f), FRotator(0.f, 0.f, 90.f), 1.f, EWRLookMesh::Cylinder, FLinearColor(0.7f, 0.66f, 0.55f));
		for (int32 t = 0; t < 14; ++t)
		{
			const float Y = -58.f + t * 9.f;
			M.Frame.Rod(EM::Cloth, FVector(-88.f, Y, 98.f), FVector(80.f, Y, 92.f), 0.8f, 1.f, false, FLinearColor(0.78f, 0.74f, 0.62f));
		}
		int32 Shaft = 0;
		InputShaft(M, -150.f, 80.f, 30.f, Shaft, 1.f);
		for (int32 h = 0; h < 2; ++h)
		{
			FPartModel& Heddle = M.Part(FVector(-20.f + h * 18.f, 0.f, 120.f), EWRMotion::Slide, FVector::UpVector, 1.f);
			Heddle.Move.Amp = 7.f;
			Heddle.Move.Phase = h * PI;
			Heddle.Blocks.Box(EM::Wood, FVector(-3.f, -68.f, -30.f), FVector(3.f, 68.f, -26.f), 0.8f);
			Heddle.Blocks.Box(EM::Wood, FVector(-3.f, -68.f, 26.f), FVector(3.f, 68.f, 30.f), 0.8f);
			for (int32 k = 0; k < 7; ++k) Heddle.Blocks.Box(EM::Cloth, FVector(-0.5f, -58.f + h * 9.f + k * 18.f - 0.5f, -26.f), FVector(0.5f, -58.f + h * 9.f + k * 18.f + 0.5f, 26.f), 1.f, FLinearColor(0.3f, 0.25f, 0.2f));
		}
		FPartModel& Batten = M.Part(FVector(40.f, 0.f, 10.f), EWRMotion::Swing, FVector::RightVector, 1.f);
		Batten.Move.Amp = 9.f;
		Batten.Move.Phase = HALF_PI;
		for (const float Y : { -66.f, 66.f }) Batten.Blocks.Box(EM::Wood, FVector(-3.f, Y - 3.f, 0.f), FVector(3.f, Y + 3.f, 96.f), 0.85f);
		Batten.Blocks.Box(EM::Wood, FVector(-5.f, -70.f, 92.f), FVector(5.f, 70.f, 104.f), 0.8f);
		Batten.Blocks.Box(EM::Wood, FVector(-2.f, -64.f, 70.f), FVector(2.f, 64.f, 92.f), 0.6f);   // the reed
		M.Loop = TEXT("machine.loom");
	}

	void Smelter(FModel& M)
	{
		// A stone shaft furnace on a square base tapering to its mouth at 240 cm, the tapping arch on +X glowing; two
		// wedge bellows on a bench at -X, their nozzles into the tuyere; the cam shaft over their backs from the stub;
		// an ingot table at the arch.
		M.Frame.Box(EM::Stone, FVector(-70.f, -70.f, 0.f), FVector(70.f, 70.f, 60.f), 1.05f);
		for (int32 c = 0; c < 8; ++c)
		{
			const float t = c / 8.f;
			const float Half = FMath::Lerp(64.f, 40.f, t);
			const float Z0 = 60.f + c * 22.5f;
			M.Frame.Add(EM::Stone, FVector(Half * 2.f, Half * 2.f, 22.f), FVector(0.f, 0.f, Z0 + 11.f), FRotator(0.f, c * 7.f, 0.f), 0.9f + 0.05f * (c % 2));
		}
		M.Frame.Add(EM::Stone, FVector(60.f, 60.f, 6.f), FVector(0.f, 0.f, 243.f), FRotator::ZeroRotator, 0.05f);   // sooty mouth
		M.Frame.Box(EM::Cloth, FVector(68.f, -24.f, 10.f), FVector(72.f, 24.f, 54.f), 1.f, Lava);                    // the glow in the arch
		M.Frame.Box(EM::Stone, FVector(66.f, -34.f, 54.f), FVector(80.f, 34.f, 66.f), 1.15f);
		for (const float Y : { -30.f, 30.f }) M.Frame.Box(EM::Stone, FVector(66.f, Y - 6.f, 0.f), FVector(80.f, Y + 6.f, 54.f), 1.1f);
		M.Frame.Box(EM::Stone, FVector(90.f, -50.f, 0.f), FVector(170.f, 50.f, 50.f), 1.f);                          // ingot table
		for (int32 g = 0; g < 4; ++g) M.Frame.Box(EM::Stone, FVector(100.f + g * 17.f, -16.f, 50.f), FVector(112.f + g * 17.f, 16.f, 52.f), 0.15f);
		M.Frame.Box(EM::Wood, FVector(-200.f, -70.f, 0.f), FVector(-70.f, 70.f, 50.f), 0.8f);                        // bellows bench
		for (const float Y : { -34.f, 34.f })
		{
			M.Frame.Box(EM::Wood, FVector(-190.f, Y - 26.f, 50.f), FVector(-80.f, Y + 26.f, 56.f), 0.85f);           // bellows bottom board
			M.Frame.Rod(EM::Iron, FVector(-80.f, Y, 60.f), FVector(-62.f, Y * 0.3f, 50.f), 6.f);                    // nozzle into the tuyere
		}
		// Posts carrying the cam shaft over the bellows' backs, and the counterweight poles.
		for (const float Y : { -78.f, 78.f }) M.Frame.Box(EM::Wood, FVector(-175.f, Y - 6.f, 0.f), FVector(-163.f, Y + 6.f, 130.f), 0.9f);
		M.Frame.Box(EM::Wood, FVector(-176.f, -84.f, 126.f), FVector(-162.f, 84.f, 136.f), 0.8f);   // the head piece over the cam shaft
		int32 Shaft = 0;
		InputShaft(M, -250.f, -150.f, 120.f, Shaft, 1.f);
		FPartModel& Cams = M.Part(FVector(-169.f, 0.f, 120.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Cams.Blocks.Rod(EM::Iron, FVector(20.f, 0.f, 0.f), FVector(-20.f, 0.f, 0.f), 10.f);
		for (int32 b = 0; b < 2; ++b)
		{
			const float Y = b == 0 ? -34.f : 34.f;
			FPartModel& Cam = M.Part(FVector(-169.f, Y, 120.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
			Cam.Move.Phase = b * PI;
			Cam.Blocks.Rod(EM::Iron, FVector(-8.f, 0.f, 0.f), FVector(8.f, 0.f, 0.f), 10.f);
			Cam.Blocks.Add(EM::Wood, FVector(14.f, 12.f, 30.f), FVector(0.f, 0.f, 14.f), FRotator::ZeroRotator, 0.8f);
			// The bellows' top board, hinged at the nozzle end, pressed by the cam and lifted back by its counterweight.
			FPartModel& Top = M.Part(FVector(-80.f, Y, 58.f), EWRMotion::Swing, FVector::RightVector, 1.f);
			Top.Move.Amp = 7.f;
			Top.Move.Phase = b * PI;
			Top.Blocks.Box(EM::Wood, FVector(-110.f, -26.f, 18.f), FVector(0.f, 26.f, 24.f), 0.85f);
			Top.Blocks.Add(EM::Cloth, FVector(104.f, 50.f, 18.f), FVector(-55.f, 0.f, 9.f), FRotator(-4.f, 0.f, 0.f), 1.f, EWRLookMesh::Cube, Leather);
		}
		M.bLight = true;
		M.LightAt = FVector(110.f, 0.f, 50.f);
		M.LightColour = FLinearColor(1.f, 0.45f, 0.15f);
		M.LightRadius = 700.f;
		M.LightIntensity = 4500.f;
		M.Loop = TEXT("machine.smelter");
	}

	void Pump(FModel& M)
	{
		// A crank on the input shaft rocks a beam on its post; the beam's far end works the piston rod in a wooden barrel
		// standing in a stone sump; the spout pours into a stone trough whose lip is where an irrigation channel joins.
		int32 Shaft = 0;
		InputShaft(M, -130.f, -84.f, 70.f, Shaft, 1.f);
		M.Frame.Box(EM::Wood, FVector(-10.f, -10.f, 0.f), FVector(10.f, 10.f, 160.f), 0.9f);                     // beam post
		M.Frame.Box(EM::Stone, FVector(40.f, -45.f, -20.f), FVector(120.f, 45.f, 25.f), 1.f);                    // sump
		M.Frame.Box(EM::Cloth, FVector(48.f, -37.f, 14.f), FVector(112.f, 37.f, 22.f), 1.f, Water * 0.4f);
		M.Frame.Add(EM::Wood, FVector(40.f, 40.f, 120.f), FVector(80.f, 0.f, 80.f), FRotator::ZeroRotator, 0.85f, EWRLookMesh::Cylinder);  // barrel
		for (const float Z : { 40.f, 80.f, 120.f }) M.Frame.Disc(EM::Iron, FVector(80.f, 0.f, Z), FVector::UpVector, 43.f, 3.f);
		M.Frame.Rod(EM::Wood, FVector(98.f, 0.f, 128.f), FVector(150.f, 0.f, 110.f), 10.f, 0.8f, true);         // spout
		M.Frame.Box(EM::Stone, FVector(130.f, -40.f, 0.f), FVector(205.f, 40.f, 40.f), 1.05f);                  // trough
		M.Frame.Box(EM::Cloth, FVector(136.f, -32.f, 34.f), FVector(200.f, 32.f, 38.f), 1.f, Water * 0.5f);
		FPartModel& Crank = M.Part(FVector(-80.f, 0.f, 70.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Crank.Blocks.Disc(EM::Iron, FVector::ZeroVector, FVector::ForwardVector, 40.f, 7.f);
		Crank.Blocks.Rod(EM::Iron, FVector(0.f, 0.f, 11.f), FVector(8.f, 0.f, 11.f), 5.f);
		// The link from the crank pin up to the beam's near end; the beam rocks on its post so that end follows the
		// link exactly, and its far end works the piston up and down in the barrel.
		FWRMovePart Kin;
		Kin.C = FVector(-76.f, 0.f, 70.f); Kin.U = FVector::UpVector; Kin.V = FVector::RightVector; Kin.CrankR = 11.f; Kin.RodL = 98.f; Kin.S = FVector::UpVector;
		FPartModel& Link = M.Part(FVector::ZeroVector, EWRMotion::Rod, FVector::UpVector, 1.f);
		{ const EWRMotion K = Link.Move.Motion; Link.Move = Kin; Link.Move.Motion = K; }
		Link.Blocks.Rod(EM::Wood, FVector(0.f, 0.f, -49.f), FVector(0.f, 0.f, 49.f), 6.f, 0.8f, true);
		FPartModel& Beam = M.Part(FVector(0.f, 0.f, 168.f), EWRMotion::Beam, FVector::RightVector, 1.f);
		{ const EWRMotion K = Beam.Move.Motion; Beam.Move = Kin; Beam.Move.Motion = K; Beam.Move.Amp = 80.f; }
		Beam.Blocks.Box(EM::Wood, FVector(-84.f, -7.f, -6.f), FVector(90.f, 7.f, 6.f), 0.85f);
		Beam.Blocks.Rod(EM::Iron, FVector(0.f, -12.f, 0.f), FVector(0.f, 12.f, 0.f), 5.f);
		FPartModel& Piston = M.Part(FVector(80.f, 0.f, 150.f), EWRMotion::BeamTip, FVector::UpVector, 1.f);
		{ const EWRMotion K = Piston.Move.Motion; Piston.Move = Kin; Piston.Move.Motion = K; Piston.Move.Amp = 80.f; Piston.Move.TopZ = 168.f; }
		Piston.Blocks.Rod(EM::Iron, FVector(0.f, 0.f, -40.f), FVector(0.f, 0.f, 18.f), 4.f);
		M.Loop = TEXT("machine.pump");
	}

	void Conveyor(FModel& M, float Len)
	{
		const float Half = ConvHalf(Len);
		// The frame: side rails, legs every 2 m sunk to the ground, the leather belt's top and bottom runs.
		for (const float Y : { -34.f, 34.f })
		{
			M.Frame.Box(EM::Wood, FVector(-Len * 0.5f, Y - 4.f, BeltTop - 22.f), FVector(Len * 0.5f, Y + 4.f, BeltTop - 2.f), 0.85f);
			for (float X = -Len * 0.5f + 30.f; X <= Len * 0.5f - 29.f; X += FMath::Max(140.f, (Len - 60.f) / FMath::Max(1.f, FMath::RoundToFloat((Len - 60.f) / 200.f))))
			{
				M.Frame.Box(EM::Wood, FVector(X - 5.f, Y - 5.f, -60.f), FVector(X + 5.f, Y + 5.f, BeltTop - 22.f), 0.9f);
			}
		}
		M.Frame.Box(EM::Cloth, FVector(-Half, -27.f, BeltTop - 2.f), FVector(Half, 27.f, BeltTop), 1.f, Leather);
		M.Frame.Box(EM::Cloth, FVector(-Half, -27.f, BeltTop - 2.f * DrumR - 2.f), FVector(Half, 27.f, BeltTop - 2.f * DrumR), 1.f, Leather * 0.8f);
		// Idler rollers between the drums.
		for (float X = -Half + 50.f; X < Half - 40.f; X += 50.f) M.Frame.Rod(EM::Wood, FVector(X, -28.f, BeltTop - DrumR - 2.f), FVector(X, 28.f, BeltTop - DrumR - 2.f), 6.f, 1.1f);
		// The drums (turn about Y) at each end; the head drum's axle comes out the side as the drive stub.
		for (const float Side : { -1.f, 1.f })
		{
			FPartModel& Drum = M.Part(FVector(Side * Half, 0.f, BeltTop - DrumR), EWRMotion::Spin, FVector::RightVector, 1.f);
			Drum.Blocks.Rod(EM::Wood, FVector(0.f, -29.f, 0.f), FVector(0.f, 29.f, 0.f), DrumR * 2.f - 2.f, 1.f);
			Drum.Blocks.Rod(EM::Iron, FVector(0.f, -40.f, 0.f), FVector(0.f, 40.f, 0.f), 5.f);
			for (int32 k = 0; k < 6; ++k) Drum.Blocks.Rod(EM::Iron, FVector(FMath::Cos(k * PI / 3.f) * DrumR, -29.f, FMath::Sin(k * PI / 3.f) * DrumR), FVector(FMath::Cos(k * PI / 3.f) * DrumR, 29.f, FMath::Sin(k * PI / 3.f) * DrumR), 1.5f);
			M.Frame.Add(EM::Cloth, FVector(10.f, 8.f, 14.f), FVector(Side * Half, -38.f, BeltTop - DrumR), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
			M.Frame.Add(EM::Cloth, FVector(10.f, 8.f, 14.f), FVector(Side * Half, 38.f, BeltTop - DrumR), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		}
		// Cleats riding the belt round its loop.
		const float LoopLen = 4.f * Half + 2.f * PI * DrumR;
		const int32 Cleats = FMath::Max(4, FMath::RoundToInt(LoopLen / 60.f));
		for (int32 c = 0; c < Cleats; ++c)
		{
			FPartModel& Cl = M.Part(FVector::ZeroVector, EWRMotion::Cleat, FVector::RightVector, 1.f);
			Cl.Move.HalfLen = Half;
			Cl.Move.DrumR = DrumR;
			Cl.Move.TopZ = BeltTop;
			Cl.Move.LoopAt = c / float(Cleats);
			Cl.Blocks.Box(EM::Wood, FVector(-2.f, -25.f, 0.f), FVector(2.f, 25.f, 3.f), 0.7f);
		}
		M.BeltLen = Len;
		M.Loop = TEXT("machine.conveyor");
	}

	void Sorter(FModel& M)
	{
		// A bound chest on a low stand; a spindle through its lid carries a bronze chute that turns to the chest it sends to.
		M.Frame.Box(EM::Wood, FVector(-55.f, -36.f, 0.f), FVector(55.f, 36.f, 12.f), 0.8f);
		M.Frame.Box(EM::Wood, FVector(-50.f, -32.f, 12.f), FVector(50.f, 32.f, 70.f), 0.95f);
		M.Frame.Box(EM::Wood, FVector(-52.f, -34.f, 70.f), FVector(52.f, 34.f, 80.f), 0.85f);
		for (const float X : { -38.f, 0.f, 38.f }) M.Frame.Box(EM::Iron, FVector(X - 3.f, -35.f, 12.f), FVector(X + 3.f, 35.f, 81.f), 1.f);
		M.Frame.Add(EM::Cloth, FVector(28.f, 28.f, 8.f), FVector(0.f, 0.f, 84.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Bronze);
		int32 Shaft = 0;
		InputShaft(M, -100.f, -50.f, 40.f, Shaft, 1.f);
		FPartModel& Chute = M.Part(FVector(0.f, 0.f, 88.f), EWRMotion::Chute, FVector::UpVector, 0.f);
		Chute.Blocks.Rod(EM::Iron, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 50.f), 7.f);
		Chute.Blocks.Add(EM::Cloth, FVector(70.f, 18.f, 4.f), FVector(32.f, 0.f, 36.f), FRotator(-18.f, 0.f, 0.f), 1.f, EWRLookMesh::Cube, Bronze);
		for (const float Y : { -9.f, 9.f }) M.Parts.Last().Blocks.Add(EM::Cloth, FVector(70.f, 2.f, 10.f), FVector(32.f, Y, 41.f), FRotator(-18.f, 0.f, 0.f), 1.f, EWRLookMesh::Cube, Bronze * 0.8f);
		M.Loop = NAME_None;
	}

	void CoilLamp(FModel& M, bool bTree)
	{
		// The coil housing at the foot (a copper coil spinning between two lodestones), a post, the lantern(s) above.
		M.Frame.Box(EM::Stone, FVector(-30.f, -30.f, -20.f), FVector(30.f, 30.f, 8.f), 1.f);
		M.Frame.Box(EM::Iron, FVector(-18.f, -22.f, 8.f), FVector(18.f, -16.f, 46.f), 1.f);
		M.Frame.Box(EM::Iron, FVector(-18.f, 16.f, 8.f), FVector(18.f, 22.f, 46.f), 1.f);
		M.Frame.Box(EM::Stone, FVector(-12.f, -16.f, 18.f), FVector(12.f, -10.f, 36.f), 0.12f);   // lodestones
		M.Frame.Box(EM::Stone, FVector(-12.f, 10.f, 18.f), FVector(12.f, 16.f, 36.f), 0.12f);
		M.Frame.Box(EM::Wood, FVector(-7.f, -7.f, 46.f), FVector(7.f, 7.f, bTree ? 260.f : 220.f), 0.85f);
		FPartModel& Coil = M.Part(FVector(0.f, 0.f, 27.f), EWRMotion::Spin, FVector::ForwardVector, 4.f);
		Coil.Blocks.Rod(EM::Iron, FVector(-50.f, 0.f, 0.f), FVector(18.f, 0.f, 0.f), 4.f);
		for (int32 w = 0; w < 8; ++w) Coil.Blocks.Disc(EM::Cloth, FVector(-8.f + w * 2.2f, 0.f, 0.f), FVector::ForwardVector, 16.f, 1.6f, 1.f, Copper);
		Coil.Blocks.Flange(FVector(-48.f, 0.f, 0.f), FVector::ForwardVector);
		const auto LanternAt = [&M](const FVector& At)
		{
			M.Frame.Add(EM::Cloth, FVector(26.f, 26.f, 6.f), At + FVector(0.f, 0.f, 22.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Bronze);
			M.Frame.Add(EM::Cloth, FVector(22.f, 22.f, 14.f), At + FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cone, Bronze);
			M.Frame.Add(EM::Cloth, FVector(20.f, 20.f, 34.f), At, FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, LampGlow);
			M.Frame.Add(EM::Cloth, FVector(24.f, 24.f, 5.f), At - FVector(0.f, 0.f, 19.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Bronze);
		};
		if (bTree)
		{
			for (int32 a = 0; a < 4; ++a)
			{
				const FVector D = FRotator(0.f, a * 90.f + 45.f, 0.f).Vector();
				M.Frame.Rod(EM::Iron, FVector(0.f, 0.f, 250.f), D * 60.f + FVector(0.f, 0.f, 236.f), 4.f);
				LanternAt(D * 60.f + FVector(0.f, 0.f, 210.f));
			}
		}
		else
		{
			M.Frame.Rod(EM::Iron, FVector(0.f, 0.f, 214.f), FVector(32.f, 0.f, 214.f), 4.f);
			LanternAt(FVector(32.f, 0.f, 186.f));
		}
		M.bLight = true;
		M.LightAt = bTree ? FVector(0.f, 0.f, 210.f) : FVector(32.f, 0.f, 186.f);
		M.LightColour = bTree ? FLinearColor(1.f, 0.85f, 0.75f) : FLinearColor(1.f, 0.7f, 0.4f);
		M.LightRadius = bTree ? 900.f : 1400.f;
		M.LightIntensity = bTree ? 7000.f : 5000.f;
		M.Loop = TEXT("machine.coil");
	}

	void Drill(FModel& M)
	{
		// A headframe over the vein (the stamp stands at the piece's origin): four posts, cross beams, two guide yokes
		// the stamp rod slides in; the cam shaft beside it lifts the rod's tappet twice a turn and lets it drop; broken
		// ore slides down a chute into the hopper box at +X.
		for (const float X : { -70.f, 70.f }) for (const float Y : { -70.f, 70.f }) M.Frame.Box(EM::Wood, FVector(X - 10.f, Y - 10.f, -40.f), FVector(X + 10.f, Y + 10.f, 360.f), 0.9f);
		for (const float Z : { 160.f, 300.f, 356.f })
		{
			M.Frame.Box(EM::Wood, FVector(-80.f, -80.f, Z - 8.f), FVector(80.f, -60.f, Z + 8.f), 0.85f);
			M.Frame.Box(EM::Wood, FVector(-80.f, 60.f, Z - 8.f), FVector(80.f, 80.f, Z + 8.f), 0.85f);
		}
		for (const float Z : { 160.f, 300.f })
		{
			M.Frame.Box(EM::Wood, FVector(-12.f, -60.f, Z - 6.f), FVector(-9.f, 60.f, Z + 6.f), 0.8f);
			M.Frame.Box(EM::Wood, FVector(9.f, -60.f, Z - 6.f), FVector(12.f, 60.f, Z + 6.f), 0.8f);
		}
		M.Frame.Rod(EM::Wood, FVector(70.f, -70.f, 200.f), FVector(-70.f, -70.f, 340.f), 9.f, 0.8f, true);
		// Hopper box and chute.
		M.Frame.Box(EM::Wood, FVector(100.f, -45.f, 0.f), FVector(190.f, 45.f, 70.f), 0.85f);
		M.Frame.Box(EM::Stone, FVector(106.f, -39.f, 60.f), FVector(184.f, 39.f, 62.f), 0.2f);
		M.Frame.Add(EM::Wood, FVector(110.f, 40.f, 4.f), FVector(55.f, 0.f, 40.f), FRotator(-22.f, 0.f, 0.f), 0.8f);
		for (const float Y : { -20.f, 20.f }) M.Frame.Add(EM::Wood, FVector(110.f, 3.f, 14.f), FVector(55.f, Y, 46.f), FRotator(-22.f, 0.f, 0.f), 0.7f);
		int32 Shaft = 0;
		InputShaft(M, -150.f, 0.f, 190.f, Shaft, 1.f);
		FPartModel& Cams = M.Part(FVector(0.f, -28.f, 190.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Cams.Blocks.Rod(EM::Iron, FVector(-110.f, 0.f, 0.f), FVector(10.f, 0.f, 0.f), 12.f);
		for (int32 c = 0; c < 2; ++c) Cams.Blocks.Add(EM::Wood, FVector(14.f, 12.f, 40.f), FRotator(0.f, 0.f, 180.f * c).RotateVector(FVector(0.f, 0.f, 18.f)), FRotator(0.f, 0.f, 180.f * c), 0.8f);
		Cams.Blocks.Flange(FVector(-110.f, 0.f, 0.f), FVector::ForwardVector);
		FPartModel& Stamp = M.Part(FVector(0.f, 0.f, 0.f), EWRMotion::Cam, FVector::UpVector, 2.f);
		Stamp.Move.Amp = 34.f;
		Stamp.Blocks.Box(EM::Wood, FVector(-9.f, -9.f, 30.f), FVector(9.f, 9.f, 330.f), 1.f);
		Stamp.Blocks.Box(EM::Iron, FVector(-12.f, -12.f, 2.f), FVector(12.f, 12.f, 34.f), 1.f);          // the iron shoe
		Stamp.Blocks.Box(EM::Wood, FVector(-12.f, -34.f, 200.f), FVector(12.f, -9.f, 214.f), 0.75f);    // the tappet the cams lift
		M.Loop = NAME_None;
	}

	void ColdStore(FModel& M)
	{
		// An insulated plank cabinet with frost on its doors, ice blocks stacked behind a slatted front, a fan wheel on
		// its side on the power stub (turns only where it needs power).
		M.Frame.Box(EM::Stone, FVector(-85.f, -55.f, 0.f), FVector(85.f, 55.f, 14.f), 1.f);
		M.Frame.Box(EM::Wood, FVector(-80.f, -50.f, 14.f), FVector(80.f, 50.f, 210.f), 0.9f);
		M.Frame.Box(EM::Wood, FVector(-84.f, -54.f, 206.f), FVector(84.f, 54.f, 216.f), 0.8f);
		for (const float Y : { -24.f, 24.f }) M.Frame.Box(EM::Wood, FVector(80.f, Y - 23.f, 24.f), FVector(84.f, Y + 23.f, 200.f), 0.75f);
		M.Frame.Box(EM::Cloth, FVector(84.f, -46.f, 24.f), FVector(85.f, 46.f, 60.f), 1.f, Ice);     // frost along the door foot
		// The ice bunker against its back: cut blocks stacked two high, the cold that feeds the cabinet.
		for (int32 i = 0; i < 6; ++i) M.Frame.Add(EM::Cloth, FVector(30.f, 30.f, 30.f), FVector(-102.f, -32.f + (i % 3) * 32.f, 16.f + (i / 3) * 31.f), FRotator(0.f, (i % 2) * 4.f - 2.f, 0.f), 1.f, EWRLookMesh::Cube, Ice);
		M.Frame.Disc(EM::Cloth, FVector(0.f, -56.f, 110.f), FVector::RightVector, 70.f, 6.f, 1.f, Bronze);   // fan shroud
		FPartModel& Fan = M.Part(FVector(0.f, -62.f, 110.f), EWRMotion::Spin, FVector::RightVector, 3.f);
		for (int32 b = 0; b < 5; ++b)
		{
			const float A = b * 2.f * PI / 5.f;
			const FVector Dir(FMath::Cos(A), 0.f, FMath::Sin(A));
			Fan.Blocks.Add(EM::Wood, FVector(28.f, 2.f, 10.f), Dir * 17.f, (FRotationMatrix::MakeFromXZ(Dir, FVector::RightVector).ToQuat() * FQuat(FVector::ForwardVector, 0.35f)).Rotator(), 0.8f);
		}
		Fan.Blocks.Disc(EM::Iron, FVector::ZeroVector, FVector::RightVector, 10.f, 8.f);
		M.Loop = TEXT("machine.coil");
	}

	void LavaForge(FModel& M)
	{
		// A basalt forge built over a lava trough that runs through its hearth; an iron hood and chimney; an anvil.
		M.Frame.Box(EM::Stone, FVector(-90.f, -70.f, 0.f), FVector(90.f, 70.f, 80.f), 0.55f);
		M.Frame.Box(EM::Cloth, FVector(-92.f, -22.f, 70.f), FVector(92.f, 22.f, 78.f), 1.f, Lava);
		M.Frame.Box(EM::Stone, FVector(-90.f, -70.f, 78.f), FVector(90.f, -24.f, 90.f), 0.5f);
		M.Frame.Box(EM::Stone, FVector(-90.f, 24.f, 78.f), FVector(90.f, 70.f, 90.f), 0.5f);
		for (const float X : { -80.f, 80.f }) for (const float Y : { -60.f, 60.f }) M.Frame.Box(EM::Iron, FVector(X - 5.f, Y - 5.f, 90.f), FVector(X + 5.f, Y + 5.f, 200.f), 1.f);
		M.Frame.Add(EM::Iron, FVector(200.f, 160.f, 70.f), FVector(0.f, 0.f, 230.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cone);
		M.Frame.Box(EM::Stone, FVector(-22.f, -22.f, 262.f), FVector(22.f, 22.f, 420.f), 0.5f);
		M.Frame.Box(EM::Wood, FVector(110.f, -25.f, 0.f), FVector(160.f, 25.f, 60.f), 0.7f);   // anvil stump
		M.Frame.Box(EM::Iron, FVector(105.f, -16.f, 60.f), FVector(170.f, 16.f, 82.f), 1.2f);
		M.Frame.Add(EM::Iron, FVector(30.f, 32.f, 22.f), FVector(184.f, 0.f, 71.f), FRotator(90.f, 0.f, 0.f), 1.2f, EWRLookMesh::Cone);   // the horn
		M.bLight = true;
		M.LightAt = FVector(0.f, 0.f, 110.f);
		M.LightColour = FLinearColor(1.f, 0.4f, 0.1f);
		M.LightRadius = 900.f;
		M.LightIntensity = 6000.f;
	}

	void StormCoil(FModel& M)
	{
		M.Frame.Box(EM::Stone, FVector(-80.f, -80.f, 0.f), FVector(80.f, 80.f, 40.f), 1.f);
		M.Frame.Rod(EM::Iron, FVector(0.f, 0.f, 40.f), FVector(0.f, 0.f, 800.f), 18.f);
		for (int32 s = 0; s < 3; ++s)
		{
			const FVector D = FRotator(0.f, s * 120.f, 0.f).Vector();
			M.Frame.Rod(EM::Cloth, D * 380.f + FVector(0.f, 0.f, -20.f), FVector(0.f, 0.f, 600.f), 1.5f, 1.f, false, Rope);   // stays
			M.Frame.Box(EM::Stone, D * 380.f + FVector(-15.f, -15.f, -40.f), D * 380.f + FVector(15.f, 15.f, 10.f), 0.9f);
		}
		for (int32 w = 0; w < 30; ++w)
		{
			const float A = w * 0.7f;
			M.Frame.Add(EM::Cloth, FVector(6.f, 6.f, 26.f), FVector(FMath::Cos(A) * 22.f, FMath::Sin(A) * 22.f, 560.f + w * 6.f), FRotator(0.f, FMath::RadiansToDegrees(A), 90.f), 1.f, EWRLookMesh::Cylinder, Copper);
		}
		for (int32 p = 0; p < 6; ++p) M.Frame.Rod(EM::Cloth, FVector(0.f, 0.f, 790.f), FRotator(40.f, p * 60.f, 0.f).Vector() * 60.f + FVector(0.f, 0.f, 790.f), 4.f, 1.f, false, Copper);   // the crown
		M.Frame.Add(EM::Cloth, FVector(70.f, 70.f, 90.f), FVector(70.f, 0.f, 85.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, StormGlass);   // the battery drum
		M.Frame.Disc(EM::Iron, FVector(70.f, 0.f, 132.f), FVector::UpVector, 76.f, 6.f);
		M.Frame.Disc(EM::Iron, FVector(70.f, 0.f, 38.f), FVector::UpVector, 76.f, 6.f);
		M.Frame.Box(EM::Iron, FVector(-110.f, -30.f, 40.f), FVector(-50.f, 30.f, 130.f), 1.f);   // spark motor
		FPartModel& Fly = M.Part(FVector(-120.f, 0.f, 85.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Fly.Blocks.Rim(EM::Iron, FVector::ZeroVector, FVector::ForwardVector, 40.f, 10.f, 10.f, 12, 1.f);
		for (int32 k = 0; k < 4; ++k) Fly.Blocks.Rod(EM::Iron, FVector::ZeroVector, FVector(0.f, FMath::Cos(k * HALF_PI) * 36.f, FMath::Sin(k * HALF_PI) * 36.f), 5.f);
		Fly.Blocks.Rod(EM::Iron, FVector(-40.f, 0.f, 0.f), FVector(60.f, 0.f, 0.f), 8.f);
		Fly.Blocks.Flange(FVector(-38.f, 0.f, 0.f), FVector::ForwardVector);
		M.bLight = true;
		M.LightAt = FVector(70.f, 0.f, 90.f);
		M.LightColour = FLinearColor(0.5f, 0.7f, 1.f);
		M.LightRadius = 700.f;
		M.LightIntensity = 3000.f;
		M.Loop = TEXT("machine.coil");
	}

	void SunArray(FModel& M)
	{
		// A curved frame of six bronze mirrors (-X side) throwing the sun on a boiler on its mast (+X); the piston engine
		// and flywheel at the mast's foot; a sunstone battery block.
		M.Frame.Box(EM::Stone, FVector(-200.f, -160.f, -10.f), FVector(200.f, 160.f, 10.f), 1.f);
		M.Frame.Rod(EM::Iron, FVector(150.f, 0.f, 10.f), FVector(150.f, 0.f, 330.f), 22.f);
		M.Frame.Add(EM::Cloth, FVector(60.f, 60.f, 70.f), FVector(150.f, 0.f, 360.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Copper * 0.6f);
		M.Frame.Rod(EM::Iron, FVector(150.f, 0.f, 330.f), FVector(150.f, 30.f, 120.f), 8.f);
		M.Frame.Box(EM::Iron, FVector(100.f, 20.f, 10.f), FVector(200.f, 60.f, 110.f), 1.f);
		M.Frame.Add(EM::Cloth, FVector(60.f, 60.f, 50.f), FVector(150.f, -90.f, 35.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, SunStone);
		for (int32 m = 0; m < 6; ++m)
		{
			const float A = FMath::DegreesToRadians(-75.f + m * 30.f);
			const FVector Foot(-40.f - FMath::Cos(A) * 160.f, FMath::Sin(A) * 140.f, 0.f);
			M.Frame.Rod(EM::Iron, Foot, Foot + FVector(0.f, 0.f, 120.f), 8.f);
			FPartModel& Mir = M.Part(Foot + FVector(0.f, 0.f, 130.f), EWRMotion::Mirror, FVector::UpVector, 0.f);
			Mir.Move.Aim = FVector(150.f, 0.f, 360.f);
			Mir.Blocks.Add(EM::Cloth, FVector(4.f, 80.f, 80.f), FVector::ZeroVector, FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, FLinearColor(0.85f, 0.7f, 0.45f));
			Mir.Blocks.Add(EM::Iron, FVector(3.f, 84.f, 84.f), FVector(-3.f, 0.f, 0.f), FRotator::ZeroRotator, 1.f);
		}
		FPartModel& Fly = M.Part(FVector(80.f, 40.f, 60.f), EWRMotion::Spin, FVector::ForwardVector, 1.f);
		Fly.Blocks.Rim(EM::Iron, FVector::ZeroVector, FVector::ForwardVector, 40.f, 10.f, 10.f, 12, 1.f);
		for (int32 k = 0; k < 4; ++k) Fly.Blocks.Rod(EM::Iron, FVector::ZeroVector, FVector(0.f, FMath::Cos(k * HALF_PI) * 36.f, FMath::Sin(k * HALF_PI) * 36.f), 5.f);
		Fly.Blocks.Rod(EM::Iron, FVector(-320.f, 0.f, 0.f), FVector(20.f, 0.f, 0.f), 8.f);
		Fly.Blocks.Flange(FVector(-318.f, 0.f, 0.f), FVector::ForwardVector);
		M.Loop = TEXT("machine.coil");
	}

	void GrovePress(FModel& M)
	{
		// Two heartwood cheeks and a head beam, a great wooden screw turning down through it, the platen on a slatted vat.
		for (const float Y : { -70.f, 70.f }) M.Frame.Box(EM::Wood, FVector(-20.f, Y - 14.f, 0.f), FVector(20.f, Y + 14.f, 280.f), 1.25f);
		M.Frame.Box(EM::Wood, FVector(-24.f, -90.f, 250.f), FVector(24.f, 90.f, 290.f), 1.2f);
		for (int32 s = 0; s < 16; ++s) M.Frame.Add(EM::Wood, FVector(6.f, 24.f, 70.f), FRotator(0.f, s * 22.5f, 0.f).Vector() * 52.f + FVector(0.f, 0.f, 35.f), FRotator(0.f, s * 22.5f, 0.f), 0.85f);
		M.Frame.Disc(EM::Iron, FVector(0.f, 0.f, 20.f), FVector::UpVector, 112.f, 3.f);
		M.Frame.Disc(EM::Iron, FVector(0.f, 0.f, 60.f), FVector::UpVector, 112.f, 3.f);
		M.Frame.Add(EM::Cloth, FVector(96.f, 96.f, 10.f), FVector(0.f, 0.f, 64.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Compost);
		FPartModel& Screw = M.Part(FVector(0.f, 0.f, 0.f), EWRMotion::Spin, FVector::UpVector, 1.f);
		Screw.Blocks.Rod(EM::Wood, FVector(0.f, 0.f, 90.f), FVector(0.f, 0.f, 330.f), 22.f, 1.2f);
		for (int32 t = 0; t < 40; ++t)
		{
			const float A = t * 0.62f;
			Screw.Blocks.Add(EM::Wood, FVector(10.f, 6.f, 5.f), FVector(FMath::Cos(A) * 13.f, FMath::Sin(A) * 13.f, 100.f + t * 5.6f), FRotator(0.f, FMath::RadiansToDegrees(A), 12.f), 1.1f);
		}
		Screw.Blocks.Rod(EM::Wood, FVector(-70.f, 0.f, 320.f), FVector(70.f, 0.f, 320.f), 9.f, 1.f, true);   // the bar
		FPartModel& Platen = M.Part(FVector(0.f, 0.f, 82.f), EWRMotion::Slide, FVector::UpVector, 0.5f);
		Platen.Move.Amp = 6.f;
		Platen.Blocks.Disc(EM::Wood, FVector::ZeroVector, FVector::UpVector, 96.f, 14.f, 1.2f);
	}

	void Lift(FModel& M)
	{
		// A stone pad, a timber gantry, the guide cable's anchor at its top; the platform rides from here to the partner.
		M.Frame.Box(EM::Stone, FVector(-150.f, -150.f, -20.f), FVector(150.f, 150.f, 20.f), 1.05f);
		for (const float Y : { -130.f, 130.f })
		{
			M.Frame.Box(EM::Wood, FVector(-12.f, Y - 12.f, 20.f), FVector(12.f, Y + 12.f, 460.f), 0.9f);
			M.Frame.Rod(EM::Wood, FVector(-120.f, Y, 20.f), FVector(0.f, Y, 300.f), 10.f, 0.85f, true);
		}
		M.Frame.Box(EM::Wood, FVector(-16.f, -146.f, 450.f), FVector(16.f, 146.f, 474.f), 0.85f);
		M.Frame.Add(EM::Cloth, FVector(40.f, 40.f, 30.f), FVector(0.f, 0.f, 440.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Bronze);
		M.Frame.Add(EM::Cloth, FVector(60.f, 60.f, 40.f), FVector(-100.f, 100.f, 40.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cube, Levi);
	}

	void Pylon(FModel& M)
	{
		for (int32 l = 0; l < 3; ++l)
		{
			const FVector D = FRotator(0.f, l * 120.f, 0.f).Vector();
			M.Frame.Rod(EM::Cloth, D * 70.f, FVector(0.f, 0.f, 90.f), 8.f, 1.f, false, Bronze);
		}
		M.Frame.Add(EM::Cloth, FVector(40.f, 40.f, 300.f), FVector(0.f, 0.f, 240.f), FRotator(0.f, 30.f, 0.f), 1.f, EWRLookMesh::Cube, Crystal);
		M.Frame.Add(EM::Cloth, FVector(40.f, 40.f, 60.f), FVector(0.f, 0.f, 420.f), FRotator(0.f, 30.f, 0.f), 1.f, EWRLookMesh::Cone, Crystal);
		FPartModel& Ring = M.Part(FVector(0.f, 0.f, 260.f), EWRMotion::Spin, FVector::UpVector, 1.f);
		Ring.Blocks.Rim(EM::Cloth, FVector::ZeroVector, FVector::UpVector, 70.f, 6.f, 4.f, 16, 1.f);
		M.bLight = true;
		M.LightAt = FVector(0.f, 0.f, 300.f);
		M.LightColour = FLinearColor(0.6f, 0.5f, 1.f);
		M.LightRadius = 600.f;
		M.LightIntensity = 2500.f;
	}

	void Still(FModel& M)
	{
		M.Frame.Box(EM::Stone, FVector(-60.f, -60.f, 0.f), FVector(60.f, 60.f, 40.f), 0.9f);
		M.Frame.Add(EM::Cloth, FVector(70.f, 70.f, 70.f), FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Copper);
		M.Frame.Add(EM::Cloth, FVector(70.f, 70.f, 50.f), FVector(0.f, 0.f, 140.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cone, Copper);
		M.Frame.Rod(EM::Cloth, FVector(0.f, 0.f, 165.f), FVector(80.f, 0.f, 120.f), 6.f, 1.f, false, Copper);
		for (int32 w = 0; w < 14; ++w) M.Frame.Add(EM::Cloth, FVector(5.f, 5.f, 18.f), FVector(90.f + FMath::Cos(w * 0.9f) * 14.f, FMath::Sin(w * 0.9f) * 14.f, 116.f - w * 4.f), FRotator(0.f, w * 52.f, 90.f), 1.f, EWRLookMesh::Cylinder, Copper);
		M.Frame.Add(EM::Cloth, FVector(30.f, 30.f, 36.f), FVector(90.f, 0.f, 18.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Blood);
		FPartModel& Paddle = M.Part(FVector(0.f, 0.f, 80.f), EWRMotion::Spin, FVector::UpVector, 1.f);
		Paddle.Blocks.Rod(EM::Wood, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 120.f), 4.f);
		M.bLight = true;
		M.LightAt = FVector(0.f, 0.f, 20.f);
		M.LightColour = FLinearColor(1.f, 0.4f, 0.15f);
		M.LightRadius = 400.f;
		M.LightIntensity = 1500.f;
	}

	void Vat(FModel& M)
	{
		for (int32 s = 0; s < 20; ++s) M.Frame.Add(EM::Wood, FVector(7.f, 30.f, 110.f), FRotator(0.f, s * 18.f, 0.f).Vector() * 88.f + FVector(0.f, 0.f, 55.f), FRotator(0.f, s * 18.f, 0.f), 0.85f);
		for (const float Z : { 15.f, 55.f, 95.f }) M.Frame.Disc(EM::Iron, FVector(0.f, 0.f, Z), FVector::UpVector, 186.f, 4.f);
		M.Frame.Add(EM::Cloth, FVector(170.f, 170.f, 10.f), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Compost);
		for (int32 c = 0; c < 9; ++c)
		{
			const FVector At = FRotator(0.f, c * 40.f + 10.f, 0.f).Vector() * (30.f + (c % 3) * 18.f) + FVector(0.f, 0.f, 104.f);
			M.Frame.Rod(EM::Cloth, At, At + FVector(0.f, 0.f, 10.f + (c % 3) * 4.f), 4.f, 1.f, false, Cap * 0.9f);
			M.Frame.Add(EM::Cloth, FVector(18.f + (c % 2) * 8.f, 18.f + (c % 2) * 8.f, 5.f), At + FVector(0.f, 0.f, 13.f + (c % 3) * 4.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Cap);
		}
	}

	void RiftChest(FModel& M)
	{
		M.Frame.Box(EM::Wood, FVector(-55.f, -36.f, 0.f), FVector(55.f, 36.f, 64.f), 0.5f);
		M.Frame.Box(EM::Wood, FVector(-57.f, -38.f, 64.f), FVector(57.f, 38.f, 76.f), 0.45f);
		for (const float X : { -44.f, 0.f, 44.f }) M.Frame.Box(EM::Cloth, FVector(X - 4.f, -39.f, 0.f), FVector(X + 4.f, 39.f, 77.f), 1.f, Void);
		M.Frame.Add(EM::Cloth, FVector(30.f, 30.f, 4.f), FVector(0.f, 0.f, 78.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Void);
		FPartModel& Swirl = M.Part(FVector(0.f, 0.f, 80.f), EWRMotion::Spin, FVector::UpVector, 1.f);
		for (int32 a = 0; a < 3; ++a) Swirl.Blocks.Add(EM::Cloth, FVector(22.f, 4.f, 1.f), FRotator(0.f, a * 120.f, 0.f).RotateVector(FVector(6.f, 0.f, 0.f)), FRotator(0.f, a * 120.f + 30.f, 0.f), 1.f, EWRLookMesh::Cube, VoidSwirl);
		M.bLight = true;
		M.LightAt = FVector(0.f, 0.f, 100.f);
		M.LightColour = FLinearColor(0.55f, 0.15f, 1.f);
		M.LightRadius = 300.f;
		M.LightIntensity = 1200.f;
	}

	void Build(FModel& M, const FWRPieceDef& Def, float Hub)
	{
		const FName Id = Def.Id;
		if (Id == TEXT("drive_shaft")) DriveShaft(M, 400.f);
		else if (Id == TEXT("long_drive_shaft")) DriveShaft(M, 600.f);
		else if (Id == TEXT("gearbox")) Gearbox(M);
		else if (Id == TEXT("water_wheel")) WaterWheel(M, Hub, false);
		else if (Id == TEXT("tide_mill")) WaterWheel(M, Hub, true);
		else if (Id == TEXT("windmill")) Windmill(M);
		else if (Id == TEXT("geothermal_turbine")) Turbine(M);
		else if (Id == TEXT("soul_engine")) SoulEngine(M);
		else if (Id == TEXT("sawmill")) Sawmill(M);
		else if (Id == TEXT("grinder")) Grinder(M);
		else if (Id == TEXT("loom")) Loom(M);
		else if (Id == TEXT("smelter")) Smelter(M);
		else if (Id == TEXT("irrigation_pump")) Pump(M);
		else if (Id == TEXT("conveyor")) Conveyor(M, 200.f);
		else if (Id == TEXT("long_conveyor")) Conveyor(M, 600.f);
		else if (Id == TEXT("sorting_chest")) Sorter(M);
		else if (Id == TEXT("coil_lamp")) CoilLamp(M, false);
		else if (Id == TEXT("greenhouse_lights")) CoilLamp(M, true);
		else if (Id == TEXT("drill")) Drill(M);
		else if (Id == TEXT("cold_store")) ColdStore(M);
		else if (Id == TEXT("lava_forge")) LavaForge(M);
		else if (Id == TEXT("storm_coil")) StormCoil(M);
		else if (Id == TEXT("sun_mirror_array")) SunArray(M);
		else if (Id == TEXT("grove_press")) GrovePress(M);
		else if (Id == TEXT("updraft_lift")) Lift(M);
		else if (Id == TEXT("resonance_pylon")) Pylon(M);
		else if (Id == TEXT("blood_still")) Still(M);
		else if (Id == TEXT("mycelium_vat")) Vat(M);
		else if (Id == TEXT("rift_chest")) RiftChest(M);
	}

	/** The cam's lift through one stroke (0..1): raised steadily for 70%, a fall under gravity, a rest on the rock. */
	float CamLift(float t)
	{
		t = t - FMath::FloorToFloat(t);
		if (t < 0.7f) return t / 0.7f;
		if (t < 0.8f) { const float f = (t - 0.7f) / 0.1f; return 1.f - f * f; }
		return 0.f;
	}

	UMaterialInterface* FlatMaterial()
	{
		static UMaterialInterface* Mat = nullptr;
		if (!Mat)
		{
			Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			if (Mat) Mat->AddToRoot();
		}
		return Mat;
	}
	UStaticMesh* ShapeMesh(bool bCylinder)
	{
		static UStaticMesh* Cube = nullptr;
		static UStaticMesh* Cyl = nullptr;
		if (!Cube) { Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")); if (Cube) Cube->AddToRoot(); }
		if (!Cyl) { Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); if (Cyl) Cyl->AddToRoot(); }
		return bCylinder ? Cyl : Cube;
	}

	/** What an item looks like riding a belt: its shape, size and colour by what it is. */
	void RiderLook(FName Item, bool& bCylinder, FVector& Size, FLinearColor& Colour)
	{
		const FString S = Item.ToString();
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Item);
		bCylinder = false;
		Size = FVector(30.f, 24.f, 18.f);
		Colour = FLinearColor(0.35f, 0.25f, 0.15f);   // a sack
		if (S.EndsWith(TEXT("_ore")) || S == TEXT("stone") || S == TEXT("sulfur_crystal") || S == TEXT("star_iron"))
		{
			Size = FVector(26.f, 22.f, 16.f);
			Colour = S.StartsWith(TEXT("copper")) ? FLinearColor(0.3f, 0.2f, 0.12f) : S.StartsWith(TEXT("tin")) ? FLinearColor(0.32f, 0.32f, 0.3f)
				: S.StartsWith(TEXT("sulfur")) ? FLinearColor(0.75f, 0.65f, 0.1f) : S.StartsWith(TEXT("star")) ? FLinearColor(0.05f, 0.06f, 0.1f) : Ore;
		}
		else if (S.EndsWith(TEXT("_ingot")))
		{
			Size = FVector(30.f, 12.f, 8.f);
			Colour = S.StartsWith(TEXT("copper")) ? Copper : S.StartsWith(TEXT("bronze")) ? Bronze : S.StartsWith(TEXT("steel")) ? FLinearColor(0.4f, 0.42f, 0.45f)
				: S.StartsWith(TEXT("starsteel")) ? FLinearColor(0.06f, 0.08f, 0.14f) : FLinearColor(0.12f, 0.12f, 0.12f);
		}
		else if (S == TEXT("wood") || S == TEXT("heartwood"))
		{
			bCylinder = true;
			Size = FVector(16.f, 16.f, 44.f);
			Colour = FLinearColor(0.32f, 0.2f, 0.1f);
		}
		else if (S == TEXT("planks")) { Size = FVector(46.f, 18.f, 8.f); Colour = FLinearColor(0.55f, 0.38f, 0.2f); }
		else if (S == TEXT("linen") || S == TEXT("fiber")) { bCylinder = true; Size = FVector(20.f, 20.f, 30.f); Colour = FLinearColor(0.7f, 0.65f, 0.52f); }
		else if (D && D->Category == TEXT("food")) { Size = FVector(26.f, 22.f, 18.f); Colour = FLinearColor(0.42f, 0.3f, 0.16f); }
	}
}

// ---------------------------------------------------------------- ports and boxes

bool FWRMachineLook::Handles(const FWRPieceDef& Def)
{
	using namespace WRMachineLookDetail;
	return Def.Id == TEXT("lava_forge") || FWRMachines::IsMachine(Def);
}

TArray<FWRPort> FWRMachineLook::LocalPorts(const FWRPieceDef& Def, float Hub)
{
	using namespace WRMachineLookDetail;
	const FName Id = Def.Id;
	TArray<FWRPort> P;
	const FVector X = FVector::ForwardVector, Y = FVector::RightVector;
	if (Id == TEXT("drive_shaft")) { P.Add({ FVector(-200.f, 0.f, 100.f), X }); P.Add({ FVector(200.f, 0.f, 100.f), X }); }
	else if (Id == TEXT("long_drive_shaft")) { P.Add({ FVector(-300.f, 0.f, 100.f), X }); P.Add({ FVector(300.f, 0.f, 100.f), X }); }
	else if (Id == TEXT("gearbox")) { for (const FVector& D : { X, -X, Y, -Y }) P.Add({ D * 72.f + FVector(0.f, 0.f, 100.f), D.GetAbs() }); }
	else if (Id == TEXT("water_wheel") || Id == TEXT("tide_mill")) P.Add({ FVector(-60.f, 0.f, Hub), X });
	else if (Id == TEXT("windmill")) P.Add({ FVector(-288.f, 0.f, 90.f), X });
	else if (Id == TEXT("geothermal_turbine")) P.Add({ FVector(-200.f, 0.f, 150.f), X });
	else if (Id == TEXT("soul_engine")) P.Add({ FVector(-140.f, 0.f, 110.f), X });
	else if (Id == TEXT("storm_coil")) P.Add({ FVector(-160.f, 0.f, 85.f), X });
	else if (Id == TEXT("sun_mirror_array")) P.Add({ FVector(-240.f, 40.f, 60.f), X });
	else if (Id == TEXT("sawmill")) P.Add({ FVector(-190.f, 0.f, 30.f), X });
	else if (Id == TEXT("grinder")) P.Add({ FVector(-150.f, 0.f, 50.f), X });
	else if (Id == TEXT("loom")) P.Add({ FVector(-150.f, 0.f, 30.f), X });
	else if (Id == TEXT("smelter")) P.Add({ FVector(-250.f, 0.f, 120.f), X });
	else if (Id == TEXT("irrigation_pump")) P.Add({ FVector(-130.f, 0.f, 70.f), X });
	else if (Id == TEXT("sorting_chest")) P.Add({ FVector(-100.f, 0.f, 40.f), X });
	else if (Id == TEXT("drill")) P.Add({ FVector(-150.f, 0.f, 190.f), X });
	else if (Id == TEXT("coil_lamp") || Id == TEXT("greenhouse_lights")) P.Add({ FVector(-50.f, 0.f, 27.f), X });
	else if (Id == TEXT("cold_store")) P.Add({ FVector(0.f, -70.f, 110.f), Y });
	else if (Id == TEXT("conveyor") || Id == TEXT("long_conveyor"))
	{
		const float L = Id == TEXT("conveyor") ? 200.f : 600.f;
		const float Half = ConvHalf(L);
		// Belt to belt at the ends (the drums' chain), and the head drum's axle out of each side for a shaft.
		P.Add({ FVector(-L * 0.5f, 0.f, BeltTop - DrumR), X });
		P.Add({ FVector(L * 0.5f, 0.f, BeltTop - DrumR), X });
		P.Add({ FVector(Half, -40.f, BeltTop - DrumR), Y });
		P.Add({ FVector(Half, 40.f, BeltTop - DrumR), Y });
	}
	return P;
}

FVector FWRMachineLook::BeltEnd(const FWRMachineDef& M, bool bHead)
{
	using namespace WRMachineLookDetail;
	return FVector((bHead ? 1.f : -1.f) * M.Length * 0.5f, 0.f, BeltTop);
}

TArray<TPair<FVector, FVector>> FWRMachineLook::CollisionBoxes(const FWRPieceDef& Def, float Hub)
{
	using namespace WRMachineLookDetail;
	TArray<TPair<FVector, FVector>> B;
	const FName Id = Def.Id;
	const auto Box = [&B](const FVector& Min, const FVector& Max) { B.Add({ Max - Min, (Min + Max) * 0.5f }); };
	if (Id == TEXT("drive_shaft")) Box(FVector(-200.f, -10.f, 90.f), FVector(200.f, 10.f, 110.f));
	else if (Id == TEXT("long_drive_shaft")) Box(FVector(-300.f, -10.f, 90.f), FVector(300.f, 10.f, 110.f));
	else if (Id == TEXT("gearbox")) Box(FVector(-55.f, -55.f, 0.f), FVector(55.f, 55.f, 174.f));
	else if (Id == TEXT("water_wheel") || Id == TEXT("tide_mill"))
	{
		const float R = Id == TEXT("tide_mill") ? 260.f : 200.f, W = Id == TEXT("tide_mill") ? 140.f : 110.f;
		Box(FVector(-70.f, -78.f, -60.f), FVector(30.f, 78.f, Hub + 10.f));
		Box(FVector(300.f - W * 0.5f, -R, Hub - R), FVector(300.f + W * 0.5f, R, Hub + R));
	}
	else if (Id == TEXT("windmill")) { Box(FVector(-200.f, -200.f, 0.f), FVector(200.f, 200.f, 660.f)); Box(FVector(-262.f, -60.f, 0.f), FVector(-180.f, 60.f, 160.f)); }
	else if (Id == TEXT("geothermal_turbine")) { Box(FVector(-200.f, -75.f, 0.f), FVector(190.f, 75.f, 230.f)); }
	else if (Id == TEXT("soul_engine")) Box(FVector(-100.f, -80.f, 0.f), FVector(90.f, 80.f, 270.f));
	else if (Id == TEXT("storm_coil")) { Box(FVector(-80.f, -80.f, 0.f), FVector(110.f, 80.f, 140.f)); Box(FVector(-12.f, -12.f, 140.f), FVector(12.f, 12.f, 800.f)); }
	else if (Id == TEXT("sun_mirror_array")) Box(FVector(-200.f, -160.f, -10.f), FVector(200.f, 160.f, 200.f));
	else if (Id == TEXT("sawmill")) { Box(FVector(-150.f, -70.f, 0.f), FVector(160.f, 70.f, 148.f)); Box(FVector(-14.f, -60.f, 148.f), FVector(14.f, 60.f, 366.f)); }
	else if (Id == TEXT("grinder")) Box(FVector(-92.f, -92.f, 0.f), FVector(92.f, 92.f, 230.f));
	else if (Id == TEXT("loom")) Box(FVector(-97.f, -82.f, 0.f), FVector(97.f, 82.f, 172.f));
	else if (Id == TEXT("smelter")) { Box(FVector(-70.f, -70.f, 0.f), FVector(80.f, 70.f, 245.f)); Box(FVector(-200.f, -70.f, 0.f), FVector(-70.f, 70.f, 82.f)); Box(FVector(90.f, -50.f, 0.f), FVector(170.f, 50.f, 52.f)); }
	else if (Id == TEXT("irrigation_pump")) { Box(FVector(-10.f, -10.f, 0.f), FVector(10.f, 10.f, 160.f)); Box(FVector(40.f, -45.f, 0.f), FVector(205.f, 45.f, 40.f)); Box(FVector(60.f, -20.f, 40.f), FVector(100.f, 20.f, 140.f)); }
	else if (Id == TEXT("conveyor")) Box(FVector(-100.f, -38.f, 40.f), FVector(100.f, 38.f, 72.f));
	else if (Id == TEXT("long_conveyor")) Box(FVector(-300.f, -38.f, 40.f), FVector(300.f, 38.f, 72.f));
	else if (Id == TEXT("sorting_chest") || Id == TEXT("rift_chest")) Box(FVector(-57.f, -38.f, 0.f), FVector(57.f, 38.f, 80.f));
	else if (Id == TEXT("coil_lamp")) Box(FVector(-20.f, -22.f, 0.f), FVector(20.f, 22.f, 220.f));
	else if (Id == TEXT("greenhouse_lights")) Box(FVector(-20.f, -22.f, 0.f), FVector(20.f, 22.f, 260.f));
	else if (Id == TEXT("drill")) { for (const float X : { -70.f, 70.f }) for (const float Y : { -70.f, 70.f }) Box(FVector(X - 10.f, Y - 10.f, 0.f), FVector(X + 10.f, Y + 10.f, 360.f)); Box(FVector(100.f, -45.f, 0.f), FVector(190.f, 45.f, 70.f)); }
	else if (Id == TEXT("cold_store")) Box(FVector(-85.f, -55.f, 0.f), FVector(85.f, 55.f, 216.f));
	else if (Id == TEXT("lava_forge")) { Box(FVector(-90.f, -70.f, 0.f), FVector(90.f, 70.f, 90.f)); Box(FVector(105.f, -25.f, 0.f), FVector(170.f, 25.f, 82.f)); }
	else if (Id == TEXT("grove_press")) Box(FVector(-60.f, -90.f, 0.f), FVector(60.f, 90.f, 290.f));
	else if (Id == TEXT("updraft_lift")) { Box(FVector(-150.f, -150.f, -20.f), FVector(150.f, 150.f, 20.f)); for (const float Y : { -130.f, 130.f }) Box(FVector(-12.f, Y - 12.f, 20.f), FVector(12.f, Y + 12.f, 474.f)); }
	else if (Id == TEXT("resonance_pylon")) Box(FVector(-30.f, -30.f, 0.f), FVector(30.f, 30.f, 450.f));
	else if (Id == TEXT("blood_still")) Box(FVector(-60.f, -60.f, 0.f), FVector(110.f, 60.f, 170.f));
	else if (Id == TEXT("mycelium_vat")) Box(FVector(-95.f, -95.f, 0.f), FVector(95.f, 95.f, 112.f));
	return B;
}

bool FWRMachineLook::OwnsPlatform(const AWRBuildPiece* Piece)
{
	using namespace WRMachineLookDetail;
	const FVector A = Piece->GetActorLocation(), B = Piece->Mach.LinkTo.IsZero() ? Piece->Machine.Target : Piece->Mach.LinkTo;
	if (B.IsZero()) return true;
	return A.X < B.X - 1.f || (FMath::IsNearlyEqual(A.X, B.X, 1.f) && A.Y < B.Y);
}

bool FWRMachineLook::LiftPlatform(const AWRBuildPiece* Piece, float ServerTime, FVector& OutWorld)
{
	using namespace WRMachineLookDetail;
	const FVector Partner = Piece->Machine.Target;
	if (Partner.IsZero() || !OwnsPlatform(Piece)) return false;
	const float A = FWRMachines::LiftAlpha(Piece->Machine, ServerTime);
	// The deck hangs under the cable between the two gantries' anchors, a little sag in the middle.
	const FVector From = Piece->GetActorLocation() + FVector(0.f, 0.f, 440.f);
	const FVector To = Partner + FVector(0.f, 0.f, 440.f);
	const float Sag = FVector::Dist(From, To) * 0.03f * 4.f * A * (1.f - A);
	OutWorld = FMath::Lerp(From, To, A) - FVector(0.f, 0.f, 300.f + Sag);
	return true;
}

// ---------------------------------------------------------------- build

void FWRMachineLook::Build(AWRBuildPiece* Piece, const FWRPieceDef& Def)
{
	using namespace WRMachineLookDetail;
	if (!Piece) return;
	const bool bDraws = Piece->GetNetMode() != NM_DedicatedServer;
	const float Hub = Piece->bGhost ? 150.f : (Def.Id == TEXT("water_wheel") || Def.Id == TEXT("tide_mill")) ? FWRMachines::HubZ(Piece, Def, Piece->GetActorTransform()) : 0.f;
	FModel M;
	M.Frame.World = Piece->GetActorTransform();
	WRMachineLookDetail::Build(M, Def, Hub);
	USceneComponent* Root = Piece->GetRootComponent();
	FWRMachineVisual V;
	V.Piece = Piece;
	if (const FWRMachineDef* MD = FWRMachineDefs::Get().Find(Def.Id)) V.Rpm = MD->Rpm;
	if (bDraws)
	{
		// The frame, one Spawn per flat colour.
		const auto SpawnGroups = [Piece](USceneComponent* Parent, const FMB& Blocks)
		{
			TMap<uint32, TArray<FWRLookBlock>> Groups;
			TMap<uint32, FLinearColor> Colours;
			for (const TPair<FLinearColor, FWRLookBlock>& B : Blocks.Blocks)
			{
				const uint32 Key = B.Key.ToFColor(false).DWColor();
				Groups.FindOrAdd(Key).Add(B.Value);
				Colours.Add(Key, B.Key);
			}
			for (const TPair<uint32, TArray<FWRLookBlock>>& G : Groups) FWRBuildLook::Spawn(Piece, Parent, G.Value, Colours[G.Key], false);
		};
		SpawnGroups(Root, M.Frame);
		TArray<USceneComponent*> Made;
		for (const FPartModel& P : M.Parts)
		{
			USceneComponent* Parent = M.Parts.IsValidIndex(P.Parent) && Made.IsValidIndex(P.Parent) ? Made[P.Parent] : Root;
			USceneComponent* Pivot = NewObject<USceneComponent>(Piece);
			Pivot->SetupAttachment(Parent);
			Pivot->SetMobility(EComponentMobility::Movable);
			Pivot->SetRelativeLocationAndRotation(P.Pivot, P.Rot);
			Pivot->RegisterComponent();
			SpawnGroups(Pivot, P.Blocks);
			Made.Add(Pivot);
			FWRMovePart Move = P.Move;
			Move.Comp = Pivot;
			Move.Base = P.Pivot;
			Move.BaseRot = P.Rot;
			V.Parts.Add(Move);
		}
		if (M.bLight && !Piece->bGhost)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(Piece);
			L->SetupAttachment(Root);
			L->SetRelativeLocation(M.LightAt);
			L->SetLightColor(M.LightColour);
			L->SetAttenuationRadius(M.LightRadius);
			L->SetIntensity(0.f);
			L->SetCastShadows(false);
			L->RegisterComponent();
			V.Light = L;
			V.LightIntensity = M.LightIntensity;
		}
		V.LoopEvent = M.Loop;
		V.BeltLen = M.BeltLen;
		V.BeltTop = BeltTop;
	}
	// Lifts: the platform (a plank deck on levistone floats hanging from its trolley), with collision on every machine
	// so a rider stands on it as it glides.
	if (Def.Id == TEXT("updraft_lift") && !Piece->bGhost)
	{
		UBoxComponent* Deck = NewObject<UBoxComponent>(Piece);
		Deck->SetupAttachment(Root);
		Deck->SetUsingAbsoluteLocation(true);
		Deck->SetUsingAbsoluteRotation(true);
		Deck->SetMobility(EComponentMobility::Movable);
		Deck->SetBoxExtent(FVector(110.f, 110.f, 8.f));
		Deck->SetCollisionProfileName(TEXT("BlockAll"));
		Deck->SetWorldLocation(Piece->GetActorLocation() + FVector(0.f, 0.f, 140.f));
		Deck->RegisterComponent();
		V.Platform = Deck;
		if (bDraws)
		{
			FMB Look;
			Look.World = Piece->GetActorTransform();
			for (int32 p = 0; p < 9; ++p) Look.Box(EM::Wood, FVector(-110.f, -110.f + p * 24.5f, -6.f), FVector(110.f, -88.f + p * 24.5f, 6.f), 0.85f + 0.05f * (p % 3));
			for (const float X : { -104.f, 104.f }) Look.Box(EM::Wood, FVector(X - 4.f, -110.f, 6.f), FVector(X + 4.f, 110.f, 100.f), 0.8f);   // rails
			for (const float X : { -80.f, 80.f }) for (const float Y : { -80.f, 80.f }) Look.Add(EM::Cloth, FVector(44.f, 44.f, 30.f), FVector(X, Y, -26.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder, Levi);
			for (const float Y : { -100.f, 100.f }) Look.Rod(EM::Cloth, FVector(0.f, Y, 6.f), FVector(0.f, 0.f, 300.f), 2.f, 1.f, false, Rope);   // hangers up to the trolley
			Look.Add(EM::Iron, FVector(40.f, 20.f, 26.f), FVector(0.f, 0.f, 300.f), FRotator::ZeroRotator, 1.f);
			TMap<uint32, TArray<FWRLookBlock>> Groups;
			TMap<uint32, FLinearColor> Colours;
			for (const TPair<FLinearColor, FWRLookBlock>& B : Look.Blocks) { const uint32 Key = B.Key.ToFColor(false).DWColor(); Groups.FindOrAdd(Key).Add(B.Value); Colours.Add(Key, B.Key); }
			for (const TPair<uint32, TArray<FWRLookBlock>>& G : Groups) FWRBuildLook::Spawn(Piece, Deck, G.Value, Colours[G.Key], true);
			UStaticMeshComponent* Cable = NewObject<UStaticMeshComponent>(Piece);
			Cable->SetStaticMesh(ShapeMesh(true));
			Cable->SetupAttachment(Root);
			Cable->SetUsingAbsoluteLocation(true);
			Cable->SetUsingAbsoluteRotation(true);
			Cable->SetUsingAbsoluteScale(true);
			Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FlatMaterial(), Piece);
			if (MID) { MID->SetVectorParameterValue(TEXT("Color"), Rope * 0.6f); Cable->SetMaterial(0, MID); }
			Cable->SetVisibility(false);
			Cable->RegisterComponent();
			V.Cable = Cable;
		}
	}
	if (!Piece->bGhost && (bDraws || V.Platform.IsValid()))
	{
		if (UWRMachineLookSubsystem* Sub = UWRMachineLookSubsystem::Get(Piece)) Sub->Add(V);
	}
}

// ---------------------------------------------------------------- the animator

UWRMachineLookSubsystem* UWRMachineLookSubsystem::Get(const UObject* WorldContext)
{
	using namespace WRMachineLookDetail;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRMachineLookSubsystem>() : nullptr;
}

TStatId UWRMachineLookSubsystem::GetStatId() const
{
	using namespace WRMachineLookDetail;
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRMachineLookSubsystem, STATGROUP_Tickables);
}

void UWRMachineLookSubsystem::Tick(float DeltaTime)
{
	using namespace WRMachineLookDetail;
	Visuals.RemoveAll([](const FWRMachineVisual& V) { return !V.Piece.IsValid(); });
	FVector Eye = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
	}
	for (FWRMachineVisual& V : Visuals) TickOne(V, DeltaTime, Eye);
}

void UWRMachineLookSubsystem::TickOne(FWRMachineVisual& V, float Dt, const FVector& Eye)
{
	using namespace WRMachineLookDetail;
	AWRBuildPiece* Piece = V.Piece.Get();
	const FWRMachineRep& Rep = Piece->Machine;
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World->GetGameState();
	const float Now = GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : World->GetTimeSeconds();
	const bool bDraws = World->GetNetMode() != NM_DedicatedServer;

	// The lift's platform moves on every machine (its collision carries riders).
	if (USceneComponent* Deck = V.Platform.Get())
	{
		FVector At;
		const bool bOwn = FWRMachineLook::LiftPlatform(Piece, Now, At);
		Deck->SetVisibility(bOwn, true);
		if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Deck)) Body->SetCollisionEnabled(bOwn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bOwn) Deck->SetWorldLocationAndRotation(At, Piece->GetActorRotation());
		if (UStaticMeshComponent* Cable = V.Cable.Get())
		{
			// The guide cable from this gantry's anchor to the partner's (drawn by the lift that owns the platform).
			const FVector A = Piece->GetActorLocation() + FVector(0.f, 0.f, 440.f);
			const FVector B = Rep.Target + FVector(0.f, 0.f, 440.f);
			const bool bShow = bOwn && !Rep.Target.IsZero();
			Cable->SetVisibility(bShow);
			if (bShow)
			{
				const FVector D = B - A;
				Cable->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
				Cable->SetWorldScale3D(FVector(0.03f, 0.03f, D.Size() / 100.f));
			}
		}
		if (bDraws && Rep.TripStart != V.LastTrip)
		{
			if (Rep.TripStart > 0.f) WRSound::PlayAt(Piece, TEXT("machine.start"), Piece->GetActorLocation());
			V.LastTrip = Rep.TripStart;
		}
	}
	if (!bDraws) return;

	// A shaft whose end runs into a station drives it by a bronze pulley and a leather belt down into its side
	// (cooks/build-machinery.md 2.0): looked for now and then, as stations come and go.
	V.StationLook -= Dt;
	if (V.StationLook <= 0.f && V.Parts.Num() > 0)
	{
		V.StationLook = 2.f;
		const FWRMachineDef* MD = FWRMachines::DefOf(Piece);
		if (MD && (MD->Role == TEXT("shaft") || MD->Role == TEXT("gearbox")))
		{
			const TArray<FWRPort> Ports = FWRMachines::Ports(Piece);
			for (int32 i = 0; i < Ports.Num(); ++i)
			{
				if (V.Pulleys.Contains(i)) continue;
				for (TActorIterator<AWRStation> It(GetWorld()); It; ++It)
				{
					if (FVector::DistSquared(It->GetActorLocation(), Ports[i].At) > 400.f * 400.f) continue;
					FVector O, E;
					It->GetActorBounds(true, O, E);
					if (E.IsNearlyZero()) { O = It->GetActorLocation(); E = FVector(60.f); }
					const FBox Box = FBox(O - E, O + E).ExpandBy(FWRMachineDefs::Get().Rule(TEXT("stationReach"), 50.f));
					if (!Box.IsInside(Ports[i].At)) continue;
					// The pulley turns with the shaft (a part of its own on the same axle); the belt runs from its rim down
					// to the station's top edge nearest it.
					const FTransform T = Piece->GetActorTransform();
					const FVector LocalAt = T.InverseTransformPosition(Ports[i].At);
					const FVector LocalAxis = T.InverseTransformVectorNoScale(Ports[i].Axis).GetSafeNormal();
					USceneComponent* Pivot = NewObject<USceneComponent>(Piece);
					Pivot->SetupAttachment(Piece->GetRootComponent());
					Pivot->SetRelativeLocation(LocalAt);
					Pivot->RegisterComponent();
					FMB Wheel;
					Wheel.World = FTransform(T.TransformPosition(LocalAt));
					Wheel.Disc(EM::Cloth, FVector::ZeroVector, LocalAxis, 44.f, 12.f, 1.f, Bronze);
					Wheel.Disc(EM::Cloth, LocalAxis * 7.f, LocalAxis, 50.f, 3.f, 1.f, Bronze * 0.8f);
					Wheel.Disc(EM::Cloth, -LocalAxis * 7.f, LocalAxis, 50.f, 3.f, 1.f, Bronze * 0.8f);
					FVector U, Vv;
					FMB::Basis(LocalAxis, U, Vv);
					for (int32 k = 0; k < 4; ++k) Wheel.Rod(EM::Iron, FVector::ZeroVector, (U * FMath::Cos(k * HALF_PI) + Vv * FMath::Sin(k * HALF_PI)) * 20.f, 4.f);
					TMap<uint32, TArray<FWRLookBlock>> Groups;
					TMap<uint32, FLinearColor> Cols;
					for (const TPair<FLinearColor, FWRLookBlock>& B : Wheel.Blocks) { const uint32 K = B.Key.ToFColor(false).DWColor(); Groups.FindOrAdd(K).Add(B.Value); Cols.Add(K, B.Key); }
					for (const TPair<uint32, TArray<FWRLookBlock>>& G : Groups) FWRBuildLook::Spawn(Piece, Pivot, G.Value, Cols[G.Key], false);
					FWRMovePart Spin;
					Spin.Comp = Pivot;
					Spin.Motion = EWRMotion::Spin;
					Spin.Axis = LocalAxis;
					Spin.Base = LocalAt;
					V.Parts.Add(Spin);
					// The belt: two runs from the pulley's top and bottom to the station's nearest top corner.
					const FVector Into = T.InverseTransformPosition(FVector(FMath::Clamp(Ports[i].At.X, Box.Min.X + 50.f, Box.Max.X - 50.f),
						FMath::Clamp(Ports[i].At.Y, Box.Min.Y + 50.f, Box.Max.Y - 50.f), Box.Max.Z - 60.f));
					FMB Belt;
					Belt.World = T;
					const FVector Up = FVector::UpVector * 20.f;
					Belt.Rod(EM::Cloth, LocalAt + Up, Into + Up * 0.4f, 3.f, 1.f, true, Leather);
					Belt.Rod(EM::Cloth, LocalAt - Up, Into - Up * 0.4f, 3.f, 1.f, true, Leather);
					TArray<FWRLookBlock> Strips;
					for (const TPair<FLinearColor, FWRLookBlock>& B : Belt.Blocks) Strips.Add(B.Value);
					FWRBuildLook::Spawn(Piece, Piece->GetRootComponent(), Strips, Leather, false);
					V.Pulleys.Add(i);
					break;
				}
			}
		}
	}

	// Far away: hold still and quiet (it's out of sight and earshot).
	const float Dist = FVector::Dist(Eye, Piece->GetActorLocation());
	const bool bNear = Dist < 9000.f;
	// Ease toward the replicated speed like a heavy wheel: spin-up and run-down take a second or two.
	const float Target = Rep.Speed100 / 100.f;
	const float Tau = FMath::Max(0.1f, FWRMachineDefs::Get().Rule(TEXT("spinUpSeconds"), 1.6f) / 3.f);
	V.Speed += (Target - V.Speed) * (1.f - FMath::Exp(-Dt / Tau));
	if (V.Speed < 0.002f && Target <= 0.f) V.Speed = 0.f;
	V.Angle = FMath::Fmod(V.Angle + Dt * V.Rpm / 60.f * 2.f * PI * V.Speed, 2.f * PI * 1000.f);
	const bool bTurning = V.Speed > 0.02f;

	if (bNear)
	{
		float ConvD = 0.f;
		if (V.BeltLen > 0.f)
		{
			ConvD = Rep.BeltD + FMath::Max(0.f, Now - Rep.BeltT) * Rep.Speed100;   // cm/s at 100: Speed100 / 100 * 100
		}
		for (FWRMovePart& P : V.Parts)
		{
			USceneComponent* C = P.Comp.Get();
			if (!C) continue;
			const float Th = V.Angle * P.Ratio + P.Phase;
			switch (P.Motion)
			{
			case EWRMotion::Spin:
				C->SetRelativeRotation(P.BaseRot * FQuat(P.Axis, Th));
				break;
			case EWRMotion::Slide:
				C->SetRelativeLocation(P.Base + P.Axis * P.Amp * FMath::Sin(Th));
				break;
			case EWRMotion::Swing:
				C->SetRelativeRotation(P.BaseRot * FQuat(P.Axis, FMath::DegreesToRadians(P.Amp) * FMath::Sin(Th)));
				break;
			case EWRMotion::Cam:
			{
				const float Lift = CamLift(Th / (2.f * PI));
				C->SetRelativeLocation(P.Base + P.Axis * P.Amp * Lift);
				// The blow: a knock and a puff of rock dust each time the stamp lands.
				if (V.CamWas > 0.5f && Lift < 0.05f && bTurning)
				{
					const FVector Shoe = Piece->GetActorTransform().TransformPosition(FVector(0.f, 0.f, 4.f));
					WRSound::PlayAt(Piece, TEXT("machine.drill"), Shoe);
					WRFx::SpawnAt(Piece, TEXT("hit.dust"), Shoe, FRotator::ZeroRotator, 0.7f);
				}
				V.CamWas = Lift;
				break;
			}
			case EWRMotion::Rod:
			case EWRMotion::CrankSlide:
			{
				// The crank pin goes round; the rod's other end slides on its line, exactly (no stretching).
				const FVector Pin = P.C + (P.U * FMath::Cos(Th) + P.V * FMath::Sin(Th)) * P.CrankR;
				const FVector Rel = Pin - P.C;
				const float Along = FVector::DotProduct(Rel, P.S);
				const float Across = (Rel - P.S * Along).Size();
				const float s = Along + FMath::Sqrt(FMath::Max(0.f, P.RodL * P.RodL - Across * Across));
				const FVector End = P.C + P.S * s;
				if (P.Motion == EWRMotion::Rod)
				{
					const FVector D = (End - Pin).GetSafeNormal();
					C->SetRelativeLocationAndRotation((Pin + End) * 0.5f, FRotationMatrix::MakeFromZ(D).ToQuat());
				}
				else
				{
					C->SetRelativeLocation(End);
				}
				break;
			}
			case EWRMotion::Beam:
			case EWRMotion::BeamTip:
			{
				// A rocking beam whose near end (Amp cm toward -X of its pivot) follows a crank's link exactly.
				const FVector Pin = P.C + (P.U * FMath::Cos(Th) + P.V * FMath::Sin(Th)) * P.CrankR;
				const FVector Rel = Pin - P.C;
				const float Along = FVector::DotProduct(Rel, P.S);
				const float Across = (Rel - P.S * Along).Size();
				const float EndZ = P.C.Z + Along + FMath::Sqrt(FMath::Max(0.f, P.RodL * P.RodL - Across * Across));
				const float PivotZ = P.Motion == EWRMotion::Beam ? P.Base.Z : P.TopZ;
				const float Sin = FMath::Clamp((EndZ - PivotZ) / FMath::Max(1.f, P.Amp), -0.5f, 0.5f);
				if (P.Motion == EWRMotion::Beam) C->SetRelativeRotation(P.BaseRot * FQuat(FVector::RightVector, FMath::Asin(Sin)));
				else C->SetRelativeLocation(P.Base - FVector(0.f, 0.f, P.Amp * Sin));
				break;
			}
			case EWRMotion::Cleat:
			{
				// Round the stadium loop: along the top tail -> head, round the head drum, back underneath, round the tail.
				const float Half = P.HalfLen, R = P.DrumR;
				const float Loop = 4.f * Half + 2.f * PI * R;
				float d = FMath::Fmod(ConvD + P.LoopAt * Loop, Loop);
				if (d < 0.f) d += Loop;
				FVector Pos;
				float Pitch = 0.f;
				const float Mid = P.TopZ - R;
				if (d < 2.f * Half) { Pos = FVector(-Half + d, 0.f, P.TopZ); Pitch = 0.f; }
				else if (d < 2.f * Half + PI * R) { const float a = (d - 2.f * Half) / R; Pos = FVector(Half + FMath::Sin(a) * R, 0.f, Mid + FMath::Cos(a) * R); Pitch = -FMath::RadiansToDegrees(a); }
				else if (d < 4.f * Half + PI * R) { Pos = FVector(Half - (d - 2.f * Half - PI * R), 0.f, Mid - R); Pitch = -180.f; }
				else { const float a = (d - 4.f * Half - PI * R) / R; Pos = FVector(-Half - FMath::Sin(a) * R, 0.f, Mid - FMath::Cos(a) * R); Pitch = -180.f - FMath::RadiansToDegrees(a); }
				C->SetRelativeLocationAndRotation(Pos, FRotator(Pitch, 0.f, 0.f));
				break;
			}
			case EWRMotion::Cap:
			{
				// The cap turns into the wind slowly (no snap); the fantail spins while it turns.
				float WindYaw = 0.f;
				FWRMachines::WindAt(Piece, Piece->GetActorLocation(), WindYaw);
				const float Want = FRotator::NormalizeAxis(WindYaw - Piece->GetActorRotation().Yaw);
				const float Diff = FRotator::NormalizeAxis(Want - V.CapYaw);
				const float Step = FMath::Clamp(Diff, -1.f, 1.f) * FMath::Min(FMath::Abs(Diff), FWRMachineDefs::Get().Rule(TEXT("wind.capTurnDegPerSec"), 6.f) * Dt);
				V.CapYaw = FRotator::NormalizeAxis(V.CapYaw + Step);
				V.FanSpin += Dt * (FMath::Abs(Diff) > 0.5f ? 4.f : 0.6f) * 2.f * PI;
				C->SetRelativeRotation(FRotator(0.f, V.CapYaw, 0.f));
				break;
			}
			case EWRMotion::Fan:
				C->SetRelativeRotation(P.BaseRot * FQuat(P.Axis, V.FanSpin));
				break;
			case EWRMotion::Chute:
			{
				if (!Rep.Target.IsZero())
				{
					const FVector Local = Piece->GetActorTransform().InverseTransformPosition(Rep.Target);
					const float Want = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
					const float Diff = FRotator::NormalizeAxis(Want - V.ChuteYaw);
					V.ChuteYaw = FRotator::NormalizeAxis(V.ChuteYaw + FMath::Clamp(Diff, -240.f * Dt, 240.f * Dt));
				}
				C->SetRelativeRotation(FRotator(0.f, V.ChuteYaw, 0.f));
				break;
			}
			case EWRMotion::Mirror:
			{
				// Each mirror's face halves the angle between the sun and the boiler, so the light lands on the boiler.
				const AWRGameState* WGS = AWRGameState::Get(Piece);
				const float Hour = WGS ? WGS->GetHour() : 12.f;
				const float Day = FMath::Clamp((Hour - 6.f) / 12.f, 0.f, 1.f);
				const FVector Sun = FRotator(FMath::Max(8.f, 70.f * FMath::Sin(PI * Day)), 90.f + 180.f * Day, 0.f).Vector();
				const FVector SunLocal = Piece->GetActorTransform().InverseTransformVectorNoScale(Sun);
				const FVector ToBoiler = (P.Aim - P.Base).GetSafeNormal();
				const FVector N = (SunLocal + ToBoiler).GetSafeNormal();
				const FQuat Want = FRotationMatrix::MakeFromXZ(N, FVector::UpVector).ToQuat();
				C->SetRelativeRotation(FQuat::Slerp(C->GetRelativeRotation().Quaternion(), Want, FMath::Min(1.f, Dt * 0.5f)));
				break;
			}
			default:
				break;
			}
		}
	}

	// Belt riders: one shape per stack, gliding from the tail at the belt's run, waiting at the head if it's jammed.
	if (V.BeltLen > 0.f)
	{
		const float D = Rep.BeltD + FMath::Max(0.f, Now - Rep.BeltT) * Rep.Speed100;
		while (V.Riders.Num() < Piece->Belt.Num())
		{
			UStaticMeshComponent* R = NewObject<UStaticMeshComponent>(Piece);
			R->SetupAttachment(Piece->GetRootComponent());
			R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			R->SetCastShadow(true);
			R->RegisterComponent();
			V.Riders.Add(R);
			V.RiderItems.Add(NAME_None);
		}
		for (int32 i = 0; i < V.Riders.Num(); ++i)
		{
			UStaticMeshComponent* R = V.Riders[i].Get();
			if (!R) continue;
			const bool bOn = Piece->Belt.IsValidIndex(i);
			R->SetVisibility(bOn && bNear);
			if (!bOn) continue;
			const FWRBeltItem& It = Piece->Belt[i];
			if (V.RiderItems[i] != It.Item)
			{
				bool bCyl = false;
				FVector Size;
				FLinearColor Col;
				RiderLook(It.Item, bCyl, Size, Col);
				R->SetStaticMesh(ShapeMesh(bCyl));
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FlatMaterial(), Piece);
				if (MID) { MID->SetVectorParameterValue(TEXT("Color"), Col); R->SetMaterial(0, MID); }
				R->SetRelativeScale3D(Size / 100.f);
				V.RiderItems[i] = It.Item;
			}
			const float Along = FMath::Clamp(D - It.Enter, 0.f, V.BeltLen - 20.f);
			const FVector Scale = R->GetRelativeScale3D() * 100.f;
			const bool bLying = R->GetStaticMesh() == ShapeMesh(true);   // logs and bolts lie across the belt
			R->SetRelativeLocationAndRotation(FVector(-V.BeltLen * 0.5f + 10.f + Along, 0.f, V.BeltTop + (bLying ? Scale.X : Scale.Z) * 0.5f),
				bLying ? FRotator(0.f, 0.f, 90.f) : FRotator(0.f, (It.Count % 7) * 6.f - 18.f, 0.f));
		}
	}

	// Light: lamps when lit, fires and glows while working; eased.
	if (UPointLightComponent* L = V.Light.Get())
	{
		const bool bOn = (Rep.Flags & WRMachineFlag::Lit) || (Piece->PieceId == TEXT("lava_forge")) || (Piece->PieceId == TEXT("rift_chest")) || (Piece->PieceId == TEXT("resonance_pylon"));
		const float Want = bOn ? V.LightIntensity * (Piece->PieceId == TEXT("storm_coil") ? 0.2f + 0.8f * Rep.Charge / 255.f : 1.f) : 0.f;
		L->SetIntensity(FMath::FInterpTo(L->Intensity, Want, Dt, 3.f));
	}

	// Sound: the machine's loop follows its speed (fades in on spin-up, out on run-down).
	if (!V.LoopEvent.IsNone())
	{
		UAudioComponent* A = V.Loop.Get();
		const bool bWant = bNear && Dist < FWRMachineDefs::Get().Rule(TEXT("soundRadius"), 3000.f) * 1.2f && V.Speed > 0.03f;
		if (bWant && !A)
		{
			A = WRSound::StartLoop(Piece, V.LoopEvent);
			V.Loop = A;
		}
		if (A)
		{
			A->SetVolumeMultiplier(FMath::Clamp(V.Speed, 0.f, 1.2f) * (bWant ? 1.f : 0.f));
			A->SetPitchMultiplier(0.85f + 0.2f * FMath::Clamp(V.Speed, 0.f, 1.2f));
			if (!bWant && V.Speed <= 0.03f) { A->Stop(); A->DestroyComponent(); V.Loop = nullptr; }
		}
	}

	// Moments: a sort lands (the chute's target changes), a water wheel churns, steam and smoke while working.
	if (Rep.Target != V.LastTarget)
	{
		if (Piece->PieceId == TEXT("sorting_chest") && !Rep.Target.IsZero()) WRSound::PlayAt(Piece, TEXT("machine.sort"), Rep.Target);
		V.LastTarget = Rep.Target;
	}
	if (bNear && bTurning)
	{
		V.FxClock -= Dt;
		if (V.FxClock <= 0.f)
		{
			V.FxClock = 0.6f;
			const FTransform T = Piece->GetActorTransform();
			if (Piece->PieceId == TEXT("water_wheel") || Piece->PieceId == TEXT("tide_mill"))
			{
				const UWRWaterSubsystem* Sea = UWRWaterSubsystem::Get(Piece);
				const FVector At = T.TransformPosition(FVector(300.f, 0.f, 0.f));
				WRFx::SpawnAt(Piece, TEXT("fish.splash"), FVector(At.X, At.Y, Sea ? Sea->GetSurfaceZ() : At.Z), FRotator::ZeroRotator, 1.2f);
			}
			else if (Piece->PieceId == TEXT("geothermal_turbine")) WRFx::SpawnAt(Piece, TEXT("reaction.steam"), T.TransformPosition(FVector(-20.f, 0.f, 340.f)), FRotator::ZeroRotator, 0.8f);
			else if (Piece->PieceId == TEXT("smelter")) WRFx::SpawnAt(Piece, TEXT("hearth.fire"), T.TransformPosition(FVector(0.f, 0.f, 246.f)), FRotator::ZeroRotator, 0.6f);
		}
	}
	if (V.bWasTurning != bTurning)
	{
		const FWRMachineDef* MD = FWRMachines::DefOf(Piece);
		if (MD && MD->IsSource()) WRSound::PlayAt(Piece, bTurning ? FName(TEXT("machine.start")) : FName(TEXT("machine.stop")), Piece->GetActorLocation());
		V.bWasTurning = bTurning;
	}
}
