// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace qor::store {

struct ArchiveResult
{
    bool ok = false;
    QString error;
    QMap<QString, QByteArray> files; // path → the bytes it holds
    qint64 unpacked = 0;             // what those bytes come to, measured
};

// Reads a package's zip archive, and only the plainest kind of one, the kind tools/store/build_store.py writes: entries
// stored, never compressed, one after another from the first byte, each named once, then the central directory and
// nothing after it. It has no way to inflate anything, so nothing here can grow past the archive itself, and `unpacked`
// is counted from where each entry's bytes actually lie, never from what a header claims.
//
// Refused: a compressed or encrypted entry; a size that differs between an entry's headers, or an entry whose stored
// and unpacked sizes differ; entries that overlap or leave a gap; anything before the first entry or after the end;
// a symbolic link or a folder; an unsafe name (isSafePackagePath); names that would land on one file on a system that
// ignores case; more than `maximumUnpacked` bytes in all.
//
// An entry's CRC is not checked: the signed manifest's SHA-256 of every file is the check that counts, and is made
// next.
ArchiveResult readStoredArchive(const QByteArray &archive, qint64 maximumUnpacked);

// The first path in `paths` that would land on the same file or folder as an earlier one on a system that ignores case,
// or that is a file where another makes it a folder. Empty if none does.
QString findPathCollision(const QStringList &paths);

} // namespace qor::store
