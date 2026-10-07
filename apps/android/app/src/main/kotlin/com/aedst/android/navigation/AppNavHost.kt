package com.aedst.android.navigation

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AdminPanelSettings
import androidx.compose.material.icons.filled.Checklist
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.PhotoLibrary
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.aedst.android.R
import com.aedst.core.analytics.NavigationTelemetry
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.admin.navigation.AdminNavGraph
import com.aedst.feature.auth.navigation.AuthNavGraph
import com.aedst.feature.media.navigation.MediaNavGraph
import com.aedst.feature.notifications.navigation.NotificationPermissionRequest
import com.aedst.feature.notifications.navigation.NotificationsBadgedIcon
import com.aedst.feature.notifications.navigation.NotificationsNavGraph
import com.aedst.feature.search.navigation.SearchNavGraph
import com.aedst.feature.tasks.navigation.TasksNavGraph

/**
 * Composes every feature's [AedstNavGraph] into one host. `app/` only knows
 * about each feature's `navigation` entry point — never its domain/data/
 * presentation packages — the same boundary a server module's `api/`
 * package enforces (see core/navigation and docs/architecture-android.md).
 */
private val featureGraphs: List<AedstNavGraph> = listOf(
    AuthNavGraph(),
    TasksNavGraph(),
    SearchNavGraph(),
    NotificationsNavGraph(),
    MediaNavGraph(),
    AdminNavGraph(),
)

private data class TopLevelDestination(
    val route: String,
    @StringRes val label: Int,
    val icon: @Composable () -> Unit,
)

private fun vectorIcon(image: ImageVector): @Composable () -> Unit = { Icon(image, contentDescription = null) }

/**
 * @param isSignedIn `null` while the session is still being restored (nothing is drawn; the splash screen covers it).
 *   The session is the single source of truth for which side of the auth boundary the user is on, so a refresh token
 *   rejected in the background sends the user back to sign-in from any screen.
 * @param isAdmin shows the admin tab (UI gating only; the server authorises every admin RPC).
 * @param pendingRoute a validated route from a tapped notification, opened once the user is signed in.
 */
@Composable
fun AppNavHost(
    isSignedIn: Boolean?,
    isAdmin: Boolean,
    pendingRoute: String?,
    onPendingRouteHandled: () -> Unit,
    navigationTelemetry: NavigationTelemetry,
    navController: NavHostController = rememberNavController(),
) {
    if (isSignedIn == null) return

    // Fixed at first composition: later session changes are handled by navigation below, not by re-creating the graph.
    val startDestination = remember { if (isSignedIn) AuthNavGraph.HOME_ROUTE else AuthNavGraph.ROUTE }
    val currentRoute = navController.currentBackStackEntryAsState().value?.destination?.route

    DisposableEffect(navController) {
        val detach = navigationTelemetry.attach(navController)
        onDispose { detach() }
    }

    LaunchedEffect(isSignedIn, currentRoute) {
        if (!isSignedIn && currentRoute != null && currentRoute != AuthNavGraph.ROUTE) {
            navController.navigate(AuthNavGraph.ROUTE) {
                popUpTo(navController.graph.id) { inclusive = true }
                launchSingleTop = true
            }
        }
    }

    LaunchedEffect(isSignedIn, pendingRoute, currentRoute) {
        if (isSignedIn && pendingRoute != null && currentRoute != null && currentRoute != AuthNavGraph.ROUTE) {
            navController.navigate(pendingRoute) { launchSingleTop = true }
            onPendingRouteHandled()
        }
    }

    if (isSignedIn) NotificationPermissionRequest()

    val destinations = topLevelDestinations(isAdmin)
    val showBottomBar = isSignedIn && destinations.any { it.route == currentRoute }

    Scaffold(
        bottomBar = {
            if (showBottomBar) BottomBar(destinations, navController)
        },
    ) { padding ->
        NavHost(
            navController = navController,
            startDestination = startDestination,
            // Screens apply their own safe-drawing insets; consuming the Scaffold's padding avoids doubling them.
            modifier = Modifier.padding(padding).consumeWindowInsets(padding),
        ) {
            featureGraphs.forEach { graph -> graph.register(this, navController) }
        }
    }
}

private fun topLevelDestinations(isAdmin: Boolean): List<TopLevelDestination> = listOfNotNull(
    TopLevelDestination(AedstRoutes.TASKS, R.string.nav_tasks, vectorIcon(Icons.Filled.Checklist)),
    TopLevelDestination(AedstRoutes.SEARCH, R.string.nav_search, vectorIcon(Icons.Filled.Search)),
    TopLevelDestination(AedstRoutes.NOTIFICATIONS, R.string.nav_notifications) { NotificationsBadgedIcon() },
    TopLevelDestination(AedstRoutes.MEDIA, R.string.nav_media, vectorIcon(Icons.Filled.PhotoLibrary)),
    TopLevelDestination(AedstRoutes.AUTH_ACCOUNT, R.string.nav_account, vectorIcon(Icons.Filled.Person)),
    if (isAdmin) {
        TopLevelDestination(AedstRoutes.ADMIN, R.string.nav_admin, vectorIcon(Icons.Filled.AdminPanelSettings))
    } else {
        null
    },
)

@Composable
private fun BottomBar(destinations: List<TopLevelDestination>, navController: NavHostController) {
    val currentDestination = navController.currentBackStackEntryAsState().value?.destination
    NavigationBar {
        destinations.forEach { destination ->
            val selected = currentDestination?.hierarchy?.any { it.route == destination.route } == true
            NavigationBarItem(
                selected = selected,
                onClick = {
                    navController.navigate(destination.route) {
                        // Standard top-level navigation: one copy per tab, state saved and restored per tab.
                        popUpTo(navController.graph.findStartDestination().id) { saveState = true }
                        launchSingleTop = true
                        restoreState = true
                    }
                },
                icon = destination.icon,
                label = { Text(stringResource(destination.label)) },
            )
        }
    }
}
