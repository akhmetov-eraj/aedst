import SwiftUI

/// Semantic text styles — screens pick `.aedstTitle`/`.aedstBody`, never a
/// raw point size, so a single scale change here reflows every feature.
///
/// Uses `Font.system(_:design:).weight(_:)` rather than `Font.system(_:design:weight:)`
/// (iOS 16 / macOS 13 only) to stay within the iOS 15 / macOS 12 floor.
public enum AedstTypography {
    public static let largeTitle = Font.system(.largeTitle, design: .rounded).weight(.bold)
    public static let title = Font.system(.title2, design: .rounded).weight(.semibold)
    public static let headline = Font.system(.headline, design: .default).weight(.semibold)
    public static let body = Font.system(.body, design: .default).weight(.regular)
    public static let caption = Font.system(.caption, design: .default).weight(.regular)
}

extension View {
    public func aedstFont(_ font: Font) -> some View {
        self.font(font)
    }
}
