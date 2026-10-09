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

    // Purely decorative: the owning view announces "Loading"; a placeholder must not be read out line by line.
    Accessible.role: Accessible.Graphic
    Accessible.ignored: true

    Rectangle {
        id: sheen
        width: parent.width * 0.5
        height: parent.height
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.shimmer }
            GradientStop { position: 1.0; color: "transparent" }
        }

        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: -sheen.width; to: root.width; duration: 1300; easing.type: Easing.InOutQuad }
        }
    }
}
