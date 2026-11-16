// Siege engines (cookpass-building 3.6; biome-crystal 2.21.5 KEPT numbers): the battering ram, the counterweight
// trebuchet and the ballista. Items you stand in front of you; crewed at their posts; their mechanisms move on every
// machine from the replicated state and the server time it began (a pendulum, a falling weight, a windlass), never on a
// tick. The server decides every move, hit and break; all damage to people goes through ApplyHit and to buildings
// through FWRRaiding::DamagePiece. Numbers: tuning.json "siege".

#include "WRSiege.h"
#include "WRPadInput.h"
#include "WRSiegeLook.h"
#include "WRToolMesh.h"
#include "WRRaiding.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRFx.h"
#include "WRStore.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WRSiegeDetail
{
	float Tn(const FString& Key, float Fallback) { return FWRTuning::Get().Num(*(TEXT("siege.") + Key), Fallback); }

	float NowOf(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : World ? World->GetTimeSeconds() : 0.f;
	}

	FString NameOf(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		return P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	}

	void Tell(AWRCharacter* Who, const FString& Line)
	{
		if (AWRPlayerController* PC = Who ? Cast<AWRPlayerController>(Who->GetController()) : nullptr) PC->ClientNotify(Line);
	}

	/** The ram's pendulum: the log's swing angle (degrees, - drawn back) on its two chains. */
	float RamAngle(const FWRSiegeRep& R, float Now)
	{
		const float Draw = Tn(TEXT("ram.drawDeg"), 35.f);
		const float T = FMath::Max(0.f, Now - R.StateAt);
		if (R.State == 1)
		{
			// Drawn back by the crew over drawSeconds (eased: heavy to start, then it comes), from where it hung.
			const float A = FMath::Clamp(T / FMath::Max(0.1f, Tn(TEXT("ram.drawSeconds"), 1.5f)), 0.f, 1.f);
			const float Ease = A * A * (3.f - 2.f * A);
			return FMath::Lerp(R.Wind * Draw, -Draw, Ease);
		}
		if (R.State == 2)
		{
			// Let go: a damped pendulum from the angle it was drawn to; the cap passes the bottom a quarter swing later.
			const float W = 2.f * PI / FMath::Max(0.2f, Tn(TEXT("ram.period"), 1.6f));
			return -Draw * R.Wind * FMath::Cos(W * T) * FMath::Exp(-Tn(TEXT("ram.damping"), 1.35f) * T);
		}
		return 0.f;
	}

	/** The trebuchet's arm: degrees from the horizontal of its long end, pointing back (-35 cocked, 80 at rest). */
	float ArmRest() { return Tn(TEXT("trebuchet.restDeg"), 80.f); }
	float ArmCocked() { return Tn(TEXT("trebuchet.cockedDeg"), -33.f); }
	float ArmThrowSeconds() { return Tn(TEXT("trebuchet.throwSeconds"), 0.9f); }
	float ArmOvershoot() { return Tn(TEXT("trebuchet.overshootDeg"), 115.f); }
	float ReleaseDeg(int32 Pin) { return Tn(FString::Printf(TEXT("trebuchet.releaseDeg%d"), FMath::Clamp(Pin, 0, 2)), 70.f + 15.f * FMath::Clamp(Pin, 0, 2)); }
	/** When after the trigger the sling lets go for this pin (the arm's angle passing the pin's). */
	float ReleaseTime(int32 Pin)
	{
		const float Span = ArmOvershoot() - ArmCocked();
		return ArmThrowSeconds() * FMath::Sqrt(FMath::Clamp((ReleaseDeg(Pin) - ArmCocked()) / Span, 0.f, 1.f));
	}
	float ArmAngle(const FWRSiegeRep& R, float Now, float ShownWind)
	{
		if (R.State == 3)
		{
			// The weight falls: the arm swings up accelerating (a real fall), past upright to its overshoot, then rocks
			// back and settles at rest with the weight swinging in the frame.
			const float T = FMath::Max(0.f, Now - R.StateAt);
			const float Th = ArmThrowSeconds();
			if (T < Th) return ArmCocked() + (ArmOvershoot() - ArmCocked()) * FMath::Square(T / Th);
			const float U = T - Th;
			const float W = 2.f * PI / FMath::Max(0.2f, Tn(TEXT("trebuchet.rockPeriod"), 1.4f));
			return ArmRest() + (ArmOvershoot() - ArmRest()) * FMath::Cos(W * U) * FMath::Exp(-Tn(TEXT("trebuchet.rockDamping"), 1.2f) * U);
		}
		return FMath::Lerp(ArmRest(), ArmCocked(), ShownWind);
	}

	/** The ballista's draw (0..1): the windlass drawing, held drawn, the string snapping forward on the loose, or easing
	 *  back when let go half-drawn. */
	float BallistaDraw(const FWRSiegeRep& R, float Now)
	{
		const float T = FMath::Max(0.f, Now - R.StateAt);
		if (R.State == 1) return FMath::Clamp(T / FMath::Max(0.1f, Tn(TEXT("ballista.drawSeconds"), 2.f)), 0.f, 1.f);
		if (R.State == 2 || R.State == 3) return 1.f;
		const float SinceShot = Now - R.ShotAt;
		if (SinceShot >= 0.f && SinceShot < 0.08f) return 1.f - SinceShot / 0.08f;
		if (R.Wind > 0.f && R.StateAt > R.ShotAt) return R.Wind * (1.f - FMath::Clamp(T / 0.4f, 0.f, 1.f));
		return 0.f;
	}

	/** Every crew post: where it stands (engine space, the base), which way the body faces (yaw in engine space). */
	struct FPost { FVector At; float Yaw; int32 Pose; const TCHAR* Name; bool bTurns; };
	const TArray<FPost>& PostsOf(FName Kind)
	{
		static const TArray<FPost> Ram = {
			{ FVector(-312.f, 68.f, 92.f), 0.f, 0, TEXT("the left handle"), false },
			{ FVector(-312.f, -68.f, 92.f), 0.f, 0, TEXT("the right handle"), false } };
		static const TArray<FPost> Treb = {
			{ FVector(-150.f, 165.f, 92.f), -90.f, 1, TEXT("the winch"), true },
			{ FVector(-430.f, -235.f, 92.f), 45.f, 2, TEXT("the aiming lever"), true } };
		static const TArray<FPost> Bal = {
			{ FVector(-168.f, 0.f, 92.f), 0.f, 2, TEXT("the ballista"), true } };   // (the gunner walks round with the stock)
		static const TArray<FPost> None;
		return Kind == TEXT("battering_ram") ? Ram : Kind == TEXT("trebuchet") ? Treb : Kind == TEXT("ballista") ? Bal : None;
	}

	/** Body collision box (half extents, centre) in engine space. */
	void BoxOf(FName Kind, FVector& OutHalf, FVector& OutCentre)
	{
		// The ram's front is where its cap is at the bottom of the swing (so the cap meets a door, never passes into it).
		if (Kind == TEXT("battering_ram")) { OutHalf = FVector(264.5f, 116.f, 130.f); OutCentre = FVector(8.5f, 0.f, 130.f); }
		else if (Kind == TEXT("trebuchet")) { OutHalf = FVector(330.f, 175.f, 215.f); OutCentre = FVector(0.f, 0.f, 215.f); }
		else { OutHalf = FVector(110.f, 110.f, 90.f); OutCentre = FVector(0.f, 0.f, 90.f); }
	}

	UMaterialInterface* GlowMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_Glow.M_Glow"));
		return M.Get();
	}
}

// ================================================================================================ the engine

AWRSiegeEngine::AWRSiegeEngine()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);   // every machine eases its own copy to Rep.Loc (never snapped at the net rate)
	bAlwaysRelevant = false;
	SetNetCullDistanceSquared(FMath::Square(25000.f));
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->SetBoxExtent(FVector(100.f));
	Blocker->SetHiddenInGame(true);
}

void AWRSiegeEngine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSiegeEngine, Kind);
	DOREPLIFETIME(AWRSiegeEngine, OwnerName);
	DOREPLIFETIME(AWRSiegeEngine, Health);
	DOREPLIFETIME(AWRSiegeEngine, MaxHealth);
	DOREPLIFETIME(AWRSiegeEngine, RoofHealth);
	DOREPLIFETIME(AWRSiegeEngine, Rep);
	DOREPLIFETIME(AWRSiegeEngine, LastHitAt);
	DOREPLIFETIME(AWRSiegeEngine, Crew);
	DOREPLIFETIME(AWRSiegeEngine, Hauler);
}

float AWRSiegeEngine::ServerNow() const { return WRSiegeDetail::NowOf(GetWorld()); }

float AWRSiegeEngine::Num(const TCHAR* Key, float Fallback) const
{
	const FString K = FString::Printf(TEXT("%s.%s"), IsRam() ? TEXT("ram") : IsTrebuchet() ? TEXT("trebuchet") : TEXT("ballista"), Key);
	return WRSiegeDetail::Tn(K, Fallback);
}

void AWRSiegeEngine::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) LastCrewed = ServerNow();   // (its abandon clock starts when it's stood up)
	if (!Kind.IsNone()) OnRep_Kind();
}

void AWRSiegeEngine::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
	{
		for (AWRCharacter* C : TArray<TObjectPtr<AWRCharacter>>(Crew)) if (C) ServerLeave(C, true);
		if (Hauler) Hauler->HarnessedTo = nullptr;
	}
	Super::EndPlay(Reason);
}

void AWRSiegeEngine::OnRep_Kind()
{
	FVector Half, Centre;
	WRSiegeDetail::BoxOf(Kind, Half, Centre);
	Blocker->SetBoxExtent(Half);
	Blocker->SetRelativeLocation(Centre);
	if (Crew.Num() != PostCount()) Crew.SetNum(PostCount());
	if (!bShownInit)
	{
		ShownLoc = GetActorLocation();
		ShownHeading = Rep.Heading = GetActorRotation().Yaw;
		ShownAim = Rep.Aim != 0.f ? Rep.Aim : ShownHeading;
		bShownInit = true;
	}
	Build();
}

int32 AWRSiegeEngine::PostCount() const { return WRSiegeDetail::PostsOf(Kind).Num(); }
FString AWRSiegeEngine::PostName(int32 Post) const { const auto& P = WRSiegeDetail::PostsOf(Kind); return P.IsValidIndex(Post) ? FString(P[Post].Name) : FString(); }
int32 AWRSiegeEngine::PostPose(int32 Post) const { const auto& P = WRSiegeDetail::PostsOf(Kind); return P.IsValidIndex(Post) ? P[Post].Pose : 0; }

FVector AWRSiegeEngine::PostLocal(int32 Post) const
{
	const auto& P = WRSiegeDetail::PostsOf(Kind);
	if (!P.IsValidIndex(Post)) return FVector::ZeroVector;
	// The trebuchet's posts ride the turntable with its upper frame.
	const float Turn = P[Post].bTurns ? FRotator::NormalizeAxis((HasAuthority() ? Rep.Aim : ShownAim) - (HasAuthority() ? Rep.Heading : ShownHeading)) : 0.f;
	return FRotator(0.f, Turn, 0.f).RotateVector(P[Post].At);
}

int32 AWRSiegeEngine::PostOf(const AActor* Who) const
{
	for (int32 i = 0; i < Crew.Num(); ++i) if (Who && Crew[i] == Who) return i;
	return INDEX_NONE;
}

