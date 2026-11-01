// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "gamebuild.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>

#include <limits>
#include <utility>

#include "confinement.h"
#include "projectworkspace.h"
#include "store.h"

namespace qq {

namespace {

const char *const manifestName = "game.json";
/// What was built before is read only for how many times, so this is more than a real one needs.
constexpr qint64 largestManifestRead = 64 * 1024 * 1024;
constexpr int longestTitle = 80;

QString tr(const char *text)
{
    return GameBuild::tr(text);
}

QVariantMap refused(const QString &problem)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("problem"), problem}};
}

struct Found {
    QStringList files;
    QStringList leftOut;
    QString problem;
};

/// Every file under `relative`, or the first thing that keeps the game from being whole, in `problem`.
void walk(const QString &root, const QString &relative, int mostFiles, Found &found)
{
    const QFileInfoList entries = QDir(QDir(root).filePath(relative))
                                      .entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                                     QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString path = relative + QLatin1Char('/') + entry.fileName();
        if (entry.fileName().startsWith(QLatin1Char('.'))) {
            found.leftOut << path;
            continue;
        }
        if (entry.isSymLink() || entry.isJunction()) {
            found.problem = tr("%1 is a link. A game carries its own files: copy the file there, then build.").arg(path);
            return;
        }
        if (!ProjectWorkspace::plainRelative(path)) {
            found.problem = tr("%1 has a name a game cannot carry: rename it, then build.").arg(path);
            return;
        }
        if (entry.isDir()) {
            walk(root, path, mostFiles, found);
            if (!found.problem.isEmpty())
                return;
        } else if (entry.isFile()) {
            if (found.files.size() >= mostFiles) {
                found.problem = tr("This project has more than %1 files, too many for one game.").arg(mostFiles);
                return;
            }
            found.files << path;
        } else {
            found.problem = tr("%1 is neither a file nor a folder.").arg(path);
            return;
        }
    }
}

/// Copy a file, counting from what is read and not from what the file claims, since a file can grow. "" or what went wrong.
QString copyCounted(const QString &from, const QString &to, qint64 mostBytes, qint64 &bytes, QByteArray &sha256)
{
    QFile in(from);
    if (!in.open(QIODevice::ReadOnly))
        return tr("%1 could not be read.").arg(from);
    if (!QDir().mkpath(QFileInfo(to).absolutePath()))
        return tr("The folder for %1 could not be made.").arg(to);
    QFile out(to);
    if (!out.open(QIODevice::NewOnly))
        return tr("%1 could not be written.").arg(to);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(1 << 20, Qt::Uninitialized);
    while (true) {
        const qint64 got = in.read(buffer.data(), buffer.size());
        if (got < 0)
            return tr("%1 could not be read.").arg(from);
        if (got == 0)
            break;
        bytes += got;
        if (bytes > mostBytes)
            return tr("This project is more than %1 MB, too large for one game.").arg(mostBytes / (1024 * 1024));
        hash.addData(QByteArrayView(buffer.constData(), got));
        if (out.write(buffer.constData(), got) != got)
            return tr("%1 could not be written: %2").arg(to, out.errorString());
    }
    if (!out.flush())
        return tr("%1 could not be written: %2").arg(to, out.errorString());
    sha256 = hash.result().toHex();
    return {};
}

/// "orb-run" -> "Orb Run", as the Library names a folder.
QString titleOf(const QString &folder)
{
    QStringList words = QString(folder).replace(QLatin1Char('_'), QLatin1Char(' ')).replace(QLatin1Char('-'), QLatin1Char(' '))
                            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &word : words)
        word[0] = word[0].toUpper();
    const QString title = words.join(QLatin1Char(' ')).simplified().left(longestTitle);
    return title.isEmpty() ? tr("Untitled") : title;
}

QString platformName()
{
    const QString kernel = QSysInfo::kernelType();
    const QString system = kernel == QStringLiteral("winnt") ? QStringLiteral("windows") : kernel;
    return system + QLatin1Char('-') + QSysInfo::currentCpuArchitecture();
}

/// How many times the game there has been built. Its folder is only this person's, but it is still only read for a number.
int builtBefore(const QString &game)
{
    QFile file(QDir(game).filePath(QLatin1String(manifestName)));
    if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly))
        return 0;
    const qint64 build = QJsonDocument::fromJson(file.read(largestManifestRead)).object().value(QStringLiteral("build")).toInteger();
    return build > 0 && build < std::numeric_limits<int>::max() ? int(build) : 0;
}

/// What a build that stopped halfway, or a game that was in use when it was replaced, left behind for this game.
void clearLeftovers(const QDir &games, const QString &folder)
{
    const QFileInfoList left = games.entryInfoList({QStringLiteral(".building-%1-*").arg(folder), QStringLiteral(".old-%1-*").arg(folder)},
                                                   QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : left) {
        if (!entry.isSymLink())
            QDir(entry.filePath()).removeRecursively();
    }
}

}  // namespace

GameBuild::GameBuild(QObject *parent) : QObject(parent) {}

GameBuild::~GameBuild()
{
    if (m_thread)
        m_thread->wait();
}

bool GameBuild::start(const QUrl &project, const QUrl &scene)
{
    if (m_thread)
        return false;
    const QString folder = project.toLocalFile();
    const QString file = scene.toLocalFile();
    const QString games = gamesFolder();
    QThread *thread = QThread::create([this, folder, file, games] { m_result = make(folder, file, games); });
    thread->setParent(this);
    connect(thread, &QThread::finished, this, [this, thread] {
        m_thread = nullptr;
        thread->deleteLater();
        emit busyChanged();
        emit finished(m_result);
    });
    m_thread = thread;
    thread->start();
    emit busyChanged();
    return true;
}

