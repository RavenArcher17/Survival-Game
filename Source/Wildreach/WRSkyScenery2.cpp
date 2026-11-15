// The Sky's scenery builders: island decoration, bridges and chain walks, wind-poles, waterfalls, the Flue's shaft, the
// Undercloud's bank, the Paper Titan, the Great Kite, the Ring's cloud cap and spore column, the Chain Cliffs' head
// (cooks/biome-sky.md 2.1-2.4, 2.7; cooks/build-biome-sky.md 2.2-2.5). See WRSkySceneryParts.h.

#include "WRSkySceneryParts.h"
#include "WRSkyWorld.h"
#include "WRForestWorld.h"
#include "WRBuildLook.h"
#include "WRLand.h"
#include "WRFungalTypes.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/Package.h"

using namespace WRSkyLook;

namespace WRSkyParts
{
	USceneComponent* Pivot(AWRSkyScenery* A, const FVector& Local, const FRotator& Rot)
	{
		USceneComponent* P = NewObject<USceneComponent>(A);
		P->SetupAttachment(A->GetRootComponent());
		P->SetRelativeLocation(Local);
		P->SetRelativeRotation(Rot);
		P->RegisterComponent();
		return P;
	}
	void SpawnUnder(AWRSkyScenery* A, USceneComponent* Under, const TFunctionRef<void(WRLookKit::FKit&)>& Fill)
	{
		if (A->GetNetMode() == NM_DedicatedServer || !Under) return;
		TArray<FWRLookBlock> Blocks;
		const FTransform Xf = Under->GetComponentTransform();
		WRLookKit::FKit K(Xf, Blocks);
		Fill(K);
		FWRBuildLook::Spawn(A, Under, Blocks, FLinearColor(0.25f, 0.2f, 0.15f), false);
	}
	void AddStreamer(AWRSkyScenery* A, const FVector& Top, int32 Var)
	{
		USceneComponent* P = Pivot(A, Top);
		SpawnUnder(A, P, [Var](WRLookKit::FKit& K)
		{
			for (int32 s = 0; s < 5; ++s)
			{
				K.Add(SkCloth(s + Var, 0.9f), FVector(50.f, 2.f, 46.f - s * 5.f), FVector(30.f + s * 52.f, 0, -30.f), FRotator(0.f, 0.f, (s % 2) ? 4.f : -4.f));
			}
			K.Rod(SkBronze(), FVector(0, 0, -5.f), FVector(10.f, 0, -5.f), 6.f);
		});
		A->Cloths.Add(P);
		A->ClothPhase.Add(float(Var) * 1.7f);
		A->SetActorTickEnabled(true);
	}
}

using namespace WRSkyParts;

// ================================================================================================ the context

WRSkyParts::FCtx::FCtx(AWRSkyScenery* InA, float Cull)
	: A(InA), B(InA, Cull), D(FWRSkyIsleData::Get())
{
	I = D.FindIsland(InA->Island);
	if (!I && InA->Kind == TEXT("island") && D.Islands.IsValidIndex(InA->Index)) I = &D.Islands[InA->Index];
}

float WRSkyParts::FCtx::G(float X, float Y) const
{
	if (I)
	{
		// Built at home (a drifting island's pieces ride its actor): the island's shape is read where it is now and
		// brought back by its drift, so a piece built mid-drift (a late joiner) still sits on the turf.
		const FVector W = A->GetActorTransform().TransformPosition(FVector(X, Y, 0.f));
		const FVector Drift = WRSkyIsles::DriftOf(I->Index);
		return I->TopZ(W + Drift) - Drift.Z - A->GetActorLocation().Z;
	}
	return B.G(X, Y);
}

FVector WRSkyParts::FCtx::L(const FVector& World) const
{
	return A->GetActorTransform().InverseTransformPosition(World);
}

bool WRSkyParts::FCtx::OnTop(float X, float Y, float Margin) const
{
	if (!I) return true;
	const FVector W = A->GetActorTransform().TransformPosition(FVector(X, Y, 0.f));
	const FVector2D L2 = I->Local(W);
	return L2.Size() + Margin <= I->EdgeRadius(FMath::Atan2(L2.Y, L2.X));
}

// ================================================================================================ decoration

