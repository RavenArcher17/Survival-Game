#include "WRBuildLook.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

// Sizes are cm in the piece's space as AWRBuildPiece::ApplyLook lays it out: walls run along X on a cell edge (cell
// corners at X = +-200, 20 thick around Y = 0, floor at Z = 0), cells span +-200 in X and Y.

namespace WRBuildLookDetail
{
	/** Loaded once and kept (a raw static pointer isn't a GC reference; see LoadRooted in WRBuilding.cpp). */
	template <typename T>
	static T* Rooted(const TCHAR* Path)
	{
		T* Obj = LoadObject<T>(nullptr, Path);
		if (Obj) Obj->AddToRoot();
		return Obj;
	}

	static UStaticMesh* MeshFor(EWRLookMesh Mesh)
	{
		static UStaticMesh* Cube = Rooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"));
		static UStaticMesh* Cylinder = Rooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static UStaticMesh* Cone = Rooted<UStaticMesh>(TEXT("/Engine/BasicShapes/Cone.Cone"));
		return Mesh == EWRLookMesh::Cylinder ? Cylinder : Mesh == EWRLookMesh::Cone ? Cone : Cube;
	}

	static UMaterialInterface* MaterialFor(EWRLookMat Mat)
	{
		static UMaterialInterface* Wood = Rooted<UMaterialInterface>(TEXT("/Game/Building/M_BuildWood.M_BuildWood"));
		static UMaterialInterface* Stone = Rooted<UMaterialInterface>(TEXT("/Game/Building/M_BuildStone.M_BuildStone"));
		static UMaterialInterface* Shape = Rooted<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		return Mat == EWRLookMat::Wood ? Wood : Mat == EWRLookMat::Stone ? Stone : Shape;   // cloth and trim: flat colours
	}

	static constexpr float Corner = 200.f;   // a cell's half width: wall ends, posts
	static constexpr float Dark = 0.25f;     // backing behind planks and mortar behind stone, seen through the gaps
	static constexpr float Iron = 0.2f;      // hinges, straps and bands (stone blocks this dark read as black iron)

	/** Adds blocks in a piece's space; each block's shade and grain come from where it lands in the world. */
	struct FBuilder
	{
		FBuilder(const FTransform& InWorld, TArray<FWRLookBlock>& InOut) : World(InWorld), Out(InOut) {}

		const FTransform& World;
		TArray<FWRLookBlock>& Out;
		/** A sub-frame inside the piece (one side of a foundation or tower): blocks go through it first. */
		FTransform Frame = FTransform::Identity;

		FVector ToWorld(const FVector& Local) const { return World.TransformPosition(Frame.TransformPosition(Local)); }
		float Rand(const FVector& Local, int32 Salt) const { return FWRBuildLook::Hash01(ToWorld(Local), Salt); }
		float Jit(const FVector& Local, int32 Salt) const { return Rand(Local, Salt) * 2.f - 1.f; }

		FWRLookBlock& Add(EWRLookMat Mat, const FVector& Size, const FVector& Centre, const FRotator& Rot = FRotator::ZeroRotator,
			float Dim = 1.f, EWRLookMesh Mesh = EWRLookMesh::Cube)
		{
			FWRLookBlock B;
			B.Size = Size.ComponentMax(FVector(0.5f));
			B.Centre = Frame.TransformPosition(Centre);
			B.Rot = (Frame.GetRotation() * Rot.Quaternion()).Rotator();
			B.Mat = Mat;
			B.Mesh = Mesh;
			B.Dim = Dim;
			B.Tone = Rand(Centre, static_cast<int32>(Mat) * 7 + 1);
			// Grain along the block's longest side (a round log's along its height), as it lies in the world.
			const FVector Long = Mesh != EWRLookMesh::Cube ? FVector::ZAxisVector
				: Size.X >= Size.Y && Size.X >= Size.Z ? FVector::XAxisVector : Size.Y >= Size.Z ? FVector::YAxisVector : FVector::ZAxisVector;
			const FVector Dir = World.TransformVectorNoScale(B.Rot.RotateVector(Long)).GetAbs();
			B.Axis = Dir.Z > 0.7f ? 2.f : Dir.X >= Dir.Y ? 0.f : 1.f;
			return Out.Add_GetRef(B);
		}

		FWRLookBlock& Box(EWRLookMat Mat, const FVector& Min, const FVector& Max, float Dim = 1.f)
		{
			return Add(Mat, Max - Min, (Min + Max) * 0.5f, FRotator::ZeroRotator, Dim);
		}

		/** A square post at X/Y. Its shade comes from its column alone: walls meeting at a corner draw one post. */
		void Post(EWRLookMat Mat, float X, float Y, float Z0, float Z1, float Width)
		{
			FWRLookBlock& B = Box(Mat, FVector(X - Width * 0.5f, Y - Width * 0.5f, Z0), FVector(X + Width * 0.5f, Y + Width * 0.5f, Z1));
			const FVector P = ToWorld(FVector(X, Y, 0.f));
			B.Tone = FWRBuildLook::Hash01(FVector(P.X, P.Y, 0.f), 91);
		}

		void Backing(EWRLookMat Mat, float X0, float X1, float Z0, float Z1, float Thick)
		{
			Box(Mat, FVector(X0, -Thick * 0.5f, Z0), FVector(X1, Thick * 0.5f, Z1), Dark);
		}

		/** Horizontal planks filling X0..X1 x Z0..Z1 of a wall (Thick around Y = 0) with gaps, and a dark backing. */
		void Planks(float X0, float X1, float Z0, float Z1, float Thick, float Board = 26.f, float Gap = 2.5f, bool bBacking = true)
		{
			if (X1 - X0 < 4.f || Z1 - Z0 < 4.f)
			{
				return;
			}
			const int32 N = FMath::Max(1, FMath::RoundToInt((Z1 - Z0) / (Board + Gap)));
			const float Pitch = (Z1 - Z0) / N;
			for (int32 k = 0; k < N; ++k)
			{
				const FVector C((X0 + X1) * 0.5f, 0.f, Z0 + k * Pitch + (Pitch - Gap) * 0.5f);
				Add(EWRLookMat::Wood, FVector(X1 - X0, Thick + Jit(C, 3) * 0.8f, Pitch - Gap), C + FVector(0.f, Jit(C, 4) * 0.5f, 0.f));
			}
			if (bBacking)
			{
				Backing(EWRLookMat::Wood, X0, X1, Z0, Z1, Thick * 0.4f);
			}
		}

		/**
		 * Stone in running bond filling X0..X1 x Z0..Z1 of a wall (Thick around Y = 0): courses Course tall counted
		 * from CourseZ0 (so the parts of one wall share them), blocks of varied length, rough faces, mortar gaps.
		 */
		void Courses(float X0, float X1, float Z0, float Z1, float Thick, float Course = 37.5f, float Gap = 2.f, float CourseZ0 = 0.f, bool bBacking = true)
		{
			if (X1 - X0 < 4.f || Z1 - Z0 < 4.f)
			{
				return;
			}
			for (int32 k = FMath::FloorToInt((Z0 - CourseZ0) / Course + 0.001f); CourseZ0 + k * Course < Z1 - 0.5f; ++k)
			{
				const float CZ0 = FMath::Max(Z0, CourseZ0 + k * Course), CZ1 = FMath::Min(Z1, CourseZ0 + (k + 1) * Course);
				if (CZ1 - CZ0 < Gap + 2.f)
				{
					continue;
				}
				// Every other course starts half a block along; lengths come from the wall's own start, so a window's
				// sides and the courses above and below it line up.
				float X = -Corner - ((k & 1) ? 36.f : 0.f) - Rand(FVector(0.f, 0.f, CourseZ0 + k * Course), 5) * 18.f;
				for (int32 b = 0; X < X1 && b < 48; ++b)
				{
					const float Len = 52.f + Rand(FVector(X, 0.f, CourseZ0 + k * Course), 6) * 38.f;
					const float BX0 = FMath::Max(X, X0), BX1 = FMath::Min(X + Len - Gap, X1);
					if (BX1 - BX0 > 3.f)
					{
						const FVector C((BX0 + BX1) * 0.5f, 0.f, (CZ0 + CZ1 - Gap) * 0.5f);
						Add(EWRLookMat::Stone, FVector(BX1 - BX0, Thick + Jit(C, 7) * 1.4f, CZ1 - CZ0 - Gap), C + FVector(0.f, Jit(C, 8) * 0.6f, 0.f));
					}
					X += Len;
				}
			}
			if (bBacking)
			{
				Backing(EWRLookMat::Stone, X0, X1, Z0, Z1, Thick * 0.5f);
			}
		}

		/** Corner stones at X (both wall ends): one per course, a little proud of the wall, shared with the neighbour. */
		void Quoins(float X, float Z0, float Z1, float Course, float Gap = 2.f)
		{
			for (float Z = Z0; Z < Z1 - Gap - 1.f; Z += Course)
			{
				Box(EWRLookMat::Stone, FVector(X - 13.f, -13.f, Z), FVector(X + 13.f, 13.f, FMath::Min(Z1, Z + Course) - Gap));
			}
		}

		/** Floorboards along X over X0..X1 x Y0..Y1 with their tops at ZTop: rows with staggered joints. */
		void Floorboards(float X0, float X1, float Y0, float Y1, float ZTop, float Thick, float Board = 24.f, float Gap = 1.5f)
		{
			const int32 N = FMath::Max(1, FMath::RoundToInt((Y1 - Y0) / (Board + Gap)));
			const float Pitch = (Y1 - Y0) / N;
			for (int32 r = 0; r < N; ++r)
			{
				const float YC = Y0 + r * Pitch + Pitch * 0.5f;
				const float Split = X0 + (0.25f + 0.5f * Rand(FVector(0.f, YC, ZTop), 21)) * (X1 - X0);
				for (int32 Half = 0; Half < 2; ++Half)
				{
					const float BX0 = Half ? Split + Gap * 0.5f : X0, BX1 = Half ? X1 : Split - Gap * 0.5f;
					const FVector C((BX0 + BX1) * 0.5f, YC, ZTop - Thick * 0.5f);
					Add(EWRLookMat::Wood, FVector(BX1 - BX0, Pitch - Gap, Thick), C + FVector(0.f, 0.f, Jit(C, 22) * 0.3f));
				}
			}
		}

		/** Flagstones over X0..X1 x Y0..Y1, tops at ZTop: rows of varied width, stones of varied length. */
		void Flags(float X0, float X1, float Y0, float Y1, float ZTop, float Thick, float Gap = 2.5f)
		{
			for (float Y = Y0; Y < Y1 - 4.f;)
			{
				const float Row = FMath::Min(45.f + Rand(FVector(0.f, Y, ZTop), 31) * 32.f, Y1 - Y);
				for (float X = X0 - Rand(FVector(1.f, Y, ZTop), 32) * 40.f; X < X1 - 3.f;)
				{
					const float Len = 45.f + Rand(FVector(X, Y, ZTop), 33) * 45.f;
					const float BX0 = FMath::Max(X, X0), BX1 = FMath::Min(X + Len - Gap, X1);
					if (BX1 - BX0 > 3.f)
					{
						const FVector C((BX0 + BX1) * 0.5f, Y + (Row - Gap) * 0.5f, ZTop - Thick * 0.5f);
						Add(EWRLookMat::Stone, FVector(BX1 - BX0, Row - Gap, Thick), C + FVector(0.f, 0.f, Jit(C, 34) * 0.4f));
					}
					X += Len;
				}
				Y += Row;
			}
		}

