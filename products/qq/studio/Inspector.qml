// The selected entity's fields, each with the editor its type calls for. Nothing here knows an entity type: an entity
// declares `fields`, and optionally `ranges` (sliders) and `choices` (a list to pick from); SceneIO.fieldType says what
// each field holds. A change is written to the entity at once, and `edited` tells the Studio the scene changed.

// Changed for QOR OS: Open outside opens only the project's own QML (QOR-0018).
// Changed for QOR OS: the colour swatch gives its preferred size to the layout instead of competing with it.
// Changed for QOR OS: field editors keep their inspector context, typed loader fields and literal source filenames.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QQ

ColumnLayout {
    id: inspector

    // The selected objects have different types; their declared field metadata is read dynamically.
    property QtObject target: null
    signal edited()
    /// What was edited last, as the target's name and the field ("Crate/colour"): the Studio's undo makes quick edits of
    /// one field a single step.
    property string lastEdit: ""


    spacing: 10

    readonly property var labels: ({
        name: qsTr("Name"), position: qsTr("Position"), eulerRotation: qsTr("Rotation"), scale: qsTr("Scale"),
        form: qsTr("Form"), colour: qsTr("Colour"), color: qsTr("Colour"), metalness: qsTr("Metal"),
        roughness: qsTr("Roughness"), emissive: qsTr("Glow colour"), emissivePower: qsTr("Glow"),
        brightness: qsTr("Brightness"), reach: qsTr("Reach"), elevation: qsTr("Height"), azimuth: qsTr("Direction"),
        extent: qsTr("Extent"), source: qsTr("Model file"), skyTop: qsTr("Sky, top"), skyHorizon: qsTr("Sky, horizon"),
        groundHorizon: qsTr("Ground, horizon"), groundBottom: qsTr("Ground, below"), skyLight: qsTr("Sky light"),
        exposure: qsTr("Exposure"), bloom: qsTr("Bloom"), gravity: qsTr("Gravity"), body: qsTr("In play"),
        mass: qsTr("Mass"), bounce: qsTr("Bounce"), friction: qsTr("Friction"), speed: qsTr("Speed"),
        jumpHeight: qsTr("Jump"), rate: qsTr("Rate"), life: qsTr("Life"), size: qsTr("Size"), spread: qsTr("Spread"),
        volume: qsTr("Volume"), loops: qsTr("Loops"), target: qsTr("Drives"), image: qsTr("Picture"),
        imageSize: qsTr("Picture size"), normalMap: qsTr("Surface detail"), crisp: qsTr("Sharp pixels"), sky: qsTr("Sky image"),
        skyTurn: qsTr("Sky turn"), cone: qsTr("Cone"), softness: qsTr("Softness")
    })
    // A file field is named for what it holds.
    readonly property var fileLabels: ({ Prop: qsTr("Model file"), Sound: qsTr("Sound file"), Behaviour: qsTr("Logic file") })

    // Optional dictionaries belong to different entity types, not to their shared QObject base.
    function metadata(field: string): var {
        return target ? target[field] : undefined
    }

    function labelFor(field) {
        if (field === "source" && target && fileLabels[target.kind])
            return fileLabels[target.kind]
        return labels[field] ?? field
    }
    function set(field, value) {
        target[field] = value
        lastEdit = target.name + "/" + field
        inspector.edited()
    }

    component FieldLoader: Loader {
        required property string modelData
        readonly property string field: modelData
    }

    Repeater {
        model: inspector.target ? inspector.target.fields : []

        delegate: FieldLoader {
            id: row
            readonly property string type: SceneIO.fieldType(inspector.target, field)
            Layout.fillWidth: true
            sourceComponent: inspector.target && inspector.metadata("choices") && inspector.metadata("choices")[field] ? choiceEditor
                           : type === "vector3d" ? vectorEditor
                           : type === "color" ? colourEditor
                           : type === "bool" ? boolEditor
                           : type === "url" ? urlEditor
                           : type === "real" || type === "int" ? numberEditor
                           : textEditor
        }
    }

    component FieldLabel: Label {
        textFormat: Text.PlainText
        font.family: Look.ui
        font.weight: Font.DemiBold
        font.pixelSize: Look.sizeMicro
        font.letterSpacing: Look.sizeMicro * Look.trackingEyebrow
        font.capitalization: Font.AllUppercase
        color: Look.inkMuted
    }

    Component {
        id: textEditor
        ColumnLayout {
            id: textRow
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            spacing: 2
            FieldLabel { text: inspector.labelFor(textRow.field) }
            TextField {
                Layout.fillWidth: true
                // A row can outlive its old target for a moment while the selected kind changes.
                text: inspector.target ? inspector.target[textRow.field] ?? "" : ""
                onEditingFinished: if (text !== inspector.target[textRow.field]) inspector.set(textRow.field, text)
            }
        }
    }

    Component {
        id: numberEditor
        ColumnLayout {
            id: numberRow
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            readonly property var range: inspector.target && inspector.metadata("ranges") ? inspector.metadata("ranges")[field] : undefined
            spacing: 2
            RowLayout {
                FieldLabel { text: inspector.labelFor(numberRow.field); Layout.fillWidth: true }
                Label {
                    text: inspector.target ? Number(inspector.target[numberRow.field]).toFixed(2) : ""
                    font.pixelSize: 12
                    opacity: 0.8
                }
            }
            Slider {
                Layout.fillWidth: true
                visible: numberRow.range !== undefined
                from: numberRow.range ? numberRow.range[0] : 0
                to: numberRow.range ? numberRow.range[1] : 1
                value: inspector.target ? inspector.target[numberRow.field] ?? 0 : 0
                onMoved: inspector.set(numberRow.field, value)
            }
            TextField {
                Layout.fillWidth: true
                visible: numberRow.range === undefined
                validator: DoubleValidator {}
                text: inspector.target ? String(inspector.target[numberRow.field]) : ""
                onEditingFinished: inspector.set(numberRow.field, Number(text))
            }
        }
    }

    Component {
        id: vectorEditor
        ColumnLayout {
            id: vectorRow
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            readonly property vector3d value: inspector.target ? inspector.target[field] : Qt.vector3d(0, 0, 0)
            spacing: 2
            FieldLabel { text: inspector.labelFor(vectorRow.field) }
            RowLayout {
                spacing: 6
                Repeater {
                    model: ["x", "y", "z"]
                    delegate: TextField {
                        id: axisField
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredWidth: 60
                        validator: DoubleValidator {}
                        placeholderText: modelData
                        text: Number(vectorRow.value[modelData]).toFixed(2).replace(/\.?0+$/, "")
                        leftPadding: 18
                        Label {
                            text: axisField.modelData
                            x: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: ["#ff4d5e", "#6cf07f", "#4da3ff"][axisField.index]
                            font.pixelSize: 11
                        }
                        onEditingFinished: {
                            const v = vectorRow.value
                            const n = Number(text)
                            inspector.set(vectorRow.field, Qt.vector3d(index === 0 ? n : v.x, index === 1 ? n : v.y,
                                                                       index === 2 ? n : v.z))
                        }
                    }
                }
            }
        }
    }

    Component {
        id: colourEditor
        ColumnLayout {
            id: colourRow
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            spacing: 2
            FieldLabel { text: inspector.labelFor(colourRow.field) }
            RowLayout {
                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: 4
                    color: inspector.target ? inspector.target[colourRow.field] : "black"
                    border.color: Qt.rgba(1, 1, 1, 0.25)
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: picker.open()
                    }
                }
                TextField {
                    Layout.fillWidth: true
                    text: inspector.target ? String(inspector.target[colourRow.field]) : ""
                    onEditingFinished: if (/^#[0-9a-fA-F]{6}$/.test(text)) inspector.set(colourRow.field, text)
                }
            }
            ColorDialog {
                id: picker
                selectedColor: inspector.target ? inspector.target[colourRow.field] ?? "black" : "black"
                onAccepted: inspector.set(colourRow.field, selectedColor)
            }
        }
    }

    Component {
        id: boolEditor
        Switch {
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            text: inspector.labelFor(field)
            // As the selection changes kind, a row can briefly read a field the new one does not have.
            checked: inspector.target ? inspector.target[field] === true : false
            onToggled: inspector.set(field, checked)
        }
    }

    Component {
        id: choiceEditor
        ColumnLayout {
            id: choiceRow
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            // Guarded: while the selection changes, this row can briefly see an entity that has no choices.
            readonly property var options: inspector.target && inspector.metadata("choices")
                                           ? inspector.metadata("choices")[field] ?? [] : []
            spacing: 2
            FieldLabel { text: inspector.labelFor(choiceRow.field) }
            ComboBox {
                Layout.fillWidth: true
                model: choiceRow.options
                currentIndex: inspector.target ? choiceRow.options.indexOf(inspector.target[choiceRow.field]) : -1
                onActivated: (i) => inspector.set(choiceRow.field, choiceRow.options[i])
            }
        }
    }

    Component {
        id: urlEditor
        ColumnLayout {
            id: urlRow
            readonly property string field: (parent as FieldLoader)?.field ?? ""
            spacing: 2
            FieldLabel { text: inspector.labelFor(urlRow.field) }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
                text: inspector.target ? decodeURIComponent(String(inspector.target[urlRow.field]).split("/").pop()) : ""
            }
            RowLayout {
                StudioButton {
                    text: qsTr("Replace…")
                    flat: true
                    onClicked: fileChooser.open()
                }
                // A QML file (logic, a prefab) can also be opened in the creator's own editor; a save there reloads it
                // in play. The Studio's own logic editor is the inspector's Edit logic (P3.15).
                StudioButton {
                    visible: inspector.target !== null && /\.qml$/i.test(String(inspector.target[urlRow.field]))
                    text: qsTr("Open outside")
                    flat: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("In your own editor")
                    onClicked: SceneIO.openInOwnEditor(SceneIO.resolvedUrl(inspector.target, inspector.target[urlRow.field]))
                }
            }
            FileDialog {
                id: fileChooser
                title: qsTr("Choose a file")
                nameFilters: inspector.target && inspector.metadata("fileTypes") && inspector.metadata("fileTypes")[urlRow.field]
                             ? inspector.metadata("fileTypes")[urlRow.field] : [qsTr("Any file (*)")]
                onAccepted: inspector.set(urlRow.field, selectedFile)
            }
        }
    }
}
