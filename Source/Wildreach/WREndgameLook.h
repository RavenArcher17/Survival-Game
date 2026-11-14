#pragma once
// How the endgame looks (cooks/build-endgame.md 2.8, 2.9): boss monuments (a granite plinth with a Roquepertuse niche,
// the boss's own head on a bronze collar, a bronze tabula ansata with the kill), banner sigils, the weapon glow, the
// armour dye and raid marks, and the prestige gold threads. Clients draw; the server only sets what replicates.
#include "CoreMinimal.h"

class AWRBuildPiece;
class AActor;
struct FWRPieceDef;

namespace WREndgameLook
{
	/** A monument piece's look (plinth, head, plaque). Ghost: the plinth only. */
	WILDREACH_API void BuildMonument(AWRBuildPiece* Piece, const FWRPieceDef& Def, bool bGhost);
	/** Server: the plaque's line ("time|names|AL") from the boards for the piece's owners. */
	WILDREACH_API void SetPlaqueLine(AWRBuildPiece* Piece);
	/** The plaque's lines from the piece's name line (server-set: "time|names|AL"). */
	WILDREACH_API void RefreshPlaque(AWRBuildPiece* Piece);
	/** A banner's woven sigil (its Emblem), redrawn over the plain emblem; None takes it off. */
	WILDREACH_API void DressBanner(AWRBuildPiece* Piece);
	/** Server: the sigil this owner weaves on new banners (their deeds' choice; None when they're offline or chose none). */
	WILDREACH_API FName SigilOf(const UWorld* World, const FString& Owner);

	// --- The hero (read by WRHeroLook.cpp) ---
	WILDREACH_API uint32 LookSig(const AActor* Hero);
	WILDREACH_API float Gold(const AActor* Hero, FName Path);
	WILDREACH_API FLinearColor GoldColour();
	/** A worn raid mark that runs in the armour's seams (its colour and glow). */
	WILDREACH_API bool SeamMark(const AActor* Hero, FLinearColor& OutColour, float& OutGlow);
}
