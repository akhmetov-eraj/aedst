package com.aedst.feature.admin.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.admin.presentation.AdminRoute

/**
 * feature/admin's public boundary. `app/` only shows the entry point to users whose token carries the `admin` role,
 * and the screen re-checks it; the server enforces the role on every AdminService RPC regardless.
 */
class AdminNavGraph : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AedstRoutes.ADMIN) { AdminRoute() }
    }
}
