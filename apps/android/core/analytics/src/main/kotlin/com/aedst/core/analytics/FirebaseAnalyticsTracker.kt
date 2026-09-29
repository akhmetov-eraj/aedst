package com.aedst.core.analytics

import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.crashlytics.FirebaseCrashlytics
import com.google.firebase.perf.FirebasePerformance
import javax.inject.Inject

class FirebaseAnalyticsTracker @Inject constructor(
    private val firebaseAnalytics: FirebaseAnalytics,
    private val crashlytics: FirebaseCrashlytics,
    private val performance: FirebasePerformance,
) : AnalyticsTracker {

    override fun logEvent(name: String, params: Map<String, Any?>) {
        val bundle = android.os.Bundle()
        params.forEach { (key, value) ->
            when (value) {
                is String -> bundle.putString(key, value)
                is Int -> bundle.putInt(key, value)
                is Long -> bundle.putLong(key, value)
                is Double -> bundle.putDouble(key, value)
                is Boolean -> bundle.putBoolean(key, value)
                else -> Unit
            }
        }
        firebaseAnalytics.logEvent(name, bundle)
    }

    override fun setUserId(userId: String?) {
        firebaseAnalytics.setUserId(userId)
        crashlytics.setUserId(userId ?: "")
    }

    override fun recordException(throwable: Throwable) {
        crashlytics.recordException(throwable)
    }

    override fun startTrace(name: String): PerformanceTrace {
        val trace = performance.newTrace(name)
        trace.start()
        return object : PerformanceTrace {
            override fun putAttribute(key: String, value: String) {
                trace.putAttribute(key, value)
            }

            override fun stop() {
                trace.stop()
            }
        }
    }
}
