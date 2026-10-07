// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstDesignSystem",
    defaultLocalization: "en",
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
        .library(name: "AedstDesignSystem", targets: ["AedstDesignSystem"]),
    ],
    dependencies: [
        // Only for `AedstError` in `AedstErrorPresenter` (Foundation-only package, no UI).
        .package(path: "../AedstCore"),
    ],
    targets: [
        .target(
            name: "AedstDesignSystem",
            dependencies: ["AedstCore"],
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstDesignSystemTests",
            dependencies: ["AedstDesignSystem"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
