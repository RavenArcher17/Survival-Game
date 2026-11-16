#pragma once
// Who is a tester (cooks/build-godmode.md 2.1): the Freedom35 site's word, per connection.
//
//   Client (the tester's game): reads the PC link Freedom35's updater keeps (%LOCALAPPDATA%\Freedom35\pc-link.bin,
//           DPAPI, the same one You're the Cupcake signs in with) and asks POST <god.site>/api/pc/tester
//           { game, bind, nonce }. bind = SHA-256(WRIdentity token + "wildreach-tester-bind-1"); nonce = the server's.
//           No link (the game wasn't run through Freedom35): nothing is asked, ever.
//   Server: checks the proof's Ed25519 signature against the public key(s) compiled in below, then iss, aud, game,
//           expiry, bind (from the ?WRKey the player joined with) and the nonce it sent that connection.
#include "CoreMinimal.h"

namespace WRGodAuth
{
	/** What the site said about a proof. */
	struct FClaims
	{
		FString Label;
		FString Sub;
		FString Bind;
		FString Nonce;
		int64 Exp = 0;
	};

	/** The bind for a PC's identity token. */
	WILDREACH_API FString BindFor(const FString& IdentityToken);

	/** Client: ask the site for a proof for this nonce; Done(Proof, Label) on the game thread (empty: not a tester, no
	 *  link, offline, refused). At most one request in flight per nonce. */
	WILDREACH_API void RequestProof(const FString& Nonce, TFunction<void(const FString& Proof, const FString& Label)> Done);

	/** Server: the proof checks out for this join key and nonce (OutWhy: why not). */
	WILDREACH_API bool Verify(const FString& Proof, const FString& JoinKey, const FString& Nonce, FClaims& OutClaims, FString& OutWhy);
}
