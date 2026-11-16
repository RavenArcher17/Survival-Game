#pragma once

#include "CoreMinimal.h"

class AWRPlayerController;
class UWorld;
class UWRInventoryComponent;
struct FWRPieceDef;
struct FWRItemStack;

/**
 * The build bar's categories and pieces as the controller needs them (WRUIBuild.cpp; cookpass-building 3.1 / 3.2):
 * the pad's D-pad walks the open category's pieces and the categories; copy picks a built piece; the bar opens on the
 * Survival tab until you own a claim, then on the tab you used last.
 */
namespace WRBuildBar
{
	/** The tab a piece shows under (Survival is extra: the starter pieces also stay in their own tab). */
	WILDREACH_API FName TabOf(const FWRPieceDef& D);
	/** Is the piece on this tab ("survival": the starter pieces)? */
	WILDREACH_API bool InTab(const FWRPieceDef& D, FName Tab);
	/** Could this player start placing it (its land live, its blueprint known)? OutWhy says why not. */
	WILDREACH_API bool CanPick(const AWRPlayerController* PC, const FWRPieceDef& D, FString* OutWhy = nullptr);
	/** The next (Dir +1) or previous (-1) piece this player can pick on Tab after Current, wrapping; None if none. */
	WILDREACH_API FName Step(const AWRPlayerController* PC, FName Tab, FName Current, int32 Dir);
	/** The next tab after Tab with a piece this player can pick, and that piece (OutFirst). */
	WILDREACH_API FName NextTab(const AWRPlayerController* PC, FName Tab, FName& OutFirst);
	/** The tab the bar opens on: Survival with no claim of your own, else the last used (saved on this PC). */
	WILDREACH_API FName OpeningTab(const AWRPlayerController* PC);
	/** Remember the tab last used (GameUserSettings [Wildreach.Build] LastTab). */
	WILDREACH_API void RememberTab(FName Tab);
}

/**
 * Building from storage (cookpass-building 3.2; ui-controls 2.11): your own (or your clan's) chests inside the claim
 * you stand in, within building.storageReachCm of you, count toward a piece's cost. The bag pays first, then the
 * nearest chest. Chests' contents replicate to every machine, so the bar can count them too.
 */
namespace WRBuildStock
{
	/** How many of Item the chests near Who (a survivor at At) hold for Player (planks count for wood). */
	WILDREACH_API int32 InChests(const UWorld* World, const FString& Player, const FVector& At, FName Item);
	/** Server: take Count of Item: the bag first (planks first for wood), then chests nearest first. */
	WILDREACH_API void Pay(UWorld* World, const FString& Player, const FVector& At, UWRInventoryComponent* Bag, FName Item, int32 Count);
}
