package com.aedst.feature.notifications.presentation.preferences

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.feature.notifications.domain.NotificationPreferences
import com.aedst.feature.notifications.domain.NotificationsRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class NotificationPreferencesState(
    val preferences: NotificationPreferences? = null,
    val isSaving: Boolean = false,
    val error: AedstError? = null,
)

@HiltViewModel
class NotificationPreferencesViewModel @Inject constructor(private val repository: NotificationsRepository) :
    ViewModel() {

    private val mutableState = MutableStateFlow(NotificationPreferencesState())
    val state: StateFlow<NotificationPreferencesState> = mutableState.asStateFlow()

    init {
        viewModelScope.launch {
            when (val outcome = repository.preferences()) {
                is Outcome.Success -> mutableState.update { it.copy(preferences = outcome.value) }
                is Outcome.Failure -> mutableState.update { it.copy(error = outcome.error) }
            }
        }
    }

    /** Optimistic update; reverts to the previous value when the server rejects it. */
    fun update(transform: (NotificationPreferences) -> NotificationPreferences) {
        val previous = mutableState.value.preferences ?: return
        val next = transform(previous)
        mutableState.update { it.copy(preferences = next, isSaving = true) }
        viewModelScope.launch {
            when (val outcome = repository.updatePreferences(next)) {
                is Outcome.Success -> mutableState.update { it.copy(preferences = outcome.value, isSaving = false) }
                is Outcome.Failure -> mutableState.update {
                    it.copy(preferences = previous, isSaving = false, error = outcome.error)
                }
            }
        }
    }

    fun errorShown() {
        mutableState.update { it.copy(error = null) }
    }
}
