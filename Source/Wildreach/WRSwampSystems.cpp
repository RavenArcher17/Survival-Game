// The Swamp's hazards, events and hidden places. See WRSwampSystems.h. Placement comes from Data/swamp.json
// (Tools/terrain/swamp.mjs), numbers from tuning swampHazards.* / redTide.*; nothing here runs without the data.

#include "WRSwampSystems.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFarm.h"
#include "WRFarmLook.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRStormTypes.h"
#include "WRSwampBosses.h"
#include "WRSwampCreatures.h"
#include "WRSwampWorld.h"
#include "WRBloodPath.h"
#include "WRBoss.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

namespace WRSwampTags
{
	UE_DEFINE_GAMEPLAY_TAG(Quag, "Swamp.Quag");
	UE_DEFINE_GAMEPLAY_TAG(Sunk, "Swamp.Sunk");
}

// ================================================================================================ helpers

namespace WRSwampSys
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;

	float H(const TCHAR* Path, float Fallback) { return WRTune(*(FString(TEXT("swampHazards.")) + Path), Fallback); }
	float Tide(const TCHAR* Path, float Fallback) { return WRTune(*(FString(TEXT("redTide.")) + Path), Fallback); }
	float Now(const UObject* Ctx) { return UWRWeatherSubsystem::Now(Ctx); }
	double Hours(const UObject* Ctx) { const AWRGameState* GS = AWRGameState::Get(Ctx); return GS ? GS->GetTotalHours() : 0.0; }
	bool IsServer(const UObject* Ctx) { const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr; return W && W->GetNetMode() != NM_Client; }

	AWRPlayerController* PCOf(const AActor* A)
	{
		const APawn* P = Cast<APawn>(A);
		return P ? Cast<AWRPlayerController>(P->GetController()) : nullptr;
	}
	void Tell(const AActor* A, const FString& Text)
	{
		if (AWRPlayerController* PC = PCOf(A)) PC->ClientNotify(Text);
	}
	FVector Feet(const ACharacter* C)
	{
		return C->GetActorLocation() - FVector(0.f, 0.f, C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f);
	}
	AWRCreature* MountOf(const AActor* Rider) { return Rider ? Cast<AWRCreature>(Rider->GetAttachParentActor()) : nullptr; }
	bool MountResists(const AActor* Rider, FName What)
	{
		const AWRCreature* M = MountOf(Rider);
		const FWRCreatureDef* D = M ? M->GetDef() : nullptr;
		return D && D->Resists.Contains(What);
	}
	AWRPlayerState* StateNamed(UWorld* World, const FString& Name)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerState* PS = It->Get() ? It->Get()->GetPlayerState<AWRPlayerState>() : nullptr;
			if (PS && PS->GetPlayerName() == Name) return PS;
		}
		return nullptr;
	}
	/** swamp.json coven.radius (the Deep Pool's), cm. */
	float DeepPoolRadius()
	{
		const TSharedPtr<FJsonObject>& Root = FWRSwampData::Get().Root;
		const TSharedPtr<FJsonObject>* Cv = nullptr;
		double R = 20.0;
		if (Root.IsValid() && Root->TryGetObjectField(TEXT("coven"), Cv)) (*Cv)->TryGetNumberField(TEXT("radius"), R);
		return float(R) * 100.f;
	}
	float GroundZ(UWorld* World, const FVector& At, float Fallback, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSwampGround), false, Ignore);
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 3000.f), At - FVector(0, 0, 6000.f), ECC_WorldStatic, Q) && !Cast<APawn>(Hit.GetActor())) return Hit.ImpactPoint.Z;
		return Fallback;
	}
	bool AnyPlayerNear(UWorld* World, const FVector& At, float Range)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && FVector::Dist(P->GetActorLocation(), At) < Range) return true;
		}
		return false;
	}

	// The Mire's palette (DC 1.3 rule 7, 2.1): peat, black-green, bone, bronze; garnet / oxblood / rust for the red.
	FLinearColor Col(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	const FLinearColor SwsPeat = Col(38, 28, 20);
	const FLinearColor PeatDark = Col(20, 15, 11);
	const FLinearColor StoneC = Col(74, 70, 60);
	const FLinearColor StoneDark = Col(40, 38, 34);
	const FLinearColor BoneC = Col(196, 186, 160);
	const FLinearColor BronzeC = Col(132, 94, 48);
	const FLinearColor Garnet = Col(122, 10, 20);
	const FLinearColor SwsOxblood = Col(74, 10, 15);
	const FLinearColor RustC = Col(138, 59, 30);
	const FLinearColor TimberC = Col(58, 44, 32);
	const FLinearColor RootC = Col(46, 34, 24);
	const FLinearColor Leather = Col(78, 52, 32);
	const FLinearColor MossC = Col(40, 52, 30);
	FSurf Stone(float K = 1.f) { return FSurf::Stone(StoneC * K); }
	FSurf SwsPeatS(float K = 1.f) { FSurf S = FSurf::Stone(SwsPeat * K, 0.97f); S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf SwsBone() { return FSurf::Stone(BoneC * 0.75f, 0.8f); }
	FSurf Bronze() { FSurf S = FSurf::Steel(BronzeC * 0.6f, 0.5f); S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf Wood(float K = 1.f) { return FSurf::Wood(TimberC * 0.5f * K); }
	FSurf Root() { return FSurf::Wood(RootC * 0.5f); }
	FSurf Iron() { return FSurf::Steel(FLinearColor(0.03f, 0.028f, 0.026f), 0.7f); }
	FSurf Blood(float K = 1.f) { return FSurf::Glaze(SwsOxblood * 0.6f * K, 0.35f); }
	FSurf Murk() { return FSurf::Glaze(FLinearColor(0.006f, 0.008f, 0.007f), 0.08f); }
	FSurf BleedWater() { return FSurf::Glaze(SwsOxblood * 0.45f, 0.1f); }
	FSurf Skin() { FSurf S = FSurf::Stone(Leather * 0.55f, 0.6f); S.Pattern = WRLookPattern::Mottle; return S; }
	FSurf Wax() { return FSurf::Glaze(Garnet * 0.5f, 0.3f); }
	FSurf Rope() { return FSurf::Cloth(Col(110, 92, 64) * 0.5f); }
	FSurf Moss() { return FSurf::Cloth(MossC * 0.5f); }

	UBoxComponent* Block(AActor* Owner, const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(Owner);
		B->SetupAttachment(Owner->GetRootComponent());
		B->SetBoxExtent(Half.ComponentMax(FVector(2.f)));
		B->SetRelativeLocation(Centre);
		B->SetRelativeRotation(Rot);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetCanEverAffectNavigation(false);
		B->RegisterComponent();
		return B;
	}
	UPointLightComponent* Light(AActor* Owner, const FVector& At, const FLinearColor& Colour, float Intensity, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Owner->GetRootComponent());
		L->SetRelativeLocation(At);
		L->SetLightColor(Colour);
		L->SetIntensity(Intensity);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->RegisterComponent();
		return L;
	}

	/** A box seen and stood on: drawn into the kit, and (Collide) a box collider with the same frame. */
	void Solid(AActor* Owner, FKit& K, const FSurf& S, const FVector& Size, const FVector& Centre, const FRotator& Rot, bool bCollide)
	{
		K.Add(S, Size, Centre, Rot);
		if (bCollide) Block(Owner, Centre, Size * 0.5f, Rot);
	}

	/** A seated bog body (DC 2.5 gallery; 2.20): legs drawn up, hands round a bronze bowl, a braided cord at the neck,
	 *  the leather-dark skin of the acid peat, calm face. Facing +X from Base (the niche's floor). */
	void BogBody(FKit& K, const FVector& Base, float Yaw, bool bBowl, float Seed)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		const float Lean = 6.f + Seed * 8.f;
		K.Add(Skin(), FVector(30.f, 38.f, 56.f), P(-6.f, 0, 62.f), FRotator(-Lean, Yaw, 0.f));                    // torso
		K.Add(Skin(), FVector(22.f, 20.f, 26.f), P(-2.f + Seed * 3.f, 0, 104.f), FRotator(-Lean * 0.5f, Yaw, 0.f), EWRLookMesh::Cylinder); // head
		K.Add(FSurf::Cloth(SwsPeat * 0.5f), FVector(24.f, 22.f, 8.f), P(-6.f, 0, 116.f), FRotator(0.f, Yaw, 0.f));   // hair, matted
		for (const float S : { -1.f, 1.f })
		{
			K.Rod(Skin(), P(-4.f, S * 10.f, 38.f), P(26.f, S * 11.f, 52.f), 11.f);                               // thighs drawn up
			K.Rod(Skin(), P(26.f, S * 11.f, 52.f), P(30.f, S * 10.f, 8.f), 9.f);                                  // shins
			K.Rod(Skin(), P(-2.f, S * 19.f, 84.f), P(14.f, S * 14.f, 60.f), 7.f);                                 // upper arms
			K.Rod(Skin(), P(14.f, S * 14.f, 60.f), P(30.f, S * 5.f, 62.f), 6.f);                                  // forearms to the bowl
		}
		K.Add(Skin(), FVector(40.f, 34.f, 10.f), P(2.f, 0, 30.f), FRotator(0.f, Yaw, 0.f));                       // hips
		// The braided cord: three strands round the neck, the knot loose at the front.
		K.Add(FSurf::Cloth(Garnet * 0.45f), FVector(18.f, 18.f, 3.f), P(-3.f, 0, 90.f), FRotator(0.f, Yaw, 0.f), EWRLookMesh::Cylinder);
		K.Rod(FSurf::Cloth(Garnet * 0.45f), P(8.f, 0, 89.f), P(14.f, 2.f, 76.f), 2.f);
		if (bBowl) K.Disc(Bronze(), P(32.f, 0, 64.f), FVector::UpVector, 20.f, 7.f);
	}

	/** A cache box: dark wood bound in bronze, bone pegs, a garnet seal; Size along X. */
	void Chest(FKit& K, const FVector& Base, float Yaw, float Size, const FSurf& Band)
	{
		const FRotator R(0.f, Yaw, 0.f);
		const auto P = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, Z)); };
		K.Add(Wood(0.7f), FVector(Size, Size * 0.62f, Size * 0.5f), P(0, 0, Size * 0.25f), R);
		K.Add(Wood(0.8f), FVector(Size * 1.04f, Size * 0.66f, Size * 0.12f), P(0, 0, Size * 0.56f), R);
		for (const float X : { -0.32f, 0.32f }) K.Add(Band, FVector(Size * 0.07f, Size * 0.68f, Size * 0.64f), P(X * Size, 0, Size * 0.31f), R);
		K.Add(SwsBone(), FVector(Size * 0.12f, 4.f, Size * 0.12f), P(0, Size * 0.33f, Size * 0.4f), R);
		K.Disc(Wax(), P(0, Size * 0.34f, Size * 0.42f), R.RotateVector(FVector::RightVector), Size * 0.09f, 2.f);
	}

	/** Down the ladder, through the hole, out of the gate: a short fade to black, the move, the fade back (the Coven's
	 *  own space, like a war-hall's). Server. */
	void Travel(AWRPlayerController* PC, const FVector& To, float Yaw, FName Sound)
	{
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		if (!World) return;
		const float Fade = H(TEXT("coven.fade"), 0.6f);
		PC->ClientSetCameraFade(true, FColor::Black, FVector2D(0.f, 1.f), Fade, true, true);
		if (!Sound.IsNone()) WRSound::Broadcast(Pawn, Sound);
		const TWeakObjectPtr<AWRPlayerController> WeakPC = PC;
		FTimerHandle T;
		World->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WeakPC, To, Yaw, Fade]()
		{
			AWRPlayerController* P = WeakPC.Get();
			APawn* Moved = P ? P->GetPawn() : nullptr;
			if (!Moved) return;
			Moved->TeleportTo(To + FVector(0, 0, Moved->GetSimpleCollisionHalfHeight() + 20.f), FRotator(0.f, Yaw, 0.f));
			P->SetControlRotation(FRotator(0.f, Yaw, 0.f));
			P->ClientSetRotation(FRotator(0.f, Yaw, 0.f), true);
			P->ClientSetCameraFade(true, FColor::Black, FVector2D(1.f, 0.f), Fade, true, false);
		}), Fade + 0.1f, false);
	}

	/** Items listed in items.json "ammo" for an ammo id: its status buildups (bleed, shock...). */
	const TMap<FName, float>& AmmoStatus(FName Id)
	{
		static TMap<FName, TMap<FName, float>> Cache;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("items.json"));
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), Items))
			{
				for (const TSharedPtr<FJsonValue>& V : *Items)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					const TSharedPtr<FJsonObject>* Ammo = nullptr;
					FString ItemId;
					if (!O.IsValid() || !O->TryGetStringField(TEXT("id"), ItemId) || !O->TryGetObjectField(TEXT("ammo"), Ammo)) continue;
					TMap<FName, float>& Out = Cache.FindOrAdd(FName(*ItemId));
					for (const auto& KV : (*Ammo)->Values)
					{
						if (WRStatus::FromName(FName(*KV.Key)) < WRStatus::Count && KV.Value->Type == EJson::Number) Out.Add(FName(*KV.Key), float(KV.Value->AsNumber()));
					}
				}
			}
		}
		static const TMap<FName, float> None;
		const TMap<FName, float>* Found = Cache.Find(Id);
		return Found ? *Found : None;
	}

	/** Did this survivor's bag or worn gear let them wade bleedwater unhurt (x0), or the tar (x0.3)? */
	float BleedwaterScale(const AWRCharacter* C)
	{
		if (UWRInventoryComponent::GearStatFor(C, TEXT("passBleedwater")) > 0.f) return 0.f;    // sealed waders, Mirehide 4
		if (UWRSwampBody::MountPasses(C, TEXT("bleedwater"))) return 0.f;                       // a Mirestrider / Mudhulk under you
		return WRStorm::HasBuff(C, TEXT("leech_tar")) ? H(TEXT("bleedwater.tarMultiplier"), 0.3f) : 1.f;
	}
}

// (Each function opens WRSwampSys itself: a file-scope using-directive would leak into the unity build's next files.)

// ================================================================================================ the shared calls

EWRSwampWater WRSwamp::HazardWaterAt(const UObject* WorldContext, const FVector& Pos)
{
	using namespace WRSwampSys;
	if (AWRSwampCoven::IsBleedPool(Pos)) return EWRSwampWater::Bleedwater;
	const FWRSwampData& D = FWRSwampData::Get();
	if (!D.bLoaded) return EWRSwampWater::Dry;
	if (WRSwamp::InCoven(Pos))
	{
		// The Drowned Lane's knee water runs red in the tide like the murk above it (the mat lifts; the tithe comes in).
		FVector Lc, Lh;
		if (IsTide(WorldContext) && AWRSwampCoven::RoomBox(TEXT("drowned_lane"), Lc, Lh) && FMath::Abs(Pos.X - Lc.X) < Lh.X && FMath::Abs(Pos.Y - Lc.Y) < Lh.Y
			&& Pos.Z < Lc.Z - Lh.Z + H(TEXT("coven.laneWater"), 50.f) + 10.f && Pos.Z > Lc.Z - Lh.Z - 100.f) return EWRSwampWater::Bleedwater;
		return EWRSwampWater::Dry;
	}
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<AWRSwampPuddle> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Pos) < It->Radius && FMath::Abs(It->GetActorLocation().Z - Pos.Z) < 150.f) return EWRSwampWater::Bleedwater;
		}
	}
	if (!D.InFrame(Pos)) return EWRSwampWater::Dry;
	EWRSwampWater K = WaterAt(WorldContext, Pos);
	// The Deep Pool's top 2 m is bleedwater (DC 2.5: the dive).
	if (!D.DeepPool.IsZero())
	{
		const float R = DeepPoolRadius();
		if (FVector::Dist2D(Pos, D.DeepPool) < R && Pos.Z < D.SeaZ && Pos.Z > D.SeaZ - 200.f) return EWRSwampWater::Bleedwater;
	}
	// Rain: pools spread a little (cells within rainSpread of bleedwater count as bleedwater).
	if ((K == EWRSwampWater::Murk || K == EWRSwampWater::Duckweed) && IsRaining(WorldContext))
	{
		const float S = H(TEXT("bleedwater.rainSpread"), 300.f);
		for (const FVector& O : { FVector(S, 0, 0), FVector(-S, 0, 0), FVector(0, S, 0), FVector(0, -S, 0) })
		{
			if (WaterAt(WorldContext, Pos + O) == EWRSwampWater::Bleedwater) return EWRSwampWater::Bleedwater;
		}
	}
	return K;
}

bool WRSwamp::IsTide(const UObject* WorldContext)
{
	using namespace WRSwampSys;
	const AWRSwampState* S = AWRSwampState::Get(WorldContext);
	return S && S->IsTide();
}

FName WRSwamp::WeatherNow(const UObject* WorldContext)
{
	using namespace WRSwampSys;
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(WorldContext, TEXT("swamp"));
	return K ? K->Id : FName(TEXT("clear"));
}

bool WRSwamp::IsRaining(const UObject* WorldContext) { return WeatherNow(WorldContext) == TEXT("rain"); }
float WRSwamp::AmbushRangeMult(const UObject* WorldContext) { return WeatherNow(WorldContext) == TEXT("fog") ? 2.f : 1.f; }
float WRSwamp::CurseCryMult(const UObject* WorldContext) { return WeatherNow(WorldContext) == TEXT("miasma") ? 1.5f : 1.f; }
bool WRSwamp::CandlesByDay(const UObject* WorldContext) { return WeatherNow(WorldContext) == TEXT("miasma"); }

float WRSwamp::SpawnMultiplier(const UObject* WorldContext, FName CreatureId)
{
	using namespace WRSwampSys;
	if (!IsTide(WorldContext)) return 1.f;
	return CreatureId == TEXT("gorgeleech") || CreatureId == TEXT("latch_leech") ? Tide(TEXT("leechSpawn"), 3.f) : 1.f;
}

float WRSwamp::SacDropMult(const UObject* WorldContext) { return IsTide(WorldContext) ? WRSwampSys::Tide(TEXT("sacDrops"), 2.f) : 1.f; }
float WRSwamp::BloatGorgeMult(const UObject* WorldContext) { return IsTide(WorldContext) ? WRSwampSys::Tide(TEXT("bloatGorge"), 2.f) : 1.f; }

WRSwamp::FOnWaterShocked& WRSwamp::OnWaterShocked() { static FOnWaterShocked D; return D; }
WRSwamp::FOnSalted& WRSwamp::OnSalted() { static FOnSalted D; return D; }

void WRSwamp::FireAt(const UObject* WorldContext, const FVector& At, AActor* By)
{
	using namespace WRSwampSys;
	UWRSwampSystems* Sys = UWRSwampSystems::Get(WorldContext);
	if (!Sys || !Sys->bServer) return;
	// Mother Maw's nest mound catches too (fire on it draws her out: the Bosses track).
	if (UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr) FWRSwampBoss::FireAt(World, At, By);
	for (const TWeakObjectPtr<AWRMarshGas>& G : Sys->GasFields)
	{
		if (G.IsValid() && FVector::Dist2D(G->GetActorLocation(), At) < G->Reach() + 4000.f) G->TryIgnite(At, By);
	}
}

void WRSwamp::ShockWater(UWorld* World, const FVector& Center, float Radius, float Seconds, AActor* Instigator)
{
	using namespace WRSwampSys;
	UWRSwampSystems* Sys = UWRSwampSystems::Get(World);
	if (!World || !Sys || !Sys->bServer) return;
	UWRSwampSystems::FShock S;
	S.Center = Center;
	S.Radius = Radius;
	S.Until = Now(World) + Seconds;
	S.By = Instigator;
	Sys->Shocks.Add(S);
	WRFx::Broadcast(World, TEXT("swamp.water_shock"), Center, FRotator::ZeroRotator, Radius / 600.f, TEXT("swamp.rod.shock"), Seconds);
	OnWaterShocked().Broadcast(World, Center, Radius, Seconds, Instigator);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP water shocked at %s (%.0f cm, %.1f s)"), *Center.ToCompactString(), Radius, Seconds);
}

void WRSwamp::ThrownLanded(AActor* Projectile, FName Kind, const FVector& At, AActor* HitActor, AWRCharacterBase* By)
{
	using namespace WRSwampSys;
	UWorld* World = Projectile ? Projectile->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client) return;
	if (Kind == TEXT("stormjar"))
	{
		// The jar breaks and its stormglass discharges into whatever water it fell in (6 m, 1.5 s).
		WRFx::Broadcast(World, TEXT("swamp.jar.burst"), At, FRotator::ZeroRotator, 1.f, TEXT("swamp.jar.burst"));
		const bool bWater = WRSwamp::HazardWaterAt(World, At) != EWRSwampWater::Dry || (UWRWaterSubsystem::Get(World) && UWRWaterSubsystem::Get(World)->DepthAt(At) > 20.f)
			|| AWRSwampCoven::IsBleedPool(At);
		float PoolZ = 0.f, PoolDepth = 0.f;
		if (bWater || WRSwamp::CovenPool(At, PoolZ, PoolDepth)) ShockWater(World, At, H(TEXT("stormJar.radius"), 600.f), H(TEXT("stormJar.seconds"), 1.5f), By);
		return;
	}
	if (Kind == TEXT("saltpouch"))
	{
		// Salt bursts in a white puff: leeches let go, the Bloat vomits (their tracks listen); anyone else's eyes sting.
		WRFx::Broadcast(World, TEXT("swamp.salt.burst"), At, FRotator::ZeroRotator, 1.f, TEXT("swamp.salt.burst"));
		const float R = H(TEXT("salt.radius"), 160.f);
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive()) continue;
			const bool bStruck = C == HitActor || FVector::Dist(C->GetActorLocation(), At) < R + C->GetSimpleCollisionRadius() + C->GetSimpleCollisionHalfHeight() * 0.6f;
			if (!bStruck) continue;
			OnSalted().Broadcast(C, By, At);
			const AWRCreature* Cr = Cast<AWRCreature>(C);
			const FName Id = Cr ? Cr->CreatureId : NAME_None;
			if (Id == TEXT("bloat") && By)
			{
				if (const APlayerState* PS = By->GetPlayerState()) if (UWRSwampSystems* Sys = UWRSwampSystems::Get(World)) Sys->SaltedBloat.FindOrAdd(C).Add(PS->GetPlayerName());
			}
			const bool bLeech = Id == TEXT("gorgeleech") || Id == TEXT("latch_leech") || Id == TEXT("bloat");
			if (!bLeech && C != By && (!By || By->IsHostileTo(C)))
			{
				C->Status->ApplyStatus(WRTags::Status_Blind, H(TEXT("salt.stingSeconds"), 2.f));
			}
		}
	}
}

