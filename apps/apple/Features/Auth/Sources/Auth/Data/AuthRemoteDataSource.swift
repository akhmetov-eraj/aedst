import AedstCore

/// Wraps the generated `AuthService` Connect client behind this feature's
/// own protocol, so `ConnectAuthRepository` (and its tests) never touch a
/// generated type directly. `TokenPairDTO` is this file's translation of
/// `aedst.auth.v1.TokenPair` (see `apps/server/api/proto/aedst/auth/v1/auth.proto`).
public protocol AuthRemoteDataSource: Sendable {
    func login(email: String, password: String, deviceID: String) async -> Outcome<(userID: String, tokens: TokenPairDTO)>
    func logout(refreshToken: String) async -> Outcome<Void>
}

public struct TokenPairDTO: Sendable {
    public let accessToken: String
    public let refreshToken: String
    public let accessTokenExpiresInSeconds: Int64
    public let refreshTokenExpiresInSeconds: Int64

    public init(
        accessToken: String,
        refreshToken: String,
        accessTokenExpiresInSeconds: Int64,
        refreshTokenExpiresInSeconds: Int64
    ) {
        self.accessToken = accessToken
        self.refreshToken = refreshToken
        self.accessTokenExpiresInSeconds = accessTokenExpiresInSeconds
        self.refreshTokenExpiresInSeconds = refreshTokenExpiresInSeconds
    }
}
