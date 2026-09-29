import SwiftUI

/// One menu item, matching shadcn/ui `DropdownMenuItem`.
public struct AedstMenuAction: Identifiable {
    public let id = UUID()
    public let title: String
    public let systemImage: String?
    public let isDestructive: Bool
    public let action: () -> Void

    public init(_ title: String, systemImage: String? = nil, isDestructive: Bool = false, action: @escaping () -> Void) {
        self.title = title
        self.systemImage = systemImage
        self.isDestructive = isDestructive
        self.action = action
    }
}

/// `Menu` wrapper matching shadcn/ui `DropdownMenu` — a styled trigger
/// button that opens the platform-native menu presentation.
public struct AedstDropdownMenu: View {
    private let label: String
    private let systemImage: String?
    private let actions: [AedstMenuAction]

    public init(_ label: String, systemImage: String? = "ellipsis", actions: [AedstMenuAction]) {
        self.label = label
        self.systemImage = systemImage
        self.actions = actions
    }

    public var body: some View {
        Menu {
            ForEach(actions) { action in
                Button(role: action.isDestructive ? .destructive : nil, action: action.action) {
                    if let systemImage = action.systemImage {
                        Label(action.title, systemImage: systemImage)
                    } else {
                        Text(action.title)
                    }
                }
            }
        } label: {
            if let systemImage {
                Image(systemName: systemImage)
                    .frame(width: 32, height: 32)
            } else {
                Text(label)
            }
        }
        .accessibilityLabel(Text(label))
    }
}

#Preview("AedstDropdownMenu") {
    AedstDropdownMenu("Row actions", actions: [
        AedstMenuAction("Rename", systemImage: "pencil") {},
        AedstMenuAction("Delete", systemImage: "trash", isDestructive: true) {},
    ])
    .padding()
}
