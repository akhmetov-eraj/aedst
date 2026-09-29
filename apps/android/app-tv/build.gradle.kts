plugins {
    alias(libs.plugins.android.application.convention)
    alias(libs.plugins.android.compose.convention)
    alias(libs.plugins.android.hilt.convention)
}

// Thin Android TV counterpart to `:app` (the Android-native equivalent of
// Apple's `App_iOS`/`App_macOS` split — see docs/architecture-android.md and
// AGENTS.md). Intentionally skips the Firebase (`google-services`,
// `firebase-crashlytics`, `firebase-perf`) and `baseline-profile` plugins
// that `:app` applies: those are per-application-module wiring
// (google-services.json, a benchmark target) this module doesn't have yet,
// not a divergence in what's approved. `:core:analytics` still carries its
// own Firebase dependency internally regardless.
android {
    namespace = "com.aedst.android.tv"

    defaultConfig {
        applicationId = "com.aedst.android.tv"
    }
}

dependencies {
    // Same shared business-logic modules `:app` depends on.
    implementation(project(":core:common"))
    implementation(project(":core:navigation"))
    implementation(project(":core:analytics"))
    implementation(project(":core:security"))

    // TV design system instead of `:core:ui` for TV-specific screens —
    // `:core:ui-tv` re-exposes `:core:ui`'s theme tokens via `api`, so
    // AedstTheme is still available here without a direct dependency.
    implementation(project(":core:ui-tv"))

    implementation(project(":feature:auth"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.compose.activity)
    implementation(libs.navigation.compose)
    implementation(libs.hilt.navigation.compose)

    androidTestImplementation(libs.androidx.test.junit)
    androidTestImplementation(libs.androidx.test.espresso)
}
