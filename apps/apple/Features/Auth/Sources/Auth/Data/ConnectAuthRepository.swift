import AedstCore
import Foundation

/// `AuthRepository` implementation: talks to `AuthRemoteDataSource`, and on
/// success writes the token pair through `SessionStore` (Keychain-backed —
/// see `AedstPersistence.KeychainTokenStore`) before ever returning to
/// `Presentation`. This is the only place in the app that writes tokens.
public final class ConnectAuthRepository: AuthRepository {
    private let remote: AuthRemoteDataSource
    private let sessionStore: SessionStore

    public init(remote: AuthRemoteDataSource, sessionStore: SessionStore) {
        self.remote = remote
        self.sessionStore = sessionStore
    }

    public func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session> {
        let result = await remote.login(email: credentials.email, password: credentials.password, deviceID: deviceID)

        switch result {
        case .success(let (userID, tokens)):
            await sessionStore.save(accessToken: tokens.accessToken, refreshToken: tokens.refreshToken)
            let now = Date()
            return .success(
                Session(
                    userID: userID,
                    accessTokenExpiresAt: now.addingTimeInterval(TimeInterval(tokens.accessTokenExpiresInSeconds)),
                    refreshTokenExpiresAt: now.addingTimeInterval(TimeInterval(tokens.refreshTokenExpiresInSeconds))
                )
            )
        case .failure(let error):
            return .failure(error)
        }
    }

    public func logout() async -> Outcome<Void> {
        guard let refreshToken = await sessionStore.currentRefreshToken() else {
            await sessionStore.clearTokens()
            return .success(())
        }

        let result = await remote.logout(refreshToken: refreshToken)
        await sessionStore.clearTokens()
        return result
    }

    public func hasActiveSession() async -> Bool {
        await sessionStore.currentAccessToken() != nil
    }
}
