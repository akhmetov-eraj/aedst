import Foundation

/// Testability seam around wall-clock time (`Date.now`, `Task.sleep`) — the
/// Swift analogue of Android's `core/common` `DispatcherProvider`. Feature
/// code takes an `AedstClock` instead of calling `Date()`/`Task.sleep`
/// directly, so `AedstTesting`'s `ManualClock` can fast-forward retries and
/// token-expiry checks in unit tests without real delays.
///
/// Durations are `TimeInterval` (seconds), not `Duration`: `Duration` and
/// `Task.sleep(for:)` are iOS 16 / macOS 13, above this package's floor
/// (iOS 15 / macOS 12); `Task.sleep(nanoseconds:)` back-deploys to iOS 13.
public protocol AedstClock: Sendable {
    var now: Date { get }
    func sleep(for duration: TimeInterval) async throws
}

public struct SystemClock: AedstClock {
    public init() {}

    public var now: Date { Date() }

    public func sleep(for duration: TimeInterval) async throws {
        try await Task.sleep(nanoseconds: UInt64(max(duration, 0) * 1_000_000_000))
    }
}
