package com.aedst.feature.auth.presentation.login

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.junit4.accessibility.enableAccessibilityChecks
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.aedst.core.common.AedstError
import com.aedst.core.ui.theme.AedstTheme
import com.aedst.feature.auth.domain.model.CredentialsIssue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumented run of the login screen with the Accessibility Test Framework (ATF) checks switched on: every
 * `perform*`/assert also validates the whole hierarchy for touch-target size, contrast, missing labels, duplicate
 * clickable bounds, etc., and fails the test on a violation. Run:
 * `./gradlew :feature:auth:connectedDebugAndroidTest`
 * (needs an emulator/device; CI runs it on the managed-device lane).
 */
@OptIn(ExperimentalTestApi::class)
@RunWith(AndroidJUnit4::class)
class LoginScreenAccessibilityTest {

    @get:Rule
    val compose = createComposeRule()

    @Before
    fun enableChecks() {
        compose.enableAccessibilityChecks()
    }

    private fun show(state: LoginState) {
        compose.setContent { AedstTheme { LoginScreen(state = state, onIntent = {}) } }
    }

    @Test
    fun emptyForm_passesAccessibilityChecks() {
        show(LoginState())

        compose.onNodeWithText("Log in").performClick()
    }

    @Test
    fun formWithFieldErrors_passesAccessibilityChecks() {
        show(LoginState(email = "nope", emailIssue = CredentialsIssue.InvalidEmail))

        compose.onNodeWithText("Email").performTextInput("x")
    }

    @Test
    fun formWithServerError_passesAccessibilityChecks() {
        show(LoginState(email = "a@b.com", password = "secret", error = AedstError.Unauthorized()))

        compose.onNodeWithText("Log in").performClick()
    }

    @Test
    fun loadingState_passesAccessibilityChecks() {
        show(LoginState(email = "a@b.com", password = "secret", isLoading = true))

        compose.onNodeWithContentDescription("Log in").assertExists()
    }
}
