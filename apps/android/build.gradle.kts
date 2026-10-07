import com.github.benmanes.gradle.versions.updates.DependencyUpdatesTask
import nl.littlerobots.vcu.plugin.versionSelector

plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.android.test) apply false
    alias(libs.plugins.kotlin.jvm) apply false
    alias(libs.plugins.kotlin.serialization) apply false
    // Registers org.jetbrains.kotlin.plugin.compose's version for the whole
    // build so AndroidComposeConventionPlugin's `pluginManager.apply(id)` can
    // resolve it (see build-logic/convention's other `apply(id)`-without-a-
    // version calls, which all rely on their plugin being declared here too).
    alias(libs.plugins.kotlin.compose) apply false
    alias(libs.plugins.ksp) apply false
    alias(libs.plugins.hilt) apply false
    alias(libs.plugins.detekt) apply false
    alias(libs.plugins.ktlint) apply false
    alias(libs.plugins.google.services) apply false
    alias(libs.plugins.sentry) apply false
    alias(libs.plugins.baseline.profile) apply false

    // Dependency-update tooling, applied to the root project only (scripts/update-deps.sh):
    //   ./gradlew dependencyUpdates --no-configuration-cache   report (ben-manes)
    //   ./gradlew versionCatalogUpdate                         rewrite gradle/libs.versions.toml
    //   ./gradlew wrapper --gradle-version latest              bump the wrapper
    alias(libs.plugins.ben.manes.versions)
    alias(libs.plugins.version.catalog.update)
}

/**
 * Version policy: latest STABLE only (docs/content/library-first.md). Rejects alpha, beta, rc, cr, m (milestone),
 * preview, eap, dev and snapshot qualifiers, case-insensitively, e.g. `2.5.0-Beta1`, `1.14.0-alpha03`, `21.0-rc-1`,
 * `1.0.0-M2`, `1.12.0-RC`. Calendar versions such as `2026.09.00` are stable.
 */
val nonStableVersion = Regex(
    "(?i).*[.\\-](alpha|beta|rc|cr|m|preview|eap|dev|snapshot)([.\\-]?\\d+)?([.\\-].*)?$",
)

fun isNonStable(version: String): Boolean = nonStableVersion.matches(version)

tasks.withType<DependencyUpdatesTask>().configureEach {
    rejectVersionIf { isNonStable(candidate.version) }
}

versionCatalogUpdate {
    // The catalog is grouped by hand into commented sections; sorting would scatter them.
    sortByKey.set(false)
    versionSelector { !isNonStable(it.candidate.version) }
    keep {
        // Keep version entries referenced only from build-logic (e.g. protoc via protobuf-javalite) or scripts.
        keepUnusedVersions.set(true)
    }
    pin {
        // detekt 2.x needs an explicit decision (decisions.md: "detekt stays on 1.23.8 until 2.0 stable").
        versions.add("detekt")
    }
}
