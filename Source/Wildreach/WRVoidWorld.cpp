// The Corrupted wasteland's world things (see WRVoidWorld.h): its gathering, keepsakes, thrown things and Maren's deeds.
#include "WRVoidWorld.h"
#include "WRVoidUnmade.h"
#include "WRVoidHazards.h"
#include "WRVoidBosses.h"
#include "Wildreach.h"
#include "WRBuildLook.h"
#include "WRBuildLookKit.h"
#include "WRCharacter.h"
#include "WRCharacterBase.h"
#include "WRFx.h"
#include "WRInventory.h"
#include "WRLand.h"
#include "WRLandSites.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "WRVoid.h"
#include "WRVoidCreatures.h"
#include "WRWorldSystems.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace WRVoidWorldImpl
{
	using WRLookKit::FSurf;
	using WRLookKit::FKit;

	double VwServerNow(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.0);
	}
	float VwNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, float F)
	{
		double V = F;
		return O.IsValid() && O->TryGetNumberField(K, V) ? float(V) : F;
	}
	FString VwStr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(K, V);
		return V;
	}

	/** A keepsake: a lore item lying at a story site (wasteland.json pois "lore"). */
	struct FVwKeepsake
	{
		FName Land;
		FName Item;
		FString Site;
		FVector World = FVector::ZeroVector;
	};
	TArray<FVwKeepsake>& VwKeepsakes()
	{
		static TArray<FVwKeepsake> L;
		return L;
	}

	/** The rifts a player's thrown stones have floated in (Count the stones: 3 different ones). */
	TMap<FString, TSet<FName>>& VwFloated()
	{
		static TMap<FString, TSet<FName>> M;
		return M;
	}

	/** Where a keepsake lies: beside its site's middle (the site's own build stands on the middle), on the ground. */
	FVector VwKeepsakeSpot(const FWRLandDef& D, const TSharedPtr<FJsonObject>& Poi)
	{
		const FVector2D At = WRLand::Vec2(Poi->TryGetField(TEXT("at")));
		FVector2D Off(3.5f, 2.5f);
		const TArray<TSharedPtr<FJsonValue>>* O = nullptr;
		if (Poi->TryGetArrayField(TEXT("loreAt"), O) && O->Num() >= 2) Off = FVector2D((*O)[0]->AsNumber(), (*O)[1]->AsNumber());
		return D.GroundPoint(At + Off);
	}

	/** A stone of rough facets, for the float stone and the thrown one: a lathe along Z with a jittered profile. */
	void VwStone(FWRToolMesh& M, float Size, float Seed, const FLinearColor& C)
	{
		M.Lathe(FVector(0.f, 0.f, -Size * 0.42f), FVector::UpVector, Size * 0.84f,
			{ FVector(0.f, Size * 0.18f, Size * 0.16f), FVector(0.22f, Size * 0.5f, Size * 0.44f), FVector(0.55f, Size * 0.55f, Size * 0.48f), FVector(0.82f, Size * 0.4f, Size * 0.36f), FVector(1.f, Size * 0.12f, Size * 0.1f) },
			7, C, 0.92f, 0.15f, Seed);
	}

	AWRRift* VwRiftAt(UWorld* World, const FVector& At, float& OutShare)
	{
		AWRRift* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (TActorIterator<AWRRift> It(World); It; ++It)
		{
			const float Dd = FVector::Dist2D(It->GetActorLocation(), At);
			if (Dd < It->ZoneRadius && FMath::Abs(It->GetActorLocation().Z - At.Z) < 3000.f && Dd < BestD) { BestD = Dd; Best = *It; }
		}
		OutShare = Best ? BestD / FMath::Max(1.f, Best->ZoneRadius) : 1.f;
		return Best;
	}

	AWRResourceNode* VwSpawnNode(UWorld* World, FName Item, const FVector& At, float Yaw, const TSharedPtr<FJsonObject>& Row)
	{
		const FTransform T(FRotator(0.f, Yaw, 0.f), At);
		AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!N) return nullptr;
		N->ItemId = Item;
		N->Hits = FMath::Max(1, int32(VwNum(Row, TEXT("hits"), 4.f)));
		N->PerHit = FMath::Max(1, int32(VwNum(Row, TEXT("perHit"), 1.f)));
		N->RespawnSeconds = VwNum(Row, TEXT("respawn"), 900.f);
		N->GemChance = 0.f;
		N->Tags.Add(TEXT("void_land"));
		N->FinishSpawning(T);
		return N;
	}

	/** Server: the land's gathering, from its "resources" rows. */
	int32 VwSpawnResources(AWRLand* Land)
	{
		const FWRLandDef& D = *Land->Def();
		const TSharedPtr<FJsonObject> Res = D.Section(TEXT("resources"));
		UWorld* World = Land->GetWorld();
		if (!Res.IsValid() || !World) return 0;
		TArray<FVector2D> Places;
		const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr;
		if (D.Json->TryGetArrayField(TEXT("pois"), Pois)) for (const TSharedPtr<FJsonValue>& V : *Pois) Places.Add(WRLand::Vec2(V->AsObject()->TryGetField(TEXT("at"))));
		for (const WRLandSites::FKitPoi& K : WRLandSites::KitPois()) if (K.Land == D.Id) Places.Add(K.At);
		int32 Count = 0;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Res->Values)
		{
			if (KV.Key.StartsWith(TEXT("_"))) continue;
			const TSharedPtr<FJsonObject> Row = KV.Value->AsObject();
			if (!Row.IsValid()) continue;
			const FName Item(*VwStr(Row, TEXT("item")));
			const uint32 Salt = GetTypeHash(KV.Key);
			// Fixed points (the Quire's mandrakes, the meteor bowl).
			const TArray<TSharedPtr<FJsonValue>>* AtList = nullptr;
			if (Row->TryGetArrayField(TEXT("at"), AtList))
			{
				int32 i = 0;
				for (const TSharedPtr<FJsonValue>& A : *AtList) { if (VwSpawnNode(World, Item, D.GroundPoint(WRLand::Vec2(A)), 47.f * i++, Row)) ++Count; }
				continue;
			}
			// A disc (the Glass Sea's voidglass): a sunflower spread, so they never bunch.
			const TArray<TSharedPtr<FJsonValue>>* Disc = nullptr;
			if (Row->TryGetArrayField(TEXT("disc"), Disc) && Disc->Num() >= 2)
			{
				const FVector2D C((*Disc)[0]->AsNumber(), (*Disc)[1]->AsNumber());
				const float R = VwNum(Row, TEXT("r"), 100.f);
				const int32 N = int32(VwNum(Row, TEXT("count"), 10.f));
				for (int32 i = 0; i < N; ++i)
				{
					const float A = i * 2.39996f, Rr = R * FMath::Sqrt((i + 0.5f) / N);
					if (VwSpawnNode(World, Item, D.GroundPoint(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr), 61.f * i, Row)) ++Count;
				}
				continue;
			}
			// On every kit POI of a kind (the Grey Strand's salt pans: four crusts round the pan).
			const FString Kit = VwStr(Row, TEXT("kit"));
			if (!Kit.IsEmpty())
			{
				const int32 Per = int32(VwNum(Row, TEXT("per"), 1.f));
				const float Spread = VwNum(Row, TEXT("spread"), 8.f);
				for (const WRLandSites::FKitPoi& K : WRLandSites::KitPois())
				{
					if (K.Land != D.Id || K.Kind != FName(*Kit)) continue;
					for (int32 p = 0; p < Per; ++p)
					{
						const float A = 2.f * PI * (p + 0.5f) / Per + 0.4f;
						if (VwSpawnNode(World, Item, D.GroundPoint(K.At + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Spread), 90.f * p, Row)) ++Count;
					}
				}
				continue;
			}
			// A zone pass: a seeded grid, kept to density and clear of the places; greywood in the forest's stands,
			// ashen grain in its clearings (the stand noise the decoration pass grows its pines from).
			const FName ZoneId(*VwStr(Row, TEXT("zone")));
			const float Step = VwNum(Row, TEXT("step"), 50.f), Density = VwNum(Row, TEXT("density"), 0.3f), Gap = VwNum(Row, TEXT("gap"), 30.f);
			bool bClearing = false;
			Row->TryGetBoolField(TEXT("clearing"), bClearing);
			const FWRLandZone* Zone = D.Zones.FindByPredicate([&](const FWRLandZone& Z) { return Z.Id == ZoneId; });
			if (!Zone) continue;
			for (float Y = D.MapBox.Min.Y; Y < D.MapBox.Max.Y; Y += Step)
			{
				for (float X = D.MapBox.Min.X; X < D.MapBox.Max.X; X += Step)
				{
					const FVector2D Cell(X, Y);
					const FVector2D M = Cell + FVector2D(D.Noise(Cell * 0.613f + FVector2D(Salt % 97, 7.f)), D.Noise(Cell * 0.587f + FVector2D(3.f, Salt % 89))) * Step * 0.4f;
					if (D.ZoneAt(M) != Zone || D.CoastDistance(M) < 15.f || D.HoleAt(M, 30.f)) continue;
					const float Stand = (D.Noise(M / 90.f + FVector2D(1.f, 1.f)) + 1.f) * 0.5f;
					if (bClearing ? Stand > 0.38f : Stand < 0.5f) continue;
					if ((D.Noise(M * 1.91f + FVector2D(Salt % 13, 5.f)) + 1.f) * 0.5f > Density) continue;
					bool bNear = false;
					for (const FVector2D& P : Places) if (FVector2D::DistSquared(P, M) < Gap * Gap) { bNear = true; break; }
					if (bNear) continue;
					if (D.Height(M + FVector2D(2.f, 0.f)) - D.Height(M - FVector2D(2.f, 0.f)) > 3.f) continue;   // not on a steep bank
					if (VwSpawnNode(World, Item, D.GroundPoint(M), 360.f * (D.Noise(M) * 0.5f + 0.5f), Row)) ++Count;
				}
			}
		}
		return Count;
	}

	/** Every machine: each keepsake's look (a small thing set down where the story left it, with a faint glint). */
	void VwBuildKeepsakes(AWRLand* Land)
	{
		const FWRLandDef& D = *Land->Def();
		const bool bDraw = Land->GetNetMode() != NM_DedicatedServer;
		const TArray<TSharedPtr<FJsonValue>>* Pois = nullptr;
		if (!D.Json->TryGetArrayField(TEXT("pois"), Pois)) return;
		TArray<FWRLookBlock> Blocks;
		const FTransform Frame(Land->GetActorLocation());
		FKit K(Frame, Blocks);
		const FSurf Cloth = FSurf::Cloth(FLinearColor(FColor(150, 142, 128)));
		const FSurf Bone = FSurf::Stone(FLinearColor(FColor(217, 207, 184)), 0.8f);
		const FSurf Slate = FSurf::Stone(FLinearColor(FColor(62, 60, 64)), 0.7f);
		for (const TSharedPtr<FJsonValue>& V : *Pois)
		{
			const TSharedPtr<FJsonObject> P = V->AsObject();
			const FString Lore = VwStr(P, TEXT("lore"));
			if (Lore.IsEmpty()) continue;
			FVwKeepsake& S = VwKeepsakes().AddDefaulted_GetRef();
			S.Land = D.Id;
			S.Item = FName(*Lore);
			S.Site = VwStr(P, TEXT("name"));
			S.World = VwKeepsakeSpot(D, P);
			if (!bDraw) continue;
			const FVector L = S.World - Land->GetActorLocation();
			// A flat stone set down like a table, a cloth folded on it, the thing itself on the cloth.
			K.Add(Slate, FVector(56.f, 44.f, 10.f), L + FVector(0, 0, 5.f), FRotator(0.f, 23.f, 0.f));
			K.Add(Cloth, FVector(34.f, 26.f, 2.f), L + FVector(0, 0, 11.f), FRotator(0.f, 31.f, 0.f));
			K.Add(Bone, FVector(14.f, 8.f, 6.f), L + FVector(0, 0, 15.f), FRotator(0.f, 61.f, 0.f), EWRLookMesh::Cylinder);
			if (USceneComponent* Fx = WRFx::Attach(Land->GetRootComponent(), TEXT("void.keepsake"), NAME_None, 1.f))
			{
				Fx->SetUsingAbsoluteLocation(true);
				Fx->SetWorldLocation(S.World + FVector(0, 0, 22.f));
			}
		}
		if (bDraw && Blocks.Num()) FWRBuildLook::Spawn(Land, Land->GetRootComponent(), Blocks, FLinearColor(FColor(150, 142, 128)), false);
	}

	const FVwKeepsake* VwKeepsakeNear(const FVector& At, float Range)
	{
		const FVwKeepsake* Best = nullptr;
		float BestD = Range;
		for (const FVwKeepsake& S : VwKeepsakes())
		{
			const float Dd = FVector::Dist(S.World, At);
			if (Dd < BestD) { BestD = Dd; Best = &S; }
		}
		return Best;
	}
}

