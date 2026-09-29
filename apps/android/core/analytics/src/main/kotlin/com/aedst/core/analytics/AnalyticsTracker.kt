package com.aedst.core.analytics

/**
 * Facade every feature depends on instead of the Firebase SDK directly, so
 * swapping providers (or running a no-op in debug/tests) never touches
 * feature code — the same reason `internal/platform/observability` wraps
 * slog/OTel on the server.
 */
interface AnalyticsTracker {
    fun logEvent(name: String, params: Map<String, Any?> = emptyMap())
    fun setUserId(userId: String?)
    fun recordException(throwable: Throwable)
    fun startTrace(name: String): PerformanceTrace
}

interface PerformanceTrace {
    fun putAttribute(key: String, value: String)
    fun stop()
}
