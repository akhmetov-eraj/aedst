// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Tasks",
    defaultLocalization: "en",
    // Kept iOS/macOS-only: those are the only app targets; widen deliberately.
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
    ],
    products: [
        .library(name: "Tasks", targets: ["Tasks"]),
    ],
    dependencies: [
        .package(path: "../../Core/AedstCore"),
        .package(path: "../../Core/AedstNetwork"),
        .package(path: "../../Core/AedstDesignSystem"),
        .package(path: "../../Core/AedstNavigation"),
        .package(path: "../../Core/AedstAnalytics"),
        .package(path: "../../Core/AedstTesting"),
    ],
    targets: [
        .target(
            name: "Tasks",
            dependencies: [
                "AedstCore",
                "AedstNetwork",
                "AedstDesignSystem",
                "AedstNavigation",
                "AedstAnalytics",
            ],
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "TasksTests",
            dependencies: ["Tasks", "AedstCore", "AedstNavigation", "AedstAnalytics", "AedstTesting"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
