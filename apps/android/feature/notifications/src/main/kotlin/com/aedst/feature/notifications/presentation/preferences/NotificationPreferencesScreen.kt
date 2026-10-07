package com.aedst.feature.notifications.presentation.preferences

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.ui.components.AedstErrorSnackbarEffect
import com.aedst.core.ui.components.AedstSnackbarHost
import com.aedst.core.ui.components.AedstSwitch
import com.aedst.core.ui.components.AedstTopAppBar
import com.aedst.core.ui.components.FullScreenLoading
import com.aedst.feature.notifications.R
import com.aedst.feature.notifications.domain.NotificationPreferences

@Composable
fun NotificationPreferencesRoute(onBack: () -> Unit, viewModel: NotificationPreferencesViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    NotificationPreferencesScreen(
        state = state,
        onChange = viewModel::update,
        onErrorShown = viewModel::errorShown,
        onBack = onBack,
    )
}

@Composable
internal fun NotificationPreferencesScreen(
    state: NotificationPreferencesState,
    onChange: ((NotificationPreferences) -> NotificationPreferences) -> Unit,
    onErrorShown: () -> Unit,
    onBack: () -> Unit,
) {
    val snackbarHostState = remember { SnackbarHostState() }
    AedstErrorSnackbarEffect(state.error, snackbarHostState, onShown = onErrorShown)

    Scaffold(
        topBar = {
            AedstTopAppBar(
                title = stringResource(R.string.notifications_preferences),
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(
                            Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = stringResource(R.string.notifications_back),
                        )
                    }
                },
            )
        },
        snackbarHost = { AedstSnackbarHost(snackbarHostState) },
    ) { padding ->
        val prefs = state.preferences
        if (prefs == null) {
            FullScreenLoading(Modifier.padding(padding))
            return@Scaffold
        }
        Column(
            modifier = Modifier.fillMaxSize().padding(padding).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            AedstSwitch(
                checked = prefs.pushEnabled,
                onCheckedChange = { value -> onChange { it.copy(pushEnabled = value) } },
                label = stringResource(R.string.notifications_pref_push),
                modifier = Modifier.fillMaxWidth(),
            )
            AedstSwitch(
                checked = prefs.taskCompleted,
                onCheckedChange = { value -> onChange { it.copy(taskCompleted = value) } },
                label = stringResource(R.string.notifications_pref_task_completed),
                enabled = prefs.pushEnabled,
                modifier = Modifier.fillMaxWidth(),
            )
            AedstSwitch(
                checked = prefs.taskDueSoon,
                onCheckedChange = { value -> onChange { it.copy(taskDueSoon = value) } },
                label = stringResource(R.string.notifications_pref_task_due_soon),
                enabled = prefs.pushEnabled,
                modifier = Modifier.fillMaxWidth(),
            )
            AedstSwitch(
                checked = prefs.mediaReady,
                onCheckedChange = { value -> onChange { it.copy(mediaReady = value) } },
                label = stringResource(R.string.notifications_pref_media_ready),
                enabled = prefs.pushEnabled,
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
