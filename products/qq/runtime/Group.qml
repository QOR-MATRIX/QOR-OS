// A group (DIRECTION P3.15): entities kept together under one place, turn and scale, moved, turned and scaled as one,
// and written inside it in the scene's file. A house of shapes, a lamp post with its lamp, a level's section: move the
// group and all of it goes; logic moves one as it moves anything (a moving platform carrying what stands on it).
//
// What is in it reads the scene's state from it (playing, paused, a replay, a 2D view and its pixels), so it passes
// those on from its own parent. Players, the ground, the sun, tile maps, and what has no place in the world stay at the
// scene's top (SceneIO::nestable). A group made into a prefab is written to a file of its own (Prefab.qml).

// Changed for QOR OS: state reads preserve the mixed parent types through their structural interface.

import QtQuick
import QtQuick3D

Node {
    id: group

    readonly property string kind: "Group"
    readonly property var fields: ["name", "position", "eulerRotation", "scale"]

    property string name: "Group"
    /// The prefab file it was unpacked from, if it was (the Studio keeps it back there). Not a field.
    property url fromPrefab

    // The scene's state, passed on to what is in the group.
    readonly property bool playing: parent !== null && _parentValue("playing") === true
    readonly property bool replaying: parent !== null && _parentValue("replaying") === true
    readonly property bool paused: parent !== null && _parentValue("paused") === true
    readonly property real _owed: parent !== null && _parentValue("_owed") !== undefined ? _parentValue("_owed") : 0
    readonly property string view: parent !== null && _parentValue("view") !== undefined ? _parentValue("view") : "3d"
    readonly property real pixelsPerUnit: parent !== null && _parentValue("pixelsPerUnit") !== undefined ? _parentValue("pixelsPerUnit") : 16
    /// True while the scene this belongs to plays.
    readonly property bool live: playing

    // A parent can be a Scene, Group or Prefab; none is the sole parent type.
    function _parentValue(field: string): var { return group.parent[field] }
}
