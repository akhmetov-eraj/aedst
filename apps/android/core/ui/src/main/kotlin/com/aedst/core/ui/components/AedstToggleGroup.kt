package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.RoundedCornerShape
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
import com.aedst.core.ui.theme.AedstRadius
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Segmented control: a row of connected toggle segments built from the same visual language as
 * [AedstToggleButton]. When [multiSelect] is `false` the group behaves like a single-select
 * segmented control — choosing one option clears the rest.
 */
@Composable
fun <T> AedstToggleGroup(
    options: List<T>,
    selected: Set<T>,
    onSelectedChange: (Set<T>) -> Unit,
    modifier: Modifier = Modifier,
    optionLabel: (T) -> String,
    multiSelect: Boolean = false,
    enabled: Boolean = true,
) {
    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(1.dp)) {
        options.forEachIndexed { index, option ->
            val isChecked = option in selected
            val shape = segmentShape(index = index, count = options.size)

            val onToggle: (Boolean) -> Unit = { checked ->
                val nextSelection = if (multiSelect) {
                    if (checked) selected + option else selected - option
                } else {
                    if (checked) setOf(option) else emptySet()
                }
                onSelectedChange(nextSelection)
            }

            val containerColor = if (isChecked) {
                MaterialTheme.colorScheme.secondary
            } else {
                MaterialTheme.colorScheme.surface
            }
            val contentColor = if (isChecked) {
                MaterialTheme.colorScheme.onSecondary
            } else {
                MaterialTheme.colorScheme.onSurface
            }

            Surface(
                modifier = Modifier
                    .toggleable(
                        value = isChecked,
                        enabled = enabled,
                        role = Role.Checkbox,
                        onValueChange = onToggle,
                    )
                    .semantics { if (!enabled) disabled() },
                shape = shape,
                color = containerColor,
                contentColor = if (enabled) contentColor else contentColor.copy(alpha = 0.38f),
                border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
            ) {
                Row(
                    modifier = Modifier.padding(
                        horizontal = AedstSpacing.md,
                        vertical = AedstSpacing.sm,
                    ),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(optionLabel(option))
                }
            }
        }
    }
}

private fun segmentShape(index: Int, count: Int): RoundedCornerShape {
    val start = if (index == 0) AedstRadius.md else 0.dp
    val end = if (index == count - 1) AedstRadius.md else 0.dp
    return RoundedCornerShape(topStart = start, bottomStart = start, topEnd = end, bottomEnd = end)
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstToggleGroupPreview() {
    AedstTheme {
        Surface {
            Row(modifier = Modifier.padding(AedstSpacing.lg)) {
                AedstToggleGroup(
                    options = listOf("Day", "Week", "Month"),
                    selected = setOf("Week"),
                    onSelectedChange = {},
                    optionLabel = { it },
                )
            }
        }
    }
}
