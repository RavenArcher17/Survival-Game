#include "WRHUD.h"
#include "WRWorldSystems.h"
#include "WRGod.h"
#include "WRTuning.h"
#include "WRPadInput.h"
#include "WRKeyBindings.h"
#include "WRArtifacts.h"
#include "WRDeeds.h"
#include "GameFramework/PlayerState.h"
#include "WRFarm.h"
#include "WRStatusComponent.h"
#include "WRAbilities.h"
#include "WRAltar.h"
#include "WRBuilding.h"
#include "WRCrafting.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRWorldDressing.h"
#include "WRPlayerController.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRCombos.h"
#include "WRPlayerController.h"
#include "WRGameState.h"
#include "WRPlayerUI.h"
#include "WRWater.h"
#include "AbilitySystemComponent.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "WRFxSystem.h"

namespace
{
	constexpr float BarHeight = 14.f;
	constexpr float BarSpacing = 20.f;
}

void AWRHUD::BeginPlay()
{
	Super::BeginPlay();
	HitHandle = AWRCharacterBase::OnHitShown.AddUObject(this, &AWRHUD::OnHitShown);
}

void AWRHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	AWRCharacterBase::OnHitShown.Remove(HitHandle);
	Super::EndPlay(Reason);
}

void AWRHUD::OnHitShown(const AActor* Target, float Damage, EWRHitResult Result)
{
	if (!Target)
	{
		return;
	}
	FFloatingText Text;
	Text.WorldPos = Target->GetActorLocation() + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 110.f);
	if (Result == EWRHitResult::Hit)
	{
		Text.Text = FString::Printf(TEXT("%.0f"), Damage);
		Text.Color = Target == GetOwningPawn() ? FLinearColor(1.f, 0.3f, 0.3f) : FLinearColor(1.f, 0.95f, 0.6f);
	}
	else
	{
		Text.Text = UWRCombatLibrary::ResultName(Result);
		Text.Color = FLinearColor(0.7f, 0.85f, 1.f);
	}
	Floating.Add(Text);
}

void AWRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const float DeltaTime = LastDrawTime > 0.0 ? static_cast<float>(Now - LastDrawTime) : 0.f;
	LastDrawTime = Now;

	const AWRCharacter* Survivor = Cast<AWRCharacter>(GetOwningPawn());
	if (const AWRPlayerController* NewUIPC = Cast<AWRPlayerController>(PlayerOwner); NewUIPC && NewUIPC->GetUI())
	{
		// The stone-and-leather UI (Slate) draws the HUD, menus and damage numbers (WRFx::MakeHudOverlay); this canvas
		// keeps the nameplates over creatures and players and the old text panel for a menu that has no new screen yet.
		Floating.Reset();
		if (const AWRCharacterBase* Self = Cast<AWRCharacterBase>(GetOwningPawn()))
		{
			DrawWorldBars(Self);
		}
		if (NewUIPC->GetMenu() != EWRMenu::None && NewUIPC->GetMenu() != EWRMenu::Tame && !NewUIPC->GetUI()->IsScreenOpen())
		{
			DrawMenuPanel(NewUIPC);
		}
		return;
	}
	DrawDamageNumbers(DeltaTime);
	if (const AWRCreature* Mount = Cast<AWRCreature>(GetOwningPawn()))
	{
		// Riding: the mount's own bars, plus the way off.
		DrawWorldBars(Mount);
		const UWRAttributeSet* MA = Mount->GetAttributes();
		DrawText(FString::Printf(TEXT("Riding %s   (WASD, Space jump, E dismount)"), *Mount->GetDisplayLabel()), FLinearColor::White, 20.f, Canvas->ClipY - 80.f);
		if (MA)
		{
			DrawBar(TEXT("Mount"), MA->GetHealth(), MA->GetMaxHealth(), FLinearColor(0.8f, 0.2f, 0.2f), 20.f, Canvas->ClipY - 55.f, 200.f);
		}
		return;
	}
	// The clock, top right: day and time, pale blue at night.
	if (const AWRGameState* State = AWRGameState::Get(this))
	{
		const float Hour = State->GetHour();
		const FString Clock = FString::Printf(TEXT("Day %d   %02d:%02d"), State->GetDay(), FMath::FloorToInt(Hour), FMath::FloorToInt(Hour * 60.f) % 60);
		DrawText(Clock, State->IsNight() ? FLinearColor(0.6f, 0.75f, 1.f) : FLinearColor(1.f, 0.92f, 0.7f), Canvas->ClipX - 190.f, 50.f, nullptr, 1.3f);
	}

	if (!Survivor || !Survivor->GetAttributes())
	{
		DrawText(TEXT("Respawning..."), FLinearColor::White, Canvas->ClipX * 0.5f - 50.f, Canvas->ClipY * 0.5f);
		return;
	}

	DrawWorldBars(Survivor);
	DrawTamingPanel(Survivor);

	// Chat (last lines, fade after 25 s) and the map list (M).
	if (const AWRPlayerController* WPC = Cast<AWRPlayerController>(PlayerOwner))
	{
		const double ChatNow = GetWorld()->GetTimeSeconds();
		float CY = Canvas->ClipY - 300.f;
		for (int32 i = WPC->GetChat().Num() - 1; i >= FMath::Max(0, WPC->GetChat().Num() - 8); --i)
		{
			const AWRPlayerController::FChatLine& L = WPC->GetChat()[i];
			if (ChatNow - L.Time > 25.0) continue;
			DrawText(L.Text, AWRPlayerController::ChatColor(L.Channel), 20.f, CY, nullptr, 1.f);
			CY -= 20.f;
		}
		if (WPC->IsMapShown() && FWRPoi::All.Num() > 0)
		{
			const FVector Here = Survivor->GetActorLocation();
			static const TCHAR* Dirs[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
			const auto Compass = [](const FVector& D) { const float Deg = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)); return Dirs[((FMath::RoundToInt(Deg / 45.f) % 8) + 8) % 8]; };
			TArray<const FWRPoi*> Sorted;
			for (const FWRPoi& P : FWRPoi::All) Sorted.Add(&P);
			Sorted.Sort([&Here](const FWRPoi& A, const FWRPoi& B) { return FVector::Dist2D(A.Location, Here) < FVector::Dist2D(B.Location, Here); });
			const float MX = Canvas->ClipX - 430.f;
			float MY = 90.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), MX - 10.f, MY - 8.f, 420.f, 34.f + Sorted.Num() * 19.f);
			DrawText(FString::Printf(TEXT("Map (M)   you face %s"), Compass(Survivor->GetControlRotation().Vector())), FLinearColor(1.f, 0.85f, 0.6f), MX, MY, nullptr, 1.1f);
			MY += 26.f;
			for (const FWRPoi* P : Sorted)
			{
				const FVector D = P->Location - Here;
				const float Km = D.Size2D() / 100000.f;
				DrawText(FString::Printf(TEXT("%-22s %-10s %5s %s"), *P->Name, *P->Kind, *(Km < 1.f ? FString::Printf(TEXT("%.0f m"), Km * 1000.f) : FString::Printf(TEXT("%.1f km"), Km)), Compass(D)),
					P->Kind == TEXT("boss") ? FLinearColor(1.f, 0.5f, 0.4f) : FLinearColor::White, MX, MY, nullptr, 0.95f);
				MY += 19.f;
			}
		}
	}

	// Biome hazard underfoot without the gear for it.
	const FName Hazard = FWRBiomeHazards::KindAt(Survivor->GetActorLocation() - FVector(0.f, 0.f, Survivor->GetSimpleCollisionHalfHeight()));
	if (!Hazard.IsNone() && !FWRBiomeHazards::Resists(Survivor, Hazard))
	{
		DrawText(Hazard == TEXT("lava") ? TEXT("LAVA - it burns! (fire path, fire-infused armor or a mount keeps you safe)")
			: TEXT("Slick ice - hard to stop (frost path or frost-infused armor gives grip)"),
			Hazard == TEXT("lava") ? FLinearColor(1.f, 0.45f, 0.1f) : FLinearColor(0.6f, 0.85f, 1.f), Canvas->ClipX * 0.5f - 300.f, Canvas->ClipY - 140.f, nullptr, 1.1f);
	}

	// Boss bar across the top while you're in an engaged boss's fight.
	for (TActorIterator<AWRBoss> It(GetWorld()); It; ++It)
	{
		const UWRAttributeSet* BA = It->GetAttributes();
		if (!It->bEngaged || !It->IsAlive() || !BA || FVector::Dist(It->GetActorLocation(), Survivor->GetActorLocation()) > 4500.f)
		{
			continue;
		}
		const float W = Canvas->ClipX * 0.5f;
		const float BX = (Canvas->ClipX - W) * 0.5f;
		DrawText(FString::Printf(TEXT("%s   phase %d%s"), *It->GetDisplayLabel(), It->Phase, It->Participants > 1 ? *FString::Printf(TEXT("   (%d fighters)"), It->Participants) : TEXT("")),
			FLinearColor(1.f, 0.85f, 0.6f), BX, 18.f, nullptr, 1.2f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX, 42.f, W, 14.f);
		DrawRect(FLinearColor(0.75f, 0.1f, 0.08f), BX, 42.f, W * FMath::Clamp(BA->GetHealth() / FMath::Max(1.f, BA->GetMaxHealth()), 0.f, 1.f), 14.f);
		if (It->ShellFraction > 0.f)
		{
			DrawRect(FLinearColor(0.5f, 0.5f, 0.55f), BX, 58.f, W * It->ShellFraction, 5.f);
		}
		break;
	}

	const UWRAttributeSet* A = Survivor->GetAttributes();
	const UWRPathComponent* Paths = UWRPathComponent::FindFor(Survivor);
	const UAbilitySystemComponent* ASC = Survivor->GetAbilitySystemComponent();

	// Vitals, bottom left.
	float X = 24.f;
	float Y = Canvas->ClipY - 8.f * BarSpacing - 24.f;
	DrawBar(TEXT("Health"), A->GetHealth(), A->GetMaxHealth(), FLinearColor(0.8f, 0.15f, 0.15f), X, Y); Y += BarSpacing;
	DrawBar(TEXT("Stamina"), A->GetStamina(), A->GetMaxStamina(), FLinearColor(0.3f, 0.8f, 0.3f), X, Y); Y += BarSpacing;
	DrawBar(TEXT("Poise"), A->GetPoise(), A->GetMaxPoise(), FLinearColor(0.75f, 0.75f, 0.75f), X, Y); Y += BarSpacing;
	if (Paths && Paths->IsActivePath(TEXT("fire")))
	{
		DrawBar(TEXT("Heat"), A->GetHeat(), A->GetMaxHeat(), FLinearColor(1.f, 0.45f, 0.1f), X, Y); Y += BarSpacing;
	}
	if (Paths && Paths->IsActivePath(TEXT("frost")))
	{
		DrawBar(TEXT("Chill"), A->GetChill(), A->GetMaxChill(), FLinearColor(0.55f, 0.85f, 1.f), X, Y); Y += BarSpacing;
	}
	if (Paths && Paths->IsActivePath(TEXT("lightning")))
	{
		DrawBar(TEXT("Charge"), A->GetCharge(), A->GetMaxCharge(), FLinearColor(FColor(0xB9, 0xA7, 0xFF)), X, Y); Y += BarSpacing;
	}
	// Growth (the Nature path, -oldui): moss green #5E8C31.
	if (Paths && Paths->IsActivePath(TEXT("nature")))
	{
		DrawBar(TEXT("Growth"), A->GetGrowth(), A->GetMaxGrowth(), FLinearColor(FColor(0x5E, 0x8C, 0x31)), X, Y); Y += BarSpacing;
	}
	// Momentum (the Wind path, -oldui): pale teal #9FD6CF.
	if (Paths && Paths->IsActivePath(TEXT("wind")))
	{
		DrawBar(TEXT("Momentum"), A->GetMomentum(), A->GetMaxMomentum(), FLinearColor(FColor(0x9F, 0xD6, 0xCF)), X, Y); Y += BarSpacing;
	}
	// Mana (the Arcane path) violet #7A3FD1; Flow (the Tide path) sea-green #1E8C86 (-oldui).
	if (Paths && Paths->IsActivePath(TEXT("arcane")))
	{
		DrawBar(TEXT("Mana"), A->GetMana(), A->GetMaxMana(), FLinearColor(FColor(0x7A, 0x3F, 0xD1)), X, Y); Y += BarSpacing;
	}
	if (Paths && Paths->IsActivePath(TEXT("spore")))
	{
		DrawBar(TEXT("Spores"), A->GetSpores(), A->GetMaxSpores(), FLinearColor(FColor(0x39, 0xE6, 0xC2)), X, Y); Y += BarSpacing;
	}
	if (Paths && Paths->IsActivePath(TEXT("tide")))
	{
		DrawBar(TEXT("Flow"), A->GetFlow(), A->GetMaxFlow(), FLinearColor(FColor(0x1E, 0x8C, 0x86)), X, Y); Y += BarSpacing;
	}
	if (Paths && Paths->IsActivePath(TEXT("sun")))
	{
		DrawBar(TEXT("Radiance"), A->GetRadiance(), A->GetMaxRadiance(), FLinearColor(FColor(0xE8, 0xB2, 0x30)), X, Y); Y += BarSpacing;
	}
	DrawBar(TEXT("Hunger"), A->GetHunger(), 100.f, FLinearColor(0.85f, 0.55f, 0.15f), X, Y, 110.f); Y += BarSpacing;
	DrawBar(TEXT("Thirst"), A->GetThirst(), 100.f, FLinearColor(0.2f, 0.5f, 0.9f), X, Y, 110.f); Y += BarSpacing;

	// Belt: potions / food / water counts and the potion cooldown.
	if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Survivor))
	{
		const float Cooldown = Inv->GetPotionCooldownRemaining();
		FString Belt = FString::Printf(TEXT("Q potion x%d%s   Z food x%d   X water x%d   (%.1f kg)"),
			Inv->CountOf(TEXT("minor_healing_potion")) + Inv->CountOf(TEXT("healing_potion")),
			Cooldown > 0.f ? *FString::Printf(TEXT(" (%.0fs)"), Cooldown) : TEXT(""),
			Inv->CountOf(TEXT("cooked_meat")), Inv->CountOf(TEXT("waterskin")),
			FWRInventoryRules::TotalWeight(Inv->GetSlots()));
		DrawText(Belt, FLinearColor(0.9f, 0.9f, 0.9f), X + 320.f, Canvas->ClipY - 3.f * BarSpacing - 24.f);
		// Ranged slot: what it is and what it's loaded with.
		if (const FWRItemDef* Ranged = FWRItemRegistry::Get().Find(Survivor->GetRangedItem()))
		{
			const FWRItemDef* Ammo = FWRItemRegistry::Get().Find(Survivor->GetSelectedAmmo());
			const FString Loaded = Ranged->IsThrowable() ? FString::Printf(TEXT("%s x%d"), *Ranged->Name, Inv->GetEquipped(EWREquipSlot::Ranged).Count)
				: FString::Printf(TEXT("%s: %s x%d"), *Ranged->Name, Ammo ? *Ammo->Name : TEXT("no ammo"), Ammo ? Inv->CountOf(Ammo->Id) : 0);
			DrawText(FString::Printf(TEXT("%s   (G: %s, RMB aim, wheel: ammo)"), *Loaded, Survivor->IsRangedOut() ? TEXT("put away") : TEXT("draw")),
				Survivor->IsRangedOut() ? FLinearColor(1.f, 0.85f, 0.45f) : FLinearColor(0.7f, 0.7f, 0.7f), X + 320.f, Canvas->ClipY - 3.f * BarSpacing - 44.f);
		}
	}

	// Statuses on yourself.
	FString StatusLine;
	const TPair<FGameplayTag, const TCHAR*> StatusNames[] = {
		{ WRTags::Status_Burn, TEXT("BURNING") }, { WRTags::Status_Frostbite, TEXT("FROSTBITE") },
		{ WRTags::Status_Slowed, TEXT("SLOWED") }, { WRTags::Status_Frozen, TEXT("FROZEN") },
		{ WRTags::Status_Staggered, TEXT("STAGGERED") }, { WRTags::State_Blocking, TEXT("blocking") } };
	for (const auto& Pair : StatusNames)
	{
		if (ASC && ASC->HasMatchingGameplayTag(Pair.Key))
		{
			StatusLine += FString(Pair.Value) + TEXT("  ");
		}
	}
	if (!StatusLine.IsEmpty())
	{
		DrawText(StatusLine, FLinearColor(1.f, 0.85f, 0.4f), X, Y);
	}

	// Path summary, top left.
	if (Paths)
	{
		FString Line = FString::Printf(TEXT("Lv %d   "), Paths->GetCharacterLevel());
		for (const FName PathId : { Paths->GetMainPath(), Paths->GetSubPath() })
		{
			if (!PathId.IsNone())
			{
				Line += FString::Printf(TEXT("%s%s %d/%d pts   "), *PathId.ToString(), PathId == Paths->GetMainPath() ? TEXT(" (main)") : TEXT(""),
					Paths->GetSpent(PathId), Paths->GetEarned(PathId));
			}
		}
		// The pair's combos the survivor owns (Data/combos.json).
		for (const FWRComboPair& Pair : FWRComboRegistry::Get().All())
		{
			for (const FWRComboLevel& Combo : Pair.Levels)
			{
				if (ASC && Combo.Grants.Num() > 0 && ASC->HasMatchingGameplayTag(Combo.Grants[0])) Line += FString::Printf(TEXT("[%s] "), *Combo.Name);
			}
		}
		DrawText(Line, FLinearColor::White, 24.f, 48.f);
	}
	DrawText(TEXT("WASD  Mouse  LMB light  R heavy  RMB block/parry  C dodge  Tab lock-on  1-8 abilities  Q/Z/X potion/eat/drink  E interact  I inventory  P paths  K skills  B build  M map  T chat  V view  G ranged weapon"),
		FLinearColor(1.f, 1.f, 1.f, 0.55f), 24.f, 24.f);

	// Quick slots, bottom centre.
	if (Paths && ASC)
	{
		const TArray<FName>& Slots = Paths->GetQuickSlots();
		const float SlotW = 110.f;
		float SX = Canvas->ClipX * 0.5f - SlotW * Slots.Num() * 0.5f;
		const float SY = Canvas->ClipY - 54.f;
		for (int32 i = 0; i < Slots.Num(); ++i, SX += SlotW)
		{
			FString Label = TEXT("-");
			float Cooldown = 0.f;
			const int32 InputId = static_cast<int32>(EWRAbilityInput::Slot1) + i;
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.InputID != InputId)
				{
					continue;
				}
				if (const UWRGameplayAbility* Instance = Cast<UWRGameplayAbility>(Spec.GetPrimaryInstance()))
				{
					Label = Instance->DisplayName;
					Cooldown = Instance->GetCooldownRemaining();
				}
				else if (const UWRGameplayAbility* CDO = Cast<UWRGameplayAbility>(Spec.Ability))
				{
					Label = CDO->DisplayName;
				}
			}
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), SX, SY, SlotW - 6.f, 34.f);
			DrawText(FString::Printf(TEXT("%d %s"), i + 1, *Label), FLinearColor::White, SX + 4.f, SY + 3.f);
			if (Cooldown > 0.f)
			{
				DrawText(FString::Printf(TEXT("%.1fs"), Cooldown), FLinearColor(1.f, 0.6f, 0.6f), SX + 4.f, SY + 17.f);
			}
		}
	}

	// Crosshair (first person) and lock-on marker.
	if (Survivor->IsAimingRanged())
	{
		// Aiming: a small cross where the shot goes.
		const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
		DrawRect(FLinearColor::White, CX - 9.f, CY - 1.f, 6.f, 2.f);
		DrawRect(FLinearColor::White, CX + 3.f, CY - 1.f, 6.f, 2.f);
		DrawRect(FLinearColor::White, CX - 1.f, CY - 9.f, 2.f, 6.f);
		DrawRect(FLinearColor::White, CX - 1.f, CY + 3.f, 2.f, 6.f);
	}
	else if (Survivor->IsFirstPerson())
	{
		DrawRect(FLinearColor::White, Canvas->ClipX * 0.5f - 1.f, Canvas->ClipY * 0.5f - 1.f, 3.f, 3.f);
	}
	if (const AWRPlayerController* PC = Cast<AWRPlayerController>(GetOwningPlayerController()))
	{
		// Crafting / inventory menu panel.
		if (PC->GetMenu() != EWRMenu::None)
		{
			DrawMenuPanel(PC);
		}
		else if (PC->IsPlacing())
		{
			const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PC->GetPlacingPiece());
			const FString Why = PC->IsPlacementValid() ? FString() : FString::Printf(TEXT("  (%s)"), *PC->GetPlacementWhy());
			const FString Prompt = Def ? FString::Printf(TEXT("Placing %s: LMB place, R rotate, RMB cancel%s"), *Def->Name, *Why)
				: FString::Printf(TEXT("Demolish: aim at a piece of yours, LMB remove (half back), RMB stop%s"), *Why);
			DrawText(Prompt, PC->IsPlacementValid() ? FLinearColor(0.6f, 1.f, 0.6f) : FLinearColor(1.f, 0.5f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.1f);
		}
		else if (const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(PC->FindUseTarget()); Piece && Piece->GetDef())
		{
			const FWRPieceDef* Def = Piece->GetDef();
			FString Prompt = FString::Printf(TEXT("E: %s"), *Def->Name);
			if (Def->Kind == TEXT("door")) Prompt += Piece->bOpen ? TEXT(" (close)") : Piece->bLocked ? TEXT(" (locked)") : TEXT(" (open)");
			if (Def->IsFarm()) Prompt = FString::Printf(TEXT("E: %s"), *FWRFarm::Describe(Piece, PC));
			DrawText(Prompt, FLinearColor(1.f, 0.9f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.2f);
		}
		else if (const AWRStation* Station = Cast<AWRStation>(PC->FindUseTarget()))
		{
			const FString Prompt = FString::Printf(TEXT("E: use %s"), *Station->GetDisplayName());
			DrawText(Prompt, FLinearColor(1.f, 0.9f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.2f);
		}
		else if (const AWRHearth* Hearth = PC->FindNearbyHearth())
		{
			const FString Prompt = FString::Printf(TEXT("E: Hearth (tech level %d, %d core%s)%s"), Hearth->GetTechLevel(), Hearth->GetCores().Num(),
				Hearth->GetCores().Num() == 1 ? TEXT("") : TEXT("s"), Hearth->IsBroken() ? TEXT("  BROKEN") : TEXT(""));
			DrawText(Prompt, FLinearColor(1.f, 0.9f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.2f);
		}
		else if (const UWRWaterSubsystem* Water = UWRWaterSubsystem::Get(PC); Water && Water->CanDrink(Cast<ACharacter>(PC->GetPawn())))
		{
			const FString Prompt = FString::Printf(TEXT("E: %s (sea water)"), *UWRWaterSubsystem::WaterVerb(Cast<ACharacter>(PC->GetPawn())).ToLower());
			DrawText(Prompt, FLinearColor(1.f, 0.9f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.2f);
		}
		if (const AWRBossAltar* Altar = PC->FindNearbyAltar())
		{
			const FString Prompt = Altar->GetPrompt(PC);
			DrawText(Prompt, FLinearColor(1.f, 0.9f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.2f);
		}
		if (!PC->GetNotice().IsEmpty() && GetWorld()->GetTimeSeconds() - PC->GetNoticeTime() < 4.0)
		{
			DrawText(PC->GetNotice(), FLinearColor::White, Canvas->ClipX * 0.5f - PC->GetNotice().Len() * 3.5f, Canvas->ClipY * 0.3f, nullptr, 1.3f);
		}
		if (const AWRCharacterBase* Lock = PC->GetLockTarget())
		{
			const FVector Screen = Project(Lock->GetActorLocation(), true);
			if (Screen.Z > 0.f)
			{
				DrawRect(FLinearColor(1.f, 0.9f, 0.2f), Screen.X - 4.f, Screen.Y - 4.f, 8.f, 8.f);
			}
		}
	}
}

void AWRHUD::DrawBar(const FString& Label, float Value, float Max, const FLinearColor& Color, float X, float Y, float Width)
{
	const float Fraction = Max > 0.f ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X + 70.f, Y, Width, BarHeight);
	DrawRect(Color, X + 70.f, Y, Width * Fraction, BarHeight);
	DrawText(Label, FLinearColor::White, X, Y - 1.f);
	DrawText(FString::Printf(TEXT("%.0f"), Value), FLinearColor(1.f, 1.f, 1.f, 0.8f), X + 74.f + Width, Y - 1.f);
}

void AWRHUD::DrawCentred(const FString& Text, const FLinearColor& Color, float CentreX, float Y, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, nullptr, Scale);
	DrawText(Text, Color, CentreX - W * 0.5f, Y, nullptr, Scale);
}

void AWRHUD::DrawWorldBars(const AWRCharacterBase* Self)
{
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		const AWRCharacterBase* Other = *It;
		const UWRAttributeSet* A = Other->GetAttributes();
		if (Other == Self || !A || Other->IsHidden())
		{
			continue;
		}
		// Bars within ui.names.barCm; other players' names by Settings > Interface > Names over players: Always (out to
		// farCm, the name alone past the bar range), Near (nearCm), Off (no name; the bar still shows close by).
		const float Dist = FVector::Dist(Other->GetActorLocation(), Self->GetActorLocation());
		const bool bPlayer = Other->GetPlayerState() != nullptr;
		const int32 NamesMode = FWRKeyBindings::Get().PrefInt(TEXT("Names"));
		const float BarRange = WRTune(TEXT("ui.names.barCm"), 2500.f);
		const AWRWorldNpc* Npc = Cast<AWRWorldNpc>(Other);
		const bool bTown = Npc && Npc->IsTownsfolk();
		const float NameRange = bTown ? WRTune(TEXT("ui.names.townCm"), 800.f) : !bPlayer ? BarRange : NamesMode == 0 ? WRTune(TEXT("ui.names.farCm"), 8000.f) : NamesMode == 1 ? WRTune(TEXT("ui.names.nearCm"), 4000.f) : 0.f;
		const bool bBar = !bTown && Dist <= BarRange;
		const bool bName = Dist <= NameRange;
		if (!bBar && !bName)
		{
			continue;
		}
		const FVector Screen = Project(Other->GetActorLocation() + FVector(0.f, 0.f, Other->GetSimpleCollisionHalfHeight() + 30.f), true);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float W = 80.f;
		const float BX = Screen.X - W * 0.5f;
		if (const AWRCreature* Body = Cast<AWRCreature>(Other); Body && Body->IsHarvestableBody())
		{
			// A kill's body (WRCarcass.cpp): what's left to cut, not an empty health bar, just over where it lies (its capsule
			// still stands upright).
			const FVector Low = Project(Other->GetActorLocation() + FVector(0.f, 0.f, 40.f), true);
			if (Low.Z > 0.f)
			{
				DrawCentred(FString::Printf(TEXT("%s (body)"), *Other->GetDisplayLabel()), FLinearColor(0.9f, 0.82f, 0.68f), Low.X, Low.Y - 30.f, 0.9f);
				DrawCentred(FString::Printf(TEXT("hit to harvest: %d left"), Body->GetBodyHitsLeft()), FLinearColor(0.75f, 0.7f, 0.62f), Low.X, Low.Y - 13.f, 0.75f);
			}
			continue;
		}
		// A survivor's worn title on a smaller line in pale brass under the name (WRDeeds).
		const UWRDeedsComponent* Deeds = Other->GetPlayerState() ? UWRDeedsComponent::For(Other->GetPlayerState()) : nullptr;
		// A tester with god mode on (WRGod): "GOD" in red-gold over the name, so nobody takes it for normal play.
		if (const UWRGodComponent* God = bName && bPlayer ? UWRGodComponent::For(Other) : nullptr; God && God->AnyOn())
		{
			const bool bTitled = Deeds && !Deeds->ActiveTitle.IsEmpty();
			DrawCentred(FString::Printf(TEXT("GOD  ·  %s"), *God->Label), FLinearColor(1.f, 0.42f, 0.18f), Screen.X, Screen.Y - (bTitled ? 62.f : 46.f), 0.72f);
		}
		if (bName && Deeds && !Deeds->ActiveTitle.IsEmpty())
		{
			DrawCentred(Other->GetDisplayLabel(), FLinearColor::White, Screen.X, Screen.Y - 45.f, 0.9f);
			DrawCentred(Deeds->ActiveTitle, FLinearColor(0.86f, 0.74f, 0.5f), Screen.X, Screen.Y - 29.f, 0.72f);
		}
		else if (bName) DrawCentred(Other->GetDisplayLabel(), FLinearColor::White, Screen.X, Screen.Y - 30.f, 0.9f);
		if (!bBar)
		{
			continue;   // a far player's name alone
		}
		// Gnomon Eye (artifact): no enemy health bars; names and statuses still show.
		if (!(PlayerOwner.Get() && WRArtifacts::Has(PlayerOwner->PlayerState.Get(), TEXT("no_enemy_bars")) && !Other->GetPlayerState()))
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX, Screen.Y - 12.f, W, 6.f);
			const float Fill = FMath::Clamp(A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()), 0.f, 1.f);
			// What a blow took stays as a pale chunk, holds, then drains (cookpass-combat 3.3); a heal clears it.
			{
				const double Now = GetWorld()->GetTimeSeconds();
				if (BarTrails.Num() > 64)
				{
					// Bodies long gone or out of sight for 4 s drop their trail.
					for (auto Tri = BarTrails.CreateIterator(); Tri; ++Tri) if (!Tri.Key().IsValid() || Now - Tri.Value().Seen > 4.0) Tri.RemoveCurrent();
				}
				FBarTrail& Tr = BarTrails.FindOrAdd(Other);
				const float Dt = Tr.Seen > 0.0 ? FMath::Clamp(float(Now - Tr.Seen), 0.f, 0.1f) : 0.f;
				Tr.Seen = Now;
				if (Tr.Last < 0.f || Fill >= Tr.Last + 0.0005f) Tr.Top = Fill;
				else if (Fill < Tr.Last - 0.0005f) { Tr.Top = FMath::Max(Tr.Top, Tr.Last); Tr.DropAt = Now; }
				Tr.Last = Fill;
				if (Now - Tr.DropAt > WRTune(TEXT("ui.bar.trailHold"), 0.5f)) Tr.Top = FMath::Max(Fill, Tr.Top - WRTune(TEXT("ui.bar.trailDrain"), 1.2f) * Dt);
				if (Tr.Top > Fill + 0.001f) DrawRect(FLinearColor(0.95f, 0.78f, 0.7f, 0.85f), BX + W * Fill, Screen.Y - 12.f, W * (Tr.Top - Fill), 6.f);
			}
			DrawRect(FLinearColor(0.85f, 0.15f, 0.15f), BX, Screen.Y - 12.f, W * Fill, 6.f);
			// Your running damage on it at the bar's right end while your blows keep landing.
			if (const UWRFxSubsystem* Fx = UWRFxSubsystem::Get(this); Fx && Fx->TallyTarget.Get() == Other && Fx->Tally >= 1.f
				&& GetWorld()->GetTimeSeconds() - Fx->TallyTime <= WRTune(TEXT("fx.hud.tallySeconds"), 2.5f))
			{
				DrawText(FString::FromInt(FMath::RoundToInt(Fx->Tally)), FLinearColor(0.92f, 0.88f, 0.8f), BX + W + 5.f, Screen.Y - 17.f, nullptr, 0.8f);
			}
		}
		// Buildup meters under the health bar: one thin row per status filling, in its colour (combat.md 2.11; bleed a
		// dark red, never the crimson of an unblockable blow).
		{
			float RowY = Screen.Y - 5.f;
			for (int32 i = 0; i < Other->BuildupPct.Num() && i < WRStatus::Count; ++i)
			{
				if (Other->BuildupPct[i] == 0) continue;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), BX, RowY, W, 3.f);
				DrawRect(WRStatus::Color(i), BX, RowY, W * Other->BuildupPct[i] / 100.f, 3.f);
				RowY += 4.f;
			}
		}
		if (const AWRCreature* Creature = Cast<AWRCreature>(Other))
		{
			// Torpor (purple) and taming progress (green) while you work on a tame.
			if (!Creature->IsTamed() && (Creature->Torpor > 0.f || Creature->TameProgress > 0.f))
			{
				DrawRect(FLinearColor(0.6f, 0.3f, 0.9f), BX, Screen.Y + 3.f, W * FMath::Clamp(Creature->Torpor / FMath::Max(1.f, Creature->MaxTorpor), 0.f, 1.f), 3.f);
				DrawRect(FLinearColor(0.3f, 0.9f, 0.35f), BX, Screen.Y + 7.f, W * FMath::Clamp(Creature->TameProgress, 0.f, 1.f), 3.f);
				if (Creature->IsUnconscious())
				{
					// The device's own button (a pad shows its button; a rebound key shows the new key).
					const FString Key = WRInput::ActionKey(TEXT("Interact")).ToString();
					DrawCentred(Key.IsEmpty() ? FString::Printf(TEXT("knocked out: tame it (%.0f%%)"), Creature->TameProgress * 100.f)
						: FString::Printf(TEXT("knocked out: %s to tame (%.0f%%)"), *Key, Creature->TameProgress * 100.f), FLinearColor(0.8f, 0.7f, 1.f), Screen.X, Screen.Y + 12.f, 0.8f);
				}
			}
		}
	}
}

