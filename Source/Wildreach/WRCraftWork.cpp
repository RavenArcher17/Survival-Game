// Hands-on crafting, the server side (cooks/cookpass-crafting.md 3.1, 3.3). Crafting by hand: each one of a batch takes
// its inputs as it starts (the bag, then your chests near you in your claim), holds State.Drinking (the item-use tag: no
// guard, roll or swing, a slow walk; creatures that read item use read it) for its seconds, and is made at the end if
// nothing (a stagger, a freeze, death) took the tag first, else its inputs come back. At a station: the first order's
// maker walks to its work spot (their own machine walks them, WRCraftWorkLook.cpp) and works it with their hands while
// they stay there, the station running x craft.work.handsOn.
#include "WRCraftWork.h"
#include "WRCharacter.h"
#include "WRCrafting.h"
#include "WRBuildLook.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace WRCraftWorkDetail
{
	AWRPlayerController* PCOf(const AWRCharacter* C) { return C ? Cast<AWRPlayerController>(C->GetController()) : nullptr; }

	/** Why the hands can't start work now (empty: they can). */
	FString Busy(const AWRCharacter* C)
	{
		if (!C || !C->IsAlive()) return TEXT("can't craft now");
		if (C->HasTag(WRTags::State_Mounted)) return TEXT("not while riding");
		if (C->GetCharacterMovement() && C->GetCharacterMovement()->IsSwimming()) return TEXT("not in the water");
		if (C->HasTag(WRTags::Status_Staggered) || C->HasTag(WRTags::Status_Frozen) || C->HasTag(WRTags::State_Dodging) || C->HasTag(WRTags::State_Attacking))
		{
			return TEXT("can't craft now");
		}
		if (C->HasTag(WRTags::State_Drinking)) return C->Work.IsHand() ? TEXT("already crafting") : TEXT("your hands are full");
		return FString();
	}

	FTransform PieceFrame(const AWRStation* S)
	{
		return FTransform(S->GetActorRotation(), S->GetActorLocation() - FVector(0.f, 0.f, FWRBuildLook::ChildLift));
	}

	FName ItemOf(const AWRStation* S)
	{
		const FWRRecipe* R = S && S->Queue.Num() > 0 ? FWRRecipeRegistry::Get().Find(S->Queue[0].Recipe) : nullptr;
		return R ? R->Output : NAME_None;
	}

	const TCHAR* SpotWord(FName Type)
	{
		return Type == TEXT("forge") || Type == TEXT("armorer") ? TEXT("the anvil") : Type == TEXT("workbench") ? TEXT("the bench") : TEXT("the table");
	}

	void Refund(AWRCharacter* C)
	{
		const AWRPlayerController* PC = PCOf(C);
		AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (PS && PS->GetInventory())
		{
			for (const TPair<FName, int32>& T : C->HandTaken)
			{
				const int32 Left = PS->GetInventory()->AddItem(T.Key, T.Value);
				if (Left > 0) UE_LOG(LogWildreach, Warning, TEXT("CRAFT %s: %d %s didn't fit back in the bag"), *PS->GetPlayerName(), Left, *T.Key.ToString());
			}
		}
		C->HandTaken.Reset();
	}

	void BeginHandItem(AWRCharacter* C);

	void FinishHandItem(TWeakObjectPtr<AWRCharacter> Weak, uint8 Serial)
	{
		AWRCharacter* C = Weak.Get();
		if (!C || C->Work.Serial != Serial || !C->Work.IsHand())
		{
			return;
		}
		AWRPlayerController* PC = PCOf(C);
		AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(C->HandRecipe);
		if (!PS || !R || !C->IsAlive() || !C->HasTag(WRTags::State_Drinking))
		{
			Refund(C);
			FWRCraftWork::End(C, C->IsAlive() ? TEXT("Interrupted: your materials are back") : FString());
			return;
		}
		UWRInventoryComponent* Inv = PS->GetInventory();
		const FWRCraftedOne One = FWRCrafting::MakeOne(*R, TEXT("hand"), 1, NAME_None, FWRCrafting::HandOf(PC, *R, TEXT("hand")));
		bool bFits = true;
		if (One.bGear)
		{
			bFits = Inv->AddInstance(One.Item);
		}
		else
		{
			TArray<FWRItemStack> Test = Inv->GetSlots();
			bFits = FWRInventoryRules::Add(Test, One.Item.ItemId, One.Item.Count) == 0;
			if (bFits) Inv->AddItem(One.Item.ItemId, One.Item.Count);
		}
		if (!bFits)
		{
			Refund(C);
			FWRCraftWork::End(C, TEXT("Your bag is full: your materials are back"));
			return;
		}
		C->HandTaken.Reset();
		UWRLifeSkillsComponent::Award(PC, One.Skill, One.XPKey, One.XP);
		const FWRItemDef* OutDef = FWRItemRegistry::Get().Find(R->Output);
		PC->ClientNotify(One.bGear ? One.Text : FString::Printf(TEXT("Crafted %d %s"), One.Item.Count, OutDef ? *OutDef->Name : *R->Output.ToString()));
		WRSound::Broadcast(C, TEXT("craft.done"));
		UE_LOG(LogWildreach, Log, TEXT("CRAFT %s by hand: %s (%d to go)"), *PS->GetPlayerName(), *R->Id.ToString(), C->HandLeft);
		C->Work.Made = static_cast<uint8>(FMath::Min(255, C->Work.Made + 1));
		if (C->HandLeft > 0)
		{
			BeginHandItem(C);
		}
		else
		{
			FWRCraftWork::End(C);
		}
	}

	void BeginHandItem(AWRCharacter* C)
	{
		AWRPlayerController* PC = PCOf(C);
		const FWRRecipe* R = FWRRecipeRegistry::Get().Find(C->HandRecipe);
		if (!PC || !R || C->HandLeft <= 0)
		{
			FWRCraftWork::End(C);
			return;
		}
		const FString Short = FWRCrafting::Shortfall(PC, *R, NAME_None, 1, C);
		if (!Short.IsEmpty())
		{
			FWRCraftWork::End(C, C->Work.Made > 0 ? FString::Printf(TEXT("Out of materials (%s)"), *Short) : Short);
			return;
		}
		C->HandTaken = FWRCrafting::InputsFor(*R, NAME_None, 1, FWRCrafting::MaterialDiscount(PC, *R));
		FWRCrafting::TakeInputs(PC, *R, NAME_None, 1, C);
		--C->HandLeft;
		const FWRTuning& T = FWRTuning::Get();
		const float Seconds = FMath::Max(T.Num(TEXT("craft.hand.minSeconds"), 0.8f), R->Seconds * T.Num(TEXT("craft.hand.timeScale"), 1.f));
		if (UAbilitySystemComponent* ASC = C->GetAbilitySystemComponent()) ASC->CancelAbilities();
		// Held a moment past the item's end so the finish still finds it on.
		C->Status->ApplyStatus(WRTags::State_Drinking, Seconds + 0.15f);
		C->Work.Kind = FWRCraftWork::HandKind(*R);
		C->Work.Item = R->Output;
		C->Work.At = nullptr;
		C->Work.Seconds = Seconds;
		C->Work.Serial = static_cast<uint8>(C->Work.Serial + 1);
		C->OnRep_Work();
		C->ForceNetUpdate();
		const uint8 Serial = C->Work.Serial;
		const TWeakObjectPtr<AWRCharacter> Weak(C);
		C->GetWorldTimerManager().SetTimer(C->HandTimer, FTimerDelegate::CreateLambda([Weak, Serial]() { FinishHandItem(Weak, Serial); }), Seconds, false);
	}
}

