package com.aedst.feature.tasks.presentation.list

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
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
fun TaskListRoute(
    onOpenTask: (String) -> Unit,
    onCreateTask: () -> Unit,
    viewModel: TaskListViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    // Reload whenever the list becomes visible again (after creating, completing or deleting a task).
    LifecycleStartEffect(viewModel) {
        viewModel.onIntent(TaskListIntent.Refresh)
        onStopOrDispose { }
    }
    TaskListScreen(state = state, onIntent = viewModel::onIntent, onOpenTask = onOpenTask, onCreateTask = onCreateTask)
}

@Composable
internal fun TaskListScreen(
    state: TaskListState,
    onIntent: (TaskListIntent) -> Unit,
    onOpenTask: (String) -> Unit,
    onCreateTask: () -> Unit,
) {
    val snackbarHostState = remember { SnackbarHostState() }
    AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = { onIntent(TaskListIntent.ErrorShown) })

    Scaffold(
        topBar = { AedstTopAppBar(title = stringResource(R.string.tasks_title)) },
        snackbarHost = { AedstSnackbarHost(snackbarHostState) },
        floatingActionButton = {
            FloatingActionButton(onClick = onCreateTask) {
                Icon(Icons.Filled.Add, contentDescription = stringResource(R.string.tasks_create))
            }
        },
    ) { padding ->
        when {
            state.isLoading && state.tasks.isEmpty() -> FullScreenLoading(Modifier.padding(padding))
            state.isEmpty -> Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) {
                Text(stringResource(R.string.tasks_empty), style = MaterialTheme.typography.bodyLarge)
            }
            else -> TaskList(state, onIntent, onOpenTask, PaddingValues(bottom = 88.dp), Modifier.padding(padding))
        }
    }
}

@Composable
private fun TaskList(
    state: TaskListState,
    onIntent: (TaskListIntent) -> Unit,
    onOpenTask: (String) -> Unit,
    contentPadding: PaddingValues,
    modifier: Modifier = Modifier,
) {
    val listState = rememberLazyListState()
    val shouldLoadMore by remember {
        derivedStateOf {
            val last = listState.layoutInfo.visibleItemsInfo.lastOrNull()?.index ?: 0
            last >= listState.layoutInfo.totalItemsCount - LOAD_MORE_THRESHOLD
        }
    }
    LaunchedEffect(shouldLoadMore, state.canLoadMore) {
        if (shouldLoadMore && state.canLoadMore) onIntent(TaskListIntent.LoadMore)
    }
    LazyColumn(state = listState, modifier = modifier.fillMaxSize(), contentPadding = contentPadding) {
        items(state.tasks, key = Task::id) { task ->
            TaskRow(task = task, onClick = { onOpenTask(task.id) })
            HorizontalDivider()
        }
    }
}

@Composable
private fun TaskRow(task: Task, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(
                text = task.title,
                style = MaterialTheme.typography.titleMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            val meta = listOfNotNull(priorityLabel(task.priority), task.dueDate?.formatDate()).joinToString(" · ")
            Text(text = meta, style = MaterialTheme.typography.bodySmall)
        }
        TaskStatusBadge(task.status)
    }
}

private const val LOAD_MORE_THRESHOLD = 5
