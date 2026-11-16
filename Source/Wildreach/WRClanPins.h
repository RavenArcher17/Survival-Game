#pragma once

#include "CoreMinimal.h"
#include "WRClanPins.generated.h"

class UWorld;

/** One map pin as it travels between a player's client, the server and their clanmates (cooks/wild-answers-1116.md 4.2). */
USTRUCT()
struct FWRSharedPin
{
	GENERATED_BODY()

	/** Who made it: the server fills it in (anything a client sends here is ignored). */
	UPROPERTY() FString Owner;
	/** World cm. */
	UPROPERTY() FVector2D Where = FVector2D::ZeroVector;
	/** The pin's mark (WRUIMap::PinGlyphName). */
	UPROPERTY() uint8 Glyph = 5;
	UPROPERTY() FString Label;
};

/**
 * Map pins shared with your clan (Aaron 11-16, q-map-pins: "allow players to share this info across clans, but players
 * have an option to 'see clan pins' but only see their own by default"). Rust's team markers and ARK's tribe markers:
 * every pin you make goes to the server, which keeps each player's pins in the world's store (the `map_pins` document,
 * so an offline clanmate's pins still show) and sends each connected player their clanmates' pins whenever one of them
 * changes, someone joins, or a clan changes. On the client they stay off the map and the compass until the player ticks
 * "Clan pins" on the map's legend (off by default, kept between runs).
 */
namespace WRClanPins
{
	/** Server: a player's own pins as their client sent them (clamped to ui.pins.max and the label length), saved, and
	 *  sent to their online clanmates. */
	WILDREACH_API void SetPlayerPins(UWorld* World, const FString& Player, const TArray<FWRSharedPin>& Pins);
	/** Server: every connected player gets their clanmates' pins again (someone joined, a clan changed). */
	WILDREACH_API void SendAll(UWorld* World);

	/** Client: the clanmates' pins the server last sent (empty in another world, or with no clan). */
	WILDREACH_API const TArray<FWRSharedPin>& Clan(const UWorld* World);
	WILDREACH_API void SetClan(const UWorld* World, const TArray<FWRSharedPin>& Pins);

	/** Client: this player's own pins changed (or were loaded): they go up to the server, at most once a second. The map's
	 *  pin store asks FlushDue each time it's read and hands the list to Send when it is. */
	WILDREACH_API void NoteOwnChanged();
	WILDREACH_API bool FlushDue();
	/** Client: send the list (via the local player's controller); false if there's no connected controller yet (it
	 *  stays due and goes next time). */
	WILDREACH_API bool Send(const UWorld* World, const TArray<FWRSharedPin>& Own);

	/** The legend's "Clan pins" option: off by default, kept in GameUserSettings.ini. */
	WILDREACH_API bool ShowClan();
	WILDREACH_API void SetShowClan(bool bOn);
}
