import AedstCore
import AedstTesting
import Foundation
import Testing
@testable import Auth

@Suite("AuthTokenRefresher")
struct AuthTokenRefresherTests {
    private let now = Date(timeIntervalSince1970: 5_000)

    private func makeRefresher(_ result: Outcome<TokenPairDTO>) -> (AuthTokenRefresher, FakeAuthRemote) {
        let remote = FakeAuthRemote(refreshResults: [result])
        return (AuthTokenRefresher(remote: remote, clock: ManualClock(now: now)), remote)
    }

    @Test("success becomes .refreshed with absolute expiries")
    func success() async {
        let (refresher, remote) = makeRefresher(.success(.stub))

        let result = await refresher.refresh(refreshToken: "old")

        #expect(result == .refreshed(TokenPairDTO.stub.tokenPair(issuedAt: now)))
        #expect(await remote.refreshCalls == ["old"])
    }

    @Test("a server rejection ends the session (.rejected)")
    func unauthorizedIsRejected() async {
        #expect(await makeRefresher(.failure(.unauthorized)).0.refresh(refreshToken: "x") == .rejected)
        #expect(await makeRefresher(.failure(.validation(fields: ["refresh_token"]))).0.refresh(refreshToken: "x") == .rejected)
    }

    @Test("offline, timeouts, throttling and server errors keep the session (.unavailable)")
    func transientIsUnavailable() async {
        for error in [
            AedstError.network(reason: "offline"),
            .rateLimited(retryAfter: 5),
            .unknown(reason: "500"),
            .decoding(reason: "bad body"),
        ] {
            #expect(await makeRefresher(.failure(error)).0.refresh(refreshToken: "x") == .unavailable)
        }
    }

    @Test("a refresh is attempted exactly once (refresh tokens rotate; never blind-retry)")
    func noBlindRetry() async {
        let (refresher, remote) = makeRefresher(.failure(.network(reason: "offline")))

        _ = await refresher.refresh(refreshToken: "x")

        #expect(await remote.refreshCalls.count == 1)
    }
}
