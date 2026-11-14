#include "WRWeapons.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRInventory.h"
#include "WRTuning.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerState.h"

static FAutoConsoleCommand GReloadWeaponsCmd(
	TEXT("WR.ReloadWeapons"),
	TEXT("Re-read Data/weapons.json (weapon classes, moves, guards)"),
	FConsoleCommandDelegate::CreateLambda([]() { FWRWeapons::Get().Reload(); }));

namespace WRWeaponsDetail
{
	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}

	bool Flag(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		bool B = false;
		double V = 0.0;
		return O.IsValid() && (O->TryGetBoolField(Field, B) ? B : (O->TryGetNumberField(Field, V) && V > 0.0));
	}

	EWRHitTier Tier(const FString& S)
	{
		if (S == TEXT("heavy")) return EWRHitTier::Heavy;
		if (S == TEXT("counter")) return EWRHitTier::Counter;
		if (S == TEXT("crit")) return EWRHitTier::Crit;
		return EWRHitTier::Light;
	}

	void ReadParry(const TSharedPtr<FJsonObject>& O, FWRGuard& G)
	{
		const TArray<TSharedPtr<FJsonValue>>* Parry = nullptr;
		if (O.IsValid() && O->TryGetArrayField(TEXT("parry"), Parry) && Parry->Num() == 3)
		{
			G.ParryStartup = float((*Parry)[0]->AsNumber());
			G.ParryActive = float((*Parry)[1]->AsNumber());
			G.ParryRecovery = float((*Parry)[2]->AsNumber());
		}
	}

	/** The sword as built before weapons.json (Raven's tuning), so a missing file still plays. */
	FWRWeaponClass Sword()
	{
		FWRWeaponClass C;
		C.Id = TEXT("sword");
		C.Name = TEXT("Sword");
		C.Guard.Id = TEXT("sword");
		const auto Add = [&C](const TCHAR* Id, float W, float A, float R, float Dmg, float Stance, float Stamina, FName Next, int32 Clip, bool bHeavy)
		{
			FWRMove M;
			M.Id = Id; M.Windup = W; M.Active = A; M.Recovery = R; M.Damage = Dmg; M.Stance = Stance; M.Stamina = Stamina;
			M.Next = Next; M.Clip = Clip; M.bHeavy = bHeavy; M.bHyperarmor = bHeavy; M.Impact = bHeavy ? 3 : 2;
			M.Poise = bHeavy ? 48.f : 30.f; M.GuardStamina = bHeavy ? 45.f : 30.f;
			M.Tier = bHeavy ? EWRHitTier::Heavy : EWRHitTier::Light;
			C.Moves.Add(M.Id, M);
		};
		Add(TEXT("light1"), 0.25f, 0.18f, 0.30f, 1.f, 5.f, 10.f, TEXT("light2"), 0, false);
		Add(TEXT("light2"), 0.22f, 0.16f, 0.30f, 1.f, 5.f, 10.f, TEXT("light3"), 1, false);
		Add(TEXT("light3"), 0.30f, 0.20f, 0.42f, 1.15f, 6.f, 10.f, NAME_None, 2, false);
		Add(TEXT("heavy"), 0.55f, 0.22f, 0.45f, 1.f, 10.f, 20.f, NAME_None, 0, true);
		Add(TEXT("guardCounter"), 0.30f, 0.20f, 0.50f, 1.3f, 30.f, 20.f, NAME_None, 2, true);
		C.Moves[TEXT("guardCounter")].bHyperarmor = false;
		C.Moves[TEXT("guardCounter")].Tier = EWRHitTier::Counter;
		FWRMove Riposte; Riposte.Id = TEXT("riposte"); Riposte.Total = 1.6f; Riposte.DamageAt = 0.9f; Riposte.Clip = 0;
		FWRMove Backstab; Backstab.Id = TEXT("backstab"); Backstab.Total = 1.4f; Backstab.DamageAt = 0.75f; Backstab.Clip = 1;
		C.Moves.Add(Riposte.Id, Riposte);
		C.Moves.Add(Backstab.Id, Backstab);
		return C;
	}
}

FWRWeapons& FWRWeapons::Get()
{
	static FWRWeapons Instance;
	return Instance;
}

FWRWeapons::FWRWeapons()
{
	Reload();
}

