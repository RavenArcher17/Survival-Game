#include "WRSocial.h"
#include "Wildreach.h"
#include "WRBoss.h"
#include "WRCharacterBase.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "WRStore.h"
#include "WRIdentity.h"
#include "WRClanPins.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "WRLootLook.h"
#include "WRGameMode.h"
#include "WRPathComponent.h"
#include "WRPathTypes.h"
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
	const FString Dir = WRWorlds::CurrentDir();
	return Dir.IsEmpty() ? FString() : FPaths::Combine(Dir, TEXT("clans.json"));  // "": the menu's island keeps none
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
	if (ClansPath().IsEmpty() || !WRStore::ReadDoc(TEXT("clans"), Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
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
	if (!ClansPath().IsEmpty()) WRStore::WriteDoc(TEXT("clans"), Text);
}

void FWRClans::RefreshPlayerStates(const UWorld* World) const
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS) return;
	for (APlayerState* P : GS->PlayerArray)
	{
		AWRPlayerState* WPS = Cast<AWRPlayerState>(P);
		if (!WPS) continue;
		const FString Name = WPS->GetPlayerName();
		WPS->ClanName = ClanOf(Name);
		// The Clan tab's: your clan as it stands, and who has invited you.
		const FClan* Mine = Clans.Find(WPS->ClanName);
		WPS->ClanLeader = Mine ? Mine->Leader : FString();
		WPS->ClanMembers = Mine ? Mine->Members : TArray<FString>();
		WPS->ClanInvited = Mine ? Mine->Invites : TArray<FString>();
		WPS->ClanInvitesToMe.Reset();
		WPS->ClanInviteFrom.Reset();
		for (const TPair<FString, FClan>& KV : Clans)
		{
			if (KV.Value.Invites.Contains(Name)) { WPS->ClanInvitesToMe.Add(KV.Key); WPS->ClanInviteFrom.Add(KV.Value.Leader); }
		}
	}
	// Clanmates' beds are offered too: a clan change changes whose beds each player has.
	WRBeds::RefreshAll(const_cast<UWorld*>(World));
	// And their map pins (WRClanPins): someone joined, or a clan gained or lost a member.
	WRClanPins::SendAll(const_cast<UWorld*>(World));
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
	return FString::Printf(TEXT("Invited %s to clan %s"), *Target, *ClanOf(Player));
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

FString FWRClans::Decline(const FString& Player, const FString& Clan)
{
	FClan* C = Clans.Find(Clan);
	if (!C || !C->Invites.Remove(Player)) return TEXT("No invite from that clan");
	Save();
	return FString::Printf(TEXT("Turned down clan %s"), *Clan);
}

FString FWRClans::Uninvite(const FString& Player, const FString& Target)
{
	FClan* C = Clans.Find(ClanOf(Player));
	if (!C || C->Leader != Player) return TEXT("Only the clan leader can take back an invite");
	if (!C->Invites.Remove(Target)) return TEXT("No invite out to them");
	Save();
	return FString::Printf(TEXT("Took back the invite to %s"), *Target);
}

FString FWRClans::LeaderOf(const FString& Clan) const
{
	const FClan* C = Clans.Find(Clan);
	return C ? C->Leader : FString();
}

