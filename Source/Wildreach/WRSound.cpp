#include "WRSound.h"

// Stub: the calls exist so gameplay can name its sounds; the sound table and playback come with the audio pass.

void WRSound::PlayAt(const UObject* WorldContext, FName Event, const FVector& Location, float VolumeScale) {}
void WRSound::PlayOn(AActor* Actor, FName Event, float VolumeScale) {}
void WRSound::Footstep(AActor* Actor, float VolumeScale) {}
void WRSound::PlayUI(const UObject* WorldContext, FName Event) {}
