plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.hilt.convention)
}

android {
    namespace = "com.aedst.core.security"
}

dependencies {
    implementation(project(":core:common"))

    implementation(libs.security.crypto)
    implementation(libs.play.integrity)
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.kotlinx.coroutines.play.services)
}
