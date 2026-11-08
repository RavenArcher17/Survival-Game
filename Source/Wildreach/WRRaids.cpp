#include "WRRaids.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRPlayerController.h"
#include "WRSocial.h"
#include "WRTuning.h"
#include "WRWater.h"
#include "WRWildlife.h"
#include "WRWorlds.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "LandscapeHeightfieldCollisionComponent.h"

namespace WRRaidsImpl
{
	enum class EStage : uint8 { Warned, On, Leaving };

	struct FRaid
	{
		TWeakObjectPtr<AWRClaimTotem> Claim;
		FString Owner;
		FVector Base = FVector::ZeroVector;   // the totem
		float Radius = 3000.f;                // the claim's
		FVector From = FVector::ZeroVector;   // where they come from, on the ground
		FString Way;                          // "north-east": from the base
		FName Region;
		int32 HearthLevel = 0;
		int32 Points = 0;
		TArray<TPair<FName, int32>> Kinds;
		TArray<TWeakObjectPtr<AWRCreature>> Raiders;
		EStage Stage = EStage::Warned;
		double ComeAt = 0.0;   // world seconds
		double EndAt = 0.0;
		double GoneAt = 0.0;
		int32 PiecesBefore = 0;
	};

	struct FState
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FRaid> Raids;
		TSet<TWeakObjectPtr<AWRClaimTotem>> Waiting;   // due, nobody of it home yet (logged once)
	} G;

	double NowHours(const UWorld* World)
	{
		const AWRGameState* State = AWRGameState::Get(World);
		return State ? State->GetTotalHours() : 0.0;
	}

	/** One interval, give or take raid.jitter. */
	double NextInterval(double Every)
	{
		const double Jitter = FMath::Clamp(WRTune(TEXT("raid.jitter"), 0.25f), 0.f, 0.9f);
		return Every * FMath::FRandRange(1.0 - Jitter, 1.0 + Jitter);
	}

	bool IsMember(const AWRClaimTotem* Claim, const FString& Player)
	{
		return !Player.IsEmpty() && FWRSocialRules::CanShare(Claim->OwnerName, Player);
	}

	/** Someone of the claim is alive within raid.homeRange of its totem. */
	bool MemberHome(UWorld* World, const AWRClaimTotem* Claim)
	{
		const float Range = WRTune(TEXT("raid.homeRange"), 25000.f);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (Pawn && PC->PlayerState && IsMember(Claim, PC->PlayerState->GetPlayerName())
				&& FVector::Dist2D(Pawn->GetActorLocation(), Claim->GetActorLocation()) <= Range)
			{
				return true;
			}
		}
		return false;
	}

	bool PlayerWithin(UWorld* World, const FVector& At, float Range)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(), At) <= Range) return true;
		}
		return false;
	}

	int32 CountPieces(UWorld* World, const FString& Owner, const FVector& Base, float Radius)
	{
		int32 Count = 0;
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			if (!It->bGhost && !It->IsActorBeingDestroyed() && FWRSocialRules::CanShare(It->OwnerName, Owner)
				&& FVector::Dist2D(It->GetActorLocation(), Base) <= Radius + 1500.f)
			{
				++Count;
			}
		}
		return Count;
	}

	/** The island's ground under a spot (the landscape, not a rock or a building on it). */
	bool GroundAt(UWorld* World, const FVector2D& At, float AroundZ, FVector& Out)
	{
		TArray<FHitResult> Hits;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(WRRaidGround), false);
		World->LineTraceMultiByObjectType(Hits, FVector(At.X, At.Y, AroundZ + 30000.f), FVector(At.X, At.Y, AroundZ - 30000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Query);
		for (const FHitResult& Hit : Hits)
		{
			if (Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent()))
			{
				Out = Hit.ImpactPoint;
				const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(World);
				return !Water || Water->DepthAt(Out) < 40.f;   // not in the sea
			}
		}
		return false;
	}

	/** "north-east": which way a spot lies from the base, as the compass reads it (north is -Y, east +X). */
	FString WayFrom(const FVector& Base, const FVector& At)
	{
		static const TCHAR* const Names[] = { TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"), TEXT("south"), TEXT("south-west"), TEXT("west"), TEXT("north-west") };
		const FVector D = At - Base;
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y));
		return Names[((FMath::RoundToInt(Deg / 45.f) % 8) + 8) % 8];
	}

	FString NameOf(FName Kind, int32 Count)
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
		const FString Name = Def ? Def->Name : Kind.ToString();
		if (Count == 1) return FString::Printf(TEXT("a %s"), *Name);
		return FString::Printf(TEXT("%d %s%s"), Count, *Name, Name.EndsWith(TEXT("s")) ? TEXT("") : TEXT("s"));
	}

	/** "3 Cinderfangs and a Scorchwing". */
	FString Who(const FRaid& R)
	{
		TArray<FString> Each;
		for (const TPair<FName, int32>& K : R.Kinds) Each.Add(NameOf(K.Key, K.Value));
		if (Each.Num() <= 1) return Each.Num() ? Each[0] : FString(TEXT("nothing"));
		const FString Last = Each.Pop();
		return FString::Join(Each, TEXT(", ")) + TEXT(" and ") + Last;
	}

	int32 Total(const FRaid& R)
	{
		int32 N = 0;
		for (const TPair<FName, int32>& K : R.Kinds) N += K.Value;
		return N;
	}

	/** Everyone of the claim who's on, wherever they are. Stage 0: the warning (a banner too). */
	void Tell(UWorld* World, const FRaid& R, uint8 Stage, const FString& Message)
	{
		FName Portrait;
		int32 Most = 0;
		for (const TPair<FName, int32>& K : R.Kinds) if (K.Value > Most) { Most = K.Value; Portrait = K.Key; }
		const FString Title = Portrait.IsNone() ? FString() : NameOf(Portrait, 2).RightChop(2);   // "Cinderfangs"
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			const AWRClaimTotem* Claim = R.Claim.Get();
			const FString Name = PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
			if (PC && !Name.IsEmpty() && FWRSocialRules::CanShare(Claim ? Claim->OwnerName : R.Owner, Name))
			{
				PC->ClientRaid(Portrait, Title, Message, Stage);
			}
		}
	}

	void ScheduleNext(UWorld* World, AWRClaimTotem* Claim)
	{
		const double Every = WRRaids::IntervalHours();
		if (!Claim || Every <= 0.0) return;
		const double Now = NowHours(World);
		Claim->NextRaidHours = Now + NextInterval(Every);
		UE_LOG(LogWildreach, Log, TEXT("RAID next for %s's claim in %.1f game hours"), *Claim->OwnerName, Claim->NextRaidHours - Now);
	}

	bool RaidOn(const AWRClaimTotem* Claim)
	{
		return G.Raids.ContainsByPredicate([Claim](const FRaid& R) { return R.Claim.Get() == Claim; });
	}

	/** The warning goes out; they come WarnSeconds later (from Bearing, compass degrees, when it's 0 or more: tests).
	 *  What the members were told, or why it didn't start. */
	FString Begin(UWorld* World, AWRClaimTotem* Claim, float WarnSeconds, float Bearing = -1.f)
	{
		FRaid R;
		R.Claim = Claim;
		R.Owner = Claim->OwnerName;
		R.Base = Claim->GetActorLocation();
		R.Radius = Claim->Radius;
		TArray<TPair<FName, float>> Kinds;
		R.Region = WRWildlife::RaidersAt(FVector2D(R.Base), Kinds);
		const AWRHearth* Hearth = AWRHearth::FindCovering(World, R.Base);
		R.HearthLevel = Hearth ? Hearth->GetTechLevel() : 0;
		R.Points = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("raid.points"), 3.f) + WRTune(TEXT("raid.pointsPerHearthLevel"), 2.f) * R.HearthLevel));
		FRandomStream Rand(FMath::Rand());
		R.Kinds = WRRaids::Plan(Kinds, R.Points, Rand);
		if (R.Kinds.Num() == 0)
		{
			UE_LOG(LogWildreach, Warning, TEXT("RAID none for %s's claim: no raiders in region %s"), *R.Owner, *R.Region.ToString());
			ScheduleNext(World, Claim);
			return TEXT("No raiders live here");
		}
		// From outside the claim's edge, on dry ground: a random way, the next one round if that's sea or no ground.
		const float Out = R.Radius + WRTune(TEXT("raid.spawnDistance"), 3000.f);
		// (A compass bearing b is the way (sin b, -cos b): north is -Y, east +X.)
		const float Start = Bearing >= 0.f ? Bearing - 90.f : FMath::FRandRange(0.f, 360.f);
		bool bFound = false;
		for (int32 i = 0; i < 16 && !bFound; ++i)
		{
			const FVector2D Dir = FVector2D(1.f, 0.f).GetRotated(Start + i * 22.5f);
			bFound = GroundAt(World, FVector2D(R.Base) + Dir * Out, R.Base.Z, R.From);
		}
		if (!bFound)
		{
			UE_LOG(LogWildreach, Warning, TEXT("RAID none for %s's claim: no ground round it to come from"), *R.Owner);
			Claim->NextRaidHours = NowHours(World) + 1.0;
			return TEXT("Nowhere round the base for them to come from");
		}
		R.Way = WayFrom(R.Base, R.From);
		R.ComeAt = World->GetTimeSeconds() + WarnSeconds;
		R.Stage = EStage::Warned;
		const FString Message = FString::Printf(TEXT("Raid: %s %s coming for your base from the %s, in %d s"), *Who(R), Total(R) == 1 ? TEXT("is") : TEXT("are"), *R.Way, FMath::RoundToInt(WarnSeconds));
		UE_LOG(LogWildreach, Log, TEXT("RAID warning for %s's claim (region %s, Hearth level %d, %d points): %s from the %s at %.0f %.0f, in %.0f s"),
			*R.Owner, *R.Region.ToString(), R.HearthLevel, R.Points, *Who(R), *R.Way, R.From.X, R.From.Y, WarnSeconds);
		Tell(World, R, 0, Message);
		G.Raids.Add(MoveTemp(R));
		return Message;
	}

	void Come(UWorld* World, FRaid& R)
	{
		R.PiecesBefore = CountPieces(World, R.Owner, R.Base, R.Radius);
		const FRotator Facing(0.f, (R.Base - R.From).Rotation().Yaw, 0.f);
		for (const TPair<FName, int32>& K : R.Kinds)
		{
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(K.Key);
			for (int32 i = 0; Def && i < K.Value; ++i)
			{
				FVector Ground = R.From;
				for (int32 Try = 0; Try < 6; ++Try)
				{
					const FVector2D At = FVector2D(R.From) + FMath::RandPointInCircle(450.f);
					if (GroundAt(World, At, R.From.Z, Ground)) break;
					Ground = R.From;
				}
				AWRCreature* Raider = AWRCreature::SpawnCreature(World, K.Key, Ground + FVector(0.f, 0.f, Def->HalfHeight + 40.f), Facing);
				if (!Raider) continue;
				Raider->bRaider = true;
				Raider->RaidBase = R.Base;
				Raider->RaidRadius = R.Radius;
				Raider->RaidOwner = R.Owner;
				Raider->SetHome(R.Base);
				R.Raiders.Add(Raider);
			}
		}
		R.Stage = EStage::On;
		R.EndAt = World->GetTimeSeconds() + WRTune(TEXT("raid.maxMinutes"), 6.f) * 60.f;
		UE_LOG(LogWildreach, Log, TEXT("RAID came at %s's claim: %d raiders (%s) from the %s; %d pieces there"), *R.Owner, R.Raiders.Num(), *Who(R), *R.Way, R.PiecesBefore);
		Tell(World, R, 1, FString::Printf(TEXT("Raid: they're here, from the %s"), *R.Way));
	}

	void Leave(UWorld* World, FRaid& R, const TCHAR* Why)
	{
		int32 Alive = 0;
		for (const TWeakObjectPtr<AWRCreature>& C : R.Raiders)
		{
			if (C.IsValid() && C->IsAlive())
			{
				C->bRaidLeaving = true;
				++Alive;
			}
		}
		const int32 Lost = FMath::Max(0, R.PiecesBefore - CountPieces(World, R.Owner, R.Base, R.Radius));
		R.Stage = EStage::Leaving;
		R.GoneAt = World->GetTimeSeconds() + WRTune(TEXT("raid.leaveSeconds"), 40.f);
		UE_LOG(LogWildreach, Log, TEXT("RAID over at %s's claim (%s): %d of %d leave, %d pieces lost"), *R.Owner, Why, Alive, R.Raiders.Num(), Lost);
		Tell(World, R, 2, FString::Printf(TEXT("Raid over: %s%s"), Alive > 0 ? *FString::Printf(TEXT("the last %d are leaving"), Alive) : TEXT("you held them off"),
			Lost > 0 ? *FString::Printf(TEXT(", %d %s lost"), Lost, Lost == 1 ? TEXT("piece") : TEXT("pieces")) : TEXT("")));
		ScheduleNext(World, R.Claim.Get());
	}

	/** One raid on by a step. True when it's done with. */
	bool Advance(UWorld* World, FRaid& R)
	{
		const double Now = World->GetTimeSeconds();
		switch (R.Stage)
		{
		case EStage::Warned:
			if (!R.Claim.IsValid())
			{
				UE_LOG(LogWildreach, Log, TEXT("RAID called off: %s's claim is gone"), *R.Owner);
				return true;
			}
			if (Now >= R.ComeAt) Come(World, R);
			return false;
		case EStage::On:
		{
			int32 Alive = 0, Dead = 0;
			for (const TWeakObjectPtr<AWRCreature>& C : R.Raiders)
			{
				if (C.IsValid() && C->IsAlive()) ++Alive;
				else ++Dead;
			}
			if (Alive == 0) Leave(World, R, *FString::Printf(TEXT("held: %d killed"), Dead));
			else if (Now >= R.EndAt) Leave(World, R, TEXT("time's up"));
			else if (!FWRSocialRules::IsAnyoneOnline(World, R.Owner)) Leave(World, R, TEXT("nobody of it is on"));
			return false;
		}
		case EStage::Leaving:
		{
			// Taken away once out of everyone's sight, or when the time's up; bodies stay (to be butchered).
			const float Out = R.Radius + WRTune(TEXT("raid.spawnDistance"), 3000.f) + 2000.f;
			int32 Left = 0, Taken = 0;
			for (const TWeakObjectPtr<AWRCreature>& C : R.Raiders)
			{
				AWRCreature* Raider = C.Get();
				if (!Raider || !Raider->IsAlive()) continue;
				const bool bAway = FVector::Dist2D(Raider->GetActorLocation(), R.Base) > Out && !PlayerWithin(World, Raider->GetActorLocation(), 5000.f);
				if (bAway || Now >= R.GoneAt)
				{
					Raider->Destroy();
					++Taken;
				}
				else ++Left;
			}
			if (Taken > 0) UE_LOG(LogWildreach, Log, TEXT("RAID %d raiders gone from %s's claim, %d still leaving"), Taken, *R.Owner, Left);
			return Left == 0;
		}
		}
		return true;
	}
}

