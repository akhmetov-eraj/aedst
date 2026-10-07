#include "app_dependencies.hpp"
#include "startup_trace.hpp"
#include "Presentation/admin_view_model.hpp"
#include "Presentation/media_library_view_model.hpp"
#include "Presentation/media_viewer_view_model.hpp"
#include "Presentation/notifications_view_model.hpp"
#include "Presentation/search_view_model.hpp"
#include "Presentation/task_detail_view_model.hpp"
#include "Presentation/task_list_view_model.hpp"
#include "aedst/connectivity/connectivity_monitor.hpp"
#include "aedst/core/logger.hpp"
#include "aedst/crash/consent_store.hpp"
#include "aedst/crash/crash_handler.hpp"
#include "aedst/designsystem/translation_loader.hpp"
#include "aedst/media/media_image_provider.hpp"
#include "aedst/navigation/route.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/platform/deep_link.hpp"
#include "aedst/platform/notification_service.hpp"
#include "aedst/platform/single_instance_guard.hpp"
#include "aedst/platform/tray_notification_service.hpp"
#include "aedst/telemetry/qt_log_bridge.hpp"
#include "aedst/telemetry/sentry_reporter.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QJSEngine>
#include <QNetworkProxyFactory>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QUuid>
#include <QVariantMap>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

using aedst::core::AedstLogger;

constexpr const char* kAppId = "com.aedst.Desktop";

/// `--version` is answered before any Qt object exists: no display, no data
/// directory, no lock. The installer verification scripts rely on this.
bool wantsVersionOnly(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            return true;
        }
    }
    return false;
}

