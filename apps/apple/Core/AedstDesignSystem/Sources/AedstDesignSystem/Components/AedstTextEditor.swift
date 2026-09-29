import SwiftUI

/// Multi-line text input matching shadcn/ui `Textarea` — a `TextEditor`
/// with the same label/border/error chrome as `AedstTextField` plus a
/// placeholder overlay, since `TextEditor` has no native placeholder.
public struct AedstTextEditor: View {
    private let label: String
    @Binding private var text: String
    private let placeholder: String
    private let errorMessage: String?
    private let minHeight: Double

    public init(
        _ label: String,
        text: Binding<String>,
        placeholder: String = "",
        errorMessage: String? = nil,
        minHeight: Double = 96
    ) {
        self.label = label
        self._text = text
        self.placeholder = placeholder
        self.errorMessage = errorMessage
        self.minHeight = minHeight
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
            AedstLabel(label)

            ZStack(alignment: .topLeading) {
                if text.isEmpty, !placeholder.isEmpty {
                    Text(placeholder)
                        .font(AedstTypography.body)
                        .foregroundStyle(AedstColor.mutedForeground)
                        .padding(.horizontal, AedstSpacing.xs)
                        .padding(.vertical, AedstSpacing.xs)
                        .allowsHitTesting(false)
                }
                TextEditor(text: $text)
                    .font(AedstTypography.body)
                    .scrollContentBackground(.hidden)
                    .padding(.horizontal, AedstSpacing.xs - 4)
            }
            .frame(minHeight: minHeight)
            .padding(AedstSpacing.xxs)
            .background(AedstColor.background)
            .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous)
                    .strokeBorder(errorMessage != nil ? AedstColor.destructive : AedstColor.border, lineWidth: 1)
            }

            if let errorMessage {
                Text(errorMessage)
                    .font(AedstTypography.caption)
                    .foregroundStyle(AedstColor.destructive)
            }
        }
    }
}

#Preview("AedstTextEditor") {
    AedstTextEditor("Notes", text: .constant(""), placeholder: "Add any context...")
        .padding()
}
