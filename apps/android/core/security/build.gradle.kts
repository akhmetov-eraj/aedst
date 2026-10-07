plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.hilt.convention)
    alias(libs.plugins.kotlin.serialization)
}

android {
    namespace = "com.aedst.core.security"
}

dependencies {
    implementation(project(":core:common"))

    // Android Keystore-wrapped Tink AEAD (replaces the deprecated androidx.security:security-crypto).
    implementation(libs.tink.android)
    implementation(libs.datastore.core)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.play.integrity)
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.kotlinx.coroutines.play.services)

    testImplementation(libs.junit4)
    testImplementation(libs.kotlinx.coroutines.test)
}
