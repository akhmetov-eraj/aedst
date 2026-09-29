import QtQuick
import QtQuick.Layouts

// Inline banner -- QML port of shadcn/ui's <Alert>. `variant` selects the
// semantic tone (info/success/warning/destructive); shadcn's own variant
// prop only has default/destructive, the others are a common extension.
Rectangle {
    id: root

    enum Variant { Info, Success, Warning, Destructive }

    property int variant: AedstAlert.Variant.Info
    property string title
    property string description

    readonly property color accentColor: {
        if (variant === AedstAlert.Variant.Success) return "#16A34A"
        if (variant === AedstAlert.Variant.Warning) return "#D97706"
        if (variant === AedstAlert.Variant.Destructive) return Theme.destructive
        return Theme.primary
    }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
    Accessible.description: description

    implicitWidth: 400
    implicitHeight: layout.implicitHeight + Theme.spacingMd * 2
    radius: Theme.radiusLg
    color: Theme.card
    border.width: 1
    border.color: root.accentColor

    Rectangle {
        width: 3
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        radius: 1.5
        color: root.accentColor
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        anchors.leftMargin: Theme.spacingMd + 4
        spacing: 2

        Text {
            Layout.fillWidth: true
            visible: root.title.length > 0
            text: root.title
            font: Theme.bodyFont
            font.weight: Font.DemiBold
            color: Theme.cardForeground
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
}
