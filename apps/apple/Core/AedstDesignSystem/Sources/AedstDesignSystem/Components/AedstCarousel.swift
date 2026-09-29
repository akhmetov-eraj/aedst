import SwiftUI

/// Paged carousel matching shadcn/ui `Carousel` — a `TabView` with
/// `.page` style, which is the idiomatic SwiftUI equivalent of a
/// swipeable, dot-indexed carousel.
public struct AedstCarousel<Item: Identifiable, ItemContent: View>: View {
    private let items: [Item]
    private let itemContent: (Item) -> ItemContent
    @State private var selection: Item.ID?

    public init(items: [Item], @ViewBuilder itemContent: @escaping (Item) -> ItemContent) {
        self.items = items
        self.itemContent = itemContent
    }

    public var body: some View {
        TabView(selection: $selection) {
            ForEach(items) { item in
                itemContent(item)
                    .tag(Optional(item.id))
            }
        }
        #if os(iOS)
        .tabViewStyle(.page(indexDisplayMode: .automatic))
        #endif
        #if !os(macOS)
        .indexViewStyle(.page(backgroundDisplayMode: .always))
        #endif
    }
}

#Preview("AedstCarousel") {
    struct Slide: Identifiable { let id: Int; let color: Color }
    let slides = [Slide(id: 0, color: .blue), Slide(id: 1, color: .green), Slide(id: 2, color: .orange)]
    return AedstCarousel(items: slides) { slide in
        RoundedRectangle(cornerRadius: AedstRadius.lg, style: .continuous)
            .fill(slide.color.opacity(0.3))
            .padding()
    }
    .frame(height: 200)
}
