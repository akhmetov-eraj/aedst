package com.aedst.core.database

import androidx.room.Database
import androidx.room.RoomDatabase

/**
 * Single Room database for the app. Each feature owns its own table(s) and
 * DAO — declared here because Room requires one `@Database` per file, but
 * a feature never queries another feature's table directly (same rule as
 * "no cross-schema SQL" on the server side); cross-feature reads go through
 * that feature's repository/`api`-equivalent instead.
 *
 * Schema changes: bump [version], add an `AutoMigration`/`Migration`, and commit the new
 * `schemas/<version>.json`. Destructive fallback is intentionally not configured — losing queued outbox items would
 * silently drop user actions. The database is **not encrypted**: store only non-sensitive, re-fetchable data here.
 */
@Database(
    entities = [OutboxEntity::class],
    version = 1,
    exportSchema = true,
)
abstract class AppDatabase : RoomDatabase() {
    abstract fun outboxDao(): OutboxDao
}
