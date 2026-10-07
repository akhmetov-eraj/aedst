// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstNavigation",
    // iOS/macOS only: `Router`/`AedstNavigationStack` use Perception's
    // `@Perceptible`/`@Perception.Bindable` (the latter is unavailable on
    // visionOS) and FlowStacks. Widen this list deliberately, not by default.
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
    ],
    products: [
        .library(name: "AedstNavigation", targets: ["AedstNavigation"]),
    ],
    dependencies: [
        // Observation backport (`@Perceptible`) so `Router` works below iOS 17 / macOS 14.
        // Latest stable is 2.0.12 (verified 2026-10-07), but it declares swift-tools-version 6.4,
        // which no released Xcode on the CI runners (26.6 max) can parse. Held at 2.0.11 (tools 6.0)
        // per docs/content/decisions.md; lift the ceiling once a Swift 6.4 Xcode is the CI default.
        .package(url: "https://github.com/pointfreeco/swift-perception", "2.0.11"..<"2.0.12"),
        // `NavigationStack`-style API back-ported to iOS 14 / macOS 11 (SwiftUI's own is iOS 16 / macOS 13).
        // 0.9.0 = latest stable (verified 2026-10-07).
        .package(url: "https://github.com/johnpatrickmorgan/FlowStacks", .upToNextMinor(from: "0.9.0")),
    ],
    targets: [
        .target(
            name: "AedstNavigation",
            dependencies: [
                .product(name: "Perception", package: "swift-perception"),
                .product(name: "FlowStacks", package: "FlowStacks"),
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstNavigationTests",
            dependencies: ["AedstNavigation"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
