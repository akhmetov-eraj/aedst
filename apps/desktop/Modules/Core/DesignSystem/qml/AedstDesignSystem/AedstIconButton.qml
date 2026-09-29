import QtQuick
import QtQuick.Controls.Basic as T

// Square icon-only action control -- QML port of shadcn/ui's <Button
// size="icon">. `iconSource` accepts anything Image.source does (an SVG
// under qrc:/, a themed icon font glyph is passed via `iconText` instead).
T.AbstractButton {
    id: control

    enum Variant { Default, Secondary, Destructive, Outline, Ghost }

    property int variant: AedstIconButton.Variant.Ghost
    property url iconSource
    property string iconText
    property string tooltipText

    readonly property color bgColor: {
        if (variant === AedstIconButton.Variant.Secondary) return Theme.secondary
        if (variant === AedstIconButton.Variant.Destructive) return Theme.destructive
        if (variant === AedstIconButton.Variant.Outline) return Theme.background
        if (variant === AedstIconButton.Variant.Ghost) return "transparent"
        return Theme.primary
    }
    readonly property color fgColor: {
        if (variant === AedstIconButton.Variant.Secondary) return Theme.secondaryForeground
        if (variant === AedstIconButton.Variant.Destructive) return Theme.destructiveForeground
        if (variant === AedstIconButton.Variant.Outline) return Theme.foreground
        if (variant === AedstIconButton.Variant.Ghost) return Theme.foreground
        return Theme.primaryForeground
    }

    implicitWidth: 36
    implicitHeight: 36
    hoverEnabled: true
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

    Accessible.role: Accessible.Button
    Accessible.name: control.tooltipText.length > 0 ? control.tooltipText : control.iconText

    T.ToolTip.visible: control.tooltipText.length > 0 && control.hovered
    T.ToolTip.text: control.tooltipText
    T.ToolTip.delay: 500

    background: Rectangle {
        radius: Theme.radiusMd
        color: control.down ? Qt.darker(control.bgColor === "transparent" ? Theme.accent : control.bgColor, 1.1)
             : control.hovered ? (control.bgColor === "transparent" ? Theme.accent : Qt.lighter(control.bgColor, 1.08))
             : control.bgColor
        opacity: control.enabled ? 1.0 : 0.5
        border.width: control.variant === AedstIconButton.Variant.Outline ? 1 : 0
        border.color: Theme.border
    }

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            visible: control.iconSource.toString().length > 0
            source: control.iconSource
            width: 18
            height: 18
            fillMode: Image.PreserveAspectFit
        }
        Text {
            anchors.centerIn: parent
            visible: control.iconSource.toString().length === 0
            text: control.iconText
            color: control.fgColor
            font: Theme.bodyFont
        }
    }
}
