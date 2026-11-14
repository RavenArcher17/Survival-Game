// Survival, FULL-GAME 6.1: rest and Rested, comfort, darkness and lights, weather, corruption (see WRSurvival.h and
// showcase-notes cooks/build-survival.md). Numbers: tuning rest / rested / comfort / darkness / lights / corruption,
// and Data/weather.json.
#include "WRSurvival.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBeds.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

FWROnRest UWRSurvivalComponent::OnRest;

// ---------------------------------------------------------------- where things are

FName WRBiome::At(const UObject* WorldContext, const FVector& Pos)
{
	// The island's ridge: the same split the sky, the sound and the fishing use (sky.biomeCentre).
	return Pos.X < WRTune(TEXT("sky.biomeCentre"), 0.f) ? FName(TEXT("volcanic")) : FName(TEXT("tundra"));
}

bool WRBiome::IsUnderground(const UObject* WorldContext, const FVector& Pos)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	// AWRCave's local layout (BuildShell): the chamber x/y -900..900, 0..700 high; the entrance tunnel runs out along
	// +X to the mouth at ~2300. Its inner part (past darkness.caveMouth from the chamber) is as dark as the chamber.
	const float Mouth = WRTune(TEXT("darkness.caveMouth"), 1700.f);
	for (TActorIterator<AWRCave> It(World); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), Pos) > FMath::Square(4000.f)) continue;
		const FVector L = It->GetActorTransform().InverseTransformPosition(Pos);
		if (L.Z < -400.f || L.Z > 1000.f || FMath::Abs(L.Y) > 1000.f) continue;
		if (L.X >= -1000.f && L.X <= 1000.f) return true;
		if (L.X > 1000.f && L.X < Mouth && FMath::Abs(L.Y) < 600.f) return true;
	}
	return false;
}

bool WRBiome::IsCovered(const UObject* WorldContext, const FVector& Pos, float Height, const AActor* Ignore)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || Height <= 0.f)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRCovered), false, Ignore);
	return World->LineTraceSingleByChannel(Hit, Pos, Pos + FVector(0.f, 0.f, Height), ECC_WorldStatic, Query);
}

bool WRBiome::IsUnderBuiltRoof(const UObject* WorldContext, const FVector& Pos, float Height, const AActor* Ignore)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	TArray<FHitResult> Hits;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRRoof), false, Ignore);
	World->LineTraceMultiByChannel(Hits, Pos, Pos + FVector(0.f, 0.f, Height), ECC_Visibility, Query);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Pos, Pos + FVector(0.f, 0.f, Height), ECC_WorldStatic, Query))
	{
		Hits.Add(Hit);
	}
	for (const FHitResult& H : Hits)
	{
		const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(H.GetActor());
		if (Piece && !Piece->bGhost && Piece != Ignore)
		{
			const FWRPieceDef* Def = Piece->GetDef();
			if (Def && (Def->Shape == TEXT("roof") || Def->Shape == TEXT("floor") || Def->Shape == TEXT("foundation") || Def->Shape == TEXT("gable")))
			{
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------- weather data

namespace WRSurvivalDetail
{
	TArray<FName> Names(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FName> Out;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List) Out.Add(FName(*V->AsString()));
		}
		return Out;
	}

	FVector2D Pair(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FVector2D Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, List) && List->Num() == 2)
		{
			return FVector2D((*List)[0]->AsNumber(), (*List)[1]->AsNumber());
		}
		return Fallback;
	}

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = Fallback;
		if (O.IsValid()) O->TryGetNumberField(Field, V);
		return float(V);
	}

	AWRGameState* MutableState(const UObject* WorldContext)
	{
		return const_cast<AWRGameState*>(AWRGameState::Get(WorldContext));
	}

	/** Lights: tuning lights.<id> (radius cm, ward, halves); a missing row lights nothing. */
	float LightRadius(FName Id) { return FWRTuning::Get().Num(FString::Printf(TEXT("lights.%s.radius"), *Id.ToString()), 0.f); }
	bool LightFlag(FName Id, const TCHAR* Flag) { return FWRTuning::Get().Num(FString::Printf(TEXT("lights.%s.%s"), *Id.ToString(), Flag), 0.f) > 0.f; }
	FName LightKind(FName ItemId)
	{
		const FString K = FWRTuning::Get().Str(FString::Printf(TEXT("lights.%s.kind"), *ItemId.ToString()), FString());
		return K.IsEmpty() ? NAME_None : FName(*K);
	}
}

FWRWeatherDefs& FWRWeatherDefs::Get()
{
	static FWRWeatherDefs Instance;
	if (!Instance.bLoaded)
	{
		Instance.Load();
	}
	return Instance;
}

