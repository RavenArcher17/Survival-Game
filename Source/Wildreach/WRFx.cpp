// The effects runtime: recipes from Data/fx.json, the particle simulation, emitter and light pools, the server's
// effect relay and the WRFx calls gameplay makes. Rendering is in WRFxRender.cpp, combat feel in WRFxCombat.cpp, the
// HUD layer in WRFxHud.cpp and the ambient effects (Hearths, torches, lava, weather) in WRFxAmbient.cpp.

#include "WRFx.h"
#include "WRFxSystem.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"

namespace WRFxDetail
{
	constexpr int32 HandleIndexBits = 12;
	constexpr int32 HandleIndexMask = (1 << HandleIndexBits) - 1;

	/** Atlas cells by name (Tools/fx/make_fx_atlas.mjs paints them in this order). */
	int32 CellFor(const FString& Name)
	{
		static const TCHAR* Names[] = { TEXT("dot"), TEXT("spark"), TEXT("smoke"), TEXT("smoke2"), TEXT("flame"), TEXT("flame2"), TEXT("flare"), TEXT("ring"),
			TEXT("chip_wood"), TEXT("chip_stone"), TEXT("leaf"), TEXT("shard"), TEXT("flake"), TEXT("droplet"), TEXT("glow"), TEXT("wisp") };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Names); ++i)
		{
			if (Name == Names[i]) return i;
		}
		return 0;
	}

	/** A number or a [min, max] pair. */
	FVector2f Range(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FVector2f Default)
	{
		double N = 0.0;
		if (O->TryGetNumberField(Key, N))
		{
			return FVector2f(N, N);
		}
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(Key, Arr) && Arr->Num() >= 2)
		{
			return FVector2f((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());
		}
		return Default;
	}

	FVector3f Vec3(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FVector3f Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(Key, Arr) && Arr->Num() >= 3)
		{
			return FVector3f((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
		}
		return Default;
	}

	FLinearColor Color(const TArray<TSharedPtr<FJsonValue>>& Arr)
	{
		const auto At = [&Arr](int32 i, float D) { return Arr.IsValidIndex(i) ? static_cast<float>(Arr[i]->AsNumber()) : D; };
		return FLinearColor(At(0, 1.f), At(1, 1.f), At(2, 1.f), At(3, 1.f));
	}

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Default)
	{
		double N = Default;
		O->TryGetNumberField(Key, N);
		return static_cast<float>(N);
	}

	bool Flag(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Default)
	{
		bool B = Default;
		O->TryGetBoolField(Key, B);
		return B;
	}

	/** Rounds a fractional count at random (0.3 = one sprite three times in ten). */
	int32 RandomRound(float Count)
	{
		const int32 Whole = FMath::FloorToInt(Count);
		return Whole + (FMath::FRand() < Count - Whole ? 1 : 0);
	}

	FLinearColor Gradient(const TArray<FLinearColor>& Colors, float T)
	{
		if (Colors.Num() == 0) return FLinearColor::White;
		if (Colors.Num() == 1) return Colors[0];
		const float X = FMath::Clamp(T, 0.f, 1.f) * (Colors.Num() - 1);
		const int32 I = FMath::Min(FMath::FloorToInt(X), Colors.Num() - 2);
		return FMath::Lerp(Colors[I], Colors[I + 1], X - I);
	}

	FColor ToColor(const FLinearColor& C)
	{
		// Linear 0..1 straight into the vertex colour (the materials read it as linear); HDR goes through intensity.
		const auto Q = [](float V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(V * 255.f), 0, 255)); };
		return FColor(Q(C.R), Q(C.G), Q(C.B), Q(C.A));
	}

	void Receive(const UObject* Context, FName Effect, const FVector& Location, const FRotator& Rotation, float Scale, float Seconds, FName Sound,
		AActor* Target, FName Socket)
	{
		if (!Sound.IsNone())
		{
			WRSound::PlayAt(Context, Sound, Target ? Target->GetActorLocation() : Location);
		}
		UWRFxSubsystem* Fx = UWRFxSubsystem::Get(Context);
		if (!Fx || !Fx->IsActive() || Effect.IsNone())
		{
			return;
		}
		if (Target)
		{
			// The skeletal mesh that has the socket (a survivor's body, a creature's art), else the actor's root.
			USceneComponent* At = Target->GetRootComponent();
			FName UseSocket = NAME_None;
			TArray<USkeletalMeshComponent*> Meshes;
			Target->GetComponents(Meshes);
			TArray<FName> Try;
			if (Socket == TEXT("hand")) Try = Fx->GetHandSockets();
			else if (!Socket.IsNone()) Try.Add(Socket);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				if (!Mesh->IsVisible() || !Mesh->GetSkeletalMeshAsset()) continue;
				for (const FName& S : Try)
				{
					if (Mesh->DoesSocketExist(S)) { At = Mesh; UseSocket = S; break; }
				}
				if (!UseSocket.IsNone()) break;
			}
			if (At)
			{
				Fx->Start(Effect, At->GetSocketTransform(UseSocket), Scale, Seconds, At, UseSocket);
			}
			return;
		}
		Fx->Start(Effect, FTransform(Rotation, Location), Scale, Seconds);
	}
}

using namespace WRFxDetail;

// ---------------------------------------------------------------- relay and pool actors

AWRFxRelay::AWRFxRelay()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(10.f);
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRFxRelay::MulticastAt_Implementation(FName Effect, FVector_NetQuantize Location, FRotator Rotation, float Scale, float Seconds, FName Sound)
{
	Receive(this, Effect, Location, Rotation, Scale, Seconds, Sound, nullptr, NAME_None);
}

void AWRFxRelay::MulticastOn_Implementation(FName Effect, AActor* Target, FName Socket, float Scale, float Seconds, FName Sound)
{
	if (Target)
	{
		Receive(this, Effect, Target->GetActorLocation(), FRotator::ZeroRotator, Scale, Seconds, Sound, Target, Socket);
	}
}

