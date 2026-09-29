import SwiftUI

/// `.popover()` wrapper matching shadcn/ui `Popover` — tokenized card
/// background so the popover content doesn't need to restyle itself.
public struct AedstPopoverModifier<PopoverContent: View>: ViewModifier {
    @Binding private var isPresented: Bool
    private let arrowEdge: Edge
    private let popoverContent: PopoverContent

    public init(isPresented: Binding<Bool>, arrowEdge: Edge = .bottom, @ViewBuilder content: () -> PopoverContent) {
        self._isPresented = isPresented
        self.arrowEdge = arrowEdge
        self.popoverContent = content()
    }

    public func body(content: Content) -> some View {
        content.popover(isPresented: $isPresented, arrowEdge: arrowEdge) {
            popoverContent
                .padding(AedstSpacing.sm)
                .background(AedstColor.popover)
                .foregroundStyle(AedstColor.popoverForeground)
                .presentationCompactAdaptation(.popover)
        }
    }
}

extension View {
    public func aedstPopover<Content: View>(
        isPresented: Binding<Bool>,
        arrowEdge: Edge = .bottom,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        modifier(AedstPopoverModifier(isPresented: isPresented, arrowEdge: arrowEdge, content: content))
    }
}

#Preview("AedstPopover") {
    @Previewable @State var isPresented = true
    AedstButton("Show info") { isPresented.toggle() }
        .aedstPopover(isPresented: $isPresented) {
            Text("Additional details go here.").font(AedstTypography.caption)
        }
        .padding()
}
