// The testers' god mode: state, gates, the proof at login, the commands, the world's marks (WRGod.h;
// cooks/build-godmode.md). The panel and the badge are WRUIGod.cpp.
#include "WRGod.h"
#include "Wildreach.h"
#include "WRGodAuth.h"
#include "WRMainMenu.h"
#include "WRIdentity.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRAttributeSet.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRGameMode.h"
#include "WRGameState.h"
#include "WRWorldSystems.h"
#include "WRWorldDressing.h"
#include "WRPortals.h"
#include "WRFarm.h"
#include "WRInventory.h"
#include "WRCrafting.h"
#include "WRBuilding.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRLifeSkills.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "WRGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WRGodImpl
{
	float Tune(const TCHAR* Key, float Fallback) { return WRTune(*(FString(TEXT("god.")) + Key), Fallback); }

	AWRPlayerState* StateOf(const AActor* Any)
	{
		if (!Any) return nullptr;
		if (const AWRPlayerState* PS = Cast<AWRPlayerState>(Any)) return const_cast<AWRPlayerState*>(PS);
		if (const APawn* P = Cast<APawn>(Any)) return P->GetPlayerState<AWRPlayerState>();
		if (const AController* C = Cast<AController>(Any)) return C->GetPlayerState<AWRPlayerState>();
		return nullptr;
	}

	/** Teleport onto the ground at a place (a place with its own height: from just above it, so a cave stays a cave). */
	FVector Land(UWorld* World, const AActor* Ignore, const FVector& Where)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRGodLand), false, Ignore);
		FHitResult Hit;
		if (!FMath::IsNearlyZero(Where.Z) && World->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 400.f), Where - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q))
		{
			return Hit.ImpactPoint + FVector(0.f, 0.f, 120.f);
		}
		const FVector Top(Where.X, Where.Y, 200000.f);
		if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 400000.f), ECC_WorldStatic, Q))
		{
			return Hit.ImpactPoint + FVector(0.f, 0.f, 120.f);
		}
		return FVector(Where.X, Where.Y, FMath::IsNearlyZero(Where.Z) ? 1000.f : Where.Z + 120.f);
	}

	FVector2D Metres(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, bool& bOk)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		bOk = O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 2;
		return bOk ? FVector2D((*A)[0]->AsNumber() * 100.0, (*A)[1]->AsNumber() * 100.0) : FVector2D::ZeroVector;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString S;
		if (O.IsValid()) O->TryGetStringField(Field, S);
		return S;
	}

	/** The Travel list, from what the world is made of (FWRPoi, world.json, the actors that exist). */
	TArray<FWRGodPlace> BuildPlaces(AWRPlayerController* PC)
	{
		TArray<FWRGodPlace> Out;
		UWorld* World = PC->GetWorld();
		TSet<FString> Seen;
		const auto Add = [&](const FString& Name, FName Kind, const FVector& Where, FName Land = NAME_None)
		{
			const FString Key = Kind.ToString() + TEXT("|") + Name;
			if (Name.IsEmpty() || Seen.Contains(Key)) return;
			Seen.Add(Key);
			FWRGodPlace& P = Out.AddDefaulted_GetRef();
			P.Name = Name;
			P.Kind = Kind;
			P.Where = Where;
			P.Land = Land.IsNone() ? WRBiome::At(World, Where) : Land;
		};
		// Every land in the game (world.json biomes, the live ones): its middle.
		if (const TSharedPtr<FJsonObject> Biomes = WRWorld::Obj(TEXT("biomes")))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Biomes->Values)
			{
				const TSharedPtr<FJsonObject> B = KV.Value.IsValid() ? KV.Value->AsObject() : nullptr;
				bool bOk = false;
				const FVector2D C = Metres(B, TEXT("centre"), bOk);
				if (KV.Key.StartsWith(TEXT("_")) || !bOk || !WRBiome::IsLive(FName(*KV.Key))) continue;
				Add(Str(B, TEXT("name")), TEXT("land"), FVector(C, 0.f), FName(*KV.Key));
			}
		}
		// Actors first (they carry their real spot): villages, shrines (found or not), portals, the other players.
		for (TActorIterator<AWRVillage> It(World); It; ++It) Add(It->DisplayName, TEXT("village"), It->GetActorLocation(), It->Biome);
		for (TActorIterator<AWRShrine> It(World); It; ++It) Add(It->DisplayName, TEXT("shrine"), It->GetActorLocation(), It->Biome);
		// Shrines whose ground hasn't streamed in yet on this server: from world.json.
		if (const TSharedPtr<FJsonObject> Shrines = WRWorld::Obj(TEXT("shrines")))
		{
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Shrines->TryGetArrayField(TEXT("list"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> S = V->AsObject();
					bool bOk = false;
					const FVector2D At = Metres(S, TEXT("at"), bOk);
					if (bOk) Add(Str(S, TEXT("name")), TEXT("shrine"), FVector(At, 0.f), FName(*Str(S, TEXT("biome"))));
				}
			}
		}
		// The boss arenas, camps, caves, passes, landmarks and the rest of the map's places.
		for (const FWRPoi& P : FWRPoi::All)
		{
			const FName Kind = P.Kind.Contains(TEXT("boss")) ? FName(TEXT("boss")) : P.Kind == TEXT("camp") || P.Kind == TEXT("cave") || P.Kind == TEXT("pass") || P.Kind == TEXT("landmark")
				? FName(*P.Kind) : FName(TEXT("place"));
			Add(P.Name, Kind, P.Location);
		}
		// The Old Island's named places that aren't landmarks (island_places.json; the landmarks came in through FWRPoi).
		if (const TSharedPtr<FJsonObject> Isle = FWRTuning::LoadDataFile(TEXT("island_places.json")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if (Isle->TryGetArrayField(TEXT("places"), Rows))
			{
				for (const TSharedPtr<FJsonValue>& V : *Rows)
				{
					const TSharedPtr<FJsonObject> R = V->AsObject();
					bool bOk = false;
					const FVector2D At = Metres(R, TEXT("at"), bOk);
					bool bLandmark = false;
					if (R.IsValid()) R->TryGetBoolField(TEXT("landmark"), bLandmark);
					if (bOk) Add(Str(R, TEXT("name")), bLandmark ? FName(TEXT("landmark")) : FName(TEXT("place")), FVector(At, 0.f));
				}
			}
		}
		// Every boss standing in the world: its home (an arena the map's data didn't name is still on the list).
		for (TActorIterator<AWRBoss> It(World); It; ++It)
		{
			const FVector Home = It->GetHome();
			const bool bListed = Out.ContainsByPredicate([&Home](const FWRGodPlace& P) { return P.Kind == TEXT("boss") && FVector::Dist2D(P.Where, Home) < 5000.f; });
			if (!bListed) Add(It->GetDisplayLabel(), TEXT("boss"), Home);
		}
		// The hidden tubes (world.json hidden): their first room's end.
		const TArray<TSharedPtr<FJsonValue>>* Hidden = nullptr;
		if (const TSharedPtr<FJsonObject> Root = WRWorld::Obj(TEXT("")); Root.IsValid() && Root->TryGetArrayField(TEXT("hidden"), Hidden))
		{
			for (const TSharedPtr<FJsonValue>& V : *Hidden)
			{
				const TSharedPtr<FJsonObject> H = V->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* Route = nullptr;
				if (H.IsValid() && H->TryGetArrayField(TEXT("route"), Route) && Route->Num() > 0)
				{
					const TArray<TSharedPtr<FJsonValue>>& First = (*Route)[0]->AsArray();
					if (First.Num() >= 2) Add(Str(H, TEXT("name")), TEXT("place"), FVector(First[0]->AsNumber() * 100.0, First[1]->AsNumber() * 100.0, 0.f), FName(*Str(H, TEXT("biome"))));
				}
			}
		}
		for (TActorIterator<AWRPortal> It(World); It; ++It)
		{
			Add(FString::Printf(TEXT("Portal: %s, rank %d"), *WRBiome::Name(It->Biome), int32(It->Rank)), TEXT("portal"), It->GetActorLocation(), It->Biome);
		}
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (*It != PC->GetPawn() && It->GetPlayerState()) Add(It->GetPlayerState()->GetPlayerName(), TEXT("player"), It->GetActorLocation());
		}
		static const TCHAR* Order[] = { TEXT("land"), TEXT("boss"), TEXT("shrine"), TEXT("village"), TEXT("camp"), TEXT("portal"), TEXT("cave"), TEXT("pass"), TEXT("landmark"), TEXT("place"), TEXT("player") };
		const auto Rank = [](FName K) { for (int32 i = 0; i < UE_ARRAY_COUNT(Order); ++i) if (K == Order[i]) return i; return 99; };
		Out.Sort([&Rank](const FWRGodPlace& A, const FWRGodPlace& B)
		{
			const int32 RA = Rank(A.Kind), RB = Rank(B.Kind);
			if (RA != RB) return RA < RB;
			if (A.Land != B.Land) return A.Land.LexicalLess(B.Land);
			return A.Name < B.Name;
		});
		return Out;
	}

	/** Stamina and the path bars abilities spend (Mana, Growth, Momentum, Souls, Flow, Radiance, Spores, Void): full under
	 *  Endless vitals. Heat, Chill and Charge are builders (they fill as you fight and nothing waits on them); pinned at
	 *  full they'd fire their full-bar effects (Overheat's blast) on every gain, so they're left to the game. */
	struct FBar { FGameplayAttribute (*Attr)(); float (UWRAttributeSet::*Max)() const; };
	const FBar Bars[] = {
		{ &UWRAttributeSet::GetStaminaAttribute, &UWRAttributeSet::GetMaxStamina },
		{ &UWRAttributeSet::GetGrowthAttribute, &UWRAttributeSet::GetMaxGrowth },
		{ &UWRAttributeSet::GetMomentumAttribute, &UWRAttributeSet::GetMaxMomentum },
		{ &UWRAttributeSet::GetSoulsAttribute, &UWRAttributeSet::GetMaxSouls },
		{ &UWRAttributeSet::GetManaAttribute, &UWRAttributeSet::GetMaxMana },
		{ &UWRAttributeSet::GetFlowAttribute, &UWRAttributeSet::GetMaxFlow },
		{ &UWRAttributeSet::GetRadianceAttribute, &UWRAttributeSet::GetMaxRadiance },
		{ &UWRAttributeSet::GetSporesAttribute, &UWRAttributeSet::GetMaxSpores },
		{ &UWRAttributeSet::GetVoidAttribute, &UWRAttributeSet::GetMaxVoid },
	};

	void FullHealth(AWRCharacterBase* Body)
	{
		UAbilitySystemComponent* ASC = Body ? Body->GetAbilitySystemComponent() : nullptr;
		const UWRAttributeSet* A = Body ? Body->GetAttributes() : nullptr;
		if (ASC && A && A->GetHealth() < A->GetEffectiveMaxHealth()) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), A->GetEffectiveMaxHealth());
	}

	void ClearStatuses(AWRCharacterBase* Body)
	{
		if (!Body || !Body->Status) return;
		for (int32 Id = 0; Id < WRStatus::Count; ++Id) Body->Status->CureStatus(Id);
		const FGameplayTag Tags[] = { WRTags::Status_Burn, WRTags::Status_Frostbite, WRTags::Status_Frozen, WRTags::Status_Slowed, WRTags::Status_Rooted };
		for (const FGameplayTag& T : Tags)
		{
			Body->Status->ClearStatus(T);
		}
	}

	FString FlagName(uint16 F)
	{
		switch (F)
		{
		case EWRGodFlag::Invulnerable: return TEXT("Invulnerable");
		case EWRGodFlag::Vitals: return TEXT("Endless vitals");
		case EWRGodFlag::OneHit: return TEXT("One-hit kill");
		case EWRGodFlag::Fly: return TEXT("Fly");
		case EWRGodFlag::Noclip: return TEXT("Pass through walls");
		case EWRGodFlag::Unseen: return TEXT("Unseen");
		default: return TEXT("?");
		}
	}

	uint16 FlagFromWord(const FString& W)
	{
		if (W == TEXT("invuln")) return EWRGodFlag::Invulnerable;
		if (W == TEXT("vitals")) return EWRGodFlag::Vitals;
		if (W == TEXT("onehit")) return EWRGodFlag::OneHit;
		if (W == TEXT("fly")) return EWRGodFlag::Fly;
		if (W == TEXT("noclip")) return EWRGodFlag::Noclip;
		if (W == TEXT("unseen")) return EWRGodFlag::Unseen;
		return 0;
	}

	/** Items of an armour set: <set>_head / _armor / _hands / _legs (the Ascendant: <set>_ascendant_...). */
	TArray<FName> SetPieces(const FString& Set, bool bAscendant)
	{
		TArray<FName> Out;
		for (const TCHAR* Slot : { TEXT("head"), TEXT("armor"), TEXT("hands"), TEXT("legs") })
		{
			const FName Id(*FString::Printf(TEXT("%s_%s%s"), *Set, bAscendant ? TEXT("ascendant_") : TEXT(""), Slot));
			if (FWRItemRegistry::Get().Find(Id)) Out.Add(Id);
		}
		return Out;
	}
}