void AWRHUD::DrawTamingPanel(const AWRCharacterBase* Self)
{
	const AWRCreature* Best = nullptr;
	float BestDist = 1500.f;
	for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Self->GetActorLocation());
		if (It->IsAlive() && !It->IsTamed() && It->IsUnconscious() && D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	if (!Best)
	{
		return;
	}
	const float X = Canvas->ClipX - 460.f;
	float Y = 150.f;
	const float W = 240.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 10.f, Y - 8.f, 450.f, 136.f);
	DrawText(FString::Printf(TEXT("Taming %s"), *Best->GetDisplayLabel()), FLinearColor::White, X, Y, nullptr, 1.1f);
	Y += 28.f;
	const float WakesIn = Best->WakeIn;
	DrawBar(TEXT("Torpor"), Best->Torpor, Best->MaxTorpor, FLinearColor(0.6f, 0.3f, 0.9f), X, Y, W);
	DrawText(FString::Printf(TEXT("wakes in %d:%02d"), FMath::FloorToInt(WakesIn / 60.f), FMath::FloorToInt(WakesIn) % 60),
		WakesIn < 30.f ? FLinearColor(1.f, 0.45f, 0.35f) : FLinearColor(0.8f, 0.8f, 0.8f), X + W + 110.f, Y - 1.f, nullptr, 0.85f);
	Y += 24.f;
	DrawBar(TEXT("Tamed"), Best->TameProgress * 100.f, 100.f, FLinearColor(0.3f, 0.9f, 0.35f), X, Y, W);
	Y += 26.f;
	const FWRItemDef* Food = FWRItemRegistry::Get().Find(Best->TameFoodItem);
	const FString FoodName = Food ? Food->Name : FString(TEXT("food"));
	DrawText(FString::Printf(TEXT("Effectiveness %.0f%%    Stash: %d%s"), Best->TameEffectiveness * 100.f, Best->TameFoodCount,
		Best->TameFoodCount > 0 ? *FString::Printf(TEXT(" (next: %s)"), *FoodName) : TEXT("")), FLinearColor(0.85f, 0.85f, 0.85f), X, Y, nullptr, 0.95f);
	Y += 24.f;
	FString Advice;
	FLinearColor Tint(0.6f, 1.f, 0.6f);
	if (Best->TameFoodCount == 0 && Best->MealLeft <= 0.f)
	{
		Advice = FString::Printf(TEXT("Hungry: E to give it %s or kibble"), *FoodName);
		Tint = FLinearColor(1.f, 0.6f, 0.3f);
	}
	else if (Best->TameEta < 0.f)
	{
		Advice = FString::Printf(TEXT("Needs %d more %s"), Best->TameFoodNeeded, *FoodName);
		Tint = FLinearColor(1.f, 0.85f, 0.4f);
	}
	else if (Best->TameEta > WakesIn)
	{
		Advice = FString::Printf(TEXT("Tamed in ~%.0f s, but it wakes first: give it narcotics"), Best->TameEta);
		Tint = FLinearColor(1.f, 0.6f, 0.3f);
	}
	else
	{
		Advice = FString::Printf(TEXT("Tamed in ~%.0f s"), Best->TameEta);
	}
	DrawText(Advice, Tint, X, Y, nullptr, 1.f);
}

