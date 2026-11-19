#include "WRSunPath.h"
#include "Wildreach.h"
#include "WRAttributeSet.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCombos.h"
#include "WRCreature.h"
#include "WRDesertTypes.h"
#include "WRDesertWorld.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRGearPerks.h"
#include "WRLand.h"
#include "WRPathComponent.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WRSun
{
	UE_DEFINE_GAMEPLAY_TAG(Passive_Sandwalker, "Passive.Sandwalker");
	UE_DEFINE_GAMEPLAY_TAG(Combo_DesertHeat, "Combo.DesertHeat");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Supernova, "Combo.Supernova");
	UE_DEFINE_GAMEPLAY_TAG(Combo_Thunderflash, "Combo.Thunderflash");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SolarStorm, "Combo.SolarStorm");
	UE_DEFINE_GAMEPLAY_TAG(Combo_DesertBloom, "Combo.DesertBloom");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SunGarden, "Combo.SunGarden");
	UE_DEFINE_GAMEPLAY_TAG(Combo_FrozenSand, "Combo.FrozenSand");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SunAndSnow, "Combo.SunAndSnow");
	UE_DEFINE_GAMEPLAY_TAG(Combo_DryBlood, "Combo.DryBlood");
	UE_DEFINE_GAMEPLAY_TAG(Combo_SunSacrifice, "Combo.SunSacrifice");
	UE_DEFINE_GAMEPLAY_TAG(Combo_DesertGraves, "Combo.DesertGraves");
	UE_DEFINE_GAMEPLAY_TAG(Combo_ScarabSwarm, "Combo.ScarabSwarm");
}

namespace WRSunPathImpl
{
	static const FName SuPathId(TEXT("sun"));
	float SuT(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("sun.path.")) + Key, Fallback); }
	float SuR(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString(TEXT("sun.radiance.")) + Key, Fallback); }
	float SuCombo(const TCHAR* Id, const TCHAR* Key, float Fallback) { return FWRComboRegistry::Get().Num(FName(Id), FName(Key), Fallback); }
	bool SuTagged(const AActor* Who, const FGameplayTag& Tag)
	{
		const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
		return C && C->HasTag(Tag);
	}
	void SuSetAttr(AWRCharacterBase* Who, const FGameplayAttribute& Attr, float V)
	{
		if (UAbilitySystemComponent* ASC = Who ? Who->GetAbilitySystemComponent() : nullptr) ASC->SetNumericAttributeBase(Attr, V);
	}
	double SuNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.0);
	}
	TMap<TWeakObjectPtr<const AActor>, double>& SuRefusals() { static TMap<TWeakObjectPtr<const AActor>, double> M; return M; }
	/** Server: per survivor, the exposure trace's answer (traced every 0.5 s), Supernova's wait. */
	struct FSuOwner { float TraceClock = 0.f; bool bExposed = false; double NovaReady = 0.0; };
	TMap<TWeakObjectPtr<AWRCharacterBase>, FSuOwner>& SuOwners() { static TMap<TWeakObjectPtr<AWRCharacterBase>, FSuOwner> M; return M; }
	/** Server: who was blind before this blow (the proc's flash shows on the onset only). */
	TSet<TWeakObjectPtr<const AActor>>& SuWasBlind() { static TSet<TWeakObjectPtr<const AActor>> S; return S; }
	bool SuBoss(const AActor* Who) { return Cast<AWRBoss>(Who) != nullptr; }
	float SuStrength(const AActor* Who)
	{
		const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
		if (!P || !P->IsActivePath(SuPathId)) return 0.f;
		const float Max = FMath::Max(1.f, SuT(TEXT("lookMaxPoints"), 60.f));
		const float S = FMath::SmoothStep(0.08f, 1.f, FMath::Clamp(float(P->GetSpent(SuPathId)) / Max, 0.f, 1.f));
		return P->GetMainPath() == SuPathId ? S : S * WRTune(TEXT("pathMarks.subScale"), 0.6f);
	}
	/** The biome whose weather falls on a point (a land's, else the island's two halves). */
	FName SuWeatherBiome(const AActor* Who)
	{
		const FName B = WRLand::BiomeAt(Who->GetActorLocation());
		if (!B.IsNone()) return B;
		return Who->GetActorLocation().X > 0.f ? FName(TEXT("tundra")) : FName(TEXT("volcanic"));
	}
	bool SuLongNoon(const AActor* Who)
	{
		const AWRDesertState* S = AWRDesertState::Get(Who);
		return S && S->bLongNoon && WRDesert::Contains(Who->GetActorLocation());
	}
	bool SuUnderground(const AActor* Who)
	{
		const FVector P = Who->GetActorLocation();
		return WRDesert::InCave(P) || WRLand::InHole(P);
	}
	/** The sand surface classes of every heightfield land (its surfaces whose id says sand: sand, wetsand, softsand,
	 *  slipface, dune, blown, shellgrit, quicksand, slot, a beach). Read once from the lands' files. */
	const TMap<FName, TSet<uint8>>& SuSandClasses()
	{
		static TMap<FName, TSet<uint8>> M;
		static bool bBuilt = false;
		if (bBuilt) return M;
		bBuilt = true;
		for (const FWRLandDef& D : WRLand::All())
		{
			if (!D.Json.IsValid()) continue;
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!D.Json->TryGetArrayField(TEXT("surfaces"), A)) continue;
			TSet<uint8>& S = M.FindOrAdd(D.Id);
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString Id;
				double Cls = 0.0;
				if (!O.IsValid() || !O->TryGetStringField(TEXT("id"), Id) || !O->TryGetNumberField(TEXT("class"), Cls)) continue;
				if (Id.Contains(TEXT("sand")) || Id.Contains(TEXT("dune")) || Id.Contains(TEXT("beach")) || Id == TEXT("slipface") || Id == TEXT("blown") || Id == TEXT("shellgrit") || Id == TEXT("slot")) S.Add(uint8(Cls));
			}
		}
		return M;
	}
}
using namespace WRSunPathImpl;

