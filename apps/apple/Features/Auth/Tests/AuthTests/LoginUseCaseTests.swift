import AedstCore
import Testing
@testable import Auth

@Suite("LoginUseCase")
struct LoginUseCaseTests {
    @Test("rejects an email without @ before hitting the repository")
    func rejectsInvalidEmail() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        let useCase = LoginUseCase(repository: repository)

        let result = await useCase(LoginInput(credentials: Credentials(email: "not-an-email", password: "hunter2"), deviceID: "device-1"))

        #expect(result.error == .validation(fields: ["email"]))
        #expect(await repository.loginCallCount == 0)
    }

    @Test("rejects an empty password before hitting the repository")
    func rejectsEmptyPassword() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        let useCase = LoginUseCase(repository: repository)

        let result = await useCase(LoginInput(credentials: Credentials(email: "a@b.com", password: ""), deviceID: "device-1"))

        #expect(result.error == .validation(fields: ["password"]))
        #expect(await repository.loginCallCount == 0)
    }

    @Test("delegates to the repository once inputs are valid")
    func delegatesToRepository() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        let useCase = LoginUseCase(repository: repository)

        let result = await useCase(LoginInput(credentials: Credentials(email: "a@b.com", password: "hunter2"), deviceID: "device-1"))

        #expect(result.value == .stub)
        #expect(await repository.loginCallCount == 1)
    }

    @Test("surfaces a repository failure unchanged")
    func surfacesRepositoryFailure() async {
        let repository = SpyAuthRepository(loginResult: .failure(.unauthorized))
        let useCase = LoginUseCase(repository: repository)

        let result = await useCase(LoginInput(credentials: Credentials(email: "a@b.com", password: "hunter2"), deviceID: "device-1"))

        #expect(result.error == .unauthorized)
    }
}

private actor SpyAuthRepository: AuthRepository {
    private let loginResult: Outcome<Session>
    private(set) var loginCallCount = 0

    init(loginResult: Outcome<Session>) {
        self.loginResult = loginResult
    }

    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session> {
        loginCallCount += 1
        return loginResult
    }

    func logout() async -> Outcome<Void> { .success(()) }

    func hasActiveSession() async -> Bool { false }
}

extension Session {
    fileprivate static let stub = Session(
        userID: "user-1",
        accessTokenExpiresAt: .distantFuture,
        refreshTokenExpiresAt: .distantFuture
    )
}