// ---------------------------------------------------------------- the component

UWRGodComponent::UWRGodComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UWRGodComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRGodComponent, bTester);
	DOREPLIFETIME(UWRGodComponent, Label);
	DOREPLIFETIME(UWRGodComponent, Flags);
	DOREPLIFETIME(UWRGodComponent, FlySpeed);
	DOREPLIFETIME(UWRGodComponent, bCharacterTested);
	DOREPLIFETIME_CONDITION(UWRGodComponent, bWorldTested, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRGodComponent, bAIFrozen, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRGodComponent, bPublicWorld, COND_OwnerOnly);
}

UWRGodComponent* UWRGodComponent::For(const AActor* Any)
{
	const AWRPlayerState* PS = WRGodImpl::StateOf(Any);
	return PS ? PS->GetGod() : nullptr;
}

void UWRGodComponent::OnRep_Flags()
{
	ApplyMovement();
}

void UWRGodComponent::ApplyMovement()
{
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	ACharacter* Body = PS ? PS->GetPawn<ACharacter>() : nullptr;
	const AWRCharacterBase* WRBody = Cast<AWRCharacterBase>(Body);
	if (!Body || !Body->GetCharacterMovement())
	{
		AppliedTo = nullptr;
		return;
	}
	if (WRBody && !WRBody->IsAlive())
	{
		return;   // a body on the ground is the death flow's: nothing is changed on it (the next one is set up fresh)
	}
	const uint16 Want = bTester ? uint16(Flags & (EWRGodFlag::Fly | EWRGodFlag::Noclip)) : uint16(0);
	UCharacterMovementComponent* Move = Body->GetCharacterMovement();
	const bool bNew = AppliedTo.Get() != Body;
	// Only this machine's own movement is set: the server's and the owner's (a simulated copy follows the server).
	const bool bDrives = Body->HasAuthority() || Body->IsLocallyControlled();
	if (Want != 0)
	{
		if (bDrives && Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		Move->BrakingDecelerationFlying = WRGodImpl::Tune(TEXT("flyBrake"), 4000.f) * FMath::Max(1.f, FlySpeed);
	}
	else if (!bNew && (AppliedFlags & (EWRGodFlag::Fly | EWRGodFlag::Noclip)) && bDrives && Move->MovementMode == MOVE_Flying)
	{
		Move->SetMovementMode(MOVE_Falling);   // let go: they drop from where they were
	}
	if (bNew || ((Want ^ AppliedFlags) & EWRGodFlag::Noclip))
	{
		Body->SetActorEnableCollision((Want & EWRGodFlag::Noclip) == 0);
	}
	AppliedTo = Body;
	AppliedFlags = Want;
}

void UWRGodComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bTester)
	{
		return;
	}
	ApplyMovement();
	if (GetOwner()->HasAuthority())
	{
		ServerClock += DeltaTime;
		if (ServerClock >= WRGodImpl::Tune(TEXT("tickSeconds"), 0.25f))
		{
			ServerClock = 0.f;
			ServerTick();
		}
	}
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	const APlayerController* PC = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		// The hitboxes draw every frame while they're on (and the tick goes back to 4 a second when they're off).
		SetComponentTickInterval(bShowHitboxes ? 0.f : 0.25f);
		if (bShowHitboxes) DrawHitboxes();
	}
}

