#include "aedst/logging/log_categories.hpp"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

Q_LOGGING_CATEGORY(lcApp, "aedst.app")
Q_LOGGING_CATEGORY(lcNetwork, "aedst.network")
Q_LOGGING_CATEGORY(lcPlatform, "aedst.platform")
Q_LOGGING_CATEGORY(lcMedia, "aedst.media")
Q_LOGGING_CATEGORY(lcNavigation, "aedst.navigation")
Q_LOGGING_CATEGORY(lcTelemetry, "aedst.telemetry")
Q_LOGGING_CATEGORY(lcAuth, "aedst.auth")

namespace aedst::logging {

void applyLogRules() {
    const QByteArray rules = qgetenv("AEDST_LOG_RULES");
    if (rules.isEmpty()) {
        return;
    }
    // Appended after QT_LOGGING_RULES / .qtlogging.ini so the AEDST variable wins.
    QLoggingCategory::setFilterRules(QString::fromUtf8(rules).replace(QLatin1Char(';'), QLatin1Char('\n')));
}

} // namespace aedst::logging
