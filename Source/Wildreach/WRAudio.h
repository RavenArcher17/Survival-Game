#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRCombatTypes.h"
#include "WRAudio.generated.h"

class AActor;
class UAudioComponent;
class USoundAttenuation;

/**
 * The sound pass (see WRSound.h for the event names). Sounds are OGG Vorbis files in Data/Audio, loaded at runtime
 * like the UI's fonts and pictures, so the repo holds small compressed files instead of imported assets and the
 * packaged game finds them in the Data folder Tools\package.ps1 copies beside it. Short sounds are decoded once into
 * memory; music and ambient beds stay compressed and are decoded as they play.
 */

/** One file's audio, shared with the audio render thread (never changed after loading). */
struct FWRAudioData
{
	TArray<int16> Pcm;          // decoded, interleaved (short sounds)
	TArray<uint8> Compressed;   // the OGG file itself (streams: music, ambient beds)
	int32 Channels = 1;
	int32 SampleRate = 44100;
	int64 Frames = 0;
	bool bStream = false;
};

/** A procedural sound wave that plays one Data/Audio file through its own generator (the mixer treats it like any
 *  other sound: 3D, attenuation, pitch, virtualisation). One per file; every play gets its own generator. */
UCLASS(Transient)
class WILDREACH_API UWRPcmSound : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	void InitFrom(const TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe>& InData, bool bLoop, const FString& InFile);
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;
	const FString& GetFile() const { return File; }
	float GetSeconds() const;

private:
	TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe> Data;
	bool bLoopSound = false;
	FString File;
};

/** One entry of the sound table. */
struct FWRSoundEvent
{
	FName Name;
	TArray<FString> Files;
	float Volume = 1.f;
	float PitchMin = 0.96f;
	float PitchMax = 1.04f;
	float Radius = 2500.f;        // 0 = 2D (UI, moments, the local player's own voice when bSelf2D)
	FName Category = TEXT("effects");
	float MinGap = 0.05f;         // seconds before the same event can play again (per actor, or per spot for PlayAt)
	int32 MaxVoices = 6;          // at most this many at once
	float Duck = 0.f;             // music dips to (1 - Duck) while this plays (big moments)
	int32 LastVariant = -1;
};

struct FWRAmbientBed
{
	FName Id;
	FString File;
	float Volume = 0.5f;
	FName Layer;                  // what turns it up: volcanic, tundra, coast, day, night, cave, lava, high, ...
};

struct FWRMusicTrack
{
	FString File;
	float Volume = 1.f;
};

/**
 * The sound table and the loaded files (one per game instance), plus the one-shot player behind WRSound.
 */
UCLASS()
class WILDREACH_API UWRAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	static UWRAudioSubsystem* Get(const UObject* WorldContext);

	/** Loads Data/Audio/sounds.json and its files the first time (false: no audio device, or no table). */
	bool EnsureLoaded();
	const FWRSoundEvent* FindEvent(FName Event) const;

	struct FPlay
	{
		FName Event;
		TWeakObjectPtr<UWorld> World;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<AActor> Follow;   // attach to this actor (PlayOn)
		bool b2D = false;
		float VolumeScale = 1.f;
		float PitchScale = 1.f;
		FName GapKeyExtra;               // e.g. the surface for footsteps
	};
	/** Plays an event; returns the component (null when held back, unknown, out of earshot or silent). */
	UAudioComponent* Play(const FPlay& P);

	/** A looping or long 2D stream (music, ambient beds) the director fades itself. */
	UAudioComponent* CreateStream(UWorld* World, const FString& File, bool bLoop);
	float StreamSeconds(const FString& File) const;

	const TArray<FWRAmbientBed>& GetBeds() const { return Beds; }
	const TArray<FWRMusicTrack>* GetMusicPool(FName Pool) const { return MusicPools.Find(Pool); }
	float Tune(FName Key, float Fallback) const;
	/** Per-creature voice settings: family and pitch ("creatures" in the table). */
	bool CreatureVoice(FName CreatureId, FName& OutFamily, float& OutPitch) const;

	void Log(const FString& Line) const;
	bool IsLogging() const;
	/** Testing: decode a Data/Audio file through the generator the game plays it with (streams: the first seconds) and
	 *  report what came out. False with OutError when the file is missing or isn't Ogg Vorbis. */
	static bool ProbeFile(const FString& File, bool bStream, int32& OutChannels, int32& OutSampleRate, float& OutSeconds, float& OutPeak, FString& OutError);
	FString Stats() const;
	void ResetStats() { Played.Empty(); Held.Empty(); }

