import QtQuick
import QtQuick.Controls.Basic as T

// Themed data grid -- QML port of shadcn/ui's <Table>. `columns` is an
// array of {title, width, role}; `rows` is an array of plain objects keyed
// by each column's `role`. Wraps Qt Quick's own TableView (via a
// QAbstractTableModel-free array-of-objects adapter) rather than
// reimplementing virtualized scrolling.
Rectangle {
    id: root

    property var columns: []
    property var rows: []
    property int currentRow: -1

    radius: Theme.radiusLg
    color: Theme.card
    border.width: 1
    border.color: Theme.border
    clip: true

    implicitWidth: 480
    implicitHeight: 320

    Accessible.role: Accessible.Table

    Column {
        anchors.fill: parent

        Row {
            id: headerRow
            width: parent.width
            height: 36

            Repeater {
                model: root.columns
                Rectangle {
                    required property var modelData
                    width: modelData.width || (headerRow.width / root.columns.length)
                    height: headerRow.height
                    color: Theme.muted

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSm
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.title
                        font: Theme.smallFont
                        font.weight: Font.DemiBold
                        color: Theme.mutedForeground
                        elide: Text.ElideRight
                    }
                }
            }
        }

        AedstSeparator { width: parent.width }

        T.ScrollView {
            width: parent.width
            height: parent.height - headerRow.height - 1

            ListView {
                model: root.rows
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: rowDelegate
                    required property var modelData
                    required property int index
                    width: headerRow.width
                    height: 36
                    color: root.currentRow === index ? Theme.accent
                         : (index % 2 === 1 ? Theme.background : Theme.card)

                    Accessible.role: Accessible.Row

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.currentRow = rowDelegate.index
                    }

                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: root.columns
                            Item {
                                required property var modelData
                                width: modelData.width || (headerRow.width / root.columns.length)
                                height: rowDelegate.height

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingSm
                                    verticalAlignment: Text.AlignVCenter
                                    text: rowDelegate.modelData[modelData.role] !== undefined
                                        ? String(rowDelegate.modelData[modelData.role]) : ""
                                    font: Theme.smallFont
                                    color: Theme.cardForeground
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