AWRFxPool::AWRFxPool()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Glow = CreateDefaultSubobject<UWRFxSpriteComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Lit = CreateDefaultSubobject<UWRFxSpriteComponent>(TEXT("Lit"));
	Lit->SetupAttachment(RootComponent);
	Lit->bSortByDistance = true;
	// Glow draws after smoke, so a fire stays bright inside its own smoke.
	Glow->TranslucencySortPriority = 1;
}

// ---------------------------------------------------------------- subsystem: setup

bool UWRFxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWRFxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UWRFxSubsystem::Deinitialize()
{
	for (FHitStop& H : HitStops)
	{
		if (USkeletalMeshComponent* Mesh = H.Mesh.Get()) Mesh->bPauseAnims = false;
	}
	HitStops.Reset();
	Super::Deinitialize();
}

void UWRFxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const ENetMode Mode = InWorld.GetNetMode();
	if (Mode != NM_Client)
	{
		// The server's voice for effects only it sees happen (WRFx::Broadcast).
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Relay = InWorld.SpawnActor<AWRFxRelay>(AWRFxRelay::StaticClass(), FTransform::Identity, Params);
	}
	bActive = Mode != NM_DedicatedServer && !IsRunningDedicatedServer() && FApp::CanEverRender() && !IsRunningCommandlet();
	if (!bActive)
	{
		return;
	}
	LoadDefs();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AWRFxPool* P = InWorld.SpawnActor<AWRFxPool>(AWRFxPool::StaticClass(), FTransform::Identity, Params);
	if (!P)
	{
		bActive = false;
		return;
	}
	Pool = P;
	// Held by the components' UPROPERTY, so they stay loaded.
	P->Glow->Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_FxAdd.M_FxAdd"));
	P->Lit->Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_FxLit.M_FxLit"));
	if (!P->Glow->Material || !P->Lit->Material)
	{
		UE_LOG(LogWildreach, Warning, TEXT("FX materials missing (/Game/FX): run Tools/fx/make_fx_assets.py; effects are off"));
		bActive = false;
		return;
	}
	P->Glow->MarkRenderStateDirty();
	P->Lit->MarkRenderStateDirty();
	const int32 NumLights = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("fx.maxLights"), 12.f)), 0, 32);
	for (int32 i = 0; i < NumLights; ++i)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(P, NAME_None, RF_Transient);
		L->SetupAttachment(P->GetRootComponent());
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->SetMobility(EComponentMobility::Movable);
		L->RegisterComponent();
		P->AddInstanceComponent(L);
		P->Lights.Add(L);
	}
	LightSlots.SetNum(NumLights);
	UE_LOG(LogWildreach, Log, TEXT("FX ready: %d effects, %d lights"), Defs.Num(), NumLights);
}

void UWRFxSubsystem::LoadDefs()
{
	for (TArray<FParticle>& Arr : Particles) Arr.Reset();
	Defs.Reset();
	Layers.Reset();
	Impacts.Reset();
	HandSockets.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("fx.json"));
	const TSharedPtr<FJsonObject>* Effects = nullptr;
	if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("effects"), Effects))
	{
		UE_LOG(LogWildreach, Warning, TEXT("FX: Data/fx.json missing or has no \"effects\""));
		return;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Effects)->Values)
	{
		const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
		if (!E.IsValid() || Pair.Key.StartsWith(TEXT("_")))
		{
			continue;
		}
		FDef Def;
		Def.bLoop = Flag(E, TEXT("loop"), false);
		Def.Cull = Num(E, TEXT("cull"), 0.f);
		Def.Seconds = Num(E, TEXT("seconds"), 2.f);
		FString Shake;
		if (E->TryGetStringField(TEXT("shake"), Shake)) Def.Shake = FName(*Shake);
		const TSharedPtr<FJsonObject>* Light = nullptr;
		if (E->TryGetObjectField(TEXT("light"), Light))
		{
			const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
			if ((*Light)->TryGetArrayField(TEXT("color"), C)) Def.LightColor = Color(*C);
			Def.LightIntensity = Num(*Light, TEXT("intensity"), 0.f);
			Def.LightRadius = Num(*Light, TEXT("radius"), 500.f);
			Def.LightSeconds = Num(*Light, TEXT("seconds"), 0.3f);
			Def.LightFlicker = Num(*Light, TEXT("flicker"), 0.f);
			Def.LightLift = Num(*Light, TEXT("lift"), 0.f);
		}
		const TArray<TSharedPtr<FJsonValue>>* LayerArr = nullptr;
		if (E->TryGetArrayField(TEXT("layers"), LayerArr))
		{
			for (const TSharedPtr<FJsonValue>& V : *LayerArr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				FLayer L;
				FString S;
				if (O->TryGetStringField(TEXT("sprite"), S)) L.Cell = static_cast<uint8>(CellFor(S));
				if (O->TryGetStringField(TEXT("blend"), S)) L.bAdditive = S != TEXT("lit");
				L.Count = Num(O, TEXT("count"), 0.f);
				L.Rate = Num(O, TEXT("rate"), 0.f);
				L.Life = Range(O, TEXT("life"), L.Life);
				L.Size = Range(O, TEXT("size"), L.Size);
				L.Grow = Num(O, TEXT("grow"), 1.f);
				L.Speed = Range(O, TEXT("speed"), L.Speed);
				L.Dir = Vec3(O, TEXT("dir"), L.Dir);
				L.Spread = Num(O, TEXT("spread"), 180.f);
				L.Gravity = Num(O, TEXT("gravity"), 0.f);
				L.Drag = Num(O, TEXT("drag"), 0.f);
				L.Spin = Range(O, TEXT("spin"), L.Spin);
				L.Rot = Range(O, TEXT("rot"), L.Rot);
				L.Stretch = Num(O, TEXT("stretch"), 0.f);
				L.Intensity = Num(O, TEXT("intensity"), L.bAdditive ? 1.f : 0.f);
				L.Radius = Num(O, TEXT("radius"), 0.f);
				L.bDisc = Flag(O, TEXT("disc"), false);
				L.bRing = Flag(O, TEXT("ring"), false);
				L.Box = Vec3(O, TEXT("box"), L.Box);
				L.Line = Num(O, TEXT("line"), 0.f);
				L.LineDelay = Num(O, TEXT("lineDelay"), 0.f);
				L.Delay = Num(O, TEXT("delay"), 0.f);
				L.bFlat = Flag(O, TEXT("flat"), false);
				L.bFollow = Flag(O, TEXT("follow"), false);
				L.bScaleSpeed = Flag(O, TEXT("scaleSpeed"), false);
				L.bScaleSize = Flag(O, TEXT("scaleSize"), true);
				L.CountExp = Num(O, TEXT("countExp"), 0.f);
				L.bRadial = Flag(O, TEXT("radial"), false);
				L.Turbulence = Num(O, TEXT("turbulence"), 0.f);
				L.Bounce = Num(O, TEXT("bounce"), -1.f);
				L.Inherit = Num(O, TEXT("inherit"), 0.f);
				L.Lift = Num(O, TEXT("lift"), 0.f);
				L.SortBias = Num(O, TEXT("sortBias"), 0.f);
				L.Soft = Num(O, TEXT("soft"), L.bFlat ? 3.f : 25.f);
				const TArray<TSharedPtr<FJsonValue>>* Colors = nullptr;
				if (O->TryGetArrayField(TEXT("colors"), Colors))
				{
					for (const TSharedPtr<FJsonValue>& C : *Colors)
					{
						const TArray<TSharedPtr<FJsonValue>>* CA = nullptr;
						if (C->TryGetArray(CA)) L.Colors.Add(Color(*CA));
					}
				}
				if (L.bFollow || L.Count > 0.f)
				{
					Def.Linger = FMath::Max(Def.Linger, L.bFollow ? L.Life.Y + L.Delay : 0.f);
				}
				Def.Layers.Add(Layers.Add(L));
			}
		}
		if (Def.LightIntensity > 0.f && !Def.bLoop)
		{
			Def.Linger = FMath::Max(Def.Linger, Def.LightSeconds);
		}
		Defs.Add(FName(*Pair.Key), MoveTemp(Def));
	}
	const TSharedPtr<FJsonObject>* ImpactObj = nullptr;
	if (Root->TryGetObjectField(TEXT("impacts"), ImpactObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ImpactObj)->Values)
		{
			if (!Pair.Key.StartsWith(TEXT("_"))) Impacts.Add(FName(*Pair.Key), FName(*Pair.Value->AsString()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Hands = nullptr;
	if (Root->TryGetArrayField(TEXT("handSockets"), Hands))
	{
		for (const TSharedPtr<FJsonValue>& H : *Hands) HandSockets.Add(FName(*H->AsString()));
	}
	// Emitters point at recipes by address: find them again (or end the ones whose recipe went away).
	for (int32 i = 0; i < Emitters.Num(); ++i)
	{
		if (!Emitters[i].bLive) continue;
		Emitters[i].Def = FindDef(Emitters[i].Name);
		if (!Emitters[i].Def) ReleaseEmitter(i);
		else Emitters[i].Acc.Init(0.f, Emitters[i].Def->Layers.Num());
	}
	Warned.Reset();
}

UWRFxSubsystem* UWRFxSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWRFxSubsystem>() : nullptr;
}

TStatId UWRFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRFxSubsystem, STATGROUP_Tickables);
}

