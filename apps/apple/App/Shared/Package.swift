// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstAppShared",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
    ],
    products: [
        .library(name: "AedstAppShared", targets: ["AedstAppShared"]),
    ],
    dependencies: [
        .package(path: "../../Core/AedstCore"),
        .package(path: "../../Core/AedstNetwork"),
        .package(path: "../../Core/AedstPersistence"),
        .package(path: "../../Core/AedstDesignSystem"),
        .package(path: "../../Core/AedstNavigation"),
        .package(path: "../../Core/AedstAnalytics"),
        .package(path: "../../Core/AedstTesting"),
        .package(path: "../../Core/AedstMedia"),
        .package(path: "../../Core/AedstTelemetry"),
        .package(path: "../../Features/Auth"),
        .package(path: "../../Features/Tasks"),
        .package(path: "../../Features/Search"),
        .package(path: "../../Features/Notifications"),
        .package(path: "../../Features/Media"),
        .package(path: "../../Features/Admin"),
        // Same range as every feature (see Core/AedstNavigation/Package.swift).
        .package(url: "https://github.com/pointfreeco/swift-perception", "2.0.11"..<"2.0.12"),
    ],
    targets: [
        .target(
            name: "AedstAppShared",
            dependencies: [
                "AedstCore",
                "AedstNetwork",
                "AedstPersistence",
                "AedstDesignSystem",
                "AedstNavigation",
                "AedstAnalytics",
                "AedstMedia",
                "AedstTelemetry",
                "Auth",
                "Tasks",
                "Search",
                "Notifications",
                "Media",
                "Admin",
                .product(name: "Perception", package: "swift-perception"),
            ],
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstAppSharedTests",
            dependencies: ["AedstAppShared", "AedstCore", "AedstPersistence", "AedstNetwork", "Auth", "AedstTesting"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
