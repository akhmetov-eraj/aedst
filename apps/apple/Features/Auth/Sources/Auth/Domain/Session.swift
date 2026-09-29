import Foundation

/// The `auth` module's session concept on the client — mirrors
/// `apps/server/api/proto/aedst/auth/v1`'s `LoginResponse`/`TokenPair`
/// without leaking the wire format into `Presentation`.
public struct Session: Sendable, Equatable {
    public let userID: String
    public let accessTokenExpiresAt: Date
    public let refreshTokenExpiresAt: Date

    public init(userID: String, accessTokenExpiresAt: Date, refreshTokenExpiresAt: Date) {
        self.userID = userID
        self.accessTokenExpiresAt = accessTokenExpiresAt
        self.refreshTokenExpiresAt = refreshTokenExpiresAt
    }

    public var isAccessTokenExpired: Bool {
        accessTokenExpiresAt <= .now
    }
}

public struct Credentials: Sendable {
    public let email: String
    public let password: String

    public init(email: String, password: String) {
        self.email = email
        self.password = password
    }
}
