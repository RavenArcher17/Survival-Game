// Eating and drinking on the body (cooks/cookpass-survival.md 3.1): the food itself in the off hand, brought to the
// mouth in two bites (Monster Hunter's steak, Red Dead's satchel bite) or a long pull with the head tipped back (a
// waterskin, a potion), committed like Elden Ring's flask (the rules are the server's: UWRInventoryComponent::
// StartConsume / StartDrink). Every machine that draws builds the food from Data/food_look.json in code (FWRToolMesh
// lathe and loft parts, M_HeldTool), lays it in the left hand, plays the bites and chews, and sets a shield or torch in
// that hand out of sight while the hand is busy. Also: a sleeper's breath and the lit torch carried at the chest (3.7).
#include "WRHeldFood.h"
#include "WRCharacter.h"
#include "WRFarmLook.h"
#include "WRGameplayTags.h"
#include "WRHeroAnimInstance.h"
#include "WRInventory.h"
#include "WRAttributeSet.h"
#include "WRSound.h"
#include "WRSurvival.h"
#include "WRToolMesh.h"
#include "WRTuning.h"
#include "Wildreach.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProceduralMeshComponent.h"

namespace WRHeldFoodDetail
{
	FLinearColor ColourOf(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FLinearColor& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Field, A) || A->Num() < 3) return Fallback;
		return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	}

	/** The table, read once: items, byWord, byCategory, raw, the fish colours by item. */
	struct FTable
	{
		TMap<FName, FWRFoodLook> Items;
		TArray<TPair<FString, FName>> Words;
		TMap<FName, FName> ByCategory;
		TMap<FName, FLinearColor> CategoryColour;
		TMap<FName, FLinearColor> Raw;
		TMap<FName, FWRFoodLook> Cache;   // every item asked for, resolved

		static FTable& Get()
		{
			static FTable T;
			return T;
		}

		FTable()
		{
			const TSharedPtr<FJsonObject> Root = FWRTuning::LoadDataFile(TEXT("food_look.json"));
			if (!Root.IsValid())
			{
				UE_LOG(LogWildreach, Warning, TEXT("food_look.json missing: food in the hand shows as a plain fruit"));
				return;
			}
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (Root->TryGetObjectField(TEXT("items"), Obj))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Obj)->Values)
				{
					const TSharedPtr<FJsonObject> O = P.Value->AsObject();
					if (!O.IsValid()) continue;
					FWRFoodLook L;
					L.Shape = FName(*O->GetStringField(TEXT("shape")));
					L.Colour = ColourOf(O, TEXT("color"), L.Colour);
					O->TryGetBoolField(TEXT("long"), L.bLong);
					O->TryGetBoolField(TEXT("flat"), L.bFlat);
					O->TryGetBoolField(TEXT("pie"), L.bPie);
					Items.Add(FName(*P.Key), L);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Root->TryGetArrayField(TEXT("byWord"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const TArray<TSharedPtr<FJsonValue>>& Pair = V->AsArray();
					if (Pair.Num() >= 2) Words.Add({ Pair[0]->AsString(), FName(*Pair[1]->AsString()) });
				}
			}
			if (Root->TryGetObjectField(TEXT("byCategory"), Obj))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Obj)->Values) ByCategory.Add(FName(*P.Key), FName(*P.Value->AsString()));
			}
			if (Root->TryGetObjectField(TEXT("categoryColor"), Obj))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Obj)->Values) CategoryColour.Add(FName(*P.Key), ColourOf(*Obj, *P.Key, FLinearColor(0.6f, 0.35f, 0.15f)));
			}
			if (Root->TryGetObjectField(TEXT("raw"), Obj))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Obj)->Values) Raw.Add(FName(*P.Key), ColourOf(*Obj, *P.Key, FLinearColor(0.55f, 0.12f, 0.1f)));
			}
			// Fish keep their own colours (fish.json look: back, belly, fins as sRGB bytes).
			if (const TSharedPtr<FJsonObject> Fish = FWRTuning::LoadDataFile(TEXT("fish.json")))
			{
				if (Fish->TryGetArrayField(TEXT("fish"), Arr))
				{
					for (const TSharedPtr<FJsonValue>& V : *Arr)
					{
						const TSharedPtr<FJsonObject> O = V->AsObject();
						const TSharedPtr<FJsonObject>* Look = nullptr;
						FString ItemId;
						if (!O.IsValid() || !O->TryGetStringField(TEXT("item"), ItemId) || !O->TryGetObjectField(TEXT("look"), Look)) continue;
						const auto Byte = [&Look](const TCHAR* Part, const FLinearColor& Def)
						{
							const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
							return (*Look)->TryGetArrayField(Part, A) && A->Num() >= 3
								? FLinearColor(FColor(uint8((*A)[0]->AsNumber()), uint8((*A)[1]->AsNumber()), uint8((*A)[2]->AsNumber()))) : Def;
						};
						FWRFoodLook L;
						L.Shape = TEXT("fish");
						L.bFishColours = true;
						L.Back = Byte(TEXT("back"), FLinearColor(0.2f, 0.22f, 0.24f));
						L.Belly = Byte(TEXT("belly"), FLinearColor(0.7f, 0.7f, 0.68f));
						L.Fins = Byte(TEXT("fins"), FLinearColor(0.3f, 0.3f, 0.3f));
						L.Colour = L.Back;
						if (!Items.Contains(FName(*ItemId))) Items.Add(FName(*ItemId), L);
					}
				}
			}
		}
	};
}

