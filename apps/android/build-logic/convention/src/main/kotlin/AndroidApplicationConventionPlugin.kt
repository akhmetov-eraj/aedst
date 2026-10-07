import com.android.build.api.dsl.ApplicationExtension
import org.gradle.api.JavaVersion
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.configure
import org.gradle.kotlin.dsl.dependencies

/** Applied only by `app/`. Everything cross-cutting (Hilt, Compose, Detekt) is composed in. */
class AndroidApplicationConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            pluginManager.apply("com.android.application")
            pluginManager.apply("aedst.detekt")

            extensions.configure<ApplicationExtension> {
                compileSdk = ANDROID_COMPILE_SDK

                defaultConfig {
                    targetSdk = ANDROID_TARGET_SDK
                    minSdk = ANDROID_MIN_SDK
                    versionCode = 1
                    versionName = providers.gradleProperty("aedst.versionName").get()
                    testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
                }

                compileOptions {
                    sourceCompatibility = JavaVersion.VERSION_17
                    targetCompatibility = JavaVersion.VERSION_17
                    isCoreLibraryDesugaringEnabled = true
                }

                buildTypes {
                    release {
                        isMinifyEnabled = true
                        proguardFiles(
                            getDefaultProguardFile("proguard-android-optimize.txt"),
                            "proguard-rules.pro",
                        )
                    }
                }
            }

            // Some Firebase artifacts ship protolite-well-known-types, whose classes duplicate
            // protobuf-javalite (needed by core/datastore + connect-kotlin) and break dexing.
            configurations.configureEach {
                exclude(mapOf("group" to "com.google.firebase", "module" to "protolite-well-known-types"))
            }

            dependencies {
                add("coreLibraryDesugaring", versionCatalog.findLibrary("desugar-jdk-libs").get())
            }
        }
    }
}
