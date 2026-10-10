#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace nexora::system {
class SystemIntegration;
}

namespace nexora::ui {

// QML-facing system status and (where the platform allows) power actions.
class SystemController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(bool powerSupported READ powerSupported NOTIFY statusChanged)
    Q_PROPERTY(QStringList statusLines READ statusLines NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)

public:
    explicit SystemController(system::SystemIntegration *system, QObject *parent = nullptr);

    QString platform() const;
    bool powerSupported() const;
    QStringList statusLines() const { return m_lines; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool powerOff();
    Q_INVOKABLE bool reboot();

signals:
    void statusChanged();

private:
    system::SystemIntegration *m_system;
    QStringList m_lines;
    QString m_lastError;
};

} // namespace nexora::ui
