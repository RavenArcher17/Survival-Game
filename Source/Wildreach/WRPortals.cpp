// The portals' server side (cooks/build-portals-artifacts.md 2.2-2.11): UWRPortalSystem (slots, placement, the real-day
// clocks, spilling, escaped bosses, keys, runs in bays, entry, clearing, closing, rewards, the save) and the hooks the
// rest of the game calls. The portal's and the fold's looks are in WRPortalFold.cpp, the fold's doors / traps / puzzles /
// chests in WRPortalThings.cpp, the bosses' arenas in WRPortalBosses.cpp, the screens in WRUIPortals.cpp.

#include "WRPortals.h"
#include "WRForestTypes.h"
#include "WRPortalsInternal.h"
#include "WRArtifacts.h"
#include "WRDeeds.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameMode.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRGear.h"
#include "WRInventory.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRSwampTypes.h"
#include "WRTuning.h"
#include "WRWildlife.h"
#include "WRWorlds.h"
#include "WRStore.h"
#include "WRWorldSystems.h"
#include "WRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

using namespace WRPortalsImpl;

namespace WRPortalsSys
{
	TSharedPtr<FJsonObject>& PsCached() { static TSharedPtr<FJsonObject> D; return D; }
	TMap<FName, TSharedPtr<FJsonObject>>& PsKits() { static TMap<FName, TSharedPtr<FJsonObject>> K; return K; }

	AWRPlayerState* PsStateOf(const FString& Name, UWorld* World)
	{
		if (!World || !World->GetGameState()) return nullptr;
		for (APlayerState* P : World->GetGameState()->PlayerArray) if (P && P->GetPlayerName() == Name) return Cast<AWRPlayerState>(P);
		return nullptr;
	}
	AWRPlayerController* PsControllerOf(AWRPlayerState* PS) { return PS ? Cast<AWRPlayerController>(PS->GetOwner()) : nullptr; }
	FString PsNameOf(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		const APlayerState* PS = P ? P->GetPlayerState() : Cast<APlayerState>(Who);
		if (!PS) if (const AController* C = Cast<AController>(Who)) PS = C->PlayerState;
		return PS ? PS->GetPlayerName() : FString();
	}
	double PsGameHours(const UObject* Ctx) { const AWRGameState* GS = AWRGameState::Get(Ctx); return GS ? GS->GetTotalHours() : 0.0; }

	/** Survivors (alive) and their names. */
	template <typename F> void PsForPlayers(UWorld* World, F&& Fn)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC) Fn(PC);
		}
	}

	/** The land's creature that stands for a band (CanStand: the side, the band, off sea and cliff, clear of camps). */
	FName PsStandAs(FName Biome, FName Band)
	{
		static const TMap<FString, FName> M = {
			{ TEXT("volcanic.lowland"), TEXT("cinderfang") }, { TEXT("volcanic.midland"), TEXT("basaltback") }, { TEXT("volcanic.highland"), TEXT("magmaw") },
			{ TEXT("tundra.lowland"), TEXT("gloomhowl") }, { TEXT("tundra.midland"), TEXT("frostantler") }, { TEXT("tundra.highland"), TEXT("rimeclaw") },
			{ TEXT("storm_peaks.lowland"), TEXT("voltfang") }, { TEXT("storm_peaks.midland"), TEXT("voltfang") }, { TEXT("storm_peaks.highland"), TEXT("voltfang") },
			// The Swamp's walkers (cooks/biome-swamp.md 2.12): the coast's Mudhulks, the domes' Bogsnouts, the Crown foot's Bogstalkers.
			{ TEXT("swamp.lowland"), TEXT("mudhulk") }, { TEXT("swamp.midland"), TEXT("bogsnout") }, { TEXT("swamp.highland"), TEXT("bogstalker") },
			// The Ancient forest's walkers (cooks/biome-forest.md 2.12): the meadows' Trufflers, the groves' Thornlynx, the
			// Elder Canopy's Mossmauls (its portals stay land:false until a forest fold kit exists, like Storm's).
			{ TEXT("forest.lowland"), TEXT("truffler") }, { TEXT("forest.midland"), TEXT("thornlynx") }, { TEXT("forest.highland"), TEXT("mossmaul") } };
		const FName* F = M.Find(Biome.ToString() + TEXT(".") + Band.ToString());
		return F ? *F : NAME_None;
	}

	/** Item counts from a reward table: "@materials" a random land material, "@essence" the land's essence, "@gem" a gem. */
	void PsResolve(const TSharedPtr<FJsonObject>& Table, FName Biome, float PsQuantity, TMap<FName, int32>& Out)
	{
		if (!Table.IsValid()) return;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Table->Values)
		{
			int32 N = 0;
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (KV.Value->TryGetArray(A) && A->Num() >= 2) N = FMath::RandRange(int32((*A)[0]->AsNumber()), int32((*A)[1]->AsNumber()));
			else N = FMath::RoundToInt(KV.Value->AsNumber());
			if (N <= 0) continue;
			const int32 Q = FMath::Max(1, FMath::RoundToInt(N * PsQuantity));
			if (KV.Key == TEXT("@materials"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Mats = PArr(WRPortals::Obj(TEXT("materials")), Biome.ToString());
				if (!Mats || Mats->Num() == 0) continue;
				// Spread over two of the land's materials, so a chest isn't one stack.
				const FName A1((*Mats)[FMath::RandHelper(Mats->Num())]->AsString());
				const FName A2((*Mats)[FMath::RandHelper(Mats->Num())]->AsString());
				Out.FindOrAdd(A1) += (Q + 1) / 2;
				Out.FindOrAdd(A2) += Q / 2;
			}
			else if (KV.Key == TEXT("@essence"))
			{
				const FString E = PStr(PChild(WRPortals::Obj(TEXT("biomes")), Biome.ToString()), TEXT("essence"));
				if (!E.IsEmpty()) Out.FindOrAdd(FName(*E)) += Q;
			}
			else if (KV.Key == TEXT("@gem"))
			{
				const TArray<TSharedPtr<FJsonValue>>* G = PArr(WRPortals::Data(), TEXT("gems"));
				for (int32 i = 0; G && G->Num() && i < N; ++i) Out.FindOrAdd(FName((*G)[FMath::RandHelper(G->Num())]->AsString())) += 1;
			}
			else if (KV.Key == TEXT("@rune"))
			{
				const TArray<TSharedPtr<FJsonValue>>* G = PArr(WRPortals::Data(), TEXT("runes"));
				if (G && G->Num()) Out.FindOrAdd(FName((*G)[FMath::RandHelper(G->Num())]->AsString())) += 1;
			}
			else if (!KV.Key.StartsWith(TEXT("@")))
			{
				Out.FindOrAdd(FName(*KV.Key)) += Q;
			}
		}
	}
	/** Chance table: each "@kind" or item that comes up, once. */
	void PsChances(const TSharedPtr<FJsonObject>& Table, FName Biome, TMap<FName, int32>& Out, TArray<FName>& OutSpecial)
	{
		if (!Table.IsValid()) return;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Table->Values)
		{
			if (FMath::FRand() >= KV.Value->AsNumber()) continue;
			if (KV.Key == TEXT("@fineGear") || KV.Key == TEXT("@masterwork")) { OutSpecial.Add(FName(*KV.Key)); continue; }
			const TSharedRef<FJsonObject> One = MakeShared<FJsonObject>();
			One->SetNumberField(KV.Key, 1);
			PsResolve(One, Biome, 1.f, Out);
		}
	}

	/** Gives the items (a full bag drops the rest in a bag at the player's feet, only theirs). Returns the line. */
	FString PsGive(AWRPlayerState* PS, const TMap<FName, int32>& Items, const TArray<FWRItemStack>& Instances)
	{
		if (!PS || !PS->GetInventory()) return FString();
		TArray<FString> Said;
		TArray<FWRItemStack> Over;
		for (const TPair<FName, int32>& KV : Items)
		{
			if (KV.Value <= 0) continue;
			const int32 Left = PS->GetInventory()->AddItem(KV.Key, KV.Value);
			if (Left > 0) { FWRItemStack Rest = FWRInventoryRules::MakeInstance(KV.Key); Rest.Count = Left; Over.Add(Rest); }
			const FWRItemDef* D = FWRItemRegistry::Get().Find(KV.Key);
			Said.Add(FString::Printf(TEXT("%s x%d"), D ? *D->Name : *KV.Key.ToString(), KV.Value));
		}
		for (const FWRItemStack& S : Instances)
		{
			if (!PS->GetInventory()->AddInstance(S)) Over.Add(S);
			const FWRItemDef* D = FWRItemRegistry::Get().Find(S.ItemId);
			Said.Add(FString::Printf(TEXT("%s %s"), FWRItemStack::QualityName(S.Quality), D ? *D->Name : *S.ItemId.ToString()));
		}
		if (Over.Num() > 0)
		{
			const APawn* Pawn = PS->GetPawn();
			if (Pawn)
			{
				if (AWRLootBag* Bag = AWRLootBag::Spill(Pawn->GetWorld(), Pawn->GetActorLocation() - FVector(0, 0, Pawn->GetSimpleCollisionHalfHeight() - 20.f), PS->GetPlayerName(), Over))
				{
					Bag->bOwnerOnly = true;
				}
			}
		}
		return FString::Join(Said, TEXT(", "));
	}

	/** A piece of gear at a set quality. */
	FWRItemStack PsGearAt(FName Id, EWRQuality Q)
	{
		return FWRInventoryRules::MakeInstance(Id, Q, FMath::FRandRange(0.97f, 1.08f));
	}

	/** The loot quantity a portal pays a player: x (1 + 0.25 a modifier) x the party's artifact loot scale. */
	float PsQuantity(const AWRFold* F, const AActor* Who)
	{
		const float PerMod = WRPortals::Num(TEXT("lootPerModifier"), 0.25f);
		return (1.f + PerMod * (F ? F->Modifiers.Num() : 0)) * WRArtifacts::LootScale(Who);
	}

	/** Spawns a creature with a fold's rank scaling (set before it begins play). */
	AWRCreature* PsSpawnScaled(UWorld* World, FName Id, const FVector& At, float Yaw, float HealthMult, float DamageMult, float StanceMult, FColor Mark)
	{
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Id);
		if (!World || !Def) return nullptr;
		UClass* Class = Def->Archetype == TEXT("boss") ? AWRBoss::StaticClass() : AWRCreature::StaticClass();
		const FTransform T(FRotator(0.f, Yaw, 0.f), At + FVector(0.f, 0.f, FMath::Max(Def->HalfHeight, Def->Radius) + 5.f));
		AWRCreature* C = World->SpawnActorDeferred<AWRCreature>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!C) return nullptr;
		C->CreatureId = Id;
		C->PortalHealth = HealthMult;
		C->PortalDamage = DamageMult;
		C->PortalStance = StanceMult;
		C->PortalMark = Mark;
		C->FinishSpawning(T);
		return C;
	}

	/** The rank's scaling for a creature of this tier. */
	void PsScaleFor(int32 Rank, FName Tier, float& H, float& D, float& S)
	{
		const TSharedPtr<FJsonObject> Row = PChild(PChild(WRPortals::Rank(Rank), TEXT("creatures")), Tier.ToString());
		H = PNum(Row, TEXT("health"), 1.f);
		D = PNum(Row, TEXT("damage"), 1.f);
		S = PNum(Row, TEXT("stance"), 1.f);
	}

	/** Point a teleport so the pawn stands on it (feet on the floor). */
	void PsPut(APawn* Pawn, const FVector& Floor, float Yaw)
	{
		if (!Pawn) return;
		const FRotator Face(0.f, Yaw, 0.f);
		Pawn->TeleportTo(Floor + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() + 30.f), Face, false, true);
		if (AController* C = Pawn->GetController()) C->SetControlRotation(Face);
	}

	/** Following tames of this player within R of From. */
	TArray<AWRCreature*> PsTamesNear(UWorld* World, const FString& Name, const FVector& From, float R)
	{
		TArray<AWRCreature*> Out;
		for (TActorIterator<AWRCreature> It(World); It; ++It)
		{
			if (It->IsAlive() && It->TamedBy == Name && !It->Rider.IsValid() && It->TameOrder == EWRTameOrder::Follow && FVector::Dist(It->GetActorLocation(), From) < R) Out.Add(*It);
		}
		return Out;
	}

	FVector PsRandIn2D(const FVector& C, float R)
	{
		const float A = FMath::FRand() * 2.f * PI;
		const float D = FMath::Sqrt(FMath::FRand()) * R;
		return C + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f);
	}
}
using namespace WRPortalsSys;

// ================================================================================================ WRPortals

