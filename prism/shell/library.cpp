// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "library.h"

#include "catalog.h"
#include "storeengine.h"
#include "storefronts.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace qor {

namespace {

constexpr int MediaDepth = 3;
constexpr int MediaPerFolder = 400;
constexpr int RecentCount = 12;
constexpr int HistoryCount = 200;
constexpr qint64 LargestManifest = 32 * 1024 * 1024;
constexpr int LongestTitle = 80;

const QStringList VideoSuffixes { u"mp4"_s, u"mkv"_s, u"webm"_s, u"mov"_s, u"m4v"_s, u"avi"_s };
const QStringList AudioSuffixes { u"mp3"_s, u"flac"_s, u"ogg"_s, u"opus"_s, u"wav"_s, u"m4a"_s, u"aac"_s };
const QStringList PictureSuffixes { u"jpg"_s, u"jpeg"_s, u"png"_s, u"webp"_s, u"gif"_s, u"heic"_s, u"avif"_s };

// A hue of its own for a name, the same on every run (qHash is seeded).
int hueOf(const QString &name)
{
    quint32 hash = 2166136261u;
    for (const QChar c : name)
        hash = (hash ^ c.unicode()) * 16777619u;
    return int(hash % 360);
}

// "orb-run" -> "Orb Run".
QString titleOf(const QString &folder)
{
    QStringList words = QString(folder).replace(u'_', u' ').replace(u'-', u' ').split(u' ', Qt::SkipEmptyParts);
    for (QString &word : words)
        word[0] = word[0].toUpper();
    return words.join(u' ');
}

// The scene a QQ game folder plays, or "" if it is not one.
QString sceneOf(const QDir &folder)
{
    const QDir scenes(folder.filePath(u"scenes"_s));
    if (!scenes.exists())
        return {};
    const QString own = scenes.filePath(folder.dirName() + u".qml"_s);
    if (QFileInfo(own).isFile())
        return own;
    const QStringList all = scenes.entryList({ u"*.qml"_s }, QDir::Files, QDir::Name);
    return all.isEmpty() ? QString() : scenes.filePath(all.first());
}

QVariantMap gameItem(const QDir &folder, const QString &scene, bool mine)
{
    const QString name = titleOf(folder.dirName());
    return { { u"id"_s, QString(u"qq:"_s + scene) },
             { u"kind"_s, u"game"_s },
             { u"source"_s, mine ? u"mine"_s : u"qq"_s },
             { u"title"_s, name },
             // Played from here it is the project's files as they stand, not a game built from them.
             { u"subtitle"_s, mine ? QCoreApplication::translate("Library", "Your QQ project, as it is now")
                                   : QCoreApplication::translate("Library", "A QQ game") },
             { u"glyph"_s, mine ? u"layers"_s : u"play"_s },
             { u"hue"_s, hueOf(name) },
             { u"path"_s, scene },
             { u"modified"_s, QFileInfo(scene).lastModified().toMSecsSinceEpoch() } };
}

// A game QQ Studio built (products/qq/studio/gamebuild.h), by what it says of itself in game.json: its title, the scene
// it starts in and how many times it has been built, and nothing else is taken from there. Any folder here could hold
// such a file, so it is a stranger's: anything wrong with it, a link or a junction anywhere it matters among that, and
// the folder is a QQ game like any other, known by its scenes. The Player's walls are what hold a game in.
QVariantMap builtGameItem(const QDir &folder)
{
    const QFileInfo manifest(folder.filePath(u"game.json"_s));
    const QFileInfo place(folder.path());
    if (place.isSymLink() || place.isJunction() || manifest.isSymLink() || !manifest.isFile()
        || manifest.size() > LargestManifest)
        return {};
    QFile file(manifest.filePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject game = QJsonDocument::fromJson(file.read(LargestManifest)).object();
    if (game.value(u"format"_s).toInt() != 1 || game.value(u"kind"_s).toString() != u"game"_s
        || game.value(u"engine"_s).toObject().value(u"id"_s).toString() != u"qq"_s)
        return {};
    const QString title = game.value(u"title"_s).toString().simplified();
    const int build = game.value(u"build"_s).toInt();
    if (title.isEmpty() || title.size() > LongestTitle || build < 1)
        return {};
    // A plain file directly in scenes/, as a build makes it: nothing that climbs, and nothing that leads out.
    const QString relative = game.value(u"scene"_s).toString();
    const QStringList parts = relative.split(u'/');
    if (parts.size() != 2 || parts.first() != u"scenes"_s || !parts.last().endsWith(u".qml"_s)
        || parts.last().contains(u'\\'))
        return {};
    const QFileInfo scene(folder.filePath(relative));
    const QFileInfo scenes(folder.filePath(u"scenes"_s));
    if (scene.isSymLink() || !scene.isFile() || scene.canonicalPath() != scenes.canonicalFilePath()
        || scenes.isSymLink() || scenes.isJunction())
        return {};
    QVariantMap item = gameItem(folder, scene.filePath(), false);
    item.insert(u"source"_s, u"built"_s);
    item.insert(u"title"_s, title);
    item.insert(u"subtitle"_s, QCoreApplication::translate("Library", "A game built in QQ · build %1").arg(build));
    item.insert(u"hue"_s, hueOf(title));
    item.insert(u"build"_s, build);
    return item;
}

// The projects QQ Studio made here, or a person said they trust (QQ's SceneIO keeps the list). A folder in Documents is
// no one's own game just for being there: a download unpacked there would otherwise be listed as "Made by you", one
// press from playing.
QSet<QString> ownProjects()
{
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/qor-os/own-projects.json"_s);
    QSet<QString> own;
    if (!file.open(QIODevice::ReadOnly))
        return own;
    for (const QJsonValue &value : QJsonDocument::fromJson(file.readAll()).array())
        own.insert(value.toString());
    return own;
}

void findGames(const QString &root, int depth, bool mine, const QSet<QString> &own, QVariantList &into,
               QSet<QString> &seen)
{
    const QDir dir(root);
    if (!dir.exists() || depth <= 0)
        return;
    for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        // A game being built, or being replaced: hidden by its name on every system, whatever else a system calls
        // hidden.
        if (name.startsWith(u'.'))
            continue;
        const QDir folder(dir.filePath(name));
        const QVariantMap built = mine ? QVariantMap() : builtGameItem(folder);
        if (!built.isEmpty()) {
            const QString canonical = QFileInfo(built.value(u"path"_s).toString()).canonicalFilePath();
            if (!seen.contains(canonical)) {
                seen.insert(canonical);
                into.append(built);
            }
            continue;
        }
        const QString scene = sceneOf(folder);
        if (!scene.isEmpty()) {
            const QString canonical = QFileInfo(scene).canonicalFilePath();
            if (mine && !own.contains(QFileInfo(folder.path()).canonicalFilePath()))
                continue;
            if (!seen.contains(canonical)) {
                seen.insert(canonical);
                into.append(gameItem(folder, scene, mine));
            }
        } else {
            findGames(folder.path(), depth - 1, mine, own, into, seen);
        }
    }
}

QString mediaKindOf(const QString &suffix)
{
    const QString s = suffix.toLower();
    if (VideoSuffixes.contains(s))
        return u"video"_s;
    if (AudioSuffixes.contains(s))
        return u"audio"_s;
    if (PictureSuffixes.contains(s))
        return u"picture"_s;
    return {};
}

QVariantMap mediaItem(const QFileInfo &file, const QString &kind)
{
    static const QHash<QString, std::pair<QString, int>> looks {
        { u"video"_s, { u"film"_s, 30 } },
        { u"audio"_s, { u"music"_s, 350 } },
        { u"picture"_s, { u"image"_s, 155 } },
    };
    const QString subtitle = kind == u"video"_s   ? QCoreApplication::translate("Library", "Video")
                             : kind == u"audio"_s ? QCoreApplication::translate("Library", "Music")
                                                  : QCoreApplication::translate("Library", "Picture");
    return { { u"id"_s, QString(u"media:"_s + file.absoluteFilePath()) },
             { u"kind"_s, u"media"_s },
             { u"source"_s, u"media"_s },
             { u"title"_s, file.completeBaseName() },
             { u"subtitle"_s, subtitle },
             { u"glyph"_s, looks.value(kind).first },
             { u"hue"_s, looks.value(kind).second },
             { u"path"_s, file.absoluteFilePath() },
             { u"mediaKind"_s, kind },
             { u"modified"_s, file.lastModified().toMSecsSinceEpoch() } };
}

void findMedia(const QString &root, QVariantList &into, QSet<QString> &seen)
{
    const QDir base(root);
    if (root.isEmpty() || !base.exists())
        return;
    int found = 0;
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext() && found < MediaPerFolder) {
        const QFileInfo file(it.next());
        // Not too deep, and nothing in a hidden folder, on every system.
        const QString relative = base.relativeFilePath(file.absoluteFilePath());
        if (relative.count(u'/') >= MediaDepth || relative.startsWith(u'.') || relative.contains(u"/."_s))
            continue;
        const QString kind = mediaKindOf(file.suffix());
        if (kind.isEmpty() || seen.contains(file.absoluteFilePath()))
            continue;
        seen.insert(file.absoluteFilePath());
        into.append(mediaItem(file, kind));
        ++found;
    }
}

} // namespace

