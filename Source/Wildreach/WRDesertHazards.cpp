// The Desert's hazards, the Long Noon and its thrown things. See WRDesertHazards.h; cooks/biome-desert.md 2.8-2.10,
// cooks/build-biome-desert.md 2.8-2.10. Numbers: tuning hazards.desert.* and desertEvents.longNoon.*.

#include "WRDesertHazards.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRDesertCreatures.h"
#include "WRDesertTypes.h"
#include "WRDesertWorld.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRSunPath.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "WRAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace WRDesertTags
{
	UE_DEFINE_GAMEPLAY_TAG(SoftSand, "Desert.SoftSand");
	UE_DEFINE_GAMEPLAY_TAG(Belt, "Desert.Belt");
	UE_DEFINE_GAMEPLAY_TAG(Eased, "Desert.Eased");
	UE_DEFINE_GAMEPLAY_TAG(Sinking, "Desert.Sinking");
	UE_DEFINE_GAMEPLAY_TAG(Held, "Desert.Held");
	UE_DEFINE_GAMEPLAY_TAG(Pit, "Desert.Pit");
	UE_DEFINE_GAMEPLAY_TAG(Buried, "Desert.Buried");
}

namespace WRDesertHzImpl
{
	float DzTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("hazards.desert.%s"), Key), Fallback); }
	float NoTn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("desertEvents.longNoon.%s"), Key), Fallback); }
	float DzHash(int32 I, int32 Salt) { return FMath::Frac(FMath::Sin(float(I) * 12.9898f + float(Salt) * 78.233f) * 43758.5453f); }
	bool DzLive(const UObject* W) { return W && WRDesert::Live() && UWRDesertWorld::Get(W) && UWRDesertWorld::Get(W)->IsActive(); }
	double DzNow(const UObject* W)
	{
		const UWorld* World = W ? W->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
	void DzTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	void DzQuest(const AActor* Pawn, FName What)
	{
		const APawn* P = Cast<APawn>(Pawn);
		AWRPlayerState* PS = P ? P->GetPlayerState<AWRPlayerState>() : nullptr;
		UWRWorldSystems* WS = Pawn ? Pawn->GetWorld()->GetSubsystem<UWRWorldSystems>() : nullptr;
		if (PS && WS) WS->QuestEvent(PS, TEXT("event"), What, 1);
	}
	void DzHit(AWRCharacterBase* V, AActor* By, float Damage, const FVector& Origin, FGameplayTag Element, const TMap<FName, float>& Buildups = {})
	{
		FWRHit Hit;
		Hit.Instigator = By;
		Hit.Damage = Damage;
		Hit.bUnblockable = true;
		Hit.Origin = Origin;
		Hit.Element = Element;
		for (const TPair<FName, float>& B : Buildups) Hit.Buildups.Add(B.Key, B.Value);
		UWRCombatLibrary::ApplyHit(V, Hit);
	}
	void DzTag(AWRCharacterBase* C, const FGameplayTag& Tag, bool bOn)
	{
		UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr;
		if (ASC && (ASC->GetTagCount(Tag) > 0) != bOn) ASC->SetLooseGameplayTagCount(Tag, bOn ? 1 : 0, EGameplayTagReplicationState::TagOnly);
	}
	bool DzHas(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}
	AWRCreature* DzMount(const AActor* Rider) { return Rider ? Cast<AWRCreature>(Rider->GetAttachParentActor()) : nullptr; }
	bool DzSandstorm(const UObject* W)
	{
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WRDesert::Biome());
		return K && K->Id == TEXT("sandstorm");
	}
	/** Under the open sky of the Desert (not in a cave, the Hourhold or the Lee). */
	bool DzOpen(const FVector& P) { return WRDesert::Contains(P) && !WRDesert::InCave(P) && !WRDesert::InHourhold(P); }
	bool DzInStorm(const UObject* W, const FVector& P) { return DzOpen(P) && (WRDesert::InBelt(P) || DzSandstorm(W)); }
	/** The quicksand's skin: a wet suck, a glossy wobble. */
	const FName FxSuck(TEXT("desert.quicksand_suck"));
	/** A flood wadi's polyline: the nearest point to P (distance along, distance across, the ground there, its heading). */
	bool DzOnFlood(const FWRDesertData::FFlood& F, const FVector& P, float& OutAlong, float& OutAcross, FVector& OutAt, FVector& OutDir)
	{
		float Best = TNumericLimits<float>::Max(), Run = 0.f;
		bool bAny = false;
		for (int32 i = 0; i + 1 < F.Pts.Num(); ++i)
		{
			const FVector A = F.Pts[i], B = F.Pts[i + 1];
			const FVector2D AB(B - A);
			const float Len = AB.Size();
			if (Len < 1.f) continue;
			const float t = FMath::Clamp(FVector2D::DotProduct(FVector2D(P - A), AB) / (Len * Len), 0.f, 1.f);
			const FVector Q = A + (B - A) * t;
			const float D = FVector::Dist2D(P, Q);
			if (D < Best) { Best = D; OutAlong = Run + Len * t; OutAcross = D; OutAt = Q; OutDir = (B - A).GetSafeNormal2D(); bAny = true; }
			Run += Len;
		}
		return bAny;
	}
	float DzFloodLength(const FWRDesertData::FFlood& F)
	{
		float L = 0.f;
		for (int32 i = 0; i + 1 < F.Pts.Num(); ++i) L += FVector::Dist2D(F.Pts[i], F.Pts[i + 1]);
		return L;
	}
	/** Where a distance along a flood wadi lies (and its heading). */
	FVector DzFloodPoint(const FWRDesertData::FFlood& F, float Along, FVector& OutDir)
	{
		float Run = 0.f;
		for (int32 i = 0; i + 1 < F.Pts.Num(); ++i)
		{
			const float Len = FVector::Dist2D(F.Pts[i], F.Pts[i + 1]);
			if (Run + Len >= Along || i + 2 == F.Pts.Num())
			{
				OutDir = (F.Pts[i + 1] - F.Pts[i]).GetSafeNormal2D();
				return FMath::Lerp(F.Pts[i], F.Pts[i + 1], FMath::Clamp((Along - Run) / FMath::Max(1.f, Len), 0.f, 1.f));
			}
			Run += Len;
		}
		OutDir = FVector::ForwardVector;
		return F.Pts.Num() > 0 ? F.Pts[0] : FVector::ZeroVector;
	}
	UStaticMeshComponent* DzShape(AActor* A, const TCHAR* Mesh, const FLinearColor& Tint, const FVector& Rel, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(A);
		M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		M->SetupAttachment(A->GetRootComponent());
		M->SetRelativeLocation(Rel);
		M->SetRelativeRotation(Rot);
		M->SetRelativeScale3D(Scale);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, A);
			MID->SetVectorParameterValue(TEXT("Tint"), Tint);
			M->SetMaterial(0, MID);
		}
		M->RegisterComponent();
		return M;
	}
	AActor* DzLookActor(UWorld* W, const FVector& At)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), P);
		if (!A) return nullptr;
		USceneComponent* Root = NewObject<USceneComponent>(A);
		A->SetRootComponent(Root);
		Root->RegisterComponent();
		A->SetActorLocation(At);
		return A;
	}
}
using namespace WRDesertHzImpl;

