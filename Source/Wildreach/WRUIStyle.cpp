#include "WRUIStyle.h"
#include "WRTuning.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace WRUI
{
	static FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	const FLinearColor Ink = Hex(TEXT("ECE2CC"));
	const FLinearColor Ink2 = Hex(TEXT("C9BB9C"));
	const FLinearColor Ink3 = Hex(TEXT("978A71"));
	const FLinearColor InkDark = Hex(TEXT("2A1D12"));
	const FLinearColor Brass = Hex(TEXT("CFA35E"));
	const FLinearColor Brass2 = Hex(TEXT("8E6B38"));
	const FLinearColor Ember = Hex(TEXT("EC7A34"));
	const FLinearColor Frost = Hex(TEXT("7FC0E8"));
	const FLinearColor Good = Hex(TEXT("9CCB74"));
	const FLinearColor Bad = Hex(TEXT("EC6A48"));
	const FLinearColor Warn = Hex(TEXT("EAB95A"));
	const FLinearColor Hp = Hex(TEXT("B8322A"));
	const FLinearColor Stamina = Hex(TEXT("5F9E3E"));
	const FLinearColor Poise = Hex(TEXT("9AA0A6"));
	const FLinearColor Hunger = Hex(TEXT("C98A3A"));
	const FLinearColor Thirst = Hex(TEXT("3F7FC0"));
	const FLinearColor Torpor = Hex(TEXT("B07FD8"));
	const FLinearColor Edge = Hex(TEXT("08090A"));
	const FLinearColor Rim = Hex(TEXT("3B4047"));
	const FLinearColor Shadow = FLinearColor(0.f, 0.f, 0.f, 0.85f);

	FLinearColor Alpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }

	FString Dir() { return FPaths::Combine(FWRTuning::DataDir(), TEXT("UI")); }

	// ---------------------------------------------------------------- fonts

	static FSlateFontInfo FromFile(const FString& File, int32 Size)
	{
		static TMap<FString, TSharedPtr<const FCompositeFont>> Fonts;
		TSharedPtr<const FCompositeFont>& Font = Fonts.FindOrAdd(File);
		if (!Font.IsValid())
		{
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Dir(), TEXT("Fonts"), File));
			if (IFileManager::Get().FileExists(*Path))
			{
				Font = MakeShared<FStandaloneCompositeFont>(FName(*File), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
		}
		if (!Font.IsValid())
		{
			// Missing data files: fall back to the engine font rather than drawing nothing.
			return FCoreStyle::GetDefaultFontStyle("Regular", Size);
		}
		return FSlateFontInfo(Font, Size);
	}

	FSlateFontInfo Title(int32 Size, int32 Weight) { return FromFile(Weight >= 700 ? TEXT("Cinzel-700.ttf") : TEXT("Cinzel-600.ttf"), Size); }

	FSlateFontInfo Body(int32 Size, int32 Weight)
	{
		const TCHAR* File = Weight >= 800 ? TEXT("AlegreyaSans-800.ttf") : Weight >= 700 ? TEXT("AlegreyaSans-700.ttf") : Weight >= 500 ? TEXT("AlegreyaSans-500.ttf") : TEXT("AlegreyaSans-400.ttf");
		return FromFile(File, Size);
	}

	FSlateFontInfo Caps(int32 Size, int32 Weight) { return FromFile(Weight >= 700 ? TEXT("AlegreyaSansSC-700.ttf") : TEXT("AlegreyaSansSC-500.ttf"), Size); }

	// ---------------------------------------------------------------- brushes

	/** A PNG from Data/UI decoded here and handed to Slate as pixels (Slate won't load arbitrary files by path). */
	static const FSlateBrush* FileBrush(const FString& RelPath, const FVector2D& Size, ESlateBrushTileType::Type Tile, bool bMustExist)
	{
		static TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> Brushes;
		if (TSharedPtr<FSlateDynamicImageBrush>* Found = Brushes.Find(RelPath))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateDynamicImageBrush>& Brush = Brushes.Add(RelPath);
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Dir(), RelPath));
		TArray<uint8> File;
		if (!FFileHelper::LoadFileToArray(File, *Path, FILEREAD_Silent))
		{
			if (!bMustExist) UE_LOG(LogTemp, Warning, TEXT("UI image missing: %s"), *Path);
			return nullptr;
		}
		IImageWrapperModule& Wrappers = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Wrappers.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Raw;
		if (!Png.IsValid() || !Png->SetCompressed(File.GetData(), File.Num()) || !Png->GetRaw(ERGBFormat::BGRA, 8, Raw))
		{
			UE_LOG(LogTemp, Warning, TEXT("UI image unreadable: %s"), *Path);
			return nullptr;
		}
		Brush = FSlateDynamicImageBrush::CreateWithImageData(FName(*Path), FVector2D(Png->GetWidth(), Png->GetHeight()), Raw, FLinearColor::White, Tile);
		if (Brush.IsValid() && Tile == ESlateBrushTileType::NoTile)
		{
			Brush->ImageSize = Size;
		}
		return Brush.Get();
	}

	const FSlateBrush* Image(FName Name)
	{
		const FString N = Name.ToString();
		const bool bTile = N == TEXT("stone") || N == TEXT("leather") || N == TEXT("parchment");
		const FVector2D Size = bTile ? FVector2D(256.f, 256.f) : N == TEXT("rivet") ? FVector2D(16.f, 16.f) : N.StartsWith(TEXT("wheel")) ? FVector2D(600.f, 600.f) : N == TEXT("map") ? FVector2D(1009.f, 1009.f) : FVector2D(256.f, 256.f);
		return FileBrush(N + TEXT(".png"), Size, bTile ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile, false);
	}

	const FSlateBrush* Box(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		// Looked up every frame by slots and bars: key on the packed colours and sizes, not a string.
		static TMap<TTuple<uint32, uint32, int32, int32>, TUniquePtr<FSlateBrush>> Boxes;
		const TTuple<uint32, uint32, int32, int32> Key(Fill.ToFColor(false).DWColor(), Outline.ToFColor(false).DWColor(), FMath::RoundToInt(Radius * 4.f), FMath::RoundToInt(OutlineWidth * 4.f));
		TUniquePtr<FSlateBrush>& Brush = Boxes.FindOrAdd(Key);
		if (!Brush.IsValid())
		{
			Brush = MakeUnique<FSlateRoundedBoxBrush>(Fill, Radius, Outline, OutlineWidth);
		}
		return Brush.Get();
	}

	const FSlateBrush* White()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	const FSlateBrush* Icon(FName ItemId)
	{
		const FSlateBrush* B = FileBrush(FString::Printf(TEXT("Icons/%s.png"), *ItemId.ToString()), FVector2D(128.f, 128.f), ESlateBrushTileType::NoTile, true);
		return B ? B : FileBrush(TEXT("Icons/_unknown.png"), FVector2D(128.f, 128.f), ESlateBrushTileType::NoTile, false);
	}

	const FSlateBrush* Portrait(FName CreatureId)
	{
		return FileBrush(FString::Printf(TEXT("Portraits/%s.png"), *CreatureId.ToString()), FVector2D(256.f, 256.f), ESlateBrushTileType::NoTile, true);
	}
}
