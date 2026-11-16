#include "WRAccess.h"
#include "Wildreach.h"
#include "WRIdentity.h"
#include "WRPlayerController.h"
#include "WRPlayerState.h"
#include "WRTuning.h"
#include "WRWorlds.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace WRAccessDetail
{
	FString PathFor(const FString& WorldId)
	{
		return WorldId.IsEmpty() ? FString() : FPaths::Combine(WRWorlds::Dir(WorldId), TEXT("access.json"));
	}

	/** An address without its port ("1.2.3.4:7777" -> "1.2.3.4"; IPv6 left as it is). */
	FString Bare(const FString& Address)
	{
		int32 First = INDEX_NONE, Last = INDEX_NONE;
		if (Address.FindChar(TEXT(':'), First) && Address.FindLastChar(TEXT(':'), Last) && First == Last)
		{
			return Address.Left(First);
		}
		return Address;
	}

	int64 Now() { return FDateTime::UtcNow().ToUnixTimestamp(); }

	/** Wrong passwords per address (FPlatformTime seconds), for access.passwordTries in access.passwordWindowSeconds. */
	TMap<FString, TArray<double>>& Tries()
	{
		static TMap<FString, TArray<double>> T;
		return T;
	}

	FWRAccess& CurrentStore()
	{
		static FWRAccess A;
		return A;
	}

	AWRPlayerController* FindPlayer(UWorld* World, const FString& Name)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AWRPlayerController* PC = Cast<AWRPlayerController>(It->Get());
			if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName().Equals(Name, ESearchCase::IgnoreCase)) return PC;
		}
		return nullptr;
	}
}

bool FWRAccess::IsAdmin(const FString& Name) const
{
	return Admins.ContainsByPredicate([&Name](const FString& A) { return A.Equals(Name, ESearchCase::IgnoreCase); });
}

int32 WRAccess::MinPlayers()
{
	return FMath::Max(1, FMath::RoundToInt(WRTune(TEXT("net.listenMinPlayers"), 2.f)));
}

int32 WRAccess::MaxPlayersLimit(bool bDedicated)
{
	return FMath::Max(MinPlayers(), FMath::RoundToInt(bDedicated ? WRTune(TEXT("net.dedicatedMaxPlayers"), 100.f) : WRTune(TEXT("net.listenMaxPlayers"), 16.f)));
}

FWRAccess WRAccess::Read(const FString& WorldId)
{
	FWRAccess A;
	// Until its host sets one: 8 on a listen world, more on a dedicated server (M7: 10 to 100).
	A.MaxPlayers = FMath::RoundToInt(IsRunningDedicatedServer() ? WRTune(TEXT("net.dedicatedDefaultPlayers"), 32.f) : WRTune(TEXT("net.listenDefaultPlayers"), 8.f));
	FString Text;
	TSharedPtr<FJsonObject> O;
	const FString Path = WRAccessDetail::PathFor(WorldId);
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid())
	{
		return A;
	}
	double Max = A.MaxPlayers;
	if (O->TryGetNumberField(TEXT("maxPlayers"), Max)) A.MaxPlayers = FMath::RoundToInt(Max);
	O->TryGetStringField(TEXT("passSalt"), A.PassSalt);
	O->TryGetStringField(TEXT("passHash"), A.PassHash);
	O->TryGetStringArrayField(TEXT("admins"), A.Admins);
	const TArray<TSharedPtr<FJsonValue>>* Bans = nullptr;
	if (O->TryGetArrayField(TEXT("bans"), Bans))
	{
		for (const TSharedPtr<FJsonValue>& V : *Bans)
		{
			const TSharedPtr<FJsonObject> B = V->AsObject();
			if (!B.IsValid()) continue;
			FWRBan Ban;
			B->TryGetStringField(TEXT("name"), Ban.Name);
			B->TryGetStringField(TEXT("address"), Ban.Address);
			B->TryGetStringField(TEXT("by"), Ban.By);
			double Until = 0.0;
			if (B->TryGetNumberField(TEXT("until"), Until)) Ban.Until = int64(Until);
			A.Bans.Add(Ban);
		}
	}
	return A;
}

bool WRAccess::Write(const FString& WorldId, const FWRAccess& A)
{
	const FString Path = WRAccessDetail::PathFor(WorldId);
	if (Path.IsEmpty()) return false;
	const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("maxPlayers"), A.MaxPlayers);
	if (A.HasPassword())
	{
		O->SetStringField(TEXT("passSalt"), A.PassSalt);
		O->SetStringField(TEXT("passHash"), A.PassHash);
	}
	TArray<TSharedPtr<FJsonValue>> Admins, Bans;
	for (const FString& S : A.Admins) Admins.Add(MakeShared<FJsonValueString>(S));
	for (const FWRBan& Ban : A.Bans)
	{
		const TSharedRef<FJsonObject> B = MakeShared<FJsonObject>();
		B->SetStringField(TEXT("name"), Ban.Name);
		B->SetStringField(TEXT("address"), Ban.Address);
		B->SetStringField(TEXT("by"), Ban.By);
		B->SetNumberField(TEXT("until"), double(Ban.Until));
		Bans.Add(MakeShared<FJsonValueObject>(B));
	}
	O->SetArrayField(TEXT("admins"), Admins);
	O->SetArrayField(TEXT("bans"), Bans);
	FString Text;
	FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Text));
	// Whole or not at all: a temp file, then moved over the old one.
	const FString Temp = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Text, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) || !IFileManager::Get().Move(*Path, *Temp, true, true))
	{
		UE_LOG(LogWildreach, Warning, TEXT("ACCESS couldn't write %s"), *Path);
		return false;
	}
	return true;
}