void WRSkyParts::Decorate(FCtx& C)
{
	// Every island (SKY 2.4 decoration rule, build 2.4): cloudpine cedars and junipers leaning 12-30 degrees downwind
	// (west), wind-combed grass, boulders at the rim, wind-poles every 150 m round the lip, levistone glints in the rim.
	// Nothing within 15 m of a site, a kit place or a bridge's end; nothing in the lagoon or the pool.
	if (!C.I) return;
	const FWRSkyIsleData::FIsland& I = *C.I;
	AWRSkyScenery* A = C.A;
	const bool bServer = A->GetNetMode() == NM_DedicatedServer;
	TArray<FVector2D> Keep;
	for (const FWRSkyIsleData::FSite& S : C.D.Sites) if (S.Island == I.Id) Keep.Add(FVector2D(C.L(S.At)));
	for (const FWRSkyIsleData::FSite& S : C.D.Kit) if (S.Island == I.Id) Keep.Add(FVector2D(C.L(S.At)));
	for (const FWRSkyIsleData::FBridge& Br : C.D.Bridges)
	{
		if (Br.From == I.Id) Keep.Add(FVector2D(C.L(Br.A)));
		if (Br.To == I.Id) Keep.Add(FVector2D(C.L(Br.B)));
	}
	for (const FWRSkyIsleData::FLift& Lf : C.D.Lifts) if (Lf.Island == I.Id) Keep.Add(FVector2D(C.L(FVector(Lf.Foot.X, Lf.Foot.Y, 0.f))));
	const float Clear = WRTune(TEXT("skyWorld.decor.clear"), 1500.f);
	auto Free = [&](const FVector2D& P, float R) { for (const FVector2D& K : Keep) if (FVector2D::Distance(K, P) < R) return false; return true; };
	const float Area = PI * I.R() * I.RMin() / 10000.f;   // m2
	const float WindLocal = 180.f - I.Yaw;                 // the kit's +X lean, turned downwind (the wind blows west)

	// ---- trees (instanced per species and variant; a trunk box each) ----
	struct FTreeOut { FName Sp; int32 Var; FTransform Xf; };
	TArray<FTreeOut> Trees;
	const bool bTemple = I.Kind == TEXT("temple");
	const int32 NTrees = bTemple ? 3 : FMath::Clamp(int32(Area / WRTune(TEXT("skyWorld.decor.treeArea"), 1600.f)), 1, 60);
	for (int32 t = 0; t < NTrees * 3 && Trees.Num() < NTrees; ++t)
	{
		const float Rho = 0.25f + 0.62f * FMath::Sqrt(C.Rand(t * 13 + 1));
		const float Ang = t * 2.39996f + C.Rand(t * 13 + 2);
		const FVector2D P = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * I.EdgeRadius(Ang) * Rho;
		if (!Free(P, Clear)) continue;
		if (I.bLagoon && Rho < 0.82f) continue;
		if (I.bPool && Rho < 0.32f) continue;
		const bool bRocky = I.Kind == TEXT("rock") || I.Zone == '4' || bTemple;
		const FName Sp = (bRocky ? C.Rand(t * 13 + 3) < 0.65f : C.Rand(t * 13 + 3) < 0.22f) ? FName(TEXT("juniper")) : FName(TEXT("cloud_cedar"));
		const float H = Sp == TEXT("juniper") ? 250.f + 200.f * C.Rand(t * 13 + 4) : (I.Zone == '5' ? 700.f : 900.f) + 900.f * C.Rand(t * 13 + 4) * (1.f - 0.4f * Rho);
		const int32 Var = int32(C.Rand(t * 13 + 5) * 4.f) % 4;
		const FVector Base(P.X, P.Y, C.G(P.X, P.Y) - 15.f);
		const float Sc = H / FMath::Max(1.f, WRTreeKit::ReferenceHeight(Sp));
		Trees.Add({ Sp, Var, FTransform(FRotator(0.f, WindLocal + (C.Rand(t * 13 + 6) - 0.5f) * 30.f, 0.f), Base, FVector(Sc)) });
		Keep.Add(P);
		if (Sp == TEXT("cloud_cedar")) C.B.Box(Base + FVector(0, 0, H * 0.2f), FVector(WRTreeKit::ReferenceRadius(Sp) * Sc * 0.8f, WRTreeKit::ReferenceRadius(Sp) * Sc * 0.8f, H * 0.2f));
	}
	if (!bServer)
	{
		TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Comps;
		for (const FTreeOut& T : Trees)
		{
			const FString Key = FString::Printf(TEXT("%s_%d"), *T.Sp.ToString(), T.Var);
			UHierarchicalInstancedStaticMeshComponent*& H = Comps.FindOrAdd(Key);
			if (!H)
			{
				UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), T.Sp, T.Var);
				if (!Mesh) continue;
				H = NewObject<UHierarchicalInstancedStaticMeshComponent>(A);
				H->SetStaticMesh(Mesh);
				H->SetupAttachment(A->GetRootComponent());
				H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				H->SetCanEverAffectNavigation(false);
				H->SetCullDistances(0, int32(WRTune(TEXT("skyWorld.decor.treeCull"), 90000.f)));
				H->RegisterComponent();
			}
			H->AddInstance(T.Xf);
		}
	}
	// ---- grass tufts combed downwind, and flax / cloudberry in the gardens ----
	const int32 NTufts = FMath::Clamp(int32(Area / 120.f), 12, 260);
	const FVector Down = FRotator(0.f, WindLocal, 0.f).Vector();
	for (int32 t = 0; t < NTufts; ++t)
	{
		const float Rho = 0.1f + 0.85f * FMath::Sqrt(C.Rand(t * 7 + 300));
		const float Ang = t * 2.39996f + 0.7f;
		const FVector2D P = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * I.EdgeRadius(Ang) * Rho;
		if (!Free(P, 300.f) || (I.bLagoon && Rho < 0.8f) || (bTemple && Rho < 0.82f)) continue;
		const FVector Base(P.X, P.Y, C.G(P.X, P.Y));
		const bool bFlax = (I.Kind == TEXT("garden")) && C.Rand(t * 7 + 301) < 0.35f;
		const FSurf Blade = bFlax ? SkFlax() : I.Zone == '5' ? FSurf::Cloth(SkSRGB(0x6E, 0x7A, 0x66) * 0.5f) : FSurf::Cloth(FMath::Lerp(SkSRGB(0x9D, 0xB5, 0x9A), SkSRGB(0xC8, 0xCC, 0xB0), C.Rand(t)) * 0.5f);
		for (int32 b = 0; b < 3; ++b)
		{
			const FVector Lean = (FVector::UpVector + Down * (0.7f + 0.3f * C.Rand(t * 3 + b))).GetSafeNormal();
			C.B.K.Spike(Blade, Base + FVector((b - 1) * 9.f, (b % 2) * 7.f, -2.f), Lean, 7.f, 40.f + 25.f * C.Rand(t * 5 + b));
		}
		if (bFlax) C.B.K.Add(SkFlax(), FVector(10.f, 10.f, 6.f), Base + Down * 36.f + FVector(0, 0, 38.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}
	// ---- boulders at the rim and levistone glints in it ----
	const float Perim = 2.f * PI * I.R() * 0.8f;
	const int32 NRocks = FMath::Clamp(int32(Perim / 3500.f), 2, 30);
	for (int32 r = 0; r < NRocks; ++r)
	{
		const float Ang = 2.f * PI * (r + C.Rand(r + 900)) / NRocks;
		const FVector2D P = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * I.EdgeRadius(Ang) * 0.9f;
		if (!Free(P, 600.f)) continue;
		const float S = 120.f + 220.f * C.Rand(r + 901);
		C.B.Rock(r, FVector(S * 1.3f, S, S * 0.7f), FVector(P.X, P.Y, C.G(P.X, P.Y)), FRotator(0.f, C.Rand(r + 902) * 360.f, 0.f), SkRimColour(I.Zone, 0.3f) * 0.6f, true, 30.f);
		if (r % 3 == 0)
		{
			const FVector Lip(FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * I.EdgeRadius(Ang) * 1.0f, -300.f - 600.f * C.Rand(r + 903));
			C.B.K.Spike(SkLevistone(1.f), Lip, FVector(FMath::Cos(Ang), FMath::Sin(Ang), -0.2f).GetSafeNormal(), 40.f, 110.f);
			C.B.N.Spike(SkLiftGlow(5.f), Lip, FVector(FMath::Cos(Ang), FMath::Sin(Ang), -0.2f).GetSafeNormal(), 22.f, 116.f);
		}
	}
	// ---- wind-poles every 150 m round the lip (at least one on any island over 60 m) ----
	if (I.Size >= 6000.f && I.Zone != '5')
	{
		const int32 NPoles = FMath::Max(1, int32(Perim / WRTune(TEXT("skyWorld.decor.poleSpacing"), 15000.f)));
		for (int32 p = 0; p < NPoles; ++p)
		{
			const float Ang = 2.f * PI * (p + 0.37f) / NPoles;
			const FVector2D P = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * I.EdgeRadius(Ang) * 0.86f;
			if (!Free(P, 800.f)) continue;
			const FVector Base(P.X, P.Y, C.G(P.X, P.Y));
			SkWindPole(C.B, Base, p + I.Seed, true);
			AddStreamer(A, Base + FVector(0, 0, 455.f), p + I.Seed);
		}
	}
}

// ================================================================================================ bridges

namespace WRSkyPartsImpl
{
	/** A walkable slab: look + collision box, both along A -> B (local), Width across, Thick deep. */
	void Slab(WRSkyParts::FCtx& C, const FSurf& S, const FVector& P0, const FVector& P1, float Width, float Thick)
	{
		const FVector D = P1 - P0;
		const float L = D.Size();
		if (L < 1.f) return;
		const FRotator R = D.Rotation();
		C.B.K.Add(S, FVector(L, Width, Thick), (P0 + P1) * 0.5f - R.RotateVector(FVector(0, 0, Thick * 0.5f)), R);
		C.B.Box((P0 + P1) * 0.5f - R.RotateVector(FVector(0, 0, Thick * 0.5f)), FVector(L * 0.5f, Width * 0.5f, Thick * 0.5f), R);
	}
}

void WRSkyParts::Bridge(FCtx& C)
{
	if (!C.D.Bridges.IsValidIndex(C.A->Index)) return;
	const FWRSkyIsleData::FBridge& Br = C.D.Bridges[C.A->Index];
	AWRSkyScenery* A = C.A;
	const FVector P0 = C.L(Br.A), P1 = C.L(Br.B);
	const FVector Along = (P1 - P0).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
	const float Span = FVector::Dist2D(P0, P1);
	const FWRSkyIsleData::FIsland* IA = C.D.FindIsland(Br.From);
	const TCHAR Zone = IA ? IA->Zone : '3';
	const float W = Br.Width;
	const FName K = Br.Kind;
	if (K == TEXT("deck") || K == TEXT("planks") || K == TEXT("stub"))
	{
		const bool bStone = K == TEXT("deck") || (K == TEXT("stub") && W >= 400.f);
		const bool bStub = K == TEXT("stub");
		const float Reach = bStub ? Br.Reach : 1.f;
		const float Sag = Span * (bStone ? 0.015f : 0.03f);
		const float Step = bStone ? 200.f : 60.f;
		const int32 N = FMath::Max(2, int32(Span * Reach / Step));
		const bool bSentinel = Br.Id == TEXT("sentinel_deck");
		// The chains: two under each edge carrying the deck, two at hand height on iron posts (the Luding bridge's way).
		const FVector End = WRForestLook::Sag(P0, P1, Reach, Sag);
		for (int32 e = -1; e <= 1; e += 2)
		{
			const FVector Off = Side * (W * 0.5f - 15.f) * e;
			if (!bStub)
			{
				SkChain(C.B, P0 + Off - FVector(0, 0, 25.f), P1 + Off - FVector(0, 0, 25.f), Sag, 26.f, 40);
				SkChain(C.B, P0 + Off + FVector(0, 0, 105.f), P1 + Off + FVector(0, 0, 105.f), Sag * 1.1f, 22.f, 40);
			}
			else
			{
				// A broken end: the chains run out to where it broke, then hang down in loose falls, swaying in the wind.
				C.B.Line(P0 + Off - FVector(0, 0, 25.f), End + Off - FVector(0, 0, 25.f), 4.f, Sag * Reach, SkChainC(), 16);
				C.B.Line(End + Off - FVector(0, 0, 25.f), End + Off - FVector(0, 0, 1200.f + 900.f * C.Rand(e + 3)) + Along * 120.f, 4.f, 40.f, SkChainC(), 10);
			}
		}
		for (int32 i = 0; i < N; ++i)
		{
			const float T0 = Reach * float(i) / N, T1 = Reach * float(i + 1) / N;
			const FVector A0 = WRForestLook::Sag(P0, P1, T0, Sag), A1 = WRForestLook::Sag(P0, P1, T1, Sag);
			// A stub's last pieces are cracked and tilted down into the drop.
			const bool bBroken = bStub && i >= N - 2;
			const FVector Tip = bBroken ? FVector(0, 0, -60.f * (i - N + 3)) : FVector::ZeroVector;
			const FSurf S = bStone ? SkRimStone(Zone, 0.9f + 0.15f * C.Rand(i)) : SkPlank(0.85f + 0.25f * C.Rand(i));
			if (bSentinel)
			{
				// The Sentinel's deck: each slab its own part so a broken one can go (WRSkyBosses); weak slabs (5
				// sections) are cracked, darker, with fresh chips under them.
				bool bWeak = false;
				for (const float Wk : Br.Weak) if (FMath::Abs((T0 + T1) * 0.5f - Wk) < 0.035f) bWeak = true;
				UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(A);
				M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				M->SetupAttachment(A->GetRootComponent());
				const FVector D = A1 - A0;
				M->SetRelativeLocation((A0 + A1) * 0.5f - FVector(0, 0, 25.f));
				M->SetRelativeRotation(D.Rotation());
				M->SetRelativeScale3D(FVector(D.Size() / 100.f, W / 100.f, 0.5f));
				M->SetCollisionProfileName(TEXT("BlockAll"));
				if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock")))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, A);
					MID->SetVectorParameterValue(TEXT("Tint"), SkRimColour(Zone, 0.4f) * (bWeak ? 0.55f : 0.75f));
					M->SetMaterial(0, MID);
				}
				M->RegisterComponent();
				A->Slabs.Add(M);
				A->SlabWeak.Add(bWeak);
				if (bWeak)
				{
					for (int32 cr = 0; cr < 3; ++cr) C.B.K.Add(FSurf::Stone(SkSRGB(0x3A, 0x38, 0x34) * 0.5f, 0.9f), FVector(D.Size() * 0.8f, 4.f, 52.f), (A0 + A1) * 0.5f + Side * (cr - 1) * W * 0.25f, D.Rotation() + FRotator(0.f, 20.f * (cr - 1), 0.f));
				}
				continue;
			}
			WRSkyPartsImpl::Slab(C, S, A0 + Tip * 0.5f, A1 + Tip, bStone ? W : W - 20.f, bStone ? 45.f : 8.f);
			// Plank decks: a gap between boards, the cross-ties underneath on the chains.
			if (!bStone && i % 3 == 0) C.B.K.Rod(SkCedar(0.8f), A0 - Side * W * 0.55f - FVector(0, 0, 14.f), A0 + Side * W * 0.55f - FVector(0, 0, 14.f), 8.f, true);
			// Handrail posts every 4 m.
			if (i % int32(400.f / Step + 0.5f) == 0)
			{
				for (int32 e = -1; e <= 1; e += 2)
				{
					const FVector Post = A0 + Side * (W * 0.5f - 15.f) * e;
					C.B.K.Rod(bStone ? SkRustIron() : SkCedar(0.85f), Post, Post + FVector(0, 0, 110.f), bStone ? 7.f : 9.f);
				}
			}
		}
		// The deck's ends: stone tower-houses where the chains are anchored (the Ruined Sky-bridges' toll towers), or a
		// plank span's two cedar posts and a stone anchor block.
		if (bStone && !bStub)
		{
			SkTollTower(C.B, P0 - Along * 250.f + FVector(0, 0, -10.f), Along, Zone, false);
			const FWRSkyIsleData::FIsland* IB = C.D.FindIsland(Br.To);
			SkTollTower(C.B, P1 + Along * 250.f + FVector(0, 0, -10.f), -Along, IB ? IB->Zone : Zone, Br.Id == TEXT("sentinel_deck") ? false : C.Rand(77) < 0.4f);
		}
		else
		{
			for (const FVector& End0 : { P0, bStub ? P0 : P1 })
			{
				const FVector Back = End0 == P0 ? -Along : Along;
				C.B.K.Add(SkRimStone(Zone, 0.85f), FVector(140.f, W + 80.f, 80.f), End0 + Back * 90.f + FVector(0, 0, 10.f), Back.Rotation());
				for (int32 e = -1; e <= 1; e += 2) C.B.K.Rod(SkCedar(0.8f), End0 + Side * (W * 0.5f + 20.f) * e, End0 + Side * (W * 0.5f + 20.f) * e + FVector(0, 0, 160.f), 16.f);
			}
		}
		return;
	}
	if (K == TEXT("hanging"))
	{
		// The Fallen Span (L3): the deck's north half hangs straight down from Span North's lip on its chains, 120 m, its
		// lower end swinging slowly (a pendulum on the server time: 0.6 degrees, 22 s). Lanterns on it once the Toll
		// Shrine is lit.
		A->Swing = Pivot(A, P0 + FVector(0, 0, -20.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f));
		A->SwingPeriod = WRTune(TEXT("skyWorld.fallenSpan.period"), 22.f);
		A->SwingDegrees = WRTune(TEXT("skyWorld.fallenSpan.degrees"), 0.6f);
		A->SetActorTickEnabled(true);
		const float Hang = FMath::Max(2000.f, Br.Hang);
		A->TollLamps = Pivot(A, FVector::ZeroVector);
		A->TollLamps->AttachToComponent(A->Swing, FAttachmentTransformRules::KeepRelativeTransform);
		const TCHAR Zn = Zone;
		SpawnUnder(A, A->Swing, [W, Hang, Zn](WRLookKit::FKit& Kk)
		{
			// Local to the pivot: X along where the bridge ran, down -Z. The slabs face out (+X), 2 m tall each.
			for (int32 s = 0; s < int32(Hang / 200.f); ++s)
			{
				const float Z = -150.f - s * 200.f;
				const float Kick = 18.f * FMath::Sin(s * 0.7f);
				Kk.Add(SkRimStone(Zn, 0.85f + 0.1f * (s % 3)), FVector(45.f, W, 196.f), FVector(60.f + Kick, 0, Z), FRotator(0.f, 0.f, 0.f));
				for (int32 e = -1; e <= 1; e += 2)
				{
					Kk.Add(SkChainIron(), FVector(10.f, 10.f, 200.f), FVector(30.f, (W * 0.5f - 15.f) * e, Z), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					if (s % 2 == 0) Kk.Rod(SkRustIron(), FVector(85.f + Kick, (W * 0.5f - 15.f) * e, Z), FVector(185.f + Kick, (W * 0.5f - 15.f) * e, Z + 30.f), 7.f);
				}
			}
			// The broken end: jagged slabs, a handrail post bent out.
			Kk.Add(SkRimStone(Zn, 0.7f), FVector(45.f, W * 0.6f, 120.f), FVector(70.f, W * 0.2f, -Hang - 120.f), FRotator(0.f, 0.f, 18.f));
		});
		SpawnUnder(A, A->TollLamps, [Hang](WRLookKit::FKit& Kk)
		{
			for (int32 l = 0; l < 8; ++l) Kk.Add(SkLampGlow(7.f), FVector(28.f, 28.f, 40.f), FVector(200.f, (l % 2 ? 1.f : -1.f) * 140.f, -300.f - l * Hang / 9.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		});
		if (A->TollLamps) A->TollLamps->SetVisibility(false, true);
		// Collision: the hanging deck is a wall you can cling to the face of and climb its chains... a slab column.
		C.B.Box(P0 + FVector(0, 0, -Hang * 0.5f) + Along * 60.f, FVector(25.f, W * 0.5f, Hang * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f));
		// Its anchors on the lip: two chain eyes in a stone sill.
		C.B.K.Add(SkRimStone(Zone, 0.9f), FVector(120.f, W + 60.f, 70.f), P0 - Along * 60.f + FVector(0, 0, 10.f), Along.Rotation());
		return;
	}
	if (K == TEXT("chainwalk"))
	{
		// One heavy chain to walk (the Wheel's drive chain; the Lid Gate's walk to the Ring): big links you set your feet
		// on, a 60 cm walking width, a gentle sag. No rail.
		const float Sag = Span * 0.02f;
		const int32 N = FMath::Max(4, int32(Span / 150.f));
		for (int32 i = 0; i < N; ++i)
		{
			const FVector A0 = WRForestLook::Sag(P0, P1, float(i) / N, Sag), A1 = WRForestLook::Sag(P0, P1, float(i + 1) / N, Sag);
			C.B.Box((A0 + A1) * 0.5f - FVector(0, 0, 12.f), FVector(FVector::Dist(A0, A1) * 0.5f, FMath::Max(30.f, W * 0.5f), 12.f), (A1 - A0).Rotation());
		}
		const int32 Links = int32(Span / 55.f);
		for (int32 l = 0; l < Links; ++l)
		{
			const FVector A0 = WRForestLook::Sag(P0, P1, float(l) / Links, Sag), A1 = WRForestLook::Sag(P0, P1, float(l + 1) / Links, Sag);
			const FVector D = (A1 - A0).GetSafeNormal();
			const FRotator R = FRotationMatrix::MakeFromXZ(D, (l % 2) ? FVector::UpVector : Side).Rotator();
			C.B.K.Add(SkRustIron(), FVector(70.f, 14.f, 34.f), (A0 + A1) * 0.5f - FVector(0, 0, 10.f), R);
		}
		return;
	}
	// ladder / line: built by AWRSkyLine (they're climbed): only their anchors here.
	C.B.K.Add(SkRimStone(Zone, 0.85f), FVector(80.f, 80.f, 60.f), P0 + FVector(0, 0, 20.f), FRotator::ZeroRotator);
	for (int32 e = -1; e <= 1; e += 2) C.B.K.Disc(SkRustIron(), P0 + Side * 20.f * e + FVector(0, 0, 55.f), Along, 18.f, 6.f);
}

void WRSkyParts::Pole(FCtx& C)
{
	SkWindPole(C.B, C.At(0.f, 0.f), C.A->Index, true);
	AddStreamer(C.A, C.At(0.f, 0.f, 455.f), C.A->Index);
}

// ================================================================================================ waterfalls

void WRSkyParts::Falls(FCtx& C)
{
	// Off an island's lip into nothing (SKY L2, 2.6): a sheet thrown out from the lip on a jet's arc, narrowing and
	// breaking into strands, then mist trails all the way down (a fall over ~300 m becomes mist: Angel Falls,
	// Staubbach). Twice as heavy in Rain (WRSkyWorld steps the mist).
	if (!C.D.Falls.IsValidIndex(C.A->Index)) return;
	const FWRSkyIsleData::FFall& F = C.D.Falls[C.A->Index];
	AWRSkyScenery* A = C.A;
	const FVector Lip(0, 0, 0);
	const FVector Flow(1, 0, 0), Side(0, 1, 0);
	const float Wd = F.Width, Drop = F.Drop;
	// A channel cut back into the turf to the lip, its water running out.
	C.B.K.Add(SkLagoonWater(), FVector(900.f, Wd * 0.8f, 6.f), FVector(-450.f, 0, -8.f), FRotator::ZeroRotator);
	for (int32 e = -1; e <= 1; e += 2) C.B.Rock(e + 3, FVector(220.f, 160.f, 120.f), FVector(-200.f, (Wd * 0.5f + 80.f) * e, -60.f), FRotator(0.f, 30.f * e, 0.f), SkRimColour('2', 0.3f) * 0.6f, true, 30.f);
	if (A->GetNetMode() == NM_DedicatedServer) return;
	TArray<FVector> Prof;
	const float Sheet = FMath::Min(Drop * 0.4f, 9000.f);
	const int32 Steps = 18;
	for (int32 i = 0; i <= Steps; ++i)
	{
		const float Fallen = Sheet * float(i) / Steps;
		Prof.Add(Lip + Flow * (40.f + 7.f * FMath::Sqrt(Fallen)) - FVector(0, 0, Fallen));
	}
	TArray<FVector> Verts, Normals;
	TArray<int32> Tris;
	TArray<FVector2D> UVs;
	TArray<FColor> Cols;
	const auto Sheet2 = [&](float Forward, float WidthK, float Narrow)
	{
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const int32 Start = Verts.Num();
			float AlongU = 0.f;
			for (int32 i = 0; i < Prof.Num(); ++i)
			{
				if (i > 0) AlongU += FVector::Dist(Prof[i], Prof[i - 1]);
				const float T = float(i) / (Prof.Num() - 1);
				const float Wi = Wd * WidthK * FMath::Lerp(1.f, Narrow, T);
				for (int32 c = 0; c < 3; ++c)
				{
					Verts.Add(Prof[i] + Side * (c - 1) * 0.5f * Wi + Flow * (Forward + (c == 1 ? 20.f : 0.f)));
					Normals.Add(Face == 0 ? Flow : -Flow);
					UVs.Add(FVector2D(AlongU / 600.f, c * 0.5f));
					Cols.Add(FColor::White);
				}
			}
			for (int32 i = 0; i + 1 < Prof.Num(); ++i)
			{
				for (int32 c = 0; c < 2; ++c)
				{
					const int32 V0 = Start + i * 3 + c, V1 = V0 + 1, V2 = V0 + 3, V3 = V0 + 4;
					if (Face == 0) Tris.Append({ V0, V2, V1, V1, V2, V3 });
					else Tris.Append({ V0, V1, V2, V1, V3, V2 });
				}
			}
		}
	};
	Sheet2(0.f, 1.f, 0.35f);
	Sheet2(35.f, 0.55f, 0.2f);
	UProceduralMeshComponent* Water = NewObject<UProceduralMeshComponent>(A);
	Water->SetupAttachment(A->GetRootComponent());
	Water->RegisterComponent();
	Water->CreateMeshSection(0, Verts, Tris, Normals, UVs, Cols, TArray<FProcMeshTangent>(), false);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("skyWorld.fallMaterial"), TEXT("/Game/WaterMaterials/Materials/M_River.M_River"))))
	{
		Water->SetMaterial(0, UMaterialInstanceDynamic::Create(Base, A));
	}
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	// Strands breaking off the sheet's end, then the mist all the way down to where it's gone.
	C.B.Loop(TEXT("sky.fallStrands"), Prof.Last() + FVector(0, 0, -200.f), FRotator::ZeroRotator, Wd / 1200.f);
	for (int32 m = 0; m < 4; ++m)
	{
		const float Z = -Sheet - (Drop - Sheet) * (m + 0.5f) / 4.f;
		C.B.Loop(TEXT("sky.fallMist"), FVector(Prof.Last().X + 200.f + m * 150.f, 0, Z), FRotator::ZeroRotator, (Wd / 1000.f) * (1.f + m * 0.6f));
	}
	// The moonlit mist trail (L2's night read).
	C.B.Lamp(FVector(400.f, 0, -Sheet * 0.6f), FLinearColor(0.75f, 0.85f, 1.f), 2200.f, Wd * 3.f);
}

