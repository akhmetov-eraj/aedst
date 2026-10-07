package com.aedst.feature.tasks.presentation.detail

import android.net.Uri
import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.analytics.AnalyticsTracker
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.core.media.MediaItem
import com.aedst.core.media.data.MediaRepository
import com.aedst.core.media.upload.MediaUploadScheduler
import com.aedst.core.media.upload.UploadProgress
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.tasks.domain.Task
import com.aedst.feature.tasks.domain.TaskRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class TaskDetailState(
    val task: Task? = null,
    val attachments: List<MediaItem> = emptyList(),
    val isLoading: Boolean = true,
    val isMutating: Boolean = false,
    val error: AedstError? = null,
)

sealed interface TaskDetailIntent {
    data object ToggleDone : TaskDetailIntent
    data object Delete : TaskDetailIntent
    data class Attach(val uris: List<Uri>) : TaskDetailIntent
    data class CancelUpload(val uploadId: String) : TaskDetailIntent
    data object Refresh : TaskDetailIntent
    data object ErrorShown : TaskDetailIntent
}

sealed interface TaskDetailEffect {
    data object Deleted : TaskDetailEffect
}

/**
 * Task + its attachments (MediaService `ListMedia(task_id)`) + in-flight uploads (WorkManager). A finished upload
 * triggers an attachment reload, so a new file appears without pull-to-refresh.
 */
@HiltViewModel
class TaskDetailViewModel @Inject constructor(
    savedStateHandle: SavedStateHandle,
    private val tasks: TaskRepository,
    private val media: MediaRepository,
    private val uploads: MediaUploadScheduler,
    private val analytics: AnalyticsTracker,
) : ViewModel() {

    private val taskId: String = checkNotNull(savedStateHandle[AedstRoutes.TASK_ID_ARG])

    private val mutableState = MutableStateFlow(TaskDetailState())
    val state: StateFlow<TaskDetailState> = mutableState.asStateFlow()

    val uploadsInFlight: StateFlow<List<UploadProgress>> = uploads.observe(taskId)
        .map { list -> list.filter { it.state != UploadProgress.State.CANCELLED } }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), emptyList())

    private val effectChannel = Channel<TaskDetailEffect>(Channel.BUFFERED)
    val effects: Flow<TaskDetailEffect> = effectChannel.receiveAsFlow()

    init {
        load()
        viewModelScope.launch {
            uploads.observe(taskId)
                .map { list -> list.count { it.state == UploadProgress.State.DONE } }
                .distinctUntilChanged()
                .collect { done -> if (done > 0) loadAttachments() }
        }
    }

    fun onIntent(intent: TaskDetailIntent) {
        when (intent) {
            TaskDetailIntent.ToggleDone -> toggleDone()
            TaskDetailIntent.Delete -> delete()
            is TaskDetailIntent.Attach -> {
                intent.uris.forEach { uploads.enqueue(it, taskId) }
                analytics.logEvent("task_attachment_added", mapOf("count" to intent.uris.size))
            }
            is TaskDetailIntent.CancelUpload -> uploads.cancel(intent.uploadId)
            TaskDetailIntent.Refresh -> load()
            TaskDetailIntent.ErrorShown -> mutableState.update { it.copy(error = null) }
        }
    }

    private fun load() {
        mutableState.update { it.copy(isLoading = true) }
        viewModelScope.launch {
            when (val outcome = tasks.get(taskId)) {
                is Outcome.Success -> mutableState.update { it.copy(task = outcome.value, isLoading = false) }
                is Outcome.Failure -> mutableState.update { it.copy(isLoading = false, error = outcome.error) }
            }
            loadAttachments()
        }
    }

    private suspend fun loadAttachments() {
        when (val outcome = media.list(taskId = taskId)) {
            is Outcome.Success -> mutableState.update { it.copy(attachments = outcome.value.items) }
            is Outcome.Failure -> mutableState.update { it.copy(error = outcome.error) }
        }
    }

    private fun toggleDone() {
        val task = mutableState.value.task ?: return
        if (mutableState.value.isMutating) return
        mutableState.update { it.copy(isMutating = true) }
        viewModelScope.launch {
            val outcome = if (task.isDone) tasks.reopen(task.id) else tasks.complete(task.id)
            when (outcome) {
                is Outcome.Success -> {
                    if (outcome.value.isDone) analytics.logEvent("task_completed")
                    mutableState.update { it.copy(task = outcome.value, isMutating = false) }
                }
                is Outcome.Failure -> mutableState.update { it.copy(isMutating = false, error = outcome.error) }
            }
        }
    }

    private fun delete() {
        if (mutableState.value.isMutating) return
        mutableState.update { it.copy(isMutating = true) }
        viewModelScope.launch {
            when (val outcome = tasks.delete(taskId)) {
                is Outcome.Success -> effectChannel.send(TaskDetailEffect.Deleted)
                is Outcome.Failure -> mutableState.update { it.copy(isMutating = false, error = outcome.error) }
            }
        }
    }

    private companion object {
        const val STOP_TIMEOUT_MS = 5_000L
    }
}