TSharedPtr<FJsonObject> WRPortals::Data()
{
	TSharedPtr<FJsonObject>& D = PsCached();
	if (!D.IsValid()) D = FWRTuning::LoadDataFile(TEXT("portals.json"));
	if (!D.IsValid()) D = MakeShared<FJsonObject>();
	return D;
}

TSharedPtr<FJsonObject> WRPortals::Obj(const FString& Path)
{
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));
	TSharedPtr<FJsonObject> O = Data();
	for (const FString& P : Parts) { O = PChild(O, P); if (!O.IsValid()) return nullptr; }
	return O;
}

float WRPortals::Num(const FString& Path, float Fallback)
{
	FString Head, Leaf;
	if (!Path.Split(TEXT("."), &Head, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) return PNum(Data(), Path, Fallback);
	return PNum(Obj(Head), Leaf, Fallback);
}

TSharedPtr<FJsonObject> WRPortals::Kit(FName Biome)
{
	if (Biome.IsNone()) return nullptr;
	TMap<FName, TSharedPtr<FJsonObject>>& K = PsKits();
	if (const TSharedPtr<FJsonObject>* F = K.Find(Biome)) return *F;
	const FString KitName = PStr(PChild(Obj(TEXT("biomes")), Biome.ToString()), TEXT("kit"), Biome.ToString());
	TSharedPtr<FJsonObject> Loaded = FWRTuning::LoadDataFile(FString::Printf(TEXT("portal_rooms/%s.json"), *KitName));
	K.Add(Biome, Loaded);
	return Loaded;
}

TSharedPtr<FJsonObject> WRPortals::Room(FName Biome, FName RoomId)
{
	const TArray<TSharedPtr<FJsonValue>>* Rooms = PArr(Kit(Biome), TEXT("rooms"));
	if (!Rooms) return nullptr;
	for (const TSharedPtr<FJsonValue>& V : *Rooms)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (O.IsValid() && PStr(O, TEXT("id")) == RoomId.ToString()) return O;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> WRPortals::Rank(int32 R)
{
	const TArray<TSharedPtr<FJsonValue>>* Ranks = PArr(Data(), TEXT("ranks"));
	return Ranks && Ranks->IsValidIndex(R - 1) ? (*Ranks)[R - 1]->AsObject() : nullptr;
}

FColor WRPortals::RankColour(int32 R) { return PColour(Rank(R), TEXT("colour"), FColor(232, 226, 208)); }
FString WRPortals::RankName(int32 R) { return PStr(Rank(R), TEXT("name"), TEXT("Common")); }
FString WRPortals::BiomeName(FName Biome) { return PStr(PChild(Obj(TEXT("biomes")), Biome.ToString()), TEXT("name"), Biome.ToString()); }

namespace WRPortalsSys
{
	TSharedPtr<FJsonObject> PsModRow(FName Id)
	{
		const TArray<TSharedPtr<FJsonValue>>* M = PArr(WRPortals::Data(), TEXT("modifiers"));
		if (M) for (const TSharedPtr<FJsonValue>& V : *M) if (PStr(V->AsObject(), TEXT("id")) == Id.ToString()) return V->AsObject();
		return nullptr;
	}
}

FString WRPortals::ModifierName(FName Id) { return PStr(PsModRow(Id), TEXT("name"), Id.ToString()); }
FString WRPortals::ModifierLine(FName Id) { return PStr(PsModRow(Id), TEXT("line")); }
float WRPortals::ModNum(FName Mod, const TCHAR* Field, float Fallback) { return PNum(PsModRow(Mod), Field, Fallback); }

bool WRPortals::InBaySpace(const FVector& P)
{
	static const float X = WRPortals::Num(TEXT("bay.x"), 1500000.f);
	static const float Z = WRPortals::Num(TEXT("bay.z"), 200000.f);
	return P.X > X - 60000.f && P.Z > Z - 30000.f;
}

AWRFold* WRPortals::FoldAt(const UObject* WorldContext, const FVector& Point)
{
	if (!InBaySpace(Point)) return nullptr;
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	for (const TWeakObjectPtr<AWRFold>& W : AWRFold::All())
	{
		AWRFold* F = W.Get();
		if (F && F->GetWorld() == World && F->Bounds.IsValid && F->Bounds.IsInsideOrOn(Point)) return F;
	}
	return nullptr;
}

AWRFold* WRPortals::FoldOf(const AActor* Who)
{
	if (!Who) return nullptr;
	const AActor* Body = Who;
	if (const APlayerState* PS = Cast<APlayerState>(Who)) Body = PS->GetPawn();
	else if (const AController* C = Cast<AController>(Who)) Body = C->GetPawn();
	return Body ? FoldAt(Body, Body->GetActorLocation()) : nullptr;
}

bool WRPortals::Modifier(const AActor* Who, FName Mod)
{
	const AWRFold* F = FoldOf(Who);
	return F && F->Modifiers.Contains(Mod);
}

bool WRPortals::IsFold(const UObject* WorldContext, const FVector& Point) { return FoldAt(WorldContext, Point) != nullptr; }

FName WRPortals::FoldBiome(const UObject* WorldContext, const FVector& Point)
{
	const AWRFold* F = FoldAt(WorldContext, Point);
	return F ? F->Biome : NAME_None;
}

float WRPortals::DamageScale(const AWRCharacterBase* Target, const AWRCharacterBase* Attacker, const FWRHit& Hit)
{
	float S = 1.f;
	// Double damage: everyone, both ways, in its fold (the world's own traps too).
	if (Target && Modifier(Target, TEXT("double_damage"))) S *= ModNum(TEXT("double_damage"), TEXT("mult"), 2.f);
	if (Attacker && !Attacker->GetPlayerState()) S *= BossOutgoing(Attacker);
	return S;
}

bool WRPortals::HealBlocked(const AActor* Who) { return Who && Modifier(Who, TEXT("no_healing")); }
float WRPortals::LightScale(const AActor* Who) { return Who && Modifier(Who, TEXT("darkness")) ? ModNum(TEXT("darkness"), TEXT("light"), 0.5f) : 1.f; }
float WRPortals::WearScale(const AActor* Who) { return Who && Modifier(Who, TEXT("brittle_gear")) ? ModNum(TEXT("brittle_gear"), TEXT("wear"), 3.f) : 1.f; }
float WRPortals::CreaturePace(const AActor* Creature) { return Creature && Modifier(Creature, TEXT("frenzy")) ? ModNum(TEXT("frenzy"), TEXT("speed"), 1.15f) : 1.f; }
bool WRPortals::CreatureIronHide(const AActor* Creature) { return Creature && Modifier(Creature, TEXT("iron_hide")); }

float WRPortals::BuildupScale(const AActor* Attacker, const AActor* Target)
{
	const APawn* A = Cast<APawn>(Attacker);
	if (!A || A->GetPlayerState() || !Target) return 1.f;
	// The Rime-Witch's freeze can't take hold of anyone standing in a lit brazier's light.
	if (const AWRCreature* C = Cast<AWRCreature>(A); C && C->CreatureId == TEXT("rime_witch"))
	{
		const float R = WRTune(TEXT("bosses.rime_witch.lightRadius"), 500.f);
		for (TActorIterator<AWRFoldThing> It(A->GetWorld()); It; ++It)
		{
			if (It->Kind == TEXT("arenabrazier") && It->State == 1 && FVector::Dist2D(It->GetActorLocation(), Target->GetActorLocation()) < R) return 0.f;
		}
	}
	return Modifier(Target, TEXT("venomous")) ? ModNum(TEXT("venomous"), TEXT("buildup"), 1.5f) : 1.f;
}

// ------------------------------------------------------------------------------------------------ hooks: bosses

float WRPortals::BossIncoming(AWRBoss* Boss, const FWRHit& Hit, float Damage)
{
	UWRPortalBossBrain* B = Boss ? Boss->FindComponentByClass<UWRPortalBossBrain>() : nullptr;
	return B ? B->Incoming(Hit, Damage) : Damage;
}

float WRPortals::BossOutgoing(const AWRCharacterBase* Attacker)
{
	const UWRPortalBossBrain* B = Attacker ? Attacker->FindComponentByClass<UWRPortalBossBrain>() : nullptr;
	return B ? B->Outgoing() : 1.f;
}

void WRPortals::OnBossAttack(AWRBoss* Boss, FName AttackId)
{
	if (UWRPortalBossBrain* B = Boss ? Boss->FindComponentByClass<UWRPortalBossBrain>() : nullptr) B->OnAttack(AttackId);
}

bool WRPortals::AttackAllowed(const AWRBoss* Boss, FName AttackId)
{
	const UWRPortalBossBrain* B = Boss ? Boss->FindComponentByClass<UWRPortalBossBrain>() : nullptr;
	if (!B) return true;
	const FName Id = Boss->CreatureId;
	if (Id == TEXT("brannoch") && AttackId == TEXT("bellows_cone") && B->VentsBroken >= 2) return false;
	return true;
}

float WRPortals::StanceScale(const AWRCharacterBase* Target)
{
	const UWRPortalBossBrain* B = Target ? Target->FindComponentByClass<UWRPortalBossBrain>() : nullptr;
	const AWRCreature* C = Cast<AWRCreature>(Target);
	if (!B || !C) return 1.f;
	const double Now = Target->GetWorld()->GetTimeSeconds();
	if (C->CreatureId == TEXT("rimeclaw_alpha") && Now >= B->PlatesMeltedUntil) return WRTune(TEXT("bosses.rimeclaw_alpha.platedStance"), 0.5f);
	if (C->CreatureId == TEXT("mould_born") && Now < B->HardenedUntil) return 1.f / FMath::Max(0.1f, WRTune(TEXT("bosses.mould_born.frostStance"), 1.5f));
	return 1.f;
}

void WRPortals::OnBossPart(AWRBoss* Boss, FName PartId, FName OnBreak)
{
	if (UWRPortalBossBrain* B = Boss ? Boss->FindComponentByClass<UWRPortalBossBrain>() : nullptr) B->OnPart(PartId, OnBreak);
}

void WRPortals::OnKilled(AWRCharacterBase* Victim, AActor* Killer)
{
	UWRPortalSystem* Sys = Victim ? UWRPortalSystem::Get(Victim) : nullptr;
	if (!Sys || !Sys->IsServer()) return;
	if (AWRBoss* B = Cast<AWRBoss>(Victim))
	{
		if (const UWRPortalBossBrain* Brain = B->FindComponentByClass<UWRPortalBossBrain>())
		{
			if (Brain->bEscaped) Sys->EscapedDied(B);
			else Sys->BossDied(B);
		}
	}
}

bool WRPortals::HitThing(AActor* Thing, AWRCharacterBase* By, float Damage, bool bFire)
{
	AWRFoldThing* T = Cast<AWRFoldThing>(Thing);
	if (!T && Thing) T = Cast<AWRFoldThing>(Thing->GetOwner());
	if (!T || !T->HasAuthority()) return false;
	T->TakeHit(By, Damage, bFire);
	return true;
}

// ------------------------------------------------------------------------------------------------ hooks: interact

namespace WRPortalsSys
{
	struct FPsNear { AActor* Actor = nullptr; float Dist = 1.0e9f; };
	FPsNear PsNearest(const AWRPlayerController* PC)
	{
		FPsNear N;
		const APawn* Me = PC ? PC->GetPawn() : nullptr;
		if (!Me) return N;
		const FVector At = Me->GetActorLocation();
		const float PortalRange = WRPortals::Num(TEXT("entry.interactRange"), 450.f);
		for (TActorIterator<AWRPortal> It(Me->GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), At);
			if (D < PortalRange && D < N.Dist && It->OpenAt <= PServerNow(Me->GetWorld())) { N.Actor = *It; N.Dist = D; }
		}
		if (WRPortals::InBaySpace(At))
		{
			for (TActorIterator<AWRFoldThing> It(Me->GetWorld()); It; ++It)
			{
				if (!It->CanInteract(PC)) continue;
				const float Reach = It->Kind == TEXT("stone") || It->Kind == TEXT("tank") || It->Kind == TEXT("crucible") ? 350.f : 260.f;
				const float D = FVector::Dist2D(It->GetActorLocation(), At);
				if (D < Reach && FMath::Abs(It->GetActorLocation().Z - At.Z) < 400.f && D < N.Dist) { N.Actor = *It; N.Dist = D; }
			}
		}
		return N;
	}
}

bool WRPortals::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const FPsNear N = PsNearest(PC);
	if (const AWRPortal* P = Cast<AWRPortal>(N.Actor))
	{
		OutTarget = FString::Printf(TEXT("%s portal · %s"), *RankName(P->Rank), *BiomeName(P->Biome));
		OutVerb = TEXT("Look into the fold");
		return true;
	}
	if (const AWRFoldThing* T = Cast<AWRFoldThing>(N.Actor))
	{
		OutTarget = T->Title();
		OutVerb = T->Verb(PC);
		return !OutVerb.IsEmpty();
	}
	return false;
}

