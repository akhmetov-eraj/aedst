import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// General-purpose modal -- QML port of shadcn/ui's <Dialog>. Unlike
// AedstAlertDialog, content is free-form (`default property` children)
// with just a title/close affordance provided.
T.Popup {
    id: root

    default property alias content: body.data
    property string title
    property bool showCloseButton: true

    modal: true
    focus: true
    x: (parent ? parent.width : 0) / 2 - width / 2
    y: (parent ? parent.height : 0) / 2 - height / 2
    implicitWidth: 480
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    Accessible.role: Accessible.Dialog
    Accessible.name: title

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.durationNormal; easing.type: Easing.OutQuad }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.popover
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            visible: root.title.length > 0 || root.showCloseButton

            Text {
                Layout.fillWidth: true
                text: root.title
                font: Theme.headingFont
                color: Theme.popoverForeground
                elide: Text.ElideRight
            }

            AedstIconButton {
                visible: root.showCloseButton
                iconText: "✕"
                variant: AedstIconButton.Variant.Ghost
                implicitWidth: 28
                implicitHeight: 28
                tooltipText: qsTr("Close")
                onClicked: root.close()
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: Theme.spacingSm
        }
    }
}
