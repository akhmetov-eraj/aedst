import SwiftUI

/// `AspectRatio` wrapper matching shadcn/ui's `AspectRatio` — a
/// `ViewModifier` so it composes with `.clipShape`/`.overlay` like any
/// other SwiftUI modifier instead of a bespoke container.
public struct AedstAspectRatioModifier: ViewModifier {
    private let ratio: Double
    private let contentMode: ContentMode

    public init(ratio: Double, contentMode: ContentMode = .fill) {
        self.ratio = ratio
        self.contentMode = contentMode
    }

    public func body(content: Content) -> some View {
        content
            .aspectRatio(ratio, contentMode: contentMode)
    }
}

extension View {
    /// - Parameter ratio: width / height, e.g. `16.0 / 9.0`.
    public func aedstAspectRatio(_ ratio: Double, contentMode: ContentMode = .fill) -> some View {
        modifier(AedstAspectRatioModifier(ratio: ratio, contentMode: contentMode))
    }
}

#Preview("AedstAspectRatio") {
    RoundedRectangle(cornerRadius: AedstRadius.md, style: .continuous)
        .fill(AedstColor.muted)
        .aedstAspectRatio(16.0 / 9.0)
        .clipShape(RoundedRectangle(cornerRadius: AedstRadius.md, style: .continuous))
        .padding()
}
