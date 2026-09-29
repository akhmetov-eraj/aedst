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

// Semantic tokens below mirror apps/web/src/app/styles/globals.css 1:1 (the
// shadcn/ui "neutral" palette), converted from its oklch() definitions to
// sRGB hex so QColor can represent them without an oklch parser.

QColor ThemeController::background() const {
    return dark_ ? QColor(0x0A, 0x0A, 0x0A) : QColor(0xFF, 0xFF, 0xFF);
}

QColor ThemeController::foreground() const {
    return dark_ ? QColor(0xFA, 0xFA, 0xFA) : QColor(0x0A, 0x0A, 0x0A);
}

QColor ThemeController::card() const {
    return dark_ ? QColor(0x17, 0x17, 0x17) : QColor(0xFF, 0xFF, 0xFF);
}

QColor ThemeController::cardForeground() const {
    return foreground();
}

QColor ThemeController::popover() const {
    return dark_ ? QColor(0x1F, 0x1F, 0x1F) : QColor(0xFF, 0xFF, 0xFF);
}

QColor ThemeController::popoverForeground() const {
    return foreground();
}

QColor ThemeController::primary() const {
    return dark_ ? QColor(0xE5, 0xE5, 0xE5) : QColor(0x17, 0x17, 0x17);
}

QColor ThemeController::primaryForeground() const {
    return dark_ ? QColor(0x17, 0x17, 0x17) : QColor(0xFA, 0xFA, 0xFA);
}

QColor ThemeController::secondary() const {
    return dark_ ? QColor(0x26, 0x26, 0x26) : QColor(0xF5, 0xF5, 0xF5);
}

QColor ThemeController::secondaryForeground() const {
    return dark_ ? QColor(0xFA, 0xFA, 0xFA) : QColor(0x17, 0x17, 0x17);
}

QColor ThemeController::muted() const {
    return dark_ ? QColor(0x26, 0x26, 0x26) : QColor(0xF5, 0xF5, 0xF5);
}

QColor ThemeController::mutedForeground() const {
    return dark_ ? QColor(0xA3, 0xA3, 0xA3) : QColor(0x73, 0x73, 0x73);
}

QColor ThemeController::accent() const {
    return dark_ ? QColor(0x26, 0x26, 0x26) : QColor(0xF5, 0xF5, 0xF5);
}

QColor ThemeController::accentForeground() const {
    return dark_ ? QColor(0xFA, 0xFA, 0xFA) : QColor(0x17, 0x17, 0x17);
}

QColor ThemeController::destructive() const {
    return dark_ ? QColor(0xF8, 0x71, 0x71) : QColor(0xDC, 0x26, 0x26);
}

QColor ThemeController::destructiveForeground() const {
    return QColor(0xFA, 0xFA, 0xFA);
}

QColor ThemeController::border() const {
    return dark_ ? QColor(0xFF, 0xFF, 0xFF, 26) : QColor(0xE5, 0xE5, 0xE5);
}

QColor ThemeController::input() const {
    return dark_ ? QColor(0xFF, 0xFF, 0xFF, 38) : QColor(0xE5, 0xE5, 0xE5);
}

QColor ThemeController::ring() const {
    return dark_ ? QColor(0x73, 0x73, 0x73) : QColor(0xA3, 0xA3, 0xA3);
}

QColor ThemeController::chart1() const {
    return dark_ ? QColor(0xDE, 0xDE, 0xDE) : QColor(0xDE, 0xDE, 0xDE);
}

QColor ThemeController::chart2() const {
    return QColor(0x8C, 0x8C, 0x8C);
}

QColor ThemeController::chart3() const {
    return QColor(0x6B, 0x6B, 0x6B);
}

QColor ThemeController::chart4() const {
    return QColor(0x55, 0x55, 0x55);
}

QColor ThemeController::chart5() const {
    return QColor(0x3B, 0x3B, 0x3B);
}

int ThemeController::radiusSm() noexcept {
    return 6;
}

int ThemeController::radiusMd() noexcept {
    return 8;
}

int ThemeController::radiusLg() noexcept {
    return 10;
}

int ThemeController::radiusXl() noexcept {
    return 14;
}

int ThemeController::radius2xl() noexcept {
    return 18;
}

int ThemeController::spacingXs() noexcept {
    return 4;
}

int ThemeController::spacingSm() noexcept {
    return 8;
}

int ThemeController::spacingMd() noexcept {
    return 12;
}

int ThemeController::spacingLg() noexcept {
    return 16;
}

int ThemeController::spacingXl() noexcept {
    return 24;
}

int ThemeController::spacing2xl() noexcept {
    return 32;
}

void registerThemeController(ThemeController* controller) {
    qmlRegisterSingletonInstance<ThemeController>("Aedst.Theme", 1, 0, "ThemeController", controller);
}

} // namespace aedst::designsystem
