import QtQuick
import QtQuick.Controls.Basic as T

// Scroll container with a consistently styled, thin overlay scrollbar --
// QML port of shadcn/ui's <ScrollArea>. Wraps QtQuick.Controls' ScrollView
// so children (Flickable, ListView, GridView, or plain Items) get native
// kinetic scrolling with a themed bar instead of the platform default.
T.ScrollView {
    id: root

    default property alias content: inner.data
    /// Name read out for the scrollable region.
    property string accessibleName

    Accessible.role: Accessible.Pane
    Accessible.name: root.accessibleName

    T.ScrollBar.vertical: T.ScrollBar {
        id: vbar
        policy: T.ScrollBar.AsNeeded
        contentItem: Rectangle {
            implicitWidth: 6
            radius: 3
            color: Theme.mutedForeground
            opacity: vbar.active ? 0.6 : 0.25
            Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
        }
    }
    T.ScrollBar.horizontal: T.ScrollBar {
        id: hbar
        policy: T.ScrollBar.AsNeeded
        contentItem: Rectangle {
            implicitHeight: 6
            radius: 3
            color: Theme.mutedForeground
            opacity: hbar.active ? 0.6 : 0.25
            Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
        }
    }

    Item {
        id: inner
        width: root.availableWidth
        height: Math.max(root.availableHeight, childrenRect.height)
    }
}
