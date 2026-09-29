#pragma once

#include "Domain/auth_repository.hpp"
#include "aedst/core/use_case.hpp"

#include <memory>

namespace aedst::feature::auth {

class LogoutUseCase final : public core::UseCase<core::NoInput, void> {
public:
    explicit LogoutUseCase(std::shared_ptr<AuthRepository> repository);

    void execute(core::NoInput input, core::Completion<void> done) override;

private:
    std::shared_ptr<AuthRepository> repository_;
};

} // namespace aedst::feature::auth
