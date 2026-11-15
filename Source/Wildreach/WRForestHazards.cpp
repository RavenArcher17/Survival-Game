// The Ancient forest's hazards and world state (cooks/biome-forest.md 2.8-2.10, cooks/build-biome-forest.md 2.7, 2.13).
// See WRForestHazards.h. Placement comes from Data/forest.json (FWRForestData), numbers from tuning hazards.*.

#include "WRForestHazards.h"
#include "Wildreach.h"
#include "WRForestTypes.h"
#include "WRTreeKit.h"
#include "WRToolMesh.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFarm.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRStormTypes.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "WRSocial.h"
#include "WRBuilding.h"
#include "WRForestHollow.h"
#include "WRForestWorld.h"
#include "WRLand.h"
#include "WRStormWorld.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "AbilitySystemComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace WRForestTags
{
	UE_DEFINE_GAMEPLAY_TAG(Hazard_Thornbrake, "Hazard.Thornbrake");
}

namespace WRForestHzImpl
{
	/** hazards.<Key> (Data/tuning.json). */
	float Tn(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("hazards.")) + Key), Fallback); }
	/** Server time as every machine sees it (the game state's): every replicated moment here is on this clock. */
	float Now(const UObject* Ctx) { return UWRWeatherSubsystem::Now(Ctx); }
	double Hours(const UObject* Ctx) { const AWRGameState* GS = AWRGameState::Get(Ctx); return GS ? GS->GetTotalHours() : 0.0; }
	/** Game hours as real seconds (clock.dayMinutes real minutes a day). */
	float HoursToSeconds(float H) { return H * WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f / 24.f; }
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	FColor Pack(const FLinearColor& C, float Rough)
	{
		return FColor(FMath::Clamp(FMath::RoundToInt(C.R * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(C.G * 255.f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(C.B * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Rough * 255.f), 0, 255));
	}

	// The forest palette (cooks/build-biome-forest.md 2.3, DC 2.8): bramble canes dark red-brown with pale thorn tips, dead
	// limbs grey, leaf litter brown, wasp paper grey with darker bands, char black with ember orange.
	const FLinearColor Cane = Srgb(74, 38, 30);
	const FLinearColor ThornTip = Srgb(214, 200, 168);
	const FLinearColor DeadWood = Srgb(126, 122, 114);
	const FLinearColor DeadWoodDark = Srgb(88, 84, 78);
	const FLinearColor Litter = Srgb(106, 80, 48);
	const FLinearColor LitterDark = Srgb(74, 58, 42);
	const FLinearColor Moss = Srgb(94, 140, 49);
	const FLinearColor Paper = Srgb(150, 146, 136);
	const FLinearColor PaperBand = Srgb(104, 98, 90);
	const FLinearColor CharBlack = Srgb(20, 16, 14);
	const FLinearColor Ember = Srgb(194, 74, 18);
	const FLinearColor Bark = Srgb(84, 66, 50);

	bool Ground(UWorld* W, const FVector& At, FVector& Out, float Up = 3000.f, float Down = 6000.f, const AActor* Ignore = nullptr)
	{
		if (!W) return false;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestGround), false, Ignore);
		if (W->LineTraceSingleByChannel(Hit, At + FVector(0, 0, Up), At - FVector(0, 0, Down), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor()))
		{
			Out = Hit.ImpactPoint;
			return true;
		}
		return false;
	}

	bool IsPlayer(const AWRCharacterBase* C) { return C && !C->IsCreature() && C->GetPlayerState() != nullptr; }

	/** The local view (the camera of this machine's first player), for building looks near it. */
	bool View(const UWorld* W, FVector& Out)
	{
		const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->IsLocalController()) return false;
		FRotator R;
		PC->GetPlayerViewPoint(Out, R);
		return true;
	}

	/** A procedural mesh section in the held-tool material (vertex colour albedo, roughness in alpha), two-sided by
	 *  writing each triangle both ways (litter mats and water seen from either side). */
	void Section(UProceduralMeshComponent* C, int32 Index, const TArray<FVector>& V, const TArray<int32>& T, const TArray<FVector>& N, const TArray<FVector2D>& UV, const TArray<FColor>& Col, UMaterialInterface* Mat, bool bTwoSided)
	{
		if (!C) return;
		TArray<int32> Tris = T;
		if (bTwoSided) for (int32 i = 0; i + 2 < T.Num(); i += 3) Tris.Append({ T[i], T[i + 2], T[i + 1] });
		C->CreateMeshSection(Index, V, Tris, N, UV, Col, TArray<FProcMeshTangent>(), false);
		if (Mat) C->SetMaterial(Index, Mat);
	}

	UProceduralMeshComponent* NewMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(Owner, Name);
		M->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->RegisterComponent();
		return M;
	}

	/** A seeded random in -1..1 (the same on every machine for the same place). */
	float Jit(const FVector& P, int32 Salt)
	{
		const float S = FMath::Sin(P.X * 0.0123f + P.Y * 0.0457f + P.Z * 0.0031f + Salt * 12.9898f) * 43758.5453f;
		return FMath::Frac(S) * 2.f - 1.f;
	}

	/** Pawn feet (capsule bottom). */
	FVector Feet(const AWRCharacterBase* C)
	{
		return C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight());
	}

	/** Players: is this body pushing through (moving) rather than standing still? */
	float Speed2D(const AWRCharacterBase* C) { return C->GetVelocity().Size2D(); }

	/** The mount a rider sits on (or null). */
	const AWRCreature* MountOf(const AActor* Rider)
	{
		if (!Rider) return nullptr;
		for (TActorIterator<AWRCreature> It(Rider->GetWorld()); It; ++It) if (It->GetRider() == Rider) return *It;
		return nullptr;
	}

	/** The site (landmark or hand site) by any of its ids or kinds; its cook spot (metres) if the data hasn't one. */
	FVector SiteAt(UWorld* W, std::initializer_list<const TCHAR*> Ids, const FVector2D& CookMetres, float* OutYaw = nullptr)
	{
		const FWRForestData& D = FWRForestData::Get();
		for (const TCHAR* Id : Ids)
		{
			for (const FWRForestData::FSite& S : D.Sites)
			{
				if (S.Id == FName(Id) || S.Kind == FName(Id)) { if (OutYaw) *OutYaw = S.Yaw; return S.At; }
			}
		}
		if (OutYaw) *OutYaw = 0.f;
		FVector G(CookMetres * 100.f, 0.f);
		FVector Hit;
		if (Ground(W, G + FVector(0, 0, 60000.f), Hit, 0.f, 120000.f)) G = Hit;
		return G;
	}
}

// ================================================================================================ the shared calls

float WRForestHazards::MoveScale(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	return C && C->HasTag(WRForestTags::Hazard_Thornbrake) ? WRForestHzImpl::Tn(TEXT("thornbrake.runMultiplier"), 0.5f) : 1.f;
}

bool WRForestHazards::Shielding(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	return C && C->HasTag(WRForestTags::Hazard_Thornbrake) && C->IsAlive();
}

bool WRForest::Bloom(const UObject* WorldContext)
{
	// The Bloom is world.json events.bloom, scheduled and announced by the world systems' event step like the Great
	// Storm; the world state replicates it, so every machine answers the same.
	const AWRWorldState* WS = AWRWorldState::Get(WorldContext);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("bloom")) : nullptr;
	return E && E->Stage == 1 && WRForestHzImpl::Now(WorldContext) < E->EndAt;
}

float WRForest::BloomWarning(const UObject* WorldContext)
{
	const AWRWorldState* WS = AWRWorldState::Get(WorldContext);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("bloom")) : nullptr;
	if (!E || E->Stage != 0) return -1.f;
	return FMath::Max(0.f, E->StartAt - WRForestHzImpl::Now(WorldContext));
}

bool WRForestHazards::HeartsBright(const UObject* WorldContext)
{
	return WRForest::Bloom(WorldContext) || WRForest::BloomWarning(WorldContext) >= 0.f;
}

bool WRForest::Raining(const UObject* WorldContext, const FVector& Pos)
{
	const FName Biome = WRBiome::At(WorldContext, Pos);
	if (Biome != TEXT("forest")) return false;
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(WorldContext, Biome);
	return K && K->Id == TEXT("rain");
}

bool WRForestHazards::Spate(const UObject* WorldContext)
{
	const AWRForestHazardState* S = AWRForestHazardState::Get(WorldContext);
	return S && S->bSpate;
}

bool WRForest::PassesThornbrake(const AActor* Who)
{
	if (!Who) return false;
	// Every forest creature ignores them (the CR rule), and so does anything that resists them (creatures.json resists).
	if (Cast<AWRCreature>(Who)) return true;
	// Raven: paths matter outside combat. Any Nature points, main or subclass, and the brambles part.
	if (WRForest::NaturePoints(Who) > 0) return true;
	// A rider on a mount that walks through them (the Elderelk) goes with it.
	if (const AWRCreature* Mount = WRForestHzImpl::MountOf(Who))
	{
		const FWRCreatureDef* Def = Mount->GetDef();
		if (Def && Def->Resists.Contains(TEXT("thornbrake"))) return true;
	}
	if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who))
	{
		// Briarproof leathers in the chest slot (survival-crafting KEPT), the Heartguard set at 4 pieces.
		const FWRItemStack& Chest = Inv->GetEquipped(EWREquipSlot::Armor);
		if (!Chest.IsEmpty() && Chest.ItemId == FName(*FWRTuning::Get().Str(TEXT("hazards.thornbrake.passItem"), TEXT("briarproof_leathers")))) return true;
		if (Inv->SetPieces(TEXT("heartguard")) >= int32(WRForestHzImpl::Tn(TEXT("thornbrake.passSetPieces"), 4.f))) return true;
	}
	return false;
}

void WRForest::BurnThornbrake(UWorld* World, const FVector& At, float Radius)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	const UWRForestHazards* H = UWRForestHazards::Get(World);
	if (!H || !H->bActive) return;
	TArray<AWRThornbrake*> Near;
	H->BrakesNear(At, Radius + 1500.f, Near);
	// Burns last half as long in the rain (cooks/biome-forest.md 2.9).
	const float Seconds = WRForestHzImpl::HoursToSeconds(WRForestHzImpl::Tn(TEXT("thornbrake.torchGapHours"), 5.f))
		* (WRForest::Raining(World, At) ? WRForestHzImpl::Tn(TEXT("thornbrake.rainBurn"), 0.5f) : 1.f);
	const float Half = WRForestHzImpl::Tn(TEXT("thornbrake.torchGapRadius"), 400.f) * 0.5f;
	for (AWRThornbrake* B : Near)
	{
		float Dist = 0.f;
		B->NearestMid(At, Dist);
		if (Dist <= FMath::Max(Radius, 150.f)) B->Burn(At, FMath::Max(Half, Radius * 0.5f), Seconds);
	}
}

void WRForest::AddSmoke(UWorld* World, const FVector& At, float Radius, float Seconds)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	AWRForestHazardState* S = AWRForestHazardState::Get(World);
	if (!S) return;
	FWRForestSmoke& Sm = S->Smoke.AddDefaulted_GetRef();
	Sm.At = At;
	Sm.Radius = Radius;
	Sm.Until = WRForestHzImpl::Now(World) + Seconds;
	S->ForceNetUpdate();
	WRFx::Broadcast(World, TEXT("forest.smoke"), At, FRotator::ZeroRotator, FMath::Max(0.5f, Radius / 400.f), NAME_None, Seconds);
}

bool WRForest::InSmoke(const UObject* WorldContext, const FVector& Pos)
{
	const AWRForestHazardState* S = AWRForestHazardState::Get(WorldContext);
	if (!S) return false;
	const float T = WRForestHzImpl::Now(WorldContext);
	for (const FWRForestSmoke& Sm : S->Smoke)
	{
		if (T < Sm.Until && FVector::Dist(Sm.At, Pos) < Sm.Radius) return true;
	}
	return false;
}

int32 WRForest::DropWidowmakers(UWorld* World, const FVector& Pos, float Range)
{
	if (!World || World->GetNetMode() == NM_Client) return 0;
	const UWRForestHazards* H = UWRForestHazards::Get(World);
	if (!H) return 0;
	int32 N = 0;
	for (const TWeakObjectPtr<AWRWidowmaker>& W : H->Limbs)
	{
		AWRWidowmaker* L = W.Get();
		if (!L || !L->IsArmed()) continue;
		// The roar shakes the crown over the roarer: limbs whose drop zone it stands in or near.
		if (FVector::Dist2D(L->GetActorLocation(), Pos) <= Range + L->FallRadius)
		{
			L->Drop(nullptr);
			++N;
		}
	}
	return N;
}

bool WRForestHazards::HitThing(AActor* Thing, AWRCharacterBase* By, float Damage, bool bFire, const FVector& At)
{
	if (!Thing) return false;
	UWorld* World = Thing->GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return false;
	// (Fire on brambles is FireLanded's, called by the projectile and the fire blow itself.)
	if (AWRWidowmaker* L = Cast<AWRWidowmaker>(Thing))
	{
		L->TakeShot(Damage, By);
		return true;
	}
	if (AWRWaspNest* N = Cast<AWRWaspNest>(Thing))
	{
		// Any blow, even an arrow, and even at night: the nest wakes and pours out (smoke spares it).
		if (!N->IsCalm()) N->Swarm(By);
		WRFx::Broadcast(N, TEXT("forest.wasp_paper"), At, FRotator::ZeroRotator, 1.f, TEXT("forest.wasp_swarm"));
		return true;
	}
	return false;
}

bool WRForestHazardsLeafPitNear(const FVector& At, float Range, FVector& OutPit)
{
	// Any world will do: find the pits through the world the caller's in (the creature track passes a position only).
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		UWorld* W = Ctx.World();
		if (!W || !W->IsGameWorld() || W->GetNetMode() == NM_Client) continue;
		const UWRForestHazards* H = UWRForestHazards::Get(W);
		if (!H || !H->bActive) continue;
		AWRLeafPit* Best = nullptr;
		float BestD = Range;
		for (const TWeakObjectPtr<AWRLeafPit>& P : H->Pits)
		{
			AWRLeafPit* Pit = P.Get();
			if (!Pit || !Pit->IsWhole()) continue;
			const float D = FVector::Dist(Pit->GetActorLocation(), At);
			if (D < BestD) { BestD = D; Best = Pit; }
		}
		if (!Best) continue;
		OutPit = Best->GetActorLocation();
		Best->Reveal(WRForestHzImpl::Tn(TEXT("leafPit.revealSeconds"), 5.f));
		return true;
	}
	return false;
}

// ================================================================================================ AWRForestHazardState

AWRForestHazardState::AWRForestHazardState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestHazardState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestHazardState, bSpate);
	DOREPLIFETIME(AWRForestHazardState, Smoke);
}

AWRForestHazardState* AWRForestHazardState::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	for (TActorIterator<AWRForestHazardState> It(const_cast<UWorld*>(W)); It; ++It) return *It;
	return nullptr;
}

// ================================================================================================ AWRThornbrake

AWRThornbrake::AWRThornbrake()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(40000.f));
}

void AWRThornbrake::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRThornbrake, bRing);
	DOREPLIFETIME(AWRThornbrake, Radius);
	DOREPLIFETIME(AWRThornbrake, Depth);
	DOREPLIFETIME(AWRThornbrake, Length);
	DOREPLIFETIME(AWRThornbrake, Height);
	DOREPLIFETIME(AWRThornbrake, Guards);
	DOREPLIFETIME(AWRThornbrake, Gaps);
}

void AWRThornbrake::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bRing && Gaps.Num() == 0 && !bGapDecided)
	{
		// Plan 2.6: every giant's root crown has its ring, "with a 2-3 m gap on one side most of the time" (a way in for
		// the player who looks). Seeded by the ring's place so it never moves between runs.
		const FVector C = GetActorLocation();
		const float Roll = WRForestHzImpl::Jit(C, 3) * 0.5f + 0.5f;
		if (Roll < WRForestHzImpl::Tn(TEXT("thornbrake.ringGapShare"), 0.8f))
		{
			const float A = (WRForestHzImpl::Jit(C, 7) * 0.5f + 0.5f) * 2.f * PI;
			FWRBrakeGap G;
			G.At = C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (Radius + Depth * 0.5f);
			G.HalfWidth = WRForestHzImpl::Tn(TEXT("thornbrake.ringGapWidth"), 250.f) * 0.5f;
			G.OpenUntil = 1.0e9f;   // the generator's gap: always open, never charred
			Gaps.Add(G);
		}
	}
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
}

bool AWRThornbrake::Contains(const FVector& Feet, float Pad) const
{
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(Feet);
	if (L.Z < -400.f || L.Z > Height + 300.f) return false;
	float Lateral = 0.f;   // where along the brake's run (cm), for the gaps
	if (bRing)
	{
		const float D = L.Size2D();
		if (D < Radius - Pad || D > Radius + Depth + Pad) return false;
	}
	else
	{
		if (FMath::Abs(L.X) > Length * 0.5f + Pad || FMath::Abs(L.Y) > Depth * 0.5f + Pad) return false;
	}
	for (const FWRBrakeGap& G : Gaps)
	{
		const FVector GL = GetActorTransform().InverseTransformPositionNoScale(G.At);
		if (bRing)
		{
			const float MidR = Radius + Depth * 0.5f;
			const float Da = FMath::Abs(FMath::FindDeltaAngleRadians(FMath::Atan2(L.Y, L.X), FMath::Atan2(GL.Y, GL.X)));
			Lateral = Da * MidR;
		}
		else
		{
			Lateral = FMath::Abs(L.X - GL.X);
		}
		if (Lateral < G.HalfWidth * (OpenAt(G.At) > 0.5f ? 1.f : 0.f)) return false;
	}
	return true;
}

FVector AWRThornbrake::NearestMid(const FVector& At, float& OutDistance) const
{
	const FTransform T = GetActorTransform();
	const FVector L = T.InverseTransformPositionNoScale(At);
	FVector Mid;
	if (bRing)
	{
		const float D = FMath::Max(1.f, L.Size2D());
		const FVector2D Dir = FVector2D(L.X, L.Y) / D;
		const float MidR = Radius + Depth * 0.5f;
		Mid = FVector(Dir * MidR, 0.f);
		OutDistance = FMath::Max(0.f, FMath::Abs(D - MidR) - Depth * 0.5f);
	}
	else
	{
		const float X = FMath::Clamp(L.X, -Length * 0.5f, Length * 0.5f);
		Mid = FVector(X, 0.f, 0.f);
		OutDistance = FVector2D(FMath::Max(0.f, FMath::Abs(L.X) - Length * 0.5f), FMath::Max(0.f, FMath::Abs(L.Y) - Depth * 0.5f)).Size();
	}
	return T.TransformPositionNoScale(Mid);
}

float AWRThornbrake::OpenAt(const FVector& At) const
{
	const float T = WRForestHzImpl::Now(this);
	const float Regrow = FMath::Max(1.f, WRForestHzImpl::Tn(TEXT("thornbrake.regrowSeconds"), 30.f));
	float Best = 0.f;
	for (const FWRBrakeGap& G : Gaps)
	{
		if (FVector::Dist2D(G.At, At) > G.HalfWidth + 50.f) continue;
		Best = FMath::Max(Best, T < G.OpenUntil ? 1.f : FMath::Clamp(1.f - (T - G.OpenUntil) / Regrow, 0.f, 1.f));
	}
	return Best;
}