void FWRWeatherDefs::Load()
{
	using namespace WRSurvivalDetail;
	bLoaded = true;
	Biomes.Reset();
	const TSharedPtr<FJsonObject> File = FWRTuning::LoadDataFile(TEXT("weather.json"));
	if (!File.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("WEATHER Data/weather.json missing: no weather"));
		return;
	}
	RollMinutes = Num(File, TEXT("rollMinutes"), 30.f);
	WarnSeconds = Num(File, TEXT("warnSeconds"), 60.f);
	RampSeconds = Num(File, TEXT("rampSeconds"), 20.f);
	FadeSeconds = Num(File, TEXT("fadeSeconds"), 30.f);
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!File->TryGetArrayField(TEXT("biomes"), List))
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& BV : *List)
	{
		const TSharedPtr<FJsonObject> B = BV->AsObject();
		if (!B.IsValid()) continue;
		FWRBiomeSurvival& Row = Biomes.AddDefaulted_GetRef();
		Row.Biome = FName(*B->GetStringField(TEXT("biome")));
		B->TryGetStringField(TEXT("name"), Row.Name);
		B->TryGetBoolField(TEXT("darkNights"), Row.bDarkNights);
		B->TryGetBoolField(TEXT("underground"), Row.bUnderground);
		const TSharedPtr<FJsonObject>* Corr = nullptr;
		if (B->TryGetObjectField(TEXT("corruption"), Corr))
		{
			Row.bCorrupt = true;
			FString Path;
			if ((*Corr)->TryGetStringField(TEXT("path"), Path)) Row.CorruptionPath = FName(*Path);
		}
		const TArray<TSharedPtr<FJsonValue>>* Kinds = nullptr;
		if (!B->TryGetArrayField(TEXT("kinds"), Kinds)) continue;
		for (const TSharedPtr<FJsonValue>& KV : *Kinds)
		{
			const TSharedPtr<FJsonObject> K = KV->AsObject();
			if (!K.IsValid()) continue;
			FWRWeatherKind& W = Row.Kinds.AddDefaulted_GetRef();
			W.Id = FName(*K->GetStringField(TEXT("id")));
			K->TryGetStringField(TEXT("name"), W.Name);
			K->TryGetStringField(TEXT("coming"), W.Coming);
			K->TryGetStringField(TEXT("does"), W.Does);
			W.Chance = Num(K, TEXT("chance"), 0.f);
			W.Minutes = Pair(K, TEXT("minutes"), FVector2D(10.f, 20.f));
			K->TryGetBoolField(TEXT("dayOnly"), W.bDayOnly);
			K->TryGetBoolField(TEXT("nightOnly"), W.bNightOnly);
			const TSharedPtr<FJsonObject>* E = nullptr;
			if (K->TryGetObjectField(TEXT("effects"), E))
			{
				const TSharedPtr<FJsonObject>& F = *E;
				W.Vision = Num(F, TEXT("vision"), 0.f);
				W.ThirstMult = Num(F, TEXT("thirstMult"), 1.f);
				F->TryGetBoolField(TEXT("sunOnly"), W.bSunOnly);
				W.StaminaRegenMult = Num(F, TEXT("staminaRegenMult"), 1.f);
				W.RunMult = Num(F, TEXT("runMult"), 1.f);
				W.LightRadiusMult = Num(F, TEXT("lightRadiusMult"), 1.f);
				W.CorruptionMult = Num(F, TEXT("corruptionMult"), 1.f);
				W.BurnMult = Num(F, TEXT("burnMult"), 1.f);
				W.SwimStaminaMult = Num(F, TEXT("swimStaminaMult"), 1.f);
				W.AggroMult = Num(F, TEXT("aggroMult"), 1.f);
				W.AggroCreatures = Names(F, TEXT("aggroCreatures"));
				F->TryGetBoolField(TEXT("cropsPause"), W.bCropsPause);
				F->TryGetBoolField(TEXT("watersCrops"), W.bWatersCrops);
				const TSharedPtr<FJsonObject>* S = nullptr;
				if (F->TryGetObjectField(TEXT("strikes"), S))
				{
					W.bStrikes = true;
					W.StrikeMinZ = Num(*S, TEXT("minAltitude"), 0.f);
					W.StrikeEvery = Pair(*S, TEXT("every"), FVector2D(20.f, 40.f));
					W.StrikeWarn = Num(*S, TEXT("warnSeconds"), 1.f);
					W.StrikeDamage = Num(*S, TEXT("damage"), 60.f);
					W.StrikeStagger = Num(*S, TEXT("staggerSeconds"), 0.8f);
					W.StrikeRadius = Num(*S, TEXT("radius"), 300.f);
					W.StrikeCell = Num(*S, TEXT("cell"), 6000.f);
				}
				const TSharedPtr<FJsonObject>* G = nullptr;
				if (F->TryGetObjectField(TEXT("gust"), G))
				{
					W.bGust = true;
					W.GustEvery = Pair(*G, TEXT("every"), FVector2D(8.f, 15.f));
					W.GustWarn = Num(*G, TEXT("warnSeconds"), 1.f);
					W.GustPush = Num(*G, TEXT("push"), 900.f);
					W.GustMinZ = Num(*G, TEXT("minAltitude"), -1.0e7f);
				}
			}
			const TSharedPtr<FJsonObject>* C = nullptr;
			if (K->TryGetObjectField(TEXT("counters"), C))
			{
				W.RoofWithin = Num(*C, TEXT("roofWithin"), 0.f);
				W.Resist = Names(*C, TEXT("resist"));
				const TSharedPtr<FJsonObject>* SetsObj = nullptr;
				if ((*C)->TryGetObjectField(TEXT("sets"), SetsObj))
				{
					for (const auto& Pair : (*SetsObj)->Values) W.Sets.Add(FName(*Pair.Key), FMath::Max(1, int32(Pair.Value->AsNumber())));
				}
				W.PathNodes = Names(*C, TEXT("pathNodes"));
				W.Items = Names(*C, TEXT("items"));
			}
			const TSharedPtr<FJsonObject>* L = nullptr;
			if (K->TryGetObjectField(TEXT("look"), L))
			{
				FString S;
				if ((*L)->TryGetStringField(TEXT("fx"), S)) W.Fx = FName(*S);
				W.FxRate = Num(*L, TEXT("fxRate"), 1.f);
				if ((*L)->TryGetStringField(TEXT("baseFx"), S)) W.BaseFx = FName(*S);
				W.BaseRate = Num(*L, TEXT("baseRate"), 1.f);
				W.FogDensity = Num(*L, TEXT("fogDensity"), 0.f);
				const TArray<TSharedPtr<FJsonValue>>* Col = nullptr;
				if ((*L)->TryGetArrayField(TEXT("fogColor"), Col) && Col->Num() >= 3)
				{
					// sRGB 0-255, as the mockups and the sky's other colours are written.
					W.FogColor = FLinearColor(FColor(uint8((*Col)[0]->AsNumber()), uint8((*Col)[1]->AsNumber()), uint8((*Col)[2]->AsNumber())));
				}
				W.SkyDim = Num(*L, TEXT("skyDim"), 0.f);
				if ((*L)->TryGetStringField(TEXT("glyph"), S)) W.Glyph = FName(*S);
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("WEATHER %d biome rows loaded (roll every %.0f min, %.0f s warning)"), Biomes.Num(), RollMinutes, WarnSeconds);
}

const FWRBiomeSurvival* FWRWeatherDefs::Biome(FName Id) const
{
	return Biomes.FindByPredicate([Id](const FWRBiomeSurvival& B) { return B.Biome == Id; });
}

const FWRWeatherKind* FWRWeatherDefs::Kind(FName BiomeId, FName KindId) const
{
	const FWRBiomeSurvival* B = Biome(BiomeId);
	return B ? B->Kinds.FindByPredicate([KindId](const FWRWeatherKind& K) { return K.Id == KindId; }) : nullptr;
}

float FWRWeatherNow::Strength(float Now) const
{
	if (!IsOn(Now))
	{
		return 0.f;
	}
	const FWRWeatherDefs& D = FWRWeatherDefs::Get();
	const float In = FMath::Clamp((Now - StartAt) / FMath::Max(1.f, D.RampSeconds), 0.f, 1.f);
	const float Out = FMath::Clamp((EndAt - Now) / FMath::Max(1.f, D.FadeSeconds), 0.f, 1.f);
	return FMath::Min(In, Out);
}

float FWRWeatherNow::Approach(float Now) const
{
	if (Kind.IsNone() || Now < WarnAt || Now >= EndAt)
	{
		return 0.f;
	}
	if (Now >= StartAt)
	{
		return FMath::Clamp((EndAt - Now) / FMath::Max(1.f, FWRWeatherDefs::Get().FadeSeconds), 0.f, 1.f);
	}
	return FMath::Clamp((Now - WarnAt) / FMath::Max(1.f, StartAt - WarnAt), 0.f, 1.f);
}

// ---------------------------------------------------------------- weather (server)

bool UWRWeatherSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRWeatherSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FWRWeatherDefs::Get().Load();   // a fresh read per world (the file may have been tuned between sessions)
}

TStatId UWRWeatherSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRWeatherSubsystem, STATGROUP_Tickables);
}

float UWRWeatherSubsystem::Now(const UObject* WorldContext)
{
	const AWRGameState* GS = AWRGameState::Get(WorldContext);
	if (GS)
	{
		return float(GS->GetServerWorldTimeSeconds());
	}
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.f;
}

const FWRWeatherNow* UWRWeatherSubsystem::For(const UObject* WorldContext, FName Biome)
{
	const AWRGameState* GS = AWRGameState::Get(WorldContext);
	return GS ? GS->Weather.FindByPredicate([Biome](const FWRWeatherNow& W) { return W.Biome == Biome; }) : nullptr;
}

const FWRWeatherKind* UWRWeatherSubsystem::ActiveKind(const UObject* WorldContext, FName Biome, float* OutStrength)
{
	const FWRWeatherNow* W = For(WorldContext, Biome);
	const float S = W ? W->Strength(Now(WorldContext)) : 0.f;
	if (OutStrength) *OutStrength = S;
	return W && S >= 0.5f ? FWRWeatherDefs::Get().Kind(Biome, W->Kind) : nullptr;
}

