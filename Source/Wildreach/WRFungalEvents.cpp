// The Fungal caverns' live half, part 2 (cooks/biome-fungal.md 2.8-2.11, 2.21; cooks/build-biome-fungal.md 2.8-2.11): the
// hazard machines placed from the data (clouds, lure lines, springcaps, cap-falls, the Breath vents, the doors and secrets),
// foul air, glow paste's light, the Spore bloom's beat, Spore drift on the surface, the Sporefall (the gold Breath, the
// flush, bloomed beasts, deepcaps on the surface, the Seed Vault, the vents' faster breath), the invasions' marks, Ilmari
// Kask's day and night, and Ilmari's Cold light walk through the Lure Gallery.

#include "WRFungalWorld.h"
#include "WRFungalHazards.h"
#include "WRFungalCaves.h"
#include "WRFungalLook.h"
#include "WRFungalTypes.h"
#include "Wildreach.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRGameState.h"
#include "WRLand.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRStormTypes.h"
#include "WRSurvival.h"
#include "WRTuning.h"
#include "WRToolMesh.h"
#include "WRWorldSystems.h"
#include "AbilitySystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"

namespace WRFungalEvImpl
{
	float FeT(const TCHAR* Key, float Def) { return WRTune(Key, Def); }
	void FeTag(AWRCharacterBase* C, const FGameplayTag& Tag, bool bOn)
	{
		if (UAbilitySystemComponent* ASC = C ? C->GetAbilitySystemComponent() : nullptr) ASC->SetLooseGameplayTagCount(Tag, bOn ? 1 : 0, EGameplayTagReplicationState::TagOnly);
	}
	/** The Sky islands the Sporefall seeds (fungal 2.10): their tops from Data/sky.json. */
	TArray<FVector> SeededIslands()
	{
		TArray<FVector> Out;
		const TSharedPtr<FJsonObject> J = FWRTuning::LoadDataFile(TEXT("sky.json"));
		const TArray<TSharedPtr<FJsonValue>>* Isl = nullptr;
		if (!J.IsValid() || !J->TryGetArrayField(TEXT("islands"), Isl)) return Out;
		for (const TSharedPtr<FJsonValue>& V : *Isl)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FString Id;
			if (!O->TryGetStringField(TEXT("id"), Id) || !(Id == TEXT("spore_scar") || Id == TEXT("wheel_isle") || Id == TEXT("lid_gate"))) continue;
			Out.Add(FVector(O->GetNumberField(TEXT("x")) * 100.0, O->GetNumberField(TEXT("y")) * 100.0, O->GetNumberField(TEXT("z")) * 100.0));
		}
		return Out;
	}
}

