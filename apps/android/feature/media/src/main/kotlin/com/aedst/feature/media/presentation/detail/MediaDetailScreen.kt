package com.aedst.feature.media.presentation.detail

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.media.MediaItem
import com.aedst.core.media.MediaKind
import com.aedst.core.media.MediaStatus
import com.aedst.core.media.ui.MediaImage
import com.aedst.core.media.ui.VideoPlayer
import com.aedst.core.ui.components.AedstErrorSnackbarEffect
import com.aedst.core.ui.components.AedstSnackbarHost
import com.aedst.core.ui.components.AedstTopAppBar
import com.aedst.core.ui.components.FullScreenLoading
import com.aedst.feature.media.R

@Composable
fun MediaDetailRoute(onBack: () -> Unit, viewModel: MediaDetailViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val currentOnBack by rememberUpdatedState(onBack)
    LaunchedEffect(viewModel) { viewModel.deleted.collect { currentOnBack() } }
    MediaDetailScreen(
        state = state,
        onDelete = viewModel::delete,
        onErrorShown = viewModel::errorShown,
        onBack = onBack,
    )
}

@Composable
internal fun MediaDetailScreen(
    state: MediaDetailState,
    onDelete: () -> Unit,
    onErrorShown: () -> Unit,
    onBack: () -> Unit,
) {
    val snackbarHostState = remember { SnackbarHostState() }
    AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = onErrorShown)

    Scaffold(
        topBar = {
            AedstTopAppBar(
                title = state.item?.filename.orEmpty(),
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(
                            Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = stringResource(R.string.media_back),
                        )
                    }
                },
                actions = {
                    if (state.item != null) {
                        IconButton(onClick = onDelete, enabled = !state.isDeleting) {
                            Icon(Icons.Filled.Delete, contentDescription = stringResource(R.string.media_delete))
                        }
                    }
                },
            )
        },
        snackbarHost = { AedstSnackbarHost(snackbarHostState) },
    ) { padding ->
        val item = state.item
        when {
            item == null -> if (state.isLoading) FullScreenLoading(Modifier.padding(padding))
            else -> Column(
                modifier = Modifier.fillMaxSize().padding(padding),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                MediaContent(item)
                Text(
                    text = statusText(item),
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(horizontal = 16.dp),
                )
            }
        }
    }
}

@Composable
private fun MediaContent(item: MediaItem) {
    val url = item.displayUrl
    val ratio = if (item.width > 0 && item.height > 0) item.width.toFloat() / item.height else DEFAULT_RATIO
    val frame = Modifier.fillMaxWidth().aspectRatio(ratio)
    when {
        url == null -> Box(frame, contentAlignment = Alignment.Center) {
            Text(stringResource(R.string.media_processing))
        }
        item.kind == MediaKind.VIDEO -> VideoPlayer(url = url, modifier = frame)
        item.kind == MediaKind.IMAGE -> MediaImage(
            url = url,
            cacheKey = "${item.id}:display",
            contentDescription = item.filename,
            modifier = frame,
            contentScale = ContentScale.Fit,
        )
        else -> Box(frame, contentAlignment = Alignment.Center) { Text(item.contentType) }
    }
}

@Composable
private fun statusText(item: MediaItem): String = when (item.status) {
    MediaStatus.READY -> stringResource(R.string.media_status_ready)
    MediaStatus.FAILED -> stringResource(R.string.media_status_failed)
    else -> stringResource(R.string.media_processing)
}

private const val DEFAULT_RATIO = 16f / 9f
