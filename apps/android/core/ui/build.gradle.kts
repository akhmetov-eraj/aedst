plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.compose.convention)
}

android {
    namespace = "com.aedst.core.ui"
}

dependencies {
    // AedstError → localized message mapping lives next to the strings (ErrorMessages.kt).
    implementation(project(":core:common"))
    implementation(libs.compose.material.icons.extended)

    testImplementation(project(":core:testing"))
}
