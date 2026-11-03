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
// skies, models, sounds, a model's buffers and plain text), each model naming only files beside it in the pack. This is checked
// here, every time, and not taken from the Store's earlier check: anything running as this person can write there. The Studio
// holds no developer's key, so it cannot check the developer's signature, and says nothing of who made a pack beyond what the
// manifest claims. The manifest's digest and its signature go into the lock, so that can be checked later.
//
// The lock is <project>/assets.lock.json, one per project, written whole or not at all, in one canonical layout:
//
//   { "format": 1, "packs": [ { "id", "name", "version", "developer", "licence": SPDX id, "provenance": { "manifest": SHA-256 of
//     the manifest, "signature": the developer's signature over it, in base64, and "origin", "author", "source", "permission"
//     where the Store recorded them }, "whole": whether the whole pack was used, "files": [ { "path": where it is in the
//     project, "source": where it is in the pack, "sha256": what it was when it came, "version": the pack's version it came
//     from } ] } ] }
//
// A copy goes to assets/<pack id>/<its path in the pack>, with the pack's licence file and its NOTICE, whatever else is
// chosen. The lock is a project's, so it is read as a stranger's on every open: too large, not JSON, of another format, a path
// that is not plainly under assets/<its pack>/, that climbs or goes through a link, or anything malformed, and the whole of it
// is not used: the project opens, the library says why, and nothing it names is changed. With no lock there is nothing to say.
//
// Changing the project is a transaction: everything new is written beside the project's files first, in a folder named with a
// dot that a build leaves behind (.qq-assets-<tag>), and a journal in the person's own store (never the project's, which a
// stranger could plant) says what is to move. Then what is replaced or removed is set aside, what is new is moved into place,
// and the lock is written: that write is the moment it happens. A crash before it puts everything back, and after it finishes,
// the next time this person's Studio looks at the project, from the journal and what is on disk: whether the lock is the one
// the journal promised. Putting back moves each file back where it was found, never deleting what it cannot account for, so
// it can itself be cut short and run again. On Linux each step is made durable before the next. One change to a project's
// assets runs at a time, by a lock in the person's store.
//
// Updating never writes over a copy the person has changed (its digest differs from the lock's): those are kept and listed, and
// the new version of each is put beside it, or not, as the person chooses. A copy the person deleted is not brought back. Every
// other copy is replaced, a file the new version no longer has is removed if unchanged, and new files come too if the whole
// pack was used. Saying no changes nothing.

#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

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
        const PackFile *file(const QString &path) const;
    };
    struct LockedFile {
        QString path, source, version;
        QByteArray sha256;  ///< raw
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
        QList<LockedPack> packs;
        const LockedPack *pack(const QString &id) const;
    };

    /// For a test: called as a change reaches each of its steps: "staged", once everything new is written beside the project
    /// and the journal says so; "aside", once what it replaces or removes is set aside; "placed", once what is new is in
    /// place; and "locked", once the lock is written. Nothing but a test sets this.
    using Steps = std::function<void(const char *)>;

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
    /// A lock's canonical bytes.
    static QByteArray lockBytes(const QList<LockedPack> &packs);
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

}  // namespace qq