void UWRFungalWorld::SpawnHazards()
{
	using namespace WRFungalEvImpl;
	UWorld* World = GetWorld();
	const FWRFungalData& D = FWRFungalData::Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (World->GetNetMode() != NM_Client)
	{
		State = World->SpawnActor<AWRFungalState>(AWRFungalState::StaticClass(), FTransform::Identity, SP);
		// The places' clouds (fungal 2.8: puffball fields, host clusters, the Breath Vents, the Great Doline's ledges).
		for (const FWRFungalData::FCloud& C : D.Clouds) AWRSporeCloud::Spawn(World, C.At, C.R, TEXT("world"), 0.f, nullptr, 0.f);
		// The lure-line sets, springcaps, cap-fall ceilings.
		for (int32 i = 0; i < D.Lines.Num(); ++i)
		{
			const FTransform Tr(FRotator::ZeroRotator, D.Lines[i].At);
			if (AWRLureLines* L = World->SpawnActorDeferred<AWRLureLines>(AWRLureLines::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)) { L->SetIndex = i; L->FinishSpawning(Tr); }
		}
		for (const FWRFungalData::FSpringcap& S : D.Springcaps)
		{
			const FTransform Tr(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), S.At - FVector(0.f, 0.f, 10.f));
			World->SpawnActor<AWRSpringcap>(AWRSpringcap::StaticClass(), Tr, SP);
		}
		for (const FWRFungalData::FCapFall& F : D.CapFalls)
		{
			const FTransform Tr(FRotator::ZeroRotator, F.At);
			if (AWRCapFall* A = World->SpawnActorDeferred<AWRCapFall>(AWRCapFall::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)) { A->Roof = F.Roof; A->FinishSpawning(Tr); }
		}
		// The doors and secrets (fungal 2.5, 2.20): each at its passage's mouth, facing along it.
		const auto Spot = [&](FName Kind, const FVector& At, float Yaw, float Width, float Height, const FString& Title, const FString& Verb)
		{
			const FTransform Tr(FRotator(0.f, Yaw, 0.f), At);
			AWRFungalSpot* S = World->SpawnActorDeferred<AWRFungalSpot>(AWRFungalSpot::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!S) return;
			S->Kind = Kind;
			S->Width = Width;
			S->HeightCm = Height;
			S->Title = Title;
			S->Verb = Verb;
			S->Reach = FMath::Max(450.f, Width * 0.7f);
			S->FinishSpawning(Tr);
		};
		const auto TubeMouth = [&](FName Tube, bool bEnd, float Inset, FVector& OutAt, float& OutYaw) -> bool
		{
			const FWRFungalData::FTube* T = D.FindTube(Tube);
			if (!T || T->Pts.Num() < 4) return false;
			const int32 N = T->Pts.Num();
			const int32 I = bEnd ? FMath::Max(0, N - 1 - int32(Inset / 300.f)) : FMath::Min(N - 1, int32(Inset / 300.f));
			const int32 J = bEnd ? FMath::Max(0, I - 1) : FMath::Min(N - 1, I + 1);
			OutAt = T->Pts[I];
			OutYaw = (T->Pts[J] - T->Pts[I]).Rotation().Yaw;
			return true;
		};
		FVector At; float Yw = 0.f;
		if (TubeMouth(TEXT("W5-HG1"), false, 1200.f, At, Yw)) Spot(TEXT("sealed_gate"), At, Yw, 1200.f, 1200.f, TEXT("The Sealed Gate"), TEXT("Burn the web"));
		if (TubeMouth(TEXT("mole"), false, 300.f, At, Yw)) Spot(TEXT("mole_wall"), At, Yw, 520.f, 300.f, TEXT("A collapsed crawl"), TEXT("Look"));
		if (TubeMouth(TEXT("H3-H7"), false, 600.f, At, Yw)) Spot(TEXT("fifth_tunnel"), At, Yw, 520.f, 340.f, TEXT("A blank wall at the junction"), TEXT("Knock"));
		if (TubeMouth(TEXT("HG9-R8"), false, 900.f, At, Yw)) Spot(TEXT("heart_gate"), At, Yw + 180.f, 1200.f, 1200.f, TEXT("The Heart Gate"), TEXT("Roll it aside"));
		if (const FWRFungalData::FHall* V = D.FindHall(TEXT("HG7"))) { const FWRFungalData::FTube* T = D.FindTube(TEXT("HG6-HG7")); FVector A = V->At; float Y = 0.f; if (T && T->Pts.Num() > 2) { A = T->Pts.Last(); Y = (T->Pts.Last() - T->Pts[T->Pts.Num() - 3]).Rotation().Yaw; } Spot(TEXT("seed_vault"), A, Y, 900.f, 800.f, TEXT("The Seed Vault"), TEXT("Look")); }
		if (!D.RingAt.IsZero())
		{
			FVector A = D.RingAt + FVector(-200.f, 0.f, 0.f);
			float Z = 0.f;
			if (WRLand::GroundUnder(A + FVector(0.f, 0.f, 3000.f), Z)) A.Z = Z + 60.f;
			Spot(TEXT("ring_altar"), A, 0.f, 320.f, 60.f, TEXT("The altar of the Ring of Caps"), TEXT("Stand at it"));
		}
	}
	// The Breath vents: every machine draws its own flap from the clock (the server's makes the plumes).
	for (int32 i = 0; i < D.Vents.Num(); ++i)
	{
		const FTransform Tr(FRotator(0.f, 120.f * i + 90.f, 0.f), D.Vents[i]);
		if (AWRBreathVent* V = World->SpawnActorDeferred<AWRBreathVent>(AWRBreathVent::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)) { V->VentIndex = i; V->FinishSpawning(Tr); }
	}
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL hazards: %d clouds, %d line sets, %d springcaps, %d cap-falls, %d vents"), D.Clouds.Num(), D.Lines.Num(), D.Springcaps.Num(), D.CapFalls.Num(), D.Vents.Num());
}

