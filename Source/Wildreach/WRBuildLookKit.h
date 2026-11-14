#pragma once

// The block kit the tiers and their furniture are drawn with (WRBuildLookSets.cpp, WRBuildLookDecor.cpp): tinted blocks
// in a piece's space whose shade comes from where they land in the world (the corner two walls share is one corner).

#include "CoreMinimal.h"
#include "WRBuildLook.h"
#include "WRBuildSets.h"

namespace WRLookKit
{
	struct FSurf
	{
		FLinearColor Color = FLinearColor(0.3f, 0.3f, 0.3f);
		float Metal = 0.f;
		float Rough = 0.85f;
		float Glow = 0.f;
		float Pattern = WRLookPattern::Mottle;
		static FSurf Stone(const FLinearColor& C, float Rough = 0.88f) { FSurf S; S.Color = C; S.Rough = Rough; return S; }
		static FSurf Wood(const FLinearColor& C) { FSurf S; S.Color = C; S.Rough = 0.7f; S.Pattern = WRLookPattern::Grain; return S; }
		static FSurf Steel(const FLinearColor& C, float Rough = 0.42f) { FSurf S; S.Color = C; S.Metal = 0.85f; S.Rough = Rough; S.Pattern = WRLookPattern::Brushed; return S; }
		static FSurf Glass(const FLinearColor& C, float Glow = 0.f) { FSurf S; S.Color = C; S.Rough = 0.08f; S.Glow = Glow; S.Pattern = WRLookPattern::Smooth; return S; }
		static FSurf Glaze(const FLinearColor& C, float Rough = 0.25f) { FSurf S; S.Color = C; S.Rough = Rough; S.Pattern = WRLookPattern::Smooth; return S; }
		static FSurf Lit(const FLinearColor& C, float Glow) { FSurf S; S.Color = C; S.Rough = 0.5f; S.Glow = Glow; S.Pattern = WRLookPattern::Smooth; return S; }
		static FSurf Cloth(const FLinearColor& C) { FSurf S; S.Color = C; S.Rough = 0.95f; S.Pattern = WRLookPattern::Mottle; return S; }
		FSurf Dim(float K) const { FSurf S = *this; S.Color = Color * K; S.Color.A = 1.f; return S; }
	};

	struct FKit
	{
		FKit(const FTransform& InWorld, TArray<FWRLookBlock>& InOut) : World(InWorld), Out(InOut) {}
		const FTransform& World;
		TArray<FWRLookBlock>& Out;
		/** A sub-frame inside the piece (one side of a foundation): blocks go through it first. */
		FTransform Frame = FTransform::Identity;

		FVector ToWorld(const FVector& Local) const { return World.TransformPosition(Frame.TransformPosition(Local)); }
		float Rand(const FVector& Local, int32 Salt) const { return FWRBuildLook::Hash01(ToWorld(Local), Salt); }
		float Jit(const FVector& Local, int32 Salt) const { return Rand(Local, Salt) * 2.f - 1.f; }

