#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

namespace nexora {

class EmulatorRegistry;
class EmulatorManager;
class ControllerManager;
class GameLibrary;

// Real system information for the Diagnostics / About screens. Nothing here
// is estimated or simulated; values that cannot be determined are "".
class Diagnostics : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString buildId READ buildId CONSTANT)
    Q_PROPERTY(QString osName READ osName CONSTANT)          // QSysInfo::prettyProductName
    Q_PROPERTY(QString kernel READ kernel CONSTANT)          // "linux 6.x"
    Q_PROPERTY(QString architecture READ architecture CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)    // runtime qVersion()
    Q_PROPERTY(QString imageVersion READ imageVersion CONSTANT) // VERSION_ID/BUILD_ID from /etc/nexora-release or /etc/os-release when ID=nexora, else ""
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
    Q_PROPERTY(QString configDirectory READ configDirectory CONSTANT)
    Q_PROPERTY(QStringList recentErrors READ recentErrors NOTIFY refreshed)

public:
    Diagnostics(const QString &configDirectory, EmulatorRegistry *registry, EmulatorManager *manager,
                ControllerManager *controllers, GameLibrary *library, QObject *parent = nullptr);

    QString appVersion() const;
    QString buildId() const;
    QString osName() const;
    QString kernel() const;
    QString architecture() const;
    QString qtVersion() const;
    QString imageVersion() const;
    QString logFilePath() const;
    QString configDirectory() const;
    QStringList recentErrors() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList logTail(int maxLines = 200) const;
    // Plain-text report. The user's home directory is replaced with "~" and no
    // environment variables are included.
    Q_INVOKABLE QString report() const;
    Q_INVOKABLE bool copyToClipboard(const QString &text) const;
    Q_INVOKABLE void clearRecentErrors();

    static QString sanitize(const QString &text); // home -> "~"

signals:
    void refreshed();

private:
    QString m_configDirectory;
    QPointer<EmulatorRegistry> m_registry;
    QPointer<EmulatorManager> m_manager;
    QPointer<ControllerManager> m_controllers;
    QPointer<GameLibrary> m_library;
};

} // namespace nexora
