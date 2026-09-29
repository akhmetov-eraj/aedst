// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstNavigation",
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
        .library(name: "AedstNavigation", targets: ["AedstNavigation"]),
    ],
    targets: [
        .target(
            name: "AedstNavigation",
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstNavigationTests",
            dependencies: ["AedstNavigation"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
