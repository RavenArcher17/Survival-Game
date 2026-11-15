// The Ancient forest's creatures (cooks/biome-forest.md 2.12, 2.15; cooks/build-biome-forest.md 1.3, 2.9). See
// WRForestCreatures.h. The Storm peaks / Swamp pattern: rows in Data/attacks.json (creatures.<id>), numbers in tuning
// creatureAI.<id> / creatureAI.forest / tame.forest, stand-in rigs with the cook's palette over their own texture and
// code-modelled silhouette parts (FWRToolMesh sweeps on M_HeldTool) on frames riding their bones.
#include "WRForestCreatures.h"
#include "WRForestTypes.h"
#include "WRForestHazards.h"
#include "WRForestHollow.h"
#include "WRForestBosses.h"
#include "WRNaturePath.h"
#include "WRCreature.h"
#include "WRBoss.h"
#include "WRBuilding.h"
#include "WRFarm.h"
#include "WRAttackRows.h"
#include "WRAttributeSet.h"
#include "WRCharacter.h"
#include "WRCombatLibrary.h"
#include "WRCombatTypes.h"
#include "WRFx.h"
#include "WRGameState.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRResourceNode.h"
#include "WRSound.h"
#include "WRStatusComponent.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

// ---------------------------------------------------------------- shared bits

namespace WRForestCrImpl
{
	/** creatures.json rows by id, read once (the look and the tame-use keys the def doesn't carry). */
	TSharedPtr<FJsonObject> Row(FName Id)
	{
		static TMap<FName, TSharedPtr<FJsonObject>> Cache;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("creatures.json"));
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("creatures"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FString IdStr;
					if (O.IsValid() && O->TryGetStringField(TEXT("id"), IdStr)) Cache.Add(FName(*IdStr), O);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Found = Cache.Find(Id);
		return Found ? *Found : nullptr;
	}

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Field, V) ? float(V) : Fallback;
	}

	FLinearColor Hex(const TCHAR* S) { return FLinearColor(FColor::FromHex(S)); }

	FLinearColor Color(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(Field, S)) return FLinearColor(FColor::FromHex(S));
		return Fallback;
	}

	TSharedPtr<FJsonObject> Look(FName Id)
	{
		const TSharedPtr<FJsonObject> R = Row(Id);
		const TSharedPtr<FJsonObject>* L = nullptr;
		return R.IsValid() && R->TryGetObjectField(TEXT("look"), L) ? *L : nullptr;
	}

	/** The body the rules and look are for (look.kind): a creature id otherwise. */
	FName Kind(FName Id)
	{
		const TSharedPtr<FJsonObject> L = Look(Id);
		FString S;
		return L.IsValid() && L->TryGetStringField(TEXT("kind"), S) ? FName(*S) : Id;
	}

	FName KindOf(const AActor* A)
	{
		const AWRCreature* C = Cast<AWRCreature>(A);
		return C ? Kind(C->CreatureId) : NAME_None;
	}

	double Now(const UObject* W) { return W && W->GetWorld() ? W->GetWorld()->GetTimeSeconds() : 0.0; }

	/** tuning creatureAI.<Key> (creatureAI.mossmaul.hugSeconds, creatureAI.forest.lifeStepSeconds...). */
	float Tn(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("creatureAI.%s"), Key), Fallback); }
	/** tuning tame.forest.<Key>. */
	float Tt(const TCHAR* Key, float Fallback) { return FWRTuning::Get().Num(FString::Printf(TEXT("tame.forest.%s"), Key), Fallback); }

	/** A [lo, hi] pair in tuning creatureAI.<Key>, rolled. */
	float Roll(const TCHAR* Key, float Lo, float Hi)
	{
		FString Group, Field;
		FString(Key).Split(TEXT("."), &Group, &Field, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		const TSharedPtr<FJsonObject> O = FWRTuning::Get().Object(FString::Printf(TEXT("creatureAI.%s"), *Group));
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 2)
		{
			Lo = float((*A)[0]->AsNumber());
			Hi = float((*A)[1]->AsNumber());
		}
		return FMath::FRandRange(Lo, FMath::Max(Lo, Hi));
	}

	bool Wild(const AWRCreature* C) { return C && C->IsAlive() && !C->IsTamed(); }

	FVector Feet(const AActor* A) { return A->GetActorLocation() - FVector(0.f, 0.f, A->GetSimpleCollisionHalfHeight()); }

	FVector Ground(const UWorld* World, const FVector& At, bool& bOk, const AActor* Ignore = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestCreatureGround), false, Ignore);
		bOk = World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Q);
		return bOk ? FVector(Hit.ImpactPoint) : At;
	}

	AWRCharacter* OwnerBody(const UWorld* World, const FString& Name)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			const APlayerState* PS = It->GetPlayerState();
			if (PS && PS->GetPlayerName() == Name && It->IsAlive()) return *It;
		}
		return nullptr;
	}

	AWRPlayerController* OwnerPC(const UWorld* World, const FString& Name)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		for (APlayerState* PS : GS ? GS->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			if (PS && PS->GetPlayerName() == Name) return Cast<AWRPlayerController>(PS->GetOwner());
		}
		return nullptr;
	}

	void Tell(const AActor* Who, const FString& Text)
	{
		const APawn* P = Cast<APawn>(Who);
		if (AWRPlayerController* PC = P ? Cast<AWRPlayerController>(P->GetController()) : nullptr) PC->ClientNotify(Text);
	}

	float HealthFrac(const AWRCharacterBase* C)
	{
		if (const AWRCreature* Cr = Cast<AWRCreature>(C)) return Cr->GetHealthFraction();
		const UWRAttributeSet* A = C ? C->GetAttributes() : nullptr;
		return A && A->GetMaxHealth() > 0.f ? A->GetHealth() / A->GetMaxHealth() : 1.f;
	}

	float Difficulty() { return FMath::Max(0.1f, WRTune(TEXT("server.creatureDifficulty"), 1.f)); }

	/** The nearest living, visible survivor within Range of At (not held in anything's grip). */
	AWRCharacter* NearestSurvivor(const UWorld* World, const FVector& At, float Range)
	{
		AWRCharacter* Best = nullptr;
		float BestD = Range;
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (!It->IsAlive() || It->IsHidden()) continue;
			const float D = FVector::Dist(It->GetActorLocation(), At);
			if (D < BestD) { BestD = D; Best = *It; }
		}
		return Best;
	}

	/** The forest's weather over a point ("rain", "fog", "overcast"; none when clear or off the forest). */
	FName WeatherAt(const UObject* Ctx, const FVector& At)
	{
		if (WRBiome::At(Ctx, At) != TEXT("forest")) return NAME_None;
		const FWRWeatherKind* K = UWRWeatherSubsystem::ActiveKind(Ctx, TEXT("forest"));
		return K ? K->Id : NAME_None;
	}

	/** A hold's or a cling's tick of damage: through ApplyHit, a grab no guard stops. */
	void Dot(AWRCreature* By, AWRCharacterBase* Victim, float Damage)
	{
		FWRHit Hit;
		Hit.Instigator = By;
		Hit.Damage = Damage;
		Hit.PoiseDamage = 0.f;
		Hit.Impact = 1;
		Hit.bDamageOverTime = true;
		Hit.bGrab = true;
		Hit.bUnblockable = true;
		Hit.PhysType = 3;
		Hit.Element = WRTags::Element_Physical;
		Hit.Origin = By->GetActorLocation();
		UWRCombatLibrary::ApplyHit(Victim, Hit);
	}

	/** A row of this creature by id. */
	const FWRAttackRow* RowOf(const AWRCreature* C, FName Id)
	{
		const TMap<FName, FWRAttackRow>* Rows = C ? FWRAttackRows::Get().Creature(C->CreatureId) : nullptr;
		return Rows ? Rows->Find(Id) : nullptr;
	}

	/** A hover kind (it lives in the air: the wasps, the bees, the finches). */
	bool Hovers(FName K) { return K == TEXT("bramblewasp") || K == TEXT("hiveback") || K == TEXT("glimmerfinch"); }

	/** Total game hours (the clock every machine shares). */
	double GameHours(const UObject* Ctx)
	{
		const AWRGameState* GS = AWRGameState::Get(Ctx);
		return GS ? GS->GetTotalHours() : 0.0;
	}

	int32 GameDay(const UObject* Ctx)
	{
		const AWRGameState* GS = AWRGameState::Get(Ctx);
		return GS ? GS->GetDay() : 0;
	}

	/** A survivor's capsule is guarding (a block held) when the roar comes. */
	bool Guarding(const AWRCharacterBase* C) { return C && (C->HasTag(WRTags::State_Blocking) || C->HasTag(WRTags::State_Parrying)); }

	/** Every beehive in the forest: the beehive trees of the kit and the honey nodes still standing (WRForestWorld). */
	void Hives(const UWorld* World, TArray<FVector>& Out)
	{
		const FWRForestData& D = FWRForestData::Get();
		for (const FWRForestData::FSite& S : D.Kit) if (S.Kind == TEXT("beehive_tree")) Out.Add(S.At);
		for (const FWRForestData::FSite& S : D.Sites) if (S.Kind == TEXT("beehive_tree") || S.Kind == TEXT("herb_terraces")) Out.Add(S.At);
		for (TActorIterator<AWRResourceNode> It(World); It; ++It)
		{
			if (It->ItemId != TEXT("honey") || !WRForest::Contains(It->GetActorLocation())) continue;
			bool bKnown = false;
			for (const FVector& H : Out) bKnown |= FVector::Dist2D(H, It->GetActorLocation()) < 1500.f;
			if (!bKnown) Out.Add(It->GetActorLocation());
		}
	}

	FVector NearestOf(const TArray<FVector>& List, const FVector& At, float Range)
	{
		FVector Best = FVector::ZeroVector;
		float BestD = Range;
		for (const FVector& P : List)
		{
			const float D = FVector::Dist2D(P, At);
			if (D < BestD) { BestD = D; Best = P; }
		}
		return Best;
	}
}

// ---------------------------------------------------------------- the code-modelled parts

namespace WRForestCrMesh
{
	FColor Pack(const FLinearColor& Colour, float Rough)
	{
		// The vertex colour M_HeldTool reads as stored (linear in, no sRGB step), roughness in alpha (as FWRToolMesh).
		return FColor(FMath::Clamp(FMath::RoundToInt(Colour.R * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Colour.G * 255.f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(Colour.B * 255.f), 0, 255), FMath::Clamp(FMath::RoundToInt(Rough * 255.f), 0, 255));
	}

	/** A smooth swept solid along Path: an ellipse at each point, RY across Width (the up-hint's direction, squared off
	 *  against the path), RZ across the third axis. Smooth-shaded (normals from the ellipse), capped at both ends, wound
	 *  as FWRToolMesh::Tube (faces out). The one primitive every part is built from: claws, horns, tines, fronds, plates,
	 *  cushions, wings, fleece curls. */
	void Sweep(FWRToolMesh& M, const TArray<FVector>& Path, const TArray<FVector2D>& Radii, const FVector& Width, int32 Sides, const FLinearColor& Colour, float Rough, bool bCaps = true)
	{
		const int32 N = FMath::Min(Path.Num(), Radii.Num());
		if (N < 2 || Sides < 3) return;
		const FColor Col = Pack(Colour, Rough);
		const int32 Base = M.Verts.Num();
		TArray<FVector> Us, Vs, Ts;
		float Along = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector T = (Path[FMath::Min(N - 1, i + 1)] - Path[FMath::Max(0, i - 1)]).GetSafeNormal();
			FVector U = Width - T * FVector::DotProduct(Width, T);
			if (U.SizeSquared() < 1e-4f) U = FVector::CrossProduct(T, FMath::Abs(T.Z) > 0.9f ? FVector::XAxisVector : FVector::ZAxisVector);
			U.Normalize();
			const FVector V = FVector::CrossProduct(T, U);
			Us.Add(U); Vs.Add(V); Ts.Add(T);
			if (i > 0) Along += FVector::Dist(Path[i], Path[i - 1]);
			const float RY = FMath::Max(0.05f, Radii[i].X), RZ = FMath::Max(0.05f, Radii[i].Y);
			for (int32 s = 0; s < Sides; ++s)
			{
				const float A = 2.f * PI * s / Sides;
				const float C = FMath::Cos(A), S = FMath::Sin(A);
				M.Verts.Add(Path[i] + U * C * RY + V * S * RZ);
				M.Normals.Add((U * C / RY + V * S / RZ).GetSafeNormal());
				M.UVs.Add(FVector2D(Along / 12.f, s / static_cast<float>(Sides)));
				M.Colors.Add(Col);
			}
		}
		for (int32 i = 0; i + 1 < N; ++i)
		{
			for (int32 s = 0; s < Sides; ++s)
			{
				const int32 A = Base + i * Sides + s, B = Base + i * Sides + (s + 1) % Sides;
				const int32 C = A + Sides, D = B + Sides;
				M.Tris.Append({ A, C, B, B, C, D });
			}
		}
		if (!bCaps) return;
		for (const int32 End : { 0, N - 1 })
		{
			if (Radii[End].X <= 0.06f && Radii[End].Y <= 0.06f) continue;
			const FVector Out = End == 0 ? -Ts[0] : Ts[N - 1];
			const int32 Mid = M.Verts.Num();
			M.Verts.Add(Path[End]); M.Normals.Add(Out); M.UVs.Add(FVector2D(0.5f, 0.5f)); M.Colors.Add(Col);
			const int32 First = M.Verts.Num();
			for (int32 s = 0; s < Sides; ++s)
			{
				M.Verts.Add(M.Verts[Base + End * Sides + s]); M.Normals.Add(Out); M.UVs.Add(FVector2D(0.f, s / static_cast<float>(Sides))); M.Colors.Add(Col);
			}
			for (int32 s = 0; s < Sides; ++s)
			{
				const int32 P0 = First + s, P1 = First + (s + 1) % Sides;
				if (End == 0) M.Tris.Append({ Mid, P0, P1 });
				else M.Tris.Append({ Mid, P1, P0 });
			}
		}
	}

	/** Points along a curve: from Base heading Dir, turning toward Curl through Degrees over Len (a claw's hook, a tine). */
	TArray<FVector> Arc(const FVector& Base, const FVector& Dir, const FVector& Curl, float Len, float Degrees, int32 Segs)
	{
		TArray<FVector> Out;
		FVector P = Base, D = Dir.GetSafeNormal();
		const FVector Axis = FVector::CrossProduct(D, Curl.GetSafeNormal()).GetSafeNormal();
		const float Step = Len / FMath::Max(1, Segs);
		const float Turn = Degrees / FMath::Max(1, Segs);
		Out.Add(P);
		for (int32 i = 0; i < Segs; ++i)
		{
			if (!Axis.IsNearlyZero()) D = D.RotateAngleAxis(Turn, Axis);
			P += D * Step;
			Out.Add(P);
		}
		return Out;
	}

	/** A tapering horn, claw, tine or spike along Pts: round R0 at the root to R1 at the tip, a little flattened. */
	void Horn(FWRToolMesh& M, const TArray<FVector>& Pts, float R0, float R1, float Flat, const FVector& Width, const FLinearColor& C, float Rough, int32 Sides = 7)
	{
		TArray<FVector2D> Radii;
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const float T = Pts.Num() > 1 ? float(i) / (Pts.Num() - 1) : 0.f;
			const float R = FMath::Lerp(R0, R1, FMath::Pow(T, 0.8f));
			Radii.Add(FVector2D(R, R * Flat));
		}
		Sweep(M, Pts, Radii, Width, Sides, C, Rough);
	}

	/** An ellipsoid along Axis from Centre - Axis * Len / 2: RY across Width, RZ across the rest (moss cushions, fungus,
	 *  bodies, pollen baskets, spots when flat). */
	void Blob(FWRToolMesh& M, const FVector& Centre, const FVector& Axis, float Len, float RY, float RZ, const FVector& Width, const FLinearColor& C, float Rough, int32 Rings = 7, int32 Sides = 9)
	{
		TArray<FVector> Path;
		TArray<FVector2D> Radii;
		const FVector A = Axis.GetSafeNormal();
		for (int32 i = 0; i <= Rings; ++i)
		{
			const float T = float(i) / Rings;
			Path.Add(Centre + A * (T - 0.5f) * Len);
			const float S = FMath::Max(0.08f, FMath::Sin(PI * T));
			Radii.Add(FVector2D(RY * S, RZ * S));
		}
		Sweep(M, Path, Radii, Width, Sides, C, Rough);
	}

	/** A flat blade (a leaf, a pinna, a wing, a palm): along Dir for Len, Width across WidthDir, Thick through it, its
	 *  widest at Widest (0..1), tapering to both ends. */
	void Blade(FWRToolMesh& M, const FVector& Root, const FVector& Dir, const FVector& WidthDir, float Len, float Width, float Thick, float Widest, const FLinearColor& C, float Rough, float Droop = 0.f, const FVector& DroopDir = FVector::ZeroVector)
	{
		TArray<FVector> Path;
		TArray<FVector2D> Radii;
		const int32 Segs = 6;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float T = float(i) / Segs;
			Path.Add(Root + Dir.GetSafeNormal() * Len * T + DroopDir * Droop * T * T);
			const float Shape = T < Widest ? FMath::Sin(HALF_PI * T / FMath::Max(0.05f, Widest)) : FMath::Cos(HALF_PI * (T - Widest) / FMath::Max(0.05f, 1.f - Widest));
			Radii.Add(FVector2D(FMath::Max(0.1f, Width * 0.5f * Shape), Thick * 0.5f));
		}
		Sweep(M, Path, Radii, WidthDir, 6, C, Rough);
	}

	/** A fern frond: a rachis arching out and over, pinnae alternating down both sides, a fiddlehead curl at the tip on
	 *  young ones (Curl). */
	void Frond(FWRToolMesh& M, const FVector& Base, const FVector& Out, const FVector& Side, float Len, float Width, const FLinearColor& C, bool bCurl, float Seed)
	{
		const FVector Up = FVector::CrossProduct(Out.GetSafeNormal(), Side.GetSafeNormal()).GetSafeNormal();
		const TArray<FVector> Rachis = Arc(Base, Out, -Up, Len, bCurl ? 220.f : 55.f + 20.f * FMath::Frac(Seed * 7.13f), bCurl ? 12 : 8);
		Horn(M, Rachis, FMath::Max(0.4f, Width * 0.06f), 0.15f, 1.f, Side, C * 0.8f, 0.85f, 5);
		if (bCurl) return;
		for (int32 i = 1; i + 1 < Rachis.Num(); ++i)
		{
			const float T = float(i) / (Rachis.Num() - 1);
			const FVector Along = (Rachis[i + 1] - Rachis[i - 1]).GetSafeNormal();
			const FVector Across = FVector::CrossProduct(Along, Up).GetSafeNormal();
			const float PL = Width * (1.f - T * 0.75f);
			for (const float S : { -1.f, 1.f })
			{
				Blade(M, Rachis[i], (Across * S + Along * 0.45f).GetSafeNormal(), Along, PL, PL * 0.32f, FMath::Max(0.3f, PL * 0.04f), 0.4f, C * (0.9f + 0.2f * FMath::Frac(Seed + i * 0.37f)), 0.75f);
			}
		}
	}
}

// ---------------------------------------------------------------- the body's shape (where parts sit on its skin)

namespace WRForestCrShape
{
	/** The rig's physics bodies (spheres, capsules, boxes on its bones) in world space, and the asset's bounds as an
	 *  ellipsoid for rigs that have none: the skin the parts are placed on. */
	struct FShape { uint8 Type = 0; FVector A = FVector::ZeroVector, B = FVector::ZeroVector; float R = 0.f; FTransform Box; FVector Half = FVector::ZeroVector; };
	struct FBodySkin
	{
		TArray<FShape> Shapes;
		FTransform EllipseTM;
		FVector EllipseHalf = FVector(50.f);

		void Build(const USkeletalMeshComponent* Skel)
		{
			Shapes.Reset();
			const USkeletalMesh* Mesh = Skel ? Skel->GetSkeletalMeshAsset() : nullptr;
			if (!Mesh) return;
			const FBox Local = Mesh->GetBounds().GetBox();
			EllipseTM = FTransform(Local.GetCenter()) * Skel->GetComponentTransform();
			EllipseHalf = Local.GetExtent() * 0.92f;
			const UPhysicsAsset* PA = Skel->GetPhysicsAsset();
			if (!PA) return;
			for (const TObjectPtr<USkeletalBodySetup>& BS : PA->SkeletalBodySetups)
			{
				if (!BS) continue;
				const int32 Bone = Skel->GetBoneIndex(BS->BoneName);
				if (Bone == INDEX_NONE) continue;
				const FTransform BoneTM = Skel->GetBoneTransform(Bone);
				const float Scale = BoneTM.GetMaximumAxisScale();
				for (const FKSphereElem& E : BS->AggGeom.SphereElems)
				{
					FShape S; S.Type = 0; S.A = S.B = BoneTM.TransformPosition(E.Center); S.R = E.Radius * Scale; Shapes.Add(S);
				}
				for (const FKSphylElem& E : BS->AggGeom.SphylElems)
				{
					const FTransform ET = E.GetTransform() * BoneTM;
					FShape S; S.Type = 1; S.A = ET.TransformPosition(FVector(0.f, 0.f, -E.Length * 0.5f)); S.B = ET.TransformPosition(FVector(0.f, 0.f, E.Length * 0.5f)); S.R = E.Radius * Scale; Shapes.Add(S);
				}
				for (const FKBoxElem& E : BS->AggGeom.BoxElems)
				{
					FShape S; S.Type = 2; S.Box = E.GetTransform() * BoneTM; S.Half = FVector(E.X, E.Y, E.Z) * 0.5f; Shapes.Add(S);
				}
			}
		}

		bool Inside(const FVector& P) const
		{
			if (Shapes.Num() == 0)
			{
				const FVector L = EllipseTM.InverseTransformPosition(P);
				return FMath::Square(L.X / EllipseHalf.X) + FMath::Square(L.Y / EllipseHalf.Y) + FMath::Square(L.Z / EllipseHalf.Z) <= 1.f;
			}
			for (const FShape& S : Shapes)
			{
				if (S.Type == 2)
				{
					const FVector L = S.Box.InverseTransformPositionNoScale(P);
					if (FMath::Abs(L.X) <= S.Half.X && FMath::Abs(L.Y) <= S.Half.Y && FMath::Abs(L.Z) <= S.Half.Z) return true;
				}
				else if (FVector::Dist(FMath::ClosestPointOnSegment(P, S.A, S.B), P) <= S.R) return true;
			}
			return false;
		}

		/** Where a line from From (outside) toward To (inside) first meets the skin; Fallback when it never does. */
		FVector Surface(const FVector& From, const FVector& To, const FVector& Fallback) const
		{
			const float Len = FVector::Dist(From, To);
			const int32 Steps = FMath::Clamp(FMath::CeilToInt(Len / 1.5f), 8, 400);
			for (int32 i = 0; i <= Steps; ++i)
			{
				const FVector P = FMath::Lerp(From, To, float(i) / Steps);
				if (Inside(P)) return P;
			}
			return Fallback;
		}
	};
}

// ---------------------------------------------------------------- the body component

TArray<TWeakObjectPtr<UWRForestBody>> UWRForestBody::Live;

UWRForestBody::UWRForestBody()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRForestBody::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRForestBody, Flags);
	DOREPLIFETIME(UWRForestBody, Spot);
	DOREPLIFETIME(UWRForestBody, PathTamer);
}

UWRForestBody* UWRForestBody::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRForestBody>() : nullptr;
}

bool UWRForestBody::IsForestKind(FName CreatureId)
{
	const FWRCreatureDef* Def = FWRCreatureRegistry::Get().Find(CreatureId);
	return Def && Def->Biome == TEXT("forest") && Def->Archetype != TEXT("boss");
}

void UWRForestBody::Attach(AWRCreature* Creature)
{
	if (!Creature || !Creature->HasAuthority() || !IsForestKind(Creature->CreatureId) || Get(Creature) || Creature->IsA<AWRBoss>())
	{
		return;
	}
	UWRForestBody* Body = NewObject<UWRForestBody>(Creature, TEXT("ForestBody"));
	Body->RegisterComponent();
	Creature->AddInstanceComponent(Body);
}

FName UWRForestBody::KindId() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? WRForestCrImpl::Kind(C->CreatureId) : NAME_None;
}

void UWRForestBody::BeginPlay()
{
	Super::BeginPlay();
	Live.RemoveAll([](const TWeakObjectPtr<UWRForestBody>& B) { return !B.IsValid(); });
	Live.Add(this);
	// After the creature's own tick, so its pose (a hang, a rear) holds over a wind-up's rear-back.
	if (GetOwner()) AddTickPrerequisiteActor(GetOwner());
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	if (!C || !Def)
	{
		return;
	}
	// Movement every machine that moves it agrees on (a rider's machine predicts its mount): the Elderelk clears a
	// fallen giant up to 2 m (creatures.json jumpHeight).
	const TSharedPtr<FJsonObject> R = WRForestCrImpl::Row(C->CreatureId);
	if (const float Jump = WRForestCrImpl::Num(R, TEXT("jumpHeight"), 0.f); Jump > 0.f)
	{
		C->GetCharacterMovement()->JumpZVelocity = FMath::Sqrt(2.f * 980.f * Jump);
	}
	if (!C->HasAuthority())
	{
		return;
	}
	const double T = WRForestCrImpl::Now(this);
	const FName K = KindId();
	NextPerchAt = T + FMath::FRandRange(10.f, 40.f);
	RestAgainAt = T + FMath::FRandRange(3.f, 12.f);
	NextTruffleAt = T + WRForestCrImpl::Tt(TEXT("truffleEvery"), 120.f);
	NextSeedAt = T + WRForestCrImpl::Tt(TEXT("seedEvery"), 300.f);
	NextCacheAt = T + FMath::FRandRange(5.f, 15.f);
	NextHoverAt = T;
	// The hive's own bees keep to it (cook 2.12.1: 3 to 6 at a hive): a wild Hiveback's home is the nearest hive.
	if (K == TEXT("hiveback") && !C->IsTamed())
	{
		TArray<FVector> Hives;
		WRForestCrImpl::Hives(C->GetWorld(), Hives);
		const FVector Hive = WRForestCrImpl::NearestOf(Hives, C->GetActorLocation(), WRForestCrImpl::Tn(TEXT("hiveback.hiveSearch"), 15000.f));
		if (!Hive.IsZero()) C->SetHome(Hive + FVector(0.f, 0.f, 150.f));
	}
}

void UWRForestBody::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Reason == EEndPlayReason::Destroyed && GetOwner() && GetOwner()->HasAuthority())
	{
		AWRCreature* C = Cast<AWRCreature>(GetOwner());
		if (C && Hold.IsValid()) FWRForestAI::ReleaseHold(C, false);
		if (C && ClingTo.IsValid()) FWRForestAI::Uncling(C, false);
	}
	ClearLook();
	Live.RemoveAll([this](const TWeakObjectPtr<UWRForestBody>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(Reason);
}

USkeletalMeshComponent* UWRForestBody::Skel() const
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	return C ? C->GetCreatureSkel() : nullptr;
}

void UWRForestBody::MulticastMoment_Implementation(FName Effect, FName Bone, float Scale)
{
	const AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || GetNetMode() == NM_DedicatedServer) return;
	const FName B = C->FindBone(Bone);
	const FVector At = B.IsNone() || !Skel() ? C->GetActorLocation() : Skel()->GetBoneLocation(B);
	WRFx::SpawnAt(C, Effect, At, C->GetActorRotation(), Scale);
}

void UWRForestBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	if (!GetOwner())
	{
		return;
	}
	TickThin();
	if (GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLook(DeltaTime);
	}
}

void UWRForestBody::TickServer(float DeltaTime)
{
	if (AWRCreature* C = Cast<AWRCreature>(GetOwner()))
	{
		FWRForestAI::TickBody(C, this, DeltaTime);
	}
}

void UWRForestBody::TickThin()
{
	// The Elderelk slips between trees (cook 2.12.1): ridden, its collision is a thin capsule so it never snags on
	// trunks or brush. Every machine that moves it (the server, its rider's) sizes it the same from the replicated rider.
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	UCapsuleComponent* Cap = C ? C->GetCapsuleComponent() : nullptr;
	if (!Cap || KindId() != TEXT("elderelk"))
	{
		return;
	}
	const bool bWant = C->GetRider() != nullptr && C->IsAlive();
	if (bWant == bThin)
	{
		return;
	}
	if (bWant)
	{
		BaseRadius = Cap->GetUnscaledCapsuleRadius();
		Cap->SetCapsuleRadius(FMath::Min(BaseRadius, WRForestCrImpl::Tt(TEXT("thinRadius"), 45.f)));
	}
	else if (BaseRadius > 0.f)
	{
		Cap->SetCapsuleRadius(BaseRadius);
	}
	bThin = bWant;
}

// ---------------------------------------------------------------- the look (machines that draw)

namespace WRForestCrPart
{
	// What a part does as the creature lives (UWRForestBody::TickLook).
	enum : int32
	{
		Static = 0,
		Wing = 1,       // beats (a wasp's and a bee's blur, a finch's flutter): Phase holds its side
		Flock = 2,      // a flock finch: flies round the lead bird on its own orbit (Orbit = radius, height, speed)
		FlockWing = 3,  // a flock finch's wing
		Bloom = 4,      // flowers and ferns grown into a Mossmaul's moss while a hand is on it (path-magic tame)
		Breath = 5,     // a Rootcoil's mid-body plates and saddles: rise and fall while it lies as a log
		Claw = 6,       // a Mossmaul's hooked claws: uncurl as it rears
		Tail = 7,       // a Dappledoe's tail: always flicking
		Coil = 8,       // a Rootcoil's coils round a held body (shown while it constricts)
		Larder = 9,     // moss pulled over a Burlbeast carcass a Mossmaul has cached
	};
}

