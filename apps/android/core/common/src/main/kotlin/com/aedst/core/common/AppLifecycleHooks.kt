package com.aedst.core.common

import dagger.BindsOptionalOf
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import dagger.multibindings.Multibinds

/**
 * Process-start work a module needs (notification channels, telemetry SDK init, push token sync). Modules contribute
 * one with `@Binds @IntoSet`; `app/`'s `Application.onCreate` runs them all. This keeps `app/` from importing any
 * feature's data/domain packages just to start it.
 */
interface AppInitializer {
    /** Lower runs first. Telemetry uses [PRIORITY_TELEMETRY] so crashes in later initializers are captured. */
    val priority: Int get() = PRIORITY_DEFAULT

    /** Called on the main thread from `Application.onCreate`: keep it cheap, launch real work on a scope. */
    fun initialize()

    companion object {
        const val PRIORITY_TELEMETRY = 0
        const val PRIORITY_DEFAULT = 100
    }
}

/**
 * Work that must happen while the session is still valid, right before an explicit sign-out clears it
 * (e.g. `UnregisterPushDevice`, which needs the bearer token). Called by `feature/auth`'s logout; each hook is
 * best-effort and must not throw for expected failures. Not called when the server rejects the refresh token —
 * there is no valid session left to call with.
 */
fun interface SignOutHook {
    suspend fun beforeSignOut()
}

/**
 * Non-fatal error sink (implemented by `core/analytics` with Sentry). Optional in the graph so modules and tests that
 * do not include telemetry still build.
 */
fun interface ErrorReporter {
    fun recordNonFatal(throwable: Throwable)
}

@Module
@InstallIn(SingletonComponent::class)
abstract class AppLifecycleHooksModule {
    @Multibinds
    abstract fun appInitializers(): Set<AppInitializer>

    @Multibinds
    abstract fun signOutHooks(): Set<SignOutHook>

    @BindsOptionalOf
    abstract fun errorReporter(): ErrorReporter
}
