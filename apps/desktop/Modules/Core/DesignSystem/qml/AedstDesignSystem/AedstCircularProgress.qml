import QtQuick

// Circular busy indicator -- QML port of shadcn/ui's spinner usage
// (Loader2 icon + animate-spin). Canvas-drawn arc so it stays crisp at any
// size without shipping an icon asset.
Item {
    id: root

    property bool running: true
    property color color: Theme.primary
    property real lineWidth: 3

    implicitWidth: 24
    implicitHeight: 24

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Loading")

    Canvas {
        id: canvas
        anchors.fill: parent
        rotation: 0

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cx = width / 2
            const cy = height / 2
            const r = Math.min(width, height) / 2 - root.lineWidth
            ctx.lineWidth = root.lineWidth
            ctx.lineCap = "round"
            ctx.strokeStyle = root.color
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 1.5)
            ctx.stroke()
        }

        RotationAnimation on rotation {
            running: root.running
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 900
        }
    }

    onColorChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
