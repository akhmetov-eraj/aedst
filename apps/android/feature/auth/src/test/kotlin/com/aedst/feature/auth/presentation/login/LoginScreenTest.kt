package com.aedst.feature.auth.presentation.login

import android.content.Context
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performTextInput
import androidx.test.core.app.ApplicationProvider
import com.aedst.core.common.AedstError
import com.aedst.core.ui.theme.AedstTheme
import com.aedst.feature.auth.R
import com.aedst.feature.auth.domain.model.CredentialsIssue
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import com.aedst.core.ui.R as UiR

/** JVM Compose UI test (Robolectric, English resources): the stateless [LoginScreen] against synthetic state. */
@RunWith(RobolectricTestRunner::class)
@Config(qualifiers = "en")
class LoginScreenTest {

    @get:Rule
    val compose = createComposeRule()

    private val context: Context = ApplicationProvider.getApplicationContext()
    private val intents = mutableListOf<LoginIntent>()

    private fun show(state: LoginState) {
        compose.setContent { AedstTheme { LoginScreen(state = state, onIntent = { intents += it }) } }
    }

    @Test
    fun `renders the title, both fields and the submit button`() {
        show(LoginState())

        compose.onNodeWithText(context.getString(R.string.auth_login_title)).assertExists()
        compose.onNodeWithText(context.getString(R.string.auth_login_email_label)).assertExists()
        compose.onNodeWithText(context.getString(R.string.auth_login_password_label)).assertExists()
        compose.onNodeWithText(context.getString(R.string.auth_login_submit)).assertExists()
    }

    @Test
    fun `typing reports intents`() {
        show(LoginState())

        compose.onNodeWithText(context.getString(R.string.auth_login_email_label)).performTextInput("a@b.com")

        assertEquals(LoginIntent.EmailChanged("a@b.com"), intents.last())
    }

    @Test
    fun `tapping the button submits`() {
        show(LoginState(email = "a@b.com", password = "secret"))

        compose.onNodeWithText(context.getString(R.string.auth_login_submit)).performClick()

        assertEquals(LoginIntent.Submit, intents.last())
    }

    @Test
    fun `the keyboard done action on the password field submits`() {
        show(LoginState(email = "a@b.com", password = "secret"))

        compose.onNodeWithText(context.getString(R.string.auth_login_password_label)).performImeAction()

        assertEquals(LoginIntent.Submit, intents.last())
    }

    @Test
    fun `shows a localized message for an invalid e-mail`() {
        show(LoginState(email = "nope", emailIssue = CredentialsIssue.InvalidEmail))

        compose.onNodeWithText(context.getString(R.string.auth_login_invalid_email)).assertExists()
    }

    @Test
    fun `shows a localized message for an empty password`() {
        show(LoginState(email = "a@b.com", passwordIssue = CredentialsIssue.EmptyPassword))

        compose.onNodeWithText(context.getString(R.string.auth_login_invalid_password)).assertExists()
    }

    @Test
    fun `an error kind is rendered as localized text, never the diagnostic message`() {
        show(LoginState(error = AedstError.Unauthorized("raw server detail: user 42 locked")))

        compose.onNodeWithText(context.getString(UiR.string.aedst_error_unauthorized)).assertExists()
        compose.onNodeWithText("raw server detail", substring = true).assertDoesNotExist()
    }

    @Test
    fun `offline errors get the offline wording`() {
        show(LoginState(error = AedstError.Offline()))

        compose.onNodeWithText(context.getString(UiR.string.aedst_error_offline)).assertExists()
    }

    @Test
    fun `while loading the button keeps its name via description and is disabled`() {
        show(LoginState(email = "a@b.com", password = "secret", isLoading = true))

        compose.onNodeWithContentDescription(context.getString(R.string.auth_login_submit)).assertIsNotEnabled()
    }

    @Test
    fun `the password visibility toggle is announced with a localized description`() {
        show(LoginState(password = "secret"))

        compose.onNodeWithContentDescription(context.getString(UiR.string.aedst_a11y_show_password)).performClick()

        compose.onNodeWithContentDescription(context.getString(UiR.string.aedst_a11y_hide_password)).assertExists()
    }
}