int32 AWRSiegeEngine::FreePostNear(const FVector& At) const
{
	int32 Best = INDEX_NONE;
	float BestD = TNumericLimits<float>::Max();
	for (int32 i = 0; i < PostCount(); ++i)
	{
		if (Crew.IsValidIndex(i) && Crew[i]) continue;
		const float D = FVector::DistSquared(GetActorTransform().TransformPosition(PostLocal(i)), At);
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

float AWRSiegeEngine::CrankAngle() const
{
	const float Now = ServerNow();
	if (IsTrebuchet()) return ShownWind * 2.f * PI * Num(TEXT("drumTurns"), 6.f);
	if (IsBallista()) return WRSiegeDetail::BallistaDraw(Rep, Now) * 2.f * PI * Num(TEXT("windlassTurns"), 4.f);
	return 0.f;
}

bool AWRSiegeEngine::MayHurt(const FString& Raider) const
{
	if (Raider.IsEmpty()) return false;
	if (FWRSocialRules::CanShare(OwnerName, Raider)) return false;
	return WRTune(TEXT("server.pvp"), 0.f) > 0.f;
}

void AWRSiegeEngine::TakeHit(float Amount, AActor* By, EWRRaidSource Source, const FString& Raider, bool bFromAbove)
{
	if (!HasAuthority() || Amount <= 0.f || Health <= 0.f || !MayHurt(Raider)) return;
	const TCHAR* Key = Source == EWRRaidSource::Weapon ? TEXT("weapon") : Source == EWRRaidSource::Blast ? TEXT("blast") : Source == EWRRaidSource::FirePot ? TEXT("firepot")
		: Source == EWRRaidSource::Spell ? TEXT("spell") : Source == EWRRaidSource::Tame ? TEXT("tame") : Source == EWRRaidSource::Siege ? TEXT("siege")
		: Source == EWRRaidSource::Ram ? TEXT("ram") : TEXT("fire");
	const float Defaults[] = { 0.25f, 1.f, 1.f, 0.5f, 0.5f, 1.f, 1.f, 1.f };
	float Dealt = Amount * WRSiegeDetail::Tn(FString::Printf(TEXT("take.%s"), Key), Defaults[FMath::Clamp(int32(Source), 0, 7)]);
	const bool bFire = Source == EWRRaidSource::FirePot || Source == EWRRaidSource::Fire;
	if (IsRam() && bFromAbove && RoofHealth > 0.f)
	{
		// The wet hides take what comes down on the roof, and don't burn well.
		RoofHealth = FMath::Max(0.f, RoofHealth - Dealt * (bFire ? Num(TEXT("roofFire"), 0.5f) : 1.f));
		if (RoofHealth <= 0.f) WRFx::Broadcast(this, TEXT("build.break.wood"), GetActorLocation() + FVector(0.f, 0.f, 240.f), FRotator::ZeroRotator, 1.4f, TEXT("siege.break"));
		ForceNetUpdate();
		return;
	}
	Health = FMath::Max(0.f, Health - Dealt);
	LastHitAt = ServerNow();
	ForceNetUpdate();
	UE_LOG(LogWildreach, Log, TEXT("SIEGE %s hit by %s (%s) for %.0f: %.0f left"), *Kind.ToString(), *Raider, Key, Dealt, Health);
	if (Health <= 0.f) Break(By);
}

void AWRSiegeEngine::Break(AActor* By)
{
	// Planks and parts fly; a third of what it took to make lies in a bag where it stood.
	WRFx::Broadcast(this, TEXT("siege.break"), GetActorLocation() + FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator, 1.8f, TEXT("siege.break"));
	TArray<FWRItemStack> Salvage;
	if (const TSharedPtr<FJsonObject> Recipe = FWRTuning::Get().Object(*FString::Printf(TEXT("siege.%s.salvage"), IsRam() ? TEXT("ram") : IsTrebuchet() ? TEXT("trebuchet") : TEXT("ballista"))))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& P : Recipe->Values)
		{
			FWRItemStack S;
			S.ItemId = FName(*P.Key);
			S.Count = FMath::Max(1, int32(P.Value->AsNumber()));
			Salvage.Add(S);
		}
	}
	if (Salvage.Num() > 0) AWRLootBag::Spill(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 30.f), OwnerName, Salvage);
	UE_LOG(LogWildreach, Log, TEXT("SIEGE %s of %s broken by %s"), *Kind.ToString(), *OwnerName, *GetNameSafe(By));
	Destroy();
}

// ---------------------------------------------------------------- crew (server)

void AWRSiegeEngine::ServerTake(AWRCharacter* Who, int32 Post)
{
	using namespace WRSiegeDetail;
	if (!HasAuthority() || !Who || !Who->IsAlive() || !Crew.IsValidIndex(Post) || Crew[Post] || PostOf(Who) != INDEX_NONE) return;
	if (Who->GetAttachParentActor()) { Tell(Who, TEXT("Not while riding")); return; }
	const auto& P = PostsOf(Kind)[Post];
	Crew[Post] = Who;
	Who->GetCharacterMovement()->StopMovementImmediately();
	Who->GetCharacterMovement()->DisableMovement();
	if (UAbilitySystemComponent* ASC = Who->GetAbilitySystemComponent()) ASC->CancelAllAbilities();
	if (UCapsuleComponent* Cap = Who->GetCapsuleComponent()) Cap->IgnoreActorWhenMoving(this, true);
	Who->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	// Every machine eases the body from where it stood onto the post (AWRCharacter's seat ease), so this isn't seen as a jump.
	Who->SetActorRelativeLocation(PostLocal(Post));
	Who->SetActorRelativeRotation(FRotator(0.f, P.Yaw + (P.bTurns ? FRotator::NormalizeAxis(Rep.Aim - Rep.Heading) : 0.f), 0.f));
	MoveForward[FMath::Min(Post, 1)] = MoveRight[FMath::Min(Post, 1)] = 0;
	bActHeld[FMath::Min(Post, 1)] = false;
	AimYawWant = Rep.Aim;          // the stock stays where it points until the gunner looks somewhere
	AimPitchWant = Rep.AimPitch;
	LastCrewed = ServerNow();
	ForceNetUpdate();
	FString Help;
	if (IsRam()) Help = TEXT("On the handle: Move pushes and turns the ram, hold Attack to draw the log back and let go to swing it; Block harnesses your hauler; Interact steps off");
	else if (IsTrebuchet() && Post == 0) Help = TEXT("At the winch: hold Attack to wind the arm down (10 s); Interact steps off");
	else if (IsTrebuchet()) Help = TEXT("At the lever: Move left / right turns it, forward / back sets the pin (20 / 40 / 60 m); Attack loads a siege stone, then throws when it's wound down");
	else Help = TEXT("At the ballista: look to aim, hold Attack to draw (a bolt goes in the groove), let go to loose after its aim line; Interact steps off");
	if (FWRSocialRules::CanShare(OwnerName, NameOf(Who))) Help += TEXT("; Dodge packs it up");
	Tell(Who, Help);
	UE_LOG(LogWildreach, Log, TEXT("SIEGE %s took %s of %s"), *NameOf(Who), P.Name, *Kind.ToString());
}

void AWRSiegeEngine::ServerLeave(AWRCharacter* Who, bool bPushedOff)
{
	if (!Who) return;
	const int32 Post = PostOf(Who);
	if (Post == INDEX_NONE) return;
	Crew[Post] = nullptr;
	bActHeld[FMath::Min(Post, 1)] = false;
	MoveForward[FMath::Min(Post, 1)] = MoveRight[FMath::Min(Post, 1)] = 0;
	const FVector PostAt = GetActorTransform().TransformPosition(PostLocal(Post));
	Who->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UCapsuleComponent* Cap = Who->GetCapsuleComponent()) Cap->IgnoreActorWhenMoving(this, false);
	// Onto the ground where the post is (or a step further out if something stands there).
	const FVector Out = (PostAt - GetActorLocation()).GetSafeNormal2D();
	for (const float Step : { 0.f, 60.f, 120.f })
	{
		const FVector Try = PostAt + Out * Step;
		FHitResult Ground;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSiegeOff), false, this);
		Q.AddIgnoredActor(Who);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Try + FVector(0.f, 0.f, 150.f), Try - FVector(0.f, 0.f, 300.f), ECC_Visibility, Q)) continue;
		const FVector Stand = Ground.ImpactPoint + FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight() + 4.f);
		if (GetWorld()->OverlapBlockingTestByChannel(Stand, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Who->GetSimpleCollisionRadius(), Who->GetSimpleCollisionHalfHeight()), Q)) continue;
		Who->SetActorLocation(Stand);
		break;
	}
	Who->SetActorRotation(FRotator(0.f, Who->GetActorRotation().Yaw, 0.f));
	Who->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	ForceNetUpdate();
	if (bPushedOff) WRSiegeDetail::Tell(Who, TEXT("Knocked off the post"));
}

void AWRSiegeEngine::ServerMove(AWRCharacter* Who, int32 Forward, int32 Right)
{
	const int32 Post = PostOf(Who);
	if (Post == INDEX_NONE) return;
	const int32 i = FMath::Min(Post, 1);
	// The trebuchet's pin moves one notch per push of the stick, not while it's held.
	if (IsTrebuchet() && Post == 1 && Forward != 0 && MoveForward[i] == 0 && Rep.State != 3)
	{
		const int32 Was = Rep.Range;
		Rep.Range = uint8(FMath::Clamp(int32(Rep.Range) + Forward, 0, 2));
		if (Rep.Range != Was)
		{
			WRFx::Broadcast(this, TEXT("siege.pin"), GetActorTransform().TransformPosition(FVector(-450.f, 0.f, 400.f)), FRotator::ZeroRotator, 0.5f, TEXT("siege.pin"));
			WRSiegeDetail::Tell(Who, FString::Printf(TEXT("Release pin: %d m"), FMath::RoundToInt(Num(*FString::Printf(TEXT("range%d"), Rep.Range), 2000.f + 2000.f * Rep.Range) / 100.f)));
		}
	}
	MoveForward[i] = FMath::Clamp(Forward, -1, 1);
	MoveRight[i] = FMath::Clamp(Right, -1, 1);
}

void AWRSiegeEngine::ServerAim(AWRCharacter* Who, float Yaw, float Pitch)
{
	if (!IsBallista() || PostOf(Who) == INDEX_NONE) return;
	AimYawWant = Yaw;
	AimPitchWant = FMath::Clamp(Pitch, Num(TEXT("pitchMin"), -10.f), Num(TEXT("pitchMax"), 25.f));
}

