// The hero's look on every machine (cook hero-look-2, hero.md 2.5 / 2.6): the worn armour's outfit, its element trim and
// Ascendant inlay, the body-hide masks under it, and the path marks on the skin, eyes and hair.
// Everything here is data (Data/player_art.json "looks", "armorLooks", "elements", "paths"); the code only applies it.
#include "WRCharacter.h"
#include "WRSporePath.h"
#include "WRTidePath.h"
#include "WRSunPath.h"
#include "WRWindPath.h"     // the Wind body change (cooks/biome-sky.md 2.18.5)
#include "WRNecroPath.h"
#include "WRArcanePath.h"
#include "WRVoidPath.h"
#include "WRInventory.h"
#include "Wildreach.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
#include "WRPlayerState.h"
#include "WRSound.h"
#include "WRTuning.h"
#include "WREndgameLook.h"
#include "WRBloodPath.h"
#include "WRNaturePath.h"   // the Nature body change (cooks/biome-forest.md 2.18)
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "WRToolMesh.h"

namespace WRHeroLookImpl
{
	static const TCHAR* SlotNames[] = { TEXT("Head"), TEXT("Chest"), TEXT("Hands"), TEXT("Legs") };
	static const TCHAR* BuildNames[] = { TEXT("Lean"), TEXT("Athletic"), TEXT("Strong") };