// ================================================================================================ the Flue's shaft

void WRSkyParts::Flue(FCtx& C)
{
	// The Flue (world-map SPEC: Þríhnúkagígur-style open volcanic throat): a 48-sided wall of dark rock from the rim at
	// 700 m down to the cavern floor at -40 m, its radius wavering; bird ledges and wind-scoured rock high up, fungus
	// shelves and spore crust low down, a cyan glow from the floor; the floor itself a disc of mycelium and rock so a
	// Featherfall drop lands alive. The actor stands on the throat's axis at the floor.
	AWRSkyScenery* A = C.A;
	const FWRSkyIsleData& D = C.D;
	// The wall's top sits 2 m under the rim's turf and flares out past the land's hidden throat (133 m), so the shaft
	// and the ground overlap and there is no slit between them to fall through.
	const float Rr = D.FlueThroat, Top = D.FlueRim - D.FlueFloor - 200.f;
	const int32 Sides = 48, Rows = 40;
	TArray<FVector> V, N;
	TArray<int32> T;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<FProcMeshTangent> Tan;
	for (int32 r = 0; r <= Rows; ++r)
	{
		const float Z = Top * float(r) / Rows;
		const float Hn = Z / Top;
		for (int32 s = 0; s <= Sides; ++s)
		{
			const float Ang = 2.f * PI * s / Sides;
			const float Wv = 1.f + 0.06f * FMath::Sin(Ang * 3.f + Hn * 9.f) + 0.04f * FMath::Sin(Ang * 7.f - Hn * 15.f) + 0.03f * FMath::Sin(Hn * 40.f + Ang);
			const float Rad = Rr * Wv * (1.08f - 0.08f * Hn) * (1.f + 0.3f * FMath::Square(FMath::Clamp((Hn - 0.92f) / 0.08f, 0.f, 1.f)));
			const FVector P(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad, Z);
			V.Add(P);
			N.Add(-FVector(P.X, P.Y, 0.f).GetSafeNormal());
			UV.Add(FVector2D(Ang * Rr / 400.f, Z / 400.f));
			FLinearColor Cc = FMath::Lerp(SkSRGB(0x3E, 0x3B, 0x38), SkSRGB(0x6E, 0x6A, 0x62), 0.5f + 0.5f * FMath::Sin(Z * 0.003f + Ang * 4.f)) * 0.8f;
			if (Hn < 0.3f) Cc = FMath::Lerp(SkSRGB(0x8A, 0x8A, 0x80) * 0.6f, Cc, Hn / 0.3f);   // spore crust low down
			Cc.A = 0.15f;
			Col.Add(Cc);
			Tan.Add(FProcMeshTangent(FVector(0, 0, 1), false));
		}
	}
	// The Fungal caverns' river passage comes through the wall at the floor (cooks/build-biome-fungal.md 2.0.7): while they
	// are live, the quads in its window are left out (the caverns' rock closes round the hole's edges).
	float MouthYaw = 0.f, MouthHalf = 0.f, MouthTop = 0.f;
	const bool bMouth = WRFungal::FlueMouth(MouthYaw, MouthHalf, MouthTop);
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 s = 0; s < Sides; ++s)
		{
			if (bMouth && Top * float(r) / Rows < MouthTop)
			{
				const float Mid = 360.f * (s + 0.5f) / Sides;
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(Mid, MouthYaw)) <= MouthHalf + 180.f / Sides) continue;   // the one panel the mouth chamber covers
			}
			const int32 A0 = r * (Sides + 1) + s, A1 = A0 + 1, B0 = A0 + Sides + 1, B1 = B0 + 1;
			T.Append({ A0, A1, B1, A0, B1, B0 });
		}
	}
	UProceduralMeshComponent* Wall = NewObject<UProceduralMeshComponent>(A);
	Wall->SetupAttachment(A->GetRootComponent());
	Wall->bUseAsyncCooking = true;
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->CreateMeshSection_LinearColor(0, V, T, N, UV, Col, Tan, true);
	if (A->GetNetMode() != NM_DedicatedServer)
	{
		if (UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_LandGround.M_LandGround"))) Wall->SetMaterial(0, Ground);
	}
	Wall->RegisterComponent();
	// The floor: mycelium over rock, and the fungus shelves round the low wall; bird ledges up high.
	C.B.K.Disc(FSurf::Cloth(SkSRGB(0xB9, 0xB9, 0xAE) * 0.45f), FVector(0, 0, -10.f), FVector::UpVector, Rr * 2.1f, 40.f);
	C.B.Box(FVector(0, 0, -30.f), FVector(Rr * 1.05f, Rr * 1.05f, 30.f));
	for (int32 s = 0; s < 36; ++s)
	{
		const float Ang = 2.f * PI * (s + C.Rand(s)) / 36.f;
		const float Z = s < 24 ? 200.f + 9000.f * C.Rand(s + 50) : Top * (0.55f + 0.4f * C.Rand(s + 51));
		const FVector P(FMath::Cos(Ang) * Rr * 0.97f, FMath::Sin(Ang) * Rr * 0.97f, Z);
		if (s < 24)
		{
			// A bracket shelf of fungus, pale with a cyan rim at night.
			C.B.K.Add(FSurf::Cloth(SkSRGB(0xC9, 0xC2, 0xB0) * 0.5f), FVector(260.f, 420.f, 50.f), P - FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 120.f, FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f), EWRLookMesh::Cylinder);
			C.B.N.Add(SkSporeGlow(3.f), FVector(270.f, 430.f, 12.f), P - FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 120.f - FVector(0, 0, 26.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f), EWRLookMesh::Cylinder);
		}
		else
		{
			C.B.K.Add(FSurf::Stone(SkSRGB(0x5E, 0x5A, 0x55) * 0.55f, 0.9f), FVector(200.f, 500.f, 60.f), P - FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 90.f, FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));
			C.B.K.Add(FSurf::Stone(SkSRGB(0xE8, 0xE2, 0xD8) * 0.6f, 0.9f), FVector(150.f, 400.f, 6.f), P - FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 90.f + FVector(0, 0, 33.f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f));   // guano
		}
	}
	// The spore glow from the floor and the column of spores carried up the updraft (fx), seen from the islands.
	C.B.Lamp(FVector(0, 0, 600.f), SkSRGB(0x39, 0xE6, 0xC2), 30000.f, Rr * 3.f);
	C.B.Loop(TEXT("sky.flueSpores"), FVector(0, 0, 400.f));
	C.B.Loop(TEXT("sky.flueSpores"), FVector(0, 0, Top * 0.5f));
	C.B.Loop(TEXT("sky.flueSpores"), FVector(0, 0, Top - 600.f));
}

