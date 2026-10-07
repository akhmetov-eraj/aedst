package com.aedst.feature.auth.data.di

import com.aedst.auth.v1.AuthServiceClient
import com.aedst.auth.v1.AuthServiceClientInterface
import com.aedst.core.common.ApplicationScope
import com.aedst.core.common.SessionState
import com.aedst.core.common.SessionUser
import com.aedst.core.network.AuthTokenProvider
import com.aedst.core.network.TokenRefresher
import com.aedst.feature.auth.data.local.AccessTokenClaims
import com.aedst.feature.auth.data.local.AuthTokenStore
import com.aedst.feature.auth.data.remote.AuthTokenRefresher
import com.aedst.feature.auth.data.repository.AuthRepositoryImpl
import com.aedst.feature.auth.domain.repository.AuthRepository
import com.connectrpc.ProtocolClientInterface
import dagger.Binds
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import javax.inject.Inject
import javax.inject.Singleton

/**
 * feature/auth's DI seam. This is the one place another module's contract
 * ([AuthTokenProvider], [TokenRefresher] — declared in `core/network` — and
 * [SessionState] — declared in `core/common`) gets an implementation —
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

    @Binds
    abstract fun bindTokenRefresher(impl: AuthTokenRefresher): TokenRefresher

    @Binds
    abstract fun bindSessionState(impl: AuthSessionState): SessionState

    companion object {
        @Provides
        @Singleton
        fun provideAuthServiceClient(client: ProtocolClientInterface): AuthServiceClientInterface =
            AuthServiceClient(client)
    }
}

class AuthTokenProviderImpl @Inject constructor(private val tokenStore: AuthTokenStore) : AuthTokenProvider {
    override fun currentAccessToken(): String? = tokenStore.currentAccessToken()
}

/** `null` until the persisted session is restored (hold the splash), then `true`/`false` as the session changes. */
@Singleton
class AuthSessionState @Inject constructor(tokenStore: AuthTokenStore, @ApplicationScope scope: CoroutineScope) :
    SessionState {
    override val isSignedIn: StateFlow<Boolean?> =
        combine(tokenStore.isRestored, tokenStore.session) { restored, session ->
            if (restored) session != null else null
        }.stateIn(scope, SharingStarted.Eagerly, null)

    /** Roles come from the access token (UI gating only) and follow it across refreshes. */
    override val currentUser: StateFlow<SessionUser?> =
        tokenStore.session
            .map { session -> session?.let { SessionUser(it.userId, AccessTokenClaims.roles(it.accessToken)) } }
            .distinctUntilChanged()
            .stateIn(scope, SharingStarted.Eagerly, null)
}
