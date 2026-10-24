#include "WRSocial.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCharacterBase.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- clans

static FString ClansPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ServerDB"), TEXT("clans.json"));
}

FWRClans& FWRClans::Get()
{
	static FWRClans Instance;
	return Instance;
}

void FWRClans::Load()
{
	Clans.Reset();
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *ClansPath()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Root->Values)
	{
		const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
		if (!O.IsValid()) continue;
		FClan C;
		C.Leader = O->GetStringField(TEXT("leader"));
		O->TryGetStringArrayField(TEXT("members"), C.Members);
		O->TryGetStringArrayField(TEXT("invites"), C.Invites);
		Clans.Add(KV.Key, C);
	}
}

void FWRClans::Save() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	for (const TPair<FString, FClan>& KV : Clans)
	{
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("leader"), KV.Value.Leader);
		TArray<TSharedPtr<FJsonValue>> M, I;
		for (const FString& S : KV.Value.Members) M.Add(MakeShared<FJsonValueString>(S));
		for (const FString& S : KV.Value.Invites) I.Add(MakeShared<FJsonValueString>(S));
		O->SetArrayField(TEXT("members"), M);
		O->SetArrayField(TEXT("invites"), I);
		Root->SetObjectField(KV.Key, O);
	}
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	FFileHelper::SaveStringToFile(Text, *ClansPath());
}

FString FWRClans::ClanOf(const FString& Player) const
{
	for (const TPair<FString, FClan>& KV : Clans)
	{
		if (KV.Value.Members.Contains(Player)) return KV.Key;
	}
	return FString();
}

bool FWRClans::SameClan(const FString& A, const FString& B) const
{
	const FString C = ClanOf(A);
	return !C.IsEmpty() && C == ClanOf(B);
}

TArray<FString> FWRClans::Members(const FString& Clan) const
{
	const FClan* C = Clans.Find(Clan);
	return C ? C->Members : TArray<FString>();
}

FString FWRClans::Create(const FString& Player, const FString& Clan)
{
	if (Clan.IsEmpty() || Clan.Len() > 24) return TEXT("Clan names are 1-24 characters");
	if (!ClanOf(Player).IsEmpty()) return TEXT("Leave your clan first");
	if (Clans.Contains(Clan)) return TEXT("That clan name is taken");
	FClan C;
	C.Leader = Player;
	C.Members.Add(Player);
	Clans.Add(Clan, C);
	Save();
	UE_LOG(LogWildreach, Log, TEXT("CLAN %s created %s"), *Player, *Clan);
	return FString::Printf(TEXT("Founded clan %s"), *Clan);
}

FString FWRClans::Invite(const FString& Player, const FString& Target)
{
	FClan* C = Clans.Find(ClanOf(Player));
	if (!C) return TEXT("You're not in a clan");
	if (C->Leader != Player) return TEXT("Only the clan leader can invite");
	const int32 Limit = FMath::RoundToInt(WRTune(TEXT("server.clanSizeLimit"), 0.f));
	if (Limit > 0 && C->Members.Num() >= Limit) return FString::Printf(TEXT("Clan is full (%d)"), Limit);
	C->Invites.AddUnique(Target);
	Save();
	return FString::Printf(TEXT("Invited %s (they type WRClan join %s)"), *Target, *ClanOf(Player));
}

FString FWRClans::Join(const FString& Player, const FString& Clan)
{
	FClan* C = Clans.Find(Clan);
	if (!C || !C->Invites.Contains(Player)) return TEXT("You need an invite to that clan");
	if (!ClanOf(Player).IsEmpty()) return TEXT("Leave your clan first");
	const int32 Limit = FMath::RoundToInt(WRTune(TEXT("server.clanSizeLimit"), 0.f));
	if (Limit > 0 && C->Members.Num() >= Limit) return TEXT("That clan is full");
	C->Invites.Remove(Player);
	C->Members.Add(Player);
	Save();
	UE_LOG(LogWildreach, Log, TEXT("CLAN %s joined %s"), *Player, *Clan);
	return FString::Printf(TEXT("Joined clan %s"), *Clan);
}

