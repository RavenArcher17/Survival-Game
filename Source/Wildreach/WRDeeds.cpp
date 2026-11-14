// The endgame's deeds (cooks/build-endgame.md 2.1 to 2.4, 2.7, 2.8): the component on every PlayerState and what it
// keeps (prestige marks and saved layouts, souls forged, achievements, titles, cosmetics), prestige at an altar, the
// Heron's draught (respec), soul forging, mythic forging's rule check (data-ready), titles and cosmetics worn, and the
// "deeds ..." commands the screens send. The achievement engine, the census and the hooks are in WRDeedsAchieve.cpp.
#include "WRDeeds.h"
#include "WRDeedsInternal.h"
#include "Wildreach.h"
#include "WRAltar.h"
#include "WRArtifacts.h"
#include "WRCharacter.h"
#include "WRAttributeSet.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRInventory.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRRecords.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

using namespace WRDeedsImpl;

namespace WRDeedsData
{
	TSharedPtr<FJsonObject> Load(TSharedPtr<FJsonObject>& Cache, const TCHAR* File)
	{
		if (!Cache.IsValid()) Cache = FWRTuning::LoadDataFile(File);
		if (!Cache.IsValid()) Cache = MakeShared<FJsonObject>();
		return Cache;
	}
	TSharedPtr<FJsonObject> GAch, GCos, GSoul, GMyth;
	/** A path's element tag name ("fire" -> Element.Fire). */
	FName PathOfElement(const FGameplayTag& Tag)
	{
		if (Tag == WRTags::Element_Fire) return TEXT("fire");
		if (Tag == WRTags::Element_Frost) return TEXT("frost");
		if (Tag == WRTags::Element_Lightning) return TEXT("lightning");
		return NAME_None;
	}
	AWRPlayerController* ControllerOf(const AWRPlayerState* PS) { return PS ? Cast<AWRPlayerController>(PS->GetOwner()) : nullptr; }
	/** Within reach of an altar of this path (the server's check for every altar action). */
	const AWRBossAltar* AltarNear(const AWRPlayerState* PS, FName Path)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn) return nullptr;
		for (TActorIterator<AWRBossAltar> It(Pawn->GetWorld()); It; ++It)
		{
			if ((Path.IsNone() || It->PathId == Path) && FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation()) < AWRBossAltar::InteractRange + 150.f) return *It;
		}
		return nullptr;
	}
	void Tell(AWRPlayerState* PS, const FString& S) { if (AWRPlayerController* PC = ControllerOf(PS)) PC->ClientNotify(S); }
}
using namespace WRDeedsData;

// ================================================================================================ data

TSharedPtr<FJsonObject> WRDeeds::Achievements() { return Load(GAch, TEXT("achievements.json")); }
TSharedPtr<FJsonObject> WRDeeds::Cosmetics() { return Load(GCos, TEXT("cosmetics.json")); }
TSharedPtr<FJsonObject> WRDeeds::Souls() { return Load(GSoul, TEXT("souls.json")); }
TSharedPtr<FJsonObject> WRDeeds::Mythic() { return Load(GMyth, TEXT("mythic.json")); }

TSharedPtr<FJsonObject> WRDeeds::Deed(FName Id)
{
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(Achievements(), TEXT("achievements")))
		for (const TSharedPtr<FJsonValue>& V : *L) if (DStr(V->AsObject(), TEXT("id")) == Id.ToString()) return V->AsObject();
	return nullptr;
}

TSharedPtr<FJsonObject> WRDeeds::Cosmetic(FName Id, FString* OutKind)
{
	static const TCHAR* Lists[] = { TEXT("glows"), TEXT("dyes"), TEXT("marks") };
	static const TCHAR* Kinds[] = { TEXT("glow"), TEXT("dye"), TEXT("mark") };
	for (int32 i = 0; i < 3; ++i)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(Cosmetics(), Lists[i]))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				if (DStr(V->AsObject(), TEXT("id")) == Id.ToString())
				{
					if (OutKind) *OutKind = Kinds[i];
					return V->AsObject();
				}
			}
		}
	}
	return nullptr;
}

FColor WRDeeds::CosmeticColour(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field) { return DColour(Row, Field, FColor::White); }

TArray<FString> WRDeeds::Sigil(FName Id)
{
	TArray<FString> Out;
	if (const TArray<TSharedPtr<FJsonValue>>* L = DArr(DChild(Cosmetics(), TEXT("sigils")), Id.ToString()))
		for (const TSharedPtr<FJsonValue>& V : *L) Out.Add(V->AsString());
	return Out;
}

TSharedPtr<FJsonObject> WRDeeds::SoulRow(FName Boss)
{
	const TSharedPtr<FJsonObject> Row = DChild(DChild(Souls(), TEXT("bosses")), Boss.ToString());
	// Only a boss that's in this build (its creature exists) has a live soul.
	return Row.IsValid() && DBool(Row, TEXT("live")) && FWRCreatureRegistry::Get().Find(Boss) ? Row : nullptr;
}

