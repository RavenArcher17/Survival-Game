#include "WRHeroAnimTools.h"
#include "WRAnimNotifies.h"
#include "Wildreach.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "AnimationBlueprintLibrary.h"
#endif

TArray<FString> UWRHeroAnimTools::ImportProperties(UObject* Target, const TArray<FString>& Lines, bool bClearArrays)
{
	TArray<FString> Failed;
#if WITH_EDITOR
	if (!Target)
	{
		return Lines;
	}
	Target->Modify();
	TSet<FProperty*> Cleared;
	for (const FString& Line : Lines)
	{
		FString Name, Value;
		if (!Line.Split(TEXT("="), &Name, &Value))
		{
			Failed.Add(Line);
			continue;
		}
		int32 Index = INDEX_NONE;
		int32 Open = INDEX_NONE;
		if (Name.FindChar(TEXT('('), Open) && Name.EndsWith(TEXT(")")))
		{
			Index = FCString::Atoi(*Name.Mid(Open + 1, Name.Len() - Open - 2));
			Name = Name.Left(Open);
		}
		FProperty* Property = FindFProperty<FProperty>(Target->GetClass(), FName(*Name));
		if (!Property)
		{
			Failed.Add(Line);
			continue;
		}
		void* Container = Property->ContainerPtrToValuePtr<void>(Target);
		const TCHAR* Result = nullptr;
		if (Index != INDEX_NONE)
		{
			if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(Array, Container);
				if (bClearArrays && !Cleared.Contains(Property))
				{
					Cleared.Add(Property);
					Helper.EmptyValues();
				}
				if (Helper.Num() <= Index)
				{
					Helper.AddValues(Index + 1 - Helper.Num());
				}
				Result = Array->Inner->ImportText_Direct(*Value, Helper.GetRawPtr(Index), Target, PPF_None);
			}
			else if (Index < Property->ArrayDim)
			{
				Result = Property->ImportText_Direct(*Value, Property->ContainerPtrToValuePtr<void>(Target, Index), Target, PPF_None);
			}
		}
		else
		{
			Result = Property->ImportText_Direct(*Value, Container, Target, PPF_None);
		}
		if (!Result)
		{
			Failed.Add(Line);
		}
	}
	Target->PostEditChange();
	Target->MarkPackageDirty();
#else
	Failed = Lines;
#endif
	return Failed;
}

int32 UWRHeroAnimTools::ClearWindows(UAnimSequenceBase* Clip)
{
	int32 Removed = 0;
#if WITH_EDITOR
	if (!Clip)
	{
		return 0;
	}
	Clip->Modify();
	Removed = Clip->Notifies.RemoveAll([](const FAnimNotifyEvent& Event) { return Event.NotifyStateClass && Event.NotifyStateClass->IsA<UWRANS_Window>(); });
	Clip->RefreshCacheData();
	Clip->MarkPackageDirty();
#endif
	return Removed;
}

bool UWRHeroAnimTools::AddWindow(UAnimSequenceBase* Clip, FName Tag, float Start, float End)
{
#if WITH_EDITOR
	const FGameplayTag Window = FGameplayTag::RequestGameplayTag(Tag, false);
	if (!Clip || !Window.IsValid() || End <= Start)
	{
		UE_LOG(LogWildreach, Warning, TEXT("AddWindow: %s %s %.2f-%.2f not added"), *GetNameSafe(Clip), *Tag.ToString(), Start, End);
		return false;
	}
	static const FName Track(TEXT("Windows"));
	if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(Clip, Track))
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Clip, Track, FLinearColor(0.9f, 0.5f, 0.1f));
	}
	UAnimNotifyState* State = UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(Clip, Track, Start, End - Start, UWRANS_Window::StaticClass());
	if (UWRANS_Window* Notify = Cast<UWRANS_Window>(State))
	{
		Notify->Window = Window;
		Clip->MarkPackageDirty();
		return true;
	}
#endif
	return false;
}
