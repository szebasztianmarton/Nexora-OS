#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace nexora::update {
class UpdateService;
}

namespace nexora::ui {

// QML-facing signed-update workflow: wraps nexora::update::UpdateService. On the desktop
// platform updates are refused outright (the console image is the only thing that is ever
// actually updated) unless NEXORA_UPDATE_TEST=1 is set, which lets the integration tests
// exercise the full check -> verify -> install flow without a real embedded image.
class UpdateController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY stateChanged)
    Q_PROPERTY(QString notes READ notes NOTIFY stateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString currentBuildId READ currentBuildId CONSTANT)
    Q_PROPERTY(bool supported READ supported CONSTANT)

public:
    // |isDesktopPlatform| decides supported() together with NEXORA_UPDATE_TEST.
    UpdateController(update::UpdateService *service, QString currentVersion, QString currentBuildId,
                     bool isDesktopPlatform, QObject *parent = nullptr);

    QString state() const;
    QString error() const;
    QString message() const;
    double progress() const;
    QString availableVersion() const;
    QString notes() const;
    QString currentVersion() const { return m_currentVersion; }
    QString currentBuildId() const { return m_currentBuildId; }
    bool supported() const { return m_supported; }

    // Idle/Available/Failed/Completed/RecoveryRequired -> Checking/Available/Failed.
    Q_INVOKABLE bool checkBundle(const QString &dir);
    // Available -> Verifying -> ReadyToInstall/Failed (hashes the image on a worker thread).
    Q_INVOKABLE bool verify();
    // ReadyToInstall -> Installing -> RestartRequired/Failed.
    Q_INVOKABLE bool install();
    Q_INVOKABLE void cancel();
    // Completed/RecoveryRequired -> Idle: the user dismissed the result of the last update
    // attempt; its record is removed so it is not shown again after a restart. Allowed on
    // every platform (it only touches the user's own state directory).
    Q_INVOKABLE bool acknowledgeLastResult();
    // Bundle directories found under removable media / the home directory; see .cpp for the
    // exact glob patterns. Never follows symlinks.
    Q_INVOKABLE QStringList candidateBundles() const;

signals:
    void stateChanged();
    void progressChanged();

private:
    update::UpdateService *m_service;
    QString m_currentVersion;
    QString m_currentBuildId;
    bool m_supported;
};

} // namespace nexora::ui
