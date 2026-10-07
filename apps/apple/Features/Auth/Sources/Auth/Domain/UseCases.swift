import AedstCore
import Foundation

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
        // Trim what a keyboard/autofill commonly leaves behind; never touch the password.
        let email = input.credentials.email.trimmingCharacters(in: .whitespacesAndNewlines)
        guard email.contains("@") else {
            return .failure(.validation(fields: ["email"]))
        }
        guard !input.credentials.password.isEmpty else {
            return .failure(.validation(fields: ["password"]))
        }
        return await repository.login(
            with: Credentials(email: email, password: input.credentials.password),
            deviceID: input.deviceID
        )
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

/// Restores the persisted session at launch (no network call).
public struct RestoreSessionUseCase: UseCase {
    private let repository: AuthRepository

    public init(repository: AuthRepository) {
        self.repository = repository
    }

    public func callAsFunction(_ input: NoInput) async -> Outcome<Session?> {
        .success(await repository.currentSession())
    }
}