bool WRPortals::ClientInteract(AWRPlayerController* PC)
{
	const FPsNear N = PsNearest(PC);
	if (AWRPortal* P = Cast<AWRPortal>(N.Actor))
	{
		PC->ClientOpenWorldScreen_Implementation(TEXT("portal"), P);
		return true;
	}
	if (AWRFoldThing* T = Cast<AWRFoldThing>(N.Actor))
	{
		if (T->Verb(PC).IsEmpty()) return false;
		const FVector L = T->GetActorLocation();
		PC->SendCommand(FString::Printf(TEXT("portal act %.1f %.1f %.1f"), L.X, L.Y, L.Z));
		return true;
	}
	return false;
}

FString WRPortals::EnterBlock(const AWRPlayerController* PC, const AWRPortal* Portal)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !Portal || !PC->PlayerState) return TEXT("Not now");
	const FString Name = PC->PlayerState->GetPlayerName();
	if (Portal->OpenAt > PServerNow(Me->GetWorld())) return TEXT("It's still opening");
	if (Portal->bClosing) return TEXT("The fold is closing");
	if (Portal->IsSealedTo(Name)) return FString::Printf(TEXT("Sealed to %s's party"), *Portal->OwnerName);
	if (Portal->bSealed) return TEXT("The boss door is sealed: wait");
	if (FVector::Dist(Me->GetActorLocation(), Portal->GetActorLocation()) > WRPortals::Num(TEXT("entry.interactRange"), 450.f) * 2.f) return TEXT("Too far");
	if (const AWRCharacterBase* C = Cast<AWRCharacterBase>(Me))
	{
		if (C->HasTag(WRTags::State_Mounted)) return TEXT("Get down from your mount");
		if (!C->IsAlive()) return TEXT("Not now");
	}
	if (!Portal->bRunOpen)
	{
		const UWRPortalSystem* Sys = UWRPortalSystem::Get(Me);
		int32 Open = 0;
		for (TActorIterator<AWRPortal> It(Me->GetWorld()); It; ++It) if (It->bRunOpen) ++Open;
		const int32 Max = FMath::Clamp(FMath::RoundToInt(WRTune(TEXT("server.portalRuns"), WRPortals::Num(TEXT("entry.maxRuns"), 8.f))), 1, 8);
		if (Open >= Max) return FString::Printf(TEXT("All folds are full (%d of %d): one frees when a run ends"), Open, Max);
		(void)Sys;
	}
	return FString();
}

TArray<int32> WRPortals::SeenRooms(const AWRPlayerController* PC)
{
	TArray<int32> Out;
	const AWRFold* F = PC ? FoldOf(PC) : nullptr;
	if (!F || !PC->PlayerState) return Out;
	// The owning client keeps its own record (rooms it has stood in), so the map needs nothing sent.
	static TMap<FString, TArray<int32>> Seen;
	const FString Key = FString::Printf(TEXT("%s|%d|%s"), *F->GetName(), F->Bay, *PC->PlayerState->GetPlayerName());
	TArray<int32>& Mine = Seen.FindOrAdd(Key);
	if (const APawn* P = PC->GetPawn())
	{
		const int32 R = F->RoomAt(P->GetActorLocation(), 100.f);
		if (R != INDEX_NONE) Mine.AddUnique(R);
	}
	return Mine;
}

FString WRPortals::HudLine(const AWRPlayerController* PC)
{
	const AWRFold* F = FoldOf(PC);
	if (!F) return FString();
	TArray<FString> Parts;
	Parts.Add(FString::Printf(TEXT("%s fold · %s"), *RankName(F->Rank), *BiomeName(F->Biome)));
	for (const FName& M : F->Modifiers) Parts.Add(ModifierName(M));
	if (F->CloseAt > 0.f && PC->GetWorld())
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(F->CloseAt - PServerNow(PC->GetWorld())));
		Parts.Add(FString::Printf(TEXT("closes in %d:%02d"), S / 60, S % 60));
	}
	return FString::Join(Parts, TEXT("  ·  "));
}

// ------------------------------------------------------------------------------------------------ commands

FString WRPortals::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	UWRPortalSystem* Sys = UWRPortalSystem::Get(PC);
	if (!Sys || !Sys->IsServer() || Args.Num() < 2) return FString();
	const FString& Verb = Args[1];
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	if (Verb == TEXT("enter")) return Sys->Enter(PC, Sys->FindPortal(FCString::Atoi(*Arg(2))));
	if (Verb == TEXT("accept")) return Sys->Accept(PC, FCString::Atoi(*Arg(2)), Arg(3) == TEXT("1"));
	if (Verb == TEXT("leave")) return Sys->Leave(PC);
	if (Verb == TEXT("act"))
	{
		const FVector At(FCString::Atof(*Arg(2)), FCString::Atof(*Arg(3)), FCString::Atof(*Arg(4)));
		AWRFoldThing* Best = nullptr;
		float BestD = 120.f;
		for (TActorIterator<AWRFoldThing> It(PC->GetWorld()); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), At);
			if (D < BestD) { BestD = D; Best = *It; }
		}
		const APawn* Me = PC->GetPawn();
		if (!Best || !Me || FVector::Dist2D(Best->GetActorLocation(), Me->GetActorLocation()) > 600.f) return TEXT("Too far");
		return Best->Interact(PC);
	}
	if (Verb == TEXT("dev")) return Sys->Dev(PC, Args);
	return FString();
}

FString WRPortals::UseKey(AWRPlayerState* PS, FName ItemId)
{
	UWRPortalSystem* Sys = PS ? UWRPortalSystem::Get(PS) : nullptr;
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemId);
	if (!Sys || !Pawn || !Def) return TEXT("Not now");
	// Its land and rank from the id: portal_key_<biome>_<rank>.
	FString Rest = ItemId.ToString().RightChop(11);
	FString BiomeStr, RankStr;
	if (!Rest.Split(TEXT("_"), &BiomeStr, &RankStr, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) return TEXT("A key with no land");
	const FName Biome(*BiomeStr);
	const int32 Rank = FMath::Clamp(FCString::Atoi(*RankStr), 1, 5);
	if (WRPortals::InBaySpace(Pawn->GetActorLocation())) return TEXT("Not inside a fold");
	if (WRBiome::At(Pawn, Pawn->GetActorLocation()) != Biome) return FString::Printf(TEXT("This key opens only in the %s"), *BiomeName(Biome));
	const FString Held = Sys->RankBlocked(Biome, Rank);
	if (!Held.IsEmpty()) return Held;
	const FVector Ahead = Pawn->GetActorLocation() + Pawn->GetActorForwardVector().GetSafeNormal2D() * WRPortals::Num(TEXT("entry.keyDistance"), 3000.f) * 0.5f;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRPortalKey), false, Pawn);
	if (!Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Ahead + FVector(0, 0, 1500), Ahead - FVector(0, 0, 4000), ECC_WorldStatic, Q) || Cast<APawn>(Hit.GetActor()))
	{
		return TEXT("Open ground needed");
	}
	if (Hit.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(WRPortals::Num(TEXT("slots.maxSlope"), 25.f)))) return TEXT("Too steep here: find flat ground");
	if (AWRClaimTotem::FindAt(Pawn->GetWorld(), Hit.ImpactPoint)) return TEXT("Not inside a claim");
	if (FWRSocialRules::IsInSafeZone(Pawn->GetWorld(), Hit.ImpactPoint)) return TEXT("Not in a safe zone");
	for (TActorIterator<AWRPortal> It(Pawn->GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Hit.ImpactPoint) < 2000.f) return TEXT("Too close to another portal");
	}
	if (PS->GetInventory()->RemoveItem(ItemId, 1) < 1) return TEXT("You need the key");
	const float Yaw = (Pawn->GetActorLocation() - Hit.ImpactPoint).Rotation().Yaw;
	AWRPortal* P = Sys->OpenPortal(Biome, Rank, Hit.ImpactPoint, Yaw, -1, PS->GetPlayerName());
	if (!P) return TEXT("The key wouldn't turn");
	P->OpenAt = PServerNow(Pawn->GetWorld()) + WRPortals::Num(TEXT("entry.keyOpenSeconds"), 3.f);
	WRSound::PlayAt(Pawn, TEXT("portal.open"), Hit.ImpactPoint);
	return FString::Printf(TEXT("The key turns in the air: a %s fold opens"), *RankName(Rank));
}

// ================================================================================================ the system

bool UWRPortalSystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UWRPortalSystem* UWRPortalSystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWRPortalSystem>() : nullptr;
}

TStatId UWRPortalSystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UWRPortalSystem, STATGROUP_Tickables); }

double UWRPortalSystem::UtcNow() { return double(FDateTime::UtcNow().ToUnixTimestamp()); }

void UWRPortalSystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	PsCached().Reset();
	PsKits().Reset();
	bServer = InWorld.GetNetMode() != NM_Client && InWorld.GetAuthGameMode<AWRGameMode>() != nullptr && !WRWorlds::Current().IsEmpty();
	if (bServer) Load();
}

void UWRPortalSystem::Deinitialize()
{
	if (bServer && bLoaded) Save();
	Super::Deinitialize();
}

void UWRPortalSystem::Load()
{
	FString Text;
	if (!WRStore::ReadDoc(TEXT("portals"), Text)) return;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()) Loaded = Root;
}

void UWRPortalSystem::Restore()
{
	bLoaded = true;
	if (!Loaded.IsValid()) return;
	NextId = FMath::Max(1, int32(PNum(Loaded, TEXT("nextId"), 1.f)));
	if (const TSharedPtr<FJsonObject> S = PChild(Loaded, TEXT("slots")))
	{
		for (const auto& KV : S->Values)
		{
			TArray<double>& Arr = SlotRefill.FindOrAdd(FName(*KV.Key));
			for (const TSharedPtr<FJsonValue>& V : KV.Value->AsArray()) Arr.Add(V->AsNumber());
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* List = PArr(Loaded, TEXT("portals")))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const FVector At(PNum(O, TEXT("x"), 0.f), PNum(O, TEXT("y"), 0.f), PNum(O, TEXT("z"), 0.f));
			TArray<FName> Mods;
			if (const TArray<TSharedPtr<FJsonValue>>* M = PArr(O, TEXT("mods"))) for (const auto& MV : *M) Mods.Add(FName(MV->AsString()));
			AWRPortal* P = OpenPortal(FName(PStr(O, TEXT("biome"))), int32(PNum(O, TEXT("rank"), 1.f)), At, PNum(O, TEXT("yaw"), 0.f), int32(PNum(O, TEXT("slot"), -1.f)), PStr(O, TEXT("owner")), &Mods);
			if (!P) continue;
			P->PortalId = int32(PNum(O, TEXT("id"), float(P->PortalId)));
			P->OpenedUtc = PNum(O, TEXT("opened"), float(UtcNow()));
			double BreakAt = 0.0;
			O->TryGetNumberField(TEXT("breakAt"), BreakAt);
			double Opened = 0.0;
			if (O->TryGetNumberField(TEXT("opened"), Opened)) P->OpenedUtc = Opened;
			P->BreakAtUtc = P->bKey ? 0.0 : BreakAt;
			P->Stage = uint8(PNum(O, TEXT("stage"), 0.f));
			P->bWardenOut = false;   // a warden doesn't outlive the server: spilling brings it again
			if (const TArray<TSharedPtr<FJsonValue>>* Party = PArr(O, TEXT("party"))) for (const auto& N : *Party) P->KeyParty.Add(N->AsString());
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* List = PArr(Loaded, TEXT("escaped")))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FEscaped E;
			E.Biome = FName(PStr(O, TEXT("biome")));
			E.Rank = int32(PNum(O, TEXT("rank"), 1.f));
			E.BossId = FName(PStr(O, TEXT("boss")));
			E.At = FVector(PNum(O, TEXT("x"), 0.f), PNum(O, TEXT("y"), 0.f), PNum(O, TEXT("z"), 0.f));
			Escaped.Add(E);
		}
	}
	if (const TSharedPtr<FJsonObject> In = PChild(Loaded, TEXT("inside")))
	{
		for (const auto& KV : In->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			const FVector Spot(double(PNum(O, TEXT("x"), 0.f)), double(PNum(O, TEXT("y"), 0.f)), double(PNum(O, TEXT("z"), 0.f)));
			InsideAtStop.Emplace(FString(KV.Key), Spot);
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("PORTALS restored %d portals, %d escaped bosses"), Portals.Num(), Escaped.Num());
}

