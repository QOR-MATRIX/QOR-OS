// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "assetlibrary.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMutex>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringDecoder>
#include <QUrl>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <memory>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "confinement.h"
#include "projectworkspace.h"

namespace qq {

namespace {

constexpr int journalFormat = 1;
const QLatin1StringView stagingPrefix(".qq-assets-");

QString tr(const char *text)
{
    return AssetLibrary::tr(text);
}

QVariantMap refused(const QString &problem)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("problem"), problem}};
}

bool isValidId(const QString &id)
{
    // As the Store names an item (store/src/catalog.cpp), so a folder named by it is a plain name.
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9]*(\\.[a-z0-9][a-z0-9-]*){2,}\\z"));
    return id.size() <= 128 && pattern.match(id).hasMatch();
}

bool isValidVersion(const QString &version)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d{1,5}\\.\\d{1,5}\\.\\d{1,5}\\z"));
    return pattern.match(version).hasMatch();
}

int compareVersions(const QString &a, const QString &b)
{
    const QStringList left = a.split(QLatin1Char('.'));
    const QStringList right = b.split(QLatin1Char('.'));
    for (qsizetype i = 0; i < std::max(left.size(), right.size()); ++i) {
        const int x = left.value(i).toInt();
        const int y = right.value(i).toInt();
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

bool isValidSpdx(const QString &spdx)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9.+()\\- ]{1,128}\\z"));
    return pattern.match(spdx).hasMatch() && spdx != QStringLiteral("NOASSERTION") && spdx != QStringLiteral("NONE");
}

/// Text that says what it looks like: in NFC, so one name is one file, and with no control or format characters (Unicode's
/// Cc and Cf: no right-to-left override, no zero-width joiner), which could make a name, a developer or a path look like another.
bool plainUnicode(const QString &text)
{
    if (text != text.normalized(QString::NormalizationForm_C))
        return false;
    const QList<uint> points = QStringView(text).toUcs4();
    return std::none_of(points.cbegin(), points.cend(), [](uint c) {
        const QChar::Category category = QChar::category(char32_t(c));
        return category == QChar::Other_Control || category == QChar::Other_Format;
    });
}

/// Plain text a person reads: a string, short, plain Unicode.
bool isPlainText(const QJsonValue &value, qsizetype longest)
{
    if (!value.isString())
        return false;
    const QString text = value.toString();
    return text.size() <= longest && plainUnicode(text);
}

QByteArray fromHex(const QJsonValue &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}\\z"));
    return value.isString() && pattern.match(value.toString()).hasMatch() ? QByteArray::fromHex(value.toString().toLatin1())
                                                                          : QByteArray();
}

