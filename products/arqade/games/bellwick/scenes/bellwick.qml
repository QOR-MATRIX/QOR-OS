// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QQ

Scene {
    name: "bellwick"
    skyTop: "#3f6fb5"
    skyHorizon: "#cfe8f6"
    groundHorizon: "#cfe8f6"
    groundBottom: "#7c8ea0"
    sky: "../assets/sky-0.hdr"
    skyTurn: 0
    skyLight: 0.95
    exposure: 1
    bloom: true
    gravity: 9.81
    screens: "title"
    view: "3d"
    pixelsPerUnit: 16
    viewHeight: 12
    pixelArt: false
    seed: 0

    Sun {
        name: "Sun"
        elevation: 54
        azimuth: -24
        color: "#fff2d8"
        brightness: 1.15
    }

    Group {
        name: "Stage"
        position: Qt.vector3d(0, 1, 0)
        eulerRotation: Qt.vector3d(0, 25, 0)
        scale: Qt.vector3d(1, 1, 1)

        Group {
            name: "Tilt"
            position: Qt.vector3d(0, 0, 0)
            eulerRotation: Qt.vector3d(-7, 0, 0)
            scale: Qt.vector3d(1.18, 1.18, 1.18)

            Prop {
                name: "Far"
                position: Qt.vector3d(0, 0, 0)
                eulerRotation: Qt.vector3d(0, 0, 0)
                scale: Qt.vector3d(1, 1, 1)
                source: "../assets/backdrop-0.glb"
                clip: ""
                clipSpeed: 1
                clipLoops: true
            }

            Group {
                name: "Level"
                position: Qt.vector3d(0, 0, 0)
                eulerRotation: Qt.vector3d(0, 0, 0)
                scale: Qt.vector3d(1, 1, 1)

                Prop {
                    name: "Ground"
                    position: Qt.vector3d(0, 0, 0)
                    eulerRotation: Qt.vector3d(0, 0, 0)
                    scale: Qt.vector3d(1, 1, 1)
                    source: "../assets/level-0.glb"
                    clip: ""
                    clipSpeed: 1
                    clipLoops: true
                }

                Prop {
                    name: "Tam 0"
                    position: Qt.vector3d(0, 0, 0)
                    eulerRotation: Qt.vector3d(0, 0, 0)
                    scale: Qt.vector3d(1, 1, 1)
                    source: "../assets/tam.glb"
                    clip: "idle"
                    clipSpeed: 1
                    clipLoops: true
                }

                Prop {
                    name: "Tam 1"
                    position: Qt.vector3d(0, -80, 0)
                    eulerRotation: Qt.vector3d(0, 0, 0)
                    scale: Qt.vector3d(1, 1, 1)
                    source: "../assets/tam-brass.glb"
                    clip: "idle"
                    clipSpeed: 1
                    clipLoops: true
                }

                Prop {
                    name: "Tam 2"
                    position: Qt.vector3d(0, -80, 0)
                    eulerRotation: Qt.vector3d(0, 0, 0)
                    scale: Qt.vector3d(1, 1, 1)
                    source: "../assets/tam-kite.glb"
                    clip: "idle"
                    clipSpeed: 1
                    clipLoops: true
                }

                Prop {
                    name: "Great bell"
                    position: Qt.vector3d(0, -80, 0)
                    eulerRotation: Qt.vector3d(0, 0, 0)
                    scale: Qt.vector3d(1, 1, 1)
                    source: "../assets/great-bell.glb"
                    clip: "hang"
                    clipSpeed: 1
                    clipLoops: true
                }

                Shape {
                    name: "Ring wave"
                    position: Qt.vector3d(0, -80, 0)
                    eulerRotation: Qt.vector3d(90, 0, 0)
                    scale: Qt.vector3d(0.4, 0.02, 0.4)
                    form: "cylinder"
                    colour: "#3a2c08"
                    metalness: 0
                    roughness: 0.6
                    emissive: "#ffd24a"
                    emissivePower: 0.9
                    body: "none"
                }

            }

        }

    }

    Behaviour {
        name: "Bellwick"
        source: "../logic/bellwick.qml"
        target: ""
    }

    Music {
        name: "Music"
        source: "../assets/world-0.wav"
        volume: 0.42
        fade: 1.5
        loops: true
        group: "music"
    }

    ScreenPanel {
        name: "Title veil"
        screen: "title"
        anchor: "centre"
        insetX: 0
        insetY: 0
        width: 0
        height: 0
        colour: "#05060a"
        shade: 0.28
    }

    ScreenText {
        name: "Title"
        screen: "title"
        anchor: "centre"
        insetX: 0
        insetY: -180
        text: "BELLWICK"
        size: 104
        colour: "#ffe9a8"
        typeface: "display"
    }

    ScreenText {
        name: "Tagline"
        screen: "title"
        anchor: "centre"
        insetX: 0
        insetY: -92
        text: "The Hush has muffled the bells. Run, jump and ring them back."
        size: 26
        colour: "#fff6dc"
        typeface: "ui"
    }

    ScreenText {
        name: "How"
        screen: "title"
        anchor: "centre"
        insetX: 0
        insetY: 130
        text: "Move with A and D, the arrows, or the pad's stick. Jump with Space or A; hold it to go higher. Run with Shift or X."
        size: 18
        colour: "#e8e4d8"
        typeface: "ui"
    }

    ScreenText {
        name: "How more"
        screen: "title"
        anchor: "centre"
        insetX: 0
        insetY: 162
        text: "Ring with K or B to stun what is near and wake hush blocks. In the air, S or down slams. W or up goes through a door."
        size: 18
        colour: "#e8e4d8"
        typeface: "ui"
    }

    ScreenButton {
        name: "Play"
        screen: "title"
        anchor: "centre"
        insetX: 0
        insetY: 246
        width: 0
        text: "Play"
        colour: "#f3c84b"
        action: ""
    }

    ScreenPanel {
        name: "Top band"
        screen: "hud"
        anchor: "top"
        insetX: 0
        insetY: 0
        width: 2600
        height: 74
        colour: "#0a0c16"
        shade: 0.46
    }

    ScreenText {
        name: "Notes"
        screen: "hud"
        anchor: "top-left"
        insetX: 36
        insetY: 22
        text: "NOTES 0"
        size: 22
        colour: "#ffe9a8"
        typeface: "ui"
    }

    ScreenText {
        name: "Lives"
        screen: "hud"
        anchor: "top-left"
        insetX: 200
        insetY: 22
        text: "LIVES 3"
        size: 22
        colour: "#fff6dc"
        typeface: "ui"
    }

    ScreenText {
        name: "Silver"
        screen: "hud"
        anchor: "top-left"
        insetX: 340
        insetY: 22
        text: "SILVER 0 OF 3"
        size: 22
        colour: "#e8f4fa"
        typeface: "ui"
    }

    ScreenText {
        name: "Score"
        screen: "hud"
        anchor: "top"
        insetX: 0
        insetY: 8
        text: "0"
        size: 30
        colour: "#fff6dc"
        typeface: "display"
    }

    ScreenText {
        name: "Level name"
        screen: "hud"
        anchor: "top"
        insetX: 0
        insetY: 46
        text: ""
        size: 14
        colour: "#c8c4d0"
        typeface: "ui"
    }

    ScreenText {
        name: "Time"
        screen: "hud"
        anchor: "top-right"
        insetX: 180
        insetY: 24
        text: "TIME 200"
        size: 22
        colour: "#fff6dc"
        typeface: "ui"
    }

    ScreenText {
        name: "Message"
        screen: "hud"
        anchor: "centre"
        insetX: 0
        insetY: -190
        text: ""
        size: 34
        colour: "#ffe9a8"
        typeface: "display"
    }

    ScreenButton {
        name: "Pause"
        screen: "hud"
        anchor: "top-right"
        insetX: 28
        insetY: 12
        width: 0
        text: "Pause"
        colour: "#c8c4d0"
        action: "pause"
    }

    ScreenButton {
        name: "Left"
        screen: "hud"
        anchor: "bottom-left"
        insetX: 28
        insetY: 40
        width: 88
        text: "Left"
        colour: "#74cdd3"
        action: "left"
    }

    ScreenButton {
        name: "Right"
        screen: "hud"
        anchor: "bottom-left"
        insetX: 124
        insetY: 40
        width: 88
        text: "Right"
        colour: "#74cdd3"
        action: "right"
    }

    ScreenButton {
        name: "Up"
        screen: "hud"
        anchor: "bottom-left"
        insetX: 76
        insetY: 98
        width: 88
        text: "Up"
        colour: "#74cdd3"
        action: "up"
    }

    ScreenButton {
        name: "Down"
        screen: "hud"
        anchor: "bottom-left"
        insetX: 220
        insetY: 40
        width: 88
        text: "Down"
        colour: "#74cdd3"
        action: "down"
    }

    ScreenButton {
        name: "Jump"
        screen: "hud"
        anchor: "bottom-right"
        insetX: 28
        insetY: 40
        width: 96
        text: "Jump"
        colour: "#f3c84b"
        action: "jump"
    }

    ScreenButton {
        name: "Run"
        screen: "hud"
        anchor: "bottom-right"
        insetX: 132
        insetY: 40
        width: 88
        text: "Run"
        colour: "#74cdd3"
        action: "run"
    }

    ScreenButton {
        name: "Ring"
        screen: "hud"
        anchor: "bottom-right"
        insetX: 80
        insetY: 98
        width: 96
        text: "Ring"
        colour: "#ffe9a8"
        action: "ring"
    }

    ScreenText {
        name: "Foe"
        screen: "foe"
        anchor: "top"
        insetX: 0
        insetY: 92
        text: "OLD MUFFLE"
        size: 16
        colour: "#d8c8ff"
        typeface: "ui"
    }

    ScreenBar {
        name: "Foe health"
        screen: "foe"
        anchor: "top"
        insetX: 0
        insetY: 118
        width: 360
        height: 10
        value: 1
        maximum: 1
        colour: "#b07cff"
    }

    ScreenPanel {
        name: "Hurt veil"
        screen: "hurt"
        anchor: "centre"
        insetX: 0
        insetY: 0
        width: 0
        height: 0
        colour: "#5a0a0a"
        shade: 0.22
    }

    ScreenPanel {
        name: "Pause veil"
        screen: "pause"
        anchor: "centre"
        insetX: 0
        insetY: 0
        width: 0
        height: 0
        colour: "#05060a"
        shade: 0.6
    }

    ScreenText {
        name: "Paused"
        screen: "pause"
        anchor: "centre"
        insetX: 0
        insetY: -60
        text: "PAUSED"
        size: 64
        colour: "#ffe9a8"
        typeface: "display"
    }

    ScreenButton {
        name: "Resume"
        screen: "pause"
        anchor: "centre"
        insetX: 0
        insetY: 40
        width: 0
        text: "Resume"
        colour: "#f3c84b"
        action: ""
    }

    ScreenPanel {
        name: "Result veil"
        screen: "result"
        anchor: "centre"
        insetX: 0
        insetY: 0
        width: 0
        height: 0
        colour: "#05060a"
        shade: 0.5
    }

    ScreenText {
        name: "Result"
        screen: "result"
        anchor: "centre"
        insetX: 0
        insetY: -120
        text: "THE BELLS RING AGAIN"
        size: 62
        colour: "#ffe9a8"
        typeface: "display"
    }

    ScreenText {
        name: "Final"
        screen: "result"
        anchor: "centre"
        insetX: 0
        insetY: -40
        text: "SCORE 0"
        size: 30
        colour: "#fff6dc"
        typeface: "ui"
    }

    ScreenText {
        name: "Details"
        screen: "result"
        anchor: "centre"
        insetX: 0
        insetY: 4
        text: ""
        size: 18
        colour: "#c8c4d0"
        typeface: "ui"
    }

    ScreenButton {
        name: "Play again"
        screen: "result"
        anchor: "centre"
        insetX: 0
        insetY: 70
        width: 0
        text: "Play again"
        colour: "#f3c84b"
        action: ""
    }

    InputAction {
        name: "left"
        keys: "A, Left"
        pad: "Left, LS Left"
    }

    InputAction {
        name: "right"
        keys: "D, Right"
        pad: "Right, LS Right"
    }

    InputAction {
        name: "up"
        keys: "W, Up"
        pad: "Up, LS Up"
    }

    InputAction {
        name: "down"
        keys: "S, Down"
        pad: "Down, LS Down"
    }

    InputAction {
        name: "jump"
        keys: "Space, J"
        pad: "A"
    }

    InputAction {
        name: "run"
        keys: "Shift, L"
        pad: "X"
    }

    InputAction {
        name: "ring"
        keys: "K, E"
        pad: "B"
    }

    InputAction {
        name: "pause"
        keys: "P"
        pad: "Start"
    }
}
