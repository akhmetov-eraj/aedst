import SwiftUI

/// Two-state pressable button matching shadcn/ui `Toggle` — visually a
/// button, semantically a `Bool` binding (unlike `AedstSwitch`, which reads
/// as an on/off setting).
public struct AedstToggleButton: View {
    private let title: String
    private let systemImage: String?
    @Binding private var isOn: Bool

    public init(_ title: String, systemImage: String? = nil, isOn: Binding<Bool>) {
        self.title = title
        self.systemImage = systemImage
        self._isOn = isOn
    }

    public var body: some View {
        Button {
            isOn.toggle()
        } label: {
            HStack(spacing: AedstSpacing.xs) {
                if let systemImage {
                    Image(systemName: systemImage)
                }
                Text(title)
            }
            .font(AedstTypography.body.weight(.medium))
            .padding(.horizontal, AedstSpacing.sm)
            .frame(minHeight: 40)
            .background(isOn ? AedstColor.accent : Color.clear)
            .foregroundStyle(isOn ? AedstColor.accentForeground : AedstColor.mutedForeground)
            .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
        }
        #if os(macOS)
        .buttonStyle(.plain)
        #endif
        .accessibilityAddTraits(.isButton)
        .accessibilityValue(Text(isOn ? "On" : "Off"))
    }
}

#Preview("AedstToggleButton") {
    @Previewable @State var isBold = true
    HStack {
        AedstToggleButton("Bold", systemImage: "bold", isOn: $isBold)
        AedstToggleButton("Italic", systemImage: "italic", isOn: .constant(false))
    }
    .padding()
}
