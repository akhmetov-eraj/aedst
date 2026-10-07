package com.aedst.core.network

import okhttp3.Authenticator
import okhttp3.Request
import okhttp3.Response
import okhttp3.Route
import javax.inject.Inject

/**
 * Supplies a fresh access token after the server rejected the one we sent.
 * Implemented by `feature/auth` (it owns the refresh token and the
 * `RefreshToken` RPC); declared here so `core/network` never depends on it.
 *
 * Called on an OkHttp worker thread, so it may block. Implementations must:
 *  - serialize concurrent callers (one refresh, everyone reuses the result);
 *  - return the already-refreshed token if [staleAccessToken] is no longer current;
 *  - return `null` when no valid session remains (after clearing it) or the refresh failed transiently.
 */
fun interface TokenRefresher {
    fun refresh(staleAccessToken: String?): String?
}

/**
 * Reactive half of token handling: when a request comes back 401, refresh once and replay it with the new token.
 * (The proactive half — attaching the token — is [AuthInterceptor].)
 *
 * Gives up (returns `null`, surfacing the 401 to the caller) when:
 *  - the failed request was itself an auth call — avoids refresh loops;
 *  - we already retried this request once;
 *  - no fresh token could be obtained.
 *
 * [TokenRefresher] is injected lazily: the refresher's implementation needs the RPC client, which needs the
 * `OkHttpClient`, which needs this authenticator — `dagger.Lazy` breaks that cycle.
 */
class TokenRefreshAuthenticator @Inject constructor(private val tokenRefresher: dagger.Lazy<TokenRefresher>) :
    Authenticator {

    override fun authenticate(route: Route?, response: Response): Request? {
        val request = response.request
        if (isAuthCall(request) || responseCount(response) > 1) return null

        val staleToken = request.header(AUTHORIZATION)?.removePrefix(BEARER_PREFIX)
        val freshToken = tokenRefresher.get().refresh(staleToken)?.takeIf { it.isNotBlank() } ?: return null

        return request.newBuilder().header(AUTHORIZATION, BEARER_PREFIX + freshToken).build()
    }

    private fun responseCount(response: Response): Int {
        var count = 1
        var prior = response.priorResponse
        while (prior != null) {
            count++
            prior = prior.priorResponse
        }
        return count
    }

    private companion object {
        const val AUTHORIZATION = "Authorization"
        const val BEARER_PREFIX = "Bearer "

        /** Connect unary calls are `POST /<package>.<Service>/<Method>`; these establish/refresh/end a session. */
        val AUTH_METHODS = setOf("Login", "Register", "RefreshToken", "Logout")

        fun isAuthCall(request: Request): Boolean {
            val path = request.url.encodedPath
            return path.contains(".AuthService/") && path.substringAfterLast('/') in AUTH_METHODS
        }
    }
}
