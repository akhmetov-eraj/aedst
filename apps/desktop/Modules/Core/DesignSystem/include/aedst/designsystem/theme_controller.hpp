#pragma once

#include <QObject>

namespace aedst::designsystem {

/// Runtime light/dark state for the design system (read through the `Theme`
/// singleton in qml/AedstDesignSystem/Theme.qml). The token values themselves
/// (colors, spacing, radius, typography, motion) are generated QML constants
/// (AedstTokens.qml, from packages/design-tokens) — this class only decides
/// which palette is active. Follows the OS light/dark scheme unless a value is
/// set explicitly.
class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ isDark WRITE setDark NOTIFY themeChanged)
    Q_PROPERTY(bool followsSystem READ followsSystem NOTIFY themeChanged)

public:
    explicit ThemeController(QObject* parent = nullptr);

    [[nodiscard]] bool isDark() const noexcept;
    void setDark(bool dark);
    [[nodiscard]] bool followsSystem() const noexcept;
    Q_INVOKABLE void followSystem();

signals:
    void themeChanged();

private:
    void applySystemScheme();

    bool dark_{false};
    bool followsSystem_{true};
};

/// Registers `controller` as the `ThemeController` singleton in the
/// `Aedst.Theme` QML module, which Theme.qml imports. Call before loading QML.
void registerThemeController(ThemeController* controller);

} // namespace aedst::designsystem
