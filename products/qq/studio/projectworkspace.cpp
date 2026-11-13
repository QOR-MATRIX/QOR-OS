// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary

#include "projectworkspace.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QStringView>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <optional>

#include "confinement.h"

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#elif defined(Q_OS_WIN)
#include <QtCore/qt_windows.h>
#endif

namespace qq {

namespace {

/// The real path of a project's folder, or "" for what is not on this computer. A folder that has gone keeps the path it
/// would have, so it still finds what was kept for it.
QString realFolder(const QUrl &folder)
{
    return folder.isLocalFile() ? Confinement::realPath(folder.toLocalFile()) : QString();
}

/// A file of the store as bytes, or nothing with `why` saying what is wrong with it: "" if it just is not there. Only a
/// regular file is opened, so a pipe left there cannot hang the Studio, and no more than the most it can be is read.
std::optional<QByteArray> readStoreFile(const QString &path, QString *why, qint64 most = ProjectWorkspace::largestFile)
{
    why->clear();
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return std::nullopt;
    if (info.isSymLink() || !info.isFile()) {
        *why = ProjectWorkspace::tr("is not a plain file");
        return std::nullopt;
    }
    if (info.size() > most) {
        *why = ProjectWorkspace::tr("is too large");
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *why = ProjectWorkspace::tr("could not be opened");
        return std::nullopt;
    }
    QByteArray bytes = file.read(most + 1);
    if (bytes.size() > most) {
        *why = ProjectWorkspace::tr("is too large");
        return std::nullopt;
    }
    return bytes;
}

/// Write a file of the store, whole or not at all, for this person's eyes. A link in its place is replaced, never written
/// through.
bool writeStoreFile(const QString &path, const QJsonObject &object, qint64 most = ProjectWorkspace::largestFile)
{
    const QByteArray bytes = QJsonDocument(object).toJson();
    if (bytes.size() > most)
        return false;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    if (QFileInfo(path).isSymLink() && !QFile::remove(path))
        return false;
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    out.write(bytes);
    return out.commit();
}

/// A whole number from what was read: a real number of a sensible size, never a string that looks like one.
bool numberFrom(const QVariant &value, int *out)
{
    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::Double:
        break;
    default:
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::abs(number) > 1e6)
        return false;
    *out = static_cast<int>(number);
    return true;
}

/// The digest of a saved file a recovery copy is of, or "" for one that cannot be read as a plain file in the project.
QString digestOf(const QString &realProject, const QString &relative)
{
    const QString real = ProjectWorkspace::fileInside(realProject, relative);
    if (real.isEmpty())
        return {};
    QFile file(real);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

/// One part of a recovery copy as it was kept, read as a stranger's: its fields, or nothing, with `*why` saying why not, if it
/// is there and wrong. "" for `why` means it is just not there, or no longer differs from its file.
QVariantMap recoveryPart(const QJsonObject &kept, const QString &key, const QString &root, const QRegularExpression &allowed, QString *why)
{
    why->clear();
    const QJsonValue value = kept.value(key);
    if (value.isUndefined())
        return {};
    const QString refused = ProjectWorkspace::tr("Part of this project's recovery copy named something that is not a plain file in this project, "
                                                 "or was not what was kept, so that part was left alone.");
    const QJsonObject part = value.toObject();
    if (!value.isObject() || !part.value(QStringLiteral("text")).isString() || !part.value(QStringLiteral("file")).isString()) {
        *why = refused;
        return {};
    }
    const QString file = part.value(QStringLiteral("file")).toString();
    const QString text = part.value(QStringLiteral("text")).toString();
    const QString name = part.value(QStringLiteral("name")).toString();
    const bool plainName = std::none_of(name.cbegin(), name.cend(), [](QChar c) { return c.unicode() < 0x20; }) && name.size() <= 200;
    if (!plainName || (!file.isEmpty() && (!ProjectWorkspace::plainRelative(file) || !allowed.match(file).hasMatch()))) {
        *why = refused;
        return {};
    }
    QVariantMap out{{QStringLiteral("file"), file}, {QStringLiteral("text"), text}, {QStringLiteral("name"), name},
                    {QStringLiteral("changed"), false}, {QStringLiteral("gone"), false}};
    if (file.isEmpty())
        return out;
    const QFileInfo there(QDir(root).filePath(file));
    if (!there.exists() && !there.isSymLink()) {
        out.insert(QStringLiteral("gone"), true);
        return out;
    }
    const QString real = ProjectWorkspace::fileInside(root, file);
    if (real.isEmpty()) {
        *why = refused;
        return {};
    }
    QFile onDisk(real);
    // What is the same as its file is no longer unsaved work: a copy can outlive a save by the moment a crash took.
    if (onDisk.open(QIODevice::ReadOnly) && onDisk.size() == text.toUtf8().size() && onDisk.readAll() == text.toUtf8())
        return {};
    out.insert(QStringLiteral("changed"), digestOf(root, file) != part.value(QStringLiteral("sha256")).toString());
    return out;
}

/// The recent list as it was kept: absolute paths with no control characters in them, each once, the most there can be.
QStringList readRecent()
{
    QString why;
    const std::optional<QByteArray> bytes = readStoreFile(ProjectWorkspace::recentFile(), &why);
    if (!bytes)
        return {};
    const QJsonObject object = QJsonDocument::fromJson(*bytes).object();
    if (object.value(QStringLiteral("version")).toInt() != ProjectWorkspace::schemaVersion)
        return {};
    QStringList paths;
    const QJsonArray projects = object.value(QStringLiteral("projects")).toArray();
    for (const QJsonValue &entry : projects) {
        const QString path = entry.toString();
        const bool plain = std::none_of(path.cbegin(), path.cend(), [](QChar c) { return c.unicode() < 0x20; });
        if (path.isEmpty() || path.size() > 4096 || !plain || !QDir::isAbsolutePath(path) || paths.contains(path))
            continue;
        paths << path;
        if (paths.size() == ProjectWorkspace::mostRecent)
            break;
    }
    return paths;
}

}  // namespace

ProjectWorkspace::ProjectWorkspace(QObject *parent)
    : QObject(parent), m_session(QUuid::createUuid().toString(QUuid::Id128))
{
}

ProjectWorkspace::~ProjectWorkspace() = default;

QString ProjectWorkspace::folderIdentity(const QString &realFolder)
{
    if (realFolder.isEmpty())
        return {};
#ifdef Q_OS_UNIX
    struct stat seen{};
    if (::stat(QFile::encodeName(realFolder).constData(), &seen) != 0)
        return {};
    return QStringLiteral("%1:%2").arg(quint64(seen.st_dev)).arg(quint64(seen.st_ino));
#elif defined(Q_OS_WIN)
    QString native = QDir::toNativeSeparators(realFolder);
    if (!native.startsWith(QStringLiteral("\\\\?\\"))) {
        native = native.startsWith(QStringLiteral("\\\\"))
                     ? QStringLiteral("\\\\?\\UNC\\") + native.mid(2)
                     : QStringLiteral("\\\\?\\") + native;
    }
    const HANDLE folder = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (folder == INVALID_HANDLE_VALUE)
        return {};
    FILE_ID_INFO seen{};
    const bool identified = GetFileInformationByHandleEx(folder, FileIdInfo, &seen, DWORD(sizeof(seen)));
    CloseHandle(folder);
    if (!identified)
        return {};
    const QByteArray id(reinterpret_cast<const char *>(seen.FileId.Identifier), sizeof(seen.FileId.Identifier));
    if (std::all_of(id.cbegin(), id.cend(), [](char byte) { return byte == '\0'; }))
        return {};
    return QStringLiteral("win:%1:%2").arg(quint64(seen.VolumeSerialNumber), 0, 16).arg(QString::fromLatin1(id.toHex()));
#else
    return {};
#endif
}

QString ProjectWorkspace::storeFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/workspaces");
}