/// Stable per-install id sent as LoginRequest.device_id, so the server can
/// scope sessions per device.
QString deviceId() {
    QSettings settings;
    QString id = settings.value(QStringLiteral("deviceId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("deviceId"), id);
    }
    return id;
}

/// The API endpoint. Debug builds default to the local dev server; a Release
/// build has no default, so a missing/insecure AEDST_API_BASE_URL fails fast in
/// validateClientConfig() instead of silently talking plain http.
QUrl apiBaseUrl() {
#ifdef NDEBUG
    return QUrl(qEnvironmentVariable("AEDST_API_BASE_URL"));
#else
    return QUrl(qEnvironmentVariable("AEDST_API_BASE_URL", QStringLiteral("http://localhost:8080")));
#endif
}

/// Runtime environment wins over the value baked in at build time (CMake cache
/// variables AEDST_SENTRY_DSN / AEDST_POSTHOG_KEY / AEDST_POSTHOG_HOST).
QString configured(const char* variable, const char* buildDefault) {
    const QString value = qEnvironmentVariable(variable).trimmed();
    return value.isEmpty() ? QString::fromUtf8(buildDefault) : value;
}

/// Random per-install id for anonymous analytics (never derived from hardware).
QString analyticsAnonymousId() {
    QSettings settings;
    QString id = settings.value(QStringLiteral("analyticsAnonymousId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("analyticsAnonymousId"), id);
    }
    return id;
}

/// crashpad_handler is deployed next to the executable (App/CMakeLists.txt).
std::filesystem::path crashpadHandlerPath() {
#ifdef _WIN32
    const QString name = QStringLiteral("crashpad_handler.exe");
#else
    const QString name = QStringLiteral("crashpad_handler");
#endif
    return std::filesystem::path(
        QDir(QCoreApplication::applicationDirPath()).filePath(name).toStdU16String());
}

/// QML singletons returned from a factory are owned by the QML engine unless
/// told otherwise; these are owned by AppDependencies.
template <typename T>
void registerViewModel(const char* uri, const char* name, T& instance) {
    qmlRegisterSingletonType<T>(uri, 1, 0, name, [&instance](QQmlEngine*, QJSEngine*) -> QObject* {
        QJSEngine::setObjectOwnership(&instance, QJSEngine::CppOwnership);
        return &instance;
    });
}

/// Acts on any deep link among `arguments` (each is validated against the
/// allow-list before anything acts on it) and, when a window is given, brings
/// it to the front. Arguments were already stripped of control characters by
/// SingleInstanceGuard.
void handleActivation(const QStringList& arguments, const QPointer<QQuickWindow>& window,
                      aedst::navigation::Router* router) {
    for (const QString& argument : arguments) {
        const std::string text = argument.toStdString();
        if (!aedst::platform::looksLikeDeepLink(text)) {
            continue;
        }
        if (auto link = aedst::platform::parseDeepLink(text)) {
            AedstLogger::info("Deep link received (route={})", link->route);
            // The allow-listed route maps onto the same relative links the server
            // uses ("/tasks/<id>"); Router::openLink ignores anything else.
            if (router != nullptr) {
                router->openLink(QStringLiteral("/") + QString::fromStdString(link->route));
            }
        } else {
            AedstLogger::warn("Deep link rejected: {}", link.error().message);
        }
    }
    if (window) {
        window->show();
        window->raise();
        window->requestActivate();
    }
}

void shutdownTelemetry() {
    aedst::telemetry::SentryReporter::shutdown();
    aedst::crash::CrashHandler::uninstall();
    aedst::telemetry::uninstallQtLogBridge();
    AedstLogger::shutdown();
}

} // namespace

int main(int argc, char* argv[]) {
    if (wantsVersionOnly(argc, argv)) {
        std::puts(AEDST_VERSION);
        return EXIT_SUCCESS;
    }

    const aedst::app::StartupTrace startup;
    // QApplication (not QGuiApplication): QSystemTrayIcon - native desktop
    // notifications on Windows - lives in Qt Widgets and requires it. The UI
    // stays 100% Qt Quick; see docs/adr/0022-telemetry-media-notifications.md.
    QApplication app(argc, argv);
    // Honour the OS proxy configuration (corporate/PAC proxies) for all Qt Network traffic.
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QGuiApplication::setOrganizationName(QStringLiteral("AEDST"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("aedst.com"));
    QGuiApplication::setApplicationName(QStringLiteral("AEDST"));
    QGuiApplication::setApplicationVersion(QStringLiteral(AEDST_VERSION));

    const std::filesystem::path dataDirectory(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdU16String());

    // AEDST's own log (spdlog; secrets redacted by default). Qt/QML warnings are
    // copied into it by the Qt log bridge (both loggers stay; the bridge only makes
    // the support log complete) - see aedst/telemetry/qt_log_bridge.hpp.
    aedst::core::LoggerConfig logConfig;
#ifdef NDEBUG
    logConfig.level = aedst::core::LogLevel::Info;
#else
    logConfig.level = aedst::core::LogLevel::Debug;
#endif
    logConfig.logFile = dataDirectory / "logs" / "aedst.log";
    AedstLogger::init(logConfig);
    aedst::telemetry::installQtLogBridge();
    AedstLogger::info("AEDST desktop {} starting", AEDST_VERSION);
    startup.mark("logger ready");

    // Crash reporting. With a Sentry DSN: sentry-native + crashpad (out-of-process
    // minidumps, uploaded only after the user consented). Without one: the local
    // CrashHandler (minidump / signal report under <data>/crashes, never uploaded
    // without consent). Never both: they would fight over the same OS hooks.
    const QString sentryDsn = configured("AEDST_SENTRY_DSN", AEDST_SENTRY_DSN_DEFAULT);
    const aedst::crash::FileConsentStore crashConsent(dataDirectory / "crash_consent");
    const bool sentryActive = aedst::telemetry::SentryReporter::start(
        aedst::telemetry::SentryConfig{
            .dsn = sentryDsn.toStdString(),
            .release = std::string("aedst-desktop@") + AEDST_VERSION,
            .environment = configured("AEDST_SENTRY_ENVIRONMENT", AEDST_SENTRY_ENVIRONMENT_DEFAULT).toStdString(),
            .databasePath = dataDirectory / "sentry",
            .handlerPath = crashpadHandlerPath(),
            .debug = false,
        },
        crashConsent.state() == aedst::crash::ConsentState::Granted);
    if (!sentryActive) {
        if (auto installed = aedst::crash::CrashHandler::install(dataDirectory, AEDST_VERSION); !installed) {
            AedstLogger::warn("Crash handler unavailable: {}", installed.error().message);
        }
    }

    // One instance per user: a second launch forwards its arguments (deep links)
    // to the first and exits before it can open the database.
    aedst::platform::SingleInstanceGuard instanceGuard(QString::fromUtf8(kAppId),
                                                       QString::fromStdU16String(dataDirectory.u16string()));
    if (instanceGuard.acquire(app.arguments()) == aedst::platform::SingleInstanceGuard::Role::Secondary) {
        AedstLogger::info("Another instance is running; activation forwarded, exiting");
        shutdownTelemetry();
        return EXIT_SUCCESS;
    }

    aedst::app::AppConfig config{
        .apiBaseUrl = apiBaseUrl(),
        .dataDirectory = dataDirectory,
        .deviceId = deviceId().toStdString(),
        .posthog = {},
        .crashReportingConfigured = sentryActive,
        .rolesClaim = configured("AEDST_OIDC_ROLES_CLAIM", "roles").toStdString(),
    };
    config.posthog.apiKey = configured("AEDST_POSTHOG_KEY", AEDST_POSTHOG_KEY_DEFAULT);
    config.posthog.host = QUrl(configured("AEDST_POSTHOG_HOST", AEDST_POSTHOG_HOST_DEFAULT));
    config.posthog.anonymousId = analyticsAnonymousId();
    config.posthog.appVersion = QStringLiteral(AEDST_VERSION);

    // Fail fast on an insecure or malformed endpoint (https required; http only for
    // loopback in Debug or with AEDST_ALLOW_INSECURE_LOCALHOST).
    if (auto valid = aedst::network::validateClientConfig({.baseUrl = config.apiBaseUrl}); !valid) {
        AedstLogger::critical("Invalid API configuration: {} (set AEDST_API_BASE_URL to an https:// URL)",
                              valid.error().message);
        shutdownTelemetry();
        return EXIT_FAILURE;
    }

    // Notifications: native desktop (tray toast / Notification Center / D-Bus) with
    // the in-app AedstToast as universal fallback. Created before the dependencies
    // because NotificationsViewModel announces through it.
    aedst::platform::InAppNotificationService inAppNotifications;
    aedst::platform::TrayNotificationService trayNotifications(inAppNotifications);
    const auto notifications = aedst::platform::makeNotificationService(inAppNotifications, &trayNotifications);

    auto dependencies = aedst::app::AppDependencies::create(config, *notifications);
    if (!dependencies) {
        AedstLogger::critical("Startup failed: {}", dependencies.error().message);
        shutdownTelemetry();
        return EXIT_FAILURE;
    }
    // Declared before the engine so it outlives every QML object that references it.
    aedst::app::AppDependencies& deps = **dependencies;
    startup.mark("dependencies ready");

    // Reachability (Offline banner / disabled submit) and UI language + RTL layout.
    auto connectivity = aedst::connectivity::ConnectivityMonitor::createDefault();
    aedst::designsystem::TranslationLoader translations(
        {QStringLiteral("designsystem"), QStringLiteral("auth"), QStringLiteral("tasks"), QStringLiteral("search"),
         QStringLiteral("notifications"), QStringLiteral("media"), QStringLiteral("admin"), QStringLiteral("app")});
    translations.installSystemLanguage();

    aedst::designsystem::registerThemeController(&deps.theme());
    qmlRegisterSingletonInstance("Aedst.Navigation", 1, 0, "AppRouter", &deps.router());
    qmlRegisterSingletonInstance("Aedst.Connectivity", 1, 0, "Connectivity", connectivity.get());
    qmlRegisterSingletonInstance("Aedst.Platform", 1, 0, "Notifications", &inAppNotifications);
    qmlRegisterSingletonInstance("Aedst.Session", 1, 0, "Session", &deps.session());
    qmlRegisterSingletonInstance("Aedst.Telemetry", 1, 0, "Errors", &deps.errorCenter());
    qmlRegisterSingletonInstance("Aedst.Telemetry", 1, 0, "Telemetry", &deps.telemetrySettings());
    qmlRegisterSingletonInstance("Aedst.Telemetry", 1, 0, "FeatureFlags", &deps.analytics());
    registerViewModel("Aedst.Feature.Auth", "LoginViewModel", deps.loginViewModel());
    registerViewModel("Aedst.Feature.Tasks", "TaskListViewModel", deps.taskListViewModel());
    registerViewModel("Aedst.Feature.Tasks", "TaskDetailViewModel", deps.taskDetailViewModel());
    registerViewModel("Aedst.Feature.Search", "SearchViewModel", deps.searchViewModel());
    registerViewModel("Aedst.Feature.Notifications", "NotificationsViewModel", deps.notificationsViewModel());
    registerViewModel("Aedst.Feature.Media", "MediaLibraryViewModel", deps.mediaLibraryViewModel());
    registerViewModel("Aedst.Feature.Media", "MediaViewerViewModel", deps.mediaViewerViewModel());
    registerViewModel("Aedst.Feature.Admin", "AdminViewModel", deps.adminViewModel());

    // image://media - async, cached thumbnails/previews. The fetcher (GUI thread,
    // owns the disk-cached QNetworkAccessManager) must outlive the engine.
    aedst::media::MediaImageFetcher imageFetcher(
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("media")),
        256LL * 1024 * 1024);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("media"), new aedst::media::MediaImageProvider(imageFetcher));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() {
            AedstLogger::critical("Failed to create the QML root object");
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);
    QObject::connect(&translations, &aedst::designsystem::TranslationLoader::languageChanged, &engine,
                     [&engine]() { engine.retranslate(); });

    engine.setInitialProperties(
        {{QStringLiteral("initialRoute"), QString::fromStdString(deps.initialRoute().name)}});
    engine.loadFromModule("Aedst.App", "Main");
    startup.mark("QML loaded");

    const QPointer<QQuickWindow> window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    if (window) {
        // The render thread emits frameSwapped; the single-shot connection is delivered on the GUI thread.
        QObject::connect(
            window.data(), &QQuickWindow::frameSwapped, &app, [&startup]() { startup.mark("first frame"); },
            Qt::SingleShotConnection);
    }
    aedst::navigation::Router* router = &deps.router();
    QObject::connect(&instanceGuard, &aedst::platform::SingleInstanceGuard::activationRequested, &app,
                     [window, router](const QStringList& arguments) { handleActivation(arguments, window, router); });
    // Deep links passed to this (first) instance on its own command line.
    handleActivation(aedst::platform::SingleInstanceGuard::sanitizeArguments(app.arguments()),
                     QPointer<QQuickWindow>(), router);

    // Tray: clicking a notification (or the icon) brings the window forward; a
    // message click also opens the inbox. The badge mirrors the unread count.
    auto& notificationsViewModel = deps.notificationsViewModel();
    QObject::connect(&trayNotifications, &aedst::platform::TrayNotificationService::messageClicked, &app,
                     [window, router, &deps]() {
                         handleActivation({}, window, nullptr);
                         if (deps.session().signedIn()) {
                             router->navigate(QString::fromUtf8(aedst::navigation::routes::kNotifications.data(),
                                                                static_cast<qsizetype>(
                                                                    aedst::navigation::routes::kNotifications.size())));
                         }
                     });
    QObject::connect(&trayNotifications, &aedst::platform::TrayNotificationService::activated, &app,
                     [window]() { handleActivation({}, window, nullptr); });
    QObject::connect(&notificationsViewModel, &aedst::feature::notifications::NotificationsViewModel::unreadCountChanged,
                     &trayNotifications, [&trayNotifications, &notificationsViewModel]() {
                         trayNotifications.setBadgeCount(notificationsViewModel.unreadCount());
                     });
    // Poll faster while the user is looking at the app.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &notificationsViewModel,
                     [&notificationsViewModel](Qt::ApplicationState state) {
                         notificationsViewModel.setForeground(state == Qt::ApplicationActive);
                     });

    deps.prewarmNetwork();
    deps.refreshSessionIfNeeded();
    deps.resumeSession();

    const int exitCode = QApplication::exec();
    AedstLogger::info("AEDST desktop exiting with code {}", exitCode);
    deps.analytics().flush();
    shutdownTelemetry();
    return exitCode;
}