Library::Library(QObject *parent) : QObject(parent)
{
    loadHistory();
    // The Store's games are its engine's record, so the Library changes as that does. Null but in a build that has it.
    if (store::StoreEngine *engine = store::Storefronts::engine()) {
        for (const auto signal : { &store::StoreEngine::installedChanged, &store::StoreEngine::catalogChanged,
                                   &store::StoreEngine::creatorFeaturesChanged })
            connect(engine, signal, this, &Library::changed);
    }
    connect(&m_watcher, &QFutureWatcher<Found>::finished, this, [this] {
        m_found = m_watcher.result();
        m_read = QDateTime::currentDateTimeUtc();
        emit scanningChanged();
        emit changed();
    });
}

Library::~Library() { m_watcher.waitForFinished(); }

QStringList Library::gameFolders()
{
    const QString asked = qEnvironmentVariable("QOR_GAMES");
    if (!asked.isEmpty())
        return asked.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    return { QDir(QCoreApplication::applicationDirPath()).filePath(u"../share/qor-os/games"_s),
             QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath(u"qor-os/games"_s) };
}

Library::Found Library::scan(const QStringList &gameFolders, const QStringList &mineFolders,
                             const QStringList &mediaFolders)
{
    Found found;
    QSet<QString> seen;
    for (const QString &folder : gameFolders)
        findGames(folder, 1, false, {}, found.games, seen);
    for (const QString &folder : mineFolders)
        findGames(folder, 2, true, ownProjects(), found.games, seen);
    QSet<QString> files;
    for (const QString &folder : mediaFolders)
        findMedia(folder, found.media, files);
    std::stable_sort(found.media.begin(), found.media.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(u"modified"_s).toLongLong() > b.toMap().value(u"modified"_s).toLongLong();
    });
    return found;
}

