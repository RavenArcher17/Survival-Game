// Machinery and power (WRMachine.h; cooks/build-machinery.md, survival-crafting 2.15). The server runs every network in
// steps of rules.stepSeconds while a player is near it; every machine draws the result from the piece's replicated
// FWRMachineRep (WRMachineLook.cpp turns the parts, WRUIMachine.cpp is the screen).

#include "WRMachine.h"
#include "WRTurret.h"
#include "WRMachineLook.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRFarm.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRWorldDressing.h"
#include "Wildreach.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Landscape.h"

namespace WRMachineDetail
{
	float R(const TCHAR* Path, float Fallback) { return FWRMachineDefs::Get().Rule(Path, Fallback); }

	double ServerTime(const UObject* Ctx)
	{
		const UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : World ? World->GetTimeSeconds() : 0.0;
	}

	/** A piece's input / output slots: Storage[0, In) and [In, In + Out). */
	TArray<FWRItemStack> Slice(const TArray<FWRItemStack>& S, int32 From, int32 Num)
	{
		TArray<FWRItemStack> Out;
		for (int32 i = From; i < From + Num; ++i) Out.Add(S.IsValidIndex(i) ? S[i] : FWRItemStack());
		return Out;
	}
	void Put(TArray<FWRItemStack>& S, int32 From, const TArray<FWRItemStack>& Part)
	{
		if (S.Num() < From + Part.Num()) S.SetNum(From + Part.Num());
		for (int32 i = 0; i < Part.Num(); ++i) S[From + i] = Part[i];
	}

	FString ItemName(FName Id)
	{
		const FWRItemDef* D = FWRItemRegistry::Get().Find(Id);
		return D ? D->Name : Id.ToString();
	}

	bool IsOre(FName Item)
	{
		const FString S = Item.ToString();
		return S.EndsWith(TEXT("_ore")) || Item == TEXT("sulfur_crystal");
	}

	/** Online player state for a name (null when they're not on). */
	APlayerState* Online(const UWorld* World, const FString& Name)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS) return nullptr;
		for (APlayerState* PS : GS->PlayerArray) if (PS && PS->GetPlayerName() == Name) return PS;
		return nullptr;
	}

	/** A piece's world bounds (its collision as built). */
	FBox Bounds(const AActor* A)
	{
		FVector O, E;
		A->GetActorBounds(true, O, E);
		if (E.IsNearlyZero()) { O = A->GetActorLocation(); E = FVector(60.f); }
		return FBox(O - E, O + E);
	}

	/** Water a pump can draw: the sea within Range of a point. */
	bool WaterWithin(const UObject* Ctx, const FVector& At, float Range)
	{
		const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(Ctx);
		if (!Water || !Water->HasWater()) return false;
		if (Water->DepthAt(At) > 0.f) return true;
		for (float D = 400.f; D <= Range; D += 400.f)
		{
			for (int32 i = 0; i < 16; ++i)
			{
				const float A = i * PI / 8.f;
				if (Water->DepthAt(At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * D) > 0.f) return true;
			}
		}
		return false;
	}

	/** Something overhead (a roof, rock): a mycelium vat's dark. */
	bool Covered(const UWorld* World, const FVector& At, const AActor* Ignore)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRMachineDark), false, Ignore);
		return World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 220.f), At + FVector(0.f, 0.f, 2500.f), ECC_WorldStatic, Q);
	}
}

// ---------------------------------------------------------------- defs

FWRMachineDefs& FWRMachineDefs::Get()
{
	using namespace WRMachineDetail;
	static FWRMachineDefs Instance;
	return Instance;
}

FWRMachineDefs::FWRMachineDefs()
{
	using namespace WRMachineDetail;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("machines.json"));
	if (!Root.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("MACHINE Data/machines.json missing: no machines"));
		return;
	}
	const TSharedPtr<FJsonObject>* RulesObj = nullptr;
	if (Root->TryGetObjectField(TEXT("rules"), RulesObj)) Rules = *RulesObj;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("machines"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			FWRMachineDef D;
			FString S;
			D.Piece = FName(*O->GetStringField(TEXT("piece")));
			D.Role = FName(*O->GetStringField(TEXT("role")));
			if (O->TryGetStringField(TEXT("flow"), S)) D.Flow = FName(*S);
			O->TryGetNumberField(TEXT("supply"), D.Supply);
			O->TryGetNumberField(TEXT("supplyHigh"), D.SupplyHigh);
			O->TryGetNumberField(TEXT("load"), D.Load);
			O->TryGetNumberField(TEXT("rpm"), D.Rpm);
			O->TryGetNumberField(TEXT("length"), D.Length);
			if (O->TryGetStringField(TEXT("station"), S)) D.Station = FName(*S);
			if (O->TryGetStringField(TEXT("needsStat"), S)) D.NeedsStat = FName(*S);
			if (O->TryGetStringField(TEXT("freeIn"), S)) D.FreeIn = FName(*S);
			if (O->TryGetStringField(TEXT("input"), S)) D.Input = FName(*S);
			if (O->TryGetStringField(TEXT("output"), S)) D.Output = FName(*S);
			const TArray<TSharedPtr<FJsonValue>>* Outs = nullptr;
			if (O->TryGetArrayField(TEXT("outputs"), Outs)) for (const TSharedPtr<FJsonValue>& X : *Outs) D.Outputs.Add(FName(*X->AsString()));
			const TSharedPtr<FJsonObject>* Slots = nullptr;
			if (O->TryGetObjectField(TEXT("slots"), Slots))
			{
				double N = 0.0;
				if ((*Slots)->TryGetNumberField(TEXT("in"), N)) D.In = static_cast<int32>(N);
				if ((*Slots)->TryGetNumberField(TEXT("out"), N)) D.Out = static_cast<int32>(N);
			}
			Defs.Add(D.Piece, D);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("Loaded %d machines"), Defs.Num());
}

float FWRMachineDefs::Rule(const TCHAR* Path, float Fallback) const
{
	using namespace WRMachineDetail;
	TArray<FString> Parts;
	FString(Path).ParseIntoArray(Parts, TEXT("."));
	TSharedPtr<FJsonObject> O = Rules;
	for (int32 i = 0; O.IsValid() && i < Parts.Num(); ++i)
	{
		if (i == Parts.Num() - 1)
		{
			double V = 0.0;
			return O->TryGetNumberField(Parts[i], V) ? static_cast<float>(V) : Fallback;
		}
		const TSharedPtr<FJsonObject>* Next = nullptr;
		O = O->TryGetObjectField(Parts[i], Next) ? *Next : nullptr;
	}
	return Fallback;
}

FString FWRMachineDefs::RuleStr(const TCHAR* Path, const FString& Fallback) const
{
	using namespace WRMachineDetail;
	TArray<FString> Parts;
	FString(Path).ParseIntoArray(Parts, TEXT("."));
	TSharedPtr<FJsonObject> O = Rules;
	for (int32 i = 0; O.IsValid() && i < Parts.Num(); ++i)
	{
		if (i == Parts.Num() - 1)
		{
			FString V;
			return O->TryGetStringField(Parts[i], V) ? V : Fallback;
		}
		const TSharedPtr<FJsonObject>* Next = nullptr;
		O = O->TryGetObjectField(Parts[i], Next) ? *Next : nullptr;
	}
	return Fallback;
}

// ---------------------------------------------------------------- shared rules

bool FWRMachines::IsMachine(const FWRPieceDef& Def)
{
	using namespace WRMachineDetail;
	return Def.Kind == TEXT("machine") && FWRMachineDefs::Get().Find(Def.Id) != nullptr;
}

const FWRMachineDef* FWRMachines::DefOf(const AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	return Piece ? FWRMachineDefs::Get().Find(Piece->PieceId) : nullptr;
}

float FWRMachines::HubZ(const UObject* WorldContext, const FWRPieceDef& Def, const FTransform& T)
{
	using namespace WRMachineDetail;
	const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(WorldContext);
	const float Surface = Water && Water->HasWater() ? Water->GetSurfaceZ() : T.GetLocation().Z - 80.f;
	const bool bTide = Def.Id == TEXT("tide_mill");
	const float Radius = bTide ? 260.f : R(TEXT("flow.wheelRadius"), 200.f);
	const float Dip = R(TEXT("flow.wheelDip"), 50.f);
	return FMath::Clamp(Surface - T.GetLocation().Z + Radius - Dip, -40.f, 400.f);
}

TArray<FWRPort> FWRMachines::PortsAt(const UObject* WorldContext, const FWRPieceDef& Def, const FTransform& T)
{
	using namespace WRMachineDetail;
	TArray<FWRPort> Out;
	const float Hub = (Def.Id == TEXT("water_wheel") || Def.Id == TEXT("tide_mill")) ? HubZ(WorldContext, Def, T) : 0.f;
	for (const FWRPort& L : FWRMachineLook::LocalPorts(Def, Hub))
	{
		Out.Add({ T.TransformPosition(L.At), T.TransformVectorNoScale(L.Axis).GetSafeNormal() });
	}
	return Out;
}

TArray<FWRPort> FWRMachines::Ports(const AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	return Def ? PortsAt(Piece, *Def, Piece->GetActorTransform()) : TArray<FWRPort>();
}

int32 FWRMachines::OwnedHearthLevel(const UObject* WorldContext, const FString& Player)
{
	using namespace WRMachineDetail;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	int32 Best = 1;
	for (TActorIterator<AWRHearth> It(World); It && World; ++It)
	{
		const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(It->GetOwner());
		if (!Piece) continue;
		const bool bMine = Piece->HasAuthority() ? Piece->CanAccess(Player) : Piece->OwnerName == Player;
		if (bMine) Best = FMath::Max(Best, It->GetTechLevel());
	}
	return Best;
}

bool FWRMachines::LavaNear(const FVector& At, float Reach)
{
	using namespace WRMachineDetail;
	const FWRBiomeHazards::FLavaGrid& Flow = FWRBiomeHazards::Flow();
	const float Step = FMath::Max(100.f, Flow.Spacing * 0.5f);
	for (float X = -Reach; X <= Reach; X += Step)
	{
		for (float Y = -Reach; Y <= Reach; Y += Step)
		{
			if (X * X + Y * Y > Reach * Reach) continue;
			float Ground = 0.f, Lift = 0.f;
			if (Flow.SurfaceAt(FVector2D(At.X + X, At.Y + Y), Ground, Lift) && Lift > 0.f && FMath::Abs(Ground - At.Z) < 800.f) return true;
		}
	}
	return false;
}

float FWRMachines::FlowAt(const UObject* WorldContext, const FVector& WheelAt, bool bTide)
{
	using namespace WRMachineDetail;
	const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(WorldContext);
	if (!Water || !Water->HasWater()) return 0.f;
	const float MinDepth = bTide ? R(TEXT("tide.minDepth"), 100.f) : R(TEXT("flow.surfMinDepth"), 60.f);
	if (Water->DepthAt(WheelAt) < MinDepth) return 0.f;
	// Moving water: the surf line, where the swell runs in and out over the shallows by a shore (rivers join this when
	// the map has them). Open water far from land lies still under a wheel.
	const float Reach = bTide ? R(TEXT("tide.shoreReach"), 1000.f) : R(TEXT("flow.shoreReach"), 1500.f);
	bool bShore = false;
	for (float D = 300.f; D <= Reach && !bShore; D += 300.f)
	{
		for (int32 i = 0; i < 16 && !bShore; ++i)
		{
			const float A = i * PI / 8.f;
			bShore = Water->DepthAt(WheelAt + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * D) <= 0.f;
		}
	}
	if (!bShore) return 0.f;
	if (bTide) return 1.f;
	const double T = ServerTime(WorldContext);
	const float Swell = R(TEXT("flow.swell"), 0.15f) * FMath::Sin(2.f * PI * static_cast<float>(FMath::Fmod(T, 3600.0)) / FMath::Max(1.f, R(TEXT("flow.swellSeconds"), 9.f)));
	return R(TEXT("flow.surf"), 0.8f) * (1.f + Swell);
}

