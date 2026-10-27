#pragma once

#include "CoreMinimal.h"
#include "WRFlowTypes.h"

class AWRPlayerController;
class AWRCreature;
class AWRBoss;
class AWRHearth;
class APawn;
class SWidget;

/**
 * The moments between play, for one local player (FWRPlayerUI owns it). All client-side, reading replicated state:
 *  - the cover: a fade to black on death, the death screen (what killed you, your bag, where you'll wake, the
 *    countdown) and a fade back in once the island has streamed in around you; the same "Loading the island" cover
 *    after joining or a long teleport, so you never stand on unloaded ground;
 *  - moments: banners (level up, a place discovered, a boss's title card, a tame), toasts (new recipes, a path point)
 *    and one-time hints that name your real keys. Discoveries and seen hints are kept per player name in
 *    GameUserSettings ([Wildreach.Flow]); Settings has a "Show hints" switch.
 * The loading screen during map loads (host, join, travel) is the movie player's, armed with the same layout.
 */
class WILDREACH_API FWRFlow : public TSharedFromThis<FWRFlow>
{
public:
	explicit FWRFlow(AWRPlayerController* InPC);
	~FWRFlow();
	void Tick(float DeltaTime);
	/** The server's note that you died (AWRPlayerController::ClientDeathNote). */
	void OnDeathNote(const FWRDeathNote& Note);
	/** A non-shipping test or screenshot command (WRFlow ...). */
	void Command(const TArray<FString>& Args);

	// --- the cover (what its widget reads)
	enum class ECover : uint8 { None, Dying, Loading };
	ECover GetCover() const { return Cover; }
	/** The black, 0..1. */
	float GetBlack() const { return Black; }
	float GetDeathCardAlpha() const;
	const FWRDeathNote& GetDeathNote() const { return Death; }
	/** Seconds until the server brings you back (<= 0: any moment now). */
	float GetRespawnLeft() const;
	/** 0..1 of the respawn wait. */
	float GetRespawnFraction() const;
	/** Back in a body and waiting for the island to stream in. */
	bool IsBack() const { return bBack; }
	/** The full loading layout (title, tip) rather than just a status line. */
	bool ShowsLoadingArt() const { return Cover == ECover::Loading && bLoadingArt; }
	bool IsWaitingForIsland() const { return bStreaming; }

	// --- moments
	struct FBanner
	{
		enum class EKind : uint8 { Level, Discover, Boss, Tame };
		EKind Kind = EKind::Level;
		FText Over, Title, Sub;
		FName Portrait;          // a creature's portrait (tame, boss)
		FName Glyph;             // Data/UI/Glyphs/<name>.png (discover)
		FLinearColor Accent = FLinearColor::White;
		float Seconds = 4.f;
		double Start = -1.0;
	};
	struct FToast
	{
		FText Over, Title, Sub;
		FName Icon;              // an item icon
		FLinearColor Accent = FLinearColor::White;
		float Seconds = 5.5f;
		double Start = -1.0;
	};
	struct FHint
	{
		FName Id;
		FName Action;            // the key binding it names (None: no key)
		FText Text;
		float Seconds = 9.f;
		double Start = -1.0;
	};
	/** The banner showing now (null: none) and its fade 0..1. */
	const FBanner* GetBanner(float& OutAlpha) const;
	/** Toast slot i (0..2, top down) and its fade, or null. */
	const FToast* GetToast(int32 Slot, float& OutAlpha) const;
	const FHint* GetHint(float& OutAlpha) const;

	/** Where your belongings bag is (from the death note) while it lasts and isn't picked up. */
	bool GetBagMarker(FVector& OutLocation) const;

	/** Settings: one-time hints on or off (saved). Tests can override it for a run without saving (WRFlow hints). */
	static bool HintsEnabled();
	static void SetHintsEnabled(bool bOn);

private:
	AWRPlayerController* GetPC() const { return PC.Get(); }
	void TickCover(float DeltaTime);
	void TickMoments();
	void StartLoading(bool bArt, float FadeSeconds);
	void StartIslandWait(const FVector& At);
	bool IsIslandStreamed(const FVector& At) const;
	FVector FocusLocation() const;

	void CheckLevel(const class UWRPathComponent* Paths);
	void CheckDiscoveries(const FVector& At);
	void CheckBosses(const FVector& At);
	void CheckTames(const FVector& At);
	void CheckHearths(const FVector& At);
	void CheckHints(const FVector& At);
	void ShowBanner(const FBanner& Banner, bool bFirst = false);
	void ShowToast(const FToast& Toast);
	void ShowHint(FName Id, FName Action, const FText& Text);
	bool WasSeen(FName HintId) const;

	void LoadMemory();
	void SaveMemory() const;
	FString MemoryKey(const TCHAR* What) const;

	void Preview(const TArray<FString>& Args);
	void TakeShot(const TCHAR* Why);

	TWeakObjectPtr<AWRPlayerController> PC;
	TSharedPtr<SWidget> MomentsWidget;
	TSharedPtr<SWidget> CoverWidget;

	// cover
	ECover Cover = ECover::None;
	float Black = 0.f;
	float BlackTarget = 0.f;
	float BlackRate = 4.f;     // per second
	/** The wait is over: fading back in. */
	bool bReleasing = false;
	double LastTickAt = 0.0;
	bool bLoadingArt = false;
	double CoverStart = 0.0;
	double CardShownAt = -1.0;
	FWRDeathNote Death;
	double DeathAt = -100.0;
	bool bBack = false;
	TWeakObjectPtr<APawn> DeadBody;
	bool bStreaming = false;
	double StreamStart = 0.0;
	double NextStreamCheck = 0.0;
	bool bBagMarker = false;
	double BagUntil = 0.0;

	// pawn tracking
	TWeakObjectPtr<APawn> LastPawn;
	/** The last living body we had (the one that falls when the death note comes). */
	TWeakObjectPtr<APawn> LastBody;
	FVector LastPawnAt = FVector::ZeroVector;
	double AliveSince = -100.0;
	bool bEverHadPawn = false;

	// moments
	double NextMomentCheck = 0.0;
	double NextSlowCheck = 0.0;
	int32 LastLevel = -1;
	int32 LastUnspent = -1;
	int32 LastCastables = -1;
	bool bWasNight = false;
	TArray<FBanner> Banners;
	TArray<FToast> Toasts;
	TArray<FHint> Hints;
	TSet<TWeakObjectPtr<AWRBoss>> CardedBosses;
	TSet<TWeakObjectPtr<AWRCreature>> WildSeen;
	TMap<TWeakObjectPtr<AWRHearth>, int32> HearthLevels;

	// memory (per player name)
	FString MemoryFor;
	TSet<FString> Found;
	TSet<FName> Seen;

	// tests
	/** Screenshots of other screens: no moments at all this run (WRFlow quiet on). */
	bool bQuiet = false;
	double ShotAt = -1.0;
	float ShotOnDeath = -1.f;
	float ShotOnBack = -1.f;
	bool bPreviewHold = false;
	double PreviewUntil = -1.0;
	TWeakObjectPtr<class ACameraActor> TestCam;
};

namespace WRUIFlow
{
	/**
	 * The loading layout: the island behind (Data/UI/loading.jpg), the carved title, a tip with its real key and a
	 * status line with a spinner. Built on the game thread; with a fixed Status it is safe on the movie player's
	 * loading thread (it reads no game state while it draws).
	 */
	WILDREACH_API TSharedRef<SWidget> MakeLoadingScreen(const TAttribute<FText>& Status, bool bBackdrop);
}