FName WRDeeds::SoulBossOfWeapon(FName WeaponItem)
{
	const TSharedPtr<FJsonObject> Bosses = DChild(Souls(), TEXT("bosses"));
	if (!Bosses.IsValid() || WeaponItem.IsNone()) return NAME_None;
	for (const auto& P : Bosses->Values)
	{
		const TSharedPtr<FJsonObject> Row = P.Value->AsObject();
		if (DStr(Row, TEXT("weapon")) == WeaponItem.ToString() && SoulRow(FName(*P.Key)).IsValid()) return FName(*P.Key);
	}
	return NAME_None;
}

FString WRDeeds::SwornTitle(FName Path)
{
	return DStr(DChild(DChild(Achievements(), TEXT("titles")), TEXT("sworn")), Path.ToString(), Path.ToString() + TEXT("-sworn"));
}

FString WRDeeds::Clock(float Seconds)
{
	const int32 S = FMath::Max(0, FMath::RoundToInt(Seconds));
	return S >= 3600 ? FString::Printf(TEXT("%d:%02d:%02d"), S / 3600, (S / 60) % 60, S % 60) : FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
}

FName WRDeeds::AltarBoss(FName Path)
{
	// The major boss of the altar's path's land (Vulkarn's altar gives Fire).
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
	if (!Def) return NAME_None;
	for (const FWRCreatureDef& C : FWRCreatureRegistry::Get().All())
	{
		if (C.Biome == Def->Biome && C.Role.Contains(TEXT("major"))) return C.Id;
	}
	return NAME_None;
}

// ================================================================================================ the component

UWRDeedsComponent::UWRDeedsComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f;
}

void UWRDeedsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRDeedsComponent, Marks);
	DOREPLIFETIME(UWRDeedsComponent, ActiveTitle);
	DOREPLIFETIME(UWRDeedsComponent, WornGlow);
	DOREPLIFETIME(UWRDeedsComponent, WornDye);
	DOREPLIFETIME(UWRDeedsComponent, WornMark);
	DOREPLIFETIME(UWRDeedsComponent, BannerSigil);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Layouts, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Forged, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Achieved, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Titles, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Cosmetics, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Bosses, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, ProgressIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWRDeedsComponent, Progress, COND_OwnerOnly);
}

UWRDeedsComponent* UWRDeedsComponent::For(const AActor* Who)
{
	const AWRPlayerState* PS = DStateOf(Who);
	return PS ? PS->FindComponentByClass<UWRDeedsComponent>() : nullptr;
}

void UWRDeedsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && DeathDay < 0)
	{
		if (const AWRGameState* GS = GetWorld()->GetGameState<AWRGameState>()) DeathDay = GS->GetDay();
	}
}

const FWRPrestigeRep* UWRDeedsComponent::MarksOn(FName Path) const
{
	return Marks.FindByPredicate([Path](const FWRPrestigeRep& R) { return R.Path == Path; });
}

bool UWRDeedsComponent::HasForged(FName Boss, bool bWeapon) const
{
	const FWRSoulForged* F = Forged.FindByPredicate([Boss](const FWRSoulForged& R) { return R.Boss == Boss; });
	return F && (bWeapon ? F->bWeapon : F->bTalisman);
}

void UWRDeedsComponent::OnRep_Look()
{
	// Your marks, title and cosmetics on your body: whoever's pawn this is redraws its outfit and held weapon.
	const AWRPlayerState* PS = Cast<AWRPlayerState>(GetOwner());
	if (AWRCharacter* Hero = PS ? Cast<AWRCharacter>(PS->GetPawn()) : nullptr) Hero->ApplyDeedsLook();
}

void UWRDeedsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority()) return;
	CheckClock += DeltaTime;
	if (CheckClock >= 2.f)
	{
		CheckClock = 0.f;
		StepChecks();
	}
}

TSharedRef<FJsonObject> UWRDeedsComponent::ToJson() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	const auto Names = [](const auto& Set)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		for (const FName& N : Set) A.Add(MakeShared<FJsonValueString>(N.ToString()));
		return A;
	};
	TArray<TSharedPtr<FJsonValue>> M;
	for (const FWRPrestigeRep& R : Marks)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("path"), R.Path.ToString());
		X->SetNumberField(TEXT("keener"), R.Keener);
		X->SetNumberField(TEXT("deeper"), R.Deeper);
		X->SetNumberField(TEXT("rooted"), R.Rooted);
		M.Add(MakeShared<FJsonValueObject>(X));
	}
	O->SetArrayField(TEXT("marks"), M);
	TArray<TSharedPtr<FJsonValue>> L;
	for (const FWRPathLayout& Lay : Layouts)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("path"), Lay.Path.ToString());
		TArray<TSharedPtr<FJsonValue>> R;
		for (const FWRNodeRank& N : Lay.Ranks) R.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s:%d"), *N.NodeId.ToString(), N.Rank)));
		X->SetArrayField(TEXT("ranks"), R);
		L.Add(MakeShared<FJsonValueObject>(X));
	}
	O->SetArrayField(TEXT("layouts"), L);
	TArray<TSharedPtr<FJsonValue>> F;
	for (const FWRSoulForged& S : Forged)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("boss"), S.Boss.ToString());
		X->SetBoolField(TEXT("weapon"), S.bWeapon);
		X->SetBoolField(TEXT("talisman"), S.bTalisman);
		F.Add(MakeShared<FJsonValueObject>(X));
	}
	O->SetArrayField(TEXT("forged"), F);
	O->SetArrayField(TEXT("achieved"), Names(Achieved));
	TArray<TSharedPtr<FJsonValue>> T;
	for (const FString& S : Titles) T.Add(MakeShared<FJsonValueString>(S));
	O->SetArrayField(TEXT("titles"), T);
	O->SetStringField(TEXT("title"), ActiveTitle);
	O->SetArrayField(TEXT("cosmetics"), Names(Cosmetics));
	O->SetStringField(TEXT("glow"), WornGlow.IsNone() ? FString() : WornGlow.ToString());
	O->SetStringField(TEXT("dye"), WornDye.IsNone() ? FString() : WornDye.ToString());
	O->SetStringField(TEXT("mark"), WornMark.IsNone() ? FString() : WornMark.ToString());
	O->SetStringField(TEXT("sigil"), BannerSigil.IsNone() ? FString() : BannerSigil.ToString());
	O->SetArrayField(TEXT("bosses"), Names(Bosses));
	O->SetArrayField(TEXT("landmarks"), Names(Landmarks));
	O->SetArrayField(TEXT("species"), Names(Species));
	O->SetArrayField(TEXT("tamed"), Names(Tamed));
	O->SetArrayField(TEXT("passes"), Names(PassesCrossed));
	O->SetArrayField(TEXT("hidden"), Names(HiddenFound));
	O->SetArrayField(TEXT("raids"), Names(Raids));
	O->SetNumberField(TEXT("deathDay"), DeathDay);
	O->SetNumberField(TEXT("soulSlot"), SoulSlot);
	O->SetStringField(TEXT("soulSlotWas"), SoulSlotWas.IsNone() ? FString() : SoulSlotWas.ToString());
	return O;
}

