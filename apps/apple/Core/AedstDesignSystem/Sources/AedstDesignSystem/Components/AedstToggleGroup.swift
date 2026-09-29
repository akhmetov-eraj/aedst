import SwiftUI

/// Segmented set of `AedstToggleButton`s sharing one selection, matching
/// shadcn/ui `ToggleGroup`. Generic over any `Hashable` option so screens
/// don't need a bespoke enum-to-string bridge.
public struct AedstToggleGroup<Option: Hashable>: View {
    private let options: [Option]
    private let label: (Option) -> String
    @Binding private var selection: Option

    public init(options: [Option], selection: Binding<Option>, label: @escaping (Option) -> String) {
        self.options = options
        self._selection = selection
        self.label = label
    }

    public var body: some View {
        HStack(spacing: AedstSpacing.xxs) {
            ForEach(options, id: \.self) { option in
                Button {
                    selection = option
                } label: {
                    Text(label(option))
                        .font(AedstTypography.caption.weight(.medium))
                        .padding(.horizontal, AedstSpacing.sm)
                        .frame(minHeight: 36)
                        .background(selection == option ? AedstColor.background : Color.clear)
                        .foregroundStyle(selection == option ? AedstColor.foreground : AedstColor.mutedForeground)
                        .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
                }
                #if os(macOS)
                .buttonStyle(.plain)
                #endif
                .accessibilityAddTraits(selection == option ? [.isButton, .isSelected] : .isButton)
            }
        }
        .padding(AedstSpacing.xxs)
        .background(AedstColor.muted)
        .clipShape(RoundedRectangle(cornerRadius: AedstRadius.md, style: .continuous))
    }
}

#Preview("AedstToggleGroup") {
    @Previewable @State var selection = "week"
    AedstToggleGroup(options: ["day", "week", "month"], selection: $selection) { $0.capitalized }
        .padding()
}