void WRSkyParts::Bank(FCtx& C)
{
	// The Undercloud's bank (SKY 2.7): a cloud bank 1 km across at 500-600 m that never moves; from outside a white
	// wall of cloud with the ocean showing round it; inside, fog (WRSky's look: vision 25 m). The gap in its top under
	// the Kitefield's lip is the hole the Mistlarks dive into. Cloud cards (fx) round its rim and over its top.
	const FWRSkyIsleData& D = C.D;
	for (int32 k = 0; k < 18; ++k)
	{
		const float Ang = 2.f * PI * k / 18.f;
		C.B.Loop(TEXT("sky.cloudBank"), FVector(FMath::Cos(Ang) * D.UnderRadius * 0.92f, FMath::Sin(Ang) * D.UnderRadius * 0.92f, (D.UnderTop - D.UnderBottom) * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(Ang), 0.f), 6.f);
	}
	for (int32 k = 0; k < 12; ++k)
	{
		const float Ang = 2.f * PI * (k + 0.5f) / 12.f;
		const FVector P(FMath::Cos(Ang) * D.UnderRadius * 0.5f, FMath::Sin(Ang) * D.UnderRadius * 0.5f, D.UnderTop - D.UnderBottom - 500.f);
		const FVector G(D.UnderGap - FVector2D(D.UnderAt), 0.f);
		if (FVector::Dist2D(P, G) < 6000.f) continue;   // the gap stays open
		C.B.Loop(TEXT("sky.cloudTop"), P, FRotator::ZeroRotator, 8.f);
	}
}

