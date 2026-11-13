// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick
import QOR.Kit

// Finite session handoff. Readiness releases it immediately; there is no
// minimum display time, progress fiction, service wait or repeating animation.
Rectangle {
    id: splash
    property bool ready: false
    property bool graphicsEnabled: true
    property string statusText: ""
    property int timeoutMs: 1500
    readonly property bool active: !_finished
    readonly property bool settled: _finished
    readonly property real markSize: Math.min(160, Math.max(72, Math.min(width, height) * 0.12))
    property bool _finished: false
    property bool _leaving: false
    property bool _constructed: false
    signal finished()

    color: identity.backgroundColor
    visible: active && width > 0 && height > 0
    opacity: 1

    function dismiss(): void {
        if (_finished)
            return;
        leaving.stop();
        _finished = true;
        opacity = 0;
        finished();
    }

    function release(): void {
        if (!_constructed || _finished || _leaving)
            return;
        if (Theme.reduceMotion) {
            dismiss();
            return;
        }
        _leaving = true;
        leaving.start();
    }

    onReadyChanged: if (ready) release()
    Component.onCompleted: {
        _constructed = true;
        // A hot-plugged output must not replay the startup branding.
        if (ready)
            dismiss();
    }
    Connections {
        target: Theme
        function onReduceMotionChanged() {
            if (Theme.reduceMotion && splash._leaving)
                splash.dismiss();
        }
    }

    StartupIdentity {
        id: identity
        objectName: "startupIdentity"
        anchors.centerIn: parent
        size: splash.markSize
        graphicsEnabled: splash.graphicsEnabled
    }
    Text {
        objectName: "startupStatus"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Tokens.spacing.xl
        width: Math.max(0, parent.width - Tokens.spacing.xxxl)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        text: splash.statusText
        visible: text.length > 0
        color: identity.inkColor
        font: Theme.body
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }
    Timer {
        interval: Math.max(100, Math.min(5000, splash.timeoutMs))
        running: splash.active && !splash._leaving
        repeat: false
        onTriggered: splash.dismiss()
    }
    ParallelAnimation {
        id: leaving
        NumberAnimation { target: splash; property: "opacity"; to: 0; duration: Motion.snappy.response * 1000; easing.type: Easing.OutCubic }
        NumberAnimation { target: identity; property: "scale"; to: 1.025; duration: Motion.snappy.response * 1000; easing.type: Easing.OutCubic }
        onFinished: splash.dismiss()
    }
}
