package com.aedst.feature.tasks.presentation.list

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.feature.tasks.domain.Task
import com.aedst.feature.tasks.domain.TaskRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class TaskListState(
    val tasks: List<Task> = emptyList(),
    val totalCount: Int = 0,
    val nextPageToken: String? = null,
    val isLoading: Boolean = true,
    val isLoadingMore: Boolean = false,
    /** Transient error, shown once as a snackbar. */
    val error: AedstError? = null,
) {
    val isEmpty: Boolean get() = !isLoading && tasks.isEmpty()
    val canLoadMore: Boolean get() = nextPageToken != null && !isLoadingMore
}

sealed interface TaskListIntent {
    data object Refresh : TaskListIntent
    data object LoadMore : TaskListIntent
    data object ErrorShown : TaskListIntent
}

@HiltViewModel
class TaskListViewModel @Inject constructor(private val repository: TaskRepository) : ViewModel() {

    private val mutableState = MutableStateFlow(TaskListState())
    val state: StateFlow<TaskListState> = mutableState.asStateFlow()

    fun onIntent(intent: TaskListIntent) {
        when (intent) {
            TaskListIntent.Refresh -> refresh()
            TaskListIntent.LoadMore -> loadMore()
            TaskListIntent.ErrorShown -> mutableState.update { it.copy(error = null) }
        }
    }

    private fun refresh() {
        mutableState.update { it.copy(isLoading = true) }
        viewModelScope.launch {
            when (val outcome = repository.list()) {
                is Outcome.Success -> mutableState.update {
                    it.copy(
                        tasks = outcome.value.tasks,
                        totalCount = outcome.value.totalCount,
                        nextPageToken = outcome.value.nextPageToken,
                        isLoading = false,
                    )
                }
                is Outcome.Failure -> mutableState.update { it.copy(isLoading = false, error = outcome.error) }
            }
        }
    }

    private fun loadMore() {
        val token = mutableState.value.nextPageToken ?: return
        if (mutableState.value.isLoadingMore) return
        mutableState.update { it.copy(isLoadingMore = true) }
        viewModelScope.launch {
            when (val outcome = repository.list(token)) {
                is Outcome.Success -> mutableState.update {
                    it.copy(
                        tasks = it.tasks + outcome.value.tasks,
                        nextPageToken = outcome.value.nextPageToken,
                        isLoadingMore = false,
                    )
                }
                is Outcome.Failure -> mutableState.update { it.copy(isLoadingMore = false, error = outcome.error) }
            }
        }
    }
}