void UWRGodComponent::ServerTick()
{
	AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	AWRCharacterBase* Body = PS ? PS->GetPawn<AWRCharacterBase>() : nullptr;
	if (!Body || !Body->IsAlive()) return;
	LastAlive = Body->GetActorLocation();
	bHasLastAlive = true;
	UAbilitySystemComponent* ASC = Body->GetAbilitySystemComponent();
	const UWRAttributeSet* A = Body->GetAttributes();
	if (!ASC || !A) return;
	if (Flags & EWRGodFlag::Invulnerable)
	{
		WRGodImpl::FullHealth(Body);   // also what reached health some other way than a blow
	}
	if (Flags & EWRGodFlag::Vitals)
	{
		for (const WRGodImpl::FBar& Bar : WRGodImpl::Bars)
		{
			const float Max = (A->*Bar.Max)();
			if (Max > 0.f && ASC->GetNumericAttributeBase(Bar.Attr()) < Max) ASC->SetNumericAttributeBase(Bar.Attr(), Max);
		}
		if (A->GetHunger() < 100.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), 100.f);
		if (A->GetThirst() < 100.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), 100.f);
		if (A->GetMaxHealthLost() > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthLostAttribute(), 0.f);
		if (A->GetBurnBuildup() > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetBurnBuildupAttribute(), 0.f);
		if (A->GetFrostBuildup() > 0.f) ASC->SetNumericAttributeBase(UWRAttributeSet::GetFrostBuildupAttribute(), 0.f);
		if (Body->Status) for (int32 Id = 0; Id < WRStatus::Count; ++Id) Body->Status->ReduceBuildup(Id, 100000.f);
		if (UWRSurvivalComponent* Survival = PS->GetSurvival(); Survival && Survival->GetCorruption() > 0.f) Survival->AddCorruption(-Survival->GetCorruption());
	}
	if (Flags & EWRGodFlag::Unseen)
	{
		// Anything that had you lets you go (PickTarget and the bosses' pick skip you from now on).
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (It->GetAggroTarget() == Body) It->SetAggroTarget(nullptr);
			if (It->CombatTarget.Get() == Body) It->CombatTarget = nullptr;
		}
	}
}

