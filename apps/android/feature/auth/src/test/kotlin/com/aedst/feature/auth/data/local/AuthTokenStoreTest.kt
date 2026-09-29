package com.aedst.feature.auth.data.local

import com.aedst.core.security.EncryptedPreferences
import com.aedst.feature.auth.domain.model.AuthSession
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

/** In-memory [EncryptedPreferences] fake — no real Keystore access under JVM unit tests. */
private class FakeEncryptedPreferences : EncryptedPreferences {
    private val values = mutableMapOf<String, String>()

    override fun putString(key: String, value: String) {
        values[key] = value
    }

    override fun getString(key: String): String? = values[key]

    override fun remove(key: String) {
        values.remove(key)
    }

    override fun clear() {
        values.clear()
    }
}

class AuthTokenStoreTest {

    private lateinit var preferences: FakeEncryptedPreferences

    @Before
    fun setUp() {
        preferences = FakeEncryptedPreferences()
    }

    @Test
    fun `starts with no session when preferences are empty`() {
        val store = AuthTokenStore(preferences)

        assertNull(store.session.value)
        assertNull(store.currentAccessToken())
    }

    @Test
    fun `save persists the session and exposes it via the session flow and currentAccessToken`() {
        val store = AuthTokenStore(preferences)
        val session = AuthSession("u1", "access-token", "refresh-token", 123L)

        store.save(session)

        assertEquals(session, store.session.value)
        assertEquals("access-token", store.currentAccessToken())
    }

    @Test
    fun `clear removes the session from state and preferences`() {
        val store = AuthTokenStore(preferences)
        store.save(AuthSession("u1", "access-token", "refresh-token", 123L))

        store.clear()

        assertNull(store.session.value)
        assertNull(store.currentAccessToken())
        assertNull(preferences.getString("auth.access_token"))
    }

    @Test
    fun `a freshly constructed store restores a previously saved session from preferences`() {
        AuthTokenStore(preferences).save(AuthSession("u1", "access-token", "refresh-token", 123L))

        val restored = AuthTokenStore(preferences)

        assertEquals(AuthSession("u1", "access-token", "refresh-token", 123L), restored.session.value)
    }

    @Test
    fun `restore ignores a partially written session`() {
        preferences.putString("auth.user_id", "u1")
        preferences.putString("auth.access_token", "access-token")
        // refresh token and expiry deliberately missing

        val store = AuthTokenStore(preferences)

        assertNull(store.session.value)
    }
}
