// A prefab placed in a scene (DIRECTION P3.15): a group kept in a file of its own, prefabs/<name>.qml in the project,
// whose root is a Group, made here as often as it is placed. The scene's file keeps only where each stands and which
// file it is; its parts come from that file, so a change to the prefab reaches every one of them the next time the scene
// is opened or played. Logic places one while the world plays as it spawns anything:
//
//   spawn("Prefab", { name: "Coin 7", source: "../prefabs/coin.qml", position: Qt.vector3d(4, 1, 0) })
//
// and finds a part of it by name with `part(name)`. Its parts play (bodies, triggers, sounds, logic) but are not the
// scene's own entities: the Studio shows the prefab, not them, and a Behaviour inside finds its target among them first.

// Changed for QOR OS: state reads preserve the mixed parent types through their structural interface.

import QtQuick
import QtQuick3D
import QQ

Node {
    id: prefab

    readonly property string kind: "Prefab"
    readonly property var fields: ["name", "position", "eulerRotation", "scale", "source"]
    readonly property var fileTypes: ({ source: ["QQ prefabs (*.qml)"] })

    property string name: "Prefab"
    /// The prefab's file, kept relative to the scene file.
    property url source

    /// Its parts, made from the file (their root Group), or null; and why not, if the file could not be made.
    property QtObject content: null
    property string error: ""

    // The scene's state, passed on to its parts as a group passes it on.
    readonly property bool playing: parent !== null && _parentValue("playing") === true
    readonly property bool replaying: parent !== null && _parentValue("replaying") === true
    readonly property bool paused: parent !== null && _parentValue("paused") === true
    readonly property real _owed: parent !== null && _parentValue("_owed") !== undefined ? _parentValue("_owed") : 0
    readonly property string view: parent !== null && _parentValue("view") !== undefined ? _parentValue("view") : "3d"
    readonly property real pixelsPerUnit: parent !== null && _parentValue("pixelsPerUnit") !== undefined ? _parentValue("pixelsPerUnit") : 16
    /// True while the scene this belongs to plays.
    readonly property bool live: playing

    // A parent can be a Scene, Group or Prefab; none is the sole parent type.
    function _parentValue(field: string): var { return prefab.parent[field] }

    /// One of its parts, by name, or null.
    function part(name) {
        return SceneIO.everyEntity(prefab).find(e => e.name === name) ?? null
    }

    function _build() {
        if (content) {
            SceneIO.remove(content)
            content = null
        }
        if (String(source) === "") {
            error = ""
            return
        }
        content = SceneIO.instantiate(prefab, SceneIO.resolvedUrl(prefab, source))
        error = content ? "" : SceneIO.lastError
    }
    onSourceChanged: if (_ready) _build()
    property bool _ready: false
    Component.onCompleted: {
        _ready = true
        _build()
    }
}
