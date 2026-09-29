package com.aedst.feature.auth.data.repository

import com.aedst.core.network.toAedstError
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.auth.v1.logoutRequest
import com.aedst.auth.v1.loginRequest
import com.connectrpc.getOrThrow
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject

class AuthRepositoryImpl @Inject constructor(
    private val authServiceClient: AuthServiceClientInterface,
    private val tokenStore: AuthTokenStore,
) : AuthRepository {

    override val session: Flow<AuthSession?> = tokenStore.session

    override suspend fun login(credentials: Credentials, deviceId: String): AuthSession {
        val response = try {
            authServiceClient.login(
                loginRequest {
                    email = credentials.email
                    password = credentials.password
                    this.deviceId = deviceId
                },
            ).getOrThrow()
        } catch (throwable: Throwable) {
            throw throwable.toAedstError()
        }
        val session = AuthSession(
            userId = response.userId,
            accessToken = response.tokens.accessToken,
            refreshToken = response.tokens.refreshToken,
            accessExpiresAtEpochSeconds = response.tokens.accessTokenExpiresInSeconds,
        )
        tokenStore.save(session)
        return session
    }

    override suspend fun logout() {
        val refreshToken = tokenStore.session.value?.refreshToken
        if (!refreshToken.isNullOrBlank()) {
            runCatching { authServiceClient.logout(logoutRequest { this.refreshToken = refreshToken }) }
        }
        tokenStore.clear()
    }
}
