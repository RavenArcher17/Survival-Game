// The Desert's bosses: Ssarrok, the Mirage Matriarch, the Glass Empress and Ahrumet, with the Sundial Plaza's stelae.
// See WRDesertBosses.h; cooks/biome-desert.md 2.13-2.14. Numbers: tuning bosses.<id>.

#include "WRDesertBosses.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRDesertCreatures.h"
#include "WRDesertHazards.h"
#include "WRDesertTypes.h"
#include "WRDesertWorld.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSkyAir.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "WRAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "WRStrikeFront.h"

namespace WRDesertBossImpl
{
	double DbNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }
	float DbT(const AWRBoss* Boss, const TCHAR* Key, float Fallback)
	{
		return Boss ? FWRTuning::Get().Num(FString::Printf(TEXT("bosses.%s.%s"), *Boss->CreatureId.ToString(), Key), Fallback) : Fallback;
	}
	void DbTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	bool Is(const AWRBoss* B, const TCHAR* Id) { return B && B->CreatureId == FName(Id); }
	UWRDesertBody* Body(const AWRBoss* B) { return UWRDesertBody::Get(B); }
	void Flag(AWRBoss* B, uint32 F, bool bOn) { if (UWRDesertBody* Bd = Body(B)) Bd->Set(F, bOn); }
	void DbHit(AWRBoss* Boss, AWRCharacterBase* V, float Damage, const FVector& From, const TMap<FName, float>& Buildups = {}, bool bUnblockable = true)
	{
		FWRHit Hit;
		Hit.Instigator = Boss;
		Hit.Damage = Damage;
		Hit.bUnblockable = bUnblockable;
		Hit.bMelee = true;
		Hit.Origin = From;
		Hit.Element = WRTags::Element_Physical;
		for (const TPair<FName, float>& B : Buildups) Hit.Buildups.Add(B.Key, B.Value);
		UWRCombatLibrary::ApplyHit(V, Hit);
	}
	bool DbDaylight(const UObject* W)
	{
		const float H = WRDesertHz::LongNoon(W) ? 12.f : AWRGameState::HourIn(W);
		if (H < 6.f || H >= 20.f) return false;
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WRDesert::Biome());
		return !(K && K->Id == TEXT("sandstorm"));
	}
	bool DbOvercast(const UObject* W)
	{
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(W, WRDesert::Biome());
		return K && (K->Id == TEXT("overcast") || K->Id == TEXT("sandstorm"));
	}
	/** Each boss's server state. */
	struct FState
	{
		// The hold (the swallow, the crushing grab, Ahrumet's grab).
		TWeakObjectPtr<AWRCharacterBase> Held;
		FName HoldKind;
		double HoldUntil = 0.0, NextHoldTick = 0.0;
		float HoldDps = 0.f, FreeAt = 0.f, Freed = 0.f;
		// Ssarrok
		int32 RouteIndex = 0;
		double BeachedUntil = 0.0;
		// The Matriarch
		TArray<TWeakObjectPtr<AWRCreature>> Mirages;
		double MirageBlockUntil = 0.0, NextReformAt = 0.0;
		int32 MiragesWanted = 0;
		// The Glass Empress: pincer L, pincer R, back L, back R, tail, bulb
		float Plate[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		double FrostAt[6] = { -100.0, -100.0, -100.0, -100.0, -100.0, -100.0 };
		float BeamYaw = 0.f;
		double NextBeamTick = 0.0;
		// Ahrumet: parts eye, chest, wrist L, wrist R, knees
		bool bWoken = false;
		double StandUntil = 0.0;
		double HotUntil = 0.0;
		TSet<int32> Cracked;            // part + 10 x phase
		TSet<int32> CrackedEver;
		float SweepDir = 1.f;
		bool bReversed = false;
		double NextPulseAt = 0.0;
		bool bPulseWarned = false;   // the Noon Lock pulse's warning shown (cookpass-combat 3.10)
		float LockYaw = 0.f;
		double NextLockTick = 0.0;
		bool bUprooted = false;
	};
	TMap<TWeakObjectPtr<AWRBoss>, FState>& States() { static TMap<TWeakObjectPtr<AWRBoss>, FState> M; return M; }
	FState& St(AWRBoss* B) { return States().FindOrAdd(B); }
	const FState* StC(const AWRBoss* B) { return States().Find(const_cast<AWRBoss*>(B)); }
	const FWRDesertData& Dd() { return FWRDesertData::Get(); }
	FVector PlazaAt() { FVector P; return Dd().ArenaPoint(TEXT("ahrumet"), TEXT("gnomon"), P) ? P : FVector::ZeroVector; }
	FVector ThroneAt() { FVector P; return Dd().ArenaPoint(TEXT("ahrumet"), TEXT("throne"), P) ? P : PlazaAt(); }
	/** The Empress's plate nearest a point (body space): 0 pincer L, 1 pincer R, 2 back L, 3 back R, 4 tail, 5 bulb. */
	int32 EmpressPlate(const AWRBoss* Boss, const FVector& P)
	{
		const FVector L = Boss->GetActorTransform().InverseTransformPositionNoScale(P);
		const float R = Boss->GetSimpleCollisionRadius();
		if (L.X > R * 0.6f) return L.Y < 0.f ? 0 : 1;
		if (L.X < -R * 0.7f) return L.Z > Boss->GetSimpleCollisionHalfHeight() * 1.2f ? 5 : 4;
		return L.Y < 0.f ? 2 : 3;
	}
	/** Ahrumet's part by height on him: 0 eye, 1 chest, 2 wrist L, 3 wrist R, 4 knees. */
	int32 ColossusPart(const AWRBoss* Boss, const FVector& P)
	{
		const FVector L = Boss->GetActorTransform().InverseTransformPositionNoScale(P);
		const float H = Boss->GetSimpleCollisionHalfHeight();
		const float Up = (L.Z + H) / FMath::Max(1.f, 2.f * H);
		if (Up > 0.85f) return 0;
		if (Up < 0.35f) return 4;
		if (FMath::Abs(L.Y) > Boss->GetSimpleCollisionRadius() * 0.55f && Up < 0.62f) return L.Y < 0.f ? 2 : 3;
		return 1;
	}
	const TCHAR* PartName(int32 P) { static const TCHAR* N[] = { TEXT("eye"), TEXT("chest"), TEXT("left wrist"), TEXT("right wrist"), TEXT("knees") }; return N[FMath::Clamp(P, 0, 4)]; }
	FVector EyeAt(const AWRBoss* Boss) { return Boss->GetActorLocation() + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight() * 0.95f) + Boss->GetActorForwardVector() * 120.f; }
	/** Stance off a boss (a crack, a held wrist's 300): a break when it runs out. */
	void TakeStance(AWRBoss* Boss, float Amount, AActor* By)
	{
		UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
		const UWRAttributeSet* A = Boss->GetAttributes();
		if (!ASC || !A) return;
		const float Left = FMath::Max(0.f, A->GetStance() - Amount);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetStanceAttribute(), Left);
		if (Left <= 0.f) UWRCombatLibrary::BreakStance(Boss, UWRCombatLibrary::StanceBreakSeconds(Boss), By);
	}
	/** A beam from the eye along Dir (2D) out to Range: who it burns (cover blocks it; stelae count the hit). */
	void BeamSweep(AWRBoss* Boss, const FVector& From, const FVector& Dir, float Range, float Damage, float Blind, TSet<TWeakObjectPtr<AWRDesertStela>>* StelaeHit)
	{
		UWorld* W = Boss->GetWorld();
		const FVector End2 = From + Dir.GetSafeNormal2D() * Range;
		float G = Boss->GetActorLocation().Z - Boss->GetSimpleCollisionHalfHeight();
		WRLand::GroundUnder(FVector(End2.X, End2.Y, From.Z), G);
		const FVector End(End2.X, End2.Y, G + 60.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRDesertBeam), false, Boss);
		// The stelae in the beam's way: each takes the hit (three breaks it), and shades what's behind it.
		FHitResult H;
		FVector Stop = End;
		if (W->LineTraceSingleByChannel(H, From, End, ECC_WorldStatic, Q))
		{
			Stop = H.ImpactPoint;
			if (AWRDesertStela* Sx = Cast<AWRDesertStela>(H.GetActor()); Sx && StelaeHit && !StelaeHit->Contains(Sx))
			{
				StelaeHit->Add(Sx);
				if (++Sx->BeamHits >= int32(DbT(Boss, TEXT("stelaBeams"), 3.f))) Sx->SetBroken(true);
			}
		}
		WRFx::Broadcast(Boss, TEXT("desert.eye_beam_hit"), Stop, FRotator::ZeroRotator, 1.f, NAME_None);
		for (TActorIterator<AWRCharacterBase> It(W); It; ++It)
		{
			AWRCharacterBase* V = *It;
			if (V == Boss || !V->IsAlive() || UWRDesertBody::Get(V)) continue;
			const FVector P = V->GetActorLocation();
			const FVector Q2 = FMath::ClosestPointOnSegment(P, From, Stop);
			if (FVector::Dist(P, Q2) > V->GetSimpleCollisionRadius() + 150.f) continue;
			DbHit(Boss, V, Damage, From, { { TEXT("blind"), Blind } });
		}
	}
	AWRBoss* FindBoss(const UWorld* W, FName Id)
	{
		for (TActorIterator<AWRBoss> It(W); It; ++It) if (It->CreatureId == Id && It->IsAlive()) return *It;
		return nullptr;
	}
	void EndHold(AWRBoss* Boss, FState& S, bool bSpit)
	{
		AWRCharacterBase* V = S.Held.Get();
		if (UWRDesertBody* Bd = Body(Boss)) { Bd->Holding = nullptr; Bd->HeldBody = nullptr; Bd->Set(WRDesertFlag::Grip, false); }
		S.Held = nullptr;
		if (!V || !V->IsAlive()) return;
		if (bSpit)
		{
			if (S.HoldKind == TEXT("swallow"))
			{
				// Spat out: 40 and blind 80, thrown clear.
				DbHit(Boss, V, DbT(Boss, TEXT("spitDamage"), 40.f), Boss->GetActorLocation(), { { TEXT("blind"), DbT(Boss, TEXT("spitBlind"), 80.f) } });
				WRSkyAir::Push(V, Boss->GetActorForwardVector() * 1000.f + FVector(0, 0, 400.f), 0.6f, TEXT("spit"), Boss);
				WRFx::Broadcast(Boss, TEXT("desert.sand_burst"), V->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("boss.ssarrok.spit"));
			}
			else if (S.HoldKind == TEXT("crush"))
			{
				// The sting through the hold: x1.5, blind 80 (none with the bulb shattered).
				DbHit(Boss, V, DbT(Boss, TEXT("crushSting"), 82.f), Boss->GetActorLocation(), { { TEXT("blind"), S.Plate[5] > 0.f ? 80.f : 0.f } });
				WRFx::Broadcast(Boss, TEXT("sun.blind_flash"), V->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("creature.glasstail.sting"));
			}
			else if (S.HoldKind == TEXT("palm"))
			{
				// The eye at point blank.
				Flag(Boss, WRDesertFlag::Flaring, true);
				DbHit(Boss, V, DbT(Boss, TEXT("grabEye"), 60.f), EyeAt(Boss), { { TEXT("blind"), 40.f } });
				WRFx::Broadcast(Boss, TEXT("desert.eye_beam_hit"), V->GetActorLocation(), FRotator::ZeroRotator, 2.f, TEXT("boss.ahrumet.beam"));
				WRSkyAir::Push(V, Boss->GetActorForwardVector() * 900.f + FVector(0, 0, 300.f), 0.5f, TEXT("throw"), Boss);
			}
		}
		else
		{
			WRSkyAir::Push(V, Boss->GetActorForwardVector() * 500.f + FVector(0, 0, 300.f), 0.4f, TEXT("drop"), Boss);
			DbTell(V, TEXT("You're free"));
		}
	}
	void StartHold(AWRBoss* Boss, FState& S, AWRCharacterBase* V, FName Kind, float Seconds, float Dps, float FreeAt)
	{
		if (!V || S.Held.IsValid()) return;
		S.Held = V;
		S.HoldKind = Kind;
		S.HoldUntil = DbNow(Boss) + Seconds;
		S.NextHoldTick = DbNow(Boss) + 1.0;
		S.HoldDps = Dps;
		S.FreeAt = FreeAt;
		S.Freed = 0.f;
		if (UWRDesertBody* Bd = Body(Boss)) { Bd->Holding = V; Bd->HeldBody = V; Bd->Set(WRDesertFlag::Grip, true); }
		DbTell(V, Kind == TEXT("swallow") ? TEXT("Swallowed! Your allies can cut you free: strike it hard before it spits you out") : TEXT("Caught! Have someone strike the hand that holds you"));
	}
	void SpawnMirage(AWRBoss* Boss, FState& S, int32 i, AActor* Target)
	{
		const FVector At = Boss->GetActorLocation() + FRotator(0.f, Boss->GetActorRotation().Yaw + 90.f * (i + 1), 0.f).Vector() * DbT(Boss, TEXT("mirageSpread"), 500.f);
		AWRCreature* M = AWRCreature::SpawnCreature(Boss->GetWorld(), TEXT("matriarch_mirage"), At, Boss->GetActorRotation());
		if (!M) return;
		M->SetHome(Boss->GetHome());
		if (Target) M->SetAggroTarget(Target);
		S.Mirages.Add(M);
		WRFx::Broadcast(Boss, TEXT("desert.mirage_shimmer"), At, FRotator::ZeroRotator, 2.f, TEXT("creature.shimmerbuck.whistle"));
	}
	void PopMirages(AWRBoss* Boss, FState& S)
	{
		for (const TWeakObjectPtr<AWRCreature>& M : S.Mirages)
		{
			if (!M.IsValid()) continue;
			WRFx::Broadcast(Boss, TEXT("desert.mirage_shimmer"), M->GetActorLocation(), FRotator::ZeroRotator, 2.f, NAME_None);
			M->Destroy();
		}
		S.Mirages.Reset();
	}
}
using namespace WRDesertBossImpl;