// ================================================================================================ the queries

float WRDesertHz::MoveScale(const AWRCharacterBase* Who)
{
	if (!Who || !WRDesert::Live()) return 1.f;
	if (Who->HasTag(WRDesertTags::Held) || Who->HasTag(WRDesertTags::Buried)) return 0.f;
	float S = 1.f;
	const bool bEased = Who->HasTag(WRDesertTags::Eased);
	if (Who->HasTag(WRDesertTags::Belt)) S *= bEased ? DzTn(TEXT("belt.easedRun"), 0.85f) : DzTn(TEXT("belt.run"), 0.7f);
	if (Who->HasTag(WRDesertTags::SoftSand)) S *= 1.f - (1.f - DzTn(TEXT("softSand.run"), 0.85f)) * (bEased ? 0.5f : 1.f);
	if (Who->HasTag(WRDesertTags::Sinking)) S *= DzTn(TEXT("quicksand.run"), 0.3f);
	if (Who->HasTag(WRDesertTags::Pit)) S *= DzTn(TEXT("brine.run"), 0.4f);
	return S;
}

float WRDesertHz::SprintStaminaMult(const AActor* Who)
{
	return DzHas(Who, WRDesertTags::SoftSand) || DzHas(Who, WRDesertTags::Belt) ? DzTn(TEXT("softSand.sprintStamina"), 1.3f) : 1.f;
}

bool WRDesertHz::NoRoll(const AActor* Who) { return DzHas(Who, WRDesertTags::Pit) || DzHas(Who, WRDesertTags::Held) || DzHas(Who, WRDesertTags::Buried); }
bool WRDesertHz::IsHeld(const AActor* Who) { return DzHas(Who, WRDesertTags::Held) || DzHas(Who, WRDesertTags::Buried); }

float WRDesertHz::SinkDepth(const AActor* Who)
{
	if (!Who || !WRDesert::Live()) return 0.f;
	const UWRDesertHazards* H = UWRDesertHazards::Get(Who);
	const float* D = H ? H->ShownSink.Find(Who) : nullptr;
	return D ? *D : 0.f;
}

float WRDesertHz::BeltAmount(const APawn* Me, float& OutVisionCm)
{
	OutVisionCm = 0.f;
	if (!Me || !WRDesert::Live()) return 0.f;
	if (!DzInStorm(Me, Me->GetActorLocation())) return 0.f;
	OutVisionCm = UWRInventoryComponent::GearStatFor(Me, TEXT("beltVision")) > 0.f ? DzTn(TEXT("belt.veilVision"), 6000.f) : DzTn(TEXT("belt.vision"), 2500.f);
	return 1.f;
}

bool WRDesertHz::CompassHidden(const APawn* Me)
{
	float Vision = 0.f;
	if (BeltAmount(Me, Vision) <= 0.f) return false;
	// The Sunwatch lens shows the sun's bearing through the storm (carried); so does a tamed Scarab digger close by.
	if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me); Inv && Inv->CountOf(TEXT("sunwatch_lens")) > 0) return false;
	const FString MyName = Me->GetPlayerState() ? Me->GetPlayerState()->GetPlayerName() : FString();
	const float Reach = DzTn(TEXT("belt.scarabReach"), 1500.f);
	for (const TWeakObjectPtr<UWRDesertBody>& W : UWRDesertBody::Live)
	{
		const AWRCreature* C = W.IsValid() ? Cast<AWRCreature>(W->GetOwner()) : nullptr;
		if (C && C->CreatureId == TEXT("scarab_digger") && C->IsTamed() && C->GetTameOwner() == MyName && FVector::Dist(C->GetActorLocation(), Me->GetActorLocation()) < Reach) return false;
	}
	return true;
}

bool WRDesertHz::LongNoon(const UObject* WorldContext)
{
	const AWRDesertState* S = AWRDesertState::Get(WorldContext);
	return S && S->bLongNoon;
}

float WRDesertHz::SkyHour(const UObject* WorldContext, float Hour, float& InOutDaylight)
{
	static float Share = 0.f;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AWRDesertState* S = World && WRDesert::Live() ? AWRDesertState::Get(World) : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const FVector View = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const bool bHeld = S && (S->NoonStage == 2 || S->NoonStage == 3);
	// Blended over the border (the camera's side of it, eased), so walking out of the Desert lets the clock back in.
	Share = FMath::FInterpConstantTo(Share, bHeld && WRDesert::Contains(View) ? 1.f : 0.f, World ? World->GetDeltaSeconds() : 0.f, 0.5f);
	if (Share <= 0.f || !S) return Hour;
	float K = 0.f;   // 0 = noon held, 1 = back to the clock
	if (S->NoonStage == 3) K = FMath::Clamp(float(DzNow(World) - S->NoonStageAt) / NoTn(TEXT("duskSeconds"), 120.f), 0.f, 1.f);
	const float Ahead = FMath::Fmod(Hour - 12.f + 24.f, 24.f);   // the clock's hour, counted on from noon
	const float Held = FMath::Fmod(12.f + Ahead * K, 24.f);
	const float Step = FMath::Fmod(Held - Hour + 36.f, 24.f) - 12.f;   // the short way round the clock
	const float Out = FMath::Fmod(Hour + Step * Share + 24.f, 24.f);
	InOutDaylight = FMath::Lerp(InOutDaylight, FMath::Lerp(1.f, InOutDaylight, K), Share);
	return Out;
}

float WRDesertHz::TameSpeedMult(const AWRCreature* Creature)
{
	if (!Creature || !WRDesert::Live() || Creature->CreatureId != TEXT("gildback")) return 1.f;
	return LongNoon(Creature) ? NoTn(TEXT("gildbackTame"), 2.f) : 1.f;
}

bool WRDesertHz::WetSandAt(const UObject* WorldContext, const FVector& P)
{
	const UWRDesertHazards* H = UWRDesertHazards::Get(WorldContext);
	if (!H) return false;
	const double T = DzNow(WorldContext);
	for (const UWRDesertHazards::FWet& W : H->WetSand) if (T < W.Until && FVector::Dist2D(W.At, P) < W.R) return true;
	return false;
}

