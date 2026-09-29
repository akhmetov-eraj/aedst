package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.toggleable
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.disabled
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Single stateful toggle. Implemented directly from [Surface] + `Modifier.toggleable` rather
 * than Material3's experimental `ToggleButton` API, so the component sits on a stable surface
 * regardless of whether that API has stabilized in the Material3 version this module depends on.
 */
@Composable
fun AedstToggleButton(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    content: @Composable RowScope.() -> Unit,
) {
    val containerColor = if (checked) {
        MaterialTheme.colorScheme.secondary
    } else {
        MaterialTheme.colorScheme.surface
    }
    val contentColor = if (checked) {
        MaterialTheme.colorScheme.onSecondary
    } else {
        MaterialTheme.colorScheme.onSurface
    }
    val border = if (checked) {
        null
    } else {
        BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant)
    }

    Surface(
        modifier = modifier
            .toggleable(
                value = checked,
                enabled = enabled,
                role = Role.Checkbox,
                onValueChange = onCheckedChange,
            )
            .semantics { if (!enabled) disabled() },
        shape = AedstShapesScale.medium,
        color = containerColor,
        contentColor = if (enabled) contentColor else contentColor.copy(alpha = 0.38f),
        border = border,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = AedstSpacing.md, vertical = AedstSpacing.sm),
            horizontalArrangement = Arrangement.spacedBy(AedstSpacing.xs),
            verticalAlignment = Alignment.CenterVertically,
            content = content,
        )
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstToggleButtonPreview() {
    AedstTheme {
        Surface {
            Row(
                horizontalArrangement = Arrangement.spacedBy(AedstSpacing.sm),
                modifier = Modifier.padding(AedstSpacing.lg),
            ) {
                AedstToggleButton(checked = true, onCheckedChange = {}) {
                    Text("Bold")
                }
                AedstToggleButton(checked = false, onCheckedChange = {}) {
                    Text("Italic")
                }
            }
        }
    }
}