QByteArray sha256(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

/// A path in a pack that is safe to copy into a project on every system and that a build carries: plain, no name a system
/// keeps for a device, nothing named with a dot.
bool safePackPath(const QString &path)
{
    if (!ProjectWorkspace::plainRelative(path) || path.size() > 512 || !plainUnicode(path))
        return false;
    static const QRegularExpression device(QStringLiteral("^(con|prn|aux|nul|com[0-9]|lpt[0-9])(\\..*)?\\z"),
                                           QRegularExpression::CaseInsensitiveOption);
    for (const QString &part : path.split(QLatin1Char('/'))) {
        if (part.startsWith(QLatin1Char('.')) || device.match(part).hasMatch())
            return false;
    }
    return true;
}

/// Read a plain file under `root`, opened a name at a time with no link followed at any step and no pipe waited on, and no
/// more than `largest` bytes of it: false, with why, if it cannot be. On Windows each part is looked at before it is opened.
bool readPlain(const QString &root, const QString &relative, qint64 largest, QByteArray *bytes, QString *why = nullptr)
{
    const auto fail = [why](const QString &reason) {
        if (why)
            *why = reason;
        return false;
    };
    QFile file;
#ifdef Q_OS_UNIX
    int folder = ::open(QFile::encodeName(root).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const QStringList parts = relative.split(QLatin1Char('/'));
    for (qsizetype i = 0; folder >= 0 && i + 1 < parts.size(); ++i) {
        const int next = ::openat(folder, QFile::encodeName(parts.at(i)).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(folder);
        folder = next;
    }
    if (folder < 0)
        return fail(tr("%1 cannot be reached, or the way to it is a link.").arg(relative));
    const int opened = ::openat(folder, QFile::encodeName(parts.last()).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    ::close(folder);
    if (opened < 0)
        return fail(tr("%1 cannot be opened, or is a link.").arg(relative));
    struct stat seen{};
    if (::fstat(opened, &seen) != 0 || !S_ISREG(seen.st_mode)) {
        ::close(opened);
        return fail(tr("%1 is not a plain file.").arg(relative));
    }
    if (seen.st_size > largest) {
        ::close(opened);
        return fail(tr("%1 is larger than it may be.").arg(relative));
    }
    if (!file.open(opened, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(opened);
        return fail(tr("%1 cannot be read.").arg(relative));
    }
#else
    QString walked = root;
    for (const QString &part : relative.split(QLatin1Char('/'))) {
        walked += QLatin1Char('/') + part;
        const QFileInfo step(walked);
        if (step.isSymLink() || step.isJunction() || step.isShortcut())
            return fail(tr("%1 cannot be reached, or the way to it is a link.").arg(relative));
    }
    const QFileInfo info(walked);
    if (!info.isFile())
        return fail(tr("%1 is not a plain file.").arg(relative));
    if (info.size() > largest)
        return fail(tr("%1 is larger than it may be.").arg(relative));
    file.setFileName(walked);
    if (!file.open(QIODevice::ReadOnly))
        return fail(tr("%1 cannot be read.").arg(relative));
#endif
    // As large as it was seen to be, and no more: a read is not given room for what the largest could be. One that grew
    // meanwhile is not taken whole, so it is not taken.
#ifdef Q_OS_UNIX
    const qint64 size = seen.st_size;
#else
    const qint64 size = info.size();
#endif
    *bytes = file.read(qMin(largest, size) + 1);
    if (bytes->size() > largest)
        return fail(tr("%1 is larger than it may be.").arg(relative));
    if (bytes->size() > size || !file.atEnd())
        return fail(tr("%1 changed while it was read.").arg(relative));
    return true;
}

/// What is wrong with the way to `relative` under `root`, if anything: a link or a junction at any step, or something that
/// is neither a folder on the way nor a plain file at the end. What is not there yet is no problem.
QString wayProblem(const QString &root, const QString &relative)
{
    QString walked = root;
    const QStringList parts = relative.split(QLatin1Char('/'));
    for (qsizetype i = 0; i < parts.size(); ++i) {
        walked += QLatin1Char('/') + parts.at(i);
        const QFileInfo step(walked);
        if (step.isSymLink() || step.isJunction() || step.isShortcut())
            return tr("%1 goes through a link, %2.").arg(relative, QDir(root).relativeFilePath(walked));
        if (!step.exists())
            return {};
        if (i + 1 < parts.size() ? !step.isDir() : !step.isFile())
            return tr("%1 is in the way of %2.").arg(QDir(root).relativeFilePath(walked), relative);
    }
    return {};
}

/// A file's digest, read with no link followed: empty if it is not there or cannot be read, and one no file has if it is
/// larger than a pack's file could be, which is a change of the person's however it came.
QByteArray digestOf(const QString &root, const QString &relative)
{
    QByteArray bytes;
    QString why;
    if (readPlain(root, relative, AssetLibrary::largestPack, &bytes, &why))
        return sha256(bytes);
    return QFileInfo(root + QLatin1Char('/') + relative).isFile() ? QByteArray("larger than a pack's file") : QByteArray();
}

bool exists(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

void syncFolder([[maybe_unused]] const QString &path)
{
#ifdef Q_OS_UNIX
    const int folder = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (folder < 0)
        return;
    ::fsync(folder);
    ::close(folder);
#endif
}

/// Everything written under `root` so far, durable: on Linux by one syncfs() of the file system it is on, which takes every file
/// and folder at once; elsewhere on POSIX by an fsync() of every folder under it (each file is synced as it is written).
void makeDurable(const QString &root)
{
#ifdef Q_OS_LINUX
    const int folder = ::open(QFile::encodeName(root).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (folder >= 0) {
        ::syncfs(folder);
        ::close(folder);
        return;
    }
#endif
    QDirIterator folders(root, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (folders.hasNext())
        syncFolder(folders.next());
    syncFolder(root);
}

bool writeNew(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).path()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(bytes) != bytes.size() || !file.flush())
        return false;
#if defined(Q_OS_UNIX) && !defined(Q_OS_LINUX)
    if (::fsync(file.handle()) != 0)
        return false;
#endif
    return true;
}

// ── What a pack may hold ──

/// What is wrong with a file of this kind, by its first bytes and, for text, all of it: "" if nothing.
QString contentProblem(const QString &path, const QString &kind, const QByteArray &bytes)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    const auto starts = [&bytes](QByteArrayView head, qsizetype at = 0) { return bytes.mid(at, head.size()) == head; };
    bool fits = true;
    if (suffix == QStringLiteral("png"))
        fits = starts("\x89PNG\r\n\x1a\n");
    else if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg"))
        fits = starts("\xff\xd8\xff");
    else if (suffix == QStringLiteral("webp"))
        fits = starts("RIFF") && starts("WEBP", 8);
    else if (suffix == QStringLiteral("ktx"))
        fits = starts("\xabKTX 11\xbb\r\n\x1a\n");
    else if (suffix == QStringLiteral("ktx2"))
        fits = starts("\xabKTX 20\xbb\r\n\x1a\n");
    else if (suffix == QStringLiteral("hdr"))
        fits = starts("#?RADIANCE") || starts("#?RGBE");
    else if (suffix == QStringLiteral("glb"))
        fits = starts("glTF");
    else if (suffix == QStringLiteral("gltf"))
        fits = QJsonDocument::fromJson(bytes).isObject();
    else if (suffix == QStringLiteral("wav"))
        fits = starts("RIFF") && starts("WAVE", 8);
    else if (suffix == QStringLiteral("ogg"))
        fits = starts("OggS");
    else if (suffix == QStringLiteral("flac"))
        fits = starts("fLaC");
    else if (suffix == QStringLiteral("mp3"))
        fits = starts("ID3") || (bytes.size() > 1 && quint8(bytes.at(0)) == 0xff && (quint8(bytes.at(1)) & 0xe0) == 0xe0);
    if (!fits)
        return tr("%1 is not what its name says it is.").arg(path);
    if (kind == QStringLiteral("text")) {
        // UTF-8, with no control characters but tab and the breaks: a licence, a notice. Shown as plain text, never markup.
        QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
        const QString text = utf8(bytes);
        if (utf8.hasError())
            return tr("%1 is not text.").arg(path);
        for (const QChar c : text) {
            if ((c.unicode() < 0x20 && c != QLatin1Char('\t') && c != QLatin1Char('\n') && c != QLatin1Char('\r') && c != QLatin1Char('\f'))
                || c.unicode() == 0x7f)
                return tr("%1 is not plain text.").arg(path);
        }
    }
    return {};
}

void collectUris(const QJsonValue &value, QStringList &uris)
{
    if (value.isArray()) {
        for (const QJsonValue &each : value.toArray())
            collectUris(each, uris);
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == QStringLiteral("uri"))
                uris.append(it.value().isString() ? it.value().toString() : QStringLiteral("\x01"));
            else
                collectUris(it.value(), uris);
        }
    }
}

/// What is wrong with what a model names: every uri is data: of a picture or a buffer, or a file beside it in the pack.
QString modelProblem(const QString &path, const QByteArray &bytes, const QSet<QString> &held)
{
    QByteArray json = bytes;
    if (path.endsWith(QStringLiteral(".glb"), Qt::CaseInsensitive)) {
        // A binary glTF: a header of 12 bytes, then its JSON chunk first.
        if (bytes.size() < 20 || qFromLittleEndian<quint32>(bytes.constData() + 4) != 2)
            return tr("%1 is not a glTF 2.0 model.").arg(path);
        const quint32 length = qFromLittleEndian<quint32>(bytes.constData() + 12);
        if (qFromLittleEndian<quint32>(bytes.constData() + 16) != 0x4E4F534Au || length > quint32(bytes.size() - 20))
            return tr("%1 is not a glTF 2.0 model.").arg(path);
        json = bytes.mid(20, qsizetype(length));
    }
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject())
        return tr("%1 is not a glTF 2.0 model.").arg(path);
    QStringList uris;
    collectUris(document.object(), uris);
    // Two in one object, which a parser keeps only one of and another reader may not: the text holds more than were read.
    if (json.count("\"uri\"") != uris.size())
        return tr("%1 names a file twice in one place, or hides one.").arg(path);
    static const QStringList allowed{QStringLiteral("data:image/png;base64,"), QStringLiteral("data:image/jpeg;base64,"),
                                     QStringLiteral("data:image/webp;base64,"), QStringLiteral("data:image/ktx2;base64,"),
                                     QStringLiteral("data:application/octet-stream;base64,"),
                                     QStringLiteral("data:application/gltf-buffer;base64,")};
    const QString folder = QFileInfo(path).path();
    for (const QString &uri : std::as_const(uris)) {
        if (uri.startsWith(QStringLiteral("data:"))) {
            if (std::none_of(allowed.cbegin(), allowed.cend(), [&uri](const QString &kind) { return uri.startsWith(kind); }))
                return tr("%1 carries data of a kind a pack may not hold.").arg(path);
            continue;
        }
        const QString named = QUrl::fromPercentEncoding(uri.toUtf8());
        const QString inPack = folder == QStringLiteral(".") ? named : folder + QLatin1Char('/') + named;
        if (!safePackPath(named.split(QLatin1Char('/')).last()) || named.contains(QLatin1Char(':')) || named.contains(QLatin1Char('\\'))
            || QDir::cleanPath(inPack) != inPack || !held.contains(inPack))
            return tr("%1 names %2, which is not beside it in the pack.").arg(path, uri.left(200));
    }
    return {};
}

QString packProblem(const QString &id, const QString &reason)
{
    return QStringLiteral("%1: %2").arg(id, reason);
}

/// What the files of a pack are on disk, each by device, inode, size, and modification and change times (a change time cannot be
/// set back), the manifest and its signature among them; or empty where that cannot be told, and then nothing is remembered.
QByteArray diskSignature(const QString &folder, QStringList files)
{
#ifdef Q_OS_LINUX
    files.sort();
    files << QStringLiteral("manifest.json") << QStringLiteral("manifest.sig");
    QByteArray signature;
    signature.reserve(files.size() * 48);
    for (const QString &file : std::as_const(files)) {
        struct stat seen {};
        if (::lstat(QFile::encodeName(folder + QLatin1Char('/') + file).constData(), &seen) != 0)
            return {};
        const qint64 fields[] = {qint64(seen.st_dev), qint64(seen.st_ino), qint64(seen.st_size), qint64(seen.st_mtim.tv_sec),
                                 qint64(seen.st_mtim.tv_nsec), qint64(seen.st_ctim.tv_sec), qint64(seen.st_ctim.tv_nsec)};
        signature.append(reinterpret_cast<const char *>(fields), sizeof fields);
        signature.append(file.toUtf8()).append('\0');
    }
    return signature;
#else
    Q_UNUSED(folder)
    Q_UNUSED(files)
    return {};
#endif
}

struct Checked {
    QByteArray signature;
    AssetLibrary::Pack pack;
};

QMutex &checkedMutex()
{
    static QMutex mutex;
    return mutex;
}

/// Packs checked whole, by their folder. Read on the window's thread and the thumbnails', so under the mutex above.
QHash<QString, Checked> &checked()
{
    static QHash<QString, Checked> packs;
    return packs;
}

/// One installed pack, checked from nothing: its record's entry, its folder, its manifest and every file.
bool readPack(const QString &items, const QString &id, const QJsonObject &entry, AssetLibrary::Pack *pack, QString *why)
{
    const auto fail = [why](const QString &reason) {
        *why = reason;
        return false;
    };
    if (entry.value(QStringLiteral("claimed")).toBool())
        return fail(tr("the Store has not checked it since its record was lost: get it again from the Store."));
    const QString version = entry.value(QStringLiteral("version")).toString();
    const QString developer = entry.value(QStringLiteral("developer")).toString();
    if (!isValidVersion(version) || !isPlainText(entry.value(QStringLiteral("developer")), 128) || developer.isEmpty())
        return fail(tr("the Store's record of it is not valid."));

    const QString folder = items + QLatin1Char('/') + id;
    const QFileInfo place(folder);
    if (place.isSymLink() || place.isJunction() || place.isShortcut())
        return fail(tr("its folder is a link."));
    if (!place.isDir())
        return fail(tr("its folder is not there."));

    QByteArray manifestBytes;
    QString reason;
    if (!readPlain(folder, QStringLiteral("manifest.json"), AssetLibrary::largestManifest, &manifestBytes, &reason))
        return fail(reason);
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(manifestBytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return fail(tr("its manifest is not JSON."));
    const QJsonObject manifest = document.object();
    if (manifest.value(QStringLiteral("format")).toInt() != 2 || manifest.value(QStringLiteral("id")).toString() != id
        || manifest.value(QStringLiteral("kind")).toString() != QStringLiteral("asset-pack")
        || manifest.value(QStringLiteral("version")).toString() != version
        || manifest.value(QStringLiteral("developer")).toString() != developer)
        return fail(tr("its manifest does not say it is this pack, of this version, from this developer."));
    if (!isPlainText(manifest.value(QStringLiteral("name")), 256) || manifest.value(QStringLiteral("name")).toString().trimmed().isEmpty())
        return fail(tr("its manifest gives it no name."));
    const QJsonObject licence = manifest.value(QStringLiteral("licence")).toObject();
    if (!isValidSpdx(licence.value(QStringLiteral("spdx")).toString()))
        return fail(tr("its manifest names no licence."));

    QByteArray developerSignature;
    if (!readPlain(folder, QStringLiteral("manifest.sig"), 1024, &developerSignature, &reason) || developerSignature.size() != 64)
        return fail(tr("its signature is not there."));

    const QJsonObject listed = manifest.value(QStringLiteral("files")).toObject();
    if (listed.isEmpty() || listed.size() > AssetLibrary::mostFilesInAPack)
        return fail(tr("its manifest lists no files, or more than a pack may hold."));
    QHash<QString, QByteArray> digests;
    for (auto it = listed.begin(); it != listed.end(); ++it) {
        const QByteArray digest = fromHex(it.value());
        if (!safePackPath(it.key()) || AssetLibrary::kindOf(it.key()).isEmpty() || digest.isEmpty())
            return fail(tr("its manifest lists %1, which a pack may not hold.").arg(it.key().left(200)));
        digests.insert(it.key(), digest);
    }
    const QString licenceFile = licence.value(QStringLiteral("file")).toString();
    if (!digests.contains(licenceFile))
        return fail(tr("its licence's text is not among its files."));

    // Exactly what the manifest lists, and nothing else: not a file, a folder that leads to none, a link of any kind, a pipe.
    // The folders its files are in are the only folders it may hold, and the walk stops at as many entries as those and the files.
    QSet<QString> implied;
    for (auto it = digests.cbegin(); it != digests.cend(); ++it) {
        for (qsizetype slash = it.key().lastIndexOf(QLatin1Char('/')); slash > 0; slash = it.key().lastIndexOf(QLatin1Char('/'), slash - 1))
            implied.insert(it.key().left(slash));
    }
    const qsizetype mostEntries = digests.size() + implied.size() + 2;
    qsizetype entries = 0;
    QSet<QString> seen;
    QDirIterator walk(folder, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        walk.next();
        if (++entries > mostEntries)
            return fail(tr("it holds more than its manifest lists."));
        const QFileInfo found = walk.fileInfo();
        const QString relative = QDir(folder).relativeFilePath(found.filePath());
        if (found.isSymLink() || found.isJunction() || found.isShortcut())
            return fail(tr("it holds a link, %1.").arg(relative.left(200)));
        if (found.isDir()) {
            if (!implied.contains(relative))
                return fail(tr("it holds a folder its manifest does not list, %1.").arg(relative.left(200)));
            continue;
        }
        if (!found.isFile())
            return fail(tr("it holds %1, which is not a file.").arg(relative.left(200)));
        if (relative == QStringLiteral("manifest.json") || relative == QStringLiteral("manifest.sig"))
            continue;
        if (!digests.contains(relative))
            return fail(tr("it holds %1, which its manifest does not list.").arg(relative.left(200)));
        seen.insert(relative);
    }

    // Checked whole before, and nothing of it changed on disk since: what was found then stands. What is copied or shown is read
    // and its digest checked again whatever this says.
    const QByteArray signature = diskSignature(folder, digests.keys()) + QJsonDocument(entry).toJson(QJsonDocument::Compact);
    if (!signature.isEmpty()) {
        const QMutexLocker locker(&checkedMutex());
        const auto cached = checked().constFind(folder);
        if (cached != checked().cend() && cached->signature == signature) {
            *pack = cached->pack;
            return true;
        }
    }

    QSet<QString> held;
    for (auto it = digests.cbegin(); it != digests.cend(); ++it)
        held.insert(it.key());
    qint64 budget = AssetLibrary::largestPack;
    QList<AssetLibrary::PackFile> files;
    for (auto it = digests.cbegin(); it != digests.cend(); ++it) {
        QByteArray bytes;
        if (!seen.contains(it.key()) || !readPlain(folder, it.key(), budget, &bytes, &reason))
            return fail(seen.contains(it.key()) ? reason : tr("%1, which its manifest lists, is not there.").arg(it.key().left(200)));
        budget -= bytes.size();
        if (sha256(bytes) != it.value())
            return fail(tr("%1 is not what its manifest says.").arg(it.key().left(200)));
        const QString kind = AssetLibrary::kindOf(it.key());
        if (const QString problem = contentProblem(it.key(), kind, bytes); !problem.isEmpty())
            return fail(problem);
        if (kind == QStringLiteral("model")) {
            if (const QString problem = modelProblem(it.key(), bytes, held); !problem.isEmpty())
                return fail(problem);
        }
        files.append({it.key(), it.value(), bytes.size(), kind});
    }
    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) { return a.path < b.path; });

    pack->id = id;
    pack->name = manifest.value(QStringLiteral("name")).toString().trimmed();
    pack->version = version;
    pack->developer = developer;
    pack->licence = licence.value(QStringLiteral("spdx")).toString();
    pack->licenceFile = licenceFile;
    pack->folder = Confinement::realPath(folder);
    pack->manifestSha256 = sha256(manifestBytes);
    pack->signature = developerSignature;
    pack->files = files;
    for (qsizetype i = 0; i < files.size(); ++i)
        pack->index.insert(files.at(i).path, i);
    // Where the Store says it came from, if its record says: text, and nothing else.
    const QJsonObject provenance = entry.value(QStringLiteral("provenance")).toObject();
    for (const char *key : {"origin", "author", "source", "permission"}) {
        const QJsonValue value = provenance.value(QLatin1String(key));
        if (isPlainText(value, 1024) && !value.toString().isEmpty())
            pack->provenance.insert(QLatin1String(key), value.toString());
    }
    if (!signature.isEmpty()) {
        const QMutexLocker locker(&checkedMutex());
        if (checked().size() > AssetLibrary::mostPacks)
            checked().clear();
        checked().insert(folder, {signature, *pack});
    }
    return true;
}

/// The project-relative path of a pack's file there.
QString inProject(const QString &id, const QString &source)
{
    return QStringLiteral("assets/%1/%2").arg(id, source);
}

/// The licence's text and any notice beside it at the pack's top: they go with whatever is used of it.
QStringList whatAlwaysGoes(const AssetLibrary::Pack &pack)
{
    QStringList always{pack.licenceFile};
    static const QRegularExpression notice(QStringLiteral("^notice(\\.txt)?\\z"), QRegularExpression::CaseInsensitiveOption);
    for (const AssetLibrary::PackFile &file : pack.files) {
        if (notice.match(file.path).hasMatch())
            always.append(file.path);
    }
    return always;
}

QJsonObject provenanceOf(const AssetLibrary::Pack &pack)
{
    QJsonObject provenance = QJsonObject::fromVariantMap(pack.provenance);
    provenance.insert(QStringLiteral("manifest"), QString::fromLatin1(pack.manifestSha256.toHex()));
    provenance.insert(QStringLiteral("signature"), QString::fromLatin1(pack.signature.toBase64()));
    return provenance;
}

// ── Changing the project: one transaction ──

struct Change {
    QStringList put;                   ///< project paths written
    QHash<QString, QByteArray> bytes;  ///< what each holds
    QStringList remove;                ///< project paths removed
    /// What a file there now must still hold when it is set aside, raw SHA-256: what was planned to be replaced or removed. A
    /// target there that is not in this was not there when it was planned, and is not set aside either.
    QHash<QString, QByteArray> expect;
};

/// The journal, read back as a stranger's file would be.
struct Journal {
    QString tag;
    QString identity;  ///< the folder it was written for: ProjectWorkspace::folderIdentity
    QStringList put, remove;
    QHash<QString, QByteArray> placed;  ///< raw SHA-256 of what each put holds
    QByteArray lock;  ///< the digest the lock will have once it is written
    QString problem;
};

Journal readJournal(const QString &realProject)
{
    Journal journal;
    QByteArray bytes;
    const QString path = AssetLibrary::journalFor(realProject);
    if (!readPlain(QFileInfo(path).path(), QFileInfo(path).fileName(), AssetLibrary::largestLock, &bytes)) {
        journal.problem = tr("what it left cannot be read");
        return journal;
    }
    const QJsonObject object = QJsonDocument::fromJson(bytes).object();
    static const QRegularExpression tag(QStringLiteral("^[0-9a-f]{32}\\z"));
    journal.tag = object.value(QStringLiteral("tag")).toString();
    journal.identity = object.value(QStringLiteral("identity")).toString();
    journal.lock = fromHex(object.value(QStringLiteral("lock")));
    if (object.value(QStringLiteral("format")).toInt() != journalFormat || object.value(QStringLiteral("project")).toString() != realProject
        || !tag.match(journal.tag).hasMatch() || journal.lock.isEmpty()) {
        journal.problem = tr("what it left cannot be read");
        return journal;
    }
    for (const char *list : {"put", "remove"}) {
        for (const QJsonValue &value : object.value(QLatin1String(list)).toArray()) {
            const QString each = value.toString();
            if (!ProjectWorkspace::plainRelative(each) || !each.startsWith(QStringLiteral("assets/"))) {
                journal.problem = tr("what it left names a file it could not have");
                return journal;
            }
            (qstrcmp(list, "put") == 0 ? journal.put : journal.remove).append(each);
        }
    }
    const QJsonObject placed = object.value(QStringLiteral("placed")).toObject();
    for (const QString &each : std::as_const(journal.put)) {
        const QByteArray digest = fromHex(placed.value(each));
        if (digest.isEmpty()) {
            journal.problem = tr("what it left does not say what it placed");
            return journal;
        }
        journal.placed.insert(each, digest);
    }
    return journal;
}

/// What putting a cut-short change right came to.
struct Settled {
    bool finished = false;  ///< its lock was written, and it was finished
    QString stuck;          ///< it could not go on, and why: the journal stays, to be tried again
    QString refused;        ///< it moved nothing, and why: the journal goes
    QStringList kept;       ///< placed files that no longer held what was placed, left where they are
    QString leftAside;      ///< what was set aside and could not go back stays here
};

/// Put a cut-short change back, or finish it, by its journal and what is on disk. Every step looks before it moves, so this can
/// be cut short too and run again.
Settled settle(const QString &realProject, const Journal &journal)
{
    Settled settled;
    const QString stagingName = stagingPrefix + journal.tag;
    const QString staging = realProject + QLatin1Char('/') + stagingName;
    const auto drop = [&realProject] {
        QFile::remove(AssetLibrary::journalFor(realProject));
    };
    // Another folder where that one was, or that one with what the change wrote beside it gone: nothing here is the change's.
    if (!journal.identity.isEmpty() && ProjectWorkspace::folderIdentity(realProject) != journal.identity) {
        settled.refused = tr("this is not the folder it was made in, so nothing was moved.");
        drop();
        return settled;
    }
    const QFileInfo stagingInfo(staging);
    if (stagingInfo.isSymLink() || stagingInfo.isJunction() || !stagingInfo.isDir()) {
        settled.refused = stagingInfo.isSymLink() || stagingInfo.isJunction()
                              ? tr("%1 is a link, so nothing was moved.").arg(stagingName)
                              : tr("what it had written beside the project, %1, is gone, so nothing was moved.").arg(stagingName);
        drop();
        return settled;
    }
    QByteArray lock;
    settled.finished = readPlain(realProject, QLatin1String(AssetLibrary::lockName), AssetLibrary::largestLock, &lock)
                       && sha256(lock) == journal.lock;
    if (!settled.finished) {
        // What was put in place goes back where it waited, if it still holds what was placed; then what was set aside comes back,
        // each only where nothing now is.
        for (const QString &path : journal.put) {
            const QString target = realProject + QLatin1Char('/') + path;
            const QString waiting = staging + QStringLiteral("/new/") + path;
            const QFileInfo there(target);
            if (exists(waiting) || !there.isFile() || there.isSymLink() || !wayProblem(realProject, path).isEmpty())
                continue;
            if (digestOf(realProject, path) != journal.placed.value(path)) {
                settled.kept.append(path);
                continue;
            }
            if (!QDir().mkpath(QFileInfo(waiting).path()) || !QFile::rename(target, waiting)) {
                settled.stuck = tr("%1 could not be put back.").arg(path);
                return settled;
            }
        }
        for (const QString &path : journal.put + journal.remove) {
            const QString target = realProject + QLatin1Char('/') + path;
            const QString aside = staging + QStringLiteral("/old/") + path;
            if (exists(aside) && !exists(target) && wayProblem(realProject, path).isEmpty()) {
                if (!QDir().mkpath(QFileInfo(target).path()) || !QFile::rename(aside, target)) {
                    settled.stuck = tr("%1 could not be put back.").arg(path);
                    return settled;
                }
            }
        }
        makeDurable(realProject);
    }
    // What was written new goes. What was set aside goes once the change stands; if it does not, anything that could not go back
    // stays where it is, for the person.
    QDir(staging + QStringLiteral("/new")).removeRecursively();
    bool holdsFiles = false;
    if (!settled.finished) {
        QDirIterator left(staging + QStringLiteral("/old"), QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
        holdsFiles = left.hasNext();
    }
    if (holdsFiles)
        settled.leftAside = stagingName;
    else if (!QDir(staging).removeRecursively()) {
        settled.stuck = tr("%1 could not be cleared away.").arg(stagingName);
        return settled;
    }
    makeDurable(realProject);
    drop();
    return settled;
}

/// What settle() came to, for a person.
QString said(const Settled &settled)
{
    if (!settled.stuck.isEmpty())
        return tr("A change to this project's assets was cut short, and putting it right stopped: %1 It will be tried again.").arg(settled.stuck);
    if (!settled.refused.isEmpty())
        return tr("A change to this project's assets was cut short, but %1").arg(settled.refused);
    if (settled.finished)
        return tr("A change to this project's assets that was cut short after it was made has been finished.");
    QString text = tr("A change to this project's assets was cut short, and what it had moved has been put back.");
    if (!settled.kept.isEmpty())
        text += QLatin1Char(' ') + tr("%1 changed since, and was kept as it is.").arg(settled.kept.join(QStringLiteral(", ")).left(400));
    if (!settled.leftAside.isEmpty())
        text += QLatin1Char(' ') + tr("What could not go back is in %1.").arg(settled.leftAside);
    return text;
}

/// Make the change, all of it or none: see assetlibrary.h. "" when done, or why not, with nothing changed.
QString apply(const QString &realProject, const Change &change, const QList<AssetLibrary::LockedPack> &packs,
              const AssetLibrary::Steps &steps)
{
    const QString tag = QUuid::createUuid().toString(QUuid::Id128);
    const QByteArray lock = AssetLibrary::lockBytes(packs, tag);
    // The lock it would write is one this Studio could read back, or nothing is done.
    if (const AssetLibrary::Lock check = AssetLibrary::parseLock(lock, realProject); !check.problem.isEmpty())
        return tr("The project's new lock would not be one this Studio can read (%1), so nothing was changed.").arg(check.problem);

    const QString staging = realProject + QLatin1Char('/') + stagingPrefix + tag;
    QJsonObject placed;
    for (const QString &path : change.put) {
        const QByteArray bytes = change.bytes.value(path);
        if (!writeNew(staging + QStringLiteral("/new/") + path, bytes)) {
            QDir(staging).removeRecursively();
            return tr("%1 could not be written into the project.").arg(path);
        }
        placed.insert(path, QString::fromLatin1(sha256(bytes).toHex()));
    }
    QDir().mkpath(staging + QStringLiteral("/old"));
    makeDurable(staging);

    const QString journalPath = AssetLibrary::journalFor(realProject);
    QDir().mkpath(QFileInfo(journalPath).path());
    const QJsonObject journal{{QStringLiteral("format"), journalFormat},
                              {QStringLiteral("project"), realProject},
                              {QStringLiteral("identity"), ProjectWorkspace::folderIdentity(realProject)},
                              {QStringLiteral("tag"), tag},
                              {QStringLiteral("put"), QJsonArray::fromStringList(change.put)},
                              {QStringLiteral("placed"), placed},
                              {QStringLiteral("remove"), QJsonArray::fromStringList(change.remove)},
                              {QStringLiteral("lock"), QString::fromLatin1(sha256(lock).toHex())}};
    QSaveFile written(journalPath);
    if (!written.open(QIODevice::WriteOnly) || written.write(QJsonDocument(journal).toJson()) < 0 || !written.commit()) {
        QDir(staging).removeRecursively();
        return tr("This person's store could not be written to, so nothing was changed.");
    }
    syncFolder(QFileInfo(journalPath).path());
    if (steps)
        steps("staged");

    const auto putBack = [&](const QString &why) {
        const Journal left = readJournal(realProject);
        if (!left.problem.isEmpty())
            return why + QLatin1Char(' ') + tr("Putting it back stopped: %1 It will be tried again.").arg(left.problem);
        const Settled settled = settle(realProject, left);
        if (!settled.stuck.isEmpty())
            return why + QLatin1Char(' ') + tr("Putting it back stopped: %1 It will be tried again.").arg(settled.stuck);
        QString text = why + QLatin1Char(' ') + tr("Nothing was changed.");
        if (!settled.kept.isEmpty() || !settled.leftAside.isEmpty())
            text += QLatin1Char(' ') + said(settled);
        return text;
    };
    // Each is looked at again just before it is set aside: one that is no longer what was planned is someone's, now.
    for (const QString &path : change.put + change.remove) {
        const QString target = realProject + QLatin1Char('/') + path;
        if (const QString way = wayProblem(realProject, path); !way.isEmpty())
            return putBack(way);
        if (!exists(target))
            continue;
        if (!change.expect.contains(path) || digestOf(realProject, path) != change.expect.value(path))
            return putBack(tr("%1 changed while this was on its way, so it is kept as it is.").arg(path));
        const QString aside = staging + QStringLiteral("/old/") + path;
        if (!QDir().mkpath(QFileInfo(aside).path()) || !QFile::rename(target, aside))
            return putBack(tr("%1 could not be set aside.").arg(path));
    }
    makeDurable(realProject);
    if (steps)
        steps("aside");

    for (const QString &path : change.put) {
        const QString target = realProject + QLatin1Char('/') + path;
        if (const QString way = wayProblem(realProject, path); !way.isEmpty())
            return putBack(way);
        if (exists(target))
            return putBack(tr("%1 appeared while this was on its way, so it is kept as it is.").arg(path));
        if (!QDir().mkpath(QFileInfo(target).path()) || !QFile::rename(staging + QStringLiteral("/new/") + path, target))
            return putBack(tr("%1 could not be put in place.").arg(path));
    }
    makeDurable(realProject);
    if (steps)
        steps("placed");

    // The moment it happens. Its tag makes it like no lock before it, so whether it was written is never in doubt.
    const QString lockPath = realProject + QLatin1Char('/') + QLatin1String(AssetLibrary::lockName);
    const QFileInfo lockInfo(lockPath);
    if (lockInfo.isSymLink() || lockInfo.isJunction() || (lockInfo.exists() && !lockInfo.isFile()))
        return putBack(tr("%1 is not a plain file.").arg(QLatin1String(AssetLibrary::lockName)));
    QSaveFile lockFile(lockPath);
    if (!lockFile.open(QIODevice::WriteOnly) || lockFile.write(lock) != lock.size() || !lockFile.commit())
        return putBack(tr("%1 could not be written.").arg(QLatin1String(AssetLibrary::lockName)));
    syncFolder(realProject);
    if (steps)
        steps("locked");

    // Done either way: what is left goes the next time the project is looked at.
    settle(realProject, readJournal(realProject));
    return {};
}

/// What every operation starts with: the project's real folder, the one change at a time, anything cut short put right, and a
/// lock that can be used.
struct Opened {
    QString real;
    std::unique_ptr<AssetLibrary::Hold> hold;
    AssetLibrary::Lock lock;
    QString problem;
    QString recovered;
};

Opened open(const QString &project)
{
    Opened opened;
    opened.real = project.isEmpty() ? QString() : Confinement::realPath(project);
    if (opened.real.isEmpty() || !QFileInfo(opened.real).isDir()) {
        opened.problem = tr("There is no project folder at %1.").arg(project);
        return opened;
    }
    opened.hold = std::make_unique<AssetLibrary::Hold>(opened.real);
    if (!opened.hold->held()) {
        opened.problem = opened.hold->problem();
        return opened;
    }
    opened.recovered = AssetLibrary::recover(opened.real);
    opened.lock = AssetLibrary::readLock(opened.real);
    if (!opened.lock.problem.isEmpty())
        opened.problem = tr("%1, so nothing it names was changed.").arg(opened.lock.problem);
    return opened;
}

/// The bytes of a pack's file, checked again as they are read: what is copied is what was checked.
bool bytesOf(const AssetLibrary::Pack &pack, const AssetLibrary::PackFile &file, QByteArray *bytes, QString *why)
{
    if (!readPlain(pack.folder, file.path, AssetLibrary::largestPack, bytes, why))
        return false;
    if (sha256(*bytes) != file.sha256) {
        *why = tr("%1 changed in the Store's folder after it was checked.").arg(file.path);
        return false;
    }
    return true;
}

const AssetLibrary::Pack *findPack(const QList<AssetLibrary::Pack> &packs, const QString &id)
{
    const auto it = std::find_if(packs.cbegin(), packs.cend(), [&id](const AssetLibrary::Pack &pack) { return pack.id == id; });
    return it == packs.cend() ? nullptr : &*it;
}

QString refusalOf(const QStringList &refusedPacks, const QString &id)
{
    for (const QString &each : refusedPacks) {
        if (each.startsWith(id + QStringLiteral(": ")))
            return tr("%1 is installed but cannot be used: %2").arg(id, each.mid(id.size() + 2));
    }
    return tr("%1 is not installed.").arg(id);
}

/// A name for the new version of `path` beside it that nothing has or will have: hero.png → hero-1.1.0.png.
QString besideName(const QString &realProject, const QString &path, const QString &version, const QSet<QString> &taken)
{
    const QFileInfo info(path);
    const QString folder = info.path() == QStringLiteral(".") ? QString() : info.path() + QLatin1Char('/');
    const QString suffix = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    for (int n = 1; n < 1000; ++n) {
        const QString name = folder + info.completeBaseName() + QLatin1Char('-') + version
                             + (n == 1 ? QString() : QStringLiteral("-%1").arg(n)) + suffix;
        if (!taken.contains(name) && !exists(realProject + QLatin1Char('/') + name))
            return name;
    }
    return {};
}

struct Plan {
    QString problem;
    AssetLibrary::Pack pack;
    AssetLibrary::LockedPack locked;
    QStringList replaced, added, removed, same, gone;
    QList<std::pair<QString, QString>> changed;  ///< a changed copy, and where the new version of it would go, or ""
    QHash<QString, QString> sources;             ///< project path → path in the new pack, for what it writes or keeps as new
    QHash<QString, QByteArray> expect;           ///< project path → what it holds now, as planned
};

Plan planFor(const Opened &opened, const QString &packs, const QString &id)
{
    Plan plan;
    if (!opened.problem.isEmpty()) {
        plan.problem = opened.problem;
        return plan;
    }
    const AssetLibrary::LockedPack *locked = opened.lock.pack(id);
    if (!locked) {
        plan.problem = tr("This project does not use %1.").arg(id);
        return plan;
    }
    QStringList refusedPacks;
    const QList<AssetLibrary::Pack> installed = AssetLibrary::readInstalled(packs, &refusedPacks);
    const AssetLibrary::Pack *pack = findPack(installed, id);
    if (!pack) {
        plan.problem = refusalOf(refusedPacks, id) + QLatin1Char(' ') + tr("Its copies in the project stay as they are.");
        return plan;
    }
    if (compareVersions(pack->version, locked->version) <= 0) {
        plan.problem = tr("%1 %2 is the newest installed: there is nothing to update.").arg(locked->name, locked->version);
        return plan;
    }
    plan.pack = *pack;
    plan.locked = *locked;
    // Every name the new version has, and every name the project's lock has, is taken for a beside copy.
    QSet<QString> taken;
    for (const AssetLibrary::PackFile &file : pack->files)
        taken.insert(inProject(id, file.path));
    QSet<QString> sources;
    for (const AssetLibrary::LockedFile &copy : locked->files) {
        taken.insert(copy.path);
        if (!copy.beside)
            sources.insert(copy.source);
    }
    for (const AssetLibrary::LockedFile &copy : locked->files) {
        // A new version put beside a changed copy is the person's: no update touches it.
        if (copy.beside)
            continue;
        if (const QString way = wayProblem(opened.real, copy.path); !way.isEmpty()) {
            plan.problem = way + QLatin1Char(' ') + tr("Nothing was changed.");
            return plan;
        }
        const AssetLibrary::PackFile *next = pack->file(copy.source);
        const QByteArray now = digestOf(opened.real, copy.path);
        if (!exists(opened.real + QLatin1Char('/') + copy.path)) {
            plan.gone.append(copy.path);
        } else if (now == copy.sha256) {
            plan.expect.insert(copy.path, now);
            if (!next)
                plan.removed.append(copy.path);
            else {
                (next->sha256 == copy.sha256 ? plan.same : plan.replaced).append(copy.path);
                plan.sources.insert(copy.path, copy.source);
            }
        } else {
            const QString beside = next ? besideName(opened.real, copy.path, pack->version, taken) : QString();
            taken.insert(beside);
            plan.changed.append({copy.path, beside});
            if (!beside.isEmpty())
                plan.sources.insert(beside, copy.source);
        }
    }
    // What is new in this version: all of it if the whole pack was used, and its licence and notice whatever was.
    const QStringList always = whatAlwaysGoes(*pack);
    for (const AssetLibrary::PackFile &file : pack->files) {
        if (sources.contains(file.path) || (!locked->whole && !always.contains(file.path)))
            continue;
        const QString path = inProject(id, file.path);
        if (const QString way = wayProblem(opened.real, path); !way.isEmpty()) {
            plan.problem = way + QLatin1Char(' ') + tr("Nothing was changed.");
            return plan;
        }
        if (!exists(opened.real + QLatin1Char('/') + path)) {
            plan.added.append(path);
            plan.sources.insert(path, file.path);
        } else if (digestOf(opened.real, path) == file.sha256) {
            plan.same.append(path);
            plan.sources.insert(path, file.path);
        } else {
            // A file of the person's own, of that name: kept, and the pack's put beside it if they choose.
            const QString beside = besideName(opened.real, path, pack->version, taken);
            taken.insert(beside);
            plan.changed.append({path, beside});
            if (!beside.isEmpty())
                plan.sources.insert(beside, file.path);
        }
    }
    return plan;
}

QVariantMap planAnswer(const Plan &plan)
{
    QVariantList changed;
    for (const auto &[path, beside] : plan.changed)
        changed.append(QVariantMap{{QStringLiteral("path"), path}, {QStringLiteral("beside"), beside}});
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("problem"), QString()},
            {QStringLiteral("id"), plan.pack.id},
            {QStringLiteral("name"), plan.pack.name},
            {QStringLiteral("from"), plan.locked.version},
            {QStringLiteral("to"), plan.pack.version},
            {QStringLiteral("replaced"), plan.replaced},
            {QStringLiteral("added"), plan.added},
            {QStringLiteral("removed"), plan.removed},
            {QStringLiteral("same"), plan.same},
            {QStringLiteral("gone"), plan.gone},
            {QStringLiteral("changed"), changed}};
}

