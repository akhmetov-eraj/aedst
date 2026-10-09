import AedstCore
import Testing
@testable import Auth

@Suite("LoginUseCase")
struct LoginUseCaseTests {
    private func input(_ email: String, _ password: String) -> LoginInput {
        LoginInput(credentials: Credentials(email: email, password: password), deviceID: "device-1")
    }

    @Test("rejects an email without @ before hitting the repository")
    func rejectsInvalidEmail() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        let result = await LoginUseCase(repository: repository)(input("not-an-email", "hunter2"))

        #expect(result.error == .invalidInput(fields: ["email"]))
        #expect(await repository.loginCallCount == 0)
    }

    @Test("rejects an empty password before hitting the repository")
    func rejectsEmptyPassword() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        let result = await LoginUseCase(repository: repository)(input("a@b.com", ""))

        #expect(result.error == .invalidInput(fields: ["password"]))
        #expect(await repository.loginCallCount == 0)
    }

    @Test("delegates to the repository once inputs are valid")
    func delegatesToRepository() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        let result = await LoginUseCase(repository: repository)(input("a@b.com", "hunter2"))

        #expect(result.value == .stub)
        #expect(await repository.loginCallCount == 1)
        #expect(await repository.lastDeviceID == "device-1")
    }

    @Test("trims whitespace around the email but never alters the password")
    func normalizesInput() async {
        let repository = SpyAuthRepository(loginResult: .success(.stub))
        _ = await LoginUseCase(repository: repository)(input("  a@b.com \n", " pass word "))

        #expect(await repository.lastCredentials?.email == "a@b.com")
        #expect(await repository.lastCredentials?.password == " pass word ")
    }

    @Test("surfaces a repository failure unchanged")
    func surfacesRepositoryFailure() async {
        let repository = SpyAuthRepository(loginResult: .failure(.unauthenticated))
        let result = await LoginUseCase(repository: repository)(input("a@b.com", "hunter2"))

        #expect(result.error == .unauthenticated)
    }
}

@Suite("RestoreSessionUseCase")
struct RestoreSessionUseCaseTests {
    @Test("returns the persisted session, or nil when signed out")
    func restores() async {
        let signedIn = SpyAuthRepository(loginResult: .failure(.unauthenticated), storedSession: .stub)
        let signedOut = SpyAuthRepository(loginResult: .failure(.unauthenticated))

        guard case .success(let restored) = await RestoreSessionUseCase(repository: signedIn)(NoInput()),
              case .success(let none) = await RestoreSessionUseCase(repository: signedOut)(NoInput()) else {
            Issue.record("restoring a session never fails")
            return
        }
        #expect(restored == .stub)
        #expect(none == nil)
    }
}
