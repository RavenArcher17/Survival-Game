// The Spore path's rules (see WRSporePath.h; cooks/biome-fungal.md 2.17-2.18).

#include "WRSporePath.h"
#include "WRFungalHazards.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRNecroPath.h"
#include "WRPathComponent.h"
#include "WRPlantSummon.h"
#include "WRPlayerController.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

namespace WRSporePath
{
	UE_DEFINE_GAMEPLAY_TAG(Passive_Immune, "Passive.ImmuneSystem");
	UE_DEFINE_GAMEPLAY_TAG(Status_SporeArmor, "Status.Spore.Armor");
	UE_DEFINE_GAMEPLAY_TAG(Status_LivingColony, "Status.Spore.LivingColony");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SporeDrift, "Combo.SporeDrift");
	UE_DEFINE_GAMEPLAY_TAG(Combo_PlagueWind, "Combo.PlagueWind");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Combustion, "Combo.Combustion");
	UE_DEFINE_GAMEPLAY_TAG(Combo_AshColony, "Combo.AshColony");
	UE_DEFINE_GAMEPLAY_TAG(Combo_StaticCloud, "Combo.StaticCloud");
	UE_DEFINE_GAMEPLAY_TAG(Combo_NeuralSpores, "Combo.NeuralSpores");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Mycorrhiza, "Combo.Mycorrhiza");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ForestFloor, "Combo.ForestFloor");
	UE_DEFINE_GAMEPLAY_TAG(Combo_HardyColony, "Combo.HardyColony");
	UE_DEFINE_GAMEPLAY_TAG(Combo_IceBloom, "Combo.IceBloom");
	UE_DEFINE_GAMEPLAY_TAG(Combo_InfectedWounds, "Combo.InfectedWounds");
	UE_DEFINE_GAMEPLAY_TAG(Combo_BloodColony, "Combo.BloodColony");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Bog, "Combo.Bog");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SwampColony, "Combo.SwampColony");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FungalPuppets, "Combo.FungalPuppets");
	UE_DEFINE_GAMEPLAY_TAG(Combo_EndlessColony, "Combo.EndlessColony");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Status_SporeSummon, "Status.Spore.Summon");
}

namespace WRSporePathImpl
{
	static const FName SpPathId(TEXT("spore"));
	float SpT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("sporePath.")) + Key, Fallback); }
	float SpR(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("spores.")) + Key, Fallback); }
	float SpSpell(const TCHAR* Spell, const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("spells.%s.%s"), Spell, Key), Fallback); }
	float SpCombo(const TCHAR* Id, const TCHAR* Key, float Fallback) { return FWRComboRegistry::Get().Num(FName(Id), FName(Key), Fallback); }
	double SpNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.0);
	}
	bool SpTagged(const AActor* Who, const FGameplayTag& Tag) { const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who); return C && C->HasTag(Tag); }
	void SpTell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}
	TMap<TWeakObjectPtr<const AActor>, double>& SpRefusals() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	struct FSpOwner { double NextPandemic = 0.0, NextColony = 0.0; TWeakObjectPtr<AWRSporeCloud> Aura; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FSpOwner>& SpOwners() { static TMap<TWeakObjectPtr<AWRCharacterBase>, FSpOwner> M; return M; }
	struct FSpSummon { TWeakObjectPtr<AWRCharacterBase> Owner; double Until = 0.0, Born = 0.0; FName Kind; bool bRisen = false; };
	TMap<TWeakObjectPtr<AWRCreature>, FSpSummon>& SpSummons() { static TMap<TWeakObjectPtr<AWRCreature>, FSpSummon> M; return M; }
	TArray<TWeakObjectPtr<UWRSporeHost>>& SpHosts() { static TArray<TWeakObjectPtr<UWRSporeHost>> L; return L; }
	/** Who detonated on whom lately (Host's credit for a kill by a detonation). */
	TMap<TWeakObjectPtr<const AActor>, TPair<TWeakObjectPtr<AWRCharacterBase>, double>>& SpDetonated() { static TMap<TWeakObjectPtr<const AActor>, TPair<TWeakObjectPtr<AWRCharacterBase>, double>> M; return M; }
	/** The Sporestalker calms under way: the wild one -> (the tamer's name, when the hand went on, where it is). */
	struct FSpCalm { FString Tamer; double Start = 0.0; FVector HandAt = FVector::ZeroVector; };
	TMap<TWeakObjectPtr<AWRCreature>, FSpCalm>& SpCalms() { static TMap<TWeakObjectPtr<AWRCreature>, FSpCalm> M; return M; }
	void SpSetAttr(AWRCharacterBase* Who, const FGameplayAttribute& Attr, float V)
	{
		if (UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr) ASC->SetNumericAttributeBase(Attr, V);
	}
	void SpHeal(AWRCharacterBase* Who, float Amount)
	{
		const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
		if (!A || Amount <= 0.f || !Who->IsAlive()) return;
		SpSetAttr(Who, UWRAttributeSet::GetHealthAttribute(), FMath::Min(A->GetMaxHealth(), A->GetHealth() + Amount));
	}
	/** The survivor a blow is really from (a summon's is its owner's). */
	AWRCharacterBase* SpSource(const AActor* Who)
	{
		if (AWRCharacterBase* O = WRSporePath::SummonOwner(Who)) return O;
		return Cast<AWRCharacterBase>(const_cast<AActor*>(Who));
	}
	AWRCharacterBase* SpTargetOf(AWRCharacterBase* Who)
	{
		if (const AWRPlayerController* PC = Who ? Cast<AWRPlayerController>(Who->GetController()) : nullptr; PC && PC->GetServerLock() && PC->GetServerLock()->IsAlive()) return PC->GetServerLock();
		return Who && Who->CombatTarget.IsValid() && Who->CombatTarget->IsAlive() ? Who->CombatTarget.Get() : nullptr;
	}
	bool SpBoss(const AActor* Who) { return Cast<AWRBoss>(Who) != nullptr; }
	float SpStrength(const AActor* Who)
	{
		const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
		if (!P || !P->IsActivePath(SpPathId)) return 0.f;
		const float S = FMath::SmoothStep(0.08f, 1.f, FMath::Clamp(float(P->GetSpent(SpPathId)) / 100.f, 0.f, 1.f));
		return P->GetMainPath() == SpPathId ? S : S * WRTune(TEXT("pathMarks.subScale"), 0.6f);
	}
}
using namespace WRSporePathImpl;