void UWRFungalWorld::GalleryWalkSpoiled(const FString& Player)
{
	GallerySpoiled.Add(Player);
}

void UWRFungalWorld::StepHazards(float Dt)
{
	using namespace WRFungalEvImpl;
	UWorld* World = GetWorld();
	HazardClock -= Dt;
	if (HazardClock > 0.f) return;
	const float Step = 0.5f;
	HazardClock = Step;
	const FWRFungalData& D = FWRFungalData::Get();
	const float Now = WRFungal::Now(World);
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (!C->IsAlive() || !C->GetPlayerState()) continue;
		const FVector P = C->GetActorLocation();
		// Foul air (fungal 2.8): heavy breathing, no sprint, stamina regen x0.5; after 20 s, 3 a second until you leave.
		bool bFoul = false;
		for (const FWRFungalData::FFoul& F : D.Foul)
		{
			const FVector L = FRotator(0.f, -F.Yaw, 0.f).RotateVector(P - F.At);
			if (FMath::Square(L.X / F.Rx) + FMath::Square(L.Y / F.Ry) <= 1.f && L.Z > -200.f && L.Z < F.H) { bFoul = true; break; }
		}
		float& Foul = FoulTime.FindOrAdd(C);
		if (bFoul != C->HasTag(WRFungalTags::FoulAir)) FeTag(C, WRFungalTags::FoulAir, bFoul);
		if (bFoul)
		{
			Foul += Step;
			if (Foul > FeT(TEXT("hazards.fungal.foulAir.damageAfter"), 20.f))
			{
				FWRHit H;
				H.Instigator = nullptr;
				H.Damage = FeT(TEXT("hazards.fungal.foulAir.dps"), 3.f) * Step;
				H.Element = WRTags::Element_Physical;
				H.bDamageOverTime = true;
				H.bReaction = true;
				H.Origin = P;
				UWRCombatLibrary::ApplyHit(C, H);
			}
		}
		else Foul = 0.f;
		// Glow paste on blade and helm: the buff's light (a tag every machine draws).
		const bool bPaste = WRStorm::HasBuff(C, TEXT("glow_paste"));
		if (bPaste != C->HasTag(WRFungalTags::GlowPaste)) FeTag(C, WRFungalTags::GlowPaste, bPaste);
		// Ilmari's Cold light: the Lure Gallery end to end without being reeled, carrying cold light.
		if (const FWRFungalData::FHall* G = D.FindHall(TEXT("H2")))
		{
			AWRPlayerState* PS = C->GetPlayerState<AWRPlayerState>();
			const FVector2D L = G->Local(P);
			const bool bIn = G->Contains(P, 300.f);
			const FString Name = PS ? PS->GetPlayerName() : FString();
			if (PS && bIn && FMath::Abs(L.X) > G->Rx * 0.75f)
			{
				const int32 End = L.X < 0.f ? 0 : 1;
				int32* From = GalleryFrom.Find(Name);
				if (!From) { GalleryFrom.Add(Name, End); GallerySpoiled.Remove(Name); }
				else if (*From != End)
				{
					if (!GallerySpoiled.Contains(Name) && WRFungal::CarriesColdLight(C))
					{
						if (UWRWorldSystems* WS = World->GetSubsystem<UWRWorldSystems>()) WS->QuestEvent(PS, TEXT("event"), TEXT("lure_gallery_walk"), 1);
					}
					GalleryFrom.Add(Name, End);
					GallerySpoiled.Remove(Name);
				}
			}
			else if (PS && !bIn) { GalleryFrom.Remove(Name); GallerySpoiled.Remove(Name); }
		}
	}
	// Spore drift (fungal 2.9): while the surface weather is drift and it isn't raining, every doline's mouth breathes a
	// thin cloud that drifts 200 m downwind and fades.
	const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(World, WRFungal::Biome());
	const bool bDrift = K && K->Id == TEXT("sporedrift");
	DriftClock -= Step;
	if (bDrift && DriftClock <= 0.f)
	{
		DriftClock = FeT(TEXT("hazards.fungal.drift.every"), 20.f);
		const FVector Wind = FRotator(0.f, FeT(TEXT("hazards.fungal.drift.windYaw"), 20.f), 0.f).Vector();
		for (const FWRFungalData::FHall& H : D.Halls)
		{
			if (!H.IsDoline()) continue;
			// Only near a survivor (no one sees it otherwise).
			bool bSeen = false;
			for (FConstPlayerControllerIterator Pc = World->GetPlayerControllerIterator(); Pc; ++Pc) if (const APawn* Pw = Pc->Get() ? Pc->Get()->GetPawn() : nullptr) bSeen |= FVector::Dist2D(Pw->GetActorLocation(), H.At) < 40000.f;
			if (!bSeen) continue;
			const FVector Mouth(H.At.X, H.At.Y, H.Top - 400.f);
			if (AWRSporeCloud* Cl = AWRSporeCloud::Spawn(World, Mouth, FeT(TEXT("hazards.fungal.drift.radius"), 1200.f), TEXT("drift"), FeT(TEXT("hazards.fungal.drift.seconds"), 90.f), nullptr))
			{
				Cl->Push(Wind, FeT(TEXT("hazards.fungal.drift.distance"), 20000.f), nullptr);
			}
		}
	}
	(void)Now;
}

