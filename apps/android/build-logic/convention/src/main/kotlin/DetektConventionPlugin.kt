import io.gitlab.arturbosch.detekt.Detekt
import io.gitlab.arturbosch.detekt.extensions.DetektExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.configure
import org.jlleitschuh.gradle.ktlint.KtlintExtension

/** Applied transitively by the android-library/application/jvm conventions — never applied on its own. */
class DetektConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            pluginManager.apply("io.gitlab.arturbosch.detekt")
            pluginManager.apply("org.jlleitschuh.gradle.ktlint")

            extensions.configure<DetektExtension> {
                buildUponDefaultConfig = true
                config.setFrom(rootProject.file("detekt.yml"))
                autoCorrect = false
            }

            // core/network registers src/main/kotlin-gen (buf-generated
            // ConnectRPC/Protobuf stubs, see its build.gradle.kts) as an extra
            // `main` Kotlin source dir. Neither linter should format-check or
            // analyze generated code.
            tasks.withType(Detekt::class.java).configureEach {
                exclude("**/kotlin-gen/**")
            }
            extensions.configure<KtlintExtension> {
                filter {
                    exclude { entry -> entry.file.path.contains("kotlin-gen") }
                }
            }
        }
    }
}
