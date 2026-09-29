import com.android.build.api.dsl.CommonExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.dependencies

/** Wires the Compose compiler + explicit Compose/Material3 artifacts on any android module. */
class AndroidComposeConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            pluginManager.apply("org.jetbrains.kotlin.plugin.compose")

            extensions.configure(CommonExtension::class.java) {
                buildFeatures.compose = true
            }

            dependencies {
                add("implementation", libs.findLibrary("compose-ui").get())
                add("implementation", libs.findLibrary("compose-ui-graphics").get())
                add("implementation", libs.findLibrary("compose-ui-tooling-preview").get())
                add("implementation", libs.findLibrary("compose-material3").get())
                add("implementation", libs.findLibrary("compose-lifecycle-runtime").get())
                add("implementation", libs.findLibrary("compose-lifecycle-viewmodel").get())
                add("debugImplementation", libs.findLibrary("compose-ui-tooling").get())
                add("androidTestImplementation", libs.findLibrary("compose-ui-test-junit4").get())
                add("debugImplementation", libs.findLibrary("compose-ui-test-manifest").get())
            }
        }
    }
}