void AWRThornbrake::Burn(const FVector& At, float HalfWidth, float Seconds)
{
	if (!HasAuthority()) return;
	float Dist = 0.f;
	const FVector Mid = NearestMid(At, Dist);
	const float Until = WRForestHzImpl::Now(this) + Seconds;
	for (FWRBrakeGap& G : Gaps)
	{
		if (FVector::Dist2D(G.At, Mid) < G.HalfWidth)
		{
			if (G.OpenUntil < 1.0e8f) G.OpenUntil = FMath::Max(G.OpenUntil, Until);   // burnt again: open longer
			OnRep_Gaps();
			return;
		}
	}
	FWRBrakeGap& G = Gaps.AddDefaulted_GetRef();
	G.At = Mid;
	G.HalfWidth = HalfWidth;
	G.OpenUntil = Until;
	OnRep_Gaps();
	ForceNetUpdate();
	// The canes catch: a roar of flame along the lane, then embers smouldering in the char for a while.
	WRFx::Broadcast(this, TEXT("forest.thorn_burn"), Mid, FRotator::ZeroRotator, FMath::Max(1.f, HalfWidth / 200.f), TEXT("forest.thorn_burn"));
	WRFx::Broadcast(this, TEXT("forest.thorn_embers"), Mid, FRotator::ZeroRotator, FMath::Max(1.f, HalfWidth / 200.f), NAME_None, WRForestHzImpl::Tn(TEXT("thornbrake.emberSeconds"), 40.f));
	UE_LOG(LogWildreach, Log, TEXT("FOREST thornbrake burnt open at %s for %.0f s"), *Mid.ToCompactString(), Seconds);
}

void AWRThornbrake::MulticastScrape_Implementation(AActor* Who)
{
	if (GetNetMode() == NM_DedicatedServer || !Who) return;
	WRSound::PlayOn(Who, TEXT("forest.thorn_scrape"));
}

void AWRThornbrake::OnRep_Gaps()
{
	if (GetNetMode() == NM_DedicatedServer || !bLookBuilt) return;
	// Charred stubs where a lane was burnt: short black canes with ember-orange tips (the char stays while the lane is
	// open, and sinks into the regrowth).
	for (UProceduralMeshComponent* C : Char) if (C) C->DestroyComponent();
	Char.Reset();
	const FTransform T = GetActorTransform();
	for (int32 g = 0; g < Gaps.Num(); ++g)
	{
		const FWRBrakeGap& G = Gaps[g];
		if (G.OpenUntil > 1.0e8f) continue;   // the natural gap was never burnt
		FWRToolMesh M;
		int32 Stubs = 0;
		for (const FCane& C : Canes)
		{
			const FVector W = T.TransformPositionNoScale(C.Base.GetLocation());
			if (FVector::Dist2D(W, G.At) > G.HalfWidth || ++Stubs > 70) continue;
			const FVector P = C.Base.GetLocation();
			const float H = 25.f + 25.f * (WRForestHzImpl::Jit(P, 11) * 0.5f + 0.5f);
			const FVector Lean(WRForestHzImpl::Jit(P, 12) * 12.f, WRForestHzImpl::Jit(P, 13) * 12.f, 0.f);
			M.Tube({ P, P + FVector(0, 0, H * 0.5f) + Lean * 0.4f, P + FVector(0, 0, H) + Lean }, 1.8f, 5, WRForestHzImpl::CharBlack, 0.95f);
			if (WRForestHzImpl::Jit(P, 14) > 0.1f)
			{
				const FVector Tip = P + FVector(0, 0, H) + Lean;
				M.Lathe(Tip - FVector(0, 0, 3.f), FVector::UpVector, 6.f, { FVector(0.f, 1.2f, 1.2f), FVector(0.5f, 2.6f, 2.6f), FVector(1.f, 0.8f, 0.8f) }, 6, WRForestHzImpl::Ember * 2.2f, 0.6f);
			}
		}
		if (M.Verts.Num() == 0) continue;
		UProceduralMeshComponent* PM = WRForestHzImpl::NewMesh(this, RootComponent, *FString::Printf(TEXT("Char%d"), g));
		M.ApplyTo(PM);
		PM->SetCastShadow(false);
		Char.Add(PM);
	}
	LookClock = 0.f;
}

void AWRThornbrake::BuildLook()
{
	if (bLookBuilt) return;
	FVector ViewAt;
	if (!WRForestHzImpl::View(GetWorld(), ViewAt) || FVector::Dist2D(ViewAt, GetActorLocation()) > WRForestHzImpl::Tn(TEXT("thornbrake.buildRange"), 20000.f)) return;
	bLookBuilt = true;
	// Bramble canes (WRTreeKit "bramble": arching dark red-brown canes with pale thorn tips) packed through the whole
	// shape at a real bramble's density: a clump every 1.2-1.6 m, each turned and sized a little differently, standing on
	// the real ground. Four variants, one instanced part each.
	const float RefH = FMath::Max(1.f, WRTreeKit::ReferenceHeight(TEXT("bramble")));
	for (int32 v = 0; v < 4; ++v)
	{
		UStaticMesh* Mesh = WRTreeKit::Mesh(this, TEXT("bramble"), v);
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetupAttachment(RootComponent);
		if (Mesh) H->SetStaticMesh(Mesh);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCanEverAffectNavigation(false);
		const int32 Cull = int32(WRForestHzImpl::Tn(TEXT("thornbrake.cull"), 15000.f));
		H->SetCullDistances(Cull - 2000, Cull);
		H->RegisterComponent();
		Parts.Add(H);
	}
	const FVector C = GetActorLocation();
	FRandomStream R(int32(GetTypeHash(C.ToCompactString()) & 0x7fffffff));
	const float Spacing = WRForestHzImpl::Tn(TEXT("thornbrake.spacing"), 140.f);
	const int32 MaxCanes = int32(WRForestHzImpl::Tn(TEXT("thornbrake.maxCanes"), 1100.f));
	TArray<FVector> Local;
	if (bRing)
	{
		for (float Rr = Radius + Spacing * 0.4f; Rr < Radius + Depth; Rr += Spacing * 0.86f)
		{
			const int32 N = FMath::Max(6, FMath::RoundToInt(2.f * PI * Rr / Spacing));
			const float Off = R.FRand() * 2.f * PI;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = Off + 2.f * PI * i / N + R.FRandRange(-0.25f, 0.25f) * Spacing / Rr;
				const float Rj = Rr + R.FRandRange(-0.3f, 0.3f) * Spacing;
				Local.Add(FVector(FMath::Cos(A) * Rj, FMath::Sin(A) * Rj, 0.f));
			}
		}
	}
	else
	{
		for (float Y = -Depth * 0.5f + Spacing * 0.4f; Y < Depth * 0.5f; Y += Spacing * 0.86f)
		{
			const float Shift = R.FRand() * Spacing;
			for (float X = -Length * 0.5f + Shift * 0.5f; X < Length * 0.5f; X += Spacing)
			{
				Local.Add(FVector(X + R.FRandRange(-0.3f, 0.3f) * Spacing, Y + R.FRandRange(-0.3f, 0.3f) * Spacing, 0.f));
			}
		}
	}
	if (Local.Num() > MaxCanes)
	{
		// Very big brakes keep the density at their edges (what you see) and thin their middles.
		Local.Sort([](const FVector& A, const FVector& B) { return A.X * 0.37f + A.Y < B.X * 0.37f + B.Y; });
		const float Step = float(Local.Num()) / MaxCanes;
		TArray<FVector> Kept;
		for (float i = 0.f; i < Local.Num(); i += Step) Kept.Add(Local[int32(i)]);
		Local = Kept;
	}
	const FTransform T = GetActorTransform();
	for (const FVector& L : Local)
	{
		const FVector W = T.TransformPositionNoScale(L);
		// The generator's natural gap stays empty: canes only where the brake is.
		bool bGap = false;
		for (const FWRBrakeGap& G : Gaps) bGap |= G.OpenUntil > 1.0e8f && FVector::Dist2D(G.At, W) < G.HalfWidth;
		if (bGap) continue;
		FVector G;
		if (!WRForestHzImpl::Ground(GetWorld(), W, G, 800.f, 1600.f, this)) G = W;
		const float S = Height / RefH * R.FRandRange(0.7f, 1.15f);
		// A clump leans a little outward (they arch away from what they grew round), turned any way.
		const FVector Out = bRing ? FVector(L.X, L.Y, 0.f).GetSafeNormal() : FVector(0.f, FMath::Sign(L.Y), 0.f);
		const FQuat Lean = FQuat(FVector::CrossProduct(FVector::UpVector, T.TransformVectorNoScale(Out)).GetSafeNormal(), FMath::DegreesToRadians(R.FRandRange(0.f, 9.f)));
		const FQuat Yaw(FVector::UpVector, R.FRand() * 2.f * PI);
		const FVector LG = T.InverseTransformPositionNoScale(G - FVector(0, 0, 8.f));
		FCane Cn;
		Cn.Part = R.RandRange(0, 3);
		Cn.Base = FTransform(T.GetRotation().Inverse() * (Lean * Yaw), LG, FVector(S));
		Cn.Index = Parts[Cn.Part]->AddInstance(Cn.Base, false);
		Canes.Add(Cn);
	}
	LastScale.Init(1.f, Canes.Num());
	OnRep_Gaps();
}

void AWRThornbrake::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) { SetActorTickEnabled(false); return; }
	if (!bLookBuilt)
	{
		LookClock -= DeltaSeconds;
		if (LookClock > 0.f) return;
		LookClock = 1.f;
		BuildLook();
		return;
	}
	StepLook(DeltaSeconds);
}

void AWRThornbrake::StepLook(float Dt)
{
	FVector ViewAt;
	if (!WRForestHzImpl::View(GetWorld(), ViewAt)) return;
	const float Near = FVector::Dist2D(ViewAt, GetActorLocation()) - (bRing ? Radius + Depth : Length * 0.5f);
	if (Near > 6000.f && Bent.Num() == 0)
	{
		// Far away: only the burnt lanes' regrowth needs drawing, and that once a second.
		GapClock -= Dt;
		if (GapClock > 0.f) return;
		GapClock = 1.f;
	}
	const FTransform T = GetActorTransform();
	// The lanes: burnt canes stand as char (scaled down to stubs) while open, then grow back to full over 30 s.
	TSet<int32> Dirty;
	if (Gaps.Num() > 0)
	{
		for (int32 i = 0; i < Canes.Num(); ++i)
		{
			const FVector W = T.TransformPositionNoScale(Canes[i].Base.GetLocation());
			const float Open = OpenAt(W);
			const float Want = FMath::Lerp(1.f, 0.02f, Open);
			if (FMath::Abs(Want - LastScale[i]) > 0.01f) { LastScale[i] = Want; Dirty.Add(i); }
		}
	}
	// Passers (any Nature points, briarproof, Heartguard 4: the ones the server doesn't hold back) part the canes:
	// within 2.2 m of them every clump bends away and springs back once they're past.
	TMap<int32, float> Want;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || C->HasTag(WRForestTags::Hazard_Thornbrake)) continue;
		const FVector F = WRForestHzImpl::Feet(C);
		if (!Contains(F, 220.f)) continue;
		const FVector L = T.InverseTransformPositionNoScale(F);
		for (int32 i = 0; i < Canes.Num(); ++i)
		{
			const float D = FVector::Dist2D(Canes[i].Base.GetLocation(), L);
			if (D < 220.f) Want.FindOrAdd(i) = FMath::Max(Want.FindRef(i), 1.f - D / 220.f);
		}
	}
	for (const TPair<int32, float>& W : Want) if (!Bent.Contains(W.Key)) Bent.Add(W.Key, 0.f);
	TArray<int32> Done;
	for (TPair<int32, float>& B : Bent)
	{
		const float Goal = Want.FindRef(B.Key);
		const float Was = B.Value;
		B.Value = FMath::FInterpTo(B.Value, Goal, Dt, Goal > B.Value ? 10.f : 3.f);
		if (FMath::Abs(B.Value - Was) > 0.002f) Dirty.Add(B.Key);
		if (Goal <= 0.f && B.Value < 0.01f) Done.Add(B.Key);
	}
	for (const int32 i : Done) { Bent.Remove(i); Dirty.Add(i); }
	if (Dirty.Num() == 0) return;
	// Which way each bending clump goes: away from the nearest passer.
	for (const int32 i : Dirty)
	{
		const FCane& Cn = Canes[i];
		FTransform X = Cn.Base;
		X.SetScale3D(Cn.Base.GetScale3D() * FVector(1.f, 1.f, LastScale.IsValidIndex(i) ? LastScale[i] : 1.f));
		if (const float* Amount = Bent.Find(i))
		{
			FVector Away = FVector::ZeroVector;
			float BestD = 1e9f;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
			{
				const FVector L = T.InverseTransformPositionNoScale(WRForestHzImpl::Feet(*It));
				const float D = FVector::Dist2D(L, Cn.Base.GetLocation());
				if (D < BestD) { BestD = D; Away = (Cn.Base.GetLocation() - L).GetSafeNormal2D(); }
			}
			if (!Away.IsNearlyZero())
			{
				const FQuat Bend(FVector::CrossProduct(FVector::UpVector, Away).GetSafeNormal(), FMath::DegreesToRadians(WRForestHzImpl::Tn(TEXT("thornbrake.partDegrees"), 48.f) * *Amount));
				X.SetRotation(Bend * Cn.Base.GetRotation());
			}
		}
		Parts[Cn.Part]->UpdateInstanceTransform(Cn.Index, X, false, false, true);
	}
	for (UHierarchicalInstancedStaticMeshComponent* P : Parts) if (P) P->MarkRenderStateDirty();
}

// ================================================================================================ AWRWidowmaker

namespace WRForestHzImpl
{
	/** A collision sphere that arrows and spells stop on (projectiles are WorldDynamic, BlockAllDynamic) and melee
	 *  sweeps (the Pawn channel) touch, without blocking anyone's walk. */
	void MakeHitbox(USphereComponent* S, float Radius)
	{
		S->InitSphereRadius(Radius);
		S->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		S->SetCollisionObjectType(ECC_WorldDynamic);
		S->SetCollisionResponseToAllChannels(ECR_Ignore);
		S->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		S->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		S->SetGenerateOverlapEvents(false);
		S->SetCanEverAffectNavigation(false);
	}
}

AWRWidowmaker::AWRWidowmaker()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Limb = CreateDefaultSubobject<USceneComponent>(TEXT("Limb"));
	Limb->SetupAttachment(RootComponent);
	Hitbox = CreateDefaultSubobject<USphereComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Limb);
	WRForestHzImpl::MakeHitbox(Hitbox, 110.f);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
}

void AWRWidowmaker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWidowmaker, GroundZ);
	DOREPLIFETIME(AWRWidowmaker, FallRadius);
	DOREPLIFETIME(AWRWidowmaker, TellAt);
}

void AWRWidowmaker::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	OnRep_Tell();
}

void AWRWidowmaker::BuildLook()
{
	if (LimbMesh) return;
	// A dead grey limb, bark half gone, wedged across a fork: a tapering bough 4-6 m long lying a little off level, its
	// broken side branches as stubs, strips of loose bark hanging, a scab of lichen. Seeded by its place.
	const FVector P = GetActorLocation();
	const float Len = 420.f + 200.f * (WRForestHzImpl::Jit(P, 1) * 0.5f + 0.5f);
	const FVector Dir = FRotator(-14.f + 8.f * WRForestHzImpl::Jit(P, 2), 180.f * WRForestHzImpl::Jit(P, 3), 0.f).Vector();
	const FVector Start = -Dir * Len * 0.5f;
	FWRToolMesh M;
	M.Lathe(Start, Dir, Len, { FVector(0.f, 21.f, 18.f), FVector(0.12f, 20.f, 17.f), FVector(0.45f, 15.f, 13.f), FVector(0.8f, 9.f, 8.f), FVector(0.97f, 5.f, 4.f), FVector(1.f, 2.f, 2.f) },
		9, WRForestHzImpl::DeadWood, 0.92f, 8.f, 3.7f);
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	for (int32 i = 0; i < 4; ++i)
	{
		const float T = 0.2f + 0.18f * i;
		const FVector From = Start + Dir * (Len * T);
		const FVector StubDir = (Dir * 0.5f + Side * (i % 2 ? 1.f : -1.f) + FVector(0, 0, 0.35f * WRForestHzImpl::Jit(P, 10 + i))).GetSafeNormal();
		M.Lathe(From, StubDir, 50.f + 60.f * (WRForestHzImpl::Jit(P, 20 + i) * 0.5f + 0.5f), { FVector(0.f, 7.f, 6.f), FVector(0.8f, 4.f, 3.5f), FVector(1.f, 1.5f, 1.5f) }, 6, WRForestHzImpl::DeadWoodDark, 0.95f);
	}
	for (int32 i = 0; i < 5; ++i)
	{
		// Loose bark curling off the underside: the shower that comes down when it cracks.
		const FVector Under = Start + Dir * (Len * (0.15f + 0.15f * i)) - FVector(0, 0, 15.f);
		M.Tube({ Under, Under + FVector(3.f, 2.f, -14.f), Under + FVector(6.f, -2.f, -30.f - 10.f * (i % 3)) }, 1.6f, 4, WRForestHzImpl::Bark, 0.95f);
	}
	M.Lathe(Start + Dir * (Len * 0.3f) + FVector(0, 0, 12.f), FVector::UpVector, 6.f, { FVector(0.f, 16.f, 10.f), FVector(1.f, 12.f, 7.f) }, 7, WRForestHzImpl::Moss * 0.7f, 0.98f);
	LimbMesh = WRForestHzImpl::NewMesh(this, Limb, TEXT("LimbMesh"));
	M.ApplyTo(LimbMesh);
}

FTransform AWRWidowmaker::LimbAt(float SinceFall) const
{
	// A real fall: from rest under gravity, its heavy butt end leading so it turns toward level as it drops, then it
	// lies where it hit, a little rolled.
	const float G = 980.f;
	const float H = FMath::Max(50.f, GetActorLocation().Z - GroundZ - 25.f);
	const float TLand = FMath::Sqrt(2.f * H / G);
	if (SinceFall <= 0.f) return FTransform::Identity;
	const float S = FMath::Min(SinceFall, TLand);
	const float Drop = 0.5f * G * S * S;
	const float A = S / TLand;
	const float Pitch = 16.f * A * A;
	const float Roll = 25.f * A * (WRForestHzImpl::Jit(GetActorLocation(), 31) > 0.f ? 1.f : -1.f);
	return FTransform(FRotator(Pitch, 0.f, Roll), FVector(0.f, 0.f, -Drop));
}

void AWRWidowmaker::Drop(AActor* By)
{
	if (!HasAuthority() || !IsArmed()) return;
	TellAt = WRForestHzImpl::Now(this);
	DroppedBy = By;
	bLanded = false;
	ForceNetUpdate();
	OnRep_Tell();
	UE_LOG(LogWildreach, Log, TEXT("FOREST widowmaker at %s cracks (%s)"), *GetActorLocation().ToCompactString(), By ? *By->GetName() : TEXT("under it / a roar"));
}

void AWRWidowmaker::TakeShot(float Damage, AActor* By)
{
	if (!HasAuthority() || !IsArmed()) return;
	ShotDamage += Damage;
	WRFx::Broadcast(this, TEXT("forest.widow_bark"), GetActorLocation(), FRotator::ZeroRotator, 0.4f, TEXT("gather.wood"));
	if (ShotDamage >= WRForestHzImpl::Tn(TEXT("widowmaker.shootDown"), 30.f)) Drop(By);
}

void AWRWidowmaker::OnRep_Tell()
{
	if (TellAt < 0.f)
	{
		// Hanging again (a new one wedged in the crown since): back up, shootable.
		bToldHere = false;
		bImpactShown = false;
		if (Limb) Limb->SetRelativeTransform(FTransform::Identity);
		if (Hitbox) Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}
	if (GetNetMode() == NM_DedicatedServer || bToldHere) return;
	bToldHere = true;
	const float Since = WRForestHzImpl::Now(this) - TellAt;
	if (Since > WRForestHzImpl::Tn(TEXT("widowmaker.tell"), 1.0f) + 3.f) { bImpactShown = true; return; }   // a late joiner: it already fell
	// The tell (1.0 s, cooks/biome-forest.md 2.8): a crack like a rifle shot, bark showering, a shadow growing under it.
	WRSound::PlayAt(this, TEXT("forest.widow_crack"), GetActorLocation());
	WRFx::SpawnAt(this, TEXT("forest.widow_bark"), GetActorLocation(), FRotator::ZeroRotator, 1.f);
	WRFx::SpawnAt(this, TEXT("forest.widow_shadow"), FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ + 4.f), FRotator::ZeroRotator, FallRadius / 200.f);
}

