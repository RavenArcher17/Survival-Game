// AWRPlayerController: build mode (client ghost + server-validated placement), chest/claim menus,
// code locks and demolish.

#include "WRPlayerController.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRBuildLook.h"
#include "WRLifeSkills.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRFx.h"
#include "WRPlayerUI.h"
#include "WRKeyBindings.h"
#include "WRSound.h"
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

	/** A piece that goes where you aim, aimed at nothing. */
	static const TCHAR* const NoAimWhy = TEXT("aim at the ground or a piece");

	/** The fine grid's lines: walls across a cell, pillars at their crossings. */
	static float FineStep() { return WRTune(TEXT("building.fineStep"), 100.f); }

	/** How far from a foundation a snap still lines up with its grid. */
	static float FrameReach() { return FWRBuildRegistry::Get().GridSize * 2.5f; }

	/** On a build grid: the world's, or that of a foundation it's beside (one placed at an angle has its own), square to it. */
	static bool IsOnGrid(UWorld* World, const FWRPieceDef& Def, const FVector& Loc, float Yaw)
	{
		const float Grid = FWRBuildRegistry::Get().GridSize;
		if (FWRBuildRules::IsSnappedIn(Def.Shape, Loc, Yaw, Grid, FineStep(), FVector::ZeroVector, 0.f))
		{
			return true;
		}
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* Other = It->GetDef();
			if (!It->bGhost && Other && Other->Shape == TEXT("foundation") && FVector::Dist2D(It->GetActorLocation(), Loc) <= FrameReach()
				&& FWRBuildRules::IsSnappedIn(Def.Shape, Loc, Yaw, Grid, FineStep(), It->GetActorLocation(), It->GetActorRotation().Yaw))
			{
				return true;
			}
		}
		return false;
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
			// (The cheap test first: this runs every frame while you place, and few pieces touch.)
			const float OtherYaw = It->GetActorRotation().Yaw;
			if (Overlap(Mine, Loc, Yaw, PieceBox(*Other), It->GetActorLocation(), OtherYaw) && (bFree || !IsOnGrid(World, *Other, It->GetActorLocation(), OtherYaw)))
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
	WRGhostInfo();
	ConfirmPlacement();
	CancelPlacement();
}
void AWRPlayerController::WRGhostInfo()
{
	const AWRBuildPiece* G = Ghost.Get();
	UE_LOG(LogWildreach, Log, TEXT("GHOST %s at %s yaw %.0f %s %s| snap %d step %.0f look %s"), *PlacingPiece.ToString(), *PlaceLoc.ToCompactString(), PlaceYaw,
		bPlaceValid ? TEXT("valid") : TEXT("invalid"), *PlaceWhy, SnapMode, PlaceStep, G ? *G->GhostLook() : TEXT("none"));
}
void AWRPlayerController::WRGhostTurn(int32 WheelDir)
{
	if (WheelDir != 0)
	{
		WheelPlacement(WheelDir);
	}
	else
	{
		RotatePlacement();
		RotatePlacementEnd();
	}
}
void AWRPlayerController::WRSnap(FName Mode)
{
	SnapMode = Mode == TEXT("off") ? 0 : Mode == TEXT("fine") ? 2 : 1;
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
static AWRBuildPiece* SpawnGhost(UWorld* World, FName Piece, bool bPlain = false)
{
	AWRBuildPiece* G = World->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (G)
	{
		G->bGhost = true;
		G->bPlainGhost = bPlain;
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
	PlaceTurn = 0.f;
	PlaceStep = 0.f;
	Ghost = SpawnGhost(GetWorld(), Piece);  // "demolish": a red copy of whatever you aim at, made in UpdatePlacement
}

void AWRPlayerController::ToggleDemolish()
{
	if (IsDemolishing())
	{
		CancelPlacement();   // back to browsing the build bar
		return;
	}
	// From anywhere, as the Build key opens build mode: the bar comes up, already demolishing.
	if (UI && FWRScreens::Find(TEXT("build")))
	{
		Menu = EWRMenu::Build;
		MenuIndex = 0;
	}
	StartPlacing(TEXT("demolish"));
}

void AWRPlayerController::CancelPlacement()
{
	PlacingPiece = NAME_None;
	if (AWRBuildPiece* G = Ghost.Get())
	{
		G->Destroy();
	}
	Ghost = nullptr;
	bPlaceSpin = false;
	ShowSnapDots(nullptr, 0.f);
}

void AWRPlayerController::RotatePlacement()
{
	// On a grid, its square steps. Free, a small turn at once, and held it keeps turning (UpdatePlacement).
	if (PlaceStep > 0.f)
	{
		PlaceTurn += PlaceStep;
		return;
	}
	PlaceTurn += WRTune(TEXT("building.turnStep"), 15.f);
	bPlaceSpin = true;
	PlaceSpinAt = GetWorld()->GetTimeSeconds() + WRTune(TEXT("building.spinDelay"), 0.25f);
}

void AWRPlayerController::RotatePlacementEnd()
{
	bPlaceSpin = false;
}

void AWRPlayerController::WheelPlacement(int32 Dir)
{
	PlaceTurn -= Dir * (PlaceStep > 0.f ? PlaceStep : WRTune(TEXT("building.wheelStep"), 5.f));
}

void AWRPlayerController::ShowSnapDots(const AWRBuildPiece* On, float Step)
{
	if (SnapDotsOn.Get() == On && SnapDotsStep == Step)
	{
		return;
	}
	if (AActor* Old = SnapDots.Get())
	{
		Old->Destroy();
	}
	SnapDots = nullptr;
	SnapDotsOn = On;
	SnapDotsStep = Step;
	const FWRPieceDef* Def = On ? On->GetDef() : nullptr;
	if (!Def || Step <= 0.f)
	{
		return;
	}
	// On the surface you'd stand on: a foundation's top is 100 above it, a floor's 20.
	const float Top = Def->Shape == TEXT("foundation") ? 100.f : 20.f;
	const float Half = FWRBuildRegistry::Get().GridSize * 0.5f;
	const int32 N = FMath::RoundToInt(Half / Step);
	const float Size = WRTune(TEXT("building.snapDotSize"), 11.f);
	TArray<FWRLookBlock> Dots;
	for (int32 i = -N; i <= N; ++i)
	{
		for (int32 j = -N; j <= N; ++j)
		{
			FWRLookBlock Dot;
			Dot.Size = FVector(Size, Size, 1.f);
			Dot.Centre = FVector(i * Step, j * Step, Top + 1.5f);
			Dot.Mat = EWRLookMat::Cloth;
			Dots.Add(Dot);
		}
	}
	AActor* Marks = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
	if (!Marks)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Marks);
	Marks->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldTransform(On->GetActorTransform());
	FWRBuildLook::Spawn(Marks, Root, Dots, FLinearColor(0.f, 0.22f, 0.9f), false);   // blue: plain on pale planks and grey stone alike
	SnapDots = Marks;
}

void AWRPlayerController::ConfirmPlacement()
{
	if (PlacingPiece == TEXT("demolish"))
	{
		if (bPlaceValid)
		{
			ServerCommand(FString::Printf(TEXT("demolish %.0f %.0f %.0f"), PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z));
		}
		else
		{
			// Say why the click did nothing (the server would refuse it too).
			ShowLocalNotice(TEXT("Can't demolish: ") + PlaceWhy);
			WRSound::PlayUI(this, TEXT("build.invalid"));
			UE_LOG(LogWildreach, Log, TEXT("DEMOLISH refused here: %s"), *PlaceWhy);
		}
	}
	else if (IsPlacing())
	{
		// With no ground to aim at there's no spot to send, and nothing on the server would stop a chest hanging in the air.
		if (PlaceWhy == WRFreeBuild::NoAimWhy)
		{
			ShowLocalNotice(PlaceWhy);
			WRSound::PlayUI(this, TEXT("build.invalid"));
			return;
		}
		// A structure piece placed with snap off says so: the server checks it box against box, not by grid slot.
		const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PlacingPiece);
		ServerCommand(FString::Printf(TEXT("place %s %.0f %.0f %.0f %.0f%s"), *PlacingPiece.ToString(), PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z, PlaceYaw,
			Def && Def->Snaps() && !IsSnapOn() ? TEXT(" free") : TEXT("")));
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
	// Demolish reaches further than the 10 m a piece is placed within, so a piece out of reach says "too far" instead of
	// "aim at a piece". The 10 m count from you, not from the camera behind your shoulder.
	const float Behind = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Self->GetActorLocation() - Eye, Dir)));
	FVector Point = Eye + Dir * (Def ? Behind + 1000.f : WRTune(TEXT("ui.demolishReach"), 1600.f));
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Query);
	if (bHit)
	{
		Point = Hit.ImpactPoint;
	}
	else if (Def && (!Def->Snaps() || !IsSnapOn()))
	{
		// Looking past your reach: what goes where you aim comes down onto the ground at the end of it, and keeps following
		// your look, instead of hanging in the air. Only with no ground under it (out over a drop) is there nowhere to put it.
		FHitResult Below;
		if (GetWorld()->LineTraceSingleByChannel(Below, Point, Point - FVector(0.f, 0.f, WRTune(TEXT("building.dropReach"), 1500.f)), ECC_Visibility, Query))
		{
			Hit = Below;
			bHit = true;
			Point = Below.ImpactPoint;
		}
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
			G = SpawnGhost(GetWorld(), Aimed, true);
			Ghost = G;
		}
		const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
		bPlaceValid = Target && PS && Target->OwnerName == PS->GetPlayerName() && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < 1100.f;
		PlaceWhy = !Target ? FString(TEXT("aim at a piece you built")) : bPlaceValid ? FString()
			: Target->OwnerName != (PS ? PS->GetPlayerName() : FString()) ? FString::Printf(TEXT("not yours: %s built it"), Target->OwnerName.IsEmpty() ? TEXT("someone else") : *Target->OwnerName)
			: FString(TEXT("too far: get closer"));
		PlaceLoc = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		if (G && Target)
		{
			G->SetActorTransform(FTransform(Target->GetActorRotation(), PlaceLoc, FVector(1.02f)));  // a hair bigger, so it shows over the piece
			// Glowing red: a click takes it down. Someone else's (or out of reach): dull grey, and the chip says why.
			G->SetGhostGlow(bPlaceValid ? FLinearColor(1.f, 0.015f, 0.008f) : FLinearColor(0.5f, 0.5f, 0.53f), WRTune(TEXT("ui.demolishGlow"), 0.35f));
		}
		return;
	}

	// The way it wants to face: the way you're looking, plus however far you've turned it, so it follows your body and the
	// camera. A snap below squares that up. (A held rotate key keeps turning a free piece.)
	if (bPlaceSpin && PlaceStep <= 0.f && GetWorld()->GetTimeSeconds() >= PlaceSpinAt)
	{
		PlaceTurn += GetWorld()->GetDeltaSeconds() * WRTune(TEXT("building.spinSpeed"), 90.f);
	}
	const float Wanted = GetControlRotation().Yaw + PlaceTurn;
	const float Grid = FWRBuildRegistry::Get().GridSize;
	const AWRBuildPiece* DotsOn = nullptr;
	float DotsEvery = 0.f;
	PlaceStep = 0.f;

	const bool bFreeStructure = Def->Snaps() && !IsSnapOn();
	if (Def->Snaps() && IsSnapOn())
	{
		// The nearest foundation: its top sets the storey heights, and close by its grid is the grid (one placed at an angle
		// takes what's built beside it to that angle too).
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
		const bool bFrame = Foundation && Best <= WRFreeBuild::FrameReach();
		const FVector Origin = bFrame ? Foundation->GetActorLocation() : FVector::ZeroVector;
		const float FrameYaw = bFrame ? Foundation->GetActorRotation().Yaw : 0.f;
		const float LineStep = SnapMode == 2 ? WRFreeBuild::FineStep() : Grid;

		float BaseZ = Point.Z;
		const bool bGroundShape = Def->Shape == TEXT("foundation") || Def->Shape == TEXT("palisade") || Def->Shape == TEXT("tower") || Def->Shape == TEXT("gate");
		if (!bGroundShape)
		{
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
			float CellYaw = 0.f;
			const FVector Cell = FWRBuildRules::SnapInFrame(Def->Shape, Point, Point.Z, Grid, Grid, Origin, FrameYaw, CellYaw);
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
		// Stairs and roofs rise away from you until you turn them; which end of a gable is high is the rotate key's alone
		// (it mustn't flip because you looked round at the wall from the other side).
		const bool bGable = Def->Shape == TEXT("gable");
		float Yaw = bGable ? PlaceTurn + FrameYaw : Wanted;
		PlaceLoc = FWRBuildRules::SnapInFrame(Def->Shape, Point, BaseZ, Grid, LineStep, Origin, FrameYaw, Yaw);
		PlaceYaw = Yaw;
		PlaceStep = bGable ? 180.f : 90.f;
		if (SnapMode == 2 && bFrame)
		{
			DotsOn = Foundation;
			DotsEvery = LineStep;
		}
	}
	else
	{
		PlaceLoc = Point;
		PlaceYaw = Wanted;
		// Furniture on a foundation or a floor: that piece's own subdivided grid, square to it (the fine grid, finer).
		const AWRBuildPiece* Under = !Def->Snaps() && IsSnapOn() && bHit && Hit.ImpactNormal.Z > 0.7f ? Cast<AWRBuildPiece>(Hit.GetActor()) : nullptr;
		const FWRPieceDef* UnderDef = Under ? Under->GetDef() : nullptr;
		if (UnderDef && (UnderDef->Shape == TEXT("foundation") || UnderDef->Shape == TEXT("floor")))
		{
			const float Step = SnapMode == 2 ? WRTune(TEXT("building.furnitureFineStep"), 50.f) : WRTune(TEXT("building.furnitureStep"), 100.f);
			PlaceStep = SnapMode == 2 ? 45.f : 90.f;
			const float FrameYaw = Under->GetActorRotation().Yaw;
			const FRotator Frame(0.f, FrameYaw, 0.f);
			const FVector Local = Frame.UnrotateVector(Point - Under->GetActorLocation());
			const FVector OnGrid = Under->GetActorLocation() + Frame.RotateVector(FVector(FMath::RoundToFloat(Local.X / Step) * Step, FMath::RoundToFloat(Local.Y / Step) * Step, 0.f));
			PlaceLoc = FVector(OnGrid.X, OnGrid.Y, Point.Z);
			PlaceYaw = FrameYaw + FMath::RoundToFloat((Wanted - FrameYaw) / PlaceStep) * PlaceStep;
			DotsOn = Under;
			DotsEvery = Step;
		}
		else if (bFreeStructure && Def->Shape == TEXT("foundation") && bHit && !Cast<AWRBuildPiece>(Hit.GetActor()))
		{
			// Snap off: where you aim, a foundation sunk into the ground as on the grid.
			PlaceLoc.Z -= WRTune(TEXT("building.foundationSink"), 60.f);
		}
	}
	// Whole centimetres and degrees, as the place command carries them, so the server checks exactly the spot you see.
	PlaceLoc = FVector(FMath::RoundToFloat(PlaceLoc.X), FMath::RoundToFloat(PlaceLoc.Y), FMath::RoundToFloat(PlaceLoc.Z));
	PlaceYaw = FMath::RoundToFloat(FRotator::NormalizeAxis(PlaceYaw));
	G->SetActorLocationAndRotation(PlaceLoc, FRotator(0.f, PlaceYaw, 0.f));
	ShowSnapDots(DotsOn, DotsEvery);

	// Quick client-side checks (the server re-checks everything).
	bPlaceValid = true;
	PlaceWhy.Reset();
	const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	if ((bFreeStructure || !Def->Snaps()) && !bHit)
	{
		bPlaceValid = false;
		PlaceWhy = WRFreeBuild::NoAimWhy;
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
	Out.Add({ FString::Printf(TEXT("Demolish (%s): aim at a piece of yours, LMB removes it (half back)"), *FWRKeyBindings::Get().Key(TEXT("Demolish")).GetDisplayName().ToString()), TEXT("build demolish") });
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
		const bool bFree = Def && Def->Snaps() && (Arg(6) == TEXT("free") || !WRFreeBuild::IsOnGrid(GetWorld(), *Def, Loc, Yaw));
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
			// A bed you place is where you'll wake (you can still set another with E).
			if (WRBeds::IsBed(Piece))
			{
				WRBeds::Set(PS, Piece);
				OutResult = FString::Printf(TEXT("Built %s. You'll wake here"), *Def->Name);
			}
			UE_LOG(LogWildreach, Log, TEXT("BUILD %s placed %s at %s (support %d)"), *Who, *PieceId.ToString(), *Loc.ToCompactString(), Support);
		}
	}
	else if (Cmd == TEXT("setcode") || Cmd == TEXT("code") || Cmd == TEXT("clearcode"))
	{
		// setcode / code <4 digits> [X Y Z], clearcode [X Y Z]: the code-lock door standing there (the keypad sends the one
		// it's for), else the nearest one within reach.
		const int32 At = Cmd == TEXT("clearcode") ? 1 : 2;
		const bool bAt = Args.Num() >= At + 3;
		const FVector Spot(FCString::Atof(*Arg(At)), FCString::Atof(*Arg(At + 1)), FCString::Atof(*Arg(At + 2)));
		AWRBuildPiece* Door = nullptr;
		float Best = 450.f;
		for (TActorIterator<AWRBuildPiece> It(GetWorld()); It && Self; ++It)
		{
			const FWRPieceDef* D = It->GetDef();
			if (It->bGhost || !D || !D->bLockable || (bAt && FVector::Dist(It->GetActorLocation(), Spot) > 50.f)) continue;
			const float Dist = FVector::Dist(It->GetActorLocation() + FVector(0.f, 0.f, 100.f), Self->GetActorLocation());
			if (Dist < Best) { Best = Dist; Door = *It; }
		}
		const FString Code = Arg(1);
		if (!Door) OutResult = TEXT("no code-lock door nearby");
		else if (Cmd == TEXT("clearcode"))
		{
			if (!Door->CanAccess(Who)) OutResult = TEXT("Not your door");
			else { Door->LockCode.Reset(); Door->bLocked = false; Door->UnlockedFor.Reset(); OutResult = TEXT("Lock removed: anyone can open it"); }
		}
		else if (Code.Len() != 4 || !Code.IsNumeric()) OutResult = TEXT("Codes are 4 digits");
		else if (Cmd == TEXT("setcode"))
		{
			if (!Door->CanAccess(Who)) OutResult = TEXT("Not your door");
			else { Door->LockCode = Code; Door->bLocked = true; Door->UnlockedFor.Reset(); OutResult = TEXT("Code set; door locked"); }
		}
		else if (Door->LockCode == Code)
		{
			// Right: it opens, and stays theirs to open until the code changes.
			Door->UnlockedFor.AddUnique(Who);
			if (!Door->bOpen) Door->SetOpen(true);
			OutResult = TEXT("Code accepted");
		}
		else OutResult = TEXT("Wrong code");
		UE_LOG(LogWildreach, Log, TEXT("DOOR %s %s at %s: %s"), *Who, *Cmd, Door ? *Door->GetActorLocation().ToCompactString() : TEXT("-"), *OutResult);
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
		else if (Piece->OwnerName != Who) OutResult = FString::Printf(TEXT("not yours: %s built it"), Piece->OwnerName.IsEmpty() ? TEXT("someone else") : *Piece->OwnerName);
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
	else if (Cmd == TEXT("chestname"))
	{
		// chestname <words>: name the chest you stand at (its screen's Rename); nothing after it = its own name again.
		AWRBuildPiece* Chest = nullptr;
		for (TActorIterator<AWRBuildPiece> It(GetWorld()); It && Self; ++It)
		{
			const FWRPieceDef* ItDef = It->GetDef();
			const float Dist = FVector::Dist(It->GetActorLocation() + FVector(0.f, 0.f, 100.f), Self->GetActorLocation());
			if (!It->bGhost && ItDef && ItDef->Kind == TEXT("chest") && Dist < 450.f && (!Chest || Dist < FVector::Dist(Chest->GetActorLocation() + FVector(0.f, 0.f, 100.f), Self->GetActorLocation()))) Chest = *It;
		}
		TArray<FString> Words(Args);
		Words.RemoveAt(0);
		FString Name = FString::Join(Words, TEXT(" ")).TrimStartAndEnd().Left(FMath::RoundToInt(WRTune(TEXT("building.chestNameLength"), 24.f)));
		Name.ReplaceInline(TEXT("|"), TEXT(""));
		if (!Chest) OutResult = TEXT("no chest nearby");
		else if (!Chest->CanAccess(Who)) OutResult = TEXT("not your chest");
		else
		{
			Chest->CustomName = Name;
			OutResult = Name.IsEmpty() ? TEXT("The chest has its own name again") : FString::Printf(TEXT("Chest named \"%s\""), *Name);
			UE_LOG(LogWildreach, Log, TEXT("CHEST %s named \"%s\" by %s"), *Chest->GetName(), *Name, *Who);
		}
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
