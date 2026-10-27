// AWRPlayerController: interaction, crafting and inventory menus, and the gameplay commands they send.
// Menus are client-side state drawn by AWRHUD; every action is a server command validated there.

#include "WRPlayerController.h"
#include "WRSound.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRLifeSkills.h"
#include "WRAbilities.h"
#include "WRCharacter.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRSocial.h"
#include "WRInventory.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRPlayerUI.h"
#include "EngineUtils.h"

template <typename T>
static T* FindNearby(const UWorld* World, const APawn* Pawn, float Range)
{
	if (!Pawn)
	{
		return nullptr;
	}
	T* Best = nullptr;
	float BestDist = Range;
	for (TActorIterator<T> It(World); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

AWRStation* AWRPlayerController::FindNearbyStation() const { return FindNearby<AWRStation>(GetWorld(), GetPawn(), AWRStation::UseRange); }
AWRHearth* AWRPlayerController::FindNearbyHearth() const { return FindNearby<AWRHearth>(GetWorld(), GetPawn(), AWRStation::UseRange); }

AWRCreature* AWRPlayerController::FindNearbyCreature(bool bOwnTame, float Range) const
{
	const APawn* Me = GetPawn();
	const FString Who = PlayerState ? PlayerState->GetPlayerName() : FString();
	AWRCreature* Best = nullptr;
	float BestDist = Range;
	for (TActorIterator<AWRCreature> It(GetWorld()); Me && It; ++It)
	{
		if (*It == Me || !It->IsAlive() || It->IsA<AWRTrainingDummy>() || (It->TamedBy == Who) != bOwnTame || (!bOwnTame && It->IsTamed()))
		{
			continue;
		}
		const float D = FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) - It->GetSimpleCollisionRadius();
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

void AWRPlayerController::WRCraft(FName Recipe, FName Element) { ServerCommand(FString::Printf(TEXT("craft %s %s"), *Recipe.ToString(), Element.IsNone() ? TEXT("none") : *Element.ToString())); }
void AWRPlayerController::WRUseSlot(int32 Slot) { ServerCommand(FString::Printf(TEXT("slot %d"), Slot)); }
void AWRPlayerController::WRRepair(FName Slot) { ServerCommand(FString::Printf(TEXT("repair %s"), *Slot.ToString())); }
void AWRPlayerController::WRUpgrade(FName Slot) { ServerCommand(FString::Printf(TEXT("upgrade %s"), *Slot.ToString())); }
void AWRPlayerController::WRUnequip(FName Slot) { ServerCommand(FString::Printf(TEXT("unequip %s"), *Slot.ToString())); }
void AWRPlayerController::WREquip(FName Item) { ServerCommand(FString::Printf(TEXT("equip %s"), *Item.ToString())); }
void AWRPlayerController::WRGoTo(FName What, FName Extra) { ServerCommand(FString::Printf(TEXT("goto %s %s"), *What.ToString(), Extra.IsNone() ? TEXT("") : *Extra.ToString())); }
void AWRPlayerController::WRTame(FName Order) { ServerCommand(FString::Printf(TEXT("tame %s"), *Order.ToString())); }
void AWRPlayerController::WRTameAll(FName Order) { ServerCommand(FString::Printf(TEXT("tameall %s"), *Order.ToString())); }
void AWRPlayerController::WRDismount() { ServerCommand(TEXT("dismount")); }
void AWRPlayerController::WRTorpor(float Amount) { ServerCommand(FString::Printf(TEXT("torpor %.0f"), Amount)); }
void AWRPlayerController::WRTameStat() { ServerCommand(TEXT("tamestat")); }
void AWRPlayerController::WRTameGive(FName Item) { ServerCommand(FString::Printf(TEXT("tamegive %s"), *Item.ToString())); }
void AWRPlayerController::WRNarcotic() { ServerCommand(TEXT("tamenarcotic")); }
void AWRPlayerController::WRCmd(const FString& Command) { ServerCommand(Command); }
void AWRPlayerController::WRBossDmg(float Amount, FName Element) { ServerCommand(FString::Printf(TEXT("bossdmg %.0f %s"), Amount, Element.IsNone() ? TEXT("none") : *Element.ToString())); }
void AWRPlayerController::WRBossDmgPct(float Percent, FName Element) { ServerCommand(FString::Printf(TEXT("bossdmg %.1f%% %s"), Percent, Element.IsNone() ? TEXT("none") : *Element.ToString())); }
void AWRPlayerController::WRClan(const FString& Action, const FString& Name) { ServerCommand(FString::Printf(TEXT("clan %s %s"), *Action, *Name)); }
void AWRPlayerController::WRPerk(FName Skill, int32 TierLevel, int32 Option) { ServerCommand(FString::Printf(TEXT("perk %s %d %d"), *Skill.ToString(), TierLevel, Option)); }
void AWRPlayerController::WRSkill(FName Skill, int32 Level) { ServerCommand(FString::Printf(TEXT("skill %s %d"), *Skill.ToString(), Level)); }
void AWRPlayerController::WRSkillXP(FName Skill, int32 XP) { ServerCommand(FString::Printf(TEXT("skillxp %s %d"), *Skill.ToString(), XP)); }
void AWRPlayerController::WRHitPlayer(const FString& Name, float Damage) { ServerCommand(FString::Printf(TEXT("hitplayer %s %.0f"), *Name, Damage)); }
void AWRPlayerController::WRHazard() { ServerCommand(TEXT("hazard")); }
void AWRPlayerController::WRSay(const FString& Message) { ServerChat(Message); }
void AWRPlayerController::WRMap() { ToggleMap(); }

void AWRPlayerController::ToggleMap()
{
	// The new UI's journal has a map tab; the old HUD draws a list of places.
	if (UI && FWRScreens::Find(TEXT("journal")))
	{
		OpenJournal(EWRMenu::Map);
		return;
	}
	bShowMap = !bShowMap;
}
void AWRPlayerController::WRLearn(FName Path, FName Node) { ServerCommand(FString::Printf(TEXT("learn %s %s"), *Path.ToString(), *Node.ToString())); }
void AWRPlayerController::WRCast(int32 Slot)
{
	if (AWRCharacter* Survivor = Cast<AWRCharacter>(GetPawn()))
	{
		const EWRAbilityInput Input = static_cast<EWRAbilityInput>(static_cast<int32>(EWRAbilityInput::Slot1) + FMath::Clamp(Slot, 1, 8) - 1);
		Survivor->PressAbilityInput(Input);
		Survivor->ReleaseAbilityInput(Input);
	}
}
void AWRPlayerController::WRSkillStat(FName Stat) { ServerCommand(FString::Printf(TEXT("skillstat %s"), *Stat.ToString())); }

void AWRPlayerController::TogglePaths()
{
	Menu = Menu == EWRMenu::Paths ? EWRMenu::None : EWRMenu::Paths;
	MenuIndex = 0;
}

void AWRPlayerController::ToggleSkills()
{
	Menu = Menu == EWRMenu::Skills ? EWRMenu::None : EWRMenu::Skills;
	MenuIndex = 0;
}

void AWRPlayerController::WRArtifacts(int32 bOn) { ServerCommand(FString::Printf(TEXT("artifacts %d"), bOn)); }

void AWRPlayerController::WRMenu(FName Which, int32 Select)
{
	if (Which == TEXT("confirm"))
	{
		MenuConfirm();
		return;
	}
	if (Which == TEXT("map"))
	{
		ToggleMap();
		return;
	}
	MenuClose();
	if (Which == TEXT("inventory")) ToggleInventory();
	else if (Which == TEXT("build")) ToggleBuildMenu();
	else if (Which == TEXT("skills")) ToggleSkills();
	else if (Which == TEXT("paths")) TogglePaths();
	else if (Which == TEXT("crafting")) OpenJournal(EWRMenu::Craft);
	else if (Which == TEXT("settings")) OpenJournal(EWRMenu::Settings);
	else if (Which == TEXT("hearth")) OpenJournal(EWRMenu::Hearth);
	MenuIndex = FMath::Clamp(Select, 0, FMath::Max(0, BuildMenuEntries().Num() - 1));
}

void AWRPlayerController::WRInteract()
{
	if (Menu != EWRMenu::None)
	{
		MenuClose();
		return;
	}
	AWRCreature* Tame = FindNearbyCreature(true);
	AWRCreature* Wild = FindNearbyCreature(false, 400.f);
	const auto DistTo = [this](const AActor* A) { return FVector::Dist2D(A->GetActorLocation(), GetPawn()->GetActorLocation()); };
	if (Tame && (!Wild || DistTo(Tame) < DistTo(Wild)))
	{
		Menu = EWRMenu::Tame;
		MenuCreature = Tame;
		MenuIndex = 0;
		return;
	}
	const FWRCreatureDef* WildDef = Wild ? Wild->GetDef() : nullptr;
	if (WildDef && Wild->IsUnconscious() && WildDef->CanTameBy(TEXT("knockout_feed")))
	{
		Menu = EWRMenu::Taming;
		MenuCreature = Wild;
		MenuIndex = 0;
		return;
	}
	// The nearer of a door / chest / bed / crop / claim and a station, like the prompt (a workbench in a small hut
	// mustn't steal the door).
	AActor* Use = FindUseTarget();
	if (Wild && Use && DistTo(Wild) < DistTo(Use))
	{
		Use = nullptr;  // the creature in front of you (feed it), not the bench behind it
	}
	if (AWRBuildPiece* Piece = Cast<AWRBuildPiece>(Use))
	{
		const FWRPieceDef* Def = Piece->GetDef();
		if (Def && (Def->Kind == TEXT("chest") || Def->Kind == TEXT("totem")))
		{
			Menu = Def->Kind == TEXT("chest") ? EWRMenu::Chest : EWRMenu::Claim;
			MenuPiece = Piece;
			MenuIndex = 0;
			return;
		}
	}
	else if (AWRStation* Station = Cast<AWRStation>(Use))
	{
		Menu = EWRMenu::Craft;
		MenuStation = Station;
		MenuIndex = 0;
		return;
	}
	// The Hearth has its own screen in the new UI (feeding it cores is a button there). Its own piece is the usable
	// piece nearest to it, so only another one (a door, a chest) closer than that keeps E on that one.
	const AWRBuildPiece* NearPiece = FindNearbyPiece(350.f, true);
	const FWRPieceDef* NearDef = NearPiece ? NearPiece->GetDef() : nullptr;
	if (UI && FWRScreens::Find(TEXT("hearth")) && FindNearbyHearth() && (!NearPiece || (NearDef && NearDef->Kind == TEXT("hearth"))))
	{
		Menu = EWRMenu::Hearth;
		MenuIndex = 0;
		return;
	}
	ServerCommand(TEXT("interact"));
}

AActor* AWRPlayerController::FindUseTarget() const
{
	const APawn* Me = GetPawn();
	AWRBuildPiece* Piece = FindNearbyPiece(350.f, true);
	AWRStation* Station = FindNearbyStation();
	if (!Me || !Piece || !Station)
	{
		return Piece ? static_cast<AActor*>(Piece) : static_cast<AActor*>(Station);
	}
	const float ToPiece = FVector::Dist2D(Piece->GetActorLocation(), Me->GetActorLocation());
	return ToPiece <= FVector::Dist2D(Station->GetActorLocation(), Me->GetActorLocation()) ? static_cast<AActor*>(Piece) : static_cast<AActor*>(Station);
}

void AWRPlayerController::ToggleInventory()
{
	Menu = Menu == EWRMenu::Inventory ? EWRMenu::None : EWRMenu::Inventory;
	MenuIndex = 0;
}

void AWRPlayerController::MenuClose()
{
	Menu = EWRMenu::None;
	MenuStation = nullptr;
}

void AWRPlayerController::MenuUp()
{
	if (Menu != EWRMenu::None)
	{
		const int32 N = FMath::Max(1, BuildMenuEntries().Num());
		MenuIndex = (MenuIndex + N - 1) % N;
	}
}

void AWRPlayerController::MenuDown()
{
	if (Menu != EWRMenu::None)
	{
		MenuIndex = (MenuIndex + 1) % FMath::Max(1, BuildMenuEntries().Num());
	}
}

void AWRPlayerController::CycleInfuse()
{
	if (Menu == EWRMenu::Craft)
	{
		MenuElement = MenuElement.IsNone() ? FName(TEXT("fire")) : MenuElement == TEXT("fire") ? FName(TEXT("frost")) : NAME_None;
	}
}

void AWRPlayerController::MenuConfirmKey()
{
	// With Crafting open, Enter also ran the hidden text menu's line under it and crafted the station's first recipe.
	// Bots confirm through WRMenu confirm, which still goes straight to MenuConfirm.
	if (UI.IsValid() && UI->IsScreenOpen())
	{
		return;
	}
	MenuConfirm();
}

void AWRPlayerController::MenuConfirm()
{
	const TArray<FWRMenuEntry> Entries = BuildMenuEntries();
	if (Menu != EWRMenu::None && Entries.IsValidIndex(MenuIndex) && !Entries[MenuIndex].Command.IsEmpty())
	{
		const FString& Command = Entries[MenuIndex].Command;
		if (Command.StartsWith(TEXT("build ")))
		{
			StartPlacing(FName(*Command.RightChop(6)));
		}
		else
		{
			ServerCommand(Command);
			if (Menu == EWRMenu::Tame)
			{
				MenuClose();
			}
		}
	}
}

FString AWRPlayerController::GetMenuTitle() const
{
	if (Menu == EWRMenu::Inventory)
	{
		return TEXT("Inventory  (Up/Down, Enter = use / equip / socket, I = close)");
	}
	if (Menu == EWRMenu::Build)
	{
		return TEXT("Build & craft  (Enter = craft a tool / pick a piece, then LMB place, R rotate, RMB/B cancel)");
	}
	if (Menu == EWRMenu::Chest)
	{
		return TEXT("Chest  (Enter = take / store, E = close)");
	}
	if (Menu == EWRMenu::Claim)
	{
		return TEXT("Land claim  (E = close)");
	}
	if (Menu == EWRMenu::Paths)
	{
		return TEXT("Paths  (Up/Down, Enter = learn a node, P = close; new abilities fill your 1-8 slots)");
	}
	if (Menu == EWRMenu::Skills)
	{
		return TEXT("Life skills  (Up/Down, Enter = choose a perk when one is waiting, K = close)");
	}
	if (Menu == EWRMenu::Tame)
	{
		const AWRCreature* Tame = MenuCreature.Get();
		return FString::Printf(TEXT("%s  (Enter = order, E = close)"), Tame ? *Tame->GetDisplayLabel() : TEXT("Tame"));
	}
	if (Menu == EWRMenu::Taming)
	{
		const AWRCreature* Wild = MenuCreature.Get();
		return FString::Printf(TEXT("Taming %s  (Enter = give, E = close)"), Wild ? *Wild->GetDisplayLabel() : TEXT(""));
	}
	if (const AWRStation* Station = MenuStation.Get())
	{
		return FString::Printf(TEXT("%s  (Up/Down, Enter = craft, F = infuse: %s, E = close)"), *Station->GetDisplayName(),
			MenuElement.IsNone() ? TEXT("none") : *MenuElement.ToString());
	}
	return FString();
}

TArray<FWRMenuEntry> AWRPlayerController::BuildMenuEntries() const
{
	TArray<FWRMenuEntry> Out;
	const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	const UWRInventoryComponent* Inv = PS ? PS->GetInventory() : nullptr;
	if (!Inv)
	{
		return Out;
	}

	const auto GearLabel = [](const FWRItemStack& S)
	{
		const FWRItemDef* Def = S.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(S.ItemId);
		if (!Def) return FString(TEXT("-"));
		return FString::Printf(TEXT("%s %s%s  dur %.0f/%.0f%s%s"), FWRItemStack::QualityName(S.Quality), *Def->Name,
			S.Upgrade > 0 ? *FString::Printf(TEXT(" +%d"), S.Upgrade) : TEXT(""), S.Durability, Def->MaxDurability,
			S.Element.IsNone() ? TEXT("") : *FString::Printf(TEXT("  [%s]"), *S.Element.ToString()),
			S.Sockets.Num() > 0 ? *FString::Printf(TEXT("  gems %d/%d"), S.Sockets.Num(), FWRItemStack::SocketCount(S.Quality)) : TEXT(""));
	};

	if (Menu == EWRMenu::Inventory)
	{
		Out.Add({ FString::Printf(TEXT("Weapon: %s"), *GearLabel(Inv->GetEquipped(EWREquipSlot::Weapon))), TEXT("unequip weapon") });
		Out.Add({ FString::Printf(TEXT("Armor:  %s"), *GearLabel(Inv->GetEquipped(EWREquipSlot::Armor))), TEXT("unequip armor") });
		Out.Add({ FString::Printf(TEXT("Ranged: %s"), *GearLabel(Inv->GetEquipped(EWREquipSlot::Ranged))), TEXT("unequip ranged") });
		const TArray<FWRItemStack>& Slots = Inv->GetSlots();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].IsEmpty())
			{
				continue;
			}
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(Slots[i].ItemId);
			const FString Label = Def && Def->IsEquipment() ? GearLabel(Slots[i])
				: FString::Printf(TEXT("%s x%d"), Def ? *Def->Name : *Slots[i].ItemId.ToString(), Slots[i].Count);
			Out.Add({ Label, FString::Printf(TEXT("slot %d"), i) });
		}
		return Out;
	}

	if (Menu == EWRMenu::Build)
	{
		return BuildPieceMenu();
	}
	if (Menu == EWRMenu::Paths)
	{
		const UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
		if (!Paths)
		{
			return Out;
		}
		Out.Add({ FString::Printf(TEXT("Character level %d"), Paths->GetCharacterLevel()), FString() });
		for (const FName PathId : { Paths->GetMainPath(), Paths->GetSubPath() })
		{
			const FWRPathDef* Def = PathId.IsNone() ? nullptr : FWRPathRegistry::Get().Find(PathId);
			if (!Def)
			{
				continue;
			}
			const FWRPathProgress* Progress = Paths->GetProgress(PathId);
			const int32 Earned = Paths->GetEarned(PathId), Spent = Paths->GetSpent(PathId);
			Out.Add({ FString::Printf(TEXT("-- %s (%s): %d points to spend (%d earned) --"), *Def->Name, PathId == Paths->GetMainPath() ? TEXT("main") : TEXT("subclass"),
				Earned - Spent, Earned), FString() });
			FName Branch = NAME_None;
			for (const FWRPathNodeDef& Node : Def->Nodes)
			{
				if (Node.Branch != Branch)
				{
					Branch = Node.Branch;
					Out.Add({ FString::Printf(TEXT("  %s branch"), *Branch.ToString().Replace(TEXT("_"), TEXT(" "))), FString() });
				}
				const int32 Rank = Progress ? Progress->GetRank(Node.Id) : 0;
				const bool bAbility = Node.Kind == EWRNodeKind::Ability;
				const bool bBuilt = !bAbility || UWRAbilityRegistry::IsBuilt(PathId, Node.Id);
				FWRMenuEntry E;
				E.Label = FString::Printf(TEXT("    [%d/%d] %s  (%d pt%s%s)  %s%s"), Rank, Node.MaxRank, *Node.Name, Node.Cost, Node.Cost == 1 ? TEXT("") : TEXT("s"),
					Node.Gate > 0 ? *FString::Printf(TEXT(", needs %d in branch"), Node.Gate) : TEXT(""), *Node.Text, bBuilt ? TEXT("") : TEXT("  [not built yet]"));
				E.Command = Rank < Node.MaxRank ? FString::Printf(TEXT("learn %s %s"), *PathId.ToString(), *Node.Id.ToString()) : FString();
				E.bAvailable = Rank < Node.MaxRank && Earned - Spent >= Node.Cost;
				Out.Add(E);
			}
		}
		return Out;
	}
	if (Menu == EWRMenu::Skills)
	{
		const UWRLifeSkillsComponent* Skills = PS ? PS->GetLifeSkills() : nullptr;
		FString Group;
		for (const FWRLifeSkillDef& Def : FWRLifeSkillRegistry::Get().All())
		{
			if (Def.Group != Group)
			{
				Group = Def.Group;
				Out.Add({ FString::Printf(TEXT("-- %s --"), *Group), FString() });
			}
			const FWRSkillProgress* P = Skills ? Skills->FindProgress(Def.Id) : nullptr;
			const int32 Level = P ? P->Level : 1;
			Out.Add({ FString::Printf(TEXT("%s  %d   (%d/%d xp)   %s%s"), *Def.Name, Level, P ? P->XP : 0, FWRLifeSkillRules::XPToNext(Level),
				*Def.EveryLevelText, Def.EveryLevelStat.IsNone() ? TEXT(" [not active yet]") : TEXT("")), FString() });
			for (int32 T = 0; T < 4; ++T)
			{
				const int32 TierLevel = FWRLifeSkillRegistry::TierLevels[T];
				const int8 Chosen = P && P->Perks.IsValidIndex(T) ? P->Perks[T] : -1;
				if (Level < TierLevel || !Def.Tiers.IsValidIndex(T)) break;
				for (int32 O = 0; O < Def.Tiers[T].Num(); ++O)
				{
					const FWRPerkDef& Perk = Def.Tiers[T][O];
					if (Chosen >= 0 && Chosen != O) continue;
					FWRMenuEntry E;
					E.Label = FString::Printf(TEXT("    L%d %s %s: %s%s"), TierLevel, Chosen == O ? TEXT("[x]") : TEXT("choose"), *Perk.Name, *Perk.Text,
						Perk.Stat.IsNone() ? TEXT(" [not active yet]") : TEXT(""));
					E.Command = Chosen >= 0 ? FString() : FString::Printf(TEXT("perk %s %d %d"), *Def.Id.ToString(), TierLevel, O);
					Out.Add(E);
				}
			}
		}
		return Out;
	}
	if (Menu == EWRMenu::Tame)
	{
		const AWRCreature* Tame = MenuCreature.Get();
		const FWRCreatureDef* CDef = Tame ? Tame->GetDef() : nullptr;
		if (!CDef)
		{
			return Out;
		}
		Out.Add({ FString::Printf(TEXT("%s  -  %s%s"), *CDef->Role, Tame->TameOrder == EWRTameOrder::Follow ? TEXT("following") : TEXT("staying"),
			Tame->bTameAggressive ? TEXT(", aggressive") : TEXT(", passive")), FString() });
		Out.Add({ TEXT("Follow me"), TEXT("tame follow") });
		Out.Add({ TEXT("Stay here"), TEXT("tame stay") });
		Out.Add({ Tame->bTameAggressive ? TEXT("Set passive") : TEXT("Set aggressive"), Tame->bTameAggressive ? TEXT("tame passive") : TEXT("tame aggressive") });
		if (CDef->bMount)
		{
			Out.Add({ TEXT("Ride"), TEXT("tame ride") });
		}
		Out.Add({ TEXT("Release into the wild"), TEXT("tame release") });
		return Out;
	}
	if (Menu == EWRMenu::Taming)
	{
		const AWRCreature* Wild = MenuCreature.Get();
		const FWRCreatureDef* CDef = Wild ? Wild->GetDef() : nullptr;
		if (!CDef || Wild->IsTamed() || !Wild->IsUnconscious())
		{
			Out.Add({ Wild && Wild->IsTamed() ? TEXT("Tamed!") : TEXT("It's awake"), FString() });
			return Out;
		}
		const FWRItemDef* Next = FWRItemRegistry::Get().Find(Wild->TameFoodItem);
		Out.Add({ FString::Printf(TEXT("Torpor %.0f/%.0f   tamed %.0f%%   effectiveness %.0f%%   stash: %d food"), Wild->Torpor, Wild->MaxTorpor,
			Wild->TameProgress * 100.f, Wild->TameEffectiveness * 100.f, Wild->TameFoodCount), FString() });
		Out.Add({ Wild->TameEta >= 0.f ? FString::Printf(TEXT("Tamed in ~%.0f s if it stays under"), Wild->TameEta)
			: FString::Printf(TEXT("Needs %d more %s"), Wild->TameFoodNeeded, Next ? *Next->Name : TEXT("food")), FString() });
		// Kibble first (twice as fast, no effectiveness cost), then what it eats.
		TArray<FName> Foods = { FName(TEXT("kibble")) };
		Foods.Append(CDef->Food);
		for (const FName& Food : Foods)
		{
			const FWRItemDef* FDef = FWRItemRegistry::Get().Find(Food);
			const int32 Have = Inv->CountOf(Food);
			FWRMenuEntry E;
			E.Label = FString::Printf(TEXT("Give all %s (you have %d)%s"), FDef ? *FDef->Name : *Food.ToString(), Have,
				Food == TEXT("kibble") ? TEXT("  - tames x2, keeps effectiveness") : TEXT(""));
			E.Command = FString::Printf(TEXT("tamegive %s"), *Food.ToString());
			E.bAvailable = Have > 0;
			Out.Add(E);
		}
		FWRMenuEntry Narcotic;
		Narcotic.Label = FString::Printf(TEXT("Give a narcotic: +%.0f%% torpor (you have %d)"), WRTune(TEXT("taming.narcoticTorpor"), 0.3f) * 100.f, Inv->CountOf(TEXT("narcotic")));
		Narcotic.Command = TEXT("tamenarcotic");
		Narcotic.bAvailable = Inv->CountOf(TEXT("narcotic")) > 0;
		Out.Add(Narcotic);
		return Out;
	}
	if (Menu == EWRMenu::Chest || Menu == EWRMenu::Claim)
	{
		const AWRBuildPiece* Piece = MenuPiece.Get();
		if (!Piece)
		{
			return Out;
		}
		if (Menu == EWRMenu::Claim)
		{
			const AWRClaimTotem* Totem = Cast<AWRClaimTotem>(Piece->GetChild());
			Out.Add({ Totem ? Totem->Describe() : FString(TEXT("Land claim")), FString() });
			Out.Add({ TEXT("Stock all your wood and stone for upkeep"), TEXT("claimdeposit") });
			Out.Add({ TEXT("Upgrade radius +10 m"), TEXT("claimupgrade") });
			return Out;
		}
		Out.Add({ TEXT("-- In the chest --"), FString() });
		for (int32 i = 0; i < Piece->Storage.Num(); ++i)
		{
			const FWRItemStack& S = Piece->Storage[i];
			if (S.IsEmpty()) continue;
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(S.ItemId);
			Out.Add({ FString::Printf(TEXT("take  %s x%d"), Def ? *Def->Name : *S.ItemId.ToString(), S.Count), FString::Printf(TEXT("take %d"), i) });
		}
		Out.Add({ TEXT("-- Your items --"), FString() });
		const TArray<FWRItemStack>& Slots = Inv->GetSlots();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].IsEmpty()) continue;
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(Slots[i].ItemId);
			Out.Add({ FString::Printf(TEXT("store %s x%d"), Def ? *Def->Name : *Slots[i].ItemId.ToString(), Slots[i].Count), FString::Printf(TEXT("store %d"), i) });
		}
		return Out;
	}

	const AWRStation* Station = MenuStation.Get();
	if (!Station)
	{
		return Out;
	}
	for (const FWRRecipe* R : FWRRecipeRegistry::Get().ForStation(Station->StationType))
	{
		const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(R->Output);
		FString Needs;
		bool bHave = R->Level <= Station->GetTier();
		for (const TPair<FName, int32>& In : R->Inputs)
		{
			const FWRItemDef* InDef = FWRItemRegistry::Get().Find(In.Key);
			const int32 Have = Inv->CountOf(In.Key);
			bHave &= Have >= In.Value;
			Needs += FString::Printf(TEXT("%s %d/%d  "), InDef ? *InDef->Name : *In.Key.ToString(), Have, In.Value);
		}
		FWRMenuEntry E;
		E.Label = FString::Printf(TEXT("%s%s  <-  %s%s"), OutDef ? *OutDef->Name : *R->Output.ToString(),
			R->Count > 1 ? *FString::Printf(TEXT(" x%d"), R->Count) : TEXT(""), *Needs,
			R->Level > Station->GetTier() ? *FString::Printf(TEXT("(tier %d)"), R->Level) : TEXT(""));
		E.Command = FString::Printf(TEXT("craft %s %s"), *R->Id.ToString(), MenuElement.IsNone() ? TEXT("none") : *MenuElement.ToString());
		E.bAvailable = bHave;
		Out.Add(E);
	}
	if (Station->StationType == TEXT("forge"))
	{
		Out.Add({ TEXT("Repair weapon"), TEXT("repair weapon") });
		Out.Add({ TEXT("Upgrade weapon (upgrade shards)"), TEXT("upgrade weapon") });
	}
	else if (Station->StationType == TEXT("armorer"))
	{
		Out.Add({ TEXT("Repair armor"), TEXT("repair armor") });
		Out.Add({ TEXT("Upgrade armor (upgrade shards)"), TEXT("upgrade armor") });
	}
	// The equipped bow / crossbow / blowgun is mended where it's made.
	const FWRItemStack& Ranged = Inv->GetEquipped(EWREquipSlot::Ranged);
	const FWRItemDef* RangedDef = Ranged.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Ranged.ItemId);
	const FWRRecipe* RangedRecipe = RangedDef ? FWRRecipeRegistry::Get().ForOutput(RangedDef->Id) : nullptr;
	if (RangedRecipe && RangedDef->MaxDurability > 0.f && RangedRecipe->Station == Station->StationType)
	{
		Out.Add({ FString::Printf(TEXT("Repair %s (%.0f/%.0f)"), *RangedDef->Name, Ranged.Durability, RangedDef->MaxDurability), TEXT("repair ranged") });
	}
	return Out;
}

