// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QOR.Kit
import QOR.Setup

ApplicationWindow {
    id: window
    required property SetupState setupState
    property int page: 0
    readonly property var headings: [qsTr("Welcome home."), qsTr("Everything at hand."), qsTr("Ready when you are.")]
    readonly property var descriptions: [
        qsTr("A place to play, make and explore. You can explore this welcome offline. Connecting a QOR ID is your choice."),
        qsTr("Your Library brings your games together. QQ is your space to create. Settings holds display, sound and accessibility controls."),
        qsTr("Take your time. You can change your preferences in Settings whenever you like. Closing this welcome keeps it available next time.")
    ]
    visible: true
    width: 1040
    height: 720
    minimumWidth: 640
    minimumHeight: 540
    title: qsTr("Welcome to QOR OS")
    color: Theme.background
    onPageChanged: if (!Theme.reduceMotion) reveal.restart()
    Binding { target: Theme; property: "dark"; value: 1 }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: window.width >= 900 ? 280 : 160
            Layout.fillHeight: true
            color: Theme.surfaceSunken
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tokens.spacing.xxl
                spacing: Tokens.spacing.xl
                StartupIdentity { size: window.width >= 900 ? 112 : 80; Layout.alignment: Qt.AlignHCenter }
                Text { text: "QOR OS"; textFormat: Text.PlainText; color: Theme.textPrimary; font: Theme.title2; Layout.alignment: Qt.AlignHCenter }
                Item { Layout.fillHeight: true }
                Repeater {
                    model: [qsTr("Welcome"), qsTr("Your desktop"), qsTr("Get started")]
                    delegate: Text {
                        required property string modelData
                        required property int index
                        text: (index + 1) + "  " + modelData
                        textFormat: Text.PlainText
                        color: window.page === index ? Theme.accentText : Theme.textSecondary
                        font: Theme.bodyStrong
                    }
                }
                Item { Layout.fillHeight: true }
                Text { text: qsTr("Yours to shape."); textFormat: Text.PlainText; color: Theme.textSecondary; font: Theme.caption; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
        }
        ColumnLayout {
            id: content
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: window.width >= 900 ? Tokens.spacing.xxxl : Tokens.spacing.xl
            spacing: Tokens.spacing.xl
            Item { Layout.fillHeight: true }
            Text {
                objectName: "setupHeading"
                Layout.fillWidth: true
                text: window.headings[window.page]
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.textPrimary
                font: Theme.title1
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: window.descriptions[window.page]
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font: Theme.callout
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.separator }
            Text {
                Layout.fillWidth: true
                text: qsTr("You are in control. QOR ID is optional; this welcome does not sign you in.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font: Theme.body
            }
            Item { Layout.fillHeight: true }
            Text {
                objectName: "setupError"
                Layout.fillWidth: true
                visible: text.length > 0
                text: window.setupState.error
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.critical
                font: Theme.body
                Accessible.role: Accessible.AlertMessage
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    id: back
                    objectName: "setupBack"
                    text: qsTr("Back")
                    visible: window.page > 0
                    Accessible.name: text
                    onClicked: { window.page--; next.forceActiveFocus(); }
                    Keys.onReturnPressed: back.clicked()
                    Keys.onEnterPressed: back.clicked()
                }
                Item { Layout.fillWidth: true }
                Button {
                    id: next
                    objectName: "setupNext"
                    text: window.page === 2 ? qsTr("Go to desktop") : qsTr("Continue")
                    Accessible.name: text
                    highlighted: true
                    enabled: window.setupState.error.length === 0
                    onClicked: {
                        if (window.page < 2) window.page++;
                        else window.setupState.finish();
                    }
                    Component.onCompleted: forceActiveFocus()
                    Keys.onReturnPressed: next.clicked()
                    Keys.onEnterPressed: next.clicked()
                }
            }
        }
    }
    NumberAnimation {
        id: reveal
        target: content
        property: "opacity"
        from: 0.65
        to: 1
        duration: Motion.snappy.response * 1000
    }
}
