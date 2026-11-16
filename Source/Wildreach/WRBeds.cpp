// Beds and where you wake (WRBeds.h): who has set each bed, the death screen's choices, resting beds and used-up
// bedrolls.

#include "WRBeds.h"
#include "WRBoss.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRFlowTypes.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Dom/JsonObject.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	FString CampName(const FVector& Location)
	{
		const FWRPoi* Best = nullptr;
		float BestDist = WRTune(TEXT("flow.campMatchRadius"), 6000.f);
		for (const FWRPoi& Poi : FWRPoi::All)
		{
			const float D = FVector::Dist2D(Poi.Location, Location);
			if (Poi.Kind == TEXT("camp") && D < BestDist)
			{
				BestDist = D;
				Best = &Poi;
			}
		}
		return Best ? Best->Name : FString(TEXT("Camp"));
	}

	/** "Vulkarn, the Forge Giant": the boss a shrine-stone belongs to. */
	FString StoneBossName(const AActor* Stone)
	{
		const AWRRunbackStone* S = Cast<AWRRunbackStone>(Stone);
		const FWRCreatureDef* Def = S ? FWRCreatureRegistry::Get().Find(S->BossId) : nullptr;
		return Def ? Def->Name : FString(TEXT("the arena"));
	}

	/** The camps of the player's path (player starts tagged with its biome), one per camp, nearest to Near first. */
	TArray<AActor*> CampsFor(UWorld* World, const AWRPlayerState* PS, const FVector& Near)
	{
		const FWRPathDef* Def = PS ? FWRPathRegistry::Get().Find(PS->GetPaths()->GetMainPath()) : nullptr;
		TArray<AActor*> All, Mine;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			All.Add(*It);
			if (Def && It->PlayerStartTag == Def->Biome) Mine.Add(*It);
		}
		TArray<AActor*>& Pick = Mine.Num() > 0 ? Mine : All;
		Pick.Sort([&Near](const AActor& A, const AActor& B) { return FVector::DistSquared2D(A.GetActorLocation(), Near) < FVector::DistSquared2D(B.GetActorLocation(), Near); });
		TArray<AActor*> Out;
		TSet<FString> Seen;
		for (AActor* A : Pick)
		{
			bool bAlreadySeen = false;
			Seen.Add(CampName(A->GetActorLocation()), &bAlreadySeen);
			if (!bAlreadySeen) Out.Add(A);
		}
		return Out;
	}

	double RestLeft(const AWRPlayerState* PS, const AActor* Bed, double Now)
	{
		const double* Until = PS->BedRestUntil.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Bed)));
		return Until ? FMath::Max(0.0, *Until - Now) : 0.0;
	}

	FString BedLabel(const AWRBuildPiece* Bed, const AWRPlayerState* PS)
	{
		const TCHAR* Thing = WRBeds::IsBedroll(Bed) ? TEXT("bedroll") : TEXT("bed");
		const FString Me = PS->GetPlayerName();
		if (Bed->Sleepers.Contains(Me))
		{
			return FString::Printf(TEXT("Your %s"), Thing);
		}
		for (const FString& S : Bed->Sleepers)
		{
			if (FWRClans::Get().SameClan(Me, S)) return FString::Printf(TEXT("%s's %s"), *S, Thing);
		}
		return FString::Printf(TEXT("%s's %s"), *Bed->OwnerName, Thing);
	}

	/** The Hearths a player may wake at (cooks/cookpass-multiplayer.md 3.6): unbroken, theirs or their clan's. */
	TArray<AActor*> HearthsFor(UWorld* World, const AWRPlayerState* PS)
	{
		TArray<AActor*> Out;
		for (TActorIterator<AWRHearth> It(World); It; ++It)
		{
			const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(It->GetOwner());
			if (Piece && !It->IsBroken() && !It->IsActorBeingDestroyed() && FWRSocialRules::CanShare(Piece->OwnerName, PS->GetPlayerName())) Out.Add(*It);
		}
		return Out;
	}

	/** "Your Hearth at Ashen Ford", "Mira's Hearth": the claim's name when it has one. */
	FString HearthLabel(const AActor* Hearth, const AWRPlayerState* PS)
	{
		const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Hearth->GetOwner());
		const FString Owner = Piece ? Piece->OwnerName : FString();
		const FString Whose = Owner == PS->GetPlayerName() ? FString(TEXT("Your Hearth")) : FString::Printf(TEXT("%s's Hearth"), *Owner);
		const AWRClaimTotem* Claim = AWRClaimTotem::FindAt(Hearth->GetWorld(), Hearth->GetActorLocation());
		const AWRBuildPiece* ClaimPiece = Claim ? Cast<AWRBuildPiece>(Claim->GetOwner()) : nullptr;
		return ClaimPiece && !ClaimPiece->CustomName.IsEmpty() ? FString::Printf(TEXT("%s at %s"), *Whose, *ClaimPiece->CustomName) : Whose;
	}

	/** Where to stand up by a Hearth: the first of eight spots around it (from its front) with room to stand and ground
	 *  under it; else on top of it. */
	AActor* SpotByHearth(AActor* Hearth, float HalfHeight, float Seconds)
	{
		UWorld* World = Hearth->GetWorld();
		const float Ring = WRTune(TEXT("beds.hearthWakeRingCm"), 160.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRHearthWake), false, Hearth);
		Q.AddIgnoredActor(Hearth->GetOwner());
		const FCollisionShape Body = FCollisionShape::MakeCapsule(34.f, HalfHeight);
		const FVector Centre = Hearth->GetActorLocation();
		const float BaseYaw = Hearth->GetActorRotation().Yaw;
		for (int32 i = 0; i < 8; ++i)
		{
			const float Yaw = BaseYaw + (i % 2 ? 1.f : -1.f) * 45.f * ((i + 1) / 2);
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			const FVector Probe = Centre + Dir * Ring + FVector(0.f, 0.f, 200.f);
			FHitResult Ground;
			if (!World->LineTraceSingleByChannel(Ground, Probe, Probe - FVector(0.f, 0.f, 520.f), ECC_Visibility, Q) || Ground.ImpactNormal.Z < 0.7f)
			{
				continue;
			}
			const FVector Stand = Ground.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 5.f);
			if (FMath::Abs(Stand.Z - (Centre.Z + HalfHeight)) > 150.f || World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_Pawn, Body, Q))
			{
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			// Facing the fire.
			if (ATargetPoint* Spot = World->SpawnActor<ATargetPoint>(Stand, FRotator(0.f, Yaw + 180.f, 0.f), Params))
			{
				Spot->SetLifeSpan(Seconds);
				return Spot;
			}
		}
		return WRBeds::SpotAbove(Hearth, HalfHeight, Seconds);
	}

	/** Pin where the controller comes back: on top of a bed, beside a Hearth, or at the camp's start. */
	void PinStart(AController* Controller, AActor* Where, float HalfHeight)
	{
		if (!Controller || !Where) return;
		AActor* Spot = Where;
		const float Keep = WRTune(TEXT("player.respawnSeconds"), 10.f) + 30.f;
		if (Cast<AWRBuildPiece>(Where))
		{
			Spot = WRBeds::SpotAbove(Where, HalfHeight, Keep);
		}
		else if (Cast<AWRHearth>(Where))
		{
			Spot = SpotByHearth(Where, HalfHeight, Keep);
		}
		Controller->StartSpot = Spot ? Spot : Where;
	}
}

