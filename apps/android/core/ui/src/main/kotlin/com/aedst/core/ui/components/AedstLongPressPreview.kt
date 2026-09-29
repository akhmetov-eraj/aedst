package com.aedst.core.ui.components

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.ColumnScope
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
import com.aedst.core.ui.theme.AedstElevation
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Adaptation of web's `hover-card.tsx` for touch. HoverCard has no mobile input analog because
 * touch devices have no hover/pointer-over signal; long-press is the closest idiomatic Android
 * equivalent (the same pattern used for long-press-to-preview elsewhere in the platform, e.g.
 * notification/app-shortcut previews). [content] is wrapped with a long-press gesture that reveals
 * a small themed popup card rendering [previewContent], reusing [AedstPopover]'s
 * `DropdownMenu`-as-popover technique.
 */
@Composable
fun AedstLongPressPreview(
    modifier: Modifier = Modifier,
    previewContent: @Composable ColumnScope.() -> Unit,
    content: @Composable () -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    Box(
        modifier = modifier.pointerInput(Unit) {
            detectTapGestures(onLongPress = { expanded = true })
        },
    ) {
        content()
        AedstPopover(
            expanded = expanded,
            onDismissRequest = { expanded = false },
            content = previewContent,
        )
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", showBackground = true, uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstLongPressPreviewPreview() {
    AedstTheme {
        Surface(color = MaterialTheme.colorScheme.background) {
            Box(modifier = Modifier.padding(AedstSpacing.xxxl)) {
                // AedstPopover's `expanded` popup cannot be meaningfully captured in a static
                // @Preview, so the resting/expanded preview-card visual is rendered directly.
                Surface(
                    shape = AedstShapesScale.medium,
                    color = MaterialTheme.colorScheme.surface,
                    tonalElevation = AedstElevation.level2,
                ) {
                    androidx.compose.foundation.layout.Column(modifier = Modifier.padding(AedstSpacing.md)) {
                        Text("Eraj Akhmetov", style = MaterialTheme.typography.titleSmall)
                        Text(
                            "Joined September 2026",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        }
    }
}
