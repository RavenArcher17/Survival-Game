// The Ancient forest's bosses' looks (cooks/biome-forest.md 2.13-2.14; cooks/build-biome-forest.md 1.3, 2.10): each boss
// is the closest real rig with its own tint (creatures.json look.tint over M_CreatureTint) and code-built parts that make
// its silhouette, modelled at its size on frames riding its bones (built on every machine that draws; the server keeps
// only the state they show). See UWRForestBossBody in WRForestBosses.h.
//
//  Old Gnarl (Obsidian warden rig): bark plates 30 cm thick on chest, back and both forearms (pale wood under each, fire
//    on a burning one), the split mouth-hollow, amber knothole eyes, an oak's crown out of its shoulders, fists of
//    knotted branches, root feet, the clear-cut's axes still sunk in its legs.
//  The Bramblequeen (Kilnmite rig): the orange head with forearm-long mandibles, the black thorax, the barrel abdomen in
//    amber and black bands with its 60 cm stinger, four wings 6 m across that roar.
//  The Grey Matron (Pale hunter rig): yellowed sabres, a torn ear, old scars (her grey-blue is the tint).
//  The Hornwarden (Frostantler rig): the 8 m palmate crown hung with ivy, moss and banner rags, a dark mane, old arrows
//    in his flank, the spear-head lump in his neck.
//  Sylvaroth (Grimjaw rig): a lattice of roots fused into a cage round its torso with foxfire and Grovekin burials inside,
//    the burl head split into a jaw of heartwood fangs with amber eye hollows, root-lattice arms with root-claws, a hump of
//    living canopy on its back, the root mass it stands in (phase 1), the six pods that swell before the seed bombs, and
//    the vine sectors on the clearing's floor (phase 2).
#include "WRForestBosses.h"
#include "WRForestTypes.h"
#include "WRBoss.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRToolMesh.h"
#include "WRTreeKit.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/Package.h"

namespace WRForestBossLookImpl
{
	using FblSurf = WRLookKit::FSurf;
	using FblKit = WRLookKit::FKit;

	// The groups a look is made of (UWRForestBossBody::FGroup::Kind): what shows, moves or glows with the state.
	enum : int32
	{
		FblStatic = 0,
		FblBark0 = 10,      // + plate: Old Gnarl's bark plate (hidden when stripped)
		FblPale0 = 20,      // + plate: the pale wood under it
		FblCrown = 30,      // a crown that sways (Gnarl's oak, Sylvaroth's canopy hump)
		FblWingL = 31, FblWingR = 32, FblAbdomen = 33,
		FblAntlerL = 40, FblAntlerR = 41, FblMane = 42,
		FblRootMass = 50, FblFoxfire = 51, FblPods = 52,
	};

	FLinearColor FblRgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	FLinearColor FblHex(const FString& Hex) { return FLinearColor(FColor::FromHex(Hex)); }

	// Surfaces (the build kit's tinted blocks) and mesh colours (M_HeldTool vertex colour), dimmed as the arenas' are.
	FblSurf FblBark(float K = 1.f) { return FblSurf::Wood(FblRgb(74, 58, 42) * 0.55f * K); }
	FblSurf FblPale(float K = 1.f) { return FblSurf::Wood(FblRgb(196, 160, 112) * 0.55f * K); }
	FblSurf FblMoss(float K = 1.f) { return FblSurf::Cloth(FblRgb(78, 107, 42) * 0.45f * K); }
	FblSurf FblAmber(float Glow) { return FblSurf::Lit(FblRgb(208, 138, 30) * 0.6f, Glow); }
	FblSurf FblFox(float Glow) { return FblSurf::Lit(FblRgb(127, 224, 122) * 0.55f, Glow); }
	FblSurf FblDark() { return FblSurf::Stone(FblRgb(30, 26, 22) * 0.3f, 0.95f); }
	FblSurf FblIron() { return FblSurf::Steel(FLinearColor(0.035f, 0.033f, 0.032f), 0.6f); }
	FblSurf FblHaft() { return FblSurf::Wood(FblRgb(120, 92, 60) * 0.5f); }
	FblSurf FblIvory(float K = 1.f) { return FblSurf::Glaze(FblRgb(214, 196, 150) * 0.55f * K, 0.45f); }
	FblSurf FblChitin(const FLinearColor& C) { return FblSurf::Glaze(C, 0.22f); }
	FblSurf FblRag(int32 i) { const FLinearColor C[3] = { FblRgb(120, 40, 30), FblRgb(150, 118, 60), FblRgb(70, 82, 60) }; return FblSurf::Cloth(C[((i % 3) + 3) % 3] * 0.45f); }
	FblSurf FblFur(float K = 1.f) { return FblSurf::Cloth(FblRgb(40, 32, 26) * 0.45f * K); }

	FLinearColor FblRootC() { return FblRgb(90, 74, 58) * 0.5f; }
	FLinearColor FblDeadC() { return FblRgb(30, 26, 22) * 0.6f; }
	FLinearColor FblVineC() { return FblRgb(62, 90, 42) * 0.5f; }
	FLinearColor FblAntlerC() { return FblRgb(170, 150, 118) * 0.55f; }

	/** A creatures.json row (cached: the look reads it once a boss). */
	TSharedPtr<FJsonObject> FblRow(FName Id)
	{
		static TMap<FName, TSharedPtr<FJsonObject>> Rows;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FString RowId;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), RowId)) Rows.Add(FName(*RowId), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Found = Rows.Find(Id);
		return Found ? *Found : nullptr;
	}

	float FblNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}

	FLinearColor FblColour(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(Field, S) && S.StartsWith(TEXT("#")) ? FblHex(S) : Fallback;
	}

	float FblServerNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
	}

	/** A knotted branch: a tube that wanders a little from A toward B (bark colour), tapering by steps. */
	void FblBranch(FWRToolMesh& M, const FVector& A, const FVector& B, float R0, float R1, const FLinearColor& C, int32 Seed)
	{
		const int32 N = 5;
		const FVector D = B - A;
		const FVector Side = FVector::CrossProduct(D.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
		TArray<FVector> Pts;
		for (int32 i = 0; i <= N; ++i)
		{
			const float T = float(i) / N;
			const float Wob = FMath::Sin(T * PI * 2.f + Seed) * D.Size() * 0.06f * (1.f - T * 0.5f);
			Pts.Add(A + D * T + Side * Wob + FVector(0.f, 0.f, FMath::Cos(T * 5.f + Seed) * D.Size() * 0.03f));
		}
		// Two tubes: the thick root of the branch and its thinner end (a step where a knot is).
		TArray<FVector> First(Pts.GetData(), 4), Second(Pts.GetData() + 3, N - 2);
		M.Tube(First, R0, 8, C, 0.92f);
		M.Tube(Second, FMath::Lerp(R0, R1, 0.6f), 7, C, 0.92f);
	}
}

// ---------------------------------------------------------------- the component

UWRForestBossBody::UWRForestBossBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRForestBossBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRForestBossBody, Stripped);
	DOREPLIFETIME(UWRForestBossBody, Burning);
	DOREPLIFETIME(UWRForestBossBody, Pose);
	DOREPLIFETIME(UWRForestBossBody, bRooted);
	DOREPLIFETIME(UWRForestBossBody, bStuck);
	DOREPLIFETIME(UWRForestBossBody, bEnraged);
	DOREPLIFETIME(UWRForestBossBody, bGrounded);
	DOREPLIFETIME(UWRForestBossBody, Sectors);
	DOREPLIFETIME(UWRForestBossBody, SectorUntil);
	DOREPLIFETIME(UWRForestBossBody, SectorCentre);
	DOREPLIFETIME(UWRForestBossBody, SectorRadius);
	DOREPLIFETIME(UWRForestBossBody, Sink);
}

UWRForestBossBody* UWRForestBossBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRForestBossBody>() : nullptr;
}

void UWRForestBossBody::Attach(AWRBoss* Boss)
{
	if (!Boss || !Boss->HasAuthority() || Get(Boss))
	{
		return;
	}
	UWRForestBossBody* Body = NewObject<UWRForestBossBody>(Boss, TEXT("ForestBossBody"));
	Body->RegisterComponent();
	Boss->AddInstanceComponent(Body);
}

void UWRForestBossBody::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearLook();
	Super::EndPlay(Reason);
}

void UWRForestBossBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Clock += DeltaTime;
	TickLook(DeltaTime);
	TickSectors();
}

