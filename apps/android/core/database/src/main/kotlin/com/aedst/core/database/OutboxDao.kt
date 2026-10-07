package com.aedst.core.database

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

/** Enum values are stored by name, so the state literals in the queries below must match [OutboxState]. */
@Dao
interface OutboxDao {
    /** Ignores a duplicate [OutboxEntity.id] (returns -1), so enqueueing one logical operation twice is harmless. */
    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insert(entity: OutboxEntity): Long

    /** Oldest first: operations are delivered in the order the user performed them. */
    @Query("SELECT * FROM outbox WHERE state = 'PENDING' ORDER BY createdAtEpochMs ASC, id ASC LIMIT :limit")
    suspend fun pending(limit: Int): List<OutboxEntity>

    @Query("SELECT * FROM outbox WHERE state = 'FAILED' ORDER BY createdAtEpochMs ASC, id ASC LIMIT :limit")
    suspend fun failed(limit: Int): List<OutboxEntity>

    @Query("SELECT COUNT(*) FROM outbox WHERE state = 'PENDING'")
    fun observePendingCount(): Flow<Int>

    @Query("DELETE FROM outbox WHERE id = :id")
    suspend fun delete(id: String)

    @Query("UPDATE outbox SET attempts = attempts + 1, lastError = :lastError WHERE id = :id")
    suspend fun recordAttempt(id: String, lastError: String?)

    @Query("UPDATE outbox SET state = 'FAILED', attempts = attempts + 1, lastError = :lastError WHERE id = :id")
    suspend fun markFailed(id: String, lastError: String?)

    /** Housekeeping: drop dead entries older than [olderThanEpochMs] so the table cannot grow without bound. */
    @Query("DELETE FROM outbox WHERE state = 'FAILED' AND createdAtEpochMs < :olderThanEpochMs")
    suspend fun deleteFailedOlderThan(olderThanEpochMs: Long): Int
}
