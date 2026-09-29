plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.compose.convention)
}

android {
    namespace = "com.aedst.core.ui.tv"
}

dependencies {
    // `api`, not `implementation`: consumers of core:ui-tv (namely :app-tv) get
    // AedstTheme/AedstSpacing/etc. transitively without adding their own
    // direct dependency on :core:ui — see this module's README.
    api(project(":core:ui"))

    // Official Jetpack Compose for TV library (transitively brings in
    // androidx.tv:tv-foundation for TvLazyRow/TvLazyColumn). `api`, not
    // `implementation`: :app-tv's own root composables (MainTvActivity) use
    // androidx.tv.material3.Surface directly for the app's root container,
    // so the artifact needs to be on :app-tv's compile classpath too.
    api(libs.tv.material)
}