namespace WRForestCrLook
{
	/** A seeded 0..1 from a part's place and a salt (the same on every machine: looks are built from data, not dice). */
	float Jit(const FVector& P, int32 Salt)
	{
		const uint32 H = HashCombine(GetTypeHash(FIntVector(FMath::RoundToInt(P.X * 0.37f), FMath::RoundToInt(P.Y * 0.37f), Salt)), uint32(Salt * 7919));
		return float(H % 10007u) / 10006.f;
	}

	/** A fern-curl of fleece or a fiddlehead: a tapering tube curling back on itself. */
	void Curl(FWRToolMesh& M, const FVector& Base, const FVector& Out, const FVector& CurlTo, float Len, float R0, float R1, const FLinearColor& C)
	{
		const TArray<FVector> Pts = WRForestCrMesh::Arc(Base, Out, CurlTo, Len, 300.f, 12);
		WRForestCrMesh::Horn(M, Pts, R0, R1, 1.f, FVector::CrossProduct(Out, CurlTo).GetSafeNormal(), C, 0.95f, 6);
	}

	/** A flat spot or blotch lying on a skin whose outward normal is Normal (an ellipse Len x Wide, Thick through). */
	void Patch(FWRToolMesh& M, const FVector& At, const FVector& Normal, const FVector& Along, float Len, float Wide, float Thick, const FLinearColor& C, float Rough)
	{
		const FVector N = Normal.GetSafeNormal();
		FVector A = (Along - N * FVector::DotProduct(Along, N)).GetSafeNormal();
		if (A.IsNearlyZero()) A = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
		if (A.IsNearlyZero()) A = FVector::ForwardVector;
		const FVector W = FVector::CrossProduct(N, A);
		WRForestCrMesh::Blob(M, At + N * Thick * 0.25f, A, Len, Wide, Thick, W, C, Rough, 5, 10);
	}

	/** A palmate antler (the Elderelk's, a Dappledoe buck's): a beam out and back from the skull, a brow tine, a broad palm
	 *  with tines along its outer edge; ivy winds up the beam with leaves when bIvy. Local: X forward, Y right, Z up. */
	void Antler(FWRToolMesh& M, float Side, float S, bool bIvy, const FLinearColor& Bone, const FLinearColor& Ivy, const FLinearColor& Moss)
	{
		const FVector BeamDir = FVector(-0.35f, Side * 0.8f, 0.55f).GetSafeNormal();
		const TArray<FVector> Beam = WRForestCrMesh::Arc(FVector::ZeroVector, BeamDir, FVector(0.f, 0.f, 1.f), 85.f * S, 25.f, 8);
		WRForestCrMesh::Horn(M, Beam, 5.5f * S, 3.4f * S, 0.9f, FVector(1.f, 0.f, 0.f), Bone, 0.6f, 9);
		// The brow tine, forward over the eye.
		WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(Beam[1], FVector(1.f, Side * 0.3f, 0.25f), FVector(0.f, 0.f, 1.f), 30.f * S, 25.f, 5), 3.f * S, 0.5f * S, 1.f, FVector(0.f, 1.f, 0.f), Bone, 0.6f);
		// The palm: broad and flat, swept out and back, its face turned up and out.
		const FVector Pt = Beam.Last();
		const FVector PalmDir = FVector(-0.35f, Side * 0.85f, 0.4f).GetSafeNormal();
		const FVector Edge = (FVector::CrossProduct(FVector(0.f, Side, 0.f), PalmDir).GetSafeNormal() + FVector(0.f, 0.f, 0.6f)).GetSafeNormal();
		const float PalmLen = 95.f * S, PalmW = 55.f * S;
		WRForestCrMesh::Blade(M, Pt, PalmDir, Edge, PalmLen, PalmW, 4.f * S, 0.55f, Bone, 0.6f, 6.f * S, FVector(0.f, 0.f, -1.f));
		// Tines along the outer (upper) edge of the palm.
		for (int32 t = 0; t < 6; ++t)
		{
			const float A = 0.18f + 0.14f * t;
			const FVector Base = Pt + PalmDir * PalmLen * A + Edge * PalmW * 0.42f * FMath::Sin(PI * FMath::Min(0.95f, A + 0.1f));
			const float Len = (18.f + 9.f * FMath::Sin(PI * A)) * S;
			WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(Base, (Edge + PalmDir * 0.35f).GetSafeNormal(), FVector(0.f, 0.f, 1.f), Len, 20.f, 4), 2.6f * S, 0.4f * S, 1.f, PalmDir, Bone, 0.6f, 6);
		}
		if (!bIvy)
		{
			return;
		}
		// Ivy winds up the beam onto the palm, a leaf every few centimetres; a moss tuft in the palm's hollow.
		TArray<FVector> Vine;
		for (int32 i = 0; i <= 24; ++i)
		{
			const float T = i / 24.f;
			const int32 Seg = FMath::Min(Beam.Num() - 2, int32(T * (Beam.Num() - 1)));
			const float F = T * (Beam.Num() - 1) - Seg;
			const FVector On = FMath::Lerp(Beam[Seg], Beam[Seg + 1], F);
			const FVector Dir = (Beam[Seg + 1] - Beam[Seg]).GetSafeNormal();
			const FVector U = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
			const FVector V = FVector::CrossProduct(Dir, U);
			const float Ang = T * 4.f * PI;
			Vine.Add(On + (U * FMath::Cos(Ang) + V * FMath::Sin(Ang)) * 6.5f * S);
		}
		M.Tube(Vine, 0.9f * S, 5, Ivy * 0.8f, 0.8f);
		for (int32 i = 1; i < Vine.Num(); i += 2)
		{
			const FVector Out = (Vine[i] - Vine[i - 1]).GetSafeNormal();
			const FVector Across = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal();
			WRForestCrMesh::Blade(M, Vine[i], (Across * (i % 4 == 1 ? 1.f : -1.f) + FVector(0.f, 0.f, -0.4f)).GetSafeNormal(), Out, 7.f * S, 5.f * S, 0.4f, 0.45f, Ivy * (0.85f + 0.3f * Jit(Vine[i], i)), 0.75f);
		}
		WRForestCrMesh::Blob(M, Pt + PalmDir * PalmLen * 0.3f + FVector(0.f, 0.f, 3.f * S), PalmDir, 22.f * S, 12.f * S, 5.f * S, Edge, Moss, 0.95f);
	}

	/** One finch of a flock: a goldfinch body (iridescent green-gold back, pale belly, violet throat, red face, a tiny
	 *  black beak, a forked tail), 11 cm long, along local +X. */
	void Finch(FWRToolMesh& M, float S)
	{
		using WRForestCrImpl::Hex;
		WRForestCrMesh::Blob(M, FVector(0.f, 0.f, 0.4f) * S, FVector::ForwardVector, 9.f * S, 2.9f * S, 2.6f * S, FVector::RightVector, Hex(TEXT("#6AA040")), 0.25f);
		WRForestCrMesh::Blob(M, FVector(0.5f, 0.f, -0.6f) * S, FVector::ForwardVector, 7.f * S, 2.4f * S, 1.6f * S, FVector::RightVector, Hex(TEXT("#E0C870")), 0.6f);
		WRForestCrMesh::Blob(M, FVector(4.6f, 0.f, 0.9f) * S, FVector::ForwardVector, 3.6f * S, 1.9f * S, 1.9f * S, FVector::RightVector, Hex(TEXT("#5A9038")), 0.25f);
		WRForestCrMesh::Blob(M, FVector(5.0f, 0.f, -0.2f) * S, FVector::ForwardVector, 2.f * S, 1.3f * S, 1.0f * S, FVector::RightVector, Hex(TEXT("#7A40A0")), 0.2f);
		WRForestCrMesh::Blob(M, FVector(6.0f, 0.f, 1.0f) * S, FVector::ForwardVector, 1.4f * S, 1.5f * S, 1.1f * S, FVector::RightVector, Hex(TEXT("#C02A20")), 0.4f);
		WRForestCrMesh::Horn(M, { FVector(6.4f, 0.f, 0.7f) * S, FVector(7.6f, 0.f, 0.5f) * S }, 0.5f * S, 0.08f * S, 0.8f, FVector::RightVector, Hex(TEXT("#18140F")), 0.4f, 5);
		for (const float Side : { -1.f, 1.f })
		{
			WRForestCrMesh::Blade(M, FVector(-4.f, Side * 0.3f, 0.6f) * S, FVector(-1.f, Side * 0.25f, 0.05f).GetSafeNormal(), FVector::RightVector, 4.5f * S, 1.6f * S, 0.25f * S, 0.6f, Hex(TEXT("#3E5A28")), 0.5f);
		}
	}

	FWRToolMesh FinchWing(float Side, float S)
	{
		FWRToolMesh M;
		WRForestCrMesh::Blade(M, FVector::ZeroVector, FVector(-0.2f, Side, 0.05f).GetSafeNormal(), FVector::ForwardVector, 6.f * S, 2.6f * S, 0.25f * S, 0.35f, WRForestCrImpl::Hex(TEXT("#4E7A2E")), 0.3f);
		return M;
	}
}

USceneComponent* UWRForestBody::AddPart(FName Bone, const FVector& WorldAt, const FRotator& WorldRot, const FWRToolMesh& Mesh, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || Mesh.Verts.Num() == 0)
	{
		return nullptr;
	}
	// A frame riding the bone (keeping its world place and a world scale of 1 under the fitted mesh), the mesh in it.
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Body, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
	Frame->SetWorldScale3D(FVector(1.f));
	Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
	UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(C);
	PM->SetupAttachment(Frame);
	PM->RegisterComponent();
	PM->SetCanEverAffectNavigation(false);
	Mesh.ApplyTo(PM);
	LookComps.Add(Frame);
	LookComps.Add(PM);
	FPart Part;
	Part.Frame = Frame;
	Part.Kind = Kind;
	Part.BaseRot = Frame->GetRelativeRotation();
	Part.BaseLoc = Frame->GetRelativeLocation();
	Part.BaseScale = Frame->GetRelativeScale3D();
	Parts.Add(Part);
	return Frame;
}

USceneComponent* UWRForestBody::AddPartTo(USceneComponent* Parent, const FVector& WorldAt, const FRotator& WorldRot, const FWRToolMesh& Mesh, int32 Kind)
{
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	if (!C || !Parent || Mesh.Verts.Num() == 0)
	{
		return nullptr;
	}
	// Its own frame under another part's (a flock finch's wing rides its body).
	USceneComponent* Frame = NewObject<USceneComponent>(C);
	Frame->RegisterComponent();
	Frame->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
	Frame->SetWorldScale3D(FVector(1.f));
	Frame->SetWorldLocationAndRotation(WorldAt, WorldRot);
	UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(C);
	PM->SetupAttachment(Frame);
	PM->RegisterComponent();
	PM->SetCanEverAffectNavigation(false);
	Mesh.ApplyTo(PM);
	LookComps.Add(Frame);
	LookComps.Add(PM);
	FPart Part;
	Part.Frame = Frame;
	Part.Kind = Kind;
	Part.BaseRot = Frame->GetRelativeRotation();
	Part.BaseLoc = Frame->GetRelativeLocation();
	Part.BaseScale = Frame->GetRelativeScale3D();
	Parts.Add(Part);
	return Frame;
}

void UWRForestBody::ShowParts(int32 Kind, bool bShow)
{
	for (FPart& Part : Parts)
	{
		if (Part.Kind != Kind || Part.bShown == bShow || !Part.Frame.IsValid()) continue;
		Part.bShown = bShow;
		Part.Frame->SetVisibility(bShow, true);
	}
}

void UWRForestBody::ClearLook()
{
	for (USceneComponent* Comp : LookComps) if (Comp) Comp->DestroyComponent();
	LookComps.Reset();
	Parts.Reset();
	if (LoopFx) { WRFx::Stop(LoopFx); LoopFx = nullptr; }
	if (LiftFx) { WRFx::Stop(LiftFx); LiftFx = nullptr; }
	if (BloomFx) { WRFx::Stop(BloomFx); BloomFx = nullptr; }
	if (USkeletalMeshComponent* Body = Skel(); Body && bAnimPaused)
	{
		Body->bPauseAnims = false;
		bAnimPaused = false;
	}
	LookFor = NAME_None;
}

void UWRForestBody::ApplyFit()
{
	// The forest's fit over the creature's own (which stands the mesh on the capsule at its row's height): the Thornlynx's
	// low long cat and the Rootcoil's 12 m log of a body come from the borrowed rig squashed (look.flatten cm tall) and
	// lengthened along its spine (look.stretch). The clips keep their strides.
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset())
	{
		return;
	}
	const FVector Cur = Body->GetRelativeScale3D();
	if (!FitScale.IsZero() && Cur.Equals(FitScale, 0.0005f))
	{
		return;   // ours already
	}
	const TSharedPtr<FJsonObject> Look = WRForestCrImpl::Look(C->CreatureId);
	const float Flat = WRForestCrImpl::Num(Look, TEXT("flatten"), 0.f);
	const float Stretch = WRForestCrImpl::Num(Look, TEXT("stretch"), 1.f);
	FVector S = Cur;
	FVector Loc = Body->GetRelativeLocation();
	const FBox Box = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
	if (Stretch > 0.f && !FMath::IsNearlyEqual(Stretch, 1.f))
	{
		const FVector MeshFwd = FRotator(0.f, -Def->MeshYaw, 0.f).RotateVector(FVector::ForwardVector);
		const bool bAlongX = FMath::Abs(MeshFwd.X) >= FMath::Abs(MeshFwd.Y);
		const float Old = bAlongX ? S.X : S.Y;
		const float New = Old * Stretch;
		// Keep the bounds' middle where it was: the capsule stays in the middle of the longer body.
		const FVector Shift = bAlongX ? FVector(Box.GetCenter().X * (Old - New), 0.f, 0.f) : FVector(0.f, Box.GetCenter().Y * (Old - New), 0.f);
		Loc += Body->GetRelativeRotation().RotateVector(Shift);
		if (bAlongX) S.X = New; else S.Y = New;
	}
	if (Flat > 0.f)
	{
		const float Height = FMath::Max(1.f, (Box.Max.Z - Box.Min.Z) * S.Z);
		const float K = FMath::Clamp(Flat / Height, 0.1f, 1.f);
		const float NewZ = S.Z * K;
		// The fit stood its bottom on the capsule's: it stays there.
		Loc.Z -= Box.Min.Z * (NewZ - S.Z);
		S.Z = NewZ;
	}
	Body->SetRelativeScale3D(S);
	Body->SetRelativeLocation(Loc);
	FitScale = S;
	SkelBaseLoc = Body->GetRelativeLocation();
	SkelBaseRot = Body->GetRelativeRotation();
	SkelBaseScale = S;
	bSkelBase = true;
}

