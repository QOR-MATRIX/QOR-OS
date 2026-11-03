// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// AssetLibrary: QQ Studio's side of the Creator Store (QOR-0021, decisions 5 and 6). The Store installs asset packs; the Studio
// only reads what is installed, lets a person look at each pack and its files, copies what they choose into the project, and
// keeps a lock file of what came from where, which is the permission an import does not have.
//
// Where packs come from. Prism names the Store's folder of installed items when it starts the Studio (QOR_ASSET_PACKS, set by
// ProductHost only where the Store's format 2 engine exists, which is a demonstration build), and --asset-packs does the same.
// With neither, `folder` is empty, there is no library, and nothing here runs.
//
// That folder is a stranger's. Its record (installed.json) and every item's folder are read as hostile input: size-capped,
// opened a name at a time with no link followed, only `asset-pack` kinds taken, and a pack counts only if its folder holds
// exactly what its manifest lists, nothing more, every file matching its SHA-256 and of a kind a pack may hold (pictures,
// skies, models, sounds, a model's buffers and plain text), each model naming only files beside it in the pack. Names, developers
// and paths are NFC with no control or format characters (no right-to-left override, no zero-width joiner). This is checked
// here, and not taken from the Store's earlier check: anything running as this person can write there. A pack checked whole is
// remembered by what its files are on disk (device, inode, size, modification and change times, for every file, the manifest and
// its signature, on Linux); while none of that changes it is listed again without being read again, and what is copied, or shown as a
// thumbnail, is read again and its digest checked again first. The Studio holds no developer's key, so it cannot check the
// developer's signature, and says the developer is not checked here. The manifest's digest and its signature go into the lock,
// so that can be checked later.
//
// The lock is <project>/assets.lock.json, one per project, written whole or not at all, in one canonical layout:
//
//   { "format": 1, "change": a tag of its own for each change, "packs": [ { "id", "name", "version", "developer", "licence": SPDX
//     id, "provenance": { "manifest": SHA-256 of the manifest, "signature": the developer's signature over it, in base64, and
//     "origin", "author", "source", "permission" where the Store recorded them }, "whole": whether the whole pack was used,
//     "files": [ { "path": where it is in the project, "source": where it is in the pack, "sha256": what it was when it came,
//     "version": the pack's version it came from, and "beside": true for a new version put beside a changed copy } ] } ] }
//
// A copy goes to assets/<pack id>/<its path in the pack>, with the pack's licence file and its NOTICE, whatever else is
// chosen. The lock is a project's, so it is read as a stranger's on every open: too large, not JSON, of another format, a path
// that is not plainly under assets/<its pack>/, that climbs or goes through a link, or anything malformed, and the whole of it
// is not used: the project opens, the library says why, and nothing it names is changed. With no lock there is nothing to say.
//
// Changing the project is a transaction: everything new is written beside the project's files first, in a folder named with a
// dot that a build leaves behind (.qq-assets-<tag>), and a journal in the person's own store (never the project's, which a
// stranger could plant) says what is to move, the SHA-256 of each file it places, and which folder it is for (its device and
// inode on POSIX, its creation time on Windows). The new lock is checked as readLock() would read it before anything moves.
// Then what is replaced or removed is set aside, each looked at again just before and left alone, with nothing changed, if it is
// no longer what was planned; what is new is moved into place; and the lock, which carries the change's own tag so no two are
// alike, is written: that write is the moment it happens. The next time this person's Studio looks at the project, a change
// whose lock was written is finished, and one whose lock was not is put back. Putting back moves back only a placed file still
// holding what was placed, and a set-aside file only where nothing now is; anything it cannot account for is left where it is,
// and what was set aside and could not go back stays in the staging folder, said so. A journal for another folder in that
// place, or with its staging folder gone, moves nothing and is dropped, said so. It can itself be cut short and run again. On
// Linux each step is made durable before the next by one syncfs() of the project's file system; elsewhere on POSIX by fsync()
// of each file and folder. One change to a project's assets, or one build of it, runs at a time, by a lock in the person's store
// (Hold): that excludes Studios that share this person's store, and not another person, a Studio with another store, another
// computer on a shared folder, or any other program writing there.
//
// Updating never writes over a copy the person has changed (its digest differs from the lock's): those are kept and listed, and
// the new version of each is put beside it, or not, as the person chooses. A copy the person deleted is not brought back. Every
// other copy is replaced, a file the new version no longer has is removed if unchanged, and new files come too if the whole
// pack was used. A beside copy is the person's from then on, and no later update replaces or removes it. Saying no changes
// nothing.

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickImageProvider>

