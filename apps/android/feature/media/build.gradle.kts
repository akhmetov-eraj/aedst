plugins {
    alias(libs.plugins.android.feature.convention)
}

android {
    namespace = "com.aedst.feature.media"
}

dependencies {
    // MediaRepository, upload pipeline, MediaThumbnail / VideoPlayer / Photo Picker live in core/media.
    implementation(project(":core:media"))
    implementation(libs.compose.material.icons.extended)
}
