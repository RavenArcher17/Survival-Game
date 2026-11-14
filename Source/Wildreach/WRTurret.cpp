// Powered turrets: targeting, the tell, firing, ammo (cooks/build-building.md 2.6; WRTurret.h). Their model and the aim
// line every machine draws live in WRMachineLook (Turret, EWRMotion::TurretYaw / TurretPitch).

#include "WRTurret.h"
#include "WRBuilding.h"
#include "WRCharacterBase.h"
#include "WRCombatLibrary.h"
#include "WRCreature.h"
#include "WRFx.h"
#include "WRGameplayTags.h"
#include "WRInventory.h"
#include "WRLifeSkills.h"
#include "WRMachine.h"
#include "WRProjectile.h"
#include "WRSocial.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

namespace WRTurretDetail
{
	/** machines.json "turrets": the ammo rows and the turret numbers. */
	struct FTable
	{
		TArray<FWRTurretAmmo> Ammo;
		TSharedPtr<FJsonObject> Rules;
		static const FTable& Get()
		{
			static FTable T;
			return T;
		}
		float Rule(const TCHAR* Key, float Fallback) const
		{
			double V = Fallback;
			if (Rules.IsValid()) Rules->TryGetNumberField(Key, V);
			return static_cast<float>(V);
		}
	private:
		FTable()
		{
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("machines.json"));
			const TSharedPtr<FJsonObject>* Turrets = nullptr;
			if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("turrets"), Turrets)) return;
			Rules = *Turrets;
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (!(*Turrets)->TryGetArrayField(TEXT("ammo"), List)) return;
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FWRTurretAmmo A;
				FString S;
				A.Item = FName(*O->GetStringField(TEXT("item")));
				if (!FWRItemRegistry::Get().Find(A.Item)) continue;   // a path's essence not in the game yet: waits for it
				A.Turret = FName(*O->GetStringField(TEXT("turret")));
				if (O->TryGetStringField(TEXT("element"), S)) A.Element = FName(*S);
				A.Shape = O->TryGetStringField(TEXT("shape"), S) ? FName(*S) : FName(TEXT("bolt"));
				if (O->TryGetStringField(TEXT("status"), S)) A.Status = FName(*S);
				O->TryGetNumberField(TEXT("damage"), A.Damage);
				O->TryGetNumberField(TEXT("speed"), A.Speed);
				O->TryGetNumberField(TEXT("burn"), A.Burn);
				O->TryGetNumberField(TEXT("frost"), A.Frost);
				O->TryGetNumberField(TEXT("buildup"), A.Buildup);
				O->TryGetNumberField(TEXT("range"), A.Range);
				O->TryGetNumberField(TEXT("cone"), A.Cone);
				O->TryGetNumberField(TEXT("coneAngle"), A.ConeAngle);
				O->TryGetNumberField(TEXT("shots"), A.Shots);
				const TArray<TSharedPtr<FJsonValue>>* Col = nullptr;
				if (O->TryGetArrayField(TEXT("color"), Col) && Col->Num() == 3) A.Color = FLinearColor((*Col)[0]->AsNumber(), (*Col)[1]->AsNumber(), (*Col)[2]->AsNumber());
				Ammo.Add(A);
			}
			UE_LOG(LogWildreach, Log, TEXT("Turrets: %d ammo rows"), Ammo.Num());
		}
	};

	float ServerNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	}

	float Slew(float From, float To, float MaxStep)
	{
		const float Diff = FRotator::NormalizeAxis(To - From);
		return FRotator::NormalizeAxis(From + FMath::Clamp(Diff, -MaxStep, MaxStep));
	}

	/** May this turret shoot at this one? Hostile wild creatures; on a PvP server, players not of its builder's clan
	 *  inside the claim it stands in. Never its builder's tames, clan, or a creature minding its own business. */
	bool Eligible(const AWRBuildPiece* Piece, const AWRCharacterBase* C)
	{
		if (!C || !C->IsAlive()) return false;
		if (const AWRCreature* Cr = Cast<AWRCreature>(C))
		{
			if (Cr->IsTamed()) return false;
			const FWRCreatureDef* Def = Cr->GetDef();
			return Cr->bRaider || (Def && Def->Behavior == TEXT("aggressive")) || Cr->GetAggroTarget() != nullptr;
		}
		const APlayerState* PS = C->GetPlayerState();
		if (!PS || WRTune(TEXT("server.pvp"), 1.f) <= 0.f) return false;
		if (FWRSocialRules::CanShare(Piece->OwnerName, PS->GetPlayerName())) return false;
		if (FWRSocialRules::IsInSafeZone(Piece->GetWorld(), C->GetActorLocation())) return false;
		const AWRClaimTotem* Claim = AWRClaimTotem::FindAt(Piece->GetWorld(), Piece->GetActorLocation());
		return Claim && FVector::Dist2D(Claim->GetActorLocation(), C->GetActorLocation()) <= Claim->Radius;
	}
}

