package com.aedst.core.navigation

/**
 * Well-known top-level route paths every feature can navigate to without
 * depending on another feature's Gradle module — the navigation-contract
 * equivalent of a server module publishing a stable RPC method name instead
 * of another module importing its handler package directly.
 */
object AedstRoutes {
    const val AUTH_LOGIN = "auth/login"
}
