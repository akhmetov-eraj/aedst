import QtQuick
import QtQuick.Controls.Basic as T

// Themed tooltip -- QML port of shadcn/ui's <Tooltip>. Attach via
// `T.ToolTip.visible`/`text` on the target normally; this component is for
// call sites that want a standalone, explicitly positioned tooltip.
T.ToolTip {
    id: control

    delay: 500
    timeout: 5000
    font: Theme.captionFont

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.popoverForeground
        wrapMode: Text.WordWrap
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.popover
        border.width: 1
        border.color: Theme.border
    }
}