using namespace WRVoidWorldImpl;

// ================================================================================================ the land

void WRVoidWorld::LandReady(AWRLand* Land)
{
	if (!Land || !Land->Def() || Land->Def()->Biome != TEXT("wasteland")) return;
	VwBuildKeepsakes(Land);
	if (Land->GetNetMode() == NM_Client) return;
	const int32 N = VwSpawnResources(Land);
	// Its dens (WRVoidCreatures): day and night tables per zone, the Riftmaws in the large rifts, the Hush on the road.
	UWRVoidLife::PlaceDens(Land);
	// Its bosses' arenas, the Null Gate and Vorhollow's stage (WRVoidBosses).
	FWRVoidBoss::PlaceArenas(Land);
	// Its litter fire, hollow crust and the Rift Storm (WRVoidHazards).
	AWRVoidState::Spawn(Land);
	// The Unmade Place in the Great Rift, its stair and its lift (WRVoidUnmade).
	AWRUnmadePlace::Spawn(Land);
	UE_LOG(LogWildreach, Log, TEXT("VOID %s: %d resource nodes, %d keepsakes"), *Land->LandId.ToString(), N, VwKeepsakes().Num());
}

FWRVoidSalted& WRVoidWorld::OnSalted()
{
	static FWRVoidSalted D;
	return D;
}

