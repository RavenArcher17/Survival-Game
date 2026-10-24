// AWRPlayerController: interaction, crafting and inventory menus, and the gameplay commands they send.
// Menus are client-side state drawn by AWRHUD; every action is a server command validated there.

#include "WRPlayerController.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRLifeSkills.h"
#include "WRSocial.h"
#include "WRInventory.h"
#include "WRPlayerState.h"
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
void AWRPlayerController::WRDismount() { ServerCommand(TEXT("dismount")); }
void AWRPlayerController::WRTorpor(float Amount) { ServerCommand(FString::Printf(TEXT("torpor %.0f"), Amount)); }
void AWRPlayerController::WRTameStat() { ServerCommand(TEXT("tamestat")); }
void AWRPlayerController::WRBossDmg(float Amount, FName Element) { ServerCommand(FString::Printf(TEXT("bossdmg %.0f %s"), Amount, Element.IsNone() ? TEXT("none") : *Element.ToString())); }
void AWRPlayerController::WRBossDmgPct(float Percent, FName Element) { ServerCommand(FString::Printf(TEXT("bossdmg %.1f%% %s"), Percent, Element.IsNone() ? TEXT("none") : *Element.ToString())); }
void AWRPlayerController::WRClan(const FString& Action, const FString& Name) { ServerCommand(FString::Printf(TEXT("clan %s %s"), *Action, *Name)); }
void AWRPlayerController::WRPerk(FName Skill, int32 TierLevel, int32 Option) { ServerCommand(FString::Printf(TEXT("perk %s %d %d"), *Skill.ToString(), TierLevel, Option)); }
void AWRPlayerController::WRSkill(FName Skill, int32 Level) { ServerCommand(FString::Printf(TEXT("skill %s %d"), *Skill.ToString(), Level)); }
void AWRPlayerController::WRSkillXP(FName Skill, int32 XP) { ServerCommand(FString::Printf(TEXT("skillxp %s %d"), *Skill.ToString(), XP)); }
void AWRPlayerController::WRHitPlayer(const FString& Name, float Damage) { ServerCommand(FString::Printf(TEXT("hitplayer %s %.0f"), *Name, Damage)); }
void AWRPlayerController::WRHazard() { ServerCommand(TEXT("hazard")); }
void AWRPlayerController::WRSkillStat(FName Stat) { ServerCommand(FString::Printf(TEXT("skillstat %s"), *Stat.ToString())); }

void AWRPlayerController::ToggleSkills()
{
	Menu = Menu == EWRMenu::Skills ? EWRMenu::None : EWRMenu::Skills;
	MenuIndex = 0;
}

void AWRPlayerController::WRArtifacts(int32 bOn) { ServerCommand(FString::Printf(TEXT("artifacts %d"), bOn)); }

void AWRPlayerController::WRInteract()
{
	if (Menu != EWRMenu::None)
	{
		MenuClose();
		return;
	}
	AWRCreature* Tame = FindNearbyCreature(true);
	const AWRCreature* Wild = FindNearbyCreature(false, 400.f);
	const auto DistTo = [this](const AActor* A) { return FVector::Dist2D(A->GetActorLocation(), GetPawn()->GetActorLocation()); };
	if (Tame && (!Wild || DistTo(Tame) < DistTo(Wild)))
	{
		Menu = EWRMenu::Tame;
		MenuCreature = Tame;
		MenuIndex = 0;
		return;
	}
	if (AWRBuildPiece* Piece = FindNearbyPiece())
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
	if (AWRStation* Station = FindNearbyStation())
	{
		Menu = EWRMenu::Craft;
		MenuStation = Station;
		MenuIndex = 0;
		return;
	}
	ServerCommand(TEXT("interact"));
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
		return TEXT("Build  (Enter = pick, then LMB place, R rotate, RMB/B cancel)");
	}
	if (Menu == EWRMenu::Chest)
	{
		return TEXT("Chest  (Enter = take / store, E = close)");
	}
	if (Menu == EWRMenu::Claim)
	{
		return TEXT("Land claim  (E = close)");
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
	const auto SlotOf = [](const FString& S) { return S == TEXT("armor") ? EWREquipSlot::Armor : EWREquipSlot::Weapon; };

	if (RunBuildCommand(Args, OutResult))
	{
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
	if (Cmd == TEXT("tame"))
	{
		AWRCreature* Tame = FindNearbyCreature(true, 800.f);
		OutResult = Tame ? Tame->Order(this, Arg(1)) : TEXT("no tame of yours nearby");
		ClientNotify(OutResult);
		return true;
	}
	if (Cmd == TEXT("interact"))
	{
		AWRBuildPiece* Piece = FindNearbyPiece();
		AWRNest* Nest = FindNearby<AWRNest>(GetWorld(), GetPawn(), 350.f);
		AWRLootBag* Bag = FindNearby<AWRLootBag>(GetWorld(), GetPawn(), 300.f);
		AWRCreature* Wild = FindNearbyCreature(false, 400.f);
		const FWRPieceDef* PieceDef = Piece ? Piece->GetDef() : nullptr;
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
