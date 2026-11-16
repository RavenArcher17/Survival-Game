// Player raiding: one damage rule for every player-side source, the raid tools, the raid bell and the raid record
// (WRRaiding.h; cooks/build-server-raids.md 2.3 to 2.7, 2.10, 2.11).
#include "WRRaiding.h"
#include "WRSiege.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRBuildSets.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRProjectile.h"
#include "WRRaidLook.h"
#include "WRResourceNode.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRStore.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

namespace WRRaidingImpl
{
	float Num(const FString& Key, float Fallback) { return WRTune(*(FString(TEXT("raiding.")) + Key), Fallback); }

	/** Server time every machine agrees on (the bell's swing, a fuse's burn). */
	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : World ? World->GetTimeSeconds() : 0.f;
	}

	const TCHAR* SourceKey(EWRRaidSource S)
	{
		switch (S)
		{
		case EWRRaidSource::Weapon: return TEXT("weapon");
		case EWRRaidSource::Blast: return TEXT("blast");
		case EWRRaidSource::FirePot: return TEXT("firepot");
		case EWRRaidSource::Spell: return TEXT("spell");
		case EWRRaidSource::Tame: return TEXT("tame");
		case EWRRaidSource::Ram: return TEXT("ram");
		case EWRRaidSource::Siege: return TEXT("siege");
		default: return TEXT("fire");
		}
	}

	/** One raid on one claim: from its first hit until raiding.bell.raidQuietSeconds pass with none. */
	struct FRaid
	{
		FString Claim;           // the totem's key ("x,y" in metres)
		FString Owner;           // the claim's owner (their clan shares it)
		FString Name;            // what the notices call it
		int64 Start = 0, End = 0;   // unix seconds, UTC
		TSet<FString> Raiders;
		int32 Broken = 0;
		bool bHearth = false;
		TSet<FString> Told;      // members who've heard about it (live or at login)
	};
	TArray<FRaid> Raids;
	TMap<FString, double> LastAlert;     // claim key -> world time of its last alert to members
	bool bDirty = false;

	/** A code breaker being held to a keypad. */
	struct FBreak
	{
		TWeakObjectPtr<AWRPlayerController> PC;
		TWeakObjectPtr<AWRBuildPiece> Door;
		double Start = 0.0;
		FVector At = FVector::ZeroVector;
		float Health = 0.f;
		double NextRing = 0.0;
		int32 Said = 0;
	};
	TArray<FBreak> Breaks;

	float HealthOf(const AWRCharacterBase* Who)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		return A ? A->GetHealth() : 0.f;
	}

	FString ClaimKey(const AWRClaimTotem* T)
	{
		const FVector L = T->GetActorLocation();
		return FString::Printf(TEXT("%d,%d"), FMath::RoundToInt(L.X / 100.f), FMath::RoundToInt(L.Y / 100.f));
	}

	FString Compass(const FVector& From, const FVector& To)
	{
		const FVector D = (To - From) * FVector(1.f, 1.f, 0.f);
		if (D.SizeSquared() < 1.f) return TEXT("here");
		// Unreal: +X north, +Y east (the island's map convention, WRUIMap).
		static const TCHAR* Names[] = { TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"), TEXT("south"), TEXT("south-west"), TEXT("west"), TEXT("north-west") };
		float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		if (Deg < 0.f) Deg += 360.f;
		return Names[FMath::RoundToInt(Deg / 45.f) % 8];
	}

	AWRPlayerController* ControllerOf(UWorld* World, const FString& Name)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Name) return PC;
		}
		return nullptr;
	}

	FString WhoList(const TSet<FString>& Raiders)
	{
		TArray<FString> Names = Raiders.Array();
		Names.Sort();
		if (Names.Num() == 0) return TEXT("someone");
		if (Names.Num() == 1) return Names[0];
		if (Names.Num() == 2) return Names[0] + TEXT(" and ") + Names[1];
		return FString::Printf(TEXT("%s and %d others"), *Names[0], Names.Num() - 1);
	}

	/** The raid record for a hit on a claim's piece; rings its bell and tells its owners who are on. */
	void NoteHit(AWRBuildPiece* Piece, const FString& Raider, bool bBroken, bool bHearthBroke)
	{
		UWorld* World = Piece->GetWorld();
		AWRClaimTotem* Totem = AWRClaimTotem::FindAt(World, Piece->GetActorLocation());
		const FString Owner = Totem ? Totem->OwnerName : Piece->OwnerName;
		const FString Key = Totem ? ClaimKey(Totem) : FString::Printf(TEXT("piece:%s"), *Owner);
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		const int64 Quiet = int64(Num(TEXT("bell.raidQuietSeconds"), 600.f));
		FRaid* Raid = nullptr;
		for (int32 i = Raids.Num() - 1; i >= 0; --i)
		{
			if (Raids[i].Claim == Key && Now - Raids[i].End <= Quiet) { Raid = &Raids[i]; break; }
		}
		if (!Raid)
		{
			Raid = &Raids.AddDefaulted_GetRef();
			Raid->Claim = Key;
			Raid->Owner = Owner;
			Raid->Name = Totem ? FString::Printf(TEXT("%s's land claim"), *Owner) : FString::Printf(TEXT("%s's building"), *Owner);
			Raid->Start = Now;
			// Keep the last few per claim.
			const int32 Keep = FMath::Max(1, FMath::RoundToInt(Num(TEXT("bell.logRaids"), 10.f)));
			int32 Count = 0;
			for (int32 i = Raids.Num() - 1; i >= 0; --i)
			{
				if (Raids[i].Claim == Key && ++Count > Keep) Raids.RemoveAt(i);
			}
			Raid = nullptr;
			for (int32 i = Raids.Num() - 1; i >= 0; --i) if (Raids[i].Claim == Key) { Raid = &Raids[i]; break; }
			UE_LOG(LogWildreach, Log, TEXT("RAIDING %s started by %s"), *Key, *Raider);
		}
		Raid->End = Now;
		if (!Raider.IsEmpty()) Raid->Raiders.Add(Raider);
		if (bBroken) ++Raid->Broken;
		Raid->bHearth |= bHearthBroke;
		bDirty = true;

		if (!Totem) return;
		FWRRaiding::RingBell(Totem);
		// The owners who are on hear of it (once every bell.alertGapSeconds per claim), wherever they are.
		const double WNow = World->GetTimeSeconds();
		if (WNow - LastAlert.FindRef(Key, -1.0e9) < Num(TEXT("bell.alertGapSeconds"), 30.f)) return;
		LastAlert.Add(Key, WNow);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (!PC || !PC->PlayerState || !FWRSocialRules::CanShare(Owner, PC->PlayerState->GetPlayerName())) continue;
			const APawn* Me = PC->GetPawn();
			const FString Where = Me ? FString::Printf(TEXT("%.0f m %s of you"), FVector::Dist2D(Me->GetActorLocation(), Totem->GetActorLocation()) / 100.f,
				*Compass(Me->GetActorLocation(), Totem->GetActorLocation())) : FString(TEXT("on the map"));
			PC->ClientRaid(NAME_None, TEXT("Claim attacked"), FString::Printf(TEXT("Raid: %s is attacking your claim, %s"), *WhoList(Raid->Raiders), *Where), 0);
			Raid->Told.Add(PC->PlayerState->GetPlayerName());
		}
	}

	/** May a blast or burst hurt Who (a character)? Raider's clanmates and their tames are spared; the raider isn't. */
	bool BlastHits(const AWRCharacterBase* Who, const FString& Raider)
	{
		if (!Who || !Who->IsAlive()) return false;
		const FString Them = FWRRaiding::Responsible(Who);
		if (Raider.IsEmpty() || Them.IsEmpty()) return true;                     // wild creatures, an unowned blast
		if (Them == Raider) return Who->IsA<AWRCharacter>();                     // yourself yes (stand back), your tames no
		if (FWRSocialRules::CanShare(Raider, Them)) return false;                // clanmates and their tames
		// Another survivor (or their tame) only where players can fight.
		return WRTune(TEXT("server.pvp"), 0.f) > 0.f;
	}

	void Burst(UWorld* World, AActor* Causer, AActor* Instigator, const FVector& At, float Radius, float Damage, float EdgeShare, float Burn, const FString& Raider)
	{
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			AWRCharacterBase* Who = *It;
			const float D = FVector::Dist(Who->GetActorLocation(), At) - Who->GetSimpleCollisionRadius();
			if (D > Radius || !BlastHits(Who, Raider)) continue;
			// Nothing solid between the blast and them (a wall takes it).
			FHitResult Block;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRaidBurst), false, Causer);
			Q.AddIgnoredActor(Who);
			if (World->LineTraceSingleByChannel(Block, At + FVector(0.f, 0.f, 20.f), Who->GetActorLocation(), ECC_Visibility, Q) && Block.GetActor() != Who)
			{
				continue;
			}
			FWRHit Hit;
			Hit.Instigator = Instigator ? Instigator : Causer;
			Hit.Damage = Damage * FMath::Lerp(1.f, EdgeShare, FMath::Clamp(D / FMath::Max(1.f, Radius), 0.f, 1.f));
			Hit.PoiseDamage = Hit.Damage;
			Hit.Origin = At;
			Hit.bUnblockable = true;
			Hit.bHeavy = true;
			Hit.Impact = 3;
			Hit.Tier = EWRHitTier::Heavy;
			Hit.bProjectile = true;
			if (Burn > 0.f)
			{
				Hit.Element = WRTags::Element_Fire;
				Hit.BurnBuildup = Burn;
			}
			UWRCombatLibrary::ApplyHit(Who, Hit);
		}
	}

	/** Eye height and the way a survivor faces (the server has no camera of theirs: their view rotation). */
	bool TraceInFront(const AWRCharacterBase* Who, float Reach, FHitResult& Out)
	{
		const FVector From = Who->GetPawnViewLocation();
		const FVector To = From + Who->GetControlRotation().Vector() * (Reach + Who->GetSimpleCollisionRadius());
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRaidUse), false, Who);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		return Who->GetWorld()->LineTraceSingleByObjectType(Out, From, To, Objects, Q);
	}

	FBox PieceBox(const AWRBuildPiece* Piece)
	{
		FBox Box = Piece->GetComponentsBoundingBox(false);
		if (const AActor* Child = Piece->GetChild())
		{
			const FBox C = Child->GetComponentsBoundingBox(false);
			if (C.IsValid) Box += C;
		}
		return Box.IsValid ? Box : FBox::BuildAABB(Piece->GetActorLocation(), FVector(100.f));
	}
}

