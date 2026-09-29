import QtQuick
import QtQuick.Controls.Basic as T

// Linear progress -- QML port of shadcn/ui's <Progress>. `indeterminate`
// renders a looping scan bar instead of a fixed fill, for unknown-duration
// operations.
T.ProgressBar {
    id: control

    implicitWidth: 240
    implicitHeight: 8

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Progress")

    background: Rectangle {
        radius: height / 2
        color: Theme.secondary
    }

    contentItem: Item {
        Rectangle {
            visible: !control.indeterminate
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: Theme.primary
            Behavior on width { NumberAnimation { duration: Theme.durationNormal } }
        }

        Rectangle {
            id: scan
            visible: control.indeterminate
            width: parent.width * 0.3
            height: parent.height
            radius: height / 2
            color: Theme.primary

            SequentialAnimation on x {
                running: control.indeterminate
                loops: Animation.Infinite
                NumberAnimation { from: -scan.width; to: control.width; duration: 1100; easing.type: Easing.InOutQuad }
            }
        }
    }
}
