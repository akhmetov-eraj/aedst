package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Thin wrapper over `Modifier.verticalScroll`, kept as a named component (rather than callers
 * reaching for the modifier directly) for two reasons: it gives every scrollable region in the
 * app one consistent call site, and it is the seam where a scrollbar affordance can be added
 * later without touching every call site.
 */
@Composable
fun AedstScrollArea(modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = modifier.verticalScroll(rememberScrollState()),
        content = content,
    )
}

/** Horizontal counterpart of [AedstScrollArea]; see its KDoc for why this wrapper exists. */
@Composable
fun AedstHorizontalScrollArea(modifier: Modifier = Modifier, content: @Composable RowScope.() -> Unit) {
    Row(
        modifier = modifier.horizontalScroll(rememberScrollState()),
        content = content,
    )
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstScrollAreaPreview() {
    AedstTheme {
        Surface {
            Column {
                AedstScrollArea(modifier = Modifier.padding(AedstSpacing.md)) {
                    repeat(8) { index ->
                        Text(
                            text = "Row $index",
                            style = MaterialTheme.typography.bodyMedium,
                            modifier = Modifier.padding(vertical = AedstSpacing.xs),
                        )
                    }
                }
                AedstHorizontalScrollArea(modifier = Modifier.padding(AedstSpacing.md)) {
                    repeat(8) { index ->
                        Text(
                            text = "Col $index",
                            modifier = Modifier
                                .width(64.dp)
                                .padding(horizontal = AedstSpacing.xs),
                        )
                    }
                }
            }
        }
    }
}