void UWRPortalSystem::Save()
{
	if (!bServer || WRWorlds::CurrentDir().IsEmpty()) return;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("nextId"), NextId);
	const TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	for (const auto& KV : SlotRefill)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const double D : KV.Value) Arr.Add(MakeShared<FJsonValueNumber>(D));
		S->SetArrayField(KV.Key.ToString(), Arr);
	}
	Root->SetObjectField(TEXT("slots"), S);
	TArray<TSharedPtr<FJsonValue>> List;
	const TSharedRef<FJsonObject> Inside = MakeShared<FJsonObject>();
	for (const TWeakObjectPtr<AWRPortal>& W : Portals)
	{
		const AWRPortal* P = W.Get();
		if (!P || P->IsActorBeingDestroyed() || P->bClosing) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		const FVector L = P->GetActorLocation();
		O->SetNumberField(TEXT("id"), P->PortalId);
		O->SetStringField(TEXT("biome"), P->Biome.ToString());
		O->SetNumberField(TEXT("rank"), P->Rank);
		O->SetNumberField(TEXT("x"), L.X); O->SetNumberField(TEXT("y"), L.Y); O->SetNumberField(TEXT("z"), L.Z);
		O->SetNumberField(TEXT("yaw"), P->GetActorRotation().Yaw);
		O->SetNumberField(TEXT("slot"), P->Slot);
		O->SetNumberField(TEXT("opened"), P->OpenedUtc);
		O->SetNumberField(TEXT("breakAt"), P->BreakAtUtc);
		O->SetNumberField(TEXT("stage"), P->Stage);
		O->SetBoolField(TEXT("warden"), P->bWardenOut);
		O->SetStringField(TEXT("owner"), P->bKey ? P->OwnerName : FString());
		TArray<TSharedPtr<FJsonValue>> Mods, Party;
		for (const FName& M : P->Modifiers) Mods.Add(MakeShared<FJsonValueString>(M.ToString()));
		for (const FString& N : P->KeyParty) Party.Add(MakeShared<FJsonValueString>(N));
		O->SetArrayField(TEXT("mods"), Mods);
		O->SetArrayField(TEXT("party"), Party);
		List.Add(MakeShared<FJsonValueObject>(O));
		// Whoever is inside now comes back out at this portal if the server stops.
		if (const AWRFold* F = P->Fold)
		{
			const FVector Front = P->Front();
			for (const FString& N : F->Roster)
			{
				const AWRPlayerState* PS = PsStateOf(N, GetWorld());
				const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
				if (!Pawn || F->Bounds.IsInsideOrOn(Pawn->GetActorLocation()))
				{
					const TSharedRef<FJsonObject> At = MakeShared<FJsonObject>();
					At->SetNumberField(TEXT("x"), Front.X); At->SetNumberField(TEXT("y"), Front.Y); At->SetNumberField(TEXT("z"), Front.Z);
					Inside->SetObjectField(N, At);
				}
			}
		}
	}
	for (const auto& KV : InsideAtStop)
	{
		if (Inside->HasField(KV.Key)) continue;
		const TSharedRef<FJsonObject> At = MakeShared<FJsonObject>();
		At->SetNumberField(TEXT("x"), KV.Value.X); At->SetNumberField(TEXT("y"), KV.Value.Y); At->SetNumberField(TEXT("z"), KV.Value.Z);
		Inside->SetObjectField(KV.Key, At);
	}
	Root->SetArrayField(TEXT("portals"), List);
	Root->SetObjectField(TEXT("inside"), Inside);
	TArray<TSharedPtr<FJsonValue>> Esc;
	for (const FEscaped& E : Escaped)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		const FVector L = E.Body.IsValid() ? E.Body->GetActorLocation() : E.At;
		O->SetStringField(TEXT("biome"), E.Biome.ToString());
		O->SetNumberField(TEXT("rank"), E.Rank);
		O->SetStringField(TEXT("boss"), E.BossId.ToString());
		O->SetNumberField(TEXT("x"), E.At.X); O->SetNumberField(TEXT("y"), E.At.Y); O->SetNumberField(TEXT("z"), L.Z);
		Esc.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("escaped"), Esc);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	WRStore::WriteDoc(TEXT("portals"), Text);
}

void UWRPortalSystem::Tick(float DeltaTime)
{
	if (!bServer) return;
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay() || !AWRGameState::Get(World)) return;
	if (!bLoaded) Restore();
	StepClock += DeltaTime;
	if (StepClock >= 1.f)
	{
		const float Dt = StepClock;
		StepClock = 0.f;
		Portals.RemoveAll([](const TWeakObjectPtr<AWRPortal>& W) { return !W.IsValid(); });
		for (const TWeakObjectPtr<AWRPortal>& W : TArray<TWeakObjectPtr<AWRPortal>>(Portals)) if (AWRPortal* P = W.Get()) StepPortal(P, Dt);
		StepInvites();
		WRArtifacts::TickBands(World);
		StepEscaped();
		StepRescue();
		PlaceClock -= Dt;
		if (PlaceClock <= 0.f)
		{
			PlaceClock = 5.f;
			StepSlots();
		}
	}
	StepRuns(DeltaTime);
	SaveClock += DeltaTime;
	if (SaveClock >= 60.f)
	{
		SaveClock = 0.f;
		Save();
	}
}

// ------------------------------------------------------------------------------------------------ placement

bool UWRPortalSystem::SpotOk(FName Biome, const FVector& At, int32 Rank, FString* Why) const
{
	UWorld* World = GetWorld();
	const float Keep = WRPortals::Num(TEXT("slots.keepFrom"), 15000.f);
	const auto Fail = [Why](const TCHAR* S) { if (Why) *Why = S; return false; };
	// The ridge between the lands is where the passes run: never a portal on it.
	// (The island's ridge only: south of the island's edge the same x is the forest's floor, no pass runs there.)
	if (At.Y > WRTune(TEXT("world.islandNorthEdge"), -201600.f) && At.Y < WRTune(TEXT("world.islandSouthEdge"), 201600.f) && FMath::Abs(At.X - WRTune(TEXT("sky.biomeCentre"), 0.f)) < 20000.f) return Fail(TEXT("pass"));
	for (TActorIterator<AWRClaimTotem> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < Keep + It->Radius) return Fail(TEXT("claim"));
	for (TActorIterator<AWRShrine> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < Keep) return Fail(TEXT("shrine"));
	for (TActorIterator<AWRVillage> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < Keep + It->Ring) return Fail(TEXT("village"));
	for (TActorIterator<AWRBoss> It(World); It; ++It) if (FVector::Dist2D(It->GetHome(), At) < Keep + It->GetChaseRadius() * 0.5f) return Fail(TEXT("arena"));
	for (TActorIterator<AWRSafeZone> It(World); It; ++It) if (FVector::Dist2D(It->GetActorLocation(), At) < Keep + It->Radius) return Fail(TEXT("camp"));
	for (const TWeakObjectPtr<AWRPortal>& W : Portals) if (W.IsValid() && FVector::Dist2D(W->GetActorLocation(), At) < WRPortals::Num(TEXT("slots.fromPortals"), 15000.f)) return Fail(TEXT("portal"));
	for (const FEscaped& E : Escaped) if (FVector::Dist2D(E.At, At) < WRPortals::Num(TEXT("escalation.escapedRoam"), 40000.f)) return Fail(TEXT("escaped"));
	// Not in someone's face when it opens.
	bool bSeen = false;
	PsForPlayers(World, [&](AWRPlayerController* PC) { if (const APawn* P = PC->GetPawn()) bSeen |= FVector::Dist2D(P->GetActorLocation(), At) < 12000.f; });
	if (bSeen) return Fail(TEXT("player"));
	return true;
}

bool UWRPortalSystem::FindSpot(FName Biome, int32 Rank, FVector& OutAt, float& OutYaw) const
{
	UWorld* World = GetWorld();
	const TArray<TSharedPtr<FJsonValue>>* Bands = PArr(WRPortals::Rank(Rank), TEXT("bands"));
	if (!Bands || Bands->Num() == 0) return false;
	const int32 Tries = FMath::RoundToInt(WRPortals::Num(TEXT("slots.tries"), 40.f));
	const float Slope = FMath::Cos(FMath::DegreesToRadians(WRPortals::Num(TEXT("slots.maxSlope"), 25.f)));
	for (int32 t = 0; t < Tries; ++t)
	{
		const FName Band((*Bands)[FMath::RandHelper(Bands->Num())]->AsString());
		const FName As = PsStandAs(Biome, Band);
		if (As.IsNone()) return false;
		// A point on the island on this side (the walk map's cells are 1 km wide at most: sample anywhere and ask it).
		const float Side = Biome == TEXT("volcanic") ? -1.f : 1.f;
		// Storm peaks: anywhere in its landscape's frame (north of the island); the island's lands: their side of it.
		// The Swamp: anywhere in its landscape's frame (Data/swamp.json), kept to its own ground by WRBiome::At below.
		const FWRSwampData& SwampD = FWRSwampData::Get();
		// The Ancient forest: anywhere in its frame (Data/forest.json), its own ground by WRBiome::At below.
		const FWRForestData& ForestD = FWRForestData::Get();
		const FVector2D P = Biome == TEXT("swamp") ? FVector2D(FMath::FRandRange(SwampD.FrameMin.X, SwampD.FrameMax.X), FMath::FRandRange(SwampD.FrameMin.Y, SwampD.FrameMax.Y))
			: Biome == TEXT("forest") ? FVector2D(FMath::FRandRange(ForestD.FrameMin.X, ForestD.FrameMax.X), FMath::FRandRange(ForestD.FrameMin.Y, ForestD.FrameMax.Y))
			: Biome == TEXT("storm_peaks") ? FVector2D(FMath::FRandRange(-290000.f, 100000.f), FMath::FRandRange(-580000.f, -205000.f))
			: FVector2D(Side * FMath::FRandRange(10000.f, 260000.f), FMath::FRandRange(-200000.f, 200000.f));
		if (Biome == TEXT("swamp") && (!SwampD.bLoaded || SwampD.BaseWaterAt(FVector(P, 0.f)) != EWRSwampWater::Dry)) continue;
		if (Biome == TEXT("forest") && !WRForest::Contains(FVector(P, 0.f))) continue;
		if (!WRWildlife::CanStand(As, P)) continue;
		const FVector Probe(P, 0.f);
		if (WRBiome::At(World, Probe) != Biome || !SpotOk(Biome, Probe, Rank, nullptr)) continue;
		// The ground has to be here (streamed) and flat enough; if it isn't loaded yet, try another.
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRPortalSpot), false);
		if (!World->LineTraceSingleByChannel(Hit, FVector(P, 200000.f), FVector(P, -20000.f), ECC_WorldStatic, Q) || Cast<APawn>(Hit.GetActor())) continue;
		if ((Biome != TEXT("swamp") && Hit.ImpactPoint.Z < WRTune(TEXT("sea.level"), -20.f) + 200.f) || Hit.ImpactNormal.Z < Slope) continue;
		// The swamp's dry ground stands only a metre or two over its water: clear the drawn surface instead.
		if (Biome == TEXT("swamp") && Hit.ImpactPoint.Z < SwampD.SeaZ + WRTune(TEXT("swampWorld.portalDryLift"), 50.f)) continue;
		OutAt = Hit.ImpactPoint;
		OutYaw = FMath::FRandRange(0.f, 360.f);
		return true;
	}
	return false;
}

FString UWRPortalSystem::RankBlocked(FName Biome, int32 Rank) const
{
	for (const FEscaped& E : Escaped)
	{
		if (E.Biome != Biome || E.Rank != Rank) continue;
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(E.BossId);
		return FString::Printf(TEXT("The %s fold of the %s is held shut: %s roams free"), *WRPortals::RankName(Rank), *WRPortals::BiomeName(Biome), Def ? *Def->Name : TEXT("its boss"));
	}
	return FString();
}

