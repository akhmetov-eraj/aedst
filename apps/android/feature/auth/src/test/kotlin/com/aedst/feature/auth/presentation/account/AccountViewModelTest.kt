package com.aedst.feature.auth.presentation.account

import app.cash.turbine.test
import com.aedst.core.analytics.AnalyticsConsentState
import com.aedst.core.analytics.AnalyticsConsentStore
import com.aedst.core.analytics.NoOpAnalyticsTracker
import com.aedst.core.testing.FakeDispatcherProvider
import com.aedst.core.testing.MainDispatcherRule
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.aedst.feature.auth.domain.usecase.LogoutUseCase
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test

class AccountViewModelTest {

    @get:Rule
    val mainDispatcherRule = MainDispatcherRule()

    private val sessionFlow = MutableStateFlow<AuthSession?>(AuthSession("u1", "access", "refresh", 0L))
    private val authRepository: AuthRepository = mockk {
        every { session } returns sessionFlow
        coEvery { logout() } returns Unit
    }
    private val consentStore = object : AnalyticsConsentStore {
        override val consent = MutableStateFlow(AnalyticsConsentState.UNSET)
        override suspend fun setConsent(granted: Boolean) {
            consent.value = if (granted) AnalyticsConsentState.GRANTED else AnalyticsConsentState.DENIED
        }
    }
    private val viewModel = AccountViewModel(
        authRepository,
        LogoutUseCase(authRepository, FakeDispatcherProvider(mainDispatcherRule.testDispatcher)),
        NoOpAnalyticsTracker(),
        consentStore,
    )

    @Test
    fun `exposes the signed-in user id`() = runTest {
        viewModel.userId.test {
            assertEquals(null, awaitItem()) // initial, before the flow is collected
            assertEquals("u1", awaitItem())
        }
    }

    @Test
    fun `signOut calls logout once and finishes not-signing-out`() = runTest {
        viewModel.signOut()
        viewModel.signOut() // double tap while in flight
        mainDispatcherRule.testDispatcher.scheduler.advanceUntilIdle()

        coVerify(exactly = 1) { authRepository.logout() }
        assertFalse(viewModel.isSigningOut.value)
    }
}