void Library::refresh(bool force)
{
    if (m_watcher.isRunning())
        return;
    if (!force && m_read.isValid() && m_read.secsTo(QDateTime::currentDateTimeUtc()) < 10)
        return;
    const QStringList games = gameFolders();
    const QStringList mine { QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) };
    const QStringList media { QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
                              QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                              QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) };
    m_watcher.setFuture(QtConcurrent::run([games, mine, media] { return scan(games, mine, media); }));
    emit scanningChanged();
}

void Library::setArqade(bool arqade)
{
    if (m_arqade == arqade)
        return;
    m_arqade = arqade;
    emit changed();
}

QVariantList Library::arqadeGames() const
{
    struct Game
    {
        const char *kind;
        const char *name;
        const char *tag;
    };
    static const Game list[] {
        { "void", "Void Runner", QT_TRANSLATE_NOOP("Library", "Reflex and survival") },
        { "synapse", "Synapse", QT_TRANSLATE_NOOP("Library", "Memory and pattern") },
        { "orbital", "Orbital", QT_TRANSLATE_NOOP("Library", "Precision and timing") },
        { "rift", "Rift Survivor", QT_TRANSLATE_NOOP("Library", "Survival and evolution") },
        { "everrift", "Ever Rift", QT_TRANSLATE_NOOP("Library", "Dungeon and action RPG") },
        { "darkveil", "Darkveil", QT_TRANSLATE_NOOP("Library", "Open world action RPG") },
        { "bellwick", "Bellwick", QT_TRANSLATE_NOOP("Library", "Platformer") },
        { "flux", "Flux Four", QT_TRANSLATE_NOOP("Library", "Arena · strategy") },
        { "reversi", "Rift Reversi", QT_TRANSLATE_NOOP("Library", "Arena · territory") },
    };
    QVariantList games;
    for (const Game &game : list) {
        games.append(
            QVariantMap { { u"id"_s, QString(u"arqade:"_s + QLatin1StringView(game.kind)) },
                          { u"kind"_s, u"game"_s },
                          { u"source"_s, u"arqade"_s },
                          { u"title"_s, QString::fromLatin1(game.name) },
                          { u"subtitle"_s, QString(u"ARQADE · "_s + QCoreApplication::translate("Library", game.tag)) },
                          { u"glyph"_s, u"gamepad-2"_s },
                          { u"hue"_s, 300 },
                          { u"game"_s, QString::fromLatin1(game.kind) } });
    }
    return games;
}

