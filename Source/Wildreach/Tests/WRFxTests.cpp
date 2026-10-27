// Effects data and the hit flags: every effect name gameplay calls has a recipe in Data/fx.json, every recipe is
// well-formed (a real sprite, a life, something that spawns, colours with four channels), and a hit's flags carry what
// kind of blow it was. The drawing itself is checked by Tools/fxshots.ps1 (it needs a rendered client).

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRTuning.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRFxRecipesTest, "Wildreach.Fx.Recipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRFxRecipesTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("fx.json"));
	const TSharedPtr<FJsonObject>* Effects = nullptr;
	if (!TestTrue(TEXT("fx.json loads with effects"), Root.IsValid() && Root->TryGetObjectField(TEXT("effects"), Effects)))
	{
		return false;
	}
	// The names WRFx.h promises (and the code calls).
	static const TCHAR* Required[] = {
		TEXT("hit.spark"), TEXT("hit.blood"), TEXT("hit.dust"), TEXT("hit.ice"), TEXT("hit.magic"), TEXT("hit.crit"), TEXT("parry.flash"),
		TEXT("dodge.dust"), TEXT("land.dust"), TEXT("death.poof"), TEXT("stomp"),
		TEXT("fire.bolt"), TEXT("frost.shard"), TEXT("fire.impact"), TEXT("frost.impact"), TEXT("firewall"), TEXT("lavapool"), TEXT("frostnova"),
		TEXT("icelance"), TEXT("blizzard"), TEXT("cast.fire"), TEXT("cast.frost"), TEXT("status.burn"), TEXT("status.frozen"),
		TEXT("gather.wood"), TEXT("gather.stone"), TEXT("gather.fiber"), TEXT("build.place"), TEXT("build.break"), TEXT("build.break.wood"),
		TEXT("hearth.fire"), TEXT("lava.embers"), TEXT("torch.fire"), TEXT("brazier.fire"), TEXT("snow"), TEXT("ash"),
		TEXT("levelup"), TEXT("tame.success"), TEXT("discover") };
	for (const TCHAR* Name : Required)
	{
		TestTrue(FString::Printf(TEXT("fx.json has %s"), Name), (*Effects)->HasField(Name));
	}
	static const TSet<FString> Sprites = { TEXT("dot"), TEXT("spark"), TEXT("smoke"), TEXT("smoke2"), TEXT("flame"), TEXT("flame2"), TEXT("flare"), TEXT("ring"),
		TEXT("chip_wood"), TEXT("chip_stone"), TEXT("leaf"), TEXT("shard"), TEXT("flake"), TEXT("droplet"), TEXT("glow"), TEXT("wisp") };
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Effects)->Values)
	{
		const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
		if (!TestTrue(FString::Printf(TEXT("%s has layers"), *Pair.Key), E.IsValid() && E->TryGetArrayField(TEXT("layers"), Layers) && Layers->Num() > 0))
		{
			continue;
		}
		bool bLoop = false;
		E->TryGetBoolField(TEXT("loop"), bLoop);
		for (int32 i = 0; i < Layers->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> L = (*Layers)[i]->AsObject();
			const FString Where = FString::Printf(TEXT("%s layer %d"), *Pair.Key, i);
			FString Sprite, Blend;
			TestTrue(Where + TEXT(" uses an atlas sprite"), L->TryGetStringField(TEXT("sprite"), Sprite) && Sprites.Contains(Sprite));
			TestTrue(Where + TEXT(" blends add or lit"), !L->TryGetStringField(TEXT("blend"), Blend) || Blend == TEXT("add") || Blend == TEXT("lit"));
			double Count = 0.0, Rate = 0.0;
			L->TryGetNumberField(TEXT("count"), Count);
			L->TryGetNumberField(TEXT("rate"), Rate);
			TestTrue(Where + TEXT(" spawns something"), Count > 0.0 || (bLoop && Rate > 0.0));
			const TArray<TSharedPtr<FJsonValue>>* Life = nullptr;
			TestTrue(Where + TEXT(" has a life"), L->TryGetArrayField(TEXT("life"), Life) && Life->Num() == 2 && (*Life)[0]->AsNumber() > 0.0 && (*Life)[1]->AsNumber() >= (*Life)[0]->AsNumber());
			const TArray<TSharedPtr<FJsonValue>>* Colors = nullptr;
			if (TestTrue(Where + TEXT(" has colours"), L->TryGetArrayField(TEXT("colors"), Colors) && Colors->Num() > 0))
			{
				for (const TSharedPtr<FJsonValue>& C : *Colors)
				{
					const TArray<TSharedPtr<FJsonValue>>* Rgba = nullptr;
					TestTrue(Where + TEXT(" colours are RGBA"), C->TryGetArray(Rgba) && Rgba->Num() == 4);
				}
			}
		}
	}
	// Impacts name real effects.
	const TSharedPtr<FJsonObject>* Impacts = nullptr;
	if (TestTrue(TEXT("fx.json has impacts"), Root->TryGetObjectField(TEXT("impacts"), Impacts)))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Impacts)->Values)
		{
			if (!Pair.Key.StartsWith(TEXT("_"))) TestTrue(FString::Printf(TEXT("impact for %s exists"), *Pair.Key), (*Effects)->HasField(Pair.Value->AsString()));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRFxHitFlagsTest, "Wildreach.Fx.HitFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRFxHitFlagsTest::RunTest(const FString& Parameters)
{
	FWRHit Hit;
	Hit.bMelee = true;
	Hit.bHeavy = true;
	Hit.Element = WRTags::Element_Fire;
	const uint16 F = WRFx::HitFlagsFor(Hit, true, false, true, false, true);
	TestTrue(TEXT("heavy"), (F & WRFx::Hit_Heavy) != 0);
	TestTrue(TEXT("crit"), (F & WRFx::Hit_Crit) != 0);
	TestTrue(TEXT("weak"), (F & WRFx::Hit_Weak) != 0);
	TestTrue(TEXT("melee"), (F & WRFx::Hit_Melee) != 0);
	TestTrue(TEXT("fire"), (F & WRFx::Hit_Fire) != 0);
	TestTrue(TEXT("armoured"), (F & WRFx::Hit_Armored) != 0);
	TestFalse(TEXT("not a riposte, shatter, frost or dot"), (F & (WRFx::Hit_Riposte | WRFx::Hit_Shatter | WRFx::Hit_Frost | WRFx::Hit_Dot)) != 0);
	FWRHit Burn;
	Burn.bDamageOverTime = true;
	Burn.Element = WRTags::Element_Frost;
	const uint16 B = WRFx::HitFlagsFor(Burn, false, false, false, false, false);
	TestTrue(TEXT("a burn tick is damage over time"), (B & WRFx::Hit_Dot) != 0 && (B & WRFx::Hit_Frost) != 0 && (B & WRFx::Hit_Heavy) == 0);
	FWRHit Art;
	Art.bWeaponArt = true;
	TestTrue(TEXT("a weapon art counts as heavy"), (WRFx::HitFlagsFor(Art, false, false, false, false, false) & WRFx::Hit_Heavy) != 0);
	return true;
}

#endif
