import AedstNavigation
import SwiftUI

/// Route to the search screen.
public enum SearchRoute: AedstRoute {
    case search
}

extension View {
    /// Registers the search destination on the root stack.
    public func searchDestinations(@ViewBuilder makeSearch: @escaping () -> some View) -> some View {
        aedstDestination(for: SearchRoute.self) { route in
            switch route {
            case .search:
                makeSearch()
            }
        }
    }
}
