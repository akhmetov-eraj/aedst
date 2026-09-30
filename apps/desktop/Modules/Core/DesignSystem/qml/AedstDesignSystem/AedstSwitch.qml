import QtQuick
import QtQuick.Controls.Basic as T

// QML port of shadcn/ui's <Switch>.
T.Switch {
    id: control

    font: Theme.bodyFont
    hoverEnabled: true
    spacing: Theme.spacingSm

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.disabled: !enabled
    Accessible.onToggleAction: control.toggle()
    Accessible.onPressAction: control.toggle()

    Keys.onReturnPressed: { control.toggle(); control.toggled() }
    Keys.onEnterPressed: { control.toggle(); control.toggled() }

    indicator: Rectangle {
        implicitWidth: 36
        implicitHeight: 20
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? Theme.primary : Theme.input
        opacity: control.enabled ? 1.0 : 0.5

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }

        HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
        // Keyboard focus indicator (Theme.ring)
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
            color: "transparent"
            border.width: 2
            border.color: Theme.ring
            visible: control.visualFocus
        }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            color: Theme.background
            anchors.verticalCenter: parent.verticalCenter
            // `mirrored` flips the knob travel under LayoutMirroring (RTL locales).
            x: (control.checked !== control.mirrored) ? parent.width - width - 2 : 2
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