void UWRGodComponent::DrawHitboxes() const
{
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	const APawn* Me = PS ? PS->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Me || !World) return;
	const float Range = WRGodImpl::Tune(TEXT("hitboxRange"), 3000.f);
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		const AWRCharacterBase* C = *It;
		if (!C->IsAlive() || FVector::Dist(C->GetActorLocation(), Me->GetActorLocation()) > Range) continue;
		const UCapsuleComponent* Cap = C->GetCapsuleComponent();
		if (!Cap) continue;
		const AWRCreature* Creature = Cast<AWRCreature>(C);
		const AActor* After = Creature ? Creature->GetAggroRep() : nullptr;
		// Players green, tames blue, wild amber, anything after you red.
		const FColor Col = C->GetPlayerState() ? FColor(80, 220, 120) : Creature && Creature->IsTamed() ? FColor(90, 160, 255) : After == Me ? FColor(255, 60, 50) : FColor(255, 176, 60);
		DrawDebugCapsule(World, Cap->GetComponentLocation(), Cap->GetScaledCapsuleHalfHeight(), Cap->GetScaledCapsuleRadius(), Cap->GetComponentQuat(), Col, false, -1.f, 0, 1.5f);
		if (After)
		{
			DrawDebugLine(World, C->GetActorLocation() + FVector(0.f, 0.f, 40.f), After->GetActorLocation() + FVector(0.f, 0.f, 40.f), Col, false, -1.f, 0, 1.f);
		}
	}
}

// ---------------------------------------------------------------- the world's marks

bool UWRGodWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRGodWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() != NM_Client) Load();
}

UWRGodWorld* UWRGodWorld::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRGodWorld>() : nullptr;
}

