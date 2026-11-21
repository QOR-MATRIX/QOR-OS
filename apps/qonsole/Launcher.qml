// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
pragma ComponentBehavior: Bound

import QtQuick

// Standalone local UI: adapters supply discrete navigation actions to dispatch().
// No shell, simulated services, hardware backend or external process is loaded.
FocusScope {
    id: launcher

    property string page: "home"
    property int homeIndex: 0
    property int sampleX: 1
    property int sampleY: 1
    property int interactions: 0
    signal quitRequested()

    function focusHome() {
        const card = choices.itemAt(homeIndex);
        if (page === "home" && card)
            card.forceActiveFocus(Qt.OtherFocusReason);
    }

    // An independent portable seam, not a physical controller implementation.
    function dispatch(action: string): bool {
        if (page === "home") {
            if (action === "left" || action === "up")
                homeIndex = Math.max(0, homeIndex - 1);
            else if (action === "right" || action === "down")
                homeIndex = Math.min(1, homeIndex + 1);
            else if (action === "confirm") {
                if (homeIndex === 1) {
                    quitRequested();
                } else {
                    page = "sample";
                    sample.forceActiveFocus(Qt.OtherFocusReason);
                }
                return true;
            } else if (action !== "back") {
                return false;
            }
            focusHome();
            return true;
        }
        if (action === "back") {
            page = "home";
            focusHome();
        } else if (action === "left") {
            sampleX = Math.max(0, sampleX - 1);
        } else if (action === "right") {
            sampleX = Math.min(2, sampleX + 1);
        } else if (action === "up") {
            sampleY = Math.max(0, sampleY - 1);
        } else if (action === "down") {
            sampleY = Math.min(2, sampleY + 1);
        } else if (action === "confirm") {
            interactions += 1;
        } else {
            return false;
        }
        return true;
    }

    Keys.onPressed: event => {
        let action = "";
        switch (event.key) {
        case Qt.Key_Left: action = "left"; break;
        case Qt.Key_Right: action = "right"; break;
        case Qt.Key_Up: action = "up"; break;
        case Qt.Key_Down: action = "down"; break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space: action = "confirm"; break;
        case Qt.Key_Escape:
        case Qt.Key_Back: action = "back"; break;
        }
        // Held confirm/back must not activate a newly opened page repeatedly.
        if (event.isAutoRepeat && (action === "confirm" || action === "back")) {
            event.accepted = true;
            return;
        }
        event.accepted = action !== "" && dispatch(action);
    }
    Component.onCompleted: Qt.callLater(focusHome)

    Rectangle {
        anchors.fill: parent
        color: "#0b0e14"
    }
    Column {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 48
        spacing: 8
        Text {
            text: "QOR OS · Qonsole"
            textFormat: Text.PlainText
            color: "#f4f4fa"
            font.pixelSize: 32
        }
        Text {
            text: launcher.page === "home" ? qsTr("Home") : qsTr("Local sample")
            textFormat: Text.PlainText
            color: "#b9b6cc"
            font.pixelSize: 20
        }
    }
    Row {
        anchors.centerIn: parent
        spacing: 24
        visible: launcher.page === "home"
        Repeater {
            id: choices
            model: [qsTr("Open sample"), qsTr("Exit")]
            delegate: Rectangle {
                id: card
                required property int index
                required property string modelData
                objectName: index === 0 ? "openSample" : "exitLauncher"
                width: 264
                height: 184
                radius: 20
                color: activeFocus ? "#302744" : "#171922"
                border.width: activeFocus ? 4 : 1
                border.color: activeFocus ? "#c9b4ff" : "#484451"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: modelData
                Accessible.onPressAction: {
                    launcher.homeIndex = card.index;
                    launcher.dispatch("confirm");
                }
                onActiveFocusChanged: if (activeFocus) launcher.homeIndex = index
                Text {
                    anchors.centerIn: parent
                    text: card.modelData
                    textFormat: Text.PlainText
                    color: "#f4f4fa"
                    font.pixelSize: 24
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        launcher.homeIndex = card.index;
                        launcher.dispatch("confirm");
                    }
                }
            }
        }
    }
    FocusScope {
        id: sample
        objectName: "samplePage"
        anchors.centerIn: parent
        width: 288
        height: 288
        visible: launcher.page === "sample"
        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Local sample: move the tile with arrows, confirm changes its colour")
        Rectangle {
            anchors.fill: parent
            radius: 16
            color: "#171922"
            border.width: sample.activeFocus ? 3 : 1
            border.color: "#c9b4ff"
        }
        Rectangle {
            x: 16 + launcher.sampleX * 88
            y: 16 + launcher.sampleY * 88
            width: 80
            height: 80
            radius: 12
            color: launcher.interactions % 2 ? "#7ee0bd" : "#c9b4ff"
        }
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        text: launcher.page === "home"
            ? qsTr("Arrows · Move     Enter · Confirm     Esc · Back")
            : qsTr("Arrows · Move tile     Enter · Change colour     Esc · Home")
        textFormat: Text.PlainText
        color: "#b9b6cc"
        font.pixelSize: 18
    }
}
