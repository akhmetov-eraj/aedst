package com.aedst.feature.tasks.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.composable
import androidx.navigation.navArgument
import com.aedst.core.navigation.AedstNavGraph
import com.aedst.core.navigation.AedstRoutes
import com.aedst.feature.tasks.presentation.create.TaskCreateRoute
import com.aedst.feature.tasks.presentation.detail.TaskDetailRoute
import com.aedst.feature.tasks.presentation.list.TaskListRoute

/**
 * feature/tasks' public boundary. Opening an attachment navigates to `media/{id}` by route string — feature/media
 * registers it — so this module never depends on feature/media.
 */
class TasksNavGraph : AedstNavGraph {
    override fun register(builder: NavGraphBuilder, navController: NavHostController) {
        builder.composable(AedstRoutes.TASKS) {
            TaskListRoute(
                onOpenTask = { id -> navController.navigate(AedstRoutes.taskDetail(id)) },
                onCreateTask = { navController.navigate(AedstRoutes.TASK_CREATE) },
            )
        }
        builder.composable(
            route = AedstRoutes.TASK_DETAIL,
            arguments = listOf(navArgument(AedstRoutes.TASK_ID_ARG) { type = NavType.StringType }),
        ) {
            TaskDetailRoute(
                onBack = { navController.popBackStack() },
                onOpenMedia = { id -> navController.navigate(AedstRoutes.mediaDetail(id)) },
            )
        }
        builder.composable(AedstRoutes.TASK_CREATE) {
            TaskCreateRoute(
                onBack = { navController.popBackStack() },
                onCreated = { id ->
                    navController.navigate(AedstRoutes.taskDetail(id)) {
                        popUpTo(AedstRoutes.TASK_CREATE) { inclusive = true }
                    }
                },
            )
        }
    }
}