void UWRGodWorld::Load()
{
	Marks = MakeShared<FJsonObject>();
	const FString Dir = WRWorlds::CurrentDir();
	FString Text;
	TSharedPtr<FJsonObject> Json;
	if (!Dir.IsEmpty() && FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("tested.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json.IsValid())
	{
		Marks = Json;
		UE_LOG(LogWildreach, Log, TEXT("GOD this world %s marked tested (tested.json)"), IsWorldTested() ? TEXT("is") : TEXT("isn't"));
	}
}

void UWRGodWorld::Save() const
{
	const FString Dir = WRWorlds::CurrentDir();
	if (Dir.IsEmpty() || !Marks.IsValid()) return;
	FString Text;
	FJsonSerializer::Serialize(Marks.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Dir, TEXT("tested.json")));
}

bool UWRGodWorld::IsWorldTested() const
{
	return Marks.IsValid() && Marks->HasField(TEXT("world"));
}

bool UWRGodWorld::IsCharacterTested(const FString& Name) const
{
	const TSharedPtr<FJsonObject>* Chars = nullptr;
	return Marks.IsValid() && Marks->TryGetObjectField(TEXT("characters"), Chars) && (*Chars)->HasField(Name);
}

void UWRGodWorld::Mark(const FString& Character, const FString& By, bool bWorld)
{
	if (!Marks.IsValid()) Marks = MakeShared<FJsonObject>();
	const FString Now = FDateTime::UtcNow().ToIso8601();
	const auto Note = [&](const TSharedPtr<FJsonObject>& Row)
	{
		if (!Row->HasField(TEXT("first"))) { Row->SetStringField(TEXT("first"), Now); Row->SetStringField(TEXT("by"), By); }
		Row->SetStringField(TEXT("last"), Now);
		double Uses = 0.0;
		Row->TryGetNumberField(TEXT("uses"), Uses);
		Row->SetNumberField(TEXT("uses"), Uses + 1.0);
	};
	const bool bWasWorld = IsWorldTested();
	if (!Character.IsEmpty())
	{
		const TSharedPtr<FJsonObject>* Chars = nullptr;
		TSharedPtr<FJsonObject> C = Marks->TryGetObjectField(TEXT("characters"), Chars) ? *Chars : MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* Row = nullptr;
		TSharedPtr<FJsonObject> R = C->TryGetObjectField(Character, Row) ? *Row : MakeShared<FJsonObject>();
		if (!R->HasField(TEXT("first"))) UE_LOG(LogWildreach, Log, TEXT("GOD %s is a test character from now on (no achievements or kill-time records)"), *Character);
		Note(R);
		C->SetObjectField(Character, R);
		Marks->SetObjectField(TEXT("characters"), C);
	}
	if (bWorld)
	{
		const TSharedPtr<FJsonObject>* W = nullptr;
		TSharedPtr<FJsonObject> R = Marks->TryGetObjectField(TEXT("world"), W) ? *W : MakeShared<FJsonObject>();
		Note(R);
		Marks->SetObjectField(TEXT("world"), R);
		if (!bWasWorld) UE_LOG(LogWildreach, Log, TEXT("GOD this world is marked tested from now on (by %s)"), *By);
	}
	Save();
	SyncComponents();
}

void UWRGodWorld::SyncComponents()
{
	const bool bPublic = WRGod::IsPublicWorld(GetWorld());
	for (TActorIterator<AWRPlayerState> It(GetWorld()); It; ++It)
	{
		if (UWRGodComponent* G = It->GetGod())
		{
			G->bWorldTested = IsWorldTested();
			G->bAIFrozen = bAIFrozen;
			G->bPublicWorld = bPublic;
			G->bCharacterTested = IsCharacterTested(It->GetPlayerName());
		}
	}
}

// ---------------------------------------------------------------- gates and helpers

bool WRGod::IsInvulnerable(const AActor* Who) { const UWRGodComponent* G = UWRGodComponent::For(Who); return G && G->Has(EWRGodFlag::Invulnerable); }
bool WRGod::OneHitKill(const AActor* Who) { const UWRGodComponent* G = UWRGodComponent::For(Who); return G && G->Has(EWRGodFlag::OneHit); }
bool WRGod::IsUnseen(const AActor* Who) { const UWRGodComponent* G = UWRGodComponent::For(Who); return G && G->Has(EWRGodFlag::Unseen); }
bool WRGod::IsFlying(const AActor* Who) { const UWRGodComponent* G = UWRGodComponent::For(Who); return G && (G->Has(EWRGodFlag::Fly) || G->Has(EWRGodFlag::Noclip)); }

float WRGod::FlySpeed(const AActor* Who)
{
	const UWRGodComponent* G = UWRGodComponent::For(Who);
	if (!G || !(G->Has(EWRGodFlag::Fly) || G->Has(EWRGodFlag::Noclip))) return 0.f;
	return WRGodImpl::Tune(TEXT("flyBase"), 600.f) * FMath::Clamp(G->FlySpeed, WRGodImpl::Tune(TEXT("flySpeedMin"), 0.5f), WRGodImpl::Tune(TEXT("flySpeedMax"), 10.f));
}

bool WRGod::IsAIFrozen(const UWorld* World)
{
	const UWRGodWorld* G = World ? World->GetSubsystem<UWRGodWorld>() : nullptr;
	return G && G->bAIFrozen;
}

bool WRGod::IsLocalTester(const APlayerController* PC)
{
	const UWRGodComponent* G = PC && PC->IsLocalController() ? UWRGodComponent::For(PC) : nullptr;
	return G && G->bTester;
}

bool WRGod::IsThisMachineTester()
{
	UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
	return IsLocalTester(World ? World->GetFirstPlayerController() : nullptr);
}

bool WRGod::IsTestedCharacter(const AActor* Any)
{
	const UWRGodComponent* G = UWRGodComponent::For(Any);
	if (G && G->bCharacterTested) return true;
	const AWRPlayerState* PS = WRGodImpl::StateOf(Any);
	return PS && IsTestedName(PS->GetWorld(), PS->GetPlayerName());
}

bool WRGod::IsTestedName(const UWorld* World, const FString& Name)
{
	const UWRGodWorld* G = World ? World->GetSubsystem<UWRGodWorld>() : nullptr;
	return G && G->IsCharacterTested(Name);
}

bool WRGod::IsPublicWorld(const UWorld* World)
{
	if (!World || World->GetNetMode() != NM_DedicatedServer || FParse::Param(FCommandLine::Get(), TEXT("wrtestworld")))
	{
		return false;
	}
	TArray<FString> Private;
	FWRTuning::Get().Str(TEXT("god.privatePresets"), TEXT("playtest")).ParseIntoArrayWS(Private);
	return !Private.Contains(FWRTuning::Get().GetPreset());
}

bool WRGod::IsDevCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0) return false;
	if (Args.Num() >= 2 && Args[1] == TEXT("dev")) return true;   // world dev, portal dev, deeds dev, artifacts dev, swamp dev...
	// The controller's own testing commands (RunServerCommand, before and after the gameplay ones).
	static const TSet<FString> Dev = {
		TEXT("setmain"), TEXT("setsub"), TEXT("givepoints"), TEXT("autospend"), TEXT("buy"), TEXT("weapon"), TEXT("spawn"), TEXT("bossinfo"),
		TEXT("vitals"), TEXT("status"), TEXT("heal"), TEXT("killme"), TEXT("goto"), TEXT("gotoaltar"), TEXT("artifacts"), TEXT("bossdmg"),
		TEXT("clearcreatures"), TEXT("corruption"), TEXT("creatureattack"), TEXT("creaturedrive"), TEXT("creatureface"), TEXT("creatureflinch"),
		TEXT("creaturefree"), TEXT("creaturehp"), TEXT("equip"), TEXT("give"), TEXT("hazard"), TEXT("hearthlevel"), TEXT("hitpiece"),
		TEXT("hitplayer"), TEXT("hold"), TEXT("hour"), TEXT("hurtpack"), TEXT("packs"), TEXT("raid"), TEXT("saveworld"), TEXT("skill"),
		TEXT("skillstat"), TEXT("skillxp"), TEXT("spawnat"), TEXT("starve"), TEXT("tamedev"), TEXT("tamestat"), TEXT("torpor"), TEXT("tp"),
		TEXT("water"), TEXT("wear"), TEXT("weather"), TEXT("wildlife") };
	return Dev.Contains(Args[0]);
}

FString WRGod::Block(const AWRPlayerController* PC)
{
	const UWRGodComponent* G = UWRGodComponent::For(PC);
	if (!G || !G->bTester) return TEXT("not a tester");
	if (IsPublicWorld(PC->GetWorld())) return TEXT("God mode is off on public worlds: it works in single player, on a hosted world and on playtest servers.");
	return FString();
}

// ---------------------------------------------------------------- the proof at login

void WRGod::Challenge(AWRPlayerController* PC)
{
	UWRGodComponent* G = PC ? UWRGodComponent::For(PC) : nullptr;
	if (!G) return;
	if (G->JoinKey.IsEmpty() && PC->IsLocalController())
	{
		G->JoinKey = WRIdentity::Token();   // the host's own player (its travel URL may carry no ?WRKey)
	}
	G->Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	G->ProofTries = 0;
	if (UWRGodWorld* W = UWRGodWorld::Get(PC)) W->SyncComponents();
	PC->ClientGodChallenge(G->Nonce);
}

void WRGod::OnChallenge(AWRPlayerController* PC, const FString& Nonce)
{
	if (SWRMainMenu::IsOpen()) return;   // the menu's own backdrop world: no god mode there, so nothing is asked
	const TWeakObjectPtr<AWRPlayerController> Weak(PC);
	WRGodAuth::RequestProof(Nonce, [Weak](const FString& Proof, const FString& Label)
	{
		if (AWRPlayerController* P = Weak.Get(); P && !Proof.IsEmpty()) P->ServerGodProof(Proof);
	});
}

