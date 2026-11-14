#pragma once
// Helpers the deeds files share (WRDeeds.cpp, WRDeedsAchieve.cpp, WRRecords.cpp, WRWarHall.cpp, WREndgameLook.cpp,
// WRUIEndgame.cpp). Inline in one namespace so unity builds that join the files see them once.
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "WRPlayerState.h"

class AWRPlayerController;

namespace WRDeedsImpl
{
	inline float DNum(const TSharedPtr<FJsonObject>& O, const FString& Key, float Fallback) { double V = 0.0; return O.IsValid() && O->TryGetNumberField(Key, V) ? float(V) : Fallback; }
	inline FString DStr(const TSharedPtr<FJsonObject>& O, const FString& Key, const FString& Fallback = FString()) { FString S; return O.IsValid() && O->TryGetStringField(Key, S) ? S : Fallback; }
	inline bool DBool(const TSharedPtr<FJsonObject>& O, const FString& Key, bool Fallback = false) { bool B = Fallback; if (O.IsValid()) O->TryGetBoolField(Key, B); return B; }
	inline TSharedPtr<FJsonObject> DChild(const TSharedPtr<FJsonObject>& O, const FString& Key) { const TSharedPtr<FJsonObject>* C = nullptr; return O.IsValid() && O->TryGetObjectField(Key, C) ? *C : nullptr; }
	inline const TArray<TSharedPtr<FJsonValue>>* DArr(const TSharedPtr<FJsonObject>& O, const FString& Key) { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; return O.IsValid() && O->TryGetArrayField(Key, A) ? A : nullptr; }
	inline float DAt(const TSharedPtr<FJsonValue>& V, int32 i, float Fallback = 0.f)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return V.IsValid() && V->TryGetArray(A) && A->IsValidIndex(i) && (*A)[i]->Type == EJson::Number ? float((*A)[i]->AsNumber()) : Fallback;
	}
	inline FColor DColour(const TSharedPtr<FJsonObject>& O, const FString& Key, FColor Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = DArr(O, Key);
		return A && A->Num() >= 3 ? FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber())) : Fallback;
	}
	inline AWRPlayerState* DStateOf(const AActor* Who)
	{
		if (const AWRPlayerState* PS = Cast<AWRPlayerState>(Who)) return const_cast<AWRPlayerState*>(PS);
		if (const APawn* P = Cast<APawn>(Who)) return Cast<AWRPlayerState>(P->GetPlayerState());
		if (const AController* C = Cast<AController>(Who)) return Cast<AWRPlayerState>(C->PlayerState);
		return nullptr;
	}
	inline AWRPlayerState* DStateNamed(const UWorld* World, const FString& Name)
	{
		if (!World || !World->GetGameState()) return nullptr;
		for (APlayerState* P : World->GetGameState()->PlayerArray) if (P && P->GetPlayerName() == Name) return Cast<AWRPlayerState>(P);
		return nullptr;
	}
	/** The server's clock on any machine. */
	inline double DNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : W ? W->GetTimeSeconds() : 0.0;
	}
	inline FLinearColor DLin(const FColor& C) { return FLinearColor(C); }
}
