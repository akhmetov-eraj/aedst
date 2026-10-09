import AedstCore

/// How the user proves who they are, which decides what `LoginView` renders.
public enum SignInMode: Sendable, Equatable {
    /// First-party email + password (`AuthService`). Time-boxed by the server
    /// (`AEDST_AUTH_FIRST_PARTY_SUNSET`, ADR-0001); used while `AEDST_OIDC_ISSUER` is empty.
    case password
    /// Authorization Code + PKCE against the identity provider in the system
    /// browser session (`ASWebAuthenticationSession`). Passkeys, MFA and social
    /// login all happen in the IdP's hosted login, never in this app.
    case browser
}

/// The seam between `Domain`/`Presentation` and `Data` — this is `auth`'s
/// `application` layer boundary (see `apps/server/internal/modules/auth/application`
/// and `docs/content/server/modules.md`'s module-internal-structure). `Presentation` never
/// imports `Data` directly.
public protocol AuthRepository: Sendable {
    /// Which sign-in UI applies. Constant for the lifetime of the repository.
    var signInMode: SignInMode { get }
    /// Email + password sign-in. `.failure(.unsupported)` unless `signInMode == .password`.
    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session>
    /// Interactive sign-in in the system browser session. `.failure(.unsupported)` unless
    /// `signInMode == .browser`; `.failure(.cancelled)` when the user dismisses the sheet.
    func signIn(deviceID: String) async -> Outcome<Session>
    /// Revokes the refresh token server-side (best effort) and always clears
    /// the local session, even if the network call fails.
    func logout() async -> Outcome<Void>
    /// The persisted, still-usable session (Keychain-backed; survives relaunch
    /// and works offline), or `nil` if signed out or the refresh token expired.
    func currentSession() async -> Session?
}

extension AuthRepository {
    public func hasActiveSession() async -> Bool {
        await currentSession() != nil
    }
}
