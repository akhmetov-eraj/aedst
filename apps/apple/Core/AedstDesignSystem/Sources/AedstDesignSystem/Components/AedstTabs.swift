import SwiftUI

/// Segmented tab switcher matching shadcn/ui `Tabs` — a custom
/// underline-indicator row rather than the native bottom `TabView`
/// (reserved for `AedstTabBar`'s top-level navigation), so it can sit
/// inline within a screen.
public struct AedstTabs<Content: View>: View {
    private let items: [AedstTabItem]
    @Binding private var selection: String
    private let content: (AedstTabItem) -> Content

    public init(items: [AedstTabItem], selection: Binding<String>, @ViewBuilder content: @escaping (AedstTabItem) -> Content) {
        self.items = items
        self._selection = selection
        self.content = content
    }

    public var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: AedstSpacing.md) {
                ForEach(items) { item in
                    Button {
                        withAnimation(.easeOut(duration: 0.2)) { selection = item.id }
                    } label: {
                        VStack(spacing: AedstSpacing.xxs) {
                            Text(item.title)
                                .font(AedstTypography.body.weight(selection == item.id ? .semibold : .regular))
                                .foregroundStyle(selection == item.id ? AedstColor.foreground : AedstColor.mutedForeground)
                            Rectangle()
                                .fill(selection == item.id ? AedstColor.primary : .clear)
                                .frame(height: 2)
                        }
                    }
                    #if os(macOS)
                    .buttonStyle(.plain)
                    #endif
                    .accessibilityAddTraits(selection == item.id ? [.isButton, .isSelected] : .isButton)
                }
            }

            if let activeItem = items.first(where: { $0.id == selection }) {
                content(activeItem)
                    .padding(.top, AedstSpacing.sm)
            }
        }
    }
}

#Preview("AedstTabs") {
    AedstTabs(
        items: [
            AedstTabItem(id: "overview", title: "Overview", systemImage: ""),
            AedstTabItem(id: "activity", title: "Activity", systemImage: ""),
        ],
        selection: .constant("overview")
    ) { item in
        Text("Content for \(item.title)")
    }
    .padding()
}
