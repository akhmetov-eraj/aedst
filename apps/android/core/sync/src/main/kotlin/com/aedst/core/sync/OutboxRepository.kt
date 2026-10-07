package com.aedst.core.sync

import com.aedst.core.database.OutboxDao
import com.aedst.core.database.OutboxEntity
import kotlinx.coroutines.flow.Flow
import java.time.Clock
import java.util.UUID
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Front door of the offline write path: persist the operation locally, then ask [OutboxScheduler] to deliver it
 * once the device is online. Callers update their own local state optimistically and rely on this for delivery.
 */
@Singleton
class OutboxRepository @Inject constructor(
    private val dao: OutboxDao,
    private val scheduler: OutboxScheduler,
    private val clock: Clock,
) {
    /** Number of operations still waiting for delivery (drive a "syncing…" indicator with it). */
    val pendingCount: Flow<Int> = dao.observePendingCount()

    /**
     * Queues [payload] for the handler registered for [type] and returns the operation id (also its idempotency key).
     * Pass a stable [id] when the same user intent could be enqueued twice (e.g. a double tap): the second insert is
     * ignored.
     */
    suspend fun enqueue(type: String, payload: String, id: String = UUID.randomUUID().toString()): String {
        dao.insert(OutboxEntity(id = id, type = type, payload = payload, createdAtEpochMs = clock.millis()))
        scheduler.scheduleSync()
        return id
    }
}
