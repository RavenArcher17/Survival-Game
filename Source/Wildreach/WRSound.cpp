#include "WRSound.h"
#include "WRAudio.h"
#include "Wildreach.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRTuning.h"
#include "WRWorldDressing.h"
#include "Async/ParallelFor.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Decoders/VorbisAudioInfo.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IAudioFormat.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundGenerator.h"

// Helpers live in a named namespace: unity builds join this file with others.
namespace WRSoundDetail
{
	static TAutoConsoleVariable<int32> CVarSoundLog(TEXT("wr.SoundLog"), 0,
		TEXT("Log every sound event played: 1 = plays (event, file, volume, where), 2 = also the ones the minimum gap or voice limit held back."));

	static int32 SoundLogLevel()
	{
		static const bool bCommandLine = FParse::Param(FCommandLine::Get(), TEXT("wrsoundlog"));
		return FMath::Max(CVarSoundLog.GetValueOnGameThread(), bCommandLine ? 1 : 0);
	}

	/** The player's volume settings (Settings > Audio), kept in GameUserSettings.ini. */
	static const TCHAR* VolumeSection = TEXT("Wildreach.Audio");
	static TMap<FName, float>& Volumes()
	{
		static TMap<FName, float> V;
		return V;
	}
	static float DefaultVolume(FName Category)
	{
		return Category == TEXT("music") ? 0.8f : Category == TEXT("ui") ? 0.8f : Category == TEXT("ambient") ? 0.9f : 1.f;
	}
	/** The table's mix trims per category (sounds.json "mix"), set when it loads. */
	static TMap<FName, float>& MixTrim()
	{
		static TMap<FName, float> M;
		return M;
	}

	static UWorld* WorldOf(const UObject* Context)
	{
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		if (!World && GEngine && GEngine->GameViewport)
		{
			World = GEngine->GameViewport->GetWorld();
		}
		return World;
	}

	static bool CanPlayIn(const UWorld* World)
	{
		return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && !IsRunningDedicatedServer();
	}

	static uint32 GapKey(FName Event, const void* Who, const FVector& Where, bool bSpot, FName Extra)
	{
		uint32 H = HashCombine(GetTypeHash(Event), GetTypeHash(Extra));
		if (Who) return HashCombine(H, GetTypeHash(Who));
		if (bSpot) return HashCombine(H, GetTypeHash(FIntVector(FMath::FloorToInt(Where.X / 200.f), FMath::FloorToInt(Where.Y / 200.f), FMath::FloorToInt(Where.Z / 200.f))));
		return H;
	}

