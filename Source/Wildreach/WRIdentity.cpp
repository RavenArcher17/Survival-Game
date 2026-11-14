// Name ownership: the client's key and token, the server's check and bind (WRIdentity.h).
#include "WRIdentity.h"
#include "Wildreach.h"
#include "WRStore.h"
#include "WRWorlds.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace WRIdentityImpl
{
	// SHA-256 (FIPS 180-4), small and self-contained: the engine's core has SHA-1 and MD5 only.
	static const uint32 K[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

	FORCEINLINE uint32 Rotr(uint32 X, uint32 N) { return (X >> N) | (X << (32 - N)); }

	void Block(uint32 H[8], const uint8* P)
	{
		uint32 W[64];
		for (int32 i = 0; i < 16; ++i) W[i] = (uint32(P[i * 4]) << 24) | (uint32(P[i * 4 + 1]) << 16) | (uint32(P[i * 4 + 2]) << 8) | uint32(P[i * 4 + 3]);
		for (int32 i = 16; i < 64; ++i)
		{
			const uint32 S0 = Rotr(W[i - 15], 7) ^ Rotr(W[i - 15], 18) ^ (W[i - 15] >> 3);
			const uint32 S1 = Rotr(W[i - 2], 17) ^ Rotr(W[i - 2], 19) ^ (W[i - 2] >> 10);
			W[i] = W[i - 16] + S0 + W[i - 7] + S1;
		}
		uint32 A = H[0], B = H[1], C = H[2], D = H[3], E = H[4], F = H[5], G = H[6], Hh = H[7];
		for (int32 i = 0; i < 64; ++i)
		{
			const uint32 S1 = Rotr(E, 6) ^ Rotr(E, 11) ^ Rotr(E, 25);
			const uint32 Ch = (E & F) ^ (~E & G);
			const uint32 T1 = Hh + S1 + Ch + K[i] + W[i];
			const uint32 S0 = Rotr(A, 2) ^ Rotr(A, 13) ^ Rotr(A, 22);
			const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
			const uint32 T2 = S0 + Maj;
			Hh = G; G = F; F = E; E = D + T1; D = C; C = B; B = A; A = T1 + T2;
		}
		H[0] += A; H[1] += B; H[2] += C; H[3] += D; H[4] += E; H[5] += F; H[6] += G; H[7] += Hh;
	}

	/** Bytes from the OS's crypto random source. */
	bool RandomBytes(uint8* Out, int32 Num)
	{
#if PLATFORM_WINDOWS
		return BCryptGenRandom(nullptr, Out, ULONG(Num), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
		TUniquePtr<FArchive> In(IFileManager::Get().CreateFileReader(TEXT("/dev/urandom")));
		if (!In) return false;
		In->Serialize(Out, Num);
		return !In->IsError();
#endif
	}

	FString KeyPath() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Identity"), TEXT("key.bin")); }
}

FString WRIdentity::Sha256Hex(const uint8* Data, int32 Num)
{
	using namespace WRIdentityImpl;
	uint32 H[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	TArray<uint8> Msg(Data, Num);
	const uint64 Bits = uint64(Num) * 8;
	Msg.Add(0x80);
	while (Msg.Num() % 64 != 56) Msg.Add(0);
	for (int32 i = 7; i >= 0; --i) Msg.Add(uint8(Bits >> (i * 8)));
	for (int32 At = 0; At < Msg.Num(); At += 64) Block(H, Msg.GetData() + At);
	FString Hex;
	for (uint32 Word : H) Hex += FString::Printf(TEXT("%08x"), Word);
	return Hex;
}

FString WRIdentity::Sha256Hex(const FString& Text)
{
	const FTCHARToUTF8 Utf8(*Text);
	return Sha256Hex(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

FString WRIdentity::Token()
{
	using namespace WRIdentityImpl;
	static FString Cached;
	if (!Cached.IsEmpty()) return Cached;
	TArray<uint8> Key;
	if (!FFileHelper::LoadFileToArray(Key, *KeyPath(), FILEREAD_Silent) || Key.Num() != 32)
	{
		Key.SetNumZeroed(32);
		if (!RandomBytes(Key.GetData(), 32))
		{
			UE_LOG(LogWildreach, Error, TEXT("IDENTITY no crypto random source: this PC joins without a key (the names it uses stay unclaimed)"));
			return FString();
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(KeyPath()), true);
		if (!FFileHelper::SaveArrayToFile(Key, *KeyPath()))
		{
			UE_LOG(LogWildreach, Error, TEXT("IDENTITY can't write %s: this PC joins without a key"), *KeyPath());
			return FString();
		}
		UE_LOG(LogWildreach, Log, TEXT("IDENTITY made this PC's key (Saved/Identity/key.bin): it's what keeps your characters' names yours"));
	}
	static const char Salt[] = "wildreach-identity-1";
	Key.Append(reinterpret_cast<const uint8*>(Salt), int32(sizeof(Salt)) - 1);
	Cached = Sha256Hex(Key.GetData(), Key.Num());
	return Cached;
}

FString WRIdentity::Check(const FString& Name, const FString& InToken, const FString& Platform, const FString& PlatformId)
{
	if (!WRStore::IsSql() || Name.IsEmpty()) return FString();
	WRStore::FAccount A;
	if (!WRStore::ReadAccount(Name, A) || A.KeyHash.IsEmpty())
	{
		return FString();   // a free name, or one from before identity: the first key claims it (Bind)
	}
	if (!PlatformId.IsEmpty() && A.Platform == Platform && A.PlatformId == PlatformId)
	{
		return FString();   // the same Steam / EOS account, from any PC
	}
	if (!InToken.IsEmpty() && Sha256Hex(InToken + WRWorlds::Current()) == A.KeyHash)
	{
		return FString();
	}
	UE_LOG(LogWildreach, Warning, TEXT("IDENTITY refused '%s': %s"), *Name, InToken.IsEmpty() ? TEXT("no key") : TEXT("another PC's key"));
	return FString::Printf(TEXT("The name %s belongs to someone else on this world: pick another name."), *Name);
}

void WRIdentity::Bind(const FString& Name, const FString& InToken, const FString& Platform, const FString& PlatformId, const FString& Address)
{
	if (!WRStore::IsSql() || Name.IsEmpty()) return;
	WRStore::FAccount A;
	const bool bKnown = WRStore::ReadAccount(Name, A);
	const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
	if (!bKnown)
	{
		A.Name = Name;
		A.FirstSeen = Now;
	}
	if (!InToken.IsEmpty())
	{
		const FString Hash = Sha256Hex(InToken + WRWorlds::Current());
		// Claimed by the first key; moved to a new PC only through a matching platform account (Check let it in).
		const bool bSamePlatform = !PlatformId.IsEmpty() && A.Platform == Platform && A.PlatformId == PlatformId;
		if (A.KeyHash.IsEmpty() || (bSamePlatform && A.KeyHash != Hash))
		{
			UE_LOG(LogWildreach, Log, TEXT("IDENTITY '%s' %s"), *Name, A.KeyHash.IsEmpty() ? TEXT("claimed by this PC's key") : TEXT("moved to a new PC (same platform account)"));
			A.KeyHash = Hash;
		}
	}
	if (!PlatformId.IsEmpty() && A.PlatformId.IsEmpty())
	{
		A.Platform = Platform;
		A.PlatformId = PlatformId;
		UE_LOG(LogWildreach, Log, TEXT("IDENTITY '%s' bound to its %s account"), *Name, *Platform);
	}
	A.LastSeen = Now;
	A.LastAddress = Address;
	WRStore::WriteAccount(A);
}
