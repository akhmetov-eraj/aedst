import QtQuick
import QtQuick.Controls.Basic as T

// QML port of shadcn/ui's <Checkbox>. Supports the tri-state ("indeterminate")
// look via `checkState`, same as the web component wrapping Radix's.
T.CheckBox {
    id: control

    tristate: true
    font: Theme.bodyFont
    hoverEnabled: true
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    spacing: Theme.spacingSm

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checkState === Qt.Checked

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 4
        color: control.checkState === Qt.Unchecked ? Theme.background : Theme.primary
        opacity: control.enabled ? 1.0 : 0.5
        border.width: control.checkState === Qt.Unchecked ? 1 : 0
        border.color: control.activeFocus ? Theme.ring : Theme.input

        Text {
            anchors.centerIn: parent
            visible: control.checkState !== Qt.Unchecked
            text: control.checkState === Qt.PartiallyChecked ? "−" : "✓"
            color: control.checkState === Qt.Unchecked ? Theme.foreground : Theme.primaryForeground
            font.pixelSize: 12
            font.bold: true
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.foreground
        opacity: control.enabled ? 1.0 : 0.5
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
