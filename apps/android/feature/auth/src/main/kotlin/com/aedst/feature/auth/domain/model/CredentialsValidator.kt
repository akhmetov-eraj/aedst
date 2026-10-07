package com.aedst.feature.auth.domain.model

/** Which field of the login form is invalid; the UI maps each to a localized message. */
enum class CredentialsIssue { InvalidEmail, EmptyPassword, PasswordTooLong }

/**
 * Client-side pre-check that mirrors the server's protovalidate rules for `LoginRequest` (email format,
 * `max_len: 254`; password `min_len: 1`, `max_len: 128`), so obviously bad input never costs a round trip.
 * The server remains the authority — this only saves latency and gives inline field errors.
 */
object CredentialsValidator {
    const val MAX_EMAIL_LENGTH = 254
    const val MAX_PASSWORD_LENGTH = 128

    private val emailPattern = Regex("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$")

    fun emailIssue(email: String): CredentialsIssue? {
        val valid = email.length <= MAX_EMAIL_LENGTH && emailPattern.matches(email.trim())
        return if (valid) null else CredentialsIssue.InvalidEmail
    }

    fun passwordIssue(password: String): CredentialsIssue? = when {
        password.isEmpty() -> CredentialsIssue.EmptyPassword
        password.length > MAX_PASSWORD_LENGTH -> CredentialsIssue.PasswordTooLong
        else -> null
    }
}
