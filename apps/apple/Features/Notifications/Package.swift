// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Notifications",
    defaultLocalization: "en",
    // Push (APNs) plumbing lives here too: UserNotifications is a system framework.
    // Kept iOS/macOS-only: those are the only app targets; widen deliberately.
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
    ],
    products: [
        .library(name: "Notifications", targets: ["Notifications"]),
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
            name: "Notifications",
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
            name: "NotificationsTests",
            dependencies: ["Notifications", "AedstCore", "AedstAnalytics", "AedstTesting"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
