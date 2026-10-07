package com.aedst.core.ui.components

import androidx.annotation.StringRes
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.res.stringResource
import com.aedst.core.common.AedstError
import com.aedst.core.ui.R

/**
 * The single place an [AedstError] becomes user-facing text. Diagnostic `error.message` strings (English, possibly
 * carrying server detail) are for logs only; users always see the localized string for the error's *kind*.
 * The Connect-code → kind mapping lives in `core/network`'s `toAedstError()`.
 */
@StringRes
fun AedstError.userMessageRes(): Int = when (this) {
    is AedstError.Offline -> R.string.aedst_error_offline
    is AedstError.Network -> R.string.aedst_error_network
    is AedstError.Unauthorized -> R.string.aedst_error_unauthorized
    is AedstError.Forbidden -> R.string.aedst_error_forbidden
    is AedstError.NotFound -> R.string.aedst_error_not_found
    is AedstError.Validation -> R.string.aedst_error_validation
    is AedstError.Conflict -> R.string.aedst_error_conflict
    is AedstError.RateLimited -> R.string.aedst_error_rate_limited
    is AedstError.Unknown -> R.string.aedst_error_unknown
}

@Composable
fun AedstError.userMessage(): String = stringResource(userMessageRes())

/**
 * Shows a transient (non-form) error as a snackbar exactly once, then calls [onShown] so the ViewModel can clear it
 * from state. Errors that belong next to a field (login form) are rendered inline instead.
 *
 * ```
 * AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = { onIntent(Intent.ErrorShown) })
 * ```
 */
@Composable
fun AedstErrorSnackbarEffect(error: AedstError?, hostState: SnackbarHostState, onShown: () -> Unit) {
    val message = error?.userMessage()
    val currentOnShown by rememberUpdatedState(onShown)
    LaunchedEffect(error) {
        if (error != null && message != null) {
            hostState.showSnackbar(message = message, duration = SnackbarDuration.Short)
            currentOnShown()
        }
    }
}