void AWRWidowmaker::Land()
{
	bLanded = true;
	const FVector Impact(GetActorLocation().X, GetActorLocation().Y, GroundZ);
	// Everything under it, player or creature: 70 and a knockdown (2 m radius). A shot that brought it down on a
	// creature counts as that player's blow.
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* V = *It;
		if (!V->IsAlive()) continue;
		const FVector F = WRForestHzImpl::Feet(V);
		if (FVector::Dist2D(F, Impact) > FallRadius + V->GetSimpleCollisionRadius() || FMath::Abs(F.Z - GroundZ) > 300.f) continue;
		FWRHit H;
		H.Instigator = DroppedBy.Get();
		H.Damage = WRForestHzImpl::Tn(TEXT("widowmaker.damage"), 70.f);
		H.Origin = Impact + FVector(0, 0, 400.f);
		H.ImpactPoint = V->GetActorLocation() + FVector(0, 0, V->GetSimpleCollisionHalfHeight() * 0.8f);
		H.bUnblockable = true;
		H.Impact = 3;
		H.PoiseDamage = 999.f;
		H.Tier = EWRHitTier::Heavy;
		H.PhysType = 2;
		UWRCombatLibrary::ApplyHit(V, H);
	}
	ReArmAt = WRForestHzImpl::Hours(this) + WRForestHzImpl::Tn(TEXT("widowmaker.rearmHours"), 24.f);
	if (Hitbox) Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWRWidowmaker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = WRForestHzImpl::Now(this);
	const float Tell = WRForestHzImpl::Tn(TEXT("widowmaker.tell"), 1.0f);
	// The limb, the same on every machine: still while armed, shuddering through the tell, then falling.
	if (TellAt >= 0.f && Limb)
	{
		const float Since = T - TellAt;
		if (Since < Tell)
		{
			const float Shudder = 1.5f * FMath::Sin(Since * 70.f) * (Since / Tell);
			Limb->SetRelativeTransform(FTransform(FRotator(Shudder, 0.f, Shudder * 0.5f), FVector(0, 0, -6.f * Since / Tell)));
		}
		else
		{
			Limb->SetRelativeTransform(LimbAt(Since - Tell));
			const float H = FMath::Max(50.f, GetActorLocation().Z - GroundZ - 25.f);
			if (!bImpactShown && Since - Tell >= FMath::Sqrt(2.f * H / 980.f) && GetNetMode() != NM_DedicatedServer)
			{
				bImpactShown = true;
				const FVector Impact(GetActorLocation().X, GetActorLocation().Y, GroundZ);
				WRSound::PlayAt(this, TEXT("forest.widow_fall"), Impact);
				WRFx::SpawnAt(this, TEXT("forest.widow_impact"), Impact, FRotator::ZeroRotator, FallRadius / 200.f);
				WRFx::Shake(this, 0.35f, Impact, 2500.f);
			}
		}
	}
	if (!HasAuthority()) return;
	if (TellAt >= 0.f)
	{
		const float H = FMath::Max(50.f, GetActorLocation().Z - GroundZ - 25.f);
		if (!bLanded && T - TellAt >= Tell + FMath::Sqrt(2.f * H / 980.f)) Land();
		if (bLanded && ReArmAt > 0.0 && WRForestHzImpl::Hours(this) >= ReArmAt)
		{
			// A game day on, another dead limb hangs there (the crown sheds them all the time).
			TellAt = -1.f;
			ReArmAt = -1.0;
			ShotDamage = 0.f;
			bLanded = false;
			DroppedBy = nullptr;
			ForceNetUpdate();
			OnRep_Tell();
		}
		return;
	}
	CheckClock -= DeltaSeconds;
	if (CheckClock > 0.f) return;
	CheckClock = 0.2f;
	// Someone under it (its 2 m footprint) running or fighting: the vibration through the roots is enough.
	const FVector Me = GetActorLocation();
	const float Run = WRForestHzImpl::Tn(TEXT("widowmaker.runSpeed"), 520.f);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive()) continue;
		const FVector F = WRForestHzImpl::Feet(C);
		if (FVector::Dist2D(F, Me) > FallRadius || F.Z > Me.Z || F.Z < GroundZ - 300.f) continue;
		const bool bFighting = C->HasTag(WRTags::State_Attacking) || C->HasTag(WRTags::State_Windup) || C->HasTag(WRTags::State_Blocking) || C->HasTag(WRTags::State_Casting);
		if (WRForestHzImpl::Speed2D(C) >= Run || bFighting)
		{
			Drop(nullptr);
			return;
		}
	}
}

// ================================================================================================ AWRLeafPit

AWRLeafPit::AWRLeafPit()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// The litter holds a walker until it gives way: a thin floor over the hole (the hole itself is carved in the ground).
	Floor = CreateDefaultSubobject<UBoxComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootComponent);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetCanEverAffectNavigation(false);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
}

void AWRLeafPit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRLeafPit, Radius);
	DOREPLIFETIME(AWRLeafPit, Depth);
	DOREPLIFETIME(AWRLeafPit, CollapsedAt);
}

void AWRLeafPit::BeginPlay()
{
	Super::BeginPlay();
	Floor->SetBoxExtent(FVector(Radius * 0.72f, Radius * 0.72f, 8.f));
	Floor->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	OnRep_Collapsed();
}

void AWRLeafPit::BuildLook()
{
	if (Mat) return;
	// The mat: leaf litter laid over the hole and out past its rim onto the real ground, sagging in a ring just inside
	// the rim (the tell) and a little in the middle, mottled browns with a darker band where the sag is. A radial grid.
	const int32 Rings = 8, Segs = 28;
	const float Out = Radius * 1.18f;
	const float Sag = WRForestHzImpl::Tn(TEXT("leafPit.sag"), 14.f);
	TArray<FVector> V; TArray<int32> Tr; TArray<FVector> N; TArray<FVector2D> UV; TArray<FColor> Col;
	const FVector P = GetActorLocation();
	V.Add(FVector(0, 0, -Sag * 0.6f)); N.Add(FVector::UpVector); UV.Add(FVector2D(0.5f, 0.5f)); Col.Add(WRForestHzImpl::Pack(WRForestHzImpl::Litter, 0.95f));
	for (int32 r = 1; r <= Rings; ++r)
	{
		const float Rr = Out * r / Rings;
		const float F = Rr / Radius;
		const float Z = -Sag * FMath::Exp(-FMath::Square((F - 0.86f) / 0.12f)) - Sag * 0.6f * FMath::Max(0.f, 1.f - F * F) + (F > 1.f ? 6.f * (F - 1.f) : 0.f);
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A = 2.f * PI * s / Segs;
			const FVector At(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Z + 2.f * WRForestHzImpl::Jit(P + FVector(r, s, 0), 5));
			V.Add(At);
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(0.5f + At.X / (2.f * Out), 0.5f + At.Y / (2.f * Out)));
			const float Band = FMath::Exp(-FMath::Square((F - 0.86f) / 0.1f));
			const FLinearColor C = FMath::Lerp(FMath::Lerp(WRForestHzImpl::Litter, WRForestHzImpl::LitterDark, 0.5f + 0.5f * WRForestHzImpl::Jit(At + P, 6)), WRForestHzImpl::LitterDark * 0.7f, Band * 0.7f);
			Col.Add(WRForestHzImpl::Pack(C, 0.95f));
		}
	}
	for (int32 s = 0; s < Segs; ++s) Tr.Append({ 0, 1 + (s + 1) % Segs, 1 + s });
	for (int32 r = 1; r < Rings; ++r)
	{
		for (int32 s = 0; s < Segs; ++s)
		{
			const int32 A = 1 + (r - 1) * Segs + s, B = 1 + (r - 1) * Segs + (s + 1) % Segs;
			const int32 C = A + Segs, D = B + Segs;
			Tr.Append({ A, B, C, B, D, C });
		}
	}
	Mat = WRForestHzImpl::NewMesh(this, RootComponent, TEXT("LitterMat"));
	WRForestHzImpl::Section(Mat, 0, V, Tr, N, UV, Col, FWRToolMesh::Material(), true);
	Mat->SetCastShadow(false);
}

void AWRLeafPit::Reveal(float Seconds)
{
	if (!HasAuthority()) return;
	// The rim shows: the litter at the edge stirred up, dark soil and pale root ends round the hole.
	WRFx::Broadcast(this, TEXT("forest.leafpit_rim"), GetActorLocation() + FVector(0, 0, 4.f), FRotator::ZeroRotator, Radius / 250.f, NAME_None, Seconds);
}

void AWRLeafPit::MulticastDrum_Implementation(FVector_NetQuantize At)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	WRSound::PlayAt(this, TEXT("forest.leafpit_drum"), At);
}

void AWRLeafPit::OnRep_Collapsed()
{
	const bool bWhole = CollapsedAt < 0.f;
	Floor->SetCollisionEnabled(bWhole ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (!Mat) return;
	if (bWhole)
	{
		Mat->SetVisibility(true);
		Mat->SetRelativeTransform(FTransform::Identity);
		return;
	}
	const float Since = WRForestHzImpl::Now(this) - CollapsedAt;
	if (Since < 3.f)
	{
		WRSound::PlayAt(this, TEXT("forest.leafpit_fall"), GetActorLocation());
		WRFx::SpawnAt(this, TEXT("forest.leafpit_collapse"), GetActorLocation(), FRotator::ZeroRotator, Radius / 250.f);
	}
}

void AWRLeafPit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Mat && CollapsedAt >= 0.f)
	{
		// The mat goes down into the hole under gravity, folding in on itself, and is gone in the dark below.
		const float S = WRForestHzImpl::Now(this) - CollapsedAt;
		const float Drop = FMath::Min(Depth, 0.5f * 980.f * S * S);
		const float K = FMath::Clamp(1.f - S * 0.45f, 0.35f, 1.f);
		Mat->SetRelativeTransform(FTransform(FRotator(6.f * S, 0.f, -4.f * S), FVector(0, 0, -Drop), FVector(K, K, 1.f)));
		Mat->SetVisibility(Drop < Depth - 10.f);
	}
	if (!HasAuthority()) return;
	CheckClock -= DeltaSeconds;
	if (CheckClock > 0.f) return;
	CheckClock = 0.1f;
	const FVector C = GetActorLocation();
	const double Now = WRForestHzImpl::Now(this);
	if (CollapsedAt >= 0.f)
	{
		// Re-forms after a game hour (leaves drift in over the roots again) once nobody is down the hole.
		if (ReformAt > 0.0 && WRForestHzImpl::Hours(this) >= ReformAt)
		{
			bool bSomeone = false;
			for (TActorIterator<AWRCharacterBase> It(GetWorld()); It && !bSomeone; ++It) bSomeone = FVector::Dist2D(It->GetActorLocation(), C) < Radius + 60.f && It->GetActorLocation().Z < C.Z + 200.f;
			if (!bSomeone) { CollapsedAt = -1.f; ReformAt = -1.0; ForceNetUpdate(); OnRep_Collapsed(); }
		}
		return;
	}
	bool bOn = false;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* P = *It;
		// Players only: every forest creature knows the pits (creatures.json resists leaf_pit).
		if (!P->IsAlive() || !WRForestHzImpl::IsPlayer(P)) continue;
		const FVector F = WRForestHzImpl::Feet(P);
		if (FMath::Abs(F.Z - C.Z) > 120.f) continue;
		const float D = FVector::Dist2D(F, C) / FMath::Max(1.f, Radius);
		if (D < 0.72f) bOn = true;
		else if (D < 1.25f)
		{
			// The edge step: a hollow drum under the foot.
			double& Last = DrumAt.FindOrAdd(P, -100.0);
			if (Now - Last > 2.5 && WRForestHzImpl::Speed2D(P) > 50.f) { Last = Now; MulticastDrum(F); }
		}
	}
	if (!bOn) { StepOnAt = -1.0; return; }
	if (StepOnAt < 0.0) StepOnAt = Now;
	if (Now - StepOnAt < WRForestHzImpl::Tn(TEXT("leafPit.giveSeconds"), 0.25f)) return;
	CollapsedAt = float(Now);
	ReformAt = WRForestHzImpl::Hours(this) + WRForestHzImpl::Tn(TEXT("leafPit.reformHours"), 1.f);
	StepOnAt = -1.0;
	ForceNetUpdate();
	OnRep_Collapsed();
	UE_LOG(LogWildreach, Log, TEXT("FOREST leaf pit at %s gave way (%.0f m deep)"), *C.ToCompactString(), Depth / 100.f);
}

// ================================================================================================ AWRWaspNest

AWRWaspNest::AWRWaspNest()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Hitbox = CreateDefaultSubobject<USphereComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(RootComponent);
	WRForestHzImpl::MakeHitbox(Hitbox, 45.f);
	Hitbox->SetRelativeLocation(FVector(0, 0, -40.f));
	SetNetCullDistanceSquared(FMath::Square(20000.f));
}

void AWRWaspNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRWaspNest, bOverhang);
	DOREPLIFETIME(AWRWaspNest, BranchFrom);
	DOREPLIFETIME(AWRWaspNest, CalmUntil);
	DOREPLIFETIME(AWRWaspNest, SwarmedAt);
}

void AWRWaspNest::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	BuildLook();
	// The rising hum within 15 m (its radius is the sound table's): the tell that you're near one.
	Hum = WRSound::StartLoop(this, TEXT("forest.wasp_hum"));
}

void AWRWaspNest::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Hum) Hum->Stop();
	if (HasAuthority()) for (const TWeakObjectPtr<AWRCreature>& W : Wasps) if (AWRCreature* C = W.Get()) C->Destroy();
	Super::EndPlay(Reason);
}

void AWRWaspNest::BuildLook()
{
	if (Body) return;
	// A Bramblewasp nest: chewed-wood paper in scalloped grey bands round a teardrop 70 cm tall, the entrance a dark
	// hole low on one side, hung by a paper stalk from a branch (or glued under a rock overhang by a broad paper cap).
	const FVector P = GetActorLocation();
	FWRToolMesh M;
	const float H = 70.f, R = 26.f;
	TArray<TArray<FVector>> Rings;
	const int32 Around = 12;
	const int32 Bands = 9;
	for (int32 b = 0; b <= Bands; ++b)
	{
		const float T = float(b) / Bands;
		// The teardrop: narrow at the stalk, widest two-thirds down, rounding to a blunt bottom.
		const float Rad = R * FMath::Sin(PI * FMath::Pow(T, 0.75f)) * (b == Bands ? 0.3f : 1.f) + (b == 0 ? 4.f : 0.f);
		TArray<FVector> Ring;
		for (int32 s = 0; s < Around; ++s)
		{
			const float A = 2.f * PI * s / Around;
			const float Scallop = 1.f + 0.06f * FMath::Sin(A * 3.f + b);
			Ring.Add(FVector(FMath::Cos(A) * Rad * Scallop, FMath::Sin(A) * Rad * Scallop, -H * T));
		}
		Rings.Add(Ring);
	}
	for (int32 b = 0; b < Bands; ++b)
	{
		M.Loft({ Rings[b], Rings[b + 1] }, b % 2 ? WRForestHzImpl::PaperBand : WRForestHzImpl::Paper, 0.9f, b == 0 || b == Bands - 1);
	}
	// The entrance: a dark mouth low on the side facing out.
	M.Lathe(FVector(R * 0.78f, 0.f, -H * 0.7f), FVector(1.f, 0.f, -0.2f).GetSafeNormal(), 5.f, { FVector(0.f, 7.f, 7.f), FVector(1.f, 5.f, 5.f) }, 10, WRForestHzImpl::CharBlack, 0.9f);
	if (bOverhang)
	{
		// The cap glued to the rock, papery and flat.
		M.Lathe(FVector(0, 0, -2.f), FVector::UpVector, 6.f, { FVector(0.f, 34.f, 30.f), FVector(1.f, 40.f, 36.f) }, 12, WRForestHzImpl::Paper * 0.9f, 0.9f);
	}
	else
	{
		// The paper stalk up to the bough it hangs from. The bough is rooted where the server found something to grow
		// from: out of a trunk beside it (a live side branch, tapering past the nest), or, with nothing near, a dead
		// snag standing in the ground with a crook arm over the nest.
		M.Lathe(FVector(0, 0, -2.f), FVector::UpVector, 22.f, { FVector(0.f, 3.f, 3.f), FVector(1.f, 2.f, 2.f) }, 6, WRForestHzImpl::Paper * 0.8f, 0.9f);
		const FVector Hook(0.f, 0.f, 26.f);
		const FVector Root = FVector(BranchFrom) - P;
		if (Root.Z < -150.f)
		{
			// The snag: a grey stem from the ground to just above the nest, its broken crook reaching over it.
			const FVector Foot(Root.X, Root.Y, Root.Z - 30.f);
			const FVector Top = Hook + FVector(Root.X, Root.Y, 0.f) * 0.95f + FVector(0, 0, 40.f);
			M.Lathe(Foot, (Top - Foot).GetSafeNormal(), (Top - Foot).Size(), { FVector(0.f, 13.f, 12.f), FVector(0.6f, 9.f, 8.5f), FVector(1.f, 6.f, 6.f) }, 8, WRForestHzImpl::DeadWood, 0.92f, 6.f, 1.3f);
			M.Lathe(Top, (Hook - Top).GetSafeNormal() + FVector(0, 0, 0.1f), FVector::Dist(Top, Hook) + 30.f, { FVector(0.f, 6.f, 6.f), FVector(1.f, 2.5f, 2.5f) }, 6, WRForestHzImpl::DeadWood, 0.92f);
		}
		else
		{
			const FVector From = Root.IsNearlyZero() ? Hook - FRotator(0.f, 180.f * WRForestHzImpl::Jit(P, 4), 0.f).Vector() * 160.f : Root;
			const FVector Along = (Hook - From).GetSafeNormal();
			const float Len = FVector::Dist(Hook, From) + 140.f;
			M.Lathe(From, Along, Len, { FVector(0.f, 11.f, 11.f), FVector(0.5f, 7.f, 7.f), FVector(1.f, 2.f, 2.f) }, 8, WRForestHzImpl::Bark, 0.9f, 10.f, 2.1f);
			M.Lathe(From + Along * (Len * 0.55f) + FVector(0, 0, 6.f), FVector::UpVector, 4.f, { FVector(0.f, 9.f, 6.f), FVector(1.f, 6.f, 4.f) }, 6, WRForestHzImpl::Moss * 0.8f, 0.98f);
		}
	}
	Body = WRForestHzImpl::NewMesh(this, RootComponent, TEXT("Nest"));
	M.ApplyTo(Body);
}

bool AWRWaspNest::IsCalm() const
{
	return WRForestHzImpl::Now(this) < CalmUntil;
}

bool AWRWaspNest::IsAsleep() const
{
	// Asleep at night (a hit wakes it: Swarm doesn't ask), unless it was stirred up just now.
	return AWRGameState::IsNightIn(this) && WRForestHzImpl::Now(this) - SwarmedAt > 60.f;
}