	static FString AudioDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FWRTuning::DataDir(), TEXT("Audio"))); }

	static TSharedPtr<FWRAudioData, ESPMode::ThreadSafe> ReadOgg(const FString& File, bool bStream, FString& OutError)
	{
		const FString Path = FPaths::Combine(AudioDir(), File + TEXT(".ogg"));
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			OutError = FString::Printf(TEXT("missing %s"), *Path);
			return nullptr;
		}
		FVorbisAudioInfo Info;
		FSoundQualityInfo Quality;
		if (!Info.ReadCompressedInfo(Bytes.GetData(), Bytes.Num(), &Quality) || Quality.NumChannels == 0 || Quality.SampleRate == 0)
		{
			OutError = FString::Printf(TEXT("not Ogg Vorbis: %s"), *Path);
			return nullptr;
		}
		TSharedPtr<FWRAudioData, ESPMode::ThreadSafe> Data = MakeShared<FWRAudioData, ESPMode::ThreadSafe>();
		Data->Channels = Quality.NumChannels;
		Data->SampleRate = Quality.SampleRate;
		Data->bStream = bStream;
		if (bStream)
		{
			Data->Frames = FMath::RoundToInt64(Quality.Duration * Quality.SampleRate);
			Data->Compressed = MoveTemp(Bytes);
			return Data;
		}
		Data->Pcm.SetNumZeroed(Quality.SampleDataSize / sizeof(int16));
		Info.ExpandFile(reinterpret_cast<uint8*>(Data->Pcm.GetData()), &Quality);
		Data->Frames = Data->Pcm.Num() / Data->Channels;
		return Data;
	}

	/** Plays decoded PCM from memory (short sounds). */
	class FPcmGenerator final : public ISoundGenerator
	{
	public:
		FPcmGenerator(const TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe>& InData, bool bInLoop) : Data(InData), bLoop(bInLoop) {}

		virtual int32 OnGenerateAudio(float* Out, int32 NumSamples) override
		{
			const int32 Total = Data->Pcm.Num();
			int32 Written = 0;
			while (Written < NumSamples)
			{
				if (Cursor >= Total)
				{
					if (bLoop && Total > 0)
					{
						Cursor = 0;
						continue;
					}
					// The tail: silence for the rest of this buffer, then one more, then the source is done.
					FMemory::Memzero(Out + Written, (NumSamples - Written) * sizeof(float));
					bDone = bEnded;
					bEnded = true;
					break;
				}
				const int32 N = FMath::Min(NumSamples - Written, Total - Cursor);
				const int16* Src = Data->Pcm.GetData() + Cursor;
				for (int32 i = 0; i < N; ++i)
				{
					Out[Written + i] = Src[i] * (1.f / 32768.f);
				}
				Cursor += N;
				Written += N;
			}
			return NumSamples;
		}
		virtual bool IsFinished() const override { return bDone; }
		virtual int32 GetNumChannels() const override { return Data->Channels; }
		virtual int32 GetDesiredNumSamplesToRenderPerCallback() const override { return 1024 * Data->Channels; }

	private:
		TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe> Data;
		bool bLoop = false;
		int32 Cursor = 0;
		bool bEnded = false;
		bool bDone = false;
	};

	/** Decodes an OGG file as it plays (music, ambient beds): only the compressed file sits in memory. */
	class FVorbisGenerator final : public ISoundGenerator
	{
	public:
		FVorbisGenerator(const TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe>& InData, bool bInLoop) : Data(InData), bLoop(bInLoop)
		{
			FSoundQualityInfo Quality;
			bOk = Info.ReadCompressedInfo(Data->Compressed.GetData(), Data->Compressed.Num(), &Quality) && Quality.NumChannels == static_cast<uint32>(Data->Channels);
		}

		virtual int32 OnGenerateAudio(float* Out, int32 NumSamples) override
		{
			if (!bOk || bEnded)
			{
				FMemory::Memzero(Out, NumSamples * sizeof(float));
				bDone = true;
				return NumSamples;
			}
			Scratch.SetNumUninitialized(NumSamples);
			const bool bEnd = Info.ReadCompressedData(reinterpret_cast<uint8*>(Scratch.GetData()), bLoop, NumSamples * sizeof(int16));
			for (int32 i = 0; i < NumSamples; ++i)
			{
				Out[i] = Scratch[i] * (1.f / 32768.f);
			}
			if (bEnd && !bLoop)
			{
				bEnded = true;   // the decoder padded the rest with silence
			}
			return NumSamples;
		}
		virtual bool IsFinished() const override { return bDone; }
		virtual int32 GetNumChannels() const override { return Data->Channels; }
		virtual int32 GetDesiredNumSamplesToRenderPerCallback() const override { return 2048 * Data->Channels; }

	private:
		TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe> Data;
		FVorbisAudioInfo Info;
		TArray<int16> Scratch;
		bool bLoop = false;
		bool bOk = false;
		bool bEnded = false;
		bool bDone = false;
	};

	static float JsonNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? static_cast<float>(V) : Fallback;
	}

	/** Size class and voice family of a creature (see WRSound.h). */
	static void CreatureClass(const AWRCreature* C, FName& OutSize, FName& OutFamily, float& OutSizeCm)
	{
		const FWRCreatureDef* Def = C->GetDef();
		const float Radius = Def ? Def->Radius : C->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float Half = Def ? Def->HalfHeight : C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		OutSizeCm = FMath::Max(Radius, Half * 0.8f);
		OutSize = OutSizeCm < 35.f ? FName(TEXT("small")) : OutSizeCm < 70.f ? FName(TEXT("medium")) : FName(TEXT("large"));
		const FName A = Def ? Def->Archetype : NAME_None;
		OutFamily = Cast<AWRBoss>(C) || A == TEXT("boss") ? FName(TEXT("boss"))
			: A == TEXT("pack_hunter") ? FName(TEXT("canine"))
			: A == TEXT("ground_predator") ? FName(TEXT("beast"))
			: A == TEXT("flyer") ? FName(TEXT("flyer"))
			: A == TEXT("burrower") ? FName(TEXT("bug"))
			: A == TEXT("herd") ? FName(TEXT("herd"))
			: A == TEXT("critter") ? FName(TEXT("critter"))
			: NAME_None;   // the training dummy has no voice
	}
}

using namespace WRSoundDetail;

// ---------------------------------------------------------------- the procedural wave

void UWRPcmSound::InitFrom(const TSharedPtr<const FWRAudioData, ESPMode::ThreadSafe>& InData, bool bLoop, const FString& InFile)
{
	Data = InData;
	bLoopSound = bLoop;
	File = InFile;
	NumChannels = Data->Channels;
	SetSampleRate(Data->SampleRate);
	bLooping = bLoop;
	Duration = bLoop ? INDEFINITELY_LOOPING_DURATION : FMath::Max(0.05f, static_cast<float>(Data->Frames) / Data->SampleRate);
	bCanProcessAsync = true;
	// Streams (music, beds) keep playing while faded right down; one-shots just don't start out of earshot.
	VirtualizationMode = Data->bStream ? EVirtualizationMode::PlayWhenSilent : EVirtualizationMode::Disabled;
	// When voices run out the engine drops the lowest priority first: never the music or the beds.
	Priority = Data->bStream ? 4.f : 1.f;
}

ISoundGeneratorPtr UWRPcmSound::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	if (!Data.IsValid())
	{
		return nullptr;
	}
	if (Data->bStream)
	{
		return MakeShared<FVorbisGenerator, ESPMode::ThreadSafe>(Data, bLoopSound);
	}
	return MakeShared<FPcmGenerator, ESPMode::ThreadSafe>(Data, bLoopSound);
}

float UWRPcmSound::GetSeconds() const
{
	return Data.IsValid() && Data->SampleRate > 0 ? static_cast<float>(Data->Frames) / Data->SampleRate : 0.f;
}

// ---------------------------------------------------------------- the table

bool UWRAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void UWRAudioSubsystem::Deinitialize()
{
	Voices.Empty();
	Sounds.Empty();
	LoopSounds.Empty();
	Super::Deinitialize();
}

UWRAudioSubsystem* UWRAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldOf(WorldContext);
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UWRAudioSubsystem>() : nullptr;
}