// ---------------------------------------------------------------- the rule

void FWRRaiding::RingBell(AWRClaimTotem* Totem)
{
	using namespace WRRaidingImpl;
	UWorld* World = Totem ? Totem->GetWorld() : nullptr;
	if (!World || !Totem->HasAuthority() || Totem->IsActorBeingDestroyed()) return;
	// At most once every bell.minGapSeconds; every machine swings it from BellRungAt (UWRRaidBellComponent).
	const float SNow = ServerNow(World);
	if (SNow - Totem->BellRungAt < Num(TEXT("bell.minGapSeconds"), 2.f)) return;
	Totem->BellRungAt = SNow;
	Totem->ForceNetUpdate();
	// The strike comes when the clapper reaches the sound bow, a quarter swing after the blow; it's heard from the bell.
	const FVector BellAt = Totem->GetActorLocation() + FVector(0.f, 0.f, 220.f);
	FTimerHandle Strike;
	World->GetTimerManager().SetTimer(Strike, FTimerDelegate::CreateWeakLambda(Totem, [Totem, BellAt]()
	{
		WRFx::Broadcast(Totem, NAME_None, BellAt, FRotator::ZeroRotator, 1.f, TEXT("raid.bell"));
	}), FMath::Max(0.05f, Num(TEXT("bell.periodSeconds"), 1.1f) * 0.25f), false);
}

AWRBuildPiece* FWRRaiding::PieceOf(AActor* Actor)
{
	if (!Actor) return nullptr;
	if (AWRBuildPiece* P = Cast<AWRBuildPiece>(Actor)) return P;
	return Cast<AWRBuildPiece>(Actor->GetOwner());
}

FString FWRRaiding::Responsible(const AActor* Actor)
{
	if (const AWRCreature* C = Cast<AWRCreature>(Actor))
	{
		return C->IsTamed() ? C->TamedBy : FString();
	}
	if (const APawn* P = Cast<APawn>(Actor))
	{
		return P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	}
	return FString();
}

bool FWRRaiding::MayRaid(const AWRBuildPiece* Piece, const FString& Raider, FString* OutWhy)
{
	if (!Piece || Piece->bGhost || Piece->IsActorBeingDestroyed() || Raider.IsEmpty())
	{
		return false;
	}
	if (Piece->CanAccess(Raider))
	{
		if (OutWhy) *OutWhy = TEXT("That's yours (or your clan's)");
		return false;
	}
	if (WRTune(TEXT("server.pvp"), 0.f) <= 0.f)
	{
		if (OutWhy) *OutWhy = TEXT("No raiding on this world (PvP is off)");
		return false;
	}
	return true;
}

FName FWRRaiding::Category(const AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	if (!Def) return TEXT("other");
	if (Def->Kind == TEXT("hearth") || Cast<AWRHearth>(Piece->GetChild())) return TEXT("hearth");
	if (Def->Material == TEXT("wood") || Def->Material == TEXT("stone") || Def->Material == TEXT("metal")) return Def->Material;
	if (const FWRTierDef* T = FWRBuildSets::Get().Tier(Def->Material); T && T->IsBiome()) return TEXT("biome");
	return TEXT("other");
}

