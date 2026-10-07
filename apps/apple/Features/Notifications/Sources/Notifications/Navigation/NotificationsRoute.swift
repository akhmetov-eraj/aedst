import AedstNavigation
import SwiftUI

/// Routes to the inbox and the preferences screen.
public enum NotificationsRoute: AedstRoute {
    case inbox
    case preferences
}

extension View {
    /// Registers the notifications destinations on the root stack.
    public func notificationsDestinations(
        @ViewBuilder makeInbox: @escaping () -> some View,
        @ViewBuilder makePreferences: @escaping () -> some View
    ) -> some View {
        aedstDestination(for: NotificationsRoute.self) { route in
            switch route {
            case .inbox:
                makeInbox()
            case .preferences:
                makePreferences()
            }
        }
    }
}
