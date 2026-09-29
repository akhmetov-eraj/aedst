plugins {
    alias(libs.plugins.android.feature.convention)
    alias(libs.plugins.kotlin.serialization)
}

android {
    namespace = "com.aedst.feature.auth"
}

dependencies {
    implementation(project(":core:network"))
    implementation(project(":core:security"))
    implementation(project(":core:analytics"))

    implementation(libs.kotlinx.serialization.json)
}
