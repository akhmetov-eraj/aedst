import AedstCore
import Testing
@testable import Auth

@Suite("LoginViewModel")
@MainActor
struct LoginViewModelTests {
    @Test("successful submit populates session and clears error")
    func successfulSubmit() async {
        let repository = StubAuthRepository(loginResult: .success(.stub))
        let viewModel = LoginViewModel(loginUseCase: LoginUseCase(repository: repository))
        viewModel.email = "a@b.com"
        viewModel.password = "hunter2"

        await viewModel.submit()

        #expect(viewModel.session == .stub)
        #expect(viewModel.errorMessage == nil)
        #expect(!viewModel.isLoading)
    }

    @Test("failed submit sets errorMessage and leaves session nil")
    func failedSubmit() async {
        let repository = StubAuthRepository(loginResult: .failure(.unauthorized))
        let viewModel = LoginViewModel(loginUseCase: LoginUseCase(repository: repository))
        viewModel.email = "a@b.com"
        viewModel.password = "wrong"

        await viewModel.submit()

        #expect(viewModel.session == nil)
        #expect(viewModel.errorMessage != nil)
    }

    @Test("isFormValid requires an @ in the email and a non-empty password")
    func formValidity() {
        let viewModel = LoginViewModel(loginUseCase: LoginUseCase(repository: StubAuthRepository(loginResult: .success(.stub))))
        #expect(!viewModel.isFormValid)

        viewModel.email = "a@b.com"
        #expect(!viewModel.isFormValid)

        viewModel.password = "hunter2"
        #expect(viewModel.isFormValid)
    }
}

private struct StubAuthRepository: AuthRepository {
    let loginResult: Outcome<Session>

    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session> { loginResult }
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