using namespace WRRaidsImpl;

double WRRaids::IntervalHours()
{
	const FString Setting = FWRSocialRules::Setting(TEXT("raids"), TEXT("normal")).ToLower();
	if (Setting == TEXT("off")) return 0.0;
	if (Setting == TEXT("rare")) return WRTune(TEXT("raid.rareHours"), 72.f);
	if (Setting == TEXT("frequent")) return WRTune(TEXT("raid.frequentHours"), 12.f);
	return WRTune(TEXT("raid.normalHours"), 24.f);
}

int32 WRRaids::CostOf(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return FMath::Max(1, FMath::RoundToInt(FWRTuning::Get().Num(FString::Printf(TEXT("raid.cost.%s"), Def ? *Def->Tier.ToString() : TEXT("lowland")), 1.f)));
}

TArray<TPair<FName, int32>> WRRaids::Plan(const TArray<TPair<FName, float>>& Kinds, int32 Points, FRandomStream& Rand)
{
	TArray<TPair<FName, int32>> Out;
	int32 Left = Points;
	for (int32 Guard = 0; Left > 0 && Guard < 64; ++Guard)
	{
		float Weight = 0.f;
		for (const TPair<FName, float>& K : Kinds) if (K.Value > 0.f && CostOf(K.Key) <= Left) Weight += K.Value;
		if (Weight <= 0.f) break;
		float Roll = Rand.FRand() * Weight;
		FName Pick;
		for (const TPair<FName, float>& K : Kinds)
		{
			if (K.Value <= 0.f || CostOf(K.Key) > Left) continue;
			Pick = K.Key;
			if ((Roll -= K.Value) <= 0.f) break;
		}
		Left -= CostOf(Pick);
		if (TPair<FName, int32>* Have = Out.FindByPredicate([Pick](const TPair<FName, int32>& P) { return P.Key == Pick; })) ++Have->Value;
		else Out.Add({ Pick, 1 });
	}
	return Out;
}

