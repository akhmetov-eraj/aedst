import QtQuick
import QtQuick.Controls.Basic as T

// Application/window menu bar -- QML port of shadcn/ui's <Menubar>.
// `model` is an array of {text, items: [{text, value, separator}]}. Each
// entry becomes a top-level T.Menu (its `title` = the button label); the
// `delegate` below is purely the visual style for the resulting buttons,
// per the standard Qt Quick Controls MenuBar customization pattern.
T.MenuBar {
    id: root

    property var model: []

    Accessible.role: Accessible.MenuBar
    Accessible.name: qsTr("Menu")

    signal triggered(var menuIndex, var value)

    background: Rectangle {
        color: Theme.card
        border.width: 1
        border.color: Theme.border
    }

    delegate: T.MenuBarItem {
        id: menuBarItem

        contentItem: Text {
            text: menuBarItem.text
            font: Theme.bodyFont
            color: Theme.cardForeground
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            implicitHeight: 32
            color: menuBarItem.highlighted ? Theme.accent : "transparent"
            radius: Theme.radiusSm
        }
    }

    Instantiator {
        model: root.model
        onObjectAdded: (index, object) => root.insertMenu(index, object)
        onObjectRemoved: (index, object) => root.removeMenu(object)

        delegate: T.Menu {
            id: topMenu
            required property var modelData
            required property int index
            title: modelData.text

            background: Rectangle {
                implicitWidth: 200
                radius: Theme.radiusMd
                color: Theme.popover
                border.width: 1
                border.color: Theme.border
            }

            Repeater {
                model: topMenu.modelData.items || []

                T.MenuItem {
                    id: subItem
                    required property var modelData
                    text: modelData.separator ? "" : modelData.text
                    height: modelData.separator ? 9 : implicitHeight

                    onTriggered: root.triggered(topMenu.index, subItem.modelData.value)

                    contentItem: Text {
                        visible: !subItem.modelData.separator
                        text: subItem.text
                        font: Theme.bodyFont
                        color: Theme.popoverForeground
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacingSm
                    }

                    background: Rectangle {
                        color: subItem.modelData.separator ? "transparent"
                             : subItem.highlighted ? Theme.accent : "transparent"
                        radius: Theme.radiusSm

                        Rectangle {
                            visible: subItem.modelData.separator === true
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
}
