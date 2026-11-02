// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include "catalog.h"

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QString>

namespace qor::store {

// What a package may unpack to, its manifest and signature included, and what its archive may be. The whole package is
// held in memory while it is checked, so this is a bound on that memory too.
inline constexpr qint64 kMaximumPackageContent = 32 * 1024 * 1024;
inline constexpr qint64 kMaximumPackageArchive = kMaximumPackageContent + 4 * 1024 * 1024;

// A QOR OS package (.qorpkg): a zip archive holding
//   manifest.json  id, kind, version, developer and the SHA-256 of every file
//   manifest.sig   the developer's ECDSA P-256 signature over manifest.json
//   the files the manifest lists, and nothing else.
//
// A format 1 manifest is a theme's or a widget's, and those packages are never signed again. A format 2 manifest is for
// the kinds of a format 2 catalogue, and mirrors what identifies them there and what they claim to work with (the
// stores it is placed in, its edition, licence, dependencies, platforms and engines), so the developer's signature
// covers them as well as the store's, and a store cannot list a package for a system its developer never said it runs
// on. A placement's category is the store's to change, and is not mirrored.
struct PackageManifest
{
    int format = 1;
    QString id;
    QString kind;
    QString version;
    QString name;
    QString developer;
    QMap<QString, QByteArray> files; // path → raw SHA-256

    // Format 2 only.
    QStringList stores; // sorted
    Edition edition;
    Licence licence;
    QList<VersionRange> dependencies;
    QStringList platforms;
    QList<VersionRange> engines;
};

struct PackageResult
{
    bool ok = false;
    QString error;
    PackageManifest manifest;
    QMap<QString, QByteArray> contents; // path → bytes, all verified
    QByteArray manifestBytes;           // the manifest and its signature, kept with what is installed
    QByteArray signature;
    qint64 unpacked = 0; // every byte the archive held, measured: the contents, the manifest and its signature
};

// Opens a package and verifies it completely: the archive's shape (readStoredArchive), the developer's signature over
// the manifest, every file against the manifest, then what a package of its kind may hold (refusePackageFiles). Paths
// must be plain relative names; symbolic links, unlisted or missing files, and more than `maximumSize` bytes unpacked
// are refused.
PackageResult openPackage(const QByteArray &archive, const QByteArray &developerKey,
                          qint64 maximumSize = kMaximumPackageContent);

// Whether a package's files, installed in `folder`, are still exactly what its developer signed: the signature kept
// beside them over the manifest, every listed file against it, and no other file. An install is verified once; what
// runs is verified each time it is loaded, since anything running as this user can write there. On success, `manifest`
// says which package it is, as its developer signed it: a signature alone does not say it is the version installed.
// Anything else in the folder fails it: a file, a folder, even an empty one, or a link of any kind.
bool verifyInstalled(const QString &folder, const QByteArray &developerKey, PackageManifest *manifest = nullptr);

// Whether a path is a plain relative one, safe to write under a folder on every system a package may reach: no
// climbing, no drive or scheme, no backslash, no name Windows keeps for a device (CON, NUL, COM1...), with or without
// an extension, and no name ending in a dot, which Windows would drop.
bool isSafePackagePath(const QString &path);

} // namespace qor::store
