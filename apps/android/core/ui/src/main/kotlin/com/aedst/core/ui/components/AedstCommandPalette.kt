package com.aedst.core.ui.components

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/** A single searchable entry in [AedstCommandPalette]. */
data class AedstCommandItem(
    val id: String,
    val label: String,
    val icon: (@Composable () -> Unit)? = null,
    val group: String? = null,
)

private const val UNGROUPED_LABEL = "Other"

/**
 * Searchable command list, mirroring web's `command.tsx`. Self-contained (does not depend on
 * [AedstTextField]) so it can build independently: the search field is an inline
 * [OutlinedTextField] rather than a shared form primitive.
 */
@Composable
fun AedstCommandPalette(
    items: List<AedstCommandItem>,
    onItemSelected: (AedstCommandItem) -> Unit,
    modifier: Modifier = Modifier,
    placeholder: String = "Search…",
) {
    var query by remember { mutableStateOf(TextFieldValue("")) }

    val filteredItems = remember(items, query.text) {
        val needle = query.text.trim()
        if (needle.isEmpty()) {
            items
        } else {
            items.filter { it.label.contains(needle, ignoreCase = true) }
        }
    }
    val groupedItems = remember(filteredItems) {
        filteredItems.groupBy { it.group ?: UNGROUPED_LABEL }
    }

    Column(modifier = modifier.fillMaxWidth()) {
        OutlinedTextField(
            value = query,
            onValueChange = { query = it },
            modifier = Modifier
                .fillMaxWidth()
                .padding(AedstSpacing.md),
            placeholder = { Text(placeholder) },
            leadingIcon = { Icon(Icons.Default.Search, contentDescription = null) },
            singleLine = true,
        )
        LazyColumn(modifier = Modifier.fillMaxWidth()) {
            groupedItems.forEach { (group, groupItems) ->
                item(key = "header_$group") {
                    Text(
                        text = group,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(
                            horizontal = AedstSpacing.md,
                            vertical = AedstSpacing.xs,
                        ),
                    )
                }
                items(groupItems, key = { it.id }) { commandItem ->
                    CommandPaletteRow(item = commandItem, onClick = { onItemSelected(commandItem) })
                }
            }
        }
    }
}

@Composable
private fun CommandPaletteRow(item: AedstCommandItem, onClick: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surface,
        contentColor = MaterialTheme.colorScheme.onSurface,
        onClick = onClick,
    ) {
        androidx.compose.foundation.layout.Row(
            modifier = Modifier.padding(horizontal = AedstSpacing.md, vertical = AedstSpacing.sm),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(AedstSpacing.sm),
            verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
        ) {
            item.icon?.invoke()
            Text(item.label, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Preview(name = "Light", showBackground = true, heightDp = 400)
@Preview(
    name = "Dark",
    showBackground = true,
    heightDp = 400,
    uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES,
)
@Composable
private fun AedstCommandPalettePreview() {
    AedstTheme {
        Surface(color = MaterialTheme.colorScheme.background) {
            AedstCommandPalette(
                items = listOf(
                    AedstCommandItem(id = "new_task", label = "New task", group = "Actions"),
                    AedstCommandItem(id = "new_project", label = "New project", group = "Actions"),
                    AedstCommandItem(id = "go_inbox", label = "Go to Inbox", group = "Navigate"),
                    AedstCommandItem(id = "go_today", label = "Go to Today", group = "Navigate"),
                ),
                onItemSelected = {},
            )
        }
    }
}
