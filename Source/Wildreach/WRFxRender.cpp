// Draws WRFx sprites: one primitive per batch whose scene proxy builds camera-facing quads for each view on the render
// thread (FDynamicMeshBuilder), so nothing per sprite lives on the GPU between frames and the game thread only hands
// over a flat array. Atlas cell -> UV0, colour -> vertex colour, HDR intensity -> UV1.x, depth-fade softness -> UV1.y
// (see Tools/fx/make_fx_assets.py).

#include "WRFxSystem.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialDomain.h"
#include "MeshElementCollector.h"
#include "Misc/ScopeExit.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "SceneView.h"

namespace WRFxRenderDetail
{
	constexpr int32 AtlasGrid = 4;
}

class FWRFxSpriteProxy final : public FPrimitiveSceneProxy
{
public:
	FWRFxSpriteProxy(const UWRFxSpriteComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
		, bSort(Component->bSortByDistance)
	{
		UMaterialInterface* Mat = Component->GetMaterial(0);
		if (!Mat) Mat = UMaterial::GetDefaultMaterial(MD_Surface);
		MaterialProxy = Mat->GetRenderProxy();
		bWillEverBeLit = !Component->bSortByDistance ? false : true;
		bCastDynamicShadow = false;
		bVerifyUsedMaterials = false;
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	void SetSprites_RenderThread(TArray<FWRFxSprite>&& InSprites)
	{
		Sprites = MoveTemp(InSprites);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WRFxSprites_GetDynamicMeshElements);
		if (Sprites.Num() == 0 || !MaterialProxy)
		{
			return;
		}
		const double Started = FPlatformTime::Seconds();
		ON_SCOPE_EXIT
		{
			UWRFxSubsystem::RenderMicros += static_cast<int64>((FPlatformTime::Seconds() - Started) * 1.0e6);
			if (bSort) ++UWRFxSubsystem::RenderFrames;  // one count per frame (the lit batch)
		};
		const float CellUV = 1.f / WRFxRenderDetail::AtlasGrid;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}
			const FSceneView* View = Views[ViewIndex];
			const FVector Origin = View->ViewMatrices.GetViewOrigin();
			const FVector3f Right = FVector3f(View->GetViewRight());
			const FVector3f Up = FVector3f(View->GetViewUp());
			const FVector3f Forward = FVector3f(View->GetViewDirection());

			// Back to front for blended smoke; glow adds up in any order.
			TArray<int32, TInlineAllocator<1>> Order;
			const int32 N = Sprites.Num();
			if (bSort)
			{
				TArray<TPair<float, int32>> Keys;
				Keys.Reserve(N);
				for (int32 i = 0; i < N; ++i) Keys.Emplace(static_cast<float>(FVector::Dist(Sprites[i].Pos, Origin)) + Sprites[i].SortBias, i);
				Keys.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key > B.Key; });
				Order.SetNumUninitialized(N);
				for (int32 i = 0; i < N; ++i) Order[i] = Keys[i].Value;
			}

			FDynamicMeshBuilder Builder(View->GetFeatureLevel(), 2);
			Builder.ReserveVertices(N * 4);
			Builder.ReserveTriangles(N * 2);
			for (int32 n = 0; n < N; ++n)
			{
				const FWRFxSprite& S = Sprites[bSort ? Order[n] : n];
				// Relative to the view origin: float precision stays fine anywhere on the island.
				const FVector3f C = FVector3f(S.Pos - Origin);
				FVector3f AxisX, AxisY;
				float HalfX = S.Size * 0.5f, HalfY = S.Size * 0.5f;
				FVector3f Centre = C;
				if (!S.Normal.IsZero())
				{
					// Flat on a surface (rings on the ground), rolled by Rot.
					const FVector3f Nrm = S.Normal.GetSafeNormal();
					FVector3f T = FMath::Abs(Nrm.Z) < 0.95f ? FVector3f::CrossProduct(Nrm, FVector3f::UpVector) : FVector3f::CrossProduct(Nrm, FVector3f::ForwardVector);
					T.Normalize();
					const FVector3f B = FVector3f::CrossProduct(Nrm, T);
					const float Cs = FMath::Cos(S.Rot), Sn = FMath::Sin(S.Rot);
					AxisX = T * Cs + B * Sn;
					AxisY = B * Cs - T * Sn;
				}
				else if (S.Stretch.SizeSquared() > 1.f)
				{
					// A streak: long axis along the velocity as seen from here, head at the sprite's position.
					FVector3f Along = S.Stretch - Forward * FVector3f::DotProduct(S.Stretch, Forward);
					const float Len = Along.Size();
					if (Len > 0.5f)
					{
						Along /= Len;
						AxisY = -Along;
						AxisX = FVector3f::CrossProduct(Along, Forward).GetSafeNormal();
						HalfY = S.Size * 0.5f + Len * 0.5f;
						Centre = C - Along * (Len * 0.5f);
					}
					else
					{
						AxisX = Right;
						AxisY = Up;
					}
				}
				else
				{
					const float Cs = FMath::Cos(S.Rot), Sn = FMath::Sin(S.Rot);
					AxisX = Right * Cs + Up * Sn;
					AxisY = Up * Cs - Right * Sn;
				}
				const FVector3f Normal = FVector3f::CrossProduct(AxisX, AxisY).GetSafeNormal();
				const float U0 = (S.Cell % WRFxRenderDetail::AtlasGrid) * CellUV, V0 = (S.Cell / WRFxRenderDetail::AtlasGrid) * CellUV;
				const FVector3f Corners[4] = {
					Centre - AxisX * HalfX + AxisY * HalfY,   // top left (UV v grows down the atlas: top of the sprite is "up")
					Centre + AxisX * HalfX + AxisY * HalfY,
					Centre + AxisX * HalfX - AxisY * HalfY,
					Centre - AxisX * HalfX - AxisY * HalfY };
				const FVector2f UVs[4] = { FVector2f(U0, V0), FVector2f(U0 + CellUV, V0), FVector2f(U0 + CellUV, V0 + CellUV), FVector2f(U0, V0 + CellUV) };
				const int32 Base = n * 4;
				for (int32 k = 0; k < 4; ++k)
				{
					FDynamicMeshVertex V(Corners[k], AxisX, Normal, UVs[k], S.Color);
					V.TextureCoordinate[1] = FVector2f(S.Intensity, S.Soft);
					Builder.AddVertex(V);
				}
				Builder.AddTriangle(Base, Base + 1, Base + 2);
				Builder.AddTriangle(Base, Base + 2, Base + 3);
			}
			Builder.GetMesh(FTranslationMatrix(Origin), MaterialProxy, SDPG_World, true, false, ViewIndex, Collector);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = false;
		return Result;
	}

	virtual bool CanBeOccluded() const override { return false; }
	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize() + Sprites.GetAllocatedSize(); }