	FLinearColor Color(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FLinearColor& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Arr) || Arr->Num() < 3)
		{
			return Fallback;
		}
		return FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), Arr->Num() > 3 ? (*Arr)[3]->AsNumber() : 1.0);
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& Parent, const FString& Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Parent.IsValid() && Parent->TryGetObjectField(Field, Out) ? *Out : nullptr;
	}

	/** hero.md 2.6: strength = smoothstep(0.08, 1.0, points / max points per path). */
	float Strength(int32 XP)
	{
		const float Max = FMath::Max(1.f, WRTune(TEXT("paths.maxPointsPerPath"), 100.f));
		const float X = FMath::Clamp(FWRPathRules::PointsForXP(XP) / Max, 0.f, 1.f);
		return FMath::SmoothStep(0.08f, 1.f, X);
	}

	/** A material slot's role on the MetaHuman head, by its slot name (player_art.json "lookSlots" lists the names). */
	bool SlotIs(const TSharedPtr<FJsonObject>& Art, const TCHAR* Role, FName SlotName)
	{
		const TSharedPtr<FJsonObject> Slots = Obj(Art, TEXT("lookSlots"));
		const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
		if (!Slots.IsValid() || !Slots->TryGetArrayField(Role, Names))
		{
			return false;
		}
		const FString S = SlotName.ToString();
		for (const TSharedPtr<FJsonValue>& V : *Names)
		{
			if (S.Equals(V->AsString(), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}
using namespace WRHeroLookImpl;

// Code-modelled head pieces (a look row's "headPiece" in player_art.json): one runtime mesh each in M_HeldTool (vertex
// colour = surface, alpha = roughness), on the head bone. Space: X toward the face, Y right, Z up; the origin is the
// band's centre at the brow. The band is an oval round the skull over the hair (Rx, Ry cm), its back riding Tilt degrees
// higher the way a crown sits on the back of the head.
namespace WRHeroHeadPieceImpl
{
	FLinearColor Lin(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	struct FBand
	{
		float Rx = 11.f, Ry = 9.2f, Tilt = 9.f;
		/** The band's point at angle A (0 = the brow, +PI/2 = the right temple), Z cm up off it. */
		FVector At(float A, float Z = 0.f) const
		{
			const float X = FMath::Cos(A) * Rx;
			return FVector(X, FMath::Sin(A) * Ry, Z - X * FMath::Tan(FMath::DegreesToRadians(Tilt)));
		}
		/** Straight out from the skull there (level). */
		FVector Out(float A) const { return FVector(FMath::Cos(A) / Rx, FMath::Sin(A) / Ry, 0.f).GetSafeNormal(); }
		/** Along the band toward a larger A. */
		FVector Along(float A) const { return (At(A + 0.01f) - At(A - 0.01f)).GetSafeNormal(); }
	};

	/** A thin flat length (a rag, a leaf) from P0 to P1: Across its width, Normal its face; a closed section so both
	 *  faces show and it has an edge. */
	void Slab(FWRToolMesh& M, const FVector& P0, const FVector& P1, const FVector& Across, const FVector& Normal, float W0, float W1, float Thick, const FLinearColor& C, float Rough)
	{
		const auto Section = [&Across, &Normal, Thick](const FVector& P, float W)
		{
			const FVector A = Across * (FMath::Max(0.05f, W) * 0.5f), N = Normal * (Thick * 0.5f);
			return TArray<FVector>{ P - A - N, P + A - N, P + A + N, P - A + N };
		};
		M.Loft({ Section(P0, W0), Section(P1, W1) }, C, Rough, true);
	}

	/** A flat strip along Spine (each piece reaching a hair into the next so a bend shows no crack), Widths per point. */
	void Strip(FWRToolMesh& M, const TArray<FVector>& Spine, const TArray<float>& Widths, const FVector& Across, float Thick, const FLinearColor& C, float Rough)
	{
		for (int32 i = 0; i + 1 < Spine.Num() && i + 1 < Widths.Num(); ++i)
		{
			const FVector Dir = (Spine[i + 1] - Spine[i]).GetSafeNormal();
			const FVector Side = (Across - Dir * FVector::DotProduct(Across, Dir)).GetSafeNormal();
			Slab(M, Spine[i], Spine[i + 1] + Dir * 0.12f, Side, FVector::CrossProduct(Dir, Side), Widths[i], Widths[i + 1], Thick, C, Rough);
		}
	}

	/** An ivy leaf off the band: a short stalk, a broad base, a pointed tip, curling a little away from the head. */
	void Leaf(FWRToolMesh& M, const FVector& Base, const FVector& Dir, const FVector& Face, float Len, float Width, const FLinearColor& C)
	{
		const FVector Side = FVector::CrossProduct(Face, Dir).GetSafeNormal();
		Strip(M, { Base, Base + Dir * (Len * 0.18f), Base + Dir * (Len * 0.45f) + Face * (Len * 0.06f), Base + Dir * (Len * 0.75f) + Face * (Len * 0.1f), Base + Dir * Len + Face * (Len * 0.07f) },
			{ 0.1f, Width * 0.9f, Width, Width * 0.55f, 0.05f }, Side, 0.12f, C, 0.5f);
	}

	/** A tapering round length through Pts (an antler tine), its colour running C0 -> C1 base to tip. */
	void Rod(FWRToolMesh& M, const TArray<FVector>& Pts, float R0, float R1, const FLinearColor& C0, const FLinearColor& C1, float Rough, float Seed)
	{
		const int32 N = Pts.Num() - 1;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector D = Pts[i + 1] - Pts[i];
			const float L = D.Size();
			if (L < 0.01f) continue;
			const FVector Dir = D / L;
			const float RA = FMath::Lerp(R0, R1, float(i) / N), RB = FMath::Lerp(R0, R1, float(i + 1) / N);
			const float Back = FMath::Min(RA * 0.6f, L * 0.3f);
			M.Lathe(Pts[i] - Dir * Back, Dir, L + Back, { FVector(0.f, RA, RA), FVector(1.f, RB, RB) }, 7, FMath::Lerp(C0, C1, (i + 0.5f) / N), Rough, 0.f, Seed + i);
		}
	}

	/** A round bead (a true sphere: the rings spaced by the angle). */
	void Ball(FWRToolMesh& M, const FVector& Centre, float R, const FLinearColor& C, float Rough)
	{
		TArray<FVector> P;
		for (int32 i = 0; i <= 8; ++i)
		{
			const float A = PI * i / 8.f;
			const float Rr = FMath::Max(0.02f, R * FMath::Sin(A));
			P.Add(FVector((1.f - FMath::Cos(A)) * 0.5f, Rr, Rr));
		}
		M.Lathe(Centre - FVector(0.f, 0.f, R), FVector::UpVector, 2.f * R, P, 12, C, Rough);
	}

	/** A small closed loop of Radius round Axis at Centre (an antler's burr, a bezel). */
	TArray<FVector> Loop(const FVector& Centre, const FVector& Axis, float Radius, int32 N)
	{
		const FVector U = FVector::CrossProduct(Axis, FMath::Abs(Axis.Z) > 0.9f ? FVector::XAxisVector : FVector::UpVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Axis, U);
		TArray<FVector> Points;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 2.f * PI * i / N;
			Points.Add(Centre + (U * FMath::Cos(A) + V * FMath::Sin(A)) * Radius);
		}
		return Points;
	}

	/** The Huntsman's crown (cooks/biome-forest.md 2.19, the Wild Hunt's leader's): a ring of polished antler beam bound
	 *  with a living ivy vine, eight antler tines rising round it (the tallest pair forked, the brow pair leaning forward,
	 *  each with its rough burr at the root), ivy leaves along the band, and three banner rags of the Hunt (a faded red,
	 *  a moss green, an undyed linen) knotted on at the back and hanging down behind. */
	void HuntsmansCrown(FWRToolMesh& M, const FBand& B)
	{
		const FLinearColor Beam = Lin(150, 124, 92), AntlerDark = Lin(106, 84, 60), AntlerPale = Lin(226, 214, 190), BurrC = Lin(88, 70, 50);
		const FLinearColor Vine = Lin(70, 62, 36), Ivy = Lin(46, 90, 34), IvyDark = Lin(34, 66, 28);
		const FVector Up = FVector::UpVector;
		TArray<FVector> Ring, Helix;
		for (int32 i = 0; i < 48; ++i) Ring.Add(B.At(2.f * PI * i / 48.f));
		M.Tube(Ring, 0.8f, 8, Beam, 0.55f, true);
		// The vine: nine turns round the beam, closing on itself.
		for (int32 i = 0; i < 180; ++i)
		{
			const float A = 2.f * PI * i / 180.f, Turn = A * 9.f;
			Helix.Add(B.At(A) + (B.Out(A) * FMath::Cos(Turn) + Up * FMath::Sin(Turn)) * 0.95f);
		}
		M.Tube(Helix, 0.24f, 5, Vine, 0.8f, true);
		// The tines, a mirrored pair at each of four places round the band: angle from the brow (deg), length, lean out
		// (deg), the fork's length (0: none).
		struct FTine { float Deg, Len, Lean, Fork; };
		static const FTine Tines[] = { { 24.f, 7.5f, 30.f, 0.f }, { 60.f, 13.f, 20.f, 5.f }, { 102.f, 11.f, 16.f, 0.f }, { 144.f, 7.f, 24.f, 0.f } };
		int32 Seed = 1;
		for (const FTine& Tine : Tines)
		{
			for (const float S : { 1.f, -1.f })
			{
				const float A = S * FMath::DegreesToRadians(Tine.Deg);
				const FVector Out = B.Out(A), Along = B.Along(A);
				const float Lean = FMath::DegreesToRadians(Tine.Lean);
				// The brow pair lean forward over the face a little (the brow tines of a stag's head).
				const FVector Dir0 = (Up * FMath::Cos(Lean) + Out * FMath::Sin(Lean) + (Tine.Deg < 40.f ? FVector(0.25f, 0.f, 0.f) : FVector::ZeroVector)).GetSafeNormal();
				const FVector P0 = B.At(A, 0.3f) + Out * 0.3f;
				const FVector P1 = P0 + Dir0 * (Tine.Len * 0.4f);
				const FVector P2 = P1 + (Dir0 + Up * 0.6f - Out * 0.25f).GetSafeNormal() * (Tine.Len * 0.35f);
				const FVector P3 = P2 + (Up - Out * 0.35f + Along * (S * 0.15f)).GetSafeNormal() * (Tine.Len * 0.25f);
				Rod(M, { P0, P1, P2, P3 }, 0.85f, 0.12f, AntlerDark, AntlerPale, 0.5f, float(Seed++));
				// The burr: the rough ring of bone where an antler grows from the pedicle.
				M.Tube(Loop(P0 + Dir0 * 0.35f, Dir0, 1.05f, 10), 0.28f, 5, BurrC, 0.9f, true);
				if (Tine.Fork > 0.f)
				{
					const FVector F0 = FMath::Lerp(P1, P2, 0.4f);
					const FVector FDir = (Out * 0.8f + Up * 0.6f + Along * (S * 0.2f)).GetSafeNormal();
					Rod(M, { F0, F0 + FDir * (Tine.Fork * 0.6f), F0 + (FDir + Up * 0.5f).GetSafeNormal() * Tine.Fork }, 0.5f, 0.1f, AntlerDark, AntlerPale, 0.5f, float(Seed++));
				}
			}
		}
		// Ivy along the band, alternate leaves on the vine leaning up and along it.
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = 2.f * PI * (i + 0.5f) / 16.f;
			const float Side = i % 2 ? 1.f : -1.f;
			const FVector Out = B.Out(A);
			const FVector Dir = (B.Along(A) * (0.75f * Side) + Up * 0.45f + Out * 0.2f).GetSafeNormal();
			Leaf(M, B.At(A, 0.35f * Side) + Out * 1.0f, Dir, Out, 2.4f + 0.5f * (i % 3), 1.9f, i % 3 ? Ivy : IvyDark);
		}
		// The Hunt's banner rags, knotted on at the back of the band and hanging down behind the head.
		struct FRag { float Deg, Len, Width; FLinearColor C; };
		const FRag Rags[] = { { 160.f, 17.f, 3.2f, Lin(122, 44, 34) }, { 180.f, 22.f, 3.6f, Lin(64, 84, 50) }, { 200.f, 13.f, 3.f, Lin(168, 154, 128) } };
		for (int32 r = 0; r < 3; ++r)
		{
			const float A = FMath::DegreesToRadians(Rags[r].Deg);
			const FVector Out = B.Out(A);
			const FVector Top = B.At(A, -0.2f) + Out * 1.0f;
			TArray<FVector> Spine = { Top, Top + Out * 1.2f - Up * 3.f };
			TArray<float> Widths = { Rags[r].Width * 0.7f, Rags[r].Width };
			for (int32 k = 1; k <= 4; ++k)
			{
				// It hangs straight with a slight sway in the cloth (no two alike).
				Spine.Add(Spine.Last() - Up * ((Rags[r].Len - 3.f) / 4.f) + Out * (0.35f * FMath::Sin(1.7f * k + r)));
				Widths.Add(Rags[r].Width * (k < 4 ? 1.f - 0.06f * k : 0.6f));
			}
			Strip(M, Spine, Widths, B.Along(A), 0.18f, Rags[r].C, 0.95f);
			Ball(M, Top + Out * 0.2f, 0.85f, Rags[r].C * 0.8f, 0.95f);   // the knot
		}
	}

	/** The Heartwood circlet (F-S4, from the barred door's table): three thin strands of heartwood braided into a band,
	 *  polished by a hand long gone, a drop of amber resin set at the brow in a little cup of the strands, and a cord
	 *  binding at each temple where the braid was joined. */
	void HeartwoodCirclet(FWRToolMesh& M, const FBand& B)
	{
		const FLinearColor Wood[3] = { Lin(122, 62, 24), Lin(154, 84, 38), Lin(96, 46, 18) };
		const FVector Up = FVector::UpVector;
		for (int32 k = 0; k < 3; ++k)
		{
			TArray<FVector> Strand;
			for (int32 i = 0; i < 144; ++i)
			{
				const float A = 2.f * PI * i / 144.f, Turn = A * 16.f + k * 2.f * PI / 3.f;
				Strand.Add(B.At(A) + (B.Out(A) * FMath::Cos(Turn) + Up * FMath::Sin(Turn)) * 0.36f);
			}
			M.Tube(Strand, 0.3f, 6, Wood[k], 0.38f, true);
		}
		const FVector Out0 = B.Out(0.f);
		const FVector Bead = B.At(0.f, 0.15f) + Out0 * 0.95f;
		Ball(M, Bead, 0.85f, Lin(232, 150, 40), 0.12f);
		M.Tube(Loop(Bead - Out0 * 0.3f, Out0, 0.82f, 12), 0.17f, 5, Wood[2], 0.4f, true);
		for (const float S : { 1.f, -1.f })
		{
			TArray<FVector> Cord;
			for (int32 j = 0; j <= 40; ++j)
			{
				const float A = S * PI * 0.5f + (j / 40.f - 0.5f) * 0.24f, Turn = j / 40.f * 5.f * 2.f * PI;
				Cord.Add(B.At(A) + (B.Out(A) * FMath::Cos(Turn) + Up * FMath::Sin(Turn)) * 0.58f);
			}
			M.Tube(Cord, 0.11f, 4, Lin(58, 38, 22), 0.9f);
		}
	}

	/** Builds the piece by its id; false for one this file doesn't know. */
	bool Build(FName Id, const FBand& B, FWRToolMesh& M)
	{
		if (Id == TEXT("huntsmans_crown")) HuntsmansCrown(M, B);
		else if (Id == TEXT("heartwood_circlet")) HeartwoodCirclet(M, B);
		else return false;
		return true;
	}

	/** Where a piece sits in the head bone's frame: the band's centre Lift cm over the bone and Forward cm ahead of it,
	 *  upright and facing the way the body faces, worked out in the mesh's reference pose (the head is level there). */
	FTransform OnHead(const USkeletalMeshComponent* BodyComp, FName Bone, float Lift, float Forward)
	{
		const USkeletalMesh* Mesh = BodyComp ? BodyComp->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh) return FTransform::Identity;
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const int32 Index = Ref.FindBoneIndex(Bone);
		if (Index == INDEX_NONE) return FTransform::Identity;
		FTransform HeadCS = FTransform::Identity;
		for (int32 b = Index; b != INDEX_NONE; b = Ref.GetParentIndex(b)) HeadCS = HeadCS * Ref.GetRefBonePose()[b];
		// The actor's frame (X forward, Z up) seen from the body component (turned to fit the mesh's own facing).
		const FQuat Upright = BodyComp->GetRelativeRotation().Quaternion().Inverse();
		const FVector At = HeadCS.GetLocation() + Upright.RotateVector(FVector(Forward, 0.f, Lift));
		return FTransform(Upright, At).GetRelativeTransform(HeadCS);
	}
}

