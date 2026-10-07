package com.aedst.feature.auth.domain.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class CredentialsValidatorTest {

    @Test
    fun `accepts ordinary e-mail addresses`() {
        listOf("a@b.co", "first.last+tag@example.com", "  padded@example.com  ").forEach {
            assertNull(it, CredentialsValidator.emailIssue(it))
        }
    }

    @Test
    fun `rejects malformed e-mail addresses`() {
        listOf("", "plain", "a@b", "@b.com", "a@.com", "a b@c.com", "a@@b.com").forEach {
            assertEquals(it, CredentialsIssue.InvalidEmail, CredentialsValidator.emailIssue(it))
        }
    }

    @Test
    fun `rejects e-mail longer than the server limit`() {
        val tooLong = "a".repeat(CredentialsValidator.MAX_EMAIL_LENGTH) + "@example.com"

        assertEquals(CredentialsIssue.InvalidEmail, CredentialsValidator.emailIssue(tooLong))
    }

    @Test
    fun `password must be non-empty and within the server limit`() {
        assertEquals(CredentialsIssue.EmptyPassword, CredentialsValidator.passwordIssue(""))
        assertNull(CredentialsValidator.passwordIssue("x"))
        assertNull(CredentialsValidator.passwordIssue("x".repeat(CredentialsValidator.MAX_PASSWORD_LENGTH)))
        assertEquals(
            CredentialsIssue.PasswordTooLong,
            CredentialsValidator.passwordIssue("x".repeat(CredentialsValidator.MAX_PASSWORD_LENGTH + 1)),
        )
    }

    @Test
    fun `a password made only of spaces is allowed - trimming would change the secret`() {
        assertNull(CredentialsValidator.passwordIssue("   "))
    }
}
