import Foundation
import Testing
@testable import AedstPersistence

@Suite("KeychainTokenStore")
struct KeychainTokenStoreTests {
    @Test("dedupes concurrent refresh calls into one refreshHandler invocation")
    func dedupesConcurrentRefresh() async {
        let callCount = Counter()
        let store = KeychainTokenStore(
            keychain: Keychain(service: "com.aedst.apple.tests.\(UUID().uuidString)"),
            refreshHandler: { _ in
                await callCount.increment()
                return ("new-access", "new-refresh")
            }
        )
        await store.save(accessToken: "old-access", refreshToken: "seed-refresh")

        async let first = store.refreshAccessToken()
        async let second = store.refreshAccessToken()
        let results = await [first, second]

        #expect(results == ["new-access", "new-access"])
        #expect(await callCount.value == 1)
    }
}

private actor Counter {
    private(set) var value = 0
    func increment() { value += 1 }
}
