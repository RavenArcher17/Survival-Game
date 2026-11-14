// The portal's look and the fold's rooms (cooks/build-portals-artifacts.md 2.2, 2.3). A portal is a ring of its land's
// signature material (obsidian shards on a cracked basalt plinth; glacier-ice blades frozen into a snow-drifted one)
// round a tear in the rank colour, a light pillar over it and a hum by rank. A fold is its run's handmade rooms, each
// built from its row in Data/portal_rooms/<biome>.json: flagstone floors on the 4 m grid, coursed basalt ashlar or
// glacier-ice walls with door sockets, a closed ceiling on beams, corridors between, and every feature (pillars,
// ledges, lava runnels, crucibles, anvils, kilns, lamps, racks, statues, bones, icicles, pools) drawn and lit by what it
// is. Collision on every machine, the look on clients.

#include "WRPortals.h"
#include "WRPortalsInternal.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRFx.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

using namespace WRPortalsImpl;
using WRLookKit::FSurf;
using WRLookKit::FKit;

// ================================================================================================ shared drawing

UPointLightComponent* WRFoldLook::Light(AActor* Owner, USceneComponent* Parent, const FVector& At, const FLinearColor& Colour, float Intensity, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
	L->SetupAttachment(Parent);
	L->SetRelativeLocation(At);
	L->SetLightColor(Colour);
	L->SetIntensity(Intensity);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->RegisterComponent();
	return L;
}

UBoxComponent* WRFoldLook::Block(AActor* Owner, USceneComponent* Parent, const FVector& Centre, const FVector& Half, const FRotator& Rot)
{
	UBoxComponent* B = NewObject<UBoxComponent>(Owner);
	B->SetupAttachment(Parent);
	B->SetBoxExtent(Half);
	B->SetRelativeLocation(Centre);
	B->SetRelativeRotation(Rot);
	B->SetCollisionProfileName(TEXT("BlockAll"));
	B->SetCanEverAffectNavigation(false);
	B->RegisterComponent();
	return B;
}

UMaterialInstanceDynamic* WRFoldLook::RockMat(UObject* Outer, const FLinearColor& Tint)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock"));
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
	M->SetVectorParameterValue(TEXT("Tint"), Tint);
	return M;
}

UStaticMeshComponent* WRFoldLook::Rock(AActor* Owner, USceneComponent* Parent, int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, float Sink)
{
	static const TCHAR* Names[] = { TEXT("01A"), TEXT("02A"), TEXT("05A"), TEXT("08A"), TEXT("09A"), TEXT("12A") };
	const TCHAR* N = Names[((Which % 6) + 6) % 6];
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/SimpleRocks/3D_Assets/Meshes/Rocks/SM_SimpleRock_%s.SM_SimpleRock_%s"), N, N));
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent);
	const FBox B = Mesh->GetBoundingBox();
	const FVector S = B.GetSize();
	const FVector Scale(Size.X / FMath::Max(1.f, S.X), Size.Y / FMath::Max(1.f, S.Y), Size.Z / FMath::Max(1.f, S.Z));
	C->SetRelativeScale3D(Scale);
	C->SetRelativeRotation(Rot);
	C->SetRelativeLocation(Base + Rot.RotateVector(FVector(-B.GetCenter().X * Scale.X, -B.GetCenter().Y * Scale.Y, -B.Min.Z * Scale.Z - Sink)));
	if (Mat) C->SetMaterial(0, Mat);
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	return C;
}

UStaticMeshComponent* WRFoldLook::Shape(AActor* Owner, USceneComponent* Parent, const TCHAR* Kind, const FVector& Size, const FVector& Centre, const FRotator& Rot, UMaterialInterface* Mat)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Kind, Kind));
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent);
	// The basic shapes are 100 cm across with their middle at the origin.
	C->SetRelativeScale3D(Size / 100.f);
	C->SetRelativeRotation(Rot);
	C->SetRelativeLocation(Centre);
	if (Mat) C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	return C;
}

UMaterialInstanceDynamic* WRFoldLook::Glow(UObject* Outer, const FLinearColor& Colour, float Strength)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow"));
	if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
	M->SetVectorParameterValue(TEXT("Color"), Colour);
	M->SetScalarParameterValue(TEXT("Glow"), Strength);
	return M;
}

FLinearColor WRFoldLook::KitColour(FName Biome, const TCHAR* Field, const FColor& Fallback)
{
	return FLinearColor(PColour(PChild(WRPortals::Kit(Biome), TEXT("look")), Field, Fallback));
}

// ================================================================================================ AWRPortal

AWRPortal::AWRPortal()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(60000.f));
}

void AWRPortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRPortal, PortalId);
	DOREPLIFETIME(AWRPortal, Biome);
	DOREPLIFETIME(AWRPortal, Rank);
	DOREPLIFETIME(AWRPortal, Modifiers);
	DOREPLIFETIME(AWRPortal, Stage);
	DOREPLIFETIME(AWRPortal, bKey);
	DOREPLIFETIME(AWRPortal, OwnerName);
	DOREPLIFETIME(AWRPortal, Allowed);
	DOREPLIFETIME(AWRPortal, BreakAtUtc);
	DOREPLIFETIME(AWRPortal, bRunOpen);
	DOREPLIFETIME(AWRPortal, bSealed);
	DOREPLIFETIME(AWRPortal, bClosing);
	DOREPLIFETIME(AWRPortal, OpenAt);
	DOREPLIFETIME(AWRPortal, Fold);
}

void AWRPortal::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// A Legendary is seen (and its hum heard near) from across its land; the rest from 500 m.
		const float Seen = PNum(WRPortals::Rank(Rank), TEXT("seen"), 50000.f);
		SetNetCullDistanceSquared(FMath::Square(FMath::Max(Seen, 20000.f)));
	}
	if (!Biome.IsNone()) BuildLook();
}

void AWRPortal::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Hum) Hum->Stop();
	Super::EndPlay(Reason);
}

void AWRPortal::OnRep_Look() { BuildLook(); }

void AWRPortal::OnRep_Stage()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	for (UStaticMeshComponent* C : Cracks) if (C) C->SetVisibility(Stage >= 1);
	if (bClosing && Hum) Hum->FadeOut(2.f, 0.f);
}

FVector AWRPortal::Front() const
{
	return GetActorLocation() + GetActorForwardVector() * 360.f;
}

double AWRPortal::SecondsToBreak() const
{
	if (BreakAtUtc <= 0.0) return -1.0;
	return FMath::Max(0.0, BreakAtUtc - double(FDateTime::UtcNow().ToUnixTimestamp()));
}

FString AWRPortal::StateLine() const
{
	if (bClosing) return TEXT("Won: closing");
	if (bSealed) return TEXT("A run is at its boss: the door is sealed");
	const FString Run = bRunOpen ? TEXT(" · a run is inside (its clock holds)") : TEXT("");
	if (bKey) return FString::Printf(TEXT("Key portal of %s · it never breaks%s"), *OwnerName, *Run);
	const double S = SecondsToBreak();
	if (S < 0.0) return FString::Printf(TEXT("Stable · this world's portals never break%s"), *Run);
	const int32 D = int32(S / 86400.0), H = int32(FMath::Fmod(S, 86400.0) / 3600.0), M = int32(FMath::Fmod(S, 3600.0) / 60.0);
	const FString Left = D > 0 ? FString::Printf(TEXT("%d d %d h"), D, H) : FString::Printf(TEXT("%d h %d min"), H, M);
	const TCHAR* St = Stage == 2 ? TEXT("Spilling: its creatures pour out") : Stage == 1 ? TEXT("Cracking: packs step out") : TEXT("Stable");
	return FString::Printf(TEXT("%s · breaks in %s%s"), St, *Left, *Run);
}

bool AWRPortal::IsSealedTo(const FString& Player) const
{
	// The opener, their clan and their band as the server keeps it (Allowed, refreshed every second).
	return bKey && Player != OwnerName && !Allowed.Contains(Player);
}

