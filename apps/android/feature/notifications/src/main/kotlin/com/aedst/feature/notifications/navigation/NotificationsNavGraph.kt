package com.aedst.feature.notifications.navigation

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.pluralStringResource
import androidx.core.content.ContextCompat
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.notifications.R
import com.aedst.feature.notifications.presentation.UnreadBadgeViewModel
import com.aedst.feature.notifications.presentation.inbox.InboxRoute
import com.aedst.feature.notifications.presentation.preferences.NotificationPreferencesRoute

/**
 * feature/notifications' public boundary: the inbox + preferences destinations, the unread badge icon for the app's
 * navigation bar, and the POST_NOTIFICATIONS request. Push registration starts by itself (an `AppInitializer`).
 */
class NotificationsNavGraph : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AedstRoutes.NOTIFICATIONS) {
            InboxRoute(
                onOpenLink = { link -> AedstRoutes.routeForLink(link)?.let { navController.navigate(it) } },
                onOpenPreferences = { navController.navigate(AedstRoutes.NOTIFICATION_PREFERENCES) },
            )
        }
        builder.composable(AedstRoutes.NOTIFICATION_PREFERENCES) {
            NotificationPreferencesRoute(onBack = { navController.popBackStack() })
        }
    }
}

/** Bell icon with the live unread count, for the app shell's bottom bar. */
@Composable
fun NotificationsBadgedIcon(viewModel: UnreadBadgeViewModel = hiltViewModel()) {
    val unread by viewModel.unread.collectAsStateWithLifecycle()
    val description = pluralStringResource(R.plurals.notifications_unread_count, unread, unread)
    BadgedBox(badge = { if (unread > 0) Badge { Text(badgeText(unread)) } }) {
        Icon(Icons.Filled.Notifications, contentDescription = description)
    }
}

private fun badgeText(unread: Int): String = if (unread > MAX_BADGE) "$MAX_BADGE+" else unread.toString()

private const val MAX_BADGE = 99

/**
 * Asks for POST_NOTIFICATIONS once per app session after sign-in (Android 13+; earlier versions grant it at
 * install). If the user declines, pushes are still registered and simply not shown; the inbox keeps working.
 */
@Composable
fun NotificationPermissionRequest() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
    val context = LocalContext.current
    var hasAsked by rememberSaveable { mutableStateOf(false) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { }
    LaunchedEffect(Unit) {
        val granted = ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED
        if (!granted && !hasAsked) {
            hasAsked = true
            launcher.launch(Manifest.permission.POST_NOTIFICATIONS)
        }
    }
}
