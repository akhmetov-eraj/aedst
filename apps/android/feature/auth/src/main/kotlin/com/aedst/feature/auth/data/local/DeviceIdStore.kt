package com.aedst.feature.auth.data.local

import com.aedst.core.security.EncryptedPreferences
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.util.UUID
import javax.inject.Inject
import javax.inject.Singleton

/**
 * A stable, random per-installation device identifier sent with login so the server can tell sessions apart
 * (`device_id` in `LoginRequest`). Generated once, then persisted — never derived from hardware identifiers.
 *
 * Stored in [EncryptedPreferences], which is excluded from backup/device transfer (the app sets
 * `allowBackup=false`), so a restored install gets a fresh id rather than impersonating the old device.
 * Cleared only when the app data is cleared; deliberately **not** cleared on logout.
 */
@Singleton
class DeviceIdStore @Inject constructor(private val encryptedPreferences: EncryptedPreferences) {
    private val mutex = Mutex()

    suspend fun get(): String = mutex.withLock {
        encryptedPreferences.getString(KEY_DEVICE_ID)
            ?: UUID.randomUUID().toString().also { encryptedPreferences.putString(KEY_DEVICE_ID, it) }
    }

    private companion object {
        const val KEY_DEVICE_ID = "device.id"
    }
}
