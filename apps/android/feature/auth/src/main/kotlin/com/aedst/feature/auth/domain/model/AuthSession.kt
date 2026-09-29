package com.aedst.feature.auth.domain.model

/** Domain entity — no ConnectRPC/Room/DataStore types leak in here, mirrors `modules/auth/domain`. */
data class AuthSession(
    val userId: String,
    val accessToken: String,
    val refreshToken: String,
    val accessExpiresAtEpochSeconds: Long,
)

data class Credentials(
    val email: String,
    val password: String,
)
