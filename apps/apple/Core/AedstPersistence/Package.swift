// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstPersistence",
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
        .library(name: "AedstPersistence", targets: ["AedstPersistence"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
        .package(path: "../AedstTesting"),
        // Local SQLite persistence (replaces SwiftData, which needs iOS 17 / macOS 14).
        // GRDB 7.x runs on iOS 13 / macOS 10.15. Floor = latest stable (v7.11.1, verified
        // 2026-10-07 via the GitHub releases API); it needs swift-tools 6.1 (Xcode 16.3+; CI uses 26.x).
        .package(url: "https://github.com/groue/GRDB.swift", from: "7.11.1"),
    ],
    targets: [
        .target(
            name: "AedstPersistence",
            dependencies: [
                "AedstCore",
                .product(name: "GRDB", package: "GRDB.swift"),
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstPersistenceTests",
            dependencies: [
                "AedstPersistence",
                "AedstCore",
                "AedstTesting",
                .product(name: "GRDB", package: "GRDB.swift"),
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
