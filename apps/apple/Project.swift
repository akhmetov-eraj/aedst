import ProjectDescription

// Tuist project manifest — replaces the XcodeGen `project.yml`. Generates
// AEDST.xcodeproj / AEDST.xcworkspace (both gitignored: never hand-edit or
// commit them). `tuist install` resolves `Tuist/Package.swift`; `tuist generate`
// writes the project. See README.md's "Getting started".
//
// One project, two platform-specific app targets (`App_iOS`, `App_macOS`)
// sharing every line of business logic through the local Swift packages under
// Core/ and Features/, composed in App/Shared — see docs/architecture-apple.md.

// Minimum OS versions: the single place these are declared for the app targets
// (they were duplicated in project.yml and Config/*.xcconfig before). Each
// local package's `platforms:` in its Package.swift must not exceed these.
// Reasoning table: README.md "Minimum OS versions".
private let iOSDeploymentTarget = "15.0"
private let macOSDeploymentTarget = "12.0"

private let bundleIdentifierPrefix = "com.aedst.apple"

private func makeAppTarget(
    name: String,
    destinations: Destinations,
    deploymentTargets: DeploymentTargets,
    platformDirectory: String,
    bundleIdentifierSuffix: String,
    resources: ResourceFileElements,
    entitlements: Entitlements? = nil,
    additionalSettings: SettingsDictionary = [:]
) -> Target {
    .target(
        name: name,
        destinations: destinations,
        product: .app,
        productName: "AEDST",
        bundleId: "\(bundleIdentifierPrefix).\(bundleIdentifierSuffix)",
        deploymentTargets: deploymentTargets,
        infoPlist: .file(path: "App/\(platformDirectory)/Info.plist"),
        sources: ["App/\(platformDirectory)/Sources/**"],
        resources: resources,
        entitlements: entitlements,
        dependencies: [
            .external(name: "AedstAppShared"),
        ],
        settings: .settings(
            base: additionalSettings,
            configurations: [
                .debug(name: .debug, xcconfig: "Config/\(platformDirectory)-Debug.xcconfig"),
                .release(name: .release, xcconfig: "Config/\(platformDirectory)-Release.xcconfig"),
            ]
        )
    )
}

private func makeScheme(named name: String) -> Scheme {
    .scheme(
        name: name,
        shared: true,
        buildAction: .buildAction(targets: [.target(name)]),
        runAction: .runAction(configuration: .debug),
        archiveAction: .archiveAction(configuration: .release)
    )
}

let project = Project(
    name: "AEDST",
    options: .options(
        // Schemes are declared explicitly below (same two as the old project.yml).
        automaticSchemesOptions: .disabled,
        // Source language of the catalogs (en, ru, ar, tg, uz are translated).
        developmentRegion: "en"
    ),
    targets: [
        makeAppTarget(
            name: "App_iOS",
            // iPhone + iPad only — not "Designed for iPad" on Mac (Tuist's `.iOS` shorthand
            // includes it); macOS has its own native target.
            destinations: [.iPhone, .iPad],
            deploymentTargets: .iOS(iOSDeploymentTarget),
            platformDirectory: "iOS",
            bundleIdentifierSuffix: "ios",
            resources: [
                "App/iOS/Assets.xcassets",
                // App-level privacy manifest (App Store requirement): tracking = none,
                // collected data types, required-reason APIs. Third-party SDKs ship their own.
                "App/iOS/PrivacyInfo.xcprivacy",
                // Localized Info.plist strings (Face ID usage description, display name).
                "App/iOS/Resources/**",
            ],
            // Push Notifications (aps-environment = $(APS_ENVIRONMENT), per configuration).
            entitlements: .file(path: "App/iOS/AEDST.entitlements")
        ),
        makeAppTarget(
            name: "App_macOS",
            destinations: [.mac],
            deploymentTargets: .macOS(macOSDeploymentTarget),
            platformDirectory: "macOS",
            bundleIdentifierSuffix: "macos",
            resources: [
                "App/macOS/Assets.xcassets",
                "App/macOS/PrivacyInfo.xcprivacy",
            ],
            entitlements: .file(path: "App/macOS/AEDST.entitlements"),
            additionalSettings: ["COMBINE_HIDPI_IMAGES": "YES"]
        ),
    ],
    schemes: [
        makeScheme(named: "App_iOS"),
        makeScheme(named: "App_macOS"),
    ]
)