void UWRWeatherSubsystem::Roll(FName Biome, float At)
{
	AWRGameState* GS = WRSurvivalDetail::MutableState(this);
	const FWRBiomeSurvival* Row = FWRWeatherDefs::Get().Biome(Biome);
	if (!GS || !Row)
	{
		return;
	}
	FWRWeatherNow* Slot = GS->Weather.FindByPredicate([Biome](const FWRWeatherNow& W) { return W.Biome == Biome; });
	if (!Slot)
	{
		Slot = &GS->Weather.AddDefaulted_GetRef();
		Slot->Biome = Biome;
	}
	if (Slot->IsOn(At) || Slot->IsComing(At))
	{
		return;   // one weather at a time; the next roll comes after it
	}
	// One roll: walk the kinds' chances (a day-only kind can't come at night, and the other way round).
	const bool bNight = AWRGameState::IsNightIn(this);
	float Pick = FMath::FRand();
	for (const FWRWeatherKind& K : Row->Kinds)
	{
		if ((K.bDayOnly && bNight) || (K.bNightOnly && !bNight)) continue;
		if (Pick < K.Chance)
		{
			const FWRWeatherDefs& D = FWRWeatherDefs::Get();
			Slot->Kind = K.Id;
			Slot->WarnAt = At;
			Slot->StartAt = At + D.WarnSeconds;
			Slot->EndAt = Slot->StartAt + FMath::FRandRange(K.Minutes.X, K.Minutes.Y) * 60.f;
			GS->ForceNetUpdate();
			UE_LOG(LogWildreach, Log, TEXT("WEATHER %s: %s in %.0f s for %.1f min"), *Biome.ToString(), *K.Id.ToString(), D.WarnSeconds, (Slot->EndAt - Slot->StartAt) / 60.f);
			return;
		}
		Pick -= K.Chance;
	}
	if (!Slot->Kind.IsNone())
	{
		Slot->Kind = NAME_None;
		GS->ForceNetUpdate();
	}
}

void UWRWeatherSubsystem::Force(FName Biome, FName Kind, float Minutes)
{
	AWRGameState* GS = WRSurvivalDetail::MutableState(this);
	if (!GS || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}
	FWRWeatherNow* Slot = GS->Weather.FindByPredicate([Biome](const FWRWeatherNow& W) { return W.Biome == Biome; });
	if (!Slot)
	{
		Slot = &GS->Weather.AddDefaulted_GetRef();
		Slot->Biome = Biome;
	}
	const float T = Now(this);
	Slot->Kind = FWRWeatherDefs::Get().Kind(Biome, Kind) ? Kind : NAME_None;
	Slot->WarnAt = T;
	Slot->StartAt = T;
	Slot->EndAt = T + FMath::Max(0.5f, Minutes) * 60.f;
	NextRoll.Add(Biome, T + FWRWeatherDefs::Get().RollMinutes * 60.f);
	GS->ForceNetUpdate();
	UE_LOG(LogWildreach, Log, TEXT("WEATHER forced %s: %s for %.1f min"), *Biome.ToString(), *Slot->Kind.ToString(), Minutes);
}

void UWRWeatherSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !AWRGameState::Get(this) || WRTune(TEXT("weather.enabled"), 1.f) <= 0.f)
	{
		return;
	}
	const float T = Now(this);
	const FWRWeatherDefs& D = FWRWeatherDefs::Get();
	for (const FWRBiomeSurvival& B : D.All())
	{
		if (B.Kinds.Num() == 0) continue;
		float& Next = NextRoll.FindOrAdd(B.Biome, -1.f);
		if (Next < 0.f)
		{
			// The first roll of each biome comes at a random point of the first window, so they don't all turn at once.
			Next = T + FMath::FRandRange(0.15f, 1.f) * D.RollMinutes * 60.f;
			continue;
		}
		if (T >= Next)
		{
			Roll(B.Biome, T);
			Next = T + D.RollMinutes * 60.f;
		}
	}
	TickStrikes(DeltaTime);
}

void UWRWeatherSubsystem::TickStrikes(float DeltaTime)
{
	UWorld* World = GetWorld();
	const float T = Now(this);
	// Bolts whose warning is over: everyone still standing in the glow is hit (it can kill: no floors).
	for (int32 i = Strikes.Num() - 1; i >= 0; --i)
	{
		const FPendingStrike S = Strikes[i];
		if (T < S.LandsAt) continue;
		Strikes.RemoveAtSwap(i);
		const FWRWeatherKind* K = FWRWeatherDefs::Get().Kind(S.Biome, S.Kind);
		if (!K) continue;
		WRFx::Broadcast(World, TEXT("weather.strike"), S.At, FRotator::ZeroRotator, 1.f, TEXT("weather.strike"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (!It->IsAlive() || FVector::Dist2D(It->GetActorLocation(), S.At) > K->StrikeRadius || FMath::Abs(It->GetActorLocation().Z - S.At.Z) > 400.f) continue;
			FWRHit Bolt;
			Bolt.Damage = K->StrikeDamage;
			Bolt.Element = WRTags::Element_Lightning;
			Bolt.Origin = S.At + FVector(0.f, 0.f, 2000.f);
			Bolt.Impact = 3;
			Bolt.PoiseDamage = 999.f;
			Bolt.bUnblockable = true;
			Bolt.Tier = EWRHitTier::Heavy;
			UWRCombatLibrary::ApplyHit(*It, Bolt);
			UE_LOG(LogWildreach, Log, TEXT("WEATHER lightning struck %s"), *It->GetName());
		}
	}

	StrikeClock -= DeltaTime;
	if (StrikeClock > 0.f)
	{
		return;
	}
	StrikeClock = 1.f;
	// Each storm cell picks its highest exposed player when its timer comes round (BotW: the tallest conductor).
	TMap<FIntPoint, AWRCharacterBase*> Highest;
	TMap<FIntPoint, const FWRWeatherKind*> KindOf;
	TMap<FIntPoint, FName> BiomeOf;
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* P = *It;
		if (!P->IsAlive() || !P->GetPlayerState()) continue;
		const FName Biome = WRBiome::At(this, P->GetActorLocation());
		const FWRWeatherKind* K = ActiveKind(this, Biome);
		if (!K || !K->bStrikes || P->GetActorLocation().Z < K->StrikeMinZ) continue;
		const UWRSurvivalComponent* Surv = UWRSurvivalComponent::For(P);
		if (Surv && (Surv->IsWeatherCountered() || Surv->IsSheltered())) continue;
		const FIntPoint Cell(FMath::FloorToInt(P->GetActorLocation().X / K->StrikeCell), FMath::FloorToInt(P->GetActorLocation().Y / K->StrikeCell));
		AWRCharacterBase*& Best = Highest.FindOrAdd(Cell, nullptr);
		if (!Best || P->GetActorLocation().Z > Best->GetActorLocation().Z)
		{
			Best = P;
			KindOf.Add(Cell, K);
			BiomeOf.Add(Cell, Biome);
		}
	}
	for (const TPair<FIntPoint, AWRCharacterBase*>& C : Highest)
	{
		const FWRWeatherKind* K = KindOf.FindRef(C.Key);
		float& Next = NextStrikeInCell.FindOrAdd(C.Key, -1.f);
		if (Next < 0.f) { Next = T + FMath::FRandRange(K->StrikeEvery.X, K->StrikeEvery.Y); continue; }
		if (T < Next) continue;
		Next = T + FMath::FRandRange(K->StrikeEvery.X, K->StrikeEvery.Y);
		const FVector Feet = C.Value->GetActorLocation() - FVector(0.f, 0.f, C.Value->GetSimpleCollisionHalfHeight());
		Strikes.Add({ Feet, T + K->StrikeWarn, BiomeOf.FindRef(C.Key), K->Id });
		WRFx::Broadcast(C.Value, TEXT("weather.strikeWarn"), Feet, FRotator::ZeroRotator, K->StrikeRadius / 300.f, TEXT("weather.strikeWarn"));
	}
}

