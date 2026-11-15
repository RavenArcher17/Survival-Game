// The Ocean's gathering nodes' looks (cooks/biome-ocean.md 2.7; cooks/build-biome-ocean.md 2.5), modelled in code on
// M_HeldTool like the tools: tidecoral alive in orange, pink and cream (it dies white within 10 s once the tide leaves it
// in air, and comes back when the sea covers it again), sea kelp and sea grapes, oysters, urchins, salt crust, sea-glass,
// driftwood, kelp ash, bronze scrap, the caves' gems. WRResourceLook asks here first for these items.
#include "WROceanTypes.h"
#include "WROceanWorld.h"
#include "Wildreach.h"
#include "WRToolMesh.h"
#include "WRUnderwater.h"
#include "ProceduralMeshComponent.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace WROceanNodesImpl
{
	FLinearColor S(uint8 R, uint8 G, uint8 B, float K = 0.6f) { return FLinearColor(FColor(R, G, B)) * K; }
	void Branch(FWRToolMesh& M, const FVector& A, const FVector& B, float R0, float R1, const FLinearColor& C)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f) return;
		M.Lathe(A, D / L, L, { FVector(0.f, R0, R0), FVector(1.f, R1, R1) }, 6, C, 0.6f);
	}
	void Blob(FWRToolMesh& M, const FVector& C, float R, const FLinearColor& Col, float Squash = 1.f, int32 Sides = 8)
	{
		TArray<FVector> P;
		for (int32 i = 0; i <= 5; ++i) { const float A = PI * i / 5.f; P.Add(FVector(i / 5.f, FMath::Max(0.5f, R * FMath::Sin(A)), FMath::Max(0.5f, R * FMath::Sin(A)))); }
		M.Lathe(C - FVector(0, 0, R * Squash), FVector::UpVector, 2.f * R * Squash, P, Sides, Col, 0.7f);
	}
	/** Living tidecoral (section 0) and the same shape dead white (section 1). */
	void Coral(FWRToolMesh& Alive, FWRToolMesh& Dead, FRandomStream& Rng)
	{
		const FLinearColor Cols[3] = { S(0xE0, 0x7A, 0x4F), S(0xE8, 0xA0, 0xA8), S(0xEA, 0xDF, 0xC8) };
		const FLinearColor C = Cols[Rng.RandRange(0, 2)];
		const FLinearColor W = S(0xF2, 0xEF, 0xE6, 0.75f);
		const int32 N = 7 + Rng.RandRange(0, 4);
		for (int32 b = 0; b < N; ++b)
		{
			const FVector Dir = FRotator(Rng.FRandRange(25.f, 70.f), Rng.FRandRange(0.f, 360.f), 0.f).Vector();
			const FVector Mid = Dir * Rng.FRandRange(40.f, 70.f) + FVector(0, 0, 10.f);
			const FVector Tip = Mid + (Dir + FVector(0, 0, 0.7f)).GetSafeNormal() * Rng.FRandRange(30.f, 60.f);
			for (FWRToolMesh* M : { &Alive, &Dead })
			{
				const FLinearColor Cc = M == &Alive ? C : W;
				Branch(*M, FVector(0, 0, 5.f), Mid, 7.f, 5.f, Cc);
				Branch(*M, Mid, Tip, 5.f, 2.f, Cc * 1.1f);
				Blob(*M, Tip, 3.5f, Cc * 1.2f);
			}
		}
		for (FWRToolMesh* M : { &Alive, &Dead }) Blob(*M, FVector(0, 0, 8.f), 28.f, (M == &Alive ? C : W) * 0.8f, 0.5f);
	}
}