void AWRHUD::DrawDamageNumbers(float DeltaTime)
{
	for (int32 i = Floating.Num() - 1; i >= 0; --i)
	{
		FFloatingText& T = Floating[i];
		T.Age += DeltaTime;
		if (T.Age > 1.2f)
		{
			Floating.RemoveAt(i);
			continue;
		}
		const FVector Screen = Project(T.WorldPos + FVector(0.f, 0.f, T.Age * 60.f), true);
		if (Screen.Z > 0.f)
		{
			FLinearColor C = T.Color;
			C.A = 1.f - T.Age / 1.2f;
			DrawText(T.Text, C, Screen.X, Screen.Y, nullptr, 1.3f);
		}
	}
}

void AWRHUD::DrawMenuPanel(const AWRPlayerController* PC)
{
	const TArray<FWRMenuEntry> Entries = PC->BuildMenuEntries();
	const float PX = 60.f, PY = 90.f, LineH = 20.f, PW = 760.f;
	const int32 Visible = 22;
	const int32 First = FMath::Clamp(PC->GetMenuIndex() - Visible / 2, 0, FMath::Max(0, Entries.Num() - Visible));
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), PX - 10.f, PY - 30.f, PW, (FMath::Min(Visible, Entries.Num()) + 2) * LineH + 20.f);
	DrawText(PC->GetMenuTitle(), FLinearColor(1.f, 0.85f, 0.4f), PX, PY - 24.f);
	for (int32 i = First; i < Entries.Num() && i < First + Visible; ++i)
	{
		const bool bSel = i == PC->GetMenuIndex();
		const float RowY = PY + (i - First) * LineH;
		if (bSel)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), PX - 6.f, RowY - 1.f, PW - 8.f, LineH);
		}
		DrawText(FString(bSel ? TEXT("> ") : TEXT("  ")) + Entries[i].Label,
			Entries[i].bAvailable ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f), PX, RowY);
	}
	if (Entries.Num() == 0)
	{
		DrawText(TEXT("(empty)"), FLinearColor(0.6f, 0.6f, 0.6f), PX, PY);
	}
}