// ---------------------------------------------------------------------------------------------------- server: what's worn

void AWRCharacter::SetWornArmor(const FName (&ItemIds)[4], const FName (&InElements)[4])
{
	if (!HasAuthority())
	{
		return;
	}
	// Each of the four visual slots (head, chest, hands, legs) shows the look of the piece worn in it (hero.md 2.5,
	// cooks/build-crafting.md 2.4): a piece names its family's armorLooks key (hide_head looks as hide_armor), whose row
	// gives the look, its Cinderhide / Snowhide by element, and the Ascendant inlay.
	FWRHeroLook Next = HeroLook;
	Next.AscendantMask = 0;
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject> Map = Obj(Art, TEXT("armorLooks"));
	for (int32 i = 0; i < 4; ++i)
	{
		Next.Looks[i] = NAME_None;
		Next.Elements[i] = InElements[i];
		if (ItemIds[i].IsNone())
		{
			continue;
		}
		const FWRItemDef* Def = FWRItemRegistry::Get().Find(ItemIds[i]);
		const FString Key = Def && !Def->LookAs.IsNone() ? Def->LookAs.ToString() : ItemIds[i].ToString();
		FString LookId;
		if (const TSharedPtr<FJsonObject> Row = Obj(Map, Key))
		{
			// { "look": "hidewear", "byElement": { "fire": "cinderhide" }, "ascendant": true }
			Row->TryGetStringField(TEXT("look"), LookId);
			bool bAsc = false;
			Row->TryGetBoolField(TEXT("ascendant"), bAsc);
			if (bAsc) Next.AscendantMask |= uint8(1 << i);
			FString ByElement;
			if (!InElements[i].IsNone() && Obj(Row, TEXT("byElement")).IsValid() && Obj(Row, TEXT("byElement"))->TryGetStringField(InElements[i].ToString(), ByElement))
			{
				LookId = ByElement;
			}
		}
		else if (Map.IsValid())
		{
			Map->TryGetStringField(Key, LookId);
		}
		Next.Looks[i] = LookId.IsEmpty() ? NAME_None : FName(*LookId);
	}
	if (!(Next == HeroLook))
	{
		HeroLook = Next;
		OnRep_HeroLook();  // a listen server or standalone game draws its own copy now
	}
}

void AWRCharacter::SetHeroAppearance(uint8 InFace, uint8 InBuild)
{
	if (!HasAuthority())
	{
		return;
	}
	// Clamp to what the art has for this sex (a bad or stale ?Look= never picks a face that isn't there).
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject> Hero = Obj(Obj(Art, TEXT("hero")), BodyStyle == TEXT("f") ? TEXT("f") : TEXT("m"));
	const TArray<TSharedPtr<FJsonValue>>* Faces = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Builds = nullptr;
	const int32 NumFaces = Hero.IsValid() && Hero->TryGetArrayField(TEXT("faces"), Faces) ? Faces->Num() : 1;
	const int32 NumBuilds = Hero.IsValid() && Hero->TryGetArrayField(TEXT("builds"), Builds) ? Builds->Num() : 1;
	FWRHeroLook Next = HeroLook;
	Next.Face = uint8(FMath::Clamp<int32>(InFace, 0, FMath::Max(NumFaces - 1, 0)));
	Next.Build = uint8(FMath::Clamp<int32>(InBuild, 0, FMath::Max(NumBuilds - 1, 0)));
	if (!(Next == HeroLook))
	{
		HeroLook = Next;
		OnRep_HeroLook();
	}
}

