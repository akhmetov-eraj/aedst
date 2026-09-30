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

    function focusEmail() {
        emailField.focusField()
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
            } else {
                root.submit()
            }
        }
    }

    ColumnLayout {
        id: form
        objectName: "loginForm"
        anchors.centerIn: parent
        width: Math.min(root.width - Theme.spacing * 4, 360)
        spacing: Theme.spacing * 2

        Accessible.role: Accessible.Grouping
        Accessible.name: qsTr("Sign in form")

        Label {
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

        AedstTextField {
            id: emailField
            objectName: "emailField"
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