float FWRRaiding::Scale(const AWRBuildPiece* Piece, EWRRaidSource Source, FName PathId)
{
	using namespace WRRaidingImpl;
	const FName Cat = Category(Piece);
	float Share = 1.f;
	if (Source == EWRRaidSource::Weapon)
	{
		// Weapons keep the building tab's own table (building_sets.json materialDamage: wood 10%, stone 2%, metal 0).
		const FWRPieceDef* Def = Piece->GetDef();
		Share = Def ? FWRBuildSets::Get().WeaponShare(Def->Material) : 1.f;
	}
	else
	{
		const FName Row = Cat == TEXT("hearth") ? FName(TEXT("stone")) : Cat;   // the Hearth is a stone hearth to everything but a charge
		Share = Num(FString::Printf(TEXT("material.%s.%s"), SourceKey(Source), *Row.ToString()), Source == EWRRaidSource::Blast ? 1.f : 0.5f);
	}
	if (!PathId.IsNone() && Share > 0.f)
	{
		// The paths as raiders (raiding.paths): fire burns wood (Raven: "fire burns wooden walls"), lightning runs
		// through metal, spore rots wood, void eats everything, arcane force cracks stone.
		const FString P = FString::Printf(TEXT("paths.%s."), *PathId.ToString());
		Share *= Num(P + Cat.ToString(), 1.f) * Num(P + TEXT("all"), 1.f);
	}
	return Share;
}

float FWRRaiding::DamagePiece(AWRBuildPiece* Piece, float Amount, EWRRaidSource Source, AActor* Causer, const FString& Raider, FName PathId)
{
	using namespace WRRaidingImpl;
	if (!Piece || !Piece->HasAuthority() || Amount <= 0.f || !MayRaid(Piece, Raider))
	{
		return 0.f;
	}
	float Dealt = Amount * Scale(Piece, Source, PathId);
	if (Source == EWRRaidSource::Blast) Dealt *= 1.f - Piece->BlastResist;   // Siege-proof walls
	if (Dealt <= 0.f)
	{
		return 0.f;
	}
	const AWRHearth* Hearth = Cast<AWRHearth>(Piece->GetChild());
	const bool bHearthWasBroken = Hearth && Hearth->IsBroken();
	const FString Name = Piece->GetName(), Kind = Piece->PieceId.ToString();
	TWeakObjectPtr<AWRBuildPiece> Weak = Piece;
	Piece->TakeBuildDamage(Dealt, Causer);
	const bool bBroken = !Weak.IsValid() || Weak->IsActorBeingDestroyed();
	const bool bHearthBroke = Hearth && !bHearthWasBroken && Hearth->IsBroken();
	UE_LOG(LogWildreach, Log, TEXT("RAIDING %s hit %s (%s) by %s for %.0f%s"), *Raider, *Name, *Kind, SourceKey(Source), Dealt, bBroken ? TEXT(": broken") : TEXT(""));
	if (Weak.IsValid() || bBroken)
	{
		// (A destroyed piece is still a valid object this frame: its world and place are read before GC.)
		NoteHit(Piece, Raider, bBroken, bHearthBroke);
	}
	return Dealt;
}

int32 FWRRaiding::SplashPieces(UWorld* World, const FVector& At, float Radius, float Amount, EWRRaidSource Source, AActor* Causer, const FString& Raider, FName PathId, const AWRBuildPiece* Skip)
{
	using namespace WRRaidingImpl;
	if (!World || Raider.IsEmpty() || Amount <= 0.f) return 0;
	TArray<AWRBuildPiece*> Hit;
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		AWRBuildPiece* P = *It;
		if (P == Skip || P->bGhost || P->IsActorBeingDestroyed()) continue;
		if (FVector::DistSquared(PieceBox(P).GetClosestPointTo(At), At) > Radius * Radius) continue;
		if (!MayRaid(P, Raider)) continue;
		Hit.Add(P);
	}
	for (AWRBuildPiece* P : Hit)
	{
		if (IsValid(P) && !P->IsActorBeingDestroyed()) DamagePiece(P, Amount, Source, Causer, Raider, PathId);
	}
	// Siege engines in the same reach take it too (their own shares: siege.take.<source>).
	FWRSiege::Splash(World, At, Radius, Amount, Source, Causer, Raider);
	return Hit.Num();
}

void FWRRaiding::TickThrowArc(AWRCharacter* Who, float DeltaSeconds)
{
	static double Next = 0.0;
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	if (!World || !Who->IsAimingRanged()) return;
	const FWRItemDef* Pot = FWRItemRegistry::Get().Find(Who->GetRangedItem());
	if (!Pot || Pot->RangedKind != TEXT("firepot")) return;
	const double Now = World->GetTimeSeconds();
	if (Now < Next) return;
	const float Every = 0.05f;
	Next = Now + Every;
	// The server's own throw (AWRCharacter::ServerFireRanged / LaunchProjectile): from the chest toward the crosshair's
	// point, at the pot's speed, under its gravity.
	FVector Aim;
	Who->CrosshairPoint(20000.f, true, Aim);
	const FVector Muzzle0 = Who->GetActorLocation() + FVector(0.f, 0.f, 55.f) + Who->GetActorForwardVector() * 35.f;
	FVector Dir = (Aim - Muzzle0).GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = Who->GetControlRotation().Vector();
	FVector P = Who->GetActorLocation() + FVector(0.f, 0.f, 55.f) + Dir * 45.f;
	FVector V = Dir * Pot->ProjectileSpeed;
	const float G = 980.f * Pot->ProjectileGravity;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRThrowArc), false, Who);
	const float Step = 0.04f;
	int32 Dots = 0;
	for (float T = 0.f; T < 3.f; T += Step)
	{
		const FVector N = P + V * Step + FVector(0.f, 0.f, -0.5f * G * Step * Step);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, P, N, ECC_Visibility, Q))
		{
			WRFx::SpawnAt(Who, TEXT("raid.arc_land"), Hit.ImpactPoint + Hit.ImpactNormal * 3.f, Hit.ImpactNormal.Rotation(), 1.f);
			break;
		}
		V.Z -= G * Step;
		P = N;
		if (++Dots % 2 == 0) WRFx::SpawnAt(Who, TEXT("raid.arc"), P, FRotator::ZeroRotator, 1.f);
	}
}

AWRBuildPiece* FWRRaiding::PieceInSights(const AWRCharacterBase* Who, float Range, FVector* OutPoint)
{
	const AWRCharacter* Survivor = Cast<AWRCharacter>(Who);
	if (!Survivor) return nullptr;
	FVector Point;
	if (!Survivor->CrosshairPoint(Range, false, Point)) return nullptr;
	if (OutPoint) *OutPoint = Point;
	// What's at the point the crosshair met (a small probe: the trace stops on the face).
	TArray<FOverlapResult> Over;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRRaidSights), false, Who);
	Who->GetWorld()->OverlapMultiByChannel(Over, Point, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.f), Q);
	AWRBuildPiece* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (const FOverlapResult& O : Over)
	{
		if (AWRBuildPiece* P = PieceOf(O.GetActor()); P && !P->bGhost)
		{
			const float D = FVector::DistSquared(P->GetActorLocation(), Point);
			if (D < BestD) { BestD = D; Best = P; }
		}
	}
	return Best;
}

