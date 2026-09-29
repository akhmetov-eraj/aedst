package com.aedst.core.security

/**
 * Facade over EncryptedSharedPreferences (Tink-backed, AES256-GCM values +
 * AES256-SIV keys). Feature modules never touch `SharedPreferences`
 * directly — they depend on this interface, the same way they depend on
 * `core/network`'s [com.aedst.core.network.AuthTokenProvider] rather than OkHttp.
 */
interface EncryptedPreferences {
    fun putString(key: String, value: String)
    fun getString(key: String): String?
    fun remove(key: String)
    fun clear()
}