void UWRPortalSystem::StepSlots()
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Biomes = WRPortals::Obj(TEXT("biomes"));
	if (!Biomes.IsValid()) return;
	const double Now = UtcNow();
	for (const auto& KV : Biomes->Values)
	{
		const TSharedPtr<FJsonObject> B = KV.Value->AsObject();
		if (!PBool(B, TEXT("land")) || !WRPortals::Kit(FName(*KV.Key)).IsValid()) continue;   // its land isn't in the world yet
		const FName Biome(*KV.Key);
		const TArray<TSharedPtr<FJsonValue>>* Slots = PArr(B, TEXT("slots"));
		if (!Slots) continue;
		TArray<double>& Refill = SlotRefill.FindOrAdd(Biome);
		if (Refill.Num() < Slots->Num()) Refill.SetNumZeroed(Slots->Num());
		for (int32 s = 0; s < Slots->Num(); ++s)
		{
			const bool bOpen = Portals.ContainsByPredicate([&](const TWeakObjectPtr<AWRPortal>& W) { return W.IsValid() && W->Biome == Biome && W->Slot == s && !W->bKey; });
			if (bOpen) { Refill[s] = 0.0; continue; }
			if (Refill[s] > Now) continue;
			const FString Kind = (*Slots)[s]->AsString();
			int32 Rank = Kind == TEXT("high") ? (FMath::FRand() < WRPortals::Num(TEXT("slots.highLegendary"), 0.05f) ? 5 : 4) : FMath::Clamp(FCString::Atoi(*Kind), 1, 5);
			if (!RankBlocked(Biome, Rank).IsEmpty())
			{
				if (Kind == TEXT("high") && RankBlocked(Biome, Rank == 5 ? 4 : 5).IsEmpty()) Rank = Rank == 5 ? 4 : 5;
				else continue;
			}
			FVector At;
			float Yaw = 0.f;
			if (!FindSpot(Biome, Rank, At, Yaw)) continue;
			OpenPortal(Biome, Rank, At, Yaw, s, FString());
		}
	}
	(void)World;
}

AWRPortal* UWRPortalSystem::OpenPortal(FName Biome, int32 Rank, const FVector& At, float Yaw, int32 Slot, const FString& KeyOwner, const TArray<FName>* Mods)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRPortal* P = World->SpawnActor<AWRPortal>(AWRPortal::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), At), SP);
	if (!P) return nullptr;
	P->PortalId = NextId++;
	P->Biome = Biome;
	P->Rank = uint8(FMath::Clamp(Rank, 1, 5));
	P->Slot = Slot;
	P->bKey = !KeyOwner.IsEmpty();
	P->OwnerName = KeyOwner;
	P->OpenedUtc = UtcNow();
	// The modifiers are rolled when it opens and shown on its card before anyone goes in.
	if (Mods)
	{
		P->Modifiers = *Mods;
	}
	else
	{
		const FVector2D Count = PRange(WRPortals::Rank(Rank), TEXT("modifiers"), FVector2D(0, 0));
		const int32 N = FMath::RandRange(int32(Count.X), int32(Count.Y));
		TArray<FName> All;
		if (const TArray<TSharedPtr<FJsonValue>>* M = PArr(WRPortals::Data(), TEXT("modifiers"))) for (const auto& V : *M) All.Add(FName(PStr(V->AsObject(), TEXT("id"))));
		for (int32 i = 0; i < N && All.Num() > 0; ++i) P->Modifiers.Add(All[FMath::RandHelper(All.Num())]), All.Remove(P->Modifiers.Last());
	}
	// Its clock: the rank's real days, times the server's break setting (never: no clock). Key portals never break.
	const FString Setting = FWRSocialRules::Setting(TEXT("portalBreak"), TEXT("normal")).ToLower();
	if (!P->bKey && Setting != TEXT("never"))
	{
		const float Scale = WRPortals::Num(FString::Printf(TEXT("escalation.breakScale.%s"), *Setting), 1.f);
		P->BreakAtUtc = P->OpenedUtc + PNum(WRPortals::Rank(Rank), TEXT("breakDays"), 1.f) * 86400.0 * Scale;
	}
	if (P->bKey)
	{
		// The opener's party now: their band (their clan is read live).
		if (const AWRPlayerState* PS = PsStateOf(KeyOwner, World)) P->KeyParty = WRArtifacts::BandOf(PS);
		P->KeyParty.AddUnique(KeyOwner);
	}
	Portals.Add(P);
	UE_LOG(LogWildreach, Log, TEXT("PORTAL %d opened: %s %s at %s%s, %d modifiers"), P->PortalId, *WRPortals::RankName(Rank), *Biome.ToString(), *At.ToCompactString(), P->bKey ? *FString::Printf(TEXT(" (key of %s)"), *KeyOwner) : TEXT(""), P->Modifiers.Num());
	// A new Legendary is told to the whole server (not one coming back with the world).
	if (Rank == 5 && !Mods)
	{
		Announce(FString::Printf(TEXT("A Legendary portal has opened in the %s"), *WRPortals::BiomeName(Biome)), TEXT("portal.legendary"));
	}
	return P;
}

void UWRPortalSystem::Announce(const FString& Line, FName Sound)
{
	PsForPlayers(GetWorld(), [&](AWRPlayerController* PC)
	{
		PC->ClientNotify(Line);
		PC->ClientChat(TEXT("The land"), Line, 4);
		if (!Sound.IsNone()) WRSound::PlayUI(PC, Sound);
	});
}

AWRPortal* UWRPortalSystem::FindPortal(int32 Id) const
{
	for (const TWeakObjectPtr<AWRPortal>& W : Portals) if (W.IsValid() && W->PortalId == Id) return W.Get();
	return nullptr;
}

int32 UWRPortalSystem::RunsOpen() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<AWRPortal>& W : Portals) if (W.IsValid() && W->bRunOpen) ++N;
	return N;
}

// ------------------------------------------------------------------------------------------------ breaking

void UWRPortalSystem::StepPortal(AWRPortal* P, float Dt)
{
	UWorld* World = GetWorld();
	if (P->bClosing) return;
	// A key portal admits its opener, their clan and their band as they are now (sent to every machine for the card).
	if (P->bKey)
	{
		TArray<FString> A = P->KeyParty;
		for (const FString& N : WRArtifacts::BandOfName(World, P->OwnerName)) A.AddUnique(N);
		PsForPlayers(World, [&](AWRPlayerController* PC) { if (PC->PlayerState && FWRClans::Get().SameClan(P->OwnerName, PC->PlayerState->GetPlayerName())) A.AddUnique(PC->PlayerState->GetPlayerName()); });
		A.Sort();
		if (A != P->Allowed) P->Allowed = A;
	}
	// A run inside holds the clock (clearing it stops the break; an ignored one keeps going).
	if (P->bRunOpen && P->BreakAtUtc > 0.0) P->BreakAtUtc += Dt;
	if (P->BreakAtUtc <= 0.0) return;
	const double Now = UtcNow();
	const double Span = FMath::Max(1.0, P->BreakAtUtc - P->OpenedUtc);
	const double Share = (Now - P->OpenedUtc) / Span;
	const uint8 Want = Share >= WRPortals::Num(TEXT("escalation.spillAt"), 0.8f) ? 2 : Share >= WRPortals::Num(TEXT("escalation.crackAt"), 0.5f) ? 1 : 0;
	if (Want != P->Stage)
	{
		P->Stage = Want;
		WRSound::PlayAt(P, Want == 2 ? TEXT("portal.spill") : TEXT("portal.crack"), P->GetActorLocation());
		UE_LOG(LogWildreach, Log, TEXT("PORTAL %d %s"), P->PortalId, Want == 2 ? TEXT("spilling") : TEXT("cracking"));
	}
	if (Share >= 1.0 && !P->bRunOpen)
	{
		Break(P);
		return;
	}
	if (P->Stage == 0 || P->bRunOpen) return;
	// Spills only near someone (out of everyone's reach nothing happens; the clock still runs).
	bool bNear = false;
	const float Near = WRPortals::Num(TEXT("escalation.spillNear"), 30000.f);
	PsForPlayers(World, [&](AWRPlayerController* PC) { if (const APawn* Pw = PC->GetPawn()) bNear |= FVector::Dist2D(Pw->GetActorLocation(), P->GetActorLocation()) < Near; });
	if (!bNear) return;
	const double Hours = PsGameHours(World);
	if (P->Stage == 2 && !P->bWardenOut)
	{
		SpillPack(P, true);
		P->bWardenOut = true;
	}
	if (P->NextSpillHours < 0.0 || Hours >= P->NextSpillHours)
	{
		if (P->NextSpillHours >= 0.0) SpillPack(P, false);
		const float Minutes = P->Stage == 2 ? WRPortals::Num(TEXT("escalation.spillPackMinutes"), 15.f) : WRPortals::Num(TEXT("escalation.crackPackMinutes"), 30.f);
		P->NextSpillHours = Hours + Minutes / 60.0;
	}
}

void UWRPortalSystem::SpillPack(AWRPortal* P, bool bWarden)
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Kit = WRPortals::Kit(P->Biome);
	const FVector Mouth = P->Front();
	if (bWarden)
	{
		const FName Id(PStr(PChild(WRPortals::Obj(TEXT("biomes")), P->Biome.ToString()), TEXT("warden")));
		if (AWRCreature* W = PsSpawnScaled(World, Id, Mouth, P->GetActorRotation().Yaw, 1.f, 1.f, 1.f, WRPortals::RankColour(P->Rank)))
		{
			W->SetHome(Mouth);
			P->Warden = W;
			WRSound::PlayAt(P, TEXT("portal.warden"), Mouth);
			WRFx::Broadcast(P, TEXT("summon.flare"), P->GetActorLocation() + FVector(0, 0, 150.f));
		}
		return;
	}
	// One pack of the fold's creatures steps out of the tear and roams (no more than maxSpilled of them about at once).
	P->Spilled.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
	if (P->Spilled.Num() >= int32(WRPortals::Num(TEXT("escalation.maxSpilled"), 14.f))) return;
	const TArray<TSharedPtr<FJsonValue>>* Table = PArr(PChild(Kit, TEXT("creatures")), FString::FromInt(P->Rank));
	const FVector2D Pack = PRange(WRPortals::Rank(P->Rank), TEXT("packs"), FVector2D(1, 3));
	const FName Kind = PPick(Table);
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
	if (!Def) return;
	const int32 N = Kind == TEXT("anvil_mite") ? 5 : FMath::RandRange(int32(Pack.X), int32(Pack.Y));
	float H = 1.f, D = 1.f, S = 1.f;
	PsScaleFor(P->Rank, Def->Tier, H, D, S);
	const FVector Roam = PsRandIn2D(P->GetActorLocation(), WRPortals::Num(TEXT("escalation.packRadius"), 15000.f));
	for (int32 i = 0; i < N; ++i)
	{
		if (AWRCreature* C = PsSpawnScaled(World, Kind, Mouth + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f), P->GetActorRotation().Yaw, H, D, S, FColor(0, 0, 0, 0)))
		{
			C->SetHome(Roam);
			P->Spilled.Add(C);
		}
	}
	WRFx::Broadcast(P, TEXT("summon.flare"), P->GetActorLocation() + FVector(0, 0, 150.f), FRotator::ZeroRotator, 0.7f);
	UE_LOG(LogWildreach, Log, TEXT("PORTAL %d spills %d %s"), P->PortalId, N, *Kind.ToString());
}

void UWRPortalSystem::Break(AWRPortal* P)
{
	UWorld* World = GetWorld();
	const TArray<TSharedPtr<FJsonValue>>* Bosses = PArr(PChild(WRPortals::Obj(TEXT("biomes")), P->Biome.ToString()), TEXT("bosses"));
	const FName BossId = Bosses && Bosses->IsValidIndex(P->Rank - 1) ? FName((*Bosses)[P->Rank - 1]->AsString()) : NAME_None;
	FEscaped E;
	E.Biome = P->Biome;
	E.Rank = P->Rank;
	E.BossId = BossId;
	E.At = P->GetActorLocation();
	// The boss walks out of the tear; the portal closes behind it.
	if (AWRCreature* B = PsSpawnScaled(World, BossId, P->Front(), P->GetActorRotation().Yaw, 1.f, 1.f, 1.f, WRPortals::RankColour(P->Rank)))
	{
		UWRPortalBossBrain* Brain = NewObject<UWRPortalBossBrain>(B);
		Brain->bEscaped = true;
		Brain->Home = E.At;
		Brain->RegisterComponent();
		E.Body = B;
	}
	Escaped.Add(E);
	WRSound::PlayAt(P, TEXT("portal.break"), P->GetActorLocation());
	WRFx::Broadcast(P, TEXT("roar.blast"), P->GetActorLocation() + FVector(0, 0, 150.f), FRotator::ZeroRotator, 2.f);
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(BossId);
	// Those in its land hear it.
	PsForPlayers(World, [&](AWRPlayerController* PC)
	{
		const APawn* Pw = PC->GetPawn();
		if (Pw && WRBiome::At(Pw, Pw->GetActorLocation()) == P->Biome)
			PC->ClientNotify(FString::Printf(TEXT("A %s portal broke: %s walks the %s"), *WRPortals::RankName(P->Rank), Def ? *Def->Name : TEXT("its boss"), *WRPortals::BiomeName(P->Biome)));
	});
	UE_LOG(LogWildreach, Log, TEXT("PORTAL %d broke: %s escaped"), P->PortalId, *BossId.ToString());
	ClosePortal(P, false);
}

