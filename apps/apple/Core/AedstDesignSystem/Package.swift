// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstDesignSystem",
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
        .library(name: "AedstDesignSystem", targets: ["AedstDesignSystem"]),
    ],
    targets: [
        .target(
            name: "AedstDesignSystem",
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstDesignSystemTests",
            dependencies: ["AedstDesignSystem"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
