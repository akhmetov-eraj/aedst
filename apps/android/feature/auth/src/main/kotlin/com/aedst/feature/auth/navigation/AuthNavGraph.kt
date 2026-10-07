package com.aedst.feature.auth.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.core.navigation.Route
import com.aedst.feature.auth.presentation.account.AccountRoute
import com.aedst.feature.auth.presentation.login.LoginRoute

/**
 * feature/auth's public boundary — the only package `app/` (or any other
 * feature) may import from this module. Nothing in `domain/`, `data/` or
 * `presentation/` above is reachable from outside this package; see
 * docs/architecture-android.md.
 */
object AuthRoute : Route {
    override val path: String = AedstRoutes.AUTH_LOGIN
}

object AccountDestination : Route {
    override val path: String = AedstRoutes.AUTH_ACCOUNT
}

/**
 * @param homeRoute where a successful sign-in lands. The phone app uses the tasks list ([AedstRoutes.HOME]); the TV
 *   app, which does not register the tasks graph yet, uses the account screen.
 */
class AuthNavGraph(private val homeRoute: String = AedstRoutes.HOME) : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AuthRoute.path) {
            LoginRoute(
                onLoggedIn = {
                    navController.navigate(homeRoute) {
                        popUpTo(AuthRoute.path) { inclusive = true }
                        launchSingleTop = true
                    }
                },
            )
        }
        // Signing out needs no navigation callback: it clears the session and `app/` reacts to SessionState.
        builder.composable(AccountDestination.path) { AccountRoute() }
    }

    companion object {
        const val ROUTE: String = AedstRoutes.AUTH_LOGIN
        const val HOME_ROUTE: String = AedstRoutes.HOME
    }
}