bool WRDesertHz::DevilAt(const UObject* WorldContext, int32 Index, double T, FVector& Out)
{
	const FWRDesertData& D = FWRDesertData::Get();
	if (!D.bLoaded || D.DevilPoly.Num() < 3 || Index < 0 || Index >= 4) return false;
	const float Hour = WRDesertHz::LongNoon(WorldContext) ? 12.f : AWRGameState::HourIn(WorldContext);
	if (Hour < D.DevilFrom || Hour >= D.DevilTo) return false;
	// Today's count (2-4), the same on every machine: a hash of the real hour the server has run.
	const int32 Day = FMath::FloorToInt(float(T / FMath::Max(60.0, double(FWRTuning::Get().Num(TEXT("clock.dayMinutes"), 60.f)) * 60.0)));
	const int32 Count = 2 + int32(DzHash(Day, 31) * 2.999f);
	if (Index >= Count) return false;
	FBox2D Box(ForceInit);
	for (const FVector2D& V : D.DevilPoly) Box += V;
	const FVector2D C = Box.GetCenter(), Ext = Box.GetExtent();
	const FVector2D Centre = C + FVector2D((DzHash(Index, 7) - 0.5f) * Ext.X, (DzHash(Index, 11) - 0.5f) * Ext.Y);
	const float A = FMath::Max(2000.f, FMath::Min(Ext.X, Ext.Y) * 0.45f);
	// A slow figure-eight at ~4 m/s (its length is about 6 A).
	const float W = DzTn(TEXT("devils.speed"), 400.f) / (6.f * A);
	const float Ph = float(FMath::Fmod(T * W, 2.0 * PI)) + DzHash(Index, 3) * 6.283f;
	const float Turn = DzHash(Index, 19) * 3.1416f;
	const FVector2D Local(A * FMath::Sin(Ph), A * 0.5f * FMath::Sin(2.f * Ph));
	const FVector2D Off(Local.X * FMath::Cos(Turn) - Local.Y * FMath::Sin(Turn), Local.X * FMath::Sin(Turn) + Local.Y * FMath::Cos(Turn));
	Out = FVector(Centre + Off, D.PanZ);
	float G = 0.f;
	if (WRLand::GroundUnder(Out + FVector(0, 0, 50000.f), G)) Out.Z = G;
	return true;
}

bool WRDesertHz::IsThrownKind(FName Kind) { return Kind == TEXT("oasis_jar") || Kind == TEXT("flash_pot"); }

void WRDesertHz::ThrownLanded(AActor* Projectile, FName Kind, const FVector& At, AActor* HitActor, AWRCharacterBase* By)
{
	UWorld* World = Projectile ? Projectile->GetWorld() : nullptr;
	if (!World) return;
	if (Kind == TEXT("oasis_jar"))
	{
		// The jar bursts: 6 m of wet sand for 30 s (no Dunejaw dives there, Ssarrok beaches) and a water hit in 2.5 m.
		WRFx::Broadcast(World, TEXT("desert.jar_splash"), At, FRotator::ZeroRotator, 1.f, TEXT("desert.jar_splash"));
		if (UWRDesertHazards* H = UWRDesertHazards::Get(World)) H->Wet(At, DzTn(TEXT("jar.wetRadius"), 600.f), DzTn(TEXT("jar.wetSeconds"), 30.f));
		const float R = DzTn(TEXT("jar.radius"), 250.f);
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It == By || !It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > R + It->GetSimpleCollisionRadius()) continue;
			DzHit(*It, By, DzTn(TEXT("jar.damage"), 5.f), At, WRTags::Element_Water, { { TEXT("soak"), DzTn(TEXT("jar.soak"), 40.f) } });
		}
		return;
	}
	if (Kind == TEXT("flash_pot"))
	{
		// A 0.6 s fuse (the hiss), then a sunstone flash: blind 80 to everyone in 4 m facing it (you too).
		WRSound::PlayAt(Projectile, TEXT("desert.flash_fuse"), At);
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TWeakObjectPtr<AWRCharacterBase> WeakBy(By);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [WeakWorld, WeakBy, At]()
		{
			UWorld* W = WeakWorld.Get();
			if (!W) return;
			WRFx::Broadcast(W, TEXT("desert.flash"), At, FRotator::ZeroRotator, 1.f, TEXT("desert.flash"));
			const float R = DzTn(TEXT("flash.radius"), 400.f);
			const float Cos = FMath::Cos(FMath::DegreesToRadians(DzTn(TEXT("flash.facing"), 60.f)));
			for (TActorIterator<AWRCharacterBase> It(W); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > R + It->GetSimpleCollisionRadius()) continue;
				const APawn* P = *It;
				const FVector Facing = P->GetController() && P->IsPlayerControlled() ? P->GetControlRotation().Vector() : P->GetActorForwardVector();
				if (FVector::DotProduct(Facing.GetSafeNormal(), (At - It->GetActorLocation()).GetSafeNormal()) < Cos && FVector::Dist(It->GetActorLocation(), At) > 150.f) continue;
				DzHit(*It, WeakBy.Get(), 0.f, At, WRTags::Element_Physical, { { TEXT("blind"), DzTn(TEXT("flash.blind"), 80.f) } });
			}
		}, DzTn(TEXT("flash.fuse"), 0.6f), false);
	}
}

void WRDesertHz::OnDodge(AWRCharacterBase* Who)
{
	UWRDesertHazards* H = UWRDesertHazards::Get(Who);
	UWRDesertHazards::FTrack* W = H ? H->Tracks.Find(Who) : nullptr;
	if (!W || W->QuickPhase < 2 || W->QuickPhase > 2) return;
	const bool bOil = WRVoid::BrewStat(Who, TEXT("duneOil")) > 0.f && !W->bOilUsed;
	W->Sink -= bOil ? 1.f : DzTn(TEXT("quicksand.dodgeLift"), 0.25f);
	if (bOil) W->bOilUsed = true;
	WRFx::Broadcast(Who, TEXT("desert.sand_stream"), Who->GetActorLocation() - FVector(0, 0, Who->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 0.6f, NAME_None);
	if (W->Sink <= 0.f)
	{
		// Out: the suck has to start over (a moment's footing on the edge).
		W->Sink = 0.f;
		W->QuickPhase = 1;
		W->QuickSince = DzNow(Who) + DzTn(TEXT("quicksand.outGrace"), 1.0f);
		DzTag(Who, WRDesertTags::Sinking, false);
		DzTell(Who, bOil ? TEXT("The oil lets the sand go of you at once") : TEXT("You tear free of the sand"));
	}
}

bool WRDesertHz::PullPrompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !WRDesert::Live()) return false;
	for (TActorIterator<AWRCharacter> It(Me->GetWorld()); It; ++It)
	{
		if (*It == Me || !It->IsAlive() || !(It->HasTag(WRDesertTags::Sinking) || It->HasTag(WRDesertTags::Held))) continue;
		if (FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) > DzTn(TEXT("quicksand.pullReach"), 300.f)) continue;
		OutTarget = It->GetPlayerState() ? It->GetPlayerState()->GetPlayerName() : FString(TEXT("Survivor"));
		OutVerb = TEXT("Pull them out (stay beside them 2 s)");
		return true;
	}
	return false;
}