QString ProjectWorkspace::fileFor(const QString &realProject)
{
    const QByteArray hash = QCryptographicHash::hash(realProject.toUtf8(), QCryptographicHash::Sha256).toHex();
    return storeFolder() + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".json");
}

namespace {

QString recoveryPrefixFor(const QString &realProject)
{
    return QString::fromLatin1(QCryptographicHash::hash(realProject.toUtf8(), QCryptographicHash::Sha256).toHex())
           + QStringLiteral(".recovery-");
}

QString lockFor(const QString &copy)
{
    return copy.left(copy.size() - 5) + QStringLiteral(".lock");
}

/// Whether the Studio that wrote a copy is still running: its lock is held. One that is not is taken and let go at once,
/// which is what tells.
bool heldByARunningStudio(const QString &lock)
{
    if (!QFileInfo::exists(lock))
        return false;
    QLockFile probe(lock);
    probe.setStaleLockTime(0);
    if (probe.tryLock(0)) {
        probe.unlock();
        return false;
    }
    return probe.error() == QLockFile::LockFailedError;
}

}  // namespace

QString ProjectWorkspace::recoveryFileFor(const QString &realProject, const QString &session)
{
    return storeFolder() + QLatin1Char('/') + recoveryPrefixFor(realProject) + session + QStringLiteral(".json");
}