// ================================================================================================ the path

bool WRSun::Walks(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(SuPathId);
}

int32 WRSun::Rank(const AActor* Who, FName Node)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	const FWRPathProgress* G = P && P->IsActivePath(SuPathId) ? P->GetProgress(SuPathId) : nullptr;
	return G ? G->GetRank(Node) : 0;
}

int32 WRSun::PointsIn(const AActor* Who)
{
	const UWRPathComponent* P = UWRPathComponent::FindFor(Who);
	return P && P->IsActivePath(SuPathId) ? P->GetSpent(SuPathId) : 0;
}

float WRSun::Num(const TCHAR* Key, float Fallback) { return SuT(Key, Fallback); }

bool WRSun::Sandwalker(const AActor* Who) { return SuTagged(Who, Passive_Sandwalker); }

// ================================================================================================ the sun over you

bool WRSun::Daylight(const AActor* Who)
{
	if (!Who) return false;
	if (SuLongNoon(Who)) return true;
	return !AWRGameState::IsNightIn(Who) && !SuUnderground(Who);
}

bool WRSun::Exposed(const AActor* Who)
{
	if (!Who || !Who->GetWorld()) return false;
	// What stands over you (static and dynamic things: roofs, overhangs, the palms' crowns' shade boxes), never bodies.
	const FVector Head = Who->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRSunExposed), false, Who);
	FHitResult Hit;
	return !Who->GetWorld()->LineTraceSingleByObjectType(Hit, Head, Head + FVector(0.f, 0.f, SuR(TEXT("exposedRoofCm"), 3000.f)), Obj, Q);
}

bool WRSun::NearSunLight(const AActor* Who)
{
	if (!Who || !Who->GetWorld()) return false;
	const float R = SuR(TEXT("lampRange"), 600.f);
	const FVector P = Who->GetActorLocation();
	// A sunstone lamp (pieces.json sunstone_lamp) on its stand, built anywhere.
	for (TActorIterator<AWRBuildPiece> It(Who->GetWorld()); It; ++It)
	{
		if (It->PieceId == TEXT("sunstone_lamp") && FVector::DistSquared(It->GetActorLocation(), P) < R * R) return true;
	}
	// A tamed Gildback of yours (its spines store the day: cook 2.12.1), or a Sun Beacon of yours.
	const FString Me = WRWorld::PlayerOf(Who);
	for (TActorIterator<AWRCreature> It(Who->GetWorld()); It; ++It)
	{
		if (It->CreatureId == TEXT("gildback") && It->IsAlive() && It->IsTamed() && It->GetTameOwner() == Me && FVector::DistSquared(It->GetActorLocation(), P) < R * R) return true;
	}
	return Who->Tags.Contains(TEXT("SunBeaconLit"));
}

