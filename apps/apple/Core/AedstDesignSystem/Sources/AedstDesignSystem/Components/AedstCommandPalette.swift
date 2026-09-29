import SwiftUI

/// One command entry, matching shadcn/ui `CommandItem`.
public struct AedstCommand: Identifiable {
    public let id = UUID()
    public let title: String
    public let subtitle: String?
    public let systemImage: String
    public let action: () -> Void

    public init(title: String, subtitle: String? = nil, systemImage: String = "chevron.right", action: @escaping () -> Void) {
        self.title = title
        self.subtitle = subtitle
        self.systemImage = systemImage
        self.action = action
    }
}

/// Searchable action list matching shadcn/ui `Command` (⌘K palette) —
/// implemented as a `.searchable` `List` inside a sheet, since that is the
/// idiomatic SwiftUI equivalent rather than a bespoke overlay.
public struct AedstCommandPalette: View {
    private let commands: [AedstCommand]
    @Binding private var isPresented: Bool
    @State private var query = ""

    public init(commands: [AedstCommand], isPresented: Binding<Bool>) {
        self.commands = commands
        self._isPresented = isPresented
    }

    private var filtered: [AedstCommand] {
        guard !query.isEmpty else { return commands }
        return commands.filter { $0.title.localizedCaseInsensitiveContains(query) }
    }

    public var body: some View {
        NavigationStack {
            List(filtered) { command in
                Button {
                    command.action()
                    isPresented = false
                } label: {
                    Label {
                        VStack(alignment: .leading) {
                            Text(command.title).foregroundStyle(AedstColor.foreground)
                            if let subtitle = command.subtitle {
                                Text(subtitle).font(AedstTypography.caption).foregroundStyle(AedstColor.mutedForeground)
                            }
                        }
                    } icon: {
                        Image(systemName: command.systemImage)
                    }
                }
            }
            .searchable(text: $query, prompt: "Type a command…")
            .navigationTitle("Commands")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
        }
    }
}

#Preview("AedstCommandPalette") {
    AedstCommandPalette(
        commands: [
            AedstCommand(title: "New task", systemImage: "plus") {},
            AedstCommand(title: "Go to settings", systemImage: "gearshape") {},
        ],
        isPresented: .constant(true)
    )
}