void UWROceanCoralLife::TickComponent(float Dt, ELevelTick T, FActorComponentTickFunction* F)
{
	Super::TickComponent(Dt, T, F);
	UProceduralMeshComponent* M = Mesh.Get();
	if (!M || !GetOwner()) return;
	const bool bUnder = WRUnderwater::WaterAt(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 50.f)) != nullptr;
	InAir = bUnder ? 0.f : InAir + Dt;
	const bool bWantDead = InAir >= 10.f;
	if (bWantDead != bDead)
	{
		bDead = bWantDead;
		M->SetMeshSectionVisible(0, !bDead);
		M->SetMeshSectionVisible(1, bDead);
	}
}

bool WROcean::IsNodeItem(FName Item)
{
	static const TSet<FName> Items = { TEXT("tidecoral"), TEXT("bronze_scrap"), TEXT("kelp_ash"), TEXT("salt"), TEXT("sea_glass"), TEXT("driftwood"), TEXT("oyster"),
		TEXT("urchin"), TEXT("sea_kelp"), TEXT("sea_grapes"), TEXT("gem_topaz") };
	return Items.Contains(Item);
}

FVector2D WROcean::NodeBody(FName Item)
{
	if (Item == TEXT("tidecoral")) return FVector2D(170.f, 220.f);
	if (Item == TEXT("driftwood")) return FVector2D(200.f, 200.f);
	if (Item == TEXT("salt") || Item == TEXT("kelp_ash")) return FVector2D(220.f, 200.f);
	if (Item == TEXT("bronze_scrap")) return FVector2D(160.f, 230.f);
	if (Item == TEXT("gem_topaz")) return FVector2D(160.f, 220.f);
	return FVector2D(130.f, 220.f);
}

