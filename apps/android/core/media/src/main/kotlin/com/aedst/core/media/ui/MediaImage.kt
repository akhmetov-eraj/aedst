package com.aedst.core.media.ui

import android.graphics.Bitmap
import android.net.Uri
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.PlayCircle
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import coil3.compose.AsyncImage
import coil3.compose.LocalPlatformContext
import coil3.request.ImageRequest
import coil3.request.crossfade
import com.aedst.core.media.MediaItem
import com.aedst.core.media.MediaKind
import com.aedst.core.media.processing.LocalThumbnailLoader

/**
 * An image from a presigned URL. Presigned URLs carry a fresh signature on every API read, so the memory/disk cache
 * is keyed by [cacheKey] (media id + variant), not by URL — otherwise every list refresh re-downloads every image.
 */
@Composable
fun MediaImage(
    url: String,
    cacheKey: String,
    contentDescription: String?,
    modifier: Modifier = Modifier,
    contentScale: ContentScale = ContentScale.Crop,
) {
    val context = LocalPlatformContext.current
    val request = remember(url, cacheKey) {
        ImageRequest.Builder(context)
            .data(url)
            .memoryCacheKey(cacheKey)
            .diskCacheKey(cacheKey)
            .crossfade(true)
            .build()
    }
    AsyncImage(
        model = request,
        contentDescription = contentDescription,
        modifier = modifier,
        contentScale = contentScale,
    )
}

/**
 * Square thumbnail for a server-side media item: the server's thumbnail/poster variant when processing has produced
 * one, otherwise a kind icon. Videos get a play glyph overlay.
 */
@Composable
fun MediaThumbnail(item: MediaItem, contentDescription: String?, modifier: Modifier = Modifier) {
    Box(modifier = modifier.background(MaterialTheme.colorScheme.surfaceVariant), contentAlignment = Alignment.Center) {
        val imageUrl = item.listImageUrl
        if (imageUrl != null) {
            MediaImage(
                url = imageUrl,
                cacheKey = "${item.id}:list",
                contentDescription = contentDescription,
                modifier = Modifier.fillMaxSize(),
            )
        } else {
            KindIcon(item.kind)
        }
        if (item.kind == MediaKind.VIDEO && imageUrl != null) PlayGlyph()
    }
}

/**
 * Thumbnail for a local item that has no server variant yet (an upload in progress). Images are decoded by Coil
 * straight from the content URI; video frames come from [LocalThumbnailLoader] (MediaStore / MediaMetadataRetriever).
 */
@Composable
fun LocalMediaThumbnail(uri: Uri, isVideo: Boolean, contentDescription: String?, modifier: Modifier = Modifier) {
    Box(modifier = modifier.background(MaterialTheme.colorScheme.surfaceVariant), contentAlignment = Alignment.Center) {
        if (!isVideo) {
            AsyncImage(
                model = uri,
                contentDescription = contentDescription,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize(),
            )
        } else {
            val context = LocalContext.current
            var frame by remember(uri) { mutableStateOf<Bitmap?>(null) }
            LaunchedEffect(uri) { frame = LocalThumbnailLoader(context.applicationContext).load(uri, isVideo = true) }
            frame?.let { bitmap ->
                Image(
                    bitmap = bitmap.asImageBitmap(),
                    contentDescription = contentDescription,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize(),
                )
            } ?: KindIcon(MediaKind.VIDEO)
            PlayGlyph()
        }
    }
}

@Composable
private fun KindIcon(kind: MediaKind) {
    Icon(
        imageVector = if (kind == MediaKind.VIDEO) Icons.Filled.PlayCircle else Icons.Filled.Description,
        contentDescription = null,
        tint = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

@Composable
private fun PlayGlyph() {
    Icon(
        imageVector = Icons.Filled.PlayCircle,
        contentDescription = null,
        tint = MaterialTheme.colorScheme.surface,
        modifier = Modifier.size(32.dp),
    )
}