bool WRSun::SandUnder(const AActor* Who)
{
	return Who && SandAt(Who->GetActorLocation() - FVector(0.f, 0.f, 90.f));
}

bool WRSun::SandstormAt(const UObject* WorldContext, const FVector& At)
{
	if (WRDesert::InBelt(At)) return true;
	const FWRWeatherKind* K = WRDesert::Contains(At) ? UWRWeatherSubsystem::ActiveKind(WorldContext, WRDesert::Biome()) : nullptr;
	return K && K->Id == TEXT("sandstorm");
}

bool WRSun::SandAt(const FVector& Feet)
{
	const FWRLandDef* L = WRLand::At(Feet);
	if (!L || !L->bHeightfield || L->HfSurface.Num() != L->HfHeights.Num()) return false;
	const FVector2D M = L->ToMap(Feet);
	const int32 I = FMath::RoundToInt((M.X - L->HfOrigin.X) / L->HfStep), J = FMath::RoundToInt((M.Y - L->HfOrigin.Y) / L->HfStep);
	if (I < 0 || J < 0 || I >= L->HfN || J >= L->HfN) return false;
	const uint8 Cls = L->HfSurface[J * L->HfN + I];
	const TSet<uint8>* S = SuSandClasses().Find(L->Id);
	return S && S->Contains(Cls);
}

// ================================================================================================ Radiance

float WRSun::RadianceOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetRadiance() : 0.f;
}

float WRSun::MaxRadianceOf(const AActor* Who)
{
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A ? A->GetMaxRadiance() : 100.f;
}

bool WRSun::HasRadiance(const AActor* Who, float Amount)
{
	return Amount <= 0.f || RadianceOf(Who) + 0.01f >= Amount;
}

bool WRSun::SpendRadiance(AWRCharacterBase* Who, float Amount)
{
	if (!Who || !Who->HasAuthority()) return false;
	if (Amount <= 0.f) return true;
	if (!HasRadiance(Who, Amount)) return false;
	SuSetAttr(Who, UWRAttributeSet::GetRadianceAttribute(), RadianceOf(Who) - Amount);
	return true;
}

void WRSun::GainRadiance(AWRCharacterBase* Who, float Amount)
{
	const UWRAttributeSet* A = Who ? Who->GetAttributes() : nullptr;
	if (!A || !Who->HasAuthority() || Amount <= 0.f || !Walks(Who)) return;
	SuSetAttr(Who, UWRAttributeSet::GetRadianceAttribute(), FMath::Min(A->GetMaxRadiance(), A->GetRadiance() + Amount));
}

void WRSun::NoteRefused(const AActor* Who)
{
	if (Who) SuRefusals().Add(Who, SuNow(Who));
}

float WRSun::RefusedFlash(const AActor* Who)
{
	const double* At = Who ? SuRefusals().Find(Who) : nullptr;
	if (!At) return 0.f;
	const float Len = SuR(TEXT("refuseFlash"), 0.35f);
	const float Age = float(SuNow(Who) - *At);
	return Age >= 0.f && Age < Len ? 1.f - Age / Len : 0.f;
}

float WRSun::FillingGlow(const AActor* Who)
{
	static TMap<TWeakObjectPtr<const AActor>, float> Last;
	static TMap<TWeakObjectPtr<const AActor>, double> RoseAt;
	const float Now = RadianceOf(Who);
	float& Prev = Last.FindOrAdd(Who);
	if (Now > Prev + 0.05f) RoseAt.Add(Who, SuNow(Who));
	Prev = Now;
	const double* At = RoseAt.Find(Who);
	return At ? FMath::Clamp(1.f - float(SuNow(Who) - *At) / 0.8f, 0.f, 1.f) : 0.f;
}

float WRSun::NightDim(const AActor* Who)
{
	// At night the bar's edge dims with the drain (cook 2.17); a night source near keeps it lit.
	return Who && !Daylight(Who) && !NearSunLight(Who) ? 1.f : 0.f;
}

