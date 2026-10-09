import AedstCore
import AedstTesting
import Auth
import Foundation
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

    @Test("without an OIDC issuer the first-party password path stays")
    func passwordModeByDefault() {
        let dependencies = makeDependencies()

        #expect(dependencies.authRepository.signInMode == .password)
        #expect(dependencies.makeLoginViewModel().signInMode == .password)
    }

    @Test("an injected OIDC configuration switches the login screen to browser sign-in")
    func browserModeWithOIDC() throws {
        let configuration = try #require(OIDCConfiguration(issuer: "https://idp.example.com", clientID: "client-1"))
        let dependencies = AppDependencies(
            environment: .local,
            secureStore: InMemorySecureStore(),
            clock: ManualClock(now: Date(timeIntervalSince1970: 1_000_000)),
            connectivity: StubConnectivity([true]),
            biometrics: StubBiometrics(),
            oidc: configuration,
            webAuthenticator: UnusedWebAuthenticator(),
            outboxStorage: .memory
        )

        #expect(dependencies.authRepository.signInMode == .browser)
        #expect(dependencies.makeLoginViewModel().signInMode == .browser)
    }
}

private struct UnusedWebAuthenticator: WebAuthenticator {
    @MainActor
    func authenticate(url: URL, callbackScheme: String) async throws -> URL {
        throw WebAuthenticationError.failed(reason: "not used")
    }
}