FString WRBeds::InSentence(const FString& Name)
{
	return Name.StartsWith(TEXT("Your ")) ? TEXT("your ") + Name.Mid(5) : Name;
}

bool WRBeds::IsBed(const AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	return Def && Def->Kind == TEXT("bed");
}

bool WRBeds::IsBedroll(const AWRBuildPiece* Piece)
{
	return Piece && Piece->PieceId == TEXT("bedroll");
}

void WRBeds::Set(AWRPlayerState* PS, AWRBuildPiece* Bed)
{
	if (!PS || !IsBed(Bed)) return;
	Bed->Sleepers.AddUnique(PS->GetPlayerName());
	PS->RespawnPoint = Bed;
	UE_LOG(LogWildreach, Log, TEXT("BEDS %s set their %s at %s"), *PS->GetPlayerName(), *Bed->PieceId.ToString(), *Bed->GetActorLocation().ToCompactString());
	RefreshAll(Bed->GetWorld());
}

void WRBeds::OnGone(AWRBuildPiece* Bed)
{
	UWorld* World = Bed ? Bed->GetWorld() : nullptr;
	if (!World) return;
	if (Bed->Sleepers.Num() > 0)
	{
		UE_LOG(LogWildreach, Log, TEXT("BEDS %s at %s is gone (set by %s)"), *Bed->PieceId.ToString(), *Bed->GetActorLocation().ToCompactString(), *FString::Join(Bed->Sleepers, TEXT(", ")));
	}
	RefreshAll(World);
}

