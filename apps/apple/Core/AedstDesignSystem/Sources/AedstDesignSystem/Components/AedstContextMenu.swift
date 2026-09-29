import SwiftUI

/// `.contextMenu()` wrapper matching shadcn/ui `ContextMenu` — reuses
/// `AedstMenuAction` so the same action list can back a `DropdownMenu` or a
/// long-press context menu without duplicating call sites.
public struct AedstContextMenuModifier: ViewModifier {
    private let actions: [AedstMenuAction]

    public init(actions: [AedstMenuAction]) {
        self.actions = actions
    }

    public func body(content: Content) -> some View {
        content.contextMenu {
            ForEach(actions) { action in
                Button(role: action.isDestructive ? .destructive : nil, action: action.action) {
                    if let systemImage = action.systemImage {
                        Label(action.title, systemImage: systemImage)
                    } else {
                        Text(action.title)
                    }
                }
            }
        }
    }
}

extension View {
    public func aedstContextMenu(actions: [AedstMenuAction]) -> some View {
        modifier(AedstContextMenuModifier(actions: actions))
    }
}

#Preview("AedstContextMenu") {
    Text("Long-press me")
        .padding()
        .aedstContextMenu(actions: [
            AedstMenuAction("Copy", systemImage: "doc.on.doc") {},
            AedstMenuAction("Delete", systemImage: "trash", isDestructive: true) {},
        ])
}
