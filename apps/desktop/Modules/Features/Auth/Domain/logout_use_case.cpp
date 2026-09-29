#include "Domain/logout_use_case.hpp"

#include <utility>

namespace aedst::feature::auth {

LogoutUseCase::LogoutUseCase(std::shared_ptr<AuthRepository> repository) : repository_(std::move(repository)) {}

void LogoutUseCase::execute(core::NoInput /*input*/, core::Completion<void> done) {
    repository_->logout(std::move(done));
}

} // namespace aedst::feature::auth
