// The default keys: no two actions start on the same key (binding one key twice silently unbinds the other, so a
// fresh player would find an action dead), and build mode's Demolish has its own key from the start (Raven couldn't
// find a way into demolish mode in 0.4.1). Tools/demolishtest.ps1 plays demolish mode with the mouse and keyboard.

#include "Misc/AutomationTest.h"
#include "WRKeyBindings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWRUIDefaultKeysTest, "Wildreach.UI.DefaultKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWRUIDefaultKeysTest::RunTest(const FString& Parameters)
{
	TMap<FKey, FName> Seen;
	for (const FWRActionKey& A : FWRKeyBindings::Get().Actions())
	{
		if (A.Group == TEXT("Abilities") || !A.Default.IsValid()) continue;   // abilities start unbound
		if (const FName* Other = Seen.Find(A.Default))
		{
			AddError(FString::Printf(TEXT("%s and %s both default to %s"), *Other->ToString(), *A.Id.ToString(), *A.Default.ToString()));
			continue;
		}
		Seen.Add(A.Default, A.Id);
	}
	const FWRActionKey* Demolish = FWRKeyBindings::Get().Find(TEXT("Demolish"));
	if (TestNotNull(TEXT("build mode has a Demolish key"), Demolish))
	{
		TestEqual(TEXT("Demolish defaults to X"), Demolish->Default, EKeys::X);
		TestEqual(TEXT("it's listed with the menu keys (Settings > Controls, H)"), Demolish->Group, FName(TEXT("Menus")));
	}
	return true;
}

#endif