// ================================================================================================ the path

bool WRSporePath::Walks(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(SpPathId);
}

int32 WRSporePath::Rank(const AActor* Who, FName Node)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	const FWRPathProgress* G = P && P->IsActivePath(SpPathId) ? P->GetProgress(SpPathId) : nullptr;
	return G ? G->GetRank(Node) : 0;
}

int32 WRSporePath::PointsIn(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(SpPathId) ? P->GetSpent(SpPathId) : 0;
}

float WRSporePath::Num(const TCHAR* Key, float Fallback) { return SpT(Key, Fallback); }

// ================================================================================================ Spores

float WRSporePath::SporesOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetSpores() : 0.f;
}

float WRSporePath::MaxSporesOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetMaxSpores() : 100.f;
}

bool WRSporePath::HasSpores(const AActor* Who, float Amount) { return Amount <= 0.f || SporesOf(Who) + 0.01f >= Amount; }

bool WRSporePath::SpendSpores(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority()) return false;
	if (Amount <= 0.f) return true;
	if (!HasSpores(Who, Amount)) return false;
	SpSetAttr(Who, UWRAttributeSet::GetSporesAttribute(), SporesOf(Who) - Amount);
	return true;
}

void WRSporePath::GainSpores(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority() || Amount <= 0.f) return;
	SpSetAttr(Who, UWRAttributeSet::GetSporesAttribute(), FMath::Min(MaxSporesOf(Who), SporesOf(Who) + Amount));
}

void WRSporePath::NoteRefused(const AActor* Who) { if (Who) SpRefusals().Add(Who, SpNow(Who)); }

float WRSporePath::RefusedFlash(const AActor* Who)
{
	const double* At = Who ? SpRefusals().Find(Who) : nullptr;
	if (!At) return 0.f;
	return FMath::Clamp(1.f - float(SpNow(Who) - *At) / 0.6f, 0.f, 1.f);
}