int32 FWRRaiding::RaidGroupMax()
{
	return FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.raidGroupMax"), 0.f)), 0, 100);
}

int32 FWRRaiding::RaidGroupTuned()
{
	return FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.raidGroupTuned"), 8.f)), 1, 100);
}

// ---------------------------------------------------------------- tools

FString FWRRaiding::UseItem(AWRPlayerState* PS, FName ItemId)
{
	using namespace WRRaidingImpl;
	AWRCharacterBase* Who = PS ? Cast<AWRCharacterBase>(PS->GetPawn()) : nullptr;
	UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (!Who || !Who->IsAlive() || !Bag) return TEXT("Not now");
	UWorld* World = Who->GetWorld();
	const FString Me = PS->GetPlayerName();
	const FString Key = ItemId.ToString();

	if (ItemId == TEXT("powder_charge"))
	{
		FHitResult Hit;
		AWRBuildPiece* Piece = TraceInFront(Who, Num(TEXT("charge.reachCm"), 300.f), Hit) ? PieceOf(Hit.GetActor()) : nullptr;
		if (!Piece) return TEXT("Press it against a wall, door, floor or the Hearth of a building");
		FString Why;
		if (!MayRaid(Piece, Me, &Why)) return Why;
		const FRotator Facing = FRotationMatrix::MakeFromX(Hit.ImpactNormal).Rotator();   // out of the face (a wall, a floor, a ceiling)
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWRRaidCharge* Charge = World->SpawnActor<AWRRaidCharge>(AWRRaidCharge::StaticClass(), Hit.ImpactPoint, Facing, P);
		if (!Charge) return TEXT("Couldn't place it");
		Charge->Kind = ItemId;
		Charge->FuseSeconds = Num(TEXT("charge.fuseSeconds"), 5.f);
		Charge->LitAt = ServerNow(World);
		Charge->OnPiece = Piece;
		Charge->Placer = Me;
		Charge->PlacerBody = Who;
		Charge->AttachToActor(Piece, FAttachmentTransformRules::KeepWorldTransform);
		Bag->RemoveItem(ItemId, 1);
		UE_LOG(LogWildreach, Log, TEXT("RAIDING %s set a petard on %s (%s)"), *Me, *Piece->GetName(), *Piece->PieceId.ToString());
		return FString::Printf(TEXT("Petard lit: %.0f s. Get clear!"), Charge->FuseSeconds);
	}
	if (ItemId == TEXT("mining_charge"))
	{
		FHitResult Hit;
		AWRResourceNode* Node = TraceInFront(Who, Num(TEXT("miningCharge.reachCm"), 300.f), Hit) ? Cast<AWRResourceNode>(Hit.GetActor()) : nullptr;
		if (!Node || Node->IsDepleted() || FWRGathering::ToolKindFor(Node->ItemId) != TEXT("pick"))
		{
			return TEXT("Tamp it into an ore vein, sulfur, nitre or a rock");
		}
		const FRotator Facing = FRotationMatrix::MakeFromX(Hit.ImpactNormal).Rotator();   // out of the face (a wall, a floor, a ceiling)
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWRRaidCharge* Charge = World->SpawnActor<AWRRaidCharge>(AWRRaidCharge::StaticClass(), Hit.ImpactPoint, Facing, P);
		if (!Charge) return TEXT("Couldn't place it");
		Charge->Kind = ItemId;
		Charge->FuseSeconds = Num(TEXT("miningCharge.fuseSeconds"), 4.f);
		Charge->LitAt = ServerNow(World);
		Charge->OnNode = Node;
		Charge->Placer = Me;
		Charge->PlacerBody = Who;
		Bag->RemoveItem(ItemId, 1);
		return FString::Printf(TEXT("Mining charge lit: %.0f s"), Charge->FuseSeconds);
	}
	if (ItemId == TEXT("siege_ladder"))
	{
		FHitResult Hit;
		AWRBuildPiece* Piece = TraceInFront(Who, Num(TEXT("ladder.reachCm"), 300.f), Hit) ? PieceOf(Hit.GetActor()) : nullptr;
		const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
		if (!Def || !Def->Snaps() || FMath::Abs(Hit.ImpactNormal.Z) > 0.35f)
		{
			return TEXT("Lean it on a wall, palisade or gate");
		}
		const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
		const float FootOut = Num(TEXT("ladder.footOutCm"), 160.f);
		const float TopZ = PieceBox(Piece).Max.Z;
		// The foot: out from the wall's face, on whatever ground (or floor) is there.
		const FVector FaceAt(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, TopZ);
		const FVector FootXY = FaceAt + Normal * FootOut;
		FHitResult Ground;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRLadderFoot), false, Who);
		if (!World->LineTraceSingleByChannel(Ground, FVector(FootXY.X, FootXY.Y, Who->GetActorLocation().Z + 100.f), FVector(FootXY.X, FootXY.Y, Who->GetActorLocation().Z - 600.f), ECC_Visibility, Q))
		{
			return TEXT("No ground for its feet there");
		}
		const float Rise = TopZ - Ground.ImpactPoint.Z;
		if (Rise > Num(TEXT("ladder.heightCm"), 600.f)) return TEXT("Too tall for a ladder (6 m)");
		if (Rise < Num(TEXT("ladder.minHeightCm"), 200.f)) return TEXT("Low enough to climb without one");
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWRSiegeLadder* Ladder = World->SpawnActor<AWRSiegeLadder>(AWRSiegeLadder::StaticClass(), Ground.ImpactPoint, Normal.Rotation(), P);
		if (!Ladder) return TEXT("Couldn't stand it there");
		Ladder->FootOut = FootOut;
		Ladder->Length = FMath::Sqrt(Rise * Rise + FootOut * FootOut);
		Ladder->Health = Num(TEXT("ladder.hp"), 300.f);
		Ladder->Against = Piece;
		Ladder->Placer = Me;
		Ladder->DieAt = World->GetTimeSeconds() + Num(TEXT("ladder.lifeSeconds"), 600.f);
		WRFx::Broadcast(Ladder, TEXT("gather.wood"), Ground.ImpactPoint, FRotator::ZeroRotator, 1.f, TEXT("raid.ladder"));
		Bag->RemoveItem(ItemId, 1);
		UE_LOG(LogWildreach, Log, TEXT("RAIDING %s stood a %.0f cm ladder against %s"), *Me, Ladder->Length, *Piece->GetName());
		return TEXT("Ladder up: walk into it to climb");
	}
	if (ItemId == TEXT("code_breaker"))
	{
		FHitResult Hit;
		AWRBuildPiece* Door = TraceInFront(Who, Num(TEXT("codeBreaker.reachCm"), 200.f), Hit) ? PieceOf(Hit.GetActor()) : nullptr;
		const FWRPieceDef* Def = Door ? Door->GetDef() : nullptr;
		if (!Def || Def->Kind != TEXT("door") || !Door->bLocked) return TEXT("Hold it to a door's code lock");
		if (Door->CanAccess(Me) || Door->UnlockedFor.Contains(Me)) return TEXT("That lock already opens for you");
		FString Why;
		if (!MayRaid(Door, Me, &Why)) return Why;
		for (const FBreak& B : Breaks) if (B.PC.Get() == Cast<AWRPlayerController>(PS->GetOwner())) return TEXT("Already at it");
		FBreak& B = Breaks.AddDefaulted_GetRef();
		B.PC = Cast<AWRPlayerController>(PS->GetOwner());
		B.Door = Door;
		B.Start = World->GetTimeSeconds();
		B.At = Who->GetActorLocation();
		B.Health = HealthOf(Who);
		B.NextRing = B.Start;
		// The owners hear it begin: their bell rings and they're told.
		NoteHit(Door, Me, false, false);
		UE_LOG(LogWildreach, Log, TEXT("RAIDING %s started a code breaker on %s"), *Me, *Door->GetName());
		return FString::Printf(TEXT("The code breaker hums against the lock: hold still %.0f s"), Num(TEXT("codeBreaker.seconds"), 60.f));
	}
	return FString::Printf(TEXT("%s can't be used like that"), *Key);
}

