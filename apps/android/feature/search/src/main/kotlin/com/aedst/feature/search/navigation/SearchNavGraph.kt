package com.aedst.feature.search.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.search.presentation.SearchRoute

/** feature/search's public boundary. A hit's server link (`/tasks/<id>`) is resolved to a route by `AedstRoutes`. */
class SearchNavGraph : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AedstRoutes.SEARCH) {
            SearchRoute(
                onOpenLink = { link -> AedstRoutes.routeForLink(link)?.let { route -> navController.navigate(route) } },
            )
        }
    }
}