QVariantMap Library::withUse(QVariantMap item) const
{
    const Use use = m_uses.value(item.value(u"id"_s).toString());
    item.insert(u"lastUsed"_s, use.lastUsed);
    item.insert(u"position"_s, use.position);
    item.insert(u"duration"_s, use.duration);
    return item;
}

QVariantList Library::games() const
{
    QVariantList games;
    QVariantList theirs;
    for (const QVariant &game : m_found.games)
        (game.toMap().value(u"source"_s) == u"mine"_s ? theirs : games).append(withUse(game.toMap()));
    if (m_arqade)
        for (const QVariant &game : arqadeGames())
            games.append(withUse(game.toMap()));
    for (const QVariant &game : storeGames())
        games.append(withUse(game.toMap()));
    return games + theirs;
}

QVariantList Library::storeGames() const
{
    QVariantList games;
    const store::StoreEngine *engine = store::Storefronts::engine();
    if (!engine)
        return games;
    for (const store::StoreEngine::Installed &installed : engine->installed()) {
        if (installed.kind != u"game"_s)
            continue;
        // Only what a person may see now. What the catalogue no longer lists, or only the Creator Store does with
        // creator features off, stays installed, and is the Store's to show.
        const store::CatalogItem *listed = engine->catalog().item(installed.id);
        if (!listed
            || !(listed->isPlacedIn(store::View::Player)
                 || (engine->creatorFeatures() && listed->isPlacedIn(store::View::Creator))))
            continue;
        const QString title = installed.name.simplified().left(LongestTitle);
        games.append(QVariantMap {
            { u"id"_s, QString(u"store:"_s + installed.id) },
            { u"kind"_s, u"game"_s },
            { u"source"_s, u"store"_s },
            { u"title"_s, title.isEmpty() ? installed.id : title },
            { u"subtitle"_s, QCoreApplication::translate("Library", "From the Store · %1").arg(installed.version) },
            { u"glyph"_s, u"shopping-bag"_s },
            { u"hue"_s, hueOf(installed.id) },
            { u"storeId"_s, installed.id },
            { u"path"_s, QString() },
            { u"modified"_s, qint64(0) },
        });
    }
    return games;
}