// ---------------------------------------------------------------- lights

UWRLightSubsystem* UWRLightSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRLightSubsystem>() : nullptr;
}

void UWRLightSubsystem::Refresh()
{
	using namespace WRSurvivalDetail;
	UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() - ScannedAt < 0.5)
	{
		return;
	}
	ScannedAt = World->GetTimeSeconds();
	Sources.Reset();
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FWRPieceDef* Def = It->bGhost ? nullptr : It->GetDef();
		// Lights, and anything else with a lights.<id> row (the campfire, the cooking pot's fire, the oven).
		if (!Def || (Def->Kind != TEXT("light") && LightRadius(Def->Id) <= 0.f)) continue;
		// Machines (coil lamps, greenhouse lights) light only while their power has them lit.
		if (Def->Kind == TEXT("machine") && !(It->Machine.Flags & WRMachineFlag::Lit)) continue;
		Sources.Add({ *It, LightRadius(Def->Id), LightFlag(Def->Id, TEXT("ward")), false });
	}
	for (TActorIterator<AWRHearth> It(World); It; ++It)
	{
		if (!It->IsBroken()) Sources.Add({ *It, LightRadius(TEXT("hearth")), LightFlag(TEXT("hearth"), TEXT("ward")), false });
	}
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		const FWRCreatureDef* Def = It->GetDef();
		if (Def && Def->Utility == TEXT("light") && It->IsAlive())
		{
			Sources.Add({ *It, LightRadius(TEXT("creature")), false, LightFlag(TEXT("creature"), TEXT("halves")) });
		}
	}
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		const UWRSurvivalComponent* S = It->GetPlayerState() ? UWRSurvivalComponent::For(*It) : nullptr;
		if (S && S->IsLightOn() && It->IsAlive())
		{
			Sources.Add({ *It, LightRadius(S->GetLightItem()), false, LightFlag(S->GetLightItem(), TEXT("halves")) });
		}
	}
}

bool UWRLightSubsystem::Any(const FVector& Pos, TFunctionRef<bool(const FSource&)> Want)
{
	Refresh();
	for (const FSource& S : Sources)
	{
		const AActor* A = S.Actor.Get();
		if (!A || S.Radius <= 0.f || !Want(S)) continue;
		// Corpse fog and the like shrink every light in their biome.
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(this, WRBiome::At(this, A->GetActorLocation()));
		const float R = S.Radius * (K ? K->LightRadiusMult : 1.f);
		if (FVector::DistSquared(A->GetActorLocation(), Pos) < FMath::Square(R)) return true;
	}
	return false;
}

bool UWRLightSubsystem::IsLit(const FVector& Pos) { return Any(Pos, [](const FSource&) { return true; }); }
bool UWRLightSubsystem::IsWarded(const FVector& Pos) { return Any(Pos, [](const FSource& S) { return S.bWard; }); }
bool UWRLightSubsystem::IsHalving(const FVector& Pos) { return Any(Pos, [](const FSource& S) { return S.bHalves; }); }

// ---------------------------------------------------------------- the survival component

UWRSurvivalComponent::UWRSurvivalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRSurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRSurvivalComponent, bSleeping);
	DOREPLIFETIME(UWRSurvivalComponent, SleepStartedAt);
	DOREPLIFETIME(UWRSurvivalComponent, SleepEndsAt);
	DOREPLIFETIME(UWRSurvivalComponent, LightItem);
	DOREPLIFETIME(UWRSurvivalComponent, bLightOn);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, RestedUntil, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, RestedMinutes, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, ComfortHere, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, bUnlit, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, Corruption, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, CorruptionFrom, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, WeatherBiome, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, BiomeHere, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, bWeatherCountered, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRSurvivalComponent, bSheltered, COND_OwnerOnly);
}

UWRSurvivalComponent* UWRSurvivalComponent::For(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	if (const APlayerState* PS = Cast<APlayerState>(Actor))
	{
		return PS->FindComponentByClass<UWRSurvivalComponent>();
	}
	const APawn* Pawn = Cast<APawn>(Actor);
	const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PS ? PS->FindComponentByClass<UWRSurvivalComponent>() : nullptr;
}

AWRPlayerState* UWRSurvivalComponent::GetPS() const
{
	return Cast<AWRPlayerState>(GetOwner());
}

AWRCharacterBase* UWRSurvivalComponent::GetPawnCharacter() const
{
	const AWRPlayerState* PS = GetPS();
	return PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
}

void UWRSurvivalComponent::Notify(const FString& Message) const
{
	const AWRPlayerState* PS = GetPS();
	if (AWRPlayerController* PC = PS ? Cast<AWRPlayerController>(PS->GetOwner()) : nullptr)
	{
		PC->ClientNotify(Message);
	}
}

void UWRSurvivalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	Clock += DeltaTime;
	if (Clock < 0.25f)
	{
		return;
	}
	const float Step = Clock;
	Clock = 0.f;
	AWRCharacterBase* Pawn = GetPawnCharacter();
	const float Now = UWRWeatherSubsystem::Now(this);
	if (!Pawn)
	{
		return;
	}
	if (LoadedRestedSeconds > 0.f)
	{
		// The save's Rested time starts counting again once the player is back in the world.
		RestedUntil = Now + LoadedRestedSeconds;
		LoadedRestedSeconds = 0.f;
	}
	if (!Pawn->IsAlive())
	{
		// Death ends a sleep and the Rested buff (Valheim); corruption stays (survival-crafting 2.5).
		if (bSleeping) Wake(FString());
		RestedUntil = 0.f;
		RestedMinutes = 0;
		return;
	}
	TickSleep(Pawn, Now);
	ComfortClock -= Step;
	if (ComfortClock <= 0.f)
	{
		ComfortClock = 2.f;
		TickComfort(Pawn);
	}
	TickWeather(Pawn, Now);
	TickLight(Pawn, Step);
	TickCorruption(Pawn, Step);
}

// ---------------------------------------------------------------- sleep and Rest