void WRSun::TickOwner(AWRCharacterBase* Who, float DeltaTime)
{
	if (!Who || !Who->HasAuthority() || !Who->GetPlayerState()) return;
	const UWRAttributeSet* A = Who->GetAttributes();
	if (!A) return;
	if (!Walks(Who))
	{
		if (A->GetRadiance() > 0.f) SuSetAttr(Who, UWRAttributeSet::GetRadianceAttribute(), 0.f);
		return;
	}
	// Capacity 100, +10 a Radiance capacity rank (nodeEffects sun.radiance_capacity maxRadiance); an amethyst's room
	// (pathResourceMax, WRGearPerks).
	const float Max = SuR(TEXT("max"), 100.f) + FMath::Max(0.f, UWRPathComponent::Stat(Who, TEXT("maxRadiance"))) + WRGearPerks::ResourceMaxBonus(Who);
	if (!FMath::IsNearlyEqual(A->GetMaxRadiance(), Max))
	{
		SuSetAttr(Who, UWRAttributeSet::GetMaxRadianceAttribute(), Max);
		if (A->GetRadiance() > Max) SuSetAttr(Who, UWRAttributeSet::GetRadianceAttribute(), Max);
	}
	if (!Who->IsAlive()) return;
	FSuOwner& O = SuOwners().FindOrAdd(Who);
	O.TraceClock -= DeltaTime;
	if (O.TraceClock <= 0.f) { O.TraceClock = 0.5f; O.bExposed = Exposed(Who); }
	const bool bLongNoon = SuLongNoon(Who);
	const bool bDay = Daylight(Who);
	const bool bBelt = WRDesert::InBelt(Who->GetActorLocation());
	float Rate = 0.f;
	if (bDay)
	{
		// The day's fill by the sky over you: exposed 2.5 a second, shade half; the weather scales it, the belt stops it.
		Rate = SuR(TEXT("dayFill"), 2.5f) * (O.bExposed || bLongNoon ? 1.f : SuR(TEXT("shadeMult"), 0.5f));
		if (const FWRWeatherKind* Wx = UWRWeatherSubsystem::ActiveKind(Who, SuWeatherBiome(Who)))
		{
			if (Wx->Id == TEXT("overcast") || Wx->Id == TEXT("rain") || Wx->Id == TEXT("fog")) Rate *= SuR(TEXT("overcast"), 0.6f);
			else if (Wx->Id == TEXT("heatwave")) Rate *= SuR(TEXT("heatWave"), 1.2f);
			else if (Wx->Id == TEXT("sandstorm") || Wx->Id == TEXT("dimming")) Rate = 0.f;
		}
		if (bLongNoon) Rate *= SuR(TEXT("longNoon"), 1.5f);
		if (bBelt) Rate = 0.f;
		if (Rate > 0.f) GainRadiance(Who, Rate * DeltaTime);
	}
	else
	{
		// Night: no sky fill; a lamp, a Gildback, Dunewrap's four pieces give some back; otherwise it drains (Dawn x0.3).
		float Night = 0.f;
		if (NearSunLight(Who)) Night += SuR(TEXT("lampFill"), 0.5f);
		Night += SuR(TEXT("dayFill"), 2.5f) * UWRInventoryComponent::GearStatFor(Who, TEXT("radianceNight"));
		if (Night > 0.f) GainRadiance(Who, Night * DeltaTime);
		else if (A->GetRadiance() > 0.f && !bLongNoon)
		{
			const float Drain = SuR(TEXT("nightDrain"), 1.5f) * (Rank(Who, TEXT("dawn")) > 0 ? SuR(TEXT("dawnMult"), 0.3f) : 1.f);
			SuSetAttr(Who, UWRAttributeSet::GetRadianceAttribute(), FMath::Max(0.f, A->GetRadiance() - Drain * DeltaTime));
		}
	}
	// Supernova (Fire + Sun 50): full Heat and full Radiance release a blinding explosion round you (both spent).
	if (Who->HasTag(Combo_Supernova) && A->GetRadiance() >= A->GetMaxRadiance() - 0.5f && A->GetHeat() >= A->GetMaxHeat() - 0.5f && SuNow(Who) >= O.NovaReady)
	{
		O.NovaReady = SuNow(Who) + SuCombo(TEXT("supernova"), TEXT("cooldown"), 90.f);
		SuSetAttr(Who, UWRAttributeSet::GetRadianceAttribute(), 0.f);
		SuSetAttr(Who, UWRAttributeSet::GetHeatAttribute(), 0.f);
		const float R = SuCombo(TEXT("supernova"), TEXT("radius"), 700.f);
		Proc(Who, TEXT("sun.supernova"), TEXT("sun.supernova"), Who->GetActorLocation());
		for (TActorIterator<AWRCharacterBase> It(Who->GetWorld()); It; ++It)
		{
			if (!It->IsAlive() || !Who->IsHostileTo(*It) || FVector::DistSquared(It->GetActorLocation(), Who->GetActorLocation()) > FMath::Square(R + It->GetSimpleCollisionRadius())) continue;
			FWRHit Hit;
			Hit.Instigator = Who;
			Hit.Damage = SuCombo(TEXT("supernova"), TEXT("damage"), 120.f);
			Hit.Element = WRTags::Element_Fire;
			Hit.PathId = SuPathId;
			Hit.bSpell = true;
			Hit.BurnBuildup = SuCombo(TEXT("supernova"), TEXT("burn"), 80.f);
			Hit.Buildups.Add(TEXT("blind"), SuCombo(TEXT("supernova"), TEXT("blind"), 100.f));
			Hit.Origin = Who->GetActorLocation();
			Hit.ImpactPoint = It->GetActorLocation();
			UWRCombatLibrary::ApplyHit(*It, Hit);
		}
	}
}

