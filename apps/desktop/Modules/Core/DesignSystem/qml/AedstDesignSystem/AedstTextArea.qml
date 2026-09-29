import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Multi-line text input -- QML port of shadcn/ui's <Textarea>. Mirrors
// AedstTextField's label/error/helper composition for visual consistency.
ColumnLayout {
    id: root

    property string label
    property string errorText
    property string helperText
    property bool disabled: false
    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property int rows: 4

    signal edited(string value)

    function focusField() {
        area.forceActiveFocus()
    }

    spacing: Theme.spacingXs

    T.Label {
        visible: root.label.length > 0
        text: root.label
        font: Theme.smallFont
        color: Theme.foreground
    }

    T.ScrollView {
        Layout.fillWidth: true
        implicitHeight: area.font.pixelSize * 1.4 * root.rows + Theme.spacingMd * 2

        T.TextArea {
            id: area
            enabled: !root.disabled
            wrapMode: TextEdit.WordWrap
            font: Theme.bodyFont
            color: Theme.foreground
            selectionColor: Theme.primary
            selectedTextColor: Theme.primaryForeground
            placeholderTextColor: Theme.mutedForeground
            topPadding: Theme.spacingSm
            bottomPadding: Theme.spacingSm
            leftPadding: Theme.spacingSm
            rightPadding: Theme.spacingSm
            onTextChanged: root.edited(text)

            Accessible.role: Accessible.EditableText
            Accessible.name: root.label
            Accessible.multiLine: true
            Accessible.disabled: !enabled

            background: Rectangle {
                radius: Theme.radiusMd
                color: area.enabled ? Theme.background : Theme.muted
                opacity: area.enabled ? 1.0 : 0.6
                border.width: area.activeFocus ? 2 : 1
                border.color: root.errorText.length > 0 ? Theme.destructive
                            : area.activeFocus ? Theme.ring
                            : Theme.input
            }
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
