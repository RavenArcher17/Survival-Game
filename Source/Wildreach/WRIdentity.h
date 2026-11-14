#pragma once

#include "CoreMinimal.h"

/**
 * Who owns a character name on a world (M7 1 "a player-chosen name + generated token in the client's Saved dir; Steam/EOS
 * later"; cooks/build-server-raids.md 2.2).
 *   Client: 32 random bytes from the OS crypto source in Saved/Identity/key.bin, made once per PC. Joins carry
 *           ?WRKey=<hex SHA-256(key + "wildreach-identity-1")>: the key itself never leaves the PC.
 *   Server: the world store's accounts table keeps SHA-256(token + world id) per name. The first join with a free (or
 *           unclaimed) name claims it; a later join with another token is refused at PreLogin. Once an online subsystem
 *           (Steam or EOS) gives a real account id, it's bound to the name too, and a join from that account is let in
 *           from any PC (its token is rebound).
 * Worlds on JSON files (-wrstore=json) and the menu's island don't check (there's no accounts table).
 */
namespace WRIdentity
{
	/** Client: the token this PC joins with (makes the key on first use). */
	WILDREACH_API FString Token();

	/** Server, PreLogin: may this join have this name? "" = yes, else the reason the joiner sees. */
	WILDREACH_API FString Check(const FString& Name, const FString& Token, const FString& Platform, const FString& PlatformId);
	/** Server, once the player is in: claims or refreshes the name's account (token, platform id, last seen, address). */
	WILDREACH_API void Bind(const FString& Name, const FString& Token, const FString& Platform, const FString& PlatformId, const FString& Address);

	/** Lower-case hex SHA-256 of the text's UTF-8 bytes. */
	WILDREACH_API FString Sha256Hex(const FString& Text);
	WILDREACH_API FString Sha256Hex(const uint8* Data, int32 Num);
}