void AWRSiegeEngine::ServerAct(AWRCharacter* Who, bool bDown)
{
	using namespace WRSiegeDetail;
	const int32 Post = PostOf(Who);
	if (Post == INDEX_NONE) return;
	bActHeld[FMath::Min(Post, 1)] = bDown;
	const float Now = ServerNow();
	AWRPlayerState* PS = Who->GetPlayerState<AWRPlayerState>();
	UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (IsTrebuchet() && Post == 1 && bDown)
	{
		if (Rep.State == 3) return;
		if (!Rep.bLoaded)
		{
			if (!Bag || Bag->CountOf(TEXT("siege_stone")) <= 0) { Tell(Who, TEXT("Load a siege stone (workbench: 25 stone)")); return; }
			Bag->RemoveItem(TEXT("siege_stone"), 1);
			Rep.bLoaded = true;
			WRFx::Broadcast(this, TEXT("hit.dust"), GetActorTransform().TransformPosition(FVector(-250.f, 0.f, 80.f)), FRotator::ZeroRotator, 0.6f, TEXT("siege.load"));
			Tell(Who, Rep.State == 2 ? TEXT("Loaded: Attack throws it") : TEXT("Loaded: wind the arm down at the winch, then throw"));
			ForceNetUpdate();
			return;
		}
		if (Rep.State != 2) { Tell(Who, TEXT("Wind it down first (the winch)")); return; }
		// The trigger pin is pulled: the weight falls.
		Rep.State = 3;
		Rep.StateAt = Now;
		ReleaseAt = Now + ReleaseTime(Rep.Range);
		WRFx::Broadcast(this, TEXT("hit.dust"), GetActorTransform().TransformPosition(FVector(0.f, 0.f, 70.f)), FRotator::ZeroRotator, 1.2f, TEXT("siege.release"));
		ForceNetUpdate();
		return;
	}
	if (IsBallista() && bDown && Rep.State == 0)
	{
		if (!Rep.bLoaded)
		{
			if (!Bag || Bag->CountOf(TEXT("ballista_bolt")) <= 0) { Tell(Who, TEXT("No bolts (workbench: ballista bolts)")); return; }
			Bag->RemoveItem(TEXT("ballista_bolt"), 1);
			Rep.bLoaded = true;
		}
		Rep.State = 1;
		Rep.StateAt = Now;
		WRFx::Broadcast(this, TEXT("hit.dust"), GetActorLocation() + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, 0.2f, TEXT("siege.windlass"));
		ForceNetUpdate();
		return;
	}
	if (IsBallista() && !bDown)
	{
		if (Rep.State == 1)
		{
			// Let go half-drawn: the string eases back (the bolt stays in the groove).
			Rep.Wind = BallistaDraw(Rep, Now);
			Rep.State = 0;
			Rep.StateAt = Now;
			ForceNetUpdate();
		}
		else if (Rep.State == 2)
		{
			// Drawn and let go: the aim line (0.9 s, everyone sees it), then the claw lets go.
			Rep.State = 3;
			Rep.StateAt = Now;
			WRFx::Broadcast(this, TEXT("hit.dust"), GetActorLocation() + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, 0.1f, TEXT("siege.tell"));
			ForceNetUpdate();
		}
	}
}

FString AWRSiegeEngine::PackBlock(const AActor* Who) const
{
	// From what every machine has (owner, health, crew, the last blow): the prompt says it before you hold, and the
	// server says the same if a hold is refused.
	const APawn* P = Cast<APawn>(Who);
	const APlayerState* PS = P ? P->GetPlayerState() : nullptr;
	if (!PS) return TEXT("Not now");
	const FString Me = PS->GetPlayerName();
	bool bMine = OwnerName == Me;
	if (!bMine && HasAuthority()) bMine = FWRSocialRules::CanShare(OwnerName, Me);
	if (!bMine && !HasAuthority())
	{
		// A client knows clans from the player states (an offline owner's clan isn't known: then it's not yours to pack).
		const AWRPlayerState* Mine = Cast<AWRPlayerState>(PS);
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (GS && Mine && !Mine->ClanName.IsEmpty())
		{
			for (const APlayerState* O : GS->PlayerArray)
			{
				const AWRPlayerState* WO = Cast<AWRPlayerState>(O);
				if (WO && WO->GetPlayerName() == OwnerName && WO->ClanName == Mine->ClanName) bMine = true;
			}
		}
	}
	if (!bMine) return FString::Printf(TEXT("It's %s's to pack up"), *OwnerName);
	const float Need = WRSiegeDetail::Tn(TEXT("packHealth"), 0.9f);
	if (Health < MaxHealth * Need)
	{
		return FString::Printf(TEXT("Too damaged to pack up (%d%%, needs %d%%): it would come apart"), FMath::RoundToInt(100.f * Health / FMath::Max(1.f, MaxHealth)), FMath::RoundToInt(100.f * Need));
	}
	for (const AWRCharacter* C : Crew) if (C && C != Who) return TEXT("Someone else is on it");
	if (ServerNow() - LastHitAt < WRSiegeDetail::Tn(TEXT("packQuietSeconds"), 5.f)) return TEXT("Under attack: not now");
	return FString();
}

FString AWRSiegeEngine::ServerPackBegin(AWRCharacter* Who)
{
	if (!Who) return FString();
	if (const FString Why = PackBlock(Who); !Why.IsEmpty()) return Why;
	if (PostOf(Who) == INDEX_NONE && FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) > WRSiegeDetail::Tn(TEXT("packReachCm"), 350.f) + 400.f)
	{
		return TEXT("Too far from it");
	}
	PackBy = Who;
	PackStartAt = ServerNow();
	return FString();
}

void AWRSiegeEngine::ServerPackCancel(AWRCharacter* Who)
{
	if (PackBy.Get() == Who)
	{
		PackBy = nullptr;
		PackStartAt = -100.f;
	}
}

FString AWRSiegeEngine::ServerPack(AWRCharacter* Who)
{
	// The hold's end: it began here, it lasted, nothing hit the engine or the packer meanwhile, and they're still at it.
	if (!Who || PackBy.Get() != Who) return TEXT("Hold Interact to pack it up");
	const float Now = ServerNow();
	const float Began = PackStartAt;
	PackBy = nullptr;
	PackStartAt = -100.f;
	if (Now - Began < WRSiegeDetail::Tn(TEXT("packHoldSeconds"), 1.5f) - 0.25f) return FString();   // let go early: nothing
	if (LastHitAt >= Began) return TEXT("It was hit: not packed");
	if (Who->HasTag(WRTags::Status_Staggered) || Who->HasTag(WRTags::State_CombatTagged)) return TEXT("In a fight: not packed");
	if (PostOf(Who) == INDEX_NONE && FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) > WRSiegeDetail::Tn(TEXT("packReachCm"), 350.f) + 550.f)
	{
		return TEXT("You stepped away: not packed");
	}
	if (const FString Why = PackBlock(Who); !Why.IsEmpty()) return Why;
	AWRPlayerState* PS = Who->GetPlayerState<AWRPlayerState>();
	UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (!Bag) return TEXT("Not now");
	if (PostOf(Who) != INDEX_NONE) ServerLeave(Who);
	Bag->AddItem(Kind, 1);
	if (Rep.bLoaded) Bag->AddItem(IsTrebuchet() ? FName(TEXT("siege_stone")) : FName(TEXT("ballista_bolt")), 1);
	WRFx::Broadcast(this, TEXT("hit.dust"), GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("siege.place"));
	UE_LOG(LogWildreach, Log, TEXT("SIEGE %s packed up %s"), *WRSiegeDetail::NameOf(Who), *Kind.ToString());
	Destroy();
	return FString::Printf(TEXT("Packed up: the %s is in your bag"), IsRam() ? TEXT("battering ram") : IsTrebuchet() ? TEXT("trebuchet") : TEXT("ballista"));
}

FString AWRSiegeEngine::ServerHarness(AWRCharacter* Who)
{
	if (!IsRam()) return FString();
	const FString Me = WRSiegeDetail::NameOf(Who);
	if (Hauler)
	{
		if (Hauler->TamedBy != Me && !FWRSocialRules::CanShare(Hauler->TamedBy, Me)) return TEXT("Someone else's hauler is in the harness");
		Hauler->HarnessedTo = nullptr;
		Hauler = nullptr;
		ForceNetUpdate();
		return TEXT("Hauler out of the harness");
	}
	// Your nearest hauler (role hauler) in earshot walks to the harness behind the ram.
	AWRCreature* Best = nullptr;
	float BestD = FMath::Square(WRSiegeDetail::Tn(TEXT("ram.haulerReach"), 2000.f));
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		const FWRCreatureDef* Def = It->GetDef();
		if (!It->IsAlive() || It->TamedBy != Me || It->GetRider() || !Def || Def->Role != TEXT("hauler") || It->IsBaby()) continue;
		const float D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (!Best) return TEXT("No hauler of yours in earshot to harness");
	Best->HarnessedTo = this;
	Best->SiegePiece = nullptr;
	Hauler = Best;
	ForceNetUpdate();
	return FString::Printf(TEXT("%s goes to the harness: it pushes with you"), *Best->GetDisplayLabel());
}

// ---------------------------------------------------------------- the server's step

void AWRSiegeEngine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Kind.IsNone()) return;
	if (HasAuthority()) ServerStep(DeltaSeconds);
	if (IsActorBeingDestroyed()) return;   // (fell apart or broke this step)
	ClientStep(DeltaSeconds);
}

void AWRSiegeEngine::ServerStep(float Dt)
{
	using namespace WRSiegeDetail;
	const float Now = ServerNow();
	if (Crew.Num() != PostCount()) Crew.SetNum(PostCount());
	// Crew who can't hold their post any more are off it (dead, staggered, frozen, gone).
	for (int32 i = 0; i < Crew.Num(); ++i)
	{
		AWRCharacter* C = Crew[i];
		if (!C) continue;
		if (!IsValid(C) || !C->IsAlive() || C->GetAttachParentActor() != this || C->HasTag(WRTags::Status_Staggered) || C->HasTag(WRTags::Status_Frozen))
		{
			if (IsValid(C)) ServerLeave(C, C->IsAlive());
			else Crew[i] = nullptr;
			continue;
		}
		LastCrewed = Now;
		// The trebuchet's crew ride its turntable round with it.
		const auto& P = PostsOf(Kind)[i];
		if (P.bTurns)
		{
			C->SetActorRelativeLocation(PostLocal(i));
			C->SetActorRelativeRotation(FRotator(0.f, P.Yaw + FRotator::NormalizeAxis(Rep.Aim - Rep.Heading), 0.f));
		}
	}
	// Left unattended long enough, an engine falls apart (no litter of abandoned engines).
	if (Now - LastCrewed > Tn(TEXT("abandonMinutes"), 120.f) * 60.f)
	{
		WRFx::Broadcast(this, TEXT("siege.break"), GetActorLocation() + FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, 1.2f, TEXT("siege.break"));
		Destroy();
		return;
	}
	if (Hauler && (!IsValid(Hauler) || !Hauler->IsAlive() || Hauler->HarnessedTo.Get() != this))
	{
		if (IsValid(Hauler) && Hauler->HarnessedTo.Get() == this) Hauler->HarnessedTo = nullptr;
		Hauler = nullptr;
	}
	WarnClock = FMath::Max(0.f, WarnClock - Dt);
	if (IsRam()) StepRam(Dt);
	else if (IsTrebuchet()) StepTrebuchet(Dt);
	else if (IsBallista()) StepBallista(Dt);
}

