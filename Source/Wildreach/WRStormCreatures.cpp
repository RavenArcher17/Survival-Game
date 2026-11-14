// The Storm peaks creatures (cooks/biome-lightning.md 2.9; cooks/build-biome-lightning.md 1.3, 2.6). See WRStormCreatures.h.
#include "WRStormCreatures.h"
#include "WRStormBosses.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// ---------------------------------------------------------------- shared bits

namespace WRStormDetail
{
	/** creatures.json rows by id, read once (the look and the tame-use keys the def doesn't carry). */
	TSharedPtr<FJsonObject> ScRow(FName Id)
	{
		static TMap<FName, TSharedPtr<FJsonObject>> Cache;
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
					FString IdStr;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), IdStr)) Cache.Add(FName(*IdStr), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Found = Cache.Find(Id);
		return Found ? *Found : nullptr;
	}

	float ScNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}

	/** A colour from data: an sRGB hex string ("#8A8D91", as the cook writes them) or a linear [r, g, b]. */
	FLinearColor ScColor(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(Field, S)) return FLinearColor(FColor::FromHex(S));
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 3) return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		return Fallback;
	}

	FLinearColor ScHex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }

	double ScNow(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }

	bool ScInStorm(const AActor* A) { return A && WRStorm::InStorm(A, A->GetActorLocation()); }

	bool ScWild(const AWRCreature* C) { return C && C->IsAlive() && !C->IsTamed(); }

	FVector ScGround(const UWorld* World, const FVector& At, bool& bOk, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRStormGround), false, Ignore);
		bOk = World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Q);
		return bOk ? Hit.ImpactPoint : At;
	}

	AWRCharacter* ScOwnerBody(const UWorld* World, const FString& Name)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			const APlayerState* PS = It->GetPlayerState();
			if (PS && PS->GetPlayerName() == Name && It->IsAlive()) return *It;
		}
		return nullptr;
	}

	APlayerController* ScOwnerPC(const UWorld* World, const FString& Name)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		for (APlayerState* PS : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			if (PS && PS->GetPlayerName() == Name) return Cast<APlayerController>(PS->GetOwner());
		}
		return nullptr;
	}

	/** How much metal it wears (kg of worn and held pieces with a metal): Coilback's sense, the star nose's sparks. */
	float ScMetalWorn(const AActor* Who)
	{
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Who);
		if (!Inv) return 0.f;
		float Kg = 0.f;
		for (const EWREquipSlot S : WRGearSlots::All)
		{
			const FWRItemStack& Stack = Inv->GetEquipped(S);
			const FWRItemDef* Def = Stack.ItemId.IsNone() ? nullptr : FWRItemRegistry::Get().Find(Stack.ItemId);
			if (Def && !Def->Metal.IsNone() && Def->Metal != TEXT("wood") && Def->Metal != TEXT("stone") && Def->Metal != TEXT("hide") && Def->Metal != TEXT("leather") && Def->Metal != TEXT("bone"))
			{
				Kg += FMath::Max(0.5f, Def->Weight);
			}
		}
		return Kg;
	}

	/** The nearest living thing a Raiju would hunt: a survivor, a tame or a wild creature (not its own kind, no boss). */
	AWRCharacterBase* ScNearestPrey(const AWRCreature* Self, float Range)
	{
		AWRCharacterBase* Best = nullptr;
		float BestD = Range;
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !It->IsAlive() || It->IsA<AWRBoss>() || It->IsHidden()) continue;
			if (const AWRCreature* C = Cast<AWRCreature>(*It); C && C->CreatureId == Self->CreatureId) continue;
			const float D = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** Sets a wild creature on what it hunts (a survivor or tame: its aggro; a wild one: a hunt). */
	void ScHunt(AWRCreature* Self, AWRCharacterBase* Prey)
	{
		if (!Self || !Prey) return;
		if (AWRCreature* C = Cast<AWRCreature>(Prey); C && !C->IsTamed()) Self->HuntTarget = C;
		Self->SetAggroTarget(Prey);
	}

	FName ScKind(FName Id)
	{
		const TSharedPtr<FJsonObject> R = ScRow(Id);
		const TSharedPtr<FJsonObject>* Look = nullptr;
		FString S;
		if (R.IsValid() && R->TryGetObjectField(TEXT("look"), Look) && (*Look)->TryGetStringField(TEXT("kind"), S)) return FName(*S);
		return Id;
	}
}
using namespace WRStormDetail;

// ---------------------------------------------------------------- the body component

UWRStormBody::UWRStormBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRStormBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRStormBody, Charge);
	DOREPLIFETIME(UWRStormBody, Flags);
	DOREPLIFETIME(UWRStormBody, EyeAt);
}

UWRStormBody* UWRStormBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRStormBody>() : nullptr;
}

bool UWRStormBody::IsStormKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("storm_peaks");
}

void UWRStormBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsStormKind(Creature->CreatureId) || Get(Creature))
	{
		return;
	}
	UWRStormBody* Body = NewObject<UWRStormBody>(Creature, TEXT("StormBody"));
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

void UWRStormBody::BeginPlay()
{
	Super::BeginPlay();
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!C || !Def)
	{
		return;
	}
	// Movement every machine that moves it agrees on: the Cragstrider climbs 60 deg slopes and jumps 3 m (its rider's
	// machine predicts it too), the slope from creatures.json climbSlope, the jump from its height.
	const TSharedPtr<FJsonObject> R = ScRow(C->CreatureId);
	if (const float Slope = ScNum(R, TEXT("climbSlope"), 0.f); Slope > 0.f)
	{
		C->GetCharacterMovement()->SetWalkableFloorAngle(Slope);
		C->GetCharacterMovement()->MaxStepHeight = FMath::Max(C->GetCharacterMovement()->MaxStepHeight, Def->HalfHeight * 0.6f);
	}
	if (const float Jump = ScNum(R, TEXT("jumpHeight"), 0.f); Jump > 0.f)
	{
		C->GetCharacterMovement()->JumpZVelocity = FMath::Sqrt(2.f * 980.f * Jump);
	}
	if (C->HasAuthority())
	{
		NextSenseAt = ScNow(this) + FMath::FRandRange(5.f, 15.f);
		NextDigAt = ScNow(this) + WRTune(TEXT("tame.storm.digSeconds"), 600.f);
		NextScoutAt = ScNow(this) + FMath::FRandRange(10.f, 30.f);
	}
}

void UWRStormBody::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearLook();
	Super::EndPlay(Reason);
}

void UWRStormBody::MulticastMoment_Implementation(FName Effect, FName Bone, float Scale)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FName B = C->FindBone(Bone);
	const FVector At = B.IsNone() || !Skel() ? C->GetActorLocation() : Skel()->GetBoneLocation(B);
	WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
}

USkeletalMeshComponent* UWRStormBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRStormBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	if (GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLook(DeltaTime);
	}
}

// ---------------------------------------------------------------- server life

void UWRStormBody::TickServer(float DeltaTime)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!C || !Def || !C->IsAlive() || C->IsA<AWRBoss>())
	{
		return;   // bosses: FWRStormBoss
	}
	const double T = ScNow(this);
	const FName Id = C->CreatureId;
	const bool bStorm = ScInStorm(C);
	const FName K = ScKind(Id);

	if (K == TEXT("voltfang"))
	{
		// Always charged in a storm; out of one, the grit roll charges it (FWRStormAI::TickWild).
		if (bStorm) Set(WRStormFlag::Charged, true);
		if (RollUntil > 0.0 && T >= RollUntil)
		{
			RollUntil = 0.0;
			Set(WRStormFlag::Charged, true);
			MulticastMoment(TEXT("charge.full"), TEXT("spine"), 1.6f);
		}
	}
	else if (K == TEXT("stormkite"))
	{
		Set(WRStormFlag::Grounded, T < GroundedUntil);
	}
	else if (K == TEXT("puffram"))
	{
		Set(WRStormFlag::Charged, bStorm);   // the wool stands out with static
	}
	else if (K == TEXT("rodcrest"))
	{
		Set(WRStormFlag::Lit, T < LitUntil);
		Set(WRStormFlag::CrestUp, bStorm || C->IsFleeing() || C->GetAggroTarget() != nullptr || T < LitUntil);
		// In a storm they climb their basking rocks (and take the strikes there).
		if (bStorm && !C->IsTamed() && C->GetAggroTarget() == nullptr && !C->IsFleeing() && FMath::Fmod(Clock, 10.f) < DeltaTime)
		{
			FVector Rock;
			if (FWRStormBoss::NearestKit(C->GetWorld(), TEXT("basking_rock"), C->GetActorLocation(), 6000.f, Rock)) C->SetHome(Rock);
		}
	}
	else if (K == TEXT("raiju"))
	{
		Set(WRStormFlag::Lit, bStorm);
		if (!C->IsTamed() && !C->IsUnconscious())
		{
			// It comes in a strike (once, on its first moment in the world).
			if (!bReturning && ArriveCalledAt < 0.0 && CombatSince < 0.0 && !Has(WRStormFlag::Arriving))
			{
				FWRStormAI::BeginArrival(C, C->GetActorLocation());
			}
			// Its strike never came (refused, lost): it steps out of the dark where it was.
			if (Has(WRStormFlag::Arriving) && ArriveCalledAt > 0.0 && T - ArriveCalledAt > 4.0)
			{
				FWRStrikeCall Fake;
				Fake.Spot = ArriveSpot;
				Fake.Instigator = C;
				Fake.Source = EWRStrikeSource::Creature;
				FWRStormAI::StrikeLanded(C->GetWorld(), Fake);
			}
			if (C->GetAggroTarget() && CombatSince < 0.0) CombatSince = T;
			// Storm return: after a minute of fighting, or when the storm ends, it leaps into the next strike near it.
			if (!bReturning && !Has(WRStormFlag::Arriving) && ((CombatSince > 0.0 && T - CombatSince > WRTune(TEXT("creatureAI.raiju.returnSeconds"), 60.f)) || !bStorm))
			{
				bReturning = true;
				ReturnCalledAt = T;
				bool bOk = false;
				const FVector Spot = ScGround(C->GetWorld(), C->GetActorLocation() + FVector(FMath::RandPointInCircle(1.f).GetSafeNormal() * FMath::FRandRange(300.f, 500.f), 0.f), bOk, C);
				ArriveSpot = Spot;
				FWRStrikeCall Call;
				Call.Spot = Spot;
				Call.Tell = WRTune(TEXT("hazards.strike.tell"), 1.f);
				Call.Damage = WRTune(TEXT("hazards.strike.damage"), 60.f);
				Call.Shock = 40.f;
				Call.Source = EWRStrikeSource::Creature;
				Call.Instigator = C;
				Call.bRodsTake = false;
				if (!WRStorm::CallStrike(C, Call))
				{
					// Nowhere for a strike: it's gone in a flash where it stands.
					WRFx::Broadcast(C, TEXT("weather.strike"), C->GetActorLocation(), FRotator::ZeroRotator, 0.6f, TEXT("storm.strike"));
					UE_LOG(LogWildreach, Log, TEXT("STORM %s returns to the storm (flash)"), *C->GetName());
					C->Destroy();
					return;
				}
				UE_LOG(LogWildreach, Log, TEXT("STORM %s leaps for the storm's next strike"), *C->GetName());
			}
			if (bReturning)
			{
				C->SetAggroTarget(nullptr);
				const FVector To = ArriveSpot - C->GetActorLocation();
				if (To.Size2D() > 60.f) C->AddMovementInput(To.GetSafeNormal2D(), 1.f);
				if (T - ReturnCalledAt > 5.0)
				{
					C->Destroy();   // gone (no drops, no XP)
					return;
				}
			}
		}
	}
	else if (K == TEXT("cragstrider") && !C->IsTamed() && T - AlarmAt > WRTune(TEXT("creatureAI.cragstrider.alarmCooldown"), 60.f))
	{
		// The sneeze-whistle: a survivor coming close puts everything within 40 m on alert.
		for (TActorIterator<AWRCharacter> It(C->GetWorld()); It; ++It)
		{
			if (It->IsAlive() && FVector::Dist(It->GetActorLocation(), C->GetActorLocation()) < WRTune(TEXT("creatureAI.cragstrider.noticeRadius"), 1800.f) && !C->IsAsleep())
			{
				FWRStormAI::Damaged(C, FWRHit(), 0.f);   // the alarm (no blow): it sets AlarmAt
				break;
			}
		}
	}

	// Star noses spark near metal.
	if ((K == TEXT("coilburrow")) && FMath::Fmod(Clock, 0.5f) < DeltaTime)
	{
		bool bMetal = false;
		for (TActorIterator<AWRCharacter> It(C->GetWorld()); It && !bMetal; ++It)
		{
			bMetal = It->IsAlive() && FVector::Dist(It->GetActorLocation(), C->GetActorLocation()) < 1000.f && ScMetalWorn(*It) > 0.f;
		}
		Set(WRStormFlag::Sensing, bMetal || (C->IsTamed() && T < ScoutUntil));
	}

	// --- Tame uses ---
	if (!C->IsTamed() || C->IsBaby())
	{
		return;
	}
	const TSharedPtr<FJsonObject> R = ScRow(Id);
	AWRCharacter* Owner = ScOwnerBody(C->GetWorld(), C->TamedBy);
	if (ScNum(R, TEXT("senseOre"), 0.f) > 0.f && Owner && C->TameOrder == EWRTameOrder::Follow)
	{
		// Coilburrow: copper, tin and iron within 40 m marked for its owner; two ore dug from a node every 10 minutes.
		const float Range = ScNum(R, TEXT("senseOre"), 4000.f);
		if (T >= NextSenseAt)
		{
			NextSenseAt = T + WRTune(TEXT("tame.storm.senseSeconds"), 20.f);
			int32 Found = 0;
			APlayerController* PC = ScOwnerPC(C->GetWorld(), C->TamedBy);
			for (TActorIterator<AWRResourceNode> It(C->GetWorld()); It; ++It)
			{
				const FString Item = It->ItemId.ToString();
				if (It->IsDepleted() || !(Item == TEXT("copper_ore") || Item == TEXT("tin_ore") || Item == TEXT("iron_ore"))) continue;
				if (FVector::Dist(It->GetActorLocation(), C->GetActorLocation()) > Range) continue;
				if (PC) AWRStormMark::Spawn(PC, It->GetActorLocation(), *It, TEXT("ore"), WRTune(TEXT("tame.storm.senseMarkSeconds"), 20.f));
				++Found;
			}
			ScoutUntil = Found > 0 ? T + 3.0 : ScoutUntil;   // (the nose sparks while it points them out)
			if (Found > 0)
			{
				if (AWRPlayerController* WPC = Cast<AWRPlayerController>(PC)) WPC->ClientNotify(FString::Printf(TEXT("Your %s senses ore: %d vein%s within %.0f m"), *Def->Name, Found, Found == 1 ? TEXT("") : TEXT("s"), Range / 100.f));
			}
		}
		if (T >= NextDigAt && FVector::Dist(Owner->GetActorLocation(), C->GetActorLocation()) < 2500.f)
		{
			NextDigAt = T + WRTune(TEXT("tame.storm.digSeconds"), 600.f);
			AWRResourceNode* Best = nullptr;
			float BestD = Range;
			for (TActorIterator<AWRResourceNode> It(C->GetWorld()); It; ++It)
			{
				const FString Item = It->ItemId.ToString();
				if (It->IsDepleted() || !Item.EndsWith(TEXT("_ore"))) continue;
				const float D = FVector::Dist(It->GetActorLocation(), C->GetActorLocation());
				if (D < BestD) { BestD = D; Best = *It; }
			}
			if (Best)
			{
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Owner))
				{
					const int32 N = FMath::RoundToInt(WRTune(TEXT("tame.storm.digOre"), 2.f));
					Inv->AddItem(Best->ItemId, N);
					WRFx::Broadcast(C, TEXT("burst"), Best->GetActorLocation(), FRotator::ZeroRotator, 0.5f, TEXT("storm.burrow"));
					C->GainBondXP(WRTune(TEXT("tame.xp.dig"), 3.f), TEXT("dig"));
					if (AWRPlayerController* WPC = Cast<AWRPlayerController>(ScOwnerPC(C->GetWorld(), C->TamedBy))) WPC->ClientNotify(FString::Printf(TEXT("Your %s dug up %d %s"), *Def->Name, N, *Best->ItemId.ToString().Replace(TEXT("_"), TEXT(" "))));
				}
			}
		}
	}
}