FString WRDesertHz::StartPull(AWRPlayerController* PC)
{
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	UWRDesertHazards* H = UWRDesertHazards::Get(PC);
	if (!Me || !H) return FString();
	AWRCharacterBase* Best = nullptr;
	float BestD = DzTn(TEXT("quicksand.pullReach"), 300.f);
	for (TPair<TWeakObjectPtr<AWRCharacterBase>, UWRDesertHazards::FTrack>& KV : H->Tracks)
	{
		AWRCharacterBase* C = KV.Key.Get();
		if (!C || C == Me || KV.Value.QuickPhase < 2) continue;
		const float D = FVector::Dist(C->GetActorLocation(), Me->GetActorLocation());
		if (D < BestD) { BestD = D; Best = C; }
	}
	if (!Best) return TEXT("No one sinking within reach");
	UWRDesertHazards::FTrack& W = H->Tracks.FindOrAdd(Best);
	W.PulledBy = Me;
	W.PullSince = DzNow(Me);
	DzTell(Best, TEXT("Someone has you by the arms: hold on"));
	return TEXT("You brace and pull...");
}

// ================================================================================================ the subsystem

bool UWRDesertHazards::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UWRDesertHazards::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRDesertHazards, STATGROUP_Tickables); }
bool UWRDesertHazards::IsTickable() const { return DzLive(GetWorld()); }

UWRDesertHazards* UWRDesertHazards::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRDesertHazards>() : nullptr;
}

double UWRDesertHazards::ServerNow() const { return DzNow(GetWorld()); }

void UWRDesertHazards::Wet(const FVector& At, float Radius, float Seconds)
{
	const double T = ServerNow();
	WetSand.RemoveAll([T](const FWet& W) { return T >= W.Until; });
	WetSand.Add({ At, Radius, T + Seconds });
}

void UWRDesertHazards::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (World->GetNetMode() != NM_Client)
	{
		ServerClock += DeltaTime;
		const float Step = 0.1f;
		if (ServerClock >= Step)
		{
			const float Dt = ServerClock;
			ServerClock = 0.f;
			StepServer(Dt);
		}
	}
	if (World->GetNetMode() != NM_DedicatedServer) StepClient(DeltaTime);
}

void UWRDesertHazards::SetSink(AWRCharacterBase* C, float DepthCm)
{
	AWRDesertState* S = AWRDesertState::Get(C);
	if (!S) return;
	const int32 i = S->Sinkers.IndexOfByPredicate([C](const FWRDesertSinker& K) { return K.Who == C; });
	if (DepthCm <= 0.5f) { if (i != INDEX_NONE) S->Sinkers.RemoveAt(i); return; }
	if (i == INDEX_NONE) { FWRDesertSinker K; K.Who = C; K.Depth = DepthCm; S->Sinkers.Add(K); return; }
	if (FMath::Abs(S->Sinkers[i].Depth - DepthCm) > 2.f) S->Sinkers[i].Depth = DepthCm;
}

