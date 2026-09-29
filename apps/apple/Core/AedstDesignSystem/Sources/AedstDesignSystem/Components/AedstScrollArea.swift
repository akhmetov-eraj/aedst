import SwiftUI

/// `ScrollView` wrapper matching shadcn/ui `ScrollArea` — consistent
/// indicator visibility and content padding so features stop deciding this
/// per-screen.
public struct AedstScrollArea<Content: View>: View {
    private let axes: Axis.Set
    private let showsIndicators: Bool
    private let content: Content

    public init(
        _ axes: Axis.Set = .vertical,
        showsIndicators: Bool = false,
        @ViewBuilder content: () -> Content
    ) {
        self.axes = axes
        self.showsIndicators = showsIndicators
        self.content = content()
    }

    public var body: some View {
        ScrollView(axes, showsIndicators: showsIndicators) {
            content
        }
    }
}

#Preview("AedstScrollArea") {
    AedstScrollArea {
        VStack(spacing: AedstSpacing.sm) {
            ForEach(0..<10) { index in
                Text("Row \(index)")
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .padding()
    }
}