void AWRPortal::BuildLook()
{
	if (bLookBuilt || Biome.IsNone()) return;
	bLookBuilt = true;
	const bool bFrost = Biome == TEXT("tundra");
	const FLinearColor RankC = FLinearColor(WRPortals::RankColour(Rank));
	// Collision, every machine: the plinth (you can stand on it; the tear itself is open air).
	WRFoldLook::Block(this, RootComponent, FVector(0, 0, -10.f), FVector(170.f, 230.f, 30.f));
	if (GetNetMode() == NM_DedicatedServer) return;

	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FSurf Plinth = bFrost ? FSurf::Stone(FLinearColor(0.42f, 0.46f, 0.5f), 0.9f) : FSurf::Stone(FLinearColor(0.02f, 0.018f, 0.017f), 0.93f);
	const FSurf Edge = bFrost ? FSurf::Glass(FLinearColor(0.55f, 0.75f, 0.95f), 0.08f) : FSurf::Glass(FLinearColor(0.012f, 0.01f, 0.014f), 0.f);
	const FSurf RootGlow = bFrost ? FSurf::Lit(FLinearColor(0.5f, 0.75f, 1.f), 0.6f) : FSurf::Lit(FLinearColor(1.f, 0.36f, 0.06f), 0.9f);
	// The plinth: a cracked slab (volcanic: four basalt slabs with glowing seams between; tundra: a stone sunk in a
	// snow drift that has banked up against its back).
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector C((i % 2 ? 80.f : -80.f), (i / 2 ? 110.f : -110.f), 0.f);
		// Each slab runs 1.8 m down into the ground, so on a slope it's set in the hill, never perched on it.
		K.Add(Plinth.Dim(0.9f + 0.15f * K.Rand(C, 5)), FVector(156.f, 216.f, 200.f + 8.f * K.Rand(C, 7)), C + FVector(0, 0, -80.f), FRotator(1.5f * K.Jit(C, 3), 2.f * K.Jit(C, 9), 1.5f * K.Jit(C, 11)));
	}
	if (!bFrost)
	{
		K.Add(RootGlow, FVector(330.f, 6.f, 30.f), FVector(0, 0, 4.f));
		K.Add(RootGlow, FVector(6.f, 450.f, 30.f), FVector(0, 0, 4.f));
	}
	else
	{
		K.Add(FSurf::Stone(FLinearColor(0.82f, 0.86f, 0.9f), 0.97f), FVector(160.f, 520.f, 90.f), FVector(-170.f, 0, 10.f), FRotator(0.f, 0.f, 0.f), EWRLookMesh::Cylinder);
	}
	// The ring: shards standing on an ellipse 3.2 m tall round the tear, leaning in, each glowing at its root.
	const int32 N = bFrost ? 12 : 14;
	const float RW = 135.f, RH = 165.f, CZ = 200.f;
	for (int32 i = 0; i < N; ++i)
	{
		const float A = 2.f * PI * (i + 0.5f) / N;
		const FVector OnRing(0.f, FMath::Cos(A) * RW, CZ + FMath::Sin(A) * RH);
		const FVector Out = FVector(0.f, FMath::Cos(A), FMath::Sin(A) * (RH / RW)).GetSafeNormal();
		const float Len = (bFrost ? 70.f : 85.f) + 40.f * K.Rand(OnRing, 13);
		// Each shard grows outward from the tear's edge, its tip bent toward the viewer a little.
		K.Spike(Edge.Dim(0.85f + 0.3f * K.Rand(OnRing, 17)), OnRing, (Out + FVector(0.15f * K.Jit(OnRing, 19), 0.f, 0.f)).GetSafeNormal(), (bFrost ? 26.f : 32.f) + 10.f * K.Rand(OnRing, 23), Len);
		K.Add(RootGlow, FVector(14.f, 22.f, 22.f), OnRing, FRotator(0.f, 0.f, FMath::RadiansToDegrees(A)));
		// The ones at the bottom stand on the plinth.
		if (OnRing.Z < 90.f) K.Add(Edge.Dim(0.7f), FVector(30.f, 30.f, OnRing.Z), FVector(0.f, OnRing.Y, OnRing.Z * 0.5f));
	}
	// Corner cairns (volcanic: slag; tundra: a small inuksuk-like stack of three stones).
	for (int32 c = 0; c < 4; ++c)
	{
		const FVector At((c % 2 ? 140.f : -140.f), (c / 2 ? 200.f : -200.f), 20.f);
		const FSurf Cairn = bFrost ? FSurf::Stone(FLinearColor(0.32f, 0.35f, 0.38f), 0.9f) : FSurf::Stone(FLinearColor(0.025f, 0.02f, 0.02f), 0.6f);
		K.Add(Cairn, FVector(42.f, 38.f, 26.f), At + FVector(0, 0, 13.f), FRotator(0.f, 30.f * c, 0.f));
		K.Add(Cairn.Dim(0.9f), FVector(30.f, 28.f, 22.f), At + FVector(0, 0, 36.f), FRotator(0.f, 70.f * c, 4.f));
		K.Add(bFrost ? Cairn : FSurf::Glass(FLinearColor(0.03f, 0.02f, 0.02f)), FVector(18.f, 18.f, 18.f), At + FVector(0, 0, 55.f), FRotator(0.f, 20.f * c, 0.f));
	}
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);

	// The tear: an elliptic membrane of the rank's light, a brighter core in it, turning slowly.
	TearMat = WRFoldLook::Glow(this, RankC, 3.5f);
	UMaterialInstanceDynamic* CoreMat = WRFoldLook::Glow(this, FMath::Lerp(RankC, FLinearColor::White, 0.55f), 6.f);
	if (UStaticMeshComponent* T = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(2.f * RH, 2.f * RW, 4.f), FVector(0, 0, CZ), FRotator(90.f, 0.f, 0.f), TearMat)) TearPlanes.Add(T);
	if (UStaticMeshComponent* T = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(2.f * RH * 0.55f, 2.f * RW * 0.45f, 6.f), FVector(0, 0, CZ), FRotator(90.f, 0.f, 0.f), CoreMat)) TearPlanes.Add(T);
	// Cracks across the shards (cracking and after): hairline glowing seams, hidden while it's stable.
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = 2.f * PI * (i * 2 + 1) / 12.f;
		const FVector At(0.f, FMath::Cos(A) * (RW + 30.f), CZ + FMath::Sin(A) * (RH + 30.f));
		if (UStaticMeshComponent* C = WRFoldLook::Shape(this, RootComponent, TEXT("Cube"), FVector(3.f, 3.f, 60.f), At, FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) + 60.f), CoreMat))
		{
			C->SetVisibility(Stage >= 1);
			Cracks.Add(C);
		}
	}
	// The light pillar over it in the rank colour (rank 5 rises 300 m).
	const float PillarH = PNum(WRPortals::Rank(Rank), TEXT("pillar"), 6000.f);
	PillarMat = WRFoldLook::Glow(this, RankC, 2.5f);
	Pillar = WRFoldLook::Shape(this, RootComponent, TEXT("Cylinder"), FVector(Rank >= 5 ? 110.f : 70.f, Rank >= 5 ? 110.f : 70.f, PillarH), FVector(0, 0, CZ + RH + PillarH * 0.5f), FRotator::ZeroRotator, PillarMat);
	if (Pillar) Pillar->SetCullDistance(0.f);
	Light = WRFoldLook::Light(this, RootComponent, FVector(80.f, 0.f, CZ), RankC, 9000.f, 1400.f);
	Motes = WRFx::Attach(RootComponent, bFrost ? TEXT("portal.snow") : TEXT("portal.embers"), NAME_None, 1.f);
	if (Motes) Motes->SetRelativeLocation(FVector(250.f, 0.f, CZ));
	Hum = WRSound::StartLoop(this, FName(*FString::Printf(TEXT("portal.hum.%d"), FMath::Clamp<int32>(Rank, 1, 5))));
	OnRep_Stage();
}

void AWRPortal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || !bLookBuilt) return;
	Clock += DeltaSeconds;
	const float Now = PServerNow(GetWorld());
	// Opening (a key's tear grows); closing (it shrinks); otherwise it breathes and turns.
	float Open = 1.f;
	if (OpenAt > Now) Open = 1.f - FMath::Clamp((OpenAt - Now) / FMath::Max(0.1f, WRPortals::Num(TEXT("entry.keyOpenSeconds"), 3.f)), 0.f, 1.f);
	if (bClosing) Open = FMath::Max(0.15f, Open * 0.6f + 0.4f * (0.5f + 0.5f * FMath::Sin(Clock * 1.5f)));
	for (int32 i = 0; i < TearPlanes.Num(); ++i)
	{
		USceneComponent* T = TearPlanes[i];
		if (!T) continue;
		const float Breathe = 1.f + 0.03f * FMath::Sin(Clock * (1.3f + i * 0.7f));
		const FVector Base = i == 0 ? FVector(2.f * 165.f, 2.f * 135.f, 4.f) / 100.f : FVector(2.f * 165.f * 0.55f, 2.f * 135.f * 0.45f, 6.f) / 100.f;
		T->SetRelativeScale3D(FVector(Base.X * Open * Breathe, Base.Y * Open * Breathe, Base.Z));
		T->SetRelativeRotation(FRotator(90.f, 0.f, 0.f) + FRotator(0.f, 0.f, Clock * (i == 0 ? 6.f : -11.f)));
	}
	// The pillar: steady while stable, flickering while cracking, stuttering red-black at the rim while spilling.
	float Flicker = 1.f;
	if (Stage == 1) Flicker = FMath::FRand() < 0.06f ? 0.35f : 0.85f + 0.15f * FMath::Sin(Clock * 9.f);
	if (Stage == 2) Flicker = FMath::FRand() < 0.15f ? 0.1f : 0.7f + 0.3f * FMath::Sin(Clock * 17.f);
	const FLinearColor RankC = FLinearColor(WRPortals::RankColour(Rank));
	if (PillarMat) PillarMat->SetScalarParameterValue(TEXT("Glow"), 2.5f * Flicker * Open);
	if (TearMat) TearMat->SetVectorParameterValue(TEXT("Color"), Stage == 2 ? FMath::Lerp(RankC, FLinearColor(0.35f, 0.02f, 0.02f), 0.35f + 0.25f * FMath::Sin(Clock * 5.f)) : RankC);
	if (Light) Light->SetIntensity(9000.f * Flicker * Open);
	if (Pillar) Pillar->SetVisibility(Open > 0.2f);
}

