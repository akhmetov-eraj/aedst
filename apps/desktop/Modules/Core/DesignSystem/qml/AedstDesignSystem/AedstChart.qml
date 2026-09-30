import QtQuick

// Minimal bar/line chart -- QML port of shadcn/ui's <ChartContainer>
// primitives, deliberately simple (Canvas-drawn, no external charting
// dependency) per this module's scope. `values` is an array of numbers;
// `kind` selects "bar" or "line".
Item {
    id: root

    // Not named `data`: that would shadow Item's default `data` property and break child declarations.
    property var values: []
    property string kind: "bar"
    property color seriesColor: Theme.chart[0]

    implicitWidth: 320
    implicitHeight: 160

    Accessible.role: Accessible.Chart
    Accessible.name: qsTr("%1 chart").arg(kind)

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (root.values.length === 0) return

            const max = Math.max(...root.values, 1)
            const min = Math.min(Math.min(...root.values), 0)
            const range = Math.max(max - min, 1)
            const w = width
            const h = height

            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(0, h - 1)
            ctx.lineTo(w, h - 1)
            ctx.stroke()

            if (root.kind === "line") {
                ctx.strokeStyle = root.seriesColor
                ctx.lineWidth = 2
                ctx.beginPath()
                root.values.forEach((value, i) => {
                    const x = (i / Math.max(root.values.length - 1, 1)) * w
                    const y = h - ((value - min) / range) * h
                    if (i === 0) ctx.moveTo(x, y)
                    else ctx.lineTo(x, y)
                })
                ctx.stroke()
            } else {
                const barWidth = w / root.values.length
                ctx.fillStyle = root.seriesColor
                root.values.forEach((value, i) => {
                    const barHeight = ((value - min) / range) * h
                    ctx.fillRect(i * barWidth + barWidth * 0.15, h - barHeight, barWidth * 0.7, barHeight)
                })
            }
        }
    }

    onValuesChanged: canvas.requestPaint()
    onKindChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
