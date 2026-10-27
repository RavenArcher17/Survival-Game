// Server side of the death screen (WRUIFlow): names what killed a survivor, finds the belongings bag the death penalty
// just left, pins where they'll come back, and sends it all to them as one death note.

#include "WRFlowTypes.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCreature.h"
#include "WRGameMode.h"
#include "WRGameplayTags.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "AbilitySystemComponent.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"

namespace WRFlowDeath
{
	/** Who killed them, or what wore them down when nobody did (empty: unknown). */
	FString KillerName(const AWRCharacter* Died, const AActor* Killer, FName& OutCreature, bool& bOutPlayer)
	{
		// A projectile or a lingering effect counts as whoever made it.
		const AActor* Who = Killer;
		if (Who && !Who->IsA<AWRCharacterBase>() && Who->GetInstigator())
		{
			Who = Who->GetInstigator();
		}
		if (const AWRCreature* C = Cast<AWRCreature>(Who))
		{
			OutCreature = C->CreatureId;
			const FWRCreatureDef* Def = C->GetDef();
			// A tame says whose it is; a wild one just its kind (not "(asleep)" or other states).
			return C->IsTamed() ? C->GetDisplayLabel() : Def ? Def->Name : C->GetDisplayLabel();
		}
		if (const AWRCharacterBase* K = Cast<AWRCharacterBase>(Who))
		{
			bOutPlayer = K->GetPlayerState() != nullptr;
			return K != Died ? K->GetDisplayLabel() : FString();
		}
		const UAbilitySystemComponent* ASC = Died->GetAbilitySystemComponent();
		const UWRAttributeSet* A = Died->GetAttributes();
		if (ASC && ASC->HasMatchingGameplayTag(WRTags::Status_Burn)) return TEXT("Burns");
		if (ASC && ASC->HasMatchingGameplayTag(WRTags::Status_Frostbite)) return TEXT("Frostbite");
		if (FWRBiomeHazards::KindAt(Died->GetActorLocation() - FVector(0.f, 0.f, Died->GetSimpleCollisionHalfHeight())) == TEXT("lava")) return TEXT("Lava");
		if (A && A->GetThirst() <= 0.f) return TEXT("Thirst");
		if (A && A->GetHunger() <= 0.f) return TEXT("Hunger");
		return FString();
	}

	/** The camp a player start belongs to (camps sit on the spawns), or empty. */
	FString CampAt(const FVector& Location)
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
		return Best ? Best->Name : FString();
	}
}

void WRFlow::SendDeathNote(AWRCharacter* Died, AActor* Killer)
{
	if (!Died || !Died->HasAuthority())
	{
		return;
	}
	AController* Controller = Died->GetController();
	AWRPlayerController* PC = Cast<AWRPlayerController>(Controller);
	AWRPlayerState* PS = Died->GetPlayerState<AWRPlayerState>();
	if (!Controller || !PS)
	{
		return;
	}
	FWRDeathNote Note;
	Note.Killer = WRFlowDeath::KillerName(Died, Killer, Note.KillerCreature, Note.bKillerIsPlayer);
	Note.DeathAt = Died->GetActorLocation();
	Note.Penalty = FWRSocialRules::Setting(TEXT("deathPenalty"), TEXT("gentle"));
	Note.Seconds = WRTune(TEXT("player.respawnSeconds"), 10.f);

	// The bag the death penalty just dropped (it ran first): theirs, where they fell.
	const AWRLootBag* Bag = nullptr;
	float BagDist = 400.f;
	for (TActorIterator<AWRLootBag> It(Died->GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Note.DeathAt);
		if (It->OwnerName == PS->GetPlayerName() && D < BagDist)
		{
			BagDist = D;
			Bag = *It;
		}
	}
	if (Bag)
	{
		Note.bBag = true;
		Note.BagAt = Bag->GetActorLocation();
		Note.BagStacks = Bag->Items.Num();
		Note.BagMinutes = WRTune(TEXT("death.bagMinutes"), 30.f);
	}

	// Where they come back. The engine respawns a player at their controller's start spot, so a bed has to be pinned
	// there to count (ChoosePlayerStart alone is only asked when there is no start spot). Without a bed they keep the
	// camp they started at, as before.
	AActor* Start = PS->RespawnPoint.Get();
	Note.bRespawnBed = Start != nullptr;
	if (!Start)
	{
		Start = Controller->StartSpot.Get();
	}
	if (!Start)
	{
		if (AWRGameMode* GM = Died->GetWorld()->GetAuthGameMode<AWRGameMode>())
		{
			Start = GM->ChoosePlayerStart(Controller);
		}
	}
	if (Start)
	{
		Controller->StartSpot = Start;
		Note.RespawnAt = Start->GetActorLocation();
		if (Note.bRespawnBed)
		{
			const AWRBuildPiece* Bed = Cast<AWRBuildPiece>(Start);
			const FWRPieceDef* Def = Bed ? Bed->GetDef() : nullptr;
			Note.RespawnName = FString::Printf(TEXT("your %s"), Def ? *Def->Name.ToLower() : TEXT("bed"));
			// The bed's origin is on the floor: a body spawned there sinks into it. Wake on top of the bed instead, at a
			// short-lived marker (gone after this respawn, so a bed broken later isn't remembered).
			FVector Origin, Extent;
			Start->GetActorBounds(true, Origin, Extent);
			const FVector Above(Note.RespawnAt.X, Note.RespawnAt.Y, Origin.Z + Extent.Z + Died->GetSimpleCollisionHalfHeight() + 5.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ATargetPoint* Spot = Died->GetWorld()->SpawnActor<ATargetPoint>(Above, FRotator(0.f, Start->GetActorRotation().Yaw, 0.f), Params))
			{
				Spot->SetLifeSpan(Note.Seconds + 30.f);
				Controller->StartSpot = Spot;
				Note.RespawnAt = Above;
			}
		}
		else
		{
			Note.RespawnName = WRFlowDeath::CampAt(Note.RespawnAt);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("FLOW death note for %s: killed by '%s', penalty %s%s, back at '%s' %s in %.0f s"),
		*PS->GetPlayerName(), *Note.Killer, *Note.Penalty, Note.bBag ? *FString::Printf(TEXT(", bag of %d stacks at %s"), Note.BagStacks, *Note.BagAt.ToCompactString()) : TEXT(""),
		*Note.RespawnName, *Note.RespawnAt.ToCompactString(), Note.Seconds);
	if (PC)
	{
		PC->ClientDeathNote(Note);
	}
}