void UWRFungalWorld::StepSporefall(float Dt)
{
	using namespace WRFungalEvImpl;
	UWorld* World = GetWorld();
	const AWRWorldState* WS = AWRWorldState::Get(World);
	const FWRWorldEventRep* E = WS ? WS->Event(TEXT("sporefall")) : nullptr;
	const float T = WRFungal::Now(World);
	const uint8 Stage = !E ? 0 : (E->Stage == 0 && T < E->StartAt) ? 1 : (E->Stage == 1 && T < E->EndAt) ? 2 : 0;
	if (World->GetNetMode() != NM_Client)
	{
		AWRFungalState* S = State.Get();
		if (S && S->Sporefall != Stage) { S->Sporefall = Stage; S->SporefallStart = E ? E->StartAt : 0.f; }
		bSporefall = Stage == 2;
		if (Stage == 2) StepFlush(Dt);
		else if (Flush.Num() || FlushNodes.Num())
		{
			// The flush is over: the caps fall back to dust; the surface deepcaps the dead fruited stay till they're taken.
			for (const TWeakObjectPtr<AWRSporeCloud>& C : Flush) if (C.IsValid()) C->Destroy();
			Flush.Reset();
			FlushCells.Reset();
			for (const TWeakObjectPtr<AWRResourceNode>& N : FlushNodes) if (N.IsValid() && N->IsDepleted()) N->Destroy();
			FlushNodes.Reset();
			UE_LOG(LogWildreach, Log, TEXT("FUNGAL the Sporefall is over"));
		}
	}
	// Every machine with a view: the beat. The Sporefall's warning and day beat every glow at 35 a minute (the Heart);
	// the Spore bloom's 30 s tell beats them once a second underground.
	if (World->GetNetMode() == NM_DedicatedServer) return;
	FVector View = FVector::ZeroVector;
	if (const APlayerController* PC = World->GetFirstPlayerController()) { FRotator R; PC->GetPlayerViewPoint(View, R); }
	float Amount = 0.f, Rate = 1.f;
	if (Stage > 0) { Amount = FeT(TEXT("fungalEvents.sporefall.beat"), 0.5f); Rate = 35.f / 60.f; }
	else if (WRFungal::BloomTell(World) && WRFungal::InCave(View) && !WRFungal::InHeartGrotto(View)) { Amount = FeT(TEXT("hazards.fungal.bloom.beat"), 0.6f); Rate = 1.f; }
	BeatAmount = FMath::FInterpTo(BeatAmount, Amount, Dt, 1.f);
	BeatRate = Rate;
	// The Breath turns gold and climbs to 800 m (the warning and the day).
	if (Stage > 0 && !GoldBreath.IsValid() && CavesActor.IsValid())
	{
		const FWRFungalData& D = FWRFungalData::Get();
		const FWRFungalData::FSite* Cap = D.FindSite(TEXT("L1"));
		if (Cap)
		{
			GoldBreath = WRFx::Attach(CavesActor->GetRootComponent(), TEXT("fungal.breathGold"), NAME_None, 4.f);
			if (GoldBreath.IsValid()) GoldBreath->SetWorldLocation(Cap->At + FVector(0.f, 0.f, 9600.f));
		}
	}
	else if (Stage == 0 && GoldBreath.IsValid()) { GoldBreath->DestroyComponent(); GoldBreath = nullptr; }
}

