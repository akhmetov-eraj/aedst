// swift-tools-version: 6.0
import PackageDescription

// Platform-API-only media toolkit (no third-party dependency): ImageIO decoding/
// downsampling/compression, a URLCache-backed image loader keyed by media id,
// AVKit playback, QuickLookThumbnailing / AVAssetImageGenerator thumbnails,
// AVAssetExportSession video compression, a PHPicker wrapper and a background
// URLSession uploader. Server-contract logic (CreateUpload → PUT → CompleteUpload)
// lives in Features/Media; this package knows nothing about the API.
let package = Package(
    name: "AedstMedia",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
    ],
    products: [
        .library(name: "AedstMedia", targets: ["AedstMedia"]),
    ],
    dependencies: [
        .package(path: "../AedstCore"),
        .package(path: "../AedstDesignSystem"),
    ],
    targets: [
        .target(
            name: "AedstMedia",
            dependencies: ["AedstCore", "AedstDesignSystem"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "AedstMediaTests",
            dependencies: ["AedstMedia"],
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
    ]
)
