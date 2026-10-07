import Foundation
import Testing
@testable import AedstCore

@Suite("AedstError")
struct AedstErrorTests {
    @Test("network and rateLimited are retryable")
    func retryableCases() {
        #expect(AedstError.network(reason: "timeout").isRetryable)
        #expect(AedstError.rateLimited(retryAfter: 1).isRetryable)
    }

    @Test("client, storage and unknown errors are not retryable")
    func nonRetryableCases() {
        #expect(!AedstError.unauthorized.isRetryable)
        #expect(!AedstError.notFound.isRetryable)
        #expect(!AedstError.validation(fields: ["email"]).isRetryable)
        #expect(!AedstError.storage(reason: "x").isRetryable)
        #expect(!AedstError.unknown(reason: "x").isRetryable)
    }

    @Test("Outcome exposes value and error")
    func outcomeAccessors() {
        let success: Outcome<Int> = .success(1)
        let failure: Outcome<Int> = .failure(.unauthorized)

        #expect(success.value == 1)
        #expect(success.error == nil)
        #expect(failure.value == nil)
        #expect(failure.error == .unauthorized)
    }

    @Test("user-facing text is localized and never leaks the technical reason")
    func errorDescriptionHidesTechnicalDetail() {
        let secret = "SELECT * FROM users; stacktrace 0xdeadbeef"
        let all: [AedstError] = [
            .network(reason: secret), .unauthorized, .notFound, .validation(fields: [secret]),
            .rateLimited(retryAfter: 3), .decoding(reason: secret), .storage(reason: secret), .unknown(reason: secret),
        ]
        for error in all {
            let text = error.errorDescription
            #expect(text != nil)
            #expect(text?.isEmpty == false)
            #expect(text?.contains(secret) == false)
            #expect(text?.hasPrefix("error") == false, "missing localization key for \(error)")
        }
    }

    @Test("technicalDescription keeps the detail for logs")
    func technicalDescriptionKeepsDetail() {
        #expect(AedstError.network(reason: "boom").technicalDescription.contains("boom"))
    }

    @Test("analytics categories are PII-free constants")
    func analyticsCategory() {
        #expect(AedstError.network(reason: "user@example.com").analyticsCategory == "network")
        #expect(AedstError.storage(reason: "x").analyticsCategory == "storage")
    }
}
