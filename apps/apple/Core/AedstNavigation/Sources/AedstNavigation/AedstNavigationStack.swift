import FlowStacks
import Perception
import SwiftUI

/// The app's one navigation container: a `NavigationStack` on iOS 16 / macOS 13+
/// and a `NavigationView`-based stack below (via FlowStacks), driven by `Router`.
/// Attach each feature's destinations to `root` with `.aedstDestination(for:)`.
public struct AedstNavigationStack<Root: View>: View {
    @Perception.Bindable private var router: Router
    private let root: Root

    /// Creates the stack for `router`; `root` is the first screen.
    public init(router: Router, @ViewBuilder root: () -> Root) {
        self.router = router
        self.root = root()
    }

    public var body: some View {
        WithPerceptionTracking {
            FlowStack($router.path, withNavigation: true) {
                root
            }
        }
    }
}

extension View {
    /// Maps a feature's route type to its destination view — the back-ported
    /// counterpart of `navigationDestination(for:destination:)`. Features call
    /// this instead of importing FlowStacks.
    public func aedstDestination<Route: AedstRoute>(
        for routeType: Route.Type,
        @ViewBuilder destination: @escaping (Route) -> some View
    ) -> some View {
        flowDestination(for: routeType, destination: destination)
    }
}