// ================================================================================================ AWRFold

TArray<TWeakObjectPtr<AWRFold>>& AWRFold::All()
{
	static TArray<TWeakObjectPtr<AWRFold>> L;
	return L;
}

AWRFold::AWRFold()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// A fold is up to ~650 m long: everyone inside keeps it however far along they are.
	SetNetCullDistanceSquared(FMath::Square(100000.f));
}

void AWRFold::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRFold, Bay);
	DOREPLIFETIME(AWRFold, Biome);
	DOREPLIFETIME(AWRFold, Rank);
	DOREPLIFETIME(AWRFold, Modifiers);
	DOREPLIFETIME(AWRFold, Rooms);
	DOREPLIFETIME(AWRFold, Portal);
	DOREPLIFETIME(AWRFold, CloseAt);
	DOREPLIFETIME(AWRFold, bSealed);
}

void AWRFold::BeginPlay()
{
	Super::BeginPlay();
	All().Add(this);
	if (Rooms.Num() > 0) Build();
}

void AWRFold::EndPlay(const EEndPlayReason::Type Reason)
{
	All().Remove(this);
	All().RemoveAll([](const TWeakObjectPtr<AWRFold>& W) { return !W.IsValid(); });
	Super::EndPlay(Reason);
}

void AWRFold::OnRep_Rooms() { if (HasActorBegunPlay()) Build(); }

bool AWRFold::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	const APlayerController* PC = Cast<APlayerController>(RealViewer);
	if (PC && PC->PlayerState && Roster.Contains(PC->PlayerState->GetPlayerName())) return true;
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

FTransform AWRFold::RoomFrame(int32 Index) const
{
	if (!Rooms.IsValidIndex(Index)) return FTransform::Identity;
	const FWRFoldRoomRep& R = Rooms[Index];
	// Main rooms run east; a north branch runs north (-Y), a south one south (+Y): +X of the frame is into the room.
	const float Yaw = R.Side == 1 ? -90.f : R.Side == 2 ? 90.f : 0.f;
	return FTransform(FRotator(0.f, Yaw, 0.f), R.Origin);
}

FVector AWRFold::RoomPoint(int32 Index, float XM, float YM, float ZCm) const
{
	return RoomFrame(Index).TransformPosition(FVector(XM * 100.f, YM * 100.f, ZCm));
}

TSharedPtr<FJsonObject> AWRFold::RoomRow(int32 Index) const
{
	return Rooms.IsValidIndex(Index) ? WRPortals::Room(Biome, Rooms[Index].RoomId) : nullptr;
}

int32 AWRFold::RoomAt(const FVector& Point, float Grow) const
{
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FVector L = RoomFrame(i).InverseTransformPosition(Point);
		const FWRFoldRoomRep& R = Rooms[i];
		if (L.X >= -Grow && L.X <= R.W + Grow && FMath::Abs(L.Y) <= R.D * 0.5f + Grow && L.Z > -400.f && L.Z < R.H + 200.f) return i;
	}
	return INDEX_NONE;
}

FVector AWRFold::EntryPoint() const
{
	if (Rooms.Num() == 0) return GetActorLocation();
	const TSharedPtr<FJsonObject> Row = RoomRow(0);
	// Just in front of the entry stone, facing into the fold.
	const TSharedPtr<FJsonValue> Stone = Row.IsValid() ? Row->Values.FindRef(TEXT("stone")) : nullptr;
	return RoomPoint(0, PAt(Stone, 0, 4.f) + 2.5f, PAt(Stone, 1, 0.f));
}

int32 AWRFold::BossRoom() const
{
	for (int32 i = 0; i < Rooms.Num(); ++i) if (Rooms[i].Type == TEXT("boss")) return i;
	return INDEX_NONE;
}

namespace WRFoldBuild
{
	/** The kit's surfaces. */
	struct FKitLook
	{
		bool bFrost = false;
		FSurf Stone, StoneAlt, Floor, FloorAlt, Trim, Beam, Dark, Ice, IceDeep, Lava, Ember, Snow, Wood, Iron, Bone, Hide, LampGlow;
		FLinearColor LampColour, GlowColour;
		explicit FKitLook(FName Biome)
		{
			bFrost = Biome == TEXT("tundra");
			const FLinearColor S = WRFoldLook::KitColour(Biome, TEXT("stone"), FColor(24, 21, 20));
			const FLinearColor SA = WRFoldLook::KitColour(Biome, TEXT("stoneAlt"), FColor(44, 38, 34));
			const FLinearColor F = WRFoldLook::KitColour(Biome, TEXT("floor"), FColor(34, 30, 27));
			const FLinearColor T = WRFoldLook::KitColour(Biome, TEXT("trim"), FColor(140, 92, 46));
			const FLinearColor B = WRFoldLook::KitColour(Biome, TEXT("beam"), FColor(52, 34, 22));
			LampColour = WRFoldLook::KitColour(Biome, TEXT("lamp"), FColor(255, 150, 70));
			GlowColour = WRFoldLook::KitColour(Biome, TEXT("glow"), FColor(255, 120, 40));
			Stone = FSurf::Stone(S, 0.9f);
			StoneAlt = FSurf::Stone(SA, 0.92f);
			Floor = FSurf::Stone(F, 0.94f);
			FloorAlt = FSurf::Stone(F * 0.82f, 0.96f);
			Trim = bFrost ? FSurf::Stone(T, 0.7f) : FSurf::Steel(T, 0.5f);
			Beam = bFrost ? FSurf::Stone(B, 0.75f) : FSurf::Stone(B, 0.85f);
			Dark = FSurf::Stone(S * 0.4f, 0.97f);
			Ice = FSurf::Glass(FLinearColor(0.42f, 0.6f, 0.8f), 0.04f);
			IceDeep = FSurf::Glass(FLinearColor(0.05f, 0.14f, 0.32f), 0.02f);
			Lava = FSurf::Lit(FLinearColor(1.f, 0.32f, 0.04f), 0.95f);
			Ember = FSurf::Lit(FLinearColor(1.f, 0.42f, 0.1f), 0.55f);
			Snow = FSurf::Stone(FLinearColor(0.85f, 0.88f, 0.92f), 0.97f);
			Wood = FSurf::Wood(FLinearColor(0.16f, 0.1f, 0.06f));
			Iron = FSurf::Steel(FLinearColor(0.03f, 0.028f, 0.026f), 0.6f);
			Bone = FSurf::Stone(FLinearColor(0.62f, 0.58f, 0.5f), 0.7f);
			Hide = FSurf::Cloth(FLinearColor(0.32f, 0.24f, 0.16f));
			LampGlow = FSurf::Lit(LampColour, 0.8f);
		}
	};

	/** Walls of one room side: the openings (door centres along it) left out. */
	struct FWallSide { FVector A, B; TArray<float> Doors; };
}
using namespace WRFoldBuild;

void AWRFold::Build()
{
	if (bBuilt) return;
	bBuilt = true;
	Bounds = FBox(ForceInit);
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FWRFoldRoomRep& R = Rooms[i];
		const FTransform F = RoomFrame(i);
		for (const FVector& C : { FVector(-200.f, -R.D * 0.5f - 200.f, -500.f), FVector(R.W + 200.f, R.D * 0.5f + 200.f, R.H + 200.f) }) Bounds += F.TransformPosition(C);
	}
	// The corridors between rooms count as inside too.
	if (Rooms.Num() > 0) Bounds = Bounds.ExpandBy(FVector(1000.f, 1000.f, 0.f));
	for (int32 i = 0; i < Rooms.Num(); ++i) BuildRoom(i);
	// Corridors: main line east door to west door; branches from the host's side door to the branch's door.
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FWRFoldRoomRep& R = Rooms[i];
		if (R.Side == 0)
		{
			if (Rooms.IsValidIndex(i + 1) && Rooms[i + 1].Side == 0) BuildCorridor(R.Origin + FVector(R.W, 0.f, 0.f), Rooms[i + 1].Origin, FMath::Min(R.H, Rooms[i + 1].H));
		}
		else
		{
			const FWRFoldRoomRep& H = Rooms[R.Host];
			const FVector HostDoor = H.Origin + FVector(R.Origin.X - H.Origin.X, R.Side == 1 ? -H.D * 0.5f : H.D * 0.5f, 0.f);
			BuildCorridor(HostDoor, R.Origin, FMath::Min(R.H, H.H));
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("FOLD built: bay %d, %d rooms, %s"), Bay, Rooms.Num(), *Bounds.ToString());
}

