import AedstCore

public struct LoginInput: Sendable {
    public let credentials: Credentials
    public let deviceID: String

    public init(credentials: Credentials, deviceID: String) {
        self.credentials = credentials
        self.deviceID = deviceID
    }
}

/// One `UseCase` per `application` command — `LoginViewModel` depends on
/// this, not on `AuthRepository` directly, so validation/analytics that
/// wrap the repository call live in one place shared by every call site.
public struct LoginUseCase: UseCase {
    private let repository: AuthRepository

    public init(repository: AuthRepository) {
        self.repository = repository
    }

    public func callAsFunction(_ input: LoginInput) async -> Outcome<Session> {
        guard input.credentials.email.contains("@") else {
            return .failure(.validation(fields: ["email"]))
        }
        guard !input.credentials.password.isEmpty else {
            return .failure(.validation(fields: ["password"]))
        }
        return await repository.login(with: input.credentials, deviceID: input.deviceID)
    }
}

public struct LogoutUseCase: UseCase {
    private let repository: AuthRepository

    public init(repository: AuthRepository) {
        self.repository = repository
    }

    public func callAsFunction(_ input: NoInput) async -> Outcome<Void> {
        await repository.logout()
    }
}