FVector FWRTurretGeom::Muzzle(const AWRBuildPiece* Piece, float Yaw, float Pitch)
{
	const FVector Local = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(0.f, 0.f, TrunnionZ) + FRotator(Pitch, 0.f, 0.f).RotateVector(FVector(MuzzleFwd, 0.f, MuzzleUp)));
	return Piece->GetActorTransform().TransformPositionNoScale(Local);
}

const FWRTurretAmmo* UWRTurretSubsystem::AmmoFor(FName Turret, FName Item)
{
	return WRTurretDetail::FTable::Get().Ammo.FindByPredicate([Turret, Item](const FWRTurretAmmo& A) { return A.Turret == Turret && A.Item == Item; });
}

bool UWRTurretSubsystem::MayHit(const AWRBuildPiece* Turret, const AWRCharacterBase* Target)
{
	return Turret && WRTurretDetail::Eligible(Turret, Target);
}

float UWRTurretSubsystem::TellSeconds()
{
	return WRTurretDetail::FTable::Get().Rule(TEXT("tellSeconds"), 0.9f);
}

bool UWRTurretSubsystem::IsTurret(const FWRPieceDef& Def)
{
	const FWRMachineDef* M = FWRMachineDefs::Get().Find(Def.Id);
	return M && M->Role == TEXT("turret");
}

bool UWRTurretSubsystem::IsArmed(const AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	const FWRMachineDef* M = Def ? FWRMachineDefs::Get().Find(Def->Id) : nullptr;
	if (!M) return false;
	if (Piece->Mach.FuelLeft >= 1.f && AmmoFor(Def->Id, Piece->Mach.Recipe)) return true;
	for (int32 i = 0; i < M->In && Piece->Storage.IsValidIndex(i); ++i)
	{
		if (!Piece->Storage[i].IsEmpty() && AmmoFor(Def->Id, Piece->Storage[i].ItemId)) return true;
	}
	return false;
}

FString UWRTurretSubsystem::AmmoText(const AWRBuildPiece* Piece)
{
	const FWRPieceDef* Def = Piece ? Piece->GetDef() : nullptr;
	const FWRMachineDef* M = Def ? FWRMachineDefs::Get().Find(Def->Id) : nullptr;
	if (!M) return FString();
	int32 Shots = 0;
	FName What = Piece->Turret.Element;
	for (int32 i = 0; i < M->In && Piece->Storage.IsValidIndex(i); ++i)
	{
		const FWRItemStack& S = Piece->Storage[i];
		if (const FWRTurretAmmo* A = S.IsEmpty() ? nullptr : AmmoFor(Def->Id, S.ItemId)) { Shots += S.Count * A->Shots; if (What.IsNone()) What = A->Element; }
	}
	Shots += Piece->Turret.ShotsLeft;
	if (Shots <= 0) return Def->Id == TEXT("bolt_turret") ? TEXT("No bolts: put crossbow bolts in its box") : TEXT("Nothing loaded: put in ember or frost essence");
	return FString::Printf(TEXT("%d shot%s (%s)"), Shots, Shots == 1 ? TEXT("") : TEXT("s"), What.IsNone() ? TEXT("loaded") : *What.ToString());
}

TStatId UWRTurretSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWRTurretSubsystem, STATGROUP_Tickables);
}

