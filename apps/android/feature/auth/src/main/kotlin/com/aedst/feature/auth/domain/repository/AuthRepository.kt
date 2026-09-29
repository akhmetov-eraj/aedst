package com.aedst.feature.auth.domain.repository

import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
import kotlinx.coroutines.flow.Flow

/**
 * Declared here (domain), implemented in `data/repository` — same rule as
 * the server's `application` package owning its repository interfaces
 * while `infrastructure/postgres` implements them.
 */
interface AuthRepository {
    val session: Flow<AuthSession?>

    suspend fun login(credentials: Credentials, deviceId: String): AuthSession

    suspend fun logout()
}
