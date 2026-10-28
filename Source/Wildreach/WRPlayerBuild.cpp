// AWRPlayerController: build mode (client ghost + server-validated placement), chest/claim menus,
// code locks and demolish.

#include "WRPlayerController.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRLifeSkills.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRPlayerUI.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------- free placement (snap off)
// With snap off (F) a structure piece goes where you aim instead of onto the grid. The grid's own rules (one piece per
// slot) can't see a piece that's off the grid, so a free piece is also checked box against box: it mustn't sink into
// another structure piece. Two pieces on the grid keep the slot rules alone (walls meeting at a corner overlap by
// design). Client and server run the same checks; the server also treats any off-grid location as free.
namespace WRFreeBuild
{
	/** The solid box of a structure piece around its location (cm, before yaw): AWRBuildPiece::ApplyLook's blocks. */
	static FBox PieceBox(const FWRPieceDef& Def)
	{
		const FName S = Def.Shape;
		if (S == TEXT("foundation")) return FBox(FVector(-200, -200, -150), FVector(200, 200, 100));
		if (S == TEXT("floor")) return FBox(FVector(-200, -200, 0), FVector(200, 200, 20));
		if (S == TEXT("roof")) return FBox(FVector(-240, -210, 0), FVector(210, 210, 215));
		if (S == TEXT("pillar")) return FBox(FVector(-20, -20, 0), FVector(20, 20, 300));
		if (S == TEXT("stairs")) return FBox(FVector(-250, -104, 0), FVector(250, 104, 300));
		if (S == TEXT("halfwall")) return FBox(FVector(-210, -10, 0), FVector(210, 10, 150));
		if (S == TEXT("upperwall") || S == TEXT("gable")) return FBox(FVector(-210, -10, 0), FVector(210, 10, 210));
		if (S == TEXT("palisade")) return FBox(FVector(-220, -20, 0), FVector(220, 20, 400));
		if (S == TEXT("gate")) return FBox(FVector(-220, -10, 0), FVector(220, 10, 400));
		if (S == TEXT("tower")) return FBox(FVector(-100, -100, 0), FVector(100, 100, 520));
		return FBox(FVector(-210, -10, 0), FVector(210, 10, 300));   // wall, door, doorway, window
	}

	/** On the build grid: where snapping would have put it, square to the grid. */
	static bool IsOnGrid(const FWRPieceDef& Def, const FVector& Loc, float Yaw)
	{
		float SnapYaw = Yaw;
		const FVector Snapped = FWRBuildRules::SnapToGrid(Def.Shape, Loc, Loc.Z, FWRBuildRegistry::Get().GridSize, SnapYaw);
		const float Turn = FMath::Fmod(FMath::Abs(Yaw), 90.f);
		return FVector::Dist2D(Snapped, Loc) < 2.f && FMath::Min(Turn, 90.f - Turn) < 1.f;
	}

	/** Two yawed boxes interpenetrate (by more than Slack: touching faces and float noise don't count). */
	static bool Overlap(const FBox& A, const FVector& LocA, float YawA, const FBox& B, const FVector& LocB, float YawB, float Slack = 5.f)
	{
		const auto Shrink = [Slack](float Min, float Max, float& OutMin, float& OutMax)
		{
			const float Mid = (Min + Max) * 0.5f, Half = FMath::Max(1.f, (Max - Min) * 0.5f - Slack);
			OutMin = Mid - Half;
			OutMax = Mid + Half;
		};
		float AZ0, AZ1, BZ0, BZ1;
		Shrink(LocA.Z + A.Min.Z, LocA.Z + A.Max.Z, AZ0, AZ1);
		Shrink(LocB.Z + B.Min.Z, LocB.Z + B.Max.Z, BZ0, BZ1);
		if (AZ1 <= BZ0 || BZ1 <= AZ0) return false;
		// Separating axes in plan: each box's own two axes.
		struct FRect { FVector2D C, X, Y; float HX = 0.f, HY = 0.f; };
		const auto Rect = [&Shrink](const FBox& Box, const FVector& Loc, float Yaw)
		{
			FRect R;
			float X0, X1, Y0, Y1;
			Shrink(Box.Min.X, Box.Max.X, X0, X1);
			Shrink(Box.Min.Y, Box.Max.Y, Y0, Y1);
			const float Rad = FMath::DegreesToRadians(Yaw);
			R.X = FVector2D(FMath::Cos(Rad), FMath::Sin(Rad));
			R.Y = FVector2D(-R.X.Y, R.X.X);
			R.C = FVector2D(Loc.X, Loc.Y) + R.X * ((X0 + X1) * 0.5f) + R.Y * ((Y0 + Y1) * 0.5f);
			R.HX = (X1 - X0) * 0.5f;
			R.HY = (Y1 - Y0) * 0.5f;
			return R;
		};
		const FRect RA = Rect(A, LocA, YawA), RB = Rect(B, LocB, YawB);
		const FVector2D D = RB.C - RA.C;
		for (const FVector2D& Axis : { RA.X, RA.Y, RB.X, RB.Y })
		{
			const float PA = RA.HX * FMath::Abs(FVector2D::DotProduct(RA.X, Axis)) + RA.HY * FMath::Abs(FVector2D::DotProduct(RA.Y, Axis));
			const float PB = RB.HX * FMath::Abs(FVector2D::DotProduct(RB.X, Axis)) + RB.HY * FMath::Abs(FVector2D::DotProduct(RB.Y, Axis));
			if (FMath::Abs(FVector2D::DotProduct(D, Axis)) >= PA + PB) return false;
		}
		return true;
	}