void WRRaids::Start(UWorld* World)
{
	for (FRaid& R : G.Raids)
	{
		for (const TWeakObjectPtr<AWRCreature>& C : R.Raiders) if (C.IsValid() && C->IsAlive()) C->Destroy();
	}
	G = FState();
	if (World && World->GetNetMode() != NM_Client && !WRWorlds::Current().IsEmpty())
	{
		G.World = World;
		const double Every = IntervalHours();
		UE_LOG(LogWildreach, Log, TEXT("RAID setting %s: %s"), *FWRSocialRules::Setting(TEXT("raids"), TEXT("normal")),
			Every > 0.0 ? *FString::Printf(TEXT("a claim's raid every %.0f game hours or so, while someone of it is home"), Every) : TEXT("no raids"));
	}
}

void WRRaids::Step(UWorld* World)
{
	if (!World || G.World.Get() != World)
	{
		return;
	}
	for (int32 i = G.Raids.Num() - 1; i >= 0; --i)
	{
		if (Advance(World, G.Raids[i])) G.Raids.RemoveAt(i);
	}
	const double Every = IntervalHours();
	if (Every <= 0.0)
	{
		return;
	}
	const double Hours = NowHours(World);
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
	{
		AWRClaimTotem* Claim = *It;
		if (Claim->OwnerName.IsEmpty() || Claim->IsActorBeingDestroyed() || RaidOn(Claim))
		{
			continue;
		}
		if (Claim->NextRaidHours < 0.0)
		{
			// A new claim, or one from before raids: its first one interval from now.
			Claim->NextRaidHours = Hours + NextInterval(Every);
			UE_LOG(LogWildreach, Log, TEXT("RAID first for %s's claim in %.1f game hours"), *Claim->OwnerName, Claim->NextRaidHours - Hours);
			continue;
		}
		if (Hours < Claim->NextRaidHours)
		{
			continue;
		}
		if (!MemberHome(World, Claim))
		{
			if (!G.Waiting.Contains(Claim))
			{
				G.Waiting.Add(Claim);
				UE_LOG(LogWildreach, Log, TEXT("RAID due on %s's claim: waiting for someone of it to be home"), *Claim->OwnerName);
			}
			continue;
		}
		G.Waiting.Remove(Claim);
		Begin(World, Claim, WRTune(TEXT("raid.warnSeconds"), 60.f));
	}
}

