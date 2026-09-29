import SwiftUI

/// Custom side-sheet matching shadcn/ui `Drawer` — slides in from a screen
/// edge, better suited than `.sheet` for macOS/iPad where a bottom sheet
/// feels out of place for auxiliary panels (filters, inspectors).
public struct AedstDrawerModifier<DrawerContent: View>: ViewModifier {
    public enum Edge {
        case leading
        case trailing
    }

    @Binding private var isPresented: Bool
    private let edge: Edge
    private let width: Double
    private let drawerContent: DrawerContent

    public init(
        isPresented: Binding<Bool>,
        edge: Edge = .trailing,
        width: Double = 320,
        @ViewBuilder content: () -> DrawerContent
    ) {
        self._isPresented = isPresented
        self.edge = edge
        self.width = width
        self.drawerContent = content()
    }

    public func body(content: Content) -> some View {
        content.overlay {
            if isPresented {
                ZStack(alignment: edge == .trailing ? .trailing : .leading) {
                    Color.black.opacity(0.3)
                        .ignoresSafeArea()
                        .onTapGesture { isPresented = false }

                    drawerContent
                        .frame(width: width)
                        .frame(maxHeight: .infinity)
                        .background(AedstColor.popover)
                        .foregroundStyle(AedstColor.popoverForeground)
                        .shadow(radius: 16)
                        .ignoresSafeArea(edges: .vertical)
                        .transition(.move(edge: edge == .trailing ? .trailing : .leading))
                }
                .transition(.opacity)
            }
        }
        .animation(.easeOut(duration: 0.25), value: isPresented)
    }
}

extension View {
    public func aedstDrawer<Content: View>(
        isPresented: Binding<Bool>,
        edge: AedstDrawerModifier<Content>.Edge = .trailing,
        width: Double = 320,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        modifier(AedstDrawerModifier(isPresented: isPresented, edge: edge, width: width, content: content))
    }
}

#Preview("AedstDrawer") {
    @Previewable @State var isPresented = true
    Color.clear
        .aedstDrawer(isPresented: $isPresented) {
            VStack(alignment: .leading, spacing: AedstSpacing.md) {
                Text("Inspector").font(AedstTypography.title)
                Spacer()
            }
            .padding()
        }
}