TArray<AWRBuildPiece*> WRBeds::BedsFor(const AWRPlayerState* PS)
{
	TArray<AWRBuildPiece*> Own, Clan;
	UWorld* World = PS ? PS->GetWorld() : nullptr;
	if (!World) return Own;
	const FString Me = PS->GetPlayerName();
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		AWRBuildPiece* Bed = *It;
		if (Bed->bGhost || Bed->IsActorBeingDestroyed() || !IsBed(Bed) || Bed->Sleepers.Num() == 0) continue;
		if (Bed->Sleepers.Contains(Me))
		{
			Own.Add(Bed);
			continue;
		}
		for (const FString& S : Bed->Sleepers)
		{
			if (FWRClans::Get().SameClan(Me, S)) { Clan.Add(Bed); break; }
		}
	}
	Own.Append(Clan);
	return Own;
}

void WRBeds::Refresh(AWRPlayerState* PS)
{
	if (!PS || !PS->HasAuthority()) return;
	if (AWRBuildPiece* Default = Cast<AWRBuildPiece>(PS->RespawnPoint.Get()))
	{
		if (Default->IsActorBeingDestroyed() || !Default->Sleepers.Contains(PS->GetPlayerName())) PS->RespawnPoint = nullptr;
	}
	PS->Beds.Reset();
	const FString Me = PS->GetPlayerName();
	for (const AWRBuildPiece* Bed : BedsFor(PS))
	{
		FWRBedSpot& S = PS->Beds.AddDefaulted_GetRef();
		S.Name = BedLabel(Bed, PS);
		S.At = Bed->GetActorLocation();
		S.bBedroll = IsBedroll(Bed);
		S.bClan = !Bed->Sleepers.Contains(Me);
	}
	PS->ForceNetUpdate();
}

void WRBeds::RefreshAll(UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS) return;
	for (APlayerState* P : GS->PlayerArray)
	{
		Refresh(Cast<AWRPlayerState>(P));
	}
}