FString FWRClans::Leave(const FString& Player)
{
	const FString Name = ClanOf(Player);
	FClan* C = Clans.Find(Name);
	if (!C) return TEXT("You're not in a clan");
	C->Members.Remove(Player);
	if (C->Members.Num() == 0)
	{
		Clans.Remove(Name);
	}
	else if (C->Leader == Player)
	{
		C->Leader = C->Members[0];
	}
	Save();
	return FString::Printf(TEXT("Left clan %s"), *Name);
}

FString FWRClans::Kick(const FString& Player, const FString& Target)
{
	FClan* C = Clans.Find(ClanOf(Player));
	if (!C || C->Leader != Player) return TEXT("Only the clan leader can kick");
	if (Target == Player || !C->Members.Remove(Target)) return TEXT("Not a member");
	Save();
	return FString::Printf(TEXT("Kicked %s"), *Target);
}

FString FWRClans::Describe(const FString& Player) const
{
	const FString Name = ClanOf(Player);
	const FClan* C = Clans.Find(Name);
	if (!C) return TEXT("No clan. WRClan create <name>, or WRClan join <name> once invited.");
	return FString::Printf(TEXT("Clan %s (leader %s): %s"), *Name, *C->Leader, *FString::Join(C->Members, TEXT(", ")));
}

// ---------------------------------------------------------------- rules

bool FWRSocialRules::CanShare(const FString& Owner, const FString& Player)
{
	return Owner.IsEmpty() || Owner == Player || FWRClans::Get().SameClan(Owner, Player);
}

FString FWRSocialRules::Setting(const TCHAR* Key, const TCHAR* Fallback)
{
	return FWRTuning::Get().Str(FString::Printf(TEXT("server.%s"), Key), Fallback);
}

bool FWRSocialRules::IsInSafeZone(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<AWRSafeZone> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Location) < It->Radius) return true;
	}
	return false;
}

bool FWRSocialRules::PlayersCanFight(const AWRCharacterBase* A, const AWRCharacterBase* B)
{
	const APlayerState* PA = A ? A->GetPlayerState() : nullptr;
	const APlayerState* PB = B ? B->GetPlayerState() : nullptr;
	if (!PA || !PB)
	{
		return true;
	}
	if (FWRClans::Get().SameClan(PA->GetPlayerName(), PB->GetPlayerName()))
	{
		return false;
	}
	const UWorld* World = A->GetWorld();
	if (IsInSafeZone(World, A->GetActorLocation()) || IsInSafeZone(World, B->GetActorLocation()))
	{
		return false;
	}
	// PvP inside boss arenas can be switched separately.
	for (TActorIterator<AWRBoss> It(World); It; ++It)
	{
		if (It->IsAlive() && FVector::Dist(It->GetHome(), A->GetActorLocation()) < WRTune(TEXT("bosses.arenaPvpRadius"), 3000.f))
		{
			return WRTune(TEXT("server.pvpInBossArenas"), 1.f) > 0.f && WRTune(TEXT("server.pvp"), 1.f) > 0.f;
		}
	}
	return WRTune(TEXT("server.pvp"), 1.f) > 0.f;
}

bool FWRSocialRules::IsAnyoneOnline(const UWorld* World, const FString& Owner)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || Owner.IsEmpty())
	{
		return true;
	}
	for (const APlayerState* P : GS->PlayerArray)
	{
		if (P && CanShare(Owner, P->GetPlayerName())) return true;
	}
	return false;
}

// ---------------------------------------------------------------- safe zone

AWRSafeZone::AWRSafeZone()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWRSafeZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRSafeZone, Radius);
}

// ---------------------------------------------------------------- loot bag

