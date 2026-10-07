package com.aedst.feature.auth.presentation.login

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.analytics.AnalyticsTracker
import com.aedst.core.common.Outcome
import com.aedst.feature.auth.domain.model.CredentialsValidator
import com.aedst.feature.auth.domain.usecase.LoginParams
import com.aedst.feature.auth.domain.usecase.LoginUseCase
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * UDF loop: intents flow in via [onIntent], state flows out via [state] for
 * `collectAsStateWithLifecycle`, one-shot navigation events flow out via
 * [effects] (a Channel, not a StateFlow, so they are never replayed).
 */
@HiltViewModel
class LoginViewModel @Inject constructor(
    private val loginUseCase: LoginUseCase,
    private val analyticsTracker: AnalyticsTracker,
) : ViewModel() {

    private val mutableState = MutableStateFlow(LoginState())
    val state: StateFlow<LoginState> = mutableState.asStateFlow()

    private val effectChannel = Channel<LoginEffect>(Channel.BUFFERED)
    val effects = effectChannel.receiveAsFlow()

    fun onIntent(intent: LoginIntent) {
        when (intent) {
            is LoginIntent.EmailChanged -> mutableState.update {
                it.copy(email = intent.value, emailIssue = null, error = null)
            }
            is LoginIntent.PasswordChanged -> mutableState.update {
                it.copy(password = intent.value, passwordIssue = null, error = null)
            }
            LoginIntent.Submit -> submit()
        }
    }

    private fun submit() {
        val current = mutableState.value
        if (current.isLoading) return // ignore double taps / IME "done" racing the button

        val emailIssue = CredentialsValidator.emailIssue(current.email)
        val passwordIssue = CredentialsValidator.passwordIssue(current.password)
        if (emailIssue != null || passwordIssue != null) {
            mutableState.update { it.copy(emailIssue = emailIssue, passwordIssue = passwordIssue) }
            return
        }

        mutableState.update { it.copy(isLoading = true, error = null) }
        viewModelScope.launch {
            when (val outcome = loginUseCase(LoginParams(current.email, current.password))) {
                is Outcome.Success -> {
                    analyticsTracker.logEvent("login_success")
                    mutableState.update { it.copy(isLoading = false, password = "") }
                    effectChannel.send(LoginEffect.NavigateToHome)
                }
                is Outcome.Failure -> {
                    // Record the failure *kind*, not the message: the message may echo server detail or the e-mail.
                    analyticsTracker.logEvent("login_failure", mapOf("error" to outcome.error::class.simpleName))
                    mutableState.update { it.copy(isLoading = false, error = outcome.error) }
                }
            }
        }
    }
}
