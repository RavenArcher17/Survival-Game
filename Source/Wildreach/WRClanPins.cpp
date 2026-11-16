// Map pins shared with your clan (cooks/wild-answers-1116.md 4.2; Aaron 11-16, q-map-pins). See WRClanPins.h.

#include "WRClanPins.h"
#include "Wildreach.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRSocial.h"
#include "WRStore.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WRClanPinsImpl
{
	static const TCHAR* Doc = TEXT("map_pins");
	static const TCHAR* Section = TEXT("Wildreach.Map");

	int32 PinMax() { return FMath::Max(1, static_cast<int32>(WRTune(TEXT("ui.pins.max"), 30.f))); }
	int32 LabelMax() { return FMath::Max(4, static_cast<int32>(WRTune(TEXT("ui.pins.labelMax"), 24.f))); }

	/** Server: everyone's pins in the current world (loaded from its store the first time it's asked). */
	struct FServer
	{
		FString World;   // WRWorlds::Current() they were loaded for ("" = the menu's island: kept in memory only)
		bool bLoaded = false;
		TMap<FString, TArray<FWRSharedPin>> ByPlayer;
	};
	FServer& Server() { static FServer S; return S; }

	FServer& Loaded()
	{
		FServer& S = Server();
		const FString Now = WRWorlds::Current();
		if (S.bLoaded && S.World == Now) return S;
		S.World = Now;
		S.bLoaded = true;
		S.ByPlayer.Reset();
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (Now.IsEmpty() || !WRStore::ReadDoc(Doc, Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return S;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Root->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (!KV.Value.IsValid() || !KV.Value->TryGetArray(List)) continue;
			TArray<FWRSharedPin>& Pins = S.ByPlayer.Add(KV.Key);
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
				if (!O.IsValid()) continue;
				FWRSharedPin& P = Pins.AddDefaulted_GetRef();
				P.Owner = KV.Key;
				P.Where = FVector2D(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")));
				P.Glyph = uint8(FMath::Clamp(static_cast<int32>(O->GetNumberField(TEXT("glyph"))), 0, 5));
				O->TryGetStringField(TEXT("label"), P.Label);
			}
		}
		return S;
	}

	void Save(const FServer& S)
	{
		if (S.World.IsEmpty()) return;
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		for (const TPair<FString, TArray<FWRSharedPin>>& KV : S.ByPlayer)
		{
			if (KV.Value.Num() == 0) continue;
			TArray<TSharedPtr<FJsonValue>> List;
			for (const FWRSharedPin& P : KV.Value)
			{
				const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetNumberField(TEXT("x"), FMath::RoundToDouble(P.Where.X));
				O->SetNumberField(TEXT("y"), FMath::RoundToDouble(P.Where.Y));
				O->SetNumberField(TEXT("glyph"), P.Glyph);
				O->SetStringField(TEXT("label"), P.Label);
				List.Add(MakeShared<FJsonValueObject>(O));
			}
			Root->SetArrayField(KV.Key, List);
		}
		FString Text;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
		WRStore::WriteDoc(Doc, Text);
	}

	/** Server: the pins of Player's clanmates (not their own), offline ones included. */
	TArray<FWRSharedPin> ClanPinsFor(const FString& Player)
	{
		TArray<FWRSharedPin> Out;
		const FWRClans& Clans = FWRClans::Get();
		const FString Clan = Clans.ClanOf(Player);
		if (Clan.IsEmpty()) return Out;
		const FServer& S = Loaded();
		for (const FString& Mate : Clans.Members(Clan))
		{
			if (Mate == Player) continue;
			if (const TArray<FWRSharedPin>* Pins = S.ByPlayer.Find(Mate)) Out.Append(*Pins);
		}
		return Out;
	}

	/** Server: send one connected player their clanmates' pins. */
	void SendTo(AWRPlayerController* PC)
	{
		const APlayerState* PS = PC ? PC->PlayerState : nullptr;
		if (!PS) return;
		PC->ClientClanPins(ClanPinsFor(PS->GetPlayerName()));
	}

	/** Client: what the server sent, for which world. */
	struct FClient
	{
		TWeakObjectPtr<const UWorld> World;
		TArray<FWRSharedPin> Pins;
		bool bDue = false;
		double LastSent = -100.0;
	};
	FClient& Client() { static FClient C; return C; }

	int32 ShowState = -1;   // -1 not read yet
}

void WRClanPins::SetPlayerPins(UWorld* World, const FString& Player, const TArray<FWRSharedPin>& Pins)
{
	using namespace WRClanPinsImpl;
	if (!World || Player.IsEmpty()) return;
	FServer& S = Loaded();
	TArray<FWRSharedPin> Clean;
	for (const FWRSharedPin& In : Pins)
	{
		if (Clean.Num() >= PinMax()) break;
		if (!FMath::IsFinite(In.Where.X) || !FMath::IsFinite(In.Where.Y)) continue;
		FWRSharedPin& P = Clean.AddDefaulted_GetRef();
		P.Owner = Player;   // never what the client wrote
		P.Where = FVector2D(FMath::Clamp(In.Where.X, -2.0e7, 2.0e7), FMath::Clamp(In.Where.Y, -2.0e7, 2.0e7));
		P.Glyph = FMath::Min<uint8>(In.Glyph, 5);
		P.Label = In.Label.Left(LabelMax());
	}
	// The same list again (a re-join sends what the server already has): nothing to save or send.
	if (const TArray<FWRSharedPin>* Had = S.ByPlayer.Find(Player); Had && Had->Num() == Clean.Num())
	{
		bool bSame = true;
		for (int32 i = 0; i < Clean.Num() && bSame; ++i)
		{
			bSame = (*Had)[i].Where.Equals(Clean[i].Where, 1.0) && (*Had)[i].Glyph == Clean[i].Glyph && (*Had)[i].Label == Clean[i].Label;
		}
		if (bSame) return;
	}
	S.ByPlayer.Add(Player, MoveTemp(Clean));
	Save(S);
	// Their online clanmates see the change now.
	const FString Clan = FWRClans::Get().ClanOf(Player);
	if (Clan.IsEmpty()) return;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
		const APlayerState* PS = PC ? PC->PlayerState : nullptr;
		if (PS && PS->GetPlayerName() != Player && FWRClans::Get().ClanOf(PS->GetPlayerName()) == Clan) SendTo(PC);
	}
	UE_LOG(LogWildreach, Log, TEXT("CLANPINS %s shares %d pins with %s"), *Player, S.ByPlayer[Player].Num(), *Clan);
}