FString UWRSurvivalComponent::TryStartSleep(AWRBuildPiece* Bed)
{
	AWRCharacterBase* Pawn = GetPawnCharacter();
	if (!Pawn || !Bed || !Pawn->IsAlive())
	{
		return TEXT("You can't sleep now");
	}
	if (bSleeping)
	{
		return FString();
	}
	const UWRAttributeSet* Attr = Pawn->GetAttributes();
	const UCharacterMovementComponent* Move = Pawn->GetCharacterMovement();
	if (Move && Move->IsSwimming()) return TEXT("Not in the water");
	if (Pawn->HasTag(WRTags::State_Mounted)) return TEXT("Get down first");
	if (Attr && Attr->GetHunger() <= 0.f) return TEXT("Too hungry to sleep. Eat first");
	if (Attr && Attr->GetThirst() <= 0.f) return TEXT("Too thirsty to sleep. Drink first");
	// Valheim's rule: nothing that's after you close by.
	const float Near = WRTune(TEXT("rest.enemyRange"), 1500.f);
	for (TActorIterator<AWRCreature> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->IsAlive() && !It->IsTamed() && It->GetAggroTarget() == Pawn && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < Near)
		{
			return TEXT("You can't sleep with enemies close");
		}
	}

	// Lie down on the mattress (the bed's top), along the bed, the head to its head end (rest.pose* fit the clip).
	FVector Origin, Extent;
	Bed->GetActorBounds(true, Origin, Extent);
	const bool bRoll = WRBeds::IsBedroll(Bed);
	const FRotator BedRot(0.f, Bed->GetActorRotation().Yaw, 0.f);
	const float Along = WRTune(bRoll ? TEXT("rest.poseOffsetBedroll") : TEXT("rest.poseOffset"), bRoll ? -60.f : 70.f);
	const float Top = Bed->GetActorLocation().Z + WRTune(bRoll ? TEXT("rest.bedrollTop") : TEXT("rest.bedTop"), bRoll ? 6.f : 40.f);
	SleepSpot = Bed->GetActorLocation() + BedRot.RotateVector(FVector(Along, 0.f, 0.f));
	SleepSpot.Z = Top + Pawn->GetSimpleCollisionHalfHeight() + 2.f;
	const FRotator Facing(0.f, BedRot.Yaw + WRTune(bRoll ? TEXT("rest.poseYawBedroll") : TEXT("rest.poseYaw"), bRoll ? 0.f : 180.f), 0.f);
	Pawn->SetActorLocationAndRotation(SleepSpot, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* M = Pawn->GetCharacterMovement())
	{
		M->StopMovementImmediately();
		M->DisableMovement();
	}
	if (AController* C = Pawn->GetController())
	{
		C->SetControlRotation(Facing);
	}

	// Comfort is counted as you lie down (the room you chose).
	SleepComfort = FWRComfort::At(Bed);
	SleepBed = Bed;
	SleepHealth = Attr ? Attr->GetHealth() : 0.f;
	SleepStartedAt = UWRWeatherSubsystem::Now(this);
	SleepEndsAt = SleepStartedAt + WRTune(TEXT("rest.sleepSeconds"), 10.f);
	bSleeping = true;
	OnRep_Sleeping();
	WRSound::Broadcast(Pawn, TEXT("rest.lie"));
	UE_LOG(LogWildreach, Log, TEXT("REST %s lies down on their %s (comfort %d)"), *GetNameSafe(GetPS()), *Bed->PieceId.ToString(), SleepComfort);
	return FString();
}

void UWRSurvivalComponent::Wake(const FString& Why)
{
	if (!bSleeping)
	{
		return;
	}
	bSleeping = false;
	SleepBed.Reset();
	if (AWRCharacterBase* Pawn = GetPawnCharacter())
	{
		if (UCharacterMovementComponent* M = Pawn->GetCharacterMovement())
		{
			M->SetMovementMode(MOVE_Falling);
		}
	}
	OnRep_Sleeping();
	if (!Why.IsEmpty())
	{
		Notify(Why);
	}
	UE_LOG(LogWildreach, Log, TEXT("REST %s gets up%s%s"), *GetNameSafe(GetPS()), Why.IsEmpty() ? TEXT("") : TEXT(": "), *Why);
}

void UWRSurvivalComponent::TickSleep(AWRCharacterBase* Pawn, float Now)
{
	if (!bSleeping)
	{
		return;
	}
	const UWRAttributeSet* Attr = Pawn->GetAttributes();
	if (!SleepBed.IsValid() || SleepBed->IsActorBeingDestroyed()) { Wake(TEXT("Your bed is gone")); return; }
	if (Attr && Attr->GetHealth() < SleepHealth - 0.5f) { Wake(TEXT("Woken by a blow")); return; }
	if (Attr && (Attr->GetHunger() <= 0.f || Attr->GetThirst() <= 0.f)) { Wake(TEXT("Too hungry and thirsty to sleep on")); return; }
	if (FVector::Dist(Pawn->GetActorLocation(), SleepSpot) > WRTune(TEXT("rest.moveTolerance"), 100.f)) { Wake(FString()); return; }
	if (Pawn->IsSwinging()) { Wake(FString()); return; }
	if (Now >= SleepEndsAt)
	{
		FinishSleep(Pawn);
	}
}

void UWRSurvivalComponent::FinishSleep(AWRCharacterBase* Pawn)
{
	AWRBuildPiece* Bed = SleepBed.Get();
	const bool bRoll = WRBeds::IsBedroll(Bed);
	// The Rest (survival-crafting 2.2): Decay's lost max health back, corruption halved, the paths that hang off it.
	if (Pawn->Status)
	{
		Pawn->Status->RestoreDecay();
	}
	Corruption *= WRTune(TEXT("corruption.restKeeps"), 0.5f);
	const float Base = bRoll ? WRTune(TEXT("rested.bedrollMinutes"), 10.f) : WRTune(TEXT("rested.bedMinutes"), 20.f);
	const int32 Comfort = FMath::Clamp(SleepComfort, 0, FMath::RoundToInt(WRTune(TEXT("comfort.cap"), 20.f)));
	RestedMinutes = FMath::RoundToInt(Base + Comfort * WRTune(TEXT("rested.perComfort"), 1.f));
	RestedUntil = UWRWeatherSubsystem::Now(this) + RestedMinutes * 60.f;
	ComfortHere = Comfort;
	UE_LOG(LogWildreach, Log, TEXT("REST %s rested in their %s: %d min (comfort %d)"), *GetNameSafe(GetPS()), Bed ? *Bed->PieceId.ToString() : TEXT("bed"), RestedMinutes, Comfort);
	Wake(FString());
	WRSound::PlayOn(Pawn, TEXT("rest.wake"));
	OnRest.Broadcast(GetPS());
}

bool UWRSurvivalComponent::IsRested() const
{
	return RestedSecondsLeft() > 0.f;
}

float UWRSurvivalComponent::RestedSecondsLeft() const
{
	if (LoadedRestedSeconds > 0.f) return LoadedRestedSeconds;
	return FMath::Max(0.f, RestedUntil - UWRWeatherSubsystem::Now(this));
}

void UWRSurvivalComponent::OnRep_Sleeping()
{
	// The body on the bed and the owner's dimmed view: the hero reads bSleeping (AWRCharacter::TickSurvivalLook).
	AWRCharacterBase* Pawn = GetPawnCharacter();
	if (Pawn && Pawn->IsLocallyControlled() && Pawn->GetNetMode() == NM_Client)
	{
		if (UCharacterMovementComponent* M = Pawn->GetCharacterMovement())
		{
			if (bSleeping) { M->StopMovementImmediately(); M->DisableMovement(); }
			else M->SetMovementMode(MOVE_Falling);
		}
	}
}

// ---------------------------------------------------------------- comfort

