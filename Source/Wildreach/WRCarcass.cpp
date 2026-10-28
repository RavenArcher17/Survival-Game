// Harvestable bodies: a wild creature's kill no longer drops its loot straight into the killer's bag. The body stays
// down in its death pose (gather.body.lifetimeSeconds) and is butchered like a resource node: each melee hit on it
// takes a share of its loot (raw meat, hide and whatever else it drops) into the hitter's bag. A blade in hand (a
// hatchet or a sword) keeps all of what a hit takes, bare hands or a pick only part; the killer and their clan have
// it to themselves for a short while, then anyone may. Emptied, it's gone. Bosses (AWRBoss::OnDeath), tames and
// creatures with no loot keep the old death. Nobody bumps into a body: MulticastDied stops its capsule blocking
// pawns, and here the server lets the melee sweep (a pawn-channel query) see it again as an overlap.
#include "WRCreature.h"
#include "Wildreach.h"
#include "WRCharacter.h"
#include "WRFx.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRTuning.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

namespace WRCarcassDetail
{
	/** Who a kill belongs to first: the player who made it, or the owner of the tame that did. Empty = anyone's. */
	FString ClaimOf(const AActor* Killer)
	{
		const APawn* Pawn = Cast<APawn>(Killer);
		if (const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr)
		{
			return PS->GetPlayerName();
		}
		const AWRCharacterBase* Character = Cast<AWRCharacterBase>(Killer);
		return Character ? Character->GetTameOwner() : FString();
	}

	FString ItemName(FName Id)
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(Id);
		return Def ? Def->Name : Id.ToString();
	}
}

bool AWRCreature::BecomeBody(AActor* Killer)
{
	const FWRCreatureDef* Def = GetDef();
	if (!HasAuthority() || !Def || IsTamed() || Def->Loot.Num() == 0 || Def->Archetype == TEXT("boss") || Def->Archetype == TEXT("dummy"))
	{
		return false;
	}
	const FWRTuning& T = FWRTuning::Get();
	// The pool: its loot at the server's gathering rate (Field dressing counts per harvester, as each takes it).
	const float Mult = WRTune(TEXT("server.gatherMultiplier"), 1.f);
	BodyLoot.Reset();
	int32 Units = 0;
	for (const TPair<FName, int32>& L : Def->Loot)
	{
		const int32 N = FMath::Max(1, FMath::RoundToInt(L.Value * Mult));
		BodyLoot.Add({ L.Key, N, N });
		Units += N;
	}
	const int32 PerHit = FMath::Max(1, FMath::RoundToInt(T.Num(TEXT("gather.body.unitsPerHit"), 2.f)));
	const int32 MinHits = FMath::Max(1, FMath::RoundToInt(T.Num(TEXT("gather.body.minHits"), 2.f)));
	const int32 MaxHits = FMath::Clamp(FMath::RoundToInt(T.Num(TEXT("gather.body.maxHits"), 8.f)), MinHits, 250);
	BodyHitsLeft = static_cast<uint8>(FMath::Min(FMath::Clamp(FMath::DivideAndRoundUp(Units, PerHit), MinHits, MaxHits), Units));  // never a hit with nothing to take
	BodyCarry = 0.f;
	BodyClaim = WRCarcassDetail::ClaimOf(Killer);
	BodyClaimUntil = GetWorld()->GetTimeSeconds() + T.Num(TEXT("gather.body.claimSeconds"), 30.f);

	// The swing finds it (an overlap on the pawn channel); walkers and the camera still pass through.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// Killed in the air (a flyer, a leap): it drops to the ground rather than hanging there.
	FHitResult Ground;
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WRBodyGround), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Ground, Feet, Feet - FVector(0.f, 0.f, 60.f), ECC_WorldStatic, Params))
	{
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->bRunPhysicsWithNoController = true;
		Move->GravityScale = FMath::Max(1.f, Move->GravityScale);
		Move->SetMovementMode(MOVE_Falling);
	}
	FString Loot;
	for (const FBodyLoot& L : BodyLoot) Loot += FString::Printf(TEXT("%s x%d "), *L.Item.ToString(), L.Total);
	UE_LOG(LogWildreach, Log, TEXT("BODY %s (%s) down: %d hits, %s, first for %s"), *GetName(), *CreatureId.ToString(), BodyHitsLeft, *Loot,
		BodyClaim.IsEmpty() ? TEXT("anyone") : *BodyClaim);
	return true;
}

void AWRCreature::OnRep_Body()
{
	// A body that came into view after it fell (it missed the death call): show it down, passable, now.
	if (BodyHitsLeft > 0 && !bDeathShown)
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		OnDeathVisual();
		if (!bDeathShown)
		{
			// Its art isn't set up yet (the first replication comes before BeginPlay): try again in a moment.
			FTimerHandle Retry;
			GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this]() { OnRep_Body(); }), 0.25f, false);
		}
	}
}

