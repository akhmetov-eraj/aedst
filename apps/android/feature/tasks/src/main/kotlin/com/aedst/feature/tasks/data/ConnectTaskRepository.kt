package com.aedst.feature.tasks.data

import com.aedst.core.common.Outcome
import com.aedst.core.common.runCatchingOutcome
import com.aedst.core.network.rpc
import com.aedst.core.network.toInstantOrNull
import com.aedst.core.network.toTimestamp
import com.aedst.feature.tasks.domain.NewTask
import com.aedst.feature.tasks.domain.Task
import com.aedst.feature.tasks.domain.TaskPage
import com.aedst.feature.tasks.domain.TaskPriority
import com.aedst.feature.tasks.domain.TaskRepository
import com.aedst.feature.tasks.domain.TaskStatus
import com.aedst.tasks.v1.TasksServiceClientInterface
import com.aedst.tasks.v1.completeTaskRequest
import com.aedst.tasks.v1.createTaskRequest
import com.aedst.tasks.v1.deleteTaskRequest
import com.aedst.tasks.v1.getTaskRequest
import com.aedst.tasks.v1.listTasksRequest
import com.aedst.tasks.v1.reopenTaskRequest
import javax.inject.Inject
import com.aedst.tasks.v1.Task as ProtoTask
import com.aedst.tasks.v1.TaskPriority as ProtoTaskPriority
import com.aedst.tasks.v1.TaskStatus as ProtoTaskStatus

class ConnectTaskRepository @Inject constructor(private val client: TasksServiceClientInterface) : TaskRepository {

    override suspend fun list(pageToken: String?): Outcome<TaskPage> = runCatchingOutcome {
        val response = rpc {
            client.listTasks(
                listTasksRequest {
                    pageSize = PAGE_SIZE
                    pageToken?.let { this.pageToken = it }
                },
            )
        }
        TaskPage(
            tasks = response.tasksList.map { it.toDomain() },
            nextPageToken = response.nextPageToken.takeIf(String::isNotEmpty),
            totalCount = response.totalCount,
        )
    }

    override suspend fun get(id: String): Outcome<Task> = runCatchingOutcome {
        rpc { client.getTask(getTaskRequest { this.id = id }) }.task.toDomain()
    }

    override suspend fun create(task: NewTask): Outcome<Task> = runCatchingOutcome {
        rpc {
            client.createTask(
                createTaskRequest {
                    title = task.title.trim()
                    description = task.description.trim()
                    priority = task.priority.toProto()
                    task.dueDate?.let { dueDate = it.toTimestamp() }
                },
            )
        }.task.toDomain()
    }

    override suspend fun complete(id: String): Outcome<Task> = runCatchingOutcome {
        rpc { client.completeTask(completeTaskRequest { this.id = id }) }.task.toDomain()
    }

    override suspend fun reopen(id: String): Outcome<Task> = runCatchingOutcome {
        rpc { client.reopenTask(reopenTaskRequest { this.id = id }) }.task.toDomain()
    }

    override suspend fun delete(id: String): Outcome<Unit> = runCatchingOutcome {
        rpc { client.deleteTask(deleteTaskRequest { this.id = id }) }
        Unit
    }

    private companion object {
        const val PAGE_SIZE = 30
    }
}

internal fun ProtoTask.toDomain(): Task = Task(
    id = id,
    title = title,
    description = description,
    status = when (status) {
        ProtoTaskStatus.TASK_STATUS_IN_PROGRESS -> TaskStatus.IN_PROGRESS
        ProtoTaskStatus.TASK_STATUS_DONE -> TaskStatus.DONE
        else -> TaskStatus.TODO
    },
    priority = when (priority) {
        ProtoTaskPriority.TASK_PRIORITY_LOW -> TaskPriority.LOW
        ProtoTaskPriority.TASK_PRIORITY_HIGH -> TaskPriority.HIGH
        ProtoTaskPriority.TASK_PRIORITY_URGENT -> TaskPriority.URGENT
        else -> TaskPriority.MEDIUM
    },
    dueDate = if (hasDueDate()) dueDate.toInstantOrNull() else null,
    createdAt = if (hasCreatedAt()) createdAt.toInstantOrNull() else null,
    completedAt = if (hasCompletedAt()) completedAt.toInstantOrNull() else null,
    version = version,
)

internal fun TaskPriority.toProto(): ProtoTaskPriority = when (this) {
    TaskPriority.LOW -> ProtoTaskPriority.TASK_PRIORITY_LOW
    TaskPriority.MEDIUM -> ProtoTaskPriority.TASK_PRIORITY_MEDIUM
    TaskPriority.HIGH -> ProtoTaskPriority.TASK_PRIORITY_HIGH
    TaskPriority.URGENT -> ProtoTaskPriority.TASK_PRIORITY_URGENT
}