int32 FWRComfort::At(const AActor* Bed)
{
	const UWorld* World = Bed ? Bed->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	const float Radius = WRTune(TEXT("comfort.radius"), 1200.f);
	const float RoofHeight = WRTune(TEXT("comfort.roofHeight"), 1500.f);
	TMap<FName, int32> Best;
	TSet<FName> Trophies;
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		const FWRPieceDef* Def = It->bGhost ? nullptr : It->GetDef();
		if (!Def || Def->Comfort <= 0 || FVector::Dist(It->GetActorLocation(), Bed->GetActorLocation()) > Radius) continue;
		// Indoors only (Valheim's "sheltered"): a built roof or floor straight above the piece.
		FVector Origin, Extent;
		It->GetActorBounds(true, Origin, Extent);
		if (!WRBiome::IsUnderBuiltRoof(World, FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z + 5.f), RoofHeight, *It)) continue;
		if (Def->ComfortGroup == TEXT("trophy"))
		{
			Trophies.Add(It->PieceId == TEXT("trophy_mount") ? FName(*It->GetName()) : It->PieceId);
			continue;
		}
		int32& B = Best.FindOrAdd(Def->ComfortGroup, 0);
		B = FMath::Max(B, Def->Comfort);
	}
	// The Hearth counts as a fireplace when it's this close.
	for (TActorIterator<AWRHearth> It(World); It; ++It)
	{
		if (!It->IsBroken() && FVector::Dist(It->GetActorLocation(), Bed->GetActorLocation()) <= Radius)
		{
			int32& B = Best.FindOrAdd(TEXT("fire"), 0);
			B = FMath::Max(B, FMath::RoundToInt(WRTune(TEXT("comfort.hearth"), 2.f)));
		}
	}
	int32 Total = FMath::Min(Trophies.Num(), FMath::RoundToInt(WRTune(TEXT("comfort.trophyMax"), 3.f)));
	for (const TPair<FName, int32>& G : Best) Total += G.Value;
	return FMath::Clamp(Total, 0, FMath::RoundToInt(WRTune(TEXT("comfort.cap"), 20.f)));
}

void UWRSurvivalComponent::TickComfort(AWRCharacterBase* Pawn)
{
	// The HUD's "Comfort N" while you stand near a bed you can sleep in.
	const AWRPlayerState* PS = GetPS();
	const FString Me = PS ? PS->GetPlayerName() : FString();
	const float Radius = WRTune(TEXT("comfort.radius"), 1200.f);
	const AWRBuildPiece* Nearest = nullptr;
	float Best = Radius;
	for (TActorIterator<AWRBuildPiece> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->bGhost || !WRBeds::IsBed(*It) || !It->CanAccess(Me)) continue;
		const float D = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
		if (D < Best) { Best = D; Nearest = *It; }
	}
	ComfortHere = Nearest ? FWRComfort::At(Nearest) : -1;
}

// ---------------------------------------------------------------- weather on the player

void UWRSurvivalComponent::TickWeather(AWRCharacterBase* Pawn, float Now)
{
	WThirst = WRegen = WRun = WBurn = WSwim = WCorruption = 1.f;
	const FVector At = Pawn->GetActorLocation();
	BiomeHere = WRBiome::At(this, At);
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(this, BiomeHere);
	bSheltered = WRBiome::IsUnderground(this, At) || (K && K->RoofWithin > 0.f && WRBiome::IsCovered(this, At, K->RoofWithin, Pawn));
	WeatherBiome = K ? BiomeHere : NAME_None;
	bool bCountered = false;
	if (K)
	{
		// A heat wave only bites in direct sun: anything between you and the sun is shade.
		if (K->bSunOnly && !bSheltered)
		{
			const FVector Sun = FRotator(-FMath::Max(10.f, WRTune(TEXT("weather.sunElevation"), 55.f)), WRTune(TEXT("weather.sunYaw"), 180.f), 0.f).Vector();
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(WRSun), false, Pawn);
			bSheltered = !AWRGameState::Get(this) || AWRGameState::IsNightIn(this)
				|| Pawn->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 60.f), At + FVector(0.f, 0.f, 60.f) - Sun * 20000.f, ECC_WorldStatic, Query);
		}
		const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Pawn);
		for (const FName& R : K->Resist) bCountered |= FWRBiomeHazards::Resists(Pawn, R);
		if (Inv)
		{
			// A biome armour set worn to its count (gear sets: Rimeguard holds a blizzard off at 2 pieces).
			for (const TPair<FName, int32>& Set : K->Sets) bCountered |= Inv->SetPieces(Set.Key) >= Set.Value;
			// Carried, or worn (a grounding charm in the talisman slot, a sand veil on the head).
			for (const FName& Item : K->Items)
			{
				bCountered |= Inv->CountOf(Item) > 0;
				for (const EWREquipSlot Slot : WRGearSlots::All) bCountered |= !Inv->GetEquipped(Slot).IsEmpty() && Inv->GetEquipped(Slot).ItemId == Item;
			}
		}
		for (const FName& Node : K->PathNodes)
		{
			FString P, N;
			if (Node.ToString().Split(TEXT("."), &P, &N)) bCountered |= UWRPathComponent::HasNode(Pawn, FName(*P), FName(*N));
		}
		if (!bCountered && !bSheltered)
		{
			WThirst = K->ThirstMult;
			WRegen = K->StaminaRegenMult;
			WRun = K->RunMult;
			WSwim = K->SwimStaminaMult;
			WCorruption = K->CorruptionMult;
		}
		// Rain soaks a torch whatever you wear; a roof keeps it dry.
		if (!bSheltered)
		{
			WBurn = K->BurnMult;
		}
		// Gusts: a whistle, then a shove along the storm's wind (one direction per storm, from when it was announced).
		const FWRWeatherNow* W = UWRWeatherSubsystem::For(this, BiomeHere);
		if (K->bGust && W && !bCountered && !bSheltered && At.Z >= K->GustMinZ)
		{
			if (NextGustAt < 0.f)
			{
				NextGustAt = Now + FMath::FRandRange(K->GustEvery.X, K->GustEvery.Y);
				bGustWarned = false;
			}
			else if (!bGustWarned && Now >= NextGustAt - K->GustWarn)
			{
				bGustWarned = true;
				WRSound::Broadcast(Pawn, TEXT("weather.gustTell"));
			}
			else if (Now >= NextGustAt)
			{
				const float Yaw = FMath::Fmod(W->WarnAt * 37.f, 360.f);
				const FVector Push = FRotator(0.f, Yaw, 0.f).Vector() * K->GustPush;
				if (ACharacter* Body = Cast<ACharacter>(Pawn); Body && !bSleeping)
				{
					Body->LaunchCharacter(FVector(Push.X, Push.Y, 120.f), false, false);
				}
				NextGustAt = -1.f;
			}
		}
		else
		{
			NextGustAt = -1.f;
		}
	}
	bWeatherCountered = bCountered;
	// Deep snow: a tag both machines read (the run speed is predicted).
	if (UAbilitySystemComponent* ASC = Pawn->GetAbilitySystemComponent())
	{
		const bool bSnow = WRun < 0.999f;
		if (bSnow != ASC->HasMatchingGameplayTag(WRTags::Weather_DeepSnow))
		{
			if (bSnow) ASC->AddLooseGameplayTag(WRTags::Weather_DeepSnow, 1, EGameplayTagReplicationState::TagOnly);
			else ASC->RemoveLooseGameplayTag(WRTags::Weather_DeepSnow, 1, EGameplayTagReplicationState::TagOnly);
		}
	}
}

// ---------------------------------------------------------------- held light and darkness

bool UWRSurvivalComponent::HoldsTorch() const
{
	return IsLightOn() && WRSurvivalDetail::LightKind(LightItem) == TEXT("torch");
}

