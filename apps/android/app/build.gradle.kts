plugins {
    alias(libs.plugins.android.application.convention)
    alias(libs.plugins.android.compose.convention)
    alias(libs.plugins.android.hilt.convention)
    alias(libs.plugins.sentry)
    alias(libs.plugins.baseline.profile)
}

// FCM: the google-services plugin needs app/google-services.json (gitignored, per Firebase project). Apply it only
// when the file exists so the template builds without a Firebase project; at runtime push is then skipped
// (feature/notifications checks FirebaseApp.getApps()). CI copies google-services.json.example to exercise it.
if (file("google-services.json").exists()) {
    apply(plugin = libs.plugins.google.services.get().pluginId)
}

// Sentry Gradle plugin: only R8 mapping handling. The SDK is added explicitly (core/analytics) and OkHttp is
// instrumented explicitly (core/network's HttpTelemetry seam), so bytecode instrumentation and auto-installation
// are off. Mappings are uploaded only when SENTRY_AUTH_TOKEN (+ SENTRY_ORG / SENTRY_PROJECT) is set — local and
// fork builds never need a Sentry account.
sentry {
    val hasAuthToken = providers.environmentVariable("SENTRY_AUTH_TOKEN").isPresent
    includeProguardMapping.set(true)
    autoUploadProguardMapping.set(hasAuthToken)
    uploadNativeSymbols.set(false)
    includeSourceContext.set(false)
    autoInstallation { enabled.set(false) }
    tracingInstrumentation { enabled.set(false) }
    telemetry.set(false)
}

// The `benchmark/` module generates this app's baseline profile
// (`./gradlew :app:generateBaselineProfile`, device/emulator required — not
// run in CI yet). See apps/android/README.md.
baselineProfile {
    // defaults are fine for the skeleton; enable `automaticGenerationDuringBuild`
    // once a CI device lane exists.
}

android {
    namespace = "com.aedst.android"

    defaultConfig {
        applicationId = "com.aedst.android"
    }
}

// A release build must ship with TLS pins (see core/network/build.gradle.kts); fail fast instead of at review time.
tasks.matching { it.name == "preReleaseBuild" }.configureEach {
    dependsOn(":core:network:verifyReleaseCertPins")
}

dependencies {
    implementation(project(":core:common"))
    implementation(project(":core:ui"))
    implementation(project(":core:navigation"))
    implementation(project(":core:analytics"))
    implementation(project(":core:security"))
    implementation(project(":core:network"))
    implementation(project(":core:database"))
    implementation(project(":core:sync"))
    implementation(project(":core:media"))

    implementation(project(":feature:auth"))
    implementation(project(":feature:tasks"))
    implementation(project(":feature:search"))
    implementation(project(":feature:notifications"))
    implementation(project(":feature:media"))
    implementation(project(":feature:admin"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.splashscreen)
    implementation(libs.compose.activity)
    implementation(libs.compose.material.icons.extended)
    implementation(libs.navigation.compose)
    implementation(libs.hilt.navigation.compose)
    implementation(libs.workmanager.runtime)
    implementation(libs.hilt.work)

    androidTestImplementation(libs.androidx.test.junit)
    androidTestImplementation(libs.androidx.test.espresso)

    "baselineProfile"(project(":benchmark"))
    implementation(libs.androidx.profileinstaller)
}