float FWRMachines::WindAt(const UObject* WorldContext, const FVector& At, float& OutYaw)
{
	using namespace WRMachineDetail;
	const double T = ServerTime(WorldContext);
	const float Gust = FMath::Max(1.f, R(TEXT("wind.gustSeconds"), 37.f));
	const float t = static_cast<float>(FMath::Fmod(T, 86400.0));
	const float Wave = 0.6f * FMath::Sin(2.f * PI * t / Gust) + 0.4f * FMath::Sin(2.f * PI * t / (Gust * 0.37f) + 1.3f);
	float Strength = FMath::Lerp(R(TEXT("wind.min"), 0.6f), R(TEXT("wind.max"), 1.2f), 0.5f + 0.5f * Wave);
	const float Veer = FMath::Max(60.f, R(TEXT("wind.veerSeconds"), 5400.f));
	OutYaw = FMath::Fmod(360.f * static_cast<float>(FMath::Fmod(T, Veer)) / Veer + 25.f * FMath::Sin(2.f * PI * t / 600.f), 360.f);
	const FName Biome = WRBiome::At(WorldContext, At);
	const FString Biomes = FString::Printf(TEXT("wind.biomes.%s"), *Biome.ToString());
	Strength *= R(*Biomes, 1.f);
	if (const FWRWeatherKind* Kind = UWRWeatherSubsystem::ActiveKind(WorldContext, Biome))
	{
		TArray<FString> Storms;
		FWRMachineDefs::Get().RuleStr(TEXT("wind.stormKinds"), TEXT("blizzard,thunderstorm,gale,squall")).ParseIntoArray(Storms, TEXT(","));
		if (Storms.Contains(Kind->Id.ToString())) Strength *= R(TEXT("wind.storm"), 2.f);
	}
	return Strength;
}

FString FWRMachines::PlaceCheck(const UObject* WorldContext, const FWRPieceDef& Def, const FVector& Location, float Yaw, const FString& Builder, const AActor* BuilderActor)
{
	using namespace WRMachineDetail;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return FString();
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location);
	if (Def.Id == TEXT("lava_forge"))
	{
		return LavaNear(Location, R(TEXT("lava.forgeReach"), 300.f)) ? FString() : FString(TEXT("a lava forge sits over running lava: within 3 m of it"));
	}
	const FWRMachineDef* M = FWRMachineDefs::Get().Find(Def.Id);
	if (!M || Def.Kind != TEXT("machine")) return FString();
	if (Def.HearthLevel > 1 && OwnedHearthLevel(WorldContext, Builder) < Def.HearthLevel)
	{
		return FString::Printf(TEXT("needs a Hearth of level %d (one of yours, anywhere)"), Def.HearthLevel);
	}
	if (!M->NeedsStat.IsNone() && UWRLifeSkillsComponent::StatFor(BuilderActor, M->NeedsStat) <= 0.f)
	{
		return TEXT("needs the Long lines perk (Engineering)");
	}
	const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(WorldContext);
	if (Def.Id == TEXT("water_wheel") || Def.Id == TEXT("tide_mill"))
	{
		const float Surface = Water && Water->HasWater() ? Water->GetSurfaceZ() : -1.0e6f;
		const float Bank = Surface - Location.Z;
		if (Bank < R(TEXT("flow.bankMin"), -160.f) || Bank > R(TEXT("flow.bankMax"), -20.f))
		{
			return TEXT("set it on a bank just above the water (0.2 to 1.6 m)");
		}
		const FVector Wheel = T.TransformPosition(FVector(R(TEXT("flow.wheelOut"), 300.f), 0.f, 0.f));
		if (FlowAt(WorldContext, Wheel, Def.Id == TEXT("tide_mill")) <= 0.f)
		{
			return Def.Id == TEXT("tide_mill") ? TEXT("its wheel goes in a tidal channel: sea at least 1 m deep, the shore within 10 m")
				: TEXT("its wheel needs moving water at least 60 cm deep: face it out into the surf, near the shore");
		}
		const float Hub = HubZ(WorldContext, Def, T);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRMachineAxle), false);
		if (World->LineTraceSingleByChannel(Hit, T.TransformPosition(FVector(0.f, 0.f, Hub)), T.TransformPosition(FVector(R(TEXT("flow.wheelOut"), 300.f) - 60.f, 0.f, Hub)), ECC_WorldStatic, Q)
			&& Cast<ALandscapeProxy>(Hit.GetActor()))
		{
			return TEXT("the bank is in the way of its axle: try where the shore drops steeper");
		}
	}
	else if (Def.Id == TEXT("windmill"))
	{
		// Open ground: nothing of the land as tall as its sails' hub within 10 m.
		const float Hub = R(TEXT("wind.hubHeight"), 710.f);
		const float Clear = R(TEXT("wind.clear"), 1000.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRMachineWind), false);
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = i * PI / 8.f;
			FHitResult Hit;
			const FVector From = Location + FVector(0.f, 0.f, Hub);
			if (World->LineTraceSingleByChannel(Hit, From, From + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Clear, ECC_WorldStatic, Q) && !Cast<AWRBuildPiece>(Hit.GetActor()))
			{
				return TEXT("too sheltered: a windmill needs open ground, nothing as tall within 10 m");
			}
		}
	}
	else if (M->Flow == TEXT("lava"))
	{
		if (!LavaNear(T.TransformPosition(FVector(130.f, 0.f, 0.f)), R(TEXT("lava.reach"), 600.f))) return TEXT("its bore needs lava within 6 m");
	}
	else if (M->Role == TEXT("drill"))
	{
		const AWRResourceNode* Best = nullptr;
		const float Reach = R(TEXT("drillReach"), 150.f);
		for (TActorIterator<AWRResourceNode> It(World); It; ++It)
		{
			if (IsOre(It->ItemId) && FVector::Dist(It->GetActorLocation(), Location) < Reach) Best = *It;
		}
		if (!Best) return TEXT("set a drill over an ore vein (within 1.5 m of it)");
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			if (!It->bGhost && It->PieceId == TEXT("drill") && FVector::Dist(It->GetActorLocation(), Best->GetActorLocation()) < Reach) return TEXT("a drill already works that vein");
		}
	}
	else if (M->Role == TEXT("pump"))
	{
		if (!WaterWithin(WorldContext, Location, R(TEXT("pumpWaterRange"), 4000.f))) return TEXT("no water within 40 m for it to draw");
	}
	else if (M->Role == TEXT("still"))
	{
		const float Depth = Water ? Water->DepthAt(Location) : 0.f;
		if (Depth < R(TEXT("still.minDepth"), 10.f) || Depth > R(TEXT("still.maxDepth"), 50.f)) return TEXT("a blood still stands in 10 to 50 cm of bog water");
	}
	return FString();
}

bool FWRMachines::SnapPlacement(const UObject* WorldContext, const FWRPieceDef& Def, FVector& InOutLoc, float& InOutYaw)
{
	using namespace WRMachineDetail;
	const FWRMachineDef* M = FWRMachineDefs::Get().Find(Def.Id);
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!M || !M->Carries() || !World) return false;
	const float Range = R(TEXT("snapRange"), 120.f);
	const TArray<FWRPort> Local = FWRMachineLook::LocalPorts(Def, 0.f);
	float Best = Range;
	bool bFound = false;
	FVector BestLoc = InOutLoc;
	float BestYaw = InOutYaw;
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		if (It->bGhost || FVector::Dist(It->GetActorLocation(), InOutLoc) > 1500.f) continue;
		const FWRPieceDef* Other = It->GetDef();
		if (!Other || !IsMachine(*Other)) continue;
		for (const FWRPort& P : Ports(*It))
		{
			if (FMath::Abs(P.Axis.Z) > 0.5f) continue;   // lines run level
			const float PortYaw = FMath::RadiansToDegrees(FMath::Atan2(P.Axis.Y, P.Axis.X));
			for (const FWRPort& L : Local)
			{
				// Line the ghost's port axis up with the target's (either way round), its port on the target's port, and
				// keep the one whose body stands away from the target piece.
				const float LocalYaw = FMath::RadiansToDegrees(FMath::Atan2(L.Axis.Y, L.Axis.X));
				for (const float Flip : { 0.f, 180.f })
				{
					const float Yaw = FRotator::NormalizeAxis(PortYaw + Flip - LocalYaw);
					const FRotator Rot(0.f, Yaw, 0.f);
					const FVector Loc = P.At - Rot.RotateVector(L.At);
					const FVector Current = FTransform(FRotator(0.f, InOutYaw, 0.f), InOutLoc).TransformPosition(L.At);
					const float Dist = FVector::Dist(Current, P.At);
					const bool bAway = FVector::DistSquared2D(Loc, It->GetActorLocation()) > FVector::DistSquared2D(P.At, It->GetActorLocation());
					if (Dist < Best && bAway)
					{
						Best = Dist;
						BestLoc = Loc;
						BestYaw = Yaw;
						bFound = true;
					}
				}
			}
		}
	}
	if (bFound)
	{
		InOutLoc = BestLoc;
		InOutYaw = BestYaw;
	}
	return bFound;
}

int32 FWRMachines::CanPay(const UWRInventoryComponent* Inv, FName Item)
{
	using namespace WRMachineDetail;
	if (!Inv) return 0;
	return Inv->CountOf(Item) + (Item == TEXT("wood") ? Inv->CountOf(TEXT("planks")) : 0);
}

void FWRMachines::Pay(UWRInventoryComponent* Inv, FName Item, int32 Count)
{
	using namespace WRMachineDetail;
	if (!Inv || Count <= 0) return;
	if (Item == TEXT("wood"))
	{
		// Planks first: they were sawn for building.
		const int32 FromPlanks = FMath::Min(Count, Inv->CountOf(TEXT("planks")));
		if (FromPlanks > 0) Inv->RemoveItem(TEXT("planks"), FromPlanks);
		Count -= FromPlanks;
	}
	if (Count > 0) Inv->RemoveItem(Item, Count);
}

FString FWRMachines::StateWord(EWRMachineState State)
{
	using namespace WRMachineDetail;
	switch (State)
	{
	case EWRMachineState::Running: return TEXT("Running");
	case EWRMachineState::Slow: return TEXT("Slow");
	case EWRMachineState::NoPower: return TEXT("No power");
	case EWRMachineState::OutputFull: return TEXT("Output full");
	case EWRMachineState::Off: return TEXT("Switched off");
	case EWRMachineState::Worn: return TEXT("Worn");
	case EWRMachineState::Asleep: return TEXT("Asleep");
	case EWRMachineState::NoWater: return TEXT("No water");
	case EWRMachineState::NoWind: return TEXT("No wind");
	case EWRMachineState::NoHeat: return TEXT("No heat");
	case EWRMachineState::NoFuel: return TEXT("No fuel");
	case EWRMachineState::NeedsDark: return TEXT("Needs the dark");
	case EWRMachineState::ChainLong: return TEXT("Chain too long");
	case EWRMachineState::NoLink: return TEXT("Not linked");
	case EWRMachineState::NoStorm: return TEXT("Waiting for a storm");
	case EWRMachineState::NoSun: return TEXT("Waiting for the sun");
	case EWRMachineState::NoAmmo: return TEXT("Nothing to shoot");
	default: return TEXT("Idle");
	}
}

