package com.aedst.feature.auth.presentation.account

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.analytics.AnalyticsConsentState
import com.aedst.core.analytics.AnalyticsConsentStore
import com.aedst.core.analytics.AnalyticsTracker
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.aedst.feature.auth.domain.usecase.LogoutUseCase
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class AccountViewModel @Inject constructor(
    authRepository: AuthRepository,
    private val logoutUseCase: LogoutUseCase,
    private val analyticsTracker: AnalyticsTracker,
    private val consentStore: AnalyticsConsentStore,
) : ViewModel() {

    private val signingOut = MutableStateFlow(false)

    val userId: StateFlow<String?> = authRepository.session
        .map { it?.userId }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), null)
    val isSigningOut: StateFlow<Boolean> = signingOut.asStateFlow()

    /** Product analytics is opt-in (unset counts as "no"). */
    val isAnalyticsEnabled: StateFlow<Boolean> = consentStore.consent
        .map { it == AnalyticsConsentState.GRANTED }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), false)

    fun setAnalyticsEnabled(enabled: Boolean) {
        viewModelScope.launch { consentStore.setConsent(enabled) }
    }

    fun signOut() {
        if (signingOut.value) return
        signingOut.update { true }
        viewModelScope.launch {
            analyticsTracker.logEvent("logout")
            logoutUseCase()
            signingOut.update { false }
        }
    }

    private companion object {
        const val STOP_TIMEOUT_MS = 5_000L
    }
}
