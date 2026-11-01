// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "testsigner.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <bcrypt.h>

#include <QtCore/QCryptographicHash>

TestSigner::TestSigner()
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0);
    BCRYPT_KEY_HANDLE key = nullptr;
    BCryptGenerateKeyPair(algorithm, &key, 256, 0);
    BCryptFinalizeKeyPair(key, 0);
    m_key = key;

    // A CNG public key blob is its header, then X and Y.
    ULONG size = 0;
    BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0);
    QByteArray blob(qsizetype(size), '\0');
    BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, reinterpret_cast<PUCHAR>(blob.data()), size, &size, 0);
    m_public = QByteArray(1, '\x04') + blob.mid(qsizetype(sizeof(BCRYPT_ECCKEY_BLOB)), 64);
    BCryptCloseAlgorithmProvider(algorithm, 0);
}

TestSigner::~TestSigner() { BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(m_key)); }

QByteArray TestSigner::sign(const QByteArray &message) const
{
    QByteArray digest = QCryptographicHash::hash(message, QCryptographicHash::Sha256);
    QByteArray raw(64, '\0');
    ULONG length = 0;
    BCryptSignHash(static_cast<BCRYPT_KEY_HANDLE>(m_key), nullptr, reinterpret_cast<PUCHAR>(digest.data()),
                   ULONG(digest.size()), reinterpret_cast<PUCHAR>(raw.data()), ULONG(raw.size()), &length, 0);
    return raw;
}