const FWRTurretAmmo* UWRTurretSubsystem::Loaded(AWRBuildPiece* Piece, bool bConsume)
{
	const FWRPieceDef* Def = Piece->GetDef();
	const FWRMachineDef* M = Def ? FWRMachineDefs::Get().Find(Def->Id) : nullptr;
	if (!M) return nullptr;
	if (Piece->Mach.FuelLeft >= 1.f)
	{
		if (const FWRTurretAmmo* A = AmmoFor(Def->Id, Piece->Mach.Recipe)) return A;
	}
	for (int32 i = 0; i < M->In && Piece->Storage.IsValidIndex(i); ++i)
	{
		FWRItemStack& S = Piece->Storage[i];
		const FWRTurretAmmo* A = S.IsEmpty() ? nullptr : AmmoFor(Def->Id, S.ItemId);
		if (!A) continue;
		if (bConsume)
		{
			// Into the chamber: a bolt drops into the groove; an essence goes into the vessel for its shots.
			Piece->Mach.Recipe = S.ItemId;
			Piece->Mach.FuelLeft = A->Shots;
			if (--S.Count <= 0) S = FWRItemStack();
			Piece->OnRep_Farm();
		}
		return A;
	}
	return nullptr;
}

AWRCharacterBase* UWRTurretSubsystem::PickTarget(AWRBuildPiece* Piece, float Range) const
{
	using namespace WRTurretDetail;
	const FVector From = FWRTurretGeom::Muzzle(Piece, Piece->Turret.Yaw, Piece->Turret.Pitch);
	AWRCharacterBase* Best = nullptr;
	float BestD = Range;
	for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), From);
		if (D >= BestD || !Eligible(Piece, *It) || !CanSee(Piece, From, *It)) continue;
		Best = *It;
		BestD = D;
	}
	return Best;
}

bool UWRTurretSubsystem::CanSee(AWRBuildPiece* Piece, const FVector& From, const AWRCharacterBase* Target) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTurretSight), false, Piece);
	Q.AddIgnoredActor(Target);
	FHitResult H;
	return !GetWorld()->LineTraceSingleByChannel(H, From, Target->GetActorLocation() + FVector(0.f, 0.f, 30.f), ECC_Visibility, Q);
}

