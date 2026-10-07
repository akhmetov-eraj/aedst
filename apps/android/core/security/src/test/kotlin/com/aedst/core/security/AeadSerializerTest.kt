package com.aedst.core.security

import androidx.datastore.core.CorruptionException
import com.google.crypto.tink.Aead
import com.google.crypto.tink.KeysetHandle
import com.google.crypto.tink.RegistryConfiguration
import com.google.crypto.tink.aead.AeadConfig
import com.google.crypto.tink.aead.PredefinedAeadParameters
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream

class AeadSerializerTest {

    private fun newAead(): Aead {
        AeadConfig.register()
        return KeysetHandle.generateNew(PredefinedAeadParameters.AES256_GCM)
            .getPrimitive(RegistryConfiguration.get(), Aead::class.java)
    }

    private suspend fun seal(serializer: AeadSerializer, data: SecretEntries): ByteArray {
        val out = ByteArrayOutputStream()
        serializer.writeTo(data, out)
        return out.toByteArray()
    }

    @Test
    fun `round trips entries and never stores plaintext`() = runTest {
        val serializer = AeadSerializer(lazyOf(newAead()))
        val data = SecretEntries(mapOf("auth.access_token" to "super-secret-token"))

        val sealed = seal(serializer, data)

        assertFalse(sealed.decodeToString().contains("super-secret-token"))
        assertEquals(data, serializer.readFrom(ByteArrayInputStream(sealed)))
    }

    @Test
    fun `empty file reads as the default value`() = runTest {
        val serializer = AeadSerializer(lazyOf(newAead()))

        assertEquals(SecretEntries(), serializer.readFrom(ByteArrayInputStream(ByteArray(0))))
    }

    @Test
    fun `data sealed with another key is reported as corruption`() = runTest {
        val sealed = seal(AeadSerializer(lazyOf(newAead())), SecretEntries(mapOf("k" to "v")))

        assertThrows(CorruptionException::class.java) {
            kotlinx.coroutines.runBlocking { AeadSerializer(lazyOf(newAead())).readFrom(ByteArrayInputStream(sealed)) }
        }
    }
}
