package com.aedst.core.media.upload

import androidx.work.ListenableWorker
import com.aedst.core.common.AedstError
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class UploadWorkDataTest {

    @Test
    fun `presigned headers survive the WorkManager Data round trip`() {
        val headers = mapOf("Content-Type" to "image/webp", "x-amz-checksum-sha256" to "abc=")
        assertEquals(headers, UploadWorkData.decodeHeaders(UploadWorkData.encodeHeaders(headers)))
        assertEquals(emptyMap<String, String>(), UploadWorkData.decodeHeaders("not json"))
        assertEquals(emptyMap<String, String>(), UploadWorkData.decodeHeaders(null))
    }

    @Test
    fun `retryable errors retry until the attempt budget is spent, others fail at once`() {
        assertTrue(UploadWorkData.resultFor(AedstError.Network("x"), 0) is ListenableWorker.Result.Retry)
        assertTrue(
            UploadWorkData.resultFor(AedstError.Network("x"), UploadWorkData.MAX_ATTEMPTS) is
                ListenableWorker.Result.Failure,
        )
        assertTrue(UploadWorkData.resultFor(AedstError.Validation("x"), 0) is ListenableWorker.Result.Failure)
    }
}
