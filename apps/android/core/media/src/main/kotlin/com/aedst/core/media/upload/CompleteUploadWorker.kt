package com.aedst.core.media.upload

import android.content.Context
import androidx.hilt.work.HiltWorker
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import androidx.work.workDataOf
import com.aedst.core.common.AedstError
import com.aedst.core.media.data.MediaUploadApi
import com.aedst.core.network.toAedstError
import dagger.assisted.Assisted
import dagger.assisted.AssistedInject
import kotlinx.coroutines.CancellationException
import java.io.File

/**
 * Step 3/3: `CompleteUpload` — the server verifies the object, then processes it (variants) in a River job and
 * notifies with `MEDIA_READY`. Deletes the staged local file on success.
 */
@HiltWorker
class CompleteUploadWorker @AssistedInject constructor(
    @Assisted appContext: Context,
    @Assisted params: WorkerParameters,
    private val uploadApi: MediaUploadApi,
) : CoroutineWorker(appContext, params) {

    override suspend fun doWork(): Result {
        val mediaId = inputData.getString(UploadWorkData.KEY_MEDIA_ID)
            ?: return UploadWorkData.resultFor(AedstError.Validation("Media id missing"), runAttemptCount)
        return try {
            uploadApi.completeUpload(mediaId)
            inputData.getString(UploadWorkData.KEY_FILE_PATH)?.let { File(it).delete() }
            Result.success(workDataOf(UploadWorkData.KEY_MEDIA_ID to mediaId))
        } catch (cancellation: CancellationException) {
            throw cancellation
        } catch (@Suppress("TooGenericExceptionCaught") throwable: Throwable) {
            UploadWorkData.resultFor(throwable.toAedstError(), runAttemptCount)
        }
    }
}
