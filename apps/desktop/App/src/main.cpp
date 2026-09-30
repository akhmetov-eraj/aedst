#include "app_dependencies.hpp"
#include "startup_trace.hpp"
#include "aedst/connectivity/connectivity_monitor.hpp"
#include "aedst/core/logger.hpp"
#include "aedst/crash/crash_handler.hpp"
#include "aedst/designsystem/translation_loader.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/platform/deep_link.hpp"
#include "aedst/platform/notification_service.hpp"
#include "aedst/platform/single_instance_guard.hpp"

#include <QCoreApplication>
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

/// QML singletons returned from a factory are owned by the QML engine unless
/// told otherwise; these are owned by AppDependencies.
template <typename T>
void registerViewModel(const char* uri, const char* name, T& instance) {
    qmlRegisterSingletonType<T>(uri, 1, 0, name, [&instance](QQmlEngine*, QJSEngine*) -> QObject* {
        QJSEngine::setObjectOwnership(&instance, QJSEngine::CppOwnership);
        return &instance;
    });
}

/// Logs any deep link among `arguments` (each is validated against the
/// allow-list before anything acts on it) and, when a window is given, brings
/// it to the front. Arguments were already stripped of control characters by
/// SingleInstanceGuard.
void handleActivation(const QStringList& arguments, const QPointer<QQuickWindow>& window) {
    for (const QString& argument : arguments) {
        const std::string text = argument.toStdString();
        if (!aedst::platform::looksLikeDeepLink(text)) {
            continue;
        }
        if (auto link = aedst::platform::parseDeepLink(text)) {
            // Routing a validated link into the Router is added with the first
            // feature that owns a deep-linkable destination.
            AedstLogger::info("Deep link received (route={})", link->route);
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

} // namespace

int main(int argc, char* argv[]) {
    if (wantsVersionOnly(argc, argv)) {
        std::puts(AEDST_VERSION);
        return EXIT_SUCCESS;
    }

    const aedst::app::StartupTrace startup;
    QGuiApplication app(argc, argv);
    // Honour the OS proxy configuration (corporate/PAC proxies) for all Qt Network traffic.
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QGuiApplication::setOrganizationName(QStringLiteral("AEDST"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("aedst.com"));
    QGuiApplication::setApplicationName(QStringLiteral("AEDST"));
    QGuiApplication::setApplicationVersion(QStringLiteral(AEDST_VERSION));

    const std::filesystem::path dataDirectory(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdU16String());

    // AEDST's own log (spdlog; secrets redacted by default). Qt's qDebug/QLoggingCategory
    // output is left alone for framework messages — see AedstLogger's header comment.
    aedst::core::LoggerConfig logConfig;
#ifdef NDEBUG
    logConfig.level = aedst::core::LogLevel::Info;
#else
    logConfig.level = aedst::core::LogLevel::Debug;
#endif
    logConfig.logFile = dataDirectory / "logs" / "aedst.log";
    AedstLogger::init(logConfig);
    AedstLogger::info("AEDST desktop {} starting", AEDST_VERSION);
    startup.mark("logger ready");

    // Local crash capture (minidump / signal report under <data>/crashes). Nothing
    // is uploaded without the user's consent (see Core/CrashReporting).
    if (auto installed = aedst::crash::CrashHandler::install(dataDirectory, AEDST_VERSION); !installed) {
        AedstLogger::warn("Crash handler unavailable: {}", installed.error().message);
    }

    // One instance per user: a second launch forwards its arguments (deep links)
    // to the first and exits before it can open the database.
    aedst::platform::SingleInstanceGuard instanceGuard(QString::fromUtf8(kAppId),
                                                       QString::fromStdU16String(dataDirectory.u16string()));
    if (instanceGuard.acquire(app.arguments()) == aedst::platform::SingleInstanceGuard::Role::Secondary) {
        AedstLogger::info("Another instance is running; activation forwarded, exiting");
        aedst::crash::CrashHandler::uninstall();
        AedstLogger::shutdown();
        return EXIT_SUCCESS;
    }

    const aedst::app::AppConfig config{
        .apiBaseUrl = apiBaseUrl(),
        .dataDirectory = dataDirectory,
        .deviceId = deviceId().toStdString(),
    };

    // Fail fast on an insecure or malformed endpoint (https required; http only for
    // loopback in Debug or with AEDST_ALLOW_INSECURE_LOCALHOST).
    if (auto valid = aedst::network::validateClientConfig({.baseUrl = config.apiBaseUrl}); !valid) {
        AedstLogger::critical("Invalid API configuration: {} (set AEDST_API_BASE_URL to an https:// URL)",
                              valid.error().message);
        aedst::crash::CrashHandler::uninstall();
        AedstLogger::shutdown();
        return EXIT_FAILURE;
    }

    auto dependencies = aedst::app::AppDependencies::create(config);
    if (!dependencies) {
        AedstLogger::critical("Startup failed: {}", dependencies.error().message);
        aedst::crash::CrashHandler::uninstall();
        AedstLogger::shutdown();
        return EXIT_FAILURE;
    }
    // Declared before the engine so it outlives every QML object that references it.
    aedst::app::AppDependencies& deps = **dependencies;
    startup.mark("dependencies ready");

    // Reachability (Offline banner / disabled submit) and UI language + RTL layout.
    auto connectivity = aedst::connectivity::ConnectivityMonitor::createDefault();
    aedst::designsystem::TranslationLoader translations(
        {QStringLiteral("designsystem"), QStringLiteral("auth"), QStringLiteral("app")});
    translations.installSystemLanguage();

    // In-app notifications; the native service (desktop D-Bus on Linux) is chosen
    // by the factory and falls back to the in-app one.
    aedst::platform::InAppNotificationService inAppNotifications;
    [[maybe_unused]] const auto notifications = aedst::platform::makeNotificationService(inAppNotifications);

    aedst::designsystem::registerThemeController(&deps.theme());
    qmlRegisterSingletonInstance("Aedst.Navigation", 1, 0, "AppRouter", &deps.router());
    qmlRegisterSingletonInstance("Aedst.Connectivity", 1, 0, "Connectivity", connectivity.get());
    qmlRegisterSingletonInstance("Aedst.Platform", 1, 0, "Notifications", &inAppNotifications);
    registerViewModel("Aedst.Feature.Auth", "LoginViewModel", deps.loginViewModel());

    QQmlApplicationEngine engine;
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
    QObject::connect(&instanceGuard, &aedst::platform::SingleInstanceGuard::activationRequested, &app,
                     [window](const QStringList& arguments) { handleActivation(arguments, window); });
    // Deep links passed to this (first) instance on its own command line.
    handleActivation(aedst::platform::SingleInstanceGuard::sanitizeArguments(app.arguments()),
                     QPointer<QQuickWindow>());

    deps.prewarmNetwork();
    deps.refreshSessionIfNeeded();

    const int exitCode = QGuiApplication::exec();
    AedstLogger::info("AEDST desktop exiting with code {}", exitCode);
    aedst::crash::CrashHandler::uninstall();
    AedstLogger::shutdown();
    return exitCode;
}