bool UWRAudioSubsystem::EnsureLoaded()
{
	if (bTried)
	{
		return bLoaded;
	}
	bTried = true;
	if (IsRunningDedicatedServer() || !GEngine || !GEngine->UseSound() || !GEngine->GetMainAudioDevice().IsValid())
	{
		UE_LOG(LogWildreach, Log, TEXT("AUDIO no audio device: sounds off"));
		return false;
	}
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("Audio/sounds.json"));
	if (!Root.IsValid())
	{
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	LoadVorbisLibraries();

	// Mix trims per category.
	const TSharedPtr<FJsonObject>* Mix = nullptr;
	if (Root->TryGetObjectField(TEXT("mix"), Mix))
	{
		for (const auto& Pair : (*Mix)->Values) MixTrim().Add(FName(*Pair.Key), static_cast<float>(Pair.Value->AsNumber()));
	}
	const TSharedPtr<FJsonObject>* DefaultsObj = nullptr;
	Root->TryGetObjectField(TEXT("defaults"), DefaultsObj);
	const TSharedPtr<FJsonObject> Defaults = DefaultsObj ? *DefaultsObj : nullptr;

	// Events.
	TSet<FString> ShortFiles;
	const TSharedPtr<FJsonObject>* EventsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("events"), EventsObj))
	{
		for (const auto& Pair : (*EventsObj)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			if (!O.IsValid()) continue;
			FWRSoundEvent E;
			E.Name = FName(*Pair.Key);
			const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
			if (O->TryGetArrayField(TEXT("files"), Files))
			{
				for (const TSharedPtr<FJsonValue>& F : *Files) E.Files.Add(F->AsString());
			}
			E.Volume = JsonNum(O, TEXT("volume"), 1.f);
			const TArray<TSharedPtr<FJsonValue>>* Pitch = nullptr;
			if (O->TryGetArrayField(TEXT("pitch"), Pitch) || (Defaults.IsValid() && Defaults->TryGetArrayField(TEXT("pitch"), Pitch)))
			{
				if (Pitch->Num() >= 2)
				{
					E.PitchMin = static_cast<float>((*Pitch)[0]->AsNumber());
					E.PitchMax = static_cast<float>((*Pitch)[1]->AsNumber());
				}
			}
			E.Radius = JsonNum(O, TEXT("radius"), JsonNum(Defaults, TEXT("radius"), 2500.f));
			FString Cat;
			E.Category = O->TryGetStringField(TEXT("category"), Cat) ? FName(*Cat) : FName(TEXT("effects"));
			E.MinGap = JsonNum(O, TEXT("minGap"), JsonNum(Defaults, TEXT("minGap"), 0.05f));
			E.MaxVoices = FMath::RoundToInt(JsonNum(O, TEXT("maxVoices"), JsonNum(Defaults, TEXT("maxVoices"), 6.f)));
			E.Duck = JsonNum(O, TEXT("duck"), 0.f);
			for (const FString& F : E.Files) ShortFiles.Add(F);
			Events.Add(E.Name, E);
		}
	}

	// Creature voices.
	const TSharedPtr<FJsonObject>* CreaturesObj = nullptr;
	if (Root->TryGetObjectField(TEXT("creatures"), CreaturesObj))
	{
		for (const auto& Pair : (*CreaturesObj)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			FString Family;
			if (O.IsValid()) O->TryGetStringField(TEXT("family"), Family);
			Creatures.Add(FName(*Pair.Key), TPair<FName, float>(Family.IsEmpty() ? NAME_None : FName(*Family), JsonNum(O, TEXT("pitch"), 1.f)));
		}
	}
	// Ambient beds and music: numbers into Settings ("ambient.x" / "music.x"), files stream.
	for (const TCHAR* Section : { TEXT("ambient"), TEXT("music") })
	{
		const TSharedPtr<FJsonObject>* S = nullptr;
		if (!Root->TryGetObjectField(Section, S)) continue;
		for (const auto& Pair : (*S)->Values)
		{
			if (Pair.Value->Type == EJson::Number)
			{
				Settings.Add(FName(*FString::Printf(TEXT("%s.%s"), Section, *Pair.Key)), static_cast<float>(Pair.Value->AsNumber()));
			}
			else if (Pair.Value->Type == EJson::Array && Pair.Value->AsArray().Num() == 2 && Pair.Value->AsArray()[0]->Type == EJson::Number)
			{
				// [min, max] ranges: key.min / key.max
				Settings.Add(FName(*FString::Printf(TEXT("%s.%s.min"), Section, *Pair.Key)), static_cast<float>(Pair.Value->AsArray()[0]->AsNumber()));
				Settings.Add(FName(*FString::Printf(TEXT("%s.%s.max"), Section, *Pair.Key)), static_cast<float>(Pair.Value->AsArray()[1]->AsNumber()));
			}
		}
		if (FCString::Strcmp(Section, TEXT("ambient")) == 0)
		{
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if ((*S)->TryGetArrayField(TEXT("beds"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) continue;
					FWRAmbientBed Bed;
					Bed.Id = FName(*O->GetStringField(TEXT("id")));
					Bed.File = O->GetStringField(TEXT("file"));
					Bed.Volume = JsonNum(O, TEXT("volume"), 0.5f);
					Bed.Layer = FName(*O->GetStringField(TEXT("layer")));
					Beds.Add(Bed);
				}
			}
		}
		else
		{
			const TSharedPtr<FJsonObject>* Pools = nullptr;
			if ((*S)->TryGetObjectField(TEXT("pools"), Pools))
			{
				for (const auto& Pair : (*Pools)->Values)
				{
					TArray<FWRMusicTrack>& Pool = MusicPools.Add(FName(*Pair.Key));
					for (const TSharedPtr<FJsonValue>& V : Pair.Value->AsArray())
					{
						const TSharedPtr<FJsonObject> O = V->AsObject();
						if (!O.IsValid()) continue;
						Pool.Add({ O->GetStringField(TEXT("file")), JsonNum(O, TEXT("volume"), 1.f) });
					}
				}
			}
		}
	}

	// Decode the short sounds now (a fraction of a second, while the world loads) so the first swing doesn't hitch.
	TArray<FString> List = ShortFiles.Array();
	TArray<TSharedPtr<FWRAudioData, ESPMode::ThreadSafe>> Decoded;
	TArray<FString> Errors;
	Decoded.SetNum(List.Num());
	Errors.SetNum(List.Num());
	ParallelFor(List.Num(), [&](int32 i) { Decoded[i] = ReadOgg(List[i], false, Errors[i]); });
	int64 Bytes = 0;
	for (int32 i = 0; i < List.Num(); ++i)
	{
		if (!Decoded[i].IsValid())
		{
			UE_LOG(LogWildreach, Warning, TEXT("AUDIO %s"), *Errors[i]);
			continue;
		}
		UWRPcmSound* Sound = NewObject<UWRPcmSound>(this);
		Sound->InitFrom(Decoded[i], false, List[i]);
		Sounds.Add(List[i], Sound);
		Bytes += Decoded[i]->Pcm.Num() * sizeof(int16);
	}
	bLoaded = true;
	UE_LOG(LogWildreach, Log, TEXT("AUDIO table: %d events, %d sounds (%.1f MB decoded), %d ambient beds, %d music pools, in %.0f ms"),
		Events.Num(), Sounds.Num(), Bytes / (1024.0 * 1024.0), Beds.Num(), MusicPools.Num(), (FPlatformTime::Seconds() - Start) * 1000.0);
	return true;
}

