import SwiftUI

/// Menu-based picker matching shadcn/ui `Select` — a bordered trigger row
/// that opens a native `Menu`, so it gets platform-correct presentation
/// (sheet on iOS, popover on macOS) for free.
public struct AedstSelect<Option: Hashable>: View {
    private let label: String?
    private let options: [Option]
    private let optionLabel: (Option) -> String
    @Binding private var selection: Option

    public init(
        _ label: String? = nil,
        options: [Option],
        selection: Binding<Option>,
        optionLabel: @escaping (Option) -> String
    ) {
        self.label = label
        self.options = options
        self._selection = selection
        self.optionLabel = optionLabel
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
            if let label {
                AedstLabel(label)
            }
            Menu {
                ForEach(options, id: \.self) { option in
                    Button {
                        selection = option
                    } label: {
                        if option == selection {
                            Label(optionLabel(option), systemImage: "checkmark")
                        } else {
                            Text(optionLabel(option))
                        }
                    }
                }
            } label: {
                HStack {
                    Text(optionLabel(selection))
                        .font(AedstTypography.body)
                        .foregroundStyle(AedstColor.foreground)
                    Spacer()
                    Image(systemName: "chevron.up.chevron.down")
                        .font(.caption)
                        .foregroundStyle(AedstColor.mutedForeground)
                }
                .padding(.horizontal, AedstSpacing.sm)
                .frame(minHeight: 44)
                .background(AedstColor.background)
                .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
                .overlay {
                    RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous)
                        .strokeBorder(AedstColor.border, lineWidth: 1)
                }
            }
            #if os(macOS)
            .menuStyle(.borderlessButton)
            #endif
            .accessibilityLabel(Text(label ?? AedstStrings.select))
            .accessibilityValue(Text(optionLabel(selection)))
        }
    }
}

#Preview("AedstSelect") {
    AedstSelect("Role", options: ["Admin", "Editor", "Viewer"], selection: .constant("Editor")) { $0 }
        .padding()
}