FString WRAccess::WireHash(const FString& Typed)
{
	return Typed.IsEmpty() ? FString() : WRIdentity::Sha256Hex(TEXT("wildreach-pass-1:") + Typed);
}

void WRAccess::SetPassword(FWRAccess& A, const FString& Typed)
{
	if (Typed.IsEmpty())
	{
		A.PassSalt.Reset();
		A.PassHash.Reset();
		return;
	}
	A.PassSalt = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	A.PassHash = WRIdentity::Sha256Hex(A.PassSalt + WireHash(Typed));
}

FWRAccess& WRAccess::Current()
{
	return WRAccessDetail::CurrentStore();
}

void WRAccess::LoadCurrent()
{
	WRAccessDetail::CurrentStore() = Read(WRWorlds::Current());
	WRAccessDetail::Tries().Reset();
}

void WRAccess::SaveCurrent()
{
	Write(WRWorlds::Current(), WRAccessDetail::CurrentStore());
}

FString WRAccess::BanLeft(const FWRBan& Ban)
{
	if (Ban.Until <= 0) return TEXT("for good");
	const int64 Left = FMath::Max<int64>(0, Ban.Until - WRAccessDetail::Now());
	return Left >= 3600 ? FString::Printf(TEXT("%lld h left"), (Left + 3599) / 3600) : FString::Printf(TEXT("%lld min left"), FMath::Max<int64>(1, (Left + 59) / 60));
}

FString WRAccess::Refuse(const FString& Name, const FString& Wire, const FString& Address, int32 NumPlayers)
{
	FWRAccess& A = Current();
	const FString Ip = WRAccessDetail::Bare(Address);
	const int64 Now = WRAccessDetail::Now();
	// Bans that have run out go (and are saved gone).
	const int32 Before = A.Bans.Num();
	A.Bans.RemoveAll([Now](const FWRBan& B) { return B.Until > 0 && B.Until <= Now; });
	if (A.Bans.Num() != Before) SaveCurrent();
	for (const FWRBan& B : A.Bans)
	{
		if ((!Name.IsEmpty() && B.Name.Equals(Name, ESearchCase::IgnoreCase)) || (!Ip.IsEmpty() && B.Address == Ip))
		{
			UE_LOG(LogWildreach, Log, TEXT("ACCESS refused %s (%s): banned (%s)"), *Name, *Ip, *BanLeft(B));
			return FString::Printf(TEXT("WR|Banned from this world (%s)."), *BanLeft(B));
		}
	}
	const int32 Max = FMath::Clamp(A.MaxPlayers, MinPlayers(), MaxPlayersLimit(IsRunningDedicatedServer()));
	if (NumPlayers >= Max)
	{
		UE_LOG(LogWildreach, Log, TEXT("ACCESS refused %s: full (%d/%d)"), *Name, NumPlayers, Max);
		return FString::Printf(TEXT("WR|World full (%d/%d). Try again when someone leaves."), NumPlayers, Max);
	}
	if (A.HasPassword())
	{
		TArray<double>& Mine = WRAccessDetail::Tries().FindOrAdd(Ip);
		const double Clock = FPlatformTime::Seconds();
		const double Window = WRTune(TEXT("access.passwordWindowSeconds"), 60.f);
		Mine.RemoveAll([Clock, Window](double T) { return Clock - T > Window; });
		if (Mine.Num() >= FMath::RoundToInt(WRTune(TEXT("access.passwordTries"), 3.f)))
		{
			const int32 Wait = FMath::Max(1, FMath::CeilToInt(Window - (Clock - Mine[0])));
			return FString::Printf(TEXT("WR|Too many wrong passwords: wait %d s and try again."), Wait);
		}
		if (Wire.IsEmpty())
		{
			return TEXT("PASSWORD|This world needs a password.");
		}
		if (WRIdentity::Sha256Hex(A.PassSalt + Wire) != A.PassHash)
		{
			Mine.Add(Clock);
			UE_LOG(LogWildreach, Log, TEXT("ACCESS refused %s (%s): wrong password (%d)"), *Name, *Ip, Mine.Num());
			return TEXT("PASSWORD|Wrong password.");
		}
	}
	return FString();
}

uint8 WRAccess::RoleOf(const APlayerController* PC)
{
	if (!PC || !PC->PlayerState) return 0;
	// The listen world's host: the server's own player.
	if (PC->GetNetMode() == NM_ListenServer && PC->IsLocalController()) return 2;
	return Current().IsAdmin(PC->PlayerState->GetPlayerName()) ? 1 : 0;
}