EWRWork FWRCraftWork::KindFor(FName StationType)
{
	if (StationType == TEXT("forge")) return EWRWork::Forge;
	if (StationType == TEXT("armorer")) return EWRWork::Armor;
	if (StationType == TEXT("workbench")) return EWRWork::Plane;
	if (StationType == TEXT("alchemy_table")) return EWRWork::Pestle;
	return EWRWork::None;
}

EWRWork FWRCraftWork::HandKind(const FWRRecipe& Recipe)
{
	if (Recipe.Work == TEXT("knap")) return EWRWork::Knap;
	if (Recipe.Work == TEXT("twist")) return EWRWork::Twist;
	for (const TPair<FName, int32>& In : Recipe.Inputs)
	{
		if (In.Key == TEXT("stone") || In.Key == TEXT("flint") || In.Key == TEXT("obsidian")) return EWRWork::Knap;
	}
	return EWRWork::Twist;
}

bool FWRCraftWork::SpotFor(FName StationType, FVector& OutLocal, float& OutYaw)
{
	// Measured on the fitted station props (cooks/cookpass-crafting.md 2.2): the smith a long step behind the forge's
	// anvil facing the fire; the armourer and the joiner at the near long side of the block and the bench; the
	// apothecary at the table's free front (the tray's low side).
	struct FSpot { const TCHAR* Type; float X, Y, Yaw; };
	static const FSpot Spots[] = { { TEXT("forge"), 0.f, 178.f, -90.f }, { TEXT("armorer"), 0.f, -78.f, 90.f },
		{ TEXT("workbench"), -14.f, -74.f, 90.f }, { TEXT("alchemy_table"), -100.f, 0.f, 0.f } };
	const FWRTuning& T = FWRTuning::Get();
	for (const FSpot& S : Spots)
	{
		if (StationType != S.Type) continue;
		const FString K = FString::Printf(TEXT("craft.work.spot.%s."), S.Type);
		OutLocal = FVector(T.Num(K + TEXT("x"), S.X), T.Num(K + TEXT("y"), S.Y), 0.f);
		OutYaw = T.Num(K + TEXT("yaw"), S.Yaw);
		return true;
	}
	return false;
}