void WRClanPins::SendAll(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) return;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		WRClanPinsImpl::SendTo(Cast<AWRPlayerController>(It->Get()));
	}
}

const TArray<FWRSharedPin>& WRClanPins::Clan(const UWorld* World)
{
	using namespace WRClanPinsImpl;
	FClient& C = Client();
	static const TArray<FWRSharedPin> None;
	return C.World.Get() == World && World ? C.Pins : None;
}

void WRClanPins::SetClan(const UWorld* World, const TArray<FWRSharedPin>& Pins)
{
	using namespace WRClanPinsImpl;
	FClient& C = Client();
	C.World = World;
	C.Pins = Pins;
}

void WRClanPins::NoteOwnChanged()
{
	WRClanPinsImpl::Client().bDue = true;
}

bool WRClanPins::FlushDue()
{
	const WRClanPinsImpl::FClient& C = WRClanPinsImpl::Client();
	return C.bDue && FPlatformTime::Seconds() - C.LastSent >= WRTune(TEXT("ui.pins.sendSeconds"), 1.f);
}

bool WRClanPins::Send(const UWorld* World, const TArray<FWRSharedPin>& Own)
{
	using namespace WRClanPinsImpl;
	AWRPlayerController* PC = World ? Cast<AWRPlayerController>(World->GetFirstPlayerController()) : nullptr;
	// A client needs its connection; a host or a single-player world runs the call in place.
	if (!PC || !PC->IsLocalController() || (World->GetNetMode() == NM_Client && !PC->GetNetConnection())) return false;
	PC->ServerMapPins(Own);
	FClient& C = Client();
	C.bDue = false;
	C.LastSent = FPlatformTime::Seconds();
	return true;
}

bool WRClanPins::ShowClan()
{
	using namespace WRClanPinsImpl;
	if (ShowState < 0)
	{
		bool bOn = false;
		if (GConfig) GConfig->GetBool(Section, TEXT("ShowClanPins"), bOn, GGameUserSettingsIni);
		ShowState = bOn ? 1 : 0;
	}
	return ShowState == 1;
}

void WRClanPins::SetShowClan(bool bOn)
{
	using namespace WRClanPinsImpl;
	ShowState = bOn ? 1 : 0;
	if (GConfig)
	{
		GConfig->SetBool(Section, TEXT("ShowClanPins"), bOn, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