bool WRSwamp::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh)
{
	using namespace WRSwampSys;
	if (!Mesh) return false;
	FWRToolMesh M;
	if (ModelPath == TEXT("code:saltpouch"))
	{
		// A hide pouch along +X, 12 cm: a round belly of salt, the neck gathered and bound with a cord, the cord's tail.
		const FLinearColor Hide(0.16f, 0.1f, 0.06f), Cord(0.3f, 0.24f, 0.15f), SaltW(0.75f, 0.74f, 0.7f);
		M.Lathe(FVector(-6.f, 0.f, 0.f), FVector::XAxisVector, 9.f, { FVector(0.f, 1.5f, 1.5f), FVector(0.15f, 4.2f, 4.0f), FVector(0.5f, 5.0f, 4.8f), FVector(0.8f, 3.6f, 3.4f), FVector(1.f, 1.4f, 1.4f) }, 12, Hide, 0.9f, 0.f, 2.f);
		M.Lathe(FVector(3.f, 0.f, 0.f), FVector::XAxisVector, 3.f, { FVector(0.f, 1.4f, 1.4f), FVector(0.5f, 2.2f, 1.6f), FVector(1.f, 2.6f, 2.0f) }, 8, Hide, 0.9f, 0.f, 4.f);
		TArray<FVector> Tie;
		for (int32 i = 0; i < 9; ++i) { const float A = 2.f * PI * i / 9.f; Tie.Add(FVector(3.2f, FMath::Cos(A) * 1.6f, FMath::Sin(A) * 1.6f)); }
		M.Tube(Tie, 0.35f, 5, Cord, 0.9f, true);
		M.Tube({ FVector(3.2f, 1.6f, 0.f), FVector(1.f, 3.f, -1.5f), FVector(-1.f, 3.4f, -3.f) }, 0.3f, 5, Cord, 0.9f);
		M.Lathe(FVector(5.4f, 0.f, 0.f), FVector::XAxisVector, 0.8f, { FVector(0.f, 1.8f, 1.4f), FVector(1.f, 0.6f, 0.5f) }, 8, SaltW, 0.95f);
	}
	else if (ModelPath == TEXT("code:stormjar"))
	{
		// A stoppered glass jar along +X, 14 cm: stormglass fused into its belly (violet-white), copper wire cage.
		const FLinearColor Glass(0.3f, 0.32f, 0.38f), Fulg(0.55f, 0.5f, 0.85f), Cu(0.45f, 0.24f, 0.1f), Cork(0.3f, 0.2f, 0.1f);
		M.Lathe(FVector(-7.f, 0.f, 0.f), FVector::XAxisVector, 11.f, { FVector(0.f, 3.f, 3.f), FVector(0.1f, 5.2f, 5.2f), FVector(0.55f, 5.4f, 5.4f), FVector(0.8f, 3.4f, 3.4f), FVector(0.9f, 2.4f, 2.4f), FVector(1.f, 2.7f, 2.7f) }, 14, Glass, 0.1f);
		M.Lathe(FVector(-5.f, 0.f, 0.f), FVector::XAxisVector, 6.f, { FVector(0.f, 1.f, 1.f), FVector(0.5f, 2.6f, 2.2f), FVector(1.f, 0.8f, 0.8f) }, 8, Fulg, 0.3f, 0.4f, 7.f);
		M.Lathe(FVector(4.f, 0.f, 0.f), FVector::XAxisVector, 2.6f, { FVector(0.f, 2.3f, 2.3f), FVector(1.f, 2.0f, 2.0f) }, 10, Cork, 0.95f);
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = PI * 0.5f * k;
			M.Tube({ FVector(-7.f, FMath::Cos(A) * 3.f, FMath::Sin(A) * 3.f), FVector(-3.f, FMath::Cos(A) * 5.6f, FMath::Sin(A) * 5.6f), FVector(1.f, FMath::Cos(A) * 4.2f, FMath::Sin(A) * 4.2f), FVector(3.6f, FMath::Cos(A) * 2.6f, FMath::Sin(A) * 2.6f) }, 0.25f, 4, Cu, 0.4f);
		}
	}
	else
	{
		return false;
	}
	M.ApplyTo(Mesh);
	return true;
}

void WRSwamp::AddAmmoBuildups(FName AmmoId, FWRHit& Hit)
{
	using namespace WRSwampSys;
	for (const TPair<FName, float>& KV : AmmoStatus(AmmoId)) Hit.Buildups.FindOrAdd(KV.Key) += KV.Value;
}

float WRSwamp::ExtraBleed(const AActor* Attacker, const FWRHit& Hit)
{
	using namespace WRSwampSys;
	if (!Attacker || !Hit.bMelee || Hit.bDamageOverTime) return 0.f;
	return WRStorm::HasBuff(Attacker, TEXT("thinblood")) ? H(TEXT("thinblood.bleedPerHit"), 20.f) : 0.f;
}

void WRSwamp::ScaleBuildups(const AActor* Attacker, FWRHit& Hit)
{
	using namespace WRSwampSys;
	if (!Attacker) return;
	const float Bleed = ExtraBleed(Attacker, Hit);
	if (Bleed > 0.f) Hit.Buildups.FindOrAdd(TEXT("bleed")) += Bleed;
	// The Coven ring: a blow's curse builds 5% more (worn "curseBuildup", on the wearer's own blows).
	if (float* Curse = Hit.Buildups.Find(TEXT("curse")))
	{
		const float Worn = UWRInventoryComponent::GearStatFor(Attacker, TEXT("curseBuildup"));
		if (Worn != 0.f) *Curse *= FMath::Max(0.f, 1.f + Worn);
	}
}

void WRSwamp::OnKilled(AWRCharacterBase* Victim, AActor* Killer, AWRPlayerState* KillerPS)
{
	using namespace WRSwampSys;
	const AWRCreature* C = Cast<AWRCreature>(Victim);
	UWorld* World = Victim ? Victim->GetWorld() : nullptr;
	if (!C || !World) return;
	UWRWorldSystems* WS = UWRWorldSystems::Get(World);
	if (!WS) return;
	if (KillerPS)
	{
		// "The duckweed rule": a Murkjaw that dies out of the water.
		if (C->CreatureId == TEXT("murkjaw"))
		{
			const FVector F = Feet(C);
			const bool bWet = UWRWaterSubsystem::IsInWater(C) || (WRSwamp::IsWet(F) && WRSwamp::WaterAt(World, F) != EWRSwampWater::Dry);
			if (!bWet) WS->QuestEvent(KillerPS, TEXT("murkjaw_on_land"), NAME_None);
		}
		// "A hex returned": a kill while the curse is on it.
		if (Victim->HasTag(WRTags::Status_Curse)) WS->QuestEvent(KillerPS, TEXT("cursed_kill"), NAME_None);
	}
	// "Salt for the Bloat": everyone who salted it this fight.
	UWRSwampSystems* Sys = UWRSwampSystems::Get(World);
	if (Sys && C->CreatureId == TEXT("bloat"))
	{
		if (const TSet<FString>* Who = Sys->SaltedBloat.Find(Victim))
		{
			for (const FString& Name : *Who) if (AWRPlayerState* PS = StateNamed(World, Name)) WS->QuestEvent(PS, TEXT("bloat_salted"), NAME_None);
		}
		Sys->SaltedBloat.Remove(Victim);
	}
}

void WRSwamp::OnBleedCured(AWRCharacterBase* Who)
{
	using namespace WRSwampSys;
	UWRSwampSystems* Sys = UWRSwampSystems::Get(Who);
	AWRCharacter* C = Cast<AWRCharacter>(Who);
	if (!Sys || !C) return;
	UWRSwampSystems::FWader* W = Sys->Waders.Find(C);
	if (!W || !W->bBledInWater) return;
	W->bBledInWater = false;
	if (AWRPlayerState* PS = C->GetPlayerState<AWRPlayerState>()) if (UWRWorldSystems* WS = UWRWorldSystems::Get(C)) WS->QuestEvent(PS, TEXT("bleed_cured"), NAME_None);
}

bool WRSwamp::ShrinePlace(FName Id, FVector2D& OutAt, float& OutYaw)
{
	using namespace WRSwampSys;
	const FWRSwampData& D = FWRSwampData::Get();
	if (!WRSwamp::IsLive()) return false;
	for (const FWRSwampData::FShrine& S : D.Shrines)
	{
		if (S.Id != Id) continue;
		OutAt = FVector2D(S.At);
		// Its steps face the nearest camp (where most people come from), else east to the coast.
		FVector Camp = S.At + FVector(10000.f, 0, 0);
		float Best = 1.0e12f;
		for (const FVector& Cp : D.Camps) { const float Dd = FVector::DistSquared2D(Cp, S.At); if (Dd < Best) { Best = Dd; Camp = Cp; } }
		OutYaw = (Camp - S.At).Rotation().Yaw;
		return true;
	}
	return false;
}

bool WRSwamp::MasterPlace(const TSharedPtr<FJsonObject>& Row, FVector& OutAt, float& OutYaw)
{
	using namespace WRSwampSys;
	FString Day;
	if (!Row.IsValid() || !Row->TryGetStringField(TEXT("swampDay"), Day) || !WRSwamp::IsLive()) return false;
	const FWRSwampData::FSite* S = FWRSwampData::Get().FindSite(FName(*Day));
	if (!S) return false;
	// Beside the Tithe Mound's basin, working (grinding bloodmoss, hanging charms), facing the mound.
	OutAt = S->At + FRotator(0.f, S->Yaw + 140.f, 0.f).Vector() * 450.f;
	OutYaw = (S->At - OutAt).Rotation().Yaw;
	return true;
}

float WRSwamp::CovenLift()
{
	using namespace WRSwampSys;
	return WRTune(TEXT("swampHazards.coven.lift"), 300000.f);
}

bool WRSwamp::InCoven(const FVector& Pos)
{
	using namespace WRSwampSys;
	// The Coven's space: straight over the Mire it lies under, a band about its rooms' floors (-20 m .. +8 m) raised by
	// the lift; nothing else in the game is up there.
	const float Lift = CovenLift();
	if (Pos.Z < Lift - 4000.f || Pos.Z > Lift + 3000.f) return false;
	const FWRSwampData& D = FWRSwampData::Get();
	return D.bLoaded && D.InFrame(Pos) && WRSwamp::IsLive();
}

bool WRSwamp::CovenPool(const FVector& At, float& OutSurfaceZ, float& OutDepth)
{
	using namespace WRSwampSys;
	FVector C, Half;
	float Surface = 0.f;
	if (!AWRSwampCoven::ChapelPool(C, Half, Surface)) return false;
	if (FMath::Abs(At.X - C.X) > Half.X || FMath::Abs(At.Y - C.Y) > Half.Y || At.Z < C.Z - Half.Z - 200.f || At.Z > Surface + 600.f) return false;
	OutSurfaceZ = Surface;
	OutDepth = Surface - (C.Z - Half.Z);
	return true;
}

// ================================================================================================ AWRMarshGas

AWRMarshGas::AWRMarshGas()
{
	using namespace WRSwampSys;
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetUpdateFrequency(4.f);
	SetNetCullDistanceSquared(FMath::Square(25000.f));
}

void AWRMarshGas::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	using namespace WRSwampSys;
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRMarshGas, Vents);
	DOREPLIFETIME(AWRMarshGas, LitAt);
	DOREPLIFETIME(AWRMarshGas, Claimed);
}

void AWRMarshGas::BeginPlay()
{
	using namespace WRSwampSys;
	Super::BeginPlay();
	BuildLook();
}

float AWRMarshGas::Reach() const
{
	using namespace WRSwampSys;
	float R = 0.f;
	for (const FVector& V : Vents) R = FMath::Max(R, FVector::Dist2D(V, GetActorLocation()));
	return R + H(TEXT("marshGas.igniteRadius"), 300.f);
}

bool AWRMarshGas::IsCharged() const
{
	using namespace WRSwampSys;
	if (LitAt <= 0.f) return true;
	const float Run = H(TEXT("marshGas.tell"), 0.9f) + Vents.Num() * H(TEXT("marshGas.chainDelay"), 0.15f);
	return Now(this) > LitAt + Run + H(TEXT("marshGas.recharge"), 60.f);
}

void AWRMarshGas::OnRep_Vents() { BuildLook(); }

void AWRMarshGas::BuildLook()
{
	using namespace WRSwampSys;
	if (GetNetMode() == NM_DedicatedServer || Vents.Num() == 0 || Bubbles.Num() > 0) return;
	// Bubble clusters rising at each vent (slow, fat), and a faint blue flame-flicker over a few at night (the ignis
	// fatuus: real marsh-gas light). The lights are pooled to six.
	for (int32 i = 0; i < Vents.Num(); ++i)
	{
		USceneComponent* At = NewObject<USceneComponent>(this);
		At->SetupAttachment(RootComponent);
		At->SetWorldLocation(FVector(Vents[i].X, Vents[i].Y, FMath::Max<float>(Vents[i].Z, FWRSwampData::Get().SeaZ)));
		At->RegisterComponent();
		WRFx::Attach(At, TEXT("swamp.gas.vent"), NAME_None, 0.8f + 0.1f * (i % 3));
		Bubbles.Add(At);
		if (i < 6)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent);
			L->SetWorldLocation(At->GetComponentLocation() + FVector(0, 0, 40.f));
			L->SetLightColor(FLinearColor(0.35f, 0.55f, 1.f));
			L->SetIntensity(0.f);
			L->SetAttenuationRadius(500.f);
			L->SetCastShadows(false);
			L->RegisterComponent();
			Flicker.Add(L);
		}
	}
}

bool AWRMarshGas::TryIgnite(const FVector& At, AActor* By)
{
	using namespace WRSwampSys;
	if (!HasAuthority() || Vents.Num() == 0 || !IsCharged()) return false;
	const float R = H(TEXT("marshGas.igniteRadius"), 300.f);
	int32 First = INDEX_NONE;
	float Best = R;
	for (int32 i = 0; i < Vents.Num(); ++i)
	{
		const float D = FVector::Dist2D(Vents[i], At);
		if (D < Best && FMath::Abs(At.Z - FMath::Max<float>(Vents[i].Z, FWRSwampData::Get().SeaZ)) < 400.f) { Best = D; First = i; }
	}
	if (First == INDEX_NONE) return false;
	// The flash runs from the lit vent to its nearest unlit neighbour, on and on (a chain, not a ring).
	TArray<FVector_NetQuantize> Order;
	TArray<FVector_NetQuantize> Left = Vents;
	Order.Add(Left[First]);
	Left.RemoveAt(First);
	while (Left.Num() > 0)
	{
		int32 Near = 0;
		float Nd = 1.0e12f;
		for (int32 i = 0; i < Left.Num(); ++i) { const float D = FVector::DistSquared(Left[i], Order.Last()); if (D < Nd) { Nd = D; Near = i; } }
		Order.Add(Left[Near]);
		Left.RemoveAt(Near);
	}
	Vents = Order;
	// Never into a land claim: a vent inside one stays dark.
	Claimed = 0;
	for (int32 i = 0; i < Vents.Num() && i < 31; ++i) if (AWRClaimTotem::FindAt(GetWorld(), Vents[i])) Claimed |= (1 << i);
	LitAt = Now(this);
	NextVent = 0;
	Igniter = By;
	ForceNetUpdate();
	OnRep_Lit();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP marsh gas lit at %s by %s (%d vents)"), *At.ToCompactString(), *GetNameSafe(By), Vents.Num());
	return true;
}

void AWRMarshGas::OnRep_Lit()
{
	using namespace WRSwampSys;
	bTellShown = false;
	ClientNext = 0;
}

void AWRMarshGas::StepServer(float T)
{
	using namespace WRSwampSys;
	if (LitAt <= 0.f || NextVent >= Vents.Num()) return;
	const float Start = LitAt + H(TEXT("marshGas.tell"), 0.9f), Delay = H(TEXT("marshGas.chainDelay"), 0.15f);
	const float R = H(TEXT("marshGas.flashRadius"), 400.f);
	while (NextVent < Vents.Num() && T >= Start + NextVent * Delay)
	{
		const int32 i = NextVent++;
		if (i < 31 && (Claimed & (1 << i))) continue;
		const FVector V(Vents[i].X, Vents[i].Y, FMath::Max<float>(Vents[i].Z, FWRSwampData::Get().SeaZ));
		// Players, creatures and bosses alike: a survivor is hurt by the field (no instigator: the Mire's own fire),
		// a creature by whoever lit it (the lure-and-light play is the igniter's kill).
		AWRCharacterBase* By = Cast<AWRCharacterBase>(Igniter.Get());
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive()) continue;
			const FVector L = C->GetActorLocation();
			if (FVector::Dist2D(L, V) > R + C->GetSimpleCollisionRadius() || L.Z - C->GetSimpleCollisionHalfHeight() > V.Z + 350.f || L.Z + C->GetSimpleCollisionHalfHeight() < V.Z - 150.f) continue;
			FWRHit Hit;
			Hit.Instigator = C->GetPlayerState() ? nullptr : By;
			Hit.Damage = H(TEXT("marshGas.damage"), 40.f);
			Hit.BurnBuildup = H(TEXT("marshGas.burn"), 40.f);
			Hit.Element = WRTags::Element_Fire;
			Hit.Origin = V;
			Hit.bUnblockable = true;
			Hit.Impact = 2;
			Hit.Tier = EWRHitTier::Heavy;
			UWRCombatLibrary::ApplyHit(C, Hit);
		}
		// A flash next to another field lights that one too (it waits for its own charge).
		WRSwamp::FireAt(this, V, Igniter.Get());
	}
}

void AWRMarshGas::Tick(float DeltaSeconds)
{
	using namespace WRSwampSys;
	Super::Tick(DeltaSeconds);
	const float T = Now(this);
	if (HasAuthority()) StepServer(T);
	if (GetNetMode() == NM_DedicatedServer || Vents.Num() == 0) return;
	// The tell: a rising whoomp and the bubbles boiling up, 0.9 s before the first flash.
	if (LitAt > 0.f && !bTellShown && T >= LitAt && T < LitAt + H(TEXT("marshGas.tell"), 0.9f) + 0.5f)
	{
		bTellShown = true;
		WRFx::SpawnAt(this, TEXT("swamp.gas.tell"), Vents[0], FRotator::ZeroRotator, 1.f);
		WRSound::PlayAt(this, TEXT("swamp.gas.whoomp"), Vents[0]);
	}
	if (LitAt > 0.f)
	{
		const float Start = LitAt + H(TEXT("marshGas.tell"), 0.9f), Delay = H(TEXT("marshGas.chainDelay"), 0.15f);
		while (ClientNext < Vents.Num() && T >= Start + ClientNext * Delay)
		{
			const int32 i = ClientNext++;
			if (T > Start + i * Delay + 1.f || (i < 31 && (Claimed & (1 << i)))) continue;   // long past (joined late), or in a claim
			const FVector V(Vents[i].X, Vents[i].Y, FMath::Max<float>(Vents[i].Z, FWRSwampData::Get().SeaZ));
			WRFx::SpawnAt(this, TEXT("swamp.gas.flash"), V, FRotator::ZeroRotator, H(TEXT("marshGas.flashRadius"), 400.f) / 400.f);
			WRSound::PlayAt(this, TEXT("swamp.gas.flash"), V);
		}
	}
	// The night flicker (dark while it recharges after a flash).
	FlickerClock += DeltaSeconds;
	const bool bNight = AWRGameState::IsNightIn(this) && IsCharged();
	for (int32 i = 0; i < Flicker.Num(); ++i)
	{
		const float F = 0.5f + 0.5f * FMath::Sin(FlickerClock * (2.3f + i * 0.7f) + i * 1.9f) * FMath::Sin(FlickerClock * (0.9f + i * 0.3f));
		Flicker[i]->SetIntensity(bNight ? 220.f * F * F : 0.f);
	}
}

// ================================================================================================ AWRSwampThing

AWRSwampThing::AWRSwampThing()
{
	using namespace WRSwampSys;
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
}

void AWRSwampThing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	using namespace WRSwampSys;
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampThing, Kind);
	DOREPLIFETIME(AWRSwampThing, Title);
	DOREPLIFETIME(AWRSwampThing, Verb);
	DOREPLIFETIME(AWRSwampThing, Reach);
	DOREPLIFETIME(AWRSwampThing, bSpent);
	DOREPLIFETIME(AWRSwampThing, bShut);
	DOREPLIFETIME(AWRSwampThing, bConcealed);
	DOREPLIFETIME(AWRSwampThing, Crop);
	DOREPLIFETIME(AWRSwampThing, Dest);
	DOREPLIFETIME(AWRSwampThing, DestYaw);
}

void AWRSwampThing::BeginPlay()
{
	using namespace WRSwampSys;
	Super::BeginPlay();
	BuildLook();
}

void AWRSwampThing::OnRep_State()
{
	using namespace WRSwampSys;
	BuildLook();
}

