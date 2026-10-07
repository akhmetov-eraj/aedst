plugins {
    alias(libs.plugins.android.feature.convention)
}

android {
    namespace = "com.aedst.feature.tasks"
}

dependencies {
    implementation(project(":core:network"))
    // Attachments on the task detail screen: MediaRepository, upload pipeline, thumbnails, Photo Picker.
    implementation(project(":core:media"))
    implementation(project(":core:analytics"))
    implementation(libs.compose.material.icons.extended)
}
