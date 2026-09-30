import QtQuick
import QtQuick.Layouts

// Expand/collapse container with animated height -- QML port of shadcn/ui's
// <Collapsible>. `label` renders the always-visible clickable header;
// default children are the collapsible body.
ColumnLayout {
    id: root

    default property alias content: body.data
    property string label
    property bool expanded: false
    property alias interactive: header.enabled

    spacing: 0

    MouseArea {
        id: header
        Layout.fillWidth: true
        implicitHeight: 36
        height: implicitHeight
        cursorShape: Qt.PointingHandCursor
        activeFocusOnTab: enabled
        onClicked: { root.expanded = !root.expanded; header.forceActiveFocus() }

        Accessible.role: Accessible.Button
        Accessible.name: root.label
        Accessible.description: root.expanded ? qsTr("Expanded") : qsTr("Collapsed")
        Accessible.disabled: !enabled
        Accessible.onPressAction: root.expanded = !root.expanded
        Keys.onSpacePressed: root.expanded = !root.expanded
        Keys.onReturnPressed: root.expanded = !root.expanded
        Keys.onEnterPressed: root.expanded = !root.expanded

        // Keyboard focus indicator (Theme.ring)
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
            color: "transparent"
            border.width: 2
            border.color: Theme.ring
            visible: header.activeFocus
        }

        RowLayout {
            anchors.fill: parent
            spacing: Theme.spacingSm

            Text {
                text: root.expanded ? "▾" : "▸"
                color: Theme.mutedForeground
                font: Theme.smallFont
            }

            Text {
                Layout.fillWidth: true
                text: root.label
                font: Theme.bodyFont
                color: Theme.foreground
                elide: Text.ElideRight
            }
        }
    }

    Item {
        id: clip
        Layout.fillWidth: true
        clip: true
        implicitHeight: root.expanded ? body.implicitHeight : 0

        Behavior on implicitHeight {
            NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.InOutQuad }
        }

        ColumnLayout {
            id: body
            width: clip.width
        }
    }
}
