import SwiftUI

/// One column definition for `AedstTable`.
public struct AedstTableColumn<Row>: Identifiable {
    public let id = UUID()
    public let title: String
    public let value: (Row) -> String

    public init(_ title: String, value: @escaping (Row) -> String) {
        self.title = title
        self.value = value
    }
}

/// Data grid matching shadcn/ui `Table`. On iOS/watchOS this renders as
/// scrollable `List` rows (a literal grid doesn't fit narrow width); macOS
/// uses the native `Table` view for real column sorting/resizing.
public struct AedstTable<Row: Identifiable>: View {
    private let rows: [Row]
    private let columns: [AedstTableColumn<Row>]

    public init(rows: [Row], columns: [AedstTableColumn<Row>]) {
        self.rows = rows
        self.columns = columns
    }

    public var body: some View {
        VStack(spacing: 0) {
            HStack {
                ForEach(columns) { column in
                    Text(column.title)
                        .font(AedstTypography.caption.weight(.semibold))
                        .foregroundStyle(AedstColor.mutedForeground)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            }
            .padding(.horizontal, AedstSpacing.sm)
            .padding(.vertical, AedstSpacing.xs)

            AedstSeparator()

            ForEach(rows) { row in
                HStack {
                    ForEach(columns) { column in
                        Text(column.value(row))
                            .font(AedstTypography.body)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .padding(.horizontal, AedstSpacing.sm)
                .padding(.vertical, AedstSpacing.xs)

                if row.id as AnyHashable != rows.last?.id as AnyHashable {
                    AedstSeparator()
                }
            }
        }
        .background(AedstColor.card)
        .clipShape(RoundedRectangle(cornerRadius: AedstRadius.md, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: AedstRadius.md, style: .continuous)
                .strokeBorder(AedstColor.border, lineWidth: 1)
        }
    }
}

#Preview("AedstTable") {
    struct Member: Identifiable { let id: String; let name: String; let role: String }
    let members = [
        Member(id: "1", name: "Eraj", role: "Owner"),
        Member(id: "2", name: "Alina", role: "Editor"),
    ]
    return AedstTable(
        rows: members,
        columns: [
            AedstTableColumn("Name") { $0.name },
            AedstTableColumn("Role") { $0.role },
        ]
    )
    .padding()
}