void AWRCharacter::PreviewLook(FName InBodyStyle, uint8 InFace, uint8 InBuild, bool bHide)
{
	// Never in a shared world: this is the menu's private stage (its game is standalone until Host or Join).
	if (!HasAuthority() || GetNetMode() != NM_Standalone)
	{
		return;
	}
	HeroLook.bHideHelmet = bHide;
	if (BodyStyle != InBodyStyle)
	{
		BodyStyle = InBodyStyle;
		OnRep_BodyStyle();   // the other body: LoadBodyArt swaps it
	}
	SetHeroAppearance(InFace, InBuild);   // clamped to the art; rebuilds the body for a new face / build
	OnRep_HeroLook();                     // the helmet (and a body that's already right) applied now
}

void AWRCharacter::ServerSetHideHelmet_Implementation(bool bHide)
{
	if (AWRPlayerState* PS = GetPlayerState<AWRPlayerState>())
	{
		PS->bHideHelmet = bHide;
	}
	if (HeroLook.bHideHelmet != bHide)
	{
		HeroLook.bHideHelmet = bHide;
		OnRep_HeroLook();
	}
}

void AWRCharacter::OnRep_HeroLook()
{
	if (!HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;  // BeginPlay builds the body with whatever arrived first; the server never draws
	}
	if (!bBodyLoaded || !LoadedLook.SameBody(HeroLook))
	{
		bBodyLoaded = false;  // another face or build: the whole body is rebuilt
		LoadBodyArt();
		return;
	}
	ApplyOutfit(FWRTuning::LoadDataFile(TEXT("player_art.json")));
	ApplyPathMarks(true);
}

// ---------------------------------------------------------------------------------------------------- the outfit

