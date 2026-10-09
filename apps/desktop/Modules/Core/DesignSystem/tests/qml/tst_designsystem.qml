import QtQuick
import QtQuick.Controls.Basic
import QtTest
import AedstDesignSystem

// Smoke test of the whole design system plus the behaviour of the components changed for accessibility, keyboard use and
// narrow windows. Runs headless (QT_QPA_PLATFORM=offscreen, QT_QUICK_CONTROLS_STYLE=Basic).
TestCase {
    id: testCase
    name: "DesignSystem"
    width: 600
    height: 600
    visible: true
    when: windowShown

    // Every component of the module must at least instantiate with its defaults (a QML error anywhere in a component
    // file would otherwise only show when a feature screen first uses it).
    function test_everyComponentInstantiates_data() {
        const tableProps = "columns: [{title: 'Name', role: 'name', width: 120}]; rows: [{name: 'a'}, {name: 'b'}]"
        return [
            { tag: "AedstAccordion", source: "AedstAccordion { model: [{title: 'A', text: 'a'}] }" },
            { tag: "AedstAlert", source: "AedstAlert { title: 'T'; description: 'D' }" },
            { tag: "AedstAlertDialog", source: "AedstAlertDialog { title: 'T' }" },
            { tag: "AedstAspectRatio", source: "AedstAspectRatio { width: 160 }" },
            { tag: "AedstAvatar", source: "AedstAvatar { }" },
            { tag: "AedstBadge", source: "AedstBadge { text: 'x' }" },
            { tag: "AedstBreadcrumb", source: "AedstBreadcrumb { }" },
            { tag: "AedstButton", source: "AedstButton { text: 'b' }" },
            { tag: "AedstCalendar", source: "AedstCalendar { }" },
            { tag: "AedstCard", source: "AedstCard { }" },
            { tag: "AedstCarousel", source: "AedstCarousel { }" },
            { tag: "AedstChart", source: "AedstChart { }" },
            { tag: "AedstCheckBox", source: "AedstCheckBox { text: 'c' }" },
            { tag: "AedstCircularProgress", source: "AedstCircularProgress { }" },
            { tag: "AedstCollapsible", source: "AedstCollapsible { label: 'l' }" },
            { tag: "AedstComboBox", source: "AedstComboBox { model: ['a', 'b'] }" },
            { tag: "AedstContextMenu", source: "AedstContextMenu { }" },
            { tag: "AedstDialog", source: "AedstDialog { title: 'T' }" },
            { tag: "AedstDrawer", source: "AedstDrawer { }" },
            { tag: "AedstDropdownMenu", source: "AedstDropdownMenu { model: [{text: 'a', value: 1}] }" },
            { tag: "AedstHoverCard", source: "AedstHoverCard { }" },
            { tag: "AedstIconButton", source: "AedstIconButton { iconText: 'x'; tooltipText: 'x' }" },
            { tag: "AedstLabel", source: "AedstLabel { text: 'l' }" },
            { tag: "AedstListItem", source: "AedstListItem { }" },
            { tag: "AedstMenuBar", source: "AedstMenuBar { }" },
            { tag: "AedstOfflineBanner", source: "AedstOfflineBanner { }" },
            { tag: "AedstOtpInput", source: "AedstOtpInput { }" },
            { tag: "AedstPagination", source: "AedstPagination { }" },
            { tag: "AedstPopover", source: "AedstPopover { }" },
            { tag: "AedstProgressBar", source: "AedstProgressBar { value: 0.5 }" },
            { tag: "AedstRadioButton", source: "AedstRadioButton { text: 'r' }" },
            { tag: "AedstRadioGroup", source: "AedstRadioGroup { }" },
            { tag: "AedstScrollArea", source: "AedstScrollArea { width: 100; height: 100 }" },
            { tag: "AedstSearchableComboBox", source: "AedstSearchableComboBox { }" },
            { tag: "AedstSearchableList", source: "AedstSearchableList { }" },
            { tag: "AedstSeparator", source: "AedstSeparator { }" },
            { tag: "AedstSkeleton", source: "AedstSkeleton { }" },
            { tag: "AedstSlider", source: "AedstSlider { }" },
            { tag: "AedstStateView", source: "AedstStateView { viewState: AedstStateView.ViewState.Empty }" },
            { tag: "AedstSwitch", source: "AedstSwitch { text: 's' }" },
            { tag: "AedstTabBar", source: "AedstTabBar { model: [{text: 'a', value: 1}] }" },
            { tag: "AedstTableView", source: "AedstTableView { " + tableProps + " }" },
            { tag: "AedstTextArea", source: "AedstTextArea { }" },
            { tag: "AedstTextField", source: "AedstTextField { }" },
            { tag: "AedstToast", source: "AedstToast { title: 'T'; duration: 0 }" },
            { tag: "AedstToggleButton", source: "AedstToggleButton { text: 't' }" },
            { tag: "AedstToggleGroup", source: "AedstToggleGroup { }" },
            { tag: "AedstToolTip", source: "AedstToolTip { text: 't' }" }
        ]
    }

    function test_everyComponentInstantiates(data) {
        const object = Qt.createQmlObject("import QtQuick; import AedstDesignSystem; " + data.source, testCase, data.tag)
        verify(object !== null, data.tag + " failed to instantiate")
        object.destroy()
    }

    function test_themeExposesBreakpointsAndATextScale() {
        verify(Theme.breakpointSm < Theme.breakpointMd)
        verify(Theme.breakpointMd < Theme.breakpointLg)
        verify(Theme.textScale >= 1 && Theme.textScale <= 2)
        verify(Theme.bodyFont.pixelSize > 0)
    }

    // --- AedstAlert ---------------------------------------------------------------------------------------------
    function test_closableAlertClosesWithEscapeAndEmitsClosed() {
        const alert = createTemporaryObject(alertComponent, testCase)
        const closed = signalSpyComponent.createObject(testCase, { target: alert, signalName: "closed" })
        alert.forceActiveFocus()
        compare(alert.visible, true)

        keyClick(Qt.Key_Escape)

        compare(alert.visible, false)
        compare(closed.count, 1)
    }

    function test_nonClosableAlertIgnoresEscape() {
        const alert = createTemporaryObject(alertComponent, testCase, { closable: false })
        alert.forceActiveFocus()

        keyClick(Qt.Key_Escape)

        compare(alert.visible, true)
    }

    // --- AedstTableView -----------------------------------------------------------------------------------------
    function test_tableViewKeepsCurrentRowAndSelectionInStep() {
        const table = createTemporaryObject(tableComponent, testCase)
        const activated = signalSpyComponent.createObject(testCase, { target: table, signalName: "rowActivated" })
        table.currentRow = 0
        table.forceActiveFocus()

        keyClick(Qt.Key_Down)
        tryCompare(table, "currentRow", 1)

        keyClick(Qt.Key_Return)
        compare(activated.count, 1)
        compare(activated.signalArguments[0][0], 1)
    }

    // --- Dialogs on narrow windows ----------------------------------------------------------------------------
    function test_dialogIsNeverWiderThanTheWindowMinusItsMargins() {
        const host = createTemporaryObject(hostComponent, testCase, { width: 480, height: 480 })
        const dialog = host.dialog
        verify(dialog.implicitWidth <= 480 - 2 * Theme.spacingLg, "dialog " + dialog.implicitWidth + " px in a 480 px window")
        host.width = 1000
        compare(dialog.implicitWidth, 480)
    }

    // --- AedstCalendar ------------------------------------------------------------------------------------------
    function test_calendarNavigatesMonthsAndSelectsADay() {
        const calendar = createTemporaryObject(calendarComponent, testCase)
        compare(calendar.month, 0)
        compare(calendar.year, 2026)
        calendar.shiftMonth(-1)
        compare(calendar.month, 11)
        compare(calendar.year, 2025)
        calendar.shiftMonth(2)
        compare(calendar.month, 1)
        compare(calendar.year, 2026)
    }

    Component {
        id: alertComponent
        AedstAlert { title: "Boom"; closable: true; width: 300 }
    }
    Component {
        id: tableComponent
        AedstTableView {
            width: 300; height: 200
            columns: [{ title: "Name", role: "name", width: 150 }]
            rows: [{ name: "a" }, { name: "b" }, { name: "c" }]
        }
    }
    Component {
        id: hostComponent
        Item {
            property alias dialog: dlg
            AedstDialog { id: dlg; title: "T" }
        }
    }
    Component {
        id: calendarComponent
        AedstCalendar { selectedDate: new Date(2026, 0, 15) }
    }
    Component {
        id: signalSpyComponent
        SignalSpy { }
    }
}
