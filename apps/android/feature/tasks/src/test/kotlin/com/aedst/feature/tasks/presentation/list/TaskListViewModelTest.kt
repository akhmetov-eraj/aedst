package com.aedst.feature.tasks.presentation.list

import com.aedst.core.common.AedstError
import com.aedst.core.common.Outcome
import com.aedst.core.testing.MainDispatcherRule
import com.aedst.feature.tasks.domain.NewTask
import com.aedst.feature.tasks.domain.Task
import com.aedst.feature.tasks.domain.TaskPage
import com.aedst.feature.tasks.domain.TaskPriority
import com.aedst.feature.tasks.domain.TaskRepository
import com.aedst.feature.tasks.domain.TaskStatus
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class TaskListViewModelTest {

    @get:Rule
    val mainDispatcherRule = MainDispatcherRule()

    private val repository = FakeTaskRepository()

    @Test
    fun `refresh loads page one, load more appends`() = runTest(mainDispatcherRule.testDispatcher) {
        repository.pages = mapOf(
            null to TaskPage(listOf(task("1"), task("2")), nextPageToken = "p2", totalCount = 3),
            "p2" to TaskPage(listOf(task("3")), nextPageToken = null, totalCount = 0),
        )
        val viewModel = TaskListViewModel(repository)

        viewModel.onIntent(TaskListIntent.Refresh)
        testScheduler.advanceUntilIdle()
        assertEquals(listOf("1", "2"), viewModel.state.value.tasks.map(Task::id))
        assertEquals(3, viewModel.state.value.totalCount)
        assertTrue(viewModel.state.value.canLoadMore)

        viewModel.onIntent(TaskListIntent.LoadMore)
        testScheduler.advanceUntilIdle()
        assertEquals(listOf("1", "2", "3"), viewModel.state.value.tasks.map(Task::id))
        assertFalse(viewModel.state.value.canLoadMore)
    }

    @Test
    fun `failure is shown once as an error kind`() = runTest(mainDispatcherRule.testDispatcher) {
        repository.failure = AedstError.Offline()
        val viewModel = TaskListViewModel(repository)

        viewModel.onIntent(TaskListIntent.Refresh)
        testScheduler.advanceUntilIdle()
        assertTrue(viewModel.state.value.error is AedstError.Offline)
        assertFalse(viewModel.state.value.isLoading)

        viewModel.onIntent(TaskListIntent.ErrorShown)
        assertNull(viewModel.state.value.error)
    }

    private fun task(id: String) = Task(
        id = id,
        title = "Task $id",
        description = "",
        status = TaskStatus.TODO,
        priority = TaskPriority.MEDIUM,
        dueDate = null,
        createdAt = null,
        completedAt = null,
        version = 1,
    )
}

private class FakeTaskRepository : TaskRepository {
    var pages: Map<String?, TaskPage> = emptyMap()
    var failure: AedstError? = null

    override suspend fun list(pageToken: String?): Outcome<TaskPage> =
        failure?.let { Outcome.Failure(it) } ?: Outcome.Success(pages.getValue(pageToken))

    override suspend fun get(id: String): Outcome<Task> = Outcome.Failure(AedstError.NotFound())

    override suspend fun create(task: NewTask): Outcome<Task> = Outcome.Failure(AedstError.Unknown("unused"))

    override suspend fun complete(id: String): Outcome<Task> = Outcome.Failure(AedstError.Unknown("unused"))

    override suspend fun reopen(id: String): Outcome<Task> = Outcome.Failure(AedstError.Unknown("unused"))

    override suspend fun delete(id: String): Outcome<Unit> = Outcome.Failure(AedstError.Unknown("unused"))
}