FString FWRClans::Describe(const FString& Player) const
{
	const FString Name = ClanOf(Player);
	const FClan* C = Clans.Find(Name);
	if (!C) return TEXT("No clan: found one, or accept an invite, in the Clan tab (O)");
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
	const AWRPlayerState* WA = Cast<AWRPlayerState>(PA);
	const AWRPlayerState* WB = Cast<AWRPlayerState>(PB);
	if (WA && WB && !WA->ClanName.IsEmpty() && WA->ClanName == WB->ClanName)
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

namespace WRSocialProtect
{
	struct FSession
	{
		TMap<FString, int64> Left;
		int64 StartedUtc = 0;
	};
	FSession& Session()
	{
		static FSession S;
		if (S.StartedUtc == 0) S.StartedUtc = FDateTime::UtcNow().ToUnixTimestamp();
		return S;
	}
}

void FWRSocialRules::ResetSession()
{
	WRSocialProtect::Session().Left.Reset();
	WRSocialProtect::Session().StartedUtc = FDateTime::UtcNow().ToUnixTimestamp();
}

void FWRSocialRules::NoteLeft(const FString& Player)
{
	if (Player.IsEmpty()) return;
	WRSocialProtect::Session().Left.Add(Player.ToLower(), FDateTime::UtcNow().ToUnixTimestamp());
	WRIdentity::NoteLeft(Player);
}

void FWRSocialRules::NoteJoined(const FString& Player)
{
	// Online now: IsAnyoneOnline covers them; their leaving time is set again when they go.
	WRSocialProtect::Session().Left.Remove(Player.ToLower());
}

int64 FWRSocialRules::LastLeftUtc(const FString& Player)
{
	WRSocialProtect::FSession& S = WRSocialProtect::Session();
	if (const int64* At = S.Left.Find(Player.ToLower()))
	{
		return *At;
	}
	// Not seen this session: their account's last seen (written as they join and leave), never before this world started
	// (a restart doesn't hand out a free, instant arm: the clock starts at the restart for anyone the store can't place).
	int64 At = S.StartedUtc;
	WRStore::FAccount A;
	if (WRStore::IsSql() && WRStore::ReadAccount(Player, A) && A.LastSeen > 0)
	{
		At = A.LastSeen;
	}
	S.Left.Add(Player.ToLower(), At);
	return At;
}

uint8 FWRSocialRules::ProtectionFor(const UWorld* World, const FString& Owner, int64 LastAttackedUtc, int64& OutArmsAtUtc)
{
	OutArmsAtUtc = 0;
	if (WRTune(TEXT("server.offlineProtection"), 0.f) <= 0.f || Owner.IsEmpty())
	{
		return ProtectOff;
	}
	if (IsAnyoneOnline(World, Owner))
	{
		return ProtectHome;
	}
	// The owner and everyone in their clan: armed from the last of them to leave.
	TArray<FString> Owners;
	const FString Clan = FWRClans::Get().ClanOf(Owner);
	if (!Clan.IsEmpty()) Owners = FWRClans::Get().Members(Clan);
	Owners.AddUnique(Owner);
	int64 Last = 0;
	for (const FString& Who : Owners) Last = FMath::Max(Last, LastLeftUtc(Who));
	int64 ArmsAt = Last + int64(FMath::Max(0.f, WRTune(TEXT("server.offlineArmMinutes"), 15.f)) * 60.f);
	if (LastAttackedUtc > 0)
	{
		ArmsAt = FMath::Max(ArmsAt, LastAttackedUtc + int64(FMath::Max(0.f, WRTune(TEXT("server.offlineQuietMinutes"), 10.f)) * 60.f));
	}
	OutArmsAtUtc = ArmsAt;
	return FDateTime::UtcNow().ToUnixTimestamp() >= ArmsAt ? ProtectOn : ProtectArming;
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
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Modelled on every machine that draws (WRLootLook); nothing collides with it: it's found by distance.
	Body = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	// Ticks only on its owner's machine, for the beacon (BuildLook turns it on).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
}

void AWRLootBag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWRLootBag, OwnerName);
	DOREPLIFETIME(AWRLootBag, bOwnerOnly);
	DOREPLIFETIME(AWRLootBag, Kind);
	DOREPLIFETIME(AWRLootBag, ExpiresAt);
	DOREPLIFETIME(AWRLootBag, OwnerPath);
}

void AWRLootBag::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Settle();
	}
	BuildLook();
}