void UWRForestBody::BuildLook()
{
	using WRForestCrImpl::Hex;
	namespace P = WRForestCrPart;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	const FWRCreatureDef* Def = C ? C->GetDef() : nullptr;
	USkeletalMeshComponent* Body = Skel();
	if (!Def || !Body || !Body->GetSkeletalMeshAsset() || !Body->IsVisible())
	{
		return;
	}
	ClearLook();
	FitScale = FVector::ZeroVector;
	bSkelBase = false;
	ApplyFit();
	if (!bSkelBase)
	{
		SkelBaseLoc = Body->GetRelativeLocation();
		SkelBaseRot = Body->GetRelativeRotation();
		SkelBaseScale = Body->GetRelativeScale3D();
		FitScale = SkelBaseScale;
		bSkelBase = true;
	}
	LookFor = C->CreatureId;
	LookSex = C->TameRep.Sex;
	const TSharedPtr<FJsonObject> Look = WRForestCrImpl::Look(C->CreatureId);

	// Its palette (the cook's colours) over the rig's own texture: M_CreatureTint, two tones from the texture's light.
	TintMats.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Tints = nullptr;
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Creatures/M_CreatureTint.M_CreatureTint"));
	if (Look.IsValid() && Master && Look->TryGetArrayField(TEXT("tint"), Tints))
	{
		const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			const FString SlotName = SlotNames.IsValidIndex(Slot) ? SlotNames[Slot].ToString().ToLower() : FString();
			TSharedPtr<FJsonObject> Pick;
			for (const TSharedPtr<FJsonValue>& V : *Tints)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString Match;
				if (!O.IsValid()) continue;
				if (O->TryGetStringField(TEXT("match"), Match) && !Match.IsEmpty())
				{
					if (SlotName.Contains(Match.ToLower())) { Pick = O; break; }
				}
				else if (!Pick.IsValid())
				{
					Pick = O;
				}
			}
			if (!Pick.IsValid()) continue;
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Master, C);
			const auto SetTex = [&Pick, MID](const TCHAR* Field, const TCHAR* Param, const TCHAR* Fallback)
			{
				FString TexPath;
				const FString PathNow = Pick->TryGetStringField(Field, TexPath) ? TexPath : FString(Fallback);
				if (UTexture* Tex = PathNow.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *PathNow)) MID->SetTextureParameterValue(Param, Tex);
			};
			SetTex(TEXT("tex"), TEXT("BaseColorTex"), TEXT(""));
			SetTex(TEXT("normal"), TEXT("NormalTex"), TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
			SetTex(TEXT("glowTex"), TEXT("GlowTex"), TEXT("/Engine/EngineResources/Black.Black"));
			MID->SetScalarParameterValue(TEXT("Desaturation"), WRForestCrImpl::Num(Pick, TEXT("desat"), 1.f));
			MID->SetScalarParameterValue(TEXT("Brightness"), WRForestCrImpl::Num(Pick, TEXT("bright"), 1.f));
			MID->SetScalarParameterValue(TEXT("TwoTone"), WRForestCrImpl::Num(Pick, TEXT("twoTone"), 0.f));
			MID->SetScalarParameterValue(TEXT("LightFrom"), WRForestCrImpl::Num(Pick, TEXT("lightFrom"), 0.f));
			MID->SetScalarParameterValue(TEXT("Roughness"), WRForestCrImpl::Num(Pick, TEXT("rough"), 0.75f));
			MID->SetScalarParameterValue(TEXT("GlowStrength"), WRForestCrImpl::Num(Pick, TEXT("glowStrength"), 0.f));
			MID->SetVectorParameterValue(TEXT("Tint"), WRForestCrImpl::Color(Pick, TEXT("tint"), FLinearColor::White));
			MID->SetVectorParameterValue(TEXT("TintLight"), WRForestCrImpl::Color(Pick, TEXT("light"), WRForestCrImpl::Color(Pick, TEXT("tint"), FLinearColor::White)));
			MID->SetVectorParameterValue(TEXT("GlowColor"), WRForestCrImpl::Color(Pick, TEXT("glow"), Hex(TEXT("#2E5A1E"))));
			Body->SetMaterial(Slot, MID);
			TintMats.Add(MID);
		}
	}

	// The silhouette parts, modelled at its size on its own skin (the physics asset's shapes) and bones.
	const float Rad = Def->Radius;
	const FRotator Yaw(0.f, C->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = Yaw.Vector(), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Up = FVector::UpVector;
	const FBox AssetBox = Body->GetSkeletalMeshAsset()->GetBounds().GetBox();
	const FVector BS = Body->GetRelativeScale3D();
	const float BodyH = FMath::Max(10.f, (AssetBox.Max.Z - AssetBox.Min.Z) * BS.Z);
	const float H = BodyH * 0.5f;   // half the drawn body's height: what the parts are measured in
	const float FloorZ = C->GetActorLocation().Z - C->GetSimpleCollisionHalfHeight();
	const float TopZ = FloorZ + BodyH;
	WRForestCrShape::FBodySkin Skin;
	Skin.Build(Body);
	// Bones whose name holds Hint (not end / nub bones), with where they are, front first.
	const auto BonesWith = [&](const TCHAR* Hint) -> TArray<TPair<FName, FVector>>
	{
		TArray<TPair<FName, FVector>> Out;
		const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			const FString N = Ref.GetBoneName(i).ToString().ToLower();
			if (!N.Contains(Hint) || N.Contains(TEXT("end")) || N.Contains(TEXT("nub"))) continue;
			Out.Add({ Ref.GetBoneName(i), Body->GetBoneLocation(Ref.GetBoneName(i)) });
		}
		Out.Sort([Fwd](const TPair<FName, FVector>& A, const TPair<FName, FVector>& B) { return FVector::DotProduct(A.Value, Fwd) > FVector::DotProduct(B.Value, Fwd); });
		return Out;
	};
	// The body's front (Sign 1) or back (-1) at a height: the mesh's bounds along its facing.
	const auto FrontAt = [&](float Z, float Sign) -> FVector
	{
		const FBoxSphereBounds B = Body->Bounds;
		const float Ext = FMath::Abs(Fwd.X) * B.BoxExtent.X + FMath::Abs(Fwd.Y) * B.BoxExtent.Y;
		const float Along = FVector::DotProduct(B.Origin - C->GetActorLocation(), Fwd) + Ext * Sign;
		FVector Pt = C->GetActorLocation() + Fwd * Along;
		Pt.Z = Z;
		return Pt;
	};
	// Where a line down from above meets the back, and in from a side meets the flank.
	const auto TopOf = [&](const FVector& Pt) { return Skin.Surface(FVector(Pt.X, Pt.Y, TopZ + 300.f), FVector(Pt.X, Pt.Y, FloorZ), FVector(Pt.X, Pt.Y, Pt.Z + H * 0.3f)); };
	const auto SideOf = [&](const FVector& Pt, const FVector& Dir) { return Skin.Surface(Pt + Dir * 800.f, Pt, Pt + Dir * H * 0.45f); };
	// World into a part's frame (X forward, Y right, Z up at Anchor).
	const auto Local = [Yaw](const FVector& World, const FVector& Anchor) { return Yaw.UnrotateVector(World - Anchor); };
	const auto Put = [this, Yaw](FName Bone, const FVector& At, int32 Kind, TFunctionRef<void(FWRToolMesh&)> Fn) -> USceneComponent*
	{
		FWRToolMesh Mesh;
		Fn(Mesh);
		return AddPart(Bone, At, Yaw, Mesh, Kind);
	};
	// The spine front to back (neck, chest, back, hips), without the head and the tail.
	TArray<TPair<FName, FVector>> Spine = BonesWith(TEXT("spine"));
	for (const TCHAR* Extra : { TEXT("neck"), TEXT("pelvis"), TEXT("hip") })
	{
		for (const TPair<FName, FVector>& B : BonesWith(Extra))
		{
			if (!Spine.ContainsByPredicate([&B](const TPair<FName, FVector>& S) { return S.Key == B.Key; })) Spine.Add(B);
		}
	}
	Spine.Sort([Fwd](const TPair<FName, FVector>& A, const TPair<FName, FVector>& B) { return FVector::DotProduct(A.Value, Fwd) > FVector::DotProduct(B.Value, Fwd); });
	if (Spine.Num() == 0)
	{
		// A rig with no named spine: points along the body's middle, on its root.
		for (int32 i = 0; i < 4; ++i) Spine.Add({ NAME_None, C->GetActorLocation() + Fwd * Rad * (0.6f - 0.4f * i) });
	}
	TArray<TPair<FName, FVector>> Feet = BonesWith(TEXT("foot"));
	if (Feet.Num() == 0) Feet = BonesWith(TEXT("hand"));
	if (Feet.Num() == 0) Feet = BonesWith(TEXT("paw"));
	FVector HeadPos = FrontAt(TopZ - H * 0.3f, 1.f) - Fwd * H * 0.3f;
	const FName HeadBone = C->FindBone(TEXT("head"));
	if (!HeadBone.IsNone()) HeadPos = Body->GetBoneLocation(HeadBone);
	const FName K = KindId();

	if (K == TEXT("mossmaul"))
	{
		const FLinearColor Moss = Hex(TEXT("#4E6B2A")), MossDark = Hex(TEXT("#3A5220")), MossLight = Hex(TEXT("#6A8A3A"));
		const FLinearColor Fungus = Hex(TEXT("#D8C8A0")), FungusUnder = Hex(TEXT("#A89A78")), ClawCol = Hex(TEXT("#1A1612")), Fern = Hex(TEXT("#5E8C31"));
		// The hump: a hill of moss over the shoulders (the second spine point from the front), cushions over it, fern
		// fronds arching off its crest and shelf fungus stepping down its flanks.
		const TPair<FName, FVector> Sh = Spine[FMath::Min(1, Spine.Num() - 1)];
		const FVector ShTop = TopOf(Sh.Value);
		const float HL = H * 1.1f, HW = H * 0.5f, HH = H * 0.42f;
		Put(Sh.Key, ShTop, P::Static, [&](FWRToolMesh& M)
		{
			WRForestCrMesh::Blob(M, FVector(0.f, 0.f, H * 0.16f), FVector::ForwardVector, HL, HW, HH, FVector::RightVector, Moss, 0.97f, 9, 12);
			for (int32 i = 0; i < 16; ++i)
			{
				const float X = (WRForestCrLook::Jit(ShTop, i) - 0.5f) * HL * 0.8f, Y = (WRForestCrLook::Jit(ShTop, i + 40) - 0.5f) * HW * 1.5f;
				const float Under = 1.f - FMath::Square(X / (HL * 0.5f)) - FMath::Square(Y / HW);
				if (Under <= 0.05f) continue;
				const float Z = H * 0.16f + HH * FMath::Sqrt(Under) - H * 0.03f;
				const FLinearColor Col = i % 3 == 0 ? MossLight : i % 3 == 1 ? MossDark : Moss;
				WRForestCrMesh::Blob(M, FVector(X, Y, Z), FVector(1.f, 0.3f * (WRForestCrLook::Jit(ShTop, i + 7) - 0.5f), 0.f), H * (0.18f + 0.1f * WRForestCrLook::Jit(ShTop, i + 3)), H * 0.13f, H * 0.07f, FVector::RightVector, Col, 0.98f, 5, 8);
			}
			for (int32 f = 0; f < 7; ++f)
			{
				const float A = FMath::DegreesToRadians(f * 360.f / 7.f + 20.f);
				const FVector Base(FMath::Cos(A) * HL * 0.18f, FMath::Sin(A) * HW * 0.3f, H * 0.16f + HH * 0.9f);
				const FVector Out = FVector(FMath::Cos(A), FMath::Sin(A), 0.55f).GetSafeNormal();
				WRForestCrMesh::Frond(M, Base, Out, FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), H * (0.5f + 0.15f * WRForestCrLook::Jit(ShTop, f)), H * 0.13f, Fern, f % 3 == 2, f * 0.37f);
			}
			for (int32 s = 0; s < 6; ++s)
			{
				const float Side = s % 2 ? 1.f : -1.f;
				const FVector At(HL * (0.22f - 0.15f * (s / 2)), Side * HW * 0.86f, H * (0.22f + 0.08f * (s / 2)));
				WRForestCrMesh::Blob(M, At + FVector(0.f, Side * H * 0.05f, 0.f), FVector(0.f, Side, 0.f), H * 0.2f, H * 0.17f, H * 0.035f, FVector::ForwardVector, Fungus, 0.8f, 5, 10);
				WRForestCrMesh::Blob(M, At + FVector(0.f, Side * H * 0.04f, -H * 0.025f), FVector(0.f, Side, 0.f), H * 0.17f, H * 0.14f, H * 0.02f, FVector::ForwardVector, FungusUnder, 0.85f, 5, 10);
			}
		});
		// The mat goes on down the spine and over the hips: cushions and a fiddlehead on every point behind the hump.
		for (int32 b = 2; b < Spine.Num(); ++b)
		{
			const FVector Top = TopOf(Spine[b].Value);
			Put(Spine[b].Key, Top, P::Static, [&](FWRToolMesh& M)
			{
				for (int32 i = 0; i < 4; ++i)
				{
					const FVector At(H * 0.1f * (WRForestCrLook::Jit(Top, i) - 0.5f), (i - 1.5f) * H * 0.16f, -H * 0.02f + H * 0.03f * (1.5f - FMath::Abs(i - 1.5f)));
					WRForestCrMesh::Blob(M, At, FVector::ForwardVector, H * 0.3f, H * 0.12f, H * 0.06f, FVector::RightVector, i % 2 ? MossDark : Moss, 0.98f, 5, 8);
				}
				WRForestCrMesh::Frond(M, FVector(0.f, H * 0.05f, H * 0.04f), FVector(-0.4f, 0.5f, 0.8f).GetSafeNormal(), FVector(1.f, 0.f, 0.f), H * 0.28f, H * 0.08f, Fern, true, b * 0.53f);
			});
		}
		// The flowers and ferns a path-magic tame grows into its moss (hidden until a hand is on it).
		Put(Sh.Key, ShTop, P::Bloom, [&](FWRToolMesh& M)
		{
			for (int32 i = 0; i < 12; ++i)
			{
				const float A = FMath::DegreesToRadians(i * 30.f + 15.f * WRForestCrLook::Jit(ShTop, i + 90));
				const FVector Base(FMath::Cos(A) * HL * 0.3f, FMath::Sin(A) * HW * 0.55f, H * 0.16f + HH * 0.7f);
				const FVector Stem = Base + FVector(0.f, 0.f, H * 0.18f);
				WRForestCrMesh::Horn(M, { Base, Base + FVector(0.f, 0.f, H * 0.09f), Stem }, 0.6f, 0.4f, 1.f, FVector::RightVector, Fern * 0.8f, 0.8f, 5);
				const FLinearColor Petal = i % 3 == 0 ? Hex(TEXT("#F0E8C0")) : i % 3 == 1 ? Hex(TEXT("#E8C040")) : Hex(TEXT("#C8A0D8"));
				for (int32 p = 0; p < 5; ++p)
				{
					const float PA = FMath::DegreesToRadians(p * 72.f);
					WRForestCrMesh::Blade(M, Stem, FVector(FMath::Cos(PA), FMath::Sin(PA), 0.25f).GetSafeNormal(), FVector(-FMath::Sin(PA), FMath::Cos(PA), 0.f), H * 0.07f, H * 0.045f, 0.4f, 0.5f, Petal, 0.6f);
				}
				WRForestCrMesh::Blob(M, Stem + FVector(0.f, 0.f, 0.6f), FVector::UpVector, H * 0.03f, H * 0.025f, H * 0.025f, FVector::RightVector, Hex(TEXT("#C89020")), 0.7f, 4, 6);
				if (i % 2 == 0) WRForestCrMesh::Frond(M, Base, FVector(FMath::Cos(A), FMath::Sin(A), 1.2f).GetSafeNormal(), FVector(-FMath::Sin(A), FMath::Cos(A), 0.f), H * 0.32f, H * 0.09f, Fern * 1.15f, i % 4 == 0, i * 0.21f);
			}
		});
		ShowParts(P::Bloom, false);
		// The forearms' three hooked claws, 35 cm, curling under so it walks on its knuckles.
		for (int32 f = 0; f < FMath::Min(2, Feet.Num()); ++f)
		{
			Put(Feet[f].Key, Feet[f].Value, P::Claw, [&](FWRToolMesh& M)
			{
				for (int32 c = -1; c <= 1; ++c)
				{
					const FVector Base(H * 0.08f, c * H * 0.065f, -H * 0.02f);
					WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(Base, FVector(1.f, c * 0.12f, -0.15f), FVector(0.f, 0.f, -1.f), 35.f, 150.f, 9), 3.6f, 0.35f, 0.75f, FVector::RightVector, ClawCol, 0.35f, 8);
				}
			});
		}
	}
	else if (K == TEXT("thornlynx"))
	{
		const FLinearColor SpikeCol = Hex(TEXT("#141210")), Ivory = Hex(TEXT("#E8E0C8")), Blotch = Hex(TEXT("#2A2620")), Blotch2 = Hex(TEXT("#6A6048"));
		const FVector HeadTop = TopOf(HeadPos);
		// The lynx tufts grown into two long black spikes, swept back off the ears.
		for (const float Side : { -1.f, 1.f })
		{
			Put(HeadBone, HeadTop + Right * Side * H * 0.2f - Fwd * H * 0.1f, P::Static, [&](FWRToolMesh& M)
			{
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector::ZeroVector, FVector(-0.3f, Side * 0.3f, 1.f), FVector(-1.f, 0.f, 0.f), H * 0.42f, 25.f, 6), H * 0.06f, H * 0.004f, 0.7f, FVector::RightVector, SpikeCol, 0.5f, 6);
			});
		}
		// The sabre canines, longer for its size than any cat's, down past the chin.
		Put(HeadBone, FrontAt(HeadPos.Z - H * 0.12f, 1.f) - Fwd * H * 0.07f, P::Static, [&](FWRToolMesh& M)
		{
			for (const float Side : { -1.f, 1.f })
			{
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(-H * 0.02f, Side * H * 0.075f, -H * 0.02f), FVector(0.12f, 0.f, -1.f), FVector(-1.f, 0.f, 0.f), H * 0.24f, 18.f, 6), H * 0.032f, H * 0.003f, 0.7f, FVector::RightVector, Ivory, 0.3f, 7);
			}
		});
		// Cloud blotches down both flanks: dark-edged clouds with lighter middles.
		for (int32 b = 0; b < Spine.Num(); ++b)
		{
			const FVector Anchor = Spine[b].Value;
			Put(Spine[b].Key, Anchor, P::Static, [&](FWRToolMesh& M)
			{
				for (const float Side : { -1.f, 1.f })
				{
					for (int32 r = 0; r < 2; ++r)
					{
						const FVector Probe = Anchor + Fwd * H * 0.25f * (WRForestCrLook::Jit(Anchor, r + (Side > 0.f ? 10 : 20)) - 0.5f) + Up * H * (r == 0 ? 0.18f : -0.12f);
						const FVector Pt = SideOf(Probe, Right * Side);
						const FVector N = Local(Pt + Right * Side * 10.f, Anchor) - Local(Pt, Anchor);
						WRForestCrLook::Patch(M, Local(Pt, Anchor), N, FVector::ForwardVector, H * 0.36f, H * 0.21f, 1.4f, Blotch, 0.8f);
						WRForestCrLook::Patch(M, Local(Pt, Anchor) + N.GetSafeNormal() * 0.9f, N, FVector::ForwardVector, H * 0.2f, H * 0.11f, 1.2f, Blotch2, 0.8f);
					}
				}
			});
		}
	}
	else if (K == TEXT("rootcoil"))
	{
		const FLinearColor Bark = Hex(TEXT("#4A3A2A")), BarkDark = Hex(TEXT("#33281E")), Moss = Hex(TEXT("#4E6B2A")), Lichen = Hex(TEXT("#8A8A78")), Amber = Hex(TEXT("#C8901A"));
		TArray<TPair<FName, FVector>> Back = Spine;
		for (const TPair<FName, FVector>& T : BonesWith(TEXT("tail"))) Back.Add(T);
		for (int32 b = 0; b < Back.Num(); ++b)
		{
			const FVector Top = TopOf(Back[b].Value);
			const float Gap = b + 1 < Back.Num() ? float(FVector::Dist(Back[b].Value, Back[b + 1].Value)) : b > 0 ? float(FVector::Dist(Back[b].Value, Back[b - 1].Value)) : H;
			const float Seg = FMath::Clamp(Gap, H * 0.6f, H * 6.f);
			const bool bMid = b > 0 && b + 1 < Back.Num();
			Put(Back[b].Key, Top, bMid ? P::Breath : P::Static, [&](FWRToolMesh& M)
			{
				// Keeled scale plates in a dorsal row, like bark ridges; scale plates down both flanks.
				for (int32 k = 0; k < 3; ++k)
				{
					const float X = (k - 1) * Seg * 0.33f;
					WRForestCrMesh::Blade(M, FVector(X - Seg * 0.2f, 0.f, -H * 0.02f), FVector::ForwardVector, FVector::UpVector, Seg * 0.42f, H * 0.32f, 2.5f, 0.5f, k % 2 ? Bark : BarkDark, 0.95f);
					for (const float Side : { -1.f, 1.f })
					{
						WRForestCrMesh::Blob(M, FVector(X, Side * H * 0.38f, -H * 0.2f), FVector(1.f, Side * 0.15f, -0.2f), Seg * 0.3f, H * 0.18f, 2.2f, FVector(0.f, Side, -0.5f), k % 2 ? BarkDark : Bark, 0.92f, 4, 8);
					}
				}
				// A moss saddle over every other stretch, lichen-grey speckle on the rest.
				if (b % 2 == 0)
				{
					WRForestCrMesh::Blob(M, FVector(0.f, 0.f, H * 0.02f), FVector::ForwardVector, Seg * 0.7f, H * 0.62f, H * 0.16f, FVector::RightVector, Moss, 0.98f, 6, 12);
				}
				for (int32 s = 0; s < 5; ++s)
				{
					WRForestCrMesh::Blob(M, FVector(Seg * (WRForestCrLook::Jit(Top, s) - 0.5f) * 0.8f, H * (WRForestCrLook::Jit(Top, s + 5) - 0.5f) * 0.9f, H * 0.04f), FVector::ForwardVector, 3.5f, 2.5f, 0.8f, FVector::RightVector, Lichen, 0.9f, 3, 6);
				}
			});
		}
		// The wedge head: flat supraocular plates, amber eyes, heat pits along the lips.
		Put(HeadBone, TopOf(HeadPos), P::Static, [&](FWRToolMesh& M)
		{
			for (const float Side : { -1.f, 1.f })
			{
				WRForestCrMesh::Blade(M, FVector(-H * 0.45f, Side * H * 0.22f, 0.f), FVector(1.f, -Side * 0.22f, -0.05f).GetSafeNormal(), FVector(0.f, 1.f, 0.f), H * 0.95f, H * 0.36f, 2.f, 0.4f, BarkDark, 0.9f);
				WRForestCrMesh::Blob(M, FVector(H * 0.05f, Side * H * 0.4f, -H * 0.08f), FVector::ForwardVector, H * 0.14f, H * 0.09f, H * 0.08f, FVector::RightVector, Amber, 0.15f, 5, 8);
				for (int32 p = 0; p < 4; ++p)
				{
					WRForestCrMesh::Blob(M, FVector(H * (0.3f + 0.12f * p), Side * H * (0.36f - 0.04f * p), -H * 0.3f), FVector::ForwardVector, 2.2f, 1.4f, 1.f, FVector::RightVector, Hex(TEXT("#140F0A")), 0.6f, 3, 6);
				}
			}
		});
		// The coils that slide round a held body (shown only while it constricts): two and a half turns of a 50 cm body.
		const FVector HoldAt = FVector(C->GetActorLocation().X, C->GetActorLocation().Y, FloorZ) + Fwd * (Rad + WRForestCrImpl::Tn(TEXT("rootcoil.holdForward"), 220.f));
		Put(NAME_None, HoldAt, P::Coil, [&](FWRToolMesh& M)
		{
			TArray<FVector> Helix;
			for (int32 i = 0; i <= 60; ++i)
			{
				const float T = i / 60.f;
				const float A = T * 2.6f * 2.f * PI;
				Helix.Add(FVector(FMath::Cos(A) * 72.f, FMath::Sin(A) * 72.f, 18.f + T * 120.f));
			}
			M.Tube(Helix, 24.f, 12, Bark, 0.9f);
			for (int32 i = 4; i + 1 < Helix.Num(); i += 9)
			{
				WRForestCrMesh::Blob(M, Helix[i] + FVector(0.f, 0.f, 18.f), (Helix[i + 1] - Helix[i - 1]).GetSafeNormal(), 34.f, 16.f, 7.f, FVector::UpVector, Moss, 0.98f, 5, 8);
			}
		});
		ShowParts(P::Coil, false);
	}
	else if (K == TEXT("bramblewasp"))
	{
		const FLinearColor Orange = Hex(TEXT("#D07A1A")), Amber = Hex(TEXT("#C9A030")), Black = Hex(TEXT("#181410")), WingCol = Hex(TEXT("#5A4C40"));
		const FVector Front = FrontAt(C->GetActorLocation().Z, 1.f);
		const FVector Rear = FrontAt(C->GetActorLocation().Z, -1.f);
		// The big orange head, heavy mandibles, dark eyes and the antennae.
		Put(HeadBone, Front - Fwd * 1.4f, P::Static, [&](FWRToolMesh& M)
		{
			WRForestCrMesh::Blob(M, FVector::ZeroVector, FVector::ForwardVector, 2.8f, 1.7f, 1.5f, FVector::RightVector, Orange, 0.4f, 6, 10);
			for (const float Side : { -1.f, 1.f })
			{
				WRForestCrMesh::Blob(M, FVector(0.3f, Side * 1.25f, 0.35f), FVector::ForwardVector, 1.3f, 0.6f, 0.9f, FVector::UpVector, Hex(TEXT("#2A1A10")), 0.15f, 4, 6);
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(1.2f, Side * 0.6f, -0.6f), FVector(1.f, Side * 0.2f, -0.2f), FVector(0.f, -Side, 0.f), 1.5f, 70.f, 5), 0.38f, 0.07f, 0.7f, FVector::UpVector, Hex(TEXT("#3A2210")), 0.4f, 5);
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(0.6f, Side * 0.4f, 1.f), FVector(0.5f, Side * 0.5f, 1.f), FVector(1.f, 0.f, 0.f), 2.2f, 40.f, 5), 0.12f, 0.06f, 1.f, FVector::RightVector, Black, 0.5f, 4);
			}
		});
		// The banded abdomen and the finger-long stinger.
		Put(NAME_None, Rear + Fwd * 1.f, P::Static, [&](FWRToolMesh& M)
		{
			for (int32 s = 0; s < 5; ++s)
			{
				const float R = FMath::Lerp(1.6f, 0.7f, s / 4.f);
				WRForestCrMesh::Blob(M, FVector(-s * 1.05f, 0.f, -0.1f * s), FVector(1.f, 0.f, 0.1f), 1.45f, R, R * 0.92f, FVector::RightVector, s % 2 ? Black : Amber, 0.35f, 5, 10);
			}
			WRForestCrMesh::Horn(M, { FVector(-5.f, 0.f, -0.55f), FVector(-5.8f, 0.f, -0.75f), FVector(-6.5f, 0.f, -0.95f) }, 0.28f, 0.03f, 1.f, FVector::RightVector, Black, 0.3f, 5);
		});
		// Two smoky wing pairs off the thorax, beating to a blur.
		const FVector Thorax = (Front + Rear) * 0.5f + Fwd * 1.2f + Up * 1.6f;
		for (const float Side : { -1.f, 1.f })
		{
			for (int32 Pair = 0; Pair < 2; ++Pair)
			{
				if (Put(NAME_None, Thorax - Fwd * Pair * 1.f, P::Wing, [&](FWRToolMesh& M)
				{
					WRForestCrMesh::Blade(M, FVector(0.f, Side * 0.5f, 0.f), FVector(-0.35f, Side, 0.12f).GetSafeNormal(), FVector::ForwardVector, Pair ? 4.f : 5.6f, Pair ? 1.4f : 1.9f, 0.12f, 0.35f, WingCol, 0.25f);
				}))
				{
					Parts.Last().Phase = Side * (Pair ? 1.3f : 1.f);
				}
			}
		}
	}
	else if (K == TEXT("elderelk") || K == TEXT("dappledoe"))
	{
		const bool bElk = K == TEXT("elderelk");
		const FLinearColor BoneCol = Hex(TEXT("#D8CCB0")), Ivy = Hex(TEXT("#3E6A2A")), Moss = Hex(TEXT("#4E6B2A"));
		if (bElk || LookSex == 1)
		{
			// Palmate antlers: 3.6 m across with ivy and moss on the Elderelk; a buck Dappledoe's small and bare.
			const float S = bElk ? 1.f : WRForestCrImpl::Tn(TEXT("dappledoe.antlerScale"), 0.36f);
			for (const float Side : { -1.f, 1.f })
			{
				Put(HeadBone, HeadPos + Up * H * 0.2f + Right * Side * H * 0.1f, P::Static, [&](FWRToolMesh& M) { WRForestCrLook::Antler(M, Side, S, bElk, BoneCol, Ivy, Moss); });
			}
		}
		// The rump: the elk's pale patch; the doe's white, edged in black either side.
		const FVector Rump = SideOf(Spine.Last().Value + Up * H * 0.05f, -Fwd);
		Put(Spine.Last().Key, Rump, P::Static, [&](FWRToolMesh& M)
		{
			WRForestCrLook::Patch(M, FVector::ZeroVector, FVector(-1.f, 0.f, 0.f), FVector::UpVector, H * 0.5f, H * 0.38f, 2.f, Hex(bElk ? TEXT("#D8C8A8") : TEXT("#F2EEE6")), 0.85f);
			if (!bElk)
			{
				for (const float Side : { -1.f, 1.f })
				{
					WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(1.f, Side * H * 0.36f, H * 0.2f), FVector(-0.1f, 0.f, -1.f), FVector(0.f, -Side, 0.f), H * 0.42f, 40.f, 6), 1.6f, 1.2f, 0.6f, FVector::ForwardVector, Hex(TEXT("#141210")), 0.8f, 5);
				}
			}
		});
		if (bElk)
		{
			// The dark mane down the neck.
			const TPair<FName, FVector> Neck = Spine[0];
			const FVector Top = TopOf(Neck.Value);
			Put(Neck.Key, Top, P::Static, [&](FWRToolMesh& M)
			{
				for (int32 i = 0; i < 10; ++i)
				{
					const float X = (i - 4.5f) * H * 0.07f;
					for (const float Side : { -1.f, 1.f })
					{
						WRForestCrMesh::Blade(M, FVector(X, Side * H * 0.04f, 0.f), FVector(-0.2f, Side * 0.5f, -0.9f).GetSafeNormal(), FVector::ForwardVector, H * (0.28f + 0.08f * WRForestCrLook::Jit(Top, i)), H * 0.06f, 0.8f, 0.3f, Hex(TEXT("#2A1C12")), 0.9f);
					}
				}
			});
		}
		else
		{
			// White spots in rows down the flanks and back, and the ever-flicking tail.
			for (int32 b = 1; b < Spine.Num(); ++b)
			{
				const FVector Anchor = Spine[b].Value;
				Put(Spine[b].Key, Anchor, P::Static, [&](FWRToolMesh& M)
				{
					for (const float Side : { -1.f, 1.f })
					{
						for (int32 r = 0; r < 3; ++r)
						{
							for (int32 s = 0; s < 2; ++s)
							{
								const FVector Probe = Anchor + Fwd * H * 0.22f * (s - 0.5f + 0.3f * (WRForestCrLook::Jit(Anchor, r * 3 + s) - 0.5f)) + Up * H * (0.32f - 0.14f * r);
								const FVector Pt = r == 0 ? TopOf(Probe + Right * Side * H * 0.12f) : SideOf(Probe, Right * Side);
								const FVector N = r == 0 ? FVector(0.f, Side * 0.4f, 1.f) : Local(Pt + Right * Side * 10.f, Anchor) - Local(Pt, Anchor);
								WRForestCrLook::Patch(M, Local(Pt, Anchor), N, FVector::ForwardVector, H * 0.085f, H * 0.065f, 1.f, Hex(TEXT("#F4F0E6")), 0.8f);
							}
						}
					}
				});
			}
			Put(Spine.Last().Key, TopOf(Spine.Last().Value - Fwd * H * 0.1f), P::Tail, [&](FWRToolMesh& M)
			{
				WRForestCrMesh::Blob(M, FVector(-H * 0.14f, 0.f, 0.f), FVector(-1.f, 0.f, -0.5f), H * 0.3f, H * 0.07f, H * 0.05f, FVector::RightVector, Hex(TEXT("#5A2E14")), 0.8f, 5, 8);
				WRForestCrMesh::Blob(M, FVector(-H * 0.14f, 0.f, -H * 0.03f), FVector(-1.f, 0.f, -0.5f), H * 0.26f, H * 0.065f, H * 0.04f, FVector::RightVector, Hex(TEXT("#F4F0E6")), 0.8f, 5, 8);
			});
		}
	}
	else if (K == TEXT("burlbeast"))
	{
		const FLinearColor Plate = Hex(TEXT("#6A5A48")), PlateDark = Hex(TEXT("#4E4236")), Crack = Hex(TEXT("#3A3026")), Stocking = Hex(TEXT("#E8E4DA"));
		// Ridged bark-burl plates over the back and shoulders, biggest on the hump.
		for (int32 b = 0; b < Spine.Num(); ++b)
		{
			const FVector Top = TopOf(Spine[b].Value);
			const float Big = b <= 1 ? 1.3f : 1.f;
			Put(Spine[b].Key, Top, P::Static, [&](FWRToolMesh& M)
			{
				for (int32 p = 0; p < 3; ++p)
				{
					const FVector At(H * 0.18f * (p - 1) * Big, H * 0.12f * (p == 1 ? 0.f : (p == 0 ? -1.f : 1.f)), H * 0.04f);
					WRForestCrMesh::Blob(M, At, FVector(1.f, 0.1f * (p - 1), 0.f), H * 0.5f * Big, H * 0.32f * Big, H * 0.1f * Big, FVector::RightVector, p == 1 ? Plate : PlateDark, 0.92f, 6, 10);
					for (int32 r = 0; r < 3; ++r)
					{
						const float Y = (r - 1) * H * 0.1f * Big;
						WRForestCrMesh::Horn(M, { At + FVector(-H * 0.2f * Big, Y, H * 0.08f * Big), At + FVector(0.f, Y * 1.1f, H * 0.11f * Big), At + FVector(H * 0.2f * Big, Y, H * 0.08f * Big) }, H * 0.022f, H * 0.012f, 0.8f, FVector::RightVector, r == 1 ? Crack : PlateDark, 0.95f, 5);
					}
				}
			});
		}
		// White stockings on all four legs, from the hoof up past the fetlock.
		for (int32 f = 0; f < FMath::Min(4, Feet.Num()); ++f)
		{
			const FVector At = Feet[f].Value;
			const FVector Mid = At + Up * H * 0.2f;
			const float LegR = FMath::Clamp(float(FVector::Dist2D(SideOf(Mid, Right), Mid)), 5.f, H * 0.18f);
			Put(Feet[f].Key, At, P::Static, [&](FWRToolMesh& M)
			{
				WRForestCrMesh::Sweep(M, { FVector(0.f, 0.f, -H * 0.05f), FVector(0.f, 0.f, H * 0.2f), FVector(0.f, 0.f, H * 0.42f) }, { FVector2D(LegR * 1.12f, LegR * 1.12f), FVector2D(LegR * 1.08f, LegR * 1.08f), FVector2D(LegR * 1.05f, LegR * 1.05f) }, FVector::RightVector, 12, Stocking, 0.85f);
			});
		}
		// Horns: pale, curving up and forward, the tips dark.
		Put(HeadBone, TopOf(HeadPos), P::Static, [&](FWRToolMesh& M)
		{
			for (const float Side : { -1.f, 1.f })
			{
				const TArray<FVector> Pts = WRForestCrMesh::Arc(FVector(0.f, Side * H * 0.2f, -H * 0.02f), FVector(0.1f, Side, 0.25f), FVector(0.4f, 0.f, 1.f), H * 0.55f, 110.f, 10);
				TArray<FVector> RootPart, TipPart;
				for (int32 i = 0; i < Pts.Num(); ++i) { if (i <= 7) RootPart.Add(Pts[i]); if (i >= 7) TipPart.Add(Pts[i]); }
				WRForestCrMesh::Horn(M, RootPart, H * 0.075f, H * 0.03f, 0.9f, FVector::ForwardVector, Hex(TEXT("#D8D0BC")), 0.5f, 8);
				WRForestCrMesh::Horn(M, TipPart, H * 0.03f, H * 0.004f, 0.9f, FVector::ForwardVector, Hex(TEXT("#2A2420")), 0.4f, 8);
			}
		});
		// Moss a Mossmaul pulls over the carcass it caches (shown only on a larder kill).
		Put(NAME_None, FVector(C->GetActorLocation().X, C->GetActorLocation().Y, FloorZ), P::Larder, [&](FWRToolMesh& M)
		{
			WRForestCrMesh::Blob(M, FVector(0.f, 0.f, H * 0.35f), FVector::ForwardVector, Rad * 2.6f, Rad * 1.05f, H * 0.6f, FVector::RightVector, Hex(TEXT("#4E6B2A")), 0.98f, 9, 14);
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = FMath::DegreesToRadians(i * 36.f);
				WRForestCrMesh::Blob(M, FVector(FMath::Cos(A) * Rad * 0.9f, FMath::Sin(A) * Rad * 0.6f, H * 0.6f), FVector(FMath::Cos(A), FMath::Sin(A), 0.f), Rad * 0.5f, Rad * 0.3f, H * 0.15f, FVector::UpVector, Hex(i % 2 ? TEXT("#3A5220") : TEXT("#6A8A3A")), 0.98f, 5, 8);
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(FMath::Cos(A) * Rad * 0.5f, FMath::Sin(A) * Rad * 0.4f, H * 0.9f), FVector(FMath::Cos(A + 1.f), FMath::Sin(A + 1.f), 0.2f), FVector::UpVector, Rad * 0.7f, 30.f, 5), 3.f, 1.f, 1.f, FVector::UpVector, Hex(TEXT("#4A3A2A")), 0.95f, 5);
			}
		});
		ShowParts(P::Larder, false);
	}
	else if (K == TEXT("truffler"))
	{
		const FLinearColor SnoutSkin = Hex(TEXT("#3A3028")), Nose = Hex(TEXT("#2A2420")), Ivory = Hex(TEXT("#E8DCC0")), Crest = Hex(TEXT("#141210"));
		Put(HeadBone, FrontAt(HeadPos.Z - H * 0.08f, 1.f) - Fwd * H * 0.06f, P::Static, [&](FWRToolMesh& M)
		{
			// The long flexible rooting snout, drooping to a disc nose with two nostrils; short tusks either side.
			const TArray<FVector> Snout = WRForestCrMesh::Arc(FVector::ZeroVector, FVector(1.f, 0.f, -0.15f), FVector(0.f, 0.f, -1.f), H * 0.5f, 35.f, 7);
			WRForestCrMesh::Horn(M, Snout, H * 0.14f, H * 0.095f, 0.85f, FVector::RightVector, SnoutSkin, 0.85f, 10);
			const FVector Tip = Snout.Last(), Dir = (Snout.Last() - Snout[Snout.Num() - 2]).GetSafeNormal();
			WRForestCrMesh::Blob(M, Tip, Dir, H * 0.05f, H * 0.11f, H * 0.095f, FVector::RightVector, Nose, 0.5f, 4, 10);
			for (const float Side : { -1.f, 1.f })
			{
				WRForestCrMesh::Blob(M, Tip + Dir * H * 0.025f + FVector(0.f, Side * H * 0.04f, 0.f), Dir, H * 0.02f, H * 0.025f, H * 0.03f, FVector::RightVector, Hex(TEXT("#0A0806")), 0.6f, 3, 6);
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(H * 0.04f, Side * H * 0.12f, -H * 0.05f), FVector(0.55f, Side * 0.35f, 0.5f), FVector(-0.5f, 0.f, 1.f), H * 0.22f, 70.f, 6), H * 0.035f, H * 0.006f, 0.8f, FVector::ForwardVector, Ivory, 0.35f, 6);
			}
		});
		// The bristle crest down the neck and the front of the back.
		for (int32 b = 0; b < FMath::Max(1, Spine.Num() - 1); ++b)
		{
			const FVector Top = TopOf(Spine[b].Value);
			Put(Spine[b].Key, Top, P::Static, [&](FWRToolMesh& M)
			{
				for (int32 i = 0; i < 7; ++i)
				{
					const FVector Base((i - 3) * H * 0.06f, H * 0.02f * (WRForestCrLook::Jit(Top, i) - 0.5f), 0.f);
					WRForestCrMesh::Horn(M, { Base, Base + FVector(-H * 0.06f, 0.f, H * 0.1f), Base + FVector(-H * 0.12f, 0.f, H * 0.17f) }, H * 0.02f, H * 0.003f, 1.f, FVector::RightVector, Crest, 0.9f, 4);
				}
			});
		}
	}
	else if (K == TEXT("hiveback"))
	{
		const FLinearColor Black = Hex(TEXT("#1A1612")), Amber = Hex(TEXT("#D0A040")), White = Hex(TEXT("#E8E4DA")), Pollen = Hex(TEXT("#E0A030")), WingCol = Hex(TEXT("#4A4038"));
		const FVector Centre = Body->Bounds.Origin;
		// The fur barrel a metre long, black and amber bands to a white tail; the head, eyes and antennae; pollen baskets.
		Put(NAME_None, Centre, P::Static, [&](FWRToolMesh& M)
		{
			static const float Radii[] = { 24.f, 31.f, 34.f, 33.f, 29.f, 19.f };
			for (int32 s = 0; s < 6; ++s)
			{
				const FLinearColor Col = s == 5 ? White : (s % 2 == 0 ? Black : Amber);
				WRForestCrMesh::Blob(M, FVector(30.f - s * 15.f, 0.f, 0.f), FVector::ForwardVector, 19.f, Radii[s], Radii[s] * 0.92f, FVector::RightVector, Col, 0.97f, 6, 14);
			}
			WRForestCrMesh::Blob(M, FVector(48.f, 0.f, -2.f), FVector::ForwardVector, 20.f, 16.f, 15.f, FVector::RightVector, Black, 0.95f, 6, 12);
			for (const float Side : { -1.f, 1.f })
			{
				WRForestCrMesh::Blob(M, FVector(52.f, Side * 11.f, 3.f), FVector(0.6f, Side, 0.f), 9.f, 5.f, 8.f, FVector::UpVector, Hex(TEXT("#0E0C0A")), 0.1f, 5, 8);
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(55.f, Side * 5.f, 10.f), FVector(0.6f, Side * 0.3f, 1.f), FVector(1.f, 0.f, -0.2f), 20.f, 60.f, 6), 1.3f, 0.7f, 1.f, FVector::RightVector, Black, 0.6f, 5);
				WRForestCrMesh::Blob(M, FVector(-14.f, Side * 27.f, -24.f), FVector::ForwardVector, 11.f, 7.f, 7.f, FVector::RightVector, Pollen, 0.9f, 5, 8);
			}
		});
		for (const float Side : { -1.f, 1.f })
		{
			for (int32 Pair = 0; Pair < 2; ++Pair)
			{
				if (Put(NAME_None, Centre + Up * 30.f + Fwd * (12.f - Pair * 12.f), P::Wing, [&](FWRToolMesh& M)
				{
					WRForestCrMesh::Blade(M, FVector(0.f, Side * 8.f, 0.f), FVector(-0.3f, Side, 0.2f).GetSafeNormal(), FVector::ForwardVector, Pair ? 30.f : 42.f, Pair ? 13.f : 18.f, 0.6f, 0.4f, WingCol, 0.25f);
				}))
				{
					Parts.Last().Phase = Side * (Pair ? 1.25f : 1.f);
				}
			}
		}
	}
	else if (K == TEXT("fernsheep"))
	{
		// Fern-curl fleece: fiddlehead clusters over the back and down the flanks, grey-green.
		for (int32 b = 1; b < Spine.Num(); ++b)
		{
			const FVector Anchor = Spine[b].Value;
			Put(Spine[b].Key, Anchor, P::Static, [&](FWRToolMesh& M)
			{
				for (int32 s = -2; s <= 2; ++s)
				{
					const FVector Dir = (Up * (1.f - FMath::Abs(s) * 0.3f) + Right * s * 0.5f).GetSafeNormal();
					for (int32 j = 0; j < 2; ++j)
					{
						const FVector Probe = Anchor + Fwd * H * 0.2f * (j - 0.5f);
						const FVector Pt = Skin.Surface(Probe + Dir * 600.f, Probe, Probe + Dir * H * 0.5f);
						const FVector N = Yaw.UnrotateVector(Dir);
						const FLinearColor Col = Hex(TEXT("#7A8A6A")) * (0.8f + 0.35f * WRForestCrLook::Jit(Pt, s * 7 + j));
						WRForestCrLook::Curl(M, Local(Pt, Anchor), (N + FVector(0.4f * (WRForestCrLook::Jit(Pt, j) - 0.5f), 0.f, 0.f)).GetSafeNormal(), FVector::CrossProduct(N, FVector::ForwardVector).GetSafeNormal() * (j ? 1.f : -1.f), H * 0.24f, H * 0.07f, H * 0.035f, Col);
					}
				}
			});
		}
		// Swept amber horns curling back and down past the ears.
		Put(HeadBone, TopOf(HeadPos), P::Static, [&](FWRToolMesh& M)
		{
			for (const float Side : { -1.f, 1.f })
			{
				WRForestCrMesh::Horn(M, WRForestCrMesh::Arc(FVector(-H * 0.05f, Side * H * 0.16f, -H * 0.02f), FVector(-0.45f, Side * 0.4f, 0.6f), FVector(0.2f, 0.f, -1.f), H * 0.62f, 230.f, 12), H * 0.075f, H * 0.014f, 0.85f, FVector::ForwardVector, Hex(TEXT("#B8862E")), 0.55f, 8);
			}
		});
	}
	else if (K == TEXT("glimmerfinch"))
	{
		// The lead bird's face and wings (the rig is its body), then its flock round it.
		Put(HeadBone, FrontAt(HeadPos.Z, 1.f) - Fwd * 1.2f, P::Static, [&](FWRToolMesh& M)
		{
			WRForestCrMesh::Blob(M, FVector(0.f, 0.f, -1.2f), FVector::ForwardVector, 2.6f, 2.2f, 1.6f, FVector::RightVector, Hex(TEXT("#7A40A0")), 0.2f, 5, 8);
			WRForestCrMesh::Blob(M, FVector(0.4f, 0.f, 0.9f), FVector::ForwardVector, 1.6f, 2.f, 1.4f, FVector::RightVector, Hex(TEXT("#C02A20")), 0.4f, 5, 8);
			WRForestCrMesh::Horn(M, { FVector(0.8f, 0.f, 0.2f), FVector(2.2f, 0.f, -0.1f) }, 0.6f, 0.1f, 0.8f, FVector::RightVector, Hex(TEXT("#18140F")), 0.4f, 5);
		});
		for (const float Side : { -1.f, 1.f })
		{
			if (Put(NAME_None, Body->Bounds.Origin + Up * H * 0.3f + Right * Side * H * 0.4f, P::Wing, [&](FWRToolMesh& M)
			{
				WRForestCrMesh::Blade(M, FVector::ZeroVector, FVector(-0.25f, Side, 0.1f).GetSafeNormal(), FVector::ForwardVector, 8.f, 3.4f, 0.3f, 0.35f, Hex(TEXT("#6AA040")), 0.3f);
			}))
			{
				Parts.Last().Phase = Side;
			}
		}
		const int32 Count = FMath::Clamp(int32(WRForestCrImpl::Num(Look, TEXT("flock"), 9.f)), 0, 24);
		for (int32 i = 0; i < Count; ++i)
		{
			const float Seed = WRForestCrLook::Jit(FVector(i * 13.f, 7.f, 3.f), i);
			FWRToolMesh M;
			WRForestCrLook::Finch(M, 1.f);
			USceneComponent* Bird = AddPart(NAME_None, C->GetActorLocation() + FVector(FMath::Cos(Seed * 6.28f) * 80.f, FMath::Sin(Seed * 6.28f) * 80.f, 20.f), Yaw, M, P::Flock);
			if (!Bird) continue;
			const int32 BirdIndex = Parts.Num() - 1;
			Parts[BirdIndex].Phase = Seed * 2.f * PI;
			Parts[BirdIndex].Orbit = FVector(60.f + 110.f * Seed, -10.f + 60.f * WRForestCrLook::Jit(FVector(i, 1.f, 2.f), i + 3), (0.9f + 0.8f * WRForestCrLook::Jit(FVector(i, 5.f, 2.f), i + 9)) * (i % 2 ? 1.f : -1.f));
			for (const float Side : { -1.f, 1.f })
			{
				if (AddPartTo(Bird, Bird->GetComponentLocation() + Bird->GetComponentRotation().RotateVector(FVector(0.5f, Side * 2.f, 1.2f)), Bird->GetComponentRotation(), WRForestCrLook::FinchWing(Side, 1.f), P::FlockWing))
				{
					Parts.Last().Phase = Side * (1.f + Seed);
				}
			}
		}
	}
	UE_LOG(LogWildreach, Verbose, TEXT("FOREST look for %s: %d parts"), *C->GetName(), Parts.Num());
}

