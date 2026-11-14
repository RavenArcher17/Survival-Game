#pragma once
// The endgame's deeds (FULL-GAME 8; cooks/build-endgame.md): prestige at a path's altar (a path at 100 returned for a
// permanent mark), the respec (the Heron's draught: one path's learning forgotten), boss soul forging (a soul at its
// boss's altar: the awakened boss weapon with its Soul art, or the soul talisman), mythic gear's rules (data-ready:
// Data/mythic.json, hidden until the Ring exists), achievements (Data/achievements.json: 150 rows, only the ones whose
// content exists are live), titles (shown under your name and before it in chat), cosmetics (Data/cosmetics.json:
// weapon glows, armour dyes, raid marks, banner sigils, prestige gold threads) and boss monuments. Everything a player
// earns lives in UWRDeedsComponent on their PlayerState and saves with their character. Server authority throughout:
// the screens send "deeds ..." commands; the component checks and acts.
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRPathTypes.h"
#include "WRDeeds.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class AWRCharacterBase;
class AWRCreature;
class AWRBoss;
class AWRBuildPiece;
class FJsonObject;
struct FWRHit;
struct FGameplayTag;

/** One path's prestige marks, by perk (keener: its damage; deeper: its resource; rooted: its root passive as subclass). */
USTRUCT()
struct FWRPrestigeRep
{
	GENERATED_BODY()
	UPROPERTY() FName Path;
	UPROPERTY() uint8 Keener = 0;
	UPROPERTY() uint8 Deeper = 0;
	UPROPERTY() uint8 Rooted = 0;
	int32 Total() const { return Keener + Deeper + Rooted; }
};

/** The nodes a path had when it was returned (Restore buys them back in this order as points come). */
USTRUCT()
struct FWRPathLayout
{
	GENERATED_BODY()
	UPROPERTY() FName Path;
	UPROPERTY() TArray<FWRNodeRank> Ranks;
};

/** What a boss's souls have made for you: its awakened weapon (A) and its talisman (B). */
USTRUCT()
struct FWRSoulForged
{
	GENERATED_BODY()
	UPROPERTY() FName Boss;
	UPROPERTY() bool bWeapon = false;
	UPROPERTY() bool bTalisman = false;
};

/** A player's deeds (on their PlayerState, saved in their character). */
UCLASS(ClassGroup=(Endgame))
class WILDREACH_API UWRDeedsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWRDeedsComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	static UWRDeedsComponent* For(const AActor* PlayerOrStateOrPawn);

	// --- Everyone sees (the look on your body and name) ---
	/** Prestige marks per path (gold threads in that path's marks on your skin). */
	UPROPERTY(ReplicatedUsing=OnRep_Look) TArray<FWRPrestigeRep> Marks;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FString ActiveTitle;
	/** Worn cosmetics (None: none): a weapon glow, an armour dye, a raid mark; the sigil woven on your claim banners. */
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName WornGlow;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName WornDye;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName WornMark;
	UPROPERTY(ReplicatedUsing=OnRep_Look) FName BannerSigil;

	// --- Only you (the screens) ---
	UPROPERTY(Replicated) TArray<FWRPathLayout> Layouts;
	UPROPERTY(Replicated) TArray<FWRSoulForged> Forged;
	UPROPERTY(Replicated) TArray<FName> Achieved;
	UPROPERTY(Replicated) TArray<FString> Titles;
	UPROPERTY(Replicated) TArray<FName> Cosmetics;
	/** Great and lesser bosses you've earned loot from (the banner sigils you can weave; the Records tab). */
	UPROPERTY(Replicated) TArray<FName> Bosses;
	/** Achievement progress the Deeds tab shows ("7 / 11 landmarks"): row id -> have, need. */
	UPROPERTY(Replicated) TArray<FName> ProgressIds;
	UPROPERTY(Replicated) TArray<FIntPoint> Progress;
	/** The soul art's quick slot (0..7; -1: none held) and what the slot held before (put back when it's gone). */
	int32 SoulSlot = -1;
	FName SoulSlotWas;

	// --- Server: counters the achievements read (saved) ---
	TSet<FName> Landmarks;
	TSet<FName> Species;
	TSet<FName> Tamed;
	TSet<FName> PassesCrossed;
	TSet<FName> HiddenFound;
	TSet<FName> Raids;
	/** The game day of your last death (days alive = today - this). */
	int32 DeathDay = -1;
	/** The side of the ridge you stood on last check, near which pass (passes crossed). */
	int32 LastSide = 0;
	FName NearPass;
	/** Rime-scale: when you last took a blow (server time). */
	double LastHurtAt = -1000.0;
	float CheckClock = 0.f;
	float CensusClock = 0.f;

	// --- Queries (server and owner) ---
	const FWRPrestigeRep* MarksOn(FName Path) const;
	bool HasForged(FName Boss, bool bWeapon) const;
	bool HasDeed(FName Id) const { return Achieved.Contains(Id); }

	TSharedRef<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& O);
	/** Redraw the look on this player's pawn (the server and a listen host call it after a change; clients on rep). */
	void RefreshLook() { OnRep_Look(); }
	bool bDirty = false;