void UWRFungalWorld::StepFlush(float Dt)
{
	using namespace WRFungalEvImpl;
	// The flush (fungal 2.10): fruiting patches burst up across the seeded ground, one per 40 m cell near a survivor, each
	// a cluster of pale caps with a 6 m cloud; deepcap on the surface where something lay dead (a patch's bones: a node);
	// creatures that stand 10 s in a flush cloud bloom (not the caverns' own).
	UWorld* World = GetWorld();
	FlushClock -= Dt;
	if (FlushClock > 0.f) return;
	FlushClock = 2.f;
	const float Cell = FeT(TEXT("fungalEvents.sporefall.patchEvery"), 40.f) * 100.f;
	const int32 Cap = int32(FeT(TEXT("fungalEvents.sporefall.maxPatches"), 70.f));
	static TArray<FVector> Islands;
	if (Islands.Num() == 0) Islands = SeededIslands();
	const auto Seeded = [&](const FVector& P) -> bool
	{
		if (WRLand::BiomeAt(P) == WRFungal::Biome() && !WRFungal::InCave(P + FVector(0.f, 0.f, 100.f))) return true;
		for (const FVector& I : Islands) if (FVector::Dist2D(I, P) < 9000.f && FMath::Abs(P.Z - I.Z) < 6000.f) return true;
		return false;
	};
	for (FConstPlayerControllerIterator Pc = World->GetPlayerControllerIterator(); Pc; ++Pc)
	{
		const APawn* Pw = Pc->Get() ? Pc->Get()->GetPawn() : nullptr;
		if (!Pw || Flush.Num() >= Cap) continue;
		const FVector P = Pw->GetActorLocation();
		if (!Seeded(P)) continue;
		for (int32 dy = -3; dy <= 3 && Flush.Num() < Cap; ++dy) for (int32 dx = -3; dx <= 3 && Flush.Num() < Cap; ++dx)
		{
			const FIntPoint Key(FMath::FloorToInt(P.X / Cell) + dx, FMath::FloorToInt(P.Y / Cell) + dy);
			if (FlushCells.Contains(Key)) continue;
			FlushCells.Add(Key);
			const uint32 H = GetTypeHash(Key);
			const FVector C((Key.X + 0.2f + 0.6f * (H % 97) / 97.f) * Cell, (Key.Y + 0.2f + 0.6f * ((H / 97) % 89) / 89.f) * Cell, P.Z + 3000.f);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRFlush), false);
			if (!World->LineTraceSingleByChannel(Hit, C, C - FVector(0.f, 0.f, 9000.f), ECC_WorldStatic, Q)) continue;
			if (!Seeded(Hit.ImpactPoint) || Hit.ImpactNormal.Z < 0.7f) continue;
			// Never on a camp or a spawn safe zone (they stay clean).
			bool bSafe = false;
			if (const FWRLandDef* L = FWRFungalData::Land())
			{
				const TSharedPtr<FJsonObject> Camps = L->Section(TEXT("camps"));
				const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
				if (Camps.IsValid() && Camps->TryGetArrayField(TEXT("list"), List))
				{
					for (const TSharedPtr<FJsonValue>& V : *List) { const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray(); if (A.Num() >= 2 && FVector::Dist2D(L->ToWorld(FVector2D(A[0]->AsNumber(), A[1]->AsNumber()), 0.f), Hit.ImpactPoint) < 6000.f) bSafe = true; }
				}
			}
			if (bSafe) continue;
			if (AWRSporeCloud* Cl = AWRSporeCloud::Spawn(World, Hit.ImpactPoint + FVector(0.f, 0.f, 150.f), FeT(TEXT("fungalEvents.sporefall.patchCloud"), 600.f), TEXT("flush"), 0.f, nullptr)) Flush.Add(Cl);
			// Bones in the patch (one in six): a deepcap fruits on them (x3 pay: the signature scale).
			if ((H % 6) == 0)
			{
				const FTransform Tr(FRotator(0.f, float(H % 360), 0.f), Hit.ImpactPoint + FVector(250.f, 0.f, 0.f));
				if (AWRResourceNode* N = World->SpawnActorDeferred<AWRResourceNode>(AWRResourceNode::StaticClass(), Tr, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					N->ItemId = TEXT("deepcap");
					N->Hits = 1;
					N->PerHit = 1;
					N->RespawnSeconds = 1.0e7f;
					N->FinishSpawning(Tr);
					TWeakObjectPtr<AWRResourceNode> Weak(N);
					N->YieldScale = [Weak](AWRCharacterBase*) { const AWRResourceNode* Nd = Weak.Get(); const UWRFungalWorld* FW = Nd ? UWRFungalWorld::Get(Nd) : nullptr; return FW ? FW->SignatureScale() : 1.f; };
					FlushNodes.Add(N);
				}
			}
		}
	}
	// Bloomed beasts: 10 s in a flush cloud (a creature that isn't the caverns' own): a pale cap swells on its back.
	const float BloomAfter = FeT(TEXT("fungalEvents.sporefall.bloomAfter"), 10.f);
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		AWRCreature* Cr = *It;
		if (!Cr->IsAlive() || Cr->HasTag(WRFungalTags::Bloomed)) continue;
		const FWRCreatureDef* Def = Cr->GetDef();
		if (!Def || Def->Biome == WRFungal::Biome() || Def->Archetype == TEXT("boss")) continue;
		AWRSporeCloud* In = AWRSporeCloud::At(Cr, Cr->GetActorLocation());
		float& Tm = BloomTime.FindOrAdd(Cr);
		if (In && In->Kind == TEXT("flush")) Tm += 2.f; else Tm = FMath::Max(0.f, Tm - 2.f);
		if (Tm >= BloomAfter) { FeTag(Cr, WRFungalTags::Bloomed, true); BloomTime.Remove(Cr); }
	}
}

