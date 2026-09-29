#include "Domain/refresh_token_use_case.hpp"

#include <utility>

namespace aedst::feature::auth {

RefreshTokenUseCase::RefreshTokenUseCase(std::shared_ptr<AuthRepository> repository,
                                         std::shared_ptr<const core::Clock> clock)
    : repository_(std::move(repository)), clock_(std::move(clock)) {}

void RefreshTokenUseCase::execute(core::NoInput /*input*/, core::Completion<Session> done) {
    auto current = repository_->currentSession();
    if (!current) {
        done(core::fail(core::Error::unauthenticated("Not signed in")));
        return;
    }
    if (!current->isAccessTokenExpired(clock_->now())) {
        done(std::move(*current));
        return;
    }
    repository_->refreshToken(std::move(done));
}

} // namespace aedst::feature::auth
