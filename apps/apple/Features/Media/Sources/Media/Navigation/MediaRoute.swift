import AedstNavigation
import SwiftUI

/// Routes to the media library and one item.
public enum MediaRoute: AedstRoute {
    case library
    case detail(id: String)
}

extension View {
    /// Registers the media destinations on the root stack.
    public func mediaDestinations(
        @ViewBuilder makeLibrary: @escaping () -> some View,
        @ViewBuilder makeDetail: @escaping (String) -> some View
    ) -> some View {
        aedstDestination(for: MediaRoute.self) { route in
            switch route {
            case .library:
                makeLibrary()
            case .detail(let id):
                makeDetail(id)
            }
        }
    }
}