void UWRDesertHazards::StepServer(float Dt)
{
	UWorld* World = GetWorld();
	AWRDesertState* S = AWRDesertState::Get(World);
	if (!S) return;
	const FWRDesertData& D = FWRDesertData::Get();
	if (S->BrineBrokenUntil.Num() != D.Brine.Num()) S->BrineBrokenUntil.Init(-1.f, D.Brine.Num());
	if (S->FloodStart.Num() != D.Floods.Num()) S->FloodStart.Init(-1.f, D.Floods.Num());
	const double T = ServerNow();
	for (TActorIterator<AWRCharacter> It(World); It; ++It)
	{
		AWRCharacter* C = *It;
		FTrack& W = Tracks.FindOrAdd(C);
		StepBody(C, W, T, Dt);
	}
	for (auto It = Tracks.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
	S->Sinkers.RemoveAll([](const FWRDesertSinker& K) { return !IsValid(K.Who); });
	StepDevilsServer(T);
	StepFloods(T);
	StepLongNoon();
}

void UWRDesertHazards::StepBody(AWRCharacterBase* C, FTrack& W, double T, float Dt)
{
	AWRDesertState* S = AWRDesertState::Get(C);
	const FWRDesertData& D = FWRDesertData::Get();
	const FVector P = C->GetActorLocation();
	const FVector Feet = P - FVector(0, 0, C->GetSimpleCollisionHalfHeight());
	const bool bAlive = C->IsAlive();
	const bool bIn = bAlive && WRDesert::Contains(P);
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	const bool bGround = Move && !Move->IsFalling() && !Move->IsSwimming() && !Move->IsFlying();
	const bool bMounted = DzMount(C) != nullptr;
	const AWRCreature* Mount = DzMount(C);
	const bool bMountPass = Mount && Mount->GetDef() && Mount->GetDef()->Resists.Contains(TEXT("soft_sand"));
	const bool bWalker = WRSun::Sandwalker(C);
	const bool bOpen = bIn && DzOpen(P);

	// --- The slows (replicated tags both machines read).
	const bool bEased = UWRInventoryComponent::GearStatFor(C, TEXT("passSandSlow")) > 0.f || UWRInventoryComponent::GearStatFor(C, TEXT("passSandstorm")) > 0.f || WRVoid::BrewStat(C, TEXT("duneOil")) > 0.f;
	const uint8 Surf = bIn ? WRDesert::SurfaceAt(Feet) : uint8(255);
	const bool bSoft = bOpen && bGround && !bWalker && !bMountPass && WRDesert::IsSoftSand(Surf);
	const bool bBelt = bOpen && !bWalker && !bMountPass && DzInStorm(C, P);
	if (bSoft != W.bSoft) { W.bSoft = bSoft; DzTag(C, WRDesertTags::SoftSand, bSoft); }
	if (bBelt != W.bBelt) { W.bBelt = bBelt; DzTag(C, WRDesertTags::Belt, bBelt); }
	if (bEased != W.bEased) { W.bEased = bEased; DzTag(C, WRDesertTags::Eased, bEased); }

	// --- Quicksand.
	const int32 Q = bIn && !bMounted && !bWalker ? WRDesert::QuicksandAt(Feet) : INDEX_NONE;
	auto FreeQuick = [&](const TCHAR* Line)
	{
		W.QuickPhase = 0;
		W.Sink = 0.f;
		W.bOilUsed = false;
		W.PulledBy = nullptr;
		DzTag(C, WRDesertTags::Sinking, false);
		DzTag(C, WRDesertTags::Held, false);
		if (Line) DzTell(C, Line);
	};
	switch (W.QuickPhase)
	{
	case 0:
		if (Q != INDEX_NONE && bGround)
		{
			W.QuickPhase = 1;
			W.QuickSince = T;
			WRFx::Broadcast(C, FxSuck, Feet, FRotator::ZeroRotator, 1.f, TEXT("desert.quicksand_suck"));
		}
		break;
	case 1:
		if (Q == INDEX_NONE) { W.QuickPhase = 0; break; }
		if (T - W.QuickSince >= DzTn(TEXT("quicksand.tell"), 0.9f))
		{
			W.QuickPhase = 2;
			W.Sink = FMath::Max(W.Sink, 0.02f);
			DzTag(C, WRDesertTags::Sinking, true);
			DzTell(C, TEXT("Quicksand! Dodge to tear free"));
		}
		break;
	case 2:
	case 3:
		if (!bAlive) { FreeQuick(nullptr); break; }
		if (W.QuickPhase == 2)
		{
			if (Q == INDEX_NONE && W.Sink < DzTn(TEXT("quicksand.walkOut"), 0.35f)) { FreeQuick(TEXT("You haul yourself out onto firm sand")); break; }
			W.Sink = FMath::Min(1.f, W.Sink + Dt / DzTn(TEXT("quicksand.sinkSeconds"), 4.f));
			if (W.Sink >= 1.f)
			{
				W.QuickPhase = 3;
				W.HeldSince = T;
				DzTag(C, WRDesertTags::Held, true);
				DzTell(C, TEXT("Sunk to the shoulders: you can't move. Someone has to pull you out"));
				WRFx::Broadcast(C, FxSuck, Feet, FRotator::ZeroRotator, 1.4f, TEXT("desert.quicksand_suck"));
			}
		}
		else if (T - W.HeldSince >= DzTn(TEXT("quicksand.drownSeconds"), 5.f))
		{
			// The sand closes over you (no floors).
			DzHit(C, nullptr, 100000.f, Feet, WRTags::Element_Physical);
			FreeQuick(nullptr);
			break;
		}
		// An ally's pull: 2 s beside you.
		if (AWRCharacterBase* By = W.PulledBy.Get())
		{
			if (!By->IsAlive() || FVector::Dist(By->GetActorLocation(), P) > DzTn(TEXT("quicksand.pullReach"), 300.f) + 100.f) { W.PulledBy = nullptr; DzTell(By, TEXT("You lost your grip")); }
			else if (T - W.PullSince >= DzTn(TEXT("quicksand.pullSeconds"), 2.f))
			{
				FreeQuick(TEXT("Hauled out, coughing sand"));
				DzTell(By, TEXT("You drag them out of the sand"));
				C->LaunchCharacter((By->GetActorLocation() - P).GetSafeNormal2D() * 400.f + FVector(0, 0, 350.f), true, true);
				W.QuickSince = T + 1.5;
				W.QuickPhase = 1;
			}
		}
		break;
	}

	// --- Brine crust (D1's flats).
	const int32 Br = bIn && bGround && !bMounted ? WRDesert::BrineAt(Feet) : INDEX_NONE;
	switch (W.BrinePhase)
	{
	case 0:
		if (Br != INDEX_NONE)
		{
			const bool bBroken = S && S->BrineBrokenUntil.IsValidIndex(Br) && S->BrineBrokenUntil[Br] > T;
			W.Brine = Br;
			W.BrineSince = T;
			W.BrinePhase = bBroken ? 2 : 1;
			if (bBroken) { DzTag(C, WRDesertTags::Pit, true); W.NextBrineHurt = T + 1.0; }
			else WRFx::Broadcast(C, NAME_None, Feet, FRotator::ZeroRotator, 1.f, TEXT("desert.brine_creak"));
		}
		break;
	case 1:
		if (Br == INDEX_NONE) { W.BrinePhase = 0; break; }
		if (T - W.BrineSince >= DzTn(TEXT("brine.tell"), 1.0f))
		{
			W.BrinePhase = 2;
			W.NextBrineHurt = T + 1.0;
			if (S && S->BrineBrokenUntil.IsValidIndex(W.Brine)) S->BrineBrokenUntil[W.Brine] = float(T + DzTn(TEXT("brine.brokenSeconds"), 300.f));
			DzTag(C, WRDesertTags::Pit, true);
			WRFx::Broadcast(C, TEXT("desert.brine_break"), Feet, FRotator::ZeroRotator, 1.f, TEXT("desert.brine_break"));
			DzTell(C, TEXT("The crust gives way: brine to the waist. Wade out to the edge"));
		}
		break;
	case 2:
		if (!bAlive) { W.BrinePhase = 0; DzTag(C, WRDesertTags::Pit, false); break; }
		if (T >= W.NextBrineHurt)
		{
			W.NextBrineHurt = T + 1.0;
			DzHit(C, nullptr, DzTn(TEXT("brine.damage"), 4.f), Feet, WRTags::Element_Physical);
		}
		if (Br == INDEX_NONE) { W.BrinePhase = 3; W.BrineSince = T; }
		break;
	case 3:
		// The climb out at the edge (still in the slow, rising out of the pit).
		if (T - W.BrineSince >= DzTn(TEXT("brine.climbSeconds"), 2.f)) { W.BrinePhase = 0; DzTag(C, WRDesertTags::Pit, false); }
		break;
	}

	// --- Booming slipfaces.
	if (bOpen && bGround && !bMounted && !bWalker && Surf == EWRDesertSurf::Slipface && T >= W.SlipReadyAt && W.BuriedUntil <= 0.0)
	{
		if (W.SlipSeconds <= 0.f)
		{
			WRFx::Broadcast(C, TEXT("desert.crest_stream"), Feet + FVector(0, 0, 300.f), FRotator::ZeroRotator, 1.f, TEXT("desert.boom"));
		}
		W.SlipSeconds += Dt;
		if (W.SlipSeconds >= DzTn(TEXT("slipface.tell"), 1.2f))
		{
			// The avalanche: 12 m down the fall line (the ground's slope here), then 2 s buried.
			float Gx0 = 0.f, Gx1 = 0.f, Gy0 = 0.f, Gy1 = 0.f;
			const float Step = 200.f;
			const FVector Up(0, 0, 20000.f);
			WRLand::GroundUnder(Feet + FVector(-Step, 0, 0) + Up, Gx0); WRLand::GroundUnder(Feet + FVector(Step, 0, 0) + Up, Gx1);
			WRLand::GroundUnder(Feet + FVector(0, -Step, 0) + Up, Gy0); WRLand::GroundUnder(Feet + FVector(0, Step, 0) + Up, Gy1);
			FVector Down(Gx0 - Gx1, Gy0 - Gy1, 0.f);
			Down = Down.IsNearlyZero() ? C->GetActorForwardVector().GetSafeNormal2D() : Down.GetSafeNormal();
			const float Carry = DzTn(TEXT("slipface.carry"), 1200.f), Secs = DzTn(TEXT("slipface.carrySeconds"), 1.0f);
			WRSkyAir::Push(Cast<ACharacter>(C), Down * (Carry / Secs) + FVector(0, 0, 150.f), Secs, TEXT("avalanche"), nullptr);
			WRFx::Broadcast(C, TEXT("desert.avalanche"), Feet, Down.Rotation(), 1.f, TEXT("desert.avalanche"));
			W.SlipSeconds = 0.f;
			W.SlipReadyAt = T + DzTn(TEXT("slipface.cooldown"), 15.f);
			W.BuriedUntil = -(T + Secs);   // negative: the burial waits for the carry to end
			// Any Dunejaw under the face surfaces.
			for (const TWeakObjectPtr<UWRDesertBody>& B : UWRDesertBody::Live)
			{
				AWRCreature* J = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
				if (!J || J->CreatureId != TEXT("dunejaw") || !J->IsAlive() || FVector::Dist(J->GetActorLocation(), P) > DzTn(TEXT("slipface.popReach"), 2000.f)) continue;
				if (B->Has(WRDesertFlag::Diving))
				{
					B->Set(WRDesertFlag::Diving, false);
					B->Set(WRDesertFlag::Beached, true);
					B->BeachedUntil = T + DzTn(TEXT("slipface.popSeconds"), 3.f);
					B->MulticastMoment(TEXT("desert.sand_burst"), 1.f, TEXT("creature.dunejaw.burst"));
				}
			}
		}
	}
	else if (Surf != EWRDesertSurf::Slipface || !bGround)
	{
		W.SlipSeconds = 0.f;
	}
	if (W.BuriedUntil < 0.0 && T >= -W.BuriedUntil)
	{
		W.BuriedUntil = T + DzTn(TEXT("slipface.buriedSeconds"), 2.f);
		DzTag(C, WRDesertTags::Buried, true);
		DzHit(C, nullptr, DzTn(TEXT("slipface.damage"), 20.f), C->GetActorLocation() + FVector(0, 0, 200.f), WRTags::Element_Physical);
		if (UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent())
		{
			if (const UWRAttributeSet* A = C->GetAttributes()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, A->GetStamina() - DzTn(TEXT("slipface.stamina"), 40.f)));
		}
		DzTell(C, TEXT("Buried by the slide: dig yourself out"));
	}
	else if (W.BuriedUntil > 0.0 && T >= W.BuriedUntil)
	{
		W.BuriedUntil = 0.0;
		DzTag(C, WRDesertTags::Buried, false);
		WRFx::Broadcast(C, TEXT("hit.dust"), C->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("desert.dig"));
	}

	// --- How deep the body shows (every machine eases to this).
	const float Depth = W.QuickPhase >= 2 ? W.Sink * DzTn(TEXT("quicksand.depth"), 140.f)
		: W.BrinePhase == 2 ? DzTn(TEXT("brine.drop"), 150.f)
		: W.BrinePhase == 3 ? DzTn(TEXT("brine.drop"), 150.f) * FMath::Clamp(1.f - float(T - W.BrineSince) / DzTn(TEXT("brine.climbSeconds"), 2.f), 0.f, 1.f)
		: W.BuriedUntil > 0.0 ? DzTn(TEXT("slipface.buryDepth"), 120.f) : 0.f;
	SetSink(C, Depth);

	// --- Amastan's quests that are places (cook 2.11): the belt walk to the Wall Shrine with the lens; the shadow's
	// road at noon to the plaza's run-back stone.
	if (bIn && C->GetPlayerState())
	{
		if (W.bBelt || DzInStorm(C, P))
		{
			const FWRDesertData::FPlace* Wall = D.FindSite(TEXT("shrine_wall"));
			const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(C);
			if (Wall && Inv && Inv->CountOf(TEXT("sunwatch_lens")) > 0 && FVector::Dist2D(Wall->At, P) < DzTn(TEXT("quest.wallReach"), 1500.f)) DzQuest(C, TEXT("belt_walk"));
		}
		const float Hour = WRDesertHz::LongNoon(C) ? 12.f : AWRGameState::HourIn(C);
		FVector Run;
		if (FMath::Abs(Hour - 12.f) <= DzTn(TEXT("quest.noonHalfHours"), 0.75f) && D.ArenaPoint(TEXT("ahrumet"), TEXT("runback"), Run) && FVector::Dist2D(Run, P) < DzTn(TEXT("quest.runbackReach"), 600.f)) DzQuest(C, TEXT("shadow_road"));
	}
}

