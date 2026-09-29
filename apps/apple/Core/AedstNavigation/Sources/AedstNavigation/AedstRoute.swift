/// Marker every feature's route value conforms to. A feature module (e.g.
/// `Auth`) defines its own `enum AuthRoute: AedstRoute { case login, signUp }`
/// and never imports another feature to navigate to it — it pushes a route
/// value onto the shared `Router`, and `App/Shared` (which links every
/// feature) is the only place that registers a view builder per route type.
///
/// This is the Swift analogue of `AedstNavGraph`/`AedstRoutes` in
/// `apps/android/core/navigation`: the seam that keeps `feature/*` modules
/// from depending on each other (see `docs/architecture-android.md`'s
/// "boundary rule").
public protocol AedstRoute: Hashable, Sendable {}