void AWRSwampThing::BuildLook()
{
	using namespace WRSwampSys;
	// Door blocks stand on the server too (they stop people).
	if (Kind == TEXT("reliquary_door"))
	{
		if (!DoorBlock) DoorBlock = Block(this, FVector(0, 0, 150.f), FVector(30.f, 160.f, 150.f));
		DoorBlock->SetCollisionEnabled(bShut ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	if (LookRoot) { LookRoot->DestroyComponent(); LookRoot = nullptr; }
	if (Plant) { Plant->DestroyComponent(); Plant = nullptr; }
	if (Glow) { Glow->DestroyComponent(); Glow = nullptr; }
	LookRoot = NewObject<USceneComponent>(this);
	LookRoot->SetupAttachment(RootComponent);
	LookRoot->RegisterComponent();
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const FName Kd = Kind;
	if (Kd == TEXT("cache") || Kd == TEXT("reliquary") || Kd == TEXT("smith_cache"))
	{
		if (!bConcealed)
		{
			Chest(K, FVector::ZeroVector, 0.f, Kd == TEXT("reliquary") ? 110.f : 80.f, Kd == TEXT("smith_cache") ? Iron() : Bronze());
			if (bSpent) K.Add(Wood(0.6f), FVector(80.f, 52.f, 8.f), FVector(-10.f, 46.f, 30.f), FRotator(0.f, 0.f, 70.f));   // the lid thrown back
		}
	}
	else if (Kd == TEXT("lore"))
	{
		// A lectern of black cypress and a pulped bark book with one readable leaf weighted by a hex-ink stone.
		K.Rod(Wood(0.6f), FVector(0, 0, 0), FVector(0, 0, 100.f), 14.f, true);
		K.Add(Wood(0.6f), FVector(60.f, 44.f, 6.f), FVector(0, 0, 104.f), FRotator(-20.f, 0.f, 0.f));
		K.Add(FSurf::Cloth(SwsPeat * 0.9f), FVector(40.f, 30.f, 5.f), FVector(0, 0, 110.f), FRotator(-20.f, 0.f, 0.f));
		K.Add(FSurf::Stone(FLinearColor(0.01f, 0.01f, 0.012f), 0.4f), FVector(12.f, 8.f, 5.f), FVector(-6.f, 8.f, 116.f), FRotator(-20.f, 20.f, 0.f));
	}
	else if (Kd == TEXT("ladder"))
	{
		// The cellar shaft's ladder: iron rungs set in the stone, climbing into the dark.
		for (const float Y : { -24.f, 24.f }) K.Rod(Iron(), FVector(0, Y, 0), FVector(0, Y, 900.f), 5.f);
		for (int32 r = 0; r < 30; ++r) K.Rod(Iron(), FVector(0, -24.f, 20.f + r * 30.f), FVector(0, 24.f, 20.f + r * 30.f), 3.f);
	}
	else if (Kd == TEXT("trapdoor") || Kd == TEXT("key_cage"))
	{
		// Drawn by the Walking House and the Hanging Cypress themselves (WRSwampScenery): the lid with its padlock in the
		// hut's floor, the key cage on its chain with the skeleton's raised hand. This is only where you use them.
	}
	else if (Kd == TEXT("dive_hole"))
	{
		// Where you came up: black water in a ring of peat, roots trailing into it.
		K.Disc(Murk(), FVector(0, 0, 4.f), FVector::UpVector, 360.f, 4.f);
		for (int32 i = 0; i < 10; ++i) K.Add(SwsPeatS(0.8f), FVector(60.f, 40.f, 30.f), FRotator(0.f, 36.f * i, 0.f).Vector() * 200.f + FVector(0, 0, 10.f), FRotator(0.f, 36.f * i, 0.f));
	}
	else if (Kd == TEXT("chimney"))
	{
		if (bShut)
		{
			// The collapsed chimney: a mound of fallen stones and turf, one sooty course still showing.
			for (int32 i = 0; i < 9; ++i) K.Add(Stone(0.8f), FVector(70.f, 50.f, 40.f), FVector(FMath::Cos(i * 0.7f) * 90.f, FMath::Sin(i * 1.3f) * 80.f, 20.f + (i % 3) * 18.f), FRotator(i * 7.f, i * 40.f, 0.f));
			K.Add(SwsPeatS(), FVector(260.f, 240.f, 60.f), FVector(0, 0, 10.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		}
		else
		{
			// Dug open: a round stone flue going down into the dark, the spoil heaped beside it.
			for (int32 i = 0; i < 12; ++i) K.Add(Stone(0.6f), FVector(50.f, 30.f, 40.f), FRotator(0.f, 30.f * i, 0.f).Vector() * 90.f + FVector(0, 0, 10.f), FRotator(0.f, 30.f * i + 90.f, 0.f));
			K.Disc(FSurf::Stone(FLinearColor(0.004f, 0.004f, 0.004f), 1.f), FVector(0, 0, 2.f), FVector::UpVector, 150.f, 4.f);
			K.Add(SwsPeatS(0.9f), FVector(200.f, 160.f, 70.f), FVector(220.f, 60.f, 20.f), FRotator(0.f, 20.f, 0.f), EWRLookMesh::Cylinder);
		}
	}
	else if (Kd == TEXT("gate"))
	{
		// The Weeping Gate: a stone leaf in a bone-and-stone frame, iron-red weeping streaks down its face.
		K.Add(Stone(0.75f), FVector(40.f, 300.f, 340.f), FVector(0, 0, 170.f));
		for (int32 s = 0; s < 6; ++s) K.Add(FSurf::Glaze(RustC * 0.35f, 0.4f), FVector(4.f, 8.f, 120.f + s * 25.f), FVector(-22.f, -110.f + s * 44.f, 260.f - s * 12.f));
		for (const float Y : { -175.f, 175.f }) K.Add(SwsBone(), FVector(50.f, 40.f, 380.f), FVector(0, Y, 190.f));
		K.Add(SwsBone(), FVector(50.f, 400.f, 40.f), FVector(0, 0, 400.f));
	}
	else if (Kd == TEXT("bell"))
	{
		// The Coven bell half-sunk in the silt on its broken yoke: green bronze, a bone clapper.
		K.Add(Bronze(), FVector(190.f, 190.f, 170.f), FVector(0, 0, 40.f), FRotator(168.f, 0.f, 0.f), EWRLookMesh::Cone);
		K.Disc(Bronze(), FVector(0, 0, 130.f), FVector::UpVector, 60.f, 20.f);
		K.Rod(Wood(0.5f), FVector(-180.f, 0, 150.f), FVector(160.f, 30.f, 120.f), 22.f, true);
		K.Add(SwsPeatS(0.7f), FVector(320.f, 300.f, 50.f), FVector(0, 0, -5.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}
	else if (Kd == TEXT("egg"))
	{
		// A Gorgeleech egg case on the wall: a leathery cocoon a metre long (real leech cocoons, spongy and banded).
		if (!bSpent)
		{
			K.Add(FSurf::Glaze(Col(112, 80, 54) * 0.5f, 0.55f), FVector(46.f, 40.f, 100.f), FVector(0, 0, 0), FRotator(0.f, 0.f, 8.f), EWRLookMesh::Cylinder);
			for (int32 b = 0; b < 4; ++b) K.Disc(FSurf::Glaze(Col(90, 60, 40) * 0.5f, 0.6f), FVector(0, 0, -36.f + b * 24.f), FVector::UpVector, 48.f, 4.f);
		}
		else
		{
			for (int32 s = 0; s < 4; ++s) K.Add(FSurf::Glaze(Col(90, 60, 40) * 0.4f, 0.7f), FVector(26.f, 4.f, 70.f), FRotator(0.f, 90.f * s, 0.f).Vector() * 16.f, FRotator(0.f, 90.f * s, 25.f));
		}
	}
	else if (Kd == TEXT("bog_body"))
	{
		if (!bConcealed)
		{
			// The fourth body, half out of the dome's edge where the snout rooted: lying curled, cord at the neck, and
			// (until taken) a twisted gold torc in its hands.
			BogBody(K, FVector(0, 0, -30.f), 0.f, false, 0.4f);
			K.Add(SwsPeatS(1.1f), FVector(220.f, 180.f, 50.f), FVector(-40.f, 0, -20.f), FRotator(0.f, 10.f, 0.f), EWRLookMesh::Cylinder);
			if (!bSpent) K.Add(FSurf::Steel(FLinearColor(0.7f, 0.45f, 0.1f), 0.3f), FVector(18.f, 18.f, 3.f), FVector(30.f, 0, 34.f), FRotator(0.f, 0.f, 70.f), EWRLookMesh::Cylinder);
		}
	}
	else if (Kd == TEXT("reliquary_door"))
	{
		// The sealed stone door; the tide lifts it a metre in its slot.
		const float Lift = bShut ? 0.f : H(TEXT("coven.reliquaryLift"), 100.f) * 2.6f;
		K.Add(Stone(0.65f), FVector(40.f, 300.f, 300.f), FVector(0, 0, 150.f + Lift));
		for (int32 s = 0; s < 5; ++s) K.Add(Blood(0.8f), FVector(4.f, 300.f, 4.f), FVector(-22.f, 0, 40.f + s * 20.f + Lift));   // the tide's lines
	}
	else if (Kd == TEXT("wild_plant"))
	{
		// Wild bog rice or bloodroot: the farm's own plant forms, a clump of them, ripe or cut back.
		if (const FWRCropDef* Def = FWRFarmDefs::Get().Crop(Crop))
		{
			FWRToolMesh M;
			for (int32 i = 0; i < 6; ++i)
			{
				const FVector B = FRotator(0.f, i * 60.f + 20.f, 0.f).Vector() * (i % 2 ? 40.f : 22.f);
				FWRFarmLook::Plant(M, *Def, bSpent ? 0.2f : 1.f, B, i * 53.f, i * 0.17f);
			}
			Plant = NewObject<UProceduralMeshComponent>(this);
			Plant->SetupAttachment(RootComponent);
			Plant->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Plant->RegisterComponent();
			M.ApplyTo(Plant);
		}
	}
	FWRBuildLook::Spawn(this, LookRoot, Blocks, SwsPeat, false);
	bLookBuilt = true;
}

void AWRSwampThing::Refill()
{
	using namespace WRSwampSys;
	if (!bSpent) return;
	bSpent = false;
	RefillAtHours = -1.0;
	TakenBy.Reset();
	ForceNetUpdate();
	OnRep_State();
}

FString AWRSwampThing::Use(AWRPlayerController* PC)
{
	using namespace WRSwampSys;
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!Me || !PS || !Me->IsAlive()) return TEXT("Not now");
	UWRInventoryComponent* Bag = PS->GetInventory();
	UWorld* World = GetWorld();
	const FName Kd = Kind;
	if (bConcealed) return TEXT("Nothing here");
	if (Kd == TEXT("key_cage"))
	{
		if (Bag->CountOf(TEXT("coven_key")) > 0) return TEXT("You have the key already");
		Bag->AddItem(TEXT("coven_key"), 1);
		WRSound::Broadcast(Me, TEXT("swamp.key"));
		return TEXT("You prise the key from the skeleton's fingers");
	}
	if (Kd == TEXT("trapdoor"))
	{
		if (Bag->CountOf(TEXT("coven_key")) <= 0) return TEXT("A trapdoor, locked: a black iron lock");
		if (!TakenBy.Contains(PS->GetPlayerName()))
		{
			TakenBy.Add(PS->GetPlayerName());
			if (UWRWorldSystems* WS = UWRWorldSystems::Get(World)) WS->QuestEvent(PS, TEXT("coven_cellar"), NAME_None);
		}
		if (bShut) { bShut = false; ForceNetUpdate(); OnRep_State(); }
		WRSwampSys::Travel(PC, Dest, DestYaw, TEXT("swamp.trapdoor"));
		return TEXT("The key turns. A shaft, and a ladder going down a long way");
	}
	if (Kd == TEXT("ladder") || Kd == TEXT("dive_hole") || Kd == TEXT("gate"))
	{
		WRSwampSys::Travel(PC, Dest, DestYaw, Kd == TEXT("ladder") ? FName(TEXT("swamp.ladder")) : Kd == TEXT("gate") ? FName(TEXT("swamp.gate")) : FName(TEXT("swamp.dive")));
		return Kd == TEXT("gate") ? TEXT("The Weeping Gate grinds open, and shuts behind you") : Kd == TEXT("ladder") ? TEXT("You climb") : TEXT("You dive back up through the gap in the mat");
	}
	if (Kd == TEXT("chimney"))
	{
		if (bShut) return TEXT("A collapsed chimney: something that digs could open it");
		WRSwampSys::Travel(PC, Dest, DestYaw, TEXT("swamp.dive"));
		return TEXT("You let yourself down the old flue");
	}
	if (Kd == TEXT("bell"))
	{
		for (TActorIterator<AWRSwampCoven> It(World); It; ++It) return It->RingBell(PC);
		return TEXT("The bell is silent");
	}
	if (bSpent) return Kd == TEXT("wild_plant") ? TEXT("Cut back: it will grow again") : TEXT("Empty");
	if (bShut) return Kd == TEXT("reliquary") || Kd == TEXT("reliquary_door") ? TEXT("Sealed. The stone won't move") : TEXT("Shut");
	if (Kd == TEXT("egg"))
	{
		Bag->AddItem(TEXT("gorgeleech_egg"), 1);
		bSpent = true;
		RefillAtHours = Hours(World) + 72.0;
		WRSound::Broadcast(Me, TEXT("swamp.egg.tear"));
		// The brood hears it.
		for (TActorIterator<AWRSwampCoven> It(World); It; ++It) It->RingChimes(Me);
		ForceNetUpdate();
		OnRep_State();
		return TEXT("You cut an egg case loose. Something in the pools turns over");
	}
	if (Kd == TEXT("wild_plant"))
	{
		const FWRCropDef* Def = FWRFarmDefs::Get().Crop(Crop);
		if (!Def) return TEXT("Nothing to pick");
		const int32 N = int32(H(TEXT("wild.yield"), 3.f)), Seeds = int32(H(TEXT("wild.seeds"), 1.f));
		Bag->AddItem(Def->Produce, N);
		if (Seeds > 0) Bag->AddItem(Def->Seed, Seeds);
		bSpent = true;
		RefillAtHours = Hours(World) + H(TEXT("wild.regrowHours"), 48.f);
		Me->MulticastBodyOneShot(TEXT("gather"));
		ForceNetUpdate();
		OnRep_State();
		return FString::Printf(TEXT("%s x%d and seed"), *WRWorld::ItemName(Def->Produce), N);
	}
	if (Kd == TEXT("lore"))
	{
		// The Witch's Library's one readable leaf: the Recipe of the tithe (the Hex-ward charm, for good).
		if (!TakenBy.Contains(PS->GetPlayerName()))
		{
			TakenBy.Add(PS->GetPlayerName());
			for (const auto& KV : Items) Bag->AddItem(KV.Key, KV.Value);
			PS->KnownBlueprints.AddUnique(TEXT("quest_hex_ward_charm"));
			WRSound::PlayUI(PC, TEXT("blueprint.learn"));
			return TEXT("Recipe of the tithe: you can make the Hex-ward charm");
		}
		return Lore.IsEmpty() ? TEXT("You've read it") : Lore;
	}
	// A cache: everything in it, and its line.
	TArray<FString> Got;
	for (const auto& KV : Items)
	{
		const int32 Left = Bag->AddItem(KV.Key, KV.Value);
		Got.Add(FString::Printf(TEXT("%s x%d"), *WRWorld::ItemName(KV.Key), KV.Value - Left));
	}
	bSpent = true;
	RefillAtHours = RefillHours > 0.f ? Hours(World) + RefillHours : -1.0;
	ForceNetUpdate();
	OnRep_State();
	WRSound::Broadcast(Me, TEXT("chest.open"));
	if (!Lore.IsEmpty()) PC->ClientNotify(Lore);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP %s took %s (%s)"), *PS->GetPlayerName(), *Title, *FString::Join(Got, TEXT(", ")));
	return FString::Printf(TEXT("%s: %s"), *Title, *FString::Join(Got, TEXT(", ")));
}

// ================================================================================================ AWRSwampPuddle

AWRSwampPuddle::AWRSwampPuddle()
{
	using namespace WRSwampSys;
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(20000.f));
}

void AWRSwampPuddle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	using namespace WRSwampSys;
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampPuddle, Radius);
}

void AWRSwampPuddle::BeginPlay()
{
	using namespace WRSwampSys;
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	// Oxblood water spreading where it stood, thin leech ripples on it, a wet rim.
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	K.Disc(BleedWater(), FVector(0, 0, 3.f), FVector::UpVector, Radius * 2.f, 3.f);
	for (int32 i = 0; i < 7; ++i) K.Disc(Blood(0.6f), FRotator(0.f, i * 51.f, 0.f).Vector() * Radius * 0.85f + FVector(0, 0, 2.f), FVector::UpVector, Radius * 0.6f, 2.f);
	FWRBuildLook::Spawn(this, RootComponent, Blocks, SwsPeat, false);
	WRFx::Attach(RootComponent, TEXT("swamp.puddle"), NAME_None, Radius / 300.f);
}

// ================================================================================================ AWRSwampCandle

AWRSwampCandle::AWRSwampCandle()
{
	using namespace WRSwampSys;
	bReplicates = true;
	// Every machine eases it along the line from the replicated Along (smooth at any net rate, no movement stream).
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetNetCullDistanceSquared(FMath::Square(30000.f));
}

void AWRSwampCandle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	using namespace WRSwampSys;
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampCandle, bOut);
	DOREPLIFETIME(AWRSwampCandle, Along);
}

void AWRSwampCandle::BeginPlay()
{
	using namespace WRSwampSys;
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	Light = Light ? Light.Get() : WRSwampSys::Light(this, FVector(0, 0, 60.f), FLinearColor(0.55f, 0.8f, 0.6f), 0.f, 900.f);
	OnRep_Out();
}

void AWRSwampCandle::OnRep_Out()
{
	using namespace WRSwampSys;
	if (GetNetMode() == NM_DedicatedServer) return;
	if (Flame) { WRFx::Stop(Flame); Flame = nullptr; }
	if (!bOut) Flame = WRFx::Attach(RootComponent, TEXT("swamp.candle"), NAME_None, 1.f);
	else if (Shown > 0.f) WRSound::PlayAt(this, TEXT("swamp.candle.out"), GetActorLocation());   // it gutters out (not on joining)
	if (Light) Light->SetIntensity(bOut ? 0.f : 900.f);
	Shown = bOut ? 0.f : 1.f;
}

FVector AWRSwampCandle::PointAt(float InAlong)
{
	using namespace WRSwampSys;
	// The hidden line (swamp.json candle.pts, the planks 30 cm under the duckweed), then the reed island it ends on. The
	// flame rides a man's height over the water, so the walker below it is on the planks.
	const FWRSwampData& D = FWRSwampData::Get();
	TArray<FVector> Line = D.CandleLine;
	if (Line.Num() == 0 && !D.CandleFrom.IsZero()) Line.Add(D.CandleFrom);
	if (!D.CandleIsland.IsZero()) Line.Add(D.CandleIsland);
	if (Line.Num() == 0) return FVector::ZeroVector;
	const float A = FMath::Clamp(InAlong, 0.f, float(Line.Num() - 1));
	const int32 I = FMath::Min(FMath::FloorToInt(A), Line.Num() - 1);
	const int32 J = FMath::Min(I + 1, Line.Num() - 1);
	const FVector P = FMath::Lerp(Line[I], Line[J], A - I);
	const float Lift = H(TEXT("secrets.candleHeight"), 130.f);
	return FVector(P.X, P.Y, FMath::Max<float>(P.Z, D.SeaZ) + Lift);
}

void AWRSwampCandle::Tick(float DeltaSeconds)
{
	using namespace WRSwampSys;
	Super::Tick(DeltaSeconds);
	// Every machine eases it to where the server's Along puts it, bobbing a hand's breadth like a flame on still air.
	const float T = GetWorld()->GetTimeSeconds();
	const FVector Want = PointAt(Along) + FVector(FMath::Sin(T * 0.9f) * 12.f, FMath::Cos(T * 0.7f) * 12.f, FMath::Sin(T * 1.3f) * 9.f);
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), Want, DeltaSeconds, 2.5f));
	if (Light && !bOut) Light->SetIntensity(700.f + 250.f * FMath::Sin(T * 5.3f) * FMath::Sin(T * 1.7f));
}

// ================================================================================================ AWRSwampWays

AWRSwampWays::AWRSwampWays()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampWays::Build()
{
	// The two secret ways are the World track's sceneries (candle_path, fall_ledge): their planks and cut steps carry
	// their own collision, fitted to the cliff by traces (Swamp-finish NOTES, agreed). This actor stays as the named
	// place the secrets hang on (LedgeTop for the smith's cache), so nothing is built twice.
	if (Kind == TEXT("candle")) BuildCandleWalk();
	else if (Kind == TEXT("ledge")) BuildLedge();
}

void AWRSwampWays::BuildCandleWalk()
{
	// Drawn and made solid by AWRSwampScenery "candle_path" (planks 120 cm wide, top 30 cm under the duckweed, one box
	// per segment). The candle (AWRSwampCandle) and the reliquary on the island are this file's.
}

void AWRSwampWays::BuildLedge()
{
	// Drawn and made solid by AWRSwampScenery "fall_ledge" (steps traced to the cliff behind the third fall). The
	// smith's cache stands on its top landing (LedgeTop).
}

FVector AWRSwampWays::LedgeTop()
{
	// The top landing the World track builds: the Gullet site's point (its mouth), where the last cut step ends.
	const FWRSwampData::FSite* G = FWRSwampData::Get().FindSite(TEXT("gullet"));
	return G ? G->At : FVector::ZeroVector;
}

// ================================================================================================ AWRSwampWater

