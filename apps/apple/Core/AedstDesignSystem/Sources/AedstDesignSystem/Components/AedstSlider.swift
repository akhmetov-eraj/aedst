import SwiftUI

/// `Slider` wrapper matching shadcn/ui `Slider` — tokenized tint and an
/// optional label/value row above the track.
public struct AedstSlider: View {
    private let label: String?
    @Binding private var value: Double
    private let range: ClosedRange<Double>
    private let step: Double
    private let showsValue: Bool

    public init(
        _ label: String? = nil,
        value: Binding<Double>,
        in range: ClosedRange<Double> = 0...100,
        step: Double = 1,
        showsValue: Bool = true
    ) {
        self.label = label
        self._value = value
        self.range = range
        self.step = step
        self.showsValue = showsValue
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
            if label != nil || showsValue {
                HStack {
                    if let label {
                        AedstLabel(label)
                    }
                    Spacer()
                    if showsValue {
                        Text(value.formatted(.number.precision(.fractionLength(0))))
                            .font(AedstTypography.caption)
                            .foregroundStyle(AedstColor.mutedForeground)
                    }
                }
            }
            Slider(value: $value, in: range, step: step)
                .tint(AedstColor.primary)
        }
        .accessibilityLabel(Text(label ?? "Slider"))
        .accessibilityValue(Text(value.formatted()))
    }
}

#Preview("AedstSlider") {
    AedstSlider("Volume", value: .constant(64))
        .padding()
}