void AWRLootBag::Setup(uint8 InKind, float LifeSeconds)
{
	Kind = InKind;
	if (LifeSeconds > 0.f)
	{
		SetLifeSpan(LifeSeconds);
		ExpiresAt = float(GetWorld()->GetTimeSeconds() + LifeSeconds);
	}
	BuildLook();   // the host's own view (OnRep only runs on clients)
	ForceNetUpdate();
}

void AWRLootBag::OnRep_Look()
{
	BuildLook();
}

void AWRLootBag::Settle()
{
	// On the ground under it (a floor, a rock, the land), its foot turned to the slope, a turn of its own.
	UWorld* World = GetWorld();
	if (!World) return;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WRBagSettle), false, this);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	if (!World->LineTraceSingleByObjectType(Hit, From, GetActorLocation() - FVector(0.f, 0.f, 300.f), Objects, Q))
	{
		return;
	}
	const float Yaw = FMath::Frac(FMath::Abs(GetActorLocation().X * 0.0137f + GetActorLocation().Y * 0.0071f)) * 360.f;
	const FVector Up = Hit.ImpactNormal.GetSafeNormal();
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Up.Z > 0.5f ? Up : FVector::UpVector);
	SetActorLocationAndRotation(Hit.ImpactPoint, Tilt * FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)));
}

void AWRLootBag::BuildLook()
{
	if (GetNetMode() == NM_DedicatedServer || BuiltKind == int32(Kind) * 2 + (OwnerPath.IsNone() ? 0 : 1))
	{
		return;
	}
	BuiltKind = int32(Kind) * 2 + (OwnerPath.IsNone() ? 0 : 1);
	const FWRToolMesh Look = Kind == KindDeath ? WRLootLook::Satchel() : Kind == KindSpill ? WRLootLook::Crate() : WRLootLook::Sack();
	Look.ApplyTo(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// A souls satchel's beacon: only its owner's game draws it (the others see the satchel alone).
	const bool bBeacon = Kind == KindDeath && bOwnerOnly;
	if (bBeacon && !Beacon)
	{
		Beacon = NewObject<UProceduralMeshComponent>(this, TEXT("Beacon"));
		Beacon->SetupAttachment(RootComponent);
		Beacon->SetUsingAbsoluteRotation(true);   // straight up, however the satchel lies
		Beacon->RegisterComponent();
		WRLootLook::Beacon(WRTune(TEXT("death.beaconHeightCm"), 600.f)).ApplyTo(Beacon);
		Beacon->SetCastShadow(false);
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_WindupGlow.M_WindupGlow")))
		{
			BeaconMat = UMaterialInstanceDynamic::Create(Base, this);
			Beacon->SetMaterial(0, BeaconMat);
		}
		BeaconLight = NewObject<UPointLightComponent>(this, TEXT("BeaconLight"));
		BeaconLight->SetupAttachment(RootComponent);
		BeaconLight->RegisterComponent();
		BeaconLight->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		BeaconLight->SetIntensity(WRTune(TEXT("death.beaconLight"), 1800.f));
		BeaconLight->SetAttenuationRadius(450.f);
		BeaconLight->SetCastShadows(false);
	}
	if (Beacon)
	{
		// The owner's main path's colour (each path's own: Fire ember, Frost pale blue...); a warm gold without one.
		const FWRPathDef* Def = OwnerPath.IsNone() ? nullptr : FWRPathRegistry::Get().Find(OwnerPath);
		const FLinearColor C = Def && Def->Color.A > 0.f ? Def->Color : FLinearColor(FColor(255, 196, 110));
		if (BeaconMat)
		{
			BeaconMat->SetVectorParameterValue(TEXT("Color"), C);
			BeaconMat->SetScalarParameterValue(TEXT("Glow"), WRTune(TEXT("death.beaconGlow"), 1.6f));
			BeaconMat->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		}
		BeaconLight->SetLightColor(C);
		Beacon->SetVisibility(false);
		BeaconLight->SetVisibility(false);
		SetActorTickEnabled(true);
	}
}

void AWRLootBag::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Beacon) { SetActorTickEnabled(false); return; }
	// Only its owner sees it, from up to death.beaconRangeCm; a slow breath of light.
	const APlayerController* Local = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* View = Local ? Local->GetPawn() : nullptr;
	const bool bMine = Local && Local->PlayerState && Local->PlayerState->GetPlayerName() == OwnerName;
	const float Range = WRTune(TEXT("death.beaconRangeCm"), 20000.f);
	const float Dist = View ? FVector::Dist(View->GetActorLocation(), GetActorLocation()) : 0.f;
	const bool bShow = bMine && Dist < Range;
	Beacon->SetVisibility(bShow);
	BeaconLight->SetVisibility(bShow && Dist < 6000.f);
	if (bShow && BeaconMat)
	{
		const float Breath = 0.5f + 0.5f * FMath::Sin(float(GetWorld()->GetTimeSeconds()) * 2.f * PI / FMath::Max(0.5f, WRTune(TEXT("death.beaconPulseSeconds"), 3.f)));
		// Fainter close up (it's beside you), stronger far off (it's what you look for).
		const float Far = FMath::Clamp((Dist - 300.f) / 3000.f, 0.25f, 1.f);
		BeaconMat->SetScalarParameterValue(TEXT("Opacity"), WRTune(TEXT("death.beaconOpacity"), 0.28f) * Far * (0.75f + 0.25f * Breath));
	}
}