AWRSwampWater::AWRSwampWater(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaterVolume = true;
	bReplicates = false;   // every machine builds its own from the same layout
	SetHidden(true);
	// As the sea's volume (AWRWaterVolume): pawns only, so the swim code finds it and nothing else ever hits it.
	UBrushComponent* Shape = GetBrushComponent();
	Shape->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shape->SetCollisionObjectType(ECC_WorldDynamic);
	Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Shape->SetGenerateOverlapEvents(true);
	Shape->SetCanEverAffectNavigation(false);
}

void AWRSwampWater::Build(const TArray<FBox>& Boxes)
{
	FluidFriction = WRTune(TEXT("water.friction"), 2.f);
	UBrushComponent* Shape = GetBrushComponent();
	UBodySetup* Setup = NewObject<UBodySetup>(Shape);
	Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Setup->bNeverNeedsCookedCollisionData = true;
	const FTransform Inv = GetActorTransform().Inverse();
	for (const FBox& B : Boxes)
	{
		const FVector Size = B.GetSize();
		FKBoxElem Elem(Size.X, Size.Y, Size.Z);
		Elem.Center = Inv.TransformPosition(B.GetCenter());
		Setup->AggGeom.BoxElems.Add(Elem);
	}
	Setup->CreatePhysicsMeshes();
	Shape->BrushBodySetup = Setup;
	Shape->RecreatePhysicsState();
	Shape->UpdateBounds();
}

// ================================================================================================ the Coven's layout

namespace WRSwampSys
{
	const FWRSwampData::FRoom* CovenRoom(FName Id)
	{
		for (const FWRSwampData::FRoom& R : FWRSwampData::Get().CovenRooms) if (R.Id == Id) return &R;
		return nullptr;
	}

	/** swamp.json coven: an entry point (mouth, chimney, cellar), world cm (swamp coordinates, not lifted). */
	bool CovenEntry(FName Id, FVector& Out)
	{
		const FWRSwampData& D = FWRSwampData::Get();
		const TSharedPtr<FJsonObject>* Cv = nullptr;
		if (!D.Root.IsValid() || !D.Root->TryGetObjectField(TEXT("coven"), Cv)) return false;
		Out = WRSwamp::Point(*Cv, *Id.ToString());
		return !Out.IsZero();
	}

	/** swamp.json coven.lanes: the pairs of places a lane joins. */
	const TArray<TPair<FName, FName>>& CovenLanes()
	{
		static TArray<TPair<FName, FName>> Lanes;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			const TSharedPtr<FJsonObject>& Root = FWRSwampData::Get().Root;
			const TSharedPtr<FJsonObject>* Cv = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root.IsValid() && Root->TryGetObjectField(TEXT("coven"), Cv) && (*Cv)->TryGetArrayField(TEXT("lanes"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
					if (V->TryGetArray(Pair) && Pair->Num() == 2) Lanes.Add({ FName(*(*Pair)[0]->AsString()), FName(*(*Pair)[1]->AsString()) });
				}
			}
		}
		return Lanes;
	}

	/** The room an entry's lane leads into (mouth -> drowned_lane, cellar -> ringing_room). */
	FName EntryRoom(FName Entry)
	{
		for (const TPair<FName, FName>& L : CovenLanes())
		{
			if (L.Key == Entry && CovenRoom(L.Value)) return L.Value;
			if (L.Value == Entry && CovenRoom(L.Key)) return L.Key;
		}
		return NAME_None;
	}

	/** A place's centre in the swamp's own coordinates (a room's floor centre, or an entry). */
	bool CovenPlace(FName Id, FVector& Out)
	{
		if (const FWRSwampData::FRoom* R = CovenRoom(Id)) { Out = R->At; return true; }
		return CovenEntry(Id, Out);
	}

	/** The Birthing Pools' three warm pools: centre (Coven space, at the floor) and radius (NOTES: agreed with World). */
	bool BirthPool(int32 Index, FVector& OutCentre, float& OutRadius)
	{
		const FWRSwampData::FRoom* R = CovenRoom(TEXT("birthing_pools"));
		if (!R || Index < 0 || Index > 2) return false;
		static const FVector2D Off[3] = { FVector2D(-0.25f, -0.20f), FVector2D(0.22f, -0.18f), FVector2D(0.f, 0.24f) };
		OutCentre = AWRSwampCoven::ToCoven(R->At) + FVector(Off[Index].X * R->Size.X, Off[Index].Y * R->Size.Y, 0.f);
		OutRadius = 0.17f * FMath::Min(R->Size.X, R->Size.Y);
		return true;
	}

	float GarDepth() { return H(TEXT("coven.garDepth"), 500.f); }
	float GarLedge() { return WRTune(TEXT("swampWorld.covenGarLedgeDepth"), 400.f); }
	float BirthWater() { return H(TEXT("coven.birthWater"), 35.f); }
	float LaneWater() { return H(TEXT("coven.laneWater"), 50.f); }

	/** A room's box in Coven space holds Pos (2D, and Z within the room's height plus a margin under the floor). */
	bool InRoom(FName Id, const FVector& Pos, float Grow = 0.f)
	{
		FVector Ctr, Half;
		if (!AWRSwampCoven::RoomBox(Id, Ctr, Half)) return false;
		return FMath::Abs(Pos.X - Ctr.X) <= Half.X + Grow && FMath::Abs(Pos.Y - Ctr.Y) <= Half.Y + Grow && Pos.Z >= Ctr.Z - Half.Z - 200.f && Pos.Z <= Ctr.Z + Half.Z + 100.f;
	}
}

// ================================================================================================ AWRSwampCoven

AWRSwampCoven::AWRSwampCoven()
{
	bReplicates = true;
	// One actor; whoever comes down (ladder, dive, flue) must find it built before they land, wherever they were.
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(2.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSwampCoven::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSwampCoven, ChimedAt);
	DOREPLIFETIME(AWRSwampCoven, BellAt);
}

FVector AWRSwampCoven::ToCoven(const FVector& SwampPoint)
{
	return SwampPoint + FVector(0.f, 0.f, WRSwamp::CovenLift());
}

FVector AWRSwampCoven::Origin()
{
	return ToCoven(FWRSwampData::Get().DeepPool);
}

bool AWRSwampCoven::RoomBox(FName Id, FVector& OutCentre, FVector& OutHalf)
{
	using namespace WRSwampSys;
	const FWRSwampData::FRoom* R = CovenRoom(Id);
	if (!R) return false;
	OutHalf = R->Size * 0.5f;
	OutCentre = ToCoven(R->At) + FVector(0.f, 0.f, OutHalf.Z);
	return true;
}

bool AWRSwampCoven::ChapelPool(FVector& OutCentre, FVector& OutHalf, float& OutSurface)
{
	using namespace WRSwampSys;
	FVector Ctr, Half;
	if (!RoomBox(TEXT("gar_pool"), Ctr, Half)) return false;
	// The flooded chapel: the World track's ledge runs along the -Y wall (its depth swampWorld.covenGarLedgeDepth); the
	// water fills the rest of the floor from the room's floor up garDepth.
	const float Floor = Ctr.Z - Half.Z;
	const float Ledge = FMath::Min(GarLedge(), Half.Y * 1.2f);
	OutSurface = Floor + GarDepth();
	OutHalf = FVector(Half.X, Half.Y - Ledge * 0.5f, GarDepth() * 0.5f);
	OutCentre = FVector(Ctr.X, Ctr.Y + Ledge * 0.5f, Floor + GarDepth() * 0.5f);
	return true;
}

bool AWRSwampCoven::IsBleedPool(const FVector& Pos)
{
	using namespace WRSwampSys;
	if (!WRSwamp::InCoven(Pos)) return false;
	for (int32 i = 0; i < 3; ++i)
	{
		FVector PC;
		float PR = 0.f;
		if (!BirthPool(i, PC, PR)) return false;
		if (FVector::Dist2D(Pos, PC) < PR && Pos.Z > PC.Z - 60.f && Pos.Z < PC.Z + BirthWater() + 90.f) return true;
	}
	return false;
}

float AWRSwampCoven::WalkFloor(FName Room)
{
	using namespace WRSwampSys;
	const FWRSwampData::FRoom* R = CovenRoom(Room);
	if (!R) return WRSwamp::CovenLift();
	const float Floor = ToCoven(R->At).Z;
	return Room == TEXT("gar_pool") ? Floor + GarDepth() + 40.f : Floor;
}

bool AWRSwampCoven::LaneGap(FName Room, FName Other, FVector& OutAt, float& OutYaw)
{
	using namespace WRSwampSys;
	FVector Ctr, Half, O;
	if (!RoomBox(Room, Ctr, Half) || !CovenPlace(Other, O)) return false;
	FVector Dir = ToCoven(O) - Ctr;
	Dir.Z = 0.f;
	if (!Dir.Normalize()) Dir = FVector::ForwardVector;
	// Where the line from the room's centre toward the other end leaves the room's box.
	const float Tx = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Dir.X) : 1.0e9f;
	const float Ty = FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Dir.Y) : 1.0e9f;
	const FVector Edge = Ctr + Dir * FMath::Min(Tx, Ty);
	OutAt = FVector(Edge.X, Edge.Y, WalkFloor(Room));
	OutYaw = (-Dir).Rotation().Yaw;
	return true;
}

bool AWRSwampCoven::RoomFloorAt(FName Room, FVector& OutAt)
{
	using namespace WRSwampSys;
	if (const FWRSwampData::FRoom* R = CovenRoom(Room))
	{
		const FVector P = ToCoven(R->At);
		OutAt = FVector(P.X, P.Y, WalkFloor(Room));
		return true;
	}
	FVector E;
	if (!CovenEntry(Room, E)) return false;
	const FName Into = EntryRoom(Room);
	const FVector P = ToCoven(E);
	OutAt = FVector(P.X, P.Y, Into.IsNone() ? P.Z : WalkFloor(Into));
	return true;
}

void AWRSwampCoven::BeginPlay()
{
	Super::BeginPlay();
	Build();
	if (HasAuthority()) SpawnServerThings();
}

void AWRSwampCoven::Build()
{
	using namespace WRSwampSys;
	if (bBuilt) return;
	bBuilt = true;
	// The chapel's water is a swim volume on every machine (the server moves swimmers too); the rest is look.
	FVector PoolC, PoolHalf;
	float Surface = 0.f;
	if (ChapelPool(PoolC, PoolHalf, Surface))
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.Owner = this;
		if (AWRSwampWater* W = GetWorld()->SpawnActor<AWRSwampWater>(AWRSwampWater::StaticClass(), FTransform(PoolC), P))
		{
			W->Build({ FBox(PoolC - PoolHalf, PoolC + PoolHalf) });
			Water.Add(W);
		}
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	LookRoot = NewObject<USceneComponent>(this);
	LookRoot->SetupAttachment(RootComponent);
	LookRoot->RegisterComponent();
	TArray<FWRLookBlock> Blocks;
	FKit K(GetActorTransform(), Blocks);
	const TArray<FWRSwampData::FRoom>& Rooms = FWRSwampData::Get().CovenRooms;
	for (int32 i = 0; i < Rooms.Num(); ++i) BuildRoom(i, K);
	FWRBuildLook::Spawn(this, LookRoot, Blocks, SwsPeat, false);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP coven built: %d rooms, %d chime strings, lift %.0f"), Rooms.Num(), ChimeGroups.Num(), WRSwamp::CovenLift());
}

