// The Desert's gathering nodes' looks (cooks/biome-desert.md 2.7; cooks/build-biome-desert.md 2.5), modelled in code on
// M_HeldTool like the tools: sunstone (amber crystals with copper glitter in sun-facing rock; at night they give back
// the day's stored light), sunglass (gold-green lumps of desert glass in the sand), rock salt (white cubes in a crust),
// nitre (white crust on red rock), clay and red ochre (cut banks of the wadi), gold flakes (glints in a gravel bar), the
// umbrella thorns (acacia logs), the date palms (dates and palm fibre) and prickly pear. WRResourceLook asks here first.
#include "WRDesertTypes.h"
#include "WRDesertWorld.h"
#include "Wildreach.h"
#include "WRGameState.h"
#include "WRToolMesh.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace WRDesertNodesImpl
{
	FLinearColor DnS(uint8 R, uint8 G, uint8 B, float K = 0.6f) { return FLinearColor(FColor(R, G, B)) * K; }
	void DnBranch(FWRToolMesh& M, const FVector& A, const FVector& B, float R0, float R1, const FLinearColor& C, int32 Sides = 6)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f) return;
		M.Lathe(A, D / L, L, { FVector(0.f, R0, R0), FVector(1.f, R1, R1) }, Sides, C, 0.7f);
	}
	void DnBlob(FWRToolMesh& M, const FVector& C, float R, const FLinearColor& Col, float Squash = 1.f, int32 Sides = 8, float Rough = 0.7f)
	{
		TArray<FVector> P;
		for (int32 i = 0; i <= 5; ++i) { const float A = PI * i / 5.f; P.Add(FVector(i / 5.f, FMath::Max(0.5f, R * FMath::Sin(A)), FMath::Max(0.5f, R * FMath::Sin(A)))); }
		M.Lathe(C - FVector(0, 0, R * Squash), FVector::UpVector, 2.f * R * Squash, P, Sides, Col, Rough);
	}
	void DnCrystal(FWRToolMesh& M, const FVector& Root, const FVector& Dir, float Len, float R, const FLinearColor& C, float Rough = 0.15f)
	{
		M.Lathe(Root, Dir, Len, { FVector(0.f, R, R), FVector(0.75f, R, R), FVector(1.f, 0.4f, 0.4f) }, 6, C, Rough);
	}
	void DnRockBed(FWRToolMesh& M, float R, float H, const FLinearColor& C, FRandomStream& Rng)
	{
		M.Lathe(FVector(0, 0, -20.f), FVector::UpVector, H, { FVector(0.f, R, R * 0.85f), FVector(0.5f, R * 0.85f, R * 0.7f), FVector(1.f, R * 0.4f, R * 0.35f) }, 9, C, 0.9f, 0.f, float(Rng.RandRange(1, 50)));
	}
}

void UWRDesertNodeGlow::TickComponent(float Dt, ELevelTick T, FActorComponentTickFunction* F)
{
	Super::TickComponent(Dt, T, F);
	AActor* O = GetOwner();
	if (!O) return;
	// The stored light (cook 2.7: brightness = hours of sun that day / 14) shows from dusk; off by day.
	const bool bNight = AWRGameState::IsNightIn(O);
	const float Stored = FMath::Clamp(UWRDesertWorld::SunHoursToday(O) / 14.f, 0.f, 1.f);
	const float Want = bNight ? Power * Stored : 0.f;
	if (UPointLightComponent* L = Light.Get()) L->SetIntensity(Want);
	if (UProceduralMeshComponent* G = Glow.Get()) G->SetVisibility(bNight && Stored > 0.05f);
}

bool WRDesert::IsNodeItem(FName Item)
{
	static const TSet<FName> Items = { TEXT("sunstone"), TEXT("sunglass"), TEXT("rock_salt"), TEXT("nitre"), TEXT("clay"), TEXT("red_ochre"), TEXT("gold_flakes"),
		TEXT("acacia_log"), TEXT("dates"), TEXT("prickly_pear"), TEXT("bat_guano") };
	return Items.Contains(Item);
}