float FWRCraftWork::Hz(EWRWork Kind)
{
	const FWRTuning& T = FWRTuning::Get();
	switch (Kind)
	{
	case EWRWork::Knap: return T.Num(TEXT("craft.work.knap.hz"), 2.f);
	case EWRWork::Twist: return T.Num(TEXT("craft.work.twist.hz"), 0.8f);
	case EWRWork::Forge: return T.Num(TEXT("craft.work.forge.hz"), 1.5f);
	case EWRWork::Armor: return T.Num(TEXT("craft.work.armor.hz"), 2.2f);
	case EWRWork::Plane: return T.Num(TEXT("craft.work.plane.hz"), 0.7f);
	case EWRWork::Pestle: return T.Num(TEXT("craft.work.pestle.hz"), 0.6f);
	default: return 1.f;
	}
}

float FWRCraftWork::StrikeAt(EWRWork Kind)
{
	const FWRTuning& T = FWRTuning::Get();
	switch (Kind)
	{
	case EWRWork::Knap: return T.Num(TEXT("craft.work.knap.strike"), 0.62f);
	case EWRWork::Twist: return 0.5f;
	case EWRWork::Forge: return T.Num(TEXT("craft.work.forge.strike"), 0.62f);
	case EWRWork::Armor: return T.Num(TEXT("craft.work.armor.strike"), 0.62f);
	case EWRWork::Plane: return 0.08f;
	case EWRWork::Pestle: return 0.f;
	default: return 0.5f;
	}
}

FString FWRCraftWork::StartHand(AWRPlayerController* Player, FName RecipeId, int32 Times)
{
	using namespace WRCraftWorkDetail;
	const FWRRecipe* R = FWRRecipeRegistry::Get().Find(RecipeId);
	if (!R || R->Station != TEXT("hand"))
	{
		return TEXT("that needs a station");
	}
	AWRCharacter* C = Player && Player->HasAuthority() ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	if (!C)
	{
		return FString();
	}
	const FString Why = Busy(C);
	if (!Why.IsEmpty())
	{
		return Why;
	}
	const FString Short = FWRCrafting::Shortfall(Player, *R, NAME_None, 1, C);
	if (!Short.IsEmpty())
	{
		return Short;
	}
	// Walking off a station's work to craft by hand ends that.
	if (C->Work.IsStation()) End(C);
	C->HandRecipe = R->Id;
	C->HandLeft = FMath::Clamp(Times, 1, FMath::RoundToInt(WRTune(TEXT("craft.hand.maxBatch"), 10.f)));
	C->Work.Made = 0;
	C->Work.Of = static_cast<uint8>(C->HandLeft);
	BeginHandItem(C);
	if (!C->Work.IsHand())
	{
		return FString();
	}
	const FWRItemDef* Out = FWRItemRegistry::Get().Find(R->Output);
	return FString::Printf(TEXT("Making %s by hand%s"), Out ? *Out->Name : *R->Output.ToString(), C->Work.Of > 1 ? *FString::Printf(TEXT(" (%d)"), C->Work.Of) : TEXT(""));
}

FString FWRCraftWork::StopHand(AWRPlayerController* Player)
{
	AWRCharacter* C = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	if (!C || !C->Work.IsHand())
	{
		return TEXT("Not crafting");
	}
	C->HandLeft = 0;
	return TEXT("Stopping after this one");
}

