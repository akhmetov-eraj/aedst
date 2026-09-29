import AedstCore
import Connect
import Foundation

/// Attaches the bearer token to every outgoing unary call and clears it on
/// a 401 so the next request re-triggers `TokenProvider.refreshAccessToken()`.
/// Registered in `ConnectClientFactory` — the Swift analogue of the OkHttp
/// `AuthInterceptor` in `apps/android/core/network`.
///
/// NOTE: written against connect-swift's documented `Interceptor` shape but
/// not compiled in this environment (no Swift toolchain available here) —
/// verify against the resolved package version on first Xcode build.
public final class AuthInterceptor: Interceptor, Sendable {
    private let tokenProvider: TokenProvider

    public init(config: ProtocolClientConfig, tokenProvider: TokenProvider) {
        self.tokenProvider = tokenProvider
    }

    public convenience init(config: ProtocolClientConfig) {
        self.init(config: config, tokenProvider: NoopTokenProvider())
    }

    public func unaryFunction() -> UnaryFunction {
        UnaryFunction(
            requestFunction: { [tokenProvider] request in
                var headers = request.headers
                if let token = await tokenProvider.currentAccessToken() {
                    headers["Authorization"] = ["Bearer \(token)"]
                }
                return HTTPRequest(
                    url: request.url,
                    contentType: request.contentType,
                    headers: headers,
                    message: request.message,
                    trailers: request.trailers,
                    method: request.method,
                    idempotencyLevel: request.idempotencyLevel,
                    writeTimeoutMs: request.writeTimeoutMs
                )
            },
            responseFunction: { [tokenProvider] response in
                if response.code == .unauthenticated {
                    // Try to refresh before giving up on the session: a single
                    // spurious 401 shouldn't log the user out if the refresh
                    // token is still valid. `refreshAccessToken()` dedupes
                    // concurrent callers (see `KeychainTokenStore`), so this
                    // is safe to call from every 401 response, not just the
                    // first. The current response still surfaces as a
                    // failure to its caller; the refreshed token only takes
                    // effect on the *next* request — retrying this one in
                    // place would need access to the interceptor chain,
                    // which `responseFunction` does not expose.
                    if await tokenProvider.refreshAccessToken() == nil {
                        await tokenProvider.clearTokens()
                    }
                }
                return response
            }
        )
    }

    public func streamFunction() -> StreamFunction {
        StreamFunction(
            requestFunction: { $0 },
            requestDataFunction: { $0 },
            streamResultFunc: { $0 }
        )
    }
}

private struct NoopTokenProvider: TokenProvider {
    func currentAccessToken() async -> String? { nil }
    func refreshAccessToken() async -> String? { nil }
    func clearTokens() async {}
}
