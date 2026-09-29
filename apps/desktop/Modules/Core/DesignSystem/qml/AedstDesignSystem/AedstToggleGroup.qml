import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Single- or multi-select group of AedstToggleButtons -- QML port of
// shadcn/ui's <ToggleGroup>. `model` is an array of {value, text}; `single`
// (default) keeps exactly one item pressed via an exclusive ButtonGroup,
// matching ToggleGroup type="single"; set single=false for type="multiple".
RowLayout {
    id: root

    property var model: []
    property bool single: true
    property var value: single ? "" : []

    spacing: 2

    T.ButtonGroup {
        id: group
        exclusive: root.single
    }

    Repeater {
        model: root.model
        delegate: AedstToggleButton {
            required property var modelData
            text: modelData.text
            T.ButtonGroup.group: group
            checked: root.single
                ? root.value === modelData.value
                : root.value.indexOf(modelData.value) !== -1
            onToggled: {
                if (root.single) {
                    root.value = checked ? modelData.value : ""
                } else {
                    const next = root.value.slice()
                    const idx = next.indexOf(modelData.value)
                    if (checked && idx === -1) next.push(modelData.value)
                    else if (!checked && idx !== -1) next.splice(idx, 1)
                    root.value = next
                }
            }
        }
    }
}