QString GameBuild::gamesFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("qor-os/games"));
}

QString GameBuild::folderNameFor(const QString &realProject)
{
    return GameStore::gameOf(QUrl::fromLocalFile(realProject));
}

QVariantMap GameBuild::make(const QString &project, const QString &scene, const QString &games, const Limits &limits)
{
    const QString root = project.isEmpty() ? QString() : Confinement::realPath(project);
    if (root.isEmpty() || !QFileInfo(root).isDir())
        return refused(tr("There is no project folder at %1.").arg(project));

    // Its first scene: a saved file directly in scenes/, since that is what makes the copy's folder its package (Confinement).
    const QString relative = QDir::isAbsolutePath(scene) ? QDir(root).relativeFilePath(Confinement::realPath(scene)) : scene;
    if (ProjectWorkspace::fileInside(root, relative).isEmpty()
        || !relative.startsWith(QStringLiteral("scenes/")) || relative.count(QLatin1Char('/')) != 1
        || !relative.endsWith(QStringLiteral(".qml")))
        return refused(tr("A game starts in a saved scene in this project's scenes folder, and %1 is not one.").arg(scene));

    const QString gamesPath = QDir::cleanPath(games);
    if (!QDir().mkpath(gamesPath))
        return refused(tr("The folder for games, %1, could not be made.").arg(gamesPath));
    if (Confinement::within(Confinement::realPath(gamesPath), root))
        return refused(tr("This project holds the folder games are built into, %1.").arg(gamesPath));

    Found found;
    for (const QString &folder : {QStringLiteral("scenes"), QStringLiteral("logic"), QStringLiteral("prefabs"), QStringLiteral("assets")}) {
        const QFileInfo top(QDir(root).filePath(folder));
        if (top.isSymLink() || top.isJunction())
            return refused(tr("%1 is a link. A game carries its own files: copy them there, then build.").arg(folder));
        if (top.exists() && !top.isDir())
            return refused(tr("%1 is not a folder.").arg(folder));
        if (top.isDir())
            walk(root, folder, limits.files, found);
        if (!found.problem.isEmpty())
            return refused(found.problem);
    }

    const QString name = folderNameFor(root);
    const QDir gamesDir(gamesPath);
    clearLeftovers(gamesDir, name);
    QTemporaryDir staging(gamesDir.filePath(QStringLiteral(".building-%1-XXXXXX").arg(name)));
    if (!staging.isValid())
        return refused(tr("A game could not be built in %1: %2").arg(gamesPath, staging.errorString()));

    QJsonObject files;
    qint64 bytes = 0;
    for (const QString &file : std::as_const(found.files)) {
        QByteArray sha256;
        const QString wrong = copyCounted(QDir(root).filePath(file), staging.filePath(file), limits.bytes, bytes, sha256);
        if (!wrong.isEmpty())
            return refused(wrong);
        files.insert(file, QString::fromLatin1(sha256));
    }

    const QString target = gamesDir.filePath(name);
    const QFileInfo existing(target);
    if (existing.isSymLink() || (existing.exists() && !existing.isDir()))
        return refused(tr("%1 is in the way of this game, and is not one.").arg(target));
    const int build = existing.exists() ? builtBefore(target) + 1 : 1;
    const QString title = titleOf(QFileInfo(root).fileName());

    const QJsonObject manifest{
        {QStringLiteral("format"), manifestFormat},
        {QStringLiteral("kind"), QStringLiteral("game")},
        {QStringLiteral("title"), title},
        {QStringLiteral("scene"), relative},
        {QStringLiteral("build"), build},
        {QStringLiteral("built"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("engine"), QJsonObject{{QStringLiteral("id"), QStringLiteral("qq")},
                                               {QStringLiteral("version"), QStringLiteral(QQ_ENGINE_VERSION)}}},
        {QStringLiteral("platform"), platformName()},
        {QStringLiteral("files"), files},
    };
    QSaveFile written(staging.filePath(QLatin1String(manifestName)));
    if (!written.open(QIODevice::WriteOnly) || written.write(QJsonDocument(manifest).toJson()) < 0 || !written.commit())
        return refused(tr("What this game holds could not be written: %1").arg(written.errorString()));

    // Into place whole. What is playing the old one, on a system that lets a folder move under it, goes on playing it.
    QString old;
    if (existing.exists()) {
        old = gamesDir.filePath(QStringLiteral(".old-%1-%2").arg(name, QFileInfo(staging.path()).fileName().right(6)));
        if (!QDir().rename(target, old))
            return refused(tr("%1 could not be replaced. If it is playing, close it, then build.").arg(title));
    }
    if (!QDir().rename(staging.path(), target)) {
        if (!old.isEmpty())
            QDir().rename(old, target);
        return refused(tr("%1 could not be put in place in %2.").arg(title, gamesPath));
    }
    staging.setAutoRemove(false);
    if (!old.isEmpty())
        QDir(old).removeRecursively();

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("problem"), QString()},
            {QStringLiteral("title"), title},
            {QStringLiteral("folder"), target},
            {QStringLiteral("scene"), QDir(target).filePath(relative)},
            {QStringLiteral("build"), build},
            {QStringLiteral("files"), int(found.files.size())},
            {QStringLiteral("bytes"), bytes},
            {QStringLiteral("leftOut"), found.leftOut}};
}

}  // namespace qq
