import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

// Elevated content container -- QML port of shadcn/ui's <Card>. Slots for
// header/content/footer are plain `default property` children; callers lay
// their own ColumnLayout inside, matching the web component's composition
// style (Card > CardHeader/CardContent/CardFooter).
Rectangle {
    id: root

    default property alias content: body.data
    property string title
    property string subtitle
    property bool elevated: true

    implicitWidth: 320
    implicitHeight: layout.implicitHeight + Theme.spacingLg * 2
    radius: Theme.radiusXl
    color: Theme.card
    border.width: 1
    border.color: Theme.border

    Accessible.role: Accessible.Grouping
    Accessible.name: title

    layer.enabled: root.elevated
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.18)
        shadowBlur: 0.4
        shadowVerticalOffset: 2
        shadowHorizontalOffset: 0
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingSm

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: root.title.length > 0 || root.subtitle.length > 0

            Text {
                Layout.fillWidth: true
                visible: root.title.length > 0
                text: root.title
                font: Theme.headingFont
                color: Theme.cardForeground
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
                text: root.subtitle
                font: Theme.smallFont
                color: Theme.mutedForeground
                wrapMode: Text.WordWrap
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
