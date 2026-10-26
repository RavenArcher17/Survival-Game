// The journal's Crafting tab (mockup "Crafting"): the station you stand at (or by hand) and the stations around, recipe
// categories, the recipes as cards (ready to make, short of something, or made at another station / a higher tier),
// and the picked recipe's details: what it makes, its ingredients have / need, quality odds and infusing for gear, how
// many, and Craft. Repairs and upgrades of your equipped gear are a category of their own, done where the old text
// panel did them. Every action is a server command checked there ("craft <id> <none|fire|frost>", "repair <slot>",
// "upgrade <slot>"); a quantity of N sends N craft commands, each validated. The tab only reads replicated state.

#include "WRPlayerUI.h"
#include "WRUIStyle.h"
#include "WRUIWidgets.h"
#include "WRCrafting.h"
#include "WRInventory.h"
#include "WRKeyBindings.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "WRUI"

// A named namespace, not an anonymous one: unity builds put several UI files in one translation unit.
namespace WRCraftUI
{
	using WRUI::U;   // mockup px -> 1080p units

	const FName Hand(TEXT("hand"));
	const FName Stations[] = { TEXT("hand"), TEXT("workbench"), TEXT("forge"), TEXT("armorer"), TEXT("alchemy_table") };

	FText StationName(FName Type)
	{
		if (Type == TEXT("workbench")) return LOCTEXT("StWorkbench", "Workbench");
		if (Type == TEXT("forge")) return LOCTEXT("StForge", "Forge");
		if (Type == TEXT("armorer")) return LOCTEXT("StArmorer", "Armorer");
		if (Type == TEXT("alchemy_table")) return LOCTEXT("StAlchemy", "Alchemy table");
		return LOCTEXT("StHand", "By hand");
	}

	/** A picture for each station: an item it's known for. */
	FName StationIcon(FName Type)
	{
		if (Type == TEXT("workbench")) return TEXT("stone_hatchet");
		if (Type == TEXT("forge")) return TEXT("copper_ingot");
		if (Type == TEXT("armorer")) return TEXT("hide_armor");
		if (Type == TEXT("alchemy_table")) return TEXT("minor_healing_potion");
		return TEXT("crude_hatchet");
	}

	struct FCategory { FName Id; FText Name; FName Icon; };
	const TArray<FCategory>& Categories()
	{
		static const TArray<FCategory> List = {
			{ TEXT("tools"), LOCTEXT("CatTools", "Tools"), TEXT("stone_pick") },
			{ TEXT("weapons"), LOCTEXT("CatWeapons", "Weapons"), TEXT("copper_sword") },
			{ TEXT("ranged"), LOCTEXT("CatRanged", "Ranged"), TEXT("hunting_bow") },
			{ TEXT("armor"), LOCTEXT("CatArmor", "Armor"), TEXT("hide_armor") },
			{ TEXT("food"), LOCTEXT("CatFood", "Food"), TEXT("cooked_meat") },
			{ TEXT("potions"), LOCTEXT("CatPotions", "Potions"), TEXT("minor_healing_potion") },
			{ TEXT("taming"), LOCTEXT("CatTaming", "Taming"), TEXT("kibble") },
			{ TEXT("materials"), LOCTEXT("CatMaterials", "Materials"), TEXT("copper_ingot") },
			{ TEXT("repair"), LOCTEXT("CatRepair", "Repair"), TEXT("upgrade_shard") },
		};
		return List;
	}

	FName CategoryOf(const FWRItemDef* Def)
	{
		if (!Def) return TEXT("materials");
		const FName C = Def->Category;
		// Knock-out gear sits with taming: kibble, narcotics, torpor darts and tranq ammo.
		if (C == TEXT("tame_food") || C == TEXT("dart") || Def->Id == TEXT("narcotic") || (C == TEXT("ammo") && Def->AmmoTorpor > Def->AmmoDamage)) return TEXT("taming");
		if (C == TEXT("tool")) return TEXT("tools");
		if (C == TEXT("weapon")) return TEXT("weapons");
		if (C == TEXT("ranged") || C == TEXT("ammo")) return TEXT("ranged");
		if (C == TEXT("armor")) return TEXT("armor");
		if (C == TEXT("food") || C == TEXT("drink")) return TEXT("food");
		if (C == TEXT("potion")) return TEXT("potions");
		return TEXT("materials");
	}

