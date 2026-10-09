// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstCore",
    defaultLocalization: "en",
    // watchOS/tvOS/visionOS are scaffolded minimums for future app targets:
    // only App_iOS and App_macOS exist in Project.swift today, so these three
    // platforms build the package but have no real UI/entry point yet.
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
        .watchOS(.v11),
        .tvOS(.v18),
        .visionOS(.v2),
    ],
    products: [
        .library(name: "AedstCore", targets: ["AedstCore"]),
    ],
    targets: [
        .target(
            name: "AedstCore",
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstCoreTests",
            dependencies: ["AedstCore"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
