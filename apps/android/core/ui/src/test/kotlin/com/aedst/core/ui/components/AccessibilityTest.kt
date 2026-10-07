package com.aedst.core.ui.components

import android.content.Context
import androidx.compose.material3.Surface
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ApplicationProvider
import com.aedst.core.ui.R
import com.aedst.core.ui.theme.AedstTheme
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

/** Semantics-level accessibility checks for the design-system components that carry non-visual text. */
@RunWith(RobolectricTestRunner::class)
@Config(qualifiers = "en")
class AccessibilityTest {

    @get:Rule
    val compose = createComposeRule()

    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun show(content: @androidx.compose.runtime.Composable () -> Unit) {
        compose.setContent { AedstTheme { Surface { content() } } }
    }

    @Test
    fun `offline banner is hidden while online`() {
        show { AedstOfflineBanner(isOffline = false) }

        compose.onNodeWithText(context.getString(R.string.aedst_offline_banner)).assertDoesNotExist()
    }

    @Test
    fun `offline banner is a polite live region so TalkBack announces it`() {
        show { AedstOfflineBanner(isOffline = true) }

        compose.onNodeWithText(context.getString(R.string.aedst_offline_banner))
            .assert(SemanticsMatcher.expectValue(SemanticsProperties.LiveRegion, LiveRegionMode.Polite))
    }

    @Test
    fun `pagination arrows have localized descriptions`() {
        show { AedstPagination(currentPage = 2, totalPages = 5, onPageChange = {}) }

        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_previous_page)).assertExists()
        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_next_page)).assertExists()
        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_page, 3)).assertExists()
    }

    @Test
    fun `expandable section announces the action it will perform and flips after a click`() {
        show { AedstExpandableSection(title = "Details") { } }

        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_expand)).performClick()

        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_collapse)).assertExists()
    }

    @Test
    fun `otp input reports progress`() {
        var value by mutableStateOf("12")
        show { AedstOtpInput(value = value, onValueChange = { value = it }, length = 6) }

        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_otp_progress, 2, 6)).assertExists()
    }

    @Test
    fun `avatar description includes the initials`() {
        show { AedstAvatar(initials = "ab") }

        compose.onNodeWithContentDescription(context.getString(R.string.aedst_a11y_avatar_named, "AB")).assertExists()
    }

    @Test
    fun `descriptions follow the device locale`() {
        val english = context.getString(R.string.aedst_a11y_next_page)
        RuntimeEnvironment.setQualifiers("+ru")

        val russian = context.getString(R.string.aedst_a11y_next_page)

        assertEquals("Next page", english)
        assertEquals("Следующая страница", russian)
    }
}