// ================================================================================================ landmarks

void WRSkyParts::PaperTitan(FCtx& C)
{
	// L4 / H15 (world-map SPEC): the Deepfarmers' mushroom titan, 40 m tall, kneeling on the highest island, dried by the
	// wind to grey paper; Kitewright spears through it; a broken kite line round its neck; its gill-ribs catch the moon;
	// a faint spore glow at its roots. Built facing +X (the actor's yaw).
	const FLinearColor PaperC = SkSRGB(0xB9, 0xB9, 0xAE) * 0.55f, Dark = SkSRGB(0x8A, 0x8A, 0x80) * 0.5f;
	const float Kneel = C.G(0, 0);
	const FVector Hip(0, 0, Kneel + 900.f);
	// Legs: a thigh down to a knee on the ground forward, a shin back; the other leg folded under.
	C.B.Limb(Hip + FVector(0, 350, 0), FVector(600.f, 380.f, Kneel + 120.f), 260.f, 200.f, PaperC, 12);
	C.B.Limb(FVector(600.f, 380.f, Kneel + 120.f), FVector(-200.f, 420.f, Kneel + 60.f), 200.f, 140.f, PaperC, 12);
	C.B.Limb(Hip - FVector(0, 350, 0), FVector(-300.f, -420.f, Kneel + 80.f), 260.f, 180.f, PaperC, 12);
	// The stalk-body, bent forward over the knee, ringed with a torn annulus.
	TArray<FVector> Spine = { Hip, Hip + FVector(150, 0, 900), Hip + FVector(450, 0, 1800), Hip + FVector(800, 0, 2500) };
	C.B.Root(Spine, 520.f, 380.f, PaperC, 14);
	C.B.M.Lathe(Hip + FVector(300, 0, 1400) - FVector(0, 0, 60.f), FVector(0.25f, 0, 1).GetSafeNormal(), 120.f, { FVector(0, 470, 470), FVector(0.5f, 640, 640), FVector(1, 470, 470) }, 18, Dark, 0.9f);
	// Arms: one hanging to the ground, one reaching up to the sky (the last thing it did).
	const FVector Shoulder = Hip + FVector(700, 0, 2200);
	C.B.Root({ Shoulder + FVector(0, 420, 0), Shoulder + FVector(500, 900, -900), Shoulder + FVector(900, 1000, -2000) }, 170.f, 90.f, PaperC, 10);
	C.B.Root({ Shoulder - FVector(0, 420, 0), Shoulder + FVector(200, -900, 800), Shoulder + FVector(500, -1100, 1700) }, 170.f, 80.f, PaperC, 10);
	// The cap: a 22 m paper dome tipped forward, gill-ribs under it in two hundred thin sheets.
	const FVector CapC = Hip + FVector(1000, 0, 2900);
	C.B.M.Lathe(CapC - FVector(0, 0, 200.f), FVector(0.3f, 0, 1).GetSafeNormal(), 900.f, { FVector(0, 1100, 1100), FVector(0.15f, 1150, 1150), FVector(0.45f, 1000, 1000), FVector(0.8f, 600, 600), FVector(1, 80, 80) }, 24, PaperC, 0.95f);
	for (int32 g = 0; g < 72; ++g)
	{
		const float Ang = 2.f * PI * g / 72.f;
		const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
		C.B.K.Add(FSurf::Cloth(SkSRGB(0xC9, 0xC6, 0xBA) * 0.5f), FVector(1000.f, 3.f, 180.f), CapC + Dir * 560.f - FVector(0, 0, 260.f), FRotator(-8.f, FMath::RadiansToDegrees(Ang), 0.f));
	}
	// Kitewright spears through it: six long cedar shafts with bronze blades, at the angles they were thrown.
	for (int32 s = 0; s < 6; ++s)
	{
		const FVector In = Hip + FVector(200.f + 150.f * s, (s % 2 ? 1 : -1) * 150.f, 600.f + 280.f * s);
		const FVector Dir = FVector(-0.4f + 0.15f * s, (s % 2 ? -1.f : 1.f) * 0.6f, -0.35f + 0.1f * (s % 3)).GetSafeNormal();
		C.B.K.Rod(SkCedar(0.7f), In - Dir * 1100.f, In + Dir * 700.f, 14.f);
		C.B.K.Spike(SkBronze(), In + Dir * 700.f, Dir, 26.f, 90.f);
		C.B.K.Add(SkCloth(s, 0.6f), FVector(4.f, 40.f, 90.f), In - Dir * 1050.f, Dir.Rotation());
	}
	// The broken kite line round its neck, trailing off the island's lip.
	TArray<FVector> Neck;
	for (int32 k = 0; k <= 12; ++k) { const float Ang = 2.f * PI * k / 12.f; Neck.Add(Hip + FVector(800, 0, 2550) + FVector(FMath::Cos(Ang) * 420.f, FMath::Sin(Ang) * 420.f, 30.f * FMath::Sin(Ang * 2.f))); }
	C.B.M.Tube(Neck, 4.f, 6, SkRopeC(), 0.95f);
	C.B.Line(Hip + FVector(800, 420, 2550), Hip + FVector(3500, 2600, -1500), 3.f, 300.f, SkRopeC(), 14);
	// Collision: the body, the knee and the cap's rim (you can climb onto its knee).
	C.B.SegBox(Hip, Hip + FVector(800, 0, 2500), 450.f, 450.f);
	C.B.SegBox(Hip + FVector(0, 380, 0), FVector(600.f, 380.f, Kneel + 120.f), 200.f, 200.f);
	C.B.Box(CapC + FVector(0, 0, 200.f), FVector(1000.f, 1000.f, 300.f));
	// Spore glow at its roots (faint, cyan), moon on the gills.
	C.B.Loop(TEXT("sky.sporeDrift"), FVector(0, 0, Kneel + 100.f), FRotator::ZeroRotator, 1.5f);
	C.B.Lamp(FVector(100, 0, Kneel + 150.f), SkSRGB(0x39, 0xE6, 0xC2), 2500.f, 1800.f);
}