void UWRDeedsComponent::FromJson(const TSharedPtr<FJsonObject>& O)
{
	if (!O.IsValid()) return;
	const auto ReadNames = [&O](const TCHAR* Key, auto& Out)
	{
		if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(O, Key)) for (const TSharedPtr<FJsonValue>& V : *A) if (!V->AsString().IsEmpty()) Out.Add(FName(*V->AsString()));
	};
	Marks.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(O, TEXT("marks")))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> X = V->AsObject();
			FWRPrestigeRep R;
			R.Path = FName(*DStr(X, TEXT("path")));
			R.Keener = uint8(DNum(X, TEXT("keener"), 0.f));
			R.Deeper = uint8(DNum(X, TEXT("deeper"), 0.f));
			R.Rooted = uint8(DNum(X, TEXT("rooted"), 0.f));
			if (!R.Path.IsNone()) Marks.Add(R);
		}
	}
	Layouts.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(O, TEXT("layouts")))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> X = V->AsObject();
			FWRPathLayout Lay;
			Lay.Path = FName(*DStr(X, TEXT("path")));
			if (const TArray<TSharedPtr<FJsonValue>>* R = DArr(X, TEXT("ranks")))
			{
				for (const TSharedPtr<FJsonValue>& N : *R)
				{
					FString Node, Rank;
					if (N->AsString().Split(TEXT(":"), &Node, &Rank)) Lay.Ranks.Add({ FName(*Node), FCString::Atoi(*Rank) });
				}
			}
			if (!Lay.Path.IsNone()) Layouts.Add(Lay);
		}
	}
	Forged.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(O, TEXT("forged")))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> X = V->AsObject();
			FWRSoulForged S;
			S.Boss = FName(*DStr(X, TEXT("boss")));
			S.bWeapon = DBool(X, TEXT("weapon"));
			S.bTalisman = DBool(X, TEXT("talisman"));
			if (!S.Boss.IsNone()) Forged.Add(S);
		}
	}
	Achieved.Reset(); ReadNames(TEXT("achieved"), Achieved);
	Titles.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* A = DArr(O, TEXT("titles"))) for (const TSharedPtr<FJsonValue>& V : *A) Titles.AddUnique(V->AsString());
	ActiveTitle = DStr(O, TEXT("title"));
	Cosmetics.Reset(); ReadNames(TEXT("cosmetics"), Cosmetics);
	const auto NameOf = [&O](const TCHAR* Key) { const FString S = DStr(O, Key); return S.IsEmpty() ? FName() : FName(*S); };
	WornGlow = NameOf(TEXT("glow"));
	WornDye = NameOf(TEXT("dye"));
	WornMark = NameOf(TEXT("mark"));
	BannerSigil = NameOf(TEXT("sigil"));
	Bosses.Reset(); ReadNames(TEXT("bosses"), Bosses);
	Landmarks.Reset(); ReadNames(TEXT("landmarks"), Landmarks);
	Species.Reset(); ReadNames(TEXT("species"), Species);
	Tamed.Reset(); ReadNames(TEXT("tamed"), Tamed);
	PassesCrossed.Reset(); ReadNames(TEXT("passes"), PassesCrossed);
	HiddenFound.Reset(); ReadNames(TEXT("hidden"), HiddenFound);
	Raids.Reset(); ReadNames(TEXT("raids"), Raids);
	DeathDay = int32(DNum(O, TEXT("deathDay"), float(DeathDay)));
	SoulSlot = int32(DNum(O, TEXT("soulSlot"), -1.f));
	SoulSlotWas = NameOf(TEXT("soulSlotWas"));
	OnRep_Look();
}

// ================================================================================================ prestige numbers

