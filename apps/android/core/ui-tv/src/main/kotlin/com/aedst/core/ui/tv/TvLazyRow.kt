package com.aedst.core.ui.tv

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.tv.foundation.lazy.list.TvLazyListScope
import androidx.tv.foundation.lazy.list.TvLazyListState
import androidx.tv.foundation.lazy.list.rememberTvLazyListState
import androidx.tv.foundation.lazy.list.TvLazyRow as FoundationTvLazyRow
import com.aedst.core.ui.theme.AedstSpacing

/**
 * Focus-aware horizontal list for 10-foot UI (D-pad left/right navigation,
 * automatic scroll-into-view on focus). Thin wrapper around
 * `androidx.tv.foundation.lazy.list.TvLazyRow` — the official Compose-for-TV
 * primitive, not a hand-rolled focus/scroll implementation. Regular
 * `LazyRow` (used by `core.ui`) has no D-pad focus semantics, which is the
 * genuine divergence justifying this wrapper (see this module's README).
 */
@Composable
fun TvLazyRow(
    modifier: Modifier = Modifier,
    state: TvLazyListState = rememberTvLazyListState(),
    horizontalArrangement: Arrangement.Horizontal = Arrangement.spacedBy(AedstSpacing.md),
    content: TvLazyListScope.() -> Unit,
) {
    FoundationTvLazyRow(
        modifier = modifier,
        state = state,
        horizontalArrangement = horizontalArrangement,
        content = content,
    )
}
