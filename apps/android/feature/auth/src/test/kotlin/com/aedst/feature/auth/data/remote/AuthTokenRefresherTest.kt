package com.aedst.feature.auth.data.remote

import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.auth.v1.RefreshTokenResponse
import com.aedst.core.common.Logger
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.testing.FakeEncryptedPreferences
import com.aedst.feature.auth.testing.failure
import com.aedst.feature.auth.testing.success
import com.aedst.feature.auth.testing.tokenPair
import com.connectrpc.Code
import com.connectrpc.ConnectException
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.time.Clock
import java.time.Instant
import java.time.ZoneOffset
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class AuthTokenRefresherTest {

    private val client: AuthServiceClientInterface = mockk()
    private val logger: Logger = mockk(relaxed = true)
    private val store = AuthTokenStore(FakeEncryptedPreferences(), CoroutineScope(Dispatchers.Unconfined))
    private val clock = Clock.fixed(Instant.ofEpochSecond(5_000), ZoneOffset.UTC)
    private val refresher = AuthTokenRefresher(store, { client }, clock, logger)

    private val signedIn = AuthSession("u1", "old-access", "old-refresh", accessExpiresAtEpochSeconds = 10L)

    private fun refreshResponse(access: String = "new-access", refresh: String = "new-refresh") =
        success(RefreshTokenResponse.newBuilder().setTokens(tokenPair(access, refresh, 900L)).build())

    @Test
    fun `exchanges the refresh token, stores the rotated pair and returns the new access token`() = runTest {
        store.save(signedIn)
        coEvery { client.refreshToken(any(), any()) } returns refreshResponse()

        val token = refresher.refresh("old-access")

        assertEquals("new-access", token)
        assertEquals(
            AuthSession("u1", "new-access", "new-refresh", accessExpiresAtEpochSeconds = 5_000L + 900L),
            store.session.value,
        )
        coVerify { client.refreshToken(match { it.refreshToken == "old-refresh" }, any()) }
    }

    @Test
    fun `keeps the old refresh token when the server does not rotate it`() = runTest {
        store.save(signedIn)
        coEvery { client.refreshToken(any(), any()) } returns refreshResponse(refresh = "")

        refresher.refresh("old-access")

        assertEquals("old-refresh", store.session.value?.refreshToken)
    }

    @Test
    fun `reuses a token another caller already refreshed instead of refreshing again`() = runTest {
        store.save(signedIn.copy(accessToken = "already-new"))

        val token = refresher.refresh("old-access")

        assertEquals("already-new", token)
        coVerify(exactly = 0) { client.refreshToken(any(), any()) }
    }

    @Test
    fun `concurrent callers trigger a single refresh call`() {
        runTest { store.save(signedIn) }
        coEvery { client.refreshToken(any(), any()) } coAnswers {
            Thread.sleep(50) // widen the race window
            refreshResponse()
        }
        val pool = Executors.newFixedThreadPool(8)

        val results = (1..8).map { pool.submit<String?> { refresher.refresh("old-access") } }.map { it.get() }
        pool.shutdown()
        pool.awaitTermination(5, TimeUnit.SECONDS)

        assertEquals(List(8) { "new-access" }, results)
        coVerify(exactly = 1) { client.refreshToken(any(), any()) }
    }

    @Test
    fun `a rejected refresh token ends the session`() = runTest {
        store.save(signedIn)
        coEvery { client.refreshToken(any(), any()) } returns
            failure(ConnectException(Code.UNAUTHENTICATED, message = "refresh token revoked"))

        assertNull(refresher.refresh("old-access"))

        assertNull(store.session.value)
    }

    @Test
    fun `a transient failure keeps the session so the user stays signed in`() = runTest {
        store.save(signedIn)
        coEvery { client.refreshToken(any(), any()) } returns
            failure(ConnectException(Code.UNAVAILABLE, message = "connection reset"))

        assertNull(refresher.refresh("old-access"))

        assertNotNull(store.session.value)
        assertEquals("old-refresh", store.session.value?.refreshToken)
    }

    @Test
    fun `returns null without any network call when nobody is signed in`() = runTest {
        assertNull(refresher.refresh("whatever"))

        coVerify(exactly = 0) { client.refreshToken(any(), any()) }
    }
}