void UWRForestBossBody::ClearLook()
{
	for (USceneComponent* C : LookComps) if (C) C->DestroyComponent();
	LookComps.Reset();
	for (USceneComponent* C : PlateFx) if (C) WRFx::Stop(C);
	PlateFx.Reset();
	for (USceneComponent* C : SectorFx) if (C) WRFx::Stop(C);
	SectorFx.Reset();
	for (UProceduralMeshComponent* M : SectorMesh) if (M) M->DestroyComponent();
	SectorMesh.Reset();
	for (UPointLightComponent* L : Lights) if (L) L->DestroyComponent();
	Lights.Reset();
	Groups.Reset();
	SectorShown.Reset();
	SectorChanged.Reset();
	ShownStripped = ShownBurning = 255;
	bBuilt = false;
}

USceneComponent* UWRForestBossBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!Skel || Blocks.Num() == 0)
	{
		return nullptr;
	}
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Skel, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
	Frame->SetWorldScale3D(FVector(1.f));
	Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
	LookComps.Add(Frame);
	FWRBuildLook::Spawn(C, Frame, Blocks, FLinearColor::White, false);
	TArray<USceneComponent*> Children;
	Frame->GetChildrenComponents(false, Children);
	for (USceneComponent* Ch : Children) LookComps.Add(Ch);
	FGroup G;
	G.Frame = Frame;
	G.Kind = Kind;
	G.BaseRot = Frame->GetRelativeRotation();
	G.BaseLoc = Frame->GetRelativeLocation();
	Groups.Add(G);
	return Frame;
}

UProceduralMeshComponent* UWRForestBossBody::AddMesh(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const FWRToolMesh& Mesh, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!Skel || Mesh.Verts.Num() == 0)
	{
		return nullptr;
	}
	// A part with a NaN point or placement gave its mesh NaN bounds (the engine's ensure on Sylvaroth's body, 11-17):
	// leave it out and say which, so the shape that makes it can be found.
	bool bNaN = WorldAt.ContainsNaN() || WorldRot.ContainsNaN();
	for (int32 i = 0; i < Mesh.Verts.Num() && !bNaN; ++i) bNaN = Mesh.Verts[i].ContainsNaN();
	if (bNaN)
	{
		UE_LOG(LogWildreach, Warning, TEXT("BOSSLOOK %s: part on %s (kind %d) has a NaN point or placement, left out"), *GetNameSafe(C), *Bone.ToString(), Kind);
		return nullptr;
	}
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Skel, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
	Frame->SetWorldScale3D(FVector(1.f));
	Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
	LookComps.Add(Frame);
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
	P->SetupAttachment(Frame);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetCanEverAffectNavigation(false);
	P->RegisterComponent();
	Mesh.ApplyTo(P);
	LookComps.Add(P);
	FGroup G;
	G.Frame = Frame;
	G.Kind = Kind;
	G.BaseRot = Frame->GetRelativeRotation();
	G.BaseLoc = Frame->GetRelativeLocation();
	Groups.Add(G);
	return P;
}

UStaticMeshComponent* UWRForestBossBody::AddKitMesh(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, FName Species, int32 Variant, float Height, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	UStaticMesh* Mesh = WRTreeKit::Mesh(GetTransientPackage(), Species, Variant);
	if (!Skel || !Mesh)
	{
		return nullptr;
	}
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Skel, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
	Frame->SetWorldScale3D(FVector(1.f));
	Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
	LookComps.Add(Frame);
	UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(C);
	S->SetStaticMesh(Mesh);
	S->SetupAttachment(Frame);
	S->SetRelativeScale3D(FVector(Height / FMath::Max(1.f, WRTreeKit::ReferenceHeight(Species))));
	S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	S->SetCanEverAffectNavigation(false);
	S->RegisterComponent();
	LookComps.Add(S);
	FGroup G;
	G.Frame = Frame;
	G.Kind = Kind;
	G.BaseRot = Frame->GetRelativeRotation();
	G.BaseLoc = Frame->GetRelativeLocation();
	Groups.Add(G);
	return S;
}

void UWRForestBossBody::SetKindVisible(int32 Kind, bool bOn)
{
	for (FGroup& G : Groups)
	{
		if (G.Kind != Kind || !G.Frame.IsValid() || G.bShown == bOn) continue;
		G.bShown = bOn;
		G.Frame->SetVisibility(bOn, true);
	}
}

void UWRForestBossBody::ApplyTint()
{
	using namespace WRForestBossLookImpl;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	const TSharedPtr<FJsonObject> Row = C ? FblRow(C->CreatureId) : nullptr;
	const TSharedPtr<FJsonObject>* LookObj = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (!Skel || !Row.IsValid() || !Master || !Row->TryGetObjectField(TEXT("look"), LookObj) || !(*LookObj)->TryGetArrayField(TEXT("tint"), Tints))
	{
		return;
	}
	// Its palette (the cook's colours) over the rig's own texture: two tones from the texture's light.
	const TArray<FName> SlotNames = Skel->GetMaterialSlotNames();
	for (int32 Slot = 0; Slot < Skel->GetNumMaterials(); ++Slot)
	{
		const FString SlotName = SlotNames.IsValidIndex(Slot) ? SlotNames[Slot].ToString().ToLower() : FString();
		TSharedPtr<FJsonObject> Pick;
		for (const TSharedPtr<FJsonValue>& V : *Tints)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FString Match;
			if (!O.IsValid()) continue;
			if (O->TryGetStringField(TEXT("match"), Match) && !Match.IsEmpty())
			{
				if (SlotName.Contains(Match.ToLower())) { Pick = O; break; }
			}
			else if (!Pick.IsValid())
			{
				Pick = O;
			}
		}
		if (!Pick.IsValid()) continue;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, C);
		FString Path;
		const auto Tex = [&](const TCHAR* Field, const TCHAR* Param, const TCHAR* Fallback)
		{
			const FString P = Pick->TryGetStringField(Field, Path) ? Path : FString(Fallback);
			if (UTexture* T = P.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *P)) M->SetTextureParameterValue(Param, T);
		};
		Tex(TEXT("tex"), TEXT("BaseColorTex"), TEXT(""));
		Tex(TEXT("normal"), TEXT("NormalTex"), TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
		Tex(TEXT("glowTex"), TEXT("GlowTex"), TEXT("/Engine/EngineResources/Black.Black"));
		M->SetScalarParameterValue(TEXT("Desaturation"), FblNum(Pick, TEXT("desat"), 1.f));
		M->SetScalarParameterValue(TEXT("Brightness"), FblNum(Pick, TEXT("bright"), 1.f));
		M->SetScalarParameterValue(TEXT("TwoTone"), FblNum(Pick, TEXT("twoTone"), 0.f));
		M->SetScalarParameterValue(TEXT("LightFrom"), FblNum(Pick, TEXT("lightFrom"), 0.f));
		M->SetScalarParameterValue(TEXT("Roughness"), FblNum(Pick, TEXT("rough"), 0.75f));
		M->SetScalarParameterValue(TEXT("GlowStrength"), FblNum(Pick, TEXT("glowStrength"), 0.f));
		M->SetVectorParameterValue(TEXT("Tint"), FblColour(Pick, TEXT("tint"), FLinearColor::White));
		M->SetVectorParameterValue(TEXT("TintLight"), FblColour(Pick, TEXT("light"), FblColour(Pick, TEXT("tint"), FLinearColor::White)));
		M->SetVectorParameterValue(TEXT("GlowColor"), FblColour(Pick, TEXT("glow"), FblHex(TEXT("#7FE07A"))));
		Skel->SetMaterial(Slot, M);
	}
}

// ---------------------------------------------------------------- building the looks

