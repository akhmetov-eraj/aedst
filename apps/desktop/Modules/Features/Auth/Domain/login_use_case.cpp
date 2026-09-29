#include "Domain/login_use_case.hpp"

#include <string_view>
#include <utility>

namespace aedst::feature::auth {

LoginUseCase::LoginUseCase(std::shared_ptr<AuthRepository> repository) : repository_(std::move(repository)) {}

void LoginUseCase::execute(Credentials credentials, core::Completion<Session> done) {
    if (auto error = validate(credentials)) {
        done(core::fail(std::move(*error)));
        return;
    }
    repository_->login(credentials, std::move(done));
}

std::optional<core::Error> LoginUseCase::validate(const Credentials& credentials) {
    const std::string_view email = credentials.email;
    if (email.empty()) {
        return core::Error::invalidArgument("Email is required");
    }
    // Same shape check the server's buf.validate `string.email` rule enforces,
    // loosely: one '@' with something on both sides. The server stays authoritative.
    const auto at = email.find('@');
    if (at == std::string_view::npos || at == 0 || at == email.size() - 1 ||
        email.find('@', at + 1) != std::string_view::npos) {
        return core::Error::invalidArgument("Enter a valid email address");
    }
    if (credentials.password.empty()) {
        return core::Error::invalidArgument("Password is required");
    }
    return std::nullopt;
}

} // namespace aedst::feature::auth
