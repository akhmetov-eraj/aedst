import QtQuick
import QtTest
import Aedst.Feature.Auth
import Aedst.Connectivity

// UI tests for Presentation/LoginView.qml against fake LoginViewModel / Connectivity
// singletons (see qml_test_main.cpp). No translators are installed, so texts are the
// English source strings.
TestCase {
    id: testCase
    name: "LoginView"
    width: 800
    height: 600
    visible: true
    when: windowShown

    function init() {
        LoginViewModel.reset()
        Connectivity.reset()
    }

    function createView() {
        const component = Qt.createComponent("qrc:/aedst/feature/auth/LoginView.qml")
        compare(component.status, Component.Ready, component.errorString())
        const view = createTemporaryObject(component, testCase, { width: testCase.width, height: testCase.height })
        verify(view !== null, "LoginView created")
        return view
    }

    function fillForm(view) {
        const email = findChild(view, "emailField")
        tryVerify(() => email.inputFocused)
        keyClicks("user@aedst.dev")
        keyClick(Qt.Key_Return) // Enter in email moves to the password field
        const password = findChild(view, "passwordField")
        tryVerify(() => password.inputFocused)
        keyClicks("secret")
    }

    function test_initialFocusIsEmail() {
        const view = createView()
        const email = findChild(view, "emailField")
        const password = findChild(view, "passwordField")
        tryVerify(() => email.inputFocused)
        verify(!password.inputFocused)
    }

    function test_submitDisabledWhileEmpty() {
        const view = createView()
        const button = findChild(view, "signInButton")
        compare(button.enabled, false)

        fillForm(view)

        compare(LoginViewModel.email, "user@aedst.dev")
        tryCompare(button, "enabled", true)
    }

    function test_errorTextIsShown() {
        const view = createView()
        const password = findChild(view, "passwordField")
        compare(password.errorText, "")

        LoginViewModel.errorMessage = "invalid credentials"

        compare(password.errorText, "invalid credentials")
        compare(password.inputItem.Accessible.description, "invalid credentials")
    }

    function test_busyStateDisablesButton() {
        const view = createView()
        const button = findChild(view, "signInButton")
        fillForm(view)
        tryCompare(button, "enabled", true)

        LoginViewModel.isLoading = true

        compare(button.busy, true)
        compare(button.enabled, false)
        compare(button.Accessible.description, "Loading")
    }

    function test_enterInPasswordTriggersLogin() {
        const view = createView()
        fillForm(view)

        keyClick(Qt.Key_Return)

        tryCompare(LoginViewModel, "loginCalls", 1)
        compare(LoginViewModel.lastPassword, "secret")
    }

    function test_passwordIsNeverBoundBackFromViewModel() {
        const view = createView()
        const password = findChild(view, "passwordField")
        fillForm(view)
        compare(password.text, "secret")
        compare(LoginViewModel.password, "") // write-only property
        verify(LoginViewModel.hasPassword)

        LoginViewModel.emitLoginSucceeded()

        compare(password.text, "")
    }

    function test_offlineDisablesSubmitAndShowsBanner() {
        const view = createView()
        const banner = findChild(view, "offlineBanner")
        const button = findChild(view, "signInButton")
        compare(banner.visible, false)
        fillForm(view)
        tryCompare(button, "enabled", true)

        Connectivity.isOffline = true

        compare(banner.visible, true)
        compare(button.enabled, false)
        compare(button.Accessible.description, "Unavailable while offline")

        keyClick(Qt.Key_Return)
        compare(LoginViewModel.loginCalls, 0)

        banner.retryRequested()
        compare(Connectivity.refreshCalls, 1)
    }

    function test_accessibleNamesAndRolesArePresent() {
        const view = createView()
        const email = findChild(view, "emailField")
        const password = findChild(view, "passwordField")
        const button = findChild(view, "signInButton")

        compare(email.inputItem.Accessible.name, "Email")
        compare(email.inputItem.Accessible.role, Accessible.EditableText)
        verify(email.inputItem.Accessible.description.length > 0)

        compare(password.inputItem.Accessible.name, "Password")
        compare(password.inputItem.Accessible.role, Accessible.EditableText)
        verify(password.inputItem.Accessible.description.length > 0)

        compare(button.Accessible.name, "Sign in")
        compare(button.Accessible.role, Accessible.Button)

        compare(view.Accessible.name, "Sign in")
        compare(findChild(view, "loginForm").Accessible.role, Accessible.Grouping)
    }
}
