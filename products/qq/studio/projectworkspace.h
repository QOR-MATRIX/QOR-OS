// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// ProjectWorkspace: what the Studio remembers of each project a person works in, so it comes back as it was left: which
// files were open, which panels, the camera and the window. And the short list of recent projects. And a recovery copy of
// what is not saved yet, so that a crash, or a launcher closing the Studio, loses none of it.
//
// It is the person's, not the project's. Projects get shared and published, and a stranger's is hostile, so nothing of
// this goes in a project's folder: each project has <AppDataLocation>/workspaces/<SHA-256 of its real path>.json, written
// whole or not at all.
//
// Everything read back is treated as a stranger's, even from there. A file that is too large, not JSON, of a version this
// Studio does not know, or written for another folder is not used at all. A path in it that is absolute, climbs out, or
// leads out through a link is dropped, and so is a window that would not fit on any screen, which is brought back
// onto one. What goes unused is said in `problem`, for the person.
//
// Whether a project may run here at all is not decided here: SceneIO's trust check (QOR-0018) is, and a remembered file
// is only ever opened through it. This class holds no scene and runs nothing, and knows no engine: `scene` and `logic` are
// two remembered paths to it, and what they mean is the Studio's.
//
// A recovery copy is <AppDataLocation>/workspaces/<SHA-256 of the project's real path>.recovery.json, beside its workspace and
// never in the project: the unsaved scene's text and name and the unsaved logic file's text and file, written whole or not at
// all, for this person's eyes, by the Studio after a short pause in editing, and gone once the work is saved or discarded. It
// is read back as a stranger's too: too large, not JSON, of another version, written for another folder, or naming a file that
// is not plainly in the project, and that much of it goes unused and is said. Scene text runs when it is loaded, so this
// only hands it over, and the Studio shows it only past the trust check and a person's Restore. Each part says whether the
// saved file has changed since the copy was made, for the Studio's question.

#pragma once

#include <QList>
#include <QObject>
#include <QRect>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace qq {

class ProjectWorkspace : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    /// The format of what is kept. A file of any other is not read.
    static constexpr int schemaVersion = 1;
    /// The largest file read back: far more than a real one needs.
    static constexpr qint64 largestFile = 64 * 1024;
    /// How many recent projects are kept.
    static constexpr int mostRecent = 12;
    /// The largest recovery copy kept or read back: far more than a scene or a logic file a person has typed.
    static constexpr qint64 largestRecovery = 16 * 1024 * 1024;
    /// The smallest window restored, so a remembered sliver can always be got hold of.
    static constexpr int smallestWidth = 640;
    static constexpr int smallestHeight = 400;

    explicit ProjectWorkspace(QObject *parent = nullptr);

    /// What is kept for `project`, each part checked: { "found": whether a usable file is there, "problem": what a person
    /// should be told, or "", "scene" and "logic": paths relative to the project of files that are in it, or "", "panels":
    /// { name: bool }, "camera": { "yaw", "pitch", "distance", "x", "y", "z" }, or nothing if any of it has no sense to it,
    /// "window": { "maximized", "width", "height", "x", "y" }, as much of it as is usable, fitted to the screens there are }.
    Q_INVOKABLE QVariantMap load(const QUrl &project) const;
    /// Keep `state` for `project`, in the form load() gives it: true if kept. A path that load() would refuse is not.
    Q_INVOKABLE bool save(const QUrl &project, const QVariantMap &state);

    /// Keep what is unsaved in `project`, whole or not at all: `work` is { "scene": { "name", "file": relative to the project,
    /// or "" for a scene never saved, "text" }, "logic": { "file", "text" } }, either or both, and with neither there is nothing
    /// to keep and the copy goes. False if it could not be kept.
    Q_INVOKABLE bool keepRecovery(const QUrl &project, const QVariantMap &work);
    /// What was kept for `project`, each part checked: { "found": whether any of it is to be offered, "stale": whether it is
    /// there and none of it is unsaved work any more, so it can go, "problem": what a person should be told, or "", "scene" and
    /// "logic": { "name", "file", "text", "changed": whether the saved file differs from when the copy was made, "gone": whether
    /// it has gone }, each only if it is usable and differs from the file }.
    Q_INVOKABLE QVariantMap loadRecovery(const QUrl &project) const;
    /// The work has been saved or discarded: no copy of it stays.
    Q_INVOKABLE void dropRecovery(const QUrl &project);

    /// Whether `folder` is a folder on this computer that is there.
    Q_INVOKABLE bool isFolder(const QUrl &folder) const;
    /// Whether two URLs are the same folder, wherever links lead.
    Q_INVOKABLE bool sameProject(const QUrl &a, const QUrl &b) const;
    /// `file`'s path relative to `project`, if it is a file there, really inside it, and a path load() would accept; or "".
    Q_INVOKABLE QString relativeInside(const QUrl &project, const QUrl &file) const;

    /// The recent projects, latest first: each { "path", "url", "name", "missing": whether its folder has gone }.
    Q_INVOKABLE QVariantList recentProjects() const;
    /// `project` is the latest.
    Q_INVOKABLE void noteRecent(const QUrl &project);

    /// Where everything is kept.
    static QString storeFolder();
    /// A project's file there, by the project's real path.
    static QString fileFor(const QString &realProject);
    /// A project's recovery copy there, by the project's real path.
    static QString recoveryFileFor(const QString &realProject);
    /// The list of recent projects there.
    static QString recentFile();
    /// Whether a path could be one the Studio wrote: relative, each part a plain name, nothing that climbs or that a URL or
    /// a file system would read as anything else.
    static bool plainRelative(const QString &relative);
    /// `relative`'s real path, if it is plain, a regular file, and really inside the project at real path `realProject`;
    /// or "". A link that leaves, and a pipe that would hang whoever reads it, are neither.
    static QString fileInside(const QString &realProject, const QString &relative);
    /// A remembered window, made to fit: only numbers count, a size with no sense to it is dropped, and what would be off
    /// every screen, or larger than the one it is on, is brought onto it.
    static QVariantMap fitWindow(const QVariantMap &stored, const QList<QRect> &screens);
    /// A remembered camera, or nothing: only finite numbers of a sensible size count, and any that does not loses all of it. Its
    /// tilt and distance are brought within what the Studio's own handles can reach.
    static QVariantMap fitCamera(const QVariantMap &stored);
};

}  // namespace qq