void UWRForestBody::TickLook(float DeltaTime)
{
	namespace P = WRForestCrPart;
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body)
	{
		return;
	}
	if (LookFor != C->CreatureId || (KindId() == TEXT("dappledoe") && LookSex != C->TameRep.Sex))
	{
		// Built once its mesh is on (the server may send the id, and a doe's sex, after this component).
		if (Body->GetSkeletalMeshAsset() && Body->IsVisible() && ++LookTries > 2) BuildLook();
		return;
	}
	// The creature's own fit ran again (a tame look, a baby growing): ours goes back on top.
	ApplyFit();
	TickPose(DeltaTime);
	const FName K = KindId();
	const bool bAlive = C->IsAlive();
	FxClock += DeltaTime;
	BloomAlpha = FMath::FInterpTo(BloomAlpha, Has(WRForestFlag::PathTame) && bAlive ? 1.f : 0.f, DeltaTime, 1.2f);
	SpreadAlpha = FMath::FInterpTo(SpreadAlpha, (Has(WRForestFlag::Reared) || Has(WRForestFlag::Holding)) && bAlive ? 1.f : 0.f, DeltaTime, 7.f);
	ShowParts(P::Bloom, BloomAlpha > 0.02f);
	ShowParts(P::Coil, bAlive && Has(WRForestFlag::Holding));
	ShowParts(P::Larder, !bAlive && Has(WRForestFlag::Larder));
	// A beat's sign: a Glimmerfinch flock's shimmer while it's lifted over something.
	const bool bLift = bAlive && Has(WRForestFlag::Lifted);
	if (bLift && !LiftFx) LiftFx = WRFx::Attach(C->GetRootComponent(), TEXT("forest.cr_shimmer"), NAME_None, 1.f);
	else if (!bLift && LiftFx) { WRFx::Stop(LiftFx); LiftFx = nullptr; }
	// The bloom's pollen while ferns grow into a Mossmaul's moss.
	const bool bBloomFx = bAlive && Has(WRForestFlag::PathTame);
	if (bBloomFx && !BloomFx) BloomFx = WRFx::Attach(C->GetRootComponent(), TEXT("forest.cr_bloom"), NAME_None, FMath::Max(0.5f, C->GetSimpleCollisionRadius() / 100.f));
	else if (!bBloomFx && BloomFx) { WRFx::Stop(BloomFx); BloomFx = nullptr; }
	// Moments on a clock: bark and leaves raining from a Thornlynx about to drop; earth thrown up by a rooting Truffler.
	if (bAlive && Has(WRForestFlag::DropTell) && FxClock > 0.12f)
	{
		FxClock = 0.f;
		WRFx::SpawnAt(C, TEXT("forest.cr_leaves"), C->GetActorLocation(), FRotator::ZeroRotator, 1.f);
	}
	else if (bAlive && Has(WRForestFlag::Rooting) && FxClock > 0.45f)
	{
		FxClock = 0.f;
		WRFx::SpawnAt(C, TEXT("forest.cr_dirt"), C->MouthLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() * 0.6f), C->GetActorRotation(), 0.7f);
	}
	// The moving parts.
	const float WingRate = K == TEXT("bramblewasp") ? 62.f : K == TEXT("hiveback") ? 48.f : 34.f;
	const float WingSwing = K == TEXT("glimmerfinch") ? 50.f : 32.f;
	const bool bFlying = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFlying();
	const FVector Centre = Body->Bounds.Origin;
	for (FPart& Part : Parts)
	{
		USceneComponent* F = Part.Frame.Get();
		if (!F) continue;
		switch (Part.Kind)
		{
		case P::Wing:
		{
			// A blur of wingbeats in the air, folded at rest; the dead lie still.
			const float WingBeat = bAlive ? (bFlying || K != TEXT("glimmerfinch") ? 1.f : 0.25f) : 0.f;
			const float Side = Part.Phase >= 0.f ? 1.f : -1.f;
			F->SetRelativeRotation(Part.BaseRot + FRotator(0.f, 0.f, Side * WingSwing * WingBeat * FMath::Sin(Clock * WingRate * FMath::Abs(Part.Phase))));
			break;
		}
		case P::Flock:
		{
			// Each finch on its own orbit round the lead bird: tight while it feeds, wide and high over a sign.
			const float Wide = bLift ? 2.6f : 1.f;
			const float A = Part.Phase + Clock * Part.Orbit.Z * (bLift ? 0.8f : 1.f);
			const float R = Part.Orbit.X * Wide;
			const FVector At = C->GetActorLocation() + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Part.Orbit.Y * Wide + 18.f * FMath::Sin(Clock * 2.3f + Part.Phase) + (bLift ? 120.f : 0.f));
			const FVector Tangent = FVector(-FMath::Sin(A), FMath::Cos(A), 0.f) * FMath::Sign(Part.Orbit.Z);
			F->SetVisibility(bAlive, true);
			F->SetWorldLocationAndRotation(At, Tangent.Rotation());
			break;
		}
		case P::FlockWing:
		{
			const float Side = Part.Phase >= 0.f ? 1.f : -1.f;
			F->SetRelativeRotation(Part.BaseRot + FRotator(0.f, 0.f, Side * 55.f * FMath::Sin(Clock * 38.f * FMath::Abs(Part.Phase))));
			break;
		}
		case P::Bloom:
			F->SetRelativeScale3D(Part.BaseScale * FMath::Max(0.001f, BloomAlpha));
			break;
		case P::Breath:
		{
			// The log breathes: a slow rise and fall down its middle (one breath every rootcoil.breathSeconds).
			const float Breath = Has(WRForestFlag::LogRest) ? 0.07f * FMath::Sin(Clock * 2.f * PI / FMath::Max(1.f, WRForestCrImpl::Tn(TEXT("rootcoil.breathSeconds"), 4.5f)) + Part.BaseLoc.X * 0.01f) : 0.f;
			F->SetRelativeScale3D(Part.BaseScale * FVector(1.f, 1.f + Breath * 0.5f, 1.f + Breath));
			break;
		}
		case P::Claw:
			// The claws uncurl and spread as the arm rises (rearing for the hammer, the hug).
			F->SetRelativeRotation(Part.BaseRot + FRotator(-40.f * SpreadAlpha, 0.f, 0.f));
			break;
		case P::Tail:
		{
			// Always flicking: a quick twitch every second or so.
			const float T = FMath::Fmod(Clock + Part.BaseLoc.X * 0.01f, 1.3f);
			const float Flick = bAlive && T < 0.35f ? FMath::Sin(T / 0.35f * 2.f * PI) : 0.f;
			F->SetRelativeRotation(Part.BaseRot + FRotator(25.f * Flick, 20.f * Flick, 0.f));
			break;
		}
		default:
			break;
		}
	}
	(void)Centre;
}

void UWRForestBody::TickPose(float DeltaTime)
{
	// The whole body's own pose over its clips (this ticks after the creature, so it holds over a wind-up's rear-back):
	// a Thornlynx hanging head-down on a trunk or climbing it, a Mossmaul reared on its hind legs, a Rootcoil still as a
	// log (its clips paused, breathing), a lynx knocked off its trunk lying on its side.
	AWRCreature* C = Cast<AWRCreature>(GetOwner());
	USkeletalMeshComponent* Body = Skel();
	if (!C || !Body || !bSkelBase)
	{
		return;
	}
	const bool bAlive = C->IsAlive();
	const FName K = KindId();
	HangAlpha = FMath::FInterpTo(HangAlpha, bAlive && Has(WRForestFlag::Perched) && !Has(WRForestFlag::Climbing) ? 1.f : 0.f, DeltaTime, 5.f);
	ClimbAlpha = FMath::FInterpTo(ClimbAlpha, bAlive && Has(WRForestFlag::Climbing) ? 1.f : 0.f, DeltaTime, 6.f);
	RearAlpha = FMath::FInterpTo(RearAlpha, bAlive && Has(WRForestFlag::Reared) && K == TEXT("mossmaul") ? 1.f : 0.f, DeltaTime, 5.f);
	RestAlpha = FMath::FInterpTo(RestAlpha, bAlive && Has(WRForestFlag::LogRest) ? 1.f : 0.f, DeltaTime, 3.f);
	DownAlpha = FMath::FInterpTo(DownAlpha, bAlive && Has(WRForestFlag::Downed) ? 1.f : 0.f, DeltaTime, 9.f);
	// Still as a log: its clips stop (it doesn't slither in place) once it's settled.
	const bool bPause = RestAlpha > 0.95f;
	if (bPause != bAnimPaused)
	{
		bAnimPaused = bPause;
		Body->bPauseAnims = bPause;
	}
	const bool bAny = HangAlpha > 0.01f || ClimbAlpha > 0.01f || RearAlpha > 0.01f || DownAlpha > 0.01f || RestAlpha > 0.01f;
	if (!bAny)
	{
		if (bPosing)
		{
			// Back to its fitted base, and held there a moment so a wind-up that began while it was posed can't restore
			// the posed transform after.
			bPosing = false;
			SettleUntil = Clock + 1.5f;
		}
		if (Clock < SettleUntil)
		{
			Body->SetRelativeLocationAndRotation(SkelBaseLoc, SkelBaseRot);
		}
		return;
	}
	bPosing = true;
	const float R = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HH = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// Head-down on the trunk (the belly against the bark: it faces out from the trunk), nose-up climbing, reared on the
	// hind feet, rolled onto its side.
	const float Pitch = -WRForestCrImpl::Tn(TEXT("thornlynx.hangPitch"), 82.f) * HangAlpha + WRForestCrImpl::Tn(TEXT("thornlynx.climbPitch"), 78.f) * ClimbAlpha
		+ WRForestCrImpl::Tn(TEXT("mossmaul.rearPitch"), 52.f) * RearAlpha;
	const float Roll = 75.f * DownAlpha;
	const FQuat Q(FRotator(Pitch, 0.f, Roll));
	// A rear pivots on the hind feet; the rest about the body's middle.
	const FVector Pivot = RearAlpha > 0.01f ? FVector(-R, 0.f, -HH) * RearAlpha : FVector::ZeroVector;
	const float Breath = RestAlpha * 1.5f * FMath::Sin(Clock * 2.f * PI / FMath::Max(1.f, WRForestCrImpl::Tn(TEXT("rootcoil.breathSeconds"), 4.5f)));
	Body->SetRelativeLocationAndRotation(Pivot + Q.RotateVector(SkelBaseLoc - Pivot) + FVector(0.f, 0.f, Breath), Q * FQuat(SkelBaseRot));
}

// ---------------------------------------------------------------- what the forest does to other bodies

TArray<TWeakObjectPtr<UWRForestTouch>> UWRForestTouch::Live;

UWRForestTouch::UWRForestTouch()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	SetIsReplicatedByDefault(true);
}

void UWRForestTouch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWRForestTouch, bWaspMark);
	DOREPLIFETIME(UWRForestTouch, Clinging);
	DOREPLIFETIME(UWRForestTouch, HeldBy);
}

UWRForestTouch* UWRForestTouch::Get(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UWRForestTouch>() : nullptr;
}

UWRForestTouch* UWRForestTouch::FindOrAdd(AActor* Actor)
{
	if (!Actor || !Actor->HasAuthority())
	{
		return Get(Actor);
	}
	if (UWRForestTouch* Have = Get(Actor))
	{
		return Have;
	}
	UWRForestTouch* Touch = NewObject<UWRForestTouch>(Actor, TEXT("ForestTouch"));
	Touch->RegisterComponent();
	Actor->AddInstanceComponent(Touch);
	return Touch;
}

void UWRForestTouch::OnRep_Mark()
{
	ShowMark(bWaspMark);
}

void UWRForestTouch::ShowMark(bool bOn)
{
	// The pheromone the scout fanned onto you: a pale dust that hangs round you while it lasts (everyone sees it: the wasps
	// are coming for whoever wears it).
	if (GetNetMode() == NM_DedicatedServer || bOn == bMarkShown || !GetOwner())
	{
		return;
	}
	bMarkShown = bOn;
	if (bOn && !MarkFx)
	{
		MarkFx = WRFx::Attach(GetOwner()->GetRootComponent(), TEXT("forest.cr_marked"), NAME_None, 1.f);
	}
	else if (!bOn && MarkFx)
	{
		WRFx::Stop(MarkFx);
		MarkFx = nullptr;
	}
}

void UWRForestTouch::EndPlay(const EEndPlayReason::Type Reason)
{
	if (MarkFx) { WRFx::Stop(MarkFx); MarkFx = nullptr; }
	if (HeatFx) { WRFx::Stop(HeatFx); HeatFx = nullptr; }
	Live.RemoveAll([this](const TWeakObjectPtr<UWRForestTouch>& T) { return !T.IsValid() || T.Get() == this; });
	Super::EndPlay(Reason);
}

void UWRForestTouch::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Me = GetOwner();
	if (!Me)
	{
		return;
	}
	if (!Live.ContainsByPredicate([this](const TWeakObjectPtr<UWRForestTouch>& T) { return T.Get() == this; }))
	{
		Live.Add(this);
	}
	// Machines that draw: the heat shimmer of a heat ball while bees cling.
	if (GetNetMode() != NM_DedicatedServer)
	{
		ShowMark(bWaspMark);
		if (Clinging > 0 && !HeatFx) HeatFx = WRFx::Attach(Me->GetRootComponent(), TEXT("forest.cr_heat"), NAME_None, 1.f);
		else if (Clinging == 0 && HeatFx) { WRFx::Stop(HeatFx); HeatFx = nullptr; }
	}
	if (!Me->HasAuthority())
	{
		return;
	}
	const double T = WRForestCrImpl::Now(this);
	AWRCharacterBase* Body = Cast<AWRCharacterBase>(Me);
	const bool bAlive = Body && Body->IsAlive();
	// The scout's mark: 30 s; water (wading or swimming) or smoke washes it off; it ends with you.
	if (bWaspMark)
	{
		bool bWashed = !bAlive || T >= MarkUntil;
		if (!bWashed)
		{
			const ACharacter* Ch = Cast<ACharacter>(Me);
			const UWRForestHazards* Hz = UWRForestHazards::Get(Me);
			bWashed = (Ch && Ch->GetCharacterMovement() && Ch->GetCharacterMovement()->IsSwimming()) || (Hz && Hz->InRiver(Me)) || WRForest::InSmoke(Me, Me->GetActorLocation());
			if (bWashed) WRForestCrImpl::Tell(Me, TEXT("The water takes the wasps' scent off you"));
		}
		if (bWashed)
		{
			WRForest::ClearWaspMark(Me);
		}
	}
	// The heat ball: a roll knocks one bee off (40%), fire on you knocks them all off.
	Bees.RemoveAll([](const TWeakObjectPtr<AWRCreature>& B) { return !B.IsValid() || !B->IsAlive(); });
	if (Bees.Num() > 0)
	{
		const bool bDodging = Body && Body->HasTag(WRTags::State_Dodging);
		if (Body && Body->HasTag(WRTags::Status_Burn))
		{
			TArray<TWeakObjectPtr<AWRCreature>> All = Bees;
			for (const TWeakObjectPtr<AWRCreature>& B : All) if (B.IsValid()) FWRForestAI::Uncling(B.Get(), true);
		}
		else if (bDodging && !bWasDodging && FMath::FRand() < WRForestCrImpl::Tn(TEXT("hiveback.rollOff"), 0.4f))
		{
			if (Bees.Num() > 0 && Bees.Last().IsValid()) FWRForestAI::Uncling(Bees.Last().Get(), true);
		}
		bWasDodging = bDodging;
	}
	const uint8 Want = uint8(FMath::Min(255, Bees.Num()));
	if (Clinging != Want)
	{
		Clinging = Want;
	}
	if (!bAlive && HeldBy)
	{
		if (AWRCreature* Holder = Cast<AWRCreature>(HeldBy.Get())) FWRForestAI::ReleaseHold(Holder, false);
		HeldBy = nullptr;
	}
}

// ---------------------------------------------------------------- the grips as the rest of the game sees them

bool WRForestGrip::IsHeld(const AActor* Who)
{
	const UWRForestTouch* Touch = UWRForestTouch::Get(Who);
	return Touch && Touch->HeldBy != nullptr && Cast<AWRCreature>(Touch->HeldBy.Get()) != nullptr && !FWRForestAI::IsWasp(Who);
}

void WRForestGrip::Mash(AActor* Who)
{
	if (!Who || !Who->HasAuthority() || !IsHeld(Who))
	{
		return;
	}
	const UWRForestTouch* Touch = UWRForestTouch::Get(Who);
	AWRCreature* Holder = Touch ? Cast<AWRCreature>(Touch->HeldBy.Get()) : nullptr;
	UWRForestBody* Body = UWRForestBody::Get(Holder);
	if (!Body || Body->Hold.Get() != Who)
	{
		return;
	}
	// Each press takes a little off the hold, never below its minimum.
	const FString Group = Body->KindId().ToString();
	const float Off = FWRTuning::Get().Num(FString::Printf(TEXT("creatureAI.%s.mashSeconds"), *Group), 0.15f);
	const float Min = FWRTuning::Get().Num(FString::Printf(TEXT("creatureAI.%s.mashMinSeconds"), *Group), 0.6f);
	Body->HoldEnds = FMath::Max(Body->HoldStart + Min, Body->HoldEnds - Off);
}

float WRForestGrip::MoveScale(const AActor* Who)
{
	const UWRForestTouch* Touch = UWRForestTouch::Get(Who);
	if (!Touch)
	{
		return 1.f;
	}
	if (Touch->HeldBy != nullptr)
	{
		return 0.f;
	}
	// Each clinging Hiveback slows you 10% (at most 4 cling).
	return Touch->Clinging > 0 ? FMath::Max(0.2f, 1.f - WRForestCrImpl::Tn(TEXT("hiveback.slowPerBee"), 0.1f) * Touch->Clinging) : 1.f;
}

// ---------------------------------------------------------------- the forest's shared calls (WRForestTypes.h)

namespace WRForestCrImpl
{
	/** Swarming nests from a mark calls the mark again (the nest's own swarm marks its stirrer): once is enough. */
	bool GMarking = false;
}

void WRForest::MarkForWasps(AActor* Who, float Seconds)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	if (!World || !Who->HasAuthority() || Seconds <= 0.f)
	{
		return;
	}
	const AWRCharacterBase* Body = Cast<AWRCharacterBase>(Who);
	if (Body && !Body->IsAlive())
	{
		return;
	}
	UWRForestTouch* Touch = UWRForestTouch::FindOrAdd(Who);
	if (!Touch)
	{
		return;
	}
	const double T = WRForestCrImpl::Now(Who);
	const bool bFresh = !Touch->bWaspMark;
	Touch->MarkUntil = FMath::Max(Touch->MarkUntil, T + Seconds);
	Touch->bWaspMark = true;
	Touch->OnRep_Mark();   // the listen server's own view
	if (bFresh)
	{
		WRFx::Broadcast(Who, TEXT("forest.cr_scout"), Who->GetActorLocation() + FVector(0.f, 0.f, Who->GetSimpleCollisionHalfHeight() * 0.8f), FRotator::ZeroRotator, 1.f, TEXT("forest.scout_mark"));
		WRForestCrImpl::Tell(Who, TEXT("A scout has marked you: every wasp near will come. Water or smoke washes it off"));
	}
	// Every Bramblewasp within 40 m comes for it, and every nest within 40 m pours out.
	const float Range = WRForestCrImpl::Tn(TEXT("bramblewasp.markRadius"), 4000.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* Wasp = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!Wasp || !WRForestCrImpl::Wild(Wasp) || !FWRForestAI::IsWasp(Wasp) || B->bRaider || FVector::Dist(Wasp->GetActorLocation(), Who->GetActorLocation()) > Range) continue;
		if (B->Has(WRForestFlag::Calmed)) continue;
		Wasp->SetAsleep(false);
		Wasp->SetAggroTarget(Who);
	}
	if (!WRForestCrImpl::GMarking)
	{
		TGuardValue<bool> Guard(WRForestCrImpl::GMarking, true);
		if (UWRForestHazards* Hz = UWRForestHazards::Get(World))
		{
			for (const TWeakObjectPtr<AWRWaspNest>& N : Hz->Nests)
			{
				AWRWaspNest* Nest = N.Get();
				if (!Nest || Nest->IsCalm() || WRForestCrImpl::Now(Nest) - Nest->SwarmedAt < 8.f || FVector::Dist(Nest->GetActorLocation(), Who->GetActorLocation()) > Range) continue;
				Nest->Swarm(Who);
			}
		}
	}
}

bool WRForest::HasWaspMark(const AActor* Who)
{
	const UWRForestTouch* Touch = UWRForestTouch::Get(Who);
	return Touch && Touch->bWaspMark;
}

void WRForest::ClearWaspMark(AActor* Who)
{
	UWRForestTouch* Touch = Who && Who->HasAuthority() ? UWRForestTouch::Get(Who) : nullptr;
	if (!Touch || !Touch->bWaspMark)
	{
		return;
	}
	Touch->bWaspMark = false;
	Touch->MarkUntil = 0.0;
	Touch->OnRep_Mark();
	// The wasps lose the scent: any not already on you lose interest.
	const float Keep = WRForestCrImpl::Tn(TEXT("bramblewasp.keepChaseRadius"), 600.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* Wasp = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!Wasp || !FWRForestAI::IsWasp(Wasp) || Wasp->GetAggroTarget() != Who) continue;
		if (FVector::Dist(Wasp->GetActorLocation(), Who->GetActorLocation()) > Keep) Wasp->SetAggroTarget(nullptr);
	}
}

void WRForest::WaspRaid(UWorld* World, const FVector& From, const FVector& To, int32 Count)
{
	if (!World || World->GetNetMode() == NM_Client || Count <= 0 || !FWRCreatureRegistry::Get().Find(TEXT("bramblewasp")))
	{
		return;
	}
	// A swarm leaves the nest for the hive tree, flying high over the canopy's understorey; the Hivebacks there meet it.
	const double T = World->GetTimeSeconds();
	int32 Came = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector At = From + FVector(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), -60.f - FMath::FRandRange(0.f, 80.f));
		AWRCreature* Wasp = AWRCreature::SpawnCreature(World, TEXT("bramblewasp"), At, FRotator(0.f, (To - From).Rotation().Yaw, 0.f));
		UWRForestBody* B = UWRForestBody::Get(Wasp);
		if (!Wasp || !B) continue;
		B->bRaider = true;
		B->RaidFrom = From;
		B->RaidTo = To;
		B->RaidStarted = T;
		B->bRaidArrived = false;
		B->bRaidDone = false;
		B->RaidQuietSince = -1.0;
		B->Set(WRForestFlag::Raiding, true);
		Wasp->SetHome(To);
		Wasp->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		++Came;
	}
	if (Came > 0)
	{
		WRFx::Broadcast(World, TEXT("forest.wasp_burst"), From, FRotator::ZeroRotator, 1.f, TEXT("forest.wasp_swarm"));
		UE_LOG(LogWildreach, Log, TEXT("FOREST a wasp raid of %d leaves %s for the hive at %s"), Came, *From.ToCompactString(), *To.ToCompactString());
	}
}

bool WRForestBurlbeastNear(const UObject* WorldContext, const FVector& At)
{
	const float Range = WRForestCrImpl::Tt(TEXT("haulRange"), 1500.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		const AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (C && C->GetWorld() == (WorldContext ? WorldContext->GetWorld() : nullptr) && C->IsAlive() && C->IsTamed() && !C->IsBaby() && B->KindId() == TEXT("burlbeast")
			&& FVector::Dist(C->GetActorLocation(), At) <= Range)
		{
			return true;
		}
	}
	return false;
}

bool WRForestRidingElderelk(const AActor* Who)
{
	const AWRCreature* Mount = Who ? Cast<AWRCreature>(Who->GetAttachParentActor()) : nullptr;
	const UWRForestBody* B = UWRForestBody::Get(Mount);
	return Mount && B && Mount->GetRider() == Who && B->KindId() == TEXT("elderelk");
}

bool WRForestTrufflerDugAt(const UObject* WorldContext, const FVector& At, float Radius)
{
	const double T = WRForestCrImpl::Now(WorldContext);
	const float Need = WRTune(TEXT("hazards.hollow.digSeconds"), 20.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		const AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || !C->IsTamed() || C->GetWorld() != (WorldContext ? WorldContext->GetWorld() : nullptr) || B->KindId() != TEXT("truffler") || B->DigStarted < 0.0) continue;
		if (FVector::Dist2D(B->DigAt, At) <= Radius && T - B->DigStarted >= Need) return true;
	}
	return false;
}

