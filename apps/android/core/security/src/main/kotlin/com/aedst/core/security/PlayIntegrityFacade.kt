package com.aedst.core.security

import kotlinx.coroutines.tasks.await

/**
 * Thin wrapper over the Play Integrity API. Compiles with no real Google
 * Cloud project credentials — a request token/cloud project number is only
 * needed at call time in production, not at build time.
 */
interface IntegrityFacade {
    suspend fun requestIntegrityToken(requestHash: String): Result<String>
}

class PlayIntegrityFacade @javax.inject.Inject constructor(
    private val integrityManager: com.google.android.play.core.integrity.IntegrityManager,
) : IntegrityFacade {
    override suspend fun requestIntegrityToken(requestHash: String): Result<String> = runCatching {
        val request = com.google.android.play.core.integrity.IntegrityTokenRequest.builder()
            .setRequestHash(requestHash)
            .build()
        integrityManager.requestIntegrityToken(request).await().token()
    }
}
