package com.yourname.myapp.core.analytics

import org.junit.Test

internal class NoOpAnalyticsTest {
    @Test
    fun `logEvent never throws, regardless of params`() {
        val analytics: Analytics = NoOpAnalytics()

        analytics.logEvent("screen_view")
        analytics.logEvent("purchase", mapOf("price" to 9.99, "currency" to "USD"))
    }
}