// ================================================================================================ the stelae

TArray<TWeakObjectPtr<AWRDesertStela>>& AWRDesertStela::All() { static TArray<TWeakObjectPtr<AWRDesertStela>> L; return L; }

AWRDesertStela::AWRDesertStela()
{
	bReplicates = true;
	Stone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stone"));
	SetRootComponent(Stone);
	Stone->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	// 4 m tall, 2 m wide, 60 cm thick, its foot on the dial.
	Stone->SetRelativeScale3D(FVector(0.6f, 2.f, 4.f));
	Stone->SetCollisionProfileName(TEXT("BlockAll"));
	Stone->SetMobility(EComponentMobility::Movable);
}

void AWRDesertStela::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRDesertStela, Broken);
}

void AWRDesertStela::BeginPlay()
{
	Super::BeginPlay();
	All().Add(this);
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_Rock.M_Rock")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(FColor(0xE6, 0xDA, 0xC2)) * 0.62f);
		Stone->SetMaterial(0, MID);
	}
	OnRep_State();
}

void AWRDesertStela::SetBroken(bool bBroken)
{
	Broken = bBroken ? 1 : 0;
	if (!bBroken) BeamHits = 0;
	if (bBroken) WRFx::Broadcast(this, TEXT("hit.dust"), GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("boss.ahrumet.stela"));
	OnRep_State();
}