const WRFxDetail::FDef* UWRFxSubsystem::FindDef(FName Effect) const
{
	if (const FDef* Def = Defs.Find(Effect))
	{
		return Def;
	}
	// A variant falls back to its base: "build.break.wood" -> "build.break".
	const FString S = Effect.ToString();
	int32 Dot = INDEX_NONE;
	if (S.FindLastChar(TEXT('.'), Dot) && Dot > 0)
	{
		if (const FDef* Base = Defs.Find(FName(*S.Left(Dot))))
		{
			return Base;
		}
	}
	return nullptr;
}

FName UWRFxSubsystem::ImpactFor(const AActor* Target) const
{
	static const FName CreatureIdProp(TEXT("CreatureId"));
	if (Target)
	{
		// Creatures by id (read through reflection so effects don't need the creature header).
		if (const FNameProperty* Prop = FindFProperty<FNameProperty>(Target->GetClass(), CreatureIdProp))
		{
			const FName Id = Prop->GetPropertyValue_InContainer(Target);
			if (const FName* Found = Impacts.Find(Id)) return *Found;
			if (const FName* Default = Impacts.Find(TEXT("creature"))) return *Default;
			return TEXT("hit.blood");
		}
	}
	if (const FName* Survivor = Impacts.Find(TEXT("survivor"))) return *Survivor;
	return TEXT("hit.blood");
}

// ---------------------------------------------------------------- emitters

int32 UWRFxSubsystem::Resolve(int32 Handle) const
{
	if (Handle == INDEX_NONE) return INDEX_NONE;
	const int32 Index = Handle & HandleIndexMask;
	const uint32 Serial = static_cast<uint32>(Handle) >> HandleIndexBits;
	return Emitters.IsValidIndex(Index) && Emitters[Index].bLive && (Emitters[Index].Serial & 0xFFFFF) == Serial ? Index : INDEX_NONE;
}

