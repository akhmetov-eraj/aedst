package com.aedst.feature.auth.data.repository

import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.auth.v1.LoginResponse
import com.aedst.auth.v1.LogoutResponse
import com.aedst.auth.v1.TokenPair
import com.aedst.core.common.AedstError
import com.aedst.core.security.EncryptedPreferences
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
import com.connectrpc.Code
import com.connectrpc.ConnectException
import com.connectrpc.ResponseMessage
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

/** In-memory [EncryptedPreferences] fake — exercises the real [AuthTokenStore], not a mock of it. */
private class FakeEncryptedPreferences : EncryptedPreferences {
    private val values = mutableMapOf<String, String>()
    override fun putString(key: String, value: String) { values[key] = value }
    override fun getString(key: String): String? = values[key]
    override fun remove(key: String) { values.remove(key) }
    override fun clear() { values.clear() }
}

private fun <T> success(message: T): ResponseMessage<T> =
    ResponseMessage.Success(message, headers = emptyMap(), trailers = emptyMap())

private fun <T> failure(exception: ConnectException): ResponseMessage<T> =
    ResponseMessage.Failure(exception, headers = emptyMap(), trailers = emptyMap())

class AuthRepositoryImplTest {

    private val authServiceClient: AuthServiceClientInterface = mockk()
    private val tokenStore = AuthTokenStore(FakeEncryptedPreferences())
    private val repository = AuthRepositoryImpl(authServiceClient, tokenStore)

    private val credentials = Credentials("a@b.com", "password123")
    private val loginResponse = LoginResponse.newBuilder()
        .setUserId("u1")
        .setTokens(
            TokenPair.newBuilder()
                .setAccessToken("access")
                .setRefreshToken("refresh")
                .setAccessTokenExpiresInSeconds(123L)
                .build(),
        )
        .build()

    @Before
    fun setUp() {
        coEvery { authServiceClient.login(any(), any()) } returns success(loginResponse)
    }

    @Test
    fun `login sends credentials and device id to the API`() = runTest {
        repository.login(credentials, deviceId = "device-1")

        coVerify {
            authServiceClient.login(
                match { it.email == "a@b.com" && it.password == "password123" && it.deviceId == "device-1" },
                any(),
            )
        }
    }

    @Test
    fun `login persists the returned session in the token store and returns it`() = runTest {
        val session = repository.login(credentials, deviceId = "device-1")

        val expected = AuthSession("u1", "access", "refresh", 123L)
        assertEquals(expected, session)
        assertEquals(expected, tokenStore.session.value)
        assertEquals("access", tokenStore.currentAccessToken())
    }

    @Test
    fun `login maps a failed API call to an AedstError instead of leaking the raw throwable`() = runTest {
        coEvery { authServiceClient.login(any(), any()) } returns failure(ConnectException(Code.UNAVAILABLE, "connection reset"))

        val thrown = kotlin.runCatching { repository.login(credentials, deviceId = "device-1") }
            .exceptionOrNull()

        assertEquals(AedstError.Network::class.java, thrown?.javaClass)
    }

    @Test
    fun `logout clears the local session even if the API call fails`() = runTest {
        tokenStore.save(AuthSession("u1", "access", "refresh", 123L))
        coEvery { authServiceClient.logout(any(), any()) } returns failure(ConnectException(Code.UNAVAILABLE, "connection reset"))

        repository.logout()

        assertNull(tokenStore.session.value)
    }

    @Test
    fun `logout clears the local session on success`() = runTest {
        tokenStore.save(AuthSession("u1", "access", "refresh", 123L))
        coEvery { authServiceClient.logout(any(), any()) } returns success(LogoutResponse.getDefaultInstance())

        repository.logout()

        assertNull(tokenStore.session.value)
    }
}
