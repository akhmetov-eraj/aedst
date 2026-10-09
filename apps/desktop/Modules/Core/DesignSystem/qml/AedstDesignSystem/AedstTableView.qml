import QtQuick
import QtQuick.Controls.Basic as T

// Themed data grid -- QML port of shadcn/ui's <Table>. `columns` is an
// array of {title, width, role}; `rows` is an array of plain objects keyed
// by each column's `role`. Wraps Qt Quick's own ListView (via a
// QAbstractTableModel-free array-of-objects adapter) rather than
// reimplementing virtualized scrolling.
//
// Keyboard: the grid takes Tab focus; Up/Down/Home/End/PageUp/PageDown move the
// selection (ListView key navigation), Enter/Return activates the selected row
// (`rowActivated`). `currentRow` and the list's `currentIndex` are one value.
// Accessibility: header cells are ColumnHeader, value cells are Cell, each row is
// announced as its cell values and reports whether it is selected.
Rectangle {
    id: root

    property var columns: []
    property var rows: []
    property int currentRow: -1
    /// Name read out for the grid itself.
    property string accessibleName

    /// Enter/Return (or a double click) on a row.
    signal rowActivated(int row)

    radius: Theme.radiusLg
    color: Theme.card
    border.width: 1
    border.color: list.activeFocus ? Theme.ring : Theme.border
    clip: true

    implicitWidth: 480
    implicitHeight: 320

    Accessible.role: Accessible.Table
    Accessible.name: root.accessibleName

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

                    Accessible.role: Accessible.ColumnHeader
                    Accessible.name: modelData.title

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
                id: list
                model: root.rows
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                activeFocusOnTab: true
                currentIndex: root.currentRow
                highlightMoveDuration: 0

                // One value: the list moves the selection with the keys, the owner may set currentRow.
                onCurrentIndexChanged: if (list.currentIndex !== root.currentRow) root.currentRow = list.currentIndex

                Keys.onReturnPressed: if (list.currentIndex >= 0) root.rowActivated(list.currentIndex)
                Keys.onEnterPressed: if (list.currentIndex >= 0) root.rowActivated(list.currentIndex)

                delegate: Rectangle {
                    id: rowDelegate
                    required property var modelData
                    required property int index
                    readonly property bool selected: root.currentRow === index
                    width: headerRow.width
                    height: 36
                    color: selected ? Theme.accent : (index % 2 === 1 ? Theme.background : Theme.card)

                    Accessible.role: Accessible.Row
                    Accessible.selectable: true
                    Accessible.selected: rowDelegate.selected
                    // The row is announced as its cell values in column order.
                    Accessible.name: root.columns.map(c => String(rowDelegate.modelData[c.role] !== undefined ? rowDelegate.modelData[c.role] : "")).join(", ")
                    Accessible.onPressAction: root.rowActivated(rowDelegate.index)

                    // Keyboard focus indicator on the selected row while the grid has focus.
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.width: rowDelegate.selected && list.activeFocus ? 2 : 0
                        border.color: Theme.ring
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.currentRow = rowDelegate.index
                            list.forceActiveFocus()
                        }
                        onDoubleClicked: root.rowActivated(rowDelegate.index)
                    }

                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: root.columns
                            Item {
                                required property var modelData
                                width: modelData.width || (headerRow.width / root.columns.length)
                                height: rowDelegate.height

                                Accessible.role: Accessible.Cell
                                Accessible.name: cellText.text

                                Text {
                                    id: cellText
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
