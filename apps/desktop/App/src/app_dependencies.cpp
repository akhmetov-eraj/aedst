#include "app_dependencies.hpp"

#include "Data/connect_admin_repository.hpp"
#include "Data/connect_auth_remote_data_source.hpp"
#include "Data/connect_notification_repository.hpp"
#include "Data/connect_search_repository.hpp"
#include "Data/connect_task_repository.hpp"
#include "Domain/create_task_use_case.hpp"
#include "Domain/login_use_case.hpp"
#include "Domain/logout_use_case.hpp"
#include "Navigation/admin_destinations.hpp"
#include "Navigation/auth_destinations.hpp"
#include "Navigation/auth_route.hpp"
#include "Navigation/media_destinations.hpp"
#include "Navigation/notifications_destinations.hpp"
#include "Navigation/search_destinations.hpp"
#include "Navigation/tasks_destinations.hpp"
#include "Navigation/tasks_route.hpp"
#include "Presentation/admin_view_model.hpp"
#include "Presentation/media_library_view_model.hpp"
#include "Presentation/media_viewer_view_model.hpp"
#include "Presentation/notifications_view_model.hpp"
#include "Presentation/search_view_model.hpp"
#include "Presentation/task_detail_view_model.hpp"
#include "Presentation/task_list_view_model.hpp"
#include "aedst/core/logger.hpp"
#include "aedst/media/media_remote_data_source.hpp"
#include "aedst/network/access_token_claims.hpp"
#include "aedst/network/transport_policy.hpp"
#include "aedst/persistence/master_key_provider.hpp"
#include "aedst/telemetry/sentry_reporter.hpp"

#include <QObject>

#include <optional>
#include <system_error>
#include <utility>

