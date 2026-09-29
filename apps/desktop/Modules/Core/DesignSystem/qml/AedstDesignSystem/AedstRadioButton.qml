import QtQuick
import QtQuick.Controls.Basic as T

// QML port of shadcn/ui's <RadioGroupItem>. Typically used inside
// AedstRadioGroup, which wires up the shared T.ButtonGroup.
T.RadioButton {
    id: control

    font: Theme.bodyFont
    hoverEnabled: true
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    spacing: Theme.spacingSm

    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 9
        color: Theme.background
        opacity: control.enabled ? 1.0 : 0.5
        border.width: control.activeFocus ? 2 : 1
        border.color: control.checked ? Theme.primary : Theme.input

        Rectangle {
            anchors.centerIn: parent
            visible: control.checked
            width: 10
            height: 10
            radius: 5
            color: Theme.primary
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