void WRSporePath::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	if (!Who || !Who->HasAuthority() || !Walks(Who) || !Who->IsAlive()) return;
	const double T = SpNow(Who);
	FSpOwner& O = SpOwners().FindOrAdd(Who);
	// The bar: 100, +20 a capacity rank.
	const float Max = SpR(TEXT("max"), 100.f) + UWRPathComponent::Stat(Who, TEXT("maxSpores"));
	if (!FMath::IsNearlyEqual(MaxSporesOf(Who), Max))
	{
		SpSetAttr(Who, UWRAttributeSet::GetMaxSporesAttribute(), Max);
		if (SporesOf(Who) > Max) SpSetAttr(Who, UWRAttributeSet::GetSporesAttribute(), Max);
	}
	// The refill: 5 a second, x2 standing in any spore cloud (the biome feeds you); the Farmer's ring +20%.
	const bool bInCloud = AWRSporeCloud::At(Who, Who->GetActorLocation()) != nullptr;
	GainSpores(Who, SpR(TEXT("regen"), 5.f) * (bInCloud ? SpR(TEXT("cloudRegen"), 2.f) : 1.f) * (1.f + UWRInventoryComponent::GearStatFor(Who, TEXT("sporesRegen"))) * DeltaTime);
	// Regrowth spores: 0.4 HP a second a rank while the bar holds 40 or more.
	if (const float Regrow = UWRPathComponent::Stat(Who, TEXT("regrowth")); Regrow > 0.f && SporesOf(Who) >= SpT(TEXT("regrowthAt"), 40.f)) SpHeal(Who, Regrow * DeltaTime);
	// Living colony: a 5 m aura (a spore cloud that follows you: wind pushes it off you 2 s, fire flashes it): every
	// enemy in it gets a Spore and poison 10 a second; you heal 1.5% a second for each (6% at most; x0.5 for players).
	if (Who->HasTag(Status_LivingColony))
	{
		const float R = SpSpell(TEXT("livingColony"), TEXT("radius"), 500.f);
		AWRSporeCloud* Aura = O.Aura.Get();
		if (!Aura || !Aura->IsLive())
		{
			Aura = AWRSporeCloud::Spawn(Who->GetWorld(), Who->GetActorLocation(), R, TEXT("path"), 0.f, Who);
			O.Aura = Aura;
		}
		if (Aura)
		{
			if (T - Aura->LastWindAt > 2.0) { Aura->Offset = Aura->OffsetFrom = FVector::ZeroVector; }
			Aura->Home = Who->GetActorLocation();
		}
		if (T >= O.NextColony)
		{
			O.NextColony = T + SpSpell(TEXT("livingColony"), TEXT("plantEvery"), 1.f);
			float Heal = 0.f;
			const float Each = SpSpell(TEXT("livingColony"), TEXT("healPerEnemy"), 0.015f);
			for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
			{
				if (!It->IsAlive() || !Who->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) > R + It->GetSimpleCollisionRadius()) continue;
				Plant(Who, *It, 1);
				if (It->Status && WRFungal::SporeTaken(*It) > 0.f) It->Status->AddStatusBuildup(WRStatus::Poison, SpSpell(TEXT("livingColony"), TEXT("poisonPerSecond"), 10.f), Who);
				Heal += Each * (It->GetPlayerState() ? SpSpell(TEXT("livingColony"), TEXT("pvpHeal"), 0.5f) : 1.f);
			}
			if (const UWRAttributeSet* A = Who->GetAttributes()) SpHeal(Who, A->GetMaxHealth() * FMath::Min(Heal, SpSpell(TEXT("livingColony"), TEXT("healMax"), 0.06f)));
		}
	}
	else if (AWRSporeCloud* Aura = O.Aura.Get())
	{
		Aura->Destroy();
		O.Aura = nullptr;
	}
	// Pandemic: every 2 s each rotting enemy near you passes rot to every non-rotting enemy within 6 m (bosses at half),
	// a cyan-rust thread showing each jump.
	if (Rank(Who, TEXT("pandemic")) > 0 && T >= O.NextPandemic)
	{
		O.NextPandemic = T + SpT(TEXT("pandemicEvery"), 2.f);
		TArray<AWRCharacterBase*> Rotting, Clean;
		for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Who->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), Who->GetActorLocation()) > SpT(TEXT("pandemicReach"), 3000.f)) continue;
			(It->HasTag(WRTags::Status_Rot) ? Rotting : Clean).Add(*It);
		}
		for (AWRCharacterBase* From : Rotting)
		{
			for (AWRCharacterBase* To : Clean)
			{
				if (!To->Status || To->HasTag(WRTags::Status_Rot) || FVector::Dist(From->GetActorLocation(), To->GetActorLocation()) > SpT(TEXT("pandemicJump"), 600.f)) continue;
				To->Status->AddStatusBuildup(WRStatus::Rot, SpT(TEXT("pandemicBuildup"), 100.f) * (SpBoss(To) ? SpT(TEXT("pandemicBoss"), 0.5f) : 1.f), Who);
				WRFx::Broadcast(Who, TEXT("spore.thread"), To->GetActorLocation(), (From->GetActorLocation() - To->GetActorLocation()).Rotation(), FMath::Max(1.f, float(FVector::Dist(From->GetActorLocation(), To->GetActorLocation())) / 300.f), TEXT("spore.pandemic"));
			}
		}
	}
	// The summons: their time runs out; Hive mind switches them to your target.
	const bool bHive = Rank(Who, TEXT("hive_mind")) > 0;
	AWRCharacterBase* Mark = bHive ? SpTargetOf(Who) : nullptr;
	for (auto It = SpSummons().CreateIterator(); It; ++It)
	{
		AWRCreature* S = It->Key.Get();
		if (!S || !S->IsAlive()) { It.RemoveCurrent(); continue; }
		if (It->Value.Owner.Get() != Who) continue;
		if (T >= It->Value.Until)
		{
			WRFx::Broadcast(S, TEXT("fungal.miteBurst"), S->GetActorLocation(), FRotator::ZeroRotator, S->GetSimpleCollisionRadius() / 30.f, TEXT("fungal.puff"));
			S->Destroy();
			It.RemoveCurrent();
			continue;
		}
		if (Mark && S->IsHostileTo(Mark)) S->SetAggroTarget(Mark);
		else if (!S->GetAggroTarget())
		{
			float Best = 1500.f;
			AWRCharacterBase* Foe = nullptr;
			for (TActorIterator<AWRCharacterBase> C(Who->GetWorld()); C; ++C)
			{
				if (*C == S || !C->IsAlive() || !S->IsHostileTo(*C)) continue;
				const float D = float(FVector::Dist(C->GetActorLocation(), S->GetActorLocation()));
				if (D < Best) { Best = D; Foe = *C; }
			}
			if (Foe) S->SetAggroTarget(Foe);
		}
	}
	// The Sporestalker's calm under way (path magic): the hand held 4 s finishes it.
	for (auto It = SpCalms().CreateIterator(); It; ++It)
	{
		AWRCreature* Wild = It->Key.Get();
		FSpCalm& C = It->Value;
		if (!Wild) { It.RemoveCurrent(); continue; }
		if (!Who->GetPlayerState() || C.Tamer != Who->GetPlayerState()->GetPlayerName()) continue;
		FString Broke;
		if (!Wild->IsAlive() || Wild->IsTamed()) Broke = TEXT("The calm breaks");
		else if (FVector::Dist2D(Who->GetActorLocation(), C.HandAt) > 150.f) Broke = TEXT("You moved: the calm breaks");
		else if (Who->HasTag(WRTags::State_Dodging) || Who->HasTag(WRTags::State_Attacking)) Broke = TEXT("The calm breaks");
		if (!Broke.IsEmpty()) { SpTell(Who, Broke); It.RemoveCurrent(); continue; }
		Wild->SetAggroTarget(nullptr);
		Wild->GetCharacterMovement()->StopMovementImmediately();
		if (T - C.Start < SpT(TEXT("pathMagic.seconds"), 4.f)) continue;
		It.RemoveCurrent();
		if (!SpendSpores(Who, SpT(TEXT("pathMagic.spores"), 10.f)))
		{
			SpTell(Who, FString::Printf(TEXT("You haven't the Spores (%.0f) to finish the calm"), SpT(TEXT("pathMagic.spores"), 10.f)));
			continue;
		}
		// Ten of your Spores planted on it and burst softly into its stalks: a full meal of narcotic.
		if (UWRSporeHost* H = UWRSporeHost::FindOrAdd(Wild)) H->SetCount(Who, int32(SpT(TEXT("pathMagic.spores"), 10.f)), T + 5.0);
		Detonate(Who, 0.f, true, Wild);
		Wild->AddTorpor(Wild->MaxTorpor * 2.f);
		Wild->Torpor = Wild->MaxTorpor;
		SpTell(Who, TEXT("Your Spores soften in its stalks and the crest lies flat: feed it to finish the tame"));
		UE_LOG(LogWildreach, Log, TEXT("SPORE path-magic calm done on %s by %s"), *Wild->GetName(), *Who->GetName());
	}
	(void)DeltaTime;
}

// ================================================================================================ planted Spores

int32 WRSporePath::Plant(AWRCharacterBase* By, AWRCharacterBase* Target, int32 N)
{
	if (!By || !Target || !By->HasAuthority() || N <= 0 || !Target->IsAlive() || !By->IsHostileTo(Target)) return 0;
	// Bog (Tide + Spore 25): Spores grow faster on the soaked.
	if (By->HasTag(Combo_Bog) && Target->HasTag(WRTags::Status_Soak)) N *= int32(SpCombo(TEXT("bog"), TEXT("plantMult"), 2.f));
	const int32 Cap = Target->GetPlayerState() ? int32(SpR(TEXT("playerMax"), 5.f)) : int32(SpR(TEXT("maxPlanted"), 10.f) + UWRPathComponent::Stat(By, TEXT("maxPlanted")));
	UWRSporeHost* H = UWRSporeHost::FindOrAdd(Target);
	if (!H) return 0;
	const int32 Was = H->Of(By);
	const int32 Now = FMath::Min(Cap, Was + N);
	H->SetCount(By, Now, SpNow(By) + SpR(TEXT("plantSeconds"), 20.f));
	if (Now > Was)
	{
		WRFx::Broadcast(Target, TEXT("spore.plant"), Target->GetActorLocation() + FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight() * 0.3f), FRotator::ZeroRotator, 1.f, TEXT("spore.plant"));
	}
	return Now - Was;
}