void AWRFold::BuildCorridor(const FVector& A, const FVector& B, float H)
{
	const FVector D = B - A;
	const float Len = D.Size2D();
	if (Len < 10.f) return;
	const FRotator Yaw(0.f, D.Rotation().Yaw, 0.f);
	const FTransform Inv = GetActorTransform().Inverse();
	const FVector Mid = (A + B) * 0.5f;
	const float Hc = FMath::Min(H, 600.f) + 40.f;
	const float Half = 200.f;
	const FKitLook L(Biome);
	// Collision: floor, two walls, roof (all in the corridor's frame).
	const auto Box = [&](const FVector& LocalC, const FVector& Ext)
	{
		WRFoldLook::Block(this, RootComponent, Inv.TransformPosition(Mid + Yaw.RotateVector(LocalC)), Ext, Yaw);
	};
	Box(FVector(0, 0, -20.f), FVector(Len * 0.5f + 60.f, Half + 80.f, 20.f));
	Box(FVector(0, -Half - 40.f, Hc * 0.5f), FVector(Len * 0.5f, 40.f, Hc * 0.5f + 20.f));
	Box(FVector(0, Half + 40.f, Hc * 0.5f), FVector(Len * 0.5f, 40.f, Hc * 0.5f + 20.f));
	Box(FVector(0, 0, Hc + 20.f), FVector(Len * 0.5f + 60.f, Half + 80.f, 20.f));
	if (GetNetMode() == NM_DedicatedServer) return;
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	K.Frame = FTransform(Yaw, Inv.TransformPosition(Mid));
	const int32 Steps = FMath::Max(1, FMath::RoundToInt(Len / 200.f));
	for (int32 s = 0; s < Steps; ++s)
	{
		const float X = -Len * 0.5f + Len * (s + 0.5f) / Steps;
		for (const float Y : { -100.f, 100.f }) K.Add(((s + (Y > 0)) % 2 ? L.Floor : L.FloorAlt).Dim(0.92f + 0.12f * K.Rand(FVector(X, Y, 0), 3)), FVector(Len / Steps - 3.f, 197.f, 20.f), FVector(X, Y, -10.f));
		for (const float S : { -1.f, 1.f })
		{
			if (L.bFrost)
			{
				K.Add(L.StoneAlt.Dim(0.8f), FVector(Len / Steps - 2.f, 70.f, 80.f), FVector(X, S * (Half + 35.f), 40.f));
				K.Add(L.Ice.Dim(0.9f + 0.15f * K.Rand(FVector(X, S, 1), 5)), FVector(Len / Steps - 4.f, 60.f, Hc - 80.f), FVector(X, S * (Half + 35.f), 80.f + (Hc - 80.f) * 0.5f));
			}
			else
			{
				for (int32 c = 0; c * 75.f < Hc; ++c)
				{
					K.Add(((c + s) % 2 ? L.Stone : L.StoneAlt).Dim(0.85f + 0.25f * K.Rand(FVector(X, S, c), 7)), FVector(Len / Steps - 3.f, 60.f, 72.f), FVector(X, S * (Half + 30.f), c * 75.f + 37.f));
				}
			}
		}
		K.Add(L.Dark, FVector(Len / Steps, 2.f * Half + 120.f, 40.f), FVector(X, 0, Hc + 20.f));
		K.Add(L.Beam, FVector(30.f, 2.f * Half + 20.f, 26.f), FVector(X, 0, Hc - 13.f));
	}
	// A lamp in the corridor's middle, on its south wall.
	const FVector LampAt = FVector(0.f, Half - 10.f, 230.f);
	K.Add(L.bFrost ? FSurf::Stone(FLinearColor(0.35f, 0.33f, 0.3f), 0.8f) : L.Iron, FVector(36.f, 30.f, 14.f), LampAt);
	K.Add(L.LampGlow, FVector(20.f, 14.f, 10.f), LampAt + FVector(0, -6.f, 12.f));
	FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
	WRFoldLook::Light(this, RootComponent, Inv.TransformPosition(Mid + Yaw.RotateVector(LampAt + FVector(0, -40.f, 20.f))), L.LampColour, 2600.f, 900.f);
}

