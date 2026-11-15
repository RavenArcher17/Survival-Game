// The Fungal bosses' looks (see WRFungalBosses.h): the arena things (a wall egg sac, a rot pool, a Draught gate's
// fungal plug, a mound pod, a web sheet) and each boss's parts built in code over its borrowed rig; Mycelorn is all code
// (cooks/biome-fungal.md 2.13-2.14: anatomy, palette, silhouette). Glowing parts use M_FungalGlow (vertex alpha = glow),
// see-through ones M_FungalVeil, the rest M_HeldTool (vertex colour, alpha = roughness).

#include "WRFungalBosses.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "WRBoss.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace WRFungalBossLookImpl
{
	FLinearColor FlRgb(int32 R, int32 G, int32 B) { return FLinearColor(FColor(uint8(R), uint8(G), uint8(B))); }
	const FLinearColor FlCyan = FlRgb(57, 230, 194), FlUmber = FlRgb(58, 42, 30), FlAmber = FlRgb(232, 160, 64), FlTendril = FlRgb(26, 23, 20), FlCream = FlRgb(240, 230, 208);
	const FLinearColor FlIvory = FlRgb(216, 203, 176), FlBone = FlRgb(232, 226, 208), FlRust = FlRgb(138, 74, 42), FlBronze = FlRgb(140, 100, 50);
	TArray<TWeakObjectPtr<AWRFungalBossProp>>& FlProps() { static TArray<TWeakObjectPtr<AWRFungalBossProp>> L; return L; }
	/** A sphere-ish lump (a lathe along +Z from Base). */
	void FlBlob(FWRToolMesh& M, const FVector& Base, float R, float H, const FLinearColor& C, float A, int32 Sides = 12, float Wobble = 0.f, float Seed = 0.f)
	{
		TArray<FVector> Prof;
		for (int32 k = 0; k <= 8; ++k) { const float T = k / 8.f; const float S = FMath::Sin(T * PI) * R; Prof.Add(FVector(T, S, S)); }
		M.Lathe(Base, FVector::UpVector, H, Prof, Sides, C, A, Wobble, Seed);
	}
	/** A cap on a stalk (Up from At). */
	void FlCap(FWRToolMesh& M, const FVector& At, const FVector& Up, float Stem, float StemR, float R, float H, const FLinearColor& StemC, const FLinearColor& CapC, float A)
	{
		M.Lathe(At, Up, Stem, { FVector(0.f, StemR, StemR), FVector(1.f, StemR * 0.8f, StemR * 0.8f) }, 6, StemC, A);
		M.Lathe(At + Up * Stem, Up, H, { FVector(0.f, StemR, StemR), FVector(0.08f, R, R), FVector(0.5f, R * 0.85f, R * 0.85f), FVector(1.f, 0.f, 0.f) }, 10, CapC, A);
	}
	/** A hanging cord from Top down Len (a tendril), with a little sway baked in by Seed. */
	void FlCord(FWRToolMesh& M, const FVector& Top, float Len, float R, const FLinearColor& C, float A, float Seed)
	{
		TArray<FVector> Pts;
		for (int32 k = 0; k <= 8; ++k) { const float T = k / 8.f; Pts.Add(Top + FVector(FMath::Sin(T * 3.f + Seed) * Len * 0.04f, FMath::Cos(T * 2.3f + Seed) * Len * 0.04f, -T * Len)); }
		M.Tube(Pts, R, 6, C, A);
	}
	USceneComponent* FlMake(AActor* Owner, USceneComponent* Parent, FWRToolMesh& M, UMaterialInterface* Mat)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(Owner);
		P->SetupAttachment(Parent);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->RegisterComponent();
		M.ApplyTo(P);
		if (Mat) P->SetMaterial(0, Mat);
		return P;
	}
}
using namespace WRFungalBossLookImpl;

// ================================================================================================ AWRFungalBossProp

AWRFungalBossProp::AWRFungalBossProp()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	HitShape = CreateDefaultSubobject<USphereComponent>(TEXT("HitShape"));
	HitShape->SetupAttachment(RootComponent);
	HitShape->InitSphereRadius(80.f);
	HitShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitShape->SetCollisionObjectType(ECC_WorldDynamic);
	HitShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitShape->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	HitShape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitShape->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HitShape->SetGenerateOverlapEvents(false);
	HitShape->SetCanEverAffectNavigation(false);
}

void AWRFungalBossProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRFungalBossProp, Kind);
	DOREPLIFETIME(AWRFungalBossProp, State);
	DOREPLIFETIME(AWRFungalBossProp, Radius);
	DOREPLIFETIME(AWRFungalBossProp, StateAt);
	DOREPLIFETIME(AWRFungalBossProp, Index);
}

TArray<TWeakObjectPtr<AWRFungalBossProp>>& AWRFungalBossProp::All()
{
	FlProps().RemoveAll([](const TWeakObjectPtr<AWRFungalBossProp>& P) { return !P.IsValid(); });
	return FlProps();
}

void AWRFungalBossProp::BeginPlay()
{
	Super::BeginPlay();
	FlProps().Add(this);
	OnRep_Look();
}

void AWRFungalBossProp::EndPlay(const EEndPlayReason::Type Reason)
{
	FlProps().Remove(this);
	Super::EndPlay(Reason);
}

float AWRFungalBossProp::ServerNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? float(GS->GetServerWorldTimeSeconds()) : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

void AWRFungalBossProp::SetState(uint8 NewState)
{
	if (State == NewState) return;
	State = NewState;
	StateAt = ServerNow();
	ForceNetUpdate();
	OnRep_Look();
}

