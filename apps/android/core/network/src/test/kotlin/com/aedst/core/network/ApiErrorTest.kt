package com.aedst.core.network

import com.aedst.core.common.AedstError
import com.connectrpc.Code
import com.connectrpc.ConnectException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.IOException

class ApiErrorTest {

    @Test
    fun `UNAUTHENTICATED maps to Unauthorized`() {
        assertTrue(connectError(Code.UNAUTHENTICATED).toAedstError() is AedstError.Unauthorized)
    }

    @Test
    fun `PERMISSION_DENIED maps to Forbidden`() {
        assertTrue(connectError(Code.PERMISSION_DENIED).toAedstError() is AedstError.Forbidden)
    }

    @Test
    fun `ABORTED and ALREADY_EXISTS map to Conflict`() {
        assertTrue(connectError(Code.ABORTED).toAedstError() is AedstError.Conflict)
        assertTrue(connectError(Code.ALREADY_EXISTS).toAedstError() is AedstError.Conflict)
    }

    @Test
    fun `RESOURCE_EXHAUSTED maps to RateLimited`() {
        assertTrue(connectError(Code.RESOURCE_EXHAUSTED).toAedstError() is AedstError.RateLimited)
    }

    @Test
    fun `NOT_FOUND maps to NotFound`() {
        assertTrue(connectError(Code.NOT_FOUND).toAedstError() is AedstError.NotFound)
    }

    @Test
    fun `INVALID_ARGUMENT maps to Validation carrying the server message`() {
        val error = connectError(Code.INVALID_ARGUMENT, message = "email is invalid").toAedstError()
        assertTrue(error is AedstError.Validation)
        assertEquals("email is invalid", error.message)
    }

    @Test
    fun `FAILED_PRECONDITION maps to Validation`() {
        assertTrue(connectError(Code.FAILED_PRECONDITION).toAedstError() is AedstError.Validation)
    }

    @Test
    fun `other codes map to Network with the server message`() {
        val error = connectError(Code.UNAVAILABLE, message = "server error").toAedstError()
        assertTrue(error is AedstError.Network)
        assertEquals("server error", error.message)
    }

    @Test
    fun `non-Connect throwable maps to Network`() {
        val error = IOException("timeout").toAedstError()
        assertTrue(error is AedstError.Network)
        assertEquals("timeout", error.message)
    }

    @Test
    fun `an already-mapped AedstError passes through unchanged`() {
        val original = AedstError.Unauthorized("already mapped")
        assertEquals(original, original.toAedstError())
    }

    private fun connectError(code: Code, message: String = "error") = ConnectException(code = code, message = message)
}
