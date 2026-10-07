package com.aedst.core.common

import kotlin.coroutines.cancellation.CancellationException

/**
 * Result wrapper every layer above `data/repository` returns instead of
 * throwing — mirrors `internal/shared` error handling on the server side.
 *
 * [AedstError.message] is a diagnostic string for logs and crash reports; it
 * is never shown to users. UI maps the error *type* to a localized string
 * (see `core/ui`'s `AedstError.userMessage()`).
 */
sealed interface Outcome<out T> {
    data class Success<T>(val value: T) : Outcome<T>
    data class Failure(val error: AedstError) : Outcome<Nothing>
}

sealed class AedstError(override val message: String, override val cause: Throwable? = null) :
    Throwable(message, cause) {
    /** The server was reached (or should have been) but the call failed: timeouts, 5xx, connection reset. */
    class Network(message: String, cause: Throwable? = null) : AedstError(message, cause)

    /** The device has no usable connection; the operation may be queued and retried when it returns. */
    class Offline(message: String = "Offline", cause: Throwable? = null) : AedstError(message, cause)
    /** No valid session (Connect `UNAUTHENTICATED`): sign in again. */
    class Unauthorized(message: String = "Unauthorized") : AedstError(message)

    /** Signed in but not allowed (Connect `PERMISSION_DENIED`, e.g. a non-admin calling AdminService). */
    class Forbidden(message: String = "Forbidden") : AedstError(message)
    class NotFound(message: String = "Not found") : AedstError(message)

    /** The resource changed or already exists (Connect `ABORTED` / `ALREADY_EXISTS`): re-read and retry. */
    class Conflict(message: String = "Conflict") : AedstError(message)

    /** Throttled by the server (Connect `RESOURCE_EXHAUSTED`); retry later. */
    class RateLimited(message: String = "Rate limited") : AedstError(message)
    class Validation(message: String) : AedstError(message)
    class Unknown(message: String, cause: Throwable? = null) : AedstError(message, cause)
}

inline fun <T, R> Outcome<T>.map(transform: (T) -> R): Outcome<R> = when (this) {
    is Outcome.Success -> Outcome.Success(transform(value))
    is Outcome.Failure -> this
}

inline fun <T> Outcome<T>.onSuccess(action: (T) -> Unit): Outcome<T> {
    if (this is Outcome.Success) action(value)
    return this
}

inline fun <T> Outcome<T>.onFailure(action: (AedstError) -> Unit): Outcome<T> {
    if (this is Outcome.Failure) action(error)
    return this
}

suspend fun <T> runCatchingOutcome(block: suspend () -> T): Outcome<T> = try {
    Outcome.Success(block())
} catch (error: AedstError) {
    Outcome.Failure(error)
} catch (cancellation: CancellationException) {
    throw cancellation
} catch (@Suppress("TooGenericExceptionCaught") throwable: Throwable) {
    Outcome.Failure(AedstError.Unknown(throwable.message ?: "Unknown error", throwable))
}

/** Whether retrying the same operation later can plausibly succeed (drives outbox retry vs. give-up). */
val AedstError.isRetryable: Boolean
    get() = this is AedstError.Network || this is AedstError.Offline || this is AedstError.RateLimited