const FWRFoodLook& FWRFoodLook::For(FName Item)
{
	using namespace WRHeldFoodDetail;
	FTable& T = FTable::Get();
	if (const FWRFoodLook* Hit = T.Cache.Find(Item)) return *Hit;
	FWRFoodLook L;
	if (const FWRFoodLook* Row = T.Items.Find(Item))
	{
		L = *Row;
	}
	else
	{
		const FString Id = Item.ToString();
		for (const TPair<FString, FName>& W : T.Words)
		{
			if (Id.Contains(W.Key))
			{
				L.Shape = W.Value;
				break;
			}
		}
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(Item);
		const FName Category = Def ? Def->Category : FName(TEXT("food"));
		if (L.Shape.IsNone()) L.Shape = T.ByCategory.FindRef(Category);
		if (L.Shape.IsNone()) L.Shape = TEXT("fruit");
		if (const FLinearColor* C = T.CategoryColour.Find(Category)) L.Colour = *C;
	}
	return T.Cache.Add(Item, L);
}

FLinearColor FWRFoodLook::Raw(FName Shape)
{
	const FLinearColor* C = WRHeldFoodDetail::FTable::Get().Raw.Find(Shape == TEXT("skewer") || Shape == TEXT("strip") ? FName(TEXT("meat")) : Shape);
	return C ? *C : FLinearColor(0.55f, 0.12f, 0.1f);
}

bool FWRFoodLook::IsAlongX(FName Shape)
{
	return Shape == TEXT("meat") || Shape == TEXT("fish") || Shape == TEXT("skewer") || Shape == TEXT("strip") || Shape == TEXT("loaf");
}

