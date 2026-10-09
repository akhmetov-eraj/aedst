// swift-tools-version: 6.0
import PackageDescription

// Media toolkit: remote image loading/caching via Nuke (owner decision: SwiftUI
// AsyncImage has no real disk cache or prefetching); platform APIs for ImageIO
// compression, AVKit playback, QuickLookThumbnailing / AVAssetImageGenerator thumbnails,
// AVAssetExportSession video compression, a PHPicker wrapper and a background
// URLSession uploader. Server-contract logic (CreateUpload → PUT → CompleteUpload)
// lives in Features/Media; this package knows nothing about the API.
let package = Package(
    name: "AedstMedia",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v18),
        .macOS(.v15),
    ],
    products: [
        .library(name: "AedstMedia", targets: ["AedstMedia"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
        .package(path: "../AedstDesignSystem"),
        .package(url: "https://github.com/kean/Nuke", from: "13.2.0"),
    ],
    targets: [
        .target(
            name: "AedstMedia",
            dependencies: [
                "AedstCore",
                "AedstDesignSystem",
                .product(name: "Nuke", package: "Nuke"),
                .product(name: "NukeUI", package: "Nuke"),
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstMediaTests",
            dependencies: [
                "AedstMedia",
                .product(name: "Nuke", package: "Nuke"),
            ],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