void AWRDesertStela::OnRep_State()
{
	// Broken: the slab falls to a stump (a quarter of its height), no longer cover.
	const bool bB = Broken != 0;
	Stone->SetRelativeScale3D(FVector(0.6f, 2.f, bB ? 1.f : 4.f));
	Stone->SetCollisionEnabled(bB ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
	Stone->SetCollisionResponseToChannel(ECC_WorldStatic, bB ? ECR_Ignore : ECR_Block);
}

// ================================================================================================ the hooks

bool FWRDesertBoss::IsDesertBoss(const AWRBoss* Boss)
{
	return Boss && (Is(Boss, TEXT("ssarrok")) || Is(Boss, TEXT("mirage_matriarch")) || Is(Boss, TEXT("glass_empress")) || Is(Boss, TEXT("ahrumet")));
}

void FWRDesertBoss::Begin(AWRBoss* Boss)
{
	if (!IsDesertBoss(Boss)) return;
	FState& S = St(Boss);
	PopMirages(Boss, S);
	S = FState();
	if (Is(Boss, TEXT("glass_empress"))) for (float& P : S.Plate) P = DbT(Boss, TEXT("plateHealth"), 300.f);
	if (Is(Boss, TEXT("ahrumet"))) Flag(Boss, WRDesertFlag::Asleep, true);
	if (Is(Boss, TEXT("ssarrok"))) Flag(Boss, WRDesertFlag::Diving, true);
}

void FWRDesertBoss::Reset(AWRBoss* Boss)
{
	if (!IsDesertBoss(Boss)) return;
	FState& S = St(Boss);
	EndHold(Boss, S, false);
	Begin(Boss);
	if (UWRDesertBody* Bd = Body(Boss)) { Bd->Flags = 0; if (Is(Boss, TEXT("ahrumet"))) Bd->Set(WRDesertFlag::Asleep, true); }
	if (Is(Boss, TEXT("ahrumet"))) if (UWRDesertArenas* A = UWRDesertArenas::Get(Boss)) A->ResetPlaza();
	Boss->GetCharacterMovement()->MaxWalkSpeed = Boss->GetDef() ? Boss->GetDef()->Speed : 400.f;
}

void FWRDesertBoss::EndPlay(AWRBoss* Boss)
{
	if (!IsDesertBoss(Boss)) return;
	if (FState* S = States().Find(Boss)) PopMirages(Boss, *S);
	States().Remove(Boss);
}

bool FWRDesertBoss::TickIdle(AWRBoss* Boss, float DeltaSeconds, bool& bSkipWake)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const double T = DbNow(Boss);
	if (Is(Boss, TEXT("ssarrok")))
	{
		// The Erg loop under the sand; at dusk down to the strand's wet sand to hunt; by day in a heat wave, at its lair.
		const float H = AWRGameState::HourIn(Boss);
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(Boss, WRDesert::Biome());
		const bool bDusk = H >= DbT(Boss, TEXT("duskFrom"), 18.f) && H < DbT(Boss, TEXT("duskTo"), 21.f);
		const bool bLair = K && K->Id == TEXT("heatwave");
		TArray<FVector> Route = Dd().ArenaPoints(TEXT("ssarrok"), bDusk ? TEXT("dusk") : TEXT("route"));
		FVector Want;
		if (bLair && Dd().ArenaPoint(TEXT("ssarrok"), TEXT("lair"), Want)) {}
		else if (Route.Num() > 0)
		{
			S.RouteIndex = S.RouteIndex % Route.Num();
			Want = Route[S.RouteIndex];
			if (FVector::Dist2D(Want, Boss->GetActorLocation()) < 1500.f) S.RouteIndex = (S.RouteIndex + 1) % Route.Num();
		}
		else return false;
		const FVector D = Want - Boss->GetActorLocation();
		if (D.Size2D() > 400.f)
		{
			Boss->AddMovementInput(D.GetSafeNormal2D(), bLair ? 0.3f : DbT(Boss, TEXT("routeInput"), 0.5f));
			Boss->SetActorRotation(FMath::RInterpTo(Boss->GetActorRotation(), D.GetSafeNormal2D().Rotation(), DeltaSeconds, 1.f));
		}
		// Under the soft sand only its fin shows; on firm ground it crawls up.
		Flag(Boss, WRDesertFlag::Diving, FWRDesertAI::CanDive(Boss));
		if (FWRDesertAI::CanDive(Boss) && FMath::Fmod(float(T), 0.8f) < DeltaSeconds) WRFx::Broadcast(Boss, TEXT("desert.fin_wake"), Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), Boss->GetActorRotation(), 3.f, NAME_None);
		Boss->SetHome(Boss->GetActorLocation());
		return true;
	}
	if (Is(Boss, TEXT("mirage_matriarch")))
	{
		// At night she sleeps in the deepest pool and can't be fought (no sun, no mirages).
		const bool bNight = AWRGameState::IsNightIn(Boss) && !WRDesertHz::LongNoon(Boss);
		Flag(Boss, WRDesertFlag::Asleep, bNight);
		if (bNight) { bSkipWake = true; return true; }
		return false;
	}
	if (Is(Boss, TEXT("ahrumet")))
	{
		// Seated on the throne until someone steps onto the meridian strip: the gnomon's hole throws a shaft of light up
		// the strip to the throne, the eye lights, he sings, tears his legs from the sand (4 s) and stands.
		if (!S.bWoken)
		{
			bSkipWake = true;
			Boss->SetActorRotation(FRotator(0.f, (PlazaAt() - ThroneAt()).Rotation().Yaw, 0.f));
			const FVector A = PlazaAt(), B = ThroneAt();
			for (TActorIterator<AWRCharacter> It(Boss->GetWorld()); It; ++It)
			{
				if (!It->IsAlive()) continue;
				const FVector P = It->GetActorLocation();
				if (FVector::Dist2D(FMath::ClosestPointOnSegment(FVector(P.X, P.Y, A.Z), A, FVector(B.X, B.Y, A.Z)), FVector(P.X, P.Y, A.Z)) > DbT(Boss, TEXT("meridianReach"), 250.f)) continue;
				S.bWoken = true;
				S.StandUntil = T + DbT(Boss, TEXT("standSeconds"), 4.f);
				Flag(Boss, WRDesertFlag::Asleep, false);
				WRFx::Broadcast(Boss, TEXT("desert.light_shaft"), FMath::Lerp(A, B, 0.5f) + FVector(0, 0, 1200.f), (B - A).Rotation(), 3.f, TEXT("desert.dawn_song"));
				WRFx::Broadcast(Boss, TEXT("desert.sand_burst"), Boss->GetActorLocation() - FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 6.f, TEXT("boss.ahrumet.rise"));
				Boss->SetAggroTarget(*It);
				UE_LOG(LogWildreach, Log, TEXT("DESERT Ahrumet wakes (the meridian: %s)"), *It->GetName());
				break;
			}
			return true;
		}
		if (T < S.StandUntil) { bSkipWake = true; return true; }
		return false;
	}
	return false;
}

