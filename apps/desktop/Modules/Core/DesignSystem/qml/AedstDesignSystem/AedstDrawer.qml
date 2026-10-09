import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Edge-anchored sliding panel -- QML port of shadcn/ui's <Sheet>. Qt Quick
// Controls has no dedicated "Sheet"/"Drawer" type, so this composes Drawer
// (which already animates in/out from an edge) with the design system's
// chrome (title + close button) and theming.
T.Drawer {
    id: root

    default property alias content: body.data
    property string title
    property bool showCloseButton: true

    edge: Qt.RightEdge
    padding: Theme.spacingLg
    modal: true
    dim: true
    implicitWidth: edge === Qt.LeftEdge || edge === Qt.RightEdge ? 360 : (parent ? parent.width : 360)
    implicitHeight: edge === Qt.TopEdge || edge === Qt.BottomEdge ? 320 : (parent ? parent.height : 320)

    Accessible.role: Accessible.Dialog
    Accessible.name: title

    Overlay.modal: Rectangle {
        color: Theme.scrimLight
    }

    background: Rectangle {
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
            Layout.fillHeight: true
            spacing: Theme.spacingSm
        }
    }
}