int32 UWRFxSubsystem::Start(FName Effect, const FTransform& Xf, float Scale, float Seconds, USceneComponent* Anchor, FName Socket, FColor Tint)
{
	if (!bActive || Effect.IsNone())
	{
		return INDEX_NONE;
	}
	const FDef* Def = FindDef(Effect);
	if (!Def)
	{
		if (!Warned.Contains(Effect))
		{
			Warned.Add(Effect);
			UE_LOG(LogWildreach, Log, TEXT("FX unknown effect %s (not in Data/fx.json)"), *Effect.ToString());
		}
		return INDEX_NONE;
	}
	FTransform At = Anchor ? Anchor->GetSocketTransform(Socket) : Xf;
	At.SetScale3D(FVector::OneVector);
	// Far-away one-shots aren't worth simulating; loops may come into view, so they're kept (and pause spawning).
	const float Cull = Def->Cull > 0.f ? Def->Cull : WRTune(TEXT("fx.cullDistance"), 15000.f);
	if (!Anchor && !Def->bLoop && FVector::DistSquared(At.GetLocation(), ViewLocation) > FMath::Square(Cull))
	{
		return INDEX_NONE;
	}
	if (Emitters.Num() - FreeEmitters.Num() >= HandleIndexMask)
	{
		return INDEX_NONE;
	}
	const int32 Index = FreeEmitters.Num() > 0 ? FreeEmitters.Pop(EAllowShrinking::No) : Emitters.AddDefaulted();
	FEmitter& E = Emitters[Index];
	E = FEmitter();
	E.Name = Effect;
	E.Def = Def;
	E.Anchor = Anchor;
	E.bAnchored = Anchor != nullptr;
	E.Socket = Socket;
	E.Xf = At;
	E.Scale = FMath::Max(0.01f, Scale);
	E.Seconds = Def->bLoop ? (Seconds > 0.f ? Seconds : (Anchor || Seconds < 0.f ? 0.f : Def->Seconds)) : 0.f;
	E.Tint = Tint;
	E.Serial = NextSerial++;
	E.bLive = true;
	E.Acc.Init(0.f, Def->Layers.Num());
	++StartCounts.FindOrAdd(Effect);

	// Debris lands on the ground under the effect: one trace for the emitter, shared by its sprites.
	bool bBounce = false;
	for (const int32 L : Def->Layers) bBounce |= Layers[L].Bounce >= 0.f;
	if (bBounce)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(WRFxFloor), false);
		const FVector From = At.GetLocation() + FVector(0.f, 0.f, 40.f);
		E.FloorZ = GetWorld()->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 800.f), ECC_WorldStatic, Query) ? Hit.ImpactPoint.Z + 2.f : At.GetLocation().Z - 100.f;
	}
	for (const int32 L : Def->Layers)
	{
		const FLayer& Layer = Layers[L];
		if (Layer.Count > 0.f)
		{
			SpawnLayer(E, Index, L, RandomRound(Layer.Count * FMath::Pow(E.Scale, Layer.CountExp)));
		}
	}
	return Index | static_cast<int32>((E.Serial & 0xFFFFF) << HandleIndexBits);
}

