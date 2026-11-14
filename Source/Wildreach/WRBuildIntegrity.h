#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WRBuildIntegrity.generated.h"

struct FWRPieceDef;
class AWRBuildPiece;
class UInstancedStaticMeshComponent;

/**
 * Structural integrity past the grid (cooks/build-building.md 2.3; RAVEN-PLANNED "freeform structural pieces;
 * structural integrity"). Raven's span rule stays: a piece's support is its steps from the ground, and a material
 * carries its span (wood 4, stone 6, metal and the biome sets 8). Freeform beams, posts and braces join it:
 *  - a beam or post takes its support from what its ends touch (the ground: 0) plus 1 step per 2 m it reaches out
 *    (its horizontal run: a post standing straight up bears like a pillar);
 *  - it passes support on to a snapped piece its body runs under or through (+1), or one standing on a post's top (+0);
 *  - a brace with one end on a wall and the other on a pillar or post lets that wall go one step further.
 */
struct WILDREACH_API FWRIntegrity
{
	/** The solid box of a piece in its own space (cm): the grid shapes as ApplyLook builds them, a beam along +X. */
	static FBox BoxOf(const FWRPieceDef& Def);
	/** A beam / post / brace's two ends in world space (it runs FreeLength along its own X from its location). */
	static void Ends(const FWRPieceDef& Def, const FTransform& T, FVector& OutA, FVector& OutB);
	/** Steps a beam costs by itself: 1 per 2 m of its horizontal run, rounded up. */
	static int32 LengthSteps(const FWRPieceDef& Def, const FTransform& T);
	/** What resting To on From costs: INDEX_NONE = they don't bear on each other. */
	static int32 LinkCost(const FWRPieceDef& FromDef, const FTransform& From, const FWRPieceDef& ToDef, const FTransform& To);
	/** A beam with an end on the ground (terrain, rock: not another piece). */
	static bool BeamGrounded(const UWorld* World, const FWRPieceDef& Def, const FTransform& T, const AActor* Ignore);
	/** The most steps this piece may be from the ground: its material's span, +1 for a braced wall. */
	static int32 Allowed(const UWorld* World, const FWRPieceDef& Def, const FTransform& T, const AActor* Ignore = nullptr);
	/** The stability colour of a support value (blue on the ground, green, yellow, orange, red at the last step). */
	static FLinearColor StepColor(int32 Support, int32 Allowed);
	static int32 StepIndex(int32 Support, int32 Allowed);
	/** Server: a piece that lost its support falls as rubble (its own look's material and colour) and is destroyed. */
	static void Collapse(AWRBuildPiece* Piece);
};

/**
 * What a collapsing piece breaks into: 4 to 8 chunks of its material, flung a little outward, falling for 3 s, then
 * shrinking away. Everyone works the chunks' paths out from the replicated seed, so they fall the same on every
 * machine; the server hurts whoever a falling chunk lands on (50 through ApplyHit, once each).
 */
UCLASS()
class WILDREACH_API AWRRubble : public AActor
{
	GENERATED_BODY()

public:
	AWRRubble();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before FinishSpawning: what fell and how big it was. */
	void Setup(FName InPieceId, const FVector& InExtent, int32 InSeed);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated) FName PieceId;
	UPROPERTY(Replicated) FVector_NetQuantize10 Extent = FVector(100.f);
	UPROPERTY(Replicated) int32 Seed = 0;
	UPROPERTY(Replicated) float StartTime = 0.f;

	struct FChunk
	{
		FVector Start = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Size = FVector(40.f);
		FRotator Spin = FRotator::ZeroRotator;
		float GroundZ = -1.0e6f;
		float LandAt = 3.f;   // seconds after the start
		float Shade = 0.5f;
	};
	TArray<FChunk> Chunks;
	TSet<TWeakObjectPtr<AActor>> Hurt;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Pieces;
	void MakeChunks();
	float Now() const;
};
