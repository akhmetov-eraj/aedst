#pragma once

#include "Domain/auth_repository.hpp"
#include "Domain/session.hpp"
#include "aedst/core/clock.hpp"
#include "aedst/core/use_case.hpp"

#include <memory>

namespace aedst::feature::auth {

/// Returns a session with a usable access token: the stored one if it hasn't
/// expired yet, otherwise a freshly rotated one from RefreshToken.
class RefreshTokenUseCase final : public core::UseCase<core::NoInput, Session> {
public:
    RefreshTokenUseCase(std::shared_ptr<AuthRepository> repository, std::shared_ptr<const core::Clock> clock);

    void execute(core::NoInput input, core::Completion<Session> done) override;

private:
    std::shared_ptr<AuthRepository> repository_;
    std::shared_ptr<const core::Clock> clock_;
};

} // namespace aedst::feature::auth