void WRGod::ReceiveProof(AWRPlayerController* PC, const FString& Proof)
{
	UWRGodComponent* G = PC ? UWRGodComponent::For(PC) : nullptr;
	const FString Name = PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(TEXT("?"));
	if (!G) return;
	if (++G->ProofTries > int32(WRGodImpl::Tune(TEXT("proofsPerConnection"), 4.f)))
	{
		UE_LOG(LogWildreach, Warning, TEXT("GOD refused proof from %s: too many tries this connection"), *Name);
		return;
	}
	WRGodAuth::FClaims Claims;
	FString Why;
	if (!WRGodAuth::Verify(Proof, G->JoinKey, G->Nonce, Claims, Why))
	{
		UE_LOG(LogWildreach, Warning, TEXT("GOD refused proof from %s: %s"), *Name, *Why);
		return;
	}
	G->Nonce.Empty();   // once
	G->bTester = true;
	G->Label = Claims.Label;
	if (UWRGodWorld* W = UWRGodWorld::Get(PC)) W->SyncComponents();
	UE_LOG(LogWildreach, Log, TEXT("GOD %s is a verified tester (%s, Freedom35 account %s)"), *Name, *Claims.Label, *Claims.Sub);
	if (IsPublicWorld(PC->GetWorld())) PC->ClientNotify(TEXT("Tester verified. God mode is off on public worlds."));
}

void WRGod::ReceivePlaces(AWRPlayerController* PC, const TArray<FWRGodPlace>& Places)
{
	if (UWRGodComponent* G = UWRGodComponent::For(PC))
	{
		G->Places = Places;
		++G->PlacesSerial;
	}
}

// ---------------------------------------------------------------- the commands

