#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace nexora::shared {
class Settings;
}
namespace nexora::input {
class InputController;
}
namespace nexora::system {
class SystemIntegration;
}

namespace nexora::ui {

// QML-facing preferences. Every change is validated by shared::Settings and saved
// immediately; side effects (volume, controller mapping) are applied here.
class SettingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)
    Q_PROPERTY(QStringList languages READ languages CONSTANT)
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY changed)
    Q_PROPERTY(QString resolution READ resolution WRITE setResolution NOTIFY changed)
    Q_PROPERTY(QStringList resolutions READ resolutions CONSTANT)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY changed)
    Q_PROPERTY(bool volumeSupported READ volumeSupported CONSTANT)
    Q_PROPERTY(bool showHints READ showHints WRITE setShowHints NOTIFY changed)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder NOTIFY changed)
    Q_PROPERTY(QString inputBackend READ inputBackend WRITE setInputBackend NOTIFY changed)
    Q_PROPERTY(QVariantList mappingRows READ mappingRows NOTIFY changed)
    Q_PROPERTY(QString capturingAction READ capturingAction NOTIFY capturingChanged)
    Q_PROPERTY(bool controllerAvailable READ controllerAvailable NOTIFY changed)
    Q_PROPERTY(QStringList issues READ issues NOTIFY changed)
    Q_PROPERTY(QString userFile READ userFile CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

public:
    SettingsController(shared::Settings *settings, input::InputController *input, system::SystemIntegration *system,
                       QObject *parent = nullptr);

    QString language() const;
    void setLanguage(const QString &value) { set(QStringLiteral("ui.language"), value); }
    QStringList languages() const { return {QStringLiteral("en")}; }
    QString displayMode() const;
    void setDisplayMode(const QString &value) { set(QStringLiteral("display.mode"), value); }
    QString resolution() const;
    void setResolution(const QString &value) { set(QStringLiteral("display.resolution"), value); }
    QStringList resolutions() const;
    int volume() const;
    void setVolume(int value) { set(QStringLiteral("audio.volume"), qBound(0, value, 100)); }
    bool volumeSupported() const;
    bool showHints() const;
    void setShowHints(bool value) { set(QStringLiteral("ui.showHints"), value); }
    QString sortOrder() const;
    void setSortOrder(const QString &value) { set(QStringLiteral("library.sortOrder"), value); }
    QString inputBackend() const;
    void setInputBackend(const QString &value) { set(QStringLiteral("input.backend"), value); }
    QVariantList mappingRows() const;
    QString capturingAction() const { return m_capturing; }
    // A gamepad backend is running (remap capture is only offered then).
    bool controllerAvailable() const;
    QStringList issues() const;
    QString userFile() const;
    QString lastError() const { return m_lastError; }

    // Cycles a list-valued option (display mode, resolution, sort, backend) by |step|.
    Q_INVOKABLE void cycle(const QString &key, int step);
    // Starts capturing the next gamepad button for |action| (5 s timeout).
    Q_INVOKABLE bool beginRemap(const QString &action);
    Q_INVOKABLE void cancelRemap();
    // Applies a binding directly (also used when no controller is present / in tests).
    Q_INVOKABLE bool remap(const QString &action, const QString &button);
    Q_INVOKABLE bool resetToDefaults();

    // Applies stored values to the input layer and the system (used at start-up).
    void applyAll();

signals:
    void changed();
    void capturingChanged();
    void displayModeChanged(const QString &mode);

private:
    bool set(const QString &key, const QVariant &value);
    void applyMapping();
    void onCaptured(const QString &button);

    shared::Settings *m_settings;
    input::InputController *m_input;
    system::SystemIntegration *m_system;
    QString m_capturing;
    QString m_lastError;
    QTimer m_captureTimeout;
};

} // namespace nexora::ui
