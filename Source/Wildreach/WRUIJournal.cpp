// The journal (mockups "Inventory", "Crafting", "Paths", "Life skills", "Map"): one stone window with a leather tab
// strip; each tab's content is a factory registered by name with WR_REGISTER_JOURNAL_TAB ("inventory", "crafting",
// "paths", "skills", "map"). The window follows the controller's menu state, so I / J / P / K / M switch tabs.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRKeyBindings.h"
#include "WRPathComponent.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

class SWRJournal : public SCompoundWidget
{
	// A class constant, not an anonymous-namespace one: unity builds can put this file next to WRUIHud.cpp's U.
	static constexpr float U = 1.2f;

public:
	SLATE_BEGIN_ARGS(SWRJournal) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		struct FTab { EWRMenu Menu; FText Name; const TCHAR* Action; };
		const FTab Tabs[] = {
			{ EWRMenu::Inventory, LOCTEXT("TabInventory", "Inventory"), TEXT("Inventory") },
			{ EWRMenu::Craft, LOCTEXT("TabCrafting", "Crafting"), TEXT("Crafting") },
			{ EWRMenu::Paths, LOCTEXT("TabPaths", "Paths"), TEXT("Paths") },
			{ EWRMenu::Skills, LOCTEXT("TabSkills", "Skills"), TEXT("Skills") },
			{ EWRMenu::Map, LOCTEXT("TabMap", "Map"), TEXT("Map") },
		};
		TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
		for (const FTab& T : Tabs)
		{
			const EWRMenu Menu = T.Menu;
			const FName Action(T.Action);
			Strip->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f * U, 0.f)
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this, Menu]() { if (PC.IsValid()) PC->SetMenu(Menu); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(14.f * U, 8.f * U, 14.f * U, 6.f * U)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							WRUI::Text(T.Name, WRUI::Title(15), TAttribute<FSlateColor>::CreateLambda([this, Menu]() { return FSlateColor(IsOn(Menu) ? WRUI::Ink : WRUI::Ink3); }), true)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f * U, 0.f, 0.f, 0.f)
						[
							SNew(SWRKeyCap).FontSize(10).Text_Lambda([Action]() { return WRUI::KeyName(FWRKeyBindings::Get().Key(Action)); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(2.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity_Lambda([this, Menu]() { return IsOn(Menu) ? WRUI::Brass : FLinearColor::Transparent; })]
					]
				]
			];
		}

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(8.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))]]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(1440.f * U).HeightOverride(812.f * U)
				[
					SNew(SWRPanel).Kind(EWRPanel::Stone).Rivets(true).BrassTrim(true).Padding(FMargin(10.f * U))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(60.f * U)
							[
								SNew(SWRPanel).Kind(EWRPanel::Leather).Padding(FMargin(18.f * U, 0.f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Strip]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
									[
										WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return WhoLine(); }), WRUI::Caps(13), WRUI::Ink2, true)
									]
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").Cursor(EMouseCursor::Hand).ToolTipText(LOCTEXT("Close", "Close (Esc)"))
										.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->SetMenu(EWRMenu::None); return FReply::Handled(); })
										[
											SNew(SBox).WidthOverride(36.f * U).HeightOverride(36.f * U)
											[
												SNew(SOverlay)
												+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.35f), 4.f, WRUI::Brass2, 1.f))]
												+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(TEXT("X")), WRUI::Title(15), WRUI::Ink)]
											]
										]
									]
								]
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(16.f * U, 22.f * U, 16.f * U, 14.f * U))
						[
							SAssignNew(Content, SBox)
						]
					]
				]
			]
		];
		ShowTab();
	}

	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
	{
		SCompoundWidget::Tick(Geo, Time, Delta);
		if (PC.IsValid() && PC->GetMenu() != Shown) ShowTab();
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
	TSharedPtr<SBox> Content;
	EWRMenu Shown = EWRMenu::None;

	bool IsOn(EWRMenu Menu) const { return PC.IsValid() && PC->GetMenu() == Menu; }

	FText WhoLine() const
	{
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		const UWRPathComponent* Paths = PC.IsValid() && PC->GetPawn() ? UWRPathComponent::FindFor(PC->GetPawn()) : nullptr;
		if (!PS) return FText::GetEmpty();
		return FText::FromString(FString::Printf(TEXT("%s · Level %d%s"), *PS->GetPlayerName(), Paths ? Paths->GetCharacterLevel() : 1,
			Paths && !Paths->GetMainPath().IsNone() ? *(TEXT(" · ") + Paths->GetMainPath().ToString()) : TEXT("")));
	}

	void ShowTab()
	{
		if (!PC.IsValid() || !Content.IsValid()) return;
		Shown = PC->GetMenu();
		const TCHAR* Tab = Shown == EWRMenu::Inventory ? TEXT("inventory") : Shown == EWRMenu::Craft ? TEXT("crafting") : Shown == EWRMenu::Paths ? TEXT("paths")
			: Shown == EWRMenu::Skills ? TEXT("skills") : Shown == EWRMenu::Map ? TEXT("map") : nullptr;
		const FWRScreenFactory* Factory = Tab ? FWRScreens::FindTab(Tab) : nullptr;
		if (Factory)
		{
			Content->SetContent((*Factory)(PC.Get()));
		}
		else
		{
			Content->SetContent(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("TabSoon", "This page is on its way."), WRUI::Title(18), WRUI::Ink3)]);
		}
	}
};

WR_REGISTER_SCREEN("journal", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(SWRJournal).PC(PC); });

#undef LOCTEXT_NAMESPACE