bool AWRFungalBossProp::Contains(const FVector& P, float Grow) const
{
	return FVector::Dist2D(P, GetActorLocation()) < Radius + Grow && FMath::Abs(P.Z - GetActorLocation().Z) < 200.f;
}

void AWRFungalBossProp::OnRep_Look()
{
	// The blow-catching shape follows what it is and whether it stands.
	const bool bHittable = State == 0 && Kind != TEXT("pool");
	HitShape->SetCollisionEnabled(bHittable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (Kind == TEXT("web")) { HitShape->SetSphereRadius(FMath::Max(150.f, Radius * 0.35f)); HitShape->SetRelativeLocation(FVector(0.f, 0.f, 300.f)); }
	else if (Kind == TEXT("plug")) { HitShape->SetSphereRadius(Radius); HitShape->SetRelativeLocation(FVector(0.f, 0.f, Radius)); }
	else if (Kind == TEXT("pod")) { HitShape->SetSphereRadius(110.f); HitShape->SetRelativeLocation(FVector(0.f, 0.f, 100.f)); }
	else HitShape->SetSphereRadius(FMath::Max(60.f, Radius));
	if (GetNetMode() == NM_DedicatedServer) return;
	if (BuiltKind == Kind && BuiltState == State && FMath::IsNearlyEqual(BuiltRadius, Radius, 1.f)) return;
	Build();
}

void AWRFungalBossProp::Build()
{
	if (Mesh) { Mesh->DestroyComponent(); Mesh = nullptr; }
	if (Glow) { Glow->DestroyComponent(); Glow = nullptr; }
	if (Lamp) { Lamp->DestroyComponent(); Lamp = nullptr; }
	BuiltKind = Kind;
	BuiltState = State;
	BuiltRadius = Radius;
	FRandomStream Rng(int32(GetTypeHash(GetName())));
	FWRToolMesh M, G, V;
	bool bVeil = false;
	if (Kind == TEXT("sac"))
	{
		if (State == 0)
		{
			// A puffball-like pod 1.5 m across, glowing cyan, held to the wall by silk and mycelium strands.
			FlBlob(G, FVector(0.f, 0.f, -Radius), Radius, Radius * 2.f, FlCyan * 0.85f, 0.65f, 14, 0.f, 1.f);
			for (int32 i = 0; i < 6; ++i) FlBlob(G, FVector(Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(-30.f, 30.f), Rng.FRandRange(-40.f, 20.f)), 14.f, 28.f, FlCyan, 1.f, 8);
			for (int32 i = 0; i < 6; ++i) M.Tube({ FVector(0.f, 0.f, 0.f), FRotator(Rng.FRandRange(-40.f, 40.f), i * 60.f, 0.f).Vector() * Radius * 1.4f + FVector(-Radius, 0.f, 0.f) }, 1.6f, 4, FlIvory * 0.6f, 0.4f);
		}
		else FlBlob(M, FVector(0.f, 0.f, -Radius * 0.5f), Radius * 0.5f, Radius, FlRgb(30, 26, 22) * 0.5f, 0.95f, 10, 4.f, 2.f);
	}
	else if (Kind == TEXT("pool"))
	{
		// A rot pool: rust-red slurry flat in its basin (frozen: pale ice); a lip of slick around it.
		if (State != 1)
		{
			const FLinearColor C = State == 2 ? FlRgb(200, 225, 240) * 0.7f : FlRust * 0.55f;
			TArray<TArray<FVector>> Rings;
			for (const float Z : { 0.f, 4.f })
			{
				TArray<FVector> Ring;
				for (int32 k = 0; k < 28; ++k) { const float A = k * 2.f * PI / 28.f; const float Rr = Radius * (1.f + 0.05f * FMath::Sin(A * 5.f + Rng.FRand() * 3.f)); Ring.Add(FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z)); }
				Rings.Add(Ring);
			}
			M.Loft(Rings, C, State == 2 ? 0.1f : 0.15f, true);
			if (State == 0) for (int32 i = 0; i < 8; ++i) FlBlob(M, FVector(FMath::Cos(i * 0.8f) * Radius * Rng.FRandRange(0.2f, 0.8f), FMath::Sin(i * 0.8f) * Radius * Rng.FRandRange(0.2f, 0.8f), 2.f), Rng.FRandRange(6.f, 14.f), 8.f, FlRust * 0.7f, 0.1f, 8);
		}
	}
	else if (Kind == TEXT("plug"))
	{
		// A fungal plug 6 m across sealing the gate's stone doorway: lumpy cream-tan flesh with glowing veins; broken,
		// only torn rim and the draught's way.
		const float R = Radius;
		if (State == 0)
		{
			for (int32 i = 0; i < 9; ++i)
			{
				const FVector C(0.f, Rng.FRandRange(-R * 0.7f, R * 0.7f), Rng.FRandRange(R * 0.3f, R * 1.7f));
				const float Rr = Rng.FRandRange(R * 0.35f, R * 0.6f);
				M.Lathe(C - FVector(Rr * 0.3f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), Rr * 0.6f, { FVector(0.f, Rr, Rr), FVector(0.5f, Rr * 1.05f, Rr), FVector(1.f, Rr * 0.6f, Rr * 0.6f) }, 10, FlRgb(196, 170, 130) * 0.6f, 0.85f);
			}
			for (int32 i = 0; i < 7; ++i)
			{
				TArray<FVector> Vein;
				FVector P(R * 0.32f, Rng.FRandRange(-R * 0.6f, R * 0.6f), Rng.FRandRange(R * 0.2f, R * 1.8f));
				for (int32 k = 0; k < 6; ++k) { Vein.Add(P); P += FVector(0.f, Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f)); }
				G.Tube(Vein, 3.f, 4, FlCyan, 1.f);
			}
		}
		else
		{
			for (int32 i = 0; i < 14; ++i) { const float A = i * 2.f * PI / 14.f; FlBlob(M, FVector(0.f, FMath::Cos(A) * R, R + FMath::Sin(A) * R - 30.f), 30.f, 50.f, FlRgb(150, 128, 98) * 0.5f, 0.9f, 7); }
		}
	}
	else if (Kind == TEXT("pod"))
	{
		// A mound pod: a fungal knob 2 m tall, its pores glowing (burnt: a charred stub).
		if (State == 0)
		{
			FlBlob(M, FVector::ZeroVector, 90.f, 200.f, FlRgb(180, 150, 110) * 0.55f, 0.8f, 12, 6.f, 3.f);
			for (int32 i = 0; i < 10; ++i) { const float A = Rng.FRandRange(0.f, 2.f * PI); const float Z = Rng.FRandRange(40.f, 170.f); const float Rr = FMath::Sin(Z / 200.f * PI) * 90.f; FlBlob(G, FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z - 6.f), 8.f, 12.f, FlCyan, 1.f, 6); }
		}
		else FlBlob(M, FVector::ZeroVector, 70.f, 60.f, FlRgb(30, 26, 22) * 0.5f, 0.95f, 10, 4.f, 2.f);
	}
	else if (Kind == TEXT("web"))
	{
		// A web sheet spanning the space: a grid of beaded threads (a see-through silk), anchored at the floor and high.
		if (State == 0)
		{
			const float W = Radius, H = 600.f;
			bVeil = true;
			for (int32 i = 0; i <= 10; ++i) V.Tube({ FVector(0.f, -W * 0.5f + W * i / 10.f, 0.f), FVector(0.f, -W * 0.5f + W * i / 10.f + Rng.FRandRange(-30.f, 30.f), H) }, 1.2f, 4, FlRgb(235, 240, 238), 0.6f);
			for (int32 j = 0; j <= 8; ++j) { const float Z = H * j / 8.f; V.Tube({ FVector(0.f, -W * 0.5f, Z), FVector(Rng.FRandRange(-20.f, 20.f), 0.f, Z + Rng.FRandRange(-40.f, 0.f)), FVector(0.f, W * 0.5f, Z) }, 1.f, 4, FlRgb(235, 240, 238), 0.6f); }
			for (int32 b = 0; b < 30; ++b) FlBlob(G, FVector(0.f, Rng.FRandRange(-W * 0.5f, W * 0.5f), Rng.FRandRange(20.f, H)), 2.5f, 5.f, FlCyan, 1.f, 6);
		}
	}
	if (M.Verts.Num()) { Mesh = NewObject<UProceduralMeshComponent>(this); Mesh->SetupAttachment(RootComponent); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent(); M.ApplyTo(Mesh); }
	if (G.Verts.Num())
	{
		Glow = NewObject<UProceduralMeshComponent>(this);
		Glow->SetupAttachment(RootComponent);
		Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glow->RegisterComponent();
		G.ApplyTo(Glow);
		if (UMaterialInterface* Mat = WRFungalLook::GlowMaterial(this)) Glow->SetMaterial(0, Mat);
	}
	if (bVeil && V.Verts.Num())
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
		P->SetupAttachment(RootComponent);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->RegisterComponent();
		V.ApplyTo(P);
		if (UMaterialInterface* Mat = WRFungalLook::VeilMaterial(this)) P->SetMaterial(0, Mat);
		if (!Mesh) Mesh = P;
	}
	if (State == 0 && (Kind == TEXT("sac") || Kind == TEXT("pod")))
	{
		Lamp = NewObject<UPointLightComponent>(this);
		Lamp->SetupAttachment(RootComponent);
		Lamp->SetRelativeLocation(FVector(0.f, 0.f, Kind == TEXT("pod") ? 120.f : 0.f));
		Lamp->SetLightColor(FlCyan);
		Lamp->SetIntensity(Kind == TEXT("sac") ? 2500.f : 1200.f);
		Lamp->SetAttenuationRadius(Kind == TEXT("sac") ? 700.f : 400.f);
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
	}
}

void AWRFungalBossProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	Clock += DeltaSeconds;
	// A sac and a pod breathe (the eggs shifting inside); a live pool's slurry trembles.
	if (Glow && Kind == TEXT("sac") && State == 0) Glow->SetRelativeScale3D(FVector(1.f + 0.04f * FMath::Sin(Clock * 2.1f + Index)));
	if (Lamp && Kind == TEXT("pod")) Lamp->SetIntensity(900.f + 500.f * FMath::Sin(Clock * 1.4f + Index));
}

// ================================================================================================ UWRFungalBossBody

UWRFungalBossBody::UWRFungalBossBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UWRFungalBossBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRFungalBossBody, Flags);
	DOREPLIFETIME(UWRFungalBossBody, Pose);
}

UWRFungalBossBody* UWRFungalBossBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRFungalBossBody>() : nullptr;
}

void UWRFungalBossBody::Attach(AWRCreature* Boss)
{
	if (!Boss || Get(Boss)) return;
	UWRFungalBossBody* B = NewObject<UWRFungalBossBody>(Boss, TEXT("FungalBossBody"));
	B->RegisterComponent();
}

FName UWRFungalBossBody::BoneLike(const TCHAR* Hint) const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!S || !S->GetSkeletalMeshAsset()) return NAME_None;
	const FReferenceSkeleton& Ref = S->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FString H = FString(Hint).ToLower();
	for (int32 i = 0; i < Ref.GetNum(); ++i) if (Ref.GetBoneName(i).ToString().ToLower() == H) return Ref.GetBoneName(i);
	for (int32 i = 0; i < Ref.GetNum(); ++i) if (Ref.GetBoneName(i).ToString().ToLower().Contains(H)) return Ref.GetBoneName(i);
	return NAME_None;
}

