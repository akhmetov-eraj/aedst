import AedstAnalytics
import AedstCore
import AedstNetwork
import Auth
import Foundation

/// The app's single composition root — every concrete implementation is
/// constructed exactly once, here, and only protocol types cross into
/// `Presentation`. `App/iOS`'s and `App/macOS`'s `@main` entry points both
/// build one `AppDependencies` and hand it to the same `RootView`; nothing
/// platform-specific belongs in this type (see `docs/architecture-apple.md`).
@MainActor
public final class AppDependencies {
    public let analytics: AnalyticsClient
    public let authRepository: AuthRepository

    public init(environment: APIEnvironment = .fromInfoPlist()) {
        let analytics = NoopAnalyticsClient()
        self.analytics = analytics

        let plainClient = ConnectClientFactory.makePlainClient(environment: environment)

        let tokenStore = KeychainTokenStore { refreshToken in
            // TODO(codegen): call Aedst_Auth_V1_AuthServiceClient(client:
            // plainClient).refreshToken(...) once `make generate` has run
            // (see Features/Auth/Sources/Auth/Data/ConnectAuthRemoteDataSource.swift).
            AedstLogger(category: "auth").error("Token refresh requested before codegen has run — treating as failed refresh.")
            return nil
        }

        let authenticatedClient = ConnectClientFactory.makeClient(environment: environment, tokenProvider: tokenStore)

        self.authRepository = ConnectAuthRepository(
            remote: ConnectAuthRemoteDataSource(client: plainClient),
            sessionStore: tokenStore
        )

        _ = authenticatedClient // wired for the next feature's remote data source to consume
    }
}
