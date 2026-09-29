import QtQuick

// Shimmering placeholder -- QML port of shadcn/ui's <Skeleton> (which is a
// pulsing `animate-pulse` div on web; a moving gradient sweep reads better
// at desktop frame rates).
Rectangle {
    id: root

    radius: Theme.radiusMd
    color: Theme.muted
    clip: true

    implicitWidth: 120
    implicitHeight: 16

    Rectangle {
        id: sheen
        width: parent.width * 0.5
        height: parent.height
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, Theme.dark ? 0.06 : 0.5) }
            GradientStop { position: 1.0; color: "transparent" }
        }

        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: -sheen.width; to: root.width; duration: 1300; easing.type: Easing.InOutQuad }
        }
    }
}