void AWRCharacter::ApplyOutfit(const TSharedPtr<FJsonObject>& Art)
{
	if (!Art.IsValid() || !Body->GetSkeletalMeshAsset())
	{
		return;
	}
	const bool bFemale = BodyStyle == TEXT("f");
	const FString Sex = bFemale ? TEXT("f") : TEXT("m");
	const FString BodyKey = FString::Printf(TEXT("%s_%s"), bFemale ? TEXT("F") : TEXT("M"), BuildNames[FMath::Min<int32>(HeroLook.Build, 2)]);
	const TSharedPtr<FJsonObject> Looks = Obj(Art, TEXT("looks"));
	// Each visual slot shows its own piece's look (cooks/build-crafting.md 2.4); an empty slot wears the wraps' piece.
	FName SlotLookId[4];
	TSharedPtr<FJsonObject> SlotLook[4];
	double SlotTier[4] = { 0.0, 0.0, 0.0, 0.0 };
	for (int32 i = 0; i < 4; ++i)
	{
		SlotLookId[i] = HeroLook.Looks[i].IsNone() ? FName(TEXT("wraps")) : HeroLook.Looks[i];
		SlotLook[i] = Obj(Looks, SlotLookId[i].ToString());
		if (!SlotLook[i].IsValid())
		{
			UE_LOG(LogWildreach, Warning, TEXT("Player art: no look '%s', wearing the wraps there"), *SlotLookId[i].ToString());
			SlotLookId[i] = TEXT("wraps");
			SlotLook[i] = Obj(Looks, TEXT("wraps"));
		}
		if (SlotLook[i].IsValid())
		{
			SlotLook[i]->TryGetNumberField(TEXT("tier"), SlotTier[i]);
		}
	}
	LoadedOutfit = SlotLookId[1];   // the chest's: what the body reads as
	// Legacy (schema before hero-look-2): one outfit per sex under hero.<sex>.outfit.
	const TSharedPtr<FJsonObject> Legacy = Obj(Obj(Obj(Art, TEXT("hero")), Sex), TEXT("outfit"));
	// Each piece's element trim palette (hero.md 2.5), the main path's colour on the seams.
	const TSharedPtr<FJsonObject> Elements = Obj(Art, TEXT("elements"));
	const TSharedPtr<FJsonObject> MainRow = Obj(Obj(Art, TEXT("paths")), PathMarks.Main.ToString());
	const FLinearColor PathColor = Color(MainRow, TEXT("color"), FLinearColor(0.886f, 0.329f, 0.102f));
	double SeamGlow = 0.0, InlayNits = 0.0;
	if (const TSharedPtr<FJsonObject> Glow = Obj(Art, TEXT("outfitGlow")))
	{
		Glow->TryGetNumberField(TEXT("pathSeam"), SeamGlow);
		Glow->TryGetNumberField(TEXT("ascendantInlay"), InlayNits);
	}
	const float MainS = PathMarks.MainS / 255.f;

	OutfitMIDs.Reset();
	// The hair goes by the head piece's look; the hide-helmet option takes the Head slot off (tier 0 has nothing to hide).
	bool bHidesHair = false;
	if (SlotLook[0].IsValid())
	{
		SlotLook[0]->TryGetBoolField(TEXT("hidesHair"), bHidesHair);
	}
	const bool bHeadOff = HeroLook.bHideHelmet && SlotTier[0] > 0.0;
	for (int32 i = 0; i < Outfit.Num() && i < UE_ARRAY_COUNT(SlotNames); ++i)
	{
		const TSharedPtr<FJsonObject>& Look = SlotLook[i];
		const TSharedPtr<FJsonObject> Meshes = Obj(Obj(Look, TEXT("meshes")), BodyKey);
		FString PiecePath;
		if (Meshes.IsValid())
		{
			Meshes->TryGetStringField(SlotNames[i], PiecePath);
		}
		else if (Legacy.IsValid() && SlotLookId[i] == TEXT("wraps"))
		{
			Legacy->TryGetStringField(SlotNames[i], PiecePath);
		}
		if (i == 0 && bHeadOff)
		{
			PiecePath.Reset();
		}
		USkeletalMesh* Piece = PiecePath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *PiecePath);
		if (!PiecePath.IsEmpty() && !Piece)
		{
			UE_LOG(LogWildreach, Warning, TEXT("Player art: look %s piece %s not found"), *SlotLookId[i].ToString(), *PiecePath);
		}
		Outfit[i]->EmptyOverrideMaterials();
		Outfit[i]->SetSkeletalMesh(Piece);
		// A look that is another look's meshes in other materials (Cinderhide, Snowhide: hero.md 2.5) swaps by slot name.
		if (Piece)
		{
			if (const TSharedPtr<FJsonObject> Swaps = Obj(Look, TEXT("materials")))
			{
				for (const auto& Swap : Swaps->Values)
				{
					const int32 Index = Outfit[i]->GetMaterialIndex(FName(*Swap.Key));
					UMaterialInterface* Mat = Index == INDEX_NONE ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Swap.Value->AsString());
					if (Mat)
					{
						Outfit[i]->SetMaterial(Index, Mat);
					}
				}
			}
		}
		Outfit[i]->SetLeaderPoseComponent(Piece ? Body.Get() : nullptr);
		Outfit[i]->SetVisibility(Piece != nullptr);
		if (!Piece)
		{
			continue;
		}
		const FName El = HeroLook.Elements[i];
		const bool bElement = !El.IsNone() && Elements.IsValid() && Elements->HasField(El.ToString());
		const FLinearColor Trim = bElement ? Color(Elements, *El.ToString(), FLinearColor::White) : FLinearColor::White;
		const bool bAscendant = (HeroLook.AscendantMask & (1 << i)) != 0;
		for (int32 M = 0; M < Outfit[i]->GetNumMaterials(); ++M)
		{
			UMaterialInstanceDynamic* MID = Outfit[i]->CreateDynamicMaterialInstance(M);
			if (!MID)
			{
				continue;
			}
			if (bElement)
			{
				MID->SetVectorParameterValue(TEXT("TrimColor"), Trim);
			}
			MID->SetVectorParameterValue(TEXT("PathTint"), PathColor);
			MID->SetScalarParameterValue(TEXT("PathGlow"), float(SeamGlow) * MainS);
			MID->SetVectorParameterValue(TEXT("InlayColor"), PathColor);
			MID->SetScalarParameterValue(TEXT("InlayNits"), bAscendant ? float(InlayNits) : 0.f);
			OutfitMIDs.Add(MID);
		}
	}
	// The hair goes under helms and raised hoods (brows and lashes stay).
	bHairHiddenByHelm = bHidesHair && !bHeadOff;
	if (Grooms.Num() > 0)
	{
		Grooms[0]->SetVisibility(Grooms[0]->GroomAsset != nullptr && !bHairHiddenByHelm);
	}
	// Ascendant halo-crest (hero.md 2.5 tier 6) behind the head: an Ascendant helm wears it.
	USkeletalMesh* Crest = nullptr;
	if ((HeroLook.AscendantMask & 1) != 0 && !bHeadOff)
	{
		FString CrestPath;
		if (const TSharedPtr<FJsonObject> Crests = Obj(Art, TEXT("ascendantCrest")))
		{
			Crests->TryGetStringField(BodyKey, CrestPath);
		}
		Crest = CrestPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *CrestPath);
	}
	HaloCrest->SetSkeletalMesh(Crest);
	HaloCrest->SetLeaderPoseComponent(Crest ? Body.Get() : nullptr);
	HaloCrest->SetVisibility(Crest != nullptr);
	if (Crest)
	{
		if (UMaterialInstanceDynamic* MID = HaloCrest->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("InlayColor"), PathColor);
			MID->SetScalarParameterValue(TEXT("InlayNits"), float(InlayNits));
		}
	}

	// A code-modelled head piece (the look row's "headPiece": the Huntsman's crown, the Heartwood circlet) sat on the head
	// bone at real scale over the hair, its band sized and placed by the row's "place" (a woman's head by "placeF").
	// Hide-helmet takes it off like a helm; first person hides it from its wearer (ApplyViewMode), its shadow stays.
	{
		FString PieceId;
		if (SlotLook[0].IsValid() && !bHeadOff)
		{
			SlotLook[0]->TryGetStringField(TEXT("headPiece"), PieceId);
		}
		const FName HeadBoneName(TEXT("head"));
		const bool bPiece = !PieceId.IsEmpty() && Body->DoesSocketExist(HeadBoneName);
		if (bPiece)
		{
			const TSharedPtr<FJsonObject> Place = Obj(SlotLook[0], bFemale && Obj(SlotLook[0], TEXT("placeF")).IsValid() ? TEXT("placeF") : TEXT("place"));
			double Rx = 11.0, Ry = 9.2, Tilt = 9.0, Lift = 10.0, Forward = 1.5;
			if (Place.IsValid())
			{
				Place->TryGetNumberField(TEXT("rx"), Rx);
				Place->TryGetNumberField(TEXT("ry"), Ry);
				Place->TryGetNumberField(TEXT("tilt"), Tilt);
				Place->TryGetNumberField(TEXT("lift"), Lift);
				Place->TryGetNumberField(TEXT("forward"), Forward);
			}
			// Rebuilt only when the piece (or the head it's sized for) changes.
			const FName PieceKey(*FString::Printf(TEXT("%s_%s"), *PieceId, bFemale ? TEXT("f") : TEXT("m")));
			bool bReady = HeadPiece != nullptr && HeadPieceShown == PieceKey;
			if (!bReady)
			{
				WRHeroHeadPieceImpl::FBand Band;
				Band.Rx = float(Rx);
				Band.Ry = float(Ry);
				Band.Tilt = float(Tilt);
				FWRToolMesh PieceMesh;
				if (WRHeroHeadPieceImpl::Build(FName(*PieceId), Band, PieceMesh))
				{
					if (!HeadPiece)
					{
						HeadPiece = NewObject<UProceduralMeshComponent>(this, TEXT("HeadPiece"));
						HeadPiece->SetupAttachment(Body, HeadBoneName);
						HeadPiece->SetCanEverAffectNavigation(false);
						HeadPiece->bCastHiddenShadow = true;
						HeadPiece->RegisterComponent();
					}
					PieceMesh.ApplyTo(HeadPiece);
					HeadPieceShown = PieceKey;
					bReady = true;
				}
				else
				{
					UE_LOG(LogWildreach, Warning, TEXT("Player art: no head piece '%s' to model"), *PieceId);
				}
			}
			if (bReady)
			{
				HeadPiece->SetRelativeTransform(WRHeroHeadPieceImpl::OnHead(Body, HeadBoneName, float(Lift), float(Forward)));
				HeadPiece->SetOwnerNoSee(bFirstPerson);
				HeadPiece->SetVisibility(true);
			}
			else if (HeadPiece)
			{
				HeadPiece->SetVisibility(false);
			}
		}
		else if (HeadPiece)
		{
			HeadPiece->SetVisibility(false);
		}
	}

	// Body-hide masks (cook hero-look-2 3.6): the skin under each worn piece is clipped by that piece's own look, so
	// cloth never shows skin through it in motion. White = keep; the body material multiplies the four.
	if (SkinMIDs.Num() > 0 && SkinMIDs[0])
	{
		// "keep everything": a white linear-greyscale texture (the material's mask samplers are linear greyscale)
		FString WhitePath;
		Art->TryGetStringField(TEXT("hideWhite"), WhitePath);
		UTexture* White = WhitePath.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *WhitePath);
		for (int32 i = 0; i < UE_ARRAY_COUNT(SlotNames); ++i)
		{
			const TSharedPtr<FJsonObject> Hide = Obj(Obj(SlotLook[i], TEXT("hide")), Sex);
			FString MaskPath;
			if (Hide.IsValid() && !(i == 0 && bHeadOff) && Outfit.IsValidIndex(i) && Outfit[i]->IsVisible())
			{
				Hide->TryGetStringField(SlotNames[i], MaskPath);
			}
			UTexture* Mask = MaskPath.IsEmpty() ? nullptr : LoadObject<UTexture>(nullptr, *MaskPath);
			if (Mask || White)
			{
				SkinMIDs[0]->SetTextureParameterValue(FName(*(FString(TEXT("Hide")) + SlotNames[i])), Mask ? Mask : White);
			}
		}
	}
	RefreshLODSync();
	// The endgame's look over the fresh outfit: an armour dye, a raid mark (WREndgameLook).
	ApplyDeedsLook();
}