/// The folder the thumbnails read, as the library was last told.
QMutex &thumbnailMutex()
{
    static QMutex mutex;
    return mutex;
}

QString &thumbnailFolder()
{
    static QString folder;
    return folder;
}

}  // namespace

// ── The class ──

const AssetLibrary::PackFile *AssetLibrary::Pack::file(const QString &path) const
{
    const auto it = index.constFind(path);
    return it == index.cend() ? nullptr : &files.at(*it);
}

const AssetLibrary::LockedPack *AssetLibrary::Lock::pack(const QString &id) const
{
    const auto it = std::find_if(packs.cbegin(), packs.cend(), [&id](const LockedPack &each) { return each.id == id; });
    return it == packs.cend() ? nullptr : &*it;
}

AssetLibrary::Hold::Hold(const QString &realProject)
{
    const QString path = AssetLibrary::journalFor(realProject) + QStringLiteral(".lock");
    if (!QDir().mkpath(QFileInfo(path).path())) {
        m_problem = tr("This person's own store, %1, cannot be written, so nothing was changed.").arg(QFileInfo(path).path());
        return;
    }
    auto lock = std::make_unique<QLockFile>(path);
    lock->setStaleLockTime(0);
    if (lock->tryLock(0)) {
        m_lock = std::move(lock);
        return;
    }
    m_problem = lock->error() == QLockFile::LockFailedError
                    ? tr("Another QQ Studio is changing or building this project now. When it has finished, try again.")
                    : tr("This person's own store, %1, cannot be written, so nothing was changed.").arg(QFileInfo(path).path());
}

