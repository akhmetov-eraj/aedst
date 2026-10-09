import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Feature.Auth
import Aedst.Feature.Notifications
import Aedst.Navigation
import Aedst.Telemetry

// The shell toolbar. At or above Theme.breakpointMd the sections are buttons in a row; below it they collapse into one
// "Menu" button that opens an AedstDropdownMenu (sections, Privacy, Sign out), so a narrow window never clips or
// scrolls its navigation.
ToolBar {
    id: root

    /// [{route, text, shortcut, feature, adminOnly}] of the sections that are shown right now.
    required property var sections
    required property bool canGoBack
    required property bool rtl
    /// Window width the layout reacts to.
    required property real availableWidth
    /// Native text of the sign-out / back shortcuts, shown in the tooltips.
    property string backShortcutText
    property string signOutShortcutText

    readonly property bool compact: availableWidth < Theme.breakpointMd

    signal sectionRequested(string route)
    signal privacyRequested()

    LayoutMirroring.enabled: root.rtl
    LayoutMirroring.childrenInherit: true

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Main toolbar")

    background: Rectangle { color: Theme.surface }

    function unreadSuffix(section) {
        return section.route === Routes.notifications && NotificationsViewModel.unreadCount > 0
            ? " (" + Math.min(NotificationsViewModel.unreadCount, 99) + ")" : ""
    }

    function menuModel() {
        const items = root.sections.map(s => ({ text: s.text + unreadSuffix(s), value: "section:" + s.route }))
        items.push({ separator: true })
        if (Telemetry.crashReportsAvailable || Telemetry.analyticsAvailable) {
            items.push({ text: qsTr("Privacy"), value: "privacy" })
        }
        items.push({ text: qsTr("Sign out"), value: "signOut", enabled: !LoginViewModel.isLoading })
        return items
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: Theme.spacingXs

        AedstButton {
            id: backButton
            objectName: "backButton"
            visible: root.canGoBack
            variant: AedstButton.Variant.Ghost
            size: AedstButton.Size.Sm
            text: qsTr("Back")
            ToolTip.visible: hovered || visualFocus
            ToolTip.delay: 500
            ToolTip.text: qsTr("Back (%1)").arg(root.backShortcutText)
            onClicked: AppRouter.goBack()
        }

        // Regular and wide windows: one button per section.
        Repeater {
            model: root.compact ? [] : root.sections
            delegate: AedstButton {
                required property var modelData
                variant: AppRouter.currentRoute === modelData.route ? AedstButton.Variant.Secondary
                                                                   : AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                text: modelData.text + root.unreadSuffix(modelData)
                Accessible.name: modelData.route === Routes.notifications
                    ? qsTr("Notifications, %n unread", "", NotificationsViewModel.unreadCount)
                    : modelData.text
                ToolTip.visible: hovered || visualFocus
                ToolTip.delay: 500
                ToolTip.text: modelData.text + " (" + modelData.shortcut + ")"
                onClicked: root.sectionRequested(modelData.route)
            }
        }

        Item { Layout.fillWidth: true }

        // Narrow windows: sections, Privacy and Sign out behind one menu.
        AedstButton {
            id: menuButton
            objectName: "menuButton"
            visible: root.compact
            variant: AedstButton.Variant.Ghost
            size: AedstButton.Size.Sm
            text: qsTr("Menu")
            onClicked: overflowMenu.openBelow(menuButton)

            AedstDropdownMenu {
                id: overflowMenu
                model: root.compact ? root.menuModel() : []
                onTriggered: (value) => {
                    if (value === "privacy") root.privacyRequested()
                    else if (value === "signOut") LoginViewModel.logout()
                    else if (String(value).startsWith("section:")) root.sectionRequested(String(value).substring(8))
                }
            }
        }

        AedstButton {
            visible: !root.compact && (Telemetry.crashReportsAvailable || Telemetry.analyticsAvailable)
            variant: AedstButton.Variant.Ghost
            size: AedstButton.Size.Sm
            text: qsTr("Privacy")
            onClicked: root.privacyRequested()
        }

        AedstButton {
            id: signOutButton
            objectName: "signOutButton"
            visible: !root.compact
            variant: AedstButton.Variant.Ghost
            size: AedstButton.Size.Sm
            text: qsTr("Sign out")
            enabled: !LoginViewModel.isLoading
            ToolTip.visible: hovered || visualFocus
            ToolTip.delay: 500
            ToolTip.text: qsTr("Sign out (%1)").arg(root.signOutShortcutText)
            onClicked: LoginViewModel.logout()
        }
    }
}