void AWRSiegeEngine::StepRam(float Dt)
{
	using namespace WRSiegeDetail;
	const float Now = ServerNow();
	// --- Pushing: a player at a handle is one push, a harnessed hauler two; two make full speed.
	int32 Pushers = 0, Forward = 0, Right = 0;
	for (int32 i = 0; i < 2 && i < Crew.Num(); ++i)
	{
		if (!Crew[i]) continue;
		++Pushers;
		Forward += MoveForward[i];
		Right += MoveRight[i];
	}
	Forward = FMath::Clamp(Forward, -1, 1);
	Right = FMath::Clamp(Right, -1, 1);
	const float Power = Pushers + (Hauler ? Num(TEXT("haulerPower"), 2.f) : 0.f);
	if (Pushers > 0 && (Forward != 0 || Right != 0))
	{
		const float Speed = Num(TEXT("pushSpeed"), 100.f) * FMath::Min(1.f, Power / FMath::Max(1.f, Num(TEXT("fullPower"), 2.f))) * Forward * (Forward < 0 ? 0.6f : 1.f);
		const float NewHeading = Rep.Heading + Right * Num(TEXT("turnRate"), 25.f) * Dt * (Forward == 0 ? 0.5f : 1.f);
		const FRotator Rot(0.f, NewHeading, 0.f);
		FVector Want = GetActorLocation() + Rot.Vector() * Speed * Dt;
		// The wheels on the ground: each traced, the body pitched to the front pair against the back pair.
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRamGround), false, this);
		for (const AWRCharacter* C : Crew) if (C) Q.AddIgnoredActor(C);
		if (Hauler) Q.AddIgnoredActor(Hauler);
		float Zs[4];
		bool bGround = true;
		int32 k = 0;
		for (const float X : { 150.f, -150.f }) for (const float Y : { 85.f, -85.f })
		{
			const FVector At = Want + Rot.RotateVector(FVector(X, Y, 0.f));
			FHitResult H;
			if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0.f, 0.f, 120.f), At - FVector(0.f, 0.f, 300.f), ECC_Visibility, Q)) Zs[k] = H.ImpactPoint.Z;
			else bGround = false;
			++k;
		}
		if (bGround)
		{
			const float Front = 0.5f * (Zs[0] + Zs[1]), Back = 0.5f * (Zs[2] + Zs[3]);
			const float Slope = FMath::RadiansToDegrees(FMath::Atan2(Front - Back, 300.f));
			Want.Z = 0.25f * (Zs[0] + Zs[1] + Zs[2] + Zs[3]);
			FVector Half, Centre;
			BoxOf(Kind, Half, Centre);
			const FQuat BodyQ = FRotator(Slope, NewHeading, 0.f).Quaternion();
			// Nothing in the way (a wall, a player, a creature): it never passes through anything.
			FCollisionObjectQueryParams Objects;
			Objects.AddObjectTypesToQuery(ECC_WorldStatic);
			Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
			Objects.AddObjectTypesToQuery(ECC_Pawn);
			const FVector From = GetActorLocation() + GetActorQuat().RotateVector(Centre) + FVector(0.f, 0.f, 25.f);
			const FVector To = Want + BodyQ.RotateVector(Centre) + FVector(0.f, 0.f, 25.f);
			const bool bBlocked = GetWorld()->SweepTestByObjectType(From, To, BodyQ, Objects, FCollisionShape::MakeBox(Half - FVector(4.f, 4.f, 30.f)), Q);
			if (FMath::Abs(Slope) > Num(TEXT("maxSlopeDeg"), 20.f))
			{
				if (WarnClock <= 0.f) { for (AWRCharacter* C : Crew) if (C) Tell(C, TEXT("Too steep for the ram")); WarnClock = 3.f; }
			}
			else if (!bBlocked)
			{
				SetActorLocationAndRotation(Speed == 0.f ? GetActorLocation() : Want, FRotator(0.f, NewHeading, 0.f));
				Rep.Loc = GetActorLocation();
				Rep.Heading = NewHeading;
				Rep.Slope = Slope;
			}
		}
	}
	// --- The swing: hold Attack to draw the log back on its chains, let go and it swings through.
	bool bAnyHeld = false;
	for (int32 i = 0; i < 2 && i < Crew.Num(); ++i) if (Crew[i] && bActHeld[i]) bAnyHeld = true;
	const float SinceState = Now - Rep.StateAt;
	if (bAnyHeld && (Rep.State == 0 || (Rep.State == 2 && SinceState > Num(TEXT("redrawAfter"), 1.5f))))
	{
		Rep.Wind = RamAngle(Rep, Now) / FMath::Max(1.f, Num(TEXT("drawDeg"), 35.f));   // from where it hangs now
		Rep.State = 1;
		Rep.StateAt = Now;
		WRFx::Broadcast(this, TEXT("hit.dust"), GetActorLocation() + FVector(0.f, 0.f, 240.f), FRotator::ZeroRotator, 0.2f, TEXT("siege.ram_draw"));
		ForceNetUpdate();
	}
	else if (!bAnyHeld && Rep.State == 1)
	{
		Rep.Wind = FMath::Clamp(-RamAngle(Rep, Now) / FMath::Max(1.f, Num(TEXT("drawDeg"), 35.f)), 0.f, 1.f);
		Rep.State = 2;
		Rep.StateAt = Now;
		StrikeAt = Now + 0.25f * Num(TEXT("period"), 1.6f);
		StruckThisSwing.Reset();
		ForceNetUpdate();
	}
	else if (Rep.State == 2 && SinceState > Num(TEXT("settleSeconds"), 3.f))
	{
		Rep.State = 0;
		Rep.StateAt = Now;
	}
	if (StrikeAt > 0.f && Now >= StrikeAt)
	{
		StrikeAt = -1.f;
		// The cap at the bottom of the arc: a door or gate there takes the blow (by how far the log was drawn).
		const FTransform BodyT(FRotator(Rep.Slope, Rep.Heading, 0.f), GetActorLocation());
		const FVector Cap = BodyT.TransformPosition(FVector(255.f, 0.f, WRSiegeLook::RamHookZ() - WRSiegeLook::RamChainLength() - WRSiegeLook::RamEyeZ()));
		TArray<FOverlapResult> Over;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRamCap), false, this);
		GetWorld()->OverlapMultiByChannel(Over, Cap + BodyT.GetRotation().GetForwardVector() * 30.f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Num(TEXT("capReachCm"), 60.f)), Q);
		AWRBuildPiece* Door = nullptr;
		bool bWall = false;
		for (const FOverlapResult& O : Over)
		{
			AWRBuildPiece* P = FWRRaiding::PieceOf(O.GetActor());
			const FWRPieceDef* D = P && !P->bGhost ? P->GetDef() : nullptr;
			if (!D) continue;
			const bool bDoorish = D->Kind == TEXT("door") || D->Shape == TEXT("door") || D->Shape == TEXT("gate");
			if (bDoorish) { Door = P; break; }
			bWall = true;
		}
		// The ram's crew: the last to let go answers for it (their clan's bell rings on the far side).
		FString Raider;
		for (int32 i = 0; i < 2 && i < Crew.Num(); ++i) if (Crew[i]) Raider = NameOf(Crew[i]);
		const float Mult = BuiltMult;
		const float Drawn = Rep.Wind;   // how far it was drawn: the blow's share
		// Something there stops the cap: the log bounces back off it instead of swinging on through.
		if (Door || bWall)
		{
			Rep.Wind = FMath::Clamp(Rep.Wind * Num(TEXT("bounce"), 1.5f), 0.f, 2.f);
			Rep.StateAt = Now - 0.75f * Num(TEXT("period"), 1.6f);
			ForceNetUpdate();
		}
		if (Door && !Raider.IsEmpty())
		{
			FString Why;
			if (!FWRRaiding::MayRaid(Door, Raider, &Why)) { for (AWRCharacter* C : Crew) if (C) Tell(C, Why); }
			else
			{
				FWRRaiding::DamagePiece(Door, Num(TEXT("damage"), 250.f) * Drawn * Mult, EWRRaidSource::Ram, this, Raider, NAME_None);
				const FWRPieceDef* D = Door->GetDef();
				const bool bWood = D && D->Material == TEXT("wood");
				WRFx::Broadcast(this, bWood ? TEXT("gather.wood") : TEXT("hit.dust"), Cap, BodyT.Rotator(), 1.6f, TEXT("siege.ram_hit"));
				WRFx::Shake(this, 0.2f, Cap, 1500.f);
			}
		}
		else if (bWall)
		{
			WRFx::Broadcast(this, TEXT("hit.dust"), Cap, BodyT.Rotator(), 1.2f, TEXT("siege.ram_hit"));
			if (WarnClock <= 0.f) { for (AWRCharacter* C : Crew) if (C) Tell(C, TEXT("A ram is for doors and gates: walls need charges or stones")); WarnClock = 4.f; }
		}
	}
	// A hauler that falls far behind its harness slips out of it.
	if (Hauler && FVector::Dist2D(Hauler->GetActorLocation(), GetActorTransform().TransformPosition(FVector(Num(TEXT("harnessX"), -520.f), 0.f, 0.f))) > Num(TEXT("haulerSlipCm"), 450.f) + 300.f)
	{
		for (AWRCharacter* C : Crew) if (C) Tell(C, TEXT("The hauler slipped out of its harness"));
		Hauler->HarnessedTo = nullptr;
		Hauler = nullptr;
	}
}

void AWRSiegeEngine::StepTrebuchet(float Dt)
{
	using namespace WRSiegeDetail;
	const float Now = ServerNow();
	// The winch: held Attack winds the arm down (the ratchet holds it when let go); wound full it's cocked on the trigger.
	if (Crew.IsValidIndex(0) && Crew[0] && bActHeld[0] && (Rep.State == 0 || Rep.State == 1))
	{
		Rep.State = 1;
		Rep.Wind = FMath::Min(1.f, Rep.Wind + Dt / FMath::Max(0.5f, Num(TEXT("winchSeconds"), 10.f)));
		if (Rep.Wind >= 1.f)
		{
			Rep.State = 2;
			Rep.StateAt = Now;
			WRFx::Broadcast(this, TEXT("hit.dust"), GetActorTransform().TransformPosition(FVector(-480.f, 0.f, 70.f)), FRotator::ZeroRotator, 0.5f, TEXT("siege.cocked"));
			if (Crew.IsValidIndex(1) && Crew[1]) Tell(Crew[1], Rep.bLoaded ? TEXT("Cocked and loaded: Attack throws") : TEXT("Cocked: load a stone"));
			Tell(Crew[0], TEXT("Cocked on the trigger"));
		}
	}
	// The aimer walks the upper frame round on its turntable.
	if (Crew.IsValidIndex(1) && Crew[1] && MoveRight[1] != 0 && Rep.State != 3)
	{
		Rep.Aim = FRotator::NormalizeAxis(Rep.Aim + MoveRight[1] * Num(TEXT("turnRate"), 10.f) * Dt);
	}
	if (Rep.State == 3)
	{
		if (ReleaseAt > 0.f && Now >= ReleaseAt)
		{
			ReleaseAt = -1.f;
			FVector From, Velocity;
			float Gravity = 1.f;
			AWRCharacter* Aimer = Crew.IsValidIndex(1) ? Crew[1].Get() : nullptr;
			if (Rep.bLoaded && ThrowPath(From, Velocity, Gravity))
			{
				FActorSpawnParameters P;
				P.Owner = this;
				P.Instigator = Aimer;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AWRProjectile* Stone = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Velocity.Rotation(), P))
				{
					FWRHit Hit;
					Hit.Instigator = Aimer ? static_cast<AActor*>(Aimer) : static_cast<AActor*>(this);
					Hit.Damage = Num(TEXT("people"), 80.f);
					Hit.PoiseDamage = 80.f;
					Hit.Impact = 4;
					Hit.Tier = EWRHitTier::Heavy;
					Hit.bUnblockable = true;
					Hit.bProjectile = true;
					Stone->InitLob(Hit, Velocity, Gravity, 0.f, TEXT("code:siegestone"));
					Stone->SetKind(TEXT("trebuchet"));
					Stone->Tags.Add(FName(*OwnerName));
					Stone->IgnoreActor(this);
				}
				UE_LOG(LogWildreach, Log, TEXT("SIEGE trebuchet of %s threw a stone (pin %d)"), *OwnerName, Rep.Range);
			}
			Rep.bLoaded = false;
			Rep.ShotAt = Now;
			ForceNetUpdate();
		}
		if (Now - Rep.StateAt > Num(TEXT("settleSeconds"), 4.f))
		{
			Rep.State = 0;
			Rep.Wind = 0.f;
			Rep.StateAt = Now;
			ForceNetUpdate();
		}
	}
}

