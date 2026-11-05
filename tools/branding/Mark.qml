// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick
import QOR.Kit

// Compile the existing mark, with its original aspect ratio and default accent.
Item {
    id: canvas
    width: 1024
    height: 1024
    readonly property color backgroundColor: Oklch.themed(Tokens.color.background, 1, Tokens.accentDefaultHue, 0)
    QorMark {
        anchors.centerIn: parent
        size: Math.min(canvas.width, canvas.height) - 8
        hue: Tokens.accentDefaultHue
    }
}
