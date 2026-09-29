import SwiftUI

/// `.sheet()` wrapper matching shadcn/ui `Sheet` — thin convenience so
/// screens get consistent detents/drag-indicator behavior instead of
/// re-deriving it per call site.
public struct AedstSheetModifier<SheetContent: View>: ViewModifier {
    @Binding private var isPresented: Bool
    private let detents: Set<PresentationDetent>
    private let sheetContent: SheetContent

    public init(
        isPresented: Binding<Bool>,
        detents: Set<PresentationDetent> = [.medium, .large],
        @ViewBuilder content: () -> SheetContent
    ) {
        self._isPresented = isPresented
        self.detents = detents
        self.sheetContent = content()
    }

    public func body(content: Content) -> some View {
        content.sheet(isPresented: $isPresented) {
            sheetContent
                .presentationDetents(detents)
                .presentationDragIndicator(.visible)
                .presentationBackground(AedstColor.background)
        }
    }
}

extension View {
    public func aedstSheet<Content: View>(
        isPresented: Binding<Bool>,
        detents: Set<PresentationDetent> = [.medium, .large],
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        modifier(AedstSheetModifier(isPresented: isPresented, detents: detents, content: content))
    }
}

#Preview("AedstSheet") {
    @Previewable @State var isPresented = true
    Color.clear
        .aedstSheet(isPresented: $isPresented) {
            VStack(spacing: AedstSpacing.md) {
                Text("Filters").font(AedstTypography.title)
                AedstButton("Apply") {}
            }
            .padding()
        }
}
