package com.aedst.android.navigation

import androidx.compose.runtime.Composable
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.rememberNavController
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.feature.auth.navigation.AuthNavGraph

/**
 * Composes every feature's [AedstNavGraph] into one host. `app/` only knows
 * about each feature's `navigation` entry point — never its domain/data/
 * presentation packages — the same boundary a server module's `api/`
 * package enforces (see core/navigation and docs/architecture-android.md).
 */
private val featureGraphs: List<AedstNavGraph> = listOf(
    AuthNavGraph(),
)

@Composable
fun AppNavHost(navController: NavHostController = rememberNavController()) {
    NavHost(navController = navController, startDestination = AuthNavGraph.ROUTE) {
        featureGraphs.forEach { graph -> graph.register(this, navController) }
    }
}