USceneComponent* UWRFungalBossBody::AddPart(FName Bone, const FVector& Local, FWRToolMesh& M, bool bGlow, int32 Tag)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* S = C ? C->GetCreatureSkel() : nullptr;
	if (!C) return nullptr;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
	if (S && !Bone.IsNone()) P->SetupAttachment(S, Bone);
	else P->SetupAttachment(C->GetRootComponent());
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetUsingAbsoluteScale(true);
	P->RegisterComponent();
	const FVector At = (S && !Bone.IsNone()) ? S->GetSocketLocation(Bone) : C->GetActorLocation();
	P->SetWorldLocationAndRotation(At + C->GetActorRotation().RotateVector(Local), C->GetActorRotation());
	P->SetWorldScale3D(FVector::OneVector);
	M.ApplyTo(P);
	if (bGlow) if (UMaterialInterface* G = WRFungalLook::GlowMaterial(C)) P->SetMaterial(0, G);
	LookComps.Add(P);
	FPart Pt;
	Pt.Comp = P;
	Pt.Tag = Tag;
	Pt.BaseLoc = P->GetRelativeLocation();
	Pt.BaseRot = P->GetRelativeRotation();
	Pt.BaseScale = FVector::OneVector;
	Parts.Add(Pt);
	return P;
}

void UWRFungalBossBody::ApplyTint()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (!Skel || !Master) return;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	TSharedPtr<FJsonObject> Row;
	if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List) { const TSharedPtr<FJsonObject> O = V->AsObject(); FString Id; if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id) && FName(*Id) == C->CreatureId) { Row = O; break; } }
	}
	const TSharedPtr<FJsonObject>* Look = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	if (!Row.IsValid() || !Row->TryGetObjectField(TEXT("look"), Look) || !(*Look)->TryGetArrayField(TEXT("tint"), Tints) || Tints->Num() == 0) return;
	const TSharedPtr<FJsonObject> Pick = (*Tints)[0]->AsObject();
	const auto Col = [&](const TCHAR* F, const FLinearColor& Def) { FString S; return Pick->TryGetStringField(F, S) ? FLinearColor(FColor::FromHex(S)) : Def; };
	const auto Num = [&](const TCHAR* F, float Def) { double V; return Pick->TryGetNumberField(F, V) ? float(V) : Def; };
	for (int32 Slot = 0; Slot < Skel->GetNumMaterials(); ++Slot)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
		FString Tex;
		if (Pick->TryGetStringField(TEXT("tex"), Tex)) if (UTexture* T = LoadObject<UTexture>(nullptr, *Tex)) M->SetTextureParameterValue(TEXT("BaseColorTex"), T);
		if (UTexture* N = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))) M->SetTextureParameterValue(TEXT("NormalTex"), N);
		if (UTexture* B = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"))) M->SetTextureParameterValue(TEXT("GlowTex"), B);
		M->SetScalarParameterValue(TEXT("Desaturation"), Num(TEXT("desat"), 1.f));
		M->SetScalarParameterValue(TEXT("Brightness"), Num(TEXT("bright"), 1.f));
		M->SetScalarParameterValue(TEXT("TwoTone"), Num(TEXT("twoTone"), 0.f));
		M->SetScalarParameterValue(TEXT("LightFrom"), Num(TEXT("lightFrom"), 0.f));
		M->SetScalarParameterValue(TEXT("Roughness"), Num(TEXT("rough"), 0.75f));
		M->SetScalarParameterValue(TEXT("GlowStrength"), Num(TEXT("glowStrength"), 0.f));
		M->SetVectorParameterValue(TEXT("Tint"), Col(TEXT("tint"), FLinearColor::White));
		M->SetVectorParameterValue(TEXT("TintLight"), Col(TEXT("light"), Col(TEXT("tint"), FLinearColor::White)));
		M->SetVectorParameterValue(TEXT("GlowColor"), Col(TEXT("glow"), FlCyan));
		Skel->SetMaterial(Slot, M);
	}
}

