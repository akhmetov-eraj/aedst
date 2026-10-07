package com.aedst.core.common

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AedstErrorTest {

    @Test
    fun `network and offline failures are retryable`() {
        assertTrue(AedstError.Network("down").isRetryable)
        assertTrue(AedstError.Offline().isRetryable)
        assertTrue(AedstError.RateLimited().isRetryable)
    }

    @Test
    fun `client, auth and unknown failures are not retryable`() {
        assertFalse(AedstError.Unauthorized().isRetryable)
        assertFalse(AedstError.NotFound().isRetryable)
        assertFalse(AedstError.Validation("bad").isRetryable)
        assertFalse(AedstError.Unknown("bug").isRetryable)
        assertFalse(AedstError.Forbidden().isRetryable)
        assertFalse(AedstError.Conflict().isRetryable)
    }
}
