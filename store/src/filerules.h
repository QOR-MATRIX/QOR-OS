// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QString>

namespace qor::store {

// What may be in a package of a format 2 kind. Returns why not, or an empty string. Files are known by an allowlist of
// their extension and what they start with, so anything not named here is refused.
//
// An asset-pack, and every kind without rules of its own yet, holds only passive files, since a QML file copied into a
// project's logic/ runs: models (glb, gltf and a bin that a gltf names), pictures, sound, and .txt for licences. No
// QML, JavaScript, shader, SVG, native code or script. A later stage that takes a kind writes down what it may hold.
//
// `game` is not passive: a QQ game's scenes and logic are QML that QQ's runtime runs. It may hold those too (qml, mjs,
// js) and the JSON its levels are written in. It holds no native code, no script for the system to start, and no
// qmldir, which could name a plugin.
//
// A .gltf or .glb must name only files beside it in the package, or carry them as data: no climbing, no scheme, no
// file that is not there. A .txt must be UTF-8 text with no markup, since a Qt Text guesses at rich text and rich text
// can fetch a picture. (That Text sets textFormat: Text.PlainText too: this is the second wall, not the first.)
QString refusePackageFiles(const QString &kind, const QMap<QString, QByteArray> &files);

// Whether a preview's name and bytes are a picture of a kind previews may be.
bool isPreviewName(const QString &path);
QString refusePreviewFile(const QString &path, const QByteArray &data);

} // namespace qor::store
