#pragma once
// The testers' god mode (cooks/build-godmode.md in showcase-notes). Aaron 11-15: "I need to have a god mode in Wildreach
// so I can test everything. This is something that should only be available to Aaron, Raven and Colin and just for
// testing."
//
// WHO: a tester is whoever the Freedom35 site vouches for (its game_testers list), proven per connection: the server
// sends a nonce at login (WRGod::Challenge), the tester's game asks the site for a signed proof bound to that nonce and
// to this PC's identity token (WRGodAuth), the server checks the Ed25519 signature against the public key compiled in
// (WRGodAuth::Verify) and sets UWRGodComponent::bTester, replicated. Nothing typed, saved or passed on the command line
// makes anyone a tester.
//
// WHAT: every power is a "god <verb> ..." server command (WRGod::Run), re-checked against the flag each time; the old
// dev commands need it too in packaged builds (WRGod::DevBlock). Nothing runs on a public world (a dedicated server
// not on a private preset). Using any of it marks the character as tested for good, and world changes mark the world
// (Saved/Worlds/<world>/tested.json): no achievements or kill-time records from a tested character.
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "WRGod.generated.h"

class AWRPlayerController;
class AWRPlayerState;
class ACharacter;
class APlayerController;
class FJsonObject;

/** The powers a tester has on (UWRGodComponent::Flags). */
namespace EWRGodFlag
{
	enum : uint16
	{
		Invulnerable = 1 << 0,   // blows land, health never drops, death refused
		Vitals       = 1 << 1,   // stamina, every path bar, hunger, thirst full; corruption and build-ups empty
		OneHit       = 1 << 2,   // your landed blows take the whole of the target's health
		Fly          = 1 << 3,   // flying, Jump rises, Dive / B sinks, the camera's pitch carries you
		Noclip       = 1 << 4,   // flying with no collision
		Unseen       = 1 << 5,   // creatures and bosses never pick you
	};
}

/** One place the Travel page lists (built by the server from the world's own data). */
USTRUCT()
struct FWRGodPlace
{
	GENERATED_BODY()
	UPROPERTY() FString Name;
	/** land, boss, shrine, village, camp, cave, pass, landmark, portal, player, place */
	UPROPERTY() FName Kind;
	UPROPERTY() FName Land;
	UPROPERTY() FVector_NetQuantize Where = FVector::ZeroVector;
};

/** A survivor's tester state (on their PlayerState). */
UCLASS(ClassGroup=(Testing))
class WILDREACH_API UWRGodComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWRGodComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The component for a PlayerState, its pawn or its controller (null for creatures). */
	static UWRGodComponent* For(const AActor* Any);

	/** Verified by the server this connection (everyone sees it: the "GOD" tag over a tester's name needs it). */
	UPROPERTY(Replicated) bool bTester = false;
	/** The tester's first name from the site's list ("Aaron"). */
	UPROPERTY(Replicated) FString Label;
	UPROPERTY(ReplicatedUsing=OnRep_Flags) uint16 Flags = 0;
	/** Fly speed as a multiple of god.flyBase. */
	UPROPERTY(ReplicatedUsing=OnRep_Flags) float FlySpeed = 1.f;
	/** This character used god mode once: no achievements or kill-time records from it, for good. */
	UPROPERTY(Replicated) bool bCharacterTested = false;
	// --- Only the owner (the panel's state line) ---
	UPROPERTY(Replicated) bool bWorldTested = false;
	UPROPERTY(Replicated) bool bAIFrozen = false;
	UPROPERTY(Replicated) bool bPublicWorld = false;

	UFUNCTION() void OnRep_Flags();

	/** A tester with this power on. */
	bool Has(uint16 Flag) const { return bTester && (Flags & Flag) != 0; }
	/** Anything on that the badge shows (a power, or the world's AI frozen). */
	bool AnyOn() const { return bTester && (Flags != 0 || bAIFrozen); }

	/** Owning client only: the View page's hitboxes and aggro lines. */
	bool bShowHitboxes = false;
	/** The Travel page's list: the client's copy (ClientGodPlaces) / the server's copy of what it sent (Go picks by index). */
	TArray<FWRGodPlace> Places;
	/** Bumped each time the client gets a new list (the page rebuilds). */
	int32 PlacesSerial = 0;

	// --- Server only ---
	/** The ?WRKey this connection joined with (the proof's bind is a hash of it). */
	FString JoinKey;
	/** The nonce sent to this connection (empty once a proof used it). */
	FString Nonce;
	int32 ProofTries = 0;

	/** Flying / noclip on the pawn of this machine, as the flags say (server, owner and other machines alike). */
	void ApplyMovement();