float WRDeeds::PrestigeDamage(const AActor* Attacker, const FWRHit& Hit)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(Attacker);
	const UWRPathComponent* Paths = D ? UWRPathComponent::FindFor(Attacker) : nullptr;
	if (!D || !Paths || D->Marks.Num() == 0) return 1.f;
	// The hit's path: a path ability names it; a weapon blow carries the path of its element.
	FName Path = Hit.PathId;
	if (Path.IsNone()) Path = PathOfElement(Hit.Element);
	if (Path.IsNone() || !Paths->IsActivePath(Path)) return 1.f;
	const FWRPrestigeRep* R = D->MarksOn(Path);
	return R ? 1.f + WRTune(TEXT("prestige.damage"), 0.05f) * R->Keener : 1.f;
}

float WRDeeds::PrestigeResource(const AActor* Who, FName Path)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(Who);
	const UWRPathComponent* Paths = D ? UWRPathComponent::FindFor(Who) : nullptr;
	if (!D || !Paths || !Paths->IsActivePath(Path)) return 1.f;
	const FWRPrestigeRep* R = D->MarksOn(Path);
	return R ? 1.f + WRTune(TEXT("prestige.resource"), 0.1f) * R->Deeper : 1.f;
}

bool WRDeeds::RootedSub(const AActor* Who, FName& OutPath)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(Who);
	const UWRPathComponent* Paths = D ? UWRPathComponent::FindFor(Who) : nullptr;
	if (!Paths) return false;
	const FWRPrestigeRep* R = D->MarksOn(Paths->GetSubPath());
	OutPath = Paths->GetSubPath();
	return R && R->Rooted > 0;
}

float WRDeeds::GoldThreads(const AActor* Who, FName Path)
{
	const UWRDeedsComponent* D = UWRDeedsComponent::For(Who);
	const FWRPrestigeRep* R = D ? D->MarksOn(Path) : nullptr;
	return R ? FMath::Clamp(WRTune(TEXT("prestige.goldPerMark"), 0.12f) * R->Total(), 0.f, 1.f) : 0.f;
}

// ================================================================================================ prestige

FString WRDeeds::PrestigeBlock(const AWRPlayerController* PC, FName Path)
{
	const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
	const UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
	if (!Paths || !D || !Def) return TEXT("Not now");
	const FWRPrestigeRep* R = D->MarksOn(Path);
	const int32 Max = FMath::RoundToInt(WRTune(TEXT("prestige.maxMarks"), 3.f));
	if (R && R->Total() >= Max) return FString::Printf(TEXT("Three marks: %s is sworn"), *Def->Name);
	if (!Paths->IsActivePath(Path)) return FString::Printf(TEXT("Walk %s first (main path or subclass)"), *Def->Name);
	const int32 Need = FMath::RoundToInt(WRTune(TEXT("prestige.atPoints"), 100.f));
	const int32 Have = Paths->GetEarned(Path);
	if (Have < Need) return FString::Printf(TEXT("Reach %d points in %s (now %d)"), Need, *Def->Name, Have);
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(PC->GetPawn()); C && C->HasTag(WRTags::State_CombatTagged)) return TEXT("Not in a fight");
	return FString();
}

FString WRDeeds::Prestige(AWRPlayerState* PS, FName Path, FName Perk, const AActor* Altar)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
	if (!D || !Paths || !Def) return TEXT("Not now");
	if (!Altar) return FString::Printf(TEXT("Only at the altar of %s"), *Def->Name);
	const FString Why = PrestigeBlock(ControllerOf(PS), Path);
	if (!Why.IsEmpty()) return Why;
	FWRPrestigeRep* R = D->Marks.FindByPredicate([Path](const FWRPrestigeRep& X) { return X.Path == Path; });
	if (Perk == TEXT("rooted") && R && R->Rooted > 0) return TEXT("Rooted is taken once a path");
	if (Perk != TEXT("keener") && Perk != TEXT("deeper") && Perk != TEXT("rooted")) return TEXT("Pick a mark");
	// The path goes back to the start: its ranks are kept as the layout to restore, its XP restarts.
	TArray<FWRNodeRank> Layout = Paths->ReturnPath(Path);
	FWRPathLayout* Lay = D->Layouts.FindByPredicate([Path](const FWRPathLayout& X) { return X.Path == Path; });
	if (!Lay) { D->Layouts.AddDefaulted(); Lay = &D->Layouts.Last(); Lay->Path = Path; }
	Lay->Ranks = Layout;
	if (!R) { D->Marks.AddDefaulted(); R = &D->Marks.Last(); R->Path = Path; }
	if (Perk == TEXT("keener")) ++R->Keener;
	else if (Perk == TEXT("deeper")) ++R->Deeper;
	else ++R->Rooted;
	D->bDirty = true;
	const int32 Total = R->Total();
	UE_LOG(LogWildreach, Log, TEXT("DEEDS %s returned %s (%s): %d marks"), *PS->GetPlayerName(), *Path.ToString(), *Perk.ToString(), Total);
	// The altar's flame roars and gold rises from it.
	if (AActor* A = const_cast<AActor*>(Altar))
	{
		WRFx::Broadcast(A, TEXT("levelup"), A->GetActorLocation() + FVector(0, 0, 100.f), FRotator::ZeroRotator, 1.4f, TEXT("endgame.return"));
	}
	if (Total >= FMath::RoundToInt(WRTune(TEXT("prestige.maxMarks"), 3.f))) GiveTitle(PS, SwornTitle(Path), TEXT("three marks"));
	Notify(PS, TEXT("prestige"), Path, Total);
	D->RefreshLook();
	return FString::Printf(TEXT("%s returned: mark %d of 3 (%s). Your path starts again; Restore layout buys it back as points come."), *Def->Name, Total,
		Perk == TEXT("keener") ? TEXT("Keener: +5% damage") : Perk == TEXT("deeper") ? TEXT("Deeper: +10% resource") : TEXT("Rooted: its root passive on your subclass"));
}

