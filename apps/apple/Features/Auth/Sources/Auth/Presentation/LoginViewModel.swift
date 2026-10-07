import AedstAnalytics
import AedstCore
import Foundation
import Perception

@MainActor
@Perceptible
public final class LoginViewModel {
    public var email = ""
    public var password = ""
    public private(set) var isLoading = false
    public private(set) var errorMessage: String?
    public private(set) var session: Session?

    private let loginUseCase: LoginUseCase
    private let analytics: AnalyticsClient
    private let deviceID: @Sendable () -> String
    private let logger = AedstLogger(category: "login")

    /// `deviceID` is injected (the composition root supplies the persisted,
    /// stable per-install identifier) — a view model must not mint a fresh
    /// random one per attempt.
    public init(
        loginUseCase: LoginUseCase,
        analytics: AnalyticsClient = NoopAnalyticsClient(),
        deviceID: @escaping @Sendable () -> String
    ) {
        self.loginUseCase = loginUseCase
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

    /// Human, localized sentence for the login form. A 401 on *login* means
    /// wrong credentials — not "your session ended", which is the generic
    /// `.unauthorized` text used elsewhere.
    static func message(for error: AedstError) -> String {
        switch error {
        case .unauthorized:
            String(localized: "authLoginInvalidCredentials", bundle: .module)
        default:
            error.errorDescription ?? String(localized: "authLoginInvalidCredentials", bundle: .module)
        }
    }
}