TSharedRef<FJsonObject> AWRLootBag::SaveAll(UWorld* World)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> List;
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	for (TActorIterator<AWRLootBag> It(World); It; ++It)
	{
		// A death's belongings (souls, drop or full loot), with the time they have left; other drops stay short-lived.
		if (It->Kind != KindDeath || It->Items.Num() == 0 || It->IsActorBeingDestroyed()) continue;
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("owner"), It->OwnerName);
		O->SetBoolField(TEXT("ownerOnly"), It->bOwnerOnly);
		O->SetStringField(TEXT("path"), It->OwnerPath.ToString());
		const FVector L = It->GetActorLocation();
		O->SetArrayField(TEXT("at"), { MakeShared<FJsonValueNumber>(L.X), MakeShared<FJsonValueNumber>(L.Y), MakeShared<FJsonValueNumber>(L.Z) });
		O->SetNumberField(TEXT("left"), It->ExpiresAt > 0.f ? FMath::Max(60.0, double(It->ExpiresAt) - Now) : WRTune(TEXT("death.bagMinutes"), 30.f) * 60.0);
		TArray<TSharedPtr<FJsonValue>> Stacks;
		for (const FWRItemStack& S : It->Items) if (!S.IsEmpty()) Stacks.Add(MakeShared<FJsonValueObject>(FWRInventoryRules::StackToJson(S)));
		O->SetArrayField(TEXT("items"), Stacks);
		List.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("bags"), List);
	return Root;
}

void AWRLootBag::SaveNow(UWorld* World, bool bClosing)
{
	// (While a world closes only its own last save writes: bags leaving with it aren't "gone".)
	if (!World || (World->bIsTearingDown && !bClosing) || WRWorlds::Current().IsEmpty()) return;
	FString Text;
	FJsonSerializer::Serialize(SaveAll(World), TJsonWriterFactory<>::Create(&Text));
	WRStore::WriteDoc(TEXT("bags"), Text);
}

void AWRLootBag::EndPlay(const EEndPlayReason::Type Reason)
{
	// A death's bag gone (taken, run out): the saved list without it, so a restart can't bring it back.
	const bool bDeath = Kind == KindDeath && HasAuthority();
	Super::EndPlay(Reason);
	if (bDeath && Reason == EEndPlayReason::Destroyed) SaveNow(GetWorld());
}

