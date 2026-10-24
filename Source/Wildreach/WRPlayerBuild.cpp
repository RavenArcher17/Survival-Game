// AWRPlayerController: build mode (client ghost + server-validated placement), chest/claim menus,
// code locks and demolish.

#include "WRPlayerController.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRLifeSkills.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRPlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"

void AWRPlayerController::WRPlace(FName Piece, float X, float Y, float Z, float Yaw)
{
	ServerCommand(FString::Printf(TEXT("place %s %.0f %.0f %.0f %.0f"), *Piece.ToString(), X, Y, Z, Yaw));
}
void AWRPlayerController::WRSetCode(const FString& Code) { ServerCommand(FString::Printf(TEXT("setcode %s"), *Code)); }
void AWRPlayerController::WRCode(const FString& Code) { ServerCommand(FString::Printf(TEXT("code %s"), *Code)); }
void AWRPlayerController::WRDemolish() { ServerCommand(TEXT("demolish")); }
void AWRPlayerController::WRHitPiece(int32 Hits) { ServerCommand(FString::Printf(TEXT("hitpiece %d"), Hits)); }

AWRBuildPiece* AWRPlayerController::FindNearbyPiece(float Range) const
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
		if (It->bGhost || !Def)
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
		CancelPlacement();
		return;
	}
	Menu = Menu == EWRMenu::Build ? EWRMenu::None : EWRMenu::Build;
	MenuIndex = 0;
}

