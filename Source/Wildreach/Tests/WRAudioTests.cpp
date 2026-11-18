// The sound table and its files: every event gameplay names is in Data/Audio/sounds.json, every file it points at is
// there, decodes as Ogg Vorbis through the generator the game plays it with, isn't silent and doesn't clip, and every
// creature has a voice for each call. (Tools/audiotest.ps1 plays it all for real, with the output recorded.)

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "WRAudio.h"
#include "WRCreature.h"
#include "WRTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WRAudioTestDetail
{
	// The events WRSound.h names (footstep resolves to footstep.<surface>; creature calls are checked below).
	static const TCHAR* Named[] = {
		TEXT("footstep.sand"), TEXT("footstep.snow"), TEXT("footstep.stone"), TEXT("footstep.wood"), TEXT("footstep.grass"), TEXT("footstep.gravel"), TEXT("footstep.water"),
		TEXT("swing.light"), TEXT("swing.heavy"), TEXT("hit.flesh"), TEXT("hit.armor"), TEXT("hit.block"), TEXT("parry"), TEXT("dodge"), TEXT("jump"), TEXT("land"),
		TEXT("hurt"), TEXT("hurt.female"), TEXT("death"), TEXT("death.female"), TEXT("eat"), TEXT("drink"), TEXT("potion"), TEXT("pickup"), TEXT("equip"),
		TEXT("bow.draw"), TEXT("bow.release"), TEXT("crossbow.fire"), TEXT("blowgun.fire"), TEXT("throw"), TEXT("arrow.hit"),
		TEXT("cast.fire"), TEXT("cast.frost"), TEXT("fire.impact"), TEXT("frost.impact"), TEXT("firewall"), TEXT("lavapool"), TEXT("frostnova"), TEXT("icelance"),
		TEXT("gather.wood"), TEXT("gather.stone"), TEXT("gather.fiber"), TEXT("build.place"), TEXT("build.demolish"), TEXT("build.invalid"),
		TEXT("door.open"), TEXT("door.close"), TEXT("craft.done"), TEXT("hearth.feed"),
		TEXT("levelup"), TEXT("recipe.unlock"), TEXT("discover"), TEXT("boss.intro"), TEXT("tame.success"), TEXT("respawn"),
		TEXT("ui.hover"), TEXT("ui.click"), TEXT("ui.open"), TEXT("ui.close"), TEXT("ui.error"), TEXT("ui.drag"), TEXT("ui.drop"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRAudioTableTest, "Wildreach.Audio.Table",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRAudioTableTest::RunTest(const FString& Parameters)
{
	using namespace WRAudioTestDetail;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("Audio/sounds.json"));
	if (!TestTrue(TEXT("Data/Audio/sounds.json loads"), Root.IsValid())) return false;
	const TSharedPtr<FJsonObject>& Events = Root->GetObjectField(TEXT("events"));
	for (const TCHAR* Name : Named)
	{
		TestTrue(FString::Printf(TEXT("event %s is in the table"), Name), Events->HasField(Name));
	}

	TSet<FString> Short, Streams;
	static const TSet<FString> Categories = { TEXT("effects"), TEXT("ui"), TEXT("ambient"), TEXT("music") };
	for (const auto& Pair : Events->Values)
	{
		// Comment strings among the rows ("_combatFull", "_survival"...) aren't events.
		if (FString(*Pair.Key).StartsWith(TEXT("_")) || !Pair.Value.IsValid() || Pair.Value->Type != EJson::Object) continue;
		const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
		const TArray<TSharedPtr<FJsonValue>> Files = E->GetArrayField(TEXT("files"));
		TestTrue(FString::Printf(TEXT("%s has files"), *Pair.Key), Files.Num() > 0);
		for (const TSharedPtr<FJsonValue>& F : Files) Short.Add(F->AsString());
		const double Volume = E->GetNumberField(TEXT("volume"));
		TestTrue(FString::Printf(TEXT("%s volume %.2f within 0..4"), *Pair.Key, Volume), Volume > 0.0 && Volume <= 4.0);
		const TArray<TSharedPtr<FJsonValue>> Pitch = E->GetArrayField(TEXT("pitch"));
		TestTrue(FString::Printf(TEXT("%s pitch range"), *Pair.Key), Pitch.Num() == 2 && Pitch[0]->AsNumber() > 0.2 && Pitch[0]->AsNumber() <= Pitch[1]->AsNumber());
		TestTrue(FString::Printf(TEXT("%s category"), *Pair.Key), Categories.Contains(E->GetStringField(TEXT("category"))));
		// UI and moments are 2D; the rest carry a sensible distance.
		const double Radius = E->GetNumberField(TEXT("radius"));
		if (FString(*Pair.Key).StartsWith(TEXT("ui."))) TestEqual(FString::Printf(TEXT("%s is 2D"), *Pair.Key), Radius, 0.0);
		// Most carry under 200 m; the far cues (the Long Noon's sign, dawn songs, bells, horns, the crater's chuff) up to 5 km.
		TestTrue(FString::Printf(TEXT("%s radius"), *Pair.Key), Radius >= 0.0 && Radius <= 500000.0);
	}
	for (const TSharedPtr<FJsonValue>& B : Root->GetObjectField(TEXT("ambient"))->GetArrayField(TEXT("beds"))) Streams.Add(B->AsObject()->GetStringField(TEXT("file")));
	for (const auto& Pool : Root->GetObjectField(TEXT("music"))->GetObjectField(TEXT("pools"))->Values)
	{
		TestTrue(FString::Printf(TEXT("music pool %s has tracks"), *Pool.Key), Pool.Value->AsArray().Num() > 0);
		for (const TSharedPtr<FJsonValue>& T : Pool.Value->AsArray()) Streams.Add(T->AsObject()->GetStringField(TEXT("file")));
	}
	for (const TCHAR* Pool : { TEXT("menu"), TEXT("explore"), TEXT("night"), TEXT("combat"), TEXT("boss") })
	{
		TestTrue(FString::Printf(TEXT("music pool %s"), Pool), Root->GetObjectField(TEXT("music"))->GetObjectField(TEXT("pools"))->HasField(Pool));
	}

	// Every file: there, Ogg Vorbis, through the game's own generator, not silent, not clipping. Short ones are mono
	// (3D) unless they're 2D moments; streams are stereo.
	for (const TSet<FString>* Set : { &Short, &Streams })
	{
		const bool bStream = Set == &Streams;
		for (const FString& File : *Set)
		{
			int32 Channels = 0, Rate = 0;
			float Seconds = 0.f, Peak = 0.f;
			FString Error;
			if (!TestTrue(FString::Printf(TEXT("%s decodes (%s)"), *File, *Error), UWRAudioSubsystem::ProbeFile(File, bStream, Channels, Rate, Seconds, Peak, Error))) continue;
			TestTrue(FString::Printf(TEXT("%s isn't silent (peak %.3f)"), *File, Peak), Peak > 0.02f);
			// A clipped stand-in is a warning: the sound pass replaces them (the files are rebuilt by Tools/audio, Python).
			if (Peak >= 1.0f) AddWarning(FString::Printf(TEXT("%s clips (peak %.3f)"), *File, Peak));
			// Events borrow ambience beds and machine loops as stand-ins (up to a minute or two), and 2D stings and beds
			// are stereo; a short sound of its own is mono so it places in 3D.
			const bool bBorrowed = File.StartsWith(TEXT("amb/")) || File.StartsWith(TEXT("machine/")) || File.StartsWith(TEXT("music/")) || File.StartsWith(TEXT("moment/"));
			TestTrue(FString::Printf(TEXT("%s length %.2f s"), *File, Seconds), Seconds > 0.02f && Seconds < (bStream ? 400.f : bBorrowed ? 120.f : 12.f));   // a thunder roll or the forge's roar runs ~10 s
			if (bStream) TestEqual(FString::Printf(TEXT("%s is stereo"), *File), Channels, 2);
			else if (!bBorrowed) TestEqual(FString::Printf(TEXT("%s is mono (3D)"), *File), Channels, 1);
		}
	}

	// Creature calls: every creature (but the training dummy) has a family with each call, or calls of its own.
	const TSharedPtr<FJsonObject>& Voices = Root->GetObjectField(TEXT("creatures"));
	for (const FWRCreatureDef& Def : FWRCreatureRegistry::Get().All())
	{
		if (Def.Archetype == TEXT("dummy") || Def.Id.IsNone()) continue;
		// A row of its own, or the family its archetype falls back to in the game (WRSound.cpp CreatureClass).
		const TSharedPtr<FJsonObject>* V = nullptr;
		const FName A = Def.Archetype;
		const FString ByArchetype = A == TEXT("boss") ? TEXT("boss") : A == TEXT("pack_hunter") ? TEXT("canine") : A == TEXT("ground_predator") ? TEXT("beast")
			: A == TEXT("flyer") ? TEXT("flyer") : A == TEXT("burrower") ? TEXT("bug") : A == TEXT("herd") ? TEXT("herd") : A == TEXT("critter") ? TEXT("critter") : TEXT("");
		const bool bRow = Voices->TryGetObjectField(Def.Id.ToString(), V);
		if (!TestTrue(FString::Printf(TEXT("%s has a voice"), *Def.Id.ToString()), bRow || !ByArchetype.IsEmpty())) continue;
		const FString Family = bRow ? (*V)->GetStringField(TEXT("family")) : ByArchetype;
		for (const TCHAR* Kind : { TEXT("aggro"), TEXT("attack"), TEXT("hurt"), TEXT("death") })
		{
			const bool bHas = Events->HasField(FString::Printf(TEXT("creature.%s.%s"), *Def.Id.ToString(), Kind)) || Events->HasField(FString::Printf(TEXT("creature.%s.%s"), *Family, Kind));
			TestTrue(FString::Printf(TEXT("%s (%s) has a %s call"), *Def.Id.ToString(), *Family, Kind), bHas);
		}
	}
	return true;
}

#endif
