import QtQuick
import QtQuick.Controls.Basic as T

// QML port of shadcn/ui's <Switch>.
T.Switch {
    id: control

    font: Theme.bodyFont
    hoverEnabled: true
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    spacing: Theme.spacingSm

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked

    indicator: Rectangle {
        implicitWidth: 36
        implicitHeight: 20
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? Theme.primary : Theme.input
        opacity: control.enabled ? 1.0 : 0.5

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            color: Theme.background
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 2 : 2
            Behavior on x { NumberAnimation { duration: Theme.durationFast; easing.type: Easing.InOutQuad } }
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
