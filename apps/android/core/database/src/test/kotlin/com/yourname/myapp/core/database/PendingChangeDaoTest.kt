package com.yourname.myapp.core.database

import androidx.room3.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.test.runTest
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import kotlin.test.assertEquals
import kotlin.test.assertTrue

@RunWith(RobolectricTestRunner::class)
internal class PendingChangeDaoTest {
    private lateinit var database: AppDatabase
    private lateinit var dao: PendingChangeDao

    @Before
    fun createDatabase() {
        database =
            Room
                .inMemoryDatabaseBuilder(ApplicationProvider.getApplicationContext(), AppDatabase::class.java)
                .allowMainThreadQueries()
                .build()
        dao = database.pendingChangeDao()
    }

    @After
    fun closeDatabase() {
        database.close()
    }

    @Test
    fun `insert then observeAll returns the inserted row`() =
        runTest {
            val change =
                PendingChangeEntity(
                    entityType = "article",
                    entityId = "42",
                    operation = "bookmark",
                    payloadJson = "{}",
                    createdAtEpochMillis = 1_000L,
                )

            dao.insert(change)

            val all = dao.getAll()
            assertEquals(1, all.size)
            assertEquals("42", all.first().entityId)
        }

    @Test
    fun `delete removes exactly the deleted row`() =
        runTest {
            val first =
                PendingChangeEntity(
                    id = 1,
                    entityType = "article",
                    entityId = "1",
                    operation = "bookmark",
                    payloadJson = "{}",
                    createdAtEpochMillis = 1_000L,
                )
            val second = first.copy(id = 2, entityId = "2")
            dao.insert(first)
            dao.insert(second)

            dao.delete(dao.getAll().first { it.entityId == "1" })

            val remaining = dao.getAll()
            assertEquals(1, remaining.size)
            assertTrue(remaining.none { it.entityId == "1" })
        }
}
