import QtQuick

// Maintains a width:height ratio for its content -- QML port of
// shadcn/ui's <AspectRatio> (a thin wrapper around Radix's primitive).
// Usage: AedstAspectRatio { ratio: 16/9; Image { anchors.fill: parent } }
Item {
    id: root

    default property alias content: contentHost.data
    property real ratio: 16 / 9

    implicitHeight: implicitWidth / ratio

    Item {
        id: contentHost
        anchors.centerIn: parent
        width: root.ratio >= (root.width / Math.max(root.height, 1))
            ? root.width
            : root.height * root.ratio
        height: root.ratio >= (root.width / Math.max(root.height, 1))
            ? root.width / root.ratio
            : root.height
    }
}