		/** Round palisade logs filling X0..X1, standing from Z0 to about Z1, each with a sharpened tip. */
		void Logs(float X0, float X1, float Z0, float Z1)
		{
			const int32 N = FMath::Max(1, FMath::RoundToInt((X1 - X0) / 37.f));
			const float Pitch = (X1 - X0) / N;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector Base(X0 + (i + 0.5f) * Pitch, 0.f, Z0);
				const float D = Pitch + 1.f + Jit(Base, 41) * 2.f;
				const float Top = Z1 - 40.f + Jit(Base, 42) * 14.f;
				Add(EWRLookMat::Wood, FVector(D, D, Top - Z0), Base + FVector(Jit(Base, 43), Jit(Base, 44) * 2.f, (Top - Z0) * 0.5f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);
				Add(EWRLookMat::Wood, FVector(D, D, 42.f), FVector(Base.X, Base.Y, Top + 21.f), FRotator::ZeroRotator, 0.9f, EWRLookMesh::Cone);
			}
		}

		// ---------------------------------------------------------------- whole pieces

		void WoodWall(float H)
		{
			Post(EWRLookMat::Wood, -Corner, 0.f, 0.f, H, 24.f);
			Post(EWRLookMat::Wood, Corner, 0.f, 0.f, H, 24.f);
			Box(EWRLookMat::Wood, FVector(-Corner + 12.f, -11.f, 0.f), FVector(Corner - 12.f, 11.f, 14.f));       // sill
			Box(EWRLookMat::Wood, FVector(-Corner + 12.f, -11.f, H - 14.f), FVector(Corner - 12.f, 11.f, H));   // top plate
			Planks(-Corner + 12.f, Corner - 12.f, 14.f, H - 14.f, 18.f);
		}

		static float CourseFor(float H) { return H / FMath::Max(1, FMath::RoundToInt(H / 37.5f)); }

		void StoneWall(float H)
		{
			const float Course = CourseFor(H);
			Courses(-Corner + 13.f, Corner - 13.f, 0.f, H, 20.f, Course);
			Quoins(-Corner, 0.f, H, Course);
			Quoins(Corner, 0.f, H, Course);
		}

		void Window(bool bStone, float H)
		{
			// The opening (X +-80, Z 100..220) as the collision blocks leave it.
			if (bStone)
			{
				const float Course = CourseFor(H);
				Courses(-Corner + 13.f, Corner - 13.f, 0.f, 92.f, 20.f, Course);
				Courses(-Corner + 13.f, Corner - 13.f, 244.f, H, 20.f, Course);
				Courses(-Corner + 13.f, -86.f, 92.f, 244.f, 20.f, Course);
				Courses(86.f, Corner - 13.f, 92.f, 244.f, 20.f, Course);
				Quoins(-Corner, 0.f, H, Course);
				Quoins(Corner, 0.f, H, Course);
				Box(EWRLookMat::Stone, FVector(-98.f, -17.f, 90.f), FVector(98.f, 17.f, 100.f));     // sill
				Box(EWRLookMat::Stone, FVector(-104.f, -12.f, 220.f), FVector(104.f, 12.f, 244.f));  // lintel
				Box(EWRLookMat::Stone, FVector(-86.f, -11.f, 100.f), FVector(-80.f, 11.f, 220.f));   // dressed jambs
				Box(EWRLookMat::Stone, FVector(80.f, -11.f, 100.f), FVector(86.f, 11.f, 220.f));
				return;
			}
			Post(EWRLookMat::Wood, -Corner, 0.f, 0.f, H, 24.f);
			Post(EWRLookMat::Wood, Corner, 0.f, 0.f, H, 24.f);
			Box(EWRLookMat::Wood, FVector(-Corner + 12.f, -11.f, 0.f), FVector(Corner - 12.f, 11.f, 14.f));
			Box(EWRLookMat::Wood, FVector(-Corner + 12.f, -11.f, H - 14.f), FVector(Corner - 12.f, 11.f, H));
			Planks(-Corner + 12.f, Corner - 12.f, 14.f, 92.f, 18.f);
			Planks(-Corner + 12.f, Corner - 12.f, 232.f, H - 14.f, 18.f);
			Planks(-Corner + 12.f, -92.f, 92.f, 232.f, 18.f);
			Planks(92.f, Corner - 12.f, 92.f, 232.f, 18.f);
			Box(EWRLookMat::Wood, FVector(-92.f, -12.f, 92.f), FVector(-80.f, 12.f, 232.f));    // frame
			Box(EWRLookMat::Wood, FVector(80.f, -12.f, 92.f), FVector(92.f, 12.f, 232.f));
			Box(EWRLookMat::Wood, FVector(-96.f, -16.f, 92.f), FVector(96.f, 16.f, 100.f));     // sill, proud of the wall
			Box(EWRLookMat::Wood, FVector(-96.f, -13.f, 220.f), FVector(96.f, 13.f, 232.f));    // head
		}

		void Doorway(bool bStone, float H)
		{
			// The opening (X +-70, up to 240) as the collision blocks leave it.
			if (bStone)
			{
				const float Course = CourseFor(H);
				Courses(-Corner + 13.f, -86.f, 0.f, H, 20.f, Course);
				Courses(86.f, Corner - 13.f, 0.f, H, 20.f, Course);
				Courses(-86.f, 86.f, 270.f, H, 20.f, Course);
				Quoins(-Corner, 0.f, H, Course);
				Quoins(Corner, 0.f, H, Course);
				Box(EWRLookMat::Stone, FVector(-96.f, -13.f, 240.f), FVector(96.f, 13.f, 270.f));   // lintel
				for (float Z = 0.f; Z < 238.f; Z += 40.f)                                               // jamb stones, long and short
				{
					const float W = static_cast<int32>(Z / 40.f) % 2 ? 12.f : 18.f;
					Box(EWRLookMat::Stone, FVector(-70.f - W, -12.f, Z), FVector(-70.f, 12.f, FMath::Min(240.f, Z + 38.f)));
					Box(EWRLookMat::Stone, FVector(70.f, -12.f, Z), FVector(70.f + W, 12.f, FMath::Min(240.f, Z + 38.f)));
				}
				return;
			}
			Post(EWRLookMat::Wood, -Corner, 0.f, 0.f, H, 24.f);
			Post(EWRLookMat::Wood, Corner, 0.f, 0.f, H, 24.f);
			Box(EWRLookMat::Wood, FVector(-Corner + 12.f, -11.f, H - 14.f), FVector(Corner - 12.f, 11.f, H));
			for (const float S : { -1.f, 1.f })
			{
				const float A = S < 0.f ? -Corner + 12.f : 84.f, B = S < 0.f ? -84.f : Corner - 12.f;
				Box(EWRLookMat::Wood, FVector(A, -11.f, 0.f), FVector(B, 11.f, 14.f));
				Planks(A, B, 14.f, H - 14.f, 18.f);
			}
			Box(EWRLookMat::Wood, FVector(-84.f, -13.f, 0.f), FVector(-70.f, 13.f, 254.f));   // jambs
			Box(EWRLookMat::Wood, FVector(70.f, -13.f, 0.f), FVector(84.f, 13.f, 254.f));
			Box(EWRLookMat::Wood, FVector(-84.f, -13.f, 240.f), FVector(84.f, 13.f, 256.f));  // head
			Planks(-70.f, 70.f, 256.f, H - 14.f, 18.f);
		}

		void Gable(bool bStone)
		{
			// Low at -X, 210 up at +X (yaw 180 flips it): rows cut where the slope crosses their top, so nothing pokes
			// above the roof line; the steps left under the slope (dark backing behind) are covered by a barge board /
			// coping as deep as a row.
			const float H = 210.f;
			const auto Edge = [](float Z) { return 2.f * Z - 210.f; };
			const float Slope = FMath::RadiansToDegrees(FMath::Atan2(210.f, 420.f));
			const FVector Normal = FVector(-210.f, 0.f, 420.f).GetSafeNormal();
			const EWRLookMat Mat = bStone ? EWRLookMat::Stone : EWRLookMat::Wood;
			const int32 N = bStone ? 8 : 9;
			const float Pitch = H / N;
			const float Cover = Pitch * FMath::Cos(FMath::DegreesToRadians(Slope)) + 4.f;  // the edge piece's depth across the slope
			for (int32 k = 0; k < N; ++k)
			{
				const float Z0 = k * Pitch, Z1 = Z0 + Pitch;
				const float X0 = FMath::Max(-Corner, Edge(Z1));
				if (bStone)
				{
					Courses(X0, Corner - 13.f, Z0, Z1, 20.f, Pitch, 2.f, 0.f, false);
				}
				else if (Corner - 12.f - X0 > 8.f)
				{
					const FVector C((X0 + Corner - 12.f) * 0.5f, 0.f, Z0 + (Pitch - 2.5f) * 0.5f);
					Add(EWRLookMat::Wood, FVector(Corner - 12.f - X0, 18.f + Jit(C, 3) * 0.8f, Pitch - 2.5f), C);
				}
				Box(Mat, FVector(X0, -3.f, Z0), FVector(Corner - 12.f, 3.f, Z1), Dark);
			}
			// The edge piece's top sits 2 cm proud of the slope, under the roof's own deck.
			if (bStone)
			{
				Quoins(Corner, 0.f, H, Pitch);
				Add(EWRLookMat::Stone, FVector(472.f, 26.f, Cover), FVector(0.f, 0.f, 105.f) - Normal * (Cover * 0.5f - 2.f), FRotator(Slope, 0.f, 0.f));
			}
			else
			{
				Post(EWRLookMat::Wood, Corner, 0.f, 0.f, H, 24.f);
				Add(EWRLookMat::Wood, FVector(470.f, 22.f, Cover), FVector(0.f, 0.f, 105.f) - Normal * (Cover * 0.5f - 2.f), FRotator(Slope, 0.f, 0.f));
			}
		}

		void Roof(bool bStone, float Len)
		{
			// The collision slab rises 200 over the cell toward +X; its underside is Z = (X + 200) / 2 from X -240 (the
			// eave) to 210. On a thin deck sit courses of lapped shingles (wood) or slates (stone), each tipped so its
			// lower edge stands proud of the course below; rafters show underneath.
			const float Th = FMath::Atan2(200.f, 400.f);
			const float Pitch = FMath::RadiansToDegrees(Th);
			const FVector Along(FMath::Cos(Th), 0.f, FMath::Sin(Th)), Up(-FMath::Sin(Th), 0.f, FMath::Cos(Th));
			const float Slope = 450.f / FMath::Cos(Th);
			const FVector Eave(-240.f, 0.f, -20.f);
			const float Deck = 10.f;
			const EWRLookMat Mat = bStone ? EWRLookMat::Stone : EWRLookMat::Wood;
			Add(EWRLookMat::Wood, FVector(Slope, Len, Deck), Eave + Along * (Slope * 0.5f) + Up * (Deck * 0.5f), FRotator(Pitch, 0.f, 0.f), 0.45f);
			for (const float Y : { -150.f, -50.f, 50.f, 150.f })
			{
				Add(EWRLookMat::Wood, FVector(Slope, 12.f, 14.f), Eave + Along * (Slope * 0.5f) - Up * 7.f + FVector(0.f, Y, 0.f), FRotator(Pitch, 0.f, 0.f), 0.8f);
			}
			const float T = bStone ? 2.2f : 2.6f, L = bStone ? 36.f : 52.f, Step = bStone ? 26.f : 40.f;
			const float WMin = bStone ? 20.f : 26.f, WMax = bStone ? 32.f : 42.f, Gap = bStone ? 1.2f : 1.5f;
			const float Tip = FMath::RadiansToDegrees(FMath::Atan2(T, L));
			for (int32 i = 0; i * Step < Slope - 4.f; ++i)
			{
				const float S0 = i * Step, S1 = FMath::Min(S0 + L, Slope);
				for (float Y = -Len * 0.5f - Rand(FVector(S0, 0.f, 0.f), 11) * WMax; Y < Len * 0.5f;)
				{
					const float W = WMin + Rand(FVector(S0, Y, 1.f), 12) * (WMax - WMin);
					const float Y0 = FMath::Max(Y, -Len * 0.5f), Y1 = FMath::Min(Y + W - Gap, Len * 0.5f);
					if (Y1 - Y0 > 4.f)
					{
						const FVector C = Eave + Along * ((S0 + S1) * 0.5f) + Up * (Deck + T) + FVector(0.f, (Y0 + Y1) * 0.5f, 0.f);
						Add(Mat, FVector(S1 - S0, Y1 - Y0, T), C, FRotator(Pitch - Tip, 0.f, 0.f), bStone ? 0.5f + 0.25f * Rand(C, 13) : 1.f);
					}
					Y += W;
				}
			}
		}

