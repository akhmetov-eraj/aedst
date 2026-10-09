// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstPersistence",
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
        .library(name: "AedstPersistence", targets: ["AedstPersistence"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
        .package(path: "../AedstTesting"),
        // Local SQLite persistence (replaces SwiftData). GRDB is kept for explicit
        // versioned migrations and control over SQL/WAL, not for OS reach. Floor = latest stable (v7.11.1, verified
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