bool UWRSurvivalComponent::HoldsTorch(const AActor* Character)
{
	const UWRSurvivalComponent* S = For(Character);
	return S && S->HoldsTorch();
}

void UWRSurvivalComponent::SetLightOn(bool bOn)
{
	if (bLightOn == bOn)
	{
		return;
	}
	bLightOn = bOn;
	OnRep_Light();
	if (AWRCharacterBase* Pawn = GetPawnCharacter())
	{
		WRSound::Broadcast(Pawn, bOn ? TEXT("light.on") : TEXT("light.off"));
		// The shield comes off the arm while a torch is in that hand: guard with the weapon (combat.md 2.4.0).
		if (UAbilitySystemComponent* ASC = Pawn->GetAbilitySystemComponent())
		{
			FGameplayTagContainer Guard;
			Guard.AddTag(WRTags::Ability_Block);
			ASC->CancelAbilities(&Guard);
		}
	}
}

void UWRSurvivalComponent::OnLightSlotChanged()
{
	const AWRPlayerState* PS = GetPS();
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Inv || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FWRItemStack& Slot = Inv->GetEquipped(EWREquipSlot::Light);
	const FName Now = Slot.IsEmpty() ? NAME_None : Slot.ItemId;
	if (Now != LightItem)
	{
		LightItem = Now;
		const AWRCharacterBase* Pawn = GetPawnCharacter();
		const bool bWet = Pawn && Pawn->GetCharacterMovement() && Pawn->GetCharacterMovement()->IsSwimming();
		bLightOn = false;
		const bool bWant = !bLoadedLightOff;
		bLoadedLightOff = false;
		SetLightOn(bWant && !LightItem.IsNone() && !bWet && Slot.Durability > 0.f);
		OnRep_Light();
	}
}

FString UWRSurvivalComponent::ToggleLight()
{
	AWRPlayerState* PS = GetPS();
	UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Inv)
	{
		return FString();
	}
	if (LightItem.IsNone())
	{
		// Nothing in the Light slot: light the first torch or lantern in the bag.
		const TArray<FWRItemStack>& Slots = Inv->GetSlots();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			const FWRItemDef* Def = Slots[i].IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Slots[i].ItemId);
			if (Def && Def->Slot == EWREquipSlot::Light)
			{
				return Inv->UseSlot(i);
			}
		}
		return TEXT("No torch or lantern in your bag");
	}
	if (bLightOn)
	{
		SetLightOn(false);
		return TEXT("Light out");
	}
	const AWRCharacterBase* Pawn = GetPawnCharacter();
	if (Pawn && Pawn->GetCharacterMovement() && Pawn->GetCharacterMovement()->IsSwimming())
	{
		return TEXT("Not in the water");
	}
	FWRItemStack* Slot = Inv->GetEquippedMutable(EWREquipSlot::Light);
	if (Slot && Slot->Durability <= 0.f)
	{
		const FName Fuel = FName(*FWRTuning::Get().Str(FString::Printf(TEXT("lights.%s.fuel"), *LightItem.ToString()), FString()));
		if (Fuel.IsNone() || Inv->CountOf(Fuel) <= 0)
		{
			return TEXT("Your lantern needs tallow");
		}
		Inv->RemoveItem(Fuel, 1);
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(LightItem);
		Slot->Durability = Def ? Def->MaxDurability : 0.f;
	}
	SetLightOn(true);
	return FString();
}

void UWRSurvivalComponent::TickLight(AWRCharacterBase* Pawn, float DeltaTime)
{
	using namespace WRSurvivalDetail;
	AWRPlayerState* PS = GetPS();
	UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (Inv && IsLightOn())
	{
		const FName Kind = LightKind(LightItem);
		const UCharacterMovementComponent* Move = Pawn->GetCharacterMovement();
		if (Move && Move->IsSwimming())
		{
			// Water puts a flame out (it stays in the slot: light it again on land).
			SetLightOn(false);
			Notify(Kind == TEXT("torch") ? TEXT("Your torch hissed out in the water") : TEXT("Your lantern drowned out in the water"));
			WRSound::Broadcast(Pawn, TEXT("light.douse"));
		}
		else if (FWRItemStack* Slot = Inv->GetEquippedMutable(EWREquipSlot::Light))
		{
			// Burn time is the item's durability, in seconds; rain makes a torch burn faster (a lantern's flame is shut in).
			Slot->Durability = FMath::Max(0.f, Slot->Durability - DeltaTime * (Kind == TEXT("torch") ? WBurn : 1.f));
			if (Slot->Durability <= 0.f)
			{
				const FName Fuel = FName(*FWRTuning::Get().Str(FString::Printf(TEXT("lights.%s.fuel"), *LightItem.ToString()), FString()));
				if (!Fuel.IsNone() && Inv->CountOf(Fuel) > 0)
				{
					// A lantern takes the next tallow from the bag by itself.
					Inv->RemoveItem(Fuel, 1);
					const FWRItemDef* Def = FWRItemRegistry::Get().Find(LightItem);
					Slot->Durability = Def ? Def->MaxDurability : 0.f;
					Notify(TEXT("Lantern refilled with tallow"));
				}
				else if (Kind == TEXT("torch"))
				{
					*Slot = FWRItemStack();
					Inv->ApplyEquipment();
					Notify(TEXT("Your torch burned out"));
					WRSound::Broadcast(Pawn, TEXT("light.douse"));
				}
				else
				{
					SetLightOn(false);
					Notify(TEXT("Your lantern is out of tallow"));
				}
			}
		}
	}

	// Lit or not: a light reaching you, or you're somewhere that's light anyway.
	const FVector At = Pawn->GetActorLocation();
	const FWRBiomeSurvival* Row = FWRWeatherDefs::Get().Biome(BiomeHere);
	const bool bDarkPlace = WRBiome::IsUnderground(this, At) || (Row && (Row->bUnderground || (Row->bDarkNights && AWRGameState::IsNightIn(this))));
	UWRLightSubsystem* Lights = UWRLightSubsystem::Get(this);
	const bool bNowUnlit = bDarkPlace && !(Lights && Lights->IsLit(At));
	if (bNowUnlit != bUnlit)
	{
		bUnlit = bNowUnlit;
		UE_LOG(LogWildreach, Verbose, TEXT("DARK %s is %s"), *GetNameSafe(PS), bUnlit ? TEXT("unlit") : TEXT("lit"));
	}
}

void UWRSurvivalComponent::OnRep_Light()
{
	if (AWRCharacter* Hero = Cast<AWRCharacter>(GetPawnCharacter()))
	{
		Hero->RefreshHeldLook();
	}
}

// ---------------------------------------------------------------- corruption

int32 UWRSurvivalComponent::CorruptionStep() const
{
	int32 Step = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		if (Corruption >= FWRTuning::Get().Num(FString::Printf(TEXT("corruption.steps.%d"), i), 25.f * (i + 1))) Step = i + 1;
	}
	return Step;
}

bool UWRSurvivalComponent::MatchesCorruption() const
{
	// Matching means the biome that corrupted you is your main path's: Necromancy in the Graveyard, Void in the Wasteland.
	const AWRPlayerState* PS = GetPS();
	const FName Main = PS && PS->GetPaths() ? PS->GetPaths()->GetMainPath() : NAME_None;
	const FWRBiomeSurvival* From = FWRWeatherDefs::Get().Biome(CorruptionFrom);
	return !Main.IsNone() && From && From->bCorrupt && From->CorruptionPath == Main;
}

