import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Non-modal floating panel anchored to a control -- QML port of
// shadcn/ui's <Popover>. Open it with `popup.open()` from the anchor's
// click handler; position it via x/y or T.Popup's built-in `y: anchor.height`.
T.Popup {
    id: root

    default property alias content: body.data

    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    padding: Theme.spacingMd

    Accessible.role: Accessible.ToolTip

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationFast }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.popover
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        id: body
        spacing: Theme.spacingSm
    }
}