	/** Why a structure piece can't go here because it would sink into another one; empty = it fits. */
	static FString OverlapWhy(UWorld* World, const FWRPieceDef& Def, const FVector& Loc, float Yaw, bool bFree)
	{
		const FBox Mine = PieceBox(Def);
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* Other = It->GetDef();
			if (It->bGhost || !Other || !Other->Snaps() || It->IsActorBeingDestroyed())
			{
				continue;
			}
			const float OtherYaw = It->GetActorRotation().Yaw;
			if (!bFree && IsOnGrid(*Other, It->GetActorLocation(), OtherYaw))
			{
				continue;
			}
			if (Overlap(Mine, Loc, Yaw, PieceBox(*Other), It->GetActorLocation(), OtherYaw))
			{
				return FString::Printf(TEXT("overlaps the %s"), *Other->Name.ToLower());
			}
		}
		return FString();
	}

	/** A piece's cost for this player (the Thrifty perk makes building cheaper). */
	static TArray<TPair<FName, int32>> CostFor(const AActor* Player, const FWRPieceDef& Def)
	{
		const float Mult = FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(Player, TEXT("buildCost")));
		TArray<TPair<FName, int32>> Out;
		for (const TPair<FName, int32>& C : Def.Cost) Out.Add({ C.Key, FMath::Max(1, FMath::CeilToInt(C.Value * Mult)) });
		return Out;
	}
}

void AWRPlayerController::WRPlace(FName Piece, float X, float Y, float Z, float Yaw)
{
	ServerCommand(FString::Printf(TEXT("place %s %.0f %.0f %.0f %.0f"), *Piece.ToString(), X, Y, Z, Yaw));
}
void AWRPlayerController::WRSetCode(const FString& Code) { ServerCommand(FString::Printf(TEXT("setcode %s"), *Code)); }
void AWRPlayerController::WRCode(const FString& Code) { ServerCommand(FString::Printf(TEXT("code %s"), *Code)); }
void AWRPlayerController::WRDemolish(float X, float Y, float Z)
{
	ServerCommand(X == 0.f && Y == 0.f && Z == 0.f ? FString(TEXT("demolish")) : FString::Printf(TEXT("demolish %.0f %.0f %.0f"), X, Y, Z));
}
void AWRPlayerController::WRGhost(FName Piece) { StartPlacing(Piece); }
void AWRPlayerController::WRGhostPlace()
{
	UE_LOG(LogWildreach, Log, TEXT("GHOST %s at %s yaw %.0f %s %s"), *PlacingPiece.ToString(), *PlaceLoc.ToCompactString(), PlaceYaw,
		bPlaceValid ? TEXT("valid") : TEXT("invalid"), *PlaceWhy);
	ConfirmPlacement();
	CancelPlacement();
}
void AWRPlayerController::WRHitPiece(int32 Hits) { ServerCommand(FString::Printf(TEXT("hitpiece %d"), Hits)); }

