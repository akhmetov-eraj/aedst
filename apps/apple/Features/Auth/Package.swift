// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Auth",
    defaultLocalization: "en",
    // iOS/macOS only: `LoginViewModel`/`LoginView` use Perception's
    // `@Perceptible`/`@Perception.Bindable` (the latter is unavailable on visionOS).
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
    ],
    products: [
        .library(name: "Auth", targets: ["Auth"]),
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
            name: "Auth",
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
            name: "AuthTests",
            dependencies: ["Auth", "AedstCore", "AedstTesting"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
