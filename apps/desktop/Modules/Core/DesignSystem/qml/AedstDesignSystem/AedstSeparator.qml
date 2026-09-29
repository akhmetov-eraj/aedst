import QtQuick

// Thin divider line -- QML port of shadcn/ui's <Separator>. Orientation
// follows Qt.Horizontal/Qt.Vertical so it composes naturally with
// RowLayout/ColumnLayout Layout.fillWidth/fillHeight.
Rectangle {
    id: root

    property int orientation: Qt.Horizontal

    Accessible.role: Accessible.Separator

    implicitWidth: orientation === Qt.Horizontal ? parent ? parent.width : 1 : 1
    implicitHeight: orientation === Qt.Horizontal ? 1 : (parent ? parent.height : 1)
    color: Theme.border
}