void FWRFoodLook::Build(FWRToolMesh& M, const FWRFoodLook& L, const FVector& At, float Cooked, float S)
{
	// Each part at its real size (cm), centred on At; X along, Z up. Colours mix toward the raw colour as Cooked falls.
	const FLinearColor Col = FMath::Lerp(Raw(L.Shape), L.Colour, FMath::Clamp(Cooked, 0.f, 1.f));
	const FLinearColor Darker = Col * 0.6f;
	const FLinearColor Wood(0.42f, 0.27f, 0.14f);
	const FLinearColor Bone(0.86f, 0.8f, 0.68f);
	const FLinearColor Green(0.16f, 0.32f, 0.08f);
	const FName K = L.Shape;
	const auto P = [](float T, float R) { return FVector(T, R, R); };
	const auto PE = [](float T, float RY, float RZ) { return FVector(T, RY, RZ); };
	const float Seed = static_cast<float>(GetTypeHash(K) % 997) + 1.f;

	if (K == TEXT("meat"))
	{
		// A roast haunch: the bone runs through it (knuckle at the grip end, the cut end showing), the meat a seared
		// bulge round it, fuller toward the far end.
		M.Lathe(At + FVector(-14.f, 0.f, 0.f) * S, FVector::ForwardVector, 30.f * S,
			{ P(0.f, 1.7f * S), P(0.05f, 1.9f * S), P(0.1f, 1.2f * S), P(0.2f, 0.9f * S), P(0.9f, 0.9f * S), P(0.95f, 1.3f * S), P(1.f, 1.1f * S) }, 10, Bone, 0.5f, 0.f, Seed);
		M.Lathe(At + FVector(-6.f, 0.f, 0.f) * S, FVector::ForwardVector, 19.f * S,
			{ PE(0.f, 1.6f * S, 1.4f * S), PE(0.15f, 4.2f * S, 3.6f * S), PE(0.5f, 5.8f * S, 4.8f * S), PE(0.8f, 5.2f * S, 4.4f * S), PE(0.95f, 3.2f * S, 2.8f * S), PE(1.f, 1.5f * S, 1.3f * S) },
			12, Col, 0.55f, 0.f, Seed);
		return;
	}
	if (K == TEXT("fish"))
	{
		// A caught fish in its own colours (fish.json); a cooked one (grilled, on the spit) in its cooking colour with a
		// paler belly.
		if (L.bFishColours) FWRFarmLook::Fish(M, At, 26.f * S, L.Back, L.Belly, L.Fins);
		else FWRFarmLook::Fish(M, At, 26.f * S, Col, FMath::Lerp(Col, FLinearColor(0.78f, 0.72f, 0.6f), 0.4f), Col * 0.8f);
		return;
	}
	if (K == TEXT("bowl"))
	{
		// A turned wooden bowl, full: the dish's surface a little below the rim.
		M.Lathe(At, FVector::UpVector, 6.5f * S, { P(0.f, 3.8f * S), P(0.12f, 5.2f * S), P(0.5f, 7.2f * S), P(0.92f, 8.2f * S), P(1.f, 8.4f * S) }, 18, Wood, 0.6f, 0.f, Seed);
		M.Lathe(At + FVector(0.f, 0.f, 5.4f * S), FVector::UpVector, 0.8f * S, { P(0.f, 7.6f * S), P(1.f, 7.8f * S) }, 18, Col, 0.35f);
		return;
	}
	if (K == TEXT("loaf"))
	{
		if (L.bFlat)
		{
			// A flatbread or a folded omelette: a thin round with a scorched edge.
			M.Lathe(At, FVector::UpVector, 1.6f * S, { P(0.f, 8.f * S), P(0.5f, 9.f * S), P(1.f, 8.2f * S) }, 18, Col, 0.7f, 0.f, Seed);
			return;
		}
		if (L.bPie)
		{
			// A lidded pie: the dish of pastry, a domed lid, a crimped rim, three steam slits.
			M.Lathe(At, FVector::UpVector, 4.f * S, { P(0.f, 7.f * S), P(0.7f, 8.5f * S), P(0.85f, 9.f * S), P(1.f, 7.5f * S) }, 20, Col, 0.6f, 0.f, Seed);
			TArray<FVector> Rim;
			for (int32 i = 0; i <= 40; ++i)
			{
				const float A = i * 2.f * PI / 40.f;
				const float R = (8.6f + 0.5f * FMath::Sin(A * 14.f)) * S;
				Rim.Add(At + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 3.4f * S));
			}
			M.Tube(Rim, 0.7f * S, 6, Col * 0.85f, 0.6f, true);
			for (int32 c = -1; c <= 1; ++c) M.Tube({ At + FVector(-2.f, c * 2.2f, 4.1f) * S, At + FVector(2.f, c * 2.2f, 4.1f) * S }, 0.35f * S, 5, Darker, 0.8f);
			return;
		}
		// A loaf: an oval dome lathed along X, flattened underneath, three scores across the crust.
		M.Lathe(At, FVector::ForwardVector, 18.f * S, { PE(0.f, 2.f * S, 1.8f * S), PE(0.12f, 4.6f * S, 3.6f * S), PE(0.5f, 5.6f * S, 4.2f * S), PE(0.88f, 4.6f * S, 3.6f * S), PE(1.f, 2.f * S, 1.8f * S) }, 14, Col, 0.7f, 0.f, Seed);
		for (int32 c = 0; c < 3; ++c)
		{
			const float X = (5.f + c * 4.f) * S;
			M.Tube({ At + FVector(X - 1.2f * S, -3.f * S, 3.7f * S), At + FVector(X + 1.2f * S, 3.f * S, 3.7f * S) }, 0.4f * S, 5, Darker, 0.8f);
		}
		return;
	}
	if (K == TEXT("cake"))
	{
		M.Lathe(At, FVector::UpVector, 4.f * S, { P(0.f, 6.f * S), P(0.9f, 6.2f * S), P(1.f, 5.6f * S) }, 18, Col, 0.6f, 0.f, Seed);
		M.Lathe(At + FVector(0.f, 0.f, 3.9f * S), FVector::UpVector, 0.6f * S, { P(0.f, 5.4f * S), P(1.f, 4.6f * S) }, 18, FMath::Lerp(Col, FLinearColor(0.95f, 0.75f, 0.3f), 0.5f), 0.2f);
		return;
	}
	if (K == TEXT("berries"))
	{
		// A cupped handful: seven berries heaped, a leaf under them.
		static const FVector Spots[] = { {0, 0, 0}, {2.1f, 0.6f, 0.1f}, {-1.9f, 1.1f, 0.2f}, {0.6f, -2.f, 0.f}, {-0.9f, -1.6f, 0.4f}, {1.2f, 1.8f, 0.3f}, {0.2f, 0.4f, 1.8f} };
		const float R = (L.Colour.R > 0.8f ? 1.3f : 1.05f) * S;
		for (int32 b = 0; b < 7; ++b)
		{
			const FVector C = At + Spots[b] * S;
			M.Lathe(C - FVector(0.f, 0.f, R), FVector::UpVector, 2.f * R, { P(0.f, 0.f), P(0.15f, 0.7f * R), P(0.5f, R), P(0.85f, 0.7f * R), P(1.f, 0.f) }, 8, b % 2 ? Col : Col * 0.85f, 0.25f);
		}
		M.Lathe(At + FVector(0.f, 0.f, -1.6f * S), FVector::UpVector, 0.3f * S, { PE(0.f, 4.5f * S, 2.8f * S), PE(1.f, 4.5f * S, 2.8f * S) }, 8, Green, 0.6f);
		return;
	}
	if (K == TEXT("fruit"))
	{
		if (L.bLong)
		{
			// A root or a pod: long and tapering, a green crown or stalk at the top.
			M.Lathe(At + FVector(-7.f, 0.f, 0.f) * S, FVector::ForwardVector, 14.f * S, { P(0.f, 0.3f * S), P(0.2f, 1.2f * S), P(0.6f, 2.f * S), P(0.92f, 2.2f * S), P(1.f, 1.6f * S) }, 10, Col, 0.35f, 0.4f * S, Seed);
			M.Lathe(At + FVector(7.f, 0.f, 0.f) * S, FVector(1.f, 0.f, 0.4f).GetSafeNormal(), 4.f * S, { P(0.f, 0.8f * S), P(1.f, 0.3f * S) }, 6, Green, 0.6f);
			return;
		}
		M.Lathe(At + FVector(0.f, 0.f, -3.6f * S), FVector::UpVector, 7.f * S, { P(0.f, 0.f), P(0.12f, 2.6f * S), P(0.45f, 3.7f * S), P(0.8f, 3.2f * S), P(0.94f, 1.6f * S), P(1.f, 0.f) }, 14, Col, 0.3f, 0.f, Seed);
		M.Lathe(At + FVector(0.f, 0.f, 3.f * S), FVector(0.15f, 0.f, 1.f).GetSafeNormal(), 2.f * S, { P(0.f, 0.3f * S), P(1.f, 0.2f * S) }, 5, FLinearColor(0.25f, 0.17f, 0.08f), 0.7f);
		return;
	}
	if (K == TEXT("skewer"))
	{
		// A green stick through four chunks, its point charred.
		M.Lathe(At + FVector(-14.f, 0.f, 0.f) * S, FVector::ForwardVector, 32.f * S, { P(0.f, 0.45f * S), P(0.9f, 0.4f * S), P(1.f, 0.05f * S) }, 6, Wood, 0.7f);
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector C = At + FVector(-2.f + c * 4.4f, 0.f, 0.f) * S;
			M.Lathe(C - FVector(1.7f, 0.f, 0.f) * S, FVector(1.f, 0.1f * (c % 2 ? 1 : -1), 0.f).GetSafeNormal(), 3.4f * S, { PE(0.f, 1.4f * S, 1.2f * S), PE(0.5f, 2.f * S, 1.7f * S), PE(1.f, 1.4f * S, 1.2f * S) }, 6, c % 2 ? Col : Col * 0.82f, 0.6f, 0.f, Seed + c);
		}
		return;
	}
	if (K == TEXT("strip"))
	{
		// A dried strip: flat, rippled, a little curled.
		M.Lathe(At + FVector(-8.f, 0.f, 0.f) * S, FVector::ForwardVector, 16.f * S, { PE(0.f, 1.2f * S, 0.3f * S), PE(0.3f, 1.7f * S, 0.4f * S), PE(0.7f, 1.6f * S, 0.35f * S), PE(1.f, 1.f * S, 0.25f * S) }, 6, Col, 0.75f, -1.5f * S, Seed);
		return;
	}
	if (K == TEXT("egg"))
	{
		M.Lathe(At + FVector(0.f, 0.f, -3.f * S), FVector::UpVector, 6.f * S, { P(0.f, 0.f), P(0.1f, 1.5f * S), P(0.4f, 2.3f * S), P(0.75f, 2.f * S), P(0.95f, 1.f * S), P(1.f, 0.f) }, 12, Col, 0.35f);
		return;
	}
	if (K == TEXT("mushroom"))
	{
		M.Lathe(At + FVector(0.f, 0.f, -3.f * S), FVector::UpVector, 4.5f * S, { P(0.f, 1.1f * S), P(1.f, 0.9f * S) }, 8, FLinearColor(0.85f, 0.82f, 0.72f), 0.6f);
		M.Lathe(At + FVector(0.f, 0.f, 1.f * S), FVector::UpVector, 3.f * S, { P(0.f, 4.f * S), P(0.3f, 4.2f * S), P(0.7f, 3.f * S), P(1.f, 0.f) }, 14, Col, 0.5f, 0.f, Seed);
		return;
	}
	if (K == TEXT("waterskin"))
	{
		// A stitched leather bag hanging from its wooden neck: wide at the bottom, gathered at the top, a cord round the
		// neck and a stopper in it; the seam down the side in darker thread.
		M.Lathe(At + FVector(0.f, 0.f, -11.f * S), FVector::UpVector, 18.f * S, { PE(0.f, 2.f * S, 1.6f * S), PE(0.12f, 6.5f * S, 4.6f * S), PE(0.4f, 7.6f * S, 5.4f * S), PE(0.72f, 5.6f * S, 4.2f * S), PE(0.9f, 2.f * S, 1.8f * S), PE(1.f, 1.5f * S, 1.5f * S) }, 14, Col, 0.6f, 0.f, Seed);
		M.Lathe(At + FVector(0.f, 0.f, 6.6f * S), FVector::UpVector, 3.2f * S, { P(0.f, 1.5f * S), P(0.8f, 1.4f * S), P(1.f, 1.7f * S) }, 10, Wood, 0.5f);
		M.Lathe(At + FVector(0.f, 0.f, 9.6f * S), FVector::UpVector, 1.6f * S, { P(0.f, 1.1f * S), P(1.f, 1.2f * S) }, 8, FLinearColor(0.55f, 0.42f, 0.25f), 0.8f);
		TArray<FVector> Cord;
		for (int32 i = 0; i <= 16; ++i) Cord.Add(At + FVector(FMath::Cos(i * 2.f * PI / 16.f) * 1.8f, FMath::Sin(i * 2.f * PI / 16.f) * 1.8f, 6.2f) * S);
		M.Tube(Cord, 0.35f * S, 5, FLinearColor(0.3f, 0.24f, 0.15f), 0.9f, true);
		TArray<FVector> Seam;
		for (int32 i = 0; i <= 8; ++i)
		{
			const float T = i / 8.f;
			const float R = FMath::Lerp(1.7f, 5.2f, FMath::Sin(PI * FMath::Clamp(T * 1.1f, 0.f, 1.f))) * S;
			Seam.Add(At + FVector(0.f, R + 0.15f * S, FMath::Lerp(-10.f, 5.f, T) * S));
		}
		M.Tube(Seam, 0.22f * S, 4, Col * 0.45f, 0.9f);
		return;
	}
	if (K == TEXT("jar"))
	{
		// A clay jar: a round belly, a short neck, a cloth tied over the mouth (the colour of what's in it).
		const FLinearColor Clay(0.55f, 0.32f, 0.2f);
		M.Lathe(At + FVector(0.f, 0.f, -6.f * S), FVector::UpVector, 11.f * S, { P(0.f, 3.f * S), P(0.15f, 4.6f * S), P(0.5f, 5.4f * S), P(0.8f, 3.6f * S), P(0.9f, 2.6f * S), P(1.f, 3.f * S) }, 14, Clay, 0.75f, 0.f, Seed);
		M.Lathe(At + FVector(0.f, 0.f, 4.6f * S), FVector::UpVector, 1.4f * S, { P(0.f, 3.4f * S), P(0.6f, 3.3f * S), P(1.f, 2.2f * S) }, 14, Col, 0.8f);
		return;
	}
	// A potion: a stoppered glass vial (the drink's colour shows through), a waxed cork.
	M.Lathe(At + FVector(0.f, 0.f, -5.f * S), FVector::UpVector, 7.5f * S, { P(0.f, 1.8f * S), P(0.1f, 2.4f * S), P(0.7f, 2.4f * S), P(0.9f, 1.2f * S), P(1.f, 0.8f * S) }, 12, Col, 0.12f);
	M.Lathe(At + FVector(0.f, 0.f, 2.5f * S), FVector::UpVector, 2.5f * S, { P(0.f, 0.8f * S), P(0.8f, 0.85f * S), P(1.f, 1.1f * S) }, 10, FLinearColor(0.75f, 0.85f, 0.85f), 0.08f);
	M.Lathe(At + FVector(0.f, 0.f, 4.8f * S), FVector::UpVector, 1.5f * S, { P(0.f, 0.8f * S), P(1.f, 0.95f * S) }, 8, FLinearColor(0.55f, 0.4f, 0.25f), 0.8f);
}

