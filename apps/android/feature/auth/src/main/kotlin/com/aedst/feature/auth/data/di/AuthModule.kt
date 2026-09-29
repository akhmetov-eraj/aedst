package com.aedst.feature.auth.data.di

import com.aedst.auth.v1.AuthServiceClient
import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.core.network.AuthTokenProvider
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.data.repository.AuthRepositoryImpl
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.connectrpc.ProtocolClientInterface
import dagger.Binds
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

/**
 * feature/auth's DI seam. This is the one place another module's contract
 * ([AuthTokenProvider], declared in `core/network`) gets an implementation —
 * analogous to `modules/users` calling `auth.api.Service` in-process on the
 * server: the dependency direction is declared where it's consumed, not
 * where it's implemented.
 */
@Module
@InstallIn(SingletonComponent::class)
abstract class AuthModule {

    @Binds
    abstract fun bindAuthRepository(impl: AuthRepositoryImpl): AuthRepository

    @Binds
    abstract fun bindAuthTokenProvider(impl: AuthTokenProviderImpl): AuthTokenProvider

    companion object {
        @Provides
        @Singleton
        fun provideAuthServiceClient(client: ProtocolClientInterface): AuthServiceClientInterface = AuthServiceClient(client)
    }
}

class AuthTokenProviderImpl @javax.inject.Inject constructor(
    private val tokenStore: AuthTokenStore,
) : AuthTokenProvider {
    override fun currentAccessToken(): String? = tokenStore.currentAccessToken()
}
