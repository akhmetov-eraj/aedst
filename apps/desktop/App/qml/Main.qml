import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Connectivity
import Aedst.Navigation
import Aedst.Feature.Auth

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

            Label {
                Layout.fillWidth: true
                text: window.title
                font: Theme.bodyFont
                color: Theme.onSurface
                elide: Text.ElideRight
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
