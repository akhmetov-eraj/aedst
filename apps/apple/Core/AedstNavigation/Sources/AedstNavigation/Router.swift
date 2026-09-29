import Observation
import SwiftUI

/// One `NavigationPath`-backed router, injected into the environment at the
/// app root and pushed to by every feature's presentation layer. Features
/// depend on `AedstNavigation` (this package) only — never on each other —
/// exactly like Android features only depend on `core/navigation`'s
/// `AedstNavGraph` contract.
@MainActor
@Observable
public final class Router {
    public private(set) var path = NavigationPath()

    public init() {}

    public func push(_ route: some AedstRoute) {
        path.append(route)
    }

    public func pop() {
        guard !path.isEmpty else { return }
        path.removeLast()
    }

    public func popToRoot() {
        path.removeLast(path.count)
    }

    public func replace(with route: some AedstRoute) {
        popToRoot()
        push(route)
    }
}

// Injected at the app root with `.environment(router)`; features read it
// back with `@Environment(Router.self) private var router`. No custom
// `EnvironmentKey` needed — `@Observable` types support this directly.
