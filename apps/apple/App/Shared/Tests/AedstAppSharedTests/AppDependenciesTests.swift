import Testing
@testable import AedstAppShared

@Suite("AppDependencies")
@MainActor
struct AppDependenciesTests {
    @Test("initializes without crashing and exposes its repositories")
    func initializes() {
        let dependencies = AppDependencies(environment: .local)
        _ = dependencies.authRepository
    }
}
