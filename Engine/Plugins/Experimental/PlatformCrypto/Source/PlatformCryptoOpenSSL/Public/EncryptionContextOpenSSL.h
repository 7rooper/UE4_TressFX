// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "PlatformCryptoTypes.h"

/**
 * Interface to certain cryptographic algorithms, using OpenSSL to implement them.
 */
class PLATFORMCRYPTOOPENSSL_API FEncryptionContextOpenSSL
{

public:

	TArray<uint8> Encrypt_AES_256_ECB(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);

	TArray<uint8> Decrypt_AES_256_ECB(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);

	bool DigestVerify_PS256(const TArrayView<const char> Message, const TArrayView<const uint8> Signature, const TArrayView<const uint8> PKCS1Key);

	TPlatformCryptoRSAKey CreateKey_RSA(const TArrayView<const uint8> PublicExponent, const TArrayView<const uint8> PrivateExponent, const TArrayView<const uint8> Modulus);
	void DestroyKey_RSA(TPlatformCryptoRSAKey Key);
	int32 GetKeySize_RSA(TPlatformCryptoRSAKey Key);
	int32 GetMaxDataSize_RSA(TPlatformCryptoRSAKey Key);
	int32 EncryptPublic_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, TPlatformCryptoRSAKey Key);
	int32 EncryptPrivate_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, TPlatformCryptoRSAKey Key);
	int32 DecryptPublic_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, TPlatformCryptoRSAKey Key);
	int32 DecryptPrivate_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, TPlatformCryptoRSAKey Key);
};

typedef FEncryptionContextOpenSSL FEncryptionContext;