const FWRSoundEvent* UWRAudioSubsystem::FindEvent(FName Event) const
{
	return Events.Find(Event);
}

float UWRAudioSubsystem::Tune(FName Key, float Fallback) const
{
	const float* V = Settings.Find(Key);
	return V ? *V : Fallback;
}

bool UWRAudioSubsystem::CreatureVoice(FName CreatureId, FName& OutFamily, float& OutPitch) const
{
	const TPair<FName, float>* Found = Creatures.Find(CreatureId);
	if (!Found) return false;
	if (!Found->Key.IsNone()) OutFamily = Found->Key;
	OutPitch = Found->Value;
	return true;
}

bool UWRAudioSubsystem::IsLogging() const { return SoundLogLevel() > 0; }

bool UWRAudioSubsystem::ProbeFile(const FString& File, bool bStream, int32& OutChannels, int32& OutSampleRate, float& OutSeconds, float& OutPeak, FString& OutError)
{
	LoadVorbisLibraries();
	const TSharedPtr<FWRAudioData, ESPMode::ThreadSafe> Data = ReadOgg(File, bStream, OutError);
	if (!Data.IsValid())
	{
		return false;
	}
	OutChannels = Data->Channels;
	OutSampleRate = Data->SampleRate;
	OutSeconds = static_cast<float>(Data->Frames) / Data->SampleRate;
	// Pull buffers as the mixer does until the generator says it's done (a stream: its first 30 seconds; some tracks open very softly).
	TUniquePtr<ISoundGenerator> Gen = bStream ? TUniquePtr<ISoundGenerator>(new FVorbisGenerator(Data, false)) : TUniquePtr<ISoundGenerator>(new FPcmGenerator(Data, false));
	TArray<float> Buffer;
	Buffer.SetNumZeroed(1024 * Data->Channels);
	OutPeak = 0.f;
	const int32 MaxCalls = bStream ? FMath::CeilToInt(30.f * Data->SampleRate / 1024.f) : 100000;
	int32 Calls = 0;
	for (; Calls < MaxCalls && !Gen->IsFinished(); ++Calls)
	{
		Gen->OnGenerateAudio(Buffer.GetData(), Buffer.Num());
		for (const float S : Buffer) OutPeak = FMath::Max(OutPeak, FMath::Abs(S));
	}
	if (!bStream && !Gen->IsFinished())
	{
		OutError = TEXT("never finished");
		return false;
	}
	return true;
}

void UWRAudioSubsystem::Log(const FString& Line) const
{
	UE_LOG(LogWildreach, Log, TEXT("SOUND %s"), *Line);
}

FString UWRAudioSubsystem::Stats() const
{
	TArray<FName> Names;
	Played.GetKeys(Names);
	for (const auto& Pair : Held) Names.AddUnique(Pair.Key);
	Names.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	FString Out;
	for (const FName& N : Names)
	{
		Out += FString::Printf(TEXT("%s=%d/%d "), *N.ToString(), Played.FindRef(N), Held.FindRef(N));
	}
	return Out;
}

