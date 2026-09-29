package com.aedst.core.database

import androidx.room.Database
import androidx.room.RoomDatabase

/**
 * Single Room database for the app. Each feature owns its own table(s) and
 * DAO — declared here because Room requires one `@Database` per file, but
 * a feature never queries another feature's table directly (same rule as
 * "no cross-schema SQL" on the server side); cross-feature reads go through
 * that feature's repository/`api`-equivalent instead.
 */
@Database(
    entities = [],
    version = 1,
    exportSchema = true,
)
abstract class AppDatabase : RoomDatabase()
