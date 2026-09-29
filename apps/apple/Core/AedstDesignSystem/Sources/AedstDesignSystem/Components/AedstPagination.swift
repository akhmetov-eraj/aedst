import SwiftUI

/// Page-number indicator matching shadcn/ui `Pagination` — previous/next
/// buttons flanking numbered pages, tokenized and disabled at the bounds.
public struct AedstPagination: View {
    @Binding private var page: Int
    private let pageCount: Int

    public init(page: Binding<Int>, pageCount: Int) {
        self._page = page
        self.pageCount = max(pageCount, 1)
    }

    public var body: some View {
        HStack(spacing: AedstSpacing.xxs) {
            AedstIconButton(systemImage: "chevron.left", accessibilityLabel: "Previous page", isDisabled: page <= 1) {
                page -= 1
            }

            ForEach(1...pageCount, id: \.self) { number in
                Button {
                    page = number
                } label: {
                    Text("\(number)")
                        .font(AedstTypography.caption.weight(.medium))
                        .frame(width: 32, height: 32)
                        .background(number == page ? AedstColor.primary : .clear)
                        .foregroundStyle(number == page ? AedstColor.primaryForeground : AedstColor.foreground)
                        .clipShape(RoundedRectangle(cornerRadius: AedstRadius.sm, style: .continuous))
                }
                #if os(macOS)
                .buttonStyle(.plain)
                #endif
                .accessibilityAddTraits(number == page ? [.isButton, .isSelected] : .isButton)
            }

            AedstIconButton(systemImage: "chevron.right", accessibilityLabel: "Next page", isDisabled: page >= pageCount) {
                page += 1
            }
        }
    }
}

#Preview("AedstPagination") {
    AedstPagination(page: .constant(2), pageCount: 5)
        .padding()
}
