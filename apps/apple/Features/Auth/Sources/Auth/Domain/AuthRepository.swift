import AedstCore

/// The seam between `Domain`/`Presentation` and `Data` — this is `auth`'s
/// `application` layer boundary (see `apps/server/internal/modules/auth/application`
/// and `docs/modules.md`'s module-internal-structure). `Presentation` never
/// imports `Data` directly.
public protocol AuthRepository: Sendable {
    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session>
    func logout() async -> Outcome<Void>
    /// `true` once a valid access token exists (Keychain-backed; survives
    /// process relaunch), without making a network call.
    func hasActiveSession() async -> Bool
}