FVector2D WRDesert::NodeBody(FName Item)
{
	if (Item == TEXT("acacia_log")) return FVector2D(90.f, 500.f);
	if (Item == TEXT("dates")) return FVector2D(80.f, 600.f);
	if (Item == TEXT("prickly_pear")) return FVector2D(150.f, 170.f);
	if (Item == TEXT("sunstone") || Item == TEXT("sunglass")) return FVector2D(180.f, 160.f);
	if (Item == TEXT("rock_salt") || Item == TEXT("nitre")) return FVector2D(200.f, 140.f);
	if (Item == TEXT("clay") || Item == TEXT("red_ochre")) return FVector2D(200.f, 150.f);
	if (Item == TEXT("gold_flakes")) return FVector2D(200.f, 80.f);
	return FVector2D(150.f, 160.f);
}

void WRDesert::NodeLook(AActor* Node, FName Item, uint32 Seed)
{
	using namespace WRDesertNodesImpl;
	if (!Node || Node->GetNetMode() == NM_DedicatedServer) return;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Node, TEXT("WRDesertNode"));
	P->SetupAttachment(Node->GetRootComponent());
	P->SetUsingAbsoluteScale(true);
	P->SetWorldScale3D(FVector::OneVector);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->RegisterComponent();
	P->SetWorldLocation(Node->GetActorLocation());
	FRandomStream Rng(static_cast<int32>(Seed));
	FWRToolMesh M;
	if (Item == TEXT("sunstone"))
	{
		// Amber feldspar crystals (real aventurescent sunstone) with copper-glitter flecks, out of a red rock knuckle.
		DnRockBed(M, 90.f, 70.f, DnS(0xA8, 0x46, 0x2E, 0.5f), Rng);
		for (int32 c = 0; c < 9; ++c)
		{
			const FVector Root(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f), 30.f + Rng.FRandRange(0.f, 20.f));
			const FVector Dir = FRotator(Rng.FRandRange(35.f, 85.f), Rng.FRandRange(0.f, 360.f), 0.f).Vector();
			DnCrystal(M, Root, Dir, Rng.FRandRange(30.f, 70.f), Rng.FRandRange(6.f, 12.f), DnS(0xF2, 0xA2, 0x3A, 0.75f));
			DnBlob(M, Root + Dir * 12.f, 1.6f, DnS(0xD8, 0x70, 0x30, 0.9f), 1.f, 4, 0.2f);    // copper glitter
		}
		// The stored-light glow: a second mesh of the same crystals a shade brighter (shown at night), a small light.
		FWRToolMesh Gm;
		for (int32 c = 0; c < 5; ++c) DnBlob(Gm, FVector(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(-30.f, 30.f), 60.f), 9.f, DnS(0xFF, 0xC0, 0x60, 1.2f), 1.f, 6, 0.1f);
		UProceduralMeshComponent* G = NewObject<UProceduralMeshComponent>(Node, TEXT("WRDesertGlow"));
		G->SetupAttachment(P);
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->RegisterComponent();
		Gm.ApplyTo(G);
		G->SetVisibility(false);
		UPointLightComponent* L = NewObject<UPointLightComponent>(Node);
		L->SetupAttachment(P);
		L->SetRelativeLocation(FVector(0, 0, 90.f));
		L->SetLightColor(FLinearColor(FColor(0xF2, 0xA2, 0x3A)));
		L->SetAttenuationRadius(500.f);
		L->SetCastShadows(false);
		L->SetIntensity(0.f);
		L->RegisterComponent();
		UWRDesertNodeGlow* Gl = NewObject<UWRDesertNodeGlow>(Node);
		Gl->Light = L;
		Gl->Glow = G;
		Gl->RegisterComponent();
	}
	else if (Item == TEXT("sunglass"))
	{
		// Gold-green translucent lumps of desert glass half in the sand (Libyan desert glass), a fulgurite tube or two.
		for (int32 g = 0; g < 6; ++g) DnBlob(M, FVector(Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f), 4.f), Rng.FRandRange(10.f, 22.f), DnS(0xC8, 0xC0, 0x60, 0.65f), 0.6f, 7, 0.08f);
		for (int32 t = 0; t < 2; ++t) DnBranch(M, FVector(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f), -10.f), FVector(Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(-60.f, 60.f), 60.f + Rng.FRandRange(0.f, 40.f)), 6.f, 3.f, DnS(0x2A, 0x36, 0x24, 0.6f));
	}
	else if (Item == TEXT("rock_salt"))
	{
		// White cubes heaped in a salt crust (the Pan's cakes, the sabkha's).
		M.Lathe(FVector(0, 0, -6.f), FVector::UpVector, 12.f, { FVector(0.f, 100.f, 90.f), FVector(1.f, 90.f, 80.f) }, 10, DnS(0xF2, 0xEF, 0xE6, 0.75f), 0.6f);
		for (int32 c = 0; c < 12; ++c)
		{
			const FVector At(Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(-60.f, 60.f), 10.f + Rng.FRandRange(0.f, 30.f));
			const float S = Rng.FRandRange(10.f, 24.f);
			M.Lathe(At - FVector(0, 0, S * 0.5f), FRotator(Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(0.f, 90.f), 0.f).RotateVector(FVector::UpVector), S, { FVector(0.f, S * 0.7f, S * 0.7f), FVector(1.f, S * 0.7f, S * 0.7f) }, 4, DnS(0xF4, 0xF2, 0xEC, 0.8f), 0.5f);
		}
	}
	else if (Item == TEXT("nitre"))
	{
		// White saltpetre crust flowering on a red rock face.
		DnRockBed(M, 100.f, 90.f, DnS(0xA8, 0x46, 0x2E, 0.5f), Rng);
		for (int32 f = 0; f < 14; ++f) DnBlob(M, FVector(Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(-60.f, 60.f), Rng.FRandRange(20.f, 70.f)), Rng.FRandRange(6.f, 16.f), DnS(0xE8, 0xE4, 0xDC, 0.75f), 0.4f, 6, 0.8f);
	}
	else if (Item == TEXT("clay") || Item == TEXT("red_ochre"))
	{
		// A cut bank: layered earth, red-brown clay or bright red ochre seams.
		const bool bOchre = Item == TEXT("red_ochre");
		for (int32 l = 0; l < 4; ++l)
		{
			const FLinearColor C = l % 2 ? (bOchre ? DnS(0xB8, 0x3A, 0x1E, 0.6f) : DnS(0xA8, 0x6A, 0x46, 0.55f)) : DnS(0xC8, 0xA0, 0x70, 0.5f);
			M.Lathe(FVector(0, 0, l * 22.f - 10.f), FVector::UpVector, 22.f, { FVector(0.f, 100.f - l * 14.f, 80.f - l * 12.f), FVector(1.f, 92.f - l * 14.f, 74.f - l * 12.f) }, 10, C, 0.9f, 0.f, float(l + 1));
		}
	}
	else if (Item == TEXT("gold_flakes"))
	{
		// A gravel bar in the wadi bed with glints of gold in it.
		M.Lathe(FVector(0, 0, -8.f), FVector::UpVector, 20.f, { FVector(0.f, 110.f, 80.f), FVector(1.f, 100.f, 70.f) }, 10, DnS(0xA8, 0x96, 0x7A, 0.55f), 0.9f);
		for (int32 g = 0; g < 30; ++g) DnBlob(M, FVector(Rng.FRandRange(-90.f, 90.f), Rng.FRandRange(-60.f, 60.f), 12.f), Rng.FRandRange(3.f, 7.f), g % 5 == 0 ? DnS(0xE8, 0xC0, 0x40, 0.9f) : DnS(0x7A, 0x6A, 0x58, 0.55f), 0.6f, 5, g % 5 == 0 ? 0.2f : 0.9f);
	}
	else if (Item == TEXT("acacia_log"))
	{
		// An umbrella thorn (Vachellia tortilis): forked trunks, a flat crown of fine leaves, white thorns.
		const float H = Rng.FRandRange(520.f, 700.f);
		TArray<FVector> Tips;
		for (int32 t = 0; t < 2; ++t)
		{
			const FVector Out = FRotator(0.f, t * 160.f + Rng.FRandRange(0.f, 40.f), 0.f).Vector();
			const FVector Mid = Out * 50.f + FVector(0, 0, H * 0.4f);
			DnBranch(M, FVector(0, 0, -20.f), Mid, 18.f, 12.f, DnS(0x4A, 0x3E, 0x34, 0.55f), 7);
			for (int32 b = 0; b < 3; ++b) { const FVector Tip = Mid + FRotator(0.f, (b - 1) * 50.f, 0.f).RotateVector(Out) * 180.f + FVector(0, 0, H * 0.45f); DnBranch(M, Mid, Tip, 9.f, 4.f, DnS(0x4A, 0x3E, 0x34, 0.6f), 5); Tips.Add(Tip); }
		}
		for (const FVector& T : Tips) DnBlob(M, T + FVector(0, 0, 20.f), 170.f, DnS(0x6E, 0x86, 0x46, 0.48f), 0.22f, 10, 0.95f);
		for (int32 th = 0; th < 10; ++th) DnBranch(M, FVector(0, 0, 60.f + th * 20.f), FVector(FMath::Cos(float(th)) * 14.f, FMath::Sin(float(th)) * 14.f, 64.f + th * 20.f), 0.8f, 0.2f, DnS(0xE8, 0xE0, 0xD0, 0.7f), 3);
	}
	else if (Item == TEXT("dates"))
	{
		// A young date palm (5-6 m): a scarred trunk, a crown of arching fronds, two bunches of orange dates.
		const float H = Rng.FRandRange(480.f, 620.f);
		TArray<FVector> Tr;
		for (int32 i = 0; i <= 5; ++i) Tr.Add(FVector(Rng.FRandRange(-3.f, 3.f) + i * 6.f, 0.f, -20.f + i * H / 5.f));
		M.Tube(Tr, 18.f, 8, DnS(0x7A, 0x62, 0x48, 0.5f), 0.92f);
		for (int32 r = 1; r < 12; ++r) for (int32 s = 0; s < 4; ++s) { const float A = (r * 45.f + s * 90.f) * PI / 180.f; DnBranch(M, FMath::Lerp(Tr[0], Tr[5], r / 12.f), FMath::Lerp(Tr[0], Tr[5], r / 12.f) + FVector(FMath::Cos(A), FMath::Sin(A), 0.3f) * 16.f, 6.f, 2.f, DnS(0x8A, 0x70, 0x52, 0.5f), 4); }
		const FVector Crown = Tr.Last();
		for (int32 f = 0; f < 16; ++f)
		{
			const float Az = (f * 137.5f) * PI / 180.f, Rise = f < 3 ? -60.f : Rng.FRandRange(-10.f, 50.f);
			TArray<FVector> Ra;
			for (int32 k = 0; k <= 4; ++k) { const float T = k / 4.f, El = FMath::DegreesToRadians(Rise - 80.f * T * T); Ra.Add(Crown + FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El)) * 280.f * T); }
			M.Tube(Ra, 3.f, 4, f < 3 ? DnS(0xA8, 0x90, 0x5A, 0.5f) : DnS(0x5E, 0x7E, 0x3A, 0.5f), 0.8f);
			for (int32 k = 1; k < 4; ++k) for (int32 sd = -1; sd <= 1; sd += 2) { const FVector Sd = FVector::CrossProduct((Ra[4] - Ra[0]).GetSafeNormal(), FVector::UpVector).GetSafeNormal(); DnBranch(M, Ra[k], Ra[k] + (Sd * sd + FVector(0, 0, -0.4f)).GetSafeNormal() * 45.f, 2.5f, 0.3f, DnS(0x5E, 0x7E, 0x3A, 0.52f), 3); }
		}
		for (int32 d = 0; d < 2; ++d) DnBlob(M, Crown + FVector(d ? 30.f : -30.f, d ? -20.f : 20.f, -50.f), 22.f, DnS(0xC0, 0x5A, 0x22, 0.6f), 1.5f, 8, 0.6f);
	}
	else if (Item == TEXT("prickly_pear"))
	{
		// A prickly pear clump: flat pads growing out of each other, magenta fruit on the top rims.
		struct FPad { FVector At; FRotator R; int32 Gen; };
		TArray<FPad> Pads = { { FVector(0, 0, 25.f), FRotator(0.f, Rng.FRandRange(0.f, 180.f), 0.f), 0 } };
		for (int32 i = 0; i < Pads.Num() && Pads.Num() < 10; ++i)
		{
			if (Pads[i].Gen >= 2) continue;
			for (int32 c = 0; c < 2; ++c)
			{
				const FRotator R = Pads[i].R + FRotator((c ? 1.f : -1.f) * Rng.FRandRange(15.f, 35.f), Rng.FRandRange(-40.f, 40.f), 0.f);
				Pads.Add({ Pads[i].At + R.RotateVector(FVector(0, 0, 46.f)), R, Pads[i].Gen + 1 });
			}
		}
		for (const FPad& Pd : Pads)
		{
			M.Lathe(Pd.At - Pd.R.RotateVector(FVector(0, 0, 24.f)), Pd.R.RotateVector(FVector(0, 0, 1)), 48.f, { FVector(0.f, 3.f, 2.f), FVector(0.3f, 20.f, 4.f), FVector(0.7f, 20.f, 4.f), FVector(1.f, 3.f, 2.f) }, 8, DnS(0x6E, 0x96, 0x58, 0.5f), 0.7f);
			if (Pd.Gen == 2) for (int32 f = 0; f < 3; ++f) DnBlob(M, Pd.At + Pd.R.RotateVector(FVector((f - 1) * 10.f, 0.f, 26.f)), 6.f, DnS(0xBE, 0x1E, 0x6E, 0.6f), 1.2f, 6, 0.5f);
		}
	}
	else if (Item == TEXT("bat_guano"))
	{
		M.Lathe(FVector(0, 0, -4.f), FVector::UpVector, 30.f, { FVector(0.f, 90.f, 70.f), FVector(1.f, 40.f, 30.f) }, 10, DnS(0x3A, 0x30, 0x26, 0.5f), 0.95f);
	}
	M.ApplyTo(P);
}

