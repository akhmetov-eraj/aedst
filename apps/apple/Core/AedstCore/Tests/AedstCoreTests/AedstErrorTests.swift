import Testing
@testable import AedstCore

@Suite("AedstError")
struct AedstErrorTests {
    @Test("network and rateLimited are retryable")
    func retryableCases() {
        #expect(AedstError.network(reason: "timeout").isRetryable)
        #expect(AedstError.rateLimited(retryAfter: 1).isRetryable)
    }

    @Test("client errors are not retryable")
    func nonRetryableCases() {
        #expect(!AedstError.unauthorized.isRetryable)
        #expect(!AedstError.notFound.isRetryable)
        #expect(!AedstError.validation(fields: ["email"]).isRetryable)
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
}