void WRVoidWorld::QuestEvent(AActor* Who, FName Event, int32 Count)
{
	const APawn* P = Cast<APawn>(Who);
	AWRPlayerState* PS = P ? P->GetPlayerState<AWRPlayerState>() : Cast<AWRPlayerState>(Who);
	if (!PS) return;
	if (UWRWorldSystems* WS = UWRWorldSystems::Get(PS)) WS->QuestEvent(PS, Event, NAME_None, Count);
}

// ================================================================================================ gathering

void WRVoidWorld::Harvested(AWRResourceNode* Node, AWRCharacterBase* Gatherer, bool bLastHit)
{
	if (!Node || !Gatherer || !Node->HasAuthority()) return;
	if (Node->ItemId == TEXT("voidglass") && Node->Tags.Contains(TEXT("rift_ring")))
	{
		QuestEvent(Gatherer, TEXT("voidglass_ring"), FMath::Max(1, Node->PerHit));
	}
	if (Node->ItemId == TEXT("mandrake") && bLastHit)
	{
		// The pull: the root shivers in your hands for 0.9 s (dust off the leaves, a thin rising whine), then screams.
		const TSharedPtr<FJsonObject> Row = [&]() { const FWRLandDef* D = WRLand::At(Node->GetActorLocation()); return D ? D->Section(TEXT("mandrake")) : TSharedPtr<FJsonObject>(); }();
		const float Shiver = VwNum(Row, TEXT("shiver"), 0.9f), Radius = VwNum(Row, TEXT("radius"), 12.f) * 100.f, Stagger = VwNum(Row, TEXT("stagger"), 0.8f);
		const FVector At = Node->GetActorLocation() + FVector(0, 0, 40.f);
		WRFx::Broadcast(Node, TEXT("void.mandrake.shiver"), At, FRotator::ZeroRotator, 1.f, TEXT("void.mandrake.shiver"));
		TWeakObjectPtr<UWorld> WeakWorld(Node->GetWorld());
		FTimerHandle H;
		Node->GetWorldTimerManager().SetTimer(H, [WeakWorld, At, Radius, Stagger]()
		{
			UWorld* W = WeakWorld.Get();
			if (!W) return;
			WRFx::Broadcast(W, TEXT("void.mandrake.scream"), At, FRotator::ZeroRotator, 1.f, TEXT("void.mandrake.scream"));
			for (TActorIterator<AWRCharacter> It(W); It; ++It)
			{
				if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), At) > Radius) continue;
				const UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(*It);
				if (Inv && Inv->CountOf(TEXT("wax_earplugs")) > 0) continue;
				if (It->Status) It->Status->Stagger(Stagger);
				if (AWRPlayerController* PC = Cast<AWRPlayerController>(It->GetController())) PC->ClientNotify(TEXT("The mandrake SCREAMS (wax ear plugs keep it out)"));
			}
		}, Shiver, false);
	}
}

