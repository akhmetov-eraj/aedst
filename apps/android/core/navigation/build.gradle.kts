plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.compose.convention)
}

android {
    namespace = "com.aedst.core.navigation"
}

dependencies {
    implementation(libs.navigation.compose)

    testImplementation(libs.junit4)
}