// ---------------------------------------------------------------- the AI hooks

namespace WRForestCrImpl
{
	AWRCreatureAI* AIOf(const AWRCreature* C) { return C ? Cast<AWRCreatureAI>(C->GetController()) : nullptr; }

	/** Prey the forest's hunters take in its beats (a lynx drop, a Rootcoil at a ford). */
	bool IsPrey(FName K) { return K == TEXT("dappledoe") || K == TEXT("truffler") || K == TEXT("fernsheep"); }

	/** A wild, idle forest creature (nothing to fight, not running, not a boss). */
	bool Idle(const AWRCreature* C)
	{
		return Wild(C) && !C->GetAggroTarget() && !C->IsFleeing() && !C->HuntTarget.IsValid() && !C->IsA<AWRBoss>() && !C->IsUnconscious();
	}

	/** The nearest standing giant whose trunk is within Range of At (its base, cm), or null. */
	const FWRForestData::FTree* TrunkNear(const FVector& At, float Range)
	{
		const FWRForestData::FTree* Best = nullptr;
		float BestD = Range;
		for (const FWRForestData::FTree& Tr : FWRForestData::Get().Giants)
		{
			const float D = FVector::Dist2D(Tr.Base, At) - Tr.Radius;
			if (D < BestD) { BestD = D; Best = &Tr; }
		}
		return Best;
	}

	/** Where a long body's head lies (a Rootcoil's is metres ahead of its middle): its row's reach along its facing. */
	FVector HeadAt(const AWRCreature* C)
	{
		// From the data on every machine (a dedicated server has no mesh): the rules must agree with the look.
		return C->GetActorLocation() + C->GetActorForwardVector() * Tn(TEXT("rootcoil.headForward"), 560.f);
	}

	/** Fly (or hover) toward Goal: the flying mode, the input scaled down as it arrives. */
	void HoverTo(AWRCreature* C, const FVector& Goal, float Scale)
	{
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		if (!Move->IsFlying()) Move->SetMovementMode(MOVE_Flying);
		const FVector To = Goal - C->GetActorLocation();
		const float D = To.Size();
		if (D > 15.f) C->AddMovementInput(To / D, Scale * FMath::Clamp(D / 250.f, 0.15f, 1.f));
	}

	/** A hover kind's idle drift round its home: low over the ground (wasps and finches) or round the hive (bees). */
	void HoverIdle(AWRCreature* C, UWRForestBody* B, float Lo, float Hi, float Span)
	{
		const double T = Now(C);
		if (T >= B->NextHoverAt || B->HoverGoal.IsZero())
		{
			B->NextHoverAt = T + FMath::FRandRange(2.5f, 6.f);
			bool bOk = false;
			const FVector Spot = C->GetHome() + FVector(FMath::RandPointInCircle(Span), 0.f);
			const FVector G = Ground(C->GetWorld(), Spot, bOk, C);
			B->HoverGoal = (bOk ? G : Spot) + FVector(0.f, 0.f, FMath::FRandRange(Lo, Hi));
		}
		HoverTo(C, B->HoverGoal, 0.45f);
	}

	/** The ground's point under a pawn's feet (trace), for shadows and landings. */
	FVector GroundUnder(const AActor* A)
	{
		bool bOk = false;
		const FVector G = Ground(A->GetWorld(), A->GetActorLocation(), bOk, A);
		return bOk ? G : Feet(A);
	}

	/** The perch a Thornlynx climbs to: a perch row (or a point up a giant) and the trunk it's on. */
	bool PickPerch(const AWRCreature* C, const FVector& Near, float Range, bool bNearTrail, FVector& OutPerch, FVector& OutTrunk, float& OutTrunkR)
	{
		const FWRForestData& D = FWRForestData::Get();
		TArray<const FWRForestData::FSpot*> Options;
		for (const FWRForestData::FSpot& S : D.Spots)
		{
			if (S.Kind != TEXT("perch") || FVector::Dist2D(S.At, Near) > Range) continue;
			if (bNearTrail)
			{
				// Over an elk trail or a road: within 30 m of one.
				bool bTrail = false;
				for (const FWRForestData::FPath& R : D.Routes)
				{
					if (R.Kind != TEXT("trail") && R.Kind != TEXT("road")) continue;
					for (int32 i = 0; i + 1 < R.Points.Num() && !bTrail; ++i)
					{
						bTrail = FVector::Dist2D(FMath::ClosestPointOnSegment(S.At, R.Points[i], R.Points[i + 1]), S.At) < Tn(TEXT("thornlynx.trailPerch"), 3000.f);
					}
					if (bTrail) break;
				}
				if (!bTrail) continue;
			}
			// Not one another lynx has.
			bool bTaken = false;
			for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
			{
				bTaken |= B.IsValid() && B->GetOwner() != C && B->PerchStage > 0 && FVector::Dist2D(B->PerchAt, S.At) < 200.f;
			}
			if (!bTaken) Options.Add(&S);
		}
		if (Options.Num() == 0)
		{
			return false;
		}
		const FWRForestData::FSpot* S = Options[FMath::RandRange(0, Options.Num() - 1)];
		OutPerch = S->At;
		if (const FWRForestData::FTree* Tr = TrunkNear(S->At, 600.f))
		{
			OutTrunk = Tr->Base;
			OutTrunkR = Tr->Radius;
		}
		else
		{
			OutTrunk = S->At - FRotator(0.f, S->Yaw, 0.f).Vector() * 250.f;
			OutTrunkR = 200.f;
		}
		return true;
	}

	/** A Thornlynx up a trunk at height Z (cm over the trunk's ground) at angle A (degrees round it): its body's place. */
	FVector OnTrunk(const UWRForestBody* B, const AWRCreature* C, float A, float Z)
	{
		const float Off = B->TrunkRadius + C->GetSimpleCollisionRadius() * 0.6f;
		return FVector(B->TrunkAt.X, B->TrunkAt.Y, B->TrunkAt.Z) + FVector(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f) * Off + FVector(0.f, 0.f, Z);
	}

	/** Holds a body at a place it moves itself (climbing, hanging): no movement mode of its own, its speed shown. */
	void Place(AWRCreature* C, const FVector& At, float Yaw, float DeltaTime)
	{
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		const FVector From = C->GetActorLocation();
		if (Move->MovementMode != MOVE_None) Move->DisableMovement();
		C->SetActorLocationAndRotation(At, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::None);
		const FVector Vel = DeltaTime > 0.f ? (At - From) / DeltaTime : FVector::ZeroVector;
		Move->Velocity = Vel;
		if (UCapsuleComponent* Cap = C->GetCapsuleComponent()) Cap->ComponentVelocity = Vel;
	}

	/** Lets go of a placed body: walking again, facing where it moves. */
	void Unplace(AWRCreature* C, bool bFall)
	{
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		Move->bOrientRotationToMovement = true;
		Move->SetMovementMode(bFall ? MOVE_Falling : MOVE_Walking);
	}

	/** A tap of E on a Mossmaul feeds it the usual way (the path-magic try is skipped for that one call). */
	bool GTapFeeding = false;
}

bool FWRForestAI::IsWasp(const AActor* Who)
{
	return WRForestCrImpl::KindOf(Who) == TEXT("bramblewasp");
}

void FWRForestAI::Signature(AWRCreature* Self)
{
	if (Self && Self->HasAuthority())
	{
		// Its voice's signature on every machine (WRSound::Voice resolves it by the family in sounds.json creatures).
		WRSound::Broadcast(Self, TEXT("voice.signature"));
	}
}

void FWRForestAI::WakeFor(AWRCreature* Self, float Seconds)
{
	if (Self && Self->IsAsleep()) Self->SetAsleep(false);
	(void)Seconds;
}

int32 FWRForestAI::RowPhaseOf(const AWRCreature* Self, FName* OutRow)
{
	const AWRCreatureAI* AI = WRForestCrImpl::AIOf(Self);
	if (OutRow) *OutRow = AI && AI->RowPhase != 0 ? AI->RowId : NAME_None;
	return AI ? AI->RowPhase : 0;
}

void FWRForestAI::ContactRow(AWRCreature* Self, AWRCharacterBase* Victim)
{
	if (AWRCreatureAI* AI = WRForestCrImpl::AIOf(Self))
	{
		if (AI->RowPhase == 3) AI->RowHit(Self, Victim);
	}
}

namespace WRForestCrImpl
{
	/** The nearest of the Four Hearts' crowns to a point (where the Bloom's finches ring them) and the ring's radius. The
	 *  layout is the arenas' (FWRForestBoss::HeartsLayout from Data/forest.json), the same on every machine. */
	bool HeartCrown(const FVector& From, FVector& OutCrown, float& OutRing)
	{
		static TArray<FVector> Hearts;
		static bool bLaidOut = false;
		if (!bLaidOut)
		{
			FVector Centre;
			TArray<FVector> Stones;
			bLaidOut = FWRForestBoss::HeartsLayout(Centre, Hearts, Stones);
		}
		if (Hearts.Num() == 0) return false;
		const FVector* Best = &Hearts[0];
		for (const FVector& H : Hearts) if (FVector::DistSquared2D(H, From) < FVector::DistSquared2D(*Best, From)) Best = &H;
		const float Height = WRTune(TEXT("bosses.sylvaroth.heartHeight"), 14000.f);
		OutCrown = *Best + FVector(0.f, 0.f, Height * Tn(TEXT("glimmerfinch.crownFrac"), 0.78f));
		OutRing = Tn(TEXT("glimmerfinch.ringRadius"), 2600.f);
		return true;
	}
}

bool FWRForestAI::TickWild(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || !AI)
	{
		return false;
	}
	const double T = WRForestCrImpl::Now(Self);
	const FName K = Body->KindId();
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	// Holding someone, clinging to someone, flying in a heat ball, on a trunk, falling off one, down: the body's own life
	// runs it (TickBody).
	if (Body->Hold.IsValid() || Body->ClingTo.IsValid() || Body->bDropping || T < Body->DownedUntil || !Body->bLanded)
	{
		return true;
	}
	if (Body->BallTarget.IsValid() && T < Body->BallUntil)
	{
		return true;
	}
	// A path-magic tame under way: it lies still under the hand.
	if (!Body->PathTamer.IsEmpty())
	{
		return true;
	}
	// The row it goes into at once (drop -> fang bite, log strike -> constrict, a bugle owed).
	if (!Body->ForceNext.IsNone())
	{
		const FName Next = Body->ForceNext;
		Body->ForceNext = NAME_None;
		AWRCharacterBase* On = Target;
		if (T < Body->ForceUntil && On && On->IsAlive())
		{
			AI->BreatheUntil = 0.0;
			AI->SetControlRotation((On->GetActorLocation() - Self->GetActorLocation()).Rotation());
			if (AI->StartRow(Self, On, Next, true)) return true;
		}
	}
	// Smoke calms wasps and bees: they hang dazed and forget whom they were after.
	if ((K == TEXT("bramblewasp") || K == TEXT("hiveback")) && WRForest::InSmoke(Self, Self->GetActorLocation()))
	{
		Body->Set(WRForestFlag::Calmed, true);
		Self->SetAggroTarget(nullptr);
		Self->HuntTarget = nullptr;
		WRForestCrImpl::HoverTo(Self, Self->GetActorLocation() + FVector(0.f, 0.f, -10.f), 0.1f);
		return true;
	}
	Body->Set(WRForestFlag::Calmed, false);

	if (K == TEXT("thornlynx"))
	{
		// On a trunk (climbing, hanging, coming down): TickBody moves it; here it watches for something passing under.
		if (Body->PerchStage == 3)
		{
			const float Reach = WRForestCrImpl::Tn(TEXT("thornlynx.dropReach"), 450.f) * (WRForestCrImpl::WeatherAt(Self, Self->GetActorLocation()) == TEXT("fog") ? WRForestCrImpl::Tn(TEXT("thornlynx.mistDropMult"), 1.5f) : 1.f);
			AWRCharacterBase* Under = nullptr;
			float Best = Reach;
			for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
			{
				if (*It == Self || !It->IsAlive() || It->IsHidden()) continue;
				const AWRCreature* Cr = Cast<AWRCreature>(*It);
				const bool bPrey = Cr && WRForestCrImpl::Wild(Cr) && WRForestCrImpl::IsPrey(WRForestCrImpl::KindOf(Cr));
				const bool bFoe = !Cr || Cr->IsTamed();
				if (!bPrey && !bFoe) continue;
				if (Target && *It != Target) continue;   // in a fight, only what it's after
				const float D = FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation());
				if (D < Best && It->GetActorLocation().Z < Self->GetActorLocation().Z - 300.f) { Best = D; Under = *It; }
			}
			if (Under)
			{
				if (AWRCreature* Prey = Cast<AWRCreature>(Under); Prey && !Prey->IsTamed()) Self->HuntTarget = Prey;
				Self->SetAggroTarget(Under);
				AI->SetControlRotation((Under->GetActorLocation() - Self->GetActorLocation()).Rotation());
				if (AI->StartRow(Self, Under, TEXT("canopy_drop"), false)) return true;
			}
			// Waited long enough (or its prey is far off): down it comes.
			if (T >= Body->PerchUntil || (Target && FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) > WRForestCrImpl::Tn(TEXT("thornlynx.leavePerchRadius"), 2000.f)))
			{
				Body->PerchStage = 4;
				Body->ClimbStarted = T;
			}
			return true;
		}
		if (Body->PerchStage > 0)
		{
			return true;
		}
		// Idle: now and then up a trunk to hang over a way through the trees.
		if (!Target && WRForestCrImpl::Idle(Self) && T >= Body->NextPerchAt)
		{
			Body->NextPerchAt = T + WRForestCrImpl::Roll(TEXT("thornlynx.perchEvery"), 60.f, 180.f);
			FVector Perch, Trunk;
			float TrunkR = 200.f;
			if (WRForestCrImpl::PickPerch(Self, Self->GetHome(), WRForestCrImpl::Tn(TEXT("thornlynx.perchSearch"), 4000.f), false, Perch, Trunk, TrunkR))
			{
				Body->PerchAt = Perch;
				Body->TrunkAt = Trunk;
				Body->TrunkRadius = TrunkR;
				Body->bEscape = false;
				Body->PerchStage = 1;
				Body->PerchSince = 0.0;
				return true;
			}
		}
	}
	else if (K == TEXT("rootcoil"))
	{
		// Rain (DC 2.9): it leaves its ford and hunts by day (the Mossmaul's rain hunt below). Off the log, hungry, and the
		// common hunter's AI takes it; the ford waits until the rain stops.
		if (!Target && !Self->IsFleeing() && !AWRGameState::IsNightIn(Self) && WRForest::Raining(Self, Self->GetActorLocation()))
		{
			if (Body->Has(WRForestFlag::LogRest)) Body->Set(WRForestFlag::LogRest, false);
			if (T >= Body->NextRainHuntAt)
			{
				Body->NextRainHuntAt = T + WRForestCrImpl::Tn(TEXT("rootcoil.rainHuntEvery"), 240.f);
				Self->MakeHungry();
			}
			return false;
		}
		if (!Target && !Self->IsFleeing() && !Self->HuntTarget.IsValid())
		{
			// To its ford (a beat of the living world) first, then lie across it.
			if (!Body->RestAt.IsZero() && FVector::Dist2D(Body->RestAt, Self->GetActorLocation()) > 300.f && !Body->Has(WRForestFlag::LogRest))
			{
				Self->AddMovementInput((Body->RestAt - Self->GetActorLocation()).GetSafeNormal2D(), 0.6f);
				return true;
			}
			if (T >= Body->RestAgainAt)
			{
				if (!Body->Has(WRForestFlag::LogRest))
				{
					Body->Set(WRForestFlag::LogRest, true);
					if (!Body->RestAt.IsZero()) Self->SetActorRotation(FRotator(0.f, Body->RestYaw, 0.f));
				}
				Self->GetCharacterMovement()->StopMovementImmediately();
				// Within 7 m of its head (a survivor, or a sounder of Trufflers walking over it): the log strike.
				const FVector Head = WRForestCrImpl::HeadAt(Self);
				const float Trigger = WRForestCrImpl::Tn(TEXT("rootcoil.restTrigger"), 700.f);
				AWRCharacterBase* Near = nullptr;
				float Best = Trigger;
				for (TActorIterator<AWRCharacterBase> It(Self->GetWorld()); It; ++It)
				{
					if (*It == Self || !It->IsAlive() || It->IsHidden()) continue;
					const AWRCreature* Cr = Cast<AWRCreature>(*It);
					if (Cr && !(Cr->IsTamed() || (WRForestCrImpl::Wild(Cr) && WRForestCrImpl::IsPrey(WRForestCrImpl::KindOf(Cr))))) continue;
					const float D = FMath::Min(FVector::Dist(It->GetActorLocation(), Head), FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) + 200.f);
					if (D < Best) { Best = D; Near = *It; }
				}
				if (Near)
				{
					if (AWRCreature* Prey = Cast<AWRCreature>(Near); Prey && !Prey->IsTamed()) Self->HuntTarget = Prey;
					Self->SetAggroTarget(Near);
					AI->SetControlRotation((Near->GetActorLocation() - Self->GetActorLocation()).Rotation());
					if (AI->StartRow(Self, Near, TEXT("log_strike"), false)) return true;
				}
				return true;
			}
		}
	}
	else if (K == TEXT("mossmaul"))
	{
		// The roar opens every fight.
		if (Target && !Body->bRoared && Target->IsAlive() && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < WRForestCrImpl::Tn(TEXT("mossmaul.roarRadius"), 1200.f))
		{
			AI->SetControlRotation((Target->GetActorLocation() - Self->GetActorLocation()).Rotation());
			if (AI->StartRow(Self, Target, TEXT("roar"), false)) return true;
		}
		// Its larder: it stays by the moss-covered kill and turns on anything that comes near it.
		if (!Target && T < Body->LarderUntil && !Body->LarderAt.IsZero())
		{
			if (AWRCharacter* P = WRForestCrImpl::NearestSurvivor(Self->GetWorld(), Body->LarderAt, WRForestCrImpl::Tn(TEXT("mossmaul.larderGuard"), 1500.f)))
			{
				Self->SetAggroTarget(P);
				return false;
			}
			const FVector To = Body->LarderAt - Self->GetActorLocation();
			if (To.Size2D() > WRForestCrImpl::Tn(TEXT("mossmaul.larderStay"), 600.f)) Self->AddMovementInput(To.GetSafeNormal2D(), 0.5f);
			return true;
		}
		// Rain: it hunts by day.
		if (!Target && WRForest::Raining(Self, Self->GetActorLocation()) && T >= Body->NextRainHuntAt)
		{
			Body->NextRainHuntAt = T + WRForestCrImpl::Tn(TEXT("mossmaul.rainHuntEvery"), 240.f);
			Self->MakeHungry();
		}
	}
	else if (K == TEXT("elderelk"))
	{
		// Hit: the bugle comes before anything else, and the herd turns.
		if (T - Body->BugleWantedAt < 2.0 && Body->BugleAt.IsValid())
		{
			Body->BugleWantedAt = -1000.0;
			if (AWRCharacterBase* By = Cast<AWRCharacterBase>(Body->BugleAt.Get()); By && By->IsAlive())
			{
				AI->SetControlRotation((By->GetActorLocation() - Self->GetActorLocation()).Rotation());
				if (AI->StartRow(Self, By, TEXT("bugle"), false)) return true;
			}
		}
	}
	else if (K == TEXT("bramblewasp"))
	{
		if (Body->bRaider)
		{
			return TickRaid(AI, Self, Body, DeltaSeconds);
		}
		// A marked body within 40 m: every wasp comes for it.
		if (!Target && T >= Body->NextWaspScanAt)
		{
			Body->NextWaspScanAt = T + 0.5;
			float Best = WRForestCrImpl::Tn(TEXT("bramblewasp.markRadius"), 4000.f);
			for (const TWeakObjectPtr<UWRForestTouch>& Tch : UWRForestTouch::Live)
			{
				AActor* Who = Tch.IsValid() ? Tch->GetOwner() : nullptr;
				if (!Who || !Tch->bWaspMark || Who->GetWorld() != Self->GetWorld()) continue;
				const float D = FVector::Dist(Who->GetActorLocation(), Self->GetActorLocation());
				if (D < Best) { Best = D; Target = Cast<AWRCharacterBase>(Who); }
			}
			if (Target)
			{
				Self->SetAggroTarget(Target);
				return false;
			}
		}
		if (!Target && !Self->IsAsleep() && !Self->IsFleeing())
		{
			// A scout notices a survivor near (its row marks them; then the rest come).
			const FWRCreatureDef* Def = Self->GetDef();
			if (AWRCharacter* P = WRForestCrImpl::NearestSurvivor(Self->GetWorld(), Self->GetActorLocation(), Def ? Def->AggroRadius : 1500.f); P && !Self->HasLostSightOf(P))
			{
				Self->SetAggroTarget(P);
				return false;
			}
			WRForestCrImpl::HoverIdle(Self, Body, 120.f, 320.f, 500.f);
			return true;
		}
	}
	else if (K == TEXT("hiveback"))
	{
		// Wasps near the hive: the bees go for them (and the wasps fight back: predator and prey both ways).
		if (!Target && T >= Body->NextWaspScanAt)
		{
			Body->NextWaspScanAt = T + 1.0;
			const float Guard = WRForestCrImpl::Tn(TEXT("hiveback.hiveDefend"), 2500.f);
			for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
			{
				AWRCreature* Wasp = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
				if (!Wasp || !WRForestCrImpl::Wild(Wasp) || !IsWasp(Wasp) || FVector::Dist(Wasp->GetActorLocation(), Self->GetHome()) > Guard) continue;
				Self->HuntTarget = Wasp;
				Self->SetAggroTarget(Wasp);
				if (!Wasp->GetAggroTarget()) { Wasp->HuntTarget = Self; Wasp->SetAggroTarget(Self); }
				return false;
			}
		}
		if (!Target && !Self->IsAsleep() && !Self->IsFleeing())
		{
			WRForestCrImpl::HoverIdle(Self, Body, 80.f, 260.f, 600.f);
			return true;
		}
	}
	else if (K == TEXT("glimmerfinch"))
	{
		// The Bloom's warning (DC 2.10): every Glimmerfinch in the forest takes off at once and rings the Four Hearts, a
		// slow wheel of glinting birds round the nearest heart-tree's crown, until the Bloom starts. Asleep or startled,
		// it goes all the same.
		if (WRForest::BloomWarning(Self) >= 0.f && WRForest::Contains(Self->GetActorLocation()))
		{
			FVector Crown;
			float Ring = 0.f;
			if (WRForestCrImpl::HeartCrown(Self->GetActorLocation(), Crown, Ring))
			{
				if (Self->IsAsleep()) Self->SetAsleep(false);
				Body->Spot = Crown;
				Body->LiftUntil = T + 2.0;   // the lifted look (the wide orbit, the shimmer) while it rings; lapses after
				const float Phase = float(Self->GetUniqueID() % 360) * (PI / 180.f);
				const float A = float(T) * WRForestCrImpl::Tn(TEXT("glimmerfinch.ringSpeed"), 0.35f) + Phase;
				const FVector Goal = Crown + FVector(FMath::Cos(A) * Ring, FMath::Sin(A) * Ring, 300.f * FMath::Sin(float(T) * 0.7f + Phase));
				WRForestCrImpl::HoverTo(Self, Goal, 1.f);
				return true;
			}
		}
		if (Self->IsFleeing() || Self->IsAsleep())
		{
			return false;
		}
		// Lifted over a sign (a fresh kill, a ripe patch): the flock circles high over it.
		if (T < Body->LiftUntil)
		{
			const float A = float(T) * 0.6f + float(Self->GetUniqueID() % 360);
			WRForestCrImpl::HoverTo(Self, FVector(Body->Spot) + FVector(FMath::Cos(A) * 600.f, FMath::Sin(A) * 600.f, WRForestCrImpl::Tn(TEXT("glimmerfinch.liftHeight"), 1200.f)), 0.8f);
			return true;
		}
		// A survivor close and moving: up and away in a shimmering burst.
		if (AWRCharacter* P = WRForestCrImpl::NearestSurvivor(Self->GetWorld(), Self->GetActorLocation(), WRForestCrImpl::Tn(TEXT("glimmerfinch.startle"), 700.f)); P && P->GetVelocity().Size2D() > 200.f && Self->CanBoltAgain())
		{
			WRFx::Broadcast(Self, TEXT("forest.cr_shimmer_burst"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.4f, NAME_None);
			WRSound::Broadcast(Self, TEXT("voice.aggro"));
			Self->StartFleeing(P, TEXT("startled"));
			return false;
		}
		WRForestCrImpl::HoverIdle(Self, Body, 60.f, 200.f, 500.f);
		return true;
	}
	else if (K == TEXT("fernsheep") || K == TEXT("dappledoe"))
	{
		// Flees, unless cornered: then a butt or a kick at whatever cornered it, and off again.
		AWRCharacterBase* From = Cast<AWRCharacterBase>(Self->GetFleeFrom());
		if (Self->IsFleeing() && From && From->IsAlive())
		{
			const FVector Away = (Self->GetActorLocation() - From->GetActorLocation()).GetSafeNormal2D();
			FHitResult Wall;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRForestCornered), false, Self);
			const bool bWall = Self->GetWorld()->SweepSingleByChannel(Wall, Self->GetActorLocation(), Self->GetActorLocation() + Away * 250.f, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(Self->GetSimpleCollisionRadius() * 0.8f), Q);
			const bool bStuck = Self->GetVelocity().Size2D() < 50.f;
			if (bWall || bStuck) { if (Body->CorneredSince < 0.0) Body->CorneredSince = T; }
			else Body->CorneredSince = -1.0;
			if (Body->CorneredSince >= 0.0 && T - Body->CorneredSince > 0.6 && FVector::Dist2D(From->GetActorLocation(), Self->GetActorLocation()) < WRTune(TEXT("creatureAI.corneredRadius"), 450.f))
			{
				AI->TickRowFight(Self, From, DeltaSeconds);
				return true;
			}
		}
		else
		{
			Body->CorneredSince = -1.0;
		}
	}
	return false;
}

bool FWRForestAI::TickRaid(AWRCreatureAI* AI, AWRCreature* Self, UWRForestBody* Body, float DeltaSeconds)
{
	// A raid swarm: over the understorey to the hive tree, at the Hivebacks there, and home again when it's over.
	const double T = WRForestCrImpl::Now(Self);
	AWRCharacterBase* Target = Cast<AWRCharacterBase>(Self->GetAggroTarget());
	const float Near = WRForestCrImpl::Tn(TEXT("bramblewasp.raidArrive"), 1500.f);
	if (Body->bRaidDone)
	{
		Self->SetAggroTarget(nullptr);
		Self->HuntTarget = nullptr;
		WRForestCrImpl::HoverTo(Self, Body->RaidFrom, 1.f);
		if (FVector::Dist(Self->GetActorLocation(), Body->RaidFrom) < 300.f || T - Body->RaidStarted > WRForestCrImpl::Tn(TEXT("bramblewasp.raidMaxSeconds"), 300.f) + 120.f)
		{
			Self->Destroy();
		}
		return true;
	}
	if (!Body->bRaidArrived)
	{
		WRForestCrImpl::HoverTo(Self, Body->RaidTo + FVector(0.f, 0.f, WRForestCrImpl::Tn(TEXT("bramblewasp.raidHeight"), 500.f)), 1.f);
		if (FVector::Dist2D(Self->GetActorLocation(), Body->RaidTo) < Near) Body->bRaidArrived = true;
		return true;
	}
	if (Target && Target->IsAlive())
	{
		return false;   // fighting a bee: its rows
	}
	// At the hive: the nearest Hiveback still flying.
	AWRCreature* Bee = nullptr;
	float Best = WRForestCrImpl::Tn(TEXT("bramblewasp.raidReach"), 3000.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* Cr = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!Cr || !WRForestCrImpl::Wild(Cr) || B->KindId() != TEXT("hiveback")) continue;
		const float D = FVector::Dist(Cr->GetActorLocation(), Body->RaidTo);
		if (D < Best) { Best = D; Bee = Cr; }
	}
	if (Bee)
	{
		Body->RaidQuietSince = -1.0;
		Self->HuntTarget = Bee;
		Self->SetAggroTarget(Bee);
		if (!Bee->GetAggroTarget()) { Bee->HuntTarget = Self; Bee->SetAggroTarget(Self); }
		return false;
	}
	// Nothing left to fight: round the hive a while (tearing at the comb), then home.
	if (Body->RaidQuietSince < 0.0) Body->RaidQuietSince = T;
	WRForestCrImpl::HoverIdle(Self, Body, 200.f, 450.f, 300.f);
	if (T - Body->RaidQuietSince > WRForestCrImpl::Tn(TEXT("bramblewasp.raidLinger"), 40.f) || T - Body->RaidStarted > WRForestCrImpl::Tn(TEXT("bramblewasp.raidMaxSeconds"), 300.f))
	{
		Body->bRaidDone = true;
	}
	(void)AI; (void)DeltaSeconds;
	return true;
}