// ================================================================================================ thrown things

bool WRVoidWorld::IsThrownKind(FName Kind)
{
	return Kind == TEXT("riftstone") || Kind == TEXT("anchorsalt");
}

bool WRVoidWorld::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh)
{
	if (!Mesh) return false;
	FWRToolMesh M;
	if (ModelPath == TEXT("code:riftstone"))
	{
		// A fist-sized grey stone, flat-faceted.
		VwStone(M, 9.f, 3.f, FLinearColor(FColor(110, 106, 100)));
	}
	else if (ModelPath == TEXT("code:anchorsalt"))
	{
		// A grey linen bag of salt along +X, 12 cm: a round belly, its neck tied with a black cord, salt grains at the mouth.
		const FLinearColor Linen(0.36f, 0.34f, 0.3f), Cord(0.04f, 0.035f, 0.04f), SaltW(0.74f, 0.73f, 0.7f);
		M.Lathe(FVector(-6.f, 0.f, 0.f), FVector::XAxisVector, 9.f, { FVector(0.f, 1.5f, 1.5f), FVector(0.15f, 4.4f, 4.1f), FVector(0.5f, 5.1f, 4.8f), FVector(0.8f, 3.6f, 3.4f), FVector(1.f, 1.4f, 1.4f) }, 12, Linen, 0.95f, 0.f, 5.f);
		M.Lathe(FVector(3.f, 0.f, 0.f), FVector::XAxisVector, 3.f, { FVector(0.f, 1.4f, 1.4f), FVector(0.5f, 2.3f, 1.7f), FVector(1.f, 2.7f, 2.1f) }, 8, Linen, 0.95f, 0.f, 6.f);
		TArray<FVector> Tie;
		for (int32 i = 0; i < 9; ++i) { const float A = 2.f * PI * i / 9.f; Tie.Add(FVector(3.2f, FMath::Cos(A) * 1.6f, FMath::Sin(A) * 1.6f)); }
		M.Tube(Tie, 0.35f, 5, Cord, 0.9f, true);
		M.Lathe(FVector(5.4f, 0.f, 0.f), FVector::XAxisVector, 0.8f, { FVector(0.f, 1.9f, 1.5f), FVector(1.f, 0.6f, 0.5f) }, 8, SaltW, 0.95f);
	}
	else
	{
		return false;
	}
	M.ApplyTo(Mesh);
	return true;
}

