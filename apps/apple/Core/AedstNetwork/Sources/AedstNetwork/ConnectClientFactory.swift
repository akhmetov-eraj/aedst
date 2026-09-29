import AedstCore
import Connect
import Foundation

/// One `ProtocolClient` per app process, shared by every feature module's
/// generated Connect service client (see `Generated/README.md`). Analogous
/// to the single Retrofit instance built in `apps/android/core/network`.
public enum ConnectClientFactory {
    /// Every call except `AuthService.Login`/`RefreshToken` goes through
    /// this client: it attaches the bearer token and clears it on a 401.
    public static func makeClient(
        environment: APIEnvironment,
        tokenProvider: TokenProvider
    ) -> ProtocolClient {
        ProtocolClient(
            httpClient: URLSessionHTTPClient(),
            config: ProtocolClientConfig(
                host: environment.baseURL.absoluteString,
                networkProtocol: .connect,
                codec: .proto,
                interceptors: [
                    { config in AuthInterceptor(config: config, tokenProvider: tokenProvider) },
                ]
            )
        )
    }

    /// No `AuthInterceptor` — used only for the two calls that can't
    /// possibly have a bearer token yet: `AuthService.Login` and
    /// `RefreshToken`. Breaks what would otherwise be a circular dependency
    /// (the authenticated client needs a `TokenProvider`, whose
    /// `refreshHandler` needs a client to call `RefreshToken` on).
    public static func makePlainClient(environment: APIEnvironment) -> ProtocolClient {
        ProtocolClient(
            httpClient: URLSessionHTTPClient(),
            config: ProtocolClientConfig(
                host: environment.baseURL.absoluteString,
                networkProtocol: .connect,
                codec: .proto
            )
        )
    }
}