// ---------------------------------------------------------------- the look

namespace WRStormLook
{
	using namespace WRLookKit;

	enum EGroup : int32 { GrpStatic = 0, GrpGlow = 1, GrpBell = 2, GrpCrest = 3, GrpFeelers = 4, GrpLegL = 5, GrpLegR = 6, GrpWool = 7, GrpTail = 8, GrpWingGlow = 9, GrpBand0 = 20 };

	/** A ram's horn: a spiral that starts up from the skull, curls back, down and forward, wider each turn (a plane
	 *  tilted out from the head), thick at the base and tapering; ridged; its last share another surface (copper tips). */
	void Horn(FKit& K, const FVector& Base, float Side, float R0, float Turns, float Thick0, float Thick1, float Drift, const FSurf& Body, const FSurf& Tip, float TipShare, float Chip, int32 Segs)
	{
		const FVector U(-1.f, 0.f, 0.f);
		const float Tilt = FMath::DegreesToRadians(28.f);
		const FVector V = FVector(0.f, Side * FMath::Sin(Tilt), -FMath::Cos(Tilt)).GetSafeNormal();
		const float Total = Turns * 2.f * PI;
		const FVector C = Base + U * R0;
		FVector Prev = Base;
		for (int32 i = 1; i <= Segs; ++i)
		{
			const float A = float(i) / Segs;
			const float T = Total * A;
			const float Th = PI + T;
			const float R = R0 * (1.f - 0.45f * A);
			const FVector P = C + (U * FMath::Cos(Th) + V * FMath::Sin(Th)) * R + FVector(0.f, Side * Drift * T, 0.f);
			const float Wear = Chip > 0.f ? 1.f - Chip * K.Rand(P, 31 + i) : 1.f;
			const float D = FMath::Lerp(Thick0, Thick1, (i - 0.5f) / Segs) * Wear;
			K.Rod(A > 1.f - TipShare ? Tip : (i % 2 ? Body : Body.Dim(0.86f)), Prev, P, D);
			Prev = P;
		}
	}

	/** A fern of light: a branching line (the Lichtenberg figure under a Raiju's skin), in the YZ... XZ plane at Y. */
	void Fern(FKit& K, const FSurf& S, const FVector& From, const FVector& Dir, float Len, float Diam, int32 Depth)
	{
		const FVector To = From + Dir * Len;
		K.Rod(S, From, To, Diam);
		if (Depth <= 0) return;
		const FVector Side = FVector::CrossProduct(Dir, FVector(0.f, 1.f, 0.f)).GetSafeNormal();
		for (const float Turn : { -38.f, 33.f })
		{
			const FVector D = Dir.RotateAngleAxis(Turn, FVector(0.f, 1.f, 0.f));
			Fern(K, S, From + Dir * Len * (Turn < 0.f ? 0.55f : 0.8f), D, Len * 0.58f, Diam * 0.75f, Depth - 1);
		}
		(void)Side;
	}
}

USceneComponent* UWRStormBody::AddGroup(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const TArray<FWRLookBlock>& Blocks, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || Blocks.Num() == 0)
	{
		return nullptr;
	}
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Body, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
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
	G.BaseScale = Frame->GetRelativeScale3D();
	Groups.Add(G);
	return Frame;
}

void UWRStormBody::SetGroupGlow(int32 Kind, float Glow)
{
	for (FGroup& G : Groups)
	{
		if (G.Kind != Kind || !G.Frame.IsValid() || FMath::IsNearlyEqual(G.Glow, Glow, 0.01f)) continue;
		G.Glow = Glow;
		TArray<USceneComponent*> Children;
		G.Frame->GetChildrenComponents(false, Children);
		for (USceneComponent* Ch : Children)
		{
			if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Ch); ISM && ISM->NumCustomDataFloats >= 10)
			{
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i) ISM->SetCustomDataValue(i, 8, Glow, i == ISM->GetInstanceCount() - 1);
			}
		}
	}
}

void UWRStormBody::ClearLook()
{
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	Groups.Reset();
	if (LoopFx) { WRFx::Stop(LoopFx); LoopFx = nullptr; }
	if (EyeFx) { WRFx::Stop(EyeFx); EyeFx = nullptr; }
	if (EyeFrame) { EyeFrame->DestroyComponent(); EyeFrame = nullptr; }
	LookFor = NAME_None;
}