void WRGod::Run(AWRPlayerController* PC, const TArray<FString>& Args)
{
	using namespace WRGodImpl;
	if (!PC) return;
	AWRPlayerState* PS = PC->GetPlayerState<AWRPlayerState>();
	UWRGodComponent* G = UWRGodComponent::For(PC);
	const FString Name = PS ? PS->GetPlayerName() : FString(TEXT("?"));
	const FString Line = FString::Join(Args, TEXT(" "));
	const FString Why = Block(PC);
	if (!PS || !G || !Why.IsEmpty())
	{
		UE_LOG(LogWildreach, Warning, TEXT("GOD refused '%s' from %s: %s"), *Line, *Name, Why.IsEmpty() ? TEXT("no player") : *Why);
		if (G && G->bTester) PC->ClientNotify(Why);   // a non-tester hears nothing: there's no god mode for them
		return;
	}
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString Verb = Arg(1);
	AWRCharacterBase* Body = Cast<AWRCharacterBase>(PC->GetPawn());
	UWorld* World = PC->GetWorld();
	FString Result;
	bool bWorld = false, bMark = true;

	if (Verb == TEXT("set"))
	{
		const uint16 F = FlagFromWord(Arg(2));
		const bool bOn = Arg(3) != TEXT("0");
		if (!F) Result = TEXT("unknown power");
		else
		{
			uint16 Next = bOn ? uint16(G->Flags | F) : uint16(G->Flags & ~F);
			if (F == EWRGodFlag::Noclip && bOn) Next |= EWRGodFlag::Fly;       // walls are passed in flight
			if (F == EWRGodFlag::Fly && !bOn) Next &= ~EWRGodFlag::Noclip;
			G->Flags = Next;
			G->ApplyMovement();
			Result = FString::Printf(TEXT("%s %s"), *FlagName(F), bOn ? TEXT("on") : TEXT("off"));
			bMark = bOn;
		}
	}
	else if (Verb == TEXT("flyspeed"))
	{
		G->FlySpeed = FMath::Clamp(FCString::Atof(*Arg(2)), Tune(TEXT("flySpeedMin"), 0.5f), Tune(TEXT("flySpeedMax"), 10.f));
		G->ApplyMovement();
		bMark = false;
	}
	else if (Verb == TEXT("heal"))
	{
		FullHealth(Body);
		ClearStatuses(Body);
		if (UAbilitySystemComponent* ASC = Body ? Body->GetAbilitySystemComponent() : nullptr; ASC && Body->GetAttributes())
		{
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), Body->GetAttributes()->GetMaxStamina());
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetHungerAttribute(), 100.f);
			ASC->SetNumericAttributeBase(UWRAttributeSet::GetThirstAttribute(), 100.f);
		}
		Result = TEXT("Healed");
	}
	else if (Verb == TEXT("clearstatus"))
	{
		ClearStatuses(Body);
		Result = TEXT("Statuses cleared");
	}
	else if (Verb == TEXT("revive"))
	{
		AWRGameMode* GM = World->GetAuthGameMode<AWRGameMode>();
		if (Body && Body->IsAlive()) Result = TEXT("You're alive");
		else if (GM)
		{
			const FVector Fell = Body ? Body->GetActorLocation() : G->LastAlive;
			const bool bHere = Body != nullptr || G->bHasLastAlive;
			PS->WakeReadyAt = World->GetTimeSeconds();
			GM->Wake(PC);
			if (APawn* Fresh = PC->GetPawn(); Fresh && bHere) Fresh->TeleportTo(Land(World, Fresh, Fell), Fresh->GetActorRotation());
			Result = TEXT("Back on your feet where you fell");
		}
	}
	else if (Verb == TEXT("killall") && Body)
	{
		int32 Count = 0;
		const float Radius = Tune(TEXT("killRadius"), 4000.f);
		TArray<AWRCreature*> Doomed;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (It->IsAlive() && !It->IsTamed() && !It->IsA<AWRTrainingDummy>() && FVector::Dist(It->GetActorLocation(), Body->GetActorLocation()) < Radius) Doomed.Add(*It);
		}
		for (AWRCreature* C : Doomed)
		{
			// Killed by you, so XP, loot, quests and souls flow as for any kill: a blow first, its death if it stood.
			const UWRAttributeSet* A = C->GetAttributes();
			FWRHit Hit;
			Hit.Instigator = Body;
			Hit.Damage = A ? A->GetHealth() + A->GetShield() + 1.f : 100000.f;
			Hit.bBurst = true;
			Hit.Origin = C->GetActorLocation();
			UWRCombatLibrary::ApplyHit(C, Hit);
			if (IsValid(C) && C->IsAlive()) C->Die(Body);
			++Count;
		}
		Result = FString::Printf(TEXT("%d killed within %.0f m"), Count, Radius / 100.f);
		bWorld = true;
	}
	else if (Verb == TEXT("places"))
	{
		G->Places = BuildPlaces(PC);
		PC->ClientGodPlaces(G->Places);
		bMark = false;
	}
	else if ((Verb == TEXT("go") || Verb == TEXT("goxy")) && Body)
	{
		FVector Where;
		bool bOk = false;
		if (Verb == TEXT("go"))
		{
			const int32 i = FCString::Atoi(*Arg(2));
			bOk = G->Places.IsValidIndex(i);
			if (bOk) { Where = G->Places[i].Where; Result = TEXT("At ") + G->Places[i].Name; }
		}
		else
		{
			Where = FVector(FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)), 0.f);
			bOk = FMath::Abs(Where.X) < 3.0e6 && FMath::Abs(Where.Y) < 3.0e6;
			Result = TEXT("At your waypoint");
		}
		if (bOk) Body->TeleportTo(Land(World, Body, Where), Body->GetActorRotation());
		else Result = TEXT("That place isn't on the list any more: refresh it");
	}
	else if (Verb == TEXT("spawn") && Body)
	{
		const FName Id(*Arg(2));
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Id);
		const int32 Count = FMath::Clamp(FCString::Atoi(*Arg(3)), 1, int32(Tune(TEXT("spawnMax"), 20.f)));
		const bool bTamed = Arg(4) == TEXT("tamed") && Def && Def->Archetype != TEXT("boss");
		int32 Made = 0;
		for (int32 i = 0; Def && i < Count; ++i)
		{
			const float Ahead = Tune(TEXT("spawnAhead"), 700.f) + Def->Radius * 2.f;
			const FVector Side = Body->GetActorRightVector() * ((i % 2 ? 1.f : -1.f) * (i + 1) / 2) * (Def->Radius * 2.5f + 80.f);
			const FVector At = Land(World, Body, Body->GetActorLocation() + Body->GetActorForwardVector() * Ahead + Side);
			if (AWRCreature* C = AWRCreature::SpawnCreature(World, Id, At, FRotator(0.f, Body->GetActorRotation().Yaw + 180.f, 0.f)))
			{
				if (bTamed) C->CompleteTame(Name);
				++Made;
			}
		}
		Result = Def ? FString::Printf(TEXT("%d %s%s"), Made, *Def->Name, bTamed ? TEXT(", tamed to you") : TEXT("")) : TEXT("unknown creature");
		bWorld = true;
	}
	else if (Verb == TEXT("give"))
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(FName(*Arg(2)));
		const int32 Count = FMath::Clamp(FCString::Atoi(*Arg(3)), 1, 9999);
		const int32 Left = Def ? PS->GetInventory()->AddItem(Def->Id, Count) : Count;
		Result = !Def ? TEXT("unknown item") : Left > 0 ? FString::Printf(TEXT("%d %s (%d didn't fit)"), Count - Left, *Def->Name, Left) : FString::Printf(TEXT("%d %s"), Count, *Def->Name);
	}
	else if (Verb == TEXT("giveset"))
	{
		const TArray<FName> Pieces = SetPieces(Arg(2), Arg(3) == TEXT("asc"));
		for (const FName& P : Pieces) PS->GetInventory()->AddItem(P, 1);
		Result = Pieces.Num() ? FString::Printf(TEXT("%d pieces of %s%s"), Pieces.Num(), *Arg(2), Arg(3) == TEXT("asc") ? TEXT(" (Ascendant)") : TEXT("")) : TEXT("no such set");
	}
	else if (Verb == TEXT("recipe"))
	{
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(FName(*Arg(2)));
		const int32 Times = FMath::Clamp(FCString::Atoi(*Arg(4)), 1, 50);
		if (!R) Result = TEXT("unknown recipe");
		else if (Arg(3) == TEXT("materials"))
		{
			for (const TPair<FName, int32>& In : R->Inputs) PS->GetInventory()->AddItem(In.Key, In.Value * Times);
			Result = FString::Printf(TEXT("Materials for %d x %s"), Times, *WRWorld::ItemName(R->Output));
		}
		else
		{
			PS->GetInventory()->AddItem(R->Output, R->Count * Times);
			Result = FString::Printf(TEXT("%d %s"), R->Count * Times, *WRWorld::ItemName(R->Output));
		}
	}
	else if (Verb == TEXT("blueprints"))
	{
		int32 Learned = 0;
		for (const FWRRecipe& R : FWRRecipeRegistry::Get().All())
		{
			if (!R.Blueprint.IsNone() && !PS->KnownBlueprints.Contains(R.Blueprint)) { PS->KnownBlueprints.Add(R.Blueprint); ++Learned; }
		}
		Result = FString::Printf(TEXT("%d blueprints learned"), Learned);
	}
	else if (Verb == TEXT("buildkit"))
	{
		TSet<FName> Materials;
		for (const FWRPieceDef& P : FWRBuildRegistry::Get().All()) for (const TPair<FName, int32>& C : P.Cost) Materials.Add(C.Key);
		const int32 Each = int32(Tune(TEXT("buildKit"), 500.f));
		for (const FName& M : Materials) PS->GetInventory()->AddItem(M, Each);
		Result = FString::Printf(TEXT("%d of each of %d building materials"), Each, Materials.Num());
	}
	else if (Verb == TEXT("hearth"))
	{
		PC->RunDevCommand(FString::Printf(TEXT("hearthlevel %d"), FMath::Clamp(FCString::Atoi(*Arg(2)), 1, 5)));
		Result = TEXT("The nearest Hearth's tech level set (stand by one)");
		bWorld = true;
	}
	else if (Verb == TEXT("path"))
	{
		UWRPathComponent* Paths = PS->GetPaths();
		const FName Path(*Arg(3));
		const FString What = Arg(2);
		if (!FWRPathRegistry::Get().Find(Path)) Result = TEXT("unknown path");
		else if (What == TEXT("main")) { Paths->DevSetMainPath(Path); Result = TEXT("Main path set"); }
		else if (What == TEXT("sub")) { const FString R = Paths->SetSubPath(Path); Result = R.IsEmpty() ? TEXT("Subclass set") : R; }
		else if (!Paths->IsActivePath(Path)) Result = TEXT("Make it your main path or subclass first");
		else if (What == TEXT("points")) { Paths->GivePoints(Path, FMath::Clamp(FCString::Atoi(*Arg(4)), 1, 100)); Result = TEXT("Points given"); }
		else if (What == TEXT("max"))
		{
			Paths->GivePoints(Path, 100);
			const int32 Spent = Paths->AutoSpend(Path, 100);
			Result = FString::Printf(TEXT("Every ability it can learn: %d points spent"), Spent);
		}
		else if (What == TEXT("unlearn")) Result = Paths->UnlearnPath(Path) ? TEXT("Unlearned (its XP kept)") : TEXT("Nothing to unlearn");
	}
	else if (Verb == TEXT("skill"))
	{
		const FName Skill(*Arg(2));
		if (!FWRLifeSkillRegistry::Get().Find(Skill)) Result = TEXT("unknown skill");
		else { PS->GetLifeSkills()->DevSetLevel(Skill, FMath::Clamp(FCString::Atoi(*Arg(3)), 1, 100)); Result = FString::Printf(TEXT("%s level %d"), *Skill.ToString(), PS->GetLifeSkills()->GetLevel(Skill)); }
	}
	else if (Verb == TEXT("hour"))
	{
		if (AWRGameState* State = World->GetGameState<AWRGameState>())
		{
			State->SetHour(FMath::Fmod(FMath::Max(0.f, FCString::Atof(*Arg(2))), 24.f));
			Result = FString::Printf(TEXT("Day %d, %05.2f"), State->GetDay(), State->GetHour());
		}
		bWorld = true;
	}
	else if (Verb == TEXT("weather"))
	{
		if (UWRWeatherSubsystem* Weather = World->GetSubsystem<UWRWeatherSubsystem>())
		{
			Weather->Force(FName(*Arg(2)), FName(*Arg(3)), FMath::Max(1.f, FCString::Atof(*Arg(4))));
			Result = Arg(3) == TEXT("clear") ? TEXT("Weather cleared") : TEXT("Weather on");
		}
		bWorld = true;
	}
	else if (Verb == TEXT("event") || Verb == TEXT("invasion") || Verb == TEXT("quest") || Verb == TEXT("shrines"))
	{
		if (UWRWorldSystems* Sys = UWRWorldSystems::Get(PC))
		{
			TArray<FString> Dev = { TEXT("world"), TEXT("dev"), Verb == TEXT("shrines") ? TEXT("found") : Verb };
			for (int32 i = 2; i < Args.Num(); ++i) Dev.Add(Args[i]);
			Result = Sys->Dev(PC, Dev);
		}
		bWorld = Verb != TEXT("quest");
	}
	else if (Verb == TEXT("freeze"))
	{
		if (UWRGodWorld* W = UWRGodWorld::Get(PC))
		{
			W->bAIFrozen = Arg(2) != TEXT("0");
			W->SyncComponents();
			Result = W->bAIFrozen ? TEXT("Every creature's mind is still") : TEXT("The creatures think again");
			bWorld = W->bAIFrozen;
			bMark = W->bAIFrozen;
		}
	}
	else
	{
		Result = TEXT("unknown god command");
		bMark = false;
	}

	if (bMark)
	{
		if (UWRGodWorld* W = UWRGodWorld::Get(PC)) W->Mark(Name, G->Label, bWorld);
	}
	UE_LOG(LogWildreach, Log, TEXT("GOD %s (%s): %s -> %s"), *G->Label, *Name, *Line, Result.IsEmpty() ? TEXT("ok") : *Result);
	if (!Result.IsEmpty()) PC->ClientNotify(Result);
}