FString WRDeeds::RestoreLayout(AWRPlayerState* PS, FName Path)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
	if (!D || !Paths) return TEXT("Not now");
	const FWRPathLayout* Lay = D->Layouts.FindByPredicate([Path](const FWRPathLayout& X) { return X.Path == Path; });
	if (!Lay || Lay->Ranks.Num() == 0) return TEXT("No saved layout for this path");
	if (!Paths->IsActivePath(Path)) return TEXT("Walk this path first");
	const int32 Bought = Paths->RestoreLayout(Path, Lay->Ranks);
	return Bought > 0 ? FString::Printf(TEXT("Restored %d rank%s of your old layout"), Bought, Bought == 1 ? TEXT("") : TEXT("s")) : TEXT("No points to restore with yet");
}

// ================================================================================================ respec

FString WRDeeds::RespecBlock(const AWRPlayerController* PC, FName Path)
{
	const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
	if (!Paths || !Def) return TEXT("Not now");
	if (PS->GetInventory()->CountOf(TEXT("heron_draught")) <= 0) return TEXT("You need a Heron's draught");
	if (!Paths->IsActivePath(Path)) return FString::Printf(TEXT("%s isn't walked now: altar swaps keep its ranks"), *Def->Name);
	// The starter is free and stays: anything more is what you'd forget.
	const FWRPathProgress* P = Paths->GetProgress(Path);
	int32 Ranks = 0;
	if (P) for (const FWRNodeRank& R : P->Ranks) if (R.NodeId != Def->Starter) Ranks += R.Rank;
	if (Ranks == 0) return FString::Printf(TEXT("No points spent in %s"), *Def->Name);
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(PC->GetPawn()); C && C->HasTag(WRTags::State_CombatTagged)) return TEXT("Not in a fight");
	return FString();
}

FString WRDeeds::Respec(AWRPlayerState* PS, FName Path)
{
	UWRPathComponent* Paths = PS ? PS->GetPaths() : nullptr;
	const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
	if (!Paths || !Def) return TEXT("Not now");
	const FString Why = RespecBlock(ControllerOf(PS), Path);
	if (!Why.IsEmpty()) return Why;
	if (PS->GetInventory()->RemoveItem(TEXT("heron_draught"), 1) < 1) return TEXT("You need a Heron's draught");
	const int32 Freed = Paths->GetSpent(Path);
	Paths->UnlearnPath(Path);
	if (AWRCharacterBase* C = Cast<AWRCharacterBase>(PS->GetPawn()))
	{
		WRSound::Broadcast(C, TEXT("endgame.forget"));
		WRFx::BroadcastOn(C, TEXT("discover"), NAME_None, 1.f);
	}
	UE_LOG(LogWildreach, Log, TEXT("DEEDS %s drank the Heron's draught: %s forgotten (%d points freed)"), *PS->GetPlayerName(), *Path.ToString(), Freed);
	return FString::Printf(TEXT("The heron takes it: %s is forgotten. %d points are yours to spend again."), *Def->Name, Freed - Paths->GetSpent(Path));
}

// ================================================================================================ soul forging

FString WRDeeds::ForgeBlock(const AWRPlayerController* PC, FName Boss, bool bWeapon)
{
	const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	const UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	const TSharedPtr<FJsonObject> Row = SoulRow(Boss);
	if (!PS || !D || !Row.IsValid()) return TEXT("Not now");
	const FWRCreatureDef* B = FWRCreatureRegistry::Get().Find(Boss);
	FString Short = B ? B->Name : Boss.ToString();
	Short.Split(TEXT(","), &Short, nullptr);
	if (D->HasForged(Boss, bWeapon)) return TEXT("Owned");
	const FName Soul(*DStr(Row, TEXT("soul")));
	if (PS->GetInventory()->CountOf(Soul) <= 0) return FString::Printf(TEXT("Bring %s's soul"), *Short);
	if (bWeapon)
	{
		const FName Weapon(*DStr(Row, TEXT("weapon")));
		const FWRItemDef* W = FWRItemRegistry::Get().Find(Weapon);
		bool bHave = false;
		for (const FWRItemStack& S : PS->GetInventory()->GetSlots()) bHave |= S.ItemId == Weapon && S.Mark != TEXT("awakened");
		if (!bHave) return FString::Printf(TEXT("Bring the %s in your bag (its blueprint drops from %s)"), W ? *W->Name : *Weapon.ToString(), *Short);
	}
	return FString();
}