void AWRCharacter::RefreshLODSync()
{
	USkeletalMesh* FaceAsset = Face->GetSkeletalMeshAsset();
	USkeletalMesh* BodyAsset = Body->GetSkeletalMeshAsset();
	if (!BodyAsset)
	{
		return;
	}
	// One LOD for the whole hero, led by the face (it has the most levels): the body, outfit and hair follow.
	LODSync->ComponentsToSync.Reset();
	LODSync->CustomLODMapping.Reset();
	LODSync->NumLODs = FaceAsset ? FaceAsset->GetLODNum() : BodyAsset->GetLODNum();
	LODSync->ComponentsToSync.Add({ Face->GetFName(), FaceAsset ? ESyncOption::Drive : ESyncOption::Disabled });
	LODSync->ComponentsToSync.Add({ Body->GetFName(), FaceAsset ? ESyncOption::Passive : ESyncOption::Drive });
	const auto MapTo = [this](int32 Levels)
	{
		FLODMappingData Map;
		for (int32 L = 0; L < LODSync->NumLODs; ++L)
		{
			Map.Mapping.Add(FMath::Min(Levels - 1, L * Levels / FMath::Max(LODSync->NumLODs, 1)));
		}
		return Map;
	};
	if (FaceAsset)
	{
		LODSync->CustomLODMapping.Add(Body->GetFName(), MapTo(BodyAsset->GetLODNum()));
	}
	TArray<USkeletalMeshComponent*> Pieces;
	for (USkeletalMeshComponent* Piece : Outfit)
	{
		Pieces.Add(Piece);
	}
	Pieces.Add(HaloCrest);
	for (USkeletalMeshComponent* Piece : Pieces)
	{
		if (Piece->GetSkeletalMeshAsset())
		{
			LODSync->ComponentsToSync.Add({ Piece->GetFName(), ESyncOption::Passive });
			LODSync->CustomLODMapping.Add(Piece->GetFName(), MapTo(Piece->GetSkeletalMeshAsset()->GetLODNum()));
		}
	}
	for (UGroomComponent* Groom : Grooms)
	{
		if (Groom->GroomAsset)
		{
			LODSync->ComponentsToSync.Add({ Groom->GetFName(), ESyncOption::Passive });
		}
	}
	LODSync->RefreshSyncComponents();
}

// ---------------------------------------------------------------------------------------------------- path marks

void AWRCharacter::BuildLookMaterials()
{
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	SkinMIDs.Reset();
	EyeMIDs.Reset();
	HairMIDs.Reset();
	EyeDefaults.Reset();
	EyeDefaultColors.Reset();
	PathMarkSig = 0;
	// The body's skin first (index 0 carries the hide masks), then the head's skin LODs, its eyes.
	for (int32 M = 0; M < Body->GetNumMaterials(); ++M)
	{
		if (UMaterialInstanceDynamic* MID = Body->CreateDynamicMaterialInstance(M))
		{
			SkinMIDs.Add(MID);
		}
	}
	if (Face->GetSkeletalMeshAsset())
	{
		const TArray<FName> Slots = Face->GetMaterialSlotNames();
		for (int32 M = 0; M < Slots.Num(); ++M)
		{
			const bool bSkin = SlotIs(Art, TEXT("skin"), Slots[M]);
			const bool bEye = SlotIs(Art, TEXT("eyes"), Slots[M]);
			if (!bSkin && !bEye)
			{
				continue;
			}
			UMaterialInstanceDynamic* MID = Face->CreateDynamicMaterialInstance(M);
			if (MID)
			{
				(bSkin ? SkinMIDs : EyeMIDs).Add(MID);
			}
		}
	}
	for (UGroomComponent* Groom : Grooms)
	{
		if (!Groom->GroomAsset || Groom->GetName() == TEXT("Eyelashes"))
		{
			continue;
		}
		for (int32 M = 0; M < Groom->GetNumMaterials(); ++M)
		{
			if (UMaterialInstanceDynamic* MID = Groom->CreateDynamicMaterialInstance(M))
			{
				HairMIDs.Add(MID);
			}
		}
	}
	// The face's hair colour (the grooms are the engine's shared ones; the colour is the face's).
	for (UMaterialInstanceDynamic* MID : HairMIDs)
	{
		for (const TPair<FName, float>& P : FaceHairParams)
		{
			MID->SetScalarParameterValue(P.Key, P.Value);
		}
	}
	// Remember the eye and hair materials' own values for every parameter a path row touches, so marks blend from them.
	const TSharedPtr<FJsonObject> Paths = Obj(Art, TEXT("paths"));
	if (!Paths.IsValid())
	{
		return;
	}
	for (const auto& Pair : Paths->Values)
	{
		const TSharedPtr<FJsonObject> Row = Pair.Value->AsObject();
		for (const TCHAR* Part : { TEXT("eye"), TEXT("eyeSub"), TEXT("hair") })
		{
			const TSharedPtr<FJsonObject> Block = Obj(Row, Part);
			const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs = FCString::Strcmp(Part, TEXT("hair")) == 0 ? HairMIDs : EyeMIDs;
			if (!Block.IsValid() || MIDs.Num() == 0)
			{
				continue;
			}
			if (const TSharedPtr<FJsonObject> Scalars = Obj(Block, TEXT("scalar")))
			{
				for (const auto& S : Scalars->Values)
				{
					const FName Key(*(FString(Part) + TEXT(":") + S.Key));
					float V = 0.f;
					if (!EyeDefaults.Contains(Key) && MIDs[0]->GetScalarParameterValue(FMaterialParameterInfo(FName(*S.Key)), V))
					{
						EyeDefaults.Add(Key, V);
					}
				}
			}
			if (const TSharedPtr<FJsonObject> Vectors = Obj(Block, TEXT("vector")))
			{
				for (const auto& S : Vectors->Values)
				{
					const FName Key(*(FString(Part) + TEXT(":") + S.Key));
					FLinearColor V;
					if (!EyeDefaultColors.Contains(Key) && MIDs[0]->GetVectorParameterValue(FMaterialParameterInfo(FName(*S.Key)), V))
					{
						EyeDefaultColors.Add(Key, V);
					}
				}
			}
		}
	}
}

void AWRCharacter::OnPathAbilityUsed(FName PathId)
{
	if (HasAuthority() && !PathId.IsNone())
	{
		MulticastPathPulse(PathId);
	}
}

void AWRCharacter::MulticastPathPulse_Implementation(FName PathId)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const TSharedPtr<FJsonObject> Row = Obj(Obj(FWRTuning::LoadDataFile(TEXT("player_art.json")), TEXT("paths")), PathId.ToString());
	double Seconds = 1.5;
	FString Sound;
	if (Row.IsValid())
	{
		Row->TryGetNumberField(TEXT("pulseSeconds"), Seconds);
		Row->TryGetStringField(TEXT("castSound"), Sound);
	}
	PulsePath = PathId;
	PulseSeconds = float(FMath::Max(Seconds, 0.05));
	PulseUntil = GetWorld()->GetTimeSeconds() + PulseSeconds;
	if (!Sound.IsEmpty())
	{
		WRSound::PlayOn(this, FName(*Sound));  // Lightning's static crackle (hero.md 2.6)
	}
	PathMarkSig = 0;
}

void AWRCharacter::StrikePreTell(float Seconds)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	StrikeTellUntil = GetWorld()->GetTimeSeconds() + Seconds;
	PathMarkSig = 0;
	FTimerHandle H;
	TWeakObjectPtr<AWRCharacter> Self(this);
	GetWorldTimerManager().SetTimer(H, [Self]() { if (Self.IsValid()) Self->PathMarkSig = 0; }, Seconds + 0.05f, false);
}

