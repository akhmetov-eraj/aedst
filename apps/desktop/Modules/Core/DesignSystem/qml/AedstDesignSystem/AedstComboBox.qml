import QtQuick
import QtQuick.Controls.Basic as T

// Single-select dropdown -- QML port of shadcn/ui's <Select>. `model` may be
// a plain string list or an array of {text, value}; `currentValue` mirrors
// the selected item's `value` (or its text when the model has no `value`).
T.ComboBox {
    id: control

    property var currentValue: currentIndex >= 0 && model && model.length > 0 && model[currentIndex] && model[currentIndex].value !== undefined
        ? model[currentIndex].value
        : currentText

    textRole: model && model.length > 0 && model[0] && model[0].text !== undefined ? "text" : ""
    valueRole: model && model.length > 0 && model[0] && model[0].value !== undefined ? "value" : ""

    implicitHeight: 40
    implicitWidth: 200
    font: Theme.bodyFont
    hoverEnabled: true

    Accessible.role: Accessible.ComboBox
    Accessible.name: control.currentText

    delegate: T.ItemDelegate {
        width: control.width
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole ? modelData[control.textRole] : modelData
            color: Theme.popoverForeground
            font: Theme.bodyFont
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: highlighted ? Theme.accent : "transparent"
            radius: Theme.radiusSm
        }
    }

    indicator: Text {
        x: control.width - width - Theme.spacingSm
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.mutedForeground
    }

    contentItem: Text {
        leftPadding: Theme.spacingSm
        rightPadding: control.indicator.width + Theme.spacingSm
        text: control.displayText
        font: control.font
        color: Theme.foreground
        opacity: control.enabled ? 1.0 : 0.5
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: control.enabled ? Theme.background : Theme.muted
        border.width: control.activeFocus || control.down ? 2 : 1
        border.color: control.activeFocus || control.down ? Theme.ring : Theme.input
    }

    popup: T.Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 280)
        padding: 4

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.popover
            border.width: 1
            border.color: Theme.border
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            T.ScrollBar.vertical: T.ScrollBar { }
        }
    }
}
