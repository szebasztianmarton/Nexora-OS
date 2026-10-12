#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
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
    Q_PROPERTY(int deadzone READ deadzone WRITE setDeadzone NOTIFY changed)
    Q_PROPERTY(QVariantList mappingRows READ mappingRows NOTIFY changed)
    Q_PROPERTY(QString capturingAction READ capturingAction NOTIFY capturingChanged)
    Q_PROPERTY(bool controllerAvailable READ controllerAvailable NOTIFY changed)
    Q_PROPERTY(QStringList issues READ issues NOTIFY changed)
    Q_PROPERTY(QString userFile READ userFile CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(int uiScale READ uiScale WRITE setUiScale NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY changed)
    Q_PROPERTY(QString startScreen READ startScreen WRITE setStartScreen NOTIFY changed)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY changed)
    Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel NOTIFY changed)
    // Default emulator id per game system id (only systems with an override are present);
    // a reactive mirror of setDefaultEmulatorForSystem()/defaultEmulatorForSystem() so QML
    // can bind to it directly instead of re-invoking the getter by hand.
    Q_PROPERTY(QVariantMap defaultEmulators READ defaultEmulators NOTIFY changed)

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
    // Analog stick dead zone in percent (5-60); applied to the running navigator at once.
    int deadzone() const;
    void setDeadzone(int percent) { set(QStringLiteral("input.deadzone"), qBound(5, percent, 60)); }
    QVariantList mappingRows() const;
    QString capturingAction() const { return m_capturing; }
    // A gamepad backend is running (remap capture is only offered then).
    bool controllerAvailable() const;
    QStringList issues() const;
    QString userFile() const;
    QString lastError() const { return m_lastError; }
    QString theme() const;
    void setTheme(const QString &value) { set(QStringLiteral("ui.theme"), value); }
    int uiScale() const;
    void setUiScale(int percent) { set(QStringLiteral("ui.scale"), qBound(75, percent, 150)); }
    bool reducedMotion() const;
    void setReducedMotion(bool value) { set(QStringLiteral("ui.reducedMotion"), value); }
    QString startScreen() const;
    void setStartScreen(const QString &value) { set(QStringLiteral("ui.startScreen"), value); }
    bool muted() const;
    void setMuted(bool value) { set(QStringLiteral("audio.muted"), value); }
    QString logLevel() const;
    void setLogLevel(const QString &value) { set(QStringLiteral("log.level"), value); }
    QVariantMap defaultEmulators() const;

    // Default emulator id for a game system id; "" clears the default / means none set.
    Q_INVOKABLE bool setDefaultEmulatorForSystem(const QString &systemId, const QString &emulatorId);
    Q_INVOKABLE QString defaultEmulatorForSystem(const QString &systemId) const;

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
    void applyDeadzone();
    void applyVolume();
    void applyLogLevel();
    void onCaptured(const QString &button);

    shared::Settings *m_settings;
    input::InputController *m_input;
    system::SystemIntegration *m_system;
    QString m_capturing;
    QString m_lastError;
    QTimer m_captureTimeout;
};

} // namespace nexora::ui
