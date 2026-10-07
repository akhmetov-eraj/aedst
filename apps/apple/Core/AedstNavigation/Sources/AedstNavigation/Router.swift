import FlowStacks
import Perception
import SwiftUI

/// One `FlowPath`-backed router, owned at the app root and pushed to by every
/// feature's presentation layer. Features depend on `AedstNavigation` (this
/// package) only — never on each other — exactly like Android features only
/// depend on `core/navigation`'s `AedstNavGraph` contract.
///
/// `@Perceptible` is Point-Free's Observation backport: on iOS 17 / macOS 14+
/// it is backed by the system `Observation` framework, below that by
/// `PerceptionRegistrar` (views must read it inside `WithPerceptionTracking`).
/// `FlowPath` (FlowStacks) stands in for `NavigationPath`, which is iOS 16 /
/// macOS 13; on those OS versions `FlowStack` renders a real `NavigationStack`.
@MainActor
@Perceptible
public final class Router {
    /// Bound by `AedstNavigationStack`; mutate through `push`/`pop`/`popToRoot`/`replace`
    /// rather than directly (it is `public` only so SwiftUI can bind to it).
    public var path = FlowPath()

    /// Creates an empty router.
    public init() {}

    /// Pushes `route` onto the stack.
    public func push(_ route: some AedstRoute) {
        path.push(route)
    }

    /// Pops the top route; no-op on an empty stack.
    public func pop() {
        guard !path.isEmpty else { return }
        path.pop()
    }

    /// Removes every pushed route.
    public func popToRoot() {
        path.popToRoot()
    }

    /// Clears the stack, then pushes `route`.
    public func replace(with route: some AedstRoute) {
        popToRoot()
        push(route)
    }
}

// Pass the router explicitly (`StartView(router: router)`) rather than through
// `@Environment(Router.self)`: SwiftUI's `Environment(_: Observable)` is iOS 17+, and
// Perception's back-port overload is `@_disfavoredOverload`, which the compiler does
// not reliably pick below iOS 17.
