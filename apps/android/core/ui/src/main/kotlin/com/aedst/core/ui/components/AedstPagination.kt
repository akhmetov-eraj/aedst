package com.aedst.core.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Page navigation control: previous/next arrows plus up to 5 visible page-number chips,
 * collapsing extra pages with an ellipsis. Mirrors web's `pagination.tsx`.
 */
@Composable
fun AedstPagination(
    currentPage: Int,
    totalPages: Int,
    onPageChange: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(AedstSpacing.xs),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        IconButton(
            onClick = { if (currentPage > 1) onPageChange(currentPage - 1) },
            enabled = currentPage > 1,
        ) {
            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Previous page")
        }

        visiblePages(currentPage, totalPages).forEach { entry ->
            when (entry) {
                is PaginationEntry.Page -> PageChip(
                    page = entry.page,
                    isSelected = entry.page == currentPage,
                    onClick = { onPageChange(entry.page) },
                )
                PaginationEntry.Ellipsis -> Text(
                    text = "…",
                    modifier = Modifier.size(32.dp),
                    textAlign = androidx.compose.ui.text.style.TextAlign.Center,
                )
            }
        }

        IconButton(
            onClick = { if (currentPage < totalPages) onPageChange(currentPage + 1) },
            enabled = currentPage < totalPages,
        ) {
            Icon(Icons.AutoMirrored.Filled.ArrowForward, contentDescription = "Next page")
        }
    }
}

@Composable
private fun PageChip(
    page: Int,
    isSelected: Boolean,
    onClick: () -> Unit,
) {
    val background = if (isSelected) MaterialTheme.colorScheme.primary else androidx.compose.ui.graphics.Color.Transparent
    val contentColor = if (isSelected) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface

    Box(
        modifier = Modifier
            .size(32.dp)
            .clip(CircleShape)
            .background(background, CircleShape)
            .clickable(onClick = onClick)
            .semantics {
                role = Role.Button
                contentDescription = "Page $page"
            },
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = page.toString(),
            color = contentColor,
            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
        )
    }
}

private sealed interface PaginationEntry {
    data class Page(val page: Int) : PaginationEntry
    data object Ellipsis : PaginationEntry
}

private fun visiblePages(currentPage: Int, totalPages: Int): List<PaginationEntry> {
    if (totalPages <= 5) {
        return (1..totalPages).map { PaginationEntry.Page(it) }
    }

    val pages = linkedSetOf<Int>()
    pages += 1
    pages += totalPages
    pages += (currentPage - 1).coerceAtLeast(1)
    pages += currentPage.coerceIn(1, totalPages)
    pages += (currentPage + 1).coerceAtMost(totalPages)

    val sorted = pages.sorted().filter { it in 1..totalPages }
    val result = mutableListOf<PaginationEntry>()
    var previous: Int? = null
    for (page in sorted) {
        if (previous != null && page - previous > 1) {
            result += PaginationEntry.Ellipsis
        }
        result += PaginationEntry.Page(page)
        previous = page
    }
    return result.take(6).let { limited ->
        // Keep to at most 5 numeric chips plus a single ellipsis, matching the spec.
        val numericCount = limited.count { it is PaginationEntry.Page }
        if (numericCount <= 5) limited else limited.filterIndexed { index, _ -> index < 6 }
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstPaginationPreview() {
    AedstTheme {
        Surface {
            androidx.compose.foundation.layout.Column(
                verticalArrangement = Arrangement.spacedBy(AedstSpacing.md),
            ) {
                AedstPagination(currentPage = 1, totalPages = 4, onPageChange = {})
                AedstPagination(currentPage = 6, totalPages = 20, onPageChange = {})
            }
        }
    }
}
