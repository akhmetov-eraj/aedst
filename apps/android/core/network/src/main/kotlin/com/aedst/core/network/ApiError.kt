package com.aedst.core.network

import com.aedst.core.common.AedstError
import com.connectrpc.Code
import com.connectrpc.ConnectException
import com.connectrpc.ResponseMessage
import com.connectrpc.getOrThrow
import kotlinx.coroutines.CancellationException
import java.io.IOException
import java.net.SocketTimeoutException
import java.net.UnknownHostException

/**
 * Maps a ConnectRPC/transport failure to the app-wide [AedstError] taxonomy (mirrors platform error mapping on the
 * server, and Apple's `NetworkErrorMapping`). The resulting `message` is diagnostic only — UI localizes by type
 * (`core/ui`'s `AedstError.userMessage()`), and shows it in a snackbar via `AedstErrorSnackbarEffect`.
 *
 * | Connect code | AedstError |
 * |---|---|
 * | UNAUTHENTICATED | Unauthorized |
 * | PERMISSION_DENIED | Forbidden |
 * | NOT_FOUND, UNIMPLEMENTED | NotFound |
 * | INVALID_ARGUMENT, FAILED_PRECONDITION, OUT_OF_RANGE | Validation |
 * | ABORTED, ALREADY_EXISTS | Conflict |
 * | RESOURCE_EXHAUSTED | RateLimited |
 * | anything else | Network (or Offline when the cause is an unknown host) |
 */
fun Throwable.toAedstError(): AedstError = when (this) {
    is AedstError -> this
    is ConnectException -> when (code) {
        Code.UNAUTHENTICATED -> AedstError.Unauthorized()
        Code.PERMISSION_DENIED -> AedstError.Forbidden()
        Code.NOT_FOUND, Code.UNIMPLEMENTED -> AedstError.NotFound()
        Code.INVALID_ARGUMENT, Code.FAILED_PRECONDITION, Code.OUT_OF_RANGE ->
            AedstError.Validation(message ?: "Invalid request")
        Code.ABORTED, Code.ALREADY_EXISTS -> AedstError.Conflict(message ?: "Conflict")
        Code.RESOURCE_EXHAUSTED -> AedstError.RateLimited(message ?: "Rate limited")
        else -> (cause as? IOException)?.toAedstError() ?: AedstError.Network(message ?: "Network error", this)
    }
    is UnknownHostException -> AedstError.Offline(message ?: "Unknown host", this)
    is SocketTimeoutException -> AedstError.Network(message ?: "Timed out", this)
    is IOException -> AedstError.Network(message ?: "I/O error", this)
    else -> AedstError.Network(message ?: "Network error", this)
}

/**
 * Runs one unary RPC and unwraps it: returns the message or throws the mapped [AedstError]. Use inside a
 * repository; `runCatchingOutcome`/`UseCase` turn the throw into an `Outcome.Failure`.
 *
 * ```
 * val task = rpc { client.getTask(getTaskRequest { id = taskId }) }.task
 * ```
 */
suspend fun <T> rpc(call: suspend () -> ResponseMessage<T>): T = try {
    call().getOrThrow()
} catch (cancellation: CancellationException) {
    throw cancellation
} catch (@Suppress("TooGenericExceptionCaught") throwable: Throwable) {
    throw throwable.toAedstError()
}
