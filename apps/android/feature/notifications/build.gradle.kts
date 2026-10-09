plugins {
    alias(libs.plugins.android.feature.convention)
}

android {
    namespace = "com.aedst.feature.notifications"
}

dependencies {
    implementation(project(":core:network"))
    // Push bookkeeping (server device id, last registered token) lives in the UserPrefs DataStore.
    implementation(project(":core:datastore"))

    // FCM only. The app module applies the google-services plugin when app/google-services.json exists; without
    // it no FirebaseApp is initialised and push is skipped at runtime (FirebaseApp.getApps(...).isEmpty()).
    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.messaging)
    implementation(libs.kotlinx.coroutines.play.services)

    implementation(libs.androidx.core.ktx)
    implementation(libs.compose.activity)
    implementation(libs.compose.material.icons.extended)

    // Screen tests run on Robolectric + the Compose test rule (JUnit 4, no Jupiter-native equivalent).
    testImplementation(libs.junit.vintage.engine)
}