AActor* WRBeds::SpotAbove(AActor* Bed, float HalfHeight, float Seconds)
{
	UWorld* World = Bed ? Bed->GetWorld() : nullptr;
	if (!World) return nullptr;
	FVector Origin, Extent;
	Bed->GetActorBounds(true, Origin, Extent);
	const FVector At = Bed->GetActorLocation();
	const FVector Above(At.X, At.Y, Origin.Z + Extent.Z + HalfHeight + 5.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATargetPoint* Spot = World->SpawnActor<ATargetPoint>(Above, FRotator(0.f, Bed->GetActorRotation().Yaw, 0.f), Params);
	if (Spot) Spot->SetLifeSpan(Seconds);
	return Spot;
}

void WRBeds::FillWakeChoices(AController* Controller, AWRPlayerState* PS, FWRDeathNote& Note, float HalfHeight)
{
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!World || !PS) return;
	const double Now = World->GetTimeSeconds();
	TArray<AWRBuildPiece*> Beds = BedsFor(PS);
	TArray<AActor*> Camps = CampsFor(World, PS, Note.DeathAt);
	TArray<AActor*> Hearths = HearthsFor(World, PS);

	// The default: the bed you set last, unless it's resting; else the camp you came from (or the nearest).
	AActor* Default = nullptr;
	if (AWRBuildPiece* Mine = Cast<AWRBuildPiece>(PS->RespawnPoint.Get()))
	{
		if (Beds.Contains(Mine) && RestLeft(PS, Mine, Now) <= 0.0) Default = Mine;
	}
	if (!Default)
	{
		for (AWRBuildPiece* Bed : Beds)
		{
			if (Bed->Sleepers.Contains(PS->GetPlayerName()) && RestLeft(PS, Bed, Now) <= 0.0) { Default = Bed; break; }
		}
	}
	// A shrine-stone you rested at last (by a boss arena: ER's stake) is where you wake by default, so the run back is
	// the way you came (cooks/build-combat-full.md 2.16).
	TArray<AActor*> Stones;
	for (const TWeakObjectPtr<AActor>& S : PS->BoundStones) if (S.IsValid()) Stones.Add(S.Get());
	if (!Default && PS->RespawnPoint.IsValid() && Stones.Contains(PS->RespawnPoint.Get()))
	{
		Default = PS->RespawnPoint.Get();
	}
	if (!Default)
	{
		AActor* Start = Controller->StartSpot.Get();
		Default = Start && Camps.Contains(Start) ? Start : Camps.Num() > 0 ? Camps[0] : nullptr;
	}

	TArray<AActor*> Order;
	if (Default) Order.Add(Default);
	for (AWRBuildPiece* Bed : Beds) if (Bed != Default) Order.Add(Bed);
	// Your clan's fire is where you come back to (Hestia's, Brigid's): after the beds, before the camps.
	for (AActor* Hearth : Hearths) if (Hearth != Default) Order.Add(Hearth);
	for (AActor* Camp : Camps) if (Camp != Default) Order.Add(Camp);
	for (AActor* Stone : Stones) if (Stone != Default) Order.Add(Stone);

	PS->WakeActors.Reset();
	PS->WakeHalfHeight = HalfHeight;
	Note.Wake.Reset();
	for (AActor* A : Order)
	{
		FWRWakeOption& O = Note.Wake.AddDefaulted_GetRef();
		const AWRBuildPiece* Bed = Cast<AWRBuildPiece>(A);
		O.At = A->GetActorLocation();
		if (Bed)
		{
			O.Name = BedLabel(Bed, PS);
			O.Kind = IsBedroll(Bed) ? FName(TEXT("bedroll")) : FName(TEXT("bed"));
			O.RestSeconds = float(RestLeft(PS, Bed, Now));
			O.bClan = !Bed->Sleepers.Contains(PS->GetPlayerName());
		}
		else if (Hearths.Contains(A))
		{
			const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(A->GetOwner());
			O.Name = HearthLabel(A, PS);
			O.Kind = TEXT("hearth");
			O.RestSeconds = float(RestLeft(PS, A, Now));
			O.bClan = Piece && Piece->OwnerName != PS->GetPlayerName();
		}
		else if (Stones.Contains(A))
		{
			O.Name = FString::Printf(TEXT("Shrine-stone by %s"), *StoneBossName(A));
			O.Kind = TEXT("shrine");
		}
		else
		{
			O.Name = CampName(O.At);
			O.Kind = TEXT("camp");
		}
		PS->WakeActors.Add(A);
	}
	PS->WakePick = Note.Wake.Num() > 0 ? 0 : INDEX_NONE;
	Note.WakePick = 0;
	if (Note.Wake.Num() > 0)
	{
		PinStart(Controller, Order[0], HalfHeight);
		Note.RespawnName = InSentence(Note.Wake[0].Name);
		Note.bRespawnBed = Note.Wake[0].Kind != TEXT("camp");
		Note.RespawnAt = Controller->StartSpot.IsValid() ? Controller->StartSpot->GetActorLocation() : Note.Wake[0].At;
	}
	UE_LOG(LogWildreach, Log, TEXT("BEDS %s can wake at: %s"), *PS->GetPlayerName(),
		*FString::JoinBy(Note.Wake, TEXT(", "), [](const FWRWakeOption& O) { return O.RestSeconds > 0.f ? FString::Printf(TEXT("%s (resting %.0f s)"), *O.Name, O.RestSeconds) : O.Name; }));
}

bool WRBeds::Choose(AController* Controller, AWRPlayerState* PS, int32 Index)
{
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!World || !PS || !PS->WakeActors.IsValidIndex(Index)) return false;
	AActor* Where = PS->WakeActors[Index].Get();
	if (!Where || Where->IsActorBeingDestroyed() || RestLeft(PS, Where, World->GetTimeSeconds()) > 0.0)
	{
		UE_LOG(LogWildreach, Log, TEXT("BEDS %s can't wake at choice %d (gone or resting)"), *PS->GetPlayerName(), Index);
		return false;
	}
	PS->WakePick = Index;
	PinStart(Controller, Where, PS->WakeHalfHeight);
	const AWRBuildPiece* Bed = Cast<AWRBuildPiece>(Where);
	UE_LOG(LogWildreach, Log, TEXT("BEDS %s chose to wake at %s (%s)"), *PS->GetPlayerName(),
		Bed ? *BedLabel(Bed, PS) : Cast<AWRHearth>(Where) ? *HearthLabel(Where, PS) : *CampName(Where->GetActorLocation()), *Where->GetActorLocation().ToCompactString());
	return true;
}

