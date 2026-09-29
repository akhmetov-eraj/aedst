import SwiftUI

/// Custom tooltip matching shadcn/ui `Tooltip` — SwiftUI has limited native
/// tooltip support (macOS `.help()` only), so this uses `.popover` on
/// macOS/iPadOS hover-equivalents and a tap-triggered popover on iOS.
public struct AedstTooltipModifier: ViewModifier {
    private let text: String
    @State private var isPresented = false

    public init(text: String) {
        self.text = text
    }

    public func body(content: Content) -> some View {
        content
            #if os(macOS)
            .help(text)
            #endif
            .onTapGesture { isPresented.toggle() }
            .popover(isPresented: $isPresented, arrowEdge: .top) {
                Text(text)
                    .font(AedstTypography.caption)
                    .padding(.horizontal, AedstSpacing.sm)
                    .padding(.vertical, AedstSpacing.xs)
                    .background(AedstColor.foreground)
                    .foregroundStyle(AedstColor.background)
                    .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
                    .presentationCompactAdaptation(.popover)
            }
            .accessibilityHint(Text(text))
    }
}

extension View {
    public func aedstTooltip(_ text: String) -> some View {
        modifier(AedstTooltipModifier(text: text))
    }
}

#Preview("AedstTooltip") {
    Image(systemName: "info.circle")
        .aedstTooltip("This is additional context.")
        .padding()
}