private:
	FMaterialRelevance MaterialRelevance;
	const FMaterialRenderProxy* MaterialProxy = nullptr;
	TArray<FWRFxSprite> Sprites;
	bool bSort = false;
};

UWRFxSpriteComponent::UWRFxSpriteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bCastDynamicShadow = false;
	bReceivesDecals = false;
	bAffectDistanceFieldLighting = false;
	bAffectDynamicIndirectLighting = false;
	bSelectable = false;
	SetMobility(EComponentMobility::Movable);
}

UMaterialInterface* UWRFxSpriteComponent::GetMaterial(int32 ElementIndex) const
{
	return Material;
}

FPrimitiveSceneProxy* UWRFxSpriteComponent::CreateSceneProxy()
{
	return Material ? new FWRFxSpriteProxy(this) : nullptr;
}

FBoxSphereBounds UWRFxSpriteComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Sprites go anywhere the player is: bounds big enough for the whole island, so the batch is never culled.
	const double Half = 2.0e6;
	return FBoxSphereBounds(FVector::ZeroVector, FVector(Half), Half * 1.7321);
}

void UWRFxSpriteComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
	if (!SceneProxy)
	{
		return;
	}
	FWRFxSpriteProxy* Proxy = static_cast<FWRFxSpriteProxy*>(SceneProxy);
	TArray<FWRFxSprite> Data = Sprites;
	ENQUEUE_RENDER_COMMAND(WRFxSprites)([Proxy, Data = MoveTemp(Data)](FRHICommandListImmediate& RHICmdList) mutable
	{
		Proxy->SetSprites_RenderThread(MoveTemp(Data));
	});
}
