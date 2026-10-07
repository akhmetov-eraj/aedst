package com.aedst.core.analytics

import androidx.navigation.NavController
import io.sentry.android.navigation.SentryNavigationListener
import javax.inject.Inject

/**
 * Navigation → telemetry: Sentry breadcrumbs/transactions ([SentryNavigationListener]) and PostHog screen events.
 * Both use the route *template* (`tasks/{taskId}`), so no ids leak into analytics.
 */
class NavigationTelemetry @Inject constructor(private val analytics: AnalyticsTracker) {

    /** Attaches the listeners; call the returned function to detach (e.g. from `DisposableEffect.onDispose`). */
    fun attach(navController: NavController): () -> Unit {
        val sentryListener = SentryNavigationListener()
        val screenListener = NavController.OnDestinationChangedListener { _, destination, _ ->
            destination.route?.let(analytics::screen)
        }
        navController.addOnDestinationChangedListener(sentryListener)
        navController.addOnDestinationChangedListener(screenListener)
        return {
            navController.removeOnDestinationChangedListener(sentryListener)
            navController.removeOnDestinationChangedListener(screenListener)
        }
    }
}
