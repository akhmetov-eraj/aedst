package com.aedst.core.navigation

/** Marker for a feature's top-level route constants, kept out of `app/` to avoid stringly-typed coupling. */
interface Route {
    val path: String
}
