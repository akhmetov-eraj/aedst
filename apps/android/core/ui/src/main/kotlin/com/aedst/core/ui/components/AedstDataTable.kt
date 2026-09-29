package com.aedst.core.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/** Definition of a single [AedstDataTable] column: header label, relative width, and cell renderer. */
data class AedstTableColumn<T>(
    val header: String,
    val weight: Float = 1f,
    val cell: @Composable (T) -> Unit,
)

/**
 * Simple tabular data list: a bold header row followed by a lazily-composed, zebra-striped row
 * list. Mirrors web's `table.tsx`.
 */
@Composable
fun <T> AedstDataTable(
    items: List<T>,
    columns: List<AedstTableColumn<T>>,
    modifier: Modifier = Modifier,
    keySelector: (T) -> Any = { it.hashCode() },
) {
    androidx.compose.foundation.layout.Column(modifier = modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = AedstSpacing.md, vertical = AedstSpacing.sm),
            horizontalArrangement = Arrangement.spacedBy(AedstSpacing.sm),
        ) {
            columns.forEach { column ->
                Text(
                    text = column.header,
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.weight(column.weight),
                )
            }
        }
        HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)

        LazyColumn {
            itemsIndexed(items = items, key = { _, item -> keySelector(item) }) { index, item ->
                val rowBackground = if (index % 2 == 1) {
                    MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.3f)
                } else {
                    androidx.compose.ui.graphics.Color.Transparent
                }
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(rowBackground)
                        .padding(horizontal = AedstSpacing.md, vertical = AedstSpacing.sm),
                    horizontalArrangement = Arrangement.spacedBy(AedstSpacing.sm),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    columns.forEach { column ->
                        androidx.compose.foundation.layout.Box(modifier = Modifier.weight(column.weight)) {
                            column.cell(item)
                        }
                    }
                }
                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
            }
        }
    }
}

private data class PreviewRow(val name: String, val role: String, val active: Boolean)

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstDataTablePreview() {
    val rows = listOf(
        PreviewRow("Ava Kim", "Admin", true),
        PreviewRow("Liam Chen", "Editor", false),
        PreviewRow("Noor Patel", "Viewer", true),
        PreviewRow("Sam Torres", "Editor", true),
    )
    AedstTheme {
        Surface {
            AedstDataTable(
                items = rows,
                keySelector = { it.name },
                columns = listOf(
                    AedstTableColumn(header = "Name", weight = 1.5f) { row -> Text(row.name) },
                    AedstTableColumn(header = "Role") { row -> Text(row.role) },
                    AedstTableColumn(header = "Active") { row -> Text(if (row.active) "Yes" else "No") },
                ),
            )
        }
    }
}
