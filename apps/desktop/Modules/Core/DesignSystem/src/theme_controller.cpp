#include "aedst/designsystem/theme_controller.hpp"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QStyleHints>

namespace aedst::designsystem {

ThemeController::ThemeController(QObject* parent) : QObject(parent) {
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
            if (followsSystem_) {
                applySystemScheme();
            }
        });
        applySystemScheme();
    }
}

bool ThemeController::isDark() const noexcept {
    return dark_;
}

void ThemeController::setDark(bool dark) {
    const bool changed = dark_ != dark || followsSystem_;
    followsSystem_ = false;
    dark_ = dark;
    if (changed) {
        emit themeChanged();
    }
}

bool ThemeController::followsSystem() const noexcept {
    return followsSystem_;
}

void ThemeController::followSystem() {
    followsSystem_ = true;
    applySystemScheme();
    emit themeChanged();
}

void ThemeController::applySystemScheme() {
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
        return;
    }
    const bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (dark != dark_) {
        dark_ = dark;
        emit themeChanged();
    }
}

} // namespace aedst::designsystem