void UWRPortalSystem::EscapedDied(AWRCreature* Boss)
{
	const int32 i = Escaped.IndexOfByPredicate([Boss](const FEscaped& E) { return E.Body.Get() == Boss; });
	if (i == INDEX_NONE) return;
	const FEscaped E = Escaped[i];
	Escaped.RemoveAt(i);
	// Its roll and its shard to everyone who did their 10%.
	if (AWRBoss* B = Cast<AWRBoss>(Boss))
	{
		const float Need = B->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
		const TArray<TSharedPtr<FJsonValue>>* Rewards = PArr(WRPortals::Data(), TEXT("rewards"));
		const TSharedPtr<FJsonObject> Row = Rewards && Rewards->IsValidIndex(E.Rank - 1) ? (*Rewards)[E.Rank - 1]->AsObject() : nullptr;
		for (const TPair<FString, float>& L : B->Ledger)
		{
			if (L.Value < Need) continue;
			AWRPlayerState* PS = PsStateOf(L.Key, GetWorld());
			if (!PS) continue;
			TMap<FName, int32> Items;
			PsResolve(PChild(Row, TEXT("boss")), E.Biome, WRArtifacts::LootScale(PS), Items);
			Items.FindOrAdd(FName(PStr(WRPortals::Rank(E.Rank), TEXT("shard")))) += 1;
			const FString Got = PsGive(PS, Items);
			if (AWRPlayerController* PC = PsControllerOf(PS)) PC->ClientNotify(FString::Printf(TEXT("The escaped boss is down: %s"), *Got));
			WRDeeds::OnEscapedSlain(PS);
		}
	}
	PsForPlayers(GetWorld(), [&](AWRPlayerController* PC)
	{
		const APawn* Pw = PC->GetPawn();
		if (Pw && WRBiome::At(Pw, Pw->GetActorLocation()) == E.Biome) PC->ClientNotify(FString::Printf(TEXT("The %s folds of the %s can open again"), *WRPortals::RankName(E.Rank), *WRPortals::BiomeName(E.Biome)));
	});
	Save();
}

void UWRPortalSystem::StepEscaped()
{
	UWorld* World = GetWorld();
	const float Wake = WRTune(TEXT("wildlife.activationRadius"), 15000.f);
	for (FEscaped& E : Escaped)
	{
		if (E.Body.IsValid()) { if (E.Body->IsAlive()) E.At.Z = E.Body->GetActorLocation().Z; continue; }
		// Out of everyone's reach it isn't simulated; it comes back where it roams when someone comes near.
		bool bNear = false;
		PsForPlayers(World, [&](AWRPlayerController* PC) { if (const APawn* Pw = PC->GetPawn()) bNear |= FVector::Dist2D(Pw->GetActorLocation(), E.At) < Wake; });
		if (!bNear) continue;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRPortalEscaped), false);
		if (!World->LineTraceSingleByChannel(Hit, E.At + FVector(0, 0, 50000), E.At - FVector(0, 0, 50000), ECC_WorldStatic, Q)) continue;
		if (AWRCreature* B = PsSpawnScaled(World, E.BossId, Hit.ImpactPoint, FMath::FRandRange(0.f, 360.f), 1.f, 1.f, 1.f, WRPortals::RankColour(E.Rank)))
		{
			UWRPortalBossBrain* Brain = NewObject<UWRPortalBossBrain>(B);
			Brain->bEscaped = true;
			Brain->Home = E.At;
			Brain->RegisterComponent();
			E.Body = B;
		}
	}
}

// ------------------------------------------------------------------------------------------------ runs

int32 UWRPortalSystem::FreeBay() const
{
	TSet<int32> Used;
	for (const TWeakObjectPtr<AWRFold>& W : AWRFold::All()) if (W.IsValid() && W->GetWorld() == GetWorld()) Used.Add(W->Bay);
	for (int32 b = 0; b < 8; ++b) if (!Used.Contains(b)) return b;
	return INDEX_NONE;
}

AWRFold* UWRPortalSystem::BuildFold(AWRPortal* P)
{
	UWorld* World = GetWorld();
	const TSharedPtr<FJsonObject> Kit = WRPortals::Kit(P->Biome);
	const int32 Bay = FreeBay();
	if (!Kit.IsValid() || Bay == INDEX_NONE) return nullptr;
	// The handmade graph for this rank, and a handmade room for each of its slots (never the same room twice in a run).
	const TArray<TSharedPtr<FJsonValue>>* Graphs = PArr(PChild(Kit, TEXT("layouts")), FString::FromInt(P->Rank));
	const TArray<TSharedPtr<FJsonValue>>* Rooms = PArr(Kit, TEXT("rooms"));
	if (!Graphs || Graphs->Num() == 0 || !Rooms) return nullptr;
	const TSharedPtr<FJsonObject> Graph = (*Graphs)[FMath::RandHelper(Graphs->Num())]->AsObject();
	TMap<FString, TArray<TSharedPtr<FJsonObject>>> Pool;
	for (const TSharedPtr<FJsonValue>& V : *Rooms) Pool.FindOrAdd(PStr(V->AsObject(), TEXT("type"))).Add(V->AsObject());
	for (auto& KV : Pool) for (int32 i = KV.Value.Num() - 1; i > 0; --i) KV.Value.Swap(i, FMath::RandHelper(i + 1));
	TMap<FString, int32> Used;
	const auto Take = [&](const FString& Type) -> TSharedPtr<FJsonObject>
	{
		TArray<TSharedPtr<FJsonObject>>* L = Pool.Find(Type);
		if (!L || L->Num() == 0) return nullptr;
		int32& U = Used.FindOrAdd(Type);
		return (*L)[U++ % L->Num()];
	};
	const FVector Base(WRPortals::Num(TEXT("bay.x"), 1500000.f), Bay * WRPortals::Num(TEXT("bay.ySpacing"), 120000.f), WRPortals::Num(TEXT("bay.z"), 200000.f));
	const float Corridor = WRPortals::Num(TEXT("bay.corridor"), 800.f);
	const float Gap = WRPortals::Num(TEXT("bay.branchGap"), 800.f);
	TArray<FWRFoldRoomRep> Out;
	TArray<FBox2D> Boxes;
	float X = 0.f;
	const TArray<TSharedPtr<FJsonValue>>* Main = PArr(Graph, TEXT("main"));
	if (!Main) return nullptr;
	for (int32 i = 0; i < Main->Num(); ++i)
	{
		const FString Type = (*Main)[i]->AsString();
		TSharedPtr<FJsonObject> Row;
		if (Type == TEXT("boss"))
		{
			const TArray<TSharedPtr<FJsonValue>>* BossRooms = PArr(Kit, TEXT("bosses"));
			const FString Id = BossRooms && BossRooms->IsValidIndex(P->Rank - 1) ? (*BossRooms)[P->Rank - 1]->AsString() : FString();
			for (const TSharedPtr<FJsonObject>& R : Pool.FindOrAdd(TEXT("boss"))) if (PStr(R, TEXT("id")) == Id) Row = R;
		}
		else Row = Take(Type);
		if (!Row.IsValid()) return nullptr;
		FWRFoldRoomRep R;
		R.RoomId = FName(PStr(Row, TEXT("id")));
		R.Type = FName(*Type);
		R.W = PNum(Row, TEXT("w"), 16.f) * 100.f;
		R.D = PNum(Row, TEXT("d"), 16.f) * 100.f;
		R.H = PNum(Row, TEXT("h"), 7.f) * 100.f;
		R.Origin = Base + FVector(X, 0.f, 0.f);
		R.Side = 0;
		Boxes.Add(FBox2D(FVector2D(X, -R.D * 0.5f), FVector2D(X + R.W, R.D * 0.5f)));
		Out.Add(R);
		X += R.W + Corridor;
	}
	// Side branches hang off their host's north wall (south if the north is taken or would overlap).
	if (const TArray<TSharedPtr<FJsonValue>>* Branches = PArr(Graph, TEXT("branch")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Branches)
		{
			const int32 HostIdx = int32(PAt(V, 0, 1.f));
			const FString Type = PAtStr(V, 1);
			if (!Out.IsValidIndex(HostIdx)) continue;
			const TSharedPtr<FJsonObject> Row = Take(Type);
			if (!Row.IsValid()) continue;
			FWRFoldRoomRep& Host = Out[HostIdx];
			const float W = PNum(Row, TEXT("w"), 12.f) * 100.f, D = PNum(Row, TEXT("d"), 12.f) * 100.f;
			const float HX = Host.Origin.X - Base.X + Host.W * 0.5f;
			bool bPlaced = false;
			for (const uint8 Side : { uint8(1), uint8(2) })
			{
				if (Host.BranchDoors & Side) continue;
				const float Sign = Side == 1 ? -1.f : 1.f;
				const float Near = Host.D * 0.5f + Gap;
				const FBox2D Box = Side == 1 ? FBox2D(FVector2D(HX - W * 0.5f, -Near - D), FVector2D(HX + W * 0.5f, -Near))
					: FBox2D(FVector2D(HX - W * 0.5f, Near), FVector2D(HX + W * 0.5f, Near + D));
				const FBox2D Grown(Box.Min - FVector2D(200.f), Box.Max + FVector2D(200.f));
				if (Boxes.ContainsByPredicate([&Grown](const FBox2D& B) { return B.Intersect(Grown); })) continue;
				FWRFoldRoomRep R;
				R.RoomId = FName(PStr(Row, TEXT("id")));
				R.Type = FName(*Type);
				R.W = W; R.D = D; R.H = PNum(Row, TEXT("h"), 6.f) * 100.f;
				R.Side = Side;
				R.Host = uint8(HostIdx);
				// A branch room's frame runs along its door axis: its "west" door faces the host.
				R.Origin = Base + FVector(HX, Sign * Near, 0.f);
				Host.BranchDoors |= Side;
				Boxes.Add(Box);
				Out.Add(R);
				bPlaced = true;
				break;
			}
			if (!bPlaced) UE_LOG(LogWildreach, Log, TEXT("PORTAL fold: branch %s off room %d had no room, left out"), *Type, HostIdx);
		}
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRFold* F = World->SpawnActorDeferred<AWRFold>(AWRFold::StaticClass(), FTransform(Base), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!F) return nullptr;
	F->Bay = uint8(Bay);
	F->Biome = P->Biome;
	F->Rank = P->Rank;
	F->Modifiers = P->Modifiers;
	F->Rooms = Out;
	F->Portal = P;
	F->LastOccupiedUtc = UtcNow();
	F->FinishSpawning(FTransform(Base));
	P->Fold = F;
	P->bRunOpen = true;
	PopulateThings(F);
	UE_LOG(LogWildreach, Log, TEXT("PORTAL %d fold built in bay %d: %d rooms"), P->PortalId, Bay, Out.Num());
	return F;
}

FString UWRPortalSystem::Enter(AWRPlayerController* PC, AWRPortal* Portal)
{
	if (!Portal) return TEXT("That portal is gone");
	const FString Why = WRPortals::EnterBlock(PC, Portal);
	if (!Why.IsEmpty()) return Why;
	const bool bFirst = !Portal->bRunOpen;
	if (bFirst && !BuildFold(Portal)) return TEXT("All folds are full: one frees when a run ends");
	const FString Me = PC->PlayerState->GetPlayerName();
	// Everyone near who may come is asked (A / click to come); they have acceptSeconds.
	FInvite Inv;
	Inv.PortalId = Portal->PortalId;
	Inv.From = Me;
	Inv.Until = GetWorld()->GetTimeSeconds() + WRPortals::Num(TEXT("entry.acceptSeconds"), 5.f);
	const float R = WRPortals::Num(TEXT("entry.partyRadius"), 1000.f);
	const APawn* MyPawn = PC->GetPawn();
	PsForPlayers(GetWorld(), [&](AWRPlayerController* Other)
	{
		const APawn* Op = Other->GetPawn();
		if (Other == PC || !Op || !Other->PlayerState || FVector::Dist(Op->GetActorLocation(), MyPawn->GetActorLocation()) > R) return;
		if (!WRPortals::EnterBlock(Other, Portal).IsEmpty() || WRPortals::FoldOf(Op)) return;
		Inv.Asked.Add(Other->PlayerState->GetPlayerName());
		Other->ClientOpenWorldScreen(TEXT("portalinvite"), Portal);
		Other->ClientNotify(FString::Printf(TEXT("%s enters the %s fold: come?"), *Me, *WRPortals::RankName(Portal->Rank)));
	});
	if (Inv.Asked.Num() > 0) Invites.Add(Inv);
	PutInside(PC, Portal, bFirst);
	return FString::Printf(TEXT("Into the %s fold"), *WRPortals::RankName(Portal->Rank));
}

FString UWRPortalSystem::Accept(AWRPlayerController* PC, int32 PortalId, bool bCome)
{
	const FString Me = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	const int32 i = Invites.IndexOfByPredicate([&](const FInvite& I) { return I.PortalId == PortalId && I.Asked.Contains(Me); });
	if (i == INDEX_NONE) return bCome ? TEXT("Too late: walk in yourself") : FString();
	Invites[i].Asked.Remove(Me);
	if (!bCome) return TEXT("You stay");
	AWRPortal* P = FindPortal(PortalId);
	if (!P || !P->bRunOpen) return TEXT("The fold is gone");
	const FString Why = WRPortals::EnterBlock(PC, P);
	if (!Why.IsEmpty()) return Why;
	PutInside(PC, P, false);
	return FString::Printf(TEXT("Into the %s fold with %s"), *WRPortals::RankName(P->Rank), *Invites[i].From);
}

void UWRPortalSystem::StepInvites()
{
	const double Now = GetWorld()->GetTimeSeconds();
	Invites.RemoveAll([Now](const FInvite& I) { return Now > I.Until || I.Asked.Num() == 0; });
}

void UWRPortalSystem::PutInside(AWRPlayerController* PC, AWRPortal* Portal, bool bFirst)
{
	AWRFold* F = Portal ? Portal->Fold.Get() : nullptr;
	AWRCharacter* Me = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
	if (!F || !Me) return;
	const FString Name = PC->PlayerState->GetPlayerName();
	F->Roster.AddUnique(Name);
	InsideAtStop.Remove(Name);
	// Following tames come too, unless the fold says No tames (then they wait at the portal).
	const bool bNoTames = F->Modifiers.Contains(TEXT("no_tames"));
	TArray<AWRCreature*> Tames = bNoTames ? TArray<AWRCreature*>() : PsTamesNear(GetWorld(), Name, Me->GetActorLocation(), 3000.f);
	if (bNoTames)
	{
		for (AWRCreature* T : PsTamesNear(GetWorld(), Name, Me->GetActorLocation(), 3000.f)) T->Order(PC, TEXT("stay"));
	}
	const float Fade = 0.8f;
	PC->ClientSetCameraFade(true, FColor::Black, FVector2D(0.f, 1.f), Fade, true, true);
	WRSound::PlayAt(Portal, TEXT("portal.enter"), Portal->GetActorLocation());
	const TWeakObjectPtr<AWRPlayerController> WeakPC = PC;
	const TWeakObjectPtr<AWRFold> WeakF = F;
	TArray<TWeakObjectPtr<AWRCreature>> WeakTames;
	for (AWRCreature* T : Tames) WeakTames.Add(T);
	FTimerHandle T;
	GetWorld()->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WeakPC, WeakF, WeakTames, Fade]()
	{
		AWRPlayerController* P = WeakPC.Get();
		AWRFold* Fo = WeakF.Get();
		APawn* Pawn = P ? P->GetPawn() : nullptr;
		if (!P || !Fo || !Pawn) return;
		PsPut(Pawn, Fo->EntryPoint(), 0.f);
		int32 n = 0;
		for (const TWeakObjectPtr<AWRCreature>& W : WeakTames)
		{
			if (AWRCreature* C = W.Get()) PsPut(C, Fo->EntryPoint() + FVector(-150.f, (n++ % 2 ? 250.f : -250.f), 0.f), 0.f);
		}
		// Bloodied: you come in at half your health.
		if (Fo->Modifiers.Contains(TEXT("bloodied")))
		{
			if (AWRCharacterBase* C = Cast<AWRCharacterBase>(Pawn); C && C->GetAttributes())
			{
				const float Max = C->GetAttributes()->GetMaxHealth();
				C->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetHealthAttribute(), FMath::Min(C->GetAttributes()->GetHealth(), Max * WRPortals::ModNum(TEXT("bloodied"), TEXT("health"), 0.5f)));
			}
		}
		P->ClientSetCameraFade(true, FColor::Black, FVector2D(1.f, 0.f), Fade, true, false);
		WRSound::PlayOn(Pawn, TEXT("portal.inside"));
	}), Fade + 0.1f, false);
	UE_LOG(LogWildreach, Log, TEXT("PORTAL %d %s enters%s (%d tames)"), Portal->PortalId, *Name, bFirst ? TEXT(" first") : TEXT(""), Tames.Num());
}

