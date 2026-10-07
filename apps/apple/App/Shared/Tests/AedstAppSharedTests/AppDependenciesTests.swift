import Testing
@testable import AedstAppShared

@Suite("AppDependencies")
@MainActor
struct AppDependenciesTests {
    @Test("builds the whole graph from injected doubles")
    func initializes() {
        let dependencies = makeDependencies()
        _ = dependencies.authRepository
        _ = dependencies.authenticatedClient
        _ = dependencies.authorizedRequests
        _ = dependencies.makeLoginViewModel()
    }

    @Test("every call to makeLoginViewModel returns a fresh, empty form")
    func freshViewModels() {
        let dependencies = makeDependencies()
        let first = dependencies.makeLoginViewModel()
        first.email = "a@b.com"

        let second = dependencies.makeLoginViewModel()

        #expect(first !== second)
        #expect(second.email.isEmpty)
    }
}
