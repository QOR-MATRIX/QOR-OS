// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick
import QOR.Kit

// The same approved mark is rasterised once for the pre-Qt splash and the
// software renderer. Its shape comes from QorMark.qml, not a second drawing.
Item {
    id: identity
    property real size: 112
    property bool graphicsEnabled: true
    readonly property color backgroundColor: Oklch.themed(Tokens.color.background, 1, Tokens.accentDefaultHue, 0)
    readonly property color inkColor: Oklch.themed(Tokens.color.textPrimary, 1, Tokens.accentDefaultHue, 0)
    readonly property bool imageReady: mark.status === Image.Ready
    implicitWidth: size
    implicitHeight: size

    Image {
        id: mark
        objectName: "startupMark"
        anchors.fill: parent
        source: Qt.resolvedUrl("branding/qor-mark.png")
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: identity.graphicsEnabled && status === Image.Ready
    }
    Text {
        objectName: "startupTextFallback"
        anchors.centerIn: parent
        visible: !identity.graphicsEnabled || mark.status === Image.Error
        text: qsTr("QOR OS")
        textFormat: Text.PlainText
        color: identity.inkColor
        font: Theme.headline
    }
}
