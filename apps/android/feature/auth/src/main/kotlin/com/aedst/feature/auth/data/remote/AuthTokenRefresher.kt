package com.aedst.feature.auth.data.remote

import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.auth.v1.refreshTokenRequest
import com.aedst.core.common.AedstError
import com.aedst.core.common.Logger
import com.aedst.core.network.TokenRefresher
import com.aedst.core.network.toAedstError
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.connectrpc.getOrThrow
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.time.Clock
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Implements `core/network`'s [TokenRefresher]: exchanges the stored refresh token for a new pair when a request
 * came back 401.
 *
 * - **Single-flight**: a mutex serializes callers; the second caller finds the token already replaced
 *   (`current.accessToken != stale`) and reuses it instead of burning the (rotating) refresh token twice.
 * - **Definitive rejection** (refresh token expired/revoked/malformed) clears the session, which flips
 *   `SessionState` and sends the user back to sign-in.
 * - **Transient failure** (offline, 5xx, timeout) keeps the session and returns `null`, so the original 401 surfaces
 *   once but the user stays signed in and the next request tries again.
 *
 * `AuthServiceClientInterface` is injected lazily: it depends on the `OkHttpClient` that depends on the
 * authenticator that depends on this class.
 */
@Singleton
class AuthTokenRefresher @Inject constructor(
    private val tokenStore: AuthTokenStore,
    private val authServiceClient: dagger.Lazy<AuthServiceClientInterface>,
    private val clock: Clock,
    private val logger: Logger,
) : TokenRefresher {
    private val mutex = Mutex()

    override fun refresh(staleAccessToken: String?): String? =
        runBlocking { mutex.withLock { refreshLocked(staleAccessToken) } }

    private suspend fun refreshLocked(staleAccessToken: String?): String? {
        val current = tokenStore.awaitSession() ?: return null
        if (staleAccessToken != null && current.accessToken != staleAccessToken) return current.accessToken

        val tokens = try {
            authServiceClient.get().refreshToken(refreshTokenRequest { refreshToken = current.refreshToken })
                .getOrThrow().tokens
        } catch (cancellation: CancellationException) {
            throw cancellation
        } catch (@Suppress("TooGenericExceptionCaught") throwable: Throwable) {
            val error = throwable.toAedstError()
            if (error is AedstError.Unauthorized || error is AedstError.Validation) {
                logger.warn(TAG, "Refresh token rejected; ending session", error)
                tokenStore.clear()
            } else {
                logger.warn(TAG, "Token refresh failed transiently", error)
            }
            return null
        }

        val refreshed = current.copy(
            accessToken = tokens.accessToken,
            refreshToken = tokens.refreshToken.ifBlank { current.refreshToken },
            accessExpiresAtEpochSeconds = clock.instant().epochSecond + tokens.accessTokenExpiresInSeconds,
        )
        tokenStore.save(refreshed)
        return refreshed.accessToken
    }

    private companion object {
        const val TAG = "AuthTokenRefresher"
    }
}