FString UWRPortalSystem::Leave(AWRPlayerController* PC)
{
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	AWRFold* F = Me ? WRPortals::FoldOf(Me) : nullptr;
	AWRPortal* P = F ? F->Portal.Get() : nullptr;
	if (!Me || !F || !P) return TEXT("You're not in a fold");
	const FString Name = PC->PlayerState->GetPlayerName();
	for (AWRCreature* T : PsTamesNear(GetWorld(), Name, Me->GetActorLocation(), 4000.f)) PsPut(T, P->Front() + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 0.f), P->GetActorRotation().Yaw);
	PsPut(Me, P->Front(), P->GetActorRotation().Yaw + 180.f);
	WRSound::PlayAt(P, TEXT("portal.enter"), P->GetActorLocation());
	return TEXT("Out of the fold");
}

void UWRPortalSystem::StepRuns(float Dt)
{
	UWorld* World = GetWorld();
	const double Utc = UtcNow();
	const float Now = World->GetTimeSeconds();
	const float WakeR = WRPortals::Num(TEXT("entry.roomWake"), 2500.f);
	for (const TWeakObjectPtr<AWRFold>& W : TArray<TWeakObjectPtr<AWRFold>>(AWRFold::All()))
	{
		AWRFold* F = W.Get();
		if (!F || F->GetWorld() != World || F->IsActorBeingDestroyed()) continue;
		AWRPortal* P = F->Portal.Get();
		if (!P) { TearDownFold(F, FVector::ZeroVector); continue; }
		// Who's inside, and where.
		bool bAnyone = false;
		PsForPlayers(World, [&](AWRPlayerController* PC)
		{
			APawn* Pawn = PC->GetPawn();
			if (!Pawn || !PC->PlayerState || !F->Bounds.IsInsideOrOn(Pawn->GetActorLocation())) return;
			bAnyone = true;
			const FVector L = Pawn->GetActorLocation();
			const int32 Here = F->RoomAt(L, 50.f);
			if (Here != INDEX_NONE) F->SeenBy.FindOrAdd(PC->PlayerState->GetPlayerName()).AddUnique(Here);
			// Rooms wake as someone comes near their door (their creatures are there waiting, in the room's far half).
			for (int32 r = 0; r < F->Rooms.Num(); ++r)
			{
				if (F->Woken.Contains(r)) continue;
				const FVector Door = F->RoomPoint(r, 0.f, 0.f);
				if (r == Here || FVector::Dist(Door, L) < WakeR) WakeRoom(F, r);
			}
			// The boss door drops behind the first one across its threshold.
			const int32 BR = F->BossRoom();
			if (!F->bSealed && Here == BR && BR != INDEX_NONE && F->RoomFrame(BR).InverseTransformPosition(L).X > 300.f)
			{
				F->bSealed = true;
				P->bSealed = true;
				for (TActorIterator<AWRFoldThing> It(World); It; ++It) if (It->Fold == F && It->Kind == TEXT("bossdoor")) It->SetOpen(false);
				UE_LOG(LogWildreach, Log, TEXT("PORTAL %d boss door sealed"), P->PortalId);
			}
		});
		if (bAnyone) F->LastOccupiedUtc = Utc;
		WRFoldHazards::Step(F, Dt);
		// Rooms whose creatures are all down open their doors.
		for (auto& KV : F->RoomCreatures)
		{
			if (F->Cleared.Contains(KV.Key)) continue;
			KV.Value.RemoveAll([](const TWeakObjectPtr<AWRCreature>& C) { return !C.IsValid() || !C->IsAlive(); });
			if (KV.Value.Num() == 0 && F->Rooms.IsValidIndex(KV.Key) && F->Rooms[KV.Key].Type == TEXT("combat")) RoomCleared(F, KV.Key);
		}
		// The loot window, then it closes.
		if (F->CloseAt > 0.f && Now >= F->CloseAt)
		{
			ClosePortal(P, true);
			continue;
		}
		// Nobody in it for a long while (and it isn't won): it settles back, the portal stays as it was.
		if (F->CloseAt <= 0.f && !bAnyone && Utc - F->LastOccupiedUtc > WRPortals::Num(TEXT("entry.idleResetMinutes"), 30.f) * 60.0)
		{
			UE_LOG(LogWildreach, Log, TEXT("PORTAL %d fold empty %.0f min: reset"), P->PortalId, (Utc - F->LastOccupiedUtc) / 60.0);
			TearDownFold(F, P->Front());
			P->Fold = nullptr;
			P->bRunOpen = false;
			P->bSealed = false;
		}
	}
}

void UWRPortalSystem::WakeRoom(AWRFold* F, int32 Room)
{
	F->Woken.Add(Room);
	const FWRFoldRoomRep& R = F->Rooms[Room];
	if (R.Type == TEXT("boss")) { SpawnBoss(F); return; }
	const TSharedPtr<FJsonObject> Row = F->RoomRow(Room);
	const TArray<TSharedPtr<FJsonValue>>* Spawns = PArr(Row, TEXT("spawns"));
	if (!Spawns || Spawns->Num() == 0) return;
	const TArray<TSharedPtr<FJsonValue>>* Table = PArr(PChild(WRPortals::Kit(F->Biome), TEXT("creatures")), FString::FromInt(F->Rank));
	const FVector2D Pack = PRange(WRPortals::Rank(F->Rank), TEXT("packs"), FVector2D(1, 3));
	int32 Count = FMath::RandRange(int32(Pack.X), int32(Pack.Y));
	if (F->Modifiers.Contains(TEXT("swarming"))) Count = FMath::CeilToInt(Count * WRPortals::ModNum(TEXT("swarming"), TEXT("count"), 1.5f));
	// Trap rooms keep a smaller guard beyond the traps (they push you into them).
	if (R.Type == TEXT("trap")) Count = FMath::Max(1, Count / 2);
	TArray<TWeakObjectPtr<AWRCreature>>& Mine = F->RoomCreatures.FindOrAdd(Room);
	for (int32 i = 0; i < Count; ++i)
	{
		const FName Kind = PPick(Table);
		const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(Kind);
		if (!Def) continue;
		float H = 1.f, D = 1.f, S = 1.f;
		PsScaleFor(F->Rank, Def->Tier, H, D, S);
		const TSharedPtr<FJsonValue>& Sp = (*Spawns)[i % Spawns->Num()];
		const FVector At = F->RoomPoint(Room, PAt(Sp, 0), PAt(Sp, 1)) + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 0.f);
		// A swarm pick is a swarm: four mites to the slot.
		const int32 N = Kind == TEXT("anvil_mite") ? 4 : 1;
		for (int32 k = 0; k < N; ++k)
		{
			if (AWRCreature* C = PsSpawnScaled(GetWorld(), Kind, At + FVector(k * 60.f, k * 40.f, 0.f), 180.f, H, D, S, FColor(0, 0, 0, 0)))
			{
				C->SetHome(At);
				Mine.Add(C);
			}
		}
	}
	UE_LOG(LogWildreach, Log, TEXT("PORTAL fold room %d (%s) wakes: %d creatures"), Room, *R.RoomId.ToString(), Mine.Num());
}

void UWRPortalSystem::RoomCleared(AWRFold* F, int32 Room)
{
	F->Cleared.Add(Room);
	for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It)
	{
		if (It->Fold == F && It->Room == Room && It->Kind == TEXT("door") && It->State == 0) It->SetOpen(true);
	}
}

