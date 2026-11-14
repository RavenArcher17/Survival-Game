#pragma once
// Kill-time boards (FULL-GAME 8.3; biome-void 2.23.5; cooks/build-endgame.md 2.5): every major, mini, portal guardian
// and war-hall raid is timed from the first survivor's blow to its death; each kill is entered on its boss's board in
// its group band (solo, 2, 3-4, 5-7, 8-10, 11+; tames count as members, as they do for boss health), and kills made
// wearing artifacts in that band's own column with their Artifact Level. Per world, never reset: the server setting
// recordsSeason starts new boards and keeps the old ones readable. The world document "records" holds them; stone
// tablets beside each boss's shrine-stone (and summoning stone, and war-hall gate) show the top three and open the
// full board. Monuments read their plaque from here.
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRRecords.generated.h"

class AWRPlayerController;
class AWRBuildPiece;
class UTextRenderComponent;

/** A carved stele by a boss's approach: its name, sigil and the top three kills; Interact opens the full board. */
UCLASS(NotPlaceable)
class WILDREACH_API AWRRecordTablet : public AActor
{
	GENERATED_BODY()
public:
	AWRRecordTablet();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	/** The board it shows: a boss id or a raid id. */
	UPROPERTY(ReplicatedUsing=OnRep_Lines) FName BoardId;
	/** Its title and the three best (lines cut into its face). */
	UPROPERTY(ReplicatedUsing=OnRep_Lines) FString Title;
	UPROPERTY(ReplicatedUsing=OnRep_Lines) FString Lines;
private:
	UFUNCTION() void OnRep_Lines();
	void BuildLook();
	bool bBuilt = false;
	bool bShownOnce = false;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextRenderComponent>> Text;
};

namespace WRRecords
{
	/** Server, at start: read the world document; place tablets as their stones appear. */
	WILDREACH_API void Start(UWorld* World);
	/** Server: a kill. Names: everyone with damage in the ledger; Size: players + tames in the fight. */
	WILDREACH_API void Note(UWorld* World, FName Board, const TArray<FString>& Names, int32 Size, float Seconds, int32 AL);
	/** The band a group of this size is in ("solo", "2", "3-4", ...) and its name. */
	WILDREACH_API FString BandOf(int32 Size);
	WILDREACH_API FString BandName(const FString& Band);
	WILDREACH_API TArray<FString> Bands();
	/** Server: send a player the boards (Board None: the summary of every board; else that board in full). */
	WILDREACH_API void SendBoards(AWRPlayerController* PC, FName Board, int32 Season = 0);
	/** Owning client: what the server last sent (the screens read it). */
	WILDREACH_API TSharedPtr<class FJsonObject> ClientBoards(FName Board);
	WILDREACH_API void ClientReceive(FName Board, const FString& Json);
	/** Server: a monument's plaque line for its owner (their best kill of that boss, names, AL); empty: none yet. */
	WILDREACH_API FString PlaqueText(UWorld* World, FName Boss, const FString& Owner, int32* OutBestAL = nullptr);
	/** Server: every banner of this owner takes their sigil again (they picked another). */
	WILDREACH_API void RefreshBanners(UWorld* World, const FString& Owner);
	/** Server: a tablet beside this spot for this board (once). */
	WILDREACH_API AWRRecordTablet* PlaceTablet(UWorld* World, FName Board, const FVector& At, const FRotator& Facing);
	/** A board's display name (a boss's short name, or the raid's). */
	WILDREACH_API FString BoardName(FName Board);
}
