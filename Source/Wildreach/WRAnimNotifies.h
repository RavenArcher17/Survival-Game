#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "WRAnimNotifies.generated.h"

/**
 * A combat window carried by a clip (cooks/combat.md "Windows", COMBAT-AND-LESSONS 1.16): while it's open the owner has
 * the window's tag (State.ComboWindow, State.CancelWindow, State.IFrames, State.TraceActive), so the input buffer, the
 * dodge and the weapon trace read the same timing the player sees. The authority's tag replicates (TagOnly); the owning
 * client adds its own copy at once so a buffered press isn't held back by the round trip.
 *
 * Placed on the hero's clips by Tools/anim/place_windows.py from Data/anim_windows.json (measured from each swing:
 * the hit frames are where the striking hand moves fastest). A dedicated server draws no bodies, so it reads the same
 * numbers through FWRAnimWindows instead of the notifies.
 */
UCLASS(meta = (DisplayName = "Wildreach Window"))
class WILDREACH_API UWRANS_Window : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Window") FGameplayTag Window;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	void Apply(USkeletalMeshComponent* MeshComp, int32 Delta) const;
};

/** The same windows as data (Data/anim_windows.json), for code that times them without a body (the server). */
struct WILDREACH_API FWRAnimWindows
{
	/** Window ("trace", "combo", "cancel", "iframes") of a clip (asset name, e.g. A_SwordAndShieldSlash) as fractions of
	 *  its length (0..1), so a clip stretched to an ability's duration scales them with it. False if it has none. */
	static bool Find(FName Clip, FName Window, float& OutStart, float& OutEnd);
};