private:
	UFUNCTION() void OnRep_Look();
	void StepChecks();
};

namespace WRDeeds
{
	// --- Data ---
	WILDREACH_API TSharedPtr<FJsonObject> Achievements();
	WILDREACH_API TSharedPtr<FJsonObject> Cosmetics();
	WILDREACH_API TSharedPtr<FJsonObject> Souls();
	WILDREACH_API TSharedPtr<FJsonObject> Mythic();
	/** An achievement's row (null: none). */
	WILDREACH_API TSharedPtr<FJsonObject> Deed(FName Id);
	/** The row's content exists in this build (its needs are all here). */
	WILDREACH_API bool IsLive(const TSharedPtr<FJsonObject>& Row);
	/** A cosmetic's row in glows / dyes / marks (OutKind: glow, dye, mark). */
	WILDREACH_API TSharedPtr<FJsonObject> Cosmetic(FName Id, FString* OutKind = nullptr);
	WILDREACH_API FColor CosmeticColour(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field = TEXT("colour"));
	/** A boss's sigil: 9 rows of '#' and '.' (empty: none). */
	WILDREACH_API TArray<FString> Sigil(FName Id);
	/** The soul row of a boss (null: none or not live). */
	WILDREACH_API TSharedPtr<FJsonObject> SoulRow(FName Boss);
	/** The boss whose soul row names this weapon (None: none). */
	WILDREACH_API FName SoulBossOfWeapon(FName WeaponItem);
	/** "Ember-sworn" for fire. */
	WILDREACH_API FString SwornTitle(FName Path);
	/** "3:12" / "1:04:09". */
	WILDREACH_API FString Clock(float Seconds);

	// --- Prestige (the numbers the game reads) ---
	/** Damage x for a hit by this attacker (keener marks on the hit's path, while that path is equipped). */
	WILDREACH_API float PrestigeDamage(const AActor* Attacker, const FWRHit& Hit);
	/** Resource capacity and gain x for this element's path (deeper marks). */
	WILDREACH_API float PrestigeResource(const AActor* Who, FName Path);
	/** The subclass's root passive works at half (rooted mark on the subclass path). */
	WILDREACH_API bool RootedSub(const AActor* Who, FName& OutPath);
	/** The gold the skin's path marks lean toward, 0..1 (gold threads). */
	WILDREACH_API float GoldThreads(const AActor* Who, FName Path);

