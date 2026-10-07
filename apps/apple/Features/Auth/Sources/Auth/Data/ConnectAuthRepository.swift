import AedstCore
import Foundation

/// `AuthRepository` implementation: talks to `AuthRemoteDataSource`, and on
/// success writes the token pair through `SessionStore` (Keychain-backed —
/// see `AedstPersistence.KeychainTokenStore`) before ever returning to
/// `Presentation`. This is the only place in the app that writes tokens.
public struct ConnectAuthRepository: AuthRepository {
    private let remote: any AuthRemoteDataSource
    private let sessionStore: any SessionStore
    private let clock: any AedstClock
    private let loginRetryPolicy: RetryPolicy
    private let signposter = AedstSignposter(category: "auth")
    private let logger = AedstLogger(category: "auth")

    /// `loginRetryPolicy` retries only transient network failures (never bad
    /// credentials); two attempts keep the worst-case spinner short.
    public init(
        remote: any AuthRemoteDataSource,
        sessionStore: any SessionStore,
        clock: any AedstClock = SystemClock(),
        loginRetryPolicy: RetryPolicy = RetryPolicy(maxAttempts: 2, baseDelay: 0.5, maxDelay: 2)
    ) {
        self.remote = remote
        self.sessionStore = sessionStore
        self.clock = clock
        self.loginRetryPolicy = loginRetryPolicy
    }

    public func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session> {
        await signposter.measure("login") {
            let remote = self.remote
            let result = await loginRetryPolicy.run(clock: clock) {
                await remote.login(email: credentials.email, password: credentials.password, deviceID: deviceID)
            }

            switch result {
            case .success(let login):
                let tokens = login.tokens.tokenPair(issuedAt: clock.now)
                do {
                    try await sessionStore.save(tokens, userID: login.userID)
                } catch {
                    logger.error("Signed in but the session could not be stored", error: error)
                    return .failure(.storage(reason: String(describing: error)))
                }
                return .success(
                    Session(
                        userID: login.userID,
                        accessTokenExpiresAt: tokens.accessTokenExpiresAt,
                        refreshTokenExpiresAt: tokens.refreshTokenExpiresAt
                    )
                )
            case .failure(let error):
                return .failure(error)
            }
        }
    }

    public func logout() async -> Outcome<Void> {
        guard let refreshToken = await sessionStore.currentRefreshToken() else {
            await sessionStore.clearTokens()
            return .success(())
        }

        let result = await remote.logout(refreshToken: refreshToken)
        // Local sign-out must not depend on the network: clear regardless.
        await sessionStore.clearTokens()
        return result
    }

    public func currentSession() async -> Session? {
        guard let snapshot = await sessionStore.currentSession() else { return nil }
        let session = Session(
            userID: snapshot.userID,
            accessTokenExpiresAt: snapshot.accessTokenExpiresAt,
            refreshTokenExpiresAt: snapshot.refreshTokenExpiresAt
        )
        // An expired *access* token is fine (it refreshes on demand, even after
        // an offline start); an expired *refresh* token means signed out.
        return session.isRefreshTokenExpired(at: clock.now) ? nil : session
    }
}