FString FWRMachines::StateText(const AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	const FWRMachineDef* M = DefOf(Piece);
	if (!M) return FString();
	const FWRMachineRep& Rep = Piece->Machine;
	const float Supply = Rep.Supply10 / 10.f, Load = Rep.Load10 / 10.f, Own = Rep.Own10 / 10.f;
	const FString Word = StateWord(Rep.State);
	switch (Rep.State)
	{
	case EWRMachineState::NoPower: return FString::Printf(TEXT("No power: its line gives %.1f P of the %.1f it needs"), Supply, Load);
	case EWRMachineState::Slow: return FString::Printf(TEXT("Slow: its line gives %.1f P of %.1f (%d%%)"), Supply, Load, FMath::RoundToInt(100.f * Supply / FMath::Max(0.1f, Load)));
	case EWRMachineState::OutputFull: return M->Role == TEXT("conveyor") ? TEXT("Jammed: what's at its head has no room") : TEXT("Output full: take what it made");
	case EWRMachineState::Off: return TEXT("Switched off");
	case EWRMachineState::Worn: return TEXT("Worn: half speed until it's repaired");
	case EWRMachineState::Asleep: return TEXT("Asleep: no one near it");
	case EWRMachineState::NoWater: return M->Role == TEXT("pump") ? TEXT("No water within 40 m") : M->Role == TEXT("still") ? TEXT("Needs bog water under it") : TEXT("Its water isn't moving");
	case EWRMachineState::NoWind: return TEXT("Blocked: something as tall within 10 m");
	case EWRMachineState::NoHeat: return TEXT("No lava by its bore");
	case EWRMachineState::NoFuel: return M->Role == TEXT("vat") ? TEXT("Needs compost") : TEXT("No soulstone in its fuel box");
	case EWRMachineState::NeedsDark: return TEXT("Needs the dark: a roof or a cave over it");
	case EWRMachineState::ChainLong: return TEXT("Chain too long: 4 machines (Chief engineer: 6)");
	case EWRMachineState::NoLink: return TEXT("Not linked: Link it to another lift within 120 m");
	case EWRMachineState::NoStorm: return TEXT("Waiting for a thunderstorm (battery flat)");
	case EWRMachineState::NoSun: return TEXT("Waiting for the sun (battery flat)");
	case EWRMachineState::NoAmmo: return UWRTurretSubsystem::AmmoText(Piece);
	case EWRMachineState::Running:
		if (M->Role == TEXT("turret")) return FString::Printf(TEXT("%s: %s"), Piece->Turret.bTracking ? TEXT("Tracking") : TEXT("On watch"), *UWRTurretSubsystem::AmmoText(Piece));
		if (M->IsSource()) return FString::Printf(TEXT("Running: %.1f P to its line"), Own);
		if (M->Carries() && M->Role != TEXT("conveyor")) return Supply > 0.f ? FString::Printf(TEXT("Turning: %.1f P on the line, %.1f drawn"), Supply, Load) : TEXT("Still: no source on this line");
		return Word;
	default:
		if (M->IsSource()) return TEXT("Idle");
		if (M->Carries() && M->Role != TEXT("conveyor")) return Supply > 0.f ? FString::Printf(TEXT("Turning: %.1f P on the line"), Supply) : TEXT("Still: no source on this line");
		return M->Role == TEXT("lamp") ? TEXT("Idle: lights at night") : TEXT("Idle: nothing to do");
	}
}

FString FWRMachines::Interact(AWRBuildPiece* Piece, AWRPlayerController* Player)
{
	using namespace WRMachineDetail;
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	return Def ? Def->Name : FString();
}

float FWRMachines::LiftAlpha(const FWRMachineRep& Rep, float ServerTime)
{
	using namespace WRMachineDetail;
	const float To = Rep.TripTo ? 1.f : 0.f;
	if (Rep.TripStart <= 0.f) return To;
	const float Trip = FMath::Max(1.f, R(TEXT("lift.tripSeconds"), 10.f));
	const float t = FMath::Clamp((ServerTime - Rep.TripStart) / Trip, 0.f, 1.f);
	const float Eased = t * t * (3.f - 2.f * t);
	return FMath::Lerp(1.f - To, To, Eased);
}

TArray<TPair<FName, int32>> FWRMachines::RepairCost(const AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	TArray<TPair<FName, int32>> Out;
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def || Piece->Health >= Piece->MaxHealth) return Out;
	const float Missing = 1.f - Piece->Health / FMath::Max(1.f, Piece->MaxHealth);
	for (const TPair<FName, int32>& C : Def->Cost)
	{
		const int32 N = FMath::CeilToInt(C.Value * R(TEXT("repairCostFraction"), 0.5f) * Missing);
		if (N > 0) Out.Add({ C.Key, N });
	}
	return Out;
}

void FWRMachines::ToJson(const AWRBuildPiece* Piece, TSharedRef<FJsonObject> O)
{
	using namespace WRMachineDetail;
	const FWRMachineState& S = Piece->Mach;
	TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
	M->SetBoolField(TEXT("on"), S.bOn);
	M->SetBoolField(TEXT("overclock"), S.bOverclock);
	M->SetNumberField(TEXT("progress"), S.Progress);
	if (!S.Recipe.IsNone()) M->SetStringField(TEXT("recipe"), S.Recipe.ToString());
	M->SetNumberField(TEXT("charge"), S.Charge);
	M->SetNumberField(TEXT("fuel"), S.FuelLeft);
	M->SetNumberField(TEXT("run"), S.RunSeconds);
	M->SetNumberField(TEXT("vat"), S.VatCount);
	M->SetBoolField(TEXT("vatLions"), S.bVatLions);
	M->SetNumberField(TEXT("ownerSpeed"), S.OwnerSpeed);
	M->SetBoolField(TEXT("ownerEfficient"), S.bOwnerEfficient);
	M->SetBoolField(TEXT("ownerSelfRepair"), S.bOwnerSelfRepair);
	M->SetBoolField(TEXT("ownerChief"), S.bOwnerChief);
	if (!S.LinkTo.IsZero())
	{
		M->SetNumberField(TEXT("linkX"), S.LinkTo.X);
		M->SetNumberField(TEXT("linkY"), S.LinkTo.Y);
		M->SetNumberField(TEXT("linkZ"), S.LinkTo.Z);
	}
	M->SetNumberField(TEXT("liftAt"), Piece->Machine.TripTo);
	O->SetObjectField(TEXT("machine"), M);
}

void FWRMachines::FromJson(AWRBuildPiece* Piece, const TSharedPtr<FJsonObject>& O)
{
	using namespace WRMachineDetail;
	const TSharedPtr<FJsonObject>* MObj = nullptr;
	if (!Piece || !O.IsValid() || !O->TryGetObjectField(TEXT("machine"), MObj)) return;
	const TSharedPtr<FJsonObject>& M = *MObj;
	FWRMachineState& S = Piece->Mach;
	M->TryGetBoolField(TEXT("on"), S.bOn);
	M->TryGetBoolField(TEXT("overclock"), S.bOverclock);
	M->TryGetNumberField(TEXT("progress"), S.Progress);
	FString Str;
	if (M->TryGetStringField(TEXT("recipe"), Str)) S.Recipe = FName(*Str);
	M->TryGetNumberField(TEXT("charge"), S.Charge);
	M->TryGetNumberField(TEXT("fuel"), S.FuelLeft);
	M->TryGetNumberField(TEXT("run"), S.RunSeconds);
	M->TryGetNumberField(TEXT("vat"), S.VatCount);
	M->TryGetBoolField(TEXT("vatLions"), S.bVatLions);
	M->TryGetNumberField(TEXT("ownerSpeed"), S.OwnerSpeed);
	M->TryGetBoolField(TEXT("ownerEfficient"), S.bOwnerEfficient);
	M->TryGetBoolField(TEXT("ownerSelfRepair"), S.bOwnerSelfRepair);
	M->TryGetBoolField(TEXT("ownerChief"), S.bOwnerChief);
	double X = 0.0, Y = 0.0, Z = 0.0;
	if (M->TryGetNumberField(TEXT("linkX"), X) && M->TryGetNumberField(TEXT("linkY"), Y) && M->TryGetNumberField(TEXT("linkZ"), Z)) S.LinkTo = FVector(X, Y, Z);
	double At = 0.0;
	if (M->TryGetNumberField(TEXT("liftAt"), At)) Piece->Machine.TripTo = At > 0.5 ? 1 : 0;
	Piece->Machine.Flags = (S.bOn ? WRMachineFlag::On : 0) | (S.bOverclock ? WRMachineFlag::Overclock : 0);
	Piece->Machine.Target = S.LinkTo;
}

// ---------------------------------------------------------------- commands

namespace WRMachineDetail
{
	AWRBuildPiece* PieceAt(AWRPlayerController* Player, const TArray<FString>& Args, FString& OutWhy)
	{
		const APawn* Self = Player ? Player->GetPawn() : nullptr;
		if (!Self || Args.Num() < 4) { OutWhy = TEXT("no machine there"); return nullptr; }
		const FVector At(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
		for (TActorIterator<AWRBuildPiece> It(Player->GetWorld()); It; ++It)
		{
			if (It->bGhost || FVector::Dist(It->GetActorLocation(), At) > 60.f) continue;
			const FWRPieceDef* Def = It->GetDef();
			if (!Def || !FWRMachines::IsMachine(*Def)) continue;
			if (FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > 900.f) { OutWhy = TEXT("too far from it"); return nullptr; }
			return *It;
		}
		OutWhy = TEXT("no machine there");
		return nullptr;
	}

	/** May this item go into this machine's input (what its recipes take, its fuel...)? */
	bool Takes(const FWRMachineDef& M, FName Item)
	{
		if (M.Role == TEXT("source")) return Item == FName(*FWRMachineDefs::Get().RuleStr(TEXT("soul.fuel"), TEXT("soulstone")));
		if (M.Role == TEXT("still") || M.Role == TEXT("vat")) return Item == M.Input;
		if (M.Role == TEXT("sorter") || M.Role == TEXT("riftchest")) return true;
		if (M.Role == TEXT("turret")) return UWRTurretSubsystem::AmmoFor(M.Piece, Item) != nullptr;
		if (M.IsProcessor())
		{
			for (const FWRRecipe* Rec : FWRRecipeRegistry::Get().ForStation(M.Station))
			{
				for (const TPair<FName, int32>& In : Rec->Inputs) if (In.Key == Item) return true;
			}
		}
		return false;
	}

	int32 InSlots(const FWRMachineDef& M)
	{
		if (M.Role == TEXT("riftchest")) return FMath::RoundToInt(R(TEXT("rift.slots"), 60.f));
		if (M.Role == TEXT("sorter")) return M.In;
		return M.In;
	}
}

bool FWRMachines::RunCommand(AWRPlayerController* Player, const TArray<FString>& Args, FString& OutResult)
{
	using namespace WRMachineDetail;
	if (Args.Num() == 0) return false;
	const FString& Cmd = Args[0];
	static const TSet<FString> Mine = { TEXT("mon"), TEXT("moverclock"), TEXT("mput"), TEXT("mtake"), TEXT("mtakeall"), TEXT("mrepair"), TEXT("mlink"), TEXT("msend") };
	if (!Mine.Contains(Cmd)) return false;
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	AWRBuildPiece* Piece = PieceAt(Player, Args, OutResult);
	const FWRMachineDef* M = DefOf(Piece);
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	UWRMachineSubsystem* Sub = UWRMachineSubsystem::Get(Player);
	if (!Piece || !M || !Def || !Inv || !Sub)
	{
		if (OutResult.IsEmpty()) OutResult = TEXT("no machine there");
		return true;
	}
	const FString Who = PS->GetPlayerName();
	if (!Piece->CanAccess(Who))
	{
		OutResult = FString::Printf(TEXT("That's %s's"), Piece->OwnerName.IsEmpty() ? TEXT("someone else") : *Piece->OwnerName);
		return true;
	}
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const int32 In = InSlots(*M);
	FWRMachineState& S = Piece->Mach;
	if (Cmd == TEXT("mon"))
	{
		S.bOn = !S.bOn;
		Piece->Machine.Flags = S.bOn ? (Piece->Machine.Flags | WRMachineFlag::On) : (Piece->Machine.Flags & ~WRMachineFlag::On);
		OutResult = S.bOn ? FString::Printf(TEXT("%s switched on"), *Def->Name) : FString::Printf(TEXT("%s switched off"), *Def->Name);
	}
	else if (Cmd == TEXT("moverclock"))
	{
		if (!S.bOverclock && UWRLifeSkillsComponent::StatFor(Player, TEXT("overclock")) <= 0.f) OutResult = TEXT("needs the Overclock perk (Engineering 75)");
		else
		{
			S.bOverclock = !S.bOverclock;
			Piece->Machine.Flags = S.bOverclock ? (Piece->Machine.Flags | WRMachineFlag::Overclock) : (Piece->Machine.Flags & ~WRMachineFlag::Overclock);
			OutResult = S.bOverclock ? TEXT("Overclocked: half again as fast, twice the power") : TEXT("Back to its normal pace");
		}
	}
	else if (Cmd == TEXT("mput"))
	{
		TArray<FWRItemStack>& Bag = Inv->GetSlotsMutable();
		const int32 Slot = FCString::Atoi(*Arg(4));
		if (In <= 0 || M->Role == TEXT("drill")) OutResult = TEXT("nothing goes into it by hand");
		else if (!Bag.IsValidIndex(Slot) || Bag[Slot].IsEmpty()) OutResult = TEXT("empty slot");
		else if (!Takes(*M, Bag[Slot].ItemId)) OutResult = FString::Printf(TEXT("The %s doesn't take %s"), *Def->Name.ToLower(), *ItemName(Bag[Slot].ItemId).ToLower());
		else
		{
			TArray<FWRItemStack> Part = Slice(Piece->Storage, 0, In);
			const FName Item = Bag[Slot].ItemId;
			const int32 Want = Arg(5).IsEmpty() ? Bag[Slot].Count : FMath::Clamp(FCString::Atoi(*Arg(5)), 1, Bag[Slot].Count);
			const int32 Moved = FWRInventoryRules::MoveFrom(Bag[Slot], Part, Want);
			Put(Piece->Storage, 0, Part);
			OutResult = Moved > 0 ? FString::Printf(TEXT("Put in %d %s"), Moved, *ItemName(Item).ToLower()) : FString::Printf(TEXT("The %s is full"), *Def->Name.ToLower());
			if (Moved > 0 && M->Role == TEXT("riftchest")) Sub->SyncRift(Piece);
		}
	}
	else if (Cmd == TEXT("mtake") || Cmd == TEXT("mtakeall"))
	{
		int32 Taken = 0;
		bool bFull = false;
		const auto TakeSlot = [&](int32 i)
		{
			if (!Piece->Storage.IsValidIndex(i) || Piece->Storage[i].IsEmpty()) return;
			const int32 Moved = FWRInventoryRules::MoveFrom(Piece->Storage[i], Inv->GetSlotsMutable(), Piece->Storage[i].Count);
			Taken += Moved;
			bFull |= !Piece->Storage[i].IsEmpty();
		};
		if (Cmd == TEXT("mtake")) TakeSlot(FCString::Atoi(*Arg(4)));
		else
		{
			// Take all: what it made (and, for chests of its own, everything).
			const bool bAll = M->Role == TEXT("sorter") || M->Role == TEXT("riftchest") || M->Role == TEXT("drill");
			for (int32 i = bAll ? 0 : In; i < Piece->Storage.Num(); ++i) TakeSlot(i);
		}
		if (M->Role == TEXT("riftchest") && Taken > 0) Sub->SyncRift(Piece);
		if (M->Role == TEXT("drill") && Taken > 0)
		{
			// Emptying a drill's hopper is mining (survival-crafting 2.23): 1 Mining XP per 10 ore.
			UWRLifeSkillsComponent::Award(Player, TEXT("mining"), TEXT("drillOre"), 1.f, Taken / FMath::Max(1.f, R(TEXT("drillMiningXpPer"), 10.f)));
		}
		OutResult = Taken > 0 ? FString::Printf(TEXT("Took %d%s"), Taken, bFull ? TEXT(": your bag is full") : TEXT("")) : bFull ? TEXT("Your bag is full") : TEXT("Nothing to take");
	}
	else if (Cmd == TEXT("mrepair"))
	{
		const TArray<TPair<FName, int32>> Cost = RepairCost(Piece);
		if (Cost.Num() == 0) OutResult = TEXT("It's in good repair");
		else
		{
			for (const TPair<FName, int32>& C : Cost)
			{
				if (CanPay(Inv, C.Key) < C.Value) { OutResult = FString::Printf(TEXT("need %d %s to repair it"), C.Value, *ItemName(C.Key).ToLower()); break; }
			}
			if (OutResult.IsEmpty())
			{
				for (const TPair<FName, int32>& C : Cost) Pay(Inv, C.Key, C.Value);
				Piece->Health = Piece->MaxHealth;
				Piece->Machine.Flags &= ~WRMachineFlag::Worn;
				UWRLifeSkillsComponent::Award(Player, TEXT("engineering"), TEXT("machineRepair"), 10.f);
				OutResult = FString::Printf(TEXT("%s repaired"), *Def->Name);
			}
		}
	}
	else if (Cmd == TEXT("mlink"))
	{
		OutResult = Sub->LinkLift(Piece, Player);
	}
	else if (Cmd == TEXT("msend"))
	{
		OutResult = Sub->SendLift(Piece);
	}
	Sub->Touch();
	UE_LOG(LogWildreach, Log, TEXT("MACHINE %s %s %s: %s"), *Who, *Cmd, *Piece->PieceId.ToString(), *OutResult);
	return true;
}

// ---------------------------------------------------------------- subsystem

UWRMachineSubsystem* UWRMachineSubsystem::Get(const UObject* WorldContext)
{
	using namespace WRMachineDetail;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRMachineSubsystem>() : nullptr;
}

TStatId UWRMachineSubsystem::GetStatId() const
{
	using namespace WRMachineDetail;
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRMachineSubsystem, STATGROUP_Tickables);
}

