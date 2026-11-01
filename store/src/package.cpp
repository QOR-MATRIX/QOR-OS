// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "package.h"

#include "archive.h"
#include "catalog.h"
#include "filerules.h"
#include "signature.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

using namespace Qt::StringLiterals;

namespace qor::store {

namespace {

const QString kManifest = u"manifest.json"_s;
const QString kSignature = u"manifest.sig"_s;
constexpr qint64 kMaximumManifest = 4 * 1024 * 1024;

PackageResult failure(const QString &error)
{
    PackageResult result;
    result.error = error;
    return result;
}

bool isDeviceName(const QString &part)
{
    static const QRegularExpression pattern(u"^(con|prn|aux|nul|com[0-9]|lpt[0-9])(\\.|$)"_s,
                                            QRegularExpression::CaseInsensitiveOption);
    return pattern.match(part).hasMatch();
}

// Reads a manifest's fields. Returns false if it does not hold what its format and kind require.
bool readManifest(const QJsonObject &manifest, PackageManifest *m)
{
    m->format = manifest.value(u"format"_s).toInt();
    m->id = manifest.value(u"id"_s).toString();
    m->kind = manifest.value(u"kind"_s).toString();
    m->version = manifest.value(u"version"_s).toString();
    m->name = manifest.value(u"name"_s).toString();
    m->developer = manifest.value(u"developer"_s).toString();
    if (!isValidItemId(m->id) || !isValidVersion(m->version) || m->developer.isEmpty())
        return false;
    if (m->format == 1)
        return m->kind == u"theme"_s || m->kind == u"widget"_s;
    if (m->format != 2 || !isKnownKind(m->kind))
        return false;

    static const QSet<QString> known { u"format"_s,  u"id"_s,           u"kind"_s,   u"version"_s,
                                       u"name"_s,    u"developer"_s,    u"stores"_s, u"edition"_s,
                                       u"licence"_s, u"dependencies"_s, u"files"_s };
    for (auto it = manifest.begin(); it != manifest.end(); ++it)
        if (!known.contains(it.key()))
            return false;

    const QJsonArray stores = manifest.value(u"stores"_s).toArray();
    if (stores.isEmpty() || stores.size() > 2)
        return false;
    for (const QJsonValue &store : stores) {
        const QString name = store.toString();
        if ((name != u"player"_s && name != u"creator"_s)
            || !kindMayBePlacedIn(m->kind, name == u"player"_s ? View::Player : View::Creator))
            return false;
        m->stores.append(name);
    }
    m->stores.sort();
    if (m->stores.removeDuplicates() != 0)
        return false;

    if (manifest.contains(u"edition"_s) && !parseEdition(manifest.value(u"edition"_s), m->kind, &m->edition).isEmpty())
        return false;
    return parseLicence(manifest.value(u"licence"_s), &m->licence).isEmpty()
           && parseDependencies(manifest.value(u"dependencies"_s), &m->dependencies).isEmpty();
}

} // namespace

bool isSafePackagePath(const QString &path)
{
    static const QRegularExpression pattern(u"^[A-Za-z0-9_][A-Za-z0-9._-]*(/[A-Za-z0-9_][A-Za-z0-9._-]*)*$"_s);
    if (path.size() > 200 || !pattern.match(path).hasMatch() || path.contains(u".."_s))
        return false;
    for (const QStringView part : QStringView(path).split(u'/')) {
        // Windows drops a trailing dot, so "a." would land on "a". A trailing space is refused by the pattern above.
        if (part.endsWith(u'.') || isDeviceName(part.toString()))
            return false;
    }
    return true;
}

PackageResult openPackage(const QByteArray &archive, const QByteArray &developerKey, qint64 maximumSize)
{
    const ArchiveResult read = readStoredArchive(archive, maximumSize);
    if (!read.ok)
        return failure(read.error);

    const QByteArray manifestBytes = read.files.value(kManifest);
    const QByteArray signature = read.files.value(kSignature);
    if (manifestBytes.isEmpty() || signature.isEmpty())
        return failure(u"The package has no signed manifest."_s);
    if (manifestBytes.size() > kMaximumManifest)
        return failure(u"The package's manifest is too large."_s);
    if (!verifySignature(developerKey, manifestBytes, signature))
        return failure(u"The package's developer signature does not match."_s);

    const QJsonObject manifest = QJsonDocument::fromJson(manifestBytes).object();
    PackageResult result;
    PackageManifest &m = result.manifest;
    if (!readManifest(manifest, &m))
        return failure(u"The package manifest is not valid."_s);

    const QJsonObject files = manifest.value(u"files"_s).toObject();
    for (auto it = files.begin(); it != files.end(); ++it) {
        const QByteArray digest = QByteArray::fromHex(it.value().toString().toLatin1());
        if (!isSafePackagePath(it.key()) || it.key() == kManifest || it.key() == kSignature || digest.size() != 32)
            return failure(u"The package manifest lists an invalid file: %1"_s.arg(printable(it.key())));
        m.files.insert(it.key(), digest);
    }

    // Exactly the listed files, each matching its digest.
    QSet<QString> entries(read.files.keyBegin(), read.files.keyEnd());
    QSet<QString> expected(m.files.keyBegin(), m.files.keyEnd());
    expected << kManifest << kSignature;
    if (entries != expected)
        return failure(u"The package's files do not match its manifest."_s);
    for (auto it = m.files.cbegin(); it != m.files.cend(); ++it) {
        const QByteArray data = read.files.value(it.key());
        if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != it.value())
            return failure(u"%1 does not match the package manifest."_s.arg(it.key()));
        result.contents.insert(it.key(), data);
    }

    if (m.format == 2 && !result.contents.contains(m.licence.file))
        return failure(u"The package does not hold its licence's file, %1."_s.arg(m.licence.file));
    if (m.format == 2) {
        if (const QString why = refusePackageFiles(m.kind, result.contents); !why.isEmpty())
            return failure(why);
    }

    result.manifestBytes = manifestBytes;
    result.signature = signature;
    result.unpacked = read.unpacked;
    result.ok = true;
    return result;
}

bool verifyInstalled(const QString &folder, const QByteArray &developerKey, PackageManifest *manifest)
{
    const auto read = [&folder](const QString &name) {
        QFile file(folder + u'/' + name);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    const QByteArray manifestBytes = read(kManifest);
    const QByteArray signature = read(kSignature);
    if (manifestBytes.isEmpty() || signature.isEmpty() || !verifySignature(developerKey, manifestBytes, signature))
        return false;
    const QJsonObject object = QJsonDocument::fromJson(manifestBytes).object();
    const QJsonObject files = object.value(u"files"_s).toObject();
    QSet<QString> expected { kManifest, kSignature };
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (!isSafePackagePath(it.key()))
            return false;
        const QByteArray digest = QByteArray::fromHex(it.value().toString().toLatin1());
        if (digest.size() != 32 || QCryptographicHash::hash(read(it.key()), QCryptographicHash::Sha256) != digest)
            return false;
        expected.insert(it.key());
    }
    // Nothing added: not a qmldir, nor a plugin for one to name.
    QSet<QString> found;
    const QDir root(folder);
    QDirIterator walk(folder, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                      QDirIterator::Subdirectories);
    while (walk.hasNext())
        found.insert(root.relativeFilePath(walk.next()));
    if (found != expected)
        return false;
    if (manifest && !readManifest(object, manifest))
        return false;
    return true;
}

} // namespace qor::store