void AWRCreature::HarvestBody(AWRCharacterBase* Harvester, bool bHeavy)
{
	if (!HasAuthority() || BodyHitsLeft == 0 || !Harvester)
	{
		return;
	}
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Harvester);
	if (!Inv)
	{
		return;  // only survivors butcher (a creature's bite on a body does nothing)
	}
	AWRPlayerController* PC = Cast<AWRPlayerController>(Harvester->GetController());
	const APlayerState* PS = Harvester->GetPlayerState();
	const FString Who = PS ? PS->GetPlayerName() : FString();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!BodyClaim.IsEmpty() && Now < BodyClaimUntil && !FWRSocialRules::CanShare(BodyClaim, Who))
	{
		if (PC) PC->ClientNotify(FString::Printf(TEXT("That's %s's kill for %d s more"), *BodyClaim, FMath::CeilToInt(BodyClaimUntil - Now)));
		return;
	}

	// This hit takes an even share of what's left (a heavy hit two shares); the last one takes the rest.
	const FWRItemStack& Held = Inv->GetEquipped(EWREquipSlot::Weapon);
	const bool bSharp = FWRGathering::IsSharp(Held);
	const int32 HitsUsed = FMath::Min<int32>(BodyHitsLeft, bHeavy ? 2 : 1);
	int32 Remaining = 0;
	for (const FBodyLoot& L : BodyLoot) Remaining += L.Left;
	const int32 Take = BodyHitsLeft - HitsUsed <= 0 ? Remaining : FMath::Min(Remaining, FMath::DivideAndRoundUp(Remaining * HitsUsed, static_cast<int32>(BodyHitsLeft)));
	// Unit by unit from whatever has the most left for its size, so meat and hide come off together.
	TArray<int32> Taken;
	Taken.Init(0, BodyLoot.Num());
	for (int32 u = 0; u < Take; ++u)
	{
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < BodyLoot.Num(); ++i)
		{
			if (BodyLoot[i].Left > 0 && (Best == INDEX_NONE || BodyLoot[i].Left * BodyLoot[Best].Total > BodyLoot[Best].Left * BodyLoot[i].Total))
			{
				Best = i;
			}
		}
		if (Best == INDEX_NONE) break;
		--BodyLoot[Best].Left;
		++Taken[Best];
	}
	// A blade keeps everything; bare hands (or a pick) tear off only part, the rest is spoiled.
	const float Share = bSharp ? 1.f : FMath::Clamp(WRTune(TEXT("gather.body.bareShare"), 0.5f), 0.f, 1.f);
	const float Dressing = UWRLifeSkillsComponent::StatFor(Harvester, TEXT("creatureLoot"));  // Field dressing
	TArray<FString> Lines;
	int32 Offered = 0, Got = 0;
	for (int32 i = 0; i < BodyLoot.Num(); ++i)
	{
		int32 Keep = Taken[i];
		if (!bSharp)
		{
			Keep = 0;
			for (int32 u = 0; u < Taken[i]; ++u)
			{
				BodyCarry += Share;
				if (BodyCarry >= 1.f - KINDA_SMALL_NUMBER) { BodyCarry -= 1.f; ++Keep; }
			}
		}
		const bool bCarcass = BodyLoot[i].Item == TEXT("raw_meat") || BodyLoot[i].Item == TEXT("hide");
		if (bCarcass && Keep > 0 && Dressing > 0.f)
		{
			const float Raw = Keep * (1.f + Dressing);
			Keep = FMath::FloorToInt(Raw) + (FMath::FRand() < FMath::Frac(Raw) ? 1 : 0);
		}
		if (Keep <= 0) continue;
		Offered += Keep;
		const int32 In = Keep - Inv->AddItem(BodyLoot[i].Item, Keep);
		Got += In;
		if (In > 0) Lines.Add(FString::Printf(TEXT("+%d %s"), In, *WRCarcassDetail::ItemName(BodyLoot[i].Item)));
	}
	if (Offered > 0 && Got == 0)
	{
		// Nothing fit: the hit takes nothing (what it would have cut stays on the body).
		for (int32 i = 0; i < BodyLoot.Num(); ++i) BodyLoot[i].Left += Taken[i];
		if (PC) PC->ClientNotify(TEXT("Your bag is full"));
		return;
	}
	BodyHitsLeft = static_cast<uint8>(FMath::Max(0, BodyHitsLeft - HitsUsed));
	FWRGathering::WearHeld(Harvester, bSharp);

	const FVector Toward = (Harvester->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const FVector At = GetActorLocation() + Toward * GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f;
	WRFx::Broadcast(this, TEXT("hit.blood"), At, Toward.Rotation(), bHeavy ? 1.3f : 1.f, TEXT("hit.flesh"));
	if (PC)
	{
		const FWRItemDef* HeldDef = Held.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Held.ItemId);
		const FString How = bSharp && HeldDef ? FString::Printf(TEXT("  (%s)"), *HeldDef->Name) : FString(TEXT("  (a blade gets more)"));
		const FString What = Lines.Num() > 0 ? FString::Join(Lines, TEXT(", ")) : FString(TEXT("Nothing usable"));
		PC->ClientNotify(What + How + (BodyHitsLeft == 0 ? TEXT("  - the body is spent") : TEXT("")));
	}
	UE_LOG(LogWildreach, Log, TEXT("BODY %s harvested by %s (%s, %s): %s, %d hits left"), *GetName(), *Harvester->GetName(),
		bSharp ? TEXT("blade") : TEXT("blunt"), Held.IsEmpty() ? TEXT("bare hands") : *Held.ItemId.ToString(),
		Lines.Num() > 0 ? *FString::Join(Lines, TEXT(" ")) : TEXT("nothing"), BodyHitsLeft);
	if (BodyHitsLeft == 0)
	{
		// Emptied: it goes, with a puff where it lay.
		WRFx::Broadcast(this, TEXT("death.poof"), GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.6f));
		UE_LOG(LogWildreach, Log, TEXT("BODY %s emptied and gone"), *GetName());
		Destroy();
	}
}
