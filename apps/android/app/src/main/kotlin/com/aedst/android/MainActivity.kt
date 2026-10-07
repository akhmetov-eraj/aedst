package com.aedst.android

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.android.navigation.AppNavHost
import com.aedst.core.analytics.NavigationTelemetry
import com.aedst.core.navigation.AedstRoutes
import com.aedst.core.ui.components.AedstOfflineBanner
import com.aedst.core.ui.theme.AedstTheme
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()

    @Inject
    lateinit var navigationTelemetry: NavigationTelemetry

    override fun onCreate(savedInstanceState: Bundle?) {
        // Must run before super.onCreate. Hold the splash until the persisted session is known, so a signed-in user
        // never sees a flash of the login screen.
        installSplashScreen().setKeepOnScreenCondition { viewModel.isSignedIn.value == null }
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        if (savedInstanceState == null) handleDeepLink(intent)

        setContent {
            val isSignedIn by viewModel.isSignedIn.collectAsStateWithLifecycle()
            val isOffline by viewModel.isOffline.collectAsStateWithLifecycle()
            val isAdmin by viewModel.isAdmin.collectAsStateWithLifecycle()
            val pendingRoute by viewModel.pendingRoute.collectAsStateWithLifecycle()

            AedstTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    Column(modifier = Modifier.fillMaxSize()) {
                        AedstOfflineBanner(isOffline = isOffline)
                        AppNavHost(
                            isSignedIn = isSignedIn,
                            isAdmin = isAdmin,
                            pendingRoute = pendingRoute,
                            onPendingRouteHandled = viewModel::onDeepLinkHandled,
                            navigationTelemetry = navigationTelemetry,
                        )
                    }
                }
            }
        }
    }

    /** A notification tap while the activity exists (launchMode singleTop via the PendingIntent flags). */
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleDeepLink(intent)
    }

    /** Only the `link` extra is read, and only routes `AedstRoutes.routeForLink` accepts are honoured. */
    private fun handleDeepLink(intent: Intent?) {
        viewModel.onDeepLink(intent?.getStringExtra(AedstRoutes.EXTRA_LINK))
    }
}
