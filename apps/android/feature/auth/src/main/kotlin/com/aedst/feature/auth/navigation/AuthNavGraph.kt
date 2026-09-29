package com.aedst.feature.auth.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.core.navigation.Route
import com.aedst.feature.auth.presentation.login.LoginRoute

/**
 * feature/auth's public boundary — the only package `app/` (or any other
 * feature) may import from this module. Nothing in `domain/`, `data/` or
 * `presentation/` above is reachable from outside this package; see
 * docs/architecture-android.md.
 *
 * TODO: the standalone users/profile business modules were removed and
 * there is no post-login destination yet (a future tasks module is
 * expected to provide one). Until then, `onLoggedIn` is a no-op and this
 * graph stays on [AuthRoute].
 */
object AuthRoute : Route {
    override val path: String = AedstRoutes.AUTH_LOGIN
}

class AuthNavGraph : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AuthRoute.path) {
            LoginRoute(
                onLoggedIn = {
                    // TODO: navigate to the tasks landing page once it exists.
                },
            )
        }
    }

    companion object {
        const val ROUTE: String = AedstRoutes.AUTH_LOGIN
    }
}