void UWRFxSubsystem::SpawnLayer(FEmitter& E, int32 EmitterIndex, int32 LayerIndex, int32 Count, const FVector& Smear)
{
	const FLayer& L = Layers[LayerIndex];
	TArray<FParticle>& Arr = Particles[L.bAdditive ? 0 : 1];
	const int32 Max = FMath::RoundToInt(WRTune(TEXT("fx.maxParticles"), 8000.f));
	const FQuat Q = E.Xf.GetRotation();
	const FVector Origin = E.Xf.GetLocation();
	const float S = E.Scale;
	for (int32 i = 0; i < Count && Arr.Num() < Max; ++i)
	{
		FVector Off(0.f, 0.f, L.Lift * S);
		if (L.Radius > 0.f)
		{
			const float R = L.Radius * S;
			const float A = FMath::FRand() * UE_TWO_PI;
			if (L.bRing) Off += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R;
			else if (L.bDisc) Off += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R * FMath::Sqrt(FMath::FRand());
			else Off += FMath::VRand() * R * FMath::Pow(FMath::FRand(), 1.f / 3.f);
		}
		if (!L.Box.IsZero())
		{
			Off += FVector(FMath::FRandRange(-L.Box.X, L.Box.X), FMath::FRandRange(-L.Box.Y, L.Box.Y), FMath::FRandRange(-L.Box.Z, L.Box.Z)) * S;
		}
		float LineT = 0.f;
		if (L.Line > 0.f)
		{
			LineT = FMath::FRand();
			Off.X += LineT * L.Line * S;
		}
		FVector Dir = FVector(L.Dir);
		if (L.bRadial)
		{
			FVector2D Flat(Off.X, Off.Y);
			if (!Flat.Normalize()) Flat = FVector2D(FMath::Cos(FMath::FRand() * UE_TWO_PI), FMath::Sin(FMath::FRand() * UE_TWO_PI));
			Dir = FVector(Flat.X, Flat.Y, L.Dir.Z);
		}
		FVector WorldDir = Q.RotateVector(Dir.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
		if (L.Spread > 0.f)
		{
			WorldDir = FMath::VRandCone(WorldDir, FMath::DegreesToRadians(FMath::Min(L.Spread, 180.f)));
		}
		const float Speed = FMath::FRandRange(L.Speed.X, L.Speed.Y) * (L.bScaleSpeed ? S : 1.f);

		FParticle P;
		P.Vel = FVector3f(WorldDir * Speed + E.Velocity * L.Inherit);
		P.Pos = L.bFollow ? Q.RotateVector(Off) : Origin + Q.RotateVector(Off) - Smear * ((i + FMath::FRand()) / Count);
		P.Life = FMath::Max(0.02f, FMath::FRandRange(L.Life.X, L.Life.Y));
		P.Age = -FMath::FRand() * L.Delay - LineT * L.Line * S * 0.01f * L.LineDelay;
		const float Size = FMath::FRandRange(L.Size.X, L.Size.Y) * (L.bScaleSize ? S : 1.f);
		P.Size0 = Size;
		P.Size1 = Size * L.Grow;
		P.Rot = FMath::DegreesToRadians(FMath::FRandRange(L.Rot.X, L.Rot.Y));
		P.Spin = FMath::DegreesToRadians(FMath::FRandRange(L.Spin.X, L.Spin.Y));
		P.FloorZ = E.FloorZ;
		P.Normal = L.bFlat ? FVector3f(Q.GetUpVector()) : FVector3f::ZeroVector;
		P.Layer = LayerIndex;
		P.Emitter = L.bFollow ? EmitterIndex : INDEX_NONE;
		P.Tint = E.Tint;
		Arr.Add(P);
	}
}

void UWRFxSubsystem::StopEmitter(int32 Handle)
{
	const int32 Index = Resolve(Handle);
	if (Index != INDEX_NONE && !Emitters[Index].bStopped)
	{
		Emitters[Index].bStopped = true;
		Emitters[Index].StopAge = Emitters[Index].Age;
	}
}

int32 UWRFxSubsystem::FindByAnchor(const USceneComponent* Anchor) const
{
	for (int32 i = 0; i < Emitters.Num(); ++i)
	{
		if (Emitters[i].bLive && Emitters[i].bAnchored && Emitters[i].Anchor.Get() == Anchor)
		{
			return i | static_cast<int32>((Emitters[i].Serial & 0xFFFFF) << HandleIndexBits);
		}
	}
	return INDEX_NONE;
}

void UWRFxSubsystem::SetRateScale(int32 Handle, float RateScale)
{
	const int32 Index = Resolve(Handle);
	if (Index != INDEX_NONE) Emitters[Index].RateScale = RateScale;
}

void UWRFxSubsystem::MoveEmitter(int32 Handle, const FTransform& Xf)
{
	const int32 Index = Resolve(Handle);
	if (Index != INDEX_NONE && !Emitters[Index].bAnchored) Emitters[Index].Xf = Xf;
}

void UWRFxSubsystem::ReleaseEmitter(int32 Index)
{
	FEmitter& E = Emitters[Index];
	if (!E.bLive) return;
	// Sprites that rode along stay where they are and fade out quickly instead of jumping to the next user.
	const FVector At = E.Xf.GetLocation();
	const bool bHasFollowers = E.Def && E.Def->Layers.ContainsByPredicate([this](int32 L) { return Layers[L].bFollow; });
	for (TArray<FParticle>& Arr : Particles)
	{
		if (!bHasFollowers) break;
		for (FParticle& P : Arr)
		{
			if (P.Emitter == Index)
			{
				P.Emitter = INDEX_NONE;
				P.Pos += At;
				P.Life = FMath::Min(P.Life, FMath::Max(P.Age, 0.f) + 0.12f);
			}
		}
	}
	for (FLightSlot& Slot : LightSlots)
	{
		if (Slot.Emitter == Index) Slot = FLightSlot();
	}
	E.bLive = false;
	E.Anchor.Reset();
	FreeEmitters.Add(Index);
}

void UWRFxSubsystem::UpdateEmitters(float DeltaTime)
{
	const float Cull = WRTune(TEXT("fx.cullDistance"), 15000.f);
	for (int32 i = 0; i < Emitters.Num(); ++i)
	{
		FEmitter& E = Emitters[i];
		if (!E.bLive) continue;
		if (E.bAnchored)
		{
			const USceneComponent* A = E.Anchor.Get();
			if (!A || !A->IsRegistered())
			{
				if (!E.bStopped) { E.bStopped = true; E.StopAge = E.Age; }
			}
			else
			{
				FTransform Now = A->GetSocketTransform(E.Socket);
				Now.SetScale3D(FVector::OneVector);
				E.Velocity = DeltaTime > 0.f ? (Now.GetLocation() - E.Xf.GetLocation()) / DeltaTime : FVector::ZeroVector;
				E.Xf = Now;
			}
		}
		E.Age += DeltaTime;
		const FDef& Def = *E.Def;
		if (Def.bLoop && !E.bStopped)
		{
			if (E.Seconds > 0.f && E.Age >= E.Seconds)
			{
				E.bStopped = true;
				E.StopAge = E.Age;
			}
			else if (E.RateScale > 0.f && FVector::DistSquared(E.Xf.GetLocation(), ViewLocation) < FMath::Square(Def.Cull > 0.f ? Def.Cull : Cull))
			{
				for (int32 k = 0; k < Def.Layers.Num(); ++k)
				{
					const FLayer& L = Layers[Def.Layers[k]];
					if (L.Rate <= 0.f) continue;
					E.Acc[k] += L.Rate * E.RateScale * FMath::Pow(E.Scale, L.CountExp) * DeltaTime;
					const int32 N = FMath::FloorToInt(E.Acc[k]);
					if (N > 0)
					{
						E.Acc[k] -= N;
						SpawnLayer(E, i, Def.Layers[k], N, E.Velocity * DeltaTime);
					}
				}
			}
		}
		const bool bDone = Def.bLoop
			? E.bStopped && E.Age - E.StopAge >= (Def.LightIntensity > 0.f ? 0.35f : 0.f)
			: E.Age >= Def.Linger;
		if (bDone)
		{
			ReleaseEmitter(i);
		}
	}
}

// ---------------------------------------------------------------- simulation

void UWRFxSubsystem::Simulate(float DeltaTime)
{
	for (TArray<FParticle>& Arr : Particles)
	{
		for (int32 i = Arr.Num() - 1; i >= 0; --i)
		{
			FParticle& P = Arr[i];
			P.Age += DeltaTime;
			if (P.Age >= P.Life)
			{
				Arr.RemoveAtSwap(i, 1, EAllowShrinking::No);
				continue;
			}
			if (P.Age < 0.f)
			{
				continue;
			}
			const FLayer& L = Layers[P.Layer];
			if (L.Gravity != 0.f) P.Vel.Z -= 980.f * L.Gravity * DeltaTime;
			if (L.Drag > 0.f) P.Vel *= 1.f / (1.f + L.Drag * DeltaTime);
			if (L.Turbulence > 0.f) P.Vel += FVector3f(FMath::VRand()) * (L.Turbulence * DeltaTime);
			P.Pos += FVector(P.Vel) * DeltaTime;
			P.Rot += P.Spin * DeltaTime;
			if (L.Bounce >= 0.f && P.Pos.Z < P.FloorZ)
			{
				P.Pos.Z = P.FloorZ;
				if (P.Vel.Z < 0.f)
				{
					P.Vel.Z = -P.Vel.Z * L.Bounce;
					P.Vel.X *= 0.45f;
					P.Vel.Y *= 0.45f;
					P.Spin *= 0.4f;
					if (P.Vel.Z < 40.f)
					{
						// Settled: lies where it fell until it fades.
						P.Vel = FVector3f::ZeroVector;
						P.Spin = 0.f;
					}
				}
			}
		}
	}
}

void UWRFxSubsystem::BuildSprites()
{
	AWRFxPool* P = Pool.Get();
	if (!P)
	{
		return;
	}
	const float MaxDraw = WRTune(TEXT("fx.drawDistance"), 25000.f);
	const double MaxDraw2 = FMath::Square(static_cast<double>(MaxDraw));
	UWRFxSpriteComponent* Batches[2] = { P->Glow.Get(), P->Lit.Get() };
	for (int32 b = 0; b < 2; ++b)
	{
		UWRFxSpriteComponent* Comp = Batches[b];
		if (!Comp) continue;
		Comp->Sprites.Reset(Particles[b].Num());
		for (const FParticle& Pt : Particles[b])
		{
			if (Pt.Age < 0.f) continue;
			const FLayer& L = Layers[Pt.Layer];
			const FEmitter* E = Pt.Emitter != INDEX_NONE ? &Emitters[Pt.Emitter] : nullptr;
			const FVector Pos = E ? E->Xf.GetLocation() + Pt.Pos : Pt.Pos;
			if (FVector::DistSquared(Pos, ViewLocation) > MaxDraw2) continue;
			const float T = Pt.Age / Pt.Life;
			FLinearColor C = Gradient(L.Colors, T);
			if (Pt.Tint != FColor::White)
			{
				const FLinearColor Tint(Pt.Tint.R / 255.f, Pt.Tint.G / 255.f, Pt.Tint.B / 255.f, Pt.Tint.A / 255.f);
				C *= Tint;
			}
			if (C.A <= 0.004f) continue;
			FWRFxSprite& S = Comp->Sprites.AddDefaulted_GetRef();
			S.Pos = Pos;
			const float Ease = 1.f - FMath::Square(1.f - T);  // grows fast, then settles
			S.Size = FMath::Lerp(Pt.Size0, Pt.Size1, Ease);
			S.Rot = Pt.Rot;
			S.Normal = Pt.Normal;
			S.Intensity = L.Intensity;
			S.Color = ToColor(C);
			S.Cell = L.Cell;
			S.SortBias = L.SortBias;
			S.Soft = L.Soft;
			if (L.Stretch > 0.f)
			{
				S.Stretch = (E ? FVector3f(E->Velocity) : Pt.Vel) * L.Stretch;
			}
		}
		Comp->MarkRenderDynamicDataDirty();
	}
}

// ---------------------------------------------------------------- lights

void UWRFxSubsystem::UpdateLights(float DeltaTime)
{
	AWRFxPool* P = Pool.Get();
	if (!P || P->Lights.Num() == 0)
	{
		return;
	}
	struct FWant { int32 Emitter; float Intensity; float Priority; };
	TArray<FWant, TInlineAllocator<64>> Want;
	const float Range = WRTune(TEXT("fx.lightDistance"), 6000.f);
	for (int32 i = 0; i < Emitters.Num(); ++i)
	{
		const FEmitter& E = Emitters[i];
		if (!E.bLive || E.Def->LightIntensity <= 0.f) continue;
		const FDef& D = *E.Def;
		float K = 1.f;
		if (D.bLoop)
		{
			K = FMath::Min(1.f, E.Age / 0.12f) * (E.bStopped ? FMath::Max(0.f, 1.f - (E.Age - E.StopAge) / 0.3f) : 1.f);
			if (D.LightFlicker > 0.f)
			{
				const float Seed = static_cast<float>(E.Serial % 97);
				K *= 1.f + D.LightFlicker * (0.6f * FMath::PerlinNoise1D(Clock * 7.f + Seed) + 0.4f * FMath::PerlinNoise1D(Clock * 17.f + Seed * 3.f));
			}
		}
		else
		{
			K = FMath::Square(FMath::Max(0.f, 1.f - E.Age / FMath::Max(0.01f, D.LightSeconds)));
		}
		if (K <= 0.01f) continue;
		const float Dist = FVector::Dist(E.Xf.GetLocation(), ViewLocation);
		if (Dist > Range + D.LightRadius) continue;
		const float I = D.LightIntensity * K * FMath::Sqrt(E.Scale);
		Want.Add({ i, I, I * D.LightRadius / (Dist + 300.f) });
	}
	Want.Sort([](const FWant& A, const FWant& B) { return A.Priority > B.Priority; });
	for (int32 l = 0; l < P->Lights.Num(); ++l)
	{
		UPointLightComponent* Light = P->Lights[l];
		if (!Light) continue;
		if (!Want.IsValidIndex(l))
		{
			if (Light->IsVisible()) Light->SetVisibility(false);
			LightSlots[l] = FLightSlot();
			continue;
		}
		const FEmitter& E = Emitters[Want[l].Emitter];
		const FDef& D = *E.Def;
		Light->SetWorldLocation(E.Xf.GetLocation() + FVector(0.f, 0.f, D.LightLift * E.Scale));
		Light->SetLightColor(D.LightColor);
		Light->SetIntensity(Want[l].Intensity);
		const float Radius = D.LightRadius * FMath::Sqrt(E.Scale);
		if (!FMath::IsNearlyEqual(Light->AttenuationRadius, Radius, 1.f)) Light->SetAttenuationRadius(Radius);
		if (!Light->IsVisible()) Light->SetVisibility(true);
		LightSlots[l].Emitter = Want[l].Emitter;
		LightSlots[l].Serial = E.Serial;
		LightSlots[l].Priority = Want[l].Priority;
	}
}

// ---------------------------------------------------------------- tick

FVector UWRFxSubsystem::CameraLocation() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		return PC->PlayerCameraManager->GetCameraLocation();
	}
	if (PC && PC->GetPawn())
	{
		return PC->GetPawn()->GetActorLocation();
	}
	return ViewLocation;
}

void UWRFxSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bActive)
	{
		return;
	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_WRFx_Tick);
	const double TickStart = FPlatformTime::Seconds();
	DeltaTime = FMath::Min(DeltaTime, 0.1f);  // a hitch shouldn't fling sprites through the floor
	Clock += DeltaTime;
	ViewLocation = CameraLocation();
	TickHitStops(DeltaTime);
	UpdateEmitters(DeltaTime);
	TickAmbient(DeltaTime);
	Simulate(DeltaTime);
	UpdateLights(DeltaTime);
	BuildSprites();
	// Old hit numbers go.
	const double Now = GetWorld()->GetTimeSeconds();
	HudHits.RemoveAll([Now](const FHudHit& H) { return Now - H.Time > 2.0; });
	TickSeconds += FPlatformTime::Seconds() - TickStart;
	++TickFrames;
}

std::atomic<int64> UWRFxSubsystem::RenderMicros{ 0 };
std::atomic<int32> UWRFxSubsystem::RenderFrames{ 0 };

int32 UWRFxSubsystem::NumParticles() const
{
	return Particles[0].Num() + Particles[1].Num();
}

int32 UWRFxSubsystem::NumEmitters() const
{
	return Emitters.Num() - FreeEmitters.Num();
}

void UWRFxSubsystem::ShakeNamed(FName Group, const FVector& At, float Scale)
{
	if (Group.IsNone()) return;
	const FString Key = FString::Printf(TEXT("fx.shake.%s"), *Group.ToString());
	const FWRTuning& T = FWRTuning::Get();
	WRFx::Shake(this, T.Num(Key + TEXT(".strength"), 0.f) * Scale, At, T.Num(Key + TEXT(".radius"), 0.f));
}