AWRBuildPiece* AWRPlayerController::FindNearbyPiece(float Range, bool bUsableOnly) const
{
	const APawn* Self = GetPawn();
	if (!Self)
	{
		return nullptr;
	}
	AWRBuildPiece* Best = nullptr;
	float BestDist = Range;
	for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
	{
		const FWRPieceDef* Def = It->GetDef();
		if (It->bGhost || !Def || (bUsableOnly && (Def->Kind == TEXT("structure") || Def->Kind == TEXT("decor") || Def->Kind == TEXT("station"))))
		{
			continue;
		}
		// Measure to the piece's middle so tall walls/doors count from either side.
		const float D = FVector::Dist(It->GetActorLocation() + FVector(0.f, 0.f, 100.f), Self->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

void AWRPlayerController::ToggleBuildMenu()
{
	if (IsPlacing())
	{
		// The new UI keeps its build bar up while you place: B leaves build mode altogether.
		CancelPlacement();
		if (Menu == EWRMenu::Build) MenuClose();
		return;
	}
	Menu = Menu == EWRMenu::Build ? EWRMenu::None : EWRMenu::Build;
	MenuIndex = 0;
}

/** A local, collision-free preview of a piece. */
static AWRBuildPiece* SpawnGhost(UWorld* World, FName Piece)
{
	AWRBuildPiece* G = World->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (G)
	{
		G->bGhost = true;
		G->SetReplicates(false);
		G->PieceId = Piece;
		G->FinishSpawning(FTransform::Identity);
		G->SetActorEnableCollision(false);
	}
	return G;
}

void AWRPlayerController::StartPlacing(FName Piece)
{
	CancelPlacement();
	// The new UI's build bar stays up while you place (it shows the piece and the keys); the old text menu closes.
	if (!(UI && Menu == EWRMenu::Build && FWRScreens::Find(TEXT("build"))))
	{
		MenuClose();
	}
	PlacingPiece = Piece;
	PlaceYaw = GetControlRotation().Yaw;
	Ghost = SpawnGhost(GetWorld(), Piece);  // "demolish": a red copy of whatever you aim at, made in UpdatePlacement
}

void AWRPlayerController::CancelPlacement()
{
	PlacingPiece = NAME_None;
	if (AWRBuildPiece* G = Ghost.Get())
	{
		G->Destroy();
	}
	Ghost = nullptr;
}

void AWRPlayerController::RotatePlacement()
{
	// On the grid a quarter turn; placed freely (snap off, or a piece that never snaps) an eighth.
	const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PlacingPiece);
	PlaceYaw += Def && Def->Snaps() && bSnap ? 90.f : 45.f;
}

void AWRPlayerController::ConfirmPlacement()
{
	if (PlacingPiece == TEXT("demolish"))
	{
		if (bPlaceValid)
		{
			ServerCommand(FString::Printf(TEXT("demolish %.0f %.0f %.0f"), PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z));
		}
	}
	else if (IsPlacing())
	{
		// A structure piece placed with snap off says so: the server checks it box against box, not by grid slot.
		const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PlacingPiece);
		ServerCommand(FString::Printf(TEXT("place %s %.0f %.0f %.0f %.0f%s"), *PlacingPiece.ToString(), PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z, PlaceYaw,
			Def && Def->Snaps() && !bSnap ? TEXT(" free") : TEXT("")));
	}
}

void AWRPlayerController::UpdatePlacement()
{
	const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PlacingPiece);
	AWRBuildPiece* G = Ghost.Get();
	const APawn* Self = GetPawn();
	if ((!Def && PlacingPiece != TEXT("demolish")) || !G || !Self || !PlayerCameraManager)
	{
		return;
	}

	// Aim from the camera.
	const FVector Eye = PlayerCameraManager->GetCameraLocation();
	const FVector Dir = PlayerCameraManager->GetCameraRotation().Vector();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRBuildAim), false, Self);
	Query.AddIgnoredActor(G);
	FHitResult Hit;
	FVector Point = Eye + Dir * 1000.f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Query);
	if (bHit)
	{
		Point = Hit.ImpactPoint;
	}

	if (!Def)
	{
		// Demolish: the piece under the crosshair (or the station / totem on it) gets a red copy over it; LMB removes it.
		AActor* HitActor = Hit.GetActor();
		AWRBuildPiece* Target = Cast<AWRBuildPiece>(HitActor);
		Target = Target ? Target : HitActor ? Cast<AWRBuildPiece>(HitActor->GetOwner()) : nullptr;
		const FName Aimed = Target ? Target->PieceId : FName(TEXT("demolish"));
		if (G->PieceId != Aimed)
		{
			G->Destroy();
			G = SpawnGhost(GetWorld(), Aimed);
			Ghost = G;
		}
		const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
		bPlaceValid = Target && PS && Target->OwnerName == PS->GetPlayerName() && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < 1100.f;
		PlaceWhy = !Target ? TEXT("aim at a piece you built") : bPlaceValid ? TEXT("") : Target->OwnerName != (PS ? PS->GetPlayerName() : FString()) ? TEXT("not yours") : TEXT("too far");
		PlaceLoc = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		if (G && Target)
		{
			G->SetActorTransform(FTransform(Target->GetActorRotation(), PlaceLoc, FVector(1.02f)));  // a hair bigger, so it shows over the piece
			G->SetGhostValid(false);
		}
		return;
	}

	const bool bFreeStructure = Def->Snaps() && !bSnap;
	if (Def->Snaps() && bSnap)
	{
		// Storey heights come from the nearest foundation's top surface.
		float BaseZ = Point.Z;
		const bool bGroundShape = Def->Shape == TEXT("foundation") || Def->Shape == TEXT("palisade") || Def->Shape == TEXT("tower") || Def->Shape == TEXT("gate");
		if (!bGroundShape)
		{
			const AWRBuildPiece* Foundation = nullptr;
			float Best = 2500.f;
			for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
			{
				const FWRPieceDef* O = It->GetDef();
				const float D = FVector::Dist2D(It->GetActorLocation(), Point);
				if (!It->bGhost && O && O->Shape == TEXT("foundation") && D < Best)
				{
					Best = D;
					Foundation = *It;
				}
			}
			if (Foundation)
			{
				// The storey you're looking into; floors, roofs and gables go on top of it.
				const float Top = Foundation->GetActorLocation().Z + 100.f;
				BaseZ = Top + FWRBuildRules::LevelFor(Def->Shape, Point.Z, Top) * FWRBuildRules::Storey;
			}
		}
		else if (Def->Shape == TEXT("foundation"))
		{
			// Level with a foundation next to this cell (so floors and walls line up); else on the ground at the cell's
			// centre, sunk in so the floor is a step up instead of a waist-high ledge.
			const float Grid = FWRBuildRegistry::Get().GridSize;
			float CellYaw = 0.f;
			const FVector Cell = FWRBuildRules::SnapToGrid(Def->Shape, Point, Point.Z, Grid, CellYaw);
			const AWRBuildPiece* Next = nullptr;
			float NextDist = Grid * 1.05f;
			for (TActorIterator<AWRBuildPiece> It(GetWorld()); It; ++It)
			{
				const FWRPieceDef* O = It->GetDef();
				const float D = FVector::Dist2D(It->GetActorLocation(), Cell);
				if (!It->bGhost && O && O->Shape == TEXT("foundation") && D > 10.f && D < NextDist && FMath::Abs(It->GetActorLocation().Z - Point.Z) < 300.f)
				{
					NextDist = D;
					Next = *It;
				}
			}
			FHitResult Ground;
			if (Next)
			{
				BaseZ = Next->GetActorLocation().Z;
			}
			else if (GetWorld()->LineTraceSingleByChannel(Ground, FVector(Cell.X, Cell.Y, Point.Z + 300.f), FVector(Cell.X, Cell.Y, Point.Z - 600.f), ECC_WorldStatic, Query)
				&& !Cast<AWRBuildPiece>(Ground.GetActor()))
			{
				BaseZ = Ground.ImpactPoint.Z - WRTune(TEXT("building.foundationSink"), 60.f);
			}
		}
		float Yaw = PlaceYaw;
		PlaceLoc = FWRBuildRules::SnapToGrid(Def->Shape, Point, BaseZ, FWRBuildRegistry::Get().GridSize, Yaw);
		G->SetActorLocationAndRotation(PlaceLoc, FRotator(0.f, Yaw, 0.f));
		PlaceYaw = Yaw;  // send the snapped yaw (stairs, roofs and gables keep the way R turned them)
	}
	else
	{
		PlaceLoc = Point;
		if (bFreeStructure)
		{
			// Snap off: where you aim, a foundation sunk into the ground as on the grid. Whole centimetres and degrees,
			// as the place command carries them, so the server checks exactly the spot you see.
			if (Def->Shape == TEXT("foundation") && bHit && !Cast<AWRBuildPiece>(Hit.GetActor()))
			{
				PlaceLoc.Z -= WRTune(TEXT("building.foundationSink"), 60.f);
			}
			PlaceLoc = FVector(FMath::RoundToFloat(PlaceLoc.X), FMath::RoundToFloat(PlaceLoc.Y), FMath::RoundToFloat(PlaceLoc.Z));
			PlaceYaw = FMath::RoundToFloat(FRotator::NormalizeAxis(PlaceYaw));
		}
		G->SetActorLocationAndRotation(PlaceLoc, FRotator(0.f, PlaceYaw, 0.f));
	}

	// Quick client-side checks (the server re-checks everything).
	bPlaceValid = true;
	PlaceWhy.Reset();
	const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	if (bFreeStructure && !bHit)
	{
		bPlaceValid = false;
		PlaceWhy = TEXT("aim at the ground or a piece");
	}
	else if (FVector::Dist(PlaceLoc, Self->GetActorLocation()) > 1100.f)
	{
		bPlaceValid = false;
		PlaceWhy = TEXT("too far");
	}
	else if (!AWRClaimTotem::CanBuild(GetWorld(), PlaceLoc, PS ? PS->GetPlayerName() : FString(), PlaceWhy))
	{
		bPlaceValid = false;
	}
	else if (Def->Snaps() && AWRBuildPiece::SupportAt(GetWorld(), PlacingPiece, PlaceLoc) == INDEX_NONE)
	{
		// Same rules as the server (support values replicate), so red means it really won't go there.
		bPlaceValid = false;
		PlaceWhy = FString::Printf(TEXT("no support (%s spans %d)"), *Def->Material.ToString(), FWRBuildRules::SpanFor(Def->Material));
	}
	else if (const FString Blocked = AWRBuildPiece::Blocked(GetWorld(), PlacingPiece, PlaceLoc); !Blocked.IsEmpty())
	{
		bPlaceValid = false;
		PlaceWhy = Blocked;
	}
	else if (const FString Overlaps = Def->Snaps() ? WRFreeBuild::OverlapWhy(GetWorld(), *Def, PlaceLoc, PlaceYaw, bFreeStructure) : FString(); !Overlaps.IsEmpty())
	{
		bPlaceValid = false;
		PlaceWhy = Overlaps;
	}
	else if (PS)
	{
		for (const TPair<FName, int32>& C : WRFreeBuild::CostFor(this, *Def))
		{
			if (PS->GetInventory()->CountOf(C.Key) < C.Value)
			{
				bPlaceValid = false;
				PlaceWhy = FString::Printf(TEXT("need %d %s"), C.Value, *C.Key.ToString());
				break;
			}
		}
	}
	G->SetGhostValid(bPlaceValid);
}