void AWRSiegeEngine::StepBallista(float Dt)
{
	using namespace WRSiegeDetail;
	const float Now = ServerNow();
	// The stock follows the gunner's view on its swivel and trunnion, at its own pace; held still for the aim line.
	if (Rep.State != 3 && Crew.IsValidIndex(0) && Crew[0])
	{
		const float YawStep = Num(TEXT("yawRate"), 60.f) * Dt, PitchStep = Num(TEXT("pitchRate"), 30.f) * Dt;
		Rep.Aim = FRotator::NormalizeAxis(Rep.Aim + FMath::Clamp(FRotator::NormalizeAxis(AimYawWant - Rep.Aim), -YawStep, YawStep));
		Rep.AimPitch += FMath::Clamp(AimPitchWant - Rep.AimPitch, -PitchStep, PitchStep);
	}
	if (Rep.State == 1 && Now - Rep.StateAt >= Num(TEXT("drawSeconds"), 2.f))
	{
		Rep.State = 2;
		Rep.StateAt = Now;
		ForceNetUpdate();
	}
	if (Rep.State == 3 && Now - Rep.StateAt >= Num(TEXT("aimLineSeconds"), 0.9f))
	{
		// The claw lets go: a straight bolt, no drop, never homing.
		AWRCharacter* Gunner = Crew.IsValidIndex(0) ? Crew[0].Get() : nullptr;
		const FRotator Dir(Rep.AimPitch, Rep.Aim, 0.f);
		const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, WRSiegeLook::BalSwivelZ() + WRSiegeLook::BalTrunnionZ()) + Dir.Vector() * 140.f;
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = Gunner;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWRProjectile* Bolt = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Muzzle, Dir, P))
		{
			FWRHit Hit;
			Hit.Instigator = Gunner ? static_cast<AActor*>(Gunner) : static_cast<AActor*>(this);
			Hit.Damage = Num(TEXT("damage"), 220.f) * BuiltMult;
			Hit.PoiseDamage = 120.f;
			Hit.Impact = 3;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.bProjectile = true;
			Bolt->InitPhysical(Hit, Dir.Vector() * Num(TEXT("speed"), 6000.f), 0.f, TEXT("code:ballistabolt"), TEXT("+X"), TEXT("ballista"), NAME_None);
			Bolt->IgnoreActor(this);
		}
		Rep.bLoaded = false;
		Rep.State = 0;
		Rep.StateAt = Now;
		Rep.ShotAt = Now;
		Rep.Wind = 0.f;
		WRFx::Broadcast(this, TEXT("hit.dust"), Muzzle, Dir, 0.4f, TEXT("siege.ballista"));
		ForceNetUpdate();
	}
}

bool AWRSiegeEngine::ThrowPath(FVector& OutFrom, FVector& OutVelocity, float& OutGravity) const
{
	using namespace WRSiegeDetail;
	if (!IsTrebuchet()) return false;
	// The sling lets go at the arm's tip when the arm passes the pin's angle; the stone goes along the turntable's
	// heading to the pin's range, under real gravity (never homing).
	const float Alpha = FMath::DegreesToRadians(ReleaseDeg(Rep.Range));
	const FRotator Turn(0.f, Rep.Aim, 0.f);
	const FVector Pivot = GetActorLocation() + FVector(0.f, 0.f, WRSiegeLook::TrebAxleZ());
	const FVector LongDir = Turn.RotateVector(FVector(-FMath::Cos(Alpha), 0.f, FMath::Sin(Alpha)));
	OutFrom = Pivot + LongDir * (WRSiegeLook::TrebLong() + WRSiegeLook::TrebSlingLength() * 0.6f);
	const float Range = Num(*FString::Printf(TEXT("range%d"), Rep.Range), 2000.f + 2000.f * Rep.Range);
	FVector To = GetActorLocation() + Turn.Vector() * Range;
	// Land on the ground there (the arc's end is the ground under the range, traced on the server and every client the same).
	FHitResult G;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTrebAim), false, this);
	if (GetWorld()->LineTraceSingleByChannel(G, To + FVector(0.f, 0.f, 3000.f), To - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Q)) To = G.ImpactPoint;
	OutGravity = Tn(TEXT("trebuchet.gravity"), 1.f);
	OutVelocity = AWRProjectile::LobVelocity(OutFrom, To, Num(TEXT("speed"), 2200.f), OutGravity);
	return true;
}

// ---------------------------------------------------------------- every machine: the model and its motion

void AWRSiegeEngine::Build()
{
	if (bBuilt || GetNetMode() == NM_DedicatedServer) return;
	bBuilt = true;
	using namespace WRSiegeLook;
	Body = NewObject<USceneComponent>(this, TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->RegisterComponent();
	const auto Make = [this](const FWRToolMesh& M, const TCHAR* Name)
	{
		UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this, Name);
		P->SetupAttachment(Body);
		P->RegisterComponent();
		M.ApplyTo(P);
		return P;
	};
	if (IsRam())
	{
		Frame = Make(RamFrame(), TEXT("RamFrame"));
		for (int32 i = 0; i < 4; ++i) Parts.Add(Make(RamWheel(), *FString::Printf(TEXT("Wheel%d"), i)));
		Parts.Add(Make(RamChain(RamChainLength()), TEXT("ChainFront")));
		Parts.Add(Make(RamChain(RamChainLength()), TEXT("ChainBack")));
		Parts.Add(Make(RamLog(), TEXT("Log")));
	}
	else if (IsTrebuchet())
	{
		Frame = Make(TrebBase(), TEXT("TrebBase"));
		Parts.Add(Make(TrebUpper(), TEXT("Upper")));        // 0
		Parts.Add(Make(TrebDrum(), TEXT("Drum")));          // 1
		Parts.Add(Make(TrebArm(), TEXT("Arm")));            // 2
		Parts.Add(Make(TrebPin(Rep.Range), TEXT("Pin")));   // 3
		Parts.Add(Make(TrebWeight(), TEXT("Weight")));      // 4
		Parts.Add(Make(TrebPawl(), TEXT("Pawl")));          // 5
		Parts.Add(Make(Rope(1.6f), TEXT("Sling")));         // 6
		Parts.Add(Make(Pouch(), TEXT("Pouch")));            // 7
		Parts.Add(Make(Stone(), TEXT("Stone")));            // 8
		Parts.Add(Make(Rope(1.8f), TEXT("WinchRope")));     // 9
		ShownPin = Rep.Range;
	}
	else
	{
		Frame = Make(BalTripod(), TEXT("Tripod"));
		Parts.Add(Make(BalYoke(), TEXT("Yoke")));           // 0
		Parts.Add(Make(BalStock(), TEXT("Stock")));         // 1
		Parts.Add(Make(BalLeaf(true), TEXT("LeafL")));      // 2
		Parts.Add(Make(BalLeaf(false), TEXT("LeafR")));     // 3
		Parts.Add(Make(BalWindlass(), TEXT("Windlass")));   // 4
		Parts.Add(Make(BalSlider(), TEXT("Slider")));       // 5
		Parts.Add(Make(Rope(0.9f), TEXT("StringL")));       // 6
		Parts.Add(Make(Rope(0.9f), TEXT("StringR")));       // 7
		Parts.Add(Make(Bolt(), TEXT("Bolt")));              // 8
	}
}