namespace aedst::app {

using core::AedstLogger;
namespace auth = feature::auth;
namespace tasks = feature::tasks;
namespace search = feature::search;
namespace notifications = feature::notifications;
namespace mediafeature = feature::media;
namespace admin = feature::admin;

AppDependencies::~AppDependencies() = default;

core::Outcome<std::unique_ptr<AppDependencies>> AppDependencies::create(const AppConfig& config,
                                                                         platform::NotificationService& desktopNotifier) {
    // Private constructor, so no make_unique.
    std::unique_ptr<AppDependencies> deps(new AppDependencies());
    deps->rolesClaim_ = config.rolesClaim;

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
    // Master key lives in the OS credential store (DPAPI-wrapped file on Windows,
    // Secret Service on Linux) with a 0600 key file only as fallback.
    std::shared_ptr<persistence::MasterKeyProvider> keyProvider(
        persistence::createPlatformMasterKeyProvider(config.dataDirectory));
    deps->secureStorage_ = std::make_shared<persistence::SecureStorage>(std::move(keyProvider));
    deps->tokenStore_ = std::make_shared<persistence::TokenStore>(deps->database_, deps->secureStorage_);
    if (auto migrated = deps->tokenStore_->migrate(); !migrated) {
        return core::fail(migrated.error());
    }

    // --- Cross-cutting: telemetry (consent-gated), errors, theme ---------------
    deps->clock_ = std::make_shared<core::SystemClock>();
    deps->crashConsent_ = std::make_unique<crash::FileConsentStore>(config.dataDirectory / "crash_consent");
    deps->analyticsConsent_ = std::make_unique<crash::FileConsentStore>(config.dataDirectory / "analytics_consent");
    deps->analytics_ = std::make_shared<telemetry::PostHogAnalyticsTracker>(config.posthog);
    deps->telemetrySettings_ = std::make_unique<telemetry::TelemetrySettings>(
        *deps->crashConsent_, *deps->analyticsConsent_, *deps->analytics_, config.crashReportingConfigured);
    deps->telemetrySettings_->applyStoredDecisions();
    deps->errorCenter_ = std::make_shared<telemetry::ErrorCenter>();
    deps->theme_ = std::make_unique<designsystem::ThemeController>();
    deps->network_ = std::make_unique<QNetworkAccessManager>();
    deps->session_ = std::make_unique<SessionInfo>();

    // --- Navigation: the only place every feature's destinations meet ---------
    deps->navGraph_ = std::make_shared<navigation::NavGraph>();
    auth::registerAuthDestinations(*deps->navGraph_);
    tasks::registerTasksDestinations(*deps->navGraph_);
    search::registerSearchDestinations(*deps->navGraph_);
    notifications::registerNotificationsDestinations(*deps->navGraph_);
    mediafeature::registerMediaDestinations(*deps->navGraph_);
    admin::registerAdminDestinations(*deps->navGraph_);
    deps->router_ = std::make_unique<navigation::Router>(deps->navGraph_);

    // --- Network ---------------------------------------------------------------
    deps->tokenAdapter_ = std::make_shared<auth::LocalTokenStoreAdapter>(deps->tokenStore_);
    deps->authInterceptor_ = std::make_shared<network::AuthInterceptor>(deps->tokenAdapter_);
    deps->connectClient_ = std::make_shared<network::ConnectClient>(
        network::ClientConfig{.baseUrl = config.apiBaseUrl}, deps->authInterceptor_);
    deps->mediaRemote_ = std::make_shared<media::ConnectMediaRemoteDataSource>(deps->connectClient_);
    const media::MediaUploadController::Dependencies upload{
        .remote = deps->mediaRemote_,
        .network = deps->network_.get(),
        .errors = deps->errorCenter_,
        .allowInsecureLoopback = network::insecureLoopbackAllowedByDefault(),
        .compression = {},
    };
    navigation::Router* router = deps->router_.get();

    // --- Auth ------------------------------------------------------------------
    deps->authRepository_ = std::make_shared<auth::AuthRepositoryImpl>(
        std::make_shared<auth::ConnectAuthRemoteDataSource>(deps->connectClient_), deps->tokenAdapter_, deps->clock_,
        config.deviceId);
    deps->refreshToken_ = std::make_shared<auth::RefreshTokenUseCase>(deps->authRepository_, deps->clock_);
    deps->loginViewModel_ = std::make_unique<auth::LoginViewModel>(
        std::make_shared<auth::LoginUseCase>(deps->authRepository_),
        std::make_shared<auth::LogoutUseCase>(deps->authRepository_), deps->analytics_, router);

    // --- Tasks (+ attachments through Core media) ------------------------------
    auto taskRepository = std::make_shared<tasks::ConnectTaskRepository>(deps->connectClient_);
    deps->taskListViewModel_ = std::make_unique<tasks::TaskListViewModel>(
        taskRepository, std::make_shared<tasks::CreateTaskUseCase>(taskRepository), deps->errorCenter_,
        deps->analytics_, router);
    deps->taskDetailViewModel_ = std::make_unique<tasks::TaskDetailViewModel>(tasks::TaskDetailViewModel::Dependencies{
        .tasks = taskRepository,
        .media = deps->mediaRemote_,
        .errors = deps->errorCenter_,
        .upload = upload,
        .router = router,
    });

    // --- Search / Notifications / Media / Admin --------------------------------
    deps->searchViewModel_ = std::make_unique<search::SearchViewModel>(
        std::make_shared<search::ConnectSearchRepository>(deps->connectClient_), deps->errorCenter_, deps->analytics_,
        router);
    deps->notificationsViewModel_ = std::make_unique<notifications::NotificationsViewModel>(
        std::make_shared<notifications::ConnectNotificationRepository>(deps->connectClient_), deps->errorCenter_,
        deps->analytics_, &desktopNotifier, router);
    deps->mediaLibraryViewModel_ =
        std::make_unique<mediafeature::MediaLibraryViewModel>(mediafeature::MediaLibraryViewModel::Dependencies{
            .remote = deps->mediaRemote_,
            .errors = deps->errorCenter_,
            .analytics = deps->analytics_,
            .upload = upload,
            .router = router,
        });
    deps->mediaViewerViewModel_ =
        std::make_unique<mediafeature::MediaViewerViewModel>(deps->mediaRemote_, deps->errorCenter_);
    deps->adminViewModel_ = std::make_unique<admin::AdminViewModel>(
        std::make_shared<admin::ConnectAdminRepository>(deps->connectClient_), deps->errorCenter_, deps->analytics_);

    deps->wireSessionLifecycle();
    deps->updateSessionInfo();

    AedstLogger::info("Dependencies ready (api={}, data={})", config.apiBaseUrl.toString().toStdString(),
                      config.dataDirectory.string());
    return deps;
}

void AppDependencies::wireSessionLifecycle() {
    // Signal connections use the ViewModel as context: they die with it.
    QObject::connect(loginViewModel_.get(), &auth::LoginViewModel::loginSucceeded, loginViewModel_.get(),
                     [this](const QString&) { onSignedIn(); });
    QObject::connect(loginViewModel_.get(), &auth::LoginViewModel::loggedOut, loginViewModel_.get(),
                     [this]() { onSignedOut(); });
    // Any feature hitting `unauthenticated` (refresh token revoked, session expired
    // server-side) ends the session the same way an explicit sign-out does.
    QObject::connect(errorCenter_.get(), &telemetry::ErrorCenter::sessionExpired, errorCenter_.get(), [this]() {
        // isLoading: a sign-out is already running (several in-flight calls can fail at once).
        if (session_->signedIn() && !loginViewModel_->isLoading()) {
            AedstLogger::info("Session expired; signing out");
            loginViewModel_->logout(); // clears local tokens, emits loggedOut -> onSignedOut
        }
    });
}

void AppDependencies::onSignedIn() {
    updateSessionInfo();
    resumeSession();
    router_->resetTo(tasks::taskListRoute());
}

void AppDependencies::resumeSession() {
    if (const auto current = authRepository_->currentSession()) {
        telemetry::SentryReporter::setUser(current->userId); // opaque id only
        notificationsViewModel_->start();
    }
}

void AppDependencies::onSignedOut() {
    // Forget every user-specific bit of state so the next user starts clean.
    notificationsViewModel_->stop();
    taskListViewModel_->clear();
    taskDetailViewModel_->clear();
    searchViewModel_->clear();
    mediaLibraryViewModel_->clear();
    mediaViewerViewModel_->clear();
    adminViewModel_->clear();
    telemetry::SentryReporter::clearUser();
    updateSessionInfo();
    // LoginViewModel::logout() already reset the router to the login route.
}

void AppDependencies::updateSessionInfo() {
    const auto current = authRepository_->currentSession();
    bool isAdmin = false;
    if (current) {
        if (const auto claims = network::parseAccessTokenClaims(current->accessToken, rolesClaim_)) {
            isAdmin = claims->hasRole(admin::kAdminRole);
        }
    }
    session_->set(current.has_value(), isAdmin);
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

feature::tasks::TaskListViewModel& AppDependencies::taskListViewModel() noexcept {
    return *taskListViewModel_;
}

feature::tasks::TaskDetailViewModel& AppDependencies::taskDetailViewModel() noexcept {
    return *taskDetailViewModel_;
}

feature::search::SearchViewModel& AppDependencies::searchViewModel() noexcept {
    return *searchViewModel_;
}

feature::notifications::NotificationsViewModel& AppDependencies::notificationsViewModel() noexcept {
    return *notificationsViewModel_;
}

feature::media::MediaLibraryViewModel& AppDependencies::mediaLibraryViewModel() noexcept {
    return *mediaLibraryViewModel_;
}

feature::media::MediaViewerViewModel& AppDependencies::mediaViewerViewModel() noexcept {
    return *mediaViewerViewModel_;
}

feature::admin::AdminViewModel& AppDependencies::adminViewModel() noexcept {
    return *adminViewModel_;
}

telemetry::ErrorCenter& AppDependencies::errorCenter() noexcept {
    return *errorCenter_;
}

telemetry::TelemetrySettings& AppDependencies::telemetrySettings() noexcept {
    return *telemetrySettings_;
}

telemetry::PostHogAnalyticsTracker& AppDependencies::analytics() noexcept {
    return *analytics_;
}

SessionInfo& AppDependencies::session() noexcept {
    return *session_;
}

QNetworkAccessManager& AppDependencies::network() noexcept {
    return *network_;
}

void AppDependencies::prewarmNetwork() {
    connectClient_->prewarm();
}

navigation::Route AppDependencies::initialRoute() const {
    // A stored session lands on the signed-in home (Tasks); refreshSessionIfNeeded()
    // routes back to login if the server rejects it.
    return authRepository_->currentSession() ? tasks::taskListRoute() : auth::loginRoute();
}

void AppDependencies::refreshSessionIfNeeded() {
    if (!authRepository_->currentSession()) {
        return;
    }
    refreshToken_->execute(core::NoInput{}, [this](core::Outcome<auth::Session> result) {
        if (result) {
            updateSessionInfo(); // the rotated token may carry different roles
            return;
        }
        if (result.error().code == core::ErrorCode::Unauthenticated) {
            AedstLogger::info("Stored session was rejected; returning to login");
            onSignedOut();
            router_->resetTo(auth::loginRoute());
            return;
        }
        AedstLogger::warn("Session refresh failed, keeping stored session: {}", result.error().message);
    });
}

} // namespace aedst::app