TArray<FWRMenuEntry> AWRPlayerController::BuildPieceMenu() const
{
	TArray<FWRMenuEntry> Out;
	const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	// Things made by hand (no station) come first: the crude tools a fresh survivor starts with.
	Out.Add({ TEXT("-- Craft by hand (hold a tool to use it) --"), FString() });
	for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(TEXT("hand")))
	{
		const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(R->Output);
		FString Needs;
		bool bHave = true;
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			const FWRItemDef* InDef = FWRItemRegistry::Get().Find(In.Key);
			const int32 Have = Inv ? Inv->CountOf(In.Key) : 0;
			bHave &= Have >= In.Value;
			Needs += FString::Printf(TEXT("%s %d/%d  "), InDef ? *InDef->Name : *In.Key.ToString(), Have, In.Value);
		}
		const int32 Carried = Inv ? Inv->CountOf(R->Output) : 0;
		Out.Add({ FString::Printf(TEXT("%s%s  <-  %s"), OutDef ? *OutDef->Name : *R->Output.ToString(), Carried > 0 ? TEXT(" (have one)") : TEXT(""), *Needs),
			FString::Printf(TEXT("craft %s none"), *R->Id.ToString()), bHave });
	}
	Out.Add({ TEXT("Demolish: aim at a piece of yours, LMB removes it (half back)"), TEXT("build demolish") });
	// Pieces in pieces.json order, under a heading per group (wood / stone building, then by category).
	FString Group;
	for (const FWRPieceDef& Def : FWRBuildRegistry::Get().All())
	{
		const FString ThisGroup = Def.Category == TEXT("structure") ? Def.Material.ToString() : Def.Category.ToString();
		if (ThisGroup != Group)
		{
			Group = ThisGroup;
			const FString Heading = Group == TEXT("wood") ? TEXT("Wood building") : Group == TEXT("stone") ? TEXT("Stone building")
				: Group == TEXT("utility") ? TEXT("Crafting, storage, beds, light, farming") : Group == TEXT("claim") ? TEXT("Land claim") : Group.Left(1).ToUpper() + Group.Mid(1);
			Out.Add({ FString::Printf(TEXT("-- %s --"), *Heading), FString() });
		}
		FString Cost;
		bool bHave = true;
		for (const TPair<FName, int32>& C : Def.Cost)
		{
			const int32 Have = Inv ? Inv->CountOf(C.Key) : 0;
			bHave &= Have >= C.Value;
			Cost += FString::Printf(TEXT("%s %d/%d  "), *C.Key.ToString(), Have, C.Value);
		}
		Out.Add({ FString::Printf(TEXT("%s  <-  %s"), *Def.Name, *Cost), FString::Printf(TEXT("build %s"), *Def.Id.ToString()), bHave });
	}
	return Out;
}

