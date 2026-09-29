plugins {
    alias(libs.plugins.android.library.convention)
    alias(libs.plugins.android.hilt.convention)
    alias(libs.plugins.protobuf)
}

android {
    namespace = "com.aedst.core.datastore"
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:${libs.versions.protobuf.javalite.get()}"
    }
    generateProtoTasks {
        all().forEach { task ->
            task.builtins {
                create("java") {
                    option("lite")
                }
            }
        }
    }
}

dependencies {
    implementation(project(":core:common"))

    implementation(libs.datastore.core)
    implementation(libs.protobuf.javalite)
}