bool FWRDesertBoss::Tick(AWRBoss* Boss, float DeltaSeconds)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const double T = DbNow(Boss);
	// ---------------------------------------------------------------- holds
	if (AWRCharacterBase* V = S.Held.Get())
	{
		if (!V->IsAlive() || !Boss->IsAlive()) EndHold(Boss, S, false);
		else
		{
			const FVector Mouth = S.HoldKind == TEXT("palm") ? Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 150.f) + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight() * 0.3f)
				: Boss->GetActorLocation() + Boss->GetActorForwardVector() * (Boss->GetSimpleCollisionRadius() + 60.f);
			V->SetActorLocation(Mouth, false);
			V->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			if (T >= S.NextHoldTick)
			{
				S.NextHoldTick = T + 1.0;
				DbHit(Boss, V, S.HoldDps, Boss->GetActorLocation());
			}
			if (S.Freed >= S.FreeAt) { EndHold(Boss, S, false); if (Boss->Status) Boss->Status->Stagger(1.0f); }
			else if (T >= S.HoldUntil) EndHold(Boss, S, true);
		}
	}
	// ---------------------------------------------------------------- Ssarrok
	if (Is(Boss, TEXT("ssarrok")))
	{
		const bool bBeached = T < S.BeachedUntil;
		Flag(Boss, WRDesertFlag::Beached, bBeached);
		if (bBeached) { Boss->GetCharacterMovement()->StopMovementImmediately(); return true; }
		Flag(Boss, WRDesertFlag::Diving, FWRDesertAI::CanDive(Boss) && Boss->StrikeState == AWRBoss::EStrike::None);
		return false;
	}
	// ---------------------------------------------------------------- the Matriarch
	if (Is(Boss, TEXT("mirage_matriarch")))
	{
		S.Mirages.RemoveAll([](const TWeakObjectPtr<AWRCreature>& M) { return !M.IsValid() || !M->IsAlive(); });
		// A popped mirage re-forms 12 s later (not while blind holds them off; half as long under cloud).
		if (S.MiragesWanted > S.Mirages.Num() && T >= S.MirageBlockUntil)
		{
			if (S.NextReformAt <= 0.0) S.NextReformAt = T + DbT(Boss, TEXT("reformSeconds"), 12.f) * (DbOvercast(Boss) ? 0.5f : 1.f);
			else if (T >= S.NextReformAt) { SpawnMirage(Boss, S, S.Mirages.Num(), Boss->AttackTarget.Get()); S.NextReformAt = 0.0; }
		}
		return false;
	}
	// ---------------------------------------------------------------- Ahrumet
	if (Is(Boss, TEXT("ahrumet")))
	{
		Flag(Boss, WRDesertFlag::Glowing, T < S.HotUntil);
		if (Boss->StrikeState == AWRBoss::EStrike::None) Flag(Boss, WRDesertFlag::Flaring, false);
		// The hidden Noon Lock (phase 3): the beam turns round the plaza, a pulse every 20 s; only under him is shade.
		if (Boss->Phase >= 3)
		{
			S.LockYaw += DbT(Boss, TEXT("lockTurn"), 30.f) * DeltaSeconds;
			if (T >= S.NextLockTick)
			{
				S.NextLockTick = T + 0.25;
				BeamSweep(Boss, EyeAt(Boss), FRotator(0.f, S.LockYaw, 0.f).Vector(), DbT(Boss, TEXT("beamRange"), 4000.f), DbT(Boss, TEXT("beamTick"), 40.f) * 0.5f, 15.f, nullptr);
			}
			if (S.NextPulseAt <= 0.0) S.NextPulseAt = T + DbT(Boss, TEXT("lockPulseEvery"), 20.f);
			else if (!S.bPulseWarned && T >= S.NextPulseAt - DbT(Boss, TEXT("pulseWarn"), 2.f))
			{
				// Two seconds of warning (cookpass-combat 3.10): the sun gathers over the plaza, his eye flares crimson,
				// the rising sting: get under him.
				S.bPulseWarned = true;
				WRFx::Broadcast(Boss, TEXT("desert.noon_pulse"), Boss->GetActorLocation() + FVector(0.f, 0.f, 1200.f), FRotator::ZeroRotator, 1.2f, TEXT("tell.final"), DbT(Boss, TEXT("pulseWarn"), 2.f));
				Boss->MulticastTell(2, TEXT("head"), TEXT("voice"));
			}
			else if (T >= S.NextPulseAt)
			{
				S.bPulseWarned = false;
				S.NextPulseAt = T + DbT(Boss, TEXT("lockPulseEvery"), 20.f);
				WRFx::Broadcast(Boss, TEXT("desert.noon_pulse"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("boss.ahrumet.pulse"));
				for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
				{
					AWRCharacterBase* V = *It;
					if (V == Boss || !V->IsAlive() || UWRDesertBody::Get(V) || FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) > Boss->GetChaseRadius()) continue;
					if (FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) < Boss->GetSimpleCollisionRadius() * DbT(Boss, TEXT("lockShade"), 1.6f)) continue;
					DbHit(Boss, V, DbT(Boss, TEXT("pulseDamage"), 80.f), EyeAt(Boss), { { TEXT("blind"), 100.f } });
				}
				S.HotUntil = T + DbT(Boss, TEXT("hotSeconds"), 6.f);
			}
		}
		return false;
	}
	return false;
}

bool FWRDesertBoss::Allowed(const AWRBoss* Boss, const FWRBossAttack& Attack, const AWRCharacterBase* Target)
{
	if (!IsDesertBoss(Boss)) return true;
	const FState* S = StC(Boss);
	const FName Id = Attack.Id;
	const double T = DbNow(Boss);
	if (S && S->Held.IsValid()) return false;
	if (Is(Boss, TEXT("ssarrok")) && S)
	{
		if (T < S->BeachedUntil) return false;
		// The fin wake and the breach come from under the sand.
		if (Id == TEXT("fin_wake") || Id == TEXT("breach")) return FWRDesertAI::CanDive(Boss);
		return true;
	}
	if (Is(Boss, TEXT("mirage_matriarch")) && S)
	{
		if (Id == TEXT("mirage_split")) return S->Mirages.Num() < 3 && T >= S->MirageBlockUntil && DbDaylight(Boss);
		if (Id == TEXT("crossfire_charge")) return S->Mirages.Num() > 0;
		return true;
	}
	if (Is(Boss, TEXT("glass_empress")) && S)
	{
		if (Id == TEXT("prism_beam")) return S->Plate[5] > 0.f && (S->Plate[2] > 0.f || S->Plate[3] > 0.f) && DbDaylight(Boss);
		return true;
	}
	if (Is(Boss, TEXT("ahrumet")) && S)
	{
		const bool bP2 = Boss->Phase >= 2;
		if (Id == TEXT("club_combo") || Id == TEXT("noon_pulse") || Id == TEXT("held_beam") || Id == TEXT("glassfall")) return bP2;
		if (Id == TEXT("eye_beam") || Id == TEXT("sand_wave")) return !bP2 || Id == TEXT("sand_wave");
		if (Id == TEXT("gaze"))
		{
			for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It; ++It) if (It->IsTamed() && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation()) < Boss->GetChaseRadius()) return true;
			return false;
		}
		return true;
	}
	return true;
}