void WRVoidWorld::ThrownLanded(AActor* Projectile, FName Kind, const FVector& At, AActor* HitActor, AWRCharacterBase* By)
{
	UWorld* World = Projectile ? Projectile->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client) return;
	if (Kind == TEXT("anchorsalt"))
	{
		// A 3 m cloud of grey salt: whatever answers anchor salt within it (brood rifts, rift hearts, island pieces).
		const float Radius = WRTune(TEXT("voidWorld.anchorSalt.radius"), 300.f);
		WRFx::Broadcast(World, TEXT("void.salt.burst"), At, FRotator::ZeroRotator, 1.f, TEXT("void.salt.burst"));
		OnSalted().Broadcast(At, Radius, By);
		UE_LOG(LogWildreach, Log, TEXT("VOID anchor salt burst at %s by %s"), *At.ToCompactString(), *GetNameSafe(By));
		return;
	}
	if (Kind == TEXT("riftstone"))
	{
		WRSound::PlayAt(World, TEXT("void.stone.land"), At);
		// On hollow crust: it cracks through and shows the drop.
		AWRVoidState::CrustStruck(World, At, 120.f);
		float Share = 1.f;
		AWRRift* Rift = VwRiftAt(World, At, Share);
		if (!Rift) return;
		// In a zone: it lifts off the ground and hangs, bobbing (the floating rule). Higher the nearer the tear.
		const FVector Ground(At.X, At.Y, WRLand::GroundZAt(At, At.Z));
		const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Ground);
		if (AWRVoidFloatStone* S = World->SpawnActorDeferred<AWRVoidFloatStone>(AWRVoidFloatStone::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			S->LandedAt = VwServerNow(World);
			S->Ground = Ground;
			S->Rise = FMath::Lerp(WRTune(TEXT("voidWorld.floatStone.riseNear"), 40.f), WRTune(TEXT("voidWorld.floatStone.riseFar"), 12.f), Share);
			S->SetLifeSpan(WRTune(TEXT("voidWorld.floatStone.seconds"), 30.f));
			S->FinishSpawning(T);
		}
		// Maren's first quest: three different rifts.
		const APawn* Thrower = Cast<APawn>(By);
		if (Thrower && Thrower->GetPlayerState())
		{
			TSet<FName>& Seen = VwFloated().FindOrAdd(Thrower->GetPlayerState()->GetPlayerName());
			if (!Seen.Contains(Rift->RiftId))
			{
				Seen.Add(Rift->RiftId);
				QuestEvent(By, TEXT("stone_floated"));
			}
		}
	}
}

