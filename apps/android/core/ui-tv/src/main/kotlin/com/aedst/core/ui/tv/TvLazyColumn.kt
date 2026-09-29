package com.aedst.core.ui.tv

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.tv.foundation.lazy.list.TvLazyListScope
import androidx.tv.foundation.lazy.list.TvLazyListState
import androidx.tv.foundation.lazy.list.rememberTvLazyListState
import androidx.tv.foundation.lazy.list.TvLazyColumn as FoundationTvLazyColumn
import com.aedst.core.ui.theme.AedstSpacing

/**
 * Focus-aware vertical list for 10-foot UI (D-pad up/down navigation,
 * automatic scroll-into-view on focus). Thin wrapper around
 * `androidx.tv.foundation.lazy.list.TvLazyColumn` — see [TvLazyRow] for the
 * same rationale on the horizontal axis.
 */
@Composable
fun TvLazyColumn(
    modifier: Modifier = Modifier,
    state: TvLazyListState = rememberTvLazyListState(),
    verticalArrangement: Arrangement.Vertical = Arrangement.spacedBy(AedstSpacing.md),
    content: TvLazyListScope.() -> Unit,
) {
    FoundationTvLazyColumn(
        modifier = modifier,
        state = state,
        verticalArrangement = verticalArrangement,
        content = content,
    )
}
