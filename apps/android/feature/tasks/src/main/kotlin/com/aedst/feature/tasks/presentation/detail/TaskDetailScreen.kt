package com.aedst.feature.tasks.presentation.detail

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.media.MediaItem
import com.aedst.core.media.ui.MediaThumbnail
import com.aedst.core.media.ui.UploadProgressRow
import com.aedst.core.media.ui.rememberMediaPicker
import com.aedst.core.media.upload.UploadProgress
import com.aedst.core.ui.components.AedstButton
import com.aedst.core.ui.components.AedstButtonVariant
import com.aedst.core.ui.components.AedstErrorSnackbarEffect
import com.aedst.core.ui.components.AedstSnackbarHost
import com.aedst.core.ui.components.AedstTopAppBar
import com.aedst.core.ui.components.FullScreenLoading
import com.aedst.feature.tasks.R
import com.aedst.feature.tasks.domain.Task
import com.aedst.feature.tasks.presentation.TaskStatusBadge
import com.aedst.feature.tasks.presentation.formatDate
import com.aedst.feature.tasks.presentation.priorityLabel

@Composable
fun TaskDetailRoute(
    onBack: () -> Unit,
    onOpenMedia: (String) -> Unit,
    viewModel: TaskDetailViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val uploads by viewModel.uploadsInFlight.collectAsStateWithLifecycle()
    val currentOnBack by rememberUpdatedState(onBack)
    LaunchedEffect(viewModel) {
        viewModel.effects.collect { effect ->
            when (effect) {
                TaskDetailEffect.Deleted -> currentOnBack()
            }
        }
    }
    TaskDetailScreen(
        state = state,
        uploads = uploads,
        onIntent = viewModel::onIntent,
        onBack = onBack,
        onOpenMedia = onOpenMedia,
    )
}

@Composable
internal fun TaskDetailScreen(
    state: TaskDetailState,
    uploads: List<UploadProgress>,
    onIntent: (TaskDetailIntent) -> Unit,
    onBack: () -> Unit,
    onOpenMedia: (String) -> Unit,
) {
    val snackbarHostState = remember { SnackbarHostState() }
    AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = { onIntent(TaskDetailIntent.ErrorShown) })
    val pickMedia = rememberMediaPicker { uris -> onIntent(TaskDetailIntent.Attach(uris)) }

    Scaffold(
        topBar = {
            AedstTopAppBar(
                title = state.task?.title.orEmpty(),
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(
                            Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = stringResource(R.string.tasks_back),
                        )
                    }
                },
                actions = {
                    if (state.task != null) {
                        IconButton(onClick = { onIntent(TaskDetailIntent.Delete) }, enabled = !state.isMutating) {
                            Icon(Icons.Filled.Delete, contentDescription = stringResource(R.string.tasks_delete))
                        }
                    }
                },
            )
        },
        snackbarHost = { AedstSnackbarHost(snackbarHostState) },
    ) { padding ->
        val task = state.task
        if (task == null) {
            if (state.isLoading) FullScreenLoading(Modifier.padding(padding))
            return@Scaffold
        }
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            TaskSummary(task)
            AedstButton(
                text = stringResource(if (task.isDone) R.string.tasks_reopen else R.string.tasks_complete),
                onClick = { onIntent(TaskDetailIntent.ToggleDone) },
                loading = state.isMutating,
                variant = if (task.isDone) AedstButtonVariant.OUTLINE else AedstButtonVariant.DEFAULT,
                modifier = Modifier.fillMaxWidth(),
            )
            Attachments(
                attachments = state.attachments,
                uploads = uploads,
                onAttach = pickMedia,
                onOpenMedia = onOpenMedia,
                onCancelUpload = { onIntent(TaskDetailIntent.CancelUpload(it)) },
            )
        }
    }
}

@Composable
private fun TaskSummary(task: Task) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            TaskStatusBadge(task.status)
            Text(priorityLabel(task.priority), style = MaterialTheme.typography.labelLarge)
        }
        task.dueDate?.let {
            Text(stringResource(R.string.tasks_due, it.formatDate()), style = MaterialTheme.typography.bodyMedium)
        }
        if (task.description.isNotBlank()) {
            Text(task.description, style = MaterialTheme.typography.bodyLarge)
        }
    }
}

@Composable
private fun Attachments(
    attachments: List<MediaItem>,
    uploads: List<UploadProgress>,
    onAttach: () -> Unit,
    onOpenMedia: (String) -> Unit,
    onCancelUpload: (String) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = stringResource(R.string.tasks_attachments),
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.weight(1f),
            )
            AedstButton(
                text = stringResource(R.string.tasks_attach),
                onClick = onAttach,
                variant = AedstButtonVariant.GHOST,
            )
        }
        if (attachments.isEmpty() && uploads.isEmpty()) {
            Text(stringResource(R.string.tasks_no_attachments), style = MaterialTheme.typography.bodyMedium)
        }
        if (attachments.isNotEmpty()) {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                items(attachments, key = MediaItem::id) { item ->
                    MediaThumbnail(
                        item = item,
                        contentDescription = item.filename,
                        modifier = Modifier
                            .size(96.dp)
                            .clip(RoundedCornerShape(8.dp))
                            .clickable { onOpenMedia(item.id) },
                    )
                }
            }
        }
        uploads.filter { it.state != UploadProgress.State.DONE }.forEach { upload ->
            UploadProgressRow(
                upload = upload,
                onCancel = { onCancelUpload(upload.uploadId) },
            )
        }
    }
}