UWRPcmSound* UWRAudioSubsystem::LoadFile(const FString& File, bool bStream, bool bLoop)
{
	TMap<FString, TObjectPtr<UWRPcmSound>>& Map = bLoop ? LoopSounds : Sounds;
	if (TObjectPtr<UWRPcmSound>* Found = Map.Find(File))
	{
		return *Found;
	}
	FString Error;
	const TSharedPtr<FWRAudioData, ESPMode::ThreadSafe> Data = ReadOgg(File, bStream, Error);
	if (!Data.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("AUDIO %s"), *Error);
		Map.Add(File, nullptr);
		return nullptr;
	}
	UWRPcmSound* Sound = NewObject<UWRPcmSound>(this);
	Sound->InitFrom(Data, bLoop, File);
	Map.Add(File, Sound);
	return Sound;
}

float UWRAudioSubsystem::StreamSeconds(const FString& File) const
{
	const TObjectPtr<UWRPcmSound>* Found = Sounds.Find(File);
	return Found && *Found ? (*Found)->GetSeconds() : 0.f;
}

USoundAttenuation* UWRAudioSubsystem::AttenuationFor(float Radius)
{
	const int32 Key = FMath::RoundToInt(Radius / 50.f);
	if (TObjectPtr<USoundAttenuation>* Found = Attenuations.Find(Key))
	{
		return *Found;
	}
	USoundAttenuation* A = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& S = A->Attenuation;
	S.bAttenuate = true;
	S.bSpatialize = true;
	S.AttenuationShape = EAttenuationShape::Sphere;
	S.AttenuationShapeExtents = FVector(FMath::Min(Radius * 0.06f, 300.f), 0.f, 0.f);   // full volume this close
	S.FalloffDistance = Radius;
	S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	S.dBAttenuationAtMax = -48.f;
	// Far sounds lose their top end, as outdoors.
	S.bAttenuateWithLPF = true;
	S.LPFRadiusMin = Radius * 0.25f;
	S.LPFRadiusMax = Radius;
	S.LPFFrequencyAtMin = 20000.f;
	S.LPFFrequencyAtMax = 3500.f;
	// Right on top of the listener a sound isn't panned hard to one ear.
	S.NonSpatializedRadiusStart = 150.f;
	S.NonSpatializedRadiusEnd = 50.f;
	Attenuations.Add(Key, A);
	return A;
}

UAudioComponent* UWRAudioSubsystem::Play(const FPlay& P)
{
	UWorld* World = P.World.Get();
	if (!CanPlayIn(World) || !EnsureLoaded())
	{
		return nullptr;
	}
	FWRSoundEvent* E = Events.Find(P.Event);
	if (!E)
	{
		if (!Unknown.Contains(P.Event))
		{
			Unknown.Add(P.Event);
			UE_LOG(LogWildreach, Warning, TEXT("SOUND unknown event %s (silent; add it to Data/Audio/sounds.json)"), *P.Event.ToString());
		}
		return nullptr;
	}
	if (E->Files.Num() == 0)
	{
		return nullptr;
	}
	const double Now = FPlatformTime::Seconds();
	const AActor* Follow = P.Follow.Get();
	const bool b2D = P.b2D || E->Radius <= 0.f;
	const uint32 Key = GapKey(P.Event, Follow, P.Location, !b2D, P.GapKeyExtra);
	const double* Last = LastPlayed.Find(Key);
	TArray<TWeakObjectPtr<UAudioComponent>>& Playing = Voices.FindOrAdd(P.Event);
	Playing.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& C) { return !C.IsValid() || !C->IsPlaying(); });
	if ((Last && Now - *Last < E->MinGap) || Playing.Num() >= E->MaxVoices)
	{
		Held.FindOrAdd(P.Event)++;
		if (SoundLogLevel() >= 2)
		{
			Log(FString::Printf(TEXT("%s held (%s)"), *P.Event.ToString(), Playing.Num() >= E->MaxVoices ? TEXT("voices") : TEXT("gap")));
		}
		return nullptr;
	}
	LastPlayed.Add(Key, Now);
	if (Now > NextPrune)
	{
		NextPrune = Now + 30.0;
		for (auto It = LastPlayed.CreateIterator(); It; ++It) if (Now - It.Value() > 20.0) It.RemoveCurrent();
	}

	// A variant, not the same one twice running.
	int32 Pick = FMath::RandHelper(E->Files.Num());
	if (E->Files.Num() > 1 && Pick == E->LastVariant) Pick = (Pick + 1 + FMath::RandHelper(E->Files.Num() - 1)) % E->Files.Num();
	E->LastVariant = Pick;
	const FString& File = E->Files[Pick];
	UWRPcmSound* Sound = LoadFile(File, false, false);
	if (!Sound)
	{
		return nullptr;
	}
	const float Cat = WRSound::CategoryGain(E->Category);
	const float Volume = E->Volume * P.VolumeScale * Cat;
	const float Pitch = FMath::Clamp(FMath::FRandRange(E->PitchMin, E->PitchMax) * P.PitchScale, 0.3f, 2.5f);
	if (Volume <= 0.001f)
	{
		return nullptr;   // muted in the settings
	}
	UAudioComponent* Comp = nullptr;
	FVector Where = P.Location;
	if (b2D)
	{
		Comp = UGameplayStatics::SpawnSound2D(World, Sound, Volume, Pitch);
	}
	else if (Follow && Follow->GetRootComponent())
	{
		Where = Follow->GetActorLocation();
		Comp = UGameplayStatics::SpawnSoundAttached(Sound, Follow->GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, false, Volume, Pitch, 0.f, AttenuationFor(E->Radius));
	}
	else
	{
		Comp = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Where, FRotator::ZeroRotator, Volume, Pitch, 0.f, AttenuationFor(E->Radius));
	}
	if (SoundLogLevel() >= 1)
	{
		FString Place = TEXT("2D");
		if (!b2D)
		{
			FVector Ear = Where;
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				FRotator R;
				PC->GetPlayerViewPoint(Ear, R);
			}
			Place = FString::Printf(TEXT("at %.0f,%.0f,%.0f r%.0fm %.0fm away%s"), Where.X, Where.Y, Where.Z, E->Radius / 100.f, FVector::Dist(Ear, Where) / 100.f,
				Comp ? TEXT("") : TEXT(" (out of earshot)"));
		}
		Log(FString::Printf(TEXT("%s file=%s vol=%.2f (table %.2f x %.2f x %s %.2f) pitch=%.2f %s"), *P.Event.ToString(), *File, Volume, E->Volume, P.VolumeScale,
			*E->Category.ToString(), Cat, Pitch, *Place));
	}
	if (!Comp)
	{
		return nullptr;
	}
	Played.FindOrAdd(P.Event)++;
	Playing.Add(Comp);
	if (E->Duck > 0.f)
	{
		if (UWRAudioDirector* D = UWRAudioDirector::Get(World))
		{
			D->Duck(E->Duck, Sound->GetSeconds() + 0.5f);
		}
	}
	return Comp;
}

