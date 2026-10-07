import SwiftUI

/// Keeps a `.popover` a popover on compact-width iPhone
/// (`presentationCompactAdaptation`, iOS 16.4 / macOS 13.3+). Below that OS
/// version the system default applies (a sheet on iPhone) — an accepted
/// degradation rather than a reason to raise the deployment target.
struct AedstPopoverAdaptationModifier: ViewModifier {
    @ViewBuilder
    func body(content: Content) -> some View {
        if #available(iOS 16.4, macOS 13.3, tvOS 16.4, watchOS 9.4, *) {
            content.presentationCompactAdaptation(.popover)
        } else {
            content
        }
    }
}

extension View {
    func aedstPopoverAdaptation() -> some View {
        modifier(AedstPopoverAdaptationModifier())
    }
}
