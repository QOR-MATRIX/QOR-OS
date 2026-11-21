// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
import QtQuick

Window {
    id: window
    width: 960
    height: 640
    minimumWidth: 720
    minimumHeight: 540
    visible: true
    color: "#0b0e14"
    title: "QOR OS · Qonsole"

    Launcher {
        objectName: "qonsoleLauncher"
        anchors.fill: parent
        focus: true
        onQuitRequested: Qt.quit()
    }
}