// ---------------------------------------------------------------- on the body

void AWRCharacter::MulticastConsume_Implementation(FName Item, float Seconds, bool bDrink)
{
	ConsumeItem = Item;
	ConsumeStart = GetWorld()->GetTimeSeconds();
	ConsumeSeconds = FMath::Max(0.1f, Seconds);
	bConsumeDrink = bDrink;
	ConsumeBitesPlayed = 0;
	// A hero with its own drink clip plays it (the hand pose gives way to it; the bottle stays in the hand).
	if (bDrink && BodyAnims.Contains(TEXT("drink")))
	{
		MulticastBodyOneShot_Implementation(TEXT("drink"));
	}
}

float AWRCharacter::ConsumeFraction() const
{
	if (ConsumeSeconds <= 0.f || ConsumeItem.IsNone() || !GetWorld())
	{
		return -1.f;
	}
	const double Elapsed = GetWorld()->GetTimeSeconds() - ConsumeStart;
	const float T = static_cast<float>(Elapsed / ConsumeSeconds);
	if (T < 0.f || T >= 1.f || !IsAlive())
	{
		return -1.f;
	}
	// Stopped by the server (a stagger, a freeze): its State.Drinking went (a moment's grace for the tag to arrive).
	if (Elapsed > 0.25 && !HasTag(WRTags::State_Drinking))
	{
		return -1.f;
	}
	return T;
}

