import SwiftUI

/// Multi-box one-time-passcode input matching shadcn/ui `InputOTP`. Backed
/// by a single hidden `TextField` for keyboard/paste handling, rendering a
/// box per character so it looks native to each platform.
public struct AedstOTPInput: View {
    private let length: Int
    @Binding private var code: String
    @FocusState private var isFocused: Bool

    public init(length: Int = 6, code: Binding<String>) {
        self.length = length
        self._code = code
    }

    public var body: some View {
        ZStack {
            TextField("", text: $code)
                #if os(iOS)
                .keyboardType(.numberPad)
                .textContentType(.oneTimeCode)
                #endif
                .focused($isFocused)
                .opacity(0.01)
                .onChange(of: code) { newValue in
                    let digits = newValue.filter(\.isNumber)
                    code = String(digits.prefix(length))
                }

            HStack(spacing: AedstSpacing.xs) {
                ForEach(0..<length, id: \.self) { index in
                    let character = character(at: index)
                    Text(character)
                        .font(AedstTypography.headline)
                        .frame(width: 40, height: 48)
                        .background(AedstColor.background)
                        .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
                        .overlay {
                            RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous)
                                .strokeBorder(
                                    isFocused && index == code.count ? AedstColor.ring : AedstColor.border,
                                    lineWidth: isFocused && index == code.count ? 2 : 1
                                )
                        }
                }
            }
            .allowsHitTesting(false)
        }
        .contentShape(Rectangle())
        .onTapGesture { isFocused = true }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(Text(AedstStrings.oneTimePasscode))
        .accessibilityValue(Text(code.isEmpty ? AedstStrings.empty : code))
    }

    private func character(at index: Int) -> String {
        guard index < code.count else { return "" }
        return String(Array(code)[index])
    }
}

#Preview("AedstOTPInput") {
    AedstOTPInput(length: 6, code: .constant("12"))
        .padding()
}
