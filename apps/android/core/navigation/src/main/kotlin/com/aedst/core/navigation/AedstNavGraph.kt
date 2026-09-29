package com.aedst.core.navigation

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController

/**
 * The boundary every `feature` module exposes to `app/` — the Android
 * analog of a server module's `api/` package (see
 * docs/decisions/0006-module-boundaries.md and
 * docs/architecture-android.md). `app/` only ever imports a feature's
 * `navigation` package; a feature's `domain`, `data` and `presentation`
 * packages are never referenced from outside the feature.
 */
interface AedstNavGraph {
    /** Registers this feature's destinations on the shared [NavGraphBuilder]. */
    fun register(builder: NavGraphBuilder, navController: NavHostController)
}
