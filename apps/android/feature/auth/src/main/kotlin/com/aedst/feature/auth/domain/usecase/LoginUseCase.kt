package com.aedst.feature.auth.domain.usecase

import com.aedst.core.common.DispatcherProvider
import com.aedst.core.common.UseCase
import com.aedst.feature.auth.domain.model.AuthSession
import com.aedst.feature.auth.domain.model.Credentials
import com.aedst.feature.auth.domain.repository.AuthRepository
import javax.inject.Inject

data class LoginParams(val email: String, val password: String, val deviceId: String)

class LoginUseCase @Inject constructor(
    private val authRepository: AuthRepository,
    dispatcherProvider: DispatcherProvider,
) : UseCase<LoginParams, AuthSession>(dispatcherProvider) {
    override suspend fun execute(params: LoginParams): AuthSession =
        authRepository.login(Credentials(params.email, params.password), params.deviceId)
}