void UWRMachineSubsystem::Register(AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	if (Piece && !Piece->bGhost && Piece->HasAuthority())
	{
		Pieces.AddUnique(Piece);
		bDirty = true;
		// A new rift chest opens onto its clan's shared slots (a loaded one already holds the same).
		if (Piece->PieceId == TEXT("rift_chest") && !Piece->Storage.ContainsByPredicate([](const FWRItemStack& X) { return !X.IsEmpty(); }))
		{
			const FString Group = FWRClans::Get().ClanOf(Piece->OwnerName).IsEmpty() ? Piece->OwnerName : FWRClans::Get().ClanOf(Piece->OwnerName);
			for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
			{
				const AWRBuildPiece* Other = W.Get();
				if (!Other || Other == Piece || Other->PieceId != TEXT("rift_chest")) continue;
				const FString OG = FWRClans::Get().ClanOf(Other->OwnerName).IsEmpty() ? Other->OwnerName : FWRClans::Get().ClanOf(Other->OwnerName);
				if (OG == Group) { Piece->Storage = Other->Storage; break; }
			}
		}
	}
}

void UWRMachineSubsystem::Unregister(AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	if (Pieces.Remove(Piece) > 0)
	{
		bDirty = true;
	}
}

bool UWRMachineSubsystem::IsAwake(const FVector& At) const
{
	using namespace WRMachineDetail;
	const float Range = R(TEXT("activeRange"), 25000.f);
	const UWorld* World = GetWorld();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && FVector::DistSquared(P->GetActorLocation(), At) < Range * Range) return true;
	}
	return false;
}

float UWRMachineSubsystem::StationBoost(const AWRBuildPiece* StationPiece) const
{
	using namespace WRMachineDetail;
	const float* B = StationBoosts.Find(StationPiece);
	return B ? *B : 1.f;
}

TArray<FVector> UWRMachineSubsystem::PumpOutlets() const
{
	using namespace WRMachineDetail;
	TArray<FVector> Out;
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (M && M->Role == TEXT("pump") && (P->Machine.State == EWRMachineState::Running || P->Machine.State == EWRMachineState::Slow))
		{
			Out.Add(P->GetActorTransform().TransformPosition(FWRMachineLook::PumpOutlet()));
		}
	}
	return Out;
}

bool UWRMachineSubsystem::GrowLightNear(const FVector& At, float Reach) const
{
	using namespace WRMachineDetail;
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (M && M->Role == TEXT("growlight") && (P->Machine.Flags & WRMachineFlag::Lit) && FVector::DistSquared(P->GetActorLocation(), At) < Reach * Reach) return true;
	}
	return false;
}

bool UWRMachineSubsystem::ColdStoreFor(const AWRBuildPiece* Container) const
{
	using namespace WRMachineDetail;
	if (!Container) return false;
	const float Reach = R(TEXT("coldStoreReach"), 800.f);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (!M || M->Role != TEXT("coldstore") || !(P->Machine.Flags & WRMachineFlag::Working)) continue;
		if (FVector::DistSquared(P->GetActorLocation(), Container->GetActorLocation()) > Reach * Reach) continue;
		// The same room: both under a roof (the same one or another of the same building).
		if (Covered(GetWorld(), P->GetActorLocation(), P) && Covered(GetWorld(), Container->GetActorLocation(), Container)) return true;
	}
	return false;
}

bool UWRMachineSubsystem::HasOtherRift(const AWRBuildPiece* Piece) const
{
	using namespace WRMachineDetail;
	if (!Piece) return false;
	const FString Group = FWRClans::Get().ClanOf(Piece->OwnerName).IsEmpty() ? Piece->OwnerName : FWRClans::Get().ClanOf(Piece->OwnerName);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* P = W.Get();
		if (!P || P == Piece || P->PieceId != TEXT("rift_chest")) continue;
		const FString Other = FWRClans::Get().ClanOf(P->OwnerName).IsEmpty() ? P->OwnerName : FWRClans::Get().ClanOf(P->OwnerName);
		if (Other == Group) return true;
	}
	return false;
}

void UWRMachineSubsystem::SyncRift(AWRBuildPiece* Changed)
{
	using namespace WRMachineDetail;
	if (!Changed) return;
	const FString Group = FWRClans::Get().ClanOf(Changed->OwnerName).IsEmpty() ? Changed->OwnerName : FWRClans::Get().ClanOf(Changed->OwnerName);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		if (!P || P == Changed || P->PieceId != TEXT("rift_chest")) continue;
		const FString Other = FWRClans::Get().ClanOf(P->OwnerName).IsEmpty() ? P->OwnerName : FWRClans::Get().ClanOf(P->OwnerName);
		if (Other == Group) P->Storage = Changed->Storage;
	}
}

AWRBuildPiece* UWRMachineSubsystem::ContainerAt(const FVector& At, float Reach, const AWRBuildPiece* Ignore) const
{
	using namespace WRMachineDetail;
	AWRBuildPiece* Best = nullptr;
	float BestD = Reach;
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRPieceDef* Def = P && P != Ignore ? P->GetDef() : nullptr;
		if (!Def) continue;
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		const bool bHolds = Def->Kind == TEXT("chest") || (M && (M->IsProcessor() || M->Role == TEXT("sorter") || M->Role == TEXT("drill") || M->Role == TEXT("riftchest") || M->Role == TEXT("still") || M->Role == TEXT("vat") || (M->IsSource() && M->In > 0)));
		if (!bHolds) continue;
		const float D = FMath::Sqrt(Bounds(P).ComputeSquaredDistanceToPoint(At));
		if (D < BestD) { BestD = D; Best = P; }
	}
	return Best;
}

