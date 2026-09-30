#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QTranslator;

namespace aedst::designsystem {

/// Installs the module catalogs (`:/i18n/<catalog>_<lang>.qm`, produced by
/// `qt_add_translations` with its default `/i18n` resource prefix) for the OS UI
/// language, and lets the app switch language at runtime.
///
/// Also sets QLocale's default and QGuiApplication::layoutDirection so Arabic
/// mirrors the layout (QML: `LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft`).
/// After every switch `languageChanged` fires; App connects it to
/// `QQmlEngine::retranslate()` so `qsTr()` bindings re-evaluate.
class TranslationLoader final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    /// `catalogs`: base names of the .ts files (e.g. {"designsystem", "auth", "app"}).
    explicit TranslationLoader(QStringList catalogs, QObject* parent = nullptr);
    ~TranslationLoader() override;

    /// Resolves QLocale::system().uiLanguages() against the supported languages
    /// (fallback "en") and applies the result.
    void installSystemLanguage();

    /// Switches to `language` (any tag; the primary subtag is used). Unsupported
    /// languages fall back to English. Returns the language actually applied.
    Q_INVOKABLE QString setLanguage(const QString& language);

    [[nodiscard]] QString language() const;

signals:
    void languageChanged();

private:
    void removeTranslators();

    QStringList catalogs_;
    QString language_;
    QList<QTranslator*> translators_;
};

} // namespace aedst::designsystem
