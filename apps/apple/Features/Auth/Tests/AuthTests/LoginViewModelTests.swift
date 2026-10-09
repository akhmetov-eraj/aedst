import AedstAnalytics
import AedstCore
import Testing
@testable import Auth

@Suite("LoginViewModel")
@MainActor
struct LoginViewModelTests {
    private func makeViewModel(
        _ result: Outcome<Session>,
        analytics: AnalyticsClient = NoopAnalyticsClient()
    ) -> (LoginViewModel, SpyAuthRepository) {
        let repository = SpyAuthRepository(loginResult: result)
        let viewModel = LoginViewModel(
            loginUseCase: LoginUseCase(repository: repository),
            signInUseCase: SignInUseCase(repository: repository),
            signInMode: .password,
            analytics: analytics,
            deviceID: { "device-1" }
        )
        return (viewModel, repository)
    }

    @Test("successful submit populates session, clears the password and the error")
    func successfulSubmit() async {
        let (viewModel, _) = makeViewModel(.success(.stub))
        viewModel.email = "a@b.com"
        viewModel.password = "hunter2"

        await viewModel.submit()

        #expect(viewModel.session == .stub)
        #expect(viewModel.errorMessage == nil)
        #expect(viewModel.password.isEmpty)
        #expect(!viewModel.isLoading)
    }

    @Test("wrong credentials show the localized invalid-credentials message, not a raw error")
    func wrongCredentials() async {
        let (viewModel, _) = makeViewModel(.failure(.unauthenticated))
        viewModel.email = "a@b.com"
        viewModel.password = "wrong"

        await viewModel.submit()

        #expect(viewModel.session == nil)
        #expect(viewModel.errorMessage == LoginViewModel.message(for: .unauthenticated))
        #expect(viewModel.errorMessage?.isEmpty == false)
        #expect(viewModel.errorMessage?.hasPrefix("auth") == false, "missing localization key")
    }

    @Test("a network failure never leaks its technical reason into the UI")
    func networkFailureIsHumanized() async {
        let (viewModel, _) = makeViewModel(.failure(.unavailable(reason: "NSURLErrorDomain -1009 secret-host")))
        viewModel.email = "a@b.com"
        viewModel.password = "x"

        await viewModel.submit()

        #expect(viewModel.errorMessage?.contains("NSURLErrorDomain") == false)
        #expect(viewModel.errorMessage?.contains("secret-host") == false)
    }

    @Test("the stable injected device id is what reaches the repository")
    func usesInjectedDeviceID() async {
        let (viewModel, repository) = makeViewModel(.success(.stub))
        viewModel.email = "a@b.com"
        viewModel.password = "x"

        await viewModel.submit()

        #expect(await repository.lastDeviceID == "device-1")
    }

    @Test("an invalid form is not submitted")
    func invalidFormDoesNothing() async {
        let (viewModel, repository) = makeViewModel(.success(.stub))
        viewModel.email = "nope"

        await viewModel.submit()

        #expect(await repository.loginCallCount == 0)
        #expect(viewModel.errorMessage == nil)
    }

    @Test("analytics receive only the PII-free error category")
    func analyticsAreScrubbed() async {
        let recorder = RecordingAnalytics()
        let (viewModel, _) = makeViewModel(.failure(.unavailable(reason: "user@example.com leaked")), analytics: recorder)
        viewModel.email = "a@b.com"
        viewModel.password = "x"

        await viewModel.submit()

        let events = recorder.events
        #expect(events.map(\.name) == ["login_failure"])
        #expect(events.first?.parameters == ["reason": "network"])
    }

    @Test("isFormValid requires an @ in the email and a non-empty password")
    func formValidity() {
        let (viewModel, _) = makeViewModel(.success(.stub))
        #expect(!viewModel.isFormValid)

        viewModel.email = "a@b.com"
        #expect(!viewModel.isFormValid)

        viewModel.password = "hunter2"
        #expect(viewModel.isFormValid)
    }
}

private final class RecordingAnalytics: AnalyticsClient, @unchecked Sendable {
    private let lock = NSLock()
    private var stored: [AnalyticsEvent] = []
    var events: [AnalyticsEvent] { lock.withLock { stored } }

    func track(_ event: AnalyticsEvent) { lock.withLock { stored.append(event) } }
    func setUserID(_ userID: String?) {}
    func recordError(_ error: Error, context: [String: String]) {}
}
