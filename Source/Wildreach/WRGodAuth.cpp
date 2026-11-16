// Who is a tester: the client's request to the Freedom35 site and the server's check of the proof (WRGodAuth.h).
#include "WRGodAuth.h"
#include "Wildreach.h"
#include "WRIdentity.h"
#include "WRTuning.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

// The engine's OpenSSL (1.1.1t): Ed25519 signature checks. OpenSSL's UI type collides with Unreal's UI namespace.
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <openssl/evp.h>
THIRD_PARTY_INCLUDES_END
#undef UI
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace WRGodAuthImpl
{
	/** The site's tester-proof public keys (Ed25519, raw 32 bytes, hex). The private half is the site's
	 *  TESTER_SIGNING_KEY secret; the second slot is for a rotation (ship the new key here first, then swap the secret). */
	static const TCHAR* PublicKeys[] = {
		TEXT("a1983322bc103f8d3e8ae6f9e850ae1614e86b686bd2bfaabb56c5ec8ae8b894"),
		TEXT(""),
	};

	static const TCHAR* Game = TEXT("wildreach");
	static const TCHAR* BindSalt = TEXT("wildreach-tester-bind-1");

	/** base64url (no padding) -> bytes. */
	bool Unb64Url(const FString& In, TArray<uint8>& Out)
	{
		FString S = In.Replace(TEXT("-"), TEXT("+")).Replace(TEXT("_"), TEXT("/"));
		while (S.Len() % 4) S += TEXT("=");
		return FBase64::Decode(S, Out);
	}

	bool HexBytes(const FString& Hex, TArray<uint8>& Out)
	{
		if (Hex.Len() % 2) return false;
		Out.SetNum(Hex.Len() / 2);
		return HexToBytes(Hex, Out.GetData()) == Out.Num();
	}

	bool Ed25519(const TArray<uint8>& PublicKey, const TArray<uint8>& Signature, const FTCHARToUTF8& Message)
	{
		if (PublicKey.Num() != 32 || Signature.Num() != 64) return false;
		EVP_PKEY* Key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, PublicKey.GetData(), PublicKey.Num());
		if (!Key) return false;
		EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
		bool bOk = false;
		if (Ctx && EVP_DigestVerifyInit(Ctx, nullptr, nullptr, nullptr, Key) == 1)
		{
			bOk = EVP_DigestVerify(Ctx, Signature.GetData(), Signature.Num(), reinterpret_cast<const unsigned char*>(Message.Get()), Message.Length()) == 1;
		}
		if (Ctx) EVP_MD_CTX_free(Ctx);
		EVP_PKEY_free(Key);
		return bOk;
	}

	/** This Windows user's Freedom35 PC link (written by F35Update with DPAPI, CurrentUser), or empty. */
	FString ReadPcLink()
	{
#if PLATFORM_WINDOWS
		const FString Dir = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
		if (Dir.IsEmpty()) return FString();
		TArray<uint8> Sealed;
		if (!FFileHelper::LoadFileToArray(Sealed, *FPaths::Combine(Dir, TEXT("Freedom35"), TEXT("pc-link.bin")), FILEREAD_Silent) || Sealed.Num() == 0)
		{
			return FString();
		}
		// CryptUnprotectData from crypt32.dll, looked up at run time with our own blob type (wincrypt.h's X509 macros would
		// collide with OpenSSL's in a unity build).
		struct FBlob { unsigned long Size; unsigned char* Data; };
		using FUnprotect = int(__stdcall*)(FBlob*, wchar_t**, FBlob*, void*, void*, unsigned long, FBlob*);
		static FUnprotect Unprotect = nullptr;
		if (!Unprotect)
		{
			if (HMODULE Crypt = ::LoadLibraryW(L"crypt32.dll"))
			{
				Unprotect = reinterpret_cast<FUnprotect>(reinterpret_cast<void*>(::GetProcAddress(Crypt, "CryptUnprotectData")));
			}
		}
		if (!Unprotect) return FString();
		FBlob In{ static_cast<unsigned long>(Sealed.Num()), Sealed.GetData() };
		FBlob Out{ 0, nullptr };
		if (!Unprotect(&In, nullptr, nullptr, nullptr, nullptr, 0, &Out) || !Out.Data)
		{
			return FString();
		}
		const FString Token = FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Out.Data), int32(Out.Size))).TrimStartAndEnd();
		::LocalFree(Out.Data);
		// Only the shape F35Update writes (f35pc_<8 hex>_<secret>); anything else is ignored.
		return Token.StartsWith(TEXT("f35pc_")) && Token.Len() < 200 ? Token : FString();
#else
		return FString();
#endif
	}
}

FString WRGodAuth::BindFor(const FString& IdentityToken)
{
	return IdentityToken.IsEmpty() ? FString() : WRIdentity::Sha256Hex(IdentityToken + WRGodAuthImpl::BindSalt);
}