bool AWRPlayerController::RunBuildCommand(const TArray<FString>& Args, FString& OutResult)
{
	AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	APawn* Self = GetPawn();
	if (!PS || Args.Num() == 0)
	{
		return false;
	}
	const FString& Cmd = Args[0];
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString Who = PS->GetPlayerName();
	UWRInventoryComponent* Inv = PS->GetInventory();

	if (Cmd == TEXT("place"))
	{
		const FName PieceId(*Arg(1));
		const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PieceId);
		const FVector Loc(FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)), FCString::Atof(*Arg(4)));
		const float Yaw = FCString::Atof(*Arg(5));
		// Snap off (the client says "free"), or anything off the grid: checked box against box, not only by grid slot.
		const bool bFree = Def && Def->Snaps() && (Arg(6) == TEXT("free") || !WRFreeBuild::IsOnGrid(*Def, Loc, Yaw));
		OutResult.Reset();
		if (!Def || !Self) OutResult = TEXT("unknown piece");
		else if (FVector::Dist(Loc, Self->GetActorLocation()) > 1300.f) OutResult = TEXT("too far away");
		else if (!AWRClaimTotem::CanBuild(GetWorld(), Loc, Who, OutResult)) {}
		else if (Def->Kind == TEXT("totem"))
		{
			for (TActorIterator<AWRClaimTotem> It(GetWorld()); It; ++It)
			{
				if (FVector::Dist2D(It->GetActorLocation(), Loc) < It->Radius + FWRBuildRegistry::Get().ClaimRadius) OutResult = TEXT("too close to another land claim");
			}
		}
		else if (Def->Kind == TEXT("hearth"))
		{
			const AWRClaimTotem* Claim = AWRClaimTotem::FindAt(GetWorld(), Loc);
			if (!Claim || Claim->OwnerName != Who) OutResult = TEXT("a Hearth must be inside your own land claim");
			else if (AWRHearth::FindCovering(GetWorld(), Loc)) OutResult = TEXT("this claim already has a Hearth");
		}
		int32 Support = 0;
		if (OutResult.IsEmpty() && Def->Snaps())
		{
			Support = AWRBuildPiece::SupportAt(GetWorld(), PieceId, Loc);
			if (Support == INDEX_NONE) OutResult = FString::Printf(TEXT("no support (%s spans %d)"), *Def->Material.ToString(), FWRBuildRules::SpanFor(Def->Material));
			else OutResult = AWRBuildPiece::Blocked(GetWorld(), PieceId, Loc);
			if (OutResult.IsEmpty()) OutResult = WRFreeBuild::OverlapWhy(GetWorld(), *Def, Loc, Yaw, bFree);
		}
		// Thrifty perk: cheaper building.
		TMap<FName, int32> Cost;
		if (Def) for (const TPair<FName, int32>& C : WRFreeBuild::CostFor(this, *Def)) Cost.Add(C.Key, C.Value);
		if (OutResult.IsEmpty())
		{
			for (const TPair<FName, int32>& C : Cost)
			{
				if (Inv->CountOf(C.Key) < C.Value) { OutResult = FString::Printf(TEXT("need %d %s"), C.Value, *C.Key.ToString()); break; }
			}
		}
		if (OutResult.IsEmpty())
		{
			for (const TPair<FName, int32>& C : Cost) Inv->RemoveItem(C.Key, C.Value);
			UWRLifeSkillsComponent::Award(this, TEXT("building"), TEXT("place"), 8.f);
			const FTransform T(FRotator(0.f, Yaw, 0.f), Loc);
			AWRBuildPiece* Piece = GetWorld()->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Piece->InitPiece(PieceId, Who);
			Piece->Support = Support;
			Piece->FinishSpawning(T);
			{
				// A puff of dust where it settles, sized to the piece; at the piece's own height (a foundation's bounds reach
				// down into the slope, so their bottom would bury the puff).
				FVector Origin, Extent;
				Piece->GetActorBounds(false, Origin, Extent);
				if (Extent.IsNearlyZero()) { Origin = Loc; Extent = FVector(50.f); }
				WRFx::Broadcast(this, TEXT("build.place"), FVector(Origin.X, Origin.Y, Piece->GetActorLocation().Z), FRotator(0.f, Yaw, 0.f),
					FMath::Clamp(FVector2D(Extent.X, Extent.Y).Size() / 120.f, 0.4f, 3.f), TEXT("build.place"));
			}
			OutResult = FString::Printf(TEXT("Built %s"), *Def->Name);
			UE_LOG(LogWildreach, Log, TEXT("BUILD %s placed %s at %s (support %d)"), *Who, *PieceId.ToString(), *Loc.ToCompactString(), Support);
		}
	}
	else if (Cmd == TEXT("setcode") || Cmd == TEXT("code"))
	{
		AWRBuildPiece* Door = FindNearbyPiece(450.f);
		const FWRPieceDef* Def = Door ? Door->GetDef() : nullptr;
		const FString Code = Arg(1);
		if (!Def || !Def->bLockable) OutResult = TEXT("no lockable door nearby");
		else if (Code.Len() != 4 || !Code.IsNumeric()) OutResult = TEXT("codes are 4 digits");
		else if (Cmd == TEXT("setcode"))
		{
			if (!Door->CanAccess(Who)) OutResult = TEXT("not your door");
			else { Door->LockCode = Code; Door->bLocked = true; Door->UnlockedFor.Reset(); OutResult = TEXT("Code set; door locked"); }
		}
		else
		{
			if (Door->LockCode == Code) { Door->UnlockedFor.Add(Who); OutResult = TEXT("Code accepted"); }
			else OutResult = TEXT("Wrong code");
		}
	}
	else if (Cmd == TEXT("demolish"))
	{
		// demolish X Y Z: the piece standing there (demolish mode sends what you aimed at); bare: the nearest one.
		AWRBuildPiece* Piece = Args.Num() >= 4 ? nullptr : FindNearbyPiece(450.f);
		const FVector At(FCString::Atof(*Arg(1)), FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)));
		for (TActorIterator<AWRBuildPiece> It(GetWorld()); It && Args.Num() >= 4 && Self && !Piece; ++It)
		{
			if (!It->bGhost && FVector::Dist(It->GetActorLocation(), At) < 50.f && FVector::Dist(At, Self->GetActorLocation()) < 1300.f) Piece = *It;
		}
		const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
		if (!Def) OutResult = TEXT("nothing to demolish");
		else if (Piece->OwnerName != Who) OutResult = TEXT("not yours");
		else
		{
			for (const TPair<FName, int32>& C : Def->Cost) Inv->AddItem(C.Key, C.Value / 2);
			const FVector Where = Piece->GetActorLocation();
			Piece->Destroy();
			AWRBuildPiece::RecomputeSupport(GetWorld(), Where, 3000.f);
			OutResult = FString::Printf(TEXT("Demolished %s (half refunded)"), *Def->Name);
		}
	}
	else if (Cmd == TEXT("craft") && FWRRecipeRegistry::Get().Find(FName(*Arg(1))) && FWRRecipeRegistry::Get().Find(FName(*Arg(1)))->Station == TEXT("hand"))
	{
		// Crafting by hand from this menu (station recipes go on to the station in front of you).
		OutResult = FWRCrafting::CraftByHand(this, FName(*Arg(1)));
	}
	else if (Cmd == TEXT("claimdeposit") || Cmd == TEXT("claimupgrade"))
	{
		AWRClaimTotem* Totem = Self ? AWRClaimTotem::FindAt(GetWorld(), Self->GetActorLocation()) : nullptr;
		if (!Totem) OutResult = TEXT("not inside a land claim");
		else if (Cmd == TEXT("claimdeposit")) OutResult = Totem->OwnerName == Who ? Totem->Deposit(this) : TEXT("not your claim");
		else OutResult = Totem->Upgrade(this);
	}
	else if (Cmd == TEXT("store") || Cmd == TEXT("take") || Cmd == TEXT("storeall") || Cmd == TEXT("takeall"))
	{
		// store / take <slot> [count]: a stack (or part of one) between your bag and the chest you stand at;
		// storeall: every resource in your bag; takeall: everything that fits.
		// The nearest chest (what E opened): the nearest piece of any kind would be the floor or wall you stand by.
		AWRBuildPiece* Chest = nullptr;
		for (TActorIterator<AWRBuildPiece> It(GetWorld()); It && Self; ++It)
		{
			const FWRPieceDef* ItDef = It->GetDef();
			const float Dist = FVector::Dist(It->GetActorLocation() + FVector(0.f, 0.f, 100.f), Self->GetActorLocation());
			if (!It->bGhost && ItDef && ItDef->Kind == TEXT("chest") && Dist < 450.f && (!Chest || Dist < FVector::Dist(Chest->GetActorLocation() + FVector(0.f, 0.f, 100.f), Self->GetActorLocation()))) Chest = *It;
		}
		const FWRPieceDef* Def = Chest ? Chest->GetDef() : nullptr;
		const int32 Index = FCString::Atoi(*Arg(1));
		// Move up to N from one stack into a slot list; returns how many moved.
		const auto MoveStack = [](FWRItemStack& From, TArray<FWRItemStack>& To, int32 N) -> int32
		{
			const FWRItemDef* ItemDef = FWRItemRegistry::Get().Find(From.ItemId);
			if (ItemDef && ItemDef->IsEquipment() && ItemDef->StackSize <= 1)
			{
				if (!FWRInventoryRules::AddInstance(To, From)) return 0;
				From = FWRItemStack();
				return 1;
			}
			N = FMath::Clamp(N, 1, From.Count);
			const int32 Moved = N - FWRInventoryRules::Add(To, From.ItemId, N);
			From.Count -= Moved;
			if (From.Count <= 0) From = FWRItemStack();
			return Moved;
		};
		if (!Def || Def->Kind != TEXT("chest")) OutResult = TEXT("no chest nearby");
		else if (!Chest->CanAccess(Who)) OutResult = TEXT("not your chest");
		else if (Cmd == TEXT("storeall") || Cmd == TEXT("takeall"))
		{
			int32 Stacks = 0;
			if (Cmd == TEXT("storeall"))
			{
				for (FWRItemStack& S : Inv->GetSlotsMutable())
				{
					const FWRItemDef* ItemDef = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
					if (ItemDef && ItemDef->Category == TEXT("material") && MoveStack(S, Chest->Storage, S.Count) > 0) ++Stacks;
				}
			}
			else
			{
				for (FWRItemStack& S : Chest->Storage)
				{
					if (!S.IsEmpty() && MoveStack(S, Inv->GetSlotsMutable(), S.Count) > 0) ++Stacks;
				}
			}
			OutResult = Stacks > 0 ? FString::Printf(TEXT("%s %d stack%s"), Cmd == TEXT("storeall") ? TEXT("Stored") : TEXT("Took"), Stacks, Stacks == 1 ? TEXT("") : TEXT("s"))
				: Cmd == TEXT("storeall") ? TEXT("no resources to store") : TEXT("nothing taken (bag full?)");
		}
		else if (Cmd == TEXT("store"))
		{
			TArray<FWRItemStack>& Slots = Inv->GetSlotsMutable();
			if (!Slots.IsValidIndex(Index) || Slots[Index].IsEmpty()) OutResult = TEXT("empty slot");
			else OutResult = MoveStack(Slots[Index], Chest->Storage, Arg(2).IsEmpty() ? Slots[Index].Count : FCString::Atoi(*Arg(2))) > 0 ? TEXT("Stored") : TEXT("chest full");
		}
		else
		{
			if (!Chest->Storage.IsValidIndex(Index) || Chest->Storage[Index].IsEmpty()) OutResult = TEXT("empty slot");
			else OutResult = MoveStack(Chest->Storage[Index], Inv->GetSlotsMutable(), Arg(2).IsEmpty() ? Chest->Storage[Index].Count : FCString::Atoi(*Arg(2))) > 0 ? TEXT("Taken") : TEXT("inventory full");
		}
	}
	else
	{
		return false;
	}
	ClientNotify(OutResult);
	return true;
}
