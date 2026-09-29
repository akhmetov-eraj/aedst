#pragma once

#include "Domain/auth_repository.hpp"
#include "Domain/session.hpp"
#include "aedst/core/use_case.hpp"

#include <memory>
#include <optional>

namespace aedst::feature::auth {

/// Validates credentials locally (cheap, instant feedback) before calling the
/// server's aedst.auth.v1.AuthService/Login.
class LoginUseCase final : public core::UseCase<Credentials, Session> {
public:
    explicit LoginUseCase(std::shared_ptr<AuthRepository> repository);

    void execute(Credentials credentials, core::Completion<Session> done) override;

    [[nodiscard]] static std::optional<core::Error> validate(const Credentials& credentials);

private:
    std::shared_ptr<AuthRepository> repository_;
};

} // namespace aedst::feature::auth