void UWRStormBody::BuildLook()
{
	using namespace WRLookKit;
	using namespace WRStormLook;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset() || !Body->IsVisible())
	{
		return;
	}
	ClearLook();
	LookFor = C->CreatureId;
	const TSharedPtr<FJsonObject> R = ScRow(C->CreatureId);
	const TSharedPtr<FJsonObject>* LookObj = nullptr;
	const TSharedPtr<FJsonObject> Look = R.IsValid() && R->TryGetObjectField(TEXT("look"), LookObj) ? *LookObj : nullptr;

	// Its palette (the cook's colours) over the rig's own texture: M_CreatureTint, two tones from the texture's light.
	TintMats.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (Look.IsValid() && Master && Look->TryGetArrayField(TEXT("tint"), Tints))
	{
		const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
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
			M->SetScalarParameterValue(TEXT("Desaturation"), ScNum(Pick, TEXT("desat"), 1.f));
			M->SetScalarParameterValue(TEXT("Brightness"), ScNum(Pick, TEXT("bright"), 1.f));
			M->SetScalarParameterValue(TEXT("TwoTone"), ScNum(Pick, TEXT("twoTone"), 0.f));
			M->SetScalarParameterValue(TEXT("LightFrom"), ScNum(Pick, TEXT("lightFrom"), 0.f));
			M->SetScalarParameterValue(TEXT("Roughness"), ScNum(Pick, TEXT("rough"), 0.75f));
			M->SetScalarParameterValue(TEXT("GlowStrength"), ScNum(Pick, TEXT("glowStrength"), 0.f));
			M->SetVectorParameterValue(TEXT("Tint"), ScColor(Pick, TEXT("tint"), FLinearColor::White));
			M->SetVectorParameterValue(TEXT("TintLight"), ScColor(Pick, TEXT("light"), ScColor(Pick, TEXT("tint"), FLinearColor::White)));
			M->SetVectorParameterValue(TEXT("GlowColor"), ScColor(Pick, TEXT("glow"), ScHex(TEXT("#B9A7FF"))));
			Body->SetMaterial(Slot, M);
			TintMats.Add(M);
		}
	}

	// The silhouette parts, modelled at its size from its own bones.
	const float H = Def->HalfHeight, Rad = Def->Radius;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Up = FVector::UpVector;
	const FTransform W = FTransform::Identity;
	const float BackLift = ScNum(Look, TEXT("backLift"), 0.24f) * H;
	const FName K = ScKind(C->CreatureId);
	const auto BoneAt = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FName B = C->FindBone(FName(Hint));
		if (!B.IsNone()) Out = Body->GetBoneLocation(B);
		return B;
	};
	// The furthest bone of a chain (a tail's tip, a wing's end) by name.
	const auto ChainEnd = [&](const TCHAR* Hint, FVector& Out) -> FName
	{
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		FName Best;
		float BestD = -1.f;
		const FVector Mid = C->GetActorLocation();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (!N.Contains(Hint) || N.Contains(TEXT("end")) || N.Contains(TEXT("nub"))) continue;
			const FVector P = Body->GetBoneLocation(Ref.GetBoneName(i));
			const float D = FVector::Dist(P, Mid);
			if (D > BestD) { BestD = D; Best = Ref.GetBoneName(i); Out = P; }
		}
		return Best;
	};
	// The front of the body at a height: the snout's tip (a mesh's bounds, along its facing).
	const auto FrontAt = [&](float Z) -> FVector
	{
		const FBoxSphereBounds B = Body->Bounds;
		const float Ext = FMath::Abs(Fwd.X) * B.BoxExtent.X + FMath::Abs(Fwd.Y) * B.BoxExtent.Y;
		const float Along = FVector::DotProduct(B.Origin - C->GetActorLocation(), Fwd) + Ext;
		FVector P = C->GetActorLocation() + Fwd * Along;
		P.Z = Z;
		return P;
	};
	const auto Blocks = [&](TFunctionRef<void(FKit&)> Fn) { TArray<FWRLookBlock> Out; FKit Kit(W, Out); Fn(Kit); return Out; };
	// Along the back: a group on each spine bone (the parts follow the body as it bends).
	static const TCHAR* Spine[] = { TEXT("neck"), TEXT("spine3"), TEXT("spine2"), TEXT("spine1"), TEXT("pelvis") };

	if (K == TEXT("voltfang"))
	{
		// The crest: a ridge of fur from nape to tail that stands up, slate-grey with white guard-hair tips (lit when
		// charged, violet-white).
		const FSurf Fur = FSurf::Cloth(ScHex(TEXT("#4A5058")));
		const FSurf Tip = FSurf::Lit(ScHex(TEXT("#E9E6FF")), 0.f);
		FVector Prev = FVector::ZeroVector;
		for (int32 b = 0; b < 5; ++b)
		{
			FVector At;
			const FName Bone = BoneAt(Spine[b], At);
			if (Bone.IsNone()) continue;
			const float Seg = Prev.IsZero() ? H * 0.32f : FMath::Clamp(float(FVector::Dist2D(At, Prev)), H * 0.15f, H * 0.6f);
			Prev = At;
			const FVector Top = At + Up * BackLift;
			const int32 N = 4;
			TArray<FVector> Bases;
			for (int32 i = 0; i < N; ++i) Bases.Add(FVector(-Seg * (i + 0.5f) / N, 0.f, 0.f));
			const float Tall = H * (b == 0 ? 0.13f : b == 4 ? 0.1f : 0.17f);
			AddGroup(Bone, Top, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < N; ++i)
				{
					const float L = Tall * (0.85f + 0.3f * Kit.Rand(Bases[i], 5 + b));
					Kit.Spike(Fur, Bases[i] - FVector(0.f, 0.f, H * 0.04f), FVector(-0.35f, 0.f, 1.f), H * 0.075f, L);
				}
			}), WRStormLook::GrpStatic);
			AddGroup(Bone, Top, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < N; ++i)
				{
					const float L = Tall * (0.85f + 0.3f * Kit.Rand(Bases[i], 5 + b));
					const FVector D = FVector(-0.35f, 0.f, 1.f).GetSafeNormal();
					Kit.Spike(Tip, Bases[i] - FVector(0.f, 0.f, H * 0.04f) + D * L * 0.78f, D, H * 0.032f, L * 0.32f);
				}
			}), WRStormLook::GrpGlow);
		}
	}
	else if (K == TEXT("thunderhorn") || K == TEXT("bellwether"))
	{
		const bool bBell = K == TEXT("bellwether");
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			// Horns curling a turn and a half (the Bellwether: three, each curl as thick as a man's waist, chipped from a
			// hundred fights), ridged amber with copper-green tips where they lick the copper outcrops.
			const FSurf HornS = FSurf::Glaze(ScHex(TEXT("#A8823E")), 0.55f);
			FSurf TipS = FSurf::Steel(ScHex(TEXT("#4E9A84")), 0.45f);
			TipS.Metal = 0.55f;
			const float R0 = H * (bBell ? 0.3f : 0.24f);
			AddGroup(HeadBone, Head + Up * H * 0.06f, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					Horn(Kit, FVector(-H * 0.02f, Side * H * 0.12f, H * 0.08f), Side, R0, bBell ? 3.f : 1.5f, H * (bBell ? 0.2f : 0.15f), H * 0.045f,
						H * (bBell ? 0.012f : 0.018f), HornS, TipS, bBell ? 0.08f : 0.14f, bBell ? 0.3f : 0.f, bBell ? 72 : 30);
				}
			}), WRStormLook::GrpStatic);
		}
		// The heavy ruff on the neck: granite wool hanging in locks.
		FVector Neck;
		const FName NeckBone = BoneAt(TEXT("neck"), Neck);
		if (!NeckBone.IsNone())
		{
			const FSurf Wool = FSurf::Cloth(ScHex(TEXT("#8A8D91")).Desaturate(0.f) * 0.8f);
			AddGroup(NeckBone, Neck, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 12; ++i)
				{
					const float A = 2.f * PI * i / 12.f;
					const FVector Out(0.f, FMath::Cos(A), FMath::Sin(A));
					Kit.Spike(Wool, Out * H * 0.16f, (Out + FVector(-0.4f, 0.f, -0.6f)).GetSafeNormal(), H * 0.1f, H * (0.22f + 0.08f * Kit.Rand(Out, 9)));
				}
			}), WRStormLook::GrpStatic);
			if (bBell)
			{
				// The copper collar round the neck and the cracked bronze bell hung under it on a shackle; the bell
				// swings on its shackle (a spring from how he moves: TickLook).
				const FSurf Copper = FSurf::Steel(ScHex(TEXT("#B87333")), 0.38f);
				const FSurf Verd = FSurf::Stone(ScHex(TEXT("#4E9A84")), 0.6f);
				const FSurf Bronze = FSurf::Steel(ScHex(TEXT("#8C6A3A")), 0.42f);
				const FSurf Crack = FSurf::Stone(ScHex(TEXT("#1C1611")), 0.9f);
				const float Rc = H * 0.2f;
				AddGroup(NeckBone, Neck, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 i = 0; i < 16; ++i)
					{
						const float A = 2.f * PI * i / 16.f;
						const FVector P(0.f, FMath::Cos(A) * Rc, FMath::Sin(A) * Rc);
						Kit.Add(i % 5 == 0 ? Verd : Copper, FVector(22.f, 2.f * PI * Rc / 16.f + 2.f, 6.f), P, FRotator(0.f, 0.f, FMath::RadiansToDegrees(A) - 90.f));
					}
					Kit.Rod(Copper, FVector(-4.f, 0.f, -Rc), FVector(-4.f, 0.f, -Rc - 14.f), 5.f);   // the shackle's stem
				}), WRStormLook::GrpStatic);
				AddGroup(NeckBone, Neck - Up * (Rc + 14.f), Yaw, Blocks([&](FKit& Kit)
				{
					// 1 m of bell: the crown, the waist flaring to the lip, the sound bow; the crack down one side.
					Kit.Disc(Bronze, FVector(0.f, 0.f, -8.f), Up, 38.f, 14.f);
					static const float Waist[][2] = { { -20.f, 46.f }, { -35.f, 52.f }, { -50.f, 58.f }, { -64.f, 66.f }, { -78.f, 76.f }, { -90.f, 88.f } };
					for (const auto& Wz : Waist) Kit.Disc(Bronze, FVector(0.f, 0.f, Wz[0]), Up, Wz[1], 15.f);
					Kit.Disc(Bronze.Dim(0.8f), FVector(0.f, 0.f, -99.f), Up, 98.f, 6.f);
					Kit.Rod(Bronze.Dim(0.6f), FVector(0.f, 0.f, -12.f), FVector(0.f, 0.f, -82.f), 6.f);   // the clapper's shaft
					Kit.Disc(Bronze.Dim(0.6f), FVector(0.f, 0.f, -86.f), Up, 16.f, 12.f);                 // and its ball
					for (int32 s = 0; s < 6; ++s)
					{
						const float Z = -18.f - s * 14.f;
						Kit.Add(Crack, FVector(3.f, 2.5f, 15.f), FVector(23.f + s * 3.6f, 3.f * FMath::Sin(s * 1.3f), Z), FRotator(-12.f, 0.f, 0.f));
					}
				}), WRStormLook::GrpBell);
			}
		}
	}
	else if (K == TEXT("stormkite") || K == TEXT("galvara"))
	{
		// The deeply forked tail of a swallow-tailed kite: two long vanes, slate with black edges, the fork's inner
		// feathers lit violet-white before a dive. Galvara: a whip of a tail ending in a scissor fork, a crest of quills,
		// and violet-white light along her flight feathers.
		const bool bQueen = K == TEXT("galvara");
		FVector TailTip;
		const FName TailBone = ChainEnd(TEXT("tail"), TailTip);
		const FSurf Slate = FSurf::Cloth(ScHex(TEXT("#3E4650")));
		const FSurf Black = FSurf::Cloth(ScHex(TEXT("#121417")));
		const FSurf Violet = FSurf::Lit(ScHex(TEXT("#B9A7FF")), 0.f);
		const float L = H * (bQueen ? 1.6f : 1.1f);
		if (!TailBone.IsNone())
		{
			AddGroup(TailBone, TailTip, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					const FVector D = FVector(-1.f, Side * 0.32f, -0.08f).GetSafeNormal();
					const FRotator Rot = D.Rotation();
					Kit.Add(Slate, FVector(L, H * 0.16f, 2.f), D * L * 0.5f, Rot);
					Kit.Add(Black, FVector(L * 0.5f, H * 0.05f, 2.5f), D * L * 0.82f + FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * Side * H * 0.06f, Rot);
				}
			}), WRStormLook::GrpTail);
			AddGroup(TailBone, TailTip, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					const FVector D = FVector(-1.f, Side * 0.22f, -0.05f).GetSafeNormal();
					Kit.Add(Violet, FVector(L * 0.55f, H * 0.035f, 2.5f), D * L * 0.4f - FRotationMatrix(D.Rotation()).GetUnitAxis(EAxis::Y) * Side * H * 0.04f, D.Rotation());
				}
			}), WRStormLook::GrpGlow);
		}
		if (bQueen)
		{
			FVector Head;
			const FName HeadBone = BoneAt(TEXT("head"), Head);
			if (!HeadBone.IsNone())
			{
				AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 i = 0; i < 9; ++i)
					{
						const float S = (i - 4) / 4.f;
						Kit.Spike(i % 2 ? Slate : Black, FVector(-H * 0.02f, S * H * 0.08f, H * 0.08f), FVector(-1.f, S * 0.4f, 0.55f).GetSafeNormal(), H * 0.025f, H * (0.32f - 0.08f * FMath::Abs(S)));
					}
				}), WRStormLook::GrpStatic);
			}
			// Light along the flight feathers: strips on the wing bones' trailing edges.
			const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
			for (int32 i = 0; i < Ref.GetNum(); ++i)
			{
				const FString N = Ref.GetBoneName(i).ToString().ToLower();
				if (!N.Contains(TEXT("wing")) || N.Contains(TEXT("end"))) continue;
				const FVector P = Body->GetBoneLocation(Ref.GetBoneName(i));
				const float Side = FVector::DotProduct(P - C->GetActorLocation(), Right) >= 0.f ? 1.f : -1.f;
				AddGroup(Ref.GetBoneName(i), P, Yaw, Blocks([&](FKit& Kit)
				{
					Kit.Add(FSurf::Lit(ScHex(TEXT("#B9A7FF")), 1.6f), FVector(H * 0.5f, H * 0.03f, 2.f), FVector(-H * 0.22f, Side * H * 0.05f, 0.f), FRotator(0.f, Side * 8.f, 0.f));
				}), WRStormLook::GrpWingGlow);
			}
		}
	}
	else if (K == TEXT("raiju"))
	{
		// Low, arched, the fur standing out in a spiky halo; the tail split into two at the tip (forked lightning); white
		// eyes; white-blue Lichtenberg ferns glowing through the skin (3 in a storm).
		const FSurf Char = FSurf::Cloth(ScHex(TEXT("#26272B")));
		const FSurf Fern = FSurf::Lit(ScHex(TEXT("#CFE0FF")), 1.f);
		FVector TailTip;
		const FName TailBone = ChainEnd(TEXT("tail"), TailTip);
		if (!TailBone.IsNone())
		{
			AddGroup(TailBone, TailTip, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					const FVector D = FVector(-1.f, Side * 0.45f, 0.15f).GetSafeNormal();
					Kit.Spike(Char, FVector::ZeroVector, D, H * 0.12f, H * 0.65f);
				}
			}), WRStormLook::GrpStatic);
			AddGroup(TailBone, TailTip, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					const FVector D = FVector(-1.f, Side * 0.45f, 0.15f).GetSafeNormal();
					Kit.Spike(Fern, D * H * 0.5f, D, H * 0.04f, H * 0.2f);
				}
			}), WRStormLook::GrpGlow);
		}
		for (int32 b = 0; b < 4; ++b)
		{
			FVector At;
			const FName Bone = BoneAt(Spine[b], At);
			if (Bone.IsNone()) continue;
			// The halo: fur standing out round the neck and shoulders.
			if (b <= 1)
			{
				AddGroup(Bone, At, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 i = 0; i < 14; ++i)
					{
						const float A = PI * (0.05f + 0.9f * i / 13.f);
						const FVector Out(0.f, FMath::Cos(A), FMath::Sin(A));
						Kit.Spike(Char, Out * H * 0.22f, (Out + FVector(-0.3f, 0.f, 0.f)).GetSafeNormal(), H * 0.05f, H * (0.18f + 0.1f * Kit.Rand(Out, 13 + b)));
					}
				}), WRStormLook::GrpStatic);
			}
			// Ferns on both flanks.
			if (b >= 1)
			{
				AddGroup(Bone, At, Yaw, Blocks([&](FKit& Kit)
				{
					for (const float Side : { -1.f, 1.f })
					{
						const FVector Base(-H * 0.05f, Side * Rad * 0.42f, -H * 0.18f);
						WRStormLook::Fern(Kit, Fern, Base, FVector(-0.35f, 0.f, 1.f).GetSafeNormal(), H * 0.22f, 2.2f, 3);
					}
				}), WRStormLook::GrpGlow);
			}
		}
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f }) Kit.Disc(FSurf::Lit(FLinearColor(0.92f, 0.95f, 1.f), 2.5f), FVector(H * 0.16f, Side * H * 0.07f, H * 0.06f), FVector(0.4f, Side, 0.f), H * 0.05f, 1.2f);
			}), WRStormLook::GrpStatic);
		}
	}
	else if (K == TEXT("rodcrest"))
	{
		// The sail crest of copper-bright scales along the back, raised in storms and when alarmed, green with age.
		const float Age = float(C->TameRep.WildLevel % 7) / 6.f;   // (its level: the same on every machine) older ones green
		const FLinearColor Cu = FMath::Lerp(ScHex(TEXT("#C8823F")), ScHex(TEXT("#4E9A84")), FMath::Square(Age));
		FSurf Plate = FSurf::Steel(Cu, 0.35f + 0.3f * Age);
		Plate.Metal = FMath::Lerp(0.9f, 0.3f, Age);
		for (int32 b = 0; b < 5; ++b)
		{
			FVector At;
			const FName Bone = BoneAt(Spine[b], At);
			if (Bone.IsNone() || b == 0) continue;
			const float Tall = H * (b == 2 ? 1.0f : b == 1 || b == 3 ? 0.8f : 0.45f);
			AddGroup(Bone, At + Up * BackLift, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 3; ++i)
				{
					const float X = -H * 0.1f * i;
					Kit.Add(Plate, FVector(H * 0.12f, 1.6f, Tall * (0.85f + 0.15f * i)), FVector(X, 0.f, Tall * 0.45f), FRotator(-8.f, 0.f, 0.f));
					Kit.Rod(Plate.Dim(0.7f), FVector(X, 0.f, 0.f), FVector(X - H * 0.05f, 0.f, Tall * 0.92f), 1.8f);   // the spine of the sail
				}
			}), WRStormLook::GrpCrest);
		}
	}
	else if (K == TEXT("coilburrow") || K == TEXT("coilback"))
	{
		// The star nose: 22 pink, fleshy feelers in a ring round the snout (each as long as an arm on Coilback), sparking
		// blue when they sense metal. Coilback's back: overlapping plates crusted with copper ore, copper and verdigris.
		const bool bBack = K == TEXT("coilback");
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			const FVector Tip = FrontAt(Head.Z);
			const float Ring = H * (bBack ? 0.16f : 0.12f), Len = bBack ? 60.f : H * 0.32f;
			const FSurf Flesh = FSurf::Glaze(ScHex(TEXT("#D88A8A")), 0.55f);
			AddGroup(HeadBone, Tip - Fwd * Len * 0.15f, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 22; ++i)
				{
					const float A = 2.f * PI * (i + 0.5f) / 22.f;
					const FVector Out(0.f, FMath::Cos(A), FMath::Sin(A));
					Kit.Spike(Flesh, Out * Ring * 0.4f, (FVector(0.7f, 0.f, 0.f) + Out).GetSafeNormal(), Len * 0.22f, Len * (0.85f + 0.3f * Kit.Rand(Out, 21)));
				}
				Kit.Disc(Flesh.Dim(0.75f), FVector::ZeroVector, FVector(1.f, 0.f, 0.f), Ring * 0.9f, Len * 0.2f);
			}), WRStormLook::GrpFeelers);
		}
		if (bBack)
		{
			const FSurf Copper = FSurf::Steel(ScHex(TEXT("#B87333")), 0.42f);
			FSurf Verd = FSurf::Steel(ScHex(TEXT("#4E9A84")), 0.6f);
			Verd.Metal = 0.4f;
			const FSurf Ore = FSurf::Stone(ScHex(TEXT("#7A4A2A")), 0.8f);
			for (int32 b = 0; b < 5; ++b)
			{
				FVector At;
				const FName Bone = BoneAt(Spine[b], At);
				if (Bone.IsNone()) continue;
				AddGroup(Bone, At + Up * BackLift, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 i = 0; i < 2; ++i)
					{
						for (const float Side : { -1.f, 1.f })
						{
							const FVector P(-Rad * 0.2f * i, Side * Rad * 0.22f, -Rad * 0.06f);
							Kit.Add((b + i) % 2 ? Verd : Copper, FVector(Rad * 0.5f, Rad * 0.5f, 7.f), P, FRotator(-10.f, 0.f, Side * 24.f));
							for (int32 o = 0; o < 3; ++o)
							{
								const FVector Q = P + FVector(Kit.Jit(P, 40 + o) * Rad * 0.15f, Side * Kit.Rand(P, 50 + o) * Rad * 0.12f, 8.f);
								Kit.Add(Ore, FVector(14.f + 10.f * Kit.Rand(Q, 2)), Q, FRotator(Kit.Jit(Q, 3) * 30.f, Kit.Jit(Q, 4) * 40.f, 0.f));
							}
						}
					}
				}), WRStormLook::GrpStatic);
			}
		}
	}
	else if (K == TEXT("cragstrider"))
	{
		// Short black dagger horns, a white beard, and the middle legs: short, set high, used for climbing (they step
		// with the gait, half a stride off the others).
		const FSurf Horn = FSurf::Glaze(ScHex(TEXT("#16151A")), 0.35f);
		const FSurf Wool = FSurf::Cloth(ScHex(TEXT("#ECEAE3")));
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f }) Kit.Spike(Horn, FVector(0.f, Side * H * 0.05f, H * 0.08f), FVector(-0.7f, Side * 0.12f, 0.75f).GetSafeNormal(), H * 0.05f, H * 0.24f);
				for (int32 i = 0; i < 6; ++i) Kit.Spike(Wool, FVector(H * 0.14f, (i - 2.5f) * H * 0.015f, -H * 0.06f), FVector(0.15f, 0.f, -1.f), H * 0.03f, H * (0.14f + 0.05f * Kit.Rand(FVector(float(i)), 7)));
			}), WRStormLook::GrpStatic);
		}
		FVector Mid;
		const FName MidBone = BoneAt(TEXT("spine2"), Mid);
		if (!MidBone.IsNone())
		{
			const FSurf Hoof = FSurf::Glaze(ScHex(TEXT("#121113")), 0.4f);
			for (const float Side : { -1.f, 1.f })
			{
				AddGroup(MidBone, Mid + Right * Side * Rad * 0.55f, Yaw, Blocks([&](FKit& Kit)
				{
					const FVector Knee(H * 0.08f, Side * 6.f, -H * 0.38f), Ankle(-H * 0.02f, Side * 8.f, -H * 0.66f);
					Kit.Rod(Wool, FVector::ZeroVector, Knee, H * 0.09f);
					Kit.Rod(Wool.Dim(0.9f), Knee, Ankle, H * 0.06f);
					Kit.Add(Hoof, FVector(H * 0.09f, H * 0.07f, H * 0.06f), Ankle - FVector(0.f, 0.f, H * 0.03f));
				}), Side < 0.f ? WRStormLook::GrpLegL : WRStormLook::GrpLegR);
			}
		}
	}
	else if (K == TEXT("galeback"))
	{
		// A skirt of long hair to the ground, wide flat horns swept sideways, a copper ring of verdigris round the muzzle.
		const FSurf Hair = FSurf::Cloth(ScHex(TEXT("#2B2119")));
		const FSurf Frost = FSurf::Cloth(ScHex(TEXT("#8E8A86")));
		for (int32 b = 0; b < 5; ++b)
		{
			FVector At;
			const FName Bone = BoneAt(Spine[b], At);
			if (Bone.IsNone() || b == 0) continue;
			bool bOk = false;
			const float Drop = FMath::Max(H * 0.5f, float(At.Z - ScGround(C->GetWorld(), At, bOk, C).Z) - 6.f);
			AddGroup(Bone, At, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					for (int32 i = 0; i < 4; ++i)
					{
						const FVector Top(-Rad * 0.12f * i, Side * Rad * 0.48f, H * 0.12f);
						const float L = Drop * (0.92f + 0.12f * Kit.Rand(Top, 3));
						Kit.Add(Hair, FVector(Rad * 0.16f, 6.f, L), Top - FVector(0.f, 0.f, L * 0.5f), FRotator(0.f, 0.f, Side * 6.f));
						Kit.Add(Frost, FVector(Rad * 0.12f, 7.f, L * 0.18f), Top - FVector(0.f, 0.f, L * 0.1f) + FVector(0.f, Side * 1.f, 0.f), FRotator(0.f, 0.f, Side * 6.f));
					}
				}
			}), WRStormLook::GrpStatic);
		}
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			const FVector Tip = FrontAt(Head.Z - H * 0.05f);
			AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f })
				{
					const FVector D = FVector(-0.2f, Side, -0.25f).GetSafeNormal();
					Kit.Add(FSurf::Glaze(ScHex(TEXT("#5C5246")), 0.6f), FVector(H * 0.7f, H * 0.2f, 7.f), D * H * 0.4f + FVector(0.f, 0.f, H * 0.08f), D.Rotation() + FRotator(0.f, 0.f, Side * 15.f));
				}
			}), WRStormLook::GrpStatic);
			AddGroup(HeadBone, Tip, Yaw, Blocks([&](FKit& Kit)
			{
				Kit.Disc(FSurf::Steel(ScHex(TEXT("#4E9A84")), 0.5f), FVector(-H * 0.06f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), H * 0.3f, H * 0.05f);
			}), WRStormLook::GrpStatic);
		}
	}
	else if (K == TEXT("puffram"))
	{
		// A huge round cloud of white wool (it stands out with static in storms), little curled horns.
		FVector Mid;
		const FName MidBone = BoneAt(TEXT("spine2"), Mid);
		if (!MidBone.IsNone())
		{
			const FSurf Wool = FSurf::Cloth(ScHex(TEXT("#F1EFE8")));
			AddGroup(MidBone, Mid + Up * H * 0.1f, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 16; ++i)
				{
					const FVector Dir = FVector(Kit.Jit(FVector(float(i)), 1), Kit.Jit(FVector(float(i)), 2), Kit.Jit(FVector(float(i)), 3) * 0.7f + 0.2f).GetSafeNormal();
					Kit.Disc(Wool.Dim(0.92f + 0.08f * Kit.Rand(FVector(float(i)), 4)), Dir * Rad * 0.38f, Dir, Rad * 0.95f, Rad * 0.42f);
				}
			}), WRStormLook::GrpWool);
		}
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
			{
				for (const float Side : { -1.f, 1.f }) WRStormLook::Horn(Kit, FVector(0.f, Side * H * 0.12f, H * 0.12f), Side, H * 0.09f, 1.f, H * 0.06f, H * 0.025f, H * 0.01f, FSurf::Glaze(ScHex(TEXT("#3A3128")), 0.5f), FSurf::Glaze(ScHex(TEXT("#2A231C")), 0.5f), 0.1f, 0.f, 14);
			}), WRStormLook::GrpStatic);
		}
	}
	else if (K == TEXT("kharvox"))
	{
		// The lammergeier's face: a beard of black bristles under the beak, a red ring round each pale eye; copper-bright
		// tips on the primaries that light violet-white from the wingtip inward as his Charge builds.
		FVector Head;
		const FName HeadBone = BoneAt(TEXT("head"), Head);
		if (!HeadBone.IsNone())
		{
			const FSurf Bristle = FSurf::Cloth(ScHex(TEXT("#0E0E10")));
			AddGroup(HeadBone, Head, Yaw, Blocks([&](FKit& Kit)
			{
				for (int32 i = 0; i < 12; ++i)
				{
					const float S = (i - 5.5f) / 5.5f;
					Kit.Spike(Bristle, FVector(H * 0.16f, S * H * 0.04f, -H * 0.06f), FVector(0.3f, S * 0.15f, -1.f).GetSafeNormal(), H * 0.025f, H * (0.18f + 0.06f * Kit.Rand(FVector(float(i)), 8)));
				}
				for (const float Side : { -1.f, 1.f })
				{
					Kit.Disc(FSurf::Lit(ScHex(TEXT("#B0281E")), 0.6f), FVector(H * 0.1f, Side * H * 0.075f, H * 0.04f), FVector(0.f, Side, 0.f), H * 0.065f, 1.f);
					Kit.Disc(FSurf::Lit(ScHex(TEXT("#F2EAD8")), 0.4f), FVector(H * 0.1f, Side * H * 0.078f, H * 0.04f), FVector(0.f, Side, 0.f), H * 0.035f, 1.2f);
				}
			}), WRStormLook::GrpStatic);
		}
		// The wing bones, outermost first per side: four bands from the tip in.
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		TArray<TPair<float, FName>> Sides[2];
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (!(N.Contains(TEXT("wing")) || N.Contains(TEXT("finger"))) || N.Contains(TEXT("end")) || N.Contains(TEXT("nub"))) continue;
			const FVector P = Body->GetBoneLocation(Ref.GetBoneName(i));
			const float Out = FVector::DotProduct(P - C->GetActorLocation(), Right);
			Sides[Out >= 0.f ? 1 : 0].Add({ FMath::Abs(Out), Ref.GetBoneName(i) });
		}
		const FSurf Copper = FSurf::Steel(ScHex(TEXT("#B87333")), 0.35f);
		const FSurf Slate = FSurf::Cloth(ScHex(TEXT("#2E333A")));
		const FSurf Lit = FSurf::Lit(ScHex(TEXT("#B9A7FF")), 0.f);
		for (int32 s = 0; s < 2; ++s)
		{
			Sides[s].Sort([](const TPair<float, FName>& A, const TPair<float, FName>& B) { return A.Key > B.Key; });
			const float Side = s == 1 ? 1.f : -1.f;
			for (int32 Band = 0; Band < FMath::Min(4, Sides[s].Num()); ++Band)
			{
				const FName Bone = Sides[s][Band].Value;
				const FVector P = Body->GetBoneLocation(Bone);
				const float Len = H * 0.55f;
				AddGroup(Bone, P, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 f = 0; f < 5; ++f)
					{
						const FVector D = FVector(-1.f, Side * (0.25f - 0.12f * f), 0.f).GetSafeNormal();
						Kit.Add(Slate, FVector(Len, H * 0.09f, 3.f), D * Len * 0.5f + FVector(0.f, Side * f * H * 0.05f, 0.f), D.Rotation());
						Kit.Add(Copper, FVector(Len * 0.22f, H * 0.095f, 3.5f), D * Len * 0.92f + FVector(0.f, Side * f * H * 0.05f, 0.f), D.Rotation());
					}
				}), WRStormLook::GrpStatic);
				AddGroup(Bone, P, Yaw, Blocks([&](FKit& Kit)
				{
					for (int32 f = 0; f < 5; ++f)
					{
						const FVector D = FVector(-1.f, Side * (0.25f - 0.12f * f), 0.f).GetSafeNormal();
						Kit.Add(Lit, FVector(Len * 0.2f, H * 0.03f, 4.f), D * Len * 0.95f + FVector(0.f, Side * f * H * 0.05f, 2.f), D.Rotation());
					}
				}), WRStormLook::GrpBand0 + Band);
			}
		}
	}
	UE_LOG(LogWildreach, Verbose, TEXT("STORM look %s: %d parts, %d tints"), *C->CreatureId.ToString(), Groups.Num(), TintMats.Num());
}

