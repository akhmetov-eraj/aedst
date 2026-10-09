import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Navigation
import Aedst.Feature.Auth
import Aedst.Platform
import Aedst.Telemetry

// App shell: one StackView mirrored from the Core Router's back stack — the
// desktop counterpart of apple's RootView.swift and android's AppNavHost.kt.
// Screens come from each feature's registered destinations; this file only
// knows route names (the `Routes` singleton, never string literals), plus the
// app-level chrome (AppToolbar, sign out, offline banner, update banner, privacy prompt).
//
// Keyboard shortcuts (shown in the toolbar tooltips):
//   Back      Alt+Left, Esc   (when there is a screen to go back to)
//   Sign out  Ctrl+Shift+Q    (not Ctrl+L: that reads as "lock"/"focus address bar"
//                              in other apps; Ctrl+Q already means Quit)
//   Quit      StandardKey.Quit (Ctrl+Q)
//   Retry     StandardKey.Refresh (F5 / Cmd+R): re-checks the connection while offline;
//             LoginView owns the same key on the login screen (retries the sign-in).
// Esc first closes the blocking error banner when one is showing, then goes back; a popup that
// must keep Esc disables `backEscShortcut` while it is open.
ApplicationWindow {
    id: window

    required property string initialRoute
    // aedst::updater::UpdateService (null when updates are off); State: 3 Downloading, 4 ReadyToInstall, 5 Available.
    property var updater: null
    readonly property bool updateBannerVisible: updater !== null && updater.availableVersion !== dismissedVersion && chromeVisible
        && (updater.state === 4 || updater.state === 5 || (updater.mandatory && updater.state === 3))
    property string dismissedVersion

    readonly property bool onLoginRoute: AppRouter.currentRoute === Routes.login
    readonly property bool canGoBack: AppRouter.depth > 1
    // The router is locked on the upgrade screen when the server refuses this version (AppDependencies::onUpgradeRequired):
    // no toolbar, no sections, no way out.
    readonly property bool onUpgradeRoute: AppRouter.currentRoute === Routes.upgradeRequired
    readonly property bool chromeVisible: AppRouter.depth > 0 && !onLoginRoute && !onUpgradeRoute

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
        objectName: "backEscShortcut"
        sequence: "Esc"
        enabled: window.canGoBack && !errorBanner.visible
        onActivated: AppRouter.goBack()
    }

    // The blocking error banner is dismissible from the keyboard without taking focus away from what the user is doing.
    Shortcut {
        objectName: "closeBannerShortcut"
        sequence: "Esc"
        enabled: errorBanner.visible
        onActivated: errorBanner.close()
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
        { route: Routes.tasks, feature: "tasks", text: qsTr("Tasks"), shortcut: "Ctrl+1" },
        { route: Routes.search, feature: "search", text: qsTr("Search"), shortcut: "Ctrl+2" },
        { route: Routes.media, feature: "media", text: qsTr("Files"), shortcut: "Ctrl+3" },
        { route: Routes.notifications, feature: "notifications", text: qsTr("Notifications"), shortcut: "Ctrl+4" },
        { route: Routes.admin, feature: "admin", text: qsTr("Admin"), shortcut: "Ctrl+5", adminOnly: true }
    ]

    // A section is shown iff the server enabled its feature (capability negotiation; the defaults apply until it has
    // answered) and, for Admin, the token carries the admin role. Reads both properties, so bindings re-evaluate.
    function sectionShown(section) {
        return Session.visibleFeatures.indexOf(section.feature) >= 0 && (!section.adminOnly || Session.isAdmin)
    }
    readonly property var shownSections: sections.filter(section => sectionShown(section))

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
                enabled: window.chromeVisible && window.sectionShown(modelData)
                onActivated: window.openSection(modelData.route)
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: window.chromeVisible && AppRouter.currentRoute !== Routes.search
        onActivated: window.openSection(Routes.search)
    }

    header: AppToolbar {
        visible: window.chromeVisible
        sections: window.shownSections
        canGoBack: window.canGoBack
        rtl: window.rtl
        availableWidth: window.width
        backShortcutText: backAltShortcut.nativeText
        signOutShortcutText: signOutShortcut.nativeText
        onSectionRequested: (route) => window.openSection(route)
        onPrivacyRequested: privacyDialog.open()
    }

    // Typed-error feedback: ErrorCenter (Aedst.Telemetry `Errors`) emits one signal per
    // reported failure; transient ones become an auto-dismissing toast, blocking ones a
    // persistent banner. Titles are localized here from the stable error kind.
    function errorTitle(kind) {
        switch (kind) {
        case "unavailable":
        case "offline":
        case "timeout": return qsTr("Connection problem")
        case "unauthenticated": return qsTr("Session expired")
        case "forbidden": return qsTr("Not allowed")
        case "notFound": return qsTr("Not found")
        case "invalidInput": return qsTr("Check your input")
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
        objectName: "errorBanner"
        z: 10
        visible: false
        closable: true
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.spacingMd
        variant: AedstAlert.Variant.Destructive
    }

    UpdateBanner {
        id: updateBanner
        objectName: "updateBanner"
        z: 9
        visible: window.updateBannerVisible
        updater: window.updater
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.spacingMd
        onLaterRequested: (version) => window.dismissedVersion = version
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

    PrivacyDialog { id: privacyDialog }

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