void FWRRaiding::Step(UWorld* World)
{
	using namespace WRRaidingImpl;
	if (!World) return;
	const double Now = World->GetTimeSeconds();
	const float Need = Num(TEXT("codeBreaker.seconds"), 60.f);
	for (int32 i = Breaks.Num() - 1; i >= 0; --i)
	{
		FBreak& B = Breaks[i];
		AWRPlayerController* PC = B.PC.Get();
		AWRCharacterBase* Who = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
		AWRBuildPiece* Door = B.Door.Get();
		FString Stop;
		if (!Who || !Who->IsAlive()) Stop = TEXT("The code breaker fell silent");
		else if (!Door || Door->IsActorBeingDestroyed()) Stop = TEXT("The door's gone");
		else if (FVector::Dist2D(Who->GetActorLocation(), B.At) > 60.f) Stop = TEXT("You moved: the code breaker lost the tumblers");
		else if (HealthOf(Who) < B.Health - 0.5f) Stop = TEXT("Hit: the code breaker slipped off the lock");
		if (!Stop.IsEmpty())
		{
			if (PC) PC->ClientNotify(Stop);
			Breaks.RemoveAt(i);
			continue;
		}
		if (Now >= B.NextRing)
		{
			// A beating, ringing tone that carries 40 m (raid.codebreaker); the keypad glints.
			WRFx::Broadcast(Door, TEXT("raid.codebreaker"), Who->GetActorLocation() + Who->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, 1.f, TEXT("raid.codebreaker"));
			B.NextRing = Now + Num(TEXT("codeBreaker.ringSeconds"), 4.f);
		}
		const float Left = Need - float(Now - B.Start);
		const int32 Step15 = FMath::FloorToInt(float(Now - B.Start) / 15.f);
		if (Step15 > B.Said && Left > 1.f)
		{
			B.Said = Step15;
			PC->ClientNotify(FString::Printf(TEXT("Code breaker: %.0f s"), Left));
		}
		if (Left <= 0.f)
		{
			Door->UnlockedFor.AddUnique(PC->PlayerState->GetPlayerName());
			Door->SetOpen(true);
			Door->ForceNetUpdate();
			if (UWRInventoryComponent* Bag = UWRInventoryComponent::FindFor(Who)) Bag->RemoveItem(TEXT("code_breaker"), 1);
			NoteHit(Door, PC->PlayerState->GetPlayerName(), false, false);
			PC->ClientNotify(TEXT("The lock gives: the door swings open"));
			UE_LOG(LogWildreach, Log, TEXT("RAIDING %s broke the code of %s"), *PC->PlayerState->GetPlayerName(), *Door->GetName());
			Breaks.RemoveAt(i);
		}
	}
	// Raids end after a quiet spell; the record is saved with the world.
	if (bDirty) Save();
}

void FWRRaiding::FirePotBurst(AWRProjectile* Pot, const FVector& At, AWRCharacterBase* Thrower)
{
	using namespace WRRaidingImpl;
	UWorld* World = Pot ? Pot->GetWorld() : nullptr;
	if (!World) return;
	const FString Raider = Responsible(Thrower);
	const float Radius = Num(TEXT("firePot.radiusCm"), 200.f);
	// The jar breaks (terracotta shards, a whoomp of fire) and spreads its burning fat.
	WRFx::Broadcast(Pot, TEXT("raid.pot"), At, FRotator(90.f, 0.f, 0.f), 1.f, TEXT("raid.pot_break"));
	Burst(World, Pot, Thrower, At, Radius, Num(TEXT("firePot.people"), 40.f), 0.6f, Num(TEXT("firePot.burnBuildup"), 60.f), Raider);
	// Pieces in the splash take the jar's impact (wood 150 x their share; stone a little; metal nothing).
	SplashPieces(World, At, Radius, Num(TEXT("firePot.impact"), 150.f), EWRRaidSource::FirePot, Pot, Raider, NAME_None);
	// The burning ground: hurts whoever stands in it (AWRHazard) and burns the wood it touches (FWRRaiding fire).
	FHitResult Ground;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRPotGround), false, Pot);
	const FVector Floor = World->LineTraceSingleByChannel(Ground, At + FVector(0.f, 0.f, 50.f), At - FVector(0.f, 0.f, 400.f), ECC_Visibility, Q) ? FVector(Ground.ImpactPoint) : At;
	const float Seconds = Num(TEXT("firePot.burnSeconds"), 10.f);
	if (AWRHazard* Fire = AWRHazard::Spawn(World, Thrower, Floor, Radius, Seconds, Num(TEXT("firePot.burnDps"), 30.f), TEXT("fire"), false))
	{
		Fire->bBurnsPieces = true;
		Fire->Raider = Raider;
	}
	WRFx::Broadcast(Pot, TEXT("firewall"), Floor, FRotator::ZeroRotator, Radius / 100.f, NAME_None, Seconds);
	UE_LOG(LogWildreach, Log, TEXT("RAIDING fire pot by %s burst at %s"), *Raider, *At.ToCompactString());
}