void UWRFungalWorld::StepIlmari(float Dt)
{
	// Ilmari Kask (fungal 2.11): by day at the log beds by the Great Shrine on the Garden Doline's floor (world.json "at");
	// at night in his cap-house on the Down-Stair Terraces (the warm window). He moves only when nobody is near.
	IlmariClock -= Dt;
	if (IlmariClock > 0.f) return;
	IlmariClock = 5.f;
	UWorld* World = GetWorld();
	const int32 Phase = AWRGameState::IsNightIn(World) ? 1 : 0;
	if (Phase == IlmariPhase) return;
	AWRWorldNpc* Ilmari = nullptr;
	for (TActorIterator<AWRWorldNpc> It(World); It; ++It) if (It->NpcRole == TEXT("master") && It->NpcId == TEXT("ilmari")) Ilmari = *It;
	if (!Ilmari) return;
	if (IlmariHome.IsZero()) { IlmariHome = Ilmari->GetActorLocation(); IlmariFace = Ilmari->GetActorRotation(); }
	FVector Spot = IlmariHome;
	FRotator Face = IlmariFace;
	if (Phase == 1)
	{
		// His cap-house on the terraces (WRFungalScenery Terraces: the house at local (700, 300), facing 200 from the
		// site's yaw): he sleeps at its door, out of the rain.
		if (const FWRFungalData::FSite* S1 = FWRFungalData::Get().FindSite(TEXT("S1")))
		{
			const FRotator R(0.f, S1->Yaw, 0.f);
			Spot = S1->At + R.RotateVector(FVector(700.f, 300.f, 0.f)) + R.RotateVector(FRotator(0.f, 200.f, 0.f).Vector()) * 420.f;
			float Z = 0.f;
			if (WRLand::GroundUnder(Spot + FVector(0.f, 0.f, 3000.f), Z)) Spot.Z = Z + 100.f;
			Face = (R.RotateVector(FVector(700.f, 300.f, 0.f)) + S1->At - Spot).GetSafeNormal2D().Rotation();
		}
	}
	const float Unseen = WRTune(TEXT("fungalWorld.ilmari.unseen"), 3000.f);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (P && (FVector::Dist(P->GetActorLocation(), Ilmari->GetActorLocation()) < Unseen || FVector::Dist(P->GetActorLocation(), Spot) < Unseen)) return;
	}
	IlmariPhase = Phase;
	Ilmari->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
	Ilmari->SetActorRotation(Face);
	Ilmari->WorkAt = Spot;
	Ilmari->WorkFacing = Face;
	Ilmari->WorkEvery = Phase == 1 ? 0.f : 4.5f;
	UE_LOG(LogWildreach, Log, TEXT("FUNGAL Ilmari goes to %s"), Phase == 1 ? TEXT("his cap-house") : TEXT("the log beds"));
}