FString FWRCraftWork::StartStation(AWRPlayerController* Player, AWRStation* Station, bool bQuiet)
{
	using namespace WRCraftWorkDetail;
	AWRCharacter* C = Player && Player->HasAuthority() ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	const EWRWork Kind = Station ? KindFor(Station->StationType) : EWRWork::None;
	if (!C || !PS || Kind == EWRWork::None || Station->Queue.Num() == 0 || Station->Queue[0].Owner != PS->GetPlayerName())
	{
		return FString();
	}
	if (C->Work.At == Station || (Station->Worker.IsValid() && Station->Worker.Get() != C) || !Busy(C).IsEmpty())
	{
		return FString();
	}
	FVector Local;
	float Yaw = 0.f;
	if (!SpotFor(Station->StationType, Local, Yaw))
	{
		return FString();
	}
	const FTransform Frame = PieceFrame(Station);
	const FVector Spot = Frame.TransformPosition(Local);
	if (FVector::Dist2D(Spot, C->GetActorLocation()) > WRTune(TEXT("craft.work.approachMax"), 400.f) + 150.f)
	{
		return bQuiet ? FString() : FString::Printf(TEXT("Stand at %s to work it yourself"), SpotWord(Station->StationType));
	}
	if (C->Work.IsStation()) End(C);
	C->Work.Kind = Kind;
	C->Work.Item = ItemOf(Station);
	C->Work.At = Station;
	C->Work.Spot = FVector(Spot.X, Spot.Y, C->GetActorLocation().Z);
	C->Work.Yaw = Station->GetActorRotation().Yaw + Yaw;
	C->Work.Seconds = 0.f;
	C->Work.Made = C->Work.Of = 0;
	C->Work.Serial = static_cast<uint8>(C->Work.Serial + 1);
	C->bWorkArrived = false;
	C->WorkSince = C->GetWorld()->GetTimeSeconds();
	C->OnRep_Work();
	C->ForceNetUpdate();
	Station->Worker = C;
	Station->SetActorTickEnabled(true);
	UE_LOG(LogWildreach, Log, TEXT("CRAFT %s goes to work the %s"), *PS->GetPlayerName(), *Station->StationType.ToString());
	return FString();
}

void FWRCraftWork::TickStation(AWRStation* Station)
{
	using namespace WRCraftWorkDetail;
	AWRCharacter* C = Station ? Station->Worker.Get() : nullptr;
	if (!C || C->Work.At != Station)
	{
		if (Station)
		{
			Station->Worker = nullptr;
			if (Station->WorkerRep) { Station->WorkerRep = nullptr; Station->ForceNetUpdate(); }
		}
		return;
	}
	const AWRPlayerState* PS = C->GetPlayerState<AWRPlayerState>();
	// Not theirs to work any more (the order's done or cancelled, someone else's is next), or they're busy elsewhere.
	if (!PS || Station->Queue.Num() == 0 || Station->Queue[0].Owner != PS->GetPlayerName() || !C->IsAlive()
		|| C->HasTag(WRTags::State_Mounted) || C->HasTag(WRTags::Status_Staggered) || C->HasTag(WRTags::Status_Frozen)
		|| C->HasTag(WRTags::State_Dodging) || C->HasTag(WRTags::State_Attacking) || C->HasTag(WRTags::State_Blocking)
		|| C->HasTag(WRTags::State_Drinking) || (C->GetCharacterMovement() && C->GetCharacterMovement()->IsSwimming()))
	{
		End(C);
		return;
	}
	// The next order of theirs makes something else: the piece changes.
	const FName Item = ItemOf(Station);
	if (Item != C->Work.Item)
	{
		C->Work.Item = Item;
		C->ForceNetUpdate();
	}
	const float Reach = WRTune(TEXT("craft.work.reach"), 60.f);
	const float Off = FVector::Dist2D(C->GetActorLocation(), C->Work.Spot);
	if (!C->bWorkArrived)
	{
		if (Off <= Reach)
		{
			C->bWorkArrived = true;
			Station->WorkerRep = C;
			Station->ForceNetUpdate();
		}
		else if (C->GetWorld()->GetTimeSeconds() - C->WorkSince > WRTune(TEXT("craft.work.arriveSeconds"), 3.f))
		{
			End(C, FString::Printf(TEXT("Stand at %s to work it yourself"), SpotWord(Station->StationType)));
		}
		return;
	}
	if (Off > Reach * 1.5f)
	{
		End(C);
	}
}

bool FWRCraftWork::IsWorking(const AWRStation* Station)
{
	const AWRCharacter* C = Station ? Station->Worker.Get() : nullptr;
	return C && C->bWorkArrived && C->Work.At == Station;
}

void FWRCraftWork::End(AWRCharacter* Who, const FString& Why)
{
	if (!Who)
	{
		return;
	}
	if (AWRStation* St = Who->Work.At)
	{
		if (St->Worker.Get() == Who) St->Worker = nullptr;
		if (St->WorkerRep == Who) { St->WorkerRep = nullptr; St->ForceNetUpdate(); }
	}
	if (Who->Work.IsHand())
	{
		Who->GetWorldTimerManager().ClearTimer(Who->HandTimer);
		Who->HandLeft = 0;
		// What's still in the hands (ended from outside the finish) goes back.
		if (Who->HandTaken.Num() > 0) WRCraftWorkDetail::Refund(Who);
	}
	const uint8 Serial = static_cast<uint8>(Who->Work.Serial + 1);
	Who->Work = FWRWorkRep();
	Who->Work.Serial = Serial;
	Who->bWorkArrived = false;
	Who->OnRep_Work();
	Who->ForceNetUpdate();
	if (!Why.IsEmpty())
	{
		if (AWRPlayerController* PC = WRCraftWorkDetail::PCOf(Who)) PC->ClientNotify(Why);
	}
}
