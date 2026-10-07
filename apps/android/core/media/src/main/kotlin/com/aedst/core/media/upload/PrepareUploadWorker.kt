package com.aedst.core.media.upload

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.hilt.work.HiltWorker
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import androidx.work.workDataOf
import com.aedst.core.common.AedstError
import com.aedst.core.media.data.MediaUploadApi
import com.aedst.core.media.processing.ImageCompressor
import com.aedst.core.media.processing.VideoCompressor
import com.aedst.core.network.toAedstError
import dagger.assisted.Assisted
import dagger.assisted.AssistedInject
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.security.MessageDigest

/**
 * Step 1/3: copy the picked item into app storage, compressing images (ImageCompressor) and videos
 * (VideoCompressor), hash it, and call `CreateUpload` for a presigned URL. Outputs everything the next steps need.
 *
 * The staged file lives in `filesDir/media-uploads/` (not the cache dir) so the OS cannot evict it between steps;
 * [CompleteUploadWorker] deletes it, as does a terminal failure (see `MediaUploadScheduler.cleanUp`).
 */
@HiltWorker
class PrepareUploadWorker @AssistedInject constructor(
    @Assisted appContext: Context,
    @Assisted params: WorkerParameters,
    private val uploadApi: MediaUploadApi,
    private val imageCompressor: ImageCompressor,
    private val videoCompressor: VideoCompressor,
) : CoroutineWorker(appContext, params) {

    override suspend fun doWork(): Result {
        val uploadId = inputData.getString(UploadWorkData.KEY_UPLOAD_ID) ?: return Result.failure()
        val source = inputData.getString(UploadWorkData.KEY_SOURCE_URI)?.let(Uri::parse) ?: return Result.failure()
        val taskId = inputData.getString(UploadWorkData.KEY_TASK_ID)

        return try {
            val staged = stage(uploadId, source)
            val target = uploadApi.createUpload(
                filename = staged.filename,
                contentType = staged.contentType,
                sizeBytes = staged.file.length(),
                taskId = taskId,
                sha256Hex = sha256Hex(staged.file),
            )
            Result.success(
                workDataOf(
                    UploadWorkData.KEY_FILE_PATH to staged.file.absolutePath,
                    UploadWorkData.KEY_CONTENT_TYPE to staged.contentType,
                    UploadWorkData.KEY_MEDIA_ID to target.mediaId,
                    UploadWorkData.KEY_UPLOAD_URL to target.url,
                    UploadWorkData.KEY_UPLOAD_METHOD to target.method,
                    UploadWorkData.KEY_UPLOAD_HEADERS to UploadWorkData.encodeHeaders(target.headers),
                    UploadWorkData.KEY_UPLOAD_EXPIRES_AT to target.expiresAtEpochMs,
                ),
            )
        } catch (cancellation: CancellationException) {
            throw cancellation
        } catch (security: SecurityException) {
            // Picker grant lost (e.g. after a reboot without a persisted grant): not retryable.
            UploadWorkData.resultFor(AedstError.Validation(security.javaClass.simpleName), runAttemptCount)
        } catch (@Suppress("TooGenericExceptionCaught") throwable: Throwable) {
            UploadWorkData.resultFor(throwable.toAedstError(), runAttemptCount)
        }
    }

    private data class Staged(val file: File, val contentType: String, val filename: String)

    private suspend fun stage(uploadId: String, source: Uri): Staged {
        val resolver = applicationContext.contentResolver
        val mimeType = resolver.getType(source) ?: DEFAULT_CONTENT_TYPE
        val displayName = displayName(source) ?: uploadId
        val baseName = displayName.substringBeforeLast('.')
        val directory = File(applicationContext.filesDir, STAGING_DIR).apply { mkdirs() }
        val destination = File(directory, uploadId)

        return when {
            mimeType.startsWith("image/") && mimeType !in PASSTHROUGH_IMAGE_TYPES -> withContext(Dispatchers.Default) {
                val result = imageCompressor.compress(resolver, source, destination)
                Staged(result.file, result.contentType, baseName + result.extension)
            }
            mimeType.startsWith("video/") -> {
                val file = File(destination.path + VideoCompressor.EXTENSION)
                videoCompressor.compress(source, file)
                Staged(file, VideoCompressor.CONTENT_TYPE, baseName + VideoCompressor.EXTENSION)
            }
            else -> withContext(Dispatchers.IO) {
                val file = File(destination.path + "." + displayName.substringAfterLast('.', "bin"))
                requireNotNull(resolver.openInputStream(source)).use { input ->
                    file.outputStream().use { input.copyTo(it) }
                }
                Staged(file, mimeType, displayName)
            }
        }
    }

    private fun displayName(source: Uri): String? = applicationContext.contentResolver
        .query(source, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
        ?.use { cursor -> if (cursor.moveToFirst()) cursor.getString(0) else null }
        ?.takeIf(String::isNotBlank)

    private suspend fun sha256Hex(file: File): String = withContext(Dispatchers.IO) {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(DIGEST_BUFFER_BYTES)
            while (true) {
                val read = input.read(buffer)
                if (read < 0) break
                digest.update(buffer, 0, read)
            }
        }
        digest.digest().joinToString("") { "%02x".format(it) }
    }

    companion object {
        const val STAGING_DIR = "media-uploads"
        private const val DEFAULT_CONTENT_TYPE = "application/octet-stream"
        private const val DIGEST_BUFFER_BYTES = 64 * 1024

        /** Animated or vector formats: re-encoding would drop animation/quality; let the server make variants. */
        private val PASSTHROUGH_IMAGE_TYPES = setOf("image/gif", "image/svg+xml")
    }
}
