import QtQuick
import QtQuick.Controls.Basic as T

// Filterable single-select dropdown -- QML port of shadcn/ui's Combobox
// pattern (<Popover> + <Command>). `model` is an array of {text, value}.
T.ComboBox {
    id: control

    editable: true
    implicitHeight: 40
    implicitWidth: 220
    font: Theme.bodyFont

    property var fullModel: model
    property string query: editText

    Accessible.role: Accessible.ComboBox
    Accessible.name: control.currentText

    onQueryChanged: {
        if (!control.popup.visible) return
        filterModel.text = control.query
    }

    QtObject {
        id: filterModel
        property string text: ""
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: control.enabled ? Theme.background : Theme.muted
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.ring : Theme.input
    }

    contentItem: T.TextField {
        text: control.editText
        font: control.font
        color: Theme.foreground
        placeholderText: qsTr("Search…")
        placeholderTextColor: Theme.mutedForeground
        leftPadding: Theme.spacingSm
        rightPadding: Theme.spacingLg
        verticalAlignment: Text.AlignVCenter
        background: Item {}
        onTextEdited: control.editText = text
    }

    indicator: Text {
        x: control.width - width - Theme.spacingSm
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.mutedForeground
    }

    delegate: T.ItemDelegate {
        id: delegateRoot
        required property var modelData
        required property int index
        width: control.width
        visible: filterModel.text.length === 0
            || (modelData.text || "").toLowerCase().indexOf(filterModel.text.toLowerCase()) !== -1
        height: visible ? implicitHeight : 0
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: delegateRoot.modelData.text !== undefined ? delegateRoot.modelData.text : delegateRoot.modelData
            color: Theme.popoverForeground
            font: Theme.bodyFont
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: delegateRoot.highlighted ? Theme.accent : "transparent"
            radius: Theme.radiusSm
        }
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