void FWRDesertBoss::AttackStarted(AWRBoss* Boss, FWRBossAttack& Attack, AWRCharacterBase* Target)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	if (Is(Boss, TEXT("ssarrok")))
	{
		if (Id == TEXT("breach") && Target) WRFx::Broadcast(Boss, TEXT("desert.sand_bulge"), Target->GetActorLocation() - FVector(0, 0, Target->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 4.f, TEXT("boss.ssarrok.boom"));
		if (Id == TEXT("swallow")) { Flag(Boss, WRDesertFlag::Diving, false); Flag(Boss, WRDesertFlag::Jaw, true); }
		if (Id == TEXT("fin_wake")) WRSound::PlayAt(Boss, TEXT("creature.dunejaw.hiss"), Boss->GetActorLocation());
		if (Id == TEXT("geyser_roar") || Id == TEXT("tail_slam")) Flag(Boss, WRDesertFlag::Diving, false);
	}
	else if (Is(Boss, TEXT("mirage_matriarch")))
	{
		if (Id == TEXT("mirage_split")) WRFx::Broadcast(Boss, TEXT("desert.mirage_shimmer"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("boss.mirage_matriarch.shimmer"));
		if (Id == TEXT("crossfire_charge"))
		{
			// All four does charge from different angles: a hoof scrape from each.
			for (const TWeakObjectPtr<AWRCreature>& M : S.Mirages) if (M.IsValid() && Target) { M->SetAggroTarget(Target); WRSound::PlayAt(M.Get(), TEXT("creature.shimmerbuck.whistle"), M->GetActorLocation()); }
		}
		if (Id.ToString().StartsWith(TEXT("horn"))) Flag(Boss, WRDesertFlag::Jaw, true);
	}
	else if (Is(Boss, TEXT("glass_empress")))
	{
		if (Id == TEXT("sting") || Id == TEXT("prism_beam")) Flag(Boss, WRDesertFlag::Raised, true);
		if (Id == TEXT("prism_beam")) WRSound::PlayAt(Boss, TEXT("creature.glasstail.glass_ring"), Boss->GetActorLocation());
		if (Id.ToString().StartsWith(TEXT("pincer")) || Id == TEXT("crushing_grab")) Flag(Boss, WRDesertFlag::Jaw, true);
		if (Id == TEXT("burrow_emerge")) { Flag(Boss, WRDesertFlag::Buried, true); if (Target) WRFx::Broadcast(Boss, TEXT("desert.sand_bulge"), Target->GetActorLocation() - FVector(0, 0, Target->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 3.f, TEXT("creature.glasstail.rattle")); }
	}
	else if (Is(Boss, TEXT("ahrumet")))
	{
		if (Id == TEXT("eye_beam") || Id == TEXT("held_beam") || Id == TEXT("gaze")) { Flag(Boss, WRDesertFlag::Raised, true); WRSound::PlayAt(Boss, TEXT("boss.ahrumet.memnon"), Boss->GetActorLocation()); }
		if (Id == TEXT("noon_pulse"))
		{
			// He plants the gnomon upright and raises his eye to the sky: the sky whitens, a rising hum.
			Flag(Boss, WRDesertFlag::Raised, true);
			if (AWRDesertState* DS = AWRDesertState::Get(Boss)) DS->bGnomonUprooted = false;
			Flag(Boss, WRDesertFlag::Dulla, false);
			WRFx::Broadcast(Boss, TEXT("desert.heat_column"), Boss->GetActorLocation(), FRotator::ZeroRotator, 0.3f, TEXT("boss.ahrumet.hum"));
		}
		if (Id == TEXT("stomp_combo") || Id == TEXT("club_combo") || Id == TEXT("palm_sweep") || Id == TEXT("sand_wave") || Id == TEXT("grab")) Flag(Boss, WRDesertFlag::Jaw, true);
		S.SweepDir = FMath::RandBool() ? 1.f : -1.f;
		S.bReversed = false;
	}
}

bool FWRDesertBoss::BeginStrike(AWRBoss* Boss, FWRBossAttack& Attack)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	if (Is(Boss, TEXT("mirage_matriarch")) && Id == TEXT("mirage_split"))
	{
		S.MiragesWanted = 3;
		for (int32 i = S.Mirages.Num(); i < 3; ++i) SpawnMirage(Boss, S, i, Boss->AttackTarget.Get());
		return true;
	}
	if (Is(Boss, TEXT("glass_empress")) && Id == TEXT("prism_beam"))
	{
		// The start direction locks at the windup's end (no tracking).
		S.BeamYaw = Boss->GetActorRotation().Yaw - DbT(Boss, TEXT("prismArc"), 40.f) * 0.5f;
		S.NextBeamTick = 0.0;
		return false;
	}
	if (Is(Boss, TEXT("ahrumet")))
	{
		if (Id == TEXT("eye_beam") || Id == TEXT("held_beam") || Id == TEXT("gaze"))
		{
			Flag(Boss, WRDesertFlag::Flaring, true);
			S.BeamYaw = Boss->GetActorRotation().Yaw - S.SweepDir * DbT(Boss, TEXT("beamArc"), 90.f) * 0.5f;
			S.NextBeamTick = 0.0;
			if (Id == TEXT("gaze"))
			{
				// Its eye fixes the furthest tame: a short beam down that line (x2 on tames: the row's tame punish).
				AWRCreature* Far = nullptr;
				float FarD = 0.f;
				for (TActorIterator<AWRCreature> It(Boss->GetWorld()); It; ++It)
				{
					const float D = FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation());
					if (It->IsTamed() && It->IsAlive() && D < Boss->GetChaseRadius() && D > FarD) { FarD = D; Far = *It; }
				}
				if (Far) { BeamSweep(Boss, EyeAt(Boss), Far->GetActorLocation() - Boss->GetActorLocation(), FarD + 300.f, DbT(Boss, TEXT("gazeDamage"), 80.f), 30.f, nullptr); }
				return true;
			}
			return false;
		}
		if (Id == TEXT("noon_pulse"))
		{
			// 80 and blind 100 to all not in shadow: shade is a line from you toward his raised eye (the sun he calls)
			// blocked by the gnomon he planted, a standing stela, or him.
			const FVector Sun = EyeAt(Boss) + FVector(0, 0, DbT(Boss, TEXT("pulseSunHeight"), 6000.f));
			WRFx::Broadcast(Boss, TEXT("desert.noon_pulse"), Boss->GetActorLocation(), FRotator::ZeroRotator, 3.f, TEXT("boss.ahrumet.pulse"));
			for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
			{
				AWRCharacterBase* V = *It;
				if (V == Boss || !V->IsAlive() || UWRDesertBody::Get(V) || FVector::Dist2D(V->GetActorLocation(), Boss->GetActorLocation()) > Boss->GetChaseRadius()) continue;
				FHitResult H;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(WRNoonPulse), false, V);
				const FVector From = V->GetActorLocation() + FVector(0, 0, 40.f);
				bool bShade = Boss->GetWorld()->LineTraceSingleByChannel(H, From, From + (Sun - From).GetSafeNormal() * 4000.f, ECC_WorldStatic, Q);
				if (!bShade)
				{
					// His own body throws shade too.
					const FVector Toward = (Sun - From).GetSafeNormal();
					const FVector C = Boss->GetActorLocation();
					const FVector Near = FMath::ClosestPointOnSegment(C, From, From + Toward * 4000.f);
					bShade = FVector::Dist(Near, C) < Boss->GetSimpleCollisionRadius();
				}
				if (bShade) continue;
				DbHit(Boss, V, DbT(Boss, TEXT("pulseDamage"), 80.f), Sun, { { TEXT("blind"), 100.f } });
			}
			S.HotUntil = DbNow(Boss) + DbT(Boss, TEXT("hotSeconds"), 6.f);
			return true;
		}
	}
	return false;
}

