package com.aedst.feature.search.presentation

import com.aedst.core.common.Outcome
import com.aedst.core.testing.MainDispatcherRule
import com.aedst.feature.search.domain.SearchRepository
import com.aedst.feature.search.domain.SearchResults
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class SearchViewModelTest {

    @get:Rule
    val mainDispatcherRule = MainDispatcherRule()

    private val queries = mutableListOf<String>()
    private val repository = object : SearchRepository {
        override suspend fun search(query: String, pageToken: String?): Outcome<SearchResults> {
            queries += query
            return Outcome.Success(SearchResults(emptyList(), nextPageToken = null, totalEstimate = 0))
        }
    }

    @Test
    fun `typing quickly sends only the settled query`() = runTest(mainDispatcherRule.testDispatcher) {
        val viewModel = SearchViewModel(repository)
        val collector = backgroundScope.launch { viewModel.state.collect { } }

        "milk".forEachIndexed { index, _ -> viewModel.onQueryChange("milk".take(index + 1)) }
        testScheduler.advanceTimeBy(SearchViewModel.DEBOUNCE_MS + 1)
        testScheduler.runCurrent()

        assertEquals(listOf("milk"), queries)
        assertTrue(viewModel.state.value.result is SearchResultState.Results)
        collector.cancel()
    }

    @Test
    fun `queries shorter than the minimum never hit the network`() = runTest(mainDispatcherRule.testDispatcher) {
        val viewModel = SearchViewModel(repository)
        val collector = backgroundScope.launch { viewModel.state.collect { } }

        viewModel.onQueryChange("m")
        testScheduler.advanceTimeBy(SearchViewModel.DEBOUNCE_MS + 1)
        testScheduler.runCurrent()

        assertTrue(queries.isEmpty())
        assertEquals(SearchResultState.Idle, viewModel.state.value.result)
        collector.cancel()
    }
}