void WRGodAuth::RequestProof(const FString& Nonce, TFunction<void(const FString& Proof, const FString& Label)> Done)
{
	using namespace WRGodAuthImpl;
	static TSet<FString> InFlight;
	const FString Link = ReadPcLink();
	const FString Bind = BindFor(WRIdentity::Token());
	if (Link.IsEmpty() || Bind.IsEmpty() || Nonce.IsEmpty() || InFlight.Contains(Nonce))
	{
		return;   // not played through Freedom35 (or already asking): no god mode, nothing sent anywhere
	}
	InFlight.Add(Nonce);
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game"), Game);
	Body->SetStringField(TEXT("bind"), Bind);
	Body->SetStringField(TEXT("nonce"), Nonce.ToLower());
	FString Text;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Text));
	const FString Site = FWRTuning::Get().Str(TEXT("god.site"), TEXT("https://freedom35studios.pages.dev"));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Site + TEXT("/api/pc/tester"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("Wildreach"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Link);   // never logged
	Request->SetContentAsString(Text);
	Request->SetTimeout(WRTune(TEXT("god.siteTimeout"), 12.f));
	Request->OnProcessRequestComplete().BindLambda([Nonce, Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		InFlight.Remove(Nonce);
		FString Proof, Label;
		TSharedPtr<FJsonObject> Json;
		const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
		if (bConnected && Code == 200 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json) && Json.IsValid())
		{
			bool bTester = false;
			Json->TryGetBoolField(TEXT("tester"), bTester);
			if (bTester)
			{
				Json->TryGetStringField(TEXT("proof"), Proof);
				Json->TryGetStringField(TEXT("label"), Label);
				UE_LOG(LogWildreach, Log, TEXT("GOD the site vouches for this account as a tester (%s)"), *Label);
			}
		}
		else if (Code != 0 && Code != 401)
		{
			UE_LOG(LogWildreach, Log, TEXT("GOD tester check: the site answered %d"), Code);
		}
		if (Done) Done(Proof, Label);
	});
	Request->ProcessRequest();
}

bool WRGodAuth::Verify(const FString& Proof, const FString& JoinKey, const FString& Nonce, FClaims& Out, FString& OutWhy)
{
	using namespace WRGodAuthImpl;
	FString PayloadText, SigText;
	if (Proof.Len() > 2000 || !Proof.Split(TEXT("."), &PayloadText, &SigText) || PayloadText.IsEmpty() || SigText.IsEmpty())
	{
		OutWhy = TEXT("malformed");
		return false;
	}
	TArray<uint8> Sig;
	if (!Unb64Url(SigText, Sig) || Sig.Num() != 64)
	{
		OutWhy = TEXT("malformed signature");
		return false;
	}
	const FTCHARToUTF8 Message(*(TEXT("wrtester.") + PayloadText));
	bool bSigned = false;
	for (const TCHAR* Hex : PublicKeys)
	{
		TArray<uint8> Key;
		if (FCString::Strlen(Hex) == 64 && HexBytes(Hex, Key) && Ed25519(Key, Sig, Message))
		{
			bSigned = true;
			break;
		}
	}
	if (!bSigned)
	{
		OutWhy = TEXT("bad signature");
		return false;
	}
	TArray<uint8> PayloadBytes;
	TSharedPtr<FJsonObject> Json;
	if (!Unb64Url(PayloadText, PayloadBytes)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(PayloadBytes.GetData()), PayloadBytes.Num()))), Json)
		|| !Json.IsValid())
	{
		OutWhy = TEXT("unreadable claims");
		return false;
	}
	const auto Str = [&Json](const TCHAR* Field) { FString S; Json->TryGetStringField(Field, S); return S; };
	double Exp = 0.0, Iat = 0.0;
	Json->TryGetNumberField(TEXT("exp"), Exp);
	Json->TryGetNumberField(TEXT("iat"), Iat);
	const double Now = double(FDateTime::UtcNow().ToUnixTimestamp());
	const FString WantAud = FString(Game) + TEXT("-tester");
	if (Str(TEXT("iss")) != TEXT("freedom35studios") || Str(TEXT("aud")) != WantAud || Str(TEXT("game")) != Game) OutWhy = TEXT("not a Wildreach tester proof");
	else if (Exp <= Now) OutWhy = TEXT("expired");
	else if (Iat > Now + WRTune(TEXT("god.clockSkew"), 120.f)) OutWhy = TEXT("issued in the future (this server's clock?)");
	else if (JoinKey.IsEmpty() || Str(TEXT("bind")) != BindFor(JoinKey)) OutWhy = TEXT("made for another PC");
	else if (Nonce.IsEmpty() || Str(TEXT("nonce")) != Nonce.ToLower()) OutWhy = TEXT("made for another connection");
	if (!OutWhy.IsEmpty())
	{
		return false;
	}
	Out.Label = Str(TEXT("label")).Left(24);
	Out.Sub = Str(TEXT("sub"));
	Out.Bind = Str(TEXT("bind"));
	Out.Nonce = Str(TEXT("nonce"));
	Out.Exp = int64(Exp);
	if (Out.Label.IsEmpty()) Out.Label = TEXT("Tester");
	return true;
}