FString WRRaids::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!World || G.World.Get() != World)
	{
		return TEXT("No raids in this game (no world)");
	}
	const FString Sub = Args.IsValidIndex(1) ? Args[1].ToLower() : FString(TEXT("status"));
	// The claim nearest the player.
	AWRClaimTotem* Claim = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
	{
		const float D = Pawn ? FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation()) : 0.f;
		if (D < Best) { Best = D; Claim = *It; }
	}
	const double Hours = NowHours(World);
	if (Sub == TEXT("now"))
	{
		if (!Claim) return TEXT("No claim");
		if (RaidOn(Claim)) return TEXT("A raid is already on there");
		return Begin(World, Claim, Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : WRTune(TEXT("raid.warnSeconds"), 60.f),
			Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : -1.f);
	}
	if (Sub == TEXT("due"))
	{
		if (!Claim) return TEXT("No claim");
		Claim->NextRaidHours = Hours;
		UE_LOG(LogWildreach, Log, TEXT("RAID made due on %s's claim"), *Claim->OwnerName);
		return TEXT("The nearest claim's raid is due");
	}
	if (Sub == TEXT("end"))
	{
		for (FRaid& R : G.Raids) if (R.Stage != EStage::Leaving) Leave(World, R, TEXT("ended by command"));
		return TEXT("Raids ended");
	}
	if (Sub == TEXT("plan"))
	{
		// What a raid on the nearest claim (or here) would be at each Hearth level, the same rolls each time.
		const FVector At = Claim ? Claim->GetActorLocation() : Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		TArray<TPair<FName, float>> Kinds;
		const FName Region = WRWildlife::RaidersAt(FVector2D(At), Kinds);
		TArray<FString> Levels;
		for (int32 Level = 0; Level <= 3; ++Level)
		{
			FRaid R;
			R.Points = FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("raid.points"), 3.f) + WRTune(TEXT("raid.pointsPerHearthLevel"), 2.f) * Level));
			FRandomStream Rand(1234 + Level);
			R.Kinds = Plan(Kinds, R.Points, Rand);
			Levels.Add(FString::Printf(TEXT("level %d (%d points) %s"), Level, R.Points, *Who(R)));
		}
		const FString Line = FString::Printf(TEXT("%s: %s"), *Region.ToString(), *FString::Join(Levels, TEXT("; ")));
		UE_LOG(LogWildreach, Log, TEXT("RAID plan %s"), *Line);
		return Line;
	}
	// status
	const double Every = IntervalHours();
	TArray<FString> Lines;
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It)
	{
		const FRaid* On = G.Raids.FindByPredicate([&It](const FRaid& R) { return R.Claim.Get() == *It; });
		Lines.Add(FString::Printf(TEXT("%s's claim: %s"), *It->OwnerName,
			On ? (On->Stage == EStage::Warned ? TEXT("warned") : On->Stage == EStage::On ? TEXT("raid on") : TEXT("raiders leaving"))
			: It->NextRaidHours < 0.0 ? TEXT("not scheduled yet") : *FString::Printf(TEXT("next in %.1f h"), It->NextRaidHours - Hours)));
	}
	const FString Line = FString::Printf(TEXT("raids %s (%s): %s"), *FWRSocialRules::Setting(TEXT("raids"), TEXT("normal")),
		Every > 0.0 ? *FString::Printf(TEXT("every %.0f h"), Every) : TEXT("off"), Lines.Num() ? *FString::Join(Lines, TEXT("; ")) : TEXT("no claims"));
	UE_LOG(LogWildreach, Log, TEXT("RAID status %s"), *Line);
	return Line;
}