void UWRTurretSubsystem::Tick(float DeltaTime)
{
	using namespace WRTurretDetail;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || DeltaTime <= 0.f) return;
	ScanClock -= DeltaTime;
	if (ScanClock <= 0.f)
	{
		ScanClock = 1.f;
		Turrets.Reset();
		for (TActorIterator<AWRBuildPiece> It(World); It; ++It)
		{
			const FWRPieceDef* Def = It->GetDef();
			if (!It->bGhost && Def && IsTurret(*Def)) Turrets.Add(*It);
		}
	}
	const FTable& Tab = FTable::Get();
	const float Now = ServerNow(this);
	const float Tell = Tab.Rule(TEXT("tellSeconds"), 0.9f), Hold = Tab.Rule(TEXT("holdSeconds"), 0.3f);
	const float YawRate = Tab.Rule(TEXT("yawDegPerSec"), 120.f), PitchRate = Tab.Rule(TEXT("pitchDegPerSec"), 60.f);
	for (const TWeakObjectPtr<AWRBuildPiece>& W : Turrets)
	{
		AWRBuildPiece* P = W.Get();
		if (!P || P->IsActorBeingDestroyed()) continue;
		FBrain& B = Brains.FindOrAdd(P);
		FWRTurretRep& R = P->Turret;
		R.ShotsLeft = static_cast<uint8>(FMath::Clamp(FMath::FloorToInt(P->Mach.FuelLeft), 0, 255));
		const float Speed = P->Machine.Speed100 / 100.f;
		const FWRTurretAmmo* Ammo = Loaded(P, false);
		if (Ammo) R.Element = Ammo->Element;
		if (!P->Mach.bOn || Speed < 0.01f || !Ammo)
		{
			// Unpowered, switched off or empty: it holds where it was and says why on its screen (state NoPower / NoAmmo).
			R.bTracking = false;
			B.TellLeft = -1.f;
			B.Target = nullptr;
			continue;
		}
		// Spanning (winding the bow, pumping the vessel) runs at the network's speed: short of power it shoots slower.
		B.Reload = FMath::Max(0.f, B.Reload - DeltaTime * Speed);
		const float Range = Ammo->Range;
		B.LookClock -= DeltaTime;
		AWRCharacterBase* T = B.Target.Get();
		if (B.LookClock <= 0.f)
		{
			B.LookClock = Tab.Rule(TEXT("lookSeconds"), 0.2f);
			const FVector From = FWRTurretGeom::Muzzle(P, R.Yaw, R.Pitch);
			if (!T || !Eligible(P, T) || FVector::Dist(T->GetActorLocation(), From) > Range || !CanSee(P, From, T))
			{
				T = PickTarget(P, Range);
				B.Target = T;
			}
		}
		if (!T)
		{
			R.bTracking = false;
			B.TellLeft = -1.f;
			continue;
		}
		R.bTracking = true;
		// Where to point: the shot's lead on a moving target (a cone just at it), in the piece's own frame.
		const FVector Muzzle = FWRTurretGeom::Muzzle(P, R.Yaw, R.Pitch);
		const FVector Pivot = P->GetActorTransform().TransformPositionNoScale(FVector(0.f, 0.f, FWRTurretGeom::TrunnionZ));
		const FVector Aim = Ammo->Shape == TEXT("cone") ? T->GetActorLocation() : UWRCombatLibrary::LeadPoint(Muzzle, T->GetActorLocation() + FVector(0.f, 0.f, 20.f), T->GetVelocity(), Ammo->Speed);
		const FVector Local = P->GetActorTransform().InverseTransformVectorNoScale(Aim - Pivot);
		const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		const float WantPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Z, Local.Size2D())), Tab.Rule(TEXT("pitchMin"), -20.f), Tab.Rule(TEXT("pitchMax"), 45.f));
		const bool bTelling = B.TellLeft >= 0.f;
		// During the tell it follows at a quarter speed, then holds still for the last moment: you can step out of the line.
		const float Follow = !bTelling ? 1.f : B.TellLeft > Hold ? Tab.Rule(TEXT("tellFollow"), 0.25f) : 0.f;
		R.Yaw = Slew(R.Yaw, WantYaw, YawRate * Follow * DeltaTime);
		R.Pitch = FMath::Clamp(Slew(R.Pitch, WantPitch, PitchRate * Follow * DeltaTime), Tab.Rule(TEXT("pitchMin"), -20.f), Tab.Rule(TEXT("pitchMax"), 45.f));
		const FVector Dir = P->GetActorTransform().TransformVectorNoScale(FRotator(R.Pitch, R.Yaw, 0.f).Vector());
		const auto LineEnd = [&]()
		{
			const FVector From = FWRTurretGeom::Muzzle(P, R.Yaw, R.Pitch);
			const float Reach = Ammo->Shape == TEXT("cone") ? Ammo->Cone : Range;
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(WRTurretLine), false, P);
			return GetWorld()->LineTraceSingleByChannel(H, From, From + Dir * Reach, ECC_Visibility, Q) ? H.ImpactPoint : From + Dir * Reach;
		};
		if (bTelling)
		{
			B.TellLeft -= DeltaTime;
			R.AimAt = LineEnd();
			if (B.TellLeft <= 0.f)
			{
				B.TellLeft = -1.f;
				B.AimDir = Dir;
				Fire(P, B, *Ammo);
				B.Reload = Tab.Rule(TEXT("intervalSeconds"), 1.5f);
			}
			continue;
		}
		const bool bOnTarget = FMath::Abs(FRotator::NormalizeAxis(WantYaw - R.Yaw)) < 3.f && FMath::Abs(WantPitch - R.Pitch) < 3.f;
		if (B.Reload <= 0.f && bOnTarget)
		{
			// The tell: a red line down the shot's lane (every machine draws it from TellAt) and a ratchet click.
			B.TellLeft = Tell;
			R.TellAt = Now;
			R.AimAt = LineEnd();
		}
	}
	// Forget turrets that are gone.
	for (auto It = Brains.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()) It.RemoveCurrent();
	}
}