// ================================================================================================ keepsakes

bool WRVoidWorld::Prompt(const AWRPlayerController* PC, FString& OutTarget, FString& OutVerb)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me) return false;
	const FVwKeepsake* S = VwKeepsakeNear(Me->GetActorLocation(), WRTune(TEXT("voidWorld.keepsakeRange"), 260.f));
	if (!S) return false;
	OutTarget = S->Site;
	OutVerb = FString::Printf(TEXT("Take: %s"), *WRWorld::ItemName(S->Item));
	return true;
}

bool WRVoidWorld::ClientInteract(AWRPlayerController* PC)
{
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Me || !VwKeepsakeNear(Me->GetActorLocation(), WRTune(TEXT("voidWorld.keepsakeRange"), 260.f))) return false;
	PC->SendCommand(TEXT("land keep"));
	return true;
}

FString WRVoidWorld::Command(AWRPlayerController* PC, const TArray<FString>& Args)
{
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FString Verb = Args.IsValidIndex(1) ? Args[1] : FString();
	if (!Me || Verb != TEXT("keep")) return FString();
	// A little slack over the prompt's reach: the client asked from where it stood a moment ago.
	const FVwKeepsake* S = VwKeepsakeNear(Me->GetActorLocation(), WRTune(TEXT("voidWorld.keepsakeRange"), 260.f) + 150.f);
	if (!S) return TEXT("Nothing to take here.");
	UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Me);
	if (!Inv) return FString();
	if (Inv->CountOf(S->Item) > 0) return FString::Printf(TEXT("You already carry the %s."), *WRWorld::ItemName(S->Item));
	if (Inv->AddItem(S->Item, 1) > 0) return TEXT("Your bag is full.");   // AddItem returns what did not fit
	WRSound::PlayAt(Me, TEXT("void.keepsake"), S->World);
	UE_LOG(LogWildreach, Log, TEXT("VOID %s took keepsake %s at %s"), *Me->GetName(), *S->Item.ToString(), *S->Site);
	return FString::Printf(TEXT("You take the %s."), *WRWorld::ItemName(S->Item));
}

// ================================================================================================ the floating stone

AWRVoidFloatStone::AWRVoidFloatStone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Model = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Stone"));
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Model;
	SetNetCullDistanceSquared(FMath::Square(9000.f));
}

void AWRVoidFloatStone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRVoidFloatStone, LandedAt);
	DOREPLIFETIME(AWRVoidFloatStone, Ground);
	DOREPLIFETIME(AWRVoidFloatStone, Rise);
}

void AWRVoidFloatStone::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		FWRToolMesh M;
		VwStone(M, 9.f, 3.f, FLinearColor(FColor(110, 106, 100)));
		M.ApplyTo(Model);
		WRFx::Attach(Model, TEXT("void.stone.float"), NAME_None, 1.f);
	}
}

double AWRVoidFloatStone::Now() const
{
	return VwServerNow(GetWorld());
}

void AWRVoidFloatStone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Lifts over 1.5 s (eased), then bobs 4 cm on a 3 s period and turns slowly: all from the clock since it landed.
	const float T = float(Now() - LandedAt);
	const float Lift = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(T / 1.5f, 0.f, 1.f), 2.f);
	const float Bob = 4.f * FMath::Sin(2.f * PI * T / 3.f) * Lift;
	SetActorLocationAndRotation(FVector(Ground) + FVector(0, 0, 6.f + Rise * Lift + Bob), FRotator(6.f * FMath::Sin(T * 0.7f), T * 9.f, 4.f * FMath::Cos(T * 0.5f)));
}