void AWRCharacter::FillConsumeInput(FWRHeroAnimInput& H)
{
	const FWRTuning& T = FWRTuning::Get();
	const float Frac = ConsumeFraction();
	H.Consume = 0.f;
	H.ConsumeBite = 0.f;
	H.bConsumeDrink = bConsumeDrink;
	if (Frac >= 0.f)
	{
		// Eased in and out over easeSeconds (smooth), the bites as short pushes centred on their moments.
		const float Ease = FMath::Clamp(T.Num(TEXT("food.pose.easeSeconds"), 0.25f) / FMath::Max(0.1f, ConsumeSeconds), 0.05f, 0.45f);
		const float In = FMath::Clamp(FMath::Min(Frac, 1.f - Frac) / Ease, 0.f, 1.f);
		H.Consume = In * In * (3.f - 2.f * In);
		if (!bConsumeDrink)
		{
			for (const float B : { T.Num(TEXT("food.eat.bite1"), 0.23f), T.Num(TEXT("food.eat.bite2"), 0.57f) })
			{
				const float D = FMath::Clamp(1.f - FMath::Abs(Frac - B) / 0.09f, 0.f, 1.f);
				H.ConsumeBite = FMath::Max(H.ConsumeBite, D * D * (3.f - 2.f * D));
			}
		}
	}
	// Asleep on the bed: the breath (rest.breathHz).
	const UWRSurvivalComponent* Survival = UWRSurvivalComponent::For(this);
	H.Sleep = bSleepPose ? 1.f : 0.f;
	H.Breath = FMath::Frac(static_cast<float>(GetWorld()->GetTimeSeconds()) * T.Num(TEXT("rest.breathHz"), 0.22f));
	// The lit torch carried at the chest (3.7): in the off hand, standing or moving on foot, not aiming or guarding.
	const bool bCarry = LightMesh && LightMesh->IsVisible() && LightMeshFor != NAME_None && Survival && Survival->HoldsTorch()
		&& !bSleepPose && IsAlive() && !GetCharacterMovement()->IsSwimming() && !IsAimingRanged() && H.Stance == EWRHeroStance::Melee && H.Seat < 0.01f;
	const float Dt = GetWorld()->GetDeltaSeconds();
	TorchCarryBlend = FMath::FInterpConstantTo(TorchCarryBlend, bCarry ? 1.f : 0.f, Dt, 1.f / FMath::Max(0.05f, T.Num(TEXT("lights.carry.easeSeconds"), 0.3f)));
	H.TorchCarry = TorchCarryBlend;
}

