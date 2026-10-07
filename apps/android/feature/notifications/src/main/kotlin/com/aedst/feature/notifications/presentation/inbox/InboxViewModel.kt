package com.aedst.feature.notifications.presentation.inbox

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.feature.notifications.domain.InboxNotification
import com.aedst.feature.notifications.domain.NotificationsRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.time.Instant
import javax.inject.Inject

data class InboxState(
    val notifications: List<InboxNotification> = emptyList(),
    val nextPageToken: String? = null,
    val isLoading: Boolean = true,
    val error: AedstError? = null,
) {
    val hasUnread: Boolean get() = notifications.any(InboxNotification::isUnread)
}

sealed interface InboxIntent {
    data object Refresh : InboxIntent
    data object LoadMore : InboxIntent
    data class Open(val notification: InboxNotification) : InboxIntent
    data object MarkAllRead : InboxIntent
    data object ErrorShown : InboxIntent
}

@HiltViewModel
class InboxViewModel @Inject constructor(private val repository: NotificationsRepository) : ViewModel() {

    private val mutableState = MutableStateFlow(InboxState())
    val state: StateFlow<InboxState> = mutableState.asStateFlow()

    fun onIntent(intent: InboxIntent) {
        when (intent) {
            InboxIntent.Refresh -> load(pageToken = null)
            InboxIntent.LoadMore -> mutableState.value.nextPageToken?.let { load(it) }
            is InboxIntent.Open -> markRead(intent.notification)
            InboxIntent.MarkAllRead -> markAllRead()
            InboxIntent.ErrorShown -> mutableState.update { it.copy(error = null) }
        }
    }

    private fun load(pageToken: String?) {
        mutableState.update { it.copy(isLoading = true) }
        viewModelScope.launch {
            when (val outcome = repository.list(pageToken = pageToken)) {
                is Outcome.Success -> mutableState.update {
                    val page = outcome.value.notifications
                    it.copy(
                        notifications = if (pageToken == null) page else it.notifications + page,
                        nextPageToken = outcome.value.nextPageToken,
                        isLoading = false,
                    )
                }
                is Outcome.Failure -> mutableState.update { it.copy(isLoading = false, error = outcome.error) }
            }
            if (pageToken == null) repository.unreadCount()
        }
    }

    /** Optimistic: the row turns read immediately; the badge follows via the shared unread store. */
    private fun markRead(notification: InboxNotification) {
        if (!notification.isUnread) return
        val now = Instant.now()
        mutableState.update { state ->
            state.copy(
                notifications = state.notifications.map { if (it.id == notification.id) it.copy(readAt = now) else it },
            )
        }
        viewModelScope.launch {
            (repository.markRead(listOf(notification.id)) as? Outcome.Failure)?.let { failure ->
                mutableState.update { it.copy(error = failure.error) }
            }
        }
    }

    private fun markAllRead() {
        val now = Instant.now()
        mutableState.update { state ->
            state.copy(notifications = state.notifications.map { it.copy(readAt = it.readAt ?: now) })
        }
        viewModelScope.launch {
            (repository.markAllRead() as? Outcome.Failure)?.let { failure ->
                mutableState.update { it.copy(error = failure.error) }
            }
        }
    }
}
