// swift-tools-version: 6.0
import PackageDescription

// The only package that imports a telemetry SDK. Features depend on
// AedstAnalytics' protocols; App/Shared composes this implementation.
//
// Versions: latest stable, verified 2026-10-07 via the GitHub releases API
// (`/repos/<owner>/<repo>/releases/latest`):
//   sentry-cocoa 9.30.0  (iOS 15 / macOS 12 floor — same as the app)
//   posthog-ios  3.90.2  (iOS 13 / macOS 10.15)
let package = Package(
    name: "AedstTelemetry",
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
    ],
    products: [
        .library(name: "AedstTelemetry", targets: ["AedstTelemetry"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
        .package(path: "../AedstAnalytics"),
        .package(url: "https://github.com/getsentry/sentry-cocoa", from: "9.30.0"),
        .package(url: "https://github.com/PostHog/posthog-ios", from: "3.90.2"),
    ],
    targets: [
        .target(
            name: "AedstTelemetry",
            dependencies: [
                "AedstCore",
                "AedstAnalytics",
                .product(name: "Sentry", package: "sentry-cocoa"),
                .product(name: "PostHog", package: "posthog-ios"),
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstTelemetryTests",
            dependencies: ["AedstTelemetry"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
