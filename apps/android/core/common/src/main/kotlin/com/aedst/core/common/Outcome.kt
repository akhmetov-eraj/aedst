package com.aedst.core.common

/**
 * Result wrapper every layer above `data/repository` returns instead of
 * throwing — mirrors `internal/shared` error handling on the server side.
 */
sealed interface Outcome<out T> {
    data class Success<T>(val value: T) : Outcome<T>
    data class Failure(val error: AedstError) : Outcome<Nothing>
}

sealed class AedstError(override val message: String, override val cause: Throwable? = null) : Throwable(message, cause) {
    class Network(message: String, cause: Throwable? = null) : AedstError(message, cause)
    class Unauthorized(message: String = "Unauthorized") : AedstError(message)
    class NotFound(message: String = "Not found") : AedstError(message)
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
} catch (throwable: Throwable) {
    Outcome.Failure(AedstError.Unknown(throwable.message ?: "Unknown error", throwable))
}
