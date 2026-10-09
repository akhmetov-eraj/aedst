import QtQuick
import QtQuick.Controls.Basic as T

// Themed list row -- the shared replacement for a raw ItemDelegate in feature views
// (scripts/check-qml-components.sh forbids ItemDelegate outside the design system).
// Highlighted and hovered rows use the accent token; the row is announced as a list
// item named after its text unless the delegate sets Accessible.name itself.
// Give the row its own `contentItem` and keep `background` unless a view needs another look.
T.ItemDelegate {
    id: root

    hoverEnabled: true
    padding: Theme.spacingMd

    Accessible.role: Accessible.ListItem
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusMd
        color: root.highlighted || root.hovered ? Theme.accent : Theme.card
        border.color: Theme.border
    }
}