QString ProjectWorkspace::recentFile()
{
    return storeFolder() + QStringLiteral("/recent.json");
}

bool ProjectWorkspace::plainRelative(const QString &relative)
{
    if (relative.isEmpty() || relative.size() > 1024 || QDir::isAbsolutePath(relative))
        return false;
    // Nothing the file system or a URL would read as more than a name: a drive or a Windows stream, the other slash, or
    // what a URL takes apart or decodes ("%2e%2e%2f" is "../" by the time it is a file's path).
    static const QString meaningful = QStringLiteral("\\:%#?*\"<>|");
    for (const QChar c : relative) {
        if (c.unicode() < 0x20 || meaningful.contains(c))
            return false;
    }
    const QList<QStringView> parts = QStringView(relative).split(QLatin1Char('/'));
    for (const QStringView part : parts) {
        // Windows ignores what a name ends in of these, so ".. " is "..".
        if (part.isEmpty() || part.endsWith(QLatin1Char('.')) || part.endsWith(QLatin1Char(' ')))
            return false;
    }
    return true;
}

QString ProjectWorkspace::fileInside(const QString &realProject, const QString &relative)
{
    if (realProject.isEmpty() || !plainRelative(relative))
        return {};
    const QString real = Confinement::realPath(QDir(realProject).filePath(relative));
    return QFileInfo(real).isFile() && Confinement::within(real, realProject) ? real : QString();
}

QVariantMap ProjectWorkspace::fitWindow(const QVariantMap &stored, const QList<QRect> &screens)
{
    QVariantMap fitted;
    const QVariant maximized = stored.value(QStringLiteral("maximized"));
    if (maximized.typeId() == QMetaType::Bool && maximized.toBool())
        fitted.insert(QStringLiteral("maximized"), true);

    int width = 0;
    int height = 0;
    if (!numberFrom(stored.value(QStringLiteral("width")), &width)
        || !numberFrom(stored.value(QStringLiteral("height")), &height) || width <= 0 || height <= 0)
        return fitted;
    int x = 0;
    int y = 0;
    const bool placed = numberFrom(stored.value(QStringLiteral("x")), &x)
                        && numberFrom(stored.value(QStringLiteral("y")), &y);

    // The screen it is most on, or the first if it is on none.
    QRect screen = screens.value(0);
    qint64 most = 0;
    if (placed) {
        for (const QRect &candidate : screens) {
            const QRect overlap = candidate.intersected(QRect(x, y, width, height));
            const qint64 area = qint64(overlap.width()) * overlap.height();
            if (area > most) {
                most = area;
                screen = candidate;
            }
        }
    }
    // With no screen to measure by, only what any window can be.
    const int widest = screen.isEmpty() ? 16384 : screen.width();
    const int tallest = screen.isEmpty() ? 16384 : screen.height();
    width = qMin(qMax(width, qMin(int(smallestWidth), widest)), widest);
    height = qMin(qMax(height, qMin(int(smallestHeight), tallest)), tallest);
    fitted.insert(QStringLiteral("width"), width);
    fitted.insert(QStringLiteral("height"), height);
    if (placed) {
        if (!screen.isEmpty()) {
            x = qBound(screen.left(), x, screen.left() + screen.width() - width);
            y = qBound(screen.top(), y, screen.top() + screen.height() - height);
        }
        fitted.insert(QStringLiteral("x"), x);
        fitted.insert(QStringLiteral("y"), y);
    }
    return fitted;
}