UAudioComponent* UWRAudioSubsystem::CreateStream(UWorld* World, const FString& File, bool bLoop)
{
	if (!CanPlayIn(World) || !EnsureLoaded())
	{
		return nullptr;
	}
	UWRPcmSound* Sound = LoadFile(File, true, bLoop);
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Comp = UGameplayStatics::CreateSound2D(World, Sound, 0.001f, 1.f, 0.f, nullptr, false, false);
	if (Comp)
	{
		Comp->bIsUISound = false;
		Comp->Play();
	}
	if (SoundLogLevel() >= 1)
	{
		Log(FString::Printf(TEXT("stream %s%s %s"), *File, bLoop ? TEXT(" (loop)") : TEXT(""), Comp ? TEXT("started") : TEXT("failed")));
	}
	return Comp;
}

// ---------------------------------------------------------------- the calls

namespace WRSoundDetail
{
	static void PlayEvent(const UObject* Context, FName Event, const FVector& Location, AActor* Follow, bool b2D, float VolumeScale, float PitchScale = 1.f, FName Extra = NAME_None)
	{
		UWorld* World = WorldOf(Follow ? static_cast<const UObject*>(Follow) : Context);
		if (!CanPlayIn(World) || Event.IsNone())
		{
			return;
		}
		UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
		if (!Audio)
		{
			return;
		}
		UWRAudioSubsystem::FPlay P;
		P.Event = Event;
		P.World = World;
		P.Location = Follow ? Follow->GetActorLocation() : Location;
		P.Follow = Follow;
		P.b2D = b2D;
		P.VolumeScale = VolumeScale;
		P.PitchScale = PitchScale;
		P.GapKeyExtra = Extra;
		Audio->Play(P);
	}
}

void WRSound::PlayAt(const UObject* WorldContext, FName Event, const FVector& Location, float VolumeScale)
{
	PlayEvent(WorldContext, Event, Location, nullptr, false, VolumeScale);
}

void WRSound::PlayOn(AActor* Actor, FName Event, float VolumeScale)
{
	if (Actor)
	{
		PlayEvent(Actor, Event, Actor->GetActorLocation(), Actor, false, VolumeScale);
	}
}

void WRSound::PlayUI(const UObject* WorldContext, FName Event)
{
	PlayEvent(WorldContext, Event, FVector::ZeroVector, nullptr, true, 1.f);
}

bool WRSound::Has(FName Event)
{
	UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(nullptr);
	return Audio && Audio->EnsureLoaded() && Audio->FindEvent(Event) != nullptr;
}

void WRSound::Broadcast(AActor* Actor, FName Event)
{
	if (!Actor)
	{
		return;
	}
	if (Actor->GetWorld() && Actor->GetWorld()->GetTimeSeconds() - Actor->CreationTime < 1.5f)
	{
		return;   // a body's first moments are quiet (the starting kit going on at spawn)
	}
	if (AWRCharacterBase* Character = Cast<AWRCharacterBase>(Actor); Character && Actor->HasAuthority())
	{
		Character->MulticastSound(Event);   // runs here too (a listen server's own player hears it)
		return;
	}
	PlayOn(Actor, Event);
}