// ---------------------------------------------------------------- WRFx calls

void WRFx::SpawnAt(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation, float Scale)
{
	UWRFxSubsystem* Fx = UWRFxSubsystem::Get(WorldContext);
	if (Fx && Fx->IsActive())
	{
		Fx->Start(Effect, FTransform(Rotation, Location), Scale, 0.f);
	}
}

void WRFx::SpawnFor(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation, float Scale, float Seconds)
{
	UWRFxSubsystem* Fx = UWRFxSubsystem::Get(WorldContext);
	if (Fx && Fx->IsActive())
	{
		Fx->Start(Effect, FTransform(Rotation, Location), Scale, Seconds);
	}
}

USceneComponent* WRFx::Attach(USceneComponent* Parent, FName Effect, FName Socket, float Scale)
{
	UWRFxSubsystem* Fx = Parent ? UWRFxSubsystem::Get(Parent) : nullptr;
	if (!Fx || !Fx->IsActive() || !Fx->HasEffect(Effect))
	{
		return nullptr;
	}
	// An anchor the effect follows: the caller keeps it, and Stop (or the parent going away) ends the effect.
	UObject* Outer = Parent->GetOwner() ? static_cast<UObject*>(Parent->GetOwner()) : static_cast<UObject*>(Parent);
	USceneComponent* Anchor = NewObject<USceneComponent>(Outer, NAME_None, RF_Transient);
	Anchor->SetupAttachment(Parent, Socket);
	if (Parent->GetOwner()) Anchor->RegisterComponent();
	else Anchor->RegisterComponentWithWorld(Parent->GetWorld());
	if (Fx->Start(Effect, Anchor->GetComponentTransform(), Scale, 0.f, Anchor) == INDEX_NONE)
	{
		Anchor->DestroyComponent();
		return nullptr;
	}
	return Anchor;
}

void WRFx::Stop(USceneComponent* Fx)
{
	if (!Fx)
	{
		return;
	}
	if (UWRFxSubsystem* Sys = UWRFxSubsystem::Get(Fx))
	{
		Sys->StopEmitter(Sys->FindByAnchor(Fx));
	}
	Fx->DestroyComponent();
}

void WRFx::Broadcast(const UObject* WorldContext, FName Effect, const FVector& Location, const FRotator& Rotation, float Scale, FName Sound, float Seconds)
{
	const UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;  // clients show their own effects with SpawnAt; only the server speaks for everyone
	}
	const UWRFxSubsystem* Fx = World->GetSubsystem<UWRFxSubsystem>();
	if (AWRFxRelay* Relay = Fx ? Fx->GetRelay() : nullptr)
	{
		Relay->MulticastAt(Effect, Location, Rotation, Scale, Seconds, Sound);
	}
	else
	{
		WRFxDetail::Receive(WorldContext, Effect, Location, Rotation, Scale, Seconds, Sound, nullptr, NAME_None);
	}
}

void WRFx::BroadcastOn(AActor* Target, FName Effect, FName Socket, float Scale, FName Sound, float Seconds)
{
	const UWorld* World = Target ? Target->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	const UWRFxSubsystem* Fx = World->GetSubsystem<UWRFxSubsystem>();
	if (AWRFxRelay* Relay = Fx ? Fx->GetRelay() : nullptr)
	{
		Relay->MulticastOn(Effect, Target, Socket, Scale, Seconds, Sound);
	}
	else
	{
		WRFxDetail::Receive(Target, Effect, Target->GetActorLocation(), FRotator::ZeroRotator, Scale, Seconds, Sound, Target, Socket);
	}
}

// ---------------------------------------------------------------- console (testing and tuning)

