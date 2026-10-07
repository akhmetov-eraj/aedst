package com.aedst.core.media.data

import com.aedst.core.media.MediaItem
import com.aedst.core.media.MediaKind
import com.aedst.core.media.MediaStatus
import com.aedst.core.network.toInstantOrNull
import com.aedst.media.v1.Media
import com.aedst.media.v1.VariantKind
import com.aedst.media.v1.MediaKind as ProtoMediaKind
import com.aedst.media.v1.MediaStatus as ProtoMediaStatus

internal fun Media.toDomain(): MediaItem {
    fun variantUrl(kind: VariantKind): String? =
        variantsList.firstOrNull { it.kind == kind }?.url?.takeIf(String::isNotEmpty)

    return MediaItem(
        id = id,
        kind = when (kind) {
            ProtoMediaKind.MEDIA_KIND_IMAGE -> MediaKind.IMAGE
            ProtoMediaKind.MEDIA_KIND_VIDEO -> MediaKind.VIDEO
            ProtoMediaKind.MEDIA_KIND_AUDIO -> MediaKind.AUDIO
            ProtoMediaKind.MEDIA_KIND_DOCUMENT -> MediaKind.DOCUMENT
            else -> MediaKind.UNKNOWN
        },
        status = when (status) {
            ProtoMediaStatus.MEDIA_STATUS_PENDING_UPLOAD -> MediaStatus.PENDING_UPLOAD
            ProtoMediaStatus.MEDIA_STATUS_PROCESSING -> MediaStatus.PROCESSING
            ProtoMediaStatus.MEDIA_STATUS_READY -> MediaStatus.READY
            ProtoMediaStatus.MEDIA_STATUS_FAILED -> MediaStatus.FAILED
            else -> MediaStatus.UNKNOWN
        },
        filename = filename,
        contentType = contentType,
        sizeBytes = sizeBytes,
        width = width,
        height = height,
        durationMs = durationMs,
        url = url,
        thumbnailUrl = variantUrl(VariantKind.VARIANT_KIND_THUMBNAIL),
        previewUrl = variantUrl(VariantKind.VARIANT_KIND_PREVIEW),
        optimizedUrl = variantUrl(VariantKind.VARIANT_KIND_OPTIMIZED),
        posterUrl = variantUrl(VariantKind.VARIANT_KIND_POSTER),
        taskId = taskId.takeIf(String::isNotEmpty),
        createdAt = if (hasCreatedAt()) createdAt.toInstantOrNull() else null,
        failureReason = failureReason.takeIf(String::isNotEmpty),
    )
}
