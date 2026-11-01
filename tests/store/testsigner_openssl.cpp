// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "testsigner.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>

TestSigner::TestSigner()
{
    EVP_PKEY *key = EVP_EC_gen("P-256");
    Q_ASSERT(key);
    m_key = key;
    unsigned char *point = nullptr;
    const size_t length = EVP_PKEY_get1_encoded_public_key(key, &point);
    m_public = QByteArray(reinterpret_cast<const char *>(point), qsizetype(length));
    OPENSSL_free(point);
}

TestSigner::~TestSigner() { EVP_PKEY_free(static_cast<EVP_PKEY *>(m_key)); }

QByteArray TestSigner::sign(const QByteArray &message) const
{
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    EVP_DigestSignInit(context, nullptr, EVP_sha256(), nullptr, static_cast<EVP_PKEY *>(m_key));
    const auto *bytes = reinterpret_cast<const unsigned char *>(message.constData());
    size_t length = 0;
    EVP_DigestSign(context, nullptr, &length, bytes, size_t(message.size()));
    QByteArray der(qsizetype(length), '\0');
    EVP_DigestSign(context, reinterpret_cast<unsigned char *>(der.data()), &length, bytes, size_t(message.size()));
    EVP_MD_CTX_free(context);

    // OpenSSL hands it over DER-encoded.
    const auto *cursor = reinterpret_cast<const unsigned char *>(der.constData());
    ECDSA_SIG *signature = d2i_ECDSA_SIG(nullptr, &cursor, long(length));
    QByteArray raw(64, '\0');
    BN_bn2binpad(ECDSA_SIG_get0_r(signature), reinterpret_cast<unsigned char *>(raw.data()), 32);
    BN_bn2binpad(ECDSA_SIG_get0_s(signature), reinterpret_cast<unsigned char *>(raw.data()) + 32, 32);
    ECDSA_SIG_free(signature);
    return raw;
}
