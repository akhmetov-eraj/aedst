import AedstCore

/// Analytics/crash-reporting facade — feature modules depend on this
/// protocol, never on Firebase/Crashlytics/Amplitude SDK types directly, so
/// swapping providers later touches one file (`FirebaseAnalyticsClient` or
/// whichever is added under `Sources/AedstAnalytics/Providers/`). Mirrors
/// `apps/android/core/analytics`'s facade + no-op split.
public protocol AnalyticsClient: Sendable {
    func track(_ event: AnalyticsEvent)
    func setUserID(_ userID: String?)
    func recordError(_ error: Error, context: [String: String])
}

public struct AnalyticsEvent: Sendable {
    public let name: String
    public let parameters: [String: String]

    public init(name: String, parameters: [String: String] = [:]) {
        self.name = name
        self.parameters = parameters
    }
}

/// Default binding until a real provider (Firebase, Amplitude, ...) is
/// wired in `App/Shared`'s composition root — logs through `AedstLogger`
/// instead of silently dropping events, so debug builds still show what
/// *would* have been sent.
public struct NoopAnalyticsClient: AnalyticsClient {
    private let logger = AedstLogger(category: "Analytics")

    public init() {}

    public func track(_ event: AnalyticsEvent) {
        logger.debug("track(\(event.name), \(event.parameters))")
    }

    public func setUserID(_ userID: String?) {
        logger.debug("setUserID(\(userID ?? "nil"))")
    }

    public func recordError(_ error: Error, context: [String: String]) {
        logger.error("recordError(\(context))", error: error)
    }
}