void AWRSwampCoven::BuildRoom(int32 Index, WRLookKit::FKit& K)
{
	using namespace WRSwampSys;
	const TArray<FWRSwampData::FRoom>& Rooms = FWRSwampData::Get().CovenRooms;
	if (!Rooms.IsValidIndex(Index)) return;
	const FWRSwampData::FRoom& R = Rooms[Index];
	FVector Ctr, Half;
	if (!RoomBox(R.Id, Ctr, Half)) return;
	// Blocks are in this actor's space (it stands at Origin, unrotated).
	const FVector Me = GetActorLocation();
	const float Floor = Ctr.Z - Half.Z;
	const auto L = [&Me](const FVector& WorldAt) { return WorldAt - Me; };
	if (R.Id == TEXT("drowned_lane"))
	{
		// Black water to the knee down the whole street, sharp and still; rings where the hanging roots touch it.
		K.Add(Murk(), FVector(Half.X * 2.f, Half.Y * 2.f, 4.f), L(FVector(Ctr.X, Ctr.Y, Floor + LaneWater() - 2.f)));
		for (int32 i = 0; i < 9; ++i)
		{
			const FVector At(Ctr.X - Half.X * 0.85f + i * Half.X * 0.21f, Ctr.Y + (i % 2 ? 1.f : -1.f) * Half.Y * (0.25f + 0.08f * (i % 3)), Floor + LaneWater());
			K.Disc(FSurf::Glaze(FLinearColor(0.02f, 0.025f, 0.02f), 0.05f), L(At), FVector::UpVector, 60.f + 15.f * (i % 3), 1.f);
		}
		return;
	}
	if (R.Id == TEXT("birthing_pools"))
	{
		// Three warm pools of oxblood water, filmed like old blood, boiling with the young (the World track's kerbs hold
		// them). The surface sits a hand under the kerb's top; the bottom is clotted dark.
		for (int32 i = 0; i < 3; ++i)
		{
			FVector PC;
			float PR = 0.f;
			if (!BirthPool(i, PC, PR)) continue;
			K.Disc(BleedWater(), L(PC + FVector(0, 0, BirthWater() - 2.f)), FVector::UpVector, PR * 2.f, 4.f);
			K.Disc(Blood(0.5f), L(PC + FVector(0, 0, 3.f)), FVector::UpVector, PR * 1.9f, 6.f);
			for (int32 s = 0; s < 6; ++s)
			{
				const FVector Scum = PC + FRotator(0.f, 60.f * s + 20.f * i, 0.f).Vector() * PR * (0.35f + 0.1f * (s % 3)) + FVector(0, 0, BirthWater());
				K.Disc(FSurf::Glaze(SwsOxblood * 0.3f, 0.6f), L(Scum), FVector::UpVector, 50.f + 20.f * (s % 2), 1.5f);
			}
			if (USceneComponent* Fx = WRFx::Attach(LookRoot, TEXT("swamp.puddle"), NAME_None, PR / 300.f)) Fx->SetWorldLocation(PC + FVector(0, 0, BirthWater()));
		}
		return;
	}
	if (R.Id == TEXT("gar_pool"))
	{
		// The flooded chapel: deep, black, clear water under the ledge, slow rings where the Old Gar rolls.
		FVector PC, PH;
		float Surface = 0.f;
		if (ChapelPool(PC, PH, Surface))
		{
			K.Add(Murk(), FVector(PH.X * 2.f, PH.Y * 2.f, 4.f), L(FVector(PC.X, PC.Y, Surface - 2.f)));
			for (int32 s = 0; s < 4; ++s)
			{
				K.Disc(FSurf::Glaze(FLinearColor(0.012f, 0.014f, 0.012f), 0.02f), L(FVector(PC.X - PH.X * 0.5f + s * PH.X * 0.33f, PC.Y + PH.Y * 0.2f * (s % 2 ? 1 : -1), Surface + 0.5f)), FVector::UpVector, 120.f + 40.f * s, 0.5f);
			}
		}
		return;
	}
	if (R.Id == TEXT("ringing_room"))
	{
		// Bone chimes on every beam (the World track's four beams along X): strings of finger-bones, vertebrae and hollow
		// long-bones on gut cord, each string its own swinging frame so the room sways and clacks when walked.
		const float HangZ = Floor + Half.Z * 2.f - 52.f;
		for (const float Yf : { -0.3f, -0.1f, 0.1f, 0.3f })
		{
			const int32 Strings = FMath::Max(4, FMath::RoundToInt(Half.X * 2.f / 110.f));
			for (int32 s = 0; s < Strings; ++s)
			{
				const float X = Ctr.X - Half.X + (s + 0.5f) * (Half.X * 2.f / Strings);
				const FVector Hook(X, Ctr.Y + Yf * Half.Y * 2.f, HangZ);
				USceneComponent* G = NewObject<USceneComponent>(this);
				G->SetupAttachment(LookRoot);
				G->SetWorldLocation(Hook);
				G->RegisterComponent();
				TArray<FWRLookBlock> Gb;
				FKit Gk(FTransform(Hook), Gb);
				const float Len = 60.f + 35.f * FMath::Frac(X * 0.013f + Yf * 7.f);
				Gk.Rod(Rope(), FVector::ZeroVector, FVector(0, 0, -Len - 30.f), 0.8f);
				for (int32 b = 0; b < 4; ++b)
				{
					const float Z = -14.f - b * (Len / 4.f);
					if ((s + b) % 3 == 0) Gk.Add(SwsBone(), FVector(5.f, 5.f, 22.f), FVector(0, 0, Z - 11.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // a long-bone tube
					else if ((s + b) % 3 == 1) Gk.Add(SwsBone(), FVector(9.f, 9.f, 5.f), FVector(0, 0, Z), FRotator::ZeroRotator, EWRLookMesh::Cylinder);     // a vertebra
					else Gk.Rod(SwsBone(), FVector(-6.f, 0, Z + 4.f), FVector(6.f, 0, Z - 4.f), 2.2f);                                                        // finger-bones crossed
				}
				Gk.Add(SwsBone(), FVector(14.f, 10.f, 12.f), FVector(0, 0, -Len - 36.f), FRotator(0.f, s * 37.f, 0.f), EWRLookMesh::Cylinder);                // a jaw at the end
				FWRBuildLook::Spawn(this, G, Gb, SwsPeat, false);
				ChimeGroups.Add(G);
			}
		}
		return;
	}
}

void AWRSwampCoven::SpawnServerThings()
{
	using namespace WRSwampSys;
	if (bServerSpawned) return;
	UWRSwampSystems* Sys = UWRSwampSystems::Get(this);
	if (!Sys) return;
	bServerSpawned = true;
	Sys->Coven = this;
	const FWRSwampData& D = FWRSwampData::Get();
	FVector At, Ctr, Half;
	float Yaw = 0.f;

	// The cellar shaft's foot (M-S2): the iron ladder up to the Walking House.
	if (RoomFloorAt(TEXT("cellar"), At))
	{
		FVector Up = FVector::ZeroVector;
		float UpYaw = 0.f;
		if (const FWRSwampData::FSite* Hs = D.FindSite(TEXT("walking_house")))
		{
			const FRotator R(0.f, Hs->Yaw, 0.f);
			Up = Hs->At + R.RotateVector(FVector(40.f, -90.f, WRTune(TEXT("swampWorld.houseFloor"), 1500.f)));
			UpYaw = Hs->Yaw;
		}
		Sys->SpawnThing(TEXT("ladder"), At, 0.f, TEXT("The cellar ladder"), TEXT("Climb to the Walking House"), 260.f, [&](AWRSwampThing& T) { T.Dest = Up; T.DestYaw = UpYaw; });
	}
	// The dive's mouth: back up through the gap in the mat into the Deep Pool.
	if (RoomFloorAt(TEXT("mouth"), At))
	{
		const FVector Up(D.DeepPool.X, D.DeepPool.Y, D.SeaZ - 60.f);
		Sys->SpawnThing(TEXT("dive_hole"), At, 0.f, TEXT("The way up through the water"), TEXT("Dive up"), 420.f, [&](AWRSwampThing& T) { T.Dest = Up; T.DestYaw = 0.f; });
	}
	// The chimney's foot: shut with the surface one until a Bogsnout digs it out.
	if (RoomFloorAt(TEXT("chimney"), At))
	{
		Sys->ChimneyIn = Sys->SpawnThing(TEXT("chimney"), At, 0.f, TEXT("The old flue"), TEXT("Climb the flue"), 300.f, [&](AWRSwampThing& T)
		{
			T.bShut = Sys->ChimneyFallsAtHours < 0.0;
			T.Dest = D.Chimney + FVector(260.f, 0.f, 60.f);
			T.DestYaw = 0.f;
		});
	}
	// The Tithe Hall's lore: a tithe bowl set apart from the forty, still dark inside.
	if (RoomBox(TEXT("tithe_hall"), Ctr, Half))
	{
		Sys->SpawnThing(TEXT("cache"), FVector(Ctr.X + Half.X * 0.55f, Ctr.Y, Ctr.Z - Half.Z), 180.f, TEXT("A bowl set apart"), TEXT("Take"), 260.f, [&](AWRSwampThing& T)
		{
			T.Items.Add(TEXT("lore_tithe_bowl"), 1);
			T.Lore = TEXT("Bronze, green at the lip, black in the bottom. The forty others were stacked; this one was left full.");
		});
	}
	// The Bog-Body Gallery's empty niche: a braided cord left where a body should sit.
	if (RoomBox(TEXT("bog_gallery"), Ctr, Half))
	{
		Sys->SpawnThing(TEXT("cache"), FVector(Ctr.X + Half.X * 0.9f, Ctr.Y - Half.Y * 0.6f, Ctr.Z - Half.Z), 90.f, TEXT("The empty niche"), TEXT("Take the cord"), 240.f, [&](AWRSwampThing& T)
		{
			T.Items.Add(TEXT("lore_braided_cord"), 1);
			T.Lore = TEXT("Three strands, plaited, the knot tied loose at the front. The niche it lies in is the only one with nobody in it.");
		});
	}
	// The Birthing Pools: Vharra's egg cases on the walls (steal-an-egg; tearing one wakes the pools).
	if (RoomBox(TEXT("birthing_pools"), Ctr, Half))
	{
		for (int32 e = 0; e < 6; ++e)
		{
			const bool bNorth = e < 3;
			const FVector Wall(Ctr.X - Half.X * 0.6f + (e % 3) * Half.X * 0.6f, Ctr.Y + (bNorth ? Half.Y - 35.f : -Half.Y + 35.f), Ctr.Z - Half.Z + 140.f + 30.f * (e % 2));
			Sys->SpawnThing(TEXT("egg"), Wall, bNorth ? -90.f : 90.f, TEXT("A Gorgeleech egg case"), TEXT("Cut it loose"), 240.f, [](AWRSwampThing&) {});
		}
	}
	// The Witch's Library: the one leaf still readable (the Recipe of the tithe: the Hex-ward charm).
	if (RoomBox(TEXT("library"), Ctr, Half))
	{
		Sys->SpawnThing(TEXT("lore"), FVector(Ctr.X, Ctr.Y + Half.Y * 0.3f, Ctr.Z - Half.Z), 0.f, TEXT("A bark book on a lectern"), TEXT("Read"), 240.f, [&](AWRSwampThing& T)
		{
			T.Items.Add(TEXT("lore_tithe_recipe"), 1);
			T.Lore = TEXT("Bloodroot, crow feather, bone, bound with a little of your own. The charm turns a hex back the way it came.");
		});
	}
	// The Bell Pit: the Coven bell, once a game day.
	if (RoomBox(TEXT("bell_pit"), Ctr, Half))
	{
		Sys->SpawnThing(TEXT("bell"), FVector(Ctr.X, Ctr.Y, Ctr.Z - Half.Z + 20.f), 25.f, TEXT("The Coven bell"), TEXT("Ring it"), 380.f, [](AWRSwampThing&) {});
	}
	// The Sealed Reliquary (M-S5): a stone door in the lane from the Ringing Room, the cache behind it. The tide opens both.
	if (LaneGap(TEXT("reliquary"), TEXT("ringing_room"), At, Yaw) && RoomBox(TEXT("reliquary"), Ctr, Half))
	{
		const AWRSwampState* St = AWRSwampState::Get(this);
		const bool bOpen = St && St->bReliquaryOpen;
		AWRSwampThing* Door = Sys->SpawnThing(TEXT("reliquary_door"), At, Yaw, TEXT("A sealed stone door"), TEXT("Push"), 320.f, [&](AWRSwampThing& T) { T.bShut = !bOpen; });
		AWRSwampThing* Box = Sys->SpawnThing(TEXT("reliquary"), FVector(Ctr.X, Ctr.Y, Ctr.Z - Half.Z), Yaw + 180.f, TEXT("The Sealed Reliquary"), TEXT("Open"), 280.f, [&](AWRSwampThing& T)
		{
			T.bShut = !bOpen;
			T.Items = { { TEXT("iron_ore"), 6 }, { TEXT("gem_ruby"), 1 }, { TEXT("upgrade_shard"), 2 }, { TEXT("lore_first_tooth"), 1 } };
			T.Lore = TEXT("A tooth as long as a forearm on a stone shelf, and the tithe they kept beside it.");
			T.RefillHours = H(TEXT("coven.reliquaryRefillHours"), 72.f);
		});
		if (Door) Sys->ReliquaryParts.Add(Door);
		if (Box) { Sys->ReliquaryParts.Add(Box); Sys->Reliquary = Box; }
	}
	// The Weeping Gate: one way, out into the Throat's bottom terrace.
	if (RoomBox(TEXT("weeping_gate"), Ctr, Half))
	{
		FVector Out = FVector::ZeroVector;
		FRotator Face = FRotator::ZeroRotator;
		if (!FWRSwampBoss::WeepingGatePoint(Out, Face))
		{
			if (const FWRSwampData::FSite* Sr = D.FindSite(TEXT("throat_stair"))) Out = WRSwamp::Point(Sr->Row, TEXT("foot"));
			if (const FWRSwampData::FSite* Th = D.FindSite(TEXT("mothers_throat"))) Face = (Th->At - Out).Rotation();
		}
		FVector In = FVector(Ctr.X, Ctr.Y, Ctr.Z - Half.Z);
		float InYaw = 0.f;
		LaneGap(TEXT("weeping_gate"), TEXT("bell_pit"), In, InYaw);
		// The leaf stands across the far wall from the lane you come in by, facing you as you enter.
		const FVector Away = (FVector(Ctr.X, Ctr.Y, 0.f) - FVector(In.X, In.Y, 0.f)).GetSafeNormal();
		const FVector Far = FVector(Ctr.X, Ctr.Y, Ctr.Z - Half.Z) + Away * (FMath::Abs(Away.X) > FMath::Abs(Away.Y) ? Half.X : Half.Y) * 0.9f;
		if (!Out.IsZero())
		{
			Sys->SpawnThing(TEXT("gate"), Far, Away.Rotation().Yaw + 180.f, TEXT("The Weeping Gate"), TEXT("Open it"), 360.f, [&](AWRSwampThing& T) { T.Dest = Out; T.DestYaw = Face.Yaw; });
		}
	}

	// Who lives here (DC 2.5): 3 Gorgeleech in the Drowned Lane; 6 young and 2 adults in the Birthing Pools (the adults sleep
	// until the chimes wake them); Lanterntoads lighting the rooms green-gold.
	const auto Slot = [this](FName Id, const FVector& Where, bool bBrood, bool bDweller)
	{
		FSlot S;
		S.Id = Id;
		S.At = Where;
		S.bBrood = bBrood;
		S.bDweller = bDweller;
		Slots.Add(S);
	};
	if (RoomBox(TEXT("drowned_lane"), Ctr, Half))
	{
		for (int32 i = 0; i < 3; ++i) Slot(TEXT("gorgeleech"), FVector(Ctr.X - Half.X * 0.6f + i * Half.X * 0.6f, Ctr.Y + (i % 2 ? 0.3f : -0.3f) * Half.Y, Ctr.Z - Half.Z), false, true);
		for (int32 i = 0; i < 3; ++i) Slot(TEXT("lanterntoad"), FVector(Ctr.X - Half.X * 0.8f + i * Half.X * 0.8f, Ctr.Y + Half.Y * 0.7f, Ctr.Z - Half.Z), false, false);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		FVector PC;
		float PR = 0.f;
		if (!BirthPool(i, PC, PR)) continue;
		for (int32 y = 0; y < 2; ++y) Slot(TEXT("latch_leech"), PC + FRotator(0.f, 180.f * y + 40.f * i, 0.f).Vector() * PR * 0.4f, true, true);
		if (i < 2) Slot(TEXT("gorgeleech"), PC + FRotator(0.f, 90.f + 60.f * i, 0.f).Vector() * (PR + 150.f), true, true);
	}
	int32 Toad = 0;
	for (const FName Room : { FName(TEXT("tithe_hall")), FName(TEXT("tithe_hall")), FName(TEXT("bog_gallery")), FName(TEXT("library")), FName(TEXT("bell_pit")) })
	{
		++Toad;
		if (RoomBox(Room, Ctr, Half)) Slot(TEXT("lanterntoad"), FVector(Ctr.X + (Toad % 2 ? 0.5f : -0.5f) * Half.X, Ctr.Y + (Toad % 3 - 1) * 0.5f * Half.Y, Ctr.Z - Half.Z), false, false);
	}
	CreatureClock = 0.f;
	StepCreatures();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP coven placed: %d creature slots"), Slots.Num());
}

void AWRSwampCoven::StepCreatures()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const double HoursNow = Hours(this);
	// Respawn only while no survivor is down here (nothing pops in behind you), coven.respawnHours after a death.
	bool bAnyone = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		bAnyone |= P && WRSwamp::InCoven(P->GetActorLocation());
	}
	const float Respawn = H(TEXT("coven.respawnHours"), 12.f);
	for (FSlot& S : Slots)
	{
		AWRCreature* Body = S.Body.Get();
		if (Body && Body->IsAlive()) continue;
		// DeadAt: -1 never spawned, -2 alive when last seen, -1000 the bell called it back now, else the hour it died.
		const bool bCalled = S.DeadAt <= -500.0;
		const bool bNever = S.DeadAt == -1.0;
		if (!bCalled && !bNever)
		{
			if (S.DeadAt < 0.0) S.DeadAt = HoursNow;
			if (bAnyone || HoursNow - S.DeadAt < Respawn) continue;
		}
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(S.Id);
		if (!Def) continue;
		AWRCreature* Cr = AWRCreature::SpawnCreature(World, S.Id, S.At + FVector(0.f, 0.f, Def->HalfHeight + 40.f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
		if (!Cr) continue;
		Cr->SetHome(S.At);
		if (S.bBrood && S.Id == TEXT("gorgeleech")) Cr->SetAsleep(true);
		S.Body = Cr;
		S.DeadAt = -2.0;
		if (S.bBrood) Brood.Add(Cr);
		if (S.bDweller) Dwellers.Add(Cr);
	}
	Brood.RemoveAll([](const TWeakObjectPtr<AWRCreature>& W) { return !W.IsValid() || !W->IsAlive(); });
	Dwellers.RemoveAll([](const TWeakObjectPtr<AWRCreature>& W) { return !W.IsValid() || !W->IsAlive(); });
}

void AWRSwampCoven::RingChimes(AWRCharacter* By)
{
	using namespace WRSwampSys;
	const float T = Now(this);
	if (!HasAuthority() || T < ChimedAt + H(TEXT("coven.chimeGap"), 4.f)) return;
	ChimedAt = T;
	ForceNetUpdate();
	OnRep_Chimes();
	// The noise wakes the pools' adults (DC 2.5, the sneak check failed): up out of the warm water, toward whoever rang.
	int32 Woke = 0;
	for (const TWeakObjectPtr<AWRCreature>& W : Brood)
	{
		AWRCreature* Cr = W.Get();
		if (!Cr || !Cr->IsAlive()) continue;
		if (Cr->IsAsleep()) ++Woke;
		Cr->SetAsleep(false);
		if (By && By->IsAlive() && FVector::Dist(Cr->GetActorLocation(), By->GetActorLocation()) < H(TEXT("coven.chimeWake"), 2500.f) * 2.f) Cr->SetAggroTarget(By);
	}
	if (Woke > 0) Tell(By, TEXT("The chimes ring through the rooms. Something in the warm pools turns over"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMP coven chimes rung by %s: %d woke"), *GetNameSafe(By), Woke);
}

FString AWRSwampCoven::RingBell(AWRPlayerController* PC)
{
	using namespace WRSwampSys;
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!HasAuthority() || !Me) return TEXT("The bell is silent");
	const double HoursNow = Hours(this);
	if (BellReadyAtHours >= 0.0 && HoursNow < BellReadyAtHours) return TEXT("The bronze is still humming from the last time. Tomorrow");
	BellReadyAtHours = HoursNow + H(TEXT("coven.bellHours"), 24.f);
	BellAt = Now(this);
	ForceNetUpdate();
	OnRep_Bell();
	// Everything that lives in the Coven comes: the dead dwellers rise out of the silt first (a farming spot, at your risk).
	for (FSlot& S : Slots) if (S.bDweller && (!S.Body.IsValid() || !S.Body->IsAlive())) S.DeadAt = -1000.0;
	StepCreatures();
	int32 Called = 0;
	for (const TWeakObjectPtr<AWRCreature>& W : Dwellers)
	{
		AWRCreature* Cr = W.Get();
		if (!Cr || !Cr->IsAlive()) continue;
		Cr->SetAsleep(false);
		Cr->SetHome(Me->GetActorLocation());
		Cr->SetAggroTarget(Me);
		++Called;
	}
	UE_LOG(LogWildreach, Log, TEXT("SWAMP coven bell rung by %s: %d called"), *Me->GetName(), Called);
	return FString::Printf(TEXT("The note goes down through the water and the stone. %d things in the Coven heard it"), Called);
}

void AWRSwampCoven::OnRep_Chimes()
{
	using namespace WRSwampSys;
	if (GetNetMode() == NM_DedicatedServer || ChimedAt <= 0.f || Now(this) - ChimedAt > 2.f || ChimedAt == ChimeShown) return;
	ChimeShown = ChimedAt;
	ChimeSwing = 1.f;
	FVector Ctr, Half;
	if (RoomBox(TEXT("ringing_room"), Ctr, Half)) WRSound::PlayAt(this, TEXT("swamp.chimes"), Ctr);
}

void AWRSwampCoven::OnRep_Bell()
{
	using namespace WRSwampSys;
	if (GetNetMode() == NM_DedicatedServer || BellAt <= 0.f || Now(this) - BellAt > 3.f || BellAt == BellShown) return;
	BellShown = BellAt;
	FVector Ctr, Half;
	if (!RoomBox(TEXT("bell_pit"), Ctr, Half)) return;
	WRSound::PlayAt(this, TEXT("swamp.bell"), Ctr);
	WRFx::SpawnAt(this, TEXT("swamp.leech.boil"), Ctr - FVector(0, 0, Half.Z - 10.f), FRotator::ZeroRotator, 2.f);
	// The note is felt: a low shake for anyone down here, and the chimes shiver in sympathy.
	WRFx::Shake(this, 0.08f, Ctr, 6000.f);
	ChimeSwing = FMath::Max(ChimeSwing, 0.5f);
}

void AWRSwampCoven::Tick(float DeltaSeconds)
{
	using namespace WRSwampSys;
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		CreatureClock += DeltaSeconds;
		if (CreatureClock >= 5.f) { CreatureClock = 0.f; StepCreatures(); }
		// The Ringing Room: walking through quickly rings the chimes (a crouch or a careful walk doesn't).
		ChimeCheck -= DeltaSeconds;
		if (ChimeCheck <= 0.f)
		{
			ChimeCheck = 0.25f;
			UWRSwampSystems* Sys = UWRSwampSystems::Get(this);
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AWRCharacter* P = It->Get() ? Cast<AWRCharacter>(It->Get()->GetPawn()) : nullptr;
				if (!P || !P->IsAlive() || !InRoom(TEXT("ringing_room"), P->GetActorLocation())) continue;
				if (P->bIsCrouched || P->GetVelocity().Size2D() < H(TEXT("coven.chimeSpeed"), 300.f)) continue;
				UWRSwampSystems::FWader* W = Sys ? Sys->Waders.Find(P) : nullptr;
				const double T = Now(this);
				if (W && T < W->NextChimeAt) continue;
				if (W) W->NextChimeAt = T + H(TEXT("coven.chimeGap"), 4.f);
				RingChimes(P);
			}
		}
	}
	if (GetNetMode() == NM_DedicatedServer || ChimeGroups.Num() == 0) return;
	// The chimes: a draught keeps them barely moving; a ring sets them swinging and clacking, dying away over seconds.
	ChimeTime += DeltaSeconds;
	ChimeSwing = FMath::Max(0.f, ChimeSwing - DeltaSeconds * 0.22f);
	const float Amp = 2.f + 16.f * ChimeSwing;
	for (int32 i = 0; i < ChimeGroups.Num(); ++i)
	{
		if (USceneComponent* G = ChimeGroups[i])
		{
			const float Ph = i * 1.37f;
			G->SetRelativeRotation(FRotator(Amp * FMath::Sin(ChimeTime * (2.1f + 0.13f * (i % 5)) + Ph), 0.f, Amp * 0.7f * FMath::Sin(ChimeTime * (1.7f + 0.11f * (i % 7)) + Ph * 0.6f)));
		}
	}
}

// ================================================================================================ AWRSwampThing (state)

void AWRSwampThing::SetState(bool bInSpent, bool bInShut, bool bInConcealed)
{
	if (!HasAuthority() || (bSpent == bInSpent && bShut == bInShut && bConcealed == bInConcealed)) return;
	bSpent = bInSpent;
	bShut = bInShut;
	bConcealed = bInConcealed;
	ForceNetUpdate();
	OnRep_State();   // the server's own block (and a listen server's look)
}

// ================================================================================================ the swamp's helpers (systems)

namespace WRSwampSys
{
	/** The water as it touches this survivor's feet, and whether they're in it (feet under its surface; a puddle or a
	 *  Coven pool always). One rule for the server's tick and the owner's tells. */
	EWRSwampWater WaterAtFeet(const UObject* Ctx, const ACharacter* C, bool& bOutWet)
	{
		bOutWet = false;
		const FVector F = Feet(C);
		const FWRSwampData& D = FWRSwampData::Get();
		const EWRSwampWater K = WRSwamp::HazardWaterAt(Ctx, F);
		if (K == EWRSwampWater::Dry) return K;
		if (AWRSwampCoven::IsBleedPool(F)) { bOutWet = true; return K; }
		if (WRSwamp::InCoven(F) || !D.InFrame(F)) { bOutWet = K == EWRSwampWater::Bleedwater; return K; }   // the lane in the tide, a puddle
		if (K == EWRSwampWater::Quag) { bOutWet = F.Z < D.SeaZ + H(TEXT("quag.depth"), 60.f); return K; }
		bOutWet = WRSwamp::IsWet(F + FVector(0.f, 0.f, 15.f)) || UWRWaterSubsystem::IsInWater(C);
		return K;
	}

	/** The Red Tide's leech boil runs in this water now (the tide's bleedwater of M3-M5 and the pools there). */
	bool InBoil(const UObject* Ctx, const FVector& F)
	{
		if (!WRSwamp::IsTide(Ctx)) return false;
		const FWRSwampData& D = FWRSwampData::Get();
		if (!D.InFrame(F) || WRSwamp::InCoven(F)) return false;
		// M3-M5 (redTide.subZones); M1/M2 are brackish and salt kills leeches.
		const FString Zones = FWRTuning::Get().Str(TEXT("redTide.subZones"), TEXT("345"));
		return Zones.Contains(FString::Chr(D.ZoneAt(F)));
	}

	void SetTag(AWRCharacterBase* C, const FGameplayTag& Tag, bool bOn)
	{
		if (UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetLooseGameplayTagCount(Tag, bOn ? 1 : 0, EGameplayTagReplicationState::TagOnly);
		}
	}

	AWRPlayerController* ControllerNamed(UWorld* World, const FString& Name)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Name) return PC;
		}
		return nullptr;
	}

	/** The candle's hidden line (its points, then the island): the closest point's index along it and the distance off it. */
	float OffCandleLine(const FVector& At, float& OutAlong)
	{
		const FWRSwampData& D = FWRSwampData::Get();
		TArray<FVector> Line = D.CandleLine;
		if (!D.CandleIsland.IsZero()) Line.Add(D.CandleIsland);
		OutAlong = 0.f;
		float Best = 1.0e12f;
		for (int32 i = 0; i + 1 < Line.Num(); ++i)
		{
			const FVector A(Line[i].X, Line[i].Y, 0.f), B(Line[i + 1].X, Line[i + 1].Y, 0.f), P(At.X, At.Y, 0.f);
			const FVector AB = B - A;
			const float T = FMath::Clamp(FVector::DotProduct(P - A, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
			const float Dd = FVector::Dist(P, A + AB * T);
			if (Dd < Best) { Best = Dd; OutAlong = i + T; }
		}
		return Best;
	}

	int32 CandleLast()
	{
		const FWRSwampData& D = FWRSwampData::Get();
		return FMath::Max(0, D.CandleLine.Num() + (D.CandleIsland.IsZero() ? 0 : 1) - 1);
	}

	/** world.json masters row by id. */
	TSharedPtr<FJsonObject> MasterRow(FName Id)
	{
		const TSharedPtr<FJsonObject> Root = WRWorld::Data();
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("masters"), List)) return nullptr;
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FString Id2;
			if (O.IsValid() && O->TryGetStringField(TEXT("id"), Id2) && FName(*Id2) == Id) return O;
		}
		return nullptr;
	}
}

// ================================================================================================ UWRSwampSystems

UWRSwampSystems* UWRSwampSystems::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRSwampSystems>() : nullptr;
}

bool UWRSwampSystems::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRSwampSystems::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace WRSwampSys;
	Super::OnWorldBeginPlay(InWorld);
	const FString Map = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	if (Map != FWRTuning::Get().Str(TEXT("swampHazards.map"), TEXT("L_Island")) || !WRSwamp::IsLive()) return;
	bActive = true;
	bServer = InWorld.GetNetMode() != NM_Client;
	BuildLocal();
	if (bServer) SpawnServer();
	UE_LOG(LogWildreach, Log, TEXT("SWAMP systems up on %s (%s): %d gas fields, %d things"), *Map, bServer ? TEXT("server") : TEXT("client"), GasFields.Num(), Things.Num());
}

void UWRSwampSystems::Deinitialize()
{
	if (StrikeHandle.IsValid()) WRStorm::OnStrikeLanded().Remove(StrikeHandle);
	StrikeHandle.Reset();
	Super::Deinitialize();
}

AWRSwampThing* UWRSwampSystems::SpawnThing(FName Kind, const FVector& At, float Yaw, const FString& Title, const FString& Verb, float Reach)
{
	return SpawnThing(Kind, At, Yaw, Title, Verb, Reach, [](AWRSwampThing&) {});
}

AWRSwampThing* UWRSwampSystems::SpawnThing(FName Kind, const FVector& At, float Yaw, const FString& Title, const FString& Verb, float Reach, TFunctionRef<void(AWRSwampThing&)> Setup)
{
	UWorld* World = GetWorld();
	if (!World || !bServer) return nullptr;
	const FTransform T(FRotator(0.f, Yaw, 0.f), At);
	AWRSwampThing* Th = World->SpawnActorDeferred<AWRSwampThing>(AWRSwampThing::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Th) return nullptr;
	Th->Kind = Kind;
	Th->Title = Title;
	Th->Verb = Verb;
	Th->Reach = Reach;
	Setup(*Th);
	Th->FinishSpawning(T);
	Things.Add(Th);
	return Th;
}

void UWRSwampSystems::BuildLocal()
{
	// The two secret ways are the World track's sceneries (look and collision fitted to the cliff); nothing of this
	// file's to build on every machine beyond what the replicated actors draw for themselves.
	Ways.Reset();
}