void AWRWaspNest::Swarm(AActor* Who)
{
	if (!HasAuthority() || !Who) return;
	const float T = WRForestHzImpl::Now(this);
	// A scout marks whoever stirred it (track E: every wasp within 40 m goes for the mark).
	WRForest::MarkForWasps(Who, WRForestHzImpl::Tn(TEXT("waspNest.markSeconds"), 30.f));
	for (int32 i = Wasps.Num() - 1; i >= 0; --i) if (!Wasps[i].IsValid() || !Wasps[i]->IsAlive()) Wasps.RemoveAt(i);
	if (T - SwarmedAt > 8.f || Wasps.Num() < 3)
	{
		const int32 Min = int32(WRForestHzImpl::Tn(TEXT("waspNest.swarmMin"), 6.f));
		const int32 Max = int32(WRForestHzImpl::Tn(TEXT("waspNest.swarmMax"), 10.f));
		const int32 Want = FMath::RandRange(Min, FMath::Max(Min, Max)) - Wasps.Num();
		const FName Id(*FWRTuning::Get().Str(TEXT("hazards.waspNest.creature"), TEXT("bramblewasp")));
		for (int32 i = 0; i < Want; ++i)
		{
			const FVector At = GetActorLocation() + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -80.f - FMath::FRandRange(0.f, 60.f));
			AWRCreature* W = AWRCreature::SpawnCreature(GetWorld(), Id, At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
			if (!W)
			{
				UE_LOG(LogWildreach, Warning, TEXT("FOREST wasp nest: no '%s' creature row to swarm with"), *Id.ToString());
				break;
			}
			W->SetAggroTarget(Who);
			Wasps.Add(W);
		}
	}
	else
	{
		for (const TWeakObjectPtr<AWRCreature>& W : Wasps) if (AWRCreature* C = W.Get()) C->SetAggroTarget(Who);
	}
	SwarmedAt = T;
	Near.Reset();
	ForceNetUpdate();
	WRFx::Broadcast(this, TEXT("forest.wasp_burst"), GetActorLocation() - FVector(0, 0, 50.f), FRotator::ZeroRotator, 1.f, TEXT("forest.wasp_swarm"));
	UE_LOG(LogWildreach, Log, TEXT("FOREST wasp nest at %s swarms %s (%d wasps)"), *GetActorLocation().ToCompactString(), *Who->GetName(), Wasps.Num());
}

FString AWRWaspNest::StealLarva(AWRPlayerController* PC)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !HasAuthority()) return FString();
	if (!IsCalm()) return TEXT("The nest is awake: smoke it first (a torch held at it, or a smoke pot)");
	const AWRGameState* GS = AWRGameState::Get(this);
	const int32 Day = GS ? GS->GetDay() : 0;
	int32& Last = RobbedDay.FindOrAdd(PS->GetPlayerName(), -1);
	if (Last == Day) return TEXT("You've had this nest's larva today");
	const FName Item(*FWRTuning::Get().Str(TEXT("hazards.waspNest.larvaItem"), TEXT("egg_bramblewasp")));
	if (PS->GetInventory()->AddItem(Item, 1) > 0) return TEXT("Inventory full");
	Last = Day;
	WRFx::Broadcast(this, TEXT("forest.wasp_paper"), GetActorLocation() - FVector(0, 0, 40.f), FRotator::ZeroRotator, 0.6f, TEXT("pickup"));
	return TEXT("You ease a fat larva out of the comb: hatch it to raise a Bramblewasp");
}

void AWRWaspNest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = WRForestHzImpl::Now(this);
	if (Hum)
	{
		// Loud and angry just after a swarm, low and slow asleep or smoked.
		const bool bAngry = T - SwarmedAt < 20.f;
		Hum->SetVolumeMultiplier(bAngry ? 1.6f : IsCalm() ? 0.35f : IsAsleep() ? 0.25f : 1.f);
		Hum->SetPitchMultiplier(bAngry ? 1.15f : IsCalm() || IsAsleep() ? 0.85f : 1.f);
	}
	if (!HasAuthority()) return;
	const FVector Me = GetActorLocation();
	if (WRForest::InSmoke(this, Me)) CalmUntil = FMath::Max(CalmUntil, T + 1.f);
	// The swarm goes home after a while (the mark lasts, the wasps lose interest).
	if (Wasps.Num() > 0 && T - SwarmedAt > WRForestHzImpl::Tn(TEXT("waspNest.swarmSeconds"), 150.f))
	{
		for (const TWeakObjectPtr<AWRCreature>& W : Wasps)
		{
			if (AWRCreature* C = W.Get(); C && C->IsAlive() && !C->IsTamed())
			{
				WRFx::Broadcast(C, TEXT("forest.wasp_return"), C->GetActorLocation(), FRotator::ZeroRotator, 0.5f);
				C->Destroy();
			}
		}
		Wasps.Reset();
	}
	const float Close = WRForestHzImpl::Tn(TEXT("waspNest.proximityRadius"), 400.f);
	const float Need = WRForestHzImpl::Tn(TEXT("waspNest.proximitySeconds"), 5.f);
	TSet<TWeakObjectPtr<AActor>> Seen;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* P = *It;
		if (!P->IsAlive() || !WRForestHzImpl::IsPlayer(P)) continue;
		const float D = FVector::Dist(P->GetActorLocation(), Me);
		// A torch held at it for 3 s smokes it (calm 60 s).
		if (D < 350.f && UWRSurvivalComponent::HoldsTorch(P))
		{
			float& Held = Torch.FindOrAdd(P, 0.f);
			Held += DeltaSeconds;
			if (Held >= WRForestHzImpl::Tn(TEXT("waspNest.torchSeconds"), 3.f))
			{
				Held = 0.f;
				const float Calm = WRForestHzImpl::Tn(TEXT("waspNest.smokeSeconds"), 60.f);
				WRForest::AddSmoke(GetWorld(), Me, WRForestHzImpl::Tn(TEXT("waspNest.smokeRadius"), 400.f), Calm);
				CalmUntil = T + Calm;
				ForceNetUpdate();
			}
		}
		else Torch.Remove(P);
		if (D > Close || IsCalm() || IsAsleep()) continue;
		Seen.Add(P);
		float& Stood = Near.FindOrAdd(P, 0.f);
		Stood += DeltaSeconds;
		if (Stood >= Need) { Swarm(P); return; }
	}
	for (auto It = Near.CreateIterator(); It; ++It) if (!Seen.Contains(It->Key)) It.RemoveCurrent();
}

// ================================================================================================ the rivers

AWRForestWaterVolume::AWRForestWaterVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// As the sea's (AWRWaterVolume): pawns only, so CharacterMovement swims while the capsule's middle is in it;
	// traces, the camera, building and projectiles never see it.
	bWaterVolume = true;
	bReplicates = false;
	SetHidden(true);
	UBrushComponent* Shape = GetBrushComponent();
	Shape->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetCollisionObjectType(ECC_WorldDynamic);
	Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Shape->SetGenerateOverlapEvents(true);
	Shape->SetCanEverAffectNavigation(false);
}

int32 AWRForestWaterVolume::Build(const TArray<FBox2>& Boxes)
{
	FluidFriction = WRTune(TEXT("water.friction"), 2.f);
	UBrushComponent* Shape = GetBrushComponent();
	UBodySetup* Setup = NewObject<UBodySetup>(Shape);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	for (const FBox2& B : Boxes)
	{
		FKBoxElem Box(B.Half.X * 2.f, B.Half.Y * 2.f, B.Half.Z * 2.f);
		Box.Center = B.Centre;
		Box.Rotation = FRotator(0.f, B.Yaw, 0.f);
		Setup->AggGeom.BoxElems.Add(Box);
	}
	Setup->CreatePhysicsMeshes();
	Shape->BrushBodySetup = Setup;
	Shape->RecreatePhysicsState();
	Shape->UpdateBounds();
	return Setup->AggGeom.BoxElems.Num();
}

AWRForestWater::AWRForestWater()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestWater::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Volume) Volume->Destroy();
	Super::EndPlay(Reason);
}

void AWRForestWater::Build()
{
	if (Points.Num() < 2) return;
	Along.Reset();
	float Sum = 0.f;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		if (i > 0) Sum += FVector::Dist2D(Points[i], Points[i - 1]);
		Along.Add(Sum);
	}
	const auto WidthAt = [this](int32 i) { return Widths.IsValidIndex(i) ? Widths[i] : 1200.f; };
	// The water you can swim in: a box per stretch, its top the surface, as deep as the channel and a little more.
	const float VolDepth = WRForestHzImpl::Tn(TEXT("water.volumeDepth"), 600.f);
	TArray<AWRForestWaterVolume::FBox2> Boxes;
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const FVector A = Points[i], B = Points[i + 1];
		const float Len = FVector::Dist2D(A, B);
		if (Len < 1.f) continue;
		AWRForestWaterVolume::FBox2 Box;
		const float Top = FMath::Min(A.Z, B.Z);
		Box.Centre = FVector((A.X + B.X) * 0.5f, (A.Y + B.Y) * 0.5f, Top - VolDepth * 0.5f);
		Box.Half = FVector(Len * 0.5f + FMath::Min(WidthAt(i), WidthAt(i + 1)) * 0.15f, FMath::Min(WidthAt(i), WidthAt(i + 1)) * 0.5f, VolDepth * 0.5f);
		Box.Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		Boxes.Add(Box);
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Volume = GetWorld()->SpawnActor<AWRForestWaterVolume>(AWRForestWaterVolume::StaticClass(), FTransform::Identity, SP);
	if (Volume) Volume->Build(Boxes);

	// Where the current shoves in spate (cooks/biome-forest.md 2.8): every road or elk trail that fords it, the stream
	// fords of the kit, and the lip of any fall on it (the Great Falls take you over).
	const FWRForestData& D = FWRForestData::Get();
	const auto DownAt = [this](int32 Seg) { return (Points[FMath::Min(Seg + 1, Points.Num() - 1)] - Points[Seg]).GetSafeNormal2D(); };
	for (const FWRForestData::FPath& R : D.Routes)
	{
		if (R.Kind != TEXT("road") && R.Kind != TEXT("trail")) continue;
		for (int32 j = 0; j + 1 < R.Points.Num(); ++j)
		{
			for (int32 i = 0; i + 1 < Points.Num(); ++i)
			{
				FVector Hit;
				if (!FMath::SegmentIntersection2D(FVector(R.Points[j].X, R.Points[j].Y, 0.f), FVector(R.Points[j + 1].X, R.Points[j + 1].Y, 0.f), FVector(Points[i].X, Points[i].Y, 0.f), FVector(Points[i + 1].X, Points[i + 1].Y, 0.f), Hit)) continue;
				FPush P;
				P.At = FVector(Hit.X, Hit.Y, Points[i].Z);
				P.Radius = FMath::Max(R.Width, WidthAt(i)) * 0.75f + 300.f;
				P.Dir = DownAt(i);
				Pushes.Add(P);
			}
		}
	}
	for (const FWRForestData::FSite& K : D.Kit)
	{
		if (!K.Kind.ToString().Contains(TEXT("ford")) && !K.Kind.ToString().Contains(TEXT("stepping"))) continue;
		float Z; FVector Down;
		if (!SurfaceAt(K.At, Z, Down)) continue;
		FPush P;
		P.At = FVector(K.At.X, K.At.Y, Z);
		P.Radius = 900.f;
		P.Dir = Down;
		Pushes.Add(P);
	}
	for (const FWRForestData::FFall& F : D.Falls)
	{
		float Z; FVector Down;
		if (!SurfaceAt(F.Top, Z, Down)) continue;
		FPush P;
		P.At = F.Top - (F.Bottom - F.Top).GetSafeNormal2D() * 600.f;
		P.Radius = F.Width * 0.5f + 600.f;
		P.Dir = (F.Bottom - F.Top).GetSafeNormal2D();
		P.bLip = true;
		Pushes.Add(P);
	}

	if (GetNetMode() == NM_DedicatedServer) return;
	// The surface: a ribbon along the polyline at its widths, run 1.5 m into each bank so its edge hides under the
	// ground, U along the flow (the river material scrolls along it). The WaterMaterials river; the lake if it's missing.
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FColor> Col;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector Prev = Points[FMath::Max(0, i - 1)], Next = Points[FMath::Min(Points.Num() - 1, i + 1)];
		const FVector Dir = (Next - Prev).GetSafeNormal2D();
		const FVector Side(-Dir.Y, Dir.X, 0.f);
		const float Half = WidthAt(i) * 0.5f + 150.f;
		for (int32 k = 0; k < 3; ++k)
		{
			const float S = (k - 1) * Half;
			V.Add(Points[i] - GetActorLocation() + Side * S);
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(Along[i] / 800.f, k * 0.5f));
			Col.Add(FColor::White);
		}
	}
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		for (int32 k = 0; k < 2; ++k)
		{
			const int32 A = i * 3 + k, B = A + 1, C = A + 3, Dd = A + 4;
			T.Append({ A, C, B, B, C, Dd });
		}
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *FWRTuning::Get().Str(TEXT("hazards.water.material"), TEXT("/Game/WaterMaterials/Materials/M_River.M_River")));
	if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WaterMaterials/Materials/M_Lake.M_Lake"));
	Mat = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	Surface = WRForestHzImpl::NewMesh(this, RootComponent, TEXT("Surface"));
	WRForestHzImpl::Section(Surface, 0, V, T, N, UV, Col, Mat, true);
	Surface->SetCastShadow(false);
	Surface->SetCullDistance(WRForestHzImpl::Tn(TEXT("water.cull"), 60000.f));
}

bool AWRForestWater::SurfaceAt(const FVector& At, float& OutZ, FVector& OutDown) const
{
	float Best = 1e12f;
	bool bFound = false;
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const FVector A = Points[i], B = Points[i + 1];
		const FVector2D C = FMath::ClosestPointOnSegment2D(FVector2D(At), FVector2D(A), FVector2D(B));
		const float D = FVector2D::Distance(C, FVector2D(At));
		const float W = (Widths.IsValidIndex(i) ? Widths[i] : 1200.f) * 0.5f;
		if (D > W || D >= Best) continue;
		Best = D;
		const float Seg = FMath::Max(1.f, FVector2D::Distance(FVector2D(A), FVector2D(B)));
		OutZ = FMath::Lerp(A.Z, B.Z, FVector2D::Distance(FVector2D(A), C) / Seg);
		OutDown = (B - A).GetSafeNormal2D();
		bFound = true;
	}
	return bFound;
}

void AWRForestWater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bSpate = WRForestHazards::Spate(this);
	SpateLook = FMath::FInterpConstantTo(SpateLook, bSpate ? 1.f : 0.f, DeltaSeconds, 1.f / 20.f);   // turns over 20 s
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (Mat)
		{
			// Clear forest water (a dark green-brown over the gravel), brown and fast in spate.
			const FLinearColor Clear(0.05f, 0.085f, 0.07f), ClearDeep(0.012f, 0.03f, 0.025f);
			const FLinearColor Mud(0.24f, 0.16f, 0.08f), MudDeep(0.12f, 0.075f, 0.035f);
			Mat->SetVectorParameterValue(TEXT("Colour"), FMath::Lerp(Clear, Mud, SpateLook));
			Mat->SetVectorParameterValue(TEXT("ColourDeep"), FMath::Lerp(ClearDeep, MudDeep, SpateLook));
			Mat->SetScalarParameterValue(TEXT("River_Speed"), FMath::Lerp(WRForestHzImpl::Tn(TEXT("water.flowSpeed"), 0.4f), WRForestHzImpl::Tn(TEXT("water.spateFlowSpeed"), 1.2f), SpateLook));
		}
		// The roar of a river in spate, from the bit of it nearest you.
		FVector ViewAt;
		if (Kind == TEXT("river") && WRForestHzImpl::View(GetWorld(), ViewAt))
		{
			if (SpateLook > 0.01f && !Roar) Roar = WRSound::StartLoop(this, TEXT("forest.spate"));
			if (Roar)
			{
				float BestD = 1e12f;
				FVector Near = Points[0];
				for (int32 i = 0; i + 1 < Points.Num(); ++i)
				{
					const FVector2D C = FMath::ClosestPointOnSegment2D(FVector2D(ViewAt), FVector2D(Points[i]), FVector2D(Points[i + 1]));
					const float D = FVector2D::Distance(C, FVector2D(ViewAt));
					if (D < BestD) { BestD = D; Near = FVector(C, Points[i].Z + 50.f); }
				}
				Roar->SetWorldLocation(Near);
				Roar->SetVolumeMultiplier(SpateLook);
				if (SpateLook <= 0.f) { Roar->Stop(); Roar = nullptr; }
			}
		}
		StepLogs(DeltaSeconds, SpateLook > 0.5f);
	}
	if (!bSpate || Pushes.Num() == 0) return;
	// The shove: the server moves everyone; a player's own machine moves its own body the same way (prediction), so
	// it doesn't fight the server's corrections.
	// (This actor is local on every machine, so it's the net mode that says whose bodies this machine moves.)
	const float Push = WRForestHzImpl::Tn(TEXT("spate.push"), 900.f);
	const bool bServer = GetNetMode() != NM_Client;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || (!bServer && !C->IsLocallyControlled())) continue;
		const FVector F = WRForestHzImpl::Feet(C);
		for (const FPush& P : Pushes)
		{
			if (FVector::Dist2D(F, P.At) > P.Radius) continue;
			float Z; FVector Down;
			if (!SurfaceAt(F, Z, Down) || F.Z > Z - 20.f) continue;
			UCharacterMovementComponent* Move = C->GetCharacterMovement();
			if (!Move) continue;
			const float Want = Push * (P.bLip ? WRForestHzImpl::Tn(TEXT("spate.lipMult"), 1.25f) : 1.f);
			if (Move->IsSwimming() || Move->IsFalling())
			{
				const float Have = FVector::DotProduct(Move->Velocity, P.Dir);
				if (Have < Want) Move->Velocity += P.Dir * FMath::Min(Want - Have, Want * 3.f * DeltaSeconds);
			}
			else
			{
				// Wading: the current takes the legs out from under you, most of its speed.
				C->AddActorWorldOffset(P.Dir * Want * WRForestHzImpl::Tn(TEXT("spate.wadeShare"), 0.5f) * DeltaSeconds, true);
			}
			break;
		}
	}
}

void AWRForestWater::StepLogs(float Dt, bool bSpate)
{
	// Logs riding the flood (only the Alder and its kind; streams and the race are too small): a few trunks bobbing
	// down the surface at the flow's speed, turning slowly, coming back in at the top when they reach the sea.
	const int32 Want = (bSpate && Kind == TEXT("river") && Along.Num() > 0 && Along.Last() > 30000.f) ? int32(WRForestHzImpl::Tn(TEXT("spate.logs"), 6.f)) : 0;
	if (Want == 0)
	{
		for (UProceduralMeshComponent* L : Logs) if (L) L->SetVisibility(false);
		return;
	}
	FVector ViewAt;
	if (!WRForestHzImpl::View(GetWorld(), ViewAt)) return;
	while (Logs.Num() < Want)
	{
		const int32 i = Logs.Num();
		FWRToolMesh M;
		const float Len = 380.f + float((60 * i) % 160);
		const float R = 22.f + (i % 3) * 4.f;
		M.Lathe(FVector(-Len * 0.5f, 0, 0), FVector::ForwardVector, Len, { FVector(0.f, R, R * 0.95f), FVector(0.5f, R * 0.92f, R * 0.9f), FVector(1.f, R * 0.8f, R * 0.78f) }, 9, WRForestHzImpl::Bark, 0.9f, 6.f, 1.f + i);
		M.Lathe(FVector(Len * 0.1f, 0, R * 0.6f), FVector(0.4f, 0.3f, 1.f).GetSafeNormal(), 70.f, { FVector(0.f, 6.f, 6.f), FVector(1.f, 2.f, 2.f) }, 6, WRForestHzImpl::Bark * 0.8f, 0.9f);
		UProceduralMeshComponent* L = WRForestHzImpl::NewMesh(this, RootComponent, *FString::Printf(TEXT("Log%d"), i));
		L->SetUsingAbsoluteLocation(true);
		L->SetUsingAbsoluteRotation(true);
		M.ApplyTo(L);
		Logs.Add(L);
		LogAlong.Add(Along.Last() * float(i) / Want);
	}
	const float Speed = WRForestHzImpl::Tn(TEXT("spate.logSpeed"), 450.f);
	const float T = WRForestHzImpl::Now(this);
	for (int32 i = 0; i < Logs.Num(); ++i)
	{
		float& D = LogAlong[i];
		D += Speed * Dt;
		if (D >= Along.Last()) D = 0.f;
		int32 Seg = 0;
		while (Seg + 2 < Along.Num() && Along[Seg + 1] < D) ++Seg;
		const float SegLen = FMath::Max(1.f, Along[Seg + 1] - Along[Seg]);
		const float K = FMath::Clamp((D - Along[Seg]) / SegLen, 0.f, 1.f);
		const FVector P = FMath::Lerp(Points[Seg], Points[Seg + 1], K);
		const FVector Dir = (Points[Seg + 1] - Points[Seg]).GetSafeNormal2D();
		const FVector Side(-Dir.Y, Dir.X, 0.f);
		const float Width = (Widths.IsValidIndex(Seg) ? Widths[Seg] : 1200.f);
		const FVector At = P + Side * (Width * 0.3f * FMath::Sin(i * 2.3f)) + FVector(0, 0, 4.f * FMath::Sin(T * 1.7f + i));
		const bool bNear = FVector::Dist2D(At, ViewAt) < 25000.f;
		Logs[i]->SetVisibility(bNear);
		if (!bNear) continue;
		Logs[i]->SetWorldLocationAndRotation(At, FRotator(0.f, Dir.Rotation().Yaw + 25.f * FMath::Sin(T * 0.2f + i), T * 12.f + i * 40.f));
	}
}

