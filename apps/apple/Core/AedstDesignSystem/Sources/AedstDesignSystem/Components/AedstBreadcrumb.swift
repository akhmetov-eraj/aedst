import SwiftUI

/// One crumb, matching shadcn/ui `BreadcrumbItem`.
public struct AedstBreadcrumbItem: Identifiable {
    public let id = UUID()
    public let title: String
    public let action: (() -> Void)?

    public init(title: String, action: (() -> Void)? = nil) {
        self.title = title
        self.action = action
    }
}

/// Horizontal path trail matching shadcn/ui `Breadcrumb` — the last item
/// renders as plain (current-page) text, earlier ones as tappable links.
public struct AedstBreadcrumb: View {
    private let items: [AedstBreadcrumbItem]

    public init(items: [AedstBreadcrumbItem]) {
        self.items = items
    }

    public var body: some View {
        HStack(spacing: AedstSpacing.xxs) {
            ForEach(Array(items.enumerated()), id: \.element.id) { index, item in
                if let action = item.action, index != items.count - 1 {
                    Button(action: action) {
                        Text(item.title)
                            .font(AedstTypography.caption)
                            .foregroundStyle(AedstColor.mutedForeground)
                    }
                    #if os(macOS)
                    .buttonStyle(.plain)
                    #endif
                } else {
                    Text(item.title)
                        .font(AedstTypography.caption.weight(.medium))
                        .foregroundStyle(AedstColor.foreground)
                }

                if index != items.count - 1 {
                    Image(systemName: "chevron.right")
                        .font(.caption2)
                        .foregroundStyle(AedstColor.mutedForeground)
                }
            }
        }
        .accessibilityElement(children: .combine)
    }
}

#Preview("AedstBreadcrumb") {
    AedstBreadcrumb(items: [
        AedstBreadcrumbItem(title: "Projects", action: {}),
        AedstBreadcrumbItem(title: "AEDST", action: {}),
        AedstBreadcrumbItem(title: "Settings"),
    ])
    .padding()
}