	// --- Server actions (each returns what to tell the player) ---
	WILDREACH_API FString Prestige(AWRPlayerState* PS, FName Path, FName Perk, const AActor* Altar);
	WILDREACH_API FString RestoreLayout(AWRPlayerState* PS, FName Path);
	WILDREACH_API FString Respec(AWRPlayerState* PS, FName Path);
	WILDREACH_API FString ForgeSoul(AWRPlayerState* PS, FName Boss, bool bWeapon, const AActor* Altar);
	WILDREACH_API FString SalvageSoul(AWRPlayerState* PS, FName Boss, const AActor* Altar);
	/** Mythic forging's rule check (data-ready; nothing calls it until the Ring exists). Empty when it would succeed. */
	WILDREACH_API FString MythicCheck(const AWRPlayerState* PS, int32 BagIndex);
	WILDREACH_API FString WearTitle(AWRPlayerState* PS, const FString& Title);
	WILDREACH_API FString WearCosmetic(AWRPlayerState* PS, const FString& Kind, FName Id);
	WILDREACH_API void GiveTitle(AWRPlayerState* PS, const FString& Title, const FString& Why);
	WILDREACH_API void GiveCosmetic(AWRPlayerState* PS, FName Id, const FString& Why);

	// --- What a screen can show (owner) ---
	/** Why Return at this altar can't happen now (empty: it can). */
	WILDREACH_API FString PrestigeBlock(const AWRPlayerController* PC, FName Path);
	WILDREACH_API FString RespecBlock(const AWRPlayerController* PC, FName Path);
	WILDREACH_API FString ForgeBlock(const AWRPlayerController* PC, FName Boss, bool bWeapon);
	/** The boss whose altar this is (the major of the altar's path's land). */
	WILDREACH_API FName AltarBoss(FName Path);

	// --- Hooks (server) ---
	/** A major or mini died: souls, heads, draughts, the solo / deathless / geis achievements, the board. */
	WILDREACH_API void OnBossKilled(AWRBoss* Boss);
	/** Anything died (species kills, belts; a survivor's death breaks their days alive and a boss fight's deathless). */
	WILDREACH_API void OnKilled(AWRCharacterBase* Victim, AActor* Killer);
	WILDREACH_API void OnTamed(AWRCreature* Creature, const FString& Player);
	/** A quest step done with a title_reward / the last of a line. */
	WILDREACH_API void OnQuestStep(AWRPlayerState* PS, const TSharedPtr<FJsonObject>& Step);
	WILDREACH_API void OnPortalCleared(AWRPlayerState* PS, int32 Rank, int32 Modifiers, float PortalChanceDraught);
	WILDREACH_API void OnEscapedSlain(AWRPlayerState* PS);
	WILDREACH_API void OnRaidHeld(AWRPlayerState* PS);
	/** A war-hall won: every looter's raid deeds. */
	WILDREACH_API void OnRaidWon(AWRPlayerState* PS, FName Raid, bool bNoTriumph, int32 PartyAL);
	/** An event a row may wait on (the generic path): ticks every live row of this event for PS. */
	WILDREACH_API void Notify(AWRPlayerState* PS, FName Event, FName Key = NAME_None, int32 Value = 0);
	/** A rest finished (comfort deeds). */
	WILDREACH_API void OnRested(AWRPlayerState* PS);

	// --- Equipment (the soul art; awakened / mythic marks) ---
	/** The soul art node ("soul.vulkarn") the held weapon gives (None: none). */
	WILDREACH_API FName SoulArtFor(const AActor* Who);
	/** Talisman effects: the path resource gain x (Forge-heart), the blow scale (Rime-scale). */
	WILDREACH_API float ResourceGainScale(const AActor* Who);
	WILDREACH_API float FirstHitScale(AActor* Victim);

	/** Server: items into the bag; what doesn't fit waits in a bag on the ground at At, only theirs. */
	WILDREACH_API void GiveOrBag(AWRPlayerState* PS, FName Item, int32 Count, const FVector& At);
	/** Server: "deeds ..." commands from the screens. */
	WILDREACH_API FString Command(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Owning client, E with no menu: an altar opens its screen. True if it did. */
	WILDREACH_API bool ClientInteract(AWRPlayerController* PC);
	/** Server, at startup: hook the rest delegate. */
	WILDREACH_API void Start(UWorld* World);
}