void AWRSiegeEngine::ClientStep(float Dt)
{
	using namespace WRSiegeDetail;
	using namespace WRSiegeLook;
	const float Now = ServerNow();
	// Where it stands: the server's own; everyone else eases to the last word from the server (no net-rate steps).
	const FVector OldLoc = ShownLoc;
	if (HasAuthority())
	{
		ShownLoc = GetActorLocation();
		ShownHeading = Rep.Heading;
		ShownSlope = Rep.Slope;
		ShownAim = Rep.Aim;
		ShownAimPitch = Rep.AimPitch;
	}
	else if (!Rep.Loc.IsZero())
	{
		ShownLoc = FMath::VInterpTo(ShownLoc, FVector(Rep.Loc), Dt, 6.f);
		ShownHeading = ShownHeading + FMath::FInterpTo(0.f, FRotator::NormalizeAxis(Rep.Heading - ShownHeading), Dt, 6.f);
		ShownSlope = FMath::FInterpTo(ShownSlope, Rep.Slope, Dt, 6.f);
		ShownAim = ShownAim + FMath::FInterpTo(0.f, FRotator::NormalizeAxis(Rep.Aim - ShownAim), Dt, 8.f);
		ShownAimPitch = FMath::FInterpTo(ShownAimPitch, Rep.AimPitch, Dt, 8.f);
		SetActorLocationAndRotation(ShownLoc, FRotator(0.f, ShownHeading, 0.f));
	}
	// Crew on a turning post (the trebuchet's turntable, the ballista's stock) ride round with what this machine shows,
	// every frame, not at the net rate.
	if (!HasAuthority())
	{
		const auto& Posts = PostsOf(Kind);
		for (int32 i = 0; i < Crew.Num() && i < Posts.Num(); ++i)
		{
			AWRCharacter* C = Crew[i];
			if (!C || !Posts[i].bTurns || C->GetAttachParentActor() != this) continue;
			C->SetActorRelativeLocation(PostLocal(i));
			C->SetActorRelativeRotation(FRotator(0.f, Posts[i].Yaw + FRotator::NormalizeAxis(ShownAim - ShownHeading), 0.f));
		}
	}
	// Winding eases toward the server's figure (it changes ten times a second at most).
	// (After a throw the arm has rocked back to rest by itself: the winding starts again from nothing, not from cocked.)
	if (Rep.State == 0 && LastState == 3) ShownWind = 0.f;
	ShownWind = Rep.State == 3 ? Rep.Wind : FMath::FInterpConstantTo(ShownWind, Rep.Wind, Dt, 0.6f);
	if (GetNetMode() == NM_DedicatedServer || !bBuilt || !Body) return;

	// Far away, nothing moves visibly enough to spend the frame on (but the arc and the aim line still show).
	Body->SetRelativeRotation(FRotator(ShownSlope, 0.f, 0.f));
	if (IsRam())
	{
		// Wheels turn by the ground they've covered (angle = distance / radius), never slide.
		const float Moved = float(FVector::DotProduct(ShownLoc - OldLoc, GetActorForwardVector()));
		if (bShownInit && FMath::Abs(Moved) < 200.f)
		{
			WheelTurn += FMath::RadiansToDegrees(Moved / RamWheelRadius());
			WheelSoundClock += FMath::Abs(Moved);
			if (WheelSoundClock > 120.f) { WheelSoundClock = 0.f; WRSound::PlayAt(this, TEXT("siege.wheel"), GetActorLocation()); }
		}
		int32 w = 0;
		for (const float X : { 150.f, -150.f }) for (const float Y : { 92.f, -92.f })
		{
			Parts[w++]->SetRelativeLocationAndRotation(FVector(X, Y, RamWheelRadius()), FRotator(-WheelTurn, 0.f, 0.f));
		}
		// The log on its two parallel chains: the chains swing round their hooks, the log stays level (a parallelogram).
		const float Theta = RamAngle(Rep, Now);
		const float L = RamChainLength();
		const float Rad = FMath::DegreesToRadians(Theta);
		for (int32 c = 0; c < 2; ++c)
		{
			const float X = c == 0 ? RamChainX() : -RamChainX();
			Parts[4 + c]->SetRelativeLocationAndRotation(FVector(X, 0.f, RamHookZ()), FRotator(Theta, 0.f, 0.f));
		}
		Parts[6]->SetRelativeLocation(FVector(L * FMath::Sin(Rad), 0.f, RamHookZ() - L * FMath::Cos(Rad) - RamEyeZ()));
	}
	else if (IsTrebuchet())
	{
		const FTransform TurnT(FRotator(0.f, FRotator::NormalizeAxis(ShownAim - ShownHeading), 0.f));
		const auto Place = [&TurnT](UProceduralMeshComponent* P, const FVector& Local, const FRotator& Rot)
		{
			const FTransform T = FTransform(Rot, Local) * TurnT;
			P->SetRelativeTransform(T);
		};
		const auto RopeBetween = [&TurnT](UProceduralMeshComponent* P, const FVector& A, const FVector& B, bool bShow)
		{
			P->SetVisibility(bShow);
			if (!bShow) return;
			const FVector D = B - A;
			const FTransform T = FTransform(D.Rotation(), A, FVector(FMath::Max(1.f, D.Size()), 1.f, 1.f)) * TurnT;
			P->SetRelativeTransform(T);
		};
		Place(Parts[0], FVector::ZeroVector, FRotator::ZeroRotator);
		// The winch drum turns with the winding (six turns from up to cocked); the pawl's nose rides each tooth and drops.
		const float DrumDeg = ShownWind * 360.f * Num(TEXT("drumTurns"), 6.f);
		Place(Parts[1], TrebDrumAt(), FRotator(-DrumDeg, 0.f, 0.f));
		const float Tooth = FMath::Fmod(DrumDeg, 15.f) / 15.f;
		Place(Parts[5], TrebPawlAt(), FRotator(-8.f * Tooth, 180.f, 0.f));
		if (FMath::Abs(DrumDeg - LastDrumClick) >= 15.f) { LastDrumClick = DrumDeg; WRSound::PlayAt(this, TEXT("siege.winch"), GetActorTransform().TransformPosition(TrebDrumAt())); }
		if (Rep.State == 0 && ShownWind < 0.01f) LastDrumClick = 0.f;
		// The arm on its axle; the counterweight hangs plumb from its pin, swinging a little behind the arm's motion.
		const float Alpha = ArmAngle(Rep, Now, ShownWind);
		const FVector Pivot(0.f, 0.f, TrebAxleZ());
		Place(Parts[2], Pivot, FRotator(-Alpha, 0.f, 0.f));
		const float AR = FMath::DegreesToRadians(Alpha);
		const FVector LongDir(-FMath::Cos(AR), 0.f, FMath::Sin(AR));
		const FVector Tip = Pivot + LongDir * TrebLong();
		const FVector WeightPin = Pivot - LongDir * (TrebShort() - 10.f);
		const float Swing = FMath::Clamp((ArmAngle(Rep, Now + 0.05f, ShownWind) - Alpha) * Num(TEXT("weightLag"), 1.4f), -25.f, 25.f);
		Place(Parts[4], WeightPin, FRotator(Swing, 0.f, 0.f));
		// The release pin at the tip, bent for the range picked (rebuilt only when the pin changes).
		if (ShownPin != Rep.Range) { ShownPin = Rep.Range; TrebPin(Rep.Range).ApplyTo(Parts[3]); }
		Place(Parts[3], Tip, FRotator(-Alpha, 0.f, 0.f));
		// The sling: cocked, it lies along the trough toward the front; thrown, it whips round behind the arm and lets go
		// at the pin's angle; after, it hangs from the tip and swings out.
		FVector Pouch;
		const float Sling = TrebSlingLength();
		if (Rep.State == 3)
		{
			const float T = Now - Rep.StateAt, Tr = ReleaseTime(Rep.Range);
			if (T < Tr)
			{
				const float S = T / FMath::Max(0.05f, Tr);
				const float Beta = AR + PI * (1.f - S);   // trailing half a turn behind, coming round in line with the arm
				Pouch = Tip + FVector(-FMath::Cos(Beta), 0.f, FMath::Sin(Beta)) * Sling;
			}
			else
			{
				const float U = T - Tr;
				const float Hang = FMath::DegreesToRadians(-90.f + 70.f * FMath::Cos(4.f * U) * FMath::Exp(-1.2f * U));
				Pouch = Tip + FVector(FMath::Cos(Hang), 0.f, FMath::Sin(Hang)) * Sling;
			}
		}
		else
		{
			// Lying back toward the trough from the tip (cocked: along the trough; up: hanging under the tip).
			const FVector Trough(Tip.X + Sling * ShownWind, 0.f, FMath::Lerp(Tip.Z - Sling, 74.f, ShownWind));
			Pouch = Tip + (Trough - Tip).GetSafeNormal() * Sling;
		}
		RopeBetween(Parts[6], Tip, Pouch, true);
		Place(Parts[7], Pouch, (Pouch - Tip).Rotation());
		const bool bStoneIn = Rep.bLoaded && !(Rep.State == 3 && Now - Rep.StateAt >= ReleaseTime(Rep.Range));
		Parts[8]->SetVisibility(bStoneIn);
		if (bStoneIn) Place(Parts[8], Pouch + FVector(0.f, 0.f, 22.f), FRotator::ZeroRotator);
		// The winch rope from the drum to the arm's hook while it's wound (slack and stowed once thrown or up).
		const FVector Hook = Pivot + LongDir * (TrebLong() - 40.f) + FVector(0.f, 0.f, -11.f);
		RopeBetween(Parts[9], TrebDrumAt() + FVector(0.f, 0.f, 22.f), Hook, Rep.State == 1 || Rep.State == 2);
		// The release's whoosh and the frame's knock when the arm overshoots.
		if (Rep.State == 3 && LastState != 3) WRSound::PlayAt(this, TEXT("siege.release"), GetActorLocation() + FVector(0.f, 0.f, 300.f));
		DrawArc();
	}
	else
	{
		const float Draw = BallistaDraw(Rep, Now);
		const float YawRel = FRotator::NormalizeAxis(ShownAim - ShownHeading);
		const FTransform Yoke(FRotator(0.f, YawRel, 0.f), FVector(0.f, 0.f, BalSwivelZ()));
		Parts[0]->SetRelativeTransform(Yoke);
		const FTransform Stock = FTransform(FRotator(ShownAimPitch, 0.f, 0.f), FVector(0.f, 0.f, BalTrunnionZ())) * Yoke;
		Parts[1]->SetRelativeTransform(Stock);
		// The leaves bend back as it's drawn (their tips come toward the gunner), spring forward on the loose.
		const float Bend = Draw * Num(TEXT("leafBendDeg"), 22.f);
		for (int32 s = 0; s < 2; ++s)
		{
			const bool bLeft = s == 0;
			Parts[2 + s]->SetRelativeTransform(FTransform(FRotator(0.f, bLeft ? Bend : -Bend, 0.f), BalLeafRoot(bLeft)) * Stock);
		}
		const float WindDeg = Draw * 360.f * Num(TEXT("windlassTurns"), 4.f);
		Parts[4]->SetRelativeTransform(FTransform(FRotator(-WindDeg, 0.f, 0.f), BalWindlassAt()) * Stock);
		const float SliderX = FMath::Lerp(BalSliderRest(), BalSliderDrawn(), Draw);
		Parts[5]->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(SliderX, 0.f, 0.f)) * Stock);
		// The string from each nock to the claw.
		const FVector Claw(SliderX + 14.f, 0.f, 12.f);
		for (int32 s = 0; s < 2; ++s)
		{
			const bool bLeft = s == 0;
			const float B = FMath::DegreesToRadians(bLeft ? Bend : -Bend);
			const FVector Root2 = BalLeafRoot(bLeft);
			const FVector Nock = Root2 + FVector(10.f * FMath::Cos(B) - (bLeft ? 1.f : -1.f) * (BalLeafLength() - 2.f) * FMath::Sin(B), (bLeft ? 1.f : -1.f) * (BalLeafLength() - 2.f) * FMath::Cos(B) + 10.f * FMath::Sin(B), 0.f);
			const FVector D = Claw - Nock;
			Parts[6 + s]->SetRelativeTransform(FTransform(D.Rotation(), Nock, FVector(FMath::Max(1.f, D.Size()), 1.f, 1.f)) * Stock);
		}
		Parts[8]->SetVisibility(Rep.bLoaded);
		if (Rep.bLoaded) Parts[8]->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(SliderX + 70.f, 0.f, 11.f)) * Stock);
		// The windlass's ratchet while drawing; the aim line for the 0.9 s before the loose.
		if (Rep.State == 1 && FMath::Abs(WindDeg - LastDrumClick) >= 30.f) { LastDrumClick = WindDeg; WRSound::PlayAt(this, TEXT("siege.windlass"), GetActorLocation() + FVector(0.f, 0.f, 150.f)); }
		if (Rep.State != 1) LastDrumClick = WindDeg;
		const FRotator Dir(ShownAimPitch, ShownAim, 0.f);
		const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, BalSwivelZ() + BalTrunnionZ()) + Dir.Vector() * 140.f;
		FVector End = Muzzle + Dir.Vector() * Num(TEXT("aimLineCm"), 6000.f);
		if (Rep.State == 3)
		{
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBallistaLine), false, this);
			for (const AWRCharacter* C : Crew) if (C) Q.AddIgnoredActor(C);
			if (GetWorld()->LineTraceSingleByChannel(H, Muzzle, End, ECC_Visibility, Q)) End = H.ImpactPoint;
		}
		DrawAimLine(Rep.State == 3, Muzzle, End);
	}
	LastState = Rep.State;
}

void AWRSiegeEngine::DrawArc()
{
	// The stone's whole flight drawn for everyone near while it's loaded and cocked and while it flies: no surprise
	// shots (2.21.5). The same path the server throws on.
	const bool bShow = (Rep.bLoaded && Rep.State == 2) || (Rep.State == 3 && ServerNow() - Rep.ShotAt < 4.f && Rep.ShotAt >= Rep.StateAt);
	if (!bShow) return;
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	const APawn* Me = Local ? Local->GetPawn() : nullptr;
	if (!Me || FVector::DistSquared(Me->GetActorLocation(), GetActorLocation()) > FMath::Square(WRSiegeDetail::Tn(TEXT("trebuchet.arcSeenCm"), 15000.f))) return;
	ArcClock -= GetWorld()->GetDeltaSeconds();
	if (ArcClock > 0.f) return;
	ArcClock = 0.06f;
	FVector P, V;
	float G = 1.f;
	if (!ThrowPath(P, V, G)) return;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTrebArc), false, this);
	const float Step = 0.05f;
	int32 Dots = 0;
	for (float T = 0.f; T < 8.f; T += Step)
	{
		const FVector N = P + V * Step + FVector(0.f, 0.f, -0.5f * 980.f * G * Step * Step);
		FHitResult Hit;
		if (T > 0.3f && GetWorld()->LineTraceSingleByChannel(Hit, P, N, ECC_Visibility, Q))
		{
			WRFx::SpawnAt(this, TEXT("siege.arc_land"), Hit.ImpactPoint + Hit.ImpactNormal * 4.f, Hit.ImpactNormal.Rotation(), 1.f);
			break;
		}
		V.Z -= 980.f * G * Step;
		P = N;
		if (++Dots % 2 == 0) WRFx::SpawnAt(this, TEXT("siege.arc"), P, FRotator::ZeroRotator, 1.f);
	}
}

void AWRSiegeEngine::DrawAimLine(bool bShow, const FVector& From, const FVector& To)
{
	if (!AimLine && bShow)
	{
		AimLine = NewObject<UStaticMeshComponent>(this, TEXT("AimLine"));
		AimLine->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimLine->SetCastShadow(false);
		AimLine->SetUsingAbsoluteLocation(true);
		AimLine->SetUsingAbsoluteRotation(true);
		AimLine->SetUsingAbsoluteScale(true);
		AimLine->SetupAttachment(Root);
		AimLine->RegisterComponent();
		if (UMaterialInterface* Glow = WRSiegeDetail::GlowMaterial())
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Glow, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.05f, 0.03f));
			MID->SetScalarParameterValue(TEXT("Glow"), 4.f);
			AimLine->SetMaterial(0, MID);
		}
	}
	if (!AimLine) return;
	AimLine->SetVisibility(bShow);
	if (!bShow) return;
	const FVector D = To - From;
	const float Len = D.Size();
	const float Grow = FMath::Clamp((ServerNow() - Rep.StateAt) / FMath::Max(0.1f, Num(TEXT("aimLineSeconds"), 0.9f)), 0.f, 1.f);
	AimLine->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(D / FMath::Max(1.f, Len)).Rotator());
	AimLine->SetWorldScale3D(FVector(0.012f + 0.012f * Grow, 0.012f + 0.012f * Grow, Len / 100.f));
}