void WRSound::Voice(AActor* Actor, FName Kind, float VolumeScale)
{
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	if (!CanPlayIn(World))
	{
		return;
	}
	UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
	if (!Audio || !Audio->EnsureLoaded())
	{
		return;
	}
	const FString K = Kind.ToString();
	if (const AWRCreature* C = Cast<AWRCreature>(Actor))
	{
		FName Size, Family;
		float SizeCm = 50.f;
		CreatureClass(C, Size, Family, SizeCm);
		float Pitch = 1.f;
		const bool bListed = Audio->CreatureVoice(C->CreatureId, Family, Pitch);
		if (Family.IsNone())
		{
			return;
		}
		const FName Own(*FString::Printf(TEXT("creature.%s.%s"), *C->CreatureId.ToString(), *K));
		if (Audio->FindEvent(Own))
		{
			PlayEvent(Actor, Own, Actor->GetActorLocation(), Actor, false, VolumeScale);
			return;
		}
		// Shared calls: smaller bodies sound higher, bigger ones lower (unless the table gives this one its own pitch).
		const float SizePitch = bListed ? 1.f : FMath::Clamp(FMath::Pow(50.f / FMath::Max(SizeCm, 10.f), 0.3f), 0.75f, 1.35f);
		for (const FString& Name : { FString::Printf(TEXT("creature.%s.%s.%s"), *Size.ToString(), *Family.ToString(), *K),
			FString::Printf(TEXT("creature.%s.%s"), *Family.ToString(), *K), FString::Printf(TEXT("creature.%s.%s"), *Size.ToString(), *K) })
		{
			if (Audio->FindEvent(FName(*Name)))
			{
				PlayEvent(Actor, FName(*Name), Actor->GetActorLocation(), Actor, false, VolumeScale, Pitch * SizePitch);
				return;
			}
		}
		return;
	}
	// Survivors: the female body has its own voice where the table has one.
	if (const AWRCharacter* P = Cast<AWRCharacter>(Actor))
	{
		if (!P->GetBodyStyle().IsNone())
		{
			const FName Female(*(K + TEXT(".female")));
			if (P->GetBodyStyle().ToString().StartsWith(TEXT("f")) && Audio->FindEvent(Female))
			{
				PlayEvent(Actor, Female, Actor->GetActorLocation(), Actor, false, VolumeScale);
				return;
			}
		}
	}
	if (Audio->FindEvent(Kind))
	{
		PlayEvent(Actor, Kind, Actor->GetActorLocation(), Actor, false, VolumeScale);
	}
}

FName WRSound::SurfaceUnder(const AActor* Actor)
{
	const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	if (!World)
	{
		return NAME_None;
	}
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const float Half = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector Feet = Actor->GetActorLocation() - FVector(0.f, 0.f, Half);
	// Wading: the sea (tuning audio.seaLevel) or a water volume.
	if (Feet.Z < WRTune(TEXT("audio.seaLevel"), -20.f) + 25.f)
	{
		return TEXT("water");
	}
	if (Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->GetPhysicsVolume() && Character->GetCharacterMovement()->GetPhysicsVolume()->bWaterVolume)
	{
		return TEXT("water");
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WRFootstep), false, Actor);
	Query.bReturnPhysicalMaterial = true;
	if (!World->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 40.f), Feet - FVector(0.f, 0.f, 120.f), ECC_Visibility, Query))
	{
		return NAME_None;   // in the air
	}
	const AActor* Ground = Hit.GetActor();
	if (const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Ground))
	{
		const FWRPieceDef* Def = Piece->GetDef();
		return Def && Def->Material == TEXT("stone") ? FName(TEXT("stone")) : FName(TEXT("wood"));
	}
	if (Hit.PhysMaterial.IsValid() && Hit.PhysMaterial->GetFName() != TEXT("DefaultPhysicalMaterial"))
	{
		// A pack mesh with its own physical material ("PM_Wood", "PhysMat_Snow"...).
		const FString M = Hit.PhysMaterial->GetName();
		static const TPair<const TCHAR*, const TCHAR*> Keys[] = { { TEXT("Wood"), TEXT("wood") }, { TEXT("Snow"), TEXT("snow") }, { TEXT("Sand"), TEXT("sand") },
			{ TEXT("Grass"), TEXT("grass") }, { TEXT("Gravel"), TEXT("gravel") }, { TEXT("Water"), TEXT("water") }, { TEXT("Stone"), TEXT("stone") }, { TEXT("Rock"), TEXT("stone") } };
		for (const TPair<const TCHAR*, const TCHAR*>& K : Keys)
		{
			if (M.Contains(K.Key)) return FName(K.Value);
		}
	}
	if (Ground && Ground->IsA<ALandscapeProxy>())
	{
		// The island's auto material (Tools/terrain/build_terrain_materials.py): rock on steep slopes, sand at the
		// shore (under 4.5 m), snow on the tundra side (east of the ridge, x > 0), ash and stones on the volcanic side.
		if (FWRBiomeHazards::KindAt(Feet) == TEXT("ice")) return TEXT("stone");
		if (Hit.ImpactNormal.Z < WRTune(TEXT("audio.rockSlope"), 0.86f)) return TEXT("stone");
		if (Hit.ImpactPoint.Z < WRTune(TEXT("audio.shoreHeight"), 300.f)) return TEXT("sand");
		return FMath::Clamp(Hit.ImpactPoint.X / 8000.f + 0.5f, 0.f, 1.f) > 0.5f ? FName(TEXT("snow")) : FName(TEXT("gravel"));
	}
	// Decoration rocks and cliffs, caves, boulders, anything else solid.
	return TEXT("stone");
}

void WRSound::Footstep(AActor* Actor, float VolumeScale)
{
	const FName Surface = SurfaceUnder(Actor);
	if (Surface.IsNone())
	{
		return;
	}
	float Pitch = 1.f;
	FName Event(*(TEXT("footstep.") + Surface.ToString()));
	if (const AWRCreature* C = Cast<AWRCreature>(Actor))
	{
		// Bigger feet: louder, lower, and the heavy set where the table has one.
		FName Size, Family;
		float SizeCm = 50.f;
		CreatureClass(C, Size, Family, SizeCm);
		VolumeScale *= FMath::Clamp(FMath::Sqrt(SizeCm / 45.f), 0.4f, 1.8f);
		Pitch = FMath::Clamp(FMath::Pow(45.f / FMath::Max(SizeCm, 10.f), 0.35f), 0.6f, 1.4f);
		const FName Heavy(*(Event.ToString() + TEXT(".heavy")));
		if (SizeCm >= 70.f && Has(Heavy))
		{
			Event = Heavy;
			Pitch = FMath::Clamp(Pitch * 1.2f, 0.7f, 1.1f);
		}
	}
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const float Half = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	// Per actor: two feet of one body don't hold each other back as much as a stampede would.
	PlayEvent(Actor, Event, Actor->GetActorLocation() - FVector(0.f, 0.f, Half), Actor, false, VolumeScale, Pitch);
}

