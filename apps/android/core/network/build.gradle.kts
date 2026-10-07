plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.hilt.convention)
    alias(libs.plugins.kotlin.serialization)
}

// Release TLS pins: comma-separated `sha256/<base64 SPKI hash>` values (live key + at least one backup), passed as
// `-Paedst.certPins=...` (CI secret / ~/.gradle/gradle.properties). Never committed: they are per-environment.
val certPins = providers.gradleProperty("aedst.certPins").orElse("")
val allowUnpinnedRelease = providers.gradleProperty("aedst.allowUnpinnedRelease").map(String::toBoolean).orElse(false)
val clientVersion = providers.gradleProperty("aedst.versionName").orElse("1.0.0")

android {
    namespace = "com.aedst.core.network"

    buildFeatures {
        buildConfig = true
    }

    defaultConfig {
        buildConfigField("String", "CLIENT_VERSION", "\"${clientVersion.get()}\"")
    }

    buildTypes {
        debug {
            buildConfigField("String", "BASE_URL", "\"https://api.dev.aedst.example/\"")
            buildConfigField("String", "CERT_PINS", "\"\"")
        }
        release {
            buildConfigField("String", "BASE_URL", "\"https://api.aedst.example/\"")
            buildConfigField("String", "CERT_PINS", "\"${certPins.get()}\"")
        }
    }

    // `buf generate` (see ../../buf.gen.yaml, run via `make generate` from
    // apps/android/) writes the ConnectRPC/Protobuf Kotlin gencode here —
    // the Kotlin-side equivalent of apps/apple's Generated/ sourceSet.
    // Nothing under kotlin-gen is committed (see .gitignore); run `make
    // generate` after cloning, before opening the project.
    sourceSets {
        // Java (protobuf messages) and Kotlin (DSL + connect client) gencode share one
        // directory; each compiler only takes its own file type.
        getByName("main").java.directories.add("src/main/kotlin-gen")
        getByName("main").kotlin.directories.add("src/main/kotlin-gen")
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

// A release build without TLS pinning is a security regression, so refuse to produce one by accident. `app/` wires
// this into its `preReleaseBuild`. Opt out explicitly with -Paedst.allowUnpinnedRelease=true (e.g. a staging flavour).
tasks.register("verifyReleaseCertPins") {
    group = "verification"
    description = "Fails if a release build would ship without TLS certificate pins."
    val pins = certPins
    val allowUnpinned = allowUnpinnedRelease
    doLast {
        if (pins.get().isBlank() && !allowUnpinned.get()) {
            throw GradleException(
                "Release build has no TLS pins. Pass -Paedst.certPins=sha256/<live>,sha256/<backup> " +
                    "(or -Paedst.allowUnpinnedRelease=true for a non-production build).",
            )
        }
    }
}