void AWRCharacter::TickPathMarks(float DeltaSeconds)
{
	// Server: what every machine draws (the path progress itself only replicates to its owner).
	if (HasAuthority())
	{
		PathMarkCheck -= DeltaSeconds;
		if (PathMarkCheck <= 0.f)
		{
			PathMarkCheck = 1.f;
			FWRPathMarks Next = bNpcMarks ? NpcMarks : FWRPathMarks();
			if (const AWRPlayerState* PS = bNpcMarks ? nullptr : GetPlayerState<AWRPlayerState>())
			{
				if (const UWRPathComponent* P = PS->GetPaths())
				{
					Next.Main = P->GetMainPath();
					Next.Sub = P->GetSubPath();
					const FWRPathProgress* MP = Next.Main.IsNone() ? nullptr : P->GetProgress(Next.Main);
					const FWRPathProgress* SP = Next.Sub.IsNone() ? nullptr : P->GetProgress(Next.Sub);
					Next.MainS = uint8(FMath::RoundToInt(255.f * (MP ? Strength(MP->XP) : 0.f)));
					Next.SubS = uint8(FMath::RoundToInt(255.f * (SP ? Strength(SP->XP) : 0.f)));
				}
			}
			if (!(Next == PathMarks))
			{
				PathMarks = Next;
			}
		}
	}
	if (GetNetMode() != NM_DedicatedServer && bBodyLoaded)
	{
		ApplyPathMarks(false);
	}
}

