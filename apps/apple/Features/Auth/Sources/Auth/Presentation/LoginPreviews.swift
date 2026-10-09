#if DEBUG
import AedstCore
import SwiftUI

/// Repository for Xcode Previews: sign-in is never reached, the screen only renders. Debug builds only.
private struct PreviewAuthRepository: AuthRepository {
    let signInMode: SignInMode

    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session> { .failure(.unsupported) }
    func signIn(deviceID: String) async -> Outcome<Session> { .failure(.unsupported) }
    func logout() async -> Outcome<Void> { .success(()) }
    func currentSession() async -> Session? { nil }
}

private func previewViewModel(_ mode: SignInMode) -> LoginViewModel {
    let repository = PreviewAuthRepository(signInMode: mode)
    return LoginViewModel(
        loginUseCase: LoginUseCase(repository: repository),
        signInUseCase: SignInUseCase(repository: repository),
        signInMode: mode,
        deviceID: { "preview-device" }
    )
}

#Preview("Login (password)") {
    LoginView(viewModel: previewViewModel(.password)) { _ in }
}

#Preview("Login (browser)") {
    LoginView(viewModel: previewViewModel(.browser)) { _ in }
}
#endif
