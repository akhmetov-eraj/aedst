import AedstCore

/// Deterministic `TokenProvider` double for feature-module unit tests that
/// exercise `AuthInterceptor`/repository retry logic without a real
/// Keychain-backed store.
public actor StubTokenProvider: SessionStore {
    public var accessToken: String?
    public var refreshResult: String?
    public private(set) var refreshCallCount = 0
    public private(set) var didClearTokens = false
    public private(set) var savedTokens: (access: String, refresh: String)?

    public init(accessToken: String? = nil, refreshResult: String? = nil) {
        self.accessToken = accessToken
        self.refreshResult = refreshResult
    }

    public func currentAccessToken() async -> String? {
        accessToken
    }

    public func refreshAccessToken() async -> String? {
        refreshCallCount += 1
        if let refreshResult {
            accessToken = refreshResult
        }
        return refreshResult
    }

    public func clearTokens() async {
        didClearTokens = true
        accessToken = nil
    }

    public func save(accessToken: String, refreshToken: String) async {
        savedTokens = (accessToken, refreshToken)
        self.accessToken = accessToken
    }

    public func currentRefreshToken() async -> String? {
        savedTokens?.refresh
    }
}
