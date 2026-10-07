package com.aedst.feature.search.presentation

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.feature.search.domain.SearchHit
import com.aedst.feature.search.domain.SearchRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.FlowPreview
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.debounce
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import javax.inject.Inject

sealed interface SearchResultState {
    data object Idle : SearchResultState
    data object Loading : SearchResultState
    data class Results(val hits: List<SearchHit>, val totalEstimate: Int) : SearchResultState
    data class Failed(val error: AedstError) : SearchResultState
}

data class SearchState(val query: String = "", val result: SearchResultState = SearchResultState.Idle)

/**
 * Debounced search-as-you-type: the query is debounced ([DEBOUNCE_MS]) and de-duplicated, and `flatMapLatest`
 * cancels the in-flight RPC when the user keeps typing, so only the latest query's results are ever shown.
 */
@OptIn(FlowPreview::class, ExperimentalCoroutinesApi::class)
@HiltViewModel
class SearchViewModel @Inject constructor(private val repository: SearchRepository) : ViewModel() {

    private val query = MutableStateFlow("")

    private val result = query
        .map(String::trim)
        .debounce(DEBOUNCE_MS)
        .distinctUntilChanged()
        .flatMapLatest { text ->
            if (text.length < MIN_QUERY_LENGTH) {
                flowOf(SearchResultState.Idle)
            } else {
                flow {
                    emit(SearchResultState.Loading)
                    emit(
                        when (val outcome = repository.search(text)) {
                            is Outcome.Success -> SearchResultState.Results(
                                outcome.value.hits,
                                outcome.value.totalEstimate,
                            )
                            is Outcome.Failure -> SearchResultState.Failed(outcome.error)
                        },
                    )
                }
            }
        }

    val state: StateFlow<SearchState> = combine(query, result) { text, current -> SearchState(text, current) }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), SearchState())

    fun onQueryChange(text: String) {
        query.value = text
    }

    companion object {
        const val DEBOUNCE_MS = 300L
        const val MIN_QUERY_LENGTH = 2
        private const val STOP_TIMEOUT_MS = 5_000L
    }
}
