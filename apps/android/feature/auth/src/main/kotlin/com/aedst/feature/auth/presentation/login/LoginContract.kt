package com.aedst.feature.auth.presentation.login

import com.aedst.core.common.AedstError
import com.aedst.feature.auth.domain.model.CredentialsIssue

/**
 * MVI contract: State is the single source of render truth, Intent is every user action, Effect is one-shot
 * (navigation). Errors live in State only — they are rendered next to the form and must survive rotation, so
 * they are not one-shot effects. State carries the error *kind* ([AedstError]) and field issues, never a
 * pre-rendered string: the UI localizes them.
 */
data class LoginState(
    val email: String = "",
    val password: String = "",
    val isLoading: Boolean = false,
    val emailIssue: CredentialsIssue? = null,
    val passwordIssue: CredentialsIssue? = null,
    val error: AedstError? = null,
)

sealed interface LoginIntent {
    data class EmailChanged(val value: String) : LoginIntent
    data class PasswordChanged(val value: String) : LoginIntent
    data object Submit : LoginIntent
}

sealed interface LoginEffect {
    data object NavigateToHome : LoginEffect
}
