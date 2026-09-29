import QtQuick
import QtQuick.Layouts

// Stack of collapsible sections -- QML port of shadcn/ui's <Accordion>.
// `model` is an array of {title, text}; `single` (default) keeps at most
// one section expanded at a time, matching <Accordion type="single">.
ColumnLayout {
    id: root

    property var model: []
    property bool single: true
    property int expandedIndex: -1

    spacing: Theme.spacingXs

    Repeater {
        model: root.model

        ColumnLayout {
            id: section
            required property var modelData
            required property int index
            Layout.fillWidth: true
            spacing: 0

            AedstCollapsible {
                Layout.fillWidth: true
                label: section.modelData.title
                expanded: root.single ? root.expandedIndex === section.index : section._expanded
                property bool _expanded: false

                onExpandedChanged: {
                    if (root.single) {
                        if (expanded) root.expandedIndex = section.index
                        else if (root.expandedIndex === section.index) root.expandedIndex = -1
                    } else {
                        section._expanded = expanded
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.margins: Theme.spacingSm
                    text: section.modelData.text
                    wrapMode: Text.WordWrap
                    font: Theme.smallFont
                    color: Theme.mutedForeground
                }
            }

            AedstSeparator { Layout.fillWidth: true }
        }
    }
}
