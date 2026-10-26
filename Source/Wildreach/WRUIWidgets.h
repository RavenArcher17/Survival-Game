#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/DragAndDrop.h"
#include "InputCoreTypes.h"

/** Building blocks of the stone-and-leather UI (WRUIStyle has the look). */

enum class EWRPanel : uint8
{
	Stone,      // carved dark stone: windows
	Glass,      // translucent stone: HUD pieces over the game
	Leather,    // stitched leather: straps, side panels
	Parchment,  // parchment with dark ink
	Inset,      // a recessed area inside a panel
};

/** A framed panel: material, optional brass trim and corner rivets, then content. */
class WILDREACH_API SWRPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRPanel) : _Kind(EWRPanel::Stone), _Rivets(false), _BrassTrim(false), _Padding(FMargin(16.f)) {}
		SLATE_ARGUMENT(EWRPanel, Kind)
		SLATE_ARGUMENT(bool, Rivets)
		SLATE_ARGUMENT(bool, BrassTrim)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
};

/** Where a dragged item came from / is going: the bag, an equipment slot, a chest, the hotbar. */
enum class EWRSlotKind : uint8 { Bag, Equipment, Chest, Hotbar, Other };

/** What one slot shows. */
struct FWRSlotView
{
	FName ItemId;              // None = empty
	int32 Count = 0;           // shown when > 1 (or ShowCount)
	bool bShowCount = false;
	float Durability = -1.f;   // 0..1, < 0 = none
	FText Key;                 // small label top-left (hotbar key)
	bool bSelected = false;
	bool bLocked = false;
	bool bDimmed = false;
	int32 Quality = 0;         // 0 common, 1 fine, 2 masterwork
	float IconRotation = 0.f;  // degrees
};

class FWRItemDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWRItemDragDrop, FDragDropOperation)
	EWRSlotKind From = EWRSlotKind::Bag;
	int32 Index = INDEX_NONE;
	FName ItemId;
	int32 Count = 0;
	static TSharedRef<FWRItemDragDrop> New(EWRSlotKind From, int32 Index, FName ItemId, int32 Count);
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};

DECLARE_DELEGATE_TwoParams(FOnWRSlotDropped, EWRSlotKind /*From*/, int32 /*FromIndex*/);
DECLARE_DELEGATE_OneParam(FOnWRSlotClicked, const FPointerEvent&);

/** An item slot: icon, count, durability, key label; clickable, a drag source and a drop target. */
class WILDREACH_API SWRSlot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRSlot) : _Size(64.f), _Kind(EWRSlotKind::Other), _Index(INDEX_NONE) {}
		SLATE_ATTRIBUTE(FWRSlotView, View)
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(EWRSlotKind, Kind)
		SLATE_ARGUMENT(int32, Index)
		SLATE_EVENT(FOnWRSlotClicked, OnClicked)
		SLATE_EVENT(FOnWRSlotDropped, OnDropped)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Event) override;
	virtual FReply OnDragDetected(const FGeometry& Geo, const FPointerEvent& Event) override;
	virtual void OnDragEnter(const FGeometry& Geo, const FDragDropEvent& Event) override;
	virtual void OnDragLeave(const FDragDropEvent& Event) override;
	virtual FReply OnDrop(const FGeometry& Geo, const FDragDropEvent& Event) override;

private:
	TAttribute<FWRSlotView> View;
	EWRSlotKind Kind = EWRSlotKind::Other;
	int32 Index = INDEX_NONE;
	FOnWRSlotClicked OnClicked;
	FOnWRSlotDropped OnDropped;
	bool bDropHover = false;
};

/** A filled bar (health, stamina, torpor...). */
class WILDREACH_API SWRBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRBar) : _Height(14.f), _Color(FLinearColor::White) {}
		SLATE_ATTRIBUTE(float, Fraction)
		SLATE_ARGUMENT(float, Height)
		SLATE_ATTRIBUTE(FLinearColor, Color)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(40.f, Height); }
private:
	TAttribute<float> Fraction;
	TAttribute<FLinearColor> Color;
	float Height = 14.f;
};

/** A keycap showing a key ("E", "LMB", "F1"). */
class WILDREACH_API SWRKeyCap : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRKeyCap) : _FontSize(12) {}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(int32, FontSize)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
};

/** A leather button with a Cinzel label. */
class WILDREACH_API SWRButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRButton) : _Primary(false), _Ghost(false), _FontSize(14), _MinHeight(44.f) {}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(bool, Primary)
		SLATE_ARGUMENT(bool, Ghost)
		SLATE_ARGUMENT(int32, FontSize)
		SLATE_ARGUMENT(float, MinHeight)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
};

/** A pill-shaped tag ("Fine", "Following", "Locked"). */
class WILDREACH_API SWRChip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRChip) : _Color(FLinearColor::White), _FontSize(12) {}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ARGUMENT(int32, FontSize)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
};

namespace WRUI
{
	/** A text block in the UI's look; bShadow for text straight over the 3D scene. */
	TSharedRef<SWidget> Text(const TAttribute<FText>& Text, const FSlateFontInfo& Font, const TAttribute<FSlateColor>& Color, bool bShadow = false, bool bWrap = false);
	/** A short name for a key: "LMB", "Wheel up", "E". */
	FText KeyName(const FKey& Key);
	/** A thin brass divider line. */
	TSharedRef<SWidget> Divider();
	/** MakeBox with the brush's own colour (custom-painted widgets). */
	void DrawBox(class FSlateWindowElementList& Out, int32 Layer, const struct FPaintGeometry& Geometry, const FSlateBrush* Brush);
}