#include <functional>
#include <memory>

class QLockFile;

namespace qq {

class AssetLibrary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /// The Store's folder of installed items, or "" for no library.
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(bool available READ available NOTIFY folderChanged)

public:
    /// The format of assets.lock.json. A lock of any other is not used.
    static constexpr int lockFormat = 1;
    static constexpr const char *lockName = "assets.lock.json";
    /// The largest record of what is installed, manifest and lock read: far more than a real one needs.
    static constexpr qint64 largestRecord = 1024 * 1024;
    static constexpr qint64 largestManifest = 1024 * 1024;
    static constexpr qint64 largestLock = 8 * 1024 * 1024;
    /// What one pack may hold altogether, as the Store bounds a package's content, and how many files.
    static constexpr qint64 largestPack = 32 * 1024 * 1024;
    static constexpr int mostFilesInAPack = 8192;
    /// How many packs are looked at, and how many files a lock may name.
    static constexpr int mostPacks = 256;
    static constexpr int mostLockedFiles = 32768;

    struct PackFile {
        QString path;       ///< in the pack
        QByteArray sha256;  ///< raw
        qint64 size = 0;
        QString kind;       ///< "picture", "sky", "model", "sound", "buffer" or "text"
    };
    struct Pack {
        QString id, name, version, developer, licence, licenceFile;
        QString folder;  ///< its real path
        QByteArray manifestSha256, signature;
        QVariantMap provenance;  ///< what the Store recorded, if anything: origin, author, source, permission
        QList<PackFile> files;
        QHash<QString, qsizetype> index;  ///< path → its place in `files`
        const PackFile *file(const QString &path) const;
    };
    struct LockedFile {
        QString path, source, version;
        QByteArray sha256;  ///< raw
        bool beside = false;  ///< a new version put beside a changed copy: the person's from then on
    };
    struct LockedPack {
        QString id, name, version, developer, licence;
        QVariantMap provenance;
        bool whole = false;
        QList<LockedFile> files;
    };
    struct Lock {
        bool found = false;
        QString problem;  ///< "" if it can be used
        QString change;   ///< the tag of the change that wrote it
        QList<LockedPack> packs;
        const LockedPack *pack(const QString &id) const;
    };

    /// For a test: called as a change reaches each of its steps: "staged", once everything new is written beside the project
    /// and the journal says so; "aside", once what it replaces or removes is set aside; "placed", once what is new is in
    /// place; and "locked", once the lock is written. Nothing but a test sets this.
    using Steps = std::function<void(const char *)>;

    /// The one change to a project's assets, or build of it, at a time: a lock in the person's store, held while it lives.
    class Hold
    {
    public:
        explicit Hold(const QString &realProject);
        ~Hold();
        Hold(const Hold &) = delete;
        Hold &operator=(const Hold &) = delete;
        bool held() const { return m_lock != nullptr; }
        /// Why it is not held, for a person: another Studio has it, or the person's store cannot be written.
        QString problem() const { return m_problem; }

    private:
        std::unique_ptr<QLockFile> m_lock;
        QString m_problem;
    };

    explicit AssetLibrary(QObject *parent = nullptr);

    QString folder() const { return m_folder; }
    void setFolder(const QString &folder);
    bool available() const { return !m_folder.isEmpty(); }

