// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstAppShared",
    platforms: [
        .iOS(.v17),
        .macOS(.v14),
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
        .package(path: "../../Features/Auth"),
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
                "Auth",
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstAppSharedTests",
            dependencies: ["AedstAppShared"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
