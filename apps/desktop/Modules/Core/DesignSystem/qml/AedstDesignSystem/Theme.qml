pragma Singleton

import QtQuick

// Read-only façade so views depend on semantic token names, not on the token
// source. Values come from the GENERATED AedstTokens singleton (single source:
// packages/design-tokens/tokens); the only runtime state is ThemeController.dark
// (OS scheme / user override). Names mirror the CSS custom properties in
// apps/web/src/app/styles/globals.css (shadcn/ui token set).
//
// Scale names: spacingXl/spacing2xl map to tokens spacingXxl/spacingXxxl (24/32)
// and radius2xl to radiusXxl (18) so existing layouts keep their exact values.
QtObject {
    readonly property bool dark: ThemeController.dark

    // --- Semantic color tokens (web parity) ---------------------------------
    readonly property color background: dark ? AedstTokens.darkBackground : AedstTokens.lightBackground
    readonly property color foreground: dark ? AedstTokens.darkForeground : AedstTokens.lightForeground
    readonly property color card: dark ? AedstTokens.darkCard : AedstTokens.lightCard
    readonly property color cardForeground: dark ? AedstTokens.darkCardForeground : AedstTokens.lightCardForeground
    readonly property color popover: dark ? AedstTokens.darkPopover : AedstTokens.lightPopover
    readonly property color popoverForeground: dark ? AedstTokens.darkPopoverForeground : AedstTokens.lightPopoverForeground
    readonly property color primary: dark ? AedstTokens.darkPrimary : AedstTokens.lightPrimary
    readonly property color primaryForeground: dark ? AedstTokens.darkPrimaryForeground : AedstTokens.lightPrimaryForeground
    readonly property color secondary: dark ? AedstTokens.darkSecondary : AedstTokens.lightSecondary
    readonly property color secondaryForeground: dark ? AedstTokens.darkSecondaryForeground : AedstTokens.lightSecondaryForeground
    readonly property color muted: dark ? AedstTokens.darkMuted : AedstTokens.lightMuted
    readonly property color mutedForeground: dark ? AedstTokens.darkMutedForeground : AedstTokens.lightMutedForeground
    readonly property color accent: dark ? AedstTokens.darkAccent : AedstTokens.lightAccent
    readonly property color accentForeground: dark ? AedstTokens.darkAccentForeground : AedstTokens.lightAccentForeground
    readonly property color destructive: dark ? AedstTokens.darkDestructive : AedstTokens.lightDestructive
    readonly property color destructiveForeground: dark ? AedstTokens.darkDestructiveForeground : AedstTokens.lightDestructiveForeground
    readonly property color border: dark ? AedstTokens.darkBorder : AedstTokens.lightBorder
    readonly property color input: dark ? AedstTokens.darkInput : AedstTokens.lightInput
    readonly property color ring: dark ? AedstTokens.darkRing : AedstTokens.lightRing
    readonly property color success: dark ? AedstTokens.darkSuccess : AedstTokens.lightSuccess
    readonly property color successForeground: dark ? AedstTokens.darkSuccessForeground : AedstTokens.lightSuccessForeground
    readonly property color warning: dark ? AedstTokens.darkWarning : AedstTokens.lightWarning
    readonly property color warningForeground: dark ? AedstTokens.darkWarningForeground : AedstTokens.lightWarningForeground

    // Derived from tokens, not new literals: a modal backdrop / shadow is the opposite-of-surface
    // colour at partial opacity; a shimmer highlight is the surface-contrast colour.
    readonly property color scrim: Qt.alpha(dark ? background : foreground, 0.45)
    readonly property color scrimLight: Qt.alpha(dark ? background : foreground, 0.35)
    readonly property color shadow: Qt.alpha(dark ? background : foreground, 0.18)
    readonly property color shimmer: Qt.alpha(dark ? foreground : background, dark ? 0.06 : 0.5)

    // Media overlays sit on top of video frames, so they use the dark palette in BOTH themes.
    readonly property color mediaBackground: AedstTokens.darkBackground
    readonly property color mediaForeground: AedstTokens.darkForeground
    readonly property color mediaScrimStrong: Qt.alpha(AedstTokens.darkBackground, 0.6)
    readonly property color mediaScrim: Qt.alpha(AedstTokens.darkBackground, 0.5)

    readonly property var chart: [
        (dark ? AedstTokens.darkChart1 : AedstTokens.lightChart1),
        (dark ? AedstTokens.darkChart2 : AedstTokens.lightChart2),
        (dark ? AedstTokens.darkChart3 : AedstTokens.lightChart3),
        (dark ? AedstTokens.darkChart4 : AedstTokens.lightChart4),
        (dark ? AedstTokens.darkChart5 : AedstTokens.lightChart5)
    ]

    // --- Scales --------------------------------------------------------------
    readonly property int radiusSm: AedstTokens.radiusSm
    readonly property int radiusMd: AedstTokens.radiusMd
    readonly property int radiusLg: AedstTokens.radiusLg
    readonly property int radiusXl: AedstTokens.radiusXl
    readonly property int radius2xl: AedstTokens.radiusXxl

    readonly property int spacingXxs: AedstTokens.spacingXxs
    readonly property int spacingXs: AedstTokens.spacingXs
    readonly property int spacingSm: AedstTokens.spacingSm
    readonly property int spacingMd: AedstTokens.spacingMd
    readonly property int spacingLg: AedstTokens.spacingLg
    readonly property int spacingXl: AedstTokens.spacingXxl
    readonly property int spacing2xl: AedstTokens.spacingXxxl
    readonly property int spacingHuge: AedstTokens.spacingHuge
    readonly property int spacingMassive: AedstTokens.spacingMassive

    // --- Layout breakpoints (window width, generated tokens) ----------------------
    // Compact below breakpointMd, regular from breakpointMd, wide from breakpointLg. A view asks its own window width:
    //   readonly property bool compact: width < Theme.breakpointMd
    readonly property int breakpointSm: AedstTokens.breakpointSm
    readonly property int breakpointMd: AedstTokens.breakpointMd
    readonly property int breakpointLg: AedstTokens.breakpointLg

    // --- Typography ------------------------------------------------------------
    // The OS text size (Windows "Text size", GNOME/KDE font scaling) reaches Qt as a larger default application font.
    // Token font sizes are fixed pixels, so they are multiplied by that ratio: UI text follows the system setting.
    // Never below 1 (a small system font must not shrink the design) and capped at 2. The reference is the stock
    // default point size of the platform (UNVERIFIED: Segoe UI 9 pt on Windows, 10 pt on the Linux desktops).
    readonly property real referencePointSize: Qt.platform.os === "windows" ? 9 : 10
    readonly property real textScale: {
        const point = Qt.application.font.pointSize
        return point > 0 ? Math.max(1, Math.min(2, point / referencePointSize)) : 1
    }
    readonly property font titleFont: Qt.font({
        pixelSize: Math.round(AedstTokens.typographyHeadlineMediumFontSize * textScale),
        weight: AedstTokens.typographyHeadlineMediumFontWeight
    })
    readonly property font headingFont: Qt.font({
        pixelSize: Math.round(AedstTokens.typographyTitleMediumFontSize * textScale),
        weight: AedstTokens.typographyTitleMediumFontWeight
    })
    readonly property font bodyFont: Qt.font({
        pixelSize: Math.round(AedstTokens.typographyBodyMediumFontSize * textScale),
        weight: AedstTokens.typographyBodyMediumFontWeight
    })
    readonly property font smallFont: Qt.font({
        pixelSize: Math.round(AedstTokens.typographySmallFontSize * textScale),
        weight: AedstTokens.typographySmallFontWeight
    })
    readonly property font captionFont: Qt.font({
        pixelSize: Math.round(AedstTokens.typographyBodySmallFontSize * textScale),
        weight: AedstTokens.typographyBodySmallFontWeight
    })
    readonly property font monoFont: Qt.font({
        pixelSize: Math.round(AedstTokens.typographyMonoFontSize * textScale),
        weight: AedstTokens.typographyMonoFontWeight,
        family: "monospace"
    })

    // --- Motion ------------------------------------------------------------
    readonly property int durationFast: AedstTokens.motionDurationFast
    readonly property int durationNormal: AedstTokens.motionDurationNormal
    readonly property int durationSlow: AedstTokens.motionDurationSlow

    // --- Back-compat aliases (pre-existing token names; keep AedstButton /
    // AedstTextField and App/LoginView.qml usages source-compatible) --------
    readonly property color surface: card
    readonly property color onPrimary: primaryForeground
    readonly property color onSurface: foreground
    readonly property color error: destructive
    readonly property color outline: border
    readonly property int spacing: spacingSm
    readonly property int radius: radiusLg
}
