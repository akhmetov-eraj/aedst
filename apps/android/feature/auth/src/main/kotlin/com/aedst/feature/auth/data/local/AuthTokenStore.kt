package com.aedst.feature.auth.data.local

import com.aedst.core.security.EncryptedPreferences
import com.aedst.feature.auth.domain.model.AuthSession
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * The only place in the app that reads/writes auth tokens — everything
 * else (including `core/network`'s [com.aedst.core.network.AuthTokenProvider]
 * binding below) goes through this. Backed by [EncryptedPreferences], never
 * plain SharedPreferences.
 */
@Singleton
class AuthTokenStore @Inject constructor(
    private val encryptedPreferences: EncryptedPreferences,
) {
    private val sessionState = MutableStateFlow(restore())
    val session: StateFlow<AuthSession?> = sessionState.asStateFlow()

    fun save(session: AuthSession) {
        encryptedPreferences.putString(KEY_USER_ID, session.userId)
        encryptedPreferences.putString(KEY_ACCESS_TOKEN, session.accessToken)
        encryptedPreferences.putString(KEY_REFRESH_TOKEN, session.refreshToken)
        encryptedPreferences.putString(KEY_EXPIRES_AT, session.accessExpiresAtEpochSeconds.toString())
        sessionState.value = session
    }

    fun clear() {
        listOf(KEY_USER_ID, KEY_ACCESS_TOKEN, KEY_REFRESH_TOKEN, KEY_EXPIRES_AT).forEach(encryptedPreferences::remove)
        sessionState.value = null
    }

    fun currentAccessToken(): String? = sessionState.value?.accessToken

    private fun restore(): AuthSession? {
        val userId = encryptedPreferences.getString(KEY_USER_ID) ?: return null
        val accessToken = encryptedPreferences.getString(KEY_ACCESS_TOKEN) ?: return null
        val refreshToken = encryptedPreferences.getString(KEY_REFRESH_TOKEN) ?: return null
        val expiresAt = encryptedPreferences.getString(KEY_EXPIRES_AT)?.toLongOrNull() ?: return null
        return AuthSession(userId, accessToken, refreshToken, expiresAt)
    }

    private companion object {
        const val KEY_USER_ID = "auth.user_id"
        const val KEY_ACCESS_TOKEN = "auth.access_token"
        const val KEY_REFRESH_TOKEN = "auth.refresh_token"
        const val KEY_EXPIRES_AT = "auth.access_expires_at"
    }
}