// ================================================================================================ FWRSiege

AWRSiegeEngine* FWRSiege::CrewingOf(const AActor* Who)
{
	AWRSiegeEngine* E = Who ? Cast<AWRSiegeEngine>(Who->GetAttachParentActor()) : nullptr;
	return E && E->PostOf(Who) != INDEX_NONE ? E : nullptr;
}

AWRSiegeEngine* FWRSiege::NearOf(const AActor* Who, float Reach)
{
	if (!Who) return nullptr;
	AWRSiegeEngine* Best = nullptr;
	float BestD = FMath::Square(Reach);
	for (TActorIterator<AWRSiegeEngine> It(Who->GetWorld()); It; ++It)
	{
		for (int32 i = 0; i < It->PostCount(); ++i)
		{
			const float D = FVector::DistSquared(It->GetActorTransform().TransformPosition(It->PostLocal(i)), Who->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
	}
	return Best;
}

FString FWRSiege::UseItem(AWRPlayerState* PS, FName ItemId)
{
	using namespace WRSiegeDetail;
	AWRCharacter* Who = PS ? Cast<AWRCharacter>(PS->GetPawn()) : nullptr;
	UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (!Who || !Who->IsAlive() || !Bag || Bag->CountOf(ItemId) <= 0) return TEXT("Not now");
	UWorld* World = Who->GetWorld();
	const bool bBallista = ItemId == TEXT("ballista");
	// Every world (Aaron 11-16, r-siege-pve): on a PvE world an engine can't touch players or their buildings, only the
	// NPC strongholds (FWRRaiding::MayRaid), so it's for practising sieges there.
	const FRotator Facing(0.f, Who->GetControlRotation().Yaw, 0.f);
	FVector Half, Centre;
	BoxOf(ItemId, Half, Centre);
	const FVector Want = Who->GetActorLocation() + Facing.Vector() * (Half.X + Tn(TEXT("placeGapCm"), 120.f));
	if (FWRSocialRules::IsInSafeZone(World, Want)) return TEXT("Not in a safe zone");
	if (WRBiome::IsUnderground(Who, Want)) return TEXT("Not underground: siege engines stand in the open");
	FHitResult Ground;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSiegePlace), false, Who);
	if (!World->LineTraceSingleByChannel(Ground, Want + FVector(0.f, 0.f, 200.f), Want - FVector(0.f, 0.f, 500.f), ECC_Visibility, Q)) return TEXT("No ground there");
	if (Ground.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(Tn(TEXT("placeSlopeDeg"), 20.f)))) return TEXT("Too steep");
	if (World->OverlapBlockingTestByChannel(Ground.ImpactPoint + Facing.RotateVector(Centre) + FVector(0.f, 0.f, 30.f), Facing.Quaternion(), ECC_Pawn, FCollisionShape::MakeBox(Half - FVector(10.f, 10.f, 40.f)), Q))
	{
		return TEXT("No room: it needs clear ground");
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRSiegeEngine* E = World->SpawnActorDeferred<AWRSiegeEngine>(AWRSiegeEngine::StaticClass(), FTransform(Facing, Ground.ImpactPoint), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E) return TEXT("Couldn't stand it there");
	const TCHAR* K = ItemId == TEXT("battering_ram") ? TEXT("ram") : ItemId == TEXT("trebuchet") ? TEXT("trebuchet") : TEXT("ballista");
	E->Kind = ItemId;
	E->OwnerName = PS->GetPlayerName();
	E->MaxHealth = E->Health = Tn(FString::Printf(TEXT("%s.health"), K), ItemId == TEXT("battering_ram") ? 2000.f : ItemId == TEXT("trebuchet") ? 1500.f : 800.f);
	E->RoofHealth = ItemId == TEXT("battering_ram") ? Tn(TEXT("ram.roofHealth"), 800.f) : 0.f;
	E->Rep.Loc = Ground.ImpactPoint;
	E->Rep.Heading = E->Rep.Aim = Facing.Yaw;
	E->Rep.Range = 1;
	// Siege engineer (Engineering 100): the engines you build hit harder, for good (saved with it).
	E->BuiltMult = 1.f + UWRLifeSkillsComponent::StatFor(Who, TEXT("siegeDamage"));
	E->FinishSpawning(FTransform(Facing, Ground.ImpactPoint));
	Bag->RemoveItem(ItemId, 1);
	WRFx::Broadcast(E, TEXT("hit.dust"), Ground.ImpactPoint, FRotator::ZeroRotator, 1.4f, TEXT("siege.place"));
	UE_LOG(LogWildreach, Log, TEXT("SIEGE %s stood a %s at %s"), *PS->GetPlayerName(), *ItemId.ToString(), *Ground.ImpactPoint.ToCompactString());
	return FString::Printf(TEXT("%s stands ready: Interact at a post to crew it"), ItemId == TEXT("battering_ram") ? TEXT("The ram") : ItemId == TEXT("trebuchet") ? TEXT("The trebuchet") : TEXT("The ballista"));
}

bool FWRSiege::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	if (const AWRSiegeEngine* On = CrewingOf(Me))
	{
		const int32 Post = On->PostOf(Me);
		OutTarget = On->PostName(Post);
		OutTarget[0] = FChar::ToUpper(OutTarget[0]);
		const FWRSiegeRep& R = On->Rep;
		// The step-off key names the device's own button; on your own engine the hold packs it up (r-packup).
		const FString Key = WRInput::ActionKey(TEXT("Interact")).ToString();
		const FString Why = On->PackBlock(Me);
		const FString Off = Why.StartsWith(TEXT("It's ")) ? FString::Printf(TEXT("%s: step off"), *Key) : FString::Printf(TEXT("%s: step off  ·  hold: pack up"), *Key);
		if (On->IsRam()) OutVerb = R.State == 1 ? FString(TEXT("Let go to swing")) : FString::Printf(TEXT("Push  ·  Attack: draw the log  ·  Block: hauler  ·  %s"), *Off);
		else if (On->IsTrebuchet() && Post == 0) OutVerb = R.State == 2 ? FString::Printf(TEXT("Cocked  ·  %s"), *Off) : FString::Printf(TEXT("Hold Attack: wind (%d%%)  ·  %s"), FMath::RoundToInt(R.Wind * 100.f), *Off);
		else if (On->IsTrebuchet()) OutVerb = FString::Printf(TEXT("Turn  ·  Pin %d m  ·  Attack: %s  ·  %s"), FMath::RoundToInt(WRSiegeDetail::Tn(FString::Printf(TEXT("trebuchet.range%d"), R.Range), 2000.f + 2000.f * R.Range) / 100.f),
			!R.bLoaded ? TEXT("load a stone") : R.State == 2 ? TEXT("throw") : TEXT("wind it first"), *Off);
		else OutVerb = R.State == 1 ? FString(TEXT("Drawing...")) : R.State == 2 ? FString(TEXT("Let go to loose")) : R.State == 3 ? FString(TEXT("Loosing")) : FString::Printf(TEXT("Look to aim  ·  Hold Attack: draw  ·  %s"), *Off);
		return true;
	}
	const AWRSiegeEngine* E = NearOf(Me);
	if (!E) return false;
	const int32 Post = E->FreePostNear(Me->GetActorLocation());
	OutTarget = FString::Printf(TEXT("%s (%s's, %d%%)"), E->IsRam() ? TEXT("Battering ram") : E->IsTrebuchet() ? TEXT("Trebuchet") : TEXT("Ballista"), *E->OwnerName, FMath::RoundToInt(100.f * E->Health / FMath::Max(1.f, E->MaxHealth)));
	OutVerb = Post == INDEX_NONE ? TEXT("Every post is taken") : FString::Printf(TEXT("Take %s"), *E->PostName(Post));
	// Your own: what the hold does, or why it can't now (said before you try).
	const FString Why = E->PackBlock(Me);
	if (Why.IsEmpty()) OutVerb += TEXT("  ·  Hold: pack up");
	else if (!Why.StartsWith(TEXT("It's "))) OutVerb += FString::Printf(TEXT("  ·  %s"), *Why);
	return true;
}

bool FWRSiege::ClientInteract(AWRPlayerController* PC)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	if (CrewingOf(Me)) { PC->SendCommand(TEXT("siege leave")); return true; }
	const AWRSiegeEngine* E = NearOf(Me);
	if (!E || E->FreePostNear(Me->GetActorLocation()) == INDEX_NONE) return false;
	PC->SendCommand(TEXT("siege take"));
	return true;
}

FString FWRSiege::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	AWRCharacter* Who = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!Who) return FString();
	const int32 Off = Args.Num() > 0 && Args[0] == TEXT("siege") ? 1 : 0;
	const FString Sub = Args.IsValidIndex(Off) ? Args[Off] : FString();
	const auto Arg = [&Args, Off](int32 i) { return Args.IsValidIndex(Off + i) ? Args[Off + i] : FString(); };
	AWRSiegeEngine* On = CrewingOf(Who);
	// Packing up beside an engine (not on a post): the nearest one in reach.
	if (!On && (Sub == TEXT("packbegin") || Sub == TEXT("pack") || Sub == TEXT("packcancel")))
	{
		AWRSiegeEngine* E = NearOf(Who, WRSiegeDetail::Tn(TEXT("packReachCm"), 350.f) + 200.f);
		if (!E) return Sub == TEXT("packcancel") ? FString() : FString(TEXT("No siege engine in reach"));
		if (Sub == TEXT("packbegin")) return E->ServerPackBegin(Who);
		if (Sub == TEXT("pack")) return E->ServerPack(Who);
		E->ServerPackCancel(Who);
		return FString();
	}
	if (Sub == TEXT("take"))
	{
		if (On) return FString();
		AWRSiegeEngine* E = NearOf(Who, 350.f);
		if (!E) return TEXT("No siege engine in reach");
		const int32 Post = E->FreePostNear(Who->GetActorLocation());
		if (Post == INDEX_NONE) return TEXT("Every post is taken");
		E->ServerTake(Who, Post);
		return FString();
	}
	if (!On) return FString();
	if (Sub == TEXT("leave")) { On->ServerLeave(Who); return FString(); }
	if (Sub == TEXT("move")) { On->ServerMove(Who, FCString::Atoi(*Arg(1)), FCString::Atoi(*Arg(2))); return FString(); }
	if (Sub == TEXT("act")) { On->ServerAct(Who, FCString::Atoi(*Arg(1)) != 0); return FString(); }
	if (Sub == TEXT("aim")) { On->ServerAim(Who, FCString::Atof(*Arg(1)), FCString::Atof(*Arg(2))); return FString(); }
	if (Sub == TEXT("packbegin")) return On->ServerPackBegin(Who);
	if (Sub == TEXT("pack")) return On->ServerPack(Who);
	if (Sub == TEXT("packcancel")) { On->ServerPackCancel(Who); return FString(); }
	if (Sub == TEXT("harness")) return On->ServerHarness(Who);
	return FString();
}