void FWRWeapons::Reload()
{
	using namespace WRWeaponsDetail;
	Classes.Reset();
	Offhands.Reset();
	Fallback = Sword();
	CarriedId = NAME_None;
	const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("weapons.json"));
	if (!Root.IsValid())
	{
		UE_LOG(LogWildreach, Warning, TEXT("WEAPONS Data/weapons.json missing: the sword's built-in moves are used"));
		Classes.Add(Fallback.Id, Fallback);
		return;
	}
	FString Carried;
	if (Root->TryGetStringField(TEXT("carried"), Carried)) CarriedId = FName(*Carried);

	const TSharedPtr<FJsonObject>* ClassObj = nullptr;
	if (Root->TryGetObjectField(TEXT("classes"), ClassObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ClassObj)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			if (!O.IsValid() || Pair.Key.StartsWith(TEXT("_"))) continue;
			FWRWeaponClass C;
			C.Id = FName(*Pair.Key);
			O->TryGetStringField(TEXT("name"), C.Name);
			C.Reach = Num(O, TEXT("reach"), 220.f);
			C.SweepRadius = Num(O, TEXT("sweepRadius"), 45.f);
			const TSharedPtr<FJsonObject>* Crit = nullptr;
			if (O->TryGetObjectField(TEXT("crit"), Crit))
			{
				C.CritBackstab = Num(*Crit, TEXT("backstab"), 2.3f);
				C.CritRiposte = Num(*Crit, TEXT("riposte"), 3.45f);
				C.CritStance = Num(*Crit, TEXT("stanceCrit"), 3.f);
			}
			const TSharedPtr<FJsonObject>* Guard = nullptr;
			C.Guard.Id = C.Id;
			C.Guard.Name = C.Name;
			if (O->TryGetObjectField(TEXT("guard"), Guard))
			{
				C.Guard.Boost = Num(*Guard, TEXT("boost"), 25.f);
				C.Guard.Phys = Num(*Guard, TEXT("phys"), 0.6f);
				C.Guard.Element = Num(*Guard, TEXT("element"), 0.3f);
				ReadParry(*Guard, C.Guard);
			}
			const TSharedPtr<FJsonObject>* MovesObj = nullptr;
			if (O->TryGetObjectField(TEXT("moves"), MovesObj))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& MovePair : (*MovesObj)->Values)
				{
					const TSharedPtr<FJsonObject> R = MovePair.Value->AsObject();
					if (!R.IsValid() || MovePair.Key.StartsWith(TEXT("_"))) continue;
					FWRMove M;
					M.Id = FName(*MovePair.Key);
					M.Windup = Num(R, TEXT("windup"), 0.25f);
					M.Active = Num(R, TEXT("active"), 0.18f);
					M.Recovery = Num(R, TEXT("recovery"), 0.3f);
					M.Damage = Num(R, TEXT("dmg"), 1.f);
					M.Stance = Num(R, TEXT("stance"), 5.f);
					M.Stamina = Num(R, TEXT("stamina"), 10.f);
					M.Poise = Num(R, TEXT("poise"), 30.f);
					M.GuardStamina = Num(R, TEXT("guardStamina"), 30.f);
					M.Impact = uint8(FMath::Clamp(FMath::RoundToInt(Num(R, TEXT("impact"), 2.f)), 1, 5));
					M.Clip = FMath::RoundToInt(Num(R, TEXT("clip"), 0.f));
					M.bHyperarmor = Flag(R, TEXT("ha"));
					M.bHeavy = Flag(R, TEXT("heavy"));
					M.Total = Num(R, TEXT("total"), 0.f);
					M.DamageAt = Num(R, TEXT("damageAt"), 0.f);
					FString S;
					if (R->TryGetStringField(TEXT("next"), S)) M.Next = FName(*S);
					if (R->TryGetStringField(TEXT("tier"), S)) M.Tier = Tier(S);
					// The coil rule (COMBAT-AND-LESSONS L2): never less than a visible wind-up.
					M.Windup = FMath::Max(M.Windup, M.bHeavy ? 0.2f : 0.14f);
					C.Moves.Add(M.Id, M);
				}
			}
			// Every class has the moves the code asks for: missing rows fall back to the sword's.
			for (const TPair<FName, FWRMove>& Base : Fallback.Moves)
			{
				if (!C.Moves.Contains(Base.Key)) C.Moves.Add(Base.Key, Base.Value);
			}
			Classes.Add(C.Id, C);
		}
	}
	if (!Classes.Contains(TEXT("sword")))
	{
		Classes.Add(Fallback.Id, Fallback);
	}

	const TSharedPtr<FJsonObject>* OffObj = nullptr;
	if (Root->TryGetObjectField(TEXT("offhands"), OffObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*OffObj)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			if (!O.IsValid() || Pair.Key.StartsWith(TEXT("_"))) continue;
			FWRGuard G;
			G.Id = FName(*Pair.Key);
			O->TryGetStringField(TEXT("name"), G.Name);
			G.Boost = Num(O, TEXT("guardBoost"), 45.f);
			G.Phys = Num(O, TEXT("phys"), 0.85f);
			G.Element = Num(O, TEXT("element"), 0.4f);
			G.Weight = Num(O, TEXT("weight"), 0.f);
			G.BlockStunScale = Num(O, TEXT("blockStun"), 1.f);
			G.bCanParry = !O->HasField(TEXT("canParry")) || Flag(O, TEXT("canParry"));
			ReadParry(O, G);
			Offhands.Add(G.Id, G);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("WEAPONS %d classes, %d off-hands, carried %s"), Classes.Num(), Offhands.Num(), *CarriedId.ToString());
}

const FWRWeaponClass& FWRWeapons::Class(FName Id) const
{
	if (const FWRWeaponClass* C = Classes.Find(Id.IsNone() ? FName(TEXT("sword")) : Id))
	{
		return *C;
	}
	const FWRWeaponClass* Sword = Classes.Find(TEXT("sword"));
	return Sword ? *Sword : Fallback;
}

const FWRWeaponClass& FWRWeapons::ClassFor(const AWRCharacterBase* Character) const
{
	FName Id;
	if (const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Character))
	{
		const FWRItemStack& Held = Inv->GetEquipped(EWREquipSlot::Weapon);
		if (const FWRItemDef* Def = Held.IsEmpty() ? nullptr : FWRItemRegistry::Get().Find(Held.ItemId))
		{
			Id = Def->WeaponClass;
		}
	}
	return Class(Id);
}

FWRGuard FWRWeapons::GuardFor(const AWRCharacterBase* Character) const
{
	// Survivors carry the left-arm shield (weapons.json carried); creatures and anyone without it guard with the weapon.
	if (Character && Character->GetPlayerState() && !CarriedId.IsNone())
	{
		if (const FWRGuard* G = Offhands.Find(CarriedId))
		{
			return *G;
		}
	}
	return ClassFor(Character).Guard;
}