FString WRDeeds::ForgeSoul(AWRPlayerState* PS, FName Boss, bool bWeapon, const AActor* Altar)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	const TSharedPtr<FJsonObject> Row = SoulRow(Boss);
	if (!D || !Row.IsValid()) return TEXT("Not now");
	if (!Altar || AltarBoss(Cast<AWRBossAltar>(Altar) ? Cast<AWRBossAltar>(Altar)->PathId : NAME_None) != Boss) return TEXT("Only at that boss's own altar");
	const FString Why = ForgeBlock(ControllerOf(PS), Boss, bWeapon);
	if (!Why.IsEmpty()) return Why;
	UWRInventoryComponent* Bag = PS->GetInventory();
	const FName Soul(*DStr(Row, TEXT("soul")));
	FString Made;
	if (bWeapon)
	{
		const FName Weapon(*DStr(Row, TEXT("weapon")));
		// The weapon goes on the altar and comes back awakened: quality, roll, sockets and upgrade kept; the metal
		// factor of starsteel; the boss's element for good; its Soul art while it's held.
		for (FWRItemStack& S : Bag->GetSlotsMutable())
		{
			if (S.ItemId != Weapon || S.Mark == TEXT("awakened")) continue;
			S.Mark = TEXT("awakened");
			S.Element = FName(*DStr(Row, TEXT("element")));
			const FWRItemDef* W = FWRItemRegistry::Get().Find(Weapon);
			if (W && W->MaxDurability > 0.f) S.Durability = W->MaxDurability;
			Made = FString::Printf(TEXT("%s awakened: its Soul art, %s, is yours while you hold it"), W ? *W->Name : *Weapon.ToString(), *DStr(DChild(Row, TEXT("art")), TEXT("name")));
			break;
		}
		if (Made.IsEmpty()) return TEXT("Bring the weapon in your bag");
	}
	else
	{
		const FName Talisman(*DStr(DChild(Row, TEXT("talisman")), TEXT("id")));
		if (!Bag->AddInstance(FWRInventoryRules::MakeInstance(Talisman))) return TEXT("No room in your bag");
		Made = FString::Printf(TEXT("%s is made"), *DStr(DChild(Row, TEXT("talisman")), TEXT("name")));
	}
	Bag->RemoveItem(Soul, 1);
	FWRSoulForged* F = D->Forged.FindByPredicate([Boss](const FWRSoulForged& R) { return R.Boss == Boss; });
	if (!F) { D->Forged.AddDefaulted(); F = &D->Forged.Last(); F->Boss = Boss; }
	(bWeapon ? F->bWeapon : F->bTalisman) = true;
	D->bDirty = true;
	if (AActor* A = const_cast<AActor*>(Altar))
	{
		WRFx::Broadcast(A, Boss == TEXT("skaldrith") ? FName(TEXT("frostnova")) : FName(TEXT("fire.impact")), A->GetActorLocation() + FVector(0, 0, 90.f), FRotator::ZeroRotator, 1.2f, TEXT("endgame.forge"));
	}
	Bag->ApplyEquipment();
	UE_LOG(LogWildreach, Log, TEXT("DEEDS %s forged %s's soul: %s"), *PS->GetPlayerName(), *Boss.ToString(), bWeapon ? TEXT("awakened weapon") : TEXT("talisman"));
	return Made;
}

FString WRDeeds::SalvageSoul(AWRPlayerState* PS, FName Boss, const AActor* Altar)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	const TSharedPtr<FJsonObject> Row = SoulRow(Boss);
	if (!D || !Row.IsValid() || !Altar) return TEXT("Not now");
	const AWRBossAltar* BossAltar = Cast<AWRBossAltar>(Altar);
	if (!BossAltar || AltarBoss(BossAltar->PathId) != Boss) return TEXT("Only at that boss's own altar");
	if (!D->HasForged(Boss, true) || !D->HasForged(Boss, false)) return TEXT("Forge both of its gifts first");
	const FName Soul(*DStr(Row, TEXT("soul")));
	if (PS->GetInventory()->RemoveItem(Soul, 1) < 1) return TEXT("No soul to give");
	const TSharedPtr<FJsonObject> S = DChild(Souls(), TEXT("salvage"));
	const FName Item(*DStr(S, TEXT("item"), TEXT("core_shard")));
	const int32 N = FMath::RoundToInt(DNum(S, TEXT("count"), 5.f));
	GiveOrBag(PS, Item, N, Altar->GetActorLocation() + FVector(0, 0, 60.f));   // a full bag: they wait by the altar
	WRSound::Broadcast(const_cast<AActor*>(Altar), TEXT("endgame.forge"));
	const FWRItemDef* I = FWRItemRegistry::Get().Find(Item);
	return FString::Printf(TEXT("The soul gives up %d %s"), N, I ? *I->Name : *Item.ToString());
}

// ================================================================================================ mythic (data-ready)

