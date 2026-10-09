import AedstAnalytics
import AedstCore
import Foundation
import Observation

@MainActor
@Observable
public final class LoginViewModel {
    public var email = ""
    public var password = ""
    public private(set) var isLoading = false
    public private(set) var errorMessage: String?
    public private(set) var session: Session?

    /// Decides what the login screen renders; fixed for the lifetime of the repository.
    public let signInMode: SignInMode

    private let loginUseCase: LoginUseCase
    private let signInUseCase: SignInUseCase
    private let analytics: AnalyticsClient
    private let deviceID: @Sendable () -> String
    private let logger = AedstLogger(category: "login")

    /// `deviceID` is injected (the composition root supplies the persisted,
    /// stable per-install identifier) — a view model must not mint a fresh
    /// random one per attempt.
    public init(
        loginUseCase: LoginUseCase,
        signInUseCase: SignInUseCase,
        signInMode: SignInMode,
        analytics: AnalyticsClient = NoopAnalyticsClient(),
        deviceID: @escaping @Sendable () -> String
    ) {
        self.loginUseCase = loginUseCase
        self.signInUseCase = signInUseCase
        self.signInMode = signInMode
        self.analytics = analytics
        self.deviceID = deviceID
    }

    public var isFormValid: Bool {
        email.contains("@") && !password.isEmpty
    }

    /// Returns the new session on success so callers never have to read observable
    /// state outside SwiftUI's tracking scope.
    @discardableResult
    public func submit() async -> Session? {
        guard !isLoading, isFormValid else { return nil }
        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        let input = LoginInput(
            credentials: Credentials(email: email, password: password),
            deviceID: deviceID()
        )

        switch await loginUseCase(input) {
        case .success(let session):
            self.session = session
            password = "" // do not keep the secret around in memory longer than needed
            analytics.track(AnalyticsEvent(name: "login_success"))
            return session
        case .failure(let error):
            errorMessage = Self.message(for: error)
            logger.error("Login failed (\(error.analyticsCategory))")
            analytics.track(AnalyticsEvent(name: "login_failure", parameters: ["reason": error.analyticsCategory]))
            return nil
        }
    }

    /// Browser sign-in (`SignInMode.browser`). A dismissed sheet is not an error: no message,
    /// no failure analytics, `nil` returned.
    @discardableResult
    public func signInWithBrowser() async -> Session? {
        guard signInMode == .browser, !isLoading else { return nil }
        isLoading = true
        errorMessage = nil
        defer { isLoading = false }

        switch await signInUseCase(SignInInput(deviceID: deviceID())) {
        case .success(let session):
            self.session = session
            analytics.track(AnalyticsEvent(name: "login_success"))
            return session
        case .failure(.cancelled):
            analytics.track(AnalyticsEvent(name: "login_cancelled"))
            return nil
        case .failure(let error):
            errorMessage = Self.browserMessage(for: error)
            logger.error("Browser sign-in failed (\(error.analyticsCategory))")
            analytics.track(AnalyticsEvent(name: "login_failure", parameters: ["reason": error.analyticsCategory]))
            return nil
        }
    }

    /// Browser mode has no "wrong password": protocol-level failures (state/nonce mismatch,
    /// rejected code, unreadable response) all read as one generic, localized sentence;
    /// connectivity problems keep their specific text.
    static func browserMessage(for error: AedstError) -> String {
        switch error {
        case .unavailable, .offline, .timeout, .rateLimited, .storage:
            error.errorDescription ?? String(localized: "authBrowserSignInFailed", bundle: .module)
        default:
            String(localized: "authBrowserSignInFailed", bundle: .module)
        }
    }

    /// Human, localized sentence for the login form. A 401 on *login* means
    /// wrong credentials — not "your session ended", which is the generic
    /// `.unauthenticated` text used elsewhere.
    static func message(for error: AedstError) -> String {
        switch error {
        case .unauthenticated:
            String(localized: "authLoginInvalidCredentials", bundle: .module)
        default:
            error.errorDescription ?? String(localized: "authLoginInvalidCredentials", bundle: .module)
        }
    }
}
