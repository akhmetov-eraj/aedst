package com.aedst.feature.media.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.composable
import androidx.navigation.navArgument
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.media.presentation.detail.MediaDetailRoute
import com.aedst.feature.media.presentation.library.MediaLibraryRoute

/** feature/media's public boundary: the caller's media library and the image/video viewer (`media/{mediaId}`). */
class MediaNavGraph : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AedstRoutes.MEDIA) {
            MediaLibraryRoute(onOpenMedia = { id -> navController.navigate(AedstRoutes.mediaDetail(id)) })
        }
        builder.composable(
            route = AedstRoutes.MEDIA_DETAIL,
            arguments = listOf(navArgument(AedstRoutes.MEDIA_ID_ARG) { type = NavType.StringType }),
        ) {
            MediaDetailRoute(onBack = { navController.popBackStack() })
        }
    }
}