    /// What the library shows for `project`: { "available", "packs": [ { "id", "name", "version", "developer", "licence",
    /// "provenance": a line for a person, "files": [ { "path", "kind", "size", "url", "inProject": its path there, or "" } ],
    /// "used": the version in the lock, or "", "update": whether the installed version is newer than it } ], "missing": [ what
    /// the lock names that is not installed, each { "id", "name", "version", "files" } ], "refused": [ { "id", "problem" } ],
    /// "lockProblem": "" or why the lock is not used, "recovered": "" or what was put right of a change cut short }.
    Q_INVOKABLE QVariantMap state(const QUrl &project);
    /// Copy `files` of pack `id` (paths in the pack; none for all of it) into the project. A file already there that differs is
    /// not replaced unless `replace` is true: without it the answer lists it in "conflicts" and changes nothing. { "ok",
    /// "problem", "conflicts": [project paths], "copied": [project paths] }.
    Q_INVOKABLE QVariantMap use(const QUrl &project, const QString &id, const QStringList &files, bool replace);
    /// What updating pack `id` would do, changing nothing: { "ok", "problem", "from", "to", "replaced", "added", "removed",
    /// "same": project paths, "changed": [ { "path", "beside": where the new version would go, or "" if it has none },
    /// "gone": copies the person deleted, which are not brought back }.
    Q_INVOKABLE QVariantMap plan(const QUrl &project, const QString &id);
    /// Update pack `id`, putting the new version beside each changed copy in `beside` (project paths) and skipping the others.
    /// { "ok", "problem", and what plan() says was done }.
    Q_INVOKABLE QVariantMap update(const QUrl &project, const QString &id, const QStringList &beside);

    // The same, for main.cpp's command line and the tests, with `packs` the folder of installed items.
    static QVariantMap usePack(const QString &project, const QString &packs, const QString &id, const QStringList &files,
                               bool replace, const Steps &steps = {});
    static QVariantMap planUpdate(const QString &project, const QString &packs, const QString &id);
    static QVariantMap updatePack(const QString &project, const QString &packs, const QString &id, const QStringList &beside,
                                  const Steps &steps = {});

    /// Every usable asset pack installed in `packs`, by id; what was refused, as "id: reason", in `refused`.
    static QList<Pack> readInstalled(const QString &packs, QStringList *refused = nullptr);
    /// The lock of the project at real path `realProject`, read as a stranger's.
    static Lock readLock(const QString &realProject);
    /// The same of a lock's bytes, as if they were in the project: what a new lock is checked by before it is written.
    static Lock parseLock(const QByteArray &bytes, const QString &realProject);
    /// A lock's canonical bytes, carrying `change`, the tag of the change that wrote it.
    static QByteArray lockBytes(const QList<LockedPack> &packs, const QString &change = {});
    /// A change to the project at `realProject` that this person's Studio left cut short, put right: finished if its lock was
    /// written, put back if not. "" if there was none, or what was done or could not be.
    static QString recover(const QString &realProject);
    /// Where that change's journal is kept: the person's, never the project's.
    static QString journalFor(const QString &realProject);
    /// The kind of a file a pack may hold, by its name, or "" for one it may not.
    static QString kindOf(const QString &path);

signals:
    void folderChanged();

private:
    QString m_folder;
};

/// Thumbnails of the pictures in the packs the library lists (image://qqassets/<pack id>/<version>/<path>), from the very bytes
/// whose digest was checked as they are read, decoded only as the format their name says (PNG, JPEG or WebP), and only once their
/// header says they are at most 4096 pixels a side, as the Store's previews are. Anything else is no picture.
class AssetThumbnails : public QQuickImageProvider
{
public:
    static constexpr int largestSide = 4096;
    AssetThumbnails();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    /// The folder the library reads, for every provider: what AssetLibrary::setFolder() says.
    static void setFolder(const QString &folder);
};

}  // namespace qq