void AWRCharacter::TickConsumeLook(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TickBodyNeeds();
	const float Frac = ConsumeFraction();
	const bool bActive = Frac >= 0.f && Body && Body->IsVisible();
	if (!bActive)
	{
		if (FoodMesh && FoodMesh->IsVisible()) FoodMesh->SetVisibility(false);
		if (bOffHandStowed)
		{
			// The shield or torch comes back to the hand.
			bOffHandStowed = false;
			if (ShieldMesh) ShieldMesh->SetHiddenInGame(false);
			if (LightMesh) LightMesh->SetHiddenInGame(false, true);
		}
		if (Frac < 0.f && ConsumeSeconds > 0.f && GetWorld()->GetTimeSeconds() - ConsumeStart > ConsumeSeconds)
		{
			ConsumeItem = NAME_None;
			ConsumeSeconds = 0.f;
		}
		return;
	}

	if (!FoodMesh)
	{
		FoodMesh = NewObject<UProceduralMeshComponent>(this, TEXT("HeldFoodMesh"));
		FoodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FoodMesh->SetCanEverAffectNavigation(false);
		FoodMesh->SetupAttachment(Body);
		FoodMesh->RegisterComponent();
		FoodMesh->SetUsingAbsoluteLocation(true);
		FoodMesh->SetUsingAbsoluteRotation(true);
	}
	const FWRFoodLook& Look = FWRFoodLook::For(ConsumeItem);
	if (FoodMeshFor != ConsumeItem)
	{
		FoodMeshFor = ConsumeItem;
		FWRToolMesh M;
		FWRFoodLook::Build(M, Look, FVector::ZeroVector);
		M.ApplyTo(FoodMesh);
	}
	FoodMesh->SetVisibility(true);
	// The hand is busy: a shield or a torch in it is set out of sight until it's free again (the light keeps burning).
	if (!bOffHandStowed)
	{
		bOffHandStowed = true;
		if (ShieldMesh) ShieldMesh->SetHiddenInGame(true);
		if (LightMesh) LightMesh->SetHiddenInGame(true, true);
	}

	// In the left hand's palm (the bone runs toward the fingers along its X): food held across the face with its far end
	// tipped up, standing things upright and tipped toward the mouth (a drink's neck to the lips, more as the pull goes).
	const FTransform Hand = Body->GetSocketTransform(TEXT("hand_l"));
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();
	const FVector Palm = Hand.GetLocation() + Hand.GetUnitAxis(EAxis::X) * 8.f;
	FRotator Rot;
	if (FWRFoodLook::IsAlongX(Look.Shape))
	{
		Rot = FRotationMatrix::MakeFromXZ((Right * 0.85f + FVector::UpVector * 0.45f - Fwd * 0.15f).GetSafeNormal(), FVector::UpVector).Rotator();
	}
	else
	{
		const float Tilt = bConsumeDrink ? 25.f + 65.f * FMath::Clamp((Frac - 0.15f) / 0.5f, 0.f, 1.f) : (Look.Shape == TEXT("bowl") ? 35.f : 10.f);
		const FVector Up = FVector::UpVector.RotateAngleAxis(-Tilt, Right);   // the top tips back toward the face
		Rot = FRotationMatrix::MakeFromZX(Up, Fwd).Rotator();
	}
	FoodMesh->SetWorldLocationAndRotation(Palm, Rot);

	// The bites and chews (each machine plays its own; the swallow comes from the server as the worth lands).
	if (!bConsumeDrink)
	{
		const FWRTuning& T = FWRTuning::Get();
		const float B1 = T.Num(TEXT("food.eat.bite1"), 0.23f), B2 = T.Num(TEXT("food.eat.bite2"), 0.57f);
		const float Moments[4] = { B1, B1 + 0.12f, B2, B2 + 0.12f };
		const FName Events[4] = { TEXT("eat.bite"), TEXT("eat.chew"), TEXT("eat.bite"), TEXT("eat.chew") };
		while (ConsumeBitesPlayed < 4 && Frac >= Moments[ConsumeBitesPlayed])
		{
			const FVector Mouth = Body->DoesSocketExist(TEXT("head")) ? Body->GetSocketLocation(TEXT("head")) : GetActorLocation() + FVector(0.f, 0.f, 60.f);
			WRSound::PlayAt(this, Events[ConsumeBitesPlayed], Mouth);
			++ConsumeBitesPlayed;
		}
	}
}