void WROcean::NodeLook(AActor* Node, FName Item, uint32 Seed)
{
	using namespace WROceanNodesImpl;
	if (!Node || Node->GetNetMode() == NM_DedicatedServer) return;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Node, TEXT("WROceanNode"));
	P->SetupAttachment(Node->GetRootComponent());
	P->SetUsingAbsoluteScale(true);
	P->SetWorldScale3D(FVector::OneVector);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->RegisterComponent();
	P->SetWorldLocation(Node->GetActorLocation());
	FRandomStream Rng(static_cast<int32>(Seed));
	FWRToolMesh M;
	if (Item == TEXT("tidecoral"))
	{
		FWRToolMesh Dead;
		Coral(M, Dead, Rng);
		M.ApplyTo(P);
		// Section 1: the same coral dead white (built into a second component's section, then moved over).
		UProceduralMeshComponent* Tmp = NewObject<UProceduralMeshComponent>(Node);
		Dead.ApplyTo(Tmp);
		if (const FProcMeshSection* Sec = Tmp->GetProcMeshSection(0)) { P->SetProcMeshSection(1, *Sec); P->SetMaterial(1, FWRToolMesh::Material()); }
		Tmp->DestroyComponent();
		P->SetMeshSectionVisible(1, false);
		UWROceanCoralLife* Life = NewObject<UWROceanCoralLife>(Node);
		Life->Mesh = P;
		Life->RegisterComponent();
		return;
	}
	if (Item == TEXT("sea_kelp"))
	{
		// A clump of young kelp: six to nine blades from a holdfast, swaying up.
		Blob(M, FVector(0, 0, 6.f), 16.f, S(0x5A, 0x4E, 0x1E), 0.5f);
		for (int32 b = 0; b < 7 + Rng.RandRange(0, 2); ++b)
		{
			const FVector Dir = FRotator(Rng.FRandRange(60.f, 85.f), Rng.FRandRange(0.f, 360.f), 0.f).Vector();
			const float L = Rng.FRandRange(90.f, 170.f);
			M.Lathe(FVector(0, 0, 8.f), Dir, L, { FVector(0.f, 2.f, 2.f), FVector(0.3f, 9.f, 1.5f), FVector(0.8f, 7.f, 1.2f), FVector(1.f, 1.f, 1.f) }, 4, S(0x8A, 0x7A, 0x2E), 0.45f, 18.f, float(b + 1));
		}
	}
	else if (Item == TEXT("sea_grapes"))
	{
		// Green bead-like seaweed: runners on the sand with upright stalks of tiny grapes.
		for (int32 r = 0; r < 4; ++r)
		{
			const FVector Dir = FRotator(0.f, r * 90.f + Rng.FRandRange(-20.f, 20.f), 0.f).Vector();
			Branch(M, FVector(0, 0, 2.f), Dir * 60.f + FVector(0, 0, 2.f), 2.f, 1.5f, S(0x6A, 0x8A, 0x3A));
			for (int32 s = 0; s < 4; ++s)
			{
				const FVector Base = Dir * (15.f + s * 12.f) + FVector(0, 0, 2.f);
				Branch(M, Base, Base + FVector(0, 0, 18.f + 8.f * Rng.FRand()), 1.2f, 1.f, S(0x5A, 0x8A, 0x3A));
				for (int32 g = 0; g < 6; ++g) Blob(M, Base + FVector(FMath::Cos(float(g)) * 2.5f, FMath::Sin(float(g)) * 2.5f, 6.f + g * 2.5f), 1.8f, S(0x7A, 0xC0, 0x4E, 0.7f), 1.f, 6);
			}
		}
	}
	else if (Item == TEXT("oyster"))
	{
		for (int32 o = 0; o < 9; ++o)
		{
			const FVector C(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f), 6.f + o * 2.f);
			M.Lathe(C - FVector(14.f, 0, 0), FRotator(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(0.f, 360.f), 0.f).Vector(), 30.f, { FVector(0.f, 2.f, 2.f), FVector(0.4f, 12.f, 4.f), FVector(1.f, 8.f, 3.f) }, 8, S(0xBD, 0xB6, 0xA6), 0.85f);
		}
	}
	else if (Item == TEXT("urchin"))
	{
		for (int32 u = 0; u < 5; ++u)
		{
			const FVector C(Rng.FRandRange(-35.f, 35.f), Rng.FRandRange(-35.f, 35.f), 7.f);
			Blob(M, C, 7.f, S(0x5A, 0x2E, 0x6A, 0.5f));
			for (int32 s = 0; s < 14; ++s) Branch(M, C, C + FRotator(Rng.FRandRange(-10.f, 80.f), s * 25.7f, 0.f).Vector() * 14.f, 1.f, 0.3f, S(0x5A, 0x2E, 0x6A, 0.6f));
		}
	}
	else if (Item == TEXT("salt"))
	{
		// A heap of raked salt crust on its pan, square-ish crystals glinting on it.
		M.Lathe(FVector(0, 0, -6.f), FVector::UpVector, 40.f, { FVector(0.f, 90.f, 70.f), FVector(0.6f, 50.f, 40.f), FVector(1.f, 10.f, 8.f) }, 12, S(0xF2, 0xEF, 0xE6, 0.78f), 0.7f);
		for (int32 c = 0; c < 10; ++c) Blob(M, FVector(Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-40.f, 40.f), 10.f + Rng.FRandRange(0.f, 20.f)), 5.f, S(0xFF, 0xFF, 0xFF, 0.85f), 0.6f, 4);
	}
	else if (Item == TEXT("kelp_ash"))
	{
		M.Lathe(FVector(0, 0, -6.f), FVector::UpVector, 26.f, { FVector(0.f, 80.f, 70.f), FVector(1.f, 50.f, 40.f) }, 10, S(0x24, 0x22, 0x20, 0.5f), 0.95f);
		for (int32 c = 0; c < 6; ++c) Blob(M, FVector(Rng.FRandRange(-45.f, 45.f), Rng.FRandRange(-40.f, 40.f), 18.f), 12.f, S(0xC8, 0xC4, 0xBC, 0.6f), 0.6f, 6);
	}
	else if (Item == TEXT("sea_glass"))
	{
		const FLinearColor Glass[4] = { S(0x7F, 0xD8, 0xD0, 0.7f), S(0xA8, 0xE0, 0x9A, 0.7f), S(0xE8, 0xE4, 0xD8, 0.8f), S(0x6A, 0x8A, 0xC8, 0.7f) };
		for (int32 g = 0; g < 12; ++g) Blob(M, FVector(Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(-60.f, 60.f), 2.f), Rng.FRandRange(2.f, 4.f), Glass[g % 4], 0.45f, 6);
	}
	else if (Item == TEXT("driftwood"))
	{
		const FVector Dir = FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector();
		M.Lathe(-Dir * 140.f + FVector(0, 0, 12.f), Dir, 280.f, { FVector(0.f, 16.f, 15.f), FVector(0.5f, 14.f, 13.f), FVector(1.f, 8.f, 8.f) }, 8, S(0xA8, 0xA3, 0x9A, 0.55f), 0.85f, 6.f, 3.f);
		for (int32 r = 0; r < 4; ++r) Branch(M, -Dir * 140.f + FVector(0, 0, 14.f), -Dir * (170.f + 20.f * r) + FRotator(r * 30.f - 45.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector() * 40.f, 6.f, 2.f, S(0x98, 0x93, 0x8A, 0.55f));
	}
	else if (Item == TEXT("bronze_scrap"))
	{
		// Verdigris crusting a bollard's foot: flaking green plates on the bronze, scrapeable.
		for (int32 f = 0; f < 10; ++f) M.Lathe(FVector(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(0.f, 120.f)), FVector::UpVector, 8.f, { FVector(0.f, 20.f, 14.f), FVector(1.f, 16.f, 10.f) }, 6, S(0x4E, 0x8A, 0x72, 0.55f), 0.8f);
	}
	else if (Item == TEXT("gem_topaz"))
	{
		// A cave pocket of amber crystals in the dark rock.
		M.Lathe(FVector(0, 0, -10.f), FVector::UpVector, 60.f, { FVector(0.f, 70.f, 60.f), FVector(1.f, 40.f, 34.f) }, 9, S(0x3A, 0x36, 0x32, 0.5f), 0.9f);
		for (int32 c = 0; c < 9; ++c) M.Lathe(FVector(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(-30.f, 30.f), 30.f), FRotator(Rng.FRandRange(40.f, 90.f), Rng.FRandRange(0.f, 360.f), 0.f).Vector(), Rng.FRandRange(20.f, 45.f), { FVector(0.f, 5.f, 5.f), FVector(0.8f, 5.f, 5.f), FVector(1.f, 0.5f, 0.5f) }, 6, S(0xE8, 0xA8, 0x40, 0.8f), 0.15f);
	}
	M.ApplyTo(P);
}

