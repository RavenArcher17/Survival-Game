#include "WRHUD.h"
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
#include "WRPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

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
	if (!Survivor || !Survivor->GetAttributes())
	{
		DrawText(TEXT("Respawning..."), FLinearColor::White, Canvas->ClipX * 0.5f - 50.f, Canvas->ClipY * 0.5f);
		return;
	}

	DrawWorldBars(Survivor);

	// Chat (last lines, fade after 25 s) and the map list (M).
	if (const AWRPlayerController* WPC = Cast<AWRPlayerController>(PlayerOwner))
	{
		const double ChatNow = GetWorld()->GetTimeSeconds();
		float CY = Canvas->ClipY - 300.f;
		for (int32 i = WPC->GetChat().Num() - 1; i >= 0; --i)
		{
			const AWRPlayerController::FChatLine& L = WPC->GetChat()[i];
			if (ChatNow - L.Time > 25.0) continue;
			DrawText(L.Text, FLinearColor(0.85f, 0.95f, 1.f), 20.f, CY, nullptr, 1.f);
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
		if (ASC && ASC->HasMatchingGameplayTag(WRTags::Combo_Thaw)) Line += TEXT("[Thaw] ");
		if (ASC && ASC->HasMatchingGameplayTag(WRTags::Combo_Balance)) Line += TEXT("[Balance] ");
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
		else if (PC->IsPlacing())
		{
			const FWRPieceDef* Def = FWRBuildRegistry::Get().Find(PC->GetPlacingPiece());
			const FString Prompt = FString::Printf(TEXT("Placing %s: LMB place, R rotate, RMB cancel%s"), Def ? *Def->Name : TEXT("?"),
				PC->IsPlacementValid() ? TEXT("") : *FString::Printf(TEXT("  (%s)"), *PC->GetPlacementWhy()));
			DrawText(Prompt, PC->IsPlacementValid() ? FLinearColor(0.6f, 1.f, 0.6f) : FLinearColor(1.f, 0.5f, 0.5f), Canvas->ClipX * 0.5f - Prompt.Len() * 3.5f, Canvas->ClipY * 0.62f, nullptr, 1.1f);
		}
		else if (const AWRBuildPiece* Piece = Cast<AWRBuildPiece>(PC->FindUseTarget()); Piece && Piece->GetDef())
		{
			const FWRPieceDef* Def = Piece->GetDef();
			FString Prompt = FString::Printf(TEXT("E: %s"), *Def->Name);
			if (Def->Kind == TEXT("door")) Prompt += Piece->bOpen ? TEXT(" (close)") : Piece->bLocked ? TEXT(" (locked)") : TEXT(" (open)");
			if (Def->Kind == TEXT("crop") && Piece->bPlanted) Prompt += TEXT(" (growing)");
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

void AWRHUD::DrawWorldBars(const AWRCharacterBase* Self)
{
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		const AWRCharacterBase* Other = *It;
		const UWRAttributeSet* A = Other->GetAttributes();
		if (Other == Self || !A || Other->IsHidden() || FVector::Dist(Other->GetActorLocation(), Self->GetActorLocation()) > 2500.f)
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
		DrawText(Other->GetDisplayLabel(), FLinearColor::White, BX, Screen.Y - 30.f, nullptr, 0.9f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX, Screen.Y - 12.f, W, 6.f);
		DrawRect(FLinearColor(0.85f, 0.15f, 0.15f), BX, Screen.Y - 12.f, W * FMath::Clamp(A->GetHealth() / FMath::Max(1.f, A->GetMaxHealth()), 0.f, 1.f), 6.f);
		// Buildup meters under the health bar.
		DrawRect(FLinearColor(1.f, 0.45f, 0.1f), BX, Screen.Y - 5.f, W * FMath::Clamp(A->GetBurnBuildup() / 100.f, 0.f, 1.f), 3.f);
		DrawRect(FLinearColor(0.55f, 0.85f, 1.f), BX, Screen.Y - 1.f, W * FMath::Clamp(A->GetFrostBuildup() / 100.f, 0.f, 1.f), 3.f);
		if (const AWRCreature* Creature = Cast<AWRCreature>(Other))
		{
			// Torpor (purple) and taming progress (green) while you work on a tame.
			if (!Creature->IsTamed() && (Creature->Torpor > 0.f || Creature->TameProgress > 0.f))
			{
				DrawRect(FLinearColor(0.6f, 0.3f, 0.9f), BX, Screen.Y + 3.f, W * FMath::Clamp(Creature->Torpor / FMath::Max(1.f, Creature->MaxTorpor), 0.f, 1.f), 3.f);
				DrawRect(FLinearColor(0.3f, 0.9f, 0.35f), BX, Screen.Y + 7.f, W * FMath::Clamp(Creature->TameProgress, 0.f, 1.f), 3.f);
				if (Creature->IsUnconscious())
				{
					DrawText(FString::Printf(TEXT("unconscious - E to feed (%.0f%%)"), Creature->TameProgress * 100.f), FLinearColor(0.8f, 0.7f, 1.f), BX, Screen.Y + 12.f, nullptr, 0.8f);
				}
			}
		}
	}
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