namespace WRFxDetail
{
	static FAutoConsoleCommandWithWorld ReloadCmd(TEXT("WR.FxReload"), TEXT("Re-read Data/fx.json"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UWRFxSubsystem* Fx = World ? World->GetSubsystem<UWRFxSubsystem>() : nullptr) Fx->LoadDefs();
		}));

	static FAutoConsoleCommandWithWorld StatsCmd(TEXT("WR.FxStats"), TEXT("Log how many effect sprites and emitters are live"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UWRFxSubsystem* Fx = World ? World->GetSubsystem<UWRFxSubsystem>() : nullptr)
			{
				UWRFxSubsystem::RenderFrames.exchange(0);
				const int32 RF = FMath::Max(1, Fx->TickFrames);
				const double RenderMs = UWRFxSubsystem::RenderMicros.exchange(0) / 1000.0 / RF;
				UE_LOG(LogWildreach, Log, TEXT("FX stats: %d sprites, %d emitters, game thread %.3f ms/frame, render thread %.3f ms/frame (over %d frames)"),
					Fx->NumParticles(), Fx->NumEmitters(), Fx->TickSeconds * 1000.0 / FMath::Max(1, Fx->TickFrames), RenderMs, Fx->TickFrames);
				Fx->TickSeconds = 0.0;
				Fx->TickFrames = 0;
				FString Started;
				Fx->StartCounts.ValueSort([](int32 A, int32 B) { return A > B; });
				for (const TPair<FName, int32>& C : Fx->StartCounts) Started += FString::Printf(TEXT("%s %d, "), *C.Key.ToString(), C.Value);
				UE_LOG(LogWildreach, Log, TEXT("FX started: %s"), *Started);
				Fx->StartCounts.Reset();
			}
		}));

	// WR.FxPress <light|heavy|block|dodge> [hold seconds]: press a combat input like a player (capture scripts).
	static FAutoConsoleCommandWithWorldAndArgs PressCmd(TEXT("WR.FxPress"), TEXT("WR.FxPress <light|heavy|block|dodge> [hold seconds]: press a combat input (testing)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
			if (!Me || Args.Num() < 1) return;
			const FString K = Args[0].ToLower();
			const EWRAbilityInput Input = K == TEXT("heavy") ? EWRAbilityInput::Heavy : K == TEXT("block") ? EWRAbilityInput::Block : K == TEXT("dodge") ? EWRAbilityInput::Dodge : EWRAbilityInput::Light;
			const float Hold = Args.Num() > 1 ? FMath::Max(0.02f, FCString::Atof(*Args[1])) : 0.05f;
			Me->PressAbilityInput(Input);
			TWeakObjectPtr<AWRCharacter> Weak = Me;
			FTimerHandle Timer;
			World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Weak, Input]() { if (Weak.IsValid()) Weak->ReleaseAbilityInput(Input); }), Hold, false);
		}));

	// WR.FxHit <parry|block|hit|heavy|crit|weak|shatter> [damage]: show a hit between you and the nearest creature on this
	// machine only, exactly as the server's MulticastHit would (for capturing the presentation of rare moments).
	static FAutoConsoleCommandWithWorldAndArgs HitCmd(TEXT("WR.FxHit"), TEXT("WR.FxHit <parry|block|hit|heavy|crit|weak|shatter> [damage]: preview a hit's presentation locally (testing)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Me = PC ? PC->GetPawn() : nullptr;
			if (!Me || Args.Num() < 1) return;
			AWRCharacterBase* Near = nullptr;
			float Best = 3000.f;
			for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
				if (*It != Me && It->IsCreature() && D < Best) { Best = D; Near = *It; }
			}
			if (!Near) return;
			const FString K = Args[0].ToLower();
			const float Damage = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 24.f;
			if (K == TEXT("parry"))
			{
				WRFx::ShowHit(Me, Near, 0.f, EWRHitResult::Parried, WRFx::Hit_Melee, Near->GetActorLocation());
				return;
			}
			uint16 Flags = WRFx::Hit_Melee;
			if (K == TEXT("heavy")) Flags |= WRFx::Hit_Heavy;
			if (K == TEXT("crit")) Flags |= WRFx::Hit_Crit | WRFx::Hit_Heavy;
			if (K == TEXT("weak")) Flags |= WRFx::Hit_Weak | WRFx::Hit_Frost;
			if (K == TEXT("shatter")) Flags |= WRFx::Hit_Shatter | WRFx::Hit_Heavy | WRFx::Hit_Frost;
			WRFx::ShowHit(Near, Me, Damage, K == TEXT("block") ? EWRHitResult::Blocked : EWRHitResult::Hit, Flags, Me->GetActorLocation());
		}));

	// WR.FxStress: a heavy load of effects around you at once (fire walls, lava pools, impacts, slams), for stat unit.
	static FAutoConsoleCommandWithWorld StressCmd(TEXT("WR.FxStress"), TEXT("Spawn many effects around you (performance check)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UWRFxSubsystem* Fx = World ? World->GetSubsystem<UWRFxSubsystem>() : nullptr;
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!Fx || !PC || !PC->GetPawn()) return;
			const FVector C = PC->GetPawn()->GetActorLocation() - FVector(0.f, 0.f, PC->GetPawn()->GetSimpleCollisionHalfHeight());
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector Ring = C + FRotator(0.f, i * 45.f, 0.f).Vector() * 900.f;
				Fx->Start(i % 2 ? TEXT("firewall") : TEXT("lavapool"), FTransform(Ring), 1.5f, 8.f);
			}
			for (int32 i = 0; i < 40; ++i)
			{
				const FVector At = C + FVector(FMath::FRandRange(-1200.f, 1200.f), FMath::FRandRange(-1200.f, 1200.f), 80.f);
				static const TCHAR* Names[] = { TEXT("fire.impact"), TEXT("frost.impact"), TEXT("hit.blood"), TEXT("hit.spark"), TEXT("gather.wood"), TEXT("stomp"), TEXT("frostnova"), TEXT("build.break.wood") };
				Fx->Start(Names[i % UE_ARRAY_COUNT(Names)], FTransform(FRotator(90.f, 0.f, 0.f), At), 1.f, 0.f);
			}
			UE_LOG(LogWildreach, Log, TEXT("FX stress: %d sprites, %d emitters"), Fx->NumParticles(), Fx->NumEmitters());
		}));

	// WR.Fx <effect> [scale] [metres ahead] [seconds]: an effect in front of the camera, on the ground.
	static FAutoConsoleCommandWithWorldAndArgs SpawnCmd(TEXT("WR.Fx"), TEXT("WR.Fx <effect> [scale] [metres ahead] [seconds]: show an effect in front of you"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWRFxSubsystem* Fx = World ? World->GetSubsystem<UWRFxSubsystem>() : nullptr;
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!Fx || !PC || !PC->PlayerCameraManager || Args.Num() < 1) return;
			const float Scale = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.f;
			const float Ahead = (Args.Num() > 2 ? FCString::Atof(*Args[2]) : 4.f) * 100.f;
			const float Seconds = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			const FRotator View = PC->PlayerCameraManager->GetCameraRotation();
			const FVector Flat = FRotator(0.f, View.Yaw, 0.f).Vector();
			FVector At = (PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : PC->PlayerCameraManager->GetCameraLocation()) + Flat * Ahead;
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(WRFxCmd), false, PC->GetPawn());
			if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 3000), ECC_WorldStatic, Query)) At = Hit.ImpactPoint;
			Fx->Start(FName(*Args[0]), FTransform(FRotator(0.f, View.Yaw + 180.f, 0.f), At), Scale, Seconds);
		}));
}