QVariantMap ProjectWorkspace::fitCamera(const QVariantMap &stored)
{
    QVariantMap fitted;
    for (const char *key : {"yaw", "pitch", "distance", "x", "y", "z"}) {
        const QVariant value = stored.value(QLatin1String(key));
        const bool isNumber = value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong || value.typeId() == QMetaType::Double;
        const double number = isNumber ? value.toDouble() : 0;
        if (!isNumber || !std::isfinite(number) || std::abs(number) > 1e6)
            return {};
        fitted.insert(QLatin1String(key), number);
    }
    fitted.insert(QStringLiteral("pitch"), qBound(-5.0, fitted.value(QStringLiteral("pitch")).toDouble(), 85.0));
    fitted.insert(QStringLiteral("distance"), qBound(1.5, fitted.value(QStringLiteral("distance")).toDouble(), 80.0));
    return fitted;
}

QVariantMap ProjectWorkspace::load(const QUrl &project) const
{
    QVariantMap out{{QStringLiteral("found"), false},          {QStringLiteral("problem"), QString()},
                    {QStringLiteral("scene"), QString()},      {QStringLiteral("logic"), QString()},
                    {QStringLiteral("panels"), QVariantMap()}, {QStringLiteral("camera"), QVariantMap()},
                    {QStringLiteral("window"), QVariantMap()}};
    const QString root = realFolder(project);
    if (root.isEmpty())
        return out;
    const auto refuse = [&out](const QString &why) {
        out.insert(QStringLiteral("problem"), tr("This project's saved workspace %1, so it opened fresh.").arg(why));
        return out;
    };

    QString why;
    const std::optional<QByteArray> bytes = readStoreFile(fileFor(root), &why);
    if (!bytes)
        return why.isEmpty() ? out : refuse(why);
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(*bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return refuse(tr("could not be read"));
    const QJsonObject kept = document.object();
    const QJsonValue version = kept.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != schemaVersion)
        return refuse(tr("is of a version this Studio does not know"));
    if (kept.value(QStringLiteral("project")).toString() != root)
        return refuse(tr("was written for another folder"));

    out.insert(QStringLiteral("found"), true);
    QStringList problems;
    const auto path = [&](const QString &key) -> QString {
        // Nothing was open, or what is there is no path at all.
        const QString relative = kept.value(key).toString();
        if (relative.isEmpty())
            return {};
        // What it names is not repeated unless it is known to be a plain path: this could say anything.
        const QString refused = tr("It named something that is not a plain file inside this project, which was left closed.");
        if (!plainRelative(relative)) {
            problems << refused;
            return {};
        }
        if (fileInside(root, relative).isEmpty()) {
            const QFileInfo there(QDir(root).filePath(relative));
            problems << (there.exists() || there.isSymLink() ? refused : tr("%1 is no longer in this project.").arg(relative));
            return {};
        }
        return relative;
    };
    out.insert(QStringLiteral("scene"), path(QStringLiteral("scene")));
    out.insert(QStringLiteral("logic"), path(QStringLiteral("logic")));
    problems.removeDuplicates();
    out.insert(QStringLiteral("problem"), problems.join(QLatin1Char(' ')));

    QVariantMap panels;
    const QJsonObject keptPanels = kept.value(QStringLiteral("panels")).toObject();
    for (auto it = keptPanels.begin(); it != keptPanels.end(); ++it) {
        if (it.value().isBool())
            panels.insert(it.key(), it.value().toBool());
    }
    out.insert(QStringLiteral("panels"), panels);
    out.insert(QStringLiteral("camera"), fitCamera(kept.value(QStringLiteral("camera")).toObject().toVariantMap()));

    QList<QRect> screens;
    const QList<QScreen *> all = QGuiApplication::screens();
    for (const QScreen *screen : all)
        screens << screen->availableGeometry();
    out.insert(QStringLiteral("window"),
               fitWindow(kept.value(QStringLiteral("window")).toObject().toVariantMap(), screens));
    return out;
}

