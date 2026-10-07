plugins {
    alias(libs.plugins.android.application.convention)
    alias(libs.plugins.android.compose.convention)
    alias(libs.plugins.android.hilt.convention)
}

// Thin Android TV counterpart to `:app` (the Android-native equivalent of
// Apple's `App_iOS`/`App_macOS` split — see docs/architecture-android.md).
// Intentionally skips the `google-services`, `sentry` and `baseline-profile`
// plugins `:app` applies: those are per-application-module wiring
// (google-services.json, R8 mapping upload, a benchmark target) this module
// doesn't have yet. Sentry/PostHog still run here through `:core:analytics`.
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
