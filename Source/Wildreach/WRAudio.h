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
	float Duck = 0.f;             // music dips to (1 - Duck) while this plays (big moments; capped at music.duckMax)
	/** A tell that rises in pitch while it plays (cookpass-audio 4.5: CAL "sound before sight"): from, to, seconds. */
	FVector Rise = FVector::ZeroVector;
	/** Sound captions (cookpass-audio 4.9): the line shown ("{who} snarls"; {who} = the creature's name), and whether it's
	 *  a danger (shown when captions are on Important). Empty: no caption. */
	FString Caption;
	bool bDanger = false;
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
	/** Its tempo and metre, so changes land on its beat or bar line (cookpass-audio 4.1.4); 0 = no sync. Lead: seconds
	 *  of the file before its first downbeat. */
	float Bpm = 0.f;
	int32 Beats = 4;
	float Lead = 0.f;
};

/** A space's reverb (sounds.json "spaces", cookpass-audio 4.3): UReverbEffect numbers, how wet and how fast it comes in. */
struct FWRSpaceDef
{
	FName Id;
	float Decay = 1.5f, Density = 1.f, Diffusion = 1.f, Gain = 0.4f, GainHF = 0.6f, DecayHFRatio = 0.8f;
	float ReflectionsGain = 0.3f, ReflectionsDelay = 0.01f, LateGain = 1.f, LateDelay = 0.02f, AirAbsorptionHF = 0.994f;
	float Wet = 0.5f;
	float Fade = 0.8f;
	bool bDry = false;   // a space where sound dies (the Silent Vault): reverb off
};

/** A spot sound (sounds.json ambient.spots, cookpass-audio 4.6): a one-shot placed round the listener now and then while
 *  its layer is up, or from a fixed place within reach. */
struct FWRAmbientSpot
{
	FName Id;
	FName Event;
	TArray<FName> Layers;         // any of them (their weight is the max)
	int32 DayNight = 0;           // 1 = by day only, 2 = by night only
	float Min = 0.5f;             // the layer weight it needs
	FVector2D Every = FVector2D(30.f, 70.f);
	FVector2D Dist = FVector2D(25.f, 50.f);   // metres
	FVector2D Up = FVector2D(0.f, 5.f);       // metres over the listener
	bool bAt = false;
	FVector2D At = FVector2D::ZeroVector;     // metres (island x east, y south)
	float Reach = 900.f;                      // metres from At it's heard
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

	/** A looping 3D sound on an actor (machines: WRSound::StartLoop). */
	UAudioComponent* PlayLoop(AActor* Actor, FName Event);
	/** A looping or long 2D stream (music, ambient beds) the director fades itself. */
	UAudioComponent* CreateStream(UWorld* World, const FString& File, bool bLoop);
	float StreamSeconds(const FString& File) const;

	const TArray<FWRAmbientBed>& GetBeds() const { return Beds; }
	const TArray<FWRAmbientSpot>& GetSpots() const { return Spots; }
	const FWRSpaceDef* FindSpace(FName Id) const { return Spaces.Find(Id); }
	/** The runtime reverb object of a space (made once from its row). */
	class UReverbEffect* SpaceEffect(FName Id);
	/** Moves the pitch of rising tells (FWRSoundEvent::Rise); the director calls it every tick. */
	void StepRises();
	/** The master dynamics (cookpass-audio 4.4): a limiter always, and Night's compressor ahead of it when the player's
	 *  Dynamic range setting is Night. Safe to call again when the setting changes. */
	void ApplyDynamics(const UObject* WorldContext);
	const TArray<FWRMusicTrack>* GetMusicPool(FName Pool) const { return MusicPools.Find(Pool); }
	float Tune(FName Key, float Fallback) const;
	/** Per-creature voice settings: family and pitch ("creatures" in the table). */
	bool CreatureVoice(FName CreatureId, FName& OutFamily, float& OutPitch) const;
	/** A creature call kind with no row of its own plays another kind, pitched and quieter (sounds.json voiceFallback:
	 *  alert -> idle, flee -> hurt, answer -> idle; cooks/cookpass-creatures.md 3.4). */
	bool VoiceFallback(FName Kind, FName& OutKind, float& OutPitch, float& OutVolume) const;

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
	struct FVoiceFallback { FName Kind; float Pitch = 1.f; float Volume = 1.f; };
	TMap<FName, FVoiceFallback> VoiceFallbacks;
	TSet<FName> Unknown;                  // unknown events already logged
	TMap<FName, FWRSpaceDef> Spaces;
	TArray<FWRAmbientSpot> Spots;
	struct FRising { TWeakObjectPtr<UAudioComponent> Comp; double Start = 0.0; FVector Rise; float BasePitch = 1.f; };
	TArray<FRising> Rising;
	bool bDynamicsAdded = false;
	int32 DynamicsMode = -1;