void AWRLootBag::LoadAll(UWorld* World, const TSharedPtr<FJsonObject>& Json)
{
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!World || !Json.IsValid() || !Json->TryGetArrayField(TEXT("bags"), List)) return;
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Stacks = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(TEXT("at"), At) || At->Num() < 3 || !O->TryGetArrayField(TEXT("items"), Stacks)) continue;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector L((*At)[0]->AsNumber(), (*At)[1]->AsNumber(), (*At)[2]->AsNumber());
		AWRLootBag* Bag = World->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), L + FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, Params);
		if (!Bag) continue;
		Bag->OwnerName = O->GetStringField(TEXT("owner"));
		O->TryGetBoolField(TEXT("ownerOnly"), Bag->bOwnerOnly);
		FString Path;
		if (O->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty() && Path != TEXT("None")) Bag->OwnerPath = FName(*Path);
		for (const TSharedPtr<FJsonValue>& S : *Stacks)
		{
			const FWRItemStack Stack = FWRInventoryRules::StackFromJson(S->AsObject());
			if (!Stack.IsEmpty()) Bag->Items.Add(Stack);
		}
		double Left = 0.0;
		O->TryGetNumberField(TEXT("left"), Left);
		Bag->Setup(KindDeath, float(FMath::Max(60.0, Left)));
		UE_LOG(LogWildreach, Log, TEXT("DEATH %s's belongings back after the restart at %s (%.0f min left)"), *Bag->OwnerName, *L.ToCompactString(), Left / 60.0);
	}
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
			// (A fallen tame's collar isn't your souls: it waits for you; cooks/cookpass-taming.md 3.10.)
			if (It->OwnerName == Who && It->bOwnerOnly && !It->Items.ContainsByPredicate([](const FWRItemStack& S) { return S.ItemId == TEXT("bond_token"); }))
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
		for (EWREquipSlot Slot : { EWREquipSlot::Weapon, EWREquipSlot::Head, EWREquipSlot::Armor, EWREquipSlot::Hands, EWREquipSlot::Legs,
			EWREquipSlot::Talisman, EWREquipSlot::Ring, EWREquipSlot::Ring2, EWREquipSlot::Light })
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
		Bag->OwnerPath = PS->GetPaths() ? PS->GetPaths()->GetMainPath() : NAME_None;
		Bag->Setup(KindDeath, WRTune(TEXT("death.bagMinutes"), 30.f) * 60.f);
		SaveNow(Died->GetWorld());
		UE_LOG(LogWildreach, Log, TEXT("DEATH %s (%s): %d stacks dropped at %s"), *Who, *Mode, Items.Num(), *At.ToCompactString());
	}
}

AWRLootBag* AWRLootBag::Spill(UWorld* World, const FVector& At, const FString& Owner, const TArray<FWRItemStack>& Items, uint8 InKind)
{
	TArray<FWRItemStack> Held = Items.FilterByPredicate([](const FWRItemStack& S) { return !S.IsEmpty(); });
	if (!World || Held.Num() == 0)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWRLootBag* Bag = World->SpawnActor<AWRLootBag>(AWRLootBag::StaticClass(), At, FRotator::ZeroRotator, Params);
	if (!Bag)
	{
		return nullptr;
	}
	Bag->OwnerName = Owner;
	Bag->bOwnerOnly = false;
	Bag->Items = MoveTemp(Held);
	Bag->Setup(InKind, WRTune(TEXT("building.spillMinutes"), 30.f) * 60.f);
	return Bag;
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
	// What was taken is saved with the taker at once, and the bag's new state too: a crash can't hand it out twice.
	if (Taken > 0 && Kind == KindDeath)
	{
		if (AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>()) GM->SavePlayer(PS);
		if (Items.Num() > 0) SaveNow(GetWorld());
	}
	if (Items.Num() == 0)
	{
		Destroy();
	}
	return Items.Num() == 0 ? FString::Printf(TEXT("Recovered everything (%d stacks)"), Taken) : FString::Printf(TEXT("Took %d stacks, %d didn't fit"), Taken, Items.Num());
}