AWRLootBag::AWRLootBag()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded()) Mesh->SetMaterial(0, Mat.Object);
	Mesh->SetRelativeScale3D(FVector(0.5f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWRLootBag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRLootBag, OwnerName);
	DOREPLIFETIME(AWRLootBag, bOwnerOnly);
}

void AWRLootBag::ApplyDeathPenalty(AWRCharacterBase* Died, AWRPlayerState* PS)
{
	if (!Died || !PS)
	{
		return;
	}
	// gentle: keep everything. souls: carried items wait where you fell, only for you (die again and the old bag is gone).
	// drop: carried items, anyone can take them. full: carried + equipped, anyone.
	const FString Mode = FWRSocialRules::Setting(TEXT("deathPenalty"), TEXT("gentle"));
	if (Mode == TEXT("gentle"))
	{
		return;
	}
	const FString Who = PS->GetPlayerName();
	if (Mode == TEXT("souls"))
	{
		for (TActorIterator<AWRLootBag> It(Died->GetWorld()); It; ++It)
		{
			if (It->OwnerName == Who && It->bOwnerOnly)
			{
				UE_LOG(LogWildreach, Log, TEXT("DEATH %s's unrecovered souls bag is lost"), *Who);
				It->Destroy();
			}
		}
	}
	UWRInventoryComponent* Inv = PS->GetInventory();
	TArray<FWRItemStack> Items;
	for (FWRItemStack& S : Inv->GetSlotsMutable())
	{
		if (!S.IsEmpty()) { Items.Add(S); S = FWRItemStack(); }
	}
	if (Mode == TEXT("full"))
	{
		for (EWREquipSlot Slot : { EWREquipSlot::Weapon, EWREquipSlot::Armor })
		{
			FWRItemStack* E = Inv->GetEquippedMutable(Slot);
			if (E && !E->IsEmpty()) { Items.Add(*E); *E = FWRItemStack(); }
		}
		Inv->ApplyEquipment();
	}
	if (Items.Num() == 0)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector At = Died->GetActorLocation() - FVector(0.f, 0.f, Died->GetSimpleCollisionHalfHeight() - 25.f);
	if (AWRLootBag* Bag = Died->GetWorld()->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), At, FRotator::ZeroRotator, Params))
	{
		Bag->OwnerName = Who;
		Bag->bOwnerOnly = Mode == TEXT("souls");
		Bag->Items = Items;
		Bag->SetLifeSpan(WRTune(TEXT("death.bagMinutes"), 30.f) * 60.f);
		if (UMaterialInstanceDynamic* MID = Bag->Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Bag->bOwnerOnly ? FLinearColor(0.5f, 0.8f, 1.f) : FLinearColor(0.6f, 0.4f, 0.2f));
		}
		UE_LOG(LogWildreach, Log, TEXT("DEATH %s (%s): %d stacks dropped at %s"), *Who, *Mode, Items.Num(), *At.ToCompactString());
	}
}

FString AWRLootBag::Loot(AWRPlayerController* Player)
{
	AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
	if (!PS || !HasAuthority())
	{
		return FString();
	}
	if (bOwnerOnly && PS->GetPlayerName() != OwnerName)
	{
		return FString::Printf(TEXT("%s's lost belongings: only they can recover them"), *OwnerName);
	}
	int32 Taken = 0;
	for (FWRItemStack& S : Items)
	{
		if (!S.IsEmpty() && PS->GetInventory()->AddInstance(S))
		{
			S = FWRItemStack();
			++Taken;
		}
	}
	Items.RemoveAll([](const FWRItemStack& S) { return S.IsEmpty(); });
	if (Items.Num() == 0)
	{
		Destroy();
	}
	return Items.Num() == 0 ? FString::Printf(TEXT("Recovered everything (%d stacks)"), Taken) : FString::Printf(TEXT("Took %d stacks, %d didn't fit"), Taken, Items.Num());
}
