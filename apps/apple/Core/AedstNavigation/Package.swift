// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstNavigation",
    // Kept iOS/macOS-only: only these have app targets today. Widen this list deliberately, not by default.
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
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