void AWRFold::BuildRoom(int32 Index)
{
	const FWRFoldRoomRep& R = Rooms[Index];
	const TSharedPtr<FJsonObject> Row = RoomRow(Index);
	if (!Row.IsValid()) return;
	const FTransform RF = RoomFrame(Index);
	const FTransform Inv = GetActorTransform().Inverse();
	const FTransform Local = RF * Inv;   // room space -> actor space
	const bool bDraw = GetNetMode() != NM_DedicatedServer;
	const FKitLook L(Biome);
	const float W = R.W, D = R.D, H = R.H;
	const float WallT = L.bFrost ? 100.f : 60.f;
	const float DoorW = 400.f, DoorH = FMath::Min(600.f, H - 60.f);
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	K.Frame = Local;
	const auto M = [](float Metres) { return Metres * 100.f; };
	const auto Box = [&](const FVector& Min, const FVector& Max)
	{
		WRFoldLook::Block(this, RootComponent, Local.TransformPosition((Min + Max) * 0.5f), (Max - Min) * 0.5f, Local.Rotator());
	};
	const auto Lamp = [&](const FVector& At, float Intensity, float Radius, const FLinearColor& C)
	{
		if (bDraw) WRFoldLook::Light(this, RootComponent, Local.TransformPosition(At), C, Intensity, Radius);
	};

	// ------------------------------------------------ hazards (every machine: the server hurts, the client draws)
	const TArray<TSharedPtr<FJsonValue>>* Features = PArr(Row, TEXT("features"));
	TArray<FBox2D> PoolRects, ThickRects, PaneRects;
	if (Features)
	{
		for (const TSharedPtr<FJsonValue>& F : *Features)
		{
			const FString Kind = PAtStr(F, 0);
			if (Kind == TEXT("pool")) PoolRects.Add(FBox2D(FVector2D(M(PAt(F, 1) - PAt(F, 3) * 0.5f), M(PAt(F, 2) - PAt(F, 4) * 0.5f)), FVector2D(M(PAt(F, 1) + PAt(F, 3) * 0.5f), M(PAt(F, 2) + PAt(F, 4) * 0.5f))));
			if (Kind == TEXT("thickice")) ThickRects.Add(FBox2D(FVector2D(M(PAt(F, 1) - PAt(F, 3) * 0.5f), M(PAt(F, 2) - PAt(F, 4) * 0.5f)), FVector2D(M(PAt(F, 1) + PAt(F, 3) * 0.5f), M(PAt(F, 2) + PAt(F, 4) * 0.5f))));
			if (Kind == TEXT("runnel"))
			{
				LavaA.Add(RF.TransformPosition(FVector(M(PAt(F, 1)), M(PAt(F, 2)), 0.f)));
				LavaB.Add(RF.TransformPosition(FVector(M(PAt(F, 3)), M(PAt(F, 4)), 0.f)));
			}
		}
	}
	const TSharedPtr<FJsonObject> Trap = PChild(Row, TEXT("trap"));
	if (PStr(Trap, TEXT("kind")) == TEXT("thinice"))
	{
		const TSharedPtr<FJsonValue> P = Trap->Values.FindRef(TEXT("pool"));
		PoolRects.Add(FBox2D(FVector2D(M(PAt(P, 0) - PAt(P, 2) * 0.5f), M(PAt(P, 1) - PAt(P, 3) * 0.5f)), FVector2D(M(PAt(P, 0) + PAt(P, 2) * 0.5f), M(PAt(P, 1) + PAt(P, 3) * 0.5f))));
		if (const TArray<TSharedPtr<FJsonValue>>* Panes = PArr(Trap, TEXT("panes")))
			for (const TSharedPtr<FJsonValue>& V : *Panes) PaneRects.Add(FBox2D(FVector2D(M(PAt(V, 0) - PAt(V, 2) * 0.5f), M(PAt(V, 1) - PAt(V, 3) * 0.5f)), FVector2D(M(PAt(V, 0) + PAt(V, 2) * 0.5f), M(PAt(V, 1) + PAt(V, 3) * 0.5f))));
	}
	const auto WorldBox = [&RF](const FVector& A, const FVector& B) { FBox Bx(ForceInit); Bx += RF.TransformPosition(A); Bx += RF.TransformPosition(B); return Bx.ExpandBy(1.f); };
	for (const FBox2D& T : ThickRects) ThickIce.Add(WorldBox(FVector(T.Min, -50.f), FVector(T.Max, 50.f)));
	const float PitDepth = 160.f;

	// ------------------------------------------------ floor: flagstones on the 2 m grid; a pool leaves a pit
	const int32 CX = FMath::Max(1, FMath::RoundToInt(W / 200.f)), CY = FMath::Max(1, FMath::RoundToInt(D / 200.f));
	const float SX = W / CX, SY = D / CY;
	TArray<uint8> Cell;   // 0 stone, 1 thick ice over a pit, 2 pane (a thing), 3 open water
	Cell.SetNumZeroed(CX * CY);
	for (int32 y = 0; y < CY; ++y)
	{
		for (int32 x = 0; x < CX; ++x)
		{
			const FVector2D C(SX * (x + 0.5f), -D * 0.5f + SY * (y + 0.5f));
			const bool bPool = PoolRects.ContainsByPredicate([&C](const FBox2D& B) { return B.IsInside(C); });
			if (!bPool) continue;
			const bool bThick = ThickRects.ContainsByPredicate([&C](const FBox2D& B) { return B.IsInside(C); });
			const bool bPane = PaneRects.ContainsByPredicate([&C](const FBox2D& B) { return B.IsInside(C); });
			Cell[y * CX + x] = bThick ? 1 : bPane ? 2 : (PStr(Trap, TEXT("kind")) == TEXT("thinice") ? 1 : 3);
		}
	}
	// Collision rows: runs of standing cells (stone or thick ice) merged; pits get a floor 1.6 m down and a ledge rim.
	for (int32 y = 0; y < CY; ++y)
	{
		int32 Start = -1;
		for (int32 x = 0; x <= CX; ++x)
		{
			const bool bSolid = x < CX && Cell[y * CX + x] <= 1;
			if (bSolid && Start < 0) Start = x;
			if (!bSolid && Start >= 0)
			{
				Box(FVector(SX * Start, -D * 0.5f + SY * y, -40.f), FVector(SX * x, -D * 0.5f + SY * (y + 1), 0.f));
				Start = -1;
			}
		}
	}
	if (PoolRects.Num() > 0)
	{
		Box(FVector(0.f, -D * 0.5f, -PitDepth - 40.f), FVector(W, D * 0.5f, -PitDepth));
		for (const FBox2D& P : PoolRects)
		{
			Pits.Add(WorldBox(FVector(P.Min, -PitDepth - 20.f), FVector(P.Max, -40.f)));
			// Pit walls with a step half way (you climb out at the edge).
			Box(FVector(P.Min.X, P.Min.Y - 30.f, -PitDepth), FVector(P.Max.X, P.Min.Y, 0.f));
			Box(FVector(P.Min.X, P.Max.Y, -PitDepth), FVector(P.Max.X, P.Max.Y + 30.f, 0.f));
			Box(FVector(P.Min.X, P.Min.Y + 60.f, -PitDepth), FVector(P.Min.X + 60.f, P.Max.Y - 60.f, -PitDepth * 0.5f));
			Box(FVector(P.Max.X - 60.f, P.Min.Y + 60.f, -PitDepth), FVector(P.Max.X, P.Max.Y - 60.f, -PitDepth * 0.5f));
		}
	}
	if (bDraw)
	{
		for (int32 y = 0; y < CY; ++y)
		{
			for (int32 x = 0; x < CX; ++x)
			{
				const FVector C(SX * (x + 0.5f), -D * 0.5f + SY * (y + 0.5f), 0.f);
				const uint8 Kc = Cell[y * CX + x];
				if (Kc == 0)
				{
					const FSurf S = ((x + y) % 3 == 0 ? L.FloorAlt : L.Floor).Dim(0.9f + 0.16f * K.Rand(C, 31));
					K.Add(S, FVector(SX - 4.f, SY - 4.f, 24.f + 4.f * K.Rand(C, 33)), C + FVector(0, 0, -12.f), FRotator(0.f, 0.6f * K.Jit(C, 35), 0.f));
				}
				else if (Kc == 1)
				{
					// Thick ice: dark blue, a hand-span over black water.
					K.Add(L.IceDeep.Dim(0.85f + 0.2f * K.Rand(C, 37)), FVector(SX - 3.f, SY - 3.f, 30.f), C + FVector(0, 0, -15.f));
				}
				else if (Kc == 3)
				{
					K.Add(FSurf::Glass(FLinearColor(0.004f, 0.008f, 0.014f), 0.f), FVector(SX, SY, 4.f), C + FVector(0, 0, -PitDepth * 0.55f));
				}
				else
				{
					K.Add(FSurf::Glass(FLinearColor(0.004f, 0.008f, 0.014f), 0.f), FVector(SX, SY, 4.f), C + FVector(0, 0, -PitDepth * 0.55f));
				}
			}
		}
		// Thick ice patches laid on stone floors (the Elder's arena): dark blue slabs set flush.
		for (const FBox2D& T : ThickRects)
		{
			if (PoolRects.ContainsByPredicate([&T](const FBox2D& P) { return P.Intersect(T); })) continue;
			K.Add(L.IceDeep, FVector(T.GetSize(), 4.f), FVector(T.GetCenter(), 1.5f));
		}
		// The pit's sides and the step to climb out by.
		for (const FBox2D& P : PoolRects)
		{
			K.Add(L.Dark, FVector(P.GetSize().X, 30.f, PitDepth), FVector(P.GetCenter().X, P.Min.Y - 15.f, -PitDepth * 0.5f));
			K.Add(L.Dark, FVector(P.GetSize().X, 30.f, PitDepth), FVector(P.GetCenter().X, P.Max.Y + 15.f, -PitDepth * 0.5f));
			K.Add(L.StoneAlt.Dim(0.6f), FVector(60.f, P.GetSize().Y - 120.f, PitDepth * 0.5f), FVector(P.Min.X + 30.f, P.GetCenter().Y, -PitDepth * 0.75f));
			K.Add(L.StoneAlt.Dim(0.6f), FVector(60.f, P.GetSize().Y - 120.f, PitDepth * 0.5f), FVector(P.Max.X - 30.f, P.GetCenter().Y, -PitDepth * 0.75f));
		}
	}

	// ------------------------------------------------ walls with their door sockets
	TArray<FWallSide> Sides;
	{
		FWallSide West{ FVector(0, -D * 0.5f, 0), FVector(0, D * 0.5f, 0), {} };
		FWallSide East{ FVector(W, -D * 0.5f, 0), FVector(W, D * 0.5f, 0), {} };
		FWallSide North{ FVector(0, -D * 0.5f, 0), FVector(W, -D * 0.5f, 0), {} };
		FWallSide South{ FVector(0, D * 0.5f, 0), FVector(W, D * 0.5f, 0), {} };
		const bool bFirst = Index == 0;
		const bool bLastMain = R.Side == 0 && !(Rooms.IsValidIndex(Index + 1) && Rooms[Index + 1].Side == 0);
		if (R.Side != 0 || !bFirst) West.Doors.Add(0.f);
		if (R.Side == 0 && !bLastMain) East.Doors.Add(0.f);
		if (R.BranchDoors & 1) North.Doors.Add(W * 0.5f);
		if (R.BranchDoors & 2) South.Doors.Add(W * 0.5f);
		Sides = { West, East, North, South };
	}
	for (const FWallSide& S : Sides)
	{
		const FVector Dir = (S.B - S.A).GetSafeNormal();
		const float Len = FVector::Dist(S.A, S.B);
		// Away points out of the room.
		const FVector Mid = (S.A + S.B) * 0.5f;
		const FVector Away = (Mid - FVector(W * 0.5f, 0.f, 0.f)).GetSafeNormal2D();
		const FVector Off = Away * (WallT * 0.5f);
		// Spans between the openings (measured from A along the wall).
		TArray<FVector2D> Spans;   // [from, to] along
		float From = -WallT;
		TArray<float> Gaps;
		for (const float G : S.Doors)
		{
			// A door's "along" is a coordinate on the wall: for west/east walls it's y (0 = middle), for north/south x.
			const bool bAlongY = S.A.X == S.B.X;
			Gaps.Add(bAlongY ? G + D * 0.5f : G);
		}
		Gaps.Sort();
		for (const float G : Gaps)
		{
			Spans.Add(FVector2D(From, G - DoorW * 0.5f));
			From = G + DoorW * 0.5f;
		}
		Spans.Add(FVector2D(From, Len + WallT));
		for (const FVector2D& Sp : Spans)
		{
			if (Sp.Y - Sp.X < 5.f) continue;
			const FVector P0 = S.A + Dir * Sp.X + Off, P1 = S.A + Dir * Sp.Y + Off;
			const FVector C = (P0 + P1) * 0.5f;
			const FVector Ext = (FMath::Abs(Dir.X) > 0.5f ? FVector((Sp.Y - Sp.X) * 0.5f, WallT * 0.5f, H * 0.5f + 20.f) : FVector(WallT * 0.5f, (Sp.Y - Sp.X) * 0.5f, H * 0.5f + 20.f));
			Box(C + FVector(0, 0, H * 0.5f) - Ext, C + FVector(0, 0, H * 0.5f) + Ext);
		}
		// Over each opening, the lintel (collision) up to the ceiling.
		for (const float G : Gaps)
		{
			const FVector P = S.A + Dir * G + Off;
			const FVector Ext = FMath::Abs(Dir.X) > 0.5f ? FVector(DoorW * 0.5f, WallT * 0.5f, (H - DoorH) * 0.5f + 20.f) : FVector(WallT * 0.5f, DoorW * 0.5f, (H - DoorH) * 0.5f + 20.f);
			if (H - DoorH > 5.f) Box(P + FVector(0, 0, (H + DoorH) * 0.5f) - Ext, P + FVector(0, 0, (H + DoorH) * 0.5f) + Ext);
		}
		if (!bDraw) continue;
		// The look: coursed basalt ashlar (60 cm courses, offset joints, a darker plinth course and a bronze-trimmed
		// cornice), or glacier ice panels over a fieldstone base course, with stones caught in the ice.
		const float Course = L.bFrost ? 80.f : 72.f;
		const FRotator WallRot(0.f, Dir.Rotation().Yaw, 0.f);
		for (const FVector2D& Sp : Spans)
		{
			const float SpanLen = Sp.Y - Sp.X;
			if (SpanLen < 5.f) continue;
			if (L.bFrost)
			{
				const int32 Panels = FMath::Max(1, FMath::RoundToInt(SpanLen / 220.f));
				for (int32 p = 0; p < Panels; ++p)
				{
					const float A0 = Sp.X + SpanLen * p / Panels, A1 = Sp.X + SpanLen * (p + 1) / Panels;
					const FVector C = S.A + Dir * ((A0 + A1) * 0.5f) + Off;
					K.Add(L.StoneAlt.Dim(0.75f + 0.2f * K.Rand(C, 41)), FVector(A1 - A0 - 4.f, WallT, Course), C + FVector(0, 0, Course * 0.5f), WallRot);
					K.Add(L.Ice.Dim(0.82f + 0.25f * K.Rand(C, 43)), FVector(A1 - A0 - 3.f, WallT - 10.f, H - Course), C + FVector(0, 0, Course + (H - Course) * 0.5f), WallRot);
					// A stone trapped in the ice now and then.
					if (K.Rand(C, 47) < 0.4f) K.Add(L.Dark.Dim(1.6f), FVector(35.f, 20.f, 28.f), C + FVector(0, 0, Course + 90.f + 260.f * K.Rand(C, 49)) - Away * 12.f, WallRot + FRotator(20.f * K.Jit(C, 51), 0.f, 30.f * K.Jit(C, 53)));
				}
			}
			else
			{
				const int32 Courses = FMath::Max(1, FMath::CeilToInt(H / Course));
				for (int32 c = 0; c < Courses; ++c)
				{
					const float Z0 = c * Course, Z1 = FMath::Min(H, (c + 1) * Course);
					const float Stagger = (c % 2) * 80.f;
					float A = Sp.X - Stagger;
					while (A < Sp.Y)
					{
						const float Blk = 140.f + 50.f * K.Rand(S.A + Dir * A + FVector(0, 0, Z0), 55);
						const float A0 = FMath::Max(A, Sp.X), A1 = FMath::Min(A + Blk, Sp.Y);
						if (A1 - A0 > 8.f)
						{
							const FVector C = S.A + Dir * ((A0 + A1) * 0.5f) + Off + FVector(0, 0, (Z0 + Z1) * 0.5f);
							const FSurf Sf = c == 0 ? L.Dark.Dim(1.4f) : ((c + int32(A / 100.f)) % 3 == 0 ? L.StoneAlt : L.Stone);
							K.Add(Sf.Dim(0.85f + 0.3f * K.Rand(C, 57)), FVector(A1 - A0 - 3.f, WallT + 4.f * K.Rand(C, 59), Z1 - Z0 - 3.f), C, WallRot);
						}
						A += Blk;
					}
				}
				// The cornice: a bronze band under the ceiling, and pilasters every 8 m.
				const FVector C = S.A + Dir * ((Sp.X + Sp.Y) * 0.5f) + Off - Away * (WallT * 0.5f + 6.f);
				K.Add(L.Trim, FVector(SpanLen, 10.f, 14.f), C + FVector(0, 0, H - 40.f), WallRot);
				for (float P = FMath::CeilToFloat((Sp.X + 50.f) / 800.f) * 800.f; P < Sp.Y - 50.f; P += 800.f)
				{
					const FVector PC = S.A + Dir * P + Off - Away * (WallT * 0.5f + 18.f);
					K.Add(L.StoneAlt.Dim(0.8f), FVector(70.f, 36.f, H), PC + FVector(0, 0, H * 0.5f), WallRot);
				}
			}
		}
		// Door frames: jambs and a lintel stone (volcanic: bronze-banded; tundra: whale-bone ribs as an arch).
		for (const float G : Gaps)
		{
			const FVector P = S.A + Dir * G + Off;
			for (const float J : { -1.f, 1.f })
			{
				const FVector JC = P + Dir * (J * (DoorW * 0.5f + 20.f)) - Away * (WallT * 0.5f + 8.f);
				K.Add(L.bFrost ? L.Bone : L.StoneAlt.Dim(1.2f), FVector(40.f, WallT + 30.f, DoorH), JC + FVector(0, 0, DoorH * 0.5f), WallRot);
			}
			K.Add(L.bFrost ? L.Bone : L.StoneAlt.Dim(1.25f), FVector(DoorW + 80.f, WallT + 34.f, 50.f), P - Away * 8.f + FVector(0, 0, DoorH + 25.f), WallRot);
			if (!L.bFrost) K.Add(L.Trim, FVector(DoorW + 84.f, WallT + 38.f, 8.f), P - Away * 8.f + FVector(0, 0, DoorH + 4.f), WallRot);
			if (H - DoorH - 50.f > 5.f)
			{
				K.Add(L.bFrost ? L.Ice : L.Stone, FVector(DoorW, WallT, H - DoorH - 50.f), P + FVector(0, 0, DoorH + 50.f + (H - DoorH - 50.f) * 0.5f), WallRot);
			}
		}
	}

	// ------------------------------------------------ the ceiling: a closed slab on cross beams every 4 m
	Box(FVector(-WallT, -D * 0.5f - WallT, H), FVector(W + WallT, D * 0.5f + WallT, H + 50.f));
	if (bDraw)
	{
		const int32 Bays = FMath::Max(1, FMath::RoundToInt(W / 400.f));
		for (int32 b = 0; b < Bays; ++b)
		{
			const float X0 = W * b / Bays, X1 = W * (b + 1) / Bays;
			K.Add(L.bFrost ? L.Ice.Dim(0.7f) : L.Dark, FVector(X1 - X0, D + 2.f * WallT, 50.f), FVector((X0 + X1) * 0.5f, 0.f, H + 25.f));
			if (b > 0) K.Add(L.Beam.Dim(0.9f + 0.15f * K.Rand(FVector(X0, 0, H), 61)), FVector(40.f, D, 46.f), FVector(X0, 0.f, H - 23.f));
		}
		if (!L.bFrost)
		{
			for (int32 b = 1; b < Bays; ++b) for (const float Y : { -D * 0.5f + 20.f, D * 0.5f - 20.f }) K.Add(L.Trim, FVector(48.f, 30.f, 52.f), FVector(W * b / Bays, Y, H - 26.f));
		}
	}

	// ------------------------------------------------ features (each drawn as what it is; collision where it stands)
	int32 LampCount = 0;
	if (Features)
	{
		for (const TSharedPtr<FJsonValue>& F : *Features)
		{
			const FString Kind = PAtStr(F, 0);
			const FVector At(M(PAt(F, 1)), M(PAt(F, 2)), 0.f);
			if (Kind == TEXT("pillar"))
			{
				Box(At + FVector(-50.f, -50.f, 0.f), At + FVector(50.f, 50.f, H));
				if (!bDraw) continue;
				if (L.bFrost)
				{
					K.Add(L.StoneAlt.Dim(0.8f), FVector(140.f, 140.f, 50.f), At + FVector(0, 0, 25.f), FRotator(0.f, 45.f * K.Rand(At, 3), 0.f), EWRLookMesh::Cylinder);
					K.Add(L.Ice.Dim(0.95f), FVector(100.f, 100.f, H - 50.f), At + FVector(0, 0, 50.f + (H - 50.f) * 0.5f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					K.Add(L.Ice.Dim(1.15f), FVector(150.f, 150.f, 70.f), At + FVector(0, 0, H - 35.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				}
				else
				{
					K.Hex(L.Dark.Dim(1.3f), At.X, At.Y, 0.f, 40.f, 70.f, 15.f);
					K.Hex(L.Stone, At.X, At.Y, 40.f, H - 60.f, 46.f, 0.f);
					K.Add(L.Trim, FVector(100.f, 100.f, 12.f), At + FVector(0, 0, 220.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
					K.Hex(L.StoneAlt, At.X, At.Y, H - 60.f, H, 66.f, 30.f);
				}
			}
			else if (Kind == TEXT("ledge"))
			{
				const float LW = M(PAt(F, 3)), LD = M(PAt(F, 4)), LH = M(PAt(F, 5, 1.6f));
				const FVector C(At.X, At.Y, 0.f);
				Box(C + FVector(-LW * 0.5f, -LD * 0.5f, 0.f), C + FVector(LW * 0.5f, LD * 0.5f, LH));
				// Steps down toward the room's middle line (y = 0).
				const float Sy = At.Y > 0.f ? -1.f : 1.f;
				const int32 Steps = FMath::Max(2, FMath::RoundToInt(LH / 40.f));
				for (int32 s = 0; s < Steps; ++s)
				{
					const float Zt = LH * (Steps - s - 1) / Steps;
					if (Zt <= 0.f) continue;
					const float Y0 = At.Y + Sy * (LD * 0.5f + 45.f * s), Y1 = Y0 + Sy * 45.f;
					Box(FVector(At.X - 120.f, FMath::Min(Y0, Y1), 0.f), FVector(At.X + 120.f, FMath::Max(Y0, Y1), Zt));
					if (bDraw) K.Add(L.StoneAlt.Dim(0.95f + 0.08f * s), FVector(240.f, 45.f, Zt), FVector(At.X, (Y0 + Y1) * 0.5f, Zt * 0.5f));
				}
				if (!bDraw) continue;
				K.Add(L.bFrost ? L.Snow.Dim(0.9f) : L.Stone.Dim(0.9f), FVector(LW, LD, LH - 12.f), C + FVector(0, 0, (LH - 12.f) * 0.5f));
				K.Add(L.bFrost ? L.Ice : L.Floor.Dim(1.1f), FVector(LW + 10.f, LD + 10.f, 12.f), C + FVector(0, 0, LH - 6.f));
			}
			else if (Kind == TEXT("runnel"))
			{
				const FVector A(M(PAt(F, 1)), M(PAt(F, 2)), 0.f), B(M(PAt(F, 3)), M(PAt(F, 4)), 0.f);
				const FVector Dir = (B - A).GetSafeNormal2D();
				const float Len = FVector::Dist2D(A, B);
				const FRotator Yaw(0.f, Dir.Rotation().Yaw, 0.f);
				if (!bDraw) continue;
				// A cut channel with stone lips and molten lava 6 cm below the floor, lit along its run.
				K.Add(L.Dark, FVector(Len + 40.f, 120.f, 8.f), (A + B) * 0.5f + FVector(0, 0, 2.f), Yaw);
				K.Add(L.Lava, FVector(Len, 70.f, 6.f), (A + B) * 0.5f + FVector(0, 0, 3.f), Yaw);
				for (const float S : { -1.f, 1.f }) K.Add(L.StoneAlt.Dim(0.7f), FVector(Len + 40.f, 22.f, 12.f), (A + B) * 0.5f + FVector::CrossProduct(FVector::UpVector, Dir) * (S * 46.f) + FVector(0, 0, 6.f), Yaw);
				for (float T = 150.f; T < Len; T += 600.f) Lamp(A + Dir * T + FVector(0, 0, 40.f), 3200.f, 800.f, L.GlowColour);
			}
			else if (Kind == TEXT("crucible"))
			{
				if (!bDraw) continue;
				const FVector C = At + FVector(0, 0, H - 300.f);
				K.Add(L.Iron, FVector(110.f, 110.f, 90.f), C, FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				K.Add(L.Lava, FVector(92.f, 92.f, 6.f), C + FVector(0, 0, 44.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				K.Add(L.Iron.Dim(1.3f), FVector(124.f, 124.f, 10.f), C + FVector(0, 0, 46.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				// Four chains (rods of links) to the beam above.
				for (int32 c = 0; c < 4; ++c)
				{
					const FVector Hook = C + FRotator(0.f, 45.f + 90.f * c, 0.f).Vector() * 55.f + FVector(0, 0, 40.f);
					K.Rod(L.Iron, Hook, At + FVector(0, 0, H - 30.f), 6.f);
				}
				K.Add(L.Beam, FVector(60.f, D, 46.f), FVector(At.X, 0.f, H - 23.f));
				Lamp(C + FVector(0, 0, -80.f), 4500.f, 1200.f, L.GlowColour);
			}
			else if (Kind == TEXT("anvil"))
			{
				const FRotator Y(0.f, PAt(F, 3), 0.f);
				Box(At + FVector(-60.f, -60.f, 0.f), At + FVector(60.f, 60.f, 85.f));
				if (!bDraw) continue;
				K.Add(L.Wood.Dim(0.8f), FVector(90.f, 90.f, 55.f), At + FVector(0, 0, 27.f), Y, EWRLookMesh::Cylinder);
				K.Add(L.Iron, FVector(48.f, 30.f, 22.f), At + FVector(0, 0, 66.f), Y);
				K.Add(L.Iron.Dim(1.2f), FVector(80.f, 34.f, 14.f), At + FVector(0, 0, 84.f), Y);
				K.Spike(L.Iron.Dim(1.2f), At + FVector(0, 0, 84.f) + Y.RotateVector(FVector(38.f, 0, 0)), Y.Vector(), 26.f, 42.f);
			}
			else if (Kind == TEXT("brazier"))
			{
				Box(At + FVector(-45.f, -45.f, 0.f), At + FVector(45.f, 45.f, 110.f));
				if (!bDraw) continue;
				for (int32 l = 0; l < 3; ++l) K.Rod(L.Iron, At + FRotator(0.f, 120.f * l, 0.f).Vector() * 40.f, At + FVector(0, 0, 95.f), 6.f);
				K.Add(L.Iron.Dim(1.2f), FVector(80.f, 80.f, 26.f), At + FVector(0, 0, 100.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				K.Add(L.Ember, FVector(64.f, 64.f, 8.f), At + FVector(0, 0, 112.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
				if (USceneComponent* Fx = WRFx::Attach(RootComponent, TEXT("brazier.fire"), NAME_None, 0.9f)) Fx->SetRelativeLocation(Local.TransformPosition(At + FVector(0, 0, 118.f)));
				Lamp(At + FVector(0, 0, 160.f), 5000.f, 1300.f, L.LampColour);
			}
			else if (Kind == TEXT("lamp"))
			{
				const FString Wall = PAtStr(F, 1);
				const float Along = M(PAt(F, 2));
				FVector P; FRotator Face;
				if (Wall == TEXT("n")) { P = FVector(Along, -D * 0.5f + 30.f, 230.f); Face = FRotator(0.f, 90.f, 0.f); }
				else if (Wall == TEXT("s")) { P = FVector(Along, D * 0.5f - 30.f, 230.f); Face = FRotator(0.f, -90.f, 0.f); }
				else if (Wall == TEXT("w")) { P = FVector(30.f, Along, 230.f); Face = FRotator(0.f, 0.f, 0.f); }
				else { P = FVector(W - 30.f, Along, 230.f); Face = FRotator(0.f, 180.f, 0.f); }
				++LampCount;
				if (!bDraw) continue;
				if (L.bFrost)
				{
					// A qulliq in a niche cut into the ice: a half-moon soapstone dish, its straight edge burning.
					K.Add(L.Dark.Dim(2.f), FVector(70.f, 70.f, 60.f), P + Face.Vector() * -10.f + FVector(0, 0, 10.f), Face);
					K.Add(FSurf::Stone(FLinearColor(0.2f, 0.21f, 0.19f), 0.6f), FVector(40.f, 40.f, 10.f), P + FVector(0, 0, -15.f), Face, EWRLookMesh::Cylinder);
					K.Add(L.LampGlow, FVector(4.f, 30.f, 6.f), P + Face.Vector() * 12.f + FVector(0, 0, -8.f), Face);
				}
				else
				{
					// An iron cage on a bracket with glowing coals.
					K.Rod(L.Iron, P - Face.Vector() * 30.f, P + FVector(0, 0, -10.f), 6.f, true);
					for (int32 b = 0; b < 4; ++b) K.Rod(L.Iron, P + FRotator(0.f, 90.f * b + 45.f, 0.f).Vector() * 16.f + FVector(0, 0, -20.f), P + FRotator(0.f, 90.f * b + 45.f, 0.f).Vector() * 16.f + FVector(0, 0, 20.f), 3.f);
					K.Add(L.Ember, FVector(24.f, 24.f, 14.f), P + FVector(0, 0, -12.f), Face);
				}
				Lamp(P + Face.Vector() * 40.f, L.bFrost ? 2600.f : 3200.f, 1100.f, L.LampColour);
			}
			else if (Kind == TEXT("rack"))
			{
				const FRotator Y(0.f, PAt(F, 3), 0.f);
				WRFoldLook::Block(this, RootComponent, Local.TransformPosition(At + FVector(0, 0, 105.f)), FVector(25.f, 120.f, 105.f), (Local.GetRotation() * Y.Quaternion()).Rotator());
				if (!bDraw) continue;
				const auto P = [&](const FVector& V) { return At + Y.RotateVector(V); };
				if (L.bFrost)
				{
					// A drying frame of whale-bone posts with hides stretched on it.
					for (const float S : { -110.f, 110.f }) K.Rod(L.Bone, P(FVector(0, S, 0)), P(FVector(0, S, 210.f)), 14.f);
					K.Rod(L.Bone, P(FVector(0, -120.f, 200.f)), P(FVector(0, 120.f, 200.f)), 10.f);
					K.Add(L.Hide, FVector(4.f, 90.f, 120.f), P(FVector(4.f, -50.f, 130.f)), Y + FRotator(0.f, 0.f, 3.f));
					K.Add(L.Hide.Dim(0.8f), FVector(4.f, 80.f, 110.f), P(FVector(4.f, 52.f, 135.f)), Y + FRotator(0.f, 0.f, -4.f));
				}
				else
				{
					// A smith's rack: oak posts, two rails, tongs and hammers hung on it.
					for (const float S : { -110.f, 110.f }) K.Add(L.Wood, FVector(14.f, 14.f, 190.f), P(FVector(0, S, 95.f)), Y);
					for (const float Z : { 90.f, 170.f }) K.Add(L.Wood.Dim(0.9f), FVector(10.f, 240.f, 10.f), P(FVector(0, 0, Z)), Y);
					for (int32 t = 0; t < 5; ++t)
					{
						const float S = -80.f + 40.f * t;
						if (t % 2) { K.Rod(L.Iron, P(FVector(8.f, S - 6.f, 165.f)), P(FVector(10.f, S - 10.f, 80.f)), 3.f); K.Rod(L.Iron, P(FVector(8.f, S + 6.f, 165.f)), P(FVector(10.f, S + 10.f, 80.f)), 3.f); }
						else { K.Rod(L.Wood, P(FVector(8.f, S, 168.f)), P(FVector(8.f, S, 110.f)), 4.f); K.Add(L.Iron, FVector(10.f, 20.f, 10.f), P(FVector(8.f, S, 104.f)), Y); }
					}
				}
			}
			else if (Kind == TEXT("rubble") || Kind == TEXT("cinder"))
			{
				const float Rr = M(PAt(F, 3, 1.4f));
				const bool bCinder = Kind == TEXT("cinder");
				UMaterialInterface* Mat = WRFoldLook::RockMat(this, bCinder ? FLinearColor(0.035f, 0.03f, 0.03f) : (L.bFrost ? FLinearColor(0.6f, 0.65f, 0.7f) : FLinearColor(0.09f, 0.08f, 0.075f)));
				const int32 Count = 3 + FMath::RoundToInt(Rr / 80.f);
				for (int32 r = 0; r < Count; ++r)
				{
					const FVector Off = FRotator(0.f, 360.f * r / Count + 40.f * K.Jit(At, r), 0.f).Vector() * Rr * (r == 0 ? 0.f : 0.55f);
					const float S = (r == 0 ? 1.f : 0.6f) * Rr;
					WRFoldLook::Rock(this, RootComponent, r + Index, FVector(S, S * 0.9f, S * 0.6f), Local.TransformPosition(At + Off), Local.Rotator() + FRotator(0.f, 70.f * r, 0.f), Mat, true, 8.f);
				}
				if (bCinder && bDraw)
				{
					// Embers still glowing in the slag.
					for (int32 g = 0; g < 4; ++g) K.Add(L.Ember, FVector(14.f, 14.f, 6.f), At + FRotator(0.f, 90.f * g + 20.f, 0.f).Vector() * Rr * 0.4f + FVector(0, 0, Rr * 0.25f), FRotator(10.f, 30.f * g, 0.f));
					Lamp(At + FVector(0, 0, 80.f), 900.f, 400.f, L.GlowColour);
				}
			}
			else if (Kind == TEXT("statue"))
			{
				const FRotator Y(0.f, PAt(F, 3), 0.f);
				Box(At + FVector(-70.f, -70.f, 0.f), At + FVector(70.f, 70.f, L.bFrost ? 220.f : 300.f));
				if (!bDraw) continue;
				const auto P = [&](const FVector& V) { return At + Y.RotateVector(V); };
				if (L.bFrost)
				{
					// An inuksuk, man-high: two legs, a lintel, a body, arms, a head.
					const FSurf S = FSurf::Stone(FLinearColor(0.33f, 0.36f, 0.39f), 0.9f);
					K.Add(S, FVector(40.f, 42.f, 80.f), P(FVector(0, -32.f, 40.f)), Y);
					K.Add(S.Dim(0.9f), FVector(38.f, 40.f, 76.f), P(FVector(0, 32.f, 38.f)), Y);
					K.Add(S.Dim(1.1f), FVector(52.f, 120.f, 28.f), P(FVector(0, 0, 94.f)), Y);
					K.Add(S, FVector(48.f, 64.f, 56.f), P(FVector(0, 0, 136.f)), Y);
					K.Add(S.Dim(0.95f), FVector(32.f, 170.f, 20.f), P(FVector(0, 0, 174.f)), Y);
					K.Add(S.Dim(0.85f), FVector(36.f, 36.f, 36.f), P(FVector(0, 0, 202.f)), Y);
				}
				else
				{
					// A smith in basalt on a plinth, hammer on shoulder: the Smiths remembered their own.
					const FSurf S = L.Stone.Dim(1.3f);
					K.Add(L.Dark.Dim(1.4f), FVector(130.f, 130.f, 50.f), P(FVector(0, 0, 25.f)), Y);
					K.Add(S, FVector(34.f, 34.f, 90.f), P(FVector(0, -20.f, 95.f)), Y);
					K.Add(S, FVector(34.f, 34.f, 90.f), P(FVector(0, 20.f, 95.f)), Y);
					K.Add(S.Dim(1.05f), FVector(54.f, 80.f, 90.f), P(FVector(0, 0, 185.f)), Y);
					K.Add(L.Hide.Dim(0.6f), FVector(58.f, 70.f, 70.f), P(FVector(6.f, 0, 160.f)), Y);
					K.Rod(S, P(FVector(0, -48.f, 220.f)), P(FVector(26.f, -52.f, 160.f)), 22.f);
					K.Rod(S, P(FVector(0, 48.f, 220.f)), P(FVector(-4.f, 56.f, 250.f)), 22.f);
					K.Rod(L.Wood.Dim(0.5f), P(FVector(-4.f, 56.f, 250.f)), P(FVector(-30.f, 58.f, 300.f)), 7.f);
					K.Add(S.Dim(0.9f), FVector(26.f, 40.f, 22.f), P(FVector(-34.f, 58.f, 306.f)), Y);
					K.Add(S.Dim(1.1f), FVector(36.f, 34.f, 42.f), P(FVector(0, 0, 252.f)), Y);
				}
			}
			else if (Kind == TEXT("bones"))
			{
				if (!bDraw) continue;
				for (int32 b = 0; b < 7; ++b)
				{
					const FVector C = At + FVector(70.f * K.Jit(At, b * 3), 70.f * K.Jit(At, b * 3 + 1), 4.f);
					K.Rod(L.Bone.Dim(0.9f + 0.2f * K.Rand(C, b)), C, C + FRotator(0.f, 360.f * K.Rand(C, b + 9), 0.f).Vector() * (35.f + 25.f * K.Rand(C, b + 4)), 6.f);
				}
				K.Add(L.Bone, FVector(22.f, 18.f, 18.f), At + FVector(20.f, -14.f, 9.f), FRotator(0.f, 40.f, 10.f), EWRLookMesh::Cylinder);
			}
			else if (Kind == TEXT("icicles"))
			{
				if (!bDraw) continue;
				const float Rr = M(PAt(F, 3, 3.f));
				for (int32 c = 0; c < 9; ++c)
				{
					const FVector C = At + FVector(Rr * K.Jit(At, c * 2), Rr * K.Jit(At, c * 2 + 1), H - 5.f);
					K.Spike(FSurf::Glass(FLinearColor(0.7f, 0.85f, 1.f), 0.12f), C, -FVector::UpVector, 14.f + 10.f * K.Rand(C, 3), 50.f + 90.f * K.Rand(C, 5));
				}
			}
			else if (Kind == TEXT("kiln"))
			{
				Box(At + FVector(-200.f, -200.f, 0.f), At + FVector(200.f, 200.f, 380.f));
				if (!bDraw) continue;
				// A beehive kiln of fired brick: courses stepping in to a smoke hole, a glowing mouth toward the room.
				for (int32 c = 0; c < 8; ++c)
				{
					const float Rr = 200.f - c * c * 2.6f;
					K.Hex(((c % 2) ? L.Stone : L.StoneAlt).Dim(1.2f), At.X, At.Y, c * 47.f, c * 47.f + 46.f, Rr, 15.f * c);
				}
				const FVector Mouth = At + FVector(-205.f, 0.f, 70.f);
				K.Add(L.Lava, FVector(10.f, 110.f, 90.f), Mouth, FRotator::ZeroRotator);
				K.Add(L.Trim, FVector(24.f, 140.f, 16.f), Mouth + FVector(0, 0, 54.f));
				Lamp(Mouth + FVector(-90.f, 0, 0), 6000.f, 1400.f, L.GlowColour);
			}
			else if (Kind == TEXT("tank"))
			{
				Box(At + FVector(-150.f, -100.f, 0.f), At + FVector(150.f, 100.f, 100.f));
				if (!bDraw) continue;
				for (const float S : { -1.f, 1.f })
				{
					K.Add(L.StoneAlt, FVector(300.f, 26.f, 100.f), At + FVector(0, S * 87.f, 50.f));
					K.Add(L.StoneAlt, FVector(26.f, 200.f, 100.f), At + FVector(S * 137.f, 0, 50.f));
				}
				K.Add(FSurf::Glass(FLinearColor(0.02f, 0.035f, 0.05f), 0.f), FVector(250.f, 150.f, 4.f), At + FVector(0, 0, 82.f));
				K.Add(L.Trim, FVector(304.f, 204.f, 6.f), At + FVector(0, 0, 103.f));
			}
		}
	}
	// A room without any lamp of its own still has the two wall lamps every hall had by its door.
	if (LampCount == 0)
	{
		for (const float S : { -1.f, 1.f })
		{
			const FVector P(140.f, S * (D * 0.5f - 30.f), 230.f);
			if (bDraw)
			{
				K.Add(L.bFrost ? L.Dark.Dim(2.f) : L.Iron, FVector(40.f, 30.f, 30.f), P);
				K.Add(L.LampGlow, FVector(20.f, 16.f, 10.f), P + FVector(0, -S * 10.f, -6.f));
			}
			Lamp(P + FVector(0, -S * 40.f, 0), 2600.f, 1000.f, L.LampColour);
		}
	}
	if (bDraw) FWRBuildLook::Spawn(this, RootComponent, Blocks, FLinearColor(0.3f, 0.25f, 0.2f), false);
}
