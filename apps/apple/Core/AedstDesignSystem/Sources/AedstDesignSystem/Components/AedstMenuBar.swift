import SwiftUI

/// macOS `CommandMenu`-friendly action group matching shadcn/ui `Menubar`.
/// Only meaningful on macOS, where a menu bar exists — on other platforms
/// it renders as a regular `Menu` so the same declaration compiles
/// everywhere.
public struct AedstMenuBarSection: Identifiable {
    public let id = UUID()
    public let title: String
    public let actions: [AedstMenuAction]

    public init(_ title: String, actions: [AedstMenuAction]) {
        self.title = title
        self.actions = actions
    }
}

public struct AedstMenuBar: View {
    private let sections: [AedstMenuBarSection]

    public init(sections: [AedstMenuBarSection]) {
        self.sections = sections
    }

    public var body: some View {
        HStack(spacing: AedstSpacing.lg) {
            ForEach(sections) { section in
                Menu(section.title) {
                    ForEach(section.actions) { action in
                        Button(role: action.isDestructive ? .destructive : nil, action: action.action) {
                            Text(action.title)
                        }
                    }
                }
                #if os(macOS)
                .menuStyle(.borderlessButton)
                .fixedSize()
                #endif
            }
        }
        .font(AedstTypography.body)
    }
}

#Preview("AedstMenuBar") {
    AedstMenuBar(sections: [
        AedstMenuBarSection("File", actions: [AedstMenuAction("New") {}, AedstMenuAction("Open…") {}]),
        AedstMenuBarSection("Edit", actions: [AedstMenuAction("Undo") {}]),
    ])
    .padding()
}