int32 WRSporePath::PlantedBy(const AActor* Target, const AActor* By)
{
	const UWRSporeHost* H = UWRSporeHost::Get(Target);
	return H ? H->Of(By) : 0;
}

int32 WRSporePath::Detonate(AWRCharacterBase* By, float Range, bool bSoft, AWRCharacterBase* Only)
{
	if (!By || !By->HasAuthority()) return 0;
	const double T = SpNow(By);
	const float Reach = Range * (By->HasTag(Combo_SporeDrift) ? SpCombo(TEXT("spore_drift"), TEXT("reach"), 1.5f) : 1.f);
	int32 Total = 0;
	TWeakObjectPtr<AWRCharacterBase> WeakBy(By);
	for (const TWeakObjectPtr<UWRSporeHost>& W : SpHosts())
	{
		UWRSporeHost* H = W.Get();
		AWRCharacterBase* Target = H ? Cast<AWRCharacterBase>(H->GetOwner()) : nullptr;
		if (!Target || (Only && Target != Only)) continue;
		const int32 N = H->Of(By);
		if (N <= 0) continue;
		if (!Only && FVector::Dist(Target->GetActorLocation(), By->GetActorLocation()) > Reach) continue;
		H->SetCount(By, 0, 0.0);
		Total += N;
		if (!bSoft) SpDetonated().Add(Target, TPair<TWeakObjectPtr<AWRCharacterBase>, double>(By, T + 2.0));
		// Every nodule bursts in a chain 0.05 s apart (cyan puffs, a popping chain like seed pods).
		TWeakObjectPtr<AWRCharacterBase> WeakT(Target);
		for (int32 k = 0; k < N; ++k)
		{
			FTimerHandle Hd;
			By->GetWorldTimerManager().SetTimer(Hd, [WeakBy, WeakT, bSoft]()
			{
				AWRCharacterBase* B = WeakBy.Get();
				AWRCharacterBase* Tg = WeakT.Get();
				if (!B || !Tg) return;
				const FVector At = Tg->GetActorLocation() + FMath::VRand() * Tg->GetSimpleCollisionRadius() * 0.7f;
				WRFx::Broadcast(Tg, TEXT("spore.burst"), At, FRotator::ZeroRotator, 1.f, TEXT("spore.pop"));
				if (bSoft || !Tg->IsAlive()) return;
				FWRHit Hit;
				Hit.Instigator = B;
				Hit.Damage = SpSpell(TEXT("detonate"), TEXT("damage"), 8.f) * (Tg->GetPlayerState() ? SpR(TEXT("pvpDetonate"), 0.6f) : 1.f);
				Hit.Element = WRTags::Element_Physical;
				Hit.PathId = SpPathId;
				Hit.bSpell = true;
				Hit.bMelee = false;
				Hit.bUnblockable = true;
				Hit.bDamageOverTime = false;
				Hit.Origin = At;
				Hit.Buildups.Add(TEXT("rot"), SpSpell(TEXT("detonate"), TEXT("rot"), 10.f));
				// Ice bloom (Frost + Spore 50): the burst is ice shards.
				if (B->HasTag(Combo_IceBloom)) { Hit.FrostBuildup += SpCombo(TEXT("ice_bloom"), TEXT("frost"), 15.f); Hit.Element = WRTags::Element_Frost; }
				UWRCombatLibrary::ApplyHit(Tg, Hit);
				// Burst: rot 6 on every other enemy within 3 m of it (rust threads spraying out).
				if (WRSporePath::Rank(B, TEXT("burst")) > 0)
				{
					for (TActorIterator<AWRCharacterBase> It(B->GetWorld()); It; ++It)
					{
						if (*It == Tg || !It->IsAlive() || !It->Status || !B->IsHostileTo(*It) || FVector::Dist(It->GetActorLocation(), At) > SpT(TEXT("burstRadius"), 300.f)) continue;
						It->Status->AddStatusBuildup(WRStatus::Rot, SpT(TEXT("burstRot"), 6.f), B);
						WRFx::Broadcast(B, TEXT("spore.thread"), It->GetActorLocation(), (At - It->GetActorLocation()).Rotation(), 1.f, TEXT("spore.hiss"));
					}
				}
			}, 0.02f + k * SpSpell(TEXT("detonate"), TEXT("chainGap"), 0.05f), false);
		}
	}
	if (Total > 0 && !bSoft)
	{
		// +3 Spores a burst; Adrenal: +2 stamina a Spore (30 at most), a pale gold flush on your arms.
		GainSpores(By, SpSpell(TEXT("detonate"), TEXT("gain"), 3.f) * Total);
		if (Rank(By, TEXT("adrenal")) > 0)
		{
			if (const UWRAttributeSet* A = By->GetAttributes()) SpSetAttr(By, UWRAttributeSet::GetStaminaAttribute(), FMath::Min(A->GetMaxStamina(), A->GetStamina() + FMath::Min(SpT(TEXT("adrenalMax"), 30.f), SpT(TEXT("adrenal"), 2.f) * Total)));
			Proc(By, TEXT("spore.adrenal"), TEXT("spore.breath"));
		}
	}
	return Total;
}

// ================================================================================================ the combat code's questions

void WRSporePath::ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit)
{
	if (!Attacker || !Walks(Attacker)) return;
	// Rot chance: +10% rot buildup a rank on every blow, and +5 rot on weapon hits (any weapon can rot).
	const int32 R = Rank(Attacker, TEXT("rot_chance"));
	if (R > 0)
	{
		if (float* Rot = Hit.Buildups.Find(TEXT("rot"))) *Rot *= 1.f + UWRPathComponent::Stat(Attacker, TEXT("rotBuildup"));
		if (Hit.bMelee && !Hit.bSpell) Hit.Buildups.FindOrAdd(TEXT("rot")) += UWRPathComponent::Stat(Attacker, TEXT("weaponRot"));
	}
	// Plague wind (Wind + Spore 50): your wind spells spread rot.
	if (Hit.PathId == TEXT("wind") && Attacker->HasTag(Combo_PlagueWind)) Hit.Buildups.FindOrAdd(TEXT("rot")) += SpCombo(TEXT("plague_wind"), TEXT("rot"), 20.f);
}

