// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstAnalytics",
    // watchOS/tvOS/visionOS are scaffolded minimums for future app targets:
    // only App_iOS and App_macOS exist in Project.swift today, so these three
    // platforms build the package but have no real UI/entry point yet.
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
        .watchOS(.v8),
        .tvOS(.v15),
        .visionOS(.v1),
    ],
    products: [
        .library(name: "AedstAnalytics", targets: ["AedstAnalytics"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
    ],
    targets: [
        .target(
            name: "AedstAnalytics",
            dependencies: ["AedstCore"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstAnalyticsTests",
            dependencies: ["AedstAnalytics"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