void UWRMachineSubsystem::Rebuild()
{
	using namespace WRMachineDetail;
	bDirty = false;
	Pieces.RemoveAll([](const TWeakObjectPtr<AWRBuildPiece>& W) { return !W.IsValid(); });
	Nets.Reset();
	NetOf.Reset();
	StationNet.Reset();
	Heads.Reset();
	Tails.Reset();
	const float Snap = R(TEXT("portSnap"), 50.f);

	// Every powered piece and its ports; join pieces whose ports meet (union-find).
	TArray<AWRBuildPiece*> Powered;
	TArray<TArray<FWRPort>> PortsOf;
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (M && M->IsPowered())
		{
			Powered.Add(P);
			PortsOf.Add(FWRMachines::Ports(P));
		}
	}
	TArray<int32> Parent;
	for (int32 i = 0; i < Powered.Num(); ++i) Parent.Add(i);
	const TFunction<int32(int32)> Find = [&Parent, &Find](int32 i) -> int32 { return Parent[i] == i ? i : Parent[i] = Find(Parent[i]); };
	const auto Join = [&Parent, &Find](int32 a, int32 b) { a = Find(a); b = Find(b); if (a != b) Parent[b] = a; };
	for (int32 i = 0; i < Powered.Num(); ++i)
	{
		for (int32 j = i + 1; j < Powered.Num(); ++j)
		{
			if (FVector::DistSquared(Powered[i]->GetActorLocation(), Powered[j]->GetActorLocation()) > 2500.f * 2500.f) continue;
			bool bMeet = false;
			for (const FWRPort& A : PortsOf[i]) for (const FWRPort& B : PortsOf[j]) bMeet |= FVector::DistSquared(A.At, B.At) < Snap * Snap;
			if (bMeet) Join(i, j);
		}
	}
	// Resonance pylons: every powered piece within reach shares one network (no shafts).
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		const AWRBuildPiece* Pylon = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(Pylon);
		if (!M || M->Role != TEXT("pylon") || !Pylon->Mach.bOn) continue;
		const float Reach = R(TEXT("pylon.reach"), 4000.f);
		int32 First = INDEX_NONE;
		for (int32 i = 0; i < Powered.Num(); ++i)
		{
			if (FVector::DistSquared(Powered[i]->GetActorLocation(), Pylon->GetActorLocation()) > Reach * Reach) continue;
			if (First == INDEX_NONE) First = i; else Join(First, i);
		}
	}
	TMap<int32, int32> RootToNet;
	for (int32 i = 0; i < Powered.Num(); ++i)
	{
		const int32 Root = Find(i);
		int32* Net = RootToNet.Find(Root);
		if (!Net)
		{
			Net = &RootToNet.Add(Root, Nets.AddDefaulted());
		}
		Nets[*Net].Pieces.Add(Powered[i]);
		NetOf.Add(Powered[i], *Net);
		const FWRMachineDef* M = FWRMachines::DefOf(Powered[i]);
		if (M && M->IsSource() && Nets[*Net].Owner.IsEmpty()) Nets[*Net].Owner = Powered[i]->OwnerName;
	}
	for (FNet& N : Nets) if (N.Owner.IsEmpty() && N.Pieces.Num() > 0 && N.Pieces[0].IsValid()) N.Owner = N.Pieces[0]->OwnerName;
	// Pylons show the network they gather.
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* Pylon = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(Pylon);
		if (!M || M->Role != TEXT("pylon")) continue;
		for (int32 i = 0; i < Powered.Num(); ++i)
		{
			if (FVector::DistSquared(Powered[i]->GetActorLocation(), Pylon->GetActorLocation()) < FMath::Square(R(TEXT("pylon.reach"), 4000.f)))
			{
				NetOf.Add(Pylon, NetOf.FindRef(Powered[i]));
				break;
			}
		}
	}

	// Stations a shaft runs into: a shaft / gearbox port within reach of the station's body.
	const float Reach = R(TEXT("stationReach"), 50.f);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRPieceDef* Def = P ? P->GetDef() : nullptr;
		if (!Def || Def->Kind != TEXT("station") || !P->GetChild()) continue;
		const FBox Box = Bounds(P->GetChild()).ExpandBy(Reach);
		for (int32 i = 0; i < Powered.Num(); ++i)
		{
			const FWRMachineDef* M = FWRMachines::DefOf(Powered[i]);
			if (!M || (M->Role != TEXT("shaft") && M->Role != TEXT("gearbox"))) continue;
			bool bIn = false;
			for (const FWRPort& Port : PortsOf[i]) bIn |= Box.IsInside(Port.At);
			if (bIn)
			{
				StationNet.Add(P, NetOf.FindRef(Powered[i]));
				Nets[NetOf.FindRef(Powered[i])].Stations.Add(P);
				break;
			}
		}
	}

	// Belts: what each one's head gives to and its tail takes from.
	const float Hand = R(TEXT("conveyorDeliverReach"), 70.f);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (!M || M->Role != TEXT("conveyor")) continue;
		const FVector Head = P->GetActorTransform().TransformPosition(FWRMachineLook::BeltEnd(*M, true));
		const FVector Tail = P->GetActorTransform().TransformPosition(FWRMachineLook::BeltEnd(*M, false));
		// A belt whose tail is at this head comes first; else a container.
		AWRBuildPiece* Next = nullptr;
		for (const TWeakObjectPtr<AWRBuildPiece>& V : Pieces)
		{
			AWRBuildPiece* Q = V.Get();
			const FWRMachineDef* QM = FWRMachines::DefOf(Q);
			if (Q && Q != P && QM && QM->Role == TEXT("conveyor") && FVector::Dist(Q->GetActorTransform().TransformPosition(FWRMachineLook::BeltEnd(*QM, false)), Head) < Hand) { Next = Q; break; }
		}
		if (!Next) Next = ContainerAt(Head, Hand, P);
		if (Next) Heads.Add(P, Next);
		bool bFedByBelt = false;
		for (const TWeakObjectPtr<AWRBuildPiece>& V : Pieces)
		{
			const AWRBuildPiece* Q = V.Get();
			const FWRMachineDef* QM = FWRMachines::DefOf(Q);
			bFedByBelt |= Q && Q != P && QM && QM->Role == TEXT("conveyor") && FVector::Dist(Q->GetActorTransform().TransformPosition(FWRMachineLook::BeltEnd(*QM, true)), Tail) < Hand;
		}
		if (!bFedByBelt)
		{
			if (AWRBuildPiece* From = ContainerAt(Tail, Hand, P)) Tails.Add(P, From);
		}
	}

	// Drills: the vein under each one never runs out while it's drilled.
	for (TActorIterator<AWRResourceNode> It(GetWorld()); It; ++It) It->bDrilled = false;
	Veins.Reset();
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (!M || M->Role != TEXT("drill")) continue;
		AWRResourceNode* Best = nullptr;
		float BestD = R(TEXT("drillReach"), 150.f);
		for (TActorIterator<AWRResourceNode> It(GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), P->GetActorLocation());
			if (IsOre(It->ItemId) && D < BestD) { BestD = D; Best = *It; }
		}
		if (Best) { Best->bDrilled = true; Veins.Add(P, Best); }
	}
	// Pumps: is there water to draw (cached: the sea doesn't move).
	PumpWater.Reset();
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (M && M->Role == TEXT("pump")) PumpWater.Add(P, WaterWithin(P, P->GetActorLocation(), R(TEXT("pumpWaterRange"), 4000.f)));
	}
	UE_LOG(LogWildreach, Verbose, TEXT("MACHINE rebuilt: %d pieces, %d networks, %d stations powered, %d belts joined"), Pieces.Num(), Nets.Num(), StationNet.Num(), Heads.Num());
}

void UWRMachineSubsystem::RefreshOwnerPerks(AWRBuildPiece* P) const
{
	using namespace WRMachineDetail;
	// The owner's perks and every-level speed as they stand while they're on (kept for when they're away).
	if (const APlayerState* PS = Online(GetWorld(), P->OwnerName))
	{
		FWRMachineState& S = P->Mach;
		S.OwnerSpeed = UWRLifeSkillsComponent::StatFor(PS, TEXT("machineSpeed"));
		S.bOwnerEfficient = UWRLifeSkillsComponent::StatFor(PS, TEXT("machinePower")) < 0.f;
		S.bOwnerSelfRepair = UWRLifeSkillsComponent::StatFor(PS, TEXT("selfRepair")) > 0.f;
		S.bOwnerChief = UWRLifeSkillsComponent::StatFor(PS, TEXT("chiefEngineer")) > 0.f;
	}
}

void UWRMachineSubsystem::Tick(float DeltaTime)
{
	using namespace WRMachineDetail;
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
	RebuildClock -= DeltaTime;
	if (bDirty || RebuildClock <= 0.f)
	{
		Rebuild();
		RebuildClock = 10.f;   // ore nodes respawning, pieces streaming in: a slow look again
	}
	StepClock += DeltaTime;
	const float StepSeconds = FMath::Max(0.1f, R(TEXT("stepSeconds"), 0.5f));
	if (StepClock >= StepSeconds)
	{
		Step(StepClock);
		StepClock = 0.f;
	}
}

void UWRMachineSubsystem::StepSource(AWRBuildPiece* P, const FWRMachineDef& D, float Dt, float& OutSupply, EWRMachineState& OutState, float& OutFactor)
{
	using namespace WRMachineDetail;
	OutSupply = 0.f;
	OutFactor = 1.f;
	OutState = EWRMachineState::Running;
	const FTransform T = P->GetActorTransform();
	FWRMachineState& S = P->Mach;
	const FName F = D.Flow;
	if (!S.bOn) { OutState = EWRMachineState::Off; return; }
	if (F == TEXT("water"))
	{
		OutFactor = FWRMachines::FlowAt(P, T.TransformPosition(FVector(R(TEXT("flow.wheelOut"), 300.f), 0.f, 0.f)), false);
		if (OutFactor <= 0.f) OutState = EWRMachineState::NoWater;
		OutSupply = D.Supply * OutFactor;
	}
	else if (F == TEXT("tide"))
	{
		const float Flow = FWRMachines::FlowAt(P, T.TransformPosition(FVector(R(TEXT("flow.wheelOut"), 300.f), 0.f, 0.f)), true);
		const AWRGameState* GS = AWRGameState::Get(P);
		const double Hours = GS ? GS->GetTotalHours() : 0.0;
		const float Tide = 0.5f + 0.5f * FMath::Sin(2.f * PI * static_cast<float>(FMath::Fmod(Hours, 1000.0)) / FMath::Max(1.f, R(TEXT("tide.hours"), 12.42f)));
		OutFactor = Flow > 0.f ? FMath::Lerp(1.f, D.SupplyHigh / FMath::Max(1.f, D.Supply), Tide) : 0.f;
		if (Flow <= 0.f) OutState = EWRMachineState::NoWater;
		OutSupply = D.Supply * OutFactor;
	}
	else if (F == TEXT("wind"))
	{
		S.BlockedCheck -= Dt;
		if (S.BlockedCheck <= 0.f)
		{
			// Something built later can shelter it too: look round now and then.
			S.BlockedCheck = 15.f;
			S.bBlocked = false;
			const float Hub = R(TEXT("wind.hubHeight"), 710.f);
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRMachineWind), false, P);
			for (int32 i = 0; i < 16 && !S.bBlocked; ++i)
			{
				const float A = i * PI / 8.f;
				FHitResult Hit;
				const FVector From = P->GetActorLocation() + FVector(0.f, 0.f, Hub);
				const FVector To = From + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R(TEXT("wind.clear"), 1000.f);
				// Start clear of its own sails (4.6 m out).
				S.bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, From + (To - From).GetSafeNormal() * 480.f, To, ECC_WorldStatic, Q);
			}
		}
		float Yaw = 0.f;
		OutFactor = S.bBlocked ? 0.f : FWRMachines::WindAt(P, P->GetActorLocation(), Yaw);
		if (S.bBlocked) OutState = EWRMachineState::NoWind;
		OutSupply = D.Supply * OutFactor;
	}
	else if (F == TEXT("lava"))
	{
		const bool bHeat = FWRMachines::LavaNear(T.TransformPosition(FVector(130.f, 0.f, 0.f)), R(TEXT("lava.reach"), 600.f));
		OutFactor = bHeat ? 1.f : 0.f;
		if (!bHeat) OutState = EWRMachineState::NoHeat;
		OutSupply = D.Supply * OutFactor;
	}
	else if (F == TEXT("soul"))
	{
		if (S.FuelLeft <= 0.f)
		{
			const FName Fuel(*FWRMachineDefs::Get().RuleStr(TEXT("soul.fuel"), TEXT("soulstone")));
			TArray<FWRItemStack> In = Slice(P->Storage, 0, D.In);
			if (FWRInventoryRules::Remove(In, Fuel, 1) == 0)
			{
				Put(P->Storage, 0, In);
				S.FuelLeft = 3600.f * R(TEXT("soul.hoursPerFuel"), 1.f);
			}
		}
		OutFactor = S.FuelLeft > 0.f ? 1.f : 0.f;
		if (S.FuelLeft <= 0.f) OutState = EWRMachineState::NoFuel;
		OutSupply = D.Supply * OutFactor;
		P->Machine.Charge = static_cast<uint8>(FMath::Clamp(S.FuelLeft / (3600.f * R(TEXT("soul.hoursPerFuel"), 1.f)), 0.f, 1.f) * 255.f);
	}
	else if (F == TEXT("storm") || F == TEXT("sun"))
	{
		// Live power when the sky gives it; the battery after (topped up from the surplus in Step).
		float Live = 0.f;
		if (F == TEXT("storm"))
		{
			const FWRWeatherKind* Kind = UWRWeatherSubsystem::ActiveKind(P, WRBiome::At(P, P->GetActorLocation()));
			Live = Kind && Kind->Id.ToString() == FWRMachineDefs::Get().RuleStr(TEXT("storm.kind"), TEXT("thunderstorm")) ? D.Supply : 0.f;
		}
		else
		{
			const AWRGameState* GS = AWRGameState::Get(P);
			Live = D.Supply * (GS ? GS->GetDaylight() : 1.f);
		}
		const float BatteryP = F == TEXT("storm") ? R(TEXT("storm.batteryP"), 20.f) : R(TEXT("sun.batteryP"), 10.f);
		S.LiveP = Live > 0.5f ? Live : 0.f;
		OutSupply = Live > 0.5f ? Live : (S.Charge > 0.001f ? BatteryP : 0.f);
		OutFactor = D.Supply > 0.f ? OutSupply / D.Supply : 0.f;
		if (OutSupply <= 0.f) OutState = F == TEXT("storm") ? EWRMachineState::NoStorm : EWRMachineState::NoSun;
	}
	if ((P->Machine.Flags & WRMachineFlag::Worn) && OutSupply > 0.f)
	{
		OutSupply *= R(TEXT("wornSpeed"), 0.5f);
		OutState = EWRMachineState::Worn;
	}
}

