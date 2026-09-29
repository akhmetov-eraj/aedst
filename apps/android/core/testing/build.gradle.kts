plugins {
    alias(libs.plugins.android.library.convention)
}

android {
    namespace = "com.aedst.core.testing"
}

dependencies {
    api(project(":core:common"))
    api(libs.junit4)
    api(libs.mockk)
    api(libs.mockk.android)
    api(libs.turbine)
    api(libs.kotlinx.coroutines.test)
    implementation(libs.kotlinx.coroutines.core)
}