void UWRForestBossBody::BuildLook()
{
	using namespace WRForestBossLookImpl;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!Def || !Skel || !Skel->GetSkeletalMeshAsset() || !Skel->IsVisible())
	{
		return;
	}
	ClearLook();
	bBuilt = true;
	ApplyTint();
	const FName Id = C->CreatureId;
	const float H = Def->HalfHeight, Rad = Def->Radius;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Up = FVector::UpVector;
	const FTransform W = FTransform::Identity;
	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		Out = B.IsNone() ? C->GetActorLocation() : Skel->GetBoneLocation(B);
		return B;
	};
	// The first of several bone names that this rig has (the island's AI rigs and Mixamo's differ).
	const auto BoneAny = [&](std::initializer_list<const TCHAR*> Hints, FVector& Out) -> FName
	{
		for (const TCHAR* Hint : Hints)
		{
			const FName B = BoneAt(Hint, Out);
			if (!B.IsNone()) return B;
		}
		Out = C->GetActorLocation();
		return NAME_None;
	};
	const auto Blocks = [&](TFunctionRef<void(FblKit&)> Fn) { TArray<FWRLookBlock> Out; FblKit Kit(W, Out); Fn(Kit); return Out; };
	const auto AddLight = [&](USceneComponent* Under, const FVector& Rel, const FLinearColor& Colour, float Intensity, float Radius)
	{
		if (!Under) return;
		UPointLightComponent* L = NewObject<UPointLightComponent>(C);
		L->SetupAttachment(Under);
		L->SetRelativeLocation(Rel);
		L->SetLightColor(Colour);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		Lights.Add(L);
	};

	if (Id == TEXT("old_gnarl"))
	{
		// ---- Old Gnarl: an 18 m oak on root legs ----
		FVector Chest, Head, ForeL, ForeR, HandL, HandR, FootL, FootR, ThighL, ShinR;
		const FName ChestB = BoneAny({ TEXT("spine2"), TEXT("spine1"), TEXT("spine") }, Chest);
		const FName HeadB = BoneAny({ TEXT("head") }, Head);
		const FName ForeLB = BoneAny({ TEXT("leftforearm") }, ForeL), ForeRB = BoneAny({ TEXT("rightforearm") }, ForeR);
		const FName HandLB = BoneAny({ TEXT("lefthand") }, HandL), HandRB = BoneAny({ TEXT("righthand") }, HandR);
		const FName FootLB = BoneAny({ TEXT("leftfoot") }, FootL), FootRB = BoneAny({ TEXT("rightfoot") }, FootR);
		const FName ThighLB = BoneAny({ TEXT("leftupleg") }, ThighL), ShinRB = BoneAny({ TEXT("rightleg") }, ShinR);
		const float Plate = H * 0.034f;   // 30 cm on an 18 m oak
		// Chest and back plates: furrowed bark in staggered vertical slabs, 7 across, 3 high, a little proud of the body;
		// the pale wood under each is one heartwood slab with sap beading at its edges.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float Dir = Side == 0 ? 1.f : -1.f;
			const FVector At = Chest + Fwd * Dir * Rad * 0.78f;
			const FRotator Face(0.f, Yaw.Yaw + (Side == 0 ? 0.f : 180.f), 0.f);
			AddGroup(ChestB, At, Face, Blocks([&](FblKit& K)
			{
				for (int32 Col = 0; Col < 7; ++Col)
				{
					for (int32 Row = 0; Row < 3; ++Row)
					{
						const FVector P(Plate * 0.5f, (Col - 3) * Rad * 0.21f + (Row % 2) * Rad * 0.05f, (Row - 1) * H * 0.2f);
						const float Hgt = H * (0.17f + 0.06f * K.Rand(P, 3));
						K.Add(FblBark(0.8f + 0.35f * K.Rand(P, 4)), FVector(Plate * (0.8f + 0.5f * K.Rand(P, 5)), Rad * 0.19f, Hgt), P, FRotator(0.f, 0.f, 4.f * K.Jit(P, 6)));
					}
				}
				// Moss on the upward ledges of the bark.
				K.Add(FblMoss(), FVector(Plate * 1.3f, Rad * 1.3f, H * 0.03f), FVector(Plate * 0.7f, 0.f, H * 0.33f));
			}), FblBark0 + Side);
			AddGroup(ChestB, At, Face, Blocks([&](FblKit& K)
			{
				K.Add(FblPale(1.f), FVector(Plate * 0.6f, Rad * 1.45f, H * 0.62f), FVector(-Plate * 0.2f, 0.f, 0.f));
				for (int32 b = 0; b < 6; ++b)
				{
					K.Add(FblAmber(1.4f), FVector(Plate * 0.5f, Rad * 0.06f, Rad * 0.08f), FVector(Plate * 0.15f, (b - 2.5f) * Rad * 0.24f, -H * 0.28f + (b % 2) * H * 0.04f));
				}
			}), FblPale0 + Side);
		}
		// The mouth-hollow: the trunk split down its front into a deep dark hollow, splintered at its lips.
		AddGroup(ChestB, Chest + Fwd * Rad * 0.82f, Yaw, Blocks([&](FblKit& K)
		{
			K.Add(FblDark(), FVector(Plate * 1.6f, Rad * 0.34f, H * 0.42f), FVector(Plate * 0.6f, 0.f, -H * 0.02f));
			for (int32 s = 0; s < 8; ++s)
			{
				const float Zs = -H * 0.2f + s * H * 0.055f;
				const float Sgn = s % 2 ? 1.f : -1.f;
				K.Spike(FblPale(0.9f), FVector(Plate * 1.3f, Sgn * Rad * 0.19f, Zs), FVector(0.3f, -Sgn, 0.f), Rad * 0.05f, Rad * 0.12f);
			}
		}), FblStatic);
		// Arm plates: four bark slabs round each forearm along its length (pale wood under).
		const FName ForeB[2] = { ForeLB, ForeRB };
		const FVector ForeAt[2] = { ForeL, ForeR }, HandAt[2] = { HandL, HandR };
		for (int32 Arm = 0; Arm < 2; ++Arm)
		{
			const FVector Along = (HandAt[Arm] - ForeAt[Arm]);
			const float Len = FMath::Max(H * 0.2f, Along.Size());
			const FRotator ArmRot = FRotationMatrix::MakeFromX(Along.GetSafeNormal()).Rotator();
			const float ArmR = Rad * 0.32f;
			AddGroup(ForeB[Arm], ForeAt[Arm], ArmRot, Blocks([&](FblKit& K)
			{
				for (int32 q = 0; q < 6; ++q)
				{
					const float A = q * 60.f + 15.f * K.Jit(FVector(q, Arm, 0.f), 1);
					const FVector Out = FRotator(0.f, 0.f, A).RotateVector(FVector(0.f, 0.f, 1.f));
					K.Add(FblBark(0.85f + 0.3f * K.Rand(FVector(q, 0.f, Arm), 2)), FVector(Len * 0.9f, ArmR * 0.95f, Plate), FVector(Len * 0.5f, 0.f, 0.f) + Out * (ArmR + Plate * 0.4f), FRotator(0.f, 0.f, A));
				}
			}), FblBark0 + 2 + Arm);
			AddGroup(ForeB[Arm], ForeAt[Arm], ArmRot, Blocks([&](FblKit& K)
			{
				K.Rod(FblPale(1.05f), FVector(Len * 0.05f, 0.f, 0.f), FVector(Len * 0.95f, 0.f, 0.f), ArmR * 2.1f);
			}), FblPale0 + 2 + Arm);
		}
		// Fists of knotted branches: five gnarled fingers splayed from each hand, twigs at their ends.
		const FName HandB[2] = { HandLB, HandRB };
		for (int32 Arm = 0; Arm < 2; ++Arm)
		{
			FWRToolMesh M;
			const FVector Out = (HandAt[Arm] - ForeAt[Arm]).GetSafeNormal();
			for (int32 f = 0; f < 5; ++f)
			{
				const FVector D = (Out + FRotationMatrix::MakeFromX(Out).GetUnitAxis(EAxis::Y) * (f - 2) * 0.35f + FVector(0.f, 0.f, -0.25f)).GetSafeNormal();
				FblBranch(M, FVector::ZeroVector, D * H * 0.28f, H * 0.03f, H * 0.012f, FblRootC() * 1.05f, f + Arm * 7);
				M.Tube({ D * H * 0.26f, D * H * 0.34f + FVector(0.f, 0.f, H * 0.03f) }, H * 0.006f, 5, FblDeadC(), 0.95f);
			}
			AddMesh(HandB[Arm], HandAt[Arm], FRotator::ZeroRotator, M, FblStatic);
		}
		// Root feet: six roots splaying from each foot into the ground.
		const FName FootB[2] = { FootLB, FootRB };
		const FVector FootAt[2] = { FootL, FootR };
		for (int32 Leg = 0; Leg < 2; ++Leg)
		{
			FWRToolMesh M;
			for (int32 r = 0; r < 6; ++r)
			{
				const FVector D = FRotator(0.f, Yaw.Yaw + r * 60.f + Leg * 25.f, 0.f).Vector();
				M.Tube({ FVector(0.f, 0.f, H * 0.06f), D * H * 0.12f, D * H * 0.24f - FVector(0.f, 0.f, H * 0.03f) }, H * (0.03f - r * 0.002f), 7, FblRootC(), 0.92f);
			}
			AddMesh(FootB[Leg], FootAt[Leg], FRotator::ZeroRotator, M, FblStatic);
		}
		// The clear-cut's axes, still sunk in its legs: a felling axe in the left thigh, two in the right shin.
		const auto Axe = [&](FblKit& K, const FVector& At, const FVector& Out, float Lean)
		{
			const FVector HaftDir = (Out + FVector(0.f, 0.f, Lean)).GetSafeNormal();
			K.Rod(FblHaft(), At, At + HaftDir * 95.f, 6.f);
			K.Add(FblIron(), FVector(22.f, 4.f, 16.f), At - HaftDir * 4.f, HaftDir.Rotation());
		};
		AddGroup(ThighLB, ThighL - Right * Rad * 0.3f, Yaw, Blocks([&](FblKit& K) { Axe(K, FVector(Rad * 0.2f, -Rad * 0.15f, -H * 0.1f), FVector(0.6f, -0.8f, 0.f), 0.4f); }), FblStatic);
		AddGroup(ShinRB, ShinR + Right * Rad * 0.2f, Yaw, Blocks([&](FblKit& K)
		{
			Axe(K, FVector(Rad * 0.15f, Rad * 0.12f, H * 0.05f), FVector(0.7f, 0.7f, 0.f), 0.2f);
			Axe(K, FVector(-Rad * 0.1f, Rad * 0.15f, -H * 0.06f), FVector(-0.3f, 0.9f, 0.f), -0.3f);
		}), FblStatic);
		// The amber knothole eyes high on the bole.
		if (USceneComponent* Eyes = AddGroup(HeadB, Head + Fwd * Rad * 0.35f, Yaw, Blocks([&](FblKit& K)
		{
			for (const float S : { -1.f, 1.f })
			{
				K.Disc(FblBark(0.7f), FVector(0.f, S * Rad * 0.14f, 0.f), FVector::ForwardVector, Rad * 0.16f, Rad * 0.05f);
				K.Disc(FblAmber(4.f), FVector(Rad * 0.02f, S * Rad * 0.14f, 0.f), FVector::ForwardVector, Rad * 0.09f, Rad * 0.03f);
			}
		}), FblStatic))
		{
			AddLight(Eyes, FVector(Rad * 0.15f, 0.f, 0.f), FblRgb(208, 138, 30), 2500.f, H * 0.6f);
		}
		// Its crown: an oak's bole and crown rising out of its shoulders (it is the tree), thrashing as it moves.
		AddKitMesh(ChestB, Chest - Fwd * Rad * 0.35f + Up * H * 0.25f, FRotator(0.f, Yaw.Yaw + 40.f, 0.f), TEXT("oak"), 1, H * 1.15f, FblCrown);
		return;
	}

	if (Id == TEXT("bramblequeen"))
	{
		// ---- The Bramblequeen: a 4 m giant hornet ----
		FVector Head, Thorax, Abd;
		const FName HeadB = BoneAny({ TEXT("head") }, Head);
		const FName ThoraxB = BoneAny({ TEXT("spine3"), TEXT("spine2"), TEXT("spine") }, Thorax);
		const FName AbdB = BoneAny({ TEXT("pelvis"), TEXT("spine1"), TEXT("hips") }, Abd);
		const FLinearColor Orange = FblRgb(216, 112, 30) * 0.6f, Black = FblRgb(22, 18, 14) * 0.6f, Band = FblRgb(200, 150, 40) * 0.6f;
		// The head: a huge orange cap, black eyes, mandibles as long as a forearm.
		{
			FWRToolMesh M;
			TArray<FVector> Prof;
			for (int32 i = 0; i <= 8; ++i) { const float T = i / 8.f; const float R = H * 0.42f * FMath::Pow(FMath::Max(0.f, FMath::Sin(T * PI)), 0.7f) + 2.f; Prof.Add(FVector(T, R * 1.15f, R)); }
			M.Lathe(FVector(-H * 0.2f, 0.f, 0.f), FVector::ForwardVector, H * 0.6f, Prof, 12, Orange, 0.3f);
			for (const float S : { -1.f, 1.f })
			{
				M.Lathe(FVector(H * 0.05f, S * H * 0.3f, H * 0.08f), FVector(0.f, S, 0.f), H * 0.12f, { FVector(0.f, 2.f, 2.f), FVector(0.5f, H * 0.12f, H * 0.16f), FVector(1.f, 2.f, 2.f) }, 10, Black, 0.1f);
				// The mandible: a curved blade that spreads before a shear.
				M.Tube({ FVector(H * 0.36f, S * H * 0.12f, -H * 0.12f), FVector(H * 0.62f, S * H * 0.2f, -H * 0.16f), FVector(H * 0.82f, S * H * 0.08f, -H * 0.14f) }, H * 0.035f, 7, Black * 1.4f, 0.25f);
			}
			AddMesh(HeadB, Head + Fwd * H * 0.15f, Yaw, M, FblStatic);
		}
		// The thorax: glossy black, ridged.
		AddGroup(ThoraxB, Thorax + Up * H * 0.15f, Yaw, Blocks([&](FblKit& K)
		{
			for (int32 i = 0; i < 4; ++i) K.Add(FblChitin(Black), FVector(H * 0.22f, H * 0.62f - i * H * 0.05f, H * 0.1f), FVector(H * (0.15f - i * 0.12f), 0.f, H * 0.12f * FMath::Sin(i * 0.9f)), FRotator(-6.f * i, 0.f, 0.f));
		}), FblStatic);
		// The abdomen: a barrel of amber and black bands curling under, the stinger glistening at its tip.
		{
			FWRToolMesh M;
			const int32 Bands = 6;
			for (int32 b = 0; b < Bands; ++b)
			{
				const float T0 = b / float(Bands), T1 = (b + 1) / float(Bands);
				TArray<FVector> Prof;
				for (int32 i = 0; i <= 3; ++i)
				{
					const float T = FMath::Lerp(T0, T1, i / 3.f);
					const float R = H * 0.55f * FMath::Sin(FMath::Min(1.f, T * 1.15f + 0.08f) * PI * 0.92f) + 3.f;
					Prof.Add(FVector(i / 3.f, R, R * 0.9f));
				}
				M.Lathe(FVector(-H * 1.4f * T0, 0.f, -H * 0.25f * T0 * T0), (FVector(-1.f, 0.f, -0.35f * (T0 + T1))).GetSafeNormal(), H * 1.4f / Bands, Prof, 12, b % 2 ? Black : Band, 0.3f);
			}
			M.Lathe(FVector(-H * 1.38f, 0.f, -H * 0.36f), FVector(-0.7f, 0.f, -0.7f).GetSafeNormal(), H * 0.5f, { FVector(0.f, H * 0.05f, H * 0.05f), FVector(1.f, 0.5f, 0.5f) }, 8, FblRgb(30, 24, 18) * 0.5f, 0.08f);
			AddMesh(AbdB, Abd - Fwd * H * 0.1f, Yaw, M, FblAbdomen);
		}
		// Four wings, 6 m across: thin amber-smoked panes with dark veins (they beat; they stop before the gust).
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float S = Side == 0 ? -1.f : 1.f;
			AddGroup(ThoraxB, Thorax + Up * H * 0.3f + Right * S * H * 0.2f, Yaw, Blocks([&](FblKit& K)
			{
				for (int32 Pair = 0; Pair < 2; ++Pair)
				{
					const float Span = Pair == 0 ? 280.f : 200.f;
					const float Back = Pair == 0 ? 0.f : -H * 0.35f;
					const FRotator Sweep(0.f, S * (Pair == 0 ? 78.f : 100.f), 0.f);
					K.Add(FblSurf::Glass(FblRgb(150, 120, 70) * 0.35f, 0.f), FVector(Span * 0.34f, Span, 1.5f), FVector(Back, 0.f, 0.f) + Sweep.RotateVector(FVector(0.f, Span * 0.5f, 0.f)) * FVector(1.f, 1.f, 0.f), FRotator(0.f, Sweep.Yaw - S * 90.f, 0.f));
					for (int32 v = 0; v < 3; ++v)
					{
						const FVector A = FVector(Back, 0.f, 1.f), B = FVector(Back, 0.f, 1.f) + Sweep.RotateVector(FVector(0.f, Span * (0.95f - v * 0.1f), 0.f)) + FVector((v - 1) * Span * 0.1f, 0.f, 0.f);
						K.Rod(FblSurf::Stone(FblRgb(40, 30, 20) * 0.4f, 0.5f), A, B, 2.f);
					}
				}
			}), Side == 0 ? FblWingL : FblWingR);
		}
		return;
	}

	if (Id == TEXT("grey_matron"))
	{
		// ---- The Grey Matron: yellowed sabres, a torn ear, old scars ----
		FVector Head;
		const FName HeadB = BoneAny({ TEXT("head") }, Head);
		AddGroup(HeadB, Head + Fwd * H * 0.32f - Up * H * 0.1f, Yaw, Blocks([&](FblKit& K)
		{
			for (const float S : { -1.f, 1.f })
			{
				K.Spike(FblIvory(0.85f), FVector(0.f, S * H * 0.07f, 0.f), FVector(0.25f, S * 0.05f, -1.f), H * 0.045f, H * 0.24f);
			}
			// Scars across the muzzle and brow: pale seams in the grey.
			K.Rod(FblIvory(1.1f), FVector(-H * 0.08f, -H * 0.12f, H * 0.16f), FVector(H * 0.05f, H * 0.08f, H * 0.06f), H * 0.012f);
			K.Rod(FblIvory(1.1f), FVector(-H * 0.14f, H * 0.04f, H * 0.2f), FVector(-H * 0.02f, H * 0.14f, H * 0.12f), H * 0.01f);
		}), FblStatic);
		AddGroup(HeadB, Head + Up * H * 0.22f - Fwd * H * 0.05f, Yaw, Blocks([&](FblKit& K)
		{
			// The left ear's black tuft; the right ear torn off short (a ragged stub).
			K.Spike(FblFur(0.7f), FVector(0.f, -H * 0.12f, H * 0.08f), FVector(0.f, -0.15f, 1.f), H * 0.05f, H * 0.16f);
			K.Add(FblFur(1.1f), FVector(H * 0.05f, H * 0.04f, H * 0.05f), FVector(0.f, H * 0.12f, H * 0.03f), FRotator(10.f, 0.f, 25.f));
		}), FblStatic);
		return;
	}

	if (Id == TEXT("hornwarden"))
	{
		// ---- The Hornwarden: the antler crown wider than a house ----
		FVector Head, Neck, Flank;
		const FName HeadB = BoneAny({ TEXT("head") }, Head);
		const FName NeckB = BoneAny({ TEXT("neck") }, Neck);
		const FName FlankB = BoneAny({ TEXT("spine1"), TEXT("spine"), TEXT("pelvis") }, Flank);
		const float Span = WRTune(TEXT("bosses.hornwarden.antlerSpan"), 800.f) * 0.5f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float S = Side == 0 ? -1.f : 1.f;
			FWRToolMesh M;
			// The beam: out of the skull, up and back, then the palm: broad flat plates with tines along its rim.
			const FVector B0(0.f, S * H * 0.06f, H * 0.05f), B1(-Span * 0.15f, S * Span * 0.35f, Span * 0.35f), B2(-Span * 0.25f, S * Span * 0.7f, Span * 0.5f);
			M.Tube({ B0, (B0 + B1) * 0.5f + FVector(0.f, 0.f, Span * 0.05f), B1, B2 }, Span * 0.045f, 9, FblAntlerC(), 0.6f);
			// The palm: a lofted plate between the beam's top and the tines' roots.
			TArray<TArray<FVector>> Rings;
			for (int32 r = 0; r < 4; ++r)
			{
				const float T = r / 3.f;
				const FVector Mid = FMath::Lerp(B1, B2 + FVector(-Span * 0.1f, S * Span * 0.2f, Span * 0.1f), T);
				const float Wd = Span * (0.18f + 0.12f * FMath::Sin(T * PI));
				Rings.Add({ Mid + FVector(Wd, 0.f, 0.f), Mid + FVector(0.f, 0.f, Span * 0.03f), Mid - FVector(Wd, 0.f, 0.f), Mid - FVector(0.f, 0.f, Span * 0.03f) });
			}
			M.Loft(Rings, FblAntlerC() * 0.95f, 0.65f, true);
			// Eight tines off the palm's upper rim and two brow tines forward.
			for (int32 t = 0; t < 8; ++t)
			{
				const float T = t / 7.f;
				const FVector Root = FMath::Lerp(B1, B2 + FVector(-Span * 0.1f, S * Span * 0.2f, Span * 0.1f), T) + FVector(Span * 0.12f * FMath::Cos(T * PI), 0.f, Span * 0.02f);
				const FVector Tip = Root + FVector(Span * 0.06f * (t % 2 ? 1.f : -1.f), S * Span * 0.05f, Span * (0.22f + 0.08f * FMath::Sin(T * 5.f)));
				M.Tube({ Root, (Root + Tip) * 0.5f + FVector(0.f, S * Span * 0.02f, 0.f), Tip }, Span * 0.018f, 6, FblAntlerC() * 1.05f, 0.55f);
			}
			M.Tube({ B0 + FVector(0.f, S * Span * 0.08f, Span * 0.08f), B0 + FVector(Span * 0.2f, S * Span * 0.12f, Span * 0.12f), B0 + FVector(Span * 0.3f, S * Span * 0.1f, Span * 0.2f) }, Span * 0.02f, 6, FblAntlerC(), 0.6f);
			// Ivy wound up the beam, moss on the palm.
			TArray<FVector> Ivy;
			for (int32 i = 0; i <= 16; ++i)
			{
				const float T = i / 16.f;
				const FVector P = FMath::Lerp(B0, B2, T);
				Ivy.Add(P + FVector(FMath::Cos(T * 18.f), FMath::Sin(T * 18.f), FMath::Sin(T * 18.f + 1.f)) * Span * 0.055f);
			}
			M.Tube(Ivy, Span * 0.008f, 5, FblVineC(), 0.85f);
			AddMesh(HeadB, Head + Up * H * 0.12f, Yaw, M, Side == 0 ? FblAntlerL : FblAntlerR);
			// The rags of old Grovekin hunting banners and the ivy's leaves hanging off the tines (blocks, so they read).
			AddGroup(HeadB, Head + Up * H * 0.12f, Yaw, Blocks([&](FblKit& K)
			{
				for (int32 g = 0; g < 4; ++g)
				{
					const float T = (g + 0.5f) / 4.f;
					const FVector Top = FMath::Lerp(B1, B2, T) + FVector(0.f, 0.f, Span * 0.18f);
					K.Add(FblRag(g + Side), FVector(Span * 0.06f, 2.f, Span * (0.18f + 0.08f * float(g % 2))), Top - FVector(0.f, 0.f, Span * 0.12f), FRotator(0.f, S * 20.f, 6.f * g));
				}
				for (int32 l = 0; l < 14; ++l)
				{
					const float T = l / 13.f;
					K.Add(FblMoss(1.2f), FVector(Span * 0.035f, Span * 0.03f, 1.5f), FMath::Lerp(B0, B2, T) + FVector(0.f, S * Span * 0.04f, Span * 0.05f), FRotator(30.f * T, 47.f * l, 20.f));
				}
			}), Side == 0 ? FblAntlerL : FblAntlerR);
		}
		// The mane: a thick dark ruff down the neck (it stands up when he bellows or rages).
		AddGroup(NeckB, Neck + Up * H * 0.18f, Yaw, Blocks([&](FblKit& K)
		{
			for (int32 i = 0; i < 14; ++i)
			{
				const FVector P(-H * 0.05f * i + H * 0.25f, (i % 2 ? 1.f : -1.f) * H * 0.05f, -H * 0.01f * i);
				K.Spike(FblFur(0.8f + 0.3f * K.Rand(P, 2)), P, FVector(-0.4f, (i % 2 ? 0.3f : -0.3f), 1.f), H * 0.08f, H * (0.16f + 0.06f * K.Rand(P, 3)));
			}
			// The lump of the heartwood spear-head under the skin of his neck, its point just through the hide.
			K.Add(FblFur(1.2f), FVector(H * 0.09f, H * 0.07f, H * 0.06f), FVector(H * 0.1f, H * 0.12f, -H * 0.12f), FRotator(0.f, 20.f, 10.f));
			K.Spike(FblHaft(), FVector(H * 0.12f, H * 0.16f, -H * 0.12f), FVector(0.3f, 1.f, 0.2f), H * 0.025f, H * 0.05f);
		}), FblMane);
		// Old arrows in his flank, broken shafts and grey fletching.
		AddGroup(FlankB, Flank + Right * Rad * 0.7f, Yaw, Blocks([&](FblKit& K)
		{
			for (int32 a = 0; a < 4; ++a)
			{
				const FVector Root(-H * 0.12f * a, 0.f, H * (0.05f + 0.04f * (a % 2)));
				const FVector D = FVector(-0.4f, 1.f, 0.15f * a).GetSafeNormal();
				const float L = H * (0.18f + 0.06f * (a % 3));
				K.Rod(FblHaft(), Root, Root + D * L, 2.5f);
				K.Add(FblSurf::Cloth(FblRgb(140, 136, 128) * 0.45f), FVector(12.f, 1.f, 6.f), Root + D * (L - 8.f), D.Rotation());
			}
		}), FblStatic);
		return;
	}

	if (Id == TEXT("sylvaroth"))
	{
		// ---- Sylvaroth, the World-Root: a strangler-fig titan ----
		FVector Hips, Spine1, Spine2, Head, ArmL, ForeL, HandL, ArmR, ForeR, HandR;
		const FName HipsB = BoneAny({ TEXT("hips"), TEXT("pelvis") }, Hips);
		const FName Spine1B = BoneAny({ TEXT("spine1"), TEXT("spine") }, Spine1);
		const FName Spine2B = BoneAny({ TEXT("spine2"), TEXT("spine1") }, Spine2);
		const FName HeadB = BoneAny({ TEXT("head") }, Head);
		const FName ArmLB = BoneAny({ TEXT("leftarm") }, ArmL), ForeLB = BoneAny({ TEXT("leftforearm") }, ForeL), HandLB = BoneAny({ TEXT("lefthand") }, HandL);
		const FName ArmRB = BoneAny({ TEXT("rightarm") }, ArmR), ForeRB = BoneAny({ TEXT("rightforearm") }, ForeR), HandRB = BoneAny({ TEXT("righthand") }, HandR);
		const FLinearColor Lattice = FblRgb(90, 74, 58) * 0.5f;
		// The cage: hoops of fused root round the torso at each spine bone, braided diagonals between them.
		const FName CageB[2] = { Spine1B, Spine2B };
		const FVector CageAt[2] = { Spine1, Spine2 };
		for (int32 c = 0; c < 2; ++c)
		{
			FWRToolMesh M;
			const float CageR = Rad * (c == 0 ? 0.95f : 1.05f);
			for (int32 h = 0; h < 3; ++h)
			{
				TArray<FVector> Hoop;
				const float Z = (h - 1) * H * 0.12f;
				for (int32 i = 0; i < 14; ++i)
				{
					const float A = 2.f * PI * i / 14.f;
					Hoop.Add(FVector(FMath::Cos(A) * CageR * 0.85f, FMath::Sin(A) * CageR, Z + FMath::Sin(A * 3.f + h) * H * 0.02f));
				}
				M.Tube(Hoop, Rad * 0.06f, 7, Lattice * (0.9f + 0.1f * h), 0.92f, true);
			}
			for (int32 d = 0; d < 10; ++d)
			{
				const float A0 = 2.f * PI * d / 10.f, A1 = A0 + (d % 2 ? 0.7f : -0.7f);
				M.Tube({ FVector(FMath::Cos(A0) * CageR * 0.85f, FMath::Sin(A0) * CageR, -H * 0.13f), FVector(FMath::Cos((A0 + A1) * 0.5f) * CageR * 0.92f, FMath::Sin((A0 + A1) * 0.5f) * CageR * 1.05f, 0.f),
					FVector(FMath::Cos(A1) * CageR * 0.85f, FMath::Sin(A1) * CageR, H * 0.13f) }, Rad * 0.045f, 6, Lattice, 0.92f);
			}
			AddMesh(CageB[c], CageAt[c], Yaw, M, FblStatic);
		}
		// Inside the cage: foxfire glowing pale green, and Grovekin burials (bark-wrapped bodies in root cradles).
		if (USceneComponent* Core = AddGroup(Spine1B, Spine1, Yaw, Blocks([&](FblKit& K)
		{
			for (int32 g = 0; g < 9; ++g)
			{
				const FVector P(Rad * 0.3f * FMath::Cos(g * 2.1f), Rad * 0.4f * FMath::Sin(g * 1.3f), H * 0.12f * FMath::Sin(g * 0.8f));
				K.Add(FblFox(3.f + K.Rand(P, 1) * 2.f), FVector(Rad * 0.12f, Rad * 0.1f, H * 0.04f), P, FRotator(30.f * g, 50.f * g, 0.f));
			}
			for (int32 b = 0; b < 3; ++b)
			{
				const FVector P(Rad * 0.1f, (b - 1) * Rad * 0.45f, -H * 0.05f + b * H * 0.06f);
				K.Add(FblBark(0.75f), FVector(Rad * 0.16f, Rad * 0.16f, H * 0.17f), P, FRotator(10.f * (b - 1), 0.f, 18.f * (b - 1)));
				K.Add(FblSurf::Cloth(FblRgb(120, 100, 70) * 0.4f), FVector(Rad * 0.17f, Rad * 0.17f, H * 0.02f), P + FVector(0.f, 0.f, H * 0.05f), FRotator(10.f * (b - 1), 0.f, 18.f * (b - 1)));
			}
		}), FblFoxfire))
		{
			AddLight(Core, FVector::ZeroVector, FblRgb(127, 224, 122), 9000.f, H * 1.4f);
		}
		// The head: a burl the size of a cottage split into a jaw of splintered heartwood fangs; amber eye hollows.
		{
			FWRToolMesh M;
			TArray<FVector> Prof;
			for (int32 i = 0; i <= 10; ++i) { const float T = i / 10.f; const float R = Rad * 0.62f * FMath::Pow(FMath::Max(0.f, FMath::Sin(T * PI)), 0.6f) * (1.f + 0.08f * FMath::Sin(T * 13.f)) + 4.f; Prof.Add(FVector(T, R, R * 0.85f)); }
			M.Lathe(FVector(-Rad * 0.35f, 0.f, 0.f), FVector::ForwardVector, Rad * 1.2f, Prof, 14, FblRgb(70, 56, 42) * 0.5f, 0.9f, Rad * 0.05f, 3.f);
			AddMesh(HeadB, Head + Fwd * Rad * 0.15f, Yaw, M, FblStatic);
			if (USceneComponent* Face = AddGroup(HeadB, Head + Fwd * Rad * 0.7f, Yaw, Blocks([&](FblKit& K)
			{
				K.Add(FblDark(), FVector(Rad * 0.2f, Rad * 0.55f, Rad * 0.14f), FVector(0.f, 0.f, -Rad * 0.18f));
				for (int32 f = 0; f < 9; ++f)
				{
					const float Y = (f - 4) * Rad * 0.06f;
					K.Spike(FblPale(0.9f), FVector(Rad * 0.05f, Y, -Rad * 0.1f), FVector(0.2f, 0.f, -1.f), Rad * 0.05f, Rad * (0.12f + 0.06f * (f % 3)));
					K.Spike(FblPale(0.8f), FVector(Rad * 0.05f, Y + Rad * 0.03f, -Rad * 0.27f), FVector(0.2f, 0.f, 1.f), Rad * 0.04f, Rad * (0.08f + 0.05f * (f % 2)));
				}
				for (const float S : { -1.f, 1.f })
				{
					K.Disc(FblDark(), FVector(-Rad * 0.02f, S * Rad * 0.2f, Rad * 0.1f), FVector::ForwardVector, Rad * 0.2f, Rad * 0.06f);
					K.Disc(FblAmber(5.f), FVector(Rad * 0.01f, S * Rad * 0.2f, Rad * 0.1f), FVector::ForwardVector, Rad * 0.11f, Rad * 0.04f);
				}
			}), FblStatic))
			{
				AddLight(Face, FVector(Rad * 0.1f, 0.f, Rad * 0.1f), FblRgb(208, 138, 30), 6000.f, H * 0.8f);
			}
		}
		// The lattice arms: three braided roots along each arm, root-claws splaying at the hands.
		const FName ArmB[2][3] = { { ArmLB, ForeLB, HandLB }, { ArmRB, ForeRB, HandRB } };
		const FVector ArmAt[2][3] = { { ArmL, ForeL, HandL }, { ArmR, ForeR, HandR } };
		for (int32 Arm = 0; Arm < 2; ++Arm)
		{
			for (int32 Seg = 0; Seg < 2; ++Seg)
			{
				const FVector A = ArmAt[Arm][Seg], B = ArmAt[Arm][Seg + 1];
				const FVector D = B - A;
				const FRotator Rot = FRotationMatrix::MakeFromX(D.GetSafeNormal()).Rotator();
				const float Len = D.Size();
				FWRToolMesh M;
				for (int32 s = 0; s < 3; ++s)
				{
					TArray<FVector> Strand;
					for (int32 i = 0; i <= 10; ++i)
					{
						const float T = i / 10.f, Ang = s * 2.1f + T * 6.f;
						Strand.Add(FVector(Len * T, FMath::Cos(Ang) * Rad * 0.12f, FMath::Sin(Ang) * Rad * 0.12f));
					}
					M.Tube(Strand, Rad * 0.065f, 7, Lattice * (0.95f + 0.05f * s), 0.92f);
				}
				AddMesh(ArmB[Arm][Seg], A, Rot, M, FblStatic);
			}
			FWRToolMesh Claw;
			const FVector Out = (ArmAt[Arm][2] - ArmAt[Arm][1]).GetSafeNormal();
			for (int32 f = 0; f < 6; ++f)
			{
				const FVector D = (Out + FRotationMatrix::MakeFromX(Out).GetUnitAxis(EAxis::Y) * (f - 2.5f) * 0.3f + FVector(0.f, 0.f, -0.5f)).GetSafeNormal();
				FblBranch(Claw, FVector::ZeroVector, D * Rad * 0.9f, Rad * 0.07f, Rad * 0.025f, Lattice, f + Arm * 11);
			}
			AddMesh(ArmB[Arm][2], ArmAt[Arm][2], FRotator::ZeroRotator, Claw, FblStatic);
		}
		// The hump of living canopy on its back: moss, ferns, saplings, a few dead branches (a hill with trees on it).
		const FVector Back = Spine2 - Fwd * Rad * 0.7f + Up * H * 0.12f;
		AddGroup(Spine2B, Back, Yaw, Blocks([&](FblKit& K)
		{
			for (int32 m = 0; m < 10; ++m)
			{
				const FVector P(-Rad * 0.08f * m, (m % 3 - 1) * Rad * 0.3f, H * 0.03f * FMath::Sin(m * 0.7f));
				K.Add(FblMoss(0.85f + 0.3f * K.Rand(P, 1)), FVector(Rad * 0.5f, Rad * 0.45f, H * 0.04f), P, FRotator(-12.f, 20.f * m, 6.f * (m % 2)));
			}
		}), FblCrown);
		static const TCHAR* Plants[] = { TEXT("sapling"), TEXT("fern"), TEXT("sapling"), TEXT("fern"), TEXT("hazel"), TEXT("fern"), TEXT("sapling") };
		for (int32 p = 0; p < 7; ++p)
		{
			const FVector At = Back + Fwd * (-Rad * 0.12f * p) + Right * ((p % 3) - 1) * Rad * 0.32f + Up * H * 0.04f;
			const bool bFern = FCString::Strcmp(Plants[p], TEXT("fern")) == 0;
			AddKitMesh(Spine2B, At, FRotator(0.f, Yaw.Yaw + 51.f * p, 0.f), Plants[p], p % 4, bFern ? H * 0.12f : H * (0.35f + 0.08f * (p % 2)), FblCrown);
		}
		{
			FWRToolMesh M;
			for (int32 d = 0; d < 4; ++d)
			{
				const FVector A(-Rad * 0.2f * d, (d % 2 ? 1.f : -1.f) * Rad * 0.25f, H * 0.03f);
				FblBranch(M, A, A + FVector(-Rad * 0.3f, (d % 2 ? 1.f : -1.f) * Rad * 0.4f, H * 0.25f), Rad * 0.04f, Rad * 0.015f, FblDeadC(), d * 3);
			}
			AddMesh(Spine2B, Back, Yaw, M, FblCrown);
		}
		// The pods that swell on its back before the seed bombs (shown only then).
		{
			FWRToolMesh M;
			for (int32 p = 0; p < 6; ++p)
			{
				TArray<FVector> Prof;
				for (int32 i = 0; i <= 8; ++i) { const float T = i / 8.f; const float R = Rad * 0.12f * FMath::Pow(FMath::Max(0.f, FMath::Sin(T * PI)), 0.8f) + 2.f; Prof.Add(FVector(T, R, R * 0.9f)); }
				M.Lathe(FVector(-Rad * 0.15f * p, ((p % 2) ? 1.f : -1.f) * Rad * 0.35f, H * 0.04f), FVector::UpVector, Rad * 0.28f, Prof, 9, FblRgb(150, 96, 40) * 0.55f, 0.6f);
			}
			AddMesh(Spine2B, Back, Yaw, M, FblPods);
		}
		// The root mass it stands in (phase 1): thick roots out of its hips, spreading and diving into the moss.
		{
			FWRToolMesh M;
			for (int32 r = 0; r < 10; ++r)
			{
				const FVector D = FRotator(0.f, r * 36.f + 10.f, 0.f).Vector();
				M.Tube({ FVector(0.f, 0.f, -H * 0.05f), D * Rad * 0.6f + FVector(0.f, 0.f, -H * 0.1f), D * Rad * 1.3f + FVector(0.f, 0.f, -H * 0.25f), D * Rad * 1.7f + FVector(0.f, 0.f, -H * 0.6f) }, Rad * (0.12f - 0.005f * r), 8, Lattice * 0.95f, 0.92f);
			}
			AddMesh(HipsB, Hips, Yaw, M, FblRootMass);
		}
		return;
	}
}

