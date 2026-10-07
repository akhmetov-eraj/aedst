package com.aedst.feature.admin.presentation

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.ui.components.AedstErrorSnackbarEffect
import com.aedst.core.ui.components.AedstSnackbarHost
import com.aedst.core.ui.components.AedstTabs
import com.aedst.core.ui.components.AedstTextField
import com.aedst.core.ui.components.AedstTopAppBar
import com.aedst.core.ui.components.FullScreenLoading
import com.aedst.feature.admin.R
import com.aedst.feature.admin.domain.AdminOverview
import com.aedst.feature.admin.domain.AdminTaskRow

@Composable
fun AdminRoute(viewModel: AdminViewModel = hiltViewModel()) {
    val isAdmin by viewModel.isAdmin.collectAsStateWithLifecycle()
    val state by viewModel.state.collectAsStateWithLifecycle()
    if (!isAdmin) {
        Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
            Text(stringResource(R.string.admin_forbidden), style = MaterialTheme.typography.bodyLarge)
        }
        return
    }
    AdminScreen(state = state, onIntent = viewModel::onIntent)
}

@Composable
internal fun AdminScreen(state: AdminState, onIntent: (AdminIntent) -> Unit) {
    val snackbarHostState = remember { SnackbarHostState() }
    AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = { onIntent(AdminIntent.ErrorShown) })
    state.pendingDelete?.let { task -> DeleteTaskDialog(task, onIntent) }

    Scaffold(
        topBar = { AedstTopAppBar(title = stringResource(R.string.admin_title)) },
        snackbarHost = { AedstSnackbarHost(snackbarHostState) },
    ) { padding ->
        Column(modifier = Modifier.fillMaxSize().padding(padding)) {
            AedstTabs(
                tabs = AdminTab.entries.map { stringResource(it.labelRes()) },
                selectedIndex = state.tab.ordinal,
                onTabSelected = { onIntent(AdminIntent.SelectTab(AdminTab.entries[it])) },
                scrollable = true,
            )
            if (state.tab == AdminTab.USERS || state.tab == AdminTab.TASKS) SearchField(state.query, onIntent)
            Box(modifier = Modifier.fillMaxSize()) {
                if (state.isLoading) {
                    FullScreenLoading()
                } else {
                    TabContent(state, onIntent)
                }
            }
        }
    }
}

@Composable
private fun TabContent(state: AdminState, onIntent: (AdminIntent) -> Unit) {
    when (state.tab) {
        AdminTab.OVERVIEW -> state.overview?.let { Overview(it) }
        AdminTab.USERS -> Rows(state.users.map { it.id to "${it.email} · ${it.displayName} · ${it.taskCount}" })
        AdminTab.TASKS -> LazyColumn(Modifier.fillMaxSize()) {
            items(state.tasks, key = AdminTaskRow::id) { task ->
                Row(
                    modifier = Modifier.fillMaxWidth().padding(start = 16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(task.title, modifier = Modifier.weight(1f), style = MaterialTheme.typography.bodyLarge)
                    IconButton(onClick = { onIntent(AdminIntent.RequestDelete(task)) }) {
                        Icon(Icons.Filled.Delete, contentDescription = stringResource(R.string.admin_delete_task))
                    }
                }
                HorizontalDivider()
            }
        }
        AdminTab.AUDIT -> Rows(state.audit.map { it.id to "${it.action} · ${it.target} · ${it.reason}" })
    }
}

@Composable
private fun Overview(overview: AdminOverview) {
    val rows = listOf(
        R.string.admin_users_total to overview.usersTotal,
        R.string.admin_tasks_total to overview.tasksTotal,
        R.string.admin_tasks_done to overview.tasksDone,
        R.string.admin_tasks_overdue to overview.tasksOverdue,
        R.string.admin_media_total to overview.mediaTotal,
        R.string.admin_push_devices to overview.pushDevices,
        R.string.admin_outbox_lag to overview.outboxLagSeconds,
        R.string.admin_jobs_pending to overview.jobsPending,
    )
    Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        rows.forEach { (label, value) ->
            Row(modifier = Modifier.fillMaxWidth()) {
                Text(stringResource(label), modifier = Modifier.weight(1f))
                Text(value.toString(), style = MaterialTheme.typography.titleMedium)
            }
        }
    }
}

@Composable
private fun Rows(rows: List<Pair<String, String>>) {
    LazyColumn(Modifier.fillMaxSize()) {
        items(rows, key = { it.first }) { (_, text) ->
            Text(text, modifier = Modifier.padding(16.dp), style = MaterialTheme.typography.bodyMedium)
            HorizontalDivider()
        }
    }
}

@Composable
private fun SearchField(query: String, onIntent: (AdminIntent) -> Unit) {
    AedstTextField(
        value = query,
        onValueChange = { onIntent(AdminIntent.QueryChanged(it)) },
        placeholder = stringResource(R.string.admin_search),
        leadingIcon = { Icon(Icons.Filled.Search, contentDescription = null) },
        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
        keyboardActions = KeyboardActions(onSearch = { onIntent(AdminIntent.Search) }),
        modifier = Modifier.fillMaxWidth().padding(16.dp),
    )
}

@Composable
private fun DeleteTaskDialog(task: AdminTaskRow, onIntent: (AdminIntent) -> Unit) {
    var reason by remember(task.id) { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = { onIntent(AdminIntent.DismissDelete) },
        title = { Text(stringResource(R.string.admin_delete_task)) },
        text = {
            AedstTextField(
                value = reason,
                onValueChange = { reason = it.take(MAX_REASON_LENGTH) },
                label = stringResource(R.string.admin_delete_reason),
                singleLine = false,
            )
        },
        confirmButton = {
            TextButton(
                onClick = { onIntent(AdminIntent.ConfirmDelete(reason)) },
                enabled = reason.trim().length >= AdminViewModel.MIN_REASON_LENGTH,
            ) { Text(stringResource(R.string.admin_delete_confirm)) }
        },
        dismissButton = {
            TextButton(onClick = { onIntent(AdminIntent.DismissDelete) }) {
                Text(stringResource(R.string.admin_cancel))
            }
        },
    )
}

private fun AdminTab.labelRes(): Int = when (this) {
    AdminTab.OVERVIEW -> R.string.admin_tab_overview
    AdminTab.USERS -> R.string.admin_tab_users
    AdminTab.TASKS -> R.string.admin_tab_tasks
    AdminTab.AUDIT -> R.string.admin_tab_audit
}

private const val MAX_REASON_LENGTH = 500
