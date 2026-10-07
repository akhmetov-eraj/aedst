package com.aedst.core.media.ui

import coil3.ImageLoader
import coil3.SingletonImageLoader
import coil3.network.okhttp.OkHttpNetworkFetcherFactory
import com.aedst.core.common.AppInitializer
import com.aedst.core.network.ExternalHttpClient
import okhttp3.OkHttpClient
import javax.inject.Inject

/**
 * Configures Coil's singleton [ImageLoader] to fetch through the app's [ExternalHttpClient] (shared connection pool,
 * no bearer token — media URLs are presigned object-storage GETs).
 */
class MediaImageLoaderInitializer @Inject constructor(@ExternalHttpClient private val client: OkHttpClient) :
    AppInitializer {
    override fun initialize() {
        SingletonImageLoader.setSafe { context ->
            ImageLoader.Builder(context)
                .components { add(OkHttpNetworkFetcherFactory(callFactory = { client })) }
                .build()
        }
    }
}
