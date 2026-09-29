import SwiftUI

/// Searchable dropdown matching shadcn/ui `Combobox` — a bordered trigger
/// that opens a `.popover` (macOS/iPadOS) or `.sheet` (compact iOS) with a
/// `.searchable` filtered list, since SwiftUI has no built-in
/// filterable-menu control.
public struct AedstCombobox<Option: Hashable & Identifiable>: View {
    private let label: String?
    private let options: [Option]
    private let optionLabel: (Option) -> String
    @Binding private var selection: Option?
    @State private var isPresented = false
    @State private var query = ""

    public init(
        _ label: String? = nil,
        options: [Option],
        selection: Binding<Option?>,
        optionLabel: @escaping (Option) -> String
    ) {
        self.label = label
        self.options = options
        self._selection = selection
        self.optionLabel = optionLabel
    }

    private var filtered: [Option] {
        guard !query.isEmpty else { return options }
        return options.filter { optionLabel($0).localizedCaseInsensitiveContains(query) }
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
            if let label {
                AedstLabel(label)
            }
            Button {
                isPresented = true
            } label: {
                HStack {
                    Text(selection.map(optionLabel) ?? "Select…")
                        .font(AedstTypography.body)
                        .foregroundStyle(selection == nil ? AedstColor.mutedForeground : AedstColor.foreground)
                    Spacer()
                    Image(systemName: "chevron.up.chevron.down")
                        .font(.system(size: 12))
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
            .buttonStyle(.plain)
            #endif
        }
        .sheet(isPresented: $isPresented) {
            NavigationStack {
                List(filtered) { option in
                    Button {
                        selection = option
                        isPresented = false
                    } label: {
                        HStack {
                            Text(optionLabel(option))
                            Spacer()
                            if option == selection {
                                Image(systemName: "checkmark")
                            }
                        }
                    }
                }
                .searchable(text: $query)
                .navigationTitle(label ?? "Select")
                #if os(iOS)
                .navigationBarTitleDisplayMode(.inline)
                #endif
            }
        }
    }
}

#Preview("AedstCombobox") {
    struct Country: Hashable, Identifiable { let id: String; let name: String }
    let countries = [Country(id: "us", name: "United States"), Country(id: "kz", name: "Kazakhstan")]
    return AedstCombobox("Country", options: countries, selection: .constant(countries.first)) { $0.name }
        .padding()
}
