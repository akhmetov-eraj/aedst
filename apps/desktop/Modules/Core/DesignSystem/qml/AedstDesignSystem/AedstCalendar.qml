import QtQuick
import QtQuick.Controls.Basic as T
import QtQuick.Layouts

// Month grid date picker -- QML port of shadcn/ui's <Calendar>. Built on Qt Quick Controls' own MonthGrid and
// DayOfWeekRow (both still part of Qt 6): they take the first day of the week, the weekday names and the digits from
// the locale, so the grid follows the UI language (Monday-first in ru/uz/tg, Saturday-first in ar-SA, Sunday-first in
// en-US) without any date arithmetic written here. Mouse click on a day selects it; the keyboard moves with the
// arrow keys and selects with Enter/Space (the grid is a single tab stop).
Rectangle {
    id: root

    property date selectedDate: new Date()
    /// Shown month, 0-11, and year (what MonthGrid takes). Defaults to the selected date's month.
    property int month: selectedDate.getMonth()
    property int year: selectedDate.getFullYear()
    property var locale: Qt.locale()

    signal dateSelected(date value)

    function shiftMonth(delta) {
        const shown = new Date(root.year, root.month + delta, 1)
        root.month = shown.getMonth()
        root.year = shown.getFullYear()
    }

    function select(day) {
        root.selectedDate = day
        root.dateSelected(day)
    }

    implicitWidth: 280
    implicitHeight: 300
    radius: Theme.radiusLg
    color: Theme.popover
    border.width: 1
    border.color: Theme.border

    Accessible.role: Accessible.Table
    Accessible.name: root.locale.standaloneMonthName(root.month) + " " + root.year

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
                onClicked: root.shiftMonth(-1)
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.locale.standaloneMonthName(root.month) + " " + root.year
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
                onClicked: root.shiftMonth(1)
            }
        }

        T.DayOfWeekRow {
            Layout.fillWidth: true
            locale: root.locale
            font: Theme.captionFont
            delegate: Text {
                required property string shortName
                horizontalAlignment: Text.AlignHCenter
                text: shortName
                font: Theme.captionFont
                color: Theme.mutedForeground
            }
        }

        T.MonthGrid {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            month: root.month
            year: root.year
            locale: root.locale
            font: Theme.smallFont
            activeFocusOnTab: true

            // Keyboard: arrows move the focused day (across month edges), Enter/Space select it.
            property date focusedDate: root.selectedDate
            Keys.onLeftPressed: moveFocus(-1)
            Keys.onRightPressed: moveFocus(1)
            Keys.onUpPressed: moveFocus(-7)
            Keys.onDownPressed: moveFocus(7)
            Keys.onReturnPressed: root.select(focusedDate)
            Keys.onEnterPressed: root.select(focusedDate)
            Keys.onSpacePressed: root.select(focusedDate)
            function moveFocus(days) {
                const next = new Date(focusedDate.getFullYear(), focusedDate.getMonth(), focusedDate.getDate() + days)
                focusedDate = next
                if (next.getMonth() !== root.month || next.getFullYear() !== root.year) {
                    root.month = next.getMonth()
                    root.year = next.getFullYear()
                }
            }

            onClicked: (date) => {
                grid.focusedDate = date
                if (date.getMonth() === root.month) {
                    root.select(date)
                }
            }

            delegate: Item {
                id: dayCell
                required property var model
                readonly property bool inShownMonth: model.month === root.month
                readonly property bool isSelected: model.year === root.selectedDate.getFullYear()
                    && model.month === root.selectedDate.getMonth()
                    && model.day === root.selectedDate.getDate()
                readonly property bool isFocused: grid.activeFocus && model.year === grid.focusedDate.getFullYear()
                    && model.month === grid.focusedDate.getMonth() && model.day === grid.focusedDate.getDate()

                Accessible.role: Accessible.Button
                Accessible.name: dayCell.inShownMonth ? model.date.toLocaleDateString(root.locale, Locale.LongFormat) : ""
                Accessible.ignored: !dayCell.inShownMonth
                Accessible.checkable: true
                Accessible.checked: dayCell.isSelected
                Accessible.onPressAction: root.select(model.date)

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, parent.height)
                    height: width
                    visible: dayCell.inShownMonth
                    radius: width / 2
                    color: dayCell.isSelected ? Theme.primary : "transparent"
                    border.width: dayCell.isFocused ? 2 : 0
                    border.color: Theme.ring

                    Text {
                        anchors.centerIn: parent
                        text: model.day
                        font: Theme.smallFont
                        color: dayCell.isSelected ? Theme.primaryForeground : Theme.foreground
                    }
                }
            }
        }
    }
}