FString WRDeeds::MythicCheck(const AWRPlayerState* PS, int32 BagIndex)
{
	const TSharedPtr<FJsonObject> M = Mythic();
	if (!DBool(M, TEXT("live"))) return TEXT("The Ring on the Crown isn't in this world yet");
	const UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (!Bag || !Bag->GetSlots().IsValidIndex(BagIndex)) return TEXT("Not now");
	const FWRItemStack& S = Bag->GetSlots()[BagIndex];
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(S.ItemId);
	const TSharedPtr<FJsonObject> Base = DChild(M, TEXT("base"));
	if (!Def || Def->Metal.ToString() != DStr(Base, TEXT("metal"), TEXT("starsteel"))) return TEXT("A starsteel weapon or ring");
	if (S.Quality != EWRQuality::Masterwork) return TEXT("It must be masterwork");
	if (S.Upgrade < int32(DNum(Base, TEXT("upgrade"), 10.f))) return TEXT("It must be at +10");
	if (S.Mark == TEXT("mythic")) return TEXT("It's mythic already");
	if (Bag->CountOf(FName(*DStr(Base, TEXT("markItem"), TEXT("champion_mark")))) < int32(DNum(Base, TEXT("marks"), 3.f))) return TEXT("Three Champion's marks");
	const TSharedPtr<FJsonObject> Pillars = DChild(DChild(M, TEXT("offer")), TEXT("pillars"));
	const int32 Per = int32(DNum(DChild(M, TEXT("offer")), TEXT("perPillar"), 10.f));
	TArray<FString> Missing;
	if (Pillars.IsValid())
		for (const auto& P : Pillars->Values)
			if (Bag->CountOf(FName(*P.Value->AsString())) < Per) Missing.Add(P.Value->AsString());
	return Missing.Num() > 0 ? FString::Printf(TEXT("Still to offer: %s"), *FString::Join(Missing, TEXT(", "))) : FString();
}

// ================================================================================================ titles and cosmetics

void WRDeeds::GiveTitle(AWRPlayerState* PS, const FString& Title, const FString& Why)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	if (!D || Title.IsEmpty() || D->Titles.Contains(Title)) return;
	D->Titles.Add(Title);
	D->bDirty = true;
	Tell(PS, FString::Printf(TEXT("A title: %s (%s). Wear it from Deeds > Titles."), *Title, *Why));
	UE_LOG(LogWildreach, Log, TEXT("DEEDS %s earns the title %s"), *PS->GetPlayerName(), *Title);
}

void WRDeeds::GiveCosmetic(AWRPlayerState* PS, FName Id, const FString& Why)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	FString Kind;
	const TSharedPtr<FJsonObject> Row = Cosmetic(Id, &Kind);
	if (!D || !Row.IsValid() || D->Cosmetics.Contains(Id)) return;
	D->Cosmetics.Add(Id);
	D->bDirty = true;
	Tell(PS, FString::Printf(TEXT("A %s: %s (%s). Wear it from Deeds > Cosmetics."), Kind == TEXT("glow") ? TEXT("weapon glow") : Kind == TEXT("dye") ? TEXT("dye") : TEXT("raid mark"), *DStr(Row, TEXT("name")), *Why));
	UE_LOG(LogWildreach, Log, TEXT("DEEDS %s earns the cosmetic %s"), *PS->GetPlayerName(), *Id.ToString());
}

FString WRDeeds::WearTitle(AWRPlayerState* PS, const FString& Title)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	if (!D) return TEXT("Not now");
	// The artifacts' Banes count as earned titles too.
	const UWRArtifactComponent* A = UWRArtifactComponent::For(PS);
	if (!Title.IsEmpty() && !D->Titles.Contains(Title) && !(A && A->Titles.Contains(Title))) return TEXT("Not a title of yours");
	D->ActiveTitle = Title;
	D->bDirty = true;
	return Title.IsEmpty() ? TEXT("No title worn") : FString::Printf(TEXT("You wear the title %s"), *Title);
}

FString WRDeeds::WearCosmetic(AWRPlayerState* PS, const FString& Kind, FName Id)
{
	UWRDeedsComponent* D = UWRDeedsComponent::For(PS);
	if (!D) return TEXT("Not now");
	if (Kind == TEXT("sigil"))
	{
		if (!Id.IsNone() && !D->Bosses.Contains(Id)) return TEXT("Only the sigil of a boss you've beaten");
		if (!Id.IsNone() && Sigil(Id).Num() == 0) return TEXT("That boss has no sigil");
		D->BannerSigil = Id;
	}
	else
	{
		FString RowKind;
		if (!Id.IsNone() && (!Cosmetic(Id, &RowKind).IsValid() || RowKind != Kind || !D->Cosmetics.Contains(Id))) return TEXT("Not one of yours");
		if (Kind == TEXT("glow")) D->WornGlow = Id;
		else if (Kind == TEXT("dye")) D->WornDye = Id;
		else if (Kind == TEXT("mark")) D->WornMark = Id;
		else return TEXT("Not now");
	}
	D->bDirty = true;
	D->RefreshLook();
	// Banners redraw with the new sigil (every banner of yours).
	if (Kind == TEXT("sigil") && PS->GetWorld()) WRRecords::RefreshBanners(PS->GetWorld(), PS->GetPlayerName());
	return Id.IsNone() ? TEXT("Taken off") : FString::Printf(TEXT("Worn: %s"), Kind == TEXT("sigil") ? *Id.ToString() : *DStr(Cosmetic(Id), TEXT("name")));
}

// ================================================================================================ equipment

FName WRDeeds::SoulArtFor(const AActor* Who)
{
	const AWRPlayerState* PS = DStateOf(Who);
	const UWRInventoryComponent* Bag = PS ? PS->GetInventory() : nullptr;
	if (!Bag) return NAME_None;
	const FWRItemStack& W = Bag->GetEquipped(EWREquipSlot::Weapon);
	if (W.IsEmpty() || W.Mark != TEXT("awakened")) return NAME_None;
	const FName Boss = SoulBossOfWeapon(W.ItemId);
	return Boss.IsNone() ? NAME_None : FName(*FString::Printf(TEXT("soul.%s"), *Boss.ToString()));
}

