import QtQuick
import QtQuick.Layouts

// Month grid date picker -- QML port of shadcn/ui's <Calendar>. Kept as a
// plain month grid (no native QtQuick.Controls Calendar dependency, which
// was removed from Qt Quick Controls 2) driven by JS Date arithmetic.
Rectangle {
    id: root

    property date selectedDate: new Date()
    property date displayedMonth: new Date(selectedDate.getFullYear(), selectedDate.getMonth(), 1)

    signal dateSelected(date value)

    readonly property var weekDays: [qsTr("Mo"), qsTr("Tu"), qsTr("We"), qsTr("Th"), qsTr("Fr"), qsTr("Sa"), qsTr("Su")]
    readonly property var cells: {
        const first = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth(), 1)
        const startOffset = (first.getDay() + 6) % 7 // Monday-first
        const daysInMonth = new Date(displayedMonth.getFullYear(), displayedMonth.getMonth() + 1, 0).getDate()
        const out = []
        for (let i = 0; i < startOffset; ++i) out.push(null)
        for (let d = 1; d <= daysInMonth; ++d) out.push(d)
        return out
    }

    implicitWidth: 280
    implicitHeight: 300
    radius: Theme.radiusLg
    color: Theme.popover
    border.width: 1
    border.color: Theme.border

    Accessible.role: Accessible.Table

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingSm

        RowLayout {
            Layout.fillWidth: true

            AedstIconButton {
                iconText: "‹"
                variant: AedstIconButton.Variant.Ghost
                implicitWidth: 28
                implicitHeight: 28
                tooltipText: qsTr("Previous month")
                onClicked: root.displayedMonth = new Date(root.displayedMonth.getFullYear(), root.displayedMonth.getMonth() - 1, 1)
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.displayedMonth.toLocaleDateString(Qt.locale(), "MMMM yyyy")
                font: Theme.bodyFont
                font.weight: Font.DemiBold
                color: Theme.popoverForeground
            }

            AedstIconButton {
                iconText: "›"
                variant: AedstIconButton.Variant.Ghost
                implicitWidth: 28
                implicitHeight: 28
                tooltipText: qsTr("Next month")
                onClicked: root.displayedMonth = new Date(root.displayedMonth.getFullYear(), root.displayedMonth.getMonth() + 1, 1)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 7
            rowSpacing: 2
            columnSpacing: 2

            Repeater {
                model: root.weekDays
                Text {
                    required property string modelData
                    Layout.preferredWidth: 32
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    font: Theme.captionFont
                    color: Theme.mutedForeground
                }
            }

            Repeater {
                model: root.cells
                Item {
                    required property var modelData
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32

                    readonly property bool isSelected: modelData !== null
                        && modelData === root.selectedDate.getDate()
                        && root.displayedMonth.getMonth() === root.selectedDate.getMonth()
                        && root.displayedMonth.getFullYear() === root.selectedDate.getFullYear()

                    Rectangle {
                        anchors.fill: parent
                        visible: parent.modelData !== null
                        radius: width / 2
                        color: parent.isSelected ? Theme.primary : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: parent.parent.modelData || ""
                            font: Theme.smallFont
                            color: parent.parent.isSelected ? Theme.primaryForeground : Theme.foreground
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const d = new Date(root.displayedMonth.getFullYear(), root.displayedMonth.getMonth(), parent.parent.modelData)
                                root.selectedDate = d
                                root.dateSelected(d)
                            }
                        }
                    }
                }
            }
        }
    }
}
