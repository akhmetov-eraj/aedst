plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.hilt.convention)
    alias(libs.plugins.kotlin.serialization)
}

android {
    namespace = "com.aedst.core.network"

    buildFeatures {
        buildConfig = true
    }

    buildTypes {
        debug {
            buildConfigField("String", "BASE_URL", "\"https://api.dev.aedst.example/\"")
        }
        release {
            buildConfigField("String", "BASE_URL", "\"https://api.aedst.example/\"")
        }
    }

    // `buf generate` (see ../../buf.gen.yaml, run via `make generate` from
    // apps/android/) writes the ConnectRPC/Protobuf Kotlin gencode here —
    // the Kotlin-side equivalent of apps/apple's Generated/ sourceSet.
    // Nothing under kotlin-gen is committed (see .gitignore); run `make
    // generate` after cloning, before opening the project.
    sourceSets {
        getByName("main").kotlin.srcDir("src/main/kotlin-gen")
    }
}

dependencies {
    implementation(project(":core:common"))
    implementation(project(":core:security"))

    // ConnectRPC client + generated message/service types (aedst.*.v1.*)
    // consumed by feature modules — see docs/content/library-first.md's
    // Android table. Retrofit/OkHttp-as-REST-client is not used for these
    // calls; OkHttp remains only as connect-kotlin's HTTP engine.
    api(libs.connect.kotlin.core)
    api(libs.connect.kotlin.google.javalite.ext)
    api(libs.protobuf.kotlin.lite)
    implementation(libs.connect.kotlin.okhttp)
    implementation(libs.okhttp.core)
    implementation(libs.okhttp.logging.interceptor)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.kotlinx.coroutines.core)

    testImplementation(libs.junit4)
    testImplementation(libs.okhttp.mockwebserver)
    testImplementation(libs.kotlinx.coroutines.test)
}