void AWRCharacter::TickBodyNeeds()
{
	// Your own body, heard only by you (radius-0 rows): the stomach growls under food.warn.bodyBelow Food, the dry throat
	// swallows under it Water, each every bodyGapMin..bodyGapMax seconds (Red Dead's growl: the body tells you before
	// the meter does).
	if (!IsLocallyControlled() || !IsAlive() || bSleepPose)
	{
		return;
	}
	const UWRAttributeSet* A = GetAttributes();
	if (!A)
	{
		return;
	}
	const FWRTuning& T = FWRTuning::Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Below = T.Num(TEXT("food.warn.bodyBelow"), 20.f);
	const float GapMin = T.Num(TEXT("food.warn.bodyGapMin"), 45.f), GapMax = FMath::Max(GapMin, T.Num(TEXT("food.warn.bodyGapMax"), 75.f));
	const auto Tick = [&](float Value, double& NextAt, FName Event)
	{
		if (Value >= Below)
		{
			NextAt = -1.0;
			return;
		}
		if (NextAt < 0.0)
		{
			NextAt = Now + FMath::FRandRange(4.f, 12.f);   // the first soon after crossing the line
			return;
		}
		if (Now >= NextAt)
		{
			WRSound::PlayAt(this, Event, GetActorLocation());
			// Emptier, more often.
			NextAt = Now + FMath::FRandRange(GapMin, GapMax) * FMath::Lerp(0.6f, 1.f, FMath::Clamp(Value / FMath::Max(1.f, Below), 0.f, 1.f));
		}
	};
	Tick(A->GetHunger(), NextStomachAt, TEXT("body.stomach"));
	Tick(A->GetThirst(), NextDryAt, TEXT("body.dry"));
}