		FWRLookBlock& Add(const FSurf& S, const FVector& Size, const FVector& Centre, const FRotator& Rot = FRotator::ZeroRotator, EWRLookMesh Mesh = EWRLookMesh::Cube, float Dim = 1.f)
		{
			FWRLookBlock B;
			B.Size = Size.ComponentMax(FVector(0.4f));
			B.Centre = Frame.TransformPosition(Centre);
			B.Rot = (Frame.GetRotation() * Rot.Quaternion()).Rotator();
			B.Mat = EWRLookMat::Tinted;
			B.Mesh = Mesh;
			B.Dim = Dim;
			B.Tone = Rand(Centre, 17);
			const FVector Long = Mesh != EWRLookMesh::Cube ? FVector::ZAxisVector
				: Size.X >= Size.Y && Size.X >= Size.Z ? FVector::XAxisVector : Size.Y >= Size.Z ? FVector::YAxisVector : FVector::ZAxisVector;
			const FVector Dir = World.TransformVectorNoScale(B.Rot.RotateVector(Long)).GetAbs();
			B.Axis = Dir.Z > 0.7f ? 2.f : Dir.X >= Dir.Y ? 0.f : 1.f;
			B.Color = S.Color;
			B.Metal = S.Metal;
			B.Rough = S.Rough;
			B.Glow = S.Glow;
			B.Pattern = S.Pattern;
			return Out.Add_GetRef(B);
		}
		FWRLookBlock& Box(const FSurf& S, const FVector& Min, const FVector& Max, float Dim = 1.f)
		{
			return Add(S, Max - Min, (Min + Max) * 0.5f, FRotator::ZeroRotator, EWRLookMesh::Cube, Dim);
		}
		/** A round (or square) bar from A to B. */
		void Rod(const FSurf& S, const FVector& A, const FVector& B, float Diam, bool bSquare = false, float Dim = 1.f)
		{
			const FVector D = B - A;
			const float Len = D.Size();
			if (Len < 0.5f) return;
			Add(S, FVector(Diam, Diam, Len), (A + B) * 0.5f, FRotationMatrix::MakeFromZ(D / Len).Rotator(), bSquare ? EWRLookMesh::Cube : EWRLookMesh::Cylinder, Dim);
		}
		/** A disc: a cylinder along Axis. */
		void Disc(const FSurf& S, const FVector& Centre, const FVector& Axis, float Diam, float Thick, float Dim = 1.f)
		{
			Add(S, FVector(Diam, Diam, Thick), Centre, FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator(), EWRLookMesh::Cylinder, Dim);
		}
		/** A cone from Base pointing along Dir, Length long. */
		void Spike(const FSurf& S, const FVector& Base, const FVector& Dir, float Diam, float Length)
		{
			const FVector D = Dir.GetSafeNormal();
			Add(S, FVector(Diam, Diam, Length), Base + D * (Length * 0.5f), FRotationMatrix::MakeFromZ(D).Rotator(), EWRLookMesh::Cone);
		}
		/** A regular hexagonal prism (three boxes, each R x R*sqrt3, turned 0 / 60 / 120 about Z): corners along X. */
		void Hex(const FSurf& S, float X, float Y, float Z0, float Z1, float R, float Turn = 0.f)
		{
			const float H = Z1 - Z0;
			if (H < 0.5f) return;
			for (int32 k = 0; k < 3; ++k)
			{
				Add(S, FVector(R, R * 1.7320508f, H), FVector(X, Y, (Z0 + Z1) * 0.5f), FRotator(0.f, Turn + 60.f * k, 0.f));
			}
		}
		/** Ring of N blocks round Centre in the XZ plane (an arch's voussoirs, a porthole's ring): from A0 to A1 degrees. */
		void Ring(const FSurf& S, const FVector& Centre, float R, float Depth, float Thick, int32 N, float A0 = 0.f, float A1 = 360.f, float Gap = 1.2f)
		{
			const float Step = (A1 - A0) / N;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = FMath::DegreesToRadians(A0 + Step * (i + 0.5f));
				const FVector Dir(FMath::Cos(A), 0.f, FMath::Sin(A));
				const float Chord = 2.f * (R + Depth * 0.5f) * FMath::Sin(FMath::DegreesToRadians(Step * 0.5f)) - Gap;
				Add(S, FVector(Chord, Thick, Depth), Centre + Dir * (R + Depth * 0.5f), FRotator(FMath::RadiansToDegrees(A) + 90.f, 0.f, 0.f));
			}
		}
	};

	/** A tier's surfaces from its look. */
	struct FPalette
	{
		FSurf A, B, C, Glass, Glow, Trim, Dark;
		explicit FPalette(const FWRTierLook& L)
		{
			A.Color = L.A; A.Metal = L.Metal; A.Rough = L.Rough; A.Pattern = L.Metal > 0.5f ? WRLookPattern::Brushed : WRLookPattern::Mottle;
			B = FSurf::Stone(L.B, FMath::Min(0.95f, L.Rough + 0.05f));
			C = FSurf::Stone(L.C, 0.8f);
			Glass = FSurf::Glass(L.Glass, L.GlassGlow);
			Glow = FSurf::Lit(L.Glow, 0.9f);
			Trim = FSurf::Stone(L.Trim, 0.6f);
			Dark = FSurf::Stone(L.B * 0.35f, 0.95f);
		}
	};
}
