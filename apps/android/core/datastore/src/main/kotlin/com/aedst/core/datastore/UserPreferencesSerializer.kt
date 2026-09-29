package com.aedst.core.datastore

import androidx.datastore.core.Serializer
import java.io.InputStream
import java.io.OutputStream

/**
 * NOTE: [UserPrefs] is generated from `src/main/proto/user_prefs.proto` by
 * the `com.google.protobuf` Gradle plugin at build time — it does not exist
 * in source control. Running a sync (or `./gradlew :core:datastore:generateProto`)
 * before opening this file in the IDE is expected to be needed once.
 */
object UserPreferencesSerializer : Serializer<UserPrefs> {
    override val defaultValue: UserPrefs = UserPrefs.getDefaultInstance()

    override suspend fun readFrom(input: InputStream): UserPrefs = UserPrefs.parseFrom(input)

    override suspend fun writeTo(t: UserPrefs, output: OutputStream) {
        t.writeTo(output)
    }
}
