#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WRMapReveal.generated.h"

class FJsonObject;

/**
 * The fill-in map (RAVEN-PLANNED "a map you complete by exploring"; cooks/ui-controls.md 2.12, cookpass-ui 3.9): what
 * each player has seen, as a grid of ui.reveal.cellCm cells (8 m) over the map picture (ui.mapMin / ui.mapSize, so it
 * grows with the map), kept on the server per player and saved with them.
 *
 * Server: every ui.reveal.tickSeconds the cells within ui.reveal.radiusCm of the player's body are marked; a shrine they
 * have found inks ui.reveal.shrineCm around it (Elden Ring's map fragments, tied to the shrines). Changed 64 x 64 cell
 * tiles go to the owning client a few a frame; a client that joins asks for all of its tiles once. The journal map draws
 * the unexplored land under a parchment fog (the island's outline faintly through it) and leaves out camps, caves and
 * passes not yet seen.
 */
UCLASS(ClassGroup=(World))
class WILDREACH_API UWRMapRevealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRMapRevealComponent();
	static UWRMapRevealComponent* For(const AActor* PlayerOrState);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Seen (a world position, cm)? Outside the map: always (the sea past the picture). */
	bool IsRevealed(const FVector2D& World) const;
	/** Cells across (and down) the grid, and tiles across. */
	int32 Cells() const { return N; }
	int32 TilesAcross() const { return T; }
	/** One cell's bit (grid coordinates). */
	bool Cell(int32 X, int32 Y) const;
	/** Changes whenever tiles arrive or are marked (the map rebuilds its fog when it moves). */
	int32 GetVersion() const { return Version; }

	/** Server: mark a circle seen. */
	void RevealCircle(const FVector2D& Centre, float RadiusCm);

	TSharedPtr<FJsonObject> ToJson() const;
	void FromJson(const TSharedPtr<FJsonObject>& Json);

	UFUNCTION(Client, Reliable) void ClientTile(int32 Tile, const TArray<uint8>& Bytes);
	UFUNCTION(Server, Reliable) void ServerSendAll();

private:
	void Size();
	int32 TileOf(int32 X, int32 Y) const { return (Y / 64) * T + (X / 64); }

	float CellCm = 800.f;
	float MinCm = 0.f;
	float SpanCm = 1.f;
	int32 N = 0;    // cells across
	int32 T = 0;    // tiles across
	TArray<uint8> Bits;        // tile-major: each tile's 64 x 64 bits are 512 bytes in a row
	TSet<int32> Dirty;         // server: tiles changed since last sent
	TArray<int32> SendQueue;
	TSet<FName> Stamped;       // server: shrines already inked
	float Accum = 0.f;
	int32 Version = 0;
	bool bAsked = false;       // client: asked the server for its tiles
};
