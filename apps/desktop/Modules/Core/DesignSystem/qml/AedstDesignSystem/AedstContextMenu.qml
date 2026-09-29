import QtQuick
import QtQuick.Controls.Basic as T

// Right-click menu -- QML port of shadcn/ui's <ContextMenu>. Wrap the
// target content: `AedstContextMenu { model: [...]; Rectangle { ... } }`.
// A right-click anywhere inside opens the themed menu at the cursor.
Item {
    id: root

    default property alias content: host.data
    property var model: []

    signal triggered(var value)

    implicitWidth: host.implicitWidth
    implicitHeight: host.implicitHeight

    Item {
        id: host
        anchors.fill: parent
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                menu.x = mouse.x
                menu.y = mouse.y
                menu.open()
            }
        }
    }

    T.Menu {
        id: menu

        background: Rectangle {
            implicitWidth: 200
            radius: Theme.radiusMd
            color: Theme.popover
            border.width: 1
            border.color: Theme.border
        }

        Repeater {
            model: root.model

            T.MenuItem {
                id: menuItem
                required property var modelData
                text: modelData.separator ? "" : modelData.text
                enabled: modelData.enabled !== false
                height: modelData.separator ? 9 : implicitHeight

                Accessible.role: Accessible.MenuItem
                Accessible.name: text

                onTriggered: root.triggered(modelData.value)

                contentItem: Text {
                    visible: !menuItem.modelData.separator
                    text: menuItem.text
                    font: Theme.bodyFont
                    color: menuItem.enabled ? Theme.popoverForeground : Theme.mutedForeground
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.spacingSm
                }

                background: Rectangle {
                    implicitWidth: 200
                    color: menuItem.modelData.separator ? "transparent"
                         : menuItem.highlighted ? Theme.accent : "transparent"
                    radius: Theme.radiusSm

                    Rectangle {
                        visible: menuItem.modelData.separator === true
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - Theme.spacingSm * 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 1
                        color: Theme.border
                    }
                }
            }
        }
    }
}
