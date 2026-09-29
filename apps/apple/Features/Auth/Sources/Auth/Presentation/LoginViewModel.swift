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

    private let loginUseCase: LoginUseCase
    private let analytics: AnalyticsClient
    private let deviceID: () -> String

    public init(
        loginUseCase: LoginUseCase,
        analytics: AnalyticsClient = NoopAnalyticsClient(),
        deviceID: @escaping () -> String = { UUID().uuidString }
    ) {
        self.loginUseCase = loginUseCase
        self.analytics = analytics
        self.deviceID = deviceID
    }

    public var isFormValid: Bool {
        email.contains("@") && !password.isEmpty
    }

    public func submit() async {
        guard !isLoading else { return }
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
            analytics.track(AnalyticsEvent(name: "login_success"))
        case .failure(let error):
            errorMessage = error.errorDescription
            analytics.track(AnalyticsEvent(name: "login_failure", parameters: ["reason": error.analyticsCategory]))
        }
    }
}
