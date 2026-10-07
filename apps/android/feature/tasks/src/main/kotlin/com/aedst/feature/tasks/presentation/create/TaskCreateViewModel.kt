package com.aedst.feature.tasks.presentation.create

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.analytics.AnalyticsTracker
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.feature.tasks.domain.NewTask
import com.aedst.feature.tasks.domain.TaskPriority
import com.aedst.feature.tasks.domain.TaskRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class TaskCreateState(
    val title: String = "",
    val description: String = "",
    val priority: TaskPriority = TaskPriority.MEDIUM,
    val isSaving: Boolean = false,
    val isTitleInvalid: Boolean = false,
    val error: AedstError? = null,
)

sealed interface TaskCreateIntent {
    data class TitleChanged(val value: String) : TaskCreateIntent
    data class DescriptionChanged(val value: String) : TaskCreateIntent
    data class PriorityChanged(val value: TaskPriority) : TaskCreateIntent
    data object Submit : TaskCreateIntent
    data object ErrorShown : TaskCreateIntent
}

@HiltViewModel
class TaskCreateViewModel @Inject constructor(
    private val repository: TaskRepository,
    private val analytics: AnalyticsTracker,
) : ViewModel() {

    private val mutableState = MutableStateFlow(TaskCreateState())
    val state: StateFlow<TaskCreateState> = mutableState.asStateFlow()

    /** Emits the new task's id once, for navigation to its detail screen. */
    private val createdChannel = Channel<String>(Channel.BUFFERED)
    val created: Flow<String> = createdChannel.receiveAsFlow()

    fun onIntent(intent: TaskCreateIntent) {
        when (intent) {
            is TaskCreateIntent.TitleChanged -> mutableState.update {
                it.copy(title = intent.value.take(MAX_TITLE_LENGTH), isTitleInvalid = false)
            }
            is TaskCreateIntent.DescriptionChanged -> mutableState.update {
                it.copy(description = intent.value.take(MAX_DESCRIPTION_LENGTH))
            }
            is TaskCreateIntent.PriorityChanged -> mutableState.update { it.copy(priority = intent.value) }
            TaskCreateIntent.Submit -> submit()
            TaskCreateIntent.ErrorShown -> mutableState.update { it.copy(error = null) }
        }
    }

    private fun submit() {
        val current = mutableState.value
        if (current.isSaving) return
        // Mirrors the proto's buf.validate rule (title 1..200) so the common mistake never hits the network.
        if (current.title.isBlank()) {
            mutableState.update { it.copy(isTitleInvalid = true) }
            return
        }
        mutableState.update { it.copy(isSaving = true) }
        viewModelScope.launch {
            val outcome = repository.create(NewTask(current.title, current.description, current.priority, null))
            when (outcome) {
                is Outcome.Success -> {
                    analytics.logEvent("task_created", mapOf("priority" to current.priority.name.lowercase()))
                    mutableState.update { it.copy(isSaving = false) }
                    createdChannel.send(outcome.value.id)
                }
                is Outcome.Failure -> mutableState.update { it.copy(isSaving = false, error = outcome.error) }
            }
        }
    }

    private companion object {
        const val MAX_TITLE_LENGTH = 200
        const val MAX_DESCRIPTION_LENGTH = 4000
    }
}