bool FWRDesertBoss::TickStrike(AWRBoss* Boss, FWRBossAttack& Attack, float DeltaSeconds)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return false;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = DbNow(Boss);
	if (Is(Boss, TEXT("glass_empress")) && Id == TEXT("prism_beam"))
	{
		// Her back plates focus the sun into a 20 m beam sweeping 40 degrees at 50 a second; rock blocks it.
		S.BeamYaw += DbT(Boss, TEXT("prismTurn"), 50.f) * DeltaSeconds;
		if (T >= S.NextBeamTick)
		{
			S.NextBeamTick = T + 0.25;
			BeamSweep(Boss, Boss->GetActorLocation() + FVector(0, 0, Boss->GetSimpleCollisionHalfHeight()), FRotator(0.f, S.BeamYaw, 0.f).Vector(), DbT(Boss, TEXT("prismRange"), 2000.f), DbT(Boss, TEXT("prismTick"), 15.f), 10.f, nullptr);
		}
		return false;
	}
	if (Is(Boss, TEXT("ahrumet")) && (Id == TEXT("eye_beam") || Id == TEXT("held_beam")))
	{
		// The 40 m beam sweeps 90 degrees at 60 a second (the held beam waits 0.6 s, then sweeps and reverses once).
		static TSet<TWeakObjectPtr<AWRDesertStela>> HitThisBeam;
		const float Rate = DbT(Boss, TEXT("beamTurn"), 60.f);
		const float Arc = DbT(Boss, TEXT("beamArc"), 90.f);
		const float Elapsed = float(T - Boss->StrikeStarted);
		if (Id == TEXT("held_beam") && Elapsed < 0.6f) return false;
		if (Elapsed < 0.05f) HitThisBeam.Reset();
		S.BeamYaw += S.SweepDir * Rate * DeltaSeconds;
		const float Start = Boss->GetActorRotation().Yaw;
		if (Id == TEXT("held_beam") && !S.bReversed && FMath::Abs(FMath::FindDeltaAngleDegrees(Start, S.BeamYaw)) > Arc * 0.5f) { S.SweepDir = -S.SweepDir; S.bReversed = true; }
		if (T >= S.NextBeamTick)
		{
			S.NextBeamTick = T + 0.25;
			BeamSweep(Boss, EyeAt(Boss), FRotator(0.f, S.BeamYaw, 0.f).Vector(), DbT(Boss, TEXT("beamRange"), 4000.f), DbT(Boss, TEXT("beamTick"), 40.f), DbT(Boss, TEXT("beamBlind"), 30.f), &HitThisBeam);
		}
		return false;
	}
	return false;
}

void FWRDesertBoss::EndStrike(AWRBoss* Boss, const FWRBossAttack& Attack)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const FName Id = Attack.Id;
	const double T = DbNow(Boss);
	Flag(Boss, WRDesertFlag::Jaw, false);
	Flag(Boss, WRDesertFlag::Raised, false);
	if (Is(Boss, TEXT("ssarrok")) && (Id == TEXT("breach") || Id == TEXT("swallow") || Id == TEXT("tail_slam")))
	{
		// Firm ground beaches it: hardpan, rock, wet sand (an Oasis jar's splash, the strand's tide line).
		if (!FWRDesertAI::CanDive(Boss))
		{
			S.BeachedUntil = T + DbT(Boss, TEXT("beachSeconds"), 6.f);
			Boss->StanceOpenUntil = S.BeachedUntil;
			Flag(Boss, WRDesertFlag::Beached, true);
			WRFx::Broadcast(Boss, TEXT("desert.sand_burst"), Boss->GetActorLocation(), FRotator::ZeroRotator, 4.f, TEXT("boss.ssarrok.beached"));
			UE_LOG(LogWildreach, Log, TEXT("DESERT Ssarrok is beached"));
		}
	}
	if (Is(Boss, TEXT("glass_empress")) && Id == TEXT("burrow_emerge")) Flag(Boss, WRDesertFlag::Buried, false);
	if (Is(Boss, TEXT("ahrumet")))
	{
		if (Id == TEXT("eye_beam") || Id == TEXT("held_beam")) S.HotUntil = T + DbT(Boss, TEXT("hotSeconds"), 6.f);
		if (Id == TEXT("noon_pulse") && Boss->Phase >= 2)
		{
			// He pulls the gnomon back up as his club.
			if (AWRDesertState* DS = AWRDesertState::Get(Boss)) DS->bGnomonUprooted = true;
			Flag(Boss, WRDesertFlag::Dulla, true);
		}
	}
}

void FWRDesertBoss::HitLanded(AWRBoss* Boss, AWRCharacterBase* Victim, FName AttackId, EWRHitResult Result)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority() || !Victim) return;
	if (Result != EWRHitResult::Hit && Result != EWRHitResult::GuardBroken) return;
	FState& S = St(Boss);
	if (Is(Boss, TEXT("ssarrok")) && AttackId == TEXT("swallow"))
		StartHold(Boss, S, Victim, TEXT("swallow"), DbT(Boss, TEXT("swallowSeconds"), 4.f), DbT(Boss, TEXT("swallowDps"), 12.f), DbT(Boss, TEXT("throatFree"), 400.f));
	else if (Is(Boss, TEXT("glass_empress")) && AttackId == TEXT("crushing_grab"))
		StartHold(Boss, S, Victim, TEXT("crush"), DbT(Boss, TEXT("crushSeconds"), 2.f), DbT(Boss, TEXT("crushDps"), 25.f), DbT(Boss, TEXT("crushFree"), 300.f));
	else if (Is(Boss, TEXT("ahrumet")) && AttackId == TEXT("grab"))
		StartHold(Boss, S, Victim, TEXT("palm"), DbT(Boss, TEXT("grabSeconds"), 2.f), DbT(Boss, TEXT("grabDps"), 15.f), DbT(Boss, TEXT("wristFree"), 300.f));
	else if (Is(Boss, TEXT("ssarrok")) && AttackId == TEXT("breach"))
		WRSkyAir::Push(Victim, FVector(0, 0, DbT(Boss, TEXT("breachLaunch"), 1200.f)), 0.5f, TEXT("breach"), Boss);
	else if (Is(Boss, TEXT("glass_empress")) && AttackId == TEXT("sting") && S.Plate[5] > 0.f)
		DbHit(Boss, Victim, 0.f, Boss->GetActorLocation(), { { TEXT("blind"), DbT(Boss, TEXT("stingBlind"), 80.f) } }, true);
}

