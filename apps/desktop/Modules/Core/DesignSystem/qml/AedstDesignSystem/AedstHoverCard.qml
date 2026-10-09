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
    /// Name read out for the preview (what it is a preview of).
    property string accessibleName

    // The card is a hover preview: it duplicates information the target already exposes, so only the popup itself
    // is described; the wrapper stays out of the accessibility tree.
    Accessible.role: Accessible.Pane
    Accessible.name: root.accessibleName
    Accessible.ignored: true

    T.Popup {
        id: card
        parent: root.target ? root.target : root
        y: root.target ? root.target.height + 6 : 0
        modal: false
        focus: false
        closePolicy: T.Popup.NoAutoClose
        padding: Theme.spacingMd

        Accessible.role: Accessible.ToolTip
        Accessible.name: root.accessibleName

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

    // Opens for the keyboard too: a control that shows the card on hover shows it while it has visual focus.
    Connections {
        target: root.target
        enabled: root.target !== null
        function onVisualFocusChanged() { root.syncOpen() }
        function onHoveredChanged() { root.syncOpen() }
    }

    function syncOpen() {
        if (!root.target) return
        if (root.target.hovered || root.target.visualFocus) {
            closeTimer.stop()
            openTimer.start()
        } else {
            openTimer.stop()
            closeTimer.start()
        }
    }
}
