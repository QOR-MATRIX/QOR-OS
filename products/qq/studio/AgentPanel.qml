// The Agent panel (DIRECTION P3.3): describe a game, and a model designs and builds it in this scene, plays it, looks at
// it and revises. Everything it does is shown here as it happens, frames included; nothing is saved: the creator
// reviews the scene, then saves and commits it, or undoes the whole run.
//
// `designer` is the Studio's DesignLoop (agent/designloop.h). The provider's key is the creator's own, typed once and
// kept in the operating system's keychain; it is never shown again or written anywhere else. The provider bills that
// account for what is used, and Demiurge neither pays for it nor charges for it (ADR-086): without a key, the agent is
// simply off. What each run used is shown under the steps.
//
// Changed for QOR OS: imports QQ, whose Look the panel's colours and faces come from. Without it, Look was undefined:
// the key's problem and the steps were not coloured, and the tool steps were not in the mono face.
// Changed for QOR OS: whether the agent's logic may run; answers, status and key problems are text, never markup (QOR-0018).

// Changed for QOR OS: step delegates keep their panel context and read roles from their own row.
// Changed for QOR OS: body step fonts name the startup application's typed singleton.
// Changed for QOR OS: the structural controller accessor follows the current required object in bindings and actions.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QQ

ColumnLayout {
    id: panel

    required property QtObject designer
    // DesignLoop and inert test controllers share this interface, without a concrete QML controller type.
    function _designerState(): var { return designer }
    /// Whether logic the agent writes may run in play: the creator's say, off until they give it.
    property bool mayRun: false
    signal mayRunAsked(bool allowed)

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Agent")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }
        BusyIndicator {
            running: panel._designerState().running
            visible: running
            implicitWidth: 28
            implicitHeight: 28
        }
    }

    Label {
        objectName: "agentStatus"
        textFormat: Text.PlainText
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: panel._designerState().status !== "" ? panel._designerState().status
                                           : qsTr("Describe a game, and the agent designs and builds it here, plays it, "
                                                  + "looks at it and revises. Nothing is saved until you save.")
        opacity: 0.75
        font.pixelSize: 13
    }

    // Logic runs with the creator's own reach, and a model's answer can be led by what it reads.
    CheckBox {
        objectName: "agentMayRun"
        Layout.fillWidth: true
        checked: panel.mayRun
        text: qsTr("Let the agent play the logic it writes")
        font.pixelSize: 12
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Logic runs with everything you can reach. Off, the agent can write it for you to read, but not run it.")
        onToggled: panel.mayRunAsked(checked)
    }

    // The key, once: kept in the keychain, never shown again.
    ColumnLayout {
        visible: !panel._designerState().hasKey
        Layout.fillWidth: true
        spacing: 6
        Label {
            text: qsTr("Anthropic API key")
            font.pixelSize: 12
            opacity: 0.7
        }
        TextField {
            id: keyField
            objectName: "agentKey"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: "sk-ant-…"
            Accessible.name: qsTr("Anthropic API key")
        }
        StudioButton {
            kind: "ghost"
            text: qsTr("Keep it in the keychain")
            enabled: keyField.text.trim() !== ""
            onClicked: {
                const why = panel._designerState().setKey(keyField.text)
                keyField.text = ""
                keyProblem.text = why
            }
        }
        Label {
            id: keyProblem
            textFormat: Text.PlainText
            visible: text !== ""
            color: Look.bad
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            objectName: "agentWhoPays"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.55
            font.pixelSize: 12
            text: qsTr("The agent runs on your own Anthropic account: Anthropic bills it for what each run uses. Demiurge "
                       + "does not pay for it and does not charge for it. The key is kept in your system keychain "
                       + "for your account only, and never written to a file or a log.")
        }
    }

    TextArea {
        id: brief
        objectName: "agentBrief"
        Layout.fillWidth: true
        Layout.preferredHeight: 110
        wrapMode: TextEdit.Wrap
        enabled: !panel._designerState().running
        placeholderText: qsTr("What should it be? What the player does, the mood, the look, how long a round lasts…")
        Accessible.name: qsTr("Describe the game")
    }

    RowLayout {
        Layout.fillWidth: true
        StudioButton {
            objectName: "agentStart"
            kind: "primary"
            text: qsTr("Design and build")
            enabled: panel._designerState().hasKey && !panel._designerState().running && brief.text.trim() !== ""
            onClicked: panel._designerState().start(brief.text)
        }
        StudioButton {
            kind: "danger"
            text: qsTr("Stop")
            visible: panel._designerState().running
            onClicked: panel._designerState().stop()
        }
        Item { Layout.fillWidth: true }
        StudioButton {
            text: qsTr("Undo the run")
            flat: true
            visible: panel._designerState().canUndo
            onClicked: panel._designerState().undo()
        }
    }

    ListView {
        id: steps
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 200
        clip: true
        spacing: 8
        model: panel._designerState().steps
        onCountChanged: positionViewAtEnd()

        delegate: ColumnLayout {
            id: stepRow
            required property var modelData
            width: steps.width
            spacing: 4
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: stepRow.modelData.text
                font.pixelSize: stepRow.modelData.kind === "said" ? 13 : 12
                font.family: stepRow.modelData.kind === "tool" || stepRow.modelData.kind === "frame" ? Look.mono : Application.font.family
                color: stepRow.modelData.failed ? Look.bad : stepRow.modelData.kind === "done" ? Look.ok : Look.inkBody
                opacity: stepRow.modelData.kind === "tool" ? 0.65 : 1
            }
            Image {
                visible: stepRow.modelData.image !== ""
                source: stepRow.modelData.image
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? width * sourceSize.height / Math.max(1, sourceSize.width) : 0
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }
    }

    // What the run used on the creator's key, as the provider counted it.
    Label {
        objectName: "agentUsage"
        readonly property var u: panel._designerState().usage
        visible: u !== undefined && u.answers !== undefined && u.answers > 0
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 12
        opacity: 0.7
        text: visible ? qsTr("Used on your key: %1 input tokens (%2 read from cache, %3 written to it), %4 output tokens, "
                             + "in %5 answers.")
                            .arg(Number(u.input).toLocaleString(Qt.locale(), "f", 0))
                            .arg(Number(u.cacheRead).toLocaleString(Qt.locale(), "f", 0))
                            .arg(Number(u.cacheWrite).toLocaleString(Qt.locale(), "f", 0))
                            .arg(Number(u.output).toLocaleString(Qt.locale(), "f", 0))
                            .arg(u.answers)
                      : ""
    }

    StudioButton {
        text: qsTr("Forget the key")
        flat: true
        visible: panel._designerState().hasKey && !panel._designerState().running
        font.pixelSize: 12
        onClicked: panel._designerState().forgetKey()
    }
}
