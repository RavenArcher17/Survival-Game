#include "WRPathLook.h"
#include "WRPathTypes.h"
#include "WRUIStyle.h"

namespace WRPathLook
{
	FLinearColor Color(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		if (Def && Def->Color.A > 0.f)
		{
			return Def->Color;
		}
		if (Path == TEXT("frost")) return WRUI::Frost;
		if (Path == TEXT("lightning")) return FLinearColor(FColor(0xB9, 0xA7, 0xFF));
		return WRUI::Ember;
	}

	FName Icon(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		if (Def && !Def->Icon.IsNone())
		{
			return Def->Icon;
		}
		return Path == TEXT("frost") ? FName(TEXT("frost_essence")) : Path == TEXT("lightning") ? FName(TEXT("stormglass")) : FName(TEXT("ember_essence"));
	}

	FString Glyph(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		if (Def && !Def->Glyph.IsEmpty())
		{
			return Def->Glyph;
		}
		return Path == TEXT("frost") ? FString(TEXT("snowflake")) : Path == TEXT("lightning") ? FString(TEXT("weather_storm")) : FString(TEXT("flame"));
	}

	FString Name(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		if (Def && !Def->Name.IsEmpty())
		{
			return Def->Name;
		}
		FString S = Path.ToString();
		if (S.Len() > 0) S[0] = FChar::ToUpper(S[0]);
		return S;
	}

	FName ResourceId(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		return Def ? Def->ResourceId : NAME_None;
	}

	FString ResourceName(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		if (Def && !Def->ResourceName.IsEmpty())
		{
			return Def->ResourceName;
		}
		FString S = Def ? Def->ResourceId.ToString() : FString();
		if (S.Len() > 0) S[0] = FChar::ToUpper(S[0]);
		return S;
	}

	FString BiomeName(FName Path)
	{
		const FWRPathDef* Def = FWRPathRegistry::Get().Find(Path);
		if (!Def) return FString();
		if (Def->Biome == TEXT("tundra")) return TEXT("Tundra");
		FString S = Def->Biome.ToString().Replace(TEXT("_"), TEXT(" "));
		if (S.Len() > 0) S[0] = FChar::ToUpper(S[0]);
		return S;
	}

	FLinearColor Dark(FName Path, float Share)
	{
		const FLinearColor C = Color(Path);
		return FLinearColor(C.R * Share, C.G * Share, C.B * Share, 1.f);
	}
}
