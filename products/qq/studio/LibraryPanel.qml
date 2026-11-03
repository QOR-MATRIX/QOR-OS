// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// The asset library (QOR-0021): the packs the Store installed, as the Studio's own checks find them (AssetLibrary), each with its
// name, version, developer, licence and where it came from, and its files, a picture shown as itself. Use copies the whole pack
// into the project, and a click on a file copies that one, with the pack's licence and notice either way; Update offers the
// installed version when it is newer than the project's. What the project uses that the Store no longer has is listed as such,
// and its copies stay. Everything here came from someone else, so it is text, never markup. Nothing here moves or animates.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QQ

ColumnLayout {
    id: panel

    /// The Studio's window: what using a pack asks of it.
    required property QtObject studio
    /// What AssetLibrary.state() says of the project.
    property var library: ({ packs: [], missing: [], refused: [], lockProblem: "" })

    spacing: 4

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Library")
            font.family: Look.ui
            font.weight: Font.DemiBold
            font.pixelSize: Look.sizeMicro
            font.letterSpacing: Look.sizeMicro * Look.trackingEyebrow
            font.capitalization: Font.AllUppercase
            color: Look.inkMuted
            Layout.fillWidth: true
        }
        StudioButton {
            objectName: "libraryRefresh"
            text: qsTr("Refresh")
            flat: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Look again at what the Store has installed")
            onClicked: panel.studio.refreshAssets()
        }
    }

    Label {
        objectName: "lockProblem"
        visible: text !== ""
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: 11
        color: Look.bad
        text: panel.library.lockProblem ?? ""
    }

    ListView {
        id: list
        objectName: "packList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        clip: true
        spacing: 6
        model: panel.library.packs ?? []
        delegate: ColumnLayout {
            id: pack
            required property var modelData
            width: list.width
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label {
                    objectName: "packName"
                    Layout.fillWidth: true
                    text: pack.modelData.name + "  " + pack.modelData.version
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                StudioButton {
                    objectName: "update:" + pack.modelData.id
                    visible: pack.modelData.update === true
                    text: qsTr("Update")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("This project has %1: see what updating changes").arg(pack.modelData.used)
                    onClicked: panel.studio.offerUpdate(pack.modelData.id)
                }
                StudioButton {
                    objectName: "use:" + pack.modelData.id
                    text: pack.modelData.used === "" ? qsTr("Use") : qsTr("Use all")
                    enabled: pack.modelData.used === "" || pack.modelData.used === pack.modelData.version
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Copy all of it into the project's assets/%1/").arg(pack.modelData.id)
                    onClicked: panel.studio.useAssets(pack.modelData.id, [])
                }
            }
            Label {
                objectName: "packAbout"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                font.pixelSize: 11
                opacity: 0.7
                text: qsTr("%1 · %2 · %3").arg(pack.modelData.developer).arg(pack.modelData.licence).arg(pack.modelData.provenance)
                      + (pack.modelData.used !== "" ? "\n" + qsTr("In this project: %1").arg(pack.modelData.used) : "")
            }
            Repeater {
                model: pack.modelData.files
                delegate: ItemDelegate {
                    id: file
                    required property var modelData
                    objectName: "useFile:" + pack.modelData.id + "/" + modelData.path
                    Layout.fillWidth: true
                    implicitHeight: 26
                    enabled: pack.modelData.used === "" || pack.modelData.used === pack.modelData.version
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: file.modelData.inProject !== "" ? qsTr("In the project as %1").arg(file.modelData.inProject)
                                                                  : qsTr("Copy this into the project")
                    onClicked: panel.studio.useAssets(pack.modelData.id, [file.modelData.path])
                    contentItem: RowLayout {
                        spacing: 8
                        // A picture as itself, decoded small; anything else by a mark.
                        Image {
                            visible: file.modelData.kind === "picture"
                            source: visible ? file.modelData.url : ""
                            sourceSize: Qt.size(40, 40)
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            asynchronous: true
                        }
                        Rectangle {
                            visible: file.modelData.kind !== "picture"
                            width: 9
                            height: 9
                            radius: 5
                            color: file.modelData.kind === "model" ? "#8ec5ff" : file.modelData.kind === "sound" ? "#c79bff"
                                 : file.modelData.kind === "sky" ? "#f2a35e" : Look.inkMuted
                            Layout.leftMargin: 4
                            Layout.rightMargin: 5
                        }
                        Label {
                            text: file.modelData.path
                            textFormat: Text.PlainText
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: file.modelData.inProject !== ""
                            text: qsTr("in use")
                            font.pixelSize: 10
                            color: Look.inkFaint
                        }
                    }
                }
            }
        }
    }

    Repeater {
        model: panel.library.missing ?? []
        delegate: Label {
            required property var modelData
            objectName: "missingPack"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.pixelSize: 11
            opacity: 0.8
            text: qsTr("%1 %2 is no longer installed. Its %3 file(s) stay in the project, under %4.")
                      .arg(modelData.name).arg(modelData.version).arg(modelData.files).arg(modelData.licence)
        }
    }

    Repeater {
        model: panel.library.refused ?? []
        delegate: Label {
            required property var modelData
            objectName: "refusedPack"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.pixelSize: 11
            color: Look.bad
            text: qsTr("%1 cannot be used: %2").arg(modelData.id).arg(modelData.problem)
        }
    }
}
