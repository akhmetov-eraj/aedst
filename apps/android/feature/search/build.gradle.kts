plugins {
    alias(libs.plugins.android.feature.convention)
}

android {
    namespace = "com.aedst.feature.search"
}

dependencies {
    implementation(project(":core:network"))
    implementation(project(":core:analytics"))
    implementation(libs.compose.material.icons.extended)
}