void UWRSurvivalComponent::AddCorruption(float Amount)
{
	Corruption = FMath::Clamp(Corruption + Amount, 0.f, 100.f);
}

void UWRSurvivalComponent::TickCorruption(AWRCharacterBase* Pawn, float DeltaTime)
{
	const FWRTuning& T = FWRTuning::Get();
	const FVector At = Pawn->GetActorLocation();
	const FWRBiomeSurvival* Row = FWRWeatherDefs::Get().Biome(BiomeHere);
	if (Row && Row->bCorrupt)
	{
		// Builds faster at night, underground and in rifts; a warding fire stops it, a lantern or a light tame halves it.
		UWRLightSubsystem* Lights = UWRLightSubsystem::Get(this);
		const bool bDark = AWRGameState::IsNightIn(this) || WRBiome::IsUnderground(this, At) || FWRBiomeHazards::KindAt(At) == TEXT("rift");
		float Per = bDark ? T.Num(TEXT("corruption.buildNightSeconds"), 6.f) : T.Num(TEXT("corruption.buildDaySeconds"), 15.f);
		float Rate = 1.f / FMath::Max(0.5f, Per) * WCorruption;
		if (Lights && Lights->IsWarded(At)) Rate = 0.f;
		else if (Lights && Lights->IsHalving(At)) Rate *= 0.5f;
		// Gravecloth (four pieces) and the Bell clapper talisman slow it (Data/gear.json corruptionRate).
		Rate *= FMath::Max(0.f, 1.f + UWRInventoryComponent::GearStatFor(Pawn, TEXT("corruptionRate")));
		if (Rate > 0.f) CorruptionFrom = BiomeHere;
		AddCorruption(Rate * DeltaTime);
	}
	else if (Corruption > 0.f)
	{
		AddCorruption(-DeltaTime / FMath::Max(0.5f, T.Num(TEXT("corruption.fallSeconds"), 3.f)));
	}
	// Max stamina follows the 75 step (the path component re-applies the level's numbers with it).
	const int32 Step = CorruptionStep();
	if (Step != LastCorruptionStep)
	{
		const bool bCrossed75 = (Step >= 3) != (LastCorruptionStep >= 3);
		LastCorruptionStep = Step;
		AWRCharacter* Hero = Cast<AWRCharacter>(Pawn);
		AWRPlayerState* PS = GetPS();
		if (bCrossed75 && Hero && PS && PS->GetPaths())
		{
			PS->GetPaths()->ApplyToCharacter(Hero);
		}
		UE_LOG(LogWildreach, Log, TEXT("CORRUPTION %s at step %d (%.0f)"), *GetNameSafe(PS), Step, Corruption);
	}
}

// ---------------------------------------------------------------- what the rest of the game asks

float UWRSurvivalComponent::StaminaRegenMult() const
{
	float M = WRegen;
	if (IsRested()) M *= WRTune(TEXT("rested.staminaRegen"), 1.2f);
	if (CorruptionStep() >= 2 && !MatchesCorruption()) M *= WRTune(TEXT("corruption.staminaRegen"), 0.85f);
	return M;
}

float UWRSurvivalComponent::HungerDrainMult() const
{
	return IsRested() ? WRTune(TEXT("rested.drain"), 0.8f) : 1.f;
}

float UWRSurvivalComponent::ThirstDrainMult() const
{
	return HungerDrainMult() * WThirst;
}

float UWRSurvivalComponent::SkillXpMult() const
{
	return IsRested() ? WRTune(TEXT("rested.skillXp"), 1.1f) : 1.f;
}

float UWRSurvivalComponent::HealingTakenMult() const
{
	return CorruptionStep() >= 1 && !MatchesCorruption() ? WRTune(TEXT("corruption.healingTaken"), 0.9f) : 1.f;
}

float UWRSurvivalComponent::MaxStaminaPenalty() const
{
	return CorruptionStep() >= 3 && !MatchesCorruption() ? WRTune(TEXT("corruption.maxStamina"), 20.f) : 0.f;
}

float UWRSurvivalComponent::ResourceGainMult() const
{
	return CorruptionStep() >= 4 && !MatchesCorruption() ? WRTune(TEXT("corruption.resourceGain"), 0.75f) : 1.f;
}

float UWRSurvivalComponent::PathDamageMult() const
{
	const int32 Step = CorruptionStep();
	if (Step <= 0 || !MatchesCorruption())
	{
		return 1.f;
	}
	return 1.f + FWRTuning::Get().Num(FString::Printf(TEXT("corruption.pathDamage.%d"), Step - 1), 0.f);
}

float UWRSurvivalComponent::SwimStaminaMult() const
{
	return WSwim;
}

float UWRSurvivalComponent::AggroMultFor(const AActor* Player, const FWRCreatureDef& Creature, FName CreatureBiome)
{
	const UWRSurvivalComponent* S = For(Player);
	if (!S)
	{
		return 1.f;
	}
	float M = 1.f;
	const bool bMatch = S->MatchesCorruption();
	// Dark-hunters smell the unlit (and anyone corruption has darkened) from twice as far.
	const bool bDarkToThem = S->IsUnlit() || (S->CorruptionStep() >= 2 && !bMatch);
	if (Creature.bDarkHunter && bDarkToThem) M *= WRTune(TEXT("darkness.darkHunterAggroMult"), 2.f);
	// Marked: every creature of the corrupt biome you're in.
	const FWRBiomeSurvival* Row = FWRWeatherDefs::Get().Biome(S->GetBiomeHere());
	if (S->CorruptionStep() >= 4 && !bMatch && Row && Row->bCorrupt && Creature.Biome == S->GetBiomeHere()) M *= WRTune(TEXT("corruption.markedAggroMult"), 2.f);
	// Fog: its ambushers strike from further.
	if (const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(Player, S->GetBiomeHere());
		K && (K->AggroCreatures.Contains(Creature.Id) || K->AggroCreatures.Contains(FName(TEXT("*")))) && !S->IsWeatherCountered() && !S->IsSheltered())
	{
		M *= K->AggroMult;
	}
	return M;
}

// ---------------------------------------------------------------- save

TSharedRef<FJsonObject> UWRSurvivalComponent::ToJson() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("restedSeconds"), RestedSecondsLeft());
	O->SetNumberField(TEXT("restedMinutes"), RestedMinutes);
	O->SetNumberField(TEXT("corruption"), Corruption);
	O->SetStringField(TEXT("corruptionFrom"), CorruptionFrom.ToString());
	O->SetBoolField(TEXT("lightOn"), bLightOn);
	return O;
}

void UWRSurvivalComponent::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid())
	{
		return;
	}
	double V = 0.0;
	if (Json->TryGetNumberField(TEXT("restedSeconds"), V)) LoadedRestedSeconds = float(V);
	if (Json->TryGetNumberField(TEXT("restedMinutes"), V)) RestedMinutes = int32(V);
	if (Json->TryGetNumberField(TEXT("corruption"), V)) Corruption = FMath::Clamp(float(V), 0.f, 100.f);
	FString From;
	if (Json->TryGetStringField(TEXT("corruptionFrom"), From) && From != TEXT("None")) CorruptionFrom = FName(*From);
	LastCorruptionStep = CorruptionStep();
	// The inventory's load fills the Light slot and lights it when the pawn arrives; a light saved out stays out.
	bool bOn = true;
	Json->TryGetBoolField(TEXT("lightOn"), bOn);
	bLoadedLightOff = !bOn;
}