float WRDeeds::ResourceGainScale(const AActor* Who)
{
	// Forge-heart: path resource +10% while above 70% health.
	const float Gain = UWRInventoryComponent::GearStatFor(Who, TEXT("resourceGainHighHp"));
	if (Gain <= 0.f) return 1.f;
	const AWRCharacterBase* C = Cast<AWRCharacterBase>(Who);
	const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
	return A && A->GetHealth() > WRTune(TEXT("deeds.talismans.forgeHeartAbove"), 0.7f) * A->GetMaxHealth() ? 1.f + Gain : 1.f;
}

float WRDeeds::FirstHitScale(AActor* Victim)
{
	// Rime-scale: the first blow after 10 s unhurt deals 30% less.
	UWRDeedsComponent* D = UWRDeedsComponent::For(Victim);
	if (!D) return 1.f;
	const double Now = Victim->GetWorld()->GetTimeSeconds();
	const float Guard = UWRInventoryComponent::GearStatFor(Victim, TEXT("firstHitGuard"));
	const bool bQuiet = Now - D->LastHurtAt >= WRTune(TEXT("deeds.talismans.rimeScaleQuiet"), 10.f);
	D->LastHurtAt = Now;
	return Guard > 0.f && bQuiet ? FMath::Max(0.f, 1.f - Guard) : 1.f;
}

// ================================================================================================ commands

FString WRDeeds::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || Args.Num() < 2) return FString();
	const FString& Verb = Args[1];
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	if (Verb == TEXT("return")) return Prestige(PS, FName(*Arg(2)), FName(*Arg(3)), AltarNear(PS, FName(*Arg(2))));
	if (Verb == TEXT("restore")) return RestoreLayout(PS, FName(*Arg(2)));
	if (Verb == TEXT("forget")) return Respec(PS, FName(*Arg(2)));
	if (Verb == TEXT("forge")) return ForgeSoul(PS, FName(*Arg(2)), Arg(3) == TEXT("weapon"), AltarNear(PS, NAME_None));
	if (Verb == TEXT("salvage")) return SalvageSoul(PS, FName(*Arg(2)), AltarNear(PS, NAME_None));
	if (Verb == TEXT("subclass"))
	{
		// The altar's built rule (take its path as your subclass), from the altar screen.
		if (const AWRBossAltar* A = AltarNear(PS, FName(*Arg(2)))) return const_cast<AWRBossAltar*>(A)->Interact(PC);
		return TEXT("Only at its altar");
	}
	if (Verb == TEXT("title"))
	{
		FString T;
		for (int32 i = 2; i < Args.Num(); ++i) T += (i > 2 ? TEXT(" ") : TEXT("")) + Args[i];
		return WearTitle(PS, T);
	}
	if (Verb == TEXT("wear")) return WearCosmetic(PS, Arg(2), Arg(3).IsEmpty() || Arg(3) == TEXT("none") ? NAME_None : FName(*Arg(3)));
	// deeds records <board | all> [season]: the summary of every board, or one board in full (season 0: this one).
	if (Verb == TEXT("records")) { WRRecords::SendBoards(PC, Arg(2).IsEmpty() || Arg(2) == TEXT("all") ? NAME_None : FName(*Arg(2)), FMath::Clamp(FCString::Atoi(*Arg(3)), 0, 999)); return FString(); }
#if !UE_BUILD_SHIPPING
	// Testing aids (Aaron tests with these on Development builds): deeds dev give <achievement> / soul <boss> / marks.
	if (Verb == TEXT("dev"))
	{
		if (Arg(2) == TEXT("give")) { Notify(PS, TEXT("dev"), FName(*Arg(3))); return TEXT("ok"); }
		if (Arg(2) == TEXT("soul")) { if (const TSharedPtr<FJsonObject> R = SoulRow(FName(*Arg(3)))) PS->GetInventory()->AddItem(FName(*DStr(R, TEXT("soul"))), 1); return TEXT("ok"); }
		if (Arg(2) == TEXT("cosmetic")) { GiveCosmetic(PS, FName(*Arg(3)), TEXT("dev")); return TEXT("ok"); }
	}
#endif
	return FString();
}

bool WRDeeds::ClientInteract(AWRPlayerController* PC)
{
	// The altar's screen: its path, Return, Forge soul (the old one-line interact stays for -oldui).
	if (!PC || !PC->GetUI()) return false;
	if (AWRBossAltar* Altar = PC->FindNearbyAltar())
	{
		PC->ClientOpenWorldScreen_Implementation(TEXT("altar"), Altar);
		return true;
	}
	return false;
}

// ================================================================================================ the owning client

void AWRPlayerController::ClientDeed_Implementation(FName Id)
{
	// The toast queue (WRUIEndgame draws them one at a time, 3 s each) and its chime.
	DeedToasts.Add(Id);
	WRSound::PlayUI(this, TEXT("endgame.deed"));
}

void AWRPlayerController::ClientRecords_Implementation(FName Board, const FString& Json)
{
	WRRecords::ClientReceive(Board, Json);
}