void UWRStormBody::TickLook(float DeltaTime)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body)
	{
		return;
	}
	if (LookFor != C->CreatureId)
	{
		// Built once its mesh is on (the server may send the id after this component).
		if (Body->GetSkeletalMeshAsset() && Body->IsVisible() && ++LookTries > 2) BuildLook();
		return;
	}
	const FName K = ScKind(C->CreatureId);
	const bool bAlive = C->IsAlive();
	FxClock += DeltaTime;

	// Glow on the parts that light.
	if (K == TEXT("voltfang"))
	{
		const float Roll = RollUntil > 0.0 ? 1.f : 0.f;
		SetGroupGlow(WRStormLook::GrpGlow, !bAlive ? 0.f : Has(WRStormFlag::Charged) ? 2.f : Roll * 1.f);
	}
	else if (K == TEXT("stormkite") || K == TEXT("galvara"))
	{
		SetGroupGlow(WRStormLook::GrpGlow, bAlive && Has(WRStormFlag::Charged) ? 3.f : 0.f);
	}
	else if (K == TEXT("raiju"))
	{
		SetGroupGlow(WRStormLook::GrpGlow, !bAlive ? 0.f : Has(WRStormFlag::Curled) ? 6.f : Has(WRStormFlag::Lit) ? 3.f : 1.f);
		if (Has(WRStormFlag::Curled) && FxClock > 0.12f) { FxClock = 0.f; WRFx::SpawnAt(C, TEXT("status.shock"), C->GetActorLocation(), FRotator::ZeroRotator, 0.7f); }
	}
	else if (K == TEXT("kharvox"))
	{
		const bool bDim = Has(WRStormFlag::Dimmed) || !bAlive;
		for (int32 Band = 0; Band < 4; ++Band) SetGroupGlow(WRStormLook::GrpBand0 + Band, !bDim && Charge >= 25 * (Band + 1) ? 4.f : 0.f);
		for (UMaterialInstanceDynamic* M : TintMats) if (M) M->SetScalarParameterValue(TEXT("GlowStrength"), bDim ? 0.f : Charge / 100.f * 4.f);
		if (Has(WRStormFlag::Eye))
		{
			if (!EyeFrame)
			{
				EyeFrame = NewObject<USceneComponent>(C);
				EyeFrame->SetUsingAbsoluteLocation(true);
				EyeFrame->SetUsingAbsoluteRotation(true);
				EyeFrame->RegisterComponent();
				EyeFrame->AttachToComponent(C->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				EyeFx = WRFx::Attach(EyeFrame, TEXT("storm.eye"), NAME_None, WRTune(TEXT("bosses.kharvox.eyeRadius"), 1000.f) / 100.f);
			}
			EyeFrame->SetWorldLocation(FMath::VInterpTo(EyeFrame->GetComponentLocation(), FVector(EyeAt), DeltaTime, 4.f));
		}
		else if (EyeFrame)
		{
			if (EyeFx) WRFx::Stop(EyeFx);
			EyeFx = nullptr;
			EyeFrame->DestroyComponent();
			EyeFrame = nullptr;
		}
	}
	// A loop effect on the body: Galvara's banner cloud, a Coilback underground (the soil ridge), Kharvox's charge whine.
	const FName Loop = K == TEXT("galvara") && Has(WRStormFlag::Cloud) && bAlive ? FName(TEXT("storm.cloud")) : NAME_None;
	if (Loop != LoopFxName)
	{
		if (LoopFx) WRFx::Stop(LoopFx);
		LoopFx = Loop.IsNone() ? nullptr : WRFx::Attach(C->GetRootComponent(), Loop, NAME_None, C->GetSimpleCollisionRadius() / 60.f);
		if (LoopFx) LoopFx->SetRelativeLocation(FVector(0.f, 0.f, -WRTune(TEXT("bosses.galvara.coreDrop"), 800.f)));
		LoopFxName = Loop;
	}
	if (Has(WRStormFlag::Underground) && FxClock > 0.15f)
	{
		FxClock = 0.f;
		bool bOk = false;
		const FVector G = ScGround(C->GetWorld(), C->GetActorLocation(), bOk, C);
		WRFx::SpawnAt(C, TEXT("storm.soil"), G, C->GetActorRotation(), C->GetSimpleCollisionRadius() / 120.f);
		if (Has(WRStormFlag::Rimed)) WRFx::SpawnFor(C, TEXT("storm.rime"), G, C->GetActorRotation(), 1.2f, WRTune(TEXT("bosses.coilback.frostTunnelSeconds"), 12.f));
	}
	if (Has(WRStormFlag::Sensing) && bAlive && FxClock > 0.3f)
	{
		FxClock = 0.f;
		const FName Head = C->FindBone(TEXT("head"));
		const FVector At = Head.IsNone() ? C->GetActorLocation() : Body->GetBoneLocation(Head) + C->GetActorForwardVector() * C->GetSimpleCollisionRadius() * 0.4f;
		WRFx::SpawnAt(C, TEXT("status.shock"), At, FRotator::ZeroRotator, K == TEXT("coilback") ? 0.8f : 0.3f);
	}
	if (K == TEXT("puffram") && Has(WRStormFlag::Charged) && bAlive && FxClock > 0.9f)
	{
		FxClock = 0.f;
		WRFx::SpawnAt(C, TEXT("status.shock"), C->GetActorLocation() + FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() * 0.6f), FRotator::ZeroRotator, 0.35f);
	}

	// Moving parts.
	const FVector Vel = C->GetVelocity();
	const FVector Accel = DeltaTime > 0.f ? (Vel - LastVelocity) / DeltaTime : FVector::ZeroVector;
	LastVelocity = Vel;
	const FVector LocalAccel = C->GetActorRotation().UnrotateVector(Accel);
	const float Speed = Vel.Size2D();
	StepClock += Speed * DeltaTime;
	CrestAlpha = FMath::FInterpTo(CrestAlpha, Has(WRStormFlag::CrestUp) ? 1.f : 0.f, DeltaTime, 3.f);
	if (K == TEXT("bellwether"))
	{
		// The bell on its shackle: a damped spring pushed by how he moves (it swings back as he charges), and a clank
		// with every step that carries 50 m.
		const float Kspring = 28.f, Damp = 2.6f, Push = 0.018f;
		for (int32 A = 0; A < 2; ++A)
		{
			const float Force = (A == 0 ? LocalAccel.X : -LocalAccel.Y) * Push;
			BellSpeed[A] += (-Kspring * BellAngle[A] - Damp * BellSpeed[A] - Force) * DeltaTime;
			BellAngle[A] = FMath::Clamp(BellAngle[A] + BellSpeed[A] * DeltaTime, -35.f, 35.f);
		}
		if (StepClock > 140.f && bAlive)
		{
			StepClock = 0.f;
			BellSpeed.Y += FMath::FRandRange(-6.f, 6.f);
			WRSound::PlayOn(C, TEXT("storm.bell_clank"), FMath::Clamp(Speed / 300.f, 0.4f, 1.f));
		}
	}
	for (FGroup& G : Groups)
	{
		USceneComponent* F = G.Frame.Get();
		if (!F) continue;
		switch (G.Kind)
		{
		case WRStormLook::GrpBell:
			F->SetRelativeRotation(G.BaseRot.Quaternion() * FRotator(BellAngle.X, 0.f, BellAngle.Y).Quaternion());
			break;
		case WRStormLook::GrpCrest:
			F->SetRelativeScale3D(G.BaseScale * FVector(1.f, 1.f, FMath::Lerp(0.18f, 1.f, CrestAlpha)));
			break;
		case WRStormLook::GrpFeelers:
			F->SetRelativeRotation(G.BaseRot.Quaternion() * FRotator(0.f, 0.f, FMath::Sin(Clock * (Has(WRStormFlag::Sensing) ? 14.f : 5.f)) * 6.f).Quaternion());
			break;
		case WRStormLook::GrpLegL:
		case WRStormLook::GrpLegR:
		{
			const float Swing = FMath::Clamp(Speed / 400.f, 0.f, 1.f) * 24.f * FMath::Sin(StepClock / 55.f + (G.Kind == WRStormLook::GrpLegR ? PI : 0.f));
			F->SetRelativeRotation(G.BaseRot.Quaternion() * FRotator(Swing, 0.f, 0.f).Quaternion());
			break;
		}
		case WRStormLook::GrpWool:
			F->SetRelativeScale3D(G.BaseScale * (Has(WRStormFlag::Charged) ? 1.12f + 0.02f * FMath::Sin(Clock * 11.f) : 1.f));
			break;
		default:
			break;
		}
	}
}

