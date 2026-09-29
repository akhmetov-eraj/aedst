#pragma once

#include "Data/auth_repository_impl.hpp"
#include "Data/local_token_store_adapter.hpp"
#include "Domain/refresh_token_use_case.hpp"
#include "Presentation/login_view_model.hpp"
#include "aedst/analytics/analytics_tracker.hpp"
#include "aedst/core/clock.hpp"
#include "aedst/core/outcome.hpp"
#include "aedst/designsystem/theme_controller.hpp"
#include "aedst/navigation/nav_graph.hpp"
#include "aedst/navigation/route.hpp"
#include "aedst/navigation/router.hpp"
#include "aedst/network/api_client.hpp"
#include "aedst/network/auth_interceptor.hpp"
#include "aedst/persistence/secure_storage.hpp"
#include "aedst/persistence/sqlite_connection.hpp"
#include "aedst/persistence/token_store.hpp"

#include <QUrl>

#include <filesystem>
#include <memory>
#include <string>

namespace aedst::app {

struct AppConfig {
    QUrl apiBaseUrl;
    std::filesystem::path dataDirectory;
    std::string deviceId;
};

/// Manual DI composition root — the desktop counterpart of apps/apple's
/// AppDependencies.swift, apps/android's app module, and cmd/server wiring
/// every module. The only class that constructs concrete Data types and sees
/// every feature; features only ever receive interfaces.
///
/// Members are declared infrastructure-first, so reverse-order destruction
/// tears down ViewModels first and the SQLite connection last.
class AppDependencies final {
public:
    [[nodiscard]] static core::Outcome<std::unique_ptr<AppDependencies>> create(const AppConfig& config);

    AppDependencies(const AppDependencies&) = delete;
    AppDependencies& operator=(const AppDependencies&) = delete;
    AppDependencies(AppDependencies&&) = delete;
    AppDependencies& operator=(AppDependencies&&) = delete;
    ~AppDependencies();

    [[nodiscard]] navigation::Router& router() noexcept;
    [[nodiscard]] designsystem::ThemeController& theme() noexcept;
    [[nodiscard]] feature::auth::LoginViewModel& loginViewModel() noexcept;

    /// Login when no session is stored; auth's own landing state otherwise
    /// until a real post-login destination exists (Users/Profile are gone and
    /// their replacement, a future "tasks" module, isn't built on desktop yet).
    [[nodiscard]] navigation::Route initialRoute() const;

    /// Rotates an expired access token once at startup; routes to login if the
    /// server rejects the refresh token. Offline failures keep the stored session.
    void refreshSessionIfNeeded();

private:
    AppDependencies() = default;

    // Infrastructure (Core)
    std::shared_ptr<persistence::SqliteConnection> database_;
    std::shared_ptr<persistence::SecureStorage> secureStorage_;
    std::shared_ptr<persistence::TokenStore> tokenStore_;
    std::shared_ptr<core::Clock> clock_;
    std::shared_ptr<analytics::AnalyticsTracker> analytics_;
    std::shared_ptr<navigation::NavGraph> navGraph_;
    std::unique_ptr<navigation::Router> router_;
    std::unique_ptr<designsystem::ThemeController> theme_;

    // Auth
    std::shared_ptr<feature::auth::LocalTokenStoreAdapter> tokenAdapter_;
    std::shared_ptr<network::AuthInterceptor> authInterceptor_;
    std::shared_ptr<network::ApiClient> apiClient_;
    std::shared_ptr<feature::auth::AuthRepositoryImpl> authRepository_;
    std::shared_ptr<feature::auth::RefreshTokenUseCase> refreshToken_;

    // ViewModels (QML singletons)
    std::unique_ptr<feature::auth::LoginViewModel> loginViewModel_;
};

} // namespace aedst::app
