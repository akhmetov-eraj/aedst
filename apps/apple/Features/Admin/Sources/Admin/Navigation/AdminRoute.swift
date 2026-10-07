import AedstNavigation
import SwiftUI

/// Admin sections. `App/Shared` only registers these destinations' entry point
/// for tokens with the `admin` role.
public enum AdminRoute: AedstRoute {
    case dashboard
    case users
    case tasks
    case broadcast
    case auditLog
}

extension View {
    /// Registers the admin destinations; `makeView` builds each section.
    public func adminDestinations(@ViewBuilder makeView: @escaping (AdminRoute) -> some View) -> some View {
        aedstDestination(for: AdminRoute.self) { route in
            makeView(route)
        }
    }
}
