import AedstCore
import AedstTesting
import Foundation
import Testing
@testable import Auth

@Suite("ConnectAuthRepository")
struct ConnectAuthRepositoryTests {
    private let issuedAt = Date(timeIntervalSince1970: 2_000_000)

    private func makeRepository(
        remote: FakeAuthRemote,
        store: StubTokenProvider,
        clock: ManualClock? = nil
    ) -> ConnectAuthRepository {
        ConnectAuthRepository(
            remote: remote,
            sessionStore: store,
            clock: clock ?? ManualClock(now: issuedAt),
            loginRetryPolicy: RetryPolicy(maxAttempts: 2, baseDelay: 0, maxDelay: 0)
        )
    }

    private let credentials = Credentials(email: "a@b.com", password: "pw")

    @Test("login stores the tokens with absolute expiries computed from the injected clock")
    func loginStoresTokens() async {
        let store = StubTokenProvider()
        let repository = makeRepository(remote: FakeAuthRemote(), store: store)

        let result = await repository.login(with: credentials, deviceID: "d1")

        #expect(result.value?.userID == "user-1")
        #expect(result.value?.accessTokenExpiresAt == issuedAt.addingTimeInterval(900))
        #expect(await store.savedTokens?.accessToken == "access-1")
        #expect(await store.savedTokens?.refreshTokenExpiresAt == issuedAt.addingTimeInterval(86_400))
        #expect(await store.savedUserID == "user-1")
    }

    @Test("bad credentials are not retried and nothing is stored")
    func badCredentials() async {
        let remote = FakeAuthRemote(loginResults: [.failure(.unauthorized)])
        let store = StubTokenProvider()
        let repository = makeRepository(remote: remote, store: store)

        let result = await repository.login(with: credentials, deviceID: "d1")

        #expect(result.error == .unauthorized)
        #expect(await remote.loginCalls.count == 1)
        #expect(await store.savedTokens == nil)
    }

    @Test("a transient network failure is retried once")
    func retriesTransientFailure() async {
        let remote = FakeAuthRemote(loginResults: [
            .failure(.network(reason: "flaky")),
            .success(LoginResultDTO(userID: "user-1", tokens: .stub)),
        ])
        let repository = makeRepository(remote: remote, store: StubTokenProvider())

        let result = await repository.login(with: credentials, deviceID: "d1")

        #expect(result.value != nil)
        #expect(await remote.loginCalls.count == 2)
    }

    @Test("a Keychain write failure surfaces as a storage error, not a silent success")
    func storageFailure() async {
        let store = StubTokenProvider()
        await store.setSaveError(InMemorySecureStore.WriteFailure())
        let repository = makeRepository(remote: FakeAuthRemote(), store: store)

        let result = await repository.login(with: credentials, deviceID: "d1")

        if case .failure(.storage) = result {
            // expected
        } else {
            Issue.record("expected .storage, got \(result)")
        }
    }

    @Test("logout revokes server-side and clears locally")
    func logoutClears() async {
        let remote = FakeAuthRemote()
        let store = StubTokenProvider()
        let repository = makeRepository(remote: remote, store: store)
        _ = await repository.login(with: credentials, deviceID: "d1")

        let result = await repository.logout()

        #expect(result.error == nil)
        #expect(await remote.logoutCalls == ["refresh-1"])
        #expect(await store.didClearTokens)
    }

    @Test("logout clears the local session even when the network call fails")
    func logoutIsLocalFirst() async {
        let remote = FakeAuthRemote(logoutResult: .failure(.network(reason: "offline")))
        let store = StubTokenProvider()
        let repository = makeRepository(remote: remote, store: store)
        _ = await repository.login(with: credentials, deviceID: "d1")

        let result = await repository.logout()

        #expect(result.error == .network(reason: "offline"))
        #expect(await store.didClearTokens)
    }

    @Test("an expired access token is still an active session (it refreshes on demand)")
    func expiredAccessTokenStillActive() async {
        let store = StubTokenProvider()
        let clock = ManualClock(now: issuedAt)
        let repository = makeRepository(remote: FakeAuthRemote(), store: store, clock: clock)
        _ = await repository.login(with: credentials, deviceID: "d1")

        clock.advance(by: 1_000) // > 900s access lifetime, < 86 400s refresh lifetime

        #expect(await repository.hasActiveSession())
        #expect(await repository.currentSession()?.isAccessTokenExpired(at: clock.now) == true)
    }

    @Test("an expired refresh token means signed out")
    func expiredRefreshTokenMeansSignedOut() async {
        let store = StubTokenProvider()
        let clock = ManualClock(now: issuedAt)
        let repository = makeRepository(remote: FakeAuthRemote(), store: store, clock: clock)
        _ = await repository.login(with: credentials, deviceID: "d1")

        clock.advance(by: 90_000)

        #expect(await repository.hasActiveSession() == false)
    }
}
