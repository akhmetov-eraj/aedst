import AedstDesignSystem
import Foundation
import SwiftUI

public struct LoginView: View {
    private enum Field {
        case email
        case password
    }

    @State private var viewModel: LoginViewModel
    @FocusState private var focus: Field?
    private let onLoginSucceeded: (Session) -> Void

    public init(viewModel: LoginViewModel, onLoginSucceeded: @escaping (Session) -> Void) {
        self._viewModel = State(wrappedValue: viewModel)
        self.onLoginSucceeded = onLoginSucceeded
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.lg) {
            Text(String(localized: "authLoginTitle", bundle: .module))
                .aedstFont(AedstTypography.largeTitle)
                .foregroundStyle(AedstColor.foreground)
                .accessibilityAddTraits(.isHeader)

            switch viewModel.signInMode {
            case .password:
                passwordForm
            case .browser:
                browserSignIn
            }

            Spacer()
        }
        .padding(AedstSpacing.xxl)
        .background(AedstColor.background)
        // Return on the email moves to the password; Return on the password signs in. (No text fields in browser mode.)
        .onSubmit { returnPressed() }
        // Keyboard users (and VoiceOver) start on the first field.
        .defaultFocus($focus, .email)
        // The inline error appears below the button; VoiceOver does not read it on its own.
        .onChange(of: viewModel.errorMessage) { previous, current in
            if let text = AedstAnnouncementPolicy.announcement(previous: previous, current: current) {
                AedstAnnouncer.post(text)
            }
        }
    }

    /// First-party email + password (until the server's first-party sunset).
    @ViewBuilder
    private var passwordForm: some View {
        @Bindable var viewModel = viewModel

        AedstTextField(String(localized: "authLoginEmailLabel", bundle: .module), text: $viewModel.email)
            .focused($focus, equals: .email)
            .textContentType(.username)
            .submitLabel(.next)
            #if os(iOS)
            .keyboardType(.emailAddress)
            .textInputAutocapitalization(.never)
            #endif
            .disableAutocorrection(true)
            .aedstSensitive()

        AedstTextField(
            String(localized: "authLoginPasswordLabel", bundle: .module),
            text: $viewModel.password,
            isSecure: true
        )
        .focused($focus, equals: .password)
        .textContentType(.password)
        .submitLabel(.go)
        .aedstSensitive()

        errorText

        AedstButton(String(localized: "authLoginSubmit", bundle: .module), isLoading: viewModel.isLoading) {
            submit()
        }
        .accessibilityIdentifier("login.submit")
        .disabled(!viewModel.isFormValid)
    }

    /// OIDC: one button; the identity provider's hosted page collects credentials,
    /// passkeys and MFA, so the app renders no credential fields.
    @ViewBuilder
    private var browserSignIn: some View {
        Text(String(localized: "authLoginContinueHint", bundle: .module))
            .aedstFont(AedstTypography.caption)
            .foregroundStyle(AedstColor.mutedForeground)

        errorText

        AedstButton(String(localized: "authLoginContinue", bundle: .module), isLoading: viewModel.isLoading) {
            continueInBrowser()
        }
        .accessibilityHint(Text(String(localized: "authLoginContinueHint", bundle: .module)))
        .accessibilityIdentifier("login.continue")
    }

    @ViewBuilder
    private var errorText: some View {
        if let errorMessage = viewModel.errorMessage {
            Text(errorMessage)
                .aedstFont(AedstTypography.caption)
                .foregroundStyle(AedstColor.destructive)
                .accessibilityIdentifier("login.error")
        }
    }

    private func returnPressed() {
        switch focus {
        case .email:
            focus = .password
        case .password, nil:
            submit()
        }
    }

    private func submit() {
        Task {
            if let session = await viewModel.submit() {
                onLoginSucceeded(session)
            }
        }
    }

    private func continueInBrowser() {
        Task {
            if let session = await viewModel.signInWithBrowser() {
                onLoginSucceeded(session)
            }
        }
    }
}