float FWRDesertBoss::Incoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsDesertBoss(Boss)) return Damage;
	FState& S = St(Boss);
	const double T = DbNow(Boss);
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : Hit.Origin;
	if (Is(Boss, TEXT("mirage_matriarch")) && AWRGameState::IsNightIn(Boss) && !WRDesertHz::LongNoon(Boss) && !Boss->bEngaged) return 0.f;
	if (Is(Boss, TEXT("ssarrok")) && T < S.BeachedUntil) return Damage * DbT(Boss, TEXT("beachTaken"), 1.3f);
	if (Is(Boss, TEXT("glass_empress")))
	{
		// Through a plate: x0.3 (the plate takes it: strike blows x2; frost then fire within 4 s shatters it).
		const int32 P = EmpressPlate(Boss, At);
		if (S.Plate[P] <= 0.f) return Damage;
		const bool bFrost = Hit.Element == WRTags::Element_Frost, bFire = Hit.Element == WRTags::Element_Fire;
		if (bFrost) S.FrostAt[P] = T;
		float ToPlate = Damage * (Hit.PhysType == 2 ? DbT(Boss, TEXT("strikePlate"), 2.f) : 1.f);
		if (bFire && T - S.FrostAt[P] <= DbT(Boss, TEXT("shockSeconds"), 4.f)) ToPlate = S.Plate[P] + 1.f;
		S.Plate[P] -= ToPlate;
		if (S.Plate[P] <= 0.f)
		{
			// Shattered: shards 4 m round, 15 to everyone there, her included.
			S.Plate[P] = 0.f;
			WRFx::Broadcast(Boss, TEXT("desert.glass_shatter"), At, FRotator::ZeroRotator, 2.f, TEXT("boss.glass_empress.shatter"));
			for (TActorIterator<AWRCharacterBase> It(Boss->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || *It == Boss || FVector::Dist(It->GetActorLocation(), At) > DbT(Boss, TEXT("shardRadius"), 400.f)) continue;
				DbHit(Boss, *It, DbT(Boss, TEXT("shardDamage"), 15.f), At);
			}
			if (Boss->GetAttributes() && Boss->GetAbilitySystemComponent())
				Boss->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Max(1.f, Boss->GetAttributes()->GetHealth() - DbT(Boss, TEXT("shardDamage"), 15.f)));
			UE_LOG(LogWildreach, Log, TEXT("DESERT the Empress's plate %d shatters (%d left)"), P, PlatesLeft(Boss));
			return Damage;
		}
		return Damage * DbT(Boss, TEXT("plateTaken"), 0.3f);
	}
	if (Is(Boss, TEXT("ahrumet")))
	{
		// Sun-hot stone cracks under water (frost at half the stance): +60 stance, that part x1.5 for the rest of the fight.
		const int32 P = ColossusPart(Boss, At);
		const bool bWater = Hit.Element == WRTags::Element_Water, bFrost = Hit.Element == WRTags::Element_Frost;
		if ((bWater || bFrost) && T < S.HotUntil && !S.Cracked.Contains(P + 10 * Boss->Phase))
		{
			S.Cracked.Add(P + 10 * Boss->Phase);
			S.CrackedEver.Add(P);
			WRFx::Broadcast(Boss, TEXT("desert.stone_crack"), At, FRotator::ZeroRotator, 2.f, TEXT("boss.ahrumet.crack"));
			TakeStance(Boss, DbT(Boss, TEXT("crackStance"), 60.f) * (bFrost ? 0.5f : 1.f), Hit.Instigator.Get());
			if (const APawn* By = Cast<APawn>(Hit.Instigator.Get())) DbTell(By, FString::Printf(TEXT("The sun-hot stone of his %s cracks"), PartName(P)));
		}
		if (S.CrackedEver.Contains(P)) Damage *= DbT(Boss, TEXT("crackTaken"), 1.5f);
		if (Boss->Phase >= 3) Damage *= 1.f;   // (the hidden phase's x1.4 is on his blows: the boss machine's artifact rule)
		return Damage;
	}
	return Damage;
}

bool FWRDesertBoss::OwnWeakness(const AWRBoss* Boss) { (void)Boss; return false; }   // Tide x1.5 is the generic opposite rule

void FWRDesertBoss::Damaged(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	const FVector At = !Hit.ImpactPoint.IsZero() ? Hit.ImpactPoint : Hit.Origin;
	// A held survivor struck free by allies (the throat, the claw, the held wrist).
	if (S.Held.IsValid() && Hit.Instigator.Get() != S.Held.Get())
	{
		if (Is(Boss, TEXT("ahrumet")))
		{
			const int32 P = ColossusPart(Boss, At);
			if (P == 3 || P == 2) S.Freed += Damage;
		}
		else S.Freed += Damage;
	}
	// The Matriarch: blind on the real one breaks every mirage and holds them off 15 s.
	if (Is(Boss, TEXT("mirage_matriarch")))
	{
		const float* Blind = Hit.Buildups.Find(TEXT("blind"));
		if (Blind && *Blind > 0.f)
		{
			PopMirages(Boss, S);
			S.MirageBlockUntil = DbNow(Boss) + DbT(Boss, TEXT("blindBreak"), 15.f);
			S.NextReformAt = 0.0;
			WRSound::PlayAt(Boss, TEXT("boss.mirage_matriarch.broken"), Boss->GetActorLocation());
		}
	}
}

void FWRDesertBoss::PhaseChanged(AWRBoss* Boss, int32 NewPhase)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	if (Is(Boss, TEXT("ahrumet")) && NewPhase == 2 && !S.bUprooted)
	{
		// The Uprooting of the Gnomon: he strides to the centre, tears it out (a 360 degree swing: 60, unblockable; over
		// 14 m away you're clear) and the dial's centre collapses into a 20 m quicksand funnel.
		S.bUprooted = true;
		const FVector C = PlazaAt();
		if (AWRDesertState* DS = AWRDesertState::Get(Boss)) DS->bGnomonUprooted = true;
		Flag(Boss, WRDesertFlag::Dulla, true);
		// The tell first (cookpass-combat 3.10): he grips it (a crimson glint, his voice), the sand cracks round the base
		// for uprootTell s; then the gnomon swings all the way round as an arc: you're struck only where it is.
		const float Tell = DbT(Boss, TEXT("uprootTell"), 1.2f);
		Boss->MulticastTell(2, TEXT("hand"), TEXT("voice"));
		WRFx::Broadcast(Boss, TEXT("desert.sand_burst"), C, FRotator::ZeroRotator, 2.5f, TEXT("boss.ahrumet.uproot"), Tell);
		TWeakObjectPtr<AWRBoss> Weak(Boss);
		AWRStrikeFront::Arc(Boss, Boss->GetActorLocation(), DbT(Boss, TEXT("uprootReach"), 1400.f), Boss->GetActorRotation().Yaw + 180.f, 360.f, DbT(Boss, TEXT("uprootSweep"), 0.8f),
			[Weak](AWRCharacterBase* V)
			{
				AWRBoss* B = Weak.Get();
				if (!B || V == B || UWRDesertBody::Get(V) || V->HasTag(WRTags::State_Dodging)) return;   // rolled through it
				DbHit(B, V, DbT(B, TEXT("uprootDamage"), 60.f), B->GetActorLocation());
			}, TEXT("desert.sand_burst"), TEXT("boss.ahrumet.uproot"), Tell);
		if (UWRDesertHazards* H = UWRDesertHazards::Get(Boss)) H->Funnels.Add({ C, Dd().ArenaNum(TEXT("ahrumet"), TEXT("pit"), 10.f) * 100.f, 1.0e12 });
		UE_LOG(LogWildreach, Log, TEXT("DESERT Ahrumet uproots the gnomon"));
	}
}

