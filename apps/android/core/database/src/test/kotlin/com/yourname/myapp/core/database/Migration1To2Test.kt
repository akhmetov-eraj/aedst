package com.yourname.myapp.core.database

import androidx.sqlite.SQLiteConnection
import androidx.sqlite.driver.AndroidSQLiteDriver
import kotlinx.coroutines.test.runTest
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Exercises MIGRATION_1_2 directly against a raw connection opened at the v1
 * schema, rather than through Room's `MigrationTestHelper` (which additionally
 * needs the exported schema JSON wired as a test asset and an Instrumentation
 * instance — real complexity not worth taking on for a single-table addition;
 * this still asserts pre-migration data survives and the new table is usable,
 * which is the actual thing the room-migration skill's DoD cares about).
 */
@RunWith(RobolectricTestRunner::class)
internal class Migration1To2Test {
    private fun SQLiteConnection.exec(sql: String) {
        prepare(sql).use { it.step() }
    }

    @Test
    fun `pending_changes rows survive migration and user_profile becomes usable`() =
        runTest {
            val connection = AndroidSQLiteDriver().open(":memory:")

            connection.exec(
                """
                CREATE TABLE `pending_changes` (
                    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
                    `entityType` TEXT NOT NULL,
                    `entityId` TEXT NOT NULL,
                    `operation` TEXT NOT NULL,
                    `payloadJson` TEXT NOT NULL,
                    `createdAtEpochMillis` INTEGER NOT NULL
                )
                """.trimIndent(),
            )
            connection.exec(
                "INSERT INTO pending_changes (entityType, entityId, operation, payloadJson, createdAtEpochMillis) " +
                    "VALUES ('article', '1', 'bookmark', '{}', 1000)",
            )

            MIGRATION_1_2.migrate(connection)

            connection.prepare("SELECT COUNT(*) FROM pending_changes").use { statement ->
                assertTrue(statement.step())
                assertEquals(1L, statement.getLong(0))
            }

            connection.exec(
                "INSERT INTO user_profile (id, displayName, email, avatarUrl) VALUES ('u1', 'Ada', 'ada@example.invalid', NULL)",
            )
            connection.prepare("SELECT displayName FROM user_profile WHERE id = 'u1'").use { statement ->
                assertTrue(statement.step())
                assertEquals("Ada", statement.getText(0))
            }

            connection.close()
        }
}