void FWRSiege::Splash(UWorld* World, const FVector& At, float Radius, float Amount, EWRRaidSource Source, AActor* Causer, const FString& Raider)
{
	if (!World || Raider.IsEmpty() || Amount <= 0.f) return;
	TArray<AWRSiegeEngine*> Hit;
	for (TActorIterator<AWRSiegeEngine> It(World); It; ++It)
	{
		if (*It == Causer || It->IsActorBeingDestroyed()) continue;
		const FBox B = It->GetComponentsBoundingBox(false);
		if (FVector::DistSquared(B.GetClosestPointTo(At), At) <= Radius * Radius) Hit.Add(*It);
	}
	for (AWRSiegeEngine* E : Hit)
	{
		// Over the ram's roof: the hides take it.
		const bool bAbove = At.Z > E->GetActorLocation().Z + 170.f;
		if (IsValid(E)) E->TakeHit(Amount, Causer, Source, Raider, bAbove);
	}
}

void FWRSiege::StoneLanded(AWRProjectile* Stone, const FVector& At, AActor* HitActor, AWRCharacterBase* Thrower)
{
	using namespace WRSiegeDetail;
	if (!Stone) return;
	UWorld* World = Stone->GetWorld();
	// The engine that threw it (if it still stands) says whose stone it is and how hard its builder makes it hit.
	const AWRSiegeEngine* From = Cast<AWRSiegeEngine>(Stone->GetOwner());
	const FString Owner = From ? From->OwnerName : Stone->Tags.Num() > 0 ? Stone->Tags[0].ToString() : FString();
	const FString Raider = Thrower ? NameOf(Thrower) : Owner;
	const float Mult = From ? From->BuiltMult : 1.f;
	const float Radius = Tn(TEXT("trebuchet.radiusCm"), 200.f);
	// The piece it struck takes the full blow; the others in its 2 m footprint half (a wall beside it cracks, it doesn't fall).
	AWRBuildPiece* Struck = FWRRaiding::PieceOf(HitActor);
	const float Full = Tn(TEXT("trebuchet.pieceDamage"), 500.f) * Mult;
	if (Struck) FWRRaiding::DamagePiece(Struck, Full, EWRRaidSource::Siege, Stone, Raider, NAME_None);
	FWRRaiding::SplashPieces(World, At, Radius, Full * Tn(TEXT("trebuchet.neighbourShare"), 0.5f), EWRRaidSource::Siege, Stone, Raider, NAME_None, Struck);
	// An engine struck square takes the rest of a full blow (the splash above gave it the neighbour's share).
	if (AWRSiegeEngine* E = Cast<AWRSiegeEngine>(HitActor)) E->TakeHit(Full * (1.f - Tn(TEXT("trebuchet.neighbourShare"), 0.5f)), Stone, EWRRaidSource::Siege, Raider, At.Z > E->GetActorLocation().Z + 170.f);
	// Everyone it lands on (people, creatures) in its footprint: 80 through ApplyHit, the thrower's clan spared.
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > Radius + It->GetSimpleCollisionRadius() + It->GetSimpleCollisionHalfHeight() * 0.5f) continue;
		const FString Them = NameOf(*It);
		if (!Them.IsEmpty() && !Owner.IsEmpty() && FWRSocialRules::CanShare(Owner, Them)) continue;
		// A PvE world: a stone never lands on a player (or a player's tame); the beasts in a stronghold's yard it does.
		if (!Them.IsEmpty() && WRTune(TEXT("server.pvp"), 0.f) <= 0.f) continue;
		FWRHit H = Stone->GetHit();
		H.Damage = Tn(TEXT("trebuchet.people"), 80.f) * Mult;
		H.Origin = At;
		H.ImpactPoint = It->GetActorLocation();
		UWRCombatLibrary::ApplyHit(*It, H);
	}
	WRFx::Broadcast(Stone, TEXT("siege.stone_land"), At, FRotator::ZeroRotator, 1.f, TEXT("siege.stone_land"));
	WRFx::Shake(Stone, 0.45f, At, 3000.f);
	UE_LOG(LogWildreach, Log, TEXT("SIEGE stone of %s landed at %s on %s"), *Owner, *At.ToCompactString(), *GetNameSafe(HitActor));
}

void FWRSiege::TickHauler(AWRCreature* Hauler, AWRSiegeEngine* Ram, float DeltaSeconds)
{
	if (!Hauler || !Ram) return;
	// It walks on its own legs to the harness behind the ram and keeps there, facing the way the ram faces.
	const FVector Harness = Ram->GetActorTransform().TransformPosition(FVector(WRSiegeDetail::Tn(TEXT("ram.harnessX"), -520.f), 0.f, 0.f));
	const FVector To = Harness - Hauler->GetActorLocation();
	const float Dist = float(To.Size2D());
	if (Dist > 25.f)
	{
		const float MaxSpeed = FMath::Max(100.f, Hauler->GetCharacterMovement()->MaxWalkSpeed);
		Hauler->AddMovementInput(To.GetSafeNormal2D(), FMath::Clamp(Dist / (MaxSpeed * 0.4f), 0.15f, 1.f));
	}
	else
	{
		Hauler->SetActorRotation(FMath::RInterpTo(Hauler->GetActorRotation(), FRotator(0.f, Ram->GetActorRotation().Yaw, 0.f), DeltaSeconds, 4.f));
	}
}

void FWRSiege::Save(UWorld* World)
{
	if (!World) return;
	TArray<TSharedPtr<FJsonValue>> List;
	for (TActorIterator<AWRSiegeEngine> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed() || It->Kind.IsNone()) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("kind"), It->Kind.ToString());
		O->SetStringField(TEXT("owner"), It->OwnerName);
		const FVector L = It->GetActorLocation();
		O->SetArrayField(TEXT("at"), { MakeShared<FJsonValueNumber>(L.X), MakeShared<FJsonValueNumber>(L.Y), MakeShared<FJsonValueNumber>(L.Z) });
		O->SetNumberField(TEXT("heading"), It->Rep.Heading);
		O->SetNumberField(TEXT("aim"), It->Rep.Aim);
		O->SetNumberField(TEXT("health"), It->Health);
		O->SetNumberField(TEXT("roof"), It->RoofHealth);
		O->SetNumberField(TEXT("range"), It->Rep.Range);
		O->SetBoolField(TEXT("loaded"), It->Rep.bLoaded);
		O->SetNumberField(TEXT("mult"), It->BuiltMult);
		List.Add(MakeShared<FJsonValueObject>(O));
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetArrayField(TEXT("engines"), List);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	WRStore::WriteDoc(TEXT("siege"), Text);
}

void FWRSiege::Load(UWorld* World)
{
	using namespace WRSiegeDetail;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!World || !WRStore::ReadDoc(TEXT("siege"), Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("engines"), List)) return;
	int32 N = 0;
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(TEXT("at"), At) || At->Num() < 3) continue;
		const FVector L((*At)[0]->AsNumber(), (*At)[1]->AsNumber(), (*At)[2]->AsNumber());
		const float Heading = float(O->GetNumberField(TEXT("heading")));
		AWRSiegeEngine* E = World->SpawnActorDeferred<AWRSiegeEngine>(AWRSiegeEngine::StaticClass(), FTransform(FRotator(0.f, Heading, 0.f), L), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!E) continue;
		E->Kind = FName(*O->GetStringField(TEXT("kind")));
		E->OwnerName = O->GetStringField(TEXT("owner"));
		const TCHAR* K = E->IsRam() ? TEXT("ram") : E->IsTrebuchet() ? TEXT("trebuchet") : TEXT("ballista");
		E->MaxHealth = Tn(FString::Printf(TEXT("%s.health"), K), E->IsRam() ? 2000.f : E->IsTrebuchet() ? 1500.f : 800.f);
		E->Health = FMath::Clamp(float(O->GetNumberField(TEXT("health"))), 1.f, E->MaxHealth);
		E->RoofHealth = float(O->GetNumberField(TEXT("roof")));
		E->Rep.Loc = L;
		E->Rep.Heading = Heading;
		E->Rep.Aim = float(O->GetNumberField(TEXT("aim")));
		E->Rep.Range = uint8(FMath::Clamp(int32(O->GetNumberField(TEXT("range"))), 0, 2));
		E->Rep.bLoaded = O->GetBoolField(TEXT("loaded"));
		double Mult = 1.0;
		O->TryGetNumberField(TEXT("mult"), Mult);
		E->BuiltMult = float(Mult);
		E->FinishSpawning(FTransform(FRotator(0.f, Heading, 0.f), L));
		++N;
	}
	UE_LOG(LogWildreach, Log, TEXT("SIEGE restored %d engines"), N);
}

void FWRSiege::TickLocalCrew(AWRCharacter* Who, float DeltaSeconds)
{
	AWRSiegeEngine* On = CrewingOf(Who);
	AWRPlayerController* PC = Who ? Cast<AWRPlayerController>(Who->GetController()) : nullptr;
	if (!Who || !PC || !Who->IsLocallyControlled()) return;
	// The owning machine stops predicting its own walking while it's held on a post (the server's DisableMovement
	// doesn't reach it), and walks again once it's let off.
	UCharacterMovementComponent* Move = Who->GetCharacterMovement();
	if (On && Move && Move->MovementMode != MOVE_None)
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Who->bCrewHeld = true;
	}
	else if (!On && Who->bCrewHeld)
	{
		Who->bCrewHeld = false;
		Who->CrewSentForward = Who->CrewSentRight = 0;
		if (Move && Move->MovementMode == MOVE_None) Move->SetMovementMode(MOVE_Walking);
	}
	if (!On) return;
	// The move stick as the post's input (-1, 0, 1 each way), sent when it changes.
	const int32 F = Who->CrewMove.Y > 0.4f ? 1 : Who->CrewMove.Y < -0.4f ? -1 : 0;
	const int32 R = Who->CrewMove.X > 0.4f ? 1 : Who->CrewMove.X < -0.4f ? -1 : 0;
	if (F != Who->CrewSentForward || R != Who->CrewSentRight)
	{
		Who->CrewSentForward = F;
		Who->CrewSentRight = R;
		PC->SendCommand(FString::Printf(TEXT("siege move %d %d"), F, R));
	}
	Who->CrewMove = FVector2D::ZeroVector;   // (filled again by this frame's Move input)
	// The ballista's gunner aims with their view (at most ten times a second, when it moved).
	if (On->IsBallista())
	{
		const FRotator View = PC->GetControlRotation();
		const double Now = Who->GetWorld()->GetTimeSeconds();
		if (Now - Who->CrewAimSentAt > 0.1 && (FMath::Abs(FRotator::NormalizeAxis(View.Yaw - Who->CrewAimSent.Yaw)) > 0.4f || FMath::Abs(FRotator::NormalizeAxis(View.Pitch - Who->CrewAimSent.Pitch)) > 0.4f))
		{
			Who->CrewAimSentAt = Now;
			Who->CrewAimSent = View;
			PC->SendCommand(FString::Printf(TEXT("siege aim %.1f %.1f"), View.Yaw, FRotator::NormalizeAxis(View.Pitch)));
		}
	}
	(void)DeltaSeconds;
}

bool FWRSiege::CrewInput(AWRCharacter* Who, int32 Input, bool bDown)
{
	AWRSiegeEngine* On = CrewingOf(Who);
	AWRPlayerController* PC = Who ? Cast<AWRPlayerController>(Who->GetController()) : nullptr;
	if (!On || !PC) return false;
	const EWRAbilityInput In = EWRAbilityInput(Input);
	if (In == EWRAbilityInput::Light || In == EWRAbilityInput::Heavy)
	{
		PC->SendCommand(FString::Printf(TEXT("siege act %d"), bDown ? 1 : 0));
		return true;
	}
	if (In == EWRAbilityInput::Block)
	{
		if (bDown && On->IsRam()) PC->SendCommand(TEXT("siege harness"));
		return true;
	}
	// (Dodge no longer packs it up: that's a held Interact now, Aaron 11-16 r-packup.)
	return true;   // no swings, casts or belt items from a post
}