bool FWRForestAI::TickTame(AWRCreatureAI* AI, AWRCreature* Self, float DeltaSeconds)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || Self->GetRider())
	{
		return false;
	}
	const double T = WRForestCrImpl::Now(Self);
	const FName K = Body->KindId();
	AWRCharacter* Owner = WRForestCrImpl::OwnerBody(Self->GetWorld(), Self->TamedBy);
	if (Body->ClingTo.IsValid() || (Body->BallTarget.IsValid() && T < Body->BallUntil))
	{
		return true;
	}
	if (WRForestCrImpl::Hovers(K))
	{
		// A whistled flock over the nearest ripe thing; else round its owner at shoulder height, or round its hive.
		if (T < Body->LiftUntil)
		{
			const float A = float(T) * 0.7f;
			WRForestCrImpl::HoverTo(Self, FVector(Body->Spot) + FVector(FMath::Cos(A) * 500.f, FMath::Sin(A) * 500.f, WRForestCrImpl::Tn(TEXT("glimmerfinch.liftHeight"), 1200.f)), 0.9f);
			return true;
		}
		if (K == TEXT("hiveback") && Self->TameOrder == EWRTameOrder::Stay)
		{
			// Ordered to stay by a beehive of its owner's: it moves in (the hive works: WRFarm's beekeeping).
			float Best = WRForestCrImpl::Tt(TEXT("hiveFind"), 1500.f);
			const AWRBuildPiece* Hive = nullptr;
			for (TActorIterator<AWRBuildPiece> It(Self->GetWorld()); It; ++It)
			{
				const FWRPieceDef* D = It->bGhost ? nullptr : It->GetDef();
				if (!D || D->Kind != TEXT("hive") || !It->CanAccess(Self->TamedBy)) continue;
				const float Dist = FVector::Dist(It->GetActorLocation(), Self->GetHome());
				if (Dist < Best) { Best = Dist; Hive = *It; }
			}
			if (Hive) Self->SetHome(Hive->GetActorLocation() + FVector(0.f, 0.f, 120.f));
			WRForestCrImpl::HoverIdle(Self, Body, 60.f, 180.f, Hive ? 250.f : 400.f);
			return true;
		}
		if (Owner && Self->TameOrder == EWRTameOrder::Follow)
		{
			const float Phase = float(Self->GetUniqueID() % 360);
			const float A = FMath::DegreesToRadians(Phase + float(T) * 25.f);
			const FVector Goal = Owner->GetActorLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (Owner->GetSimpleCollisionRadius() + 180.f) + FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight() * 0.6f);
			WRForestCrImpl::HoverTo(Self, Goal, 1.f);
			return true;
		}
		WRForestCrImpl::HoverIdle(Self, Body, 60.f, 200.f, 300.f);
		return true;
	}
	if (K == TEXT("truffler"))
	{
		// Nose down over the soft spot it found (the Hollow Root's dig), or on its way there.
		if (T < Body->RootingUntil)
		{
			Self->GetCharacterMovement()->StopMovementImmediately();
			return true;
		}
		if (!Body->DigAt.IsZero() && Body->DigStarted < 0.0 && Owner && Self->TameOrder == EWRTameOrder::Follow)
		{
			const FVector To = Body->DigAt - Self->GetActorLocation();
			if (To.Size2D() > 120.f)
			{
				Self->AddMovementInput(To.GetSafeNormal2D(), 0.7f);
				return true;
			}
			// At the spot: nose down and digging (the hazards track's plug takes the dig: WRForestHollow::TrufflerDigging).
			Body->DigStarted = T;
			Body->RootingUntil = T + 1.0;
			Self->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
			WRSound::Broadcast(Self, TEXT("forest.dig"));
			WRForestCrImpl::Tell(Owner, FString::Printf(TEXT("Your %s roots hard at the soft ground: something hollow under it"), *Self->GetDef()->Name));
			return true;
		}
	}
	(void)AI; (void)DeltaSeconds;
	return false;
}

bool FWRForestAI::RowAllowed(const AWRCreatureAI* AI, const AWRCreature* Self, const FWRAttackRow& Row, const AWRCharacterBase* Target, bool bReading)
{
	const UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || !Target)
	{
		return true;
	}
	const FName Id = Row.Id;
	if (bReading)
	{
		return true;   // the reads are their rows' own (readsAction)
	}
	// Forced only: from a perch, from the log rest, the roar's opening, a hit's bugle.
	if (Id == TEXT("canopy_drop") || Id == TEXT("log_strike") || Id == TEXT("roar") || Id == TEXT("bugle"))
	{
		return false;
	}
	// On a trunk nothing else starts (it drops first).
	if (Body->PerchStage > 0)
	{
		return false;
	}
	const double T = WRForestCrImpl::Now(Self);
	if (Id == TEXT("tree_escape"))
	{
		return Self->GetHealthFraction() < WRForestCrImpl::Tn(TEXT("thornlynx.escapeBelow"), 0.4f) && T >= Body->NextEscapeAt
			&& WRForestCrImpl::TrunkNear(Self->GetActorLocation(), WRForestCrImpl::Tn(TEXT("thornlynx.escapeRange"), 1500.f)) != nullptr;
	}
	if (Id == TEXT("maul_hug") || Id == TEXT("constrict"))
	{
		return !WRForestGrip::IsHeld(Target) && !Body->Hold.IsValid();
	}
	if (Id == TEXT("scout_mark"))
	{
		return !WRForest::HasWaspMark(Target) && !Body->bRaider && !Cast<AWRCreature>(Target);
	}
	if (Id == TEXT("sting"))
	{
		// A Hiveback stings only near its hive, when it was hit, or at a wasp.
		return IsWasp(Target) || FVector::Dist(Self->GetActorLocation(), Self->GetHome()) < WRForestCrImpl::Tn(TEXT("hiveback.hiveRange"), 1500.f) || Self->SinceHit() < 20.f;
	}
	if (Id == TEXT("heat_ball"))
	{
		const UWRForestTouch* Tch = UWRForestTouch::Get(Target);
		const int32 Max = FMath::RoundToInt(WRForestCrImpl::Tn(TEXT("hiveback.maxCling"), 4.f));
		// On wasps (the Bramblequeen too: three balled on her hold her, WRForestBosses), and on survivors at its hive.
		const bool bHornet = IsWasp(Target) || WRForestCrImpl::KindOf(Target) == TEXT("bramblequeen");
		return (!Tch || Tch->Bees.Num() < Max) && (bHornet || FVector::Dist(Target->GetActorLocation(), Self->GetHome()) < WRForestCrImpl::Tn(TEXT("hiveback.hiveRange"), 1500.f));
	}
	if (Id == TEXT("butt") || Id == TEXT("kick"))
	{
		return Body->CorneredSince >= 0.0;
	}
	(void)AI;
	return true;
}

void FWRForestAI::RowStarted(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body)
	{
		return;
	}
	const double T = WRForestCrImpl::Now(Self);
	Body->LastAttackAt = T;
	Body->LastFightAt = T;
	const FName Id = Row.Id;
	if (Id == TEXT("roar"))
	{
		Body->bRoared = true;
		Signature(Self);   // the long chest-deep roar
	}
	else if (Id == TEXT("canopy_drop") && Target)
	{
		// The tell everyone can read from 15 m: its shadow grows on the ground where it will land, bark and leaves rain
		// down from the trunk, a rustle overhead.
		Body->Spot = WRForestCrImpl::GroundUnder(Target);
		WRFx::Broadcast(Self, TEXT("forest.cr_shadow"), FVector(Body->Spot) + FVector(0.f, 0.f, 4.f), FRotator::ZeroRotator, 1.f, TEXT("forest.canopy_rustle"), Row.Windup);
	}
	else if (Id == TEXT("log_strike"))
	{
		// The log breathes, a hiss, the head lifts out of the root tangle.
		WRSound::Broadcast(Self, TEXT("forest.log_breath"));
		Signature(Self);
	}
	else if (Id == TEXT("bull_rush") || Id == TEXT("antler_charge") || Id == TEXT("crash"))
	{
		// Head down and a scrape in the leaf litter.
		WRFx::Broadcast(Self, TEXT("forest.cr_scrape"), WRForestCrImpl::Feet(Self) + Self->GetActorForwardVector() * Self->GetSimpleCollisionRadius() * 0.6f, Self->GetActorRotation(), FMath::Max(0.6f, Self->GetSimpleCollisionRadius() / 100.f));
		if (Id != TEXT("antler_charge")) Signature(Self);
	}
	else if (Id == TEXT("scout_mark") && Target)
	{
		// The scout hangs at your head and fans its abdomen.
		WRFx::Broadcast(Self, TEXT("forest.cr_fan"), Target->GetActorLocation() + FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight() * 0.85f), FRotator::ZeroRotator, 1.f, TEXT("forest.wasp_hum"), Row.Windup);
	}
	else if (Id == TEXT("heat_ball") && Target)
	{
		// Several converge: up to three hive-mates near join the ball on the same body.
		Body->BallTarget = Target;
		Body->BallUntil = T + Row.Windup + Row.Active;
		WRSound::Broadcast(Self, TEXT("forest.heat_ball"));
		int32 Joined = 0;
		const int32 Max = FMath::RoundToInt(WRForestCrImpl::Tn(TEXT("hiveback.maxCling"), 4.f)) - 1;
		for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
		{
			AWRCreature* Mate = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
			AWRCreatureAI* MateAI = WRForestCrImpl::AIOf(Mate);
			if (Joined >= Max || !Mate || Mate == Self || !MateAI || MateAI->RowPhase != 0 || B->KindId() != TEXT("hiveback") || Mate->IsTamed() != Self->IsTamed()) continue;
			if (B->ClingTo.IsValid() || FVector::Dist(Mate->GetActorLocation(), Self->GetActorLocation()) > WRForestCrImpl::Tn(TEXT("hiveback.joinRadius"), 1500.f)) continue;
			if (!Mate->IsTamed() && !Mate->GetAggroTarget()) { Mate->SetAggroTarget(Target); if (AWRCreature* TC = Cast<AWRCreature>(Target); TC && !TC->IsTamed()) Mate->HuntTarget = TC; }
			if (Mate->GetAggroTarget() == Target && MateAI->StartRow(Mate, Target, TEXT("heat_ball"), false)) ++Joined;
		}
	}
	else if (Id == TEXT("bugle"))
	{
		Signature(Self);   // the bugle: a rising scream and grunts
	}
	(void)AI;
}

bool FWRForestAI::StrikeRow(AWRCreatureAI* AI, AWRCreature* Self, const FWRAttackRow& Row, AWRCharacterBase* Target)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || !AI)
	{
		return false;
	}
	const FName M = Row.Motion;
	const double T = WRForestCrImpl::Now(Self);
	UWorld* World = Self->GetWorld();
	if (M == TEXT("roar"))
	{
		// Survivors within 12 m stagger unless guarding; the crown shakes and the widowmakers near it come down.
		const float Radius = FMath::Max(300.f, Row.Radius);
		WRFx::Broadcast(Self, TEXT("forest.cr_roar"), Self->MouthLocation(), Self->GetActorRotation(), Radius / 600.f, TEXT("forest.mossmaul_roar_big"));
		for (TActorIterator<AWRCharacter> It(World); It; ++It)
		{
			if (!It->IsAlive() || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) > Radius + It->GetSimpleCollisionRadius()) continue;
			if (!WRForestCrImpl::Guarding(*It)) It->Status->Stagger(WRForestCrImpl::Tn(TEXT("mossmaul.roarStagger"), 0.6f));
		}
		const int32 Fell = WRForest::DropWidowmakers(World, Self->GetActorLocation(), WRForestCrImpl::Tn(TEXT("mossmaul.roarWidowRange"), 600.f));
		UE_LOG(LogWildreach, Log, TEXT("FOREST %s roars (%d widowmakers fall)"), *Self->GetName(), Fell);
		return true;
	}
	if (M == TEXT("drop"))
	{
		// Off the trunk: down onto where its shadow grew, fast enough to land with the strike.
		const FVector From = Self->GetActorLocation();
		const FVector To = FVector(Body->Spot) + FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
		const float Flight = FMath::Max(0.2f, WRForestCrImpl::Tn(TEXT("thornlynx.dropSeconds"), 0.35f));
		const FVector D = To - From;
		Body->PerchStage = 0;
		Body->bDropping = true;
		Body->DropStarted = T;
		Body->NextEscapeAt = T + WRForestCrImpl::Tn(TEXT("thornlynx.escapeCooldown"), 12.f);
		WRForestCrImpl::Unplace(Self, true);
		Self->SetActorRotation(FRotator(0.f, D.Rotation().Yaw, 0.f));
		Self->LaunchCharacter(FVector(D.X / Flight, D.Y / Flight, (D.Z - 0.5f * -980.f * Flight * Flight) / Flight), true, true);
		AI->RowPhaseEnd = FMath::Max(AI->RowPhaseEnd, T + Flight + 0.2);
		WRSound::Broadcast(Self, TEXT("forest.canopy_rustle"));
		return true;
	}
	if (M == TEXT("climb"))
	{
		// Up the nearest trunk, out of reach, to drop again in 4 to 8 s.
		const FWRForestData::FTree* Tr = WRForestCrImpl::TrunkNear(Self->GetActorLocation(), WRForestCrImpl::Tn(TEXT("thornlynx.escapeRange"), 1500.f));
		if (Tr)
		{
			bool bOk = false;
			const FVector Foot = WRForestCrImpl::Ground(World, Tr->Base, bOk, Self);
			const float A = (Self->GetActorLocation() - Tr->Base).Rotation().Yaw;
			Body->TrunkAt = FVector(Tr->Base.X, Tr->Base.Y, bOk ? Foot.Z : Tr->Base.Z);
			Body->TrunkRadius = Tr->Radius;
			Body->PerchAt = WRForestCrImpl::OnTrunk(Body, Self, A, WRForestCrImpl::Roll(TEXT("thornlynx.escapeHeight"), 600.f, 1100.f));
			Body->bEscape = true;
			Body->PerchStage = 1;
				Body->PerchSince = 0.0;
			Body->NextEscapeAt = T + WRForestCrImpl::Tn(TEXT("thornlynx.escapeCooldown"), 12.f);
		}
		return true;
	}
	if (M == TEXT("bodyslam"))
	{
		// Down from 3 m onto where you stood (within its head's reach): everything within 250 cm of the landing.
		FVector Land = AI->RowAim;
		const FVector Head = WRForestCrImpl::HeadAt(Self);
		if (FVector::Dist2D(Land, Head) > Row.Radius * 2.f) Land = Head;
		const float Radius = FMath::Max(150.f, Row.Radius);
		WRFx::Broadcast(Self, TEXT("forest.cr_slam"), Land, FRotator::ZeroRotator, Radius / 150.f, TEXT("stomp"));
		for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
		{
			if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist2D(It->GetActorLocation(), Land) < Radius + It->GetSimpleCollisionRadius()) AI->RowHit(Self, *It);
		}
		return true;
	}
	if (M == TEXT("mark"))
	{
		// The pheromone: only if it's still at your head (a step away and it fanned the air).
		if (Target && Target->IsAlive() && FVector::Dist(Target->GetActorLocation(), Self->GetActorLocation()) < Row.MaxRange + Target->GetSimpleCollisionRadius() + 150.f)
		{
			WRForest::MarkForWasps(Target, WRForestCrImpl::Tn(TEXT("bramblewasp.markSeconds"), 30.f));
		}
		return true;
	}
	if (M == TEXT("heatball"))
	{
		// The flight to the body and the cling are the body's (TickBody): it has the active window to get there.
		Body->BallUntil = FMath::Max(Body->BallUntil, T + Row.Active);
		return true;
	}
	if (M == TEXT("bugle"))
	{
		Rally(Self, Target, Row.Radius);
		return true;
	}
	return false;
}

void FWRForestAI::Rally(AWRCreature* Self, AActor* By, float Radius)
{
	// The herd within 40 m turns on the attacker together.
	if (!Self || !By)
	{
		return;
	}
	const float R = FMath::Max(1000.f, Radius > 0.f ? Radius : WRForestCrImpl::Tn(TEXT("elderelk.bugleRadius"), 4000.f));
	int32 Turned = 0;
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* Mate = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!Mate || Mate == Self || !WRForestCrImpl::Wild(Mate) || B->KindId() != WRForestCrImpl::KindOf(Self) || FVector::Dist(Mate->GetActorLocation(), Self->GetActorLocation()) > R) continue;
		Mate->SetAsleep(false);
		if (!Mate->GetAggroTarget()) { Mate->SetAggroTarget(By); ++Turned; }
		if (AWRCreature* Wild = Cast<AWRCreature>(By); Wild && !Wild->IsTamed()) Mate->HuntTarget = Wild;
	}
	if (UWRForestBody* Body = UWRForestBody::Get(Self)) Body->BugledAt = WRForestCrImpl::Now(Self);
	UE_LOG(LogWildreach, Log, TEXT("FOREST %s bugles: %d of the herd turn"), *Self->GetName(), Turned);
}

void FWRForestAI::Resolved(AWRCreature* Self, AWRCharacterBase* Victim, FName RowId, EWRHitResult Result)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || !Victim || !Self->IsAlive())
	{
		return;
	}
	const double T = WRForestCrImpl::Now(Self);
	const bool bLanded = Result == EWRHitResult::Hit || Result == EWRHitResult::GuardBroken;
	if (RowId == TEXT("canopy_drop") && bLanded && Victim->IsAlive() && FMath::FRand() < WRForestCrImpl::Tn(TEXT("thornlynx.biteAfterDrop"), 0.5f))
	{
		// Off the drop straight into the fang bite (its gape is the tell).
		Body->ForceNext = TEXT("fang_bite");
		Body->ForceUntil = T + 2.5;
		if (AWRCreatureAI* AI = WRForestCrImpl::AIOf(Self); AI && AI->RowPhase == 4) AI->RowPhaseEnd = FMath::Min(AI->RowPhaseEnd, T + 0.25);
	}
	else if (RowId == TEXT("log_strike") && bLanded && Victim->IsAlive() && FMath::FRand() < WRForestCrImpl::Tn(TEXT("rootcoil.constrictAfterStrike"), 0.5f))
	{
		Body->ForceNext = TEXT("constrict");
		Body->ForceUntil = T + 2.5;
		if (AWRCreatureAI* AI = WRForestCrImpl::AIOf(Self); AI && AI->RowPhase == 4) AI->RowPhaseEnd = FMath::Min(AI->RowPhaseEnd, T + 0.25);
	}
	else if ((RowId == TEXT("maul_hug") || RowId == TEXT("constrict")) && bLanded && Victim->IsAlive())
	{
		BeginHold(Self, Victim);
	}
}

void FWRForestAI::ModifyHit(const AWRCreature* Self, FWRHit& Hit, const FWRAttackRow* Row)
{
	// A Thornlynx's fang bite straight off the drop goes in deep (the cat is still braced on its landing).
	const UWRForestBody* Body = UWRForestBody::Get(Self);
	if (Body && Row && Row->Id == TEXT("fang_bite") && WRForestCrImpl::Now(Self) - Body->DropStarted < 3.0)
	{
		if (float* Bleed = Hit.Buildups.Find(TEXT("bleed"))) *Bleed *= WRForestCrImpl::Tn(TEXT("thornlynx.dropBiteBleedMult"), 1.25f);
	}
}

void FWRForestAI::AfterRowHit(AWRCreature* Self, AWRCharacterBase* Victim, const FWRAttackRow& Row)
{
	// A charge throws a rider off their mount (combat.md 2.19).
	if (!Self || !Victim || Row.Motion != TEXT("charge") || Row.Impact < 5)
	{
		return;
	}
	if (AWRCreature* Mount = Cast<AWRCreature>(Victim->GetAttachParentActor()); Mount && Mount->GetRider() == Victim)
	{
		Mount->Unhorse(Self);
	}
}

float FWRForestAI::Incoming(const AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	const UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || Hit.bDamageOverTime)
	{
		return Damage;
	}
	const FName K = Body->KindId();
	if (K == TEXT("mossmaul") && Body->Has(WRForestFlag::Reared))
	{
		// Reared, the belly is open: x1.3 on the chest and belly.
		if (Hit.ImpactPoint.IsZero() || Self->IsNearLockPoint(Hit.ImpactPoint, TEXT("chest"), Self->GetSimpleCollisionRadius() * 1.3f))
		{
			Damage *= WRForestCrImpl::Tn(TEXT("mossmaul.bellyMult"), 1.3f);
		}
	}
	else if (K == TEXT("burlbeast") && Hit.PhysType == 2 && !Hit.ImpactPoint.IsZero())
	{
		// Strike on the bark plates (the back) glances: x0.8 there only.
		if (Hit.ImpactPoint.Z > Self->GetActorLocation().Z + Self->GetSimpleCollisionHalfHeight() * WRForestCrImpl::Tn(TEXT("burlbeast.backAbove"), 0.35f))
		{
			Damage *= WRForestCrImpl::Tn(TEXT("burlbeast.backStrike"), 0.8f);
		}
	}
	else if (K == TEXT("bramblewasp") && WRForestCrImpl::KindOf(Hit.Instigator) == TEXT("hiveback"))
	{
		// The bee ball: x3 on wasps (real Japanese honeybees cook a hornet).
		Damage *= WRForestCrImpl::Tn(TEXT("hiveback.waspMult"), 3.f);
	}
	return Damage;
}

void FWRForestAI::Damaged(AWRCreature* Self, const FWRHit& Hit, float Damage)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || !Self->HasAuthority())
	{
		return;
	}
	const double T = WRForestCrImpl::Now(Self);
	const FName K = Body->KindId();
	AActor* By = Hit.Instigator;
	// Holding someone: an ally's blows on its head free them (strike x1.5 on a Rootcoil); fire on a Rootcoil frees them now.
	if (Body->Hold.IsValid() && By != Body->Hold.Get() && Damage > 0.f)
	{
		const bool bFire = Hit.Element == WRTags::Element_Fire || Hit.BurnBuildup > 0.f;
		if (K == TEXT("rootcoil") && bFire)
		{
			ReleaseHold(Self, true);
		}
		else if (Hit.ImpactPoint.IsZero() || Self->IsNearLockPoint(Hit.ImpactPoint, TEXT("head"), Self->GetSimpleCollisionRadius() * 1.3f))
		{
			Body->FreeDamage += Damage * (K == TEXT("rootcoil") && Hit.PhysType == 2 ? WRForestCrImpl::Tn(TEXT("rootcoil.strikeFreeMult"), 1.5f) : 1.f);
			if (Body->FreeDamage >= FWRTuning::Get().Num(FString::Printf(TEXT("creatureAI.%s.headFree"), *K.ToString()), 120.f))
			{
				UE_LOG(LogWildreach, Log, TEXT("FOREST %s lets go (struck free)"), *Self->GetName());
				ReleaseHold(Self, true);
			}
		}
	}
	// A clinging bee swatted: knocked off.
	if (Body->ClingTo.IsValid() && Damage > 0.f)
	{
		Uncling(Self, true);
	}
	// A path-magic tame broken by a blow.
	if (!Body->PathTamer.IsEmpty() && Damage > 0.f)
	{
		if (AWRCharacter* Tamer = WRForestCrImpl::OwnerBody(Self->GetWorld(), Body->PathTamer)) WRForestCrImpl::Tell(Tamer, TEXT("The blow breaks the calm: the moss closes over"));
		Body->PathTamer.Empty();
	}
	if (Self->IsTamed())
	{
		return;
	}
	if (K == TEXT("thornlynx") && Body->PerchStage >= 2 && Damage > 0.f)
	{
		// An arrow, a stone, a lash up the trunk brings it down: it falls, and is down a moment where it lands.
		Body->PerchStage = 0;
		Body->bLanded = false;
		Body->DownedUntil = T + 30.0;
		WRForestCrImpl::Unplace(Self, true);
		Self->LaunchCharacter((Self->GetActorLocation() - Body->TrunkAt).GetSafeNormal2D() * 250.f, true, true);
		WRFx::Broadcast(Self, TEXT("forest.cr_leaves"), Self->GetActorLocation(), FRotator::ZeroRotator, 1.5f, TEXT("forest.canopy_rustle"));
		if (AWRCharacterBase* Who = Cast<AWRCharacterBase>(By); Who && Who != Self) Self->SetAggroTarget(Who);
	}
	else if (K == TEXT("rootcoil") && Body->Has(WRForestFlag::LogRest))
	{
		Body->Set(WRForestFlag::LogRest, false);
		Body->RestAgainAt = T + WRForestCrImpl::Tn(TEXT("rootcoil.restAgain"), 20.f);
		if (AWRCharacterBase* Who = Cast<AWRCharacterBase>(By); Who && Who != Self) Self->SetAggroTarget(Who);
	}
	else if (K == TEXT("elderelk") && T - Body->BugledAt > WRForestCrImpl::Tn(TEXT("elderelk.bugleCooldown"), 10.f) && By && By != Self)
	{
		Body->BugleWantedAt = T;
		Body->BugleAt = By;
		if (!Self->GetAggroTarget()) Self->SetAggroTarget(By);
	}
	// Neutral herds defend each other against a wild attacker.
	if (const AWRCreature* WildBy = Cast<AWRCreature>(By); WildBy && !WildBy->IsTamed() && Self->Spawner.IsValid() && (K == TEXT("burlbeast") || K == TEXT("elderelk") || K == TEXT("truffler") || K == TEXT("hiveback")))
	{
		for (TActorIterator<AWRCreature> It(Self->GetWorld()); It; ++It)
		{
			if (*It == Self || !WRForestCrImpl::Wild(*It) || It->Spawner != Self->Spawner || It->GetAggroTarget()) continue;
			It->HuntTarget = const_cast<AWRCreature*>(WildBy);
			It->SetAggroTarget(const_cast<AWRCreature*>(WildBy));
		}
	}
}

void FWRForestAI::Died(AWRCreature* Self, AActor* Killer)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body)
	{
		return;
	}
	if (Body->Hold.IsValid()) ReleaseHold(Self, false);
	if (Body->ClingTo.IsValid()) Uncling(Self, false);
	Body->BallTarget = nullptr;
	Body->PathTamer.Empty();
	if (Body->PerchStage > 0 || Body->bDropping)
	{
		Body->PerchStage = 0;
		Body->bDropping = false;
		WRForestCrImpl::Unplace(Self, true);
	}
	const FName K = Body->KindId();
	// Chance drops (creatures.json lootChance) to the killer, or the killing tame's owner.
	if (!Self->IsTamed())
	{
		AActor* Getter = Killer;
		if (const AWRCreature* Tame = Cast<AWRCreature>(Killer); Tame && Tame->IsTamed()) Getter = WRForestCrImpl::OwnerBody(Self->GetWorld(), Tame->TamedBy);
		const TSharedPtr<FJsonObject> R = WRForestCrImpl::Row(Self->CreatureId);
		const TSharedPtr<FJsonObject>* Chances = nullptr;
		UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Getter);
		if (Inv && R.IsValid() && R->TryGetObjectField(TEXT("lootChance"), Chances))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*Chances)->Values)
			{
				const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
				if (O.IsValid() && FMath::FRand() < WRForestCrImpl::Num(O, TEXT("chance"), 0.f)) Inv->AddItem(FName(*KV.Key), FMath::Max(1, int32(WRForestCrImpl::Num(O, TEXT("count"), 1.f))));
			}
		}
	}
	// A Mossmaul's kill of a Burlbeast: it pulls moss over the carcass and keeps it (the Bear's Larder).
	if (K == TEXT("burlbeast") && WRForestCrImpl::KindOf(Killer) == TEXT("mossmaul"))
	{
		AWRCreature* Maul = Cast<AWRCreature>(Killer);
		if (UWRForestBody* MB = UWRForestBody::Get(Maul); MB && !Maul->IsTamed())
		{
			MB->LarderAt = Self->GetActorLocation();
			MB->LarderUntil = WRForestCrImpl::Now(Self) + WRForestCrImpl::Tn(TEXT("mossmaul.larderSeconds"), 900.f);
			MB->Set(WRForestFlag::Larder, true);
			Body->LarderOf = Maul;
			Body->Set(WRForestFlag::Larder, true);
			UE_LOG(LogWildreach, Log, TEXT("FOREST %s caches %s under moss"), *Maul->GetName(), *Self->GetName());
		}
	}
	// A fresh kill in the forest: a sign the Glimmerfinches lift over.
	if (WRForest::Contains(Self->GetActorLocation()) && !IsWasp(Self) && K != TEXT("glimmerfinch"))
	{
		UWRForestLife::NoteKill(Self->GetWorld(), Self->GetActorLocation());
	}
	// (A Bramblewasp has no death burst: real hornets don't burst.)
}

bool FWRForestAI::OwnsPathMagic(const AWRCreature* Self)
{
	return WRForestCrImpl::KindOf(Self) == TEXT("mossmaul");
}

namespace WRForestCrImpl
{
	FString NameOf(const AActor* Who)
	{
		const APawn* P = Cast<APawn>(Who);
		const APlayerState* PS = P ? P->GetPlayerState() : nullptr;
		return PS ? PS->GetPlayerName() : FString();
	}

	/** The Mossmaul is open to the calm: knocked out, or below 30% and held by roots. */
	bool PathCalmable(const AWRCreature* Self)
	{
		return Self->IsUnconscious() || (Self->GetHealthFraction() < Tt(TEXT("pathTameBelow"), 0.3f) && WRForest::IsEntangled(Self));
	}
}

bool FWRForestAI::PathTamePrompt(const AWRCreature* Self, const AActor* Who, FString& OutVerb)
{
	const UWRForestBody* Body = UWRForestBody::Get(Self);
	const FWRCreatureDef* Def = Self ? Self->GetDef() : nullptr;
	if (!Body || !Def || !Who || Self->IsTamed() || !Self->IsAlive() || !OwnsPathMagic(Self) || !Def->CanTameBy(TEXT("path_magic")))
	{
		return false;
	}
	const FString Me = WRForestCrImpl::NameOf(Who);
	if (!Body->PathTamer.IsEmpty())
	{
		if (Body->PathTamer != Me) return false;
		OutVerb = TEXT("Hold still: ferns are growing into its moss");
		return true;
	}
	if (!WRForestCrImpl::PathCalmable(Self) || WRForest::NaturePoints(Who) < FMath::RoundToInt(WRForestCrImpl::Tt(TEXT("pathTamePoints"), 10.f))
		|| !WRNature::HasGrowth(Who, WRForestCrImpl::Tt(TEXT("pathTameGrowth"), 40.f)))
	{
		return false;
	}
	OutVerb = FString::Printf(TEXT("Hold: grow ferns into its moss (%.0f s, %.0f Growth)"), WRForestCrImpl::Tt(TEXT("pathTameSeconds"), 4.f), WRForestCrImpl::Tt(TEXT("pathTameGrowth"), 40.f));
	return true;
}

