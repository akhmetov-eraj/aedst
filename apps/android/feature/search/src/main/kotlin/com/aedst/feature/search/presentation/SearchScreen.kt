package com.aedst.feature.search.presentation

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.ui.components.AedstTextField
import com.aedst.core.ui.components.FullScreenLoading
import com.aedst.core.ui.components.userMessage
import com.aedst.feature.search.R
import com.aedst.feature.search.domain.SearchHit

@Composable
fun SearchRoute(onOpenLink: (String) -> Unit, viewModel: SearchViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    SearchScreen(state = state, onQueryChange = viewModel::onQueryChange, onOpenLink = onOpenLink)
}

@Composable
internal fun SearchScreen(state: SearchState, onQueryChange: (String) -> Unit, onOpenLink: (String) -> Unit) {
    val focusRequester = remember { FocusRequester() }
    Column(modifier = Modifier.fillMaxSize().padding(top = 16.dp)) {
        AedstTextField(
            value = state.query,
            onValueChange = onQueryChange,
            placeholder = stringResource(R.string.search_placeholder),
            leadingIcon = { Icon(Icons.Filled.Search, contentDescription = null) },
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp)
                .focusRequester(focusRequester),
        )
        Box(modifier = Modifier.fillMaxSize().padding(top = 8.dp)) {
            when (val result = state.result) {
                SearchResultState.Idle -> Centered(stringResource(R.string.search_hint))
                SearchResultState.Loading -> FullScreenLoading()
                is SearchResultState.Failed -> Centered(result.error.userMessage())
                is SearchResultState.Results ->
                    if (result.hits.isEmpty()) {
                        Centered(stringResource(R.string.search_no_results))
                    } else {
                        HitList(result.hits, onOpenLink)
                    }
            }
        }
    }
}

@Composable
private fun HitList(hits: List<SearchHit>, onOpenLink: (String) -> Unit) {
    val highlight = SpanStyle(
        fontWeight = FontWeight.Bold,
        background = MaterialTheme.colorScheme.primaryContainer,
        color = MaterialTheme.colorScheme.onPrimaryContainer,
    )
    LazyColumn(modifier = Modifier.fillMaxSize()) {
        items(hits, key = { "${it.kind}:${it.id}" }) { hit ->
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onOpenLink(hit.link) }
                    .padding(horizontal = 16.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                Text(
                    hit.title,
                    style = MaterialTheme.typography.titleMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                val snippet = remember(hit.snippet, highlight) { highlightSnippet(hit.snippet, highlight) }
                Text(
                    snippet,
                    style = MaterialTheme.typography.bodyMedium,
                    maxLines = 3,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            HorizontalDivider()
        }
    }
}

@Composable
private fun Centered(text: String) {
    Box(modifier = Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
        Text(text, style = MaterialTheme.typography.bodyLarge)
    }
}
