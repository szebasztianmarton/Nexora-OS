#pragma once

#include "EmulatorListModel.h"

#include "nexora/emulator/EmulatorManager.h"
#include "nexora/shared/ConfigLoader.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace nexora::input {
class InputController;
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
    Q_PROPERTY(QString emulatorLogPath READ emulatorLogPath NOTIFY lastOutputChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    struct Options
    {
        QString configDirectory;
        QString configDirectorySource; // human-readable origin of configDirectory
        QString logFilePath;
        bool runningAsRoot = false;
        QString emulatorLogDirectory; // per-emulator output logs; empty = disabled
    };

    explicit AppController(const Options &options, QObject *parent = nullptr);

    // Optional; the controller works without input devices (keyboard only).
    void setInputController(input::InputController *input);
    // While the game bar is open the controller drives the launcher even if an emulator runs.
    void setGameBarOpen(bool open);

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
    // Output log of the current / last launch, or the log directory before the first launch.
    QString emulatorLogPath() const;
    QString version() const;

    // Starts an emulator with its default profile (see setDefaultProfiles()).
    Q_INVOKABLE bool launch(const QString &emulatorId);
    // Starts an emulator with an explicit profile ("" = emulator defaults).
    Q_INVOKABLE bool launchProfile(const QString &emulatorId, const QString &profileId);
    // Starts |gamePath| with |emulatorId|; |title| is shown while it runs. An empty
    // |profileId| uses the emulator's default profile, unless |applyDefaultProfile| is false
    // (then the emulator's own arguments are used).
    bool launchGame(const QString &emulatorId, const QString &gamePath, const QString &title,
                    const QString &profileId = {}, bool applyDefaultProfile = true);

    // emulator id -> profile id used by launch(); unknown/disabled profiles are ignored.
    void setDefaultProfiles(const QVariantMap &defaults) { m_defaultProfiles = defaults; }
    QList<shared::EmulatorConfig> enabledEmulators() const { return m_enabled; }
    // Empty when |emulatorId| is configured and its executable passed validation.
    QString unavailableReason(const QString &emulatorId) const;
    Q_INVOKABLE bool stop();
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
    QTimer m_outputNotifyTimer;
    QString m_activeName;
    QString m_statusMessage;
    bool m_statusIsError = false;
    bool m_configDirectoryFound = false;
    bool m_gameBarOpen = false;
    QStringList m_configIssues;
    QStringList m_disabled;
    QList<shared::EmulatorConfig> m_enabled;
    QHash<QString, QString> m_unavailable;
    QVariantMap m_defaultProfiles;
    bool startLaunch(const emulator::LaunchRequest &request);
};

} // namespace nexora::ui
