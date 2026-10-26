#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "WRUIWidgets.h"

class AWRPlayerController;
class SWRInvAmountSlider;
struct FWRItemStack;

/**
 * Pieces the two bag screens share (the journal's Inventory tab, WRUIInventory.cpp, and the storage chest,
 * WRUIStorage.cpp): slot views of stacks, a drop zone, the "how many?" picker, line icons, icon buttons and sorting
 * the bag. Everything they change goes through the server's commands (move, split, drop, store, take...).
 */

/** Line art on a 24x24 grid (the mockups' little SVG icons): polylines plus filled dots. */
struct FWRInvIcon
{
	TArray<TArray<FVector2f>> Lines;
	TArray<FVector3f> Dots;   // x, y, radius
};

class WILDREACH_API SWRInvLineIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWRInvLineIcon) : _Size(16.f), _Thickness(2.f), _Color(FLinearColor::White) {}
		SLATE_ARGUMENT(FWRInvIcon, Icon)
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(float, Thickness)
		SLATE_ATTRIBUTE(FLinearColor, Color)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	FWRInvIcon Icon;
	float Size = 16.f;
	float Thickness = 2.f;
	TAttribute<FLinearColor> Color;
};

/** Catches items dropped on its area (not on a slot inside it, which handles its own). */
class WILDREACH_API SWRInvDropZone : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRInvDropZone) {}
		SLATE_EVENT(FOnWRSlotDropped, OnDropped)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual FReply OnDrop(const FGeometry& Geo, const FDragDropEvent& Event) override;
private:
	FOnWRSlotDropped OnDropped;
};

DECLARE_DELEGATE_OneParam(FOnWRInvAmount, int32);

/** "Move how many?": the item, a brass slider from 1 to Max, a line about what's left, Cancel and a confirm button. */
class WILDREACH_API SWRInvAmountPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRInvAmountPicker) : _Max(1), _Initial(1) {}
		SLATE_ARGUMENT(FName, ItemId)
		SLATE_ARGUMENT(int32, Max)
		SLATE_ARGUMENT(int32, Initial)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, ConfirmText)
		/** Under the title, for the amount picked ("Wood into Storage chest · 5 kg"). */
		SLATE_ARGUMENT(TFunction<FText(int32)>, SubLine)
		/** Bottom left ("55 stay in your bag"). */
		SLATE_ARGUMENT(TFunction<FText(int32)>, RestLine)
		SLATE_EVENT(FOnWRInvAmount, OnConfirm)
		SLATE_EVENT(FSimpleDelegate, OnCancel)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& Event) override;
	int32 GetValue() const { return Value; }
private:
	void Confirm();
	int32 Value = 1;
	int32 Max = 1;
	FOnWRInvAmount OnConfirm;
	FSimpleDelegate OnCancel;
};

namespace WRInv
{
	/** What a bag / chest / worn stack shows in a slot (icon, count, durability, quality rim). */
	FWRSlotView StackView(const FWRItemStack& Stack);
	/** "1.2", "16", "0.05" (kg, no unit). */
	FString Kg(float Kg);
	FString ItemName(FName ItemId);
	/** "Stone arrows" for more than one. */
	FString Plural(const FString& Name, int32 Count);
	/** Sort the bag (gear, ammo, food, resources...), merging part stacks, with move commands the server runs in order. */
	void SortBag(AWRPlayerController* PC);
	/** A leather button with a line icon before or after its label (SWRButton's look). */
	TSharedRef<SWidget> IconButton(const FText& Text, const FWRInvIcon& Icon, bool bIconAfter, bool bPrimary, bool bGhost, int32 FontSize, float MinHeight, FOnClicked OnClicked);
	/** The same without an icon; a disabled one fades (the mockup's desaturated leather). */
	TSharedRef<SWidget> TextButton(const FText& Text, bool bPrimary, bool bGhost, int32 FontSize, float MinHeight, FOnClicked OnClicked, bool bEnabled = true);
	/** A text block with justification and wrapping (WRUI::Text has neither). */
	TSharedRef<class STextBlock> Label(const TAttribute<FText>& Text, const FSlateFontInfo& Font, const TAttribute<FSlateColor>& Color, bool bWrap = false, ETextJustify::Type Justify = ETextJustify::Left);
	/** The item drag in progress, if any. */
	TSharedPtr<FWRItemDragDrop> CurrentDrag();

	namespace Icons
	{
		FWRInvIcon Sort();
		FWRInvIcon ArrowRight();
		FWRInvIcon ArrowLeft();
		FWRInvIcon Swap();
		FWRInvIcon Drag();
		FWRInvIcon Flag();
		FWRInvIcon Paw();
		FWRInvIcon Info();
	}
}
