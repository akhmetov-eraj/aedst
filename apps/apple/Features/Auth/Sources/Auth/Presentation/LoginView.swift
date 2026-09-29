import AedstDesignSystem
import Foundation
import SwiftUI

public struct LoginView: View {
    @State private var viewModel: LoginViewModel
    private let onLoginSucceeded: (Session) -> Void

    public init(viewModel: LoginViewModel, onLoginSucceeded: @escaping (Session) -> Void) {
        self._viewModel = State(wrappedValue: viewModel)
        self.onLoginSucceeded = onLoginSucceeded
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.md) {
            Text(String(localized: "authLoginTitle", bundle: .module))
                .aedstFont(AedstTypography.largeTitle)
                .foregroundStyle(AedstColor.textPrimary)

            AedstTextField(String(localized: "authLoginEmailLabel", bundle: .module), text: $viewModel.email)
                .textContentType(.username)
                #if os(iOS)
                .textInputAutocapitalization(.never)
                #endif

            AedstTextField(
                String(localized: "authLoginPasswordLabel", bundle: .module),
                text: $viewModel.password,
                isSecure: true
            )
                .textContentType(.password)

            if let errorMessage = viewModel.errorMessage {
                Text(String(format: String(localized: "authLoginError", bundle: .module), errorMessage))
                    .aedstFont(AedstTypography.caption)
                    .foregroundStyle(AedstColor.danger)
            }

            AedstButton(String(localized: "authLoginSubmit", bundle: .module), isLoading: viewModel.isLoading) {
                Task {
                    await viewModel.submit()
                    if let session = viewModel.session {
                        onLoginSucceeded(session)
                    }
                }
            }
            .disabled(!viewModel.isFormValid)

            Spacer()
        }
        .padding(AedstSpacing.lg)
        .background(AedstColor.background)
    }
}