void WRAccess::RefreshRoles(UWorld* World)
{
	if (!World) return;
	TArray<FString> BanLines;
	for (const FWRBan& B : Current().Bans) BanLines.Add(FString::Printf(TEXT("%s\t%s"), *B.Name, *BanLeft(B)));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
		if (!PS) continue;
		PS->AccessRole = RoleOf(PC);
		PS->AdminBans = PS->AccessRole > 0 ? BanLines : TArray<FString>();
	}
}

FString WRAccess::Command(APlayerController* By, const TArray<FString>& Args)
{
	UWorld* World = By ? By->GetWorld() : nullptr;
	const uint8 Role = RoleOf(By);
	if (!World || Role == 0) return TEXT("Only the host and admins can do that");
	const FString Act = Args.IsValidIndex(1) ? Args[1].ToLower() : FString();
	const FString Name = Args.IsValidIndex(2) ? Args[2] : FString();
	const FString Me = By->PlayerState ? By->PlayerState->GetPlayerName() : FString();
	FWRAccess& A = Current();
	if (Name.IsEmpty()) return TEXT("Who?");
	if (Act == TEXT("unban"))
	{
		const int32 Gone = A.Bans.RemoveAll([&Name](const FWRBan& B) { return B.Name.Equals(Name, ESearchCase::IgnoreCase); });
		if (Gone == 0) return FString::Printf(TEXT("%s isn't banned"), *Name);
		SaveCurrent();
		RefreshRoles(World);
		UE_LOG(LogWildreach, Log, TEXT("ACCESS %s unbanned %s"), *Me, *Name);
		return FString::Printf(TEXT("%s may come back"), *Name);
	}
	if (Act == TEXT("op") || Act == TEXT("deop"))
	{
		if (Role < 2) return TEXT("Only the host makes admins");
		if (Act == TEXT("op")) A.Admins.AddUnique(Name);
		else A.Admins.RemoveAll([&Name](const FString& S) { return S.Equals(Name, ESearchCase::IgnoreCase); });
		SaveCurrent();
		RefreshRoles(World);
		UE_LOG(LogWildreach, Log, TEXT("ACCESS %s %s %s"), *Me, *Act, *Name);
		if (AWRPlayerController* T = WRAccessDetail::FindPlayer(World, Name))
		{
			T->ClientNotify(Act == TEXT("op") ? TEXT("You're an admin of this world now: kick and ban from the Clan tab (O)") : TEXT("You're no longer an admin of this world"));
		}
		return Act == TEXT("op") ? FString::Printf(TEXT("%s is an admin now"), *Name) : FString::Printf(TEXT("%s is no longer an admin"), *Name);
	}
	if (Act != TEXT("kick") && Act != TEXT("ban")) return TEXT("admin kick|ban|unban|op|deop <name> [hours]");
	AWRPlayerController* Target = WRAccessDetail::FindPlayer(World, Name);
	if (!Target) return FString::Printf(TEXT("Nobody called %s is here"), *Name);
	if (Target == By) return Act == TEXT("ban") ? TEXT("You can't ban yourself") : TEXT("You can't kick yourself");
	const uint8 TargetRole = RoleOf(Target);
	if (TargetRole == 2) return Act == TEXT("ban") ? TEXT("You can't ban the host") : TEXT("You can't kick the host");
	if (TargetRole == 1 && Role < 2) return TEXT("Only the host can remove an admin");
	const FString Who = Role == 2 ? FString(TEXT("the host")) : FString::Printf(TEXT("an admin (%s)"), *Me);
	FString Reason;
	if (Act == TEXT("ban"))
	{
		const int32 Hours = Args.IsValidIndex(3) ? FMath::Max(0, FCString::Atoi(*Args[3])) : 0;
		FWRBan Ban;
		Ban.Name = Target->PlayerState->GetPlayerName();
		Ban.Address = WRAccessDetail::Bare(Target->GetPlayerNetworkAddress());
		Ban.Until = Hours > 0 ? WRAccessDetail::Now() + int64(Hours) * 3600 : 0;
		Ban.By = Me;
		A.Bans.RemoveAll([&Ban](const FWRBan& B) { return B.Name.Equals(Ban.Name, ESearchCase::IgnoreCase); });
		A.Bans.Add(Ban);
		SaveCurrent();
		Reason = FString::Printf(TEXT("Banned from this world by %s (%s)."), *Who, Hours > 0 ? *FString::Printf(TEXT("%d h"), Hours) : TEXT("for good"));
	}
	else
	{
		Reason = FString::Printf(TEXT("Kicked by %s. You can join again."), *Who);
	}
	UE_LOG(LogWildreach, Log, TEXT("ACCESS %s %s %s: %s"), *Me, *Act, *Name, *Reason);
	const FString Shown = Target->PlayerState->GetPlayerName();
	AGameModeBase* GM = World->GetAuthGameMode();
	if (GM && GM->GameSession)
	{
		GM->GameSession->KickPlayer(Target, FText::FromString(Reason));
	}
	RefreshRoles(World);
	return Act == TEXT("ban") ? FString::Printf(TEXT("Banned %s"), *Shown) : FString::Printf(TEXT("Kicked %s"), *Shown);
}
