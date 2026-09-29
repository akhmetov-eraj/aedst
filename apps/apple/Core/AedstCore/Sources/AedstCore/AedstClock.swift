import Foundation

/// Testability seam around wall-clock time (`Date.now`, `Task.sleep`) — the
/// Swift analogue of Android's `core/common` `DispatcherProvider`. Feature
/// code takes an `AedstClock` instead of calling `Date()`/`Task.sleep`
/// directly, so `AedstTesting`'s `ManualClock` can fast-forward retries and
/// token-expiry checks in unit tests without real delays.
public protocol AedstClock: Sendable {
    var now: Date { get }
    func sleep(for duration: Duration) async throws
}

public struct SystemClock: AedstClock {
    public init() {}

    public var now: Date { Date() }

    public func sleep(for duration: Duration) async throws {
        try await Task.sleep(for: duration)
    }
}
