#include "app_dependencies.hpp"
#include "startup_trace.hpp"
#include "aedst/core/logger.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJSEngine>
#include <QNetworkProxyFactory>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QUuid>
#include <QVariantMap>

#include <cstdlib>
#include <filesystem>

namespace {

using aedst::core::AedstLogger;

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

/// QML singletons returned from a factory are owned by the QML engine unless
/// told otherwise; these are owned by AppDependencies.
template <typename T>
void registerViewModel(const char* uri, const char* name, T& instance) {
    qmlRegisterSingletonType<T>(uri, 1, 0, name, [&instance](QQmlEngine*, QJSEngine*) -> QObject* {
        QJSEngine::setObjectOwnership(&instance, QJSEngine::CppOwnership);
        return &instance;
    });
}

} // namespace

int main(int argc, char* argv[]) {
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

    // AEDST's own log (spdlog). Qt's qDebug/QLoggingCategory output is left
    // alone for framework messages — see AedstLogger's header comment.
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

    const aedst::app::AppConfig config{
        .apiBaseUrl = QUrl(qEnvironmentVariable("AEDST_API_BASE_URL", QStringLiteral("http://localhost:8080"))),
        .dataDirectory = dataDirectory,
        .deviceId = deviceId().toStdString(),
    };

    auto dependencies = aedst::app::AppDependencies::create(config);
    if (!dependencies) {
        AedstLogger::critical("Startup failed: {}", dependencies.error().message);
        AedstLogger::shutdown();
        return EXIT_FAILURE;
    }
    // Declared before the engine so it outlives every QML object that references it.
    aedst::app::AppDependencies& deps = **dependencies;
    startup.mark("dependencies ready");

    aedst::designsystem::registerThemeController(&deps.theme());
    qmlRegisterSingletonInstance("Aedst.Navigation", 1, 0, "AppRouter", &deps.router());
    registerViewModel("Aedst.Feature.Auth", "LoginViewModel", deps.loginViewModel());

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() {
            AedstLogger::critical("Failed to create the QML root object");
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);

    engine.setInitialProperties(
        {{QStringLiteral("initialRoute"), QString::fromStdString(deps.initialRoute().name)}});
    engine.loadFromModule("Aedst.App", "Main");
    startup.mark("QML loaded");

    // The render thread emits frameSwapped; the single-shot connection is delivered on the GUI thread.
    if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0))) {
        QObject::connect(
            window, &QQuickWindow::frameSwapped, &app, [&startup]() { startup.mark("first frame"); },
            Qt::SingleShotConnection);
    }

    deps.prewarmNetwork();
    deps.refreshSessionIfNeeded();

    const int exitCode = QGuiApplication::exec();
    AedstLogger::info("AEDST desktop exiting with code {}", exitCode);
    AedstLogger::shutdown();
    return exitCode;
}