namespace WRSporePathImpl
{
	TMap<TWeakObjectPtr<const AActor>, double>& SpInfect() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
}

void WRSporePath::MarkInfect(const AActor* Who, float Seconds)
{
	if (Who) SpInfect().Add(Who, SpNow(Who) + Seconds);
}

void WRSporePath::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	if (!Target || !Target->HasAuthority()) return;
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	const double T = SpNow(Target);
	if (Attacker && Attacker != Target && Damage > 0.f && !Hit.bDamageOverTime)
	{
		// Infect strike's blow plants 3.
		if (const double* Until = SpInfect().Find(Attacker); Until && T <= *Until && Hit.bMelee) Plant(Attacker, Target, int32(SpSpell(TEXT("infectStrike"), TEXT("plants"), 3.f)));
		// A summon's bite plants its owner's Spores (a sporeling 1, the behemoth's slam 2); Ash colony's bites burn,
		// Swamp colony's soak.
		if (AWRCharacterBase* Owner = SummonOwner(Attacker))
		{
			const AWRCreature* S = Cast<AWRCreature>(Attacker);
			const bool bGiant = S && S->CreatureId == TEXT("fungal_behemoth");
			Plant(Owner, Target, bGiant ? int32(SpSpell(TEXT("fungalBehemoth"), TEXT("plants"), 2.f)) : 1);
			if (Target->Status && Owner->HasTag(Combo_AshColony)) Target->Status->AddStatusBuildup(WRStatus::Burn, SpCombo(TEXT("ash_colony"), TEXT("burn"), 10.f), Owner);
			if (Target->Status && Owner->HasTag(Combo_SwampColony)) Target->Status->AddStatusBuildup(WRStatus::Soak, SpCombo(TEXT("swamp_colony"), TEXT("soak"), 15.f), Owner);
		}
		// Mycorrhiza (Nature + Spore 25): your plants' blows plant Spores.
		if (const AWRPlantSummon* Plantling = Cast<AWRPlantSummon>(Attacker); Plantling && Plantling->OwnerPawn && Plantling->OwnerPawn->HasTag(Combo_Mycorrhiza)) Plant(Plantling->OwnerPawn, Target, 1);
		// Toxic blood: a melee attacker that hurts you gets 2 Spores and poison 15 (cyan motes from the wound).
		if (Hit.bMelee && Rank(Target, TEXT("toxic_blood")) > 0 && Attacker->IsAlive() && Target->IsHostileTo(Attacker))
		{
			Plant(Target, Attacker, int32(SpT(TEXT("toxicBloodPlants"), 2.f)));
			if (Attacker->Status) Attacker->Status->AddStatusBuildup(WRStatus::Poison, SpT(TEXT("toxicBloodPoison"), 15.f), Target);
			Proc(Target, TEXT("spore.toxic"), TEXT("fungal.puff"));
		}
		const int32 Mine = PlantedBy(Target, Attacker);
		// Combustion (Fire + Spore 25): fire on your Spores detonates them in flame.
		if (Mine > 0 && Attacker->HasTag(Combo_Combustion) && (Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f)) Detonate(Attacker, 0.f, false, Target);
		// Neural spores (Lightning + Spore 50): lightning detonates them and stuns.
		if (Mine > 0 && Attacker->HasTag(Combo_NeuralSpores) && Hit.Element == WRTags::Element_Lightning)
		{
			Detonate(Attacker, 0.f, false, Target);
			if (Target->Status && !SpBoss(Target)) Target->Status->Stagger(SpCombo(TEXT("neural_spores"), TEXT("stun"), 1.f));
		}
		// Infected wounds (Blood + Spore 25): bleeding enemies take your Spores.
		if (Attacker->HasTag(Combo_InfectedWounds) && Target->HasTag(WRTags::Status_Bleeding)) Plant(Attacker, Target, 1);
	}
}

float WRSporePath::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker || !Target) return 1.f;
	float S = 1.f;
	const AWRCharacterBase* Src = SpSource(Attacker);
	// Corrode: a rotting target's physical resist down 15% (bosses 10%) to the corroder's blows.
	if (Src && Hit.PhysType > 0 && Target->HasTag(WRTags::Status_Rot) && Rank(Src, TEXT("corrode")) > 0) S *= SpBoss(Target) ? SpT(TEXT("corrodeBoss"), 1.1f) : SpT(TEXT("corrode"), 1.15f);
	if (AWRCharacterBase* Owner = SummonOwner(Attacker))
	{
		// Hive mind: +20% to your target; Forest floor: plants and sporelings +15%; Blood colony: +25% on the bleeding.
		if (Rank(Owner, TEXT("hive_mind")) > 0 && SpTargetOf(Owner) == Target) S *= 1.f + SpT(TEXT("hiveDamage"), 0.2f);
		if (Owner->HasTag(Combo_ForestFloor)) S *= SpCombo(TEXT("forest_floor"), TEXT("damage"), 1.15f);
		if (Owner->HasTag(Combo_BloodColony) && Target->HasTag(WRTags::Status_Bleeding)) S *= SpCombo(TEXT("blood_colony"), TEXT("damage"), 1.25f);
	}
	if (const AWRPlantSummon* Plantling = Cast<AWRPlantSummon>(Attacker); Plantling && Plantling->OwnerPawn && Plantling->OwnerPawn->HasTag(Combo_ForestFloor)) S *= SpCombo(TEXT("forest_floor"), TEXT("damage"), 1.15f);
	return S;
}

