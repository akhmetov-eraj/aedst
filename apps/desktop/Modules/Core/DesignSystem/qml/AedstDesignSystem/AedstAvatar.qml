import QtQuick
import QtQuick.Effects

// Circular user avatar with initials fallback -- QML port of shadcn/ui's
// <Avatar>. Falls back to `fallbackText` initials whenever `source` is
// empty or fails to load, same as the web component's <AvatarFallback>.
// The image is clipped to a circle via MultiEffect's alpha mask (Qt6's
// replacement for the deprecated OpacityMask/QtGraphicalEffects).
Item {
    id: root

    property url source
    property string fallbackText: "?"
    property int diameter: 40

    implicitWidth: diameter
    implicitHeight: diameter

    Accessible.role: Accessible.Graphic
    Accessible.name: fallbackText

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.muted
        border.width: 1
        border.color: Theme.border

        Text {
            anchors.centerIn: parent
            visible: image.status !== Image.Ready
            text: root.fallbackText
            color: Theme.mutedForeground
            font.pixelSize: root.diameter * 0.4
            font.weight: Font.DemiBold
        }
    }

    Image {
        id: image
        anchors.fill: parent
        visible: false
        asynchronous: true
        source: root.source
        fillMode: Image.PreserveAspectCrop
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: width / 2
        }
    }

    MultiEffect {
        anchors.fill: image
        visible: image.status === Image.Ready
        source: image
        maskEnabled: true
        maskSource: mask
    }
}
