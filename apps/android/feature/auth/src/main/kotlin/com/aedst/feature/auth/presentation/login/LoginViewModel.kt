package com.aedst.feature.auth.presentation.login

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.analytics.AnalyticsTracker
import com.aedst.core.common.Outcome
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
import java.util.UUID
import javax.inject.Inject

/**
 * UDF loop: intents flow in via [onIntent], state flows out via [state] for
 * `collectAsStateWithLifecycle`, one-shot navigation/snackbar events flow
 * out via [effects] (a Channel, not a StateFlow, so they are never replayed).
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

    private val deviceId by lazy { UUID.randomUUID().toString() }

    fun onIntent(intent: LoginIntent) {
        when (intent) {
            is LoginIntent.EmailChanged -> mutableState.update { it.copy(email = intent.value, errorMessage = null) }
            is LoginIntent.PasswordChanged -> mutableState.update { it.copy(password = intent.value, errorMessage = null) }
            LoginIntent.Submit -> submit()
        }
    }

    private fun submit() {
        val current = mutableState.value
        viewModelScope.launch {
            mutableState.update { it.copy(isLoading = true, errorMessage = null) }
            val params = LoginParams(current.email, current.password, deviceId)
            when (val outcome = loginUseCase(params)) {
                is Outcome.Success -> {
                    analyticsTracker.logEvent("login_success")
                    mutableState.update { it.copy(isLoading = false) }
                    effectChannel.send(LoginEffect.NavigateToHome)
                }
                is Outcome.Failure -> {
                    val message = outcome.error.message
                    mutableState.update { it.copy(isLoading = false, errorMessage = message) }
                    effectChannel.send(LoginEffect.ShowError(message))
                }
            }
        }
    }
}
