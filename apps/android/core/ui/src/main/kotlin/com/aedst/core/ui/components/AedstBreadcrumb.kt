package com.aedst.core.ui.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/** A single crumb in [AedstBreadcrumb]. A `null` [onClick] renders the label as plain text. */
data class AedstBreadcrumbItem(val label: String, val onClick: (() -> Unit)? = null)

/**
 * Horizontal trail of navigation crumbs separated by `/`. The last item is always rendered as
 * the current, non-clickable page, mirroring web's `breadcrumb.tsx`.
 */
@Composable
fun AedstBreadcrumb(items: List<AedstBreadcrumbItem>, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(AedstSpacing.xs),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        items.forEachIndexed { index, item ->
            val isCurrent = index == items.lastIndex
            if (isCurrent) {
                Text(
                    text = item.label,
                    color = MaterialTheme.colorScheme.onSurface,
                    fontWeight = FontWeight.Medium,
                    style = MaterialTheme.typography.bodyMedium,
                )
            } else {
                val clickModifier = if (item.onClick != null) {
                    Modifier
                        .clickable(onClick = item.onClick)
                        .semantics { role = Role.Button }
                } else {
                    Modifier
                }
                Text(
                    text = item.label,
                    color = MaterialTheme.colorScheme.primary,
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = clickModifier,
                )
            }
            if (!isCurrent) {
                Text(
                    text = "/",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstBreadcrumbPreview() {
    AedstTheme {
        Surface {
            AedstBreadcrumb(
                items = listOf(
                    AedstBreadcrumbItem("Home", onClick = {}),
                    AedstBreadcrumbItem("Projects", onClick = {}),
                    AedstBreadcrumbItem("AEDST Mobile"),
                ),
            )
        }
    }
}
