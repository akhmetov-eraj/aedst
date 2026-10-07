// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Tasks",
    defaultLocalization: "en",
    // iOS/macOS only, like Features/Auth (Perception's `@Perception.Bindable`).
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
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
        // Observation backport — see Core/AedstNavigation/Package.swift for the version-range rationale.
        .package(url: "https://github.com/pointfreeco/swift-perception", "2.0.11"..<"2.0.12"),
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
                .product(name: "Perception", package: "swift-perception"),
            ],
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "TasksTests",
            dependencies: ["Tasks", "AedstCore", "AedstTesting"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