void UWRFungalWorld::StepInvasionMarks(float Dt)
{
	// The invasions through the Flue (fungal 2.21): Spore invaders that stand 4 s leave a 3 m cloud for 2 game hours; Wind
	// invaders in the caverns push every cloud within 30 m of them 6 m every 8 s (the downdraft and their wings).
	UWorld* World = GetWorld();
	const AWRWorldState* WS = AWRWorldState::Get(World);
	if (!WS || !WS->Invasion.bOn) { StandStill.Reset(); return; }
	const bool bSporeOut = WS->Invasion.From == WRFungal::Biome();
	const bool bWindIn = WS->Invasion.To == WRFungal::Biome();
	if (AWRFungalState* S = State.Get()) S->FlueInvasion = bSporeOut ? 1 : bWindIn ? 2 : 0;
	InvClock -= Dt;
	if (InvClock > 0.f) return;
	InvClock = 1.f;
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		AWRCreature* Cr = *It;
		if (!Cr->IsAlive() || Cr->RaidOwner != TEXT("~invasion")) continue;
		if (bSporeOut)
		{
			FVector& Last = StandAt.FindOrAdd(Cr, Cr->GetActorLocation());
			float& Still = StandStill.FindOrAdd(Cr);
			if (FVector::Dist(Last, Cr->GetActorLocation()) < 100.f) Still += 1.f; else { Still = 0.f; Last = Cr->GetActorLocation(); }
			if (Still >= WRTune(TEXT("fungalEvents.invasion.standSeconds"), 4.f))
			{
				Still = -1000.f;   // once per spot
				AWRSporeCloud::Spawn(World, Cr->GetActorLocation(), WRTune(TEXT("fungalEvents.invasion.markRadius"), 300.f), TEXT("creature"), WRTune(TEXT("fungalEvents.invasion.markHours"), 2.f) * WRTune(TEXT("clock.dayMinutes"), 60.f) * 2.5f, Cr);
			}
		}
		else if (bWindIn && FMath::Fmod(WRFungal::Now(World), 8.f) < 1.f && WRFungal::Contains(Cr->GetActorLocation()))
		{
			for (const TWeakObjectPtr<AWRSporeCloud>& C : AWRSporeCloud::All())
			{
				if (C.IsValid() && C->IsLive() && FVector::Dist(C->Centre(), Cr->GetActorLocation()) < 3000.f) C->Push((C->Centre() - Cr->GetActorLocation()).GetSafeNormal2D(), WRTune(TEXT("hazards.fungal.cloud.windPush"), 600.f), Cr);
			}
		}
	}
}

void UWRFungalWorld::StepClientLooks(float Dt)
{
	using namespace WRFungalLook;
	// Every machine that draws: glow paste's cold light on whoever wears it; a bloomed beast's fruiting cap on its back.
	UWorld* World = GetWorld();
	LookClock -= Dt;
	if (LookClock > 0.f) return;
	LookClock = 0.5f;
	FVector View = FVector::ZeroVector;
	if (const APlayerController* PC = World->GetFirstPlayerController()) { FRotator R; PC->GetPlayerViewPoint(View, R); }
	for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
	{
		AWRCharacterBase* C = *It;
		if (FVector::DistSquared(C->GetActorLocation(), View) > FMath::Square(12000.f)) continue;
		const bool bPaste = C->IsAlive() && C->HasTag(WRFungalTags::GlowPaste);
		TWeakObjectPtr<UPointLightComponent>* L = PasteLights.Find(C);
		if (bPaste && (!L || !L->IsValid()))
		{
			UPointLightComponent* P = NewObject<UPointLightComponent>(C);
			P->SetupAttachment(C->GetRootComponent());
			P->SetRelativeLocation(FVector(30.f, 0.f, 60.f));
			P->SetLightColor(CyanC());
			P->SetIntensity(WRTune(TEXT("hazards.fungal.glowPaste.intensity"), 1600.f));
			P->SetAttenuationRadius(WRTune(TEXT("hazards.fungal.glowPaste.radius"), 600.f));
			P->SetCastShadows(false);
			P->RegisterComponent();
			PasteLights.Add(C, P);
		}
		else if (!bPaste && L && L->IsValid()) { (*L)->DestroyComponent(); PasteLights.Remove(C); }
		if (const AWRCreature* Cr = Cast<AWRCreature>(C))
		{
			const bool bBloom = Cr->IsAlive() && C->HasTag(WRFungalTags::Bloomed);
			TWeakObjectPtr<UProceduralMeshComponent>* B = BloomCaps.Find(C);
			if (bBloom && (!B || !B->IsValid()))
			{
				// A pale fruiting cap swollen on its back (Sporefall 2.10): it's alive and still itself.
				FWRToolMesh M;
				const float R = FMath::Max(25.f, C->GetSimpleCollisionRadius() * 0.7f);
				M.Lathe(FVector(0.f, 0.f, -R * 0.2f), FVector::UpVector, R * 0.6f, { FVector(0.f, R * 0.25f, R * 0.25f), FVector(1.f, R * 0.2f, R * 0.2f) }, 10, StipeC(), 0.8f);
				M.Lathe(FVector(0.f, 0.f, R * 0.35f), FVector::UpVector, R * 0.6f, { FVector(0.f, R, R), FVector(0.4f, R * 0.85f, R * 0.85f), FVector(1.f, 2.f, 2.f) }, 14, Rgb(232, 226, 210) * 0.62f, 0.7f);
				UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(C);
				P->SetupAttachment(C->GetRootComponent());
				P->SetRelativeLocation(FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() * 0.8f));
				P->RegisterComponent();
				M.ApplyTo(P);
				BloomCaps.Add(C, P);
			}
			else if (!bBloom && B && B->IsValid()) { (*B)->DestroyComponent(); BloomCaps.Remove(C); }
		}
	}
}

