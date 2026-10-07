package com.aedst.core.ui.components

import androidx.compose.material3.Surface
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.junit4.accessibility.enableAccessibilityChecks
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.aedst.core.ui.theme.AedstTheme
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/** Runs the Accessibility Test Framework over representative design-system components, light and dark. */
@OptIn(ExperimentalTestApi::class)
@RunWith(AndroidJUnit4::class)
class ComponentAccessibilityTest {

    @get:Rule
    val compose = createComposeRule()

    @Before
    fun enableChecks() {
        compose.enableAccessibilityChecks()
    }

    private fun show(darkTheme: Boolean, content: @androidx.compose.runtime.Composable () -> Unit) {
        compose.setContent { AedstTheme(useDarkTheme = darkTheme) { Surface { content() } } }
        compose.onRoot().performClick()
    }

    @Test
    fun offlineBanner_light() = show(darkTheme = false) { AedstOfflineBanner(isOffline = true) }

    @Test
    fun offlineBanner_dark() = show(darkTheme = true) { AedstOfflineBanner(isOffline = true) }

    @Test
    fun passwordField_light() =
        show(darkTheme = false) { AedstPasswordTextField(value = "secret", onValueChange = {}, label = "Password") }

    @Test
    fun passwordField_dark() =
        show(darkTheme = true) { AedstPasswordTextField(value = "secret", onValueChange = {}, label = "Password") }

    @Test
    fun button_light() = show(darkTheme = false) { AedstButton(text = "Continue", onClick = {}) }

    @Test
    fun button_dark() = show(darkTheme = true) { AedstButton(text = "Continue", onClick = {}) }

    @Test
    fun pagination_light() =
        show(darkTheme = false) { AedstPagination(currentPage = 2, totalPages = 5, onPageChange = {}) }
}
