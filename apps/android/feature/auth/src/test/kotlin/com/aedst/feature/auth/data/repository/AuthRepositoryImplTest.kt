package com.aedst.feature.auth.data.repository

import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.auth.v1.LoginResponse
import com.aedst.auth.v1.LogoutResponse
import com.aedst.core.common.AedstError
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.data.local.DeviceIdStore
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
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
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import java.time.Clock
import java.time.Instant
import java.time.ZoneOffset

class AuthRepositoryImplTest {

    private val authServiceClient: AuthServiceClientInterface = mockk()
    private val preferences = FakeEncryptedPreferences()
    private val tokenStore = AuthTokenStore(preferences, CoroutineScope(Dispatchers.Unconfined))
    private val deviceIdStore = DeviceIdStore(preferences)
    private val clock = Clock.fixed(Instant.ofEpochSecond(1_000), ZoneOffset.UTC)
    private val repository = AuthRepositoryImpl(authServiceClient, tokenStore, deviceIdStore, clock)

    private val credentials = Credentials("a@b.com", "password123")
    private val loginResponse = LoginResponse.newBuilder()
        .setUserId("u1")
        .setTokens(tokenPair(access = "access", refresh = "refresh", expiresInSeconds = 900L))
        .build()

    @Before
    fun setUp() {
        coEvery { authServiceClient.login(any(), any()) } returns success(loginResponse)
    }

    @Test
    fun `login sends credentials and the persisted device id to the API`() = runTest {
        repository.login(credentials)

        val expectedDeviceId = deviceIdStore.get()
        coVerify {
            authServiceClient.login(
                match { it.email == "a@b.com" && it.password == "password123" && it.deviceId == expectedDeviceId },
                any(),
            )
        }
    }

    @Test
    fun `login reuses the same device id across calls`() = runTest {
        repository.login(credentials)
        repository.login(credentials)

        coVerify(exactly = 2) { authServiceClient.login(match { it.deviceId == deviceIdStore.get() }, any()) }
    }

    @Test
    fun `login trims the e-mail but never the password`() = runTest {
        repository.login(Credentials("  a@b.com  ", " secret "))

        coVerify { authServiceClient.login(match { it.email == "a@b.com" && it.password == " secret " }, any()) }
    }

    @Test
    fun `login stores an absolute expiry computed from the server's relative expires_in`() = runTest {
        val session = repository.login(credentials)

        val expected = AuthSession("u1", "access", "refresh", accessExpiresAtEpochSeconds = 1_000L + 900L)
        assertEquals(expected, session)
        assertEquals(expected, tokenStore.session.value)
        assertEquals("access", tokenStore.currentAccessToken())
    }

    @Test
    fun `login maps a failed API call to an AedstError instead of leaking the raw throwable`() = runTest {
        coEvery { authServiceClient.login(any(), any()) } returns
            failure(ConnectException(Code.UNAVAILABLE, message = "connection reset"))

        val thrown = runCatching { repository.login(credentials) }.exceptionOrNull()

        assertEquals(AedstError.Network::class.java, thrown?.javaClass)
    }

    @Test
    fun `login maps wrong credentials to Unauthorized and stores nothing`() = runTest {
        coEvery { authServiceClient.login(any(), any()) } returns
            failure(ConnectException(Code.UNAUTHENTICATED, message = "invalid credentials"))

        val thrown = runCatching { repository.login(credentials) }.exceptionOrNull()

        assertEquals(AedstError.Unauthorized::class.java, thrown?.javaClass)
        assertNull(tokenStore.session.value)
    }

    @Test
    fun `logout revokes the refresh token server-side and clears the local session`() = runTest {
        tokenStore.save(AuthSession("u1", "access", "refresh", 123L))
        coEvery { authServiceClient.logout(any(), any()) } returns success(LogoutResponse.getDefaultInstance())

        repository.logout()

        coVerify { authServiceClient.logout(match { it.refreshToken == "refresh" }, any()) }
        assertNull(tokenStore.session.value)
    }

    @Test
    fun `logout clears the local session even if the API call fails`() = runTest {
        tokenStore.save(AuthSession("u1", "access", "refresh", 123L))
        coEvery { authServiceClient.logout(any(), any()) } returns
            failure(ConnectException(Code.UNAVAILABLE, message = "connection reset"))

        repository.logout()

        assertNull(tokenStore.session.value)
    }

    @Test
    fun `logout clears the local session even if the API call throws`() = runTest {
        tokenStore.save(AuthSession("u1", "access", "refresh", 123L))
        coEvery { authServiceClient.logout(any(), any()) } throws java.io.IOException("boom")

        repository.logout()

        assertNull(tokenStore.session.value)
    }

    @Test
    fun `logout without a session makes no API call`() = runTest {
        repository.logout()

        coVerify(exactly = 0) { authServiceClient.logout(any(), any()) }
    }
}