void UWRSwampSystems::SpawnServer()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	if (bSpawned || !World) return;
	bSpawned = true;
	AWRSwampState::GetOrSpawn(World);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Marsh gas: one actor per field (swamp.json gas), its vents in chain order from wherever it's lit.
	for (const FWRSwampData::FGasField& G : D.Gas)
	{
		if (G.Vents.Num() == 0) continue;
		const FTransform T(G.At);
		AWRMarshGas* Gas = World->SpawnActorDeferred<AWRMarshGas>(AWRMarshGas::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Gas) continue;
		for (const FVector& V : G.Vents) Gas->Vents.Add(FVector_NetQuantize(V));
		Gas->FinishSpawning(T);
		GasFields.Add(Gas);
	}

	// The Hanging Cypress's key cage (M-S2, quest 5): the skeleton's raised hand, 18 m up on the long east limb.
	if (const FWRSwampData::FSite* Hc = D.FindSite(TEXT("hanging_cypress")))
	{
		FVector Cage = WRSwamp::Point(Hc->Row, TEXT("keyCage"));
		if (Cage.IsZero()) Cage = Hc->At + FVector(600.f, 0.f, WRTune(TEXT("swampWorld.keyCageHeight"), 1800.f));
		SpawnThing(TEXT("key_cage"), Cage, 0.f, TEXT("The key cage"), TEXT("Take the key"), H(TEXT("secrets.cageReach"), 400.f));
	}
	// The Walking House's trapdoor (M-S2): in the hut's floor beside the hearth, a black iron lock.
	FVector CellarFoot = FVector::ZeroVector;
	AWRSwampCoven::RoomFloorAt(TEXT("cellar"), CellarFoot);
	if (const FWRSwampData::FSite* Hs = D.FindSite(TEXT("walking_house")))
	{
		const FRotator R(0.f, Hs->Yaw, 0.f);
		const FVector Lid = Hs->At + R.RotateVector(FVector(-40.f, -90.f, WRTune(TEXT("swampWorld.houseFloor"), 1500.f) + 4.f));
		SpawnThing(TEXT("trapdoor"), Lid, Hs->Yaw, TEXT("A trapdoor in the floor"), TEXT("Unlock with the Coven key"), 220.f, [&](AWRSwampThing& T)
		{
			T.bShut = true;
			T.Dest = CellarFoot + FVector(120.f, 0.f, 0.f);
			T.DestYaw = Hs->Yaw;
		});
	}
	// The collapsed chimney (way in 3): a tamed Bogsnout digs it open over 20 s.
	if (!D.Chimney.IsZero())
	{
		FVector In = FVector::ZeroVector;
		AWRSwampCoven::RoomFloorAt(TEXT("chimney"), In);
		Chimney = SpawnThing(TEXT("chimney"), D.Chimney, 0.f, TEXT("A collapsed chimney"), TEXT("Climb down the flue"), 320.f, [&](AWRSwampThing& T)
		{
			T.bShut = true;
			T.Dest = In + FVector(150.f, 0.f, 0.f);
			T.DestYaw = 0.f;
		});
	}
	// M-S3: the fourth bog body at the dome's edge past the Peat Cut, hidden until a Bogsnout roots it out.
	if (const FWRSwampData::FSite* Pc = D.FindSite(TEXT("peat_cut")))
	{
		FVector Fourth = WRSwamp::Point(Pc->Row, TEXT("fourth"));
		Fourth = Fourth.IsZero() ? Pc->At + FVector(5500.f, -3000.f, 0.f) : FVector(Fourth.X, Fourth.Y, Pc->At.Z);
		FourthBody = SpawnThing(TEXT("bog_body"), Fourth, Pc->Yaw + 160.f, TEXT("A fourth body"), TEXT("Take the torc"), 280.f, [](AWRSwampThing& T)
		{
			T.bConcealed = true;
			T.Items.Add(TEXT("gold_torc"), 1);
			T.Lore = TEXT("Curled on her side in the peat, the cord still at her throat, a twisted gold torc in her hands. Calm, like the other three.");
		});
	}
	// M-S1: the candle and the reliquary on the reed island at the end of its line (hidden until the walk is made).
	if (CandleLast() > 0)
	{
		const FVector Start = AWRSwampCandle::PointAt(0.f);
		if (AWRSwampCandle* Cd = World->SpawnActor<AWRSwampCandle>(AWRSwampCandle::StaticClass(), FTransform(Start), P)) Candle = Cd;
		const FVector Isle = D.CandleIsland.IsZero() ? D.CandleLine.Last() : D.CandleIsland;
		CandleCache = SpawnThing(TEXT("cache"), Isle + FVector(0.f, 0.f, 20.f), 30.f, TEXT("A Coven reliquary"), TEXT("Open"), 260.f, [](AWRSwampThing& T)
		{
			T.bConcealed = true;
			T.Items = { { TEXT("coven_ring"), 1 }, { TEXT("bronze_bowl"), 1 }, { TEXT("gem_ruby"), 1 } };
			T.Lore = TEXT("Wrapped in oilcloth under the reeds: a ring blackened by hands, a bowl, a stone the colour of the tithe.");
			T.RefillHours = H(TEXT("secrets.candleRefillHours"), 72.f);
		});
	}
	// M-S4: the Coven smith's cache on the landing at the top of the cut steps behind the third fall.
	{
		const FVector Top = AWRSwampWays::LedgeTop();
		if (!Top.IsZero())
		{
			SpawnThing(TEXT("smith_cache"), Top + FVector(0.f, 0.f, 10.f), 0.f, TEXT("The Coven smith's cache"), TEXT("Open"), 260.f, [](AWRSwampThing& T)
			{
				T.Items = { { TEXT("iron_ingot"), 6 }, { TEXT("bloomery_mould"), 1 } };
				T.Lore = TEXT("Bog-iron blooms beaten into bars, and a clay mould for a furnace's throat. They smelted behind the water, where the smoke hid in the spray.");
				T.RefillHours = H(TEXT("secrets.smithRefillHours"), 72.f);
			});
		}
	}
	// Wild bog rice in the Stilt Village's drowned paddies and bloodroot on the domes' dry edges (seed comes with them).
	if (const FWRSwampData::FSite* Sv = D.FindSite(TEXT("stilt_village")))
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector At = Sv->At + FRotator(0.f, 25.f + 60.f * i, 0.f).Vector() * (1800.f + 500.f * (i % 2));
			SpawnThing(TEXT("wild_plant"), FVector(At.X, At.Y, Sv->At.Z), 40.f * i, TEXT("Wild bog rice"), TEXT("Cut"), 260.f, [](AWRSwampThing& T) { T.Crop = TEXT("bog_rice"); });
		}
	}
	for (const FName Site : { FName(TEXT("bloodmoss_dome")), FName(TEXT("peat_cut")) })
	{
		if (const FWRSwampData::FSite* S = D.FindSite(Site))
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const FVector At = S->At + FRotator(0.f, 70.f + 150.f * i, 0.f).Vector() * 2600.f;
				SpawnThing(TEXT("wild_plant"), FVector(At.X, At.Y, S->At.Z), 90.f * i, TEXT("Wild bloodroot"), TEXT("Dig up"), 240.f, [](AWRSwampThing& T) { T.Crop = TEXT("bloodroot"); });
			}
		}
	}
	// The Sunken Coven itself (its own space; it places its own things and creatures).
	if (!D.DeepPool.IsZero() && D.CovenRooms.Num() > 0)
	{
		if (AWRSwampCoven* Cv = World->SpawnActor<AWRSwampCoven>(AWRSwampCoven::StaticClass(), FTransform(AWRSwampCoven::Origin()), P)) Coven = Cv;
	}
	// Storm strikes on swamp water shock everyone standing in it (cook 2.21).
	if (!StrikeHandle.IsValid()) StrikeHandle = WRStorm::OnStrikeLanded().AddUObject(this, &UWRSwampSystems::StrikeLanded);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP server things: %d gas fields, %d things, coven %s"), GasFields.Num(), Things.Num(), Coven.IsValid() ? TEXT("up") : TEXT("none"));
}

void UWRSwampSystems::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !bActive) return;
	if (bServer)
	{
		ServerClock += DeltaTime;
		const float Step = 1.f / FMath::Max(1.f, WRTune(TEXT("swampHazards.tickHz"), 4.f));
		if (ServerClock >= Step)
		{
			StepWaders(ServerClock);
			ServerClock = 0.f;
		}
		StepShocks();
		StepSecrets(DeltaTime);
		SlowClock += DeltaTime;
		if (SlowClock >= 1.f)
		{
			StepTide();
			StepGas();
			StepPuddles(SlowClock);
			StepStormCell();
			StepRuslana();
			SlowClock = 0.f;
		}
		RefillClock += DeltaTime;
		if (RefillClock >= 10.f)
		{
			StepRefills();
			RefillClock = 0.f;
		}
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		TellClock += DeltaTime;
		if (TellClock >= 0.1f) { StepClientTells(); TellClock = 0.f; }
	}
}

void UWRSwampSystems::StepWaders(float Dt)
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const FWRSwampData& D = FWRSwampData::Get();
	const double T = Now(World);
	const float Tell0 = H(TEXT("bleedwater.tellSeconds"), 0.9f);
	TSet<AWRCharacter*> Seen;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
		AWRCharacter* C = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
		if (!C || !C->IsAlive() || !C->Status) continue;
		Seen.Add(C);
		FWader& W = Waders.FindOrAdd(C);
		const FVector F = Feet(C);

		// The Hex-ward charm (worn): a curse that takes you breaks, once a minute (cook 2.19).
		if (C->HasTag(WRTags::Status_Curse) && UWRInventoryComponent::GearStatFor(C, TEXT("hexWard")) > 0.f && T >= W.HexWardReadyAt)
		{
			C->Status->CureStatus(WRStatus::Curse);
			W.HexWardReadyAt = T + H(TEXT("hexWard.cooldown"), 60.f);
			WRFx::BroadcastOn(C, TEXT("mire.flash"), NAME_None, 0.6f, TEXT("world.chime"));
			Tell(C, TEXT("Your Hex-ward charm cracks, and the curse goes back the way it came"));
		}

		bool bWet = false;
		const EWRSwampWater K = WaterAtFeet(World, C, bWet);
		const bool bMountPass = UWRSwampBody::MountPasses(C, TEXT("bleedwater"));
		const bool bBleed = K == EWRSwampWater::Bleedwater && bWet;

		// --- Bleedwater: 10 buildup a second (15 in the tide's boil), after the edge tell; never slows.
		if (bBleed)
		{
			// On a mount that passes it the tell is already done: get down in the water and it starts at once.
			if (W.InBleedSince < 0.0 || bMountPass) W.InBleedSince = bMountPass ? T - Tell0 - 1.0 : T;
			const bool bBoil = InBoil(World, F);
			if (!bMountPass && T - W.InBleedSince >= Tell0)
			{
				const float Rate = bBoil ? H(TEXT("leechBoil.buildupPerSecond"), 15.f) : H(TEXT("bleedwater.buildupPerSecond"), 10.f);
				const float Amount = Rate * Dt * BleedwaterScale(C);
				if (Amount > 0.f) C->Status->AddStatusBuildup(WRStatus::Bleed, Amount, nullptr, 0.f);
			}
			// The leech boil: a hum rises a second before the latch leeches come up for you (waders don't stop them).
			if (bBoil && !bMountPass)
			{
				const float Every = H(TEXT("leechBoil.latchEvery"), 10.f), Hum = H(TEXT("leechBoil.hum"), 1.f);
				const float Before = W.WadeSeconds;
				W.WadeSeconds += Dt;
				if (Before < Every - Hum && W.WadeSeconds >= Every - Hum)
				{
					WRFx::Broadcast(World, TEXT("swamp.leech.boil"), F, FRotator::ZeroRotator, 1.f, TEXT("swamp.leech.hum"));
				}
				if (W.WadeSeconds >= Every)
				{
					W.WadeSeconds -= Every;
					SpawnLatchers(C, FMath::RoundToInt(H(TEXT("leechBoil.latchCount"), 2.f)));
				}
			}
			else
			{
				W.WadeSeconds = 0.f;
			}
			// Leeches by hand while wading (cook 2.7): stand in it and one comes off your shin every leechEvery seconds.
			if (!bMountPass)
			{
				const float Every = H(TEXT("bleedwater.leechEvery"), 10.f);
				if (!W.bGathering || FVector::Dist2D(F, W.GatherFrom) > H(TEXT("bleedwater.leechMove"), 150.f))
				{
					W.bGathering = true;
					W.GatherFrom = F;
					W.NextLeechAt = T + Every;
				}
				else if (T >= W.NextLeechAt)
				{
					W.NextLeechAt = T + Every;
					if (AWRPlayerState* PS = C->GetPlayerState<AWRPlayerState>())
					{
						if (PS->GetInventory() && PS->GetInventory()->AddItem(TEXT("leech"), 1) == 0) Tell(C, TEXT("You pull a fat leech off your shin: leech +1"));
					}
				}
			}
		}
		else
		{
			W.InBleedSince = -1.0;
			W.WadeSeconds = 0.f;
			W.bGathering = false;
		}
		// Ruslana's second quest: a bleed proc taken in bleedwater, until it's cured (OnBleedCured).
		const bool bBleeding = C->HasTag(WRTags::Status_Bleeding);
		if (bBleeding && !W.bWasBleeding && bBleed) W.bBledInWater = true;
		W.bWasBleeding = bBleeding;

		// --- Quag: run x0.5 (the replicated tag both machines read); standing still 3 s sinks you to the knee.
		const bool bMounted = MountOf(C) != nullptr;
		const bool bQuag = K == EWRSwampWater::Quag && bWet && !bMounted && !UWRSwampBody::MountPasses(C, TEXT("quag")) && !C->GetCharacterMovement()->IsSwimming();
		if (bQuag)
		{
			if (!W.bQuag) { W.bQuag = true; W.StillSince = -1.0; SetTag(C, WRSwampTags::Quag, true); }
			const bool bStill = C->GetVelocity().Size2D() < H(TEXT("quag.stillSpeed"), 40.f) && !C->GetCharacterMovement()->IsFalling();
			if (bStill)
			{
				if (W.StillSince < 0.0) W.StillSince = T;
				if (!W.bSunk && T - W.StillSince >= H(TEXT("quag.sinkAfter"), 3.f))
				{
					W.bSunk = true;
					SetTag(C, WRSwampTags::Sunk, true);
					WRFx::Broadcast(World, TEXT("mire.mud"), F, FRotator::ZeroRotator, 0.8f, TEXT("swamp.quag.suck"));
				}
			}
			else
			{
				W.StillSince = -1.0;
			}
		}
		else if (W.bQuag || W.bSunk)
		{
			W.bQuag = W.bSunk = false;
			W.StillSince = -1.0;
			SetTag(C, WRSwampTags::Quag, false);
			SetTag(C, WRSwampTags::Sunk, false);
		}

		// --- The Deep Pool's dive (way in 1): down through the gap in the mat, to the Drowned Lane's air pocket.
		if (!D.DeepPool.IsZero() && FVector::Dist2D(F, D.DeepPool) < DeepPoolRadius() && C->GetCharacterMovement()->IsSwimming()
			&& C->GetActorLocation().Z < D.SeaZ - H(TEXT("coven.diveBelow"), 400.f))
		{
			W.StandSeconds += Dt;
			if (W.StandSeconds >= H(TEXT("coven.diveHold"), 1.5f) && T >= W.NextDiveAt)
			{
				W.NextDiveAt = T + 10.0;
				W.StandSeconds = 0.f;
				FVector To;
				if (AWRSwampCoven::RoomFloorAt(TEXT("mouth"), To))
				{
					float Yaw = 0.f;
					FVector Gap;
					if (AWRSwampCoven::LaneGap(TEXT("drowned_lane"), TEXT("mouth"), Gap, Yaw)) Yaw = (Gap - To).Rotation().Yaw;
					Travel(PC, To + FVector(0.f, 0.f, 10.f), Yaw, TEXT("swamp.dive"));
					Tell(C, FString::Printf(TEXT("Down through the gap in the mat, %d seconds without air, and up into the dark"), FMath::RoundToInt(H(TEXT("coven.breath"), 12.f))));
					UE_LOG(LogWildreach, Log, TEXT("SWAMP %s dives into the Sunken Coven"), *C->GetName());
				}
			}
		}
		else
		{
			W.StandSeconds = 0.f;
		}
		W.LastPos = F;
	}
	// The ones who left (dead, gone, logged out): their tags come off; the record goes.
	for (auto It = Waders.CreateIterator(); It; ++It)
	{
		AWRCharacter* C = It.Key().Get();
		if (C && Seen.Contains(C)) continue;
		if (C)
		{
			SetTag(C, WRSwampTags::Quag, false);
			SetTag(C, WRSwampTags::Sunk, false);
		}
		It.RemoveCurrent();
	}
}

void UWRSwampSystems::SpawnLatchers(AWRCharacter* Wader, int32 Count)
{
	using namespace WRSwampSys;
	if (!Wader || !Wader->IsAlive() || Count <= 0) return;
	const int32 Came = FWRSwampAI::SpawnLeechSwarm(Wader, Count);
	if (Came > 0) Tell(Wader, Came > 1 ? TEXT("Leeches come up out of the boil for you") : TEXT("A leech comes up out of the boil for you"));
	UE_LOG(LogWildreach, Log, TEXT("SWAMP leech boil: %d latch leeches on %s"), Came, *Wader->GetName());
}

void UWRSwampSystems::StepTide()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	AWRSwampState* State = AWRSwampState::GetOrSpawn(World);
	if (!State) return;
	// The schedule is the world systems' (world.json events.red_tide: its warning, then one game day); this follows it.
	const AWRWorldState* WS = AWRWorldState::Get(World);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("red_tide")) : nullptr;
	const EWRRedTide Want = !E ? EWRRedTide::None : E->Stage >= 1 ? EWRRedTide::Tide : EWRRedTide::Warning;
	const bool bNight = AWRGameState::IsNightIn(World);
	bool bDirty = false;
	if (Want != State->RedTide)
	{
		const EWRRedTide Was = State->RedTide;
		State->RedTide = Want;
		State->PhaseStartedAt = Now(World);
		State->TideEndsAt = E ? E->EndAt : 0.f;
		bDirty = true;
		const FWRSwampData& D = FWRSwampData::Get();
		if (Want == EWRRedTide::Warning)
		{
			// The warning (5 min): the falls run oxblood and the mist doubles (the World track reads the state), every
			// murder of crows in the Mire goes up at once, and the low warning sounds for everyone in the swamp.
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* Pn = It->Get() ? It->Get()->GetPawn() : nullptr;
				if (Pn && (WRSwamp::Contains(Pn->GetActorLocation()) || WRSwamp::InCoven(Pn->GetActorLocation()))) WRSound::PlayUI(It->Get(), TEXT("swamp.tide.warning"));
			}
			for (const FName Site : { FName(TEXT("rookery")), FName(TEXT("hanging_cypress")), FName(TEXT("ossuary")), FName(TEXT("bled_roc")), FName(TEXT("tithe_mound")) })
			{
				if (const FWRSwampData::FSite* S = D.FindSite(Site)) WRFx::Broadcast(World, TEXT("swamp.crows.lift"), S->At + FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, 1.f, TEXT("swamp.crows"));
			}
		}
		if (Want == EWRRedTide::Tide)
		{
			// The tide: the Reliquary's door lifts with the mat; bloodmoss comes thick (x3).
			State->bReliquaryOpen = true;
			for (const TWeakObjectPtr<AWRSwampThing>& Pt : ReliquaryParts) if (Pt.IsValid()) Pt->SetState(Pt->bSpent, false, false);
			const float Moss = Tide(TEXT("bloodmoss"), 3.f);
			for (TActorIterator<AWRResourceNode> It(World); It; ++It)
			{
				if (It->ItemId != TEXT("bloodmoss") || !WRSwamp::Contains(It->GetActorLocation()) || MossBase.Contains(*It)) continue;
				MossBase.Add(*It, It->PerHit);
				It->PerHit = FMath::RoundToInt(It->PerHit * Moss);
			}
		}
		if (Want == EWRRedTide::None || (Was == EWRRedTide::Tide && Want != EWRRedTide::Tide))
		{
			// The ebb: the stone door seals again (anyone inside the Reliquary is put out in the lane first), the moss
			// goes back to its own.
			State->bReliquaryOpen = false;
			FVector Out;
			float Yaw = 0.f;
			FVector RC, RH;
			if (AWRSwampCoven::LaneGap(TEXT("ringing_room"), TEXT("reliquary"), Out, Yaw) && AWRSwampCoven::RoomBox(TEXT("reliquary"), RC, RH))
			{
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					APawn* Pn = It->Get() ? It->Get()->GetPawn() : nullptr;
					if (Pn && InRoom(TEXT("reliquary"), Pn->GetActorLocation(), 120.f))
					{
						Pn->TeleportTo(Out + FRotator(0.f, Yaw, 0.f).Vector() * 200.f + FVector(0.f, 0.f, Pn->GetSimpleCollisionHalfHeight() + 20.f), FRotator(0.f, Yaw, 0.f));
						Tell(Pn, TEXT("The water drops, and the stone door grinds down behind you"));
					}
				}
			}
			for (const TWeakObjectPtr<AWRSwampThing>& Pt : ReliquaryParts) if (Pt.IsValid()) Pt->SetState(Pt->bSpent, true, false);
			for (const auto& KV : MossBase) if (AWRResourceNode* N = Cast<AWRResourceNode>(KV.Key.Get())) N->PerHit = KV.Value;
			MossBase.Reset();
		}
		UE_LOG(LogWildreach, Log, TEXT("SWAMP red tide: %d -> %d"), int32(Was), int32(Want));
	}
	// The Hex-Warden's walk is a Red Tide night's (the Bosses track checks nobody's fighting her at the Ossuary).
	const bool bWalk = State->RedTide == EWRRedTide::Tide && bNight;
	if (State->bHexWalk != bWalk) { State->bHexWalk = bWalk; bDirty = true; }
	if (bDirty) State->ForceNetUpdate();
	LastTide = State->RedTide;
	bWasTide = State->IsTide();
}