bool WRDesert::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Comp)
{
	using namespace WRDesertNodesImpl;
	if (!Comp) return false;
	FWRToolMesh M;
	if (ModelPath == TEXT("code:oasis_jar") || ModelPath == TEXT("code:ochre_beer"))
	{
		// A round clay water jar with a narrow neck and a palm-fibre stopper (the beer's is stained red at the lip).
		const bool bBeer = ModelPath == TEXT("code:ochre_beer");
		M.Lathe(FVector(-18.f, 0, 0), FVector(1, 0, 0), 36.f, { FVector(0.f, 6.f, 6.f), FVector(0.2f, 16.f, 16.f), FVector(0.6f, 17.f, 17.f), FVector(0.85f, 7.f, 7.f), FVector(1.f, 6.f, 6.f) }, 12, DnS(0xA8, 0x6A, 0x46, 0.55f), 0.85f);
		DnBlob(M, FVector(19.f, 0, 0), 5.f, bBeer ? DnS(0x9A, 0x1E, 0x14, 0.6f) : DnS(0x9C, 0x84, 0x60, 0.5f), 0.7f, 6, 0.9f);
	}
	else if (ModelPath == TEXT("code:flash_pot"))
	{
		// A small clay pot sealed with wax, a sunstone shard set in the seal, a fuse of palm fibre.
		DnBlob(M, FVector::ZeroVector, 9.f, DnS(0x8A, 0x5A, 0x3E, 0.55f), 1.f, 10);
		DnCrystal(M, FVector(6.f, 0, 2.f), FVector(1, 0, 0.4f).GetSafeNormal(), 8.f, 2.5f, DnS(0xF2, 0xA2, 0x3A, 0.9f));
		DnBranch(M, FVector(-8.f, 0, 2.f), FVector(-15.f, 0, 6.f), 0.8f, 0.6f, DnS(0x9C, 0x84, 0x60, 0.5f), 4);
	}
	else if (ModelPath == TEXT("code:glassfall"))
	{
		// A glob of molten glass (Ahrumet's): a glowing orange core in a skin cooling green-black.
		DnBlob(M, FVector::ZeroVector, 40.f, DnS(0xFF, 0x8A, 0x2A, 1.2f), 1.f, 10, 0.2f);
		DnBlob(M, FVector(-20.f, 0, 0), 30.f, DnS(0x2A, 0x3A, 0x22, 0.7f), 0.8f, 8, 0.1f);
	}
	else if (ModelPath == TEXT("code:kite_bone"))
	{
		// A long bone a Carrion kite drops to break it (or on you).
		M.Lathe(FVector(-40.f, 0, 0), FVector(1, 0, 0), 80.f, { FVector(0.f, 7.f, 6.f), FVector(0.15f, 4.f, 4.f), FVector(0.85f, 4.f, 4.f), FVector(1.f, 7.f, 6.f) }, 8, DnS(0xE8, 0xE0, 0xCC, 0.7f), 0.7f);
	}
	else if (ModelPath == TEXT("code:sand_ball"))
	{
		// A Scarab digger's ball of packed sand, bigger than itself.
		DnBlob(M, FVector::ZeroVector, 45.f, DnS(0xC8, 0x98, 0x58, 0.55f), 1.f, 12, 0.95f);
	}
	else return false;
	M.ApplyTo(Comp);
	return true;
}
