package com.aedst.core.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.R
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/** A single expandable entry rendered by [AedstAccordion]. */
data class AedstAccordionItem(val id: String, val title: String, val content: @Composable () -> Unit)

/**
 * Vertical list of expandable items, distinct from the single-section [AedstExpandableSection].
 * When [allowMultipleOpen] is `false` (default), expanding one item collapses the others.
 */
@Composable
fun AedstAccordion(
    items: List<AedstAccordionItem>,
    modifier: Modifier = Modifier,
    allowMultipleOpen: Boolean = false,
) {
    var expandedIds by remember { mutableStateOf(setOf<String>()) }

    Column(modifier = modifier.fillMaxWidth()) {
        items.forEachIndexed { index, item ->
            val isExpanded = item.id in expandedIds
            AccordionRow(
                item = item,
                isExpanded = isExpanded,
                onToggle = {
                    expandedIds = if (isExpanded) {
                        expandedIds - item.id
                    } else if (allowMultipleOpen) {
                        expandedIds + item.id
                    } else {
                        setOf(item.id)
                    }
                },
            )
            if (index != items.lastIndex) {
                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
            }
        }
    }
}

@Composable
private fun AccordionRow(item: AedstAccordionItem, isExpanded: Boolean, onToggle: () -> Unit) {
    val rotation by animateFloatAsState(targetValue = if (isExpanded) 180f else 0f, label = "chevronRotation")

    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable(onClick = onToggle)
                .semantics { role = Role.Button }
                .padding(vertical = AedstSpacing.md),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = item.title,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.Medium,
                modifier = Modifier.weight(1f),
            )
            Icon(
                imageVector = Icons.Default.KeyboardArrowDown,
                contentDescription = stringResource(
                    if (isExpanded) R.string.aedst_a11y_collapse else R.string.aedst_a11y_expand,
                ),
                modifier = Modifier.rotate(rotation),
            )
        }
        AnimatedVisibility(
            visible = isExpanded,
            enter = expandVertically() + fadeIn(),
            exit = shrinkVertically() + fadeOut(),
        ) {
            Column(modifier = Modifier.padding(bottom = AedstSpacing.md)) {
                item.content()
            }
        }
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstAccordionPreview() {
    val items = listOf(
        AedstAccordionItem("1", "What is AEDST?") {
            Text("A cross-platform productivity suite.", style = MaterialTheme.typography.bodyMedium)
        },
        AedstAccordionItem("2", "Is there a free tier?") {
            Text("Yes, the core features are free.", style = MaterialTheme.typography.bodyMedium)
        },
        AedstAccordionItem("3", "Which platforms are supported?") {
            Text("Web, Android, iOS, and Desktop.", style = MaterialTheme.typography.bodyMedium)
        },
    )
    AedstTheme {
        Surface {
            AedstAccordion(items = items)
        }
    }
}
