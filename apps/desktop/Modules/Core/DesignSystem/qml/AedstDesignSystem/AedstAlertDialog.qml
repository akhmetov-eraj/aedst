import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Modal confirmation dialog -- QML port of shadcn/ui's <AlertDialog>.
// Unlike AedstDialog, this always blocks input (modal) and only exposes
// confirm/cancel actions, matching the web component's narrower contract.
T.Popup {
    id: root

    property string title
    property string description
    property string confirmText: qsTr("Continue")
    property string cancelText: qsTr("Cancel")
    property bool destructive: false

    signal confirmed()
    signal cancelled()

    modal: true
    focus: true
    x: (parent ? parent.width : 0) / 2 - width / 2
    y: (parent ? parent.height : 0) / 2 - height / 2
    implicitWidth: 420
    closePolicy: T.Popup.CloseOnEscape

    Accessible.role: Accessible.Dialog
    Accessible.name: title
    Accessible.description: description

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.popover
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingLg

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            Text {
                Layout.fillWidth: true
                text: root.title
                font: Theme.headingFont
                color: Theme.popoverForeground
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                font: Theme.smallFont
                color: Theme.mutedForeground
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacingSm

            AedstButton {
                text: root.cancelText
                variant: AedstButton.Variant.Outline
                onClicked: { root.cancelled(); root.close() }
            }
            AedstButton {
                text: root.confirmText
                variant: root.destructive ? AedstButton.Variant.Destructive : AedstButton.Variant.Default
                onClicked: { root.confirmed(); root.close() }
            }
        }
    }
}
