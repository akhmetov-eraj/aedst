import AedstCore

/// The seam between `Domain`/`Presentation` and `Data` — this is `auth`'s
/// `application` layer boundary (see `apps/server/internal/modules/auth/application`
/// and `docs/modules.md`'s module-internal-structure). `Presentation` never
/// imports `Data` directly.
public protocol AuthRepository: Sendable {
    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session>
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
