plugins {
    alias(libs.plugins.android.test)
    alias(libs.plugins.baseline.profile)
}

android {
    namespace = "com.aedst.benchmark"
    compileSdk = 37

    defaultConfig {
        minSdk = 28
        targetSdk = 36
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    targetProjectPath = ":app"

    // Register a Gradle Managed Device via `testOptions.managedDevices.localDevices`
    // once CI has an emulator image pinned; local `connectedCheck` against a
    // physical/emulated device works without one.

    // Build types (`benchmarkRelease`, `nonMinifiedRelease`) are created by the
    // androidx.baselineprofile plugin on both this producer and `:app`.
}

baselineProfile {
    // Physical device / emulator required; no Gradle Managed Device is pinned yet.
    useConnectedDevices = true
}

dependencies {
    implementation(libs.androidx.test.junit)
    implementation(libs.androidx.test.espresso)
    implementation(libs.androidx.benchmark.macro)
}