FString FWRRaiding::OrderTamesToBreak(AWRPlayerController* PC)
{
	using namespace WRRaidingImpl;
	AWRCharacterBase* Me = PC ? Cast<AWRCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Me || !PC->PlayerState) return TEXT("Not now");
	const FString Name = PC->PlayerState->GetPlayerName();
	AWRBuildPiece* Piece = PieceInSights(Me, Num(TEXT("tames.sightCm"), 6000.f));
	if (!Piece) return TEXT("Look at a building to send them at");
	FString Why;
	if (!MayRaid(Piece, Name, &Why)) return Why;
	const float Range = WRTune(TEXT("taming.whistleRange"), 3000.f);
	int32 Count = 0;
	for (TActorIterator<AWRCreature> It(PC->GetWorld()); It; ++It)
	{
		AWRCreature* Tame = *It;
		if (!Tame->IsAlive() || Tame->TamedBy != Name || Tame->GetRider() || FVector::Dist(Tame->GetActorLocation(), Me->GetActorLocation()) >= Range) continue;
		const FWRCreatureDef* Def = Tame->GetDef();
		if (!Def || Def->Damage <= 0.f) continue;   // grazers don't bite walls
		Tame->SiegePiece = Piece;
		if (Tame->IsAsleep()) Tame->SetAsleep(false);
		++Count;
	}
	const FWRPieceDef* Def = Piece->GetDef();
	UE_LOG(LogWildreach, Log, TEXT("RAIDING %s sent %d tames at %s"), *Name, Count, *Piece->GetName());
	return Count == 0 ? FString(TEXT("No tames of yours in earshot that bite"))
		: FString::Printf(TEXT("%d %s go for the %s"), Count, Count == 1 ? TEXT("tame") : TEXT("tames"), Def ? *Def->Name.ToLower() : TEXT("building"));
}

// ---------------------------------------------------------------- the record

void FWRRaiding::PlayerJoined(AWRPlayerController* PC)
{
	using namespace WRRaidingImpl;
	if (!PC || !PC->PlayerState) return;
	const FString Me = PC->PlayerState->GetPlayerName();
	int32 Lines = 0;
	TArray<FString> Said;
	for (FRaid& R : Raids)
	{
		if (R.Told.Contains(Me) || !FWRSocialRules::CanShare(R.Owner, Me)) continue;
		R.Told.Add(Me);
		bDirty = true;
		const FDateTime S = FDateTime::FromUnixTimestamp(R.Start), E = FDateTime::FromUnixTimestamp(R.End);
		FString Lost = R.Broken > 0 ? FString::Printf(TEXT("%d %s broken"), R.Broken, R.Broken == 1 ? TEXT("piece") : TEXT("pieces")) : FString(TEXT("nothing broken"));
		if (R.bHearth) Lost += TEXT(", the Hearth broken");
		// (Times in UTC: the server's clock; the card says so.)
		if (++Lines <= 3)
		{
			Said.Add(FString::Printf(TEXT("%s was attacked %s to %s UTC by %s: %s."), *R.Name, *S.ToString(TEXT("%d %b %H:%M")), *E.ToString(TEXT("%H:%M")), *WhoList(R.Raiders), *Lost));
		}
	}
	if (Said.Num() == 0) return;
	// One card once the island has streamed in round them (the loading cover would hide it): raiding.awayCardSeconds.
	const FString Text = FString::Join(Said, TEXT(" "));
	TWeakObjectPtr<AWRPlayerController> Weak = PC;
	FTimerHandle Later;
	PC->GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda([Weak, Text]()
	{
		if (AWRPlayerController* P = Weak.Get()) P->ClientRaid(NAME_None, TEXT("While you were away"), Text, 0);
	}), FMath::Max(1.f, Num(TEXT("awayCardSeconds"), 10.f)), false);
}

void FWRRaiding::Save()
{
	using namespace WRRaidingImpl;
	bDirty = false;
	TArray<TSharedPtr<FJsonValue>> List;
	for (const FRaid& R : Raids)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("claim"), R.Claim);
		O->SetStringField(TEXT("owner"), R.Owner);
		O->SetStringField(TEXT("name"), R.Name);
		O->SetNumberField(TEXT("start"), double(R.Start));
		O->SetNumberField(TEXT("end"), double(R.End));
		O->SetNumberField(TEXT("broken"), R.Broken);
		O->SetBoolField(TEXT("hearth"), R.bHearth);
		TArray<TSharedPtr<FJsonValue>> Who, Told;
		for (const FString& S : R.Raiders) Who.Add(MakeShared<FJsonValueString>(S));
		for (const FString& S : R.Told) Told.Add(MakeShared<FJsonValueString>(S));
		O->SetArrayField(TEXT("raiders"), Who);
		O->SetArrayField(TEXT("told"), Told);
		List.Add(MakeShared<FJsonValueObject>(O));
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetArrayField(TEXT("raids"), List);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	WRStore::WriteDoc(TEXT("raid"), Text);
}

void FWRRaiding::Load(UWorld* World)
{
	using namespace WRRaidingImpl;
	Raids.Reset();
	LastAlert.Reset();
	Breaks.Reset();
	bDirty = false;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!WRStore::ReadDoc(TEXT("raid"), Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("raids"), List)) return;
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FRaid& R = Raids.AddDefaulted_GetRef();
		R.Claim = O->GetStringField(TEXT("claim"));
		R.Owner = O->GetStringField(TEXT("owner"));
		R.Name = O->GetStringField(TEXT("name"));
		R.Start = int64(O->GetNumberField(TEXT("start")));
		R.End = int64(O->GetNumberField(TEXT("end")));
		R.Broken = int32(O->GetNumberField(TEXT("broken")));
		R.bHearth = O->GetBoolField(TEXT("hearth"));
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(TEXT("raiders"), A)) for (const TSharedPtr<FJsonValue>& S : *A) R.Raiders.Add(S->AsString());
		if (O->TryGetArrayField(TEXT("told"), A)) for (const TSharedPtr<FJsonValue>& S : *A) R.Told.Add(S->AsString());
	}
	UE_LOG(LogWildreach, Log, TEXT("RAIDING record: %d raids"), Raids.Num());
}

// ---------------------------------------------------------------- the charge

AWRRaidCharge::AWRRaidCharge()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AWRRaidCharge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRRaidCharge, Kind);
	DOREPLIFETIME(AWRRaidCharge, LitAt);
	DOREPLIFETIME(AWRRaidCharge, FuseSeconds);
}

void AWRRaidCharge::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Kind();
}

void AWRRaidCharge::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Hiss) Hiss->Stop();
	Super::EndPlay(Reason);
}

void AWRRaidCharge::OnRep_Kind()
{
	if (bBuilt || Kind.IsNone() || GetNetMode() == NM_DedicatedServer) return;
	bBuilt = true;
	Body = NewObject<UProceduralMeshComponent>(this, TEXT("Charge"));
	Body->SetupAttachment(Root);
	Body->RegisterComponent();
	(Kind == TEXT("mining_charge") ? WRRaidLook::MiningCartridge() : WRRaidLook::Petard()).ApplyTo(Body);
	// The match's glowing end lights the plank as it creeps (a small warm light that moves with it).
	Glow = NewObject<UPointLightComponent>(this, TEXT("MatchGlow"));
	Glow->SetupAttachment(Root);
	Glow->RegisterComponent();
	Glow->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Glow->SetAttenuationRadius(160.f);
	Glow->SetIntensity(WRTune(TEXT("raiding.charge.glow"), 900.f));
	Glow->SetCastShadows(false);
	Hiss = WRSound::StartLoop(this, TEXT("raid.fuse"));
}

float AWRRaidCharge::FuseLeft() const
{
	return FuseSeconds - (WRRaidingImpl::ServerNow(GetWorld()) - LitAt);
}

