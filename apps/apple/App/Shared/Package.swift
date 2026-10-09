// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstAppShared",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
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
                // Foundation-only doubles; the DEBUG-only UI-test launch mode reuses InMemorySecureStore.
                "AedstTesting",
                "Auth",
                "Tasks",
                "Search",
                "Notifications",
                "Media",
                "Admin",
            ],
            resources: [.process("Resources")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstAppSharedTests",
            dependencies: [
                "AedstAppShared", "AedstCore", "AedstPersistence", "AedstNetwork", "AedstAnalytics",
                "Auth", "Tasks", "Admin", "AedstTesting",
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
