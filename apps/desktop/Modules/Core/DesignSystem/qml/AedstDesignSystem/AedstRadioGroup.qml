import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// QML port of shadcn/ui's <RadioGroup>. `model` is an array of
// {value, text}; `value` is the selected value (two-way bindable).
ColumnLayout {
    id: root

    property var model: []
    property var value: null
    property int orientation: Qt.Vertical

    spacing: Theme.spacingSm

    Accessible.role: Accessible.Grouping

    T.ButtonGroup {
        id: group
    }

    GridLayout {
        columns: root.orientation === Qt.Vertical ? 1 : root.model.length
        rowSpacing: Theme.spacingSm
        columnSpacing: Theme.spacingLg

        Repeater {
            model: root.model
            delegate: AedstRadioButton {
                required property var modelData
                text: modelData.text
                T.ButtonGroup.group: group
                checked: root.value === modelData.value
                onToggled: if (checked) root.value = modelData.value
            }
        }
    }
}