// ---------------------------------------------------------------- the AI hooks

bool FWRStormAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRStormBody* Body = UWRStormBody::Get(Self);
	if (!Body)
	{
		return false;
	}
	const double T = ScNow(Self);
	const FName K = ScKind(Self->CreatureId);
	if (K == TEXT("galvara_core"))
	{
		return true;   // its queen moves it (FWRStormBoss)
	}
	// Arriving in its strike, or leaping back into one: nothing else.
	if (Body->Has(WRStormFlag::Arriving) || Body->bReturning || Body->SnatchedBy.IsValid())
	{
		return true;
	}
	// A beat of the living world under way (a Rams' Ring clash, a kite going for a Rodcrest).
	if (T < Body->BeatUntil && FWRStormBoss::TickBeat(AI, Self, Body, DeltaSeconds))
	{
		return true;
	}
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());

	if (K == TEXT("voltfang"))
	{
		// Charge-up: a pack member that hasn't attacked for 6 s rolls in grit and shakes; its fur glows, the next shake
		// is ready. In a storm they're always charged.
		if (T < Body->RollUntil)
		{
			if (Target) AI->SetControlRotation((Target->GetActorLocation() - Self->GetActorLocation()).Rotation());
			return true;
		}
		if (Target && Target->IsAlive() && !Body->Has(WRStormFlag::Charged) && !ScInStorm(Self) && Self->HuntTarget.Get() != Target
			&& T - Body->LastAttackAt > WRTune(TEXT("creatureAI.voltfang.chargeAfter"), 6.f) && Self->SinceHit() > 2.f
			&& FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) > WRTune(TEXT("creatureAI.voltfang.chargeMinDistance"), 500.f))
		{
			const float Roll = WRTune(TEXT("creatureAI.voltfang.chargeSeconds"), 1.5f);
			Body->RollUntil = T + Roll;
			Self->PlaySwing(EWRSwingKind::Stomp, Roll);
			Self->MulticastTell(1, TEXT("spine"), TEXT("voice"));
			WRFx::Broadcast(Self, TEXT("storm.grit"), Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("storm.charge_whine"), Roll);
			UE_LOG(LogWildreach, Log, TEXT("AI %s rolls in grit (charging up)"), *Self->GetName());
			return true;
		}
	}
	if (K == TEXT("galeback"))
	{
		// The herd ring (musk oxen, yaks): horns out, young inside, facing whatever threatens it; strikes only at what comes close.
		AActor* Threat = Target;
		TArray<AWRCreature*> Herd;
		FVector Centre = FVector::ZeroVector;
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (ScWild(*It) && It->Spawner == Self->Spawner && Self->Spawner.IsValid() && ScKind(It->CreatureId) == K)
			{
				Herd.Add(*It);
				Centre += It->GetActorLocation();
				if (!Threat && It->GetAggroTarget()) Threat = It->GetAggroTarget();
			}
		}
		if (Herd.Num() == 0)
		{
			Herd.Add(Self);
			Centre = Self->GetActorLocation();
		}
		Centre /= float(Herd.Num());
		if (!Threat)
		{
			for (TActorIterator<AWRCreature> It(Self->GetWorld()); It && !Threat; ++It)
			{
				if (ScWild(*It) && ScKind(It->CreatureId) == TEXT("voltfang") && FVector::Dist2D(It->GetActorLocation(), Centre) < WRTune(TEXT("creatureAI.galeback.ringRadius"), 2500.f)) Threat = *It;
			}
		}
		if (Threat && Herd.Num() >= 3 && FVector::Dist2D(Threat->GetActorLocation(), Centre) < WRTune(TEXT("creatureAI.galeback.ringHold"), 3000.f))
		{
			Herd.Sort([](const AWRCreature& A, const AWRCreature& B) { return A.GetName() < B.GetName(); });
			TArray<AWRCreature*> Adults = Herd.FilterByPredicate([](const AWRCreature* H) { return !H->IsBaby(); });
			const int32 Index = Adults.IndexOfByKey(Self);
			FVector Spot = Centre;
			FVector Face = (Threat->GetActorLocation() - Centre).GetSafeNormal2D();
			if (Index != INDEX_NONE)
			{
				const float Ring = Self->GetSimpleCollisionRadius() * 1.4f + 60.f * Adults.Num();
				const float A = 2.f * PI * Index / FMath::Max(1, Adults.Num()) + FMath::Atan2(Face.Y, Face.X);
				const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
				Spot = Centre + Out * Ring;
				Face = Out;
			}
			const FVector To = Spot - Self->GetActorLocation();
			AWRCharacterBase* Near = Cast<AWRCharacterBase>(Threat);
			if (Near && Near->IsAlive() && FVector::Dist2D(Near->GetActorLocation(), Self->GetActorLocation()) < Self->GetSimpleCollisionRadius() + WRTune(TEXT("creatureAI.galeback.strikeReach"), 400.f))
			{
				if (!Self->GetAggroTarget()) Self->SetAggroTarget(Near);
				AI->TickRowFight(Self, Near, DeltaSeconds);
				return true;
			}
			if (To.Size2D() > 120.f) Self->AddMovementInput(To.GetSafeNormal2D(), 0.7f);
			else Self->SetActorRotation(FMath::RInterpConstantTo(Self->GetActorRotation(), Face.Rotation(), DeltaSeconds, 90.f));
			if (T - Body->AlarmAt > 6.0)
			{
				Body->AlarmAt = T;
				WRSound::Voice(Self, TEXT("aggro"));   // the low grunt chorus as the herd rings up
			}
			return true;
		}
	}
	if ((K == TEXT("rodcrest") || K == TEXT("puffram")) && Self->IsFleeing())
	{
		// Flee, unless cornered: then the crest flash (or a butt) at whatever cornered it.
		AWRCharacterBase* From = Cast<AWRCharacterBase>(Self->GetFleeFrom());
		if (From && From->IsAlive())
		{
			const FVector Away = (Self->GetActorLocation() - From->GetActorLocation()).GetSafeNormal2D();
			FHitResult Wall;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRCornered), false, Self);
			const bool bWall = Self->GetWorld()->SweepSingleByChannel(Wall, Self->GetActorLocation(), Self->GetActorLocation() + Away * 220.f, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(Self->GetSimpleCollisionRadius() * 0.8f), Q);
			const bool bStuck = Self->GetVelocity().Size2D() < 60.f;
			if (bWall || bStuck) { if (Body->CorneredSince < 0.0) Body->CorneredSince = T; }
			else Body->CorneredSince = -1.0;
			if (Body->CorneredSince > 0.0 && T - Body->CorneredSince > 0.6 && FVector::Dist2D(From->GetActorLocation(), Self->GetActorLocation()) < WRTune(TEXT("creatureAI.corneredRadius"), 450.f))
			{
				AI->TickRowFight(Self, From, DeltaSeconds);
				return true;
			}
		}
	}
	return false;
}

