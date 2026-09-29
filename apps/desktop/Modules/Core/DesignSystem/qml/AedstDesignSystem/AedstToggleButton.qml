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
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked

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
    }
}