bool ProjectWorkspace::save(const QUrl &project, const QVariantMap &state)
{
    const QString root = realFolder(project);
    if (root.isEmpty())
        return false;
    const auto path = [&state](const QString &key) {
        const QString relative = state.value(key).toString();
        return plainRelative(relative) ? relative : QString();
    };
    return writeStoreFile(
        fileFor(root),
        QJsonObject{{QStringLiteral("version"), schemaVersion},
                    {QStringLiteral("project"), root},
                    {QStringLiteral("scene"), path(QStringLiteral("scene"))},
                    {QStringLiteral("logic"), path(QStringLiteral("logic"))},
                    {QStringLiteral("panels"), QJsonObject::fromVariantMap(state.value(QStringLiteral("panels")).toMap())},
                    {QStringLiteral("camera"), QJsonObject::fromVariantMap(state.value(QStringLiteral("camera")).toMap())},
                    {QStringLiteral("window"), QJsonObject::fromVariantMap(state.value(QStringLiteral("window")).toMap())}});
}

bool ProjectWorkspace::keepRecovery(const QUrl &project, const QVariantMap &work)
{
    const QString root = realFolder(project);
    if (root.isEmpty())
        return false;
    QJsonObject kept{{QStringLiteral("version"), schemaVersion}, {QStringLiteral("project"), root},
                     {QStringLiteral("folder"), folderIdentity(root)}};
    for (const char *key : {"scene", "logic"}) {
        const QVariantMap part = work.value(QLatin1String(key)).toMap();
        if (part.isEmpty())
            continue;
        const QString file = part.value(QStringLiteral("file")).toString();
        kept.insert(QLatin1String(key), QJsonObject{{QStringLiteral("name"), part.value(QStringLiteral("name")).toString()},
                                                    {QStringLiteral("file"), file},
                                                    {QStringLiteral("text"), part.value(QStringLiteral("text")).toString()},
                                                    {QStringLiteral("sha256"), file.isEmpty() ? QString() : digestOf(root, file)}});
    }
    if (kept.size() == 3) {
        dropRecovery(project);
        return true;
    }
    // Locked first, and for as long as the copy is there: a Studio opening this project meanwhile leaves it be.
    const QString copy = recoveryFileFor(root, m_session);
    if (m_lockedFor != root) {
        m_lock.reset();
        m_lockedFor.clear();
        if (!QDir().mkpath(storeFolder()))
            return false;
        auto lock = std::make_unique<QLockFile>(lockFor(copy));
        lock->setStaleLockTime(0);
        if (!lock->tryLock(0))
            return false;
        m_lock = std::move(lock);
        m_lockedFor = root;
    }
    return writeStoreFile(copy, kept, largestRecovery);
}