void AWRCharacter::ApplyPathMarks(bool bForce)
{
	if (SkinMIDs.Num() == 0 && EyeMIDs.Num() == 0 && HairMIDs.Num() == 0)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float PulseLeft = PulseUntil > Now ? float((PulseUntil - Now) / PulseSeconds) : 0.f;
	const bool bWet = GetCharacterMovement() && GetCharacterMovement()->IsSwimming();
	uint32 Sig = HashCombine(GetTypeHash(PathMarks.Main), GetTypeHash(PathMarks.Sub));
	Sig = HashCombine(Sig, (uint32(PathMarks.MainS) << 8) | PathMarks.SubS);
	Sig = HashCombine(Sig, (uint32(FMath::RoundToInt(PulseLeft * 20.f)) << 1) | (bWet ? 1u : 0u));
	Sig = HashCombine(Sig, GetTypeHash(PulsePath));
	const bool bStrikeTell = StrikeTellUntil > Now;
	Sig = HashCombine(Sig, bStrikeTell ? 0x5717u : 0u);
	Sig = HashCombine(Sig, WREndgameLook::LookSig(this));   // prestige gold threads, a raid mark on the seams
	Sig = HashCombine(Sig, WRBlood::LookSig(this));         // Blood: veins with Spilled, Sanguine lord, the flush, the pulse
	Sig = HashCombine(Sig, WRVoidPath::LookSig(this));      // Void: the bar and the band move the greying within its stage
	Sig = HashCombine(Sig, WRNature::LookSig(this));        // Nature: Rooted's green tint on the bark while it heals
	Sig = HashCombine(Sig, WRWind::LookSig(this));          // Wind: the spirals race and the hair streams at high Momentum in a fight
	Sig = HashCombine(Sig, WRNecro::LookSig(this));         // Necromancy: Lich form's full ash, a raise's pupil flare
	Sig = HashCombine(Sig, WRArcane::LookSig(this));        // Arcane: a shield's facets lift the rune lines
	Sig = HashCombine(Sig, WRTidePath::LookSig(this));
	Sig = HashCombine(Sig, WRSun::LookSig(this));           // Sun: the flecks' stored glow at night by Radiance, Zenith
	Sig = HashCombine(Sig, WRSporePath::LookSig(this));     // Spore: the freckles glow in the dark, blaze in Living colony      // Tide: Flow brightens the sheen; the glow under water
	if (!bForce && Sig == PathMarkSig)
	{
		return;
	}
	PathMarkSig = Sig;
	const TSharedPtr<FJsonObject> Art = FWRTuning::LoadDataFile(TEXT("player_art.json"));
	const TSharedPtr<FJsonObject> Paths = Obj(Art, TEXT("paths"));
	const float MainS = PathMarks.MainS / 255.f;
	const float SubS = (PathMarks.SubS / 255.f) * float(WRTune(TEXT("pathMarks.subScale"), 0.6f));

	// Skin: MF_PathMarks takes Main* and Sub* parameters (prefix + the row's "skin" keys).
	const auto SkinRow = [&](const FString& Prefix, FName PathId, float S)
	{
		const TSharedPtr<FJsonObject> Row = Obj(Paths, PathId.ToString());
		const TSharedPtr<FJsonObject> Skin = Obj(Row, TEXT("skin"));
		for (UMaterialInstanceDynamic* MID : SkinMIDs)
		{
			MID->SetScalarParameterValue(FName(*(Prefix + TEXT("Strength"))), Skin.IsValid() ? S : 0.f);
			if (!Skin.IsValid())
			{
				continue;
			}
			// Prestige: each mark draws a gold thread into the path's marks (cooks/build-endgame.md 2.1; gold is yours).
			MID->SetVectorParameterValue(FName(*(Prefix + TEXT("Color"))), FMath::Lerp(Color(Row, TEXT("color"), FLinearColor::White), WREndgameLook::GoldColour(), WREndgameLook::Gold(this, PathId)));
			for (const auto& Pair : Skin->Values)
			{
				const FName Name(*(Prefix + Pair.Key));
				if (Pair.Value->Type == EJson::Number)
				{
					MID->SetScalarParameterValue(Name, float(Pair.Value->AsNumber()));
				}
				else if (Pair.Value->Type == EJson::Array)
				{
					MID->SetVectorParameterValue(Name, Color(Skin, *Pair.Key, FLinearColor::Black));
				}
				else if (Pair.Value->Type == EJson::String)
				{
					if (UTexture* Tex = LoadObject<UTexture>(nullptr, *Pair.Value->AsString()))
					{
						MID->SetTextureParameterValue(Name, Tex);
					}
				}
			}
		}
	};
	SkinRow(TEXT("Main"), PathMarks.Main, MainS);
	SkinRow(TEXT("Sub"), PathMarks.Sub, SubS);
	// Void greys with corruption (hero.md 2.6); until corruption is a stat, it follows the path's strength.
	bool bByCorruption = false;
	if (const TSharedPtr<FJsonObject> Row = Obj(Paths, PathMarks.Main.ToString()))
	{
		Row->TryGetBoolField(TEXT("byCorruption"), bByCorruption);
	}
	const float Pulse = PulsePath == PathMarks.Main || PulsePath == PathMarks.Sub ? PulseLeft : 0.f;
	for (UMaterialInstanceDynamic* MID : SkinMIDs)
	{
		MID->SetScalarParameterValue(TEXT("Pulse"), Pulse);
		MID->SetScalarParameterValue(TEXT("MainByCorruption"), bByCorruption ? 1.f : 0.f);
		MID->SetScalarParameterValue(TEXT("Corruption"), MainS);
		MID->SetScalarParameterValue(TEXT("Underwater"), bWet ? 1.f : 0.f);
	}

	// Eyes and hair: each parameter blends from the material's own value toward the row's by the path's strength. The
	// main path drives the iris and the hair; the sub path's "eyeSub" block (the limbal ring) at half its strength.
	const auto Blend = [&](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs, const TCHAR* Part, FName PathId, float S)
	{
		const TSharedPtr<FJsonObject> Row = Obj(Paths, PathId.ToString());
		const TSharedPtr<FJsonObject> Block = Obj(Row, Part);
		if (!Block.IsValid())
		{
			return;
		}
		const float Boost = PathId == PulsePath ? 1.f + Pulse : 1.f;
		if (const TSharedPtr<FJsonObject> Scalars = Obj(Block, TEXT("scalar")))
		{
			for (const auto& P : Scalars->Values)
			{
				const float* Def = EyeDefaults.Find(FName(*(FString(Part) + TEXT(":") + P.Key)));
				const float V = FMath::Lerp(Def ? *Def : 0.f, float(P.Value->AsNumber()) * Boost, S);
				for (UMaterialInstanceDynamic* MID : MIDs)
				{
					MID->SetScalarParameterValue(FName(*P.Key), V);
				}
			}
		}
		if (const TSharedPtr<FJsonObject> Vectors = Obj(Block, TEXT("vector")))
		{
			for (const auto& P : Vectors->Values)
			{
				const FLinearColor* Def = EyeDefaultColors.Find(FName(*(FString(Part) + TEXT(":") + P.Key)));
				const FLinearColor V = FMath::Lerp(Def ? *Def : FLinearColor::Black, Color(Vectors, *P.Key, FLinearColor::White), S);
				for (UMaterialInstanceDynamic* MID : MIDs)
				{
					MID->SetVectorParameterValue(FName(*P.Key), V);
				}
			}
		}
	};
	// Reset everything a row may have touched to the material's own value, then blend in the current paths.
	for (const auto& Pair : EyeDefaults)
	{
		FString Part, Param;
		Pair.Key.ToString().Split(TEXT(":"), &Part, &Param);
		for (UMaterialInstanceDynamic* MID : Part == TEXT("hair") ? HairMIDs : EyeMIDs)
		{
			MID->SetScalarParameterValue(FName(*Param), Pair.Value);
		}
	}
	for (const auto& Pair : EyeDefaultColors)
	{
		FString Part, Param;
		Pair.Key.ToString().Split(TEXT(":"), &Part, &Param);
		for (UMaterialInstanceDynamic* MID : Part == TEXT("hair") ? HairMIDs : EyeMIDs)
		{
			MID->SetVectorParameterValue(FName(*Param), Pair.Value);
		}
	}
	Blend(EyeMIDs, TEXT("eyeSub"), PathMarks.Sub, SubS * 0.5f);
	Blend(EyeMIDs, TEXT("eye"), PathMarks.Main, MainS);
	Blend(HairMIDs, TEXT("hair"), PathMarks.Main, MainS);
	for (UMaterialInstanceDynamic* MID : HairMIDs)
	{
		MID->SetScalarParameterValue(TEXT("PathPulse"), bStrikeTell ? 1.f : Pulse);
		// The storm's pre-tell lifts anyone's hair the full Lightning amount (the same channel a Lightning player
		// reads on their own body, so they know it at once).
		if (bStrikeTell) MID->SetScalarParameterValue(TEXT("PathLift"), WRTune(TEXT("storm.preTellHairLift"), 1.5f));
	}
	// Blood & curse (cooks/biome-swamp.md 2.18): its stages and live parts over what the row drew (WRBloodPath.cpp).
	WRBlood::ApplyLook(this, SkinMIDs, EyeMIDs, EyeDefaultColors, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	WRVoidPath::ApplyLook(this, SkinMIDs, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	// Nature (cooks/biome-forest.md 2.18): the bark's stages, the iris going leaf-green, the sun-warm skin, the mixed looks
	// with Fire / Frost / Blood, and Rooted's green tint while it heals (WRNaturePath.cpp). After Blood's, so its garnet
	// threads can follow the bark.
	WRNature::ApplyLook(this, SkinMIDs, EyeMIDs, EyeDefaultColors, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	// Wind (cooks/biome-sky.md 2.18.5): the row's drifting spirals and stirring hair by strength; in a fight at >= 60
	// Momentum the drift x4, +1 nit, the hair streaming (WRWindPath.cpp).
	WRWind::ApplyLook(this, SkinMIDs, HairMIDs, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	// Necromancy (cooks/biome-graveyard.md 2.18): the stages are the player_art row; Lich form shows the full ash at any
	// point count and a raise flares the pupil ring (WRNecroPath.cpp).
	WRNecro::ApplyLook(this, SkinMIDs, EyeMIDs, EyeDefaultColors, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	// Arcane (cooks/biome-crystal.md 2.18): the stages are the player_art row; a shield holding lifts the lines to their
	// resting glow (WRArcanePath.cpp).
	WRArcane::ApplyLook(this, SkinMIDs, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	WRTidePath::ApplyLook(this, SkinMIDs, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	// Sun (cooks/biome-desert.md 2.18): the stages are the player_art row; the flecks glow at night by Radiance (WRSunPath.cpp).
	WRSun::ApplyLook(this, SkinMIDs, PathMarks.Main, MainS, PathMarks.Sub, SubS);
	WRSporePath::ApplyLook(this, SkinMIDs, PathMarks.Main, MainS, PathMarks.Sub, SubS);

	// Blood: sharper canines from strength 0.6 (main, or the sub path's share).
	bool bFangs = false;
	FString FangPath;
	for (const TPair<FName, float>& P : { TPair<FName, float>(PathMarks.Main, MainS), TPair<FName, float>(PathMarks.Sub, SubS) })
	{
		double From = 0.0;
		const TSharedPtr<FJsonObject> Row = Obj(Paths, P.Key.ToString());
		if (Row.IsValid() && Row->TryGetNumberField(TEXT("caninesFrom"), From) && P.Value >= From)
		{
			bFangs = true;
			Row->TryGetStringField(TEXT("canineMesh"), FangPath);
		}
	}
	UStaticMesh* Fangs = bFangs && !FangPath.IsEmpty() ? LoadObject<UStaticMesh>(nullptr, *FangPath) : nullptr;
	if (Canines->GetStaticMesh() != Fangs)
	{
		Canines->SetStaticMesh(Fangs);
	}
	Canines->SetVisibility(Fangs != nullptr && Face->IsVisible());
	// Nature: the small leaf buds at the temples from strength 1.0 (cooks/biome-forest.md 2.18), hidden with the face.
	WRNature::ApplyTempleBuds(this, Body, Face->IsVisible(), PathMarks.Main, MainS);

	// The worn armour's seams carry the main path's colour (hero.md 2.5).
	if (OutfitMIDs.Num() > 0)
	{
		double SeamGlow = 0.0;
		if (const TSharedPtr<FJsonObject> Glow = Obj(Art, TEXT("outfitGlow")))
		{
			Glow->TryGetNumberField(TEXT("pathSeam"), SeamGlow);
		}
		FLinearColor PathColor = Color(Obj(Paths, PathMarks.Main.ToString()), TEXT("color"), FLinearColor(0.886f, 0.329f, 0.102f));
		float SeamNits = float(SeamGlow) * MainS * (1.f + Pulse);
		// A raid mark on the seams (the Smith's molten iron) runs in them instead of the path's colour.
		FLinearColor MarkColour;
		float MarkGlow = 0.f;
		if (WREndgameLook::SeamMark(this, MarkColour, MarkGlow)) { PathColor = MarkColour; SeamNits = FMath::Max(SeamNits, MarkGlow * (1.f + 0.5f * Pulse)); }
		for (UMaterialInstanceDynamic* MID : OutfitMIDs)
		{
			MID->SetVectorParameterValue(TEXT("PathTint"), PathColor);
			MID->SetScalarParameterValue(TEXT("PathGlow"), SeamNits);
			MID->SetScalarParameterValue(TEXT("SeamPulse"), Pulse);
		}
	}
}
