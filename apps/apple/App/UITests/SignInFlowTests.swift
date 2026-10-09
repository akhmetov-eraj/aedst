import XCTest

/// User flow over the screens reachable without a backend, launched with `-aedst-ui-testing`
/// (DEBUG builds only; `AppLaunchMode`): in-memory secure store, telemetry off, always signed
/// out, first-party password form (no OIDC configuration is injected in this mode).
///
/// Flow: start screen -> Sign in -> the submit button follows form validity -> a rejected
/// attempt shows an inline error and leaves the user on the form. The app has no stubbed
/// backend yet, so the attempt fails at the network; the assertion is only that *an* error
/// appears (`login.error`) and the screen stays, not which sentence it shows.
///
/// UNVERIFIED: written without Xcode; never run. Test credentials are throw-away dummies.
///
/// XCTest, not Swift Testing: only XCTest can drive `XCUIApplication`.
@MainActor
final class SignInFlowTests: XCTestCase {
    private var app: XCUIApplication!

    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication()
        app.launchArguments += ["-aedst-ui-testing"]
        app.launch()
    }

    func testSubmitFollowsFormValidityAndAFailedAttemptShowsAnError() throws {
        let signIn = app.buttons["start.signIn"]
        XCTAssertTrue(signIn.waitForExistence(timeout: 10))
        signIn.tap()

        let email = app.textFields.firstMatch
        XCTAssertTrue(email.waitForExistence(timeout: 10))
        let password = app.secureTextFields.firstMatch
        XCTAssertTrue(password.exists)
        let submit = app.buttons["login.submit"]
        XCTAssertTrue(submit.waitForExistence(timeout: 5))

        // Empty form: not submittable.
        XCTAssertFalse(submit.isEnabled)

        // An address without "@" keeps it disabled even with a password.
        email.tap()
        email.typeText("not-an-email")
        password.tap()
        password.typeText("dummy-password")
        XCTAssertFalse(submit.isEnabled)

        // A plausible address enables it.
        email.tap()
        email.typeText("@example.test")
        XCTAssertTrue(submit.isEnabled)

        submit.tap()

        XCTAssertTrue(app.staticTexts["login.error"].waitForExistence(timeout: 20))
        XCTAssertTrue(app.secureTextFields.firstMatch.exists, "still on the login form")
    }
}
