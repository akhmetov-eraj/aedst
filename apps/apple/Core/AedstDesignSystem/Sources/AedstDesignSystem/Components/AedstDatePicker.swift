import SwiftUI

/// `DatePicker` wrapper matching shadcn/ui `Calendar`/`DatePicker` — a
/// bordered trigger row that expands the native graphical calendar,
/// tokenized to match the rest of the form controls.
public struct AedstDatePicker: View {
    private let label: String
    @Binding private var date: Date
    private let displayedComponents: DatePickerComponents

    public init(_ label: String, date: Binding<Date>, displayedComponents: DatePickerComponents = [.date]) {
        self.label = label
        self._date = date
        self.displayedComponents = displayedComponents
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
            AedstLabel(label)
            DatePicker(label, selection: $date, displayedComponents: displayedComponents)
                .labelsHidden()
                .tint(AedstColor.primary)
                .padding(.horizontal, AedstSpacing.sm)
                .frame(minHeight: 44)
                .background(AedstColor.background)
                .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
                .overlay {
                    RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous)
                        .strokeBorder(AedstColor.border, lineWidth: 1)
                }
        }
    }
}

#Preview("AedstDatePicker") {
    AedstDatePicker("Due date", date: .constant(.now))
        .padding()
}