const FWRRecipe* UWRMachineSubsystem::PickRecipe(AWRBuildPiece* P, const FWRMachineDef& D, bool& bOutFull) const
{
	using namespace WRMachineDetail;
	bOutFull = false;
	const TArray<FWRItemStack> In = Slice(P->Storage, 0, D.In);
	const int32 Level = FWRMachines::OwnedHearthLevel(P, P->OwnerName);
	const FWRRecipe* Best = nullptr;
	int32 BestSlot = MAX_int32;
	for (const FWRRecipe* Rec : FWRRecipeRegistry::Get().ForStation(D.Station))
	{
		if (Rec->Level > Level) continue;
		bool bHas = true;
		int32 First = MAX_int32;
		for (const TPair<FName, int32>& Need : Rec->Inputs)
		{
			bHas &= FWRInventoryRules::CountOf(In, Need.Key) >= Need.Value;
			for (int32 i = 0; i < In.Num(); ++i) if (In[i].ItemId == Need.Key) { First = FMath::Min(First, i); break; }
		}
		if (bHas && First < BestSlot) { Best = Rec; BestSlot = First; }
	}
	if (Best)
	{
		TArray<FWRItemStack> Out = Slice(P->Storage, D.In, D.Out);
		if (FWRInventoryRules::Add(Out, Best->Output, Best->Count) > 0) { bOutFull = true; }
	}
	return Best;
}

float UWRMachineSubsystem::Demand(AWRBuildPiece* P, const FWRMachineDef& D, EWRMachineState& OutState)
{
	using namespace WRMachineDetail;
	FWRMachineState& S = P->Mach;
	OutState = EWRMachineState::Idle;
	if (!S.bOn) { OutState = EWRMachineState::Off; return 0.f; }
	const FName Role = D.Role;
	float Load = 0.f;
	if (D.IsProcessor())
	{
		bool bFull = false;
		const FWRRecipe* Rec = PickRecipe(P, D, bFull);
		if (Rec && bFull) OutState = EWRMachineState::OutputFull;
		else if (Rec) Load = D.Load;
	}
	else if (Role == TEXT("conveyor"))
	{
		Load = D.Load;   // a powered belt runs
	}
	else if (Role == TEXT("sorter"))
	{
		for (const FWRItemStack& X : P->Storage) if (!X.IsEmpty()) { Load = D.Load; break; }
	}
	else if (Role == TEXT("drill"))
	{
		const AWRResourceNode* Vein = Veins.FindRef(P).Get();
		TArray<FWRItemStack> Hopper = Slice(P->Storage, 0, D.Out);
		if (Vein && FWRInventoryRules::Add(Hopper, Vein->ItemId, 1) > 0) OutState = EWRMachineState::OutputFull;
		else if (Vein) Load = D.Load;
	}
	else if (Role == TEXT("pump"))
	{
		if (PumpWater.FindRef(P)) Load = D.Load; else OutState = EWRMachineState::NoWater;
	}
	else if (Role == TEXT("lamp"))
	{
		if (AWRGameState::IsNightIn(P)) Load = D.Load;
	}
	else if (Role == TEXT("growlight"))
	{
		Load = D.Load;
	}
	else if (Role == TEXT("coldstore"))
	{
		if (D.FreeIn.IsNone() || WRBiome::At(P, P->GetActorLocation()) != D.FreeIn) Load = D.Load;
	}
	else if (Role == TEXT("turret"))
	{
		// A turret draws its power while it has something to shoot (it keeps its bow spanned, its vessel pumped).
		if (UWRTurretSubsystem::IsArmed(P)) Load = D.Load; else OutState = EWRMachineState::NoAmmo;
	}
	if (Load > 0.f)
	{
		if (S.bOwnerEfficient) Load *= R(TEXT("efficientPower"), 0.8f);
		if (S.bOverclock) Load *= R(TEXT("overclockPower"), 2.f);
	}
	return Load;
}

void UWRMachineSubsystem::StepProcessor(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt)
{
	using namespace WRMachineDetail;
	FWRMachineState& S = P->Mach;
	bool bFull = false;
	const FWRRecipe* Rec = PickRecipe(P, D, bFull);
	if (!Rec || bFull)
	{
		S.Recipe = NAME_None;
		S.Progress = 0.f;
		P->Machine.Progress = 0;
		P->Machine.Making = Rec ? Rec->Output : NAME_None;
		return;
	}
	if (S.Recipe != Rec->Id) { S.Recipe = Rec->Id; S.Progress = 0.f; }
	S.Progress += Dt * Rate;
	const float Seconds = FMath::Max(0.5f, Rec->Seconds);
	if (S.Progress >= Seconds)
	{
		S.Progress = 0.f;
		TArray<FWRItemStack> In = Slice(P->Storage, 0, D.In);
		TArray<FWRItemStack> Out = Slice(P->Storage, D.In, D.Out);
		for (const TPair<FName, int32>& Need : Rec->Inputs) FWRInventoryRules::Remove(In, Need.Key, Need.Value);
		FWRInventoryRules::Add(Out, Rec->Output, Rec->Count);
		// The grove press: now and then a rare seed of a land in the game.
		if (D.Role == TEXT("press") && FMath::FRand() < R(TEXT("press.rareSeedChance"), 0.1f))
		{
			TArray<FName> Seeds;
			for (const FWRCropDef& C : FWRFarmDefs::Get().Crops()) if (C.bRare && C.IsLive()) Seeds.Add(C.Seed);
			if (Seeds.Num() > 0) FWRInventoryRules::Add(Out, Seeds[FMath::RandHelper(Seeds.Num())], 1);
		}
		Put(P->Storage, 0, In);
		Put(P->Storage, D.In, Out);
		UE_LOG(LogWildreach, Verbose, TEXT("MACHINE %s made %d %s"), *P->PieceId.ToString(), Rec->Count, *Rec->Output.ToString());
	}
	P->Machine.Progress = static_cast<uint8>(FMath::Clamp(S.Progress / Seconds, 0.f, 1.f) * 255.f);
	P->Machine.Making = Rec->Output;
}

int32 UWRMachineSubsystem::ChainDepth(const AWRBuildPiece* P, int32 Guard) const
{
	using namespace WRMachineDetail;
	// Machines along the belts that feed P, counting P's own feeders back to a chest or a drill.
	if (!P || Guard > 16) return 0;
	int32 Best = 0;
	for (const TPair<TWeakObjectPtr<AWRBuildPiece>, TWeakObjectPtr<AWRBuildPiece>>& H : Heads)
	{
		if (H.Value.Get() != P) continue;
		// A belt runs into P: follow it back to what fed it.
		const AWRBuildPiece* Belt = H.Key.Get();
		for (int32 Hop = 0; Belt && Hop < 32; ++Hop)
		{
			const AWRBuildPiece* From = Tails.FindRef(Belt).Get();
			if (From)
			{
				const FWRMachineDef* M = FWRMachines::DefOf(From);
				if (M && (M->IsProcessor() || M->Role == TEXT("drill") || M->Role == TEXT("sorter"))) Best = FMath::Max(Best, 1 + ChainDepth(From, Guard + 1));
				break;
			}
			// Another belt before this one?
			const AWRBuildPiece* Prev = nullptr;
			for (const TPair<TWeakObjectPtr<AWRBuildPiece>, TWeakObjectPtr<AWRBuildPiece>>& G : Heads) if (G.Value.Get() == Belt) { Prev = G.Key.Get(); break; }
			Belt = Prev;
		}
	}
	return Best;
}

void UWRMachineSubsystem::StepConveyor(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt)
{
	using namespace WRMachineDetail;
	FWRMachineState& S = P->Mach;
	FWRMachineRep& Rep = P->Machine;
	const float Spacing = 100.f * R(TEXT("beltSeconds"), 2.f);
	const float Len = D.Length;
	const double Now = ServerTime(P);
	// After a load the stacks are back but not their places: lay them out again from the head, a belt's spacing apart.
	P->Storage.RemoveAll([](const FWRItemStack& X) { return X.IsEmpty(); });
	if (P->Belt.Num() != P->Storage.Num())
	{
		P->Belt.Reset();
		for (int32 i = 0; i < P->Storage.Num(); ++i)
		{
			FWRBeltItem& B = P->Belt.AddDefaulted_GetRef();
			B.Item = P->Storage[i].ItemId;
			B.Count = P->Storage[i].Count;
			B.Enter = Rep.BeltD - Len + i * Spacing;
		}
	}
	// An empty belt starts its count again (the run is a float: keep it small).
	if (P->Belt.Num() == 0 && Rep.BeltD > 100000.f) Rep.BeltD = 0.f;
	// Deliver what reached the head.
	bool bJam = false;
	AWRBuildPiece* Next = Heads.FindRef(P).Get();
	for (int32 i = 0; i < P->Belt.Num(); )
	{
		if (Rep.BeltD - P->Belt[i].Enter < Len) { ++i; continue; }
		bool bGiven = false;
		FWRItemStack& Stack = P->Storage.IsValidIndex(i) ? P->Storage[i] : P->Storage.AddDefaulted_GetRef();
		const FWRMachineDef* NM = FWRMachines::DefOf(Next);
		if (Next && NM && NM->Role == TEXT("conveyor"))
		{
			// Onto the next belt when its tail has room.
			const bool bRoom = Next->Belt.Num() == 0 || Next->Machine.BeltD - Next->Belt.Last().Enter >= Spacing;
			if (bRoom && Next->Belt.Num() < FMath::FloorToInt(NM->Length / Spacing) + 1)
			{
				FWRBeltItem& B = Next->Belt.AddDefaulted_GetRef();
				B.Item = Stack.ItemId;
				B.Count = Stack.Count;
				B.Enter = Next->Machine.BeltD;
				Next->Storage.Add(Stack);
				bGiven = true;
			}
		}
		else if (Next)
		{
			const FWRPieceDef* ND = Next->GetDef();
			int32 From = 0, Num = Next->Storage.Num();
			if (NM)
			{
				const bool bIntoMachine = NM->IsProcessor() || NM->Role == TEXT("sorter") || NM->Role == TEXT("still") || NM->Role == TEXT("vat") || NM->IsSource();
				const int32 Limit = FMath::RoundToInt(S.bOwnerChief ? R(TEXT("chiefChain"), 6.f) : R(TEXT("chain"), 4.f));
				if ((NM->IsProcessor() || NM->Role == TEXT("sorter")) && ChainDepth(Next) + 1 > Limit)
				{
					Rep.State = EWRMachineState::ChainLong;
					bJam = true;
					break;
				}
				if (bIntoMachine && !WRMachineDetail::Takes(*NM, Stack.ItemId)) { bJam = true; break; }
				Num = NM->Role == TEXT("riftchest") ? Next->Storage.Num() : NM->In;
			}
			else if (!ND || ND->Kind != TEXT("chest"))
			{
				bJam = true;
				break;
			}
			TArray<FWRItemStack> Part = Slice(Next->Storage, From, Num);
			FWRItemStack Copy = Stack;
			if (FWRInventoryRules::MoveFrom(Copy, Part, Copy.Count) == Stack.Count)
			{
				Put(Next->Storage, From, Part);
				bGiven = true;
				if (NM && NM->Role == TEXT("riftchest")) SyncRift(Next);
			}
		}
		if (!bGiven) { bJam = true; break; }
		P->Belt.RemoveAt(i);
		if (P->Storage.IsValidIndex(i)) P->Storage.RemoveAt(i);
	}
	if (bJam)
	{
		// The front item waits at the head and the belt stands still behind it.
		if (Rep.State != EWRMachineState::ChainLong) Rep.State = EWRMachineState::OutputFull;
		Rep.BeltT = static_cast<float>(Now);
		Rep.Speed100 = 0;
		return;
	}
	// Move the belt on.
	Rep.BeltD += Dt * Rate * 100.f;
	Rep.BeltT = static_cast<float>(Now);
	// Take the next stack from what its tail touches, when there's room on it.
	S.BeltClock -= Dt * Rate;
	AWRBuildPiece* From = Tails.FindRef(P).Get();
	const bool bRoom = P->Belt.Num() == 0 || Rep.BeltD - P->Belt.Last().Enter >= Spacing;
	if (From && bRoom && S.BeltClock <= 0.f && Rate > 0.f)
	{
		const FWRMachineDef* FM = FWRMachines::DefOf(From);
		int32 First = 0;
		if (FM && FM->IsProcessor()) First = FM->In;   // a machine gives what it made
		for (int32 i = First; i < From->Storage.Num(); ++i)
		{
			FWRItemStack& X = From->Storage[i];
			if (X.IsEmpty()) continue;
			FWRItemStack Taken = X;
			X = FWRItemStack();
			P->Storage.Add(Taken);
			FWRBeltItem& B = P->Belt.AddDefaulted_GetRef();
			B.Item = Taken.ItemId;
			B.Count = Taken.Count;
			B.Enter = Rep.BeltD;
			S.BeltClock = R(TEXT("beltSeconds"), 2.f);
			if (FM && FM->Role == TEXT("riftchest")) SyncRift(From);
			break;
		}
	}
}

