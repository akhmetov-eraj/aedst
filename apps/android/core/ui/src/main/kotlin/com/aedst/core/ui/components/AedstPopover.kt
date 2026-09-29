package com.aedst.core.ui.components

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstElevation
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Free-form content popover anchored to a trigger, mirroring web's `popover.tsx`. Material3 has
 * no standalone `Popover` composable, so this is implemented pragmatically on top of
 * [DropdownMenu] (which accepts arbitrary content, not just menu items) rather than a raw
 * [androidx.compose.ui.window.Popup], for consistent positioning/dismiss/elevation behavior.
 */
@Composable
fun AedstPopover(
    expanded: Boolean,
    onDismissRequest: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    DropdownMenu(
        expanded = expanded,
        onDismissRequest = onDismissRequest,
        modifier = modifier,
    ) {
        Column(
            modifier = Modifier.padding(AedstSpacing.md),
            content = content,
        )
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", showBackground = true, uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstPopoverPreview() {
    AedstTheme {
        Surface(color = MaterialTheme.colorScheme.background) {
            Box(modifier = Modifier.padding(AedstSpacing.xxxl)) {
                // DropdownMenu requires an anchor composition; preview its content directly since
                // `expanded` popups cannot be meaningfully captured in a static @Preview.
                Surface(
                    shape = AedstShapesScale.medium,
                    color = MaterialTheme.colorScheme.surface,
                    tonalElevation = AedstElevation.level2,
                ) {
                    Column(modifier = Modifier.padding(AedstSpacing.md)) {
                        Text("Notifications", style = MaterialTheme.typography.titleSmall)
                        Text(
                            "You're all caught up.",
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    }
                }
            }
        }
    }
}
