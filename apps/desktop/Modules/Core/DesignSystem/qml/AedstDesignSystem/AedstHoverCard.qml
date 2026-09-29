import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Rich hover preview -- QML port of shadcn/ui's <HoverCard>. Desktop has
// real pointer hover (unlike mobile/touch), so this is a ToolTip-based
// panel: hover the wrapped `target` Item for `openDelay` ms to reveal it.
Item {
    id: root

    default property alias content: body.data
    property Item target
    property int openDelay: 500
    property int closeDelay: 150

    T.Popup {
        id: card
        parent: root.target ? root.target : root
        y: root.target ? root.target.height + 6 : 0
        modal: false
        focus: false
        closePolicy: T.Popup.NoAutoClose
        padding: Theme.spacingMd

        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.popover
            border.width: 1
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            id: body
            spacing: Theme.spacingSm
        }
    }

    Timer {
        id: openTimer
        interval: root.openDelay
        onTriggered: card.open()
    }
    Timer {
        id: closeTimer
        interval: root.closeDelay
        onTriggered: card.close()
    }

    Connections {
        target: root.target
        enabled: root.target !== null
        function onHoveredChanged() {
            if (!root.target) return
            if (root.target.hovered) {
                closeTimer.stop()
                openTimer.start()
            } else {
                openTimer.stop()
                closeTimer.start()
            }
        }
    }
}
