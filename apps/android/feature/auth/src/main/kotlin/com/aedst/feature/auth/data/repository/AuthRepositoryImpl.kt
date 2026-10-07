package com.aedst.feature.auth.data.repository

import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.auth.v1.loginRequest
import com.aedst.auth.v1.logoutRequest
import com.aedst.core.common.SignOutHook
import com.aedst.core.network.toAedstError
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.data.local.DeviceIdStore
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.connectrpc.getOrThrow
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.withContext
import java.time.Clock
import javax.inject.Inject

class AuthRepositoryImpl @Inject constructor(
    private val authServiceClient: AuthServiceClientInterface,
    private val tokenStore: AuthTokenStore,
    private val deviceIdStore: DeviceIdStore,
    private val clock: Clock,
    private val signOutHooks: Set<@JvmSuppressWildcards SignOutHook> = emptySet(),
) : AuthRepository {

    override val session: Flow<AuthSession?> = tokenStore.session

    override suspend fun login(credentials: Credentials): AuthSession {
        val deviceId = deviceIdStore.get()
        val response = try {
            authServiceClient.login(
                loginRequest {
                    email = credentials.email.trim()
                    password = credentials.password
                    this.deviceId = deviceId
                },
            ).getOrThrow()
        } catch (cancellation: CancellationException) {
            throw cancellation
        } catch (@Suppress("TooGenericExceptionCaught") throwable: Throwable) {
            throw throwable.toAedstError()
        }
        val session = AuthSession(
            userId = response.userId,
            accessToken = response.tokens.accessToken,
            refreshToken = response.tokens.refreshToken,
            accessExpiresAtEpochSeconds = clock.instant().epochSecond + response.tokens.accessTokenExpiresInSeconds,
        )
        tokenStore.save(session)
        return session
    }

    override suspend fun logout() {
        // While the session is still valid: e.g. UnregisterPushDevice needs the bearer token. Best effort each.
        signOutHooks.forEach { hook ->
            try {
                hook.beforeSignOut()
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (@Suppress("TooGenericExceptionCaught") ignored: Throwable) {
                // A failing hook must never block sign-out.
            }
        }
        val refreshToken = tokenStore.awaitSession()?.refreshToken
        try {
            if (!refreshToken.isNullOrBlank()) {
                authServiceClient.logout(logoutRequest { this.refreshToken = refreshToken })
            }
        } catch (cancellation: CancellationException) {
            throw cancellation
        } catch (@Suppress("TooGenericExceptionCaught") ignored: Throwable) {
            // Best effort: an unreachable server must not trap the user in a signed-in state. The refresh token
            // simply expires server-side; the local session is cleared below regardless.
        } finally {
            // NonCancellable: sign-out must complete locally even if the caller is cancelled mid-request.
            withContext(NonCancellable) { tokenStore.clear() }
        }
    }
}
