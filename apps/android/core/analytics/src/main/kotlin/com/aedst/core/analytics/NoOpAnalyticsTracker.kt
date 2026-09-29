package com.aedst.core.analytics

/** Used in debug builds and `core/testing` fixtures so no event/trace ever leaves the device outside release builds. */
class NoOpAnalyticsTracker : AnalyticsTracker {
    override fun logEvent(name: String, params: Map<String, Any?>) = Unit
    override fun setUserId(userId: String?) = Unit
    override fun recordException(throwable: Throwable) = Unit
    override fun startTrace(name: String): PerformanceTrace = object : PerformanceTrace {
        override fun putAttribute(key: String, value: String) = Unit
        override fun stop() = Unit
    }
}
