package com.aedst.feature.notifications.presentation.inbox

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
import androidx.compose.material.icons.filled.DoneAll
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.LifecycleStartEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.ui.components.AedstButton
import com.aedst.core.ui.components.AedstButtonVariant
import com.aedst.core.ui.components.AedstErrorSnackbarEffect
import com.aedst.core.ui.components.AedstSnackbarHost
import com.aedst.core.ui.components.AedstTopAppBar
import com.aedst.core.ui.components.FullScreenLoading
import com.aedst.feature.notifications.R
import com.aedst.feature.notifications.domain.InboxNotification

@Composable
fun InboxRoute(
    onOpenLink: (String) -> Unit,
    onOpenPreferences: () -> Unit,
    viewModel: InboxViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    LifecycleStartEffect(viewModel) {
        viewModel.onIntent(InboxIntent.Refresh)
        onStopOrDispose { }
    }
    InboxScreen(
        state = state,
        onIntent = viewModel::onIntent,
        onOpen = { notification ->
            viewModel.onIntent(InboxIntent.Open(notification))
            notification.link?.let(onOpenLink)
        },
        onOpenPreferences = onOpenPreferences,
    )
}

@Composable
internal fun InboxScreen(
    state: InboxState,
    onIntent: (InboxIntent) -> Unit,
    onOpen: (InboxNotification) -> Unit,
    onOpenPreferences: () -> Unit,
) {
    val snackbarHostState = remember { SnackbarHostState() }
    AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = { onIntent(InboxIntent.ErrorShown) })

    Scaffold(
        topBar = {
            AedstTopAppBar(
                title = stringResource(R.string.notifications_title),
                actions = {
                    if (state.hasUnread) {
                        IconButton(onClick = { onIntent(InboxIntent.MarkAllRead) }) {
                            Icon(
                                Icons.Filled.DoneAll,
                                contentDescription = stringResource(R.string.notifications_mark_all_read),
                            )
                        }
                    }
                    IconButton(onClick = onOpenPreferences) {
                        Icon(
                            Icons.Filled.Settings,
                            contentDescription = stringResource(R.string.notifications_preferences),
                        )
                    }
                },
            )
        },
        snackbarHost = { AedstSnackbarHost(snackbarHostState) },
    ) { padding ->
        when {
            state.isLoading && state.notifications.isEmpty() -> FullScreenLoading(Modifier.padding(padding))
            state.notifications.isEmpty() ->
                Box(Modifier.fillMaxSize().padding(padding), contentAlignment = Alignment.Center) {
                    Text(stringResource(R.string.notifications_empty), style = MaterialTheme.typography.bodyLarge)
                }
            else -> LazyColumn(modifier = Modifier.fillMaxSize().padding(padding)) {
                items(state.notifications, key = InboxNotification::id) { notification ->
                    NotificationRow(notification, onClick = { onOpen(notification) })
                    HorizontalDivider()
                }
                if (state.nextPageToken != null) {
                    item {
                        AedstButton(
                            text = stringResource(R.string.notifications_load_more),
                            onClick = { onIntent(InboxIntent.LoadMore) },
                            variant = AedstButtonVariant.GHOST,
                            loading = state.isLoading,
                            modifier = Modifier.fillMaxWidth().padding(16.dp),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun NotificationRow(notification: InboxNotification, onClick: () -> Unit) {
    val weight = if (notification.isUnread) FontWeight.SemiBold else FontWeight.Normal
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(notification.title, style = MaterialTheme.typography.titleMedium, fontWeight = weight)
        if (notification.body.isNotBlank()) {
            Text(notification.body, style = MaterialTheme.typography.bodyMedium)
        }
    }
}