QVariantMap Library::playable(const QString &id) const
{
    const auto refused = [](const QString &message) {
        return QVariantMap { { u"ok"_s, false }, { u"path"_s, QString() }, { u"message"_s, message } };
    };
    const store::StoreEngine *engine = store::Storefronts::engine();
    QVariantMap listed;
    for (const QVariant &game : storeGames())
        if (game.toMap().value(u"id"_s) == id)
            listed = game.toMap();
    if (!engine || listed.isEmpty())
        return refused(QCoreApplication::translate("Library", "That game is not in the Library."));
    const QString storeId = listed.value(u"storeId"_s).toString();
    const QString title = listed.value(u"title"_s).toString();
    // Whole, every file as its developer signed it and nothing beside them, each time it is played: anything running
    // as this person could have written there since it was installed.
    if (!engine->isIntact(storeId))
        return refused(QCoreApplication::translate("Library", "%1 is not as its developer signed it, so it will not "
                                                              "play. Remove it in the Store and get it again.")
                           .arg(title));
    // The scene its own game.json names, held to what a QQ Studio build writes. Nothing is guessed from its files.
    const QVariantMap built = builtGameItem(QDir(engine->installDirectory(storeId)));
    if (built.isEmpty())
        return refused(
            QCoreApplication::translate("Library", "%1 does not say which scene it starts in, so it will not play.")
                .arg(title));
    return { { u"ok"_s, true }, { u"path"_s, built.value(u"path"_s) }, { u"message"_s, QString() } };
}

QVariantList Library::media() const
{
    QVariantList media;
    for (const QVariant &item : m_found.media)
        media.append(withUse(item.toMap()));
    return media;
}

QVariantList Library::recent() const
{
    QVariantList used;
    for (const QVariantList &list : { games(), media() })
        for (const QVariant &item : list)
            if (item.toMap().value(u"lastUsed"_s).toLongLong() > 0)
                used.append(item);
    std::stable_sort(used.begin(), used.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(u"lastUsed"_s).toLongLong() > b.toMap().value(u"lastUsed"_s).toLongLong();
    });
    return used.mid(0, RecentCount);
}

QVariantMap Library::item(const QString &id) const
{
    for (const QVariantList &list : { games(), media() })
        for (const QVariant &item : list)
            if (item.toMap().value(u"id"_s) == id)
                return item.toMap();
    return {};
}

void Library::used(const QString &id)
{
    if (id.isEmpty())
        return;
    m_uses[id].lastUsed = QDateTime::currentMSecsSinceEpoch();
    saveHistory();
    emit changed();
}

void Library::reported(const QVariantMap &report)
{
    const QString type = report.value(u"type"_s).toString();
    const QString path = report.value(u"path"_s).toString();
    if (path.isEmpty() || path.size() > 4096)
        return;
    const QString id = u"media:"_s + path;
    if (type == u"position"_s) {
        const double position = report.value(u"position"_s).toDouble();
        if (!std::isfinite(position) || position < 0)
            return;
        Use &use = m_uses[id];
        use.position = position;
        use.lastUsed = QDateTime::currentMSecsSinceEpoch();
    } else if (type == u"learned"_s) {
        const double duration = report.value(u"duration"_s).toDouble();
        if (!std::isfinite(duration) || duration <= 0)
            return;
        m_uses[id].duration = duration;
    } else {
        return;
    }
    saveHistory();
    emit changed();
}

QString Library::historyFile() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(u"library-history.json"_s);
}

void Library::loadHistory()
{
    QFile file(historyFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject all = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        m_uses.insert(it.key(), { entry.value(u"lastUsed"_s).toInteger(), entry.value(u"position"_s).toDouble(),
                                  entry.value(u"duration"_s).toDouble() });
    }
}

void Library::saveHistory() const
{
    // The most recent only.
    QList<std::pair<QString, Use>> uses;
    for (auto it = m_uses.cbegin(); it != m_uses.cend(); ++it)
        uses.append({ it.key(), it.value() });
    std::sort(uses.begin(), uses.end(),
              [](const auto &a, const auto &b) { return a.second.lastUsed > b.second.lastUsed; });
    QJsonObject all;
    for (const auto &[id, use] : uses.mid(0, HistoryCount))
        all.insert(id, QJsonObject { { u"lastUsed"_s, use.lastUsed },
                                     { u"position"_s, use.position },
                                     { u"duration"_s, use.duration } });
    const QString path = historyFile();
    QDir().mkpath(QFileInfo(path).path());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(all).toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace qor