void AWRPlayerController::StartPlacing(FName Piece)
{
	CancelPlacement();
	MenuClose();
	PlacingPiece = Piece;
	PlaceYaw = GetControlRotation().Yaw;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRBuildPiece* G = GetWorld()->SpawnActorDeferred<AWRBuildPiece>(AWRBuildPiece::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (G)
	{
		G->bGhost = true;
		G->SetReplicates(false);
		G->PieceId = Piece;
		G->FinishSpawning(FTransform::Identity);
		G->SetActorEnableCollision(false);
		Ghost = G;
	}
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
	PlaceYaw += 90.f;
}

void AWRPlayerController::ConfirmPlacement()
{
	if (IsPlacing())
	{
		ServerCommand(FString::Printf(TEXT("place %s %.0f %.0f %.0f %.0f"), *PlacingPiece.ToString(), PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z, PlaceYaw));
	}
}

void AWRPlayerController::UpdatePlacement()
{
	const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PlacingPiece);
	AWRBuildPiece* G = Ghost.Get();
	const APawn* Self = GetPawn();
	if (!Def || !G || !Self || !PlayerCameraManager)
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
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Query))
	{
		Point = Hit.ImpactPoint;
	}

	if (Def->Snaps())
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
				const float Top = Foundation->GetActorLocation().Z + 100.f;
				const int32 Level = FMath::Max(Def->Shape == TEXT("floor") || Def->Shape == TEXT("roof") ? 1 : 0, FMath::RoundToInt((Point.Z - Top) / 300.f));
				BaseZ = Top + Level * 300.f;
			}
		}
		float Yaw = PlaceYaw;
		PlaceLoc = FWRBuildRules::SnapToGrid(Def->Shape, Point, BaseZ, FWRBuildRegistry::Get().GridSize, Yaw);
		G->SetActorLocationAndRotation(PlaceLoc, FRotator(0.f, Yaw, 0.f));
		PlaceYaw = Def->Shape == TEXT("stairs") ? Yaw : PlaceYaw;
		if (Def->Shape != TEXT("stairs"))
		{
			// The server snaps the same way; send the snapped yaw.
			PlaceYaw = Yaw;
		}
	}
	else
	{
		PlaceLoc = Point;
		G->SetActorLocationAndRotation(PlaceLoc, FRotator(0.f, PlaceYaw, 0.f));
	}

	// Quick client-side checks (the server re-checks everything).
	bPlaceValid = true;
	PlaceWhy.Reset();
	const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	if (FVector::Dist(PlaceLoc, Self->GetActorLocation()) > 1100.f)
	{
		bPlaceValid = false;
		PlaceWhy = TEXT("too far");
	}
	else if (!AWRClaimTotem::CanBuild(GetWorld(), PlaceLoc, PS ? PS->GetPlayerName() : FString(), PlaceWhy))
	{
		bPlaceValid = false;
	}
	else if (PS)
	{
		for (const TPair<FName, int32>& C : Def->Cost)
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
	for (const FWRPieceDef& Def : FWRBuildRegistry::Get().All())
	{
		FString Cost;
		bool bHave = true;
		for (const TPair<FName, int32>& C : Def.Cost)
		{
			const int32 Have = Inv ? Inv->CountOf(C.Key) : 0;
			bHave &= Have >= C.Value;
			Cost += FString::Printf(TEXT("%s %d/%d  "), *C.Key.ToString(), Have, C.Value);
		}
		Out.Add({ FString::Printf(TEXT("[%s] %s  <-  %s"), *Def.Category.ToString(), *Def.Name, *Cost), FString::Printf(TEXT("build %s"), *Def.Id.ToString()), bHave });
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
			for (TActorIterator<AWRBuildPiece> It(GetWorld()); It && OutResult.IsEmpty(); ++It)
			{
				const FWRPieceDef* O = It->GetDef();
				if (!It->bGhost && O && O->Shape == Def->Shape && FVector::Dist(It->GetActorLocation(), Loc) < 30.f) OutResult = TEXT("something is already there");
			}
		}
		// Thrifty perk: cheaper building.
		TMap<FName, int32> Cost;
		const float CostMult = FMath::Max(0.1f, 1.f + UWRLifeSkillsComponent::StatFor(this, TEXT("buildCost")));
		if (Def) for (const TPair<FName, int32>& C : Def->Cost) Cost.Add(C.Key, FMath::Max(1, FMath::CeilToInt(C.Value * CostMult)));
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
		AWRBuildPiece* Piece = FindNearbyPiece(450.f);
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
	else if (Cmd == TEXT("claimdeposit") || Cmd == TEXT("claimupgrade"))
	{
		AWRClaimTotem* Totem = Self ? AWRClaimTotem::FindAt(GetWorld(), Self->GetActorLocation()) : nullptr;
		if (!Totem) OutResult = TEXT("not inside a land claim");
		else if (Cmd == TEXT("claimdeposit")) OutResult = Totem->OwnerName == Who ? Totem->Deposit(this) : TEXT("not your claim");
		else OutResult = Totem->Upgrade(this);
	}
	else if (Cmd == TEXT("store") || Cmd == TEXT("take"))
	{
		AWRBuildPiece* Chest = FindNearbyPiece(450.f);
		const FWRPieceDef* Def = Chest ? Chest->GetDef() : nullptr;
		const int32 Index = FCString::Atoi(*Arg(1));
		if (!Def || Def->Kind != TEXT("chest")) OutResult = TEXT("no chest nearby");
		else if (!Chest->CanAccess(Who)) OutResult = TEXT("not your chest");
		else if (Cmd == TEXT("store"))
		{
			const TArray<FWRItemStack>& Slots = Inv->GetSlots();
			if (!Slots.IsValidIndex(Index) || Slots[Index].IsEmpty()) OutResult = TEXT("empty slot");
			else
			{
				const FWRItemStack Stack = Slots[Index];
				const FWRItemDef* ItemDef = FWRItemRegistry::Get().Find(Stack.ItemId);
				const bool bOk = ItemDef && ItemDef->IsEquipment() ? FWRInventoryRules::AddInstance(Chest->Storage, Stack)
					: FWRInventoryRules::Add(Chest->Storage, Stack.ItemId, Stack.Count) == 0;
				if (bOk) { Inv->RemoveItem(Stack.ItemId, Stack.Count); OutResult = TEXT("Stored"); }
				else OutResult = TEXT("chest full");
			}
		}
		else
		{
			if (!Chest->Storage.IsValidIndex(Index) || Chest->Storage[Index].IsEmpty()) OutResult = TEXT("empty slot");
			else
			{
				const FWRItemStack Stack = Chest->Storage[Index];
				const FWRItemDef* ItemDef = FWRItemRegistry::Get().Find(Stack.ItemId);
				const bool bOk = ItemDef && ItemDef->IsEquipment() ? Inv->AddInstance(Stack) : Inv->AddItem(Stack.ItemId, Stack.Count) == 0;
				if (bOk) { Chest->Storage[Index] = FWRItemStack(); OutResult = TEXT("Taken"); }
				else OutResult = TEXT("inventory full");
			}
		}
	}
	else
	{
		return false;
	}
	ClientNotify(OutResult);
	return true;
}