void WRBeds::OnWoke(AWRPlayerState* PS)
{
	if (!PS || !PS->WakeActors.IsValidIndex(PS->WakePick))
	{
		if (PS) PS->WakeActors.Reset();
		return;
	}
	AActor* Picked = PS->WakeActors[PS->WakePick].Get();
	AWRBuildPiece* Bed = Cast<AWRBuildPiece>(Picked);
	PS->WakeActors.Reset();
	PS->WakePick = INDEX_NONE;
	// A Hearth rests longer than a bed after you wake at it (it can be broken, never destroyed).
	if (AWRHearth* Hearth = Cast<AWRHearth>(Picked))
	{
		const float HearthRest = WRTune(TEXT("beds.hearthRestSeconds"), 300.f);
		if (HearthRest > 0.f)
		{
			PS->BedRestUntil.Add(Hearth, Hearth->GetWorld()->GetTimeSeconds() + HearthRest);
			UE_LOG(LogWildreach, Log, TEXT("BEDS %s woke at %s: it rests %.0f s"), *PS->GetPlayerName(), *HearthLabel(Hearth, PS), HearthRest);
		}
		return;
	}
	UWorld* World = Bed ? Bed->GetWorld() : nullptr;
	if (!World || Bed->IsActorBeingDestroyed()) return;
	AWRPlayerController* PC = Cast<AWRPlayerController>(PS->GetOwner());
	if (IsBedroll(Bed) && WRTune(TEXT("beds.bedrollSingleUse"), 1.f) > 0.5f)
	{
		UE_LOG(LogWildreach, Log, TEXT("BEDS %s woke on their bedroll at %s: used up"), *PS->GetPlayerName(), *Bed->GetActorLocation().ToCompactString());
		if (PC) PC->ClientNotify(TEXT("Your bedroll is worn out"));
		Bed->Destroy();
		return;
	}
	const float Rest = WRTune(TEXT("beds.restSeconds"), 60.f);
	if (Rest > 0.f)
	{
		PS->BedRestUntil.Add(Bed, World->GetTimeSeconds() + Rest);
		UE_LOG(LogWildreach, Log, TEXT("BEDS %s woke at their bed at %s: it rests %.0f s"), *PS->GetPlayerName(), *Bed->GetActorLocation().ToCompactString(), Rest);
	}
}

TSharedPtr<FJsonObject> WRBeds::SaveDefault(const AWRPlayerState* PS)
{
	const AActor* Bed = PS ? PS->RespawnPoint.Get() : nullptr;
	if (!Bed) return nullptr;
	const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	const FVector L = Bed->GetActorLocation();
	O->SetNumberField(TEXT("x"), L.X);
	O->SetNumberField(TEXT("y"), L.Y);
	O->SetNumberField(TEXT("z"), L.Z);
	return O;
}

void WRBeds::LoadDefault(AWRPlayerState* PS, const TSharedPtr<FJsonObject>& Json)
{
	if (!PS || !Json.IsValid()) return;
	const FVector At(Json->GetNumberField(TEXT("x")), Json->GetNumberField(TEXT("y")), Json->GetNumberField(TEXT("z")));
	for (AWRBuildPiece* Bed : BedsFor(PS))
	{
		if (Bed->Sleepers.Contains(PS->GetPlayerName()) && FVector::Dist(Bed->GetActorLocation(), At) < 60.f)
		{
			PS->RespawnPoint = Bed;
			UE_LOG(LogWildreach, Log, TEXT("BEDS %s will wake at their %s at %s"), *PS->GetPlayerName(), *Bed->PieceId.ToString(), *At.ToCompactString());
			return;
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("BEDS %s's bed at %s is gone"), *PS->GetPlayerName(), *At.ToCompactString());
}
