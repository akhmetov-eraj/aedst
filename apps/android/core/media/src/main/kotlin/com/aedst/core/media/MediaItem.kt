package com.aedst.core.media

import java.time.Instant

enum class MediaKind { IMAGE, VIDEO, AUDIO, DOCUMENT, UNKNOWN }

enum class MediaStatus { PENDING_UPLOAD, PROCESSING, READY, FAILED, UNKNOWN }

/**
 * Domain view of `aedst.media.v1.Media`. URLs are short-lived presigned GETs: never persist them, and key image
 * caches by [id] + variant (see `MediaImage`), not by URL.
 */
data class MediaItem(
    val id: String,
    val kind: MediaKind,
    val status: MediaStatus,
    val filename: String,
    val contentType: String,
    val sizeBytes: Long,
    val width: Int,
    val height: Int,
    val durationMs: Long,
    /** Original; empty until [MediaStatus.READY]. */
    val url: String,
    /** Server-generated variants (empty until processing finishes). */
    val thumbnailUrl: String?,
    val previewUrl: String?,
    val optimizedUrl: String?,
    val posterUrl: String?,
    val taskId: String?,
    val createdAt: Instant?,
    val failureReason: String?,
) {
    /** Best small image for lists: thumbnail → poster (video) → preview → original image. */
    val listImageUrl: String?
        get() = thumbnailUrl ?: posterUrl ?: previewUrl ?: url.takeIf { kind == MediaKind.IMAGE && it.isNotEmpty() }

    /** Best URL to display/play in full: optimized rendition when available, else the original. */
    val displayUrl: String?
        get() = optimizedUrl ?: url.takeIf { it.isNotEmpty() }
}

data class MediaPage(val items: List<MediaItem>, val nextPageToken: String?)