// ================================================================================================ helpers (part 2)

namespace WRForestHzImpl
{
	/** The ground (cm) under a point from the land's own height function (no trace: works before the runtime land's
	 *  collision exists, and on a dedicated server). False outside every live land. */
	bool LandZ(const FVector& At, float& OutZ)
	{
		// The forest is a heightfield land: its own sampler (holes and the hidden strip under the island answer false).
		return WRLand::GroundUnder(At, OutZ);
	}

	/** The ground under a point: the land's function first, then a trace, then the point itself. */
	float GroundZAt(UWorld* W, const FVector& At)
	{
		float Z = 0.f;
		if (LandZ(At, Z)) return Z;
		FVector G;
		if (Ground(W, At, G, 3000.f, 9000.f)) return G.Z;
		return At.Z;
	}

	/** Data/forest.json as written by the generator (metres): the rows FWRForestData doesn't carry (a brake's gap). Read
	 *  once. */
	const TSharedPtr<FJsonObject>& Raw()
	{
		static TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("forest.json"));
		return Root;
	}
	float RawNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = Fallback;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}
	FString RawStr(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(Field, V);
		return V;
	}
	bool RawBool(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, bool Fallback)
	{
		bool V = Fallback;
		if (O.IsValid()) O->TryGetBoolField(Field, V);
		return V;
	}
	/** A row's x, y, z (metres) as game cm. */
	FVector RawAt(const TSharedPtr<FJsonObject>& O)
	{
		return FVector(RawNum(O, TEXT("x"), 0.f), RawNum(O, TEXT("y"), 0.f), RawNum(O, TEXT("z"), 0.f)) * 100.f;
	}
	const TArray<TSharedPtr<FJsonValue>>* RawArray(const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return Raw().IsValid() && Raw()->TryGetArrayField(Field, A) ? A : nullptr;
	}

	void SetTag(AWRCharacterBase* C, const FGameplayTag& Tag, bool bOn)
	{
		if (UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetLooseGameplayTagCount(Tag, bOn ? 1 : 0, EGameplayTagReplicationState::TagOnly);
		}
	}

	AWRPlayerController* ControllerOf(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr;
	}

	/** An axe in the hand (a hatchet tool with edge left, or an axe-class weapon): what the heartwood core wants. */
	bool HoldsAxe(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		if (!Inv) return false;
		const FWRItemStack& H = Inv->GetEquipped(EWREquipSlot::Weapon);
		if (H.IsEmpty()) return false;
		if (const FWRToolDef* Tool = FWRGathering::FindTool(H.ItemId)) return Tool->Kind == TEXT("hatchet") && H.Durability > 0.f;
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(H.ItemId);
		return Def && Def->WeaponClass == TEXT("axe");
	}

	/** Game hour H inside [From, To), wrapping midnight (21 -> 4). */
	bool HourIn(float H, float From, float To)
	{
		return From <= To ? (H >= From && H < To) : (H >= From || H < To);
	}

	/** The tree-kit species an Elder Fall draws as (forest.json "dead" giants are the kit's dead_giant). */
	const TCHAR* DeadSpecies() { return TEXT("dead_giant"); }
}

// ================================================================================================ more shared calls

bool WRForestHazards::IsHazardThing(const AActor* Thing)
{
	return Thing && (Thing->IsA<AWRWidowmaker>() || Thing->IsA<AWRWaspNest>());
}

void WRForestHazards::FireLanded(UWorld* World, const FVector& At)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	const UWRForestHazards* H = UWRForestHazards::Get(World);
	if (!H || !H->bActive) return;
	// A fire blow or a fire bolt's burst: the canes it touches catch (a lane torchGapRadius wide), and the barred door's
	// living bark takes a full burn (cooks/biome-forest.md 2.8: "fire spells do the same"; 2.22 F-S4).
	WRForest::BurnThornbrake(World, At, WRForestHzImpl::Tn(TEXT("thornbrake.torchGapRadius"), 400.f) * 0.5f);
	WRForestHollow::FireAt(World, At, WRForestHzImpl::Tn(TEXT("secrets.doorBurnSeconds"), 6.f));
}

float WRForestHazards::CropScale(const UObject* WorldContext, const FVector& At, bool bRainOnPlot)
{
	if (!WRForest::Contains(At)) return 1.f;
	float Scale = 1.f;
	// The signature Rain (cooks/biome-forest.md 2.9): +25% on every outdoor plot it falls on (the watering itself is the
	// weather row's watersCrops, read by WRFarm).
	if (bRainOnPlot && WRForest::Raining(WorldContext, At)) Scale *= WRForestHzImpl::Tn(TEXT("rain.cropBoost"), 1.25f);
	// The Bloom (2.10): every crop in the forest grows twice as fast for the day.
	if (WRForest::Bloom(WorldContext)) Scale *= WRForestHzImpl::Tn(TEXT("bloom.growthMult"), 2.f);
	return Scale;
}

bool WRForestHazards::Misty(const UObject* WorldContext, const FVector& Pos)
{
	if (!WRForest::Contains(Pos)) return false;
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(WorldContext, TEXT("forest"));
	return K && K->Id == TEXT("fog");
}

float WRForestHazards::GatherScale(const UObject* WorldContext, FName ItemId, const FVector& At)
{
	// The plenty after rain (world-map's rule, the forest's signature sap): weeping resin runs thick in the morning mist.
	if (!Misty(WorldContext, At)) return 1.f;
	TArray<FString> Items;
	FWRTuning::Get().Str(TEXT("hazards.mist.resinItems"), TEXT("resin")).ParseIntoArray(Items, TEXT(","));
	for (const FString& I : Items)
	{
		if (FName(*I.TrimStartAndEnd()) == ItemId) return WRForestHzImpl::Tn(TEXT("mist.resinBoost"), 1.5f);
	}
	return 1.f;
}

bool WRForestHazards::RefuseFelling(AActor* Who, const FVector& Tree)
{
	if (!Who || !WRForest::NoFelling(Tree)) return false;
	UWorld* World = Who->GetWorld();
	// The axe bounces off root-bound wood: a dull knock and a few chips (cooks/biome-forest.md 2.7). The line once every
	// few seconds, not every swing.
	const FVector Out = (Who->GetActorLocation() - Tree).GetSafeNormal2D();
	WRFx::Broadcast(World, TEXT("forest.axe_bounce"), Tree + FVector(0.f, 0.f, 120.f) + Out * 60.f, Out.Rotation(), 1.f, TEXT("gather.wood"));
	if (AWRPlayerController* PC = WRForestHzImpl::ControllerOf(Who))
	{
		static TMap<TWeakObjectPtr<AWRPlayerController>, double> LastTold;
		double& Last = LastTold.FindOrAdd(PC, -100.0);
		const double Now = WRForestHzImpl::Now(World);
		if (Now - Last > 4.0 || Now < Last)
		{
			Last = Now;
			PC->ClientNotify(FWRTuning::Get().Str(TEXT("hazards.felling.refuseText"), TEXT("The roots hold it.")));
		}
	}
	return true;
}

bool WRForestHazards::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return false;
	FWRToolMesh M;
	if (ModelPath == TEXT("code:blightjar"))
	{
		// Bootlace blight (DC 2.19): a squat clay jar 16 cm long, its belly dark where the black rhizomorph slurry soaks
		// through, the neck stopped with a wax-sealed bung, a twist of black lace tied round the neck.
		const FLinearColor Clay = WRForestHzImpl::Srgb(150, 104, 70), ClayDark = WRForestHzImpl::Srgb(98, 66, 44);
		const FLinearColor Wax = WRForestHzImpl::Srgb(196, 150, 70), Lace = WRForestHzImpl::Srgb(26, 22, 20);
		M.Lathe(FVector(-8.f, 0.f, 0.f), FVector::ForwardVector, 16.f, { FVector(0.f, 4.f, 4.f), FVector(0.1f, 6.5f, 6.5f), FVector(0.45f, 7.6f, 7.6f), FVector(0.78f, 5.2f, 5.2f), FVector(0.86f, 2.8f, 2.8f), FVector(1.f, 3.2f, 3.2f) }, 12, Clay, 0.85f);
		M.Lathe(FVector(-2.f, 0.f, 0.f), FVector::ForwardVector, 6.f, { FVector(0.f, 7.65f, 7.65f), FVector(1.f, 7.4f, 7.4f) }, 12, ClayDark, 0.9f);
		M.Lathe(FVector(7.5f, 0.f, 0.f), FVector::ForwardVector, 3.5f, { FVector(0.f, 3.f, 3.f), FVector(0.6f, 3.3f, 3.3f), FVector(1.f, 2.4f, 2.4f) }, 8, Wax, 0.5f);
		M.Tube({ FVector(5.5f, 3.f, 0.f), FVector(5.5f, 0.f, 3.1f), FVector(5.5f, -3.f, 0.f), FVector(5.5f, 0.f, -3.1f) }, 0.45f, 5, Lace, 0.8f, true);
		M.Tube({ FVector(5.5f, 0.f, -3.1f), FVector(3.f, 1.f, -6.f), FVector(1.f, 0.5f, -8.f) }, 0.4f, 5, Lace, 0.8f);
		M.ApplyTo(Mesh);
		return true;
	}
	if (ModelPath == TEXT("code:smokepot"))
	{
		// The smoke pot (DC 2.19): a round clay pot with a pierced lid, rotten wood and green leaves packed in, a
		// smouldering wick out of the lid trailing smoke as it flies.
		const FLinearColor Clay = WRForestHzImpl::Srgb(166, 120, 84), Soot = WRForestHzImpl::Srgb(46, 40, 36), Wick = WRForestHzImpl::Srgb(120, 96, 60);
		M.Lathe(FVector(-7.f, 0.f, 0.f), FVector::ForwardVector, 13.f, { FVector(0.f, 3.f, 3.f), FVector(0.15f, 6.f, 6.f), FVector(0.5f, 7.f, 7.f), FVector(0.85f, 5.5f, 5.5f), FVector(1.f, 5.f, 5.f) }, 12, Clay, 0.85f);
		M.Lathe(FVector(6.f, 0.f, 0.f), FVector::ForwardVector, 1.5f, { FVector(0.f, 5.4f, 5.4f), FVector(1.f, 4.6f, 4.6f) }, 12, Soot, 0.9f);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = i * PI / 3.f;
			M.Lathe(FVector(7.4f, FMath::Cos(A) * 2.8f, FMath::Sin(A) * 2.8f), FVector::ForwardVector, 0.3f, { FVector(0.f, 0.7f, 0.7f), FVector(1.f, 0.7f, 0.7f) }, 6, WRForestHzImpl::CharBlack, 1.f);
		}
		M.Tube({ FVector(7.5f, 0.f, 0.f), FVector(10.f, 0.6f, 0.4f), FVector(12.f, 0.2f, 1.2f) }, 0.5f, 5, Wick, 0.9f);
		M.ApplyTo(Mesh);
		WRFx::Attach(Mesh, TEXT("forest.smoke_trail"));
		return true;
	}
	return false;
}

bool WRForestHazards::ThrownLanded(UWorld* World, FName Kind, const FVector& At, AActor* Thrower)
{
	if (!World || World->GetNetMode() == NM_Client) return false;
	if (Kind == TEXT("blightjar"))
	{
		// The jar shatters: black lace spatter on whatever it hit (the Sylvaroth counter: WRForestWorld broadcasts it to
		// whoever listens, the boss track's root lines and knots).
		WRFx::Broadcast(World, TEXT("forest.blight_burst"), At, FRotator::ZeroRotator, 1.f, TEXT("forest.comb_break"));
		UWRForestWorld::BlightLanded(World, At, Thrower);
		return true;
	}
	if (Kind == TEXT("smokepot"))
	{
		WRFx::Broadcast(World, TEXT("forest.smokepot_break"), At, FRotator::ZeroRotator, 1.f, TEXT("forest.comb_break"));
		UWRForestWorld::SmokePotLanded(World, At, Thrower);
		return true;
	}
	return false;
}

bool WRForestHazards::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me || !WRForest::Contains(Me->GetActorLocation())) return false;
	// The Hollow Root and the secrets first (their doors stand beside brambles and nests).
	if (WRForestHollow::Prompt(PC, OutTarget, OutVerb)) return true;
	// A wasp nest within arm's reach (on a bough at head height, or under an overhang): rob it once it's smoked.
	const FVector At = Me->GetActorLocation();
	for (TActorIterator<AWRWaspNest> It(Me->GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), At) > WRForestHzImpl::Tn(TEXT("waspNest.robReach"), 320.f)) continue;
		OutTarget = TEXT("Bramblewasp nest");
		OutVerb = It->IsCalm() ? TEXT("Take a larva") : TEXT("Smoke it first (a torch held 3 s, or a smoke pot)");
		return true;
	}
	return false;
}

bool WRForestHazards::ClientInteract(AWRPlayerController* PC)
{
	const AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me || !WRForest::Contains(Me->GetActorLocation())) return false;
	if (WRForestHollow::ClientInteract(PC)) return true;
	const FVector At = Me->GetActorLocation();
	for (TActorIterator<AWRWaspNest> It(Me->GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), At) > WRForestHzImpl::Tn(TEXT("waspNest.robReach"), 320.f)) continue;
		if (!It->IsCalm())
		{
			PC->ShowLocalNotice(TEXT("The nest is awake: smoke it first (hold a torch to it, or throw a smoke pot)"));
			return true;
		}
		const FVector L = It->GetActorLocation();
		PC->SendCommand(FString::Printf(TEXT("world fhaz use %.1f %.1f %.1f"), L.X, L.Y, L.Z));
		return true;
	}
	return false;
}

FString WRForestHazards::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	UWRForestHazards* H = UWRForestHazards::Get(World);
	if (!H || !H->bActive) return TEXT("The forest isn't here");
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString Verb = Arg(2);
	if (Verb == TEXT("secret")) return WRForestHollow::Command(PC, Args);
	if (Verb == TEXT("use"))
	{
		const APawn* Me = PC->GetPawn();
		if (!Me) return FString();
		const FVector At(FCString::Atof(*Arg(3)), FCString::Atof(*Arg(4)), FCString::Atof(*Arg(5)));
		AWRWaspNest* Best = nullptr;
		float BestD = 200.f;
		for (TActorIterator<AWRWaspNest> It(World); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), At);
			if (D < BestD) { BestD = D; Best = *It; }
		}
		// The server checks the reach itself (a little slack for the step between the press and the command).
		if (!Best || FVector::Dist(Best->GetActorLocation(), Me->GetActorLocation()) > WRForestHzImpl::Tn(TEXT("waspNest.robReach"), 320.f) + 150.f) return TEXT("Nothing to take here");
		return Best->StealLarva(PC);
	}
#if !UE_BUILD_SHIPPING
	// Dev: world fhaz spate | elderfall (the Bloom itself: "world dev event bloom").
	if (Verb == TEXT("spate")) { H->DevSpate(); return TEXT("The Alder is rising"); }
	if (Verb == TEXT("elderfall")) { H->DevElderFall(); return TEXT("A dead giant cracks somewhere in the old growth"); }
	if (Verb == TEXT("bloom")) return TEXT("Use: world dev event bloom");
#endif
	return TEXT("?");
}

// ================================================================================================ AWRForestRingPillar

AWRForestRingPillar::AWRForestRingPillar()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestRingPillar::Build(const FVector& Ground)
{
	SetActorLocation(Ground);
	// The Grovekin pillar (cooks/world-map.md 2.12): a 9 m shaft of oak-grey granite on a stepped plinth, a living root
	// carved round it in a slow double spiral (the Grovekin's knotwork) gripping the steps at its foot, the trail-tree
	// waymark (a sapling bent at the knee) cut on its cap, and three bands of green glass set into the stone that burn
	// when the forest's Bloom comes.
	const FLinearColor Granite = WRForestHzImpl::Srgb(122, 120, 112), GraniteDark = WRForestHzImpl::Srgb(92, 90, 84);
	const FLinearColor RootStone = WRForestHzImpl::Srgb(104, 98, 86), Lichen = WRForestHzImpl::Srgb(132, 140, 96);
	const float H = 900.f, R = 85.f;
	FWRToolMesh Stone;
	// The plinth: two square steps (a four-sided lathe reads as a cut block), seated 20 cm into the terrace.
	Stone.Lathe(FVector(0.f, 0.f, -20.f), FVector::UpVector, 60.f, { FVector(0.f, 190.f, 190.f), FVector(1.f, 185.f, 185.f) }, 4, GraniteDark, 0.92f);
	Stone.Lathe(FVector(0.f, 0.f, 40.f), FVector::UpVector, 45.f, { FVector(0.f, 140.f, 140.f), FVector(1.f, 135.f, 135.f) }, 4, Granite, 0.9f);
	// The shaft: eight-sided with a little entasis, a weathered cap.
	Stone.Lathe(FVector(0.f, 0.f, 85.f), FVector::UpVector, H, { FVector(0.f, R, R), FVector(0.35f, R * 1.04f, R * 1.04f), FVector(0.9f, R * 0.9f, R * 0.9f), FVector(1.f, R * 0.92f, R * 0.92f) }, 8, Granite, 0.88f);
	Stone.Lathe(FVector(0.f, 0.f, 85.f + H), FVector::UpVector, 40.f, { FVector(0.f, R * 1.15f, R * 1.15f), FVector(0.6f, R * 1.1f, R * 1.1f), FVector(1.f, R * 0.5f, R * 0.5f) }, 8, GraniteDark, 0.9f);
	// The carved root: standing proud of the shaft, thickest at its foot where it grows out of the plinth.
	TArray<FVector> Spiral;
	for (int32 i = 0; i <= 48; ++i)
	{
		const float T = float(i) / 48.f;
		const float A = T * 4.f * PI;
		const float Rr = R * (1.04f - 0.12f * T) + 9.f;
		Spiral.Add(FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 95.f + T * (H - 40.f)));
	}
	Stone.Tube(Spiral, 11.f, 7, RootStone, 0.9f);
	Stone.Tube({ Spiral[0], FVector(150.f, 30.f, 60.f), FVector(175.f, 60.f, 0.f) }, 10.f, 6, RootStone, 0.9f);
	Stone.Tube({ Spiral[2], FVector(-40.f, 150.f, 60.f), FVector(-60.f, 178.f, 2.f) }, 8.f, 6, RootStone, 0.9f);
	// The waymark on the cap.
	const FVector Cap(0.f, 0.f, 85.f + H + 24.f);
	Stone.Tube({ Cap, Cap + FVector(0.f, 0.f, 40.f), Cap + FVector(30.f, 0.f, 52.f), Cap + FVector(55.f, 0.f, 50.f) }, 6.f, 6, RootStone, 0.9f);
	// Lichen scabs on the weather (north) face.
	for (int32 i = 0; i < 6; ++i) Stone.Lathe(FVector(-R * 0.98f, 18.f * (i % 3 - 1), 160.f + i * 120.f), FVector(-1.f, 0.f, 0.f), 2.f, { FVector(0.f, 22.f, 16.f), FVector(1.f, 18.f, 12.f) }, 7, Lichen, 0.98f);
	UProceduralMeshComponent* Body = WRForestHzImpl::NewMesh(this, RootComponent, TEXT("PillarStone"));
	Stone.ApplyTo(Body);
	// The glass: three bands set flush into the shaft, moss-green glass (#5E8C31) lit from inside.
	FWRToolMesh Glass;
	for (int32 b = 0; b < 3; ++b)
	{
		const float Z = 85.f + H * (0.32f + 0.22f * b);
		Glass.Lathe(FVector(0.f, 0.f, Z), FVector::UpVector, 28.f, { FVector(0.f, R * 1.02f, R * 1.02f), FVector(1.f, R * 1.0f, R * 1.0f) }, 16, WRForestHzImpl::Srgb(94, 140, 49), 0.2f);
	}
	Bands = WRForestHzImpl::NewMesh(this, RootComponent, TEXT("PillarGlass"));
	Glass.ApplyTo(Bands);
	if (UMaterialInstanceDynamic* Mid = WRTreeKit::GlowMaterial(this, WRForestHzImpl::Srgb(110, 230, 90), 0.4f))
	{
		Bands->SetMaterial(0, Mid);
		GlowMats.Add(Mid);
	}
	Light = NewObject<UPointLightComponent>(this, TEXT("PillarLight"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 85.f + H * 0.6f));
	Light->SetLightColor(WRForestHzImpl::Srgb(110, 230, 90));
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(WRForestHzImpl::Tn(TEXT("bloom.pillarLightRadius"), 6000.f));
	Light->SetCastShadows(false);
	Light->RegisterComponent();
}

