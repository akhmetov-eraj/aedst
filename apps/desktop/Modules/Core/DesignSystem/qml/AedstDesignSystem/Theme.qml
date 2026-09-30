pragma Singleton

import QtQuick
import Aedst.Theme

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

    readonly property int spacingXs: AedstTokens.spacingXs
    readonly property int spacingSm: AedstTokens.spacingSm
    readonly property int spacingMd: AedstTokens.spacingMd
    readonly property int spacingLg: AedstTokens.spacingLg
    readonly property int spacingXl: AedstTokens.spacingXxl
    readonly property int spacing2xl: AedstTokens.spacingXxxl

    // --- Typography ------------------------------------------------------------
    readonly property font titleFont: Qt.font({
        pixelSize: AedstTokens.typographyHeadlineMediumFontSize,
        weight: AedstTokens.typographyHeadlineMediumFontWeight
    })
    readonly property font headingFont: Qt.font({
        pixelSize: AedstTokens.typographyTitleMediumFontSize,
        weight: AedstTokens.typographyTitleMediumFontWeight
    })
    readonly property font bodyFont: Qt.font({
        pixelSize: AedstTokens.typographyBodyMediumFontSize,
        weight: AedstTokens.typographyBodyMediumFontWeight
    })
    readonly property font smallFont: Qt.font({
        pixelSize: AedstTokens.typographySmallFontSize,
        weight: AedstTokens.typographySmallFontWeight
    })
    readonly property font captionFont: Qt.font({
        pixelSize: AedstTokens.typographyBodySmallFontSize,
        weight: AedstTokens.typographyBodySmallFontWeight
    })
    readonly property font monoFont: Qt.font({
        pixelSize: AedstTokens.typographyMonoFontSize,
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
