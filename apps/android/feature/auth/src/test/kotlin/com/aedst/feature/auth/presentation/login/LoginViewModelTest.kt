package com.aedst.feature.auth.presentation.login

import app.cash.turbine.test
import com.aedst.core.analytics.NoOpAnalyticsTracker
import com.aedst.core.common.AedstError
import com.aedst.core.testing.FakeDispatcherProvider
import com.aedst.core.testing.MainDispatcherRule
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
import com.aedst.feature.auth.domain.model.CredentialsIssue
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.aedst.feature.auth.domain.usecase.LoginUseCase
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class LoginViewModelTest {

    @get:Rule
    val mainDispatcherRule = MainDispatcherRule()

    private val authRepository: AuthRepository = mockk()
    private val loginUseCase = LoginUseCase(authRepository, FakeDispatcherProvider(mainDispatcherRule.testDispatcher))
    private val viewModel = LoginViewModel(loginUseCase, NoOpAnalyticsTracker())

    private fun fillValidForm() {
        viewModel.onIntent(LoginIntent.EmailChanged("a@b.com"))
        viewModel.onIntent(LoginIntent.PasswordChanged("password123"))
    }

    @Test
    fun `submit emits NavigateToHome on success`() = runTest {
        coEvery { authRepository.login(any()) } returns AuthSession("u1", "access", "refresh", 0L)
        fillValidForm()

        viewModel.effects.test {
            viewModel.onIntent(LoginIntent.Submit)
            assertEquals(LoginEffect.NavigateToHome, awaitItem())
        }
    }

    @Test
    fun `success clears the password from state`() = runTest {
        coEvery { authRepository.login(any()) } returns AuthSession("u1", "access", "refresh", 0L)
        fillValidForm()

        viewModel.effects.test {
            viewModel.onIntent(LoginIntent.Submit)
            awaitItem()
        }

        assertEquals("", viewModel.state.value.password)
        assertFalse(viewModel.state.value.isLoading)
    }

    @Test
    fun `failure is exposed as an error kind in state, not as text`() = runTest {
        coEvery { authRepository.login(any()) } throws AedstError.Unauthorized()
        fillValidForm()

        viewModel.onIntent(LoginIntent.Submit)
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()

        assertTrue(viewModel.state.value.error is AedstError.Unauthorized)
        assertFalse(viewModel.state.value.isLoading)
    }

    @Test
    fun `invalid input never reaches the repository and flags each field`() = runTest {
        viewModel.onIntent(LoginIntent.EmailChanged("not-an-email"))

        viewModel.onIntent(LoginIntent.Submit)
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()

        assertEquals(CredentialsIssue.InvalidEmail, viewModel.state.value.emailIssue)
        assertEquals(CredentialsIssue.EmptyPassword, viewModel.state.value.passwordIssue)
        assertFalse(viewModel.state.value.isLoading)
        coVerify(exactly = 0) { authRepository.login(any()) }
    }

    @Test
    fun `editing a field clears its issue and the previous error`() = runTest {
        coEvery { authRepository.login(any()) } throws AedstError.Network("down")
        fillValidForm()
        viewModel.onIntent(LoginIntent.Submit)
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()

        viewModel.onIntent(LoginIntent.PasswordChanged("password1234"))

        assertNull(viewModel.state.value.error)
        assertNull(viewModel.state.value.passwordIssue)
    }

    @Test
    fun `a second submit while one is in flight is ignored`() = runTest {
        val gate = CompletableDeferred<AuthSession>()
        coEvery { authRepository.login(any()) } coAnswers { gate.await() }
        fillValidForm()

        viewModel.onIntent(LoginIntent.Submit)
        viewModel.onIntent(LoginIntent.Submit)
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()
        gate.complete(AuthSession("u1", "access", "refresh", 0L))
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { authRepository.login(Credentials("a@b.com", "password123")) }
    }

    @Test
    fun `isLoading is true while the request is in flight`() = runTest {
        val gate = CompletableDeferred<AuthSession>()
        coEvery { authRepository.login(any()) } coAnswers { gate.await() }
        fillValidForm()

        viewModel.onIntent(LoginIntent.Submit)
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()

        assertTrue(viewModel.state.value.isLoading)
        gate.complete(AuthSession("u1", "access", "refresh", 0L))
    }
}
