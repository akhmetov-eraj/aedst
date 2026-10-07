plugins {
    alias(libs.plugins.android.feature.convention)
}

android {
    namespace = "com.aedst.feature.admin"
}

dependencies {
    implementation(project(":core:network"))
    implementation(libs.compose.material.icons.extended)
}
