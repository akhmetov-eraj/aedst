// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Media",
    defaultLocalization: "en",
    // Kept iOS/macOS-only: those are the only app targets; widen deliberately.
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
    ],
    products: [
        .library(name: "Media", targets: ["Media"]),
    ],
    dependencies: [
        .package(path: "../../Core/AedstCore"),
        .package(path: "../../Core/AedstNetwork"),
        .package(path: "../../Core/AedstDesignSystem"),
        .package(path: "../../Core/AedstNavigation"),
        .package(path: "../../Core/AedstAnalytics"),
        .package(path: "../../Core/AedstTesting"),
        .package(path: "../../Core/AedstMedia"),
    ],
    targets: [
        .target(
            name: "Media",
            dependencies: [
                "AedstCore",
                "AedstNetwork",
                "AedstDesignSystem",
                "AedstNavigation",
                "AedstAnalytics",
                "AedstMedia",
            ],
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "MediaTests",
            dependencies: ["Media", "AedstCore", "AedstAnalytics", "AedstMedia", "AedstTesting"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
