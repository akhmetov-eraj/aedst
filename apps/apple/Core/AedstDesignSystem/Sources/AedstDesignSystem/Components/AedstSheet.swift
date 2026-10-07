import SwiftUI

/// Height stops a sheet may rest at. Mirrors the two `PresentationDetent`s the
/// design system uses; `PresentationDetent` itself is iOS 16 / macOS 13, so it
/// cannot appear in this package's public API at the iOS 15 / macOS 12 floor.
public enum AedstSheetDetent: Hashable, Sendable {
    /// Half-height stop.
    case medium
    /// Full-height stop.
    case large

    @available(iOS 16, macOS 13, tvOS 16, watchOS 9, *)
    fileprivate var presentationDetent: PresentationDetent {
        switch self {
        case .medium: .medium
        case .large: .large
        }
    }
}

/// Applies detents / drag indicator / background where the OS supports them
/// (iOS 16 / 16.4); on older OS versions the sheet falls back to the system default.
private struct AedstSheetPresentationModifier: ViewModifier {
    let detents: Set<AedstSheetDetent>

    @ViewBuilder
    func body(content: Content) -> some View {
        if #available(iOS 16.4, macOS 13.3, tvOS 16.4, watchOS 9.4, *) {
            content
                .presentationDetents(Set(detents.map(\.presentationDetent)))
                .presentationDragIndicator(.visible)
                .presentationBackground(AedstColor.background)
        } else if #available(iOS 16, macOS 13, tvOS 16, watchOS 9, *) {
            content
                .presentationDetents(Set(detents.map(\.presentationDetent)))
                .presentationDragIndicator(.visible)
        } else {
            content
        }
    }
}

/// `.sheet()` wrapper matching shadcn/ui `Sheet` — thin convenience so
/// screens get consistent detents/drag-indicator behavior instead of
/// re-deriving it per call site.
public struct AedstSheetModifier<SheetContent: View>: ViewModifier {
    @Binding private var isPresented: Bool
    private let detents: Set<AedstSheetDetent>
    private let sheetContent: SheetContent

    public init(
        isPresented: Binding<Bool>,
        detents: Set<AedstSheetDetent> = [.medium, .large],
        @ViewBuilder content: () -> SheetContent
    ) {
        self._isPresented = isPresented
        self.detents = detents
        self.sheetContent = content()
    }

    public func body(content: Content) -> some View {
        content.sheet(isPresented: $isPresented) {
            sheetContent
                .modifier(AedstSheetPresentationModifier(detents: detents))
        }
    }
}

extension View {
    public func aedstSheet<Content: View>(
        isPresented: Binding<Bool>,
        detents: Set<AedstSheetDetent> = [.medium, .large],
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        modifier(AedstSheetModifier(isPresented: isPresented, detents: detents, content: content))
    }
}

#Preview("AedstSheet") {
    @Previewable @State var isPresented = true
    Color.clear
        .aedstSheet(isPresented: $isPresented) {
            VStack(spacing: AedstSpacing.lg) {
                Text("Filters").font(AedstTypography.title)
                AedstButton("Apply") {}
            }
            .padding()
        }
}
