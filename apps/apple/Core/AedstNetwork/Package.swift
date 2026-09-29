// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "AedstNetwork",
    // watchOS/tvOS/visionOS are scaffolded minimums for future app targets:
    // only App_iOS and App_macOS exist in project.yml today, so these three
    // platforms build the package but have no real UI/entry point yet.
    platforms: [
        .iOS(.v17),
        .macOS(.v14),
        .watchOS(.v10),
        .tvOS(.v17),
        .visionOS(.v1),
    ],
    products: [
        .library(name: "AedstNetwork", targets: ["AedstNetwork"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
        // Generated ConnectRPC stubs (see ../../buf.gen.yaml) import these two.
        .package(url: "https://github.com/connectrpc/connect-swift", from: "0.13.0"),
        .package(url: "https://github.com/apple/swift-protobuf", from: "1.28.0"),
    ],
    targets: [
        .target(
            name: "AedstNetwork",
            dependencies: [
                "AedstCore",
                .product(name: "Connect", package: "connect-swift"),
                .product(name: "SwiftProtobuf", package: "swift-protobuf"),
            ],
            exclude: ["Generated/README.md"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstNetworkTests",
            dependencies: ["AedstNetwork"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