// ---------------------------------------------------------------- every frame

void UWRForestBossBody::TickLook(float DeltaTime)
{
	using namespace WRForestBossLookImpl;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Skel = C ? C->GetCreatureSkel() : nullptr;
	if (!C || !Skel)
	{
		return;
	}
	if (!bBuilt)
	{
		// Built once its mesh is on and fitted (the server may send the id after this component).
		Age += DeltaTime;
		if (Skel->GetSkeletalMeshAsset() && Skel->IsVisible() && Age > 0.4f) BuildLook();
		return;
	}
	const FName Id = C->CreatureId;
	const bool bAlive = C->IsAlive();
	const FVector Vel = C->GetVelocity();
	if (Id == TEXT("old_gnarl"))
	{
		// Bark or pale wood per plate; fire licking a burning spot.
		if (Stripped != ShownStripped || Burning != ShownBurning)
		{
			for (int32 P = 0; P < 4; ++P)
			{
				const bool bStrip = (Stripped & (1 << P)) != 0;
				SetKindVisible(FblBark0 + P, !bStrip);
				SetKindVisible(FblPale0 + P, bStrip);
			}
			PlateFx.SetNum(4);
			for (int32 P = 0; P < 4; ++P)
			{
				const bool bBurn = (Burning & (1 << P)) != 0;
				if (bBurn && !PlateFx[P])
				{
					for (const FGroup& G : Groups)
					{
						if (G.Kind == FblPale0 + P && G.Frame.IsValid()) { PlateFx[P] = WRFx::Attach(G.Frame.Get(), TEXT("gnarl.plate_burn"), NAME_None, C->GetSimpleCollisionRadius() / 200.f); break; }
					}
				}
				else if (!bBurn && PlateFx[P])
				{
					WRFx::Stop(PlateFx[P]);
					PlateFx[P] = nullptr;
				}
			}
			ShownStripped = Stripped;
			ShownBurning = Burning;
		}
		// The crown thrashes as it moves, leans into a swing.
		const float Thrash = FMath::Clamp(Vel.Size2D() / 400.f, 0.f, 1.f) * 6.f + (Pose == 1 ? 0.f : 1.5f);
		for (FGroup& G : Groups)
		{
			if (G.Kind != FblCrown || !G.Frame.IsValid()) continue;
			G.Frame->SetRelativeRotation(G.BaseRot + FRotator(FMath::Sin(Clock * 1.7f) * Thrash, 0.f, FMath::Sin(Clock * 1.1f + 1.f) * Thrash));
		}
		return;
	}
	if (Id == TEXT("bramblequeen"))
	{
		// The wings beat (a blur of a throb); stopped for the gust's deep beat, still when she's dead.
		const bool bBeat = bAlive && Pose != 4;
		const float Flap = bBeat ? FMath::Sin(Clock * 55.f) * 28.f : (Pose == 4 ? -20.f : -35.f);
		for (FGroup& G : Groups)
		{
			if (!G.Frame.IsValid()) continue;
			if (G.Kind == FblWingL) G.Frame->SetRelativeRotation(G.BaseRot + FRotator(0.f, 0.f, -Flap));
			else if (G.Kind == FblWingR) G.Frame->SetRelativeRotation(G.BaseRot + FRotator(0.f, 0.f, Flap));
			else if (G.Kind == FblAbdomen)
			{
				// Curled under before the sting, fanning for the mark, bowed over a comb, stuck: pulled forward taut.
				const float Curl = Pose == 1 ? 38.f : Pose == 2 ? 10.f * FMath::Sin(Clock * 9.f) : Pose == 3 ? 18.f : bStuck ? 30.f : 4.f * FMath::Sin(Clock * 2.f);
				G.Frame->SetRelativeRotation(FMath::RInterpTo(G.Frame->GetRelativeRotation(), G.BaseRot + FRotator(-Curl, 0.f, 0.f), DeltaTime, 10.f));
			}
		}
		return;
	}
	if (Id == TEXT("hornwarden"))
	{
		// A broken antler is gone (it lies where it fell: the break's own effect); his mane stands up in a rage or a bellow.
		const bool bLeft = !C->IsPartBroken(TEXT("antler_left")), bRight = !C->IsPartBroken(TEXT("antler_right"));
		if (bLeft != bAntlerShown[0]) { SetKindVisible(FblAntlerL, bLeft); bAntlerShown[0] = bLeft; }
		if (bRight != bAntlerShown[1]) { SetKindVisible(FblAntlerR, bRight); bAntlerShown[1] = bRight; }
		const float Raise = (bEnraged || Pose == 1) ? 1.f : 0.f;
		for (FGroup& G : Groups)
		{
			if (G.Kind != FblMane || !G.Frame.IsValid()) continue;
			const FVector Want(1.f, 1.f, 1.f + 0.45f * Raise);
			G.Frame->SetRelativeScale3D(FMath::VInterpTo(G.Frame->GetRelativeScale3D(), Want, DeltaTime, 6.f));
		}
		return;
	}
	if (Id == TEXT("sylvaroth"))
	{
		// The root mass shows while it stands rooted; the pods only while they swell; the canopy shakes like a wet dog's
		// coat in the shake; the foxfire inside breathes.
		SetKindVisible(FblRootMass, bRooted);
		SetKindVisible(FblPods, Pose == 3);
		const float Shake = Pose == 1 ? 9.f : 1.2f;
		const float Rate = Pose == 1 ? 14.f : 0.9f;
		for (FGroup& G : Groups)
		{
			if (!G.Frame.IsValid()) continue;
			if (G.Kind == FblCrown) G.Frame->SetRelativeRotation(G.BaseRot + FRotator(FMath::Sin(Clock * Rate) * Shake, 0.f, FMath::Sin(Clock * Rate * 1.3f + 0.6f) * Shake));
			if (G.Kind == FblPods)
			{
				const float Swell = 0.6f + 0.4f * FMath::Clamp(FMath::Fmod(Clock, 1.f), 0.f, 1.f);
				G.Frame->SetRelativeScale3D(FVector(Swell));
			}
		}
		for (UPointLightComponent* L : Lights)
		{
			if (L && L->GetLightColor().G > L->GetLightColor().R) L->SetIntensity(bAlive ? 9000.f * (0.8f + 0.2f * FMath::Sin(Clock * 1.3f)) : 0.f);
		}
	}
}

