import SwiftUI
import Charts

/// One data point for `AedstChart`.
public struct AedstChartPoint: Identifiable {
    public let id = UUID()
    public let label: String
    public let value: Double

    public init(label: String, value: Double) {
        self.label = label
        self.value = value
    }
}

/// Swift Charts wrapper matching shadcn/ui `Chart` — first-party and
/// lightweight, so this stays a thin tokenizing layer (series colors from
/// `AedstColor.chartSeries`) rather than a bespoke drawing engine.
///
/// Swift Charts is iOS 16 / macOS 13+, above this package's iOS 15 / macOS 12 floor, so the
/// chart is `@available`-gated; call sites on older OS versions must provide a fallback.
@available(iOS 16, macOS 13, tvOS 16, watchOS 9, *)
public struct AedstChart: View {
    public enum Style {
        case bar
        case line
        case area
    }

    private let points: [AedstChartPoint]
    private let style: Style

    public init(points: [AedstChartPoint], style: Style = .bar) {
        self.points = points
        self.style = style
    }

    public var body: some View {
        Chart(Array(points.enumerated()), id: \.element.id) { item in
            let index = item.offset
            let point = item.element
            switch style {
            case .bar:
                BarMark(x: .value("Label", point.label), y: .value("Value", point.value))
                    .foregroundStyle(seriesColor(index))
            case .line:
                LineMark(x: .value("Label", point.label), y: .value("Value", point.value))
                    .foregroundStyle(AedstColor.primary)
                    .interpolationMethod(.catmullRom)
            case .area:
                AreaMark(x: .value("Label", point.label), y: .value("Value", point.value))
                    .foregroundStyle(AedstColor.primary.opacity(0.3))
                LineMark(x: .value("Label", point.label), y: .value("Value", point.value))
                    .foregroundStyle(AedstColor.primary)
            }
        }
        .chartXAxis {
            AxisMarks { _ in
                AxisValueLabel().foregroundStyle(AedstColor.mutedForeground)
            }
        }
        .chartYAxis {
            AxisMarks { _ in
                AxisGridLine().foregroundStyle(AedstColor.border)
                AxisValueLabel().foregroundStyle(AedstColor.mutedForeground)
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel(Text(AedstStrings.chartSummary(points.count)))
    }

    private func seriesColor(_ index: Int) -> Color {
        AedstColor.chartSeries[index % AedstColor.chartSeries.count]
    }
}

#Preview("AedstChart") {
    if #available(iOS 16, macOS 13, *) {
        AedstChart(
            points: [
                AedstChartPoint(label: "Mon", value: 4),
                AedstChartPoint(label: "Tue", value: 7),
                AedstChartPoint(label: "Wed", value: 3),
                AedstChartPoint(label: "Thu", value: 9),
            ]
        )
        .frame(height: 200)
        .padding()
    }
}
