package com.aedst.feature.auth.presentation.login

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.autofill.ContentType
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.contentType
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.ui.components.AedstButton
import com.aedst.core.ui.components.AedstPasswordTextField
import com.aedst.core.ui.components.AedstTextField
import com.aedst.core.ui.components.userMessage
import com.aedst.feature.auth.R
import com.aedst.feature.auth.domain.model.CredentialsIssue

@Composable
fun LoginRoute(onLoggedIn: () -> Unit, viewModel: LoginViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val currentOnLoggedIn by rememberUpdatedState(onLoggedIn)

    LaunchedEffect(viewModel) {
        viewModel.effects.collect { effect ->
            when (effect) {
                LoginEffect.NavigateToHome -> currentOnLoggedIn()
            }
        }
    }

    LoginScreen(state = state, onIntent = viewModel::onIntent)
}

@Composable
internal fun LoginScreen(state: LoginState, onIntent: (LoginIntent) -> Unit) {
    // Contents are capped at a phone-form width and centred, so tablets/foldables/landscape don't stretch the form.
    Box(
        modifier = Modifier
            .fillMaxSize()
            .safeDrawingPadding()
            .imePadding()
            .verticalScroll(rememberScrollState()),
        contentAlignment = Alignment.TopCenter,
    ) {
        Column(
            modifier = Modifier
                .widthIn(max = FORM_MAX_WIDTH)
                .fillMaxWidth()
                .padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Text(
                text = stringResource(R.string.auth_login_title),
                style = MaterialTheme.typography.headlineMedium,
                modifier = Modifier.semantics { heading() },
            )
            LoginFields(state = state, onIntent = onIntent)
            AedstButton(
                text = stringResource(R.string.auth_login_submit),
                onClick = { onIntent(LoginIntent.Submit) },
                loading = state.isLoading,
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

@Composable
private fun LoginFields(state: LoginState, onIntent: (LoginIntent) -> Unit) {
    val emailMessage = state.emailIssue?.let { stringResource(R.string.auth_login_invalid_email) }
    // A server/transport error is shown under the password field; a local password issue takes precedence.
    val passwordMessage = when (state.passwordIssue) {
        CredentialsIssue.EmptyPassword -> stringResource(R.string.auth_login_invalid_password)
        CredentialsIssue.PasswordTooLong -> stringResource(R.string.auth_login_password_too_long)
        else -> state.error?.userMessage()
    }

    AedstTextField(
        value = state.email,
        onValueChange = { onIntent(LoginIntent.EmailChanged(it)) },
        label = stringResource(R.string.auth_login_email_label),
        supportingText = emailMessage,
        isError = emailMessage != null,
        enabled = !state.isLoading,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Email, imeAction = ImeAction.Next),
        modifier = Modifier
            .fillMaxWidth()
            .semantics { contentType = ContentType.EmailAddress },
    )
    AedstPasswordTextField(
        value = state.password,
        onValueChange = { onIntent(LoginIntent.PasswordChanged(it)) },
        label = stringResource(R.string.auth_login_password_label),
        supportingText = passwordMessage,
        isError = passwordMessage != null,
        enabled = !state.isLoading,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { onIntent(LoginIntent.Submit) }),
        modifier = Modifier
            .fillMaxWidth()
            // Errors appear/disappear without focus change; announce them to TalkBack.
            .semantics {
                contentType = ContentType.Password
                liveRegion = LiveRegionMode.Polite
            },
    )
}

private val FORM_MAX_WIDTH = 480.dp
