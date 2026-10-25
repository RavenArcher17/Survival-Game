#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "WRGameState.generated.h"

/**
 * Replicates the server's settings (preset + every server.* value) so client-side rules match: lock-on, PvP, HUD.
 * Also the world clock: the server anchors an hour to its world time, and everyone works the current hour out from the
 * replicated server time (clock.dayMinutes real minutes per day), so nothing replicates every tick.
 */
UCLASS()
class WILDREACH_API AWRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_ServerSettings) FString Preset;
	UPROPERTY(ReplicatedUsing=OnRep_ServerSettings) TArray<FString> ServerSettings;

	// --- World clock ---
	/** Hour of the day, 0..24. */
	float GetHour() const;
	/** Whole days since the clock started (day 1 = the first). */
	int32 GetDay() const;
	/** Between clock.duskHour and clock.dawnHour. */
	bool IsNight() const;
	/** 0 at night, 1 in full day, easing through dawn and dusk (lighting, creatures that come out at dusk). */
	float GetDaylight() const;
	/** Server: jump the clock to an hour (later the same day, or the next day if it's already past). */
	void SetHour(float Hour);
	/** Server: restore a saved clock exactly (total hours since day 1, 00:00). */
	void SetTotalHours(double Hours);
	double GetTotalHours() const;

	/** The world's game state, or null (e.g. before it exists). */
	static const AWRGameState* Get(const UObject* WorldContext);
	/** Convenience for code that only needs the time: 12 / false when there's no game state yet. */
	static float HourIn(const UObject* WorldContext);
	static bool IsNightIn(const UObject* WorldContext);

private:
	UFUNCTION() void OnRep_ServerSettings();

	/** Clock anchor: total hours (since day 1, 00:00) at server world time ClockAnchorTime. */
	UPROPERTY(Replicated) double ClockAnchorHours = 8.0;
	UPROPERTY(Replicated) double ClockAnchorTime = 0.0;
	/** Server: set before BeginPlay (the world save's time may load first), so the start hour doesn't replace it. */
	bool bClockSet = false;
};
