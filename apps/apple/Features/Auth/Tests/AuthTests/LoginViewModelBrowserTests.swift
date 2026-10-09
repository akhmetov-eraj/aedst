import AedstAnalytics
import AedstCore
import Foundation
import Testing
@testable import Auth

@Suite("LoginViewModel (browser sign-in)")
@MainActor
struct LoginViewModelBrowserTests {
    private func makeViewModel(
        signIn: Outcome<Session>,
        mode: SignInMode = .browser,
        analytics: AnalyticsClient = NoopAnalyticsClient()
    ) -> (LoginViewModel, SpyAuthRepository) {
        let repository = SpyAuthRepository(loginResult: .failure(.unsupported), mode: mode, signInResult: signIn)
        let viewModel = LoginViewModel(
            loginUseCase: LoginUseCase(repository: repository),
            signInUseCase: SignInUseCase(repository: repository),
            signInMode: repository.signInMode,
            analytics: analytics,
            deviceID: { "device-1" }
        )
        return (viewModel, repository)
    }

    @Test("success returns the session and passes the stable device id")
    func success() async {
        let (viewModel, repository) = makeViewModel(signIn: .success(.stub))

        let session = await viewModel.signInWithBrowser()

        #expect(session == .stub)
        #expect(viewModel.session == .stub)
        #expect(viewModel.errorMessage == nil)
        #expect(!viewModel.isLoading)
        #expect(await repository.lastSignInDeviceID == "device-1")
    }

    @Test("a dismissed sheet shows no error and records no failure")
    func cancelledIsSilent() async {
        let recorder = BrowserRecordingAnalytics()
        let (viewModel, _) = makeViewModel(signIn: .failure(.cancelled), analytics: recorder)

        let session = await viewModel.signInWithBrowser()

        #expect(session == nil)
        #expect(viewModel.errorMessage == nil)
        #expect(recorder.names == ["login_cancelled"])
    }

    @Test("a protocol failure shows one generic localized sentence, never the technical reason")
    func failureIsGeneric() async {
        let recorder = BrowserRecordingAnalytics()
        let (viewModel, _) = makeViewModel(signIn: .failure(.unauthenticated), analytics: recorder)

        _ = await viewModel.signInWithBrowser()

        #expect(viewModel.errorMessage?.isEmpty == false)
        #expect(viewModel.errorMessage?.hasPrefix("auth") == false, "missing localization key")
        #expect(viewModel.errorMessage != LoginViewModel.message(for: .unauthenticated), "not the wrong-password text")
        #expect(recorder.names == ["login_failure"])
    }

    @Test("a network failure keeps its specific localized text")
    func networkFailure() async {
        let (viewModel, _) = makeViewModel(signIn: .failure(.unavailable(reason: "secret-host")))

        _ = await viewModel.signInWithBrowser()

        #expect(viewModel.errorMessage == AedstError.unavailable(reason: "").errorDescription)
        #expect(viewModel.errorMessage?.contains("secret-host") == false)
    }

    @Test("password mode never starts a browser sign-in")
    func passwordModeIgnoresBrowser() async {
        let (viewModel, repository) = makeViewModel(signIn: .success(.stub), mode: .password)

        let session = await viewModel.signInWithBrowser()

        #expect(session == nil)
        #expect(await repository.signInCallCount == 0)
    }

    @Test("the use case forwards the device id to the repository")
    func useCase() async {
        let repository = SpyAuthRepository(loginResult: .failure(.unsupported), mode: .browser, signInResult: .success(.stub))

        let result = await SignInUseCase(repository: repository)(SignInInput(deviceID: "d-7"))

        #expect(result.value == .stub)
        #expect(await repository.lastSignInDeviceID == "d-7")
    }
}

private final class BrowserRecordingAnalytics: AnalyticsClient, @unchecked Sendable {
    private let lock = NSLock()
    private var stored: [AnalyticsEvent] = []
    var names: [String] { lock.withLock { stored.map(\.name) } }

    func track(_ event: AnalyticsEvent) { lock.withLock { stored.append(event) } }
    func setUserID(_ userID: String?) {}
    func recordError(_ error: Error, context: [String: String]) {}
}
