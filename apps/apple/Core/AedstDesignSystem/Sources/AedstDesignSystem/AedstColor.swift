import SwiftUI
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

/// Semantic color tokens, one adaptive `Color` per name — the Swift
/// counterpart to the Compose `MaterialTheme.colorScheme` wiring in
/// `apps/android/core/ui` and the CSS custom properties in
/// `apps/web/src/app/styles/globals.css`. Backed by dynamic light/dark
/// initializers rather than an asset catalog so the token names stay
/// greppable from Swift; swap for `Color("primary", bundle: .module)`
/// backed by `Resources/Colors.xcassets` if designers want to own these via
/// Figma tokens instead.
///
/// Names and pairing mirror web's token set 1:1 (`background`/`foreground`,
/// `primary`/`primaryForeground`, ...) so a design spec reads the same
/// token name on every platform.
public enum AedstColor {
    public static let background = adaptive(light: "#ffffff", dark: "#0a0a0a")
    public static let foreground = adaptive(light: "#0a0a0a", dark: "#fafafa")

    public static let card = adaptive(light: "#ffffff", dark: "#171717")
    public static let cardForeground = adaptive(light: "#0a0a0a", dark: "#fafafa")

    public static let popover = adaptive(light: "#ffffff", dark: "#171717")
    public static let popoverForeground = adaptive(light: "#0a0a0a", dark: "#fafafa")

    public static let primary = adaptive(light: "#171717", dark: "#e5e5e5")
    public static let primaryForeground = adaptive(light: "#fafafa", dark: "#171717")

    public static let secondary = adaptive(light: "#f5f5f5", dark: "#262626")
    public static let secondaryForeground = adaptive(light: "#171717", dark: "#fafafa")

    public static let muted = adaptive(light: "#f5f5f5", dark: "#262626")
    public static let mutedForeground = adaptive(light: "#737373", dark: "#a1a1a1")

    public static let accent = adaptive(light: "#f5f5f5", dark: "#262626")
    public static let accentForeground = adaptive(light: "#171717", dark: "#fafafa")

    public static let destructive = adaptive(light: "#e7000b", dark: "#ff6467")
    public static let destructiveForeground = adaptive(light: "#fafafa", dark: "#171717")

    public static let success = adaptive(light: "#15803d", dark: "#4ade80")
    public static let successForeground = adaptive(light: "#fafafa", dark: "#052e16")

    public static let warning = adaptive(light: "#b45309", dark: "#fbbf24")
    public static let warningForeground = adaptive(light: "#fafafa", dark: "#451a03")

    public static let border = adaptive(light: "#e5e5e5", dark: Color.white.opacity(0.1))
    public static let input = adaptive(light: "#e5e5e5", dark: Color.white.opacity(0.15))
    public static let ring = adaptive(light: "#a1a1a1", dark: "#8a8a8a")

    public static let chart1 = adaptive(light: "#dedede", dark: "#dedede")
    public static let chart2 = adaptive(light: "#8a8a8a", dark: "#8a8a8a")
    public static let chart3 = adaptive(light: "#6b6b6b", dark: "#6b6b6b")
    public static let chart4 = adaptive(light: "#5a5a5a", dark: "#5a5a5a")
    public static let chart5 = adaptive(light: "#383838", dark: "#383838")

    /// Ordered palette for chart series that need to cycle through colors.
    public static let chartSeries: [Color] = [chart1, chart2, chart3, chart4, chart5]

    // MARK: - Legacy aliases (kept for `AedstButton`/`AedstTextField` call sites during migration)

    @available(*, deprecated, renamed: "primary")
    public static let brandPrimary = primary
    @available(*, deprecated, renamed: "accent")
    public static let brandSecondary = accent
    @available(*, deprecated, renamed: "muted")
    public static let surface = muted
    @available(*, deprecated, renamed: "foreground")
    public static let textPrimary = foreground
    @available(*, deprecated, renamed: "mutedForeground")
    public static let textSecondary = mutedForeground
    @available(*, deprecated, renamed: "destructive")
    public static let danger = destructive

    private static func adaptive(light: String, dark: String) -> Color {
        Color(light: Color(hex: light), dark: Color(hex: dark))
    }

    private static func adaptive(light: String, dark: Color) -> Color {
        Color(light: Color(hex: light), dark: dark)
    }
}

extension Color {
    /// `UITraitCollection`/`NSAppearance`-driven light/dark pair, so a single
    /// token works unmodified on iOS, macOS, watchOS and tvOS.
    init(light: Color, dark: Color) {
        #if canImport(UIKit)
        self = Color(uiColor: UIColor { traits in
            traits.userInterfaceStyle == .dark ? UIColor(dark) : UIColor(light)
        })
        #elseif canImport(AppKit)
        self = Color(nsColor: NSColor(name: nil) { appearance in
            appearance.bestMatch(from: [.darkAqua, .aqua]) == .darkAqua ? NSColor(dark) : NSColor(light)
        })
        #else
        self = light
        #endif
    }

    /// `#rrggbb` hex literal, used to port the design tokens from
    /// `apps/web`'s CSS custom properties without hand-converting OKLCH.
    init(hex: String) {
        var sanitized = hex.trimmingCharacters(in: .whitespacesAndNewlines)
        sanitized.removeAll { $0 == "#" }
        var value: UInt64 = 0
        Scanner(string: sanitized).scanHexInt64(&value)
        let r = Double((value & 0xFF0000) >> 16) / 255
        let g = Double((value & 0x00FF00) >> 8) / 255
        let b = Double(value & 0x0000FF) / 255
        self = Color(red: r, green: g, blue: b)
    }
}