// ================================================================================================ deaths and thrown models

void WRFungal::CreatureDied(AWRCreature* Self, AActor* Killer)
{
	if (!Self || !Self->HasAuthority() || !WRFungal::Live()) return;
	// A bloomed beast bursts on death (fungal 2.10): a 4 m spore cloud for 6 s.
	if (Self->HasTag(WRFungalTags::Bloomed))
	{
		AWRSporeCloud::Spawn(Self->GetWorld(), Self->GetActorLocation() + FVector(0.f, 0.f, 80.f), WRTune(TEXT("fungalEvents.sporefall.bloomBurst"), 400.f), TEXT("creature"), WRTune(TEXT("fungalEvents.sporefall.bloomBurstSeconds"), 6.f), nullptr);
		WRFx::Broadcast(Self, TEXT("fungal.bloomBurst"), Self->GetActorLocation() + FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, 1.f, TEXT("fungal.bloom_burst"));
	}
}

bool WRFungal::BuildThrownModel(const FString& ModelPath, UProceduralMeshComponent* Mesh)
{
	if (!Mesh || ModelPath != TEXT("code:galeflask")) return false;
	// The Gale flask (fungal 2.19): a flat clay flask 16 cm long, a levistone shard showing blue-white through a window cut
	// in its belly (it tugs upward), a wax stopper, a twine sling.
	FWRToolMesh M;
	const FLinearColor Clay = FLinearColor(FColor(160, 120, 88)) * 0.55f, Stone = FLinearColor(FColor(200, 226, 255)) * 0.62f;
	const FLinearColor Wax = FLinearColor(FColor(198, 160, 92)) * 0.55f, Twine = FLinearColor(FColor(120, 98, 70)) * 0.5f;
	M.Lathe(FVector(-8.f, 0.f, 0.f), FVector::ForwardVector, 16.f, { FVector(0.f, 3.f, 2.f), FVector(0.2f, 7.f, 4.f), FVector(0.55f, 7.5f, 4.5f), FVector(0.85f, 4.f, 2.6f), FVector(1.f, 2.6f, 2.f) }, 12, Clay, 0.9f);
	M.Lathe(FVector(-2.f, 0.f, 0.f), FVector::ForwardVector, 5.f, { FVector(0.f, 4.f, 4.6f), FVector(1.f, 3.6f, 4.4f) }, 8, Stone, 0.3f);
	M.Lathe(FVector(7.6f, 0.f, 0.f), FVector::ForwardVector, 2.f, { FVector(0.f, 2.4f, 2.f), FVector(1.f, 2.6f, 2.2f) }, 8, Wax, 0.5f);
	M.Tube({ FVector(6.f, 3.f, 0.f), FVector(6.f, 0.f, 2.6f), FVector(6.f, -3.f, 0.f), FVector(6.f, 0.f, -2.6f) }, 0.4f, 5, Twine, 0.85f, true);
	M.ApplyTo(Mesh);
	return true;
}
