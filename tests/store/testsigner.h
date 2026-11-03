// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QByteArray>

// A P-256 key made for one test and dropped with it: it never reaches a file, so a suite can sign whatever hostile
// catalogue or package it needs, beside the assertion that refuses it, with nothing to keep out of the repository.
class TestSigner
{
public:
    TestSigner();
    ~TestSigner();
    TestSigner(const TestSigner &) = delete;
    TestSigner &operator=(const TestSigner &) = delete;

    // The 65-byte uncompressed point, as a catalogue lists a developer's key and QOR OS holds the store's.
    QByteArray publicKey() const { return m_public; }
    // r ‖ s, 64 bytes, as qor::store::verifySignature takes it.
    QByteArray sign(const QByteArray &message) const;

private:
    void *m_key = nullptr;
    QByteArray m_public;
};
