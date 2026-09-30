import QtQuick
import QtQuick.Controls.Basic as T

// Two-state pressed/unpressed button -- QML port of shadcn/ui's <Toggle>.
T.Button {
    id: control

    checkable: true
    implicitHeight: 36
    implicitWidth: Math.max(label.implicitWidth + Theme.spacingLg * 2, 36)
    font: Theme.bodyFont
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.disabled: !enabled
    Accessible.onToggleAction: control.toggle()
    Accessible.onPressAction: control.toggle()

    Keys.onReturnPressed: { control.toggle(); control.toggled() }
    Keys.onEnterPressed: { control.toggle(); control.toggled() }

    contentItem: Text {
        id: label
        anchors.centerIn: parent
        text: control.text
        font: control.font
        color: control.checked ? Theme.accentForeground : Theme.foreground
        opacity: control.enabled ? 1.0 : 0.5
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: control.checked ? Theme.accent
             : control.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6)
             : "transparent"
        opacity: control.enabled ? 1.0 : 0.5

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
    }
}