void UWRFungalBossBody::BuildLook()
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!D || !Skel) return;
	const FName Id = C->CreatureId;
	if (Id != TEXT("mycelorn") && !Skel->GetSkeletalMeshAsset()) return;
	bBuilt = true;
	ApplyTint();
	const float R = D->Radius, H = D->HalfHeight;
	FRandomStream Rng(int32(GetTypeHash(C->GetName())));
	if (Id == TEXT("sporemother"))
	{
		// The abdomen swollen tight and translucent behind her, glowing egg sacs shifting inside (Tag 1 pulses); a ring of
		// pale caps for a crown (Tag 2); the barbed hypostome, long as a spear.
		FWRToolMesh Ab, Eggs, Crown, Spear;
		Ab.Lathe(FVector(-R * 0.25f, 0.f, H * 0.2f), FVector(-1.f, 0.f, 0.25f).GetSafeNormal(), R * 1.5f, { FVector(0.f, R * 0.35f, R * 0.3f), FVector(0.35f, R * 0.85f, R * 0.7f), FVector(0.7f, R * 0.8f, R * 0.65f), FVector(1.f, 0.f, 0.f) }, 16, FlRgb(150, 200, 190) * 0.55f, 0.35f);
		for (int32 i = 0; i < 18; ++i) FlBlob(Eggs, FVector(-R * Rng.FRandRange(0.5f, 1.4f), Rng.FRandRange(-R * 0.45f, R * 0.45f), H * Rng.FRandRange(0.f, 0.9f)), Rng.FRandRange(14.f, 26.f), Rng.FRandRange(26.f, 44.f), FlCyan, 1.f, 8);
		for (int32 i = 0; i < 9; ++i) { const float A = i * 2.f * PI / 9.f; FlCap(Crown, FVector(FMath::Cos(A) * R * 0.35f, FMath::Sin(A) * R * 0.35f, H * 0.7f), FVector(FMath::Cos(A) * 0.3f, FMath::Sin(A) * 0.3f, 1.f).GetSafeNormal(), 30.f, 6.f, 28.f, 18.f, FlIvory * 0.6f, FlIvory * 0.62f, 0.7f); }
		Spear.Lathe(FVector(R * 0.6f, 0.f, -H * 0.2f), FVector(1.f, 0.f, -0.1f).GetSafeNormal(), 400.f, { FVector(0.f, 14.f, 12.f), FVector(0.6f, 8.f, 7.f), FVector(1.f, 0.f, 0.f) }, 8, FlRgb(120, 110, 96) * 0.55f, 0.5f);
		for (int32 b = 0; b < 8; ++b) Spear.Lathe(FVector(R * 0.6f + 120.f + b * 30.f, 0.f, -H * 0.2f - 10.f), FVector(-0.5f, (b % 2 ? 1.f : -1.f), 0.3f).GetSafeNormal(), 22.f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 0.f, 0.f) }, 4, FlRgb(120, 110, 96) * 0.55f, 0.5f);
		AddPart(NAME_None, FVector::ZeroVector, Ab, false, 1);
		AddPart(NAME_None, FVector::ZeroVector, Eggs, true, 1);
		AddPart(NAME_None, FVector::ZeroVector, Crown, false, 2);
		AddPart(BoneLike(TEXT("head")), FVector::ZeroVector, Spear, false, 0);
	}
	else if (Id == TEXT("rot_king"))
	{
		// A crown of tall black rotting caps 3 m high over the shell, dripping rust slurry (Tag 1: the crown tilts to
		// pour); old Deepfarmer bronze hooks still in his shell; his beak chipped white.
		FWRToolMesh Crown, Hooks;
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI), Rr = FMath::Sqrt(Rng.FRand()) * R * 0.6f;
			const float Tall = Rng.FRandRange(120.f, 300.f);
			FlCap(Crown, FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr * 0.8f, H * 0.7f), FVector(Rng.FRandRange(-0.2f, 0.2f), Rng.FRandRange(-0.2f, 0.2f), 1.f).GetSafeNormal(), Tall, Rng.FRandRange(7.f, 14.f), Rng.FRandRange(30.f, 70.f), Rng.FRandRange(20.f, 40.f), FlRgb(40, 30, 26) * 0.5f, FlRgb(26, 20, 18) * 0.5f, 0.35f);
		}
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			const FVector Base(FMath::Cos(A) * R * 0.75f, FMath::Sin(A) * R * 0.6f, H * 0.3f);
			Hooks.Tube({ Base, Base + FVector(FMath::Cos(A) * 40.f, FMath::Sin(A) * 40.f, 50.f), Base + FVector(FMath::Cos(A) * 70.f, FMath::Sin(A) * 70.f, 30.f), Base + FVector(FMath::Cos(A) * 60.f, FMath::Sin(A) * 60.f, 5.f) }, 4.f, 6, FlBronze * 0.6f, 0.4f);
		}
		AddPart(NAME_None, FVector::ZeroVector, Crown, false, 1);
		AddPart(NAME_None, FVector::ZeroVector, Hooks, false, 0);
	}
	else if (Id == TEXT("weaver"))
	{
		// Two mirror-bright eyes, the abdomen a field of cyan dots like constellations, and eight 4.5 m legs banded cream
		// and black (stepping with her speed; folded up while she hangs asleep).
		FWRToolMesh Eyes, Stars;
		for (const float S : { -1.f, 1.f }) Eyes.Lathe(FVector(R * 0.55f, S * 18.f, H * 0.5f), FVector(1.f, 0.f, 0.f), 24.f, { FVector(0.f, 0.f, 0.f), FVector(0.4f, 16.f, 16.f), FVector(1.f, 0.f, 0.f) }, 10, FlRgb(225, 230, 235), 0.08f);
		for (int32 i = 0; i < 40; ++i) FlBlob(Stars, FVector(-R * Rng.FRandRange(0.3f, 1.1f), Rng.FRandRange(-R * 0.4f, R * 0.4f), H * Rng.FRandRange(0.4f, 1.0f)), Rng.FRandRange(2.5f, 6.f), 4.f, FlCyan, 1.f, 6);
		AddPart(NAME_None, FVector::ZeroVector, Eyes, false, 0);
		AddPart(NAME_None, FVector::ZeroVector, Stars, true, 0);
		for (int32 i = 0; i < 8; ++i)
		{
			FLeg Lg;
			Lg.Angle = i < 4 ? 35.f + i * 33.f : -35.f - (i - 4) * 33.f;
			Lg.Phase = (i % 2) * PI + (i >= 4 ? PI * 0.5f : 0.f);
			for (int32 s = 0; s < 2; ++s)
			{
				FWRToolMesh Seg;
				Seg.Lathe(FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 100.f, { FVector(0.f, 7.f, 7.f), FVector(1.f, 5.f, 5.f) }, 6, FlRgb(20, 18, 16) * 0.5f, 0.5f);
				for (int32 b = 1; b < 5; ++b) Seg.Lathe(FVector(b * 20.f - 5.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 10.f, { FVector(0.f, 7.5f, 7.5f), FVector(1.f, 7.2f, 7.2f) }, 6, FlCream * 0.62f, 0.6f);
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
				P->SetupAttachment(C->GetRootComponent());
				P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				P->SetUsingAbsoluteLocation(true);
				P->SetUsingAbsoluteRotation(true);
				P->SetUsingAbsoluteScale(true);
				P->RegisterComponent();
				Seg.ApplyTo(P);
				(s == 0 ? Lg.Upper : Lg.Lower) = P;
				LookComps.Add(P);
			}
			Legs.Add(Lg);
		}
	}
	else if (Id == TEXT("mycelorn"))
	{
		// All code (fungal 2.14): from the floor (the capsule's foot) a translucent amber stipe 6 m thick, 24 m tall, a
		// titan's ribs inside it; the Heart-seed in its base (Tag 3: beats 35 a minute); a torn veil ring at 8 m; the
		// summited host spiralling up it; a 36 m umber cap with glowing gills under it (Tag 4 lit / Tag 5 dark); the
		// curtain of a hundred black tendrils from its rim (Tag 6), the twelve thickest ending in bone-tipped clubs (Tag 7).
		Skel->SetVisibility(false, true);
		const FVector Foot(0.f, 0.f, -H);
		const float StemH = 2400.f, StemR = 300.f, CapR = 1800.f;
		FWRToolMesh Stem, Ribs, Heart, Veil, Host, Cap, Gills, GillsDark, Curtain, Clubs;
		TArray<FVector> SP;
		for (int32 k = 0; k <= 10; ++k) { const float T = k / 10.f; const float Rr = StemR * (1.25f - 0.35f * T + 0.05f * FMath::Sin(T * 9.f)); SP.Add(FVector(T, Rr, Rr)); }
		Stem.Lathe(Foot, FVector::UpVector, StemH, SP, 20, FlAmber * 0.7f, 0.25f, 0.f, 1.f);
		for (int32 r = 0; r < 9; ++r)
		{
			TArray<FVector> Rib;
			const float Z = 300.f + r * 170.f;
			for (int32 k = 0; k <= 10; ++k) { const float A = -0.8f * PI + k * 1.6f * PI / 10.f; Rib.Add(Foot + FVector(FMath::Cos(A) * StemR * 0.7f, FMath::Sin(A) * StemR * 0.7f, Z + FMath::Sin(k / 10.f * PI) * 60.f)); }
			Ribs.Tube(Rib, 10.f, 5, FlBone * 0.6f, 0.6f);
		}
		Ribs.Lathe(Foot + FVector(0.f, -StemR * 0.6f, 250.f), FVector::UpVector, 1700.f, { FVector(0.f, 14.f, 14.f), FVector(1.f, 10.f, 10.f) }, 6, FlBone * 0.6f, 0.6f);
		FlBlob(Heart, Foot + FVector(0.f, 0.f, 40.f), 150.f, 300.f, FlRgb(255, 170, 70), 1.f, 16);
		for (int32 k = 0; k < 18; ++k)
		{
			const float A = k * 2.f * PI / 18.f;
			const FVector Top = Foot + FVector(FMath::Cos(A) * StemR * 1.12f, FMath::Sin(A) * StemR * 1.12f, 800.f);
			Veil.Lathe(Top, FVector(FMath::Cos(A) * 0.4f, FMath::Sin(A) * 0.4f, -1.f).GetSafeNormal(), Rng.FRandRange(120.f, 260.f), { FVector(0.f, 4.f, 60.f), FVector(1.f, 2.f, 30.f) }, 4, FlCream * 0.6f, 0.4f);
		}
		for (int32 k = 0; k < 60; ++k)
		{
			// The summited host: bodies fused into the stem, all facing up, jaws open.
			const float T = k / 60.f, A = T * 9.f * PI;
			const FVector At = Foot + FVector(FMath::Cos(A) * StemR * 1.05f, FMath::Sin(A) * StemR * 1.05f, 200.f + T * 2000.f);
			const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
			Host.Lathe(At, (FVector::UpVector + Out * 0.15f).GetSafeNormal(), 160.f, { FVector(0.f, 16.f, 12.f), FVector(0.45f, 20.f, 14.f), FVector(0.75f, 12.f, 10.f), FVector(0.85f, 13.f, 13.f), FVector(1.f, 0.f, 0.f) }, 6, FlRgb(150, 120, 90) * 0.55f, 0.8f);
		}
		TArray<FVector> CP;
		CP.Add(FVector(0.f, StemR * 0.8f, StemR * 0.8f));
		for (int32 k = 1; k <= 10; ++k) { const float T = k / 10.f; const float Rr = CapR * FMath::Sin(T * PI * 0.5f) * (1.f - 0.92f * FMath::Pow(T, 6.f)); CP.Add(FVector(T, FMath::Max(Rr, 1.f), FMath::Max(Rr, 1.f))); }
		Cap.Lathe(Foot + FVector(0.f, 0.f, StemH - 150.f), FVector::UpVector, 900.f, CP, 32, FlUmber * 0.6f, 0.75f, 0.f, 2.f);
		const auto GillDisc = [&](FWRToolMesh& M, const FLinearColor& Col, float Glow)
		{
			TArray<TArray<FVector>> Rings;
			for (const float Z : { 0.f, 12.f })
			{
				TArray<FVector> Ring;
				for (int32 k = 0; k < 48; ++k) { const float A = k * 2.f * PI / 48.f; const float Rr = k % 2 ? CapR * 0.97f : CapR * 0.93f; Ring.Add(Foot + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, StemH - 160.f + Z)); }
				Rings.Add(Ring);
			}
			M.Loft(Rings, Col, Glow, true);
			for (int32 k = 0; k < 72; ++k)
			{
				const float A = k * 2.f * PI / 72.f;
				M.Tube({ Foot + FVector(FMath::Cos(A) * StemR * 1.1f, FMath::Sin(A) * StemR * 1.1f, StemH - 130.f), Foot + FVector(FMath::Cos(A) * CapR * 0.95f, FMath::Sin(A) * CapR * 0.95f, StemH - 175.f) }, 9.f, 3, Col, Glow);
			}
		};
		GillDisc(Gills, FlCyan, 1.f);
		GillDisc(GillsDark, FlRgb(30, 50, 46) * 0.5f, 0.6f);
		for (int32 k = 0; k < 100; ++k)
		{
			const float A = k * 2.f * PI / 100.f + Rng.FRandRange(-0.02f, 0.02f);
			const bool bClub = k % 8 == 0 && k / 8 < 12;
			const float Rr = CapR * Rng.FRandRange(0.82f, 0.97f);
			const FVector Top = Foot + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, StemH - 160.f);
			const float Len = FMath::Min(StemH - 150.f, Rng.FRandRange(800.f, 2000.f));
			if (bClub)
			{
				FlCord(Clubs, Top, Len, Rng.FRandRange(45.f, 75.f), FlTendril * 0.6f, 0.5f, float(k));
				const FVector End = Top + FVector(FMath::Sin(3.f + k) * Len * 0.04f, FMath::Cos(2.3f + k) * Len * 0.04f, -Len);
				FlBlob(Clubs, End - FVector(0.f, 0.f, 160.f), 90.f, 200.f, FlTendril * 0.6f, 0.5f, 10);
				for (int32 h = 0; h < 4; ++h) Clubs.Lathe(End - FVector(0.f, 0.f, 120.f), FRotator(-30.f, h * 90.f, 0.f).Vector(), 120.f, { FVector(0.f, 12.f, 12.f), FVector(1.f, 0.f, 0.f) }, 5, FlBone * 0.6f, 0.5f);
			}
			else FlCord(Curtain, Top, Len, Rng.FRandRange(15.f, 40.f), FlTendril * 0.6f, 0.55f, float(k));
		}
		USceneComponent* StemC = AddPart(NAME_None, FVector::ZeroVector, Stem, false, 0);
		if (UProceduralMeshComponent* PS = Cast<UProceduralMeshComponent>(StemC)) if (UMaterialInterface* Vm = WRFungalLook::VeilMaterial(C)) PS->SetMaterial(0, Vm);
		AddPart(NAME_None, FVector::ZeroVector, Ribs, false, 0);
		AddPart(NAME_None, FVector::ZeroVector, Heart, true, 3);
		AddPart(NAME_None, FVector::ZeroVector, Veil, false, 0);
		AddPart(NAME_None, FVector::ZeroVector, Host, false, 0);
		AddPart(NAME_None, FVector::ZeroVector, Cap, false, 0);
		AddPart(NAME_None, FVector::ZeroVector, Gills, true, 4);
		AddPart(NAME_None, FVector::ZeroVector, GillsDark, false, 5);
		AddPart(NAME_None, FVector::ZeroVector, Curtain, false, 6);
		AddPart(NAME_None, FVector::ZeroVector, Clubs, false, 7);
		Lamp = NewObject<UPointLightComponent>(C);
		Lamp->SetupAttachment(C->GetRootComponent());
		Lamp->SetRelativeLocation(Foot + FVector(0.f, 0.f, 200.f));
		Lamp->SetLightColor(FlAmber);
		Lamp->SetIntensity(30000.f);
		Lamp->SetAttenuationRadius(5000.f);
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
		for (FPart& P : Parts) if (P.Comp.IsValid() && P.Tag == 5) P.Comp->SetVisibility(false);
	}
}

void UWRFungalBossBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!bBuilt && ++Tries < 60) BuildLook();
	if (bBuilt) TickLook(DeltaTime);
}

void UWRFungalBossBody::TickLook(float Dt)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* D = C ? C->GetDef() : nullptr;
	if (!C || !D) return;
	Clock += Dt;
	const FName Id = C->CreatureId;
	Flare = FMath::FInterpTo(Flare, (Pose == 2) ? 1.f : 0.f, Dt, 5.f);
	Lift = FMath::FInterpTo(Lift, (Pose == 1 || Pose == 3 || Has(WRFungalBossFlag::Rearing)) ? 1.f : 0.f, Dt, 4.f);
	if (Id == TEXT("sporemother"))
	{
		// The abdomen breathes, the eggs shift; rearing (the press) lifts the crown.
		for (FPart& P : Parts)
		{
			if (!P.Comp.IsValid()) continue;
			if (P.Tag == 1) P.Comp->SetWorldScale3D(FVector(1.f + 0.03f * FMath::Sin(Clock * 1.7f)));
			if (P.Tag == 2) P.Comp->SetRelativeLocation(P.BaseLoc + FVector(0.f, 0.f, Lift * 60.f));
		}
	}
	else if (Id == TEXT("rot_king"))
	{
		// The crown drips rust slurry unless wind has dried it; it tilts to pour for the Rot spread.
		const bool bDry = Has(WRFungalBossFlag::Dry);
		for (FPart& P : Parts) if (P.Comp.IsValid() && P.Tag == 1) P.Comp->SetRelativeRotation(P.BaseRot + FRotator(-Lift * 18.f, 0.f, 0.f));
		DripClock -= Dt;
		if (!bDry && DripClock <= 0.f)
		{
			DripClock = 0.5f;
			const FVector At = C->GetActorLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * D->Radius * 0.5f + FVector(0.f, 0.f, D->HalfHeight + 150.f);
			WRFx::SpawnAt(C, TEXT("fungal.slurryDrip"), At, FRotator::ZeroRotator, 1.f);
		}
	}
	else if (Id == TEXT("weaver"))
	{
		// Her legs: hips round the body to feet at her 4.5 m span stepping with her speed; folded above her as she
		// hangs asleep; drawn in the air while she's out of sight.
		const bool bAsleep = Has(WRFungalBossFlag::Asleep);
		if (USkeletalMeshComponent* S = C->GetCreatureSkel())
		{
			const float Roll = bAsleep ? 180.f : 0.f;
			if (!FMath::IsNearlyEqual(S->GetRelativeRotation().Roll, Roll, 1.f)) S->SetRelativeRotation(FRotator(S->GetRelativeRotation().Pitch, S->GetRelativeRotation().Yaw, Roll));
		}
		const float Span = WRTune(TEXT("bosses.weaver.legReach"), 430.f);
		Stride += C->GetVelocity().Size2D() * Dt / 220.f;
		const FVector Body = C->GetActorLocation();
		const float Ground = Body.Z - D->HalfHeight;
		for (FLeg& L : Legs)
		{
			const FVector Dir = FRotator(0.f, C->GetActorRotation().Yaw + L.Angle, 0.f).Vector();
			const float Up = FMath::Max(0.f, FMath::Sin(Stride + L.Phase)) * 50.f;
			const FVector Hip = Body + Dir * 40.f;
			const FVector Foot = bAsleep ? Body + Dir * Span * 0.6f + FVector(0.f, 0.f, D->HalfHeight + 40.f)
				: FVector(Body.X, Body.Y, Ground) + Dir * Span + C->GetActorForwardVector() * FMath::Cos(Stride + L.Phase) * 50.f + FVector(0.f, 0.f, Up);
			const FVector Knee = (Hip + Foot) * 0.5f + FVector(0.f, 0.f, bAsleep ? -120.f : 240.f);
			const auto Place = [](UProceduralMeshComponent* P, const FVector& A, const FVector& B)
			{
				if (!P) return;
				const FVector V = B - A;
				P->SetWorldLocationAndRotation(A, V.Rotation());
				P->SetWorldScale3D(FVector(V.Size() / 100.f, 1.f, 1.f));
			};
			Place(L.Upper.Get(), Hip, Knee);
			Place(L.Lower.Get(), Knee, Foot);
			if (L.Upper.IsValid()) L.Upper->SetVisibility(!C->IsHidden());
			if (L.Lower.IsValid()) L.Lower->SetVisibility(!C->IsHidden());
		}
	}
	else if (Id == TEXT("mycelorn"))
	{
		// The Heart-seed beats 35 times a minute (bigger and brighter split open in phase 3); the gills go dark while it
		// chokes or once they're torn; the curtain sways like a jellyfish's and flares outward for the spin and the lash;
		// a club lifts for its slam; the fist draws in for the engulf.
		const bool bSplit = Has(WRFungalBossFlag::Split);
		const bool bDark = Has(WRFungalBossFlag::Choked) || Has(WRFungalBossFlag::NoGills);
		const float Beat = FMath::Pow(FMath::Max(0.f, FMath::Sin(Clock * 2.f * PI * 35.f / 60.f)), 6.f);
		for (FPart& P : Parts)
		{
			if (!P.Comp.IsValid()) continue;
			if (P.Tag == 3) P.Comp->SetWorldScale3D(FVector((bSplit ? 1.3f : 1.f) * (1.f + 0.08f * Beat)));
			if (P.Tag == 4) P.Comp->SetVisibility(!bDark && !C->IsHidden());
			if (P.Tag == 5) P.Comp->SetVisibility(bDark && !C->IsHidden());
			if (P.Tag == 6 || P.Tag == 7)
			{
				const float Sway = FMath::Sin(Clock * 0.6f + P.Tag) * 1.5f;
				const float Out = 1.f + Flare * 0.25f + (P.Tag == 7 ? Lift * 0.1f : 0.f);
				P.Comp->SetRelativeRotation(P.BaseRot + FRotator(Sway, Clock * 2.f, -Sway));
				P.Comp->SetWorldScale3D(FVector(Out, Out, 1.f - (P.Tag == 7 ? Lift * 0.25f : 0.f) - (Pose == 3 ? 0.2f : 0.f)));
			}
		}
		if (Lamp) Lamp->SetIntensity((bSplit ? 60000.f : 30000.f) * (0.7f + 0.3f * Beat));
	}
}
