import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Labelled text input with an inline error line -- QML port of shadcn/ui's
// <Input> + <Label> + form message pattern. Emits `edited` only for user
// edits (not programmatic text changes), which is what ViewModels bind to.
ColumnLayout {
    id: root

    property string label
    property string errorText
    property string helperText
    property bool disabled: false
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property alias validator: field.validator
    property alias inputMethodHints: field.inputMethodHints

    signal edited(string value)
    signal accepted()

    function focusField() {
        field.forceActiveFocus()
    }

    spacing: Theme.spacingXs

    T.Label {
        visible: root.label.length > 0
        text: root.label
        font: Theme.smallFont
        color: Theme.foreground
    }

    T.TextField {
        id: field
        Layout.fillWidth: true
        enabled: !root.disabled
        font: Theme.bodyFont
        color: Theme.foreground
        selectionColor: Theme.primary
        selectedTextColor: Theme.primaryForeground
        placeholderTextColor: Theme.mutedForeground
        onTextEdited: root.edited(text)
        onAccepted: root.accepted()

        Accessible.role: Accessible.EditableText
        Accessible.name: root.label.length > 0 ? root.label : root.placeholderText
        Accessible.description: root.errorText
        Accessible.disabled: !enabled

        background: Rectangle {
            implicitHeight: 40
            radius: Theme.radiusMd
            color: field.enabled ? Theme.background : Theme.muted
            opacity: field.enabled ? 1.0 : 0.6
            border.width: field.activeFocus ? 2 : 1
            border.color: root.errorText.length > 0 ? Theme.destructive
                        : field.activeFocus ? Theme.ring
                        : Theme.input
        }
    }

    T.Label {
        Layout.fillWidth: true
        visible: root.errorText.length > 0
        text: root.errorText
        wrapMode: Text.WordWrap
        font: Theme.captionFont
        color: Theme.destructive
    }

    T.Label {
        Layout.fillWidth: true
        visible: root.errorText.length === 0 && root.helperText.length > 0
        text: root.helperText
        wrapMode: Text.WordWrap
        font: Theme.captionFont
        color: Theme.mutedForeground
    }
}