void UWRDesertHazards::StepDevilsServer(double T)
{
	UWorld* World = GetWorld();
	const float R = DzTn(TEXT("devils.radius"), 250.f);
	for (int32 i = 0; i < 4; ++i)
	{
		FVector At;
		if (!WRDesertHz::DevilAt(World, i, T, At)) continue;
		for (TPair<TWeakObjectPtr<AWRCharacterBase>, FTrack>& KV : Tracks)
		{
			AWRCharacterBase* C = KV.Key.Get();
			if (!C || !C->IsAlive() || T - KV.Value.DevilHitAt < DzTn(TEXT("devils.again"), 4.f)) continue;
			const FVector P = C->GetActorLocation();
			if (FVector::Dist2D(P, At) > R + C->GetSimpleCollisionRadius() || P.Z - At.Z > 2000.f || P.Z < At.Z - 300.f) continue;
			// Lifted 1.2 s and thrown 6 m: 20 and blind 30.
			KV.Value.DevilHitAt = T;
			const FVector Out = (P - At).GetSafeNormal2D().IsNearlyZero() ? FVector(1, 0, 0) : (P - At).GetSafeNormal2D();
			const float Secs = DzTn(TEXT("devils.liftSeconds"), 1.2f);
			WRSkyAir::Push(Cast<ACharacter>(C), Out * (DzTn(TEXT("devils.throw"), 600.f) / Secs) + FVector(0, 0, DzTn(TEXT("devils.lift"), 700.f)), Secs, TEXT("dust_devil"), nullptr);
			DzHit(C, nullptr, DzTn(TEXT("devils.damage"), 20.f), At, WRTags::Element_Physical, { { TEXT("blind"), DzTn(TEXT("devils.blind"), 30.f) } });
			WRFx::Broadcast(C, TEXT("hit.dust"), P, FRotator::ZeroRotator, 2.f, TEXT("desert.devil_hiss"));
		}
	}
}