bool FWRStormAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRStormBody* Body = UWRStormBody::Get(Self);
	const TSharedPtr<FJsonObject> R = ScRow(Self->CreatureId);
	if (!Body || ScNum(R, TEXT("scoutMark"), 0.f) <= 0.f || Self->TameOrder != EWRTameOrder::Follow || Self->GetRider())
	{
		return false;
	}
	// A Stormkite sent ahead: it flies 40 m out along its owner's way, circles there and marks the creatures within 50 m
	// for 20 s on its owner's machine, then comes back.
	const double T = ScNow(Self);
	AWRCharacter* Owner = ScOwnerBody(Self->GetWorld(), Self->TamedBy);
	if (!Owner)
	{
		return false;
	}
	if (T >= Body->NextScoutAt && Body->ScoutUntil <= T)
	{
		Body->NextScoutAt = T + WRTune(TEXT("tame.storm.scoutEvery"), 30.f);
		bool bOk = false;
		Body->ScoutAt = ScGround(Self->GetWorld(), Owner->GetActorLocation() + FRotator(0.f, Owner->GetControlRotation().Yaw, 0.f).Vector() * WRTune(TEXT("tame.storm.scoutAhead"), 4000.f), bOk, Self);
		Body->ScoutUntil = T + WRTune(TEXT("tame.storm.scoutCircleSeconds"), 10.f) + 4.0;
		Body->BeatStep = 0;
	}
	if (T < Body->ScoutUntil)
	{
		const float Ring = WRTune(TEXT("tame.storm.scoutCircle"), 800.f);
		const FVector Centre = Body->ScoutAt + FVector(0.f, 0.f, WRTune(TEXT("tame.storm.scoutHeight"), 1500.f));
		const float A = float(T) * 0.8f;
		const FVector Goal = Centre + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Ring;
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const FVector To = Goal - Self->GetActorLocation();
		Self->AddMovementInput(To.GetSafeNormal(), 1.f);
		if (Body->BeatStep == 0 && FVector::Dist2D(Self->GetActorLocation(), Body->ScoutAt) < Ring * 1.6f)
		{
			Body->BeatStep = 1;
			int32 Marked = 0;
			if (APlayerController* PC = ScOwnerPC(Self->GetWorld(), Self->TamedBy))
			{
				const float Range = ScNum(R, TEXT("scoutMark"), 5000.f);
				for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
				{
					if (*It == Self || !It->IsAlive() || It->IsTamed() || It->IsHidden() || FVector::Dist2D(It->GetActorLocation(), Body->ScoutAt) > Range) continue;
					AWRStormMark::Spawn(PC, It->GetActorLocation(), *It, TEXT("creature"), WRTune(TEXT("tame.storm.scoutMarkSeconds"), 20.f));
					++Marked;
				}
				if (AWRPlayerController* WPC = Cast<AWRPlayerController>(PC)) WPC->ClientNotify(FString::Printf(TEXT("Your %s marks %d creature%s ahead"), *Self->GetDef()->Name, Marked, Marked == 1 ? TEXT("") : TEXT("s")));
			}
			WRSound::Voice(Self, TEXT("aggro"));
		}
		return true;
	}
	if (Self->GetCharacterMovement()->MovementMode == MOVE_Flying && FVector::Dist2D(Owner->GetActorLocation(), Self->GetActorLocation()) < 600.f)
	{
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);   // home on its owner: down again
	}
	else if (Self->GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		Self->AddMovementInput((Owner->GetActorLocation() + FVector(0.f, 0.f, 300.f) - Self->GetActorLocation()).GetSafeNormal(), 1.f);
		return true;
	}
	(void)AI;
	return false;
}

bool FWRStormAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRStormBody* Body = UWRStormBody::Get(Self);
	if (!Body || !Target)
	{
		return true;
	}
	const FName Id = Row.Id;
	const float Dist = FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation());
	if (bReading)
	{
		// Reads only in reach: the Thunderhorn a drink or a cast within 10 m, the Raiju a drink within 8 m.
		const FName K = ScKind(Self->CreatureId);
		if (K == TEXT("raiju") && !Target->HasTag(WRTags::State_Drinking)) return false;
		if (Dist > WRTune(K == TEXT("raiju") ? TEXT("creatureAI.raiju.readRadius") : TEXT("creatureAI.thunderhorn.readRadius"), K == TEXT("raiju") ? 800.f : 1000.f)) return false;
	}
	if (Id == TEXT("static_shake")) return Body->Has(WRStormFlag::Charged);
	if (Id == TEXT("grounded_snap")) return Body->Has(WRStormFlag::Grounded);
	if (Id == TEXT("dive_strike") || Id == TEXT("spark_darts")) return !Body->Has(WRStormFlag::Grounded);
	if (Id == TEXT("flank_lunge"))
	{
		// From the flank only (90 or 150 deg off the target's facing).
		const FVector Facing = FRotator(0.f, Target->GetController() ? Target->GetControlRotation().Yaw : Target->GetActorRotation().Yaw, 0.f).Vector();
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, (Self->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D()), -1.f, 1.f)));
		return Off >= WRTune(TEXT("creatureAI.voltfang.flankMinAngle"), 60.f);
	}
	if (Id == TEXT("rebound_leap"))
	{
		// Only with a rock face, a rod or a tree within 6 m to spring off.
		const float Reach = WRTune(TEXT("creatureAI.raiju.reboundSurface"), 600.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector D = FRotator(0.f, i * 45.f, 0.f).Vector();
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRebound), false, Self);
			if (Self->GetWorld()->LineTraceSingleByChannel(Hit, Self->GetActorLocation(), Self->GetActorLocation() + D * Reach, ECC_WorldStatic, Q) && FMath::Abs(Hit.ImpactNormal.Z) < 0.5f)
			{
				return true;
			}
		}
		return false;
	}
	(void)AI;
	return true;
}

void FWRStormAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRStormBody* Body = UWRStormBody::Get(Self);
	if (!Body)
	{
		return;
	}
	Body->LastAttackAt = ScNow(Self);
	if (Row.Id == TEXT("dive_strike"))
	{
		Body->Set(WRStormFlag::Charged, true);   // the tail fork's inner feathers glow before the fold
	}
	else if (Row.Id == TEXT("discharge_curl"))
	{
		Body->Set(WRStormFlag::Curled, true);
		WRSound::PlayOn(Self, TEXT("storm.charge_whine"));
	}
	else if (Row.Id == TEXT("rebound_leap"))
	{
		// Springs to the nearest face first (the wind-up), to come at its target from above.
		const float Reach = WRTune(TEXT("creatureAI.raiju.reboundSurface"), 600.f);
		FVector Best = FVector::ZeroVector;
		float BestD = Reach;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector D = FRotator(0.f, i * 45.f, 0.f).Vector();
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRebound), false, Self);
			if (Self->GetWorld()->LineTraceSingleByChannel(Hit, Self->GetActorLocation(), Self->GetActorLocation() + D * Reach, ECC_WorldStatic, Q) && FMath::Abs(Hit.ImpactNormal.Z) < 0.5f && Hit.Distance < BestD)
			{
				BestD = Hit.Distance;
				Best = Hit.ImpactPoint + Hit.ImpactNormal * (Self->GetSimpleCollisionRadius() + 10.f);
			}
		}
		if (!Best.IsZero())
		{
			const FVector To = Best + FVector(0.f, 0.f, 200.f) - Self->GetActorLocation();
			const float Flight = FMath::Max(0.3f, Row.Windup * 0.6f);
			Self->LaunchCharacter(FVector(To.X / Flight, To.Y / Flight, 600.f), true, true);
		}
	}
	(void)AI; (void)Target;
}

