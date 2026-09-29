package com.aedst.core.network

import com.connectrpc.Interceptor
import com.connectrpc.StreamFunction
import com.connectrpc.UnaryFunction
import com.connectrpc.http.HTTPRequest
import com.connectrpc.http.clone
import javax.inject.Inject

/** Reads the current access token from [AuthTokenProvider] (implemented in feature/auth's data layer) and attaches it. */
fun interface AuthTokenProvider {
    fun currentAccessToken(): String?
}

/**
 * Attaches the bearer token to every outgoing unary call — the Kotlin
 * analogue of `ConnectClientFactory.AuthInterceptor` on Apple. Registered in
 * [NetworkModule]. Token refresh/clear-on-401 lives in `AuthRepositoryImpl`
 * (triggered by [com.aedst.core.common.AedstError.Unauthorized] surfacing
 * from [toAedstError]), not here — this interceptor only ever reads.
 *
 * NOTE: written against connect-kotlin 0.7.2's documented `Interceptor`/
 * `UnaryFunction`/`HTTPRequest.clone()` shape but not compiled in this
 * environment (no Android/Gradle toolchain available here) — verify against
 * the resolved library version on first Gradle sync, same caveat as Apple's
 * `AuthInterceptor.swift`.
 */
class AuthInterceptor @Inject constructor(
    private val tokenProvider: AuthTokenProvider,
) : Interceptor {
    override fun unaryFunction(): UnaryFunction =
        UnaryFunction(
            requestFunction = { request ->
                val token = tokenProvider.currentAccessToken()
                if (token.isNullOrBlank()) {
                    request
                } else {
                    request.clone(headers = request.headers + mapOf("Authorization" to listOf("Bearer $token")))
                }
            },
        )

    override fun streamFunction(): StreamFunction = StreamFunction()
}