	UPROPERTY(Transient) TMap<FString, TObjectPtr<UWRPcmSound>> Sounds;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UWRPcmSound>> LoopSounds;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<USoundAttenuation>> Attenuations;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class UReverbEffect>> SpaceEffects;
	UPROPERTY(Transient) TObjectPtr<class USubmixEffectDynamicsProcessorPreset> NightPreset;
	UPROPERTY(Transient) TObjectPtr<class USubmixEffectDynamicsProcessorPreset> LimiterPreset;

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
		/** Seconds of it heard so far (paused time not counted: the bar clock), and its tempo (FWRMusicTrack). */
		double Played = 0.0;
		float Bpm = 0.f;
		int32 Beats = 4;
		float Lead = 0.f;
		/** Real time it starts its fade (on a bar line); 0 = none pending. */
		double FadeAt = 0.0;
	};
	/** Seconds from now to this stream's next beat (bBar: bar line), capped at music.syncMax; 0 when it has no tempo. */
	float NextBoundary(const FStream& S, bool bBar) const;
	/** Starts the stream's fade on its next bar line (or now when it has no tempo). */
	void FadeOnBar(FStream& S, float Seconds);
	/** Switches the music mode now (the pending change's moment arrived). */
	void SwitchMode(EMusic Want, double Now);
	/** How much of a fight the creatures on the listener make (cookpass-audio 4.1.3). */
	float ThreatOf(const class AWRCreature* C) const;
	/** The hidden tube (Kiln Roads, Blue Below) the listener is inside; None outside. */
	FName TubeAt() const;
	/** The reverb space the listener is in (sounds.json spaces; cookpass-audio 4.3), applied when it changes. */
	void UpdateSpace();
	FName SpaceNow() const;
	bool UnderBuiltRoof() const;
	/** Spot sounds (ambient.spots). */
	void UpdateSpots(double Now);

	void UpdateListener();
	void UpdateAmbient(float DeltaTime);
	void UpdateMusic(float DeltaTime);
	void WatchBodies();
	void OnHitShown(const AActor* Target, float Damage, EWRHitResult Result);
	float LayerWeight(FName Layer) const;
	float SampleCoast();
	float CaveFactor() const;
	float LavaFactor() const;
	/** The Ancient forest's listener state (UpdateAmbient): its air, sub-zone beds, the Great Falls, the Hollow Root. */
	void UpdateForest(float DeltaTime);
	/** The phase of the engaged Sylvaroth near the listener (its music turns with it); 1 when there is none. */
	int32 ForestBossPhaseNow() const;
	EMusic WantedMusic();
	void StartMusic(EMusic Which);
	bool StepStream(FStream& S, float DeltaTime, float Gain);
	float Tune(const TCHAR* Key, float Fallback) const;
	/** The swamp's listener state (UpdateAmbient). */
	void UpdateSwamp(float DeltaTime);
	/** A music layer over whatever plays (the Great Storm's drums, the Red Tide's pulse): fades in while wanted. */
	void StepMusicLayer(TArray<FStream>& Layer, bool bWant, FName Pool, float DeltaTime, float Gain);

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
	/** A runtime land the listener stands in (WRLand), its sub-zone, and how far a planned silence (Dead calm) has fallen. */
	FName LandBiome;
	FName LandZone;
	/** The Fire / Frost sub-zone the listener stands in (WRIsle::ZoneAt): its bed (layer ffzone.<zone>) over the island's. */
	FName IsleZone;
	float Hush = 0.f;
	/** The Fungal caverns (cooks/biome-fungal.md 2.16): the zone the listener is in (F1 on the plateau, F2-F5 and HG under
	 *  it), the Flue's roar on the Fall Field, and when the Fall Field was stepped onto (its 20 s planned silence). */
	FName FungalZone;
	float FlueRoar = 0.f;
	double FallFieldSince = -1.0;
	TArray<FStream> SporefallLayer;
	/** The Ocean (cooks/biome-ocean.md 2.15-2.16): the King Tide's layer, and how far under the water the listener is
	 *  (0..1: every bed and tune muffled through a low-pass as it rises; the underwater bed comes up). */
	TArray<FStream> KingTideLayer;
	float UnderBlend = 0.f;
	/** The Desert (cooks/biome-desert.md 2.15-2.16): the Long Noon's layer, and how deep in the storm belt or a
	 *  sandstorm the listener is (0..1: the music low-passed under the wind, never muted). */
	TArray<FStream> LongNoonLayer;
	float BeltBlend = 0.f;
	/** Storm peaks (cooks/biome-lightning.md 2.12): how far into its air, and each sub-zone's bed weight (1-6 as the
	 *  zone letters, eased), the Rodfield's wind-song by distance. */
	float Storm = 0.f;
	float StormZone[7] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float Rodfield = 0.f;
	/** The Swamp (cooks/biome-swamp.md 2.2, 2.16): how far into its air (eased), each sub-zone's bed weight (M1..M5 as
	 *  '1'..'5', eased), the falls' roar by distance to the Weeping Wall, the Sunken Coven, bleedwater round you, the
	 *  Red Tide. */
	float Swamp = 0.f;
	float SwampZone[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float SwampFalls = 0.f;
	float SwampCoven = 0.f;
	float SwampBleed = 0.f;
	float SwampTide = 0.f;
	/** The Red Tide's pulse layer over whatever plays (music.pools layer.red_tide). */
	TArray<FStream> RedTideLayer;
	/** The engaged boss's phase, and the phase its music was picked for (boss.<id>.p<n> pools). */
	int32 BossPhase = 1;
	int32 MusicPhase = 1;
	/** The Great Storm's war-drum layer over whatever plays (music.pools layer.great_storm). */
	float Daylight = 1.f;
	float High = 0.f;
	double NextCaveScan = 0.0;
	TArray<TWeakObjectPtr<AActor>> Caves;
	double NextSpot = 0.0;
	/** The Ancient forest (cooks/biome-forest.md 2.2, 2.16, 3.4): how far into its air (eased), each sub-zone's bed
	 *  weight ('1'..'5' as the zone letters, eased; [6] = the Hollow Root, where the open-air beds give way), and the Great
	 *  Falls' roar by distance. */
	float Forest = 0.f;
	float ForestZone[7] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float ForestFalls = 0.f;
	/** Music layers over whatever plays in the forest (music.pools layer.bloom / layer.wild_hunt). */
	TArray<FStream> BloomLayer;
	TArray<FStream> TurningWindLayer;
	TArray<FStream> WildHuntLayer;
	/** Sylvaroth's phase its music was picked for (boss.sylvaroth.p<n>), and when a seen phase change is acted on (a short
	 *  wait, so a second phase-music switch elsewhere never restarts the same piece twice). */
	int32 ForestMusicPhase = 1;
	double ForestPhaseSwitchAt = 0.0;
	double NextForestPhaseCheck = 0.0;

	// Spaces and spots
	FName Space;
	double NextRoofCheck = 0.0;
	bool bRoof = false;
	double NextTubeScan = 0.0;
	TArray<TWeakObjectPtr<AActor>> Tubes;
	FName Tube;
	TMap<FName, double> SpotNext;
	double LastRealTime = 0.0;

	// Music
	EMusic Mode = EMusic::None;
	/** A change waiting for the playing piece's beat or bar (cookpass-audio 4.1.4). */
	EMusic PendingMode = EMusic::None;
	bool bPending = false;
	double PendingAt = 0.0;
	/** A boss phase change waiting for its bar. */
	double PhaseSwitchAt = 0.0;
	/** The boss the music followed (its fall plays the stinger and the hush), and the hush's end. */
	TWeakObjectPtr<class AWRBoss> MusicBoss;
	double HushUntil = 0.0;
	/** The listener's last blow taken from a player (PvP: a fight for the music) and from anything. */
	double PlayerHitAt = -100.0;
	double HitAt = -100.0;
	/** Dead: the music dips (at most the cap) and no new piece starts. */
	float DeadDip = 1.f;
	TArray<FStream> EruptionLayer;
	TArray<FStream> WhiteoutLayer;
	FName ForcedMode;
	TArray<FStream> Music;        // the current track last; older ones fading out
	double NextExploreAt = 0.0;
	double CombatUntil = -1.0;
	double NextEngageCheck = 0.0;
	bool bEngaged = false;
	bool bBoss = false;
	/** The engaged boss's creature id (its own music pool: boss.<id>, or its land's mini pool). */
	FName BossId;
	FString LastExplore;
	TArray<FStream> GreatStormLayer;
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
		uint8 Mood = 0;            // the creature's mood when last watched (alert / flee calls on the change)
		double NextAlert = 0.0;
		double NextBreath = 0.0;   // asleep: its next breath (cookpass-audio 4.8)
	};
	TMap<TWeakObjectPtr<AActor>, FBody> Bodies;
	/** Packmates answering a contact call (cookpass-creatures 3.4): who and when. */
	TArray<TPair<TWeakObjectPtr<class AWRCreature>, double>> Answers;
	/** After an idle call: one packmate in earshot may answer it a moment later. */
	void QueueAnswer(class AWRCreature* Caller, double Now);
	double NextWatch = 0.0;
};