		void WoodFoundation()
		{
			Box(EWRLookMat::Wood, FVector(-198.f, -198.f, -150.f), FVector(198.f, 198.f, 93.f), 0.3f);
			Floorboards(-Corner, Corner, -Corner, Corner, 100.f, 7.f);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				Frame = FTransform(FRotator(0.f, 90.f * Side, 0.f), FRotator(0.f, 90.f * Side, 0.f).RotateVector(FVector(0.f, -194.f, 0.f)));
				Planks(-Corner + 13.f, Corner - 13.f, -150.f, 93.f, 12.f, 30.f, 2.5f, false);
			}
			Frame = FTransform::Identity;
			for (const float X : { -Corner, Corner }) for (const float Y : { -Corner, Corner }) Post(EWRLookMat::Wood, X, Y, -150.f, 101.f, 26.f);
		}

		void StoneFoundation()
		{
			Box(EWRLookMat::Stone, FVector(-198.f, -198.f, -150.f), FVector(198.f, 198.f, 92.f), 0.3f);
			Flags(-Corner, Corner, -Corner, Corner, 100.f, 8.f);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				Frame = FTransform(FRotator(0.f, 90.f * Side, 0.f), FRotator(0.f, 90.f * Side, 0.f).RotateVector(FVector(0.f, -193.f, 0.f)));
				Courses(-Corner + 14.f, Corner - 14.f, -150.f, 92.f, 14.f, 40.f, 2.f, -150.f, false);
			}
			Frame = FTransform::Identity;
			for (const float X : { -Corner, Corner }) for (const float Y : { -Corner, Corner })
			{
				for (float Z = -150.f; Z < 90.f; Z += 40.f)
				{
					Box(EWRLookMat::Stone, FVector(X - 14.f, Y - 14.f, Z), FVector(X + 14.f, Y + 14.f, FMath::Min(92.f, Z + 38.f)));
				}
			}
		}

		void Floor(bool bStone)
		{
			if (bStone)
			{
				Box(EWRLookMat::Stone, FVector(-199.f, -199.f, 0.f), FVector(199.f, 199.f, 12.f), 0.35f);
				Flags(-Corner, Corner, -Corner, Corner, 20.f, 8.f);
				return;
			}
			Box(EWRLookMat::Wood, FVector(-199.f, -199.f, 0.f), FVector(199.f, 199.f, 13.5f), 0.45f);
			Floorboards(-Corner, Corner, -Corner, Corner, 20.f, 6.5f);
			for (const float X : { -100.f, 0.f, 100.f })
			{
				Box(EWRLookMat::Wood, FVector(X - 6.f, -199.f, -16.f), FVector(X + 6.f, 199.f, 0.f), 0.85f);  // joists under the ceiling
			}
		}

		void Pillar(bool bStone)
		{
			if (bStone)
			{
				Box(EWRLookMat::Stone, FVector(-16.f, -16.f, 0.f), FVector(16.f, 16.f, 300.f), Dark);
				for (float Z = 0.f; Z < 299.f; Z += 37.5f)
				{
					const FVector C(0.f, 0.f, Z + 18.f);
					const float W = 40.f + Jit(C, 51) * 1.5f;
					Add(EWRLookMat::Stone, FVector(W, W, 35.5f), C);
				}
				return;
			}
			Post(EWRLookMat::Wood, 0.f, 0.f, 8.f, 292.f, 34.f);
			Box(EWRLookMat::Wood, FVector(-21.f, -21.f, 0.f), FVector(21.f, 21.f, 8.f));
			Box(EWRLookMat::Wood, FVector(-21.f, -21.f, 292.f), FVector(21.f, 21.f, 300.f));
		}

		void Stairs(bool bStone)
		{
			// Eight steps a storey up across the cell toward +X (yaw turns it), as the plain treads stand.
			const float Rise = 37.5f;
			for (int32 k = 1; k <= 8; ++k)
			{
				const float X = -225.f + 50.f * k;
				if (bStone)
				{
					Box(EWRLookMat::Stone, FVector(X - 24.f, -99.f, 0.f), FVector(X + 24.f, 99.f, Rise * k - 2.f), Dark);
					for (int32 j = 0; j < k; ++j)
					{
						const float Split = -30.f + 60.f * Rand(FVector(X, 0.f, j * Rise), 61);
						for (int32 Half = 0; Half < 2; ++Half)
						{
							const float Y0 = Half ? Split + 1.f : -100.f, Y1 = Half ? 100.f : Split - 1.f;
							Box(EWRLookMat::Stone, FVector(X - 24.5f, Y0, j * Rise), FVector(X + 24.5f, Y1, (j + 1) * Rise - 1.5f));
						}
					}
				}
				else
				{
					for (const float D : { -12.75f, 12.75f })
					{
						Box(EWRLookMat::Wood, FVector(X + D - 12.f, -100.f, Rise * k - 6.f), FVector(X + D + 12.f, 100.f, Rise * k));
					}
				}
			}
			if (!bStone)
			{
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(300.f, 400.f));
				Add(EWRLookMat::Wood, FVector(500.f, 8.f, 30.f), FVector(0.f, -104.f, 135.f), FRotator(Pitch, 0.f, 0.f));
				Add(EWRLookMat::Wood, FVector(500.f, 8.f, 30.f), FVector(0.f, 104.f, 135.f), FRotator(Pitch, 0.f, 0.f));
			}
		}

		void Tower()
		{
			// A 2 m planked tower (collision 200 x 200 x 500) under a 4 m platform at 5 m.
			Box(EWRLookMat::Wood, FVector(-80.f, -80.f, 0.f), FVector(80.f, 80.f, 500.f), Dark);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				Frame = FTransform(FRotator(0.f, 90.f * Side, 0.f), FRotator(0.f, 90.f * Side, 0.f).RotateVector(FVector(0.f, -93.f, 0.f)));
				Planks(-74.f, 74.f, 0.f, 486.f, 14.f, 30.f, 2.5f, false);
			}
			Frame = FTransform::Identity;
			for (const float X : { -88.f, 88.f }) for (const float Y : { -88.f, 88.f }) Post(EWRLookMat::Wood, X, Y, 0.f, 512.f, 28.f);
			Box(EWRLookMat::Wood, FVector(-200.f, -100.f, 486.f), FVector(200.f, -76.f, 500.f));
			Box(EWRLookMat::Wood, FVector(-200.f, 76.f, 486.f), FVector(200.f, 100.f, 500.f));
			Box(EWRLookMat::Wood, FVector(-199.f, -199.f, 500.f), FVector(199.f, 199.f, 513.f), 0.45f);
			Floorboards(-200.f, 200.f, -200.f, 200.f, 520.f, 7.f);
		}

		void Palisade(float Len)
		{
			Logs(-Len * 0.5f, Len * 0.5f, 0.f, 400.f);
			for (const float Z : { 90.f, 300.f })
			{
				Box(EWRLookMat::Wood, FVector(-Len * 0.5f, 17.f, Z), FVector(Len * 0.5f, 27.f, Z + 18.f), 0.85f);   // binding rails
			}
		}

		void Gate(float Len)
		{
			// Opening 300 wide, 240 high (the leaf), a lintel over it, logs above and at the sides.
			Post(EWRLookMat::Wood, -167.f, 0.f, 0.f, 400.f, 34.f);
			Post(EWRLookMat::Wood, 167.f, 0.f, 0.f, 400.f, 34.f);
			Box(EWRLookMat::Wood, FVector(-184.f, -15.f, 240.f), FVector(184.f, 15.f, 272.f));
			Logs(-150.f, 150.f, 272.f, 400.f);
			Logs(-Len * 0.5f, -184.f, 0.f, 400.f);
			Logs(184.f, Len * 0.5f, 0.f, 400.f);
		}
	};
}

// ---------------------------------------------------------------- FWRBuildLook

