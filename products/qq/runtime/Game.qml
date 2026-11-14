// A game: a world that plays one scene file from the moment it opens (DIRECTION P3.2). It is what the QQ Player shows,
// natively and in a browser: the scene's sky and light, its physics, its player under the keyboard, mouse and gamepad,
// its particles, sound and logic.
//
// With a player in the scene the eye follows it, and a drag looks around; without one the eye orbits the scene, a drag
// turns it and scrolling comes closer. A 2D scene is seen flat (P3.10; pixel art pixel for pixel, ADR-106), its eye on
// the player and stopped at the level's edges, or with no player on the scene from the ground up. `restart()` plays the
// scene again from its file.

// Changed for QOR OS: failed scene startup is reported and its loaded object is discarded before retry.
// Changed for QOR OS: the loaded playingScene does not override View3D.scene.

import QtQuick
import QtQuick3D
import QQ

World {
    id: game

    /// The scene file to play: on this computer, or bundled in qrc:.
    property url file

    /// The scene playing, or null; and why there is none, if there is not.
    property QtObject playingScene: null
    property string problem: ""
    property var entityList: []

    /// True once the scene plays and every model in it has loaded or failed: the first frame of the game proper.
    readonly property bool settled: playingScene !== null && entityList.every(e => e.kind !== "Prop" || e.status !== Prop.Empty)
    /// The models that failed to load, each "name: why". The game plays without them, so the Player reports them
    /// rather than stopping; a game that settles with one missing is not the game its creator made.
    readonly property list<string> unloaded: entityList.filter(e => e.kind === "Prop" && e.status === Prop.Failed)
                                                       .map(e => e.name + ": " + e.errorString)

    readonly property QtObject follow: playingScene ? playingScene.player : null

    // The eye with no player: on a sphere around the middle of the scene.
    property real yaw: 25
    property real pitch: 16
    property real distance: 10
    property vector3d middle: Qt.vector3d(0, 1, 0)

    eye: follow ? follow.eye : Qt.vector3d(
        middle.x + distance * Math.cos(pitch * Math.PI / 180) * Math.sin(yaw * Math.PI / 180),
        middle.y + distance * Math.sin(pitch * Math.PI / 180),
        middle.z + distance * Math.cos(pitch * Math.PI / 180) * Math.cos(yaw * Math.PI / 180))
    // Flat, the eye on the player stops at the level's edges (Scene.keptInView); with no player, it looks from the
    // ground up.
    target: follow ? (flat ? playingScene.keptInView(follow.lookAt, width / (2 * magnification), height / (2 * magnification))
                           : follow.lookAt)
                   : flat ? Qt.vector3d(0, viewHeight / 2, 0) : middle

    flat: playingScene ? playingScene.view === "2d" : false
    pixelsPerUnit: playingScene ? playingScene.pixelsPerUnit : 16
    viewHeight: playingScene ? playingScene.viewHeight : 12
    pixelArt: playingScene ? playingScene.pixelArt : true

    hearing: true
    exposure: playingScene ? playingScene.exposure : 1
    bloom: playingScene ? playingScene.bloom : true
    skyLight: playingScene ? playingScene.skyLight : 0.75
    skyTop: playingScene ? playingScene.skyTop : "#0b1430"
    skyHorizon: playingScene ? playingScene.skyHorizon : "#c7623a"
    groundHorizon: playingScene ? playingScene.groundHorizon : "#2a1a1a"
    groundBottom: playingScene ? playingScene.groundBottom : "#07080c"
    sky: playingScene && String(playingScene.sky) !== "" ? SceneIO.resolvedUrl(playingScene, playingScene.sky) : ""
    skyTurn: playingScene ? playingScene.skyTurn : 0

    /// The game asked its host something (ADR-117): a JSON value. Whoever hosts the game answers with tell().
    signal asked(var message)
    /// Tell the game something (ADR-117): a JSON value its logic hears at the next tick (Logic.onTold). False if it was
    /// refused (not playing, not JSON, larger than 16 KiB, or too many waiting).
    function tell(message) {
        return playingScene ? playingScene.tell(message) : false
    }

    /// A seed for this play, in place of the scene's (P3.11: the Player's --seed, a run ARQADE starts); 0 keeps the scene's.
    property int seed: 0
    /// What keeps the run (a RunRecord: the Player's --record), or null.
    property QtObject recorder: null

    function restart() {
        if (playingScene)
            SceneIO.discard(playingScene)
        playingScene = null
        entityList = []
        Input.release()
        const next = SceneIO.load(file, holder)
        if (!next) {
            problem = SceneIO.lastError
            return false
        }
        try {
            if (seed !== 0)
                next.playSeed = seed
            next.recorder = recorder
            next.asked.connect(game.asked)
            next.playing = true
            problem = ""
            playingScene = next
            entityList = SceneIO.entities(next)
            return true
        } catch (error) {
            playingScene = null
            entityList = []
            SceneIO.discard(next)
            problem = qsTr("This scene could not start.")
            return false
        }
    }

    /// Put the scene away in a safe order (its bodies before their world): what the player does as it quits.
    function release() {
        if (playingScene)
            SceneIO.discard(playingScene)
        playingScene = null
        entityList = []
    }

    property bool started: false
    Component.onCompleted: {
        started = true
        if (String(file) !== "")
            restart()
    }
    onFileChanged: if (started) restart()

    Node { id: holder }

    DragHandler {
        target: null
        enabled: !game.flat || game.follow !== null
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property point last
        property real startYaw
        property real startPitch
        onActiveChanged: {
            last = Qt.point(0, 0)
            startYaw = game.yaw
            startPitch = game.pitch
        }
        onTranslationChanged: {
            if (game.follow) {
                Input.pointerMoved(Qt.point(translation.x - last.x, translation.y - last.y))
                last = Qt.point(translation.x, translation.y)
                return
            }
            game.yaw = startYaw - translation.x * 0.3
            game.pitch = Math.max(-5, Math.min(85, startPitch + translation.y * 0.3))
        }
    }

    WheelHandler {
        enabled: !game.follow && !game.flat
        onWheel: (event) => game.distance = Math.max(1.5, Math.min(80, game.distance * Math.pow(0.9, event.angleDelta.y / 120)))
    }
}
