import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Navigation
import Aedst.Feature.Auth
import Aedst.Feature.Notifications
import Aedst.Platform
import Aedst.Session
import Aedst.Telemetry

// App shell: one StackView mirrored from the Core Router's back stack — the
// desktop counterpart of apple's RootView.swift and android's AppNavHost.kt.
// Screens come from each feature's registered destinations; this file only
// knows route names, plus the app-level chrome (toolbar, sign out, offline banner).
//
// Keyboard shortcuts (shown in the toolbar tooltips):
//   Back      Alt+Left, Esc   (when there is a screen to go back to)
//   Sign out  Ctrl+Shift+Q    (not Ctrl+L: that reads as "lock"/"focus address bar"
//                              in other apps; Ctrl+Q already means Quit)
//   Quit      StandardKey.Quit (Ctrl+Q)
//   Retry     StandardKey.Refresh (F5 / Cmd+R): re-checks the connection while offline;
//             LoginView owns the same key on the login screen (retries the sign-in).
// Esc also closes popups first in most controls; if a screen adds a Popup that must
// keep Esc, disable `backEscShortcut` while it is open.
ApplicationWindow {
    id: window

    required property string initialRoute

    readonly property bool onLoginRoute: AppRouter.currentRoute === "auth/login"
    readonly property bool canGoBack: AppRouter.depth > 1
    readonly property bool chromeVisible: AppRouter.depth > 0 && !onLoginRoute

    width: 1024
    height: 720
    minimumWidth: 480
    minimumHeight: 480
    visible: true
    title: qsTr("AEDST")
    color: Theme.background

    // Right-to-left locales (Arabic): the direction is set by
    // aedst::designsystem::TranslationLoader via QGuiApplication::setLayoutDirection.
    // LayoutMirroring is an Item attached property (it does not apply to a Window), so it
    // is enabled on the window's three top-level items (header, stack, footer) below and
    // inherited by everything inside them.
    readonly property bool rtl: Qt.application.layoutDirection === Qt.RightToLeft

    // No Accessible.* here: the attached type is for Items, and Qt already exposes a Window role
    // named after `title`.

    Shortcut {
        id: backAltShortcut
        sequence: "Alt+Left"
        enabled: window.canGoBack
        onActivated: AppRouter.goBack()
    }

    Shortcut {
        id: backEscShortcut
        sequence: "Esc"
        enabled: window.canGoBack
        onActivated: AppRouter.goBack()
    }

    Shortcut {
        id: signOutShortcut
        sequence: "Ctrl+Shift+Q"
        enabled: window.chromeVisible && !LoginViewModel.isLoading
        onActivated: LoginViewModel.logout()
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: window.close()
    }

    Shortcut {
        sequence: StandardKey.Refresh
        enabled: Connectivity.isOffline && !window.onLoginRoute
        onActivated: Connectivity.refresh()
    }

    // Top-level sections. Notifications shows the unread badge (polled by
    // NotificationsViewModel); Admin only exists for tokens with the admin role.
    readonly property var sections: [
        { route: "tasks/list", text: qsTr("Tasks"), shortcut: "Ctrl+1" },
        { route: "search/home", text: qsTr("Search"), shortcut: "Ctrl+2" },
        { route: "media/library", text: qsTr("Files"), shortcut: "Ctrl+3" },
        { route: "notifications/inbox", text: qsTr("Notifications"), shortcut: "Ctrl+4" },
        { route: "admin/home", text: qsTr("Admin"), shortcut: "Ctrl+5", adminOnly: true }
    ]

    function openSection(route) {
        if (AppRouter.currentRoute !== route) {
            AppRouter.replaceAll(route)
        }
    }

    Repeater {
        model: window.sections
        delegate: Item {
            required property var modelData
            Shortcut {
                sequence: modelData.shortcut
                enabled: window.chromeVisible && (!modelData.adminOnly || Session.isAdmin)
                onActivated: window.openSection(modelData.route)
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: window.chromeVisible && AppRouter.currentRoute !== "search/home"
        onActivated: window.openSection("search/home")
    }

    header: ToolBar {
        visible: window.chromeVisible
        LayoutMirroring.enabled: window.rtl
        LayoutMirroring.childrenInherit: true

        Accessible.role: Accessible.ToolBar
        Accessible.name: qsTr("Main toolbar")

        background: Rectangle { color: Theme.surface }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing
            anchors.rightMargin: Theme.spacing
            spacing: Theme.spacingXs

            AedstButton {
                id: backButton
                objectName: "backButton"
                visible: window.canGoBack
                variant: AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                text: qsTr("Back")
                ToolTip.visible: hovered || visualFocus
                ToolTip.delay: 500
                ToolTip.text: qsTr("Back (%1)").arg(backAltShortcut.nativeText)
                onClicked: AppRouter.goBack()
            }

            Repeater {
                model: window.sections
                delegate: AedstButton {
                    required property var modelData
                    visible: !modelData.adminOnly || Session.isAdmin
                    variant: AppRouter.currentRoute === modelData.route ? AedstButton.Variant.Secondary
                                                                       : AedstButton.Variant.Ghost
                    size: AedstButton.Size.Sm
                    text: modelData.route === "notifications/inbox" && NotificationsViewModel.unreadCount > 0
                        ? modelData.text + " (" + Math.min(NotificationsViewModel.unreadCount, 99) + ")"
                        : modelData.text
                    Accessible.name: modelData.route === "notifications/inbox"
                        ? qsTr("Notifications, %n unread", "", NotificationsViewModel.unreadCount)
                        : modelData.text
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.delay: 500
                    ToolTip.text: modelData.text + " (" + modelData.shortcut + ")"
                    onClicked: window.openSection(modelData.route)
                }
            }

            Item { Layout.fillWidth: true }

            AedstButton {
                variant: AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                text: qsTr("Privacy")
                visible: Telemetry.crashReportsAvailable || Telemetry.analyticsAvailable
                onClicked: privacyDialog.open()
            }

            AedstButton {
                id: signOutButton
                objectName: "signOutButton"
                variant: AedstButton.Variant.Ghost
                size: AedstButton.Size.Sm
                text: qsTr("Sign out")
                enabled: !LoginViewModel.isLoading
                ToolTip.visible: hovered || visualFocus
                ToolTip.delay: 500
                ToolTip.text: qsTr("Sign out (%1)").arg(signOutShortcut.nativeText)
                onClicked: LoginViewModel.logout()
            }
        }
    }

    // Typed-error feedback: ErrorCenter (Aedst.Telemetry `Errors`) emits one signal per
    // reported failure; transient ones become an auto-dismissing toast, blocking ones a
    // persistent banner. Titles are localized here from the stable error kind.
    function errorTitle(kind) {
        switch (kind) {
        case "network": return qsTr("Connection problem")
        case "unauthenticated": return qsTr("Session expired")
        case "permission_denied": return qsTr("Not allowed")
        case "not_found": return qsTr("Not found")
        case "invalid_argument": return qsTr("Check your input")
        case "storage": return qsTr("Storage error")
        default: return qsTr("Something went wrong")
        }
    }

    ListModel { id: toasts }

    Connections {
        target: Errors
        function onErrorRaised(kind, message, retryable, blocking) {
            if (blocking) {
                errorBanner.title = window.errorTitle(kind)
                errorBanner.description = message
                errorBanner.visible = true
                return
            }
            if (toasts.count >= 3) {
                toasts.remove(0)
            }
            toasts.append({ toastTitle: window.errorTitle(kind), toastText: message })
        }
    }

    // In-app fallback for platform notifications (no tray / D-Bus available).
    Connections {
        target: Notifications
        function onNotificationRaised(title, body, severity) {
            if (toasts.count >= 3) {
                toasts.remove(0)
            }
            toasts.append({ toastTitle: title, toastText: body })
        }
    }

    AedstAlert {
        id: errorBanner
        z: 10
        visible: false
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.spacingMd
        variant: AedstAlert.Variant.Destructive
        MouseArea { anchors.fill: parent; onClicked: errorBanner.visible = false }
    }

    Column {
        z: 20
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingSm
        LayoutMirroring.enabled: window.rtl

        Repeater {
            model: toasts
            delegate: AedstToast {
                required property int index
                required property string toastTitle
                required property string toastText
                title: toastTitle
                description: toastText
                variant: AedstToast.Variant.Destructive
                onDismissed: toasts.remove(index)
            }
        }
    }

    // One-time privacy prompt (both integrations are opt-in and only offered when
    // configured), reopened from the toolbar's Privacy button.
    AedstDialog {
        id: privacyDialog
        title: qsTr("Privacy")

        ColumnLayout {
            width: 440
            spacing: Theme.spacingMd

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font: Theme.smallFont
                color: Theme.mutedForeground
                text: qsTr("Help improve AEDST. Crash reports include technical details and your account id only. Usage statistics are anonymous until you sign in and never include what you type.")
            }
            AedstSwitch {
                visible: Telemetry.crashReportsAvailable
                text: qsTr("Send crash reports")
                checked: Telemetry.crashReportsEnabled
                onToggled: Telemetry.crashReportsEnabled = checked
            }
            AedstSwitch {
                visible: Telemetry.analyticsAvailable
                text: qsTr("Share usage statistics")
                checked: Telemetry.analyticsEnabled
                onToggled: Telemetry.analyticsEnabled = checked
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                visible: Telemetry.needsDecision
                AedstButton {
                    text: qsTr("No thanks")
                    variant: AedstButton.Variant.Ghost
                    onClicked: { Telemetry.decideAll(false); privacyDialog.close() }
                }
                AedstButton {
                    text: qsTr("Allow")
                    onClicked: { Telemetry.decideAll(true); privacyDialog.close() }
                }
            }
            AedstButton {
                Layout.alignment: Qt.AlignRight
                visible: !Telemetry.needsDecision
                text: qsTr("Done")
                onClicked: privacyDialog.close()
            }
        }
    }

    onChromeVisibleChanged: if (chromeVisible && Telemetry.needsDecision) Qt.callLater(() => privacyDialog.open())

    footer: Item {
        visible: Connectivity.isOffline && !window.onLoginRoute
        LayoutMirroring.enabled: window.rtl
        LayoutMirroring.childrenInherit: true
        implicitHeight: visible ? offlineBanner.implicitHeight + Theme.spacingMd * 2 : 0

        AedstOfflineBanner {
            id: offlineBanner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.spacingMd
            onRetryRequested: Connectivity.refresh()
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        LayoutMirroring.enabled: window.rtl
        LayoutMirroring.childrenInherit: true

        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Current screen")
    }

    Connections {
        target: AppRouter

        function onPushRequested(view, properties) {
            stack.push(view, properties)
        }

        function onPopRequested() {
            stack.pop()
        }

        function onReplaceAllRequested(view, properties) {
            stack.clear()
            stack.push(view, properties)
        }
    }

    Component.onCompleted: AppRouter.replaceAll(initialRoute)
}