float WRSporePath::TakenScale(const AWRCharacterBase* Who, const FWRHit& Hit)
{
	if (!Who) return 1.f;
	float S = 1.f;
	if (Who->HasTag(Status_SporeArmor)) S *= SpSpell(TEXT("sporeArmor"), TEXT("taken"), 0.85f);
	if (AWRCharacterBase* Owner = SummonOwner(Who))
	{
		// Ash colony: sporelings immune to fire; Hardy colony: they shrug off cold (half).
		if (Owner->HasTag(Combo_AshColony) && (Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f)) S = 0.f;
		if (Owner->HasTag(Combo_HardyColony) && Hit.Element == WRTags::Element_Frost) S *= 0.5f;
	}
	return S;
}

float WRSporePath::BuildupTakenScale(const AActor* Who, int32 StatusId)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	if (!P) return 1.f;
	// Immune system (the root passive, main path only): no poison, no rot.
	if (P->GetMainPath() == SpPathId && (StatusId == WRStatus::Poison || StatusId == WRStatus::Rot)) return 0.f;
	if (!P->IsActivePath(SpPathId)) return 1.f;
	const int32 R = Rank(Who, TEXT("bloom_body"));
	return R > 0 ? FMath::Pow(0.9f, float(R)) : 1.f;
}

float WRSporePath::RotSecondsScale(const AActor* Source)
{
	return Source ? 1.f + UWRPathComponent::Stat(SpSource(Source), TEXT("rotDuration")) : 1.f;
}

float WRSporePath::RotDamageScale(const AActor* Source)
{
	return Source ? 1.f + UWRPathComponent::Stat(SpSource(Source), TEXT("rotDamage")) : 1.f;
}

// ================================================================================================ summons

bool WRSporePath::IsSummon(const AActor* Who)
{
	const AWRCreature* C = Cast<AWRCreature>(Who);
	if (!C) return false;
	if (C->HasAuthority()) return SpSummons().Contains(const_cast<AWRCreature*>(C));
	return C->HasTag(Status_SporeSummon);
}

AWRCharacterBase* WRSporePath::SummonOwner(const AActor* Who)
{
	const AWRCreature* C = Cast<AWRCreature>(Who);
	const FSpSummon* S = C ? SpSummons().Find(const_cast<AWRCreature*>(C)) : nullptr;
	return S ? S->Owner.Get() : nullptr;
}

int32 WRSporePath::SummonCap(const AActor* Owner)
{
	return FMath::Min(int32(SpT(TEXT("swarmMax"), 6.f)), 1 + int32(UWRPathComponent::Stat(Owner, TEXT("sporelings"))));
}

AWRCreature* WRSporePath::Summon(AWRCharacterBase* Owner, FName Kind, float Seconds, const FVector& At)
{
	if (!Owner || !Owner->HasAuthority() || !Owner->GetPlayerState()) return nullptr;
	const bool bGiant = Kind == TEXT("fungal_behemoth");
	// Sporelings: at most 1 + a Swarm rank (6) at once; the oldest goes.
	if (!bGiant)
	{
		TArray<TPair<double, AWRCreature*>> Mine;
		for (const auto& KV : SpSummons()) if (KV.Key.IsValid() && KV.Value.Owner.Get() == Owner && KV.Value.Kind == TEXT("sporeling")) Mine.Add({ KV.Value.Born, KV.Key.Get() });
		Mine.Sort([](const TPair<double, AWRCreature*>& A, const TPair<double, AWRCreature*>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i <= Mine.Num() - SummonCap(Owner); ++i) { SpSummons().Remove(Mine[i].Value); Mine[i].Value->Destroy(); }
	}
	else
	{
		for (auto It = SpSummons().CreateIterator(); It; ++It) if (It->Value.Owner.Get() == Owner && It->Value.Kind == Kind && It->Key.IsValid()) { It->Key->Destroy(); It.RemoveCurrent(); }
	}
	FHitResult G;
	FVector Spot = At;
	if (Owner->GetWorld()->LineTraceSingleByChannel(G, At + FVector(0.f, 0.f, 300.f), At - FVector(0.f, 0.f, 1500.f), ECC_WorldStatic)) Spot = G.ImpactPoint;
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(bGiant ? FName(TEXT("fungal_behemoth")) : FName(TEXT("sporeling")));
	AWRCreature* M = AWRCreature::SpawnCreature(Owner->GetWorld(), bGiant ? FName(TEXT("fungal_behemoth")) : FName(TEXT("sporeling")), Spot + FVector(0.f, 0.f, (Def ? Def->HalfHeight : 40.f) + 20.f), FRotator(0.f, Owner->GetActorRotation().Yaw, 0.f));
	if (!M) return nullptr;
	// Its summoner's: it follows them and fights what they fight, but it's never a tame (no limit, no save, no body).
	M->TamedBy = Owner->GetPlayerState()->GetPlayerName();
	M->TameOrder = EWRTameOrder::Follow;
	M->bTameAggressive = true;
	const float Thick = 1.f + UWRPathComponent::Stat(Owner, TEXT("sporelingHealth"));
	const float HP = (bGiant ? SpSpell(TEXT("fungalBehemoth"), TEXT("health"), 500.f) : SpSpell(TEXT("sporeling"), TEXT("health"), 60.f)) * Thick;
	if (UAbilitySystemComponent* ASC = M->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(WRSporePath::Status_SporeSummon, 1, EGameplayTagReplicationState::TagOnly);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetMaxHealthAttribute(), HP);
		ASC->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), HP);
	}
	// The bite 10 (a row x0.5), the slam 30 (a row x1.0).
	M->Weapon.FlatDamage = bGiant ? SpSpell(TEXT("fungalBehemoth"), TEXT("slam"), 30.f) : SpSpell(TEXT("sporeling"), TEXT("bite"), 10.f) * 2.f;
	FSpSummon S;
	S.Owner = Owner;
	S.Until = SpNow(Owner) + Seconds;
	S.Born = SpNow(Owner);
	S.Kind = bGiant ? FName(TEXT("fungal_behemoth")) : FName(TEXT("sporeling"));
	SpSummons().Add(M, S);
	M->SetLifeSpan(Seconds + 10.f);
	if (AWRCharacterBase* Mark = SpTargetOf(Owner)) M->SetAggroTarget(Mark);
	WRFx::Broadcast(M, TEXT("fungal.puff"), Spot, FRotator::ZeroRotator, bGiant ? 3.f : 1.f, bGiant ? TEXT("spore.behemoth") : TEXT("creature.sporeling.signature"));
	UE_LOG(LogWildreach, Log, TEXT("SPORE %s grows a %s (%.0f s, %.0f HP)"), *Owner->GetName(), *S.Kind.ToString(), Seconds, HP);
	return M;
}

