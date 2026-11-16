#pragma once

#include "CoreMinimal.h"

class UWorld;
class APlayerController;

/** One outlawry from a world (cooks/cookpass-multiplayer.md 3.4): the name and the address it came from are turned away. */
struct FWRBan
{
	FString Name;
	FString Address;
	/** UTC seconds it ends (0 = for good). */
	int64 Until = 0;
	FString By;
};

/**
 * Who may come into a world (cooks/cookpass-multiplayer.md 3.4; multiplayer.md 2.1 / 2.5): Saved/Worlds/<id>/access.json,
 * outside the world store so the menu can set it while the world isn't running.
 *   maxPlayers       listen worlds net.listenMinPlayers..net.listenMaxPlayers (default net.listenDefaultPlayers); a
 *                    dedicated server up to net.dedicatedMaxPlayers
 *   passSalt/Hash    SHA-256(salt + the wire hash); no password when empty. The joiner's game sends
 *                    ?WRPass=SHA-256("wildreach-pass-1:" + what they typed): the typed text never leaves their PC
 *   admins           names (held by WRIdentity, so a name can't be borrowed); a listen world's host is always in charge
 *   bans             FWRBan
 * Written whole to a temp file, then moved over the old one.
 */
struct FWRAccess
{
	int32 MaxPlayers = 8;
	FString PassSalt;
	FString PassHash;
	TArray<FString> Admins;
	TArray<FWRBan> Bans;

	bool HasPassword() const { return !PassHash.IsEmpty(); }
	bool IsAdmin(const FString& Name) const;
};

namespace WRAccess
{
	WILDREACH_API FWRAccess Read(const FString& WorldId);
	WILDREACH_API bool Write(const FString& WorldId, const FWRAccess& Access);

	/** Client: what a typed password travels as. */
	WILDREACH_API FString WireHash(const FString& Typed);
	/** Sets (or, with "", clears) the password: a new salt each time. */
	WILDREACH_API void SetPassword(FWRAccess& Access, const FString& Typed);
	/** The player cap's limits on this kind of server. */
	WILDREACH_API int32 MinPlayers();
	WILDREACH_API int32 MaxPlayersLimit(bool bDedicated);

	/** Server: the running world's access (loaded at InitGame), and saving it. */
	WILDREACH_API FWRAccess& Current();
	WILDREACH_API void LoadCurrent();
	WILDREACH_API void SaveCurrent();

	/**
	 * Server, PreLogin: "" lets the join in; else the reason the joiner reads. Refusals start "WR|" (the menu shows the
	 * rest as it is) or "PASSWORD|" (the menu asks for the password). Order: banned, full, password.
	 */
	WILDREACH_API FString Refuse(const FString& Name, const FString& Wire, const FString& Address, int32 NumPlayers);

	/** Server: everyone's role on their player state (2 host, 1 admin, 0 player), and the ban list on admins' own. */
	WILDREACH_API void RefreshRoles(UWorld* World);
	/** Server: their role (2 host, 1 admin, 0). */
	WILDREACH_API uint8 RoleOf(const APlayerController* PC);

	/** Server: an admin's command ("admin kick|ban|unban|op|deop <name> [hours]"); the reply for them. */
	WILDREACH_API FString Command(APlayerController* By, const TArray<FString>& Args);

	/** "21 h left" / "for good". */
	WILDREACH_API FString BanLeft(const FWRBan& Ban);
}