bool FWRForestAI::TryPathTame(AWRCreature* Wild, AWRPlayerController* Player, FString& OutResult)
{
	UWRForestBody* Body = UWRForestBody::Get(Wild);
	AWRCharacter* Who = Player ? Cast<AWRCharacter>(Player->GetPawn()) : nullptr;
	FString Verb;
	if (WRForestCrImpl::GTapFeeding || !Body || !Who || !Wild->HasAuthority() || !PathTamePrompt(Wild, Who, Verb))
	{
		return false;
	}
	const FString Me = WRForestCrImpl::NameOf(Who);
	if (Body->PathTamer == Me)
	{
		OutResult = Verb;
		return true;
	}
	const float Reach = Wild->GetSimpleCollisionRadius() + WRForestCrImpl::Tt(TEXT("pathTameReach"), 300.f);
	if (FVector::Dist2D(Who->GetActorLocation(), Wild->GetActorLocation()) > Reach)
	{
		OutResult = TEXT("Get your hand on its moss");
		return true;
	}
	Body->PathTamer = Me;
	Body->PathTameStart = WRForestCrImpl::Now(Wild);
	Body->DigAt = Who->GetActorLocation();   // where the hand went on (moving off it breaks the calm)
	Wild->SetAggroTarget(nullptr);
	WRFx::Broadcast(Wild, TEXT("forest.cr_bloom_burst"), Wild->GetActorLocation() + FVector(0.f, 0.f, Wild->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.f, TEXT("nature.cast"));
	UE_LOG(LogWildreach, Log, TEXT("FOREST %s lays a hand on %s's moss (path-magic tame)"), *Me, *Wild->GetName());
	OutResult = FString::Printf(TEXT("Hold E: ferns grow from your hand into its moss (%.0f s)"), WRForestCrImpl::Tt(TEXT("pathTameSeconds"), 4.f));
	return true;
}

void FWRForestAI::StopPathTame(AWRPlayerController* Player)
{
	const FString Me = WRForestCrImpl::NameOf(Player ? Player->GetPawn() : nullptr);
	if (Me.IsEmpty())
	{
		return;
	}
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* Wild = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!Wild || B->PathTamer != Me) continue;
		const double Held = WRForestCrImpl::Now(Wild) - B->PathTameStart;
		B->PathTamer.Empty();
		if (Held < WRForestCrImpl::Tt(TEXT("pathTameTap"), 0.35f))
		{
			// A tap: it's fed the usual way instead.
			TGuardValue<bool> Guard(WRForestCrImpl::GTapFeeding, true);
			Player->ClientNotify(Wild->Feed(Player));
		}
		else
		{
			Player->ClientNotify(TEXT("You let go: the moss closes over again"));
		}
		return;
	}
}

bool FWRForestAI::PathTameNear(const APlayerController* Player)
{
	const FString Me = WRForestCrImpl::NameOf(Player ? Player->GetPawn() : nullptr);
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (Me.IsEmpty() || !Pawn)
	{
		return false;
	}
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		const AActor* Wild = B.IsValid() ? B->GetOwner() : nullptr;
		if (Wild && B->PathTamer == Me && FVector::Dist(Wild->GetActorLocation(), Pawn->GetActorLocation()) < 1500.f) return true;
	}
	return false;
}

void FWRForestAI::FinishPathTame(AWRCreature* Mossmaul, AWRCharacter* Tamer)
{
	UWRForestBody* Body = UWRForestBody::Get(Mossmaul);
	if (!Body || !Tamer)
	{
		return;
	}
	Body->PathTamer.Empty();
	if (!WRNature::SpendGrowth(Tamer, WRForestCrImpl::Tt(TEXT("pathTameGrowth"), 40.f)))
	{
		WRForestCrImpl::Tell(Tamer, FString::Printf(TEXT("You haven't the Growth (%.0f) to finish the calm"), WRForestCrImpl::Tt(TEXT("pathTameGrowth"), 40.f)));
		return;
	}
	// A full meal of narcotic and kibble: under, and kept under; taming goes on as knockout-and-feed.
	Mossmaul->AddTorpor(Mossmaul->MaxTorpor * 2.f);
	Mossmaul->Torpor = Mossmaul->MaxTorpor;
	Mossmaul->TameProgress = FMath::Min(0.99f, Mossmaul->TameProgress + WRForestCrImpl::Tt(TEXT("pathTameMeal"), 0.15f));
	WRFx::Broadcast(Mossmaul, TEXT("forest.cr_bloom_burst"), Mossmaul->GetActorLocation() + FVector(0.f, 0.f, Mossmaul->GetSimpleCollisionHalfHeight()), FRotator::ZeroRotator, 1.6f, TEXT("nature.heal"));
	WRForestCrImpl::Tell(Tamer, FString::Printf(TEXT("Ferns grow through its moss and the %s sleeps: feed it to finish the tame"), *Mossmaul->GetDef()->Name));
	UE_LOG(LogWildreach, Log, TEXT("FOREST path-magic calm done on %s by %s"), *Mossmaul->GetName(), *Tamer->GetName());
}

void FWRForestAI::Whistled(AWRCreature* Tame, AActor* Owner)
{
	UWRForestBody* Body = UWRForestBody::Get(Tame);
	if (!Body || !Tame->IsTamed() || Body->KindId() != TEXT("glimmerfinch") || Tame->IsBaby())
	{
		return;
	}
	// The flock lifts and circles over the nearest ripe wild plant or hive within 60 m of you.
	const FVector From = Owner ? Owner->GetActorLocation() : Tame->GetActorLocation();
	float Best = WRForestCrImpl::Tt(TEXT("whistleRange"), 6000.f);
	FVector Found = FVector::ZeroVector;
	TSet<FName> Ripe;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	const TSharedPtr<FJsonObject> Cfg = FWRTuning::Get().Object(TEXT("tame.forest"));
	if (Cfg.IsValid() && Cfg->TryGetArrayField(TEXT("ripeItems"), List)) for (const TSharedPtr<FJsonValue>& V : *List) Ripe.Add(FName(*V->AsString()));
	if (Ripe.Num() == 0) Ripe = { TEXT("meadow_oats"), TEXT("idunn_apple"), TEXT("honey"), TEXT("berries") };
	for (TActorIterator<AWRResourceNode> It(Tame->GetWorld()); It; ++It)
	{
		if (It->IsDepleted() || !Ripe.Contains(It->ItemId)) continue;
		const float D = FVector::Dist(It->GetActorLocation(), From);
		if (D < Best) { Best = D; Found = It->GetActorLocation(); }
	}
	for (TActorIterator<AWRBuildPiece> It(Tame->GetWorld()); It; ++It)
	{
		const FWRPieceDef* D = It->bGhost ? nullptr : It->GetDef();
		if (!D || D->Kind != TEXT("hive")) continue;
		const float Dist = FVector::Dist(It->GetActorLocation(), From);
		if (Dist < Best) { Best = Dist; Found = It->GetActorLocation(); }
	}
	if (Found.IsZero())
	{
		WRForestCrImpl::Tell(Owner, TEXT("Your Glimmerfinches circle and come back: nothing ripe within 60 m"));
		return;
	}
	Body->Spot = Found;
	Body->LiftUntil = WRForestCrImpl::Now(Tame) + WRForestCrImpl::Tt(TEXT("whistleLift"), 25.f);
	WRFx::Broadcast(Tame, TEXT("forest.cr_shimmer_burst"), Tame->GetActorLocation(), FRotator::ZeroRotator, 1.2f, NAME_None);
	WRSound::Broadcast(Tame, TEXT("voice.idle"));
}

bool FWRForestAI::OutInRain(const UObject* WorldContext, FName CreatureId, const FVector& At)
{
	static const TArray<FName> Fallback = { TEXT("mossmaul"), TEXT("rootcoil") };
	TArray<FName> Hunters;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	const TSharedPtr<FJsonObject> Cfg = FWRTuning::Get().Object(TEXT("creatureAI.forest"));
	if (Cfg.IsValid() && Cfg->TryGetArrayField(TEXT("rainHunters"), List)) for (const TSharedPtr<FJsonValue>& V : *List) Hunters.Add(FName(*V->AsString()));
	if (!(Hunters.Num() > 0 ? Hunters : Fallback).Contains(CreatureId)) return false;
	return !AWRGameState::IsNightIn(WorldContext) && WRForest::Raining(WorldContext, At);
}

bool FWRForestAI::FinchPollinates(const UObject* WorldContext, const FVector& At)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const float Range = WRForestCrImpl::Tt(TEXT("pollinateRange"), 2000.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		const AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (C && C->GetWorld() == World && C->IsAlive() && C->IsTamed() && B->KindId() == TEXT("glimmerfinch") && FVector::Dist(C->GetActorLocation(), At) <= Range) return true;
	}
	return false;
}

// ---------------------------------------------------------------- the body's server life

void FWRForestAI::BeginHold(AWRCreature* Self, AWRCharacterBase* Victim)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body || !Victim || !Victim->IsAlive() || Body->Hold.IsValid() || WRForestGrip::IsHeld(Victim) || !Self->IsAlive())
	{
		return;
	}
	UWRForestTouch* Touch = UWRForestTouch::FindOrAdd(Victim);
	if (!Touch)
	{
		return;
	}
	const double T = WRForestCrImpl::Now(Self);
	const FName K = Body->KindId();
	const bool bHug = K == TEXT("mossmaul");
	const float Seconds = bHug ? WRForestCrImpl::Tn(TEXT("mossmaul.hugSeconds"), 2.5f) : WRForestCrImpl::Tn(TEXT("rootcoil.constrictSeconds"), 3.f);
	// Off a mount first.
	if (AWRCreature* Mount = Cast<AWRCreature>(Victim->GetAttachParentActor()); Mount && Mount->GetRider() == Victim)
	{
		Mount->Unhorse(Self);
	}
	Body->Hold = Victim;
	Body->HoldStart = T;
	Body->HoldEnds = T + Seconds;
	Body->HoldTickAt = T + WRForestCrImpl::Tn(TEXT("forest.holdTickSeconds"), 0.5f);
	Body->FreeDamage = 0.f;
	Body->Set(WRForestFlag::Holding, true);
	Touch->HeldBy = Self;
	Victim->Status->Stagger(Seconds + 0.3f);
	Victim->GetCharacterMovement()->DisableMovement();
	Victim->SetActorEnableCollision(false);
	Victim->AttachToActor(Self, FAttachmentTransformRules::KeepWorldTransform);
	const float R = Self->GetSimpleCollisionRadius(), HH = Self->GetSimpleCollisionHalfHeight();
	if (bHug)
	{
		// Crushed to the chest, lifted off the ground as it stands tall.
		Victim->SetActorRelativeLocation(FVector(R + Victim->GetSimpleCollisionRadius() * 0.5f, 0.f, HH * WRForestCrImpl::Tn(TEXT("mossmaul.hugLift"), 0.35f)));
		Victim->SetActorRelativeRotation(FRotator(0.f, 180.f, 0.f));
	}
	else
	{
		// In the coils where they slid round you (the coil look is drawn there).
		Victim->SetActorRelativeLocation(FVector(R + WRForestCrImpl::Tn(TEXT("rootcoil.holdForward"), 220.f), 0.f, -HH + Victim->GetSimpleCollisionHalfHeight() + 20.f));
		Victim->SetActorRelativeRotation(FRotator(0.f, 180.f, 0.f));
		// The squeeze takes the wind out of you: stamina -30.
		if (UWRAttributeSet* A = Victim->GetAttributes(); A && Victim->GetAbilitySystemComponent())
		{
			Victim->GetAbilitySystemComponent()->SetNumericAttributeBase(UWRAttributeSet::GetStaminaAttribute(), FMath::Max(0.f, A->GetStamina() - WRForestCrImpl::Tn(TEXT("rootcoil.constrictStamina"), 30.f)));
		}
	}
	Self->SetAggroTarget(Victim);
	WRFx::Broadcast(Self, TEXT("forest.cr_grab"), Victim->GetActorLocation(), FRotator::ZeroRotator, 1.f, NAME_None);
	WRSound::Broadcast(Self, TEXT("voice.attack"));
	Signature(Self);
	WRForestCrImpl::Tell(Victim, bHug ? TEXT("Caught in its hug: mash to break free (an ally's blows on its head free you)")
		: TEXT("The coils close round you: mash to break free (blows on its head free you; fire frees you at once)"));
	UE_LOG(LogWildreach, Log, TEXT("FOREST %s %s %s (%.1f s)"), *Self->GetName(), bHug ? TEXT("hugs") : TEXT("constricts"), *Victim->GetName(), Seconds);
}

void FWRForestAI::ReleaseHold(AWRCreature* Self, bool bThrow)
{
	UWRForestBody* Body = UWRForestBody::Get(Self);
	if (!Body)
	{
		return;
	}
	AWRCharacterBase* V = Body->Hold.Get();
	const bool bHug = Body->KindId() == TEXT("mossmaul");
	Body->Hold = nullptr;
	Body->Set(WRForestFlag::Holding, false);
	if (!V)
	{
		return;
	}
	if (UWRForestTouch* Touch = UWRForestTouch::Get(V)) Touch->HeldBy = nullptr;
	V->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector Fwd = Self->GetActorForwardVector().GetSafeNormal2D();
	FVector At = V->GetActorLocation();
	At.Z = FMath::Max(At.Z, WRForestCrImpl::Feet(Self).Z + V->GetSimpleCollisionHalfHeight() + 20.f);
	V->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	V->SetActorRotation(FRotator(0.f, (Self->GetActorLocation() - At).Rotation().Yaw, 0.f));
	V->SetActorEnableCollision(true);
	V->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	V->Status->ClearStatus(WRTags::Status_Staggered);
	if (bThrow && V->IsAlive())
	{
		// The hug ends in a throw: 5 m and knocked down where you land. The coils just let go.
		if (bHug)
		{
			V->LaunchCharacter(Fwd * WRForestCrImpl::Tn(TEXT("mossmaul.throwPush"), 700.f) + FVector(0.f, 0.f, 380.f), true, true);
			V->Status->Stagger(WRForestCrImpl::Tn(TEXT("mossmaul.throwDown"), 1.2f));
		}
		else
		{
			V->LaunchCharacter(Fwd * 250.f + FVector(0.f, 0.f, 200.f), true, true);
		}
	}
}

void FWRForestAI::Cling(AWRCreature* Bee, AWRCharacterBase* Victim)
{
	UWRForestBody* Body = UWRForestBody::Get(Bee);
	if (!Body || !Victim || !Victim->IsAlive() || Body->ClingTo.IsValid())
	{
		return;
	}
	UWRForestTouch* Touch = UWRForestTouch::FindOrAdd(Victim);
	const int32 Max = FMath::RoundToInt(WRForestCrImpl::Tn(TEXT("hiveback.maxCling"), 4.f));
	if (!Touch || Touch->Bees.Num() >= Max)
	{
		return;
	}
	TSet<int32> Used;
	for (const TWeakObjectPtr<AWRCreature>& B : Touch->Bees) if (const UWRForestBody* BB = UWRForestBody::Get(B.Get())) Used.Add(BB->ClingSlot);
	int32 Slot = 0;
	while (Used.Contains(Slot) && Slot < Max) ++Slot;
	const double T = WRForestCrImpl::Now(Bee);
	const bool bWasp = IsWasp(Victim);
	Body->ClingTo = Victim;
	Body->ClingSlot = Slot;
	Body->ClingUntil = T + (bWasp ? WRForestCrImpl::Tn(TEXT("hiveback.waspKillSeconds"), 3.f) : WRForestCrImpl::Tn(TEXT("hiveback.clingSeconds"), 3.f));
	Body->ClingTickAt = T + 0.5;
	Body->BallTarget = nullptr;
	Body->Set(WRForestFlag::Clinging, true);
	Touch->Bees.Add(Bee);
	Touch->Clinging = uint8(Touch->Bees.Num());
	if (bWasp) Touch->HeldBy = Bee;
	Bee->SetActorEnableCollision(false);
	Bee->GetCharacterMovement()->DisableMovement();
	Bee->AttachToActor(Victim, FAttachmentTransformRules::KeepWorldTransform);
	// Round the body: left, right, back, chest, at the shoulders' height.
	const float R = Victim->GetSimpleCollisionRadius(), H = Victim->GetSimpleCollisionHalfHeight();
	static const FVector Around[] = { FVector(0.f, -1.f, 0.35f), FVector(0.f, 1.f, 0.35f), FVector(-1.f, 0.f, 0.5f), FVector(1.f, 0.f, 0.15f) };
	const FVector Dir = Around[Slot % 4];
	Bee->SetActorRelativeLocation(FVector(Dir.X * (R + Bee->GetSimpleCollisionRadius() * 0.5f), Dir.Y * (R + Bee->GetSimpleCollisionRadius() * 0.5f), Dir.Z * H));
	Bee->SetActorRelativeRotation(FRotator(0.f, (-FVector(Dir.X, Dir.Y, 0.f)).Rotation().Yaw, 0.f));
	WRSound::Broadcast(Bee, TEXT("forest.heat_ball"));
	if (!bWasp) WRForestCrImpl::Tell(Victim, TEXT("Hivebacks cling to you, buzzing hot: roll to shake one off, or fire to burn them all off"));
}

void FWRForestAI::Uncling(AWRCreature* Bee, bool bKnocked)
{
	UWRForestBody* Body = UWRForestBody::Get(Bee);
	if (!Body)
	{
		return;
	}
	AWRCharacterBase* V = Body->ClingTo.Get();
	Body->ClingTo = nullptr;
	Body->BallTarget = nullptr;
	Body->Set(WRForestFlag::Clinging, false);
	if (UWRForestTouch* Touch = UWRForestTouch::Get(V))
	{
		Touch->Bees.Remove(Bee);
		Touch->Clinging = uint8(Touch->Bees.Num());
		if (Touch->HeldBy == Bee) Touch->HeldBy = nullptr;
	}
	Bee->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Bee->SetActorEnableCollision(true);
	Bee->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Bee->SetActorRotation(FRotator(0.f, Bee->GetActorRotation().Yaw, 0.f));
	if (bKnocked && Bee->IsAlive() && V)
	{
		Bee->LaunchCharacter((Bee->GetActorLocation() - V->GetActorLocation()).GetSafeNormal() * 400.f + FVector(0.f, 0.f, 150.f), true, true);
		Bee->Status->Stagger(WRForestCrImpl::Tn(TEXT("hiveback.knockedStagger"), 0.6f));
	}
}

