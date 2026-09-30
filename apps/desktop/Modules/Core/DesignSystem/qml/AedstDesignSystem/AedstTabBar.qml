import QtQuick
import QtQuick.Controls.Basic as T

// Segmented tab strip -- QML port of shadcn/ui's <Tabs>/<TabsList> (and, in
// a ToolBar-style top-level layout, its <NavigationMenu>). `model` is an
// array of {text, value}; `currentValue` mirrors the selected tab.
T.TabBar {
    id: root

    property var model: []
    property var currentValue: model.length > 0 ? model[currentIndex] && model[currentIndex].value : null

    implicitHeight: 40

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Tabs")

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.secondary
    }

    Repeater {
        model: root.model

        T.TabButton {
            id: tabButton
            required property var modelData
            required property int index
            text: modelData.text

            Accessible.role: Accessible.PageTab
            Accessible.name: text
            Accessible.selected: root.currentIndex === index
            Accessible.onPressAction: root.currentIndex = index

            Keys.onReturnPressed: root.currentIndex = index
            Keys.onEnterPressed: root.currentIndex = index

            contentItem: Text {
                text: tabButton.text
                font: Theme.smallFont
                font.weight: tabButton.checked ? Font.DemiBold : Font.Normal
                color: tabButton.checked ? Theme.foreground : Theme.mutedForeground
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.radiusMd
                color: tabButton.checked ? Theme.background : "transparent"
                border.width: tabButton.checked ? 1 : 0
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                // Keyboard focus indicator (Theme.ring)
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.ring
                    visible: tabButton.visualFocus
                }
            }
        }
    }
}
