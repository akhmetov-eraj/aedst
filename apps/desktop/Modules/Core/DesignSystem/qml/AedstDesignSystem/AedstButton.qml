import QtQuick
import QtQuick.Controls.Basic as T

// Primary action control -- QML/Qt Quick Controls port of shadcn/ui's
// <Button>. `variant` and `size` are the same axes as the web component's
// CVA config; `busy` swaps the label for a spinner without changing the
// button's size, so layouts don't jump while a request is in flight.
T.Button {
    id: control

    enum Variant { Default, Secondary, Destructive, Outline, Ghost, Link }
    enum Size { Default, Sm, Lg, Icon }

    property int variant: AedstButton.Variant.Default
    property int size: AedstButton.Size.Default
    property bool busy: false

    readonly property bool isOutlineLike: variant === AedstButton.Variant.Outline
        || variant === AedstButton.Variant.Ghost
    readonly property color bgColor: {
        if (variant === AedstButton.Variant.Secondary) return Theme.secondary
        if (variant === AedstButton.Variant.Destructive) return Theme.destructive
        if (variant === AedstButton.Variant.Outline) return Theme.background
        if (variant === AedstButton.Variant.Ghost) return "transparent"
        if (variant === AedstButton.Variant.Link) return "transparent"
        return Theme.primary
    }
    readonly property color fgColor: {
        if (variant === AedstButton.Variant.Secondary) return Theme.secondaryForeground
        if (variant === AedstButton.Variant.Destructive) return Theme.destructiveForeground
        if (variant === AedstButton.Variant.Outline) return Theme.foreground
        if (variant === AedstButton.Variant.Ghost) return Theme.foreground
        if (variant === AedstButton.Variant.Link) return Theme.primary
        return Theme.primaryForeground
    }
    readonly property int implicitHeightForSize: {
        if (size === AedstButton.Size.Sm) return 32
        if (size === AedstButton.Size.Lg) return 44
        if (size === AedstButton.Size.Icon) return 40
        return 40
    }

    enabled: !busy
    implicitHeight: implicitHeightForSize
    implicitWidth: size === AedstButton.Size.Icon
        ? implicitHeightForSize
        : Math.max(label.implicitWidth + Theme.spacingLg * 2, 64)
    font: Theme.bodyFont
    hoverEnabled: true
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: busy ? qsTr("Loading") : ""
    Accessible.ignored: false
    Accessible.disabled: !enabled

    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()

    contentItem: Item {
        implicitWidth: label.implicitWidth
        implicitHeight: label.implicitHeight

        Text {
            id: label
            anchors.centerIn: parent
            visible: !control.busy
            text: control.text
            font: control.font
            color: control.fgColor
            opacity: control.enabled || control.busy ? 1.0 : 0.5
            elide: Text.ElideRight
        }

        T.BusyIndicator {
            anchors.centerIn: parent
            visible: control.busy
            running: control.busy
            implicitWidth: 20
            implicitHeight: 20
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: control.down ? Qt.darker(control.bgColor, control.isOutlineLike ? 1.0 : 1.15)
             : control.hovered ? Qt.lighter(control.bgColor, control.isOutlineLike ? 1.0 : 1.08)
             : control.bgColor
        opacity: control.enabled ? 1.0 : 0.5
        border.width: control.variant === AedstButton.Variant.Outline ? 1 : 0
        border.color: Theme.border

        // Ghost/link hover affordance, since their base color is transparent.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: (control.variant === AedstButton.Variant.Ghost) && control.hovered
            color: Theme.accent
        }

        Rectangle {
            visible: control.variant === AedstButton.Variant.Link && (control.hovered || control.activeFocus)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: label.bottom
            anchors.bottomMargin: -2
            anchors.leftMargin: (parent.width - label.width) / 2
            anchors.rightMargin: (parent.width - label.width) / 2
            height: 1
            color: control.fgColor
        }
    }
}
