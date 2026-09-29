import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Filterable command palette -- QML port of shadcn/ui's <Command>. `model`
// is an array of {text, value, group}; typing filters case-insensitively
// on `text`, arrow keys move the highlight, Enter/click emits `activated`.
Rectangle {
    id: root

    property var model: []
    property string placeholderText: qsTr("Type a command or search…")
    property var filtered: root.model.filter((item) => item.text.toLowerCase().indexOf(search.text.toLowerCase()) !== -1)

    signal activated(var value)

    function focusSearch() {
        search.forceActiveFocus()
    }

    implicitWidth: 360
    implicitHeight: 360
    radius: Theme.radiusXl
    color: Theme.popover
    border.width: 1
    border.color: Theme.border
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        T.TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: Theme.spacingSm
            placeholderText: root.placeholderText
            placeholderTextColor: Theme.mutedForeground
            font: Theme.bodyFont
            color: Theme.foreground
            background: Rectangle { color: "transparent" }
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onUpPressed: list.decrementCurrentIndex()
            Keys.onReturnPressed: {
                if (list.currentItem) root.activated(root.filtered[list.currentIndex].value)
            }

            Accessible.role: Accessible.EditableText
            Accessible.name: root.placeholderText
        }

        AedstSeparator { Layout.fillWidth: true }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.filtered
            currentIndex: model.length > 0 ? 0 : -1

            delegate: T.ItemDelegate {
                id: item
                required property var modelData
                required property int index
                width: list.width
                highlighted: list.currentIndex === index

                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.text

                onClicked: root.activated(modelData.value)

                contentItem: Text {
                    text: item.modelData.text
                    font: Theme.bodyFont
                    color: Theme.popoverForeground
                    elide: Text.ElideRight
                    leftPadding: Theme.spacingSm
                }
                background: Rectangle {
                    color: item.highlighted ? Theme.accent : "transparent"
                    radius: Theme.radiusSm
                }
            }

            T.ScrollBar.vertical: T.ScrollBar { }
        }
    }
}
