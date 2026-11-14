#include "WRAnimNotifies.h"
#include "WRTuning.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Pawn.h"

void UWRANS_Window::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	Apply(MeshComp, 1);
}

void UWRANS_Window::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Apply(MeshComp, -1);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWRANS_Window::GetNotifyName_Implementation() const
{
	return Window.IsValid() ? Window.ToString() : TEXT("Window");
}

void UWRANS_Window::Apply(USkeletalMeshComponent* MeshComp, int32 Delta) const
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const IAbilitySystemInterface* Holder = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* ASC = Holder ? Holder->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !Window.IsValid())
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Owner->HasAuthority())
	{
		const int32 Count = FMath::Max(0, ASC->GetTagCount(Window) + Delta);
		ASC->SetLooseGameplayTagCount(Window, Count, EGameplayTagReplicationState::TagOnly);
	}
	else if (Pawn && Pawn->IsLocallyControlled())
	{
		// The owner's own copy (not replicated): the server's arrives a round trip later and replaces it.
		if (Delta > 0) ASC->AddLooseGameplayTag(Window);
		else if (ASC->HasMatchingGameplayTag(Window)) ASC->RemoveLooseGameplayTag(Window);
	}
}

bool FWRAnimWindows::Find(FName Clip, FName Window, float& OutStart, float& OutEnd)
{
	static TSharedPtr<FJsonObject> Data = FWRTuning::LoadDataFile(TEXT("anim_windows.json"));  // read once
	const TSharedPtr<FJsonObject>* Row = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Span = nullptr;
	double Length = 0.0;
	if (!Data.IsValid() || !Data->TryGetObjectField(Clip.ToString(), Row) || !(*Row)->TryGetNumberField(TEXT("length"), Length)
		|| Length <= 0.0 || !(*Row)->TryGetArrayField(Window.ToString(), Span) || Span->Num() != 2)
	{
		return false;
	}
	OutStart = float((*Span)[0]->AsNumber() / Length);
	OutEnd = float((*Span)[1]->AsNumber() / Length);
	return OutEnd > OutStart;
}
