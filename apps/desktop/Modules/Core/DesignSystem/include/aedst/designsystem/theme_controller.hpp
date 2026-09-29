#pragma once

#include <QColor>
#include <QObject>

namespace aedst::designsystem {

/// Design tokens exposed to QML (read through the `Theme` singleton in
/// qml/AedstDesignSystem/Theme.qml). Mirrors the semantic token set defined
/// for the web app in apps/web/src/app/styles/globals.css (shadcn/ui
/// "neutral" palette) so every AEDST client shares one visual language.
/// Follows the OS light/dark scheme unless a value is set explicitly.
class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ isDark WRITE setDark NOTIFY themeChanged)
    Q_PROPERTY(bool followsSystem READ followsSystem NOTIFY themeChanged)

    // Semantic color tokens -- one Q_PROPERTY per CSS custom property on web.
    Q_PROPERTY(QColor background READ background NOTIFY themeChanged)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY themeChanged)
    Q_PROPERTY(QColor card READ card NOTIFY themeChanged)
    Q_PROPERTY(QColor cardForeground READ cardForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor popover READ popover NOTIFY themeChanged)
    Q_PROPERTY(QColor popoverForeground READ popoverForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor primary READ primary NOTIFY themeChanged)
    Q_PROPERTY(QColor primaryForeground READ primaryForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY themeChanged)
    Q_PROPERTY(QColor secondaryForeground READ secondaryForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor muted READ muted NOTIFY themeChanged)
    Q_PROPERTY(QColor mutedForeground READ mutedForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentForeground READ accentForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor destructive READ destructive NOTIFY themeChanged)
    Q_PROPERTY(QColor destructiveForeground READ destructiveForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
    Q_PROPERTY(QColor input READ input NOTIFY themeChanged)
    Q_PROPERTY(QColor ring READ ring NOTIFY themeChanged)

    // Chart palette (5 categorical colors), mirroring --chart-1..5 on web.
    Q_PROPERTY(QColor chart1 READ chart1 NOTIFY themeChanged)
    Q_PROPERTY(QColor chart2 READ chart2 NOTIFY themeChanged)
    Q_PROPERTY(QColor chart3 READ chart3 NOTIFY themeChanged)
    Q_PROPERTY(QColor chart4 READ chart4 NOTIFY themeChanged)
    Q_PROPERTY(QColor chart5 READ chart5 NOTIFY themeChanged)

    // Scales (device-independent pixels). CONSTANT: same in light/dark.
    Q_PROPERTY(int radiusSm READ radiusSm CONSTANT)
    Q_PROPERTY(int radiusMd READ radiusMd CONSTANT)
    Q_PROPERTY(int radiusLg READ radiusLg CONSTANT)
    Q_PROPERTY(int radiusXl READ radiusXl CONSTANT)
    Q_PROPERTY(int radius2xl READ radius2xl CONSTANT)
    Q_PROPERTY(int spacingXs READ spacingXs CONSTANT)
    Q_PROPERTY(int spacingSm READ spacingSm CONSTANT)
    Q_PROPERTY(int spacingMd READ spacingMd CONSTANT)
    Q_PROPERTY(int spacingLg READ spacingLg CONSTANT)
    Q_PROPERTY(int spacingXl READ spacingXl CONSTANT)
    Q_PROPERTY(int spacing2xl READ spacing2xl CONSTANT)

public:
    explicit ThemeController(QObject* parent = nullptr);

    [[nodiscard]] bool isDark() const noexcept;
    void setDark(bool dark);
    [[nodiscard]] bool followsSystem() const noexcept;
    Q_INVOKABLE void followSystem();

    [[nodiscard]] QColor background() const;
    [[nodiscard]] QColor foreground() const;
    [[nodiscard]] QColor card() const;
    [[nodiscard]] QColor cardForeground() const;
    [[nodiscard]] QColor popover() const;
    [[nodiscard]] QColor popoverForeground() const;
    [[nodiscard]] QColor primary() const;
    [[nodiscard]] QColor primaryForeground() const;
    [[nodiscard]] QColor secondary() const;
    [[nodiscard]] QColor secondaryForeground() const;
    [[nodiscard]] QColor muted() const;
    [[nodiscard]] QColor mutedForeground() const;
    [[nodiscard]] QColor accent() const;
    [[nodiscard]] QColor accentForeground() const;
    [[nodiscard]] QColor destructive() const;
    [[nodiscard]] QColor destructiveForeground() const;
    [[nodiscard]] QColor border() const;
    [[nodiscard]] QColor input() const;
    [[nodiscard]] QColor ring() const;

    [[nodiscard]] QColor chart1() const;
    [[nodiscard]] QColor chart2() const;
    [[nodiscard]] QColor chart3() const;
    [[nodiscard]] QColor chart4() const;
    [[nodiscard]] QColor chart5() const;

    [[nodiscard]] static int radiusSm() noexcept;
    [[nodiscard]] static int radiusMd() noexcept;
    [[nodiscard]] static int radiusLg() noexcept;
    [[nodiscard]] static int radiusXl() noexcept;
    [[nodiscard]] static int radius2xl() noexcept;
    [[nodiscard]] static int spacingXs() noexcept;
    [[nodiscard]] static int spacingSm() noexcept;
    [[nodiscard]] static int spacingMd() noexcept;
    [[nodiscard]] static int spacingLg() noexcept;
    [[nodiscard]] static int spacingXl() noexcept;
    [[nodiscard]] static int spacing2xl() noexcept;

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
