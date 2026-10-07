package com.aedst.feature.media.presentation.library

import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.core.media.MediaItem
import com.aedst.core.media.data.MediaRepository
import com.aedst.core.media.upload.MediaUploadScheduler
import com.aedst.core.media.upload.UploadProgress
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class MediaLibraryState(
    val items: List<MediaItem> = emptyList(),
    val nextPageToken: String? = null,
    val isLoading: Boolean = true,
    val error: AedstError? = null,
)

@HiltViewModel
class MediaLibraryViewModel @Inject constructor(
    private val repository: MediaRepository,
    private val uploads: MediaUploadScheduler,
) : ViewModel() {

    private val mutableState = MutableStateFlow(MediaLibraryState())
    val state: StateFlow<MediaLibraryState> = mutableState.asStateFlow()

    /** Uploads not attached to a task show here too (all uploads), minus finished/cancelled ones. */
    val activeUploads: StateFlow<List<UploadProgress>> = uploads.observe()
        .map { list ->
            list.filter { it.state != UploadProgress.State.DONE && it.state != UploadProgress.State.CANCELLED }
        }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), emptyList())

    init {
        viewModelScope.launch {
            uploads.observe()
                .map { list -> list.count { it.state == UploadProgress.State.DONE } }
                .distinctUntilChanged()
                .collect { refresh() }
        }
    }

    fun refresh() {
        load(pageToken = null)
    }

    fun loadMore() {
        mutableState.value.nextPageToken?.let { load(it) }
    }

    fun upload(uris: List<Uri>) {
        uris.forEach { uploads.enqueue(it) }
    }

    fun cancelUpload(uploadId: String) {
        uploads.cancel(uploadId)
    }

    fun errorShown() {
        mutableState.update { it.copy(error = null) }
    }

    private fun load(pageToken: String?) {
        mutableState.update { it.copy(isLoading = true) }
        viewModelScope.launch {
            when (val outcome = repository.list(pageToken = pageToken)) {
                is Outcome.Success -> mutableState.update {
                    val page = outcome.value.items
                    it.copy(
                        items = if (pageToken == null) page else it.items + page,
                        nextPageToken = outcome.value.nextPageToken,
                        isLoading = false,
                    )
                }
                is Outcome.Failure -> mutableState.update { it.copy(isLoading = false, error = outcome.error) }
            }
        }
    }

    private companion object {
        const val STOP_TIMEOUT_MS = 5_000L
    }
}
