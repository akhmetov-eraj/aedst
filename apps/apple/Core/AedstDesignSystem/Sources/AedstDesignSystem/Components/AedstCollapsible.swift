import SwiftUI

/// `DisclosureGroup` wrapper matching shadcn/ui `Collapsible` — styled
/// trigger row with a chevron so every expandable section looks the same.
public struct AedstCollapsible<Content: View>: View {
    private let title: String
    @State private var isExpanded: Bool
    private let content: Content

    public init(_ title: String, isExpanded: Bool = false, @ViewBuilder content: () -> Content) {
        self.title = title
        self._isExpanded = State(initialValue: isExpanded)
        self.content = content()
    }

    public var body: some View {
        DisclosureGroup(isExpanded: $isExpanded) {
            content
                .padding(.top, AedstSpacing.xs)
        } label: {
            Text(title)
                .font(AedstTypography.body.weight(.medium))
                .foregroundStyle(AedstColor.foreground)
        }
        .accessibilityHint(Text(isExpanded ? "Collapses \(title)" : "Expands \(title)"))
    }
}

#Preview("AedstCollapsible") {
    AedstCollapsible("What's included") {
        Text("Priority support, unlimited projects, and API access.")
            .font(AedstTypography.caption)
            .foregroundStyle(AedstColor.mutedForeground)
    }
    .padding()
}