bool AWRPlayerController::RunGameplayCommand(const TArray<FString>& Args, FString& OutResult)
{
	AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
	if (!PS || Args.Num() == 0)
	{
		return false;
	}
	const FString& Cmd = Args[0];
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const auto SlotOf = [](const FString& S) { return S == TEXT("armor") ? EWREquipSlot::Armor : S == TEXT("ranged") ? EWREquipSlot::Ranged : EWREquipSlot::Weapon; };

	if (RunBuildCommand(Args, OutResult))
	{
		return true;
	}
	if (Cmd == TEXT("learn"))
	{
		const FString Why = PS->GetPaths()->BuyNode(FName(*Arg(1)), FName(*Arg(2)));
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(FName(*Arg(1)));
		const FWRPathNodeDef* Node = Def ? Def->FindNode(FName(*Arg(2))) : nullptr;
		OutResult = !Why.IsEmpty() ? Why : FString::Printf(TEXT("Learned %s"), Node ? *Node->Name : *Arg(2));
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("perk"))
	{
		OutResult = PS->GetLifeSkills()->ChoosePerk(FName(*Arg(1)), FCString::Atoi(*Arg(2)), FCString::Atoi(*Arg(3)));
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("clan"))
	{
		const FString Who = PS->GetPlayerName();
		FWRClans& Clans = FWRClans::Get();
		const FString& A = Arg(1);
		OutResult = A == TEXT("create") ? Clans.Create(Who, Arg(2)) : A == TEXT("invite") ? Clans.Invite(Who, Arg(2)) : A == TEXT("join") ? Clans.Join(Who, Arg(2))
			: A == TEXT("leave") ? Clans.Leave(Who) : A == TEXT("kick") ? Clans.Kick(Who, Arg(2)) : Clans.Describe(Who);
		Clans.RefreshPlayerStates(GetWorld());
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("dismount"))
	{
		if (AWRCreature* Mount = Cast<AWRCreature>(GetPawn()))
		{
			Mount->Dismount();
		}
		return true;
	}
	if (Cmd == TEXT("tamegive") || Cmd == TEXT("tamenarcotic"))
	{
		AWRCreature* Wild = FindNearbyCreature(false, 500.f);
		OutResult = !Wild ? FString(TEXT("no knocked-out creature nearby"))
			: Cmd == TEXT("tamenarcotic") ? Wild->GiveNarcotic(this) : Wild->GiveTameFood(this, FName(*Arg(1)));
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("hearthfeed"))
	{
		// The Hearth screen's "Feed a core": E on the Hearth opens that screen instead of sending "interact".
		AWRHearth* Hearth = FindNearbyHearth();
		OutResult = Hearth ? Hearth->DepositCores(this) : TEXT("no Hearth nearby");
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("tame"))
	{
		AWRCreature* Tame = FindNearbyCreature(true, 800.f);
		OutResult = Tame ? Tame->Order(this, Arg(1)) : TEXT("no tame of yours nearby");
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("tameall") && PS && GetPawn())
	{
		// A whistle: every tame of yours within 30 m takes the order.
		int32 Count = 0;
		for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && It->TamedBy == PS->GetPlayerName() && FVector::Dist(It->GetActorLocation(), GetPawn()->GetActorLocation()) < 3000.f)
			{
				It->Order(this, Arg(1));
				++Count;
			}
		}
		OutResult = FString::Printf(TEXT("%d tames: %s"), Count, *Arg(1));
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("interact"))
	{
		AWRBuildPiece* Piece = FindNearbyPiece(350.f, true);  // the door, not the wall beside it
		AWRNest* Nest = FindNearby<AWRNest>(GetWorld(), GetPawn(), 350.f);
		AWRLootBag* Bag = FindNearby<AWRLootBag>(GetWorld(), GetPawn(), 300.f);
		AWRCreature* Wild = FindNearbyCreature(false, 400.f);
		const FWRPieceDef* PieceDef = Piece ? Piece->GetDef() : nullptr;
		if ((Bag || Nest || Wild) && Cast<AWRCharacter>(GetPawn()))
		{
			Cast<AWRCharacter>(GetPawn())->MulticastBodyOneShot(TEXT("pickup"));
		}
		if (Bag || Nest)
		{
			WRSound::Broadcast(GetPawn(), TEXT("pickup"));
		}
		if (Bag)
		{
			OutResult = Bag->Loot(this);
		}
		else if (Nest)
		{
			OutResult = Nest->TakeEgg(this);
		}
		else if (Wild)
		{
			OutResult = Wild->Feed(this);
		}
		else if (PieceDef && PieceDef->Kind != TEXT("structure") && PieceDef->Kind != TEXT("decor") && PieceDef->Kind != TEXT("station"))
		{
			OutResult = Piece->Interact(this);
		}
		else if (AWRHearth* Hearth = FindNearbyHearth())
		{
			OutResult = Hearth->DepositCores(this);
		}
		else if (AWRBossAltar* Altar = FindNearbyAltar())
		{
			OutResult = Altar->Interact(this);
		}
		else
		{
			OutResult = TEXT("nothing to interact with");
			return true;
		}
	}
	else if (Cmd == TEXT("craft") || Cmd == TEXT("repair") || Cmd == TEXT("upgrade"))
	{
		AWRStation* Station = FindNearbyStation();
		if (!Station)
		{
			OutResult = TEXT("no station nearby");
		}
		else if (Cmd == TEXT("craft"))
		{
			OutResult = Station->Craft(this, FName(*Arg(1)), Arg(2) == TEXT("none") ? NAME_None : FName(*Arg(2)));
		}
		else if (Cmd == TEXT("repair"))
		{
			OutResult = Station->Repair(this, SlotOf(Arg(1)));
		}
		else
		{
			OutResult = Station->Upgrade(this, SlotOf(Arg(1)));
		}
	}
	else if (Cmd == TEXT("move") || Cmd == TEXT("split") || Cmd == TEXT("drop"))
	{
		// The inventory screen's drag and drop: move / swap / merge two bag slots, split a stack into the first
		// empty slot, drop (part of) a stack on the ground in a bag anyone can pick up.
		TArray<FWRItemStack>& Slots = PS->GetInventory()->GetSlotsMutable();
		const int32 A = FCString::Atoi(*Arg(1));
		if (!Slots.IsValidIndex(A) || Slots[A].IsEmpty())
		{
			OutResult = TEXT("empty slot");
		}
		else if (Cmd == TEXT("move"))
		{
			const int32 B = FCString::Atoi(*Arg(2));
			const FWRItemDef* Def = FWRItemRegistry::Get().Find(Slots[A].ItemId);
			if (!Slots.IsValidIndex(B) || A == B) OutResult = TEXT("");
			else if (Def && !Slots[B].IsEmpty() && Slots[B].ItemId == Slots[A].ItemId && Def->StackSize > 1 && !Def->IsEquipment())
			{
				const int32 Moved = FMath::Min(Slots[A].Count, Def->StackSize - Slots[B].Count);
				Slots[B].Count += Moved;
				Slots[A].Count -= Moved;
				if (Slots[A].Count <= 0) Slots[A] = FWRItemStack();
			}
			else
			{
				Swap(Slots[A], Slots[B]);
			}
			return true;   // silent: the screen shows the result
		}
		else if (Cmd == TEXT("split"))
		{
			const int32 N = FMath::Clamp(FCString::Atoi(*Arg(2)), 1, Slots[A].Count - 1);
			const int32 Free = Slots.IndexOfByPredicate([](const FWRItemStack& S) { return S.IsEmpty(); });
			if (Slots[A].Count < 2) OutResult = TEXT("nothing to split");
			else if (Free == INDEX_NONE) OutResult = TEXT("no empty slot");
			else
			{
				Slots[Free] = Slots[A];
				Slots[Free].Count = N;
				Slots[A].Count -= N;
				return true;
			}
		}
		else
		{
			APawn* Me = GetPawn();
			const int32 N = Arg(2).IsEmpty() ? Slots[A].Count : FMath::Clamp(FCString::Atoi(*Arg(2)), 1, Slots[A].Count);
			if (!Me) OutResult = TEXT("no body");
			else
			{
				FWRItemStack Dropped = Slots[A];
				Dropped.Count = N;
				Slots[A].Count -= N;
				if (Slots[A].Count <= 0) Slots[A] = FWRItemStack();
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const FVector At = Me->GetActorLocation() + Me->GetActorForwardVector() * 80.f - FVector(0.f, 0.f, Me->GetSimpleCollisionHalfHeight() - 25.f);
				if (AWRLootBag* Bag = GetWorld()->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), At, FRotator::ZeroRotator, Params))
				{
					Bag->OwnerName = PS->GetPlayerName();
					Bag->bOwnerOnly = false;
					Bag->Items = { Dropped };
					Bag->SetLifeSpan(WRTune(TEXT("inventory.dropMinutes"), 10.f) * 60.f);
				}
				const FWRItemDef* Def = FWRItemRegistry::Get().Find(Dropped.ItemId);
				OutResult = FString::Printf(TEXT("Dropped %s x%d"), Def ? *Def->Name : *Dropped.ItemId.ToString(), N);
			}
		}
	}
	else if (Cmd == TEXT("slot"))
	{
		OutResult = PS->GetInventory()->UseSlot(FCString::Atoi(*Arg(1)));
	}
	else if (Cmd == TEXT("unequip"))
	{
		OutResult = PS->GetInventory()->Unequip(SlotOf(Arg(1)));
	}
	else if (Cmd == TEXT("use"))
	{
		OutResult = PS->GetInventory()->UseCategory(FName(*Arg(1)));
	}
	else
	{
		return false;
	}
	ClientNotify(OutResult);
	return true;
}

