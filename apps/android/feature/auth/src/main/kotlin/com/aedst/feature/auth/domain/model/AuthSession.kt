package com.aedst.feature.auth.domain.model

/** Domain entity — no ConnectRPC/Room/DataStore types leak in here, mirrors `modules/auth/domain`. */
data class AuthSession(
    val userId: String,
    val accessToken: String,
    val refreshToken: String,
    /** Absolute expiry of [accessToken] (Unix epoch seconds), i.e. issue time + the server's `expires_in`. */
    val accessExpiresAtEpochSeconds: Long,
)

data class Credentials(val email: String, val password: String)
