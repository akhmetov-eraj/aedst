import SwiftUI

/// One tab item, matching shadcn/ui `NavigationMenu`/`Tabs` item shape.
public struct AedstTabItem: Identifiable, Hashable {
    public let id: String
    public let title: String
    public let systemImage: String

    public init(id: String, title: String, systemImage: String) {
        self.id = id
        self.title = title
        self.systemImage = systemImage
    }
}

/// Top-level app navigation matching shadcn/ui `NavigationMenu` — adapted
/// to the native `TabView` bottom-bar pattern on iOS rather than a literal
/// horizontal menu bar, since that's the HIG-correct primary navigation.
public struct AedstTabBar<Content: View>: View {
    private let items: [AedstTabItem]
    @Binding private var selection: String
    private let content: (AedstTabItem) -> Content

    public init(items: [AedstTabItem], selection: Binding<String>, @ViewBuilder content: @escaping (AedstTabItem) -> Content) {
        self.items = items
        self._selection = selection
        self.content = content
    }

    public var body: some View {
        TabView(selection: $selection) {
            ForEach(items) { item in
                content(item)
                    .tabItem {
                        Label(item.title, systemImage: item.systemImage)
                    }
                    .tag(item.id)
            }
        }
        .tint(AedstColor.primary)
    }
}

#Preview("AedstTabBar") {
    AedstTabBar(
        items: [
            AedstTabItem(id: "home", title: "Home", systemImage: "house"),
            AedstTabItem(id: "settings", title: "Settings", systemImage: "gearshape"),
        ],
        selection: .constant("home")
    ) { item in
        Text(item.title)
    }
}