private:
	TWeakObjectPtr<ACharacter> AppliedTo;
	uint16 AppliedFlags = 0;
	float ServerClock = 0.f;
	void ServerTick();
	void DrawHitboxes() const;
};

/** The world's side (server): AI frozen, and the tested marks (Saved/Worlds/<world>/tested.json). */
UCLASS()
class WILDREACH_API UWRGodWorld : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	static UWRGodWorld* Get(const UObject* WorldContext);

	bool bAIFrozen = false;
	bool IsWorldTested() const;
	bool IsCharacterTested(const FString& Name) const;
	/** Mark the character (and the world, for a world change); written at once. */
	void Mark(const FString& Character, const FString& By, bool bWorld);
	/** Push the world's state to every tester's component (the owner-only lines). */
	void SyncComponents();

private:
	TSharedPtr<FJsonObject> Marks;
	void Load();
	void Save() const;
};

namespace WRGod
{
	/** God components and their powers, by a pawn / PlayerState / controller (false / 0 for creatures). */
	WILDREACH_API bool IsInvulnerable(const AActor* Who);
	WILDREACH_API bool OneHitKill(const AActor* Who);
	WILDREACH_API bool IsUnseen(const AActor* Who);
	WILDREACH_API bool IsFlying(const AActor* Who);
	/** God flight's top speed in cm/s (0: not god-flying). Both machines agree (replicated flags). */
	WILDREACH_API float FlySpeed(const AActor* Who);
	/** Server: every creature's and boss's brain stopped. */
	WILDREACH_API bool IsAIFrozen(const UWorld* World);
	/** The local player is a verified tester (the panel, its key, the pad stop, the badge ask this first). */
	WILDREACH_API bool IsLocalTester(const APlayerController* PC);
	/** This machine's first local player is a verified tester (the controls pages' tester-only key rows). */
	WILDREACH_API bool IsThisMachineTester();
	/** A character marked tested (server: from the marks; anywhere: the replicated flag). */
	WILDREACH_API bool IsTestedCharacter(const AActor* Any);
	/** Server: a character of this name is marked tested on this world. */
	WILDREACH_API bool IsTestedName(const UWorld* World, const FString& Name);
	/** A dedicated server on a public preset (no god or dev commands there). */
	WILDREACH_API bool IsPublicWorld(const UWorld* World);

	/** One of the old dev commands (tp, give, spawn... and any "<system> dev ..."). */
	WILDREACH_API bool IsDevCommand(const TArray<FString>& Args);
	/** Server: why this player may not run a god / dev command here (empty: they may). */
	WILDREACH_API FString Block(const AWRPlayerController* PC);

	/** Server, at login: the connection's nonce, sent to its client. */
	WILDREACH_API void Challenge(AWRPlayerController* PC);
	/** Owning client: the server's nonce came; a tester's game fetches a proof for it. */
	WILDREACH_API void OnChallenge(AWRPlayerController* PC, const FString& Nonce);
	/** Server: a proof came; checked, and the player becomes a tester for this connection. */
	WILDREACH_API void ReceiveProof(AWRPlayerController* PC, const FString& Proof);
	/** Server: "god <verb> ..." (every one re-checks the tester flag). */
	WILDREACH_API void Run(AWRPlayerController* PC, const TArray<FString>& Args);
	/** Owning client: the Travel list arrived. */
	WILDREACH_API void ReceivePlaces(AWRPlayerController* PC, const TArray<FWRGodPlace>& Places);

	/** The powers on, as the badge words them ("INVULNERABLE", "FLY"...). */
	WILDREACH_API TArray<FString> ActiveWords(const UWRGodComponent* God);
	/** The F8 report's context block (null when there's nothing to say). */
	WILDREACH_API TSharedPtr<FJsonObject> ReportContext(const APlayerController* PC);
	/** Owning client: show / hide the badge as the flags say (called every frame by the player's UI). */
	WILDREACH_API void TickBadge(APlayerController* PC);
}