void UWRPortalSystem::SpawnBoss(AWRFold* F)
{
	if (F->Boss.IsValid()) return;
	const int32 Room = F->BossRoom();
	const TArray<TSharedPtr<FJsonValue>>* Bosses = PArr(PChild(WRPortals::Obj(TEXT("biomes")), F->Biome.ToString()), TEXT("bosses"));
	const FName Id = Bosses && Bosses->IsValidIndex(F->Rank - 1) ? FName((*Bosses)[F->Rank - 1]->AsString()) : NAME_None;
	const TSharedPtr<FJsonObject> Row = F->RoomRow(Room);
	const TSharedPtr<FJsonValue> At = Row.IsValid() && Row->HasField(TEXT("boss")) ? Row->Values.FindRef(TEXT("boss")) : nullptr;
	const FVector Spot = F->RoomPoint(Room, PAt(At, 0, F->Rooms[Room].W / 200.f), PAt(At, 1, 0.f));
	AWRCreature* C = PsSpawnScaled(GetWorld(), Id, Spot, 180.f, 1.f, 1.f, 1.f, WRPortals::RankColour(F->Rank));
	AWRBoss* B = Cast<AWRBoss>(C);
	if (!B) return;
	UWRPortalBossBrain* Brain = NewObject<UWRPortalBossBrain>(B);
	Brain->Fold = F;
	Brain->Room = Room;
	Brain->Home = Spot;
	Brain->RegisterComponent();
	Brain->Spawned();
	F->Boss = B;
	UE_LOG(LogWildreach, Log, TEXT("PORTAL fold boss %s in room %d"), *Id.ToString(), Room);
}

void UWRPortalSystem::BossDied(AWRBoss* Boss)
{
	for (const TWeakObjectPtr<AWRFold>& W : AWRFold::All())
	{
		AWRFold* F = W.Get();
		if (!F || F->Boss.Get() != Boss || F->CloseAt > 0.f) continue;
		Reward(F);
		F->CloseAt = GetWorld()->GetTimeSeconds() + WRPortals::Num(TEXT("entry.lootSeconds"), 120.f);
		if (AWRPortal* P = F->Portal.Get()) P->bClosing = true;
		// Every door in the fold opens (the way back to the entry stone, the side rooms).
		for (TActorIterator<AWRFoldThing> It(GetWorld()); It; ++It) if (It->Fold == F && (It->Kind == TEXT("door") || It->Kind == TEXT("bossdoor"))) It->SetOpen(true);
		for (const FString& N : F->Roster)
		{
			if (AWRPlayerController* PC = PsControllerOf(PsStateOf(N, GetWorld()))) PC->ClientNotify(TEXT("The fold is won: it closes in 2 minutes"));
		}
		return;
	}
}

void UWRPortalSystem::Reward(AWRFold* F)
{
	AWRBoss* B = F->Boss.Get();
	if (!B) return;
	const float Need = B->GetAttributes()->GetMaxHealth() * WRTune(TEXT("bosses.lootDamageFraction"), 0.1f);
	const TArray<TSharedPtr<FJsonValue>>* Rewards = PArr(WRPortals::Data(), TEXT("rewards"));
	const TSharedPtr<FJsonObject> Row = Rewards && Rewards->IsValidIndex(F->Rank - 1) ? (*Rewards)[F->Rank - 1]->AsObject() : nullptr;
	for (const TPair<FString, float>& L : B->Ledger)
	{
		AWRPlayerState* PS = PsStateOf(L.Key, GetWorld());
		if (!PS || L.Value < Need) continue;
		const float Q = PsQuantity(F, PS);
		TMap<FName, int32> Items;
		PsResolve(PChild(Row, TEXT("boss")), F->Biome, Q, Items);
		TArray<FName> Special;
		PsChances(PChild(Row, TEXT("bossChance")), F->Biome, Items, Special);
		Items.FindOrAdd(FName(PStr(WRPortals::Rank(F->Rank), TEXT("shard")))) += 1;
		const int32 Up = int32(PNum(Row, TEXT("upgradeShards"), 0.f));
		if (Up > 0) Items.FindOrAdd(TEXT("upgrade_shard")) += Up;
		TArray<FWRItemStack> Gear;
		for (const FName& S : Special)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pool = PArr(WRPortals::Obj(TEXT("fineGear")), S == TEXT("@masterwork") ? TEXT("5") : TEXT("2"));
			const EWRQuality Grade = S == TEXT("@masterwork") || WRArtifacts::RollQualityStep(PS) ? EWRQuality::Masterwork : EWRQuality::Fine;   // artifacts: a step up now and then
			if (Pool && Pool->Num()) Gear.Add(PsGearAt(FName((*Pool)[FMath::RandHelper(Pool->Num())]->AsString()), Grade));
		}
		const FName Bp = FWRBlueprints::PortalDrop(PS, F->Rank);
		if (!Bp.IsNone()) Items.FindOrAdd(Bp) += 1;
		FString Got = PsGive(PS, Items, Gear);
		// Artifacts (rank 3+): the land's portal artifact, a point more a party AL.
		const FString Art = WRArtifacts::PortalDrop(PS, F->Biome, F->Rank, PNum(Row, TEXT("artifactChance"), 0.f));
		if (!Art.IsEmpty()) Got += TEXT(", ") + Art;
		if (AWRPlayerController* PC = PsControllerOf(PS)) PC->ClientNotify(FString::Printf(TEXT("The fold's guardian falls: %s"), *Got));
		WRDeeds::OnPortalCleared(PS, F->Rank, F->Modifiers.Num(), 0.f);   // the Heron's draught now and then; the portal deeds
		UE_LOG(LogWildreach, Log, TEXT("PORTAL reward %s (x%.2f): %s"), *L.Key, Q, *Got);
	}
}

void UWRPortalSystem::ClosePortal(AWRPortal* P, bool bCleared)
{
	UWorld* World = GetWorld();
	if (!P) return;
	const FVector Front = P->Front();
	if (AWRFold* F = P->Fold.Get()) TearDownFold(F, Front);
	if (P->Slot >= 0 && !P->bKey)
	{
		TArray<double>& Refill = SlotRefill.FindOrAdd(P->Biome);
		if (Refill.Num() <= P->Slot) Refill.SetNumZeroed(P->Slot + 1);
		const FVector2D Hours = PRange(WRPortals::Obj(TEXT("slots")), TEXT("refillHours"), FVector2D(2, 6));
		Refill[P->Slot] = UtcNow() + FMath::FRandRange(Hours.X, Hours.Y) * 3600.0;
	}
	if (bCleared)
	{
		// A faint scorched ring where it stood, for a game day.
		WRFx::Broadcast(P, TEXT("crater.smoke"), P->GetActorLocation(), FRotator::ZeroRotator, 1.f);
		// A game day of real time: the clock's dayMinutes.
		const float Seconds = WRPortals::Num(TEXT("entry.scorchHours"), 24.f) / 24.f * WRTune(TEXT("clock.dayMinutes"), 60.f) * 60.f;
		AWRGroundMarker::Spawn(World, P->GetActorLocation() + FVector(0, 0, 5.f), FRotator::ZeroRotator, FVector2D(260.f, 260.f), false, FLinearColor(0.02f, 0.015f, 0.012f, 0.6f), Seconds);
	}
	WRSound::PlayAt(P, TEXT("portal.close"), P->GetActorLocation());
	Portals.Remove(P);
	P->Destroy();
	Save();
}

void UWRPortalSystem::TearDownFold(AWRFold* F, const FVector& PutAt)
{
	UWorld* World = GetWorld();
	if (!F) return;
	AWRPortal* P = F->Portal.Get();
	const FVector Out = !PutAt.IsZero() ? PutAt : P ? P->Front() : FVector::ZeroVector;
	const float Yaw = P ? P->GetActorRotation().Yaw + 180.f : 0.f;
	// Everyone inside is put back at the portal, with their tames; every death bag still inside goes there too.
	PsForPlayers(World, [&](AWRPlayerController* PC)
	{
		APawn* Pawn = PC->GetPawn();
		if (Pawn && F->Bounds.IsInsideOrOn(Pawn->GetActorLocation()) && !Out.IsZero()) PsPut(Pawn, Out + FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 0.f), Yaw);
	});
	int32 Bags = 0;
	for (TActorIterator<AWRLootBag> It(World); It; ++It)
	{
		if (F->Bounds.IsInsideOrOn(It->GetActorLocation()) && !Out.IsZero())
		{
			It->SetActorLocation(Out + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 30.f));
			++Bags;
		}
	}
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (!F->Bounds.IsInsideOrOn(It->GetActorLocation())) continue;
		if (It->IsTamed()) { if (!Out.IsZero()) PsPut(*It, Out + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 0.f), Yaw); }
		else It->Destroy();
	}
	for (TActorIterator<AWRFoldThing> It(World); It; ++It) if (It->Fold == F) It->Destroy();
	if (Bags > 0) UE_LOG(LogWildreach, Log, TEXT("PORTAL fold closed: %d death bags moved to the entrance"), Bags);
	F->Destroy();
}

void UWRPortalSystem::StepRescue()
{
	// Anyone in the bays with no fold round them (a fold torn down under them, a server restart): back to their portal,
	// or where the server last saw them go in, or the island's start.
	UWorld* World = GetWorld();
	PsForPlayers(World, [&](AWRPlayerController* PC)
	{
		APawn* Pawn = PC->GetPawn();
		if (!Pawn || !PC->PlayerState || !WRPortals::InBaySpace(Pawn->GetActorLocation()) || WRPortals::FoldAt(Pawn, Pawn->GetActorLocation())) return;
		const FString Name = PC->PlayerState->GetPlayerName();
		FVector To = FVector::ZeroVector;
		if (const FVector* Saved = InsideAtStop.Find(Name)) To = *Saved;
		if (To.IsZero())
		{
			for (const TWeakObjectPtr<AWRPortal>& W : Portals) if (W.IsValid() && W->Fold && W->Fold->Roster.Contains(Name)) To = W->Front();
		}
		if (To.IsZero())
		{
			if (AGameModeBase* GM = World->GetAuthGameMode()) if (AActor* Start = GM->FindPlayerStart(PC)) To = Start->GetActorLocation();
		}
		InsideAtStop.Remove(Name);
		PsPut(Pawn, To, 0.f);
		UE_LOG(LogWildreach, Log, TEXT("PORTAL rescue: %s put back at %s"), *Name, *To.ToCompactString());
	});
}

FString UWRPortalSystem::Dev(AWRPlayerController* PC, const TArray<FString>& Args)
{
	const auto Arg = [&Args](int32 i) { return Args.IsValidIndex(i) ? Args[i] : FString(); };
	const FString What = Arg(2);
	APawn* Me = PC->GetPawn();
	AWRPortal* PsNearest = nullptr;
	float Best = 1.0e9f;
	for (const TWeakObjectPtr<AWRPortal>& W : Portals)
	{
		if (!W.IsValid() || !Me) continue;
		const float D = FVector::Dist(W->GetActorLocation(), Me->GetActorLocation());
		if (D < Best) { Best = D; PsNearest = W.Get(); }
	}
	if (What == TEXT("open") && Me)
	{
		const FName Biome = Arg(3).IsEmpty() ? WRBiome::At(Me, Me->GetActorLocation()) : FName(*Arg(3));
		const int32 Rank = FMath::Clamp(FCString::Atoi(*Arg(4)), 1, 5);
		const FVector At = Me->GetActorLocation() + Me->GetActorForwardVector().GetSafeNormal2D() * 1200.f - FVector(0, 0, Me->GetSimpleCollisionHalfHeight());
		AWRPortal* P = OpenPortal(Biome, Rank, At, Me->GetActorRotation().Yaw + 180.f, -1, FString());
		return P ? FString::Printf(TEXT("Portal %d: %s %s"), P->PortalId, *WRPortals::RankName(Rank), *Biome.ToString()) : TEXT("Couldn't open");
	}
	if (!PsNearest) return TEXT("No portal");
	if (What == TEXT("crack") || What == TEXT("spill") || What == TEXT("break"))
	{
		const double Span = FMath::Max(1.0, PsNearest->BreakAtUtc - PsNearest->OpenedUtc);
		const double Share = What == TEXT("crack") ? 0.55 : What == TEXT("spill") ? 0.85 : 1.01;
		if (PsNearest->BreakAtUtc <= 0.0) PsNearest->BreakAtUtc = UtcNow() + 86400.0;
		PsNearest->OpenedUtc = UtcNow() - Span * Share;
		PsNearest->BreakAtUtc = PsNearest->OpenedUtc + Span;
		PsNearest->NextSpillHours = -1.0;
		return FString::Printf(TEXT("Portal %d set to %s"), PsNearest->PortalId, *What);
	}
	if (What == TEXT("clear"))
	{
		if (AWRFold* F = PsNearest->Fold.Get()) { if (AWRBoss* B = F->Boss.Get()) B->Die(Me); else ClosePortal(PsNearest, true); }
		else ClosePortal(PsNearest, true);
		return TEXT("Cleared");
	}
	return TEXT("portal dev open <biome> <rank> | crack | spill | break | clear");
}
