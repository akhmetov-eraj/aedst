import Foundation
import Testing
@testable import AedstPersistence

/// Exercises the real `SecItem*` calls. Opt-in (`AEDST_KEYCHAIN_TESTS=1`)
/// because an unsigned `swift test` process on a headless CI runner often has
/// no usable keychain; run it from an app test host or locally.
@Suite(
    "Keychain (real SecItem calls)",
    .enabled(if: ProcessInfo.processInfo.environment["AEDST_KEYCHAIN_TESTS"] == "1")
)
struct KeychainIntegrationTests {
    private func makeKeychain() -> Keychain {
        Keychain(service: "com.aedst.apple.tests.\(UUID().uuidString)")
    }

    @Test("write, overwrite (upsert), read and delete")
    func roundTrip() throws {
        let keychain = makeKeychain()
        #expect(try keychain.get("k") == nil)

        try keychain.set("one", for: "k")
        try keychain.set("two", for: "k")
        #expect(try keychain.get("k") == "two")

        try keychain.remove("k")
        #expect(try keychain.get("k") == nil)
        try keychain.remove("k") // deleting a missing item is not an error
    }
}
