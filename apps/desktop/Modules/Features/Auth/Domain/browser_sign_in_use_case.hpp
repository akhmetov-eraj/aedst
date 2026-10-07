#pragma once

#include "Domain/auth_repository.hpp"
#include "Domain/session.hpp"
#include "aedst/core/use_case.hpp"

#include <memory>

namespace aedst::feature::auth {

/// Signs the user in through the identity provider in the system browser. There is nothing to
/// validate locally (no credentials pass through the app); cancel() abandons the browser step.
class BrowserSignInUseCase final : public core::UseCase<core::NoInput, Session> {
public:
    explicit BrowserSignInUseCase(std::shared_ptr<AuthRepository> repository);

    void execute(core::NoInput input, core::Completion<Session> done) override;
    void cancel();
    [[nodiscard]] bool isAvailable() const;

private:
    std::shared_ptr<AuthRepository> repository_;
};

} // namespace aedst::feature::auth
