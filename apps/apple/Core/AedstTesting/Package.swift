// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstTesting",
    // watchOS/tvOS/visionOS are scaffolded minimums for future app targets:
    // only App_iOS and App_macOS exist in project.yml today, so these three
    // platforms build the package but have no real UI/entry point yet.
    platforms: [
        .iOS(.v17),
        .macOS(.v14),
        .watchOS(.v10),
        .tvOS(.v17),
        .visionOS(.v1),
    ],
    products: [
        .library(name: "AedstTesting", targets: ["AedstTesting"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
    ],
    targets: [
        .target(
            name: "AedstTesting",
            dependencies: ["AedstCore"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
