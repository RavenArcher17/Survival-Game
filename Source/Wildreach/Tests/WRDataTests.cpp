// Data integrity: the cross-file checks that would have caught the 0.8 data bugs found in the 11-18 audit (the Desert's
// settings pasted inside the Ocean's, effects that never emit, monument heads that weren't items, land ids that
// aren't the game's, achievements that could never go live, sound rows in the wrong section, eggs with no item).
// Reads the data files as they ship; no world is spun up.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WRCreature.h"
#include "WRFarm.h"
#include "WRInventory.h"
#include "WRTuning.h"
#include "WRUIStyle.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace WRDataTestDetail
{
	TArray<TSharedPtr<FJsonObject>> Rows(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Root.IsValid() && Root->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr) if (V.IsValid() && V->Type == EJson::Object) Out.Add(V->AsObject());
		}
		return Out;
	}
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(Field, S) ? S : FString();
	}
	/** The keys of an object field (loot, costs, inputs...), comments left out. */
	TArray<FString> Keys(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FString> Out;
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Field, Obj))
		{
			for (const auto& KV : (*Obj)->Values) { const FString K(*KV.Key); if (!K.StartsWith(TEXT("_"))) Out.Add(K); }
		}
		return Out;
	}
	bool IsItem(const FString& Id) { return FWRItemRegistry::Get().Find(FName(*Id)) != nullptr; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRDataItemsTest, "Wildreach.Data.ItemIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRDataItemsTest::RunTest(const FString& Parameters)
{
	using namespace WRDataTestDetail;
	// Every id a creature drops, a recipe takes or makes, a pen gives, a piece costs, a turret fires or a quest hands
	// over is an item (an unknown id is dropped silently by AddItem: "Inventory full", a dead-end recipe).
	for (const TSharedPtr<FJsonObject>& C : Rows(FWRTuning::LoadDataFile(TEXT("creatures.json")), TEXT("creatures")))
	{
		const FString Id = Str(C, TEXT("id"));
		for (const FString& K : Keys(C, TEXT("loot"))) TestTrue(FString::Printf(TEXT("%s drops %s, an item"), *Id, *K), IsItem(K));
		for (const FString& K : Keys(C, TEXT("products"))) TestTrue(FString::Printf(TEXT("%s gives %s, an item"), *Id, *K), IsItem(K));
		const FString First = Str(C, TEXT("firstKill"));
		if (!First.IsEmpty()) TestTrue(FString::Printf(TEXT("%s's first-kill trophy %s is an item"), *Id, *First), IsItem(First));
	}
	for (const TSharedPtr<FJsonObject>& R : Rows(FWRTuning::LoadDataFile(TEXT("recipes.json")), TEXT("recipes")))
	{
		const FString Id = Str(R, TEXT("id"));
		for (const FString& K : Keys(R, TEXT("inputs"))) TestTrue(FString::Printf(TEXT("recipe %s takes %s, an item"), *Id, *K), IsItem(K));
		const FString Out = Str(R, TEXT("output"));
		TestTrue(FString::Printf(TEXT("recipe %s makes %s, an item"), *Id, *Out), IsItem(Out));
	}
	for (const TSharedPtr<FJsonObject>& A : Rows(FWRTuning::LoadDataFile(TEXT("farming.json")), TEXT("animals")))
	{
		for (const TSharedPtr<FJsonObject>& P : Rows(A, TEXT("products")))
		{
			TestTrue(FString::Printf(TEXT("a %s pen gives %s, an item"), *Str(A, TEXT("species")), *Str(P, TEXT("item"))), IsItem(Str(P, TEXT("item"))));
		}
	}
	for (const TSharedPtr<FJsonObject>& P : Rows(FWRTuning::LoadDataFile(TEXT("pieces.json")), TEXT("pieces")))
	{
		for (const FString& K : Keys(P, TEXT("cost"))) TestTrue(FString::Printf(TEXT("piece %s costs %s, an item"), *Str(P, TEXT("id")), *K), IsItem(K));
	}
	if (const TSharedPtr<FJsonObject> Machines = FWRTuning::LoadDataFile(TEXT("machines.json")))
	{
		const TSharedPtr<FJsonObject>* Turrets = nullptr;
		if (Machines->TryGetObjectField(TEXT("turrets"), Turrets))
		{
			for (const TSharedPtr<FJsonObject>& Ammo : Rows(*Turrets, TEXT("ammo")))
			{
				TestTrue(FString::Printf(TEXT("turret ammo %s is an item"), *Str(Ammo, TEXT("item"))), IsItem(Str(Ammo, TEXT("item"))));
			}
		}
	}
	if (const TSharedPtr<FJsonObject> World = FWRTuning::LoadDataFile(TEXT("world.json")))
	{
		const TSharedPtr<FJsonObject>* Quests = nullptr;
		if (World->TryGetObjectField(TEXT("quests"), Quests))
		{
			for (const auto& Master : (*Quests)->Values)
			{
				if (!Master.Value.IsValid() || Master.Value->Type != EJson::Array) continue;
				for (const TSharedPtr<FJsonValue>& Q : Master.Value->AsArray())
				{
					if (!Q.IsValid() || Q->Type != EJson::Object) continue;
					for (const FString& K : Keys(Q->AsObject(), TEXT("items"))) TestTrue(FString::Printf(TEXT("quest %s gives %s, an item"), *Str(Q->AsObject(), TEXT("id")), *K), IsItem(K));
					const FString Bring = Str(Q->AsObject(), TEXT("item"));
					if (!Bring.IsEmpty()) TestTrue(FString::Printf(TEXT("quest %s wants %s, an item"), *Str(Q->AsObject(), TEXT("id")), *Bring), IsItem(Bring));
				}
			}
		}
	}
	// Every learnable blueprint is an item.
	if (const TSharedPtr<FJsonObject> Gear = FWRTuning::LoadDataFile(TEXT("gear.json")))
	{
		const TSharedPtr<FJsonObject>* Bps = nullptr;
		if (Gear->TryGetObjectField(TEXT("blueprints"), Bps))
		{
			for (const TSharedPtr<FJsonObject>& B : Rows(*Bps, TEXT("list"))) TestTrue(FString::Printf(TEXT("blueprint %s is an item"), *Str(B, TEXT("id"))), IsItem(Str(B, TEXT("id"))));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRDataLandsTest, "Wildreach.Data.Lands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRDataLandsTest::RunTest(const FString& Parameters)
{
	using namespace WRDataTestDetail;
	const FWRTuning& T = FWRTuning::Get();
	// The lands' own settings sit at the top of tuning.json, where their code reads them (the Desert's were pasted inside
	// the Ocean's and its wildlife never spawned).
	for (const TCHAR* Root : { TEXT("desertWorld"), TEXT("desertLife"), TEXT("desertEvents"), TEXT("oceanWorld"), TEXT("skyLife"), TEXT("crystalLife"), TEXT("fungalLife") })
	{
		TestTrue(FString::Printf(TEXT("tuning.json has %s at the top"), Root), T.Object(Root).IsValid());
	}
	TestTrue(TEXT("the Desert's creature tables are where its spawner reads them"), T.Object(TEXT("desertLife")).IsValid() && T.Object(TEXT("desertLife"))->HasField(TEXT("tables")));

	// Every major boss has its core (the Hearth takes them all) and its head (a monument at home).
	int32 Majors = 0;
	for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All())
	{
		if (!C.Role.Contains(TEXT("major"))) continue;
		++Majors;
		TestTrue(FString::Printf(TEXT("%s has a core"), *C.Id.ToString()), IsItem(FString::Printf(TEXT("core_%s"), *C.Id.ToString())));
		TestTrue(FString::Printf(TEXT("%s has a head for its monument"), *C.Id.ToString()), IsItem(FString::Printf(TEXT("trophy_head_%s"), *C.Id.ToString())));
	}
	TestEqual(TEXT("twelve major bosses"), Majors, 12);

	// Pieces live with a land use the game's land ids (storm_peaks, sky_islands, wasteland: not storm, sky, void).
	for (const TSharedPtr<FJsonObject>& P : Rows(FWRTuning::LoadDataFile(TEXT("pieces.json")), TEXT("pieces")))
	{
		const FString With = Str(P, TEXT("liveWith"));
		if (!With.IsEmpty()) TestTrue(FString::Printf(TEXT("piece %s lives with %s, a live land"), *Str(P, TEXT("id")), *With), WRBiome::IsLive(FName(*With)));
	}
	// Achievements name lands creatures live in, and a belt lists its mini-bosses.
	for (const TSharedPtr<FJsonObject>& A : Rows(FWRTuning::LoadDataFile(TEXT("achievements.json")), TEXT("achievements")))
	{
		const FString Id = Str(A, TEXT("id"));
		const TArray<TSharedPtr<FJsonValue>>* Needs = nullptr;
		if (A->TryGetArrayField(TEXT("needs"), Needs))
		{
			for (const TSharedPtr<FJsonValue>& N : *Needs)
			{
				FString K, V;
				if (!N->AsString().Split(TEXT(":"), &K, &V) || K != TEXT("biome")) continue;
				bool bLives = false;
				for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All()) bLives |= C.Biome == FName(*V);
				TestTrue(FString::Printf(TEXT("achievement %s names %s, a land with creatures"), *Id, *V), bLives);
			}
		}
		if (Str(A, TEXT("event")) == TEXT("belt"))
		{
			const TSharedPtr<FJsonObject>* Params = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Minis = nullptr;
			const bool bList = A->TryGetObjectField(TEXT("params"), Params) && (*Params)->TryGetArrayField(TEXT("minis"), Minis) && Minis->Num() > 0;
			if (TestTrue(FString::Printf(TEXT("belt %s lists its mini-bosses"), *Id), bList))
			{
				for (const TSharedPtr<FJsonValue>& M : *Minis) TestNotNull(FString::Printf(TEXT("belt %s: %s is a creature"), *Id, *M->AsString()), FWRCreatureRegistry::Get().Find(FName(*M->AsString())));
			}
		}
	}
	// Eggs: a species that lays has a fertilised egg item; a nest species has its wild egg item.
	for (const TSharedPtr<FJsonObject>& C : Rows(FWRTuning::LoadDataFile(TEXT("creatures.json")), TEXT("creatures")))
	{
		const FString Id = Str(C, TEXT("id"));
		if (Str(C, TEXT("birth")) == TEXT("egg")) TestTrue(FString::Printf(TEXT("%s lays: fertile_%s is an item"), *Id, *Id), IsItem(TEXT("fertile_") + Id));
		bool bNest = false;
		if (C->TryGetBoolField(TEXT("nest"), bNest) && bNest) TestTrue(FString::Printf(TEXT("%s nests: egg_%s is an item"), *Id, *Id), IsItem(TEXT("egg_") + Id));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRDataFxSoundTest, "Wildreach.Data.FxAndSound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRDataFxSoundTest::RunTest(const FString& Parameters)
{
	using namespace WRDataTestDetail;
	// Effects: every layer's sprite is a cell of the atlas (an unknown name draws as a dot), and a layer that only has a
	// rate emits only in a looping effect (a one-shot with a rate-only layer shows nothing).
	static const TSet<FString> Cells = { TEXT("dot"), TEXT("spark"), TEXT("smoke"), TEXT("smoke2"), TEXT("flame"), TEXT("flame2"), TEXT("flare"), TEXT("ring"),
		TEXT("chip_wood"), TEXT("chip_stone"), TEXT("leaf"), TEXT("shard"), TEXT("flake"), TEXT("droplet"), TEXT("glow"), TEXT("wisp") };
	const TSharedPtr<FJsonObject> Fx = FWRTuning::LoadDataFile(TEXT("fx.json"));
	const TSharedPtr<FJsonObject>* Effects = nullptr;
	if (TestTrue(TEXT("fx.json has its effects"), Fx.IsValid() && Fx->TryGetObjectField(TEXT("effects"), Effects)))
	{
		for (const auto& E : (*Effects)->Values)
		{
			const FString Id(*E.Key);
			if (Id.StartsWith(TEXT("_")) || !E.Value.IsValid() || E.Value->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> Def = E.Value->AsObject();
			bool bLoop = false;
			Def->TryGetBoolField(TEXT("loop"), bLoop);
			for (const TSharedPtr<FJsonObject>& L : Rows(Def, TEXT("layers")))
			{
				const FString Sprite = Str(L, TEXT("sprite"));
				if (!Sprite.IsEmpty()) TestTrue(FString::Printf(TEXT("%s: sprite %s is an atlas cell"), *Id, *Sprite), Cells.Contains(Sprite));
				double Rate = 0.0, Count = 0.0;
				L->TryGetNumberField(TEXT("rate"), Rate);
				L->TryGetNumberField(TEXT("count"), Count);
				bool bFollow = false;
				L->TryGetBoolField(TEXT("follow"), bFollow);
				if (Rate > 0.0 && Count <= 0.0 && !bFollow) TestTrue(FString::Printf(TEXT("%s: a rate-only layer needs \"loop\""), *Id), bLoop);
			}
		}
	}
	// Sounds: every event row is an object with files; the creatures section holds voice rows (family, pitch), not events.
	const TSharedPtr<FJsonObject> Snd = FWRTuning::LoadDataFile(TEXT("Audio/sounds.json"));
	const TSharedPtr<FJsonObject>* Events = nullptr;
	const TSharedPtr<FJsonObject>* Voices = nullptr;
	if (TestTrue(TEXT("sounds.json has events and creatures"), Snd.IsValid() && Snd->TryGetObjectField(TEXT("events"), Events) && Snd->TryGetObjectField(TEXT("creatures"), Voices)))
	{
		for (const auto& E : (*Events)->Values)
		{
			const FString Id(*E.Key);
			if (Id.StartsWith(TEXT("_"))) continue;
			TestTrue(FString::Printf(TEXT("sound %s has files"), *Id), E.Value.IsValid() && E.Value->Type == EJson::Object && E.Value->AsObject()->HasField(TEXT("files")));
		}
		for (const auto& V : (*Voices)->Values)
		{
			const FString Id(*V.Key);
			if (Id.StartsWith(TEXT("_")) || !V.Value.IsValid() || V.Value->Type != EJson::Object) continue;
			TestFalse(FString::Printf(TEXT("%s is a voice row, not an event (it's in the creatures section)"), *Id), V.Value->AsObject()->HasField(TEXT("files")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRDataUIImagesTest, "Wildreach.Data.UIImages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRDataUIImagesTest::RunTest(const FString& Parameters)
{
	// Every picture the UI reads from Data/UI decodes as the game decodes it (BGRA, 8 bits): an unreadable one draws as
	// nothing (the loading screen's island view, an icon, a portrait), with only a warning in the log.
	IImageWrapperModule& Wrappers = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const FString Dir = FPaths::ConvertRelativePathToFull(WRUI::Dir());
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.png"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.jpg"), true, false, false);
	TestTrue(TEXT("Data/UI has the loading screen's picture"), Files.ContainsByPredicate([](const FString& F) { return F.EndsWith(TEXT("/loading.jpg")); }));
	TArray<FString> Bad;
	for (const FString& F : Files)
	{
		TArray<uint8> File;
		const bool bJpg = F.EndsWith(TEXT(".jpg"));
		TSharedPtr<IImageWrapper> W = Wrappers.CreateImageWrapper(bJpg ? EImageFormat::JPEG : EImageFormat::PNG);
		TArray<uint8> Raw;
		FString Why;
		if (!FFileHelper::LoadFileToArray(File, *F)) Why = TEXT("can't be read");
		else if (!W.IsValid()) Why = TEXT("no decoder");
		else if (!W->SetCompressed(File.GetData(), File.Num())) Why = TEXT("not a file the decoder takes");
		else if (!W->GetRaw(ERGBFormat::BGRA, 8, Raw)) Why = FString::Printf(TEXT("doesn't decode (%lldx%lld, %d bits)"), W->GetWidth(), W->GetHeight(), W->GetBitDepth());
		else if (Raw.Num() != W->GetWidth() * W->GetHeight() * 4) Why = TEXT("decodes to the wrong size");
		if (!Why.IsEmpty()) Bad.Add(FPaths::GetCleanFilename(F) + TEXT(": ") + Why);
	}
	AddInfo(FString::Printf(TEXT("%d pictures in Data/UI"), Files.Num()));
	TestEqual(FString::Printf(TEXT("every Data/UI picture decodes (%s)"), *FString::Join(Bad, TEXT("; "))), Bad.Num(), 0);
	return true;
}

#endif
