package com.aedst.feature.auth.data.local

import com.aedst.feature.auth.testing.FakeEncryptedPreferences
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.UUID

class DeviceIdStoreTest {

    private val preferences = FakeEncryptedPreferences()

    @Test
    fun `generates a UUID on first use`() = runTest {
        val id = DeviceIdStore(preferences).get()

        UUID.fromString(id) // throws if not a UUID
        assertTrue(id.length in 1..128) // server-side protovalidate bound for device_id
    }

    @Test
    fun `returns the same id on every call and across store instances`() = runTest {
        val first = DeviceIdStore(preferences).get()

        assertEquals(first, DeviceIdStore(preferences).get())
        assertEquals(first, DeviceIdStore(preferences).get())
    }

    @Test
    fun `concurrent first calls agree on a single id`() = runTest {
        val store = DeviceIdStore(preferences)

        val ids = (1..20).map { async { store.get() } }.awaitAll()

        assertEquals(1, ids.toSet().size)
    }

    @Test
    fun `survives sign-out because it is not part of the auth session keys`() = runTest {
        val id = DeviceIdStore(preferences).get()

        preferences.remove("auth.user_id", "auth.access_token", "auth.refresh_token", "auth.access_expires_at")

        assertEquals(id, DeviceIdStore(preferences).get())
    }

    @Test
    fun `different installations get different ids`() = runTest {
        val first = DeviceIdStore(FakeEncryptedPreferences()).get()
        val second = DeviceIdStore(FakeEncryptedPreferences()).get()

        assertNotEquals(first, second)
    }
}