void UWRSwampSystems::StepGas()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	// Fire near a field lights it (DC 2.8): a held torch, anything burning, a fire wall's or lava pool's ground. Fire shots,
	// fire pots and burn procs call WRSwamp::FireAt themselves.
	const float Range = H(TEXT("marshGas.activeRange"), 9000.f);
	for (const TWeakObjectPtr<AWRMarshGas>& Wg : GasFields)
	{
		AWRMarshGas* G = Wg.Get();
		if (!G || !G->IsCharged() || !AnyPlayerNear(World, G->GetActorLocation(), Range)) continue;
		const float Reach = G->Reach();
		bool bLit = false;
		for (TActorIterator<AWRCharacterBase> It(World); It && !bLit; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive() || FVector::Dist2D(C->GetActorLocation(), G->GetActorLocation()) > Reach) continue;
			const bool bFire = C->HasTag(WRTags::Status_Burn) || UWRSurvivalComponent::HoldsTorch(C);
			if (bFire) bLit = G->TryIgnite(Feet(C), C);
		}
		for (TActorIterator<AWRHazard> It(World); It && !bLit; ++It)
		{
			if ((It->Kind == TEXT("fire") || It->Kind == TEXT("lava")) && FVector::Dist2D(It->GetActorLocation(), G->GetActorLocation()) < Reach + It->Radius)
			{
				bLit = G->TryIgnite(It->GetActorLocation(), It->GetOwner());
			}
		}
	}
}

void UWRSwampSystems::StepShocks()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const double T = Now(World);
	for (int32 i = Shocks.Num() - 1; i >= 0; --i)
	{
		FShock& S = Shocks[i];
		if (T >= S.Until) { Shocks.RemoveAt(i); continue; }
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* C = *It;
			if (!C->IsAlive() || S.Hit.Contains(C) || FVector::Dist2D(C->GetActorLocation(), S.Center) > S.Radius + C->GetSimpleCollisionRadius()) continue;
			const FVector F = Feet(C);
			if (FMath::Abs(F.Z - S.Center.Z) > 450.f) continue;
			// In the water: swimming, wading the Mire's water, a Coven pool, the chapel.
			float PoolZ = 0.f, PoolDepth = 0.f;
			const bool bInWater = UWRWaterSubsystem::IsInWater(C) || AWRSwampCoven::IsBleedPool(F) || (WRSwamp::CovenPool(F, PoolZ, PoolDepth) && F.Z < PoolZ)
				|| (WRSwamp::Contains(F) && WRSwamp::HazardWaterAt(World, F) != EWRSwampWater::Dry && WRSwamp::IsWet(F + FVector(0.f, 0.f, 15.f)));
			if (!bInWater) continue;
			S.Hit.Add(C);
			if (WRStorm::HasBuff(C, TEXT("grounding"))) continue;   // Grounded: the current goes round you
			FWRHit Hit;
			Hit.Instigator = S.By.Get();
			Hit.Damage = H(TEXT("stormJar.damage"), 10.f);
			Hit.Buildups.Add(TEXT("shock"), H(TEXT("stormJar.shock"), 60.f));
			Hit.Element = WRTags::Element_Lightning;
			Hit.Origin = S.Center;
			Hit.bUnblockable = true;
			UWRCombatLibrary::ApplyHit(C, Hit);
		}
	}
}

void UWRSwampSystems::StepPuddles(float Dt)
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const double Hrs = Hours(World);
	// Blood invaders mark where they stand (cook 2.21): 4 s still leaves a 3 m bleedwater puddle for 2 game hours.
	const AWRWorldState* WS = AWRWorldState::Get(World);
	UWRWorldSystems* Sys = UWRWorldSystems::Get(World);
	if (WS && Sys && WS->Invasion.bOn && WS->Invasion.From == TEXT("swamp") && WS->Invasion.Stage >= 1)
	{
		const float Stand = H(TEXT("puddle.standSeconds"), 4.f), Radius = H(TEXT("puddle.radius"), 300.f);
		for (const TWeakObjectPtr<AWRCreature>& Wc : Sys->Invaders)
		{
			AWRCreature* C = Wc.Get();
			if (!C || !C->IsAlive()) continue;
			float& Still = InvaderStill.FindOrAdd(C);
			const bool bStill = C->GetVelocity().Size2D() < 40.f && !C->GetCharacterMovement()->IsFalling();
			if (!bStill) { Still = 0.f; continue; }
			if (Still < 0.f) continue;   // already left one here; moves before the next
			Still += Dt;
			if (Still < Stand) continue;
			Still = -1.f;
			const FVector F(C->GetActorLocation().X, C->GetActorLocation().Y, GroundZ(World, C->GetActorLocation(), Feet(C).Z, C));
			bool bNear = false;
			for (const TWeakObjectPtr<AWRSwampPuddle>& Pd : Puddles) bNear |= Pd.IsValid() && FVector::Dist2D(Pd->GetActorLocation(), F) < Radius;
			if (bNear) continue;
			const FTransform Tr(F + FVector(0.f, 0.f, 2.f));
			AWRSwampPuddle* Pd = World->SpawnActorDeferred<AWRSwampPuddle>(AWRSwampPuddle::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Pd) continue;
			Pd->Radius = Radius;
			Pd->GoneAtHours = Hrs + H(TEXT("puddle.hours"), 2.f);
			Pd->FinishSpawning(Tr);
			Puddles.Add(Pd);
		}
	}
	else
	{
		InvaderStill.Reset();
	}
	for (int32 i = Puddles.Num() - 1; i >= 0; --i)
	{
		AWRSwampPuddle* Pd = Puddles[i].Get();
		if (!Pd) { Puddles.RemoveAt(i); continue; }
		if (Hrs >= Pd->GoneAtHours) { Pd->Destroy(); Puddles.RemoveAt(i); }
	}
}

void UWRSwampSystems::StepStormCell()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const AWRWorldState* WS = AWRWorldState::Get(World);
	UWRWorldSystems* Sys = UWRWorldSystems::Get(World);
	if (!WS || !Sys || !WS->Invasion.bOn || WS->Invasion.From != TEXT("storm_peaks") || WS->Invasion.To != TEXT("swamp") || WS->Invasion.Stage < 1) return;
	// The cell rides over the invaders (cook 2.21): their middle, the Bellwether's if it's out.
	FVector Mid = FVector::ZeroVector;
	int32 N = 0;
	for (const TWeakObjectPtr<AWRCreature>& Wc : Sys->Invaders)
	{
		const AWRCreature* C = Wc.Get();
		if (!C || !C->IsAlive()) continue;
		if (C->CreatureId == TEXT("bellwether")) { Mid = C->GetActorLocation(); N = 1; break; }
		Mid += C->GetActorLocation();
		++N;
	}
	if (N == 0) return;
	Mid /= float(N);
	if (!WRSwamp::Contains(Mid)) return;   // over the Mire only (Storm peaks' own weather handles its side)
	const double T = Now(World);
	if (T >= NextCellCloudAt)
	{
		NextCellCloudAt = T + 20.0;
		WRFx::Broadcast(World, TEXT("storm.cloud"), Mid + FVector(0.f, 0.f, 3500.f), FRotator::ZeroRotator, 2.f, NAME_None, 21.f);
	}
	if (NextCellStrike <= 0.0) NextCellStrike = T + FMath::FRandRange(8.f, 16.f);
	if (T < NextCellStrike) return;
	const float EveryMin = H(TEXT("stormCell.everyMin"), 20.f);
	NextCellStrike = T + FMath::FRandRange(EveryMin, FMath::Max(EveryMin, H(TEXT("stormCell.everyMax"), 40.f)));
	// The tallest thing within 60 m of it (not the storm's own).
	AWRCharacterBase* Tallest = nullptr;
	float Top = -1.0e9f;
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || FVector::Dist2D(C->GetActorLocation(), Mid) > H(TEXT("stormCell.range"), 6000.f)) continue;
		if (const AWRCreature* Cr = Cast<AWRCreature>(C); Cr && Sys->Invaders.Contains(TWeakObjectPtr<AWRCreature>(const_cast<AWRCreature*>(Cr)))) continue;
		const float Z = C->GetActorLocation().Z + C->GetSimpleCollisionHalfHeight();
		if (Z > Top) { Top = Z; Tallest = C; }
	}
	FWRStrikeCall Call;
	Call.Spot = Tallest ? Feet(Tallest) : Mid + FMath::VRand().GetSafeNormal2D() * FMath::FRandRange(1000.f, 4000.f);
	Call.Tell = FMath::Max(1.f, H(TEXT("stormCell.tell"), 1.f));
	Call.Damage = WRTune(TEXT("storm.strike.damage"), 60.f);
	Call.Shock = WRTune(TEXT("storm.strike.shock"), 40.f);
	Call.Source = EWRStrikeSource::Weather;
	Call.Picked = Tallest;
	WRStorm::CallStrike(World, Call);
}

void UWRSwampSystems::StrikeLanded(UWorld* World, const FWRStrikeCall& Call)
{
	using namespace WRSwampSys;
	if (!World || World != GetWorld() || !bServer) return;
	// A strike on the Mire's water runs through it: everyone in that water within 15 m is shocked (cook 2.21).
	const FVector At = Call.Spot;
	const bool bPuddle = !FWRSwampData::Get().InFrame(At) && WRSwamp::HazardWaterAt(World, At) == EWRSwampWater::Bleedwater;
	const bool bWater = bPuddle || (WRSwamp::Contains(At) && WRSwamp::HazardWaterAt(World, At) != EWRSwampWater::Dry && WRSwamp::IsWet(At + FVector(0.f, 0.f, 30.f)));
	if (bWater) WRSwamp::ShockWater(World, At, H(TEXT("stormCell.waterShock"), 1500.f), H(TEXT("stormCell.shockSeconds"), 1.f), Call.Instigator.Get());
}

void UWRSwampSystems::StepRuslana()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	AWRWorldNpc* Rus = nullptr;
	for (TActorIterator<AWRWorldNpc> It(World); It; ++It) if (It->NpcRole == TEXT("master") && It->NpcId == TEXT("ruslana")) { Rus = *It; break; }
	if (!Rus) return;
	// By day at the Tithe Mound grinding bloodmoss; by night in the Walking House, its window lit (cook 2.11).
	const bool bNight = AWRGameState::IsNightIn(World);
	FVector Want = FVector::ZeroVector;
	float WantYaw = 0.f;
	if (bNight)
	{
		const FWRSwampData::FSite* Hs = FWRSwampData::Get().FindSite(TEXT("walking_house"));
		if (!Hs) return;
		const FRotator R(0.f, Hs->Yaw, 0.f);
		Want = Hs->At + R.RotateVector(FVector(-30.f, 70.f, WRTune(TEXT("swampWorld.houseFloor"), 1500.f)));
		WantYaw = Hs->Yaw;
	}
	else if (!WRSwamp::MasterPlace(MasterRow(TEXT("ruslana")), Want, WantYaw))
	{
		return;
	}
	bRuslanaNight = bNight;
	if (FVector::Dist2D(Rus->WorkAt, Want) < 100.f) return;
	// She goes when nobody's watching (her old place or her new one): no one sees her vanish.
	const float Clear = H(TEXT("ruslana.swapClear"), 6000.f);
	if (AnyPlayerNear(World, Rus->GetActorLocation(), Clear) || AnyPlayerNear(World, Want, Clear)) return;
	Rus->WorkAt = Want;
	Rus->WorkFacing = FRotator(0.f, WantYaw, 0.f);
	Rus->TeleportTo(Want + FVector(0.f, 0.f, Rus->GetSimpleCollisionHalfHeight() + 10.f), Rus->WorkFacing);
	UE_LOG(LogWildreach, Log, TEXT("SWAMP Ruslana goes to %s"), bNight ? TEXT("the Walking House") : TEXT("the Tithe Mound"));
}

void UWRSwampSystems::StepSecrets(float Dt)
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const double T = Now(World);
	const bool bNight = AWRGameState::IsNightIn(World);

	// --- M-S1, the candle path: at night a corpse candle drifts the hidden line over the Mere; walk exactly under it.
	if (AWRSwampCandle* Cd = Candle.Get())
	{
		const int32 Last = CandleLast();
		const auto PutOut = [&](float For)
		{
			if (!Cd->bOut) { Cd->bOut = true; Cd->ForceNetUpdate(); Cd->OnRep_Out(); }
			Cd->Along = 0.f;
			CandleOutUntil = T + For;
			for (auto& KV : Waders) KV.Value.CandleIndex = -1;
		};
		if (!bNight)
		{
			if (!Cd->bOut || Cd->Along > 0.f) PutOut(0.f);
			// Dawn: the reed island's reliquary sinks back out of sight if it wasn't taken.
			if (AWRSwampThing* Cc = CandleCache.Get()) if (!Cc->bConcealed && !Cc->bSpent) Cc->SetState(false, false, true);
		}
		else if (Cd->bOut)
		{
			// It kindles over the jetty when someone waits there at night.
			if (T >= CandleOutUntil && AnyPlayerNear(World, AWRSwampCandle::PointAt(0.f) - FVector(0.f, 0.f, 130.f), H(TEXT("secrets.candleStart"), 1500.f)))
			{
				Cd->Along = 0.f;
				Cd->bOut = false;
				Cd->ForceNetUpdate();
				Cd->OnRep_Out();
			}
		}
		else
		{
			float Lead = -1.f;
			bool bOff = false;
			APawn* Strayed = nullptr;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AWRCharacter* P = It->Get() ? Cast<AWRCharacter>(It->Get()->GetPawn()) : nullptr;
				if (!P || !P->IsAlive()) continue;
				FWader* W = Waders.Find(P);
				if (!W) continue;
				float Along = 0.f;
				const float Off = OffCandleLine(Feet(P), Along);
				const bool bOnLine = Off <= H(TEXT("secrets.candleStep"), 250.f) && FMath::Abs(Feet(P).Z - FWRSwampData::Get().SeaZ) < 250.f;
				if (W->CandleIndex >= 0)
				{
					if (!bOnLine && Along < Last - 0.3f) { bOff = true; Strayed = P; continue; }
					W->CandleIndex = FMath::Max(W->CandleIndex, FMath::FloorToInt(Along));
					Lead = FMath::Max(Lead, Along);
				}
				else if (bOnLine && Along < 1.f)
				{
					W->CandleIndex = 0;
					Lead = FMath::Max(Lead, Along);
				}
			}
			if (bOff)
			{
				// Step off the line and it goes out (it comes back later in the night).
				Tell(Strayed, TEXT("The candle gutters out"));
				PutOut(H(TEXT("secrets.candleRelight"), 45.f));
			}
			else
			{
				// It keeps a few steps ahead of whoever walks under it, and waits for them.
				const float Want = Lead < 0.f ? Cd->Along : FMath::Min(Lead + H(TEXT("secrets.candleLead"), 0.8f), float(Last));
				const float SegLen = FMath::Max(100.f, FVector::Dist2D(AWRSwampCandle::PointAt(FMath::FloorToFloat(Cd->Along)), AWRSwampCandle::PointAt(FMath::FloorToFloat(Cd->Along) + 1.f)));
				Cd->Along = FMath::FInterpConstantTo(Cd->Along, FMath::Max(Cd->Along, Want), Dt, H(TEXT("secrets.candleSpeed"), 120.f) / SegLen);
				if (Lead >= Last - 0.4f)
				{
					AWRSwampThing* Cc = CandleCache.Get();
					if (Cc && Cc->bConcealed)
					{
						Cc->SetState(Cc->bSpent, false, false);
						for (auto& KV : Waders) if (KV.Value.CandleIndex >= 0) Tell(KV.Key.Get(), TEXT("The candle settles over a reed island. Something is wrapped in the reeds"));
						UE_LOG(LogWildreach, Log, TEXT("SWAMP the candle path walked: the reliquary shows"));
					}
				}
			}
		}
	}

	// --- Tamed Bogsnouts: the fourth bog body (M-S3) and the collapsed chimney (way in 3).
	AWRSwampThing* Body = FourthBody.Get();
	AWRSwampThing* Flue = Chimney.Get();
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		AWRCreature* Sn = *It;
		if (Sn->CreatureId != TEXT("bogsnout") || !Sn->IsTamed() || !Sn->IsAlive()) continue;
		if (Body && Body->bConcealed && FVector::Dist(Sn->GetActorLocation(), Body->GetActorLocation()) < H(TEXT("secrets.bogsnoutRange"), 1500.f))
		{
			Body->SetState(Body->bSpent, false, false);
			WRFx::Broadcast(World, TEXT("mire.mud"), Body->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("swamp.quag.suck"));
			if (AWRPlayerController* Own = ControllerNamed(World, Sn->GetTameOwner())) Own->ClientNotify(TEXT("Your Bogsnout roots at the dome's edge and turns up a hand: a fourth body in the peat"));
			UE_LOG(LogWildreach, Log, TEXT("SWAMP a Bogsnout uncovered the fourth bog body"));
		}
		if (Flue && Flue->bShut && FVector::Dist2D(Sn->GetActorLocation(), Flue->GetActorLocation()) < H(TEXT("coven.digRange"), 600.f))
		{
			const float Before = ChimneyDug;
			ChimneyDug += Dt;
			if (FMath::FloorToInt(Before / 2.f) != FMath::FloorToInt(ChimneyDug / 2.f)) WRFx::Broadcast(World, TEXT("mire.mud"), Flue->GetActorLocation(), FRotator::ZeroRotator, 1.f, TEXT("swamp.quag.suck"));
			if (ChimneyDug >= H(TEXT("coven.digSeconds"), 20.f))
			{
				ChimneyDug = 0.f;
				ChimneyFallsAtHours = Hours(World) + H(TEXT("coven.chimneyHours"), 72.f);
				Flue->SetState(false, false, false);
				if (AWRSwampThing* In = ChimneyIn.Get()) In->SetState(false, false, false);
				if (AWRPlayerController* Own = ControllerNamed(World, Sn->GetTameOwner())) Own->ClientNotify(TEXT("Your Bogsnout breaks through: a stone flue, going down into the dark"));
				UE_LOG(LogWildreach, Log, TEXT("SWAMP the chimney is dug open"));
			}
			break;   // one digger at a time
		}
	}
}

void UWRSwampSystems::StepRefills()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	const double Hrs = Hours(World);
	for (int32 i = Things.Num() - 1; i >= 0; --i)
	{
		AWRSwampThing* Th = Things[i].Get();
		if (!Th) { Things.RemoveAt(i); continue; }
		if (Th->bSpent && Th->RefillAtHours >= 0.0 && Hrs >= Th->RefillAtHours && !AnyPlayerNear(World, Th->GetActorLocation(), 2500.f))
		{
			const bool bCandle = Th == CandleCache.Get();
			Th->Refill();
			if (bCandle) Th->SetState(false, false, true);   // it waits for the next walk
		}
	}
	// The dug chimney falls in again after a few days, when nobody's at either end.
	if (ChimneyFallsAtHours >= 0.0 && Hrs >= ChimneyFallsAtHours)
	{
		AWRSwampThing* Flue = Chimney.Get();
		AWRSwampThing* In = ChimneyIn.Get();
		if ((!Flue || !AnyPlayerNear(World, Flue->GetActorLocation(), 3000.f)) && (!In || !AnyPlayerNear(World, In->GetActorLocation(), 3000.f)))
		{
			ChimneyFallsAtHours = -1.0;
			if (Flue) Flue->SetState(false, true, false);
			if (In) In->SetState(false, true, false);
		}
	}
}

void UWRSwampSystems::StepClientTells()
{
	using namespace WRSwampSys;
	UWorld* World = GetWorld();
	APlayerController* LPC = World ? World->GetFirstPlayerController() : nullptr;
	AWRCharacter* C = LPC && LPC->IsLocalController() ? Cast<AWRCharacter>(LPC->GetPawn()) : nullptr;
	if (!C || !C->IsAlive())
	{
		LastLocalKind = EWRSwampWater::Dry;
		bLocalWasWet = false;
		bLocalSunk = false;
		return;
	}
	const FVector F = Feet(C);
	bool bWet = false;
	const EWRSwampWater K = WaterAtFeet(World, C, bWet);
	const EWRSwampWater Was = bLocalWasWet ? LastLocalKind : EWRSwampWater::Dry;
	const EWRSwampWater Is = bWet ? K : EWRSwampWater::Dry;
	const FVector Dir = C->GetVelocity().Size2D() > 10.f ? C->GetVelocity().GetSafeNormal2D() : C->GetActorForwardVector().GetSafeNormal2D();
	if (Is == EWRSwampWater::Bleedwater && Was != EWRSwampWater::Bleedwater && !UWRSwampBody::MountPasses(C, TEXT("bleedwater")))
	{
		// The edge tell (0.9 s): ripples crawl in across the oxblood toward your foot, and the wet suck.
		const float Tell0 = H(TEXT("bleedwater.tellSeconds"), 0.9f);
		const FVector WaterAt(F.X, F.Y, WRSwamp::InCoven(F) || !FWRSwampData::Get().InFrame(F) ? F.Z + 20.f : FWRSwampData::Get().SeaZ + 1.f);
		for (int32 r = 0; r < 3; ++r)
		{
			const FVector At = WaterAt + Dir * (180.f - 75.f * r);
			if (r == 0) { WRFx::SpawnAt(this, TEXT("swamp.ripple"), At, FRotator::ZeroRotator, 1.6f); continue; }
			FTimerHandle Th;
			const TWeakObjectPtr<UWRSwampSystems> Self(this);
			World->GetTimerManager().SetTimer(Th, FTimerDelegate::CreateLambda([Self, At]() { if (Self.IsValid()) WRFx::SpawnAt(Self.Get(), TEXT("swamp.ripple"), At, FRotator::ZeroRotator, 1.6f); }), Tell0 * 0.33f * r, false);
		}
		WRSound::PlayAt(this, TEXT("swamp.bleed.suck"), F);
	}
	if (Is == EWRSwampWater::Quag && Was != EWRSwampWater::Quag)
	{
		WRSound::PlayAt(this, TEXT("swamp.quag.suck"), F);
		WRFx::SpawnAt(this, TEXT("mire.mud"), F, FRotator::ZeroRotator, 0.6f);
	}
	const bool bSunkNow = C->HasTag(WRSwampTags::Sunk);
	if (bSunkNow && !bLocalSunk)
	{
		if (AWRPlayerController* PC = Cast<AWRPlayerController>(LPC)) PC->ShowLocalNotice(TEXT("Sunk to the knee: step back or jump out"));
	}
	bLocalSunk = bSunkNow;
	// The boil round your legs while you wade the tide's bleedwater.
	const double T = Now(World);
	if (Is == EWRSwampWater::Bleedwater && InBoil(World, F) && T >= NextLocalBoil)
	{
		NextLocalBoil = T + 0.45;
		const FVector2D R2 = FMath::RandPointInCircle(160.f);
		WRFx::SpawnAt(this, TEXT("swamp.ripple"), FVector(F.X + R2.X, F.Y + R2.Y, FWRSwampData::Get().SeaZ + 1.f), FRotator::ZeroRotator, 1.2f);
	}
	LastLocalKind = Is;
	bLocalWasWet = bWet;
}

