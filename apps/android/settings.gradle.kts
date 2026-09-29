pluginManagement {
    includeBuild("build-logic")
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "aedst-android"

include(":app")
include(":app-tv")

include(":core:common")
include(":core:ui")
include(":core:ui-tv")
include(":core:network")
include(":core:database")
include(":core:datastore")
include(":core:security")
include(":core:analytics")
include(":core:navigation")
include(":core:testing")

include(":feature:auth")

include(":benchmark")
