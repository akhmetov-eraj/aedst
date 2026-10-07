package com.aedst.feature.auth.testing

import com.aedst.auth.v1.TokenPair
import com.connectrpc.ConnectException
import com.connectrpc.ResponseMessage

fun <T> success(message: T): ResponseMessage<T> =
    ResponseMessage.Success(message, headers = emptyMap(), trailers = emptyMap())

fun <T> failure(exception: ConnectException): ResponseMessage<T> =
    ResponseMessage.Failure(exception, headers = emptyMap(), trailers = emptyMap())

fun tokenPair(access: String, refresh: String, expiresInSeconds: Long): TokenPair = TokenPair.newBuilder()
    .setAccessToken(access)
    .setRefreshToken(refresh)
    .setAccessTokenExpiresInSeconds(expiresInSeconds)
    .build()
