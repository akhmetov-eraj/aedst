import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Feature.Auth

// Stateless view: renders LoginViewModel's properties and forwards intents.
// Navigation after success is the ViewModel's job (via the Core Router), not this file's.
Page {
    id: root

    background: Rectangle { color: Theme.background }

    Connections {
        target: LoginViewModel
        function onLoginFailed(message) {
            passwordField.focusField()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(root.width - Theme.spacing * 4, 360)
        spacing: Theme.spacing * 2

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Sign in to AEDST")
            font: Theme.titleFont
            color: Theme.onSurface
        }

        AedstTextField {
            id: emailField
            Layout.fillWidth: true
            label: qsTr("Email")
            placeholderText: qsTr("you@example.com")
            text: LoginViewModel.email
            onEdited: (value) => { LoginViewModel.email = value }
            onAccepted: passwordField.focusField()
            Component.onCompleted: focusField()
        }

        AedstTextField {
            id: passwordField
            Layout.fillWidth: true
            label: qsTr("Password")
            echoMode: TextInput.Password
            text: LoginViewModel.password
            errorText: LoginViewModel.errorMessage
            onEdited: (value) => { LoginViewModel.password = value }
            onAccepted: LoginViewModel.login()
        }

        AedstButton {
            Layout.fillWidth: true
            text: qsTr("Sign in")
            busy: LoginViewModel.isLoading
            enabled: LoginViewModel.canSubmit
            onClicked: LoginViewModel.login()
        }
    }
}