void AWRForestRingPillar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Burns through the Bloom's warning and the Bloom (the world's event beacon, no UI): the glass flares up over 3 s,
	// a light column stands over the Ring while it burns, then it dies back slowly.
	const bool bOn = WRForestHazards::HeartsBright(this);
	Burn = FMath::FInterpConstantTo(Burn, bOn ? 1.f : 0.f, DeltaSeconds, bOn ? 1.f / 3.f : 1.f / 20.f);
	const float Flicker = 0.9f + 0.1f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.3f);
	for (UMaterialInstanceDynamic* M : GlowMats) if (M) M->SetScalarParameterValue(TEXT("Glow"), 0.4f + Burn * WRForestHzImpl::Tn(TEXT("bloom.pillarGlow"), 30.f) * Flicker);
	if (Light) Light->SetIntensity(Burn * WRForestHzImpl::Tn(TEXT("bloom.pillarLight"), 60000.f) * Flicker);
	if (Burn > 0.5f)
	{
		FlameClock -= DeltaSeconds;
		if (FlameClock <= 0.f)
		{
			FlameClock = 10.f;
			WRFx::SpawnFor(this, TEXT("forest.pillar_column"), GetActorLocation() + FVector(0.f, 0.f, 1000.f), FRotator::ZeroRotator, 1.f, 11.f);
		}
	}
}

// ================================================================================================ AWRElderFall

AWRElderFall::AWRElderFall()
{
	bReplicates = true;
	bAlwaysRelevant = true;   // its crack is heard from 1 km, and its crush line must be on every nearby machine on time
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(RootComponent);
	// One box along the bole: the standing trunk's collision, and once it lies, the log you climb on (it turns with the
	// pivot, so it is wherever the trunk is drawn, on every machine).
	LogBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("LogBlock"));
	LogBlock->SetupAttachment(Pivot);
	LogBlock->SetCollisionProfileName(TEXT("BlockAll"));
	LogBlock->SetCanEverAffectNavigation(false);
}

void AWRElderFall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRElderFall, FallYaw);
	DOREPLIFETIME(AWRElderFall, FallLength);
	DOREPLIFETIME(AWRElderFall, StartAt);
	DOREPLIFETIME(AWRElderFall, EndTilt);
}

void AWRElderFall::ResolveShape()
{
	if (bShapeKnown) return;
	bShapeKnown = true;
	Variant = WRTreeKit::VariantAt(GetActorLocation());
	const float RefH = FMath::Max(1.f, WRTreeKit::ReferenceHeight(WRForestHzImpl::DeadSpecies()));
	TrunkRadius = FMath::Max(120.f, WRTreeKit::ReferenceRadius(WRForestHzImpl::DeadSpecies()) * FallLength / RefH);
	// The bole's box: up to 84% of the height (above that it's dead limbs), a little inside the bark.
	LogBlock->SetBoxExtent(FVector(TrunkRadius * 0.8f, TrunkRadius * 0.8f, FallLength * 0.42f));
	LogBlock->SetRelativeLocation(FVector(0.f, 0.f, FallLength * 0.42f));
	// The fall (a rigid trunk pivoting on its broken foot): theta'' = 3g/(2L) sin(theta), from the lean the tell ends on,
	// worked once at 1/120 s so every machine draws the same fall from the replicated start.
	FallTable.Reset();
	const float G = 980.f, K = 3.f * G / (2.f * FMath::Max(1000.f, FallLength));
	float Theta = FMath::DegreesToRadians(WRForestHzImpl::Tn(TEXT("bloom.elderLean"), 6.f));
	float W = 0.f;
	const float End = FMath::DegreesToRadians(EndTilt);
	const float Dt = 1.f / 120.f;
	for (int32 i = 0; i < 120 * 30 && Theta < End; ++i)
	{
		FallTable.Add(FMath::RadiansToDegrees(Theta));
		W += K * FMath::Sin(Theta) * Dt;
		Theta += W * Dt;
	}
	FallTable.Add(EndTilt);
}

float AWRElderFall::Tilt(float Since) const
{
	const float Warn = WRForestHzImpl::Tn(TEXT("bloom.elderFallWarn"), 5.f);
	const float Lean = WRForestHzImpl::Tn(TEXT("bloom.elderLean"), 6.f);
	if (Since <= 0.f) return 0.f;
	if (Since < Warn)
	{
		// The tell: it lurches over in two cracks (at the start and halfway), shuddering, settling a little each time.
		const float A = Since / Warn;
		const float Steps = 0.45f * FMath::SmoothStep(0.f, 0.15f, A) + 0.55f * FMath::SmoothStep(0.5f, 0.7f, A);
		return Lean * Steps + 0.25f * FMath::Sin(Since * 9.f) * (1.f - A);
	}
	if (FallTable.Num() == 0) return EndTilt;
	const float F = (Since - Warn) * 120.f;
	const int32 I = FMath::FloorToInt(F);
	if (I >= FallTable.Num() - 1) return EndTilt;
	return FMath::Lerp(FallTable[I], FallTable[I + 1], F - I);
}

void AWRElderFall::BeginPlay()
{
	Super::BeginPlay();
	ResolveShape();
	if (GetNetMode() != NM_DedicatedServer) BuildLook();
	OnRep_Start();
}

void AWRElderFall::BuildLook()
{
	if (Trunk) return;
	// A dead giant (the tree kit's dead_giant: grey, barkless in strips, its crown a few snapped limbs), FallLength tall,
	// its root plate sunk 1.5 m like any standing giant's, turned by its place.
	Trunk = NewObject<UStaticMeshComponent>(this, TEXT("DeadGiant"));
	Trunk->SetupAttachment(Pivot);
	Trunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trunk->SetCanEverAffectNavigation(false);
	if (UStaticMesh* M = WRTreeKit::Mesh(this, WRForestHzImpl::DeadSpecies(), Variant)) Trunk->SetStaticMesh(M);
	const float RefH = FMath::Max(1.f, WRTreeKit::ReferenceHeight(WRForestHzImpl::DeadSpecies()));
	Trunk->SetRelativeLocation(FVector(0.f, 0.f, -150.f));
	Trunk->SetRelativeRotation(FRotator(0.f, 360.f * (WRForestHzImpl::Jit(GetActorLocation(), 41) * 0.5f + 0.5f), 0.f));
	Trunk->SetRelativeScale3D(FVector(FallLength / RefH));
	Trunk->RegisterComponent();
}

void AWRElderFall::Begin()
{
	if (!HasAuthority() || HasBegun()) return;
	// Where its crown meets the ground: the first point along the fall line the trunk touches as it goes over.
	const FVector Base = GetActorLocation();
	const FVector Dir = FRotator(0.f, FallYaw, 0.f).Vector();
	float Best = 92.f;
	for (float D = 600.f; D <= FallLength; D += 200.f)
	{
		const float Up = WRForestHzImpl::GroundZAt(GetWorld(), Base + Dir * D) - Base.Z + TrunkRadius * 0.6f;
		if (FMath::Sqrt(D * D + Up * Up) > FallLength) break;
		Best = FMath::Min(Best, FMath::RadiansToDegrees(FMath::Atan2(D, Up)));
	}
	EndTilt = FMath::Clamp(Best, 55.f, 100.f);
	bShapeKnown = false;
	ResolveShape();
	StartAt = WRForestHzImpl::Now(this);
	ForceNetUpdate();
	OnRep_Start();
	UE_LOG(LogWildreach, Log, TEXT("FOREST Bloom: an Elder Fall at %s cracks (comes to rest at %.0f deg, bearing %.0f)"), *Base.ToCompactString(), EndTilt, FallYaw);
}

void AWRElderFall::OnRep_Start()
{
	if (StartAt < 0.f || bToldHere) return;
	// The fall table needs the replicated rest tilt (it may arrive with the start).
	bShapeKnown = false;
	ResolveShape();
	bToldHere = true;
	const float Since = WRForestHzImpl::Now(this) - StartAt;
	const float Warn = WRForestHzImpl::Tn(TEXT("bloom.elderFallWarn"), 5.f);
	if (Since > Warn + FallTable.Num() / 120.f + 2.f)
	{
		// A late joiner: it's already down.
		bFellHere = true;
		bLanded = true;
		LieDown();
		return;
	}
	LogBlock->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // no shoving bodies about while it goes over
	if (GetNetMode() == NM_DedicatedServer) return;
	// The tell (DC 2.10, 5 s): cracks like cannon fire heard from a kilometre, every bird in the crown lifting at once,
	// bark and dust sliding down the trunk, and litter shaken up along the ground where it will land.
	const FVector Base = GetActorLocation();
	const FVector Dir = FRotator(0.f, FallYaw, 0.f).Vector();
	WRSound::PlayAt(this, TEXT("forest.elder_crack"), Base + FVector(0.f, 0.f, 600.f));
	WRFx::SpawnAt(this, TEXT("forest.birds_lift"), Base + FVector(0.f, 0.f, FallLength * 0.85f), FRotator::ZeroRotator, 2.f);
	WRFx::SpawnFor(this, TEXT("forest.elder_dust"), Base + FVector(0.f, 0.f, 300.f), FRotator::ZeroRotator, 2.f, Warn);
	const float Reach = FallLength * FMath::Sin(FMath::DegreesToRadians(FMath::Min(EndTilt, 90.f)));
	for (float D = 800.f; D < Reach; D += 1000.f)
	{
		const FVector P = Base + Dir * D;
		WRFx::SpawnFor(this, TEXT("forest.elder_line"), FVector(P.X, P.Y, WRForestHzImpl::GroundZAt(GetWorld(), P) + 10.f), FRotator(0.f, FallYaw, 0.f), 1.f, Warn);
	}
	WRFx::Shake(this, 0.15f, Base, 6000.f);
}

void AWRElderFall::LieDown()
{
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, FRotator(0.f, FallYaw, 0.f).Vector()).GetSafeNormal();
	Pivot->SetRelativeRotation(FQuat(Axis, FMath::DegreesToRadians(EndTilt)));
	LogBlock->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AWRElderFall::Land()
{
	bLanded = true;
	LieDown();
	if (!HasAuthority() || bCrushed) return;
	bCrushed = true;
	// The crush: everything along the line it came down on, player or creature, tame or wild (DC 2.10: lethal). The spot
	// was picked 150 m clear of every camp, the village, claims, shrines and arenas.
	const FVector Base = GetActorLocation();
	const FVector Dir = FRotator(0.f, FallYaw, 0.f).Vector();
	const float Reach = FallLength * FMath::Sin(FMath::DegreesToRadians(FMath::Min(EndTilt, 90.f))) + TrunkRadius;
	const float Half = FMath::Max(TrunkRadius, WRForestHzImpl::Tn(TEXT("bloom.elderCrushWidth"), 500.f) * 0.5f);
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		AWRCharacterBase* V = *It;
		if (!V->IsAlive()) continue;
		const FVector P = V->GetActorLocation() - Base;
		const float Along = FVector::DotProduct(P, Dir);
		const float Side = FVector::CrossProduct(Dir, P).Z;
		if (Along < 0.f || Along > Reach || FMath::Abs(Side) > Half + V->GetSimpleCollisionRadius()) continue;
		const float GroundHere = WRForestHzImpl::GroundZAt(GetWorld(), V->GetActorLocation());
		if (V->GetActorLocation().Z - V->GetSimpleCollisionHalfHeight() > GroundHere + TrunkRadius * 2.f + 300.f) continue;   // up a tree, on a deck
		FWRHit H;
		H.Instigator = nullptr;
		H.Damage = WRForestHzImpl::Tn(TEXT("bloom.elderCrushDamage"), 9999.f);
		H.Origin = V->GetActorLocation() + FVector(0.f, 0.f, 1000.f);
		H.ImpactPoint = V->GetActorLocation();
		H.bUnblockable = true;
		H.Impact = 3;
		H.PoiseDamage = 999.f;
		H.Tier = EWRHitTier::Heavy;
		H.PhysType = 2;
		UWRCombatLibrary::ApplyHit(V, H);
	}
	SpawnNodes();
	GoneAtHours = WRForestHzImpl::Hours(this) + WRForestHzImpl::Tn(TEXT("bloom.elderLieHours"), 24.f);
	UE_LOG(LogWildreach, Log, TEXT("FOREST Bloom: the Elder Fall at %s is down"), *Base.ToCompactString());
}

void AWRElderFall::SpawnNodes()
{
	// A fresh fallen giant for the day (DC 2.10): 12 heartwood nodes where the amber core shows along the split bole, 6
	// resin nodes weeping from bark wounds on its flanks. They don't grow back: the giant goes with the day.
	const FVector Base = GetActorLocation();
	const FVector Dir = FRotator(0.f, FallYaw, 0.f).Vector();
	const FVector Side(-Dir.Y, Dir.X, 0.f);
	const float Lie = FMath::Sin(FMath::DegreesToRadians(FMath::Min(EndTilt, 90.f)));
	const float Rise = FMath::Cos(FMath::DegreesToRadians(EndTilt));
	const auto OnLog = [&](float T, float Around)
	{
		// A point on the log's skin: T along it (0 foot .. 1 crown), Around the angle from its top.
		const float L = FallLength * T;
		const FVector Axis = Base + Dir * (L * Lie) + FVector(0.f, 0.f, L * Rise);
		const float R = TrunkRadius * (1.f - 0.45f * T);
		return Axis + FVector(0.f, 0.f, R * FMath::Cos(Around)) + Side * (R * FMath::Sin(Around));
	};
	const auto Spawn = [&](FName Item, const FVector& At, int32 Hits, int32 PerHit, bool bAxe)
	{
		const FTransform Tr(FRotator(0.f, FallYaw + 90.f, 0.f), At);
		AWRResourceNode* N = GetWorld()->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!N) return;
		N->ItemId = Item;
		N->Hits = Hits;
		N->PerHit = PerHit;
		N->RespawnSeconds = 1.0e7f;
		N->GemChance = 0.f;
		if (bAxe)
		{
			// The amber core takes an axe (DC 2.7: 8 a hit); the Bloom's heartwood x3 is on this wood too.
			TWeakObjectPtr<AWRResourceNode> Weak(N);
			N->CanHarvest = [](AWRCharacterBase* Who)
			{
				if (WRForestHzImpl::HoldsAxe(Who)) return true;
				if (AWRPlayerController* PC = WRForestHzImpl::ControllerOf(Who)) PC->ClientNotify(TEXT("Heartwood needs an axe: the amber core turns anything else"));
				return false;
			};
			N->YieldScale = [Weak](AWRCharacterBase*)
			{
				return Weak.IsValid() && WRForest::Bloom(Weak.Get()) ? WRTune(TEXT("forest.bloomHeartwood"), 3.f) : 1.f;
			};
		}
		N->FinishSpawning(Tr);
		Nodes.Add(N);
	};
	const int32 Heart = int32(WRForestHzImpl::Tn(TEXT("bloom.elderHeartwood"), 12.f));
	const int32 Resin = int32(WRForestHzImpl::Tn(TEXT("bloom.elderResin"), 6.f));
	const int32 Hits = FMath::Max(1, int32(WRForestHzImpl::Tn(TEXT("bloom.elderNodeHits"), 6.f)));
	for (int32 i = 0; i < Heart; ++i)
	{
		const float T = 0.04f + 0.5f * float(i) / FMath::Max(1, Heart - 1);
		Spawn(TEXT("heartwood"), OnLog(T, i % 2 ? 0.35f : -0.35f), Hits, int32(WRTune(TEXT("forest.heartwoodPerHit"), 8.f)), true);
	}
	for (int32 i = 0; i < Resin; ++i)
	{
		const float T = 0.1f + 0.6f * float(i) / FMath::Max(1, Resin - 1);
		Spawn(TEXT("resin"), OnLog(T, i % 2 ? 1.45f : -1.45f), 3, 2, false);
	}
}

void AWRElderFall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StartAt < 0.f) return;
	if (!bLanded)
	{
		const float Since = WRForestHzImpl::Now(this) - StartAt;
		const float Warn = WRForestHzImpl::Tn(TEXT("bloom.elderFallWarn"), 5.f);
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, FRotator(0.f, FallYaw, 0.f).Vector()).GetSafeNormal();
		Pivot->SetRelativeRotation(FQuat(Axis, FMath::DegreesToRadians(Tilt(Since))));
		if (!bFellHere && Since >= Warn && GetNetMode() != NM_DedicatedServer)
		{
			// The last great tearing crack as it lets go.
			bFellHere = true;
			WRSound::PlayAt(this, TEXT("forest.elder_crack"), GetActorLocation() + FVector(0.f, 0.f, 400.f), 1.3f);
		}
		if (Since >= Warn + FallTable.Num() / 120.f)
		{
			if (GetNetMode() != NM_DedicatedServer)
			{
				// The impact: the ground jumps under you for 300 m, a wall of dust and snapped limbs along its length.
				const FVector Dir = FRotator(0.f, FallYaw, 0.f).Vector();
				const FVector Mid = GetActorLocation() + Dir * (FallLength * 0.55f);
				WRSound::PlayAt(this, TEXT("forest.elder_fall"), Mid);
				for (float D = 1000.f; D < FallLength; D += 1500.f)
				{
					const FVector P = GetActorLocation() + Dir * D;
					WRFx::SpawnAt(this, TEXT("forest.elder_impact"), FVector(P.X, P.Y, WRForestHzImpl::GroundZAt(GetWorld(), P)), FRotator::ZeroRotator, 2.f);
				}
				WRFx::Shake(this, 0.6f, Mid, 30000.f);
			}
			Land();
		}
		return;
	}
	if (HasAuthority() && GoneAtHours > 0.0 && WRForestHzImpl::Hours(this) >= GoneAtHours) Destroy();
}

void AWRElderFall::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
	{
		for (const TWeakObjectPtr<AWRResourceNode>& N : Nodes) if (AWRResourceNode* Node = N.Get()) Node->Destroy();
		Nodes.Reset();
	}
	Super::EndPlay(Reason);
}