FVector AWRRaidCharge::MatchPoint() const
{
	// The petard's match burns from its coil toward the touch hole; the mining charge's short fuse into its plug.
	const float T = FMath::Clamp(1.f - FuseLeft() / FMath::Max(0.1f, FuseSeconds), 0.f, 1.f);
	const TArray<FVector> Cord = Kind == TEXT("mining_charge")
		? TArray<FVector>{ FVector(17.f, 4.f, -3.f), FVector(15.f, 3.f, 1.f), FVector(12.f, 1.f, 3.f), FVector(8.f, 0.f, 0.f) }
		: WRRaidLook::MatchCord();
	float Total = 0.f;
	for (int32 i = 1; i < Cord.Num(); ++i) Total += FVector::Dist(Cord[i - 1], Cord[i]);
	float Want = T * Total;
	FVector Local = Cord.Last();
	for (int32 i = 1; i < Cord.Num(); ++i)
	{
		const float Seg = FVector::Dist(Cord[i - 1], Cord[i]);
		if (Want <= Seg) { Local = FMath::Lerp(Cord[i - 1], Cord[i], Seg > 0.f ? Want / Seg : 0.f); break; }
		Want -= Seg;
	}
	return GetActorTransform().TransformPosition(Local);
}

void AWRRaidCharge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer && Glow)
	{
		// The burning end: sparks spitting off it every few hundredths of a second, its glow flickering.
		const FVector At = MatchPoint();
		Glow->SetWorldLocation(At + GetActorForwardVector() * 4.f);
		Glow->SetIntensity(WRTune(TEXT("raiding.charge.glow"), 900.f) * FMath::FRandRange(0.7f, 1.15f));
		SparkClock -= DeltaSeconds;
		if (SparkClock <= 0.f)
		{
			SparkClock = 0.06f;
			WRFx::SpawnAt(this, TEXT("raid.fuse"), At, GetActorRotation(), 1.f);
		}
	}
	if (!HasAuthority()) return;
	// A charge on a piece that broke under it drops (and still goes off where it lands).
	if (Kind == TEXT("powder_charge") && !bFalling && (!OnPiece.IsValid() || OnPiece->IsActorBeingDestroyed()))
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bFalling = true;
	}
	if (bFalling)
	{
		FallVelocity.Z -= 980.f * DeltaSeconds;
		const FVector From = GetActorLocation(), To = From + FallVelocity * DeltaSeconds;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRChargeFall), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
		{
			// Down on whatever's below, lying on its plank (the bell up).
			bFalling = false;
			FallVelocity = FVector::ZeroVector;
			SetActorLocation(Hit.ImpactPoint);
			SetActorRotation(FRotationMatrix::MakeFromX(Hit.ImpactNormal).Rotator());
		}
		else
		{
			SetActorLocation(To);
		}
	}
	if (FuseLeft() <= 0.f)
	{
		Explode();
	}
}

void AWRRaidCharge::Explode()
{
	using namespace WRRaidingImpl;
	UWorld* World = GetWorld();
	const FVector At = GetActorLocation() + GetActorForwardVector() * 10.f;
	if (Kind == TEXT("mining_charge"))
	{
		// The vein bursts: everything it had left, three times over, lies in a heap where it stood (WRResourceNode).
		if (AWRResourceNode* Node = OnNode.Get())
		{
			Node->Blast(PlacerBody.Get(), Placer, Num(TEXT("miningCharge.yieldMult"), 3.f));
		}
		WRFx::Broadcast(this, TEXT("raid.blast_small"), At, GetActorRotation(), 1.f, TEXT("raid.blast_small"));
		Burst(World, this, PlacerBody.Get(), At, Num(TEXT("miningCharge.radiusCm"), 200.f), Num(TEXT("miningCharge.people"), 40.f), 0.5f, 0.f, Placer);
		UE_LOG(LogWildreach, Log, TEXT("RAIDING mining charge of %s went off"), *Placer);
		Destroy();
		return;
	}
	const float Radius = Num(TEXT("charge.radiusCm"), 400.f);
	// The piece it's on takes the full blast for its material; every other piece in reach of the blast takes a share,
	// so a wall beside the target cracks but the cost table holds wall by wall (cooks/build-server-raids.md 2.4).
	const auto Amount = [](const AWRBuildPiece* P)
	{
		const FName Cat = FWRRaiding::Category(P);
		const FName Row = Cat == TEXT("other") ? FName(TEXT("wood")) : Cat;
		return Num(FString::Printf(TEXT("charge.%s"), *Row.ToString()), 600.f);
	};
	AWRBuildPiece* Target = OnPiece.Get();
	TArray<AWRBuildPiece*> Near;
	for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
	{
		if (*It == Target || It->bGhost || It->IsActorBeingDestroyed()) continue;
		if (FVector::DistSquared(PieceBox(*It).GetClosestPointTo(At), At) <= Radius * Radius) Near.Add(*It);
	}
	if (Target && !Target->IsActorBeingDestroyed())
	{
		FWRRaiding::DamagePiece(Target, Amount(Target), EWRRaidSource::Blast, this, Placer, NAME_None);
	}
	const float Share = Num(TEXT("charge.neighbourShare"), 0.5f);
	for (AWRBuildPiece* P : Near)
	{
		if (IsValid(P) && !P->IsActorBeingDestroyed()) FWRRaiding::DamagePiece(P, Amount(P) * Share, EWRRaidSource::Blast, this, Placer, NAME_None);
	}
	// A siege engine in the blast takes the charge's wood number (they're timber).
	FWRSiege::Splash(World, At, Radius, Num(TEXT("charge.wood"), 800.f) * Share, EWRRaidSource::Blast, this, Placer);
	// Silent charges (a stealth perk): the same blast, heard over a much shorter reach.
	const bool bQuiet = PlacerBody.IsValid() && UWRLifeSkillsComponent::StatFor(PlacerBody.Get(), TEXT("silentCharges")) > 0.f;
	WRFx::Broadcast(this, TEXT("raid.blast"), At, GetActorRotation(), 1.f, bQuiet ? FName(TEXT("raid.blast_quiet")) : FName(TEXT("raid.blast")));
	Burst(World, this, PlacerBody.Get(), At, Radius, Num(TEXT("charge.people"), 120.f), Num(TEXT("charge.edgeShare"), 0.5f), 0.f, Placer);
	UE_LOG(LogWildreach, Log, TEXT("RAIDING petard of %s went off (%d pieces near)"), *Placer, Near.Num());
	Destroy();
}

// ---------------------------------------------------------------- the ladder

AWRSiegeLadder::AWRSiegeLadder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// Thin along the rails: blows land on it (the melee sweep's Pawn channel) and you bump its rails from the side,
	// while the climbing lane in front of the rungs stays clear.
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Rails"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Blocker->SetCollisionObjectType(ECC_WorldDynamic);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Blocker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Blocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AWRSiegeLadder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSiegeLadder, Length);
	DOREPLIFETIME(AWRSiegeLadder, FootOut);
	DOREPLIFETIME(AWRSiegeLadder, Health);
}

