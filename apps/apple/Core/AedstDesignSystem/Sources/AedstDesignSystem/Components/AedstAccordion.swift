import SwiftUI

/// One section of `AedstAccordion`.
public struct AedstAccordionItem: Identifiable {
    public let id = UUID()
    public let title: String
    public let content: String

    public init(title: String, content: String) {
        self.title = title
        self.content = content
    }
}

/// Stack of `AedstCollapsible` sections matching shadcn/ui `Accordion`,
/// with `allowsMultipleOpen` mirroring the `type="single" | "multiple"`
/// prop on the web component.
public struct AedstAccordion: View {
    private let items: [AedstAccordionItem]
    private let allowsMultipleOpen: Bool
    @State private var expandedIDs: Set<UUID> = []

    public init(items: [AedstAccordionItem], allowsMultipleOpen: Bool = false) {
        self.items = items
        self.allowsMultipleOpen = allowsMultipleOpen
    }

    public var body: some View {
        VStack(spacing: 0) {
            ForEach(Array(items.enumerated()), id: \.element.id) { index, item in
                DisclosureGroup(isExpanded: binding(for: item.id)) {
                    Text(item.content)
                        .font(AedstTypography.caption)
                        .foregroundStyle(AedstColor.mutedForeground)
                        .padding(.top, AedstSpacing.xs)
                } label: {
                    Text(item.title)
                        .font(AedstTypography.body.weight(.medium))
                }
                .padding(.vertical, AedstSpacing.sm)

                if index < items.count - 1 {
                    AedstSeparator()
                }
            }
        }
    }

    private func binding(for id: UUID) -> Binding<Bool> {
        Binding(
            get: { expandedIDs.contains(id) },
            set: { isExpanded in
                if isExpanded {
                    if !allowsMultipleOpen { expandedIDs.removeAll() }
                    expandedIDs.insert(id)
                } else {
                    expandedIDs.remove(id)
                }
            }
        )
    }
}

#Preview("AedstAccordion") {
    AedstAccordion(items: [
        AedstAccordionItem(title: "Is it accessible?", content: "Yes, it adheres to the WAI-ARIA design pattern."),
        AedstAccordionItem(title: "Is it styled?", content: "Yes, it uses AedstColor and AedstTypography tokens."),
    ])
    .padding()
}