bool WROcean::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Comp)
{
	using namespace WROceanNodesImpl;
	if (!Comp) return false;
	FWRToolMesh M;
	if (ModelPath == TEXT("code:harpoon") || ModelPath == TEXT("code:harpoon_chain"))
	{
		const bool bChain = ModelPath == TEXT("code:harpoon_chain");
		// Ash shaft 2 m (its tip at the actor), a bronze toggle head with its barb, a coil of line or chain at the butt.
		M.Lathe(FVector(-200.f, 0, 0), FVector(1, 0, 0), 175.f, { FVector(0.f, 2.6f, 2.6f), FVector(1.f, 2.2f, 2.2f) }, 8, S(0x9C, 0x84, 0x60), 0.7f);
		M.Lathe(FVector(-25.f, 0, 0), FVector(1, 0, 0), 25.f, { FVector(0.f, 3.f, 3.f), FVector(0.6f, 4.f, 2.f), FVector(1.f, 0.4f, 0.4f) }, 6, bChain ? S(0x3A, 0x34, 0x30, 0.55f) : S(0x8C, 0x6A, 0x3A), 0.4f);
		Branch(M, FVector(-14.f, 0, 0), FVector(-30.f, 0, 9.f), 1.6f, 0.4f, bChain ? S(0x3A, 0x34, 0x30, 0.55f) : S(0x8C, 0x6A, 0x3A));
		for (int32 r = 0; r < 4; ++r) M.Lathe(FVector(-195.f + r * 3.f, 0, 0), FVector(1, 0, 0), 3.f, { FVector(0.f, 4.5f, 4.5f), FVector(1.f, 4.5f, 4.5f) }, 8, bChain ? S(0x3A, 0x34, 0x30, 0.55f) : S(0x9C, 0x84, 0x60, 0.5f), 0.9f);
	}
	else if (ModelPath == TEXT("code:spout"))
	{
		// A ball of thrown sea-water (Thalassor's spout): foam-white over blue, a wake behind.
		Blob(M, FVector::ZeroVector, 40.f, S(0x9E, 0xD8, 0xE0, 0.7f), 1.f, 10);
		Blob(M, FVector(-30.f, 0, 0), 26.f, S(0xF0, 0xF4, 0xF4, 0.8f), 0.8f, 8);
	}
	else if (ModelPath == TEXT("code:barrel"))
	{
		// A barnacled cask out of Old Barnacle's hull (hoops, staves).
		M.Lathe(FVector(-50.f, 0, 0), FVector(1, 0, 0), 100.f, { FVector(0.f, 30.f, 30.f), FVector(0.5f, 38.f, 38.f), FVector(1.f, 30.f, 30.f) }, 12, S(0x6E, 0x54, 0x3A, 0.5f), 0.85f);
		for (int32 h = 0; h < 3; ++h) M.Lathe(FVector(-42.f + h * 40.f, 0, 0), FVector(1, 0, 0), 6.f, { FVector(0.f, 34.f + (h == 1 ? 4.f : 0.f), 34.f + (h == 1 ? 4.f : 0.f)), FVector(1.f, 34.f + (h == 1 ? 4.f : 0.f), 34.f + (h == 1 ? 4.f : 0.f)) }, 12, S(0x3A, 0x30, 0x28, 0.5f), 0.6f);
		for (int32 b = 0; b < 6; ++b) Blob(M, FVector(-20.f + b * 8.f, FMath::Cos(float(b)) * 36.f, FMath::Sin(float(b)) * 36.f), 5.f, S(0xD8, 0xD0, 0xBE), 0.6f, 6);
	}
	else if (ModelPath == TEXT("code:anchor"))
	{
		M.Lathe(FVector(-80.f, 0, 0), FVector(1, 0, 0), 160.f, { FVector(0.f, 8.f, 8.f), FVector(1.f, 8.f, 8.f) }, 8, S(0x6A, 0x3A, 0x22, 0.5f), 0.85f);
		Branch(M, FVector(80.f, 0, 0), FVector(40.f, 60.f, 0.f), 7.f, 4.f, S(0x6A, 0x3A, 0x22, 0.5f));
		Branch(M, FVector(80.f, 0, 0), FVector(40.f, -60.f, 0.f), 7.f, 4.f, S(0x6A, 0x3A, 0x22, 0.5f));
		Branch(M, FVector(-70.f, 0, -50.f), FVector(-70.f, 0, 50.f), 5.f, 5.f, S(0x6A, 0x3A, 0x22, 0.5f));
	}
	else if (ModelPath == TEXT("code:sunflare"))
	{
		// The Sunstone flare: a glass bulb round a sunstone, kelp-ash fuse.
		Blob(M, FVector::ZeroVector, 8.f, S(0xE8, 0xE4, 0xC8, 0.7f), 1.f, 10);
		Blob(M, FVector::ZeroVector, 5.f, S(0xFF, 0xC8, 0x40, 1.f), 1.f, 8);
		Branch(M, FVector(-8.f, 0, 0), FVector(-14.f, 0, 4.f), 1.f, 0.6f, S(0x24, 0x22, 0x20, 0.5f));
	}
	else return false;
	M.ApplyTo(Comp);
	return true;
}