void UWRMachineSubsystem::StepSorter(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt)
{
	using namespace WRMachineDetail;
	FWRMachineState& S = P->Mach;
	S.BeltClock -= Dt * Rate;
	if (S.BeltClock > 0.f) return;
	S.BeltClock = R(TEXT("sortSeconds"), 2.f);
	const float Range = R(TEXT("sortRange"), 2000.f);
	for (FWRItemStack& X : P->Storage)
	{
		if (X.IsEmpty()) continue;
		// A chest within range that already holds this item, the nearest first.
		AWRBuildPiece* Best = nullptr;
		float BestD = Range;
		for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
		{
			AWRBuildPiece* C = W.Get();
			const FWRPieceDef* CD = C ? C->GetDef() : nullptr;
			if (!CD || CD->Kind != TEXT("chest") || !C->CanAccess(P->OwnerName)) continue;
			const float Dist = FVector::Dist(C->GetActorLocation(), P->GetActorLocation());
			if (Dist >= BestD || FWRInventoryRules::CountOf(C->Storage, X.ItemId) <= 0) continue;
			TArray<FWRItemStack> Probe = C->Storage;
			FWRItemStack Copy = X;
			if (FWRInventoryRules::MoveFrom(Copy, Probe, 1) > 0) { Best = C; BestD = Dist; }
		}
		if (!Best) continue;
		const FName Item = X.ItemId;
		const int32 Moved = FWRInventoryRules::MoveFrom(X, Best->Storage, X.Count);
		// (Every machine that draws it hears the thunk where the chute points: WRMachineLook.)
		P->Machine.Target = Best->GetActorLocation() + FVector(0.f, 0.f, static_cast<float>(FMath::RandRange(-3, 3)));
		P->Machine.Making = Item;
		UE_LOG(LogWildreach, Verbose, TEXT("MACHINE sorting chest sent %d %s to %s"), Moved, *Item.ToString(), *Best->GetName());
		return;
	}
}

void UWRMachineSubsystem::StepDrill(AWRBuildPiece* P, const FWRMachineDef& D, float Rate, float Dt)
{
	using namespace WRMachineDetail;
	FWRMachineState& S = P->Mach;
	const AWRResourceNode* Vein = Veins.FindRef(P).Get();
	if (!Vein) return;
	S.Progress += Dt * Rate;
	const float Seconds = FMath::Max(1.f, R(TEXT("drillSeconds"), 60.f));
	if (S.Progress >= Seconds)
	{
		S.Progress = 0.f;
		TArray<FWRItemStack> Hopper = Slice(P->Storage, 0, D.Out);
		FWRInventoryRules::Add(Hopper, Vein->ItemId, 1);
		Put(P->Storage, 0, Hopper);
	}
	P->Machine.Progress = static_cast<uint8>(FMath::Clamp(S.Progress / Seconds, 0.f, 1.f) * 255.f);
	P->Machine.Making = Vein->ItemId;
}

void UWRMachineSubsystem::StepUnpowered(AWRBuildPiece* P, const FWRMachineDef& D, float Dt)
{
	using namespace WRMachineDetail;
	FWRMachineState& S = P->Mach;
	FWRMachineRep& Rep = P->Machine;
	Rep.Speed100 = 0;
	Rep.Flags &= ~WRMachineFlag::Working;
	if (!S.bOn) { Rep.State = EWRMachineState::Off; return; }
	const FName Role = D.Role;
	if (Role == TEXT("press"))
	{
		bool bFull = false;
		const FWRRecipe* Rec = PickRecipe(P, D, bFull);
		Rep.State = !Rec ? EWRMachineState::Idle : bFull ? EWRMachineState::OutputFull : EWRMachineState::Running;
		if (Rep.State == EWRMachineState::Running) { StepProcessor(P, D, 1.f, Dt); Rep.Speed100 = 100; Rep.Flags |= WRMachineFlag::Working; }
		return;
	}
	if (Role == TEXT("still"))
	{
		const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(P);
		const float Depth = Water ? Water->DepthAt(P->GetActorLocation()) : 0.f;
		if (Depth < R(TEXT("still.minDepth"), 10.f)) { Rep.State = EWRMachineState::NoWater; return; }
		TArray<FWRItemStack> In = Slice(P->Storage, 0, D.In);
		TArray<FWRItemStack> Out = Slice(P->Storage, D.In, D.Out);
		if (FWRInventoryRules::CountOf(In, D.Input) <= 0) { Rep.State = EWRMachineState::Idle; S.Progress = 0.f; Rep.Progress = 0; return; }
		TArray<FWRItemStack> Probe = Out;
		if (FWRInventoryRules::Add(Probe, D.Output, 1) > 0) { Rep.State = EWRMachineState::OutputFull; return; }
		Rep.State = EWRMachineState::Running;
		Rep.Speed100 = 100;
		Rep.Flags |= WRMachineFlag::Working;
		S.Progress += Dt;
		const float Seconds = R(TEXT("still.seconds"), 300.f);
		if (S.Progress >= Seconds)
		{
			S.Progress = 0.f;
			FWRInventoryRules::Remove(In, D.Input, 1);
			FWRInventoryRules::Add(Out, D.Output, 1);
			Put(P->Storage, 0, In);
			Put(P->Storage, D.In, Out);
		}
		Rep.Progress = static_cast<uint8>(FMath::Clamp(S.Progress / Seconds, 0.f, 1.f) * 255.f);
		Rep.Making = D.Output;
		return;
	}
	if (Role == TEXT("vat"))
	{
		if (!Covered(GetWorld(), P->GetActorLocation(), P)) { Rep.State = EWRMachineState::NeedsDark; return; }
		TArray<FWRItemStack> In = Slice(P->Storage, 0, D.In);
		TArray<FWRItemStack> Out = Slice(P->Storage, D.In, D.Out);
		const int32 Every = FMath::Max(1, FMath::RoundToInt(R(TEXT("vat.compostEvery"), 10.f)));
		if (S.VatCount == 0 && FWRInventoryRules::CountOf(In, D.Input) <= 0) { Rep.State = EWRMachineState::NoFuel; return; }
		const FName Grow = D.Outputs.IsValidIndex(S.bVatLions ? 1 : 0) ? D.Outputs[S.bVatLions ? 1 : 0] : NAME_None;
		TArray<FWRItemStack> Probe = Out;
		if (Grow.IsNone() || FWRInventoryRules::Add(Probe, Grow, 1) > 0) { Rep.State = EWRMachineState::OutputFull; return; }
		Rep.State = EWRMachineState::Running;
		Rep.Flags |= WRMachineFlag::Working;
		S.Progress += Dt;
		const float Seconds = R(TEXT("vat.seconds"), 60.f);
		if (S.Progress >= Seconds)
		{
			S.Progress = 0.f;
			if (S.VatCount == 0) FWRInventoryRules::Remove(In, D.Input, 1);   // a fresh bed of compost
			S.VatCount = (S.VatCount + 1) % Every;
			FWRInventoryRules::Add(Out, Grow, 1);
			S.bVatLions = !S.bVatLions;
			Put(P->Storage, 0, In);
			Put(P->Storage, D.In, Out);
		}
		Rep.Progress = static_cast<uint8>(FMath::Clamp(S.Progress / Seconds, 0.f, 1.f) * 255.f);
		Rep.Making = Grow;
		return;
	}
	if (Role == TEXT("lift"))
	{
		StepLift(P, Dt);
		return;
	}
	if (Role == TEXT("pylon") || Role == TEXT("riftchest"))
	{
		Rep.State = EWRMachineState::Running;
		Rep.Speed100 = 100;
		Rep.Flags |= WRMachineFlag::Working;
		if (Role == TEXT("pylon"))
		{
			if (const int32* Net = NetOf.Find(P); Net && Nets.IsValidIndex(*Net))
			{
				Rep.Supply10 = static_cast<uint16>(FMath::Clamp(Nets[*Net].Supply * 10.f, 0.f, 65535.f));
				Rep.Load10 = static_cast<uint16>(FMath::Clamp(Nets[*Net].Load * 10.f, 0.f, 65535.f));
				Rep.NetSources = static_cast<uint8>(FMath::Min(255, Nets[*Net].Sources));
				Rep.NetMachines = static_cast<uint8>(FMath::Min(255, Nets[*Net].Machines));
			}
		}
	}
}

void UWRMachineSubsystem::StepLift(AWRBuildPiece* P, float Dt)
{
	using namespace WRMachineDetail;
	FWRMachineRep& Rep = P->Machine;
	AWRBuildPiece* Partner = nullptr;
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* Q = W.Get();
		if (Q && Q != P && Q->PieceId == TEXT("updraft_lift") && !P->Mach.LinkTo.IsZero() && FVector::Dist(Q->GetActorLocation(), P->Mach.LinkTo) < 60.f) { Partner = Q; break; }
	}
	if (!Partner)
	{
		Rep.State = EWRMachineState::NoLink;
		Rep.Target = FVector::ZeroVector;
		return;
	}
	Rep.Target = Partner->GetActorLocation();
	const float Now = static_cast<float>(ServerTime(P));
	if (Rep.TripStart > 0.f && Now - Rep.TripStart >= R(TEXT("lift.tripSeconds"), 10.f)) Rep.TripStart = 0.f;
	Rep.State = Rep.TripStart > 0.f ? EWRMachineState::Running : EWRMachineState::Idle;
	Rep.Speed100 = Rep.TripStart > 0.f ? 100 : 0;
}

FString UWRMachineSubsystem::LinkLift(AWRBuildPiece* Piece, AWRPlayerController* Player)
{
	using namespace WRMachineDetail;
	const APlayerState* PS = Player ? Player->GetPlayerState<APlayerState>() : nullptr;
	if (!Piece || !PS || Piece->PieceId != TEXT("updraft_lift")) return TEXT("only lifts link");
	const FString Who = PS->GetPlayerName();
	AWRBuildPiece* Waiting = PendingLink.FindRef(Who).Get();
	if (!Waiting || Waiting == Piece)
	{
		PendingLink.Add(Who, Piece);
		return TEXT("Linking: go to the other lift (within 120 m) and Link it to this one");
	}
	if (FVector::Dist(Waiting->GetActorLocation(), Piece->GetActorLocation()) > R(TEXT("lift.range"), 12000.f))
	{
		return TEXT("too far apart: two lifts link within 120 m");
	}
	// Each one's platform rides between the two; one platform rests at each end.
	Waiting->Mach.LinkTo = Piece->GetActorLocation();
	Piece->Mach.LinkTo = Waiting->GetActorLocation();
	Waiting->Machine.TripTo = 0;
	Piece->Machine.TripTo = 0;
	Waiting->Machine.TripStart = Piece->Machine.TripStart = 0.f;
	PendingLink.Remove(Who);
	return TEXT("Linked: E on either lift sends its platform");
}

FString UWRMachineSubsystem::SendLift(AWRBuildPiece* Piece)
{
	using namespace WRMachineDetail;
	if (!Piece || Piece->PieceId != TEXT("updraft_lift")) return TEXT("only lifts go");
	if (Piece->Mach.LinkTo.IsZero()) return TEXT("Not linked yet");
	// One platform runs between the pair (the one whose lift owns it); either end calls or sends it.
	if (!FWRMachineLook::OwnsPlatform(Piece))
	{
		for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
		{
			AWRBuildPiece* Q = W.Get();
			if (Q && Q != Piece && Q->PieceId == TEXT("updraft_lift") && FVector::Dist(Q->GetActorLocation(), Piece->Mach.LinkTo) < 60.f) { Piece = Q; break; }
		}
		if (!FWRMachineLook::OwnsPlatform(Piece)) return TEXT("its partner lift isn't there");
	}
	FWRMachineRep& Rep = Piece->Machine;
	if (Rep.TripStart > 0.f) return TEXT("It's on its way");
	Rep.TripTo = Rep.TripTo ? 0 : 1;
	Rep.TripStart = static_cast<float>(ServerTime(Piece));
	return Rep.TripTo ? TEXT("Up and away") : TEXT("Coming back");
}