	/** Bits of items.json the item registry doesn't keep: a tool's kind and yield, the player-facing note. */
	struct FItemExtra { FString ToolKind; float Yield = 0.f; FString Note; };
	const FItemExtra* ItemExtra(FName Id)
	{
		static TMap<FName, FItemExtra> Extras;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("items.json"));
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) continue;
					FItemExtra X;
					O->TryGetStringField(TEXT("note"), X.Note);
					const TSharedPtr<FJsonObject>* Tool = nullptr;
					if (O->TryGetObjectField(TEXT("tool"), Tool))
					{
						(*Tool)->TryGetStringField(TEXT("kind"), X.ToolKind);
						(*Tool)->TryGetNumberField(TEXT("yield"), X.Yield);
					}
					Extras.Add(FName(*O->GetStringField(TEXT("id"))), X);
				}
			}
		}
		return Extras.Find(Id);
	}

	FString ItemName(FName Id)
	{
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(Id);
		return Def ? Def->Name : Id.ToString();
	}

	/** "Ranged weapon", "Tool"... for the details panel. */
	FText KindLabel(const FWRItemDef* Def)
	{
		if (!Def) return FText::GetEmpty();
		const FName C = Def->Category;
		if (C == TEXT("weapon")) return LOCTEXT("KWeapon", "Melee weapon");
		if (Def->IsThrowable()) return LOCTEXT("KThrown", "Thrown weapon");
		if (Def->IsRanged()) return LOCTEXT("KRanged", "Ranged weapon");
		if (C == TEXT("ammo") || C == TEXT("dart")) return LOCTEXT("KAmmo", "Ammo");
		if (C == TEXT("armor")) return LOCTEXT("KArmor", "Armor");
		if (C == TEXT("tool")) return LOCTEXT("KTool", "Tool");
		if (C == TEXT("food")) return LOCTEXT("KFood", "Food");
		if (C == TEXT("drink")) return LOCTEXT("KDrink", "Drink");
		if (C == TEXT("potion")) return LOCTEXT("KPotion", "Potion");
		if (C == TEXT("tame_food")) return LOCTEXT("KTameFood", "Taming food");
		return LOCTEXT("KMaterial", "Material");
	}

	/** What the item does, from its stats (the details panel's line under the name). */
	FString StatLine(const FWRItemDef* Def)
	{
		if (!Def) return FString();
		TArray<FString> Bits;
		const FItemExtra* X = ItemExtra(Def->Id);
		if (Def->Category == TEXT("weapon"))
		{
			const float Metal = FWRTuning::Get().Num(FString::Printf(TEXT("weapon.metal.%s"), *Def->Metal.ToString()), 1.f);
			Bits.Add(FString::Printf(TEXT("%.0f light / %.0f heavy damage at common quality"), WRTune(TEXT("weapon.copperSwordLight"), 12.f) * Metal, WRTune(TEXT("weapon.copperSwordHeavy"), 22.f) * Metal));
		}
		else if (Def->IsThrowable()) Bits.Add(FString::Printf(TEXT("Thrown for %.0f damage"), Def->RangedDamage));
		else if (Def->IsRanged()) Bits.Add(FString::Printf(TEXT("Shoots %ss: %.0f damage, %.1f s between shots"), *Def->AmmoKind.ToString(), Def->RangedDamage, Def->Reload));
		else if (!Def->AmmoOf.IsNone()) Bits.Add(FString::Printf(TEXT("%.0f damage%s"), Def->AmmoDamage, Def->AmmoTorpor > 0.f ? *FString::Printf(TEXT(", %.0f torpor"), Def->AmmoTorpor) : TEXT("")));
		if (Def->Armor > 0.f) Bits.Add(FString::Printf(TEXT("%.0f armor"), Def->Armor));
		if (X && X->Yield > 0.f) Bits.Add(FString::Printf(TEXT("Gathers %s x%.1f; works from your bag"), X->ToolKind == TEXT("hatchet") ? TEXT("wood") : X->ToolKind == TEXT("pick") ? TEXT("stone and ore") : *X->ToolKind, X->Yield));
		if (Def->Heal > 0.f) Bits.Add(FString::Printf(TEXT("Heals %.0f"), Def->Heal));
		if (Def->Hunger > 0.f) Bits.Add(FString::Printf(TEXT("+%.0f food"), Def->Hunger));
		if (Def->Thirst > 0.f) Bits.Add(FString::Printf(TEXT("+%.0f water"), Def->Thirst));
		if (Def->bCleanse) Bits.Add(TEXT("Cures poison"));
		if (Def->MaxDurability > 0.f) Bits.Add(FString::Printf(TEXT("%.0f durability"), Def->MaxDurability));
		if (Bits.Num() == 0 && X && !X->Note.IsEmpty() && Def->Category == TEXT("tame_food"))
		{
			FString Note = X->Note;
			Note[0] = FChar::ToUpper(Note[0]);
			Bits.Add(Note);
		}
		if (Bits.Num() == 0) Bits.Add(Def->Category == TEXT("material") ? TEXT("Used to make other things") : TEXT(""));
		FString Line = FString::Join(Bits, TEXT(" · "));
		return Line;
	}

	/** Small drawn marks: ready / short / locked, the "all" grid, the infuse "none" ring, the search glass, radio buttons. */
	class SGlyph : public SLeafWidget
	{
	public:
		enum class EKind : uint8 { Ok, No, Lock, Grid, NoneRing, Search, Radio, RadioOn, Check };
		SLATE_BEGIN_ARGS(SGlyph) : _Kind(EKind::Ok), _Size(18.f), _Color(WRUI::Ink2) {}
			SLATE_ATTRIBUTE(EKind, Kind)
			SLATE_ARGUMENT(float, Size)
			SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Kind = InArgs._Kind; Size = InArgs._Size; Color = InArgs._Color; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			// Drawn on a 24-unit grid (the mockup's SVG viewBox), scaled to the widget.
			const float K = Geo.GetLocalSize().X / 24.f;
			const auto P = [K](float X, float Y) { return FVector2D(X * K, Y * K); };
			const auto Line = [&](TArray<FVector2D> Pts, const FLinearColor& C, float W) { FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, C, true, W * K); };
			const auto Disc = [&](float CX, float CY, float R, const FLinearColor& Fill, const FLinearColor& Edge, float W)
			{
				WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(FVector2D(R * 2.f * K, R * 2.f * K), FSlateLayoutTransform(P(CX - R, CY - R))), WRUI::Box(Fill, R * K, Edge, W * K));
			};
			const FLinearColor C = Color.Get();
			switch (Kind.Get())
			{
			case EKind::Ok:
				Disc(12, 12, 10, WRUI::Alpha(WRUI::Good, 0.18f), WRUI::Good, 1.6f);
				Line({ P(7, 12.5f), P(10.2f, 15.7f), P(17, 9) }, WRUI::Good, 2.4f);
				break;
			case EKind::No:
				Disc(12, 12, 10, WRUI::Alpha(WRUI::Bad, 0.16f), WRUI::Bad, 1.6f);
				Line({ P(8.5f, 8.5f), P(15.5f, 15.5f) }, WRUI::Bad, 2.2f);
				Line({ P(15.5f, 8.5f), P(8.5f, 15.5f) }, WRUI::Bad, 2.2f);
				break;
			case EKind::Lock:
			{
				TArray<FVector2D> Arc;
				for (int32 i = 0; i <= 10; ++i)
				{
					const float A = PI * i / 10.f;
					Arc.Add(P(12.f - 4.5f * FMath::Cos(A), 8.f - 4.5f * FMath::Sin(A)));
				}
				Arc.Insert(P(7.5f, 11.5f), 0);
				Arc.Add(P(16.5f, 11.5f));
				Line(Arc, C, 2.f);
				WRUI::DrawBox(Out, Layer + 2, Geo.ToPaintGeometry(P(14, 10), FSlateLayoutTransform(P(5, 11))), WRUI::Box(C, 2.f * K));
				break;
			}
			case EKind::Grid:
				for (const FVector2D& O : { FVector2D(3, 3), FVector2D(13, 3), FVector2D(3, 13), FVector2D(13, 13) })
				{
					WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(P(8, 8), FSlateLayoutTransform(P(O.X, O.Y))), WRUI::Box(C, 1.f * K));
				}
				break;
			case EKind::NoneRing:
				Disc(12, 12, 8, FLinearColor::Transparent, C, 2.f);
				Line({ P(6.5f, 17.5f), P(17.5f, 6.5f) }, C, 2.f);
				break;
			case EKind::Search:
				Disc(10.5f, 10.5f, 6.5f, FLinearColor::Transparent, C, 2.2f);
				Line({ P(15.5f, 15.5f), P(21, 21) }, C, 2.4f);
				break;
			case EKind::Radio:
				Disc(12, 12, 10, FLinearColor(0.f, 0.f, 0.f, 0.5f), C, 2.5f);
				break;
			case EKind::RadioOn:
				Disc(12, 12, 10, FLinearColor(0.f, 0.f, 0.f, 0.5f), WRUI::Brass, 2.5f);
				Disc(12, 12, 4.5f, FLinearColor(0.93f, 0.79f, 0.54f), WRUI::Brass2, 1.f);
				break;
			case EKind::Check:
				Line({ P(5, 12.5f), P(9.5f, 17), P(19, 6.5f) }, C, 3.f);
				break;
			}
			return Layer + 2;
		}
	private:
		TAttribute<EKind> Kind;
		float Size = 18.f;
		TAttribute<FLinearColor> Color;
	};

	/** The quality odds bar: crude / common / fine / masterwork. */
	class SOddsBar : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SOddsBar) {}
			SLATE_ARGUMENT(TArray<float>, Odds)
		SLATE_END_ARGS()
		void Construct(const FArguments& InArgs) { Odds = InArgs._Odds; }
		static const FLinearColor& Color(int32 i)
		{
			static const FLinearColor C[4] = { FLinearColor(FColor(0x7A, 0x5C, 0x46)), FLinearColor(FColor(0x8B, 0x8F, 0x95)), FLinearColor(FColor(0x5F, 0x9E, 0x3E)), FLinearColor(FColor(0x3F, 0x6F, 0xA8)) };
			return C[FMath::Clamp(i, 0, 3)];
		}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 14.f * U); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2D S = Geo.GetLocalSize();
			WRUI::DrawBox(Out, Layer, Geo.ToPaintGeometry(), WRUI::Box(FLinearColor(0.047f, 0.05f, 0.06f), 3.f, FLinearColor::Black, 1.f));
			float X = 1.f;
			const float W = S.X - 2.f;
			for (int32 i = 0; i < Odds.Num(); ++i)
			{
				const float SegW = W * Odds[i];
				if (SegW < 0.5f) continue;
				FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2D(SegW, S.Y - 2.f), FSlateLayoutTransform(FVector2D(X, 1.f))), WRUI::White(), ESlateDrawEffect::None, Color(i));
				FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(SegW, (S.Y - 2.f) * 0.45f), FSlateLayoutTransform(FVector2D(X, 1.f))), WRUI::White(), ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.16f));
				if (X > 1.5f) FSlateDrawElement::MakeBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(1.f, S.Y - 2.f), FSlateLayoutTransform(FVector2D(X, 1.f))), WRUI::White(), ESlateDrawEffect::None, FLinearColor::Black);
				X += SegW;
			}
			return Layer + 3;
		}
	private:
		TArray<float> Odds;
	};

	/** One line of the recipe book: a recipe, or a repair / upgrade of a piece of equipped gear. */
	struct FEntry
	{
		enum class EKind : uint8 { Recipe, Repair, Upgrade };
		EKind Kind = EKind::Recipe;
		FString Key;                      // recipe id, or "repair weapon" / "upgrade armor"
		const FWRRecipe* Recipe = nullptr;
		EWREquipSlot Slot = EWREquipSlot::None;
		FName Category;
		FName Icon;
		FString Name;
		int32 OutCount = 1;
		FName Station;                    // where it's done
		int32 Level = 1;
		TArray<TPair<FName, int32>> Cost;
		bool bHere = false;               // this station (or by hand) does it
		bool bTierOk = true;
		int32 CanMake = 0;                // times it can be done right now
		FString Blocker;                  // why it can't be done here and now, other than ingredients
		FString Command;
		float Durability = -1.f;
		float MaxDurability = 0.f;
		int32 Upgrade = 0;
		int32 UpgradeCap = 0;

		bool Locked() const { return !bHere || !bTierOk; }
		bool Ready() const { return !Locked() && Blocker.IsEmpty() && CanMake > 0; }
	};
}

// The screen itself, in the same namespace (a using-directive would leak into the unity file).
namespace WRCraftUI
{
class SWRCraftingTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWRCraftingTab) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWRPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PC = InArgs._PC;
		Station = PC.IsValid() ? PC->GetMenuStation() : nullptr;

		ChildSlot
		[
			SNew(SVerticalBox)
			// Top row: where you craft, and the stations around you.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f * U)
			[
				SAssignNew(TopRow, SBox)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				// Left: categories, the skill that levels, the count.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(160.f * U)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(Nav, SVerticalBox)]
						+ SVerticalBox::Slot().FillHeight(1.f)[SNullWidget::NullWidget]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(NavFoot, SBox)]
					]
				]
				// Middle: search and the recipe cards.
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22.f * U, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f * U)[SearchRow()]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(List, SScrollBox).ScrollBarThickness(FVector2D(5.f, 5.f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)[FooterHints()]
				]
				// Right: the picked recipe.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(420.f * U)
					[
						SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(16.f * U, 16.f * U, 18.f * U, 16.f * U))
						[
							SAssignNew(Details, SBox)
						]
					]
				]
			]
		];
		RebuildAll();
	}

	virtual void Tick(const FGeometry& Geo, const double Time, const float Delta) override
	{
		SCompoundWidget::Tick(Geo, Time, Delta);
		if (!PC.IsValid()) return;
		if (PC->GetMenuStation() != Station.Get())
		{
			Station = PC->GetMenuStation();
			bDirty = true;
		}
		const uint32 Hash = StateHash();
		if (bDirty || Hash != LastHash)
		{
			LastHash = Hash;
			bDirty = false;
			RebuildAll();
		}
	}