// ================================================================================================ the combat code's asks

void WRSun::ScaleBuildups(const AWRCharacterBase* Attacker, FWRHit& Hit)
{
	if (!Attacker) return;
	// Blind chance: +10% blind buildup on all your hits and +5 blind on every weapon hit, a rank (nodeEffects
	// sun.blind_chance blindBuildup / weaponBlind).
	if (float* B = Hit.Buildups.Find(TEXT("blind")))
	{
		*B *= 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Attacker, TEXT("blindBuildup")));
	}
	if (Hit.bMelee && !Hit.bDamageOverTime)
	{
		const float Weapon = UWRPathComponent::Stat(Attacker, TEXT("weaponBlind"));
		if (Weapon > 0.f) Hit.Buildups.FindOrAdd(TEXT("blind")) += Weapon;
	}
	// Thunderflash (Lightning + Sun 25): every shock also blinds.
	if (Attacker->HasTag(Combo_Thunderflash) && Hit.Buildups.Contains(TEXT("shock"))) Hit.Buildups.FindOrAdd(TEXT("blind")) += SuCombo(TEXT("thunderflash"), TEXT("blind"), 40.f);
	// Crit blind: the Colossus eye (blind 60 a crit, 3 s), the Empress's lens ring and the Sunglass ring (gear stat critBlind).
	if (Hit.bCritical)
	{
		const float Crit = UWRInventoryComponent::GearStatFor(Attacker, TEXT("critBlind"));
		if (Crit > 0.f) Hit.Buildups.FindOrAdd(TEXT("blind")) += Crit;
	}
}

float WRSun::DamageScale(const AWRCharacterBase* Attacker, const AWRCharacterBase* Target, const FWRHit& Hit)
{
	if (!Attacker || !Target) return 1.f;
	float S = 1.f;
	const bool bSun = Hit.PathId == SuPathId;
	// Brilliance: +6% a rank on every Sun spell and Wildbrand; Sun-forged +6% a rank on the Wildbrands.
	if (bSun) S *= 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Attacker, TEXT("sunDamage")));
	if (bSun && Hit.bWeaponArt) S *= 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Attacker, TEXT("sunBrandDamage")));
	// Zenith: at 90% Radiance and over, all your Sun damage +15%.
	if (bSun && ZenithLit(Attacker)) S *= 1.f + SuT(TEXT("zenithBonus"), 0.15f);
	// Searing light: blinded targets take +6% a rank from you.
	if (Target->HasTag(WRTags::Status_Blind)) S *= 1.f + FMath::Max(0.f, UWRPathComponent::Stat(Attacker, TEXT("vsBlinded")));
	// Dry blood (Sun + Blood 25): bleeds burst +20% by day under the open sky.
	if (Hit.bDamageOverTime && Attacker->HasTag(Combo_DryBlood) && Daylight(Attacker) && Exposed(Target)) S *= SuCombo(TEXT("dry_blood"), TEXT("bleed"), 1.2f);
	return S;
}

