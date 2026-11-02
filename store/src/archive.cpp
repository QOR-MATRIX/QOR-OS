// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "archive.h"

#include "catalog.h"
#include "package.h"

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QtEndian>

using namespace Qt::StringLiterals;

namespace qor::store {

namespace {

constexpr qint64 kEndSize = 22;
constexpr qint64 kCentralSize = 46;
constexpr qint64 kLocalSize = 30;
constexpr quint32 kEndSignature = 0x06054b50;
constexpr quint32 kCentralSignature = 0x02014b50;
constexpr quint32 kLocalSignature = 0x04034b50;
constexpr int kMaximumEntries = 4096;

constexpr quint16 kUtf8Names = 0x0800; // the only flag allowed: QZipWriter sets it, and every name here is ASCII
constexpr quint16 kStored = 0;

// The system that made an entry, which says how to read its attributes. Anything else is not something we write.
constexpr int kHostFat = 0;
constexpr int kHostUnix = 3;
constexpr int kHostNtfs = 10;
constexpr quint32 kUnixTypeMask = 0170000;
constexpr quint32 kUnixRegular = 0100000;
constexpr quint32 kDosDirectory = 0x10;

ArchiveResult failure(const QString &error)
{
    ArchiveResult result;
    result.error = error;
    return result;
}

ArchiveResult notReadable(const QString &reason)
{
    return failure(u"The package is not a readable archive: %1."_s.arg(reason));
}

ArchiveResult disagrees(const QString &path, const QString &what)
{
    return failure(
        u"The package's entry %1 says a different %2 in its own header and in the directory."_s.arg(path, what));
}

} // namespace

QString findPathCollision(const QStringList &paths)
{
    QSet<QString> files;
    QHash<QString, QString> folders; // lower case → as first spelled
    for (const QString &path : paths) {
        const QString folded = path.toLower();
        if (files.contains(folded) || folders.contains(folded))
            return path;
        files.insert(folded);
        for (qsizetype slash = path.indexOf(u'/'); slash >= 0; slash = path.indexOf(u'/', slash + 1)) {
            const QString folder = path.left(slash);
            const QString foldedFolder = folded.left(slash);
            if (files.contains(foldedFolder))
                return path;
            const auto known = folders.constFind(foldedFolder);
            if (known != folders.cend() && *known != folder)
                return path;
            folders.insert(foldedFolder, folder);
        }
    }
    return {};
}

ArchiveResult readStoredArchive(const QByteArray &archive, qint64 maximumUnpacked)
{
    const qint64 size = archive.size();
    if (size < kEndSize)
        return notReadable(u"it is too short to be one"_s);
    const char *bytes = archive.constData();
    const auto u16 = [bytes](qint64 at) { return qFromLittleEndian<quint16>(bytes + at); };
    const auto u32 = [bytes](qint64 at) { return qFromLittleEndian<quint32>(bytes + at); };

    // The end record is the last thing in the file, with no comment after it, so there is one place to find it.
    const qint64 end = size - kEndSize;
    if (u32(end) != kEndSignature)
        return notReadable(u"it does not finish with an end record"_s);
    if (u16(end + 20) != 0)
        return notReadable(u"it says it has a comment"_s);
    if (u16(end + 4) != 0 || u16(end + 6) != 0)
        return notReadable(u"it says it spans disks"_s);
    const int count = u16(end + 10);
    if (u16(end + 8) != count)
        return notReadable(u"its two counts of entries disagree"_s);
    if (count > kMaximumEntries)
        return failure(u"The package holds too many entries."_s);
    const qint64 centralSize = u32(end + 12);
    const qint64 centralStart = u32(end + 16);
    if (centralStart + centralSize != end)
        return notReadable(u"its directory does not finish where the end record starts"_s);

    ArchiveResult result;
    QStringList paths;
    qint64 at = centralStart;
    qint64 nextLocal = 0;
    for (int i = 0; i < count; ++i) {
        if (at + kCentralSize > end)
            return notReadable(u"its directory finishes before its count of entries does"_s);
        if (u32(at) != kCentralSignature)
            return notReadable(u"a directory entry has the wrong signature"_s);
        const int host = u16(at + 4) >> 8;
        const quint16 flags = u16(at + 8);
        const quint16 method = u16(at + 10);
        const quint32 crc = u32(at + 16);
        const qint64 packed = u32(at + 20);
        const qint64 unpacked = u32(at + 24);
        const qint64 nameLength = u16(at + 28);
        const qint64 extraLength = u16(at + 30);
        const qint64 commentLength = u16(at + 32);
        const quint16 diskStart = u16(at + 34);
        const quint32 attributes = u32(at + 38);
        const qint64 local = u32(at + 42);
        if (at + kCentralSize + nameLength + extraLength + commentLength > end)
            return notReadable(u"a directory entry runs past the directory"_s);
        const QByteArrayView name(bytes + at + kCentralSize, nameLength);
        at += kCentralSize + nameLength + extraLength + commentLength;

        // Every name is ASCII or it is unsafe, so this reading of the bytes is the only one there is.
        const QString path = QString::fromLatin1(name);
        if (!isSafePackagePath(path))
            return failure(u"The package contains an unsafe entry: %1"_s.arg(printable(path)));
        if (host != kHostFat && host != kHostUnix && host != kHostNtfs)
            return failure(u"The package contains an unsafe entry, from a system it does not know: %1"_s.arg(path));
        const quint32 unixType = (attributes >> 16) & kUnixTypeMask;
        if (unixType != 0 && unixType != kUnixRegular)
            return failure(u"The package contains an unsafe entry, a link or a folder: %1"_s.arg(path));
        if ((attributes & kDosDirectory) != 0)
            return failure(u"The package contains an unsafe entry, marked as a folder: %1"_s.arg(path));
        paths.append(path);

        if (method != kStored)
            return failure(u"The package holds a compressed entry, and only stored ones are read: %1"_s.arg(path));
        if ((flags & ~kUtf8Names) != 0)
            return failure(u"The package's entry %1 has a flag a package may not."_s.arg(path));
        if (extraLength != 0)
            return failure(u"The package's entry %1 has an extra field in the directory."_s.arg(path));
        if (commentLength != 0)
            return failure(u"The package's entry %1 has a comment."_s.arg(path));
        if (diskStart != 0)
            return failure(u"The package's entry %1 says it starts on another disk."_s.arg(path));
        if (packed != unpacked)
            return failure(u"The package's entry %1 claims a size it does not hold."_s.arg(path));

        // What the entry's own header says must be what the directory says: two readers of one file get one answer.
        if (local != nextLocal)
            return failure(u"The package's entries overlap or leave a gap, at %1."_s.arg(path));
        if (local + kLocalSize > centralStart)
            return notReadable(u"an entry's header runs past the entries"_s);
        if (u32(local) != kLocalSignature)
            return notReadable(u"an entry's header has the wrong signature"_s);
        if (u16(local + 6) != flags)
            return disagrees(path, u"flags"_s);
        if (u16(local + 8) != method)
            return disagrees(path, u"method"_s);
        if (u32(local + 14) != crc)
            return disagrees(path, u"CRC"_s);
        if (u32(local + 18) != packed)
            return disagrees(path, u"stored size"_s);
        if (u32(local + 22) != unpacked)
            return disagrees(path, u"unpacked size"_s);
        if (u16(local + 26) != nameLength)
            return disagrees(path, u"name length"_s);
        if (u16(local + 28) != 0)
            return failure(u"The package's entry %1 has an extra field in its own header."_s.arg(path));
        if (local + kLocalSize + nameLength > centralStart)
            return notReadable(u"an entry's name runs past the entries"_s);
        if (QByteArrayView(bytes + local + kLocalSize, nameLength) != name)
            return disagrees(path, u"name"_s);

        const qint64 data = local + kLocalSize + nameLength;
        if (data + packed > centralStart)
            return notReadable(u"an entry's bytes run past the entries"_s);
        // Counted from where the bytes lie. No two entries share a byte, so nothing is counted but what is here.
        result.unpacked += packed;
        if (result.unpacked > maximumUnpacked)
            return failure(u"The package is too large."_s);
        result.files.insert(path, archive.mid(data, packed));
        nextLocal = data + packed;
    }
    if (at != end)
        return notReadable(u"its directory holds more than its count of entries"_s);
    if (nextLocal != centralStart)
        return failure(u"The package's entries overlap or leave a gap, after the last."_s);
    // Asked once, of all the names: asked for each entry, it would cost a hostile archive's many entries squared.
    if (const QString collision = findPathCollision(paths); !collision.isEmpty())
        return failure(u"The package has two entries that would land on one file: %1"_s.arg(collision));

    result.ok = true;
    return result;
}

} // namespace qor::store