void UWRDesertHazards::StepFloods(double T)
{
	UWorld* World = GetWorld();
	AWRDesertState* S = AWRDesertState::Get(World);
	const FWRDesertData& D = FWRDesertData::Get();
	if (!S || D.Floods.Num() == 0) return;
	// Upstream: rain or a storm in the Storm peaks (or the Great Storm) sends the floods down a while later.
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(World, TEXT("storm_peaks"));
	const FName Now = K ? K->Id : NAME_None;
	const bool bWet = Now == TEXT("rain") || Now == TEXT("thunderstorm") || Now == TEXT("storm") || Now == TEXT("squall");
	const bool bWas = LastStormWeather == TEXT("rain") || LastStormWeather == TEXT("thunderstorm") || LastStormWeather == TEXT("storm") || LastStormWeather == TEXT("squall");
	if (bWet && !bWas)
	{
		for (int32 i = 0; i < D.Floods.Num(); ++i) if (S->FloodStart[i] < 0.f) S->FloodStart[i] = float(T + DzTn(TEXT("flood.delay"), 90.f) + i * DzTn(TEXT("flood.stagger"), 40.f));
		UE_LOG(LogWildreach, Log, TEXT("DESERT rain in the peaks (%s): the wadis will run"), *Now.ToString());
	}
	LastStormWeather = Now;
	const float Speed = DzTn(TEXT("flood.speed"), 600.f);
	for (int32 i = 0; i < D.Floods.Num(); ++i)
	{
		if (S->FloodStart[i] < 0.f || T < S->FloodStart[i]) continue;
		const FWRDesertData::FFlood& F = D.Floods[i];
		const float Front = float(T - S->FloodStart[i]) * Speed;
		const float Len = DzFloodLength(F);
		if (Front > Len + 3000.f) { S->FloodStart[i] = -1.f; continue; }
		// The roar runs ahead of the wall; the trickle 3 s ahead of it (both heard and seen on every machine).
		if (FMath::Fmod(float(T - S->FloodStart[i]), 2.f) < 0.1f)
		{
			FVector Dir;
			const FVector Roar = DzFloodPoint(F, FMath::Min(Front, Len), Dir);
			WRFx::Broadcast(World, TEXT("desert.flood_front"), Roar, Dir.Rotation(), 1.f, TEXT("desert.flood_roar"));
			const FVector Trick = DzFloodPoint(F, FMath::Min(Front + Speed * 3.f, Len), Dir);
			WRFx::Broadcast(World, TEXT("desert.flood_trickle"), Trick, Dir.Rotation(), 1.f, NAME_None);
		}
		for (TPair<TWeakObjectPtr<AWRCharacterBase>, FTrack>& KV : Tracks)
		{
			AWRCharacterBase* C = KV.Key.Get();
			FTrack& W = KV.Value;
			if (!C || !C->IsAlive()) continue;
			const FVector P = C->GetActorLocation();
			float Along = 0.f, Across = 0.f;
			FVector At, Dir;
			if (!DzOnFlood(F, P, Along, Across, At, Dir) || Across > F.Width * 0.5f) continue;
			float G = At.Z;
			WRLand::GroundUnder(FVector(P.X, P.Y, P.Z + 300.f), G);
			const float Feet = P.Z - C->GetSimpleCollisionHalfHeight();
			if (Feet > G + DzTn(TEXT("flood.height"), 200.f)) continue;    // on a ledge: the water runs under you
			if (W.FloodHit != i && FMath::Abs(Along - Front) < DzTn(TEXT("flood.frontDepth"), 300.f))
			{
				W.FloodHit = i;
				W.DebrisLeft = 3 + int32(DzHash(int32(T * 10.0), C->GetUniqueID()) * 2.999f);
				W.NextDebrisAt = T + 0.3;
				DzHit(C, nullptr, DzTn(TEXT("flood.damage"), 50.f), At - Dir * 200.f, WRTags::Element_Water, { { TEXT("soak"), 100.f } });
				const float Secs = DzTn(TEXT("flood.carrySeconds"), 1.0f);
				WRSkyAir::Push(Cast<ACharacter>(C), Dir * (DzTn(TEXT("flood.carry"), 1500.f) / Secs) + FVector(0, 0, 120.f), Secs, TEXT("flood"), nullptr);
				DzTell(C, TEXT("The flood takes you: get to a ledge"));
			}
		}
	}
	// The debris in the water (3-5 hits of 10 while it carries you).
	for (TPair<TWeakObjectPtr<AWRCharacterBase>, FTrack>& KV : Tracks)
	{
		AWRCharacterBase* C = KV.Key.Get();
		FTrack& W = KV.Value;
		if (!C || W.DebrisLeft <= 0 || T < W.NextDebrisAt) continue;
		--W.DebrisLeft;
		W.NextDebrisAt = T + 0.35;
		DzHit(C, nullptr, DzTn(TEXT("flood.debris"), 10.f), C->GetActorLocation() + FVector(0, 0, 50.f), WRTags::Element_Physical);
		if (W.DebrisLeft <= 0) W.FloodHit = INDEX_NONE;
	}
}

void UWRDesertHazards::StepLongNoon()
{
	UWorld* World = GetWorld();
	AWRDesertState* S = AWRDesertState::Get(World);
	if (!S) return;
	const AWRWorldState* WS = AWRWorldState::Get(World);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("long_noon")) : nullptr;
	const double T = ServerNow();
	uint8 Want = 0;
	if (E) Want = E->Stage == 0 && T < E->StartAt ? 1 : (T < E->EndAt ? 2 : 0);
	// After it, a 2-minute dusk (the sky eases from noon to the real hour), then nothing.
	if (Want == 0 && NoonStage == 2) Want = 3;
	if (Want == 0 && NoonStage == 3 && T - S->NoonStageAt < NoTn(TEXT("duskSeconds"), 120.f)) Want = 3;
	if (Want == NoonStage) return;
	const uint8 Was = NoonStage;
	NoonStage = Want;
	S->NoonStage = Want;
	S->NoonStageAt = float(T);
	const FWRDesertData& D = FWRDesertData::Get();
	UWRDesertLife* Life = UWRDesertLife::Get(World);
	const FVector PanAt = D.PanPoly.Num() > 0 ? [&]() { FVector2D Sum(0, 0); for (const FVector2D& V : D.PanPoly) Sum += V; return FVector(Sum / float(D.PanPoly.Num()), D.PanZ); }() : FVector::ZeroVector;
	if (Want == 1)
	{
		// The sign: the sun goes white, the shimmer column over the Pan seen from the whole map.
		WRFx::Broadcast(World, TEXT("desert.heat_column"), PanAt, FRotator::ZeroRotator, 1.f, TEXT("desert.long_noon_sign"));
		UE_LOG(LogWildreach, Log, TEXT("DESERT the Long Noon's sign"));
	}
	if (Want == 2)
	{
		S->bLongNoon = true;
		// The seven tilted stelae cast the only shadows: their caches open.
		for (TActorIterator<AWRDesertSpot> It(World); It; ++It) if (It->Kind == TEXT("stela_cache") || It->Kind == TEXT("seventh_vault")) It->bConcealed = false;
		// 20 bloom nodes rise on the Pan (they stay a game day after or till gathered).
		if (UWRDesertWorld* DW = UWRDesertWorld::Get(World))
		{
			for (const FVector& B : D.Bloom)
			{
				FVector At = B;
				float G = 0.f;
				if (WRLand::GroundUnder(B + FVector(0, 0, 50000.f), G)) At.Z = G;
				if (AWRResourceNode* N = DW->SpawnNode(TEXT("sunstone"), At, int32(NoTn(TEXT("bloomHits"), 4.f)), 1.f, 1.0e7f))
				{
					N->YieldScale = [](AWRCharacterBase* Who) { return UWRDesertWorld::SunstoneYield(Who); };
					BloomNodes.Add(N);
				}
			}
		}
		if (Life)
		{
			Life->KindScale.Add(TEXT("glasstail"), NoTn(TEXT("dugIn"), 0.5f));
			Life->KindScale.Add(TEXT("dunejaw"), NoTn(TEXT("dugIn"), 0.5f));
			Life->KindScale.Add(TEXT("carrion_kite"), NoTn(TEXT("kites"), 2.f));
		}
		UE_LOG(LogWildreach, Log, TEXT("DESERT the Long Noon: the sun stands still"));
	}
	if (Want == 3 || (Want == 0 && Was == 2))
	{
		S->bLongNoon = false;
		for (TActorIterator<AWRDesertSpot> It(World); It; ++It) if (It->Kind == TEXT("stela_cache") || It->Kind == TEXT("seventh_vault")) It->bConcealed = true;
		const float Life1 = NoTn(TEXT("bloomStaySeconds"), 3600.f);
		for (const TWeakObjectPtr<AActor>& N : BloomNodes) if (N.IsValid()) N->SetLifeSpan(Life1);
		BloomNodes.Reset();
		if (Life) Life->KindScale.Reset();
		UE_LOG(LogWildreach, Log, TEXT("DESERT the Long Noon ends: a short dusk"));
	}
}

