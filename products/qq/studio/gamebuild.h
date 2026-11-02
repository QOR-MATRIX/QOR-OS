// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// GameBuild: makes a game of a project. Play in the Studio runs the project as it stands, with an editor's reach. A built
// game is a copy of it, kept apart in the folder the Library reads, which the QQ Player plays locked down and which nothing
// done to the project afterwards can touch. A person's edits reach it only by building again.
//
// The copy holds what a game can use: the project's scenes, logic, prefabs and assets, and game.json, which says what it
// is, which scene it starts in and every file's SHA-256. Nothing else of the project goes: not its history, not what lies
// beside those folders, and no file whose name starts with a dot. There is no program in it: the Player that QOR OS has
// plays it, so `engine` says which QQ it was built for and `platform` where.
//
// A build that cannot be whole is refused, and whatever was built before stays as it was. A link is refused rather than
// followed, wherever it leads, and so is anything that is neither a file nor a folder, unopened, since reading a pipe hangs.
// The copy is made beside the game it replaces, in a folder the Library does not list, and moved into place when complete.
//
// Each project has one game, named by its real path as a game's store is (store.h), so two projects of one name are two
// games, and building again keeps what a player has saved. The game's store is not the project's: it is named for the copy.

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
    /// first scene there, "build": how many times this game has been built, "files", "bytes", "leftOut": what was passed
    /// over for its name }.
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
