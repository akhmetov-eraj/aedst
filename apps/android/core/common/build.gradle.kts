plugins {
    alias(libs.plugins.android.library.convention)
}

android {
    namespace = "com.aedst.core.common"
}

dependencies {
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.javax.inject)

    testImplementation(libs.junit4)
    testImplementation(libs.kotlinx.coroutines.test)
}