// ================================================================================================ UWRForestHazards

UWRForestHazards* UWRForestHazards::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRForestHazards>() : nullptr;
}

bool UWRForestHazards::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRForestHazards::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	if (Map != FWRTuning::Get().Str(TEXT("forestWorld.map"), TEXT("L_Island")) || !WRBiome::IsLive(TEXT("forest"))) return;
	if (!FWRForestData::Get().bLoaded || !WRForestHzImpl::Raw().IsValid()) return;
	bActive = true;
	BuildLocal();
	if (InWorld.GetNetMode() != NM_Client) SpawnServerActors();
	UE_LOG(LogWildreach, Log, TEXT("FOREST hazards up: %d brakes, %d nests, %d limbs, %d pits, %d rivers"), Brakes.Num(), Nests.Num(), Limbs.Num(), Pits.Num(), Waters.Num());
}

AWRThornbrake* UWRForestHazards::SpawnBrake(const FWRForestData::FBrake& B, float GapWidth)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return nullptr;
	const FTransform Tr(FRotator(0.f, B.Yaw, 0.f), B.Centre);
	AWRThornbrake* T = World->SpawnActorDeferred<AWRThornbrake>(AWRThornbrake::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!T) return nullptr;
	T->bRing = B.bRing;
	T->Radius = B.Radius;
	T->Depth = B.Depth;
	T->Length = B.Length;
	T->Height = B.Height;
	T->Guards = B.Guards;
	if (GapWidth >= 0.f)
	{
		T->bGapDecided = true;
		if (GapWidth > 1.f)
		{
			// The natural way in: a ring's toward the nearest way (the generator's yaw), a wall's at its middle.
			FWRBrakeGap G;
			G.At = B.bRing ? B.Centre + FRotator(0.f, B.Yaw, 0.f).Vector() * (B.Radius + B.Depth * 0.5f) : B.Centre;
			G.HalfWidth = GapWidth * 0.5f;
			G.OpenUntil = 1.0e9f;
			T->Gaps.Add(G);
		}
	}
	T->FinishSpawning(Tr);
	Brakes.Add(T);
	// The grid (50 m cells) the pawn step looks brakes up in.
	const float Ext = B.bRing ? B.Radius + B.Depth : FMath::Max(B.Length, B.Depth) * 0.5f;
	const int32 X0 = FMath::FloorToInt((B.Centre.X - Ext) / 5000.f), X1 = FMath::FloorToInt((B.Centre.X + Ext) / 5000.f);
	const int32 Y0 = FMath::FloorToInt((B.Centre.Y - Ext) / 5000.f), Y1 = FMath::FloorToInt((B.Centre.Y + Ext) / 5000.f);
	for (int32 x = X0; x <= X1; ++x) for (int32 y = Y0; y <= Y1; ++y) BrakeGrid.FindOrAdd(FIntPoint(x, y)).Add(Brakes.Num() - 1);
	return T;
}

void UWRForestHazards::BrakesNear(const FVector& At, float Range, TArray<AWRThornbrake*>& Out) const
{
	const int32 X0 = FMath::FloorToInt((At.X - Range) / 5000.f), X1 = FMath::FloorToInt((At.X + Range) / 5000.f);
	const int32 Y0 = FMath::FloorToInt((At.Y - Range) / 5000.f), Y1 = FMath::FloorToInt((At.Y + Range) / 5000.f);
	for (int32 x = X0; x <= X1; ++x)
	{
		for (int32 y = Y0; y <= Y1; ++y)
		{
			const TArray<int32>* Cell = BrakeGrid.Find(FIntPoint(x, y));
			if (!Cell) continue;
			for (const int32 i : *Cell)
			{
				AWRThornbrake* T = Brakes.IsValidIndex(i) ? Brakes[i].Get() : nullptr;
				if (T) Out.AddUnique(T);
			}
		}
	}
}

AWRThornbrake* UWRForestHazards::BrakeAt(const FVector& Feet) const
{
	TArray<AWRThornbrake*> Near;
	BrakesNear(Feet, 100.f, Near);
	for (AWRThornbrake* T : Near) if (T->Contains(Feet, 0.f)) return T;
	return nullptr;
}

bool UWRForestHazards::InRiver(const AActor* Who) const
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	if (!C) return false;
	const FVector F = WRForestHzImpl::Feet(C);
	for (const TWeakObjectPtr<AWRForestWater>& W : Waters)
	{
		float Z = 0.f;
		FVector Down;
		if (W.IsValid() && W->SurfaceAt(F, Z, Down) && F.Z < Z + 10.f && F.Z > Z - 800.f) return true;
	}
	return false;
}

void UWRForestHazards::AddGuardedCache(AWRWorldCache* Cache, FName Guards)
{
	if (!Cache) return;
	FGuarded& G = Guarded.AddDefaulted_GetRef();
	G.Cache = Cache;
	G.Guards = Guards;
	G.bWasTaken = Cache->bTaken;
}

void UWRForestHazards::BuildLocal()
{
	UWorld* World = GetWorld();
	const FWRForestData& D = FWRForestData::Get();
	// The rivers, streams and the mill race (every machine: their swim volumes are local, like the sea's). The Drowned
	// Root's pool is the Hollow Root's (WRForestHollow).
	for (const FWRForestData::FPath& R : D.Rivers)
	{
		if (R.Points.Num() < 2 || R.Kind == TEXT("pool")) continue;
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWRForestWater* W = World->SpawnActor<AWRForestWater>(AWRForestWater::StaticClass(), FTransform(R.Points[0]), SP);
		if (!W) continue;
		W->RiverId = R.Id;
		W->Kind = R.Kind;
		W->Points = R.Points;
		W->Widths = R.Widths;
		W->Build();
		Waters.Add(W);
	}
	// The Grovekin pillar on the Crown's Ring (only where the Ring stands: Storm peaks live; a look, so not on a
	// dedicated server).
	if (World->GetNetMode() != NM_DedicatedServer && WRBiome::IsLive(TEXT("storm_peaks")))
	{
		const FWRStormData& SD = FWRStormData::Get();
		const FWRStormData::FSite* Ring = nullptr;
		for (const FWRStormData::FSite& S : SD.Sites)
		{
			if (S.Id == TEXT("ring_terrace") || S.Kind == TEXT("ring_west")) { Ring = &S; break; }
		}
		if (Ring)
		{
			// The forest's people stand on the Ring's south-east arc (their land lies south-east of the Crown), out from
			// the terrace's centre stone toward home.
			const FVector At = Ring->At + FVector(WRForestHzImpl::Tn(TEXT("bloom.pillarOffsetX"), 1400.f), WRForestHzImpl::Tn(TEXT("bloom.pillarOffsetY"), 1100.f), 0.f);
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Pillar = World->SpawnActor<AWRForestRingPillar>(AWRForestRingPillar::StaticClass(), FTransform(At), SP);
			if (Pillar)
			{
				FVector G = At;
				if (!WRForestHzImpl::Ground(World, At + FVector(0.f, 0.f, 3000.f), G, 0.f, 8000.f)) G = At;
				Pillar->Build(G);
			}
		}
	}
	WRForestHollow::BuildLocal(World);
}

void UWRForestHazards::SpawnServerActors()
{
	UWorld* World = GetWorld();
	const FWRForestData& D = FWRForestData::Get();
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		State = World->SpawnActor<AWRForestHazardState>(AWRForestHazardState::StaticClass(), FTransform::Identity, SP);
	}

	// Thornbrakes, from the generator's rows (with their gaps: forest.json "gap" metres, toward the nearest way).
	if (const TArray<TSharedPtr<FJsonValue>>* Rows = WRForestHzImpl::RawArray(TEXT("brakes")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Rows)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid()) continue;
			FWRForestData::FBrake B;
			B.Centre = WRForestHzImpl::RawAt(O);
			B.bRing = WRForestHzImpl::RawBool(O, TEXT("ring"), true);
			B.Radius = WRForestHzImpl::RawNum(O, TEXT("r"), 0.f) * 100.f;
			B.Depth = WRForestHzImpl::RawNum(O, TEXT("depth"), 6.f) * 100.f;
			B.Length = WRForestHzImpl::RawNum(O, TEXT("length"), 0.f) * 100.f;
			B.Yaw = WRForestHzImpl::RawNum(O, TEXT("yaw"), 0.f);
			B.Height = WRForestHzImpl::RawNum(O, TEXT("h"), 3.f) * 100.f;
			B.Guards = FName(*WRForestHzImpl::RawStr(O, TEXT("guards")));
			SpawnBrake(B, WRForestHzImpl::RawNum(O, TEXT("gap"), 0.f) * 100.f);
		}
	}

	// Caches behind thornbrake (DC 2.4 kit, 7 in F4/F5; Rowan's "Through the briars"): the generator's rows (kit
	// thornbrake_cache, each inside a closed ring that guards it), and up to seven the deepest closed giant rings of the
	// deep wood keep one in the buttress hollow at the giant's foot, behind 7-9 m of bramble.
	{
		struct FLoot { FString Title; TMap<FName, int32> Items; };
		TArray<FLoot> Tables;
		{
			FLoot& L0 = Tables.AddDefaulted_GetRef();
			L0.Title = TEXT("A Grovekin forager's cache");
			L0.Items.Add(TEXT("resin"), 4); L0.Items.Add(TEXT("heartwood"), 6); L0.Items.Add(TEXT("iron_ore"), 2);
			FLoot& L1 = Tables.AddDefaulted_GetRef();
			L1.Title = TEXT("A hunter's cache in the roots");
			L1.Items.Add(TEXT("hide"), 4); L1.Items.Add(TEXT("bone"), 3); L1.Items.Add(TEXT("gem_topaz"), 1);
			FLoot& L2 = Tables.AddDefaulted_GetRef();
			L2.Title = TEXT("An old cache under the briars");
			L2.Items.Add(TEXT("heartwood"), 4); L2.Items.Add(TEXT("resin"), 3); L2.Items.Add(TEXT("gem_onyx"), 1);
		}
		TArray<FVector> Placed;
		const auto PlaceCache = [&](const FVector& At, float Yaw, int32 Variant, FName Guards)
		{
			const FTransform Tr(FRotator(0.f, Yaw, 0.f), At + FVector(0.f, 0.f, 30.f));
			AWRWorldCache* C = World->SpawnActorDeferred<AWRWorldCache>(AWRWorldCache::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!C) return;
			const FLoot& L = Tables[FMath::Abs(Variant) % Tables.Num()];
			C->Kind = TEXT("chest");
			C->Biome = TEXT("forest");
			C->Title = L.Title;
			C->Items = L.Items;
			C->RefillHours = WRForestHzImpl::Tn(TEXT("thornbrake.cacheRefillHours"), 72.f);
			C->FinishSpawning(Tr);
			AddGuardedCache(C, Guards);
			Placed.Add(At);
		};
		for (const FWRForestData::FSite& K : D.Kit)
		{
			if (K.Kind == TEXT("thornbrake_cache")) PlaceCache(K.At, K.Yaw, K.Variant, K.Id);
		}
		const int32 Want = int32(WRForestHzImpl::Tn(TEXT("thornbrake.cachesWanted"), 7.f));
		if (Placed.Num() < Want)
		{
			// The design's seven (DC 2.4): the generator's, topped up from the deepest closed rings.
			const float Spread = WRForestHzImpl::Tn(TEXT("thornbrake.cacheSpread"), 30000.f);
			const float MinDepth = WRForestHzImpl::Tn(TEXT("thornbrake.cacheMinDepth"), 700.f);
			TArray<AWRThornbrake*> Candidates;
			for (const TWeakObjectPtr<AWRThornbrake>& W : Brakes)
			{
				AWRThornbrake* T = W.Get();
				if (!T || !T->bRing || T->Gaps.Num() > 0 || !T->Guards.IsNone() || T->Depth < MinDepth) continue;
				const TCHAR Z = WRForest::ZoneAt(World, T->GetActorLocation());
				if (Z == TEXT('4') || Z == TEXT('5')) Candidates.Add(T);
			}
			Candidates.Sort([](const AWRThornbrake& A, const AWRThornbrake& B) { return A.Depth > B.Depth; });
			for (AWRThornbrake* T : Candidates)
			{
				if (Placed.Num() >= Want) break;
				const FVector C = T->GetActorLocation();
				bool bFar = true;
				for (const FVector& P : Placed) bFar &= FVector::Dist2D(P, C) > Spread;
				if (!bFar) continue;
				// In the hollow between the trunk and the ring's inner edge (the generator's ring starts at 1.6 x the
				// trunk's radius + 1.5 m), on the side its yaw faces (toward the nearest way).
				const float TrunkR = FMath::Max(100.f, (T->Radius - 150.f) / 1.6f);
				const FVector Dir = FRotator(0.f, T->GetActorRotation().Yaw, 0.f).Vector();
				FVector At = C + Dir * ((TrunkR + T->Radius) * 0.5f);
				At.Z = WRForestHzImpl::GroundZAt(World, At);
				T->Guards = FName(*FString::Printf(TEXT("briar_cache_%d"), Placed.Num()));
				PlaceCache(At, T->GetActorRotation().Yaw + 180.f, Placed.Num(), T->Guards);
			}
		}
	}

	// Point hazards (forest.json spots, metres).
	if (const TArray<TSharedPtr<FJsonValue>>* Rows = WRForestHzImpl::RawArray(TEXT("spots")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Rows)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid()) continue;
			const FString Kind = WRForestHzImpl::RawStr(O, TEXT("kind"));
			const FVector At = WRForestHzImpl::RawAt(O);
			const float R = WRForestHzImpl::RawNum(O, TEXT("r"), 0.f) * 100.f;
			const float Depth = WRForestHzImpl::RawNum(O, TEXT("depth"), 0.f) * 100.f;
			const float Yaw = WRForestHzImpl::RawNum(O, TEXT("yaw"), 0.f);
			const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
			if (Kind == TEXT("widowmaker"))
			{
				// A dead limb in a giant's crown over an F4 path: it falls on the path below.
				AWRWidowmaker* L = World->SpawnActorDeferred<AWRWidowmaker>(AWRWidowmaker::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!L) continue;
				L->GroundZ = WRForestHzImpl::GroundZAt(World, At - FVector(0.f, 0.f, 500.f));
				L->FallRadius = R > 0.f ? R : WRForestHzImpl::Tn(TEXT("widowmaker.radius"), 200.f);
				L->FinishSpawning(Tr);
				Limbs.Add(L);
			}
			else if (Kind == TEXT("leaf_pit"))
			{
				AWRLeafPit* P = World->SpawnActorDeferred<AWRLeafPit>(AWRLeafPit::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!P) continue;
				P->Radius = FMath::Max(150.f, R);
				P->Depth = FMath::Max(300.f, Depth);
				P->FinishSpawning(Tr);
				Pits.Add(P);
			}
			else if (Kind == TEXT("wasp_nest"))
			{
				// On a bough (from the nearest giant's trunk within 12 m), or with nothing near, from a dead snag of its own;
				// the Wasp Ravine's (depth 1) glued under the overhang.
				AWRWaspNest* N = World->SpawnActorDeferred<AWRWaspNest>(AWRWaspNest::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!N) continue;
				N->bOverhang = Depth > 0.f;
				if (!N->bOverhang)
				{
					const FWRForestData::FTree* Best = nullptr;
					float BestD = 1200.f;
					for (const FWRForestData::FTree& G : D.Giants)
					{
						const float Dd = FVector::Dist2D(G.Base, At) - G.Radius;
						if (Dd < BestD) { BestD = Dd; Best = &G; }
					}
					if (Best)
					{
						const FVector Out = (At - Best->Base).GetSafeNormal2D();
						N->BranchFrom = FVector(Best->Base.X, Best->Base.Y, At.Z + 120.f) + Out * (Best->Radius * 0.7f);
					}
					else
					{
						N->BranchFrom = FVector(At.X - 180.f, At.Y, WRForestHzImpl::GroundZAt(World, At));
					}
				}
				N->FinishSpawning(Tr);
				Nests.Add(N);
			}
		}
	}
	WRForestHollow::SpawnServer(World);
}

void UWRForestHazards::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!bActive || !World) return;
	if (World->GetNetMode() != NM_Client)
	{
		StepPawns(DeltaTime);
		StepSmoke(DeltaTime);
		StepSpate(DeltaTime);
		StepBloom(DeltaTime);
		StepCaches(DeltaTime);
		WRForestHollow::StepServer(World, DeltaTime);
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		StepWarningLook(DeltaTime);
		StepRainLook(DeltaTime);
	}
}

void UWRForestHazards::StepPawns(float Dt)
{
	PawnClock += Dt;
	if (PawnClock < 0.1f) return;
	const float Step = PawnClock;
	PawnClock = 0.f;
	UWorld* World = GetWorld();
	const double Now = WRForestHzImpl::Now(World);
	const float FirstTick = WRForestHzImpl::Tn(TEXT("thornbrake.firstTickDelay"), 0.6f);
	const float Bleed = WRForestHzImpl::Tn(TEXT("thornbrake.bleedPerSecond"), 6.f);
	const float TorchReach = WRForestHzImpl::Tn(TEXT("thornbrake.torchReach"), 250.f);
	const FName Balm(*FWRTuning::Get().Str(TEXT("hazards.thornbrake.balmBuff"), TEXT("briar_balm")));
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!WRForestHzImpl::IsPlayer(C)) continue;   // every forest creature ignores them (the CR rule)
		FPawnState& S = Pawns.FindOrAdd(C);
		const FVector F = WRForestHzImpl::Feet(C);
		if (!C->IsAlive() || !WRForest::Contains(F))
		{
			// Death (or leaving the forest) mid-bramble: the slow and the arms-up end with it.
			if (S.bTagged) { WRForestHzImpl::SetTag(C, WRForestTags::Hazard_Thornbrake, false); S.bTagged = false; }
			S.Contact = -1.f;
			S.Torch = 0.f;
			continue;
		}
		TArray<AWRThornbrake*> Near;
		BrakesNear(F, TorchReach + 200.f, Near);
		AWRThornbrake* In = nullptr;
		for (AWRThornbrake* T : Near)
		{
			if (T->Contains(F, 0.f)) { In = T; break; }
		}

		// A torch held to the canes (standing in them, or within arm's reach facing them) for 2 s: a 4 m lane burns open.
		bool bTorching = false;
		if (Near.Num() > 0 && UWRSurvivalComponent::HoldsTorch(C))
		{
			AWRThornbrake* Target = In;
			FVector Mid = F;
			if (!Target)
			{
				float BestD = TorchReach;
				const FVector Reach = F + C->GetActorForwardVector().GetSafeNormal2D() * 100.f;
				for (AWRThornbrake* T : Near)
				{
					float Dd = 0.f;
					const FVector M = T->NearestMid(Reach, Dd);
					if (Dd <= BestD && T->OpenAt(M) < 0.5f) { BestD = Dd; Target = T; Mid = Reach; }
				}
			}
			if (Target && Target->OpenAt(Mid) < 0.5f)
			{
				bTorching = true;
				S.Torch += Step;
				if (S.Torch >= WRForestHzImpl::Tn(TEXT("thornbrake.torchSeconds"), 2.f))
				{
					S.Torch = 0.f;
					// Burns last half as long in the rain (cooks/biome-forest.md 2.9).
					const float Seconds = WRForestHzImpl::HoursToSeconds(WRForestHzImpl::Tn(TEXT("thornbrake.torchGapHours"), 5.f))
						* (WRForest::Raining(World, F) ? WRForestHzImpl::Tn(TEXT("thornbrake.rainBurn"), 0.5f) : 1.f);
					Target->Burn(Mid, WRForestHzImpl::Tn(TEXT("thornbrake.torchGapRadius"), 400.f) * 0.5f, Seconds);
				}
			}
		}
		if (!bTorching) S.Torch = 0.f;

		const bool bHeld = In && In->OpenAt(F) < 0.5f && !WRForest::PassesThornbrake(C);
		if (bHeld)
		{
			if (S.Contact < 0.f)
			{
				// Contact: the dry crackle and scrape at once (the tell); the first tick is 0.6 s away, so a step back is free.
				S.Contact = float(Now);
				In->MulticastScrape(C);
				S.ScrapeClock = WRForestHzImpl::Tn(TEXT("thornbrake.scrapeEvery"), 1.2f);
			}
			if (Now - S.Contact >= FirstTick)
			{
				if (!S.bTagged) { WRForestHzImpl::SetTag(C, WRForestTags::Hazard_Thornbrake, true); S.bTagged = true; }
				// 6 bleed buildup a second (briar balm x0.3). The proc is combat's bleed burst: it can kill.
				const float Scale = WRStorm::HasBuff(C, Balm) ? WRForestHzImpl::Tn(TEXT("thornbrake.balmMultiplier"), 0.3f) : 1.f;
				if (C->Status) C->Status->AddStatusBuildup(WRStatus::Bleed, Bleed * Scale * Step, nullptr);
				S.ScrapeClock -= Step;
				if (S.ScrapeClock <= 0.f && WRForestHzImpl::Speed2D(C) > 50.f)
				{
					S.ScrapeClock = WRForestHzImpl::Tn(TEXT("thornbrake.scrapeEvery"), 1.2f);
					In->MulticastScrape(C);
				}
			}
			S.LastIn = Now;
		}
		else
		{
			if (S.bTagged) { WRForestHzImpl::SetTag(C, WRForestTags::Hazard_Thornbrake, false); S.bTagged = false; }
			S.Contact = -1.f;
		}

		// The scout's mark comes off in water (wading or swimming) and in smoke (DC 2.8, plan 2.15).
		if (WRForest::HasWaspMark(C))
		{
			const UCharacterMovementComponent* Move = C->GetCharacterMovement();
			const bool bWater = (Move && Move->IsSwimming()) || InRiver(C);
			if (bWater || WRForest::InSmoke(World, C->GetActorLocation()))
			{
				WRForest::ClearWaspMark(C);
				if (AWRPlayerController* PC = WRForestHzImpl::ControllerOf(C)) PC->ClientNotify(bWater ? TEXT("The water washes the wasps' mark off you") : TEXT("The smoke hides the wasps' mark on you"));
			}
		}
		S.Last = F;
	}
	// Forget the gone.
	for (auto It = Pawns.CreateIterator(); It; ++It) if (!It->Key.IsValid()) It.RemoveCurrent();
}

