#pragma once
// Helpers shared by the portal files (WRPortals.cpp, WRPortalFold.cpp, WRPortalThings.cpp, WRPortalBosses.cpp,
// WRUIPortals.cpp). Inline, in one namespace, so unity builds that join the files see them once.
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "WRInventory.h"

namespace WRPortalsImpl
{
	inline TSharedPtr<FJsonObject> PChild(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		const TSharedPtr<FJsonObject>* C = nullptr;
		return O.IsValid() && O->TryGetObjectField(Key, C) ? *C : nullptr;
	}
	inline const TArray<TSharedPtr<FJsonValue>>* PArr(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(Key, A) ? A : nullptr;
	}
	inline float PNum(const TSharedPtr<FJsonObject>& O, const FString& Key, float Fallback)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback;
	}
	inline FString PStr(const TSharedPtr<FJsonObject>& O, const FString& Key, const FString& Fallback = FString())
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(Key, S) ? S : Fallback;
	}
	inline bool PBool(const TSharedPtr<FJsonObject>& O, const FString& Key, bool Fallback = false)
	{
		bool B = Fallback;
		if (O.IsValid()) O->TryGetBoolField(Key, B);
		return B;
	}
	/** [a, b] from a json array (or the fallback). */
	inline FVector2D PRange(const TSharedPtr<FJsonObject>& O, const FString& Key, FVector2D Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = PArr(O, Key);
		return A && A->Num() >= 2 ? FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) : Fallback;
	}
	/** Element i of a number array value. */
	inline float PAt(const TSharedPtr<FJsonValue>& V, int32 i, float Fallback = 0.f)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return V.IsValid() && V->TryGetArray(A) && A->IsValidIndex(i) && (*A)[i]->Type == EJson::Number ? float((*A)[i]->AsNumber()) : Fallback;
	}
	inline FString PAtStr(const TSharedPtr<FJsonValue>& V, int32 i)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		FString S;
		if (V.IsValid() && V->TryGetArray(A) && A->IsValidIndex(i)) (*A)[i]->TryGetString(S);
		return S;
	}
	inline int32 PLen(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return V.IsValid() && V->TryGetArray(A) ? A->Num() : 0;
	}
	inline FColor PColour(const TSharedPtr<FJsonObject>& O, const FString& Key, FColor Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = PArr(O, Key);
		return A && A->Num() >= 3 ? FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber())) : Fallback;
	}
	inline FLinearColor PLin(const FColor& C) { return FLinearColor(C); }
	/** A weighted pick from [[id, weight], ...]. */
	inline FName PPick(const TArray<TSharedPtr<FJsonValue>>* List)
	{
		if (!List || List->Num() == 0) return NAME_None;
		float Total = 0.f;
		for (const TSharedPtr<FJsonValue>& V : *List) Total += PAt(V, 1, 1.f);
		float R = FMath::FRand() * Total;
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			R -= PAt(V, 1, 1.f);
			if (R <= 0.f) return FName(*PAtStr(V, 0));
		}
		return FName(*PAtStr(List->Last(), 0));
	}
	/** The server's clock on any machine (replicated times are on it). */
	inline float PServerNow(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? float(GS->GetServerWorldTimeSeconds()) : World ? float(World->GetTimeSeconds()) : 0.f;
	}
}

class AActor;
class USceneComponent;
class UPointLightComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Drawing helpers the portal files share (defined in WRPortalFold.cpp). */
namespace WRFoldLook
{
	WILDREACH_API UPointLightComponent* Light(AActor* Owner, USceneComponent* Parent, const FVector& At, const FLinearColor& Colour, float Intensity, float Radius);
	WILDREACH_API UBoxComponent* Block(AActor* Owner, USceneComponent* Parent, const FVector& Centre, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator);
	WILDREACH_API UMaterialInstanceDynamic* RockMat(UObject* Outer, const FLinearColor& Tint);
	/** A rock of the SimpleRocks set fitted to Size (cm), its foot at Base, sunk Sink cm. */
	WILDREACH_API UStaticMeshComponent* Rock(AActor* Owner, USceneComponent* Parent, int32 Which, const FVector& Size, const FVector& Base, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, float Sink = 10.f);
	/** An emissive engine shape (its own component, so it can move or change): Cube, Cylinder, Cone, Sphere. */
	WILDREACH_API UStaticMeshComponent* Shape(AActor* Owner, USceneComponent* Parent, const TCHAR* Kind, const FVector& Size, const FVector& Centre, const FRotator& Rot, UMaterialInterface* Mat);
	WILDREACH_API UMaterialInstanceDynamic* Glow(UObject* Outer, const FLinearColor& Colour, float Strength);
	/** A kit's colour (portal_rooms/<kit>.json look.<field>), sRGB -> linear. */
	WILDREACH_API FLinearColor KitColour(FName Biome, const TCHAR* Field, const FColor& Fallback);
}

class AWRFold;
/** The fold's own hazards (lava runnels, black-water pits), server (WRPortalThings.cpp). */
namespace WRFoldHazards
{
	WILDREACH_API void Step(AWRFold* Fold, float Dt);
	/** The server's clock as every machine reads it (replicated trap times are on it). */
	WILDREACH_API double Now(const UObject* WorldContext);
}

class AWRPlayerState;
class AWRCreature;
class AWRFold;
/** The portal system's shared rules (defined in WRPortals.cpp). */
namespace WRPortalsSys
{
	/** Item counts from a reward table ("@materials" the land's, "@essence", "@gem", "@rune"), times PsQuantity. */
	void PsResolve(const TSharedPtr<FJsonObject>& Table, FName Biome, float PsQuantity, TMap<FName, int32>& Out);
	/** A chance table: each that comes up once ("@fineGear" / "@masterwork" go to OutSpecial). */
	void PsChances(const TSharedPtr<FJsonObject>& Table, FName Biome, TMap<FName, int32>& Out, TArray<FName>& OutSpecial);
	/** Gives the items; what doesn't fit waits in a bag at their feet, only theirs. Returns the line. */
	FString PsGive(AWRPlayerState* PS, const TMap<FName, int32>& Items, const TArray<FWRItemStack>& Instances = TArray<FWRItemStack>());
	FWRItemStack PsGearAt(FName Id, EWRQuality Q);
	/** A creature with a fold's rank scaling and mark, set before it begins play. */
	AWRCreature* PsSpawnScaled(UWorld* World, FName Id, const FVector& At, float Yaw, float HealthMult, float DamageMult, float StanceMult, FColor Mark);
	void PsScaleFor(int32 Rank, FName Tier, float& H, float& D, float& S);
}
