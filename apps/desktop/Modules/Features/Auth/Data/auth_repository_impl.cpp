#include "Data/auth_repository_impl.hpp"

#include "aedst/core/logger.hpp"

#include <chrono>
#include <utility>

namespace aedst::feature::auth {

using core::AedstLogger;
namespace dto = network::dto;

AuthRepositoryImpl::AuthRepositoryImpl(std::shared_ptr<AuthRemoteDataSource> remote,
                                       std::shared_ptr<LocalTokenStoreAdapter> local,
                                       std::shared_ptr<const core::Clock> clock, std::string deviceId)
    : remote_(std::move(remote)), local_(std::move(local)), clock_(std::move(clock)),
      deviceId_(std::move(deviceId)) {}

void AuthRepositoryImpl::login(const Credentials& credentials, core::Completion<Session> done) {
    const dto::LoginRequestDto request{
        .email = credentials.email,
        .password = credentials.password,
        .deviceId = deviceId_,
    };
    remote_->login(request, [weak = weak_from_this(), done = std::move(done)](
                                 network::ApiResult<dto::LoginResponseDto> result) {
        const auto self = weak.lock();
        if (!self) {
            return;
        }
        if (!result) {
            done(core::fail(result.error().toCoreError()));
            return;
        }
        self->persistAndComplete(self->toSession(std::move(result->userId), result->tokens), done);
    });
}

void AuthRepositoryImpl::refreshToken(core::Completion<Session> done) {
    auto current = local_->load();
    if (!current) {
        done(core::fail(current.error()));
        return;
    }
    if (!*current) {
        done(core::fail(core::Error::unauthenticated("No session to refresh")));
        return;
    }

    const dto::RefreshTokenRequestDto request{.refreshToken = (*current)->refreshToken};
    remote_->refreshToken(request, [weak = weak_from_this(), userId = (*current)->userId, done = std::move(done)](
                                        network::ApiResult<dto::RefreshTokenResponseDto> result) {
        const auto self = weak.lock();
        if (!self) {
            return;
        }
        if (!result) {
            if (result.error().kind == network::ApiErrorKind::Unauthenticated) {
                // Refresh token revoked or expired server-side: the local session is dead too.
                if (auto cleared = self->local_->clear(); !cleared) {
                    AedstLogger::error("Failed to clear rejected session: {}", cleared.error().message);
                }
            }
            done(core::fail(result.error().toCoreError()));
            return;
        }
        self->persistAndComplete(self->toSession(userId, result->tokens), done);
    });
}

void AuthRepositoryImpl::logout(core::Completion<void> done) {
    auto current = local_->load();
    if (!current || !*current) {
        (void)local_->clear();
        done(core::ok());
        return;
    }

    const dto::LogoutRequestDto request{.refreshToken = (*current)->refreshToken};
    remote_->logout(request, [weak = weak_from_this(), done = std::move(done)](network::ApiResult<void> result) {
        const auto self = weak.lock();
        if (!self) {
            return;
        }
        if (!result) {
            AedstLogger::warn("Server-side logout failed ({}); clearing local session anyway", result.error().message);
        }
        if (auto cleared = self->local_->clear(); !cleared) {
            done(core::fail(cleared.error()));
            return;
        }
        done(core::ok());
    });
}

std::optional<Session> AuthRepositoryImpl::currentSession() const {
    auto session = local_->load();
    if (!session) {
        AedstLogger::error("Failed to read stored session: {}", session.error().message);
        return std::nullopt;
    }
    return *session;
}

Session AuthRepositoryImpl::toSession(std::string userId, const dto::TokenPairDto& tokens) const {
    return {
        .userId = std::move(userId),
        .accessToken = tokens.accessToken,
        .refreshToken = tokens.refreshToken,
        .accessTokenExpiresAt = clock_->now() + std::chrono::seconds{tokens.accessTokenExpiresInSeconds},
    };
}

void AuthRepositoryImpl::persistAndComplete(Session session, const core::Completion<Session>& done) {
    if (auto saved = local_->save(session); !saved) {
        AedstLogger::error("Failed to persist session: {}", saved.error().message);
        done(core::fail(saved.error()));
        return;
    }
    done(std::move(session));
}

} // namespace aedst::feature::auth