// ================================================================================================ interact

bool WRSwamp::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	using namespace WRSwampSys;
	const AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!Me || !WRSwamp::IsLive()) return false;
	// A gap in a boardwalk (or the Peat Cellar's wall), then the bosses' broken square, then the swamp's own things.
	if (UWRSwampWorld::PlankPrompt(PC, OutTarget, OutVerb)) return true;
	if (FWRSwampBoss::Prompt(PC, OutTarget, OutVerb)) return true;
	const FVector At = Me->GetActorLocation();
	const AWRSwampThing* Best = nullptr;
	float BestD = 1.0e9f;
	for (TActorIterator<AWRSwampThing> It(Me->GetWorld()); It; ++It)
	{
		if (It->bConcealed) continue;
		const float D = FVector::Dist(It->GetActorLocation(), At);
		if (D < It->Reach && D < BestD) { BestD = D; Best = *It; }
	}
	if (Best)
	{
		const FName Kd = Best->Kind;
		const UWRInventoryComponent* Bag = UWRInventoryComponent::FindFor(Me);
		OutTarget = Best->Title;
		OutVerb = Best->Verb;
		if (Kd == TEXT("key_cage") && Bag && Bag->CountOf(TEXT("coven_key")) > 0) OutVerb = TEXT("Empty: you have the key");
		else if (Kd == TEXT("trapdoor")) OutVerb = Bag && Bag->CountOf(TEXT("coven_key")) > 0 ? FString(TEXT("Unlock with the Coven key")) : FString(TEXT("Locked: a black iron lock"));
		else if (Kd == TEXT("chimney") && Best->bShut) OutVerb = TEXT("Collapsed: something that digs could open it");
		else if ((Kd == TEXT("reliquary") || Kd == TEXT("reliquary_door")) && Best->bShut) OutVerb = TEXT("Sealed: it won't move");
		else if (Best->bSpent) OutVerb = Kd == TEXT("wild_plant") ? FString(TEXT("Cut back: it grows again")) : Kd == TEXT("egg") ? FString(TEXT("Torn open")) : FString(TEXT("Empty"));
		return true;
	}
	// The Murkjaw blood rite (cook 2.12.1): at the mouth of one knocked out, or low and bleeding, with 10 Blood points.
	if (UWRBloodPath::BloodPoints(Me) >= int32(WRTune(TEXT("swampCreatures.ritual.bloodPoints"), 10.f)))
	{
		for (TActorIterator<AWRCreature> It(Me->GetWorld()); It; ++It)
		{
			const AWRCreature* Cr = *It;
			if (Cr->CreatureId != TEXT("murkjaw") || Cr->IsTamed() || !Cr->IsAlive()) continue;
			if (FVector::Dist(Cr->MouthLocation(), At) > WRTune(TEXT("swampCreatures.ritual.reach"), 450.f)) continue;
			const bool bLow = Cr->GetHealthFraction() < WRTune(TEXT("swampCreatures.ritual.lowHealth"), 0.3f) && Cr->HasTag(WRTags::Status_Bleeding);
			if (!Cr->IsUnconscious() && !bLow) continue;
			OutTarget = Cr->GetDisplayLabel();
			OutVerb = FString::Printf(TEXT("Blood rite (hold still %.0f s)"), WRTune(TEXT("swampCreatures.ritual.seconds"), 4.f));
			return true;
		}
	}
	return false;
}

bool WRSwamp::ClientInteract(AWRPlayerController* PC)
{
	using namespace WRSwampSys;
	const AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!Me || !WRSwamp::IsLive()) return false;
	FString T0, V0;
	if (UWRSwampWorld::PlankPrompt(PC, T0, V0)) { PC->SendCommand(TEXT("world swamp plank")); return true; }
	if (FWRSwampBoss::ClientInteract(PC)) return true;
	const FVector At = Me->GetActorLocation();
	const AWRSwampThing* Best = nullptr;
	float BestD = 1.0e9f;
	for (TActorIterator<AWRSwampThing> It(Me->GetWorld()); It; ++It)
	{
		if (It->bConcealed) continue;
		const float D = FVector::Dist(It->GetActorLocation(), At);
		if (D < It->Reach && D < BestD) { BestD = D; Best = *It; }
	}
	if (Best)
	{
		const FVector L = Best->GetActorLocation();
		PC->SendCommand(FString::Printf(TEXT("world swamp use %.1f %.1f %.1f"), L.X, L.Y, L.Z));
		return true;
	}
	FString T1, V1;
	if (Prompt(PC, T1, V1))
	{
		PC->SendCommand(TEXT("world swamp rite"));
		return true;
	}
	return false;
}

FString WRSwamp::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	using namespace WRSwampSys;
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	UWorld* World = Me ? Me->GetWorld() : nullptr;
	if (!Me || !World || World->GetNetMode() == NM_Client || !WRSwamp::IsLive()) return TEXT("Not here");
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString Verb = Arg(2);
	if (Verb == TEXT("plank")) return UWRSwampWorld::PlankCommand(PC);
	if (Verb == TEXT("boss"))
	{
		TArray<FString> Sub;
		Sub.Add(TEXT("swampboss"));
		for (int32 i = 3; i < Args.Num(); ++i) Sub.Add(Args[i]);
		return FWRSwampBoss::Command(PC, Sub);
	}
	if (Verb == TEXT("use"))
	{
		const FVector At(FCString::Atof(*Arg(3)), FCString::Atof(*Arg(4)), FCString::Atof(*Arg(5)));
		for (TActorIterator<AWRSwampThing> It(World); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), At) < 60.f && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < It->Reach * 1.5f) return It->Use(PC);
		}
		return TEXT("Nothing here");
	}
	if (Verb == TEXT("rite"))
	{
		AWRCreature* Best = nullptr;
		float BestD = WRTune(TEXT("swampCreatures.ritual.reach"), 450.f) * 1.5f;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (It->CreatureId != TEXT("murkjaw") || It->IsTamed() || !It->IsAlive()) continue;
			const float D = FVector::Dist(It->MouthLocation(), Me->GetActorLocation());
			if (D < BestD) { BestD = D; Best = *It; }
		}
		FString Msg;
		if (Best && FWRSwampAI::Feed(Best, PC, Msg)) return Msg;
		return TEXT("It won't take your blood now");
	}
#if !UE_BUILD_SHIPPING
	if (Verb == TEXT("dev"))
	{
		// world swamp dev coven | dig | candle (test helpers for the people who playtest it)
		UWRSwampSystems* Sys = UWRSwampSystems::Get(World);
		const FString What = Arg(3);
		FVector To;
		if (What == TEXT("coven") && AWRSwampCoven::RoomFloorAt(TEXT("drowned_lane"), To)) { Travel(PC, To, 0.f, TEXT("swamp.dive")); return TEXT("Down into the Sunken Coven"); }
		if (What == TEXT("dig") && Sys && Sys->Chimney.IsValid())
		{
			Sys->ChimneyFallsAtHours = Hours(World) + H(TEXT("coven.chimneyHours"), 72.f);
			Sys->Chimney->SetState(false, false, false);
			if (Sys->ChimneyIn.IsValid()) Sys->ChimneyIn->SetState(false, false, false);
			return TEXT("The chimney is open");
		}
		if (What == TEXT("candle") && Sys && Sys->Candle.IsValid()) { Travel(PC, AWRSwampCandle::PointAt(0.f) - FVector(0.f, 0.f, 120.f), 0.f, NAME_None); return TEXT("At the candle path's start (it lights at night)"); }
		return TEXT("world swamp dev coven | dig | candle");
	}
#endif
	return TEXT("?");
}

// ================================================================================================ building: the bloomery, the stilt foundation

void WRSwamp::BloomeryBlocks(const FTransform& PieceToWorld, TArray<FWRLookBlock>& Out)
{
	using namespace WRSwampSys;
	// A bog-iron bloomery as the Coven smiths built them (cook 2.19; the real thing: a clay shaft furnace on a stone
	// footing, a tapping arch at the base for the slag, a clay tuyere fed by hide pot-bellows). Origin at its foot,
	// +X the tapping arch, the bellows on the -Y side where the smith kneels.
	FKit K(PieceToWorld, Out);
	const FSurf SwsClay = FSurf::Stone(Col(128, 78, 50) * 0.55f, 0.93f);
	const FSurf Fired = FSurf::Stone(Col(150, 92, 58) * 0.5f, 0.9f);
	const FSurf Soot = FSurf::Stone(Col(30, 24, 20) * 0.6f, 0.95f);
	const FSurf Ember = FSurf::Lit(FLinearColor(1.f, 0.36f, 0.08f), 1.3f);
	const FSurf Slag = FSurf::Glaze(FLinearColor(0.025f, 0.03f, 0.04f), 0.18f);
	const FSurf Hide = Skin();
	// The footing: a ring of flat fieldstones bedded in the mud, a clay apron over them.
	for (int32 i = 0; i < 11; ++i)
	{
		const float A = 360.f / 11.f * i;
		K.Add(Stone(0.8f + 0.1f * (i % 3)), FVector(46.f, 30.f, 16.f), FRotator(0.f, A, 0.f).Vector() * 74.f + FVector(0, 0, 8.f), FRotator(0.f, A + 90.f, 0.f));
	}
	K.Add(SwsClay, FVector(150.f, 150.f, 10.f), FVector(0, 0, 14.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	// The shaft: hand-built rings of clay, thick at the foot and narrowing to the mouth, darker and sooted near the top;
	// the pressed seams between the courses stand proud.
	const int32 Courses = 8;
	for (int32 c = 0; c < Courses; ++c)
	{
		const float T = float(c) / (Courses - 1);
		const float Diam = FMath::Lerp(112.f, 66.f, T);
		const float Z = 22.f + c * 19.f;
		K.Add(c >= Courses - 2 ? Soot : (c % 2 ? SwsClay : Fired), FVector(Diam, Diam, 20.f), FVector(0, 0, Z + 10.f), FRotator(0.f, c * 23.f, 0.f), EWRLookMesh::Cylinder);
		if (c > 0) K.Add(c >= Courses - 2 ? Soot : SwsClay, FVector(Diam + 5.f, Diam + 5.f, 3.f), FVector(0, 0, Z), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	}
	// The mouth: a thick lip, and the charge inside burning low and orange under the peat.
	const float Top = 22.f + Courses * 19.f;
	K.Add(Soot, FVector(74.f, 74.f, 8.f), FVector(0, 0, Top + 2.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	K.Add(Ember, FVector(44.f, 44.f, 4.f), FVector(0, 0, Top - 6.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	for (int32 p = 0; p < 4; ++p) K.Add(SwsPeatS(0.6f), FVector(16.f, 10.f, 6.f), FVector(FMath::Cos(p * 1.6f) * 12.f, FMath::Sin(p * 1.6f) * 12.f, Top - 2.f), FRotator(10.f, p * 40.f, 0.f));
	// The tapping arch (+X): clay voussoirs round an opening at the foot, the bloom glowing inside, a slag runnel out to
	// a glassy black pool, and the spent slag heaped beside it.
	K.Frame = FTransform(FRotator(0.f, 90.f, 0.f), FVector(54.f, 0.f, 0.f));
	K.Ring(Fired, FVector(0.f, 0.f, 26.f), 20.f, 12.f, 18.f, 7, 0.f, 180.f, 1.f);
	K.Frame = FTransform::Identity;
	K.Add(Ember, FVector(10.f, 34.f, 22.f), FVector(50.f, 0, 30.f));
	K.Add(Soot, FVector(12.f, 44.f, 6.f), FVector(58.f, 0, 17.f));
	for (const float S : { -1.f, 1.f }) K.Add(SwsClay, FVector(70.f, 10.f, 12.f), FVector(96.f, S * 14.f, 12.f));
	K.Add(Slag, FVector(70.f, 18.f, 4.f), FVector(96.f, 0, 8.f));
	K.Add(Slag, FVector(60.f, 52.f, 5.f), FVector(146.f, 6.f, 4.f), FRotator(0.f, 12.f, 0.f), EWRLookMesh::Cylinder);
	for (int32 h = 0; h < 7; ++h) K.Add(FSurf::Glaze(FLinearColor(0.04f, 0.04f, 0.05f), 0.35f), FVector(26.f - h * 2.f, 22.f, 14.f), FVector(110.f + (h % 3) * 14.f, 62.f + (h / 3) * 12.f, 7.f + (h / 3) * 9.f), FRotator(h * 17.f, h * 41.f, h * 9.f));
	// The tuyere (-Y): a clay pipe through the wall a hand above the hearth, sloping down into the fire.
	K.Rod(Fired, FVector(0.f, -44.f, 34.f), FVector(0.f, -112.f, 46.f), 16.f);
	K.Add(SwsClay, FVector(24.f, 24.f, 10.f), FVector(0.f, -112.f, 46.f), FRotator(90.f, 0.f, 90.f), EWRLookMesh::Cylinder);
	// Two hide pot-bellows: clay pots with goat hide tied over the mouth and a pumping stick from each hide; wooden pipes
	// from the pots' sides join at the tuyere's end. The smith works them one hand each.
	for (const float S : { -1.f, 1.f })
	{
		const FVector Pot(S * 34.f, -168.f, 0.f);
		K.Add(Fired, FVector(52.f, 52.f, 30.f), Pot + FVector(0, 0, 15.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(SwsClay, FVector(56.f, 56.f, 5.f), Pot + FVector(0, 0, 30.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Hide, FVector(54.f, 54.f, 9.f), Pot + FVector(0, 0, 36.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Add(Rope(), FVector(57.f, 57.f, 3.f), Pot + FVector(0, 0, 33.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
		K.Rod(Wood(0.9f), Pot + FVector(0, 0, 40.f), Pot + FVector(S * 6.f, 4.f, 104.f), 4.f);
		K.Rod(Rope(), Pot + FVector(S * 6.f, 4.f, 104.f), Pot + FVector(S * 6.f, 4.f, 112.f), 5.f);
		K.Rod(Wood(0.8f), Pot + FVector(-S * 22.f, 20.f, 14.f), FVector(0.f, -118.f, 44.f), 8.f);
		K.Rod(Rope(), Pot + FVector(-S * 22.f, 20.f, 14.f), Pot + FVector(-S * 26.f, 24.f, 14.f), 10.f);
	}
	// What it eats: a withy basket of rust-red bog iron, and a stack of cut peat.
	K.Add(FSurf::Wood(Col(110, 86, 50) * 0.45f), FVector(54.f, 54.f, 34.f), FVector(-96.f, 62.f, 17.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);
	for (int32 n = 0; n < 8; ++n) K.Add(FSurf::Stone(RustC * 0.55f, 0.95f), FVector(14.f, 12.f, 10.f), FVector(-96.f + FMath::Cos(n * 0.8f) * 14.f, 62.f + FMath::Sin(n * 0.8f) * 14.f, 34.f + (n % 2) * 6.f), FRotator(n * 21.f, n * 47.f, 0.f));
	for (int32 r = 0; r < 3; ++r) for (int32 b = 0; b < 3 - r; ++b)
	{
		K.Add(SwsPeatS(0.85f + 0.05f * b), FVector(32.f, 16.f, 12.f), FVector(-96.f, -54.f + b * 18.f + r * 9.f, 6.f + r * 12.f), FRotator(0.f, 90.f, 0.f));
	}
}

void WRSwamp::StiltFoundationBlocks(const FTransform& PieceToWorld, TArray<FWRLookBlock>& Out)
{
	using namespace WRSwampSys;
	// The swamp's foundation (cook 2.19): nine bald-cypress posts driven a metre into the mud under up to 2 m of water
	// (cypress never rots), X-braced on every side, girders and joists on the posts, a deck of split cypress on top at
	// the same height as any foundation's (+92). The posts run long so they always meet the bottom.
	FKit K(PieceToWorld, Out);
	const FSurf Post = FSurf::Wood(Col(54, 44, 34) * 0.5f);
	const FSurf Beam = FSurf::Wood(Col(70, 56, 40) * 0.5f);
	const FSurf Deck = FSurf::Wood(Col(92, 76, 54) * 0.5f);
	const FSurf Wet = FSurf::Glaze(Col(30, 28, 22) * 0.5f, 0.3f);
	const float Corner = 180.f, Low = -H(TEXT("stilt.reach"), 500.f) + 30.f;
	for (const float X : { -Corner, 0.f, Corner }) for (const float Y : { -Corner, 0.f, Corner })
	{
		K.Rod(Post, FVector(X, Y, Low), FVector(X, Y, 66.f), 26.f);
		K.Spike(Post, FVector(X, Y, Low), -FVector::UpVector, 26.f, 30.f);
		K.Add(Wet, FVector(28.f, 28.f, 90.f), FVector(X, Y, -40.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);   // the slick waterline band
		K.Add(Rope(), FVector(30.f, 30.f, 6.f), FVector(X, Y, 50.f), FRotator::ZeroRotator, EWRLookMesh::Cylinder);    // a lashing under the girder
	}
	// X-bracing on the four sides, pegged to the posts.
	for (int32 Side = 0; Side < 4; ++Side)
	{
		K.Frame = FTransform(FRotator(0.f, 90.f * Side, 0.f));
		for (const float X0 : { -Corner, 0.f })
		{
			K.Rod(Beam, FVector(X0, -Corner, -260.f), FVector(X0 + Corner, -Corner, 30.f), 12.f);
			K.Rod(Beam, FVector(X0 + Corner, -Corner, -260.f), FVector(X0, -Corner, 30.f), 12.f);
		}
		K.Add(Beam, FVector(Corner * 2.f + 30.f, 16.f, 20.f), FVector(0.f, -Corner, 62.f));    // the rim girder
		K.Add(Deck, FVector(Corner * 2.f + 40.f, 6.f, 14.f), FVector(0.f, -Corner - 14.f, 84.f));   // the fascia board
	}
	K.Frame = FTransform::Identity;
	K.Add(Beam, FVector(Corner * 2.f + 30.f, 16.f, 20.f), FVector(0.f, 0.f, 62.f));
	for (const float X : { -120.f, -60.f, 60.f, 120.f }) K.Add(Beam, FVector(12.f, Corner * 2.f + 20.f, 14.f), FVector(X, 0.f, 79.f));   // joists
	// The deck: split boards across, each a little different, small gaps between.
	const int32 Boards = 13;
	for (int32 b = 0; b < Boards; ++b)
	{
		const float X = -196.f + (b + 0.5f) * (392.f / Boards);
		K.Add(Deck.Dim(0.9f + 0.2f * K.Rand(FVector(X, 0, 0), 3)), FVector(392.f / Boards - 2.f, 396.f, 5.f), FVector(X, 0.f, 89.5f), FRotator(0.f, 0.f, K.Jit(FVector(X, 0, 0), 5) * 0.6f));
	}
}

float WRSwamp::StiltBaseZ(const UObject* WorldContext, FName PieceId, const FVector& At, float BaseZ)
{
	using namespace WRSwampSys;
	if (PieceId != TEXT("stilt_foundation")) return BaseZ;
	const UWRWaterSubsystem* W = UWRWaterSubsystem::Get(WorldContext);
	const FWRSwampData& D = FWRSwampData::Get();
	const float Surface = W && W->HasWater() ? W->GetSurfaceZ() : D.bLoaded ? D.SeaZ : -1.0e9f;
	// Over water the deck stands clear of the surface on its posts; on dry ground it sits like any foundation.
	return FMath::Max(BaseZ, Surface + H(TEXT("stilt.clearance"), 40.f) - H(TEXT("stilt.deckTop"), 92.f));
}

FString WRSwamp::StiltCheck(const UObject* WorldContext, FName PieceId, const FVector& At)
{
	using namespace WRSwampSys;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (PieceId != TEXT("stilt_foundation") || !World) return FString();
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRStiltGround), false);
	if (!World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 100.f), At - FVector(0, 0, H(TEXT("stilt.reach"), 500.f) + 200.f), ECC_WorldStatic, Q))
	{
		return TEXT("too deep: the posts can't reach the bottom");
	}
	const UWRWaterSubsystem* W = UWRWaterSubsystem::Get(WorldContext);
	const float Surface = W && W->HasWater() ? W->GetSurfaceZ() : -1.0e9f;
	if (Surface - Hit.ImpactPoint.Z > H(TEXT("stilt.maxDepth"), 200.f)) return TEXT("too deep: stilts stand in up to 2 m of water");
	if (At.Z + H(TEXT("stilt.deckTop"), 92.f) - Hit.ImpactPoint.Z > H(TEXT("stilt.reach"), 500.f)) return TEXT("too high above the bottom for the posts");
	return FString();
}

float WRSwamp::GroundReach(FName PieceId, float Default)
{
	return PieceId == TEXT("stilt_foundation") ? WRSwampSys::H(TEXT("stilt.reach"), 500.f) + 60.f : Default;
}