void UWRForestHazards::StepSmoke(float Dt)
{
	SmokeClock += Dt;
	if (SmokeClock < 1.f || !State) return;
	SmokeClock = 0.f;
	const float T = WRForestHzImpl::Now(GetWorld());
	const int32 Was = State->Smoke.Num();
	State->Smoke.RemoveAll([T](const FWRForestSmoke& S) { return T >= S.Until; });
	if (State->Smoke.Num() != Was) State->ForceNetUpdate();
}

void UWRForestHazards::StepSpate(float Dt)
{
	SpateClock += Dt;
	if (SpateClock < 1.f || !State) return;
	SpateClock = 0.f;
	UWorld* World = GetWorld();
	const double Now = WRForestHzImpl::Now(World);
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(World, TEXT("forest"));
	const bool bRain = K && K->Id == TEXT("rain");
	if (bRain && !bWasRaining) RainStartedAt = Now;
	if (!bRain && bWasRaining) RainEndedAt = Now;
	bWasRaining = bRain;
	// The Alder rises a few minutes into the rain and stays high for the block after it (cooks/biome-forest.md 2.8:
	// "use the Root Bridge or the mill race weir; wait a block").
	const bool bSpate = (bRain && Now - RainStartedAt > WRForestHzImpl::Tn(TEXT("spate.riseMinutes"), 3.f) * 60.0)
		|| Now - RainEndedAt < WRForestHzImpl::Tn(TEXT("spate.afterRainMinutes"), 30.f) * 60.0;
	if (bSpate != State->bSpate)
	{
		State->bSpate = bSpate;
		State->ForceNetUpdate();
		UE_LOG(LogWildreach, Log, TEXT("FOREST the Alder %s"), bSpate ? TEXT("is in spate") : TEXT("falls back"));
	}
}

void UWRForestHazards::DevSpate()
{
	RainEndedAt = WRForestHzImpl::Now(GetWorld());
	SpateClock = 1.f;
	StepSpate(0.f);
}

bool UWRForestHazards::FarFromSafe(const FVector& At, float Clear) const
{
	UWorld* World = GetWorld();
	const FWRForestData& D = FWRForestData::Get();
	for (TActorIterator<AWRSafeZone> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < It->Radius + Clear) return false;
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < It->Radius + Clear) return false;
	for (TActorIterator<AWRShrine> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < Clear) return false;
	for (const TPair<FName, FWRForestData::FArena>& A : D.Arenas) if (FVector::Dist2D(A.Value.Centre, At) < A.Value.Radius + Clear) return false;
	for (const FWRForestData::FSite& S : D.Sites)
	{
		if ((S.Kind == TEXT("camp") || S.Kind == TEXT("village")) && FVector::Dist2D(S.At, At) < Clear + 4000.f) return false;
	}
	if (!D.Village.IsNearlyZero() && FVector::Dist2D(D.Village, At) < D.VillageRadius + Clear) return false;
	return true;
}

bool UWRForestHazards::PickElderSpot(FVector& OutBase, float& OutYaw) const
{
	UWorld* World = GetWorld();
	const FWRForestData& D = FWRForestData::Get();
	if (D.Giants.Num() == 0) return false;
	const float Len = WRForestHzImpl::Tn(TEXT("bloom.elderFallLength"), 9000.f);
	const float Clear = WRForestHzImpl::Tn(TEXT("bloom.elderClear"), 15000.f);
	const float Unseen = WRForestHzImpl::Tn(TEXT("bloom.elderUnseen"), 12000.f);
	// A dead giant somewhere in the old growth (F4/F5), standing in a gap 25-45 m off a living giant, its fall line clear
	// of the other trunks, 150 m from every camp, the village, claims, shrines and arenas (DC 2.10), not over the Hollow
	// Root, and away from everyone close by (it was always there: nobody should see it appear).
	for (int32 Try = 0; Try < 400; ++Try)
	{
		const FWRForestData::FTree& G = D.Giants[FMath::RandRange(0, D.Giants.Num() - 1)];
		if (G.Kind != TEXT("giant")) continue;
		const FVector Base2 = G.Base + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(2500.f, 4500.f);
		const TCHAR Z = WRForest::ZoneAt(World, Base2);
		if (Z != TEXT('4') && Z != TEXT('5')) continue;
		if (WRForestHollowContains(Base2) || !FarFromSafe(Base2, Clear)) continue;
		const float Yaw = FMath::FRandRange(0.f, 360.f);
		const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
		const FVector Tip = Base2 + Dir * Len;
		if (!WRForest::Contains(Tip) || !FarFromSafe(Tip, Clear) || !FarFromSafe(Base2 + Dir * (Len * 0.5f), Clear)) continue;
		bool bClear = true;
		for (const FWRForestData::FTree& O : D.Giants)
		{
			if (FVector::Dist2D(O.Base, Base2) > Len + 3000.f) continue;
			if (FVector::Dist2D(O.Base, Base2) < O.Radius + 800.f) { bClear = false; break; }
			const FVector2D C = FMath::ClosestPointOnSegment2D(FVector2D(O.Base), FVector2D(Base2), FVector2D(Tip));
			if (FVector2D::Distance(C, FVector2D(O.Base)) < O.Radius + 500.f) { bClear = false; break; }
		}
		if (!bClear) continue;
		bool bSeen = false;
		for (TActorIterator<AWRCharacterBase> It(World); It && !bSeen; ++It)
		{
			bSeen = WRForestHzImpl::IsPlayer(*It) && FVector::Dist2D(It->GetActorLocation(), Base2) < Unseen;
		}
		if (bSeen) continue;
		OutBase = FVector(Base2.X, Base2.Y, WRForestHzImpl::GroundZAt(World, Base2));
		OutYaw = Yaw;
		return true;
	}
	return false;
}

void UWRForestHazards::SpawnElderFalls()
{
	UWorld* World = GetWorld();
	const int32 Count = int32(WRForestHzImpl::Tn(TEXT("bloom.elderFalls"), 3.f));
	Elders.Reset();
	ElderTimes.Reset();
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Base;
		float Yaw = 0.f;
		if (!PickElderSpot(Base, Yaw)) continue;
		const FTransform Tr(FRotator::ZeroRotator, Base);
		AWRElderFall* E = World->SpawnActorDeferred<AWRElderFall>(AWRElderFall::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!E) continue;
		E->FallYaw = Yaw;
		E->FallLength = WRForestHzImpl::Tn(TEXT("bloom.elderFallLength"), 9000.f);
		E->FinishSpawning(Tr);
		Elders.Add(E);
		// Spread through the day: one in each third, never in the first minute or the last ten.
		const float Span = FMath::Max(600.f, BloomEndAt - BloomStartAt - 660.f);
		ElderTimes.Add(BloomStartAt + 60.f + Span * (float(i) + FMath::FRandRange(0.15f, 0.85f)) / FMath::Max(1, Count));
	}
	UE_LOG(LogWildreach, Log, TEXT("FOREST Bloom: %d Elder Falls stand ready"), Elders.Num());
}

void UWRForestHazards::DevElderFall()
{
	UWorld* World = GetWorld();
	FVector Base;
	float Yaw = 0.f;
	if (!World || !PickElderSpot(Base, Yaw)) return;
	const FTransform Tr(FRotator::ZeroRotator, Base);
	AWRElderFall* E = World->SpawnActorDeferred<AWRElderFall>(AWRElderFall::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E) return;
	E->FallYaw = Yaw;
	E->FallLength = WRForestHzImpl::Tn(TEXT("bloom.elderFallLength"), 9000.f);
	E->FinishSpawning(Tr);
	E->Begin();
	UE_LOG(LogWildreach, Log, TEXT("FOREST dev: an Elder Fall at %s"), *Base.ToCompactString());
}

void UWRForestHazards::StepBloom(float Dt)
{
	BloomClock += Dt;
	if (BloomClock < 0.5f) return;
	BloomClock = 0.f;
	UWorld* World = GetWorld();
	const AWRWorldState* WS = AWRWorldState::Get(World);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("bloom")) : nullptr;
	const float Now = WRForestHzImpl::Now(World);
	const bool bOn = E && E->Stage == 1 && Now < E->EndAt;
	if (E && E->Stage == 0 && !bBloomWarned)
	{
		// The warning (its world message is the event's own): the pollen and the pillar are every machine's
		// (StepWarningLook, AWRForestRingPillar), the Hearts' sap the boss track's (HeartsBright).
		bBloomWarned = true;
		UE_LOG(LogWildreach, Log, TEXT("FOREST Bloom warning: %.0f s"), E->StartAt - Now);
	}
	if (bOn && !bBloomOn)
	{
		// It starts: the day's Elder Falls stand ready, wild nodes grow back twice as fast.
		bBloomOn = true;
		bRaidDone = false;
		BloomStartAt = E->StartAt;
		BloomEndAt = E->EndAt;
		SpawnElderFalls();
		const float Mult = WRForestHzImpl::Tn(TEXT("bloom.nodeRespawnMult"), 0.5f);
		for (TActorIterator<AWRResourceNode> It(World); It; ++It)
		{
			if (!WRForest::Contains(It->GetActorLocation()) || It->RespawnSeconds > 1.0e6f) continue;
			BloomRespawn.Add(*It, It->RespawnSeconds);
			It->RespawnSeconds *= Mult;
		}
		RaidDay = -1;
	}
	if (!bOn && bBloomOn)
	{
		// It's over: the nodes back to their own pace, the Hunt called off. Fallen elders lie their day out (AWRElderFall);
		// ones whose turn never came go quietly.
		bBloomOn = false;
		bBloomWarned = false;
		for (const TPair<TWeakObjectPtr<AWRResourceNode>, float>& R : BloomRespawn) if (AWRResourceNode* N = R.Key.Get()) N->RespawnSeconds = R.Value;
		BloomRespawn.Reset();
		for (const TWeakObjectPtr<AWRElderFall>& W : Elders) if (AWRElderFall* El = W.Get(); El && !El->HasBegun()) El->Destroy();
		Elders.Reset();
		ElderTimes.Reset();
		if (bHuntOn) { WRForest::StartWildHunt(World, false); bHuntOn = false; }
		return;
	}
	if (!bBloomOn) return;
	// The Elder Falls, on their times.
	for (int32 i = 0; i < Elders.Num(); ++i)
	{
		AWRElderFall* El = Elders[i].Get();
		if (El && !El->HasBegun() && ElderTimes.IsValidIndex(i) && Now >= ElderTimes[i]) El->Begin();
	}
	const float Hour = AWRGameState::HourIn(World);
	const AWRGameState* GS = AWRGameState::Get(World);
	const int32 Day = GS ? GS->GetDay() : 0;
	// The noon raid (DC 2.10): two swarms of 12 from the Paper Hollow to the nearest beehive trees and player beehives
	// within 1.5 km; the Hivebacks there heat-ball them (WRForestCreatures). Never at a hive inside a safe zone.
	const float RaidHour = WRForestHzImpl::Tn(TEXT("bloom.waspRaidHour"), 12.f);
	if (!bRaidDone && RaidDay != Day && Hour >= RaidHour && Hour < RaidHour + 1.f)
	{
		RaidDay = Day;
		bRaidDone = true;
		const FWRForestData& D = FWRForestData::Get();
		if (const FWRForestData::FSite* Hollow = D.FindSite(TEXT("paper_hollow")))
		{
			const FVector From = Hollow->At + FVector(0.f, 0.f, 1500.f);
			const float Range = WRForestHzImpl::Tn(TEXT("bloom.waspRaidRadius"), 150000.f);
			TArray<FVector> Hives;
			for (const FWRForestData::FSite& K : D.Kit)
			{
				if (K.Kind == TEXT("beehive_tree") && FVector::Dist2D(K.At, From) < Range) Hives.Add(K.At + FVector(0.f, 0.f, 400.f));
			}
			for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
			{
				const FWRPieceDef* Def = It->GetDef();
				if (!Def || Def->Kind != TEXT("hive") || FVector::Dist2D(It->GetActorLocation(), From) > Range) continue;
				bool bSafe = false;
				for (TActorIterator<AWRSafeZone> Z(World); Z && !bSafe; ++Z) bSafe = FVector::Dist2D(Z->GetActorLocation(), It->GetActorLocation()) < Z->Radius;
				if (!bSafe) Hives.Add(It->GetActorLocation());
			}
			Hives.Sort([From](const FVector& A, const FVector& B) { return FVector::DistSquared2D(A, From) < FVector::DistSquared2D(B, From); });
			const int32 Swarms = FMath::Min(Hives.Num(), int32(WRForestHzImpl::Tn(TEXT("bloom.waspRaidSwarms"), 2.f)));
			for (int32 i = 0; i < Swarms; ++i) WRForest::WaspRaid(World, From, Hives[i], int32(WRForestHzImpl::Tn(TEXT("bloom.waspRaidSize"), 12.f)));
			UE_LOG(LogWildreach, Log, TEXT("FOREST Bloom: the noon raid (%d swarms)"), Swarms);
		}
	}
	// The Wild Hunt runs the Bloom's night (the boss track's Hornwarden leads it; it avoids camps and the village).
	const bool bHuntHours = WRForestHzImpl::HourIn(Hour, WRForestHzImpl::Tn(TEXT("bloom.huntFromHour"), 21.f), WRForestHzImpl::Tn(TEXT("bloom.huntToHour"), 4.f));
	if (bHuntHours != bHuntOn)
	{
		bHuntOn = bHuntHours;
		WRForest::StartWildHunt(World, bHuntOn);
	}
}

void UWRForestHazards::StepCaches(float Dt)
{
	CacheClock += Dt;
	if (CacheClock < 0.5f) return;
	CacheClock = 0.f;
	UWorld* World = GetWorld();
	UWRWorldSystems* WS = UWRWorldSystems::Get(World);
	for (FGuarded& G : Guarded)
	{
		AWRWorldCache* C = G.Cache.Get();
		if (!C) continue;
		if (C->bTaken && !G.bWasTaken)
		{
			// Rowan's "Through the briars": whoever stands at it as it empties crossed the brambles to get there.
			for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
			{
				AWRPlayerState* PS = It->GetPlayerState<AWRPlayerState>();
				if (!PS || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), C->GetActorLocation()) > 600.f) continue;
				if (WS) WS->QuestEvent(PS, TEXT("forest_briar_cache"), G.Guards);
				break;
			}
		}
		G.bWasTaken = C->bTaken;
	}
}

void UWRForestHazards::StepWarningLook(float Dt)
{
	// The Bloom's warning and the Bloom (DC 2.10): every giant of the old growth near you drops a gold-green pollen haze
	// from its crown (#E8E0A0), refreshed while it lasts.
	UWorld* World = GetWorld();
	if (!WRForestHazards::HeartsBright(World)) { bWarnLookOn = false; return; }
	WarnLookClock -= Dt;
	if (bWarnLookOn && WarnLookClock > 0.f) return;
	bWarnLookOn = true;
	const float Every = WRForestHzImpl::Tn(TEXT("bloom.hazeEvery"), 20.f);
	WarnLookClock = Every;
	FVector View;
	if (!WRForestHzImpl::View(World, View) || !WRForest::Contains(View)) return;
	const float Range = WRForestHzImpl::Tn(TEXT("bloom.hazeRange"), 30000.f);
	int32 N = 0;
	for (const FWRForestData::FTree& G : FWRForestData::Get().Giants)
	{
		if (N >= 48 || FVector::Dist2D(G.Base, View) > Range) continue;
		if (G.Kind != TEXT("giant") && G.Kind != TEXT("eldest") && G.Kind != TEXT("heart")) continue;
		const TCHAR Z = WRForest::ZoneAt(World, G.Base);
		if (Z != TEXT('4') && Z != TEXT('5')) continue;
		WRFx::SpawnFor(World, TEXT("forest.pollen_haze"), G.Base + FVector(0.f, 0.f, G.Height * 0.8f), FRotator::ZeroRotator, FMath::Max(1.f, G.Height / 6000.f), Every + 2.f);
		++N;
	}
}

void UWRForestHazards::StepRainLook(float Dt)
{
	// Rain under the old growth (DC 2.9): the open meadows get the rain at once (the weather's own fall); under the F4/F5
	// canopy it arrives as heavy drips off the leaves 20 s later, and keeps dripping 20 s after it stops.
	RainLookClock -= Dt;
	if (RainLookClock > 0.f) return;
	RainLookClock = 1.f;
	UWorld* World = GetWorld();
	FVector View;
	if (!WRForestHzImpl::View(World, View) || !WRForest::Contains(View)) { bWasRainingLook = false; return; }
	const bool bRain = WRForest::Raining(World, View);
	const float T = WRForestHzImpl::Now(World);
	if (bRain && !bWasRainingLook) RainSeenAt = T;
	if (!bRain && bWasRainingLook) RainGoneAt = T;
	bWasRainingLook = bRain;
	const float Delay = WRForestHzImpl::Tn(TEXT("rain.dripDelay"), 20.f);
	const bool bDrips = (bRain && T - RainSeenAt >= Delay) || (!bRain && RainGoneAt > 0.f && T - RainGoneAt < Delay);
	const float Cover = WRForest::Canopy(View);
	if (!bDrips || Cover < 0.5f || WRForestHollowContains(View)) return;
	WRFx::SpawnFor(World, TEXT("forest.canopy_drips"), View + FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, FMath::Clamp(Cover, 0.5f, 1.f), 1.6f);
}
