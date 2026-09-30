#include "app_dependencies.hpp"

#include "Data/connect_auth_remote_data_source.hpp"
#include "Domain/login_use_case.hpp"
#include "Domain/logout_use_case.hpp"
#include "Navigation/auth_destinations.hpp"
#include "Navigation/auth_route.hpp"
#include "aedst/analytics/no_op_analytics_tracker.hpp"
#include "aedst/core/logger.hpp"

#include <optional>
#include <system_error>
#include <utility>

namespace aedst::app {

using core::AedstLogger;
namespace auth = feature::auth;

AppDependencies::~AppDependencies() = default;

core::Outcome<std::unique_ptr<AppDependencies>> AppDependencies::create(const AppConfig& config) {
    // Private constructor, so no make_unique.
    std::unique_ptr<AppDependencies> deps(new AppDependencies());

    // --- Persistence ---------------------------------------------------------
    std::error_code ec;
    std::filesystem::create_directories(config.dataDirectory, ec);
    if (ec) {
        return core::fail(core::Error::storage("Cannot create data directory: " + ec.message()));
    }
    auto database = persistence::SqliteConnection::open(config.dataDirectory / "aedst.sqlite3");
    if (!database) {
        return core::fail(database.error());
    }
    deps->database_ = std::make_shared<persistence::SqliteConnection>(std::move(*database));
    deps->secureStorage_ = std::make_shared<persistence::SecureStorage>(config.dataDirectory / "secure.key");
    deps->tokenStore_ = std::make_shared<persistence::TokenStore>(deps->database_, deps->secureStorage_);
    if (auto migrated = deps->tokenStore_->migrate(); !migrated) {
        return core::fail(migrated.error());
    }

    // --- Cross-cutting ---------------------------------------------------------
    deps->clock_ = std::make_shared<core::SystemClock>();
    deps->analytics_ = std::make_shared<analytics::NoOpAnalyticsTracker>();
    deps->theme_ = std::make_unique<designsystem::ThemeController>();

    // --- Navigation: the only place every feature's destinations meet ---------
    deps->navGraph_ = std::make_shared<navigation::NavGraph>();
    auth::registerAuthDestinations(*deps->navGraph_);
    deps->router_ = std::make_unique<navigation::Router>(deps->navGraph_);

    // --- Network ---------------------------------------------------------------
    deps->tokenAdapter_ = std::make_shared<auth::LocalTokenStoreAdapter>(deps->tokenStore_);
    deps->authInterceptor_ = std::make_shared<network::AuthInterceptor>(deps->tokenAdapter_);
    deps->connectClient_ = std::make_shared<network::ConnectClient>(
        network::ClientConfig{.baseUrl = config.apiBaseUrl}, deps->authInterceptor_);

    // --- Auth ------------------------------------------------------------------
    deps->authRepository_ = std::make_shared<auth::AuthRepositoryImpl>(
        std::make_shared<auth::ConnectAuthRemoteDataSource>(deps->connectClient_), deps->tokenAdapter_, deps->clock_,
        config.deviceId);
    deps->refreshToken_ = std::make_shared<auth::RefreshTokenUseCase>(deps->authRepository_, deps->clock_);
    deps->loginViewModel_ = std::make_unique<auth::LoginViewModel>(
        std::make_shared<auth::LoginUseCase>(deps->authRepository_),
        std::make_shared<auth::LogoutUseCase>(deps->authRepository_), deps->analytics_, deps->router_.get());

    AedstLogger::info("Dependencies ready (api={}, data={})", config.apiBaseUrl.toString().toStdString(),
                      config.dataDirectory.string());
    return deps;
}

navigation::Router& AppDependencies::router() noexcept {
    return *router_;
}

designsystem::ThemeController& AppDependencies::theme() noexcept {
    return *theme_;
}

feature::auth::LoginViewModel& AppDependencies::loginViewModel() noexcept {
    return *loginViewModel_;
}

void AppDependencies::prewarmNetwork() {
    connectClient_->prewarm();
}

navigation::Route AppDependencies::initialRoute() const {
    // Users/Profile (and their stored-session landing page) are gone; there is
    // no post-login destination yet on desktop (a future "tasks" module will
    // eventually provide one), so every launch lands on login for now. A
    // stored session still keeps refreshSessionIfNeeded() from bouncing the
    // user, but the initial route itself no longer branches on it.
    return auth::loginRoute();
}

void AppDependencies::refreshSessionIfNeeded() {
    if (!authRepository_->currentSession()) {
        return;
    }
    refreshToken_->execute(core::NoInput{}, [router = router_.get()](core::Outcome<auth::Session> result) {
        if (result) {
            return;
        }
        if (result.error().code == core::ErrorCode::Unauthenticated) {
            AedstLogger::info("Stored session was rejected; returning to login");
            router->resetTo(auth::loginRoute());
            return;
        }
        AedstLogger::warn("Session refresh failed, keeping stored session: {}", result.error().message);
    });
}

} // namespace aedst::app