private:
	UWRPcmSound* LoadFile(const FString& File, bool bStream, bool bLoop);
	USoundAttenuation* AttenuationFor(float Radius);
	bool bLoaded = false;
	bool bTried = false;

	TMap<FName, FWRSoundEvent> Events;
	TArray<FWRAmbientBed> Beds;
	TMap<FName, TArray<FWRMusicTrack>> MusicPools;
	TMap<FName, float> Settings;          // "music" / "ambient" numbers
	TMap<FName, TPair<FName, float>> Creatures;
	TSet<FName> Unknown;                  // unknown events already logged

	UPROPERTY(Transient) TMap<FString, TObjectPtr<UWRPcmSound>> Sounds;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UWRPcmSound>> LoopSounds;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<USoundAttenuation>> Attenuations;

	TMap<FName, TArray<TWeakObjectPtr<UAudioComponent>>> Voices;
	TMap<uint32, double> LastPlayed;
	double NextPrune = 0.0;
	TMap<FName, int32> Played;
	TMap<FName, int32> Held;
};

/**
 * Per game world (not on a dedicated server): ambient beds crossfaded by where the listener is (which side of the
 * ridge, time of day from the world clock, how near the sea, in a cave, by lava), music (the menu track, exploring
 * tracks now and then, combat when something is after you, the boss track in a boss fight; ducked under big moments),
 * and the body sounds no single place in gameplay owns: jumps, landings, dodges, creature calls (idle, aggro) and hurt
 * voices, watched for every character near the listener.
 */
UCLASS()
class WILDREACH_API UWRAudioDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted; }
	virtual bool IsTickableWhenPaused() const override { return true; }

	static UWRAudioDirector* Get(const UObject* WorldContext);
	/** Music dips to (1 - Amount) for Seconds (big moments). */
	void Duck(float Amount, float Seconds);
	/** What the mixer and music are doing (logs, tests): "music explore:file 0.40 | volcanic 0.80 day 1.00 ...". */
	FString Describe() const;
	/** Testing: force a music mode (menu, explore, combat, boss; empty = automatic) and start an explore track now. */
	void ForceMusic(FName InMode) { ForcedMode = InMode; NextExploreAt = 0.0; }

private:
	enum class EMusic : uint8 { None, Menu, Explore, Combat, Boss };

	struct FStream
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		FString File;
		float Base = 1.f;      // its table volume
		float Gain = 0.f;      // current fade, 0..1
		float Target = 0.f;
		float FadeSeconds = 2.f;
		double StartedAt = 0.0;
	};

	void UpdateListener();
	void UpdateAmbient(float DeltaTime);
	void UpdateMusic(float DeltaTime);
	void WatchBodies();
	void OnHitShown(const AActor* Target, float Damage, EWRHitResult Result);
	float LayerWeight(FName Layer) const;
	float SampleCoast();
	float CaveFactor() const;
	float LavaFactor() const;
	EMusic WantedMusic();
	void StartMusic(EMusic Which);
	bool StepStream(FStream& S, float DeltaTime, float Gain);
	float Tune(const TCHAR* Key, float Fallback) const;

	bool bStarted = false;
	FDelegateHandle HitHandle;
	/** The streams' components (created without an owner: held here so they live while they play). */
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Held;
	double NextDescribe = 0.0;
	double NextLavaCheck = 0.0;

	// Listener
	FVector Listener = FVector::ZeroVector;
	bool bHasListener = false;
	TWeakObjectPtr<APawn> MyPawn;

	// Ambient
	TMap<FName, FStream> Beds;
	float Coast = 0.f;
	float CoastTarget = 0.f;
	TArray<uint8> CoastSamples;   // 1 = water, per direction x radius
	int32 CoastCursor = 0;
	double NextCoastSample = 0.0;
	float Cave = 0.f;
	float Lava = 0.f;
	float Tundra = 0.f;
	float Daylight = 1.f;
	float High = 0.f;
	double NextCaveScan = 0.0;
	TArray<TWeakObjectPtr<AActor>> Caves;
	double NextSpot = 0.0;

	// Music
	EMusic Mode = EMusic::None;
	FName ForcedMode;
	TArray<FStream> Music;        // the current track last; older ones fading out
	double NextExploreAt = 0.0;
	double CombatUntil = -1.0;
	double NextEngageCheck = 0.0;
	bool bEngaged = false;
	bool bBoss = false;
	FString LastExplore;
	float DuckLevel = 1.f;
	float DuckTarget = 1.f;
	double DuckUntil = 0.0;

	// Bodies
	struct FBody
	{
		bool bFalling = false;
		double FallStart = 0.0;
		float FallSpeed = 0.f;
		bool bDodging = false;
		bool bDead = false;
		TWeakObjectPtr<AActor> Aggro;
		double NextIdle = 0.0;
		double Seen = 0.0;
	};
	TMap<TWeakObjectPtr<AActor>, FBody> Bodies;
	double NextWatch = 0.0;
};
