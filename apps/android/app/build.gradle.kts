plugins {
    alias(libs.plugins.android.application.convention)
    alias(libs.plugins.android.compose.convention)
    alias(libs.plugins.android.hilt.convention)
    alias(libs.plugins.google.services)
    alias(libs.plugins.firebase.crashlytics)
    alias(libs.plugins.firebase.perf)
    alias(libs.plugins.baseline.profile)
}

// The `benchmark/` module generates this app's baseline profile
// (`./gradlew :app:generateBaselineProfile`, device/emulator required — not
// run in this environment). See apps/android/README.md.
baselineProfile {
    // defaults are fine for the skeleton; enable `automaticGenerationDuringBuild`
    // once a CI device lane exists (Instructions.md TODO: Fastlane/Play track wiring).
}

android {
    namespace = "com.aedst.android"

    defaultConfig {
        applicationId = "com.aedst.android"
    }
}

dependencies {
    implementation(project(":core:common"))
    implementation(project(":core:ui"))
    implementation(project(":core:navigation"))
    implementation(project(":core:analytics"))
    implementation(project(":core:security"))

    implementation(project(":feature:auth"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.compose.activity)
    implementation(libs.navigation.compose)
    implementation(libs.hilt.navigation.compose)

    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.crashlytics)
    implementation(libs.firebase.perf)
    implementation(libs.firebase.analytics)

    androidTestImplementation(libs.androidx.test.junit)
    androidTestImplementation(libs.androidx.test.espresso)

    "baselineProfile"(project(":benchmark"))
    implementation(libs.androidx.profileinstaller)
}