void UWRMachineSubsystem::AwardRun(AWRBuildPiece* P, float Dt)
{
	using namespace WRMachineDetail;
	// Engineering: each machine you own that runs, 1 XP per 5 minutes of running (up to 10 machines counted at once).
	FWRMachineState& S = P->Mach;
	S.RunSeconds += Dt;
	const float Every = 60.f * R(TEXT("runXpMinutes"), 5.f);
	if (S.RunSeconds < Every) return;
	S.RunSeconds -= Every;
	APlayerState* Owner = Online(GetWorld(), P->OwnerName);
	if (!Owner) return;
	const double Window = FMath::FloorToDouble(GetWorld()->GetTimeSeconds() / Every);
	FRunWindow& W = RunWindows.FindOrAdd(P->OwnerName);
	if (W.Window != Window) { W.Window = Window; W.Count = 0; }
	if (W.Count >= FMath::RoundToInt(R(TEXT("runXpMax"), 10.f))) return;
	++W.Count;
	UWRLifeSkillsComponent::Award(Owner, TEXT("engineering"), TEXT("machineRun5m"), 1.f);
}

void UWRMachineSubsystem::Step(float Dt)
{
	using namespace WRMachineDetail;
	const float Wear = R(TEXT("wearPerHour"), 0.02f);
	const float WornAt = R(TEXT("wornAt"), 0.25f);
	const float SpeedFull = R(TEXT("stationSpeed"), 1.5f);
	StationBoosts.Reset();
	for (FNet& N : Nets)
	{
		N.Pieces.RemoveAll([](const TWeakObjectPtr<AWRBuildPiece>& W) { return !W.IsValid(); });
		N.Supply = N.Load = 0.f;
		N.Sources = N.Machines = 0;
		N.bAwake = false;
		for (const TWeakObjectPtr<AWRBuildPiece>& W : N.Pieces) N.bAwake |= IsAwake(W->GetActorLocation());
		if (!N.bAwake)
		{
			for (const TWeakObjectPtr<AWRBuildPiece>& W : N.Pieces)
			{
				FWRMachineRep& Rep = W->Machine;
				Rep.State = EWRMachineState::Asleep;
				Rep.Speed100 = 0;
				Rep.Flags &= ~(WRMachineFlag::Working | WRMachineFlag::Lit);
			}
			continue;
		}
		// Sources, then what the machines ask for.
		TMap<AWRBuildPiece*, float> Given;
		TMap<AWRBuildPiece*, TPair<float, EWRMachineState>> Asked;
		for (const TWeakObjectPtr<AWRBuildPiece>& W : N.Pieces)
		{
			AWRBuildPiece* P = W.Get();
			const FWRMachineDef* M = FWRMachines::DefOf(P);
			RefreshOwnerPerks(P);
			if (M->IsSource())
			{
				float Supply = 0.f, Factor = 1.f;
				EWRMachineState State = EWRMachineState::Running;
				StepSource(P, *M, Dt, Supply, State, Factor);
				N.Supply += Supply;
				++N.Sources;
				Given.Add(P, Supply);
				P->Machine.State = State;
				P->Machine.Own10 = static_cast<uint16>(FMath::Clamp(Supply * 10.f, 0.f, 65535.f));
				P->Machine.Factor100 = static_cast<uint16>(FMath::Clamp(Factor * 100.f, 0.f, 65535.f));
				P->Machine.Speed100 = Supply > 0.f ? static_cast<uint8>(FMath::Clamp(Factor * 100.f, 20.f, 200.f)) : 0;
				P->Machine.Flags = Supply > 0.f ? (P->Machine.Flags | WRMachineFlag::Working | WRMachineFlag::Lit) : (P->Machine.Flags & ~(WRMachineFlag::Working | WRMachineFlag::Lit));
			}
			else if (!M->Carries() || M->Role == TEXT("conveyor"))
			{
				EWRMachineState State = EWRMachineState::Idle;
				const float Load = Demand(P, *M, State);
				Asked.Add(P, { Load, State });
				N.Load += Load;
				++N.Machines;
			}
		}
		for (const TWeakObjectPtr<AWRBuildPiece>& W : N.Stations)
		{
			const AWRBuildPiece* SP = W.Get();
			const AWRStation* St = SP ? Cast<AWRStation>(SP->GetChild()) : nullptr;
			if (St && St->IsWorking()) N.Load += R(TEXT("stationLoad"), 5.f);
			if (St) ++N.Machines;
		}
		N.Speed = N.Load > 0.f ? FMath::Min(1.f, N.Supply / N.Load) : (N.Supply > 0.f ? 1.f : 0.f);

		// Batteries: a storm coil / sun array banks what the line didn't use, and pays out what it gave from the bank.
		for (const TPair<AWRBuildPiece*, float>& G : Given)
		{
			const FWRMachineDef* M = FWRMachines::DefOf(G.Key);
			if (!M || (M->Flow != TEXT("storm") && M->Flow != TEXT("sun"))) continue;
			FWRMachineState& S = G.Key->Mach;
			const bool bStorm = M->Flow == TEXT("storm");
			const float Cap = (bStorm ? R(TEXT("storm.batteryHours"), 2.f) * R(TEXT("storm.batteryP"), 20.f) : R(TEXT("sun.batteryHours"), 4.f) * R(TEXT("sun.batteryP"), 10.f));
			const float Used = N.Supply > 0.f ? G.Value * FMath::Min(1.f, N.Load / N.Supply) : 0.f;
			// While the sky gives power the battery banks what the line doesn't use; after, it pays out what's drawn.
			if (S.LiveP > 0.f) S.Charge = FMath::Min(Cap, S.Charge + FMath::Max(0.f, S.LiveP - Used) * Dt / 3600.f);
			else S.Charge = FMath::Max(0.f, S.Charge - Used * Dt / 3600.f);
			G.Key->Machine.Charge = static_cast<uint8>(FMath::Clamp(S.Charge / FMath::Max(0.01f, Cap), 0.f, 1.f) * 255.f);
		}
		// Soul engines burn while their power is drawn.
		for (const TPair<AWRBuildPiece*, float>& G : Given)
		{
			const FWRMachineDef* M = FWRMachines::DefOf(G.Key);
			if (M && M->Flow == TEXT("soul") && N.Load > 0.f) G.Key->Mach.FuelLeft = FMath::Max(0.f, G.Key->Mach.FuelLeft - Dt);
		}

		// Run the machines at the line's speed.
		for (const TWeakObjectPtr<AWRBuildPiece>& W : N.Pieces)
		{
			AWRBuildPiece* P = W.Get();
			const FWRMachineDef* M = FWRMachines::DefOf(P);
			FWRMachineRep& Rep = P->Machine;
			const FWRMachineState& S = P->Mach;
			Rep.Supply10 = static_cast<uint16>(FMath::Clamp(N.Supply * 10.f, 0.f, 65535.f));
			Rep.Load10 = static_cast<uint16>(FMath::Clamp(N.Load * 10.f, 0.f, 65535.f));
			Rep.NetSources = static_cast<uint8>(FMath::Min(255, N.Sources));
			Rep.NetMachines = static_cast<uint8>(FMath::Min(255, N.Machines));
			// Wear: a running machine loses health slowly (never all of it); worn ones run at half speed.
			const bool bWorn = P->Health <= P->MaxHealth * WornAt;
			Rep.Flags = bWorn ? (Rep.Flags | WRMachineFlag::Worn) : (Rep.Flags & ~WRMachineFlag::Worn);
			if (M->IsSource())
			{
				if (Rep.Own10 > 0)
				{
					if (!S.bOwnerSelfRepair) P->Health = FMath::Max(P->MaxHealth * WornAt * 0.5f, P->Health - P->MaxHealth * Wear * Dt / 3600.f);
					AwardRun(P, Dt);
				}
				continue;
			}
			if (M->Carries() && M->Role != TEXT("conveyor"))
			{
				// Shafts and gearboxes turn at the line's speed (a source with nothing drawing still turns them).
				Rep.Speed100 = static_cast<uint8>(FMath::Clamp(N.Speed * 100.f, 0.f, 200.f));
				Rep.State = N.Supply > 0.f ? EWRMachineState::Running : EWRMachineState::Idle;
				continue;
			}
			const TPair<float, EWRMachineState>* A = Asked.Find(P);
			const float Load = A ? A->Key : 0.f;
			float Rate = N.Speed * (1.f + S.OwnerSpeed) * (S.bOverclock ? R(TEXT("overclockSpeed"), 1.5f) : 1.f) * (bWorn ? R(TEXT("wornSpeed"), 0.5f) : 1.f);
			if (M->Role == TEXT("coldstore") && !M->FreeIn.IsNone() && WRBiome::At(P, P->GetActorLocation()) == M->FreeIn)
			{
				// The tundra's cold does it: no power, no fan.
				Rep.State = S.bOn ? EWRMachineState::Running : EWRMachineState::Off;
				Rep.Flags = S.bOn ? (Rep.Flags | WRMachineFlag::Working | WRMachineFlag::Tundra) : (Rep.Flags & ~WRMachineFlag::Working);
				Rep.Speed100 = 0;
				continue;
			}
			Rep.Flags &= ~WRMachineFlag::Tundra;
			if (Load <= 0.f)
			{
				// Idle: belts and lamps still show why.
				Rep.State = A ? A->Value : EWRMachineState::Idle;
				Rep.Speed100 = 0;
				Rep.Flags &= ~(WRMachineFlag::Working | WRMachineFlag::Lit);
				if (M->Role == TEXT("drill") || M->IsProcessor()) { Rep.Progress = 0; }
				continue;
			}
			if (N.Speed <= 0.f)
			{
				Rep.State = EWRMachineState::NoPower;
				Rep.Speed100 = 0;
				Rep.Flags &= ~(WRMachineFlag::Working | WRMachineFlag::Lit);
				if (M->Role == TEXT("conveyor")) Rep.BeltT = static_cast<float>(ServerTime(P));
				continue;
			}
			Rep.State = bWorn ? EWRMachineState::Worn : N.Speed < 0.999f ? EWRMachineState::Slow : EWRMachineState::Running;
			Rep.Speed100 = static_cast<uint8>(FMath::Clamp(Rate * 100.f, 0.f, 200.f));
			Rep.Own10 = static_cast<uint16>(FMath::Clamp(Load * 10.f, 0.f, 65535.f));
			Rep.Flags |= WRMachineFlag::Working;
			const bool bLight = M->Role == TEXT("lamp") || M->Role == TEXT("growlight");
			Rep.Flags = (bLight ? N.Speed >= 0.5f : true) ? (Rep.Flags | WRMachineFlag::Lit) : (Rep.Flags & ~WRMachineFlag::Lit);
			if (M->IsProcessor()) StepProcessor(P, *M, Rate, Dt);
			else if (M->Role == TEXT("conveyor")) StepConveyor(P, *M, Rate, Dt);
			else if (M->Role == TEXT("sorter")) StepSorter(P, *M, Rate, Dt);
			else if (M->Role == TEXT("drill")) StepDrill(P, *M, Rate, Dt);
			if (!S.bOwnerSelfRepair && M->Role != TEXT("conveyor") && M->Role != TEXT("lamp"))
			{
				P->Health = FMath::Max(P->MaxHealth * WornAt * 0.5f, P->Health - P->MaxHealth * Wear * Dt / 3600.f);
			}
			AwardRun(P, Dt);
		}
		// Stations on the line: half again as fast at full power.
		for (const TWeakObjectPtr<AWRBuildPiece>& W : N.Stations)
		{
			if (W.IsValid()) StationBoosts.Add(W.Get(), 1.f + (SpeedFull - 1.f) * N.Speed);
		}
		N.bWasRunning = N.Supply > 0.f && N.Speed > 0.f;
	}
	// Machines on no line's power: the biome machines that need none, and the lift.
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Pieces)
	{
		AWRBuildPiece* P = W.Get();
		const FWRMachineDef* M = FWRMachines::DefOf(P);
		if (!M || M->IsPowered()) continue;
		if (!IsAwake(P->GetActorLocation()))
		{
			if (M->Role != TEXT("lift")) { P->Machine.State = EWRMachineState::Asleep; P->Machine.Speed100 = 0; }
			continue;
		}
		RefreshOwnerPerks(P);
		StepUnpowered(P, *M, Dt);
	}
}