AssetLibrary::Hold::~Hold() = default;

AssetLibrary::AssetLibrary(QObject *parent) : QObject(parent) {}

void AssetLibrary::setFolder(const QString &folder)
{
    AssetThumbnails::setFolder(folder);
    if (folder == m_folder)
        return;
    m_folder = folder;
    emit folderChanged();
}

QString AssetLibrary::kindOf(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    static const QHash<QString, QString> kinds{
        {QStringLiteral("png"), QStringLiteral("picture")}, {QStringLiteral("jpg"), QStringLiteral("picture")},
        {QStringLiteral("jpeg"), QStringLiteral("picture")}, {QStringLiteral("webp"), QStringLiteral("picture")},
        {QStringLiteral("ktx"), QStringLiteral("picture")}, {QStringLiteral("ktx2"), QStringLiteral("picture")},
        {QStringLiteral("hdr"), QStringLiteral("sky")}, {QStringLiteral("gltf"), QStringLiteral("model")},
        {QStringLiteral("glb"), QStringLiteral("model")}, {QStringLiteral("wav"), QStringLiteral("sound")},
        {QStringLiteral("ogg"), QStringLiteral("sound")}, {QStringLiteral("flac"), QStringLiteral("sound")},
        {QStringLiteral("mp3"), QStringLiteral("sound")}, {QStringLiteral("bin"), QStringLiteral("buffer")},
        {QStringLiteral("txt"), QStringLiteral("text")}};
    return kinds.value(suffix);
}