void WRSkyParts::GreatKite(FCtx& C)
{
	// L5: a 60 m Kitewright kite of faded red, yellow, green and blue on a chain-and-silk line, still flying, tethered to
	// the anchor drum at the Titan's feet. It always streams downwind: the world's weathervane (WRSkyScenery::Tick turns
	// it). Its line is a 300 m catenary with 12 lanterns. The anchor drum (H15) is a bronze-bound cedar drum on a stone
	// plinth, the line wound round it.
	AWRSkyScenery* A = C.A;
	const FVector Drum = C.At(0, 0);
	C.B.K.Add(SkTempleWhite(0.85f), FVector(500.f, 500.f, 120.f), Drum + FVector(0, 0, 60.f), FRotator::ZeroRotator);
	C.B.K.Add(SkCedar(0.9f), FVector(260.f, 260.f, 320.f), Drum + FVector(0, 0, 300.f), FRotator(90.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	for (int32 b = -1; b <= 1; ++b) C.B.K.Disc(SkBronze(), Drum + FVector(0, b * 140.f, 300.f), FVector(0, 1, 0), 285.f, 14.f);
	for (int32 r = 0; r < 6; ++r) C.B.K.Disc(FSurf::Cloth(SkSRGB(0xC9, 0xB8, 0x90) * 0.5f), Drum + FVector(0, -100.f + r * 40.f, 300.f), FVector(0, 1, 0), 270.f, 30.f);
	C.B.Box(Drum + FVector(0, 0, 220.f), FVector(250.f, 250.f, 220.f));
	A->Swing = Pivot(A, Drum + FVector(0, 0, 420.f));
	A->SetActorTickEnabled(true);
	const float Out = WRTune(TEXT("skyWorld.greatKite.out"), 22000.f), Up = WRTune(TEXT("skyWorld.greatKite.up"), 14000.f);
	SpawnUnder(A, A->Swing, [Out, Up](WRLookKit::FKit& K)
	{
		// The line: chain near the drum, silk beyond, a catenary out (+X downwind) and up to the kite; 12 lanterns.
		const FVector KiteAt(Out, 0, Up);
		for (int32 s = 0; s < 60; ++s)
		{
			const float T0 = s / 60.f, T1 = (s + 1) / 60.f;
			const FVector P0 = FMath::Lerp(FVector::ZeroVector, KiteAt, T0) - FVector(0, 0, 4.f * 2600.f * T0 * (1.f - T0));
			const FVector P1 = FMath::Lerp(FVector::ZeroVector, KiteAt, T1) - FVector(0, 0, 4.f * 2600.f * T1 * (1.f - T1));
			K.Rod(s < 6 ? SkChainIron() : SkRope(), P0, P1, s < 6 ? 9.f : 4.f);
			if (s % 5 == 2) K.Add(SkLampGlow(4.f), FVector(24.f, 24.f, 36.f), P0 - FVector(0, 0, 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		// The kite: a 60 m hexagon of panels on cedar spars, facing the wind (its face toward -X, tilted back).
		const FRotator Face(55.f, 0.f, 0.f);
		const FVector Up2 = Face.RotateVector(FVector(0, 0, 1)), Side = FVector(0, 1, 0);
		for (int32 p = 0; p < 12; ++p)
		{
			const float Ang = 2.f * PI * (p + 0.5f) / 12.f;
			const FVector Mid = KiteAt + (Side * FMath::Cos(Ang) + Up2 * FMath::Sin(Ang)) * 1500.f;
			K.Add(SkCloth(p % 4 == 0 ? 2 : p % 4 == 1 ? 4 : p % 4 == 2 ? 3 : 0, 0.75f), FVector(6.f, 1500.f, 1600.f), Mid, FRotator(Face.Pitch, 0.f, FMath::RadiansToDegrees(Ang) + 90.f));
		}
		for (int32 sp = 0; sp < 3; ++sp)
		{
			const float Ang = PI * sp / 3.f;
			const FVector D = Side * FMath::Cos(Ang) + Up2 * FMath::Sin(Ang);
			K.Rod(SkCedar(0.8f), KiteAt - D * 3000.f, KiteAt + D * 3000.f, 30.f);
		}
		// The tail: a long ribbon of knotted cloth bows streaming downwind below it.
		for (int32 t = 0; t < 14; ++t) K.Add(SkCloth(t, 0.8f), FVector(3.f, 220.f, 120.f), KiteAt - Up2 * 3000.f + FVector(600.f * t, 0, -180.f * t + 60.f * FMath::Sin(t * 0.9f)), FRotator(0.f, 0.f, 30.f * FMath::Sin(t * 1.3f)));
	});
}

void WRSkyParts::RingSky(FCtx& C)
{
	// L1's sky: a lenticular cap of cloud over the Ring in calm weather, and the spore glow rising up the Flue's
	// column to it at night. (The platforms, chains, wind-stones and braziers are the arena's: WRSkyBosses.)
	C.B.Loop(TEXT("sky.lenticular"), FVector(0, 0, 9000.f), FRotator::ZeroRotator, 10.f);
	C.B.Loop(TEXT("sky.flueSpores"), FVector(0, 0, -2000.f), FRotator::ZeroRotator, 2.f);
	C.B.Lamp(FVector(0, 0, -1500.f), SkSRGB(0x39, 0xE6, 0xC2), 12000.f, 9000.f);
}

void WRSkyParts::ChainCliffs(FCtx& C)
{
	// L6's head (the Chain Cliffs, world-map SPEC): where the Windward Cliffs meet the Blowhole Shelf, the Kitewrights'
	// old anchor stones stand on the lip with the snapped ends of earlier lift chains running down the cliff face into the
	// surf; a cairn and a brazier the lift-keepers lit. The three working lifts are AWRSkyLift.
	for (int32 a = 0; a < 3; ++a)
	{
		const FVector P = C.At(-300.f + a * 300.f, -200.f + a * 120.f);
		C.B.K.Add(SkCliffGranite(0.9f), FVector(160.f, 160.f, 220.f), P + FVector(0, 0, 100.f), FRotator(0.f, a * 25.f, 0.f));
		C.B.Box(P + FVector(0, 0, 100.f), FVector(80.f, 80.f, 110.f), FRotator(0.f, a * 25.f, 0.f));
		C.B.K.Disc(SkRustIron(), P + FVector(85.f, 0, 170.f), FVector(1, 0, 0), 40.f, 10.f);
		// The chain over the lip and down the face (east, +X), lying on the rock.
		const FVector Lip = P + FVector(1800.f, 0, 0);
		C.B.Line(P + FVector(90.f, 0, 170.f), FVector(Lip.X, Lip.Y, C.G(Lip.X, Lip.Y) + 20.f), 6.f, 30.f, SkChainC(), 10);
		C.B.Line(FVector(Lip.X, Lip.Y, C.G(Lip.X, Lip.Y) + 20.f), Lip + FVector(1600.f, a * 150.f, -9000.f), 6.f, 200.f, SkChainC(), 14);
	}
	SkWindPole(C.B, C.At(200.f, 300.f), 3, true);
	AddStreamer(C.A, C.At(200.f, 300.f, 455.f), 3);
	SkBrazier(C.B, C.At(-200.f, 400.f), 3600.f);
}
