package com.aedst.core.network

import com.aedst.core.common.AedstError
import com.connectrpc.Code
import com.connectrpc.ConnectException

/** Maps a ConnectRPC failure to the app-wide [AedstError] taxonomy (mirrors platform error mapping on the server, and Apple's `NetworkErrorMapping`). */
fun Throwable.toAedstError(): AedstError = when (this) {
    is AedstError -> this
    is ConnectException -> when (code) {
        Code.UNAUTHENTICATED, Code.PERMISSION_DENIED -> AedstError.Unauthorized()
        Code.NOT_FOUND, Code.UNIMPLEMENTED -> AedstError.NotFound()
        Code.INVALID_ARGUMENT, Code.FAILED_PRECONDITION -> AedstError.Validation(message ?: "Invalid request")
        else -> AedstError.Network(message ?: "Network error", this)
    }
    else -> AedstError.Network(message ?: "Network error", this)
}