bool FWRDesertBoss::PlaceRunback(AWRBoss* Boss)
{
	// The Desert's run-back stones are the land's own spots (WRDesertWorld: the plaza's at the Avenue's foot, the
	// Hourhold's): nothing more here.
	return Is(Boss, TEXT("ahrumet"));
}

FName FWRDesertBoss::BossPart(const AWRBoss* Boss, bool bScarred, FName Fallback)
{
	(void)bScarred;
	if (Is(Boss, TEXT("ahrumet"))) return TEXT("colossus_eye");
	return Fallback;
}

void FWRDesertBoss::Died(AWRBoss* Boss)
{
	if (!IsDesertBoss(Boss) || !Boss->HasAuthority()) return;
	FState& S = St(Boss);
	EndHold(Boss, S, false);
	PopMirages(Boss, S);
	if (Is(Boss, TEXT("ahrumet")))
	{
		// The eye cracks with a glass shatter; the song stops mid-note. The plaza comes back a minute later.
		WRFx::Broadcast(Boss, TEXT("desert.glass_shatter"), EyeAt(Boss), FRotator::ZeroRotator, 3.f, TEXT("boss.ahrumet.death"));
		if (UWRDesertArenas* A = UWRDesertArenas::Get(Boss)) { FTimerHandle H; TWeakObjectPtr<UWRDesertArenas> WA(A); Boss->GetWorldTimerManager().SetTimer(H, [WA]() { if (WA.IsValid()) WA->ResetPlaza(); }, 60.f, false); }
	}
}

int32 FWRDesertBoss::PlatesLeft(const AWRBoss* Boss)
{
	const FState* S = StC(Boss);
	if (!S) return 0;
	int32 N = 0;
	for (const float P : S->Plate) if (P > 0.f) ++N;
	return N;
}

// ================================================================================================ the arenas

UWRDesertArenas* UWRDesertArenas::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UWRDesertArenas>() : nullptr;
}

bool UWRDesertArenas::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId UWRDesertArenas::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRDesertArenas, STATGROUP_Tickables); }

bool UWRDesertArenas::IsTickable() const
{
	const UWorld* W = GetWorld();
	return W && W->GetNetMode() != NM_Client && WRDesert::Live() && UWRDesertWorld::Get(W) && UWRDesertWorld::Get(W)->IsBuilt();
}

void UWRDesertArenas::Tick(float DeltaTime)
{
	PlaceClock += DeltaTime;
	if (PlaceClock < 3.f) return;
	PlaceClock = 0.f;
	for (const TCHAR* Id : { TEXT("ssarrok"), TEXT("mirage_matriarch"), TEXT("glass_empress"), TEXT("ahrumet") })
	{
		const FName B(Id);
		if (!Placed.Contains(B) && Place(B)) Placed.Add(B);
	}
}

void UWRDesertArenas::ResetPlaza()
{
	for (const TWeakObjectPtr<AWRDesertStela>& W : AWRDesertStela::All()) if (W.IsValid()) W->SetBroken(false);
	if (AWRDesertState* DS = AWRDesertState::Get(GetWorld())) DS->bGnomonUprooted = false;
	if (UWRDesertHazards* H = UWRDesertHazards::Get(GetWorld())) H->Funnels.Reset();
}

bool UWRDesertArenas::Place(FName BossId)
{
	UWorld* World = GetWorld();
	const FWRDesertData& D = Dd();
	if (!FWRCreatureRegistry::Get().Find(BossId)) return true;
	bool bNear = false;
	for (TActorIterator<AWRCharacter> It(World); It && !bNear; ++It) bNear = It->IsAlive() && WRDesert::Contains(It->GetActorLocation());
	if (!bNear) return false;
	for (TActorIterator<AWRCreatureSpawner> It(World); It; ++It) if (It->CreatureId == BossId) return true;
	FVector Start;
	float Yaw = 0.f;
	if (BossId == TEXT("ssarrok"))
	{
		if (!D.ArenaPoint(TEXT("ssarrok"), TEXT("lair"), Start)) return true;
	}
	else if (BossId == TEXT("mirage_matriarch") || BossId == TEXT("glass_empress"))
	{
		if (!D.ArenaPoint(*BossId.ToString(), TEXT("at"), Start)) return true;
	}
	else if (BossId == TEXT("ahrumet"))
	{
		Start = ThroneAt();
		Yaw = (PlazaAt() - Start).Rotation().Yaw;
		if (!bStelae)
		{
			// The twelve hour-stelae on their ring (4 m tall, 2 m wide), each facing the gnomon.
			bStelae = true;
			const FVector C = PlazaAt();
			const float R = D.ArenaNum(TEXT("ahrumet"), TEXT("stelaRing"), 25.f) * 100.f;
			const int32 N = int32(D.ArenaNum(TEXT("ahrumet"), TEXT("stelae"), 12.f));
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 360.f * i / N;
				FVector P = C + FRotator(0.f, A, 0.f).Vector() * R;
				float G = C.Z;
				WRLand::GroundUnder(P + FVector(0, 0, 20000.f), G);
				P.Z = G + 200.f;
				const FTransform Tr(FRotator(0.f, A, 0.f), P);
				if (AWRDesertStela* Sx = World->SpawnActorDeferred<AWRDesertStela>(AWRDesertStela::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)) Sx->FinishSpawning(Tr);
			}
		}
	}
	else return true;
	float G = Start.Z;
	if (WRLand::GroundUnder(Start + FVector(0, 0, 20000.f), G)) Start.Z = G;
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(BossId);
	Start.Z += (Def ? Def->HalfHeight : 200.f) + 30.f;
	const FTransform Where(FRotator(0.f, Yaw, 0.f), Start);
	if (AWRCreatureSpawner* Spawner = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Spawner->CreatureId = BossId;
		Spawner->Count = 1;
		Spawner->Radius = 1.f;
		Spawner->RespawnSeconds = WRTune(TEXT("server.bossRespawnHours"), 4.f) * 3600.f;
		Spawner->ActivationRadius = 0.f;
		Spawner->bTraceGround = true;
		Spawner->FinishSpawning(Where);
	}
	UE_LOG(LogWildreach, Log, TEXT("DESERT arena %s placed at %s"), *BossId.ToString(), *Start.ToCompactString());
	return true;
}