QString AssetLibrary::journalFor(const QString &realProject)
{
    const QByteArray hash = QCryptographicHash::hash(realProject.toUtf8(), QCryptographicHash::Sha256).toHex();
    return ProjectWorkspace::storeFolder() + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".assets.json");
}

QList<AssetLibrary::Pack> AssetLibrary::readInstalled(const QString &packs, QStringList *refusedPacks)
{
    QList<Pack> found;
    const auto refuse = [refusedPacks](const QString &id, const QString &why) {
        if (refusedPacks)
            refusedPacks->append(packProblem(id, why));
    };
    const QString real = packs.isEmpty() ? QString() : Confinement::realPath(packs);
    if (real.isEmpty() || !QFileInfo(real).isDir())
        return found;
    QByteArray bytes;
    QString why;
    if (!readPlain(real, QStringLiteral("installed.json"), largestRecord, &bytes, &why)) {
        if (exists(real + QStringLiteral("/installed.json")))
            refuse(QStringLiteral("installed.json"), why);
        return found;
    }
    // The folder of every item is the Store's own, never a link to somewhere else.
    const QFileInfo items(real + QStringLiteral("/items"));
    if (items.isSymLink() || items.isJunction() || items.isShortcut()) {
        refuse(QStringLiteral("items"), tr("the Store's folder of items is a link."));
        return found;
    }
    const QJsonObject records = QJsonDocument::fromJson(bytes).object().value(QStringLiteral("items")).toObject();
    int looked = 0;
    for (auto it = records.begin(); it != records.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        // A game, a theme or anything else the Store installs is not a pack.
        if (entry.value(QStringLiteral("kind")).toString() != QStringLiteral("asset-pack"))
            continue;
        if (!isValidId(it.key())) {
            refuse(it.key().left(128), tr("it is not named as the Store names a pack."));
            continue;
        }
        if (++looked > mostPacks) {
            refuse(it.key(), tr("more packs are installed than the Studio looks at."));
            continue;
        }
        Pack pack;
        if (readPack(real + QStringLiteral("/items"), it.key(), entry, &pack, &why))
            found.append(pack);
        else
            refuse(it.key(), why);
    }
    std::sort(found.begin(), found.end(), [](const Pack &a, const Pack &b) { return a.name == b.name ? a.id < b.id : a.name < b.name; });
    return found;
}

