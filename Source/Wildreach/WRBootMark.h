// The Freedom35 site's PC boot check (tools/bootcheck-pc.mjs) starts the packaged game with -F35Boot -abslog=<file> and
// reads that file for "F35READY menu", "F35BOOT ..." and "F35READY play" (WRMainMenu.cpp, WRUIFlow.cpp).
//
// A Shipping build has no log at all: NO_LOGGING strips every UE_LOG, and an installed engine can't build Shipping with
// logging on (UnrealBuildTool refuses a unique build environment on it). So in such a build these few lines are written
// by hand into the file -abslog names, and nowhere else; a player's normal start (no -abslog) writes nothing. In a build
// with logging they are ordinary log lines, as before.
#pragma once

#include "CoreMinimal.h"
#include "Wildreach.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

namespace WRBootMark
{
	inline void Line(const FString& Text)
	{
		UE_LOG(LogWildreach, Display, TEXT("%s"), *Text);
#if NO_LOGGING
		FString Path;
		if (FParse::Value(FCommandLine::Get(), TEXT("-abslog="), Path) && !Path.IsEmpty())
		{
			const FString Stamp = FDateTime::Now().ToString(TEXT("[%Y.%m.%d-%H.%M.%S:%s]"));
			FFileHelper::SaveStringToFile(Stamp + TEXT(" LogWildreach: Display: ") + Text + LINE_TERMINATOR, *Path,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}
#endif
	}
}
