import QtQuick

// Small status pill -- QML port of shadcn/ui's <Badge>.
Rectangle {
    id: root

    enum Variant { Default, Secondary, Destructive, Outline }

    property int variant: AedstBadge.Variant.Default
    property alias text: label.text

    readonly property color bgColor: {
        if (variant === AedstBadge.Variant.Secondary) return Theme.secondary
        if (variant === AedstBadge.Variant.Destructive) return Theme.destructive
        if (variant === AedstBadge.Variant.Outline) return "transparent"
        return Theme.primary
    }
    readonly property color fgColor: {
        if (variant === AedstBadge.Variant.Secondary) return Theme.secondaryForeground
        if (variant === AedstBadge.Variant.Destructive) return Theme.destructiveForeground
        if (variant === AedstBadge.Variant.Outline) return Theme.foreground
        return Theme.primaryForeground
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: label.text

    implicitWidth: label.implicitWidth + Theme.spacingSm * 2
    implicitHeight: label.implicitHeight + 6
    radius: height / 2
    color: bgColor
    border.width: variant === AedstBadge.Variant.Outline ? 1 : 0
    border.color: Theme.border

    Text {
        id: label
        anchors.centerIn: parent
        font: Theme.captionFont
        font.weight: Font.Medium
        color: root.fgColor
    }
}