AssetLibrary::Lock AssetLibrary::readLock(const QString &realProject)
{
    Lock lock;
    const QString path = realProject + QLatin1Char('/') + QLatin1String(lockName);
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return lock;
    lock.found = true;
    const auto broken = [&lock](const QString &why) {
        lock.problem = tr("%1 cannot be used: %2").arg(QLatin1String(lockName), why);
        return lock;
    };
    if (info.isSymLink() || info.isJunction() || info.isShortcut())
        return broken(tr("it is a link"));
    if (info.size() > largestLock)
        return broken(tr("it is larger than a lock may be"));
    QByteArray bytes;
    QString why;
    if (!readPlain(realProject, QLatin1String(lockName), largestLock, &bytes, &why))
        return broken(tr("it cannot be read"));
    return parseLock(bytes, realProject);
}

AssetLibrary::Lock AssetLibrary::parseLock(const QByteArray &bytes, const QString &realProject)
{
    Lock lock;
    lock.found = true;
    const auto broken = [&lock](const QString &why) {
        lock.problem = tr("%1 cannot be used: %2").arg(QLatin1String(lockName), why);
        lock.packs.clear();
        return lock;
    };
    if (bytes.size() > largestLock)
        return broken(tr("it is larger than a lock may be"));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return broken(tr("it is not JSON"));
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toInt(-1) != lockFormat || !root.value(QStringLiteral("format")).isDouble())
        return broken(tr("it is of a format this Studio does not know"));
    static const QRegularExpression tag(QStringLiteral("^[0-9a-f]{32}\\z"));
    if (root.contains(QStringLiteral("change")) && !tag.match(root.value(QStringLiteral("change")).toString()).hasMatch())
        return broken(tr("its change is not one"));
    lock.change = root.value(QStringLiteral("change")).toString();
    const QJsonValue packsValue = root.value(QStringLiteral("packs"));
    if (!packsValue.isArray() || packsValue.toArray().size() > mostPacks)
        return broken(tr("its list of packs is not one"));
    const QFileInfo assets(realProject + QStringLiteral("/assets"));
    if (assets.isSymLink() || assets.isJunction() || assets.isShortcut() || (assets.exists() && !assets.isDir()))
        return broken(tr("assets is a link, or not a folder"));
    QSet<QString> ids;
    QSet<QString> paths;
    int files = 0;
    for (const QJsonValue &value : packsValue.toArray()) {
        const QJsonObject entry = value.toObject();
        LockedPack pack;
        pack.id = entry.value(QStringLiteral("id")).toString();
        pack.name = entry.value(QStringLiteral("name")).toString();
        pack.version = entry.value(QStringLiteral("version")).toString();
        pack.developer = entry.value(QStringLiteral("developer")).toString();
        pack.licence = entry.value(QStringLiteral("licence")).toString();
        pack.whole = entry.value(QStringLiteral("whole")).toBool();
        if (!value.isObject() || !isValidId(pack.id) || ids.contains(pack.id))
            return broken(tr("it names a pack that is not valid, or one twice"));
        ids.insert(pack.id);
        if (!isPlainText(entry.value(QStringLiteral("name")), 256) || !isValidVersion(pack.version)
            || !isPlainText(entry.value(QStringLiteral("developer")), 128) || pack.developer.isEmpty() || !isValidSpdx(pack.licence)
            || !entry.value(QStringLiteral("whole")).isBool())
            return broken(tr("what it says of %1 is not valid").arg(pack.id));
        const QJsonValue provenanceValue = entry.value(QStringLiteral("provenance"));
        const QJsonObject provenance = provenanceValue.toObject();
        if (!provenanceValue.isObject() || provenance.size() > 8)
            return broken(tr("what it says of %1 is not valid").arg(pack.id));
        for (auto it = provenance.begin(); it != provenance.end(); ++it) {
            if (!isPlainText(it.value(), 1024))
                return broken(tr("what it says of %1 is not valid").arg(pack.id));
            pack.provenance.insert(it.key(), it.value().toString());
        }
        const QJsonValue list = entry.value(QStringLiteral("files"));
        if (!list.isArray())
            return broken(tr("what it says of %1 is not valid").arg(pack.id));
        for (const QJsonValue &each : list.toArray()) {
            const QJsonObject file = each.toObject();
            LockedFile copy;
            copy.path = file.value(QStringLiteral("path")).toString();
            copy.source = file.value(QStringLiteral("source")).toString();
            copy.version = file.value(QStringLiteral("version")).toString();
            copy.sha256 = fromHex(file.value(QStringLiteral("sha256")));
            copy.beside = file.value(QStringLiteral("beside")).toBool();
            if (++files > mostLockedFiles)
                return broken(tr("it names more files than a lock may"));
            const QString folder = QStringLiteral("assets/%1/").arg(pack.id);
            if (!each.isObject() || !ProjectWorkspace::plainRelative(copy.path) || !copy.path.startsWith(folder)
                || !safePackPath(copy.source) || !safePackPath(copy.path.mid(folder.size())) || copy.sha256.isEmpty()
                || !isValidVersion(copy.version) || (file.contains(QStringLiteral("beside")) && !file.value(QStringLiteral("beside")).isBool()))
                return broken(tr("it names %1, which is not plainly a file of %2 in assets/%2/").arg(copy.path.left(200), pack.id));
            if (paths.contains(copy.path))
                return broken(tr("it names %1 twice").arg(copy.path.left(200)));
            paths.insert(copy.path);
            if (const QString way = wayProblem(realProject, copy.path); !way.isEmpty())
                return broken(way);
            const QString real = Confinement::realPath(realProject + QLatin1Char('/') + copy.path);
            if (!Confinement::within(real, realProject))
                return broken(tr("%1 leads out of the project").arg(copy.path.left(200)));
            pack.files.append(copy);
        }
        std::sort(pack.files.begin(), pack.files.end(), [](const LockedFile &a, const LockedFile &b) { return a.path < b.path; });
        lock.packs.append(pack);
    }
    std::sort(lock.packs.begin(), lock.packs.end(), [](const LockedPack &a, const LockedPack &b) { return a.id < b.id; });
    return lock;
}

