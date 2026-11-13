// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QOR.Kit
import QOR.Setup

ApplicationWindow {
    id: window
    required property InstallerState installer
    title: qsTr("Install QOR OS — disposable VM")
    visible: true
    width: 1120
    height: 800
    minimumWidth: 640
    minimumHeight: 540
    color: Theme.background
    Binding { target: Theme; property: "dark"; value: true }
    onClosing: close => { if (installer.writing) close.accepted = false; }
    Component.onCompleted: installer.inspect()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: window.width >= 900 ? Tokens.spacing.xxxl : Tokens.spacing.xl
        spacing: Tokens.spacing.l
        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.spacing.xl
            StartupIdentity { size: 80 }
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Install QOR OS"); textFormat: Text.PlainText; color: Theme.textPrimary; font: Theme.title1; Accessible.role: Accessible.Heading }
                Text { Layout.fillWidth: true; text: qsTr("Private disposable VM development image"); textFormat: Text.PlainText; color: Theme.textSecondary; font: Theme.body; wrapMode: Text.Wrap }
            }
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: Tokens.spacing.l
                Text {
                    Layout.fillWidth: true
                    text: qsTr("This test creates a local account named qortest. QOR ID sign-in is unavailable. Only the new virtual disk assigned to this VM can be installed.")
                    textFormat: Text.PlainText; color: Theme.textSecondary; font: Theme.callout; wrapMode: Text.Wrap
                }
                Text {
                    objectName: "installStatus"
                    Layout.fillWidth: true
                    text: window.installer.message
                    textFormat: Text.PlainText; color: window.installer.phase === "error" ? Theme.critical : Theme.textPrimary
                    font: Theme.bodyStrong; wrapMode: Text.Wrap
                    Accessible.role: window.installer.phase === "error" ? Accessible.AlertMessage : Accessible.StaticText
                }
                Text {
                    objectName: "installTarget"
                    Layout.fillWidth: true
                    text: window.installer.target
                    textFormat: Text.PlainText; color: Theme.textPrimary; font: Theme.body; wrapMode: Text.WrapAnywhere
                }
                Text {
                    Layout.fillWidth: true
                    text: window.installer.provenance
                    textFormat: Text.PlainText; color: Theme.textSecondary; font: Theme.caption; wrapMode: Text.WrapAnywhere
                }
                CheckBox {
                    text: qsTr("Encrypt this disk — unavailable")
                    checked: false
                    enabled: false
                    Accessible.description: qsTr("Encryption has not passed a separate install, unlock and reboot test. This path supports unencrypted ext4 only.")
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Encryption is disabled until a separate encrypted installation, unlock and reboot is proved. This test uses ext4 on one whole virtual disk.")
                    textFormat: Text.PlainText; color: Theme.textSecondary; font: Theme.body; wrapMode: Text.Wrap
                }
                Button {
                    id: selectDisk
                    objectName: "selectInstallDisk"
                    visible: window.installer.phase === "choose"
                    text: qsTr("Select this virtual disk and review")
                    onClicked: { erase.checked = false; confirmation.clear(); window.installer.review(); }
                    Keys.onReturnPressed: selectDisk.clicked()
                    Keys.onEnterPressed: selectDisk.clicked()
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: window.installer.phase === "review"
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Final review: every partition and all data on the disk identified above will be erased. Confirm its serial, capacity and VM assignment before continuing.")
                        textFormat: Text.PlainText; color: Theme.critical; font: Theme.bodyStrong; wrapMode: Text.Wrap
                    }
                    CheckBox { id: erase; objectName: "eraseAcknowledged"; text: qsTr("Erase this selected virtual disk and install QOR OS"); checked: false }
                    Label { text: qsTr("Type the complete serial: %1").arg(window.installer.serial); textFormat: Text.PlainText; color: Theme.textPrimary; font: Theme.body }
                    TextField {
                        id: confirmation
                        objectName: "installConfirmation"
                        Layout.fillWidth: true
                        maximumLength: 64
                        Accessible.name: qsTr("Confirm selected disk serial")
                    }
                    Button {
                        id: begin
                        objectName: "beginInstall"
                        text: qsTr("Erase selected disk and install")
                        enabled: erase.checked && confirmation.text === window.installer.serial && window.installer.serial.length > 0
                        onClicked: window.installer.install(confirmation.text, erase.checked)
                        Keys.onReturnPressed: begin.clicked()
                        Keys.onEnterPressed: begin.clicked()
                    }
                }
                BusyIndicator { running: window.installer.writing && !Theme.reduceMotion; visible: window.installer.writing }
                Text {
                    Layout.fillWidth: true
                    visible: window.installer.writing
                    text: qsTr("Disk writing cannot be safely cancelled. Progress is reported by the backend; no percentage is estimated. Diagnostics: /run/qor-vm-install/bootc.log")
                    textFormat: Text.PlainText; color: Theme.textSecondary; font: Theme.body; wrapMode: Text.Wrap
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { id: cancel; text: qsTr("Cancel"); enabled: !window.installer.writing; onClicked: Qt.quit(); Keys.onReturnPressed: cancel.clicked(); Keys.onEnterPressed: cancel.clicked() }
            Item { Layout.fillWidth: true }
            Button { id: retry; text: qsTr("Check again"); visible: window.installer.phase === "error"; enabled: window.installer.retryAllowed; onClicked: window.installer.inspect(); Keys.onReturnPressed: retry.clicked(); Keys.onEnterPressed: retry.clicked() }
            Button { id: rebootAction; text: qsTr("Reboot to verify installation"); visible: window.installer.phase === "complete"; onClicked: window.installer.reboot(); Keys.onReturnPressed: rebootAction.clicked(); Keys.onEnterPressed: rebootAction.clicked() }
        }
    }
}