namespace {

/// One copy, as a stranger's: its parts, checked, or what is wrong with it in `*why`; nothing and no `why` for a copy none of
/// which is unsaved work any more.
QVariantMap readCopy(const QString &path, const QString &root, QString *why)
{
    why->clear();
    const auto refuse = [why](const QString &reason) {
        *why = ProjectWorkspace::tr("A recovery copy of this project %1, so it was left alone.").arg(reason);
        return QVariantMap();
    };
    QString unreadable;
    const std::optional<QByteArray> bytes = readStoreFile(path, &unreadable, ProjectWorkspace::largestRecovery);
    if (!bytes)
        return refuse(unreadable.isEmpty() ? ProjectWorkspace::tr("could not be read") : unreadable);
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(*bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return refuse(ProjectWorkspace::tr("could not be read"));
    const QJsonObject kept = document.object();
    const QJsonValue version = kept.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != ProjectWorkspace::schemaVersion)
        return refuse(ProjectWorkspace::tr("is of a version this Studio does not know"));
    if (kept.value(QStringLiteral("project")).toString() != root)
        return refuse(ProjectWorkspace::tr("was written for another folder"));
    const QString identity = ProjectWorkspace::folderIdentity(root);
    if (identity.isEmpty())
        return refuse(ProjectWorkspace::tr("could not be tied to this project folder"));
#ifdef Q_OS_WIN
    const QString recorded = kept.value(QStringLiteral("folder")).toString();
    static const QRegularExpression timestamp(QStringLiteral("^-?[0-9]+\\z"));
    if (recorded.isEmpty() || timestamp.match(recorded).hasMatch())
        return refuse(ProjectWorkspace::tr("records only an older creation-time identity and needs manual review"));
#endif
    if (kept.value(QStringLiteral("folder")).toString() != identity)
        return refuse(ProjectWorkspace::tr("was written for another folder that was at this place before"));

    static const QRegularExpression aScene(QStringLiteral("^scenes/[^/]+\\.qml$"));
    static const QRegularExpression aLogicFile(QStringLiteral("^logic/.+\\.qml$"));
    QStringList problems;
    QString wrong;
    QVariantMap scene = recoveryPart(kept, QStringLiteral("scene"), root, aScene, &wrong);
    if (!wrong.isEmpty())
        problems << wrong;
    QVariantMap logic = recoveryPart(kept, QStringLiteral("logic"), root, aLogicFile, &wrong);
    if (!wrong.isEmpty())
        problems << wrong;
    // A logic file is its file. A part with no file to go back to is a scene not saved yet, which is all it can be.
    if (!logic.isEmpty() && logic.value(QStringLiteral("file")).toString().isEmpty()) {
        logic.clear();
        problems << ProjectWorkspace::tr("Part of this project's recovery copy named no file, so that part was left alone.");
    }
    problems.removeDuplicates();
    *why = problems.join(QLatin1Char(' '));
    QVariantMap copy;
    if (!scene.isEmpty()) {
        if (scene.value(QStringLiteral("name")).toString().trimmed().isEmpty())
            scene.insert(QStringLiteral("name"), QStringLiteral("untitled"));
        copy.insert(QStringLiteral("scene"), scene);
    }
    if (!logic.isEmpty()) {
        const QString file = logic.value(QStringLiteral("file")).toString();
        logic.insert(QStringLiteral("name"), file.mid(6, file.size() - 10));
        copy.insert(QStringLiteral("logic"), logic);
    }
    return copy;
}

}  // namespace

