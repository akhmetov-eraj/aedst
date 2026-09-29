#pragma once

#include "Domain/session.hpp"
#include "aedst/core/outcome.hpp"

#include <optional>
#include <string>

namespace aedst::feature::auth {

struct Credentials {
    std::string email;
    std::string password;
};

/// Domain-owned port; Data/AuthRepositoryImpl implements it. Presentation only
/// ever reaches this through a use case.
class AuthRepository {
public:
    AuthRepository() = default;
    AuthRepository(const AuthRepository&) = delete;
    AuthRepository& operator=(const AuthRepository&) = delete;
    AuthRepository(AuthRepository&&) = delete;
    AuthRepository& operator=(AuthRepository&&) = delete;
    virtual ~AuthRepository() = default;

    virtual void login(const Credentials& credentials, core::Completion<Session> done) = 0;
    virtual void refreshToken(core::Completion<Session> done) = 0;
    /// Always ends signed out locally, even if the server call fails.
    virtual void logout(core::Completion<void> done) = 0;
    [[nodiscard]] virtual std::optional<Session> currentSession() const = 0;
};

} // namespace aedst::feature::auth