void WRSporePath::CreatureDied(AWRCreature* Dead, AActor* Killer)
{
	if (!Dead || !Dead->HasAuthority()) return;
	const double T = SpNow(Dead);
	// A summon falls: Endless colony raises a sporeling once more.
	if (FSpSummon* S = SpSummons().Find(Dead))
	{
		AWRCharacterBase* Owner = S->Owner.Get();
		const bool bAgain = Owner && Owner->HasTag(Combo_EndlessColony) && !S->bRisen && S->Kind == TEXT("sporeling");
		const float Left = float(S->Until - T);
		SpSummons().Remove(Dead);
		if (bAgain && Left > 1.f)
		{
			if (AWRCreature* Again = Summon(Owner, TEXT("sporeling"), Left, Dead->GetActorLocation())) if (FSpSummon* S2 = SpSummons().Find(Again)) S2->bRisen = true;
		}
		return;
	}
	// Host: a kill on an enemy carrying any of your Spores (or killed by your detonation) spawns a free sporeling for
	// 15 s (it counts toward the cap); Fungal puppets (Necromancy + Spore 25) raise it as a fungal minion instead.
	TSet<AWRCharacterBase*> Planters;
	if (const UWRSporeHost* H = UWRSporeHost::Get(Dead)) for (const FWRPlantedSpores& P : H->Planted) if (P.Count > 0) if (AWRCharacterBase* C = Cast<AWRCharacterBase>(P.Planter)) Planters.Add(C);
	if (const TPair<TWeakObjectPtr<AWRCharacterBase>, double>* D = SpDetonated().Find(Dead); D && T <= D->Value && D->Key.IsValid()) Planters.Add(D->Key.Get());
	SpDetonated().Remove(Dead);
	for (AWRCharacterBase* P : Planters)
	{
		if (!P->IsAlive()) continue;
		if (P->HasTag(Combo_FungalPuppets) && WRNecro::CanRaise(Dead, P))
		{
			TWeakObjectPtr<AWRCharacterBase> WeakP(P);
			TWeakObjectPtr<AWRCreature> WeakD(Dead);
			FTimerHandle Hd;
			Dead->GetWorldTimerManager().SetTimer(Hd, [WeakP, WeakD]() { if (WeakP.IsValid() && WeakD.IsValid()) WRNecro::Raise(WeakP.Get(), WeakD.Get(), SpCombo(TEXT("fungal_puppets"), TEXT("seconds"), 20.f), false); }, 0.3f, false);
			continue;
		}
		if (Rank(P, TEXT("host")) > 0)
		{
			Summon(P, TEXT("sporeling"), SpT(TEXT("hostSeconds"), 15.f), Dead->GetActorLocation());
			Proc(Dead, TEXT("fungal.eggHatch"), TEXT("creature.sporeling.signature"));
		}
	}
	(void)Killer;
}

void WRSporePath::Proc(AActor* At, FName Fx, FName Sound, float Scale)
{
	if (!At || !At->HasAuthority()) return;
	WRFx::Broadcast(At, Fx, At->GetActorLocation(), At->GetActorRotation(), Scale, Sound);
}

// ================================================================================================ the Sporestalker's calm

bool WRSporePath::PathTamePrompt(const AWRCreature* Wild, const AActor* Who, FString& OutVerb)
{
	const FWRCreatureDef* Def = Wild ? Wild->GetDef() : nullptr;
	if (!Def || !Who || Wild->CreatureId != TEXT("sporestalker") || Wild->IsTamed() || !Wild->IsAlive() || !Def->CanTameBy(TEXT("path_magic"))) return false;
	const APawn* P = Cast<APawn>(Who);
	const FString Me = P && P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString();
	for (const auto& KV : SpCalms()) if (KV.Key.Get() == Wild) { if (KV.Value.Tamer != Me) return false; OutVerb = TEXT("Hold still: your Spores are settling in its stalks"); return true; }
	// Knocked out, or under 30% and rotting; 10+ Spore points; the Spores to plant.
	const bool bOpen = Wild->IsUnconscious() || (Wild->GetHealthFraction() < SpT(TEXT("pathMagic.below"), 0.3f) && Wild->HasTag(WRTags::Status_Rot));
	if (!bOpen || PointsIn(Who) < int32(SpT(TEXT("pathMagic.points"), 10.f)) || !HasSpores(Who, SpT(TEXT("pathMagic.spores"), 10.f))) return false;
	OutVerb = FString::Printf(TEXT("Hold: calm it with your Spores (%.0f s, %.0f Spores)"), SpT(TEXT("pathMagic.seconds"), 4.f), SpT(TEXT("pathMagic.spores"), 10.f));
	return true;
}

bool WRSporePath::TryPathTame(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult)
{
	AWRCharacter* Who = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	FString Verb;
	if (!Wild || !Who || !Wild->HasAuthority() || !PathTamePrompt(Wild, Who, Verb)) return false;
	const FString Me = Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (const FSpCalm* C = SpCalms().Find(Wild); C && C->Tamer == Me) { OutResult = Verb; return true; }
	if (FVector::Dist2D(Who->GetActorLocation(), Wild->GetActorLocation()) > Wild->GetSimpleCollisionRadius() + SpT(TEXT("pathMagic.reach"), 400.f))
	{
		OutResult = TEXT("Get your hand on its crest");
		return true;
	}
	FSpCalm C;
	C.Tamer = Me;
	C.Start = SpNow(Wild);
	C.HandAt = Who->GetActorLocation();
	SpCalms().Add(Wild, C);
	Wild->SetAggroTarget(nullptr);
	WRFx::BroadcastOn(Who, TEXT("spore.cast"), TEXT("hand"), 1.2f, TEXT("spore.plant"), SpT(TEXT("pathMagic.seconds"), 4.f));
	OutResult = FString::Printf(TEXT("Hold E: plant your Spores in its stalks (%.0f s)"), SpT(TEXT("pathMagic.seconds"), 4.f));
	return true;
}