bool FWRStormAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRStormBody* Body = UWRStormBody::Get(Self);
	if (!Body)
	{
		return false;
	}
	const FName M = Row.Motion;
	if (M == TEXT("discharge"))
	{
		// A burst round itself (the Voltfang's static shake 3 m, the Raiju's discharge 4 m): bodies inside take it.
		const float Radius = FMath::Max(150.f, Row.Radius);
		WRFx::Broadcast(Self, TEXT("storm.discharge"), Self->GetActorLocation(), FRotator::ZeroRotator, Radius / 100.f, TEXT("storm.arc"));
		for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (*It != Self && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Radius + It->GetSimpleCollisionRadius())
			{
				AI->RowHit(Self, *It);
			}
		}
		Body->Set(WRStormFlag::Charged, Row.Id == TEXT("static_shake") ? false : Body->Has(WRStormFlag::Charged));
		Body->Set(WRStormFlag::Curled, false);
		return true;
	}
	if (M == TEXT("darts"))
	{
		// Three darts in a 15 deg fan at where the target stood when the wind-up began: never homing, never inside 4 m.
		const FVector From = Self->MouthLocation();
		const FVector Aim = AI->RowAim + FVector(0.f, 0.f, 60.f);
		if (FVector::Dist(From, Aim) < WRTune(TEXT("creatureAI.storm.minRanged"), 400.f))
		{
			return true;
		}
		const float Speed = FMath::Min(Row.Speed > 0.f ? Row.Speed : 1200.f, WRTune(TEXT("creatureAI.storm.maxProjectileSpeed"), 1400.f));
		const int32 N = FMath::Max(1, Row.Repeats > 1 ? Row.Repeats : 3);
		const float Fan = Row.Arc > 0.f ? Row.Arc : 15.f;
		for (int32 i = 0; i < N; ++i)
		{
			const float Turn = N == 1 ? 0.f : -Fan * 0.5f + Fan * i / (N - 1);
			const FVector Dir = (Aim - From).GetSafeNormal().RotateAngleAxis(Turn, FVector::UpVector);
			FActorSpawnParameters Params;
			Params.Owner = Self;
			Params.Instigator = Self;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AWRProjectile* Dart = Self->GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), From, Dir.Rotation(), Params))
			{
				FWRHit Hit;
				Hit.Instigator = Self;
				Hit.Damage = Self->Weapon.FlatDamage * Row.Dmg * FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f));
				Hit.PoiseDamage = Row.Poise;
				Hit.Impact = Row.Impact;
				Hit.bProjectile = true;
				Hit.Element = WRTags::Element_Lightning;
				for (const TPair<FName, float>& B : Row.Buildups) Hit.Buildups.Add(B.Key, B.Value);
				Dart->Init(Hit, Speed, ScHex(TEXT("#E4DEFF")));
			}
		}
		WRSound::PlayOn(Self, TEXT("storm.spark"));
		return true;
	}
	if (Row.Id == TEXT("dive_strike"))
	{
		// Wings folded: a straight line from the fold at where the target is now, no tracking; it lands after.
		const FVector Aim = Target ? Target->GetActorLocation() : AI->RowAim;
		const FVector Dir = (Aim - Self->GetActorLocation()).GetSafeNormal();
		AI->RowDir = Dir.GetSafeNormal2D();
		Self->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		Self->LaunchCharacter(Dir * (Row.Speed > 0.f ? Row.Speed : 1300.f), true, true);
		Body->Set(WRStormFlag::Charged, false);
		WRSound::Voice(Self, TEXT("attack"));
		return true;
	}
	if (Row.Id == TEXT("rebound_leap"))
	{
		// Off the face and down on the target from above.
		const FVector Aim = Target ? Target->GetActorLocation() : AI->RowAim;
		const FVector To = Aim - Self->GetActorLocation();
		const float Flight = 0.5f;
		AI->RowDir = To.GetSafeNormal2D();
		Self->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		Self->LaunchCharacter(FVector(To.X / Flight, To.Y / Flight, FMath::Max(150.f, To.Z / Flight + 245.f)), true, true);
		return true;
	}
	return false;
}

void FWRStormAI::ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row)
{
	if (!Self || !UWRStormBody::IsStormKind(Self->CreatureId))
	{
		return;
	}
	if (Hit.Buildups.Contains(TEXT("shock")))
	{
		Hit.Element = WRTags::Element_Lightning;
	}
	if (Self->IsTamed())
	{
		const FWRCreatureDef* Def = Self->GetDef();
		// A tame keeps its own element: its plain bites carry shock.
		if (Def && Def->Element == TEXT("lightning") && !Hit.Buildups.Contains(TEXT("shock")))
		{
			Hit.Buildups.Add(TEXT("shock"), WRTune(TEXT("tame.storm.shockBite"), 15.f));
			Hit.Element = WRTags::Element_Lightning;
		}
		// A tame Raiju in a storm: +20%.
		if (ScKind(Self->CreatureId) == TEXT("raiju") && ScInStorm(Self))
		{
			Hit.Damage *= 1.f + WRTune(TEXT("tame.storm.raijuStormBonus"), 0.2f);
		}
	}
	(void)Row;
}

void FWRStormAI::AfterRowHit(AWRCreature* Self, AWRCharacterBase* Victim, const FWRAttackRow& Row)
{
	if (Row.Id == TEXT("thunderclap_butt") && Victim)
	{
		// The clap and the white flash at the horns.
		WRFx::Broadcast(Self, TEXT("storm.clap"), Victim->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("storm.clap"));
	}
}

bool FWRStormAI::TracksCharge(const AWRCreature* Self)
{
	return Self && ScKind(Self->CreatureId) == TEXT("thunderhorn");
}

bool FWRStormAI::Grounded(const AWRCreature* Self)
{
	const UWRStormBody* Body = UWRStormBody::Get(Self);
	return Body && Body->Has(WRStormFlag::Grounded);
}

float FWRStormAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	if (Self && Self->CreatureId == TEXT("galvara_core") && (Hit.bProjectile || Hit.bSpell))
	{
		Damage *= WRTune(TEXT("bosses.galvara.coreRangedMult"), 1.5f);
	}
	return Damage;
}

void FWRStormAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRStormBody* Body = UWRStormBody::Get(Self);
	if (!Body || !Self->HasAuthority() || Self->IsTamed())
	{
		return;
	}
	const double T = ScNow(Self);
	const FName K = ScKind(Self->CreatureId);
	AActor* By = Hit.Instigator;
	if (const AWRCharacterBase* Who = Cast<AWRCharacterBase>(By); Who && Who->IsCreature() && !Who->GetTameOwner().IsEmpty())
	{
		By = const_cast<AWRCharacterBase*>(Who);
	}
	if (K == TEXT("voltfang") && By && T - Body->HowledAt > WRTune(TEXT("creatureAI.voltfang.howlCooldown"), 30.f))
	{
		// The crackle-howl: the first hit on any member pulls every pack within 60 m.
		const float Call = WRTune(TEXT("creatureAI.voltfang.howlRadius"), 6000.f);
		Self->MulticastTell(0, NAME_None, TEXT("voice"));
		WRFx::Broadcast(Self, TEXT("status.shock"), Self->GetActorLocation() + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.4f, TEXT("storm.arc"));
		int32 Pulled = 0;
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (!ScWild(*It) || ScKind(It->CreatureId) != K || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > Call) continue;
			if (UWRStormBody* Other = UWRStormBody::Get(*It)) Other->HowledAt = T;
			if (!It->GetAggroTarget() && *It != Self) { It->SetAggroTarget(By); ++Pulled; }
		}
		UE_LOG(LogWildreach, Log, TEXT("AI %s crackle-howls: %d come"), *Self->GetName(), Pulled);
	}
	else if (K == TEXT("stormkite") && Damage > 0.f && !Body->Has(WRStormFlag::Grounded) && Self->GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		// Three hits on the wings in the air bring it down for 6 s (SPEC "bring it down").
		const bool bWing = Hit.ImpactPoint.IsZero() || Self->IsNearLockPoint(Hit.ImpactPoint, TEXT("wing_left"), 160.f) || Self->IsNearLockPoint(Hit.ImpactPoint, TEXT("wing_right"), 160.f);
		if (bWing && ++Body->WingHits >= FMath::RoundToInt(WRTune(TEXT("creatureAI.stormkite.wingHits"), 3.f)))
		{
			Body->WingHits = 0;
			Body->GroundedUntil = T + WRTune(TEXT("creatureAI.stormkite.groundedSeconds"), 6.f);
			Body->Set(WRStormFlag::Grounded, true);
			Self->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Self->Status->Stagger(1.f);
			WRSound::Voice(Self, TEXT("hurt"));
			UE_LOG(LogWildreach, Log, TEXT("AI %s brought down"), *Self->GetName());
		}
	}
	else if (K == TEXT("cragstrider") && T - Body->AlarmAt > 1.0)
	{
		// The sneeze-whistle: everything within 40 m on alert (asleep wakes, prey bolts, hunters look for you).
		Body->AlarmAt = T;
		AActor* Threat = By;
		if (!Threat)
		{
			float Best = 4000.f;
			for (TActorIterator<AWRCharacter> It(Self->GetWorld()); It; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
				if (It->IsAlive() && D < Best) { Best = D; Threat = *It; }
			}
		}
		Self->MulticastTell(0, NAME_None, TEXT("voice"));
		const float Radius = WRTune(TEXT("creatureAI.cragstrider.alarmRadius"), 4000.f);
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			const FWRCreatureDef* Def = It->GetDef();
			if (!ScWild(*It) || !Def || It->IsA<AWRBoss>() || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > Radius) continue;
			It->SetAsleep(false);
			if (Def->Behavior == TEXT("aggressive") && Threat && !It->GetAggroTarget() && Cast<AWRCharacterBase>(Threat)) It->SetAggroTarget(Threat);
			else if (Def->Behavior != TEXT("aggressive") && Threat) It->StartFleeing(Threat, TEXT("the alarm whistle"));
		}
		UE_LOG(LogWildreach, Log, TEXT("AI %s whistles the alarm"), *Self->GetName());
	}
}

void FWRStormAI::Died(AWRCreature* Self, AActor* Killer)
{
	// The chance drops (cooks/biome-lightning.md 2.9 drops: "storm essence x1 (40%)"): creatures.json lootChance.
	const TSharedPtr<FJsonObject> R = ScRow(Self->CreatureId);
	const TSharedPtr<FJsonObject>* Chances = nullptr;
	if (!R.IsValid() || Self->IsTamed() || !R->TryGetObjectField(TEXT("lootChance"), Chances))
	{
		return;
	}
	AActor* Getter = Killer;
	if (const AWRCreature* Tame = Cast<AWRCreature>(Killer); Tame && Tame->IsTamed()) Getter = ScOwnerBody(Self->GetWorld(), Tame->TamedBy);
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Getter);
	if (!Inv) return;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*Chances)->Values)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		const bool bObj = L.Value->TryGetObject(O);
		const float Chance = bObj ? ScNum(*O, TEXT("chance"), 0.f) : float(L.Value->AsNumber());
		const int32 Count = bObj ? FMath::RoundToInt(ScNum(*O, TEXT("count"), 1.f)) : 1;
		if (FMath::FRand() < Chance) Inv->AddItem(FName(*L.Key), Count);
	}
}

void FWRStormAI::BeginArrival(AWRCreature* Raiju, const FVector& Near)
{
	UWRStormBody* Body = UWRStormBody::Get(Raiju);
	if (!Body || !Raiju->HasAuthority())
	{
		return;
	}
	// It rides a strike down: the strike lands 6 to 12 m from what it hunts (the normal 1 s glow) and it's crouched in
	// the scorch. Until then it's nowhere.
	AWRCharacterBase* Prey = ScNearestPrey(Raiju, WRTune(TEXT("creatureAI.raiju.arrivalSearch"), 6000.f));
	const FVector From = Prey ? Prey->GetActorLocation() : Near;
	FVector Spot = Near;
	bool bOk = false;
	for (int32 Try = 0; Try < 6; ++Try)
	{
		const FVector2D D = FMath::RandPointInCircle(1.f).GetSafeNormal() * FMath::FRandRange(WRTune(TEXT("creatureAI.raiju.arrivalMin"), 600.f), WRTune(TEXT("creatureAI.raiju.arrivalMax"), 1200.f));
		Spot = ScGround(Raiju->GetWorld(), From + FVector(D, 0.f), bOk, Raiju);
		if (bOk) break;
	}
	Raiju->SetActorHiddenInGame(true);
	Raiju->SetActorEnableCollision(false);
	Raiju->GetCharacterMovement()->DisableMovement();
	Body->Set(WRStormFlag::Arriving, true);
	Body->ArriveSpot = Spot;
	Body->ArriveCalledAt = ScNow(Raiju);
	const TMap<FName, FWRAttackRow>* Rows = FWRAttackRows::Get().Creature(Raiju->CreatureId);
	const FWRAttackRow* Row = Rows ? Rows->Find(TEXT("strike_arrival")) : nullptr;
	FWRStrikeCall Call;
	Call.Spot = Spot;
	Call.Tell = Row ? Row->Windup : 1.f;
	Call.Damage = Raiju->Weapon.FlatDamage * (Row ? Row->Dmg : 1.f);
	Call.Shock = Row ? Row->Buildups.FindRef(TEXT("shock")) : 40.f;
	Call.Source = EWRStrikeSource::Creature;
	Call.Instigator = Raiju;
	Call.bRodsTake = false;
	if (!WRStorm::CallStrike(Raiju, Call))
	{
		Body->ArriveCalledAt = ScNow(Raiju) - 3.0;   // refused: it comes out at once (TickServer's fallback)
	}
	UE_LOG(LogWildreach, Log, TEXT("STORM %s arrives in a strike at %s"), *Raiju->GetName(), *Spot.ToCompactString());
}

