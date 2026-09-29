import QtQuick
import QtQuick.Controls.Basic as T

// QML port of shadcn/ui's <Slider>.
T.Slider {
    id: control

    implicitWidth: 200
    implicitHeight: 20
    hoverEnabled: true

    Accessible.role: Accessible.Slider
    Accessible.name: control.value.toString()

    Keys.onLeftPressed: control.decrease()
    Keys.onRightPressed: control.increase()

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 6
        width: control.availableWidth
        height: implicitHeight
        radius: 3
        color: Theme.secondary

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Theme.primary
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: 8
        color: Theme.background
        border.width: 2
        border.color: Theme.primary
        scale: control.pressed ? 1.15 : 1.0

        Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
    }
}
