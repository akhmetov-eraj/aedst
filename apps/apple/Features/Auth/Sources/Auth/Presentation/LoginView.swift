import AedstDesignSystem
import Foundation
import Perception
import SwiftUI

public struct LoginView: View {
    @State private var viewModel: LoginViewModel
    private let onLoginSucceeded: (Session) -> Void

    public init(viewModel: LoginViewModel, onLoginSucceeded: @escaping (Session) -> Void) {
        self._viewModel = State(wrappedValue: viewModel)
        self.onLoginSucceeded = onLoginSucceeded
    }

    public var body: some View {
        // `WithPerceptionTracking` + `@Perception.Bindable` are the Observation
        // back-port's view-side halves (needed below iOS 17 / macOS 14; a
        // pass-through above). Every read of `viewModel` must happen inside this closure.
        WithPerceptionTracking {
            @Perception.Bindable var viewModel = viewModel

            VStack(alignment: .leading, spacing: AedstSpacing.lg) {
                Text(String(localized: "authLoginTitle", bundle: .module))
                    .aedstFont(AedstTypography.largeTitle)
                    .foregroundStyle(AedstColor.foreground)
                    .accessibilityAddTraits(.isHeader)

                AedstTextField(String(localized: "authLoginEmailLabel", bundle: .module), text: $viewModel.email)
                    .textContentType(.username)
                    .submitLabel(.next)
                    #if os(iOS)
                    .keyboardType(.emailAddress)
                    .textInputAutocapitalization(.never)
                    #endif
                    .disableAutocorrection(true)

                AedstTextField(
                    String(localized: "authLoginPasswordLabel", bundle: .module),
                    text: $viewModel.password,
                    isSecure: true
                )
                .textContentType(.password)
                .submitLabel(.go)

                if let errorMessage = viewModel.errorMessage {
                    Text(errorMessage)
                        .aedstFont(AedstTypography.caption)
                        .foregroundStyle(AedstColor.destructive)
                        .accessibilityIdentifier("login.error")
                }

                AedstButton(String(localized: "authLoginSubmit", bundle: .module), isLoading: viewModel.isLoading) {
                    submit()
                }
                .disabled(!viewModel.isFormValid)

                Spacer()
            }
            .padding(AedstSpacing.xxl)
            .background(AedstColor.background)
            .onSubmit { submit() }
        }
    }

    private func submit() {
        Task {
            if let session = await viewModel.submit() {
                onLoginSucceeded(session)
            }
        }
    }
}
