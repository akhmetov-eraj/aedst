package com.aedst.core.navigation

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AedstRoutesTest {

    @Test
    fun `relative in-app links map to routes`() {
        assertEquals("tasks/3f2a", AedstRoutes.routeForLink("/tasks/3f2a"))
        assertEquals("notifications", AedstRoutes.routeForLink("/notifications"))
        assertEquals("media/abc-1", AedstRoutes.routeForLink("/media/abc-1?x=1#y"))
    }

    @Test
    fun `absolute, protocol-relative and unknown links are rejected`() {
        assertNull(AedstRoutes.routeForLink("https://evil.example/tasks/1"))
        assertNull(AedstRoutes.routeForLink("//evil.example/tasks/1"))
        assertNull(AedstRoutes.routeForLink("/unknown/1"))
        assertNull(AedstRoutes.routeForLink("/tasks/../admin"))
        assertNull(AedstRoutes.routeForLink("/tasks/1/2"))
        assertNull(AedstRoutes.routeForLink(""))
        assertNull(AedstRoutes.routeForLink(null))
    }
}
