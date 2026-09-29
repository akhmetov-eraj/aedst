pragma Singleton

import QtQuick
import Aedst.Theme

// Read-only façade over the C++ ThemeController so views depend on tokens,
// not on the controller type. Semantic names mirror the CSS custom
// properties in apps/web/src/app/styles/globals.css (shadcn/ui token set):
// background/foreground, card, popover, primary, secondary, muted, accent,
// destructive, border/input/ring, plus a radius and spacing scale.
QtObject {
    readonly property bool dark: ThemeController.dark

    // --- Semantic color tokens (web parity) ---------------------------------
    readonly property color background: ThemeController.background
    readonly property color foreground: ThemeController.foreground
    readonly property color card: ThemeController.card
    readonly property color cardForeground: ThemeController.cardForeground
    readonly property color popover: ThemeController.popover
    readonly property color popoverForeground: ThemeController.popoverForeground
    readonly property color primary: ThemeController.primary
    readonly property color primaryForeground: ThemeController.primaryForeground
    readonly property color secondary: ThemeController.secondary
    readonly property color secondaryForeground: ThemeController.secondaryForeground
    readonly property color muted: ThemeController.muted
    readonly property color mutedForeground: ThemeController.mutedForeground
    readonly property color accent: ThemeController.accent
    readonly property color accentForeground: ThemeController.accentForeground
    readonly property color destructive: ThemeController.destructive
    readonly property color destructiveForeground: ThemeController.destructiveForeground
    readonly property color border: ThemeController.border
    readonly property color input: ThemeController.input
    readonly property color ring: ThemeController.ring

    readonly property var chart: [
        ThemeController.chart1,
        ThemeController.chart2,
        ThemeController.chart3,
        ThemeController.chart4,
        ThemeController.chart5
    ]

    // --- Scales --------------------------------------------------------------
    readonly property int radiusSm: ThemeController.radiusSm
    readonly property int radiusMd: ThemeController.radiusMd
    readonly property int radiusLg: ThemeController.radiusLg
    readonly property int radiusXl: ThemeController.radiusXl
    readonly property int radius2xl: ThemeController.radius2xl

    readonly property int spacingXs: ThemeController.spacingXs
    readonly property int spacingSm: ThemeController.spacingSm
    readonly property int spacingMd: ThemeController.spacingMd
    readonly property int spacingLg: ThemeController.spacingLg
    readonly property int spacingXl: ThemeController.spacingXl
    readonly property int spacing2xl: ThemeController.spacing2xl

    // --- Typography ------------------------------------------------------------
    readonly property font titleFont: Qt.font({ pixelSize: 24, weight: Font.DemiBold })
    readonly property font headingFont: Qt.font({ pixelSize: 18, weight: Font.DemiBold })
    readonly property font bodyFont: Qt.font({ pixelSize: 14 })
    readonly property font smallFont: Qt.font({ pixelSize: 13 })
    readonly property font captionFont: Qt.font({ pixelSize: 12 })
    readonly property font monoFont: Qt.font({ pixelSize: 13, family: "monospace" })

    // --- Motion ------------------------------------------------------------
    readonly property int durationFast: 100
    readonly property int durationNormal: 160
    readonly property int durationSlow: 240

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
