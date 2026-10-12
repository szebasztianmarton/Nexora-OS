#pragma once

#include "EmulatorListModel.h"

#include "nexora/emulator/EmulatorManager.h"
#include "nexora/shared/ConfigLoader.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace nexora::input {
class InputController;
}
namespace nexora::emulator {
class LaunchHistory;
}

namespace nexora::ui {

// Backend object exposed to QML. Bridges configuration, the emulator manager and the
// input layer; holds no process logic itself.
class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(nexora::ui::EmulatorListModel *emulators READ emulators CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(QString activeEmulatorName READ activeEmulatorName NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusMessageChanged)
    Q_PROPERTY(QString lastOutput READ lastOutput NOTIFY lastOutputChanged)
    Q_PROPERTY(QString configDirectory READ configDirectory NOTIFY configChanged)
    Q_PROPERTY(QString configDirectorySource READ configDirectorySource NOTIFY configChanged)
    Q_PROPERTY(bool configDirectoryFound READ configDirectoryFound NOTIFY configChanged)
    Q_PROPERTY(QStringList configIssues READ configIssues NOTIFY configChanged)
    Q_PROPERTY(QStringList disabledEmulators READ disabledEmulators NOTIFY configChanged)
    Q_PROPERTY(bool runningAsRoot READ runningAsRoot CONSTANT)
    Q_PROPERTY(QString inputBackend READ inputBackend NOTIFY inputChanged)
    Q_PROPERTY(QString inputStatus READ inputStatus NOTIFY inputChanged)
    Q_PROPERTY(QStringList inputDevices READ inputDevices NOTIFY inputChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool canPause READ canPause NOTIFY stateChanged)
    Q_PROPERTY(QVariantList launchHistory READ launchHistory NOTIFY launchHistoryChanged)

public:
    struct Options
    {
        QString configDirectory;
        QString configDirectorySource; // human-readable origin of configDirectory
        QString logFilePath;
        bool runningAsRoot = false;
    };

    explicit AppController(const Options &options, QObject *parent = nullptr);

    // Optional; the controller works without input devices (keyboard only).
    void setInputController(input::InputController *input);
    // Optional; when set, every finished launch is recorded (see onProcessFinished()).
    void setLaunchHistory(emulator::LaunchHistory *history) { m_history = history; }

    emulator::EmulatorManager *manager() { return &m_manager; }
    EmulatorListModel *emulators() { return &m_model; }

    QString state() const;
    bool isBusy() const { return m_manager.isActive(); }
    QString activeEmulatorName() const { return m_activeName; }
    QString statusMessage() const { return m_statusMessage; }
    bool statusIsError() const { return m_statusIsError; }
    QString lastOutput() const;
    QString configDirectory() const { return m_options.configDirectory; }
    QString configDirectorySource() const { return m_options.configDirectorySource; }
    bool configDirectoryFound() const { return m_configDirectoryFound; }
    QStringList configIssues() const { return m_configIssues; }
    QStringList disabledEmulators() const { return m_disabled; }
    bool runningAsRoot() const { return m_options.runningAsRoot; }
    QString inputBackend() const;
    QString inputStatus() const;
    QStringList inputDevices() const;
    QString logFilePath() const { return m_options.logFilePath; }
    QString version() const;
    bool paused() const { return m_manager.isPaused(); }
    // The active process may be suspended (its config has pausable=true) and is Running or
    // already paused. Does not depend on the paused state itself: it stays true throughout
    // a pause so the UI can offer "Resume" instead of hiding the control.
    bool canPause() const;
    QVariantList launchHistory() const;

    // Starts an emulator with its default profile (see setDefaultProfiles()).
    Q_INVOKABLE bool launch(const QString &emulatorId);
    // Starts an emulator with an explicit profile ("" = emulator defaults).
    Q_INVOKABLE bool launchProfile(const QString &emulatorId, const QString &profileId);
    // Starts |gamePath| with |emulatorId|; |title| is shown while it runs.
    bool launchGame(const QString &emulatorId, const QString &gamePath, const QString &title,
                    const QString &profileId = {});

    // emulator id -> profile id used by launch(); unknown/disabled profiles are ignored.
    void setDefaultProfiles(const QVariantMap &defaults) { m_defaultProfiles = defaults; }
    QList<shared::EmulatorConfig> enabledEmulators() const { return m_enabled; }
    // Empty when |emulatorId| is configured and its executable passed validation.
    QString unavailableReason(const QString &emulatorId) const;
    Q_INVOKABLE bool stop();
    // Suspends / resumes the active process (SIGSTOP / SIGCONT); see canPause().
    Q_INVOKABLE bool pause();
    Q_INVOKABLE bool resume();
    // Controller "return to launcher" chord: stops the running emulator, if any.
    Q_INVOKABLE bool returnToLauncher();
    Q_INVOKABLE bool reloadConfig();
    Q_INVOKABLE QStringList recentLog() const;

signals:
    void stateChanged();
    void statusMessageChanged();
    void lastOutputChanged();
    void configChanged();
    void inputChanged();
    void pausedChanged();
    void launchHistoryChanged();
    // A launch request was accepted (process is starting).
    void launchStarted(const QString &emulatorId, const QString &gamePath);

private:
    void onManagerStateChanged(emulator::ProcessState state, const QString &emulatorId);
    void onProcessFinished(const QString &emulatorId, const emulator::ProcessOutcome &outcome);
    void setStatusMessage(const QString &message, bool isError);
    void updateInputSuspension();

    Options m_options;
    emulator::EmulatorManager m_manager;
    EmulatorListModel m_model;
    QPointer<input::InputController> m_input;
    emulator::LaunchHistory *m_history = nullptr;
    QTimer m_outputNotifyTimer;
    QString m_activeName;
    QString m_activeProfileId;
    QString m_statusMessage;
    bool m_statusIsError = false;
    bool m_configDirectoryFound = false;
    QStringList m_configIssues;
    QStringList m_disabled;
    QList<shared::EmulatorConfig> m_enabled;
    QHash<QString, QString> m_unavailable;
    QVariantMap m_defaultProfiles;
    bool m_activePausable = false;
    bool startLaunch(const emulator::LaunchRequest &request);
};

} // namespace nexora::ui