void AWRPlayerController::WRWheel(int32 Which, int32 Hover)
{
	if (UI) UI->DebugWheel(Which, Hover);
}

void AWRPlayerController::WRHelp()
{
	if (UI) UI->ToggleHelp();
}

void AWRPlayerController::WRProbe()
{
	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}
	FCollisionQueryParams Query(TEXT("WRProbe"), true, P);
	const auto Report = [this, &Query](const TCHAR* What, const FVector& A, const FVector& B)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Query))
		{
			UE_LOG(LogWildreach, Log, TEXT("PROBE %s: nothing"), What);
			return;
		}
		UPrimitiveComponent* C = Hit.GetComponent();
		UE_LOG(LogWildreach, Log, TEXT("PROBE %s: %s (%s) component %s (%s) material %s at %s visible %d hidden-in-game %d"), What,
			*GetNameSafe(Hit.GetActor()), Hit.GetActor() ? *Hit.GetActor()->GetClass()->GetName() : TEXT("-"), *GetNameSafe(C),
			C ? *C->GetClass()->GetName() : TEXT("-"), C ? *GetNameSafe(C->GetMaterial(0)) : TEXT("-"), *Hit.ImpactPoint.ToString(),
			C ? C->IsVisible() : 0, C ? C->bHiddenInGame : 0);
	};
	const FVector Here = P->GetActorLocation();
	Report(TEXT("down"), Here, Here - FVector(0.f, 0.f, 5000.f));
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);
	Report(TEXT("view"), CamLoc, CamLoc + CamRot.Vector() * 20000.f);
	// Anything big and flat over this spot (a plane, a water or fog volume, a sea tile), drawn or not.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (UActorComponent* AC : It->GetComponents())
		{
			const UPrimitiveComponent* C = Cast<UPrimitiveComponent>(AC);
			if (!C || !C->IsRegistered())
			{
				continue;
			}
			const FBox B = C->Bounds.GetBox();
			if (B.GetSize().X > 4000.f && B.GetSize().Y > 4000.f && B.Min.X <= Here.X && B.Max.X >= Here.X && B.Min.Y <= Here.Y && B.Max.Y >= Here.Y)
			{
				UE_LOG(LogWildreach, Log, TEXT("PROBE over: %s (%s) component %s (%s) material %s z %.0f..%.0f visible %d"), *It->GetName(), *It->GetClass()->GetName(),
					*C->GetName(), *C->GetClass()->GetName(), *GetNameSafe(C->GetMaterial(0)), B.Min.Z, B.Max.Z, C->IsVisible());
			}
		}
	}
}

void AWRPlayerController::EscapePressed()
{
	if (IsPlacing())
	{
		CancelPlacement();
		return;
	}
	if (Menu != EWRMenu::None)
	{
		MenuClose();
		return;
	}
	if (UI && FWRScreens::Find(TEXT("settings")))
	{
		OpenJournal(EWRMenu::Settings);
	}
}