float FWRBuildLook::Hash01(const FVector& WorldPoint, int32 Salt)
{
	// 5 cm cells, offset so whole-centimetre positions never sit on a cell boundary (float noise can't flip them).
	const FIntVector Cell(FMath::FloorToInt((WorldPoint.X + 1.3) / 5.0), FMath::FloorToInt((WorldPoint.Y + 1.3) / 5.0), FMath::FloorToInt((WorldPoint.Z + 1.3) / 5.0));
	uint32 H = GetTypeHash(Cell) ^ (static_cast<uint32>(Salt) * 0x9E3779B9u);
	H ^= H >> 16; H *= 0x85EBCA6Bu; H ^= H >> 13; H *= 0xC2B2AE35u; H ^= H >> 16;
	return (H & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

// ---------------------------------------------------------------- farm and kitchen pieces (cooks/build-farming.md)

namespace WRBuildLookDetail
{
	/** The farm's and the kitchen's pieces, each built as the real thing (the plan's 2.8 table and 1.2 references). */
	static bool FarmBlocks(FBuilder& B, const FWRPieceDef& Def)
	{
		const FName Id = Def.Id;
		const FName K = Def.Kind;
		// A ring of fire stones with charcoal in it (the campfire's, the pot's).
		const auto FireRing = [&B](float R, int32 N)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const float A = i * 360.f / N;
				const FVector C(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, 6.f);
				B.Add(EWRLookMat::Stone, FVector(20.f + 6.f * B.Rand(C, 3), 15.f, 12.f + 4.f * B.Rand(C, 5)), C, FRotator(4.f * B.Jit(C, 7), A + 90.f, 6.f * B.Jit(C, 9)));
			}
			B.Add(EWRLookMat::Stone, FVector(R * 1.6f, R * 1.6f, 3.f), FVector(0.f, 0.f, 2.f), FRotator::ZeroRotator, 0.12f, EWRLookMesh::Cylinder);   // charcoal bed
		};
		if (Id == TEXT("campfire"))
		{
			// Nine fire stones, five split logs leaning in a cone over the coals, a spit across two forked sticks.
			FireRing(45.f, 9);
			for (int32 i = 0; i < 5; ++i)
			{
				const float A = i * 72.f + 15.f;
				const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f);
				// Leaning in over the coals: the foot out on the ring, the top over the middle.
				FWRLookBlock& Log = B.Add(EWRLookMat::Wood, FVector(10.f, 10.f, 62.f), Dir * 14.f + FVector(0.f, 0.f, 24.f), FRotator::ZeroRotator, 0.85f, EWRLookMesh::Cylinder);
				Log.Rot = FRotationMatrix::MakeFromZX(-Dir * 0.55f + FVector(0.f, 0.f, 0.85f), Dir).Rotator();
			}
			for (const float X : { -62.f, 62.f })
			{
				B.Post(EWRLookMat::Wood, X, 0.f, 0.f, 72.f, 5.f);
				B.Add(EWRLookMat::Wood, FVector(4.f, 4.f, 16.f), FVector(X - 4.f, 0.f, 78.f), FRotator(0.f, 0.f, 25.f));
				B.Add(EWRLookMat::Wood, FVector(4.f, 4.f, 16.f), FVector(X + 4.f, 0.f, 78.f), FRotator(0.f, 0.f, -25.f));
			}
			B.Add(EWRLookMat::Wood, FVector(3.5f, 3.5f, 150.f), FVector(0.f, 0.f, 74.f), FRotator(90.f, 0.f, 0.f), 1.1f, EWRLookMesh::Cylinder);   // the spit
			return true;
		}
		if (Id == TEXT("cooking_pot"))
		{
			// Its own fire ring, an iron tripod whose three legs meet over it, a chain, a round copper pot and its lid.
			FireRing(42.f, 8);
			for (int32 i = 0; i < 3; ++i)
			{
				const float A = i * 120.f;
				const FVector Foot(FMath::Cos(FMath::DegreesToRadians(A)) * 60.f, FMath::Sin(FMath::DegreesToRadians(A)) * 60.f, 0.f);
				const FVector Apex(0.f, 0.f, 128.f);
				const FVector Leg = Apex - Foot;
				FWRLookBlock& L = B.Add(EWRLookMat::Stone, FVector(4.f, 4.f, Leg.Size()), (Foot + Apex) * 0.5f, FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);
				L.Rot = FRotationMatrix::MakeFromZ(Leg.GetSafeNormal()).Rotator();
			}
			B.Add(EWRLookMat::Stone, FVector(8.f, 8.f, 6.f), FVector(0.f, 0.f, 128.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);   // the ring at the apex
			for (int32 c = 0; c < 6; ++c) B.Add(EWRLookMat::Stone, FVector(2.f, c % 2 ? 4.f : 2.f, 7.f), FVector(0.f, 0.f, 122.f - c * 7.f), FRotator(0.f, c % 2 ? 90.f : 0.f, 0.f), Iron);
			B.Add(EWRLookMat::Cloth, FVector(46.f, 46.f, 24.f), FVector(0.f, 0.f, 70.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);       // the pot
			B.Add(EWRLookMat::Cloth, FVector(50.f, 50.f, 3.f), FVector(0.f, 0.f, 82.f), FRotator::ZeroRotator, 0.8f, EWRLookMesh::Cylinder);       // its rim
			B.Add(EWRLookMat::Cloth, FVector(42.f, 42.f, 4.f), FVector(0.f, 0.f, 85.f), FRotator::ZeroRotator, 0.7f, EWRLookMesh::Cylinder);       // the lid
			B.Add(EWRLookMat::Stone, FVector(5.f, 5.f, 4.f), FVector(0.f, 0.f, 89.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);        // its knob
			B.Add(EWRLookMat::Stone, FVector(52.f, 2.f, 2.f), FVector(0.f, 0.f, 92.f), FRotator::ZeroRotator, Iron);                               // the bail
			return true;
		}
		if (Id == TEXT("oven"))
		{
			// A square plinth with a wood store under the hearth, a dome of coursed stone, an arched mouth, a chimney.
			B.Box(EWRLookMat::Stone, FVector(-70.f, -60.f, 0.f), FVector(70.f, -45.f, 60.f));
			B.Box(EWRLookMat::Stone, FVector(-70.f, 45.f, 0.f), FVector(70.f, 60.f, 60.f));
			B.Box(EWRLookMat::Stone, FVector(-70.f, -45.f, 0.f), FVector(-55.f, 45.f, 60.f));
			B.Box(EWRLookMat::Stone, FVector(-70.f, -62.f, 52.f), FVector(72.f, 62.f, 64.f), 1.1f);   // the hearth slab
			for (int32 l = 0; l < 6; ++l) B.Add(EWRLookMat::Wood, FVector(9.f, 9.f, 95.f), FVector(10.f, -30.f + (l % 3) * 18.f, 10.f + (l / 3) * 17.f), FRotator(90.f, 0.f, 0.f), 0.9f, EWRLookMesh::Cylinder);
			for (int32 c = 0; c < 6; ++c)
			{
				const float R = 58.f * FMath::Cos(c / 6.f * HALF_PI);
				B.Add(EWRLookMat::Stone, FVector(R * 2.f, R * 2.f, 12.f), FVector(-5.f, 0.f, 70.f + c * 11.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);
			}
			B.Box(EWRLookMat::Stone, FVector(48.f, -24.f, 64.f), FVector(58.f, 24.f, 104.f), 0.08f);     // the mouth, dark inside
			B.Box(EWRLookMat::Stone, FVector(50.f, -30.f, 104.f), FVector(62.f, 30.f, 112.f), 1.15f);   // the lintel stone
			B.Box(EWRLookMat::Stone, FVector(-50.f, -16.f, 110.f), FVector(-22.f, 16.f, 190.f));        // the chimney
			B.Box(EWRLookMat::Stone, FVector(-53.f, -19.f, 186.f), FVector(-19.f, 19.f, 194.f), 1.1f);
			return true;
		}
		if (Id == TEXT("smokehouse"))
		{
			// A 2 x 2 m plank shed on sill beams: a door on the front, a gable roof of shingles with a vent at the ridge,
			// a stone fire pit inside (the smoke goes up past the racks of meat and fish).
			for (int32 s = 0; s < 4; ++s)
			{
				B.Frame = FTransform(FRotator(0.f, 90.f * s, 0.f), FRotator(0.f, 90.f * s, 0.f).RotateVector(FVector(0.f, -96.f, 0.f)));
				B.Planks(-92.f, 92.f, 10.f, 200.f, 6.f, 24.f, 2.f, s != 0);
				B.Box(EWRLookMat::Wood, FVector(-100.f, -6.f, 0.f), FVector(100.f, 6.f, 12.f), 0.8f);
			}
			B.Frame = FTransform::Identity;
			for (const float X : { -96.f, 96.f }) for (const float Y : { -96.f, 96.f }) B.Post(EWRLookMat::Wood, X, Y, 0.f, 205.f, 12.f);
			B.Box(EWRLookMat::Wood, FVector(-30.f, -101.f, 12.f), FVector(30.f, -97.f, 180.f), 0.7f);                 // the door
			B.Box(EWRLookMat::Stone, FVector(18.f, -104.f, 95.f), FVector(22.f, -101.f, 105.f), Iron);              // its latch
			for (const float Side : { -1.f, 1.f })
			{
				B.Add(EWRLookMat::Wood, FVector(240.f, 140.f, 8.f), FVector(0.f, Side * 52.f, 238.f), FRotator(0.f, 0.f, Side * 32.f), 0.9f);
			}
			B.Box(EWRLookMat::Wood, FVector(-110.f, -14.f, 268.f), FVector(110.f, 14.f, 276.f), 0.6f);              // the ridge vent cap
			B.Add(EWRLookMat::Stone, FVector(70.f, 70.f, 14.f), FVector(0.f, 0.f, 12.f), FRotator::ZeroRotator, 0.9f, EWRLookMesh::Cylinder);
			return true;
		}
		if (Id == TEXT("charcoal_kiln"))
		{
			// A domed clay-and-stone kiln (cooks/build-crafting.md 2.1, the charcoal burner's beehive kiln): a stone plinth
			// ring, seven courses of dome closing to a chimney stub, an arched fire mouth facing out (+X) with its lintel,
			// six vent holes round the crown that let the smoke out slow, split logs stacked on the right to feed it and a
			// heap of black lump charcoal raked out on the left.
			B.Add(EWRLookMat::Stone, FVector(232.f, 232.f, 30.f), FVector(0.f, 0.f, 15.f), FRotator::ZeroRotator, 1.1f, EWRLookMesh::Cylinder);
			for (int32 c = 0; c < 7; ++c)
			{
				const float R = 104.f * FMath::Cos(c / 7.f * HALF_PI);
				B.Add(EWRLookMat::Stone, FVector(R * 2.f, R * 2.f, 17.f), FVector(0.f, 0.f, 38.f + c * 16.f), FRotator(0.f, c * 23.f, 0.f), 0.85f + 0.06f * (c % 2), EWRLookMesh::Cylinder);
			}
			B.Add(EWRLookMat::Stone, FVector(42.f, 42.f, 44.f), FVector(0.f, 0.f, 162.f), FRotator::ZeroRotator, 0.7f, EWRLookMesh::Cylinder);   // the chimney stub
			B.Add(EWRLookMat::Stone, FVector(30.f, 30.f, 4.f), FVector(0.f, 0.f, 184.5f), FRotator::ZeroRotator, 0.05f, EWRLookMesh::Cylinder); // its sooty mouth
			B.Box(EWRLookMat::Stone, FVector(86.f, -30.f, 30.f), FVector(104.f, 30.f, 92.f), 0.06f);      // the fire mouth, dark inside
			B.Box(EWRLookMat::Stone, FVector(84.f, -40.f, 92.f), FVector(108.f, 40.f, 104.f), 1.2f);     // its lintel stone
			for (const float Y : { -38.f, 38.f }) B.Box(EWRLookMat::Stone, FVector(86.f, Y - 8.f, 30.f), FVector(108.f, Y + 8.f, 92.f), 1.15f);   // jambs
			for (int32 v = 0; v < 6; ++v)
			{
				const float A = v * 60.f + 30.f;
				const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f);
				B.Add(EWRLookMat::Stone, FVector(10.f, 10.f, 6.f), Dir * 64.f + FVector(0.f, 0.f, 118.f), FRotator(0.f, A, 0.f), 0.04f);   // vent holes
			}
			for (int32 Row = 0; Row < 3; ++Row)
			{
				for (int32 L = 0; L < 4 - Row; ++L)
				{
					const FVector C(-20.f + 8.f * B.Jit(FVector(L, Row, 1.f), 3), 146.f + L * 18.f + Row * 9.f, 9.f + Row * 16.f);
					B.Add(EWRLookMat::Wood, FVector(16.f, 16.f, 110.f), C, FRotator(90.f, 0.f, 6.f * B.Jit(C, 5)), 0.85f + 0.1f * B.Rand(C, 7), EWRLookMesh::Cylinder);
				}
			}
			B.Add(EWRLookMat::Stone, FVector(90.f, 90.f, 34.f), FVector(10.f, -160.f, 17.f), FRotator::ZeroRotator, 0.05f, EWRLookMesh::Cone);    // the charcoal heap
			for (int32 k = 0; k < 6; ++k)
			{
				const FVector C(10.f + 48.f * B.Jit(FVector(k, 0.f, 2.f), 11), -160.f + 48.f * B.Jit(FVector(k, 1.f, 2.f), 13), 5.f);
				B.Add(EWRLookMat::Stone, FVector(10.f, 8.f, 7.f), C, FRotator(20.f * B.Jit(C, 3), 90.f * B.Rand(C, 4), 0.f), 0.05f);           // lumps rolled off it
			}
			return true;
		}
		if (Id == TEXT("salting_barrel"))
		{
			// An oak cask with three iron hoops on a low stand, its lid held down by a river stone.
			B.Box(EWRLookMat::Wood, FVector(-38.f, -38.f, 0.f), FVector(38.f, 38.f, 10.f), 0.8f);
			for (int32 s = 0; s < 16; ++s)
			{
				const float A = s * 22.5f;
				const FVector C(FMath::Cos(FMath::DegreesToRadians(A)) * 31.f, FMath::Sin(FMath::DegreesToRadians(A)) * 31.f, 52.f);
				B.Add(EWRLookMat::Wood, FVector(5.f, 12.5f, 84.f), C, FRotator(0.f, A, 0.f));   // staves
			}
			for (const float Z : { 22.f, 52.f, 82.f }) B.Add(EWRLookMat::Stone, FVector(70.f, 70.f, 4.f), FVector(0.f, 0.f, Z), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);
			B.Add(EWRLookMat::Wood, FVector(62.f, 62.f, 4.f), FVector(0.f, 0.f, 93.f), FRotator::ZeroRotator, 0.9f, EWRLookMesh::Cylinder);
			B.Add(EWRLookMat::Stone, FVector(26.f, 20.f, 12.f), FVector(4.f, -3.f, 101.f), FRotator(0.f, 20.f, 0.f), 1.f, EWRLookMesh::Cylinder);
			return true;
		}
		if (Id == TEXT("drying_rack"))
		{
			// Two A-frames of poles lashed at the top, three cross poles hung with strips of meat and fish.
			for (const float X : { -90.f, 90.f })
			{
				for (const float Lean : { -1.f, 1.f }) B.Add(EWRLookMat::Wood, FVector(6.f, 6.f, 175.f), FVector(X, Lean * 22.f, 84.f), FRotator(0.f, 0.f, Lean * 15.f), 0.95f, EWRLookMesh::Cylinder);
				B.Add(EWRLookMat::Trim, FVector(9.f, 9.f, 8.f), FVector(X, 0.f, 165.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);   // the lashing
			}
			const float Heights[] = { 162.f, 125.f, 95.f };
			const float Offs[] = { 0.f, 10.f, 18.f };
			for (int32 p = 0; p < 3; ++p)
			{
				B.Add(EWRLookMat::Wood, FVector(4.f, 4.f, 200.f), FVector(0.f, (p == 0 ? 0.f : (p == 1 ? -Offs[1] : Offs[2])), Heights[p]), FRotator(90.f, 0.f, 0.f), 1.1f, EWRLookMesh::Cylinder);
				for (int32 s = 0; s < 7; ++s)
				{
					const float X = -66.f + s * 22.f;
					const FVector C(X, (p == 0 ? 0.f : (p == 1 ? -Offs[1] : Offs[2])), Heights[p] - 20.f - 6.f * B.Rand(FVector(X, p, 0.f), 4));
					B.Add(EWRLookMat::Cloth, FVector(7.f, 1.5f, 36.f + 10.f * B.Rand(C, 6)), C, FRotator(0.f, 0.f, 4.f * B.Jit(C, 8)));
				}
			}
			return true;
		}
		if (Id == TEXT("compost_bin"))
		{
			// A slatted box (air between the boards), corner posts, dark compost heaped inside.
			for (const float X : { -60.f, 60.f }) for (const float Y : { -50.f, 50.f }) B.Post(EWRLookMat::Wood, X, Y, 0.f, 88.f, 9.f);
			for (int32 r = 0; r < 5; ++r)
			{
				const float Z0 = 6.f + r * 16.f, Z1 = Z0 + 11.f;
				B.Box(EWRLookMat::Wood, FVector(-56.f, -52.f, Z0), FVector(56.f, -48.f, Z1));
				B.Box(EWRLookMat::Wood, FVector(-56.f, 48.f, Z0), FVector(56.f, 52.f, Z1));
				B.Box(EWRLookMat::Wood, FVector(-62.f, -46.f, Z0), FVector(-58.f, 46.f, Z1));
				B.Box(EWRLookMat::Wood, FVector(58.f, -46.f, Z0), FVector(62.f, 46.f, Z1));
			}
			B.Box(EWRLookMat::Cloth, FVector(-56.f, -46.f, 0.f), FVector(56.f, 46.f, 58.f));
			B.Add(EWRLookMat::Cloth, FVector(90.f, 75.f, 20.f), FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 0.9f, EWRLookMesh::Cylinder);
			return true;
		}
		if (Id == TEXT("ice_box"))
		{
			// A stone chest lined with straw, two iron bands, a heavy stone lid with an iron ring to lift it.
			B.Box(EWRLookMat::Stone, FVector(-60.f, -36.f, 0.f), FVector(60.f, 36.f, 58.f));
			B.Box(EWRLookMat::Trim, FVector(-56.f, -32.f, 58.f), FVector(56.f, 32.f, 61.f));          // straw at the seam
			B.Box(EWRLookMat::Stone, FVector(-62.f, -38.f, 61.f), FVector(62.f, 38.f, 72.f), 1.15f);  // the lid
			for (const float X : { -38.f, 38.f })
			{
				B.Box(EWRLookMat::Stone, FVector(X - 4.f, -39.f, 0.f), FVector(X + 4.f, 39.f, 73.f), Iron);
			}
			B.Add(EWRLookMat::Stone, FVector(10.f, 2.f, 10.f), FVector(0.f, -40.f, 62.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);
			return true;
		}
		if (Id == TEXT("salt_pan"))
		{
			// A shallow bed on the upper beach: a stone rim, a clay floor under the brine, a wooden rake.
			B.Box(EWRLookMat::Stone, FVector(-150.f, -100.f, 0.f), FVector(150.f, -88.f, 14.f));
			B.Box(EWRLookMat::Stone, FVector(-150.f, 88.f, 0.f), FVector(150.f, 100.f, 14.f));
			B.Box(EWRLookMat::Stone, FVector(-150.f, -88.f, 0.f), FVector(-138.f, 88.f, 14.f));
			B.Box(EWRLookMat::Stone, FVector(138.f, -88.f, 0.f), FVector(150.f, 88.f, 14.f));
			B.Box(EWRLookMat::Cloth, FVector(-138.f, -88.f, 0.f), FVector(138.f, 88.f, 8.f));
			B.Add(EWRLookMat::Wood, FVector(3.f, 3.f, 170.f), FVector(60.f, 112.f, 4.f), FRotator(90.f, 10.f, 0.f), 1.f, EWRLookMesh::Cylinder);
			B.Add(EWRLookMat::Wood, FVector(6.f, 45.f, 4.f), FVector(-24.f, 97.f, 4.f), FRotator(0.f, 10.f, 0.f));
			return true;
		}
		if (Id == TEXT("well"))
		{
			// A round stone curb (two courses, offset), two posts, the windlass drum and crank, a rope and bucket.
			for (int32 c = 0; c < 2; ++c)
			{
				for (int32 i = 0; i < 12; ++i)
				{
					const float A = i * 30.f + c * 15.f;
					const FVector C(FMath::Cos(FMath::DegreesToRadians(A)) * 54.f, FMath::Sin(FMath::DegreesToRadians(A)) * 54.f, 18.f + c * 34.f);
					B.Add(EWRLookMat::Stone, FVector(26.f, 24.f, 33.f), C, FRotator(0.f, A, 0.f));
				}
			}
			for (const float Y : { -72.f, 72.f }) B.Post(EWRLookMat::Wood, 0.f, Y, 0.f, 185.f, 11.f);
			B.Add(EWRLookMat::Wood, FVector(18.f, 18.f, 130.f), FVector(0.f, 0.f, 160.f), FRotator(0.f, 0.f, 90.f), 0.9f, EWRLookMesh::Cylinder);    // the drum
			B.Add(EWRLookMat::Stone, FVector(4.f, 4.f, 26.f), FVector(0.f, 90.f, 160.f), FRotator(0.f, 0.f, 90.f), Iron, EWRLookMesh::Cylinder);     // its axle out to the crank
			B.Add(EWRLookMat::Stone, FVector(4.f, 4.f, 26.f), FVector(0.f, 103.f, 148.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);       // the crank arm
			B.Add(EWRLookMat::Wood, FVector(4.f, 16.f, 4.f), FVector(0.f, 111.f, 136.f), FRotator::ZeroRotator, 1.f);                                 // its handle
			B.Add(EWRLookMat::Trim, FVector(2.f, 2.f, 50.f), FVector(0.f, 0.f, 128.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);           // the rope
			B.Add(EWRLookMat::Wood, FVector(24.f, 24.f, 24.f), FVector(0.f, 0.f, 92.f), FRotator::ZeroRotator, 0.85f, EWRLookMesh::Cylinder);        // the bucket
			B.Add(EWRLookMat::Stone, FVector(26.f, 26.f, 2.f), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);
			B.Add(EWRLookMat::Stone, FVector(28.f, 2.f, 2.f), FVector(0.f, 0.f, 106.f), FRotator::ZeroRotator, Iron);
			B.Box(EWRLookMat::Wood, FVector(-12.f, -90.f, 185.f), FVector(12.f, 90.f, 193.f), 0.8f);                                                // the beam over it
			return true;
		}
		if (Id == TEXT("irrigation_channel"))
		{
			// A 4 m stone-lined trough: a base slab, two side walls, flat coping stones; the water runs in it when fed.
			B.Box(EWRLookMat::Stone, FVector(-200.f, -35.f, 0.f), FVector(200.f, 35.f, 10.f), 0.85f);
			for (const float Side : { -1.f, 1.f })
			{
				for (int32 s = 0; s < 5; ++s)
				{
					const float X0 = -200.f + s * 80.f;
					B.Box(EWRLookMat::Stone, FVector(X0 + 0.5f, Side > 0.f ? 23.f : -35.f, 10.f), FVector(X0 + 79.5f, Side > 0.f ? 35.f : -23.f, 30.f));
					B.Box(EWRLookMat::Stone, FVector(X0 + 1.f, Side > 0.f ? 21.f : -39.f, 30.f), FVector(X0 + 79.f, Side > 0.f ? 39.f : -21.f, 35.f), 1.15f);
				}
			}
			return true;
		}
		if (Def.bGlass && Def.Shape == TEXT("wall"))
		{
			// Glazing over a stone knee wall: posts at the ends, timber mullions every 50 cm, rails; the panes are
			// drawn in glass by WRFarmLook.
			B.Box(EWRLookMat::Stone, FVector(-210.f, -12.f, 0.f), FVector(210.f, 12.f, 60.f));
			B.Box(EWRLookMat::Stone, FVector(-210.f, -15.f, 58.f), FVector(210.f, 15.f, 64.f), 1.15f);
			for (int32 m = 0; m <= 8; ++m) B.Box(EWRLookMat::Wood, FVector(-204.f + m * 51.f - 3.f, -5.f, 64.f), FVector(-204.f + m * 51.f + 3.f, 5.f, 300.f));
			for (const float Z : { 64.f, 180.f, 294.f }) B.Box(EWRLookMat::Wood, FVector(-210.f, -6.f, Z), FVector(210.f, 6.f, Z + 6.f));
			return true;
		}
		if (Def.bGlass && Def.Shape == TEXT("roof"))
		{
			// Rafters down the slope every 50 cm (the panes between them in glass by WRFarmLook).
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(200.f, 400.f));
			for (int32 r = 0; r <= 8; ++r)
			{
				B.Add(EWRLookMat::Wood, FVector(500.f, 6.f, 10.f), FVector(-15.f, -204.f + r * 51.f, 108.f), FRotator(Pitch, 0.f, 0.f));
			}
			return true;
		}
		if (K == TEXT("crop") && Def.Plot == TEXT("ashpit"))
		{
			// La Geria: a round bed of black picÃ³n ash ringed with basalt, a crescent of stacked stone on its windward side.
			for (int32 i = 0; i < 26; ++i)
			{
				const float A = i * 360.f / 26.f;
				const FVector C(FMath::Cos(FMath::DegreesToRadians(A)) * 172.f, FMath::Sin(FMath::DegreesToRadians(A)) * 172.f, 10.f);
				B.Add(EWRLookMat::Stone, FVector(40.f + 8.f * B.Rand(C, 1), 26.f, 20.f + 6.f * B.Rand(C, 2)), C, FRotator(3.f * B.Jit(C, 3), A + 90.f, 4.f * B.Jit(C, 4)), 0.55f);
			}
			for (int32 c = 0; c < 4; ++c)
			{
				for (int32 i = 0; i < 9; ++i)
				{
					const float A = 140.f + i * 10.f + (c % 2) * 5.f;   // the crescent behind the bed (-X)
					const FVector C(FMath::Cos(FMath::DegreesToRadians(A)) * 196.f, FMath::Sin(FMath::DegreesToRadians(A)) * 196.f, 14.f + c * 24.f);
					B.Add(EWRLookMat::Stone, FVector(38.f, 34.f - c * 3.f, 24.f), C, FRotator(4.f * B.Jit(C, 5), A + 90.f, 0.f), 0.5f);
				}
			}
			B.Add(EWRLookMat::Cloth, FVector(330.f, 330.f, 6.f), FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("crop") && Def.Plot == TEXT("paddy"))
		{
			// An earth bund round a flooded bed.
			for (const float S : { -1.f, 1.f })
			{
				B.Box(EWRLookMat::Cloth, FVector(-200.f, S > 0.f ? 180.f : -200.f, -10.f), FVector(200.f, S > 0.f ? 200.f : -180.f, 25.f), 1.4f);
				B.Box(EWRLookMat::Cloth, FVector(S > 0.f ? 180.f : -200.f, -180.f, -10.f), FVector(S > 0.f ? 200.f : -180.f, 180.f, 25.f), 1.4f);
			}
			return true;
		}
		if (K == TEXT("crop") && Def.Plot == TEXT("kelp"))
		{
			// Two floats and the line the kelp is seeded on.
			for (const float X : { -200.f, 200.f }) B.Add(EWRLookMat::Wood, FVector(40.f, 40.f, 30.f), FVector(X, 0.f, 15.f), FRotator::ZeroRotator, 0.7f, EWRLookMesh::Cylinder);
			B.Add(EWRLookMat::Trim, FVector(2.f, 2.f, 400.f), FVector(0.f, 0.f, 20.f), FRotator(90.f, 0.f, 0.f), 1.f, EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("crop") && Def.Plot == TEXT("log"))
		{
			// Three hardwood logs, drilled and plugged with spawn.
			for (int32 l = 0; l < 3; ++l) B.Add(EWRLookMat::Wood, FVector(36.f, 36.f, 380.f), FVector(0.f, -120.f + l * 120.f, 18.f), FRotator(90.f, 0.f, 0.f), 0.8f, EWRLookMesh::Cylinder);
			return true;
		}
		if (K == TEXT("hive"))
		{
			// A coiled straw skep on a plank stand under a shingled hood on two posts; the entrance at its foot.
			for (const float X : { -35.f, 35.f }) for (const float Y : { -30.f, 30.f }) B.Post(EWRLookMat::Wood, X, Y, 0.f, 60.f, 7.f);
			B.Box(EWRLookMat::Wood, FVector(-45.f, -40.f, 56.f), FVector(45.f, 40.f, 62.f));
			for (int32 c = 0; c < 7; ++c)
			{
				const float R = 30.f * FMath::Cos(c / 7.f * HALF_PI * 1.1f) + 2.f;
				B.Add(EWRLookMat::Trim, FVector(R * 2.f, R * 2.f, 8.f), FVector(0.f, 0.f, 66.f + c * 7.5f), FRotator::ZeroRotator, 0.9f + 0.05f * (c % 2), EWRLookMesh::Cylinder);
			}
			B.Box(EWRLookMat::Stone, FVector(26.f, -6.f, 62.f), FVector(32.f, 6.f, 68.f), 0.05f);   // the entrance
			for (const float X : { -48.f, 48.f }) B.Post(EWRLookMat::Wood, X, -42.f, 0.f, 150.f, 6.f);
			for (const float Side : { -1.f, 1.f }) B.Add(EWRLookMat::Wood, FVector(120.f, 60.f, 4.f), FVector(0.f, Side * 24.f, 150.f), FRotator(0.f, 0.f, Side * 22.f), 0.85f);
			return true;
		}
		if (K == TEXT("trap"))
		{
			// A willow funnel basket on a driven stake: three hoops, staves along it, the throat at the mouth.
			B.Post(EWRLookMat::Wood, -50.f, 0.f, -60.f, 110.f, 7.f);
			// Three hoops of bent willow (each a ring of short lengths), the basket narrowing to its end.
			const float Hoops[] = { 26.f, 22.f, 15.f };
			for (int32 h = 0; h < 3; ++h)
			{
				for (int32 g = 0; g < 12; ++g)
				{
					const float A = g * 30.f;
					const FVector C(-30.f + h * 28.f, FMath::Cos(FMath::DegreesToRadians(A)) * Hoops[h], 26.f + FMath::Sin(FMath::DegreesToRadians(A)) * Hoops[h]);
					B.Add(EWRLookMat::Trim, FVector(2.5f, Hoops[h] * 0.55f, 2.5f), C, FRotator(0.f, 0.f, A + 90.f), 0.75f);
				}
			}
			for (int32 s = 0; s < 10; ++s)
			{
				const float A = s * 36.f;
				const FVector C(-2.f, FMath::Cos(FMath::DegreesToRadians(A)) * 22.f, 26.f + FMath::Sin(FMath::DegreesToRadians(A)) * 22.f);
				B.Add(EWRLookMat::Trim, FVector(66.f, 1.5f, 1.5f), C, FRotator(0.f, 0.f, A), 0.7f);
			}
			return true;
		}
		if (!Def.Trophy.IsNone())
		{
			// An oak board of three planks with two iron pins the fish hangs on (the fish is WRFarmLook's).
			for (int32 i = 0; i < 3; ++i) B.Box(EWRLookMat::Wood, FVector(-72.f, -10.f, 120.f + i * 20.f + 0.5f), FVector(72.f, -4.f, 120.f + (i + 1) * 20.f - 0.5f));
			for (const float X : { -24.f, 24.f }) B.Add(EWRLookMat::Stone, FVector(2.5f, 2.5f, 12.f), FVector(X, 0.f, 150.f), FRotator(0.f, 0.f, 90.f), Iron, EWRLookMesh::Cylinder);
			return true;
		}
		return false;
	}
}

TArray<FWRLookBlock> FWRBuildLook::Blocks(const FWRPieceDef& Def, const FTransform& PieceToWorld)
{
	using namespace WRBuildLookDetail;
	TArray<FWRLookBlock> Out;
	FBuilder B(PieceToWorld, Out);
	const FName S = Def.Shape;
	const FName K = Def.Kind;
	const bool bStone = Def.Material == TEXT("stone");
	const float Len = S == TEXT("palisade") || S == TEXT("gate") ? 440.f : 420.f;
	if (FarmBlocks(B, Def)) {}
	else if (S == TEXT("foundation")) bStone ? B.StoneFoundation() : B.WoodFoundation();
	else if (S == TEXT("wall")) bStone ? B.StoneWall(300.f) : B.WoodWall(300.f);
	else if (S == TEXT("halfwall")) bStone ? B.StoneWall(150.f) : B.WoodWall(150.f);
	else if (S == TEXT("upperwall")) bStone ? B.StoneWall(210.f) : B.WoodWall(210.f);
	else if (S == TEXT("floor")) B.Floor(bStone);
	else if (S == TEXT("roof")) B.Roof(bStone, Len);
	else if (S == TEXT("gable")) B.Gable(bStone);
	else if (S == TEXT("pillar")) B.Pillar(bStone);
	else if (S == TEXT("stairs")) B.Stairs(bStone);
	else if (S == TEXT("window")) B.Window(bStone, 300.f);
	else if (S == TEXT("door") || S == TEXT("doorway")) B.Doorway(bStone, 300.f);
	else if (S == TEXT("palisade")) B.Palisade(Len);
	else if (S == TEXT("gate")) B.Gate(Len);
	else if (S == TEXT("tower")) B.Tower();
	else if (K == TEXT("bed") && Def.Id == TEXT("bedroll"))
	{
		B.Box(EWRLookMat::Cloth, FVector(-100.f, -38.f, 0.f), FVector(78.f, 38.f, 6.f));
		B.Add(EWRLookMat::Cloth, FVector(24.f, 24.f, 76.f), FVector(88.f, 0.f, 12.f), FRotator(0.f, 0.f, 90.f), 1.f, EWRLookMesh::Cylinder);  // rolled end
	}
	else if (K == TEXT("bed"))
	{
		for (const float X : { -104.f, 104.f }) for (const float Y : { -44.f, 44.f }) B.Box(EWRLookMat::Wood, FVector(X - 4.f, Y - 4.f, 0.f), FVector(X + 4.f, Y + 4.f, 30.f));
		B.Box(EWRLookMat::Wood, FVector(-108.f, -48.f, 16.f), FVector(108.f, -42.f, 32.f));   // side rails
		B.Box(EWRLookMat::Wood, FVector(-108.f, 42.f, 16.f), FVector(108.f, 48.f, 32.f));
		B.Box(EWRLookMat::Wood, FVector(-110.f, -48.f, 0.f), FVector(-104.f, 48.f, 78.f));     // headboard
		B.Box(EWRLookMat::Wood, FVector(104.f, -48.f, 0.f), FVector(110.f, 48.f, 44.f));       // footboard
		B.Box(EWRLookMat::Cloth, FVector(-104.f, -42.f, 22.f), FVector(104.f, 42.f, 40.f));    // mattress and cover
		B.Box(EWRLookMat::Cloth, FVector(-100.f, -30.f, 40.f), FVector(-72.f, 30.f, 50.f));    // pillow
	}
	else if (Def.Id == TEXT("table"))
	{
		for (int32 i = 0; i < 3; ++i) B.Box(EWRLookMat::Wood, FVector(-75.f, -45.f + i * 30.f + 0.5f, 74.f), FVector(75.f, -45.f + (i + 1) * 30.f - 0.5f, 80.f));
		for (const float X : { -66.f, 66.f }) for (const float Y : { -36.f, 36.f }) B.Box(EWRLookMat::Wood, FVector(X - 4.f, Y - 4.f, 0.f), FVector(X + 4.f, Y + 4.f, 74.f));
		B.Box(EWRLookMat::Wood, FVector(-62.f, -38.f, 62.f), FVector(62.f, -34.f, 74.f));
		B.Box(EWRLookMat::Wood, FVector(-62.f, 34.f, 62.f), FVector(62.f, 38.f, 74.f));
	}
	else if (Def.Id == TEXT("chair"))
	{
		B.Box(EWRLookMat::Wood, FVector(-23.f, -23.f, 44.f), FVector(23.f, -0.5f, 48.f));
		B.Box(EWRLookMat::Wood, FVector(-23.f, 0.5f, 44.f), FVector(23.f, 23.f, 48.f));
		for (const float X : { -20.f, 20.f }) for (const float Y : { -20.f, 20.f }) B.Box(EWRLookMat::Wood, FVector(X - 2.5f, Y - 2.5f, 0.f), FVector(X + 2.5f, Y + 2.5f, 44.f));
		for (const float Y : { -20.f, 20.f }) B.Box(EWRLookMat::Wood, FVector(-22.5f, Y - 2.5f, 48.f), FVector(-17.5f, Y + 2.5f, 92.f));
		for (const float Z : { 62.f, 78.f }) B.Box(EWRLookMat::Wood, FVector(-22.f, -20.f, Z), FVector(-18.f, 20.f, Z + 8.f));
	}
	else if (Def.Id == TEXT("torch"))
	{
		B.Box(EWRLookMat::Wood, FVector(-3.f, -3.f, 0.f), FVector(3.f, 3.f, 142.f));
		B.Box(EWRLookMat::Cloth, FVector(-6.f, -6.f, 132.f), FVector(6.f, 6.f, 152.f));   // pitch-soaked wrapping
	}
	else if (Def.Id == TEXT("brazier"))
	{
		for (const float X : { -20.f, 20.f }) for (const float Y : { -20.f, 20.f }) B.Box(EWRLookMat::Stone, FVector(X - 3.f, Y - 3.f, 0.f), FVector(X + 3.f, Y + 3.f, 52.f), Iron);
		B.Add(EWRLookMat::Stone, FVector(62.f, 62.f, 22.f), FVector(0.f, 0.f, 61.f), FRotator::ZeroRotator, 0.35f, EWRLookMesh::Cylinder);
		B.Add(EWRLookMat::Cloth, FVector(52.f, 52.f, 4.f), FVector(0.f, 0.f, 72.f), FRotator::ZeroRotator, 1.f, EWRLookMesh::Cylinder);  // coals
	}
	else if (Def.Id == TEXT("trophy_mount"))
	{
		for (int32 i = 0; i < 3; ++i) B.Box(EWRLookMat::Wood, FVector(-36.f + i * 24.f + 0.5f, -10.f, 114.f), FVector(-36.f + (i + 1) * 24.f - 0.5f, -4.f, 186.f));
		for (const float S2 : { -1.f, 1.f }) B.Add(EWRLookMat::Stone, FVector(8.f, 8.f, 36.f), FVector(S2 * 18.f, 0.f, 172.f), FRotator(0.f, 0.f, S2 * -28.f), 1.f, EWRLookMesh::Cone);
	}
	else if (Def.Id == TEXT("banner"))
	{
		// A clan banner: an iron-capped pole, a crossbar it hangs from on rings, three panels of wool (hung a little
		// apart, so they fold), a woven emblem, and tassels along the bottom edge.
		B.Post(EWRLookMat::Wood, 0.f, 0.f, 0.f, 252.f, 8.f);
		B.Add(EWRLookMat::Stone, FVector(10.f, 10.f, 14.f), FVector(0.f, 0.f, 259.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cone);   // finial
		B.Box(EWRLookMat::Stone, FVector(-5.f, -5.f, 240.f), FVector(5.f, 5.f, 244.f), Iron);                                          // collar
		B.Box(EWRLookMat::Wood, FVector(3.f, -40.f, 233.f), FVector(8.f, 40.f, 238.f));                                                 // crossbar
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y0 = -36.f + i * 24.f;
			const float X = 5.f + (i == 1 ? 1.5f : 0.f);
			B.Box(EWRLookMat::Cloth, FVector(X, Y0 + 0.4f, 88.f + (i == 1 ? 0.f : 3.f)), FVector(X + 2.f, Y0 + 23.6f, 232.f));
			B.Add(EWRLookMat::Stone, FVector(3.f, 3.f, 3.f), FVector(5.5f, Y0 + 12.f, 233.f), FRotator::ZeroRotator, Iron, EWRLookMesh::Cylinder);  // ring
		}
		// The emblem: a diamond of trim over the middle panel, with a bar under it.
		B.Add(EWRLookMat::Trim, FVector(1.f, 26.f, 26.f), FVector(8.6f, 0.f, 178.f), FRotator(0.f, 0.f, 45.f));
		B.Box(EWRLookMat::Trim, FVector(8.f, -30.f, 140.f), FVector(9.f, 30.f, 145.f));
		for (int32 t = 0; t < 9; ++t)
		{
			const float Y = -32.f + t * 8.f;
			B.Box(EWRLookMat::Trim, FVector(5.5f, Y - 1.2f, 78.f), FVector(7.f, Y + 1.2f, 90.f));
		}
	}
	else if (Def.Id == TEXT("rug"))
	{
		// A woven rug: a dyed field, a trim border, an inner band, fringe at both ends.
		B.Box(EWRLookMat::Cloth, FVector(-100.f, -75.f, 0.f), FVector(100.f, 75.f, 1.6f));
		B.Box(EWRLookMat::Trim, FVector(-100.f, -75.f, 1.6f), FVector(100.f, -64.f, 2.f));
		B.Box(EWRLookMat::Trim, FVector(-100.f, 64.f, 1.6f), FVector(100.f, 75.f, 2.f));
		B.Box(EWRLookMat::Trim, FVector(-100.f, -64.f, 1.6f), FVector(-89.f, 64.f, 2.f));
		B.Box(EWRLookMat::Trim, FVector(89.f, -64.f, 1.6f), FVector(100.f, 64.f, 2.f));
		B.Box(EWRLookMat::Trim, FVector(-70.f, -44.f, 1.6f), FVector(70.f, -40.f, 2.f));
		B.Box(EWRLookMat::Trim, FVector(-70.f, 40.f, 1.6f), FVector(70.f, 44.f, 2.f));
		B.Add(EWRLookMat::Trim, FVector(30.f, 30.f, 0.4f), FVector(0.f, 0.f, 1.8f), FRotator(0.f, 45.f, 0.f));
		for (const float End : { -1.f, 1.f })
		{
			for (int32 t = 0; t < 15; ++t)
			{
				const float Y = -70.f + t * 10.f;
				B.Box(EWRLookMat::Trim, FVector(End > 0.f ? 100.f : -106.f, Y - 1.5f, 0.f), FVector(End > 0.f ? 106.f : -100.f, Y + 1.5f, 0.8f));
			}
		}
	}
	else if (K == TEXT("crop"))
	{
		B.Box(EWRLookMat::Wood, FVector(-200.f, -200.f, 0.f), FVector(200.f, -190.f, 32.f));
		B.Box(EWRLookMat::Wood, FVector(-200.f, 190.f, 0.f), FVector(200.f, 200.f, 32.f));
		B.Box(EWRLookMat::Wood, FVector(-200.f, -190.f, 0.f), FVector(-190.f, 190.f, 32.f));
		B.Box(EWRLookMat::Wood, FVector(190.f, -190.f, 0.f), FVector(200.f, 190.f, 32.f));
		B.Box(EWRLookMat::Cloth, FVector(-190.f, -190.f, 0.f), FVector(190.f, 190.f, 22.f));
		for (int32 r = 0; r < 6; ++r) B.Box(EWRLookMat::Cloth, FVector(-180.f, -165.f + r * 60.f, 22.f), FVector(180.f, -135.f + r * 60.f, 29.f));  // furrows
	}
	else if (K == TEXT("pen"))
	{
		for (const float P : { -400.f, -200.f, 0.f, 200.f, 400.f })
		{
			B.Post(EWRLookMat::Wood, P, -400.f, 0.f, 130.f, 14.f);
			B.Post(EWRLookMat::Wood, P, 400.f, 0.f, 130.f, 14.f);
			if (FMath::Abs(P) < 300.f)
			{
				B.Post(EWRLookMat::Wood, -400.f, P, 0.f, 130.f, 14.f);
				B.Post(EWRLookMat::Wood, 400.f, P, 0.f, 130.f, 14.f);
			}
		}
		for (const float Z : { 52.f, 102.f })
		{
			B.Box(EWRLookMat::Wood, FVector(-400.f, -405.f, Z), FVector(400.f, -398.f, Z + 12.f));
			B.Box(EWRLookMat::Wood, FVector(-400.f, 398.f, Z), FVector(400.f, 405.f, Z + 12.f));
			B.Box(EWRLookMat::Wood, FVector(-405.f, -400.f, Z), FVector(-398.f, 400.f, Z + 12.f));
			B.Box(EWRLookMat::Wood, FVector(398.f, -400.f, Z), FVector(405.f, 400.f, Z + 12.f));
		}
		// The feed trough along the -X fence: a hollowed log on two trestles (WRFarmLook heaps the fodder in it).
		for (const float X : { -320.f, -200.f })
		{
			B.Add(EWRLookMat::Wood, FVector(5.f, 5.f, 48.f), FVector(X, -165.f, 22.f), FRotator(0.f, 0.f, 18.f), 1.f, EWRLookMesh::Cylinder);
			B.Add(EWRLookMat::Wood, FVector(5.f, 5.f, 48.f), FVector(X, -135.f, 22.f), FRotator(0.f, 0.f, -18.f), 1.f, EWRLookMesh::Cylinder);
		}
		B.Add(EWRLookMat::Wood, FVector(40.f, 40.f, 170.f), FVector(-260.f, -150.f, 52.f), FRotator(90.f, 0.f, 0.f), 0.8f, EWRLookMesh::Cylinder);
		B.Add(EWRLookMat::Stone, FVector(30.f, 30.f, 160.f), FVector(-260.f, -150.f, 58.f), FRotator(90.f, 0.f, 0.f), 0.1f, EWRLookMesh::Cylinder);   // its hollow
		// The shelf by the +X fence's middle, where the products wait: a plank on two posts (the nest box sits on it).
		for (const float Y : { 226.f, 294.f }) B.Post(EWRLookMat::Wood, 330.f, Y, 0.f, 80.f, 8.f);
		B.Box(EWRLookMat::Wood, FVector(270.f, 222.f, 74.f), FVector(390.f, 298.f, 80.f));
	}
	return Out;
}

TArray<FWRLookBlock> FWRBuildLook::LeafBlocks(const FWRPieceDef& Def, const FVector& LeafSize, const FTransform& PieceToWorld)
{
	using namespace WRBuildLookDetail;
	TArray<FWRLookBlock> Out;
	const FTransform LeafToWorld = FTransform(FVector(0.f, 0.f, LeafSize.Z * 0.5f)) * PieceToWorld;
	FBuilder B(LeafToWorld, Out);
	const float W = LeafSize.X, H = LeafSize.Z;
	const bool bReinforced = Def.Material == TEXT("stone");
	// Vertical boards on one face, two battens and a brace on the other (hinge side -X), iron hinge straps and a ring.
	const int32 N = FMath::Max(3, FMath::RoundToInt(W / 23.f));
	const float Pitch = W / N;
	for (int32 i = 0; i < N; ++i)
	{
		B.Add(EWRLookMat::Wood, FVector(Pitch - 1.5f, 5.f, H - 2.f), FVector(-W * 0.5f + (i + 0.5f) * Pitch, 1.5f, 0.f));
	}
	for (const float Z : { -H * 0.33f, H * 0.33f })
	{
		B.Box(EWRLookMat::Wood, FVector(-W * 0.5f + 5.f, -4.5f, Z - 7.f), FVector(W * 0.5f - 5.f, -1.f, Z + 7.f));
	}
	const float Brace = FMath::RadiansToDegrees(FMath::Atan2(H * 0.66f - 14.f, W - 14.f));
	B.Add(EWRLookMat::Wood, FVector(FMath::Sqrt(FMath::Square(W - 14.f) + FMath::Square(H * 0.66f - 14.f)), 3.5f, 12.f), FVector(0.f, -2.75f, 0.f), FRotator(Brace, 0.f, 0.f));
	for (const float Z : bReinforced ? TArray<float>{ -H * 0.4f, -H * 0.13f, H * 0.13f, H * 0.4f } : TArray<float>{ -H * 0.33f, H * 0.33f })
	{
		B.Box(EWRLookMat::Stone, FVector(-W * 0.5f - 1.f, 4.f, Z - 4.f), FVector(bReinforced ? W * 0.5f : W * 0.15f, 5.5f, Z + 4.f), Iron);
	}
	B.Add(EWRLookMat::Stone, FVector(3.f, 3.f, 14.f), FVector(W * 0.5f - 16.f, 6.5f, 0.f), FRotator::ZeroRotator, Iron);
	return Out;
}

FLinearColor FWRBuildLook::ClothColor(const FWRPieceDef& Def)
{
	if (Def.Plot == TEXT("ashpit")) return FLinearColor(0.022f, 0.021f, 0.02f);          // black picon ash
	if (Def.Plot == TEXT("paddy")) return FLinearColor(0.06f, 0.045f, 0.03f);            // wet mud
	if (Def.Id == TEXT("cooking_pot")) return FLinearColor(0.45f, 0.17f, 0.07f);         // beaten copper
	if (Def.Id == TEXT("drying_rack")) return FLinearColor(0.25f, 0.06f, 0.04f);         // strips of meat and fish
	if (Def.Id == TEXT("compost_bin")) return FLinearColor(0.04f, 0.025f, 0.015f);       // compost
	if (Def.Id == TEXT("salt_pan")) return FLinearColor(0.18f, 0.22f, 0.24f);            // brine over clay
	if (Def.Kind == TEXT("crop")) return FLinearColor(0.07f, 0.045f, 0.025f);           // tilled soil
	if (Def.Id == TEXT("torch")) return FLinearColor(0.05f, 0.035f, 0.025f);            // pitch-soaked wrapping
	if (Def.Id == TEXT("brazier")) return FLinearColor(0.12f, 0.03f, 0.01f);            // banked coals
	if (Def.Id == TEXT("bedroll")) return FLinearColor(0.2f, 0.13f, 0.07f);             // hide
	return FLinearColor(0.32f, 0.12f, 0.08f);                                            // a wool cover
}

int32 FWRBuildLook::Spawn(AActor* Owner, USceneComponent* Parent, const TArray<FWRLookBlock>& Blocks, const FLinearColor& Cloth, bool bAbsoluteScale)
{
	using namespace WRBuildLookDetail;
	// One instanced component per (material, shape): a whole wall is one or two draw calls.
	TMap<int32, TArray<const FWRLookBlock*>> Groups;
	for (const FWRLookBlock& Bl : Blocks)
	{
		Groups.FindOrAdd(static_cast<int32>(Bl.Mat) * 4 + static_cast<int32>(Bl.Mesh)).Add(&Bl);
	}
	int32 Made = 0;
	for (const TPair<int32, TArray<const FWRLookBlock*>>& G : Groups)
	{
		const EWRLookMat Mat = static_cast<EWRLookMat>(G.Key / 4);
		UInstancedStaticMeshComponent* Part = NewObject<UInstancedStaticMeshComponent>(Owner);
		Part->SetStaticMesh(MeshFor(static_cast<EWRLookMesh>(G.Key % 4)));
		Part->SetupAttachment(Parent);
		Part->SetUsingAbsoluteScale(bAbsoluteScale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetNumCustomDataFloats(3);
		if (Mat == EWRLookMat::Cloth || Mat == EWRLookMat::Trim)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MaterialFor(Mat), Owner);
			MID->SetVectorParameterValue(TEXT("Color"), Mat == EWRLookMat::Trim ? FWRBuildLook::TrimColor() : Cloth);
			Part->SetMaterial(0, MID);
		}
		else
		{
			Part->SetMaterial(0, MaterialFor(Mat));
		}
		Part->RegisterComponent();
		TArray<FTransform> Transforms;
		Transforms.Reserve(G.Value.Num());
		for (const FWRLookBlock* Bl : G.Value)
		{
			Transforms.Emplace(Bl->Rot, Bl->Centre, Bl->Size / 100.f);
		}
		Part->AddInstances(Transforms, false, false, false);
		for (int32 i = 0; i < G.Value.Num(); ++i)
		{
			const float Data[3] = { G.Value[i]->Tone, G.Value[i]->Axis, G.Value[i]->Dim };
			Part->SetCustomData(i, Data, i == G.Value.Num() - 1);
		}
		++Made;
	}
	return Made;
}

const FWRPieceDef* FWRBuildLook::StationDef(FName StationType)
{
	for (const FWRPieceDef& D : FWRBuildRegistry::Get().All())
	{
		if (D.Kind == TEXT("station") && D.Station == StationType) return &D;
	}
	return nullptr;
}

const FWRPieceDef* FWRBuildLook::KindDef(FName Kind)
{
	for (const FWRPieceDef& D : FWRBuildRegistry::Get().All())
	{
		if (D.Kind == Kind) return &D;
	}
	return nullptr;
}

namespace WRBuildLookDetail
{
	/** The prop mesh of a def (loaded once, rooted), on any machine. */
	static UStaticMesh* PropMesh(const FWRPieceDef& Def)
	{
		static TMap<FString, UStaticMesh*> Cache;
		if (Def.Mesh.IsEmpty()) return nullptr;
		if (UStaticMesh* Found = Cache.FindRef(Def.Mesh)) return Found;
		UStaticMesh* Mesh = Rooted<UStaticMesh>(*Def.Mesh);
		if (Mesh) Cache.Add(Def.Mesh, Mesh);
		return Mesh;
	}

	/** Where the fitted prop sits in its parent's unscaled space (AttachProp's maths): its box, scale and offset. */
	static bool PropFit(UStaticMesh* Mesh, const FWRPieceDef& Def, float BottomZ, FBox& OutBox, float& OutScale, FVector& OutOffset)
	{
		if (!Mesh) return false;
		const FRotator Turn(0.f, Def.MeshYaw, 0.f);
		const FBox Local = Mesh->GetBoundingBox();
		FBox Turned(ForceInit);
		for (int32 i = 0; i < 8; ++i)
		{
			Turned += Turn.RotateVector(FVector(i & 1 ? Local.Max.X : Local.Min.X, i & 2 ? Local.Max.Y : Local.Min.Y, i & 4 ? Local.Max.Z : Local.Min.Z));
		}
		const FVector Ext = Turned.GetSize().ComponentMax(FVector(1.f));
		const FVector Fit = Def.MeshFit.IsNearlyZero() ? Ext : Def.MeshFit;
		OutScale = FMath::Min3(Fit.X / Ext.X, Fit.Y / Ext.Y, Fit.Z / Ext.Z);
		const FVector Centre = Turned.GetCenter() * OutScale;
		OutOffset = FVector(-Centre.X, -Centre.Y, BottomZ - Turned.Min.Z * OutScale);
		OutBox = FBox(Turned.Min * OutScale + OutOffset, Turned.Max * OutScale + OutOffset);
		return true;
	}
}

UBoxComponent* FWRBuildLook::FitCollision(AActor* Owner, UPrimitiveComponent* Plain, const FWRPieceDef& Def, float BottomZ)
{
	using namespace WRBuildLookDetail;
	FBox Box;
	float Scale = 1.f;
	FVector Offset;
	if (!Owner || !Plain || !PropFit(PropMesh(Def), Def, BottomZ, Box, Scale, Offset))
	{
		return nullptr;
	}
	UBoxComponent* Fit = nullptr;
	TArray<UBoxComponent*> Boxes;
	Owner->GetComponents(Boxes);
	for (UBoxComponent* B : Boxes) if (B->ComponentHasTag(TEXT("WRFit"))) Fit = B;
	if (!Fit)
	{
		Fit = NewObject<UBoxComponent>(Owner, TEXT("FitCollision"));
		Fit->ComponentTags.Add(TEXT("WRFit"));
		Fit->SetupAttachment(Plain);
		Fit->SetUsingAbsoluteScale(true);
		Fit->RegisterComponent();
	}
	// The parent may be a scaled engine shape: undo its scale on the offset (as AttachProp does).
	const FVector ParentScale = Plain->GetComponentScale().ComponentMax(FVector(0.001f));
	Fit->SetBoxExtent(Box.GetExtent(), false);
	Fit->SetRelativeLocation(Box.GetCenter() / ParentScale);
	Fit->SetCollisionProfileName(TEXT("BlockAll"));
	Fit->SetCanEverAffectNavigation(true);
	Fit->SetHiddenInGame(true);
	Plain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return Fit;
}

UStaticMeshComponent* FWRBuildLook::AttachProp(AActor* Owner, USceneComponent* Parent, const FWRPieceDef& Def, float BottomZ)
{
	if (Def.Mesh.IsEmpty() || !Owner || !Parent || Owner->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	// Rooted like every runtime-loaded asset a replicated actor shows (a base you walk away from and back to).
	static TMap<FString, UStaticMesh*> Cache;
	UStaticMesh* Mesh = Cache.FindRef(Def.Mesh);
	if (!Mesh)
	{
		Mesh = WRBuildLookDetail::Rooted<UStaticMesh>(*Def.Mesh);
		if (!Mesh)
		{
			UE_LOG(LogWildreach, Warning, TEXT("BUILD look: %s mesh %s not found, keeping the plain shape"), *Def.Id.ToString(), *Def.Mesh);
			return nullptr;
		}
		Cache.Add(Def.Mesh, Mesh);
	}
	// Uniform scale so the turned mesh fits the box, bottom on BottomZ, centred over the piece.
	const FRotator Turn(0.f, Def.MeshYaw, 0.f);
	const FBox Local = Mesh->GetBoundingBox();
	FBox Turned(ForceInit);
	for (int32 i = 0; i < 8; ++i)
	{
		Turned += Turn.RotateVector(FVector(i & 1 ? Local.Max.X : Local.Min.X, i & 2 ? Local.Max.Y : Local.Min.Y, i & 4 ? Local.Max.Z : Local.Min.Z));
	}
	const FVector Ext = Turned.GetSize().ComponentMax(FVector(1.f));
	const FVector Fit = Def.MeshFit.IsNearlyZero() ? Ext : Def.MeshFit;
	const float Scale = FMath::Min3(Fit.X / Ext.X, Fit.Y / Ext.Y, Fit.Z / Ext.Z);
	const FVector Centre = Turned.GetCenter() * Scale;
	const FVector Offset(-Centre.X, -Centre.Y, BottomZ - Turned.Min.Z * Scale);

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(Owner);
	Prop->SetStaticMesh(Mesh);
	Prop->SetupAttachment(Parent);
	Prop->SetUsingAbsoluteScale(true);
	Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prop->SetGenerateOverlapEvents(false);
	Prop->SetCanEverAffectNavigation(false);
	// The parent may be a scaled engine shape: undo its scale on the offset (rotation and location still follow it).
	const FVector ParentScale = Parent->GetComponentScale().ComponentMax(FVector(0.001f));
	Prop->SetRelativeLocationAndRotation(Offset / ParentScale, Turn);
	Prop->SetRelativeScale3D(FVector(Scale));  // absolute: this is its world scale
	Prop->RegisterComponent();
	return Prop;
}
