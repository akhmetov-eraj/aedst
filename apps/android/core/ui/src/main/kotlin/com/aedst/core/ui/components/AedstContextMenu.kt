package com.aedst.core.ui.components

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Long-press menu, mirroring web's `context-menu.tsx` (which is triggered by right-click on
 * desktop; long-press is the idiomatic touch equivalent). Wraps [content] in a long-press
 * gesture detector and shows [AedstDropdownMenu] anchored to it once triggered.
 */
@Composable
fun <T> AedstContextMenu(
    options: List<AedstDropdownMenuOption<T>>,
    onOptionSelected: (T) -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    Box(
        modifier = modifier.pointerInput(Unit) {
            detectTapGestures(onLongPress = { expanded = true })
        },
    ) {
        content()
        AedstDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
            options = options,
            onOptionSelected = onOptionSelected,
        )
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", showBackground = true, uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstContextMenuPreview() {
    AedstTheme {
        Surface(color = MaterialTheme.colorScheme.background) {
            AedstContextMenu(
                options = listOf(
                    AedstDropdownMenuOption(value = "rename", label = "Rename"),
                    AedstDropdownMenuOption(value = "delete", label = "Delete", isDestructive = true),
                ),
                onOptionSelected = {},
            ) {
                Surface(
                    shape = AedstShapesScale.medium,
                    color = MaterialTheme.colorScheme.surfaceVariant,
                ) {
                    Text(
                        "Long-press me",
                        modifier = Modifier.padding(AedstSpacing.lg),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}