// ---------------------------------------------------------------- volumes

float WRSound::GetVolume(FName Category)
{
	TMap<FName, float>& V = Volumes();
	if (const float* Found = V.Find(Category))
	{
		return *Found;
	}
	float Value = DefaultVolume(Category);
	if (GConfig)
	{
		GConfig->GetFloat(VolumeSection, *Category.ToString(), Value, GGameUserSettingsIni);
	}
	Value = FMath::Clamp(Value, 0.f, 1.f);
	V.Add(Category, Value);
	return Value;
}

void WRSound::SetVolume(FName Category, float Value, bool bSave)
{
	Value = FMath::Clamp(Value, 0.f, 1.f);
	Volumes().Add(Category, Value);
	if (bSave && SoundLogLevel() > 0)
	{
		UE_LOG(LogWildreach, Log, TEXT("SOUND volume %s %.2f"), *Category.ToString(), Value);
	}
	if (GConfig && bSave)
	{
		GConfig->SetFloat(VolumeSection, *Category.ToString(), Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void WRSound::ResetVolumes()
{
	for (const TCHAR* C : { TEXT("master"), TEXT("music"), TEXT("effects"), TEXT("ambient"), TEXT("ui") })
	{
		SetVolume(C, DefaultVolume(C));
	}
}

float WRSound::CategoryGain(FName Category)
{
	const float* Trim = MixTrim().Find(Category);
	return GetVolume(TEXT("master")) * GetVolume(Category) * (Trim ? *Trim : 1.f);
}

// ---------------------------------------------------------------- dev commands

#if !UE_BUILD_SHIPPING
namespace WRSoundDetail
{
	static FAutoConsoleCommandWithWorldAndArgs GPlayCommand(TEXT("WRSoundPlay"),
		TEXT("Testing: play a sound event at the local pawn (WRSoundPlay <event> [2d] [times]: several at once show the minimum gap)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0 || !World) return;
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			const int32 Times = Args.Num() > 2 ? FMath::Clamp(FCString::Atoi(*Args[2]), 1, 50) : 1;
			for (int32 i = 0; i < Times; ++i)
			{
				if (Args.Num() > 1 && Args[1] == TEXT("2d")) WRSound::PlayUI(World, FName(*Args[0]));
				else if (Pawn) WRSound::PlayOn(Pawn, FName(*Args[0]));
				else WRSound::PlayUI(World, FName(*Args[0]));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStatsCommand(TEXT("WRSoundStats"),
		TEXT("Testing: log how often each sound event played / was held back (WRSoundStats [reset])."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UWRAudioSubsystem* Audio = UWRAudioSubsystem::Get(World);
			if (!Audio) return;
			UE_LOG(LogWildreach, Log, TEXT("SOUND stats (played/held): %s"), *Audio->Stats());
			if (UWRAudioDirector* D = UWRAudioDirector::Get(World)) UE_LOG(LogWildreach, Log, TEXT("SOUND state: %s"), *D->Describe());
			if (Args.Num() > 0 && Args[0] == TEXT("reset")) Audio->ResetStats();
		}));

	static FAutoConsoleCommandWithWorldAndArgs GMusicCommand(TEXT("WRMusic"),
		TEXT("Testing: force the music (WRMusic menu|explore|combat|boss|auto)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWRAudioDirector* D = UWRAudioDirector::Get(World))
			{
				D->ForceMusic(Args.Num() == 0 || Args[0] == TEXT("auto") ? FName() : FName(*Args[0]));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStepCommand(TEXT("WRSoundFootstep"),
		TEXT("Testing: a footstep for the local pawn (logs the surface it picked)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
			{
				UE_LOG(LogWildreach, Log, TEXT("SOUND footstep surface %s at %s"), *WRSound::SurfaceUnder(Pawn).ToString(), *Pawn->GetActorLocation().ToCompactString());
				WRSound::Footstep(Pawn);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GRecordCommand(TEXT("WRSoundRecord"),
		TEXT("Testing: record the mixed game output (WRSoundRecord start [seconds] | stop <name>: Saved/BouncedWavFiles/<name>.wav)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() == 0) return;
			if (Args[0] == TEXT("start"))
			{
				UAudioMixerBlueprintLibrary::StartRecordingOutput(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 600.f);
				UE_LOG(LogWildreach, Log, TEXT("SOUND record start"));
			}
			else if (Args[0] == TEXT("stop"))
			{
				UAudioMixerBlueprintLibrary::StopRecordingOutput(World, EAudioRecordingExportType::WavFile, Args.Num() > 1 ? Args[1] : FString(TEXT("wrsound")), FString());
				UE_LOG(LogWildreach, Log, TEXT("SOUND record stop"));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GVolumeCommand(TEXT("WRVolume"),
		TEXT("Testing: set a volume setting (WRVolume <master|music|effects|ambient|ui> <0..1>)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() >= 2) WRSound::SetVolume(FName(*Args[0]), FCString::Atof(*Args[1]));
		}));
}
#endif
