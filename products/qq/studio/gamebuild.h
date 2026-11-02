// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// GameBuild: makes a game of a project. Play in the Studio runs the project as it stands, with an editor's reach. A built
// game is a copy of it, kept apart in the folder the Library reads, which the QQ Player plays locked down and which nothing
// done to the project afterwards can touch. A person's edits reach it only by building again.
//
// The copy holds what a game can use: the project's scenes, logic, prefabs and assets, and game.json, which says what it
// is, which scene it starts in and every file's SHA-256. Nothing else of the project goes: not its history, not what lies
// beside those folders, and no file whose name starts with a dot. No native program goes in it: its scenes and logic are
// code, QML and JavaScript, which the Player that QOR OS has runs behind its walls, so `engine` says which QQ it was built
// for and `platform` where. Nothing checks those digests when a game is played.
//
// What refuses a build, with the file and the reason, leaving whatever was built before as it was:
//
//   a link, followed by nothing, wherever it leads; anything that is neither a file nor a folder, unopened, since reading a
//     pipe hangs; a name a system would read as more; a folder or a file that cannot be read; a first scene named with a dot
//   compiled QML (.qmlc, .jsc, .mjsc), wherever it is. Qt would map it straight into the Player as code, past every check
//     on source, so the Player keeps Qt's disk cache off as well
//   what a file names that the game would not hold. In a .qml, .mjs or .js: an import of anything not in the game, and a
//     string that starts ../ or climbs and leads out of the game or to what it does not hold, or that is a full path or a file:
//     URL. In a .gltf or a .glb: every uri that is not data:. These are linear scans, of the copy and not the project, no QML
//     engine involved, of at most 16 MB of code and 1 GB of model. So a string in logic that only looks like a path can be
//     taken for one, and the reason says so; and a path a game puts together as it runs is not seen. That is what the
//     Player's walls are for: this is about a game being whole, and they are about keeping it in
//   more than 8 GB or 50,000 files
//
// A build compiles nothing here: the Studio asks SceneIO::brokenCode before it starts one, and `qq-studio --build` says in its
// answer that nothing was compiled.
//
// The copy is made beside the game it replaces, in a folder the Library does not list, and moved into place when complete.
// One build of a game runs at a time, by a QLockFile in the games folder held for all of make(): a second is refused in plain
// words, and a lock left by a build that died goes by QLockFile's own check of whether its holder is alive. Dying between the
// two renames leaves the old game aside: the next build of it, under that lock, puts it back before anything else. A power
// cut can still leave short files behind a whole game.json: only the manifest is synced.
//
// On POSIX each file is opened a name at a time from the project's folder, following no link at any step and waiting on no
// pipe, then looked at through its descriptor, so a file or folder swapped after the walk is refused, and not followed. On
// Windows it is looked at once more just before it is opened, which leaves a moment in which a swap would pass, and its
// folders are not looked at again. A process of the person's own, or a network share, is what could do that.
//
// Each project folder has one game, named by its real path as a game's store is (store.h), so two projects of one name are two
// games, and building again keeps what a player has saved. Moved or renamed, the folder builds a second game, and the first
// stops changing and stays in the Library. The game's store is not the project's: it is named for the copy.

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace qq {

class GameBuild : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    /// The format of game.json. The Library reads no other.
    static constexpr int manifestFormat = 1;

    struct Limits {
        qint64 bytes = 8LL * 1024 * 1024 * 1024;
        int files = 50000;
        /// For a test: called as a build reaches each of its steps, on the build's thread: "walked", once it knows its files;
        /// "copied", once they and the manifest are in the staging folder; "moved-aside", between the two renames; and
        /// "in-place", before the old game is deleted. Nothing but a test sets this: no command line or setting reaches it.
        std::function<void(const char *)> step;
    };

    explicit GameBuild(QObject *parent = nullptr);
    ~GameBuild() override;

    bool busy() const { return m_thread != nullptr; }

    /// Build in the background; finished() follows, with what make() answers. False, and nothing starts, if one is under way.
    Q_INVOKABLE bool start(const QUrl &project, const QUrl &scene);

    /// The whole of a build, on this thread. `scene` is a file of the project, by its full path or relative to the project.
    /// Answers { "ok", "problem": what a person should be told, or "", "title", "folder": where the game is, "scene": its
    /// first scene there, "build": how many times this game has been built, "files", "bytes", "leftOut": the first of what
    /// was passed over for its name, and "leftOutCount": how many there were }.
    static QVariantMap make(const QString &project, const QString &scene, const QString &games, const Limits &limits);
    static QVariantMap make(const QString &project, const QString &scene, const QString &games)
    {
        return make(project, scene, games, Limits{});
    }

    /// The folder this person's games are built into, which the Library reads (prism/shell/library.cpp).
    static QString gamesFolder();
    /// The game's folder name for a project at this real path.
    static QString folderNameFor(const QString &realProject);

    /// What start() builds with, for a test.
    void setLimits(const Limits &limits) { m_limits = limits; }

signals:
    void busyChanged();
    void finished(const QVariantMap &result);

private:
    QPointer<QThread> m_thread;
    QVariantMap m_result;
    Limits m_limits;
};

}  // namespace qq