private:
	TWeakObjectPtr<AWRPlayerController> PC;
	TWeakObjectPtr<AWRStation> Station;
	TSharedPtr<SBox> TopRow, NavFoot, Details;
	TSharedPtr<SVerticalBox> Nav;
	TSharedPtr<SScrollBox> List;
	TArray<FEntry> Entries;
	FName Category;              // None = all
	FString Search;
	bool bReadyOnly = false;
	FString SelectedKey;
	FName Element;               // infuse: None, fire, frost
	int32 Quantity = 1;
	bool bDirty = false;
	uint32 LastHash = 0;

	// ---------------------------------------------------------------- state

	const UWRInventoryComponent* Inv() const
	{
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		return PS ? PS->GetInventory() : nullptr;
	}
	const UWRLifeSkillsComponent* Skills() const
	{
		const AWRPlayerState* PS = PC.IsValid() ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		return PS ? PS->GetLifeSkills() : nullptr;
	}
	int32 Have(FName Item) const { const UWRInventoryComponent* I = Inv(); return I ? I->CountOf(Item) : 0; }
	FName StationType() const { return Station.IsValid() ? Station->StationType : Hand; }
	int32 Tier() const { return Station.IsValid() ? Station->GetTier() : 1; }

	/** Everything the cards show that can change while the tab is open. */
	uint32 StateHash() const
	{
		uint32 H = GetTypeHash(Station.Get());
		H = HashCombine(H, GetTypeHash(Tier()));
		if (const UWRInventoryComponent* I = Inv())
		{
			for (const FWRItemStack& S : I->GetSlots()) H = HashCombine(H, HashCombine(GetTypeHash(S.ItemId), GetTypeHash(S.Count)));
			for (const EWREquipSlot Slot : { EWREquipSlot::Weapon, EWREquipSlot::Armor, EWREquipSlot::Ranged })
			{
				const FWRItemStack& E = I->GetEquipped(Slot);
				H = HashCombine(H, HashCombine(GetTypeHash(E.ItemId), HashCombine(GetTypeHash(FMath::RoundToInt(E.Durability)), GetTypeHash(E.Upgrade))));
			}
		}
		if (const UWRLifeSkillsComponent* S = Skills())
		{
			for (const FWRSkillProgress& P : S->GetSkills()) H = HashCombine(H, HashCombine(GetTypeHash(P.Level), GetTypeHash(P.XP)));
		}
		return H;
	}

	void GatherEntries()
	{
		Entries.Reset();
		const FName Here = StationType();
		const int32 T = Tier();
		for (const FName St : Stations)
		{
			for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(St))
			{
				const FWRItemDef* Out = FWRItemRegistry::Get().Find(R->Output);
				FEntry E;
				E.Kind = FEntry::EKind::Recipe;
				E.Key = R->Id.ToString();
				E.Recipe = R;
				E.Category = CategoryOf(Out);
				E.Icon = R->Output;
				E.OutCount = R->Count;
				E.Name = Out ? Out->Name : R->Output.ToString();
				if (R->Count > 1) E.Name += FString::Printf(TEXT(" x%d"), R->Count);
				E.Station = R->Station;
				E.Level = R->Level;
				E.Cost = R->Inputs;
				// Hand recipes are made anywhere, a station's only standing at one (the server's rule).
				E.bHere = R->Station == Hand || R->Station == Here;
				E.bTierOk = R->Station == Hand || !E.bHere || R->Level <= T;
				E.Command = FString::Printf(TEXT("craft %s"), *R->Id.ToString());
				if (!E.bHere) E.Blocker = R->Level > 1 ? FString::Printf(TEXT("%s · tier %d"), *StationName(R->Station).ToString(), R->Level) : StationName(R->Station).ToString();
				else if (!E.bTierOk) E.Blocker = FString::Printf(TEXT("Needs a tier %d %s"), R->Level, *StationName(R->Station).ToString().ToLower());
				E.CanMake = MaxTimes(E.Cost);
				Entries.Add(MoveTemp(E));
			}
		}
		// Repairs and upgrades of what you wear, where the old panel offered them: weapons at the forge, armor at the
		// armorer, the ranged weapon where it's made.
		const UWRInventoryComponent* I = Inv();
		if (!I) return;
		const float Fraction = FWRRecipeRegistry::Get().RepairCostFraction;
		for (const EWREquipSlot Slot : { EWREquipSlot::Weapon, EWREquipSlot::Ranged, EWREquipSlot::Armor })
		{
			const FWRItemStack& Worn = I->GetEquipped(Slot);
			const FWRItemDef* Def = Worn.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Worn.ItemId);
			if (!Def || Def->MaxDurability <= 0.f) continue;
			const FWRRecipe* R = FWRRecipeRegistry::Get().ForOutput(Def->Id);
			const TCHAR* SlotName = Slot == EWREquipSlot::Armor ? TEXT("armor") : Slot == EWREquipSlot::Ranged ? TEXT("ranged") : TEXT("weapon");
			const FName At = Slot == EWREquipSlot::Weapon ? FName(TEXT("forge")) : Slot == EWREquipSlot::Armor ? FName(TEXT("armorer")) : R ? R->Station : NAME_None;
			if (At.IsNone() || At == Hand) continue;

			FEntry Fix;
			Fix.Kind = FEntry::EKind::Repair;
			Fix.Key = FString::Printf(TEXT("repair %s"), SlotName);
			Fix.Slot = Slot;
			Fix.Category = TEXT("repair");
			Fix.Icon = Def->Id;
			Fix.Name = FString::Printf(TEXT("Repair: %s"), *Def->Name);
			Fix.Station = At;
			if (R) for (const TPair<FName, int32>& In : R->Inputs) Fix.Cost.Add({ In.Key, FMath::CeilToInt(In.Value * Fraction) });
			Fix.bHere = Here == At;
			Fix.Durability = Worn.Durability;
			Fix.MaxDurability = Def->MaxDurability;
			Fix.Command = Fix.Key;
			if (!Fix.bHere) Fix.Blocker = StationName(At).ToString();
			else if (Worn.Durability >= Def->MaxDurability) Fix.Blocker = TEXT("Like new");
			Fix.CanMake = FMath::Min(1, MaxTimes(Fix.Cost));
			Entries.Add(MoveTemp(Fix));

			if (Slot == EWREquipSlot::Ranged) continue;
			FEntry Up;
			Up.Kind = FEntry::EKind::Upgrade;
			Up.Key = FString::Printf(TEXT("upgrade %s"), SlotName);
			Up.Slot = Slot;
			Up.Category = TEXT("repair");
			Up.Icon = Def->Id;
			Up.Name = FString::Printf(TEXT("Upgrade: %s +%d"), *Def->Name, Worn.Upgrade + 1);
			Up.Station = At;
			Up.Cost.Add({ TEXT("upgrade_shard"), FWRCraftingRules::UpgradeCost(Worn.Upgrade) });
			Up.bHere = Here == At;
			Up.Upgrade = Worn.Upgrade;
			Up.UpgradeCap = FMath::Min(10, FWRCraftingRules::UpgradeCap(Up.bHere ? T : 1));
			Up.Command = Up.Key;
			if (!Up.bHere) Up.Blocker = StationName(At).ToString();
			else if (Worn.Upgrade >= Up.UpgradeCap) Up.Blocker = FString::Printf(TEXT("+%d is the cap at tier %d"), Up.UpgradeCap, T);
			Up.CanMake = FMath::Min(1, MaxTimes(Up.Cost));
			Entries.Add(MoveTemp(Up));
		}
	}

	int32 MaxTimes(const TArray<TPair<FName, int32>>& Cost) const
	{
		int32 N = 999;
		for (const TPair<FName, int32>& C : Cost) N = FMath::Min(N, C.Value > 0 ? Have(C.Key) / C.Value : 999);
		return N;
	}

	bool Matches(const FEntry& E) const
	{
		if (bReadyOnly && !E.Ready()) return false;
		if (Search.IsEmpty()) return true;
		if (E.Name.Contains(Search)) return true;
		for (const TPair<FName, int32>& C : E.Cost) if (ItemName(C.Key).Contains(Search)) return true;
		return false;
	}

	const FEntry* Selected() const { return Entries.FindByPredicate([this](const FEntry& E) { return E.Key == SelectedKey; }); }

	/** Gear that takes an essence: a quality recipe making one piece of equipment (FWRCrafting::Make's rule). */
	bool CanInfuse(const FEntry& E) const
	{
		const FWRItemDef* Out = E.Recipe ? FWRItemRegistry::Get().Find(E.Recipe->Output) : nullptr;
		return E.Kind == FEntry::EKind::Recipe && E.Recipe->bQuality && E.Recipe->Station != Hand && Out && Out->IsEquipment() && Out->StackSize <= 1;
	}
	static FName EssenceFor(FName El) { return El == TEXT("fire") ? FName(TEXT("ember_essence")) : El == TEXT("frost") ? FName(TEXT("frost_essence")) : NAME_None; }

	int32 MaxQuantity(const FEntry& E) const
	{
		if (!E.Ready()) return 0;
		if (E.Kind != FEntry::EKind::Recipe) return 1;
		int32 N = FMath::Min(E.CanMake, 50);
		if (CanInfuse(E) && !Element.IsNone()) N = FMath::Min(N, Have(EssenceFor(Element)));
		return N;
	}

	/** The life skill this levels (and that sets gear quality): FWRCrafting::Make's choice. */
	FName SkillFor(const FEntry& E) const
	{
		const FWRItemDef* Out = FWRItemRegistry::Get().Find(E.Kind == FEntry::EKind::Recipe ? E.Recipe->Output : E.Icon);
		if (E.Kind != FEntry::EKind::Recipe) return E.Slot == EWREquipSlot::Armor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"));
		if (Out && Out->IsEquipment() && Out->StackSize <= 1) return Out->Slot == EWREquipSlot::Armor ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"));
		if (E.Station == TEXT("alchemy_table")) return TEXT("alchemy");
		if (Out && Out->Hunger > 0.f) return TEXT("cooking");
		return E.Station == TEXT("armorer") ? FName(TEXT("armorcraft")) : FName(TEXT("smithing"));
	}
	int32 SkillLevel(FName Skill) const { const UWRLifeSkillsComponent* S = Skills(); return S ? S->GetLevel(Skill) : 1; }

	/** Crude / common / fine / masterwork chances: FWRCraftingRules::RollQuality plus the Master perks' floor. */
	TArray<float> QualityOdds(const FEntry& E) const
	{
		const FWRRecipeRegistry& Reg = FWRRecipeRegistry::Get();
		TArray<float> Odds = { Reg.BaseOdds[0], Reg.BaseOdds[1], Reg.BaseOdds[2], Reg.BaseOdds[3] };
		const int32 T = E.bHere ? Tier() : 1;
		const FName Skill = SkillFor(E);
		const float Shift = FMath::Clamp((T - 1) * Reg.PerHearthLevel + SkillLevel(Skill) * Reg.PerSkillLevel, 0.f, 0.5f);
		const float FromCrude = FMath::Min(Odds[0], Shift);
		Odds[0] -= FromCrude;
		Odds[2] += FromCrude * 0.7f;
		Odds[3] += FromCrude * 0.3f;
		if (UWRLifeSkillsComponent::StatFor(PC.Get(), Skill == TEXT("armorcraft") ? TEXT("armorMinFine") : TEXT("weaponMinFine")) > 0.f)
		{
			Odds[2] += Odds[0] + Odds[1];
			Odds[0] = Odds[1] = 0.f;
		}
		const float Sum = Odds[0] + Odds[1] + Odds[2] + Odds[3];
		for (float& O : Odds) O /= FMath::Max(0.001f, Sum);
		return Odds;
	}

	// ---------------------------------------------------------------- building the tab

	void RebuildAll()
	{
		GatherEntries();
		// Keep the pick if it's still listed. Else suggest something this station makes that you can make now and don't
		// carry yet, else anything you can make, else anything made here, else the first card.
		const TArray<const FEntry*> Shown = Visible();
		if (!Shown.ContainsByPredicate([this](const FEntry* E) { return E->Key == SelectedKey; }))
		{
			const UWRInventoryComponent* I = Inv();
			const auto Carried = [I](FName Item)
			{
				if (!I) return false;
				for (const EWREquipSlot S : { EWREquipSlot::Weapon, EWREquipSlot::Armor, EWREquipSlot::Ranged }) if (I->GetEquipped(S).ItemId == Item) return true;
				return I->CountOf(Item) > 0;
			};
			const FName Here = StationType();
			const FEntry* const* New = Shown.FindByPredicate([&](const FEntry* E) { return E->Ready() && E->Recipe && E->Station == Here && !Carried(E->Recipe->Output); });
			const FEntry* const* Ready = Shown.FindByPredicate([](const FEntry* E) { return E->Ready(); });
			const FEntry* const* AnyHere = Shown.FindByPredicate([](const FEntry* E) { return !E->Locked(); });
			SelectedKey = New ? (*New)->Key : Ready ? (*Ready)->Key : AnyHere ? (*AnyHere)->Key : Shown.Num() > 0 ? Shown[0]->Key : FString();
		}
		if (const FEntry* E = Selected())
		{
			if (!CanInfuse(*E) || (!Element.IsNone() && Have(EssenceFor(Element)) < 1)) Element = NAME_None;
			Quantity = FMath::Clamp(Quantity, 1, FMath::Max(1, MaxQuantity(*E)));
		}
		TopRow->SetContent(BuildTopRow());
		BuildNav();
		NavFoot->SetContent(BuildNavFoot());
		BuildList();
		Details->SetContent(BuildDetails());
	}

	/** The cards that pass the category, search and ready filters, in the order the list shows them. */
	TArray<const FEntry*> Visible() const
	{
		TArray<const FEntry*> Out;
		for (const FName Cat : SectionOrder())
		{
			for (const FEntry* E : InSection(Cat)) Out.Add(E);
		}
		return Out;
	}

	TArray<const FEntry*> InSection(FName Cat) const
	{
		TArray<const FEntry*> Out;
		for (const FEntry& E : Entries) if (E.Category == Cat && Matches(E) && !E.Locked()) Out.Add(&E);
		for (const FEntry& E : Entries) if (E.Category == Cat && Matches(E) && E.Locked()) Out.Add(&E);   // made elsewhere, last
		return Out;
	}

	/** Categories shown: the picked one, or all; ones with something to make here come first. */
	TArray<FName> SectionOrder() const
	{
		TArray<FName> Here, Elsewhere;
		for (const FCategory& C : Categories())
		{
			if (!Category.IsNone() && C.Id != Category) continue;
			bool bAny = false, bAnyHere = false;
			for (const FEntry& E : Entries)
			{
				if (E.Category != C.Id || !Matches(E)) continue;
				bAny = true;
				bAnyHere |= !E.Locked();
			}
			if (bAny) (bAnyHere ? Here : Elsewhere).Add(C.Id);
		}
		Here.Append(Elsewhere);
		return Here;
	}

	TSharedRef<SWidget> BuildTopRow()
	{
		const FName Type = StationType();
		const int32 T = Tier();
		FText Title = Station.IsValid() ? FText::Format(LOCTEXT("StationTier", "{0} · Tier {1}"), StationName(Type), T) : StationName(Hand);
		FText Sub;
		FLinearColor SubColor = WRUI::Ink2;
		if (!Station.IsValid())
		{
			Sub = LOCTEXT("HandSub", "Crude tools, spears, bolas and arrows, anywhere. Stand at a station for the rest.");
		}
		else if (const AWRHearth* Hearth = AWRHearth::FindCovering(Station->GetWorld(), Station->GetActorLocation()))
		{
			if (Hearth->IsBroken())
			{
				Sub = LOCTEXT("HearthBroken", "The Hearth is broken: repair it to craft here.");
				SubColor = WRUI::Bad;
			}
			else if (T >= 5) Sub = LOCTEXT("TopTier", "Top tier: everything this station makes is open.");
			else
			{
				static const int32 CoresFor[] = { 0, 0, 1, 2, 4, 6 };
				const int32 Need = FMath::Max(1, CoresFor[T + 1] - Hearth->GetCores().Num());
				Sub = FText::Format(LOCTEXT("NextTier", "Tier {0} unlocks at Hearth tech level {0}: feed it {1} more boss {1}|plural(one=core,other=cores)."), T + 1, Need);
			}
		}
		else
		{
			Sub = LOCTEXT("NoHearth", "Tier 2 needs a Hearth in your land claim, fed a boss core.");
		}

		TSharedRef<SHorizontalBox> Chips = SNew(SHorizontalBox);
		Chips->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f * U, 0.f)[WRUI::Text(LOCTEXT("Stations", "Stations"), WRUI::Caps(12), WRUI::Ink2)];
		for (const FName St : Stations)
		{
			const bool bCurrent = St == Type;
			FText State;
			FLinearColor StateColor = WRUI::Ink3;
			bool bNear = bCurrent;
			if (St == Hand) { State = bCurrent ? LOCTEXT("Here", "here") : LOCTEXT("Always", "always"); StateColor = bCurrent ? WRUI::Good : WRUI::Ink3; bNear = true; }
			else if (bCurrent) { State = LOCTEXT("Here", "here"); StateColor = WRUI::Good; }
			else
			{
				const float D = NearestStation(St);
				if (D >= 0.f) { State = FText::Format(LOCTEXT("MAway", "{0} m away"), FMath::Max(1, FMath::RoundToInt(D / 100.f))); StateColor = WRUI::Ink2; bNear = true; }
				else State = LOCTEXT("NotNearby", "not nearby");
			}
			Chips->AddSlot().AutoWidth().Padding(4.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(bCurrent ? WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.16f), 6.f, WRUI::Brass, 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.38f), 6.f, bNear ? WRUI::Rim : WRUI::Alpha(WRUI::Rim, 0.6f), 1.f))
				.Padding(FMargin(6.f * U, 5.f * U, 12.f * U, 5.f * U))
				.RenderOpacity(bNear ? 1.f : 0.5f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(bCurrent ? FLinearColor(FColor(0x3A, 0x28, 0x12)) : FLinearColor(0.f, 0.f, 0.f, 0.45f), 15.f * U, bCurrent ? WRUI::Brass : WRUI::Brass2, 1.f))]
							+ SOverlay::Slot().Padding(5.f * U)[SNew(SImage).Image(WRUI::Icon(StationIcon(St)))]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(StationName(St), WRUI::Body(13, 800), bNear ? WRUI::Ink : WRUI::Ink2)]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(State, WRUI::Body(11, bCurrent ? 700 : 500), StateColor)]
					]
				]
			];
		}

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 20.f * U, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Title, WRUI::Title(22), WRUI::Ink, true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f * U, 0.f, 0.f)[WRUI::Text(Sub, WRUI::Body(12, 500), SubColor, false, true)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Chips];
	}

	/** Distance to the nearest station of a type within 80 m (-1: none). */
	float NearestStation(FName Type) const
	{
		const APawn* Me = PC.IsValid() ? PC->GetPawn() : nullptr;
		if (!Me) return -1.f;
		float Best = 8000.f;
		bool bFound = false;
		for (TActorIterator<AWRStation> It(Me->GetWorld()); It; ++It)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation());
			if (It->StationType == Type && D < Best) { Best = D; bFound = true; }
		}
		return bFound ? Best : -1.f;
	}

	void BuildNav()
	{
		Nav->ClearChildren();
		const auto Count = [this](FName Cat) { int32 N = 0; for (const FEntry& E : Entries) N += (Cat.IsNone() || E.Category == Cat) && Matches(E); return N; };
		const auto Exists = [this](FName Cat) { return Entries.ContainsByPredicate([Cat](const FEntry& E) { return E.Category == Cat; }); };
		Nav->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 5.f * U)[NavButton(NAME_None, LOCTEXT("CatAll", "All"), NAME_None, Count(NAME_None))];
		for (const FCategory& C : Categories())
		{
			if (!Exists(C.Id)) continue;
			Nav->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 5.f * U)[NavButton(C.Id, C.Name, C.Icon, Count(C.Id))];
		}
	}

	TSharedRef<SWidget> NavButton(FName Cat, const FText& Name, FName Icon, int32 Count)
	{
		const bool bOn = Category == Cat;
		TSharedRef<SWidget> Glyph = Icon.IsNone()
			? StaticCastSharedRef<SWidget>(SNew(SGlyph).Kind(SGlyph::EKind::Grid).Size(16.f * U).Color(bOn ? WRUI::Ink : WRUI::Ink2))
			: StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(20.f * U).HeightOverride(20.f * U)[SNew(SImage).Image(WRUI::Icon(Icon))]);
		TSharedPtr<SButton> Button;
		SAssignNew(Button, SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
			.OnClicked_Lambda([this, Cat]() { Category = Cat; bDirty = true; return FReply::Handled(); })
		[
			SNew(SBox).HeightOverride(40.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Image(TEXT("leather"))).ColorAndOpacity(bOn ? FLinearColor::White : FLinearColor(0.72f, 0.72f, 0.72f))]
				+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor::Transparent, 4.f, bOn ? WRUI::Brass : WRUI::Brass2, bOn ? 1.5f : 1.f))]
				+ SOverlay::Slot().Padding(FMargin(12.f * U, 0.f, 12.f * U, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)[Glyph]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(Name, WRUI::Title(13), bOn ? WRUI::Ink : WRUI::Ink2, true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::AsNumber(Count), WRUI::Body(12, 800), bOn ? WRUI::Brass : WRUI::Ink3)]
				]
			]
		];
		// The picked category gets an ember marker at its left edge.
		return SNew(SOverlay)
			+ SOverlay::Slot()[Button.ToSharedRef()]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(FMargin(-7.f * U, 9.f * U, 0.f, 9.f * U))
			[
				SNew(SBox).WidthOverride(4.f * U).Visibility(bOn ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				[
					SNew(SImage).Image(WRUI::Box(WRUI::Brass, 2.f))
				]
			];
	}

	TSharedRef<SWidget> BuildNavFoot()
	{
		const FEntry* E = Selected();
		const FName SkillId = E ? SkillFor(*E) : FName(TEXT("smithing"));
		const FWRLifeSkillDef* Def = FWRLifeSkillRegistry::Get().Find(SkillId);
		const UWRLifeSkillsComponent* S = Skills();
		const FWRSkillProgress* P = S ? S->FindProgress(SkillId) : nullptr;
		const int32 Level = P ? P->Level : 1;
		const float Frac = P ? float(P->XP) / FMath::Max(1, FWRLifeSkillRules::XPToNext(Level)) : 0.f;
		FString Blurb;
		if (Def)
		{
			Blurb = Def->EveryLevelText + TEXT(".");
			if (!Def->LevelsBy.IsEmpty()) Blurb += FString::Printf(TEXT(" %s earns XP."), *Def->LevelsBy);
		}
		int32 Recipes = 0, Ready = 0;
		for (const FEntry& X : Entries)
		{
			if (X.Kind != FEntry::EKind::Recipe || !Matches(X)) continue;
			++Recipes;
			Ready += X.Ready();
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SWRPanel).Kind(EWRPanel::Inset).Padding(FMargin(12.f * U))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(Def ? Def->Name : SkillId.ToString()), WRUI::Caps(12), WRUI::Ink2)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::AsNumber(Level), WRUI::Body(15, 800), WRUI::Ink)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U)[SNew(SWRBar).Height(6.f * U).Color(WRUI::Brass).Fraction(Frac)]
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(Blurb), WRUI::Body(11, 500), WRUI::Ink3, false, true)]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f * U, 0.f, 0.f)[WRUI::Divider()]
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 10.f * U, 0.f, 0.f)
			[
				WRUI::Text(FText::Format(LOCTEXT("NRecipes", "{0} {0}|plural(one=recipe,other=recipes)"), Recipes), WRUI::Body(14, 800), WRUI::Ink)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4.f * U, 2.f * U, 0.f, 0.f)
			[
				WRUI::Text(FText::Format(LOCTEXT("NReady", "{0} ready to craft"), Ready), WRUI::Body(12, 500), Ready > 0 ? WRUI::Good : WRUI::Ink3)
			];
	}

	TSharedRef<SWidget> SearchRow()
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)[WRUI::Text(LOCTEXT("Search", "Search"), WRUI::Caps(12), WRUI::Ink2)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(38.f * U)
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.42f), 4.f, FLinearColor::Black, 1.f)).Padding(FMargin(12.f * U, 0.f)).VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f * U, 0.f)[SNew(SGlyph).Kind(SGlyph::EKind::Search).Size(16.f * U).Color(WRUI::Ink3)]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SEditableText)
							.HintText(LOCTEXT("SearchHint", "Recipe or material"))
							.Font(WRUI::Body(13, 500))
							.ColorAndOpacity(WRUI::Ink)
							.OnTextChanged_Lambda([this](const FText& T) { Search = T.ToString().TrimStartAndEnd(); bDirty = true; })
						]
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
				.OnClicked_Lambda([this]() { bReadyOnly = !bReadyOnly; bDirty = true; return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f * U, 0.f)
					[
						SNew(SBox).WidthOverride(18.f * U).HeightOverride(18.f * U)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.5f), 3.f, WRUI::Brass2, 1.f))]
							+ SOverlay::Slot().Padding(1.f)[SNew(SGlyph).Kind(SGlyph::EKind::Check).Size(16.f * U).Color(WRUI::Brass).Visibility_Lambda([this]() { return bReadyOnly ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("ReadyOnly", "Ready to craft only"), WRUI::Body(13, 500), WRUI::Ink2)]
				]
			];
	}

	TSharedRef<SWidget> FooterHints()
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 7.f * U, 0.f)[SNew(SWRKeyCap).FontSize(11).Text(WRUI::KeyName(EKeys::LeftMouseButton))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f * U, 0.f)[WRUI::Text(LOCTEXT("SelectRecipe", "Select a recipe"), WRUI::Body(12, 500), WRUI::Ink3)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("RepairWhere", "Repair at the station that makes the item"), WRUI::Body(12, 500), WRUI::Ink3)];
	}

	// ---------------------------------------------------------------- cards

	void BuildList()
	{
		List->ClearChildren();
		// Sections of three or more cards get full rows; smaller ones share a row side by side (as the mockup's
		// Food · Taming · Repair line), each under its own heading.
		TArray<TPair<FName, TArray<const FEntry*>>> Small;
		int32 SmallCards = 0;
		bool bFirst = true;
		const auto Heading = [this](FName Cat, const TArray<const FEntry*>& Items)
		{
			const bool bPicked = Items.ContainsByPredicate([this](const FEntry* E) { return E->Key == SelectedKey; });
			const FCategory* C = Categories().FindByPredicate([Cat](const FCategory& X) { return X.Id == Cat; });
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[WRUI::Text(C ? C->Name : FText::GetEmpty(), WRUI::Caps(12), bPicked ? WRUI::Brass : WRUI::Ink2)]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(WRUI::Alpha(WRUI::Brass2, bPicked ? 0.9f : 0.5f))]];
		};
		const auto FlushSmall = [&]()
		{
			if (Small.Num() == 0) return;
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			for (int32 i = 0; i < Small.Num(); ++i)
			{
				TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
				for (int32 c = 0; c < Small[i].Value.Num(); ++c)
				{
					Cards->AddSlot().FillWidth(1.f).Padding(c == 0 ? 0.f : 12.f * U, 0.f, 0.f, 0.f)[Card(*Small[i].Value[c])];
				}
				Row->AddSlot().FillWidth(Small[i].Value.Num()).Padding(i == 0 ? 0.f : 12.f * U, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f * U)[Heading(Small[i].Key, Small[i].Value)]
					+ SVerticalBox::Slot().AutoHeight()[Cards]
				];
			}
			if (SmallCards < 3) Row->AddSlot().FillWidth(3 - SmallCards).Padding(12.f * U, 0.f, 0.f, 0.f)[SNullWidget::NullWidget];
			List->AddSlot().Padding(0.f, bFirst ? 0.f : 18.f * U, 0.f, 0.f)[Row];
			bFirst = false;
			Small.Reset();
			SmallCards = 0;
		};
		for (const FName Cat : SectionOrder())
		{
			const TArray<const FEntry*> Items = InSection(Cat);
			if (Items.Num() == 0) continue;
			if (Items.Num() < 3)
			{
				if (SmallCards + Items.Num() > 3) FlushSmall();
				Small.Add({ Cat, Items });
				SmallCards += Items.Num();
				continue;
			}
			FlushSmall();
			List->AddSlot().Padding(0.f, bFirst ? 0.f : 18.f * U, 0.f, 8.f * U)[Heading(Cat, Items)];
			bFirst = false;
			for (int32 i = 0; i < Items.Num(); i += 3)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				for (int32 c = 0; c < 3; ++c)
				{
					Row->AddSlot().FillWidth(1.f).Padding(c == 0 ? 0.f : 12.f * U, 0.f, 0.f, 0.f)[Items.IsValidIndex(i + c) ? Card(*Items[i + c]) : SNullWidget::NullWidget];
				}
				List->AddSlot().Padding(0.f, i == 0 ? 0.f : 12.f * U, 0.f, 0.f)[Row];
			}
		}
		FlushSmall();
		if (Entries.Num() > 0 && Visible().Num() == 0)
		{
			List->AddSlot().Padding(0.f, 40.f * U)[SNew(SBox).HAlign(HAlign_Center)[WRUI::Text(bReadyOnly ? LOCTEXT("NothingReady", "Nothing you can make right now matches.") : LOCTEXT("NothingMatches", "No recipe matches."), WRUI::Title(16), WRUI::Ink3)]];
		}
	}

	TSharedRef<SWidget> Card(const FEntry& E)
	{
		const FString Key = E.Key;
		const bool bLocked = E.Locked();
		const bool bPicked = Key == SelectedKey;
		FWRSlotView View;
		View.ItemId = E.Icon;
		View.Count = E.OutCount;
		View.bLocked = bLocked;
		if (E.Kind == FEntry::EKind::Repair && E.MaxDurability > 0.f) View.Durability = E.Durability / E.MaxDurability;

		// Status mark and the line under the ingredients.
		TSharedRef<SWidget> Mark = SNullWidget::NullWidget;
		FString Note;
		FLinearColor NoteColor = WRUI::Ink2;
		if (bLocked)
		{
			Mark = SNew(SGlyph).Kind(SGlyph::EKind::Lock).Size(18.f * U).Color(WRUI::Ink2);
			Note = E.Blocker;
		}
		else if (!E.Blocker.IsEmpty())
		{
			Note = E.Blocker;
			NoteColor = WRUI::Ink3;
		}
		else if (E.CanMake > 0)
		{
			Mark = SNew(SGlyph).Kind(SGlyph::EKind::Ok).Size(18.f * U);
			Note = E.Kind == FEntry::EKind::Recipe ? FString::Printf(TEXT("Can make %d"), E.CanMake) : E.Kind == FEntry::EKind::Repair ? TEXT("Ready to repair") : TEXT("Ready to upgrade");
			NoteColor = WRUI::Good;
		}
		else
		{
			Mark = SNew(SGlyph).Kind(SGlyph::EKind::No).Size(18.f * U);
			TArray<FString> Short;
			for (const TPair<FName, int32>& C : E.Cost)
			{
				if (Have(C.Key) < C.Value) Short.Add(FString::Printf(TEXT("%d/%d %s"), Have(C.Key), C.Value, *ItemName(C.Key)));
			}
			Note = FString::Join(Short, TEXT(", "));
			NoteColor = WRUI::Bad;
		}
		if (E.Kind == FEntry::EKind::Repair && E.MaxDurability > 0.f && Note.IsEmpty()) Note = FString::Printf(TEXT("%.0f/%.0f"), E.Durability, E.MaxDurability);

		TSharedRef<SHorizontalBox> Ingredients = SNew(SHorizontalBox);
		for (int32 i = 0; i < E.Cost.Num() && i < 3; ++i)
		{
			const TPair<FName, int32>& C = E.Cost[i];
			const bool bShort = !bLocked && Have(C.Key) < C.Value;
			Ingredients->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(i == 0 ? 0.f : 9.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 3.f * U, 0.f)[SNew(SBox).WidthOverride(22.f * U).HeightOverride(22.f * U)[SNew(SImage).Image(WRUI::Icon(C.Key))]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::AsNumber(C.Value), WRUI::Body(12, 800), bShort ? WRUI::Bad : WRUI::Ink)]
			];
		}

		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(E.Name), WRUI::Body(14, 800), WRUI::Ink, false, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(6.f * U, 0.f, 0.f, 0.f)[Mark]
			];
		if (E.Cost.Num() > 0) Body->AddSlot().AutoHeight().Padding(0.f, 7.f * U, 0.f, 0.f)[Ingredients];
		if (E.Kind == FEntry::EKind::Repair && E.MaxDurability > 0.f)
		{
			const float F = E.Durability / E.MaxDurability;
			Body->AddSlot().AutoHeight().Padding(0.f, 7.f * U, 0.f, 0.f)[SNew(SWRBar).Height(6.f * U).Color(F < 0.25f ? WRUI::Bad : F < 0.5f ? WRUI::Warn : WRUI::Good).Fraction(F)];
		}
		Body->AddSlot().AutoHeight().Padding(0.f, 7.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Note), WRUI::Body(11, 700), NoteColor, false, true)];

		TSharedPtr<SButton> Button;
		SAssignNew(Button, SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
			.OnClicked_Lambda([this, Key]() { if (SelectedKey != Key) { SelectedKey = Key; Quantity = 1; bDirty = true; } return FReply::Handled(); })
		[
			SNew(SBox).MinDesiredHeight(116.f * U)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage).Image(bPicked ? WRUI::Box(FLinearColor(FColor(0x2E, 0x26, 0x1A, 0xE0)), 5.f, WRUI::Brass, 1.5f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.3f), 5.f, FLinearColor(0.f, 0.f, 0.f, 0.85f), 1.f))
				]
				+ SOverlay::Slot().VAlign(VAlign_Top).Padding(1.f)[SNew(SBox).HeightOverride(1.f)[SNew(SImage).Image(WRUI::White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bPicked ? 0.08f : 0.05f))]]
				+ SOverlay::Slot().Padding(FMargin(12.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f * U, 0.f)
					[
						SNew(SBox).Visibility(EVisibility::HitTestInvisible)[SNew(SWRSlot).Size(60.f * U).View(View)]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Body]
				]
			]
		];
		// The picked card glows: a soft brass ring just outside it. Hover lifts the others a little.
		TWeakPtr<SButton> WeakButton = Button;
		return SNew(SOverlay).RenderOpacity(bLocked ? 0.6f : 1.f)
			+ SOverlay::Slot().Padding(-3.f)[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image(bPicked ? WRUI::Box(FLinearColor::Transparent, 8.f, WRUI::Alpha(WRUI::Ember, 0.35f), 2.f) : WRUI::Box(FLinearColor::Transparent, 8.f))]
			+ SOverlay::Slot()[Button.ToSharedRef()]
			+ SOverlay::Slot()[SNew(SImage).Visibility(EVisibility::HitTestInvisible).Image_Lambda([WeakButton, bPicked]()
			{
				const TSharedPtr<SButton> B = WeakButton.Pin();
				return B && B->IsHovered() && !bPicked ? WRUI::Box(FLinearColor(1.f, 0.85f, 0.6f, 0.05f), 5.f, WRUI::Brass2, 1.f) : WRUI::Box(FLinearColor::Transparent, 5.f);
			})];
	}

	// ---------------------------------------------------------------- details

	TSharedRef<SWidget> BuildDetails()
	{
		const FEntry* E = Selected();
		if (!E)
		{
			return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("PickRecipe", "Pick a recipe"), WRUI::Title(16), WRUI::Ink3)];
		}
		const FString Key = E->Key;
		const FWRItemDef* Out = FWRItemRegistry::Get().Find(E->Icon);
		FWRSlotView Big;
		Big.ItemId = E->Icon;
		Big.Count = E->OutCount;
		Big.bLocked = E->Locked();

		// Header: the output, its kind and station, what it does.
		FText Label;
		FString Line;
		if (E->Kind == FEntry::EKind::Recipe)
		{
			Label = FText::Format(LOCTEXT("KindAt", "{0} · {1}"), KindLabel(Out), StationName(E->Station));
			Line = StatLine(Out);
		}
		else if (E->Kind == FEntry::EKind::Repair)
		{
			Label = FText::Format(LOCTEXT("RepairAt", "Repair · {0}"), StationName(E->Station));
			Line = FString::Printf(TEXT("Back to full durability (%.0f / %.0f now) for %d%% of what it cost to make."), E->Durability, E->MaxDurability, FMath::RoundToInt(FWRRecipeRegistry::Get().RepairCostFraction * 100.f));
		}
		else
		{
			Label = FText::Format(LOCTEXT("UpgradeAt", "Upgrade · {0}"), StationName(E->Station));
			Line = FString::Printf(TEXT("+%d to +%d: %.0f%% more damage or armor per step. The cap is +%d at this tier (2 per Hearth level)."), E->Upgrade, E->Upgrade + 1,
				WRTune(TEXT("weapon.upgradePerLevel"), 0.05f) * 100.f, E->UpgradeCap);
		}

		TSharedRef<SVerticalBox> Middle = SNew(SVerticalBox);
		// Ingredients / cost, have / need.
		Middle->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[WRUI::Text(E->Kind == FEntry::EKind::Recipe ? LOCTEXT("Ingredients", "Ingredients") : LOCTEXT("Cost", "Cost"), WRUI::Caps(12), WRUI::Ink2)]
			+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(LOCTEXT("HaveNeed", "have / need"), WRUI::Body(11, 500), WRUI::Ink3)]
		];
		if (E->Cost.Num() == 0)
		{
			Middle->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[WRUI::Text(LOCTEXT("Free", "Nothing"), WRUI::Body(13, 500), WRUI::Ink3)];
		}
		for (const TPair<FName, int32>& C : E->Cost)
		{
			const int32 H = Have(C.Key);
			FWRSlotView Small;
			Small.ItemId = C.Key;
			Middle->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)
			[
				SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.28f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f)).Padding(FMargin(3.f * U, 3.f * U, 10.f * U, 3.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SWRSlot).Size(30.f * U).View(Small)]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(FText::FromString(ItemName(C.Key)), WRUI::Body(13, 700), WRUI::Ink)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[WRUI::Text(FText::FromString(FString::Printf(TEXT("%d / %d"), H, C.Value)), WRUI::Body(14, 800), H >= C.Value ? WRUI::Good : WRUI::Bad)]
				]
			];
		}
		if (E->Kind == FEntry::EKind::Recipe && E->Recipe->bQuality)
		{
			// Quality odds for gear: the station's tier and your skill.
			const TArray<float> Odds = QualityOdds(*E);
			const FName SkillId = SkillFor(*E);
			const FWRLifeSkillDef* SkillDef = FWRLifeSkillRegistry::Get().Find(SkillId);
			static const FText QNames[4] = { LOCTEXT("QCrude", "Crude"), LOCTEXT("QCommon", "Common"), LOCTEXT("QFine", "Fine"), LOCTEXT("QMaster", "Masterwork") };
			const FLinearColor QColors[4] = { FLinearColor(FColor(0xB4, 0x93, 0x7A)), FLinearColor(FColor(0xC9, 0xCC, 0xD0)), WRUI::Good, FLinearColor(FColor(0x8D, 0xB4, 0xEA)) };
			TSharedRef<SHorizontalBox> Legend = SNew(SHorizontalBox);
			for (int32 i = 0; i < 4; ++i)
			{
				Legend->AddSlot().FillWidth(1.f).HAlign(i == 0 ? HAlign_Left : i == 3 ? HAlign_Right : HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f * U, 0.f)[WRUI::Text(QNames[i], WRUI::Body(12, 800), QColors[i])]
					+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Odds[i] * 100.f))), WRUI::Body(12, 500), WRUI::Ink2)]
				];
			}
			Middle->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 6.f * U)
			[
				WRUI::Text(FText::Format(LOCTEXT("QualityOdds", "Quality odds ({0} {1}{2})"), FText::FromString(SkillDef ? SkillDef->Name : SkillId.ToString()), SkillLevel(SkillId),
					E->bHere ? FText::GetEmpty() : LOCTEXT("AtTier1", ", tier 1")), WRUI::Caps(12), WRUI::Ink2)
			];
			Middle->AddSlot().AutoHeight()[SNew(SOddsBar).Odds(Odds)];
			Middle->AddSlot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)[Legend];
		}
		if (CanInfuse(*E))
		{
			Middle->AddSlot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 5.f * U)[WRUI::Text(LOCTEXT("Infuse", "Infuse"), WRUI::Caps(12), WRUI::Ink2)];
			for (const FName El : { FName(), FName(TEXT("fire")), FName(TEXT("frost")) })
			{
				Middle->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f * U)[InfuseRow(*E, El)];
			}
		}

		const bool bRecipe = E->Kind == FEntry::EKind::Recipe;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f * U)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 14.f * U, 0.f)
				[
					SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(FColor(0x2C, 0x20, 0x12)), 6.f, WRUI::Brass2, 2.f)).Padding(4.f * U)
					[
						SNew(SWRSlot).Size(72.f * U).View(Big)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(FText::FromString(E->Name), WRUI::Title(18), WRUI::Ink, true, true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(Label, WRUI::Caps(11), WRUI::Ink2)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f * U, 0.f, 0.f)[WRUI::Text(FText::FromString(Line), WRUI::Body(12, 500), WRUI::Ink2, false, true)]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox).ScrollBarThickness(FVector2D(4.f, 4.f))
				+ SScrollBox::Slot()[Middle]
			]
			// The server's answer to the last craft (the journal covers the HUD's notice line).
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f * U, 0.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { return NoticeText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				[
					WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(NoticeText()); }), WRUI::Body(12, 700), TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(NoticeColor()); }), false, true)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f * U, 0.f, 0.f)
			[
				SNew(SBox).Visibility(bRecipe && !E->Locked() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)[QuantityRow(Key)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f * U, 0.f, 0.f)
			[
				// A fixed height: the button's leather tile would otherwise ask for its texture's 256.
				SNew(SBox).HeightOverride(52.f * U)
				[
					SNew(SWRButton).Primary(true).FontSize(14).MinHeight(52.f * U)
					.Text_Lambda([this, Key]() { return ActionLabel(Key); })
					.IsEnabled_Lambda([this, Key]() { const FEntry* X = Find(Key); return X && MaxQuantity(*X) > 0; })
					.OnClicked_Lambda([this, Key]() { DoAction(Key); return FReply::Handled(); })
				]
			];
	}

	const FEntry* Find(const FString& Key) const { return Entries.FindByPredicate([&Key](const FEntry& E) { return E.Key == Key; }); }

	TSharedRef<SWidget> InfuseRow(const FEntry& E, FName El)
	{
		const FName Essence = EssenceFor(El);
		const int32 H = Essence.IsNone() ? 0 : Have(Essence);
		const bool bUsable = Essence.IsNone() || H >= 1;
		const FWRItemDef* Out = FWRItemRegistry::Get().Find(E.Recipe->Output);
		const bool bArmor = Out && Out->Slot == EWREquipSlot::Armor;
		const bool bRanged = Out && Out->Slot == EWREquipSlot::Ranged;
		FText Name, Need, Effect;
		FLinearColor NameColor = WRUI::Ink;
		if (El.IsNone())
		{
			Name = LOCTEXT("InfNone", "None");
			Effect = LOCTEXT("InfNoneFx", "No essence used");
		}
		else
		{
			const bool bFire = El == TEXT("fire");
			Name = bFire ? LOCTEXT("InfFire", "Fire") : LOCTEXT("InfFrost", "Frost");
			NameColor = bFire ? WRUI::Ember : WRUI::Frost;
			Need = FText::Format(LOCTEXT("InfNeed", "· needs 1 {0}"), FText::FromString(ItemName(Essence)));
			// What it does in the game today: melee hits build the element, armor resists lava / ice; shots ignore it.
			Effect = bRanged ? LOCTEXT("InfRangedFx", "Shots don't carry it yet")
				: bArmor ? (bFire ? LOCTEXT("InfArmorFire", "Walk on lava unharmed") : LOCTEXT("InfArmorFrost", "Sure footing on ice"))
				: (bFire ? LOCTEXT("InfWeaponFire", "Hits build up burning") : LOCTEXT("InfWeaponFrost", "Hits build up frostbite"));
			if (!bUsable) Effect = FText::Format(LOCTEXT("InfYouHave", "{0} · you have 0"), Effect);
		}
		TSharedRef<SWidget> Icon = El.IsNone()
			? StaticCastSharedRef<SWidget>(SNew(SGlyph).Kind(SGlyph::EKind::NoneRing).Size(22.f * U).Color(WRUI::Ink2))
			: StaticCastSharedRef<SWidget>(SNew(SImage).Image(WRUI::Icon(Essence)));
		return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(bUsable ? EMouseCursor::Hand : EMouseCursor::Default)
			.IsEnabled(bUsable)
			.OnClicked_Lambda([this, El]() { Element = El; return FReply::Handled(); })
			[
				SNew(SBorder).RenderOpacity(bUsable ? 1.f : 0.5f)
				.BorderImage_Lambda([this, El]() { return Element == El ? WRUI::Box(WRUI::Alpha(WRUI::Brass, 0.12f), 4.f, WRUI::Brass, 1.f) : WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.28f), 4.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f); })
				.Padding(FMargin(10.f * U, 4.f * U))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)
					[
						SNew(SGlyph).Size(18.f * U).Color(bUsable ? WRUI::Brass2 : WRUI::Rim).Kind_Lambda([this, El]() { return Element == El ? SGlyph::EKind::RadioOn : SGlyph::EKind::Radio; })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f * U, 0.f)[SNew(SBox).WidthOverride(30.f * U).HeightOverride(30.f * U).HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[WRUI::Text(Name, WRUI::Body(13, 800), NameColor)]
							+ SHorizontalBox::Slot().AutoWidth().Padding(5.f * U, 0.f, 0.f, 0.f).VAlign(VAlign_Bottom)[WRUI::Text(Need, WRUI::Body(12, 500), WRUI::Ink3)]
						]
						+ SVerticalBox::Slot().AutoHeight()[WRUI::Text(Effect, WRUI::Body(12, 500), El.IsNone() ? WRUI::Ink3 : WRUI::Ink2, false, true)]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f * U, 0.f, 0.f, 0.f)
					[
						WRUI::Text(El.IsNone() ? FText::GetEmpty() : FText::FromString(FString::Printf(TEXT("%d/1"), H)), WRUI::Body(13, 800), bUsable ? WRUI::Good : WRUI::Bad)
					]
				]
			];
	}

	TSharedRef<SWidget> StepButton(bool bMore)
	{
		return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0.f).Cursor(EMouseCursor::Hand)
			.IsEnabled_Lambda([this, bMore]() { const FEntry* E = Selected(); return E && (bMore ? Quantity < MaxQuantity(*E) : Quantity > 1); })
			.OnClicked_Lambda([this, bMore]() { const FEntry* E = Selected(); if (E) Quantity = FMath::Clamp(Quantity + (bMore ? 1 : -1), 1, FMath::Max(1, MaxQuantity(*E))); return FReply::Handled(); })
			[
				SNew(SBox).WidthOverride(36.f * U).HeightOverride(34.f * U).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					WRUI::Text(FText::FromString(bMore ? TEXT("+") : TEXT("-")), WRUI::Body(18, 800), TAttribute<FSlateColor>::CreateLambda([this, bMore]()
					{
						const FEntry* E = Selected();
						const bool bOn = E && (bMore ? Quantity < MaxQuantity(*E) : Quantity > 1);
						return FSlateColor(bOn ? WRUI::Ink : WRUI::Alpha(WRUI::Ink, 0.35f));
					}))
				]
			];
	}

	TSharedRef<SWidget> QuantityRow(const FString& Key)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[WRUI::Text(LOCTEXT("Quantity", "Quantity"), WRUI::Caps(12), WRUI::Ink2)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(0.f, 0.f, 0.f, 0.42f), 4.f, FLinearColor::Black, 1.f)).Padding(0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(FColor(0x2F, 0x33, 0x38)), 3.f)).Padding(0.f)[StepButton(false)]]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(46.f * U).HAlign(HAlign_Center)
						[
							WRUI::Text(TAttribute<FText>::CreateLambda([this]() { return FText::AsNumber(Quantity); }), WRUI::Body(15, 800), WRUI::Ink)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBorder).BorderImage(WRUI::Box(FLinearColor(FColor(0x2F, 0x33, 0x38)), 3.f)).Padding(0.f)[StepButton(true)]]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f * U, 0.f, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(34.f * U)
				[
					SNew(SWRButton).Ghost(true).FontSize(11).MinHeight(34.f * U)
					.Text_Lambda([this, Key]() { const FEntry* E = Find(Key); return FText::Format(LOCTEXT("MaxN", "Max {0}"), E ? FMath::Max(0, MaxQuantity(*E)) : 0); })
					.IsEnabled_Lambda([this, Key]() { const FEntry* E = Find(Key); return E && MaxQuantity(*E) > 1; })
					.OnClicked_Lambda([this, Key]() { const FEntry* E = Find(Key); if (E) Quantity = FMath::Max(1, MaxQuantity(*E)); return FReply::Handled(); })
				]
			];
	}

	FText ActionLabel(const FString& Key) const
	{
		const FEntry* E = Find(Key);
		if (!E) return FText::GetEmpty();
		if (!E->bHere) return FText::Format(LOCTEXT("MadeAt", "Made at the {0}"), FText::FromString(StationName(E->Station).ToString().ToLower()));
		if (!E->bTierOk) return FText::Format(LOCTEXT("NeedsTier", "Needs tier {0}"), E->Level);
		if (E->Kind == FEntry::EKind::Repair) return FText::Format(LOCTEXT("RepairX", "Repair {0}"), FText::FromString(ItemName(E->Icon)));
		if (E->Kind == FEntry::EKind::Upgrade) return FText::Format(LOCTEXT("UpgradeX", "Upgrade to +{0}"), E->Upgrade + 1);
		const int32 N = FMath::Clamp(Quantity, 1, FMath::Max(1, MaxQuantity(*E)));
		return N > 1 ? FText::Format(LOCTEXT("CraftN", "Craft {0} ({1})"), FText::FromString(E->Name), N) : FText::Format(LOCTEXT("CraftX", "Craft {0}"), FText::FromString(E->Name));
	}

	void DoAction(const FString& Key)
	{
		const FEntry* E = Find(Key);
		if (!E || !PC.IsValid() || MaxQuantity(*E) <= 0) return;
		if (E->Kind != FEntry::EKind::Recipe)
		{
			PC->SendCommand(E->Command);
			return;
		}
		// No repeat count on the server: N crafts are N commands, each checked there on its own.
		const FString Infuse = CanInfuse(*E) && !Element.IsNone() ? Element.ToString() : TEXT("none");
		const int32 N = FMath::Clamp(Quantity, 1, MaxQuantity(*E));
		for (int32 i = 0; i < N; ++i) PC->SendCommand(FString::Printf(TEXT("%s %s"), *E->Command, *Infuse));
		Quantity = 1;
	}

	FString NoticeText() const
	{
		if (!PC.IsValid() || PC->GetNotice().IsEmpty() || PC->GetNotice().StartsWith(TEXT("+")) || PC->GetWorld()->GetTimeSeconds() - PC->GetNoticeTime() > 5.0) return FString();
		FString N = PC->GetNotice();
		N[0] = FChar::ToUpper(N[0]);
		return N;
	}
	FLinearColor NoticeColor() const
	{
		const FString N = NoticeText();
		return N.StartsWith(TEXT("Crafted")) || N.StartsWith(TEXT("Repaired")) || N.StartsWith(TEXT("Upgraded")) ? WRUI::Good : WRUI::Warn;
	}
};

	WR_REGISTER_JOURNAL_TAB("crafting", [](AWRPlayerController* PC) -> TSharedRef<SWidget> { return SNew(SWRCraftingTab).PC(PC); });
}

#undef LOCTEXT_NAMESPACE
