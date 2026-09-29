package com.aedst.android.tv.navigation

import androidx.compose.runtime.Composable
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.rememberNavController
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.feature.auth.navigation.AuthNavGraph

/**
 * TV counterpart to `:app`'s `AppNavHost` — same composition pattern (each
 * feature's [AedstNavGraph] registered into one host), reusing the exact
 * same `:feature:auth` nav graph and business logic. `app-tv/` only knows
 * about each feature's `navigation` entry point, never its domain/data/
 * presentation packages, same boundary rule as the phone app (see
 * `apps/android/app/.../navigation/AppNavHost.kt` and
 * docs/architecture-android.md).
 */
private val featureGraphs: List<AedstNavGraph> = listOf(
    AuthNavGraph(),
)

@Composable
fun AppTvNavHost(navController: NavHostController = rememberNavController()) {
    NavHost(navController = navController, startDestination = AuthNavGraph.ROUTE) {
        featureGraphs.forEach { graph -> graph.register(this, navController) }
    }
}
