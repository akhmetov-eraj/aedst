import AedstCore

/// `AedstCore.TokenProvider` backed by the Keychain, with in-flight-refresh
/// de-duplication: if three requests hit a 401 at once, only the first
/// actually calls `refreshHandler` — the rest await its result.
public actor KeychainTokenStore: SessionStore {
    private let keychain: Keychain
    private var refreshTask: Task<String?, Never>?
    private let refreshHandler: @Sendable (_ refreshToken: String) async -> (access: String, refresh: String)?

    private enum Key {
        static let accessToken = "aedst.accessToken"
        static let refreshToken = "aedst.refreshToken"
    }

    public init(
        keychain: Keychain = Keychain(),
        refreshHandler: @escaping @Sendable (_ refreshToken: String) async -> (access: String, refresh: String)?
    ) {
        self.keychain = keychain
        self.refreshHandler = refreshHandler
    }

    public func currentAccessToken() async -> String? {
        keychain.get(Key.accessToken)
    }

    public func save(accessToken: String, refreshToken: String) async {
        keychain.set(accessToken, for: Key.accessToken)
        keychain.set(refreshToken, for: Key.refreshToken)
    }

    public func refreshAccessToken() async -> String? {
        if let refreshTask {
            return await refreshTask.value
        }

        let task = Task<String?, Never> { [keychain, refreshHandler] in
            guard let refreshToken = keychain.get(Key.refreshToken),
                  let refreshed = await refreshHandler(refreshToken) else {
                return nil
            }
            keychain.set(refreshed.access, for: Key.accessToken)
            keychain.set(refreshed.refresh, for: Key.refreshToken)
            return refreshed.access
        }
        refreshTask = task

        let result = await task.value
        refreshTask = nil
        return result
    }

    public func currentRefreshToken() async -> String? {
        keychain.get(Key.refreshToken)
    }

    public func clearTokens() async {
        keychain.remove(Key.accessToken)
        keychain.remove(Key.refreshToken)
    }
}