QVariantMap ProjectWorkspace::loadRecovery(const QUrl &project)
{
    QVariantMap out{{QStringLiteral("found"), false}, {QStringLiteral("problem"), QString()}, {QStringLiteral("more"), 0}};
    const QString root = realFolder(project);
    if (root.isEmpty())
        return out;
    // Every session's copy of this project, a pipe or a folder in a copy's place too, so that it is refused and said; newest
    // first.
    const QString prefix = recoveryPrefixFor(root);
    const QFileInfoList found = QDir(storeFolder()).entryInfoList({prefix + QStringLiteral("*.json")},
                                                                  QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                                                                  QDir::Time);
    static const QRegularExpression aSession(QStringLiteral("^[A-Za-z0-9-]{1,64}$"));
    QStringList problems;
    QVariantList offered;
    int looked = 0;
    for (const QFileInfo &entry : found) {
        const QString session = entry.fileName().mid(prefix.size(), entry.fileName().size() - prefix.size() - 5);
        if (session == m_session)
            continue;
        if (++looked > mostCopies) {
            problems << tr("This project has more recovery copies than are looked at, and those were left alone.");
            break;
        }
        if (!aSession.match(session).hasMatch()) {
            problems << tr("A recovery copy of this project is named as no Studio names one, so it was left alone.");
            continue;
        }
        // Another Studio still at work on this project: its copy is its own, and it has not crashed.
        if (heldByARunningStudio(lockFor(entry.filePath())))
            continue;
        QString why;
        QVariantMap copy = readCopy(entry.filePath(), root, &why);
        if (!why.isEmpty())
            problems << why;
        if (copy.isEmpty()) {
            // Whole, and none of it unsaved any more: a crash came between a save and the copy going.
            if (why.isEmpty())
                QFile::remove(entry.filePath());
            continue;
        }
        copy.insert(QStringLiteral("session"), session);
        offered << copy;
    }
    problems.removeDuplicates();
    out.insert(QStringLiteral("problem"), problems.join(QLatin1Char(' ')));
    if (!offered.isEmpty()) {
        out.insert(QStringLiteral("found"), true);
        out.insert(QStringLiteral("copy"), offered.first());
        out.insert(QStringLiteral("more"), int(offered.size()) - 1);
    }
    return out;
}

void ProjectWorkspace::dropRecovery(const QUrl &project)
{
    const QString root = realFolder(project);
    if (root.isEmpty())
        return;
    QFile::remove(recoveryFileFor(root, m_session));
    if (m_lockedFor == root) {
        m_lock.reset();
        m_lockedFor.clear();
    }
}

void ProjectWorkspace::dropRecoveryOf(const QUrl &project, const QString &session)
{
    static const QRegularExpression aSession(QStringLiteral("^[A-Za-z0-9-]{1,64}$"));
    const QString root = realFolder(project);
    if (root.isEmpty() || session == m_session || !aSession.match(session).hasMatch())
        return;
    const QString copy = recoveryFileFor(root, session);
    if (heldByARunningStudio(lockFor(copy)))
        return;
    QFile::remove(copy);
}

bool ProjectWorkspace::isFolder(const QUrl &folder) const
{
    return folder.isLocalFile() && QFileInfo(folder.toLocalFile()).isDir();
}

bool ProjectWorkspace::sameProject(const QUrl &a, const QUrl &b) const
{
    const QString first = realFolder(a);
    return !first.isEmpty() && Confinement::within(first, realFolder(b)) && Confinement::within(realFolder(b), first);
}

QString ProjectWorkspace::relativeInside(const QUrl &project, const QUrl &file) const
{
    if (!project.isLocalFile() || !file.isLocalFile())
        return {};
    // As it is named from the project, which may itself be reached through a link.
    const QString relative = QDir(project.toLocalFile()).relativeFilePath(file.toLocalFile());
    return fileInside(realFolder(project), relative).isEmpty() ? QString() : relative;
}

QVariantList ProjectWorkspace::recentProjects() const
{
    QVariantList rows;
    const QStringList paths = readRecent();
    for (const QString &path : paths) {
        const QFileInfo info(path);
        rows << QVariantMap{{QStringLiteral("path"), path},
                            {QStringLiteral("url"), QUrl::fromLocalFile(path)},
                            {QStringLiteral("name"), info.fileName().isEmpty() ? path : info.fileName()},
                            {QStringLiteral("missing"), !info.isDir()}};
    }
    return rows;
}

void ProjectWorkspace::noteRecent(const QUrl &project)
{
    const QString root = realFolder(project);
    if (root.isEmpty())
        return;
    QStringList paths = readRecent();
    paths.removeAll(root);
    paths.prepend(root);
    if (paths.size() > mostRecent)
        paths.resize(mostRecent);
    writeStoreFile(recentFile(), QJsonObject{{QStringLiteral("version"), schemaVersion},
                                             {QStringLiteral("projects"), QJsonArray::fromStringList(paths)}});
}

}  // namespace qq
