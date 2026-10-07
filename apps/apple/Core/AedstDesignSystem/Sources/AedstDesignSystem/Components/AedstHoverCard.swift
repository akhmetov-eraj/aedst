import SwiftUI

/// Preview-on-interaction card matching shadcn/ui `HoverCard`. macOS gets a
/// true `.onHover`-driven popover; other platforms (no pointer/hover
/// concept) fall back to a long-press-triggered popover.
public struct AedstHoverCardModifier<CardContent: View>: ViewModifier {
    private let cardContent: CardContent
    @State private var isPresented = false

    public init(@ViewBuilder content: () -> CardContent) {
        self.cardContent = content()
    }

    public func body(content: Content) -> some View {
        content
            #if os(macOS)
            .onHover { isHovering in isPresented = isHovering }
            #else
            .onLongPressGesture(minimumDuration: 0.35) { isPresented = true }
            #endif
            .popover(isPresented: $isPresented, arrowEdge: .top) {
                cardContent
                    .padding(AedstSpacing.sm)
                    .frame(width: 240)
                    .background(AedstColor.popover)
                    .foregroundStyle(AedstColor.popoverForeground)
                    .aedstPopoverAdaptation()
            }
    }
}

extension View {
    public func aedstHoverCard<Content: View>(@ViewBuilder content: @escaping () -> Content) -> some View {
        modifier(AedstHoverCardModifier(content: content))
    }
}

#Preview("AedstHoverCard") {
    Text("@aedst")
        .font(AedstTypography.body.weight(.semibold))
        .aedstHoverCard {
            VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
                Text("AEDST").font(AedstTypography.headline)
                Text("Task management, on every platform.")
                    .font(AedstTypography.caption)
                    .foregroundStyle(AedstColor.mutedForeground)
            }
        }
        .padding()
}