// ================================================================================================ the client

void UWRDesertHazards::StepClient(float Dt)
{
	UWorld* World = GetWorld();
	const AWRDesertState* S = AWRDesertState::Get(World);
	const FWRDesertData& D = FWRDesertData::Get();
	if (!S || !D.bLoaded) return;
	const double T = ServerNow();

	// Bodies sinking: eased toward the replicated depth (a brine drop falls fast, quicksand creeps).
	TSet<const AActor*> Seen;
	for (const FWRDesertSinker& K : S->Sinkers)
	{
		if (!IsValid(K.Who)) continue;
		Seen.Add(K.Who);
		float& Cur = ShownSink.FindOrAdd(K.Who);
		Cur = FMath::FInterpConstantTo(Cur, K.Depth, Dt, K.Depth - Cur > 100.f ? 600.f : 160.f);
	}
	for (auto It = ShownSink.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); continue; }
		if (Seen.Contains(It.Key().Get())) continue;
		It.Value() = FMath::FInterpConstantTo(It.Value(), 0.f, Dt, 200.f);
		if (It.Value() <= 0.f) It.RemoveCurrent();
	}

	// The dust devils: a spinning column of sand where each one stands now.
	if (DevilLooks.Num() == 0)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			AActor* A = DzLookActor(World, FVector::ZeroVector);
			if (A)
			{
				WRFx::Attach(A->GetRootComponent(), TEXT("desert.dust_devil"));
				A->SetActorHiddenInGame(true);
			}
			DevilLooks.Add(A);
		}
	}
	for (int32 i = 0; i < DevilLooks.Num(); ++i)
	{
		AActor* A = DevilLooks[i].Get();
		if (!A) continue;
		FVector At;
		const bool bUp = WRDesertHz::DevilAt(World, i, T, At);
		if (bUp) A->SetActorLocation(At);
		if (A->IsHidden() == bUp)
		{
			A->SetActorHiddenInGame(!bUp);
			if (bUp) WRSound::PlayAt(A, TEXT("desert.devil_hiss"), At);
		}
	}

	// The flood walls: a 2 m brown wall moving down each wadi from its replicated start.
	while (FloodLooks.Num() < D.Floods.Num())
	{
		AActor* A = DzLookActor(World, FVector::ZeroVector);
		if (A)
		{
			const float Wd = D.Floods[FloodLooks.Num()].Width;
			DzShape(A, TEXT("/Engine/BasicShapes/Cube.Cube"), FLinearColor(0.26f, 0.17f, 0.09f), FVector(0, 0, 100.f), FVector(3.f, Wd / 100.f, 2.f));
			DzShape(A, TEXT("/Engine/BasicShapes/Cube.Cube"), FLinearColor(0.42f, 0.3f, 0.18f), FVector(-600.f, 0, 50.f), FVector(10.f, Wd / 100.f, 1.f));
			WRFx::Attach(A->GetRootComponent(), TEXT("desert.flood_spray"));
			A->SetActorHiddenInGame(true);
		}
		FloodLooks.Add(A);
	}
	for (int32 i = 0; i < FloodLooks.Num() && i < D.Floods.Num(); ++i)
	{
		AActor* A = FloodLooks[i].Get();
		if (!A) continue;
		const bool bRun = S->FloodStart.IsValidIndex(i) && S->FloodStart[i] >= 0.f && T >= S->FloodStart[i];
		if (bRun)
		{
			FVector Dir;
			const float Front = FMath::Min(float(T - S->FloodStart[i]) * DzTn(TEXT("flood.speed"), 600.f), DzFloodLength(D.Floods[i]));
			FVector At = DzFloodPoint(D.Floods[i], Front, Dir);
			float G = At.Z;
			if (WRLand::GroundUnder(At + FVector(0, 0, 20000.f), G)) At.Z = G;
			A->SetActorLocationAndRotation(At, Dir.Rotation());
		}
		if (A->IsHidden() == bRun) A->SetActorHiddenInGame(!bRun);
	}

	// The Fata Morgana: the Hourhold's towers upside down in the sky over the Erg, only in the Long Noon and only from
	// far off (it's gone within 300 m).
	const bool bNoon = S->bLongNoon;
	const APlayerController* LocalPC = World->GetFirstPlayerController();
	const APawn* Me = LocalPC ? LocalPC->GetPawn() : nullptr;
	const FVector MAt(NoTn(TEXT("morganaX"), -165430.f), NoTn(TEXT("morganaY"), -716320.f), NoTn(TEXT("morganaZ"), 30000.f));
	if (bNoon && MorganaLooks.Num() == 0)
	{
		if (AActor* A = DzLookActor(World, MAt))
		{
			const FLinearColor Pale(0.86f, 0.82f, 0.86f);
			for (int32 k = 0; k < 7; ++k)
			{
				const float H = 1800.f + DzHash(k, 41) * 2600.f, Wd = 300.f + DzHash(k, 43) * 300.f;
				const FVector Off((k - 3) * 1100.f, (DzHash(k, 47) - 0.5f) * 1600.f, -H * 0.5f);
				DzShape(A, TEXT("/Engine/BasicShapes/Cube.Cube"), Pale, Off, FVector(Wd / 100.f, Wd / 100.f, H / 100.f));
				DzShape(A, TEXT("/Engine/BasicShapes/Cone.Cone"), Pale * 0.95f, Off - FVector(0, 0, H * 0.5f + 250.f), FVector(Wd / 90.f, Wd / 90.f, 5.f), FRotator(180.f, 0.f, 0.f));
			}
			MorganaLooks.Add(A);
		}
	}
	for (const TWeakObjectPtr<AActor>& M : MorganaLooks)
	{
		AActor* A = M.Get();
		if (!A) continue;
		const bool bShow = bNoon && Me && FVector::Dist2D(Me->GetActorLocation(), MAt) > NoTn(TEXT("morganaGone"), 30000.f);
		if (A->IsHidden() == bShow) A->SetActorHiddenInGame(!bShow);
	}
}
