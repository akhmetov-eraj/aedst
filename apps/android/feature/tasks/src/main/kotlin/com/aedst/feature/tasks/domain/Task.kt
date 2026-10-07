package com.aedst.feature.tasks.domain

import com.aedst.core.common.Outcome
import java.time.Instant

enum class TaskStatus { TODO, IN_PROGRESS, DONE }

enum class TaskPriority { LOW, MEDIUM, HIGH, URGENT }

/** Domain entity — no generated proto types leak above `data/`, mirrors `modules/tasks/domain` on the server. */
data class Task(
    val id: String,
    val title: String,
    val description: String,
    val status: TaskStatus,
    val priority: TaskPriority,
    val dueDate: Instant?,
    val createdAt: Instant?,
    val completedAt: Instant?,
    val version: Int,
) {
    val isDone: Boolean get() = status == TaskStatus.DONE
}

data class TaskPage(val tasks: List<Task>, val nextPageToken: String?, val totalCount: Int)

data class NewTask(val title: String, val description: String, val priority: TaskPriority, val dueDate: Instant?)

/** The caller's own tasks (TasksService). `user_id` is never sent: the server derives it from the token. */
interface TaskRepository {
    suspend fun list(pageToken: String? = null): Outcome<TaskPage>

    suspend fun get(id: String): Outcome<Task>

    suspend fun create(task: NewTask): Outcome<Task>

    suspend fun complete(id: String): Outcome<Task>

    suspend fun reopen(id: String): Outcome<Task>

    suspend fun delete(id: String): Outcome<Unit>
}