TArray<FString> WRGod::ActiveWords(const UWRGodComponent* G)
{
	TArray<FString> Out;
	if (!G || !G->bTester) return Out;
	if (G->Flags & EWRGodFlag::Invulnerable) Out.Add(TEXT("INVULNERABLE"));
	if (G->Flags & EWRGodFlag::Vitals) Out.Add(TEXT("ENDLESS"));
	if (G->Flags & EWRGodFlag::OneHit) Out.Add(TEXT("ONE-HIT"));
	if (G->Flags & EWRGodFlag::Noclip) Out.Add(TEXT("NOCLIP"));
	else if (G->Flags & EWRGodFlag::Fly) Out.Add(TEXT("FLY"));
	if (G->Flags & EWRGodFlag::Unseen) Out.Add(TEXT("UNSEEN"));
	if (G->bAIFrozen) Out.Add(TEXT("AI FROZEN"));
	return Out;
}

TSharedPtr<FJsonObject> WRGod::ReportContext(const APlayerController* PC)
{
	const UWRGodComponent* G = UWRGodComponent::For(PC);
	if (!G || (!G->bTester && !G->bCharacterTested && !G->bWorldTested)) return nullptr;
	const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetBoolField(TEXT("tester"), G->bTester);
	if (!G->Label.IsEmpty()) O->SetStringField(TEXT("label"), G->Label);
	O->SetStringField(TEXT("on"), FString::Join(ActiveWords(G), TEXT(", ")));
	O->SetBoolField(TEXT("characterTested"), G->bCharacterTested);
	O->SetBoolField(TEXT("worldTested"), G->bWorldTested);
	return O;
}
