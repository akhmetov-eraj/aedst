import SwiftUI

/// `NavigationStack` on iOS 16 / macOS 13+, `NavigationView` below — for the
/// design system's own self-contained sheets (`AedstCombobox`,
/// `AedstCommandPalette`) that need a navigation bar/search field but not a
/// programmatic path. App-level navigation goes through `AedstNavigation`.
struct AedstNavigationContainer<Content: View>: View {
    private let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        if #available(iOS 16, macOS 13, tvOS 16, watchOS 9, *) {
            NavigationStack { content }
        } else {
            NavigationView { content }
                #if os(iOS)
                .navigationViewStyle(.stack)
                #endif
        }
    }
}