void UWRForestBossBody::TickSectors()
{
	using namespace WRForestBossLookImpl;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || C->CreatureId != TEXT("sylvaroth") || Sectors.Num() != 6 || SectorCentre.IsZero())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (SectorMesh.Num() != 6)
	{
		// Each sector's vines, built once on this machine: a dozen thorned vines curling over the moss from the clearing's
		// middle outward, following the ground.
		SectorMesh.Reset();
		SectorShown.Init(255, 6);
		SectorChanged.Init(0.f, 6);
		SectorFx.Init(nullptr, 6);
		for (int32 s = 0; s < 6; ++s)
		{
			FWRToolMesh M;
			const FVector Centre = FVector(SectorCentre);
			for (int32 v = 0; v < 12; ++v)
			{
				const float Seed = s * 13.f + v * 3.7f;
				const float A0 = FMath::DegreesToRadians(60.f * s + 5.f + 50.f * (0.5f + 0.5f * FMath::Sin(Seed)));
				const float R0 = SectorRadius * (0.15f + 0.75f * (0.5f + 0.5f * FMath::Sin(Seed * 1.7f)));
				TArray<FVector> Pts;
				for (int32 i = 0; i <= 10; ++i)
				{
					const float T = i / 10.f;
					const float A = A0 + 0.25f * FMath::Sin(T * 4.f + Seed);
					const float R = R0 + 380.f * T;
					const FVector Flat = Centre + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R + FVector(FMath::Cos(T * 9.f + Seed), FMath::Sin(T * 9.f + Seed), 0.f) * 45.f;
					FHitResult Hit;
					FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSylvarothVine), false, C);
					const bool bGround = World->LineTraceSingleByChannel(Hit, Flat + FVector(0.f, 0.f, 3000.f), Flat - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q);
					Pts.Add((bGround ? FVector(Hit.ImpactPoint) : Flat) - Centre + FVector(0.f, 0.f, 8.f + 10.f * FMath::Sin(T * 7.f + Seed)));
				}
				M.Tube(Pts, 7.f + 2.f * FMath::Sin(Seed), 6, FblVineC() * (0.85f + 0.2f * FMath::Sin(Seed * 3.f)), 0.85f);
				for (int32 t = 1; t < 10; t += 2)
				{
					const FVector P = Pts[t];
					M.Tube({ P, P + FVector(FMath::Cos(t + Seed), FMath::Sin(t + Seed), 0.6f) * 22.f }, 2.f, 4, FblVineC() * 0.6f, 0.8f);
				}
			}
			UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
			P->SetupAttachment(C->GetRootComponent());
			P->SetUsingAbsoluteLocation(true);
			P->SetUsingAbsoluteRotation(true);
			P->SetUsingAbsoluteScale(true);
			P->SetWorldLocationAndRotation(Centre, FRotator::ZeroRotator);
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->SetCanEverAffectNavigation(false);
			P->RegisterComponent();
			M.ApplyTo(P);
			P->SetVisibility(false);
			SectorMesh.Add(P);
		}
	}
	const float Now = FblServerNow(this);
	const float LocalNow = World->GetTimeSeconds();
	for (int32 s = 0; s < 6; ++s)
	{
		UProceduralMeshComponent* P = SectorMesh[s];
		if (!P) continue;
		const uint8 St = Sectors[s];
		if (St != SectorShown[s])
		{
			SectorShown[s] = St;
			SectorChanged[s] = LocalNow;
			if (SectorFx[s]) { WRFx::Stop(SectorFx[s]); SectorFx[s] = nullptr; }
			P->SetVisibility(St == 1 || St == 2);
			const float Mid = FMath::DegreesToRadians(60.f * s + 30.f);
			const FVector At = FVector(SectorCentre) + FVector(FMath::Cos(Mid), FMath::Sin(Mid), 0.f) * SectorRadius * 0.55f;
			const FName Fx = St == 1 ? FName(TEXT("sylvaroth.vine_creep")) : St == 2 ? FName(TEXT("sylvaroth.vine_live")) : St == 3 ? FName(TEXT("sylvaroth.sector_burnt")) : NAME_None;
			if (!Fx.IsNone())
			{
				USceneComponent* Anchor = NewObject<USceneComponent>(C);
				Anchor->SetupAttachment(C->GetRootComponent());
				Anchor->SetUsingAbsoluteLocation(true);
				Anchor->SetUsingAbsoluteRotation(true);
				Anchor->SetWorldLocationAndRotation(At, FRotator(0.f, 60.f * s + 30.f, 0.f));
				Anchor->RegisterComponent();
				LookComps.Add(Anchor);
				SectorFx[s] = WRFx::Attach(Anchor, Fx, NAME_None, SectorRadius / 1500.f);
			}
		}
		// Creeping: the vines rise out of the moss over the creep (1.5 s); live they lie on it, writhing a little.
		if (St == 1)
		{
			const float Creep = WRTune(TEXT("bosses.sylvaroth.vineCreep"), 1.5f);
			const float U = FMath::Clamp((LocalNow - SectorChanged[s]) / FMath::Max(0.1f, Creep), 0.f, 1.f);
			P->SetWorldLocation(FVector(SectorCentre) - FVector(0.f, 0.f, 30.f * (1.f - U)));
		}
		else if (St == 2)
		{
			P->SetWorldLocation(FVector(SectorCentre) + FVector(0.f, 0.f, 2.f * FMath::Sin(LocalNow * 3.f + s)));
		}
	}
	(void)Now;
}
