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
				// A shore creature (the Floeback: cooks/build-taming.md 2.5) lives only on the ground by the sea.
				if (Def.Habitat == TEXT("shore") && C != TEXT('w'))
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

	/**
	 * A part of the island with its own creatures and how many of them it should have (Data/spawn_regions.json): a side of
	 * the island and an elevation band of the walk map, optionally cut down to a box.
	 */
	struct FRegion
	{
		FName Id;
		bool bVolcanic = true;
		FName Band;
		FBox2D Area = FBox2D(ForceInit);   // not valid: the whole band
		float Target = 0.f;                // animals, before the world's creature numbers
		TArray<TPair<const FWRCreatureDef*, float>> Kinds;   // each with its weight: the chance a new pack is of it
		float TotalWeight = 0.f;
		TMap<FName, TArray<int32>> CellsOf;   // where each kind can start in it
	};

	/** How far a new pack of each kind is spread when it comes out (spawn_regions.json "packRadius", cm). */
	TMap<FName, float> PackRadii;
	float DefaultPackRadius = 400.f;

	float RadiusOf(const FWRCreatureDef& Def)
	{
		const float* Found = PackRadii.Find(Def.Id);
		return Found ? *Found : DefaultPackRadius;
	}

	/** This game's packs. One game runs at a time in a process; Start begins afresh. */
	struct FState
	{
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AWRCreatureSpawner>> Packs;
		TArray<FRegion> Regions;
		TArray<const AWRClaimTotem*> Claims;
		double Clock = 0.0;          // seconds of wildlife time (saved)
		double CheckOwed = 0.0;      // wildlife seconds since the last look at the regions
		double NextLogAt = 0.0;
		int32 Caught = 0, Missed = 0, NewPacks = 0, Retired = 0, Moved = 0;
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

	/** How many animals a region should have just now: its target times the world's creature numbers. */
	float TargetOf(const FRegion& R) { return R.Target * CreatureNumbers(); }

	/** About how many of a kind the island should have: its share of each region's target, by weight and pack size. */
	float TargetFor(const FWRCreatureDef& Def)
	{
		float Total = 0.f;
		for (const FRegion& R : G.Regions)
		{
			float Mine = 0.f, All = 0.f;
			for (const TPair<const FWRCreatureDef*, float>& K : R.Kinds)
			{
				const float Share = K.Value * MeanPack(*K.Key);
				All += Share;
				if (K.Key == &Def) Mine += Share;
			}
			if (All > 0.f) Total += TargetOf(R) * Mine / All;
		}
		return Total;
	}

	int32 AnimalsIn(const FRegion& R)
	{
		int32 Animals = 0;
		for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs)
		{
			if (P.IsValid() && P->Region == R.Id) Animals += P->Count;
		}
		return Animals;
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

	// ------------------------------------------------------------ the regions

	/** Is this cell centre in the region (its side, its band, its box)? */
	bool InRegion(const FRegion& R, const FVector2D& At)
	{
		const FVector2D* Band = Map().Bands.Find(R.Band);
		if (R.bVolcanic != (At.X < 0.0) || (Band && (FMath::Abs(At.X) < Band->X || FMath::Abs(At.X) > Band->Y)))
		{
			return false;
		}
		return !R.Area.bIsValid || R.Area.IsInside(At);
	}

	/** Data/spawn_regions.json: each region's place, its weighted creatures and its target. False: nothing to spawn from. */
	bool LoadRegions()
	{
		const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("spawn_regions.json"));
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("regions"), List))
		{
			UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE no Data/spawn_regions.json: no wildlife"));
			return false;
		}
		PackRadii.Reset();
		DefaultPackRadius = 400.f;
		const TSharedPtr<FJsonObject>* Radii = nullptr;
		if (Root->TryGetObjectField(TEXT("packRadius"), Radii))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Radii)->Values)
			{
				if (KV.Key == TEXT("default")) DefaultPackRadius = static_cast<float>(KV.Value->AsNumber());
				else if (!KV.Key.StartsWith(TEXT("_"))) PackRadii.Add(FName(*KV.Key), static_cast<float>(KV.Value->AsNumber()));
			}
		}
		FWalkMap& M = Map();
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid()) continue;
			FRegion R;
			R.Id = FName(*O->GetStringField(TEXT("id")));
			R.bVolcanic = O->GetStringField(TEXT("side")) != TEXT("tundra");
			R.Band = FName(*O->GetStringField(TEXT("band")));
			R.Target = O->GetNumberField(TEXT("target"));
			const TArray<TSharedPtr<FJsonValue>>* Box = nullptr;
			if (O->TryGetArrayField(TEXT("area"), Box) && Box->Num() == 4)
			{
				// Metres, as the map's bands are.
				R.Area = FBox2D(FVector2D((*Box)[0]->AsNumber() * 100.0, (*Box)[1]->AsNumber() * 100.0), FVector2D((*Box)[2]->AsNumber() * 100.0, (*Box)[3]->AsNumber() * 100.0));
			}
			const TSharedPtr<FJsonObject>* Weights = nullptr;
			if (O->TryGetObjectField(TEXT("creatures"), Weights))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Weights)->Values)
				{
					const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*KV.Key));
					const float Weight = static_cast<float>(KV.Value->AsNumber());
					if (!Def || Weight <= 0.f)
					{
						UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE region %s: no creature %s (or no weight)"), *R.Id.ToString(), *KV.Key);
						continue;
					}
					// Where it can start here: ground its kind can stand on that's in the region.
					TArray<int32>& Cells = R.CellsOf.Add(Def->Id);
					if (M.N > 0)
					{
						for (const int32 Cell : M.RangeOf(*Def).All)
						{
							if (InRegion(R, M.Centre(Cell))) Cells.Add(Cell);
						}
					}
					if (Cells.Num() == 0)
					{
						UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE region %s: %s can't live anywhere in it (its side and band are %s %s)"), *R.Id.ToString(), *KV.Key, *Def->Biome.ToString(), *Def->Tier.ToString());
						R.CellsOf.Remove(Def->Id);
						continue;
					}
					R.Kinds.Add({ Def, Weight });
					R.TotalWeight += Weight;
				}
			}
			if (R.Kinds.Num() == 0)
			{
				UE_LOG(LogWildreach, Warning, TEXT("WILDLIFE region %s has nothing that can live in it"), *R.Id.ToString());
				continue;
			}
			G.Regions.Add(MoveTemp(R));
		}
		return G.Regions.Num() > 0;
	}

	/** Which region a pack from an older save belongs to: one that has its kind, where it is, or failing that anywhere. */
	FName RegionFor(const FWRCreatureDef& Def, const FVector2D& At)
	{
		FName Anywhere;
		for (const FRegion& R : G.Regions)
		{
			if (!R.CellsOf.Contains(Def.Id)) continue;
			if (InRegion(R, At)) return R.Id;
			if (Anywhere.IsNone()) Anywhere = R.Id;
		}
		return Anywhere;
	}

	double NearestOf(const TArray<FVector2D>& Points, const FVector2D& At)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FVector2D& P : Points) Best = FMath::Min(Best, FVector2D::Distance(P, At));
		return Best;
	}

	/**
	 * One new pack of the region's, its kind picked by weight, somewhere it can start: away from every player (so nobody
	 * sees it appear) and every building, out of the land claims. Null if nowhere was found this time.
	 */
	AWRCreatureSpawner* SpawnIn(UWorld* World, const FRegion& R, const TArray<FVector2D>& Players, const TArray<FVector2D>& Buildings)
	{
		float Roll = FMath::FRand() * R.TotalWeight;
		const FWRCreatureDef* Def = R.Kinds.Last().Key;
		for (const TPair<const FWRCreatureDef*, float>& K : R.Kinds)
		{
			if ((Roll -= K.Value) <= 0.f) { Def = K.Key; break; }
		}
		const TArray<int32>& Cells = R.CellsOf.FindChecked(Def->Id);
		const double AwayPlayers = WRTune(TEXT("wildlife.spawnAwayFromPlayers"), 30000.f);
		const double AwayBuildings = WRTune(TEXT("wildlife.spawnAwayFromBuildings"), 10000.f);
		FWalkMap& M = Map();
		for (int32 Try = 0; Try < 40; ++Try)
		{
			const FVector2D At = M.Centre(Cells[FMath::RandHelper(Cells.Num())]);
			const double ToPlayer = NearestOf(Players, At), ToBuilding = NearestOf(Buildings, At);
			if (ToPlayer < AwayPlayers || ToBuilding < AwayBuildings || InAClaim(At))
			{
				continue;
			}
			const int32 Count = RollPack(*Def, FMath::FRand());
			// (Its height doesn't matter: the ground under each animal is found as it comes out.)
			AWRCreatureSpawner* Pack = MakePack(World, *Def, FVector(At.X, At.Y, 0.f), Count, RadiusOf(*Def));
			if (Pack)
			{
				Pack->Region = R.Id;
				UE_LOG(LogWildreach, Log, TEXT("WILDLIFE spawned %s x%d in %s at %.0f %.0f: %d of %.0f there now; %s, %s"), *Def->Id.ToString(), Count,
					*R.Id.ToString(), At.X, At.Y, AnimalsIn(R), TargetOf(R),
					*(Players.Num() ? FString::Printf(TEXT("%.0f m from a player"), ToPlayer / 100.0) : FString(TEXT("no players"))),
					*(Buildings.Num() ? FString::Printf(TEXT("%.0f m from a building"), ToBuilding / 100.0) : FString(TEXT("nothing built"))));
			}
			return Pack;
		}
		return nullptr;
	}

	/** "WILDLIFE regions: volcanic_lowland 131/133, ...": each region's animals against its target. */
	void LogRegions()
	{
		TArray<FString> Each;
		for (const FRegion& R : G.Regions) Each.Add(FString::Printf(TEXT("%s %d/%.0f"), *R.Id.ToString(), AnimalsIn(R), TargetOf(R)));
		UE_LOG(LogWildreach, Log, TEXT("WILDLIFE regions: %s"), *FString::Join(Each, TEXT(", ")));
	}

	/**
	 * The periodic check: a region short of its target gets new packs (a few at a time: wildlife.spawnPacksPerCheck); one
	 * over it (the world asked for fewer) loses an animal from its smallest pack out of sight.
	 */
	void CheckRegions(UWorld* World, bool bFillUp)
	{
		TArray<FVector2D> Players, Buildings;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr) Players.Add(FVector2D(Pawn->GetActorLocation()));
		}
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			if (!It->bGhost) Buildings.Add(FVector2D(It->GetActorLocation()));
		}
		const int32 PerCheck = bFillUp ? 100000 : FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("wildlife.spawnPacksPerCheck"), 2.f)));
		for (const FRegion& R : G.Regions)
		{
			const float Target = TargetOf(R);
			int32 Animals = AnimalsIn(R);
			for (int32 Made = 0, Fails = 0; Made < PerCheck && Animals < Target - 0.5f && Fails < 3;)
			{
				if (AWRCreatureSpawner* Pack = SpawnIn(World, R, Players, Buildings))
				{
					Animals += Pack->Count;
					++Made;
					if (!bFillUp) ++G.NewPacks;
				}
				else ++Fails;   // nowhere this time (players all over it, say): next check
			}
			if (!bFillUp && Animals > Target + 1.f)
			{
				AWRCreatureSpawner* Smallest = nullptr;
				for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs)
				{
					if (P.IsValid() && P->Region == R.Id && !P->IsActive() && P->Count > 0 && (!Smallest || P->Count < Smallest->Count)) Smallest = P.Get();
				}
				if (Smallest)
				{
					Smallest->Count--;
					++G.Retired;
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

FName WRWildlife::RaidersAt(const FVector2D& At, TArray<TPair<FName, float>>& OutRaiders)
{
	using namespace WRWildlifeImpl;
	OutRaiders.Reset();
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("spawn_regions.json"));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("regions"), List))
	{
		return NAME_None;
	}
	// The region it's in has no gap; otherwise the nearest band on its side (a region's box counts as far off).
	const FWalkMap& M = Map();
	const double Across = FMath::Abs(At.X);
	TSharedPtr<FJsonObject> Best;
	double BestGap = TNumericLimits<double>::Max();
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
		if (!O.IsValid() || (O->GetStringField(TEXT("side")) != TEXT("tundra")) != (At.X < 0.0))
		{
			continue;
		}
		const FVector2D* Band = M.Bands.Find(FName(*O->GetStringField(TEXT("band"))));
		double Gap = !Band ? 0.0 : Across < Band->X ? Band->X - Across : Across > Band->Y ? Across - Band->Y : 0.0;
		const TArray<TSharedPtr<FJsonValue>>* Box = nullptr;
		if (O->TryGetArrayField(TEXT("area"), Box) && Box->Num() == 4
			&& !FBox2D(FVector2D((*Box)[0]->AsNumber() * 100.0, (*Box)[1]->AsNumber() * 100.0), FVector2D((*Box)[2]->AsNumber() * 100.0, (*Box)[3]->AsNumber() * 100.0)).IsInside(At))
		{
			Gap += 1.0e7;
		}
		if (Gap < BestGap)
		{
			BestGap = Gap;
			Best = O;
		}
	}
	if (!Best.IsValid())
	{
		return NAME_None;
	}
	const TSharedPtr<FJsonObject>* Weights = nullptr;
	const bool bListed = Best->TryGetObjectField(TEXT("raiders"), Weights);
	if (!bListed) Best->TryGetObjectField(TEXT("creatures"), Weights);
	if (Weights)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Weights)->Values)
		{
			const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(FName(*KV.Key));
			const float Weight = static_cast<float>(KV.Value->AsNumber());
			if (Def && Weight > 0.f && (bListed || (Def->Behavior == TEXT("aggressive") && Def->Tier != TEXT("boss"))))
			{
				OutRaiders.Add({ Def->Id, Weight });
			}
		}
	}
	return FName(*Best->GetStringField(TEXT("id")));
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
	// The regions are laid out on the walk map: the island it was made for, or none.
	if (Map().N == 0 || Map().MapName != UWorld::RemovePIEPrefix(World->GetMapName()))
	{
		return 0;
	}
	G.World = World;
	if (!LoadRegions())
	{
		return 0;
	}
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It) G.Claims.Add(*It);

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
				// A herd bigger than its kind is now (an older save, packs of up to 12): what's over the top has gone.
				const int32 Count = FMath::Min(O->GetIntegerField(TEXT("n")), Def->PackMax);
				AWRCreatureSpawner* Pack = MakePack(World, *Def, At, Count, O->GetNumberField(TEXT("r")));
				if (!Pack)
				{
					continue;
				}
				FString Region;
				Pack->Region = O->TryGetStringField(TEXT("region"), Region) && G.Regions.ContainsByPredicate([&Region](const FRegion& R) { return R.Id == FName(*Region); })
					? FName(*Region) : RegionFor(*Def, FVector2D(At));
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
		// A new world: every region filled to its target, its kinds by weight, clear of anything built.
		CheckRegions(World, true);
	}

	int32 Animals = 0;
	for (const TWeakObjectPtr<AWRCreatureSpawner>& P : G.Packs) if (P.IsValid()) Animals += P->Count;
	float Target = 0.f;
	for (const FRegion& R : G.Regions) Target += TargetOf(R);
	UE_LOG(LogWildreach, Log, TEXT("WILDLIFE %d packs, %d animals, %s; the island's share is %.0f; a pack comes out when a player is within %.0f m"),
		G.Packs.Num(), Animals, bLoaded ? TEXT("as the world left them") : TEXT("new"), Target, WRTune(TEXT("wildlife.activationRadius"), 25000.f) / 100.f);
	LogRegions();
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
		// Wiped out: gone, once nobody's looking. (Its region makes up the numbers with new packs elsewhere.)
		if (Pack->Count <= 0 && !Pack->IsActive() && Pack->NumAlive() == 0)
		{
			Pack->Destroy();
			continue;
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

	// The periodic check: regions short of their target get new packs, out of everyone's sight.
	G.CheckOwed += Seconds;
	if (G.CheckOwed >= WRTune(TEXT("wildlife.spawnCheckSeconds"), 60.f))
	{
		G.CheckOwed = 0.0;
		CheckRegions(World, false);
	}

	G.StepMs = static_cast<float>((FPlatformTime::Seconds() - Began) * 1000.0);
	const double Now = World->GetTimeSeconds();
	if (Now >= G.NextLogAt)
	{
		G.NextLogAt = Now + WRTune(TEXT("wildlife.logSeconds"), 300.f);
		float Target = 0.f;
		for (const FRegion& R : G.Regions) Target += TargetOf(R);
		UE_LOG(LogWildreach, Log, TEXT("WILDLIFE step: %d packs (%d in view), %d animals of %.0f; %d travelling, %d grazing, %d hunting, %d resting; since last: %d caught, %d missed, %d new packs, %d thinned; %.2f ms"),
			G.Packs.Num(), Active, Animals, Target, Travelling, Grazing, Hunting, Resting, G.Caught, G.Missed, G.NewPacks, G.Retired, G.StepMs);
		LogRegions();
		G.Caught = G.Missed = G.NewPacks = G.Retired = 0;
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
		O->SetStringField(TEXT("region"), Pack->Region.ToString());
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