void WRSun::AfterHit(AWRCharacterBase* Target, const FWRHit& Hit, float Damage)
{
	AWRCharacterBase* Attacker = Cast<AWRCharacterBase>(Hit.Instigator);
	if (!Target) return;
	// The blind proc's feedback (cook 2.17 proc table): a cold-white flash in the eyes and a high glass ring, on onset.
	const bool bBlind = Target->HasTag(WRTags::Status_Blind);
	if (bBlind && !SuWasBlind().Contains(Target)) { SuWasBlind().Add(Target); Proc(Target, TEXT("sun.blind_flash"), TEXT("sun.blind_ring"), Target->GetActorLocation() + FVector(0, 0, Target->GetSimpleCollisionHalfHeight() * 0.7f)); }
	else if (!bBlind) SuWasBlind().Remove(Target);
	if (!Attacker || !Walks(Attacker) || Hit.bDamageOverTime) return;
	// Radiance: +3 a landed melee hit, +5 a kill.
	if (Hit.bMelee) GainRadiance(Attacker, SuR(TEXT("perHit"), 3.f));
	if (!Target->IsAlive()) GainRadiance(Attacker, SuR(TEXT("killGain"), 5.f));
	// Searing light's gold sparks on a blinded body's hits.
	if (bBlind && Rank(Attacker, TEXT("searing_light")) > 0) WRFx::Broadcast(Target, TEXT("sun.sear"), Hit.ImpactPoint.IsZero() ? Target->GetActorLocation() : Hit.ImpactPoint, FRotator::ZeroRotator, 1.f, TEXT("sun.sizzle"));
}

void WRSun::OnParry(AWRCharacterBase* Defender, AWRCharacterBase* Attacker)
{
	// Gleam: a parry throws the sun off your blade into the attacker's eyes (blind 100; bosses 60): a white starburst and a
	// bright ting on the clang.
	if (!Defender || !Attacker || !Defender->HasAuthority() || Rank(Defender, TEXT("gleam")) <= 0 || !Attacker->Status) return;
	const float Blind = SuBoss(Attacker) ? SuT(TEXT("gleamBossBlind"), 60.f) : SuT(TEXT("gleamBlind"), 100.f);
	Attacker->Status->AddStatusBuildup(WRStatus::FromName(TEXT("blind")), Blind, Defender, 0.f);
	Proc(Attacker, TEXT("sun.gleam"), TEXT("sun.gleam"), Attacker->GetActorLocation() + FVector(0, 0, Attacker->GetSimpleCollisionHalfHeight() * 0.8f));
}

bool WRSun::ZenithLit(const AActor* Who)
{
	return Who && Rank(Who, TEXT("zenith")) > 0 && MaxRadianceOf(Who) > 0.f && RadianceOf(Who) >= MaxRadianceOf(Who) * SuT(TEXT("zenithAt"), 0.9f);
}

void WRSun::Proc(AWRCharacterBase* Who, FName Fx, FName Sound, const FVector& At)
{
	if (!Who || !Who->HasAuthority()) return;
	WRFx::Broadcast(Who, Fx, At, FRotator::ZeroRotator, 1.f, Sound);
}

// ================================================================================================ the body

uint32 WRSun::LookSig(const AActor* Hero)
{
	const float R = MaxRadianceOf(Hero) > 0.f ? RadianceOf(Hero) / MaxRadianceOf(Hero) : 0.f;
	return (uint32(FMath::RoundToInt(R * 8.f)) << 2) | (Daylight(Hero) ? 1u : 0u) | (ZenithLit(Hero) ? 2u : 0u);
}

void WRSun::ApplyLook(const AActor* Hero, const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Skin, FName Main, float MainS, FName Sub, float SubS)
{
	// The flecks hold a stored glow at night, brighter the more Radiance you carry (cook 2.18); at 100 and full Radiance a
	// warm glow rises off the skin (the shimmer at the hands is the fx's).
	if (Main != SuPathId && Sub != SuPathId) return;
	const FString Prefix = Main == SuPathId ? TEXT("Main") : TEXT("Sub");
	const float S = Main == SuPathId ? MainS : SubS;
	const float R = MaxRadianceOf(Hero) > 0.f ? RadianceOf(Hero) / MaxRadianceOf(Hero) : 0.f;
	const bool bDay = Daylight(Hero);
	for (UMaterialInstanceDynamic* M : Skin)
	{
		if (!M) continue;
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), FMath::Min(1.f, S));
		const float Night = bDay ? 0.f : SuT(TEXT("lookNightGlow"), 0.5f) * R * S;
		const float Full = S >= 0.99f && R >= 0.99f ? SuT(TEXT("lookFullGlow"), 0.35f) : 0.f;
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("EmissiveNits"))), Night + Full);
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseHz"))), Full > 0.f ? SuT(TEXT("lookShimmerHz"), 0.6f) : 0.f);
		M->SetScalarParameterValue(FName(*(Prefix + TEXT("PulseAmp"))), Full > 0.f ? SuT(TEXT("lookShimmerAmp"), 0.15f) : 0.f);
	}
}