void WRSporePath::StopPathTame(AWRPlayerController* Player)
{
	const FString Me = Player && Player->PlayerState ? Player->PlayerState->GetPlayerName() : FString();
	if (Me.IsEmpty()) return;
	for (auto It = SpCalms().CreateIterator(); It; ++It)
	{
		if (It->Value.Tamer != Me) continue;
		const double Held = SpNow(Player) - It->Value.Start;
		AWRCreature* Wild = It->Key.Get();
		It.RemoveCurrent();
		Player->ClientNotify(Held < 0.35 && Wild ? Wild->Feed(Player) : FString(TEXT("You let go: the calm fades")));
		return;
	}
}

bool WRSporePath::PathTameNear(const APlayerController* Player)
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Pawn) return false;
	// The owning machine can't see the server's calm list: a Sporestalker it's holding E at, open to the calm.
	for (TActorIterator<AWRCreature> It(Pawn->GetWorld()); It; ++It)
	{
		FString Verb;
		if (It->CreatureId == TEXT("sporestalker") && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < 800.f && PathTamePrompt(*It, Pawn, Verb)) return true;
	}
	return false;
}

// ================================================================================================ the body

uint32 WRSporePath::LookSig(const AActor* Hero)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Hero);
	const bool bDark = WRFungal::UnderRock(Hero ? Hero->GetActorLocation() : FVector::ZeroVector) || AWRGameState::IsNightIn(Hero);
	return (bDark ? 1u : 0u) | (C && C->HasTag(Status_LivingColony) ? 2u : 0u);
}

void WRSporePath::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS)
{
	// The freckles glow only in the dark (caves, night: 0.8 nits); at full strength they shimmer in a slow 0.3 Hz wave
	// like a breathing constellation; Living colony blazes them.
	if (Main != SpPathId && Sub != SpPathId) return;
	const FString Prefix = Main == SpPathId ? TEXT("Main") : TEXT("Sub");
	const float S = Main == SpPathId ? MainS : SubS;
	const uint32 Sig = LookSig(Hero);
	const bool bDark = (Sig & 1u) != 0, bColony = (Sig & 2u) != 0;
	for (UMaterialInstanceDynamic* M : Skin)
	{
		if (!M) continue;
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("EmissiveNits"))), bColony ? 2.f : bDark ? SpT(TEXT("lookDark"), 0.6f) + 0.2f * S : 0.f);
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseHz"))), bDark && S >= 0.99f ? SpT(TEXT("lookPulseHz"), 0.3f) : 0.f);
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseAmp"))), bDark && S >= 0.99f ? 0.25f : 0.f);
	}
}

// ================================================================================================ UWRSporeHost

UWRSporeHost::UWRSporeHost()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

void UWRSporeHost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRSporeHost, Planted);
}

UWRSporeHost* UWRSporeHost::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRSporeHost>() : nullptr;
}

UWRSporeHost* UWRSporeHost::FindOrAdd(AActor* Actor)
{
	if (!Actor) return nullptr;
	if (UWRSporeHost* H = Get(Actor)) return H;
	if (!Actor->HasAuthority()) return nullptr;
	UWRSporeHost* H = NewObject<UWRSporeHost>(Actor, TEXT("SporeHost"));
	H->RegisterComponent();
	SpHosts().RemoveAll([](const TWeakObjectPtr<UWRSporeHost>& W) { return !W.IsValid(); });
	SpHosts().Add(H);
	return H;
}

void UWRSporeHost::SetCount(AActor* By, int32 N, double Until)
{
	for (int32 i = 0; i < Planted.Num(); ++i)
	{
		if (Planted[i].Planter != By) continue;
		if (N <= 0) Planted.RemoveAt(i);
		else { Planted[i].Count = uint8(FMath::Clamp(N, 0, 255)); Planted[i].Until = Until; }
		Changed();
		return;
	}
	if (N <= 0) return;
	FWRPlantedSpores P;
	P.Planter = By;
	P.Count = uint8(FMath::Clamp(N, 0, 255));
	P.Until = Until;
	Planted.Add(P);
	Changed();
}

void UWRSporeHost::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* O = GetOwner();
	if (!O || !O->HasAuthority()) return;
	// Each planter's Spores last 20 s past their last plant; a dead host's go with it.
	const double T = SpNow(O);
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(O);
	const int32 Was = Planted.Num();
	Planted.RemoveAll([T, C](const FWRPlantedSpores& P) { return !P.Planter || T >= P.Until || (C && !C->IsAlive()); });
	if (Planted.Num() != Was) Changed();
}

void UWRSporeHost::OnRep_Planted()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	AActor* O = GetOwner();
	const int32 N = FMath::Min(13, Total());
	if (!O || N == Shown) return;
	Shown = N;
	// A small cyan nodule per Spore on the body's surface (visible to all), spread over its upper half.
	if (!Nodules)
	{
		Nodules = NewObject<UProceduralMeshComponent>(O);
		Nodules->SetupAttachment(O->GetRootComponent());
		Nodules->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Nodules->RegisterComponent();
	}
	FWRToolMesh M;
	FRandomStream Rng(int32(GetTypeHash(O->GetName())));
	const float R = O->GetSimpleCollisionRadius(), H = O->GetSimpleCollisionHalfHeight();
	for (int32 i = 0; i < N; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI), Z = Rng.FRandRange(-0.2f, 0.8f) * H;
		const FVector At(FMath::Cos(A) * R * 0.95f, FMath::Sin(A) * R * 0.95f, Z);
		const FVector Out = FVector(FMath::Cos(A), FMath::Sin(A), 0.2f).GetSafeNormal();
		M.Lathe(At - Out * 2.f, Out, 9.f, { FVector(0.f, 4.5f, 4.5f), FVector(0.5f, 5.f, 5.f), FVector(1.f, 0.f, 0.f) }, 8, FLinearColor(FColor(57, 230, 194)), 1.f);
	}
	if (N == 0) { Nodules->ClearAllMeshSections(); return; }
	M.ApplyTo(Nodules);
	if (UMaterialInterface* G = WRFungalLook::GlowMaterial(O)) Nodules->SetMaterial(0, G);
}