void UWRTurretSubsystem::Fire(AWRBuildPiece* Piece, FBrain& B, const FWRTurretAmmo& /*Peeked*/)
{
	using namespace WRTurretDetail;
	const FWRTurretAmmo* Ammo = Loaded(Piece, true);
	if (!Ammo) return;
	Piece->Mach.FuelLeft = FMath::Max(0.f, Piece->Mach.FuelLeft - 1.f);
	FWRTurretRep& R = Piece->Turret;
	R.ShotAt = ServerNow(this);
	R.ShotsLeft = static_cast<uint8>(FMath::Clamp(FMath::FloorToInt(Piece->Mach.FuelLeft), 0, 255));
	const FVector Muzzle = FWRTurretGeom::Muzzle(Piece, R.Yaw, R.Pitch);
	const FVector Dir = B.AimDir.GetSafeNormal();

	// Turret tech (Engineering 50): its builder's turrets hit harder (read while they're on, kept while they're away).
	static TMap<FString, float> TechOf;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Piece->OwnerName)
		{
			TechOf.Add(Piece->OwnerName, UWRLifeSkillsComponent::StatFor(*It, TEXT("turretDamage")));
			break;
		}
	}
	const float Mult = 1.f + TechOf.FindRef(Piece->OwnerName);

	FWRHit Hit;
	Hit.Instigator = Piece;
	Hit.Damage = Ammo->Damage * Mult;
	Hit.Origin = Muzzle;
	Hit.bProjectile = true;
	Hit.Impact = 2;
	Hit.Tier = EWRHitTier::Light;
	Hit.Element = Ammo->Element == TEXT("fire") ? WRTags::Element_Fire : Ammo->Element == TEXT("frost") ? WRTags::Element_Frost : WRTags::Element_Physical;
	Hit.BurnBuildup = Ammo->Burn;
	Hit.FrostBuildup = Ammo->Frost;
	if (!Ammo->Status.IsNone()) Hit.Buildups.Add(Ammo->Status, Ammo->Buildup);

	if (Ammo->Shape == TEXT("cone"))
	{
		// A spray of fire over Cone cm in a ConeAngle cone: whoever the turret may shoot at inside it takes the blow.
		const float Cos = FMath::Cos(FMath::DegreesToRadians(Ammo->ConeAngle * 0.5f));
		for (TActorIterator<AWRCharacterBase> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Muzzle;
			if (To.Size() > Ammo->Cone || FVector::DotProduct(To.GetSafeNormal(), Dir) < Cos || !Eligible(Piece, *It) || !CanSee(Piece, Muzzle, *It)) continue;
			FWRHit H = Hit;
			H.ImpactPoint = It->GetActorLocation();
			UWRCombatLibrary::ApplyHit(*It, H);
		}
		WRFx::Broadcast(Piece, TEXT("breath.fire"), Muzzle, Dir.Rotation(), Ammo->Cone / 1300.f, TEXT("turret.flame"), 0.45f);
		UE_LOG(LogWildreach, Log, TEXT("TURRET %s sprays %s"), *Piece->GetName(), *Ammo->Element.ToString());
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Piece;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Start = Muzzle + Dir * 30.f;
	AWRProjectile* Shot = GetWorld()->SpawnActor<AWRProjectile>(AWRProjectile::StaticClass(), Start, Dir.Rotation(), Params);
	if (!Shot) return;
	if (Ammo->Element == TEXT("bolt"))
	{
		// A real crossbow bolt, straight and flat: no drop, no homing.
		Hit.PhysType = 3;
		const FWRItemDef* Bolt = FWRItemRegistry::Get().Find(Ammo->Item);
		Shot->InitPhysical(Hit, Dir * Ammo->Speed, 0.f, Bolt ? Bolt->ProjectileMesh : FString(), Bolt ? Bolt->MeshAlong : FString(), TEXT("bolt"), NAME_None);
		WRSound::Broadcast(Piece, TEXT("turret.loose"));
	}
	else
	{
		// A path's bolt: the element's trail in flight, straight on (Init flies it along its facing; never homed).
		Shot->Init(Hit, Ammo->Speed, Ammo->Color);
		WRSound::Broadcast(Piece, Ammo->Element == TEXT("frost") ? TEXT("turret.frost") : TEXT("turret.loose"));
	}
	UE_LOG(LogWildreach, Log, TEXT("TURRET %s looses %s (%.0f damage)"), *Piece->GetName(), *Ammo->Element.ToString(), Hit.Damage);
}
