#include "WRWildlife.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRCreature.h"
#include "WRGameState.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "Algo/Reverse.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WRWildlifeImpl
{
	// ------------------------------------------------------------ the walk map

	/** Where one kind of creature may be: its cells, and the ones to feed on. */
	struct FRange
	{
		TBitArray<> Allowed;
		TArray<int32> All;
		TArray<int32> Feeding;
		/** Which connected piece of ground each cell is on: a way exists only within one. */
		TArray<int32> Piece;
	};

	struct FWalkMap
	{
		FString MapName;
		int32 N = 0;
		float Cell = 3200.f;
		float Origin = 0.f;
		TArray<TCHAR> Cells;
		TArray<FVector2D> Camps;
		TMap<FName, FVector2D> Bands;
		TMap<FName, float> CampGaps;
		TMap<FName, FRange> Ranges;

		int32 CellAt(const FVector2D& At) const
		{
			const int32 X = FMath::FloorToInt32((At.X - Origin) / Cell), Y = FMath::FloorToInt32((At.Y - Origin) / Cell);
			return X >= 0 && Y >= 0 && X < N && Y < N ? Y * N + X : INDEX_NONE;
		}
		FVector2D Centre(int32 Index) const { return FVector2D(Origin + (Index % N + 0.5f) * Cell, Origin + (Index / N + 0.5f) * Cell); }

		bool Load()
		{
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("island_walk.json"));
			const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
			if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("rows"), Rows) || Rows->Num() == 0)
			{
				return false;
			}
			Root->TryGetStringField(TEXT("map"), MapName);
			N = Rows->Num();
			Cell = Root->GetNumberField(TEXT("cell"));
			Origin = Root->GetNumberField(TEXT("origin"));
			Cells.Reset(N * N);
			for (const TSharedPtr<FJsonValue>& Row : *Rows)
			{
				const FString Text = Row->AsString();
				if (Text.Len() != N)
				{
					UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE Data/island_walk.json: a row of %d cells in a map of %d"), Text.Len(), N);
					N = 0;
					return false;
				}
				for (const TCHAR C : Text) Cells.Add(C);
			}
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Root->TryGetObjectField(TEXT("bands"), Object))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Object)->Values)
				{
					const TArray<TSharedPtr<FJsonValue>>& B = KV.Value->AsArray();
					if (B.Num() == 2) Bands.Add(FName(*KV.Key), FVector2D(B[0]->AsNumber() * 100.0, B[1]->AsNumber() * 100.0));
				}
			}
			if (Root->TryGetObjectField(TEXT("campGap"), Object))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Object)->Values) CampGaps.Add(FName(*KV.Key), KV.Value->AsNumber() * 100.f);
			}
			const TArray<TSharedPtr<FJsonValue>>* CampList = nullptr;
			if (Root->TryGetArrayField(TEXT("camps"), CampList))
			{
				for (const TSharedPtr<FJsonValue>& V : *CampList) Camps.Add(FVector2D(V->AsObject()->GetNumberField(TEXT("x")), V->AsObject()->GetNumberField(TEXT("y"))));
			}
			return true;
		}

		const FRange& RangeOf(const FWRCreatureDef& Def)
		{
			if (const FRange* Found = Ranges.Find(Def.Id))
			{
				return *Found;
			}
			FRange& R = Ranges.Add(Def.Id);
			R.Allowed.Init(false, N * N);
			const bool bFlyer = Def.Archetype == TEXT("flyer");
			const bool bVolcanic = Def.Biome == TEXT("volcanic");
			const FVector2D* BandOf = Bands.Find(Def.Tier);
			const FVector2D Band = BandOf ? *BandOf : FVector2D(0.0, 1.0e7);
			const float* GapOf = CampGaps.Find(Def.Behavior);
			const double GapSq = FMath::Square(GapOf ? *GapOf : 16000.f);
			for (int32 i = 0; i < Cells.Num(); ++i)
			{
				const TCHAR C = Cells[i];
				if (C == TEXT('~') || C == TEXT('x') || (C == TEXT('#') && !bFlyer))
				{
					continue;
				}
				const FVector2D At = Centre(i);
				if (bVolcanic != (At.X < 0.0) || FMath::Abs(At.X) < Band.X || FMath::Abs(At.X) > Band.Y)
				{
					continue;
				}
				if (Camps.ContainsByPredicate([&At, GapSq](const FVector2D& Camp) { return FVector2D::DistSquared(Camp, At) < GapSq; }))
				{
					continue;
				}
				R.Allowed[i] = true;
			}
			// Connected pieces of it, the same way a walk connects them. A scrap too small to live on is left out: a pack
			// starting there moves to proper ground.
			const int32 MinPiece = FMath::RoundToInt(WRTune(TEXT("wildlife.minPieceCells"), 40.f));
			R.Piece.Init(INDEX_NONE, N * N);
			int32 NextPiece = 0;
			TArray<int32> Stack, Members;
			for (int32 i = 0; i < Cells.Num(); ++i)
			{
				if (!R.Allowed[i] || R.Piece[i] != INDEX_NONE)
				{
					continue;
				}
				const int32 Label = NextPiece++;
				Members.Reset();
				Stack.Reset();
				Stack.Add(i);
				R.Piece[i] = Label;
				while (Stack.Num() > 0)
				{
					const int32 Here = Stack.Pop(EAllowShrinking::No);
					Members.Add(Here);
					const int32 X = Here % N, Y = Here / N;
					for (int32 Dy = -1; Dy <= 1; ++Dy)
					{
						for (int32 Dx = -1; Dx <= 1; ++Dx)
						{
							const int32 Nx = X + Dx, Ny = Y + Dy;
							if ((!Dx && !Dy) || Nx < 0 || Ny < 0 || Nx >= N || Ny >= N)
							{
								continue;
							}
							const int32 Next = Ny * N + Nx;
							if (R.Allowed[Next] && R.Piece[Next] == INDEX_NONE && !(Dx && Dy && (!R.Allowed[Y * N + Nx] || !R.Allowed[Ny * N + X])))
							{
								R.Piece[Next] = Label;
								Stack.Add(Next);
							}
						}
					}
				}
				if (Members.Num() < MinPiece)
				{
					for (const int32 Member : Members) R.Allowed[Member] = false;
				}
			}
			for (int32 i = 0; i < Cells.Num(); ++i)
			{
				if (R.Allowed[i])
				{
					R.All.Add(i);
					if (Cells[i] == TEXT('f') || Cells[i] == TEXT('w')) R.Feeding.Add(i);
				}
			}
			return R;
		}

		/** The nearest cell of the range, up to Reach cells away. */
		int32 Nearest(const FRange& R, const FVector2D& At, int32 Reach = 12) const
		{
			const int32 Here = CellAt(At);
			if (Here == INDEX_NONE)
			{
				return INDEX_NONE;
			}
			if (R.Allowed[Here])
			{
				return Here;
			}
			const int32 X = Here % N, Y = Here / N;
			for (int32 Ring = 1; Ring <= Reach; ++Ring)
			{
				for (int32 Dy = -Ring; Dy <= Ring; ++Dy)
				{
					for (int32 Dx = -Ring; Dx <= Ring; ++Dx)
					{
						if (FMath::Max(FMath::Abs(Dx), FMath::Abs(Dy)) != Ring || X + Dx < 0 || Y + Dy < 0 || X + Dx >= N || Y + Dy >= N)
						{
							continue;
						}
						const int32 Index = (Y + Dy) * N + X + Dx;
						if (R.Allowed[Index]) return Index;
					}
				}
			}
			return INDEX_NONE;
		}

		/** A*, eight ways, no cutting a blocked corner. Cells from Start to Goal. */
		bool Search(const FRange& R, int32 Start, int32 Goal, TArray<int32>& Out) const
		{
			struct FOpen { float Cost; int32 Index; };
			const auto Lower = [](const FOpen& A, const FOpen& B) { return A.Cost < B.Cost; };
			static TArray<float> Spent;
			static TArray<int32> Came;
			Spent.Init(TNumericLimits<float>::Max(), N * N);
			Came.SetNumUninitialized(N * N);
			const int32 GoalX = Goal % N, GoalY = Goal / N;
			const auto Guess = [GoalX, GoalY, this](int32 Index)
			{
				const int32 Dx = FMath::Abs(Index % N - GoalX), Dy = FMath::Abs(Index / N - GoalY);
				return (Dx + Dy) + (1.414f - 2.f) * FMath::Min(Dx, Dy);
			};
			TArray<FOpen> Open;
			Open.HeapPush(FOpen{ Guess(Start), Start }, Lower);
			Spent[Start] = 0.f;
			Came[Start] = INDEX_NONE;
			while (Open.Num() > 0)
			{
				FOpen Top;
				Open.HeapPop(Top, Lower, EAllowShrinking::No);
				if (Top.Index == Goal)
				{
					Out.Reset();
					for (int32 At = Goal; At != INDEX_NONE; At = Came[At]) Out.Add(At);
					Algo::Reverse(Out);
					return true;
				}
				const int32 X = Top.Index % N, Y = Top.Index / N;
				const float Here = Spent[Top.Index];
				for (int32 Dy = -1; Dy <= 1; ++Dy)
				{
					for (int32 Dx = -1; Dx <= 1; ++Dx)
					{
						const int32 Nx = X + Dx, Ny = Y + Dy;
						if ((!Dx && !Dy) || Nx < 0 || Ny < 0 || Nx >= N || Ny >= N)
						{
							continue;
						}
						const int32 Next = Ny * N + Nx;
						if (!R.Allowed[Next] || (Dx && Dy && (!R.Allowed[Y * N + Nx] || !R.Allowed[Ny * N + X])))
						{
							continue;
						}
						const float Cost = Here + (Dx && Dy ? 1.414f : 1.f);
						if (Cost < Spent[Next])
						{
							Spent[Next] = Cost;
							Came[Next] = Top.Index;
							Open.HeapPush(FOpen{ Cost + Guess(Next), Next }, Lower);
						}
					}
				}
			}
			return false;
		}

		bool ClearLine(const FRange& R, const FVector2D& A, const FVector2D& B) const
		{
			const int32 Steps = FMath::CeilToInt32(FVector2D::Distance(A, B) / (Cell * 0.5f));
			for (int32 i = 1; i < Steps; ++i)
			{
				const int32 Index = CellAt(FMath::Lerp(A, B, static_cast<double>(i) / Steps));
				if (Index == INDEX_NONE || !R.Allowed[Index]) return false;
			}
			return true;
		}

		/** Waypoints from one spot to another: straight lines wherever the ground allows, not cell by cell. */
		bool Route(const FRange& R, const FVector2D& From, const FVector2D& To, TArray<FVector2D>& Out) const
		{
			const int32 Start = Nearest(R, From), Goal = Nearest(R, To);
			TArray<int32> Steps;
			if (Start == INDEX_NONE || Goal == INDEX_NONE || !Search(R, Start, Goal, Steps))
			{
				return false;
			}
			Out.Reset();
			int32 Anchor = 0;
			while (Anchor < Steps.Num() - 1)
			{
				int32 Far = Anchor + 1;
				for (int32 Try = FMath::Min(Anchor + 48, Steps.Num() - 1); Try > Far; --Try)
				{
					if (ClearLine(R, Centre(Steps[Anchor]), Centre(Steps[Try]))) { Far = Try; break; }
				}
				Out.Add(Centre(Steps[Far]));
				Anchor = Far;
			}
			if (Out.Num() == 0) Out.Add(Centre(Goal));
			return true;
		}
	};

	FWalkMap& Map()
	{
		static FWalkMap Instance;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			Instance.Load();
		}
		return Instance;
	}

	// ------------------------------------------------------------ the packs

	struct FSite { FName Id; FVector At; float Radius; };

	/** This game's packs. One game runs at a time in a process; Start begins afresh. */
	struct FState
	{
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AWRCreatureSpawner>> Packs;
		TArray<FSite> Sites;
		TMap<FName, int32> SitesOf;
		TMap<FName, float> RefillOwed;
		TArray<const AWRClaimTotem*> Claims;
		double Clock = 0.0;          // seconds of wildlife time (saved)
		double NextLogAt = 0.0;
		int32 Caught = 0, Missed = 0, Bred = 0, WanderedIn = 0, Retired = 0, Moved = 0;
		float StepMs = 0.f;
	} G;

	/** The way a pack's size is rolled: mostly small, now and then the whole range. */
	int32 RollPack(const FWRCreatureDef& Def, float Roll)
	{
		const int32 Span = Def.PackMax - Def.PackMin + 1;
		return FMath::Clamp(Def.PackMin + FMath::FloorToInt32(Span * FMath::Pow(Roll, WRTune(TEXT("wildlife.packSkew"), 1.6f))), Def.PackMin, Def.PackMax);
	}

	float MeanPack(const FWRCreatureDef& Def)
	{
		float Sum = 0.f;
		for (int32 i = 0; i < 100; ++i) Sum += RollPack(Def, (i + 0.5f) / 100.f);
		return Sum / 100.f;
	}

	float CreatureNumbers() { return FMath::Max(0.1f, WRTune(TEXT("server.creatureNumbers"), 1.f)); }

	float TargetFor(const FWRCreatureDef& Def)
	{
		const int32* Sites = G.SitesOf.Find(Def.Id);
		return Sites ? *Sites * MeanPack(Def) * CreatureNumbers() : 0.f;
	}

	AWRCreatureSpawner* MakePack(UWorld* World, const FWRCreatureDef& Def, const FVector& At, int32 Count, float Radius)
	{
		const FTransform Place{ At };
		AWRCreatureSpawner* Pack = World->SpawnActorDeferred<AWRCreatureSpawner>(AWRCreatureSpawner::StaticClass(), Place, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Pack)
		{
			return nullptr;
		}
		Pack->bRoams = true;
		Pack->CreatureId = Def.Id;
		Pack->Count = Count;
		Pack->Radius = Radius;
		Pack->ActivationRadius = WRTune(TEXT("wildlife.activationRadius"), 25000.f);
		Pack->bTraceGround = true;
		Pack->Territory = FVector2D(At);
		Pack->bWanderer = FMath::FRand() < Def.WanderChance;
		// Spread out over the first minutes, not all setting off in the same step.
		Pack->NextThinkAt = G.Clock + FMath::FRand() * WRTune(TEXT("wildlife.grazeMaxMinutes"), 10.f) * 60.0;
		Pack->FinishSpawning(Place);
		G.Packs.Add(Pack);
		return Pack;
	}

	bool NearAPlayer(UWorld* World, const FVector2D& At, float Range)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (Pawn && FVector2D::Distance(FVector2D(Pawn->GetActorLocation()), At) < Range)
			{
				return true;
			}
		}
		return false;
	}

	bool InAClaim(const FVector2D& At)
	{
		for (const AWRClaimTotem* Claim : G.Claims)
		{
			if (FVector2D::Distance(FVector2D(Claim->GetActorLocation()), At) < Claim->Radius + 1500.f) return true;
		}
		return false;
	}

	/** Sends it on a walk to somewhere within these distances of Centre. False: nowhere found this time. */
	bool PickGoal(AWRCreatureSpawner& Pack, const FWRCreatureDef& Def, const TArray<int32>& Cells, const FVector2D& Centre, float MinDist, float MaxDist)
	{
		FWalkMap& M = Map();
		const FRange& R = M.RangeOf(Def);
		if (Cells.Num() == 0)
		{
			return false;
		}
		const FVector2D From(Pack.GetActorLocation());
		const int32 HereCell = M.Nearest(R, From);
		if (HereCell == INDEX_NONE)
		{
			return false;
		}
		int32 Searches = 0;
		for (int32 Try = 0; Try < 32 && Searches < 3; ++Try)
		{
			const int32 ToCell = Cells[FMath::RandHelper(Cells.Num())];
			const FVector2D To = M.Centre(ToCell);
			const double D = FVector2D::Distance(To, Centre);
			if (R.Piece[ToCell] != R.Piece[HereCell] || D < MinDist || D > MaxDist || InAClaim(To))
			{
				continue;
			}
			++Searches;
			if (M.Route(R, From, To, Pack.Path))
			{
				Pack.Doing = EWRPackDoing::Travelling;
				return true;
			}
		}
		return false;
	}

	/** Where to next, by kind. */
	void SetOff(AWRCreatureSpawner& Pack, const FWRCreatureDef& Def)
	{
		const FRange& R = Map().RangeOf(Def);
		const FVector2D Here(Pack.GetActorLocation());
		bool bOff = false;
		if (Pack.bWanderer)
		{
			bOff = PickGoal(Pack, Def, R.All, Here, WRTune(TEXT("wildlife.wanderMinDistance"), 120000.f), 1.0e7f);
			if (bOff) Pack.Territory = Pack.Path.Last();
		}
		else if (Def.Diet == TEXT("carnivore"))
		{
			bOff = PickGoal(Pack, Def, R.All, Pack.Territory, WRTune(TEXT("wildlife.prowlMinDistance"), 10000.f), WRTune(TEXT("wildlife.territoryRadius"), 60000.f));
		}
		else
		{
			const float Min = WRTune(TEXT("wildlife.grazeMinDistance"), 20000.f), Max = WRTune(TEXT("wildlife.grazeMaxDistance"), 80000.f);
			bOff = PickGoal(Pack, Def, R.Feeding, Here, Min, Max) || PickGoal(Pack, Def, R.All, Here, Min, Max);
		}
		if (!bOff)
		{
			Pack.NextThinkAt = G.Clock + 60.0;  // try again in a minute
		}
	}

	void StayAWhile(AWRCreatureSpawner& Pack)
	{
		const float Scale = Pack.bWanderer ? WRTune(TEXT("wildlife.wandererStayScale"), 0.25f) : 1.f;
		Pack.Doing = EWRPackDoing::Grazing;
		Pack.Path.Reset();
		Pack.Quarry = nullptr;
		Pack.NextThinkAt = G.Clock + FMath::FRandRange(WRTune(TEXT("wildlife.grazeMinMinutes"), 3.f), WRTune(TEXT("wildlife.grazeMaxMinutes"), 10.f)) * 60.0 * Scale;
	}

	/** A hungry hunter goes after the nearest prey in sight. False: nothing to go after. */
	bool Hunt(AWRCreatureSpawner& Pack, const FWRCreatureDef& Def)
	{
		const FVector2D Here(Pack.GetActorLocation());
		const FRange& R = Map().RangeOf(Def);
		const int32 HereCell = Map().Nearest(R, Here);
		AWRCreatureSpawner* Best = nullptr;
		double BestDist = WRTune(TEXT("wildlife.huntSight"), 50000.f);
		for (const TWeakObjectPtr<AWRCreatureSpawner>& Other : G.Packs)
		{
			AWRCreatureSpawner* P = Other.Get();
			const FWRCreatureDef* PreyDef = P && P != &Pack && P->Count > 0 ? FWRCreatureRegistry::Get().Find(P->CreatureId) : nullptr;
			if (HereCell == INDEX_NONE || !PreyDef || PreyDef->Archetype == TEXT("burrower") || !FWRCreatureDef::CanHunt(Def, *PreyDef))
			{
				continue;
			}
			const double D = FVector2D::Distance(Here, FVector2D(P->GetActorLocation()));
			if (D < BestDist)
			{
				// Only what it can get to: the prey's on ground this hunter walks, on the same piece of it.
				const int32 PreyCell = Map().Nearest(R, FVector2D(P->GetActorLocation()), 2);
				if (PreyCell != INDEX_NONE && R.Piece[PreyCell] == R.Piece[HereCell])
				{
					BestDist = D;
					Best = P;
				}
			}
		}
		if (!Best)
		{
			Pack.Quarry = nullptr;
			return false;
		}
		const FVector2D There(Best->GetActorLocation());
		// A new way only when it has moved on: a search each step for every hunter would add up.
		if (Pack.Quarry.Get() != Best || Pack.Path.Num() == 0 || FVector2D::Distance(There, Pack.QuarryWas) > WRTune(TEXT("wildlife.rehuntDistance"), 10000.f))
		{
			if (!Map().Route(Map().RangeOf(Def), Here, There, Pack.Path))
			{
				Pack.FedUntil = G.Clock + 300.0;  // no way there: try again later
				Pack.Quarry = nullptr;
				return false;
			}
			Pack.QuarryWas = There;
		}
		Pack.Quarry = Best;
		Pack.Doing = EWRPackDoing::Hunting;
		return true;
	}

	void Think(AWRCreatureSpawner& Pack, const FWRCreatureDef& Def, bool bNight)
	{
		if (Def.Archetype == TEXT("burrower"))
		{
			Pack.Doing = EWRPackDoing::Resting;  // home is home
			return;
		}
		const bool bBedtime = (Def.Activity == TEXT("diurnal") && bNight) || (Def.Activity == TEXT("nocturnal") && !bNight);
		if (bBedtime)
		{
			Pack.Doing = EWRPackDoing::Resting;
			Pack.Path.Reset();
			Pack.Quarry = nullptr;
			return;
		}
		if (Pack.Doing == EWRPackDoing::Resting)
		{
			StayAWhile(Pack);
		}
		// Hunters hunt when hungry, and by night unless they sleep through it.
		if (Def.Diet == TEXT("carnivore") && G.Clock >= Pack.FedUntil && (bNight || Def.Activity == TEXT("diurnal")) && Hunt(Pack, Def))
		{
			return;
		}
		if (Pack.Doing == EWRPackDoing::Hunting)
		{
			StayAWhile(Pack);
		}
		if (Pack.Doing == EWRPackDoing::Travelling && Pack.Path.Num() == 0)
		{
			StayAWhile(Pack);
		}
		if (Pack.Doing == EWRPackDoing::Grazing && G.Clock >= Pack.NextThinkAt)
		{
			SetOff(Pack, Def);
		}
	}

	/** A hunter has caught up with its quarry where nobody can see: simple odds, and either way the quarry runs. */
	void SettleHunt(AWRCreatureSpawner& Hunter, AWRCreatureSpawner& Prey, const FWRCreatureDef& PreyDef)
	{
		const bool bCaught = FMath::FRand() < WRTune(TEXT("wildlife.huntChance"), 0.35f);
		const float Hours = bCaught ? WRTune(TEXT("wildlife.fedHours"), 0.75f) : WRTune(TEXT("wildlife.missedHours"), 0.15f);
		Hunter.FedUntil = G.Clock + Hours * 3600.f * FMath::FRandRange(0.8f, 1.2f);
		StayAWhile(Hunter);
		if (bCaught)
		{
			Prey.Count = FMath::Max(0, Prey.Count - 1);
			++G.Caught;
		}
		else
		{
			++G.Missed;
		}
		UE_LOG(LogWildreach, Log, TEXT("WILDLIFE hunt: %s %s a %s at %.0f %.0f (%d left)"), *Hunter.CreatureId.ToString(), bCaught ? TEXT("caught") : TEXT("missed"), *Prey.CreatureId.ToString(),
			Prey.GetActorLocation().X, Prey.GetActorLocation().Y, Prey.Count);
		if (Prey.Count > 0)
		{
			const FRange& R = Map().RangeOf(PreyDef);
			if (!PickGoal(Prey, PreyDef, R.All, FVector2D(Prey.GetActorLocation()), 30000.f, 90000.f))
			{
				StayAWhile(Prey);
			}
		}
	}

	/** Numbers come back slowly towards each kind's share, and go down slowly if the world asks for fewer. */
	void Refill(UWorld* World, float Seconds)
	{
		const float PerHour = WRTune(TEXT("wildlife.refillPerHour"), 0.1f);
		for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All())
		{
			const int32* SiteCount = G.SitesOf.Find(Def.Id);
			if (!SiteCount)
			{
				continue;
			}
			const float Target = TargetFor(Def);
			int32 Animals = 0;
			TArray<AWRCreatureSpawner*> Quiet;
			for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs)
			{
				if (P.IsValid() && P->CreatureId == Def.Id)
				{
					Animals += P->Count;
					if (!P->IsActive()) Quiet.Add(P.Get());
				}
			}
			float& Owed = G.RefillOwed.FindOrAdd(Def.Id);
			if (FMath::Abs(Animals - Target) < 1.f)
			{
				Owed = 0.f;
				continue;
			}
			Owed += FMath::Max(Target, 1.f) * PerHour * Seconds / 3600.f;
			while (Owed >= 1.f)
			{
				Owed -= 1.f;
				if (Animals < Target)
				{
					// A pack with room breeds; failing that, a new one wanders in from somewhere nobody's looking.
					const TArray<AWRCreatureSpawner*> Room = Quiet.FilterByPredicate([&Def](const AWRCreatureSpawner* P) { return P->Count < Def.PackMax && (P->Count > 0 || Def.Archetype == TEXT("burrower")); });
					if (Room.Num() > 0 && FMath::FRand() < 0.7f)
					{
						Room[FMath::RandHelper(Room.Num())]->Count++;
						++G.Bred;
						++Animals;
					}
					else if (Def.Archetype != TEXT("burrower"))
					{
						const TArray<FSite> Own = G.Sites.FilterByPredicate([&Def](const FSite& S) { return S.Id == Def.Id; });
						const FSite& Site = Own[FMath::RandHelper(Own.Num())];
						if (!NearAPlayer(World, FVector2D(Site.At), WRTune(TEXT("wildlife.arriveDistance"), 40000.f)))
						{
							const int32 Count = FMath::Min(RollPack(Def, FMath::FRand()), FMath::CeilToInt32(Target - Animals));
							if (MakePack(World, Def, Site.At, FMath::Max(1, Count), Site.Radius))
							{
								++G.WanderedIn;
								Animals += FMath::Max(1, Count);
							}
						}
					}
				}
				else if (Animals > Target && Quiet.Num() > 0)
				{
					// Fewer asked for (the world's settings changed): the smallest pack thins out, out of sight.
					AWRCreatureSpawner* Smallest = nullptr;
					for (AWRCreatureSpawner* P : Quiet)
					{
						if (P->Count > 0 && (!Smallest || P->Count < Smallest->Count)) Smallest = P;
					}
					if (Smallest)
					{
						Smallest->Count--;
						++G.Retired;
						--Animals;
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------- the public face

const TCHAR* WRWildlife::DoingName(EWRPackDoing Doing)
{
	switch (Doing)
	{
	case EWRPackDoing::Travelling: return TEXT("travelling");
	case EWRPackDoing::Hunting: return TEXT("hunting");
	case EWRPackDoing::Resting: return TEXT("resting");
	default: return TEXT("grazing");
	}
}

bool WRWildlife::CanStand(FName CreatureId, const FVector2D& At)
{
	using namespace WRWildlifeImpl;
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	FWalkMap& M = Map();
	const int32 Cell = M.N > 0 ? M.CellAt(At) : INDEX_NONE;
	return Def && Cell != INDEX_NONE && M.RangeOf(*Def).Allowed[Cell];
}

bool WRWildlife::FindPath(FName CreatureId, const FVector2D& From, const FVector2D& To, TArray<FVector2D>& OutPath)
{
	using namespace WRWildlifeImpl;
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	FWalkMap& M = Map();
	return Def && M.N > 0 && M.Route(M.RangeOf(*Def), From, To, OutPath);
}

int32 WRWildlife::TargetOf(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def ? FMath::RoundToInt(WRWildlifeImpl::TargetFor(*Def)) : 0;
}

int32 WRWildlife::Start(UWorld* World)
{
	using namespace WRWildlifeImpl;
	G = FState();
	// (Not behind the main menu: that island is no world, and nothing streams in around the idle body it holds, so a pack
	// that came out near it would fall through the ground and be fished out over and over.)
	if (!World || World->GetNetMode() == NM_Client || WRTune(TEXT("wildlife.enabled"), 1.f) <= 0.f || WRWorlds::Current().IsEmpty())
	{
		return 0;
	}
	const TSharedPtr<FJsonObject> Spawns = FWRTuning::LoadDataFile(TEXT("island_spawns.json"));
	const TArray<TSharedPtr<FJsonValue>>* SiteList = nullptr;
	FString MapName;
	if (!Spawns || !Spawns->TryGetStringField(TEXT("map"), MapName) || MapName != UWorld::RemovePIEPrefix(World->GetMapName()) || !Spawns->TryGetArrayField(TEXT("sites"), SiteList))
	{
		return 0;
	}
	if (Map().N == 0 || Map().MapName != MapName)
	{
		UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE no walk map for %s (Data/island_walk.json, Tools/terrain/walkmap.mjs): packs stay where they start"), *MapName);
	}
	G.World = World;
	for (const TSharedPtr<FJsonValue>& Value : *SiteList)
	{
		const TSharedPtr<FJsonObject> Site = Value.IsValid() ? Value->AsObject() : nullptr;
		const FName Id = Site ? FName(*Site->GetStringField(TEXT("creature"))) : NAME_None;
		if (!FWRCreatureRegistry::Get().Find(Id))
		{
			UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE site for unknown creature %s"), *Id.ToString());
			continue;
		}
		G.Sites.Add({ Id, FVector(Site->GetNumberField(TEXT("x")), Site->GetNumberField(TEXT("y")), Site->GetNumberField(TEXT("z")) + 100.f), static_cast<float>(Site->GetNumberField(TEXT("radius"))) });
		G.SitesOf.FindOrAdd(Id)++;
	}

	// The world's own, if it has been played.
	bool bLoaded = false;
	const FString Dir = WRWorlds::CurrentDir();
	FString Text;
	TSharedPtr<FJsonObject> Saved;
	// (They're saved together: without its world.json a world is starting over, and so does its wildlife.)
	if (!Dir.IsEmpty() && FPaths::FileExists(FPaths::Combine(Dir, TEXT("world.json"))) && FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("wildlife.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Saved) && Saved.IsValid())
	{
		Saved->TryGetNumberField(TEXT("clock"), G.Clock);
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Saved->TryGetArrayField(TEXT("packs"), List))
		{
			bLoaded = true;
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const FWRCreatureDef* Def = O.IsValid() ? FWRCreatureRegistry::Get().Find(FName(*O->GetStringField(TEXT("id")))) : nullptr;
				if (!Def)
				{
					continue;
				}
				const FVector At(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z")));
				AWRCreatureSpawner* Pack = MakePack(World, *Def, At, O->GetIntegerField(TEXT("n")), O->GetNumberField(TEXT("r")));
				if (!Pack)
				{
					continue;
				}
				Pack->PackHealth = O->GetNumberField(TEXT("hp"));
				Pack->Territory = FVector2D(O->GetNumberField(TEXT("tx")), O->GetNumberField(TEXT("ty")));
				Pack->bWanderer = O->GetBoolField(TEXT("wander"));
				Pack->NextThinkAt = O->GetNumberField(TEXT("next"));
				Pack->FedUntil = O->GetNumberField(TEXT("fed"));
				// Its way isn't saved: it picks a new one at the next step (a resting pack is still resting).
				const FString Doing = O->GetStringField(TEXT("doing"));
				Pack->Doing = Doing == TEXT("resting") ? EWRPackDoing::Resting : EWRPackDoing::Grazing;
			}
		}
	}
	if (!bLoaded)
	{
		// A new world: the sites, packs of all sizes. Fewer asked for: that share of the sites, spread evenly over the
		// list. More: more than one pack to a site, each going its own way.
		const float Numbers = CreatureNumbers();
		float Owed = 0.f;
		for (const FSite& Site : G.Sites)
		{
			const FWRCreatureDef& Def = *FWRCreatureRegistry::Get().Find(Site.Id);
			Owed += Numbers;
			for (bool bFirst = true; Owed >= 1.f - KINDA_SMALL_NUMBER; bFirst = false)
			{
				Owed -= 1.f;
				FVector At = Site.At;
				if (!bFirst && Def.Archetype != TEXT("burrower"))
				{
					FWalkMap& M = Map();
					const TArray<int32>* Cells = M.N > 0 ? &M.RangeOf(Def).All : nullptr;
					if (Cells && Cells->Num() > 0)
					{
						const FVector2D Elsewhere = M.Centre((*Cells)[FMath::RandHelper(Cells->Num())]);
						At = FVector(Elsewhere.X, Elsewhere.Y, Site.At.Z);
					}
				}
				MakePack(World, Def, At, RollPack(Def, FMath::FRand()), Site.Radius);
			}
		}
	}

	int32 Animals = 0;
	for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs) if (P.IsValid()) Animals += P->Count;
	float Target = 0.f;
	for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All()) Target += TargetFor(Def);
	UE_LOG(LogWildreach, Log, TEXT("WILDLIFE %d packs, %d animals, %s; the island's share is %.0f; a pack comes out when a player is within %.0f m"),
		G.Packs.Num(), Animals, bLoaded ? TEXT("as the world left them") : TEXT("new"), Target, WRTune(TEXT("wildlife.activationRadius"), 25000.f) / 100.f);
	return G.Packs.Num();
}

void WRWildlife::Step(UWorld* World, float RealSeconds)
{
	using namespace WRWildlifeImpl;
	if (G.World.Get() != World || Map().N == 0)
	{
		return;
	}
	const double Began = FPlatformTime::Seconds();
	const float Seconds = RealSeconds * FMath::Max(0.f, WRTune(TEXT("wildlife.timeScale"), 1.f));
	G.Clock += Seconds;
	const bool bNight = AWRGameState::IsNightIn(World);
	G.Claims.Reset();
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It) G.Claims.Add(*It);
	G.Packs.RemoveAll([](const TWeakObjectPtr<AWRCreatureSpawner>& P) { return !P.IsValid(); });

	const float HealPerHour = WRTune(TEXT("wildlife.healPerHour"), 0.25f);
	int32 Travelling = 0, Grazing = 0, Hunting = 0, Resting = 0, Animals = 0, Active = 0;
	G.Moved = 0;
	for (const TWeakObjectPtr<AWRCreatureSpawner>& P : TArray<TWeakObjectPtr<AWRCreatureSpawner>>(G.Packs))
	{
		AWRCreatureSpawner* Pack = P.Get();
		const FWRCreatureDef* Def = Pack ? FWRCreatureRegistry::Get().Find(Pack->CreatureId) : nullptr;
		if (!Def)
		{
			continue;
		}
		// Wiped out: gone, once nobody's looking (a colony's home stays, and can fill again).
		if (Pack->Count <= 0 && !Pack->IsActive() && Pack->NumAlive() == 0)
		{
			if (Def->Archetype != TEXT("burrower"))
			{
				Pack->Destroy();
				continue;
			}
		}
		Animals += Pack->Count;
		if (Pack->IsActive())
		{
			++Active;
			// In view it walks at its own pace (its Tick), and stays put in a fight.
			if (!Pack->IsBusy()) Think(*Pack, *Def, bNight);
		}
		else if (Pack->Count > 0)
		{
			Pack->PackHealth = FMath::Min(1.f, Pack->PackHealth + HealPerHour * Seconds / 3600.f);
			Think(*Pack, *Def, bNight);
			const bool bMoving = Pack->Path.Num() > 0;
			Pack->Advance(Seconds);
			G.Moved += bMoving;
		}
		switch (Pack->Doing)
		{
		case EWRPackDoing::Travelling: ++Travelling; break;
		case EWRPackDoing::Hunting: ++Hunting; break;
		case EWRPackDoing::Resting: ++Resting; break;
		default: ++Grazing; break;
		}
	}

	// Hunts nobody can see. (In view the creatures hunt for themselves: AWRCreatureAI.)
	const float Catch = WRTune(TEXT("wildlife.catchRadius"), 5000.f);
	for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs)
	{
		AWRCreatureSpawner* Hunter = P.Get();
		AWRCreatureSpawner* Prey = Hunter && Hunter->Doing == EWRPackDoing::Hunting ? Hunter->Quarry.Get() : nullptr;
		if (Prey && !Hunter->IsActive() && !Prey->IsActive() && Prey->Count > 0 && FVector::Dist2D(Hunter->GetActorLocation(), Prey->GetActorLocation()) < Catch)
		{
			if (const FWRCreatureDef* PreyDef = FWRCreatureRegistry::Get().Find(Prey->CreatureId))
			{
				SettleHunt(*Hunter, *Prey, *PreyDef);
			}
		}
	}

	Refill(World, Seconds);

	G.StepMs = static_cast<float>((FPlatformTime::Seconds() - Began) * 1000.0);
	const double Now = World->GetTimeSeconds();
	if (Now >= G.NextLogAt)
	{
		G.NextLogAt = Now + WRTune(TEXT("wildlife.logSeconds"), 300.f);
		float Target = 0.f;
		for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All()) Target += TargetFor(Def);
		UE_LOG(LogWildreach, Log, TEXT("WILDLIFE step: %d packs (%d in view), %d animals of %.0f; %d travelling, %d grazing, %d hunting, %d resting; since last: %d caught, %d missed, %d bred, %d wandered in, %d thinned; %.2f ms"),
			G.Packs.Num(), Active, Animals, Target, Travelling, Grazing, Hunting, Resting, G.Caught, G.Missed, G.Bred, G.WanderedIn, G.Retired, G.StepMs);
		G.Caught = G.Missed = G.Bred = G.WanderedIn = G.Retired = 0;
	}
}

