import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Feature.Auth
import Aedst.Connectivity

// Stateless view: renders LoginViewModel's properties and forwards intents.
// Navigation after success is the ViewModel's job (via the Core Router), not this file's.
//
// Password handling: LoginViewModel.password is WRITE-ONLY (it holds the secret in a
// wiping C++ string), so the field never binds its text from the ViewModel; the view
// only clears the field after success / sign-out.
//
// Keyboard: Tab order is document order (email -> password -> Sign in); Enter in the
// email field moves to the password, Enter in the password field signs in, F5
// (StandardKey.Refresh) retries after an error or re-checks the connection.
Page {
    id: root

    /// True while the OS reports no network; sign-in is disabled and a banner is shown.
    readonly property bool offline: Connectivity.isOffline
    readonly property bool canSubmit: LoginViewModel.canSubmit && !offline

    function submit() {
        if (root.canSubmit) {
            LoginViewModel.login()
        }
    }

    /// Single sign-on: the identity provider authenticates the user in the system browser, so the
    /// e-mail/password form is not shown at all.
    readonly property bool useBrowserSignIn: LoginViewModel.browserSignInAvailable
    readonly property bool restoring: LoginViewModel.restoringSession
    readonly property bool canStartBrowserSignIn: useBrowserSignIn && !offline && !LoginViewModel.isLoading

    function startBrowserSignIn() {
        if (root.canStartBrowserSignIn) {
            LoginViewModel.signInWithBrowser()
        }
    }

    function focusEmail() {
        if (!root.useBrowserSignIn) {
            emailField.focusField()
        }
    }

    background: Rectangle { color: Theme.background }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Sign in")

    Connections {
        target: LoginViewModel

        function onLoginFailed(message) {
            passwordField.focusField()
        }

        function onLoginSucceeded(userId) {
            passwordField.text = ""
        }

        function onLoggedOut() {
            passwordField.text = ""
        }
    }

    // Focus the first field once the page is shown (Component.onCompleted can run
    // before the item is attached to a window, hence callLater / activation).
    Component.onCompleted: Qt.callLater(root.focusEmail)
    StackView.onActivated: root.focusEmail()

    Shortcut {
        sequences: [StandardKey.Refresh]
        enabled: root.visible && !LoginViewModel.isLoading
            && (root.offline || LoginViewModel.errorMessage.length > 0)
        onActivated: {
            if (root.offline) {
                Connectivity.refresh()
            } else if (root.useBrowserSignIn) {
                root.startBrowserSignIn()
            } else {
                root.submit()
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.visible && LoginViewModel.waitingForBrowser
        onActivated: LoginViewModel.cancelBrowserSignIn()
    }

    ColumnLayout {
        id: form
        objectName: "loginForm"
        anchors.centerIn: parent
        width: Math.min(root.width - Theme.spacing * 4, 360)
        spacing: Theme.spacing * 2

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Sign in form")

        AedstLabel {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Sign in to AEDST")
            font: Theme.titleFont
            color: Theme.onSurface
        }

        AedstOfflineBanner {
            id: offlineBanner
            objectName: "offlineBanner"
            Layout.fillWidth: true
            visible: root.offline
            onRetryRequested: Connectivity.refresh()
        }

        // A stored session is being restored at startup (refresh token -> access token).
        AedstCircularProgress {
            id: restoringIndicator
            objectName: "restoringIndicator"
            Layout.alignment: Qt.AlignHCenter
            visible: root.restoring
            running: visible
            Accessible.role: Accessible.ProgressBar
            Accessible.name: qsTr("Restoring your session")
        }

        // --- Single sign-on in the system browser ---------------------------------------
        AedstButton {
            id: browserSignInButton
            objectName: "browserSignInButton"
            Layout.fillWidth: true
            visible: root.useBrowserSignIn && !root.restoring
            text: qsTr("Continue in browser")
            busy: LoginViewModel.waitingForBrowser
            enabled: root.canStartBrowserSignIn
            Accessible.description: root.offline ? qsTr("Unavailable while offline")
                : LoginViewModel.waitingForBrowser ? qsTr("Waiting for the browser") : ""
            onClicked: root.startBrowserSignIn()
        }

        AedstLabel {
            id: browserHint
            objectName: "browserHint"
            Layout.fillWidth: true
            visible: LoginViewModel.waitingForBrowser
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Finish signing in in your browser, then return to this window.")
            font: Theme.smallFont
            color: Theme.mutedForeground
        }

        AedstButton {
            id: cancelBrowserButton
            objectName: "cancelBrowserButton"
            Layout.fillWidth: true
            visible: LoginViewModel.waitingForBrowser
            variant: AedstButton.Variant.Outline
            text: qsTr("Cancel")
            onClicked: LoginViewModel.cancelBrowserSignIn()
        }

        AedstLabel {
            id: browserError
            objectName: "browserError"
            Layout.fillWidth: true
            visible: root.useBrowserSignIn && !root.restoring && LoginViewModel.errorMessage.length > 0
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: LoginViewModel.errorMessage
            font: Theme.smallFont
            color: Theme.error
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        // --- First-party e-mail/password form (until the first-party auth sunset) ------------
        AedstTextField {
            id: emailField
            objectName: "emailField"
            visible: !root.useBrowserSignIn && !root.restoring
            Layout.fillWidth: true
            label: qsTr("Email")
            placeholderText: qsTr("you@example.com")
            accessibleDescription: qsTr("Enter your email address")
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            text: LoginViewModel.email
            onEdited: (value) => { LoginViewModel.email = value }
            onAccepted: passwordField.focusField()
        }

        AedstTextField {
            id: passwordField
            objectName: "passwordField"
            visible: !root.useBrowserSignIn && !root.restoring
            Layout.fillWidth: true
            label: qsTr("Password")
            accessibleDescription: qsTr("Enter your password")
            echoMode: TextInput.Password
            errorText: LoginViewModel.errorMessage
            onEdited: (value) => { LoginViewModel.password = value }
            onAccepted: root.submit()
        }

        AedstButton {
            id: signInButton
            objectName: "signInButton"
            visible: !root.useBrowserSignIn && !root.restoring
            Layout.fillWidth: true
            text: qsTr("Sign in")
            busy: LoginViewModel.isLoading
            enabled: root.canSubmit
            Accessible.description: root.offline ? qsTr("Unavailable while offline")
                : LoginViewModel.isLoading ? qsTr("Loading") : ""
            onClicked: root.submit()
        }
    }
}