QByteArray AssetLibrary::lockBytes(const QList<LockedPack> &packs, const QString &change)
{
    QList<LockedPack> sorted = packs;
    std::sort(sorted.begin(), sorted.end(), [](const LockedPack &a, const LockedPack &b) { return a.id < b.id; });
    QJsonArray list;
    for (LockedPack &pack : sorted) {
        std::sort(pack.files.begin(), pack.files.end(), [](const LockedFile &a, const LockedFile &b) { return a.path < b.path; });
        QJsonArray files;
        for (const LockedFile &file : std::as_const(pack.files)) {
            QJsonObject entry{{QStringLiteral("path"), file.path},
                              {QStringLiteral("source"), file.source},
                              {QStringLiteral("sha256"), QString::fromLatin1(file.sha256.toHex())},
                              {QStringLiteral("version"), file.version}};
            if (file.beside)
                entry.insert(QStringLiteral("beside"), true);
            files.append(entry);
        }
        list.append(QJsonObject{{QStringLiteral("id"), pack.id},
                                {QStringLiteral("name"), pack.name},
                                {QStringLiteral("version"), pack.version},
                                {QStringLiteral("developer"), pack.developer},
                                {QStringLiteral("licence"), pack.licence},
                                {QStringLiteral("provenance"), QJsonObject::fromVariantMap(pack.provenance)},
                                {QStringLiteral("whole"), pack.whole},
                                {QStringLiteral("files"), files}});
    }
    QJsonObject root{{QStringLiteral("format"), lockFormat}, {QStringLiteral("packs"), list}};
    if (!change.isEmpty())
        root.insert(QStringLiteral("change"), change);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString AssetLibrary::recover(const QString &realProject)
{
    if (!exists(journalFor(realProject)))
        return {};
    const Journal journal = readJournal(realProject);
    if (!journal.problem.isEmpty()) {
        QFile::remove(journalFor(realProject));
        return tr("A change to this project's assets was cut short, and %1, so it was left as it is.").arg(journal.problem);
    }
    return said(settle(realProject, journal));
}

QVariantMap AssetLibrary::usePack(const QString &project, const QString &packs, const QString &id, const QStringList &files,
                                  bool replace, const Steps &steps)
{
    const Opened opened = open(project);
    if (!opened.problem.isEmpty())
        return refused(opened.problem);
    QStringList refusedPacks;
    const QList<Pack> installed = readInstalled(packs, &refusedPacks);
    const Pack *pack = findPack(installed, id);
    if (!pack)
        return refused(refusalOf(refusedPacks, id));
    const LockedPack *locked = opened.lock.pack(id);
    if (locked && locked->version != pack->version)
        return refused(tr("This project has %1 %2, and %3 is installed: update it first.").arg(locked->name, locked->version, pack->version));

    QStringList chosen = files;
    if (files.isEmpty()) {
        for (const PackFile &file : pack->files)
            chosen.append(file.path);
    }
    for (const QString &path : std::as_const(chosen)) {
        if (!pack->file(path))
            return refused(tr("%1 is not in %2.").arg(path.left(200), pack->name));
    }
    chosen += whatAlwaysGoes(*pack);
    chosen.removeDuplicates();
    chosen.sort();

    Change change;
    QStringList conflicts;
    QStringList copied;
    LockedPack entry = locked ? *locked : LockedPack{};
    entry.id = pack->id;
    entry.name = pack->name;
    entry.version = pack->version;
    entry.developer = pack->developer;
    entry.licence = pack->licence;
    entry.provenance = provenanceOf(*pack).toVariantMap();
    entry.whole = entry.whole || files.isEmpty();
    QHash<QString, LockedFile> lockedFiles;
    for (const LockedFile &file : std::as_const(entry.files))
        lockedFiles.insert(file.path, file);
    for (const QString &source : std::as_const(chosen)) {
        const PackFile *file = pack->file(source);
        const QString path = inProject(id, source);
        if (const QString way = wayProblem(opened.real, path); !way.isEmpty())
            return refused(way + QLatin1Char(' ') + tr("Nothing was changed."));
        copied.append(path);
        const bool there = exists(opened.real + QLatin1Char('/') + path);
        const QByteArray now = there ? digestOf(opened.real, path) : QByteArray();
        if (!there || now != file->sha256) {
            if (there && !replace) {
                conflicts.append(path);
                continue;
            }
            QByteArray bytes;
            QString why;
            if (!bytesOf(*pack, *file, &bytes, &why))
                return refused(why + QLatin1Char(' ') + tr("Nothing was changed."));
            change.put.append(path);
            change.bytes.insert(path, bytes);
            if (there)
                change.expect.insert(path, now);
        }
        lockedFiles.insert(path, {path, source, pack->version, file->sha256, false});
    }
    if (!conflicts.isEmpty()) {
        QVariantMap answer = refused(tr("%n file(s) of %1 are already in the project and differ from the pack's. Nothing was changed: "
                                        "replace them, or leave them.", nullptr, int(conflicts.size())).arg(pack->name));
        answer.insert(QStringLiteral("conflicts"), conflicts);
        return answer;
    }
    entry.files = lockedFiles.values();
    QList<LockedPack> next = opened.lock.packs;
    next.removeIf([&id](const LockedPack &each) { return each.id == id; });
    next.append(entry);
    if (const QString problem = apply(opened.real, change, next, steps); !problem.isEmpty())
        return refused(problem);
    return {{QStringLiteral("ok"), true}, {QStringLiteral("problem"), QString()}, {QStringLiteral("copied"), copied},
            {QStringLiteral("conflicts"), QStringList()}, {QStringLiteral("recovered"), opened.recovered}};
}

QVariantMap AssetLibrary::planUpdate(const QString &project, const QString &packs, const QString &id)
{
    const Opened opened = open(project);
    const Plan plan = planFor(opened, packs, id);
    return plan.problem.isEmpty() ? planAnswer(plan) : refused(plan.problem);
}

QVariantMap AssetLibrary::updatePack(const QString &project, const QString &packs, const QString &id, const QStringList &beside,
                                     const Steps &steps)
{
    const Opened opened = open(project);
    const Plan plan = planFor(opened, packs, id);
    if (!plan.problem.isEmpty())
        return refused(plan.problem);

    LockedPack entry = plan.locked;
    entry.name = plan.pack.name;
    entry.version = plan.pack.version;
    entry.developer = plan.pack.developer;
    entry.licence = plan.pack.licence;
    entry.provenance = provenanceOf(plan.pack).toVariantMap();
    QHash<QString, LockedFile> kept;
    for (const LockedFile &copy : std::as_const(plan.locked.files))
        kept.insert(copy.path, copy);

    Change change;
    change.expect = plan.expect;
    QStringList besideDone;
    const auto write = [&](const QString &path) -> QString {
        const PackFile *file = plan.pack.file(plan.sources.value(path));
        QByteArray bytes;
        QString why;
        if (!file || !bytesOf(plan.pack, *file, &bytes, &why))
            return file ? why : tr("%1 is not in the new version.").arg(path);
        change.put.append(path);
        change.bytes.insert(path, bytes);
        return {};
    };
    const auto record = [&](const QString &path, bool besideCopy) {
        const PackFile *file = plan.pack.file(plan.sources.value(path));
        kept.insert(path, {path, file->path, plan.pack.version, file->sha256, besideCopy});
    };
    for (const QString &path : plan.replaced + plan.added) {
        if (const QString why = write(path); !why.isEmpty())
            return refused(why + QLatin1Char(' ') + tr("Nothing was changed."));
        record(path, false);
    }
    for (const QString &path : plan.same)
        record(path, false);
    for (const auto &[path, besidePath] : plan.changed) {
        if (besidePath.isEmpty() || !beside.contains(path))
            continue;
        if (const QString why = write(besidePath); !why.isEmpty())
            return refused(why + QLatin1Char(' ') + tr("Nothing was changed."));
        record(besidePath, true);
        besideDone.append(besidePath);
    }
    for (const QString &path : plan.removed + plan.gone)
        kept.remove(path);
    change.remove = plan.removed;
    entry.files = kept.values();

    QList<LockedPack> next = opened.lock.packs;
    next.removeIf([&id](const LockedPack &each) { return each.id == id; });
    next.append(entry);
    if (const QString problem = apply(opened.real, change, next, steps); !problem.isEmpty())
        return refused(problem);
    QVariantMap answer = planAnswer(plan);
    answer.insert(QStringLiteral("beside"), besideDone);
    return answer;
}

// ── For the window ──

QVariantMap AssetLibrary::state(const QUrl &project)
{
    if (!available())
        return {{QStringLiteral("available"), false}};
    if (QQmlEngine *engine = qmlEngine(this); engine && !engine->imageProvider(QStringLiteral("qqassets")))
        engine->addImageProvider(QStringLiteral("qqassets"), new AssetThumbnails);
    const QString real = project.isLocalFile() ? Confinement::realPath(project.toLocalFile()) : QString();
    QVariantMap answer{{QStringLiteral("available"), true}, {QStringLiteral("lockProblem"), QString()},
                       {QStringLiteral("recovered"), QString()}};
    Lock lock;
    if (!real.isEmpty() && QFileInfo(real).isDir()) {
        // A change cut short is put right first, unless another Studio is changing or building it now.
        const Hold hold(real);
        if (hold.held())
            answer.insert(QStringLiteral("recovered"), recover(real));
        lock = readLock(real);
        answer.insert(QStringLiteral("lockProblem"), lock.problem.isEmpty() ? QString() : tr("%1. The Studio changes nothing it names until it is "
                                                                                         "put right or removed.").arg(lock.problem));
    }
    QStringList refusedPacks;
    const QList<Pack> installed = readInstalled(m_folder, &refusedPacks);
    QVariantList packs;
    for (const Pack &pack : installed) {
        const LockedPack *locked = lock.pack(pack.id);
        QSet<QString> lockedPaths;
        if (locked) {
            for (const LockedFile &each : locked->files)
                lockedPaths.insert(each.path);
        }
        QVariantList files;
        for (const PackFile &file : pack.files) {
            const QString there = inProject(pack.id, file.path);
            // A thumbnail from the checked bytes, never the file as it is now; named by the version, so an update is drawn anew.
            const QString thumbnail = QStringLiteral("image://qqassets/")
                                      + QString::fromLatin1(QUrl::toPercentEncoding(pack.id + QLatin1Char('/') + pack.version + QLatin1Char('/') + file.path, "/"));
            files.append(QVariantMap{{QStringLiteral("path"), file.path},
                                     {QStringLiteral("kind"), file.kind},
                                     {QStringLiteral("size"), file.size},
                                     {QStringLiteral("url"), file.kind == QStringLiteral("picture") ? thumbnail : QString()},
                                     {QStringLiteral("inProject"), lockedPaths.contains(there) ? there : QString()}});
        }
        QStringList from;
        for (const char *key : {"author", "origin"}) {
            if (pack.provenance.contains(QLatin1String(key)))
                from.append(pack.provenance.value(QLatin1String(key)).toString());
        }
        const QString provenance = from.isEmpty()
                                       ? tr("Developer (not checked here): %1").arg(pack.developer)
                                       : tr("From %1, through the Store. Developer (not checked here): %2").arg(from.join(QStringLiteral(", ")), pack.developer);
        packs.append(QVariantMap{{QStringLiteral("id"), pack.id},
                                 {QStringLiteral("name"), pack.name},
                                 {QStringLiteral("version"), pack.version},
                                 {QStringLiteral("developer"), pack.developer},
                                 {QStringLiteral("licence"), pack.licence},
                                 {QStringLiteral("provenance"), provenance},
                                 {QStringLiteral("files"), files},
                                 {QStringLiteral("used"), locked ? locked->version : QString()},
                                 {QStringLiteral("update"), locked && compareVersions(pack.version, locked->version) > 0}});
    }
    QVariantList missing;
    for (const LockedPack &locked : std::as_const(lock.packs)) {
        if (findPack(installed, locked.id))
            continue;
        missing.append(QVariantMap{{QStringLiteral("id"), locked.id},
                                   {QStringLiteral("name"), locked.name},
                                   {QStringLiteral("version"), locked.version},
                                   {QStringLiteral("licence"), locked.licence},
                                   {QStringLiteral("files"), int(locked.files.size())}});
    }
    QVariantList refusals;
    for (const QString &each : std::as_const(refusedPacks)) {
        const qsizetype colon = each.indexOf(QStringLiteral(": "));
        refusals.append(QVariantMap{{QStringLiteral("id"), each.left(colon)}, {QStringLiteral("problem"), each.mid(colon + 2)}});
    }
    answer.insert(QStringLiteral("packs"), packs);
    answer.insert(QStringLiteral("missing"), missing);
    answer.insert(QStringLiteral("refused"), refusals);
    return answer;
}

QVariantMap AssetLibrary::use(const QUrl &project, const QString &id, const QStringList &files, bool replace)
{
    if (!available() || !project.isLocalFile())
        return refused(tr("There is no asset library here."));
    return usePack(project.toLocalFile(), m_folder, id, files, replace);
}

QVariantMap AssetLibrary::plan(const QUrl &project, const QString &id)
{
    if (!available() || !project.isLocalFile())
        return refused(tr("There is no asset library here."));
    return planUpdate(project.toLocalFile(), m_folder, id);
}

QVariantMap AssetLibrary::update(const QUrl &project, const QString &id, const QStringList &beside)
{
    if (!available() || !project.isLocalFile())
        return refused(tr("There is no asset library here."));
    return updatePack(project.toLocalFile(), m_folder, id, beside);
}

// ── Thumbnails ──

AssetThumbnails::AssetThumbnails() : QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading) {}