void WRWildlife::Save(UWorld* World)
{
	using namespace WRWildlifeImpl;
	const FString Dir = WRWorlds::CurrentDir();
	if (G.World.Get() != World || Dir.IsEmpty())
	{
		return;
	}
	TArray<TSharedPtr<FJsonValue>> List;
	for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs)
	{
		const AWRCreatureSpawner* Pack = P.Get();
		if (!Pack || Pack->IsActorBeingDestroyed())
		{
			continue;
		}
		const FVector At = Pack->GetActorLocation();
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), Pack->CreatureId.ToString());
		O->SetNumberField(TEXT("x"), FMath::RoundToInt(At.X));
		O->SetNumberField(TEXT("y"), FMath::RoundToInt(At.Y));
		O->SetNumberField(TEXT("z"), FMath::RoundToInt(At.Z));
		O->SetNumberField(TEXT("n"), Pack->Count);
		O->SetNumberField(TEXT("r"), Pack->Radius);
		O->SetNumberField(TEXT("hp"), Pack->IsActive() ? Pack->AverageHealth() : Pack->PackHealth);
		O->SetStringField(TEXT("doing"), DoingName(Pack->Doing));
		O->SetNumberField(TEXT("tx"), FMath::RoundToInt(Pack->Territory.X));
		O->SetNumberField(TEXT("ty"), FMath::RoundToInt(Pack->Territory.Y));
		O->SetBoolField(TEXT("wander"), Pack->bWanderer);
		O->SetNumberField(TEXT("next"), Pack->NextThinkAt);
		O->SetNumberField(TEXT("fed"), Pack->FedUntil);
		List.Add(MakeShared<FJsonValueObject>(O));
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("clock"), G.Clock);
	Root->SetArrayField(TEXT("packs"), List);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Dir, TEXT("wildlife.json")));
}
