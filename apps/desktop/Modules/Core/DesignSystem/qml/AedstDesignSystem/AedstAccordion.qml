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
                id: collapsible
                Layout.fillWidth: true
                label: section.modelData.title

                // A Binding (not an `expanded:` expression) so the header click, which assigns
                // `expanded` imperatively, does not permanently break the single-open rule.
                Binding {
                    target: collapsible
                    property: "expanded"
                    value: root.expandedIndex === section.index
                    when: root.single
                }

                onExpandedChanged: {
                    if (!root.single) return
                    if (expanded) root.expandedIndex = section.index
                    else if (root.expandedIndex === section.index) root.expandedIndex = -1
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
