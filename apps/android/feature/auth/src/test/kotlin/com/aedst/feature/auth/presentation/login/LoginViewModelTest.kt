package com.aedst.feature.auth.presentation.login

import app.cash.turbine.test
import com.aedst.core.analytics.NoOpAnalyticsTracker
import com.aedst.core.testing.FakeDispatcherProvider
import com.aedst.core.testing.MainDispatcherRule
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.aedst.feature.auth.domain.usecase.LoginUseCase
import io.mockk.coEvery
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test

class LoginViewModelTest {

    @get:Rule
    val mainDispatcherRule = MainDispatcherRule()

    private val authRepository: AuthRepository = mockk()
    private val loginUseCase = LoginUseCase(authRepository, FakeDispatcherProvider())
    private val viewModel = LoginViewModel(loginUseCase, NoOpAnalyticsTracker())

    @Test
    fun `submit emits NavigateToHome on success`() = runTest {
        coEvery { authRepository.login(any(), any()) } returns AuthSession("u1", "access", "refresh", 0L)

        viewModel.onIntent(LoginIntent.EmailChanged("a@b.com"))
        viewModel.onIntent(LoginIntent.PasswordChanged("password123"))

        viewModel.effects.test {
            viewModel.onIntent(LoginIntent.Submit)
            assertEquals(LoginEffect.NavigateToHome, awaitItem())
        }
    }

    @Test
    fun `submit surfaces failure message in state`() = runTest {
        coEvery { authRepository.login(any(), any()) } throws com.aedst.core.common.AedstError.Unauthorized()

        viewModel.effects.test {
            viewModel.onIntent(LoginIntent.Submit)
            awaitItem()
        }
        assertEquals("Unauthorized", viewModel.state.value.errorMessage)
    }
}