void AssetThumbnails::setFolder(const QString &folder)
{
    const QMutexLocker locker(&thumbnailMutex());
    thumbnailFolder() = folder;
}

QImage AssetThumbnails::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString folder;
    {
        const QMutexLocker locker(&thumbnailMutex());
        folder = thumbnailFolder();
    }
    // <pack id>/<version>/<path in the pack>
    const QString decoded = QUrl::fromPercentEncoding(id.toUtf8());
    const QStringList parts = decoded.split(QLatin1Char('/'));
    if (folder.isEmpty() || parts.size() < 3)
        return {};
    const QString path = parts.mid(2).join(QLatin1Char('/'));
    static const QHash<QString, QByteArray> formats{{QStringLiteral("png"), "png"}, {QStringLiteral("jpg"), "jpeg"},
                                                    {QStringLiteral("jpeg"), "jpeg"}, {QStringLiteral("webp"), "webp"}};
    const QByteArray format = formats.value(QFileInfo(path).suffix().toLower());
    if (format.isEmpty())
        return {};
    const QList<AssetLibrary::Pack> packs = AssetLibrary::readInstalled(folder);
    const AssetLibrary::Pack *pack = findPack(packs, parts.at(0));
    const AssetLibrary::PackFile *file = pack && pack->version == parts.at(1) ? pack->file(path) : nullptr;
    QByteArray bytes;
    QString why;
    if (!file || !bytesOf(*pack, *file, &bytes, &why))
        return {};
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, format);
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(false);
    const QSize whole = reader.size();
    if (!whole.isValid() || whole.width() > largestSide || whole.height() > largestSide)
        return {};
    if (requestedSize.isValid() && (whole.width() > requestedSize.width() || whole.height() > requestedSize.height()))
        reader.setScaledSize(whole.scaled(requestedSize, Qt::KeepAspectRatio));
    const QImage image = reader.read();
    if (size)
        *size = image.size();
    return image;
}

}  // namespace qq
