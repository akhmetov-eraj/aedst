package com.yourname.myapp.core.database

import androidx.sqlite.SQLiteConnection
import androidx.sqlite.driver.AndroidSQLiteDriver
import kotlinx.coroutines.test.runTest
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import kotlin.test.assertEquals
import kotlin.test.assertTrue

@RunWith(RobolectricTestRunner::class)
internal class Migration2To3Test {
    private fun SQLiteConnection.exec(sql: String) {
        prepare(sql).use { it.step() }
    }

    @Test
    fun `user_profile rows survive migration and articles becomes usable`() =
        runTest {
            val connection = AndroidSQLiteDriver().open(":memory:")

            connection.exec(
                """
                CREATE TABLE `user_profile` (
                    `id` TEXT NOT NULL,
                    `displayName` TEXT NOT NULL,
                    `email` TEXT NOT NULL,
                    `avatarUrl` TEXT,
                    PRIMARY KEY(`id`)
                )
                """.trimIndent(),
            )
            connection.exec(
                "INSERT INTO user_profile (id, displayName, email, avatarUrl) VALUES ('u1', 'Ada', 'ada@example.invalid', NULL)",
            )

            MIGRATION_2_3.migrate(connection)

            connection.prepare("SELECT COUNT(*) FROM user_profile").use { statement ->
                assertTrue(statement.step())
                assertEquals(1L, statement.getLong(0))
            }

            connection.exec(
                "INSERT INTO articles (id, title, summary, imageUrl, publishedAtEpochMillis, isBookmarked, page) " +
                    "VALUES ('a1', 'Title', 'Summary', NULL, 1000, 0, 0)",
            )
            connection.prepare("SELECT title FROM articles WHERE id = 'a1'").use { statement ->
                assertTrue(statement.step())
                assertEquals("Title", statement.getText(0))
            }

            connection.close()
        }
}