FVector AWRSiegeLadder::Up() const
{
	const float Rise = FMath::Sqrt(FMath::Max(1.f, Length * Length - FootOut * FootOut));
	return GetActorTransform().TransformVectorNoScale(FVector(-FootOut, 0.f, Rise).GetSafeNormal());
}

FVector AWRSiegeLadder::Out() const
{
	return GetActorForwardVector();
}

bool AWRSiegeLadder::OnLane(const FVector& At, float Radius, float& OutAlong) const
{
	const FVector Rel = At - Foot();
	const FVector U = Up();
	const FVector Side = GetActorRightVector();
	const FVector N = FVector::CrossProduct(Side, U).GetSafeNormal();   // out of the ladder's face, toward the climber
	const float Along = FVector::DotProduct(Rel, U);
	const float Lateral = FVector::DotProduct(Rel, Side);
	const float Off = FVector::DotProduct(Rel, N);
	OutAlong = Along / FMath::Max(1.f, Length);
	return OutAlong > -0.15f && OutAlong < 1.08f && FMath::Abs(Lateral) < 45.f && Off > -10.f && Off < Radius + 70.f;
}

void AWRSiegeLadder::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Shape();
}

void AWRSiegeLadder::OnRep_Shape()
{
	Build();
}

void AWRSiegeLadder::Build()
{
	const float Rise = FMath::Sqrt(FMath::Max(1.f, Length * Length - FootOut * FootOut));
	const FVector LocalUp = FVector(-FootOut, 0.f, Rise).GetSafeNormal();
	// The rails' box: along the ladder, 52 cm across the rails, 8 deep.
	Blocker->SetBoxExtent(FVector(4.f, 26.f, Length * 0.5f));
	Blocker->SetRelativeLocation(LocalUp * Length * 0.5f);
	Blocker->SetRelativeRotation(FRotationMatrix::MakeFromZX(LocalUp, FVector::XAxisVector).Rotator());
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!Body)
	{
		Body = NewObject<UProceduralMeshComponent>(this, TEXT("Ladder"));
		Body->SetupAttachment(Root);
		Body->RegisterComponent();
	}
	WRRaidLook::Ladder(Length, FootOut).ApplyTo(Body);
}

void AWRSiegeLadder::TakeHit(float Amount, AActor* By)
{
	if (!HasAuthority() || Amount <= 0.f) return;
	Health -= Amount;
	WRFx::Broadcast(this, TEXT("gather.wood"), GetActorLocation() + Up() * Length * 0.4f, FRotator::ZeroRotator, 1.f, TEXT("gather.wood"));
	if (Health <= 0.f)
	{
		UE_LOG(LogWildreach, Log, TEXT("RAIDING ladder of %s broken by %s"), *Placer, *GetNameSafe(By));
		WRFx::Broadcast(this, TEXT("gather.wood"), GetActorLocation() + Up() * Length * 0.5f, FRotator::ZeroRotator, 2.f, TEXT("raid.ladder"));
		Destroy();
	}
}

void AWRSiegeLadder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	// It goes with the wall it leans on, and after its time (the lashings work loose).
	const AWRBuildPiece* Wall = Against.Get();
	if (!Wall || Wall->IsActorBeingDestroyed() || GetWorld()->GetTimeSeconds() >= DieAt)
	{
		WRFx::Broadcast(this, TEXT("gather.wood"), GetActorLocation() + Up() * Length * 0.5f, FRotator::ZeroRotator, 1.5f, TEXT("raid.ladder"));
		Destroy();
	}
}

// ---------------------------------------------------------------- the bell

UWRRaidBellComponent::UWRRaidBellComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

void UWRRaidBellComponent::Hang(float Height, float PostRadius)
{
	const float Reach = WRTune(TEXT("raiding.bell.reachCm"), 34.f);
	SetRelativeLocation(FVector(0.f, 0.f, Height));
	Bracket = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("BellBracket"));
	Bracket->SetupAttachment(this);
	Bracket->RegisterComponent();
	WRRaidLook::Bracket(PostRadius, Reach + PostRadius).ApplyTo(Bracket);
	Pivot = NewObject<USceneComponent>(GetOwner(), TEXT("BellPivot"));
	Pivot->SetupAttachment(this);
	Pivot->RegisterComponent();
	Pivot->SetRelativeLocation(FVector(Reach + PostRadius, 0.f, -6.f));
	Bell = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("Bell"));
	Bell->SetupAttachment(Pivot);
	Bell->RegisterComponent();
	WRRaidLook::Bell().ApplyTo(Bell);
	ClapperPivot = NewObject<USceneComponent>(GetOwner(), TEXT("ClapperPivot"));
	ClapperPivot->SetupAttachment(Pivot);
	ClapperPivot->RegisterComponent();
	ClapperPivot->SetRelativeLocation(FVector(0.f, 0.f, -8.f));
	Clapper = NewObject<UProceduralMeshComponent>(GetOwner(), TEXT("Clapper"));
	Clapper->SetupAttachment(ClapperPivot);
	Clapper->RegisterComponent();
	WRRaidLook::Clapper().ApplyTo(Clapper);
}

void UWRRaidBellComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AWRClaimTotem* Totem = Cast<AWRClaimTotem>(GetOwner());
	if (!Totem || !Pivot) return;
	// A damped pendulum from the last strike: swing deg, period s, gone in decay s. The clapper is a second pendulum
	// hung inside, a little longer and lagging, so it meets the sound bow at the ends of the swing. Pure functions of
	// time since the strike: every machine draws the same swing and nothing snaps (rest is angle 0).
	const float T = WRRaidingImpl::ServerNow(GetWorld()) - Totem->BellRungAt;
	const float Decay = FMath::Max(0.1f, WRTune(TEXT("raiding.bell.decaySeconds"), 4.f));
	if (T < 0.f || T > Decay * 1.5f)
	{
		if (!Pivot->GetRelativeRotation().IsNearlyZero(0.01f))
		{
			Pivot->SetRelativeRotation(FRotator::ZeroRotator);
			ClapperPivot->SetRelativeRotation(FRotator::ZeroRotator);
		}
		return;
	}
	const float Swing = WRTune(TEXT("raiding.bell.swingDeg"), 18.f);
	const float Period = FMath::Max(0.2f, WRTune(TEXT("raiding.bell.periodSeconds"), 1.1f));
	const float Env = FMath::Exp(-3.f * T / Decay) * FMath::Min(1.f, T / 0.08f);   // a quick start, not a jump
	const float BellAngle = Swing * Env * FMath::Sin(2.f * PI * T / Period);
	const float Clap = Swing * 1.25f * Env * FMath::Sin(2.f * PI * T / (Period * 1.08f) - 0.55f);
	// The swing is across the bracket's arm (about the pin's Y axis): pitch.
	Pivot->SetRelativeRotation(FRotator(BellAngle, 0.f, 0.f));
	ClapperPivot->SetRelativeRotation(FRotator(FMath::Clamp(Clap - BellAngle, -14.f, 14.f), 0.f, 0.f));
}