void FWRForestAI::TickBody(AWRCreature* Self, UWRForestBody* Body, float DeltaTime)
{
	const double T = WRForestCrImpl::Now(Self);
	const FName K = Body->KindId();
	AWRCreatureAI* AI = WRForestCrImpl::AIOf(Self);
	UWorld* World = Self->GetWorld();
	const float Diff = WRForestCrImpl::Difficulty();

	// --- The holds: the Maul hug and the Constrict ---
	if (Body->Hold.IsValid())
	{
		AWRCharacterBase* V = Body->Hold.Get();
		const bool bHug = K == TEXT("mossmaul");
		if (!V->IsAlive() || !Self->IsAlive() || Self->IsUnconscious() || Self->HasTag(WRTags::Status_StanceBroken))
		{
			ReleaseHold(Self, false);
		}
		else if (!bHug && Self->HasTag(WRTags::Status_Burn))
		{
			ReleaseHold(Self, true);   // fire on the coils frees you at once
		}
		else if (T >= Body->HoldEnds)
		{
			ReleaseHold(Self, true);
		}
		else if (T >= Body->HoldTickAt)
		{
			const float Tick = WRForestCrImpl::Tn(TEXT("forest.holdTickSeconds"), 0.5f);
			Body->HoldTickAt = T + Tick;
			const float Dps = bHug ? WRForestCrImpl::Tn(TEXT("mossmaul.hugDps"), 18.f) : WRForestCrImpl::Tn(TEXT("rootcoil.constrictDps"), 20.f);
			WRForestCrImpl::Dot(Self, V, Dps * Tick * Diff);
			V->Status->Stagger(float(Body->HoldEnds - T) + 0.3f);
		}
	}

	// --- A heat ball: flying at its body, then clinging and cooking it ---
	if (Body->ClingTo.IsValid())
	{
		AWRCharacterBase* V = Body->ClingTo.Get();
		const bool bWasp = IsWasp(V);
		if (!V->IsAlive() || !Self->IsAlive() || Self->IsUnconscious())
		{
			Uncling(Self, false);
		}
		else if (T >= Body->ClingUntil)
		{
			// A wasp in the ball is cooked by the end of it.
			if (bWasp && V->GetAttributes())
			{
				WRForestCrImpl::Dot(Self, V, V->GetAttributes()->GetHealth() + 1.f);
			}
			Uncling(Self, false);
		}
		else if (T >= Body->ClingTickAt)
		{
			Body->ClingTickAt = T + 0.5;
			WRForestCrImpl::Dot(Self, V, WRForestCrImpl::Tn(TEXT("hiveback.clingDps"), 6.f) * 0.5f * Diff);
		}
	}
	else if (Body->BallTarget.IsValid())
	{
		AWRCharacterBase* V = Body->BallTarget.Get();
		if (!V->IsAlive() || T >= Body->BallUntil || !Self->IsAlive())
		{
			Body->BallTarget = nullptr;
		}
		else
		{
			const FVector Goal = V->GetActorLocation() + FVector(0.f, 0.f, V->GetSimpleCollisionHalfHeight() * 0.3f);
			WRForestCrImpl::HoverTo(Self, Goal, 1.f);
			if (FVector::Dist(Goal, Self->GetActorLocation()) < V->GetSimpleCollisionRadius() + Self->GetSimpleCollisionRadius() + 80.f)
			{
				Cling(Self, V);
			}
		}
	}

	// --- A Thornlynx on a trunk: to its foot, up it, hanging, down again; dropping off it; knocked off it ---
	if (K == TEXT("thornlynx") && Self->IsAlive())
	{
		UCharacterMovementComponent* Move = Self->GetCharacterMovement();
		const float ClimbSpeed = WRForestCrImpl::Tn(TEXT("thornlynx.climbSpeed"), 320.f);
		const float Around = WRForestCrImpl::Tn(TEXT("thornlynx.climbTurn"), 40.f);
		if (Body->PerchStage == 1)
		{
			// The perch's angle round its trunk, and the foot of the trunk below it.
			const FVector Off = Body->PerchAt - Body->TrunkAt;
			Body->ClimbAngle = FMath::RadiansToDegrees(FMath::Atan2(Off.Y, Off.X)) - Around;
			bool bOk = false;
			Body->TrunkFoot = WRForestCrImpl::Ground(World, WRForestCrImpl::OnTrunk(Body, Self, Body->ClimbAngle, 0.f), bOk, Self);
			if (bOk) Body->TrunkAt.Z = Body->TrunkFoot.Z;
			const FVector To = Body->TrunkFoot - WRForestCrImpl::Feet(Self);
			if (Body->PerchSince <= 0.0) Body->PerchSince = T;
			if (To.Size2D() > 90.f && T - Body->PerchSince < WRForestCrImpl::Tn(TEXT("thornlynx.reachTrunkSeconds"), 30.f))
			{
				Self->AddMovementInput(To.GetSafeNormal2D(), Body->bEscape ? 1.f : 0.55f);
			}
			else if (To.Size2D() > 250.f)
			{
				// Couldn't get to the trunk (a brake, a bank): it gives the perch up.
				Body->PerchStage = 0;
				Body->PerchSince = 0.0;
				Body->bEscape = false;
			}
			else
			{
				Body->PerchStage = 2;
				Body->ClimbStarted = T;
				Body->PerchSince = 0.0;
				Move->bOrientRotationToMovement = false;
				WRSound::Broadcast(Self, TEXT("forest.canopy_rustle"));
			}
		}
		else if (Body->PerchStage == 2 || Body->PerchStage == 4)
		{
			// Up (or down) the bark in a slow quarter-turn round the trunk, belly to the bark.
			const float Height = FMath::Max(100.f, Body->PerchAt.Z - Body->TrunkAt.Z);
			const float U = FMath::Clamp(float(T - Body->ClimbStarted) * ClimbSpeed / Height, 0.f, 1.f);
			const float S = Body->PerchStage == 2 ? U : 1.f - U;
			const float A = Body->ClimbAngle + Around * S;
			const FVector At = WRForestCrImpl::OnTrunk(Body, Self, A, Height * S + Self->GetSimpleCollisionHalfHeight() * (1.f - S));
			WRForestCrImpl::Place(Self, At, A, DeltaTime);
			if (U >= 1.f)
			{
				if (Body->PerchStage == 2)
				{
					Body->PerchStage = 3;
					Body->PerchSince = T;
					Body->PerchUntil = T + (Body->bEscape ? WRForestCrImpl::Roll(TEXT("thornlynx.escapeWait"), 4.f, 8.f) : WRForestCrImpl::Roll(TEXT("thornlynx.perchSeconds"), 60.f, 180.f));
				}
				else
				{
					Body->PerchStage = 0;
					Body->bEscape = false;
					WRForestCrImpl::Unplace(Self, false);
					Body->NextPerchAt = T + WRForestCrImpl::Roll(TEXT("thornlynx.perchEvery"), 60.f, 180.f);
				}
			}
		}
		else if (Body->PerchStage == 3)
		{
			// Hanging head-down on the trunk, facing out over the way below.
			const float A = Body->ClimbAngle + Around;
			WRForestCrImpl::Place(Self, WRForestCrImpl::OnTrunk(Body, Self, A, Body->PerchAt.Z - Body->TrunkAt.Z), A, DeltaTime);
		}
		if (Body->bDropping)
		{
			// Down on its shadow: whatever is there takes the drop.
			if (T - Body->DropStarted > 0.08 && !Move->IsFalling())
			{
				Body->bDropping = false;
				const FWRAttackRow* Row = WRForestCrImpl::RowOf(Self, TEXT("canopy_drop"));
				const float Reach = (Row ? FMath::Max(Row->Radius, Row->Reach) : 150.f) + Self->GetSimpleCollisionRadius();
				WRFx::Broadcast(Self, TEXT("forest.cr_land"), WRForestCrImpl::Feet(Self), FRotator::ZeroRotator, 1.f, TEXT("stomp"));
				for (TActorIterator<AWRCharacterBase> It(World); It; ++It)
				{
					if (*It != Self && It->IsAlive() && Self->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < Reach + It->GetSimpleCollisionRadius()) ContactRow(Self, *It);
				}
				if (AI && AI->RowPhase == 3) AI->RowPhaseEnd = FMath::Min(AI->RowPhaseEnd, T);
			}
			else if (AI && AI->RowPhase == 3)
			{
				AI->RowPhaseEnd = FMath::Max(AI->RowPhaseEnd, T + 0.1);   // the strike lasts until it lands
			}
			if (T - Body->DropStarted > 4.0) Body->bDropping = false;
		}
		if (!Body->bLanded && !Move->IsFalling())
		{
			// Knocked off its trunk and down: on its side a moment.
			Body->bLanded = true;
			Body->DownedUntil = T + WRForestCrImpl::Tn(TEXT("thornlynx.downedSeconds"), 1.5f);
			Self->Status->Stagger(WRForestCrImpl::Tn(TEXT("thornlynx.downedSeconds"), 1.5f));
			WRFx::Broadcast(Self, TEXT("forest.cr_land"), WRForestCrImpl::Feet(Self), FRotator::ZeroRotator, 0.8f, TEXT("stomp"));
		}
		if (Body->PerchStage == 0 && !Body->bDropping && Body->bLanded && !Move->bOrientRotationToMovement)
		{
			Move->bOrientRotationToMovement = true;
		}
	}

	// --- The Rootcoil: up off its rest when it moves or fights; a ford's yaw kept while it lies ---
	if (K == TEXT("rootcoil") && Body->Has(WRForestFlag::LogRest) && (Self->GetAggroTarget() || Self->GetVelocity().Size2D() > 40.f || !Self->IsAlive()))
	{
		Body->Set(WRForestFlag::LogRest, false);
		Body->RestAgainAt = T + WRForestCrImpl::Tn(TEXT("rootcoil.restAgain"), 20.f);
	}

	// --- The Mossmaul: its roar again in a fresh fight; its larder kept a while ---
	if (K == TEXT("mossmaul"))
	{
		if (Self->GetAggroTarget()) Body->LastFightAt = T;
		else if (Body->bRoared && T - Body->LastFightAt > WRForestCrImpl::Tn(TEXT("mossmaul.roarResetSeconds"), 30.f)) Body->bRoared = false;
		if (Body->LarderUntil > 0.0 && T >= Body->LarderUntil) { Body->LarderUntil = 0.0; Body->LarderAt = FVector::ZeroVector; }
	}

	// --- The path-magic tame: the hand stays on the moss for its 4 s ---
	if (!Body->PathTamer.IsEmpty())
	{
		AWRCharacter* Tamer = WRForestCrImpl::OwnerBody(World, Body->PathTamer);
		FString Broke;
		const float Reach = Self->GetSimpleCollisionRadius() + WRForestCrImpl::Tt(TEXT("pathTameReach"), 300.f) + 100.f;
		if (!Tamer || !Self->IsAlive() || Self->IsTamed()) Broke = TEXT("The calm is broken");
		else if (FVector::Dist2D(Tamer->GetActorLocation(), Self->GetActorLocation()) > Reach || FVector::Dist2D(Tamer->GetActorLocation(), Body->DigAt) > WRForestCrImpl::Tt(TEXT("pathTameMove"), 150.f)) Broke = TEXT("You moved: the moss closes over again");
		else if (Tamer->IsSwinging() || Tamer->HasTag(WRTags::State_Dodging)) Broke = TEXT("The calm is broken");
		else if (!WRForestCrImpl::PathCalmable(Self)) Broke = TEXT("It's awake: the calm is broken");
		if (!Broke.IsEmpty())
		{
			Body->PathTamer.Empty();
			if (Tamer) WRForestCrImpl::Tell(Tamer, Broke);
		}
		else
		{
			Self->SetAggroTarget(nullptr);
			if (T - Body->PathTameStart >= WRForestCrImpl::Tt(TEXT("pathTameSeconds"), 4.f)) FinishPathTame(Self, Tamer);
		}
	}

	// --- Tame uses ---
	if (Self->IsTamed() && !Self->IsBaby() && Self->IsAlive())
	{
		AWRCharacter* Owner = WRForestCrImpl::OwnerBody(World, Self->TamedBy);
		AWRPlayerController* PC = WRForestCrImpl::OwnerPC(World, Self->TamedBy);
		const bool bWith = Owner && Self->TameOrder == EWRTameOrder::Follow && FVector::Dist(Owner->GetActorLocation(), Self->GetActorLocation()) < WRForestCrImpl::Tt(TEXT("withOwner"), 3000.f);
		const FString Name = Self->GetDef() ? Self->GetDef()->Name : FString(TEXT("tame"));
		if (K == TEXT("truffler") && bWith && WRForest::Contains(Self->GetActorLocation()))
		{
			// Truffles (twice as often in rain).
			if (T >= Body->NextTruffleAt)
			{
				Body->NextTruffleAt = T + WRForestCrImpl::Tt(TEXT("truffleEvery"), 120.f) * (WRForest::Raining(Self, Self->GetActorLocation()) ? WRForestCrImpl::Tt(TEXT("rainTruffleMult"), 0.5f) : 1.f);
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Owner); Inv && Inv->AddItem(FName(*FWRTuning::Get().Str(TEXT("tame.forest.truffleItem"), TEXT("truffle"))), 1) == 0)
				{
					Body->RootingUntil = T + 2.5;
					WRSound::Broadcast(Self, TEXT("forest.dig"));
					Self->GainBondXP(WRTune(TEXT("tame.xp.dig"), 3.f), TEXT("truffle"));
					if (PC) PC->ClientNotify(FString::Printf(TEXT("Your %s roots up a truffle"), *Name));
				}
			}
			// Buried caches: marked for its owner.
			if (T >= Body->NextCacheAt)
			{
				Body->NextCacheAt = T + WRForestCrImpl::Tt(TEXT("cacheEvery"), 30.f);
				// Rain (DC 2.9): the wet ground carries the smell, so it finds buried things twice as far off.
				const float Range = WRForestCrImpl::Num(WRForestCrImpl::Row(Self->CreatureId), TEXT("findSites"), 4000.f)
					* (WRForest::Raining(Self, Self->GetActorLocation()) ? WRForestCrImpl::Tt(TEXT("rainFindMult"), 2.f) : 1.f);
				const FWRForestData& D = FWRForestData::Get();
				int32 Found = 0;
				const auto Consider = [&](const FWRForestData::FSite& S)
				{
					if ((S.Loot.Num() > 0 || S.Kind == TEXT("thornbrake_cache")) && FVector::Dist2D(S.At, Self->GetActorLocation()) <= Range && PC)
					{
						AWRForestMark::Spawn(PC, S.At + FVector(0.f, 0.f, 60.f), TEXT("cache"), WRForestCrImpl::Tt(TEXT("markSeconds"), 30.f));
						++Found;
					}
				};
				for (const FWRForestData::FSite& S : D.Sites) Consider(S);
				for (const FWRForestData::FSite& S : D.Kit) Consider(S);
				if (Found > 0 && T - Body->NextSnortAt > 60.0)
				{
					Body->NextSnortAt = T;
					WRSound::Broadcast(Self, TEXT("voice.idle"));
					if (PC) PC->ClientNotify(FString::Printf(TEXT("Your %s smells something buried: %d place%s marked"), *Name, Found, Found == 1 ? TEXT("") : TEXT("s")));
				}
			}
			// Leaf pits: it snorts at them and their rims show.
			if (T >= Body->NextPitAt)
			{
				Body->NextPitAt = T + WRForestCrImpl::Tt(TEXT("pitEvery"), 4.f);
				if (const UWRForestHazards* Hz = UWRForestHazards::Get(World))
				{
					for (const TWeakObjectPtr<AWRLeafPit>& PitPtr : Hz->Pits)
					{
						AWRLeafPit* Pit = PitPtr.Get();
						if (!Pit || !Pit->IsWhole() || FVector::Dist2D(Pit->GetActorLocation(), Self->GetActorLocation()) > WRForestCrImpl::Tt(TEXT("pitSense"), 1500.f)) continue;
						Pit->Reveal(WRForestCrImpl::Tt(TEXT("pitRevealSeconds"), 8.f));
						if (PC) AWRForestMark::Spawn(PC, Pit->GetActorLocation() + FVector(0.f, 0.f, 40.f), TEXT("pit"), WRForestCrImpl::Tt(TEXT("pitRevealSeconds"), 8.f));
						Body->MulticastMoment(TEXT("forest.cr_snort"), TEXT("head"), 1.f);
						WRSound::Broadcast(Self, TEXT("voice.aggro"));
						break;
					}
				}
			}
			// The Hollow Root's soft spot (the root plug, WRForestHollow): it noses it out, goes to it and digs it through.
			if (Body->DigStarted < 0.0 && T >= Body->NextDigSenseAt)
			{
				Body->NextDigSenseAt = T + 2.0;
				FVector Spot;
				float SpotR = 0.f;
				if (WRForestHollow::DigSpot(Self, Spot, SpotR) && FVector::Dist2D(Spot, Self->GetActorLocation()) < WRForestCrImpl::Tt(TEXT("digSense"), 1500.f))
				{
					if (!Body->DigAt.Equals(Spot, 10.f) && PC) AWRForestMark::Spawn(PC, Spot + FVector(0.f, 0.f, 40.f), TEXT("dig"), 20.f);
					Body->DigAt = Spot;
				}
				else
				{
					Body->DigAt = FVector::ZeroVector;
				}
			}
			if (Body->DigStarted >= 0.0)
			{
				const float Dug = WRForestHollow::TrufflerDigging(Self, DeltaTime);
				if (Dug >= 1.f || (Dug < 0.f && WRForestHollow::IsDugOpen(Self)))
				{
					if (PC) PC->ClientNotify(FString::Printf(TEXT("Your %s has dug through: the ground gives into the dark below"), *Name));
					Self->GainBondXP(WRTune(TEXT("tame.xp.dig"), 3.f) * 5.f, TEXT("dig"));
					Body->DigStarted = -1.0;
					Body->DigAt = FVector::ZeroVector;
					Body->NextDigSenseAt = T + 600.0;
				}
				else if (Dug < 0.f)
				{
					Body->DigStarted = -1.0;   // not at the spot any more: it goes back to it
				}
				else
				{
					Body->RootingUntil = T + 0.5;
				}
			}
			// Forest seeds now and then.
			if (T >= Body->NextSeedAt)
			{
				// Twice as often in rain (DC 2.9: Truffler finds double).
				Body->NextSeedAt = T + WRForestCrImpl::Tt(TEXT("seedEvery"), 300.f) * (WRForest::Raining(Self, Self->GetActorLocation()) ? WRForestCrImpl::Tt(TEXT("rainTruffleMult"), 0.5f) : 1.f);
				TArray<FName> Seeds;
				const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
				const TSharedPtr<FJsonObject> Cfg = FWRTuning::Get().Object(TEXT("tame.forest"));
				if (Cfg.IsValid() && Cfg->TryGetArrayField(TEXT("seeds"), List)) for (const TSharedPtr<FJsonValue>& V : *List) Seeds.Add(FName(*V->AsString()));
				if (Seeds.Num() == 0) Seeds = { TEXT("meadow_oats_seeds"), TEXT("idunn_apple_seeds"), TEXT("seeds") };
				const FName Seed = Seeds[FMath::RandRange(0, Seeds.Num() - 1)];
				if (UWRInventoryComponent* Inv = UWRInventoryComponent::FindFor(Owner); Inv && Inv->AddItem(Seed, 1) == 0)
				{
					Body->RootingUntil = T + 2.f;
					if (PC) PC->ClientNotify(FString::Printf(TEXT("Your %s turns up a seed (%s)"), *Name, *Seed.ToString()));
				}
			}
		}
		else if (K == TEXT("burlbeast") && bWith && T >= Body->NextDragCheckAt)
		{
			// Once a game day it drags a length off a fallen giant near its owner: a bundle of heartwood in its packs.
			Body->NextDragCheckAt = T + 5.0;
			const int32 Day = WRForestCrImpl::GameDay(Self);
			if (Body->LogDragDay != Day)
			{
				const float Range = WRForestCrImpl::Tt(TEXT("dragRange"), 2500.f);
				for (const FWRForestData::FLog& L : FWRForestData::Get().Fallen)
				{
					const FVector Mid = (L.A + L.B) * 0.5f;
					if (FVector::Dist2D(FMath::ClosestPointOnSegment(Self->GetActorLocation(), L.A, L.B), Self->GetActorLocation()) > Range) continue;
					const int32 Bundle = FMath::RoundToInt(WRForestCrImpl::Num(WRForestCrImpl::Row(Self->CreatureId), TEXT("haulDrag"), 40.f));
					const int32 Left = Self->AddCargo(TEXT("heartwood"), Bundle);
					if (Left < Bundle)
					{
						Body->LogDragDay = Day;
						WRFx::Broadcast(Self, TEXT("forest.cr_dirt"), Mid, FRotator::ZeroRotator, 2.f, TEXT("forest.bark_strip"));
						Self->GainBondXP(WRTune(TEXT("tame.xp.haulMetres"), 100.f) * 0.2f, TEXT("haul"));
						if (PC) PC->ClientNotify(FString::Printf(TEXT("Your %s drags a length off the fallen giant: %d heartwood in its packs"), *Name, Bundle - Left));
					}
					else if (PC)
					{
						PC->ClientNotify(FString::Printf(TEXT("Your %s's packs are full: it can't take the heartwood"), *Name));
						Body->NextDragCheckAt = T + 120.0;
					}
					break;
				}
			}
		}
	}

	// --- Hover kinds keep to the air (a Bramblewasp, a Hiveback, a Glimmerfinch flock) ---
	if (WRForestCrImpl::Hovers(K) && Self->IsAlive() && !Self->IsAsleep() && !Body->ClingTo.IsValid() && !Self->GetRider())
	{
		UCharacterMovementComponent* Move = Self->GetCharacterMovement();
		if (Move->MovementMode == MOVE_Walking && !Self->IsUnconscious() && !Self->HasTag(WRTags::Status_Staggered)) Move->SetMovementMode(MOVE_Flying);
	}

	SyncFlags(Self, Body);
	(void)DeltaTime;
}

void FWRForestAI::SyncFlags(AWRCreature* Self, UWRForestBody* Body)
{
	const double T = WRForestCrImpl::Now(Self);
	FName Row;
	const int32 Phase = RowPhaseOf(Self, &Row);
	const bool bWind = Phase == 1 || Phase == 2;
	Body->Set(WRForestFlag::Perched, Body->PerchStage >= 2);
	Body->Set(WRForestFlag::Climbing, Body->PerchStage == 2 || Body->PerchStage == 4);
	Body->Set(WRForestFlag::DropTell, Row == TEXT("canopy_drop") && bWind);
	Body->Set(WRForestFlag::Downed, T < Body->DownedUntil || !Body->bLanded);
	Body->Set(WRForestFlag::Holding, Body->Hold.IsValid());
	Body->Set(WRForestFlag::PathTame, !Body->PathTamer.IsEmpty());
	Body->Set(WRForestFlag::Clinging, Body->ClingTo.IsValid());
	Body->Set(WRForestFlag::Reared, ((Row == TEXT("hammer_down") || Row == TEXT("maul_hug") || Row == TEXT("body_slam")) && Phase >= 1 && Phase <= 3) || (Body->Hold.IsValid() && Body->KindId() == TEXT("mossmaul")));
	Body->Set(WRForestFlag::Lifted, T < Body->LiftUntil);
	Body->Set(WRForestFlag::Rooting, T < Body->RootingUntil);
	Body->Set(WRForestFlag::Raiding, Body->bRaider && !Body->bRaidDone);
	if (Self->IsAlive()) Body->Set(WRForestFlag::Larder, T < Body->LarderUntil);
}

// ---------------------------------------------------------------- owner-only marks

AWRForestMark::AWRForestMark()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRForestMark::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRForestMark, Kind);
}

AWRForestMark* AWRForestMark::Spawn(APlayerController* For, const FVector& At, FName InKind, float Seconds)
{
	if (!For || !For->GetWorld())
	{
		return nullptr;
	}
	// One mark per place: a fresh find renews it.
	for (TActorIterator<AWRForestMark> It(For->GetWorld()); It; ++It)
	{
		if (It->GetOwner() == For && It->Kind == InKind && FVector::Dist(It->GetActorLocation(), At) < 200.f)
		{
			It->SetLifeSpan(Seconds);
			return *It;
		}
	}
	FActorSpawnParameters Params;
	Params.Owner = For;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AWRForestMark* Mark = For->GetWorld()->SpawnActor<AWRForestMark>(AWRForestMark::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (Mark)
	{
		Mark->Kind = InKind;
		Mark->FinishSpawning(FTransform(At));
		Mark->SetLifeSpan(Seconds);
	}
	return Mark;
}

void AWRForestMark::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Only its owner's machine has it: a ring over the ground in the find's colour (a cache gold, a pit's rim brown,
	// the dig green).
	Fx = WRFx::Attach(RootComponent, FName(*FString::Printf(TEXT("forest.cr_mark_%s"), *Kind.ToString())), NAME_None, 1.f);
	WRSound::PlayUI(this, TEXT("discover"));
}

// ---------------------------------------------------------------- the forest's living world

bool UWRForestLife::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWRForestLife::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &UWRForestLife::Step), FMath::Max(0.5f, WRForestCrImpl::Tn(TEXT("forest.lifeStepSeconds"), 2.f)), true);
}

void UWRForestLife::NoteKill(UWorld* World, const FVector& At)
{
	UWRForestLife* Life = World ? World->GetSubsystem<UWRForestLife>() : nullptr;
	if (!Life || World->GetNetMode() == NM_Client)
	{
		return;
	}
	FKill K;
	K.At = At;
	K.When = World->GetTimeSeconds();
	Life->Kills.Add(K);
	if (Life->Kills.Num() > 16) Life->Kills.RemoveAt(0);
}

namespace WRForestCrImpl
{
	bool PlayerWithin(const UWorld* World, const FVector& At, float Range)
	{
		for (TActorIterator<AWRCharacter> It(World); It; ++It) if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), At) < Range) return true;
		return false;
	}

	bool InHours(float Hour, const TCHAR* From, const TCHAR* To, float F, float T)
	{
		const float A = Tn(From, F), B = Tn(To, T);
		return A <= B ? Hour >= A && Hour <= B : Hour >= A || Hour <= B;
	}
}

void UWRForestLife::Step()
{
	UWorld* World = GetWorld();
	if (!World || !WRBiome::IsLive(TEXT("forest")))
	{
		return;
	}
	const float Hour = AWRGameState::HourIn(World);
	BeatLarders();
	BeatDusk(Hour);
	BeatFords();
	BeatWasps(Hour);
	BeatFinches();
	BeatRootgrave(Hour);
}

void UWRForestLife::BeatLarders()
{
	// A Mossmaul near someone (so it's seen) goes for a Burlbeast; the kill becomes its larder (FWRForestAI::Died).
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (T < NextLarderAt)
	{
		return;
	}
	NextLarderAt = T + WRForestCrImpl::Roll(TEXT("forest.larderEvery"), 300.f, 600.f);
	const float Watch = WRForestCrImpl::Tn(TEXT("forest.watchRadius"), 30000.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* Maul = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!Maul || !WRForestCrImpl::Idle(Maul) || B->KindId() != TEXT("mossmaul") || B->LarderUntil > T || !WRForestCrImpl::PlayerWithin(World, Maul->GetActorLocation(), Watch)) continue;
		for (const TWeakObjectPtr<UWRForestBody>& O : UWRForestBody::Live)
		{
			AWRCreature* Ox = O.IsValid() ? Cast<AWRCreature>(O->GetOwner()) : nullptr;
			if (!Ox || !WRForestCrImpl::Wild(Ox) || O->KindId() != TEXT("burlbeast") || FVector::Dist(Ox->GetActorLocation(), Maul->GetActorLocation()) > WRForestCrImpl::Tn(TEXT("forest.larderReach"), 6000.f)) continue;
			Maul->HuntTarget = Ox;
			Maul->SetAggroTarget(Ox);
			UE_LOG(LogWildreach, Log, TEXT("FOREST %s goes for %s (a larder)"), *Maul->GetName(), *Ox->GetName());
			return;
		}
	}
}

void UWRForestLife::BeatDusk(float Hour)
{
	// At dusk the Thornlynx go up the trunks over the elk trails and wait for the Dappledoe; the Dappledoe bed down in
	// rings in the meadows for the night.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (!WRForestCrImpl::InHours(Hour, TEXT("forest.duskFrom"), TEXT("forest.duskTo"), 17.5f, 20.f))
	{
		return;
	}
	const float Watch = WRForestCrImpl::Tn(TEXT("forest.watchRadius"), 30000.f);
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || !WRForestCrImpl::Idle(C) || !WRForest::Contains(C->GetActorLocation()) || T < B->BeatUntil) continue;
		const FName K = B->KindId();
		if (K == TEXT("thornlynx") && B->PerchStage == 0 && WRForestCrImpl::PlayerWithin(World, C->GetActorLocation(), Watch))
		{
			B->BeatUntil = T + 600.0;
			FVector Perch, Trunk;
			float TrunkR = 200.f;
			if (WRForestCrImpl::PickPerch(C, C->GetActorLocation(), WRForestCrImpl::Tn(TEXT("thornlynx.duskSearch"), 6000.f), true, Perch, Trunk, TrunkR))
			{
				B->PerchAt = Perch;
				B->TrunkAt = Trunk;
				B->TrunkRadius = TrunkR;
				B->bEscape = false;
				B->PerchStage = 1;
				B->PerchSince = 0.0;
				C->SetHome(Trunk);
				UE_LOG(LogWildreach, Log, TEXT("FOREST %s goes up a trunk over the elk trail for the dusk"), *C->GetName());
			}
		}
		else if (K == TEXT("dappledoe") && C->Spawner.IsValid())
		{
			// Each group in a ring, every doe facing out (a sneak-tame window while they sleep).
			B->BeatUntil = T + 900.0;
			TArray<AWRCreature*> Group;
			FVector Mid = FVector::ZeroVector;
			for (const TWeakObjectPtr<UWRForestBody>& O : UWRForestBody::Live)
			{
				AWRCreature* Doe = O.IsValid() ? Cast<AWRCreature>(O->GetOwner()) : nullptr;
				if (Doe && WRForestCrImpl::Idle(Doe) && Doe->Spawner == C->Spawner) { Group.Add(Doe); Mid += Doe->GetActorLocation(); }
			}
			if (Group.Num() < 2) continue;
			Mid /= Group.Num();
			const float Ring = WRForestCrImpl::Tn(TEXT("dappledoe.ringRadius"), 260.f);
			for (int32 i = 0; i < Group.Num(); ++i)
			{
				Group[i]->SetHome(Mid + FRotator(0.f, 360.f * i / Group.Num(), 0.f).Vector() * Ring);
				if (UWRForestBody* GB = UWRForestBody::Get(Group[i])) GB->BeatUntil = T + 900.0;
			}
		}
	}
}

void UWRForestLife::BeatFords()
{
	// The Rootcoils lie across the fords: the crossing's log that isn't one.
	if (!bFordsBuilt)
	{
		bFordsBuilt = true;
		for (const FWRForestData::FSite& S : FWRForestData::Get().Kit) if (S.Kind == TEXT("ford")) Fords.Add(FVector(S.At.X, S.At.Y, S.Yaw));
	}
	for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
	{
		AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
		if (!C || !WRForestCrImpl::Idle(C) || B->KindId() != TEXT("rootcoil") || !B->RestAt.IsZero()) continue;
		const float Reach = WRForestCrImpl::Tn(TEXT("rootcoil.fordReach"), 6000.f);
		for (const FVector& F : Fords)
		{
			const FVector At(F.X, F.Y, C->GetActorLocation().Z);
			if (FVector::Dist2D(At, C->GetActorLocation()) > Reach) continue;
			bool bTaken = false;
			for (const TWeakObjectPtr<UWRForestBody>& O : UWRForestBody::Live) bTaken |= O.IsValid() && O != B && FVector::Dist2D(O->RestAt, At) < 500.f;
			if (bTaken) continue;
			B->RestAt = At;
			B->RestYaw = F.Z + 90.f;   // across the crossing
			C->SetHome(At);
			break;
		}
	}
}

void UWRForestLife::BeatWasps(float Hour)
{
	// At midday a Bramblewasp scout marks a beehive tree; an hour later a swarm raids it and the Hivebacks heat-ball them.
	UWorld* World = GetWorld();
	const int32 Day = WRForestCrImpl::GameDay(World);
	const double Hours = WRForestCrImpl::GameHours(World);
	if (RaidAtHours > 0.0 && Hours >= RaidAtHours)
	{
		RaidAtHours = -1.0;
		WRForest::WaspRaid(World, RaidNest, RaidHive, FMath::RandRange(int32(WRForestCrImpl::Tn(TEXT("bramblewasp.raidMin"), 6.f)), int32(WRForestCrImpl::Tn(TEXT("bramblewasp.raidMax"), 10.f))));
		return;
	}
	if (ScoutDay == Day || !WRForestCrImpl::InHours(Hour, TEXT("forest.noonFrom"), TEXT("forest.noonTo"), 11.5f, 13.5f))
	{
		return;
	}
	const UWRForestHazards* Hz = UWRForestHazards::Get(World);
	if (!Hz)
	{
		return;
	}
	TArray<FVector> Hives;
	WRForestCrImpl::Hives(World, Hives);
	const float Watch = WRForestCrImpl::Tn(TEXT("forest.watchRadius"), 30000.f);
	for (const FVector& Hive : Hives)
	{
		if (!WRForestCrImpl::PlayerWithin(World, Hive, Watch)) continue;
		for (const TWeakObjectPtr<AWRWaspNest>& N : Hz->Nests)
		{
			const AWRWaspNest* Nest = N.Get();
			if (!Nest || FVector::Dist2D(Nest->GetActorLocation(), Hive) > WRForestCrImpl::Tn(TEXT("bramblewasp.raidRange"), 8000.f)) continue;
			ScoutDay = Day;
			RaidNest = Nest->GetActorLocation();
			RaidHive = Hive;
			RaidAtHours = Hours + WRForestCrImpl::Tn(TEXT("bramblewasp.raidAfterHours"), 1.f);
			// The scout's mark on the hive tree: a pale puff and a hum over it.
			WRFx::Broadcast(World, TEXT("forest.cr_scout"), Hive + FVector(0.f, 0.f, 400.f), FRotator::ZeroRotator, 1.5f, TEXT("forest.scout_mark"));
			UE_LOG(LogWildreach, Log, TEXT("FOREST a scout marks the hive at %s: a raid in %.1f h"), *Hive.ToCompactString(), WRForestCrImpl::Tn(TEXT("bramblewasp.raidAfterHours"), 1.f));
			return;
		}
	}
}

void UWRForestLife::BeatFinches()
{
	// Glimmerfinch flocks lift over any fresh carcass or ripe wild patch: a sign seen from 300 m.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (T < NextFinchAt)
	{
		return;
	}
	// Clear days (DC 2.9 "Glimmerfinch flocks fly"): the beat comes twice as often and idle flocks take to the air on
	// their own as well. The Bloom's warning has every flock ringing the Hearts (TickWild), so nothing is handed out then.
	const bool bClearDay = UWRWeatherSubsystem::ActiveKind(World, TEXT("forest")) == nullptr && !AWRGameState::IsNightIn(World);
	NextFinchAt = T + WRForestCrImpl::Tn(TEXT("forest.finchEvery"), 20.f) * (bClearDay ? WRForestCrImpl::Tn(TEXT("glimmerfinch.clearEveryMult"), 0.5f) : 1.f);
	if (WRForest::BloomWarning(World) >= 0.f)
	{
		return;
	}
	if (bClearDay)
	{
		int32 Flights = FMath::RoundToInt(WRForestCrImpl::Tn(TEXT("glimmerfinch.clearFlights"), 2.f));
		const float Near = WRForestCrImpl::Tn(TEXT("forest.watchRadius"), 30000.f) * 0.5f;
		for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
		{
			if (Flights <= 0) break;
			AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
			if (!C || !WRForestCrImpl::Wild(C) || !WRForestCrImpl::Idle(C) || B->KindId() != TEXT("glimmerfinch") || B->LiftUntil > T || C->IsAsleep()) continue;
			if (!WRForest::Contains(C->GetActorLocation()) || !WRForestCrImpl::PlayerWithin(World, C->GetActorLocation(), Near) || FMath::FRand() > 0.5f) continue;
			// Up over its own patch of meadow, wheeling there a while: a glint over the grass seen from far off.
			B->Spot = C->GetHome();
			B->LiftUntil = T + WRForestCrImpl::Tn(TEXT("glimmerfinch.clearLiftSeconds"), 45.f);
			--Flights;
		}
	}
	Kills.RemoveAll([T](const FKill& K) { return T - K.When > WRForestCrImpl::Tn(TEXT("forest.killSignSeconds"), 300.f); });
	TArray<FVector> Signs;
	for (const FKill& K : Kills) Signs.Add(K.At);
	static const TSet<FName> Ripe = { TEXT("meadow_oats"), TEXT("idunn_apple"), TEXT("berries") };
	const float Watch = WRForestCrImpl::Tn(TEXT("forest.watchRadius"), 30000.f);
	for (TActorIterator<AWRResourceNode> It(World); It && Signs.Num() < 12; ++It)
	{
		if (!It->IsDepleted() && Ripe.Contains(It->ItemId) && WRForest::Contains(It->GetActorLocation()) && WRForestCrImpl::PlayerWithin(World, It->GetActorLocation(), Watch * 0.5f)) Signs.Add(It->GetActorLocation());
	}
	const float See = WRForestCrImpl::Tn(TEXT("glimmerfinch.signRadius"), 30000.f);
	for (const FVector& Sign : Signs)
	{
		bool bHas = false;
		for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live) bHas |= B.IsValid() && B->LiftUntil > T && FVector::Dist2D(B->Spot, Sign) < 1500.f;
		if (bHas) continue;
		UWRForestBody* Best = nullptr;
		float BestD = See;
		for (const TWeakObjectPtr<UWRForestBody>& B : UWRForestBody::Live)
		{
			AWRCreature* C = B.IsValid() ? Cast<AWRCreature>(B->GetOwner()) : nullptr;
			if (!C || !WRForestCrImpl::Idle(C) || B->KindId() != TEXT("glimmerfinch") || B->LiftUntil > T || C->IsAsleep()) continue;
			const float D = FVector::Dist2D(C->GetActorLocation(), Sign);
			if (D < BestD) { BestD = D; Best = B.Get(); }
		}
		if (Best)
		{
			Best->Spot = Sign;
			Best->LiftUntil = T + WRForestCrImpl::Tn(TEXT("glimmerfinch.liftSeconds"), 120.f);
		}
	}
}

void UWRForestLife::BeatRootgrave(float Hour)
{
	// At Rootgrave's mouth at dusk Thornlynx and Gravehounds fight. Dormant until the Graveyard brings the Gravehounds.
	UWorld* World = GetWorld();
	const double T = World->GetTimeSeconds();
	if (T < NextRootgraveAt || !FWRCreatureRegistry::Get().Find(TEXT("gravehound")) || !WRForestCrImpl::InHours(Hour, TEXT("forest.duskFrom"), TEXT("forest.duskTo"), 17.5f, 20.f))
	{
		return;
	}
	NextRootgraveAt = T + 120.0;
	const FWRForestData::FSite* Mouth = FWRForestData::Get().FindSite(TEXT("rootgrave_fort"));
	if (!Mouth)
	{
		return;
	}
	const float Reach = WRForestCrImpl::Tn(TEXT("forest.rootgraveReach"), 5000.f);
	AWRCreature* Lynx = nullptr;
	AWRCreature* Hound = nullptr;
	for (TActorIterator<AWRCreature> It(World); It; ++It)
	{
		if (!WRForestCrImpl::Idle(*It) || FVector::Dist2D(It->GetActorLocation(), Mouth->At) > Reach) continue;
		if (!Lynx && WRForestCrImpl::KindOf(*It) == TEXT("thornlynx")) Lynx = *It;
		if (!Hound && It->CreatureId == TEXT("gravehound")) Hound = *It;
	}
	if (Lynx && Hound)
	{
		Lynx->HuntTarget = Hound;
		Lynx->SetAggroTarget(Hound);
		Hound->HuntTarget = Lynx;
		Hound->SetAggroTarget(Lynx);
		UE_LOG(LogWildreach, Log, TEXT("FOREST at Rootgrave's mouth %s and %s fight"), *Lynx->GetName(), *Hound->GetName());
	}
}