void FWRStormAI::StrikeLanded(UWorld* World, const FWRStrikeCall& Call)
{
	AWRCreature* By = Cast<AWRCreature>(Call.Instigator.Get());
	UWRStormBody* Body = UWRStormBody::Get(By);
	if (Body && Body->Has(WRStormFlag::Arriving))
	{
		// Crouched in the scorch, and it acts at once: the nearest living thing.
		Body->Set(WRStormFlag::Arriving, false);
		By->SetActorLocation(Call.Spot + FVector(0.f, 0.f, By->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		By->SetActorHiddenInGame(false);
		By->SetActorEnableCollision(true);
		By->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		By->SetHome(By->GetActorLocation());
		Body->CombatSince = ScNow(By);
		if (AWRCharacterBase* Prey = ScNearestPrey(By, WRTune(TEXT("creatureAI.raiju.arrivalSearch"), 6000.f)))
		{
			ScHunt(By, Prey);
			By->SetActorRotation(FRotator(0.f, (Prey->GetActorLocation() - By->GetActorLocation()).Rotation().Yaw, 0.f));
		}
		WRSound::Voice(By, TEXT("aggro"));
		return;
	}
	if (Body && Body->bReturning)
	{
		UE_LOG(LogWildreach, Log, TEXT("STORM %s is gone into the storm"), *By->GetName());
		By->Destroy();
		return;
	}
	// A Rodcrest by a strike glows after it (copper crest lit).
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (ScKind(It->CreatureId) == TEXT("rodcrest") && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Call.Spot) < WRTune(TEXT("creatureAI.rodcrest.glowRadius"), 3000.f))
		{
			if (UWRStormBody* R = UWRStormBody::Get(*It)) R->LitUntil = ScNow(*It) + 8.0;
		}
	}
}

// ---------------------------------------------------------------- creature hooks

USkeletalMeshComponent* AWRCreature::GetCreatureSkel() const
{
	return Skel;
}

void AWRCreature::OnDamageTaken(const FWRHit& Hit, float Damage)
{
	Super::OnDamageTaken(Hit, Damage);
	if (HasAuthority() && UWRStormBody::Get(this))
	{
		FWRStormAI::Damaged(this, Hit, Damage);
	}
}

// ---------------------------------------------------------------- owner-only marks

TArray<TWeakObjectPtr<AWRStormMark>> AWRStormMark::Live;

AWRStormMark::AWRStormMark()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRStormMark::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRStormMark, Kind);
	DOREPLIFETIME(AWRStormMark, Follow);
}

AWRStormMark* AWRStormMark::Spawn(APlayerController* For, const FVector& At, AActor* InFollow, FName InKind, float Seconds)
{
	if (!For)
	{
		return nullptr;
	}
	// One mark per thing: a fresh find of the same thing renews it.
	for (TActorIterator<AWRStormMark> It(For->GetWorld()); It; ++It)
	{
		if (It->GetOwner() == For && ((InFollow && It->Follow == InFollow) || (!InFollow && FVector::Dist(It->GetActorLocation(), At) < 100.f)))
		{
			It->SetLifeSpan(Seconds);
			return *It;
		}
	}
	FActorSpawnParameters Params;
	Params.Owner = For;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRStormMark* Mark = For->GetWorld()->SpawnActor<AWRStormMark>(AWRStormMark::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (Mark)
	{
		Mark->Kind = InKind;
		Mark->Follow = InFollow;
		Mark->SetLifeSpan(Seconds);
	}
	return Mark;
}

void AWRStormMark::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Live.RemoveAll([](const TWeakObjectPtr<AWRStormMark>& M) { return !M.IsValid(); });
	Live.Add(this);
	// A beacon only its owner sees: sparks over the ore (copper blue-green), a violet-white ring over a spotted creature.
	Fx = WRFx::Attach(RootComponent, Kind == TEXT("ore") ? TEXT("storm.mark.ore") : TEXT("storm.mark.creature"), NAME_None, 1.f);
	WRSound::PlayUI(this, TEXT("discover"));
}

void AWRStormMark::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Follow && HasAuthority())
	{
		const AActor* F = Follow;
		SetActorLocation(F->GetActorLocation() + FVector(0.f, 0.f, F->GetSimpleCollisionHalfHeight() + 120.f));
	}
	else if (Follow)
	{
		SetActorLocation(Follow->GetActorLocation() + FVector(0.f, 0.f, Follow->GetSimpleCollisionHalfHeight() + 120.f));
	}
}

// ---------------------------------------------------------------- the land's living world

bool UWRStormLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRStormLife::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	StrikeHandle = WRStorm::OnStrikeLanded().AddUObject(this, &UWRStormLife::OnStrike);
	InWorld.GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &UWRStormLife::Step), FMath::Max(0.5f, WRTune(TEXT("creatureAI.storm.lifeStepSeconds"), 2.f)), true);
}

void UWRStormLife::Deinitialize()
{
	if (StrikeHandle.IsValid())
	{
		WRStorm::OnStrikeLanded().Remove(StrikeHandle);
		StrikeHandle.Reset();
	}
	Super::Deinitialize();
}

void UWRStormLife::OnStrike(UWorld* World, const FWRStrikeCall& Call)
{
	if (World != GetWorld())
	{
		return;
	}
	FWRStormAI::StrikeLanded(World, Call);
	FWRStormBoss::StrikeLanded(World, Call);
	// A storm's strikes on the high ground now and then bring a Raiju down in them (at most two near anyone).
	if (Call.Source != EWRStrikeSource::Weather || !FWRCreatureRegistry::Get().Find(TEXT("raiju")))
	{
		return;
	}
	const TCHAR Zone = WRStorm::ZoneAt(World, Call.Spot);
	if (!(Zone == TEXT('2') || Zone == TEXT('3') || Zone == TEXT('4') || Zone == TEXT('6')) || FMath::FRand() > WRTune(WRStorm::GreatStorm(World) ? TEXT("events.greatStorm.raijuChance") : TEXT("creatureAI.raiju.strikeChance"), WRStorm::GreatStorm(World) ? 0.1667f : 0.08f))
	{
		return;
	}
	const float Near = WRTune(TEXT("creatureAI.raiju.capRadius"), 15000.f);
	int32 Count = 0;
	bool bPlayer = false;
	for (TActorIterator<AWRCharacter> It(World); It; ++It) bPlayer |= It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Call.Spot) < Near;
	for (TActorIterator<AWRCreature> It(World); It; ++It) Count += It->IsAlive() && !It->IsTamed() && It->CreatureId == TEXT("raiju") && FVector::Dist2D(It->GetActorLocation(), Call.Spot) < Near;
	if (!bPlayer || Count >= FMath::RoundToInt(WRTune(TEXT("creatureAI.raiju.maxNear"), 2.f)))
	{
		return;
	}
	if (AWRCreature* Raiju = AWRCreature::SpawnCreature(World, TEXT("raiju"), Call.Spot, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)))
	{
		if (UWRStormBody* Body = UWRStormBody::Get(Raiju))
		{
			// It came in this one.
			Body->Set(WRStormFlag::Arriving, true);
			Body->ArriveCalledAt = Body->GetWorld()->GetTimeSeconds();
			FWRStrikeCall Here = Call;
			Here.Instigator = Raiju;
			FWRStormAI::StrikeLanded(World, Here);
		}
		UE_LOG(LogWildreach, Log, TEXT("STORM a Raiju rides a strike down at %s"), *Call.Spot.ToCompactString());
	}
}

void UWRStormLife::Step()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Hour = AWRGameState::HourIn(World);
	BeatHunt(Hour);
	BeatClash(Hour);
	BeatSnatch();
}

namespace WRStormDetail
{
	bool ScDusk(float Hour)
	{
		return Hour >= WRTune(TEXT("creatureAI.storm.duskFrom"), 17.5f) && Hour <= WRTune(TEXT("creatureAI.storm.duskTo"), 20.f);
	}

	bool ScPlayerWithin(const UWorld* World, const FVector& At, float Range)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It) if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), At) < Range) return true;
		return false;
	}
}

void UWRStormLife::BeatHunt(float Hour)
{
	// Voltfangs hunt Puffram flocks at dusk (the Galebacks ring up facing them: FWRStormAI::TickWild).
	if (!ScDusk(Hour))
	{
		return;
	}
	UWorld* World = GetWorld();
	for (TActorIterator<AWRCreature> Fang(World); Fang; ++Fang)
	{
		if (!ScWild(*Fang) || ScKind(Fang->CreatureId) != TEXT("voltfang") || Fang->GetAggroTarget() || Fang->IsHungry()) continue;
		for (TActorIterator<AWRCreature> Ram(World); Ram; ++Ram)
		{
			if (ScWild(*Ram) && ScKind(Ram->CreatureId) == TEXT("puffram") && FVector::Dist2D(Ram->GetActorLocation(), Fang->GetActorLocation()) < WRTune(TEXT("creatureAI.storm.duskHuntRadius"), 8000.f))
			{
				Fang->MakeHungry();
				break;
			}
		}
	}
}

void UWRStormLife::BeatClash(float Hour)
{
	// Thunderhorns clash at the Rams' Ring at dusk (the crack carries 1 km): two rams near the ring pair off.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	FVector Ring;
	if (!ScDusk(Hour) || T < NextClashAt || !FWRStormBoss::SiteAt(World, TEXT("rams_ring"), Ring) || !ScPlayerWithin(World, Ring, WRTune(TEXT("creatureAI.storm.clashWatchRadius"), 40000.f)))
	{
		return;
	}
	NextClashAt = T + 20.0;
	TArray<AWRCreature*> Rams;
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		UWRStormBody* B = UWRStormBody::Get(*It);
		if (ScWild(*It) && !It->IsA<AWRBoss>() && ScKind(It->CreatureId) == TEXT("thunderhorn") && B && T >= B->BeatUntil && !It->GetAggroTarget()
			&& FVector::Dist2D(It->GetActorLocation(), Ring) < WRTune(TEXT("creatureAI.storm.clashGather"), 30000.f))
		{
			Rams.Add(*It);
		}
	}
	if (Rams.Num() < 2)
	{
		return;
	}
	Rams.Sort([&Ring](const AWRCreature& A, const AWRCreature& B) { return FVector::DistSquared2D(A.GetActorLocation(), Ring) < FVector::DistSquared2D(B.GetActorLocation(), Ring); });
	const double Until = T + WRTune(TEXT("creatureAI.storm.clashSeconds"), 75.f);
	for (int32 i = 0; i < 2; ++i)
	{
		UWRStormBody* B = UWRStormBody::Get(Rams[i]);
		B->Partner = Rams[1 - i];
		B->BeatUntil = Until;
		B->BeatStep = i;   // 0 / 1: which side of the ring
		B->ScoutAt = Ring;
		Rams[i]->SetAsleep(false);
	}
	UE_LOG(LogWildreach, Log, TEXT("STORM two Thunderhorns square off at the Rams' Ring"));
}

void UWRStormLife::BeatSnatch()
{
	// Stormkites snatch Rodcrests off basking rocks and drop them (they're tough): you see a Rodcrest fall.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (T < NextSnatchAt)
	{
		return;
	}
	NextSnatchAt = T + FMath::FRandRange(WRTune(TEXT("creatureAI.storm.snatchMin"), 90.f), WRTune(TEXT("creatureAI.storm.snatchMax"), 180.f));
	for (TActorIterator<AWRCreature> Lizard(World); Lizard; ++Lizard)
	{
		UWRStormBody* LB = UWRStormBody::Get(*Lizard);
		if (!ScWild(*Lizard) || !LB || ScKind(Lizard->CreatureId) != TEXT("rodcrest") || Lizard->IsFleeing() || LB->SnatchedBy.IsValid() || !ScPlayerWithin(World, Lizard->GetActorLocation(), 15000.f)) continue;
		for (TActorIterator<AWRCreature> Kite(World); Kite; ++Kite)
		{
			UWRStormBody* KB = UWRStormBody::Get(*Kite);
			if (ScWild(*Kite) && KB && ScKind(Kite->CreatureId) == TEXT("stormkite") && !Kite->GetAggroTarget() && T >= KB->BeatUntil
				&& FVector::Dist(Kite->GetActorLocation(), Lizard->GetActorLocation()) < WRTune(TEXT("creatureAI.storm.snatchRadius"), 6000.f))
			{
				KB->Partner = *Lizard;
				KB->BeatUntil = T + 25.0;
				KB->BeatStep = 10;   // the snatch: 10 dive, 11 carry, 12 done
				UE_LOG(LogWildreach, Log, TEXT("STORM %s goes for %s"), *Kite->GetName(), *Lizard->GetName());
				return;
			}
		}
	}
}
