package com.aedst.core.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Horizontally-paged carousel with a dot page indicator below the current page's content.
 * Mirrors web's `carousel.tsx`.
 */
@Composable
fun <T> AedstCarousel(items: List<T>, modifier: Modifier = Modifier, itemContent: @Composable (T) -> Unit) {
    val pagerState = rememberPagerState { items.size }

    Column(modifier = modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        HorizontalPager(state = pagerState) { page ->
            itemContent(items[page])
        }
        if (items.size > 1) {
            PagerIndicator(
                pageCount = items.size,
                currentPage = pagerState.currentPage,
                modifier = Modifier.padding(top = AedstSpacing.sm),
            )
        }
    }
}

@Composable
private fun PagerIndicator(pageCount: Int, currentPage: Int, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(AedstSpacing.xs),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        repeat(pageCount) { index ->
            val color = if (index == currentPage) {
                MaterialTheme.colorScheme.primary
            } else {
                MaterialTheme.colorScheme.outlineVariant
            }
            androidx.compose.foundation.layout.Box(
                modifier = Modifier
                    .size(6.dp)
                    .background(color, CircleShape),
            )
        }
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstCarouselPreview() {
    val slides = listOf("First", "Second", "Third")
    AedstTheme {
        Surface {
            AedstCarousel(items = slides) { slide ->
                androidx.compose.foundation.layout.Box(
                    modifier = Modifier
                        .padding(AedstSpacing.lg)
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                        .padding(AedstSpacing.xxl),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(slide, color = MaterialTheme.colorScheme.onSurface)
                }
            }
        }
    }
}
