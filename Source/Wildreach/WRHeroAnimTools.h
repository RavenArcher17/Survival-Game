#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WRHeroAnimTools.generated.h"

class UAnimSequenceBase;

/**
 * Editor helpers for the hero's animation assets, called from Python (Tools/anim/build_hero_anim.py) because the parts
 * they touch aren't open to Python: a Pose Search database keeps its clip list in a private property, and a window
 * notify needs its gameplay tag set. Editor builds only; elsewhere they do nothing.
 */
UCLASS()
class WILDREACH_API UWRHeroAnimTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Set an object's properties from exported text lines ("Name=Value", "Name(3)=Value"), the form Unreal's own text
	 *  export writes (Tools/anim/hero_databases.json holds GASP's databases in it). Returns the lines it couldn't use. */
	UFUNCTION(BlueprintCallable, Category = "Wildreach|Editor")
	static TArray<FString> ImportProperties(UObject* Target, const TArray<FString>& Lines, bool bClearArrays = true);

	/** Remove every Wildreach window notify from a clip. */
	UFUNCTION(BlueprintCallable, Category = "Wildreach|Editor")
	static int32 ClearWindows(UAnimSequenceBase* Clip);

	/** Add a window (State.ComboWindow ...) from Start to End seconds on the clip's "Windows" track. */
	UFUNCTION(BlueprintCallable, Category = "Wildreach|Editor")
	static bool AddWindow(UAnimSequenceBase* Clip, FName Tag, float Start, float End);
};
