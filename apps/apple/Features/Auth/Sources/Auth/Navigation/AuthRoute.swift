import AedstNavigation

/// The well-known route values other features/`App/Shared` may push onto
/// `Router` without importing anything else from this feature — the Swift
/// analogue of `AedstRoutes`' string constants in
/// `apps/android/core/navigation`.
public enum AuthRoute: AedstRoute {
    case login
}
