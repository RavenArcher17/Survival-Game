#pragma once

#include "CoreMinimal.h"

class APlayerController;
struct FSlateBrush;

/**
 * Report a problem (F8, or View on a pad): the testers' channel to the developers, in place of automated testing
 * (COMBAT-AND-LESSONS L10). The moment it's asked for, the frame on screen (world and interface) is frozen as a JPEG,
 * and what a developer needs to find the problem is gathered: version, map, net mode, position and facing, health and
 * the rest of the player's numbers, the open screen, device and pad, frame rate, resolution and quality, time played,
 * and the last 60 s of gameplay events, notices and warnings (a ring started at boot).
 *
 * The player picks one of 8 kinds (Bug, Stuck, Controls, Camera, Music-Audio, Visual, Balance, Idea, as Waveraider's) and
 * types a note; Send:
 *  - posts it to the studio's feedback inbox (POST https://freedom35studios.pages.dev/api/feedback, game "wildreach",
 *    the same body as Waveraider's report.ts); offline or refused, it waits in Saved/Reports/Outbox and is sent again
 *    every 30 s and at the next start;
 *  - saves it to Saved/Reports/<time>_<player>.json (+ .jpg) on this machine;
 *  - on a client, sends it to the host too (in small pieces, a little each frame), which saves it to its own
 *    Saved/Reports, so the host of a playtest has every player's reports in one place.
 */
namespace WRReport
{
	/** Start the 60 s ring (log lines from the game, warnings and errors). Called once at startup. */
	WILDREACH_API void InstallLog();
	/** A gameplay event for the ring ("menu inventory", "died to Cinderfang"...). */
	WILDREACH_API void Event(const TCHAR* Kind, const FString& Text);

	/** F8: freeze this frame and open the report panel (a no-op while one is open). Via: "F8", "pad", "menu". */
	WILDREACH_API void Open(const TCHAR* Via);
	WILDREACH_API bool IsOpen();
	/** Close the panel without sending. */
	WILDREACH_API void Cancel();

	/** Each frame on game clients: outbox retries, pieces of a report on their way to the host. */
	WILDREACH_API void Tick(float DeltaTime);

	/** Host side: a piece of a client's report arrived (WRPlayerController's ServerReportPart). */
	WILDREACH_API void HostReceive(APlayerController* From, int32 Id, int32 Part, int32 Parts, uint8 Kind, const TArray<uint8>& Bytes);

	/** The panel (WRUIReport.cpp) */
	struct FDraft
	{
		FString Shot;               // the frozen frame as JPEG bytes in base64 ("" when it couldn't be taken)
		TArray<uint8> Jpeg;
		int32 Width = 0, Height = 0;
		TSharedPtr<FSlateBrush> Brush;   // the frozen frame for the panel
		TSharedPtr<class FJsonObject> Context;
		FString Via;
		bool bPaused = false;
	};
	WILDREACH_API TSharedPtr<FDraft> GetDraft();
	/** Send the open draft with a kind (0-7) and note, then close the panel. */
	WILDREACH_API void Send(int32 Kind, const FString& Note);
	WILDREACH_API const TArray<FString>& Kinds();
}
